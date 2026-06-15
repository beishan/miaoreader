/**
 * @file bookmark.c
 * @brief 书签管理模块：NVS 持久化存储
 */
#include "bookmark.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>
#include <time.h>

static const char *TAG = "bookmark";

static const char *NVS_NAMESPACE = "bookmarks";
static const char *NVS_KEY_LIST = "bookmark_list";

static Bookmark s_bookmarks[BOOKMARK_MAX];
static int s_count = 0;
static bool s_initialized = false;

/* 从 NVS 加载书签 */
static void load_from_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &handle);
    if (err != ESP_OK) return;

    size_t size = sizeof(s_bookmarks);
    if (nvs_get_blob(handle, NVS_KEY_LIST, s_bookmarks, &size) == ESP_OK) {
        s_count = size / sizeof(Bookmark);
        ESP_LOGI(TAG, "从 NVS 加载 %d 个书签", s_count);
    }

    nvs_close(handle);
}

/* 保存书签到 NVS */
static void save_to_nvs(void)
{
    nvs_handle_t handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &handle);
    if (err != ESP_OK) return;

    nvs_set_blob(handle, NVS_KEY_LIST, s_bookmarks, s_count * sizeof(Bookmark));
    nvs_commit(handle);
    nvs_close(handle);

    ESP_LOGI(TAG, "保存 %d 个书签到 NVS", s_count);
}

esp_err_t bookmark_init(void)
{
    if (s_initialized) return ESP_OK;

    ESP_LOGI(TAG, "初始化书签模块");
    load_from_nvs();
    s_initialized = true;
    return ESP_OK;
}

esp_err_t bookmark_add(const char *book_name, uint32_t page_num)
{
    if (!book_name) return ESP_ERR_INVALID_ARG;

    /* 检查是否已存在 */
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_bookmarks[i].book_name, book_name) == 0 &&
            s_bookmarks[i].page_num == page_num) {
            ESP_LOGW(TAG, "书签已存在: %s 第 %lu 页", book_name, (unsigned long)page_num);
            return ESP_OK;
        }
    }

    /* 检查容量 */
    if (s_count >= BOOKMARK_MAX) {
        ESP_LOGE(TAG, "书签已满 (%d)", BOOKMARK_MAX);
        return ESP_ERR_NO_MEM;
    }

    /* 添加书签 */
    Bookmark *bm = &s_bookmarks[s_count];
    strncpy(bm->book_name, book_name, BOOKMARK_BOOK_NAME_LEN - 1);
    bm->book_name[BOOKMARK_BOOK_NAME_LEN - 1] = '\0';
    bm->page_num = page_num;

    time_t now;
    time(&now);
    bm->timestamp = (uint32_t)now;

    s_count++;
    save_to_nvs();

    ESP_LOGI(TAG, "添加书签: %s 第 %lu 页", book_name, (unsigned long)page_num);
    return ESP_OK;
}

esp_err_t bookmark_remove(const char *book_name, uint32_t page_num)
{
    if (!book_name) return ESP_ERR_INVALID_ARG;

    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_bookmarks[i].book_name, book_name) == 0 &&
            s_bookmarks[i].page_num == page_num) {
            /* 移除：用最后一个覆盖 */
            if (i < s_count - 1) {
                s_bookmarks[i] = s_bookmarks[s_count - 1];
            }
            s_count--;
            save_to_nvs();

            ESP_LOGI(TAG, "删除书签: %s 第 %lu 页", book_name, (unsigned long)page_num);
            return ESP_OK;
        }
    }

    ESP_LOGW(TAG, "书签不存在: %s 第 %lu 页", book_name, (unsigned long)page_num);
    return ESP_ERR_NOT_FOUND;
}

esp_err_t bookmark_get_list(const char *book_name, Bookmark *bookmarks,
                            int max_count, int *count)
{
    if (!book_name || !bookmarks || !count) return ESP_ERR_INVALID_ARG;

    *count = 0;
    for (int i = 0; i < s_count && *count < max_count; i++) {
        if (strcmp(s_bookmarks[i].book_name, book_name) == 0) {
            bookmarks[*count] = s_bookmarks[i];
            (*count)++;
        }
    }

    return ESP_OK;
}

bool bookmark_exists(const char *book_name, uint32_t page_num)
{
    if (!book_name) return false;

    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_bookmarks[i].book_name, book_name) == 0 &&
            s_bookmarks[i].page_num == page_num) {
            return true;
        }
    }

    return false;
}
