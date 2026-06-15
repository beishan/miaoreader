#include "keys.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"

static const char *TAG = "keys";

#define KEY_PWR_PIN   GPIO_NUM_0
#define KEY_PREV_PIN  GPIO_NUM_9
#define KEY_NEXT_PIN  GPIO_NUM_10
#define KEY_HOME_PIN  GPIO_NUM_8

#define DEBOUNCE_MS   10
#define SHORT_PRESS_MS  500
#define LONG_PRESS_MS   800
#define SUPER_LONG_MS   2000
#define COMBO_WINDOW_MS 100

static key_callback_t callback = NULL;
static TaskHandle_t key_task_handle = NULL;

typedef struct {
    gpio_num_t pin;
    KeyId id;
    uint32_t press_start;
    bool pressed;
    bool long_triggered;
} key_state_t;

static key_state_t keys[KEY_COUNT] = {
    {KEY_PWR_PIN, KEY_PWR, 0, false, false},
    {KEY_PREV_PIN, KEY_PREV, 0, false, false},
    {KEY_NEXT_PIN, KEY_NEXT, 0, false, false},
    {KEY_HOME_PIN, KEY_HOME, 0, false, false},
};

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    int idx = (int)arg;
    if (idx >= 0 && idx < KEY_COUNT) {
        key_state_t *k = &keys[idx];
        k->pressed = (gpio_get_level(k->pin) == 0);
        if (k->pressed) {
            k->press_start = xTaskGetTickCountFromISR();
            k->long_triggered = false;
        }
    }
}

static void key_scan_task(void *arg)
{
    while (1) {
        uint32_t now = xTaskGetTickCount();
        
        for (int i = 0; i < KEY_COUNT; i++) {
            key_state_t *k = &keys[i];
            
            if (k->pressed) {
                uint32_t duration = (now - k->press_start) * portTICK_PERIOD_MS;
                
                if (!k->long_triggered && duration >= LONG_PRESS_MS) {
                    k->long_triggered = true;
                    if (callback) {
                        callback(k->id, KEY_EVT_LONG);
                    }
                }
                
                if (duration >= SUPER_LONG_MS && k->id == KEY_PWR) {
                    if (callback) {
                        callback(k->id, KEY_EVT_SUPER_LONG);
                    }
                    k->pressed = false;
                }
            } else {
                if (k->press_start > 0) {
                    uint32_t duration = (now - k->press_start) * portTICK_PERIOD_MS;
                    
                    if (duration >= DEBOUNCE_MS && duration < SHORT_PRESS_MS) {
                        if (callback) {
                            callback(k->id, KEY_EVT_SHORT);
                        }
                    }
                    
                    k->press_start = 0;
                }
            }
        }
        
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

esp_err_t keys_init(key_callback_t cb)
{
    ESP_LOGI(TAG, "初始化按键驱动");
    
    callback = cb;
    
    gpio_config_t io_conf = {
        .intr_type = GPIO_INTR_ANYEDGE,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
    };
    
    for (int i = 0; i < KEY_COUNT; i++) {
        io_conf.pin_bit_mask = (1ULL << keys[i].pin);
        gpio_config(&io_conf);
    }
    
    gpio_install_isr_service(0);
    
    for (int i = 0; i < KEY_COUNT; i++) {
        gpio_isr_handler_add(keys[i].pin, gpio_isr_handler, (void *)i);
    }
    
    xTaskCreate(key_scan_task, "key_scan", 2048, NULL, 10, &key_task_handle);
    
    ESP_LOGI(TAG, "按键驱动初始化完成");
    return ESP_OK;
}
