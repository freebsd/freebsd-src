/*-
 * Copyright (c) 1993, 1994
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
static char sccsid[] = "@(#)sex_screen.c	8.47 (Berkeley) 7/21/94";
#endif /* not lint */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <bitstring.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "compat.h"
#include <curses.h>
#include <db.h>
#include <regex.h>

#include "vi.h"
#include "excmd.h"
#include "sex_screen.h"
#include "../svi/svi_screen.h"

static void	sex_abort __P((void));
static int	sex_noop __P((void));
static int	sex_nope __P((SCR *));
static int	sex_term_init __P((SCR *));
static void	so_se_init __P((SCR *));

/*
 * sex_screen_init --
 *	Initialize the ex screen.
 */
int
sex_screen_init(sp)
	SCR *sp;
{
	/* Initialize support routines. */
	sp->s_bell		= sex_bell;
	sp->s_bg		= (int (*)())sex_nope;
	sp->s_busy		= (int (*)())sex_busy;
	sp->s_change		= (int (*)())sex_noop;
	sp->s_clear		= (int (*)())sex_noop;
	sp->s_colpos		= (size_t (*)())sex_abort;
	sp->s_column		= (int (*)())sex_abort;
	sp->s_confirm		= sex_confirm;
	sp->s_crel		= (int (*)())sex_nope;
	sp->s_edit		= sex_screen_edit;
	sp->s_end		= (int (*)())sex_noop;
	sp->s_ex_cmd		= (int (*)())sex_abort;
	sp->s_ex_run		= (int (*)())sex_abort;
	sp->s_ex_write		= (int (*)())sex_abort;
	sp->s_fg		= (int (*)())sex_nope;
	sp->s_fill		= (int (*)())sex_abort;
	sp->s_get		= sex_get;
	sp->s_key_read		= sex_key_read;
	sp->s_optchange		= sex_optchange;
	sp->s_fmap		= (int (*)())sex_noop;
	sp->s_position		= (int (*)())sex_abort;
	sp->s_rabs		= (int (*)())sex_nope;
	sp->s_rcm		= (size_t (*)())sex_abort;
	sp->s_refresh		= sex_refresh;
	sp->s_scroll		= (int (*)())sex_abort;
	sp->s_split		= (int (*)())sex_nope;
	sp->s_suspend		= sex_suspend;
	sp->s_window		= sex_window;

	return (0);
}

/*
 * sex_screen_copy --
 *	Copy to a new screen.
 */
int
sex_screen_copy(orig, sp)
	SCR *orig, *sp;
{
	SEX_PRIVATE *osex, *nsex;

	/* Create the private screen structure. */
	CALLOC_RET(orig, nsex, SEX_PRIVATE *, 1, sizeof(SEX_PRIVATE));
	sp->sex_private = nsex;

/* INITIALIZED AT SCREEN CREATE. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	if (orig == NULL) {
	} else {
		osex = SXP(orig);
#ifndef SYSV_CURSES
		if (osex->SE != NULL && (nsex->SE = strdup(osex->SE)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			return (1);
		}
		if (osex->SO != NULL && (nsex->SO = strdup(osex->SO)) == NULL) {
			msgq(sp, M_SYSERR, NULL);
			free(osex->SE);
			return (1);
		}
#endif
	}

	return (0);
}

/*
 * sex_screen_end --
 *	End a screen.
 */
int
sex_screen_end(sp)
	SCR *sp;
{
#ifndef SYSV_CURSES
	/* Free inverse video strings. */
	if (SXP(sp)->SE != NULL)
		free(SXP(sp)->SE);
	if (SXP(sp)->SO != NULL)
		free(SXP(sp)->SO);
#endif

	/* Free private memory. */
	FREE(SXP(sp), sizeof(SEX_PRIVATE));
	sp->sex_private = NULL;

	return (0);
}

/*
 * sex_screen_edit --
 *	Main ex screen loop.  The ex screen is relatively uncomplicated.
 *	As long as it has a stdio FILE pointer for output, it's happy.
 */
int
sex_screen_edit(sp, ep)
	SCR *sp;
	EXF *ep;
{
	struct termios t;
	GS *gp;
	int force, rval;

	/* Initialize the terminal state. */
	gp = sp->gp;
	if (F_ISSET(gp, G_STDIN_TTY))
		SEX_RAW(t);

	/* Write to the terminal. */
	sp->stdfp = stdout;

	/*
	 * The resize bit is probably set, but clear it, we're
	 * going to initialize the screen right now.
	 */
	F_CLR(sp, S_RESIZE);

	/* Initialize the termcap buffer. */
	if (sex_term_init(sp))
		return (1);

	for (;;) {
		/*
		 * Run ex.  If ex fails, sex data structures
		 * may be corrupted, be careful what you do.
		 */
		if (rval = ex(sp, sp->ep)) {
			(void)rcv_sync(sp, sp->ep,
			    RCV_EMAIL | RCV_ENDSESSION | RCV_PRESERVE);
			(void)screen_end(sp);	/* General SCR info. */
			break;
		}

		force = 0;
		switch (F_ISSET(sp, S_MAJOR_CHANGE)) {
		case S_EXIT_FORCE:
			force = 1;
			/* FALLTHROUGH */
		case S_EXIT:
			F_CLR(sp, S_EXIT_FORCE | S_EXIT);
			if (file_end(sp, sp->ep, force))
				break;
			(void)screen_end(sp);	/* General SCR info. */
			goto ret;
		case 0:				/* Changing from ex mode. */
			goto ret;
		case S_FSWITCH:
			F_CLR(sp, S_FSWITCH);
			break;
		case S_SSWITCH:
		default:
			abort();
		}
	}

	/* Reset the terminal state. */
ret:	if (F_ISSET(gp, G_STDIN_TTY) && SEX_NORAW(t))
		rval = 1;
	return (rval);
}

/*
 * sex_term_init --
 *	Initialize ex's relationship with the termcap/terminfo entry.
 */
static int
sex_term_init(sp)
	SCR *sp;
{

#ifndef SYSV_CURSES
	/* Initialize standout information. */
		so_se_init(sp);
#endif

	sp->rows = O_VAL(sp, O_LINES);
	sp->cols = O_VAL(sp, O_COLUMNS);
	return (0);
}

#ifndef SYSV_CURSES
/*
 * so_se_init --
 *	Initialize the inverse video strings.
 */
static void
so_se_init(sp)
	SCR *sp;
{
	SEX_PRIVATE *sxp;
	size_t len;
	char *s, *t, buf[128], tbuf[2048];

	if (tgetent(tbuf, O_STR(sp, O_TERM)) != 1)
		return;

	sxp = SXP(sp);

	/* Get SE. */
	t = buf;
	if ((t = tgetstr("se", &t)) == NULL)
		return;
	if ((len = strlen(t)) == 0)
		return;
	MALLOC_NOMSG(sp, s, char *, len + 1);
	if (s == NULL)
		return;
	memmove(s, buf, len);
	s[len] = '\0';
	sxp->SE = s;

	/* Get SO. */
	t = buf;
	if ((t = tgetstr("so", &t)) == NULL)
		goto err;
	if ((len = strlen(t)) == 0)
		goto err;
	MALLOC_NOMSG(sp, s, char *, len + 1);
	if (s == NULL)
		goto err;
	memmove(s, buf, len);
	s[len] = '\0';
	sxp->SO = s;

	return;

err:	free(sxp->SE);
	sxp->SE = NULL;
	return;
}
#endif

/*
 * sex_abort --
 *	Fake function.  Die.
 */
static void
sex_abort()
{
	abort();
}

/*
 * sex_noop --
 *	Fake function.  Do nothing.
 */
static int
sex_noop()
{
	return (0);
}

/*
 * sex_nope --
 *	Fake function.  Not in ex, you don't.
 */
static int
sex_nope(sp)
	SCR *sp;
{
	msgq(sp, M_ERR, "Command not applicable to ex mode");
	return (1);
}
