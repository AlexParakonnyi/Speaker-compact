#include "player.h"

#include <esp_log.h>

#include "codec.h"
#include "global.h"
#include "speaker.h"
#include "storage.h"

static const char* TAG = "player";

volatile State currentState = IDLE;
String currentTrack = "";
File audioFile;

static CodecType activeCodec = CODEC_NONE;  // CODEC_NONE = сырой WAV/PCM путь

void stopAudio() {
  if (currentState == PLAYING) {
    if (activeCodec != CODEC_NONE) {
      codecEnd(activeCodec);
      activeCodec = CODEC_NONE;
    }
    audioFile.close();
    speakerStop();
  }
  currentState = IDLE;
  currentTrack = "";
}

void startPlayback(const String& filename) {
  if (!sdMounted) {
    ESP_LOGE(TAG, "Воспроизведение невозможно — карта не смонтирована");
    return;
  }
  stopAudio();
  audioFile = SD.open(trackPath(filename), FILE_READ);
  if (!audioFile) {
    ESP_LOGE(TAG, "Failed to open file for playback: %s", filename.c_str());
    return;
  }

  activeCodec = codecFromFilename(filename);
  if (activeCodec != CODEC_NONE) {
    // Частоту дискретизации выставит сам codecTick(), разобрав первый
    // фрейм — у MP3/AAC она известна только после декодирования заголовка.
    if (!codecBegin(activeCodec)) {
      ESP_LOGE(TAG, "Не удалось выделить буферы декодера: %s", filename.c_str());
      audioFile.close();
      activeCodec = CODEC_NONE;
      return;
    }
  } else if (filename.endsWith(".wav")) {
    WavInfo info = parseWavHeader(audioFile);
    if (!info.valid) {
      ESP_LOGE(TAG, "Некорректный WAV-заголовок: %s", filename.c_str());
      audioFile.close();
      return;
    }
    if (info.bitsPerSample != 16 || info.channels != 1) {
      ESP_LOGE(TAG, "Не поддерживается формат (%u-бит, %u канал(ов)) — только моно 16-бит: %s",
               info.bitsPerSample, info.channels, filename.c_str());
      audioFile.close();
      return;
    }
    setPlaybackSampleRate(info.sampleRate);
    audioFile.seek(info.dataOffset);
  } else {
    setPlaybackSampleRate(SAMPLE_RATE);  // .pcm — фиксированный формат
  }

  currentTrack = filename;
  currentState = PLAYING;
  speakerStart();  // клоки усилителя — только на время реального воспроизведения
  ESP_LOGI(TAG, "Playback started: %s", filename.c_str());
}

void playbackTick() {
  static uint8_t buf[BLOCK_SIZE_BYTES];
  if (currentState != PLAYING) return;

  bool hasMore;
  if (activeCodec != CODEC_NONE) {
    hasMore = codecTick(activeCodec);
  } else if (audioFile.available()) {
    size_t toRead = audioFile.read(buf, BLOCK_SIZE_BYTES);
    speakerWrite(buf, toRead);
    hasMore = true;
  } else {
    hasMore = false;
  }

  if (!hasMore) stopAudio();  // зацикливание сценария — Plan/08, не часть минимальной версии
}
