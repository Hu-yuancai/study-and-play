/*
 * piano_game.cpp - 钢琴游戏实现
 * 音符从屏幕顶部下落，玩家在底部按对应键弹奏
 */
#include "piano_game.h"
#include "game_common.h"
#include "lcd_driver.h"
#include "oled_driver.h"
#include "audio.h"
#include "keyboard.h"

#define MAX_NOTES     16
#define NOTE_LANES    4
#define LANE_WIDTH    (LCD_WIDTH / NOTE_LANES)
#define NOTE_HEIGHT   8
#define JUDGE_LINE_Y  56
#define FALL_SPEED    2

struct FallingNote {
  int x;
  int y;
  uint8_t lane;
  uint8_t noteIndex;
  bool active;
};

static FallingNote notes[MAX_NOTES];
static int score;
static int combo;
static int missCount;
static bool gameOver;
static unsigned long lastSpawn;
static unsigned long spawnInterval;
static unsigned long lastFrame;

// 预设曲谱: {lane, noteIndex, spawnDelay_ms}
static const uint8_t songData[][3] = {
  {0, 0, 0}, {1, 2, 4}, {2, 4, 4}, {3, 5, 4},
  {2, 4, 4}, {1, 2, 4}, {0, 0, 4}, {1, 2, 8},
  {0, 0, 4}, {1, 2, 4}, {2, 4, 4}, {3, 5, 4},
  {2, 4, 4}, {1, 2, 4}, {0, 0, 4}, {0, 0, 8},
};
static const int songLength = sizeof(songData) / sizeof(songData[0]);
static int songPos;
static unsigned long nextNoteTime;

static void spawnNote(uint8_t lane, uint8_t noteIdx) {
  for (int i = 0; i < MAX_NOTES; i++) {
    if (!notes[i].active) {
      notes[i].active = true;
      notes[i].lane = lane;
      notes[i].noteIndex = noteIdx;
      notes[i].x = lane * LANE_WIDTH + 4;
      notes[i].y = 0;
      return;
    }
  }
}

void pianoGameInit() {
  score = 0;
  combo = 0;
  missCount = 0;
  gameOver = false;
  songPos = 0;
  spawnInterval = 500;
  lastSpawn = millis();
  lastFrame = millis();
  nextNoteTime = millis() + 1000;

  for (int i = 0; i < MAX_NOTES; i++) {
    notes[i].active = false;
  }

  oledShowFace(FACE_HAPPY);
}

static void updateNotes() {
  for (int i = 0; i < MAX_NOTES; i++) {
    if (notes[i].active) {
      notes[i].y += FALL_SPEED;
      if (notes[i].y > LCD_HEIGHT) {
        notes[i].active = false;
        missCount++;
        combo = 0;
        if (missCount >= 10) gameOver = true;
      }
    }
  }
}

static void spawnFromSong() {
  if (songPos >= songLength) {
    songPos = 0;
    nextNoteTime = millis();
  }
  if (millis() >= nextNoteTime) {
    spawnNote(songData[songPos][0], songData[songPos][1]);
    nextNoteTime += songData[songPos][2] * 100 + spawnInterval;
    songPos++;
  }
}

static void handleInput(uint8_t key) {
  int lane = -1;
  if (key == KEY_1 || key == KEY_LEFT)  lane = 0;
  if (key == KEY_2 || key == KEY_UP)    lane = 1;
  if (key == KEY_3 || key == KEY_DOWN)  lane = 2;
  if (key == KEY_4 || key == KEY_RIGHT) lane = 3;

  if (lane < 0) return;

  for (int i = 0; i < MAX_NOTES; i++) {
    if (notes[i].active && notes[i].lane == lane) {
      int dist = abs(notes[i].y - JUDGE_LINE_Y);
      if (dist < 12) {
        notes[i].active = false;
        playNote(notes[i].noteIndex);
        combo++;
        if (dist < 4) score += 100 * (1 + combo / 10);
        else score += 50 * (1 + combo / 10);
        return;
      }
    }
  }
  combo = 0;
}

static void render() {
  lcdClear();

  for (int i = 1; i < NOTE_LANES; i++) {
    lcdDrawLine(i * LANE_WIDTH, 0, i * LANE_WIDTH, LCD_HEIGHT);
  }
  lcdDrawLine(0, JUDGE_LINE_Y, LCD_WIDTH, JUDGE_LINE_Y);

  for (int i = 0; i < MAX_NOTES; i++) {
    if (notes[i].active) {
      lcdFillRect(notes[i].x, notes[i].y, LANE_WIDTH - 8, NOTE_HEIGHT);
    }
  }

  char buf[20];
  snprintf(buf, sizeof(buf), "%d", score);
  lcdDrawString(90, 0, buf);

  lcdRefresh();
}

void pianoGameLoop(uint8_t key) {
  if (key == KEY_BACK) {
    currentMode = MODE_MENU;
    drawMenu();
    oledShowWelcome();
    return;
  }

  if (gameOver) {
    lcdClear();
    lcdDrawString(20, 20, "游戏结束!");
    char buf[32];
    snprintf(buf, sizeof(buf), "得分: %d", score);
    lcdDrawString(20, 40, buf);
    lcdRefresh();
    oledShowFace(FACE_SAD);
    if (key == KEY_ENTER) pianoGameInit();
    return;
  }

  if (millis() - lastFrame < FRAME_MS) return;
  lastFrame = millis();

  handleInput(key);
  spawnFromSong();
  updateNotes();
  render();

  if (combo > 0 && combo % 10 == 0) {
    oledShowFace(FACE_CHEER);
  }
}
