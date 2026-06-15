#pragma once
#include "types.h"
#include "esp_err.h"

esp_err_t config_init(void);
esp_err_t config_load_sys(SysConfig *cfg);
esp_err_t config_save_sys(const SysConfig *cfg);
esp_err_t config_load_reader(ReaderConfig *cfg);
esp_err_t config_save_reader(const ReaderConfig *cfg);
esp_err_t config_factory_reset(void);
