/*
 * Copyright (c) 1987 The Regents of the University of California.
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
static char sccsid[] = "@(#)fortran.c	5.5 (Berkeley) 2/26/91";
#endif /* not lint */

#include <stdio.h>
#include <string.h>
#include "ctags.h"

static void takeprec();

char *lbp;				/* line buffer pointer */

PF_funcs()
{
	register bool	pfcnt;		/* pascal/fortran functions found */
	register char	*cp;
	char	tok[MAXTOKEN],
		*gettoken();

	for (pfcnt = NO;;) {
		lineftell = ftell(inf);
		if (!fgets(lbuf,sizeof(lbuf),inf))
			return(pfcnt);
		++lineno;
		lbp = lbuf;
		if (*lbp == '%')	/* Ratfor escape to fortran */
			++lbp;
		for (;isspace(*lbp);++lbp);
		if (!*lbp)
			continue;
		switch (*lbp | ' ') {	/* convert to lower-case */
		case 'c':
			if (cicmp("complex") || cicmp("character"))
				takeprec();
			break;
		case 'd':
			if (cicmp("double")) {
				for (;isspace(*lbp);++lbp);
				if (!*lbp)
					continue;
				if (cicmp("precision"))
					break;
				continue;
			}
			break;
		case 'i':
			if (cicmp("integer"))
				takeprec();
			break;
		case 'l':
			if (cicmp("logical"))
				takeprec();
			break;
		case 'r':
			if (cicmp("real"))
				takeprec();
			break;
		}
		for (;isspace(*lbp);++lbp);
		if (!*lbp)
			continue;
		switch (*lbp | ' ') {
		case 'f':
			if (cicmp("function"))
				break;
			continue;
		case 'p':
			if (cicmp("program") || cicmp("procedure"))
				break;
			continue;
		case 's':
			if (cicmp("subroutine"))
				break;
		default:
			continue;
		}
		for (;isspace(*lbp);++lbp);
		if (!*lbp)
			continue;
		for (cp = lbp + 1;*cp && intoken(*cp);++cp);
		if (cp = lbp + 1)
			continue;
		*cp = EOS;
		(void)strcpy(tok,lbp);
		getline();			/* process line for ex(1) */
		pfnote(tok,lineno);
		pfcnt = YES;
	}
	/*NOTREACHED*/
}

/*
 * cicmp --
 *	do case-independent strcmp
 */
cicmp(cp)
	register char	*cp;
{
	register int	len;
	register char	*bp;

	for (len = 0,bp = lbp;*cp && (*cp &~ ' ') == (*bp++ &~ ' ');
	    ++cp,++len);
	if (!*cp) {
		lbp += len;
		return(YES);
	}
	return(NO);
}

static void
takeprec()
{
	for (;isspace(*lbp);++lbp);
	if (*lbp == '*') {
		for (++lbp;isspace(*lbp);++lbp);
		if (!isdigit(*lbp))
			--lbp;			/* force failure */
		else
			while (isdigit(*++lbp));
	}
}
