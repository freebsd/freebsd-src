/*-
 * Copyright (c) 1995 Bruce Evans.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$Id: cyreg.h,v 1.1 1995/07/05 12:15:51 bde Exp $
 */

/*
 * Definitions for Cyclades Cyclom-Y serial boards.
 */

#define	CY8_SVCACKR		0x100
#define	CY8_SVCACKT		0x200
#define	CY8_SVCACKM		0x300
#define	CY_CD1400_MEMSIZE	0x400
#define	CY16_RESET		0x1400
#define	CY_CLEAR_INTR		0x1800	/* intr ack address */

#define	CY_MAX_CD1400s		8	/* for Cyclom-32Y */

#define	CY_CLOCK		25000000	/* baud rate clock */

#ifdef CyDebug
#define	cd_inb(iobase, reg)		(++cd_inbs, *((iobase) + 2 * (reg)))
#define	cy_inb(iobase, reg)		(++cy_inbs, *((iobase) + (reg)))
#define	cd_outb(iobase, reg, val)	(++cd_outbs, (void)(*((iobase) + 2 * (reg)) = (val)))
#define	cy_outb(iobase, reg, val)	(++cy_outbs, (void)(*((iobase) + (reg)) = (val)))
#else
#define	cd_inb(iobase, reg)		(*((iobase) + 2 * (reg)))
#define	cy_inb(iobase, reg)		(*((iobase) + (reg)))
#define	cd_outb(iobase, reg, val)	((void)(*((iobase) + 2 * (reg)) = (val)))
#define	cy_outb(iobase, reg, val)	((void)(*((iobase) + (reg)) = (val)))
#endif
