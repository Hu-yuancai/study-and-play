/*
 * study_buddy.cpp - 学习伴侣实现
 * 番茄工作法计时器 + OLED表情动画
 */
#include "study_buddy.h"
#include "game_common.h"
#include "lcd_driver.h"
#include "oled_driver.h"
#include "audio.h"
#include "keyboard.h"

enum StudyState {
  STUDY_SELECT,
  STUDY_RUNNING,
  STUDY_PAUSED,
  STUDY_DONE
};

static StudyState state;
static int timerModes[] = {25, 30, 45, 60};
static int modeCount = 4;
static int selectedMode;
static unsigned long totalSeconds;
static unsigned long remainSeconds;
static unsigned long lastTick;
static unsigned long startTime;
static int encourageCount;

static void renderSelect();
static void renderTimer();
static void renderDone();

void studyBuddyInit() {
  state = STUDY_SELECT;
  selectedMode = 0;
  encourageCount = 0;
  renderSelect();
  oledShowFace(FACE_HAPPY);
}

static void renderSelect() {
  lcdClear();
  lcdDrawString(0, 0, "=== 学习伴侣 ===");
  lcdDrawString(0, 16, "选择计时模式:");

  for (int i = 0; i < modeCount; i++) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s %d 分钟",
             (i == selectedMode) ? ">" : " ", timerModes[i]);
    lcdDrawString(10, 28 + i * 10, buf);
  }
  lcdRefresh();
}

static void renderTimer() {
  lcdClear();
  lcdDrawString(20, 0, "专注学习中...");

  int mins = remainSeconds / 60;
  int secs = remainSeconds % 60;
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);

  U8G2* lcd = getLCD();
  lcd->setFont(u8g2_font_logisoso28_tn);
  lcd->drawStr(20, 50, timeBuf);
  lcd->setFont(u8g2_font_wqy12_t_gb2312);

  int progress = ((totalSeconds - remainSeconds) * LCD_WIDTH) / totalSeconds;
  lcdFillRect(0, 60, progress, 4);

  lcdRefresh();
}

static void renderDone() {
  lcdClear();
  lcdDrawString(15, 16, "恭喜完成!");
  char buf[32];
  snprintf(buf, sizeof(buf), "坚持了 %d 分钟", (int)(totalSeconds / 60));
  lcdDrawString(15, 36, buf);
  lcdDrawString(10, 52, "按确认继续");
  lcdRefresh();
}

void studyBuddyLoop(uint8_t key) {
  if (key == KEY_BACK) {
    stopTone();
    currentMode = MODE_MENU;
    drawMenu();
    oledShowWelcome();
    return;
  }

  switch (state) {
    case STUDY_SELECT:
      if (key == KEY_UP) {
        selectedMode = (selectedMode + modeCount - 1) % modeCount;
        renderSelect();
      } else if (key == KEY_DOWN) {
        selectedMode = (selectedMode + 1) % modeCount;
        renderSelect();
      } else if (key == KEY_ENTER) {
        totalSeconds = timerModes[selectedMode] * 60UL;
        remainSeconds = totalSeconds;
        lastTick = millis();
        startTime = millis();
        state = STUDY_RUNNING;
        oledShowFace(FACE_FOCUS);
        renderTimer();
      }
      break;

    case STUDY_RUNNING:
      if (key == KEY_ENTER) {
        state = STUDY_PAUSED;
        lcdDrawString(30, 30, "[暂停]");
        lcdRefresh();
        return;
      }

      if (millis() - lastTick >= 1000) {
        lastTick += 1000;
        if (remainSeconds > 0) {
          remainSeconds--;
          renderTimer();

          if (remainSeconds > 0 && remainSeconds % 300 == 0) {
            encourageCount++;
            if (encourageCount % 2 == 0) oledShowFace(FACE_CHEER);
            else oledShowFace(FACE_FOCUS);
          }
        }

        if (remainSeconds == 0) {
          state = STUDY_DONE;
          renderDone();
          oledShowFace(FACE_DONE);
          playSFX_success();
        }
      }
      break;

    case STUDY_PAUSED:
      if (key == KEY_ENTER) {
        state = STUDY_RUNNING;
        lastTick = millis();
        renderTimer();
        oledShowFace(FACE_FOCUS);
      }
      break;

    case STUDY_DONE:
      if (key == KEY_ENTER) {
        studyBuddyInit();
      }
      break;
  }
}
