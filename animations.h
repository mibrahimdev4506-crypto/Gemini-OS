/*
 * animations.h  —  Eye Animation Engine
 * ============================================================
 *  Frame-based, millis()-driven animated robot eyes.
 *  Behaviors: blink, look left/right, idle drift, wink.
 *  Smooth lerp-style transitions (integer arithmetic only).
 *  Fully non-blocking.
 */

#ifndef ANIMATIONS_H
#define ANIMATIONS_H

#include <Arduino.h>
#include "input.h"
#include "ui.h"

// ──────────────────────────────────────────────
//  Eye parameters
// ──────────────────────────────────────────────
#define EYE_W       28     // eye width
#define EYE_H       22     // eye height open
#define EYE_RADIUS   6     // corner radius
#define EYE_GAP      8     // gap between eyes (centre separation = EYE_W + EYE_GAP)

// Eye centres (X)
#define EYE_L_CX    38     // left eye centre X
#define EYE_R_CX    90     // right eye centre X
#define EYE_CY      32     // eye centre Y (screen)

// Pupil
#define PUPIL_R      4

// ──────────────────────────────────────────────
//  API
// ──────────────────────────────────────────────
void eyesInit();
bool eyesUpdate(uint32_t now);   // true → exit to menu

// ══════════════════════════════════════════════
#ifdef ANIM_IMPL

// ── Eye state ────────────────────────────────
enum EyeBehavior : uint8_t {
  EYE_IDLE = 0,
  EYE_BLINK,
  EYE_LOOK_LEFT,
  EYE_LOOK_RIGHT,
  EYE_WINK_L,
  EYE_WINK_R
};

static EyeBehavior _eyeBeh        = EYE_IDLE;
static uint32_t    _eyeNextEvent  = 0;
static uint32_t    _eyeEventMs    = 0;   // when current behavior started

// Blink / wink height (0=open, EYE_H=fully closed)
static int8_t  _blinkH       = 0;   // current blink amount (pixels)
static int8_t  _blinkTarget  = 0;

// Pupil position (relative to eye centre, range ±7)
static int8_t  _pupilX     = 0;
static int8_t  _pupilY     = 0;
static int8_t  _pupilTgtX  = 0;
static int8_t  _pupilTgtY  = 0;

// Idle drift
static uint32_t _driftNextMs = 0;

// Lerp helper (integer, 1/3 each frame)
static int8_t lerpB(int8_t cur, int8_t tgt) {
  if (cur == tgt) return tgt;
  int8_t d = tgt - cur;
  if (abs(d) <= 1) return tgt;
  return cur + d / 3;
}

// Random within range (avr rand)
static int8_t randRange(int8_t lo, int8_t hi) {
  return lo + (int8_t)(random(hi - lo + 1));
}

void eyesInit() {
  _eyeBeh       = EYE_IDLE;
  _eyeNextEvent = millis() + 1200;
  _blinkH       = 0;
  _blinkTarget  = 0;
  _pupilX = _pupilY = _pupilTgtX = _pupilTgtY = 0;
  _driftNextMs  = millis() + 800;
}

// ── Draw one eye ─────────────────────────────
static void drawEye(Adafruit_SSD1306* d,
                    int16_t cx, int16_t cy,
                    int8_t blinkAmt,   // 0=open, EYE_H/2=half, EYE_H=closed
                    int8_t px, int8_t py,  // pupil offset from centre
                    bool isLeft)
{
  int16_t ex = cx - EYE_W / 2;
  int16_t ey = cy - EYE_H / 2;

  // Clamp blink
  if (blinkAmt < 0)      blinkAmt = 0;
  if (blinkAmt > EYE_H)  blinkAmt = EYE_H;

  int16_t openH = EYE_H - blinkAmt;
  if (openH < 2) openH = 2;

  int16_t eyY = cy - openH / 2;

  // Eye outline
  d->drawRoundRect(ex, eyY, EYE_W, openH, EYE_RADIUS, SSD1306_WHITE);

  // Pupil (only if open enough)
  if (openH > 8) {
    int16_t pcx = cx + px;
    int16_t pcy = cy + py;
    // Clamp pupil inside eye
    int16_t pxMin = ex + PUPIL_R + 2;
    int16_t pxMax = ex + EYE_W - PUPIL_R - 2;
    int16_t pyMin = eyY + PUPIL_R + 1;
    int16_t pyMax = eyY + openH - PUPIL_R - 1;
    if (pcx < pxMin) pcx = pxMin;
    if (pcx > pxMax) pcx = pxMax;
    if (pcy < pyMin) pcy = pyMin;
    if (pcy > pyMax) pcy = pyMax;
    d->fillCircle(pcx, pcy, PUPIL_R, SSD1306_WHITE);
    // Pupil highlight
    d->fillCircle(pcx - 1, pcy - 1, 1, SSD1306_BLACK);
  }

  // Eyelid top (fills from top of eye box down by blinkAmt/2)
  if (blinkAmt > 0) {
    int16_t lidH = blinkAmt / 2 + 1;
    d->fillRect(ex + 1, eyY, EYE_W - 2, lidH, SSD1306_BLACK);
    // Redraw top border
    d->drawFastHLine(ex + 1, eyY, EYE_W - 2, SSD1306_WHITE);
  }
}

bool eyesUpdate(uint32_t now) {
  Adafruit_SSD1306* d = getDisplay();

  // ── Exit ──
  if (inputJustPressed(BTN_BWD) || inputJustPressed(BTN_SEL)) {
    buzzerPip(NOTE_NAV);
    return true;
  }

  // ── Behavior state machine ──
  switch (_eyeBeh) {
    case EYE_IDLE:
      // Drift pupils slowly
      if (now > _driftNextMs) {
        _pupilTgtX   = randRange(-5, 5);
        _pupilTgtY   = randRange(-3, 3);
        _driftNextMs = now + random(600, 1400);
      }
      if (now > _eyeNextEvent) {
        // Pick next behavior randomly
        uint8_t r = random(10);
        if      (r < 4) { _eyeBeh = EYE_BLINK;      _blinkTarget = EYE_H; }
        else if (r < 6) { _eyeBeh = EYE_LOOK_LEFT;  _pupilTgtX = -7; _pupilTgtY = 0; }
        else if (r < 8) { _eyeBeh = EYE_LOOK_RIGHT; _pupilTgtX =  7; _pupilTgtY = 0; }
        else if (r < 9) { _eyeBeh = EYE_WINK_L;     _blinkTarget = EYE_H; }
        else            { _eyeBeh = EYE_WINK_R;     _blinkTarget = EYE_H; }
        _eyeEventMs  = now;
        _eyeNextEvent = now + random(1200, 3000);
      }
      break;

    case EYE_BLINK:
    case EYE_WINK_L:
    case EYE_WINK_R: {
      uint32_t elapsed = now - _eyeEventMs;
      if (elapsed < 80)        _blinkTarget = EYE_H;      // closing
      else if (elapsed < 160)  _blinkTarget = 0;          // opening
      else {
        _blinkTarget = 0;
        _blinkH      = 0;
        _eyeBeh      = EYE_IDLE;
      }
      break;
    }

    case EYE_LOOK_LEFT:
    case EYE_LOOK_RIGHT: {
      uint32_t elapsed = now - _eyeEventMs;
      if (elapsed > 900) {
        _pupilTgtX = 0; _pupilTgtY = 0;
        _eyeBeh    = EYE_IDLE;
      }
      break;
    }
  }

  // ── Smooth lerp pupil ──
  _pupilX = lerpB(_pupilX, _pupilTgtX);
  _pupilY = lerpB(_pupilY, _pupilTgtY);
  _blinkH = lerpB(_blinkH, _blinkTarget);

  // ── Render ──
  d->clearDisplay();

  // Label
  d->setTextSize(1);
  d->setCursor(34, 56);
  d->setTextColor(SSD1306_WHITE);
  d->print(F("[BWD] = Back"));

  // Left eye
  int8_t leftBlink  = (_eyeBeh == EYE_WINK_L || (_eyeBeh == EYE_BLINK)) ? _blinkH : 0;
  int8_t rightBlink = (_eyeBeh == EYE_WINK_R || (_eyeBeh == EYE_BLINK)) ? _blinkH : 0;
  if (_eyeBeh == EYE_BLINK) { leftBlink = _blinkH; rightBlink = _blinkH; }

  drawEye(d, EYE_L_CX, EYE_CY, leftBlink,  _pupilX, _pupilY, true);
  drawEye(d, EYE_R_CX, EYE_CY, rightBlink, _pupilX, _pupilY, false);

  d->display();
  return false;
}

#endif // ANIM_IMPL
#endif // ANIMATIONS_H
