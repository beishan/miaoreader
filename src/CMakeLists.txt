idf_component_register(SRCS "main.c" "engine/config.c" "hal/epd.c" "hal/keys.c" "hal/battery.c" "hal/rtc.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_wifi esp_netif nvs_flash)
