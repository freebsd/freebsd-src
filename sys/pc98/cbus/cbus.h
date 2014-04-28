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
 * $FreeBSD$
 */

#ifndef _PC98_PC98_PC98_H_
#define	_PC98_PC98_PC98_H_

/*
 * PC98 Bus conventions
 * modified for PC9801 by A.Kojima F.Ukai M.Ishii 
 *			Kyoto University Microcomputer Club (KMC)
 */

/*
 * Input / Output Port Assignments -- PC98 IO address ... very dirty (^_^;
 */

#define	IO_ICU1		0x000		/* 8259A Interrupt Controller #1 */
#define	IO_ICU2		0x008		/* 8259A Interrupt Controller #2 */
#define	IO_RTC		0x020		/* 4990A RTC */
#define	IO_SYSPORT	0x031		/* 8255A System Port */
#define	IO_KBD		0x041		/* 8251A Keyboard */
#define	IO_COM2		0x0B1		/* 8251A RS232C serial I/O (ext) */
#define	IO_COM3		0x0B9		/* 8251A RS232C serial I/O (ext) */
#define	IO_FDPORT	0x0BE		/* FD I/F port (1M<->640K,EMTON) */

/*
 * Input / Output Port Sizes
 */
#define	IO_KBDSIZE	16		/* 8042 Keyboard controllers */

#endif /* !_PC98_PC98_PC98_H_ */
