/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2018 Emmanuel Vadot <manu@FreeBSD.org>
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

#ifndef _RK805REG_H_
#define	 _RK805REG_H_

/* RTC registers */
#define	RK805_RTC_SECS		0x00
#define	 RK805_RTC_SECS_MASK	0x7f
#define	RK805_RTC_MINUTES	0x01
#define	 RK805_RTC_MINUTES_MASK	0x7f
#define	RK805_RTC_HOURS		0x02
#define	 RK805_RTC_HOURS_MASK	0x3f
#define	RK805_RTC_HOURS_PM	0x80
#define	RK805_RTC_DAYS		0x03
#define	 RK805_RTC_DAYS_MASK	0x3f
#define	RK805_RTC_MONTHS	0x04
#define	 RK805_RTC_MONTHS_MASK	0x1f
#define	RK805_RTC_YEARS		0x05
#define	RK805_RTC_WEEKS		0x06 /* day of week */
#define	 RK805_RTC_WEEKS_MASK	0x07
#define	RK805_ALARM_SECONDS	0x8
#define	RK805_ALARM_MINUTES	0x9
#define	RK805_ALARM_HOURS	0xA
#define	RK805_ALARM_DAYS	0xB
#define	RK805_ALARM_MONTHS	0xC
#define	RK805_ALARM_YEARS	0xD
#define	RK805_RTC_CTRL		0x10
#define	 RK805_RTC_CTRL_STOP	(1 << 0)
#define	 RK805_RTC_AMPM_MODE	(1 << 3)
#define	 RK805_RTC_GET_TIME	(1 << 6)
#define	 RK805_RTC_READSEL	(1 << 7)
#define	RK805_CLK32KOUT		0x20

/* Version registers */
#define	RK805_CHIP_NAME		0x17
#define	RK805_CHIP_VER		0x18
#define	RK805_OTP_VER		0x19

/* Power channel enable registers */
#define	RK805_DCDC_EN		0x23
#define	RK805_SLP_DCDC_EN	0x25
#define	RK805_SLP_LDO_EN	0x26
#define	RK805_LDO_EN		0x27
#define	RK805_BUCK_LDO_SLP_LP	0x2A

/* Buck and LDO configuration registers */
#define	RK805_BUCK1_CONFIG	0x2E
#define	RK805_BUCK1_ON_VSEL	0x2F
#define	RK805_BUCK1_SLEEP_VSEL	0x30
#define	RK805_BUCK2_CONFIG	0x32
#define	RK805_BUCK2_ON_VSEL	0x33
#define	RK805_BUCK2_SLEEP_VSEL	0x34
#define	RK805_BUCK3_CONFIG	0x36
#define	RK805_BUCK4_CONFIG	0x37
#define	RK805_BUCK4_ON_VSEL	0x38
#define	RK805_BUCK4_SLEEP_VSEL	0x39
#define	RK805_LDO1_ON_VSEL	0x3B
#define	RK805_LDO1_SLEEP_VSEL	0x3C
#define	RK805_LDO2_ON_VSEL	0x3D
#define	RK805_LDO2_SLEEP_VSEL	0x3E
#define	RK805_LDO3_ON_VSEL	0x3F
#define	RK805_LDO3_SLEEP_VSEL	0x40

#define	RK805_DEV_CTRL		0x4B
#define	 RK805_DEV_CTRL_OFF	(1 << 0)
#define	 RK805_DEV_CTRL_SLP	(1 << 1)

enum rk805_regulator {
	RK805_BUCK1 = 0,
	RK805_BUCK2,
	RK805_BUCK3,
	RK805_BUCK4,
	RK805_LDO1,
	RK805_LDO2,
	RK805_LDO3,
};

#endif /* _RK805REG_H_ */
