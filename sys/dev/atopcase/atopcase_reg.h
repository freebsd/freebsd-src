/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021-2023 Val Packett <val@packett.cool>
 * Copyright (c) 2023 Vladimir Kondratyev <wulf@FreeBSD.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef _ATOPCASE_REG_H_
#define	_ATOPCASE_REG_H_

#include <sys/types.h>

#define	ATOPCASE_PACKET_SIZE	256
#define	ATOPCASE_DATA_SIZE	246
#define	ATOPCASE_PKT_PER_MSG	2
#define	ATOPCASE_MSG_SIZE	(ATOPCASE_DATA_SIZE * ATOPCASE_PKT_PER_MSG)

/* Read == device-initiated, Write == host-initiated or reply to that */
#define	ATOPCASE_DIR_READ		0x20
#define	ATOPCASE_DIR_WRITE		0x40
#define	ATOPCASE_DIR_NOTHING		0x80

#define	ATOPCASE_DEV_MGMT		0x00
#define	ATOPCASE_DEV_KBRD		0x01
#define	ATOPCASE_DEV_TPAD		0x02
#define	ATOPCASE_DEV_INFO		0xD0

#define	ATOPCASE_BKL_REPORT_ID		0xB0

#define	ATOPCASE_INFO_DEVICE		0x01
#define	ATOPCASE_INFO_IFACE		0x02
#define	ATOPCASE_INFO_DESCRIPTOR	0x10

#define	ATOPCASE_MSG_TYPE_SET_REPORT(dev,rid)	((rid << 8) | 0x50 | dev)
#define	ATOPCASE_MSG_TYPE_REPORT(dev)		((dev << 8) | 0x10)
#define	ATOPCASE_MSG_TYPE_INFO(inf)		((inf << 8) | 0x20)

struct atopcase_bl_payload {
	uint8_t report_id;
	uint8_t device;
	uint16_t level;
	uint16_t status;
} __packed;

struct atopcase_device_info_payload {
	uint16_t unknown[2];
	uint16_t num_devs;
	uint16_t vid;
	uint16_t pid;
	uint16_t ver;
	uint16_t vendor_off;
	uint16_t vendor_len;
	uint16_t product_off;
	uint16_t product_len;
	uint16_t serial_off;
	uint16_t serial_len;
} __packed;

struct atopcase_iface_info_payload {
	uint8_t unknown0;
	uint8_t iface_num;
	uint8_t unknown1[3];
	uint8_t country_code;
	uint16_t max_input_report_len;
	uint16_t max_output_report_len;
	uint16_t max_control_report_len;
	uint16_t name_off;
	uint16_t name_len;
} __packed;

struct atopcase_header {
	uint16_t type;
	uint8_t type_arg; /* means "device" for ATOPCASE_MSG_TYPE_DESCRIPTOR */
	uint8_t seq_no;
	uint16_t resp_len;
	uint16_t len;
} __packed;

struct atopcase_packet {
	uint8_t direction;
	uint8_t device;
	uint16_t offset;
	uint16_t remaining;
	uint16_t length;
	uint8_t data[ATOPCASE_DATA_SIZE];
	uint16_t checksum;
} __packed;

#endif /* _ATOPCASE_REG_H_ */
