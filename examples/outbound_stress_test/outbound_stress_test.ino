#include <MicroLab.h>

/*
  Publishes 20 topics at 1 Hz with random data to stress-test the outbound pipeline.
  Also takes a 640x480 photo every 2 seconds with auto flash.

  4 x string  topics — random state labels
  4 x int     topics — random state (1, 2, or 3)
  4 x float   topics — random range   0 to 100
  4 x float   topics — random range   0 to 1000
  4 x float   topics — random range -200 to 200
*/

const char* status_vals[] = { "idle", "running", "error", "standby" };
const char* mode_vals[]   = { "auto", "manual", "calibrate" };
const char* alert_vals[]  = { "none", "warning", "critical" };
const char* phase_vals[]  = { "init", "ramp", "hold", "cool" };

uint32_t last_picture_ms = 0;

void setup() {
    pinMode(5, OUTPUT);
    MicroLab.begin();
    MicroLab.beginDebugSerial(115200);
    // randomSeed(micros());

    MicroLab.turnOnCamera();
    // MicroLab.setCameraResolution("96x96");  // 640x480
    MicroLab.setCameraLED("auto");
}

void loop() {
    MicroLab.doBackgroundTasks();

    digitalWrite(5, HIGH);
    MicroLab.delay(200);
    digitalWrite(5, LOW);
    MicroLab.delay(100);

    // // Strings
    // MicroLab.write("status", status_vals[random(4)]);
    // MicroLab.write("mode",   mode_vals[random(3)]);
    // MicroLab.write("alert",  alert_vals[random(3)]);
    // MicroLab.write("phase",  phase_vals[random(4)]);

    // // Ints — states 1, 2, or 3
    // MicroLab.write("valve_state", (int)random(1, 4));
    // MicroLab.write("pump_state",  (int)random(1, 4));
    // MicroLab.write("fan_state",   (int)random(1, 4));
    // MicroLab.write("led_state",   (int)random(1, 4));

    // // Floats 0–100
    // MicroLab.write("humidity",        (float)random(0, 10001) / 100.0f);
    // MicroLab.write("battery_pct",     (float)random(0, 10001) / 100.0f);
    // MicroLab.write("fill_level",      (float)random(0, 10001) / 100.0f);
    // MicroLab.write("signal_strength", (float)random(0, 10001) / 100.0f);

    // // Floats 0–1000
    // MicroLab.write("pressure",  (float)random(0, 100001) / 100.0f);
    // MicroLab.write("rpm",       (float)random(0, 100001) / 100.0f);
    // MicroLab.write("flow_rate", (float)random(0, 100001) / 100.0f);
    // MicroLab.write("power_w",   (float)random(0, 100001) / 100.0f);

    // // Floats -200–200
    // MicroLab.write("temperature",  (float)random(-20000, 20001) / 100.0f);
    // MicroLab.write("velocity",     (float)random(-20000, 20001) / 100.0f);
    // MicroLab.write("acceleration", (float)random(-20000, 20001) / 100.0f);
    // MicroLab.write("torque",       (float)random(-20000, 20001) / 100.0f);

    // MicroLab.serial.println("Published 20 topics");

    // if (millis() - last_picture_ms >= 20) {
    //     last_picture_ms = millis();
        // MicroLab.serial.println("Taking picture...");
        MicroLab.takePicture();
    // }

    // MicroLab.delay(1000);
}
