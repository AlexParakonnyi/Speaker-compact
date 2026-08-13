#ifndef BATTERY_H
#define BATTERY_H

#include <Arduino.h>

struct BatteryReading {
  float voltage = 0.0f;
  uint8_t percent = 0;
  bool valid = false;  // false, если ADS1115 не ответил на I2C
};

// Инициализирует I2C (GPIO8/9, wiring.md) и ADS1115. Отсутствие модуля не
// должно останавливать остальную прошивку — просто индикатор не работает.
void initBattery();

// Реальный I2C-запрос — вызывать из loop() не чаще раза в несколько секунд
// (не из HTTP-хендлера — так же, как storage/SD, не гонять шину откуда попало).
void updateBatteryReading();

// Кэш последнего чтения — им пользуется GET /api/status.
extern BatteryReading batteryReading;

#endif
