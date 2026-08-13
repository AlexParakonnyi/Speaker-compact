#include "battery.h"

#include <Adafruit_ADS1X15.h>
#include <Wire.h>
#include <esp_log.h>

static const char* TAG = "battery";

static Adafruit_ADS1115 ads;
static bool adsReady = false;

BatteryReading batteryReading;

// Откалибровано 2026-08-11 по факту: при предположении 2.0 UI показывал
// 7.54В, реально на входе (мультиметром) — 5.1В. channelVoltage (то, что
// физически видит канал 0) = 7.54 / 2.0 = 3.77В — эта величина не зависит
// от DIVIDER_RATIO, только от самого делителя. actualRatio = 5.1 / 3.77 ≈
// 1.353 — подставлено ниже. Источник на момент калибровки — USB-зарядка
// телефона (5В), не реальная батарея (см. Plan/21) — сам коэффициент
// делителя от этого не меняется, он свойство резисторов на плате, но
// BATTERY_EMPTY_V/FULL_V ниже по-прежнему рассчитаны на 1S Li-ion, поэтому
// при 5.1В на входе процент будет упираться в 100% (ожидаемо, не баг) —
// пересчитывать имеет смысл, когда подключат реальную батарею. Если
// делитель на плате физически изменится — пересчитать заново по той же
// формуле (см. Plan/21-battery-indicator.done.md).
static const float DIVIDER_RATIO = 1.353f;

// 1S Li-ion/LiPo — линейная аппроксимация напряжение->процент (реальная
// разрядная кривая нелинейна, но для индикатора "прикидочно сколько осталось"
// линейного приближения достаточно, не усложняем).
static const float BATTERY_EMPTY_V = 3.0f;
static const float BATTERY_FULL_V = 4.2f;

void initBattery() {
  Wire.begin(8, 9);  // SDA=8, SCL=9 — дефолтные I2C-пины esp32-s3-devkitc-1, см. wiring.md
  adsReady = ads.begin();
  if (adsReady) {
    ESP_LOGI(TAG, "ADS1115 инициализирован (0x48)");
  } else {
    ESP_LOGE(TAG, "ADS1115 не отвечает на I2C — индикатор батареи будет недоступен");
  }
}

void updateBatteryReading() {
  if (!adsReady) {
    batteryReading.valid = false;
    return;
  }
  int16_t raw = ads.readADC_SingleEnded(0);
  float channelVoltage = ads.computeVolts(raw);
  float voltage = channelVoltage * DIVIDER_RATIO;
  float pct = (voltage - BATTERY_EMPTY_V) / (BATTERY_FULL_V - BATTERY_EMPTY_V) * 100.0f;

  batteryReading.voltage = voltage;
  batteryReading.percent = (uint8_t)constrain(pct, 0.0f, 100.0f);
  batteryReading.valid = true;
}
