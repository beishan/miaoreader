/**
 * @file renderer_sdl.c
 * @brief PC 仿真渲染器：使用 SDL2 + FreeType
 *
 * 在 PC 上模拟 400x300 三色电子墨水屏
 * 支持 ASCII 和中文字符渲染
 */
#ifdef PC_SIMULATION

#include "renderer_if.h"
#include <SDL2/SDL.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include FT_GLYPH_H
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 窗口和渲染器 */
static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;

/* 帧缓冲 */
static uint8_t s_framebuffer[RENDERER_HEIGHT][RENDERER_WIDTH][3];

/* 按键回调 */
static renderer_key_callback_t s_key_callback = NULL;

/* FreeType */
static FT_Library s_ft_library = NULL;
static FT_Face s_ft_face = NULL;
static int s_font_size = 16;

/* 颜色映射表 */
static const uint8_t color_map[][3] = {
    {0, 0, 0},       // BLACK
    {255, 255, 255},  // WHITE
    {255, 0, 0},      // RED
};

/* 按键映射 */
static RendererKeyId map_sdl_key(SDL_Keycode key)
{
    switch (key) {
    case SDLK_ESCAPE:  return RENDERER_KEY_PWR;
    case SDLK_LEFT:    return RENDERER_KEY_PREV;
    case SDLK_RIGHT:   return RENDERER_KEY_NEXT;
    case SDLK_RETURN:  return RENDERER_KEY_HOME;
    default:           return RENDERER_KEY_COUNT;
    }
}

/* 更新纹理 */
static void update_texture(void)
{
    if (!s_texture) return;

    void *pixels;
    int pitch;
    SDL_LockTexture(s_texture, NULL, &pixels, &pitch);

    for (int y = 0; y < RENDERER_HEIGHT; y++) {
        uint8_t *dst = (uint8_t *)pixels + y * pitch;
        for (int x = 0; x < RENDERER_WIDTH; x++) {
            dst[x * 4 + 0] = s_framebuffer[y][x][2];
            dst[x * 4 + 1] = s_framebuffer[y][x][1];
            dst[x * 4 + 2] = s_framebuffer[y][x][0];
            dst[x * 4 + 3] = 255;
        }
    }

    SDL_UnlockTexture(s_texture);
}

/* 查找系统中文字体 */
static const char *find_chinese_font(void)
{
    /* macOS 常见中文字体路径 */
    static const char *font_paths[] = {
        "/System/Library/Fonts/PingFang.ttc",
        "/System/Library/Fonts/STHeiti Light.ttc",
        "/System/Library/Fonts/Hiragino Sans GB.ttc",
        "/Library/Fonts/Arial Unicode.ttf",
        "/System/Library/Fonts/Supplemental/Songti.ttc",
        "/usr/share/fonts/truetype/wqy/wqy-zenhei.ttc",  /* Linux */
        "/usr/share/fonts/opentype/noto/NotoSansCJK-Regular.ttc",  /* Linux */
        "C:\\Windows\\Fonts\\msyh.ttc",  /* Windows */
        "C:\\Windows\\Fonts\\simsun.ttc",  /* Windows */
        NULL
    };

    for (int i = 0; font_paths[i]; i++) {
        FILE *f = fopen(font_paths[i], "rb");
        if (f) {
            fclose(f);
            printf("找到字体: %s\n", font_paths[i]);
            return font_paths[i];
        }
    }

    printf("未找到中文字体，使用默认字体\n");
    return NULL;
}

int renderer_init(const char *title)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init 失败: %s\n", SDL_GetError());
        return -1;
    }

    s_window = SDL_CreateWindow(
        title ? title : "妙阅 E-Reader 仿真",
        SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED,
        RENDERER_WIDTH * 2,
        RENDERER_HEIGHT * 2,
        SDL_WINDOW_SHOWN
    );

    if (!s_window) {
        fprintf(stderr, "SDL_CreateWindow 失败: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }

    s_renderer = SDL_CreateRenderer(s_window, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    if (!s_renderer) {
        fprintf(stderr, "SDL_CreateRenderer 失败: %s\n", SDL_GetError());
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return -1;
    }

    s_texture = SDL_CreateTexture(s_renderer,
        SDL_PIXELFORMAT_ARGB8888,
        SDL_TEXTUREACCESS_STREAMING,
        RENDERER_WIDTH,
        RENDERER_HEIGHT);

    if (!s_texture) {
        fprintf(stderr, "SDL_CreateTexture 失败: %s\n", SDL_GetError());
        SDL_DestroyRenderer(s_renderer);
        SDL_DestroyWindow(s_window);
        SDL_Quit();
        return -1;
    }

    /* 初始化 FreeType */
    FT_Error error = FT_Init_FreeType(&s_ft_library);
    if (error) {
        fprintf(stderr, "FreeType 初始化失败: %d\n", error);
    } else {
        /* 查找并加载中文字体 */
        const char *font_path = find_chinese_font();
        if (font_path) {
            error = FT_New_Face(s_ft_library, font_path, 0, &s_ft_face);
            if (error) {
                fprintf(stderr, "字体加载失败: %d\n", error);
                s_ft_face = NULL;
            } else {
                /* 设置字体大小 */
                error = FT_Set_Pixel_Sizes(s_ft_face, 0, s_font_size);
                if (error) {
                    fprintf(stderr, "设置字体大小失败: %d\n", error);
                } else {
                    printf("FreeType 初始化成功，字体大小: %d\n", s_font_size);
                }
            }
        }
    }

    renderer_clear(RENDERER_COLOR_WHITE);

    printf("渲染器初始化完成: %dx%d\n", RENDERER_WIDTH, RENDERER_HEIGHT);
    return 0;
}

void renderer_clear(RendererColor color)
{
    const uint8_t *c = color_map[color];
    for (int y = 0; y < RENDERER_HEIGHT; y++) {
        for (int x = 0; x < RENDERER_WIDTH; x++) {
            s_framebuffer[y][x][0] = c[0];
            s_framebuffer[y][x][1] = c[1];
            s_framebuffer[y][x][2] = c[2];
        }
    }
}

void renderer_set_pixel(int x, int y, RendererColor color)
{
    if (x < 0 || x >= RENDERER_WIDTH || y < 0 || y >= RENDERER_HEIGHT) return;

    const uint8_t *c = color_map[color];
    s_framebuffer[y][x][0] = c[0];
    s_framebuffer[y][x][1] = c[1];
    s_framebuffer[y][x][2] = c[2];
}

void renderer_draw_rect(int x, int y, int w, int h, RendererColor color)
{
    renderer_fill_rect(x, y, w, 1, color);
    renderer_fill_rect(x, y + h - 1, w, 1, color);
    renderer_fill_rect(x, y, 1, h, color);
    renderer_fill_rect(x + w - 1, y, 1, h, color);
}

void renderer_fill_rect(int x, int y, int w, int h, RendererColor color)
{
    const uint8_t *c = color_map[color];

    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if (px >= 0 && px < RENDERER_WIDTH && py >= 0 && py < RENDERER_HEIGHT) {
                s_framebuffer[py][px][0] = c[0];
                s_framebuffer[py][px][1] = c[1];
                s_framebuffer[py][px][2] = c[2];
            }
        }
    }
}

void renderer_display(void)
{
    update_texture();
    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);
}

void renderer_display_partial(void)
{
    renderer_display();
}

void renderer_sleep(void)
{
    printf("[渲染] 进入休眠模式\n");
}

void renderer_set_key_callback(renderer_key_callback_t callback)
{
    s_key_callback = callback;
}

int renderer_poll_events(void)
{
    SDL_Event event;
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_QUIT:
            return 1;
        case SDL_KEYDOWN: {
            RendererKeyId key = map_sdl_key(event.key.keysym.sym);
            if (key < RENDERER_KEY_COUNT && s_key_callback) {
                s_key_callback(key, RENDERER_KEY_EVT_SHORT);
            }
            break;
        }
        }
    }
    return 0;
}

void renderer_delay(uint32_t ms)
{
    SDL_Delay(ms);
}

/* 使用 FreeType 渲染字符 */
void renderer_draw_char(int x, int y, char c, RendererColor color)
{
    if (!s_ft_face) {
        /* FreeType 不可用，使用简单矩形 */
        renderer_fill_rect(x + 2, y + 2, 8, 12, color);
        return;
    }

    /* 加载字形 */
    FT_UInt glyph_index = FT_Get_Char_Index(s_ft_face, c);
    if (glyph_index == 0) return;

    FT_Error error = FT_Load_Glyph(s_ft_face, glyph_index, FT_LOAD_DEFAULT);
    if (error) return;

    error = FT_Render_Glyph(s_ft_face->glyph, FT_RENDER_MODE_NORMAL);
    if (error) return;

    FT_GlyphSlot glyph = s_ft_face->glyph;
    FT_Bitmap *bitmap = &glyph->bitmap;

    /* 计算绘制位置 */
    int draw_x = x + glyph->bitmap_left;
    int draw_y = y - glyph->bitmap_top + s_font_size;

    /* 绘制位图 */
    const uint8_t *c_rgb = color_map[color];
    for (int row = 0; row < (int)bitmap->rows; row++) {
        for (int col = 0; col < (int)bitmap->width; col++) {
            int px = draw_x + col;
            int py = draw_y + row;

            if (px >= 0 && px < RENDERER_WIDTH && py >= 0 && py < RENDERER_HEIGHT) {
                uint8_t alpha = bitmap->buffer[row * bitmap->pitch + col];
                if (alpha > 128) {
                    s_framebuffer[py][px][0] = c_rgb[0];
                    s_framebuffer[py][px][1] = c_rgb[1];
                    s_framebuffer[py][px][2] = c_rgb[2];
                }
            }
        }
    }
}

/* 绘制文本字符串 */
void renderer_draw_text(int x, int y, const char *text, RendererColor color)
{
    if (!s_ft_face) {
        /* FreeType 不可用，使用简单绘制 */
        int cx = x;
        for (const char *p = text; *p; p++) {
            if (*p == '\n') {
                y += 20;
                cx = x;
                continue;
            }
            renderer_fill_rect(cx + 2, y + 2, 8, 12, color);
            cx += 12;
        }
        return;
    }

    int cx = x;
    const unsigned char *p = (const unsigned char *)text;

    while (*p) {
        if (*p == '\n') {
            y += s_font_size + 4;
            cx = x;
            p++;
            continue;
        }

        /* 解析 UTF-8 */
        uint32_t codepoint = 0;
        int bytes = 0;

        if (*p < 0x80) {
            codepoint = *p;
            bytes = 1;
        } else if ((*p & 0xE0) == 0xC0) {
            codepoint = (*p & 0x1F) << 6;
            if (p[1]) codepoint |= (p[1] & 0x3F);
            bytes = 2;
        } else if ((*p & 0xF0) == 0xE0) {
            codepoint = (*p & 0x0F) << 12;
            if (p[1]) codepoint |= (p[1] & 0x3F) << 6;
            if (p[2]) codepoint |= (p[2] & 0x3F);
            bytes = 3;
        } else if ((*p & 0xF8) == 0xF0) {
            codepoint = (*p & 0x07) << 18;
            if (p[1]) codepoint |= (p[1] & 0x3F) << 12;
            if (p[2]) codepoint |= (p[2] & 0x3F) << 6;
            if (p[3]) codepoint |= (p[3] & 0x3F);
            bytes = 4;
        } else {
            p++;
            continue;
        }

        /* 加载字形 */
        FT_UInt glyph_index = FT_Get_Char_Index(s_ft_face, codepoint);
        if (glyph_index > 0) {
            FT_Error error = FT_Load_Glyph(s_ft_face, glyph_index, FT_LOAD_DEFAULT);
            if (error == 0) {
                error = FT_Render_Glyph(s_ft_face->glyph, FT_RENDER_MODE_NORMAL);
                if (error == 0) {
                    FT_GlyphSlot glyph = s_ft_face->glyph;
                    FT_Bitmap *bitmap = &glyph->bitmap;

                    int draw_x = cx + glyph->bitmap_left;
                    int draw_y = y - glyph->bitmap_top + s_font_size;

                    const uint8_t *c_rgb = color_map[color];
                    for (int row = 0; row < (int)bitmap->rows; row++) {
                        for (int col = 0; col < (int)bitmap->width; col++) {
                            int px = draw_x + col;
                            int py = draw_y + row;

                            if (px >= 0 && px < RENDERER_WIDTH && py >= 0 && py < RENDERER_HEIGHT) {
                                uint8_t alpha = bitmap->buffer[row * bitmap->pitch + col];
                                if (alpha > 128) {
                                    s_framebuffer[py][px][0] = c_rgb[0];
                                    s_framebuffer[py][px][1] = c_rgb[1];
                                    s_framebuffer[py][px][2] = c_rgb[2];
                                }
                            }
                        }
                    }

                    cx += glyph->advance.x >> 6;
                }
            }
        }

        p += bytes;
    }
}

/* 测量文本宽度 */
int renderer_text_width(const char *text)
{
    if (!s_ft_face) {
        int len = 0;
        for (const char *p = text; *p; p++) {
            if (*p != '\n') len++;
        }
        return len * 12;
    }

    int width = 0;
    const unsigned char *p = (const unsigned char *)text;

    while (*p) {
        if (*p == '\n') break;

        uint32_t codepoint = 0;
        int bytes = 0;

        if (*p < 0x80) {
            codepoint = *p;
            bytes = 1;
        } else if ((*p & 0xE0) == 0xC0) {
            codepoint = (*p & 0x1F) << 6;
            if (p[1]) codepoint |= (p[1] & 0x3F);
            bytes = 2;
        } else if ((*p & 0xF0) == 0xE0) {
            codepoint = (*p & 0x0F) << 12;
            if (p[1]) codepoint |= (p[1] & 0x3F) << 6;
            if (p[2]) codepoint |= (p[2] & 0x3F);
            bytes = 3;
        } else {
            p++;
            continue;
        }

        FT_UInt glyph_index = FT_Get_Char_Index(s_ft_face, codepoint);
        if (glyph_index > 0) {
            FT_Error error = FT_Load_Glyph(s_ft_face, glyph_index, FT_LOAD_DEFAULT);
            if (error == 0) {
                width += s_ft_face->glyph->advance.x >> 6;
            }
        }

        p += bytes;
    }

    return width;
}

#endif /* PC_SIMULATION */
