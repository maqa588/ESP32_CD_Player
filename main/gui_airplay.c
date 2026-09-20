#include "gui_airplay.h"
#include <stdio.h>
#include <string.h>
#include <math.h>

#include "st7735.h"
#include "airplay_service.h"
#include "rtsp_events.h"
#include "i2s.h"

#define AREA_STATUS_BAR_HEIGHT 15

static lv_obj_t *area_topBar = NULL;
static lv_obj_t *area_bottomBar = NULL;
static lv_obj_t *area_player = NULL;

static lv_obj_t *lb_modeTag = NULL;
static lv_obj_t *lb_wifiStatus = NULL;

static lv_obj_t *lb_trackTitle = NULL;
static lv_obj_t *lb_trackArtist = NULL;
static lv_obj_t *lb_playState = NULL;
static lv_obj_t *lb_modeHint = NULL;
static lv_obj_t *lb_volume = NULL;

static lv_obj_t *bar_meterLeft = NULL;
static lv_obj_t *bar_meterRight = NULL;

typedef struct {
    char title[64];
    char artist[64];
    char album[64];
    bool is_playing;
    bool is_connected;
} airplay_gui_meta_t;

static airplay_gui_meta_t s_meta = {
    .title = "ESP32 AirPlay 2",
    .artist = "Ready to cast audio",
    .album = "",
    .is_playing = false,
    .is_connected = false,
};

static void on_rtsp_event(rtsp_event_t event, const rtsp_event_data_t *data, void *user_data)
{
    (void)user_data;
    switch (event) {
    case RTSP_EVENT_CLIENT_CONNECTED:
        s_meta.is_connected = true;
        break;
    case RTSP_EVENT_DISCONNECTED:
        s_meta.is_connected = false;
        s_meta.is_playing = false;
        snprintf(s_meta.title, sizeof(s_meta.title), "ESP32 AirPlay 2");
        snprintf(s_meta.artist, sizeof(s_meta.artist), "Ready to cast audio");
        break;
    case RTSP_EVENT_PLAYING:
        s_meta.is_playing = true;
        break;
    case RTSP_EVENT_PAUSED:
        s_meta.is_playing = false;
        break;
    case RTSP_EVENT_METADATA:
        if (data) {
            if (strlen(data->metadata.title) > 0) {
                snprintf(s_meta.title, sizeof(s_meta.title), "%s", data->metadata.title);
            }
            if (strlen(data->metadata.artist) > 0) {
                snprintf(s_meta.artist, sizeof(s_meta.artist), "%s", data->metadata.artist);
            }
            if (strlen(data->metadata.album) > 0) {
                snprintf(s_meta.album, sizeof(s_meta.album), "%s", data->metadata.album);
            }
        }
        break;
    default:
        break;
    }
}

void gui_airplay_init(void)
{
    lv_color_t color_background = lv_color_make(0x10, 0x14, 0x1c); // subtle dark blue tint
    lv_color_t color_foreground = lv_color_make(0x18, 0x20, 0x2b);

    lv_obj_t *screen = lv_scr_act();

    // 1. Top Bar
    area_topBar = lv_obj_create(screen);
    lv_obj_set_size(area_topBar, LCD_W, AREA_STATUS_BAR_HEIGHT);
    lv_obj_align(area_topBar, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(area_topBar, color_foreground, 0);
    lv_obj_set_style_radius(area_topBar, 0, 0);
    lv_obj_set_style_border_width(area_topBar, 0, 0);
    lv_obj_set_style_pad_all(area_topBar, 0, 0);

    lb_modeTag = lv_label_create(area_topBar);
    lv_label_set_text(lb_modeTag, LV_SYMBOL_AUDIO " AirPlay 2");
    lv_obj_align(lb_modeTag, LV_ALIGN_LEFT_MID, 4, 0);
    lv_obj_set_style_text_color(lb_modeTag, lv_color_make(0x38, 0x90, 0xFF), 0); // Apple blue accent

    lb_wifiStatus = lv_label_create(area_topBar);
    lv_label_set_text(lb_wifiStatus, "WiFi: Init...");
    lv_obj_align(lb_wifiStatus, LV_ALIGN_RIGHT_MID, -4, 0);

    // 2. Bottom Bar
    area_bottomBar = lv_obj_create(screen);
    lv_obj_set_size(area_bottomBar, LCD_W, AREA_STATUS_BAR_HEIGHT);
    lv_obj_align(area_bottomBar, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(area_bottomBar, color_foreground, 0);
    lv_obj_set_style_radius(area_bottomBar, 0, 0);
    lv_obj_set_style_border_width(area_bottomBar, 0, 0);
    lv_obj_set_style_pad_all(area_bottomBar, 0, 0);

    lb_playState = lv_label_create(area_bottomBar);
    lv_label_set_text(lb_playState, LV_SYMBOL_PAUSE);
    lv_obj_align(lb_playState, LV_ALIGN_LEFT_MID, 4, 0);

    lb_modeHint = lv_label_create(area_bottomBar);
    lv_label_set_text(lb_modeHint, "Eject:CD");
    lv_obj_align(lb_modeHint, LV_ALIGN_CENTER, -6, 0);
    lv_obj_set_style_text_color(lb_modeHint, lv_color_make(0x80, 0x80, 0x80), 0);

    lb_volume = lv_label_create(area_bottomBar);
    lv_label_set_text(lb_volume, LV_SYMBOL_VOLUME_MAX "25");
    lv_obj_align(lb_volume, LV_ALIGN_RIGHT_MID, -4, 0);

    // 3. Central Player Area
    area_player = lv_obj_create(screen);
    int center_height = LCD_H - (AREA_STATUS_BAR_HEIGHT * 2);
    lv_obj_set_size(area_player, LCD_W - 12, center_height);
    lv_obj_align(area_player, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_bg_color(area_player, color_background, 0);
    lv_obj_set_style_radius(area_player, 0, 0);
    lv_obj_set_style_border_width(area_player, 0, 0);
    lv_obj_set_style_pad_all(area_player, 0, 0);

    lb_trackTitle = lv_label_create(area_player);
    lv_label_set_long_mode(lb_trackTitle, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_width(lb_trackTitle, LCD_W - 16);
    lv_label_set_text(lb_trackTitle, "ESP32 AirPlay 2");
    lv_obj_set_style_text_align(lb_trackTitle, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lb_trackTitle, LV_ALIGN_TOP_MID, 0, 6);

    lb_trackArtist = lv_label_create(area_player);
    lv_label_set_long_mode(lb_trackArtist, LV_LABEL_LONG_DOT);
    lv_obj_set_width(lb_trackArtist, LCD_W - 16);
    lv_label_set_text(lb_trackArtist, "Ready to cast audio");
    lv_obj_set_style_text_color(lb_trackArtist, lv_color_make(0xAA, 0xAA, 0xAA), 0);
    lv_obj_set_style_text_align(lb_trackArtist, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(lb_trackArtist, LV_ALIGN_BOTTOM_MID, 0, -6);

    // 4. Side Audio Meters
    bar_meterLeft = lv_bar_create(screen);
    lv_obj_set_size(bar_meterLeft, 3, center_height);
    lv_obj_align(bar_meterLeft, LV_ALIGN_LEFT_MID, 1, 0);
    lv_bar_set_range(bar_meterLeft, 0, 96);
    lv_obj_set_style_bg_color(bar_meterLeft, lv_color_make(0x30, 0x30, 0x30), 0);
    lv_obj_set_style_bg_color(bar_meterLeft, lv_color_make(0x38, 0x90, 0xFF), LV_PART_INDICATOR);

    bar_meterRight = lv_bar_create(screen);
    lv_obj_set_size(bar_meterRight, 3, center_height);
    lv_obj_align(bar_meterRight, LV_ALIGN_RIGHT_MID, -1, 0);
    lv_bar_set_range(bar_meterRight, 0, 96);
    lv_obj_set_style_bg_color(bar_meterRight, lv_color_make(0x30, 0x30, 0x30), 0);
    lv_obj_set_style_bg_color(bar_meterRight, lv_color_make(0x38, 0x90, 0xFF), LV_PART_INDICATOR);

    // Register RTSP playback events listener
    rtsp_events_register(on_rtsp_event, NULL);

    // Initially hidden (defaults to CD Player mode)
    gui_airplay_set_visible(false);
}

void gui_airplay_set_visible(bool visible)
{
    if (area_topBar == NULL)
        return;

    if (visible)
    {
        lv_obj_clear_flag(area_topBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(area_bottomBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(area_player, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bar_meterLeft, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(bar_meterRight, LV_OBJ_FLAG_HIDDEN);
    }
    else
    {
        lv_obj_add_flag(area_topBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(area_bottomBar, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(area_player, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar_meterLeft, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(bar_meterRight, LV_OBJ_FLAG_HIDDEN);
    }
}

void gui_airplay_update(void)
{
    if (area_topBar == NULL)
        return;

    airplay_status_t status = airplay_service_get_status();

    // 1. Wi-Fi status
    char wifi_buf[32];
    if (status.wifi_state == AIRPLAY_WIFI_CONNECTED_STA || status.wifi_state == AIRPLAY_WIFI_CONNECTED_AP)
    {
        snprintf(wifi_buf, sizeof(wifi_buf), "%s", status.ip_addr);
    }
    else if (status.wifi_state == AIRPLAY_WIFI_CONNECTING)
    {
        snprintf(wifi_buf, sizeof(wifi_buf), "Connecting...");
    }
    else
    {
        snprintf(wifi_buf, sizeof(wifi_buf), "WiFi Discon");
    }
    lv_label_set_text(lb_wifiStatus, wifi_buf);

    // 2. Playback state
    if (s_meta.is_playing)
    {
        lv_label_set_text(lb_playState, LV_SYMBOL_PLAY);
    }
    else
    {
        lv_label_set_text(lb_playState, LV_SYMBOL_PAUSE);
    }

    // 3. Track title & artist
    lv_label_set_text(lb_trackTitle, s_meta.title);
    lv_label_set_text(lb_trackArtist, s_meta.artist);

    // 4. Volume
    uint8_t vol = audio_output_get_volume();
    lv_label_set_text_fmt(lb_volume, LV_SYMBOL_VOLUME_MAX "%02d", vol);
}

void gui_airplay_set_meter(int l, int r)
{
    if (bar_meterLeft && bar_meterRight)
    {
        lv_bar_set_value(bar_meterLeft, l, LV_ANIM_OFF);
        lv_bar_set_value(bar_meterRight, r, LV_ANIM_OFF);
    }
}
