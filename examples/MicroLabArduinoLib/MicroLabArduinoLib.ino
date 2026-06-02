#include <microlab.h>
#include "pico/time.h"

#define LED_RED    7
#define LED_GREEN 4
#define LED_BLUE  5

#define PIN_UART0_TX        0
#define PIN_UART0_RX        1

unsigned long lastSendTime = 0; 
float last_picture = 0;

void setup() {
  pinMode(LED_BLUE,  OUTPUT);
  pinMode(LED_GREEN, OUTPUT);
  pinMode(LED_RED,   OUTPUT);

  //Debug serial that goes to the picoprobe
  //RP2350 UART0
  Serial1.setTX(PIN_UART0_TX);   // 
  Serial1.setRX(PIN_UART0_RX);   // 
  Serial1.begin(115200);
    while(!Serial1); 

  delay(100);

  Serial1.write("HELLO WORLD\n");

  MicroLab.begin();
  Serial1.write("INIT CAMERA\n");
  MicroLab.initCamera();
  for (uint32_t t = to_ms_since_boot(get_absolute_time()); to_ms_since_boot(get_absolute_time()) - t < 2000;)
    MicroLab.update();
  Serial1.write("SET RESOLUTION\n");
  MicroLab.setCameraResolution("240x240");
  for (uint32_t t = to_ms_since_boot(get_absolute_time()); to_ms_since_boot(get_absolute_time()) - t < 2000;)
    MicroLab.update();
}

void loop() {
  MicroLab.update();

  float red = 0, green = 0, blue = 0, picture = 0;
  MicroLab.receive_data("red",   red);
  MicroLab.receive_data("green", green);
  MicroLab.receive_data("blue",  blue);
  MicroLab.receive_data("picture",  picture);

  digitalWrite(LED_RED,   red   > 0.5 ? HIGH : LOW);
  digitalWrite(LED_GREEN, green > 0.5 ? HIGH : LOW);
  digitalWrite(LED_BLUE,  blue  > 0.5 ? HIGH : LOW);

  // if(last_picture != picture){
  //   MicroLab.setCameraResolution("240x240");
  //   delay(1000);
  //   MicroLab.takePicture();
  // }

  // // Check if 500 milliseconds (half a second) have passed
  // if (to_ms_since_boot(get_absolute_time()) - lastSendTime >= 200 && to_ms_since_boot(get_absolute_time()) < 10500) {
  //     MicroLab.send_data("test", (int)random(0, 100));
  //     lastSendTime = to_ms_since_boot(get_absolute_time()); // Reset the timer
  // }

  // last_picture = picture;
}