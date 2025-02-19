#ifndef __LEDDRV_H__
#define __LEDDRV_H__

#include "CH58x_common.h"

#define LED_COLS 44
#define LED_ROWS 11

#define BRIGHTNESS_LEVELS   (4)

void led_init();
void leds_releaseall();
void led_write2dcol(int dcol, uint16_t col1_val, uint16_t col2_val);
void led_write2row_raw(int row, int which_half, uint32_t val);

#endif /* __LEDDRV_H__ */
