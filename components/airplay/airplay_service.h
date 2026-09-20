#ifndef _AIRPLAY_SERVICE_H_
#define _AIRPLAY_SERVICE_H_

#include <stdbool.h>
#include "esp_err.h"
#include "airplay_config.h"

typedef enum
{
    AIRPLAY_WIFI_DISCONNECTED,
    AIRPLAY_WIFI_CONNECTING,
    AIRPLAY_WIFI_CONNECTED_STA,
    AIRPLAY_WIFI_CONNECTED_AP,
} airplay_wifi_state_t;

typedef struct
{
    airplay_wifi_state_t wifi_state;
    char ip_addr[32];
    char device_name[32];
    bool service_active;
} airplay_status_t;

esp_err_t airplay_service_init(void);
esp_err_t airplay_service_start(void);
void airplay_service_stop(void);
airplay_status_t airplay_service_get_status(void);

#endif // _AIRPLAY_SERVICE_H_
