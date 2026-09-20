#ifndef __ST7735_H_
#define __ST7735_H_

#define LCD_W 160
#define LCD_H 128

// MADCTL display orientation:
// 0x68: 180-degree flipped landscape (MY=0, MX=1, MV=1, BGR=1)
// 0xA8: default landscape (MY=1, MX=0, MV=1, BGR=1)
#ifndef LCD_MADCTL
#define LCD_MADCTL 0x68
#endif

// Offsets for ST7735S (RAM 132x162). Adjust offsets here if screen window has shift.
#ifndef LCD_X_OFFSET
#define LCD_X_OFFSET 0
#endif
#ifndef LCD_Y_OFFSET
#define LCD_Y_OFFSET 0
#endif

void lcd_set_window(uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2);
void lcd_write_data_batch(uint8_t *dat, int len);
void lcd_fill(uint32_t color);
void lcd_drawPoint(uint16_t x, uint16_t y, uint32_t color);

void lcd_init();

void lcd_disp(bool en);
void lcd_setBrightness(uint8_t brightness);



#endif