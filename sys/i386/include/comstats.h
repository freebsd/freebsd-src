/*****************************************************************************/

/*
 * comstats.h  -- Serial Port Stats.
 *
 * Copyright (c) 1994-1996 Greg Ungerer (gerg@stallion.oz.au).
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Greg Ungerer.
 * 4. Neither the name of the author nor the names of any co-contributors
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
 * $FreeBSD: src/sys/i386/include/comstats.h,v 1.5 1999/08/28 00:44:09 peter Exp $
 */

/*****************************************************************************/
#ifndef	_COMSTATS_H
#define	_COMSTATS_H
/*****************************************************************************/

/*
 *	Serial port stats structure. The structure itself is UART
 *	independent, but some fields may be UART/driver specific (for
 *	example state).
 */

typedef struct {
	unsigned long	brd;
	unsigned long	panel;
	unsigned long	port;
	unsigned long	hwid;
	unsigned long	type;
	unsigned long	txtotal;
	unsigned long	rxtotal;
	unsigned long	txbuffered;
	unsigned long	rxbuffered;
	unsigned long	rxoverrun;
	unsigned long	rxparity;
	unsigned long	rxframing;
	unsigned long	rxlost;
	unsigned long	txbreaks;
	unsigned long	rxbreaks;
	unsigned long	txxon;
	unsigned long	txxoff;
	unsigned long	rxxon;
	unsigned long	rxxoff;
	unsigned long	txctson;
	unsigned long	txctsoff;
	unsigned long	rxrtson;
	unsigned long	rxrtsoff;
	unsigned long	modem;
	unsigned long	state;
	unsigned long	flags;
	unsigned long	ttystate;
	unsigned long	cflags;
	unsigned long	iflags;
	unsigned long	oflags;
	unsigned long	lflags;
	unsigned long	signals;
} comstats_t;


/*
 *	Board stats structure. Returns usefull info about the board.
 */

#define	COM_MAXPANELS	8

typedef struct {
	unsigned long	panel;
	unsigned long	type;
	unsigned long	hwid;
	unsigned long	nrports;
} companel_t;

typedef struct {
	unsigned long	brd;
	unsigned long	type;
	unsigned long	hwid;
	unsigned long	state;
	unsigned long	ioaddr;
	unsigned long	ioaddr2;
	unsigned long	memaddr;
	unsigned long	irq;
	unsigned long	nrpanels;
	unsigned long	nrports;
	companel_t	panels[COM_MAXPANELS];
} combrd_t;


/*
 *	Define the ioctl operations for stats stuff.
 */
#define	COM_GETPORTSTATS	_IOWR('c', 30, comstats_t)
#define	COM_CLRPORTSTATS	_IOWR('c', 31, comstats_t)
#define	COM_GETBRDSTATS		_IOWR('c', 32, combrd_t)

/*****************************************************************************/
#endif
