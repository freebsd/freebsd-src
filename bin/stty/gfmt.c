/*-
 * Copyright (c) 1991 The Regents of the University of California.
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
 */

#ifndef lint
static char sccsid[] = "@(#)gfmt.c	5.4 (Berkeley) 6/10/91";
#endif /* not lint */

#include <sys/types.h>
#include <stdio.h>
#include <string.h>
#include "stty.h"
#include "extern.h"

static void gerr __P((char *));

void
gprint(tp, wp, ldisc)
	struct termios *tp;
	struct winsize *wp;
	int ldisc;
{
	register struct cchar *cp;

	(void)printf("gfmt1:cflag=%x:iflag=%x:lflag=%x:oflag=%x:",
	    tp->c_cflag, tp->c_iflag, tp->c_lflag, tp->c_oflag);
	for (cp = cchars1; cp->name; ++cp)
		(void)printf("%s=%x:", cp->name, tp->c_cc[cp->sub]);
	(void)printf("ispeed=%d:ospeed=%d\n", cfgetispeed(tp), cfgetospeed(tp));
}

void
gread(tp, s) 
	register struct termios *tp;
	char *s;
{
	register char *ep, *p;
	long tmp;

#define	CHK(s)	(*p == s[0] && !strcmp(p, s))
	if (!(s = index(s, ':')))
		gerr(NULL);
	for (++s; s;) {
		p = strsep(&s, ":\0");
		if (!p || !*p)
			break;
		if (!(ep = index(p, '=')))
			gerr(p);
		*ep++ = '\0';
		(void)sscanf(ep, "%lx", &tmp);
		if (CHK("cflag")) {
			tp->c_cflag = tmp;
			continue;
		}
		if (CHK("discard")) {
			tp->c_cc[VDISCARD] = tmp;
			continue;
		}
		if (CHK("dsusp")) {
			tp->c_cc[VDSUSP] = tmp;
			continue;
		}
		if (CHK("eof")) {
			tp->c_cc[VEOF] = tmp;
			continue;
		}
		if (CHK("eol")) {
			tp->c_cc[VEOL] = tmp;
			continue;
		}
		if (CHK("eol2")) {
			tp->c_cc[VEOL2] = tmp;
			continue;
		}
		if (CHK("erase")) {
			tp->c_cc[VERASE] = tmp;
			continue;
		}
		if (CHK("iflag")) {
			tp->c_iflag = tmp;
			continue;
		}
		if (CHK("intr")) {
			tp->c_cc[VINTR] = tmp;
			continue;
		}
		if (CHK("ispeed")) {
			(void)sscanf(ep, "%ld", &tmp);
			tp->c_ispeed = tmp;
			continue;
		}
		if (CHK("kill")) {
			tp->c_cc[VKILL] = tmp;
			continue;
		}
		if (CHK("lflag")) {
			tp->c_lflag = tmp;
			continue;
		}
		if (CHK("lnext")) {
			tp->c_cc[VLNEXT] = tmp;
			continue;
		}
		if (CHK("oflag")) {
			tp->c_oflag = tmp;
			continue;
		}
		if (CHK("ospeed")) {
			(void)sscanf(ep, "%ld", &tmp);
			tp->c_ospeed = tmp;
			continue;
		}
		if (CHK("quit")) {
			tp->c_cc[VQUIT] = tmp;
			continue;
		}
		if (CHK("reprint")) {
			tp->c_cc[VREPRINT] = tmp;
			continue;
		}
		if (CHK("start")) {
			tp->c_cc[VSTART] = tmp;
			continue;
		}
		if (CHK("status")) {
			tp->c_cc[VSTATUS] = tmp;
			continue;
		}
		if (CHK("stop")) {
			tp->c_cc[VSTOP] = tmp;
			continue;
		}
		if (CHK("susp")) {
			tp->c_cc[VSUSP] = tmp;
			continue;
		}
		if (CHK("vmin")) {
			(void)sscanf(ep, "%ld", &tmp);
			tp->c_cc[VMIN] = tmp;
			continue;
		}
		if (CHK("vtime")) {
			(void)sscanf(ep, "%ld", &tmp);
			tp->c_cc[VTIME] = tmp;
			continue;
		}
		if (CHK("werase")) {
			tp->c_cc[VWERASE] = tmp;
			continue;
		}
		gerr(p);
	}
}

static void
gerr(s)
	char *s;
{
	if (s)
		err("illegal gfmt1 option -- %s", s);
	else
		err("illegal gfmt1 option");
}
