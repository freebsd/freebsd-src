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
static char sccsid[] = "@(#)wwdump.c	8.1 (Berkeley) 6/6/93";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

static char cmap[] = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ";

wwdumpwin(w)
register struct ww *w;
{
	register i, j;

	tt.tt_nmodes = 0;
	(*tt.tt_clear)();
	for (i = w->ww_i.t; i < w->ww_i.b; i++) {
		(*tt.tt_move)(i, w->ww_i.l);
		for (j = w->ww_i.l; j < w->ww_i.r; j++)
			(*tt.tt_putc)(w->ww_win[i][j] & WWM_GLS ? 'G' : ' ');
	}
}

wwdumpnvis(w)
register struct ww *w;
{
	register i;
	char buf[20];

	tt.tt_nmodes = 0;
	(*tt.tt_clear)();
	for (i = w->ww_i.t; i < w->ww_i.b; i++) {
		(*tt.tt_move)(i, w->ww_i.l);
		(void) sprintf(buf, "%d", w->ww_nvis[i]);
		(*tt.tt_write)(buf, strlen(buf));
	}
}

wwdumpsmap()
{
	register i, j;

	tt.tt_nmodes = 0;
	(*tt.tt_clear)();
	for (i = 0; i < wwnrow; i++) {
		(*tt.tt_move)(i, 0);
		for (j = 0; j < wwncol; j++)
			(*tt.tt_putc)(cmap[wwsmap[i][j]]);
	}
}

wwdumpns()
{
	register i, j;

	(*tt.tt_clear)();
	for (i = 0; i < wwnrow; i++) {
		(*tt.tt_move)(i, 0);
		for (j = 0; j < wwncol; j++) {
			tt.tt_nmodes = wwns[i][j].c_m & tt.tt_availmodes;
			(*tt.tt_putc)(wwns[i][j].c_c);
		}
	}
}

wwdumpos()
{
	register i, j;

	(*tt.tt_clear)();
	for (i = 0; i < wwnrow; i++) {
		(*tt.tt_move)(i, 0);
		for (j = 0; j < wwncol; j++) {
			tt.tt_nmodes = wwos[i][j].c_m & tt.tt_availmodes;
			(*tt.tt_putc)(wwns[i][j].c_c);
		}
	}
}
