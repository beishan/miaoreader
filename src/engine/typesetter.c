/**
 * @file typesetter.c
 * @brief FreeType 排版引擎 + 预分页实现
 *
 * 内存预算：
 *  - 字体文件从 SD 卡按需读取到 PSRAM
 *  - 分页索引按 12 字节/页计算
 *  - 渲染时按页增量渲染到 EPD 帧缓冲
 */
#include "typesetter.h"
#include "esp_log.h"
#include "esp_heap_caps.h"
#include <string.h>
#include <stdlib.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include FT_BITMAP_H

static const char *TAG = "typesetter";

/* 字体路径表（typesetter_register_font 注册） */
#define MAX_FONTS 4
static char s_font_paths[MAX_FONTS][128] = {{0}};

/* FreeType 全局状态 */
static FT_Library s_ft = NULL;
static FT_Face    s_face = NULL;
static uint8_t   *s_font_buf = NULL;
static size_t     s_font_buf_size = 0;
static int        s_current_font_id = -1;

/* 当前排版配置 */
static TypesetterConfig s_cfg = {
    .fontId = 0, .fontSize = 20, .lineSpacing = 15,
    .charSpacing = 0, .margin = 12,
    .pageWidth = 376, .pageHeight = 262,
};

/* 字符宽度（按当前字号） */
static int char_width(uint8_t ch)
{
    if (!s_face) return s_cfg.fontSize / 2;
    if (FT_Load_Char(s_face, ch, FT_LOAD_DEFAULT) == 0) {
        return s_face->glyph->advance.x >> 6;
    }
    return s_cfg.fontSize / 2;
}

/* 行高（px） */
static int line_height(void)
{
    return s_cfg.fontSize * s_cfg.lineSpacing / 10;
}

esp_err_t typesetter_register_font(uint8_t fontId, const char *ttf_path)
{
    if (fontId >= MAX_FONTS || !ttf_path) return ESP_ERR_INVALID_ARG;
    strncpy(s_font_paths[fontId], ttf_path, sizeof(s_font_paths[0]) - 1);
    s_font_paths[fontId][sizeof(s_font_paths[0]) - 1] = '\0';
    return ESP_OK;
}

esp_err_t typesetter_init(const TypesetterConfig *cfg)
{
    if (cfg) s_cfg = *cfg;

    if (!s_ft) {
        FT_Error err = FT_Init_FreeType(&s_ft);
        if (err) {
            ESP_LOGE(TAG, "FT_Init_FreeType 失败: %d", err);
            return ESP_FAIL;
        }
    }
    ESP_LOGI(TAG, "排版器初始化: fontId=%u size=%u margin=%u page=%dx%d",
             s_cfg.fontId, s_cfg.fontSize, s_cfg.margin,
             s_cfg.pageWidth, s_cfg.pageHeight);
    return ESP_OK;
}

void typesetter_reload_config(const TypesetterConfig *cfg)
{
    s_cfg = *cfg;
    if (s_face) {
        FT_Set_Pixel_Sizes(s_face, 0, s_cfg.fontSize);
    }
}

/* 加载字体到内存（PSRAM） */
static esp_err_t load_font_internal(uint8_t fontId)
{
    if (s_current_font_id == fontId && s_face) return ESP_OK;
    if (fontId >= MAX_FONTS || s_font_paths[fontId][0] == '\0') {
        ESP_LOGW(TAG, "字体 %u 未注册", fontId);
        return ESP_ERR_NOT_FOUND;
    }

    /* 释放旧字体 */
    if (s_face) {
        FT_Done_Face(s_face);
        s_face = NULL;
    }
    if (s_font_buf) {
        free(s_font_buf);
        s_font_buf = NULL;
        s_font_buf_size = 0;
    }

    /* 从 SD 卡读取 TTF 到 PSRAM */
    FILE *f = fopen(s_font_paths[fontId], "rb");
    if (!f) {
        ESP_LOGE(TAG, "打开字体文件失败: %s", s_font_paths[fontId]);
        return ESP_ERR_NOT_FOUND;
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (sz <= 0 || sz > 4 * 1024 * 1024) {
        ESP_LOGE(TAG, "字体文件大小异常: %ld", sz);
        fclose(f);
        return ESP_ERR_INVALID_SIZE;
    }
    ESP_LOGI(TAG, "加载字体: %s (%ld bytes), PSRAM 可用: %u bytes",
             s_font_paths[fontId], sz,
             (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    s_font_buf = heap_caps_malloc(sz, MALLOC_CAP_SPIRAM);
    if (!s_font_buf) {
        ESP_LOGE(TAG, "PSRAM 分配失败，尝试内部 SRAM");
        s_font_buf = malloc(sz);
        if (!s_font_buf) {
            fclose(f);
            return ESP_ERR_NO_MEM;
        }
    }
    fread(s_font_buf, 1, sz, f);
    fclose(f);
    s_font_buf_size = sz;

    /* FreeType 从内存加载 face */
    FT_Error err = FT_New_Memory_Face(s_ft, s_font_buf, sz, 0, &s_face);
    if (err) {
        ESP_LOGE(TAG, "FT_New_Memory_Face 失败: %d", err);
        free(s_font_buf);
        s_font_buf = NULL;
        return ESP_FAIL;
    }
    FT_Set_Pixel_Sizes(s_face, 0, s_cfg.fontSize);
    s_current_font_id = fontId;
    ESP_LOGI(TAG, "字体加载完成: id=%u size=%u", fontId, s_cfg.fontSize);
    return ESP_OK;
}

esp_err_t typesetter_paginate(const char *text, uint32_t textLen,
                              PageIndex **pages_out, uint32_t *page_count_out)
{
    if (!text || !pages_out || !page_count_out || textLen == 0) {
        return ESP_ERR_INVALID_ARG;
    }
    *pages_out = NULL;
    *page_count_out = 0;

    /* 懒加载字体（失败不阻塞，使用启发式） */
    (void)load_font_internal(s_cfg.fontId);

    int avail_w = s_cfg.pageWidth;
    int line_h = line_height();
    int lines_per_page = s_cfg.pageHeight / line_h;
    if (lines_per_page < 1) lines_per_page = 1;

    /* 估算初始页数 */
    uint32_t est_pages = (textLen / (lines_per_page * 30)) + 1;
    if (est_pages < 4) est_pages = 4;
    /* PageIndex 使用内部 SRAM（每页仅 8 字节，1000 页才 8KB，内部 SRAM 更快） */
    PageIndex *pages = heap_caps_calloc(est_pages, sizeof(PageIndex), MALLOC_CAP_INTERNAL);
    if (!pages) return ESP_ERR_NO_MEM;

    uint32_t page_count = 0;
    uint32_t i = 0;
    int cur_line = 0;

    pages[page_count].startByte = 0;
    pages[page_count].pageNum = 1;
    page_count = 1;

    while (i < textLen) {
        /* 扫描一行 */
        uint32_t line_start = i;
        int w = 0;
        while (i < textLen && text[i] != '\n') {
            int cw = char_width((uint8_t)text[i]) + s_cfg.charSpacing;
            if (w + cw > avail_w && i > line_start) break;
            w += cw;
            i++;
        }
        cur_line++;

        /* 段尾换行：消费 \n */
        if (i < textLen && text[i] == '\n') i++;

        /* 检查是否需要分页 */
        if (cur_line >= lines_per_page && i < textLen) {
            cur_line = 0;
            if (page_count >= est_pages) {
                est_pages *= 2;
                PageIndex *np = heap_caps_realloc(pages,
                    est_pages * sizeof(PageIndex), MALLOC_CAP_INTERNAL);
                if (!np) {
                    free(pages);
                    return ESP_ERR_NO_MEM;
                }
                pages = np;
            }
            pages[page_count].startByte = i;
            pages[page_count].pageNum = page_count + 1;
            page_count++;
        }
    }

    *pages_out = pages;
    *page_count_out = page_count;
    ESP_LOGI(TAG, "分页完成: %u 字节 → %u 页", (unsigned)textLen, (unsigned)page_count);
    return ESP_OK;
}

void typesetter_free_pages(PageIndex *pages)
{
    if (pages) free(pages);
}

esp_err_t typesetter_render_page(const char *text, const PageIndex *page,
                                  uint8_t *epd_buf, int buf_w, int buf_h, int y_offset)
{
    if (!text || !page) return ESP_ERR_INVALID_ARG;
    if (!s_face) {
        esp_err_t err = load_font_internal(s_cfg.fontId);
        if (err != ESP_OK) return err;
    }
    /* 重置 framebuffer（仅在直接传入时） */
    if (epd_buf) {
        memset(epd_buf, 0xFF, buf_w * buf_h / 8);
    }

    int x = s_cfg.margin;
    int y = y_offset + s_cfg.margin;
    int line_h = line_height();
    int avail_h = s_cfg.pageHeight - 2 * s_cfg.margin;
    int lines_drawn = 0;

    uint32_t i = page->startByte;
    while (text[i] != '\0' && lines_drawn * line_h < avail_h) {
        if (text[i] == '\n') {
            x = s_cfg.margin;
            y += line_h;
            i++;
            lines_drawn++;
            continue;
        }
        uint32_t cp = (uint8_t)text[i];
        if (FT_Load_Char(s_face, cp, FT_LOAD_RENDER) == 0) {
            FT_Bitmap *bmp = &s_face->glyph->bitmap;
            int gx = x + s_face->glyph->bitmap_left;
            int gy = y - s_face->glyph->bitmap_top;
            for (int row = 0; row < (int)bmp->rows; row++) {
                int py = gy + row;
                if (py < 0 || py >= buf_h) continue;
                for (int col = 0; col < (int)bmp->width; col++) {
                    int px = gx + col;
                    if (px < 0 || px >= buf_w) continue;
                    if (bmp->buffer[row * bmp->pitch + col] > 128) {
                        int byte_idx = (py * buf_w + px) / 8;
                        int bit_idx = 7 - (px % 8);
                        if (epd_buf) {
                            epd_buf[byte_idx] &= ~(1 << bit_idx);  /* 0=黑 */
                        } else {
                            extern void epd_set_pixel(int, int, int);
                            epd_set_pixel(px, py, 0);
                        }
                    }
                }
            }
            x += s_face->glyph->advance.x >> 6;
        }
        x += s_cfg.charSpacing;
        i++;
    }
    return ESP_OK;
}
