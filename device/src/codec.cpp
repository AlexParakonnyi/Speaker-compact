#include "codec.h"

#include "mp3_decoder/mp3_decoder.h"
#include "aac_decoder/aac_decoder.h"

#include "global.h"
#include "speaker.h"

// Логика буферизации/восстановления после ошибки — по образцу
// Audio::sendBytes() из этой же библиотеки (Audio.cpp), но без сети:
// читаем файл вместо чанков HTTP, до нашего собственного speakerWrite()
// вместо m_i2s внутри библиотеки.
#define CODEC_INBUF_SIZE 4096
// MP3: до 2 * 1152 сэмплов/фрейм (стерео). AAC: до 2 * 1024 — укладывается.
#define CODEC_OUTBUF_SAMPLES (2 * 1152)

static uint8_t inBuf[CODEC_INBUF_SIZE];
static int inBufBytes = 0;  // валидных байт в начале inBuf
static short pcmOut[CODEC_OUTBUF_SAMPLES];
static int16_t monoBuf[CODEC_OUTBUF_SAMPLES];
static bool paramsSet = false;

CodecType codecFromFilename(const String& filename) {
  if (filename.endsWith(".mp3")) return CODEC_MP3;
  if (filename.endsWith(".aac") || filename.endsWith(".m4a")) return CODEC_AAC;
  return CODEC_NONE;
}

bool codecBegin(CodecType type) {
  inBufBytes = 0;
  paramsSet = false;
  if (type == CODEC_MP3) return MP3Decoder_AllocateBuffers();
  if (type == CODEC_AAC) return AACDecoder_AllocateBuffers();
  return false;
}

void codecEnd(CodecType type) {
  if (type == CODEC_MP3) MP3Decoder_FreeBuffers();
  if (type == CODEC_AAC) AACDecoder_FreeBuffers();
}

static void refill() {
  if (inBufBytes >= CODEC_INBUF_SIZE) return;  // буфер и так полон
  size_t space = CODEC_INBUF_SIZE - inBufBytes;
  size_t got = audioFile.read(inBuf + inBufBytes, space);
  inBufBytes += got;
}

bool codecTick(CodecType type) {
  refill();
  if (inBufBytes <= 0) return false;  // файл кончился, буфер декодера пуст

  int bytesLeft = inBufBytes;
  int ret = (type == CODEC_MP3) ? MP3Decode(inBuf, &bytesLeft, pcmOut, 0)
                                 : AACDecode(inBuf, &bytesLeft, pcmOut);
  int consumed = inBufBytes - bytesLeft;

  if (ret < 0) {
    // UNDERFLOW на старте/стыке файла не фатален — часто это ID3-тег или
    // недостаточно данных для целого фрейма. Ищем следующее sync-слово,
    // как это делает сама библиотека при поиске начала потока.
    int sync = (type == CODEC_MP3) ? MP3FindSyncWord(inBuf, inBufBytes)
                                    : AACFindSyncWord((uint8_t*)inBuf, inBufBytes);
    if (sync < 0) {
      inBufBytes = 0;  // sync не найден во всём буфере -> сброс, читаем дальше
    } else if (sync > 0) {
      memmove(inBuf, inBuf + sync, inBufBytes - sync);
      inBufBytes -= sync;
    } else if (consumed > 0) {
      memmove(inBuf, inBuf + consumed, inBufBytes - consumed);
      inBufBytes -= consumed;
    } else {
      // Ничего не помогло — откусываем минимум байт, чтобы не зациклиться.
      memmove(inBuf, inBuf + 1, inBufBytes - 1);
      inBufBytes -= 1;
    }
    return true;  // пробуем ещё раз на следующем тике
  }

  if (consumed > 0) {
    memmove(inBuf, inBuf + consumed, inBufBytes - consumed);
    inBufBytes -= consumed;
  }

  int channels = (type == CODEC_MP3) ? MP3GetChannels() : AACGetChannels();
  if (channels <= 0) return true;  // неполный фрейм/тишина — пробуем дальше

  if (!paramsSet) {
    paramsSet = true;
    uint32_t sampRate = (type == CODEC_MP3) ? MP3GetSampRate() : AACGetSampRate();
    setPlaybackSampleRate(sampRate);
    Serial.printf("[codec] %s: %u Hz, %d канал(ов)\n",
                  type == CODEC_MP3 ? "MP3" : "AAC", sampRate, channels);
  }

  int outputSamps = (type == CODEC_MP3) ? MP3GetOutputSamps() : AACGetOutputSamps();
  int framesPerChan = outputSamps / channels;
  if (framesPerChan <= 0 || framesPerChan > CODEC_OUTBUF_SAMPLES) return true;

  // Наш I2S TX настроен как моно -> даунмикс стерео в моно (среднее L+R).
  if (channels == 2) {
    for (int i = 0; i < framesPerChan; i++) {
      monoBuf[i] = (int16_t)(((int32_t)pcmOut[i * 2] + pcmOut[i * 2 + 1]) / 2);
    }
    speakerWrite((uint8_t*)monoBuf, framesPerChan * sizeof(int16_t));
  } else {
    speakerWrite((uint8_t*)pcmOut, framesPerChan * sizeof(int16_t));
  }
  return true;
}
