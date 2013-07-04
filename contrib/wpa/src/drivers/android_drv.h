/*
 * Android driver interface
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * Alternatively, this software may be distributed under the terms of BSD
 * license.
 */

#ifndef ANDROID_DRV_H
#define ANDROID_DRV_H

#define WPA_EVENT_DRIVER_STATE "CTRL-EVENT-DRIVER-STATE "

#define MAX_SSID_LEN 32

#define MAX_DRV_CMD_SIZE		248
#define DRV_NUMBER_SEQUENTIAL_ERRORS	4

#define WEXT_PNOSETUP_HEADER		"PNOSETUP "
#define WEXT_PNOSETUP_HEADER_SIZE	9
#define WEXT_PNO_TLV_PREFIX		'S'
#define WEXT_PNO_TLV_VERSION		'1'
#define WEXT_PNO_TLV_SUBVERSION		'2'
#define WEXT_PNO_TLV_RESERVED		'0'
#define WEXT_PNO_VERSION_SIZE		4
#define WEXT_PNO_AMOUNT			16
#define WEXT_PNO_SSID_SECTION		'S'
/* SSID header size is SSID section type above + SSID length */
#define WEXT_PNO_SSID_HEADER_SIZE	2
#define WEXT_PNO_SCAN_INTERVAL_SECTION	'T'
#define WEXT_PNO_SCAN_INTERVAL_LENGTH	2
#define WEXT_PNO_SCAN_INTERVAL		30
/* Scan interval size is scan interval section type + scan interval length
 * above */
#define WEXT_PNO_SCAN_INTERVAL_SIZE	(1 + WEXT_PNO_SCAN_INTERVAL_LENGTH)
#define WEXT_PNO_REPEAT_SECTION		'R'
#define WEXT_PNO_REPEAT_LENGTH		1
#define WEXT_PNO_REPEAT			4
/* Repeat section size is Repeat section type + Repeat value length above */
#define WEXT_PNO_REPEAT_SIZE		(1 + WEXT_PNO_REPEAT_LENGTH)
#define WEXT_PNO_MAX_REPEAT_SECTION	'M'
#define WEXT_PNO_MAX_REPEAT_LENGTH	1
#define WEXT_PNO_MAX_REPEAT		3
/* Max Repeat section size is Max Repeat section type + Max Repeat value length
 * above */
#define WEXT_PNO_MAX_REPEAT_SIZE	(1 + WEXT_PNO_MAX_REPEAT_LENGTH)
/* This corresponds to the size of all sections expect SSIDs */
#define WEXT_PNO_NONSSID_SECTIONS_SIZE \
(WEXT_PNO_SCAN_INTERVAL_SIZE + WEXT_PNO_REPEAT_SIZE + WEXT_PNO_MAX_REPEAT_SIZE)
/* PNO Max command size is total of header, version, ssid and other sections +
 * Null termination */
#define WEXT_PNO_MAX_COMMAND_SIZE \
	(WEXT_PNOSETUP_HEADER_SIZE + WEXT_PNO_VERSION_SIZE \
	 + WEXT_PNO_AMOUNT * (WEXT_PNO_SSID_HEADER_SIZE + MAX_SSID_LEN) \
	 + WEXT_PNO_NONSSID_SECTIONS_SIZE + 1)

#endif /* ANDROID_DRV_H */
