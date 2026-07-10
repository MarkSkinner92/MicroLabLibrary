#include <MicroLab.h>

/*
  Subscribes to topics:
    "take_burst"
        [1] Start a 5-second burst at 3 fps

  GPIO 5 blinks at 1 Hz while the burst is in progress,
  then turns off when the burst completes.
*/

#define LED_PIN 5

int take_burst = 0;

bool  burst_active   = false;
bool  led_state      = false;
uint32_t last_blink_ms = 0;

void setup() {
    MicroLab.begin();
    MicroLab.beginDebugSerial(115200);

    pinMode(LED_PIN, OUTPUT);

    MicroLab.turnOnCamera();
    MicroLab.setCameraResolution("240x240");
    MicroLab.setCameraLED("auto");

    MicroLab.linkToTopic("take_burst", take_burst);
}

void loop() {
    MicroLab.doBackgroundTasks();

    if (MicroLab.received("take_burst") && take_burst == 1) {
        if (MicroLab.takeBurst(3, 5)) {
            burst_active   = true;
            led_state      = false;
            last_blink_ms  = millis();
        }
    }

    if (burst_active) {
        if (!MicroLab.cameraReady()) {
            uint32_t now = millis();
            if (now - last_blink_ms >= 500) {
                last_blink_ms = now;
                led_state = !led_state;
                digitalWrite(LED_PIN, led_state ? HIGH : LOW);
            }
        } else {
            burst_active = false;
            digitalWrite(LED_PIN, LOW);
        }
    }
}
