#pragma once
#include "esp_err.h"
#include "engine/types.h"

typedef void (*key_callback_t)(KeyId key, KeyEvent event);

esp_err_t keys_init(key_callback_t cb);
