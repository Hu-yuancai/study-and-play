/*
 * ESP32-S3 学习游戏一体机 - 主程序
 * 硬件: ESP32-S3-DevKitC-1 + LCD12864(ST7920) + OLED(SSD1306) + 4x4键盘 + LM386功放
 * 功能: 钢琴游戏 / 飞机大战 / 学习伴侣
 */

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

void setup() {
  Serial.begin(115200);
  Serial.println("=== ESP32-S3 Learning Game Machine ===");

  initAudio();
  initKeyboard();
  initLCD();
  initOLED();

  drawMenu();
  oledShowWelcome();
}

void loop() {
  uint8_t key = scanKeyboard();

  switch (currentMode) {
    case MODE_MENU:
      handleMenu(key);
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

void drawMenu() {
  lcdClear();
  lcdDrawString(0, 0, "=== 学习游戏一体机 ===");
  lcdDrawString(0, 16, menuSelection == 0 ? "> 钢琴游戏" : "  钢琴游戏");
  lcdDrawString(0, 32, menuSelection == 1 ? "> 飞机大战" : "  飞机大战");
  lcdDrawString(0, 48, menuSelection == 2 ? "> 学习伴侣" : "  学习伴侣");
  lcdRefresh();
}
