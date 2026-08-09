# Plan/18 — Device Structured Logging

**Status:** in-progress  
**Implements:** замена `Serial.println`/`Serial.printf` на `ESP_LOGx(TAG, ...)` во всех модулях устройства.

---

## Цель

Каждый модуль прошивки должен эмитировать диагностические сообщения через ESP-IDF log API вместо прямых вызовов `Serial`. Это даёт:

- Уровни логирования (INFO / WARN / ERROR / DEBUG), фильтруемые через `CORE_DEBUG_LEVEL`
- Временную метку и имя модуля в каждой строке автоматически: `I (1234) storage: Card mounted OK`
- Возможность отключить весь лог в релизе одним флагом (`-DCORE_DEBUG_LEVEL=0`)
- Единый стиль вместо разнобоя форматов

## Зависимости

- `platformio.ini` уже содержит `-DCORE_DEBUG_LEVEL=3` → INFO-уровень включён
- `esp_log.h` включается в каждый файл явно

## Файлы

| Файл | Изменения |
|------|-----------|
| `main.cpp` | boot/ready/heartbeat → `ESP_LOGI` |
| `network.cpp` | AP start/stop/latch → `ESP_LOGI`; добавить `WiFi.mode(WIFI_AP)` перед `softAP()` |
| `storage.cpp` | mount OK/FAIL → `ESP_LOGI`/`ESP_LOGE`; open fail в `storageBeginWrite` → `ESP_LOGE` |
| `player.cpp` | playback start/errors → `ESP_LOGI`/`ESP_LOGE` |
| `codec.cpp` | format detect → `ESP_LOGI` |
| `servers.cpp` | server start + deploy/upload events → `ESP_LOGI` (новые, не были) |
| `speaker.cpp` | I2S init done → `ESP_LOGI` (новый) |

## Соглашение

```cpp
#include <esp_log.h>
static const char* TAG = "module_name";

ESP_LOGI(TAG, "message %s %u", str.c_str(), num);   // INFO
ESP_LOGW(TAG, "warning: %s", reason);                // WARN
ESP_LOGE(TAG, "error: %s", detail.c_str());          // ERROR
```

- Без `\n` — добавляется автоматически
- `String` → `.c_str()` для `%s`
- `Serial.begin(115200)` в `main.cpp` остаётся — нужен для USB-CDC baud rate
