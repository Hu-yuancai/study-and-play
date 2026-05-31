/*
 * audio.cpp — 音频驱动模块（非阻塞 PWM 音频输出）
 * ============================================================================
 * 【非阻塞架构】
 * 所有播放函数立即返回，不阻塞主循环。
 * 音频状态机在 updateAudio() 中推进，每帧由 main.cpp 调用。
 *
 * 【状态机设计】
 *
 *   playTone(freq, dur)
 *     IDLE ───────────────────→ TONE ──(dur 毫秒后)──→ IDLE
 *
 *   playMelody(steps[], n)
 *     IDLE ───────────────────→ MELODY ──(逐步推进)──→ IDLE
 *
 * 【v5 优先级抢占规则】
 *   新请求优先级 >= 当前优先级 → 抢占; 否则 → 丢弃新请求
 *   stopTone() 无视优先级, 立即静音
 *   默认优先级: playTone/playNote/playMelody() = MEDIUM
 *
 * 【硬件说明】
 * 3 脚无源蜂鸣器模块（低电平触发）：
 *   - HIGH (DUTY_MAX) = 不导通 = 静音
 *   - 50% 占空比方波 = 最大音量
 */

#include <Arduino.h>
#include "audio.h"

// ============================================================================
// 常量和状态
// ============================================================================

#define DUTY_MAX 1023     // 10-bit PWM 最大值 ≈ 100% 占空比 → HIGH → 静音

// 音频状态枚举
enum AudioState {
  AUDIO_IDLE,     // 空闲，没有播放
  AUDIO_TONE,     // 正在播放单个音符
  AUDIO_MELODY    // 正在播放旋律序列
};

static AudioState audioState = AUDIO_IDLE;
static AudioPriority currentPriority = AUDIO_PRIO_LOW;  // 当前播放的优先级

// ── 单音状态 ──
static unsigned long toneStartTime;   // 单音开始的时间戳
static uint16_t     toneDuration;     // 单音计划持续时长

// ── 旋律状态 ──
static MelodyStep melodyQueue[MAX_MELODY_STEPS];  // 旋律步骤数组
static uint8_t     melodyCount;                    // 旋律总步数
static uint8_t     melodyIdx;                      // 当前播放到第几步
static unsigned long melodyStepStart;              // 当前步骤开始的时间戳

// 音符频率表 (playNote 索引, 国际标准音高 A4=440Hz)
//  0=C4 1=D4 2=E4 3=F4 4=G4 5=A4 6=B4 7=C5 8=D5 9=E5 10=F5 11=G5 12=A5
//  哆   来   咪   发   嗦   啦   西   高哆 高来 高咪 高发 高嗦 高啦
static const uint16_t noteFreqs[] = {
  523, 587, 659, 698, 784, 880, 988,        // C4~B4 中音
  1047,1175,1319,1397,1568,1760              // C5~A5 高音
};

// ============================================================================
// 内部辅助：开始输出指定频率
// ============================================================================
static void startFreq(uint16_t freq) {
  if (freq == 0 || freq == NOTE_REST) {
    // 休止符 → 直接静音
    ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
  } else {
    // 播放指定频率（50% 占空比方波，最大音量）
    ledcWriteTone(AUDIO_CHANNEL, freq);
  }
}

// 内部辅助：停止发声，重置状态
static void stopAndIdle() {
  ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
  audioState = AUDIO_IDLE;
}

// ============================================================================
// 公共函数
// ============================================================================

/*
 * initAudio() — 初始化 LEDC 通道
 */
void initAudio() {
  ledcSetup(AUDIO_CHANNEL, 2000, 10);        // 通道0, PWM基频2000Hz, 10-bit分辨率
  ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);    // 绑定到蜂鸣器引脚
  ledcWrite(AUDIO_CHANNEL, DUTY_MAX);         // 初始静音
  audioState = AUDIO_IDLE;
}

/*
 * updateAudio() — 每帧调用，推进非阻塞状态机
 *
 * 逻辑：
 *   - AUDIO_IDLE:  什么都不做
 *   - AUDIO_TONE:  检查是否超时 → 超时则停止并回到 IDLE
 *   - AUDIO_MELODY: 检查当前步骤是否超时 → 超时则推进到下一步
 *                   所有步骤播完 → 停止并回到 IDLE
 */
void updateAudio() {
  unsigned long now = millis();

  switch (audioState) {
    case AUDIO_IDLE:
      return;  // 空闲，什么都不做

    case AUDIO_TONE:
      // 检查单音是否到期
      if (now - toneStartTime >= toneDuration) {
        stopAndIdle();  // 时间到，静音
      }
      break;

    case AUDIO_MELODY:
      // 检查当前步骤是否到期
      if (now - melodyStepStart >= melodyQueue[melodyIdx].dur) {
        melodyIdx++;  // 推进到下一步

        if (melodyIdx >= melodyCount) {
          // 旋律全部播完
          stopAndIdle();
        } else {
          // 播放下一步
          melodyStepStart = now;
          startFreq(melodyQueue[melodyIdx].freq);
        }
      }
      break;
  }
}

/*
 * playTone(frequency, duration) — 非阻塞单音 (默认 MEDIUM 优先级)
 */
void playTone(uint16_t frequency, uint16_t duration) {
  playTonePri(frequency, duration, AUDIO_PRIO_MEDIUM);
}

/*
 * playTonePri(freq, dur, prio) — 带优先级的非阻塞单音
 * 只有 prio >= currentPriority 时才抢占当前音频
 */
void playTonePri(uint16_t frequency, uint16_t duration, AudioPriority prio) {
  // 优先级检查: 新请求优先级不够 → 丢弃
  if (audioState != AUDIO_IDLE && prio < currentPriority) return;

  startFreq(frequency);
  toneStartTime  = millis();
  toneDuration   = duration;
  audioState     = AUDIO_TONE;
  currentPriority = prio;
}

/*
 * playMelody(steps, count) — 非阻塞旋律 (默认 MEDIUM 优先级)
 */
void playMelody(const MelodyStep* steps, uint8_t count) {
  playMelodyPri(steps, count, AUDIO_PRIO_MEDIUM);
}

/*
 * playMelodyPri(steps, count, prio) — 带优先级的非阻塞旋律
 */
void playMelodyPri(const MelodyStep* steps, uint8_t count, AudioPriority prio) {
  if (count == 0 || steps == NULL) return;
  // 优先级检查
  if (audioState != AUDIO_IDLE && prio < currentPriority) return;

  if (count > MAX_MELODY_STEPS) count = MAX_MELODY_STEPS;
  for (uint8_t i = 0; i < count; i++) melodyQueue[i] = steps[i];
  melodyCount = count;
  melodyIdx   = 0;
  melodyStepStart = millis();
  startFreq(melodyQueue[0].freq);
  audioState      = AUDIO_MELODY;
  currentPriority = prio;
}

/*
 * stopTone() — 立即停止 (无视优先级, 重置为 IDLE)
 */
void stopTone() {
  stopAndIdle();
  currentPriority = AUDIO_PRIO_LOW;
}

/*
 * playNote(noteIndex) — 非阻塞播放预设音符
 * 每个音符固定 80ms（适合钢琴游戏的触键反馈）
 */
void playNote(uint8_t noteIndex) {
  if (noteIndex < 13) {
    playTone(noteFreqs[noteIndex], 80);
  }
}

/*
 * isAudioPlaying() — 查询播放状态
 */
bool isAudioPlaying() {
  return audioState != AUDIO_IDLE;
}

// ============================================================================
// 音效函数（非阻塞：构造 MelodyStep 数组 → playMelody）
// ============================================================================

/*
 * playSFX_fire() — "发射"音效 (约 60ms)
 */
void playSFX_fire() {
  static const MelodyStep steps[] = {
    {1200, 30},  // 高音 30ms
    {800,  30},  // 降调 30ms
  };
  playMelodyPri(steps, 2, AUDIO_PRIO_LOW);  // 发射音效=低优先级, 可被覆盖
}

/*
 * playSFX_explode() — "爆炸"音效 (约 180ms, MEDIUM 优先级)
 * 频率从 300Hz 逐步降至 50Hz，模拟轰隆隆的效果
 */
void playSFX_explode() {
  static MelodyStep steps[10];  // 预计算后缓存
  static bool computed = false;

  // 首次调用时计算降调序列
  if (!computed) {
    int idx = 0;
    for (int f = 300; f > 50 && idx < 10; f -= 30) {
      steps[idx].freq = f;
      steps[idx].dur  = 20;
      idx++;
    }
    computed = true;
  }

  // 计算实际步数 (300→50, 步长30 = 约9步)
  int count = 0;
  for (int f = 300; f > 50; f -= 30) count++;
  if (count > 10) count = 10;

  playMelody(steps, count);  // 爆炸音效=MEDIUM (通过 playMelody 默认)
}

/*
 * playSFX_success() — "成功"音效 (约 600ms, HIGH 优先级, 不被打断)
 * 上行四音: C5 → E5 → G5 → C6
 */
void playSFX_success() {
  static const MelodyStep steps[] = {
    {NOTE_C5,     150},
    {NOTE_E5,     150},
    {NOTE_G5,     150},
    {NOTE_C5 * 2, 150},  // C6 = C5 频率的两倍
  };
  playMelodyPri(steps, 4, AUDIO_PRIO_HIGH);  // 胜利旋律=高优先级, 不被打断
}
