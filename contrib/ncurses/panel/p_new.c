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
 *  Author: Zeyd M. Ben-Halim <zmbenhal@netcom.com> 1995                    *
 *     and: Eric S. Raymond <esr@snark.thyrsus.com>                         *
 ****************************************************************************/

/* p_new.c
 * Creation of a new panel 
 */
#include "panel.priv.h"

MODULE_ID("$Id: p_new.c,v 1.5 1999/11/22 18:02:41 juergen Exp $")

#ifdef TRACE
static char* stdscr_id;
static char* new_id;
#endif

/*+-------------------------------------------------------------------------
  Get root (i.e. stdscr's) panel.
  Establish the pseudo panel for stdscr if necessary.
--------------------------------------------------------------------------*/
static PANEL*
root_panel(void)
{
  if(_nc_stdscr_pseudo_panel == (PANEL*)0)
    {
      
      assert(stdscr && !_nc_bottom_panel && !_nc_top_panel);
      _nc_stdscr_pseudo_panel = (PANEL*)malloc(sizeof(PANEL));
      if (_nc_stdscr_pseudo_panel != 0) {
	PANEL* pan  = _nc_stdscr_pseudo_panel;
	WINDOW* win = stdscr;
	pan->win = win;
	pan->below   = (PANEL*)0;
	pan->above   = (PANEL*)0;
#ifdef TRACE
	if (!stdscr_id)
	  stdscr_id = strdup("stdscr");
	pan->user = stdscr_id;
#else
	pan->user = (void*)0;
#endif
	_nc_bottom_panel = _nc_top_panel = pan;
      }
    }
  return _nc_stdscr_pseudo_panel;
}

PANEL *
new_panel(WINDOW *win)
{
  PANEL *pan = (PANEL*)0;

  if (!win)
    return(pan);

  if (!_nc_stdscr_pseudo_panel)
    (void)root_panel();
  assert(_nc_stdscr_pseudo_panel);

  if (!(win->_flags & _ISPAD) && (pan = (PANEL*)malloc(sizeof(PANEL))))
    {
      pan->win = win;
      pan->above = (PANEL *)0;
      pan->below = (PANEL *)0;
#ifdef TRACE
      if (!new_id)
	new_id = strdup("new");
      pan->user = new_id;
#else
      pan->user = (char *)0;
#endif
      (void)show_panel(pan);
    }
  return(pan);
}
