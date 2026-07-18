#pragma once
#include <stdint.h>
#include <Arduino.h>

// Uncomment to mirror each flush to Serial1 for debugging
// #define MICROLAB_DEBUG_FLUSH

// Uncomment to hex-dump every raw byte received on Serial2 to Serial1
// #define MICROLAB_DEBUG_SERIAL2_RX

#include "command_reader.h"
#include "control_data_cache.h"
#include "outbound_data_cache.h"

#define CONTROL_LINK_MAX 20
#define WRITE_TOPIC_MAX  20

struct ControlLink {
    char topic[CACHE_CHANNEL_LEN];  // copied on registration — safe against dangling pointers
    enum Type : uint8_t { FLOAT, DOUBLE, INT } type;
    union {
        float*  f;
        double* d;
        int*    i;
    } ptr;
};

class MicroLabClass {
public:
    MicroLabClass() : serial(Serial1), Serial(Serial1) {}

    HardwareSerial& serial;  // alias for Serial1 — use MicroLab.serial.print(...)
    HardwareSerial& Serial;  // same as serial, capitalized to match Arduino's Serial.print(...) convention

    void begin();
    void beginDebugSerial(uint32_t baud = 115200);
    void doBackgroundTasks();      // call in loop()
    void update() { doBackgroundTasks(); }  // backward compat alias
    void flush();
    void delay(uint32_t ms);      // like ::delay() but pumps background tasks
    bool receiveData(const char* channel, double& out) const;

    uint64_t getAbsoluteTime();
    uint64_t getMissionTime();

    bool linkToTopic(const char* topic, float&  var);
    bool linkToTopic(const char* topic, double& var);
    bool linkToTopic(const char* topic, int&    var);

    int   read(const char* topic, int   defaultValue) const;
    float read(const char* topic, float defaultValue) const;
    bool  received(const char* topic);

    bool write(const char* topic, int data);
    bool write(const char* topic, float data);
    bool write(const char* topic, double data);
    bool write(const char* topic, const char* data);

    // Enables the control-data echo (like linkToTopic, but for the whole
    // cache) and immediately performs one echo pass on the "control_echo"
    // topic, republishing only the topics that arrived in the most recent
    // queue-data packet (not the whole cache — topics from earlier packets
    // that weren't refreshed this time are skipped). Packs as many "<name>:
    // <value>" lines as fit (joined by a literal "\n" escape — not a real
    // newline byte, which would corrupt the CSV row) into one data point;
    // once a point is full, starts a new one 2ms later so consecutive
    // points stay distinguishable. From then on, every incoming queue-data
    // packet automatically re-echoes just its own topics — no further
    // calls needed. Idempotent — calling again just performs another
    // immediate pass (over whichever topics were freshest as of the last
    // packet).
    // Returns the number of topics successfully packed and queued THIS
    // call; if it's less than the number of topics in that packet, the
    // outbound cache (OUTBOUND_CACHE_MAX_LINES, 20 lines) filled up before
    // the next flush() and the remainder were dropped.
    uint8_t enableControlEcho();

    // Does the packing/write loop only, no registration. Used by
    // enableControlEcho() and by process_command()'s auto-fire on every
    // incoming queue-data packet. Echoes only topics currently flagged
    // "received" (i.e. present in the packet just processed).
    uint8_t _echo_control_data();

    bool turnOnCamera();
    bool turnOffCamera();
    bool takePicture(uint32_t timeout_ms = 5000);
    bool takeBurst(int fps, int duration);
    bool setCameraResolution(const char* res);
    bool setCameraLED(const char* mode);  // "on", "off", or "auto"
    bool cameraReady() const;

    control_data_cache  _cache;
    outbound_data_cache _outbound;
    bool _camera_initialized;
    bool _camera_cmd_acked;
    bool _camera_busy;
    // Set by enableControlEcho(); read directly by process_command() (free
    // function in microlab.cpp), same convention as _cache/_camera_* above.
    bool _control_echo_enabled = false;

private:
    bool        _initialized = false;
    uint32_t    _mission_start_ms;
    uint32_t    _last_flush_ms;
    ControlLink _links[CONTROL_LINK_MAX];
    uint8_t     _link_count = 0;

    uint32_t _last_turn_on_ms    = 0;
    uint32_t _last_turn_off_ms   = 0;
    uint32_t _last_resolution_ms = 0;
    uint32_t _last_led_ms        = 0;
    uint32_t _last_picture_ms    = 0;
    uint32_t _burst_end_ms       = 0;  // non-zero while a burst is in flight; cleared by doBackgroundTasks()

    uint8_t  _camera_resolution_idx       = 4;  // index of current resolution in valid[] table (default: 240x240)
    uint8_t  _last_picture_resolution_idx = 4;  // resolution at which the last picture was taken (default: 240x240)

    uint32_t _write_topic_hashes[WRITE_TOPIC_MAX];
    uint8_t  _write_topic_count  = 0;

    bool _debug_enabled = false;  // set by beginDebugSerial(); gates _apiWarn() output

    void _update_links();
    bool _register_write_topic(const char* topic);
    // Shared implementation of write(topic, const char*), parameterized on
    // the raw boot-ms tick so callers (e.g. _echo_control_data) can space
    // out timestamps within a batch instead of sampling "now" every call.
    bool _write_char_at(const char* topic, const char* data, uint32_t t);

    // Prints "[API_WARN] " + the formatted message to `serial`, but only if
    // beginDebugSerial() has been called — surfaces the many silent-failure
    // points (invalid topic name, camera cooldown/not-on, etc.) without
    // adding overhead or requiring a wired-up debug UART on every user.
    void _apiWarn(const char* fmt, ...) const;
};

extern MicroLabClass MicroLab;