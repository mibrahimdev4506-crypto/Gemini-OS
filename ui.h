/*
 * ui.h  —  UI Engine  (menus, components, boot, system info)
 * ============================================================
 *  Provides:
 *    - Boot animation (CRT scan + loading bar)
 *    - Vertical scrolling main menu with easing
 *    - Reusable UI components (boxes, inverted text, dialogs)
 *    - System Info screen
 *    - Free SRAM helper
 */

#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <avr/pgmspace.h>

// ── Forward declaration (defined in GeminiOS.ino) ──
Adafruit_SSD1306* getDisplay();
uint16_t          getLoopUs();

// ──────────────────────────────────────────────────
//  PROGMEM  Assets
// ──────────────────────────────────────────────────

// 8×8 mini icons for menu  (PROGMEM)
// Calculator icon
static const uint8_t ICON_CALC[] PROGMEM = {
  0x7E, 0x42, 0x5A, 0x42, 0x5A, 0x42, 0x5A, 0x7E
};
// Game / spaceship icon
static const uint8_t ICON_GAME[] PROGMEM = {
  0x18, 0x3C, 0x7E, 0xFF, 0xDB, 0xFF, 0x24, 0x00
};
// Eye icon
static const uint8_t ICON_EYES[] PROGMEM = {
  0x00, 0x3C, 0x42, 0x81, 0x99, 0x81, 0x42, 0x3C
};
// Alarm / bell icon
static const uint8_t ICON_ALARM[] PROGMEM = {
  0x18, 0x3C, 0x3C, 0x3C, 0x7E, 0xFF, 0x18, 0x00
};
// Chip / system icon
static const uint8_t ICON_SYS[] PROGMEM = {
  0x3C, 0x5A, 0xA5, 0xBD, 0xBD, 0xA5, 0x5A, 0x3C
};

// Menu item labels stored in PROGMEM
static const char MENU_L0[] PROGMEM = "Calculator";
static const char MENU_L1[] PROGMEM = "Shoot Game";
static const char MENU_L2[] PROGMEM = "Eye Anim";
static const char MENU_L3[] PROGMEM = "Alarm";
static const char MENU_L4[] PROGMEM = "System Info";

static const char* const MENU_LABELS[] PROGMEM = {
  MENU_L0, MENU_L1, MENU_L2, MENU_L3, MENU_L4
};

#define MENU_COUNT 5
#define MENU_ITEM_H 12       // pixels per menu row
#define MENU_VISIBLE 4       // rows visible at once

// Boot strings in PROGMEM
static const char BOOT_S0[] PROGMEM = "GEMINI  OS";
static const char BOOT_S1[] PROGMEM = "Initializing...";
static const char BOOT_S2[] PROGMEM = "Checking Memory...";
static const char BOOT_S3[] PROGMEM = "System Ready";

// ──────────────────────────────────────────────────
//  Free SRAM helper
// ──────────────────────────────────────────────────
inline int freeMemory() {
  extern int __heap_start, *__brkval;
  int v;
  return (int)&v - (__brkval == 0 ? (int)&__heap_start : (int)__brkval);
}

// ──────────────────────────────────────────────────
//  Module API
// ──────────────────────────────────────────────────
void uiInit(Adafruit_SSD1306* disp);

// Boot
void    bootInit();
void    bootUpdate(uint32_t now);
bool    bootDone();

// Menu  — returns selected index (0‥4) or -1
void    menuInit();
int8_t  menuUpdate(uint32_t now);

// System info
void    sysinfoInit();
bool    sysinfoUpdate(uint32_t now);  // returns true when user exits

// Low-level UI helpers (used by apps)
void uiDrawBox(int16_t x, int16_t y, int16_t w, int16_t h, bool filled = false);
void uiDrawInvText(int16_t x, int16_t y, const char* str);
void uiDrawIcon(int16_t x, int16_t y, const uint8_t* pgmIcon);
void uiDrawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct);

// ──────────────────────────────────────────────────
//  Implementation
// ──────────────────────────────────────────────────
#ifdef UI_IMPL

static Adafruit_SSD1306* _d = nullptr;

void uiInit(Adafruit_SSD1306* disp) { _d = disp; }

// ── Low-level helpers ─────────────────────────────
void uiDrawBox(int16_t x, int16_t y, int16_t w, int16_t h, bool filled) {
  if (filled) _d->fillRoundRect(x, y, w, h, 3, SSD1306_WHITE);
  else        _d->drawRoundRect(x, y, w, h, 3, SSD1306_WHITE);
}

void uiDrawInvText(int16_t x, int16_t y, const char* str) {
  int16_t tx, ty; uint16_t tw, th;
  _d->getTextBounds(str, x, y, &tx, &ty, &tw, &th);
  _d->fillRect(tx - 1, ty - 1, tw + 2, th + 2, SSD1306_WHITE);
  _d->setTextColor(SSD1306_BLACK);
  _d->setCursor(x, y);
  _d->print(str);
  _d->setTextColor(SSD1306_WHITE);
}

void uiDrawIcon(int16_t x, int16_t y, const uint8_t* pgmIcon) {
  _d->drawBitmap(x, y, pgmIcon, 8, 8, SSD1306_WHITE);
}

void uiDrawProgressBar(int16_t x, int16_t y, int16_t w, int16_t h, uint8_t pct) {
  _d->drawRect(x, y, w, h, SSD1306_WHITE);
  int16_t fill = (int32_t)(w - 2) * pct / 100;
  if (fill > 0) _d->fillRect(x + 1, y + 1, fill, h - 2, SSD1306_WHITE);
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  BOOT
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static uint8_t  _bootPhase    = 0;
static uint32_t _bootPhaseMs  = 0;
static bool     _bootFinished = false;
static uint8_t  _bootScanY    = 0;

void bootInit() {
  _bootPhase    = 0;
  _bootPhaseMs  = millis();
  _bootFinished = false;
  _bootScanY    = 0;
}

bool bootDone() { return _bootFinished; }

// Helper: copy PROGMEM string to stack buffer (max 24 chars)
static void pgm2buf(const char* pgmStr, char* buf, uint8_t sz) {
  strncpy_P(buf, pgmStr, sz - 1);
  buf[sz - 1] = '\0';
}

void bootUpdate(uint32_t now) {
  if (_bootFinished) return;
  _d->clearDisplay();

  switch (_bootPhase) {

    // Phase 0: CRT scan lines (64 scan lines over ~800 ms)
    case 0: {
      uint8_t target = (uint8_t)map(now - _bootPhaseMs, 0, 800, 0, 64);
      if (target > 64) target = 64;
      for (uint8_t y = 0; y < target; y += 2)
        _d->drawFastHLine(0, y, 128, SSD1306_WHITE);
      if (target >= 64) { _bootPhase = 1; _bootPhaseMs = now; }
      break;
    }

    // Phase 1: Title + "Initializing..." + empty bar
    case 1: {
      _d->setTextSize(2);
      _d->setCursor(8, 8);
      _d->print(F("GEMINI OS"));
      _d->setTextSize(1);
      _d->setCursor(20, 32);
      _d->print(F("Initializing..."));
      uiDrawProgressBar(14, 48, 100, 8, 0);
      if (now - _bootPhaseMs > 600) { _bootPhase = 2; _bootPhaseMs = now; }
      break;
    }

    // Phase 2: Loading bar progress + status strings
    case 2: {
      uint32_t elapsed = now - _bootPhaseMs;
      uint8_t pct = (uint8_t)min((uint32_t)100, elapsed * 100 / 1200);

      _d->setTextSize(2);
      _d->setCursor(8, 8);
      _d->print(F("GEMINI OS"));
      _d->setTextSize(1);

      char buf[24];
      if      (pct < 40) pgm2buf(BOOT_S1, buf, sizeof(buf));
      else if (pct < 75) pgm2buf(BOOT_S2, buf, sizeof(buf));
      else               pgm2buf(BOOT_S3, buf, sizeof(buf));

      _d->setCursor(20, 32);
      _d->print(buf);
      uiDrawProgressBar(14, 48, 100, 8, pct);

      if (pct >= 100) { _bootPhase = 3; _bootPhaseMs = now; }
      break;
    }

    // Phase 3: Flash "System Ready" briefly
    case 3: {
      _d->setTextSize(2);
      _d->setCursor(8, 8);
      _d->print(F("GEMINI OS"));
      _d->setTextSize(1);
      _d->setCursor(28, 32);
      _d->print(F("System Ready"));
      uiDrawProgressBar(14, 48, 100, 8, 100);
      if (now - _bootPhaseMs > 700) { _bootFinished = true; }
      break;
    }
  }

  _d->display();
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  MAIN MENU
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
static int8_t  _menuSel    = 0;
static int16_t _menuScrollY = 0;    // current pixel scroll (easing target)
static int16_t _menuTargetY = 0;    // target pixel offset
static int8_t  _menuResult = -1;

static const uint8_t* const MENU_ICONS[] PROGMEM = {
  ICON_CALC, ICON_GAME, ICON_EYES, ICON_ALARM, ICON_SYS
};

void menuInit() {
  _menuSel     = 0;
  _menuScrollY = 0;
  _menuTargetY = 0;
  _menuResult  = -1;
}

// Lerp easing — moves 1/4 of remaining distance each frame
static int16_t lerpI(int16_t cur, int16_t tgt) {
  if (cur == tgt) return tgt;
  int16_t d = tgt - cur;
  if (abs(d) <= 1) return tgt;
  return cur + d / 4;
}

int8_t menuUpdate(uint32_t now) {
  // ── Input ──
  if (inputJustPressed(BTN_FWD)) {
    buzzerPip(NOTE_NAV);
    if (_menuSel < MENU_COUNT - 1) {
      _menuSel++;
      _menuTargetY = _menuSel * MENU_ITEM_H;
    }
  }
  if (inputJustPressed(BTN_BWD)) {
    buzzerPip(NOTE_NAV);
    if (_menuSel > 0) {
      _menuSel--;
      _menuTargetY = _menuSel * MENU_ITEM_H;
    }
  }
  if (inputJustPressed(BTN_SEL)) {
    return _menuSel;
  }

  // ── Easing scroll ──
  _menuScrollY = lerpI(_menuScrollY, _menuTargetY);

  // ── Render ──
  _d->clearDisplay();

  // Title bar
  _d->fillRect(0, 0, 128, 10, SSD1306_WHITE);
  _d->setTextColor(SSD1306_BLACK);
  _d->setTextSize(1);
  _d->setCursor(4, 2);
  _d->print(F("GEMINI OS"));
  _d->setTextColor(SSD1306_WHITE);

  // Clip region for menu items (y: 11 to 63)
  char buf[14];
  for (uint8_t i = 0; i < MENU_COUNT; i++) {
    int16_t itemY = 12 + (int16_t)(i * MENU_ITEM_H) - _menuScrollY;
    if (itemY < 10 || itemY > 63) continue;

    // Highlight selected
    if (i == (uint8_t)_menuSel) {
      _d->fillRoundRect(0, itemY - 1, 128, MENU_ITEM_H, 2, SSD1306_WHITE);
      _d->setTextColor(SSD1306_BLACK);
    } else {
      _d->setTextColor(SSD1306_WHITE);
    }

    // Icon
    const uint8_t* iconPgm = (const uint8_t*)pgm_read_word(&MENU_ICONS[i]);
    uiDrawIcon(2, itemY, iconPgm);

    // Label
    pgm2buf((const char*)pgm_read_word(&MENU_LABELS[i]), buf, sizeof(buf));
    _d->setCursor(14, itemY + 1);
    _d->setTextSize(1);
    _d->print(buf);

    _d->setTextColor(SSD1306_WHITE);
  }

  // Scrollbar
  uint8_t sbH = max(4, (int)(52 / MENU_COUNT));
  uint8_t sbY = 11 + (uint8_t)map(_menuSel, 0, MENU_COUNT - 1, 0, 52 - sbH);
  _d->drawRect(125, 11, 3, 52, SSD1306_WHITE);
  _d->fillRect(126, sbY, 1, sbH, SSD1306_WHITE);

  _d->display();
  return -1;
}

// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
//  SYSTEM INFO
// ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
bool sysinfoUpdate(uint32_t now) {
  if (inputJustPressed(BTN_BWD) || inputJustPressed(BTN_SEL)) {
    buzzerPip(NOTE_NAV);
    return true;
  }

  _d->clearDisplay();

  // Title bar
  _d->fillRect(0, 0, 128, 10, SSD1306_WHITE);
  _d->setTextColor(SSD1306_BLACK);
  _d->setTextSize(1);
  _d->setCursor(22, 2);
  _d->print(F("System Info"));
  _d->setTextColor(SSD1306_WHITE);

  // Free RAM
  int freeSRAM = freeMemory();
  _d->setCursor(2, 14);
  _d->print(F("Free RAM: "));
  _d->print(freeSRAM);
  _d->print(F("B"));

  // RAM bar
  uint8_t ramPct = (uint8_t)((long)freeSRAM * 100 / 2048L);
  uiDrawProgressBar(2, 24, 124, 6, ramPct);

  // Uptime
  uint32_t upSec = now / 1000UL;
  _d->setCursor(2, 34);
  _d->print(F("Uptime: "));
  if (upSec >= 3600) { _d->print(upSec / 3600); _d->print(F("h ")); }
  _d->print((upSec % 3600) / 60); _d->print(F("m "));
  _d->print(upSec % 60);          _d->print(F("s"));

  // CPU load  (loop us / 33000 us frame budget)
  extern uint16_t getLoopUs();
  uint16_t us   = getLoopUs();
  uint8_t  load = (uint8_t)min(100UL, (uint32_t)us * 100 / 33000UL);
  _d->setCursor(2, 44);
  _d->print(F("CPU Load: "));
  _d->print(load);
  _d->print(F("%"));
  uiDrawProgressBar(2, 54, 124, 6, load);

  _d->display();
  return false;
}

#endif // UI_IMPL
#endif // UI_H
