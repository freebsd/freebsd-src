/*
 *	include/asm-mips/dec/rtc-dec.h
 *
 *	RTC definitions for DECstation style attached Dallas DS1287 chip.
 *
 *	Copyright (C) 2002  Maciej W. Rozycki
 *
 *	This program is free software; you can redistribute it and/or
 *	modify it under the terms of the GNU General Public License
 *	as published by the Free Software Foundation; either version
 *	2 of the License, or (at your option) any later version.
 */
#ifndef __ASM_MIPS_DEC_RTC_DEC_H
#define __ASM_MIPS_DEC_RTC_DEC_H

#include <linux/types.h>

#include <asm/addrspace.h>

extern volatile u8 *dec_rtc_base;
extern unsigned long dec_kn_slot_size;

extern struct rtc_ops dec_rtc_ops;

#define RTC_PORT(x)	CPHYSADDR(dec_rtc_base)
#define RTC_IO_EXTENT	dec_kn_slot_size
#define RTC_IOMAPPED	0
#define RTC_IRQ		0

#define RTC_DEC_YEAR	0x3f	/* Where we store the real year on DECs.  */

#endif /* __ASM_MIPS_DEC_RTC_DEC_H */
