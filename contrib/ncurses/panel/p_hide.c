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

/* p_hide.c
 * Remove a panel from the stack
 */
#include "panel.priv.h"

MODULE_ID("$Id: p_hide.c,v 1.2 1998/02/11 12:14:01 tom Exp $")

/*+-------------------------------------------------------------------------
	__panel_unlink(pan) - unlink panel from stack
--------------------------------------------------------------------------*/
static void
__panel_unlink(PANEL *pan)
{
  PANEL *prev;
  PANEL *next;

#ifdef TRACE
  dStack("<u%d>",1,pan);
  if(!_nc_panel_is_linked(pan))
    return;
#endif

  _nc_override(pan,P_TOUCH);
  _nc_free_obscure(pan);

  prev = pan->below;
  next = pan->above;

  if(prev)
    { /* if non-zero, we will not update the list head */
      prev->above = next;
      if(next)
	next->below = prev;
    }
  else if(next)
    next->below = prev;
  if(pan == _nc_bottom_panel)
    _nc_bottom_panel = next;
  if(pan == _nc_top_panel)
    _nc_top_panel = prev;

  _nc_calculate_obscure();

  pan->above = (PANEL *)0;
  pan->below = (PANEL *)0;
  dStack("<u%d>",9,pan);
}

int
hide_panel(register PANEL *pan)
{
  if(!pan)
    return(ERR);

  dBug(("--> hide_panel %s", USER_PTR(pan->user)));

  if(!_nc_panel_is_linked(pan))
    {
      pan->above = (PANEL *)0;
      pan->below = (PANEL *)0;
      return(ERR);
    }

  __panel_unlink(pan);
  return(OK);
}
