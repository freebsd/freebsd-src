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
static char sccsid[] = "@(#)sex_get.c	8.37 (Berkeley) 8/14/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <ctype.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>

#include "compat.h"
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "../vi/vcmd.h"
#include "sex_screen.h"

/*
 * !!!
 * The ex input didn't have escape characters like ^V.  The only special
 * character was the backslash character, and that only when it preceded
 * a newline as part of a substitution replacement pattern.  For example,
 * the command input ":a\<cr>" failed immediately with an error, as the
 * <cr> wasn't part of a substitution replacement pattern.  This implies
 * a frightening integration of the editor and the RE engine.  There's no
 * way we're going to reproduce those semantics.  So, if backslashes are
 * special, this code inserts the backslash and the next character into the
 * string, without regard for the character or the command being entered.
 * Since "\<cr>" was illegal historically (except for the one special case),
 * and the command will fail eventually, historical scripts shouldn't break
 * (presuming they didn't depend on the failure mode itself or the characters
 * remaining when failure occurred.
 */
static void	txt_display __P((SCR *, TEXT *, size_t, size_t *));
static int	txt_outdent __P((SCR *, TEXT *));

#define	ERASECH {							\
	for (cnt = tp->wd[tp->len]; cnt-- > 0; --col)			\
		(void)printf("\b \b");					\
}

/*
 * sex_get --
 *	Get lines from the terminal for ex.
 */
enum input
sex_get(sp, ep, tiqh, prompt, flags)
	SCR *sp;
	EXF *ep;
	TEXTH *tiqh;
	ARG_CHAR_T prompt;
	u_int flags;
{
				/* State of the "[^0]^D" sequences. */
	enum { C_NOTSET, C_CARATSET, C_NOCHANGE, C_ZEROSET } carat_st;
	TEXT *ntp, *tp, ait;	/* Input and autoindent text structures. */
	CH ikey;		/* Input character. */
	size_t col;		/* 0-N: screen column. */
	size_t cnt;
	int rval, istty;

	/*
	 * !!!
	 * Most of the special capabilities (like autoindent, erase,
	 * etc.) are turned off if ex isn't talking to a terminal.
	 */
	istty = F_ISSET(sp->gp, G_STDIN_TTY);

	/*
	 * Get a TEXT structure with some initial buffer space, reusing
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

	if (istty) {
		/* Display the prompt. */
		if (LF_ISSET(TXT_PROMPT)) {
			col = KEY_LEN(sp, prompt);
			(void)printf("%s", KEY_NAME(sp, prompt));
		}

		/* Initialize autoindent value and print it out. */
		if (LF_ISSET(TXT_AUTOINDENT)) {
			if (txt_auto(sp, ep, sp->lno, NULL, 0, tp))
				return (INP_ERR);
			BINC_GOTO(sp, tp->wd, tp->wd_len, tp->len + 1);
			for (cnt = 0; cnt < tp->ai; ++cnt)
				txt_display(sp, tp, cnt, &col);
		}
	} else {
		col = 0;

		/* Turn off autoindent here, less special casing below. */
		LF_CLR(TXT_AUTOINDENT);
	}

	for (carat_st = C_NOTSET;;) {
		if (istty)
			(void)fflush(stdout);
		/*
		 * !!!
		 * Historically, ex never mapped commands or keys.
		 */
		if (rval = term_key(sp, &ikey, 0))
			return (rval);

		if (INTERRUPTED(sp))
			return (INP_INTR);

		BINC_GOTO(sp, tp->lb, tp->lb_len, tp->len + 1);
		BINC_GOTO(sp, tp->wd, tp->wd_len, tp->len + 1);

		switch (ikey.value) {
		case K_CR:
		case K_NL:
			/* '\' can escape <carriage-return>/<newline>. */
			if (LF_ISSET(TXT_BACKSLASH) &&
			    tp->len != 0 && tp->lb[tp->len - 1] == '\\')
				goto ins_ch;

			/* Echo the newline if requested. */
			if (istty && LF_ISSET(TXT_NLECHO)) {
				(void)putc('\r', stdout);
				(void)putc('\n', stdout);
				(void)fflush(stdout);
			}

			/*
			 * CR returns from the ex command line, interrupt
			 * always returns.
			 */
			if (LF_ISSET(TXT_CR)) {
				/* Terminate with a nul, needed by filter. */
				tp->lb[tp->len] = '\0';
				return (INP_OK);
			}

			/* '.' terminates ex input modes. */
			if (LF_ISSET(TXT_DOTTERM) &&
			    tp->len == tp->ai + 1 &&
			    tp->lb[tp->len - 1] == '.') {
				/* Release the current TEXT. */
				ntp = tp->q.cqe_prev;
				CIRCLEQ_REMOVE(tiqh, tp, q);
				text_free(tp);
				tp = ntp;
				return (INP_OK);
			}

			/*
			 * If we echoed the newline, display any accumulated
			 * error messages.
			 */
			if (LF_ISSET(TXT_NLECHO) && sex_refresh(sp, ep))
				return (INP_ERR);

			/* Set up bookkeeping for the new line. */
			if ((ntp = text_init(sp, NULL, 0, 32)) == NULL)
				return (INP_ERR);
			ntp->lno = tp->lno + 1;

			/*
			 * Reset the autoindent line value.  0^D keeps the ai
			 * line from changing, ^D changes the level, even if
			 * there are no characters in the old line.  Note,
			 * if using the current tp structure, use the cursor
			 * as the length, the user may have erased autoindent
			 * characters.
			 */
			col = 0;
			if (LF_ISSET(TXT_AUTOINDENT)) {
				if (carat_st == C_NOCHANGE) {
					if (txt_auto(sp, ep,
					    OOBLNO, &ait, ait.ai, ntp))
						return (INP_ERR);
					FREE_SPACE(sp, ait.lb, ait.lb_len);
				} else
					if (txt_auto(sp, ep,
					    OOBLNO, tp, tp->len, ntp))
						return (INP_ERR);
				carat_st = C_NOTSET;

				if (ntp->ai) {
					BINC_GOTO(sp,
					    ntp->wd, ntp->wd_len, ntp->len + 1);
					for (cnt = 0; cnt < ntp->ai; ++cnt)
						txt_display(sp, ntp, cnt, &col);
				}
			}
			/*
			 * Swap old and new TEXT's, and insert the new TEXT
			 * into the queue.
			 */
			tp = ntp;
			CIRCLEQ_INSERT_TAIL(tiqh, tp, q);
			break;
		case K_CARAT:			/* Delete autoindent chars. */
			if (LF_ISSET(TXT_AUTOINDENT) && tp->len <= tp->ai)
				carat_st = C_CARATSET;
			goto ins_ch;
		case K_ZERO:			/* Delete autoindent chars. */
			if (LF_ISSET(TXT_AUTOINDENT) && tp->len <= tp->ai)
				carat_st = C_ZEROSET;
			goto ins_ch;
		case K_CNTRLD:			/* Delete autoindent char. */
			/*
			 * !!!
			 * Historically, the ^D command took (but then ignored)
			 * a count.  For simplicity, we don't return it unless
			 * it's the first character entered.  The check for len
			 * equal to 0 is okay, TXT_AUTOINDENT won't be set.
			 */
			if (LF_ISSET(TXT_CNTRLD)) {
				for (cnt = 0; cnt < tp->len; ++cnt)
					if (!isblank(tp->lb[cnt]))
						break;
				if (cnt == tp->len) {
					tp->len = 1;
					tp->lb[0] = '\004';
					tp->lb[1] = '\0';
					return (INP_OK);
				}
			}

			/*
			 * If in the first column or no characters to erase,
			 * ignore the ^D (this matches historic practice).  If
			 * not doing autoindent or already inserted non-ai
			 * characters, it's a literal.  The latter test is done
			 * in the switch, as the CARAT forms are N + 1, not N.
			 */
			if (!LF_ISSET(TXT_AUTOINDENT))
				goto ins_ch;
			if (tp->len == 0)
				break;
			switch (carat_st) {
			case C_CARATSET:	/* ^^D */
				if (tp->len > tp->ai + 1)
					goto ins_ch;
				/* Save the ai string for later. */
				ait.lb = NULL;
				ait.lb_len = 0;
				BINC_GOTO(sp, ait.lb, ait.lb_len, tp->ai);
				memmove(ait.lb, tp->lb, tp->ai);
				ait.ai = ait.len = tp->ai;

				carat_st = C_NOCHANGE;
				goto leftmargin;
			case C_ZEROSET:		/* 0^D */
				if (tp->len > tp->ai + 1)
					goto ins_ch;
				carat_st = C_NOTSET;
leftmargin:			(void)printf("\b \r");
				tp->ai = tp->len = 0;
				break;
			case C_NOTSET:		/* ^D */
				if (tp->len > tp->ai)
					goto ins_ch;
				if (txt_outdent(sp, tp))
					return (INP_ERR);
				break;
			default:
				abort();
			}
			break;
		case K_VERASE:
			if (!istty)
				goto ins_ch;
			if (tp->len) {
				--tp->len;
				if (tp->lb[tp->len] == '\n' ||
				    tp->lb[tp->len] == '\r')
					goto repaint;
				ERASECH;
			}
			break;
		case K_VWERASE:
			if (!istty)
				goto ins_ch;

			/* Move to the last non-space character. */
			while (tp->len) {
				--tp->len;
				if (tp->lb[tp->len] == '\n' ||
				    tp->lb[tp->len] == '\r')
					goto repaint;
				if (!isblank(tp->lb[tp->len])) {
					++tp->len;
					break;
				} else
					ERASECH;
			}

			/* Move to the last space character. */
			while (tp->len) {
				--tp->len;
				if (tp->lb[tp->len] == '\n' ||
				    tp->lb[tp->len] == '\r')
					goto repaint;
				if (isblank(tp->lb[tp->len])) {
					++tp->len;
					break;
				} else
					ERASECH;
			}
			break;
		case K_VKILL:
			if (!istty)
				goto ins_ch;
			while (tp->len) {
				--tp->len;
				if (tp->lb[tp->len] == '\n' ||
				    tp->lb[tp->len] == '\r') {
					tp->len = 0;
					goto repaint;
				}
				ERASECH;
			}
			break;
		/*
		 * XXX
		 * Historic practice is that ^Z suspended command mode, and
		 * that it was unaffected by the autowrite option.  ^Z ended
		 * insert mode, retaining all but the current line of input,
		 * which was discarded.  When ex was foregrounded, it was in
		 * command mode.  I don't want to discard input because a user
		 * tried to enter a ^Z, and I'd like to be consistent with vi.
		 * So, nex matches vi's historic practice, and doesn't permit
		 * ^Z in input mode.
		 */
		case K_CNTRLZ:
			if (!istty || !LF_ISSET(TXT_EXSUSPEND))
				goto ins_ch;
			sex_suspend(sp);
			goto repaint;
		case K_CNTRLR:
			if (!istty)
				goto ins_ch;
repaint:		if (LF_ISSET(TXT_PROMPT)) {
				col = KEY_LEN(sp, prompt);
				(void)printf("\r%s", KEY_NAME(sp, prompt));
			} else {
				col = 0;
				(void)putc('\r', stdout);
			}
			for (cnt = 0; cnt < tp->len; ++cnt)
				txt_display(sp, tp, cnt, &col);
			break;
		default:
			/*
			 * See the TXT_BEAUTIFY comment in vi/v_ntext.c.
			 *
			 * Silently eliminate any iscntrl() character that
			 * wasn't already handled specially, except for <tab>
			 * and <ff>.
			 */
ins_ch:			if (LF_ISSET(TXT_BEAUTIFY) && iscntrl(ikey.ch) &&
			    ikey.value != K_FORMFEED && ikey.value != K_TAB)
				break;
			tp->lb[tp->len] = ikey.ch;
			if (istty)
				txt_display(sp, tp, tp->len, &col);
			++tp->len;
			break;
		}
	}
	/* NOTREACHED */

binc_err:
	return (INP_ERR);
}

/*
 * txt_display --
 *	Display the character.
 */
static void
txt_display(sp, tp, off, colp)
	SCR *sp;
	TEXT *tp;
	size_t off, *colp;
{
	CHAR_T ch;
	size_t width;

	switch (ch = tp->lb[off]) {
	case '\t':
		*colp += tp->wd[off] = width =
		    O_VAL(sp, O_TABSTOP) - *colp % O_VAL(sp, O_TABSTOP);
		while (width--)
			putc(' ', stdout);
		break;
	case '\n':
	case '\r':
		(void)putc('\r', stdout);
		(void)putc('\n', stdout);
		break;
	default:
		*colp += tp->wd[off] = KEY_LEN(sp, ch);
		(void)printf("%s", KEY_NAME(sp, ch));
	}
}

/*
 * txt_outdent --
 *	Handle ^D outdents.
 *
 * Ex version of vi/v_ntext.c:txt_outdent().  See that code for the
 * usual ranting and raving.
 */
static int
txt_outdent(sp, tp)
	SCR *sp;
	TEXT *tp;
{
	u_long sw, ts;
	size_t cno, cnt, off, scno, spaces;

	ts = O_VAL(sp, O_TABSTOP);
	sw = O_VAL(sp, O_SHIFTWIDTH);

	/* Get the current screen column. */
	for (off = scno = 0; off < tp->len; ++off)
		if (tp->lb[off] == '\t')
			scno += STOP_OFF(scno, ts);
		else
			++scno;

	/* Get the previous shiftwidth column. */
	for (cno = scno; --scno % sw != 0;);

	/* Decrement characters until less than or equal to that slot. */
	for (; cno > scno; --tp->ai) {
		for (cnt = tp->wd[--tp->len]; cnt-- > 0;)
			(void)printf("\b \b");
		if (tp->lb[--off] == '\t')
			cno -= STOP_OFF(cno, ts);
		else
			--cno;
	}

	/* Spaces needed to get to the target. */
	spaces = scno - cno;

	/* Maybe just a delete. */
	if (spaces == 0)
		return (0);

	/* Make sure there's enough room. */
	BINC_RET(sp, tp->lb, tp->lb_len, tp->len + spaces);

	/* Maybe that was enough. */
	if (spaces == 0)
		return (0);

	/* Add new space characters. */
	for (; spaces--; ++tp->ai)
		tp->lb[tp->len++] = ' ';
	return (0);
}
