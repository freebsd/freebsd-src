/*-
 * Copyright (c) 1992, 1993
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
static char sccsid[] = "@(#)v_ch.c	8.2 (Berkeley) 12/20/93";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>

#include "vi.h"
#include "vcmd.h"

#define	NOPREV {							\
	msgq(sp, M_BERR, "No previous F, f, T or t search.");		\
	return (1);							\
}

#define	NOTFOUND(ch) {							\
	msgq(sp, M_BERR, "%s not found.", charname(sp, ch));		\
	return (1);							\
}

/*
 * v_chrepeat -- [count];
 *	Repeat the last F, f, T or t search.
 */
int
v_chrepeat(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	vp->character = sp->lastckey;

	switch (sp->csearchdir) {
	case CNOTSET:
		NOPREV;
	case FSEARCH:
		return (v_chF(sp, ep, vp, fm, tm, rp));
	case fSEARCH:
		return (v_chf(sp, ep, vp, fm, tm, rp));
	case TSEARCH:
		return (v_chT(sp, ep, vp, fm, tm, rp));
	case tSEARCH:
		return (v_cht(sp, ep, vp, fm, tm, rp));
	default:
		abort();
	}
	/* NOTREACHED */
}

/*
 * v_chrrepeat -- [count],
 *	Repeat the last F, f, T or t search in the reverse direction.
 */
int
v_chrrepeat(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;
	enum cdirection savedir;

	vp->character = sp->lastckey;
	savedir = sp->csearchdir;

	switch (sp->csearchdir) {
	case CNOTSET:
		NOPREV;
	case FSEARCH:
		rval = v_chf(sp, ep, vp, fm, tm, rp);
		break;
	case fSEARCH:
		rval = v_chF(sp, ep, vp, fm, tm, rp);
		break;
	case TSEARCH:
		rval = v_cht(sp, ep, vp, fm, tm, rp);
		break;
	case tSEARCH:
		rval = v_chT(sp, ep, vp, fm, tm, rp);
		break;
	default:
		abort();
	}
	sp->csearchdir = savedir;
	return (rval);
}

/*
 * v_cht -- [count]tc
 *	Search forward in the line for the next occurrence of the character.
 *	Place the cursor on it if a motion command, to its left if its not.
 */
int
v_cht(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	rval = v_chf(sp, ep, vp, fm, tm, rp);
	if (!rval)
		--rp->cno;	/* XXX: Motion interaction with v_chf. */
	sp->csearchdir = tSEARCH;
	return (rval);
}
	
/*
 * v_chf -- [count]fc
 *	Search forward in the line for the next occurrence of the character.
 *	Place the cursor to its right if a motion command, on it if its not.
 */
int
v_chf(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	size_t len;
	recno_t lno;
	u_long cnt;
	int key;
	char *endp, *p, *startp;

	/*
	 * !!!
	 * If it's a dot command, it doesn't reset the key for which
	 * we're searching, e.g. in "df1|f2|.|;", the ';' searches
	 * for a '2'.
	 */
	key = vp->character;
	if (!F_ISSET(vp, VC_ISDOT))
		sp->lastckey = key;
	sp->csearchdir = fSEARCH;

	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			NOTFOUND(key);
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		NOTFOUND(key);

	startp = p;
	endp = p + len;
	p += fm->cno;
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		while (++p < endp && *p != key);
		if (p == endp)
			NOTFOUND(key);
	}
	rp->lno = fm->lno;
	rp->cno = p - startp;
	if (F_ISSET(vp, VC_C | VC_D | VC_Y))
		++rp->cno;
	return (0);
}

/*
 * v_chT -- [count]Tc
 *	Search backward in the line for the next occurrence of the character.
 *	Place the cursor to its right.
 */
int
v_chT(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	int rval;

	rval = v_chF(sp, ep, vp, fm, tm, rp);
	if (!rval)
		++rp->cno;
	sp->csearchdir = TSEARCH;
	return (0);
}

/*
 * v_chF -- [count]Fc
 *	Search backward in the line for the next occurrence of the character.
 *	Place the cursor on it.
 */
int
v_chF(sp, ep, vp, fm, tm, rp)
	SCR *sp;
	EXF *ep;
	VICMDARG *vp;
	MARK *fm, *tm, *rp;
{
	recno_t lno;
	size_t len;
	u_long cnt;
	int key;
	char *p, *endp;

	/*
	 * !!!
	 * If it's a dot command, it doesn't reset the key for which
	 * we're searching, e.g. in "df1|f2|.|;", the ';' searches
	 * for a '2'.
	 */
	key = vp->character;
	if (!F_ISSET(vp, VC_ISDOT))
		sp->lastckey = key;
	sp->csearchdir = FSEARCH;

	if ((p = file_gline(sp, ep, fm->lno, &len)) == NULL) {
		if (file_lline(sp, ep, &lno))
			return (1);
		if (lno == 0)
			NOTFOUND(key);
		GETLINE_ERR(sp, fm->lno);
		return (1);
	}

	if (len == 0)
		NOTFOUND(key);

	endp = p - 1;
	p += fm->cno;
	for (cnt = F_ISSET(vp, VC_C1SET) ? vp->count : 1; cnt--;) {
		while (--p > endp && *p != key);
		if (p == endp)
			NOTFOUND(key);
	}
	rp->lno = fm->lno;
	rp->cno = (p - endp) - 1;
	return (0);
}
