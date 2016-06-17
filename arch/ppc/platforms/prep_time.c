/*
 *  arch/ppc/platforms/prep_time.c
 *
 *  Copyright (C) 1991, 1992, 1995  Linus Torvalds
 *
 * Adapted for PowerPC (PReP) by Gary Thomas
 * Modified by Cort Dougan (cort@cs.nmt.edu).
 * Copied and modified from arch/i386/kernel/time.c
 */

#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/param.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/timex.h>
#include <linux/kernel_stat.h>
#include <linux/init.h>

#include <asm/sections.h>
#include <asm/segment.h>
#include <asm/io.h>
#include <asm/processor.h>
#include <asm/machdep.h>
#include <asm/prep_nvram.h>
#include <asm/mk48t59.h>

#include <asm/time.h>

extern spinlock_t rtc_lock;

/*
 * The motorola uses the m48t18 rtc (includes DS1643) whose registers
 * are at a higher end of nvram (1ff8-1fff) than the ibm mc146818
 * rtc (ds1386) which has regs at addr 0-d).  The intel gets
 * past this because the bios emulates the mc146818.
 *
 * Why in the world did they have to use different clocks?
 *
 * Right now things are hacked to check which machine we're on then
 * use the appropriate macro.  This is very very ugly and I should
 * probably have a function that checks which machine we're on then
 * does things correctly transparently or a function pointer which
 * is setup at boot time to use the correct addresses.
 * -- Cort
 */

/*
 * Set the hardware clock. -- Cort
 */
__prep
int mc146818_set_rtc_time(unsigned long nowtime)
{
	unsigned char save_control, save_freq_select;
	struct rtc_time tm;

	spin_lock(&rtc_lock);
	to_tm(nowtime, &tm);

	/* tell the clock it's being set */
	save_control = CMOS_READ(RTC_CONTROL);

	CMOS_WRITE((save_control|RTC_SET), RTC_CONTROL);

	/* stop and reset prescaler */
	save_freq_select = CMOS_READ(RTC_FREQ_SELECT);

	CMOS_WRITE((save_freq_select|RTC_DIV_RESET2), RTC_FREQ_SELECT);

        tm.tm_year = (tm.tm_year - 1900) % 100;
	if (!(save_control & RTC_DM_BINARY) || RTC_ALWAYS_BCD) {
		BIN_TO_BCD(tm.tm_sec);
		BIN_TO_BCD(tm.tm_min);
		BIN_TO_BCD(tm.tm_hour);
		BIN_TO_BCD(tm.tm_mon);
		BIN_TO_BCD(tm.tm_mday);
		BIN_TO_BCD(tm.tm_year);
	}
	CMOS_WRITE(tm.tm_sec,  RTC_SECONDS);
	CMOS_WRITE(tm.tm_min,  RTC_MINUTES);
	CMOS_WRITE(tm.tm_hour, RTC_HOURS);
	CMOS_WRITE(tm.tm_mon,  RTC_MONTH);
	CMOS_WRITE(tm.tm_mday, RTC_DAY_OF_MONTH);
	CMOS_WRITE(tm.tm_year, RTC_YEAR);

	/* The following flags have to be released exactly in this order,
	 * otherwise the DS12887 (popular MC146818A clone with integrated
	 * battery and quartz) will not reset the oscillator and will not
	 * update precisely 500 ms later. You won't find this mentioned in
	 * the Dallas Semiconductor data sheets, but who believes data
	 * sheets anyway ...                           -- Markus Kuhn
	 */
	CMOS_WRITE(save_control,     RTC_CONTROL);
	CMOS_WRITE(save_freq_select, RTC_FREQ_SELECT);
	spin_unlock(&rtc_lock);

	return 0;
}

__prep
unsigned long mc146818_get_rtc_time(void)
{
	unsigned int year, mon, day, hour, min, sec;
	int uip, i;

	/* The Linux interpretation of the CMOS clock register contents:
	 * When the Update-In-Progress (UIP) flag goes from 1 to 0, the
	 * RTC registers show the second which has precisely just started.
	 * Let's hope other operating systems interpret the RTC the same way.
	 */

	/* Since the UIP flag is set for about 2.2 ms and the clock
	 * is typically written with a precision of 1 jiffy, trying
	 * to obtain a precision better than a few milliseconds is
	 * an illusion. Only consistency is interesting, this also
	 * allows to use the routine for /dev/rtc without a potential
	 * 1 second kernel busy loop triggered by any reader of /dev/rtc.
	 */

	for ( i = 0; i<1000000; i++) {
		uip = CMOS_READ(RTC_FREQ_SELECT);
		sec = CMOS_READ(RTC_SECONDS);
		min = CMOS_READ(RTC_MINUTES);
		hour = CMOS_READ(RTC_HOURS);
		day = CMOS_READ(RTC_DAY_OF_MONTH);
		mon = CMOS_READ(RTC_MONTH);
		year = CMOS_READ(RTC_YEAR);
		uip |= CMOS_READ(RTC_FREQ_SELECT);
		if ((uip & RTC_UIP)==0) break;
	}

	if (!(CMOS_READ(RTC_CONTROL) & RTC_DM_BINARY)
	    || RTC_ALWAYS_BCD)
	{
		BCD_TO_BIN(sec);
		BCD_TO_BIN(min);
		BCD_TO_BIN(hour);
		BCD_TO_BIN(day);
		BCD_TO_BIN(mon);
		BCD_TO_BIN(year);
	}
	if ((year += 1900) < 1970)
		year += 100;
	return mktime(year, mon, day, hour, min, sec);
}

__prep
int mk48t59_set_rtc_time(unsigned long nowtime)
{
	unsigned char save_control;
	struct rtc_time tm;

	spin_lock(&rtc_lock);
	to_tm(nowtime, &tm);

	/* tell the clock it's being written */
	save_control = ppc_md.nvram_read_val(MK48T59_RTC_CONTROLA);

	ppc_md.nvram_write_val(MK48T59_RTC_CONTROLA,
			     (save_control | MK48T59_RTC_CA_WRITE));

        tm.tm_year = (tm.tm_year - 1900) % 100;
	BIN_TO_BCD(tm.tm_sec);
	BIN_TO_BCD(tm.tm_min);
	BIN_TO_BCD(tm.tm_hour);
	BIN_TO_BCD(tm.tm_mon);
	BIN_TO_BCD(tm.tm_mday);
	BIN_TO_BCD(tm.tm_year);

	ppc_md.nvram_write_val(MK48T59_RTC_SECONDS,      tm.tm_sec);
	ppc_md.nvram_write_val(MK48T59_RTC_MINUTES,      tm.tm_min);
	ppc_md.nvram_write_val(MK48T59_RTC_HOURS,        tm.tm_hour);
	ppc_md.nvram_write_val(MK48T59_RTC_MONTH,        tm.tm_mon);
	ppc_md.nvram_write_val(MK48T59_RTC_DAY_OF_MONTH, tm.tm_mday);
	ppc_md.nvram_write_val(MK48T59_RTC_YEAR,         tm.tm_year);

	/* Turn off the write bit. */
	ppc_md.nvram_write_val(MK48T59_RTC_CONTROLA, save_control);
	spin_unlock(&rtc_lock);

	return 0;
}

__prep
unsigned long mk48t59_get_rtc_time(void)
{
	unsigned char save_control;
	unsigned int year, mon, day, hour, min, sec;

	/* Simple: freeze the clock, read it and allow updates again */
	save_control = ppc_md.nvram_read_val(MK48T59_RTC_CONTROLA);
	save_control &= ~MK48T59_RTC_CA_READ;
	ppc_md.nvram_write_val(MK48T59_RTC_CONTROLA, save_control);

	/* Set the register to read the value. */
	ppc_md.nvram_write_val(MK48T59_RTC_CONTROLA,
			     (save_control | MK48T59_RTC_CA_READ));

	sec = ppc_md.nvram_read_val(MK48T59_RTC_SECONDS);
	min = ppc_md.nvram_read_val(MK48T59_RTC_MINUTES);
	hour = ppc_md.nvram_read_val(MK48T59_RTC_HOURS);
	day = ppc_md.nvram_read_val(MK48T59_RTC_DAY_OF_MONTH);
	mon = ppc_md.nvram_read_val(MK48T59_RTC_MONTH);
	year = ppc_md.nvram_read_val(MK48T59_RTC_YEAR);

	/* Let the time values change again. */
	ppc_md.nvram_write_val(MK48T59_RTC_CONTROLA, save_control);

	BCD_TO_BIN(sec);
	BCD_TO_BIN(min);
	BCD_TO_BIN(hour);
	BCD_TO_BIN(day);
	BCD_TO_BIN(mon);
	BCD_TO_BIN(year);

	year = year + 1900;
	if (year < 1970) {
		year += 100;
	}

	return mktime(year, mon, day, hour, min, sec);
}
