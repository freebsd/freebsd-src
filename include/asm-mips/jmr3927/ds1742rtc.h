/*
 * ds1742rtc.h - register definitions for the Real-Time-Clock / CMOS RAM
 *
 *   Based on include/asm-mips/ds1643rtc.h.
 *
 * Copyright (C) 1999-2001 Toshiba Corporation
 * It was written to be part of the Linux operating system.
 */
/* permission is hereby granted to copy, modify and redistribute this code
 * in terms of the GNU Library General Public License, Version 2 or later,
 * at your option.
 */
#ifndef _DS1742RTC_H
#define _DS1742RTC_H

#include <linux/rtc.h>
#include <asm/mc146818rtc.h>	/* bad name... */

#define RTC_BRAM_SIZE		0x800
#define RTC_OFFSET		0x7f8

/**********************************************************************
 * register summary
 **********************************************************************/
#define RTC_CONTROL		(RTC_OFFSET + 0)
#define RTC_CENTURY		(RTC_OFFSET + 0)
#define RTC_SECONDS		(RTC_OFFSET + 1)
#define RTC_MINUTES		(RTC_OFFSET + 2)
#define RTC_HOURS		(RTC_OFFSET + 3)
#define RTC_DAY			(RTC_OFFSET + 4)
#define RTC_DATE		(RTC_OFFSET + 5)
#define RTC_MONTH		(RTC_OFFSET + 6)
#define RTC_YEAR		(RTC_OFFSET + 7)

#define RTC_CENTURY_MASK	0x3f
#define RTC_SECONDS_MASK	0x7f
#define RTC_DAY_MASK		0x07

/*
 * Bits in the Control/Century register
 */
#define RTC_WRITE		0x80
#define RTC_READ		0x40

/*
 * Bits in the Seconds register
 */
#define RTC_STOP		0x80

/*
 * Bits in the Day register
 */
#define RTC_BATT_FLAG		0x80
#define RTC_FREQ_TEST		0x40

/*
 * Conversion between binary and BCD.
 */
#ifndef BCD_TO_BIN
#define BCD_TO_BIN(val) ((val)=((val)&15) + ((val)>>4)*10)
#endif

#ifndef BIN_TO_BCD
#define BIN_TO_BCD(val) ((val)=(((val)/10)<<4) + (val)%10)
#endif

#endif /* _DS1742RTC_H */
