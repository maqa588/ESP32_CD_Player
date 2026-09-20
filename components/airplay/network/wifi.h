#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdio.h>
#include "esp_mac.h"
#include "esp_err.h"

static inline void wifi_get_mac_str(char *mac_str, size_t len) {
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    snprintf(mac_str, len, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

static inline bool wifi_is_connected(void) {
    return true;
}
