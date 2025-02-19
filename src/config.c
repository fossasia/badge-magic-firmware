/**
 * TODO: needs constraints checking as some of the config fields may have
 * invalid values which will make the badge broke.
 */
#include "config.h"
#include "resource.h"
#include "res/foss-asia-2.xbm"
#include "debug.h"
#include "util/crc.h"

#include "ISP583.h"
#include <stdlib.h>

#define CFG_SIZE sizeof(badge_cfg_t)
#define CFG_DEF_FLASH_OFFS (EEPROM_MAX_SIZE - CFG_SIZE - 1) // Default offset

badge_cfg_t badge_cfg;

/* In case of first time firmware upgrading */
void cfg_fallback()
{
	badge_cfg.ble_always_on = 0;
	memcpy(badge_cfg.ble_devname, "LED Badge Magic\0\0\0\0", 20);
	/* OEM app testing: */
	// memcpy(badge_cfg.ble_devname, "LSLED\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 20);

	badge_cfg.led_brightness = 0;
	badge_cfg.led_scan_freq = 2000;

	badge_cfg.splash_speedT = 30; // ms

	int splash_size = ALIGN_1BYTE(splash.w) * splash.h;
	memcpy(badge_cfg.splash_bm_bits, splash.bits, splash_size);
	badge_cfg.splash_bm_w = splash.w;
	badge_cfg.splash_bm_h = splash.h;
	badge_cfg.splash_bm_fh = splash.fh;

	badge_cfg.reset_rx = FALSE;
}

void cfg_update_crc(badge_cfg_t *cfg)
{
	cfg->crc = crc_cal((uint8_t *)cfg, CFG_SIZE - 1);
	PRINT(__func__);
	PRINT(": crc: %02X\n", cfg->crc);
}

int cfg_writeflash(uint16_t flash_offs, badge_cfg_t *cfg)
{
	if (flash_offs + CFG_SIZE > EEPROM_MAX_SIZE) {
		return -1;
	}
	cfg_update_crc(&badge_cfg);
	return EEPROM_WRITE(flash_offs, cfg, CFG_SIZE);
}

int cfg_readflash(uint16_t flash_offs, badge_cfg_t *cfg)
{
	int ret = EEPROM_READ(flash_offs, cfg, CFG_SIZE);
	if (ret)
		return ret;

	return crc_cal((uint8_t *)cfg, CFG_SIZE);
}

int cfg_readflash_def(badge_cfg_t *cfg)
{
	return cfg_readflash(CFG_DEF_FLASH_OFFS, cfg);
}

int cfg_writeflash_def(badge_cfg_t *cfg)
{
	return cfg_writeflash(CFG_DEF_FLASH_OFFS, cfg);
}

void cfg_init()
{
	badge_cfg_t cfg;
	int r = cfg_readflash_def(&cfg);
	if (r) {
		cfg_fallback();
		PRINT("configuration falling back: %02x\n", r);
	} else {
		memcpy(&badge_cfg, &cfg, CFG_SIZE);
		PRINT("configuration read from flash successfully\n");
	}
}