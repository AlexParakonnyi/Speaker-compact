#ifndef STORAGE_H
#define STORAGE_H

#include <Arduino.h>
#include <FS.h>
#include <SD.h>

#define WAV_HEADER_SIZE 44

struct WavInfo {
  bool valid = false;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
  uint16_t channels = 0;
  uint32_t dataOffset = 0;  // смещение начала аудиоданных в файле
};

// Инициализирует SPI и монтирует SD через SD.h (реальный модуль — SPI-only
// брейкаут, не SDIO, см. device/docs/wiring.md), обновляет
// global.h::sdMounted и возвращает тот же результат.
bool initStorage();

WavInfo parseWavHeader(File& f);

// Обновляет global.h::trackList (имя/размер/длительность), только при
// sdMounted. Распознаёт .wav/.pcm/.mp3/.aac/.m4a.
void updateTrackList();

bool deleteRecording(const String& name);
bool renameRecording(const String& from, const String& to);

// ── Потоковая запись файла на SD (upload аудио, деплой фронтенда) ──────
// Пишет прямо из обработчика AsyncTCP по частям (chunked body) — используют
// глобальный sdMutex (global.h), которым также защищено воспроизведение в
// loop(), чтобы не гоняться за один и тот же SD-ресурс из разных задач.
// Создаёт недостающие директории на пути (нужно для /www/assets/...).
bool storageBeginWrite(const String& absolutePath);
void storageWriteChunk(const uint8_t* data, size_t len);
void storageEndWrite();

#endif
