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

#define CONTROL_LINK_MAX 8

struct ControlLink {
    const char* topic;
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

    void syncMissionTime(uint64_t time);
    void syncAbsoluteTime(uint64_t time);
    uint64_t getAbsoluteTime();
    uint64_t getMissionTime();

    bool linkToTopic(const char* topic, float&  var);
    bool linkToTopic(const char* topic, double& var);
    bool linkToTopic(const char* topic, int&    var);

    int   read(const char* topic, int   defaultValue) const;
    float read(const char* topic, float defaultValue) const;
    bool  received(const char* topic) const;

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

    void _update_links();
};

extern MicroLabClass MicroLab;