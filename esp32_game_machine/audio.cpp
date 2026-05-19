/*
 * audio.cpp - 音频驱动实现
 * 使用ESP32 LEDC PWM通道产生方波驱动LM386功放
 */
#include "audio.h"

void initAudio() {
  ledcSetup(AUDIO_CHANNEL, 2000, 8);
  ledcAttachPin(AUDIO_PIN, AUDIO_CHANNEL);
  ledcWrite(AUDIO_CHANNEL, 0);
}

void playTone(uint16_t frequency, uint16_t duration) {
  if (frequency == 0) {
    ledcWrite(AUDIO_CHANNEL, 0);
    delay(duration);
    return;
  }
  ledcWriteTone(AUDIO_CHANNEL, frequency);
  delay(duration);
  ledcWrite(AUDIO_CHANNEL, 0);
}

void stopTone() {
  ledcWrite(AUDIO_CHANNEL, 0);
}

void playNote(uint8_t noteIndex) {
  static const uint16_t noteFreqs[] = {
    NOTE_C4, NOTE_D4, NOTE_E4, NOTE_F4,
    NOTE_G4, NOTE_A4, NOTE_B4, NOTE_C5
  };
  if (noteIndex < 8) {
    ledcWriteTone(AUDIO_CHANNEL, noteFreqs[noteIndex]);
  }
}

void playSFX_fire() {
  ledcWriteTone(AUDIO_CHANNEL, 1200);
  delay(30);
  ledcWriteTone(AUDIO_CHANNEL, 800);
  delay(30);
  ledcWrite(AUDIO_CHANNEL, 0);
}

void playSFX_explode() {
  for (int f = 300; f > 50; f -= 30) {
    ledcWriteTone(AUDIO_CHANNEL, f);
    delay(20);
  }
  ledcWrite(AUDIO_CHANNEL, 0);
}

void playSFX_success() {
  uint16_t melody[] = {NOTE_C5, NOTE_E5, NOTE_G5, NOTE_C5 * 2};
  for (int i = 0; i < 4; i++) {
    ledcWriteTone(AUDIO_CHANNEL, melody[i]);
    delay(150);
  }
  ledcWrite(AUDIO_CHANNEL, 0);
}
