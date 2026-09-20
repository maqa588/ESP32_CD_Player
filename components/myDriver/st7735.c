#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/ledc.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "main.h"
#include "st7735.h"

#define TAG "st7735"

#define LCD_BL PIN_TFT_BL
#define LCD_DC PIN_TFT_DCX
#define LCD_CS PIN_SPI_CS
#define LCD_SCLK PIN_SPI_CLK
#define LCD_MOSI PIN_SPI_MOSI

spi_device_handle_t spiHandle = NULL;
uint32_t blDuty = 0;

void lcd_write_cmd(uint8_t cmd)
{
    gpio_set_level(LCD_DC, 0);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8;
    t.tx_data[0] = cmd;
    spi_device_polling_transmit(spiHandle, &t);
}

void lcd_write_data(uint8_t dat)
{
    gpio_set_level(LCD_DC, 1);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 8;
    t.tx_data[0] = dat;
    spi_device_polling_transmit(spiHandle, &t);
}

void lcd_write_data32(uint32_t dat)
{
    gpio_set_level(LCD_DC, 1);
    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 32;
    t.tx_data[0] = (dat >> 24) & 0xFF;
    t.tx_data[1] = (dat >> 16) & 0xFF;
    t.tx_data[2] = (dat >> 8) & 0xFF;
    t.tx_data[3] = dat & 0xFF;
    spi_device_polling_transmit(spiHandle, &t);
}

void lcd_write_data_batch(uint8_t *dat, int len)
{
    if (len <= 0)
        return;

    gpio_set_level(LCD_DC, 1);

    while (len > 0)
    {
        int chunk = len > 4092 ? 4092 : len;
        spi_transaction_t t;
        memset(&t, 0, sizeof(t));
        t.length = chunk * 8;
        t.tx_buffer = dat;
        spi_device_polling_transmit(spiHandle, &t);
        dat += chunk;
        len -= chunk;
    }
}

void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    x1 += LCD_X_OFFSET;
    x2 += LCD_X_OFFSET;
    y1 += LCD_Y_OFFSET;
    y2 += LCD_Y_OFFSET;

    lcd_write_cmd(0x2a); // Column address set
    lcd_write_data32(((uint32_t)x1 << 16) | (uint32_t)x2);

    lcd_write_cmd(0x2b); // Row address set
    lcd_write_data32(((uint32_t)y1 << 16) | (uint32_t)y2);

    lcd_write_cmd(0x2c); // Memory write
}

void lcd_fill(uint32_t color)
{
    lcd_set_window(0, 0, LCD_W - 1, LCD_H - 1);

    uint16_t c = (uint16_t)color;
    uint8_t c_hi = c >> 8;
    uint8_t c_lo = c & 0xFF;

    uint8_t *line_buf = (uint8_t *)heap_caps_malloc(LCD_W * 2, MALLOC_CAP_DMA);
    if (line_buf != NULL)
    {
        for (int i = 0; i < LCD_W; i++)
        {
            line_buf[i * 2] = c_hi;
            line_buf[i * 2 + 1] = c_lo;
        }

        for (int y = 0; y < LCD_H; y++)
        {
            lcd_write_data_batch(line_buf, LCD_W * 2);
        }
        free(line_buf);
    }
    else
    {
        for (int i = 0; i < LCD_W * LCD_H; i++)
        {
            lcd_write_data(c_hi);
            lcd_write_data(c_lo);
        }
    }
}

void lcd_init(void)
{
    ESP_LOGI(TAG, "Configuring LCD GPIOs (RST=%d, DC=%d, CS=%d, BL=%d)...", PIN_TFT_RST, LCD_DC, LCD_CS, LCD_BL);

    // Explicitly reset and configure GPIOs
    gpio_reset_pin(PIN_TFT_RST);
    gpio_set_direction(PIN_TFT_RST, GPIO_MODE_OUTPUT);
    gpio_set_level(PIN_TFT_RST, 1);

    gpio_reset_pin(LCD_DC);
    gpio_set_direction(LCD_DC, GPIO_MODE_OUTPUT);
    gpio_set_level(LCD_DC, 1);

    // Initialize SPI bus
    ESP_LOGI(TAG, "Initializing SPI bus (MOSI=%d, SCLK=%d)...", LCD_MOSI, LCD_SCLK);
    spi_bus_config_t buscfg = {
        .miso_io_num = -1,
        .mosi_io_num = LCD_MOSI,
        .sclk_io_num = LCD_SCLK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = SOC_SPI_MAXIMUM_BUFFER_SIZE,
    };
    spi_bus_initialize(SPI2_HOST, &buscfg, SPI_DMA_CH_AUTO);

    // Initialize SPI device
    spi_device_interface_config_t devcfg = {
        .command_bits = 0,
        .address_bits = 0,
        .dummy_bits = 0,
        .clock_speed_hz = 10 * 1000 * 1000, // 10MHz
        .mode = 0,                          // SPI mode 0
        .spics_io_num = LCD_CS,
        .cs_ena_pretrans = 1,
        .cs_ena_posttrans = 1,
        .queue_size = 7,
        .flags = 0,
    };
    spi_bus_add_device(SPI2_HOST, &devcfg, &spiHandle);

    // 硬件复位
    ESP_LOGI(TAG, "Hardware resetting LCD...");
    gpio_set_level(PIN_TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(PIN_TFT_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(50));
    gpio_set_level(PIN_TFT_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(150));

    // ST7735S 初始化序列
    ESP_LOGI(TAG, "Sending ST7735S initialization sequence...");

    // 1. Software Reset
    lcd_write_cmd(0x01); // SWRESET
    vTaskDelay(pdMS_TO_TICKS(150));

    // 2. Out of Sleep
    lcd_write_cmd(0x11); // SLPOUT
    vTaskDelay(pdMS_TO_TICKS(150));

    // 3. Frame Rate Control (Normal mode)
    lcd_write_cmd(0xB1);
    lcd_write_data(0x01);
    lcd_write_data(0x2C);
    lcd_write_data(0x2D);

    // 4. Frame Rate Control (Idle mode)
    lcd_write_cmd(0xB2);
    lcd_write_data(0x01);
    lcd_write_data(0x2C);
    lcd_write_data(0x2D);

    // 5. Frame Rate Control (Partial mode)
    lcd_write_cmd(0xB3);
    lcd_write_data(0x01);
    lcd_write_data(0x2C);
    lcd_write_data(0x2D);
    lcd_write_data(0x01);
    lcd_write_data(0x2C);
    lcd_write_data(0x2D);

    // 6. Display Inversion Control
    lcd_write_cmd(0xB4);
    lcd_write_data(0x07); // Column inversion

    // 7. Power Control
    lcd_write_cmd(0xC0);
    lcd_write_data(0xA2);
    lcd_write_data(0x02);
    lcd_write_data(0x84);

    lcd_write_cmd(0xC1);
    lcd_write_data(0xC5);

    lcd_write_cmd(0xC2);
    lcd_write_data(0x0A);
    lcd_write_data(0x00);

    lcd_write_cmd(0xC3);
    lcd_write_data(0x8A);
    lcd_write_data(0x2A);

    lcd_write_cmd(0xC4);
    lcd_write_data(0x8A);
    lcd_write_data(0xEE);

    lcd_write_cmd(0xC5); // VCOM
    lcd_write_data(0x0E);

    // 8. Inversion OFF
    lcd_write_cmd(0x20);

    // 9. Color Mode (16-bit RGB565)
    lcd_write_cmd(0x3A);
    lcd_write_data(0x05);

    // 10. Memory Access Control (MADCTL)
    // Flipped landscape: LCD_MADCTL
    lcd_write_cmd(0x36);
    lcd_write_data(LCD_MADCTL);

    // 11. Gamma Sequence
    lcd_write_cmd(0xE0);
    lcd_write_data(0x02);
    lcd_write_data(0x1C);
    lcd_write_data(0x07);
    lcd_write_data(0x12);
    lcd_write_data(0x37);
    lcd_write_data(0x32);
    lcd_write_data(0x29);
    lcd_write_data(0x2D);
    lcd_write_data(0x29);
    lcd_write_data(0x25);
    lcd_write_data(0x2B);
    lcd_write_data(0x39);
    lcd_write_data(0x00);
    lcd_write_data(0x01);
    lcd_write_data(0x03);
    lcd_write_data(0x10);

    lcd_write_cmd(0xE1);
    lcd_write_data(0x03);
    lcd_write_data(0x1D);
    lcd_write_data(0x07);
    lcd_write_data(0x06);
    lcd_write_data(0x2E);
    lcd_write_data(0x2C);
    lcd_write_data(0x29);
    lcd_write_data(0x2D);
    lcd_write_data(0x2E);
    lcd_write_data(0x2E);
    lcd_write_data(0x37);
    lcd_write_data(0x3F);
    lcd_write_data(0x00);
    lcd_write_data(0x00);
    lcd_write_data(0x02);
    lcd_write_data(0x10);

    // 12. Normal Display Mode On
    lcd_write_cmd(0x13); // NORON
    vTaskDelay(pdMS_TO_TICKS(10));

    // 13. Display On
    lcd_write_cmd(0x29); // DISPON
    vTaskDelay(pdMS_TO_TICKS(100));

    // Clear display to pure black before enabling backlight
    lcd_fill(0x0000); // Black

    // Enable backlight smoothly
    lcd_setBrightness(80);
    ESP_LOGI(TAG, "LCD initialized without color test flash.");
}

void lcd_disp(bool en)
{
    if (en)
    {
        lcd_write_cmd(0x11);
        vTaskDelay(pdMS_TO_TICKS(120));
        lcd_write_cmd(0x29);
    }
    else
    {
        lcd_write_cmd(0x28);
        lcd_write_cmd(0x10);
    }
}

// 0~100
void lcd_setBrightness(uint8_t brightness)
{
    uint32_t duty = (pow(2, LEDC_DUTY_RES) - 1) * brightness / 100;
    if (duty == blDuty)
        return;

    ESP_LOGI(TAG, "Brightness %d%%", brightness);

    ledc_set_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL, duty);
    ledc_update_duty(LEDC_LOW_SPEED_MODE, LEDC_CHANNEL);

    blDuty = duty;
}
