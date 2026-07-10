#include <MicroLab.h>

#define LED_RED  6

void setup() {
  pinMode(LED_RED,  OUTPUT);

  // Must be called in setup for every MicroLab program.
  MicroLab.begin();
}

void loop() {
  // Must be called in loop for every MicroLab program
  MicroLab.doBackgroundTasks();

  // For delay times larger than 10ms, 
  // it is important to use MicroLab.delay() instead of delay()
  // This allows the MicroLab to do important background tasks while it's waiting.
  digitalWrite(LED_RED, HIGH);
  MicroLab.delay(500);
  digitalWrite(LED_RED, LOW);
  MicroLab.delay(500);
}