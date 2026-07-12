#pragma once
#include <stdint.h>

// The coprocessor can push QUEUE_DATA (0x30) packets up to 16 KB
// (incoming_json[16384] in the firmware's telemetry.h). A full control-data
// update (50 topics, see CACHE_MAX_ITEMS) is ~2.5 KB, so 4 KB covers every
// realistic packet; anything larger is truncated safely by the reader.
#define PAYLOAD_BUFFER_SIZE 4096

void reset_state_machine();
bool is_packet_in_progress();
void process_incoming_byte(uint8_t b);
void process_command(uint8_t command, char* payload, uint16_t length);
