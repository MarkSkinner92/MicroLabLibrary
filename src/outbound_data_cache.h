#pragma once
#include <stdint.h>

#define OUTBOUND_CACHE_MAX_LINES 20
#define OUTBOUND_LINE_MAX_LEN    128

struct outbound_entry {
    char     suffix[OUTBOUND_LINE_MAX_LEN]; // "topic,value"
    uint32_t raw_millis;
    uint64_t abs_ms;
    uint64_t mission_ms;
    bool     abs_synced;
    bool     mission_synced;
};

class outbound_data_cache {
public:
    void        init();
    bool        add_line(const char* suffix, uint32_t raw_millis,
                         uint64_t abs_ms, uint64_t mission_ms,
                         bool abs_synced, bool mission_synced);
    const char* get_line(uint8_t index) const;
    uint8_t     count() const { return _count; }
    void        clear()       { _count = 0; }
    void        patch_abs_times(uint64_t offset);
    void        patch_mission_times(uint64_t offset);

private:
    outbound_entry _entries[OUTBOUND_CACHE_MAX_LINES];
    uint8_t        _count;
};
