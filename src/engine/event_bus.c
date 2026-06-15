#include "event_bus.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "event_bus";

#define MAX_HANDLERS_PER_EVENT 8

typedef struct {
    event_handler_t handlers[MAX_HANDLERS_PER_EVENT];
    int count;
} event_slot_t;

static event_slot_t events[EVT_WAKE + 1];

esp_err_t event_bus_init(void)
{
    ESP_LOGI(TAG, "初始化事件总线");
    memset(events, 0, sizeof(events));
    return ESP_OK;
}

esp_err_t event_bus_subscribe(EventType type, event_handler_t handler)
{
    if (type > EVT_WAKE || !handler) {
        return ESP_ERR_INVALID_ARG;
    }
    
    event_slot_t *slot = &events[type];
    
    if (slot->count >= MAX_HANDLERS_PER_EVENT) {
        ESP_LOGE(TAG, "事件处理器已满");
        return ESP_ERR_NO_MEM;
    }
    
    for (int i = 0; i < slot->count; i++) {
        if (slot->handlers[i] == handler) {
            ESP_LOGW(TAG, "处理器已注册");
            return ESP_OK;
        }
    }
    
    slot->handlers[slot->count++] = handler;
    ESP_LOGI(TAG, "订阅事件 %d", type);
    
    return ESP_OK;
}

esp_err_t event_bus_unsubscribe(EventType type, event_handler_t handler)
{
    if (type > EVT_WAKE || !handler) {
        return ESP_ERR_INVALID_ARG;
    }
    
    event_slot_t *slot = &events[type];
    
    for (int i = 0; i < slot->count; i++) {
        if (slot->handlers[i] == handler) {
            for (int j = i; j < slot->count - 1; j++) {
                slot->handlers[j] = slot->handlers[j + 1];
            }
            slot->count--;
            ESP_LOGI(TAG, "取消订阅事件 %d", type);
            return ESP_OK;
        }
    }
    
    ESP_LOGW(TAG, "处理器未找到");
    return ESP_ERR_NOT_FOUND;
}

esp_err_t event_bus_publish(EventType type, void *data)
{
    if (type > EVT_WAKE) {
        return ESP_ERR_INVALID_ARG;
    }
    
    event_slot_t *slot = &events[type];
    
    for (int i = 0; i < slot->count; i++) {
        if (slot->handlers[i]) {
            slot->handlers[i](type, data);
        }
    }
    
    return ESP_OK;
}
