# MicroLab

Arduino library for talking to the MicroLab coprocessor over UART: publishing telemetry to the dashboard, receiving control values back, timekeeping, and driving the onboard camera.

This document covers every public method on `MicroLabClass` (the global `MicroLab` instance): whether it blocks, how long it can take, what it returns, and any hard limits.

## Required boilerplate

Every sketch needs `MicroLab.begin()` in `setup()` and `MicroLab.doBackgroundTasks()` in `loop()`. Nothing else works reliably without both.

```cpp
void setup() {
    MicroLab.begin();
}

void loop() {
    MicroLab.doBackgroundTasks();
    // ...
}
```

---

## Lifecycle

### `void begin()`
- **Blocking:** No (aside from a fixed internal `while (!Serial2)` wait for the UART peripheral to come up, which is effectively instant on the RP2350).
- **Returns:** nothing.
- **Notes:** Idempotent — a second call is a no-op if already initialized. Resets all internal state (camera flags, caches, link table, cooldown timers). Must be called before any other method; every other public method checks an internal `_initialized` flag and fails safe (returns `false`/default/0) if called first.

### `void beginDebugSerial(uint32_t baud = 115200)`
- **Blocking:** No.
- **Returns:** nothing.
- **Notes:** Opens `Serial1` on pins TX0/RX1 as a separate USB-UART debug console. Safe to call before or after `begin()`. Use `MicroLab.serial.print(...)` to write to it.

### `void doBackgroundTasks()` (alias: `void update()`)
- **Blocking:** Effectively no. It drains whatever is waiting on `Serial2`, and if it happens to stop mid-packet it will spin for **up to 20 ms** to let the packet finish before returning (`is_packet_in_progress()` guard) — negligible next to a typical loop.
- **Returns:** nothing.
- **Must be called every `loop()` iteration.** It is responsible for: processing incoming UART packets, updating any variables bound with `linkToTopic()`, clearing the camera-busy flag once a burst's duration elapses, flushing the outbound telemetry queue (see `flush()`, at most once per second), and retrying the absolute-time sync request every 2 s until the RTC date is confirmed.
- `update()` is a pure alias, kept for backward compatibility — identical behavior.

### `void delay(uint32_t ms)`
- **Blocking:** **Yes, for exactly `ms` milliseconds** — but cooperatively: it spins calling `doBackgroundTasks()` in a loop rather than halting like the stock Arduino `delay()`.
- **Returns:** nothing.
- **Notes:** Use this instead of `::delay()` for anything over ~10 ms. Using the stock `delay()` for longer stretches starves `doBackgroundTasks()`, which can cause missed UART packets, stalled telemetry flushes, and a camera that appears to hang.

## Publishing telemetry (device → dashboard)

### `bool write(const char* topic, int data)`
### `bool write(const char* topic, float data)`
### `bool write(const char* topic, double data)`
### `bool write(const char* topic, const char* data)`
- **Blocking:** No. `write()` only queues a line in memory — it is sent on the next `flush()` (up to ~1 s later), never immediately.
- **Returns:** `true` if the line was queued, `false` if it was rejected or dropped.
- **Limits:**
  - **~20 writes/second effective throughput.** The outbound queue holds at most **20 lines** (`OUTBOUND_CACHE_MAX_LINES`) between flushes; once full, further `write()` calls return `false` and the data is silently dropped until the next flush empties the queue.
  - **20 unique topic names max** (`WRITE_TOPIC_MAX`) for the life of the sketch (topics are registered permanently on first use — writing under a 21st distinct topic name fails).
  - Topic name max length: **64 characters**.
  - `const char*` overload: combined `topic + data` length must be under **128 characters** (`OUTBOUND_LINE_MAX_LEN`), and `data` may not contain a comma, `\n`, or `\r` (would corrupt the CSV row) — such calls return `false`.
  - Topic name `"camera"` is reserved and cannot be used.
  - Which of the three display types (numeric / state / event) a topic renders as on the dashboard is chosen by the graph widget configuration there — the code doesn't declare it.

---

## Reading control values (dashboard → device)

### `bool linkToTopic(const char* topic, float& var)`
### `bool linkToTopic(const char* topic, double& var)`
### `bool linkToTopic(const char* topic, int& var)`
- **Blocking:** No.
- **Returns:** `true` if the link was registered, `false` if the link table is full or `topic` is null.
- **Limits:** **20 links max** total, shared across all three overloads (`CONTROL_LINK_MAX`). Topic names longer than 20 characters are silently truncated (`CACHE_CHANNEL_LEN = 21`, including the null terminator). No duplicate-name checking — linking the same topic twice consumes two slots.
- **Notes:** Call once in `setup()`. The bound variable is updated automatically inside `doBackgroundTasks()` whenever a fresh value for that topic arrives — no polling needed. `var` is left untouched (keeps its last value) on any loop where nothing new arrived for that topic.

### `int read(const char* topic, int defaultValue) const`
### `float read(const char* topic, float defaultValue) const`
- **Blocking:** No.
- **Returns:** the topic's latest known value, or `defaultValue` if the topic has never been received (or the library isn't initialized).
- **Notes:** Pull-based alternative to `linkToTopic()` — call it whenever you need the value rather than binding a variable. Internally a linear scan over received topics (max 50 — see below), effectively O(1) in practice.

### `bool received(const char* topic)`
- **Blocking:** No.
- **Returns:** `true` only once, on the first call after a fresh value for that topic has arrived — it's edge-triggered (checking-and-clearing a flag), not a plain "has this ever arrived" check. Calling it again before the next update returns `false`.
- **Notes:** Use for one-shot actions triggered by a control change (a "take picture" button, a mode switch), not for reading the value itself — pair it with `read()`.

### `bool receiveData(const char* channel, double& out) const`
- **Blocking:** No.
- **Returns:** `true` and fills `out` if `channel` has a known value; `false` otherwise (leaves `out` untouched).
- **Notes:** Lower-level than `read()` — always a `double`, no default-value convenience, and doesn't consume the received-flag the way `received()` does.
- **Shared limit for all control-input methods:** the control cache holds at most **50 distinct topics** (`CACHE_MAX_ITEMS`) at once; a 51st distinct incoming topic name is dropped by the firmware-side JSON parser. Topic names are truncated at 20 characters, same as `linkToTopic()`.

---

## Control echo (debugging what the dashboard actually sent)

### `uint8_t enableControlEcho()`
- **Blocking:** No.
- **Returns:** the number of topics successfully packed and queued **on this call's immediate echo pass** (a `uint8_t`, so 0–255, though it's realistically capped by the same 20-line/flush limit as `write()`).
- **Notes:** One-time call (typically in `setup()`) that also flips on automatic echoing: after this, every incoming control packet is automatically republished under the `"control_echo"` topic as `"topic: value"` lines, packed as many per line as fit, with no further calls needed. Idempotent — calling it again just runs one more manual echo pass. If the packed lines exceed the outbound queue's free capacity, the remainder for that pass are dropped (same silent-drop behavior as `write()`).

---

## Time

### `uint64_t getMissionTime()`
- **Blocking:** No, O(1).
- **Returns:** milliseconds on the dashboard's mission clock. If the mission is stopped, returns the frozen timestamp from when it was stopped rather than advancing.
- **Notes:** Mission start/stop/reset are driven remotely from the dashboard, not by the device.

### `uint64_t getAbsoluteTime()`
- **Blocking:** No, O(1).
- **Returns:** real (RTC-synced) time in milliseconds, independent of mission start/stop state.
- **Notes:** Reads as ms-since-boot (epoch 1970) until the coprocessor's RTC sync response arrives — `doBackgroundTasks()` keeps retrying that request every 2 s in the background until it succeeds, with no action needed from your code.

---

## Camera

All camera methods except `cameraReady()` require `turnOnCamera()` to have already succeeded, and all of them (including `turnOnCamera()`/`turnOffCamera()`) fail immediately (return `false`) if the camera is currently busy — see `cameraReady()`.

### `bool turnOnCamera()`
- **Blocking: yes, up to 5000 ms** while it spin-waits for the coprocessor's acknowledgment (blocks the whole sketch, not just the caller's logic — background comms are still serviced during the spin, but `loop()` doesn't advance).
- **Returns:** `true` once acknowledged (or immediately `true` if the camera is already on — a no-op); `false` on timeout or if a call is already within a 1 s cooldown of the previous one.
- **Limits:** 1-second minimum interval between calls (`HTTP_COOLDOWN_MS`).

### `bool turnOffCamera()`
- **Blocking: yes, up to 2000 ms.**
- **Returns:** `true` once acknowledged; `false` on timeout, if the camera isn't on, if it's currently busy, or if called within 1 s of a previous call.

### `bool cameraReady() const`
- **Blocking:** No, O(1) — just reads two flags.
- **Returns:** `true` only if the camera has been turned on **and** isn't in the middle of taking a picture/burst or another camera command.

### `bool setCameraResolution(const char* res)`
- **Blocking: yes, up to 2000 ms.**
- **Returns:** `true` once acknowledged; `false` immediately if `res` isn't one of the 16 valid names (see table below), or the usual camera-not-on / busy / 1 s-cooldown reasons.
- **Notes:** `res` is a name string like `"VGA"`, not an index — see the [resolution table](#camera-resolutions) below. Affects the FPS cap used by a later `takeBurst()` and the cooldown used by a later `takePicture()`.

### `bool setCameraLED(const char* mode)`
- **Blocking: yes, up to 2000 ms.**
- **Returns:** `true` once acknowledged; `false` immediately if `mode` isn't exactly `"on"`, `"off"`, or `"auto"`, or the usual camera-not-on / busy / 1 s-cooldown reasons.

### `bool takePicture(uint32_t timeout_ms = 5000)`
- **Blocking:** **Depends on `timeout_ms`.**
  - `timeout_ms > 0` (default 5000): blocks up to that many milliseconds waiting for the picture to complete.
  - `timeout_ms == 0`: **fire-and-forget, non-blocking.** Returns `true` right away without waiting; the camera stays reported as busy (`cameraReady() == false`) until a later `doBackgroundTasks()` call processes the completion response. Only safe to use from `loop()`, where `doBackgroundTasks()` keeps running afterward — calling it with `timeout_ms == 0` from `setup()` would leave the camera stuck "busy" forever, since nothing would ever process the response.
- **Returns:** `true` on success (or immediately on a `timeout_ms == 0` fire-and-forget call); `false` if the camera isn't on, is already busy, is still within its per-resolution cooldown (see [picture cooldown table](#camera-resolutions)), or the wait times out.
- **Limits:** cooldown between pictures depends on the resolution the *previous* picture was taken at — ranges from 3 s (`96x96`) to 60 s (`WQXGA`).

### `bool takeBurst(int fps, int duration)`
- **Blocking:** partially — blocks up to **5000 ms** waiting for the coprocessor to accept the burst request. Once accepted, the burst itself runs **in the background**; the call returns immediately after the accept and does not wait out the full duration.
- **Returns:** `true` if the burst was accepted; `false` if the camera isn't on, is busy, or the initial accept times out.
- **Limits:**
  - `duration` is clamped to **1–10 seconds**, silently (not rejected).
  - `fps` is clamped to **1 up to the max for whatever resolution is currently set** — see the [burst FPS table](#burst-mode-fps-limits) below. Resolution is **not** a parameter to this call; it uses whatever `setCameraResolution()` last set.
  - After acceptance, the camera reports busy (`cameraReady() == false`, and all other camera calls fail) for `duration + 1` extra seconds — i.e. slightly longer than the burst itself.

---

## Appendix

### Camera resolutions

| Index | Name | Pixel Dimensions | Cooldown after `takePicture()` at this resolution |
|---|---|---|---|
| 0 | `96x96` | 96×96 | 3 s |
| 1 | `QQVGA` | 160×120 | 5 s |
| 2 | `QCIF` | 176×144 | 7 s |
| 3 | `HQVGA` | 240×176 | 10 s |
| 4 | `240x240` *(default)* | 240×240 | 12 s |
| 5 | `QVGA` | 320×240 | 15 s |
| 6 | `CIF` | 352×288 | 17 s |
| 7 | `HVGA` | 480×320 | 20 s |
| 8 | `VGA` | 640×480 | 25 s |
| 9 | `SVGA` | 800×600 | 30 s |
| 10 | `XGA` | 1024×768 | 35 s |
| 11 | `HD` | 1280×720 | 40 s |
| 12 | `SXGA` | 1280×1024 | 45 s |
| 13 | `UXGA` | 1600×1200 | 50 s |
| 14 | `QHDA` | 2560×1440 | 55 s |
| 15 | `WQXGA` | 2560×1600 | 60 s |

Pass the name string (e.g. `"VGA"`) to `setCameraResolution()`. The cooldown applies based on the resolution of the *last* picture taken, not the one you're switching to.

### Burst-mode FPS limits

| Index | Resolution | Max Burst FPS |
|---|---|---|
| 0 | `96x96` | 30 |
| 1 | `QQVGA` | 30 |
| 2 | `QCIF` | 30 |
| 3 | `HQVGA` | 20 |
| 4 | `240x240` *(default)* | 20 |
| 5 | `QVGA` | 15 |
| 6 | `CIF` | 15 |
| 7 | `HVGA` | 10 |
| 8 | `VGA` | 8 |
| 9 | `SVGA` | 5 |
| 10 | `XGA` | 3 |
| 11 | `HD` | 3 |
| 12 | `SXGA` | 2 |
| 13 | `UXGA` | 2 |
| 14 | `QHDA` | 1 |
| 15 | `WQXGA` | 1 |
