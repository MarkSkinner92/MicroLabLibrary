#include "outbound_data_cache.h"
#include <cstring>
#include <Arduino.h>

void outbound_data_cache::init() {
    _count = 0;
}

bool outbound_data_cache::add_line(const char* suffix, uint32_t raw_millis,
                                    uint64_t abs_ms, uint64_t mission_ms,
                                    bool abs_synced, bool mission_synced) {
    if (_count >= OUTBOUND_CACHE_MAX_LINES) return false;
    outbound_entry& e = _entries[_count];
    strncpy(e.suffix, suffix, OUTBOUND_LINE_MAX_LEN - 1);
    e.suffix[OUTBOUND_LINE_MAX_LEN - 1] = '\0';
    e.raw_millis     = raw_millis;
    e.abs_ms         = abs_ms;
    e.mission_ms     = mission_ms;
    e.abs_synced     = abs_synced;
    e.mission_synced = mission_synced;
    _count++;
    return true;
}

const char* outbound_data_cache::get_line(uint8_t index) const {
    if (index >= _count) return nullptr;
    static char buf[OUTBOUND_LINE_MAX_LEN + 50];
    const outbound_entry& e = _entries[index];
    snprintf(buf, sizeof(buf), "%llu,%llu,%s",
             (unsigned long long)e.abs_ms,
             (unsigned long long)e.mission_ms,
             e.suffix);
    return buf;
}

void outbound_data_cache::patch_abs_times(uint64_t offset) {
    for (uint8_t i = 0; i < _count; i++) {
        if (!_entries[i].abs_synced) {
            _entries[i].abs_ms = (uint64_t)_entries[i].raw_millis + offset;
            _entries[i].abs_synced = true;
        }
    }
}

void outbound_data_cache::patch_mission_times(uint64_t offset) {
    for (uint8_t i = 0; i < _count; i++) {
        if (!_entries[i].mission_synced) {
            _entries[i].mission_ms = (uint64_t)_entries[i].raw_millis + offset;
            _entries[i].mission_synced = true;
        }
    }
}
