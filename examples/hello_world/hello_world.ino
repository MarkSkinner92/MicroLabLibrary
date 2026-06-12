#include <MicroLab.h>

int counter = 0;

void setup() {
    MicroLab.beginDebugSerial(115200);
    MicroLab.begin();
}

void loop() {
    MicroLab.doBackgroundTasks();

    MicroLab.serial.print("Hello World ");
    MicroLab.serial.println(counter);
    counter++;

    MicroLab.delay(1000);
}
