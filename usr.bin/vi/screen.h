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
 *
 *	@(#)screen.h	8.81 (Berkeley) 12/29/93
 */

/*
 * There are minimum values that vi has to have to display a screen.  The
 * row minimum is fixed at 1 line for the text, and 1 line for any error
 * messages.  The column calculation is a lot trickier.  For example, you
 * have to have enough columns to display the line number, not to mention
 * guaranteeing that tabstop and shiftwidth values are smaller than the
 * current column value.  It's a lot simpler to have a fixed value and not
 * worry about it.
 *
 * XXX
 * MINIMUM_SCREEN_COLS is probably wrong.
 */
#define	MINIMUM_SCREEN_ROWS	 2
#define	MINIMUM_SCREEN_COLS	20
					/* Line operations. */
enum operation { LINE_APPEND, LINE_DELETE, LINE_INSERT, LINE_RESET };
					/* Position values. */
enum position { P_BOTTOM, P_FILL, P_MIDDLE, P_TOP };

/*
 * Structure for holding file references.  Each SCR structure contains a
 * linked list of these (the user's argument list) as well as pointers to
 * the current and previous files.  The structure contains the name of the
 * file, along with the information that follows the name.  A file has up
 * to three "names".  The tname field is the path of the temporary backing
 * file, if any.  The name field is the name the user originally used to
 * specify the file to be edited.  The cname field is the changed name if
 * the user changed the name.
 *
 * Note that the read-only bit follows the file name, not the file itself.
 *
 * XXX
 * The mtime field should be a struct timespec, but time_t is more portable.
 */
struct _fref {
	CIRCLEQ_ENTRY(_fref) q;		/* Linked list of file references. */
	char	*cname;			/* Changed file name. */
	char	*name;			/* File name. */
	char	*tname;			/* Temporary file name. */

	recno_t	 lno;			/* 1-N: file cursor line. */
	size_t	 cno;			/* 0-N: file cursor column. */
	time_t	 mtime;			/* Last modification time. */

#define	FR_CHANGEWRITE	0x001		/* Name changed and then written. */
#define	FR_CURSORSET	0x002		/* If lno/cno valid. */
#define	FR_EDITED	0x004		/* If the file was ever edited. */
#define	FR_IGNORE	0x008		/* File isn't part of argument list. */
#define	FR_NEWFILE	0x010		/* File doesn't really exist yet. */
#define	FR_RDONLY	0x020		/* File is read-only. */
	u_int	 flags;
};

/*
 * There's a file name hierarchy -- if the user has changed the name, we
 * use it, otherwise, we use the original name, if there was one, othewise
 * use the temporary name.
 */
#define	FILENAME(frp)							\
	((frp)->cname != NULL) ? (frp)->cname :				\
	((frp)->name != NULL) ? (frp)->name : (frp)->tname

/*
 * SCR --
 *	The screen structure.  To the extent possible, all screen information
 *	is stored in the various private areas.  The only information here
 *	is used by global routines or is shared by too many screens.
 */
struct _scr {
/* INITIALIZED AT SCREEN CREATE. */
	CIRCLEQ_ENTRY(_scr) q;		/* Screens. */

	GS	*gp;			/* Pointer to global area. */

	SCR	*nextdisp;		/* Next display screen. */

	EXF	*ep;			/* Screen's current EXF structure. */

	MSGH	 msgq;			/* Message list. */
					/* FREF list. */
	CIRCLEQ_HEAD(_frefh, _fref) frefq;
	FREF	*frp;			/* FREF being edited. */
	FREF	*a_frp;			/* Last argument list FREF edited. */
	FREF	*p_frp;			/* Last FREF edited. */

	u_long	 ccnt;			/* Command count. */
	u_long	 q_ccnt;		/* Quit or ZZ command count. */

					/* Screen's: */
	size_t	 rows;			/* 1-N: number of rows. */
	size_t	 cols;			/* 1-N: number of columns. */
	size_t	 woff;			/* 0-N: row offset in screen. */
	size_t	 t_rows;		/* 1-N: cur number of text rows. */
	size_t	 t_maxrows;		/* 1-N: max number of text rows. */
	size_t	 t_minrows;		/* 1-N: min number of text rows. */

					/* Cursor's: */
	recno_t	 lno;			/* 1-N: file line. */
	size_t	 cno;			/* 0-N: file character in line. */

	size_t	 rcm;			/* Vi: 0-N: Column suck. */
#define	RCM_FNB		0x01		/* Column suck: first non-blank. */
#define	RCM_LAST	0x02		/* Column suck: last. */
	u_int	 rcmflags;

#define	L_ADDED		0		/* Added lines. */
#define	L_CHANGED	1		/* Changed lines. */
#define	L_COPIED	2		/* Copied lines. */
#define	L_DELETED	3		/* Deleted lines. */
#define	L_JOINED	4		/* Joined lines. */
#define	L_MOVED		5		/* Moved lines. */
#define	L_PUT		6		/* Put lines. */
#define	L_LSHIFT	7		/* Left shift lines. */
#define	L_RSHIFT	8		/* Right shift lines. */
#define	L_YANKED	9		/* Yanked lines. */
	recno_t	 rptlines[L_YANKED + 1];/* Ex/vi: lines changed by last op. */

	FILE	*stdfp;			/* Ex output file pointer. */

	char	*if_name;		/* Ex input file name, for messages. */
	recno_t	 if_lno;		/* Ex input file line, for messages. */

	fd_set	 rdfd;			/* Ex/vi: read fd select mask. */

	TEXTH	 tiq;			/* Ex/vi: text input queue. */

	SCRIPT	*script;		/* Vi: script mode information .*/

	char const *time_msg;		/* ITIMER_REAL message. */
	struct itimerval time_value;	/* ITIMER_REAL saved value. */
	struct sigaction time_handler;	/* ITIMER_REAL saved handler. */

	void	*vi_private;		/* Vi private area. */
	void	*ex_private;		/* Ex private area. */
	void	*svi_private;		/* Vi curses screen private area. */
	void	*xaw_private;		/* Vi XAW screen private area. */

/* PARTIALLY OR COMPLETELY COPIED FROM PREVIOUS SCREEN. */
	char	*alt_name;		/* Ex/vi: alternate file name. */

					/* Ex/vi: search/substitute info. */
	regex_t	 sre;			/* Last search RE. */
	regex_t	 subre;			/* Last substitute RE. */
	enum direction	searchdir;	/* File search direction. */
	enum cdirection	csearchdir;	/* Character search direction. */
	CHAR_T	 lastckey;		/* Last search character. */
	regmatch_t     *match;		/* Substitute match array. */
	size_t	 matchsize;		/* Substitute match array size. */
	char	*repl;			/* Substitute replacement. */
	size_t	 repl_len;		/* Substitute replacement length.*/
	size_t	*newl;			/* Newline offset array. */
	size_t	 newl_len;		/* Newline array size. */
	size_t	 newl_cnt;		/* Newlines in replacement. */

	u_int	 saved_vi_mode;		/* Saved vi display type. */

	OPTION	 opts[O_OPTIONCOUNT];	/* Options. */

/*
 * SCREEN SUPPORT ROUTINES.
 *
 * A SCR * MUST be the first argument to these routines.
 */
					/* Ring the screen bell. */
	void	 (*s_bell) __P((SCR *));
					/* Background the screen. */
	int	 (*s_bg) __P((SCR *));
					/* Put up a busy message. */
	int	 (*s_busy) __P((SCR *, char const *));
					/* Change a screen line. */
	int	 (*s_change) __P((SCR *, EXF *, recno_t, enum operation));
					/* Return column close to specified. */
	size_t	 (*s_chposition) __P((SCR *, EXF *, recno_t, size_t));
					/* Clear the screen. */
	int	 (*s_clear) __P((SCR *));
					/* Return the logical cursor column. */
	int	 (*s_column) __P((SCR *, EXF *, size_t *));
	enum confirm			/* Confirm an action with the user. */
		 (*s_confirm) __P((SCR *, EXF *, MARK *, MARK *));
					/* Move down the screen. */
	int	 (*s_down) __P((SCR *, EXF *, MARK *, recno_t, int));
					/* Edit a file. */
	int	 (*s_edit) __P((SCR *, EXF *));
					/* End a screen. */
	int	 (*s_end) __P((SCR *));
					/* Run a single ex command. */
	int	 (*s_ex_cmd) __P((SCR *, EXF *, EXCMDARG *, MARK *));
					/* Run user's ex commands. */
	int	 (*s_ex_run) __P((SCR *, EXF *, MARK *));
					/* Screen's ex write function. */
	int	 (*s_ex_write) __P((void *, const char *, int));
					/* Foreground the screen. */
	int	 (*s_fg) __P((SCR *, CHAR_T *));
					/* Fill the screen's map. */
	int	 (*s_fill) __P((SCR *, EXF *, recno_t, enum position));
	enum input			/* Get a line from the user. */
		 (*s_get) __P((SCR *, EXF *, TEXTH *, int, u_int));
	enum input			/* Get a key from the user. */
		 (*s_key_read) __P((SCR *, int *, struct timeval *));
					/* Tell the screen an option changed. */
	int	 (*s_optchange) __P((SCR *, int));
					/* Return column at screen position. */
	int	 (*s_position) __P((SCR *, EXF *,
		    MARK *, u_long, enum position));
					/* Change the absolute screen size. */
	int	 (*s_rabs) __P((SCR *, long));
					/* Refresh the screen. */
	int	 (*s_refresh) __P((SCR *, EXF *));
					/* Return column close to last char. */
	size_t	 (*s_relative) __P((SCR *, EXF *, recno_t));
					/* Change the relative screen size. */
	int	 (*s_rrel) __P((SCR *, long));
					/* Split the screen. */
	int	 (*s_split) __P((SCR *, ARGS *[]));
					/* Suspend the screen. */
	int	 (*s_suspend) __P((SCR *));
					/* Move up the screen. */
	int	 (*s_up) __P((SCR *, EXF *, MARK *, recno_t, int));

/* Editor screens. */
#define	S_EX		0x0000001	/* Ex screen. */
#define	S_VI_CURSES	0x0000002	/* Vi: curses screen. */
#define	S_VI_XAW	0x0000004	/* Vi: Athena widgets screen. */

#define	IN_EX_MODE(sp)			/* If in ex mode. */		\
	(F_ISSET(sp, S_EX))
#define	IN_VI_MODE(sp)			/* If in vi mode. */		\
	(F_ISSET(sp, S_VI_CURSES | S_VI_XAW))
#define	S_SCREENS			/* Screens. */			\
	(S_EX | S_VI_CURSES | S_VI_XAW)

/* Major screen/file changes. */
#define	S_EXIT		0x0000008	/* Exiting (not forced). */
#define	S_EXIT_FORCE	0x0000010	/* Exiting (forced). */
#define	S_FSWITCH	0x0000020	/* Switch files. */
#define	S_SSWITCH	0x0000040	/* Switch screens. */
#define	S_MAJOR_CHANGE			/* Screen or file changes. */	\
	(S_EXIT | S_EXIT_FORCE | S_FSWITCH | S_SSWITCH)

#define	S_BELLSCHED	0x0000080	/* Bell scheduled. */
#define	S_CONTINUE	0x0000100	/* Need to ask the user to continue. */
#define	S_EXSILENT	0x0000200	/* Ex batch script. */
#define	S_GLOBAL	0x0000400	/* Doing a global command. */
#define	S_INPUT		0x0000800	/* Doing text input. */
#define	S_INTERRUPTED	0x0001000	/* If have been interrupted. */
#define	S_INTERRUPTIBLE	0x0002000	/* If can be interrupted. */
#define	S_REDRAW	0x0004000	/* Redraw the screen. */
#define	S_REFORMAT	0x0008000	/* Reformat the screen. */
#define	S_REFRESH	0x0010000	/* Refresh the screen. */
#define	S_RENUMBER	0x0020000	/* Renumber the screen. */
#define	S_RESIZE	0x0040000	/* Resize the screen. */
#define	S_SCRIPT	0x0080000	/* Window is a shell script. */
#define	S_SRE_SET	0x0100000	/* The search RE has been set. */
#define	S_SUBRE_SET	0x0200000	/* The substitute RE has been set. */
#define	S_TIMER_SET	0x0400000	/* If a busy timer is running. */
#define	S_UPDATE_MODE	0x0800000	/* Don't repaint modeline. */
	u_int flags;
};

/* Generic routines to start/stop a screen. */
int	screen_end __P((SCR *));
int	screen_init __P((SCR *, SCR **, u_int));

/* Public interfaces to the underlying screens. */
int	ex_screen_copy __P((SCR *, SCR *));
int	ex_screen_end __P((SCR *));
int	ex_screen_init __P((SCR *));
int	sex_screen_copy __P((SCR *, SCR *));
int	sex_screen_end __P((SCR *));
int	sex_screen_init __P((SCR *));
int	svi_screen_copy __P((SCR *, SCR *));
int	svi_screen_end __P((SCR *));
int	svi_screen_init __P((SCR *));
int	v_screen_copy __P((SCR *, SCR *));
int	v_screen_end __P((SCR *));
int	v_screen_init __P((SCR *));
int	xaw_screen_copy __P((SCR *, SCR *));
int	xaw_screen_end __P((SCR *));
int	xaw_screen_init __P((SCR *));
