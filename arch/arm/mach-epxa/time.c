/*
 *  linux/arch/arm/mach-epxa10db/time.c
 *
 *  Copyright (C) 2000 Deep Blue Solutions
 *  Copyright (C) 2001 Altera Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/init.h>

#include <asm/hardware.h>



extern int (*set_rtc)(void);

static int epxa10db_set_rtc(void)
{
	return 1;
}

static int epxa10db_rtc_init(void)
{
	set_rtc = epxa10db_set_rtc;

	return 0;
}

__initcall(epxa10db_rtc_init);
