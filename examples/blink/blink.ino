#include <MicroLab.h>

#define LED_BLUE  5

void setup() {
  pinMode(LED_BLUE,  OUTPUT);

  MicroLab.begin();
}

void loop() {
  MicroLab.doBackgroundTasks();

  // For delay times larger than 10ms, 
  // it is important to use MicroLab.delay() instead of delay()
  // This allows the MicroLab to do important background tasks while it's waiting.
  digitalWrite(LED_BLUE, HIGH);
  MicroLab.delay(1000);
  digitalWrite(LED_BLUE, LOW);
  MicroLab.delay(1000);
}