/*
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 1996, 2001-2003 Silicon Graphics, Inc. All rights reserved.
 * Copyright (C) 2001 by Ralf Baechle
 */
#ifndef _ASM_IA64_SN_KLCLOCK_H
#define _ASM_IA64_SN_KLCLOCK_H

#include <asm/sn/ioc3.h>
#include <asm/sn/ioc4.h>

#define RTC_BASE_ADDR		(unsigned char *)(nvram_base)

/* Defines for the SGS-Thomson M48T35 clock */
#define RTC_SGS_WRITE_ENABLE    0x80
#define RTC_SGS_READ_PROTECT    0x40
#define RTC_SGS_YEAR_ADDR       (RTC_BASE_ADDR + 0x7fffL)
#define RTC_SGS_MONTH_ADDR      (RTC_BASE_ADDR + 0x7ffeL)
#define RTC_SGS_DATE_ADDR       (RTC_BASE_ADDR + 0x7ffdL)
#define RTC_SGS_DAY_ADDR        (RTC_BASE_ADDR + 0x7ffcL)
#define RTC_SGS_HOUR_ADDR       (RTC_BASE_ADDR + 0x7ffbL)
#define RTC_SGS_MIN_ADDR        (RTC_BASE_ADDR + 0x7ffaL)
#define RTC_SGS_SEC_ADDR        (RTC_BASE_ADDR + 0x7ff9L)
#define RTC_SGS_CONTROL_ADDR    (RTC_BASE_ADDR + 0x7ff8L)

/* Defines for the Dallas DS1386 */
#define RTC_DAL_UPDATE_ENABLE   0x80
#define RTC_DAL_UPDATE_DISABLE  0x00
#define RTC_DAL_YEAR_ADDR       (RTC_BASE_ADDR + 0xaL)
#define RTC_DAL_MONTH_ADDR      (RTC_BASE_ADDR + 0x9L)
#define RTC_DAL_DATE_ADDR       (RTC_BASE_ADDR + 0x8L)
#define RTC_DAL_DAY_ADDR        (RTC_BASE_ADDR + 0x6L)
#define RTC_DAL_HOUR_ADDR       (RTC_BASE_ADDR + 0x4L)
#define RTC_DAL_MIN_ADDR        (RTC_BASE_ADDR + 0x2L)
#define RTC_DAL_SEC_ADDR        (RTC_BASE_ADDR + 0x1L)
#define RTC_DAL_CONTROL_ADDR    (RTC_BASE_ADDR + 0xbL)
#define RTC_DAL_USER_ADDR       (RTC_BASE_ADDR + 0xeL)

/* Defines for the Dallas DS1742 */
#define RTC_DS1742_WRITE_ENABLE    0x80
#define RTC_DS1742_READ_ENABLE     0x40
#define RTC_DS1742_UPDATE_DISABLE  0x00
#define RTC_DS1742_YEAR_ADDR       (RTC_BASE_ADDR + 0x7ffL)
#define RTC_DS1742_MONTH_ADDR      (RTC_BASE_ADDR + 0x7feL)
#define RTC_DS1742_DATE_ADDR       (RTC_BASE_ADDR + 0x7fdL)
#define RTC_DS1742_DAY_ADDR        (RTC_BASE_ADDR + 0x7fcL)
#define RTC_DS1742_HOUR_ADDR       (RTC_BASE_ADDR + 0x7fbL)
#define RTC_DS1742_MIN_ADDR        (RTC_BASE_ADDR + 0x7faL)
#define RTC_DS1742_SEC_ADDR        (RTC_BASE_ADDR + 0x7f9L)
#define RTC_DS1742_CONTROL_ADDR    (RTC_BASE_ADDR + 0x7f8L)
#define RTC_DS1742_USER_ADDR       (RTC_BASE_ADDR + 0x0L)

#define BCD_TO_INT(x) (((x>>4) * 10) + (x & 0xf))
#define INT_TO_BCD(x) (((x / 10)<<4) + (x % 10))

#define YRREF	1970 

#endif /* _ASM_IA64_SN_KLCLOCK_H  */
