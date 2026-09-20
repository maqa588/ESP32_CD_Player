#ifndef _APP_MODE_H_
#define _APP_MODE_H_

#include <stdbool.h>

typedef enum
{
    APP_MODE_CD = 0,
    APP_MODE_AIRPLAY = 1,
} app_mode_t;

app_mode_t app_mode_get(void);
void app_mode_set(app_mode_t mode);
void app_mode_toggle(void);

// Mode change event listener / check
bool app_mode_has_changed(void);
void app_mode_clear_changed(void);

#endif // _APP_MODE_H_
