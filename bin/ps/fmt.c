/*-
 * Copyright (c) 1992, 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)fmt.c	8.4 (Berkeley) 4/15/94";
#endif /* not lint */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <vis.h>
#include "ps.h"

static char *cmdpart __P((char *));
static char *shquote __P((char **));

/*
 * XXX
 * This is a stub until marc does the real one.
 */
static char *
shquote(argv)
	char **argv;
{
	char **p, *dst, *src;
	static char buf[4096];		/* XXX */

	if (*argv == 0) {
		buf[0] = 0;
		return (buf);
	}
	dst = buf;
	for (p = argv; (src = *p++) != 0; ) {
		if (*src == 0)
			continue;
		strvis(dst, src, VIS_NL | VIS_CSTYLE);
		while (*dst)
			dst++;
		*dst++ = ' ';
	}
	*dst = '\0';
	return (buf);
}

static char *
cmdpart(arg0)
	char *arg0;
{
	char *cp;

	return ((cp = strrchr(arg0, '/')) != NULL ? cp + 1 : arg0);
}

char *
fmt_argv(argv, cmd, maxlen)
	char **argv;
	char *cmd;
	int maxlen;
{
	int len;
	char *ap, *cp;

	if (argv == 0 || argv[0] == 0) {
		if (cmd == NULL)
			return ("");
		ap = NULL;
		len = maxlen + 3;
	} else {
		ap = shquote(argv);
		len = strlen(ap) + maxlen + 4;
	}
	if ((cp = malloc(len)) == NULL)
		return (NULL);
	if (ap == NULL)
		sprintf(cp, "(%.*s)", maxlen, cmd);
	else if (strncmp(cmdpart(argv[0]), cmd, maxlen) != 0)
		sprintf(cp, "%s (%.*s)", ap, maxlen, cmd);
	else
		(void) strcpy(cp, ap);
	return (cp);
}
