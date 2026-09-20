#include "settings.h"
#include <string.h>
#include <ctype.h>
#include "esp_log.h"
#include "airplay_config.h"
#include "myDriver/i2s.h"

static const char *TAG = "airplay_settings";
static float s_current_volume_db = -10.0f; // Default volume ~66%
static uint8_t s_channel_mode = 0; // STEREO

esp_err_t settings_init(void) {
    return ESP_OK;
}

esp_err_t settings_get_volume(float *volume_db) {
    if (volume_db) {
        *volume_db = s_current_volume_db;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t settings_set_volume(float volume_db) {
    s_current_volume_db = volume_db;
    // Map iPhone AirPlay volume [-16.9dB, 0.0dB] to ESP32 CD volume [15, 60]
    int vol_level = 15;
    if (volume_db <= -140.0f) {
        vol_level = 0; // Mute
    } else if (volume_db <= -16.9f) {
        vol_level = 15; // iPhone minimum volume -> ESP32 level 15
    } else if (volume_db >= 0.0f) {
        vol_level = 60; // iPhone maximum volume -> ESP32 level 60
    } else {
        // Linear mapping from [-16.9f, 0.0f] to [15, 60]
        vol_level = 15 + (int)(((volume_db + 16.9f) / 16.9f) * 45.0f + 0.5f);
        if (vol_level < 15) vol_level = 15;
        if (vol_level > 60) vol_level = 60;
    }
    audio_output_set_volume((uint8_t)vol_level);
    ESP_LOGI(TAG, "AirPlay volume set to %.1f dB (mapped to level %d/60)", volume_db, vol_level);
    return ESP_OK;
}

esp_err_t settings_persist_volume(void) {
    return ESP_OK;
}

esp_err_t settings_get_device_name(char *name, size_t len) {
    if (!name || len == 0) return ESP_ERR_INVALID_ARG;
    strncpy(name, AIRPLAY_DEVICE_NAME, len - 1);
    name[len - 1] = '\0';
    return ESP_OK;
}

esp_err_t settings_set_device_name(const char *name) {
    return ESP_OK;
}

void settings_device_name_to_hostname(const char *name, char *out, size_t out_len) {
    if (!out || out_len < 2) return;
    if (!name || strlen(name) == 0) {
        snprintf(out, out_len, "esp32-cd-player");
        return;
    }
    size_t j = 0;
    for (size_t i = 0; name[i] != '\0' && j < out_len - 1; i++) {
        char c = name[i];
        if (isalnum((unsigned char)c)) {
            out[j++] = (char)tolower((unsigned char)c);
        } else if (c == ' ' || c == '_' || c == '-') {
            if (j > 0 && out[j-1] != '-') {
                out[j++] = '-';
            }
        }
    }
    if (j == 0) {
        snprintf(out, out_len, "esp32-cd-player");
    } else {
        out[j] = '\0';
    }
}

esp_err_t settings_get_channel_mode(uint8_t *mode) {
    if (mode) {
        *mode = s_channel_mode;
        return ESP_OK;
    }
    return ESP_ERR_INVALID_ARG;
}

esp_err_t settings_set_channel_mode(uint8_t mode) {
    s_channel_mode = mode;
    return ESP_OK;
}
