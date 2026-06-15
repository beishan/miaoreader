#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t battery_init(void);
uint8_t battery_get_percent(void);
bool battery_is_charging(void);
bool battery_is_low(void);
bool battery_is_warning(void);
