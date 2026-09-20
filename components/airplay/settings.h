#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define SETTINGS_DEFAULT_DEVICE_NAME "ESP32 CD Player"

esp_err_t settings_init(void);
esp_err_t settings_get_volume(float *volume_db);
esp_err_t settings_set_volume(float volume_db);
esp_err_t settings_persist_volume(void);

esp_err_t settings_get_device_name(char *name, size_t len);
esp_err_t settings_set_device_name(const char *name);
void settings_device_name_to_hostname(const char *name, char *out, size_t out_len);

esp_err_t settings_get_channel_mode(uint8_t *mode);
esp_err_t settings_set_channel_mode(uint8_t mode);
