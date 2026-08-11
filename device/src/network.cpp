#include "network.h"

#include <WiFi.h>
#include <esp_log.h>

static const char* TAG = "network";

// Фиксированный пароль — проще, чем MAC-производный (см. Plan/09).
// Задокументирован в device/docs/wiring.md, пользователь печатает на корпусе.
#define AP_PASSWORD "speaker1234"

static bool apActive = false;
static bool lastLatchState = false;    // подтверждённое (debounced) состояние

// Дребезг контактов — не только у momentary-кнопок: наводки от усилителя
// класса D рядом (MAX98357A) или мех. вибрация могут дать кратковременные
// ложные срабатывания и на "защёлкивающемся" переключателе. Без дебаунса
// networkTick() реагировал на каждое сырое чтение — стабильно давал шторм
// stopAP()/startAP() (см. серийный лог с "netstack cb reg failed 12308" —
// WiFi-стек не успевал закрыться перед повторным поднятием AP), который
// рвал все активные HTTP/WS-соединения с фронтенда.
static bool candidateLatchState = false;
static uint32_t candidateSince = 0;
static const uint32_t LATCH_DEBOUNCE_MS = 50;

bool latchClosed() { return digitalRead(PIN_LATCH_BUTTON) == LOW; }

static void startAP() {
  String ssid = "SpeakerCompact-" + WiFi.macAddress().substring(12);
  ssid.replace(":", "");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), AP_PASSWORD);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);  // максимум — дешёвый эксперимент против throughput/дальности
  digitalWrite(PIN_AP_LED, HIGH);
  apActive = true;
  ESP_LOGI(TAG, "SSID: %s  IP: %s", ssid.c_str(), WiFi.softAPIP().toString().c_str());
}

static void stopAP() {
  WiFi.softAPdisconnect(true);
  digitalWrite(PIN_AP_LED, LOW);
  apActive = false;
  ESP_LOGI(TAG, "Остановлена (защёлка разомкнута)");
}

void initNetwork() {
  pinMode(PIN_LATCH_BUTTON, INPUT_PULLUP);
  pinMode(PIN_AP_LED, OUTPUT);
  digitalWrite(PIN_AP_LED, LOW);

  lastLatchState = latchClosed();
  if (lastLatchState) {
    startAP();
  } else {
    // Разомкнута при старте: сценарий/глубокий сон — Plan/08, Plan/11,
    // ещё не реализованы. Минимальная версия просто ждёт в IDLE.
    ESP_LOGI(TAG, "Защёлка разомкнута — AP не поднимается (сценарии: Plan/08, TODO)");
  }
}

void networkTick() {
  bool raw = latchClosed();
  if (raw != candidateLatchState) {
    candidateLatchState = raw;
    candidateSince = millis();
  }
  if (candidateLatchState == lastLatchState) return;
  if (millis() - candidateSince < LATCH_DEBOUNCE_MS) return;  // ещё не устоялось

  lastLatchState = candidateLatchState;
  if (lastLatchState) {
    startAP();
  } else {
    stopAP();
  }
}
