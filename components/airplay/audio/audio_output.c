#include "audio_output.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"
#include "audio_receiver.h"
#include "settings.h"
#include "i2s.h"

#define TAG "ap2_aout"

static TaskHandle_t playback_task_handle = NULL;
static volatile bool playback_running = false;

// Static decode and stream output buffer (1024 stereo samples = 4096 bytes)
// Matches AAC single-frame period (1024 samples @ 44.1kHz ~ 23.2ms)
static int16_t s_playback_pcm_buf[1024 * 2];

static void playback_task(void *pvParameters) {
    (void)pvParameters;
    ESP_LOGI(TAG, "AirPlay 2 playback task started on core %d", xPortGetCoreID());

    while (playback_running) {
        size_t samples = audio_receiver_read(s_playback_pcm_buf, 1024);
        if (samples > 0) {
            size_t bytes_to_write = samples * 2 * sizeof(int16_t);
            // Write to unified I2S ringbuffer
            audio_output_write((uint8_t *)s_playback_pcm_buf, bytes_to_write, pdMS_TO_TICKS(100));
        } else {
            // Buffer empty or waiting for jitter buffer/anchor, short delay (5ms) to prevent starving
            TickType_t delay_ticks = pdMS_TO_TICKS(5);
            vTaskDelay(delay_ticks > 0 ? delay_ticks : 1);
        }
    }

    ESP_LOGI(TAG, "AirPlay 2 playback task stopped");
    playback_task_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t audio_output_init(void) {
    ESP_LOGI(TAG, "Audio output bridge initialized");
    return ESP_OK;
}

void audio_output_deinit(void) {
    audio_output_stop();
}

void audio_output_start(void) {
    if (playback_task_handle != NULL) {
        return;
    }
    playback_running = true;
    // Run playback task on Core 1 with generous 32KB stack in SPIRAM (8MB PSRAM)
    BaseType_t ret = xTaskCreatePinnedToCoreWithCaps(
        playback_task,
        "ap2_play",
        32768,
        NULL,
        4,
        &playback_task_handle,
        1,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );
    if (ret != pdPASS) {
        ret = xTaskCreatePinnedToCore(
            playback_task,
            "ap2_play",
            8192,
            NULL,
            4,
            &playback_task_handle,
            1
        );
    }
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to start AirPlay 2 playback task");
        playback_running = false;
    }
}

void audio_output_stop(void) {
    if (playback_task_handle == NULL) {
        return;
    }
    playback_running = false;
    // Wait for task to exit
    int timeout = 20;
    while (playback_task_handle != NULL && timeout-- > 0) {
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

void audio_output_set_sample_rate(uint32_t rate) {
    (void)rate;
}

void audio_output_set_source_rate(int rate) {
    (void)rate;
}

uint32_t audio_output_get_hardware_latency_us(void) {
    // Typical I2S DMA pipeline latency (e.g. 20ms)
    return 20000;
}
