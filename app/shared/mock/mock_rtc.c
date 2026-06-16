/**
 * @file mock_rtc.c
 * @brief Mock RTC 模块 - 模拟 DS3231 实时时钟
 */
#ifdef PC_SIMULATION

#include "mock_rtc.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

/* 中文星期 */
static const char *weekday_cn[] = {
    "周日", "周一", "周二", "周三", "周四", "周五", "周六"
};

void mock_rtc_init(void)
{
    printf("[Mock RTC] 初始化\n");
}

void mock_rtc_get_time(struct tm *tm)
{
    /* 获取系统真实时间 */
    time_t now = time(NULL);
    localtime_r(&now, tm);
}

void mock_rtc_get_time_str(char *buf, int size)
{
    struct tm tm;
    mock_rtc_get_time(&tm);
    snprintf(buf, size, "%02d:%02d", tm.tm_hour, tm.tm_min);
}

void mock_rtc_get_date_str(char *buf, int size)
{
    struct tm tm;
    mock_rtc_get_time(&tm);
    snprintf(buf, size, "%d年%02d月%02d日",
             tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday);
}

const char* mock_rtc_get_weekday_cn(void)
{
    struct tm tm;
    mock_rtc_get_time(&tm);
    return weekday_cn[tm.tm_wday];
}

#endif /* PC_SIMULATION */
