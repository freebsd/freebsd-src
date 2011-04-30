/****************************************************************************
 * Copyright (c) 1998-2010,2011 Free Software Foundation, Inc.              *
 *                                                                          *
 * Permission is hereby granted, free of charge, to any person obtaining a  *
 * copy of this software and associated documentation files (the            *
 * "Software"), to deal in the Software without restriction, including      *
 * without limitation the rights to use, copy, modify, merge, publish,      *
 * distribute, distribute with modifications, sublicense, and/or sell       *
 * copies of the Software, and to permit persons to whom the Software is    *
 * furnished to do so, subject to the following conditions:                 *
 *                                                                          *
 * The above copyright notice and this permission notice shall be included  *
 * in all copies or substantial portions of the Software.                   *
 *                                                                          *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS  *
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF               *
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.   *
 * IN NO EVENT SHALL THE ABOVE COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,   *
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR    *
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR    *
 * THE USE OR OTHER DEALINGS IN THE SOFTWARE.                               *
 *                                                                          *
 * Except as contained in this notice, the name(s) of the above copyright   *
 * holders shall not be used in advertising or otherwise to promote the     *
 * sale, use or other dealings in this Software without prior written       *
 * authorization.                                                           *
 ****************************************************************************/

/****************************************************************************
 *  Author: Thomas E. Dickey                    1996-on                     *
 ****************************************************************************/
/* $Id: test.priv.h,v 1.102 2011/01/15 23:50:33 tom Exp $ */

#ifndef __TEST_PRIV_H
#define __TEST_PRIV_H 1

#include <ncurses_cfg.h>

/*
 * Fix ifdef's that look for the form/menu/panel libraries, if we are building
 * with wide-character ncurses.
 */
#ifdef  HAVE_LIBFORMW
#define HAVE_LIBFORMW 1
#define HAVE_LIBFORM 1
#endif

#ifdef  HAVE_LIBMENUW
#define HAVE_LIBMENUW 1
#define HAVE_LIBMENU 1
#endif

#ifdef  HAVE_LIBPANELW
#define HAVE_LIBPANELW 1
#define HAVE_LIBPANEL 1
#endif

/*
 * Fallback definitions to accommodate broken compilers.
 */
#ifndef HAVE_CURSES_VERSION
#define HAVE_CURSES_VERSION 0
#endif

#ifndef HAVE_CHGAT
#define HAVE_CHGAT 0
#endif

#ifndef HAVE_COLOR_SET
#define HAVE_COLOR_SET 0
#endif

#ifndef HAVE_FILTER
#define HAVE_FILTER 0
#endif

#ifndef HAVE_FORM_H
#define HAVE_FORM_H 0
#endif

#ifndef HAVE_GETBEGX
#define HAVE_GETBEGX 0
#endif

#ifndef HAVE_GETCURX
#define HAVE_GETCURX 0
#endif

#ifndef HAVE_GETMAXX
#define HAVE_GETMAXX 0
#endif

#ifndef HAVE_GETOPT_H
#define HAVE_GETOPT_H 0
#endif

#ifndef HAVE_GETPARX
#define HAVE_GETPARX 0
#endif

#ifndef HAVE_GETWIN
#define HAVE_GETWIN 0
#endif

#ifndef HAVE_LIBFORM
#define HAVE_LIBFORM 0
#endif

#ifndef HAVE_LIBMENU
#define HAVE_LIBMENU 0
#endif

#ifndef HAVE_LIBPANEL
#define HAVE_LIBPANEL 0
#endif

#ifndef HAVE_LOCALE_H
#define HAVE_LOCALE_H 0
#endif

#ifndef HAVE_MENU_H
#define HAVE_MENU_H 0
#endif

#ifndef HAVE_MVVLINE
#define HAVE_MVVLINE 0
#endif

#ifndef HAVE_MVWVLINE
#define HAVE_MVWVLINE 0
#endif

#ifndef HAVE_NAPMS
#define HAVE_NAPMS 1
#endif

#ifndef HAVE_NC_ALLOC_H
#define HAVE_NC_ALLOC_H 0
#endif

#ifndef HAVE_PANEL_H
#define HAVE_PANEL_H 0
#endif

#ifndef HAVE_PUTWIN
#define HAVE_PUTWIN 0
#endif

#ifndef HAVE_RESIZE_TERM
#define HAVE_RESIZE_TERM 0
#endif

#ifndef HAVE_RIPOFFLINE
#define HAVE_RIPOFFLINE 0
#endif

#ifndef HAVE_SETUPTERM
#define HAVE_SETUPTERM 0
#endif

#ifndef HAVE_SLK_COLOR
#define HAVE_SLK_COLOR 0
#endif

#ifndef HAVE_SLK_INIT
#define HAVE_SLK_INIT 0
#endif

#ifndef HAVE_TERMATTRS
#define HAVE_TERMATTRS 0
#endif

#ifndef HAVE_TERMNAME
#define HAVE_TERMNAME 0
#endif

#ifndef HAVE_TGETENT
#define HAVE_TGETENT 0
#endif

#ifndef HAVE_TIGETNUM
#define HAVE_TIGETNUM 0
#endif

#ifndef HAVE_TIGETSTR
#define HAVE_TIGETSTR 0
#endif

#ifndef HAVE_TYPEAHEAD
#define HAVE_TYPEAHEAD 0
#endif

#ifndef HAVE_WINSSTR
#define HAVE_WINSSTR 0
#endif

#ifndef HAVE_USE_DEFAULT_COLORS
#define HAVE_USE_DEFAULT_COLORS 0
#endif

#ifndef HAVE_WRESIZE
#define HAVE_WRESIZE 0
#endif

#ifndef NCURSES_EXT_FUNCS
#define NCURSES_EXT_FUNCS 0
#endif

#ifndef NEED_PTEM_H
#define NEED_PTEM_H 0
#endif

#ifndef NEED_WCHAR_H
#define NEED_WCHAR_H 0
#endif

#ifndef NO_LEAKS
#define NO_LEAKS 0
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <sys/types.h>
#include <errno.h>

#if HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <signal.h>		/* include before curses.h to work around glibc bug */

#if NEED_WCHAR_H
#include <wchar.h>
#ifdef HAVE_LIBUTF8_H
#include <libutf8.h>
#endif
#endif

#if defined(HAVE_XCURSES)
#include <xcurses.h>
#elif defined(HAVE_NCURSESW_NCURSES_H)
#include <ncursesw/curses.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/curses.h>
#else
#include <curses.h>
#endif

#if defined(HAVE_XCURSES) || defined(PDCURSES)
/* no other headers */
#undef  HAVE_SETUPTERM		/* nonfunctional */
#define HAVE_SETUPTERM 0
#undef  HAVE_TGETENT		/* nonfunctional */
#define HAVE_TGETENT 0
#undef  HAVE_TIGETSTR		/* nonfunctional */
#define HAVE_TIGETSTR 0
#elif defined(HAVE_NCURSESW_TERM_H)
#include <ncursesw/term.h>
#elif defined(HAVE_NCURSES_TERM_H)
#include <ncurses/term.h>
#elif defined(HAVE_TERM_H)
#include <term.h>
#endif

/*
 * Not all curses.h implementations include unctrl.h,
 * Solaris 10 xpg4 for example.
 */
#if defined(NCURSES_VERSION) || defined(_XOPEN_CURSES)
#if defined(HAVE_NCURSESW_NCURSES_H)
#include <ncursesw/unctrl.h>
#elif defined(HAVE_NCURSES_NCURSES_H)
#include <ncurses/unctrl.h>
#else
#include <unctrl.h>
#endif
#endif

#if HAVE_GETOPT_H
#include <getopt.h>
#else
/* 'getopt()' may be prototyped in <stdlib.h>, but declaring its variables
 * doesn't hurt.
 */
extern char *optarg;
extern int optind;
#endif /* HAVE_GETOPT_H */

#if HAVE_LOCALE_H
#include <locale.h>
#else
#define setlocale(name,string)	/* nothing */
#endif

#include <assert.h>
#include <ctype.h>

#ifndef GCC_NORETURN
#define GCC_NORETURN		/* nothing */
#endif
#ifndef GCC_PRINTFLIKE
#define GCC_PRINTFLIKE(a,b)	/* nothing */
#endif
#ifndef GCC_UNUSED
#define GCC_UNUSED		/* nothing */
#endif

#ifndef HAVE_GETNSTR
#define getnstr(s,n) getstr(s)
#endif

#ifndef USE_SOFTKEYS
#if HAVE_SLK_INIT
#define USE_SOFTKEYS 1
#else
#define USE_SOFTKEYS 0
#endif
#endif

#if !USE_SOFTKEYS
#define slk_init() /* nothing */
#define slk_restore() /* nothing */
#define slk_clear() /* nothing */
#endif

#ifndef HAVE_WSYNCDOWN
#define wsyncdown(win) /* nothing */
#endif

#ifndef USE_WIDEC_SUPPORT
#if (defined(_XOPEN_SOURCE_EXTENDED) || defined(_XPG5)) && defined(WACS_ULCORNER)
#define USE_WIDEC_SUPPORT 1
#else
#define USE_WIDEC_SUPPORT 0
#endif
#endif

#if HAVE_PANEL_H && HAVE_LIBPANEL
#define USE_LIBPANEL 1
#else
#define USE_LIBPANEL 0
#endif

#if HAVE_MENU_H && HAVE_LIBMENU
#define USE_LIBMENU 1
#else
#define USE_LIBMENU 0
#endif

#if HAVE_FORM_H && HAVE_LIBFORM
#define USE_LIBFORM 1
#else
#define USE_LIBFORM 0
#endif

/* workaround, to build against NetBSD's variant of the form library */
#ifdef HAVE_NETBSD_FORM_H
#define form_getyx(form, y, x) y = current_field(form)->cursor_ypos, x = current_field(form)->cursor_xpos
#else
#define form_getyx(form, y, x) y = (form)->currow, x = (form)->curcol
#endif

/* workaround, to build against NetBSD's variant of the form library */
#ifdef HAVE_NETBSD_MENU_H
#define menu_itemwidth(menu) (menu)->max_item_width
#else
#define menu_itemwidth(menu) (menu)->itemlen
#endif

#ifndef HAVE_TYPE_ATTR_T
#if !USE_WIDEC_SUPPORT && !defined(attr_t)
#define attr_t chtype
#endif
#endif

#undef NCURSES_CH_T
#if !USE_WIDEC_SUPPORT
#define NCURSES_CH_T chtype
#else
#define NCURSES_CH_T cchar_t
#endif

#ifndef NCURSES_OPAQUE
#define NCURSES_OPAQUE 0
#endif

#ifndef CCHARW_MAX
#define CCHARW_MAX 5
#endif

#if defined(NCURSES_VERSION) && defined(CURSES_WACS_ARRAY) && !defined(CURSES_WACS_SYMBOLS)
#define CURSES_WACS_SYMBOLS
#endif

#if defined(CURSES_WACS_ARRAY) && !defined(CURSES_WACS_SYMBOLS)
/* NetBSD 5.1 defines these incorrectly */
#undef	WACS_RARROW
#undef	WACS_LARROW
#undef	WACS_UARROW
#undef	WACS_DARROW
#undef	WACS_BLOCK
#undef	WACS_DIAMOND
#undef	WACS_CKBOARD
#undef	WACS_DEGREE
#undef	WACS_PLMINUS
#undef	WACS_BOARD
#undef	WACS_LANTERN
#undef	WACS_LRCORNER
#undef	WACS_URCORNER
#undef	WACS_ULCORNER
#undef	WACS_LLCORNER
#undef	WACS_PLUS
#undef	WACS_HLINE
#undef	WACS_S1
#undef	WACS_S9
#undef	WACS_LTEE
#undef	WACS_RTEE
#undef	WACS_BTEE
#undef	WACS_TTEE
#undef	WACS_VLINE
#undef	WACS_BULLET
#undef	WACS_S3
#undef	WACS_S7
#undef	WACS_LEQUAL
#undef	WACS_GEQUAL
#undef	WACS_PI
#undef	WACS_NEQUAL
#undef	WACS_STERLING

#define	WACS_RARROW     &(CURSES_WACS_ARRAY['+'])
#define	WACS_LARROW     &(CURSES_WACS_ARRAY[','])
#define	WACS_UARROW     &(CURSES_WACS_ARRAY['-'])
#define	WACS_DARROW     &(CURSES_WACS_ARRAY['.'])
#define	WACS_BLOCK      &(CURSES_WACS_ARRAY['0'])
#define	WACS_DIAMOND    &(CURSES_WACS_ARRAY['`'])
#define	WACS_CKBOARD    &(CURSES_WACS_ARRAY['a'])
#define	WACS_DEGREE     &(CURSES_WACS_ARRAY['f'])
#define	WACS_PLMINUS    &(CURSES_WACS_ARRAY['g'])
#define	WACS_BOARD      &(CURSES_WACS_ARRAY['h'])
#define	WACS_LANTERN    &(CURSES_WACS_ARRAY['i'])
#define	WACS_LRCORNER   &(CURSES_WACS_ARRAY['j'])
#define	WACS_URCORNER   &(CURSES_WACS_ARRAY['k'])
#define	WACS_ULCORNER   &(CURSES_WACS_ARRAY['l'])
#define	WACS_LLCORNER   &(CURSES_WACS_ARRAY['m'])
#define	WACS_PLUS       &(CURSES_WACS_ARRAY['n'])
#define	WACS_HLINE      &(CURSES_WACS_ARRAY['q'])
#define	WACS_S1         &(CURSES_WACS_ARRAY['o'])
#define	WACS_S9         &(CURSES_WACS_ARRAY['s'])
#define	WACS_LTEE       &(CURSES_WACS_ARRAY['t'])
#define	WACS_RTEE       &(CURSES_WACS_ARRAY['u'])
#define	WACS_BTEE       &(CURSES_WACS_ARRAY['v'])
#define	WACS_TTEE       &(CURSES_WACS_ARRAY['w'])
#define	WACS_VLINE      &(CURSES_WACS_ARRAY['x'])
#define	WACS_BULLET     &(CURSES_WACS_ARRAY['~'])
#define	WACS_S3		&(CURSES_WACS_ARRAY['p'])
#define	WACS_S7		&(CURSES_WACS_ARRAY['r'])
#define	WACS_LEQUAL	&(CURSES_WACS_ARRAY['y'])
#define	WACS_GEQUAL	&(CURSES_WACS_ARRAY['z'])
#define	WACS_PI		&(CURSES_WACS_ARRAY['{'])
#define	WACS_NEQUAL	&(CURSES_WACS_ARRAY['|'])
#define	WACS_STERLING	&(CURSES_WACS_ARRAY['}'])
#endif

#undef CTRL
#define CTRL(x)	((x) & 0x1f)

#define QUIT		CTRL('Q')
#define ESCAPE		CTRL('[')

#ifndef KEY_MIN
#define KEY_MIN 256		/* not defined in Solaris 8 */
#endif

#ifdef DECL_CURSES_DATA_BOOLNAMES
extern char *boolnames[], *boolcodes[], *boolfnames[];
extern char *numnames[], *numcodes[], *numfnames[];
extern char *strnames[], *strcodes[], *strfnames[];
#endif

#define colored_chtype(ch, attr, pair) \
	((chtype) (ch) | (chtype) (attr) | (chtype) COLOR_PAIR(pair))

/*
 * Workaround for HPUX
 */
#if defined(__hpux) && !defined(NCURSES_VERSION)
#define getbegx(w) __getbegx(w)
#define getbegy(w) __getbegy(w)
#define getcurx(w) __getcurx(w)
#define getcury(w) __getcury(w)
#define getmaxx(w) __getmaxx(w)
#define getmaxy(w) __getmaxy(w)
#define getparx(w) __getparx(w)
#define getpary(w) __getpary(w)
#endif

/*
 * Workaround in case getcchar() returns a positive value when the source
 * string produces only a L'\0'.
 */
#define TEST_CCHAR(s, count, then_stmt, else_stmt) \
	if ((count = getcchar(s, NULL, NULL, NULL, NULL)) > 0) { \
	    wchar_t test_wch[CCHARW_MAX + 2]; \
	    attr_t test_attrs; \
	    short test_pair; \
	    \
	    if (getcchar( s, test_wch, &test_attrs, &test_pair, NULL) == OK \
		&& test_wch[0] != L'\0') { \
		then_stmt \
	    } else { \
		else_stmt \
	    } \
	} else { \
	    else_stmt \
	}
/*
 * These usually are implemented as macros, but may be functions.
 */
#if !defined(getcurx) && !HAVE_GETCURX
#define getcurx(win)            ((win)?(win)->_curx:ERR)
#define getcury(win)            ((win)?(win)->_cury:ERR)
#endif

#if !defined(getbegx) && !HAVE_GETBEGX
#define getbegx(win)            ((win)?(win)->_begx:ERR)
#define getbegy(win)            ((win)?(win)->_begy:ERR)
#endif

#if !defined(getmaxx) && !HAVE_GETMAXX
#define getmaxx(win)            ((win)?((win)->_maxx + 1):ERR)
#define getmaxy(win)            ((win)?((win)->_maxy + 1):ERR)
#endif

/*
 * Solaris 10 xpg4:
#define	__m_getparx(w)		((w)->_parent == (WINDOW *) 0 ? -1 \
				: (w)->_begx - (w)->_parent->_begx)
 */
#if !defined(getparx) && !HAVE_GETPARX
#ifdef __m_getparx
#define getparx(win)            __m_getparx(win)
#define getpary(win)            __m_getpary(win)
#else
#define getparx(win)            ((win)?((win)->_parx + 1):ERR)
#define getpary(win)            ((win)?((win)->_pary + 1):ERR)
#endif
#endif

#if !defined(mvwvline) && !HAVE_MVWVLINE
#define mvwvline(w,y,x,ch,n)    (move(y,x) == ERR ? ERR : wvline(w,ch,n))
#define mvwhline(w,y,x,ch,n)    (move(y,x) == ERR ? ERR : whline(w,ch,n))
#endif

#if !defined(mvvline) && !HAVE_MVVLINE
#define mvvline(y,x,ch,n)       (move(y,x) == ERR ? ERR : vline(ch,n))
#define mvhline(y,x,ch,n)       (move(y,x) == ERR ? ERR : hline(ch,n))
#endif

/*
 * Try to accommodate curses implementations that have no terminfo support.
 */
#if HAVE_TIGETNUM
#define TIGETNUM(ti,tc) tigetnum(ti)
#else
#define TIGETNUM(ti,tc) tgetnum(tc)
#endif

#if HAVE_TIGETSTR
#define TIGETSTR(ti,tc) tigetstr(ti)
#else
#define TIGETSTR(ti,tc) tgetstr(tc,&area_pointer)
#endif

/* ncurses implements tparm() with varargs, X/Open with a fixed-parameter list
 * (which is incompatible with legacy usage, doesn't solve any problems).
 */
#define tparm3(a,b,c) tparm(a,b,c,0,0,0,0,0,0,0)
#define tparm2(a,b)   tparm(a,b,0,0,0,0,0,0,0,0)

#define UChar(c)    ((unsigned char)(c))

#define SIZEOF(table)	(sizeof(table)/sizeof(table[0]))

#if defined(NCURSES_VERSION) && HAVE_NC_ALLOC_H
#include <nc_alloc.h>
#if HAVE_NC_FREEALL && defined(USE_TINFO)
#undef ExitProgram
#define ExitProgram(code) _nc_free_tinfo(code)
#endif
#else
#define typeMalloc(type,n) (type *) malloc((n) * sizeof(type))
#define typeCalloc(type,elts) (type *) calloc((elts), sizeof(type))
#define typeRealloc(type,n,p) (type *) realloc(p, (n) * sizeof(type))
#endif

#ifndef ExitProgram
#define ExitProgram(code) exit(code)
#endif

#ifndef EXIT_SUCCESS
#define EXIT_SUCCESS 0
#endif
#ifndef EXIT_FAILURE
#define EXIT_FAILURE 1
#endif

#ifdef __MINGW32__
#include <nc_mingw.h>
/* conflicts in test/firstlast.c */
#undef large
#undef small

#endif

/* Use this to quiet gcc's -Wwrite-strings warnings, but accommodate SVr4
 * curses which doesn't have const parameters declared (so far) in the places
 * that XSI shows.
 */
#ifndef NCURSES_CONST
#define NCURSES_CONST		/* nothing */
#endif

/* out-of-band values for representing absent capabilities */
#define ABSENT_BOOLEAN		((signed char)-1)	/* 255 */
#define ABSENT_NUMERIC		(-1)
#define ABSENT_STRING		(char *)0

/* out-of-band values for representing cancels */
#define CANCELLED_BOOLEAN	((signed char)-2)	/* 254 */
#define CANCELLED_NUMERIC	(-2)
#define CANCELLED_STRING	(char *)(-1)

#define VALID_BOOLEAN(s) ((unsigned char)(s) <= 1)	/* reject "-1" */
#define VALID_NUMERIC(s) ((s) >= 0)
#define VALID_STRING(s)  ((s) != CANCELLED_STRING && (s) != ABSENT_STRING)

#define VT_ACSC "``aaffggiijjkkllmmnnooppqqrrssttuuvvwwxxyyzz{{||}}~~"

#define CATCHALL(handler) { \
		int nsig; \
		for (nsig = SIGHUP; nsig < SIGTERM; ++nsig) \
		    if (nsig != SIGKILL) \
			signal(nsig, handler); \
	    }

/*
 * Workaround for clean(er) compile with Solaris's legacy curses.
 * The same would be needed for HPUX 10.20
 */
#ifndef TPUTS_ARG
#if defined(sun) && !defined(_XOPEN_CURSES) && !defined(NCURSES_VERSION_PATCH)
#define TPUTS_ARG char
extern char *tgoto(char *, int, int);	/* available, but not prototyped */
#else
#define TPUTS_ARG int
#endif
#endif

/*
 * Workarounds for Solaris's X/Open curses
 */
#if defined(sun) && defined(_XOPEN_CURSES) && !defined(NCURSES_VERSION_PATCH)
#if !defined(KEY_MIN) && defined(__KEY_MIN)
#define KEY_MIN __KEY_MIN
#endif
#if !defined(KEY_MAX) && defined(__KEY_MIN)
#define KEY_MAX __KEY_MAX
#endif
#endif

/*
 * Workaround to build with Sun's default SVr4 curses.
 */
#ifdef NCURSES_VERSION
#ifndef HAVE_VW_PRINTW
#define HAVE_VW_PRINTW 1
#endif
#endif

/*
 * ncurses provides arrays of capability names; X/Open discarded these SVr4
 * features.  Some implementations continue to provide them (see the test
 * configure script).
 */
#ifdef NCURSES_VERSION
#ifndef HAVE_CURSES_DATA_BOOLNAMES
#define HAVE_CURSES_DATA_BOOLNAMES 1
#endif
#endif

/*
 * ncurses uses const in some places where X/Open does (or did) not allow.
 */
#ifdef NCURSES_VERSION
#define CONST_MENUS const
#else
#define CONST_MENUS		/* nothing */
#endif

#ifndef HAVE_USE_WINDOW
#if !defined(NCURSES_VERSION_PATCH) || (NCURSES_VERSION_PATCH < 20070915) || !NCURSES_EXT_FUNCS
#define HAVE_USE_WINDOW 0
#else
#define HAVE_USE_WINDOW 1
#endif
#endif

/*
 * Simplify setting up demo of threading with these macros.
 */

#if !HAVE_USE_WINDOW
typedef int (*NCURSES_WINDOW_CB) (WINDOW *, void *);
typedef int (*NCURSES_SCREEN_CB) (SCREEN *, void *);
#endif

#if HAVE_USE_WINDOW
#define USING_WINDOW(w,func) use_window(w, (NCURSES_WINDOW_CB) func, w)
#define USING_WINDOW2(w,func,data) use_window(w, (NCURSES_WINDOW_CB) func, data)
#define WANT_USE_WINDOW() extern void _nc_want_use_window(void)
#else
#define USING_WINDOW(w,func) func(w)
#define USING_WINDOW2(w,func,data) func(w,data)
#define WANT_USE_WINDOW() extern void _nc_want_use_window(void)
#endif

#if HAVE_USE_WINDOW
#define USING_SCREEN(s,func,data) use_screen(s, (NCURSES_SCREEN_CB) func, data)
#define WANT_USE_SCREEN() extern void _nc_want_use_screen(void)
#else
#define USING_SCREEN(s,func,data) func(data)
#define WANT_USE_SCREEN() extern void _nc_want_use_screen(void)
#endif

#ifdef TRACE
#define Trace(p) _tracef p
#define USE_TRACE 1
#else
#define Trace(p)		/* nothing */
#define USE_TRACE 0
#endif

#define MvAddCh         (void) mvaddch
#define MvWAddCh        (void) mvwaddch
#define MvAddStr        (void) mvaddstr
#define MvWAddStr       (void) mvwaddstr
#define MvWAddChStr     (void) mvwaddchstr
#define MvPrintw        (void) mvprintw
#define MvWPrintw       (void) mvwprintw
#define MvHLine         (void) mvhline
#define MvWHLine        (void) mvwhline
#define MvVLine         (void) mvvline
#define MvWVLine        (void) mvwvline

/*
 * Workaround for defective implementation of gcc attribute warn_unused_result
 */
#if defined(__GNUC__) && defined(_FORTIFY_SOURCE)
#define IGNORE_RC(func) errno = func
#else
#define IGNORE_RC(func) (void) func
#endif /* gcc workarounds */

#define init_mb(state)	memset(&state, 0, sizeof(state))

#endif /* __TEST_PRIV_H */
