/*
 * Copyright (c) UNIX System Laboratories, Inc.  All or some portions
 * of this file are derived from material licensed to the
 * University of California by American Telephone and Telegraph Co.
 * or UNIX System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 */
/*-
 * Copyright (c) 1982, 1986, 1991 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
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
 *	from: @(#)tty_conf.c	7.6 (Berkeley) 5/9/91
 *	$Id: tty_conf.c,v 1.9 1994/05/30 03:23:14 ache Exp $
 */

#include "param.h"
#include "systm.h"
#include "buf.h"
#include "ioctl.h"
#include "tty.h"
#include "conf.h"

int	enodev();
int	nullop();
static int nullioctl(struct tty *, int, caddr_t, int);

#define VE(x) ((void (*)())x)	/* XXX */
#define IE(x) ((int (*)())x)	/* XXX */

/* XXX - doesn't work */
#include "tb.h"
#if NTB > 0
int	tbopen(),tbclose(),tbread(),tbinput(),tbioctl();
#endif

#include "sl.h"
#if NSL > 0
int	slopen(),slclose(),slinput(),sltioctl(),slstart();
#endif
#include "ppp.h"
#if NPPP > 0
int	pppopen(),pppclose(),pppread(),pppwrite(),pppinput();
int	ppptioctl(),pppstart(),pppselect();
#endif


struct	linesw linesw[] =
{
	ttyopen, ttylclose, ttread, ttwrite, nullioctl,
	ttyinput, enodev, nullop, ttstart, ttymodem,	/* 0- termios */

	IE(enodev), VE(enodev), IE(enodev), IE(enodev), IE(enodev), /* 1- defunct */
	VE(enodev), IE(enodev), IE(enodev), VE(enodev), IE(enodev),

	ttyopen, ttylclose, ttread, ttwrite, nullioctl,
	ttyinput, enodev, nullop, ttstart, ttymodem,    /* 2- NTTYDISC */

#if NTB > 0
	tbopen, tbclose, tbread, IE(enodev), tbioctl,
	tbinput, enodev, nullop, ttstart, ttymodem,	/* 3- TABLDISC */
#else
	IE(enodev), VE(enodev), IE(enodev), IE(enodev), IE(enodev),
	VE(enodev), IE(enodev), IE(enodev), VE(enodev), IE(enodev),
#endif
#if NSL > 0
	slopen, VE(slclose), IE(enodev), IE(enodev), sltioctl,
	VE(slinput), enodev, nullop, VE(slstart), ttymodem, /* 4- SLIPDISC */
#else
	IE(enodev), VE(enodev), IE(enodev), IE(enodev), IE(enodev),
	VE(enodev), IE(enodev), IE(enodev), VE(enodev), IE(enodev),
#endif
#if NPPP > 0
	pppopen, VE(pppclose), pppread, pppwrite, ppptioctl,
	VE(pppinput), enodev, nullop, VE(pppstart), ttymodem, /* 5- PPPDISC */
#else
	IE(enodev), VE(enodev), IE(enodev), IE(enodev), IE(enodev),
	VE(enodev), IE(enodev), IE(enodev), VE(enodev), IE(enodev),
#endif
};

int	nldisp = sizeof (linesw) / sizeof (linesw[0]);

/*
 * Do nothing specific version of line
 * discipline specific ioctl command.
 */
/*ARGSUSED*/
static int
nullioctl(tp, cmd, data, flags)
	struct tty *tp;
	int cmd;
	char *data;
	int flags;
{

#ifdef lint
	tp = tp; data = data; flags = flags;
#endif
	return (-1);
}
