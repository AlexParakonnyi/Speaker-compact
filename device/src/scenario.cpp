#include "scenario.h"

#include <ArduinoJson.h>
#include <SD.h>
#include <esp_log.h>

#include "global.h"
#include "player.h"

static const char* TAG = "scenario";

static String scenarioPath(const String& name) { return String(SCENARIOS_DIR) + "/" + name + ".json"; }

void initScenarios() {
  if (!sdMounted) return;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.exists(SCENARIOS_DIR)) SD.mkdir(SCENARIOS_DIR);
  xSemaphoreGive(sdMutex);
}

std::vector<String> listScenarioNames() {
  std::vector<String> names;
  if (!sdMounted) return names;

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.exists(SCENARIOS_DIR)) {
    xSemaphoreGive(sdMutex);
    return names;
  }
  File dir = SD.open(SCENARIOS_DIR);
  if (dir) {
    File f = dir.openNextFile();
    while (f) {
      String name = String(f.name());
      bool isDir = f.isDirectory();
      f.close();
      if (!isDir && name.endsWith(".json")) {
        names.push_back(name.substring(0, name.length() - 5));
      }
      f = dir.openNextFile();
    }
    dir.close();
  }
  xSemaphoreGive(sdMutex);
  return names;
}

bool loadScenario(const String& name, Scenario& out) {
  if (!sdMounted) return false;

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  File f = SD.open(scenarioPath(name), FILE_READ);
  if (!f) {
    xSemaphoreGive(sdMutex);
    return false;
  }
  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();
  xSemaphoreGive(sdMutex);

  if (err) {
    ESP_LOGE(TAG, "%s parse error: %s", name.c_str(), err.c_str());
    return false;
  }

  out.name = name;
  out.startDelaySec = doc["startDelaySec"] | 0;
  out.loop = doc["loop"] | false;
  out.steps.clear();
  for (JsonVariant v : doc["steps"].as<JsonArray>()) {
    ScenarioStep step;
    step.file = v["file"].as<String>();
    step.delayAfterPrevSec = v["delayAfterPrevSec"] | 0;
    step.volume = v["volume"] | 1.0f;
    out.steps.push_back(step);
  }
  return true;
}

bool saveScenario(const Scenario& s) {
  if (!sdMounted) return false;
  if (s.steps.size() > MAX_SCENARIO_STEPS) {
    ESP_LOGE(TAG, "%s: too many steps (%u > %u)", s.name.c_str(), (unsigned)s.steps.size(),
             (unsigned)MAX_SCENARIO_STEPS);
    return false;
  }

  JsonDocument doc;
  doc["name"] = s.name;
  doc["startDelaySec"] = s.startDelaySec;
  doc["loop"] = s.loop;
  JsonArray steps = doc["steps"].to<JsonArray>();
  for (const ScenarioStep& step : s.steps) {
    JsonObject o = steps.add<JsonObject>();
    o["file"] = step.file;
    o["delayAfterPrevSec"] = step.delayAfterPrevSec;
    o["volume"] = step.volume;
  }

  xSemaphoreTake(sdMutex, portMAX_DELAY);
  if (!SD.exists(SCENARIOS_DIR)) SD.mkdir(SCENARIOS_DIR);
  File f = SD.open(scenarioPath(s.name), FILE_WRITE);
  bool ok = f && serializeJson(doc, f) > 0;
  if (f) f.close();
  xSemaphoreGive(sdMutex);

  if (!ok) ESP_LOGE(TAG, "Failed to write %s", scenarioPath(s.name).c_str());
  return ok;
}

bool deleteScenario(const String& name) {
  if (!sdMounted) return false;
  xSemaphoreTake(sdMutex, portMAX_DELAY);
  String path = scenarioPath(name);
  bool existed = SD.exists(path);
  bool ok = existed && SD.remove(path);
  xSemaphoreGive(sdMutex);
  return existed ? ok : false;
}

// ── Выполнение (план 08) ────────────────────────────────────────────────

volatile bool scenarioActive = false;
String activeScenarioName;

static Scenario activeScenario;
static int activeStepIndex = -1;
static uint32_t nextEventAtMs = 0;
// true, пока играет трек текущего шага — ждём, когда player.cpp сам доиграет
// его до конца (currentState вернётся в IDLE), тогда продвигаем шаг. Ручной
// /api/stop посреди шага сработает так же (currentState станет IDLE) —
// известное упрощение интерим-версии, см. Plan/08 "Как реализовано".
static bool waitingForTrackEnd = false;

static void armStepTimer(uint32_t delaySec) { nextEventAtMs = millis() + delaySec * 1000UL; }

static void advanceStep() {
  activeStepIndex++;
  if (activeStepIndex >= (int)activeScenario.steps.size()) {
    if (activeScenario.loop && !activeScenario.steps.empty()) {
      ESP_LOGI(TAG, "'%s': loop, restart from step 1", activeScenarioName.c_str());
      activeStepIndex = 0;
      armStepTimer(activeScenario.steps[0].delayAfterPrevSec);
    } else {
      ESP_LOGI(TAG, "'%s': finished", activeScenarioName.c_str());
      scenarioActive = false;
    }
    return;
  }
  armStepTimer(activeScenario.steps[activeStepIndex].delayAfterPrevSec);
}

bool startScenario(const String& name) {
  Scenario s;
  if (!loadScenario(name, s)) {
    ESP_LOGE(TAG, "startScenario: '%s' not found/invalid", name.c_str());
    return false;
  }
  if (currentState != IDLE) stopAudio();  // не оставляем висеть предыдущее воспроизведение

  activeScenario = s;
  activeScenarioName = name;
  activeStepIndex = 0;
  waitingForTrackEnd = false;
  scenarioActive = true;
  armStepTimer(s.startDelaySec);
  ESP_LOGI(TAG, "'%s' activated: %u step(s), startDelay %us, loop=%s", name.c_str(),
           (unsigned)s.steps.size(), (unsigned)s.startDelaySec, s.loop ? "true" : "false");
  return true;
}

void stopScenario() {
  if (!scenarioActive) return;
  ESP_LOGI(TAG, "'%s' stopped manually", activeScenarioName.c_str());
  scenarioActive = false;
  waitingForTrackEnd = false;
  if (currentState != IDLE) stopAudio();
}

void scenarioTick() {
  if (!scenarioActive) return;

  if (waitingForTrackEnd) {
    if (currentState == IDLE) {  // player.cpp сам доиграл трек шага до конца
      waitingForTrackEnd = false;
      advanceStep();
    }
    return;  // трек шага ещё играет — больше тут делать нечего
  }

  if ((int32_t)(millis() - nextEventAtMs) < 0) return;  // ещё не время (millis() overflow-safe)
  if (activeStepIndex < 0 || activeStepIndex >= (int)activeScenario.steps.size()) {
    scenarioActive = false;  // не должны сюда попадать — защита от рассинхрона
    return;
  }

  const ScenarioStep& step = activeScenario.steps[activeStepIndex];
  // Громкость на шаг — поверх общего speakerWrite()/playbackVolume, как
  // предписывает план 08 п.3 (не отдельный путь применения громкости).
  // Побочный эффект: playbackVolume — глобальная настройка, после сценария
  // остаётся тем значением, что стояло на последнем шаге — осознанно, тот
  // же playbackVolume используется и ручным preview-плеем.
  playbackVolume = constrain(step.volume, 0.0f, 4.0f);
  startPlayback(step.file);
  waitingForTrackEnd = true;
  ESP_LOGI(TAG, "'%s' step %d/%d: %s", activeScenarioName.c_str(), activeStepIndex + 1,
           (int)activeScenario.steps.size(), step.file.c_str());
}
