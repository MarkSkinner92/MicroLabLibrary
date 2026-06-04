# MicroLab Arduino Library

MicroLab is an Arduino library for the MicroLab science payload board. It lets your sketch talk to a web dashboard — you can read controls that a user sets in a browser (like sliders and buttons), send sensor readings back to be logged and graphed, and control an onboard camera.

---

## Quick Start

Every MicroLab sketch follows the same three-step pattern:

```cpp
#include <microlab.h>

void setup() {
  MicroLab.begin();         // 1. Start the library
}

void loop() {
  MicroLab.do_background_tasks();  // 2. Call this every loop — keeps communication running

  if (MicroLab.controlDataArrived()) {  // 3. React to inputs from the dashboard
    int speed = MicroLab.read("motor_speed", 0);
    MicroLab.write("speed_echo", speed);
  }
}
```

---

## Setup

### `MicroLab.begin()`

**Call this once in `setup()`.**

Starts the library and opens the serial connection to the MicroLab coprocessor. You must call this before using any other MicroLab functions.

```cpp
MicroLab.begin();
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `baud` | `uint32_t` | `500000` | Communication speed. Leave this at the default. |

---

## The Loop

### `MicroLab.do_background_tasks()`

**Call this at the top of every `loop()`.**

This function handles all communication with the coprocessor in the background — receiving data from the dashboard, sending your logged data, and processing camera responses. If you forget to call it, nothing will work.

```cpp
void loop() {
  MicroLab.do_background_tasks();  // Always call this first

  // ... the rest of your code
}
```

> **Note:** `MicroLab.update()` is an alias for the same function. Both do the same thing.

---

## Receiving Control Data

The dashboard lets users change values with sliders, buttons, and text boxes. These values are sent to your sketch as **topics** — just a name paired with a number.

### `MicroLab.controlDataArrived()`

Returns `true` once each time a new batch of control data arrives from the dashboard. Use this as a gate before reading topics so you only react when something actually changed.

```cpp
if (MicroLab.controlDataArrived()) {
  // New data is here — safe to read topics
}
```

> **Important:** This returns `true` only once per delivery. The next call will return `false` until new data arrives.

---

### `MicroLab.read(topic, defaultValue)`

Reads the current value of a topic sent from the dashboard. Returns `defaultValue` if the topic hasn't arrived yet.

```cpp
int   value = MicroLab.read("my_topic", 0);    // reads as int
float value = MicroLab.read("my_topic", 0.0f); // reads as float
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `topic` | `const char*` | The topic name, matching what the dashboard sends |
| `defaultValue` | `int` or `float` | Returned when the topic has no value yet |

**Example — reading a brightness slider:**
```cpp
if (MicroLab.controlDataArrived()) {
  int brightness = MicroLab.read("brightness", 0);
  analogWrite(LED_PIN, brightness);
}
```

---

### `MicroLab.received(topic)`

Returns `true` if the given topic was included in the most recent data delivery. Useful for one-shot actions like buttons — you only want to trigger them when the topic was actually sent, not just when it has a value.

```cpp
bool wasPressed = MicroLab.received("take_picture");
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `topic` | `const char*` | The topic name to check |

**Example — triggering an action from a button:**
```cpp
if (MicroLab.controlDataArrived()) {
  if (MicroLab.received("take_picture") && MicroLab.read("take_picture", 0)) {
    MicroLab.takePicture();
  }
}
```

> **Tip:** Combine `received()` with `read()` — `received()` confirms the dashboard sent the topic this time, and `read()` gives you its value.

---

## Sending Data

Use `write()` to send sensor readings, states, or any number back to the dashboard. Data is batched and sent automatically once per second.

### `MicroLab.write(topic, value)`

Logs a value under a topic name. The dashboard will display and graph it.

```cpp
MicroLab.write("temperature", 23.5f);
MicroLab.write("led_on",      1);
MicroLab.write("status",      "ok");
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `topic` | `const char*` | Name for this data channel (keep it short and descriptive) |
| `value` | `int`, `float`, `double`, or `const char*` | The value to send |

**Example — logging a temperature sensor every loop:**
```cpp
void loop() {
  MicroLab.do_background_tasks();

  float temp = readTemperatureSensor();  // your sensor code
  MicroLab.write("temperature_c", temp);
}
```

> **Note:** `MicroLab.send_data()` is an alias for `write()`. Both do the same thing.

---

## Camera

The MicroLab board has an OV5640 camera. Before taking pictures, you need to initialize it and set a resolution.

### `MicroLab.initCamera()`

**Call once in `setup()` before using any other camera functions.**

Turns the camera on and waits for it to be ready. Returns `true` on success.

```cpp
MicroLab.initCamera();
```

---

### `MicroLab.setCameraResolution(resolution)`

Sets the image size. Call this in `setup()` after `initCamera()`. Returns `true` if the resolution name was valid and the camera accepted it.

```cpp
MicroLab.setCameraResolution("240x240");
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `resolution` | `const char*` | One of the valid resolution strings (see table below) |

**Valid resolutions:**

| Name | Size |
|------|------|
| `"96x96"` | 96 × 96 |
| `"QQVGA"` | 160 × 120 |
| `"QCIF"` | 176 × 144 |
| `"HQVGA"` | 240 × 176 |
| `"240x240"` | 240 × 240 |
| `"QVGA"` | 320 × 240 |
| `"CIF"` | 400 × 296 |
| `"HVGA"` | 480 × 320 |
| `"VGA"` | 640 × 480 |
| `"SVGA"` | 800 × 600 |
| `"XGA"` | 1024 × 768 |
| `"HD"` | 1280 × 720 |
| `"SXGA"` | 1280 × 1024 |
| `"UXGA"` | 1600 × 1200 |
| `"QHDA"` | 2560 × 1440 |
| `"WQXGA"` | 2560 × 1600 |

> **Tip:** Larger images take longer to save and use more storage. `"240x240"` or `"QVGA"` are good starting points.

---

### `MicroLab.setCameraLED(mode)`

Controls the camera flash LED. Returns `true` if the mode was valid and accepted.

```cpp
MicroLab.setCameraLED("auto");  // default — LED turns on only while capturing
MicroLab.setCameraLED("on");    // LED stays on permanently
MicroLab.setCameraLED("off");   // LED stays off permanently
```

| Parameter | Type | Description |
|-----------|------|-------------|
| `mode` | `const char*` | `"on"`, `"off"`, or `"auto"` |

| Mode | Behavior |
|------|----------|
| `"auto"` | LED turns on while a picture is being taken, then off (default) |
| `"on"` | LED stays on all the time |
| `"off"` | LED stays off all the time |

---

### `MicroLab.takePicture()`

Captures an image and saves it. Returns `true` if the picture was taken successfully.

```cpp
MicroLab.takePicture();
```

| Parameter | Type | Default | Description |
|-----------|------|---------|-------------|
| `timeout_ms` | `uint32_t` | `5000` | How long to wait (in milliseconds) for the camera to confirm the shot |

**Example — taking a picture when a dashboard button is pressed:**
```cpp
if (MicroLab.controlDataArrived()) {
  if (MicroLab.received("snap") && MicroLab.read("snap", 0)) {
    bool ok = MicroLab.takePicture();
    MicroLab.write("picture_taken", ok ? 1 : 0);
  }
}
```

> **Note:** `takePicture()` blocks your sketch for up to `timeout_ms` milliseconds while waiting for confirmation. During this time `do_background_tasks()` is handled internally.

---

### `MicroLab.cameraReady()`

Returns `true` if the camera is initialized and not currently busy taking a picture. Use this to avoid starting a new picture before the previous one finishes.

```cpp
if (MicroLab.cameraReady()) {
  MicroLab.takePicture();
}
```

**Full camera setup example:**
```cpp
void setup() {
  MicroLab.begin();
  MicroLab.initCamera();
  MicroLab.setCameraResolution("QVGA");
  MicroLab.setCameraLED("auto");
}
```

---

## Timing

MicroLab tracks two clocks that are automatically synchronized with the payload system when it connects.

### `MicroLab.getMissionTime()`

Returns the number of milliseconds since the **mission started** (i.e., since the experiment was powered on and the clock was synced). Use this to timestamp your data relative to the start of the experiment.

```cpp
unsigned long t = MicroLab.getMissionTime();
MicroLab.write("timestamp_ms", (float)t);
```

---

### `MicroLab.getAbsoluteTime()`

Returns the current **real-world time** as milliseconds since January 1, 1970 (Unix time), once the onboard RTC has been synced. Before sync, this falls back to time-since-boot.

```cpp
unsigned long t = MicroLab.getAbsoluteTime();
```

> **Tip:** For most experiments, `getMissionTime()` is the more useful of the two — it tells you *when* during the mission something happened. `getAbsoluteTime()` is useful if you need to correlate data with an external clock.

---

## Full Example Sketch

```cpp
#include <microlab.h>

#define LED_PIN 5

void setup() {
  pinMode(LED_PIN, OUTPUT);

  MicroLab.begin();
  MicroLab.initCamera();
  MicroLab.setCameraResolution("QVGA");
  MicroLab.setCameraLED("auto");
}

void loop() {
  MicroLab.do_background_tasks();

  // Log time every loop
  MicroLab.write("mission_time_ms", (float)MicroLab.getMissionTime());

  // React to dashboard controls
  if (MicroLab.controlDataArrived()) {

    // LED on/off toggle
    int ledState = MicroLab.read("led", 0);
    digitalWrite(LED_PIN, ledState);
    MicroLab.write("led_state", ledState);

    // Take a picture when the button is pressed
    if (MicroLab.received("snap") && MicroLab.read("snap", 0)) {
      if (MicroLab.cameraReady()) {
        bool ok = MicroLab.takePicture();
        MicroLab.write("picture_taken", ok ? 1 : 0);
      }
    }
  }
}
```
