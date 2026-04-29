# GEMINI OS — Arduino Uno/Nano OLED Operating System

```
  ██████╗ ███████╗███╗   ███╗██╗███╗   ██╗██╗     ██████╗ ███████╗
 ██╔════╝ ██╔════╝████╗ ████║██║████╗  ██║██║    ██╔═══██╗██╔════╝
 ██║  ███╗█████╗  ██╔████╔██║██║██╔██╗ ██║██║    ██║   ██║███████╗
 ██║   ██║██╔══╝  ██║╚██╔╝██║██║██║╚██╗██║██║    ██║   ██║╚════██║
 ╚██████╔╝███████╗██║ ╚═╝ ██║██║██║ ╚████║██║    ╚██████╔╝███████║
  ╚═════╝ ╚══════╝╚═╝     ╚═╝╚═╝╚═╝  ╚═══╝╚═╝     ╚═════╝ ╚══════╝
```

A fully modular, memory-optimized "operating system" for the Arduino Uno/Nano
running on a 128×64 SSD1306 OLED. Features a multi-app UI, smooth animations,
sleep/wake power management, and a non-blocking architecture.

---

## Hardware Requirements

| Component         | Part                        |
|-------------------|-----------------------------|
| Microcontroller   | Arduino Uno or Nano         |
| Display           | SSD1306 128×64 OLED (I2C)   |
| Buttons           | 4× tactile push buttons     |
| Buzzer            | Active or passive buzzer    |

### Wiring Diagram

```
Arduino Uno/Nano
┌─────────────────────────────────────────────────────────┐
│                                                         │
│  A4 ──────── SDA ─────────────────── OLED SDA          │
│  A5 ──────── SCL ─────────────────── OLED SCL          │
│  3.3V/5V ──────────────────────────── OLED VCC          │
│  GND ────────────────────────────────── OLED GND         │
│                                                         │
│  D2 ──┬── 10kΩ ── VCC   ← POWER button (INT0)           │
│       └── Button ── GND                                 │
│                                                         │
│  D3 ──┬── (internal pull-up)  ← FORWARD button          │
│       └── Button ── GND                                 │
│                                                         │
│  D4 ──┬── (internal pull-up)  ← BACKWARD button         │
│       └── Button ── GND                                 │
│                                                         │
│  D10 ─┬── (internal pull-up)  ← SELECT button           │
│       └── Button ── GND                                 │
│                                                         │
│  D9 ──────── Buzzer(+) ───────── Buzzer GND → GND       │
│                                                         │
└─────────────────────────────────────────────────────────┘
```

> **Note:** All buttons use INPUT_PULLUP (active LOW). The external
> 10kΩ on D2 is recommended for the interrupt pin but optional since
> Arduino has internal pull-ups.

---

## Installation

### 1. Install Required Libraries

Open Arduino IDE → Sketch → Include Library → Manage Libraries

Search and install:
- **Adafruit SSD1306** (by Adafruit)
- **Adafruit GFX Library** (by Adafruit)
- **Adafruit BusIO** (auto-dependency)

### 2. Copy Files

Copy all files into a folder named `GeminiOS/`:
```
GeminiOS/
├── GeminiOS.ino      ← Main sketch (open this)
├── input.h           ← Button HAL
├── ui.h              ← UI engine (boot, menu, system info)
├── apps.h            ← Calculator + Alarm
├── animations.h      ← Eye animation system
└── game.h            ← Space shooter game
```

### 3. Upload

Open `GeminiOS.ino` in Arduino IDE, select your board (Uno/Nano),
select the correct COM port, and click Upload.

---

## Controls

| Button    | Pin | Function                                   |
|-----------|-----|--------------------------------------------|
| POWER     | D2  | Sleep / Wake (interrupt-based)             |
| FORWARD   | D3  | Navigate forward / move right in game      |
| BACKWARD  | D4  | Navigate backward / move left / exit apps  |
| SELECT    | D10 | Confirm / press calculator key / fire      |

---

## System Architecture

```
┌─────────────────────────────────────────────────────────┐
│                    GeminiOS.ino                         │
│  OS State Machine + Frame Scheduler (millis-based)      │
├──────────────┬──────────────────────────────────────────┤
│   HAL Layer  │           Kernel Layer                   │
│  ──────────  │  ──────────────────────────────────────  │
│  input.h     │  millis()-based task scheduling          │
│  • debounce  │  30ms frame gate (~30 FPS)               │
│  • state mch │  OSState enum state machine              │
│  • buzzer    │  enterSleep() / powerISR()               │
├──────────────┴──────────────────────────────────────────┤
│                     UI Engine  (ui.h)                   │
│  bootUpdate()   menuUpdate()   sysinfoUpdate()          │
│  uiDrawBox()    uiDrawInvText()  uiDrawProgressBar()    │
├─────────────────────────────────────────────────────────┤
│              Applications Layer                         │
│  apps.h            animations.h        game.h           │
│  • Calculator      • Eye animation     • Space shooter  │
│  • Alarm+EEPROM    • Blink/look/wink   • Collision det. │
└─────────────────────────────────────────────────────────┘
```

---

## Applications

### 🧮 Calculator
- 4×5 key grid layout (7–9, 4–6, 1–3, 0/.=+, C()\<)
- FORWARD/BACKWARD navigate keys, SELECT presses
- Supports `+ - * /` with basic expression parsing
- Expression buffer: 20 chars (`char[]`, no String class)
- Backspace key (`<`) and Clear (`C`)

### 🚀 Shooting Game
- Player ship at bottom, 2×6 enemy grid
- FORWARD/BACKWARD → move player horizontally
- SELECT → manual fire | auto-fire every 600ms
- Bounding-box collision detection
- Wave system with increasing enemy speed
- Score and wave counter in HUD

### 👁️ Eye Animations
- Lifelike robotic eyes with random behavior engine
- Behaviors: idle drift, blink, look left/right, wink L/R
- Smooth lerp-based pupil and eyelid motion
- All timing non-blocking via `millis()`

### ⏰ Alarm
- Toggle ON/OFF with SELECT button
- State saved to EEPROM (addr 0) — persists power cycles
- When ON: screen flashes + repeating buzzer pattern
- Non-blocking flash and tone timing

### 📊 System Info
- Free SRAM (live reading via `freeMemory()`)
- Uptime counter (H M S formatted)
- CPU load % (loop timing vs 33ms frame budget)
- Progress bars for RAM and CPU load

---

## Memory Optimization Techniques

| Technique              | Benefit                                   |
|------------------------|-------------------------------------------|
| `PROGMEM` for icons    | Saves ~40 bytes SRAM for bitmap assets    |
| `char[]` not `String`  | Avoids heap fragmentation entirely        |
| `F()` macro for prints | String literals in Flash, not SRAM        |
| Header-only + `*_IMPL` | Single translation unit, no linker bloat  |
| `int8_t` / `uint8_t`   | Half the size of `int` for small counters |
| Fixed-size bullet pool | No dynamic allocation in game loop        |
| PROGMEM menu labels    | ~60 bytes of string literals out of SRAM  |

Typical free SRAM at runtime: **~400–600 bytes**

---

## Power Management

- **Sleep**: Press POWER button at any time (except boot)
  - Display blanked → OLED off → MCU enters PWR_DOWN mode
  - All peripherals powered down
  - Current draw drops to ~6µA (MCU) + OLED standby

- **Wake**: Press POWER button (INT0 on D2)
  - Full boot sequence plays again
  - Boot animation: CRT scan → progress bar → "System Ready"

---

## Performance

- **Target FPS**: 30 (33ms frame budget)
- **CPU Load**: Typically < 15% measured via `getLoopUs()`
- **No `delay()` calls** anywhere in the main codebase
  - Exception: one 30ms delay before MCU sleep (safe, intentional)
- **Non-blocking tone**: `tone()` with duration — no waiting

---

## EEPROM Usage

| Address | Data         | Description         |
|---------|--------------|---------------------|
| 0       | 0x00 / 0x01  | Alarm ON/OFF state  |

Only 1 byte used. Plenty of room for future settings.

---

## Extending Gemini OS

To add a new app:

1. Create `myapp.h` with `#ifdef MYAPP_IMPL` guard
2. Implement `myappInit()` and `bool myappUpdate(uint32_t now)`
3. Add `#define MYAPP_IMPL` and `#include "myapp.h"` to `GeminiOS.ino`
4. Add menu entry in `ui.h` MENU_LABELS + MENU_ICONS arrays
5. Add `case OS_MYAPP:` to the state machine in `loop()`

---

## Troubleshooting

| Problem                    | Solution                                      |
|----------------------------|-----------------------------------------------|
| Blank screen               | Check SDA/SCL (A4/A5), verify I2C addr 0x3C  |
| Buttons not responding     | Check INPUT_PULLUP wiring (active LOW)        |
| Compilation errors         | Ensure all 5 files are in same folder         |
| Display flickering         | Normal — display.display() needed per frame   |
| "Low memory" warning       | Expected — ~70% SRAM usage is normal          |
| OLED shows garbage on wake | Re-init in enterSleep() handles this          |

---

## License

MIT License — free to use, modify, and distribute.
