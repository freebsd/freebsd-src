/*
 * Copyright (c) 1981, 1993, 1994
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
 *	@(#)curses.h	8.3 (Berkeley) 7/27/94
 *	$Id: curses.h,v 1.14 1997/08/23 23:23:07 steve Exp $
 */

#ifndef _CURSES_H_
#define	_CURSES_H_

#include <sys/types.h>
#include <sys/cdefs.h>

#include <stdio.h>

#ifndef _BSD_VA_LIST_
#define _BSD_VA_LIST_ char *
#endif

/*
 * The following #defines and #includes are present for backward
 * compatibility only.  They should not be used in future code.
 *
 * START BACKWARD COMPATIBILITY ONLY.
 */
#ifndef _CURSES_PRIVATE

/* This stuff needed for those pgms which include <sgtty.h> */
/* Undef things manually to avoid redefinition              */

#undef USE_OLD_TTY
#undef B0
#undef B50
#undef B75
#undef B110
#undef B134
#undef B150
#undef B200
#undef B300
#undef B600
#undef B1200
#undef B1800
#undef B2400
#undef B4800
#undef B9600
#undef EXTA
#undef EXTB
#undef B57600
#undef B115200

#include <termios.h>
#include <sys/ioctl.h>
#include <sys/ioctl_compat.h>           /* For sgttyb and related */

#define	bool	char
#define	reg	register

#ifndef TRUE
#define	TRUE	(1)
#endif
#ifndef FALSE
#define	FALSE	(0)
#endif

#define	_puts(s)	tputs(s, 0, __cputchar)

/* Old-style terminal modes access. */
#define	baudrate()	(cfgetospeed(&__baset))
#define	crmode()	cbreak()
#define	erasechar()	(__baset.c_cc[VERASE])
#define	killchar()	(__baset.c_cc[VKILL])
#define	nocrmode()	nocbreak()

/* WINDOW structure members name compatibility */
#define _curx   curx
#define _cury   cury
#define _begx   begx
#define _begy   begy
#define _maxx   maxx
#define _maxy   maxy

#define _tty __baset

#endif /* _CURSES_PRIVATE */

extern char	 GT;			/* Gtty indicates tabs. */
extern char	 NONL;			/* Term can't hack LF doing a CR. */
extern char	 UPPERCASE;		/* Terminal is uppercase only. */

extern int	 My_term;		/* Use Def_term regardless. */
extern char	*Def_term;		/* Default terminal type. */

/* Termcap capabilities. */
extern char     AM, BS, CA, DA, EO, HC, IN, MI, MS, NC, NS, OS,
		PC, UL, XB, XN, XT, XS, XX;
extern char	*AL, *BC, *BT, *CD, *CE, *CL, *CM, *CR, *CS, *DC, *DL,
		*DM, *DO, *ED, *EI, *K0, *K1, *K2, *K3, *K4, *K5, *K6,
		*K7, *K8, *K9, *HO, *IC, *IM, *IP, *KD, *KE, *KH, *KL,
		*KR, *KS, *KU, *LL, *MA, *ND, *NL, *RC, *SC, *SE, *SF,
		*SO, *SR, *TA, *TE, *TI, *UC, *UE, *UP, *US, *VB, *VS,
		*VE, *al, *dl, *sf, *sr,
		*AL_PARM, *DL_PARM, *UP_PARM, *DOWN_PARM, *LEFT_PARM,
		*RIGHT_PARM;

/* END BACKWARD COMPATIBILITY ONLY. */

/* 8-bit ASCII characters. */
#define	unctrl(c)		__unctrl[(c) & 0xff]
#define	unctrllen(ch)		__unctrllen[(ch) & 0xff]

extern char	*__unctrl[256];	/* Control strings. */
extern char	 __unctrllen[256];	/* Control strings length. */

/*
 * A window an array of __LINE structures pointed to by the 'lines' pointer.
 * A line is an array of __LDATA structures pointed to by the 'line' pointer.
 *
 * IMPORTANT: the __LDATA structure must NOT induce any padding, so if new
 * fields are added -- padding fields with *constant values* should ensure
 * that the compiler will not generate any padding when storing an array of
 *  __LDATA structures.  This is to enable consistent use of memcmp, and memcpy
 * for comparing and copying arrays.
 */
typedef struct {
	char ch;			/* the actual character */

#define	__STANDOUT	0x01  		/* Added characters are standout. */
	char attr;			/* attributes of character */
} __LDATA;

#define __LDATASIZE	(sizeof(__LDATA))

typedef struct {
#define	__ISDIRTY	0x01		/* Line is dirty. */
#define __ISPASTEOL	0x02		/* Cursor is past end of line */
#define __FORCEPAINT	0x04		/* Force a repaint of the line */
	u_int flags;
	u_int hash;			/* Hash value for the line. */
	size_t *firstchp, *lastchp;	/* First and last chngd columns ptrs */
	size_t firstch, lastch;		/* First and last changed columns. */
	__LDATA *line;			/* Pointer to the line text. */
} __LINE;

typedef struct __window {		/* Window structure. */
	struct __window	*nextp, *orig;	/* Subwindows list and parent. */
	size_t begy, begx;		/* Window home. */
	size_t cury, curx;		/* Current x, y coordinates. */
	size_t maxy, maxx;		/* Maximum values for curx, cury. */
	short ch_off;			/* x offset for firstch/lastch. */
	__LINE **lines;			/* Array of pointers to the lines */
	__LINE  *lspace;		/* line space (for cleanup) */
	__LDATA *wspace;		/* window space (for cleanup) */

#define	__ENDLINE	0x001		/* End of screen. */
#define	__FLUSH		0x002		/* Fflush(stdout) after refresh. */
#define	__FULLWIN	0x004		/* Window is a screen. */
#define	__IDLINE	0x008		/* Insert/delete sequences. */
#define	__SCROLLWIN	0x010		/* Last char will scroll window. */
#define	__SCROLLOK	0x020		/* Scrolling ok. */
#define	__CLEAROK	0x040		/* Clear on next refresh. */
#define __WSTANDOUT	0x080		/* Standout window */
#define __LEAVEOK	0x100		/* If curser left */
#define __FULLLINE      0x200           /* Line width = terminal width. */
	u_int flags;
} WINDOW;

/* Curses external declarations. */
extern WINDOW	*curscr;		/* Current screen. */
extern WINDOW	*stdscr;		/* Standard screen. */

typedef struct termios SGTTY;

extern SGTTY          __orig_termios;   /* Terminal state before curses */
extern SGTTY          __baset;          /* Our base terminal state */
extern int __tcaction;			/* If terminal hardware set. */
extern int __tty_fileno;		/* Terminal file descriptor */

extern int	 COLS;			/* Columns on the screen. */
extern int	 LINES;			/* Lines on the screen. */

extern char	*ttytype;		/* Full name of current terminal. */

#define	ERR	(0)			/* Error return. */
#define	OK	(1)			/* Success return. */

/* Standard screen pseudo functions. */
#define	addbytes(s, n)			__waddbytes(stdscr, s, n, 0)
#define	addch(ch)			waddch(stdscr, ch)
#define	addnstr(s, n)			waddnstr(stdscr, s, n)
#define	addstr(s)			__waddbytes(stdscr, s, strlen(s), 0)
#define	clear()				wclear(stdscr)
#define	clrtobot()			wclrtobot(stdscr)
#define	clrtoeol()			wclrtoeol(stdscr)
#define	delch()				wdelch(stdscr)
#define	deleteln()			wdeleteln(stdscr)
#define	erase()				werase(stdscr)
#define	getch()				wgetch(stdscr)
#define	getstr(s)			wgetstr(stdscr, s)
#define	inch()				winch(stdscr)
#define	insch(ch)			winsch(stdscr, ch)
#define	insertln()			winsertln(stdscr)
#define	move(y, x)			wmove(stdscr, y, x)
#define	refresh()			wrefresh(stdscr)
#define	standend()			wstandend(stdscr)
#define	standout()			wstandout(stdscr)
#define	waddbytes(w, s, n)		__waddbytes(w, s, n, 0)
#define	waddstr(w, s)			__waddbytes(w, s, strlen(s), 0)

/* Standard screen plus movement pseudo functions. */
#define	mvaddbytes(y, x, s, n)		mvwaddbytes(stdscr, y, x, s, n)
#define	mvaddch(y, x, ch)		mvwaddch(stdscr, y, x, ch)
#define	mvaddnstr(y, x, s, n)		mvwaddnstr(stdscr, y, x, s, n)
#define	mvaddstr(y, x, s)		mvwaddstr(stdscr, y, x, s)
#define	mvdelch(y, x)			mvwdelch(stdscr, y, x)
#define	mvgetch(y, x)			mvwgetch(stdscr, y, x)
#define	mvgetstr(y, x, s)		mvwgetstr(stdscr, y, x, s)
#define	mvinch(y, x)			mvwinch(stdscr, y, x)
#define	mvinsch(y, x, c)		mvwinsch(stdscr, y, x, c)
#define	mvwaddbytes(w, y, x, s, n) \
	(wmove(w, y, x) == ERR ? ERR : __waddbytes(w, s, n, 0))
#define	mvwaddch(w, y, x, ch) \
	(wmove(w, y, x) == ERR ? ERR : waddch(w, ch))
#define	mvwaddnstr(w, y, x, s, n) \
	(wmove(w, y, x) == ERR ? ERR : waddnstr(w, s, n))
#define	mvwaddstr(w, y, x, s) \
	(wmove(w, y, x) == ERR ? ERR : __waddbytes(w, s, strlen(s), 0))
#define	mvwdelch(w, y, x) \
	(wmove(w, y, x) == ERR ? ERR : wdelch(w))
#define	mvwgetch(w, y, x) \
	(wmove(w, y, x) == ERR ? ERR : wgetch(w))
#define	mvwgetstr(w, y, x, s) \
	(wmove(w, y, x) == ERR ? ERR : wgetstr(w, s))
#define	mvwinch(w, y, x) \
	(wmove(w, y, x) == ERR ? ERR : winch(w))
#define	mvwinsch(w, y, x, c) \
	(wmove(w, y, x) == ERR ? ERR : winsch(w, c))

/* Psuedo functions. */
#define	clearok(w, bf) \
	((bf) ? ((w)->flags |= __CLEAROK) : ((w)->flags &= ~__CLEAROK))
#define	flushok(w, bf) \
	((bf) ? ((w)->flags |= __FLUSH) : ((w)->flags &= ~__FLUSH))
#define	getyx(w, y, x) \
	(y) = (w)->cury, (x) = (w)->curx
#define	leaveok(w, bf) \
	((bf) ? ((w)->flags |= __LEAVEOK) : ((w)->flags &= ~__LEAVEOK))
#define	scrollok(w, bf) \
	((bf) ? ((w)->flags |= __SCROLLOK) : ((w)->flags &= ~__SCROLLOK))
#define	winch(w) \
	((w)->lines[(w)->cury]->line[(w)->curx].ch & 0377)

__BEGIN_DECLS
/* Public function prototypes. */
int	 box __P((WINDOW *, int, int));
int	 cbreak __P((void));
int	 delwin __P((WINDOW *));
int	 echo __P((void));
int	 endwin __P((void));
char	*fullname __P((char *, char *));
char	*getcap __P((char *));
int	 gettmode __P((void));
void	 idlok __P((WINDOW *, int));
WINDOW	*initscr __P((void));
char	*longname __P((char *, char *));
int	 mvcur __P((int, int, int, int));
int	 mvprintw __P((int, int, const char *, ...));
int	 mvscanw __P((int, int, const char *, ...));
int	 mvwin __P((WINDOW *, int, int));
int	 mvwprintw __P((WINDOW *, int, int, const char *, ...));
int	 mvwscanw __P((WINDOW *, int, int, const char *, ...));
WINDOW	*newwin __P((int, int, int, int));
int	 nl __P((void));
int	 nocbreak __P((void));
int	 noecho __P((void));
int	 nonl __P((void));
int	 noraw __P((void));
int	 overlay __P((WINDOW *, WINDOW *));
int	 overwrite __P((WINDOW *, WINDOW *));
int	 printw __P((const char *, ...));
int	 raw __P((void));
int	 resetty __P((void));
int	 savetty __P((void));
int	 scanw __P((const char *, ...));
int	 scroll __P((WINDOW *));
int	 setterm __P((char *));
int	 sscans __P((WINDOW *, const char *, ...));
WINDOW	*subwin __P((WINDOW *, int, int, int, int));
int	 suspendwin __P((void));
int	 touchline __P((WINDOW *, int, int, int));
int	 touchoverlap __P((WINDOW *, WINDOW *));
int	 touchwin __P((WINDOW *));
int 	 vwprintw __P((WINDOW *, const char *, _BSD_VA_LIST_));
int      vwscanw __P((WINDOW *, const char *, _BSD_VA_LIST_));
int	 waddch __P((WINDOW *, int));
int	 waddnstr __P((WINDOW *, const char *, int));
int	 wclear __P((WINDOW *));
int	 wclrtobot __P((WINDOW *));
int	 wclrtoeol __P((WINDOW *));
int	 wdelch __P((WINDOW *));
int	 wdeleteln __P((WINDOW *));
int	 werase __P((WINDOW *));
int	 wgetch __P((WINDOW *));
int	 wgetstr __P((WINDOW *, char *));
int	 winsch __P((WINDOW *, int));
int	 winsertln __P((WINDOW *));
int	 wmove __P((WINDOW *, int, int));
int	 wprintw __P((WINDOW *, const char *, ...));
int	 wrefresh __P((WINDOW *));
int	 wscanw __P((WINDOW *, const char *, ...));
int      wstandend __P((WINDOW *));
int      wstandout __P((WINDOW *));

/* Private functions that are needed for user programs prototypes. */
void	 __cputchar __P((int));
int      _putchar __P((int));
int	 __waddbytes __P((WINDOW *, const char *, int, int));

/* Private functions. */
#ifdef _CURSES_PRIVATE
void	 __CTRACE __P((const char *, ...));
u_int	 __hash __P((char *, int));
void	 __id_subwins __P((WINDOW *));
int	 __mvcur __P((int, int, int, int, int));
void	 __restore_stophandler __P((void));
void	 __set_stophandler __P((void));
void	 __set_subwin __P((WINDOW *, WINDOW *));
void     __set_scroll_region __P((int, int, int, int));
void	 __startwin __P((void));
void	 __stop_signal_handler __P((int));
void	 __swflags __P((WINDOW *));
int	 __touchline __P((WINDOW *, int, int, int, int));
int	 __touchwin __P((WINDOW *));
char	*__tscroll __P((const char *, int, int));
int	 __waddch __P((WINDOW *, __LDATA *));

/* Private #defines. */
#define	min(a,b)	(a < b ? a : b)
#define	max(a,b)	(a > b ? a : b)

/* Private externs. */
extern char      DB;
extern int	 __echoit;
extern int	 __endwin;
extern int	 __pfast;
extern int	 __rawmode;
extern int	 __noqch;
extern int       __usecs;

int      tputs __P((const char *, int, void (*)(int)));

#else

int      tputs __P((const char *, int, int (*)(int)));

#endif

/* Termcap functions. */
int      tgetent __P((char *, const char *));
int      tgetnum __P((const char *));
int      tgetflag __P((const char *));
char    *tgetstr __P((const char *, char **));
char    *tgoto __P((const char *, int, int));
__END_DECLS

#endif /* !_CURSES_H_ */
