#include "app_mode.h"
#include "esp_log.h"
#include "cdPlayer.h"
#include "i2s.h"
#include "airplay_service.h"

#define TAG "app_mode"

static app_mode_t current_mode = APP_MODE_CD;
static volatile bool mode_changed = true;

app_mode_t app_mode_get(void)
{
    return current_mode;
}

void app_mode_set(app_mode_t mode)
{
    if (current_mode == mode)
        return;

    ESP_LOGI(TAG, "Switching mode from [%s] to [%s]",
             current_mode == APP_MODE_CD ? "CD" : "AIRPLAY",
             mode == APP_MODE_CD ? "CD" : "AIRPLAY");

    if (current_mode == APP_MODE_CD && mode == APP_MODE_AIRPLAY)
    {
        // 1. Pause CD playback
        cdplayer_playerInfo.playing = 0;
        // 2. Flush audio buffer to avoid residual sound
        audio_output_flush();
        // 3. Start AirPlay Wi-Fi & RTSP stack
        airplay_service_start();
    }
    else if (current_mode == APP_MODE_AIRPLAY && mode == APP_MODE_CD)
    {
        // 1. Stop AirPlay stack
        airplay_service_stop();
        // 2. Flush audio buffer
        audio_output_flush();
    }

    current_mode = mode;
    mode_changed = true;
}

void app_mode_toggle(void)
{
    if (current_mode == APP_MODE_CD)
    {
        app_mode_set(APP_MODE_AIRPLAY);
    }
    else
    {
        app_mode_set(APP_MODE_CD);
    }
}

bool app_mode_has_changed(void)
{
    return mode_changed;
}

void app_mode_clear_changed(void)
{
    mode_changed = false;
}
