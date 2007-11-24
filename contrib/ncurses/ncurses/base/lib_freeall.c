/****************************************************************************
 * Copyright (c) 1998-2005,2006 Free Software Foundation, Inc.              *
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
 *  Author: Thomas E. Dickey                    1996,1997                   *
 ****************************************************************************/

#include <curses.priv.h>
#include <term_entry.h>
#include <tic.h>

#if HAVE_NC_FREEALL

#if HAVE_LIBDBMALLOC
extern int malloc_errfd;	/* FIXME */
#endif

MODULE_ID("$Id: lib_freeall.c,v 1.38 2006/12/02 22:36:43 tom Exp $")

/*
 * Free all ncurses data.  This is used for testing only (there's no practical
 * use for it as an extension).
 */
NCURSES_EXPORT(void)
_nc_freeall(void)
{
    WINDOWLIST *p, *q;
    char *s;
    static va_list empty_va;

    T((T_CALLED("_nc_freeall()")));
#if NO_LEAKS
    _nc_free_tparm();
    if (_nc_oldnums != 0) {
	FreeAndNull(_nc_oldnums);
    }
#endif
    if (SP != 0) {
	while (_nc_windows != 0) {
	    /* Delete only windows that're not a parent */
	    for (p = _nc_windows; p != 0; p = p->next) {
		bool found = FALSE;

		for (q = _nc_windows; q != 0; q = q->next) {
		    if ((p != q)
			&& (q->win._flags & _SUBWIN)
			&& (&(p->win) == q->win._parent)) {
			found = TRUE;
			break;
		    }
		}

		if (!found) {
		    delwin(&(p->win));
		    break;
		}
	    }
	}
	delscreen(SP);
    }
#if NO_LEAKS
    _nc_tgetent_leaks();
#endif
    del_curterm(cur_term);
    _nc_free_entries(_nc_head);
    _nc_get_type(0);
    _nc_first_name(0);
#if USE_WIDEC_SUPPORT
    FreeIfNeeded(_nc_wacs);
#endif
#if NO_LEAKS
    _nc_alloc_entry_leaks();
    _nc_captoinfo_leaks();
    _nc_comp_scan_leaks();
    _nc_keyname_leaks();
    _nc_tic_expand(0, FALSE, 0);
#endif

    if ((s = _nc_home_terminfo()) != 0)
	free(s);

    (void) _nc_printf_string(0, empty_va);
#ifdef TRACE
    (void) _nc_trace_buf(-1, 0);
#endif

#if HAVE_LIBDBMALLOC
    malloc_dump(malloc_errfd);
#elif HAVE_LIBDMALLOC
#elif HAVE_LIBMPATROL
    __mp_summary();
#elif HAVE_PURIFY
    purify_all_inuse();
#endif
    returnVoid;
}

NCURSES_EXPORT(void)
_nc_free_and_exit(int code)
{
    char *last_setbuf = (SP != 0) ? SP->_setbuf : 0;

    _nc_freeall();
#ifdef TRACE
    trace(0);			/* close trace file, freeing its setbuf */
    free(_nc_varargs("?", 0));
#endif
    fclose(stdout);
    FreeIfNeeded(last_setbuf);
    exit(code);
}

#else
NCURSES_EXPORT(void)
_nc_freeall(void)
{
}
#endif
