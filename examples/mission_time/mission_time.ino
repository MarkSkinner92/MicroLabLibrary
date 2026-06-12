#include <MicroLab.h>

/*
  initial state:    both LEDs off
  10 Seconds:       Only blue LED on
  20 Seconds:       Only green LED on
  30 Seconds:       Both LEDs on
  40 Seconds:       both LEDS off.

  Prints the mission time to the serial monitor
*/

#define BLUE_PIN 5
#define GREEN_PIN 4

// Using -1 as an "unset" flag
uint64_t lastMissionTimeSeconds = -1;

void setup() {
    MicroLab.beginDebugSerial(115200);
    MicroLab.begin();

    pinMode(BLUE_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
}

void loop() {
    MicroLab.doBackgroundTasks();

    uint64_t missionTimeSeconds = MicroLab.getMissionTime() / 1000;

    if(missionTimeSeconds < 10){
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(GREEN_PIN, LOW);
    }
    else if(missionTimeSeconds < 20){
      digitalWrite(BLUE_PIN, HIGH);
      digitalWrite(GREEN_PIN, LOW);
    }
    else if(missionTimeSeconds < 30){
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(GREEN_PIN, HIGH);
    }
    else if(missionTimeSeconds < 40){
      digitalWrite(BLUE_PIN, HIGH);
      digitalWrite(GREEN_PIN, HIGH);
    }
    else {
      digitalWrite(BLUE_PIN, LOW);
      digitalWrite(GREEN_PIN, LOW);
    }

    if(missionTimeSeconds != lastMissionTimeSeconds){
      MicroLab.serial.print("Mission time: ");
      MicroLab.serial.print(missionTimeSeconds);
      MicroLab.serial.println(" sec");
    }
    lastMissionTimeSeconds = missionTimeSeconds;
}
