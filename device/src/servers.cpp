#include "servers.h"

#include <SD.h>

#include "global.h"
#include "player.h"
#include "speaker.h"
#include "storage.h"

AsyncWebServer server(80);

volatile PendingCmd pendingCmd = CMD_NONE;
String pendingArg1, pendingArg2;

static bool pathSafe(const String& path) {
  return path.length() > 0 && path.indexOf("..") < 0;
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
    if (!storageBeginWrite(path)) {
      request->send(500, "text/plain", "SD write failed to start");
      return;
    }
  }
  storageWriteChunk(data, len);
  if (index + len >= total) {
    storageEndWrite();
    request->send(200, "text/plain", "OK");
  }
}

// Аудио-загрузка с браузера (план 04) — тем же raw-body механизмом, только
// пишем в корень (треки), не в /www/.
static void handleAudioUploadBody(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                   size_t index, size_t total) {
  if (!request->hasParam("name")) {
    request->send(400, "text/plain", "Missing 'name' param");
    return;
  }
  String name = request->getParam("name")->value();
  String path = "/" + name;
  if (!pathSafe(path) || path.indexOf('/', 1) >= 0) {
    request->send(400, "text/plain", "Invalid 'name'");
    return;
  }
  if (index == 0) {
    if (!storageBeginWrite(path)) {
      request->send(500, "text/plain", "SD write failed to start");
      return;
    }
  }
  storageWriteChunk(data, len);
  if (index + len >= total) {
    storageEndWrite();
    updateTrackList();
    request->send(200, "text/plain", "OK");
  }
}

void initServers() {
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    String json = "{\"sdMounted\":" + String(sdMounted ? "true" : "false") + ",";
    json += "\"status\":\"";
    json += currentState == PLAYING ? "PLAYING" : "IDLE";
    json += "\",\"currentTrack\":\"" + currentTrack + "\",";
    json += "\"volume\":" + String(playbackVolume, 2) + ",";
    json += "\"tracks\":[";
    for (size_t i = 0; i < trackList.size(); i++) {
      json += "{\"name\":\"" + trackList[i].name + "\",";
      json += "\"size\":" + String(trackList[i].sizeBytes) + ",";
      json += "\"duration\":" + String(trackList[i].durationSec, 1) + "}";
      json += (i < trackList.size() - 1 ? "," : "");
    }
    json += "]}";
    request->send(200, "application/json", json);
  });

  server.on("/api/play", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("file", true)) {
      request->send(400, "text/plain", "Missing 'file' param");
      return;
    }
    pendingArg1 = request->getParam("file", true)->value();
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
    pendingArg1 = request->getParam("file", true)->value();
    pendingCmd = CMD_DELETE;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/rename", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("from", true) || !request->hasParam("to", true)) {
      request->send(400, "text/plain", "Missing 'from'/'to' param");
      return;
    }
    pendingArg1 = request->getParam("from", true)->value();
    pendingArg2 = request->getParam("to", true)->value();
    pendingCmd = CMD_RENAME;
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
      if (ok && currentTrack == pendingArg1) currentTrack = to;
      break;
    }
    default:
      break;
  }
}
