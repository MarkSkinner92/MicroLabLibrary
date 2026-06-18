#include <MicroLab.h>
#include <Wire.h>

/*
  NEEDS WORK
    - The setpoint doesn't actually set the target
    - Data is just written out the UART, not to the student dashboard

  Reads temperature from an SHT30 sensor (I2C, address 0x44) once per second
  and publishes it to the MicroLab.

  Publishes:
    "measured_temp" — temperature in degrees Celsius

  Subscribes to:
    "set_temp" — target temperature setpoint in degrees Celsius
*/

#define SHT30_ADDR 0x44

float set_temp      = 20.0f;
float measured_temp =  0.0f;

bool readSHT30(float& temp_c) {
    Wire.beginTransmission(SHT30_ADDR);
    Wire.write(0x24);  // single-shot measurement, high repeatability, no clock stretching
    Wire.write(0x00);
    if (Wire.endTransmission() != 0) return false;

    delay(15);  // SHT30 needs up to 15ms to complete a high-repeatability measurement

    if (Wire.requestFrom((uint8_t)SHT30_ADDR, (uint8_t)6) != 6) return false;

    uint8_t data[6];
    for (uint8_t i = 0; i < 6; i++) data[i] = Wire.read();

    // Bytes 0-1: raw temperature, byte 2: temperature CRC (not verified here)
    uint16_t raw = ((uint16_t)data[0] << 8) | data[1];
    temp_c = -45.0f + 175.0f * (float)raw / 65535.0f;
    return true;
}

void setup() {
    MicroLab.beginDebugSerial(115200);
    MicroLab.begin();

    Wire.begin();

    MicroLab.linkToTopic("set_temp", set_temp);
}

void loop() {
    MicroLab.doBackgroundTasks();

    if (readSHT30(measured_temp)) {
        MicroLab.write("measured_temp", measured_temp);

        MicroLab.serial.print("Set: ");
        MicroLab.serial.print(set_temp, 1);
        MicroLab.serial.print("C  Measured: ");
        MicroLab.serial.print(measured_temp, 1);
        MicroLab.serial.println("C");
    } else {
        MicroLab.serial.println("SHT30 read failed");
    }

    MicroLab.delay(1000);
}
