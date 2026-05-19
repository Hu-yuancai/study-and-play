/*
 * lcd_driver.h - LCD12864 (ST7920) SPI驱动
 */
#ifndef LCD_DRIVER_H
#define LCD_DRIVER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "config.h"

void initLCD();
void lcdClear();
void lcdRefresh();
void lcdDrawString(int x, int y, const char* str);
void lcdDrawPixel(int x, int y);
void lcdDrawRect(int x, int y, int w, int h);
void lcdFillRect(int x, int y, int w, int h);
void lcdDrawLine(int x0, int y0, int x1, int y1);
void lcdDrawBitmap(int x, int y, int w, int h, const uint8_t* bitmap);
U8G2* getLCD();

#endif
