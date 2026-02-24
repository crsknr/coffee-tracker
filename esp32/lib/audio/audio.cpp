#include "audio.h"
#include "config.h"
#include <Arduino.h>
#include <math.h>

#if ENABLE_AUDIO_WAKE
#include <driver/i2s.h>
#endif

static int sustainedCount = 0;

// ============================================
// Start I2S microphone
// ============================================

void startAudio() {
#if ENABLE_AUDIO_WAKE
    i2s_config_t i2s_config = {};
    i2s_config.mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
    i2s_config.sample_rate = I2S_SAMPLE_RATE;
    i2s_config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
    i2s_config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
    i2s_config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
    i2s_config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
    i2s_config.dma_buf_count = 4;
    i2s_config.dma_buf_len = I2S_BUFFER_SIZE;
    i2s_config.use_apll = false;

    i2s_pin_config_t pin_config = {};
    pin_config.bck_io_num = I2S_SCK_PIN;
    pin_config.ws_io_num = I2S_WS_PIN;
    pin_config.data_out_num = I2S_PIN_NO_CHANGE;
    pin_config.data_in_num = I2S_SD_PIN;

    i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pin_config);
    sustainedCount = 0;

    Serial.println("[AUDIO] I2S started");
#endif
}

// ============================================
// Stop I2S microphone
// ============================================

void stopAudio() {
#if ENABLE_AUDIO_WAKE
    i2s_driver_uninstall(I2S_NUM_0);
    sustainedCount = 0;
    Serial.println("[AUDIO] I2S stopped");
#endif
}

// ============================================
// Process one audio frame, return true if
// grinder detected (sustained loud noise)
// ============================================

bool processAudioFrame() {
#if ENABLE_AUDIO_WAKE
    int16_t samples[I2S_BUFFER_SIZE];
    size_t bytesRead = 0;

    esp_err_t err = i2s_read(I2S_NUM_0, samples, sizeof(samples), &bytesRead, 100);
    if (err != ESP_OK || bytesRead == 0) return false;

    int numSamples = bytesRead / sizeof(int16_t);

    // Compute RMS energy
    int64_t sumSquares = 0;
    for (int i = 0; i < numSamples; i++) {
        sumSquares += (int64_t)samples[i] * samples[i];
    }
    float rms = sqrtf((float)sumSquares / numSamples);

    // Check against threshold
    if (rms > GRINDER_DETECTION_THRESHOLD) {
        sustainedCount++;
        // Each frame ~64ms (1024 samples @ 16kHz)
        float sustainedMs = sustainedCount * (1000.0f * I2S_BUFFER_SIZE / I2S_SAMPLE_RATE);
        if (sustainedMs >= GRINDER_SUSTAINED_MS) {
            Serial.printf("[AUDIO] Grinder detected (RMS=%.0f, sustained=%.0fms)\n",
                          rms, sustainedMs);
            return true;
        }
    } else {
        sustainedCount = 0;
    }
#endif
    return false;
}
