/**
 * @file book_meta.c
 * @brief 书籍元数据管理：SD:/books/metadata.json
 */
#include "book_meta.h"
#include "book_parser.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>

static const char *TAG = "book_meta";

static const char *META_PATH = "/sdcard/books/metadata.json";

static BookMeta s_books[BOOK_META_MAX];
static int s_count = 0;
static bool s_initialized = false;

/* 从 JSON 解析单个书籍元数据 */
static void parse_book_json(cJSON *item, BookMeta *meta)
{
    memset(meta, 0, sizeof(BookMeta));

    cJSON *id = cJSON_GetObjectItem(item, "id");
    cJSON *title = cJSON_GetObjectItem(item, "title");
    cJSON *author = cJSON_GetObjectItem(item, "author");
    cJSON *filePath = cJSON_GetObjectItem(item, "filePath");
    cJSON *totalPages = cJSON_GetObjectItem(item, "totalPages");
    cJSON *currentPage = cJSON_GetObjectItem(item, "currentPage");
    cJSON *readingSeconds = cJSON_GetObjectItem(item, "readingSeconds");
    cJSON *lastRead = cJSON_GetObjectItem(item, "lastRead");

    if (id && cJSON_IsString(id))
        strncpy(meta->id, id->valuestring, sizeof(meta->id) - 1);
    if (title && cJSON_IsString(title))
        strncpy(meta->title, title->valuestring, sizeof(meta->title) - 1);
    if (author && cJSON_IsString(author))
        strncpy(meta->author, author->valuestring, sizeof(meta->author) - 1);
    if (filePath && cJSON_IsString(filePath))
        strncpy(meta->filePath, filePath->valuestring, sizeof(meta->filePath) - 1);
    if (totalPages && cJSON_IsNumber(totalPages))
        meta->totalPages = (uint32_t)totalPages->valueint;
    if (currentPage && cJSON_IsNumber(currentPage))
        meta->currentPage = (uint32_t)currentPage->valueint;
    if (readingSeconds && cJSON_IsNumber(readingSeconds))
        meta->readingSeconds = (uint32_t)readingSeconds->valueint;
}

/* 将单个书籍元数据转为 JSON */
static cJSON *book_to_json(const BookMeta *meta)
{
    cJSON *item = cJSON_CreateObject();
    cJSON_AddStringToObject(item, "id", meta->id);
    cJSON_AddStringToObject(item, "title", meta->title);
    cJSON_AddStringToObject(item, "author", meta->author);
    cJSON_AddStringToObject(item, "filePath", meta->filePath);
    cJSON_AddNumberToObject(item, "totalPages", meta->totalPages);
    cJSON_AddNumberToObject(item, "currentPage", meta->currentPage);
    cJSON_AddNumberToObject(item, "readingSeconds", meta->readingSeconds);
    return item;
}

/* 从文件加载元数据 */
static esp_err_t load_from_file(void)
{
    FILE *f = fopen(META_PATH, "r");
    if (!f) {
        ESP_LOGI(TAG, "metadata.json 不存在，将创建新文件");
        s_count = 0;
        return ESP_OK;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (size <= 0 || size > 64 * 1024) {
        fclose(f);
        s_count = 0;
        return ESP_OK;
    }

    char *buf = malloc(size + 1);
    if (!buf) {
        fclose(f);
        return ESP_ERR_NO_MEM;
    }

    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    cJSON *root = cJSON_Parse(buf);
    free(buf);

    if (!root || !cJSON_IsArray(root)) {
        if (root) cJSON_Delete(root);
        s_count = 0;
        return ESP_OK;
    }

    s_count = 0;
    cJSON *item;
    cJSON_ArrayForEach(item, root) {
        if (s_count >= BOOK_META_MAX) break;
        parse_book_json(item, &s_books[s_count]);
        s_count++;
    }

    cJSON_Delete(root);
    ESP_LOGI(TAG, "从 metadata.json 加载 %d 本书籍", s_count);
    return ESP_OK;
}

esp_err_t book_meta_init(void)
{
    if (s_initialized) return ESP_OK;

    ESP_LOGI(TAG, "初始化书籍元数据模块");
    load_from_file();
    s_initialized = true;
    return ESP_OK;
}

esp_err_t book_meta_save(void)
{
    cJSON *root = cJSON_CreateArray();
    for (int i = 0; i < s_count; i++) {
        cJSON *item = book_to_json(&s_books[i]);
        cJSON_AddItemToArray(root, item);
    }

    char *json = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);

    if (!json) return ESP_ERR_NO_MEM;

    FILE *f = fopen(META_PATH, "w");
    if (!f) {
        free(json);
        ESP_LOGE(TAG, "无法写入 metadata.json");
        return ESP_FAIL;
    }

    fwrite(json, 1, strlen(json), f);
    fclose(f);
    free(json);

    ESP_LOGI(TAG, "保存 metadata.json (%d 本书籍)", s_count);
    return ESP_OK;
}

int book_meta_get_count(void)
{
    return s_count;
}

esp_err_t book_meta_get(int index, BookMeta *meta)
{
    if (index < 0 || index >= s_count || !meta) {
        return ESP_ERR_INVALID_ARG;
    }
    *meta = s_books[index];
    return ESP_OK;
}

esp_err_t book_meta_find_by_path(const char *file_path, BookMeta *meta)
{
    if (!file_path || !meta) return ESP_ERR_INVALID_ARG;

    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_books[i].filePath, file_path) == 0) {
            *meta = s_books[i];
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t book_meta_upsert(const BookMeta *meta)
{
    if (!meta) return ESP_ERR_INVALID_ARG;

    /* 查找是否已存在 */
    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_books[i].filePath, meta->filePath) == 0) {
            s_books[i] = *meta;
            return book_meta_save();
        }
    }

    /* 不存在则添加 */
    if (s_count >= BOOK_META_MAX) {
        ESP_LOGE(TAG, "元数据已满 (%d)", BOOK_META_MAX);
        return ESP_ERR_NO_MEM;
    }

    s_books[s_count] = *meta;
    s_count++;
    return book_meta_save();
}

esp_err_t book_meta_remove(const char *file_path)
{
    if (!file_path) return ESP_ERR_INVALID_ARG;

    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_books[i].filePath, file_path) == 0) {
            /* 用最后一个覆盖 */
            if (i < s_count - 1) {
                s_books[i] = s_books[s_count - 1];
            }
            s_count--;
            return book_meta_save();
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t book_meta_update_page(const char *file_path, uint32_t current_page)
{
    if (!file_path) return ESP_ERR_INVALID_ARG;

    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_books[i].filePath, file_path) == 0) {
            s_books[i].currentPage = current_page;
            return book_meta_save();
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t book_meta_update_total_pages(const char *file_path, uint32_t total_pages)
{
    if (!file_path) return ESP_ERR_INVALID_ARG;

    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_books[i].filePath, file_path) == 0) {
            s_books[i].totalPages = total_pages;
            return book_meta_save();
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t book_meta_add_reading_time(const char *file_path, uint32_t seconds)
{
    if (!file_path) return ESP_ERR_INVALID_ARG;

    for (int i = 0; i < s_count; i++) {
        if (strcmp(s_books[i].filePath, file_path) == 0) {
            s_books[i].readingSeconds += seconds;
            return book_meta_save();
        }
    }
    return ESP_ERR_NOT_FOUND;
}

esp_err_t book_meta_sync_from_sd(void)
{
    DIR *dir = opendir("/sdcard/books");
    if (!dir) {
        ESP_LOGW(TAG, "/sdcard/books 目录不存在");
        return ESP_ERR_NOT_FOUND;
    }

    /* 标记所有现有书籍为"未找到" */
    bool found[BOOK_META_MAX] = {0};

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        if (entry->d_name[0] == '.') continue;

        const char *ext = strrchr(entry->d_name, '.');
        if (!ext) continue;
        if (strcasecmp(ext, ".txt") != 0 && strcasecmp(ext, ".epub") != 0) continue;

        char path[280];
        snprintf(path, sizeof(path), "/sdcard/books/%s", entry->d_name);

        /* 查找是否已存在 */
        bool exists = false;
        for (int i = 0; i < s_count; i++) {
            if (strcmp(s_books[i].filePath, path) == 0) {
                found[i] = true;
                exists = true;
                break;
            }
        }

        /* 不存在则添加 */
        if (!exists && s_count < BOOK_META_MAX) {
            BookMeta *meta = &s_books[s_count];
            memset(meta, 0, sizeof(BookMeta));

            /* 生成 ID */
            book_extract_metadata(path, meta);

            found[s_count] = true;
            s_count++;
            ESP_LOGI(TAG, "发现新书籍: %s", meta->title);
        }
    }
    closedir(dir);

    /* 删除已不存在的书籍 */
    for (int i = s_count - 1; i >= 0; i--) {
        if (!found[i]) {
            ESP_LOGI(TAG, "删除已不存在的书籍: %s", s_books[i].title);
            if (i < s_count - 1) {
                s_books[i] = s_books[s_count - 1];
            }
            s_count--;
        }
    }

    book_meta_save();
    ESP_LOGI(TAG, "同步完成，共 %d 本书籍", s_count);
    return ESP_OK;
}

esp_err_t book_meta_get_current_reading(BookMeta *meta)
{
    if (!meta) return ESP_ERR_INVALID_ARG;

    /* 找到 currentPage > 0 且 totalPages > 0 的书，按 readingSeconds 排序 */
    int best_idx = -1;
    uint32_t max_time = 0;

    for (int i = 0; i < s_count; i++) {
        if (s_books[i].currentPage > 0 && s_books[i].totalPages > 0) {
            if (s_books[i].readingSeconds > max_time) {
                max_time = s_books[i].readingSeconds;
                best_idx = i;
            }
        }
    }

    if (best_idx >= 0) {
        *meta = s_books[best_idx];
        return ESP_OK;
    }

    return ESP_ERR_NOT_FOUND;
}
