/*
 * piano_game.cpp — 钢琴块节奏游戏 (非阻塞音频 + 轨道音高分离)
 * ============================================================================
 * v2 更新:
 *   1) 音频改为非阻塞: playNote() 立即返回, 由 main loop 的 updateAudio() 推进
 *   2) 轨道与音高分离: 每个音符独立记录 pitch 字段, 不再依赖 songPitches 查表
 *
 * 【数据结构变更】
 * NoteProto / RuntimeNote 新增 uint8_t pitch 字段.
 * 歌曲数据格式: {tick, lane, type, duration, pitch}
 *   - lane  决定音符显示在哪条轨道(需要按哪个键)
 *   - pitch 决定播放哪个音高 (0=C4 ... 12=A5, 见 audio.cpp noteFreqs[])
 *
 * 【如何添加新歌】
 * 新建 songN[] 数组, 每个音符指定 tick/lane/type/duration/pitch,
 * 其中 pitch 从乐谱获得, lane 决定按键(0=左,3=右).
 * 终止标记: {0xFFFFFFFF,0,0,0,0}
 */

#include "piano_game.h"
#include "game_common.h"
#include "lcd_driver.h"
#include "oled_driver.h"
#include "audio.h"
#include "keyboard.h"
#include "config.h"
#include <Arduino.h>

// ============================================================================
// 游戏参数
// ============================================================================

#define NOTE_LANES      4
#define LANE_WIDTH      (LCD_WIDTH / NOTE_LANES)
#define NOTE_HEIGHT     26
#define JUDGE_LINE_Y    280
#define MAX_NOTES_ON_SCREEN 30
#define NOTE_POOL_SIZE  300

#define PERFECT_MS  72
#define GOOD_MS     128
#define BAD_MS      160
#define SCROLL_SPEED 1.0f

#define MAX_SONGS 6

// ============================================================================
// 数据结构 (v2: 新增 pitch 字段)
// ============================================================================

struct RuntimeNote {
  uint32_t tick;
  uint8_t  lane;
  uint8_t  type;
  uint32_t duration;
  bool     active;
  uint8_t  pitch;    // ★ 新增: 音高索引 0-12 (C4~A5)
};

struct NoteProto {
  uint32_t tick;
  uint8_t  lane;
  uint8_t  type;
  uint32_t duration;
  uint8_t  pitch;    // ★ 新增: 独立音高, 不再依赖轨道查表
};

// ============================================================================
// 歌曲名称
// ============================================================================

const char* songNames[MAX_SONGS] = {
  "Twinkle Star","Happy Birthday","Test Hold",
  "Ode to Joy","Jingle Bells","Mary's Lamb"
};

// ============================================================================
// 轨道基准音阶表 (保留, 仅用于自由演奏模式, 不用于曲目播放)
// ============================================================================
static const uint8_t PROGMEM songPitches[MAX_SONGS][4] = {
  { 0, 2, 4, 7 },   // C4 E4 G4 C5 — Twinkle Star (C大调)
  { 4, 5, 7, 9 },   // G4 A4 C5 D5 — Happy Birthday (G大调)
  { 0, 4, 7, 11 },  // C4 G4 C5 E5 — Test Hold
  { 2, 4, 5, 7 },   // E4 G4 A4 C5 — Ode to Joy
  { 4, 5, 7, 9 },   // G4 A4 C5 D5 — Jingle Bells (G大调)
  { 0, 2, 4, 5 },   // C4 E4 G4 A4 — Mary's Lamb (C大调)
};

// ============================================================================
// 歌曲数据 (v2 格式: 每个音符末尾增加了 pitch 字段)
// ============================================================================
// 终止标记 {0xFFFFFFFF,0,0,0,0}

// 歌曲 0: 小星星 (C大调)
// 旋律: Do Do Sol Sol La La Sol — Fa Fa Mi Mi Re Re Do
static const NoteProto PROGMEM song0[] = {
  {  0,0,0,0, 0},{500,1,0,0, 2},{1000,2,0,0, 4},{1500,3,0,0, 7},
  {2000,2,0,0, 4},{2500,1,0,0, 2},{3000,0,0,0, 0},{3500,0,1,800, 0},
  {4000,1,0,0, 2},{4500,2,0,0, 4},{5000,3,0,0, 7},{5500,2,0,0, 4},
  {6000,1,0,0, 2},{6500,0,0,0, 0},{7000,0,0,0, 0},
  {0xFFFFFFFF,0,0,0,0}
};

// 歌曲 1: 生日快乐 (G大调)
static const NoteProto PROGMEM song1[] = {
  {  0,2,0,0, 7},{500,2,0,0, 7},{1000,3,0,0, 9},{1500,2,0,0, 7},
  {2000,0,0,0, 4},{2500,1,0,0, 5},{3000,0,1,1000, 4},{4000,2,0,0, 7},
  {4500,2,0,0, 7},{5000,3,0,0, 9},{5500,2,0,0, 7},{6000,1,0,0, 5},
  {6500,0,0,0, 4},{7000,0,1,800, 4},
  {0xFFFFFFFF,0,0,0,0}
};

// 歌曲 2: 长按练习
static const NoteProto PROGMEM song2[] = {
  {  0,0,0,0, 0},{500,1,0,0, 4},{1000,2,1,600, 7},{1500,3,0,0, 11},
  {2000,0,1,1000, 0},{3000,1,0,0, 4},{3500,2,0,0, 7},{4000,3,1,800, 11},
  {0xFFFFFFFF,0,0,0,0}
};

// 歌曲 3: 欢乐颂 (贝多芬)
static const NoteProto PROGMEM song3[] = {
  {  0,0,0,0, 2},{500,0,0,0, 2},{1000,0,0,0, 2},{1500,1,0,0, 4},
  {2000,1,0,0, 4},{2500,1,0,0, 4},{3000,0,0,0, 2},{3500,2,0,0, 5},
  {4000,2,0,0, 5},{4500,2,0,0, 5},{5000,1,0,0, 4},{5500,1,0,0, 4},
  {6000,1,0,0, 4},{6500,0,0,0, 2},{7000,3,0,0, 7},{7500,3,0,0, 7},
  {8000,3,0,0, 7},{8500,0,0,0, 2},{9000,2,0,0, 5},{9500,2,0,0, 5},
  {10000,3,0,0, 7},{10500,1,1,800, 4},{11500,0,0,0, 2},{12000,2,1,600, 5},
  {0xFFFFFFFF,0,0,0,0}
};

// 歌曲 4: 铃儿响叮当 (G大调)
static const NoteProto PROGMEM song4[] = {
  {  0,0,0,0, 4},{400,0,0,0, 4},{800,0,0,0, 4},{1200,0,0,0, 4},
  {1600,0,0,0, 4},{2000,1,0,0, 5},{2400,1,0,0, 5},{2800,0,0,0, 4},
  {3200,2,0,0, 7},{3400,2,0,0, 7},{3600,2,0,0, 7},{4000,3,0,0, 9},
  {4400,0,0,0, 4},{4800,2,0,0, 7},{5200,1,0,0, 5},{5600,1,0,0, 5},
  {6000,1,0,0, 5},{6400,1,0,0, 5},{6800,0,0,0, 4},{7200,1,0,0, 5},
  {7600,1,0,0, 5},{8000,1,0,0, 5},{8400,3,0,0, 9},{8800,3,1,400, 9},
  {9200,2,0,0, 7},{9600,1,0,0, 5},{10000,2,0,0, 7},{10400,3,0,0, 9},
  {0xFFFFFFFF,0,0,0,0}
};

// 歌曲 5: 玛丽有只小羊羔 (C大调)
static const NoteProto PROGMEM song5[] = {
  {  0,0,0,0, 0},{500,1,0,0, 2},{1000,2,0,0, 4},{1500,1,0,0, 2},
  {2000,0,0,0, 0},{2500,0,0,0, 0},{3000,0,0,0, 0},{3500,1,0,0, 2},
  {4000,1,0,0, 2},{4500,1,0,0, 2},{5000,0,0,0, 0},{5500,2,0,0, 4},
  {6000,2,0,0, 4},{6500,0,0,0, 0},{7000,1,0,0, 2},{7500,2,0,0, 4},
  {8000,1,0,0, 2},{8500,0,0,0, 0},{9000,0,0,0, 0},{9500,0,0,0, 0},
  {10000,0,0,0, 0},{10500,1,0,0, 2},{11000,0,0,0, 0},{11500,1,0,0, 2},
  {12000,0,1,600, 0},{12500,0,0,0, 0},
  {0xFFFFFFFF,0,0,0,0}
};

static const NoteProto* const songData[MAX_SONGS] PROGMEM = {
  song0, song1, song2, song3, song4, song5
};
static const int songLengths[MAX_SONGS] = {
  sizeof(song0)/sizeof(song0[0]),
  sizeof(song1)/sizeof(song1[0]),
  sizeof(song2)/sizeof(song2[0]),
  sizeof(song3)/sizeof(song3[0]),
  sizeof(song4)/sizeof(song4[0]),
  sizeof(song5)/sizeof(song5[0])
};

// ============================================================================
// 全局游戏状态
// ============================================================================

static struct {
  int currentSong;
  RuntimeNote pool[NOTE_POOL_SIZE];
  int noteCount;
  int nextLoadIdx;

  struct ActiveNote {
    int poolIdx, x, y;
    bool holdActive;
    int  holdJudge;
    uint32_t pressTime;
  };
  ActiveNote active[MAX_NOTES_ON_SCREEN];
  int activeCnt;

  int perfect, good, bad, miss;
  int combo, maxCombo;
  float acc;
  bool gameOver, paused;

  uint32_t gameStartTime, audioStartTime;
  bool keyState[4];
  uint32_t holdStartTime[4];

  int lastHitType;
  uint32_t lastHitTime, lastOledUpdate;
  int lastKeyLane;
  uint32_t lastKeyTime;

  char currentSongName[32];
} game;

// ============================================================================
// 辅助函数
// ============================================================================

static void computeACC() {
  int total = game.perfect + game.good + game.bad + game.miss;
  game.acc = (total == 0) ? 100.0f
            : (game.perfect + game.good * 0.5f) / total * 100.0f;
}

static void setHitFeedback(int type) {
  game.lastHitType = type;
  game.lastHitTime = millis();
}

/*
 * loadCurrentSong() — 从 Flash 加载歌曲到音符池 (v2: 包含 pitch)
 */
static void loadCurrentSong() {
  int idx = game.currentSong;
  game.noteCount = 0;
  for (int i = 0; i < songLengths[idx]; i++) {
    NoteProto np;
    memcpy_P(&np, &songData[idx][i], sizeof(NoteProto));
    if (np.tick == 0xFFFFFFFF) break;
    game.pool[game.noteCount].tick     = np.tick;
    game.pool[game.noteCount].lane     = np.lane;
    game.pool[game.noteCount].type     = np.type;
    game.pool[game.noteCount].duration = np.duration;
    game.pool[game.noteCount].active   = true;
    game.pool[game.noteCount].pitch    = np.pitch;   // ★ v2: 复制独立音高
    game.noteCount++;
  }
  strncpy(game.currentSongName, songNames[idx], sizeof(game.currentSongName)-1);
  game.currentSongName[sizeof(game.currentSongName)-1] = '\0';
}

static void resetGame() {
  game.perfect = game.good = game.bad = game.miss = 0;
  game.combo = game.maxCombo = 0;
  game.acc = 100.0;
  game.gameOver = false;
  game.paused = false;
  game.activeCnt = 0;
  game.nextLoadIdx = 0;
  game.lastHitType = -1;
  game.lastHitTime = 0;
  game.lastKeyLane = -1;
  game.lastKeyTime = 0;
  for (int i=0; i<4; i++) game.keyState[i] = false;
  loadCurrentSong();
}

static void loadActiveNotes(uint32_t now) {
  while (game.nextLoadIdx < game.noteCount &&
         game.pool[game.nextLoadIdx].tick <= now + 500) {
    if (game.activeCnt >= MAX_NOTES_ON_SCREEN) break;
    int idx = game.nextLoadIdx;
    int lane = game.pool[idx].lane;
    game.active[game.activeCnt].poolIdx    = idx;
    game.active[game.activeCnt].x          = lane * LANE_WIDTH + 2;
    game.active[game.activeCnt].y          = 0;
    game.active[game.activeCnt].holdActive = false;
    game.active[game.activeCnt].holdJudge  = -1;
    game.active[game.activeCnt].pressTime  = 0;
    game.activeCnt++;
    game.nextLoadIdx++;
  }
}

static void updateNotesPosition(uint32_t now) {
  for (int i = 0; i < game.activeCnt; ) {
    RuntimeNote& note = game.pool[game.active[i].poolIdx];
    int64_t delta = (int64_t)now - (game.gameStartTime + note.tick);
    if (delta < 0) delta = 0;
    float yf = delta * SCROLL_SPEED;
    if (yf > LCD_HEIGHT + NOTE_HEIGHT) {
      if (!note.active) {
        game.active[i] = game.active[--game.activeCnt];
        continue;
      }
      game.miss++; game.combo = 0; computeACC(); setHitFeedback(3);
      note.active = false;
      game.active[i] = game.active[--game.activeCnt];
      continue;
    }
    game.active[i].y = (int)yf;
    i++;
  }
}

/*
 * processInput(now, key) — 按键判定 (v2: 使用 note.pitch 代替查表)
 *
 * 音符命中后调用 playNote(note.pitch)，播放该音符独立记录的音高。
 * playNote() 是非阻塞的：调用后立即返回，由 updateAudio() 在后台控制停止。
 */
static void processInput(uint32_t now, uint8_t key) {
  int lane = -1;
  switch(key) {
    case KEY_1: case KEY_LEFT:  lane = 0; break;
    case KEY_2: case KEY_UP:    lane = 1; break;
    case KEY_3: case KEY_DOWN:  lane = 2; break;
    case KEY_4: case KEY_RIGHT: lane = 3; break;
  }
  if (lane != -1) {
    if (!game.keyState[lane]) {
      game.keyState[lane] = true;
      game.lastKeyLane = lane;
      game.lastKeyTime = now;
      int bestIdx = -1, bestDist = 9999;
      for (int i = 0; i < game.activeCnt; i++) {
        RuntimeNote& note = game.pool[game.active[i].poolIdx];
        if (note.lane == lane && note.active) {
          int dist = abs(game.active[i].y - JUDGE_LINE_Y);
          if (dist < bestDist) { bestDist = dist; bestIdx = i; }
        }
      }
      if (bestIdx != -1) {
        RuntimeNote& note = game.pool[game.active[bestIdx].poolIdx];
        int delta = abs((int)((now - game.gameStartTime) - note.tick));
        if (delta <= BAD_MS) {
          int judgeType;
          if (delta <= PERFECT_MS) judgeType = 0;
          else if (delta <= GOOD_MS) judgeType = 1;
          else judgeType = 2;
          if (note.type == 0) {
            // ── TAP ──
            // ★ v2: 使用 note.pitch 代替 songPitches 查表
            playNote(note.pitch);
            if (judgeType == 0) game.perfect++;
            else if (judgeType == 1) game.good++;
            else game.bad++;
            game.combo = (judgeType <= 1) ? (game.combo + 1) : 0;
            if (game.combo > game.maxCombo) game.maxCombo = game.combo;
            computeACC(); setHitFeedback(judgeType);
            note.active = false;
            game.active[bestIdx] = game.active[--game.activeCnt];
          } else {
            // ── HOLD ──
            if (judgeType == 2) {
              game.bad++; game.combo = 0; computeACC(); setHitFeedback(2);
              note.active = false;
              game.active[bestIdx] = game.active[--game.activeCnt];
            } else {
              game.active[bestIdx].holdActive = true;
              game.active[bestIdx].holdJudge  = judgeType;
              game.active[bestIdx].pressTime  = now;
              playNote(note.pitch);  // ★ v2: 独立音高
            }
          }
        } else {
          game.miss++; game.combo = 0; computeACC(); setHitFeedback(3);
          note.active = false;
          game.active[bestIdx] = game.active[--game.activeCnt];
        }
      } else {
        game.combo = 0;
      }
    }
  } else {
    for (int l = 0; l < 4; l++) {
      if (game.keyState[l]) {
        uint8_t phyKey = KEY_NONE;
        switch(l) {
          case 0: phyKey = KEY_1; break;
          case 1: phyKey = KEY_2; break;
          case 2: phyKey = KEY_3; break;
          case 3: phyKey = KEY_4; break;
        }
        if (!isKeyPressed(phyKey)) {
          game.keyState[l] = false;
          for (int i = 0; i < game.activeCnt; i++) {
            RuntimeNote& note = game.pool[game.active[i].poolIdx];
            if (note.type == 1 && note.lane == l && game.active[i].holdActive) {
              uint32_t holdDuration = now - game.active[i].pressTime;
              int delta = abs((int)holdDuration - (int)note.duration);
              int finalJudge = game.active[i].holdJudge;
              if (delta > GOOD_MS) finalJudge = 2;
              else if (delta > PERFECT_MS) finalJudge = 1;
              if (finalJudge == 0) game.perfect++;
              else if (finalJudge == 1) game.good++;
              else game.bad++;
              if (finalJudge <= 1) game.combo++;
              else game.combo = 0;
              if (game.combo > game.maxCombo) game.maxCombo = game.combo;
              computeACC(); setHitFeedback(finalJudge);
              note.active = false;
              game.active[i] = game.active[--game.activeCnt];
              break;
            }
          }
        }
      }
    }
  }
}

// ============================================================================
// 渲染
// ============================================================================

static void renderGame(uint32_t now) {
  lcdClear(COLOR_BLACK);
  for (int i = 1; i < NOTE_LANES; i++)
    lcdDrawLine(i * LANE_WIDTH, 0, i * LANE_WIDTH, LCD_HEIGHT, 0x4208);
  lcdDrawLine(0, JUDGE_LINE_Y, LCD_WIDTH, JUDGE_LINE_Y, COLOR_WHITE);
  for (int i = 0; i < game.activeCnt; i++) {
    RuntimeNote& note = game.pool[game.active[i].poolIdx];
    int x = game.active[i].x, y = game.active[i].y;
    if (note.type == 0)
      lcdFillRect(x, y, LANE_WIDTH - 4, NOTE_HEIGHT, COLOR_WHITE);
    else {
      int endY = y + (int)(note.duration * SCROLL_SPEED);
      if (endY > LCD_HEIGHT) endY = LCD_HEIGHT;
      if (endY > y) lcdFillRect(x, y, LANE_WIDTH - 4, endY - y, 0x8410);
    }
  }
  lcdSetTextColor(COLOR_WHITE, COLOR_BLACK);
  char buf[20];
  snprintf(buf, sizeof(buf), "%d", game.combo);
  lcdDrawString(LCD_WIDTH - 40, 4, buf, 2);
  lcdDrawString(2, 4, game.currentSongName, 1);
  lcdRefresh();
}

static void updateOLEDFace() {
  uint32_t now = millis();
  uint8_t face = FACE_IDLE_A;
  bool k0 = game.keyState[0], k1 = game.keyState[1],
       k2 = game.keyState[2], k3 = game.keyState[3];
  if (game.lastKeyTime > 0 && now - game.lastKeyTime < 500) {
    bool leftAny = (k0 || k1), rightAny = (k2 || k3);
    if (leftAny && rightAny) { oledShowConflict(); return; }
    if (k0 && k1)            { oledShowPhanion3(); return; }
    if (k2 && k3)            { oledShowMydei3();   return; }
    if (k0)                  { oledShowPhanion();  return; }
    if (k1)                  { oledShowPhanion2(); return; }
    if (k2)                  { oledShowMydei1();   return; }
    if (k3)                  { oledShowMydei2();   return; }
  }
  if (game.lastHitTime > 0 && now - game.lastHitTime < 400) {
    switch (game.lastHitType) {
      case 0: face = FACE_PIANO_COMBO; break;
      case 1: face = FACE_PIANO_1;     break;
      case 2: face = FACE_PIANO_MISS;  break;
      case 3: face = FACE_PIANO_MISS;  break;
    }
  } else if (game.combo >= 20)      face = FACE_PIANO_COMBO;
  else if (game.combo >= 10)        face = FACE_PIANO_4;
  else if (game.combo >= 5)         face = FACE_PIANO_2;
  char top[16], bottom[16];
  snprintf(top, sizeof(top), "ACC:%.0f%%", game.acc);
  snprintf(bottom, sizeof(bottom), "P%dG%dB%dM%d",
           game.perfect, game.good, game.bad, game.miss);
  oledDrawBorderText(top, bottom);
  oledShowFace(face);
}

static void drawStartScreen() {
  lcdClear(COLOR_DARKBG);
  lcdFillRect(0, 0, LCD_WIDTH, 45, COLOR_NAVY);
  lcdSetTextColor(COLOR_CYAN, COLOR_NAVY);
  lcdDrawString(30, 8, "Piano Game", 2);
  lcdSetTextColor(COLOR_WHITE, COLOR_DARKBG);
  lcdDrawString(15, 70, "Song:", 2);
  lcdSetTextColor(COLOR_GOLD, COLOR_DARKBG);
  lcdDrawString(15, 100, songNames[game.currentSong], 2);
  lcdSetTextColor(0x8410, COLOR_DARKBG);
  lcdDrawString(15, 150, "Keys:", 1);
  lcdDrawString(15, 170, "1 2 3 4", 2);
  lcdSetTextColor(0x8410, COLOR_DARKBG);
  lcdDrawString(15, 220, "5/6:Change Song", 1);
  lcdDrawString(15, 245, "ENTER:Start", 2);
  lcdRefresh();
}

// ============================================================================
// 公共接口
// ============================================================================

void pianoGameInit() {
  game.currentSong = 0;
  resetGame();
  game.gameStartTime = millis() + 2000;
  game.lastOledUpdate = 0;
  stopTone();
  drawStartScreen();
  oledDrawBorderText("Piano Game", "Press 5/6");
}

void pianoGameLoop(uint8_t key) {
  if (key == KEY_BACK) {
    stopTone();
    currentMode = MODE_MENU;
    drawMenu();
    oledShowWelcome();
    return;
  }
  if (key == KEY_5) {
    game.currentSong = (game.currentSong + 1) % MAX_SONGS;
    resetGame();
    game.gameStartTime = millis() + 2000;
    drawStartScreen();
    return;
  }
  if (key == KEY_6) {
    game.currentSong = (game.currentSong - 1 + MAX_SONGS) % MAX_SONGS;
    resetGame();
    game.gameStartTime = millis() + 2000;
    drawStartScreen();
    return;
  }
  if (key == KEY_ENTER && !game.gameOver) {
    game.paused = !game.paused;
    if (game.paused) {
      stopTone();
      lcdClear(COLOR_DARKBG);
      lcdFillRect(0, 120, LCD_WIDTH, 50, COLOR_NAVY);
      lcdSetTextColor(COLOR_GOLD, COLOR_NAVY);
      lcdDrawString(50, 130, "PAUSED", 3);
      lcdSetTextColor(COLOR_GRAY, COLOR_DARKBG);
      lcdDrawString(30, 220, "ENTER to resume", 2);
      lcdRefresh();
    }
    return;
  }
  if (game.paused) return;

  uint32_t now = millis();
  uint32_t gameTime = (now >= game.gameStartTime) ? (now - game.gameStartTime) : 0;
  loadActiveNotes(gameTime);
  updateNotesPosition(now);
  processInput(now, key);

  if (game.nextLoadIdx >= game.noteCount && game.activeCnt == 0 && !game.gameOver)
    game.gameOver = true;

  if (game.gameOver) {
    lcdClear(COLOR_DARKBG);
    lcdFillRect(0, 0, LCD_WIDTH, 40, COLOR_NAVY);
    lcdSetTextColor(COLOR_GOLD, COLOR_NAVY);
    lcdDrawString(45, 5, "GAME OVER", 3);
    char grade = 'D';
    if (game.acc >= 95.0f) grade = 'S';
    else if (game.acc >= 85.0f) grade = 'A';
    else if (game.acc >= 70.0f) grade = 'B';
    else if (game.acc >= 55.0f) grade = 'C';
    char gs[4] = {grade, 0};
    lcdSetTextColor(COLOR_GOLD, COLOR_DARKBG);
    lcdDrawString(LCD_WIDTH - 50, 45, gs, 4);
    char buf[40];
    lcdSetTextColor(COLOR_WHITE, COLOR_DARKBG);
    snprintf(buf, sizeof(buf), "Song: %s", game.currentSongName);
    lcdDrawString(10, 50, buf, 1);
    snprintf(buf, sizeof(buf), "ACC: %.1f%%", game.acc);
    lcdSetTextColor(COLOR_CYAN, COLOR_DARKBG);
    lcdDrawString(10, 75, buf, 2);
    lcdSetTextColor(COLOR_WHITE, COLOR_DARKBG);
    snprintf(buf, sizeof(buf), "Combo: %d", game.maxCombo);
    lcdDrawString(10, 110, buf, 2);
    lcdSetTextColor(COLOR_GOLD, COLOR_DARKBG);
    snprintf(buf, sizeof(buf), "P:%d", game.perfect);
    lcdDrawString(10, 145, buf, 2);
    lcdSetTextColor(COLOR_GREEN, COLOR_DARKBG);
    snprintf(buf, sizeof(buf), "G:%d", game.good);
    lcdDrawString(80, 145, buf, 2);
    lcdSetTextColor(COLOR_ORANGE, COLOR_DARKBG);
    snprintf(buf, sizeof(buf), "B:%d", game.bad);
    lcdDrawString(150, 145, buf, 2);
    lcdSetTextColor(COLOR_RED, COLOR_DARKBG);
    snprintf(buf, sizeof(buf), "M:%d", game.miss);
    lcdDrawString(10, 180, buf, 2);
    lcdDrawLine(10, 220, LCD_WIDTH - 10, 220, 0x4208);
    lcdSetTextColor(COLOR_GRAY, COLOR_DARKBG);
    lcdDrawString(40, 250, "ENTER to retry", 2);
    lcdRefresh();
    oledDrawBorderText("Game Over", "ENTER=retry");
    oledShowBlinkingFace(FACE_DONE_A, FACE_DONE_B, 500);
    if (key == KEY_ENTER) {
      resetGame();
      game.gameOver = false;
      game.gameStartTime = millis() + 1000;
    }
    return;
  }

  renderGame(now);
  if (now - game.lastOledUpdate >= 80) {
    updateOLEDFace();
    game.lastOledUpdate = now;
  }
}

void pianoGameNextSong() {
  game.currentSong = (game.currentSong + 1) % MAX_SONGS;
  resetGame();
  game.gameStartTime = millis() + 1000;
}

void pianoGamePrevSong() {
  game.currentSong = (game.currentSong - 1 + MAX_SONGS) % MAX_SONGS;
  resetGame();
  game.gameStartTime = millis() + 1000;
}
