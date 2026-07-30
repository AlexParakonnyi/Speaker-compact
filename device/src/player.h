#ifndef PLAYER_H
#define PLAYER_H

#include <Arduino.h>

// Минимальная версия: только preview-воспроизведение (нет записи, нет VAD,
// нет плейлиста/сценариев — это Plan/07-08, добавится позже поверх этого же
// файла).
void stopAudio();
void startPlayback(const String& filename);

// Вызывается из loop(): продолжает чтение/декодирование текущего файла, пока
// currentState == PLAYING.
void playbackTick();

#endif
