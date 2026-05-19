/*
 * piano_game.h - 钢琴游戏模块
 */
#ifndef PIANO_GAME_H
#define PIANO_GAME_H

#include <Arduino.h>
#include "config.h"

void pianoGameInit();
void pianoGameLoop(uint8_t key);

#endif
