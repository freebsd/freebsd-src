/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Andrew Turner
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

#ifndef _BCM2835_FIRMWARE_H_
#define _BCM2835_FIRMWARE_H_

#define BCM2835_FIRMWARE_TAG_GET_CLOCK_RATE		0x00030002
#define BCM2835_FIRMWARE_TAG_SET_CLOCK_RATE		0x00038002
#define BCM2835_FIRMWARE_TAG_GET_MAX_CLOCK_RATE		0x00030004
#define BCM2835_FIRMWARE_TAG_GET_MIN_CLOCK_RATE		0x00030007

#define BCM2835_FIRMWARE_CLOCK_ID_EMMC			0x00000001
#define BCM2835_FIRMWARE_CLOCK_ID_UART			0x00000002
#define BCM2835_FIRMWARE_CLOCK_ID_ARM			0x00000003
#define BCM2835_FIRMWARE_CLOCK_ID_CORE			0x00000004
#define BCM2835_FIRMWARE_CLOCK_ID_V3D			0x00000005
#define BCM2835_FIRMWARE_CLOCK_ID_H264			0x00000006
#define BCM2835_FIRMWARE_CLOCK_ID_ISP			0x00000007
#define BCM2835_FIRMWARE_CLOCK_ID_SDRAM			0x00000008
#define BCM2835_FIRMWARE_CLOCK_ID_PIXEL			0x00000009
#define BCM2835_FIRMWARE_CLOCK_ID_PWM			0x0000000a
#define BCM2838_FIRMWARE_CLOCK_ID_EMMC2			0x0000000c

union msg_get_clock_rate_body {
	struct {
		uint32_t clock_id;
	} req;
	struct {
		uint32_t clock_id;
		uint32_t rate_hz;
	} resp;
};

union msg_set_clock_rate_body {
	struct {
		uint32_t clock_id;
		uint32_t rate_hz;
	} req;
	struct {
		uint32_t clock_id;
		uint32_t rate_hz;
	} resp;
};

#define BCM2835_FIRMWARE_TAG_GET_VOLTAGE		0x00030003
#define BCM2835_FIRMWARE_TAG_SET_VOLTAGE		0x00038003
#define BCM2835_FIRMWARE_TAG_GET_MAX_VOLTAGE		0x00030005
#define BCM2835_FIRMWARE_TAG_GET_MIN_VOLTAGE		0x00030008

#define BCM2835_FIRMWARE_VOLTAGE_ID_CORE		0x00000001
#define BCM2835_FIRMWARE_VOLTAGE_ID_SDRAM_C		0x00000002
#define BCM2835_FIRMWARE_VOLTAGE_ID_SDRAM_P		0x00000003
#define BCM2835_FIRMWARE_VOLTAGE_ID_SDRAM_I		0x00000004

union msg_get_voltage_body {
	struct {
		uint32_t voltage_id;
	} req;
	struct {
		uint32_t voltage_id;
		uint32_t value;
	} resp;
};

union msg_set_voltage_body {
	struct {
		uint32_t voltage_id;
		uint32_t value;
	} req;
	struct {
		uint32_t voltage_id;
		uint32_t value;
	} resp;
};

#define BCM2835_FIRMWARE_TAG_GET_TEMPERATURE		0x00030006
#define BCM2835_FIRMWARE_TAG_GET_MAX_TEMPERATURE	0x0003000a

union msg_get_temperature_body {
	struct {
		uint32_t temperature_id;
	} req;
	struct {
		uint32_t temperature_id;
		uint32_t value;
	} resp;
};

#define BCM2835_FIRMWARE_TAG_GET_TURBO			0x00030009
#define BCM2835_FIRMWARE_TAG_SET_TURBO			0x00038009

#define BCM2835_FIRMWARE_TURBO_ON			1
#define BCM2835_FIRMWARE_TURBO_OFF			0

union msg_get_turbo_body {
	struct {
		uint32_t id;
	} req;
	struct {
		uint32_t id;
		uint32_t level;
	} resp;
};

union msg_set_turbo_body {
	struct {
		uint32_t id;
		uint32_t level;
	} req;
	struct {
		uint32_t id;
		uint32_t level;
	} resp;
};

#define	BCM2835_FIRMWARE_TAG_GET_GPIO_STATE		0x00030041
#define	BCM2835_FIRMWARE_TAG_SET_GPIO_STATE		0x00038041
#define	BCM2835_FIRMWARE_TAG_GET_GPIO_CONFIG		0x00030043
#define	BCM2835_FIRMWARE_TAG_SET_GPIO_CONFIG		0x00038043

#define	BCM2835_FIRMWARE_GPIO_IN			0
#define	BCM2835_FIRMWARE_GPIO_OUT			1

union msg_get_gpio_state {
	struct {
		uint32_t gpio;
	} req;
	struct {
		uint32_t gpio;
		uint32_t state;
	} resp;
};

union msg_set_gpio_state {
	struct {
		uint32_t gpio;
		uint32_t state;
	} req;
	struct {
		uint32_t gpio;
	} resp;
};

union msg_get_gpio_config {
	struct {
		uint32_t gpio;
	} req;
	struct {
		uint32_t gpio;
		uint32_t dir;
		uint32_t pol;
		uint32_t term_en;
		uint32_t term_pull_up;
	} resp;
};

union msg_set_gpio_config {
	struct {
		uint32_t gpio;
		uint32_t dir;
		uint32_t pol;
		uint32_t term_en;
		uint32_t term_pull_up;
		uint32_t state;
	} req;
	struct {
		uint32_t gpio;
	} resp;
};

int bcm2835_firmware_property(device_t, uint32_t, void *, size_t);

#endif
