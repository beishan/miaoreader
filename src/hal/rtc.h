#pragma once
#include <stdint.h>
#include <time.h>
#include "esp_err.h"

esp_err_t ds3231_init(void);
esp_err_t ds3231_get_time(struct tm *timeinfo);
esp_err_t ds3231_set_time(const struct tm *timeinfo);
