## Badge Data Format for BLE

### Connection Information

#### OEM Firmware

The pixel data is transferred to the badge via BLE. The OEM firmware advertises
the name 'LSLED' and a 16-bit vendor-specific service/characteristic pair:

- Service-UUID: 0xFEE0 (128-bit equivalent:
  0000fee0-0000-1000-8000-00805f9b34fb)
- Characteristic: 0xFEE1 (128-bit equivalent:
  0000fee1-0000-1000-8000-00805f9b34fb)

The characteristic of the OEM firmware has Read/Write/Notify properties. It
accepts 16 byte long data packets on writing. Its Read/Notify behavior is
currently unknown and needs to be investigated.

#### Open Firmware

This open firmware advertises the service/characteristic pair 0xFEE0/0xFEE1 in
the same way as the OEM firmware and uses the name "LED Badge Magic" which can
be renamed via the [next-gen profile](#next-gen-profile). The characteristic
0xFEE1, marked as the legacy profile, is write-only. Read/Notify functionality
will be supported once its behavior is fully understood.

An additional custom profile has been implemented, enabling more remote features
and configuration over the BLE connection, see the [next-gen
profile](#next-gen-profile) for details.

### Data Format (Legacy)

The badge supports up to 8 bitmaps which can have various features enabled.

| offset    | byte-size | format       | name       | description  |
| --------- | --------- | ------------ | ---------- | ------------ |
| 00h       | 6         | uint8[6]     | header     | "wang\0\0" - magic number to indicate a new data frame begin |
| 06h       | 1         | uint8        | flash      | bitfield describing which of the 8 bitmaps are flashing |
| 07h       | 1         | uint8        | marquee    | bitfield describing which of the 8 bitmaps feature an animated border |
| 08h       | 8         | uint8[8]     | modes      | speed+mode for each of the 8 bitmaps. See below. |
| 10h       | 16        | uint16[8]    | sizes      | count of 8 pixel wide chunks for each bitmap (Big endian) |
| 20h       | 6         | uint8[6]     | padding    | 6 bytes zeroes of padding
| 26h       | 6         | uint8[6]     | timestamp  | one byte each for (year % 256), month, day, hour, minute, second. Purpose unclear.
| 2Ch       | 4         | uint8[4]     | padding    | 4 bytes zeroes of padding
| 30h       | 16        | uint8[16]    | separator  | zeroes to separate header from bitmap data |
| 40h       | 11*N      | uint8[11\*N] | bitmapdata | image data for all bitmaps. Each bitmap consists of a number of 11 byte chunks describing an 8x11 bitmap, each byte describes 8 pixels, starting with the most significant bit. The number of chunks is listed in the "sizes" fields above, N = sum(sizes). |
| 40h+11*N  | ?         | uint8[?]     | padding    | the last 16-byte block of bitmap data is padded with zeroes if necessary. |

Because the characteristic data value is fixed at 16 bytes, it only be able to
receive 16 bytes once each time, so the big fat data frame will be split into
16-byte chunks and will be sent out in sequence. For example, 16 bytes from
offset 00h-0Fh will be sent first, followed by the next 16 bytes from offset
10h-1Fh, and so on. For the last 16-byte of bitmap data, there would be some
cases, where some last bytes were not used, these should be filled with zeros.

#### Modes

The "mode" bytes are a combination of two 4 bit values. The high nibble describes the animation speed in the range from 0 (slowest) to 7 (highest). The low nibble describes the animation to use:

| mode | animation         |
|------|-------------------|
| 0x00 | scroll leftwards  |
| 0x01 | scroll rightwards |
| 0x02 | scroll up         |
| 0x03 | scroll down       |
| 0x04 | fixed             |
| 0x05 | "animation"       |
| 0x06 | "snowflake"       |
| 0x07 | "picture"         |
| 0x08 | "laser"           |

### Next-Gen profile

 16-bit Service/characteristic:

- Service-UUID: 0xF055 (128-bit equivalent:
  0000f055-0000-1000-8000-00805f9b34fb)
- Characteristic: 0xF057 (128-bit equivalent:
  0000f057-0000-1000-8000-00805f9b34fb)
  - Write property: to receive controls and configs
- Characteristic: 0xF056 (128-bit equivalent:
  0000f056-0000-1000-8000-00805f9b34fb)
  - Notify property: to return error codes
  - Read property: to be implemented

#### Protocol

The next-gen characteristic accepts messages of varying lengths (0 to 512
bytes). The first byte represents the function/command code, while the remaining
bytes contain parameters for the corresponding function/command.

![next-gen message format](/assets/ng-protocol.svg)

Supported functions/commands:

- power_setting
- streaming_setting
- stream_bitmap
- ble_setting
- flash_splash_screen
- save_cfg
- load_fallback_cfg
- Miscellaneous configs

The client app should enable notifications for the characteristic to receive the
returned error code (e.g., by using setCharacteristicNotification() on Android).

##### power_setting

Function/Command code: `0x01`.

Parameters:

- Power off: `0x00`.
- Enable resetting after uploading is done: `[0x01, 0x00]`. Note: call
  the [save_cfg](#save_cfg) command to save this config.
- Disable resetting after uploading is done: `[0x01, 0x01]`. Note: call
  the [save_cfg](#save_cfg) command to save this config.
- Power off: `0x02`.

Returns:

- Parameters out of range: `0xff`.
- Success: `0x00`.

##### streaming_setting

Function/Command code: `0x02`.

Parameters:

- Enter streaming mode `0x00`. This command stops all animations, clears the
  screen, and switches the device to streaming mode.
- Leave streaming mode `0x01`. This command resumes all normal operations.

Returns:

- Parameters out of range: `0xff`.
- Success: `0x00`.

##### stream_bitmap

Function/Command code: `0x03`.

Parameters:

- Array of bitmap in word (16-bit). Each word represents a column. The
  least-significant bit of each word represents the top pixel of each column.
  The length of the word array must be less than the number of columns on the
  screen; otherwise, any overflow pixels will be ignored.

Returns:

- Parameters out of range or streaming not enabled: `0xff`.
- Success: `0x00`.

##### ble_setting

Function/Command code: `0x04`.

Parameters:

- Disable always-on BLE: `[0x00, 0x00]`. Note: Call the save_cfg command to save
  this configuration.
- Enable always-on BLE: `[0x00, 0x01]`. Note: Call the save_cfg command to save
  this configuration.
- Change BLE device name: `[0x01, "This is name"]`. The name must be less than
  or equal to 20 characters. Note: Call the save_cfg command to save this
  configuration.

Returns:

- Parameters out of range: `0xff`.
- Success: `0x00`.

##### flash_splash_screen

Function/Command code: `0x05`.

Parameters:

- The first byte describes the width of the image.
- The second byte describes the height of the image.
- The third byte describes the frame height that will be displayed on the
  screen.
- The rest are pixel content in `xbm` format.

Returns:

- Parameters out of range: `0xff`.
- The width is larger than the maximum allowed (currently 48 pixels): `0xff`.
- The height is larger than the maximum allowed (currently 44 pixels): `0xfe`.
- Message length is not matched (currently 44 pixels): `0xfd`.
- Missing pixel contents: `0xfc`.
- Success: `0x00`.

##### save_cfg

Save configs to flash.

Command code: `0x06`.

Returns:

- Parameters out of range: `0xff`.
- Flash writing error: `0x01`.
- Success: `0x00`.

##### load_fallback_cfg

Load firmware default configuration. Note: call the [save_cfg](#save_cfg)
command to save this config.

Function/Command code: `0x07`.

Returns:

- Success: `0x00`.

##### Miscellaneous configs

Function/Command code: `0x08`.

Parameters:

- Adjust splash screen speed: `[0x00, speed_ms]`. `speed_ms` (16-bit) is the delay of each frame in milliseconds and must not be lower than 10 ms.
- Adjust LED brightness: `[0x01, brightness_level]`. `brightness_level` has a value from 0 to 3.

Returns:

- Parameters out of range: `0xff`.
- `speed_ms` or `brightness_level` is out of allowed range: `0x02`.
- Success: `0x00`.
