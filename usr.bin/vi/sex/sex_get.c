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
static char sccsid[] = "@(#)sex_get.c	8.12 (Berkeley) 12/9/93";
#endif /* not lint */

#include <sys/types.h>

#include <stdlib.h>
#include <ctype.h>

#include "vi.h"
#include "excmd.h"
#include "sex_screen.h"

static void	repaint __P((SCR *, int, char *, size_t));

#define	DISPLAY(wval, ch, col) {					\
	size_t __len;							\
	int __ch;							\
	if ((__ch = (ch)) == '\t') {					\
		__len = O_VAL(sp, O_TABSTOP) -				\
		    ((col) % O_VAL(sp, O_TABSTOP));			\
		(col) += (wval) = __len;				\
		while (__len--)						\
			putc(' ', stdout);				\
	} else {							\
		(col) += (wval) = cname[(__ch)].len;			\
		(void)fprintf(stdout,					\
		    "%.*s", cname[(__ch)].len, cname[(__ch)].name);	\
	}								\
}

#define	ERASECH {							\
	for (cnt = tp->wd[tp->len]; cnt > 0; --cnt, --col)		\
		(void)fprintf(stdout, "%s", "\b \b");			\
}

/*
 * sex_get --
 *	Fill a buffer from the terminal for ex.
 */
enum input
sex_get(sp, ep, tiqh, prompt, flags)
	SCR *sp;
	EXF *ep;
	TEXTH *tiqh;
	int prompt;
	u_int flags;
{
	enum { Q_NOTSET, Q_THISCHAR } quoted;
	CHNAME const *cname;		/* Character map. */
	TEXT *tp;			/* Input text structures. */
	CH ikey;			/* Input character. */
	size_t col;			/* 0-N: screen column. */
	size_t cnt;
	u_int iflags;			/* Input flags. */
	int rval;

#ifdef DEBUG
	if (LF_ISSET(~TXT_VALID_EX) || !LF_ISSET(TXT_CR))
		abort();
#endif
	/*
	 * Get one TEXT structure with some initial buffer space, reusing
	 * the last one if it's big enough.  (All TEXT bookkeeping fields
	 * default to 0 -- text_init() handles this.)
	 */
	if (tiqh->cqh_first != (void *)tiqh) {
		tp = tiqh->cqh_first;
		if (tp->q.cqe_next != (void *)tiqh || tp->lb_len < 32) {
			text_lfree(tiqh);
			goto newtp;
		}
		tp->len = 0;
	} else {
newtp:		if ((tp = text_init(sp, NULL, 0, 32)) == NULL)
			return (INP_ERR);
		CIRCLEQ_INSERT_HEAD(tiqh, tp, q);
	}

	cname = sp->gp->cname;
	if (LF_ISSET(TXT_PROMPT) && O_ISSET(sp, O_PROMPT)) {
		(void)fprintf(stdout, "%s", cname[prompt].name);
		col = cname[prompt].len;
	} else
		col = 0;

	iflags = LF_ISSET(TXT_MAPCOMMAND | TXT_MAPINPUT);
	for (quoted = Q_NOTSET;;) {
		(void)fflush(stdout);

		if (rval = term_key(sp, &ikey, iflags))
			return (rval);

		BINC_RET(sp, tp->lb, tp->lb_len, tp->len + 1);
		BINC_RET(sp, tp->wd, tp->wd_len, tp->len + 1);

		if (quoted == Q_THISCHAR) {
			ERASECH;
			goto ins_ch;
		}

		switch (ikey.value) {
		case K_CNTRLZ:
			sex_suspend(sp);
			/* FALLTHROUGH */
		case K_CNTRLR:
			repaint(sp, prompt, tp->lb, tp->len);
			break;
		case K_CR:
		case K_NL:
			if (LF_ISSET(TXT_NLECHO)) {
				(void)putc('\r', stdout);
				(void)putc('\n', stdout);
				(void)fflush(stdout);
			}
			/* Terminate with a newline, needed by filter. */
			tp->lb[tp->len] = '\0';
			return (INP_OK);
		case K_VERASE:
			if (tp->len) {
				--tp->len;
				ERASECH;
			}
			break;
		case K_VKILL:
			for (; tp->len; --tp->len)
				ERASECH;
			break;
		case K_VLNEXT:
			(void)fprintf(stdout, "%s%c", cname['^'].name, '\b');
			quoted = Q_THISCHAR;
			break;
		case K_VWERASE:
			/* Move to the last non-space character. */
			while (tp->len)
				if (!isblank(tp->lb[--tp->len])) {
					++tp->len;
					break;
				} else
					ERASECH;

			/* Move to the last space character. */
			while (tp->len)
				if (isblank(tp->lb[--tp->len])) {
					++tp->len;
					break;
				} else
					ERASECH;
			break;
		default:
ins_ch:			tp->lb[tp->len] = ikey.ch;
			DISPLAY(tp->wd[tp->len], ikey.ch, col);
			++tp->len;
			quoted = Q_NOTSET;
			break;
		}
	}
	/* NOTREACHED */
}

/*
 * sex_get_notty --
 *	Fill a buffer from the terminal for ex, but don't echo
 *	input.
 */
enum input
sex_get_notty(sp, ep, tiqh, prompt, flags)
	SCR *sp;
	EXF *ep;
	TEXTH *tiqh;
	int prompt;
	u_int flags;
{
	enum { Q_NOTSET, Q_THISCHAR } quoted;
	CHNAME const *cname;		/* Character map. */
	TEXT *tp;			/* Input text structures. */
	CH ikey;			/* Input character. */
	size_t col;			/* 0-N: screen column. */
	u_int iflags;			/* Input flags. */
	int rval;

#ifdef DEBUG
	if (LF_ISSET(~TXT_VALID_EX) || !LF_ISSET(TXT_CR))
		abort();
#endif
	/*
	 * Get one TEXT structure with some initial buffer space, reusing
	 * the last one if it's big enough.  (All TEXT bookkeeping fields
	 * default to 0 -- text_init() handles this.)
	 */
	if (tiqh->cqh_first != (void *)tiqh) {
		tp = tiqh->cqh_first;
		if (tp->q.cqe_next != (void *)tiqh || tp->lb_len < 32) {
			text_lfree(tiqh);
			goto newtp;
		}
		tp->len = 0;
	} else {
newtp:		if ((tp = text_init(sp, NULL, 0, 32)) == NULL)
			return (INP_ERR);
		CIRCLEQ_INSERT_HEAD(tiqh, tp, q);
	}

	cname = sp->gp->cname;
	col = 0;

	iflags = LF_ISSET(TXT_MAPCOMMAND | TXT_MAPINPUT);
	for (quoted = Q_NOTSET;;) {
		if (rval = term_key(sp, &ikey, iflags))
			return (rval);

		BINC_RET(sp, tp->lb, tp->lb_len, tp->len + 1);
		BINC_RET(sp, tp->wd, tp->wd_len, tp->len + 1);

		if (quoted == Q_THISCHAR)
			goto ins_ch;

		switch (ikey.value) {
		case K_CNTRLZ:
			sex_suspend(sp);
			/* FALLTHROUGH */
		case K_CNTRLR:
			break;
		case K_CR:
		case K_NL:
			/* Terminate with a newline, needed by filter. */
			tp->lb[tp->len] = '\0';
			return (INP_OK);
		case K_VERASE:
			if (tp->len)
				--tp->len;
			break;
		case K_VKILL:
			tp->len = 0;
			break;
		case K_VLNEXT:
			quoted = Q_THISCHAR;
			break;
		case K_VWERASE:
			/* Move to the last non-space character. */
			while (tp->len)
				if (!isblank(tp->lb[--tp->len])) {
					++tp->len;
					break;
				}

			/* Move to the last space character. */
			while (tp->len)
				if (isblank(tp->lb[--tp->len])) {
					++tp->len;
					break;
				}
			break;
		default:
ins_ch:			tp->lb[tp->len] = ikey.ch;
			++tp->len;
			quoted = Q_NOTSET;
			break;
		}
	}
	/* NOTREACHED */
}

/*
 * repaint --
 *	Repaint the line.
 */
static void
repaint(sp, prompt, p, len)
	SCR *sp;
	int prompt;
	char *p;
	size_t len;
{
	CHNAME const *cname;
	size_t col;
	u_char width;

	cname = sp->gp->cname;

	(void)putc('\n', stdout);
	if (prompt && O_ISSET(sp, O_PROMPT)) {	/* Display prompt. */
		(void)fprintf(stdout, "%s", cname[prompt].name);
		col = cname[prompt].len;
	} else
		col = 0;

	while (len--)
		DISPLAY(width, *p++, col);
}
