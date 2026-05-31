/*
 * game_common.h - 共享类型定义和全局变量声明
 */
#ifndef GAME_COMMON_H
#define GAME_COMMON_H

#include "config.h"

enum GameMode {
  MODE_MENU,
  MODE_PIANO,
  MODE_PLANE,
  MODE_MINESWEEPER,   // 扫雷游戏
  MODE_SNAKE,         // 贪吃蛇
  MODE_TETRIS,        // 俄罗斯方块
  MODE_2048,          // 2048
  MODE_STUDY,
  MODE_SHELL          // Ghost Shell 隐藏调试终端 (Konami码进入)
};

extern GameMode currentMode;
extern void drawMenu();

#endif
