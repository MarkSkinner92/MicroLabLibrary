/*
  This program makes use of the dropdown widget in the dashboard.

  Subscribes to topics:
    "rate":
        [1-1000] milliseconds between LED state changes
    "included_leds"
        [0] No LEDs
        [1] Blue LED only
        [2] Green LED only
        [3] Both LEDs

    Import the attached json file control view to automatically make the control widgets for this example.
*/
#include <MicroLab.h>

#define RED_LED 6
#define GREEN_LED 7

int rate = 300;
int included_leds = 1;

void setup() {
    MicroLab.begin();

    pinMode(RED_LED, OUTPUT);
    pinMode(GREEN_LED, OUTPUT);

    MicroLab.linkToTopic("rate", rate);
    MicroLab.linkToTopic("included_leds", included_leds);
}

void loop() {
    MicroLab.doBackgroundTasks();

    // Turn included LEDs on
    if(included_leds == 1 || included_leds == 3) digitalWrite(RED_LED, HIGH);
    if(included_leds == 2 || included_leds == 3) digitalWrite(GREEN_LED, HIGH);
    MicroLab.delay(rate);

    // Turn all LEDs off
    digitalWrite(RED_LED, LOW);
    digitalWrite(GREEN_LED, LOW);
    MicroLab.delay(rate);
}
