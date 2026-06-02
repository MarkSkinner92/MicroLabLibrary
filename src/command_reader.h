#pragma once
#include <stdint.h>

#define PAYLOAD_BUFFER_SIZE 500

void reset_state_machine();
bool is_packet_in_progress();
void process_incoming_byte(uint8_t b);
void process_command(uint8_t command, char* payload, uint16_t length);
