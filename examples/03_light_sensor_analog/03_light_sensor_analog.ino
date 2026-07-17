/*
This program showcases the ability for a MicroLab to generate data and have it displayed on the dashboard.
Three types of messages are supported: numeric, state, and event. You do not need to specify this in the code;
   however, you will need to specify the data type in the graph widget to choose how the data will be represented there.

Telemetry topics:
    "light_sensor" -> A numeric message published every half-second representing the light level percentage.
    "above_90" -> A state message published every half-second. 1 if light level is above 90%, 0 if below
    "above_90_warning" -> An event message published every time the light level exceeds 90% (measured every half-second)

*/

#include <MicroLab.h>

#define ANALOG_LIGHT_SENSOR 28

bool above90LastTime = false;

void setup() {
    MicroLab.begin();
    MicroLab.beginDebugSerial(115200);
    MicroLab.enableControlEcho();
}

void loop() {
    MicroLab.doBackgroundTasks();

    int rawLight = analogRead(ANALOG_LIGHT_SENSOR);
    float lightPercent = (float)rawLight / 10.24;
    bool isAbove90 = lightPercent > 90;

    MicroLab.serial.println();
    MicroLab.write("light_sensor", lightPercent);
    MicroLab.write("above_90", isAbove90);

    if(isAbove90 && !above90LastTime){
      MicroLab.write("above_90_warning", "Light levels > 90%");
    }

    MicroLab.delay(100);
    above90LastTime = isAbove90;
}
