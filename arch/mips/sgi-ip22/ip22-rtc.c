/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * RTC routines for Indy style attached Dallas chip.
 *
 * Copyright (C) 1998, 2001 by Ralf Baechle
 */
#include <asm/ds1286.h>
#include <asm/sgi/hpc3.h>

static unsigned char ip22_rtc_read_data(unsigned long addr)
{
	return hpc3c0->rtcregs[addr];
}

static void ip22_rtc_write_data(unsigned char data, unsigned long addr)
{
	hpc3c0->rtcregs[addr] = data;
}

static int ip22_rtc_bcd_mode(void)
{
	return 0;
}

struct rtc_ops ip22_rtc_ops = {
	&ip22_rtc_read_data,
	&ip22_rtc_write_data,
	&ip22_rtc_bcd_mode
};
