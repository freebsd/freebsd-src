/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * RTC routines for Jazz style attached Dallas chip.
 *
 * Copyright (C) 1998, 2001 by Ralf Baechle
 */
#include <linux/mc146818rtc.h>
#include <asm/io.h>
#include <asm/jazz.h>

static unsigned char jazz_rtc_read_data(unsigned long addr)
{
	outb_p(addr, RTC_PORT(0));
	return *(char *)JAZZ_RTC_BASE;
}

static void jazz_rtc_write_data(unsigned char data, unsigned long addr)
{
	outb_p(addr, RTC_PORT(0));
	*(char *)JAZZ_RTC_BASE = data;
}

static int jazz_rtc_bcd_mode(void)
{
	return 0;
}

struct rtc_ops jazz_rtc_ops = {
	&jazz_rtc_read_data,
	&jazz_rtc_write_data,
	&jazz_rtc_bcd_mode
};
