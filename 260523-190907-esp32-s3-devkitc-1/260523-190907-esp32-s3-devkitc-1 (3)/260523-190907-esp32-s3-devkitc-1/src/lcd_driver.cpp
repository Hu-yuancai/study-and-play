/*
 * lcd_driver.cpp — ILI9341 LCD 驱动 (脏矩形优化版)
 * ============================================================================
 * 【脏矩形机制】
 *   - 每次绘图操作自动标记被修改区域 (lcdMarkDirty)
 *   - lcdRefresh() 仅发送脏区像素到屏幕, 大幅降低 SPI 传输量
 *   - 脏区面积 > 50% 全屏时自动回退全屏刷新
 *   - lcdClear() 后强制全屏刷新 (画布完全改变)
 */

#include "lcd_driver.h"
#include <SPI.h>

// ── 物理屏幕 + 帧缓冲 ─────────────────────────────────────────
static Adafruit_ILI9341 tft(LCD_CS_PIN, LCD_DC_PIN, LCD_RST_PIN);
static GFXcanvas16 canvas(LCD_WIDTH, LCD_HEIGHT);
static uint16_t currentTextColor = COLOR_WHITE;
static uint16_t currentBgColor   = COLOR_BLACK;

// ── 脏矩形追踪 ────────────────────────────────────────────────
static int dirtyX1, dirtyY1, dirtyX2, dirtyY2;
static bool dirtyValid = false;   // false = 首次/清屏后, 需要全屏刷新

// 将指定矩形区域合并到脏矩形
static void lcdMarkDirty(int x, int y, int w, int h) {
  if (w <= 0 || h <= 0) return;
  // 裁剪到屏幕范围
  if (x < 0) { w += x; x = 0; }
  if (y < 0) { h += y; y = 0; }
  if (x + w > LCD_WIDTH)  w = LCD_WIDTH - x;
  if (y + h > LCD_HEIGHT) h = LCD_HEIGHT - y;
  if (w <= 0 || h <= 0) return;

  int x2 = x + w - 1, y2 = y + h - 1;

  if (!dirtyValid) {
    dirtyX1 = x; dirtyY1 = y;
    dirtyX2 = x2; dirtyY2 = y2;
    dirtyValid = true;
  } else {
    if (x  < dirtyX1) dirtyX1 = x;
    if (y  < dirtyY1) dirtyY1 = y;
    if (x2 > dirtyX2) dirtyX2 = x2;
    if (y2 > dirtyY2) dirtyY2 = y2;
  }
}

// ============================================================================
// 初始化
// ============================================================================
void initLCD() {
  SPI.begin(LCD_SCLK_PIN, LCD_MISO_PIN, LCD_MOSI_PIN, LCD_CS_PIN);
  tft.begin();
  tft.setRotation(0);
  tft.fillScreen(COLOR_BLACK);
  canvas.fillScreen(COLOR_BLACK);
  canvas.setTextColor(COLOR_WHITE, COLOR_BLACK);
  canvas.setTextSize(1);
}

// ============================================================================
// 清屏 (强制全屏刷新)
// ============================================================================
void lcdClear(uint16_t color) {
  canvas.fillScreen(color);
  currentBgColor = color;
  dirtyValid = false;  // 清屏后必须全屏刷新
}

// ============================================================================
// 脏矩形优化刷新
// ============================================================================
void lcdRefresh() {
  if (!dirtyValid) {
    // ── 全屏刷新 ──
    tft.drawRGBBitmap(0, 0, canvas.getBuffer(), LCD_WIDTH, LCD_HEIGHT);
  } else {
    int dw = dirtyX2 - dirtyX1 + 1;
    int dh = dirtyY2 - dirtyY1 + 1;
    int area = dw * dh;
    int fullArea = LCD_WIDTH * LCD_HEIGHT;

    if (area > fullArea / 2) {
      // 脏区超过 50% → 全屏刷新更高效 (避免多次 SPI 事务开销)
      tft.drawRGBBitmap(0, 0, canvas.getBuffer(), LCD_WIDTH, LCD_HEIGHT);
    } else {
      // ── 区域刷新: setAddrWindow + 逐行发送 ──
      tft.startWrite();
      tft.setAddrWindow(dirtyX1, dirtyY1, dw, dh);
      for (int row = dirtyY1; row <= dirtyY2; row++) {
        tft.writePixels(
          (uint16_t*)&canvas.getBuffer()[row * LCD_WIDTH + dirtyX1], dw);
      }
      tft.endWrite();
    }
  }
  // 重置脏矩形
  dirtyValid = false;
}

// ============================================================================
// 文字绘制 (自动标记脏区)
// ============================================================================
void lcdSetTextColor(uint16_t fg, uint16_t bg) {
  currentTextColor = fg;
  currentBgColor = bg;
  canvas.setTextColor(fg, bg);
}

// 估算文字像素大小: 默认字体约 6×8 像素/字符, 乘 size 倍
static void markTextDirty(int x, int y, const char* str, uint8_t size) {
  int len = strlen(str);
  if (len == 0) return;
  int tw = 6 * size * len;   // 宽
  int th = 8 * size;          // 高
  // U8g2/GFX 的文本基线在 y+th 附近, 标记完整区域
  lcdMarkDirty(x, y, tw, th);
}

void lcdDrawString(int x, int y, const char* str, uint8_t size) {
  canvas.setTextSize(size);
  canvas.setCursor(x, y);
  canvas.print(str);
  markTextDirty(x, y, str, size);
}

void lcdDrawText(int x, int y, const char* str, uint8_t size) {
  lcdDrawString(x, y, str, size);
}

// ============================================================================
// 图形绘制 (自动标记脏区)
// ============================================================================
void lcdDrawPixel(int x, int y, uint16_t color) {
  canvas.drawPixel(x, y, color);
  lcdMarkDirty(x, y, 1, 1);
}

void lcdDrawRect(int x, int y, int w, int h, uint16_t color) {
  canvas.drawRect(x, y, w, h, color);
  lcdMarkDirty(x, y, w, h);
}

void lcdFillRect(int x, int y, int w, int h, uint16_t color) {
  canvas.fillRect(x, y, w, h, color);
  lcdMarkDirty(x, y, w, h);
}

void lcdDrawLine(int x0, int y0, int x1, int y1, uint16_t color) {
  canvas.drawLine(x0, y0, x1, y1, color);
  int x = (x0 < x1) ? x0 : x1;
  int y = (y0 < y1) ? y0 : y1;
  int w = abs(x1 - x0) + 1;
  int h = abs(y1 - y0) + 1;
  lcdMarkDirty(x, y, w, h);
}

void lcdDrawBitmap(int x, int y, int w, int h, const uint8_t* bitmap, uint16_t color) {
  canvas.drawBitmap(x, y, bitmap, w, h, color);
  lcdMarkDirty(x, y, w, h);
}

void lcdFillScreen(uint16_t color) {
  canvas.fillScreen(color);
  dirtyValid = false;  // 全屏变化
}

GFXcanvas16* getLCD() {
  return &canvas;
}
