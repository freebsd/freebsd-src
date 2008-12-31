/*-
 * Copyright (c) 2006 M. Warner Losh.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/boot/arm/at91/libat91/at91rm9200_lowlevel.h,v 1.6.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _AT91RM9200_LOWLEVEL_H_
#define _AT91RM9200_LOWLEVEL_H_

/* default system config parameters */

#define SDRAM_BASE		0x20000000

#ifdef BOOT_KB920X
/* The following divisor sets PLLA frequency: e.g. 10/5 * 90 = 180MHz */
#define OSC_MAIN_FREQ_DIV	5	/* for 10MHz osc */
#define SDRAM_WIDTH	AT91C_SDRC_DBW_16_BITS
typedef unsigned short sdram_size_t;
#define OSC_MAIN_MULT		90
#endif

#ifdef BOOT_CENTIPAD
/* The following divisor sets PLLA frequency: e.g. 10/5 * 90 = 180MHz */
#define OSC_MAIN_FREQ_DIV	5	/* for 10MHz osc */
#define SDRAM_WIDTH	AT91C_SDRC_DBW_16_BITS
typedef unsigned short sdram_size_t;
#define OSC_MAIN_MULT		90
#endif

#ifdef BOOT_BWCT
/* The following divisor sets PLLA frequency: e.g. 16/4 * 45 = 180MHz */
#define OSC_MAIN_FREQ_DIV	4	/* for 16MHz osc */
#define SDRAM_WIDTH	AT91C_SDRC_DBW_32_BITS
typedef unsigned int sdram_size_t;
#define OSC_MAIN_MULT		45
#endif

#ifdef BOOT_TSC
/* The following divisor sets PLLA frequency: e.g. 16/4 * 45 = 180MHz */
#define OSC_MAIN_FREQ_DIV	4	/* for 16MHz osc */
#define SDRAM_WIDTH	AT91C_SDRC_DBW_32_BITS
typedef unsigned int sdram_size_t;
#define OSC_MAIN_MULT		45
#endif

/* Master clock frequency at power-up */
#define AT91C_MASTER_CLOCK 60000000

/* #define GetSeconds() (AT91C_BASE_RTC->RTC_TIMR & AT91C_RTC_SEC) */
#define GetSeconds() (AT91C_BASE_ST->ST_CRTR >> 15)

extern void _init(void);

#endif /* _AT91RM9200_LOWLEVEL_H_ */
