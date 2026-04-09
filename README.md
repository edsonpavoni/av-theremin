# AV Theremin

A gesture-controlled MIDI instrument by [Edson Pavoni](https://edsonpavoni.art). Two time-of-flight distance sensors translate hand position into continuous MIDI control signals, turning empty space into a musical interface.

## How It Works

The AV Theremin uses two VL53L1X laser time-of-flight sensors mounted on opposite sides of a custom PCB. Each sensor measures the distance to the performer's hand (30-400mm range) and converts it into a MIDI Control Change message sent over USB.

The instrument appears as a standard USB MIDI device — no drivers needed. Plug it into any computer and it works with Ableton Live, TouchDesigner, Max/MSP, or any MIDI-capable software.

### Signal Flow

```
Hand position (mm) → VL53L1X ToF sensor → I2C → ESP32-S3
    → Exponential smoothing filter → Decay envelope
    → MIDI CC (0-127) → USB MIDI → Host computer
```

### MIDI Output

| Channel | CC# | Source |
|---------|-----|--------|
| 1 | 1 | Sensor 1 — distance mapped to 0-127 |
| 1 | 2 | Sensor 2 — distance mapped to 0-127 |

Closer hand = lower CC value. Farther hand = higher CC value. When the hand is removed, the value decays smoothly to zero over 1.5 seconds.

## Hardware

### Components

| Part | Description |
|------|-------------|
| **MCU** | Seeed Studio XIAO ESP32-S3 |
| **Sensors** | 2x VL53L1X time-of-flight distance sensor (I2C) |
| **PCB** | Custom board — "Visual Theremin v02" |
| **Connection** | USB-C (power + MIDI data) |

### PCB Pin Mapping

| Function | XIAO Pin | GPIO |
|----------|----------|------|
| Sensor 1 XSHUT | D1 | GPIO2 |
| Sensor 2 XSHUT | D4 | GPIO7 |
| I2C SDA | D5 | GPIO8 |
| I2C SCL | D6 | GPIO9 |
| Status LED | — | GPIO21 |

The two sensors share the I2C bus. On startup, sensor 1 is brought out of reset first and reassigned to address `0x30`, then sensor 2 is brought up at `0x31`. The XSHUT pins control this sequencing.

### Schematic

```
                    USB-C
                      |
                 [XIAO ESP32-S3]
                /    |    |    \
           XSHUT1  SDA  SCL  XSHUT2
              |      |    |      |
         [VL53L1X]---+----+---[VL53L1X]
          Sensor 1   I2C bus   Sensor 2
         (addr 0x30)          (addr 0x31)
```

## Firmware

### Features

- **Exponential smoothing** — removes sensor jitter for clean MIDI output (weight: 0.7)
- **Decay envelope** — when the hand is removed, the value fades to zero over 1.5 seconds instead of cutting off abruptly
- **USB MIDI** — class-compliant, no drivers needed
- **LED diagnostics** — on startup, the LED blinks to indicate sensor status:
  - 3 blinks = both sensors OK
  - 2 blinks = only sensor 1
  - 1 blink = only sensor 2
  - Rapid flicker = no sensors found

### Building and Uploading

**Requirements:**
- [Arduino IDE](https://www.arduino.cc/en/software) 2.x
- Board package: `esp32` by Espressif (install via Board Manager)
- Libraries (install via Library Manager):
  - `VL53L1X` by Pololu
  - `Adafruit TinyUSB Library` by Adafruit

**Arduino IDE Settings:**

| Setting | Value |
|---------|-------|
| Board | XIAO_ESP32S3 |
| USB Mode | USB-OTG (TinyUSB) |
| USB CDC On Boot | Enabled |

**Upload:**
1. Open `firmware/av-theremin.ino` in Arduino IDE
2. Select the correct board and port
3. Click Upload

### Tuning Parameters

These constants at the top of the firmware can be adjusted:

```cpp
const float FILTER_W = 0.7;           // Smoothing: 0.0 (none) to 1.0 (max)
const unsigned long DECAY_MS = 1500;   // Fade-out time in milliseconds
const int MIN_MM = 30;                 // Closest detection distance
const int MAX_MM = 400;                // Farthest detection distance
```

## License

MIT

## Author

[Edson Pavoni](https://edsonpavoni.art) — artist and engineer based in Brooklyn, NY.
