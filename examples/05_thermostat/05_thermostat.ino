#include <MicroLab.h>
#include <Wire.h>

/*
  Reads temperature from an SHT30 sensor (I2C, address 0x44) once per second
  and runs a bang-bang (on/off) thermostat against it, driving a heater
  resistor through a low-side driver.

  The heater turns on once measured_temp drops HYSTERESIS_C below set_temp,
  and turns off once it rises HYSTERESIS_C above set_temp. The dead band
  keeps the heater from chattering right at the setpoint.

  set_temp is constrained to 20-40 degrees C, regardless of what a client
  writes to the topic.

  Publishes:
    "measured_temp" — temperature in degrees Celsius
    "heater_on"      — 1 while the heater is on, 0 while it's off

  Subscribes to:
    "set_temp" — target temperature setpoint in degrees Celsius (clamped to 20-40)
*/

#define SHT30_ADDR 0x44
#define HEATER_RESISTOR 4 // Heater resistor on pin 4 (Low Side Driver)

#define SETPOINT_MIN_C 20.0f
#define SETPOINT_MAX_C 40.0f
#define HYSTERESIS_C    0.5f

// I2C1 (Wire1) pins on this RP2350 board
#define I2C1_SDA 2
#define I2C1_SCL 3

#define I2C_BUS Wire1

float set_temp      = 20.0f;
float measured_temp =  0.0f;
bool  heaterOn       = false;

bool readSHT30(float& temp_c) {
    I2C_BUS.beginTransmission(SHT30_ADDR);
    I2C_BUS.write(0x24);  // single-shot measurement, high repeatability, no clock stretching
    I2C_BUS.write(0x00);
    if (I2C_BUS.endTransmission() != 0) return false;

    delay(15);  // SHT30 needs up to 15ms to complete a high-repeatability measurement

    if (I2C_BUS.requestFrom((uint8_t)SHT30_ADDR, (uint8_t)6) != 6) return false;

    uint8_t data[6];
    for (uint8_t i = 0; i < 6; i++) data[i] = I2C_BUS.read();

    // Bytes 0-1: raw temperature, byte 2: temperature CRC (not verified here)
    uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
    temp_c = -45.0f + 175.0f * (float)raw / 65535.0f;
    return true;
}

void setup() {
    MicroLab.beginDebugSerial(115200);

    pinMode(HEATER_RESISTOR, OUTPUT);
    digitalWrite(HEATER_RESISTOR, LOW);

    I2C_BUS.setSDA(I2C1_SDA);
    I2C_BUS.setSCL(I2C1_SCL);
    I2C_BUS.begin();
    MicroLab.begin();
    MicroLab.linkToTopic("set_temp", set_temp);
    MicroLab.enableControlEcho();
}

void loop() {
    MicroLab.doBackgroundTasks();

    set_temp = constrain(set_temp, SETPOINT_MIN_C, SETPOINT_MAX_C);

    if (readSHT30(measured_temp)) {
        MicroLab.write("measured_temp", measured_temp);

        // Bang-bang control with a hysteresis dead band around the setpoint.
        if (measured_temp < set_temp - HYSTERESIS_C) {
            heaterOn = true;
        } else if (measured_temp > set_temp + HYSTERESIS_C) {
            heaterOn = false;
        }
        digitalWrite(HEATER_RESISTOR, heaterOn ? HIGH : LOW);
        MicroLab.write("heater_on", heaterOn ? 1 : 0);

        MicroLab.serial.print("Set: ");
        MicroLab.serial.print(set_temp, 1);
        MicroLab.serial.print("C  Measured: ");
        MicroLab.serial.print(measured_temp, 1);
        MicroLab.serial.print("C  Heater: ");
        MicroLab.serial.println(heaterOn ? "ON" : "OFF");
    } else {
        MicroLab.serial.println("SHT30 read failed");
    }

    MicroLab.delay(500);
}
