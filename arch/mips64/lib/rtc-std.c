/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * RTC routines for PC style attached Dallas chip.
 *
 * Copyright (C) 1998, 2001 by Ralf Baechle
 */
#include <linux/module.h>
#include <linux/mc146818rtc.h>
#include <asm/io.h>

static unsigned char std_rtc_read_data(unsigned long addr)
{
	outb_p(addr, RTC_PORT(0));
	return inb_p(RTC_PORT(1));
}

static void std_rtc_write_data(unsigned char data, unsigned long addr)
{
	outb_p(addr, RTC_PORT(0));
	outb_p(data, RTC_PORT(1));
}

static int std_rtc_bcd_mode(void)
{
	return 1;
}

struct rtc_ops std_rtc_ops = {
	&std_rtc_read_data,
	&std_rtc_write_data,
	&std_rtc_bcd_mode
};

EXPORT_SYMBOL(rtc_ops);
