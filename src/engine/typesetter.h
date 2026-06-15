#pragma once
/**
 * @file typesetter.h
 * @brief FreeType 排版引擎 + 预分页算法
 */
#include <stdint.h>
#include <stdbool.h>
#include <esp_err.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 预分页索引：每页记录起始字节偏移和行号 */
typedef struct {
    uint32_t startByte;   /* 页起始字节偏移 */
    uint16_t startLine;   /* 页起始行号 */
    uint16_t pageNum;     /* 页码（1-based） */
} PageIndex;

/* 排版配置 */
typedef struct {
    uint8_t  fontId;       /* 字体索引（0=思源宋体 1=思源黑体 2=文楷） */
    uint8_t  fontSize;     /* 字号 px */
    uint8_t  lineSpacing;  /* 行距倍数 ×10（15=1.5 倍） */
    uint8_t  charSpacing;  /* 字间距 px */
    uint8_t  margin;       /* 页边距 px */
    int      pageWidth;    /* 可用宽度 px */
    int      pageHeight;   /* 可用高度 px */
} TypesetterConfig;

/* 注册可用字体（id → 路径） */
esp_err_t typesetter_register_font(uint8_t fontId, const char *ttf_path);

/* 初始化排版器（懒加载字体到 PSRAM，可选） */
esp_err_t typesetter_init(const TypesetterConfig *cfg);

/* 预分页：扫描文本生成 PageIndex 数组（调用者负责 free） */
esp_err_t typesetter_paginate(const char *text, uint32_t textLen,
                              PageIndex **pages_out, uint32_t *page_count_out);

/* 渲染单页到 EPD 帧缓冲（buf_w × buf_h 字节，y_offset 为起始行） */
esp_err_t typesetter_render_page(const char *text, const PageIndex *page,
                                  uint8_t *epd_buf, int buf_w, int buf_h, int y_offset);

/* 释放分页结果 */
void typesetter_free_pages(PageIndex *pages);

/* 热切换排版配置（重新触发分页） */
void typesetter_reload_config(const TypesetterConfig *cfg);

#ifdef __cplusplus
}
#endif
