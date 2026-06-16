#ifdef PC_SIMULATION

#include "typesetter.h"
#include <ft2build.h>
#include FT_FREETYPE_H
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_FONTS      4
#define MAX_FONT_PATH  256
#define INITIAL_PAGES  64

// 全局状态
static char s_font_paths[MAX_FONTS][MAX_FONT_PATH];
static FT_Library s_ft = NULL;
static FT_Face s_face = NULL;
static int s_current_font_id = -1;
static TypesetterConfig s_cfg = {
    .fontSize = TYPESETTER_DEFAULT_FONT_SIZE,
    .lineSpacing = TYPESETTER_DEFAULT_LINE_SPACING,
    .charSpacing = TYPESETTER_DEFAULT_CHAR_SPACING,
    .margin = TYPESETTER_DEFAULT_MARGIN,
    .pageWidth = 376,
    .pageHeight = 262,
};

// 从 UTF-8 解码 Unicode 码点
static uint32_t utf8_decode(const char *s, int *bytes) {
    unsigned char c = (unsigned char)s[0];
    if (c < 0x80) { *bytes = 1; return c; }
    if (c < 0xE0) { *bytes = 2; return ((c & 0x1F) << 6) | (s[1] & 0x3F); }
    if (c < 0xF0) { *bytes = 3; return ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); }
    *bytes = 4;
    return ((c & 0x07) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
}

// 加载字体
static int load_font(int fontId) {
    if (fontId == s_current_font_id && s_face) return 0;
    if (fontId < 0 || fontId >= MAX_FONTS) return -1;
    if (s_font_paths[fontId][0] == '\0') return -1;

    if (s_face) {
        FT_Done_Face(s_face);
        s_face = NULL;
    }

    FT_Error err = FT_New_Face(s_ft, s_font_paths[fontId], 0, &s_face);
    if (err) {
        printf("[ERROR][typesetter] Failed to load font %d: %d\n", fontId, err);
        return -1;
    }

    FT_Set_Pixel_Sizes(s_face, 0, s_cfg.fontSize);
    s_current_font_id = fontId;
    return 0;
}

// 获取字符宽度
static int char_width(uint32_t codepoint) {
    if (!s_face) return s_cfg.fontSize / 2;
    if (FT_Load_Char(s_face, codepoint, FT_LOAD_DEFAULT) == 0) {
        return s_face->glyph->advance.x >> 6;
    }
    return s_cfg.fontSize / 2;
}

int typesetter_register_font(int fontId, const char *ttfPath) {
    if (fontId < 0 || fontId >= MAX_FONTS || !ttfPath) return -1;
    strncpy(s_font_paths[fontId], ttfPath, MAX_FONT_PATH - 1);
    s_font_paths[fontId][MAX_FONT_PATH - 1] = '\0';
    return 0;
}

int typesetter_init(const TypesetterConfig *cfg) {
    if (cfg) {
        s_cfg = *cfg;
    }

    if (!s_ft) {
        FT_Error err = FT_Init_FreeType(&s_ft);
        if (err) {
            printf("[ERROR][typesetter] FT_Init_FreeType failed: %d\n", err);
            return -1;
        }
    }

    // 加载默认字体
    if (s_cfg.fontId < MAX_FONTS && s_font_paths[s_cfg.fontId][0] != '\0') {
        load_font(s_cfg.fontId);
    }

    return 0;
}

int typesetter_paginate(const char *text, uint32_t textLen,
                        PageIndex **pagesOut, uint32_t *pageCountOut) {
    if (!text || !pagesOut || !pageCountOut) return -1;

    load_font(s_cfg.fontId);

    int avail_w = s_cfg.pageWidth;
    int line_h = s_cfg.fontSize * s_cfg.lineSpacing / 10;
    uint32_t lines_per_page = (uint32_t)(s_cfg.pageHeight / line_h);
    if (lines_per_page < 1) lines_per_page = 1;

    uint32_t capacity = INITIAL_PAGES;
    PageIndex *pages = (PageIndex *)malloc(capacity * sizeof(PageIndex));
    if (!pages) return -1;

    uint32_t page_count = 0;
    uint32_t line_in_page = 0;
    uint32_t line_num = 0;
    uint32_t i = 0;

    // 第一页起始
    pages[page_count].startByte = 0;
    pages[page_count].startLine = 0;
    pages[page_count].pageNum = page_count + 1;
    page_count++;

    while (i < textLen) {
        if (text[i] == '\n') {
            i++;
            line_in_page++;
            line_num++;
        } else {
            // 累积一行
            int w = 0;
            uint32_t line_start = i;
            while (i < textLen && text[i] != '\n') {
                int bytes = 1;
                uint32_t cp = utf8_decode(text + i, &bytes);
                int cw = char_width(cp) + s_cfg.charSpacing;
                if (w + cw > avail_w && i > line_start) break;
                w += cw;
                i += bytes;
            }
            line_in_page++;
            line_num++;
        }

        // 分页判定
        if (line_in_page >= lines_per_page && i < textLen) {
            if (page_count >= capacity) {
                capacity *= 2;
                PageIndex *new_pages = (PageIndex *)realloc(pages, capacity * sizeof(PageIndex));
                if (!new_pages) { free(pages); return -1; }
                pages = new_pages;
            }
            pages[page_count].startByte = i;
            pages[page_count].startLine = line_num;
            pages[page_count].pageNum = page_count + 1;
            page_count++;
            line_in_page = 0;
        }
    }

    *pagesOut = pages;
    *pageCountOut = page_count;
    return 0;
}

void typesetter_render_page(const char *text, const PageIndex *page,
                            uint8_t *epdBuf, int bufW, int bufH, int yOffset) {
    (void)epdBuf; // 通过 renderer_set_pixel 直接绘制，不使用 epdBuf
    if (!text || !page) return;

    load_font(s_cfg.fontId);

    int x = s_cfg.margin;
    int y = yOffset + s_cfg.margin;
    int line_h = s_cfg.fontSize * s_cfg.lineSpacing / 10;
    uint32_t i = page->startByte;

    // 使用 renderer 接口绘制
    extern void renderer_set_pixel(int x, int y, int color);

    while (text[i] != '\0') {
        if (text[i] == '\n') {
            x = s_cfg.margin;
            y += line_h;
            i++;
            if (y >= yOffset + s_cfg.pageHeight) break;
            continue;
        }

        int bytes = 1;
        uint32_t cp = utf8_decode(text + i, &bytes);

        if (s_face && FT_Load_Char(s_face, cp, FT_LOAD_RENDER) == 0) {
            FT_GlyphSlot glyph = s_face->glyph;
            int gx = x + glyph->bitmap_left;
            int gy = y + s_cfg.fontSize - glyph->bitmap_top;

            for (unsigned int row = 0; row < glyph->bitmap.rows; row++) {
                for (unsigned int col = 0; col < glyph->bitmap.width; col++) {
                    uint8_t val = glyph->bitmap.buffer[row * glyph->bitmap.pitch + col];
                    if (val > 128) {
                        int px = gx + col;
                        int py = gy + row;
                        if (px >= 0 && px < bufW && py >= 0 && py < bufH) {
                            renderer_set_pixel(px, py, 0); // 黑色
                        }
                    }
                }
            }
            x += glyph->advance.x >> 6;
        } else {
            // 回退：简单矩形
            extern void renderer_fill_rect(int x, int y, int w, int h, int color);
            renderer_fill_rect(x, y, s_cfg.fontSize / 2, s_cfg.fontSize, 0);
            x += s_cfg.fontSize / 2;
        }

        i += bytes;
        if (x >= bufW - s_cfg.margin) {
            x = s_cfg.margin;
            y += line_h;
            if (y >= yOffset + s_cfg.pageHeight) break;
        }
    }
}

void typesetter_free_pages(PageIndex *pages) {
    if (pages) free(pages);
}

void typesetter_reload_config(const TypesetterConfig *cfg) {
    if (cfg) {
        s_cfg = *cfg;
        s_current_font_id = -1; // 强制重新加载字体
    }
}

#endif // PC_SIMULATION
