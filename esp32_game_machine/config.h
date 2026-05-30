/*
 * config.h - 引脚定义与系统配置
 *
 * ESP32-S3-DevKitC-1 (N16R8) 可用GPIO:
 *   安全可用: 1-21, 38-48 (其中38=板载RGB LED)
 *   被PSRAM占用(N16R8): 33-37
 *   被Flash占用: 26-32
 *   Strapping pins(慎用): 0, 3, 45, 46
 *
 * 引脚分配方案:
 *   I2C:    SDA=GPIO8, SCL=GPIO9
 *   键盘行: GPIO1, GPIO2, GPIO41, GPIO42
 *   键盘列: GPIO39, GPIO40, GPIO47, GPIO48
 *   音频:   GPIO5
 */
#ifndef CONFIG_H
#define CONFIG_H

// ========== LCD12864 (ST7920) SPI引脚 (实物用) ==========
#define LCD_CS_PIN    10
#define LCD_SCLK_PIN  12
#define LCD_MOSI_PIN  11

// ========== OLED (SSD1306) I2C引脚 ==========
#define OLED_SDA_PIN  8
#define OLED_SCL_PIN  9
#define OLED_ADDR     0x3C

// ========== 4x4矩阵键盘引脚 ==========
#define KB_ROW1  1
#define KB_ROW2  2
#define KB_ROW3  41
#define KB_ROW4  42
#define KB_COL1  39
#define KB_COL2  40
#define KB_COL3  47
#define KB_COL4  48

// ========== 音频输出引脚 (PWM -> LM386) ==========
#define AUDIO_PIN     5
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
