/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 */

#ifndef lint
static char sccsid[] = "@(#)wwopen.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "ww.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <fcntl.h>

struct ww *
wwopen(flags, nrow, ncol, row, col, nline)
{
	register struct ww *w;
	register i, j;
	char m;
	short nvis;

	w = (struct ww *)calloc(sizeof (struct ww), 1);
	if (w == 0) {
		wwerrno = WWE_NOMEM;
		goto bad;
	}
	w->ww_pty = -1;
	w->ww_socket = -1;

	for (i = 0; i < NWW && wwindex[i] != 0; i++)
		;
	if (i >= NWW) {
		wwerrno = WWE_TOOMANY;
		goto bad;
	}
	w->ww_index = i;

	if (nline < nrow)
		nline = nrow;

	w->ww_w.t = row;
	w->ww_w.b = row + nrow;
	w->ww_w.l = col;
	w->ww_w.r = col + ncol;
	w->ww_w.nr = nrow;
	w->ww_w.nc = ncol;

	w->ww_b.t = row;
	w->ww_b.b = row + nline;
	w->ww_b.l = col;
	w->ww_b.r = col + ncol;
	w->ww_b.nr = nline;
	w->ww_b.nc = ncol;

	w->ww_i.t = MAX(w->ww_w.t, 0);
	w->ww_i.b = MIN(w->ww_w.b, wwnrow);
	w->ww_i.l = MAX(w->ww_w.l, 0);
	w->ww_i.r = MIN(w->ww_w.r, wwncol);
	w->ww_i.nr = w->ww_i.b - w->ww_i.t;
	w->ww_i.nc = w->ww_i.r - w->ww_i.l;

	w->ww_cur.r = w->ww_w.t;
	w->ww_cur.c = w->ww_w.l;

	if (flags & WWO_PTY) {
		if (wwgetpty(w) < 0)
			goto bad;
		w->ww_ispty = 1;
	} else if (flags & WWO_SOCKET) {
		int d[2];
		if (socketpair(AF_UNIX, SOCK_STREAM, PF_UNSPEC, d) < 0) {
			wwerrno = WWE_SYS;
			goto bad;
		}
		(void) fcntl(d[0], F_SETFD, 1);
		(void) fcntl(d[1], F_SETFD, 1);
		w->ww_pty = d[0];
		w->ww_socket = d[1];
	}
	if (flags & (WWO_PTY|WWO_SOCKET)) {
		if ((w->ww_ob = malloc(512)) == 0) {
			wwerrno = WWE_NOMEM;
			goto bad;
		}
		w->ww_obe = w->ww_ob + 512;
		w->ww_obp = w->ww_obq = w->ww_ob;
	}

	w->ww_win = wwalloc(w->ww_w.t, w->ww_w.l,
		w->ww_w.nr, w->ww_w.nc, sizeof (char));
	if (w->ww_win == 0)
		goto bad;
	m = 0;
	if (flags & WWO_GLASS)
		m |= WWM_GLS;
	if (flags & WWO_REVERSE)
		if (wwavailmodes & WWM_REV)
			m |= WWM_REV;
		else
			flags &= ~WWO_REVERSE;
	for (i = w->ww_w.t; i < w->ww_w.b; i++)
		for (j = w->ww_w.l; j < w->ww_w.r; j++)
			w->ww_win[i][j] = m;

	if (flags & WWO_FRAME) {
		w->ww_fmap = wwalloc(w->ww_w.t, w->ww_w.l,
			w->ww_w.nr, w->ww_w.nc, sizeof (char));
		if (w->ww_fmap == 0)
			goto bad;
		for (i = w->ww_w.t; i < w->ww_w.b; i++)
			for (j = w->ww_w.l; j < w->ww_w.r; j++)
				w->ww_fmap[i][j] = 0;
	}

	w->ww_buf = (union ww_char **)
		wwalloc(w->ww_b.t, w->ww_b.l,
			w->ww_b.nr, w->ww_b.nc, sizeof (union ww_char));
	if (w->ww_buf == 0)
		goto bad;
	for (i = w->ww_b.t; i < w->ww_b.b; i++)
		for (j = w->ww_b.l; j < w->ww_b.r; j++)
			w->ww_buf[i][j].c_w = ' ';

	w->ww_nvis = (short *)malloc((unsigned) w->ww_w.nr * sizeof (short));
	if (w->ww_nvis == 0) {
		wwerrno = WWE_NOMEM;
		goto bad;
	}
	w->ww_nvis -= w->ww_w.t;
	nvis = m ? 0 : w->ww_w.nc;
	for (i = w->ww_w.t; i < w->ww_w.b; i++)
		w->ww_nvis[i] = nvis;

	w->ww_state = WWS_INITIAL;
	w->ww_oflags = flags;
	return wwindex[w->ww_index] = w;
bad:
	if (w != 0) {
		if (w->ww_win != 0)
			wwfree(w->ww_win, w->ww_w.t);
		if (w->ww_fmap != 0)
			wwfree(w->ww_fmap, w->ww_w.t);
		if (w->ww_buf != 0)
			wwfree((char **)w->ww_buf, w->ww_b.t);
		if (w->ww_nvis != 0)
			free((char *)(w->ww_nvis + w->ww_w.t));
		if (w->ww_ob != 0)
			free(w->ww_ob);
		if (w->ww_pty >= 0)
			(void) close(w->ww_pty);
		if (w->ww_socket >= 0)
			(void) close(w->ww_socket);
		free((char *)w);
	}
	return 0;
}
