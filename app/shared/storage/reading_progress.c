/**
 * @file reading_progress.c
 * @brief 阅读进度存储抽象层
 *
 * ESP32: 使用 NVS
 * PC: 使用文件
 */
#include "reading_progress.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifdef PC_SIMULATION

/* PC 端：使用文件存储 */
#define PROGRESS_DIR "progress"

esp_err_t reading_progress_init(void)
{
    /* 创建进度目录 */
    mkdir(PROGRESS_DIR, 0755);
    return ESP_OK;
}

esp_err_t reading_progress_save(const char *book_id, uint32_t page)
{
    if (!book_id) return ESP_ERR_INVALID_ARG;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s.txt", PROGRESS_DIR, book_id);

    FILE *f = fopen(path, "w");
    if (!f) return ESP_FAIL;

    fprintf(f, "%u", (unsigned)page);
    fclose(f);

    return ESP_OK;
}

esp_err_t reading_progress_load(const char *book_id, uint32_t *page)
{
    if (!book_id || !page) return ESP_ERR_INVALID_ARG;

    char path[256];
    snprintf(path, sizeof(path), "%s/%s.txt", PROGRESS_DIR, book_id);

    FILE *f = fopen(path, "r");
    if (!f) {
        *page = 0;
        return ESP_OK;
    }

    if (fscanf(f, "%u", (unsigned *)page) != 1) {
        *page = 0;
    }

    fclose(f);
    return ESP_OK;
}

#else /* ESP32 */

/* ESP32 端：使用 NVS */
#include "nvs_flash.h"
#include "nvs.h"

static const char *NVS_NAMESPACE = "reader_progress";

esp_err_t reading_progress_init(void)
{
    return ESP_OK;
}

esp_err_t reading_progress_save(const char *book_id, uint32_t page)
{
    if (!book_id) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return err;

    /* 使用 book_id 的 hash 作为 key */
    char key[16];
    uint32_t h = 2166136261u;
    for (const char *p = book_id; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    snprintf(key, sizeof(key), "pg_%08lx", (unsigned long)h);

    nvs_set_u32(handle, key, page);
    nvs_commit(handle);
    nvs_close(handle);

    return ESP_OK;
}

esp_err_t reading_progress_load(const char *book_id, uint32_t *page)
{
    if (!book_id || !page) return ESP_ERR_INVALID_ARG;

    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) {
        *page = 0;
        return ESP_OK;
    }

    char key[16];
    uint32_t h = 2166136261u;
    for (const char *p = book_id; *p; p++) {
        h ^= (uint8_t)*p;
        h *= 16777619u;
    }
    snprintf(key, sizeof(key), "pg_%08lx", (unsigned long)h);

    uint32_t val = 0;
    nvs_get_u32(handle, key, &val);
    *page = val;

    nvs_close(handle);
    return ESP_OK;
}

#endif /* PC_SIMULATION */
