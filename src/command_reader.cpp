#include "command_reader.h"
#include "microlab.h"
#include <Arduino.h>

static char payload[PAYLOAD_BUFFER_SIZE];
static uint8_t active_command;
static uint16_t payload_length;
static uint8_t length_bytes_received;
static uint16_t payload_bytes_received;

void reset_state_machine() {
    active_command = 0;
    payload_length = 0;
    length_bytes_received = 0;
    payload_bytes_received = 0;
}

bool is_packet_in_progress() {
    return active_command != 0;
}

void process_incoming_byte(uint8_t b) {
    if (active_command == 0) {
        active_command = b;
        payload_length = 0;
        length_bytes_received = 0;
        payload_bytes_received = 0;
#ifdef MICROLAB_DEBUG_SERIAL2_RX
        Serial1.print("[sm] start cmd=0x");
        Serial1.println(active_command, HEX);
#endif
    } else {
        if (length_bytes_received == 0) {
            payload_length = b;
            length_bytes_received++;
        } else if (length_bytes_received == 1) {
            payload_length |= ((uint16_t)b << 8);
            length_bytes_received++;
#ifdef MICROLAB_DEBUG_SERIAL2_RX
            Serial1.print("[sm] cmd=0x");
            Serial1.print(active_command, HEX);
            Serial1.print(" expecting ");
            Serial1.print(payload_length);
            Serial1.println(" bytes");
#endif
            if (payload_length == 0) {
                process_command(active_command, payload, 0);
                reset_state_machine();
            }
        } else {
            payload[payload_bytes_received] = (char)b;
            payload_bytes_received++;
            if (payload_bytes_received >= payload_length) {
#ifdef MICROLAB_DEBUG_SERIAL2_RX
                Serial1.print("[sm] complete cmd=0x");
                Serial1.println(active_command, HEX);
#endif
                process_command(active_command, payload, payload_length);
                reset_state_machine();
            }
        }
    }
}
