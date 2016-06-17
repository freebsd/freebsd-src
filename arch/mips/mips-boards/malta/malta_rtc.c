/*
 * Carsten Langgaard, carstenl@mips.com
 * Copyright (C) 2000 MIPS Technologies, Inc.  All rights reserved.
 *
 * ########################################################################
 *
 *  This program is free software; you can distribute it and/or modify it
 *  under the terms of the GNU General Public License (Version 2) as
 *  published by the Free Software Foundation.
 *
 *  This program is distributed in the hope it will be useful, but WITHOUT
 *  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 *  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
 *  for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  59 Temple Place - Suite 330, Boston MA 02111-1307, USA.
 *
 * ########################################################################
 *
 * RTC routines for Malta style attached PIIX4 device, which contains a
 * Motorola MC146818A-compatible Real Time Clock.
 *
 */
#include <linux/mc146818rtc.h>
#include <asm/mips-boards/malta.h>

static unsigned char malta_rtc_read_data(unsigned long addr)
{
	outb(addr, MALTA_RTC_ADR_REG);
	return inb(MALTA_RTC_DAT_REG);
}

static void malta_rtc_write_data(unsigned char data, unsigned long addr)
{
	outb(addr, MALTA_RTC_ADR_REG);
	outb(data, MALTA_RTC_DAT_REG);
}

static int malta_rtc_bcd_mode(void)
{
	return 0;
}

struct rtc_ops malta_rtc_ops = {
	&malta_rtc_read_data,
	&malta_rtc_write_data,
	&malta_rtc_bcd_mode
};
