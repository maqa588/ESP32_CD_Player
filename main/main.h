#ifndef __MAIN_H_
#define __MAIN_H_

// Buttons
#define PIN_BTN_EJECT 41
#define PIN_BTN_PLAY 40
#define PIN_BTN_LIGHT 39
#define PIN_BTN_VOL_UP 38
#define PIN_BTN_VOL_DOWN 9
#define PIN_BTN_NEXT 10
#define PIN_BTN_PREVIOUS 11

// Audio DAC (MS4344)
#define PIN_I2S_DAT 4
#define PIN_I2S_BCK 5
#define PIN_I2S_LRCK 6
#define PIN_I2S_MCLK 7

// Display (ST7789)
#define PIN_TFT_BL 12
#define PIN_SPI_CS 13
#define PIN_TFT_DCX 14
#define PIN_TFT_RST 21
#define PIN_SPI_MOSI 47
#define PIN_SPI_CLK 48

// I2C (optional)
#define PIN_I2C_SDA -1
#define PIN_I2C_SCL -1

#define LEDC_CHANNEL LEDC_CHANNEL_0
#define LEDC_DUTY_RES LEDC_TIMER_12_BIT

typedef struct
{
    int16_t l;
    int16_t r;
} ChannelValue_t;

extern QueueHandle_t queue_meter;
extern QueueHandle_t queue_oscilloscope;

void task_oled(void *args);
void task_lvgl(void *args);

#endif