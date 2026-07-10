/*
  This program introduces the media widget and camera methods.
  Each image resolution has a cool-down period to prevent too many images from being taken to quickly

  Subscribes to topics:
    "resolution"
        [0]  96x96
        [1]  QQVGA  (160x120)
        [2]  QCIF   (176x144)
        [3]  HQVGA  (240x176)
        [4]  240x240
        [5]  QVGA   (320x240)
        [6]  CIF    (352x288)
        [7]  HVGA   (480x320)
        [8]  VGA    (640x480)
        [9]  SVGA   (800x600)
        [10] XGA    (1024x768)
        [11] HD     (1280x720)
        [12] SXGA   (1280x1024)
        [13] UXGA   (1600x1200)
        [14] QHDA   (2560x1440)
        [15] WQXGA  (2560x1600)

    "flash_mode"
        [0] Off
        [1] On
        [2] Auto

    "take_picture"
        [1] Take a picture
*/
#include <MicroLab.h>

const char* resolutionNames[] = {
    "96x96", "QQVGA", "QCIF",  "HQVGA",  "240x240",
    "QVGA",  "CIF",   "HVGA",  "VGA",    "SVGA",
    "XGA",   "HD",    "SXGA",  "UXGA",   "QHDA",   "WQXGA"
};
#define NUM_RESOLUTIONS 16

int resolution   = 8;  // default: VGA
int flash_mode   = 2;  // default: auto
int take_picture = 0;

void setup() {
    MicroLab.begin();
    MicroLab.turnOnCamera();
    MicroLab.beginDebugSerial(115200);

    MicroLab.linkToTopic("resolution",   resolution);
    MicroLab.linkToTopic("flash_mode",   flash_mode);
    MicroLab.linkToTopic("take_picture", take_picture);
}

void loop() {
    MicroLab.doBackgroundTasks();

    if (MicroLab.received("resolution")) {
        MicroLab.serial.print("Set Resolution ");
        MicroLab.serial.println(resolutionNames[resolution]);
        if (resolution >= 0 && resolution < NUM_RESOLUTIONS) {
            MicroLab.setCameraResolution(resolutionNames[resolution]);
        }
    }

    if (MicroLab.received("flash_mode")) {
        MicroLab.serial.print("Set Flash Mode ");
        MicroLab.serial.println(flash_mode);
        if      (flash_mode == 0) MicroLab.setCameraLED("off");
        else if (flash_mode == 1) MicroLab.setCameraLED("on");
        else if (flash_mode == 2) MicroLab.setCameraLED("auto");
    }

    if (MicroLab.received("take_picture") && take_picture == 1) {
        MicroLab.serial.print("Take Picture ");
        MicroLab.serial.println(take_picture);
        MicroLab.takePicture();
    }
}
