#ifndef NETWORK_H
#define NETWORK_H

#include <Arduino.h>

// Пины — см. device/docs/wiring.md. Не путать с audio_test/Level-compact,
// там другая разводка.
#define PIN_LATCH_BUTTON 2   // INPUT_PULLUP, замкнута(AP-режим) = LOW
#define PIN_AP_LED 15        // активный HIGH

// true, пока защёлка замкнута (пользователь хочет AP/конфиг-режим).
// Минимальная версия не спит и не бежит сценарии — глубокий сон
// (Plan/11) переключит смысл false-состояния позже; сейчас там просто IDLE.
bool latchClosed();

// Настраивает пины защёлки/LED, поднимает WiFi AP (SSID+пароль см. network.cpp),
// включает LED-индикатор на время работы точки доступа.
void initNetwork();

// Вызывается из loop(): следит за защёлкой и держит LED синхронизированным
// с фактическим состоянием AP (WiFi.softAPgetStationNum() и т.п. сюда не
// нужны — просто "AP поднят/не поднят").
void networkTick();

#endif
