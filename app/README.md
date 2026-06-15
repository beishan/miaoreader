# 妙阅 E-Reader PC 仿真

## 概述

本目录包含电子书阅读器的 PC 仿真版本，使用 LVGL + SDL2 在电脑上渲染 UI，方便开发和调试。

## 架构

```
app/
 ├── renderer_if.h        // 抽象渲染接口
 ├── renderer_sdl.c       // PC 仿真（SDL2）
 ├── renderer_eink.c      // ESP32 EPD 驱动封装
 ├── sim_main.c           // PC 仿真入口
 └── CMakeLists.txt       // 构建配置
```

## 依赖安装

### macOS

```bash
brew install sdl2 pkg-config cmake
```

### Ubuntu/Debian

```bash
sudo apt-get install libsdl2-dev pkg-config cmake
```

### Windows (MSYS2)

```bash
pacman -S mingw-w64-x86_64-SDL2 mingw-w64-x86_64-cmake
```

## 构建和运行

```bash
cd app
mkdir build && cd build
cmake ..
make
./reader_sim
```

## 按键映射

| PC 按键 | 设备按键 | 功能 |
|---------|----------|------|
| ESC     | PWR      | 电源/返回主页 |
| ←       | PREV     | 上一页/向上 |
| →       | NEXT     | 下一页/向下 |
| ENTER   | HOME     | 确认/菜单 |

## 页面导航

1. **主页** → 按 → 进入书架
2. **书架** → 按 ENTER 进入阅读器
3. **书架** → 按 ←/→ 选择书籍
4. **阅读器** → 按 ←/→ 翻页
5. **阅读器** → 按 ENTER 返回书架
6. **任意页面** → 按 ESC 返回主页

## 渲染接口

所有渲染操作通过 `renderer_if.h` 中的接口函数完成：

```c
renderer_init()           // 初始化渲染器
renderer_clear()          // 清屏
renderer_set_pixel()      // 设置像素
renderer_draw_rect()      // 画矩形边框
renderer_fill_rect()      // 填充矩形
renderer_display()        // 刷新显示
renderer_poll_events()    // 处理输入事件
```

## 集成到 ESP32 项目

将 `renderer_if.h` 中的接口函数映射到 ESP32 的 EPD 驱动：

```c
// 在 renderer_eink.c 中
#include "hal/epd.h"

void renderer_set_pixel(int x, int y, RendererColor color) {
    epd_set_pixel(x, y, (EpdColor)color);
}
```

## 后续扩展

- [ ] 集成 LVGL 9.x 图形库
- [ ] 支持中文字符渲染
- [ ] 支持触摸屏输入
- [ ] 支持截图功能
- [ ] 支持动画效果
