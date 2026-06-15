idf_component_register(SRCS "main.c" "engine/config.c" "engine/event_bus.c" "hal/epd.c" "hal/keys.c" "hal/battery.c" "hal/rtc.c" "hal/sd_card.c" "ui/page_mgr.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_wifi esp_netif nvs_flash)
