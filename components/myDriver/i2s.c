#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/ringbuf.h"
#include "driver/i2s_std.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_heap_caps.h"

#include "main.h"
#include "i2s.h"
#include "cdPlayer.h"

#define TAG "audio_output"

#define EXAMPLE_STD_BCLK_IO1 PIN_I2S_BCK // I2S bit clock io number
#define EXAMPLE_STD_WS_IO1 PIN_I2S_LRCK  // I2S word select io number
#define EXAMPLE_STD_DOUT_IO1 PIN_I2S_DAT // I2S data in io number

#define I2S_SAMPLE_RATE 44100
#define I2S_DATA_BIT I2S_DATA_BIT_WIDTH_16BIT
#define AUDIO_RING_BUF_SIZE (96 * 1024)   // 96KB stream buffer
#define AUDIO_CHUNK_SIZE (1024)            // Frame chunk per I2S write

static TaskHandle_t transmitTask = NULL;
static i2s_chan_handle_t tx_chan = NULL;
static RingbufHandle_t audioRingBuf = NULL;

volatile bool i2s_bufsFull = false;
static uint8_t g_currentVolume = 45;
static int16_t last_sample_l = 0;
static int16_t last_sample_r = 0; // Default ~75% (level 45/60)

// 0 ~ 60 volume scale (0 = mute, 1..60 = -59dB .. 0dB)
const float volumeScale[61] = {
    0.000000f, 0.001122f, 0.001259f, 0.001413f, 0.001585f, 0.001778f,
    0.001995f, 0.002239f, 0.002512f, 0.002818f, 0.003162f, 0.003548f,
    0.003981f, 0.004467f, 0.005012f, 0.005623f, 0.006310f, 0.007079f,
    0.007943f, 0.008913f, 0.010000f, 0.011220f, 0.012589f, 0.014125f,
    0.015849f, 0.017783f, 0.019953f, 0.022387f, 0.025119f, 0.028184f,
    0.031623f, 0.035481f, 0.039811f, 0.044668f, 0.050119f, 0.056234f,
    0.063096f, 0.070795f, 0.079433f, 0.089125f, 0.100000f, 0.112202f,
    0.125893f, 0.141254f, 0.158489f, 0.177828f, 0.199526f, 0.223872f,
    0.251189f, 0.281838f, 0.316228f, 0.354813f, 0.398107f, 0.446684f,
    0.501187f, 0.562341f, 0.630957f, 0.707946f, 0.794328f, 0.891251f,
    1.000000f};

void audio_output_set_volume(uint8_t volume_0_60)
{
    if (volume_0_60 > 60)
        volume_0_60 = 60;
    g_currentVolume = volume_0_60;
    cdplayer_playerInfo.volume = volume_0_60;
}

uint8_t audio_output_get_volume(void)
{
    return g_currentVolume;
}

void audio_output_flush(void)
{
    if (audioRingBuf == NULL)
        return;

    size_t item_size = 0;
    // Drain ringbuffer completely
    while (1)
    {
        void *item = xRingbufferReceiveUpTo(audioRingBuf, &item_size, 0, 4096);
        if (item == NULL)
            break;
        vRingbufferReturnItem(audioRingBuf, item);
    }
    ESP_LOGI(TAG, "Audio buffer flushed");
}

int audio_output_write(const void *samples, size_t bytes, TickType_t wait_ticks)
{
    if (audioRingBuf == NULL || samples == NULL || bytes == 0)
        return 0;

    BaseType_t ret = xRingbufferSend(audioRingBuf, samples, bytes, wait_ticks);
    return (ret == pdTRUE) ? (int)bytes : 0;
}

void i2s_fillBuffer(uint8_t *dat)
{
    // Write full block to audio ring buffer
    if (audio_output_write(dat, I2S_TX_BUFFER_LEN, pdMS_TO_TICKS(100)) == 0)
    {
        i2s_bufsFull = true;
    }
    else
    {
        i2s_bufsFull = false;
    }
}

static void i2s_transmitTask(void *args)
{
    ESP_LOGI(TAG, "Audio TX task running on core %d", xPortGetCoreID());

    static uint8_t chunkBuffer[AUDIO_CHUNK_SIZE];
    static int downSampleCount = 0;
    static int64_t oL = 0;
    static int64_t oR = 0;

    while (1)
    {
        size_t received_bytes = 0;
        void *data = xRingbufferReceiveUpTo(audioRingBuf, &received_bytes, pdMS_TO_TICKS(10), AUDIO_CHUNK_SIZE);

        if (data != NULL && received_bytes > 0)
        {
            memcpy(chunkBuffer, data, received_bytes);
            vRingbufferReturnItem(audioRingBuf, data);

            // Feed oscilloscope queue
            if (queue_oscilloscope != NULL)
            {
                ChannelValue_t oscilloscope;
                int sample_count = received_bytes / 4; // 16-bit stereo = 4 bytes per sample
                int16_t *s = (int16_t *)chunkBuffer;

                for (int i = 0; i < sample_count; i++)
                {
                    oL += s[i * 2];
                    oR += s[i * 2 + 1];
                    downSampleCount++;

                    if (downSampleCount >= 20)
                    {
                        oscilloscope.l = (int16_t)(oL / downSampleCount);
                        oscilloscope.r = (int16_t)(oR / downSampleCount);
                        xQueueSend(queue_oscilloscope, &oscilloscope, 0);
                        oL = 0;
                        oR = 0;
                        downSampleCount = 0;
                    }
                }
            }

            // Software volume scaling (0..60)
            uint8_t vol = g_currentVolume;
            if (vol > 60)
                vol = 60;
            float scale = volumeScale[vol];
            int16_t *sample_ptr = (int16_t *)chunkBuffer;
            int total_samples = received_bytes / 2;
            for (int i = 0; i < total_samples; i++)
            {
                sample_ptr[i] = (int16_t)((float)sample_ptr[i] * scale);
            }

            if (total_samples >= 2) {
                last_sample_l = sample_ptr[total_samples - 2];
                last_sample_r = sample_ptr[total_samples - 1];
            }

            size_t bytes_written = 0;
            i2s_channel_write(tx_chan, chunkBuffer, received_bytes, &bytes_written, portMAX_DELAY);
        }
        else
        {
            // If buffer empty or idle, smooth ramp-down to 0 to prevent audio clicks/pops on pause
            int16_t *samples = (int16_t *)chunkBuffer;
            const int ramp_frames = 32; // 32 stereo frames = 128 bytes
            for (int i = 0; i < ramp_frames; i++) {
                if (last_sample_l != 0 || last_sample_r != 0) {
                    last_sample_l = (int16_t)(last_sample_l * 0.85f);
                    last_sample_r = (int16_t)(last_sample_r * 0.85f);
                    if (abs(last_sample_l) < 4) last_sample_l = 0;
                    if (abs(last_sample_r) < 4) last_sample_r = 0;
                }
                samples[i * 2] = last_sample_l;
                samples[i * 2 + 1] = last_sample_r;
            }
            size_t bytes_written = 0;
            i2s_channel_write(tx_chan, chunkBuffer, 128, &bytes_written, pdMS_TO_TICKS(10));
        }
    }
}

void i2s_init(void)
{
    // Allocate 96KB Ringbuffer in SPIRAM (8MB PSRAM), preserving precious internal SRAM for Wi-Fi & DMA
    audioRingBuf = xRingbufferCreateWithCaps(AUDIO_RING_BUF_SIZE, RINGBUF_TYPE_BYTEBUF, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
    if (audioRingBuf == NULL)
    {
        ESP_LOGW(TAG, "Failed to allocate audio ringbuffer in SPIRAM, falling back to internal RAM");
        audioRingBuf = xRingbufferCreate(32 * 1024, RINGBUF_TYPE_BYTEBUF);
    }
    assert(audioRingBuf != NULL);

    i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
    ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_chan, NULL));

    i2s_std_config_t std_cfg = {
        .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(I2S_SAMPLE_RATE),
        .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT, I2S_SLOT_MODE_STEREO),
        .gpio_cfg = {
            .mclk = PIN_I2S_MCLK,
            .bclk = EXAMPLE_STD_BCLK_IO1,
            .ws = EXAMPLE_STD_WS_IO1,
            .dout = EXAMPLE_STD_DOUT_IO1,
            .din = I2S_GPIO_UNUSED,
            .invert_flags = {
                .mclk_inv = false,
                .bclk_inv = false,
                .ws_inv = false,
            },
        },
    };
    ESP_ERROR_CHECK(i2s_channel_init_std_mode(tx_chan, &std_cfg));
    ESP_ERROR_CHECK(i2s_channel_enable(tx_chan));

    // Pin audio transmit task to Core 1 with high priority (18) for ultra-low latency audio
    BaseType_t ret = xTaskCreatePinnedToCore(i2s_transmitTask,
                                             "i2s_tx",
                                             4096,
                                             NULL,
                                             18,
                                             &transmitTask,
                                             1);
    if (ret == pdPASS)
    {
        ESP_LOGI(TAG, "Audio transmit task created successfully on Core 1");
    }
    else
    {
        ESP_LOGE(TAG, "Failed to create audio transmit task");
    }
}
