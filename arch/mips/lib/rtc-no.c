/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Stub RTC routines to keep Linux from crashing on machine which don't
 * have a RTC chip.
 *
 * Copyright (C) 1998, 2001 by Ralf Baechle
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mc146818rtc.h>

static unsigned int shouldnt_happen(void)
{
	static int called;

	if (!called) {
		called = 1;
		printk(KERN_DEBUG "RTC functions called - shouldn't happen\n");
	}

	return 0;
}

struct rtc_ops no_rtc_ops = {
    .rtc_read_data  = (void *) &shouldnt_happen,
    .rtc_write_data = (void *) &shouldnt_happen,
    .rtc_bcd_mode   = (void *) &shouldnt_happen
};

EXPORT_SYMBOL(rtc_ops);
