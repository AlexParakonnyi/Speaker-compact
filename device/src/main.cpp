#include <Arduino.h>
#include <esp_log.h>

#include "global.h"
#include "groups.h"
#include "network.h"
#include "player.h"
#include "servers.h"
#include "speaker.h"
#include "storage.h"

static const char* TAG = "main";
static const char* stateName(State s) { return s == PLAYING ? "PLAYING" : "IDLE"; }

void setup() {
  Serial.begin(115200);
  delay(300);  // время USB-CDC подняться, чтобы не терять первые строки
  ESP_LOGI(TAG, "=== Speaker-compact: boot (минимальная версия) ===");

  sdMutex = xSemaphoreCreateMutex();

  initStorage();
  initSpeaker();
  updateTrackList();
  loadGroups();
  ESP_LOGI(TAG, "SD: найдено треков: %u, групп: %u", (unsigned)trackList.size(),
           (unsigned)groupList.size());

  initNetwork();  // защёлка решает: поднимать AP или нет (Plan/09)
  initServers();

  ESP_LOGI(TAG, "=== Готово ===");
}

void loop() {
  networkTick();  // следит за защёлкой, включает/гасит AP и LED на лету

  // Команды с веб-хендлеров выполняем здесь же, в единственном месте (вместе
  // с плеером), где вообще трогаем SD/File для play/stop/delete/rename —
  // исключает гонку с AsyncWebServer.
  processPendingCommands();

  playbackTick();  // чтение/декодирование файла + отправка на динамик

  // Heartbeat раз в 10 сек — чтобы IP/статус были видны в мониторе порта,
  // даже если его подключили уже после старта устройства.
  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    ESP_LOGI(TAG, "heartbeat latch=%s state=%s tracks=%u",
             latchClosed() ? "closed(AP)" : "open", stateName(currentState),
             (unsigned)trackList.size());
  }

  // Подстраховка: раз в 3 сек, только в IDLE (нет открытого audioFile),
  // обновляем кэш списка файлов — на случай загрузки нового трека извне.
  static uint32_t lastListRefresh = 0;
  if (sdMounted && currentState == IDLE && millis() - lastListRefresh >= 3000) {
    lastListRefresh = millis();
    updateTrackList();
  }

  delay(1);  // отдаём время watchdog'у
}
