/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * RTC routines for PC style attached Dallas chip.
 *
 * Copyright (C) 1998, 2001 by Ralf Baechle
 */
#include <linux/mc146818rtc.h>
#include <asm/io.h>
#include <asm/au1000.h>

#define PB1500_RTC_ADDR 0xAC000000

unsigned char std_rtc_read_data(unsigned long offset)
{
	offset <<= 2;
	return (u8)(au_readl(offset + PB1500_RTC_ADDR) & 0xff);
}

static void std_rtc_write_data(unsigned char data, unsigned long offset)
{
	offset <<= 2;
	au_writel(data, offset + PB1500_RTC_ADDR);
}

static int std_rtc_bcd_mode(void)
{
	return 1;
}

struct rtc_ops pb1500_rtc_ops = {
	&std_rtc_read_data,
	&std_rtc_write_data,
	&std_rtc_bcd_mode
};
