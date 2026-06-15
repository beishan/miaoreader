/**
 * @file power_mgr.h
 * @brief 电源管理模块：空闲休眠、深度睡眠、低电量保护
 */
#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化电源管理器
 *
 * 创建空闲计时器和电池监控任务。
 * 需要在所有硬件初始化完成后调用。
 */
esp_err_t power_mgr_init(void);

/**
 * @brief 重置空闲计时器
 *
 * 每次按键活动时调用，重置自动休眠倒计时。
 */
void power_mgr_reset_idle(void);

/**
 * @brief 进入休眠页面
 *
 * 刷新 E-Ink 显示休眠界面，然后进入深度睡眠。
 */
void power_mgr_enter_sleep(void);

/**
 * @brief 进入深度睡眠
 *
 * 配置 GPIO 唤醒（PWR 键），关闭外设，进入 ESP32 深度睡眠。
 */
void power_mgr_deep_sleep(void);

/**
 * @brief 检查是否处于低电量状态
 */
bool power_mgr_is_low_battery(void);

#ifdef __cplusplus
}
#endif
