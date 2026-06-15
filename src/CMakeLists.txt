idf_component_register(
    SRCS "main.c"
         "engine/config.c"
         "engine/event_bus.c"
         "engine/typesetter.c"
         "engine/book_parser.c"
         "hal/epd.c"
         "hal/keys.c"
         "hal/battery.c"
         "hal/rtc.c"
         "hal/sd_card.c"
         "ui/page_mgr.c"
         "ui/widget.c"
         "ui/status_bar.c"
    INCLUDE_DIRS "."
    REQUIRES driver esp_wifi esp_netif nvs_flash esp_timer freetype espressif__cjson
)
