#include <MicroLab.h>

int sendback_val = 0;
int startcam_val = 0;
int pic_val      = 0;
int stopcam_val  = 0;

// Resolution lookup for __pic index: 0 = 96x96, 1 = 240x240
const char* pic_resolutions[] = { "96x96", "240x240" };

void setup() {
    MicroLab.begin();

    MicroLab.linkToTopic("__sendback", sendback_val);
    MicroLab.linkToTopic("__startcam", startcam_val);
    MicroLab.linkToTopic("__pic",      pic_val);
    MicroLab.linkToTopic("__stopcam",  stopcam_val);
}

void loop() {
    MicroLab.doBackgroundTasks();

    if (MicroLab.received("__sendback")) {
        MicroLab.write("__sendback", sendback_val);
    }

    if (MicroLab.received("__startcam")) {
        MicroLab.turnOnCamera();
    }

    if (MicroLab.received("__pic")) {
        if (pic_val >= 0 && pic_val < 2)
            MicroLab.setCameraResolution(pic_resolutions[pic_val]);
        MicroLab.takePicture();
    }

    if (MicroLab.received("__stopcam")) {
        MicroLab.turnOffCamera();
    }
}
