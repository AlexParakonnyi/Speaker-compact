#include "network.h"

#include <WiFi.h>

// Фиксированный пароль — проще, чем MAC-производный (см. Plan/09).
// Задокументирован в device/docs/wiring.md, пользователь печатает на корпусе.
#define AP_PASSWORD "speaker1234"

static bool apActive = false;
static bool lastLatchState = false;

bool latchClosed() { return digitalRead(PIN_LATCH_BUTTON) == LOW; }

static void startAP() {
  String ssid = "SpeakerCompact-" + WiFi.macAddress().substring(12);
  ssid.replace(":", "");
  WiFi.softAP(ssid.c_str(), AP_PASSWORD);
  digitalWrite(PIN_AP_LED, HIGH);
  apActive = true;
  Serial.println("[AP] SSID: " + ssid + "  IP: " + WiFi.softAPIP().toString());
}

static void stopAP() {
  WiFi.softAPdisconnect(true);
  digitalWrite(PIN_AP_LED, LOW);
  apActive = false;
  Serial.println("[AP] Остановлена (защёлка разомкнута)");
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
    Serial.println("[AP] Защёлка разомкнута — AP не поднимается (сценарии: Plan/08, TODO)");
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
