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
static char sccsid[] = "@(#)ttoutput.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "ww.h"
#include "tt.h"
#include <errno.h>
#include <unistd.h>

/*
 * Buffered output package.
 * We need this because stdio fails on non-blocking writes.
 */

ttflush()
{
	register char *p;
	register n = tt_obp - tt_ob;

	if (n == 0)
		return;
	if (tt.tt_checksum)
		(*tt.tt_checksum)(tt_ob, n);
	if (tt.tt_flush) {
		(*tt.tt_flush)();
		return;
	}
	wwnflush++;
	for (p = tt_ob; p < tt_obp;) {
		wwnwr++;
		n = write(STDOUT_FILENO, p, tt_obp - p);
		if (n < 0) {
			wwnwre++;
			if (errno != EWOULDBLOCK) {
				/* can't deal with this */
				p = tt_obp;
			}
		} else if (n == 0) {
			/* what to do? */
			wwnwrz++;
		} else {
			wwnwrc += n;
			p += n;
		}
	}
	tt_obp = tt_ob;
}

ttputs(s)
register char *s;
{
	while (*s)
		ttputc(*s++);
}

ttwrite(s, n)
	register char *s;
	register n;
{
	switch (n) {
	case 0:
		break;
	case 1:
		ttputc(*s);
		break;
	case 2:
		if (tt_obe - tt_obp < 2)
			ttflush();
		*tt_obp++ = *s++;
		*tt_obp++ = *s;
		break;
	case 3:
		if (tt_obe - tt_obp < 3)
			ttflush();
		*tt_obp++ = *s++;
		*tt_obp++ = *s++;
		*tt_obp++ = *s;
		break;
	case 4:
		if (tt_obe - tt_obp < 4)
			ttflush();
		*tt_obp++ = *s++;
		*tt_obp++ = *s++;
		*tt_obp++ = *s++;
		*tt_obp++ = *s;
		break;
	case 5:
		if (tt_obe - tt_obp < 5)
			ttflush();
		*tt_obp++ = *s++;
		*tt_obp++ = *s++;
		*tt_obp++ = *s++;
		*tt_obp++ = *s++;
		*tt_obp++ = *s;
		break;
	default:
		while (n > 0) {
			register m;

			while ((m = tt_obe - tt_obp) == 0)
				ttflush();
			if ((m = tt_obe - tt_obp) > n)
				m = n;
			bcopy(s, tt_obp, m);
			tt_obp += m;
			s += m;
			n -= m;
		}
	}
}
