/*
 * Copyright (c) 2026 Ahmad Khalifa <vexeduxr@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#ifndef R23B_FW_CMD_H
#define R23B_FW_CMD_H

/*
 * Host to firmware commands.
 */
#define R23B_CMD_RSSI_SETTING	0x42
#define R23B_CMD_PS_TDMA	0x60
#define R23B_CMD_BT_INFO	0x61
#define R23B_CMD_BT_WLANACT	0x63
#define R23B_CMD_ANT_INV	0x65
#define R23B_CMD_BT_MP_OPER	0x67
#define R23B_CMD_WLAN_CALIB	0x6d
#define R23B_CMD_BT_GRANT	0x6e

#define R23B_CMD_BT_WLANACT_IGNORE	((uint8_t[]){ 0 })

#define R23B_CMD_WLAN_CALIB_BEGIN	((uint8_t[]){ 1 })
#define R23B_CMD_WLAN_CALIB_END		((uint8_t[]){ 0 })

#define R23B_CMD_BT_GRANT_LOW		((uint8_t[]){ 0 })
#define R23B_CMD_BT_GRANT_HIGH		((uint8_t[]){ 1 })

/* Structure for R23B_CMD_RSSI_SETTING. */
struct r23b_fw_cmd_rssi {
	uint8_t	macid;
	uint8_t	reserved;
	uint8_t	pwdb;
	uint8_t uldl;
} __packed;

/* Structure for R23B_CMD_BT_MP_OPER. */
struct r23b_fw_cmd_btoper {
	uint8_t req_num;
	uint8_t opcode;
#define R23B_BT_MP_OPER_WRITE_VAL	0xd
#define R23B_BT_MP_OPER_WRITE_ADDR	0xc

	uint8_t value;
	uint8_t addr;
} __packed;

/* Structure for R23B_CMD_ANT_INV. */
struct r23b_fw_cmd_antinv {
	uint8_t invert;
	uint8_t internal_switch;
} __packed;

#endif
