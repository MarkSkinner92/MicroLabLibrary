#include <MicroLab.h>

#define LED_BLUE 5

int rate = 500;

void setup() {
    MicroLab.begin();
    MicroLab.linkToTopic("rate", rate);
    pinMode(LED_BLUE, OUTPUT);
}

void loop() {
    MicroLab.doBackgroundTasks();
    // Clamp to at least 1 ms so a zero/negative value from the server
    // doesn't stall the loop indefinitely.
    int half_period = rate > 1 ? rate : 1;

    digitalWrite(LED_BLUE, HIGH);
    MicroLab.delay(half_period);   // keeps background tasks running during the wait

    digitalWrite(LED_BLUE, LOW);
    MicroLab.delay(half_period);
}
