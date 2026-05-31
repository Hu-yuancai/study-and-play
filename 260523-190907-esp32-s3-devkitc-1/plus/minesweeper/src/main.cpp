/*
 * main.cpp — ESP32-S3 学习游戏一体机 主程序
 * ============================================================================
 * 【项目概述】
 * 这是一个运行在 ESP32-S3 开发板上的多功能学习游戏机。
 * 硬件组成：
 *   - ESP32-S3-DevKitC-1 开发板（主控芯片）
 *   - ILI9341 TFT LCD (240×320 彩色屏, SPI 接口)
 *   - SSD1306 OLED (128×64 单色屏, I2C 接口)
 *   - 4×4 矩阵键盘（16 个按键）
 *   - 无源蜂鸣器（PWM 驱动发声）
 *
 * 软件功能：
 *   1. 钢琴块节奏游戏：6 首曲目，4 轨下落式音符，支持长按
 *   2. 扫雷游戏：8-16 网格，键盘操作，Ghost Shell 调试
 *   3. 学习伴侣（番茄钟）：4 种时长选择，倒计时，鼓励提醒
 *   4. Ghost Shell：串口调试终端 (USB Serial)
 *
 * 【程序执行流程】
 * 1. 上电 → setup() 逐一初始化外设 → 播放启动提示音
 * 2. loop() 无限循环：
 *    a. 扫描键盘（约每 16ms 一次，受 delay(16) 控制）
 *    b. 根据 currentMode 调用对应模式的处理函数
 *    c. delay(16) — 简单帧率控制，约 60 FPS
 *
 * 【如何添加新的游戏/功能？】
 * 1. 创建新的 .cpp 和 .h 文件（如 my_game.cpp, my_game.h）
 * 2. 在 game_common.h 的 GameMode 枚举中加 MODE_MY_GAME
 * 3. 在 main.cpp 顶部 #include "my_game.h"
 * 4. 在 loop() 的 switch 中加 case MODE_MY_GAME: myGameLoop(key); break;
 * 5. 在 drawMenu() 中为你的游戏添加菜单项
 * 6. 在 handleMenu() 中为你的菜单项添加处理逻辑
 */

#include <Arduino.h>      // Arduino 核心框架
#include <U8g2lib.h>      // OLED 图形库（U8g2）
#include <Wire.h>         // I2C 通信（用于 OLED）
#include <SPI.h>          // SPI 通信（用于 LCD）
#include "config.h"       // 引脚定义和键值宏
#include "game_common.h"  // GameMode 枚举，currentMode 全局变量声明
#include "audio.h"        // PWM 音频驱动
#include "keyboard.h"     // 4×4 矩阵键盘
#include "lcd_driver.h"   // ILI9341 LCD 驱动
#include "oled_driver.h"  // SSD1306 OLED 驱动
#include "piano_game.h"      // 钢琴块游戏
#include "minesweeper.h"     // 扫雷游戏
#include "study_buddy.h"     // 学习伴侣
#include "ghost_shell.h"     // Ghost Shell 调试终端
#include "safe_ram.h"        // safeRAM 内存区

// ============================================================================
// 全局变量定义
// ============================================================================

/*
 * currentMode — 当前游戏模式
 * 这是整个程序的状态核心，决定 loop() 调用哪个模块的 update 函数。
 * game_common.h 中声明为 extern，这里才是真正的定义（分配内存）。
 * 初始值 MODE_MENU 表示开机后先进入主菜单。
 */
GameMode currentMode = MODE_MENU;

/*
 * deltaTime — 当前帧耗时（秒），由 loop() 每帧计算
 * 所有模块中的运动速度乘以 deltaTime 实现帧率无关的物理
 */
float deltaTime = 0.016f;  // 初始默认约 60 FPS

/*
 * menuSelection — 主菜单中当前选中的项目索引
 * 0 = 钢琴游戏, 1 = 飞机大战, 2 = 学习伴侣
 * 键盘上下键改变这个值，回车键进入对应模式
 */
int menuSelection = 0;

// ============================================================================
// 菜单相关变量
// ============================================================================

/*
 * lastMenuOled — 上次刷新 OLED 菜单文字的时间戳
 * 用于实现"每 1 秒刷新一次菜单文字"
 */
static unsigned long lastMenuOled;

/*
 * lastMenuActivity — 上次在菜单中有按键操作的时间戳
 * 用于检测用户是否超过 1 分钟没有操作
 */
static unsigned long lastMenuActivity;

// ============================================================================
// 菜单文字内容（可自定义）
// ============================================================================
//
// menuLines[mode][line] — 每个游戏模式有 3 行描述文字
// 在菜单中选中某模式时，这些文字会显示在 OLED 上
//
// 【如何修改菜单文字？】
// 直接修改下面的字符串即可。可以写中文、英文、甚至 emoji（如果字体支持）。
// 注意：每行最多约 10 个中文字符（OLED 宽 128 像素，中文字约 12 像素宽）
//
// menuLines[0][*] — 选中钢琴模式时显示的 3 行文字
// menuLines[1][*] — 选中飞机模式时显示的 3 行文字
// menuLines[2][*] — 选中学习模式时显示的 3 行文字
//
static const char* menuLines[3][3] = {
  // 第 0 模式：钢琴游戏
  {"选这个这个好玩",
   "确定选这个吗？",
   "别听它的这个不好玩"},

  // 第 1 模式：飞机大战
  {"？你玩这个？",
   "要不再想想呢？",
   "就要玩这个！"},

  // 第 2 模式：学习伴侣
  {"善。",
   "对的对的、",
   "卷起来！！！"}
};

// ============================================================================
// 超时提示文字
// ============================================================================
//
// 当用户在菜单中超过 1 分钟没有操作时，OLED 显示这些文字。
// 这是一个有趣的"催促"提示，模拟角色在等得不耐烦了。
//
static const char* timeoutLine1 = "？";
static const char* timeoutLine2 = "再不选就自动重启";
static const char* timeoutLine3 = "你不会真信了吧… ";

/*
 * menuIdleTimedOut — 是否已触发超时提示
 * 防止每帧都重复设置 OLED 文字
 */
static bool menuIdleTimedOut = false;

// ============================================================================
// setup() — 系统初始化（开机时调用一次）
// ============================================================================
//
// 【Arduino 程序结构】
// 每个 Arduino 程序都有两个核心函数：
//   setup()  — 上电/复位后调用一次，用于初始化
//   loop()   — setup() 结束后无限循环调用
//
// 【初始化顺序说明】
// 1. Serial.begin()  — 串口（用于调试输出到电脑）
// 2. initAudio()     — 音频（先初始化以便播放启动提示音）
// 3. playTone()      — 播放一个短促的启动音
// 4. initKeyboard()  — 键盘
// 5. initLCD()       — TFT 彩屏
// 6. initOLED()      — OLED 副屏
// 7. drawMenu()      — 绘制主菜单
// 8. oledShowWelcome() — OLED 显示欢迎文字
//
void setup() {
  // 初始化串口通信（波特率 115200 = 每秒传输约 14KB）
  // 打开电脑的"串口监视器"（PlatformIO: 点击底部 Serial Monitor）
  // 就能看到这些打印信息，方便调试
  Serial.begin(115200);
  Serial.println("=== ESP32-S3 Learning Game Machine ===");

  // 初始化 safeRAM (Ghost Shell 调试用)
  safeRAMInit();

  // 初始化音频模块（配置 PWM 输出到蜂鸣器引脚）
  initAudio();

  // 播放一声短促的提示音（880Hz ≈ A4音，持续100毫秒）


  // 初始化矩阵键盘（配置 4 行输出 + 4 列输入上拉）
  initKeyboard();

  // 初始化 TFT LCD 彩屏（SPI 通信，240×320 像素）
  initLCD();

  // 初始化 OLED 副屏（I2C 通信，128×64 像素，支持中文）
  initOLED();

  // 绘制主菜单界面到 LCD
  drawMenu();

  // OLED 显示欢迎文字
  oledShowWelcome();

  // 记录当前时间，用于后续的"每秒刷新"和"超时检测"
  lastMenuOled = millis();
  lastMenuActivity = millis();
}

// ============================================================================
// handleMenu(key) — 处理主菜单中的按键输入
// ============================================================================
//
// 【按键在菜单中的功能】
// KEY_UP    → 选中上一个菜单项（循环）
// KEY_DOWN  → 选中下一个菜单项（循环）
// KEY_ENTER → 进入选中的游戏模式
//
// 【循环选择的实现】
// menuSelection = (menuSelection + 2) % 3;  ← 上移（+2 相当于 -1 再 +3 保证非负）
// menuSelection = (menuSelection + 1) % 3;  ← 下移
// % 3 确保值始终在 0,1,2 之间循环（0→2→1→0→2→...）
//
// @param key  当前检测到的按键值
//
void handleMenu(uint8_t key) {
  // ── Konami 码检测: ↑↑↓↓←→←→ C ENTER → 进入 Ghost Shell ──
  static const uint8_t konami[] = {KEY_UP,KEY_UP,KEY_DOWN,KEY_DOWN,
    KEY_LEFT,KEY_RIGHT,KEY_LEFT,KEY_RIGHT,KEY_FIRE,KEY_ENTER};
  static int konamiPos = 0;
  if (key != KEY_NONE) {
    if (key == konami[konamiPos]) {
      konamiPos++;
      if (konamiPos == 10) {
        konamiPos = 0; keyQueueFlush();
        playTone(880,40); delay(50); playTone(1100,40); delay(50); playTone(1400,60);
        currentMode = MODE_SHELL; shellInit(); return;
      }
    } else { konamiPos = (key == konami[0]) ? 1 : 0; }
  }

  if (key == KEY_UP) {
    // 向上选择：(current + 2) % 3 等价于 (current - 1 + 3) % 3
    menuSelection = (menuSelection + 2) % 3;
    lastMenuActivity = millis();   // 记录活动时间
    menuIdleTimedOut = false;       // 清除超时标志
    drawMenu();                     // 重绘菜单（高亮新的选项）
  } else if (key == KEY_DOWN) {
    // 向下选择：(current + 1) % 3
    menuSelection = (menuSelection + 1) % 3;
    lastMenuActivity = millis();
    menuIdleTimedOut = false;
    drawMenu();
  } else if (key == KEY_ENTER) {
    // 确认：根据当前选中的菜单项进入对应模式
    keyQueueFlush();  // 清空旧按键, 避免带入新模块
    switch (menuSelection) {
      case 0:
        currentMode = MODE_PIANO;
        pianoGameInit();
        break;
      case 1:
        currentMode = MODE_MINESWEEPER;
        minesweeperInit();
        break;
      case 2:
        currentMode = MODE_STUDY;
        studyBuddyInit();
        break;
    }
  }
}

// ============================================================================
// loop() — 主循环（setup() 之后无限重复执行）
// ============================================================================
//
// 【程序的主循环逻辑】
// while(true) {  ← Arduino 内部就是这样调用 loop() 的
//   读取按键
//   根据当前模式分发处理
//   延迟 16ms（控制帧率）
// }
//
void loop() {
  // ── 计算帧时间差 dt ──
  static unsigned long lastFrameUs = micros();
  unsigned long nowUs = micros();
  deltaTime = (nowUs - lastFrameUs) / 1000000.0f;
  lastFrameUs = nowUs;
  if (deltaTime > 0.1f) deltaTime = 0.016f;

  // 第 1 步：扫描键盘, 入队 (防漏键)
  uint8_t rawKey = scanKeyboard();
  if (rawKey != KEY_NONE) keyQueuePush(rawKey);

  // 第 2 步：消费队列中所有按键事件
  while (keyQueueAvailable()) {
    uint8_t key = keyQueuePop();
    switch (currentMode) {
      case MODE_MENU:
        handleMenu(key);
        break;
      case MODE_PIANO:  pianoGameLoop(key);  break;
      case MODE_MINESWEEPER: minesweeperLoop(key); break;
      case MODE_STUDY:  studyBuddyLoop(key); break;
      case MODE_SHELL:  shellLoop(key);        break;
    }
  }

  // 第 3 步：菜单超时检测 + OLED 刷新 (不依赖按键)
  if (currentMode == MODE_MENU) {
    if (millis() - lastMenuActivity > 60000 && !menuIdleTimedOut) {
      menuIdleTimedOut = true;
      oledShowMenuText(timeoutLine1, timeoutLine2, timeoutLine3);
    }
    if (millis() - lastMenuOled > 1000) {
      lastMenuOled = millis();
      if (!menuIdleTimedOut) {
        int s = menuSelection;
        oledShowMenuText(menuLines[s][0], menuLines[s][1], menuLines[s][2]);
      }
    }
  }

  // 第 4 步：推进非阻塞音频状态机
  updateAudio();

  // 第 5 步：让出 CPU 给 RTOS (替代固定 delay)
  yield();
}

// ============================================================================
// drawMenu() — 绘制主菜单界面到 TFT LCD
// ============================================================================
//
// 【界面布局】（从上到下）
// ┌────────────────────────┐
// │   Learning Game        │ ← 标题栏（黄色背景，高 55px）
// ├────────────────────────┤
// │ █ Piano Game           │ ← 第 0 项（绿色指示条=选中态）
// │   Rhythm game with...  │
// │                        │
// │   Plane Battle         │ ← 第 1 项
// │   Shoot-em-up arcade   │
// │                        │
// │   Study Buddy          │ ← 第 2 项
// │   Pomodoro focus timer │
// ├────────────────────────┤
// │   UP/DOWN  ENTER       │ ← 底部操作提示
// └────────────────────────┘
//
// 【颜色方案】
// - 背景：COLOR_DARKBG（深蓝黑色）
// - 标题栏：COLOR_YELLOW（黄色背景）+ 紫色文字
// - 选中项：亮色文字 + 绿色左边框
// - 未选中：暗灰色文字
//
void drawMenu() {
  // 第 1 步：清空画布为深蓝黑背景
  lcdClear(COLOR_DARKBG);

  // --- 第 2 步：绘制顶部标题栏 ---
  // fillRect(x, y, w, h, color): 从(x,y)开始画一个 w×h 的实心矩形
  lcdFillRect(0, 0, LCD_WIDTH, 55, COLOR_YELLOW);  // 黄色标题栏背景
  lcdSetTextColor(0x8010, COLOR_YELLOW);             // 紫色文字 + 黄色底
  lcdDrawString(5, 19, "Learning Game", 3);         // 标题文字，3 倍大小

  // --- 第 3 步：绘制三个菜单项 ---
  // 这里用数组存储标题和描述，方便统一管理
  const char* items[]  = {"Piano Game", "Minesweeper", "Study Buddy"};
  const char* descs[]  = {"Rhythm game with 4 keys",
                          "Classic mine sweeping",
                          "Pomodoro focus timer"};

  for (int i = 0; i < 3; i++) {
    bool sel = (menuSelection == i);  // 当前项是否为选中状态
    int y = 80 + i * 78;               // Y 坐标：80, 158, 236（每项间距 78px）

    if (sel) {
      // 选中状态：画一条绿色的左边框（5px 宽 × 68px 高）
      lcdFillRect(12, y - 2, 5, 68, COLOR_GREEN);
      lcdSetTextColor(0xFE19, 0x1082);  // 亮白文字 + 深蓝底
    } else {
      // 未选中状态：暗灰色文字
      lcdSetTextColor(0x4208, COLOR_DARKBG);
    }

    // 画标题（字体大小 2）
    lcdDrawString(30, y + 4, items[i], 2);

    // 画描述文字（字体大小 1，颜色也随选中态变化）
    lcdSetTextColor(sel ? 0xE7FF : 0x4208, sel ? 0x1082 : COLOR_DARKBG);
    lcdDrawString(30, y + 28, descs[i], 1);
  }

  // --- 第 4 步：绘制底部操作提示 ---
  lcdSetTextColor(0x4208, COLOR_DARKBG);             // 暗灰色文字
  lcdDrawString(40, LCD_HEIGHT - 20, "UP/DOWN  ENTER", 1);

  // --- 第 5 步：刷新到屏幕 ---
  // 所有绘图都在内存画布中，这一步才真正显示到 LCD
  lcdRefresh();
}
