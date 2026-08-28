# 🏠 Smart Home Hub — Arduino UNO

A compact, fully-documented **Smart Home Hub** built around an Arduino UNO.
It fuses three classic home-automation subsystems — **security**, **climate**,
and **lighting** — into a single, non-blocking sketch that you can assemble
on a breadboard in an afternoon.

> ✨ The code is written with `millis()` instead of `delay()`, so all sensors
> run in parallel and the LCD updates smoothly without freezing.

---

## 📑 Table of Contents
1. [Features](#-features)
2. [Hardware Required](#-hardware-required)
3. [Libraries](#-libraries)
4. [Wiring & Pinout](#-wiring--pinout)
5. [Power & Safety Notes](#-power--safety-notes)
6. [System Logic](#-system-logic)
7. [Installation & Upload](#-installation--upload)
8. [Calibration Tips](#-calibration-tips)
9. [Project Structure](#-project-structure)
10. [Troubleshooting](#-troubleshooting)
11. [License](#-license)

---

## ✨ Features

| Subsystem  | Sensor        | Actuator             | Behavior |
|------------|---------------|----------------------|----------|
| Security   | PIR (HC-SR501)| Piezo Buzzer         | Motion → 3-second audible alarm |
| Climate    | DHT11         | 16×2 I²C LCD + DC Motor (fan) | Reads T/H every 2 s; fan auto-ON when T > 28 °C |
| Lighting   | LDR + resistor| LED                  | Auto-ON in the dark, auto-OFF in the light |
| UX         | —             | I²C LCD 16×2         | Live display of T, H, fan/LED/PIR state |
| Engine     | —             | —                    | `millis()`-based scheduler (no blocking `delay()`) |

---

## 🧰 Hardware Required

| # | Component | Qty | Notes |
|---|-----------|-----|-------|
| 1 | Arduino UNO (ATmega328P)        | 1 | Any 5 V / 16 MHz clone works |
| 2 | PIR Motion Sensor (HC-SR501)   | 1 | 3-pin: VCC / OUT / GND |
| 3 | DHT11 Temperature & Humidity    | 1 | 3-pin module recommended (has pull-up) |
| 4 | LDR (photoresistor)             | 1 | Plus 10 kΩ resistor (voltage divider) |
| 5 | 16×2 LCD with I²C backpack (PCF8574) | 1 | Address usually `0x27` (or `0x3F`) |
| 6 | Piezo Buzzer (active)           | 1 | 5 V, passive is also fine with `tone()` |
| 7 | 5 V DC Motor (fan)              | 1 | Small 5 V hobby motor |
| 8 | Motor driver                    | 1 | 2N2222 transistor **+ 1N4007 flyback diode** *or* L298N module |
| 9 | LED + 220 Ω resistor            | 1 | Any color, current-limit |
|10 | Breadboard + jumper wires       | 1 | ~30 wires |
|11 | External supply (recommended)   | 1 | 9 V / 1 A barrel for motor + Arduino, **or** 5 V / 2 A USB |

---

## 📚 Libraries

Install through the Arduino IDE → **Library Manager**:

| Library | Author | Purpose |
|---------|--------|---------|
| `DHT sensor library`     | Adafruit | DHT11 readings |
| `Adafruit Unified Sensor`| Adafruit | Dependency of DHT |
| `LiquidCrystal_I2C`      | Frank de Brabander | I²C LCD control |

> Built-in: `Wire.h` (I²C) — already shipped with the IDE.

---

## 🔌 Wiring & Pinout

### Pin Assignment

| Component                  | Signal | Arduino Pin  | Type        |
|----------------------------|--------|--------------|-------------|
| PIR — VCC                  | Power  | **5V**       | Power       |
| PIR — GND                  | Ground | **GND**      | Ground      |
| PIR — OUT                  | Signal | **D2**       | Digital IN  |
| DHT11 — VCC                | Power  | **5V**       | Power       |
| DHT11 — GND                | Ground | **GND**      | Ground      |
| DHT11 — DATA               | Signal | **D3**       | Digital IN  |
| LDR — top node             | Signal | **A0**       | Analog IN   |
| LDR — VCC                  | Power  | **5V**       | Power       |
| LDR — GND (via 10 kΩ)      | Ground | **GND**      | Ground      |
| LCD — VCC                  | Power  | **5V**       | Power       |
| LCD — GND                  | Ground | **GND**      | Ground      |
| LCD — SDA                  | I²C    | **A4**       | I²C Data    |
| LCD — SCL                  | I²C    | **A5**       | I²C Clock   |
| Buzzer — +                 | Signal | **D8**       | Digital OUT |
| Buzzer — −                 | Ground | **GND**      | Ground      |
| LED — anode (via 220 Ω)    | Signal | **D9**       | Digital OUT |
| LED — cathode              | Ground | **GND**      | Ground      |
| Motor driver — IN          | Signal | **D10**      | Digital OUT |
| Motor driver — VCC (motor) | Power  | **EXT 5V/9V**| Power       |
| Motor driver — GND         | Ground | **GND (common with Arduino)** | Ground |

### Visual Pin Map

```
Arduino UNO
  ┌─────────────────────────────┐
  │ D2  ◀── PIR OUT             │
  │ D3  ◀── DHT11 DATA          │
  │ D8  ──▶ Buzzer (+)          │
  │ D9  ──▶ LED (via 220Ω)      │
  │ D10 ──▶ Motor driver IN     │
  │ A0  ◀── LDR divider         │
  │ A4  ◀── LCD SDA             │
  │ A5  ◀── LCD SCL             │
  │ 5V  ──▶ PIR/DHT/LCD/LDR top │
  │ GND ──▶ Common ground rail  │
  └─────────────────────────────┘
```

### LDR Voltage Divider (mandatory)

```
   5V ──┬── LDR ──┬── A0
        │         │
        │        10 kΩ
        │         │
        GND       GND
```
- **Bright light** → LDR resistance ↓ → A0 voltage ↓ → `analogRead` high.
- **Darkness**     → LDR resistance ↑ → A0 voltage ↑ → `analogRead` low.
- The code treats `value < 500` as "dark" (tune to taste).

### DC Motor Driver (two options)

**Option A — 2N2222 (NPN) low-side switch (cheap, single direction):**
```
Arduino D10 ── 1 kΩ ── 2N2222 Base
                Collector ── Motor (−)
                Emitter   ── GND
                Motor (+) ── 5V/9V external
                1N4007 flyback diode across Motor (+/−), cathode to +
```

**Option B — L298N module (recommended, robust):**
```
Arduino D10  ── L298N  IN1
              L298N  IN2 ── GND  (or LOW for single direction)
              L298N  +12V ── External 7–12 V supply
              L298N  GND  ── Common GND with Arduino
              L298N  OUT1, OUT2 ── Motor leads
              L298N  5V jumper ── leave OFF when Arduino powered by barrel
```

---

## ⚡ Power & Safety Notes

> ⚠️ **Never power a DC motor directly from the Arduino 5V pin.** Motors draw
> surge currents that will reset or damage the MCU.

- 🔋 **Use an external supply** for the motor: 9 V / 1 A via the barrel
  jack, or a 5 V / 2 A USB charger feeding the L298N's `+12V` (it has an
  onboard 5 V regulator you can use for the Arduino *if* you remove the
  5 V jumper).
- 🔌 **Common ground** is mandatory. Tie Arduino GND, driver GND, and
  external supply GND together.
- 🛡️ **Flyback diode** (1N4007) across the motor terminals if you use the
  transistor driver — it protects the transistor from inductive kickback.
- 🔥 The 2N2222 in **TO-92** package is fine only for small motors (< 300 mA).
  For anything larger, prefer L298N or a MOSFET (e.g. IRLZ44N).
- 🧯 The 220 Ω resistor on the LED is **not optional** — it limits current
  to a safe ~15 mA and protects both the LED and the Arduino pin.
- 🔍 Double-check the I²C address of your LCD. Run an I²C scanner if `0x27`
  doesn't work; try `0x3F`.

---

## 🧠 System Logic

```
loop() ── every tick (≈1 ms) ────────────────────────────
   │
   ├─► handleMotionAndBuzzer(now)
   │     • Edge-debounced PIR trigger
   │     • Non-blocking 3 s buzzer window
   │
   ├─► if (now - lastDht >= 2000) readClimateAndControlFan()
   │     • dht.readTemperature() / dht.readHumidity()
   │     • if T > 28 °C → fan ON ; else fan OFF
   │
   ├─► handleLighting(now)
   │     • analogRead(LDR) ; dark = LED ON, light = LED OFF
   │
   └─► updateLcd()
         • Line 1: T xx.x°C  H xx%
         • Line 2: F:ON L:OFF M:NO
```

Because every block is gated by `millis()` or an instantaneous check, the
sketch remains responsive: pressing the PIR trigger never delays the LCD
update, and the fan can turn on/off independently of the buzzer.

---

## 🚀 Installation & Upload

1. **Clone or download** this repository.
2. Open `smart_home_hub.ino` in the Arduino IDE.
3. From **Tools → Board**, pick **Arduino UNO**.
4. From **Tools → Port**, select the COM/tty port of your board.
5. Install the three libraries listed in [📚 Libraries](#-libraries).
6. Click **Upload** (→).
7. Open **Serial Monitor** at **9600 baud** to see live logs:
   ```
   [BOOT] Smart Home Hub ready.
   [DHT11] Sensor read FAILED, skipping.
   [FAN]   ON (T > 28C)
   [LDR]   value=412 -> LED ON (dark)
   [ALERT] Motion detected! Buzzer ON
   [ALERT] Buzzer OFF (3s elapsed)
   ```

---

## 🎛️ Calibration Tips

| Knob | Where | What to tune |
|------|-------|--------------|
| Fan trigger temperature    | `TEMP_FAN_ON`  in code  | Try 26–30 °C depending on your DHT11 unit |
| LDR darkness threshold    | `LDR_DARK`     in code  | Read the Serial value in your room and pick the midpoint |
| PIR sensitivity / delay   | Potentiometers on the HC-SR501 board | Left pot = sensitivity, right pot = retrigger time |
| LCD I²C address           | `LCD_ADDR`     in code  | `0x27` (PCF8574) or `0x3F` |
| Buzzer duration           | `BUZZER_DURATION` in code | Change to taste (in milliseconds) |

---

## 📂 Project Structure

```
Smarthome/
├── smart_home_hub.ino   # Main Arduino sketch (the only file you upload)
├── README.md            # You are here 📖
└── LICENSE              # Project license
```

---

## 🛠️ Troubleshooting

| Symptom | Likely cause | Fix |
|---------|--------------|-----|
| LCD is blank / only backlight | Wrong I²C address | Run an I²C scanner sketch, update `LCD_ADDR` |
| LCD shows blocks on top row   | Contrast turned to 0 | Turn the small blue potentiometer on the backpack |
| DHT always returns `NaN`      | Wiring / timing | Use 10 kΩ pull-up between DATA and VCC if using a raw sensor; wait 2 s after boot |
| Fan never turns on            | Threshold not reached / motor wiring | Check `TEMP_FAN_ON`, verify the motor driver wiring, try a 9 V supply |
| PIR triggers constantly       | Retrigger potentiometer too sensitive | Adjust the two orange trimpots on the HC-SR501 |
| Arduino resets when fan starts| Motor drawing too much current | Add decoupling (100 µF across motor), use external power, common GND |
| LED never turns off           | LDR threshold too low | Print `g_ldrValue` over Serial and adjust `LDR_DARK` |

---

## 📜 License

This project is released under the terms of the `LICENSE` file in the
repository root.
