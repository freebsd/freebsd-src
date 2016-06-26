/*-
 * Copyright (c) 2009 Greg Ansley  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * $FreeBSD$
 */

#ifndef _AT91REG_H_
#define	_AT91REG_H_

#include "opt_at91.h"

/* Where builtin peripherals start in KVM */
#define	AT91_BASE		0xd0000000

/* Where builtin peripherals start PA */
#define	AT91_PA_BASE		0xf0000000

/* A few things that we count on being the same
 * throughout the whole family of SOCs */

/* SYSC System Controller */
/* System Registers */
#define	AT91_SYS_BASE	0xffff000
#define	AT91_SYS_SIZE	0x1000

#define AT91_DBGU0	0x0ffff200	/* Most */
#define AT91_DBGU1	0x0fffee00	/* SAM9263, CAP9, and SAM9G45 */

#define	AT91_DBGU_SIZE	0x200
#define	DBGU_C1R		(64) /* Chip ID1 Register */
#define	DBGU_C2R		(68) /* Chip ID2 Register */
#define	DBGU_FNTR		(72) /* Force NTRST Register */

#define	AT91_CPU_VERSION_MASK	0x0000001f
#define	AT91_CPU_FAMILY_MASK    0x0ff00000

#define	AT91_CPU_RM9200 	0x09290780
#define	AT91_CPU_SAM9260	0x019803a0
#define	AT91_CPU_SAM9261	0x019703a0
#define	AT91_CPU_SAM9263	0x019607a0
#define	AT91_CPU_SAM9G10	0x819903a0
#define	AT91_CPU_SAM9G20	0x019905a0
#define	AT91_CPU_SAM9G45	0x819b05a0
#define	AT91_CPU_SAM9N12        0x819a07a0
#define	AT91_CPU_SAM9RL64	0x019b03a0
#define	AT91_CPU_SAM9X5		0x819a05a0

#define	AT91_CPU_SAM9XE128	0x329973a0
#define	AT91_CPU_SAM9XE256	0x329a93a0
#define	AT91_CPU_SAM9XE512	0x329aa3a0

#define	AT91_CPU_CAP9           0x039a03a0

#define	AT91_EXID_SAM9M11	0x00000001
#define	AT91_EXID_SAM9M10	0x00000002
#define	AT91_EXID_SAM9G46	0x00000003
#define	AT91_EXID_SAM9G45	0x00000004

#define	AT91_EXID_SAM9G15	0x00000000
#define	AT91_EXID_SAM9G35	0x00000001
#define	AT91_EXID_SAM9X35	0x00000002
#define	AT91_EXID_SAM9G25	0x00000003
#define	AT91_EXID_SAM9X25	0x00000004

#define AT91_IRQ_SYSTEM		1

#endif /* _AT91REG_H_ */
