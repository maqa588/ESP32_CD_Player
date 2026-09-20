#ifndef __I2S_H_
#define __I2S_H_

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "freertos/FreeRTOS.h"

#define I2S_BUF_NUM 5
#define I2S_TX_BUFFER_SIZE_FRAME (8)
#define I2S_TX_BUFFER_LEN (2352 * I2S_TX_BUFFER_SIZE_FRAME)

extern volatile bool i2s_bufsFull;

void i2s_init(void);

// Compatible with existing CD Player interface
void i2s_fillBuffer(uint8_t *dat);

// Generic streaming audio interface for both CD and AirPlay
int audio_output_write(const void *samples, size_t bytes, TickType_t wait_ticks);
void audio_output_flush(void);
void audio_output_set_volume(uint8_t volume_0_60);
uint8_t audio_output_get_volume(void);

#endif
