#include "servers.h"

#include <ArduinoJson.h>
#include <AsyncJson.h>
#include <SD.h>
#include <esp_log.h>

#include "battery.h"
#include "global.h"
#include "groups.h"
#include "images.h"
#include "player.h"
#include "scenario.h"
#include "speaker.h"
#include "storage.h"

static const char* TAG = "servers";

AsyncWebServer server(80);

volatile PendingCmd pendingCmd = CMD_NONE;
String pendingArg1, pendingArg2;
Scenario pendingScenario;

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

static bool trackExists(const String& name) {
  for (const TrackInfo& t : trackList) {
    if (t.name == name) return true;
  }
  return false;
}

// Имя сценария — становится именем файла (scenario.cpp::scenarioPath), та же
// проверка, что и для треков (trackNameSafe), плюс лимит длины как у групп.
static bool scenarioNameSafe(const String& name) {
  return pathSafe(name) && name.indexOf('/') < 0 && name.length() <= MAX_GROUP_NAME_LEN;
}

// Plan/06: фронтенд ресайзит перед отправкой (~480-600px, JPEG ~80% — обычно
// десятки КБ), но лимит на устройстве — не доверять фронтенду единственной
// линией защиты (см. CLAUDE.md §4.5.1).
static const uint32_t MAX_IMAGE_BYTES = 500UL * 1024;

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

// Картинка трека (план 06) — тем же raw-body механизмом, в IMAGES_DIR.
// Клиент уже прислал ресайзнутый JPEG (см. frontend/src/lib/image.ts),
// здесь только лимит размера + проверка, что трек реально существует.
static void handleImageUploadBody(AsyncWebServerRequest* request, uint8_t* data, size_t len,
                                   size_t index, size_t total) {
  if (!request->hasParam("file")) {
    request->send(400, "text/plain", "Missing 'file' param");
    return;
  }
  String file = request->getParam("file")->value();
  if (!trackNameSafe(file)) {
    request->send(400, "text/plain", "Invalid 'file'");
    return;
  }
  if (total > MAX_IMAGE_BYTES) {
    request->send(413, "text/plain", "Image too large (limit 500 KB)");
    return;
  }
  if (index == 0) {
    if (!trackExists(file)) {
      request->send(404, "text/plain", "Unknown track");
      return;
    }
    ESP_LOGI(TAG, "image upload start: %s (%u B)", file.c_str(), (unsigned)total);
    if (!storageBeginWrite(imagePath(file))) {
      request->send(500, "text/plain", "SD write failed to start");
      return;
    }
  }
  storageWriteChunk(data, len);
  if (index + len >= total) {
    storageEndWrite();
    ESP_LOGI(TAG, "image upload done: %s", file.c_str());
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
    // trackList мутируется из AsyncTCP-таска тоже (upload), не только из
    // loop() — тот же sdMutex, что и в storage.cpp::updateTrackList(),
    // иначе можно поймать пустой/частично обновлённый список на чтении.
    xSemaphoreTake(sdMutex, portMAX_DELAY);
    for (const TrackInfo& t : trackList) {
      JsonObject o = tracks.add<JsonObject>();
      o["name"] = t.name;
      o["size"] = t.sizeBytes;
      o["duration"] = t.durationSec;
    }
    xSemaphoreGive(sdMutex);
    // План 08 (интерим без deep sleep) — виден статус активного сценария,
    // чтобы можно было наблюдать прогресс без отдельного эндпоинта.
    doc["scenarioActive"] = scenarioActive;
    doc["activeScenario"] = scenarioActive ? activeScenarioName : "";
    // Кэш из battery.cpp, обновляется раз в 10с из loop() — см. предупреждение
    // там же про неоткалиброванный делитель (нет ещё реальной батареи).
    doc["batteryValid"] = batteryReading.valid;
    doc["batteryVoltage"] = batteryReading.voltage;
    doc["batteryPercent"] = batteryReading.percent;
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

  server.on("/api/scenarios", HTTP_GET, [](AsyncWebServerRequest* request) {
    JsonDocument doc;
    JsonArray scenarios = doc["scenarios"].to<JsonArray>();
    for (const String& name : listScenarioNames()) {
      Scenario s;
      if (!loadScenario(name, s)) continue;  // повреждённый файл — пропустить, не падать
      JsonObject o = scenarios.add<JsonObject>();
      o["name"] = s.name;
      o["startDelaySec"] = s.startDelaySec;
      o["loop"] = s.loop;
      JsonArray steps = o["steps"].to<JsonArray>();
      for (const ScenarioStep& step : s.steps) {
        JsonObject so = steps.add<JsonObject>();
        so["file"] = step.file;
        so["delayAfterPrevSec"] = step.delayAfterPrevSec;
        so["volume"] = step.volume;
      }
    }
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

  server.on("/api/scenarios/delete", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing 'name' param");
      return;
    }
    String name = request->getParam("name", true)->value();
    if (!scenarioNameSafe(name)) {
      request->send(400, "text/plain", "Invalid 'name'");
      return;
    }
    pendingArg1 = name;
    pendingCmd = CMD_SCENARIO_DELETE;
    request->send(200, "text/plain", "OK");
  });

  // Плана 09/10 (AP-режим настройки, отключение сети при активации) ещё
  // нет — это временный минимум, чтобы вообще можно было проверить движок
  // из плана 08. Полноценная "активация" (гашение AP и т.д.) — план 10.
  server.on("/api/scenarios/activate", HTTP_POST, [](AsyncWebServerRequest* request) {
    if (!request->hasParam("name", true)) {
      request->send(400, "text/plain", "Missing 'name' param");
      return;
    }
    String name = request->getParam("name", true)->value();
    if (!scenarioNameSafe(name)) {
      request->send(400, "text/plain", "Invalid 'name'");
      return;
    }
    Scenario probe;
    if (!loadScenario(name, probe)) {
      request->send(404, "text/plain", "Unknown scenario");
      return;
    }
    pendingArg1 = name;
    pendingCmd = CMD_SCENARIO_ACTIVATE;
    request->send(200, "text/plain", "OK");
  });

  server.on("/api/scenarios/stop", HTTP_POST, [](AsyncWebServerRequest* request) {
    pendingCmd = CMD_SCENARIO_STOP;
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
    if (!trackExists(file)) {
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

  // JSON body, не form-urlencoded — сценарий вложенный (steps — массив),
  // form-параметры для этого неудобны (в отличие от остальных эндпоинтов).
  // Требует Content-Type: application/json от клиента (canHandle() в
  // AsyncCallbackJsonWebHandler это проверяет).
  auto* scenarioSaveHandler = new AsyncCallbackJsonWebHandler(
      "/api/scenarios/save",
      [](AsyncWebServerRequest* request, JsonVariant& json) {
        if (!json.is<JsonObject>()) {
          request->send(400, "text/plain", "Expected a JSON object");
          return;
        }
        JsonObject obj = json.as<JsonObject>();
        String name = obj["name"] | "";
        if (!scenarioNameSafe(name)) {
          request->send(400, "text/plain", "Invalid 'name'");
          return;
        }
        JsonArray steps = obj["steps"].as<JsonArray>();
        if (steps.size() > MAX_SCENARIO_STEPS) {
          request->send(413, "text/plain", "Too many steps");
          return;
        }

        Scenario s;
        s.name = name;
        s.startDelaySec = obj["startDelaySec"] | 0;
        s.loop = obj["loop"] | false;
        for (JsonVariant v : steps) {
          ScenarioStep step;
          step.file = v["file"].as<String>();
          // Пустой file — "чистая пауза" (device/src/scenario.cpp::scenarioTick),
          // не связана ни с каким треком. Непустой — обычная проверка имени.
          if (!step.file.isEmpty() && !trackNameSafe(step.file)) {
            request->send(400, "text/plain", "Invalid step 'file'");
            return;
          }
          step.delayAfterPrevSec = v["delayAfterPrevSec"] | 0;
          step.volume = v["volume"] | 1.0f;
          s.steps.push_back(step);
        }

        pendingScenario = s;
        pendingCmd = CMD_SCENARIO_SAVE;
        request->send(200, "text/plain", "OK");
      },
      8192);  // дефолт 1024Б мал для сценария с полусотней шагов
  server.addHandler(scenarioSaveHandler);

  server.on(
      "/api/deploy-frontend", HTTP_POST,
      [](AsyncWebServerRequest* request) {},  // ответ уходит из onBody, когда файл дописан
      nullptr, handleDeployBody);

  server.on(
      "/api/upload-audio", HTTP_POST,
      [](AsyncWebServerRequest* request) {},
      nullptr, handleAudioUploadBody);

  server.on(
      "/api/tracks/image", HTTP_POST,
      [](AsyncWebServerRequest* request) {},
      nullptr, handleImageUploadBody);

  // Раздача картинок треков (план 06) — регистрировать ДО catch-all "/" ниже:
  // диспетчер AsyncWebServer берёт первый подошедший handler по порядку
  // регистрации, а static-handler для "/" матчит вообще любой путь (у него
  // startsWith("/") — то же самое нашли и починили для onNotFound выше).
  server.serveStatic("/images/", SD, "/images/");

  // Фронтенд деплоится в /www/ (план 16). index.html — вход по умолчанию.
  server.serveStatic("/", SD, "/www/").setDefaultFile("index.html");

  // Без этого библиотека (ESPAsyncWebServer-esphome) шлёт 500 на любой путь,
  // который не подошёл ни одному handler'у и не нашёлся в /www/ — вводит в
  // заблуждение при диагностике (выглядит как сбой сервера, а не "нет
  // такого пути"). Заодно лог — если видите этот ESP_LOGW, а не 500 без
  // объяснения, сразу ясно, что запрос дошёл до устройства, но не совпал ни
  // с одним зарегистрированным роутом (например, прошивка ещё не содержит
  // этот эндпоинт).
  server.onNotFound([](AsyncWebServerRequest* request) {
    ESP_LOGW(TAG, "404: %s %s", request->methodToString(), request->url().c_str());
    request->send(404, "text/plain", "Not found");
  });

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
      // storage.cpp не знает ни про группы, ни про картинки — свести здесь
      // (тот же принцип оркестрации, что и со stopAudio() выше).
      unassignTrack(pendingArg1);
      deleteTrackImage(pendingArg1);
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
        renameTrackImage(pendingArg1, to);
      }
      break;
    }
    case CMD_CLEAR_ALL:
      if (currentState != IDLE) stopAudio();  // не удаляем открытый файл
      clearAllRecordings();
      clearAllAssignments();
      clearAllImages();
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
    case CMD_SCENARIO_SAVE:
      saveScenario(pendingScenario);
      break;
    case CMD_SCENARIO_DELETE:
      deleteScenario(pendingArg1);
      break;
    case CMD_SCENARIO_ACTIVATE:
      startScenario(pendingArg1);
      break;
    case CMD_SCENARIO_STOP:
      stopScenario();
      break;
    default:
      break;
  }
}
