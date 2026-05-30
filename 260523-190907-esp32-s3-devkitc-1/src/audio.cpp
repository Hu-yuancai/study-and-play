/*
 * audio.cpp - PWM音频驱动 (无源蜂鸣器, 低电平触发)
 * 蜂鸣器3引脚: VCC / GND / IO(信号)
 * 电路: VCC→蜂鸣器→IO, 当IO=LOW时导通发声
 * 因此: 静音=IO HIGH, 发声=PWM 50%占空比
 */
#include <Arduino.h>
#include "audio.h"

#define DUTY_MAX 1023  // 10-bit PWM → 100% duty → IO HIGH → 静音

void initAudio() {
    ledcSetup(AUDIO_CHANNEL, 2000, 10);
    ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);
    ledcWrite(AUDIO_CHANNEL, DUTY_MAX);  // HIGH = 低电平触发下静音
}

void playTone(uint16_t frequency, uint16_t duration) {
    if (frequency == 0) {
        ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
        delay(duration);
        return;
    }
    ledcWriteTone(AUDIO_CHANNEL, frequency);  // 50% duty 方波
    delay(duration);
    ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
}

void stopTone() {
    ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
}

void playNote(uint8_t noteIndex) {
    static const uint16_t noteFreqs[] = {
        NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4,
        NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5,
        NOTE_D5, NOTE_E5, NOTE_F5, NOTE_G5, NOTE_A5
    };
    if (noteIndex < 13) {
        ledcWriteTone(AUDIO_CHANNEL, noteFreqs[noteIndex]);
        delay(80);
        ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
    }
}

void playSFX_fire() {
    ledcWriteTone(AUDIO_CHANNEL, 1200);
    delay(30);
    ledcWriteTone(AUDIO_CHANNEL, 800);
    delay(30);
    ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
}

void playSFX_explode() {
    for (int f = 300; f > 50; f -= 30) {
        ledcWriteTone(AUDIO_CHANNEL, f);
        delay(20);
    }
    ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
}

void playSFX_success() {
    uint16_t melody[] = {NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C5 * 2};
    for (int i = 0; i < 4; i++) {
        ledcWriteTone(AUDIO_CHANNEL, melody[i]);
        delay(150);
    }
    ledcWrite(AUDIO_CHANNEL, DUTY_MAX);
}
