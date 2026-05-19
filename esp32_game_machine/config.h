/*
 * config.h - 引脚定义与系统配置
 */
#ifndef CONFIG_H
#define CONFIG_H

// ========== LCD12864 (ST7920) SPI引脚 ==========
#define LCD_CS_PIN    5
#define LCD_SCLK_PIN  18
#define LCD_MOSI_PIN  23

// ========== OLED (SSD1306) I2C引脚 ==========
#define OLED_SDA_PIN  21
#define OLED_SCL_PIN  22
#define OLED_ADDR     0x3C

// ========== 4x4矩阵键盘引脚 ==========
#define KB_ROW1  32
#define KB_ROW2  33
#define KB_ROW3  25
#define KB_ROW4  26
#define KB_COL1  27
#define KB_COL2  14
#define KB_COL3  12
#define KB_COL4  13

// ========== 音频输出引脚 (PWM -> LM386) ==========
#define AUDIO_PIN     4
#define AUDIO_CHANNEL 0

// ========== 键值定义 ==========
#define KEY_NONE   0
#define KEY_UP     1
#define KEY_DOWN   2
#define KEY_LEFT   3
#define KEY_RIGHT  4
#define KEY_ENTER  5
#define KEY_BACK   6
#define KEY_FIRE   7
#define KEY_1      8
#define KEY_2      9
#define KEY_3      10
#define KEY_4      11
#define KEY_5      12
#define KEY_6      13
#define KEY_7      14
#define KEY_8      15

// ========== 游戏参数 ==========
#define LCD_WIDTH   128
#define LCD_HEIGHT  64
#define OLED_WIDTH  128
#define OLED_HEIGHT 64

#define FRAME_RATE  60
#define FRAME_MS    (1000 / FRAME_RATE)

#endif
