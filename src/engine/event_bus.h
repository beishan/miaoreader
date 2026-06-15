#pragma once
#include "types.h"
#include "esp_err.h"

typedef void (*event_handler_t)(EventType type, void *data);

esp_err_t event_bus_init(void);
esp_err_t event_bus_subscribe(EventType type, event_handler_t handler);
esp_err_t event_bus_unsubscribe(EventType type, event_handler_t handler);
esp_err_t event_bus_publish(EventType type, void *data);
