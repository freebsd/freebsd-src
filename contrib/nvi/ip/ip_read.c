/*-
 * Copyright (c) 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 */

#include "config.h"

#ifndef lint
static const char sccsid[] = "@(#)ip_read.c	8.3 (Berkeley) 9/24/96";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>

#include <bitstring.h>
#include <stdio.h>
#include <termios.h>
#include <time.h>
 
#include "../common/common.h"
#include "../ex/script.h"
#include "ip.h"

extern GS *__global_list;

typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_TIMEOUT } input_t;

static input_t	ip_read __P((SCR *, IP_PRIVATE *, struct timeval *));
static int	ip_resize __P((SCR *, size_t, size_t));
static int	ip_trans __P((SCR *, IP_PRIVATE *, EVENT *));

/*
 * ip_event --
 *	Return a single event.
 *
 * PUBLIC: int ip_event __P((SCR *, EVENT *, u_int32_t, int));
 */
int
ip_event(sp, evp, flags, ms)
	SCR *sp;
	EVENT *evp;
	u_int32_t flags;
	int ms;
{
	IP_PRIVATE *ipp;
	struct timeval t, *tp;

	if (LF_ISSET(EC_INTERRUPT)) {		/* XXX */
		evp->e_event = E_TIMEOUT;
		return (0);
	}

	ipp = sp == NULL ? GIPP(__global_list) : IPP(sp);

	/* Discard the last command. */
	if (ipp->iskip != 0) {
		ipp->iblen -= ipp->iskip;
		memmove(ipp->ibuf, ipp->ibuf + ipp->iskip, ipp->iblen);
		ipp->iskip = 0;
	}

	/* Set timer. */
	if (ms == 0)
		tp = NULL;
	else {
		t.tv_sec = ms / 1000;
		t.tv_usec = (ms % 1000) * 1000;
		tp = &t;
	}

	/* Read input events. */
	for (;;) {
		switch (ip_read(sp, ipp, tp)) {
		case INP_OK:
			if (!ip_trans(sp, ipp, evp))
				continue;
			break;
		case INP_EOF:
			evp->e_event = E_EOF;
			break;
		case INP_ERR:
			evp->e_event = E_ERR;
			break;
		case INP_TIMEOUT:
			evp->e_event = E_TIMEOUT;
			break;
		default:
			abort();
		}
		break;
	}
	return (0);
}

/*
 * ip_read --
 *	Read characters from the input.
 */
static input_t
ip_read(sp, ipp, tp)
	SCR *sp;
	IP_PRIVATE *ipp;
	struct timeval *tp;
{
	struct timeval poll;
	GS *gp;
	SCR *tsp;
	fd_set rdfd;
	input_t rval;
	size_t blen;
	int maxfd, nr;
	char *bp;

	gp = sp == NULL ? __global_list : sp->gp;
	bp = ipp->ibuf + ipp->iblen;
	blen = sizeof(ipp->ibuf) - ipp->iblen;

	/*
	 * 1: A read with an associated timeout, e.g., trying to complete
	 *    a map sequence.  If input exists, we fall into #2.
	 */
	FD_ZERO(&rdfd);
	poll.tv_sec = 0;
	poll.tv_usec = 0;
	if (tp != NULL) {
		FD_SET(ipp->i_fd, &rdfd);
		switch (select(ipp->i_fd + 1,
		    &rdfd, NULL, NULL, tp == NULL ? &poll : tp)) {
		case 0:
			return (INP_TIMEOUT);
		case -1:
			goto err;
		default:
			break;
		}
	}
	
	/*
	 * 2: Wait for input.
	 *
	 * Select on the command input and scripting window file descriptors.
	 * It's ugly that we wait on scripting file descriptors here, but it's
	 * the only way to keep from locking out scripting windows.
	 */
	if (sp != NULL && F_ISSET(gp, G_SCRWIN)) {
loop:		FD_ZERO(&rdfd);
		FD_SET(ipp->i_fd, &rdfd);
		maxfd = ipp->i_fd;
		for (tsp = gp->dq.cqh_first;
		    tsp != (void *)&gp->dq; tsp = tsp->q.cqe_next)
			if (F_ISSET(sp, SC_SCRIPT)) {
				FD_SET(sp->script->sh_master, &rdfd);
				if (sp->script->sh_master > maxfd)
					maxfd = sp->script->sh_master;
			}
		switch (select(maxfd + 1, &rdfd, NULL, NULL, NULL)) {
		case 0:
			abort();
		case -1:
			goto err;
		default:
			break;
		}
		if (!FD_ISSET(ipp->i_fd, &rdfd)) {
			if (sscr_input(sp))
				return (INP_ERR);
			goto loop;
		}
	}

	/*
	 * 3: Read the input.
	 */
	switch (nr = read(ipp->i_fd, bp, blen)) {
	case  0:				/* EOF. */
		rval = INP_EOF;
		break;
	case -1:				/* Error or interrupt. */
err:		rval = INP_ERR;
		msgq(sp, M_SYSERR, "input");
		break;
	default:				/* Input characters. */
		ipp->iblen += nr;
		rval = INP_OK;
		break;
	}
	return (rval);
}

/*
 * ip_trans --
 *	Translate messages into events.
 */
static int
ip_trans(sp, ipp, evp)
	SCR *sp;
	IP_PRIVATE *ipp;
	EVENT *evp;
{
	u_int32_t val1, val2;

	switch (ipp->ibuf[0]) {
	case IPO_EOF:
		evp->e_event = E_EOF;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case IPO_ERR:
		evp->e_event = E_ERR;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case IPO_INTERRUPT:
		evp->e_event = E_INTERRUPT;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case IPO_QUIT:
		evp->e_event = E_QUIT;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case IPO_RESIZE:
		if (ipp->iblen < IPO_CODE_LEN + IPO_INT_LEN * 2)
			return (0);
		evp->e_event = E_WRESIZE;
		memcpy(&val1, ipp->ibuf + IPO_CODE_LEN, IPO_INT_LEN);
		val1 = ntohl(val1);
		memcpy(&val2,
		    ipp->ibuf + IPO_CODE_LEN + IPO_INT_LEN, IPO_INT_LEN);
		val2 = ntohl(val2);
		ip_resize(sp, val1, val2);
		ipp->iskip = IPO_CODE_LEN + IPO_INT_LEN * 2;
		return (1);
	case IPO_SIGHUP:
		evp->e_event = E_SIGHUP;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case IPO_SIGTERM:
		evp->e_event = E_SIGTERM;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	case IPO_STRING:
		evp->e_event = E_STRING;
string:		if (ipp->iblen < IPO_CODE_LEN + IPO_INT_LEN)
			return (0);
		memcpy(&val1, ipp->ibuf + IPO_CODE_LEN, IPO_INT_LEN);
		val1 = ntohl(val1);
		if (ipp->iblen < IPO_CODE_LEN + IPO_INT_LEN + val1)
			return (0);
		ipp->iskip = IPO_CODE_LEN + IPO_INT_LEN + val1;
		evp->e_csp = ipp->ibuf + IPO_CODE_LEN + IPO_INT_LEN;
		evp->e_len = val1;
		return (1);
	case IPO_WRITE:
		evp->e_event = E_WRITE;
		ipp->iskip = IPO_CODE_LEN;
		return (1);
	default:
		/*
		 * XXX: Protocol is out of sync?
		 */
		abort();
	}
	/* NOTREACHED */
}

/* 
 * ip_resize --
 *	Reset the options for a resize event.
 */
static int
ip_resize(sp, lines, columns)
	SCR *sp;
	size_t lines, columns;
{
	GS *gp;
	ARGS *argv[2], a, b;
	char b1[1024];

	/*
	 * XXX
	 * The IP screen has to know the lines and columns before anything
	 * else happens.  So, we may not have a valid SCR pointer, and we
	 * have to deal with that.
	 */
	if (sp == NULL) {
		gp = __global_list;
		OG_VAL(gp, GO_LINES) = OG_D_VAL(gp, GO_LINES) = lines;
		OG_VAL(gp, GO_COLUMNS) = OG_D_VAL(gp, GO_COLUMNS) = columns;
		return (0);
	}

	a.bp = b1;
	b.bp = NULL;
	a.len = b.len = 0;
	argv[0] = &a;
	argv[1] = &b;

	(void)snprintf(b1, sizeof(b1), "lines=%lu", (u_long)lines);
	a.len = strlen(b1);
	if (opts_set(sp, argv, NULL))
		return (1);
	(void)snprintf(b1, sizeof(b1), "columns=%lu", (u_long)columns);
	a.len = strlen(b1);
	if (opts_set(sp, argv, NULL))
		return (1);
	return (0);
}
