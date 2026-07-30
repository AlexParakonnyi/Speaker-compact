#ifndef CODEC_H
#define CODEC_H

#include <Arduino.h>

enum CodecType { CODEC_NONE, CODEC_MP3, CODEC_AAC };

// По расширению файла: .mp3 -> CODEC_MP3, .aac/.m4a -> CODEC_AAC, иначе NONE
// (значит raw PCM/WAV — не сюда, см. player.cpp).
CodecType codecFromFilename(const String& filename);

// Выделяет буферы декодера под конкретный кодек. false, если не хватило памяти.
bool codecBegin(CodecType type);
void codecEnd(CodecType type);

// Декодирует и проигрывает (через speakerWrite(), с нашей громкостью/clamp)
// очередной блок из уже открытого global.h::audioFile. Дискретизацию I2S
// выставляет сама, разобрав первый фрейм. Возвращает false, когда поток
// закончился (файл иссяк и в буфере декодера пусто).
bool codecTick(CodecType type);

#endif
