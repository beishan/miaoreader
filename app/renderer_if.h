/**
 * @file renderer_if.h
 * @brief 抽象渲染接口：统一 EPD 驱动和 PC 仿真
 */
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 屏幕尺寸 */
#define RENDERER_WIDTH   400
#define RENDERER_HEIGHT  300

/* 颜色定义（兼容 EPD 三色） */
typedef enum {
    RENDERER_COLOR_BLACK = 0,
    RENDERER_COLOR_WHITE = 1,
    RENDERER_COLOR_RED   = 2,
} RendererColor;

/* 按键 ID（兼容现有按键定义） */
typedef enum {
    RENDERER_KEY_PWR,
    RENDERER_KEY_PREV,
    RENDERER_KEY_NEXT,
    RENDERER_KEY_HOME,
    RENDERER_KEY_COUNT,
} RendererKeyId;

/* 按键事件 */
typedef enum {
    RENDERER_KEY_EVT_SHORT,      // 短按
    RENDERER_KEY_EVT_LONG,       // 长按
    RENDERER_KEY_EVT_SUPER_LONG, // 超长按
    RENDERER_KEY_EVT_COMBO,      // 组合按
} RendererKeyEvent;

/* 按键回调函数类型 */
typedef void (*renderer_key_callback_t)(RendererKeyId key, RendererKeyEvent event);

/**
 * @brief 初始化渲染器
 *
 * @param title 窗口标题（PC 仿真用）
 * @return 0 成功
 */
int renderer_init(const char *title);

/**
 * @brief 清屏
 *
 * @param color 填充颜色
 */
void renderer_clear(RendererColor color);

/**
 * @brief 设置单个像素
 *
 * @param x X 坐标
 * @param y Y 坐标
 * @param color 颜色
 */
void renderer_set_pixel(int x, int y, RendererColor color);

/**
 * @brief 绘制矩形边框
 *
 * @param x 起始 X
 * @param y 起始 Y
 * @param w 宽度
 * @param h 高度
 * @param color 颜色
 */
void renderer_draw_rect(int x, int y, int w, int h, RendererColor color);

/**
 * @brief 绘制填充矩形
 *
 * @param x 起始 X
 * @param y 起始 Y
 * @param w 宽度
 * @param h 高度
 * @param color 颜色
 */
void renderer_fill_rect(int x, int y, int w, int h, RendererColor color);

/**
 * @brief 刷新显示（全刷）
 */
void renderer_display(void);

/**
 * @brief 局部刷新
 */
void renderer_display_partial(void);

/**
 * @brief 进入休眠
 */
void renderer_sleep(void);

/**
 * @brief 设置按键回调
 *
 * @param callback 回调函数
 */
void renderer_set_key_callback(renderer_key_callback_t callback);

/**
 * @brief 运行事件循环（PC 仿真用）
 *
 * 在 ESP32 上为空实现，在 PC 上处理 SDL 事件。
 * 应在主循环中定期调用。
 *
 * @return 0 继续运行，1 退出
 */
int renderer_poll_events(void);

/**
 * @brief 延时函数（兼容 FreeRTOS）
 *
 * @param ms 毫秒
 */
void renderer_delay(uint32_t ms);

/**
 * @brief 绘制单个字符（5x7 像素字体）
 *
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param c 字符（ASCII 0x20-0x7E）
 * @param color 颜色
 */
void renderer_draw_char(int x, int y, char c, RendererColor color);

/**
 * @brief 绘制文本字符串
 *
 * @param x 起始 X 坐标
 * @param y 起始 Y 坐标
 * @param text 文本字符串
 * @param color 颜色
 */
void renderer_draw_text(int x, int y, const char *text, RendererColor color);

/**
 * @brief 测量文本宽度（像素）
 *
 * @param text 文本字符串
 * @return 宽度（像素）
 */
int renderer_text_width(const char *text);

#ifdef __cplusplus
}
#endif
