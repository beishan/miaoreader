#pragma once
/**
 * @file status_bar.h
 * @brief 全局状态栏（顶部 20px，黑底白字，显示时间/电量/WiFi）
 */
#include "hal/epd.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define STATUS_BAR_HEIGHT 20

/** 订阅 EVT_BATTERY/EVT_WIFI_STATUS 事件，启动时间刷新任务 */
esp_err_t status_bar_init(void);

/** 渲染状态栏到 EPD 帧缓冲（不调用 epd_display_*，由调用者决定刷新方式） */
void status_bar_render(void);

/** 强制刷新时间（每分钟由定时器触发，或按需调用） */
void status_bar_update_time(void);

/** 强制刷新电量区 */
void status_bar_update_battery(void);

/** 强制刷新 WiFi 区 */
void status_bar_update_wifi(bool connected, int rssi);

#ifdef __cplusplus
}
#endif
