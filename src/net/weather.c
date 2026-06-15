/**
 * @file weather.c
 * @brief 天气服务模块：和风天气 API 集成
 */
#include "weather.h"
#include "engine/config.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>
#include <time.h>

static const char *TAG = "weather";

/* 和风天气 API 地址 */
#define QWEATHER_API_HOST   "devapi.qweather.com"
#define QWEATHER_GEO_HOST   "geoapi.qweather.com"
#define WEATHER_TIMEOUT_MS  10000

/* 缓存有效期（秒） */
#define CACHE_VALID_SEC     (30 * 60)  /* 30 分钟 */

/* 静态数据 */
static WeatherData s_current = {0};
static WeatherForecast s_forecast = {0};
static bool s_initialized = false;
static uint32_t s_last_update = 0;
static char s_api_key[64] = {0};
static char s_city[32] = {0};

/* HTTP 响应缓冲区 */
#define RESP_BUF_SIZE 4096
static char s_resp_buf[RESP_BUF_SIZE];
static int s_resp_len = 0;

/* HTTP 事件处理 */
static esp_err_t http_event_handler(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_DATA:
        if (s_resp_len + evt->data_len < RESP_BUF_SIZE) {
            memcpy(s_resp_buf + s_resp_len, evt->data, evt->data_len);
            s_resp_len += evt->data_len;
            s_resp_buf[s_resp_len] = '\0';
        }
        break;
    default:
        break;
    }
    return ESP_OK;
}

/* 发送 HTTP GET 请求 */
static esp_err_t http_get(const char *url, char *resp, int max_len)
{
    s_resp_len = 0;
    memset(s_resp_buf, 0, RESP_BUF_SIZE);

    esp_http_client_config_t config = {
        .url = url,
        .event_handler = http_event_handler,
        .timeout_ms = WEATHER_TIMEOUT_MS,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(TAG, "HTTP 客户端初始化失败");
        return ESP_FAIL;
    }

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "HTTP 请求失败: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return err;
    }

    int status = esp_http_client_get_status_code(client);
    esp_http_client_cleanup(client);

    if (status != 200) {
        ESP_LOGE(TAG, "HTTP 状态码: %d", status);
        return ESP_FAIL;
    }

    /* 复制响应 */
    int copy_len = s_resp_len < max_len - 1 ? s_resp_len : max_len - 1;
    memcpy(resp, s_resp_buf, copy_len);
    resp[copy_len] = '\0';

    return ESP_OK;
}

/* 解析实时天气 JSON */
static esp_err_t parse_weather_now(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        ESP_LOGE(TAG, "JSON 解析失败");
        return ESP_FAIL;
    }

    /* 检查 API 响应码 */
    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!code || !cJSON_IsString(code) || strcmp(code->valuestring, "200") != 0) {
        ESP_LOGE(TAG, "API 错误: %s", code ? code->valuestring : "unknown");
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *now = cJSON_GetObjectItem(root, "now");
    if (!now) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    /* 解析天气数据 */
    cJSON *temp = cJSON_GetObjectItem(now, "temp");
    cJSON *humidity = cJSON_GetObjectItem(now, "humidity");
    cJSON *windSpeed = cJSON_GetObjectItem(now, "windSpeed");
    cJSON *windDir = cJSON_GetObjectItem(now, "windDir");
    cJSON *text = cJSON_GetObjectItem(now, "text");

    if (temp && cJSON_IsString(temp)) {
        s_current.temp_now = atoi(temp->valuestring);
    }
    if (humidity && cJSON_IsString(humidity)) {
        s_current.humidity = atoi(humidity->valuestring);
    }
    if (windSpeed && cJSON_IsString(windSpeed)) {
        s_current.wind_speed = atoi(windSpeed->valuestring);
    }
    if (windDir && cJSON_IsString(windDir)) {
        strncpy(s_current.wind_dir, windDir->valuestring, sizeof(s_current.wind_dir) - 1);
    }
    if (text && cJSON_IsString(text)) {
        strncpy(s_current.condition, text->valuestring, sizeof(s_current.condition) - 1);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

/* 解析天气预报 JSON */
static esp_err_t parse_weather_forecast(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_FAIL;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!code || !cJSON_IsString(code) || strcmp(code->valuestring, "200") != 0) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *daily = cJSON_GetObjectItem(root, "daily");
    if (!daily || !cJSON_IsArray(daily)) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    int i = 0;
    cJSON *day;
    cJSON_ArrayForEach(day, daily) {
        if (i >= 3) break;

        cJSON *fxDate = cJSON_GetObjectItem(day, "fxDate");
        cJSON *textDay = cJSON_GetObjectItem(day, "textDay");
        cJSON *tempMax = cJSON_GetObjectItem(day, "tempMax");
        cJSON *tempMin = cJSON_GetObjectItem(day, "tempMin");

        if (fxDate && cJSON_IsString(fxDate)) {
            /* 只取月-日 */
            strncpy(s_forecast.days[i].date, fxDate->valuestring + 5, 5);
            s_forecast.days[i].date[5] = '\0';
        }
        if (textDay && cJSON_IsString(textDay)) {
            strncpy(s_forecast.days[i].condition, textDay->valuestring,
                    sizeof(s_forecast.days[i].condition) - 1);
        }
        if (tempMax && cJSON_IsString(tempMax)) {
            s_forecast.days[i].temp_high = atoi(tempMax->valuestring);
        }
        if (tempMin && cJSON_IsString(tempMin)) {
            s_forecast.days[i].temp_low = atoi(tempMin->valuestring);
        }

        i++;
    }

    cJSON_Delete(root);
    return ESP_OK;
}

/* 解析空气质量 JSON */
static esp_err_t parse_air_quality(const char *json)
{
    cJSON *root = cJSON_Parse(json);
    if (!root) {
        return ESP_FAIL;
    }

    cJSON *code = cJSON_GetObjectItem(root, "code");
    if (!code || !cJSON_IsString(code) || strcmp(code->valuestring, "200") != 0) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *now = cJSON_GetObjectItem(root, "now");
    if (!now) {
        cJSON_Delete(root);
        return ESP_FAIL;
    }

    cJSON *aqi = cJSON_GetObjectItem(now, "aqi");
    cJSON *category = cJSON_GetObjectItem(now, "category");

    if (aqi && cJSON_IsString(aqi)) {
        s_current.aqi_value = atoi(aqi->valuestring);
    }
    if (category && cJSON_IsString(category)) {
        strncpy(s_current.aqi, category->valuestring, sizeof(s_current.aqi) - 1);
    }

    cJSON_Delete(root);
    return ESP_OK;
}

esp_err_t weather_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    ESP_LOGI(TAG, "初始化天气服务");

    /* 从配置加载 API Key 和城市 */
    SysConfig cfg;
    config_load_sys(&cfg);

    if (cfg.weatherApiKey[0] != '\0') {
        strncpy(s_api_key, cfg.weatherApiKey, sizeof(s_api_key) - 1);
    }
    if (cfg.weatherCity[0] != '\0') {
        strncpy(s_city, cfg.weatherCity, sizeof(s_city) - 1);
    }

    s_initialized = true;

    if (s_api_key[0] == '\0' || s_city[0] == '\0') {
        ESP_LOGW(TAG, "天气 API 未配置");
        return ESP_OK;
    }

    ESP_LOGI(TAG, "天气服务初始化完成: city=%s", s_city);
    return ESP_OK;
}

esp_err_t weather_refresh(void)
{
    if (s_api_key[0] == '\0' || s_city[0] == '\0') {
        ESP_LOGW(TAG, "天气 API 未配置");
        return ESP_ERR_INVALID_STATE;
    }

    ESP_LOGI(TAG, "刷新天气数据...");

    /* 获取实时天气 */
    char url[256];
    snprintf(url, sizeof(url),
             "https://%s/v7/weather/now?location=%s&key=%s",
             QWEATHER_API_HOST, s_city, s_api_key);

    char resp[RESP_BUF_SIZE];
    esp_err_t err = http_get(url, resp, sizeof(resp));
    if (err == ESP_OK) {
        parse_weather_now(resp);
    }

    /* 获取 3 天预报 */
    snprintf(url, sizeof(url),
             "https://%s/v7/weather/3d?location=%s&key=%s",
             QWEATHER_API_HOST, s_city, s_api_key);

    err = http_get(url, resp, sizeof(resp));
    if (err == ESP_OK) {
        parse_weather_forecast(resp);
    }

    /* 获取空气质量 */
    snprintf(url, sizeof(url),
             "https://%s/v7/air/now?location=%s&key=%s",
             QWEATHER_API_HOST, s_city, s_api_key);

    err = http_get(url, resp, sizeof(resp));
    if (err == ESP_OK) {
        parse_air_quality(resp);
    }

    /* 更新时间戳 */
    time_t now;
    time(&now);
    s_last_update = (uint32_t)now;
    s_current.update_time = s_last_update;

    /* 设置城市名 */
    strncpy(s_current.city, s_city, sizeof(s_current.city) - 1);

    /* 设置温度范围（从预报获取今天的数据） */
    if (s_forecast.days[0].temp_high > 0) {
        s_current.temp_high = s_forecast.days[0].temp_high;
        s_current.temp_low = s_forecast.days[0].temp_low;
    }

    ESP_LOGI(TAG, "天气更新完成: %s %d°C %s",
             s_current.city, s_current.temp_now, s_current.condition);

    return ESP_OK;
}

esp_err_t weather_get_current(WeatherData *data)
{
    if (!data) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(data, &s_current, sizeof(WeatherData));
    return ESP_OK;
}

esp_err_t weather_get_forecast(WeatherForecast *data, int days)
{
    if (!data || days <= 0 || days > 3) {
        return ESP_ERR_INVALID_ARG;
    }

    memcpy(data, &s_forecast, sizeof(WeatherForecast));
    return ESP_OK;
}

bool weather_is_stale(void)
{
    if (s_last_update == 0) {
        return true;
    }

    time_t now;
    time(&now);
    return ((uint32_t)now - s_last_update) > CACHE_VALID_SEC;
}

esp_err_t weather_config(const char *api_key, const char *city)
{
    if (!api_key || !city) {
        return ESP_ERR_INVALID_ARG;
    }

    strncpy(s_api_key, api_key, sizeof(s_api_key) - 1);
    s_api_key[sizeof(s_api_key) - 1] = '\0';
    strncpy(s_city, city, sizeof(s_city) - 1);
    s_city[sizeof(s_city) - 1] = '\0';

    /* 保存到配置 */
    SysConfig cfg;
    config_load_sys(&cfg);
    strncpy(cfg.weatherApiKey, api_key, sizeof(cfg.weatherApiKey) - 1);
    strncpy(cfg.weatherCity, city, sizeof(cfg.weatherCity) - 1);
    config_save_sys(&cfg);

    ESP_LOGI(TAG, "天气配置已更新: city=%s", s_city);
    return ESP_OK;
}
