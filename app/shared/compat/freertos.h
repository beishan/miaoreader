/**
 * @file freertos.h
 * @brief FreeRTOS 兼容层（PC 仿真用）
 */
#pragma once

#ifdef PC_SIMULATION

#include <stdint.h>
#include <unistd.h>

/* FreeRTOS 类型定义 */
typedef uint32_t TickType_t;
typedef void * TaskHandle_t;
typedef void * TimerHandle_t;

#define pdMS_TO_TICKS(x) ((TickType_t)(x))
#define portTICK_PERIOD_MS 1

/* FreeRTOS 函数 stub */
static inline void vTaskDelay(TickType_t ticks)
{
    usleep(ticks * 1000);
}

static inline TickType_t xTaskGetTickCount(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (TickType_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

static inline TickType_t xTaskGetTickCountFromISR(void)
{
    return xTaskGetTickCount();
}

/* 定时器 stub */
typedef void (*TimerCallbackFunction_t)(TimerHandle_t);

typedef struct {
    const char *name;
    TimerCallbackFunction_t callback;
} TimerHandle_t_create_args;

static inline TimerHandle_t xTimerCreate(const char *name, TickType_t period,
                                          int auto_reload, void *id,
                                          TimerCallbackFunction_t callback)
{
    (void)name; (void)period; (void)auto_reload; (void)id; (void)callback;
    return NULL;
}

static inline int xTimerStart(TimerHandle_t timer, TickType_t ticks)
{
    (void)timer; (void)ticks;
    return 1;
}

#endif /* PC_SIMULATION */
