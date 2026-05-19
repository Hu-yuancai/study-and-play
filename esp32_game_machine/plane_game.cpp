/*
 * plane_game.cpp - 飞机大战游戏实现
 * 经典纵向射击游戏，方向键移动，按键射击
 */
#include "plane_game.h"
#include "game_common.h"
#include "lcd_driver.h"
#include "oled_driver.h"
#include "audio.h"
#include "keyboard.h"

#define MAX_BULLETS   8
#define MAX_ENEMIES   6
#define PLAYER_W      7
#define PLAYER_H      8
#define BULLET_SPEED  4
#define ENEMY_SPEED   1
#define PLAYER_SPEED  3

struct Bullet {
  int x, y;
  bool active;
};

struct Enemy {
  int x, y;
  int w, h;
  int hp;
  bool active;
};

static int playerX, playerY;
static int lives;
static int score;
static bool gameOver;
static Bullet bullets[MAX_BULLETS];
static Enemy enemies[MAX_ENEMIES];
static unsigned long lastFrame;
static unsigned long lastEnemySpawn;
static unsigned long enemySpawnInterval;
static int level;
static int frameCount;

// 玩家飞机图形 (7x8)
static const uint8_t playerBitmap[] PROGMEM = {
  0x08, 0x1C, 0x1C, 0x3E, 0x7F, 0x7F, 0x36, 0x22
};

void planeGameInit() {
  playerX = LCD_WIDTH / 2 - PLAYER_W / 2;
  playerY = LCD_HEIGHT - PLAYER_H - 2;
  lives = 3;
  score = 0;
  gameOver = false;
  level = 1;
  frameCount = 0;
  enemySpawnInterval = 1500;
  lastFrame = millis();
  lastEnemySpawn = millis();

  for (int i = 0; i < MAX_BULLETS; i++) bullets[i].active = false;
  for (int i = 0; i < MAX_ENEMIES; i++) enemies[i].active = false;

  oledClear();
  oledDrawString(10, 10, "飞机大战");
  oledDrawString(10, 30, "方向键移动");
  oledDrawString(10, 50, "C键射击");
  oledRefresh();
}

static void fireBullet() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) {
      bullets[i].active = true;
      bullets[i].x = playerX + PLAYER_W / 2;
      bullets[i].y = playerY - 2;
      playSFX_fire();
      return;
    }
  }
}

static void spawnEnemy() {
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (!enemies[i].active) {
      enemies[i].active = true;
      enemies[i].w = 6 + random(6);
      enemies[i].h = 6;
      enemies[i].x = random(LCD_WIDTH - enemies[i].w);
      enemies[i].y = -enemies[i].h;
      enemies[i].hp = 1 + level / 3;
      return;
    }
  }
}

static void updateBullets() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      bullets[i].y -= BULLET_SPEED;
      if (bullets[i].y < 0) bullets[i].active = false;
    }
  }
}

static void updateEnemies() {
  int speed = ENEMY_SPEED + level / 2;
  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      enemies[i].y += speed;
      if (enemies[i].y > LCD_HEIGHT) {
        enemies[i].active = false;
        lives--;
        if (lives <= 0) gameOver = true;
      }
    }
  }
}

static void checkCollisions() {
  for (int i = 0; i < MAX_BULLETS; i++) {
    if (!bullets[i].active) continue;
    for (int j = 0; j < MAX_ENEMIES; j++) {
      if (!enemies[j].active) continue;
      if (bullets[i].x >= enemies[j].x &&
          bullets[i].x <= enemies[j].x + enemies[j].w &&
          bullets[i].y >= enemies[j].y &&
          bullets[i].y <= enemies[j].y + enemies[j].h) {
        bullets[i].active = false;
        enemies[j].hp--;
        if (enemies[j].hp <= 0) {
          enemies[j].active = false;
          score += 10 * level;
          playSFX_explode();
        }
        break;
      }
    }
  }

  for (int j = 0; j < MAX_ENEMIES; j++) {
    if (!enemies[j].active) continue;
    if (enemies[j].x < playerX + PLAYER_W &&
        enemies[j].x + enemies[j].w > playerX &&
        enemies[j].y < playerY + PLAYER_H &&
        enemies[j].y + enemies[j].h > playerY) {
      enemies[j].active = false;
      lives--;
      if (lives <= 0) gameOver = true;
    }
  }
}

static void handleInput(uint8_t key) {
  if (key == KEY_LEFT  && playerX > 0)                playerX -= PLAYER_SPEED;
  if (key == KEY_RIGHT && playerX < LCD_WIDTH - PLAYER_W) playerX += PLAYER_SPEED;
  if (key == KEY_UP    && playerY > 0)                playerY -= PLAYER_SPEED;
  if (key == KEY_DOWN  && playerY < LCD_HEIGHT - PLAYER_H) playerY += PLAYER_SPEED;
  if (key == KEY_FIRE || key == KEY_ENTER) fireBullet();

  if (isKeyPressed(KEY_LEFT)  && playerX > 0)                playerX -= 1;
  if (isKeyPressed(KEY_RIGHT) && playerX < LCD_WIDTH - PLAYER_W) playerX += 1;
}

static void render() {
  lcdClear();

  lcdDrawBitmap(playerX, playerY, PLAYER_W, PLAYER_H, playerBitmap);

  for (int i = 0; i < MAX_BULLETS; i++) {
    if (bullets[i].active) {
      lcdDrawLine(bullets[i].x, bullets[i].y, bullets[i].x, bullets[i].y + 3);
    }
  }

  for (int i = 0; i < MAX_ENEMIES; i++) {
    if (enemies[i].active) {
      lcdFillRect(enemies[i].x, enemies[i].y, enemies[i].w, enemies[i].h);
    }
  }

  char buf[16];
  snprintf(buf, sizeof(buf), "L:%d S:%d", lives, score);
  lcdDrawString(0, 0, buf);

  lcdRefresh();
}

static void updateOLED() {
  oledClear();
  char buf[32];
  snprintf(buf, sizeof(buf), "得分: %d", score);
  oledDrawString(10, 10, buf);
  snprintf(buf, sizeof(buf), "生命: %d", lives);
  oledDrawString(10, 30, buf);
  snprintf(buf, sizeof(buf), "等级: %d", level);
  oledDrawString(10, 50, buf);
  oledRefresh();
}

void planeGameLoop(uint8_t key) {
  if (key == KEY_BACK) {
    currentMode = MODE_MENU;
    drawMenu();
    oledShowWelcome();
    return;
  }

  if (gameOver) {
    lcdClear();
    lcdDrawString(20, 16, "游戏结束!");
    char buf[32];
    snprintf(buf, sizeof(buf), "最终得分: %d", score);
    lcdDrawString(10, 36, buf);
    lcdDrawString(10, 52, "按确认重新开始");
    lcdRefresh();
    oledShowFace(FACE_SAD);
    if (key == KEY_ENTER) planeGameInit();
    return;
  }

  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();
  frameCount++;

  handleInput(key);
  updateBullets();
  updateEnemies();
  checkCollisions();

  if (millis() - lastEnemySpawn > enemySpawnInterval) {
    spawnEnemy();
    lastEnemySpawn = millis();
  }

  if (frameCount % 600 == 0) {
    level++;
    enemySpawnInterval = max(400UL, enemySpawnInterval - 200);
  }

  render();

  if (frameCount % 30 == 0) {
    updateOLED();
  }
}
