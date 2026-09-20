file(GLOB_RECURSE SOURCES *.c)

idf_component_register(
    SRCS
        ${SOURCES}
    INCLUDE_DIRS
        "."
    REQUIRES
        airplay
        myDriver
        lvgl
        usb_host_msc
        esp_wifi
        esp_netif
        espressif__mdns
        spi_flash
        esp_driver_gptimer
        esp_driver_gpio
        esp_driver_ledc
        driver
)
