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
static char sccsid[] = "@(#)wwflush.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] = "@(#)$FreeBSD$";
#endif /* not lint */

#include "ww.h"
#include "tt.h"
#include <signal.h>

wwflush()
{
	register row, col;

	if ((row = wwcursorrow) < 0)
		row = 0;
	else if (row >= wwnrow)
		row = wwnrow - 1;
	if ((col = wwcursorcol) < 0)
		col = 0;
	else if (col >= wwncol)
		col = wwncol - 1;
	xxmove(row, col);
	if (wwdocheckpoint) {
		xxflush(0);
		wwcheckpoint();
	} else
		xxflush(1);
}

wwcheckpoint()
{
	int s = sigblock(sigmask(SIGALRM) | sigmask(SIGIO));

	tt.tt_ack = 0;
	do {
		(*tt.tt_checkpoint)();
#ifndef OLD_TTY
		(void) tcdrain(1);
#endif
		(void) alarm(3);
		for (wwdocheckpoint = 0; !wwdocheckpoint && tt.tt_ack == 0;)
			(void) sigpause(s);
	} while (tt.tt_ack == 0);
	(void) alarm(0);
	wwdocheckpoint = 0;
	if (tt.tt_ack < 0) {
		wwcopyscreen(wwcs, wwos);
		(void) alarm(1);
		wwreset();
		wwupdate();
		wwflush();
	} else {
		wwcopyscreen(wwos, wwcs);
		(void) alarm(3);
	}
	(void) sigsetmask(s);
}

wwcopyscreen(s1, s2)
	register union ww_char **s1, **s2;
{
	register i;
	register s = wwncol * sizeof **s1;

	for (i = wwnrow; --i >= 0;)
		bcopy((char *) *s1++, (char *) *s2++, s);
}

void
wwalarm()
{
	wwdocheckpoint = 1;
}
