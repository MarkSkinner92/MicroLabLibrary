#include "MicroLab.h"
#include "tiny-json.h"
#include "http_request_helper.h"
#include <Arduino.h>
#include "pico/time.h"

#define PIN_UART1_TX  20
#define PIN_UART1_RX  21

#define API_RESPONSE_COMMAND  0x10
#define QUEUE_DATA_COMMAND    0x30
#define MISSION_START_COMMAND 0x60
#define MISSION_STOP_COMMAND  0x61
#define MISSION_RESET_COMMAND 0x62

#define HTTP_COOLDOWN_MS        1000
#define WRITE_TOPIC_NAME_MAX_LEN  64

// Minimum time between pictures, indexed by resolution (matches valid[] in setCameraResolution).
// Cooldown is applied based on the resolution of the LAST picture taken, not the current one.
// Tweak individual entries freely — they are independent.
static const uint32_t PICTURE_COOLDOWN_MS[16] = {
     3000,  //  0  96x96
     5000,  //  1  QQVGA
     7000,  //  2  QCIF
    10000,  //  3  HQVGA
    12000,  //  4  240x240
    15000,  //  5  QVGA
    17000,  //  6  CIF
    20000,  //  7  HVGA
    25000,  //  8  VGA
    30000,  //  9  SVGA
    35000,  // 10  XGA
    40000,  // 11  HD
    45000,  // 12  SXGA
    50000,  // 13  UXGA
    55000,  // 14  QHDA
    60000,  // 15  WQXGA
};

// Returns false if s is NULL or contains characters that would corrupt a CSV row.
static bool is_safe_string(const char* s) {
    if (!s) return false;
    for (const char* p = s; *p; p++) {
        if (*p == ',' || *p == '\n' || *p == '\r') return false;
    }
    return true;
}

MicroLabClass MicroLab;

// These offsets are computed when we receive timestamps from the coprocessor
// Use MicroLab::getMissionTime() and getAbsoluteTime(), not these offsets.
uint64_t absoluteTimeOffset = 0;  // Equal to absoluteTime - to_ms_since_boot(get_absolute_time())
uint64_t missionTimeOffset  = 0;  // Equal to missionTime  - to_ms_since_boot(get_absolute_time())
static bool     missionRunning   = false;
static uint64_t missionFrozenMs  = 0;

// Absolute time sync state. The getDateRTC request/response can be lost (UART
// RX overflow, framing desync, coprocessor busy), so doBackgroundTasks keeps
// re-requesting until a response actually arrives.
#define ABS_TIME_RETRY_MS 2000
static bool     absTimeSynced        = false;
static bool     missionTimeSynced    = false;
static uint32_t lastAbsTimeRequestMs = 0;

void syncMissionTime(uint64_t time);
void syncAbsoluteTime(uint64_t time);

// Howard Hinnant's algorithm: days since Unix epoch → civil date inverse
// month is 1-indexed (January = 1)
static uint64_t date_to_unix_ms(int year, int month, int day, int hour, int min, int sec) {
    if (month <= 2) { year--; month += 9; } else { month -= 3; }
    int era = year / 400;
    int yoe = year - era * 400;
    int doy = (153 * month + 2) / 5 + day - 1;
    int doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    int32_t days = (int32_t)era * 146097 + doe - 719468;
    return ((int64_t)days * 86400LL + hour * 3600 + min * 60 + sec) * 1000ULL;
}

static void read_serial2_byte(uint8_t b);

// Drain the UART RX buffer completely before issuing a new camera command.
// If a packet is partially received, spin up to 20 ms to let it finish so the
// state machine is not left stranded. Clears any stale ACK that could cause
// camera_spin_wait to return true instantly for the wrong command.
static void drain_uart_rx() {
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    do {
        while (Serial2.available()) read_serial2_byte((uint8_t)Serial2.read());
    } while (is_packet_in_progress() &&
             to_ms_since_boot(get_absolute_time()) - t0 < 20);
    if (is_packet_in_progress()) reset_state_machine();
}

// Spin-reads UART until process_command sets _camera_cmd_acked or timeout expires.
// Resets the packet state machine on timeout so a stranded partial packet does
// not corrupt subsequent UART communication.
static bool camera_spin_wait(uint32_t timeout_ms) {
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (!MicroLab._camera_cmd_acked) {
        while (Serial2.available()) read_serial2_byte((uint8_t)Serial2.read());
        if (to_ms_since_boot(get_absolute_time()) - t0 > timeout_ms) {
            reset_state_machine();
            return false;
        }
    }
    return true;
}

// process_command is called by command_reader when a full packet arrives
void process_command(uint8_t command, char* payload, uint16_t length) {
    if(command == API_RESPONSE_COMMAND){
        // Serial1.write((const uint8_t*)payload, length);
        /*
        payloads look like this <path>\0<http-payload>
        for example, JSON payloads look like this /api/whatever.json\0{the:"json-data"}
        */

        // The path is already null-terminated by the \0 separator in the payload format,
        // so strcmp stops at the right place without any extra work.
        const char* path = payload;
        if (strcmp(path, "/api/getMissionTimeRTC.json") == 0) {
          char* json_str = (char*)(path + strlen(path) + 1);
          json_t mem[4];
          json_t const* root = json_create(json_str, mem, 4);
          if (root) {
              json_t const* ts = json_getProperty(root, "timestamp");
              if (ts) syncMissionTime((uint64_t)json_getInteger(ts)*1000);
          }
        }
        if (strcmp(path, "/api/getDateRTC.json") == 0) {
          http_send_get(Serial2, "getMissionTimeRTC.json");
          char* json_str = (char*)(path + strlen(path) + 1);
          json_t mem[8];
          json_t const* root = json_create(json_str, mem, 8);
          if (root) {
              json_t const* year_p  = json_getProperty(root, "year");
              json_t const* month_p = json_getProperty(root, "month");
              json_t const* day_p   = json_getProperty(root, "day");
              json_t const* hour_p  = json_getProperty(root, "hour");
              json_t const* min_p   = json_getProperty(root, "min");
              json_t const* sec_p   = json_getProperty(root, "sec");
              if (year_p && month_p && day_p && hour_p && min_p && sec_p) {
                  uint64_t time_ms = date_to_unix_ms(
                      (int)json_getInteger(year_p),
                      (int)json_getInteger(month_p),
                      (int)json_getInteger(day_p),
                      (int)json_getInteger(hour_p),
                      (int)json_getInteger(min_p),
                      (int)json_getInteger(sec_p)
                  );
                  syncAbsoluteTime(time_ms);
              }
          }
        }
        if (strcmp(path, "/api/initCamera.json") == 0) {
            MicroLab._camera_initialized = true;
            MicroLab._camera_cmd_acked   = true;
        } else if (strcmp(path, "/api/powerDownCamera.json") == 0) {
            MicroLab._camera_initialized = false;
            MicroLab._camera_cmd_acked   = true;
        } else if (strcmp(path, "/api/takePicture.json") == 0) {
            MicroLab._camera_busy = false;
        } else if (strncmp(path, "/api/setCameraSettings.json", 27) == 0) {
            MicroLab._camera_cmd_acked = true;
        } else if (strcmp(path, "/api/cameraFlash.json") == 0) {
            MicroLab._camera_cmd_acked = true;
        } else if (strcmp(path, "/api/takeBurst.json") == 0) {
            MicroLab._camera_cmd_acked = true;
        }
    }
    else if (command == QUEUE_DATA_COMMAND){
        payload[length] = '\0';
        MicroLab._cache.update_from_json(payload);
    }
    else if (command == MISSION_START_COMMAND ||
             command == MISSION_STOP_COMMAND  ||
             command == MISSION_RESET_COMMAND) {
        if (length < 4) return;
        uint32_t ts_sec = (uint32_t)(uint8_t)payload[0]
                        | ((uint32_t)(uint8_t)payload[1] << 8)
                        | ((uint32_t)(uint8_t)payload[2] << 16)
                        | ((uint32_t)(uint8_t)payload[3] << 24);
        uint64_t ts_ms = (uint64_t)ts_sec * 1000ULL;

        if (command == MISSION_START_COMMAND) {
            syncMissionTime(ts_ms);
            missionRunning = true;
        } else if (command == MISSION_STOP_COMMAND) {
            missionFrozenMs = ts_ms;
            missionRunning  = false;
        } else { // MISSION_RESET_COMMAND
            missionFrozenMs = 0;
            missionRunning  = false;
        }
    }
}

void MicroLabClass::beginDebugSerial(uint32_t baud) {
    Serial1.setTX(0);
    Serial1.setRX(1);
    Serial1.begin(baud);
}

void MicroLabClass::begin() {
    if (_initialized) return;
    _initialized = true;
    _mission_start_ms = to_ms_since_boot(get_absolute_time());
    _last_flush_ms    = 0;
    reset_state_machine();
    _link_count           = 0;
    _camera_initialized  = false;
    _camera_cmd_acked    = false;
    _camera_busy         = false;
    _last_turn_on_ms              = 0;
    _last_turn_off_ms             = 0;
    _last_resolution_ms           = 0;
    _last_led_ms                  = 0;
    _last_picture_ms              = 0;
    _camera_resolution_idx        = 4;  // 240x240 is the coprocessor default
    _last_picture_resolution_idx  = 4;
    _write_topic_count            = 0;
    _cache.init();
    _outbound.init();
    Serial2.setTX(PIN_UART1_TX);
    Serial2.setRX(PIN_UART1_RX);
    // The arduino-pico default RX buffer is only 32 bytes; at 500 kbaud a
    // single queue-data push from the coprocessor overflows it during any
    // blocking section of user code, dropping bytes mid-packet and desyncing
    // the framing state machine. 1 KB rides out ~16 ms of unread traffic.
    Serial2.setFIFOSize(1024);
    Serial2.begin(500000);
    while (!Serial2);

    delay(1000); // Allows time for the flight computer to tell the coprocessor to update the mission time before we get it.
    lastAbsTimeRequestMs = to_ms_since_boot(get_absolute_time());
    http_send_get(Serial2, "getDateRTC.json");
    // this->delay(100);
}

uint64_t MicroLabClass::getMissionTime(){
  if (!missionRunning) return missionFrozenMs;
  return missionTimeOffset + (uint64_t)to_ms_since_boot(get_absolute_time());
}

uint64_t MicroLabClass::getAbsoluteTime(){
  return absoluteTimeOffset + (uint64_t)to_ms_since_boot(get_absolute_time());
}

void syncMissionTime(uint64_t time){
  missionTimeOffset = time - (uint64_t)to_ms_since_boot(get_absolute_time());
  MicroLab._outbound.patch_mission_times(missionTimeOffset);
  missionRunning     = true;
  missionTimeSynced  = true;
}

void syncAbsoluteTime(uint64_t time){
  absoluteTimeOffset = time - (uint64_t)to_ms_since_boot(get_absolute_time());
  MicroLab._outbound.patch_abs_times(absoluteTimeOffset);
  absTimeSynced = true;
}

static void writeCSVOverUART(HardwareSerial& serial, const uint8_t* csv, uint16_t csv_len) {
    // Content-Length tells the coprocessor's HTTP handler exactly how many body
    // bytes to read, so it stops at the end of the CSV and does not consume the
    // framing bytes of the next HTTP request (which caused malformed SD card rows).
    char header[80];
    int hlen = snprintf(header, sizeof(header),
                        "POST /api/sendDataToMemory.csv HTTP/1.0\r\n"
                        "Content-Length: %u\r\n"
                        "\r\n",
                        (unsigned)csv_len);
    if (hlen <= 0 || hlen >= (int)sizeof(header)) return;
    const uint16_t total = (uint16_t)hlen + csv_len;
    serial.write(0x11);
    serial.write((uint8_t)(total & 0xFF));
    serial.write((uint8_t)(total >> 8));
    serial.write((const uint8_t*)header, (size_t)hlen);
    serial.write(csv, csv_len);
}

void MicroLabClass::flush() {
    if (!_initialized) return;
    if (_outbound.count() == 0) return;

    // Build the full CSV payload — each get_line() call reuses the same static
    // buffer, so copy each line out before the next call overwrites it.
    static char buf[OUTBOUND_CACHE_MAX_LINES * (OUTBOUND_LINE_MAX_LEN + 52)];
    uint16_t pos = 0;
    for (uint8_t i = 0; i < _outbound.count(); i++) {
        const char* line = _outbound.get_line(i);
        uint16_t len = (uint16_t)strlen(line);
        if (pos + len + 1 > sizeof(buf)) break;
        memcpy(buf + pos, line, len);
        pos += len;
        buf[pos++] = '\n';
    }

    writeCSVOverUART(Serial2, (const uint8_t*)buf, pos);

#ifdef MICROLAB_DEBUG_FLUSH
    Serial1.println("####");
    buf[pos] = '\0';
    Serial1.print(buf);
    Serial1.println("####");
#endif

    _outbound.clear();
}

static void read_serial2_byte(uint8_t b) {
#ifdef MICROLAB_DEBUG_SERIAL2_RX
    Serial1.print(" 0x");
    if (b < 0x10) Serial1.print('0');
    Serial1.print(b, HEX);
#endif
    process_incoming_byte(b);
}

void MicroLabClass::doBackgroundTasks() {
    if (!_initialized) return;
#ifdef MICROLAB_DEBUG_SERIAL2_RX
    bool got_bytes = Serial2.available();
    if (got_bytes) Serial1.print("[rx]");
#endif
    while (Serial2.available()) {
        read_serial2_byte((uint8_t)Serial2.read());
    }

    // If we stopped mid-packet, spin until it completes or times out (20 ms >>
    // the ~1.6 ms needed for 80 bytes at 500 kbaud).  This prevents the next
    // loop() iteration from consuming a queue packet as part of this response.
    if (is_packet_in_progress()) {
        uint32_t t0 = to_ms_since_boot(get_absolute_time());
        while (is_packet_in_progress()) {
            while (Serial2.available()) {
                read_serial2_byte((uint8_t)Serial2.read());
            }
            if (to_ms_since_boot(get_absolute_time()) - t0 > 20) {
#ifdef MICROLAB_DEBUG_SERIAL2_RX
                Serial1.println(" [timeout reset]");
#endif
                reset_state_machine();
                break;
            }
        }
    }

#ifdef MICROLAB_DEBUG_SERIAL2_RX
    if (got_bytes) Serial1.println();
#endif
    _update_links();

    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (_burst_end_ms != 0 && now >= _burst_end_ms) {
        _burst_end_ms = 0;
        _camera_busy  = false;
    }
    if (now - _last_flush_ms >= 1000) {
        _last_flush_ms = now;
        flush();
    }

    // Keep requesting the RTC date until a response actually makes it through.
    // The single request in begin() can be lost to UART overflow or framing
    // desync, which previously left absolute timestamps at ms-since-boot
    // (epoch 1970) for the whole session. A successful sync also triggers a
    // getMissionTimeRTC request via the response handler.
    if (!absTimeSynced && now - lastAbsTimeRequestMs >= ABS_TIME_RETRY_MS) {
        lastAbsTimeRequestMs = now;
        http_send_get(Serial2, "getDateRTC.json");
    }
}

void MicroLabClass::delay(uint32_t ms) {
    uint32_t start = to_ms_since_boot(get_absolute_time());
    while (to_ms_since_boot(get_absolute_time()) - start < ms) {
        doBackgroundTasks();
    }
}

bool MicroLabClass::receiveData(const char* channel, double& out) const {
    if (!_initialized || !channel) return false;
    return _cache.fetch_value(channel, out);
}

void MicroLabClass::_update_links() {
    for (uint8_t i = 0; i < _link_count; i++) {
        double val;
        if (!_cache.fetch_value(_links[i].topic, val)) continue;
        switch (_links[i].type) {
            case ControlLink::FLOAT:  *_links[i].ptr.f = val;          break;
            case ControlLink::DOUBLE: *_links[i].ptr.d = (double)val;  break;
            case ControlLink::INT:    *_links[i].ptr.i = (int)val;     break;
        }
    }
}

bool MicroLabClass::linkToTopic(const char* topic, float& var) {
    if (!topic) return false;
    if (_link_count >= CONTROL_LINK_MAX) return false;
    strncpy(_links[_link_count].topic, topic, CACHE_CHANNEL_LEN - 1);
    _links[_link_count].topic[CACHE_CHANNEL_LEN - 1] = '\0';
    _links[_link_count].type  = ControlLink::FLOAT;
    _links[_link_count].ptr.f = &var;
    _link_count++;
    return true;
}

bool MicroLabClass::linkToTopic(const char* topic, double& var) {
    if (!topic) return false;
    if (_link_count >= CONTROL_LINK_MAX) return false;
    strncpy(_links[_link_count].topic, topic, CACHE_CHANNEL_LEN - 1);
    _links[_link_count].topic[CACHE_CHANNEL_LEN - 1] = '\0';
    _links[_link_count].type  = ControlLink::DOUBLE;
    _links[_link_count].ptr.d = &var;
    _link_count++;
    return true;
}

bool MicroLabClass::linkToTopic(const char* topic, int& var) {
    if (!topic) return false;
    if (_link_count >= CONTROL_LINK_MAX) return false;
    strncpy(_links[_link_count].topic, topic, CACHE_CHANNEL_LEN - 1);
    _links[_link_count].topic[CACHE_CHANNEL_LEN - 1] = '\0';
    _links[_link_count].type  = ControlLink::INT;
    _links[_link_count].ptr.i = &var;
    _link_count++;
    return true;
}

int MicroLabClass::read(const char* topic, int defaultValue) const {
    if (!_initialized || !topic) return defaultValue;
    double val;
    if (_cache.fetch_value(topic, val)) return (int)val;
    return defaultValue;
}

float MicroLabClass::read(const char* topic, float defaultValue) const {
    if (!_initialized || !topic) return defaultValue;
    double val;
    if (_cache.fetch_value(topic, val)) return (float)val;
    return defaultValue;
}

bool MicroLabClass::received(const char* topic) {
    if (!_initialized || !topic) return false;
    return _cache.was_received(topic);
}

bool MicroLabClass::_register_write_topic(const char* topic) {
    if (!is_safe_string(topic)) return false;
    if (strcmp(topic, "camera") == 0) return false;
    if (strlen(topic) > WRITE_TOPIC_NAME_MAX_LEN) return false;
    uint32_t h = 2166136261u;
    for (const char* p = topic; *p; p++) { h ^= (uint8_t)*p; h *= 16777619u; }
    for (uint8_t i = 0; i < _write_topic_count; i++) {
        if (_write_topic_hashes[i] == h) return true;
    }
    if (_write_topic_count >= WRITE_TOPIC_MAX) return false;
    _write_topic_hashes[_write_topic_count++] = h;
    return true;
}

static bool outbound_add(outbound_data_cache& cache, const char* suffix) {
    uint32_t t          = (uint32_t)to_ms_since_boot(get_absolute_time());
    uint64_t abs_ms     = absTimeSynced     ? absoluteTimeOffset + t : t;
    uint64_t mission_ms = missionTimeSynced ? missionTimeOffset  + t : t;
    return cache.add_line(suffix, t, abs_ms, mission_ms, absTimeSynced, missionTimeSynced);
}

bool MicroLabClass::write(const char* topic, int data) {
    if (!_initialized) return false;
    if (!_register_write_topic(topic)) return false;
    char suffix[OUTBOUND_LINE_MAX_LEN];
    snprintf(suffix, sizeof(suffix), "%s,%d", topic, data);
    return outbound_add(_outbound, suffix);
}

bool MicroLabClass::write(const char* topic, float data) {
    if (!_initialized) return false;
    if (!_register_write_topic(topic)) return false;
    char suffix[OUTBOUND_LINE_MAX_LEN];
    snprintf(suffix, sizeof(suffix), "%s,%.6g", topic, data);
    return outbound_add(_outbound, suffix);
}

bool MicroLabClass::write(const char* topic, double data) {
    if (!_initialized) return false;
    if (!_register_write_topic(topic)) return false;
    char suffix[OUTBOUND_LINE_MAX_LEN];
    snprintf(suffix, sizeof(suffix), "%s,%.10g", topic, data);
    return outbound_add(_outbound, suffix);
}

bool MicroLabClass::write(const char* topic, const char* data) {
    if (!_initialized) return false;
    if (!is_safe_string(data)) return false;
    if (!_register_write_topic(topic)) return false;
    if (strlen(topic) + strlen(data) + 1 >= OUTBOUND_LINE_MAX_LEN) return false;
    char suffix[OUTBOUND_LINE_MAX_LEN];
    snprintf(suffix, sizeof(suffix), "%s,%s", topic, data);
    return outbound_add(_outbound, suffix);
}

bool MicroLabClass::turnOnCamera() {
    if (!_initialized || _camera_initialized) return _camera_initialized;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (_last_turn_on_ms != 0 && now - _last_turn_on_ms < HTTP_COOLDOWN_MS) return false;
    _last_turn_on_ms = now;
    drain_uart_rx();
    _camera_cmd_acked = false;
    http_send_get(Serial2, "initCamera.json");
    return camera_spin_wait(5000);
}

bool MicroLabClass::turnOffCamera() {
    if (!_initialized || !_camera_initialized || _camera_busy) return false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (_last_turn_off_ms != 0 && now - _last_turn_off_ms < HTTP_COOLDOWN_MS) return false;
    _last_turn_off_ms = now;
    drain_uart_rx();
    _camera_cmd_acked = false;
    http_send_get(Serial2, "powerDownCamera.json");
    return camera_spin_wait(2000);
}

bool MicroLabClass::setCameraResolution(const char* res) {
    if (!_initialized || !_camera_initialized || !res || _camera_busy) return false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (_last_resolution_ms != 0 && now - _last_resolution_ms < HTTP_COOLDOWN_MS) return false;
    static const char* const valid[] = {
        "96x96", "QQVGA", "QCIF",  "HQVGA",  "240x240",
        "QVGA",  "CIF",   "HVGA",  "VGA",     "SVGA",
        "XGA",   "HD",    "SXGA",  "UXGA",    "QHDA",   "WQXGA"
    };
    uint8_t found_idx = 0;
    bool found = false;
    for (uint8_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        if (strcmp(res, valid[i]) == 0) { found = true; found_idx = i; break; }
    }
    if (!found) return false;
    _last_resolution_ms = now;
    _camera_resolution_idx = found_idx;
    drain_uart_rx();
    _camera_cmd_acked = false;
    char tag[HTTP_TAG_MAX_LEN];
    snprintf(tag, sizeof(tag), "setCameraSettings.json?resolution=%s", res);
    http_send_get(Serial2, tag);
    return camera_spin_wait(2000);
}

bool MicroLabClass::setCameraLED(const char* mode) {
    if (!_initialized || !_camera_initialized || !mode || _camera_busy) return false;
    if (strcmp(mode, "on") != 0 && strcmp(mode, "off") != 0 && strcmp(mode, "auto") != 0) return false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (_last_led_ms != 0 && now - _last_led_ms < HTTP_COOLDOWN_MS) return false;
    _last_led_ms = now;
    drain_uart_rx();
    _camera_cmd_acked = false;
    char tag[HTTP_TAG_MAX_LEN];
    snprintf(tag, sizeof(tag), "cameraFlash.json?mode=%s", mode);
    http_send_get(Serial2, tag);
    return camera_spin_wait(2000);
}

bool MicroLabClass::takePicture(uint32_t timeout_ms) {
    if (!_initialized || !_camera_initialized || _camera_busy) return false;
    uint32_t now = to_ms_since_boot(get_absolute_time());
    uint32_t cooldown = PICTURE_COOLDOWN_MS[_last_picture_resolution_idx];
    if (_last_picture_ms != 0 && now - _last_picture_ms < cooldown) return false;
    _last_picture_ms = now;
    _last_picture_resolution_idx = _camera_resolution_idx;
    drain_uart_rx();
    _camera_busy = true;
    this->serial.print("Taking picture: ");
    this->serial.println(now);
    http_send_get(Serial2, "takePicture.json");
    if (timeout_ms == 0) return true;
    // Drain UART until the firmware response clears _camera_busy. Without this,
    // callers in setup() (no loop) never receive the response and the camera
    // appears permanently busy; callers in loop() must remember to poll.
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (_camera_busy) {
        while (Serial2.available()) read_serial2_byte((uint8_t)Serial2.read());
        if (to_ms_since_boot(get_absolute_time()) - t0 > timeout_ms) {
            reset_state_machine();
            _camera_busy = false;
            return false;
        }
    }
    return true;
}

bool MicroLabClass::takeBurst(int fps, int duration) {
    if (!_initialized || !_camera_initialized || _camera_busy) return false;

    // Cap duration to [1, 10]
    if (duration < 1) duration = 1;
    if (duration > 10) duration = 10;

    // Cap fps to [1, max] based on current resolution (mirrors firmware burst_max_fps_by_res table)
    static const uint8_t BURST_MAX_FPS[16] = {
        30, 30, 30, 20, 20, 15, 15, 10, 8, 5, 3, 3, 2, 2, 1, 1
    };
    int max_fps = BURST_MAX_FPS[_camera_resolution_idx];
    if (fps < 1) fps = 1;
    if (fps > max_fps) fps = max_fps;

    _camera_busy = true;
    drain_uart_rx();
    _camera_cmd_acked = false;
    char tag[HTTP_TAG_MAX_LEN];
    snprintf(tag, sizeof(tag), "takeBurst.json?fps=%d&duration=%d", fps, duration);
    http_send_get(Serial2, tag);

    if (!camera_spin_wait(5000)) {
        _camera_busy = false;
        return false;
    }

    uint32_t now = to_ms_since_boot(get_absolute_time());
    _burst_end_ms = now + (uint32_t)(duration + 1) * 1000;
    return true;
}

bool MicroLabClass::cameraReady() const {
    return _camera_initialized && !_camera_busy;
}