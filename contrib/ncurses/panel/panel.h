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

/* panel.h -- interface file for panels library */

#ifndef _PANEL_H
#define _PANEL_H

#include <curses.h>

typedef struct panel
{
  WINDOW *win;
  struct panel *below;
  struct panel *above;
  NCURSES_CONST void *user;
} PANEL;

#if	defined(__cplusplus)
extern "C" {
#endif

extern  WINDOW* panel_window(const PANEL *);
extern  void    update_panels(void);
extern  int     hide_panel(PANEL *);
extern  int     show_panel(PANEL *);
extern  int     del_panel(PANEL *);
extern  int     top_panel(PANEL *);
extern  int     bottom_panel(PANEL *);
extern  PANEL*  new_panel(WINDOW *);
extern  PANEL*  panel_above(const PANEL *);
extern  PANEL*  panel_below(const PANEL *);
extern  int     set_panel_userptr(PANEL *, NCURSES_CONST void *);
extern  NCURSES_CONST void* panel_userptr(const PANEL *);
extern  int     move_panel(PANEL *, int, int);
extern  int     replace_panel(PANEL *,WINDOW *);
extern	int     panel_hidden(const PANEL *);

#if	defined(__cplusplus)
}
#endif

#endif /* _PANEL_H */

/* end of panel.h */
