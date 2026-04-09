// AV Theremin v2 — Edson Pavoni
// XIAO ESP32-S3 + 2x VL53L1X → USB MIDI CC
//
// Board settings in Arduino IDE:
//   Board: XIAO ESP32S3
//   USB Mode: USB-OTG (TinyUSB)
//   USB CDC On Boot: Enabled
//
// Features:
//   - Exponential smoothing filter (removes jitter)
//   - 3-second decay when hand removed (smooth fade to zero)
//   - Holds last value between sensor reads (no zero gaps)
//
// MIDI output:
//   CC#1 channel 1 = sensor 1 (smooth)
//   CC#2 channel 1 = sensor 2 (smooth)

#include <Wire.h>
#include <VL53L1X.h>
#include "Adafruit_TinyUSB.h"

Adafruit_USBD_MIDI usb_midi;

// --- USB Device Name ---
// This makes the device appear as "AV Theremin" in MIDI apps
void setupUSBName() {
  TinyUSBDevice.setManufacturerDescriptor("Edson Pavoni");
  TinyUSBDevice.setProductDescriptor("AV Theremin");
}

// --- Pins (same as Release001 — proven working) ---
#define XSHUT1  1
#define XSHUT2  4
#define MY_SDA  5
#define MY_SCL  6
#define LED     21

// --- Range ---
const int MIN_MM = 30;
const int MAX_MM = 400;

// --- Smoothing filter weight (0.0–1.0, higher = more smoothing) ---
const float FILTER_W = 0.7;

// --- Decay: time in ms to fade from last value to zero ---
const unsigned long DECAY_MS = 1500;

// --- MIDI send rate ---
const unsigned long MIDI_INTERVAL_MS = 20;

// --- Sensors ---
VL53L1X sensor1;
VL53L1X sensor2;
bool s1ok = false;
bool s2ok = false;

// --- Per-sensor state ---
struct SensorState {
  float filtered;           // smoothed distance in mm
  unsigned long lastSeenAt; // millis() of last valid reading
  bool wasValid;            // was previous reading valid?
  uint8_t lastSentCC;       // last MIDI CC value sent (avoid duplicates)
};

SensorState st1 = {0, 0, false, 255};
SensorState st2 = {0, 0, false, 255};

void blink(int n, int on_ms, int off_ms) {
  for (int i = 0; i < n; i++) {
    digitalWrite(LED, HIGH); delay(on_ms);
    digitalWrite(LED, LOW);  delay(off_ms);
  }
}

void setup() {
  setupUSBName();
  usb_midi.begin();
  delay(1000);

  Serial.begin(115200);

  pinMode(LED, OUTPUT);
  digitalWrite(LED, LOW);

  Wire.begin(MY_SDA, MY_SCL);
  Wire.setClock(400000);

  // --- Init sensors (same sequence as Release001) ---
  pinMode(XSHUT1, OUTPUT);
  pinMode(XSHUT2, OUTPUT);
  digitalWrite(XSHUT1, LOW);
  digitalWrite(XSHUT2, LOW);
  delay(100);

  digitalWrite(XSHUT1, HIGH);
  delay(100);
  sensor1.setTimeout(500);
  s1ok = sensor1.init();
  if (s1ok) sensor1.setAddress(0x30);

  digitalWrite(XSHUT2, HIGH);
  delay(100);
  sensor2.setTimeout(500);
  s2ok = sensor2.init();
  if (s2ok) sensor2.setAddress(0x31);

  if (s1ok) sensor1.startContinuous(100);
  if (s2ok) sensor2.startContinuous(100);

  // LED feedback
  if (s1ok && s2ok)    blink(3, 300, 200);
  else if (s1ok)       blink(2, 300, 200);
  else if (s2ok)       blink(1, 300, 200);
  else                 blink(20, 50, 50);

  Serial.printf("Sensors: s1=%s s2=%s\n", s1ok?"OK":"FAIL", s2ok?"OK":"FAIL");
}

// --- Per-sensor read + filter ---
void updateSensor(VL53L1X &sensor, bool ok, SensorState &st, unsigned long now) {
  if (ok && sensor.dataReady()) {
    int d = sensor.read();
    if (!sensor.timeoutOccurred() && d > 0 && d <= MAX_MM) {
      if (d < MIN_MM) d = MIN_MM;
      // Exponential smoothing
      st.filtered = st.wasValid
        ? (FILTER_W * st.filtered + (1.0 - FILTER_W) * d)
        : d;
      st.lastSeenAt = now;
      st.wasValid = true;
    }
  }
}

// --- Convert filtered mm to MIDI 0-127, with decay ---
uint8_t toMidi(SensorState &st, unsigned long now) {
  if (!st.wasValid || st.lastSeenAt == 0) return 0;

  unsigned long elapsed = now - st.lastSeenAt;
  float mm;

  if (elapsed < 200) {
    // Hand present — use filtered value
    mm = st.filtered;
  } else if (elapsed < DECAY_MS) {
    // Decay
    mm = st.filtered * (1.0 - (float)elapsed / DECAY_MS);
  } else {
    st.wasValid = false;
    return 0;
  }

  if (mm < MIN_MM) return 0;
  float norm = (mm - MIN_MM) / (MAX_MM - MIN_MM);
  return (uint8_t)(constrain(norm, 0.0, 1.0) * 127.0);
}

void loop() {
  unsigned long now = millis();

  updateSensor(sensor1, s1ok, st1, now);
  updateSensor(sensor2, s2ok, st2, now);

  uint8_t cc1 = toMidi(st1, now);
  uint8_t cc2 = toMidi(st2, now);

  // Always send — same as diagnostic version that worked
  uint8_t msg1[] = {0xB0, 1, cc1};
  uint8_t msg2[] = {0xB0, 2, cc2};
  usb_midi.write(msg1, 3);
  usb_midi.write(msg2, 3);

  digitalWrite(LED, (cc1 > 0 || cc2 > 0) ? HIGH : LOW);

  delay(50);
}
