#include "storage.h"

#include <SPI.h>
#include <algorithm>

#include "global.h"

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

bool initStorage() {
  SPI.begin(SD_PIN_SCK, SD_PIN_MISO, SD_PIN_MOSI, SD_PIN_CS);
  sdMounted = SD.begin(SD_PIN_CS);
  Serial.println(sdMounted ? "[SD] Card mounted OK"
                            : "[SD] Card Mount Failed — треки/загрузка работать не будут");
  return sdMounted;
}

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

void updateTrackList() {
  if (!sdMounted) return;
  trackList.clear();
  File root = SD.open("/");
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
}

bool deleteRecording(const String& name) {
  if (!sdMounted) return false;
  bool ok = SD.remove("/" + name);
  updateTrackList();
  return ok;
}

bool renameRecording(const String& from, const String& to) {
  if (!sdMounted) return false;
  bool ok = SD.rename("/" + from, "/" + to);
  updateTrackList();
  return ok;
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
