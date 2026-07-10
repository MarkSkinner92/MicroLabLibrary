/*
This program showcases the ability for a MicroLab to generate data and have it displayed on the dashboard.
This makes use of the i2c color sensor to capture the levels of red, green, and blue present.
Shine different colors on the sensor and watch what happens to the levels in real-time

Telemetry topics:
    "red_level" -> numeric data source from 0-100 (red channel's own intensity, percent of its brightest reading so far)
    "green_level" -> numeric data source from 0-100 (green channel's own intensity, percent of its brightest reading so far)
    "blue_level" -> numeric data source from 0-100 (blue channel's own intensity, percent of its brightest reading so far)

It's nice to put these all on the same graph, as they all have the same scale.
You can import LightSensorGraph.json as a telmetry view to quickly set up the dashboard
*/

#include <MicroLab.h>
#include <Wire.h>

#define DIGITAL_COLOR_SENSOR_ADDR 0x10 // VEML 6040 i2c sensor

#define RED_LED 6
#define GREEN_LED 7

// I2C1 (Wire1) pins on this RP2350 board
#define I2C1_SDA 2
#define I2C1_SCL 3

// VEML6040 register addresses
#define VEML6040_CONFIG_REG 0x00
#define VEML6040_RED_REG    0x08
#define VEML6040_GREEN_REG  0x09
#define VEML6040_BLUE_REG   0x0A

// Config value: IT = 40ms, auto mode, sensor enabled (all bits 0)
#define VEML6040_CONFIG_VALUE 0x00

#define I2C_BUS Wire1

float redLevel = 0;
float greenLevel = 0;
float blueLevel = 0;

// Each channel's brightest raw reading seen so far, used to auto-scale that
// channel's own 0-100% intensity. In practice the VEML6040's raw counts under
// normal indoor/LED light only use a small slice of the sensor's theoretical
// 16-bit range, so scaling against the fixed full scale made every reading
// collapse to a few percent. Scaling each channel against its own observed
// peak keeps the percentages independent (no division by the other channels)
// while making real changes in that channel's light level clearly visible.
// Start above 0 so an initial noise blip can't cause a divide-by-a-tiny-number
// spike.
uint16_t maxRed   = 50;
uint16_t maxGreen = 50;
uint16_t maxBlue  = 50;

int ledState = 0;

void veml6040Init() {
    I2C_BUS.beginTransmission(DIGITAL_COLOR_SENSOR_ADDR);
    I2C_BUS.write(VEML6040_CONFIG_REG);
    I2C_BUS.write(VEML6040_CONFIG_VALUE); // LSB
    I2C_BUS.write(0x00);                  // MSB
    I2C_BUS.endTransmission();
}

uint16_t veml6040ReadChannel(uint8_t reg) {
    I2C_BUS.beginTransmission(DIGITAL_COLOR_SENSOR_ADDR);
    I2C_BUS.write(reg);
    uint8_t writeStatus = I2C_BUS.endTransmission(false); // repeated start, keep the bus held for the read

    uint8_t bytesReceived = I2C_BUS.requestFrom(DIGITAL_COLOR_SENSOR_ADDR, 2);

    // writeStatus: 0 = ok, 1 = data too long, 2 = NACK on address, 3 = NACK on data, 4 = other error
    // bytesReceived should be 2 if the read actually happened
    MicroLab.serial.print("  [reg 0x");
    MicroLab.serial.print(reg, HEX);
    MicroLab.serial.print(" writeStatus=");
    MicroLab.serial.print(writeStatus);
    MicroLab.serial.print(" bytesReceived=");
    MicroLab.serial.print(bytesReceived);
    MicroLab.serial.print("]");

    if (bytesReceived < 2) {
        return 0;
    }

    uint8_t lsb = I2C_BUS.read();
    uint8_t msb = I2C_BUS.read();
    return (uint16_t)(msb << 8) | lsb;
}

void setup() {
    MicroLab.begin();
    MicroLab.beginDebugSerial(115200);

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

    I2C_BUS.setSDA(I2C1_SDA);
    I2C_BUS.setSCL(I2C1_SCL);
    I2C_BUS.begin();

    veml6040Init();
    MicroLab.delay(200); // let the first integration cycle finish before reading
}

void loop() {
    MicroLab.doBackgroundTasks();

    switch((ledState/8)%4){
      case 0:
        digitalWrite(RED_LED,   HIGH);
        digitalWrite(GREEN_LED, LOW);
        break;
      case 1:
        digitalWrite(RED_LED,   LOW);
        digitalWrite(GREEN_LED, HIGH);
        break;
      case 2:
        digitalWrite(RED_LED,   HIGH);
        digitalWrite(GREEN_LED, HIGH);
        break;
      case 3:
        digitalWrite(RED_LED,   LOW);
        digitalWrite(GREEN_LED, LOW);
        break;
    }
    ledState++;

    uint16_t rawRed   = veml6040ReadChannel(VEML6040_RED_REG);
    uint16_t rawGreen = veml6040ReadChannel(VEML6040_GREEN_REG);
    uint16_t rawBlue  = veml6040ReadChannel(VEML6040_BLUE_REG);
    MicroLab.serial.println();

    if (rawRed   > maxRed)   maxRed   = rawRed;
    if (rawGreen > maxGreen) maxGreen = rawGreen;
    if (rawBlue  > maxBlue)  maxBlue  = rawBlue;

    // Absolute intensity of each channel on its own, scaled against that
    // channel's own brightest reading so far, independent of the other
    // channels.
    redLevel   = ((float)rawRed   / maxRed)   * 100.0f;
    greenLevel = ((float)rawGreen / maxGreen) * 100.0f;
    blueLevel  = ((float)rawBlue  / maxBlue)  * 100.0f;

    MicroLab.write("red_level", redLevel);
    MicroLab.write("green_level", greenLevel);
    MicroLab.write("blue_level", blueLevel);

    MicroLab.serial.print("raw R: ");
    MicroLab.serial.print(rawRed);
    MicroLab.serial.print("  raw G: ");
    MicroLab.serial.print(rawGreen);
    MicroLab.serial.print("  raw B: ");
    MicroLab.serial.println(rawBlue);

    MicroLab.serial.print("R: ");
    MicroLab.serial.print(redLevel);
    MicroLab.serial.print("  G: ");
    MicroLab.serial.print(greenLevel);
    MicroLab.serial.print("  B: ");
    MicroLab.serial.println(blueLevel);

    MicroLab.delay(250);
}
