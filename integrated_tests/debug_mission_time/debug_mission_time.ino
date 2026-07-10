#include <MicroLab.h>

void setup() {
    MicroLab.beginDebugSerial(115200);
    MicroLab.begin();
}

void loop() {
    MicroLab.doBackgroundTasks();

    MicroLab.serial.println(MicroLab.getMissionTime()/1000);

    MicroLab.delay(500);
}