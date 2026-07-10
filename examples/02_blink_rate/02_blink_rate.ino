/*
This program showcases the ability for the dashboard to tweak parameters of a running experiment.
As "halfPeriod" changes, the LED blink rate will change accordinly

Control topics:
    "halfPeriod" -> milliseconds between LED state changes

For convenience, you can import the attached json into your dashboard to automatically set up the necessary control(s)
*/

#include <MicroLab.h>

#define LED_RED 6

int halfPeriod = 100;

void setup() {
    MicroLab.begin();
    MicroLab.linkToTopic("halfPeriod", halfPeriod);
    pinMode(LED_RED, OUTPUT);
}

void loop() {
    MicroLab.doBackgroundTasks();
    // Clamp the half period between 1 and 3000 inclusive.
    int halfPeriodClamped = halfPeriod > 1 ? (halfPeriod <= 3000 ? halfPeriod : 3000) : 1;

    digitalWrite(LED_RED, HIGH);
    MicroLab.delay(halfPeriodClamped);   // keeps background tasks running during the wait

    digitalWrite(LED_RED, LOW);
    MicroLab.delay(halfPeriodClamped);
}
