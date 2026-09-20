file(GLOB_RECURSE SOURCES *.c)

idf_component_register(
    SRCS
        ${SOURCES}
    INCLUDE_DIRS
        "."
    REQUIRES
        main
        esp_driver_spi
        esp_driver_i2s
        esp_driver_gpio
        esp_ringbuf
        driver
)
