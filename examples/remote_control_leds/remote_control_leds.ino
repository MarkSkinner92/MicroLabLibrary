#include <MicroLab.h>

/*
  Subscribes to topics:
    "rate":
        [1-1000] milliseconds between led on/off
    "included_leds"
        [0] No LEDs
        [1] Blue LED only
        [2] Green LED only
        [3] Both LEDs
*/

#define BLUE_PIN 5
#define GREEN_PIN 4

int rate = 300;
int included_leds = 1;

void setup() {
    MicroLab.begin();

    pinMode(BLUE_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);

    MicroLab.linkToTopic("rate", rate);
    MicroLab.linkToTopic("included_leds", included_leds);
}

void loop() {
    MicroLab.doBackgroundTasks();

    // Turn included LEDs on
    if(included_leds == 1 || included_leds == 3) digitalWrite(BLUE_PIN, HIGH);
    if(included_leds == 2 || included_leds == 3) digitalWrite(GREEN_PIN, HIGH);
    MicroLab.delay(rate);

    // Turn all LEDs off
    digitalWrite(BLUE_PIN, LOW);
    digitalWrite(GREEN_PIN, LOW);
    MicroLab.delay(rate);
}
