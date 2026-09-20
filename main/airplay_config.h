#ifndef _AIRPLAY_CONFIG_H_
#define _AIRPLAY_CONFIG_H_

#include "sdkconfig.h"

/*
 * ====================================================================
 *                 ESP32 AirPlay Wi-Fi 连接配置
 * ====================================================================
 * 你可以直接在此处修改你的 Wi-Fi 路由器名称（SSID）和密码（Password）。
 * 也可以在终端执行 `idf.py menuconfig` 进入 "AirPlay Configuration" 界面进行配置。
 * ====================================================================
 */

#ifdef CONFIG_AIRPLAY_WIFI_SSID
#define AIRPLAY_WIFI_SSID       CONFIG_AIRPLAY_WIFI_SSID
#else
#define AIRPLAY_WIFI_SSID       "607"        // <-- 请在此修改你的 Wi-Fi 名称
#endif

#ifdef CONFIG_AIRPLAY_WIFI_PASSWORD
#define AIRPLAY_WIFI_PASS       CONFIG_AIRPLAY_WIFI_PASSWORD
#else
#define AIRPLAY_WIFI_PASS       "607zhendetai6"    // <-- 请在此修改你的 Wi-Fi 密码
#endif

#ifdef CONFIG_AIRPLAY_DEVICE_NAME
#define AIRPLAY_DEVICE_NAME     CONFIG_AIRPLAY_DEVICE_NAME
#else
#define AIRPLAY_DEVICE_NAME     "ESP32-CD-AirPlay"      // <-- 隔空播放设备名称
#endif

#endif // _AIRPLAY_CONFIG_H_
