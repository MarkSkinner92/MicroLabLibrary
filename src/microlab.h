#pragma once
#include <stdint.h>

// Uncomment to mirror each flush to Serial1 for debugging
// #define MICROLAB_DEBUG_FLUSH

// Uncomment to hex-dump every raw byte received on Serial2 to Serial1
#define MICROLAB_DEBUG_SERIAL2_RX
#include "command_reader.h"
#include "control_data_cache.h"
#include "outbound_data_cache.h"

class MicroLabClass {
public:
    void begin(uint32_t baud = 500000);
    void do_background_tasks();   // call in loop()
    void update() { do_background_tasks(); }  // backward compat alias
    void flush();
    bool receive_data(const char* channel, float& out) const;

    void syncMissionTime(uint64_t time);
    void syncAbsoluteTime(uint64_t time);
    unsigned long getAbsoluteTime();
    unsigned long getMissionTime();

    bool controlDataArrived();

    int   read(const char* topic, int   defaultValue) const;
    float read(const char* topic, float defaultValue) const;

    bool write(const char* topic, int data);
    bool write(const char* topic, float data);
    bool write(const char* topic, double data);
    bool write(const char* topic, const char* data);

    bool send_data(const char* topic, int data);
    bool send_data(const char* topic, float data);
    bool send_data(const char* topic, double data);
    bool send_data(const char* topic, const char* data);

    void initCamera();
    void takePicture();
    bool setCameraResolution(const char* res);

    control_data_cache  _cache;
    outbound_data_cache _outbound;
    bool _control_data_arrived;

private:
    bool     _initialized = false;
    uint32_t _mission_start_ms;
    uint32_t _last_flush_ms;
};

extern MicroLabClass MicroLab;