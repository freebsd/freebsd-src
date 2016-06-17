/*
 *  linux/arch/arm/mach-integrator/time.c
 *
 *  Copyright (C) 2000-2001 Deep Blue Solutions
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

#define RTC_DR		(IO_ADDRESS(INTEGRATOR_RTC_BASE) + 0)
#define RTC_MR		(IO_ADDRESS(INTEGRATOR_RTC_BASE) + 4)
#define RTC_STAT	(IO_ADDRESS(INTEGRATOR_RTC_BASE) + 8)
#define RTC_EOI		(IO_ADDRESS(INTEGRATOR_RTC_BASE) + 8)
#define RTC_LR		(IO_ADDRESS(INTEGRATOR_RTC_BASE) + 12)
#define RTC_CR		(IO_ADDRESS(INTEGRATOR_RTC_BASE) + 16)

#define RTC_CR_MIE	0x00000001

extern int (*set_rtc)(void);

static int integrator_set_rtc(void)
{
	__raw_writel(xtime.tv_sec, RTC_LR);
	return 1;
}

static int integrator_rtc_init(void)
{
	__raw_writel(0, RTC_CR);
	__raw_writel(0, RTC_EOI);

	xtime.tv_sec = __raw_readl(RTC_DR);

	set_rtc = integrator_set_rtc;

	return 0;
}

__initcall(integrator_rtc_init);
