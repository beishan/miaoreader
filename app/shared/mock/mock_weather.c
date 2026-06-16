/**
 * @file mock_weather.c
 * @brief Mock 天气模块 - 模拟和风天气 API
 */
#ifdef PC_SIMULATION

#include "mock_weather.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* Mock 天气数据 */
static MockWeatherData s_weather = {
    .city = "北京",
    .condition = "晴转多云",
    .temp_now = 26,
    .temp_low = 18,
    .temp_high = 28,
    .humidity = 55,
    .aqi = "良",
    .alert = "",
};

/* 预设的天气变化场景 */
static const MockWeatherData weather_scenes[] = {
    {
        .city = "北京",
        .condition = "晴转多云",
        .temp_now = 26,
        .temp_low = 18,
        .temp_high = 28,
        .humidity = 55,
        .aqi = "良",
        .alert = "",
    },
    {
        .city = "上海",
        .condition = "小雨",
        .temp_now = 22,
        .temp_low = 19,
        .temp_high = 24,
        .humidity = 78,
        .aqi = "优",
        .alert = "今晚有小雨",
    },
    {
        .city = "深圳",
        .condition = "多云",
        .temp_now = 30,
        .temp_low = 26,
        .temp_high = 32,
        .humidity = 65,
        .aqi = "良",
        .alert = "",
    },
};

static int s_scene_index = 0;

void mock_weather_init(void)
{
    printf("[Mock Weather] 初始化\n");
    /* 根据时间切换场景 */
    time_t now = time(NULL);
    s_scene_index = (now / 3600) % 3;
    s_weather = weather_scenes[s_scene_index];
}

void mock_weather_get_current(MockWeatherData *data)
{
    if (data) {
        *data = s_weather;
    }
}

void mock_weather_get_summary(char *buf, int size)
{
    snprintf(buf, size, "%s  %s", s_weather.city, s_weather.condition);
}

void mock_weather_get_temp_str(char *buf, int size)
{
    snprintf(buf, size, "%d°C  %d/%d°C",
             s_weather.temp_now, s_weather.temp_low, s_weather.temp_high);
}

void mock_weather_get_detail_str(char *buf, int size)
{
    snprintf(buf, size, "湿度:%d%%  AQI:%s",
             s_weather.humidity, s_weather.aqi);
}

#endif /* PC_SIMULATION */
