/**
 * @file status_bar.h
 * @brief 状态栏（共享层）
 */
#pragma once

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化状态栏
 */
esp_err_t status_bar_init(void);

/**
 * @brief 渲染状态栏
 */
void status_bar_render(void);

#ifdef __cplusplus
}
#endif
