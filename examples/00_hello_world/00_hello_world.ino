/*
Plug the USB-UART in to the top of your demo board, and then into your computer.
You can use this to print debug messages from your MicroLab without having to publish them to the dashboard
*/

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
