/**
 * @file renderer_eink.c
 * @brief ESP32 EPD 驱动渲染器封装
 *
 * 将现有 epd.c 封装为 renderer_if 接口
 */
#ifndef PC_SIMULATION

#include "renderer_if.h"
#include "hal/epd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* 颜色映射：RendererColor -> EpdColor */
static const int color_map[] = {
    EPD_COLOR_BLACK,  // RENDERER_COLOR_BLACK
    EPD_COLOR_WHITE,  // RENDERER_COLOR_WHITE
    EPD_COLOR_RED,    // RENDERER_COLOR_RED
};

int renderer_init(const char *title)
{
    (void)title;
    return epd_init() == ESP_OK ? 0 : -1;
}

void renderer_clear(RendererColor color)
{
    epd_clear(color_map[color]);
}

void renderer_set_pixel(int x, int y, RendererColor color)
{
    epd_set_pixel(x, y, color_map[color]);
}

void renderer_draw_rect(int x, int y, int w, int h, RendererColor color)
{
    epd_draw_rect(x, y, w, h, color_map[color]);
}

void renderer_fill_rect(int x, int y, int w, int h, RendererColor color)
{
    epd_fill_rect(x, y, w, h, color_map[color]);
}

void renderer_display(void)
{
    epd_display_full();
}

void renderer_display_partial(void)
{
    epd_display_partial();
}

void renderer_sleep(void)
{
    epd_sleep();
}

void renderer_set_key_callback(renderer_key_callback_t callback)
{
    /* ESP32 上通过 keys.c 处理按键，这里为空实现 */
    (void)callback;
}

int renderer_poll_events(void)
{
    /* ESP32 上不需要轮询事件 */
    return 0;
}

void renderer_delay(uint32_t ms)
{
    vTaskDelay(pdMS_TO_TICKS(ms));
}

#endif /* !PC_SIMULATION */
