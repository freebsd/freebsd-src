/*-
 * Copyright (c) 1982, 1986, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)tty_conf.c	8.4 (Berkeley) 1/21/94
 * $Id: tty_conf.c,v 1.4 1994/10/05 21:22:24 wollman Exp $
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/tty.h>
#include <sys/conf.h>

#define	ttynodisc ((int (*) __P((dev_t, struct tty *)))enodev)
#define	ttyerrclose ((int (*) __P((struct tty *, int flags)))enodev)
#define	ttyerrio ((int (*) __P((struct tty *, struct uio *, int)))enodev)
#define	ttyerrinput ((int (*) __P((int c, struct tty *)))enodev)
#define	ttyerrstart ((int (*) __P((struct tty *)))enodev)

int	nullioctl __P((struct tty *tp, int cmd, caddr_t data,
			int flag, struct proc *p));

#ifndef MAXLDISC
#define MAXLDISC 8
#endif

#define NODISC(n) \
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl, \
	  ttyerrinput, ttyerrstart, nullmodem },

struct	linesw linesw[MAXLDISC] =
{
	{ ttyopen, ttylclose, ttread, ttwrite, nullioctl,
	  ttyinput, ttstart, ttymodem },		/* 0- termios */

	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },	/* 1- defunct */

#ifdef COMPAT_43
	{ ttyopen, ttylclose, ttread, ttwrite, nullioctl,
	  ttyinput, ttstart, ttymodem },		/* 2- NTTYDISC */
#else
	{ ttynodisc, ttyerrclose, ttyerrio, ttyerrio, nullioctl,
	  ttyerrinput, ttyerrstart, nullmodem },
#endif

	NODISC(3)		/* TABLDISC */
	NODISC(4)		/* SLIPDISC */
	NODISC(5)		/* PPPDISC */
	NODISC(6)		/* loadable */
	NODISC(7)		/* loadable */
};

int	nlinesw = sizeof (linesw) / sizeof (linesw[0]);

static struct linesw nodisc = 
{
	ttynodisc,
	ttyerrclose,
	ttyerrio,
	ttyerrio,
	nullioctl,
	ttyerrinput,
	ttyerrstart,
	nullmodem
};

#define LOADABLE_LDISC 6
/*
 * ldisc_register: Register a line discipline.
 *
 * discipline: Index for discipline to load, or LDISC_LOAD for us to choose.
 * linesw_p:   Pointer to linesw_p.
 *
 * Returns: Index used or -1 on failure.
 */
int
ldisc_register(discipline, linesw_p)
	int discipline;
	struct linesw *linesw_p;
{
	int slot = -1;

	if (discipline == LDISC_LOAD) {
		int i;
		for (i = LOADABLE_LDISC; i < MAXLDISC; i++)
			if (bcmp(linesw + i, &nodisc, sizeof(nodisc)) == 0) {
				slot = i;
			}
	}
	else if (discipline >= 0 && discipline < MAXLDISC) {
		slot = discipline;
	}

	if (slot != -1 && linesw_p)
		linesw[slot] = *linesw_p;

	return slot;
}

/*
 * ldisc_deregister: Deregister a line discipline obtained with
 * ldisc_register.  Can only deregister "loadable" ones now.
 *
 * discipline: Index for discipline to unload.
 */
void
ldisc_deregister(discipline)
	int discipline;
{
	if (discipline >= LOADABLE_LDISC && discipline < MAXLDISC) {
		linesw[discipline] = nodisc;
	}
}


/*
 * Do nothing specific version of line
 * discipline specific ioctl command.
 */
/*ARGSUSED*/
int
nullioctl(tp, cmd, data, flags, p)
	struct tty *tp;
	int cmd;
	char *data;
	int flags;
	struct proc *p;
{

#ifdef lint
	tp = tp; data = data; flags = flags; p = p;
#endif
	return (-1);
}
