/**
 * @file widget.h
 * @brief Widget 绘制接口（共享层）
 */
#pragma once

#include <stdint.h>
#include "renderer_if.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 图标 ID */
#define ICON_BATTERY_FULL    0x80
#define ICON_BATTERY_75      0x81
#define ICON_BATTERY_50      0x82
#define ICON_BATTERY_25      0x83
#define ICON_WIFI_CONNECTED  0x84
#define ICON_WIFI_DISCONNECTED 0x85
#define ICON_CHARGING        0x86
#define ICON_SUNNY           0x87
#define ICON_CLOUDY          0x88
#define ICON_RAINY           0x89
#define ICON_BOOK            0x8A
#define ICON_CLOCK           0x8B
#define ICON_ARROW_LEFT      0x8C
#define ICON_ARROW_RIGHT     0x8D
#define ICON_CHECK           0x8E
#define ICON_CROSS           0x8F

/**
 * @brief 初始化 widget
 */
void widget_init(void);

/**
 * @brief 绘制单个字符
 *
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param c 字符
 * @param color 颜色
 */
void widget_draw_char(int x, int y, char c, RendererColor color);

/**
 * @brief 绘制文本字符串
 *
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param text 文本
 * @param color 颜色
 */
void widget_draw_text(int x, int y, const char *text, RendererColor color);

/**
 * @brief 测量文本宽度
 *
 * @param text 文本
 * @return 宽度（像素）
 */
int widget_text_width(const char *text);

/**
 * @brief 绘制图标
 *
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param icon_id 图标 ID
 * @param color 颜色
 */
void widget_draw_icon(int x, int y, uint16_t icon_id, RendererColor color);

#ifdef __cplusplus
}
#endif
