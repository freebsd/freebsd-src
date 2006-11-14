// * this is for making emacs happy: -*-Mode: C++;-*-
/****************************************************************************
 * Copyright (c) 1999,2001 Free Software Foundation, Inc.                   *
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
 *   Author: Juergen Pfeifer <juergen.pfeifer@gmx.net> 1999                 *
 ****************************************************************************/

#include "internal.h"
#include "etip.h"
#include "cursesw.h"

MODULE_ID("$Id: cursespad.cc,v 1.4 2001/03/24 21:25:57 tom Exp $")

NCursesPad::NCursesPad(int lines, int cols)
  : NCursesWindow(),
    viewWin((NCursesWindow*)0),
    viewSub((NCursesWindow*)0),
    h_gridsize(0), v_gridsize(0),
    min_row(0), min_col(0)
{
  w = ::newpad(lines,cols);
  if ((WINDOW*)0==w) {
    count--;
    err_handler("Cannot construct window");
  }
  alloced = TRUE;
}


int NCursesPad::driver (int key) {
  // Default implementation
  switch(key) {
  case KEY_UP:
    // =======
    return REQ_PAD_UP;
  case KEY_DOWN:
    // =========
    return REQ_PAD_DOWN;
  case KEY_LEFT:
    // =========
    return REQ_PAD_LEFT;
  case KEY_RIGHT:
    // ==========
    return REQ_PAD_RIGHT;
  case KEY_EXIT:
    // =========
  case CTRL('X'):
    // ==========
    return REQ_PAD_EXIT;

  default: return(key);
  }
}


void NCursesPad::operator()(void) {
  NCursesWindow* W = Win();

  if ((NCursesWindow*)0 != W) {
    int Width  = W->width();
    int Height = W->height();

    int req = REQ_PAD_REFRESH;

    W->keypad(TRUE);
    W->meta(TRUE);
    refresh();

    do {
      bool changed = FALSE;

      switch (req) {
      case REQ_PAD_REFRESH:
	// ================
	changed = TRUE;
	break;
      case REQ_PAD_LEFT:
	// =============
	if (min_col > 0) {
	  changed = TRUE;
	  if (min_col < h_gridsize)
	    min_col = 0;
	  else
	    min_col -= h_gridsize;
	}
	else
	  OnNavigationError(req);
	break;
      case REQ_PAD_RIGHT:
	// ==============
	if (min_col < (width() - Width - 1)) {
	  changed = TRUE;
	  if (min_col > (width() - Width - h_gridsize - 1))
	    min_col = width() - Width - 1;
	  else
	    min_col += h_gridsize;
	}
	else
	  OnNavigationError(req);
	break;
      case REQ_PAD_UP:
	// ===========
	if (min_row > 0) {
	  changed = TRUE;
	  if (min_row < v_gridsize)
	    min_row = 0;
	  else
	    min_row -= v_gridsize;
	}
	else
	  OnNavigationError(req);
	break;
      case REQ_PAD_DOWN:
	// =============
	if (min_row < (height() - Height - 1)) {
	  changed = TRUE;
	  if (min_row > (height() - Height - v_gridsize - 1))
	    min_row = height() - Height - 1;
	  else
	    min_row += v_gridsize;
	}
	else
	  OnNavigationError(req);
	break;

      default:
	OnUnknownOperation(req);
      }

      if (changed) {
	noutrefresh();
	W->syncup();
	OnOperation(req);
	viewWin->refresh();
      }
    } while( (req=driver(W->getch())) != REQ_PAD_EXIT );
  }
}


int NCursesPad::refresh() {
  int res = noutrefresh();
  if (res==OK && ((NCursesWindow*)0 != viewWin)) {
    res = (viewWin->refresh());
  }
  return(res);
}

int NCursesPad::noutrefresh() {
  int res = OK;
  NCursesWindow* W = Win();
  if ((NCursesWindow*)0 != W) {
    res = copywin(*W,min_row,min_col,
		  0,0,W->maxy(),W->maxx(),
		  FALSE);
    if (res==OK) {
      W->syncup();
      res = viewWin->noutrefresh();
    }
  }
  return (res);
}

void NCursesPad::setWindow(NCursesWindow& view,
			   int v_grid NCURSES_PARAM_INIT(1),
			   int h_grid NCURSES_PARAM_INIT(1))
{
  viewWin = &view;
  min_row = min_col = 0;
  if (h_grid <=0 || v_grid <= 0)
    err_handler("Illegal Gridsize");
  else {
    h_gridsize = h_grid;
    v_gridsize = v_grid;
  }
}

void NCursesPad::setSubWindow(NCursesWindow& sub)
{
  if ((NCursesWindow*)0 == viewWin)
    err_handler("Pad has no viewport");
  if (!viewWin->isDescendant(sub))
    THROW(new NCursesException("NCursesFramePad", E_SYSTEM_ERROR));
  viewSub = &sub;
}

void NCursesFramedPad::OnOperation(int pad_req) {
  NCursesWindow* W = Win();
  NCursesWindow* Win = getWindow();

  if (((NCursesWindow*)0 != W) && ((NCursesWindow*)0 != Win)) {
    int Width  = W->width();
    int Height = W->height();
    int i, row, col, h_len, v_len;

    h_len = (Width*Width + width() - 1)/width();
    if (h_len==0)
      h_len = 1;
    if (h_len > Width)
      h_len = Width;

    v_len = (Height*Height + height() - 1)/height();
    if (v_len==0)
      v_len = 1;
    if (v_len > Height)
      v_len = Height;

    col  = (min_col * Width + width() - 1)  / width();
    if (col + h_len > Width)
      col = Width - h_len;

    row  = (min_row * Height + height() - 1) / height();
    if (row + v_len > Height)
      row = Height - v_len;

    Win->vline(1,Width+1,Height);
    Win->attron(A_REVERSE);
    if (v_len>=2) {
      Win->addch(row+1,Width+1,ACS_UARROW);
      for(i=2;i<v_len;i++)
	Win->addch(row+i,Width+1,' ');
      Win->addch(row+v_len,Width+1,ACS_DARROW);
    }
    else {
      for(i=1;i<=v_len;i++)
	Win->addch(row+i,Width+1,' ');
    }
    Win->attroff(A_REVERSE);

    Win->hline(Height+1,1,Width);
    Win->attron(A_REVERSE);
    if (h_len >= 2) {
      Win->addch(Height+1,col+1,ACS_LARROW);
      for(i=2;i<h_len;i++)
	Win->addch(Height+1,col+i,' ');
      Win->addch(Height+1,col+h_len,ACS_RARROW);
    }
    else {
      for(i=1;i<=h_len;i++)
	Win->addch(Height+1,col+i,' ');
    }
    Win->attroff(A_REVERSE);
  }
}
