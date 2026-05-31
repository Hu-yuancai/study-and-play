/*
 * oled_driver.h — OLED SSD1306 驱动模块（头文件）
 * ============================================================================
 * 【这个模块的作用】
 * 驱动 SSD1306 芯片的 OLED 显示屏（128×64 单色），显示：
 *   - 14 种颜文字(kaomoji)表情（通过 UTF-8 字符串绘制）
 *   - 6 组全屏角色图像（XBM 位图：phanion 系列、mydei 系列）
 *   - 眨眼动画（在两个表情间切换）
 *   - 边框文字（屏幕顶部和底部的说明文字）
 *   - 菜单文本
 *
 * 【硬件说明】
 * SSD1306 OLED 模块通常有 4 个引脚（I2C 版本）：
 *   VCC — 3.3V 电源
 *   GND — 接地
 *   SCL — I2C 时钟线
 *   SDA — I2C 数据线
 *
 * 【分辨率】
 * 128 × 64 像素，每页 8 像素高，共 8 页（page）
 *
 * 【U8g2 库简介】
 * U8g2 (Universal 8-bit Graphics Library 2) 是一个功能强大的单色显示库，
 * 支持数十种 OLED/LCD 控制器，内置大量字体（包括中文字体）。
 * 本项目用它来显示中文菜单文字和颜文字表情（通过 UTF-8 编码）。
 *
 * 【14 种颜文字对照表】
 * 索引 0-5:   钢琴游戏表情（按键反馈、combo、miss）
 * 索引 6-7:   学习模式表情（专注/眨眼）
 * 索引 8-9:   暂停表情
 * 索引 10-11: 完成表情
 * 索引 12-13: 待机表情
 *
 * 【修改颜文字的方法】
 * 直接编辑 oled_driver.cpp 中的 kaomoji_0 ~ kaomoji_13 字符串即可。
 * 字符串支持 ASCII 和 CJK 字符，程序会自动居中显示。
 */

#ifndef OLED_DRIVER_H
#define OLED_DRIVER_H

#include <Arduino.h>
#include <U8g2lib.h>     // OLED 图形库（支持中文等 UTF-8 文本）
#include "config.h"      // OLED_SDA_PIN, OLED_SCL_PIN, OLED_ADDR

// ============================================================================
// 表情类型定义（共 14 种颜文字）
// ============================================================================
// 每个表情是一个 UTF-8 字符串（如 "(^_^)"），通过 U8g2 drawUTF8 渲染

// --- 钢琴游戏表情 (0-5) ---
#define FACE_PIANO_1     0   // 按键按下 → "(o_o)" 默认
#define FACE_PIANO_2     1   // 连击≥5   → "(^-^)" 眨眼
#define FACE_PIANO_4     2   // 连击≥10  → "(O_O)" 惊讶
#define FACE_PIANO_5     3   // combo≥20 → "(-_-)" 酷
#define FACE_PIANO_COMBO 4   // Perfect  → "(*^_^*)" 开心
#define FACE_PIANO_MISS  5   // Miss     → "(>_<)" 失误

// --- 学习模式表情 (6-7) ---
#define FACE_STUDY_A     6   // 专注 — "(._.)" 睁眼
#define FACE_STUDY_B     7   // 专注 — "(-_-)zz" 闭眼（眨眼对）

// --- 暂停表情 (8-9) ---
#define FACE_PAUSE_A     8   // 暂停 — "(o_o;)" 睁眼
#define FACE_PAUSE_B     9   // 暂停 — "(-_-;)" 闭眼

// --- 完成表情 (10-11) ---
#define FACE_DONE_A      10  // 完成 — "\\(^o^)/" 举手欢呼
#define FACE_DONE_B      11  // 完成 — "(^_^)b" 竖拇指

// --- 待机表情 (12-13) ---
#define FACE_IDLE_A      12  // 待机 — "(^_^)" 睁眼（菜单空闲）
#define FACE_IDLE_B      13  // 待机 — "(=_=)" 闭眼（眨眼对）

// --- 向后兼容的别名 ---
// 有些旧代码可能用这些名字，保留这些别名避免编译错误
#define FACE_HAPPY  FACE_PIANO_COMBO   // "开心" -> combo 表情
#define FACE_CHEER  FACE_PIANO_1       // "欢呼" -> 按键 1 表情
#define FACE_FOCUS  FACE_STUDY_A       // "专注" -> 学习睁眼
#define FACE_DONE   FACE_DONE_A        // "完成" -> 完成睁眼
#define FACE_SAD    FACE_PIANO_MISS    // "难过" -> miss 表情

#define FACE_COUNT  14  // 表情总数

// ============================================================================
// 公共函数声明
// ============================================================================

/*
 * initOLED() — 初始化 OLED
 * 启动 I2C 通信，初始化 U8g2 库，设置中文字体
 * 在 setup() 中调用一次
 */
void initOLED();

/*
 * oledClear() — 清空 OLED 缓冲区
 * 注意：这只是清空内存缓冲区，还需要 oledRefresh() 才会显示
 */
void oledClear();

/*
 * oledRefresh() — 将缓冲区内容发送到 OLED
 * 所有绘图操作后都需要调用这个函数才能看到
 */
void oledRefresh();

/*
 * oledDrawString(x, y, str) — 在指定位置显示 UTF-8 字符串
 * 使用 U8g2 的中文字体渲染
 * @param x, y  起始坐标（像素），注意 y 会被自动偏移 12 像素
 * @param str    UTF-8 编码的字符串（支持中文！）
 */
void oledDrawString(int x, int y, const char* str);

/*
 * oledDrawBitmap(x, y, w, h, bitmap) — 绘制 XBM 格式位图
 * @param x, y    左上角坐标
 * @param w, h    宽高
 * @param bitmap  位图数据（XBM 格式，每个 bit=1个像素）
 */
void oledDrawBitmap(int x, int y, int w, int h, const uint8_t* bitmap);

// --- 欢迎界面 ---
void oledShowWelcome();

// --- 表情显示 ---

/*
 * oledShowFace(faceType) — 显示指定类型的表情（16×16 图标 + 边框文字）
 * 同时渲染边框文字（如果有通过 oledDrawBorderText 设置过）
 */
void oledShowFace(uint8_t faceType);

// --- 全屏角色图像 ---

/*
 * 这些函数在 128×64 OLED 上显示全屏像素角色
 * 角色包括：
 *   phanion 系列（phanion, phanion2, phanion3）— 类似狐狸的角色
 *   mydei 系列（mydei1, mydei2, mydei3）— 另一个角色
 *   conflict — 左右键同时按下时的融合角色
 *
 * 图像数据以 XBM 格式存储在 Flash (PROGMEM) 中
 */
void oledShowPhanion();    // 显示 phanion 角色（左一键）
void oledShowPhanion2();   // 显示 phanion2（左二键）
void oledShowPhanion3();   // 显示 phanion3（左一+左二键）
void oledShowMydei1();     // 显示 mydei1（右一键）
void oledShowMydei2();     // 显示 mydei2（右二键）
void oledShowMydei3();     // 显示 mydei3（右一+右二键）
void oledShowConflict();   // 显示冲突角色（左右键同时按）

// --- 眨眼动画 ---

/*
 * oledShowBlinkingFace(faceA, faceB, interval) — 在两个表情间循环切换
 * 利用 millis() 时间戳在 faceA 和 faceB 之间交替显示
 * @param faceA, faceB  两个表情类型（通常是一个睁眼一个闭眼）
 * @param interval      切换间隔（毫秒），interval 越小眨眼越快
 *
 * 使用示例：
 *   oledShowBlinkingFace(FACE_IDLE_A, FACE_IDLE_B, 1000); // 1秒眨一次眼
 */
void oledShowBlinkingFace(uint8_t faceA, uint8_t faceB, uint32_t interval);

// --- 菜单文字 ---

/*
 * oledShowMenuText(line1, line2, line3) — 显示三行菜单文字
 * 用于主菜单中显示歌曲/模式的描述文字
 * @param line1-3  三行文字（支持中文），传入 NULL 跳过某行
 */
void oledShowMenuText(const char* line1, const char* line2, const char* line3);

// --- 边框文字 ---

/*
 * oledDrawBorderText(top, bottom) — 设置顶部和底部的边框文字
 * 文字会被缓存，在下次调用 oledShowFace 时一起绘制
 * 常用于显示分数、提示等信息
 * @param top    顶部文字
 * @param bottom 底部文字
 */
void oledDrawBorderText(const char* top, const char* bottom);

// --- 分数显示 ---
void oledShowScore(int score);

/*
 * getOLED() — 获取 U8g2 对象指针
 * 高级用途：需要直接操作 U8g2 库时使用
 */
U8G2* getOLED();

#endif  // OLED_DRIVER_H 结束
