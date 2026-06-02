#include "control_data_cache.h"
#include "tiny-json.h"
#include <cstring>

#define JSON_POOL_SIZE 256

static json_t _pool[JSON_POOL_SIZE];

void control_data_cache::init() {
    _count = 0;
}

void control_data_cache::update_from_json(char* json) {
    json_t const* root = json_create(json, _pool, JSON_POOL_SIZE);
    if (root == NULL) return;

    json_t const* outer = json_getChild(root);
    while (outer != NULL) {

        if (json_getType(outer) == JSON_ARRAY) {
            json_t const* name_field  = json_getChild(outer);
            json_t const* value_field = name_field ? json_getSibling(name_field) : NULL;

            if (name_field && value_field &&
                json_getType(name_field) == JSON_TEXT &&
                (json_getType(value_field) == JSON_REAL    ||
                 json_getType(value_field) == JSON_INTEGER ||
                 json_getType(value_field) == JSON_TEXT)) {

                const char* name = json_getValue(name_field);
                float val = (float)atof(json_getValue(value_field));

                // Look for existing entry to overwrite
                int8_t found = -1;
                for (uint8_t i = 0; i < _count; i++) {
                    if (strncmp(_cache[i].channel, name, CACHE_CHANNEL_LEN) == 0) {
                        found = i;
                        break;
                    }
                }

                if (found >= 0) {
                    // Overwrite existing
                    _cache[found].value = val;
                } else if (_count < CACHE_MAX_ITEMS) {
                    // Insert new
                    strncpy(_cache[_count].channel, name, CACHE_CHANNEL_LEN - 1);
                    _cache[_count].channel[CACHE_CHANNEL_LEN - 1] = '\0';
                    _cache[_count].value = val;
                    _count++;
                }
            }
        }

        outer = json_getSibling(outer);
    }
}

bool control_data_cache::fetch_value(const char* channel, float& out) const {
    for (uint8_t i = 0; i < _count; i++) {
        if (strncmp(_cache[i].channel, channel, CACHE_CHANNEL_LEN) == 0) {
            out = _cache[i].value;
            return true;
        }
    }
    return false;
}