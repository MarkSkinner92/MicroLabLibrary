#include "microlab.h"
#include "tiny-json.h"
#include "http_request_helper.h"
#include <Arduino.h>
#include "pico/time.h"

#define PIN_UART1_TX  20
#define PIN_UART1_RX  21

#define API_RESPONSE_COMMAND 0x10
#define QUEUE_DATA_COMMAND 0x30

MicroLabClass MicroLab;

// These offsets are computed when we receive timestamps from the coprocessor
// Use MicroLab::getMissionTime() and getAbsoluteTime(), not these offsets.
uint64_t absoluteTimeOffset = 0; // Equal to absoluteTime - to_ms_since_boot(get_absolute_time())
uint64_t missionTimeOffset = 0; // Equal to missionTime - to_ms_since_boot(get_absolute_time())

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

/*
Bytes to request mission time
11 2C 00 47 45 54 20 2F 61 70 69 2F 67 65 74 4D 69 73 73 69 6F 6E 54 69 6D 65 52 54 43 2E 6A 73 6F 6E 20 48 54 54 50 2F 31 2E 30 0D 0A 0D 0A

Bytes to turn on experiment
11 2C 00 47 45 54 20 2F 61 70 69 2F 70 6F 77 65 72 4F 6E 45 78 70 65 72 69 6D 65 6E 74 2E 6A 73 6F 6E 20 48 54 54 50 2F 31 2E 30 0D 0A 0D 0A

*/

static void read_serial2_byte(uint8_t b);

// Spin-reads UART until process_command sets _camera_cmd_acked or timeout expires.
// Used by blocking camera setup calls (initCamera, setCameraResolution).
static bool camera_spin_wait(uint32_t timeout_ms) {
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (!MicroLab._camera_cmd_acked) {
        while (Serial2.available()) read_serial2_byte((uint8_t)Serial2.read());
        if (to_ms_since_boot(get_absolute_time()) - t0 > timeout_ms) return false;
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
        } else if (strcmp(path, "/api/takePicture.json") == 0) {
            MicroLab._camera_busy = false;
        } else if (strncmp(path, "/api/setCameraSettings.json", 27) == 0) {
            MicroLab._camera_cmd_acked = true;
        }
    }
    else if (command == QUEUE_DATA_COMMAND){
        payload[length] = '\0';
        Serial1.print("[queue] ");
        Serial1.println(payload);
        MicroLab._cache.update_from_json(payload);
        MicroLab._control_data_arrived = true;
    }
}

void MicroLabClass::begin(uint32_t baud) {
    if (_initialized) return;
    _initialized = true;
    _mission_start_ms = to_ms_since_boot(get_absolute_time());
    _last_flush_ms    = 0;
    reset_state_machine();
    _control_data_arrived = false;
    _camera_initialized   = false;
    _camera_cmd_acked     = false;
    _camera_busy          = false;
    _cache.init();
    _outbound.init();
    Serial2.setTX(PIN_UART1_TX);
    Serial2.setRX(PIN_UART1_RX);
    Serial2.begin(baud);
    while (!Serial2);
    delay(100);

    // getDateRTC.json is sent after getMissionTimeRTC response arrives (see process_command)
    http_send_get(Serial2, "getDateRTC.json");
}

uint64_t getMissionTime(){
  return missionTimeOffset + (uint64_t)to_ms_since_boot(get_absolute_time());
}

uint64_t getAbsoluteTime(){
  return absoluteTimeOffset + (uint64_t)to_ms_since_boot(get_absolute_time());
}

void syncMissionTime(uint64_t time){
  missionTimeOffset = time - (uint64_t)to_ms_since_boot(get_absolute_time());
  MicroLab._outbound.patch_mission_times(missionTimeOffset);
  Serial1.print("syncMissionTime: ");
  Serial1.println((unsigned long long)time);
}

void syncAbsoluteTime(uint64_t time){
  absoluteTimeOffset = time - (uint64_t)to_ms_since_boot(get_absolute_time());
  MicroLab._outbound.patch_abs_times(absoluteTimeOffset);
  Serial1.print("syncAbsoluteTime: ");
  Serial1.println((unsigned long long)time);
}

static void writeCSVOverUART(HardwareSerial& serial, const uint8_t* csv, uint16_t csv_len) {
    static const char header[] = "POST /api/sendDataToMemory.csv HTTP/1.0\r\n\r\n";
    const uint16_t header_len  = sizeof(header) - 1;
    const uint16_t total       = header_len + csv_len;
    serial.write(0x11);
    serial.write((uint8_t)(total & 0xFF));
    serial.write((uint8_t)(total >> 8));
    serial.write((const uint8_t*)header, header_len);
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

void MicroLabClass::do_background_tasks() {
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
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (now - _last_flush_ms >= 1000) {
        _last_flush_ms = now;
        flush();
    }
}

bool MicroLabClass::receive_data(const char* channel, float& out) const {
    if (!_initialized) return false;
    return _cache.fetch_value(channel, out);
}

bool MicroLabClass::controlDataArrived() {
    if (!_initialized) return false;
    if (!_control_data_arrived) return false;
    _control_data_arrived = false;
    return true;
}

int MicroLabClass::read(const char* topic, int defaultValue) const {
    if (!_initialized) return defaultValue;
    float val;
    if (_cache.fetch_value(topic, val)) return (int)val;
    return defaultValue;
}

float MicroLabClass::read(const char* topic, float defaultValue) const {
    if (!_initialized) return defaultValue;
    float val;
    if (_cache.fetch_value(topic, val)) return val;
    return defaultValue;
}

bool MicroLabClass::received(const char* topic) const {
    if (!_initialized) return false;
    return _cache.was_received(topic);
}

static bool outbound_add(outbound_data_cache& cache, const char* suffix) {
    uint32_t t          = (uint32_t)to_ms_since_boot(get_absolute_time());
    bool abs_synced     = (absoluteTimeOffset != 0);
    bool mission_synced = (missionTimeOffset  != 0);
    uint64_t abs_ms     = abs_synced     ? absoluteTimeOffset + t : t;
    uint64_t mission_ms = mission_synced ? missionTimeOffset  + t : t;
    return cache.add_line(suffix, t, abs_ms, mission_ms, abs_synced, mission_synced);
}

bool MicroLabClass::send_data(const char* topic, int data) {
    if (!_initialized) return false;
    char suffix[OUTBOUND_LINE_MAX_LEN];
    snprintf(suffix, sizeof(suffix), "%s,%d", topic, data);
    return outbound_add(_outbound, suffix);
}

bool MicroLabClass::send_data(const char* topic, float data) {
    if (!_initialized) return false;
    char suffix[OUTBOUND_LINE_MAX_LEN];
    snprintf(suffix, sizeof(suffix), "%s,%.6g", topic, data);
    return outbound_add(_outbound, suffix);
}

bool MicroLabClass::send_data(const char* topic, double data) {
    if (!_initialized) return false;
    char suffix[OUTBOUND_LINE_MAX_LEN];
    snprintf(suffix, sizeof(suffix), "%s,%.10g", topic, data);
    return outbound_add(_outbound, suffix);
}

bool MicroLabClass::send_data(const char* topic, const char* data) {
    if (!_initialized) return false;
    char suffix[OUTBOUND_LINE_MAX_LEN];
    snprintf(suffix, sizeof(suffix), "%s,%s", topic, data);
    return outbound_add(_outbound, suffix);
}

bool MicroLabClass::write(const char* topic, int data)         { return send_data(topic, data); }
bool MicroLabClass::write(const char* topic, float data)       { return send_data(topic, data); }
bool MicroLabClass::write(const char* topic, double data)      { return send_data(topic, data); }
bool MicroLabClass::write(const char* topic, const char* data) { return send_data(topic, data); }

bool MicroLabClass::initCamera() {
    if (!_initialized || _camera_initialized) return _camera_initialized;
    _camera_cmd_acked = false;
    http_send_get(Serial2, "initCamera.json");
    if (!camera_spin_wait(5000)) return false;
    // The firmware's ssi_initCamera has its inter-step sleep_ms calls commented
    // out, so the ack arrives before voltage rails, power-on, and register init
    // have settled. Give the OV5640 time to stabilize before use.
    delay(300);
    return true;
}

bool MicroLabClass::setCameraResolution(const char* res) {
    if (!_initialized || !_camera_initialized) return false;
    static const char* const valid[] = {
        "96x96", "QQVGA", "QCIF",  "HQVGA",  "240x240",
        "QVGA",  "CIF",   "HVGA",  "VGA",     "SVGA",
        "XGA",   "HD",    "SXGA",  "UXGA",    "QHDA",   "WQXGA"
    };
    bool found = false;
    for (uint8_t i = 0; i < sizeof(valid) / sizeof(valid[0]); i++) {
        if (strcmp(res, valid[i]) == 0) { found = true; break; }
    }
    if (!found) return false;
    _camera_cmd_acked = false;
    char tag[HTTP_TAG_MAX_LEN];
    snprintf(tag, sizeof(tag), "setCameraSettings.json?resolution=%s", res);
    http_send_get(Serial2, tag);
    if (!camera_spin_wait(2000)) return false;
    // The OV5640 needs several frames for AEC/AWB to converge after a resolution
    // change. Taking a picture immediately produces a black or underexposed frame.
    delay(500);
    return true;
}

bool MicroLabClass::takePicture(uint32_t timeout_ms) {
    if (!_initialized || !_camera_initialized || _camera_busy) return false;
    _camera_busy = true;
    http_send_get(Serial2, "takePicture.json");
    if (timeout_ms == 0) return true;
    // Drain UART until the firmware response clears _camera_busy. Without this,
    // callers in setup() (no loop) never receive the response and the camera
    // appears permanently busy; callers in loop() must remember to poll.
    uint32_t t0 = to_ms_since_boot(get_absolute_time());
    while (_camera_busy) {
        while (Serial2.available()) read_serial2_byte((uint8_t)Serial2.read());
        if (to_ms_since_boot(get_absolute_time()) - t0 > timeout_ms) return false;
    }
    return true;
}

bool MicroLabClass::cameraReady() const {
    return _camera_initialized && !_camera_busy;
}