idf_component_register(
    SRCS "app_main.c" "display/display.c" "input/buttons.c" "resources/image_assets.c" "ui/graphics.c" "ui/ui.c" "voice/voice.c"
    PRIV_REQUIRES esp_lcd esp_driver_spi esp_driver_gpio esp_driver_ledc esp_timer esp_driver_i2s esp_wifi esp_event esp_netif nvs_flash esp_http_client mbedtls
    INCLUDE_DIRS "."
    EMBED_FILES "assets/home.bin" "assets/beg.bin" "assets/caught.bin" "assets/defiant.bin"
                "assets/dust.bin" "assets/flee.bin" "assets/knocked.bin" "assets/scared.bin"
                "assets/gatling.bin" "assets/broom.bin" "assets/sock.bin" "assets/rice.bin" "assets/splash.bin")
