/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)ttinit.c	3.27 (Berkeley) 8/12/90";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

int tt_h19();
int tt_h29();
int tt_f100();
int tt_tvi925();
int tt_wyse75();
int tt_wyse60();
int tt_zapple();
int tt_zentec();
int tt_generic();
struct tt_tab tt_tab[] = {
	{ "h19",	3, tt_h19 },
	{ "h29",	3, tt_h29 },
	{ "f100",	4, tt_f100 },
	{ "tvi925",	6, tt_tvi925 },
	{ "wyse75",	6, tt_wyse75 },
	{ "wyse60",	6, tt_wyse60 },
	{ "w60",	3, tt_wyse60 },
	{ "zapple",	6, tt_zapple },
	{ "zentec",	6, tt_zentec },
	{ "generic",	0, tt_generic },
	0
};

ttinit()
{
	int i;
	register struct tt_tab *tp;
	register char *p, *q;
	register char *t;
	int ttflush();

	tt_strp = tt_strings;

	/*
	 * Set output buffer size to about 1 second of output time.
	 */
	i = MIN(wwbaud/10, 512);
	if ((tt_ob = malloc((unsigned) i)) == 0) {
		wwerrno = WWE_NOMEM;
		return -1;
	}
	tt_obp = tt_ob;
	tt_obe = tt_ob + i;

	/*
	 * Use the standard name of the terminal (i.e. the second
	 * name in termcap).
	 */
	for (p = wwtermcap; *p && *p != '|' && *p != ':'; p++)
		;
	if (*p == '|')
		p++;
	for (q = p; *q && *q != '|' && *q != ':'; q++)
		;
	if (q != p && (t = malloc((unsigned) (q - p + 1))) != 0) {
		wwterm = t;
		while (p < q)
			*t++ = *p++;
		*t = 0;
	}
	for (tp = tt_tab; tp->tt_name != 0; tp++)
		if (strncmp(tp->tt_name, wwterm, tp->tt_len) == 0)
			break;
	if (tp->tt_name == 0) {
		wwerrno = WWE_BADTERM;
		return -1;
	}
	if ((*tp->tt_func)() < 0) {
		wwerrno = WWE_CANTDO;
		return -1;
	}
	if (wwgetttysize(0, &tt.tt_nrow, &tt.tt_ncol) < 0)
		return -1;
	tt.tt_scroll_top = 0;
	tt.tt_scroll_bot = tt.tt_nrow - 1;
	tt.tt_flush = ttflush;
	return 0;
}
