#pragma once
/**
 * @file widget.h
 * @brief 点阵字体基础控件：字符/字符串/图标渲染
 *
 * 字模数据 (data/fonts/ui_font_16.bin) 通过 ESP-IDF EMBED_FILES 嵌入固件，
 * 不依赖 SPIFFS/SD。字模布局：95 个 ASCII (0x20..0x7E) + 16 个图标 (0x80..0x8F)。
 */
#include "hal/epd.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** 初始化：从嵌入字模复制到 PSRAM 缓存（可选，懒加载） */
void widget_init(void);

/** 绘制单个 ASCII 字符 (0x20..0x7E)。\r 不支持；' ' 为空格占位。 */
void widget_draw_char(int x, int y, char c, EpdColor color);

/** 绘制 ASCII 字符串，遇到 '\\0' 停止。支持 '\\n' 换行（行高 18px）。 */
void widget_draw_text(int x, int y, const char *text, EpdColor color);

/** 测量字符串像素宽度（不含 \\n）。 */
int widget_text_width(const char *text);

/** 绘制图标 (icon_id 范围 0x80..0x8F，见 ICON_* 常量)。 */
void widget_draw_icon(int x, int y, uint16_t icon_id, EpdColor color);

/* 图标 ID */
#define ICON_BATTERY_FULL      0x80
#define ICON_BATTERY_75        0x81
#define ICON_BATTERY_50        0x82
#define ICON_BATTERY_25        0x83
#define ICON_WIFI_CONNECTED    0x84
#define ICON_WIFI_DISCONNECTED 0x85
#define ICON_CHARGING          0x86
#define ICON_SUNNY             0x87
#define ICON_CLOUDY            0x88
#define ICON_RAINY             0x89
#define ICON_BOOK              0x8A
#define ICON_CLOCK             0x8B
#define ICON_ARROW_LEFT        0x8C
#define ICON_ARROW_RIGHT       0x8D
#define ICON_CHECK             0x8E
#define ICON_CROSS             0x8F

#ifdef __cplusplus
}
#endif
