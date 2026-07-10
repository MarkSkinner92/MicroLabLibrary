/*
This program scans the i2c bus and reports every connected i2c device's
address on MicroLab.serial. Useful for discovering what's wired up before
writing a sensor-specific program.
*/

#include <MicroLab.h>
#include <Wire.h>

// I2C1 (Wire1) pins on this RP2350 board
#define I2C1_SDA 2
#define I2C1_SCL 3

#define I2C_BUS Wire1

void setup() {
    MicroLab.begin();
    MicroLab.beginDebugSerial(115200);

    I2C_BUS.setSDA(I2C1_SDA);
    I2C_BUS.setSCL(I2C1_SCL);
    I2C_BUS.begin();
}

void loop() {
    MicroLab.doBackgroundTasks();

    MicroLab.serial.println("Scanning i2c bus...");

    int devicesFound = 0;
    for (uint8_t address = 1; address < 127; address++) {
        I2C_BUS.beginTransmission(address);
        uint8_t error = I2C_BUS.endTransmission();

        if (error == 0) {
            MicroLab.serial.print("  Found device at 0x");
            if (address < 16) {
                MicroLab.serial.print("0");
            }
            MicroLab.serial.println(address, HEX);
            devicesFound++;
        }
    }

    if (devicesFound == 0) {
        MicroLab.serial.println("  No i2c devices found");
    } else {
        MicroLab.serial.print("  Done. ");
        MicroLab.serial.print(devicesFound);
        MicroLab.serial.println(" device(s) found.");
    }

    MicroLab.delay(2000);
}
