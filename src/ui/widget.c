/**
 * @file widget.c
 * @brief 点阵字体渲染实现（16×16，1bpp，行优先高位在前）
 */
#include "widget.h"
#include "font_data.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>

static const char *TAG = "widget";

#define GLYPH_W      16
#define GLYPH_H      16
#define GLYPH_BYTES  32        /* 16*16/8 */
#define ASCII_BASE   0x20      /* ' ' */
#define ICON_BASE    0x80      /* 图标起始字节索引 = 95*32 */

/* 字模直接编译进 flash（font_data.h 由 scripts/embed_font.py 生成） */
#define s_font        (ui_font_16)
#define s_font_size   (ui_font_16_len)

void widget_init(void)
{
    ESP_LOGI(TAG, "字模就绪: %u 字节, %u 字模",
             (unsigned)s_font_size,
             (unsigned)(s_font_size / GLYPH_BYTES));
}

static const uint8_t *get_glyph(char c)
{
    if ((uint8_t)c >= ASCII_BASE && (uint8_t)c < 0x7F) {
        /* ASCII 字模在数组开头 */
        return s_font + ((uint8_t)c - ASCII_BASE) * GLYPH_BYTES;
    }
    if ((uint8_t)c >= ICON_BASE && (uint8_t)c < ICON_BASE + 16) {
        /* 图标在 ASCII 之后 */
        return s_font + (ICON_BASE - ASCII_BASE + ((uint8_t)c - ICON_BASE)) * GLYPH_BYTES;
    }
    return NULL;
}

static void draw_glyph_bitmap(int x0, int y0, const uint8_t *g, EpdColor color)
{
    for (int row = 0; row < GLYPH_H; row++) {
        /* 每行 2 字节，bit15(左) 在前 */
        uint8_t b0 = g[row * 2];
        uint8_t b1 = g[row * 2 + 1];
        for (int col = 0; col < GLYPH_W; col++) {
            uint8_t bit;
            if (col < 8)  bit = (b0 >> (7 - col)) & 1;
            else          bit = (b1 >> (7 - (col - 8))) & 1;
            if (bit) {
                epd_set_pixel(x0 + col, y0 + row, color);
            }
        }
    }
}

void widget_draw_char(int x, int y, char c, EpdColor color)
{
    const uint8_t *g = get_glyph(c);
    if (!g) return;
    draw_glyph_bitmap(x, y, g, color);
}

void widget_draw_text(int x, int y, const char *text, EpdColor color)
{
    if (!text) return;
    int cursor_x = x;
    int cursor_y = y;
    while (*text) {
        char c = (char)*text++;
        if (c == '\n') {
            cursor_x = x;
            cursor_y += GLYPH_H + 2;   /* 换行 + 2px 行间距 */
            continue;
        }
        const uint8_t *g = get_glyph(c);
        if (g) {
            draw_glyph_bitmap(cursor_x, cursor_y, g, color);
        }
        cursor_x += GLYPH_W;
    }
}

int widget_text_width(const char *text)
{
    if (!text) return 0;
    int w = 0, max_w = 0;
    while (*text) {
        if (*text == '\n') {
            if (w > max_w) max_w = w;
            w = 0;
        } else {
            w += GLYPH_W;
        }
        text++;
    }
    return w > max_w ? w : max_w;
}

void widget_draw_icon(int x, int y, uint16_t icon_id, EpdColor color)
{
    if (icon_id < ICON_BASE || icon_id >= ICON_BASE + 16) return;
    const uint8_t *g = get_glyph((char)icon_id);
    if (!g) return;
    draw_glyph_bitmap(x, y, g, color);
}
