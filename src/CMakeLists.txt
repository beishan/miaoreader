idf_component_register(SRCS "main.c" "engine/config.c"
                       INCLUDE_DIRS "."
                       REQUIRES driver esp_wifi esp_netif nvs_flash)
