/****************************************************************************
 * Copyright (c) 1998 Free Software Foundation, Inc.                        *
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
 ****************************************************************************/

/*
**	lib_set_term.c
**
**	The routine set_term().
**
*/

#include <curses.priv.h>

#include <term.h>	/* cur_term */

MODULE_ID("$Id: lib_set_term.c,v 1.46 1999/07/24 20:05:49 tom Exp $")

SCREEN * set_term(SCREEN *screenp)
{
SCREEN	*oldSP;

	T((T_CALLED("set_term(%p)"), screenp));

	oldSP = SP;
	_nc_set_screen(screenp);

	set_curterm(SP->_term);
	curscr      = SP->_curscr;
	newscr      = SP->_newscr;
	stdscr      = SP->_stdscr;
	COLORS      = SP->_color_count;
	COLOR_PAIRS = SP->_pair_count;
	memcpy(acs_map, SP->_acs_map, sizeof(chtype)*ACS_LEN);

	T((T_RETURN("%p"), oldSP));
	return(oldSP);
}

static void _nc_free_keytry(struct tries *kt)
{
	if (kt != 0) {
		_nc_free_keytry(kt->child);
		_nc_free_keytry(kt->sibling);
		free(kt);
	}
}

/*
 * Free the storage associated with the given SCREEN sp.
 */
void delscreen(SCREEN *sp)
{
	SCREEN **scan = &_nc_screen_chain;

	T((T_CALLED("delscreen(%p)"), sp));

	while(*scan)
	{
	    if (*scan == sp)
	    {
		*scan = sp->_next_screen;
		break;
	    }
	    scan = &(*scan)->_next_screen;
	}

	_nc_freewin(sp->_curscr);
	_nc_freewin(sp->_newscr);
	_nc_freewin(sp->_stdscr);
	_nc_free_keytry(sp->_keytry);
	_nc_free_keytry(sp->_key_ok);

	FreeIfNeeded(sp->_color_table);
	FreeIfNeeded(sp->_color_pairs);

	FreeIfNeeded(sp->oldhash);
	FreeIfNeeded(sp->newhash);

	del_curterm(sp->_term);

	free(sp);

	/*
	 * If this was the current screen, reset everything that the
	 * application might try to use (except cur_term, which may have
	 * multiple references in different screens).
	 */
	if (sp == SP) {
		curscr = 0;
		newscr = 0;
		stdscr = 0;
		COLORS = 0;
		COLOR_PAIRS = 0;
		_nc_set_screen(0);
	}
	returnVoid;
}

static ripoff_t rippedoff[5];
static ripoff_t *rsp = rippedoff;
#define N_RIPS SIZEOF(rippedoff)

static bool no_mouse_event (SCREEN *sp GCC_UNUSED) { return FALSE; }
static bool no_mouse_inline(SCREEN *sp GCC_UNUSED) { return FALSE; }
static bool no_mouse_parse (int code   GCC_UNUSED) { return TRUE; }
static void no_mouse_resume(SCREEN *sp GCC_UNUSED) { }
static void no_mouse_wrap  (SCREEN *sp GCC_UNUSED) { }

int _nc_setupscreen(short slines, short const scolumns, FILE *output)
/* OS-independent screen initializations */
{
int	bottom_stolen = 0;
size_t	i;

        assert(SP==0); /* has been reset in newterm() ! */
	if (!_nc_alloc_screen())
		return ERR;

	SP->_next_screen = _nc_screen_chain;
	_nc_screen_chain = SP;

	_nc_set_buffer(output, TRUE);
	SP->_term        = cur_term;
	SP->_lines       = slines;
	SP->_lines_avail = slines;
	SP->_columns     = scolumns;
	SP->_cursrow     = -1;
	SP->_curscol     = -1;
	SP->_nl          = TRUE;
	SP->_raw         = FALSE;
	SP->_cbreak      = 0;
	SP->_echo        = TRUE;
	SP->_fifohead    = -1;
	SP->_endwin      = TRUE;
	SP->_ofp         = output;
	SP->_cursor      = -1;	/* cannot know real cursor shape */
#ifdef NCURSES_NO_PADDING
	SP->_no_padding  = getenv("NCURSES_NO_PADDING") != 0;
#endif

	SP->_maxclick     = DEFAULT_MAXCLICK;
	SP->_mouse_event  = no_mouse_event;
	SP->_mouse_inline = no_mouse_inline;
	SP->_mouse_parse  = no_mouse_parse;
	SP->_mouse_resume = no_mouse_resume;
	SP->_mouse_wrap   = no_mouse_wrap;
	SP->_mouse_fd     = -1;

	/* initialize the panel hooks */
	SP->_panelHook.top_panel = (struct panel*)0;
	SP->_panelHook.bottom_panel = (struct panel*)0;
	SP->_panelHook.stdscr_pseudo_panel = (struct panel*)0;

	/*
	 * If we've no magic cookie support, we suppress attributes that xmc
	 * would affect, i.e., the attributes that affect the rendition of a
	 * space.  Note that this impacts the alternate character set mapping
	 * as well.
	 */
	if (magic_cookie_glitch > 0) {

		SP->_xmc_triggers = termattrs() & (
				A_ALTCHARSET |
				A_BLINK |
				A_BOLD |
				A_REVERSE |
				A_STANDOUT |
				A_UNDERLINE
				);
		SP->_xmc_suppress = SP->_xmc_triggers & (chtype)~(A_BOLD);

		T(("magic cookie attributes %s", _traceattr(SP->_xmc_suppress)));
#if USE_XMC_SUPPORT
		/*
		 * To keep this simple, suppress all of the optimization hooks
		 * except for clear_screen and the cursor addressing.
		 */
		clr_eol = 0;
		clr_eos = 0;
		set_attributes = 0;
#else
		magic_cookie_glitch = -1;
		acs_chars = 0;
#endif
	}
	_nc_init_acs();
	memcpy(SP->_acs_map, acs_map, sizeof(chtype)*ACS_LEN);

	_nc_idcok = TRUE;
	_nc_idlok = FALSE;

	_nc_windows = 0; /* no windows yet */

	SP->oldhash = 0;
	SP->newhash = 0;

	T(("creating newscr"));
	if ((newscr = newwin(slines, scolumns, 0, 0)) == 0)
		return ERR;

	T(("creating curscr"));
	if ((curscr = newwin(slines, scolumns, 0, 0)) == 0)
		return ERR;

	SP->_newscr = newscr;
	SP->_curscr = curscr;
#if USE_SIZECHANGE
	SP->_resize = resizeterm;
#endif

	newscr->_clear = TRUE;
	curscr->_clear = FALSE;

	for (i=0, rsp = rippedoff; rsp->line && (i < N_RIPS); rsp++, i++) {
	  if (rsp->hook) {
	      WINDOW *w;
	      int count = (rsp->line < 0) ? -rsp->line : rsp->line;

	      if (rsp->line < 0) {
		  w = newwin(count,scolumns,SP->_lines_avail - count,0);
		  if (w) {
		      rsp->w = w;
		      rsp->hook(w, scolumns);
		      bottom_stolen += count;
		  }
		  else
		    return ERR;
	      } else {
		  w = newwin(count,scolumns, 0, 0);
		  if (w) {
		      rsp->w = w;
		      rsp->hook(w, scolumns);
		      SP->_topstolen += count;
		  }
		  else
		    return ERR;
	      }
	      SP->_lines_avail -= count;
	  }
	  rsp->line = 0;
	}
	/* reset the stack */
	rsp = rippedoff;

	T(("creating stdscr"));
	assert ((SP->_lines_avail + SP->_topstolen + bottom_stolen) == slines);
	if ((stdscr = newwin(LINES = SP->_lines_avail, scolumns, 0, 0)) == 0)
		return ERR;
	SP->_stdscr = stdscr;

	def_shell_mode();
	def_prog_mode();

	return OK;
}

/* The internal implementation interprets line as the number of
   lines to rip off from the top or bottom.
   */
int
_nc_ripoffline(int line, int (*init)(WINDOW *,int))
{
    if (line == 0)
	return(OK);

    if (rsp >= rippedoff + N_RIPS)
	return(ERR);

    rsp->line = line;
    rsp->hook = init;
    rsp->w    = 0;
    rsp++;

    return(OK);
}

int
ripoffline(int line, int (*init)(WINDOW *, int))
{
    T((T_CALLED("ripoffline(%d,%p)"), line, init));

    if (line == 0)
	returnCode(OK);

    returnCode(_nc_ripoffline ((line<0) ? -1 : 1, init));
}
