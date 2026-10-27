idf_component_register(
    SRCS "main.c" "board.c" "wifi_mgr.c" "wifi_scan.c" "ble_scan.c"
         "web_server.c" "mqtt_service.c" "ui.c"
    INCLUDE_DIRS "."
    EMBED_TXTFILES "web/index.html"
    PRIV_REQUIRES driver esp_lcd esp_adc esp_wifi esp_netif esp_event
                  nvs_flash esp_timer spi_flash esp_hw_support esp_system
                  bt esp_http_server app_update esp_partition mqtt
)
