/**
 * @file mock_wifi.c
 * @brief Mock WiFi 模块 - 模拟 WiFi 连接状态
 */
#ifdef PC_SIMULATION

#include "mock_wifi.h"
#include <stdio.h>
#include <string.h>

static bool s_connected = false;
static char s_ssid[64] = {0};

void mock_wifi_init(void)
{
    printf("[Mock WiFi] 初始化，默认已连接\n");
    /* 默认模拟已连接状态 */
    s_connected = true;
    strncpy(s_ssid, "Home-WiFi", sizeof(s_ssid) - 1);
}

bool mock_wifi_is_connected(void)
{
    return s_connected;
}

void mock_wifi_connect(const char *ssid)
{
    if (ssid) {
        strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
        s_ssid[sizeof(s_ssid) - 1] = '\0';
    }
    s_connected = true;
    printf("[Mock WiFi] 已连接: %s\n", s_ssid);
}

void mock_wifi_disconnect(void)
{
    s_connected = false;
    printf("[Mock WiFi] 已断开\n");
}

const char *mock_wifi_get_ssid(void)
{
    return s_connected ? s_ssid : "";
}

#endif /* PC_SIMULATION */
