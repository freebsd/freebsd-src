/*-
 * Copyright (C) 2013-2014 Daisuke Aoyama <aoyama@peach.ne.jp>
 * All rights reserved.
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
 *
 * $FreeBSD$
 */

#ifndef _BCM2835_MBOX_PROP_H_
#define _BCM2835_MBOX_PROP_H_

#include <sys/cdefs.h>
#include <sys/types.h>

/*
 * Mailbox property interface:
 * https://github.com/raspberrypi/firmware/wiki/Mailbox-property-interface
 */
#define BCM2835_MBOX_CODE_REQ			0
#define BCM2835_MBOX_CODE_RESP_SUCCESS		0x80000000
#define BCM2835_MBOX_CODE_RESP_ERROR		0x80000001
#define BCM2835_MBOX_TAG_VAL_LEN_RESPONSE	0x80000000

struct bcm2835_mbox_hdr {
	uint32_t	buf_size;
	uint32_t	code;
};

struct bcm2835_mbox_tag_hdr {
	uint32_t	tag;
	uint32_t	val_buf_size;
	uint32_t	val_len;
};

#define BCM2835_MBOX_POWER_ID_EMMC		0x00000000
#define BCM2835_MBOX_POWER_ID_UART0		0x00000001
#define BCM2835_MBOX_POWER_ID_UART1		0x00000002
#define BCM2835_MBOX_POWER_ID_USB_HCD		0x00000003
#define BCM2835_MBOX_POWER_ID_I2C0		0x00000004
#define BCM2835_MBOX_POWER_ID_I2C1		0x00000005
#define BCM2835_MBOX_POWER_ID_I2C2		0x00000006
#define BCM2835_MBOX_POWER_ID_SPI		0x00000007
#define BCM2835_MBOX_POWER_ID_CCP2TX		0x00000008

#define BCM2835_MBOX_POWER_ON			(1 << 0)
#define BCM2835_MBOX_POWER_WAIT			(1 << 1)

#define BCM2835_MBOX_TAG_GET_POWER_STATE	0x00020001
#define BCM2835_MBOX_TAG_SET_POWER_STATE	0x00028001

struct msg_get_power_state {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t device_id;
		} req;
		struct {
			uint32_t device_id;
			uint32_t state;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_set_power_state {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t device_id;
			uint32_t state;
		} req;
		struct {
			uint32_t device_id;
			uint32_t state;
		} resp;
	} body;
	uint32_t end_tag;
};

/* Sets the power state for a given device */
int bcm2835_mbox_set_power_state(device_t, uint32_t, boolean_t);

#define BCM2835_MBOX_CLOCK_ID_EMMC		0x00000001
#define BCM2835_MBOX_CLOCK_ID_UART		0x00000002
#define BCM2835_MBOX_CLOCK_ID_ARM		0x00000003
#define BCM2835_MBOX_CLOCK_ID_CORE		0x00000004
#define BCM2835_MBOX_CLOCK_ID_V3D		0x00000005
#define BCM2835_MBOX_CLOCK_ID_H264		0x00000006
#define BCM2835_MBOX_CLOCK_ID_ISP		0x00000007
#define BCM2835_MBOX_CLOCK_ID_SDRAM		0x00000008
#define BCM2835_MBOX_CLOCK_ID_PIXEL		0x00000009
#define BCM2835_MBOX_CLOCK_ID_PWM		0x0000000a

#define BCM2835_MBOX_TAG_GET_CLOCK_RATE		0x00030002
#define BCM2835_MBOX_TAG_SET_CLOCK_RATE		0x00038002
#define BCM2835_MBOX_TAG_GET_MAX_CLOCK_RATE	0x00030004
#define BCM2835_MBOX_TAG_GET_MIN_CLOCK_RATE	0x00030007

struct msg_get_clock_rate {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t clock_id;
		} req;
		struct {
			uint32_t clock_id;
			uint32_t rate_hz;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_set_clock_rate {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t clock_id;
			uint32_t rate_hz;
		} req;
		struct {
			uint32_t clock_id;
			uint32_t rate_hz;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_get_max_clock_rate {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t clock_id;
		} req;
		struct {
			uint32_t clock_id;
			uint32_t rate_hz;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_get_min_clock_rate {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t clock_id;
		} req;
		struct {
			uint32_t clock_id;
			uint32_t rate_hz;
		} resp;
	} body;
	uint32_t end_tag;
};

#define BCM2835_MBOX_TURBO_ON			1
#define BCM2835_MBOX_TURBO_OFF			0

#define BCM2835_MBOX_TAG_GET_TURBO		0x00030009
#define BCM2835_MBOX_TAG_SET_TURBO		0x00038009

struct msg_get_turbo {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t id;
		} req;
		struct {
			uint32_t id;
			uint32_t level;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_set_turbo {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t id;
			uint32_t level;
		} req;
		struct {
			uint32_t id;
			uint32_t level;
		} resp;
	} body;
	uint32_t end_tag;
};

#define BCM2835_MBOX_VOLTAGE_ID_CORE		0x00000001
#define BCM2835_MBOX_VOLTAGE_ID_SDRAM_C		0x00000002
#define BCM2835_MBOX_VOLTAGE_ID_SDRAM_P		0x00000003
#define BCM2835_MBOX_VOLTAGE_ID_SDRAM_I		0x00000004

#define BCM2835_MBOX_TAG_GET_VOLTAGE		0x00030003
#define BCM2835_MBOX_TAG_SET_VOLTAGE		0x00038003
#define BCM2835_MBOX_TAG_GET_MAX_VOLTAGE	0x00030005
#define BCM2835_MBOX_TAG_GET_MIN_VOLTAGE	0x00030008

struct msg_get_voltage {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t voltage_id;
		} req;
		struct {
			uint32_t voltage_id;
			uint32_t value;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_set_voltage {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t voltage_id;
			uint32_t value;
		} req;
		struct {
			uint32_t voltage_id;
			uint32_t value;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_get_max_voltage {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t voltage_id;
		} req;
		struct {
			uint32_t voltage_id;
			uint32_t value;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_get_min_voltage {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t voltage_id;
		} req;
		struct {
			uint32_t voltage_id;
			uint32_t value;
		} resp;
	} body;
	uint32_t end_tag;
};

#define BCM2835_MBOX_TAG_GET_TEMPERATURE	0x00030006
#define BCM2835_MBOX_TAG_GET_MAX_TEMPERATURE	0x0003000a

struct msg_get_temperature {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t temperature_id;
		} req;
		struct {
			uint32_t temperature_id;
			uint32_t value;
		} resp;
	} body;
	uint32_t end_tag;
};

struct msg_get_max_temperature {
	struct bcm2835_mbox_hdr hdr;
	struct bcm2835_mbox_tag_hdr tag_hdr;
	union {
		struct {
			uint32_t temperature_id;
		} req;
		struct {
			uint32_t temperature_id;
			uint32_t value;
		} resp;
	} body;
	uint32_t end_tag;
};

#endif /* _BCM2835_MBOX_PROP_H_ */
