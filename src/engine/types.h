#pragma once
#include <stdint.h>
#include <stdbool.h>

// 系统配置（持久化到 NVS）
typedef struct {
    char     ssid[64];
    char     password[64];
    char     weatherApiKey[64];
    char     weatherCity[32];
    uint8_t  sleepLayout;       // 0=时钟天气 1=风景图 2=诗词
    uint8_t  displayRotation;   // 0/1/2/3 → 0°/90°/180°/270°
    uint32_t sleepTimeoutSec;   // 无操作自动休眠秒数
    bool     initialized;       // 首次配网标志
} SysConfig;

// 阅读器设置（持久化到 NVS）
typedef struct {
    uint8_t  fontSize;          // 16/20/24/28/32px
    uint8_t  fontId;            // 字体索引
    uint8_t  lineSpacing;       // 1.0/1.2/1.5/2.0 (×10存储)
    uint8_t  charSpacing;       // 0–8px
    uint8_t  margin;            // 页边距 px
} ReaderConfig;

// 书目元数据（存储于 SD:/books/metadata.json）
typedef struct {
    char     id[16];
    char     title[128];
    char     author[64];
    char     filePath[256];
    uint32_t totalPages;
    uint32_t currentPage;
    uint32_t readingSeconds;
    uint8_t  coverOffset;
} BookMeta;

// 页面 ID
typedef enum {
    PAGE_BOOT,
    PAGE_HOME,
    PAGE_BOOKSHELF,
    PAGE_READER,
    PAGE_SETTINGS,
    PAGE_SLEEP,
} PageId;

// 按键事件
typedef enum {
    KEY_EVT_SHORT,      // 短按 (<500ms)
    KEY_EVT_LONG,       // 长按 (>800ms, 500ms 重复)
    KEY_EVT_SUPER_LONG, // 超长按 (>2000ms, 仅 PWR)
    KEY_EVT_COMBO,      // 组合按
} KeyEvent;

typedef enum {
    KEY_PWR,
    KEY_PREV,
    KEY_NEXT,
    KEY_HOME,
    KEY_COUNT,
} KeyId;

// 事件总线事件类型
typedef enum {
    EVT_KEY,            // 按键事件
    EVT_PAGE_SWITCH,    // 页面切换
    EVT_CONFIG_CHANGED, // 配置变更
    EVT_WIFI_STATUS,    // WiFi 状态变化
    EVT_BATTERY,        // 电量变化
    EVT_SLEEP,          // 进入休眠
    EVT_WAKE,           // 唤醒
    EVT_WEATHER,        // 天气数据更新
} EventType;

// 天气数据结构
typedef struct {
    char city[32];          // 城市名
    char condition[32];     // 天气状况："晴"、"多云"、"小雨"
    int  temp_now;          // 当前温度 (°C)
    int  temp_high;         // 最高温
    int  temp_low;          // 最低温
    int  humidity;          // 湿度 (%)
    int  wind_speed;        // 风速 (km/h)
    char wind_dir[16];      // 风向
    char aqi[16];           // 空气质量等级
    int  aqi_value;         // AQI 数值
    uint32_t update_time;   // 更新时间戳
} WeatherData;

typedef struct {
    char date[16];          // 日期 MM-DD
    char condition[32];     // 天气状况
    int  temp_high;         // 最高温
    int  temp_low;          // 最低温
} WeatherDayForecast;

typedef struct {
    WeatherDayForecast days[3];  // 3 天预报
} WeatherForecast;
