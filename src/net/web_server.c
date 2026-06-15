/**
 * @file web_server.c
 * @brief 网页服务器模块：HTTP 路由和 API
 */
#include "web_server.h"
#include "ota.h"
#include "weather.h"
#include "hal/wifi_mgr.h"
#include "hal/sd_card.h"
#include "engine/config.h"
#include "engine/types.h"
#include "esp_http_server.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>

static const char *TAG = "web_server";

static httpd_handle_t s_server = NULL;
static bool s_running = false;

/* ============= API 处理函数 ============= */

/* GET /api/system/info - 系统信息 */
static esp_err_t api_system_info_handler(httpd_req_t *req)
{
    cJSON *root = cJSON_CreateObject();

    /* WiFi 信息 */
    cJSON *wifi = cJSON_CreateObject();
    cJSON_AddStringToObject(wifi, "ssid", wifi_mgr_get_ssid());
    cJSON_AddBoolToObject(wifi, "connected", wifi_mgr_is_connected());

    char ip[16];
    wifi_mgr_get_ip(ip, sizeof(ip));
    cJSON_AddStringToObject(wifi, "ip", ip);
    cJSON_AddNumberToObject(wifi, "rssi", wifi_mgr_get_rssi());
    cJSON_AddItemToObject(root, "wifi", wifi);

    /* 系统信息 */
    cJSON *system = cJSON_CreateObject();
    cJSON_AddStringToObject(system, "version", "0.5.0");
    cJSON_AddStringToObject(system, "device", "妙阅 E-Reader");
    cJSON_AddBoolToObject(system, "sd_mounted", sd_card_is_mounted());
    cJSON_AddItemToObject(root, "system", system);

    /* 配置状态 */
    SysConfig cfg;
    config_load_sys(&cfg);
    cJSON *config = cJSON_CreateObject();
    cJSON_AddBoolToObject(config, "initialized", cfg.initialized);
    cJSON_AddStringToObject(config, "weather_city", cfg.weatherCity);
    cJSON_AddBoolToObject(config, "weather_configured", cfg.weatherApiKey[0] != '\0');
    cJSON_AddNumberToObject(config, "sleep_layout", cfg.sleepLayout);
    cJSON_AddNumberToObject(config, "sleep_timeout", cfg.sleepTimeoutSec);
    cJSON_AddItemToObject(root, "config", config);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* GET /api/wifi/scan - 扫描 WiFi */
static esp_err_t api_wifi_scan_handler(httpd_req_t *req)
{
    wifi_ap_record_t results[20];
    uint16_t count = 20;

    esp_err_t err = wifi_mgr_scan(results, &count);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"scan failed\"}", -1);
        return ESP_OK;
    }

    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < count && i < 20; i++) {
        cJSON *ap = cJSON_CreateObject();
        cJSON_AddStringToObject(ap, "ssid", (char *)results[i].ssid);
        cJSON_AddNumberToObject(ap, "rssi", results[i].rssi);
        cJSON_AddNumberToObject(ap, "auth", results[i].authmode);
        cJSON_AddItemToArray(root, ap);
    }

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* POST /api/wifi/connect - 连接 WiFi */
static esp_err_t api_wifi_connect_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"no data\"}", -1);
        return ESP_OK;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"invalid json\"}", -1);
        return ESP_OK;
    }

    cJSON *ssid = cJSON_GetObjectItem(root, "ssid");
    cJSON *password = cJSON_GetObjectItem(root, "password");

    if (!ssid || !cJSON_IsString(ssid)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"ssid required\"}", -1);
        return ESP_OK;
    }

    const char *pwd = (password && cJSON_IsString(password)) ? password->valuestring : "";

    /* 保存 WiFi 配置 */
    SysConfig cfg;
    config_load_sys(&cfg);
    strncpy(cfg.ssid, ssid->valuestring, sizeof(cfg.ssid) - 1);
    strncpy(cfg.password, pwd, sizeof(cfg.password) - 1);
    cfg.initialized = true;
    config_save_sys(&cfg);

    cJSON_Delete(root);

    /* 连接 WiFi */
    esp_err_t err = wifi_mgr_connect_sta(ssid->valuestring, pwd);
    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"connect failed\"}", -1);
        return ESP_OK;
    }

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"connecting\"}", -1);

    /* 延迟重启以允许响应发送 */
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}

/* GET /api/weather/data - 获取天气数据 */
static esp_err_t api_weather_data_handler(httpd_req_t *req)
{
    WeatherData current;
    WeatherForecast forecast;

    weather_get_current(&current);
    weather_get_forecast(&forecast, 3);

    cJSON *root = cJSON_CreateObject();

    /* 当前天气 */
    cJSON *now = cJSON_CreateObject();
    cJSON_AddStringToObject(now, "city", current.city);
    cJSON_AddStringToObject(now, "condition", current.condition);
    cJSON_AddNumberToObject(now, "temp_now", current.temp_now);
    cJSON_AddNumberToObject(now, "temp_high", current.temp_high);
    cJSON_AddNumberToObject(now, "temp_low", current.temp_low);
    cJSON_AddNumberToObject(now, "humidity", current.humidity);
    cJSON_AddStringToObject(now, "aqi", current.aqi);
    cJSON_AddNumberToObject(now, "aqi_value", current.aqi_value);
    cJSON_AddNumberToObject(now, "update_time", current.update_time);
    cJSON_AddItemToObject(root, "current", now);

    /* 预报 */
    cJSON *fc = cJSON_CreateArray();
    for (int i = 0; i < 3; i++) {
        cJSON *day = cJSON_CreateObject();
        cJSON_AddStringToObject(day, "date", forecast.days[i].date);
        cJSON_AddStringToObject(day, "condition", forecast.days[i].condition);
        cJSON_AddNumberToObject(day, "temp_high", forecast.days[i].temp_high);
        cJSON_AddNumberToObject(day, "temp_low", forecast.days[i].temp_low);
        cJSON_AddItemToArray(fc, day);
    }
    cJSON_AddItemToObject(root, "forecast", fc);

    char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    free(json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* POST /api/weather/config - 配置天气 API */
static esp_err_t api_weather_config_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"no data\"}", -1);
        return ESP_OK;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"invalid json\"}", -1);
        return ESP_OK;
    }

    cJSON *api_key = cJSON_GetObjectItem(root, "api_key");
    cJSON *city = cJSON_GetObjectItem(root, "city");

    if (!api_key || !cJSON_IsString(api_key) || !city || !cJSON_IsString(city)) {
        cJSON_Delete(root);
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"api_key and city required\"}", -1);
        return ESP_OK;
    }

    esp_err_t err = weather_config(api_key->valuestring, city->valuestring);
    cJSON_Delete(root);

    if (err != ESP_OK) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"config failed\"}", -1);
        return ESP_OK;
    }

    /* 立即刷新天气 */
    weather_refresh();

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
    return ESP_OK;
}

/* POST /api/sleep/config - 配置休眠布局 */
static esp_err_t api_sleep_config_handler(httpd_req_t *req)
{
    char buf[256];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"no data\"}", -1);
        return ESP_OK;
    }
    buf[len] = '\0';

    cJSON *root = cJSON_Parse(buf);
    if (!root) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"invalid json\"}", -1);
        return ESP_OK;
    }

    cJSON *layout = cJSON_GetObjectItem(root, "layout");
    cJSON *timeout = cJSON_GetObjectItem(root, "timeout");

    SysConfig cfg;
    config_load_sys(&cfg);

    if (layout && cJSON_IsNumber(layout)) {
        cfg.sleepLayout = (uint8_t)layout->valueint;
    }
    if (timeout && cJSON_IsNumber(timeout)) {
        cfg.sleepTimeoutSec = (uint32_t)timeout->valueint;
    }

    config_save_sys(&cfg);
    cJSON_Delete(root);

    ESP_LOGI(TAG, "休眠配置已更新: layout=%u, timeout=%lu",
             cfg.sleepLayout, (unsigned long)cfg.sleepTimeoutSec);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\"}", -1);
    return ESP_OK;
}

/* URL 解码辅助函数 */
static void url_decode(const char *src, char *dst, size_t dst_len)
{
    size_t i = 0, j = 0;
    while (src[i] && j < dst_len - 1) {
        if (src[i] == '%' && src[i+1] && src[i+2]) {
            char hex[3] = {src[i+1], src[i+2], 0};
            dst[j++] = (char)strtol(hex, NULL, 16);
            i += 3;
        } else if (src[i] == '+') {
            dst[j++] = ' ';
            i++;
        } else {
            dst[j++] = src[i++];
        }
    }
    dst[j] = '\0';
}

/* GET /api/books - 书籍列表 */
static esp_err_t api_books_handler(httpd_req_t *req)
{
    DIR *dir = opendir("/sdcard/books");
    if (!dir) {
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "[]", -1);
        return ESP_OK;
    }

    cJSON *books = cJSON_CreateArray();
    struct dirent *entry;

    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        const char *ext = strrchr(entry->d_name, '.');
        if (!ext) continue;
        if (strcasecmp(ext, ".txt") != 0 && strcasecmp(ext, ".epub") != 0) continue;

        cJSON *book = cJSON_CreateObject();
        cJSON_AddStringToObject(book, "name", entry->d_name);

        /* 获取文件大小 */
        char path[280];
        snprintf(path, sizeof(path), "/sdcard/books/%s", entry->d_name);
        struct stat st;
        if (stat(path, &st) == 0) {
            cJSON_AddNumberToObject(book, "size", st.st_size);
        }

        cJSON_AddItemToArray(books, book);
    }
    closedir(dir);

    char *json = cJSON_PrintUnformatted(books);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    free(json);
    cJSON_Delete(books);
    return ESP_OK;
}

/* POST /api/books/delete?name=xxx - 删除书籍 */
static esp_err_t api_books_delete_handler(httpd_req_t *req)
{
    /* 从查询参数获取书名 */
    char name[256] = {0};
    size_t buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        char *buf = malloc(buf_len);
        if (buf) {
            httpd_req_get_url_query_str(req, buf, buf_len);
            httpd_query_key_value(buf, "name", name, sizeof(name));
            free(buf);
        }
    }

    if (name[0] == '\0') {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"name parameter required\"}", -1);
        return ESP_OK;
    }

    /* URL 解码 */
    char decoded[256];
    url_decode(name, decoded, sizeof(decoded));

    char path[280];
    snprintf(path, sizeof(path), "/sdcard/books/%s", decoded);

    if (unlink(path) == 0) {
        ESP_LOGI(TAG, "删除书籍: %s", path);
        httpd_resp_set_type(req, "application/json");
        httpd_resp_send(req, "{\"status\":\"deleted\"}", -1);
    } else {
        httpd_resp_set_status(req, "404 Not Found");
        httpd_resp_send(req, "{\"error\":\"book not found\"}", -1);
    }

    return ESP_OK;
}

/* POST /api/books/upload - 上传书籍 */
static esp_err_t api_books_upload_handler(httpd_req_t *req)
{
    char buf[512];
    int ret;
    int remaining = req->content_len;

    if (remaining <= 0 || remaining > 50 * 1024 * 1024) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"invalid content length\"}", -1);
        return ESP_OK;
    }

    /* 从查询参数获取文件名 */
    char filename[128] = "uploaded.txt";
    size_t query_len = httpd_req_get_url_query_len(req) + 1;
    if (query_len > 1) {
        char *query_buf = malloc(query_len);
        if (query_buf) {
            httpd_req_get_url_query_str(req, query_buf, query_len);
            httpd_query_key_value(query_buf, "name", filename, sizeof(filename));
            free(query_buf);
        }
    }

    /* 如果没有指定文件名，使用默认名称 */
    if (filename[0] == '\0') {
        snprintf(filename, sizeof(filename), "uploaded_%lu.txt", (unsigned long)time(NULL));
    }

    /* 检查文件扩展名 */
    const char *ext = strrchr(filename, '.');
    if (!ext || (strcasecmp(ext, ".txt") != 0 && strcasecmp(ext, ".epub") != 0)) {
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "{\"error\":\"only .txt and .epub files are supported\"}", -1);
        return ESP_OK;
    }

    /* 创建目标文件 */
    char path[280];
    snprintf(path, sizeof(path), "/sdcard/books/%s", filename);
    FILE *f = fopen(path, "wb");
    if (!f) {
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"failed to create file\"}", -1);
        return ESP_OK;
    }

    /* 接收文件数据 */
    int total_received = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf, remaining > sizeof(buf) ? sizeof(buf) : remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) continue;
            fclose(f);
            unlink(path);
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "{\"error\":\"receive failed\"}", -1);
            return ESP_FAIL;
        }
        fwrite(buf, 1, ret, f);
        total_received += ret;
        remaining -= ret;
    }

    fclose(f);
    ESP_LOGI(TAG, "上传书籍: %s (%d 字节)", path, total_received);

    /* 返回成功响应 */
    cJSON *resp = cJSON_CreateObject();
    cJSON_AddStringToObject(resp, "status", "ok");
    cJSON_AddStringToObject(resp, "filename", filename);
    cJSON_AddNumberToObject(resp, "size", total_received);
    char *json = cJSON_PrintUnformatted(resp);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, json, strlen(json));

    free(json);
    cJSON_Delete(resp);
    return ESP_OK;
}

/* ============= 静态页面处理 ============= */

/* 内嵌的 HTML 页面 */
static const char INDEX_HTML[] = R"html(<!DOCTYPE html>
<html><head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>妙阅 E-Reader</title>
<style>
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,sans-serif;background:#f5f5f5;padding:20px}
.container{max-width:600px;margin:0 auto}
.card{background:#fff;border-radius:8px;padding:20px;margin-bottom:16px;box-shadow:0 2px 4px rgba(0,0,0,.1)}
h1{color:#333;margin-bottom:20px;text-align:center}
h2{color:#666;font-size:16px;margin-bottom:12px;border-bottom:1px solid #eee;padding-bottom:8px}
.form-group{margin-bottom:12px}
label{display:block;color:#666;font-size:14px;margin-bottom:4px}
input,select{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;font-size:14px}
button{background:#4CAF50;color:#fff;border:none;padding:10px 20px;border-radius:4px;cursor:pointer;font-size:14px;width:100%;margin-top:8px}
button:hover{background:#45a049}
button.danger{background:#f44336}
button.danger:hover{background:#d32f2f}
button.secondary{background:#2196F3}
button.secondary:hover{background:#1976D2}
.status{padding:8px;background:#e8f5e9;border-radius:4px;margin-bottom:12px;font-size:14px}
.status.error{background:#ffebee}
.book-list{list-style:none}
.book-list li{padding:8px;border-bottom:1px solid #eee;display:flex;justify-content:space-between;align-items:center}
.book-list li:last-child{border-bottom:none}
.delete-btn{background:#f44336;color:#fff;border:none;padding:4px 8px;border-radius:4px;cursor:pointer;font-size:12px}
.weather{text-align:center}
.weather .temp{font-size:48px;font-weight:bold;color:#333}
.weather .condition{color:#666;font-size:18px}
.weather .details{color:#999;font-size:14px;margin-top:8px}
.radio-group{display:flex;gap:16px;margin:8px 0}
.radio-group label{display:flex;align-items:center;gap:4px;cursor:pointer}
.progress{margin-top:8px;font-size:12px;color:#999}
</style>
</head><body>
<div class="container">
<h1>📖 妙阅 E-Reader</h1>

<div class="card">
<h2>📡 WiFi 设置</h2>
<div id="wifi-status" class="status">加载中...</div>
<button class="secondary" onclick="scanWifi()">扫描附近 WiFi</button>
<div id="wifi-list"></div>
<div class="form-group" style="margin-top:12px">
<label>WiFi 名称</label>
<input type="text" id="wifi-ssid" placeholder="选择或输入 WiFi 名称">
</div>
<div class="form-group">
<label>密码</label>
<input type="password" id="wifi-password" placeholder="WiFi 密码">
</div>
<button onclick="connectWifi()">连接 WiFi</button>
</div>

<div class="card weather">
<h2>🌤️ 天气</h2>
<div id="weather-data">
<div class="temp">--°C</div>
<div class="condition">未配置</div>
</div>
<div class="form-group" style="margin-top:16px;text-align:left">
<label>和风天气 API Key</label>
<input type="text" id="weather-key" placeholder="输入 API Key">
</div>
<div class="form-group" style="text-align:left">
<label>城市</label>
<input type="text" id="weather-city" placeholder="输入城市名">
</div>
<button onclick="saveWeatherConfig()">保存天气配置</button>
</div>

<div class="card">
<h2>📚 书架管理</h2>
<div class="form-group">
<label>上传书籍（支持 .txt / .epub）</label>
<input type="file" id="book-file" accept=".txt,.epub">
</div>
<button onclick="uploadBook()">上传</button>
<div id="upload-progress" class="progress"></div>
<ul id="book-list" class="book-list" style="margin-top:16px"><li>加载中...</li></ul>
</div>

<div class="card">
<h2>😴 休眠设置</h2>
<div class="form-group">
<label>休眠布局</label>
<div class="radio-group">
<label><input type="radio" name="sleep-layout" value="0" checked> 时钟天气</label>
<label><input type="radio" name="sleep-layout" value="1"> 风景图</label>
<label><input type="radio" name="sleep-layout" value="2"> 诗词</label>
</div>
</div>
<div class="form-group">
<label>自动休眠时间</label>
<select id="sleep-timeout">
<option value="300">5 分钟</option>
<option value="600">10 分钟</option>
<option value="1800">30 分钟</option>
<option value="0">不自动休眠</option>
</select>
</div>
<button onclick="saveSleepConfig()">保存休眠设置</button>
</div>

<div class="card">
<h2>⬆️ 固件升级</h2>
<div class="form-group">
<label>选择固件文件（.bin）</label>
<input type="file" id="firmware-file" accept=".bin">
</div>
<button class="danger" onclick="uploadFirmware()">升级固件</button>
<div id="firmware-progress" class="progress"></div>
</div>

<div class="card">
<h2>ℹ️ 系统信息</h2>
<div id="system-info">加载中...</div>
</div>
</div>

<script>
async function loadData() {
try {
const [sysRes, weatherRes, booksRes] = await Promise.all([
fetch('/api/system/info').then(r=>r.json()),
fetch('/api/weather/data').then(r=>r.json()),
fetch('/api/books').then(r=>r.json())
]);

const ws = document.getElementById('wifi-status');
if (sysRes.wifi.connected) {
ws.className = 'status';
ws.textContent = '已连接: ' + sysRes.wifi.ssid + ' (' + sysRes.wifi.ip + ')';
} else {
ws.className = 'status error';
ws.textContent = '未连接';
}

const wd = document.getElementById('weather-data');
if (weatherRes.current.temp_now > 0) {
wd.innerHTML = '<div class="temp">' + weatherRes.current.temp_now + '°C</div>' +
'<div class="condition">' + weatherRes.current.city + ' ' + weatherRes.current.condition + '</div>' +
'<div class="details">湿度 ' + weatherRes.current.humidity + '% | AQI ' + weatherRes.current.aqi + '</div>';
}

const bl = document.getElementById('book-list');
if (booksRes.length === 0) {
bl.innerHTML = '<li>暂无书籍</li>';
} else {
bl.innerHTML = booksRes.map(function(b) {
return '<li><span>' + b.name + ' (' + Math.round(b.size/1024) + 'KB)</span>' +
'<button class="delete-btn" onclick="deleteBook(\'' + b.name + '\')">删除</button></li>';
}).join('');
}

var si = document.getElementById('system-info');
si.innerHTML = '版本: ' + sysRes.system.version + ' | SD卡: ' + (sysRes.system.sd_mounted ? '已挂载' : '未挂载');

if (sysRes.config.weather_configured) {
document.getElementById('weather-key').value = '***已配置***';
document.getElementById('weather-city').value = sysRes.config.weather_city;
}

var layout = sysRes.config.sleep_layout || 0;
document.querySelector('input[name="sleep-layout"][value="' + layout + '"]').checked = true;
document.getElementById('sleep-timeout').value = sysRes.config.sleep_timeout || 300;
} catch(e) { console.error(e); }
}

async function scanWifi() {
var list = document.getElementById('wifi-list');
list.innerHTML = '<div style="padding:8px;color:#999">扫描中...</div>';
try {
var res = await fetch('/api/wifi/scan');
var data = await res.json();
list.innerHTML = '<div style="margin-top:8px;border:1px solid #eee;border-radius:4px;max-height:200px;overflow-y:auto">' +
data.map(function(ap) {
return '<div style="padding:8px;cursor:pointer;border-bottom:1px solid #f5f5f5" onclick="document.getElementById(\'wifi-ssid\').value=\'' + ap.ssid + '\'">' + ap.ssid + ' (' + ap.rssi + 'dBm)</div>';
}).join('') + '</div>';
} catch(e) { list.innerHTML = '<div style="padding:8px;color:red">扫描失败</div>'; }
}

async function connectWifi() {
var ssid = document.getElementById('wifi-ssid').value;
var pwd = document.getElementById('wifi-password').value;
if (!ssid) { alert('请输入 WiFi 名称'); return; }
try {
await fetch('/api/wifi/connect', {
method: 'POST',
headers: {'Content-Type':'application/json'},
body: JSON.stringify({ssid: ssid, password: pwd})
});
alert('正在连接，设备将重启...');
} catch(e) { alert('连接失败'); }
}

async function saveWeatherConfig() {
var key = document.getElementById('weather-key').value;
var city = document.getElementById('weather-city').value;
if (!key || !city) { alert('请输入 API Key 和城市'); return; }
try {
await fetch('/api/weather/config', {
method: 'POST',
headers: {'Content-Type':'application/json'},
body: JSON.stringify({api_key: key, city: city})
});
alert('天气配置已保存');
loadData();
} catch(e) { alert('保存失败'); }
}

async function saveSleepConfig() {
var layout = document.querySelector('input[name="sleep-layout"]:checked').value;
var timeout = document.getElementById('sleep-timeout').value;
try {
await fetch('/api/sleep/config', {
method: 'POST',
headers: {'Content-Type':'application/json'},
body: JSON.stringify({layout: parseInt(layout), timeout: parseInt(timeout)})
});
alert('休眠设置已保存');
} catch(e) { alert('保存失败'); }
}

async function uploadBook() {
var fileInput = document.getElementById('book-file');
if (!fileInput.files.length) { alert('请选择文件'); return; }
var file = fileInput.files[0];
var progress = document.getElementById('upload-progress');
progress.textContent = '上传中...';
try {
var formData = new FormData();
formData.append('file', file);
var res = await fetch('/api/books/upload', {method: 'POST', body: formData});
var data = await res.json();
if (data.status === 'ok') {
progress.textContent = '上传成功: ' + data.filename;
loadData();
} else {
progress.textContent = '上传失败: ' + (data.error || 'unknown error');
}
} catch(e) { progress.textContent = '上传失败'; }
}

async function deleteBook(name) {
if (!confirm('确定删除 "' + name + '"？')) return;
try {
await fetch('/api/books/delete?name=' + encodeURIComponent(name), {method: 'POST'});
loadData();
} catch(e) { alert('删除失败'); }
}

async function uploadFirmware() {
var fileInput = document.getElementById('firmware-file');
if (!fileInput.files.length) { alert('请选择固件文件'); return; }
if (!confirm('确定升级固件？设备将重启。')) return;
var file = fileInput.files[0];
var progress = document.getElementById('firmware-progress');
progress.textContent = '上传中，请勿断电...';
try {
var res = await fetch('/api/system/update', {
method: 'POST',
headers: {'Content-Type': 'application/octet-stream'},
body: file
});
if (res.ok) {
progress.textContent = '升级成功，设备正在重启...';
} else {
progress.textContent = '升级失败';
}
} catch(e) { progress.textContent = '升级失败'; }
}

loadData();
setInterval(loadData, 60000);
</script>
</body></html>)html";

/* GET / - 首页 */
static esp_err_t index_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, INDEX_HTML, sizeof(INDEX_HTML) - 1);
    return ESP_OK;
}

/* ============= 服务器管理 ============= */

esp_err_t web_server_init(void)
{
    ESP_LOGI(TAG, "初始化网页服务器");
    return ESP_OK;
}

esp_err_t web_server_start(void)
{
    if (s_running) {
        ESP_LOGW(TAG, "服务器已在运行");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "启动网页服务器");

    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;

    esp_err_t err = httpd_start(&s_server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "服务器启动失败: %s", esp_err_to_name(err));
        return err;
    }

    /* 注册路由 */
    httpd_uri_t uri_root = {
        .uri = "/",
        .method = HTTP_GET,
        .handler = index_handler,
    };
    httpd_register_uri_handler(s_server, &uri_root);

    httpd_uri_t uri_system_info = {
        .uri = "/api/system/info",
        .method = HTTP_GET,
        .handler = api_system_info_handler,
    };
    httpd_register_uri_handler(s_server, &uri_system_info);

    httpd_uri_t uri_wifi_scan = {
        .uri = "/api/wifi/scan",
        .method = HTTP_GET,
        .handler = api_wifi_scan_handler,
    };
    httpd_register_uri_handler(s_server, &uri_wifi_scan);

    httpd_uri_t uri_wifi_connect = {
        .uri = "/api/wifi/connect",
        .method = HTTP_POST,
        .handler = api_wifi_connect_handler,
    };
    httpd_register_uri_handler(s_server, &uri_wifi_connect);

    httpd_uri_t uri_weather_data = {
        .uri = "/api/weather/data",
        .method = HTTP_GET,
        .handler = api_weather_data_handler,
    };
    httpd_register_uri_handler(s_server, &uri_weather_data);

    httpd_uri_t uri_weather_config = {
        .uri = "/api/weather/config",
        .method = HTTP_POST,
        .handler = api_weather_config_handler,
    };
    httpd_register_uri_handler(s_server, &uri_weather_config);

    httpd_uri_t uri_sleep_config = {
        .uri = "/api/sleep/config",
        .method = HTTP_POST,
        .handler = api_sleep_config_handler,
    };
    httpd_register_uri_handler(s_server, &uri_sleep_config);

    httpd_uri_t uri_books = {
        .uri = "/api/books",
        .method = HTTP_GET,
        .handler = api_books_handler,
    };
    httpd_register_uri_handler(s_server, &uri_books);

    httpd_uri_t uri_books_delete = {
        .uri = "/api/books/delete",
        .method = HTTP_POST,
        .handler = api_books_delete_handler,
    };
    httpd_register_uri_handler(s_server, &uri_books_delete);

    httpd_uri_t uri_books_upload = {
        .uri = "/api/books/upload",
        .method = HTTP_POST,
        .handler = api_books_upload_handler,
    };
    httpd_register_uri_handler(s_server, &uri_books_upload);

    httpd_uri_t uri_system_update = {
        .uri = "/api/system/update",
        .method = HTTP_POST,
        .handler = ota_update_handler,
    };
    httpd_register_uri_handler(s_server, &uri_system_update);

    s_running = true;
    ESP_LOGI(TAG, "网页服务器启动成功，访问 http://<device-ip>/");
    return ESP_OK;
}

esp_err_t web_server_stop(void)
{
    if (!s_running || !s_server) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "停止网页服务器");
    esp_err_t err = httpd_stop(s_server);
    s_server = NULL;
    s_running = false;
    return err;
}

bool web_server_is_running(void)
{
    return s_running;
}
