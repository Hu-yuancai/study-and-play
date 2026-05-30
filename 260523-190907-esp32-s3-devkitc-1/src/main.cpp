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
#include "study_buddy.h"

GameMode currentMode = MODE_MENU;
int menuSelection = 0;
static unsigned long lastMenuOled;

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32-S3 Learning Game Machine ===");

  initAudio();
  playTone(880, 100);  // 启动提示音
  initKeyboard();
  initLCD();
  initOLED();

  drawMenu();
  oledShowWelcome();
  lastMenuOled = millis();
}

void handleMenu(uint8_t key) {
  if (key == KEY_UP) {
    menuSelection = (menuSelection + 2) % 3;
    drawMenu();
  } else if (key == KEY_DOWN) {
    menuSelection = (menuSelection + 1) % 3;
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
        currentMode = MODE_STUDY;
        studyBuddyInit();
        break;
    }
  }
}

void loop() {
  uint8_t key = scanKeyboard();

  switch (currentMode) {
    case MODE_MENU:
      handleMenu(key);
      // 菜单待机: 每800ms切换眨眼表情
      if (millis() - lastMenuOled > 800) {
        oledShowBlinkingFace(FACE_IDLE_A, FACE_IDLE_B, 800);
        lastMenuOled = millis();
      }
      break;
    case MODE_PIANO:
      pianoGameLoop(key);
      break;
    case MODE_PLANE:
      planeGameLoop(key);
      break;
    case MODE_STUDY:
      studyBuddyLoop(key);
      break;
  }

  delay(16);
}

void drawMenu() {
  lcdClear(COLOR_DARKBG);

  // 顶部标题栏
  lcdFillRect(0, 0, LCD_WIDTH, 55, COLOR_NAVY);
  lcdSetTextColor(COLOR_CYAN, COLOR_NAVY);
  lcdDrawString(12, 10, "Learning Game", 2);
  lcdSetTextColor(0x632C, COLOR_NAVY);
  lcdDrawString(12, 32, "ESP32-S3", 1);

  // 三个菜单卡片
  const char* items[]  = {"Piano Game", "Plane Battle", "Study Buddy"};
  const char* descs[]  = {"Rhythm game with 4 keys", "Shoot-em-up arcade", "Pomodoro focus timer"};
  const uint16_t accentColors[] = {COLOR_CYAN, COLOR_GREEN, COLOR_GOLD};

  for (int i = 0; i < 3; i++) {
    bool sel = (menuSelection == i);
    int y = 80 + i * 78;
    uint16_t ac = accentColors[i];

    if (sel) {
      lcdFillRect(12, y - 2, LCD_WIDTH - 24, 68, 0x1082);
      lcdFillRect(12, y - 2, 5, 68, ac);
      lcdDrawRect(12, y - 2, LCD_WIDTH - 24, 68, ac);
      lcdSetTextColor(COLOR_WHITE, 0x1082);
    } else {
      lcdSetTextColor(0x8410, COLOR_DARKBG);
    }
    lcdDrawString(30, y + 4, items[i], 2);
    lcdSetTextColor(sel ? 0x8410 : 0x4208, sel ? 0x1082 : COLOR_DARKBG);
    lcdDrawString(30, y + 28, descs[i], 1);
  }

  // 底部
  lcdSetTextColor(0x4208, COLOR_DARKBG);
  lcdDrawString(40, LCD_HEIGHT - 20, "UP/DOWN  ENTER", 1);

  lcdRefresh();
}
