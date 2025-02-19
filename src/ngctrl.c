#include "ngctrl.h"

#include "CH58xBLE_LIB.h"
#include "CH583SFR.h"

#include "ble/profile.h"
#include "power.h"
#include "debug.h"
#include "config.h"
#include "leddrv.h"

// TODO: Some of configs can be added, just listing:
// - Remote brighness adjusting
// - Upload bitmap to ram

uint8_t next_packet(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT(": to be implemented\n");
	return 0;
}

static void cfg_reset_rx(uint8_t *state, uint16_t len)
{
	badge_cfg.reset_rx = !!state[0];
}

inline static void __poweroff(uint8_t *val, uint16_t len)
{
	poweroff();
}

inline static void __reset(uint8_t *val, uint16_t len)
{
	SYS_ResetExecute();
}

// Power off, reset after bitmap received.
uint8_t power_setting(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT("\n");

	const void (*ble_lut[])(uint8_t *, uint16_t) = {
		__poweroff,
		cfg_reset_rx,
		__reset,
	};

	uint8_t fn = val[0];
	if (fn >= (sizeof(ble_lut) / sizeof(ble_lut[0])))
		return -1;

	ble_lut[fn](&val[1], len - 1);
	return 0;
}

static void cfg_ble_devname(uint8_t *name, uint16_t len)
{
	tmos_memcpy(badge_cfg.ble_devname, name, len);
}

static void cfg_ble_alwayon(uint8_t *val, uint16_t len)
{
	badge_cfg.ble_always_on = val[0] != 0;
}

/* always-on, adv name
Screen off/on can be archived by entering/leaving streaming mode
 */
uint8_t ble_setting(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT("\n");

	const void (*ble_lut[])(uint8_t *, uint16_t) = {
		cfg_ble_alwayon,
		cfg_ble_devname
	};

	uint8_t fn = val[0];
	if (fn >= (sizeof(ble_lut) / sizeof(ble_lut[0])))
		return -1;

	ble_lut[fn](&val[1], len - 1);

	return 0;
}

uint8_t flash_splash_screen(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT("\n");
	
	uint8_t w = val[0];
	uint8_t h = val[1];
	uint8_t fh = val[2];
	uint8_t sz = len - 3;

	if (w > SPLASH_MAX_WIDTH)
		return -1;
	if (h > SPLASH_MAX_HEIGHT)
		return -2;
	if (sz > SPLASH_MAX_SIZE)
		return -3;
	if (len < 3)
		return -4;

	tmos_memcpy(badge_cfg.splash_bm_bits, &val[3], sz);
	badge_cfg.splash_bm_w = w;
	badge_cfg.splash_bm_h = h;
	badge_cfg.splash_bm_fh = fh;

	return 0;
}

uint8_t save_cfg(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT("\n");
	return (uint8_t)cfg_writeflash_def(&badge_cfg);
}

uint8_t load_fallback_cfg(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT("\n");
	cfg_fallback(&badge_cfg);
	return 0;
}

static uint8_t cfg_splash_speed(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT("\n");

	uint16_t ms = *((uint16_t *)val);
	if (ms < SPLASH_MIN_SPEED_T)
		return -2;

	badge_cfg.splash_speedT = ms;
	return 0;
}

static uint8_t cfg_led_brightness(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT("\n");

	uint8_t lvl = val[0];
	if (lvl >= BRIGHTNESS_LEVELS)
		return -2;

	badge_cfg.led_brightness = lvl;
	return 0;
}

uint8_t misc(uint8_t *val, uint16_t len)
{
	PRINT(__func__);
	PRINT("\n");

	const uint8_t (*misc_cmd[])(uint8_t *, uint16_t) = {
		cfg_splash_speed,
		cfg_led_brightness,
	};

	uint8_t fn = val[0];
	if (fn >= (sizeof(misc_cmd) / sizeof(misc_cmd[0])))
		return -1;

	return misc_cmd[fn](&val[1], len - 1);
}

/* TODO: add a way to read configs */
const uint8_t (*cmd_lut[])(uint8_t *val, uint16_t len) = {
	next_packet, // Unsure if we need this
	power_setting,
	streaming_setting,
	stream_bitmap,
	ble_setting,
	flash_splash_screen,
	save_cfg,
	load_fallback_cfg,
	misc,
};

#define CMD_LUT_LEN (sizeof(cmd_lut) / sizeof(cmd_lut[0]))

uint8_t ng_parse(uint8_t *val, uint16_t len)
{
	uint8_t cmd = val[0];
	PRINT("LUT_LEN: %02x \n", CMD_LUT_LEN);
	if (cmd >= CMD_LUT_LEN) {
		PRINT("invalid command!\n");
		return bleInvalidRange;
	}

	if (cmd_lut[cmd]) {
		PRINT("executing [cmd %02x] \n", cmd);
		uint8_t ret = (*cmd_lut[cmd])(val + 1, len - 1);
		ng_notify(&ret, 1); // response to the client app
	} else {
		PRINT("function is not defined!\n");
	}
	return SUCCESS;
}