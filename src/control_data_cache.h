#pragma once
#include <stdint.h>

#define CACHE_MAX_ITEMS 50
#define CACHE_CHANNEL_LEN 21

typedef struct {
    char  channel[CACHE_CHANNEL_LEN];
    float value;
} control_data_cache_item;

class control_data_cache {
public:
    void init();
    void update_from_json(char* json); // NOTE: mutates json buffer (tiny-json)
    bool fetch_value(const char* channel, float& out) const;
    uint8_t count() const { return _count; }
    void clear() { _count = 0; }

private:
    control_data_cache_item _cache[CACHE_MAX_ITEMS];
    uint8_t                 _count;
};