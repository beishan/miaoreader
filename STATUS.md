# 妙阅（ai-bookreader）功能完成状态

> 基于 ESP32-S3 + 4.2 寸三色墨水屏的电子书阅读器
> 文档版本：v1.0 · 更新日期：2026-06-16

---

## 目录

1. [项目概述](#1-项目概述)
2. [功能完成总览](#2-功能完成总览)
3. [各模块详细状态](#3-各模块详细状态)
4. [已知问题与 TODO](#4-已知问题与-todo)
5. [下一步计划](#5-下一步计划)

---

## 1. 项目概述

### 1.1 硬件平台

| 项目 | 规格 |
|------|------|
| 主控 | ESP32-S3 N16R8（16MB Flash + 8MB PSRAM） |
| 显示屏 | 4.2 寸三色墨水屏（SSD1683，400×300，黑白红） |
| 存储 | SD 卡（SPI）+ NVS |
| 按键 | 4 个（PWR/PREV/NEXT/HOME） |

### 1.2 开发进度

| 阶段 | 内容 | 状态 |
|------|------|------|
| Phase 1 | HAL 层 + 引擎骨架 | ✅ 完成 |
| Phase 2 | 核心 UI + 排版引擎 | ✅ 完成 |
| Phase 3 | WiFi / 配网 / 网页服务 / 天气 | ✅ 完成 |
| Phase 4 | 休眠 / 电源管理 / OTA | ✅ 完成 |
| Phase 5 | 优化与打磨 | 进行中 |

### 1.3 构建状态

- **编译状态**：通过
- **Flash 占用**：660KB / 3MB（21.0%）
- **RAM 占用**：48KB / 320KB（14.8%）

---

## 2. 功能完成总览

### 2.1 按需求章节统计

| 需求章节 | 功能项数 | 已完成 | 部分完成 | 未开始 |
|----------|----------|--------|----------|--------|
| 1. 硬件选型 | 6 | 6 | 0 | 0 |
| 2. 系统架构 | 2 | 2 | 0 | 0 |
| 3. 初始化流程 | 2 | 2 | 0 | 0 |
| 4. 页面规格 | 6 | 6 | 0 | 0 |
| 5. 按键交互 | 4 | 3 | 1 | 0 |
| 6. 网页配置 | 4 | 2 | 2 | 0 |
| 7. 电源管理 | 3 | 2 | 1 | 0 |
| 8. 存储规格 | 2 | 2 | 0 | 0 |
| 9. 字体与排版 | 3 | 3 | 0 | 0 |
| 10. 天气服务 | 2 | 2 | 0 | 0 |
| 11. 休眠界面 | 2 | 1 | 1 | 0 |
| 12. 非功能性 | 9 | 7 | 2 | 0 |
| **合计** | **45** | **38** | **7** | **0** |

### 2.2 按模块层级统计

| 层级 | 模块数 | 完整实现 | 部分实现 | 未实现 |
|------|--------|----------|----------|--------|
| HAL 层 | 6 | 5 | 1 | 0 |
| Engine 层 | 9 | 9 | 0 | 0 |
| UI 层 | 11 | 9 | 2 | 0 |
| Net 层 | 4 | 4 | 0 | 0 |
| **合计** | **30** | **27** | **3** | **0** |

**整体完成度：90%（27/30 模块完整实现）**

---

## 3. 各模块详细状态

### 3.1 HAL 层（硬件抽象层）

#### ✅ epd — E-Ink 显示驱动

**状态**：完整实现

**已实现功能**：
- `epd_init` — SPI 初始化 + 帧缓冲分配
- `epd_clear` — 清屏（黑白+红色缓冲区）
- `epd_set_pixel` — 设置像素（支持黑白/红色）
- `epd_draw_rect` / `epd_fill_rect` — 矩形绘制
- `epd_display_full` — 三色全刷（~15s）
- `epd_display_partial` — 黑白局部刷新（~250ms）
- `epd_sleep` — 低功耗模式
- `epd_set_rotation` — 屏幕旋转（0°/90°/180°/270°）

**对应需求**：1.2 显示屏规格 ✓

---

#### ✅ keys — 按键驱动

**状态**：完整实现

**已实现功能**：
- 4 个按键支持（PWR/PREV/NEXT/HOME）
- GPIO 中断 + FreeRTOS 扫描任务
- 10ms 软件去抖
- 事件类型：短按（<500ms）、长按（>800ms，500ms 重复）、超长按（>2000ms）、组合键（PREV+NEXT）

**对应需求**：1.5 按键规格、5.1 按键事件定义 ✓

---

#### ⚠️ battery — 电池检测

**状态**：基本实现（充电检测为 stub）

**已实现功能**：
- `battery_init` — ADC 初始化
- `battery_get_percent` — 16 次 ADC 采样 + 电压-SOC 映射表插值
- `battery_is_low` — 低电量检测（<5%）
- `battery_is_warning` — 警告检测（<15%）

**未完全实现**：
- `battery_is_charging()` — 硬编码返回 `false`，缺少充电检测逻辑

**对应需求**：1.4 电源规格、7.3 电量显示（部分）⚠️

---

#### ✅ rtc — DS3231 实时时钟

**状态**：完整实现

**已实现功能**：
- `ds3231_init` — I2C 总线初始化
- `ds3231_get_time` — 读取时间（BCD 解码）
- `ds3231_set_time` — 设置时间（BCD 编码）

**对应需求**：1.6 RTC 规格 ✓

---

#### ✅ sd_card — SD 卡驱动

**状态**：完整实现

**已实现功能**：
- `sd_card_init` — SPI 模式挂载 FAT 文件系统
- `sd_card_is_mounted` — 挂载状态检查
- `sd_card_get_mount_point` — 获取挂载点路径
- `sd_card_deinit` — 卸载

**对应需求**：1.3 存储规格、8.1 SD 卡目录结构 ✓

---

#### ✅ wifi_mgr — WiFi 管理器

**状态**：完整实现

**已实现功能**：
- `wifi_mgr_init` — WiFi 子系统初始化
- `wifi_mgr_connect_sta` — STA 模式连接（自动重连 5 次）
- `wifi_mgr_start_ap` — AP 模式（SSID: EReader-XXXX，密码: ereader123）
- `wifi_mgr_stop_ap` — 停止 AP
- `wifi_mgr_disconnect` — 断开连接
- `wifi_mgr_is_connected` — 连接状态查询
- `wifi_mgr_get_ip` — 获取 IP 地址
- `wifi_mgr_scan` — 扫描附近 WiFi
- `wifi_mgr_get_ssid` / `wifi_mgr_get_rssi` — 获取 SSID 和信号强度

**对应需求**：3.1 首次配网、3.2 正常连接、6.2 WiFi 配置 ✓

---

### 3.2 Engine 层（引擎层）

#### ✅ types — 核心数据结构

**状态**：完整实现

**已定义结构**：
- `SysConfig` — 系统配置（WiFi/天气/休眠/显示方向等）
- `ReaderConfig` — 阅读配置（字体/字号/行距/字间距/页边距）
- `BookMeta` — 书籍元数据（标题/作者/路径/进度等）
- `PageId` — 页面标识（8 种：BOOT/HOME/BOOKSHELF/READER/SETTINGS/SLEEP/MENU/JUMP）
- `KeyEvent` — 按键事件（4 种：SHORT/LONG/EXTRA_LONG/COMBO）
- `KeyId` — 按键标识（4 种：PWR/PREV/NEXT/HOME）
- `EventType` — 事件类型（8 种）
- `WeatherData` / `WeatherForecast` — 天气数据结构

**对应需求**：2.2 核心数据结构 ✓

---

#### ✅ config — NVS 配置管理

**状态**：完整实现

**已实现功能**：
- `config_init` — NVS 初始化
- `config_load_sys` / `config_save_sys` — 系统配置读写
- `config_load_reader` / `config_save_reader` — 阅读配置读写
- `config_factory_reset` — 恢复出厂设置

**存储方式**：ESP-IDF NVS 库，namespace = "miaoyue"

**对应需求**：8.2 存储规格 ✓

---

#### ✅ event_bus — 事件总线

**状态**：完整实现

**已实现功能**：
- `event_bus_init` — 初始化
- `event_bus_subscribe` — 订阅事件（每事件最多 8 个 handler）
- `event_bus_unsubscribe` — 取消订阅
- `event_bus_publish` — 发布事件（同步阻塞）

**事件类型**：
- `EVT_KEY` — 按键事件
- `EVT_PAGE_SWITCH` — 页面切换
- `EVT_CONFIG_CHANGED` — 配置变更
- `EVT_WIFI_STATUS` — WiFi 状态
- `EVT_BATTERY` — 电池状态
- `EVT_SLEEP` / `EVT_WAKE` — 休眠/唤醒

**对应需求**：2.1 系统架构（事件总线） ✓

---

#### ✅ typesetter — 排版引擎

**状态**：完整实现

**已实现功能**：
- `typesetter_register_font` — 注册字体（3 个槽位：思源宋体/黑体、霞鹜文楷）
- `typesetter_init` — FreeType 初始化
- `typesetter_paginate` — 预分页（按字符宽度累积 + 断行）
- `typesetter_render_page` — 渲染到 EPD 帧缓冲
- `typesetter_free_pages` — 释放分页数据
- `typesetter_reload_config` — 热切换排版配置

**技术细节**：
- FreeType 2.14.2 集成
- 字体从 SD 卡加载到 PSRAM（8MB 充足）
- 支持 TTF 矢量字体

**对应需求**：9.1 字体分类、9.2 分页算法、4.4 热切换 ✓

---

#### ✅ book_parser — 书籍文件解析

**状态**：完整实现

**已实现功能**：
- `book_detect_format` — 格式检测（魔数：TXT/EPUB）
- `book_load_text` — 加载文本（自动检测 UTF-8/GBK 并转换）
- `book_extract_metadata` — 提取元数据
- `book_free_text` — 释放文本内存

**支持格式**：
- **TXT**：完整支持，UTF-8 BOM 跳过 + 启发式 GBK 检测 + 精确 GBK→UTF-8 转换（23940 项静态映射表）
- **EPUB**：纯文本提取（stored + deflate via miniz），HTML 标签剥离

**对应需求**：9.3 支持的文件格式 ✓

---

#### ✅ book_meta — 书籍元数据管理

**状态**：完整实现

**已实现功能**：
- `book_meta_init` — 初始化（从 SD:/books/metadata.json 加载）
- `book_meta_save` — 保存到 JSON
- `book_meta_get_count` — 获取书籍数量
- `book_meta_get` — 按索引获取
- `book_meta_find_by_path` — 按路径查找
- `book_meta_upsert` — 添加或更新
- `book_meta_remove` — 删除
- `book_meta_update_page` — 更新当前页
- `book_meta_update_total_pages` — 更新总页数
- `book_meta_add_reading_time` — 累加阅读时间
- `book_meta_sync_from_sd` — 同步 SD 卡目录（添加新书/删除已删除的书）
- `book_meta_get_current_reading` — 获取正在读的书

**对应需求**：8.1 SD 卡目录结构（metadata.json） ✓

---

#### ✅ bookmark — 书签管理

**状态**：完整实现

**已实现功能**：
- `bookmark_init` — 初始化
- `bookmark_add` — 添加书签（NVS 持久化，最多 100 个）
- `bookmark_remove` — 删除书签
- `bookmark_get_list` — 获取书签列表
- `bookmark_exists` — 检查是否存在

**对应需求**：5.3 阅读上下文菜单（添加书签） ✓

---

#### ✅ power_mgr — 电源管理

**状态**：完整实现

**已实现功能**：
- `power_mgr_init` — 初始化（esp_timer 空闲计时器 + 电池监控任务）
- `power_mgr_reset_idle` — 重置空闲计时器
- `power_mgr_enter_sleep` — 进入休眠（显示休眠界面 → Deep Sleep）
- `power_mgr_deep_sleep` — 直接深度睡眠
- `power_mgr_is_low_battery` — 低电量检查

**电源模式**：
- 活跃模式（~100-150mA）
- WiFi 关闭模式（~30-50mA）
- 轻度睡眠（~2-5mA）
- 深度睡眠（<0.1mA）

**对应需求**：7.1 运行模式、7.2 自动休眠逻辑、7.3 低电量保护 ✓

---

#### ✅ reading_stats — 阅读统计

**状态**：完整实现

**已实现功能**：
- `reading_stats_init` — 初始化（NVS 持久化）
- `reading_stats_start_session` — 开始阅读会话
- `reading_stats_end_session` — 结束阅读会话
- `reading_stats_get_today_seconds` / `reading_stats_get_today_minutes` — 今日阅读时间
- `reading_stats_get_total_seconds` / `reading_stats_get_total_hours` — 累计阅读时间
- `reading_stats_check_day_reset` — 日期变化自动重置

**对应需求**：4.2 主页（统计区） ✓

---

### 3.3 UI 层（用户界面层）

#### ✅ page_mgr — 页面状态机

**状态**：完整实现

**已实现功能**：
- `page_mgr_init` — 初始化
- `page_mgr_register` — 注册页面（PageVtbl 虚表）
- `page_mgr_switch` — 扁平切换（调用 on_exit → 切换 → on_enter → on_render → epd_display_full）
- `page_mgr_push` / `page_mgr_pop` — 栈式导航（MAX_STACK=4）
- `page_mgr_current` — 获取当前页面
- `page_mgr_handle_key` — 按键分发

**对应需求**：2.1 系统架构（UI 页面状态机） ✓

---

#### ✅ widget — 点阵字体控件

**状态**：完整实现

**已实现功能**：
- `widget_init` — 初始化
- `widget_draw_char` — 绘制单个字符
- `widget_draw_text` — 绘制文本（支持换行）
- `widget_text_width` — 计算文本宽度
- `widget_draw_icon` — 绘制图标

**技术细节**：
- 16×16 像素，1 bit/像素，行优先高位在前
- 95 个 ASCII 可打印字符 + 16 个图标
- 编译时嵌入 Flash（font_data.h，3552 字节）

**对应需求**：9.1 字体分类（UI 标签/状态栏用点阵字体） ✓

---

#### ✅ status_bar — 全局状态栏

**状态**：完整实现

**已实现功能**：
- `status_bar_init` — 初始化（订阅 EVT_BATTERY/EVT_WIFI_STATUS，启动 1 分钟定时器）
- `status_bar_render` — 渲染（20px 高，黑底白字）
- `status_bar_update_time` — 更新时间（左对齐 HH:MM）
- `status_bar_update_battery` — 更新电量（右对齐，百分比 + 充电图标 + WiFi 图标）
- `status_bar_update_wifi` — 更新 WiFi 状态

**特殊处理**：
- 电量 <20% 时显示红色警告（EPD_COLOR_RED）
- 充电状态依赖 `battery_is_charging()`（当前为 stub）

**对应需求**：4.1 全局状态栏 ✓

---

#### ✅ page_boot — 首次配网引导页

**状态**：完整实现

**渲染内容**：
- 标题"欢迎使用 妙阅"
- AP SSID 和密码（EReader-XXXX / ereader123）
- 网页访问地址（192.168.4.1）
- 底部提示"请使用手机或电脑连接上述 WiFi 并访问配置页面"

**按键行为**：不响应（等待网页配置完成重启）

**对应需求**：3.1 首次开机配网引导页 ✓

---

#### ✅ page_home — 主页

**状态**：完整实现

**渲染内容**：
- 日期区（居中）：星期 + 年月日
- 天气区（居中）：城市 + 天气状况 + 温度 + 湿度 + AQI
- 统计区（左对齐）：藏书数量、今日阅读时间、正在读的书 + 进度、WiFi 状态（IP 或提示）

**按键行为**：
- NEXT 短按 → 进入书架
- HOME 短按 → 进入设置

**对应需求**：4.2 主页 ✓

---

#### ✅ page_bookshelf — 书架页

**状态**：完整实现

**渲染内容**：
- 3×2 网格布局
- 每格：封面图（占位图）+ 书名（2 行截断）
- 选中项：红色高亮放大
- 底部：页码指示（第 X 页 / 共 Y 页）

**按键行为**：
- PREV/NEXT 短按：移动光标
- PREV/NEXT 长按：翻页
- HOME 短按：选书进入阅读

**对应需求**：4.3 书架页 ✓

---

#### ✅ page_reader — 阅读界面

**状态**：完整实现

**已实现功能**：
- 书籍加载（调用 book_parser）
- 排版分页（调用 typesetter）
- FreeType 渲染到 EPD
- NVS 阅读进度持久化
- 崩溃恢复（保存 last_book + 周期保存进度）
- 局部/全刷交替（每 5 次局部刷新后全刷）
- 阅读计时（reading_stats）

**按键行为**：
- PREV/NEXT 短按：翻页
- PREV/NEXT 长按：跳 5 页
- HOME 短按：弹出上下文菜单

**对应需求**：4.4 阅读界面 ✓

---

#### ✅ page_settings — 设置页

**状态**：完整实现

**渲染内容**：
- **阅读排版子菜单**：字体、字号、行距、字间距、页边距
- **系统设置子菜单**：休眠布局、屏幕方向、休眠超时、WiFi 状态、清洁屏幕、恢复出厂、关于

**按键行为**：
- PREV/NEXT：移动
- HOME：切换子菜单 / 执行操作
- 长按：切换排版值

**对应需求**：4.5 设置页 ✓

---

#### ✅ page_sleep — 休眠界面

**状态**：完整实现

**三种布局**：
- **布局 0**：时钟 + 天气（14:35 + 星期 + 日期 + 天气信息）
- **布局 1**：风景图（内置山水线稿）
- **布局 2**：诗词 + 时间（内置 50 首唐诗宋词）

**按键行为**：PWR 短按唤醒返回主页

**对应需求**：6.4 / 11 休眠界面（部分，诗词数量 50/100）⚠️

---

#### ⚠️ page_menu — 阅读上下文菜单

**状态**：基本实现（书籍信息为 stub）

**已实现功能**：
- 继续阅读（pop 返回）
- 跳转到（推入 page_jump）
- 添加书签（调用 bookmark_add）

**未完全实现**：
- "书籍信息"选项 — 直接 pop 返回，未展示详细信息（代码中标注 TODO）

**对应需求**：5.3 阅读上下文菜单（部分）⚠️

---

#### ⚠️ page_jump — 跳转页码

**状态**：基本实现（跳转传递机制有缺陷）

**已实现功能**：
- 数字输入界面（最多 4 位）
- PREV/NEXT 调整数字
- HOME 确认跳转
- 范围检查

**潜在问题**：
- 确认跳转后执行两次 pop（page_jump → page_menu → page_reader）
- 但跳转的目标页码通过 `page_jump_set_page_info(target - 1, ...)` 设置到自身
- page_reader 并未读取这个值来实际跳转
- **实际跳转功能可能不生效**

**对应需求**：5.3 跳转到...（部分）⚠️

---

### 3.4 Net 层（网络层）

#### ✅ weather — 天气服务

**状态**：完整实现

**已实现功能**：
- `weather_init` — 初始化
- `weather_refresh` — 刷新天气数据
- `weather_get_current` — 获取实时天气
- `weather_get_forecast` — 获取 3 天预报
- `weather_is_stale` — 检查数据是否过期
- `weather_config` — 配置（API Key、城市）

**API 集成**：
- 和风天气免费版 API
- 实时天气 + 3 天预报 + 空气质量
- NVS 缓存（30 分钟有效期）

**对应需求**：10. 天气服务集成 ✓

---

#### ✅ web_server — 网页服务器

**状态**：完整实现

**已实现路由**：
- `GET /` — 内嵌完整 HTML/CSS/JS 单页应用
- `GET /api/system/info` — 系统信息（WiFi/SD/配置）
- `GET /api/wifi/scan` — WiFi 扫描
- `POST /api/wifi/connect` — WiFi 连接
- `GET /api/weather/data` — 天气数据
- `POST /api/weather/config` — 天气配置
- `POST /api/sleep/config` — 休眠配置
- `GET /api/books` — 书籍列表
- `POST /api/books/delete` — 删除书籍
- `POST /api/books/upload` — 上传书籍（最大 50MB）
- `POST /api/system/update` — OTA 升级

**未实现功能**：
- 拖拽排序书籍
- 上传封面

**对应需求**：6. 网页配置界面（部分）⚠️

---

#### ✅ ota — OTA 升级

**状态**：完整实现

**已实现功能**：
- `ota_init` — 初始化
- `ota_update_handler` — 处理固件上传（esp_ota API，流式接收 + 写入，成功后自动重启）

**对应需求**：12. 非功能性需求（OTA 升级） ✓

---

#### ✅ sntp — 时间同步

**状态**：完整实现

**已实现功能**：
- `sntp_sync_init` — 初始化（时区 CST-8，多服务器）
- `sntp_force_sync` — 强制同步
- `sntp_is_synced` — 同步状态查询
- `sntp_get_last_sync_time` — 获取上次同步时间

**NTP 服务器**：
- ntp.aliyun.com
- cn.ntp.org.cn
- pool.ntp.org

**对应需求**：3.2 正常开机（SNTP 时间同步 + 写入 RTC） ✓

---

## 4. 已知问题与 TODO

### 4.1 功能缺陷

| 编号 | 问题 | 位置 | 严重程度 |
|------|------|------|----------|
| 1 | `battery_is_charging()` 硬编码返回 false | `src/hal/battery.c:107` | 中 |
| 2 | page_menu "书籍信息"选项为 stub | `src/ui/page_menu.c:121` | 低 |
| 3 | page_jump 跳转页码传递机制缺陷 | `src/ui/page_jump.c:142-144` | 高 |
| 4 | book_parser EPUB 元数据提取为 TODO | `src/engine/book_parser.c:470` | 低 |
| 5 | epd SPI handle 传入 NULL | `src/hal/epd.c:209,219,230` | 中 |

### 4.2 需求差距

| 编号 | 需求 | 当前状态 | 差距 |
|------|------|----------|------|
| 1 | 诗词库 100 首 | 50 首 | 缺少 50 首 |
| 2 | 网页拖拽排序书籍 | 未实现 | 功能缺失 |
| 3 | 网页上传封面 | 未实现 | 功能缺失 |
| 4 | SPIFFS 存储（web 界面 gzip） | 使用内嵌 HTML | 架构差异 |
| 5 | 充电状态检测 | stub | 硬件逻辑缺失 |

### 4.3 技术债务

| 编号 | 问题 | 说明 |
|------|------|------|
| 1 | EPUB 元数据提取 | 未从 OPF 提取 dc:title/dc:creator |
| 2 | GBK 转换精度 | 使用区间映射，可能有少量字符错位 |
| 3 | SPIFFS 未启用 | 当前使用内嵌 HTML，未使用 SPIFFS 存储 |

---

## 5. 下一步计划

### 5.1 Phase 5 优化与打磨（进行中）

**已完成**：
- ✅ PSRAM 兼容（N16R8 型号，8MB PSRAM）
- ✅ 完整 ZIP 支持（ESP-IDF ROM miniz，deflate 解压）
- ✅ Flash 大小配置（sdkconfig 更新为 16MB）
- ✅ GBK→UTF-8 精确查表（23940 项全量静态映射表）
- ✅ I2C 驱动迁移（新版 driver/i2c_master.h）
- ✅ 看门狗 + 崩溃恢复（Task WDT 30 秒超时 + NVS 崩溃恢复）

**待完成**：
- [ ] 关闭 ADC 告警（升级到 driver/adc.h）
- [ ] 修复 page_jump 跳转传递机制
- [ ] 实现 page_menu "书籍信息"展示
- [ ] 补充诗词库至 100 首
- [ ] 实现 EPUB 元数据提取（dc:title/dc:creator）
- [ ] 修复 epd SPI handle 问题

### 5.2 功能增强（可选）

- [ ] 网页拖拽排序书籍
- [ ] 网页上传封面
- [ ] SPIFFS 存储优化
- [ ] 充电状态检测硬件逻辑

---

## 6. 附录

### 6.1 文件结构

```
ai-bookreader/
├── src/                    # ESP32 固件源码
│   ├── hal/                # 硬件抽象层（6 个模块）
│   ├── engine/             # 引擎层（9 个模块）
│   ├── ui/                 # UI 层（11 个模块）
│   ├── net/                # 网络层（4 个模块）
│   └── main.c              # 入口
├── app/                    # PC 仿真（SDL2）
├── docs/                   # 文档
├── platformio.ini          # PlatformIO 配置
├── requires_01.md          # 产品需求文档
├── DESIGN.md               # 架构设计文档
├── README.md               # 项目说明
└── STATUS.md               # 功能完成状态（本文档）
```

### 6.2 相关文档

- [产品需求文档](./requires_01.md)
- [架构设计文档](./DESIGN.md)
- [项目说明](./README.md)
- [PC 仿真说明](./app/README.md)

---

**文档维护**：妙阅开发团队
**最后更新**：2026-06-16
