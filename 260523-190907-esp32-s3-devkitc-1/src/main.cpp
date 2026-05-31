/*
 * ESP32-S3 学习游戏一体机 - 主程序
 * 硬件: ESP32-S3-DevKitC-1 + LCD12864(ST7920) + OLED(SSD1306) + 4x4键盘 + LM386功放
 * 功能: 钢琴游戏 / 飞机大战 / 学习伴侣
 */

#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <SPI.h>
#include "config.h"
#include "game_common.h"
#include "audio.h"
#include "keyboard.h"
#include "lcd_driver.h"
#include "oled_driver.h"
#include "piano_game.h"
#include "plane_game.h"
#include "minesweeper.h"
#include "snake_game.h"
#include "tetris_game.h"
#include "game2048.h"
#include "study_buddy.h"
#include "ghost_shell.h"
#include "safe_ram.h"

GameMode currentMode = MODE_MENU;
int menuSelection = 0;
static unsigned long lastMenuOled;

void setup() {
  Serial.begin(115200);
  // 原生USB(CDC)需等待枚举就绪, 否则最早的日志会丢失; 最多等2秒
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 2000) { delay(10); }
  delay(300);
  Serial.println();
  Serial.println("=== ESP32-S3 Learning Game Machine ===");

  safeRAMInit();   // Ghost Shell 沙盒内存 + 扫雷参数默认值
  Serial.println("[1] initAudio...");
  initAudio();
  // 不播开机提示音: 上电瞬间供电最紧张, 避免叠加蜂鸣器电流引发欠压
  Serial.println("[2] initKeyboard...");
  initKeyboard();
  Serial.println("[3] initLCD...");
  initLCD();
  Serial.println("[4] initOLED...");
  initOLED();
  Serial.println("[5] drawMenu...");
  drawMenu();
  oledShowWelcome();
  lastMenuOled = millis();
  Serial.println("[6] setup done, entering loop");
}

void handleMenu(uint8_t key) {
  // ── Konami 码: ↑↑↓↓←→←→ FIRE ENTER → 进入隐藏 Ghost Shell ──
  static const uint8_t konami[] = {KEY_UP, KEY_UP, KEY_DOWN, KEY_DOWN,
    KEY_LEFT, KEY_RIGHT, KEY_LEFT, KEY_RIGHT, KEY_FIRE, KEY_ENTER};
  static int konamiPos = 0;
  if (key != KEY_NONE) {
    if (key == konami[konamiPos]) {
      konamiPos++;
      if (konamiPos == 10) {
        konamiPos = 0;
        playTone(880, 40); delay(50);
        playTone(1100, 40); delay(50);
        playTone(1400, 60);
        currentMode = MODE_SHELL;
        shellInit();
        return;
      }
    } else {
      konamiPos = (key == konami[0]) ? 1 : 0;
    }
  }

  if (key == KEY_UP) {
    menuSelection = (menuSelection + 6) % 7;
    drawMenu();
  } else if (key == KEY_DOWN) {
    menuSelection = (menuSelection + 1) % 7;
    drawMenu();
  } else if (key == KEY_ENTER) {
    switch (menuSelection) {
      case 0:
        currentMode = MODE_PIANO;
        pianoGameInit();
        break;
      case 1:
        currentMode = MODE_PLANE;
        planeGameInit();
        break;
      case 2:
        currentMode = MODE_MINESWEEPER;
        minesweeperInit();
        break;
      case 3:
        currentMode = MODE_SNAKE;
        snakeGameInit();
        break;
      case 4:
        currentMode = MODE_TETRIS;
        tetrisGameInit();
        break;
      case 5:
        currentMode = MODE_2048;
        game2048Init();
        break;
      case 6:
        currentMode = MODE_STUDY;
        studyBuddyInit();
        break;
    }
  }
}

void loop() {
  uint8_t key = scanKeyboard();

  audioUpdate();  // 推进非阻塞音效

  switch (currentMode) {
    case MODE_MENU:
      handleMenu(key);
      // 菜单待机: 每2秒切换眨眼表情 (降低I2C刷新频率, 减小平均电流)
      if (millis() - lastMenuOled > 2000) {
        oledShowBlinkingFace(FACE_IDLE_A, FACE_IDLE_B, 2000);
        lastMenuOled = millis();
      }
      break;
    case MODE_PIANO:
      pianoGameLoop(key);
      break;
    case MODE_PLANE:
      planeGameLoop(key);
      break;
    case MODE_MINESWEEPER:
      minesweeperLoop(key);
      break;
    case MODE_SNAKE:
      snakeGameLoop(key);
      break;
    case MODE_TETRIS:
      tetrisGameLoop(key);
      break;
    case MODE_2048:
      game2048Loop(key);
      break;
    case MODE_STUDY:
      studyBuddyLoop(key);
      break;
    case MODE_SHELL:
      shellLoop(key);
      break;
  }

  delay(16);
}

void drawMenu() {
  lcdClear(COLOR_DARKBG);

  // 顶部标题栏 (黄底紫字)
  lcdFillRect(0, 0, LCD_WIDTH, 46, COLOR_YELLOW);
  lcdSetTextColor(0x8010, COLOR_YELLOW);
  lcdDrawString(8, 14, "Learning Game", 2);

  // 7 个菜单项 (紧凑列表, 每行 34px)
  const char* items[] = {"Piano Game", "Plane Battle", "Minesweeper",
                         "Snake", "Tetris", "2048", "Study Buddy"};
  const uint16_t accent[] = {COLOR_CYAN, COLOR_GREEN, COLOR_ORANGE,
                             0x07E0, 0xFD20, 0xFFE0, COLOR_GOLD};

  for (int i = 0; i < 7; i++) {
    bool sel = (menuSelection == i);
    int y = 54 + i * 34;            // 54,88,...,258
    int rowH = 30;

    if (sel) {
      lcdFillRect(8, y, LCD_WIDTH - 16, rowH, 0x1082);   // 选中底块
      lcdFillRect(8, y, 5, rowH, accent[i]);              // 左侧强调条
      lcdSetTextColor(0xFE19, 0x1082);
    } else {
      lcdFillRect(8, y, 5, rowH, accent[i]);              // 非选中也显示色条
      lcdSetTextColor(0x9CD3, COLOR_DARKBG);
    }
    lcdDrawString(24, y + 7, items[i], 2);
  }

  // 底部
  lcdSetTextColor(0x4208, COLOR_DARKBG);
  lcdDrawString(35, LCD_HEIGHT - 12, "UP/DOWN  ENTER", 1);

  lcdRefresh();
}
