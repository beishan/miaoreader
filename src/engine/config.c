#include "config.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "config";
static const char *NVS_NAMESPACE = "miaoyue";

static const char *SYS_KEY = "sys_config";
static const char *READER_KEY = "reader_config";

esp_err_t config_init(void)
{
    ESP_LOGI(TAG, "NVS 配置初始化");
    return ESP_OK;
}

esp_err_t config_load_sys(SysConfig *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS 打开失败，使用默认配置");
        memset(cfg, 0, sizeof(SysConfig));
        cfg->initialized = false;
        cfg->sleepTimeoutSec = 300;
        return err;
    }

    size_t size = sizeof(SysConfig);
    err = nvs_get_blob(handle, SYS_KEY, cfg, &size);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "读取系统配置失败，使用默认值");
        memset(cfg, 0, sizeof(SysConfig));
        cfg->initialized = false;
        cfg->sleepTimeoutSec = 300;
    }

    return err;
}

esp_err_t config_save_sys(const SysConfig *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS 打开失败");
        return err;
    }

    err = nvs_set_blob(handle, SYS_KEY, cfg, sizeof(SysConfig));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t config_load_reader(ReaderConfig *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS 打开失败，使用默认阅读配置");
        cfg->fontSize = 20;
        cfg->fontId = 0;
        cfg->lineSpacing = 15;
        cfg->charSpacing = 1;
        cfg->margin = 8;
        return err;
    }

    size_t size = sizeof(ReaderConfig);
    err = nvs_get_blob(handle, READER_KEY, cfg, &size);
    nvs_close(handle);

    if (err != ESP_OK) {
        ESP_LOGW(TAG, "读取阅读配置失败，使用默认值");
        cfg->fontSize = 20;
        cfg->fontId = 0;
        cfg->lineSpacing = 15;
        cfg->charSpacing = 1;
        cfg->margin = 8;
    }

    return err;
}

esp_err_t config_save_reader(const ReaderConfig *cfg)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS 打开失败");
        return err;
    }

    err = nvs_set_blob(handle, READER_KEY, cfg, sizeof(ReaderConfig));
    if (err == ESP_OK) {
        err = nvs_commit(handle);
    }

    nvs_close(handle);
    return err;
}

esp_err_t config_factory_reset(void)
{
    ESP_LOGW(TAG, "恢复出厂设置");
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) {
        return err;
    }

    nvs_erase_all(handle);
    nvs_commit(handle);
    nvs_close(handle);

    return ESP_OK;
}
