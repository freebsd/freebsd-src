/*
 * Device driver for Specialix range (SI/XIO) of serial line multiplexors.
 * 'C' definitions for Specialix serial multiplex driver.
 *
 * Copyright (C) 1990, 1992 Specialix International,
 * Copyright (C) 1993, Andy Rutter <andy@acronym.co.uk>
 * Copyright (C) 1995, Peter Wemm <peter@haywire.dialix.com>
 *
 * Derived from:	SunOS 4.x version
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notices, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notices, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Andy Rutter of
 *	Advanced Methods and Tools Ltd. based on original information
 *	from Specialix International.
 * 4. Neither the name of Advanced Methods and Tools, nor Specialix
 *    International may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN
 * NO EVENT SHALL THE AUTHORS BE LIABLE.
 *
 *	$FreeBSD$
 */

/*
 * Hardware parameters which should be changed at your peril!
 */

/* Base and mask for SI Host 2.x (SIHOST2) */
#define SIPLSIG		0x7FF8			/* Start of control space */
#define SIPLCNTL	0x7FF8			/* Ditto */
#define SIPLRESET	SIPLCNTL		/* 0 = reset */
#define SIPLIRQ11	(SIPLCNTL+1)		/* 0 = mask irq 11 */
#define SIPLIRQ12	(SIPLCNTL+2)		/* 0 = mask irq 12 */
#define SIPLIRQ15	(SIPLCNTL+3)		/* 0 = mask irq 15 */
#define SIPLIRQSET	(SIPLCNTL+4)		/* 0 = interrupt host */
#define SIPLIRQCLR	(SIPLCNTL+5)		/* 0 = clear irq */

/* SI Host 1.x */
#define	SIRAM		0x0000			/* Ram Starts here */
#define	SIRESET		0x8000			/* Set reset */
#define	SIRESET_CL 	0xc000			/* Clear reset */
#define	SIWAIT		0x9000			/* Set wait */
#define	SIWAIT_CL 	0xd000			/* Set wait */
#define SIINTCL		0xA000			/* Clear host int */
#define SIINTCL_CL 	0xE000			/* Clear host int */

/*
 * MEMSIZE is the total shared mem region
 * RAMSIZE is value to use when probing
 */
#define	SIHOST_MEMSIZE		0x10000
#define	SIHOST_RAMSIZE		0x8000
#define	SIHOST2_MEMSIZE		0x8000
#define	SIHOST2_RAMSIZE		0x7ff7
#define	SIEISA_MEMSIZE		0x10000
#define	SEISAT_RAMSIZE		0x10000

