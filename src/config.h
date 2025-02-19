#ifndef __CONFIG_H__
#define __CONFIG_H__

#include <stdint.h>

#include "xbm.h"
#include "leddrv.h"

#define SPLASH_MIN_SPEED_T (10) // ms

#define SPLASH_MAX_WIDTH (48) // pixels
#define SPLASH_MAX_HEIGHT (44) // pixels
#define SPLASH_MAX_SIZE (ALIGN_1BYTE(SPLASH_MAX_WIDTH) * SPLASH_MAX_HEIGHT)

typedef struct {
	// Turn on Bluetooth while in Normal mode and disable Downloads mode
	uint8_t ble_always_on;
	char ble_devname[20];

	uint8_t led_brightness;
	uint16_t led_scan_freq; // Scan frequency

	uint8_t splash_bm_bits[SPLASH_MAX_SIZE]; // Up to 48x44 (Width x Height)
	uint8_t splash_bm_w;
	uint8_t splash_bm_h;
	uint8_t splash_bm_fh;

	// Speed in period of micro second. The lower value, the higher speed
	uint16_t splash_speedT;
	uint8_t reset_rx; // Reset after bitmap received

	uint8_t crc;
} __attribute__((packed)) badge_cfg_t;

extern badge_cfg_t badge_cfg;

void cfg_init();
int cfg_writeflash(uint16_t flash_offs, badge_cfg_t *cfg);
int cfg_readflash(uint16_t flash_offs, badge_cfg_t *cfg);
int cfg_writeflash_def(badge_cfg_t *cfg);
int cfg_readflash_def(badge_cfg_t *cfg);
void cfg_fallback();

#endif /* __CONFIG_H__ */
