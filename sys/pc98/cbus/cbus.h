/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from: @(#)isa.h	5.7 (Berkeley) 5/9/91
 * $FreeBSD: src/sys/pc98/cbus/cbus.h,v 1.22.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef _PC98_PC98_PC98_H_
#define	_PC98_PC98_PC98_H_

/* BEWARE:  Included in both assembler and C code */

/*
 * PC98 Bus conventions
 * modified for PC9801 by A.Kojima F.Ukai M.Ishii 
 *			Kyoto University Microcomputer Club (KMC)
 */

/*
 * Input / Output Port Assignments
 */
#ifndef IO_ISABEGIN
#define	IO_ISABEGIN	0x000		/* 0x000 - Beginning of I/O Registers */

/* PC98 IO address ... very dirty (^_^; */

#define	IO_ICU1		0x000		/* 8259A Interrupt Controller #1 */
#define	IO_ICU2		0x008		/* 8259A Interrupt Controller #2 */
#define	IO_RTC		0x020		/* 4990A RTC */
#define	IO_SYSPORT	0x031		/* 8255A System Port */
#define	IO_KBD		0x041		/* 8251A Keyboard */
#define	IO_COM2		0x0B1		/* 8251A RS232C serial I/O (ext) */
#define	IO_COM3		0x0B9		/* 8251A RS232C serial I/O (ext) */
#define	IO_FDPORT	0x0BE		/* FD I/F port (1M<->640K,EMTON) */
#define	IO_WD1_EPSON	0x80		/* 386note Hard disk controller */
#define	IO_ISAEND	0xFFFF		/* - 0x3FF End of I/O Registers */
#endif /* !IO_ISABEGIN */

/*
 * Input / Output Port Sizes - these are from several sources, and tend
 * to be the larger of what was found, ie COM ports can be 4, but some
 * boards do not fully decode the address, thus 8 ports are used.
 */
#ifndef	IO_ISASIZES
#define	IO_ISASIZES

#define	IO_KBDSIZE	16		/* 8042 Keyboard controllers */
#define	IO_LPTSIZE	8		/* LPT controllers, some use only 4 */
#define	IO_LPTSIZE_EXTENDED	8	/* "Extended" LPT controllers */
#define	IO_LPTSIZE_NORMAL	4	/* "Normal" LPT controllers */

#endif /* !IO_ISASIZES */

/*
 * Input / Output Memory Physical Addresses
 */
#ifndef	IOM_BEGIN
#define	IOM_BEGIN	0x0A0000	/* Start of I/O Memory "hole" */
#define	IOM_END		0x100000	/* End of I/O Memory "hole" */
#define	IOM_SIZE	(IOM_END - IOM_BEGIN)
#endif /* !IOM_BEGIN */

/*
 * RAM Physical Address Space (ignoring the above mentioned "hole")
 */
#ifndef	RAM_BEGIN
#define	RAM_BEGIN	0x0000000	/* Start of RAM Memory */
#ifdef	EPSON_BOUNCEDMA
#define	RAM_END		0x0f00000	/* End of EPSON GR?? RAM Memory */
#else
#define	RAM_END		0x1000000	/* End of RAM Memory */
#endif
#define	RAM_SIZE	(RAM_END - RAM_BEGIN)
#endif /* !RAM_BEGIN */

#endif /* !_PC98_PC98_PC98_H_ */
