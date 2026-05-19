/*
 * game_common.h - 共享类型定义和全局变量声明
 */
#ifndef GAME_COMMON_H
#define GAME_COMMON_H

enum GameMode {
  MODE_MENU,
  MODE_PIANO,
  MODE_PLANE,
  MODE_STUDY
};

extern GameMode currentMode;
extern void drawMenu();

#endif
