/*
 * input.h  —  Button HAL  (debounce + state machine)
 * ============================================================
 *  All buttons are INPUT_PULLUP.  Active LOW.
 *  Debounce: 30 ms stable window.
 *  Provides:  justPressed / held / released per button.
 */

#ifndef INPUT_H
#define INPUT_H

#include <Arduino.h>

// ── Pin map ──────────────────────────────────
#define PIN_BTN_PWR   2
#define PIN_BTN_FWD   3
#define PIN_BTN_BWD   4
#define PIN_BTN_SEL  10
#define PIN_BUZZER    9

// ── Note shortcuts ───────────────────────────
#define NOTE_LOW   330
#define NOTE_NAV   660
#define NOTE_SEL   880
#define NOTE_WARN  200

// ── Button IDs ───────────────────────────────
enum ButtonID : uint8_t {
  BTN_PWR = 0,
  BTN_FWD,
  BTN_BWD,
  BTN_SEL,
  BTN_COUNT
};

// ── Debounce state per button ─────────────────
struct BtnState {
  uint8_t  pin;
  bool     current;        // debounced state (true = pressed)
  bool     raw;            // last raw read
  bool     justPressed;
  bool     justReleased;
  bool     held;
  uint32_t lastChangeMs;
};

// ── Buzzer non-blocking ───────────────────────
static uint32_t _buzzerEndMs = 0;

inline void buzzerPip(uint16_t freq, uint16_t dur = 30) {
  tone(PIN_BUZZER, freq, dur);
  _buzzerEndMs = millis() + dur;
}

// ── Module API ───────────────────────────────
void inputInit();
void inputUpdate(uint32_t now);
bool inputJustPressed(ButtonID id);
bool inputJustReleased(ButtonID id);
bool inputHeld(ButtonID id);

// ── Implementation (header-only for single .ino build) ──
#ifdef INPUT_IMPL

static BtnState _buttons[BTN_COUNT];
static const uint8_t _pins[BTN_COUNT] = {
  PIN_BTN_PWR, PIN_BTN_FWD, PIN_BTN_BWD, PIN_BTN_SEL
};
static const uint16_t DEBOUNCE_MS = 30;

void inputInit() {
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    _buttons[i].pin          = _pins[i];
    _buttons[i].current      = false;
    _buttons[i].raw          = true;   // pull-up default HIGH = not pressed
    _buttons[i].justPressed  = false;
    _buttons[i].justReleased = false;
    _buttons[i].held         = false;
    _buttons[i].lastChangeMs = 0;
  }
}

void inputUpdate(uint32_t now) {
  for (uint8_t i = 0; i < BTN_COUNT; i++) {
    BtnState &b = _buttons[i];
    b.justPressed  = false;
    b.justReleased = false;

    bool raw = (digitalRead(b.pin) == LOW);   // LOW = pressed (pull-up)

    if (raw != b.raw) {
      b.raw          = raw;
      b.lastChangeMs = now;
    }

    if ((now - b.lastChangeMs) >= DEBOUNCE_MS) {
      if (raw != b.current) {
        b.current = raw;
        if (b.current) {
          b.justPressed = true;
          b.held        = true;
        } else {
          b.justReleased = true;
          b.held         = false;
        }
      }
    }
  }
}

bool inputJustPressed(ButtonID id)  { return _buttons[id].justPressed;  }
bool inputJustReleased(ButtonID id) { return _buttons[id].justReleased; }
bool inputHeld(ButtonID id)         { return _buttons[id].held;         }

#endif // INPUT_IMPL
#endif // INPUT_H
