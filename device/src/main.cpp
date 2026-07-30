#include <Arduino.h>

#include "global.h"
#include "network.h"
#include "player.h"
#include "servers.h"
#include "speaker.h"
#include "storage.h"

static const char* stateName(State s) { return s == PLAYING ? "PLAYING" : "IDLE"; }

void setup() {
  Serial.begin(115200);
  delay(300);  // время USB-CDC подняться, чтобы не терять первые строки
  Serial.println("\n\n=== Speaker-compact: boot (минимальная версия) ===");

  sdMutex = xSemaphoreCreateMutex();

  initStorage();
  initSpeaker();
  updateTrackList();
  Serial.printf("[SD] Найдено треков: %u\n", (unsigned)trackList.size());

  initNetwork();  // защёлка решает: поднимать AP или нет (Plan/09)
  initServers();

  Serial.println("=== Готово ===");
}

void loop() {
  networkTick();  // следит за защёлкой, включает/гасит AP и LED на лету

  // Команды с веб-хендлеров выполняем здесь же, в единственном месте (вместе
  // с плеером), где вообще трогаем SD_MMC/File для play/stop/delete/rename —
  // исключает гонку с AsyncWebServer.
  processPendingCommands();

  playbackTick();  // чтение/декодирование файла + отправка на динамик

  // Heartbeat раз в 10 сек — чтобы IP/статус были видны в мониторе порта,
  // даже если его подключили уже после старта устройства.
  static uint32_t lastHeartbeat = 0;
  if (millis() - lastHeartbeat >= 10000) {
    lastHeartbeat = millis();
    Serial.printf("[heartbeat] latch=%s state=%s tracks=%u\n",
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
