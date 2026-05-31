/*
 * lcd_driver.h — ILI9341 TFT LCD 驱动模块 (脏矩形优化 + 帧缓冲)
 * ============================================================================
 * 【v3 优化】脏矩形 (Dirty Rect) 机制:
 *   所有绘图操作自动追踪被修改区域, lcdRefresh() 仅传输变化部分,
 *   将 SPI 传输量从 150KB/帧 降至 5-20KB/帧 (降低 70-95%)。
 */

#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <Arduino.h>
#include <Adafruit_ILI9341.h>
#include <Adafruit_GFX.h>
#include "config.h"

// ========== 颜色常量 (RGB565) ==========
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_ORANGE      0xFD20
#define COLOR_GOLD        0xFEA0
#define COLOR_GRAY        0x8410
#define COLOR_DARKGRAY    0x4208
#define COLOR_NAVY        0x000F
#define COLOR_DARKGREEN   0x03E0
#define COLOR_PURPLE      0x8010
#define COLOR_PINK        0xF81F
#define COLOR_DARKBG      0x1082
#define COLOR_LANE_BG     0x18E3
#define COLOR_JUDGELINE   0x07FF

// ========== 轨道颜色 ==========
#define LANE_COLOR_0  COLOR_RED
#define LANE_COLOR_1  COLOR_BLUE
#define LANE_COLOR_2  COLOR_GREEN
#define LANE_COLOR_3  COLOR_YELLOW

// ============================================================================
// 公共接口
// ============================================================================

void initLCD();
void lcdClear(uint16_t color = COLOR_BLACK);

/*
 * lcdRefresh() — 脏矩形优化刷新
 * 自动选择全屏或区域刷新: 脏区面积 < 50% 全屏 → 区域刷新, 否则 → 全屏刷新
 */
void lcdRefresh();

void lcdSetTextColor(uint16_t fg, uint16_t bg = COLOR_BLACK);
void lcdDrawString(int x, int y, const char* str, uint8_t size = 1);
void lcdDrawText(int x, int y, const char* str, uint8_t size);
void lcdDrawPixel(int x, int y, uint16_t color = COLOR_WHITE);
void lcdDrawRect(int x, int y, int w, int h, uint16_t color = COLOR_WHITE);
void lcdFillRect(int x, int y, int w, int h, uint16_t color = COLOR_WHITE);
void lcdDrawLine(int x0, int y0, int x1, int y1, uint16_t color = COLOR_WHITE);
void lcdDrawBitmap(int x, int y, int w, int h, const uint8_t* bitmap, uint16_t color = COLOR_WHITE);
void lcdFillScreen(uint16_t color);
GFXcanvas16* getLCD();

#endif
