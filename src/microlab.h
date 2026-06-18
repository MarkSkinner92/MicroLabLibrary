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
    MicroLabClass() : serial(Serial1) {}

    HardwareSerial& serial;  // alias for Serial1 — use MicroLab.serial.print(...)

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

    bool turnOnCamera();
    bool turnOffCamera();
    bool takePicture(uint32_t timeout_ms = 5000);
    bool setCameraResolution(const char* res);
    bool setCameraLED(const char* mode);  // "on", "off", or "auto"
    bool cameraReady() const;

    control_data_cache  _cache;
    outbound_data_cache _outbound;
    bool _camera_initialized;
    bool _camera_cmd_acked;
    bool _camera_busy;

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

    uint8_t  _camera_resolution_idx       = 4;  // index of current resolution in valid[] table (default: 240x240)
    uint8_t  _last_picture_resolution_idx = 4;  // resolution at which the last picture was taken (default: 240x240)

    uint32_t _write_topic_hashes[WRITE_TOPIC_MAX];
    uint8_t  _write_topic_count  = 0;

    void _update_links();
    bool _register_write_topic(const char* topic);
};

extern MicroLabClass MicroLab;