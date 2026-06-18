#include <MicroLab.h>

float phase = 0.0f;

void setup() {
    MicroLab.beginDebugSerial(115200);
    MicroLab.begin();
}

void loop() {
    MicroLab.doBackgroundTasks();

    float value = sinf(phase);
    MicroLab.write("sine", value);

    phase += 2.0f * 3.14159265f / 18.0f;  // 10 samples per full cycle
    if (phase >= 2.0f * 3.14159265f) phase -= 2.0f * 3.14159265f;

    MicroLab.delay(500);
}
