/**
 * @file renderer_sdl.c
 * @brief PC 仿真渲染器：使用 SDL2 + LVGL 9.x
 *
 * 在 PC 上模拟 400x300 三色电子墨水屏
 */
#ifdef PC_SIMULATION

#include "renderer_if.h"
#include <SDL2/SDL.h>
#include <stdio.h>
#include <string.h>

/* 窗口和渲染器 */
static SDL_Window *s_window = NULL;
static SDL_Renderer *s_renderer = NULL;
static SDL_Texture *s_texture = NULL;

/* 帧缓冲：RGB 格式，每个像素 3 字节 */
static uint8_t s_framebuffer[RENDERER_HEIGHT][RENDERER_WIDTH][3];

/* 按键回调 */
static renderer_key_callback_t s_key_callback = NULL;

/* 颜色映射表（RendererColor -> RGB） */
static const uint8_t color_map[][3] = {
    {0, 0, 0},       // BLACK
    {255, 255, 255},  // WHITE
    {255, 0, 0},      // RED
};

/* 按键映射：SDL keycode -> RendererKeyId */
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
            dst[x * 4 + 0] = s_framebuffer[y][x][2];  // B
            dst[x * 4 + 1] = s_framebuffer[y][x][1];  // G
            dst[x * 4 + 2] = s_framebuffer[y][x][0];  // R
            dst[x * 4 + 3] = 255;                      // A
        }
    }

    SDL_UnlockTexture(s_texture);
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
        RENDERER_WIDTH * 2,  /* 放大 2 倍显示 */
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

    /* 初始化帧缓冲为白色 */
    renderer_clear(RENDERER_COLOR_WHITE);

    printf("渲染器初始化完成: %dx%d (放大 2 倍)\n", RENDERER_WIDTH, RENDERER_HEIGHT);
    printf("按键映射:\n");
    printf("  ESC   -> PWR (电源)\n");
    printf("  LEFT  -> PREV (上一页)\n");
    printf("  RIGHT -> NEXT (下一页)\n");
    printf("  ENTER -> HOME (确认)\n");

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
    renderer_fill_rect(x, y, w, 1, color);         // 上边
    renderer_fill_rect(x, y + h - 1, w, 1, color); // 下边
    renderer_fill_rect(x, y, 1, h, color);         // 左边
    renderer_fill_rect(x + w - 1, y, 1, h, color); // 右边
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

    printf("[渲染] 全刷完成\n");
}

void renderer_display_partial(void)
{
    update_texture();

    SDL_RenderClear(s_renderer);
    SDL_RenderCopy(s_renderer, s_texture, NULL, NULL);
    SDL_RenderPresent(s_renderer);

    printf("[渲染] 局部刷新完成\n");
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
                /* 简化：直接触发短按事件 */
                s_key_callback(key, RENDERER_KEY_EVT_SHORT);
            }
            break;
        }

        case SDL_KEYUP:
            /* 可以在这里处理按键释放事件 */
            break;
        }
    }
    return 0;
}

void renderer_delay(uint32_t ms)
{
    SDL_Delay(ms);
}

#endif /* PC_SIMULATION */
