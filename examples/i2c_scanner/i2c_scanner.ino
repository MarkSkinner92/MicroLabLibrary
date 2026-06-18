#include <MicroLab.h>
#include <Wire.h>

void setup() {
    MicroLab.beginDebugSerial(115200);
    Wire.begin();

    MicroLab.serial.println("I2C scanner — scanning addresses 0x01 to 0x7E...");

    uint8_t found = 0;
    for (uint8_t addr = 1; addr < 127; addr++) {
        Wire.beginTransmission(addr);
        uint8_t err = Wire.endTransmission();
        if (err == 0) {
            MicroLab.serial.print("  Device at 0x");
            if (addr < 0x10) MicroLab.serial.print("0");
            MicroLab.serial.println(addr, HEX);
            found++;
        }
    }

    if (found == 0) {
        MicroLab.serial.println("No devices found — check wiring and pull-ups.");
    } else {
        MicroLab.serial.print(found);
        MicroLab.serial.println(" device(s) found.");
    }
}

void loop() {}
