#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#define EPD_WIDTH  400
#define EPD_HEIGHT 300
#define EPD_BUF_SIZE (EPD_WIDTH * EPD_HEIGHT / 4)

typedef enum {
    EPD_COLOR_BLACK = 0,
    EPD_COLOR_WHITE = 1,
    EPD_COLOR_RED   = 2,
} EpdColor;

esp_err_t epd_init(void);
void epd_clear(EpdColor color);
void epd_set_pixel(int x, int y, EpdColor color);
void epd_draw_rect(int x, int y, int w, int h, EpdColor color);
void epd_fill_rect(int x, int y, int w, int h, EpdColor color);
void epd_display_full(void);
void epd_display_partial(void);
void epd_sleep(void);
void epd_set_rotation(uint8_t rot);
