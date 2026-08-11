#ifndef SCENARIO_H
#define SCENARIO_H

#include <Arduino.h>

#include <vector>

// Реальная FAT-директория, один файл на сценарий (план 07).
#define SCENARIOS_DIR "/scenarios"

// Разумный потолок на число шагов — держим разбор JSON в RAM устройства
// в предсказуемых рамках (см. план 07 п.3).
#define MAX_SCENARIO_STEPS 50

struct ScenarioStep {
  String file;
  uint32_t delayAfterPrevSec = 0;
  float volume = 1.0f;
};

struct Scenario {
  String name;
  uint32_t startDelaySec = 0;
  bool loop = false;
  std::vector<ScenarioStep> steps;
};

// Создаёт SCENARIOS_DIR, если его ещё нет. Вызывать один раз при старте.
void initScenarios();

// Имена сценариев (без ".json") — сканирует SCENARIOS_DIR, не рекурсивно.
std::vector<String> listScenarioNames();

// true и заполняет out, если файл существует и валиден.
bool loadScenario(const String& name, Scenario& out);

// Перезаписывает /scenarios/<s.name>.json целиком. Проверка MAX_SCENARIO_STEPS
// — здесь; проверка имени (без "/"/".." ) — на вызывающей стороне
// (servers.cpp, тот же принцип, что и с trackNameSafe/groupNameSafe).
bool saveScenario(const Scenario& s);

bool deleteScenario(const String& name);

// ── Выполнение (план 08) ────────────────────────────────────────────────
// Интерим-версия БЕЗ deep sleep (план 11 ещё не реализован, согласовано с
// пользователем): между шагами устройство ждёт в обычном loop() по
// millis()-таймеру, CPU/AP остаются активны — не экономит батарею. Когда
// появится план 11, поменяется только механизм ожидания (esp_deep_sleep +
// RTC_DATA_ATTR вместо millis()), логика шагов — та же.
extern volatile bool scenarioActive;
extern String activeScenarioName;

// false, если сценарий с таким именем не найден/не грузится. Прерывает
// текущее воспроизведение, если что-то играло.
bool startScenario(const String& name);
void stopScenario();

// Вызывать из loop() каждую итерацию, вместе с playbackTick().
void scenarioTick();

#endif
