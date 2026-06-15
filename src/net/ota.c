/**
 * @file ota.c
 * @brief OTA 升级模块
 */
#include "ota.h"
#include "esp_ota_ops.h"
#include "esp_log.h"
#include "cJSON.h"

static const char *TAG = "ota";

static esp_ota_handle_t s_ota_handle = 0;
static bool s_ota_in_progress = false;
static const esp_partition_t *s_update_partition = NULL;

esp_err_t ota_init(void)
{
    ESP_LOGI(TAG, "OTA 模块初始化");
    return ESP_OK;
}

/* POST /api/system/update - OTA 升级 */
esp_err_t ota_update_handler(httpd_req_t *req)
{
    char buf[1024];
    int ret;
    int remaining = req->content_len;

    ESP_LOGI(TAG, "OTA 升级开始，固件大小: %d 字节", remaining);

    /* 检查是否有正在进行的 OTA */
    if (s_ota_in_progress) {
        httpd_resp_set_status(req, "409 Conflict");
        httpd_resp_send(req, "{\"error\":\"OTA already in progress\"}", -1);
        return ESP_FAIL;
    }

    /* 获取 OTA 分区 */
    s_update_partition = esp_ota_get_next_update_partition(NULL);
    if (!s_update_partition) {
        ESP_LOGE(TAG, "获取 OTA 分区失败");
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"no OTA partition\"}", -1);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "OTA 分区: %s, 偏移: 0x%lx, 大小: 0x%lx",
             s_update_partition->label,
             (unsigned long)s_update_partition->address,
             (unsigned long)s_update_partition->size);

    /* 开始 OTA */
    esp_err_t err = esp_ota_begin(s_update_partition, OTA_SIZE_UNKNOWN, &s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA 开始失败: %s", esp_err_to_name(err));
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"OTA begin failed\"}", -1);
        return ESP_FAIL;
    }

    s_ota_in_progress = true;

    /* 接收固件数据 */
    int total_received = 0;
    while (remaining > 0) {
        ret = httpd_req_recv(req, buf, remaining > sizeof(buf) ? sizeof(buf) : remaining);
        if (ret <= 0) {
            if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
                continue;
            }
            ESP_LOGE(TAG, "接收数据失败");
            esp_ota_abort(s_ota_handle);
            s_ota_in_progress = false;
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "{\"error\":\"receive failed\"}", -1);
            return ESP_FAIL;
        }

        err = esp_ota_write(s_ota_handle, buf, ret);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "OTA 写入失败: %s", esp_err_to_name(err));
            esp_ota_abort(s_ota_handle);
            s_ota_in_progress = false;
            httpd_resp_set_status(req, "500 Internal Server Error");
            httpd_resp_send(req, "{\"error\":\"OTA write failed\"}", -1);
            return ESP_FAIL;
        }

        total_received += ret;
        remaining -= ret;

        /* 进度日志 */
        if (total_received % 10240 == 0) {
            ESP_LOGI(TAG, "OTA 进度: %d / %d 字节", total_received, req->content_len);
        }
    }

    /* 完成 OTA */
    err = esp_ota_end(s_ota_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA 结束失败: %s", esp_err_to_name(err));
        s_ota_in_progress = false;
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"OTA end failed\"}", -1);
        return ESP_FAIL;
    }

    /* 设置启动分区 */
    err = esp_ota_set_boot_partition(s_update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "设置启动分区失败: %s", esp_err_to_name(err));
        s_ota_in_progress = false;
        httpd_resp_set_status(req, "500 Internal Server Error");
        httpd_resp_send(req, "{\"error\":\"set boot partition failed\"}", -1);
        return ESP_FAIL;
    }

    s_ota_in_progress = false;

    ESP_LOGI(TAG, "OTA 升级完成，共接收 %d 字节", total_received);

    /* 返回成功响应 */
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, "{\"status\":\"ok\",\"message\":\"OTA upgrade successful, rebooting...\"}", -1);

    /* 延迟重启 */
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_restart();

    return ESP_OK;
}
