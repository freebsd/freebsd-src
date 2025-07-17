/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018-2021 Emmanuel Vadot <manu@FreeBSD.org>
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

#ifndef _RK808REG_H_
#define	 _RK808REG_H_

/* RTC registers */
#define	RK808_RTC_SECS		0x00
#define	 RK808_RTC_SECS_MASK	0x7f
#define	RK808_RTC_MINUTES	0x01
#define	 RK808_RTC_MINUTES_MASK	0x7f
#define	RK808_RTC_HOURS		0x02
#define	 RK808_RTC_HOURS_MASK	0x3f
#define	RK808_RTC_DAYS		0x03
#define	 RK808_RTC_DAYS_MASK	0x3f
#define	RK808_RTC_MONTHS	0x04
#define	 RK808_RTC_MONTHS_MASK	0x1f
#define	RK808_RTC_YEARS		0x05
#define	RK808_RTC_WEEKS		0x06 /* day of week */
#define	 RK808_RTC_WEEKS_MASK	0x07
#define	RK808_ALARM_SECONDS	0x8
#define	RK808_ALARM_MINUTES	0x9
#define	RK808_ALARM_HOURS	0xA
#define	RK808_ALARM_DAYS	0xB
#define	RK808_ALARM_MONTHS	0xC
#define	RK808_ALARM_YEARS	0xD
#define	RK808_RTC_CTRL		0x10
#define	 RK808_RTC_CTRL_STOP	(1 << 0)
#define	 RK808_RTC_AMPM_MODE	(1 << 3)
#define	 RK808_RTC_GET_TIME	(1 << 6)
#define	 RK808_RTC_READSEL	(1 << 7)
#define	RK808_RTC_STATUS	0x11
#define	RK808_RTC_INT		0x12
#define	RK808_RTC_COMP_LSB	0x13
#define	RK808_RTC_COMP_MSB	0x14

/* Misc registers*/
#define	RK808_CLK32KOUT		0x20
#define	RK808_VB_MON		0x21
#define	RK808_THERMAL		0x22

/* Power channel control and monitoring registers */
#define	RK808_DCDC_EN		0x23
#define	RK808_LDO_EN		0x24
#define	RK808_SLEEP_SET_OFF_1	0x25
#define	RK808_SLEEP_SET_OFF_2	0x26
#define	RK808_DCDC_UV_STS	0x27
#define	RK808_DCDC_UV_ACT	0x28
#define	RK808_LDO_UV_STS	0x29
#define	RK808_LDO_UV_ACT	0x2A
#define	RK808_DCDC_PG		0x2B
#define	RK808_LDO_PG		0x2C
#define	RK808_VOUT_MON_TDB	0x2D

/* Power channel configuration registers */
#define	RK808_BUCK1_CONFIG	0x2E
#define	RK808_BUCK1_ON_VSEL	0x2F
#define	RK808_BUCK1_SLP_VSEL	0x30
#define	RK808_BUCK2_CONFIG	0x32
#define	RK808_BUCK2_ON_VSEL	0x33
#define	RK808_BUCK2_SLEEP_VSEL	0x34
#define	RK808_BUCK3_CONFIG	0x36
#define	RK808_BUCK4_CONFIG	0x37
#define	RK808_BUCK4_ON_VSEL	0x38
#define	RK808_BUCK4_SLEEP_VSEL	0x39
#define	RK808_DCDC_ILMAX_REG	0x90
#define	RK808_LDO1_ON_VSEL	0x3B
#define	RK808_LDO1_SLEEP_VSEL	0x3C
#define	RK808_LDO2_ON_VSEL	0x3D
#define	RK808_LDO2_SLEEP_VSEL	0x3E
#define	RK808_LDO3_ON_VSEL	0x3F
#define	RK808_LDO3_SLEEP_VSEL	0x40
#define	RK808_LDO4_ON_VSEL	0x41
#define	RK808_LDO4_SLEEP_VSEL	0x42
#define	RK808_LDO5_ON_VSEL	0x43
#define	RK808_LDO5_SLEEP_VSEL	0x44
#define	RK808_LDO6_ON_VSEL	0x45
#define	RK808_LDO6_SLEEP_VSEL	0x46
#define	RK808_LDO7_ON_VSEL	0x47
#define	RK808_LDO7_SLEEP_VSEL	0x48
#define	RK808_LDO8_ON_VSEL	0x49
#define	RK808_LDO8_SLEEP_VSEL	0x4A

#define	RK808_DEV_CTRL		0x4B
#define	 RK808_DEV_CTRL_OFF	(1 << 0)
#define	 RK808_DEV_CTRL_SLP	(1 << 1)

enum rk808_regulator {
	RK808_BUCK1 = 0,
	RK808_BUCK2,
	RK808_BUCK3,
	RK808_BUCK4,
	RK808_LDO1,
	RK808_LDO2,
	RK808_LDO3,
	RK808_LDO4,
	RK808_LDO5,
	RK808_LDO6,
	RK808_LDO7,
	RK808_LDO8,
	RK808_SWITCH1,
	RK808_SWITCH2,
};

#endif /* _RK808REG_H_ */
