#ifndef SPEAKER_H
#define SPEAKER_H

#include <Arduino.h>
#include <driver/i2s.h>

// Усилитель MAX98357A -> I2S_NUM_1, только передача (TX). Тот же усилитель и
// та же распиновка, что и в audio_test — см. device/docs/wiring.md.
#define I2S_AMP_DIN GPIO_NUM_41
#define I2S_AMP_LRC GPIO_NUM_47
#define I2S_AMP_BCLK GPIO_NUM_21
#define I2S_AMP_PORT I2S_NUM_1

void initSpeaker();

// В режиме master i2s_driver_install сразу и непрерывно генерирует BCLK/LRC,
// даже если ничего не пишется через i2s_write — глушим клоки, пока реально
// не играем, и включаем обратно на время воспроизведения.
void speakerStart();
void speakerStop();

void setPlaybackSampleRate(uint32_t rate);

// Применяет playbackVolume + насыщение по int16_t (без wraparound) и пишет в I2S.
size_t speakerWrite(uint8_t* buf, size_t len);

#endif
