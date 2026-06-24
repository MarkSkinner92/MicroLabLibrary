#include <MicroLab.h>

// Smooth random walk state — each value drifts toward a randomly-chosen target
float val_a = 50.0f,  target_a = 50.0f;   // range 0–100
float val_b = 0.0f,   target_b = 0.0f;    // range -50–50

uint32_t last_picture_ms = 0;

// Lerp current toward target; pick a new target when close
static float smooth(float current, float& target, float lo, float hi) {
    if (fabsf(current - target) < 0.5f)
        target = lo + (float)random(0, 10001) / 10000.0f * (hi - lo);
    return current + (target - current) * 0.2f;
}

void setup() {
    MicroLab.beginDebugSerial(115200);
    MicroLab.begin();
    randomSeed(micros());

    MicroLab.turnOnCamera();
    MicroLab.setCameraResolution("240x240");
    MicroLab.setCameraLED("auto");
}

void loop() {
    MicroLab.doBackgroundTasks();

    val_a = smooth(val_a, target_a,   0.0f, 100.0f);
    val_b = smooth(val_b, target_b, -50.0f,  50.0f);

    MicroLab.write("sensor_a", val_a);
    MicroLab.write("sensor_b", val_b);

    if (millis() - last_picture_ms >= 5000) {
        last_picture_ms = millis();
        MicroLab.takePicture();
    }

    MicroLab.delay(500);
}
