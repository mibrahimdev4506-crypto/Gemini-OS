/*
 * game.h  —  Space Shooter (non-blocking, bounding-box collision)
 * ============================================================
 *  Player ship at bottom, enemies march across top rows.
 *  FWD/BWD = move left/right, SEL = fire.
 *  Increasing difficulty: enemy speed ramps with each wave.
 *  Score display + Game Over screen.
 *  All game logic fits within 2KB SRAM budget.
 */

#ifndef GAME_H
#define GAME_H

#include <Arduino.h>
#include "input.h"
#include "ui.h"

// ──────────────────────────────────────────────
//  API
// ──────────────────────────────────────────────
void gameInit();
bool gameUpdate(uint32_t now);   // true → exit to menu

// ══════════════════════════════════════════════
#ifdef GAME_IMPL

// ── Constants ────────────────────────────────
#define PLAYER_W      10
#define PLAYER_H       6
#define BULLET_W       2
#define BULLET_H       4
#define BULLET_SPEED   4    // pixels per frame

#define ENEMY_W        8
#define ENEMY_H        6
#define ENEMY_COLS     6
#define ENEMY_ROWS     2
#define ENEMY_COUNT   (ENEMY_COLS * ENEMY_ROWS)

#define SCREEN_W_G   128
#define SCREEN_H_G    64

// ── Player ────────────────────────────────────
static int16_t _px = 60;    // player X (left edge)
static int16_t _py = 56;    // player Y (top edge)
#define PLAYER_SPEED 3

// ── Bullets (pool of 3) ───────────────────────
#define BULLET_POOL 3
struct Bullet { int16_t x, y; bool active; };
static Bullet _bullets[BULLET_POOL];

// ── Enemies ───────────────────────────────────
struct Enemy { int16_t x, y; bool alive; };
static Enemy   _enemies[ENEMY_COUNT];
static int8_t  _enemyDX      = 1;    // current move direction
static uint32_t _enemyMoveMs = 0;
static uint16_t _enemyInterval = 350; // ms between enemy steps (decreases)
static uint8_t  _wave          = 0;

// ── Game state ────────────────────────────────
static uint16_t _score       = 0;
static bool     _gameOver    = false;
static uint32_t _fireMs      = 0;
#define AUTO_FIRE_INTERVAL 600  // ms between auto-fires

// ── Helpers ──────────────────────────────────
static bool bbCollide(int16_t ax, int16_t ay, int16_t aw, int16_t ah,
                      int16_t bx, int16_t by, int16_t bw, int16_t bh) {
  return !(ax + aw <= bx || bx + bw <= ax || ay + ah <= by || by + bh <= ay);
}

static void spawnBullet() {
  for (uint8_t i = 0; i < BULLET_POOL; i++) {
    if (!_bullets[i].active) {
      _bullets[i].x      = _px + PLAYER_W / 2 - BULLET_W / 2;
      _bullets[i].y      = _py - BULLET_H;
      _bullets[i].active = true;
      return;
    }
  }
}

static void resetEnemies() {
  uint8_t idx = 0;
  uint16_t startX = 4;
  uint16_t stepX  = (SCREEN_W_G - 8) / ENEMY_COLS;
  for (uint8_t r = 0; r < ENEMY_ROWS; r++) {
    for (uint8_t c = 0; c < ENEMY_COLS; c++) {
      _enemies[idx].x     = startX + c * stepX;
      _enemies[idx].y     = 10 + r * 10;
      _enemies[idx].alive = true;
      idx++;
    }
  }
  _enemyDX = 1;
}

void gameInit() {
  _px            = 60;
  _score         = 0;
  _gameOver      = false;
  _wave          = 0;
  _enemyInterval = 350;
  _fireMs        = 0;

  for (uint8_t i = 0; i < BULLET_POOL; i++) _bullets[i].active = false;
  resetEnemies();
}

bool gameUpdate(uint32_t now) {
  Adafruit_SSD1306* d = getDisplay();

  // ── Game Over screen ──
  if (_gameOver) {
    d->clearDisplay();
    d->setTextSize(2);
    d->setCursor(14, 10);
    d->print(F("GAME OVER"));
    d->setTextSize(1);
    d->setCursor(30, 34);
    d->print(F("Score: "));
    d->print(_score);
    d->setCursor(8, 48);
    d->print(F("[SEL]=Retry [BWD]=Menu"));
    d->display();

    if (inputJustPressed(BTN_SEL)) { buzzerPip(NOTE_SEL); gameInit(); return false; }
    if (inputJustPressed(BTN_BWD)) { buzzerPip(NOTE_NAV);  return true; }
    return false;
  }

  // ── Player movement ──
  if (inputHeld(BTN_FWD)) {
    _px += PLAYER_SPEED;
    if (_px + PLAYER_W > SCREEN_W_G) _px = SCREEN_W_G - PLAYER_W;
  }
  if (inputHeld(BTN_BWD)) {
    _px -= PLAYER_SPEED;
    if (_px < 0) _px = 0;
  }

  // ── Fire (manual + auto) ──
  bool fire = inputJustPressed(BTN_SEL) ||
              (now - _fireMs > AUTO_FIRE_INTERVAL);
  if (fire) {
    spawnBullet();
    _fireMs = now;
    if (inputJustPressed(BTN_SEL)) buzzerPip(NOTE_NAV, 20);
  }

  // ── Bullet movement ──
  for (uint8_t i = 0; i < BULLET_POOL; i++) {
    if (!_bullets[i].active) continue;
    _bullets[i].y -= BULLET_SPEED;
    if (_bullets[i].y < 0) { _bullets[i].active = false; continue; }

    // Bullet-enemy collision
    for (uint8_t e = 0; e < ENEMY_COUNT; e++) {
      if (!_enemies[e].alive) continue;
      if (bbCollide(_bullets[i].x, _bullets[i].y, BULLET_W, BULLET_H,
                    _enemies[e].x,  _enemies[e].y,  ENEMY_W,  ENEMY_H)) {
        _enemies[e].alive   = false;
        _bullets[i].active  = false;
        _score++;
        buzzerPip(NOTE_SEL, 25);
        break;
      }
    }
  }

  // ── Enemy movement ──
  if (now - _enemyMoveMs > _enemyInterval) {
    _enemyMoveMs = now;
    bool hitWall = false;

    for (uint8_t e = 0; e < ENEMY_COUNT; e++) {
      if (!_enemies[e].alive) continue;
      _enemies[e].x += _enemyDX * 2;
      if (_enemies[e].x <= 0 || _enemies[e].x + ENEMY_W >= SCREEN_W_G)
        hitWall = true;
    }
    if (hitWall) {
      _enemyDX = -_enemyDX;
      for (uint8_t e = 0; e < ENEMY_COUNT; e++) {
        if (_enemies[e].alive) _enemies[e].y += 3;
      }
    }

    // Check if enemy reached player
    for (uint8_t e = 0; e < ENEMY_COUNT; e++) {
      if (_enemies[e].alive && _enemies[e].y + ENEMY_H >= _py) {
        _gameOver = true;
        tone(PIN_BUZZER, 200, 800);
        return false;
      }
    }
  }

  // ── Check wave cleared ──
  uint8_t alive = 0;
  for (uint8_t e = 0; e < ENEMY_COUNT; e++) if (_enemies[e].alive) alive++;
  if (alive == 0) {
    _wave++;
    if (_enemyInterval > 80) _enemyInterval -= 40;
    resetEnemies();
    buzzerPip(880, 100);
  }

  // ── Render ──
  d->clearDisplay();

  // HUD
  d->setTextSize(1);
  d->setCursor(0, 0);
  d->print(F("Score:"));
  d->print(_score);
  d->setCursor(84, 0);
  d->print(F("Wave:"));
  d->print(_wave + 1);

  // Divider
  d->drawFastHLine(0, 8, 128, SSD1306_WHITE);

  // Enemies
  for (uint8_t e = 0; e < ENEMY_COUNT; e++) {
    if (!_enemies[e].alive) continue;
    // Draw enemy ship (invader shape)
    int16_t ex = _enemies[e].x;
    int16_t ey = _enemies[e].y;
    d->drawRect(ex + 2, ey, 4, 4, SSD1306_WHITE);
    d->drawFastHLine(ex, ey + 2, ENEMY_W, SSD1306_WHITE);
    d->drawPixel(ex, ey + 5, SSD1306_WHITE);
    d->drawPixel(ex + ENEMY_W - 1, ey + 5, SSD1306_WHITE);
  }

  // Bullets
  for (uint8_t i = 0; i < BULLET_POOL; i++) {
    if (!_bullets[i].active) continue;
    d->fillRect(_bullets[i].x, _bullets[i].y, BULLET_W, BULLET_H, SSD1306_WHITE);
  }

  // Player ship (arrow shape)
  d->fillTriangle(_px + PLAYER_W / 2, _py,
                  _px,                _py + PLAYER_H,
                  _px + PLAYER_W,     _py + PLAYER_H,
                  SSD1306_WHITE);
  d->drawFastHLine(_px + 2, _py + PLAYER_H, PLAYER_W - 4, SSD1306_BLACK);

  d->display();
  return false;
}

#endif // GAME_IMPL
#endif // GAME_H
