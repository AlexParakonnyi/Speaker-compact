#include "storage.h"

#include <SPI.h>
#include <algorithm>
#include <esp_log.h>

#include "global.h"

static const char* TAG = "storage";

// Купленный модуль — обычный 6-контактный SPI-брейкаут (TF Micro SD Card
// Module for Arduino/ARM/AVR), не SDIO-модуль — поэтому SD.h/SPI.h, не SD_MMC.
// Пины — дефолтные для esp32-s3-devkitc-1 (pins_arduino.h), см. wiring.md.
#define SD_PIN_CS 10
#define SD_PIN_MOSI 11
#define SD_PIN_SCK 12
#define SD_PIN_MISO 13

bool sdMounted = false;
std::vector<TrackInfo> trackList;
SemaphoreHandle_t sdMutex;

static File uploadFile;

// Дефолт библиотеки (SD.begin() без аргумента frequency) — 4 МГц, это
// сильно консервативно и напрямую ограничивает скорость upload/деплоя
// (~4 Мбит/с теоретический потолок SPI, на практике ещё меньше из-за
// протокольных накладных расходов SD-в-SPI-режиме). 20 МГц — типовое
// безопасное значение для большинства SPI SD-модулей; если появятся ошибки
// записи/чтения (нестабильная проводка на брейкауте) — снижать здесь.
static const uint32_t SD_SPI_FREQUENCY_HZ = 20000000;

bool initStorage() {
  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  sdMounted = SD.begin(SD_PIN_CS, SPI, SD_SPI_FREQUENCY_HZ);
  if (sdMounted) {
    ESP_LOGI(TAG, "Card mounted OK");
    if (!SD.exists(TRACKS_DIR)) SD.mkdir(TRACKS_DIR);
  } else {
    ESP_LOGE(TAG, "Card Mount Failed — треки/загрузка работать не будут");
  }
  return sdMounted;
}

String trackPath(const String& name) { return String(TRACKS_DIR) + "/" + name; }

WavInfo parseWavHeader(File& f) {
  WavInfo info;
  uint8_t buf[128];
  size_t n = f.read(buf, sizeof(buf));
  if (n < 12 || memcmp(buf, "RIFF", 4) != 0 || memcmp(buf + 8, "WAVE", 4) != 0) {
    return info;
  }
  size_t pos = 12;
  while (pos + 8 <= n) {
    uint32_t chunkSize;
    memcpy(&chunkSize, buf + pos + 4, 4);
    size_t dataStart = pos + 8;
    if (memcmp(buf + pos, "fmt ", 4) == 0 && dataStart + 16 <= n) {
      memcpy(&info.channels, buf + dataStart + 2, 2);
      memcpy(&info.sampleRate, buf + dataStart + 4, 4);
      memcpy(&info.bitsPerSample, buf + dataStart + 14, 2);
    } else if (memcmp(buf + pos, "data", 4) == 0) {
      info.dataOffset = dataStart;
      info.valid = true;
      return info;
    }
    pos = dataStart + chunkSize + (chunkSize % 2);  // чанки выровнены по 2 байта
  }
  return info;
}

static uint32_t extractTimestamp(const String& name) {
  int start = name.indexOf('_');
  int end = name.indexOf('.');
  if (start < 0 || end < 0 || end <= start + 1) return 0;
  return (uint32_t)name.substring(start + 1, end).toInt();
}

// trackList — общее состояние между loop() (play/delete/rename/clear_all
// через pendingCmd, периодический safety-net refresh) и AsyncTCP-таском
// (handleAudioUploadBody зовёт это напрямую, GET /api/status читает
// trackList на чтение). Без sdMutex здесь GET /api/status мог поймать
// trackList ровно между clear() и повторным наполнением — пустой список
// или "фантомные" записи на стороне фронтенда (мигание списка треков).
void updateTrackList() {
  if (!sdMounted) return;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  trackList.clear();
  File root = SD.open(TRACKS_DIR);
  File file = root.openNextFile();
  while (file) {
    String name = String(file.name());
    bool isWav = name.endsWith(".wav");
    bool isCompressed = name.endsWith(".mp3") || name.endsWith(".aac") || name.endsWith(".m4a");
    if (!file.isDirectory() && (isWav || isCompressed || name.endsWith(".pcm"))) {
      TrackInfo info;
      info.name = name;
      info.sizeBytes = file.size();
      if (isCompressed) {
        info.durationSec = -1;  // не считаем без разбора битрейта/фреймов
      } else {
        uint32_t headerBytes = isWav ? WAV_HEADER_SIZE : 0;
        uint32_t dataBytes = info.sizeBytes > headerBytes ? info.sizeBytes - headerBytes : 0;
        info.durationSec = (float)dataBytes / (SAMPLE_RATE * sizeof(int16_t));
      }
      trackList.push_back(info);
    }
    file = root.openNextFile();
  }
  std::sort(trackList.begin(), trackList.end(),
            [](const TrackInfo& a, const TrackInfo& b) {
              uint32_t ta = extractTimestamp(a.name);
              uint32_t tb = extractTimestamp(b.name);
              if (ta != tb) return ta < tb;
              return a.name < b.name;
            });
  xSemaphoreGive(sdMutex);
}

bool deleteRecording(const String& name) {
  if (!sdMounted) return false;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  bool ok = SD.remove(trackPath(name));
  xSemaphoreGive(sdMutex);
  updateTrackList();  // сам берёт sdMutex — не держим его дважды подряд
  return ok;
}

bool renameRecording(const String& from, const String& to) {
  if (!sdMounted) return false;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  bool ok = SD.rename(trackPath(from), trackPath(to));
  xSemaphoreGive(sdMutex);
  updateTrackList();
  return ok;
}

void clearAllRecordings() {
  if (!sdMounted) return;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  for (const TrackInfo& t : trackList) {
    SD.remove(trackPath(t.name));
  }
  xSemaphoreGive(sdMutex);
  updateTrackList();
}

static void ensureParentDirs(const String& absolutePath) {
  int from = 1;  // пропускаем ведущий '/'
  int slash;
  while ((slash = absolutePath.indexOf('/', from)) != -1) {
    String dir = absolutePath.substring(0, slash);
    if (!SD.exists(dir)) SD.mkdir(dir);
    from = slash + 1;
  }
}

bool storageBeginWrite(const String& absolutePath) {
  if (!sdMounted) return false;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  ensureParentDirs(absolutePath);
  uploadFile = SD.open(absolutePath, FILE_WRITE);
  if (!uploadFile) {
    ESP_LOGE(TAG, "SD.open failed: %s", absolutePath.c_str());
    xSemaphoreGive(sdMutex);
    return false;
  }
  return true;
}

void storageWriteChunk(const uint8_t* data, size_t len) {
  if (uploadFile) uploadFile.write(data, len);
}

void storageEndWrite() {
  if (uploadFile) uploadFile.close();
  xSemaphoreGive(sdMutex);
}
