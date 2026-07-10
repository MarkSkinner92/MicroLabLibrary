
/*
  This program showcases how mission time can be used to control automation tasks
  Try these different exercises
  1. Turning on experiment
  2. Setting the mission time to 15 seconds and then turning on the experiment
  3. Stoping the mission time and starting it at 35 seconds

  After
  0  Seconds:       both LEDs off
  10 Seconds:       Only blue LED on
  20 Seconds:       Only green LED on
  30 Seconds:       Both LEDs on
  40 Seconds:       both LEDS off.

  This program also prints the mission time to the serial monitor
*/

#include <MicroLab.h>

#define RED_PIN 6
#define GREEN_PIN 7

// Using -1 as an "unset" flag
uint64_t lastMissionTimeSeconds = -1;

void setup() {
    MicroLab.beginDebugSerial(115200);
    MicroLab.begin();

    pinMode(RED_PIN, OUTPUT);
    pinMode(GREEN_PIN, OUTPUT);
}

void loop() {
    MicroLab.doBackgroundTasks();

    uint64_t missionTimeSeconds = MicroLab.getMissionTime() / 1000;

    if(missionTimeSeconds < 10){
      digitalWrite(RED_PIN, LOW);
      digitalWrite(GREEN_PIN, LOW);
    }
    else if(missionTimeSeconds < 20){
      digitalWrite(RED_PIN, HIGH);
      digitalWrite(GREEN_PIN, LOW);
    }
    else if(missionTimeSeconds < 30){
      digitalWrite(RED_PIN, LOW);
      digitalWrite(GREEN_PIN, HIGH);
    }
    else if(missionTimeSeconds < 40){
      digitalWrite(RED_PIN, HIGH);
      digitalWrite(GREEN_PIN, HIGH);
    }
    else {
      digitalWrite(RED_PIN, LOW);
      digitalWrite(GREEN_PIN, LOW);
    }

    if(missionTimeSeconds != lastMissionTimeSeconds){
      MicroLab.serial.print("Mission time: ");
      MicroLab.serial.print(missionTimeSeconds);
      MicroLab.serial.println(" sec");
    }
    lastMissionTimeSeconds = missionTimeSeconds;
}
