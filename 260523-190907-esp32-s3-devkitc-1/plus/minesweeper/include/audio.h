/*
 * audio.h — 音频驱动模块（非阻塞 PWM 音频输出）
 * ============================================================================
 * 【这个模块的作用】
 * 用 ESP32 的 LEDC (LED PWM Controller) 外设驱动无源蜂鸣器，
 * 支持单音播放和旋律序列播放，所有函数均为非阻塞（调用后立即返回）。
 *
 * 【非阻塞设计】
 * 所有播放函数只负责"安排"声音，不等待声音播完。
 * 主循环必须每帧调用 updateAudio() 来推进播放状态。
 *
 * 【状态机】
 * AUDIO_IDLE → playTone(freq,dur) → AUDIO_TONE → dur毫秒后 → IDLE
 * AUDIO_IDLE → playMelody(steps,n) → AUDIO_MELODY → 播完 → IDLE
 *
 * 【使用示例】
 *   initAudio();
 *   playTone(NOTE_C4, 200);  // 安排播放 200ms，立即返回
 *   // 在 loop() 中每帧调用 updateAudio();
 *   updateAudio();  // 推进音频状态机，时间到了自动停
 */

#ifndef AUDIO_H
#define AUDIO_H

#include <Arduino.h>
#include "config.h"      // AUDIO_PIN, AUDIO_CHANNEL

// ============================================================================
// 标准音符频率定义 (国际标准音高 A4=440Hz, 十二平均律)
// ============================================================================
// ESP32 用 ledcWriteTone() 直接设频率, 无需手动算定时器初值.
// 原理: 方波频率 = 音高, 持续时间 = 节拍. 2次翻转=1个完整方波周期.
//
// 索引表 (playNote 使用):
//   0=C4 1=D4 2=E4 3=F4 4=G4 5=A4 6=B4 7=C5 8=D5 9=E5 10=F5 11=G5 12=A5
//   哆   来   咪   发   嗦   啦   西   高哆 高来 高咪 高发 高嗦 高啦

// --- 第三八度 (C3-B3, 低音区) ---
#define NOTE_C3  262   // 低音哆
#define NOTE_D3  294   // 低音来
#define NOTE_E3  330   // 低音咪
#define NOTE_F3  349   // 低音发
#define NOTE_G3  392   // 低音嗦
#define NOTE_A3  440   // 低音啦
#define NOTE_B3  494   // 低音西

// --- 第四八度 (C4-B4, 中音区, playNote 索引 0-6) ---
#define NOTE_C4  523   // 哆 (中央C)
#define NOTE_D4  587   // 来
#define NOTE_E4  659   // 咪
#define NOTE_F4  698   // 发
#define NOTE_G4  784   // 嗦
#define NOTE_A4  880   // 啦
#define NOTE_B4  988   // 西

// --- 第五八度 (C5-B5, 高音区, playNote 索引 7-12) ---
#define NOTE_C5  1047  // 高音哆
#define NOTE_D5  1175  // 高音来
#define NOTE_E5  1319  // 高音咪
#define NOTE_F5  1397  // 高音发
#define NOTE_G5  1568  // 高音嗦
#define NOTE_A5  1760  // 高音啦
#define NOTE_B5  1976  // 高音西

// --- 休止符 ---
#define NOTE_REST 0

// ============================================================================
// 旋律步骤结构体
// ============================================================================
// 一个旋律由多个 MelodyStep 组成的数组定义
// 例如 playSFX_fire() 就是 [{1200,30}, {800,30}] — 两个音符
struct MelodyStep {
  uint16_t freq;   // 频率 (Hz)，0 = 休止
  uint16_t dur;    // 持续时间 (毫秒)
};

#define MAX_MELODY_STEPS 16  // 旋律队列最大长度（覆盖所有 SFX）

// ============================================================================
// 音频优先级 (v5 新增)
// ============================================================================
// 抢占规则: 新请求优先级 >= 当前优先级 → 抢占; 否则 → 丢弃
// stopTone() 无视优先级, 立即静音
enum AudioPriority {
  AUDIO_PRIO_LOW    = 0,  // 可被任意覆盖 (发射音效)
  AUDIO_PRIO_MEDIUM = 1,  // 仅被 MEDIUM/HIGH 覆盖 (按键音符、爆炸)
  AUDIO_PRIO_HIGH   = 2   // 仅被 HIGH 覆盖 (胜利旋律)
};

// ============================================================================
// 公共函数声明
// ============================================================================

/*
 * initAudio() — 初始化音频模块
 * 配置 LEDC 通道，绑定到蜂鸣器引脚，初始静音。
 * 在 setup() 中调用一次。
 */
void initAudio();

/*
 * updateAudio() — 每帧调用，推进非阻塞音频状态机
 * 必须在主循环 loop() 中每帧调用（约每 16ms 一次）。
 * 检查当前音符/旋律步骤是否到期，到期则停止或推进到下一步。
 */
void updateAudio();

/*
 * playTone(frequency, duration) — 非阻塞播放单个音符
 * 启动指定频率的 PWM，安排 duration 毫秒后自动停止。
 * 如果当前正在播放其他音符，会被新音符覆盖（抢占式）。
 *
 * @param frequency  声音频率（Hz），0 = 静音
 * @param duration   持续时间（毫秒）
 */
void playTone(uint16_t frequency, uint16_t duration);

/*
 * playTonePri(freq, dur, prio) — 带优先级的单音播放
 * 只有 prio >= 当前播放优先级时才能抢占
 */
void playTonePri(uint16_t frequency, uint16_t duration, AudioPriority prio);

// playMelody(steps, count) — 非阻塞播放旋律序列 (默认 MEDIUM)
// 使用 playMelodyPri(steps, count, prio) 可指定优先级
// 将旋律步骤数组加载到播放队列，逐步自动推进。
// 新请求优先级 >= 当前优先级时抢占, 否则丢弃
void playMelody(const MelodyStep* steps, uint8_t count);
void playMelodyPri(const MelodyStep* steps, uint8_t count, AudioPriority prio);

/*
 * stopTone() — 立即停止所有音频播放 (无视优先级)
 * 停止 PWM 输出，重置状态机为 IDLE。
 */
void stopTone();

/*
 * playNote(noteIndex) — 非阻塞播放预设音符表中的一个音
 * 索引 0=C4 … 12=A5，每个音固定 80ms。
 *
 * @param noteIndex  0-12 之间的音符索引
 */
void playNote(uint8_t noteIndex);

/*
 * isAudioPlaying() — 查询是否正在播放音频
 * @return true 如果正在播放（单音或旋律）
 */
bool isAudioPlaying();

// --- 音效函数（非阻塞，所有函数安排旋律后立即返回）---

/*
 * playSFX_fire() — "发射"音效
 * 两个降调音: 1200Hz(30ms) → 800Hz(30ms)，总长约 60ms
 */
void playSFX_fire();

/*
 * playSFX_explode() — "爆炸"音效
 * 频率从 300Hz 降至 50Hz, 每步降 30Hz, 每步 20ms，总长约 180ms
 */
void playSFX_explode();

/*
 * playSFX_success() — "成功"音效
 * 上行四音: C5(150ms) → E5(150ms) → G5(150ms) → C6(150ms)，总长约 600ms
 */
void playSFX_success();

#endif
