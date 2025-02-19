#include "CH58x_common.h"
#include "CH58x_sys.h"
#include "CH58xBLE_LIB.h"

#include "leddrv.h"
#include "button.h"
#include "bmlist.h"
#include "resource.h"
#include "animation.h"
#include "font.h"

#include "power.h"
#include "data.h"
#include "config.h"
#include "debug.h"

#include "ble/setup.h"
#include "ble/profile.h"

#include "usb/usb.h"
#include "legacyctrl.h"

#define NEXT_STATE(v, min, max) \
				(v)++; \
				if ((v) >= (max)) \
					(v) = (min)

enum MODES {
	BOOT = 0,
	NORMAL,
	DOWNLOAD,
	POWER_OFF,
	MODES_COUNT,
};

#define ANI_BASE_SPEED_T      (200000) // uS
#define ANI_MARQUE_SPEED_T    (100000) // uS
#define ANI_FLASH_SPEED_T     (500000) // uS
#define SCAN_BOOTLD_BTN_SPEED_T         (200000) // uS
#define ANI_SPEED_STRATEGY(speed_level) \
				(ANI_BASE_SPEED_T - ((speed_level) \
				* ANI_BASE_SPEED_T / 8))

#define ANI_NEXT_STEP       (1 << 0)
#define ANI_MARQUE          (1 << 1)
#define ANI_FLASH           (1 << 2)
#define SCAN_BOOTLD_BTN     (1 << 3)
#define BLE_NEXT_STEP       (1 << 4)

static tmosTaskID common_taskid = INVALID_TASK_ID ;

volatile uint16_t fb[LED_COLS] = {0};
volatile int mode, is_play_sequentially = 1;

__HIGH_CODE
static void change_brightness()
{
	NEXT_STATE(badge_cfg.led_brightness, 0, BRIGHTNESS_LEVELS);
}

static void mode_setup_download();
static void mode_setup_normal();

__HIGH_CODE
static void change_mode()
{
	NEXT_STATE(mode, 0, MODES_COUNT);
	const static void (*modes[])(void) = {
		NULL,
		mode_setup_normal,
		mode_setup_download,
		poweroff
	};

	if (modes[mode])
		modes[mode]();
}

__HIGH_CODE
static void bm_transition()
{
	if (is_play_sequentially) {
		is_play_sequentially = 0;
		bmlist_gohead();
		return;
	}

	bmlist_gonext();
	if (bmlist_current() == bmlist_head()) {
		is_play_sequentially = 1;
		return;
	}
}
void play_splash(xbm_t *xbm, int col, int row, int spT)
{
	while (ani_xbm_scrollup_pad(xbm, 11, 11, 11, fb, 0, 0) != 0) {
		DelayMs(spT);
	}
}

void load_bmlist()
{
	data_legacy_t header;
	data_get_header(&header);
	if (memcmp(header.header, "wang", 5))
		return; // There is no bitmap stored in flash

	bm_t *curr_bm = bmlist_current();

	for (int i=0; i<8; i++) {
		bm_t *bm = flash2newbm(i);
		if (bm == NULL)
			continue;
		bmlist_append(bm);
	}
	bmlist_gonext();

	bmlist_drop(curr_bm);
}

static uint16_t common_tasks(tmosTaskID task_id, uint16_t events)
{
	static int marque_step, flash_step;

	if(events & SYS_EVENT_MSG) {
		uint8 *pMsg = tmos_msg_receive(common_taskid);
		if(pMsg != NULL)
		{
			tmos_msg_deallocate(pMsg);
		}
		return (events ^ SYS_EVENT_MSG);
	}

	if(events & ANI_NEXT_STEP) {

		static int (*animations[])(bm_t *bm, uint16_t *fb) = {
			ani_scroll_left,
			ani_scroll_right,
			ani_scroll_up,
			ani_scroll_down,
			ani_fixed,
			ani_animation,
			ani_snowflake,
			ani_picture,
			ani_laser
		};

		bm_t *bm = bmlist_current();
		if (animations[LEGACY_GET_ANIMATION(bm->modes)])
			if (animations[LEGACY_GET_ANIMATION(bm->modes)](bm, fb) == 0
				&& is_play_sequentially) {
				bmlist_gonext();
			}

		if (bm->is_flash) {
			ani_flash(bm, fb, flash_step);
		}
		if (bm->is_marquee) {
			ani_marque(bm, fb, marque_step);
		}

		uint32_t t = ANI_SPEED_STRATEGY(LEGACY_GET_SPEED(bm->modes));
		tmos_start_task(common_taskid, ANI_NEXT_STEP, t / 625);

		return events ^ ANI_NEXT_STEP;
	}

	if (events & ANI_MARQUE) {
		bm_t *bm = bmlist_current();
		marque_step++;
		if (bm->is_marquee) {
			ani_marque(bm, fb, marque_step);
		}

		return events ^ ANI_MARQUE;
	}

	if (events & SCAN_BOOTLD_BTN) {
		static uint32_t hold;
		hold = isPressed(KEY2) ? hold + 1 : 0;
		if (hold > 10) {
			reset_jump();
		}

		return events ^ SCAN_BOOTLD_BTN;
	}

	if (events & ANI_FLASH) {
		bm_t *bm = bmlist_current();
		flash_step++;

		if (bm->is_flash) {
			ani_flash(bm, fb, flash_step);
		}
		/* After flash is applied, it will potentialy overwrite the marque
		effect after it just wrote, results in flickering. So here apply the
		marque effect again */
		if (bm->is_marquee) {
			ani_marque(bm, fb, marque_step);
		}

		return events ^ ANI_FLASH;
	}

	if (events & BLE_NEXT_STEP) {
		ani_xbm_next_frame(&bluetooth, fb, 10, 0);

		return events ^ BLE_NEXT_STEP;
	}

	return 0;
}

void ble_setup()
{
	ble_hardwareInit();
	tmos_clockInit();

	peripheral_init();

	if (! badge_cfg.ble_always_on) {
		ble_disable_advertise();
	}

	devInfo_registerService();
	legacy_registerService();
	batt_registerService();
	ng_registerService();
}

static void spawn_tasks()
{
	common_taskid = TMOS_ProcessEventRegister(common_tasks);

	tmos_start_reload_task(common_taskid, ANI_MARQUE, ANI_MARQUE_SPEED_T / 625);
	tmos_start_reload_task(common_taskid, ANI_FLASH, ANI_FLASH_SPEED_T / 625);
	tmos_start_reload_task(common_taskid, SCAN_BOOTLD_BTN,
				SCAN_BOOTLD_BTN_SPEED_T / 625);
	tmos_start_task(common_taskid, ANI_NEXT_STEP, 500000 / 625);
}

static void start_ble_animation()
{
	tmos_stop_task(common_taskid, ANI_NEXT_STEP);
	tmos_stop_task(common_taskid, ANI_MARQUE);
	tmos_stop_task(common_taskid, ANI_FLASH);
	memset(fb, 0, sizeof(fb));

	tmos_start_reload_task(common_taskid, BLE_NEXT_STEP, 500000 / 625);
}

static void start_normal_animation()
{
	tmos_start_reload_task(common_taskid, ANI_MARQUE, ANI_MARQUE_SPEED_T / 625);
	tmos_start_reload_task(common_taskid, ANI_FLASH, ANI_FLASH_SPEED_T / 625);
	tmos_start_task(common_taskid, ANI_NEXT_STEP, 500000 / 625);
	tmos_stop_task(common_taskid, BLE_NEXT_STEP);
}

static void resume_from_streaming()
{
	if (badge_cfg.ble_always_on) {
		start_normal_animation();
	} else {
		start_ble_animation();
	}
}

static void stop_all_animation()
{
	tmos_stop_task(common_taskid, ANI_NEXT_STEP);
	tmos_stop_task(common_taskid, ANI_MARQUE);
	tmos_stop_task(common_taskid, ANI_FLASH);
	tmos_stop_task(common_taskid, BLE_NEXT_STEP);
	memset(fb, 0, sizeof(fb));
}

int streaming_enabled;

uint8_t streaming_setting(uint8_t *params, uint16_t len)
{
	if (params[0] == 0x00) { // enter streaming mode
		stop_all_animation();
		streaming_enabled = 1;
	} else if (params[0] == 0x01) { // return to normal mode
		resume_from_streaming();
		streaming_enabled = 0;
	}
	return 0;
}

uint8_t stream_bitmap(uint8_t *params, uint16_t len)
{
	if (! streaming_enabled) {
		return -1;
	}

	tmos_memcpy(fb, params, min(LED_COLS, len));
	return 0;
}

static void debug_init()
{
	GPIOA_SetBits(GPIO_Pin_9);
	GPIOA_ModeCfg(GPIO_Pin_8, GPIO_ModeIN_PU);
	GPIOA_ModeCfg(GPIO_Pin_9, GPIO_ModeOut_PP_5mA);
	UART1_DefInit();
	UART1_BaudRateCfg(921600);
}

static void disp_bat_stt(int bat_percent, int col, int row)
{
	if (bat_percent < 0) {
		xbm2fb(&batwarn_xbm, fb, col, row);
		return;
	}

	xbm2fb(&bat_xbm, fb, col, row);
	bat_percent /= 10;
	for (int i=1; i <= bat_percent; i++) {
		fb[col + i] = fb[col];
	}
}

static void fb_putchar(char c, int col, int row)
{
	for (int i=0; i < 6; i++) {
		if (col + i >= LED_COLS) break;
		fb[col + i] = (fb[col + i] & ~(0x7f << row))
				| (font5x7[c - ' '][i] << row);
	}
}

static void fb_puts(char *s, int len, int col, int row)
{
	while (*s && len--) {
		fb_putchar(*s, col, row);
		col += 6;
		s++;
	}
}

static void disp_charging()
{
	int blink = 0;
	while (mode == BOOT) {
		int percent = batt_raw2percent(batt_raw());

		if (charging_status()) {
			disp_bat_stt(blink ? percent : 0, 2, 2);
			if (ani_xbm_next_frame(&fabm_xbm, fb, 16, 0) == 0) {
				fb_puts(VERSION_ABBR, sizeof(VERSION_ABBR), 16, 2);
				fb_putchar(' ', 40, 2);
			}
			blink = !blink;
			DelayMs(500);
		} else {
			disp_bat_stt(percent, 7, 2);
			DelayMs(500);
			return;
		}
	}
}

static void mode_setup_download()
{
	// If always-on BLE is enabled, then skip this mode, jump to next mode
	if (badge_cfg.ble_always_on) {
		change_mode();
	}

	// Disable bitmap transition while in download mode
	btn_onOnePress(KEY2, NULL);

	// Take control of the current bitmap to display
	// the Bluetooth animation
	ble_enable_advertise();
	start_ble_animation();
}

void clean_bmlist()
{
	bm_t *curr_bm = bmlist_current();
	while (curr_bm->next != curr_bm)
		bmlist_drop(curr_bm->next);
}

void reload_bmlist()
{
	clean_bmlist();
	load_bmlist();
}

static void mode_setup_normal()
{
	btn_onOnePress(KEY2, bm_transition);
	reload_bmlist();
	start_normal_animation();
}

void handle_after_rx()
{
	if (badge_cfg.reset_rx) {
		SYS_ResetExecute();
	} else {
		mode_setup_normal();
	}
}

int main()
{
	SetSysClock(CLK_SOURCE_PLL_60MHz);

	debug_init();
	PRINT("\nDebug console is on UART%d\n", DEBUG);

	cdc_onWrite(legacy_usb_rx);
	hiddev_onWrite(legacy_usb_rx);
	usb_start();

	led_init();
	TMR0_TimerInit((FREQ_SYS / 2000) / 2);
	TMR0_ITCfg(ENABLE, TMR0_3_IT_CYC_END);
	PFIC_EnableIRQ(TMR0_IRQn);

	bmlist_init(LED_COLS * 4);

	btn_init();
	btn_onOnePress(KEY1, change_mode);
	btn_onOnePress(KEY2, bm_transition);
	btn_onLongPress(KEY1, change_brightness);

	power_init();
	disp_charging();
	cfg_init();
	xbm_t spl = {
		.bits = &(badge_cfg.splash_bm_bits),
		.w = badge_cfg.splash_bm_w,
		.h = badge_cfg.splash_bm_h,
		.fh = badge_cfg.splash_bm_fh,
	};
	play_splash(&spl, 0, 0, badge_cfg.splash_speedT);

	load_bmlist();

	ble_setup();

	spawn_tasks();

	mode = NORMAL;
	while (1) {
		TMOS_SystemProcess();
	}
}

__INTERRUPT
__HIGH_CODE
void TMR0_IRQHandler(void)
{
	static int i;
	int state;

	if (TMR0_GetITFlag(TMR0_3_IT_CYC_END)) {
		i++;
		state = i&3;

		if (state == 0) {
			if ((i >> 1) >= LED_COLS)
				i = 0;
			led_write2dcol(i >> 2, fb[i >> 1], fb[(i >> 1) + 1]);
		}
		else if (state > (badge_cfg.led_brightness&3))
			leds_releaseall();

		TMR0_ClearITFlag(TMR0_3_IT_CYC_END);
	}
}
