/**
 * @file wifi_mgr.c
 * @brief WiFi 管理模块：STA/AP 模式切换、自动重连、事件发布
 */
#include "wifi_mgr.h"
#include "engine/event_bus.h"
#include "engine/types.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include <string.h>

static const char *TAG = "wifi_mgr";

/* 状态变量 */
static bool s_initialized = false;
static bool s_connected = false;
static bool s_ap_active = false;
static char s_ssid[64] = {0};
static char s_password[64] = {0};
static char s_ip_str[16] = {0};
static int s_rssi = 0;

/* 网络接口 */
static esp_netif_t *s_sta_netif = NULL;
static esp_netif_t *s_ap_netif = NULL;

/* 重连控制 */
#define MAX_RETRY_COUNT     5
static int s_retry_count = 0;

/* WiFi 事件处理 */
static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT) {
        switch (event_id) {
        case WIFI_EVENT_STA_START:
            ESP_LOGI(TAG, "STA 模式启动");
            esp_wifi_connect();
            break;

        case WIFI_EVENT_STA_DISCONNECTED:
            ESP_LOGW(TAG, "WiFi 断开连接");
            s_connected = false;
            bool disconnected = false;
            event_bus_publish(EVT_WIFI_STATUS, &disconnected);

            /* 自动重连 */
            if (s_retry_count < MAX_RETRY_COUNT && s_ssid[0] != '\0') {
                s_retry_count++;
                ESP_LOGI(TAG, "尝试重连 (%d/%d)", s_retry_count, MAX_RETRY_COUNT);
                esp_wifi_connect();
            } else {
                ESP_LOGE(TAG, "WiFi 连接失败，已达到最大重试次数");
            }
            break;

        case WIFI_EVENT_STA_CONNECTED:
            ESP_LOGI(TAG, "WiFi 已关联");
            break;

        case WIFI_EVENT_AP_START:
            ESP_LOGI(TAG, "AP 模式启动");
            s_ap_active = true;
            break;

        case WIFI_EVENT_AP_STOP:
            ESP_LOGI(TAG, "AP 模式停止");
            s_ap_active = false;
            break;

        case WIFI_EVENT_AP_STACONNECTED:
            ESP_LOGI(TAG, "AP: 设备连接");
            break;

        default:
            break;
        }
    } else if (event_base == IP_EVENT) {
        switch (event_id) {
        case IP_EVENT_STA_GOT_IP: {
            ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
            snprintf(s_ip_str, sizeof(s_ip_str), IPSTR, IP2STR(&event->ip_info.ip));
            ESP_LOGI(TAG, "获取 IP: %s", s_ip_str);
            s_connected = true;
            s_retry_count = 0;

            /* 发布连接成功事件 */
            bool connected = true;
            event_bus_publish(EVT_WIFI_STATUS, &connected);
            break;
        }

        case IP_EVENT_STA_LOST_IP:
            ESP_LOGW(TAG, "丢失 IP 地址");
            s_connected = false;
            s_ip_str[0] = '\0';
            break;

        default:
            break;
    }
    }
}

esp_err_t wifi_mgr_init(void)
{
    if (s_initialized) {
        ESP_LOGW(TAG, "WiFi 管理器已初始化");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化 WiFi 管理器");

    /* 初始化网络接口 */
    esp_err_t err = esp_netif_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "网络接口初始化失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 创建默认事件循环 */
    err = esp_event_loop_create_default();
    if (err != ESP_OK && err != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "事件循环创建失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 创建 STA 和 AP 网络接口 */
    s_sta_netif = esp_netif_create_default_wifi_sta();
    s_ap_netif = esp_netif_create_default_wifi_ap();

    /* 初始化 WiFi */
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    err = esp_wifi_init(&cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 初始化失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 注册事件处理 */
    err = esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                               &wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 事件注册失败: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                               &wifi_event_handler, NULL, NULL);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "IP 事件注册失败: %s", esp_err_to_name(err));
        return err;
    }

    s_initialized = true;
    ESP_LOGI(TAG, "WiFi 管理器初始化完成");
    return ESP_OK;
}

esp_err_t wifi_mgr_connect_sta(const char *ssid, const char *password)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi 管理器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    if (!ssid || !password) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "连接 WiFi: %s", ssid);

    /* 保存连接信息 */
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_password, password, sizeof(s_password) - 1);
    s_password[sizeof(s_password) - 1] = '\0';

    /* 如果 AP 模式活跃，先停止 */
    if (s_ap_active) {
        wifi_mgr_stop_ap();
    }

    /* 配置 STA 模式 */
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置 STA 模式失败: %s", esp_err_to_name(err));
        return err;
    }

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    err = esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "配置 WiFi 失败: %s", esp_err_to_name(err));
        return err;
    }

    s_retry_count = 0;
    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动 WiFi 失败: %s", esp_err_to_name(err));
        return err;
    }

    return ESP_OK;
}

esp_err_t wifi_mgr_start_ap(void)
{
    if (!s_initialized) {
        ESP_LOGE(TAG, "WiFi 管理器未初始化");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "启动 AP 模式");

    /* 生成 AP SSID：EReader-XXXX（MAC 末四位） */
    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    char ap_ssid[32];
    snprintf(ap_ssid, sizeof(ap_ssid), "EReader-%02X%02X", mac[4], mac[5]);

    /* 配置 AP 模式 */
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_AP);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置 AP 模式失败: %s", esp_err_to_name(err));
        return err;
    }

    wifi_config_t wifi_config = {
        .ap = {
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK,
        },
    };
    strncpy((char *)wifi_config.ap.ssid, ap_ssid, sizeof(wifi_config.ap.ssid) - 1);
    strncpy((char *)wifi_config.ap.password, "ereader123", sizeof(wifi_config.ap.password) - 1);
    wifi_config.ap.ssid_len = strlen(ap_ssid);

    err = esp_wifi_set_config(WIFI_IF_AP, &wifi_config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "配置 AP 失败: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_start();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "启动 AP 失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "AP 模式启动: SSID=%s, 密码=ereader123, IP=192.168.4.1", ap_ssid);
    return ESP_OK;
}

esp_err_t wifi_mgr_stop_ap(void)
{
    if (!s_ap_active) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止 AP 模式");
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    return err;
}

esp_err_t wifi_mgr_disconnect(void)
{
    if (!s_initialized) {
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "断开 WiFi 连接");
    s_connected = false;
    s_ip_str[0] = '\0';

    bool disconnected = false;
    event_bus_publish(EVT_WIFI_STATUS, &disconnected);

    return esp_wifi_disconnect();
}

bool wifi_mgr_is_connected(void)
{
    return s_connected;
}

esp_err_t wifi_mgr_get_ip(char *ip_str, size_t max_len)
{
    if (!ip_str || max_len == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    if (!s_connected) {
        ip_str[0] = '\0';
        return ESP_ERR_INVALID_STATE;
    }

    strncpy(ip_str, s_ip_str, max_len - 1);
    ip_str[max_len - 1] = '\0';
    return ESP_OK;
}

esp_err_t wifi_mgr_scan(wifi_ap_record_t *results, uint16_t *count)
{
    if (!s_initialized || !results || !count) {
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "扫描 WiFi...");

    /* 设置为 STA 模式以进行扫描 */
    esp_err_t err = esp_wifi_set_mode(WIFI_MODE_STA);
    if (err != ESP_OK) {
        return err;
    }

    err = esp_wifi_scan_start(NULL, true);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "WiFi 扫描失败: %s", esp_err_to_name(err));
        return err;
    }

    err = esp_wifi_scan_get_ap_records(count, results);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "获取扫描结果失败: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "扫描完成，发现 %d 个 AP", *count);
    return ESP_OK;
}

const char *wifi_mgr_get_ssid(void)
{
    return s_ssid;
}

int wifi_mgr_get_rssi(void)
{
    if (!s_connected) {
        return 0;
    }

    wifi_ap_record_t ap_info;
    if (esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        s_rssi = ap_info.rssi;
    }
    return s_rssi;
}
