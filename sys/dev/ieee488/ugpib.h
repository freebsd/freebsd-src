/*-
 * Copyright (c) 2005 Poul-Henning Kamp <phk@FreeBSD.org>
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
 * $FreeBSD: src/sys/dev/ieee488/ugpib.h,v 1.5.20.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

#ifndef _DEV_IEEE488_UGPIB_H_
#define _DEV_IEEE488_UGPIB_H_

/* ibfoo() return values */
#define EDVR	0		/* System error				*/
#define ECIC	1		/* Not Active Controller		*/
#define ENOL	2		/* Nobody listening			*/
#define EADR	3		/* Controller not addressed		*/
#define EARG	4		/* Invalid argument			*/
#define ESAC	5		/* Not System Controller		*/
#define EABO	6		/* I/O Aborted/Time out			*/
#define ENEB	7		/* No such controller			*/
#define EOIP	10		/* Async I/O in progress		*/
#define ECAP	11		/* No such capability			*/
#define EFSO	12		/* File system error			*/
#define EBUS	14		/* Command byte xfer error		*/
#define ESTB	15		/* Serial poll status byte lost		*/
#define ESRQ	16		/* SRQ line stuck			*/
#define ETAB	20		/* Table problem			*/

/* ibsta bits */
#define ERR	(1<<15)		/* Error				*/
#define TIMO	(1<<14)		/* Timeout				*/
#define END	(1<<13)		/* EOI/EOS				*/
#define SRQI	(1<<12)		/* SRQ					*/
#define RQS	(1<<11)		/* Device requests service		*/
#define SPOLL	(1<<10)		/* Serial Poll				*/
#define EVENT	(1<<9)		/* Event occured			*/
#define CMPL	(1<<8)		/* I/O complete				*/
#define LOK	(1<<7)		/* Lockout				*/
#define REM	(1<<6)		/* Remote				*/
#define CIC	(1<<5)		/* CIC					*/
#define ATN	(1<<4)		/* ATN					*/
#define TACS	(1<<3)		/* Talker				*/
#define LACS	(1<<2)		/* Listener				*/
#define DTAS	(1<<1)		/* Device trigger status		*/
#define DCAS	(1<<0)		/* Device clear state			*/

/* Timeouts */
#define TNONE	0
#define T10us	1
#define T30us	2
#define T100us	3
#define T300us	4
#define T1ms	5
#define T3ms	6
#define T10ms	7
#define T30ms	8
#define T100ms	9
#define T300ms	10
#define T1s	11
#define T3s	12
#define T10s	13
#define T30s	14
#define T100s	15
#define T300s	16
#define T1000s	17

/* EOS bits */
#define REOS	(1 << 10)
#define XEOS	(1 << 11)
#define BIN	(1 << 12)

/* Bus commands */
#define GTL	0x01		/* Go To Local				*/
#define SDC	0x04		/* Selected Device Clear		*/
#define GET	0x08		/* Group Execute Trigger		*/
#define LAD	0x20		/* Listen address			*/
#define UNL	0x3F		/* Unlisten				*/
#define TAD	0x40		/* Talk address				*/
#define UNT	0x5F		/* Untalk				*/

#ifndef _KERNEL

extern int ibcnt, iberr, ibsta;

int ibask(int handle, int option, int *retval);
int ibbna(int handle, char *bdname);
int ibcac(int handle, int v);
int ibclr(int handle);
int ibcmd(int handle, void *buffer, long cnt);
int ibcmda(int handle, void *buffer, long cnt);
int ibconfig(int handle, int option, int value);
int ibdev(int boardID, int pad, int sad, int tmo, int eot, int eos);
int ibdiag(int handle, void *buffer, long cnt);
int ibdma(int handle, int v);
int ibeos(int handle, int eos);
int ibeot(int handle, int eot);
int ibevent(int handle, short *event);
int ibfind(char *bdname);
int ibgts(int handle, int v);
int ibist(int handle, int v);
int iblines(int handle, short *lines);
int ibllo(int handle);
int ibln(int handle, int padval, int sadval, short *listenflag);
int ibloc(int handle);
int ibonl(int handle, int v);
int ibpad(int handle, int pad);
int ibpct(int handle);
int ibpoke(int handle, int option, int value);
int ibppc(int handle, int v);
int ibrd(int handle, void *buffer, long cnt);
int ibrda(int handle, void *buffer, long cnt);
int ibrdf(int handle, char *flname);
int ibrdkey(int handle, void *buffer, int cnt);
int ibrpp(int handle, char *ppr);
int ibrsc(int handle, int v);
int ibrsp(int handle, char *spr);
int ibrsv(int handle, int v);
int ibsad(int handle, int sad);
int ibsgnl(int handle, int v);
int ibsic(int handle);
int ibsre(int handle, int v);
int ibsrq(void (*func)(void));
int ibstop(int handle);
int ibtmo(int handle, int tmo);
int ibtrap(int  mask, int mode);
int ibtrg(int handle);
int ibwait(int handle, int mask);
int ibwrt(int handle, const void *buffer, long cnt);
int ibwrta(int handle, const void *buffer, long cnt);
int ibwrtf(int handle, const char *flname);
int ibwrtkey(int handle, const void *buffer, int cnt);
int ibxtrc(int handle, void *buffer, long cnt);
#endif /* _KERNEL */
#endif /* _DEV_IEEE488_UGPIB_H_ */
