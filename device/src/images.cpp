#include "images.h"

#include <SD.h>
#include <esp_log.h>

#include "global.h"

static const char* TAG = "images";

String imagePath(const String& trackName) { return String(IMAGES_DIR) + "/" + trackName + ".jpg"; }

bool deleteTrackImage(const String& trackName) {
  if (!sdMounted) return false;
  String path = imagePath(trackName);
  if (!SD.exists(path)) return true;  // нечего удалять — не ошибка
  bool ok = SD.remove(path);
  if (!ok) ESP_LOGE(TAG, "Failed to remove %s", path.c_str());
  return ok;
}

bool renameTrackImage(const String& from, const String& to) {
  if (!sdMounted) return false;
  String fromPath = imagePath(from);
  if (!SD.exists(fromPath)) return true;  // не было картинки — не ошибка
  bool ok = SD.rename(fromPath, imagePath(to));
  if (!ok) ESP_LOGE(TAG, "Failed to rename %s -> %s", fromPath.c_str(), imagePath(to).c_str());
  return ok;
}

void clearAllImages() {
  if (!sdMounted || !SD.exists(IMAGES_DIR)) return;
  File dir = SD.open(IMAGES_DIR);
  if (!dir) return;
  File f = dir.openNextFile();
  while (f) {
    String name = String(f.name());
    bool isDir = f.isDirectory();
    f.close();
    if (!isDir) SD.remove(String(IMAGES_DIR) + "/" + name);
    f = dir.openNextFile();
  }
  dir.close();
}
