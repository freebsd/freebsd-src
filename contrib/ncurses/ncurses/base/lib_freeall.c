/****************************************************************************
 * Copyright (c) 1998-2006,2007 Free Software Foundation, Inc.              *
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

#include <curses.priv.h>
#include <term_entry.h>
#include <tic.h>

#if HAVE_NC_FREEALL

#if HAVE_LIBDBMALLOC
extern int malloc_errfd;	/* FIXME */
#endif

MODULE_ID("$Id: lib_freeall.c,v 1.45 2007/12/22 23:29:37 tom Exp $")

/*
 * Free all ncurses data.  This is used for testing only (there's no practical
 * use for it as an extension).
 */
NCURSES_EXPORT(void)
_nc_freeall(void)
{
    WINDOWLIST *p, *q;
    static va_list empty_va;

    T((T_CALLED("_nc_freeall()")));
#if NO_LEAKS
    if (SP != 0) {
	if (SP->_oldnum_list != 0) {
	    FreeAndNull(SP->_oldnum_list);
	}
    }
#endif
    if (SP != 0) {
	_nc_lock_global(windowlist);

	while (_nc_windows != 0) {
	    bool deleted = FALSE;

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
		    if (delwin(&(p->win)) != ERR)
			deleted = TRUE;
		    break;
		}
	    }

	    /*
	     * Don't continue to loop if the list is trashed.
	     */
	    if (!deleted)
		break;
	}
	delscreen(SP);
	_nc_unlock_global(windowlist);
    }
    if (cur_term != 0)
	del_curterm(cur_term);

#if USE_WIDEC_SUPPORT
    FreeIfNeeded(_nc_wacs);
#endif
    (void) _nc_printf_string(0, empty_va);
#ifdef TRACE
    (void) _nc_trace_buf(-1, 0);
#endif

#if BROKEN_LINKER || USE_REENTRANT
    FreeIfNeeded(_nc_prescreen.real_acs_map);
#endif

    _nc_leaks_tinfo();

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
