#include "network.h"

#include <WiFi.h>
#include <esp_log.h>

static const char* TAG = "network";

// Фиксированный пароль — проще, чем MAC-производный (см. Plan/09).
// Задокументирован в device/docs/wiring.md, пользователь печатает на корпусе.
#define AP_PASSWORD "speaker1234"

static bool apActive = false;
static bool lastLatchState = false;

bool latchClosed() { return digitalRead(PIN_LATCH_BUTTON) == LOW; }

static void startAP() {
  String ssid = "SpeakerCompact-" + WiFi.macAddress().substring(12);
  ssid.replace(":", "");
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ssid.c_str(), AP_PASSWORD);
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
  bool nowClosed = latchClosed();
  if (nowClosed == lastLatchState) return;
  lastLatchState = nowClosed;

  if (nowClosed) {
    startAP();
  } else {
    stopAP();
  }
}
