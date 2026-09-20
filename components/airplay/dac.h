#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include "settings.h"

static inline void dac_set_volume(float volume_db) {
    settings_set_volume(volume_db);
}
static inline void dac_on_i2s_started(void) {}
static inline void dac_enable_speaker(bool enable) { (void)enable; }
static inline void dac_enable_line_out(bool enable) { (void)enable; }
