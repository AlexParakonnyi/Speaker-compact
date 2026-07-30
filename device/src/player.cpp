#include "player.h"

#include "codec.h"
#include "global.h"
#include "speaker.h"
#include "storage.h"

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
    Serial.println("[SD] Воспроизведение невозможно — карта не смонтирована");
    return;
  }
  stopAudio();
  audioFile = SD_MMC.open("/" + filename, FILE_READ);
  if (!audioFile) {
    Serial.println("Failed to open file for playback: " + filename);
    return;
  }

  activeCodec = codecFromFilename(filename);
  if (activeCodec != CODEC_NONE) {
    // Частоту дискретизации выставит сам codecTick(), разобрав первый
    // фрейм — у MP3/AAC она известна только после декодирования заголовка.
    if (!codecBegin(activeCodec)) {
      Serial.println("[codec] Не удалось выделить буферы декодера: " + filename);
      audioFile.close();
      activeCodec = CODEC_NONE;
      return;
    }
  } else if (filename.endsWith(".wav")) {
    WavInfo info = parseWavHeader(audioFile);
    if (!info.valid) {
      Serial.println("[WAV] Некорректный заголовок: " + filename);
      audioFile.close();
      return;
    }
    if (info.bitsPerSample != 16 || info.channels != 1) {
      Serial.printf("[WAV] Не поддерживается формат (%u-бит, %u канал(ов)) — только моно 16-бит: %s\n",
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
  Serial.println("Playback started: " + filename);
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
