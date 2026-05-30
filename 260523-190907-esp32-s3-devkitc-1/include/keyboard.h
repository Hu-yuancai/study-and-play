/*
 * keyboard.h - 4x4矩阵键盘驱动
 */
#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <Arduino.h>
#include "config.h"

void initKeyboard();
uint8_t scanKeyboard();
bool isKeyPressed(uint8_t key);

#endif
