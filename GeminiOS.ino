/*
 * ============================================================
 *  GEMINI OS  —  v1.0
 *  Arduino Uno / Nano  |  128×64 SSD1306 OLED (I2C)
 * ============================================================
 *  Wiring:
 *    OLED  SDA → A4   SCL → A5
 *    D2  Power button (INT0 wake/sleep)
 *    D3  Forward button
 *    D4  Backward button
 *    D10 Select button
 *    D9  Buzzer
 *
 *  Libraries required (install via Library Manager):
 *    Adafruit SSD1306
 *    Adafruit GFX Library
 *    Adafruit BusIO  (dependency)
 *
 *  Architecture:
 *    HAL  →  input.h      (debounce state machine + buzzer)
 *    UI   →  ui.h         (boot, menu, sysinfo, components)
 *    Apps →  apps.h       (calculator, alarm + EEPROM)
 *    Anim →  animations.h (eye engine)
 *    Game →  game.h       (space shooter)
 *
 *  All modules are header-only (single-translation-unit build).
 *  Define *_IMPL before #include to activate the implementation.
 * ============================================================
 */

// ── Activate implementations ───────────────────────────────
#define INPUT_IMPL
#define UI_IMPL
#define APPS_IMPL
#define ANIM_IMPL
#define GAME_IMPL

#include <avr/sleep.h>
#include <avr/power.h>
#include <EEPROM.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "input.h"
#include "ui.h"
#include "apps.h"
#include "animations.h"
#include "game.h"

// ──────────────────────────────────────────────
//  Display singleton
// ──────────────────────────────────────────────
#define SCREEN_W   128
#define SCREEN_H    64
#define OLED_RESET  -1
Adafruit_SSD1306 display(SCREEN_W, SCREEN_H, &Wire, OLED_RESET);

// ──────────────────────────────────────────────
//  OS state machine
// ──────────────────────────────────────────────
enum OSState : uint8_t {
  OS_BOOT = 0,
  OS_MENU,
  OS_CALCULATOR,
  OS_GAME,
  OS_EYES,
  OS_ALARM,
  OS_SYSINFO,
  OS_SLEEP
};

static OSState  osState     = OS_BOOT;
static uint32_t lastFrameMs = 0;
static uint32_t loopStartUs = 0;
static uint16_t loopDurUs   = 0;

const uint16_t FRAME_MS = 33;   // ~30 FPS

// ──────────────────────────────────────────────
//  INT0 wake ISR
// ──────────────────────────────────────────────
volatile bool wakeRequest = false;
void powerISR() { wakeRequest = true; }

// ──────────────────────────────────────────────
//  Sleep / Wake helpers
// ──────────────────────────────────────────────
static void enterSleep() {
  display.clearDisplay();
  display.display();
  delay(30);   // flush I2C before power-down

  attachInterrupt(digitalPinToInterrupt(PIN_BTN_PWR), powerISR, LOW);
  set_sleep_mode(SLEEP_MODE_PWR_DOWN);
  sleep_enable();
  sleep_mode();           // ← MCU halts here until INT0 fires

  // -- Woken up --
  sleep_disable();
  detachInterrupt(digitalPinToInterrupt(PIN_BTN_PWR));
  wakeRequest = false;

  // Re-init display (I2C bus was idle during sleep)
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.clearDisplay();
  display.display();

  osState = OS_BOOT;
  bootInit();
}

// ──────────────────────────────────────────────
//  Global accessors (used by modules)
// ──────────────────────────────────────────────
Adafruit_SSD1306* getDisplay() { return &display; }
uint16_t          getLoopUs()  { return loopDurUs; }

// ──────────────────────────────────────────────
//  setup()
// ──────────────────────────────────────────────
void setup() {
  pinMode(PIN_BTN_FWD, INPUT_PULLUP);
  pinMode(PIN_BTN_BWD, INPUT_PULLUP);
  pinMode(PIN_BTN_SEL, INPUT_PULLUP);
  pinMode(PIN_BTN_PWR, INPUT_PULLUP);
  pinMode(PIN_BUZZER,  OUTPUT);

  randomSeed(analogRead(A0));   // seed RNG from floating pin

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    // OLED init failed — minimal SOS
    for (uint8_t i = 0; i < 6; i++) { tone(PIN_BUZZER, 440, 150); delay(300); }
  }
  display.setTextColor(SSD1306_WHITE);
  display.clearDisplay();
  display.display();

  inputInit();
  uiInit(&display);
  bootInit();
}

// ──────────────────────────────────────────────
//  loop()
// ──────────────────────────────────────────────
void loop() {
  loopStartUs      = micros();
  uint32_t now     = millis();

  // Input polling runs every loop iteration for accuracy
  inputUpdate(now);

  // Power button → sleep (any state except boot)
  if (osState != OS_BOOT && inputJustPressed(BTN_PWR)) {
    buzzerPip(NOTE_WARN, 60);
    enterSleep();
    return;
  }

  // Frame rate gate
  if ((uint32_t)(now - lastFrameMs) < FRAME_MS) {
    loopDurUs = (uint16_t)(micros() - loopStartUs);
    return;
  }
  lastFrameMs = now;

  // ── State machine ──────────────────────────
  switch (osState) {

    case OS_BOOT:
      bootUpdate(now);
      if (bootDone()) {
        osState = OS_MENU;
        menuInit();
        buzzerPip(NOTE_SEL, 80);
      }
      break;

    case OS_MENU: {
      int8_t sel = menuUpdate(now);
      if (sel >= 0) {
        buzzerPip(NOTE_SEL);
        switch (sel) {
          case 0: osState = OS_CALCULATOR; calcInit();  break;
          case 1: osState = OS_GAME;       gameInit();  break;
          case 2: osState = OS_EYES;       eyesInit();  break;
          case 3: osState = OS_ALARM;      alarmInit(); break;
          case 4: osState = OS_SYSINFO;                 break;
        }
      }
      break;
    }

    case OS_CALCULATOR:
      if (calcUpdate(now))    { osState = OS_MENU; menuInit(); } break;

    case OS_GAME:
      if (gameUpdate(now))    { osState = OS_MENU; menuInit(); } break;

    case OS_EYES:
      if (eyesUpdate(now))    { osState = OS_MENU; menuInit(); } break;

    case OS_ALARM:
      if (alarmUpdate(now))   { osState = OS_MENU; menuInit(); } break;

    case OS_SYSINFO:
      if (sysinfoUpdate(now)) { osState = OS_MENU; menuInit(); } break;

    default: break;
  }

  loopDurUs = (uint16_t)(micros() - loopStartUs);
}
