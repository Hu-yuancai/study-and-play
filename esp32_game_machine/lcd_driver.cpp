/*
 * lcd_driver.cpp - LCD12864 (ST7920) SPI驱动实现
 * 使用U8g2库驱动ST7920控制器的128x64点阵LCD
 *
 * 仿真模式 (WOKWI_SIM): Wokwi 没有 ST7920 元件，
 *   改用 SSD1306 I2C (地址 0x3D) 替代，副屏 OLED 用 0x3C
 */
#include <Wire.h>
#include "lcd_driver.h"

#ifdef WOKWI_SIM
// 仿真：用第二块 SSD1306 (I2C 地址 0x3D) 当作主屏
static U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2_lcd(U8G2_R0, U8X8_PIN_NONE);
#else
// 实物：真实 ST7920 SPI 接线
static U8G2_ST7920_128X64_F_SW_SPI u8g2_lcd(
  U8G2_R0,
  LCD_SCLK_PIN,
  LCD_MOSI_PIN,
  LCD_CS_PIN,
  U8X8_PIN_NONE
);
#endif

void initLCD() {
#ifdef WOKWI_SIM
  Wire.begin(OLED_SDA_PIN, OLED_SCL_PIN);
  u8g2_lcd.setI2CAddress(0x3D * 2);
#endif
  u8g2_lcd.begin();
  u8g2_lcd.enableUTF8Print();
  u8g2_lcd.setFont(u8g2_font_wqy12_t_gb2312);
  u8g2_lcd.clearBuffer();
  u8g2_lcd.sendBuffer();
}

void lcdClear() {
  u8g2_lcd.clearBuffer();
}

void lcdRefresh() {
  u8g2_lcd.sendBuffer();
}

void lcdDrawString(int x, int y, const char* str) {
  u8g2_lcd.drawUTF8(x, y + 12, str);
}

void lcdDrawPixel(int x, int y) {
  u8g2_lcd.drawPixel(x, y);
}

void lcdDrawRect(int x, int y, int w, int h) {
  u8g2_lcd.drawFrame(x, y, w, h);
}

void lcdFillRect(int x, int y, int w, int h) {
  u8g2_lcd.drawBox(x, y, w, h);
}

void lcdDrawLine(int x0, int y0, int x1, int y1) {
  u8g2_lcd.drawLine(x0, y0, x1, y1);
}

void lcdDrawBitmap(int x, int y, int w, int h, const uint8_t* bitmap) {
  u8g2_lcd.drawXBM(x, y, w, h, bitmap);
}

U8G2* getLCD() {
  return &u8g2_lcd;
}
