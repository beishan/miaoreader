#ifndef TYPESETTER_H
#define TYPESETTER_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 预分页索引条目
typedef struct {
    uint32_t startByte;   // 页起始字节偏移
    uint16_t startLine;   // 页起始行号
    uint16_t pageNum;     // 页码（1-based）
} PageIndex;

// 排版配置
typedef struct {
    uint8_t  fontId;       // 字体索引（0=思源宋体 1=思源黑体 2=文楷）
    uint8_t  fontSize;     // 字号 px
    uint8_t  lineSpacing;  // 行距倍数 x10（15=1.5倍）
    uint8_t  charSpacing;  // 字间距 px
    uint8_t  margin;       // 页边距 px
    int      pageWidth;    // 可用宽度 px
    int      pageHeight;   // 可用高度 px
} TypesetterConfig;

// 默认配置
#define TYPESETTER_DEFAULT_FONT_SIZE     20
#define TYPESETTER_DEFAULT_LINE_SPACING  15  // 1.5倍
#define TYPESETTER_DEFAULT_CHAR_SPACING  1
#define TYPESETTER_DEFAULT_MARGIN        12

// API
int  typesetter_register_font(int fontId, const char *ttfPath);
int  typesetter_init(const TypesetterConfig *cfg);
int  typesetter_paginate(const char *text, uint32_t textLen,
                         PageIndex **pagesOut, uint32_t *pageCountOut);
void typesetter_render_page(const char *text, const PageIndex *page,
                            uint8_t *epdBuf, int bufW, int bufH, int yOffset);
void typesetter_free_pages(PageIndex *pages);
void typesetter_reload_config(const TypesetterConfig *cfg);

#ifdef __cplusplus
}
#endif

#endif // TYPESETTER_H
