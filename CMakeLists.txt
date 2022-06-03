idf_component_register(
    SRCS "src/esp_hass.c"
        "src/parser.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "src"
    REQUIRES log lwip mbedtls esp_websocket_client json)
