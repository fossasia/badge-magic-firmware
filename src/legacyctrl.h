#ifndef __LEGACYCTRL_H__
#define __LEGACYCTRL_H__

#include <stdint.h>

void handle_after_rx();
int legacy_ble_rx(uint8_t *val, uint16_t len);
int legacy_usb_rx(uint8_t *buf, uint16_t len);

#endif /* __LEGACYCTRL_H__ */
