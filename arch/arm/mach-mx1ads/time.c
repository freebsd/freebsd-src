/*
 *  linux/arch/arm/mach-mx1ads/time.c
 *
 *  Copyright (C) 2000-2001 Deep Blue Solutions
 *  Copyright (C) 2002 Shane Nay (shane@minirl.com)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/hardware.h>
#include <asm/io.h>


extern int (*set_rtc)(void);

/* FIXME-
 * When we have an external RTC part,
 * put the mapping in for that part.
 *
 * The internal RTC within the MX1 is not sufficient
 * for tracking time other than time of day, or
 * date over very short periods of time.
 *
 */

static int mx1ads_set_rtc(void)
{
	return 0;
}

static int mx1ads_rtc_init(void)
{
	xtime.tv_sec = 0;

	set_rtc = mx1ads_set_rtc;

	return 0;
}

__initcall(mx1ads_rtc_init);
