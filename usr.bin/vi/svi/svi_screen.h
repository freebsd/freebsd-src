/*-
 * Copyright (c) 1993
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
 *
 *	@(#)svi_screen.h	8.38 (Berkeley) 3/15/94
 */

/*
 * Structure for mapping lines to the screen.  An SMAP is an array, with one
 * structure element per screen line, which holds information describing the
 * physical line which is displayed in the screen line.  The first two fields
 * (lno and off) are all that are necessary to describe a line.  The rest of
 * the information is useful to keep information from being re-calculated.
 *
 * Lno is the line number.  Off is the screen offset into the line.  For
 * example, the pair 2:1 would be the first screen of line 2, and 2:2 would
 * be the second.  If doing left-right scrolling, all of the offsets will be
 * the same, i.e. for the second screen, 1:2, 2:2, 3:2, etc.  If doing the
 * standard vi scrolling, it will be staggered, i.e. 1:1, 1:2, 1:3, 2:1, 3:1,
 * etc.
 *
 * The SMAP is always as large as the physical screen, plus a slot for the
 * info line, so that there is room to add any screen into another one at
 * screen exit.
 */
typedef struct _smap {
	recno_t  lno;		/* 1-N: Physical file line number. */
	size_t	 off;		/* 1-N: Screen offset in the line. */

				/* svi_line() cache information. */
	size_t	 c_sboff;	/* 0-N: offset of first character byte. */
	size_t	 c_eboff;	/* 0-N: offset of  last character byte. */
	u_char	 c_scoff;	/* 0-N: offset into the first character. */
	u_char	 c_eclen;	/* 1-N: columns from the last character. */
	u_char	 c_ecsize;	/* 1-N: size of the  last character. */
} SMAP;

				/* Macros to flush/test cached information. */
#define	SMAP_CACHE(smp)		((smp)->c_ecsize != 0)
#define	SMAP_FLUSH(smp)		((smp)->c_ecsize = 0)

typedef struct _svi_private {
/* INITIALIZED AT SCREEN CREATE. */
	SMAP	*h_smap;	/* First slot of the line map. */
	SMAP	*t_smap;	/*  Last slot of the line map. */

	size_t	 exlinecount;	/* Ex overwrite count. */
	size_t	 extotalcount;	/* Ex overwrite count. */
	size_t	 exlcontinue;	/* Ex line continue value. */

				/* svi_opt_screens() cache information. */
#define	SVI_SCR_CFLUSH(svp)	svp->ss_lno = OOBLNO
	recno_t	 ss_lno;	/* 1-N: Line number. */
	size_t	 ss_screens;	/* Return value. */

	recno_t	 olno;		/* 1-N: old cursor file line. */
	size_t	 ocno;		/* 0-N: old file cursor column. */
	size_t	 sc_col;	/* 0-N: LOGICAL screen column. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	size_t	 srows;		/* 1-N: Rows in the terminal/window. */

	char	*VB;		/* Visual bell termcap string. */

#define	SVI_CUR_INVALID	0x001	/* Cursor position is unknown. */
#define	SVI_DIVIDER	0x002	/* Screen divider is displayed. */
#define	SVI_INFOLINE	0x004	/* The infoline is being used by v_ntext(). */
#define	SVI_NO_VBELL	0x008	/* No visual bell available. */
#define	SVI_SCREENDIRTY	0x010	/* Screen needs refreshing. */
	u_int	 flags;
} SVI_PRIVATE;

#define	SVP(sp)		((SVI_PRIVATE *)((sp)->svi_private))
#define	 HMAP		(SVP(sp)->h_smap)
#define	 TMAP		(SVP(sp)->t_smap)
#define	_HMAP(sp)	(SVP(sp)->h_smap)
#define	_TMAP(sp)	(SVP(sp)->t_smap)

/*
 * One extra slot is always allocated for the map so that we can use
 * it to do vi :colon command input; see svi_get().
 */
#define	SIZE_HMAP(sp)	(SVP(sp)->srows + 1)

#define	O_NUMBER_FMT	"%7lu "			/* O_NUMBER format, length. */
#define	O_NUMBER_LENGTH	8
						/* Columns on a screen. */
#define	SCREEN_COLS(sp)							\
	((O_ISSET(sp, O_NUMBER) ? (sp)->cols - O_NUMBER_LENGTH : (sp)->cols))

#define	HALFSCREEN(sp)	((sp)->t_maxrows / 2)	/* Half the screen. */
#define	HALFTEXT(sp)	((sp)->t_rows / 2)	/* Half the text. */

#define	INFOLINE(sp)	((sp)->t_maxrows)	/* Info line test, offset. */
#define	ISINFOLINE(sp, smp)	(((smp) - HMAP) == INFOLINE(sp))

						/* Small screen test. */
#define	ISSMALLSCREEN(sp)	((sp)->t_minrows != (sp)->t_maxrows)

/*
 * Next tab offset.
 *
 * !!!
 * There are problems with how the historical vi handled tabs.  For example,
 * by doing "set ts=3" and building lines that fold, you can get it to step
 * through tabs as if they were spaces and move inserted characters to new
 * positions when <esc> is entered.  I think that nvi does tabs correctly,
 * but there may be some historical incompatibilities.
 */
#define	TAB_OFF(sp, c)	(O_VAL(sp, O_TABSTOP) - (c) % O_VAL(sp, O_TABSTOP))

/* Move in a screen (absolute), and fail if it doesn't work. */
#define	MOVEA(sp, lno, cno) {						\
	if (move(lno, cno) == ERR) {					\
		msgq(sp, M_ERR,						\
		    "Error: %s/%d: move:l(%u), c(%u), abs.",		\
		    tail(__FILE__), __LINE__, lno, cno);		\
		return (1);						\
	}								\
}

/* Move in a window, and fail if it doesn't work. */
#define	MOVE(sp, lno, cno) {						\
	size_t __lno = (sp)->woff + (lno);				\
	if (move(__lno, cno) == ERR) {					\
		msgq(sp, M_ERR,						\
		    "Error: %s/%d: move:l(%u), c(%u), o(%u).",		\
		    tail(__FILE__), __LINE__, lno, cno, sp->woff);	\
		return (1);						\
	}								\
}

/* Add a character. */
#define	ADDCH(ch) {							\
	int __ch = (ch);						\
	ADDNSTR(cname[__ch].name, cname[__ch].len);			\
}

/* Add a string len bytes long. */
#define	ADDNSTR(str, len) {						\
	if (addnstr(str, len) == ERR) {					\
		int __x, __y;						\
		getyx(stdscr, __y, __x);				\
		msgq(sp, M_ERR, "Error: %s/%d: addnstr: (%d/%u).",	\
		    tail(__FILE__), __LINE__, __y, __x);		\
		return (1);						\
	}								\
}

/* Add a string. */
#define	ADDSTR(str) {							\
	if (addstr(str) == ERR) {					\
		int __x, __y;						\
		getyx(stdscr, __y, __x);				\
		msgq(sp, M_ERR, "Error: %s/%d: addstr: (%d/%u).",	\
		    tail(__FILE__), __LINE__, __y, __x);		\
		return (1);						\
	}								\
}

/* Public routines. */
void	svi_bell __P((SCR *));
int	svi_bg __P((SCR *));
int	svi_busy __P((SCR *, char const *));
int	svi_change __P((SCR *, EXF *, recno_t, enum operation));
size_t	svi_cm_public __P((SCR *, EXF *, recno_t, size_t));
int	svi_column __P((SCR *, EXF *, size_t *));
enum confirm
	svi_confirm __P((SCR *, EXF *, MARK *, MARK *));
int	svi_clear __P((SCR *));
int	svi_crel __P((SCR *, long));
int	svi_ex_cmd __P((SCR *, EXF *, struct _excmdarg *, MARK *));
int	svi_ex_run __P((SCR *, EXF *, MARK *));
int	svi_ex_write __P((void *, const char *, int));
int	svi_fg __P((SCR *, CHAR_T *));
enum input
	svi_get __P((SCR *, EXF *, TEXTH *, int, u_int));
int	svi_optchange __P((SCR *, int));
int	svi_rabs __P((SCR *, long, enum adjust));
size_t	svi_rcm __P((SCR *, EXF *, recno_t));
int	svi_refresh __P((SCR *, EXF *));
int	svi_screen_copy __P((SCR *, SCR *));
int	svi_screen_edit __P((SCR *, EXF *));
int	svi_screen_end __P((SCR *));
int	svi_sm_down __P((SCR *, EXF *, MARK *, recno_t, int));
int	svi_sm_fill __P((SCR *, EXF *, recno_t, enum position));
int	svi_sm_position __P((SCR *, EXF *, MARK *, u_long, enum position));
int	svi_sm_up __P((SCR *, EXF *, MARK *, recno_t, int));
int	svi_split __P((SCR *, ARGS *[]));
int	svi_suspend __P((SCR *));
int	svi_swap __P((SCR *, SCR **, char *));

/* Private routines. */
size_t	svi_cm_private __P((SCR *, EXF *, recno_t, size_t, size_t));
int	svi_curses_end __P((SCR *));
int	svi_curses_init __P((SCR *));
int	svi_divider __P((SCR *));
int	svi_init __P((SCR *));
int	svi_join __P((SCR *, SCR **));
void	svi_keypad __P((SCR *, int));
int	svi_line __P((SCR *, EXF *, SMAP *, size_t *, size_t *));
int	svi_number __P((SCR *, EXF *));
size_t	svi_opt_screens __P((SCR *, EXF *, recno_t, size_t *));
int	svi_paint __P((SCR *, EXF *));
int	svi_putchar __P((int));
size_t	svi_screens __P((SCR *, EXF *, char *, size_t, recno_t, size_t *));
int	svi_sm_1down __P((SCR *, EXF *));
int	svi_sm_1up __P((SCR *, EXF *));
int	svi_sm_cursor __P((SCR *, EXF *, SMAP **));
int	svi_sm_next __P((SCR *, EXF *, SMAP *, SMAP *));
recno_t	svi_sm_nlines __P((SCR *, EXF *, SMAP *, recno_t, size_t));
int	svi_sm_prev __P((SCR *, EXF *, SMAP *, SMAP *));

/* Private debugging routines. */
#ifdef DEBUG
int	svi_gdbrefresh __P((void));
#endif
