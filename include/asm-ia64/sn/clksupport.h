/*
 *
 * This file is subject to the terms and conditions of the GNU General Public
 * License.  See the file "COPYING" in the main directory of this archive
 * for more details.
 *
 * Copyright (C) 2000-2003 Silicon Graphics, Inc. All rights reserved.
 */

/*
 * This file contains definitions for accessing a platform supported high resolution
 * clock. The clock is monitonically increasing and can be accessed from any node
 * in the system. The clock is synchronized across nodes - all nodes see the
 * same value.
 * 
 *	RTC_COUNTER_ADDR - contains the address of the counter 
 *
 *	GET_RTC_COUNTER() - macro to read the value of the clock
 *
 *	RTC_CYCLES_PER_SEC - clock frequency in ticks per second	
 *
 */

#ifndef _ASM_IA64_SN_CLKSUPPORT_H
#define _ASM_IA64_SN_CLKSUPPORT_H

#include <linux/config.h>
#include <asm/sn/arch.h>
#include <asm/sn/addrs.h>

typedef long clkreg_t;

extern unsigned long sn_rtc_cycles_per_second;
extern unsigned long sn_rtc_usec_per_cyc;
extern unsigned long sn_rtc_per_itc;
extern unsigned long sn_rtc_delta;


#include <asm/sn/addrs.h>
#include <asm/sn/sn2/addrs.h>
#include <asm/sn/sn2/shubio.h>
#include <asm/sn/sn2/shub_mmr.h>
#define RTC_MASK		SH_RTC_MASK
#define RTC_COUNTER_ADDR	((clkreg_t*)LOCAL_MMR_ADDR(SH_RTC))
#define RTC_COMPARE_A_ADDR      ((clkreg_t*)LOCAL_MMR_ADDR(SH_RTC))
#define RTC_COMPARE_B_ADDR      ((clkreg_t*)LOCAL_MMR_ADDR(SH_RTC))
#define RTC_INT_PENDING_A_ADDR  ((clkreg_t*)LOCAL_MMR_ADDR(SH_RTC))
#define RTC_INT_PENDING_B_ADDR  ((clkreg_t*)LOCAL_MMR_ADDR(SH_RTC))
#define RTC_INT_ENABLED_A_ADDR  ((clkreg_t*)LOCAL_MMR_ADDR(SH_RTC))
#define RTC_INT_ENABLED_B_ADDR  ((clkreg_t*)LOCAL_MMR_ADDR(SH_RTC))

#define SN_RTC_PER_ITC_SHIFT	34
#define GET_RTC_COUNTER()	(*RTC_COUNTER_ADDR)
#define rtc_time()		GET_RTC_COUNTER()

#define RTC_CYCLES_PER_SEC	sn_rtc_cycles_per_second

#endif /* _ASM_IA64_SN_CLKSUPPORT_H */
