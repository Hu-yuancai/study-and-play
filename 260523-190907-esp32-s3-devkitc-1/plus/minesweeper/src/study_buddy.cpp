/*
 * study_buddy.cpp — 学习伴侣 / 番茄工作法计时器
 * ============================================================================
 * 【功能介绍】
 * 一个专注学习的倒计时工具，灵感来源于番茄工作法：
 *   1. 选择专注时长（25/30/45/60 分钟）
 *   2. 倒计时开始，每 5 分钟 OLED 角色给你一个鼓励表情
 *   3. 可以随时暂停/继续
 *   4. 计时结束后播放成功音效 + 完成界面
 *
 * 【番茄工作法简介】
 * 由弗朗西斯科·西里洛在 1980 年代创立：
 *   - 25 分钟专注工作 → 5 分钟休息 → 循环
 *   - 每 4 个"番茄"后休息 15-30 分钟
 *   - 目的是保持高度专注，避免疲劳
 *
 * 【状态机设计】
 * 程序使用有限状态机（Finite State Machine, FSM）管理界面：
 *
 *   STUDY_SELECT ──ENTER──→ STUDY_RUNNING
 *        ↑                      │  ↑
 *        │                   ENTER  ENTER
 *        │                      ↓  │
 *        └──BACK──────── STUDY_PAUSED
 *                              │
 *                          (时间到)
 *                              ↓
 *                         STUDY_DONE ──ENTER──→ STUDY_SELECT
 *
 * 状态机的优点是逻辑清晰：每个状态只响应特定的按键，
 * 不会出现"在暂停时还能改时长"这种 bug。
 *
 * 【计时精度】
 * 使用 millis() 做非阻塞计时，误差约 ±2ms/秒（取决于主循环频率）。
 * 对于分钟级的倒计时来说完全够用。
 */

#include "study_buddy.h"
#include "game_common.h"   // currentMode, drawMenu
#include "lcd_driver.h"   // LCD 绘图
#include "oled_driver.h"  // OLED 表情和文字
#include "audio.h"        // playSFX_success
#include "keyboard.h"     // 键盘输入

// ============================================================================
// 状态机定义
// ============================================================================
//
// enum（枚举）给每个状态一个有意义的名字
// enum StudyState 类型的变量 state 记录当前在哪个界面
//
enum StudyState {
  STUDY_SELECT,   // 时长选择界面
  STUDY_RUNNING,  // 倒计时运行中
  STUDY_PAUSED,   // 暂停中
  STUDY_DONE      // 计时完成
};

// ============================================================================
// 模块级全局变量
// ============================================================================

static StudyState state;           // 当前状态（状态机核心）
static int timerModes[] = {25, 30, 45, 60};  // 可选的专注时长（分钟）
static int modeCount = 4;          // 选项数量 = 数组长度
static int selectedMode;           // 当前选中的时长索引 (0-3)
static unsigned long totalSeconds; // 选中的总秒数（如 25分钟 = 1500秒）
static unsigned long remainSeconds;// 剩余秒数（倒计时核心变量）
static unsigned long lastTick;     // 上次计时触发的时间戳
static unsigned long startTime;    // 计时开始的时间戳
static int encourageCount;         // 鼓励次数计数器
static unsigned long lastOledBlink;// 上次 OLED 眨眼时间戳

// ============================================================================
// 前向声明（函数在下面实现，需要提前告诉编译器这些函数存在）
// ============================================================================
static void renderSelect();   // 渲染时长选择界面
static void renderTimer();    // 渲染倒计时界面
static void renderDone();     // 渲染完成界面

// ============================================================================
// 公共接口
// ============================================================================

/*
 * studyBuddyInit() — 初始化学习伴侣
 *
 * 进入时长选择界面，OLED 显示专注表情（眨眼动画）。
 */
void studyBuddyInit() {
  state = STUDY_SELECT;           // 初始状态：选择时长
  selectedMode = 0;               // 默认选中第一项（25分钟）
  encourageCount = 0;             // 鼓励计数器归零
  lastOledBlink = millis();       // 记录时间戳
  renderSelect();                 // 绘制选择界面
  // OLED 用眨眼表情（专注睁眼 ↔ 闭眼 交替，800ms 周期）
  oledShowBlinkingFace(FACE_STUDY_A, FACE_STUDY_B, 800);
}

// ============================================================================
// 界面渲染函数
// ============================================================================

/*
 * modeColors[4] — 四个时长选项对应的颜色
 * 25分钟=青色(轻松), 30分钟=绿色(适中), 45分钟=金色(挑战), 60分钟=红色(极限)
 */
static const uint16_t modeColors[4] = {COLOR_CYAN, COLOR_GREEN, COLOR_GOLD, COLOR_ORANGE};

/*
 * renderSelect() — 渲染时长选择界面
 *
 * 布局：
 * ┌──────────────────────┐
 * │    Study Buddy        │ ← 标题栏（海军蓝背景）
 * │   Select Timer        │ ← 副标题
 * │                       │
 * │   ┌──────────────┐    │
 * │   │  25 min      │    │ ← 选项 0（选中=双线边框+颜色）
 * │   └──────────────┘    │
 * │   ┌──────────────┐    │
 * │   │  30 min      │    │ ← 选项 1
 * │   └──────────────┘    │
 * │   ...                 │
 * ├──────────────────────┤
 * │  UP/DOWN  ENTER       │ ← 操作提示
 * └──────────────────────┘
 */
static void renderSelect() {
  lcdClear(COLOR_DARKBG);

  // 标题栏
  lcdFillRect(0, 0, LCD_WIDTH, 45, 0xB7F7);
  lcdSetTextColor(0x24A4,0xB7F7);
  lcdDrawString(30, 12, "Study Buddy", 2);


  // 四个时长选项
  for (int i = 0; i < modeCount; i++) {
    bool sel = (i == selectedMode);  // 是否为选中项
    int y = 95 + i * 55;              // 垂直间距 55px

    if (sel) {
      // 选中状态：双线边框 + 彩色文字
      // 双线框通过画两个紧挨着的矩形实现
      lcdDrawRect(25, y - 2, LCD_WIDTH - 50, 46, 0xC618);
      lcdSetTextColor(modeColors[i], COLOR_DARKBG);
    } else {
      // 未选中：暗绿灰色文字
      lcdFillRect(25, y - 2, LCD_WIDTH - 50, 46, COLOR_DARKBG);
      lcdSetTextColor(modeColors[i], COLOR_DARKBG);
    }

    char buf[24];
    snprintf(buf, sizeof(buf), "%d min", timerModes[i]);
    lcdDrawString(55, y + 4, buf, 2);  // 大字显示时长
  }

  // 底部操作提示
  lcdSetTextColor(0x4208, COLOR_DARKBG);
  lcdDrawString(45, LCD_HEIGHT - 20, "UP/DOWN  ENTER", 1);

  lcdRefresh();
}

/*
 * renderTimer() — 渲染倒计时界面
 *
 * 布局：
 * ┌──────────────────────┐
 * │  Focusing  45%        │ ← 标题栏 + 进度百分比
 * │                       │
 * │      12:30            │ ← 大号时间显示 (MM:SS)
 * │                       │
 * │  ████████░░░░░░░░░░   │ ← 进度条
 * │                       │
 * │  ENTER to pause       │ ← 操作提示
 * └──────────────────────┘
 */
static void renderTimer() {
  lcdClear(COLOR_DARKBG);

  // 计算已完成的百分比
  // 注意：这里用 unsigned long 运算，先乘 100 再除避免精度丢失
  int pct = (int)(100 - ((remainSeconds * 100) / totalSeconds));

  // 标题栏显示进度
  lcdFillRect(0, 0, LCD_WIDTH, 45, 0xB7F7);
  lcdSetTextColor(0x24A4, 0xB7F7);
  char titleBuf[24];
  snprintf(titleBuf, sizeof(titleBuf), "Focusing  %d%%", pct);
  lcdDrawString(25, 12, titleBuf, 2);

  // 大号时间：分钟:秒钟 格式
  int mins = remainSeconds / 60;    // 总秒数 → 分钟（整数除法）
  int secs = remainSeconds % 60;    // 总秒数 → 余数秒数
  char timeBuf[16];
  snprintf(timeBuf, sizeof(timeBuf), "%02d:%02d", mins, secs);
  // %02d 表示"至少两位，不足补 0"：5:03 而不是 5:3
  lcdSetTextColor(0xADEF, COLOR_DARKBG);
  lcdDrawString(55, 100, timeBuf, 4);  // 4 倍大字体

  // 进度条
  int barY = 180;
  int barW = LCD_WIDTH - 40;          // 进度条总宽度（留 20px 边距）

  // 计算已填充的宽度
  // (已过秒数 × 总宽度) ÷ 总秒数 = 填充像素
  int progress = ((totalSeconds - remainSeconds) * barW) / totalSeconds;

  // 画进度条外框
  lcdDrawRect(20, barY, barW, 16, 0x4208);

  // 画进度条填充（留 2px 内边距）
  if (progress > 2) {
    // 进度条颜色随进度变化：<50%青色 → <85%绿色 → ≥85%金色
    uint16_t barColor = (pct < 50) ? COLOR_CYAN :
                        (pct < 85) ? COLOR_GREEN : COLOR_GOLD;
    lcdFillRect(22, barY + 2, progress - 4, 12, barColor);
  }

  // 底部提示
  lcdSetTextColor(0x4208, COLOR_DARKBG);
  lcdDrawString(55, 240, "ENTER to pause", 1);

  lcdRefresh();
}

/*
 * renderDone() — 渲染完成界面
 *
 * 显示"恭喜！"标题、本次会话时长、获得鼓励次数。
 */
static void renderDone() {
  lcdClear(COLOR_DARKBG);

  // 标题栏
  lcdFillRect(0, 0, LCD_WIDTH, 55, COLOR_GREEN);
  lcdSetTextColor(COLOR_GOLD, COLOR_GREEN);
  lcdDrawString(20, 10, "Congratulations!", 3);

  // 完成信息
  lcdSetTextColor(0xADEF, COLOR_DARKBG);
  char buf[32];
  snprintf(buf, sizeof(buf), "Session: %d min", (int)(totalSeconds / 60));
  lcdDrawString(40, 90, buf, 2);

  snprintf(buf, sizeof(buf), "Encouraged: %d times", encourageCount);
  lcdDrawString(30, 130, buf, 1);

  // 装饰分割线
  lcdDrawLine(30, 180, LCD_WIDTH - 30, 180, 0x4208);

  // 继续按钮提示
  lcdSetTextColor(COLOR_CYAN, COLOR_DARKBG);
  lcdDrawString(45, 220, "ENTER to continue", 2);

  lcdRefresh();
}

// ============================================================================
// 主循环
// ============================================================================

/*
 * studyBuddyLoop(key) — 每帧调用，处理学习伴侣逻辑
 *
 * 【按键在不同状态下有不同的含义】
 * - SELECT:  UP/DOWN 选时长, ENTER 确认开始
 * - RUNNING: ENTER 暂停
 * - PAUSED:  ENTER 继续
 * - DONE:    ENTER 回到选择界面
 * - 任何:    BACK 回到主菜单
 *
 * 【非阻塞计时】
 * 不使用 delay()，而是记录 lastTick，每次检查是否过了 1 秒。
 * 这样在倒计时过程中 LCD 显示和 OLED 表情仍然可以更新。
 *
 * @param key  当前按键值
 */
void studyBuddyLoop(uint8_t key) {
  // --- 全局返回键：回到主菜单 ---
  if (key == KEY_BACK) {
    stopTone();           // 停止可能正在播放的音效
    currentMode = MODE_MENU;
    drawMenu();
    oledShowWelcome();
    return;
  }

  // --- 状态机分发 ---
  switch (state) {

    // ============================================================
    // 状态 1: 选择时长
    // ============================================================
    case STUDY_SELECT:
      if (key == KEY_UP) {
        // 向上选择：(current + count - 1) % count 实现循环
        selectedMode = (selectedMode + modeCount - 1) % modeCount;
        renderSelect();
      } else if (key == KEY_DOWN) {
        // 向下选择
        selectedMode = (selectedMode + 1) % modeCount;
        renderSelect();
      } else if (key == KEY_ENTER) {
        // 确认：初始化倒计时并进入运行状态
        totalSeconds = timerModes[selectedMode] * 60UL; // 分钟→秒
        // 注意 60UL 中的 UL 后缀，表示这是一个 unsigned long 类型的数字
        // 避免 25*60 时 16 位整数溢出（虽然 1500 不会溢出，但 60*60=3600 也不会）
        remainSeconds = totalSeconds;
        lastTick = millis();      // 记录开始时间
        startTime = millis();
        state = STUDY_RUNNING;    // 切换到运行状态
        oledShowBlinkingFace(FACE_STUDY_A, FACE_STUDY_B, 800);
        renderTimer();
      }
      break;

    // ============================================================
    // 状态 2: 倒计时运行中
    // ============================================================
    case STUDY_RUNNING:
      // ENTER → 暂停
      if (key == KEY_ENTER) {
        state = STUDY_PAUSED;
        oledShowBlinkingFace(FACE_PAUSE_A, FACE_PAUSE_B, 600);
        lcdDrawString(100, 100, "[PAUSED]");
        lcdRefresh();
        return;
      }

      // 非阻塞计时：检查是否过去了 1 秒
      if (millis() - lastTick >= 1000) {
        lastTick += 1000;  // 增加 1 秒（而不是 = millis()，避免累积误差）

        if (remainSeconds > 0) {
          remainSeconds--;      // 倒计时 -1 秒
          renderTimer();        // 刷新界面

          // 每 5 分钟 (300 秒) 鼓励一次
          if (remainSeconds > 0 && remainSeconds % 300 == 0) {
            encourageCount++;
            // 加快眨眼速度 (400ms) 表示"兴奋/鼓励"
            oledShowBlinkingFace(FACE_STUDY_A, FACE_STUDY_B, 400);
          }
        }

        // 倒计时归零 → 完成！
        if (remainSeconds == 0) {
          state = STUDY_DONE;
          renderDone();
          oledShowBlinkingFace(FACE_DONE_A, FACE_DONE_B, 500);
          playSFX_success();  // 播放庆祝音效
        }
      }
      break;

    // ============================================================
    // 状态 3: 暂停中
    // ============================================================
    case STUDY_PAUSED:
      if (key == KEY_ENTER) {
        state = STUDY_RUNNING;     // 回到运行状态
        lastTick = millis();       // 重置计时基准（避免暂停期间的时间被计入）
        renderTimer();
        oledShowBlinkingFace(FACE_STUDY_A, FACE_STUDY_B, 800);
      }
      break;

    // ============================================================
    // 状态 4: 完成
    // ============================================================
    case STUDY_DONE:
      if (key == KEY_ENTER) {
        studyBuddyInit();  // 回到选择界面，开始新的番茄钟
      }
      break;
  }
}
