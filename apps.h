/*
 * apps.h  —  Calculator + Alarm applications
 * ============================================================
 *  Calculator:
 *    4-column grid navigator, expression char[] buffer,
 *    integer arithmetic parser, supports + - * /
 *
 *  Alarm:
 *    ON/OFF toggle, flashing screen, repeating buzzer,
 *    EEPROM-backed alarm state
 */

#ifndef APPS_H
#define APPS_H

#include <Arduino.h>
#include <EEPROM.h>
#include "input.h"
#include "ui.h"

// ──────────────────────────────────────────────
//  Calculator API
// ──────────────────────────────────────────────
void   calcInit();
bool   calcUpdate(uint32_t now);   // true → exit to menu

// ──────────────────────────────────────────────
//  Alarm API
// ──────────────────────────────────────────────
void   alarmInit();
bool   alarmUpdate(uint32_t now);  // true → exit to menu

#define EEPROM_ALARM_ADDR 0        // 1 byte: 0=OFF, 1=ON

// ══════════════════════════════════════════════
//  Implementation
// ══════════════════════════════════════════════
#ifdef APPS_IMPL

// ──────────────────────────────────────────────
//  CALCULATOR
// ──────────────────────────────────────────────
/*
 *  Key grid layout (4×5):
 *   Row 0:  7  8  9  /
 *   Row 1:  4  5  6  *
 *   Row 2:  1  2  3  -
 *   Row 3:  0  .  =  +
 *   Row 4:  C  (  )  <  ← backspace
 */

#define CALC_COLS 4
#define CALC_ROWS 5
#define EXPR_BUF  20

static const char CALC_KEYS[CALC_ROWS][CALC_COLS] PROGMEM = {
  {'7','8','9','/'},
  {'4','5','6','*'},
  {'1','2','3','-'},
  {'0','.','=','+'},
  {'C','(',')','\b'}
};

static char    _expr[EXPR_BUF + 1];
static char    _result[EXPR_BUF + 1];
static uint8_t _exprLen   = 0;
static uint8_t _calcCol   = 0;
static uint8_t _calcRow   = 0;
static bool    _calcError = false;

// ── Simple integer expression evaluator ──────
// Handles: single binary op  A op B
// Sufficient for a pocket calc without FP bloat
static bool calcEval(const char* expr, char* outBuf, uint8_t outSz) {
  // Find operator from right (respects last op)
  int8_t opIdx = -1;
  char   op    = 0;
  // Scan from right so we pick lowest precedence last
  // Two-pass: first find + or -, then * or /
  for (int8_t i = strlen(expr) - 1; i >= 1; i--) {
    char c = expr[i];
    if ((c == '+' || c == '-') && i > 0) { opIdx = i; op = c; break; }
  }
  if (opIdx < 0) {
    for (int8_t i = strlen(expr) - 1; i >= 1; i--) {
      char c = expr[i];
      if ((c == '*' || c == '/') && i > 0) { opIdx = i; op = c; break; }
    }
  }
  if (opIdx < 0) {
    // Just a number
    strncpy(outBuf, expr, outSz - 1);
    return true;
  }

  char lbuf[12], rbuf[12];
  uint8_t li = (uint8_t)min((uint8_t)(opIdx), (uint8_t)11);
  strncpy(lbuf, expr, li);
  lbuf[li] = '\0';
  strncpy(rbuf, expr + opIdx + 1, 11);
  rbuf[11] = '\0';

  long a = atol(lbuf);
  long b = atol(rbuf);
  long res = 0;

  switch (op) {
    case '+': res = a + b; break;
    case '-': res = a - b; break;
    case '*': res = a * b; break;
    case '/':
      if (b == 0) { strncpy_P(outBuf, PSTR("Err:Div0"), outSz); return false; }
      res = a / b; break;
  }
  ltoa(res, outBuf, 10);
  return true;
}

void calcInit() {
  memset(_expr, 0, sizeof(_expr));
  memset(_result, 0, sizeof(_result));
  _exprLen   = 0;
  _calcCol   = 0;
  _calcRow   = 0;
  _calcError = false;
}

bool calcUpdate(uint32_t now) {
  Adafruit_SSD1306* d = getDisplay();

  // ── Navigation ──
  if (inputJustPressed(BTN_FWD)) {
    buzzerPip(NOTE_NAV);
    _calcCol = (_calcCol + 1) % CALC_COLS;
  }
  if (inputJustPressed(BTN_BWD)) {
    buzzerPip(NOTE_NAV);
    if (_calcCol == 0) { _calcCol = CALC_COLS - 1; _calcRow = (_calcRow + CALC_ROWS - 1) % CALC_ROWS; }
    else _calcCol--;
  }
  // Select moves to next row (so Fwd/Bwd = within row, Select = press + advance row)
  if (inputJustPressed(BTN_SEL)) {
    buzzerPip(NOTE_SEL);
    char key = pgm_read_byte(&CALC_KEYS[_calcRow][_calcCol]);
    if (key == '=') {
      if (_exprLen > 0) {
        _calcError = !calcEval(_expr, _result, sizeof(_result));
      }
    } else if (key == 'C') {
      calcInit();
    } else if (key == '\b') {
      if (_exprLen > 0) { _expr[--_exprLen] = '\0'; _calcError = false; }
    } else {
      if (_exprLen < EXPR_BUF) { _expr[_exprLen++] = key; _expr[_exprLen] = '\0'; }
    }
    // Advance to next key automatically
    _calcCol = (_calcCol + 1) % CALC_COLS;
    if (_calcCol == 0) _calcRow = (_calcRow + 1) % CALC_ROWS;
  }
  // BWD from row 0 = exit
  if (inputJustPressed(BTN_BWD) && _calcRow == 0 && _calcCol == 0) {
    buzzerPip(NOTE_NAV);
    return true;
  }

  // ── Render ──
  d->clearDisplay();

  // Title bar
  d->fillRect(0, 0, 128, 9, SSD1306_WHITE);
  d->setTextColor(SSD1306_BLACK);
  d->setTextSize(1);
  d->setCursor(30, 1);
  d->print(F("Calculator"));
  d->setTextColor(SSD1306_WHITE);

  // Expression display
  d->drawRect(0, 10, 128, 12, SSD1306_WHITE);
  d->setCursor(2, 12);
  d->setTextSize(1);
  if (_result[0] != '\0') {
    d->print(_calcError ? F("ERR") : F("= "));
    d->print(_result);
  } else {
    d->print(_expr[0] ? _expr : "0");
  }

  // Key grid  (5 rows × 4 cols, y starts at 24)
  #define KEY_W  30
  #define KEY_H   8
  for (uint8_t r = 0; r < CALC_ROWS; r++) {
    for (uint8_t c = 0; c < CALC_COLS; c++) {
      int16_t kx = 1 + c * (KEY_W + 1);
      int16_t ky = 24 + r * (KEY_H + 1);
      bool sel = (r == _calcRow && c == _calcCol);
      if (sel) d->fillRoundRect(kx, ky, KEY_W, KEY_H, 1, SSD1306_WHITE);
      else     d->drawRoundRect(kx, ky, KEY_W, KEY_H, 1, SSD1306_WHITE);
      d->setTextColor(sel ? SSD1306_BLACK : SSD1306_WHITE);
      char key = pgm_read_byte(&CALC_KEYS[r][c]);
      char ks[2] = { key == '\b' ? '<' : key, '\0' };
      d->setCursor(kx + 11, ky + 1);
      d->print(ks);
    }
  }
  d->setTextColor(SSD1306_WHITE);

  d->display();
  return false;
}

// ──────────────────────────────────────────────
//  ALARM
// ──────────────────────────────────────────────
static bool     _alarmOn       = false;
static bool     _alarmRinging  = false;
static uint32_t _alarmToggleMs = 0;
static uint32_t _alarmBuzzMs   = 0;
static bool     _alarmBuzzPhase = false;

void alarmInit() {
  _alarmOn      = (EEPROM.read(EEPROM_ALARM_ADDR) == 1);
  _alarmRinging = _alarmOn;   // if was saved ON, start ringing
  _alarmToggleMs = 0;
  _alarmBuzzMs   = 0;
  _alarmBuzzPhase = false;
}

bool alarmUpdate(uint32_t now) {
  Adafruit_SSD1306* d = getDisplay();

  // Exit
  if (inputJustPressed(BTN_BWD)) {
    buzzerPip(NOTE_NAV);
    noTone(PIN_BUZZER);
    return true;
  }

  // Toggle alarm on Select
  if (inputJustPressed(BTN_SEL)) {
    _alarmOn = !_alarmOn;
    EEPROM.write(EEPROM_ALARM_ADDR, _alarmOn ? 1 : 0);
    _alarmRinging = _alarmOn;
    if (!_alarmOn) noTone(PIN_BUZZER);
    buzzerPip(NOTE_SEL);
  }

  // Buzzer pattern (non-blocking): beep 150ms ON / 200ms OFF / 150ms ON / 600ms pause
  if (_alarmRinging) {
    if (!_alarmBuzzPhase) {
      // ON phase
      if (now - _alarmBuzzMs > 600) {
        tone(PIN_BUZZER, 880);
        _alarmBuzzMs   = now;
        _alarmBuzzPhase = true;
      }
    } else {
      if (now - _alarmBuzzMs > 300) {
        noTone(PIN_BUZZER);
        _alarmBuzzMs   = now;
        _alarmBuzzPhase = false;
      }
    }
  }

  // ── Render ──
  // Flash screen when ringing
  bool flashInv = _alarmRinging && ((now / 300) % 2 == 0);
  d->clearDisplay();
  if (flashInv) d->fillRect(0, 0, 128, 64, SSD1306_WHITE);

  d->setTextColor(flashInv ? SSD1306_BLACK : SSD1306_WHITE);

  // Title
  d->setTextSize(1);
  d->setCursor(40, 4);
  d->print(F("ALARM"));

  // Big bell icon (hand-drawn with primitives)
  int16_t bx = 52, by = 18;
  d->drawCircle(bx + 12, by + 10, 10, flashInv ? SSD1306_BLACK : SSD1306_WHITE);
  d->drawFastHLine(bx + 2, by + 10, 20, flashInv ? SSD1306_BLACK : SSD1306_WHITE);
  d->drawCircle(bx + 12, by + 20, 2,  flashInv ? SSD1306_BLACK : SSD1306_WHITE);
  d->fillTriangle(bx, by + 10, bx + 24, by + 10, bx + 12, by - 2,
                  flashInv ? SSD1306_BLACK : SSD1306_WHITE);

  // Status text
  d->setCursor(28, 44);
  d->print(_alarmOn ? F("ALARM: ON ") : F("ALARM: OFF"));

  // Instructions
  d->setCursor(16, 55);
  d->print(F("[SEL]=toggle [BWD]=back"));

  d->display();
  return false;
}

#endif // APPS_IMPL
#endif // APPS_H
