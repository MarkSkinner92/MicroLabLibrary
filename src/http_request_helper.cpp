#include "http_request_helper.h"
#include <Arduino.h>
#include <stdio.h>

// "GET /api/" = 9, " HTTP/1.0\r\n\r\n" = 14, tag up to HTTP_TAG_MAX_LEN, plus null
#define REQUEST_BUF_SIZE  (9 + HTTP_TAG_MAX_LEN + 14 + 1)

/*
Examples:
http_send_get(Serial2, "getMissionTimeRTC.json");
http_send_get(Serial2, "powerOnExperiment.json");
http_send_get(Serial2, "status.txt");
*/
bool http_send_get(Stream& port, const char* tag) {
    char payload[REQUEST_BUF_SIZE];
    int len = snprintf(payload, sizeof(payload), "GET /api/%s HTTP/1.0\r\n\r\n", tag);
    if (len <= 0 || len >= (int)sizeof(payload)) return false;

    uint16_t plen = (uint16_t)len;
    port.write((uint8_t)HTTP_REQUEST_CMD);
    port.write((uint8_t)(plen & 0xFF));
    port.write((uint8_t)(plen >> 8));
    port.write((const uint8_t*)payload, plen);
    return true;
}
