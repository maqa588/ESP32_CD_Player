#include <string.h>
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "mdns.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/idf_additions.h"
#include "esp_heap_caps.h"

#include "airplay_service.h"
#include "settings.h"
#include "hap.h"
#include "ptp_clock.h"
#include "audio_receiver.h"
#include "audio_output.h"
#include "mdns_airplay.h"
#include "rtsp_server.h"

#define TAG "airplay2_svc"

static airplay_status_t g_airplay_status = {
    .wifi_state = AIRPLAY_WIFI_DISCONNECTED,
    .ip_addr = "0.0.0.0",
    .device_name = AIRPLAY_DEVICE_NAME,
    .service_active = false,
};

static esp_netif_t *sta_netif = NULL;
static bool wifi_inited = false;
static bool s_airplay2_infrastructure_ready = false;

static void airplay2_start_services_task(void *pvParameters)
{
    (void)pvParameters;
    ESP_LOGI(TAG, "Starting AirPlay 2 services stack...");

    if (!s_airplay2_infrastructure_ready) {
        // Initialize mDNS stack first
        esp_err_t mdns_err = mdns_init();
        if (mdns_err != ESP_OK && mdns_err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "mdns_init returned: %s", esp_err_to_name(mdns_err));
        }

        // 1. Initialize PTP clock for AirPlay 2 nanosecond synchronization
        esp_err_t err = ptp_clock_init();
        if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
            ESP_LOGW(TAG, "PTP clock init: %s", esp_err_to_name(err));
        }

        // 2. Initialize HAP (HomeKit Accessory Protocol)
        hap_init();

        // 3. Initialize Audio Receiver (AAC + ALAC)
        audio_receiver_init();

        // 4. Initialize Unified Audio Output
        audio_output_init();

        // 5. Advertise AirPlay 2 mDNS service
        mdns_airplay_init();

        s_airplay2_infrastructure_ready = true;
    }

    // 6. Start audio playback consumer task
    audio_output_start();

    // 7. Start RTSP server (handles FairPlay handshake & RTSP methods)
    rtsp_server_start();

    g_airplay_status.service_active = true;
    ESP_LOGI(TAG, ">>> AirPlay 2 receiver is now ONLINE and discoverable! <<<");
    vTaskDelete(NULL);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        // Disable Wi-Fi modem sleep / power saving to eliminate RF latency and packet jitter
        esp_wifi_set_ps(WIFI_PS_NONE);
        ESP_LOGI(TAG, "Wi-Fi Power Save disabled (WIFI_PS_NONE) for smooth audio streaming");
        esp_wifi_connect();
        g_airplay_status.wifi_state = AIRPLAY_WIFI_CONNECTING;
        ESP_LOGI(TAG, "Connecting to Wi-Fi AP [%s]...", AIRPLAY_WIFI_SSID);
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Wi-Fi disconnected, reconnecting...");
        g_airplay_status.wifi_state = AIRPLAY_WIFI_CONNECTING;
        esp_wifi_connect();
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        snprintf(g_airplay_status.ip_addr, sizeof(g_airplay_status.ip_addr), IPSTR, IP2STR(&event->ip_info.ip));
        g_airplay_status.wifi_state = AIRPLAY_WIFI_CONNECTED_STA;
        ESP_LOGI(TAG, "Wi-Fi Connected! IP Address: %s", g_airplay_status.ip_addr);

        // Offload mDNS and AirPlay 2 stack initialization to dedicated task in INTERNAL SRAM
        // (Stack MUST be in internal SRAM to allow Flash/NVS access without cache-disabled assertions).
        // Once initialization completes, ap2_start deletes itself, reclaiming all memory.
        BaseType_t start_ret = xTaskCreatePinnedToCore(
            airplay2_start_services_task,
            "ap2_start",
            6144,
            NULL,
            5,
            NULL,
            0
        );
        if (start_ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create ap2_start task in internal SRAM!");
        }
    }
}

esp_err_t airplay_service_init(void)
{
    if (wifi_inited)
        return ESP_OK;

    settings_init();

    // Pre-initialize HAP (loads or generates Ed25519 keypair from NVS in internal SRAM task context)
    hap_init();

    // 1. Initialize TCP/IP stack
    esp_err_t ret = esp_netif_init();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_netif_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 2. Create default event loop (required for default Wi-Fi handlers)
    ret = esp_event_loop_create_default();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_event_loop_create_default failed: %s", esp_err_to_name(ret));
        return ret;
    }

    // 3. Create default STA netif
    if (sta_netif == NULL) {
        sta_netif = esp_netif_create_default_wifi_sta();
        if (sta_netif == NULL) {
            ESP_LOGE(TAG, "Failed to create default wifi sta netif");
            return ESP_FAIL;
        }
    }

    // 4. Initialize Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ret = esp_wifi_init(&cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "esp_wifi_init failed: %s", esp_err_to_name(ret));
        return ret;
    }

    esp_event_handler_instance_register(WIFI_EVENT,
                                        ESP_EVENT_ANY_ID,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);
    esp_event_handler_instance_register(IP_EVENT,
                                        IP_EVENT_STA_GOT_IP,
                                        &wifi_event_handler,
                                        NULL,
                                        NULL);

    wifi_config_t wifi_config = {
        .sta = {
            .ssid = AIRPLAY_WIFI_SSID,
            .password = AIRPLAY_WIFI_PASS,
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
        },
    };

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);

    wifi_inited = true;
    return ESP_OK;
}

esp_err_t airplay_service_start(void)
{
    if (!wifi_inited) {
        esp_err_t err = airplay_service_init();
        if (err != ESP_OK) return err;
    }

    ESP_LOGI(TAG, "Starting Wi-Fi STA for AirPlay 2...");
    return esp_wifi_start();
}

void airplay_service_stop(void)
{
    if (g_airplay_status.service_active) {
        ESP_LOGI(TAG, "Stopping AirPlay 2 RTSP server and audio output...");
        rtsp_server_stop();
        audio_output_stop();
        g_airplay_status.service_active = false;
    }

    if (wifi_inited) {
        ESP_LOGI(TAG, "Stopping Wi-Fi...");
        esp_wifi_stop();
        g_airplay_status.wifi_state = AIRPLAY_WIFI_DISCONNECTED;
    }
}

airplay_status_t airplay_service_get_status(void)
{
    return g_airplay_status;
}
