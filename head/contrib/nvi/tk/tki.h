/*-
 * Copyright (c) 1993, 1994
 *	The Regents of the University of California.  All rights reserved.
 * Copyright (c) 1993, 1994, 1995, 1996
 *	Keith Bostic.  All rights reserved.
 *
 * See the LICENSE file for redistribution information.
 *
 *	@(#)tki.h	8.6 (Berkeley) 4/27/96
 */

#include <tcl.h>
#include <tk.h>

typedef struct _tk_private {
	Tcl_Interp	*interp;/* Tcl interpreter cookie. */

				/* Shared variables. */
	int	tk_cursor_row;	/* Current cursor row. */
	int	tk_cursor_col;	/* Current cursor col. */
	int	tk_ssize_row;	/* Screen rows. */
	int	tk_ssize_col;	/* Screen columns. */

	struct termios orig;	/* Original terminal values. */

	CHAR_T	ibuf[64];	/* Input keys. */
	int	ibuf_cnt;	/* Number of input keys. */

				/* Event queue. */
	TAILQ_HEAD(_eventh, _event) evq;

	int	 killersig;	/* Killer signal. */
#define	INDX_HUP	0
#define	INDX_INT	1
#define	INDX_TERM	2
#define	INDX_WINCH	3
#define	INDX_MAX	4	/* Original signal information. */
	struct sigaction oact[INDX_MAX];

#define	TK_LLINE_IV	0x0001	/* Last line is in inverse video. */
#define	TK_SCR_VI_INIT	0x0002	/* Vi screen initialized. */
#define	TK_SIGHUP	0x0004	/* SIGHUP arrived. */
#define	TK_SIGINT	0x0008	/* SIGINT arrived. */
#define	TK_SIGTERM	0x0010	/* SIGTERM arrived. */
#define	TK_SIGWINCH	0x0020	/* SIGWINCH arrived. */
	u_int16_t flags;
} TK_PRIVATE;

extern GS *__global_list;
#define	TKP(sp)		((TK_PRIVATE *)((sp)->gp->tk_private))
#define	GTKP(gp)	((TK_PRIVATE *)gp->tk_private)

/* Return possibilities from the keyboard read routine. */
typedef enum { INP_OK=0, INP_EOF, INP_ERR, INP_INTR, INP_TIMEOUT } input_t;

/* The screen line relative to a specific window. */
#define	RLNO(sp, lno)	(sp)->woff + (lno)

/* Some functions can be safely ignored until the screen is running. */
#define	VI_INIT_IGNORE(sp)						\
	if (F_ISSET(sp, SC_VI) && !F_ISSET(TKP(sp), TK_SCR_VI_INIT))	\
		return (0);

#include "tk_extern.h"
