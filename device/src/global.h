#ifndef GLOBAL_H
#define GLOBAL_H

#include <Arduino.h>
#include <FS.h>

#include <vector>

// Общие аудио-константы.
#define SAMPLE_RATE 16000
#define BLOCK_SIZE_BYTES 1024  // порция чтения/записи за один проход (16-бит PCM)

// ── Состояние воспроизведения ──────────────────────────────────────────
// Минимальная версия: только превью-проигрывание с фронтенда, без сценария
// (сценарий/deep sleep — следующие пункты плана, см. Plan/07, Plan/08, Plan/11).
enum State { IDLE, PLAYING };
extern volatile State currentState;
extern String currentTrack;

struct TrackInfo {
  String name;
  uint32_t sizeBytes;
  float durationSec;  // -1, если не посчитана (сжатые форматы, см. storage.cpp)
};
extern std::vector<TrackInfo> trackList;
extern bool sdMounted;

// AsyncWebServer выполняет обработчики в отдельной задаче (AsyncTCP), не в
// Arduino loop(). Короткие мутирующие команды (play/stop/delete/rename) идут
// через очередь и выполняются строго в loop() — тот же принцип, что и в
// audio_test (см. CLAUDE.md §4.2).
enum PendingCmd { CMD_NONE, CMD_PLAY, CMD_STOP, CMD_DELETE, CMD_RENAME };
extern volatile PendingCmd pendingCmd;
extern String pendingArg1, pendingArg2;

// Потоковая загрузка (upload аудио, деплой фронтенда) пишет на SD прямо из
// обработчика AsyncTCP по частям (chunked body) — так проще, чем гонять
// целый файл через pendingCmd. Вместо этого используем общий мьютекс: любая
// работа с SD (в т.ч. voспроизведение в loop()) берёт его на время операции.
extern SemaphoreHandle_t sdMutex;

extern float playbackVolume;  // 0.0 - 4.0, пока фиксированное значение по умолчанию

static inline int16_t clampToInt16(int32_t v) {
  if (v > 32767) return 32767;
  if (v < -32768) return -32768;
  return (int16_t)v;
}

extern File audioFile;

#endif
