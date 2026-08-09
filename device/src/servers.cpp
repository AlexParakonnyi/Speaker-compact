#include "servers.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_log.h>

#include "global.h"
#include "groups.h"
#include "player.h"
#include "speaker.h"
#include "storage.h"

static const char* TAG = "servers";

AsyncWebServer server(80);

volatile PendingCmd pendingCmd = CMD_NONE;
String pendingArg1, pendingArg2;

static bool pathSafe(const String& path) {
  return path.length() > 0 && path.indexOf("..") < 0;
}

// Имя трека (не полный путь) — используется в play/delete/rename/upload,
// все они в итоге строят путь через storage.h::trackPath(TRACKS_DIR + name).
// Слэши запрещены полностью: имя не должно указывать ни на подкаталог, ни
// (через "..") наружу из TRACKS_DIR.
static bool trackNameSafe(const String& name) {
  return pathSafe(name) && name.indexOf('/') < 0;
}

// Plan/04: лимит и допустимые расширения проверяются на устройстве, не
// только на фронтенде — фронтенд не единственный возможный клиент API.
static const uint32_t MAX_UPLOAD_BYTES = 20UL * 1024 * 1024;

static bool audioExtensionAllowed(const String& name) {
  return name.endsWith(".wav") || name.endsWith(".mp3") || name.endsWith(".aac") ||
         name.endsWith(".m4a") || name.endsWith(".pcm");
}

// Группы — не имена файлов, слэши/".." тут не проблема (ArduinoJson сам
// экранирует значения при записи groups.json), но пустое имя зарезервировано
// под "снять группу", а разумный лимит длины защищает groups.json от раздувания.
static const size_t MAX_GROUP_NAME_LEN = 64;

static bool groupNameSafe(const String& name) {
  return name.length() > 0 && name.length() <= MAX_GROUP_NAME_LEN;
}

static bool groupExists(const String& name) {
  for (const String& g : groupList) {
    if (g == name) return true;
  }
  return false;
}

// Раздача фронтенда (план 16) пишет сюда файлами по одному, raw body —
// проще на обоих концах, чем multipart/zip. Разрешаем запись только под
// /www/, чтобы хендлер деплоя не мог случайно затереть треки на карте.
static void handleDeployBody(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                              size_t index, size_t total) {
  if (!request->hasParam("path")) {
    request->send(400, "text/plain", "Missing 'path' param");
    return;
  }
  String path = request->getParam("path")->value();
  if (!path.startsWith("/www/") || !pathSafe(path)) {
    request->send(400, "text/plain", "Invalid 'path' (must start with /www/)");
    return;
  }
  if (index == 0) {
    ESP_LOGI(TAG, "deploy start: %s (%u B)", path.c_str(), (unsigned)total);
    if (!storageBeginWrite(path)) {
      request->send(500, "text/plain", "SD write failed to start");
      return;
    }
  }
  storageWriteChunk(data, len);
  if (index + len >= total) {
    storageEndWrite();
    ESP_LOGI(TAG, "deploy done: %s", path.c_str());
    request->send(200, "text/plain", "OK");
  }
}

// Аудио-загрузка с браузера (план 04) — тем же raw-body механизмом, только
// пишем в TRACKS_DIR, не в /www/.
static void handleAudioUploadBody(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                   size_t index, size_t total) {
  if (!request->hasParam("name")) {
    request->send(400, "text/plain", "Missing 'name' param");
    return;
  }
  String name = request->getParam("name")->value();
  if (!trackNameSafe(name)) {
    request->send(400, "text/plain", "Invalid 'name'");
    return;
  }
  String path = trackPath(name);
  if (!audioExtensionAllowed(name)) {
    request->send(400, "text/plain", "Unsupported extension (allowed: .wav/.mp3/.aac/.m4a/.pcm)");
    return;
  }
  if (total > MAX_UPLOAD_BYTES) {
    request->send(413, "text/plain", "File too large (limit 20 MB)");
    return;
  }
  if (index == 0) {
    uint64_t freeBytes = SD.totalBytes() - SD.usedBytes();
    if ((uint64_t)total > freeBytes) {
      ESP_LOGW(TAG, "upload rejected, SD full: need %u B, have %llu B", (unsigned)total, freeBytes);
      request->send(507, "text/plain", "Not enough space on SD card");
      return;
    }
    ESP_LOGI(TAG, "upload start: %s (%u B)", name.c_str(), (unsigned)total);
    if (!storageBeginWrite(path)) {
      request->send(500, "text/plain", "SD write failed to start");
      return;
    }
  }
  storageWriteChunk(data, len);
  if (index + len >= total) {
    storageEndWrite();
    updateTrackList();
    ESP_LOGI(TAG, "upload done: %s", name.c_str());
    request->send(200, "text/plain", "OK");
  }
}

void initServers() {
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    doc["sdMounted"] = sdMounted;
    doc["status"] = currentState == PLAYING ? "PLAYING" : "IDLE";
    doc["currentTrack"] = currentTrack;
    doc["volume"] = playbackVolume;
    JsonArray tracks = doc["tracks"].to<JsonArray>();
    for (const TrackInfo& t : trackList) {
      JsonObject o = tracks.add<JsonObject>();
      o["name"] = t.name;
      o["size"] = t.sizeBytes;
      o["duration"] = t.durationSec;
    }
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/api/groups", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray groups = doc["groups"].to<JsonArray>();
    for (const String& g : groupList) groups.add(g);
    JsonObject assignments = doc["assignments"].to<JsonObject>();
    for (const TrackGroupAssignment& a : trackGroups) assignments[a.track] = a.group;
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
  });

  server.on("/api/play", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("file", true)) {
      request->send(400, "text/plain", "Missing 'file' param");
      return;
    }
    String file = request->getParam("file", true)->value();
    if (!trackNameSafe(file)) {
      request->send(400, "text/plain", "Invalid 'file'");
      return;
    }
    pendingArg1 = file;
    pendingCmd = CMD_PLAY;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
    pendingCmd = CMD_STOP;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/delete", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("file", true)) {
      request->send(400, "text/plain", "Missing 'file' param");
      return;
    }
    String file = request->getParam("file", true)->value();
    if (!trackNameSafe(file)) {
      request->send(400, "text/plain", "Invalid 'file'");
      return;
    }
    pendingArg1 = file;
    pendingCmd = CMD_DELETE;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/rename", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("from", true) || !request->hasParam("to", true)) {
      request->send(400, "text/plain", "Missing 'from'/'to' param");
      return;
    }
    String from = request->getParam("from", true)->value();
    String to = request->getParam("to", true)->value();
    if (!trackNameSafe(from) || !trackNameSafe(to)) {
      request->send(400, "text/plain", "Invalid 'from'/'to'");
      return;
    }
    pendingArg1 = from;
    pendingArg2 = to;
    pendingCmd = CMD_RENAME;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/clear_all", HTTP_POST, [](AsyncWebServerRequest* request) {
    pendingCmd = CMD_CLEAR_ALL;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/groups/create", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing 'name' param");
      return;
    }
    String name = request->getParam("name", true)->value();
    if (!groupNameSafe(name)) {
      request->send(400, "text/plain", "Invalid 'name'");
      return;
    }
    if (groupExists(name)) {
      request->send(409, "text/plain", "Group already exists");
      return;
    }
    pendingArg1 = name;
    pendingCmd = CMD_GROUP_CREATE;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/groups/rename", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("from", true) || !request->hasParam("to", true)) {
      request->send(400, "text/plain", "Missing 'from'/'to' param");
      return;
    }
    String from = request->getParam("from", true)->value();
    String to = request->getParam("to", true)->value();
    if (!groupNameSafe(from) || !groupNameSafe(to)) {
      request->send(400, "text/plain", "Invalid 'from'/'to'");
      return;
    }
    if (!groupExists(from)) {
      request->send(404, "text/plain", "Unknown group");
      return;
    }
    if (groupExists(to)) {
      request->send(409, "text/plain", "Target name already exists");
      return;
    }
    pendingArg1 = from;
    pendingArg2 = to;
    pendingCmd = CMD_GROUP_RENAME;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/groups/delete", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing 'name' param");
      return;
    }
    String name = request->getParam("name", true)->value();
    if (!groupExists(name)) {
      request->send(404, "text/plain", "Unknown group");
      return;
    }
    pendingArg1 = name;
    pendingCmd = CMD_GROUP_DELETE;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/tracks/assign_group", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("file", true) || !request->hasParam("group", true)) {
      request->send(400, "text/plain", "Missing 'file'/'group' param");
      return;
    }
    String file = request->getParam("file", true)->value();
    String group = request->getParam("group", true)->value();
    if (!trackNameSafe(file)) {
      request->send(400, "text/plain", "Invalid 'file'");
      return;
    }
    // group == "" разрешено — это "снять группу", остальное должно совпадать
    // с уже существующей группой (создаётся отдельно через /groups/create).
    if (group.length() > 0 && !groupExists(group)) {
      request->send(404, "text/plain", "Unknown group");
      return;
    }
    bool trackKnown = false;
    for (const TrackInfo& t : trackList) {
      if (t.name == file) {
        trackKnown = true;
        break;
      }
    }
    if (!trackKnown) {
      request->send(404, "text/plain", "Unknown track");
      return;
    }
    pendingArg1 = file;
    pendingArg2 = group;
    pendingCmd = CMD_ASSIGN_GROUP;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (request->hasParam("volume", true)) {
      float v = request->getParam("volume", true)->value().toFloat();
      playbackVolume = constrain(v, 0.0f, 4.0f);
    }
    request->send(200, "text/plain", "OK");
  });

  server.on(
      "/api/deploy-frontend", HTTP_POST,
      [](AsyncWebServerRequest* request) {},  // ответ уходит из onBody, когда файл дописан
      nullptr, handleDeployBody);

  server.on(
      "/api/upload-audio", HTTP_POST,
      [](AsyncWebServerRequest* request) {},
      nullptr, handleAudioUploadBody);

  // Фронтенд деплоится в /www/ (план 16). index.html — вход по умолчанию.
  server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

  server.begin();
  ESP_LOGI(TAG, "HTTP server started on :80");
}

void processPendingCommands() {
  if (pendingCmd == CMD_NONE) return;
  PendingCmd cmd = pendingCmd;
  pendingCmd = CMD_NONE;

  switch (cmd) {
    case CMD_PLAY:
      startPlayback(pendingArg1);
      break;
    case CMD_STOP:
      stopAudio();
      break;
    case CMD_DELETE:
      if (pendingArg1 == currentTrack && currentState != IDLE)
        stopAudio();  // не удаляем открытый файл
      deleteRecording(pendingArg1);
      unassignTrack(pendingArg1);  // storage.cpp не знает про группы — свести здесь
      break;
    case CMD_RENAME: {
      String to = pendingArg2;
      bool hasKnownExt = to.endsWith(".wav") || to.endsWith(".pcm") ||
                         to.endsWith(".mp3") || to.endsWith(".aac") || to.endsWith(".m4a");
      if (!hasKnownExt) {
        int dot = pendingArg1.lastIndexOf('.');
        to += dot >= 0 ? pendingArg1.substring(dot) : ".wav";
      }
      bool ok = renameRecording(pendingArg1, to);
      if (ok) {
        if (currentTrack == pendingArg1) currentTrack = to;
        renameTrackAssignment(pendingArg1, to);
      }
      break;
    }
    case CMD_CLEAR_ALL:
      if (currentState != IDLE) stopAudio();  // не удаляем открытый файл
      clearAllRecordings();
      clearAllAssignments();
      break;
    case CMD_GROUP_CREATE:
      createGroup(pendingArg1);
      break;
    case CMD_GROUP_RENAME:
      renameGroup(pendingArg1, pendingArg2);
      break;
    case CMD_GROUP_DELETE:
      deleteGroup(pendingArg1);
      break;
    case CMD_ASSIGN_GROUP:
      assignTrackGroup(pendingArg1, pendingArg2);
      break;
    default:
      break;
  }
}
