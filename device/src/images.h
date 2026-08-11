#ifndef IMAGES_H
#define IMAGES_H

#include <Arduino.h>

// Реальная FAT-директория (в отличие от TRACKS_DIR группировка треков не
// нужна — картинка физически привязана к треку 1:1, план 06/CLAUDE.md §2.2).
#define IMAGES_DIR "/images"

// IMAGES_DIR + "/" + trackName + ".jpg" — конкатенация имени целиком, не
// замена расширения: иначе "song.wav" и "song.mp3" делили бы один файл.
String imagePath(const String& trackName);

bool deleteTrackImage(const String& trackName);
bool renameTrackImage(const String& from, const String& to);
void clearAllImages();

#endif
