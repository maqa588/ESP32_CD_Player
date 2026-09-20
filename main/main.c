#include <stdio.h>
#include <inttypes.h>
#include "sdkconfig.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_chip_info.h"
#include "esp_flash.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_netif.h"
#include "esp_event.h"

#include "main.h"
#include "button.h"
#include "iic.h"
#include "i2s.h"
#include "usbhost_driver.h"
#include "cdPlayer.h"

void app_main(void)
{
    ESP_LOGI("MAIN", "==================================================");
    ESP_LOGI("MAIN", "       ESP32-S3 CD PLAYER SYSTEM STARTUP          ");
    ESP_LOGI("MAIN", "==================================================");

    // init nvs
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_LOGW("MAIN", "NVS partition was truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI("MAIN", "[1/6] NVS flash initialized.");

    // Pre-initialize TCP/IP stack and system default event loop
    esp_netif_init();
    esp_event_loop_create_default();

    /**
     * 配置io
     * configure gpio
     */
    gpio_config_t io_conf;

    // output pin (Backlight LEDC)
    ledc_timer_config_t ledc_timer = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .timer_num = LEDC_TIMER_0,
        .duty_resolution = LEDC_DUTY_RES,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));
    ledc_channel_config_t ledc_channel = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER_0,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = PIN_TFT_BL,
        .duty = 0, // Set duty to 0% initially
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.pin_bit_mask = (1ULL << PIN_TFT_RST) | (1ULL << PIN_TFT_DCX);
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    ESP_LOGI("MAIN", "[2/6] Display control GPIOs initialized (RST=%d, DC=%d, BL=%d).", PIN_TFT_RST, PIN_TFT_DCX, PIN_TFT_BL);

    // input pin (Buttons)
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = (1ULL << PIN_BTN_NEXT) | (1ULL << PIN_BTN_PREVIOUS) | (1ULL << PIN_BTN_VOL_DOWN) |
                           (1ULL << PIN_BTN_VOL_UP) | (1ULL << PIN_BTN_LIGHT) | (1ULL << PIN_BTN_PLAY) |
                           (1ULL << PIN_BTN_EJECT);
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    gpio_config(&io_conf);

    ESP_LOGI("MAIN", "[3/6] Button GPIOs initialized (EJECT=%d, PLAY=%d, LIGHT=%d, VOL+=%d, VOL-=%d, NEXT=%d, PREV=%d).",
             PIN_BTN_EJECT, PIN_BTN_PLAY, PIN_BTN_LIGHT, PIN_BTN_VOL_UP, PIN_BTN_VOL_DOWN, PIN_BTN_NEXT, PIN_BTN_PREVIOUS);

    ESP_LOGI("MAIN", "[4/6] Initializing USB Host...");
    usbhost_driverInit();

    ESP_LOGI("MAIN", "[5/6] Initializing I2S audio DAC & Button service...");
    i2s_init();
    btn_init();

    ESP_LOGI("MAIN", "[6/6] Starting CD Player service & LVGL UI...");
    cdplay_init();

    // 启动系统UI任务 (ST7735 + LVGL)
    xTaskCreatePinnedToCore(task_lvgl, "task_lvgl", 1024 * 16, NULL, 5, NULL, 1);
    ESP_LOGI("MAIN", ">>> LVGL Task created successfully! <<<");
}
