/****************************************************************************
 * Copyright (c) 1998-2005,2007 Free Software Foundation, Inc.              *
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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1992,1995               *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 *     and: Thomas E. Dickey                        1996-on                 *
 ****************************************************************************/

/*
**	lib_data.c
**
**	Common data that may/may not be allocated, but is referenced globally
**
*/

#include <curses.priv.h>

MODULE_ID("$Id: lib_data.c,v 1.34 2007/10/20 21:49:10 tom Exp $")

/*
 * OS/2's native linker complains if we don't initialize public data when
 * constructing a dll (reported by J.J.G.Ripoll).
 */
#if USE_REENTRANT
NCURSES_EXPORT(WINDOW *)
NCURSES_PUBLIC_VAR(stdscr) (void)
{
    return SP ? SP->_stdscr : 0;
}
NCURSES_EXPORT(WINDOW *)
NCURSES_PUBLIC_VAR(curscr) (void)
{
    return SP ? SP->_curscr : 0;
}
NCURSES_EXPORT(WINDOW *)
NCURSES_PUBLIC_VAR(newscr) (void)
{
    return SP ? SP->_newscr : 0;
}
#else
NCURSES_EXPORT_VAR(WINDOW *) stdscr = 0;
NCURSES_EXPORT_VAR(WINDOW *) curscr = 0;
NCURSES_EXPORT_VAR(WINDOW *) newscr = 0;
#endif

NCURSES_EXPORT_VAR(SCREEN *) _nc_screen_chain = 0;

/*
 * The variable 'SP' will be defined as a function on systems that cannot link
 * data-only modules, since it is used in a lot of places within ncurses and we
 * cannot guarantee that any application will use any particular function.  We
 * put the WINDOW variables in this module, because it appears that any
 * application that uses them will also use 'SP'.
 *
 * This module intentionally does not reference other ncurses modules, to avoid
 * module coupling that increases the size of the executable.
 */
#if BROKEN_LINKER
static SCREEN *my_screen;

NCURSES_EXPORT(SCREEN *)
_nc_screen(void)
{
    return my_screen;
}

NCURSES_EXPORT(int)
_nc_alloc_screen(void)
{
    return ((my_screen = typeCalloc(SCREEN, 1)) != 0);
}

NCURSES_EXPORT(void)
_nc_set_screen(SCREEN *sp)
{
    my_screen = sp;
}

#else
NCURSES_EXPORT_VAR(SCREEN *) SP = NULL; /* Some linkers require initialized data... */
#endif
/* *INDENT-OFF* */
#define CHARS_0s { '\0' }

#define TGETENT_0 { 0L, FALSE, NULL, NULL, NULL }
#define TGETENT_0s { TGETENT_0, TGETENT_0, TGETENT_0, TGETENT_0 } 

NCURSES_EXPORT_VAR(NCURSES_GLOBALS) _nc_globals = {
    0,				/* have_sigwinch */
    0,				/* cleanup_nested */

    FALSE,			/* init_signals */
    FALSE,			/* init_screen */

    NULL,			/* comp_sourcename */
    NULL,			/* comp_termtype */

    FALSE,			/* have_tic_directory */
    FALSE,			/* keep_tic_directory */
    TERMINFO,			/* tic_directory */

    NULL,			/* dbi_list */
    0,				/* dbi_size */

    NULL,			/* first_name */
    NULL,			/* keyname_table */

    NULL,			/* safeprint_buf */
    0,				/* safeprint_used */

    TGETENT_0s,			/* tgetent_cache */
    0,				/* tgetent_index */
    0,				/* tgetent_sequence */

#if USE_HOME_TERMINFO
    NULL,			/* home_terminfo */
#endif

#if !USE_SAFE_SPRINTF
    0,				/* safeprint_cols */
    0,				/* safeprint_rows */
#endif

#ifdef TRACE
    FALSE,			/* init_trace */
    CHARS_0s,			/* trace_fname */
    0,				/* trace_level */
    NULL,			/* trace_fp */

    NULL,			/* tracearg_buf */
    0,				/* tracearg_used */

    NULL,			/* tracebuf_ptr */
    0,				/* tracebuf_used */

    CHARS_0s,			/* tracechr_buf */

    NULL,			/* tracedmp_buf */
    0,				/* tracedmp_used */

    CHARS_0s,			/* tracemse_buf */

    NULL,			/* tracetry_buf */
    0,				/* tracetry_used */

#ifndef USE_TERMLIB
    { CHARS_0s, CHARS_0s },	/* traceatr_color_buf */
    0,				/* traceatr_color_sel */
    -1,				/* traceatr_color_last */
#endif /* USE_TERMLIB */

#endif /* TRACE */
#ifdef USE_PTHREADS
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,	/* mutex_set_SP */
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,	/* mutex_use_screen */
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,	/* mutex_use_window */
    PTHREAD_RECURSIVE_MUTEX_INITIALIZER_NP,	/* mutex_windowlist */
    PTHREAD_MUTEX_INITIALIZER,	/* mutex_tst_tracef */
    PTHREAD_MUTEX_INITIALIZER,	/* mutex_tracef */
    0,				/* nested_tracef */
#endif
};

#define STACK_FRAME_0	{ { 0 }, 0 }
#define STACK_FRAME_0s	{ STACK_FRAME_0 }
#define NUM_VARS_0s	{ 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0 }

#define RIPOFF_0	{ 0,0 }
#define RIPOFF_0s	{ RIPOFF_0 }

NCURSES_EXPORT_VAR(NCURSES_PRESCREEN) _nc_prescreen = {
    TRUE,			/* use_env */
    FALSE,			/* filter_mode */
    A_NORMAL,			/* previous_attr */
    RIPOFF_0s,			/* ripoff */
    NULL,			/* rsp */
    {				/* tparm_state */
#ifdef TRACE
	NULL,			/* tname */
#endif
	NULL,			/* tparam_base */

	STACK_FRAME_0s,		/* stack */
	0,			/* stack_ptr */

	NULL,			/* out_buff */
	0,			/* out_size */
	0,			/* out_used */

	NULL,			/* fmt_buff */
	0,			/* fmt_size */

	NUM_VARS_0s,		/* dynamic_var */
	NUM_VARS_0s,		/* static_vars */
    },
    NULL,			/* saved_tty */
#if BROKEN_LINKER || USE_REENTRANT
    NULL,			/* real_acs_map */
    0,				/* LINES */
    0,				/* COLS */
#ifdef TRACE
    0L,				/* _outchars */
    NULL,			/* _tputs_trace */
#endif
#endif
};
/* *INDENT-ON* */
