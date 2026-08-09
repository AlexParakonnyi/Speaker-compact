#include "speaker.h"

#include <esp_log.h>

#include "global.h"

static const char* TAG = "speaker";

float playbackVolume = 1.0f;

void initSpeaker() {
  i2s_config_t amp_cfg = {};
  amp_cfg.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX);
  amp_cfg.sample_rate = SAMPLE_RATE;
  amp_cfg.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  amp_cfg.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  amp_cfg.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  amp_cfg.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  amp_cfg.dma_buf_count = 4;
  amp_cfg.dma_buf_len = 256;
  amp_cfg.use_apll = false;
  amp_cfg.tx_desc_auto_clear = true;  // тишина в DMA, когда i2s_write не
                                      // вызывается -> без щелчков/повтора

  i2s_pin_config_t amp_pins = {};
  amp_pins.bck_io_num = I2S_AMP_BCLK;
  amp_pins.ws_io_num = I2S_AMP_LRC;
  amp_pins.data_out_num = I2S_AMP_DIN;
  amp_pins.data_in_num = I2S_PIN_NO_CHANGE;

  i2s_driver_install(I2S_AMP_PORT, &amp_cfg, 0, NULL);
  i2s_set_pin(I2S_AMP_PORT, &amp_pins);

  // Непрерывные клоки усилителя в режиме master — потенциальный источник
  // наводок на соседние линии -> глушим, пока реально не играем.
  i2s_stop(I2S_AMP_PORT);
  ESP_LOGI(TAG, "I2S init done: port %d, %u Hz, mono 16-bit", I2S_AMP_PORT, SAMPLE_RATE);
}

void speakerStart() { i2s_start(I2S_AMP_PORT); }
void speakerStop() { i2s_stop(I2S_AMP_PORT); }

void setPlaybackSampleRate(uint32_t rate) { i2s_set_sample_rates(I2S_AMP_PORT, rate); }

size_t speakerWrite(uint8_t* buf, size_t len) {
  if (playbackVolume != 1.0f) {
    int16_t* samples = (int16_t*)buf;
    size_t n = len / sizeof(int16_t);
    for (size_t i = 0; i < n; i++) {
      samples[i] = clampToInt16((int32_t)(samples[i] * playbackVolume));
    }
  }
  size_t written = 0;
  i2s_write(I2S_AMP_PORT, buf, len, &written, pdMS_TO_TICKS(100));
  return written;
}
