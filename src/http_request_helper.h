#pragma once
#include <Arduino.h>

#define HTTP_REQUEST_CMD     0x11
#define HTTP_TAG_MAX_LEN     64    // max characters allowed in a tag name

// Sends "GET /api/<tag> HTTP/1.0\r\n\r\n" as a framed 0x11 packet over port.
// Returns false if tag is too long to fit in the packet buffer.
bool http_send_get(Stream& port, const char* tag);
