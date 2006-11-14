// * this is for making emacs happy: -*-Mode: C++;-*-

/*
  Copyright (C) 1989 Free Software Foundation
  written by Eric Newton (newton@rocky.oswego.edu)

  This file is part of the GNU C++ Library.  This library is free
  software; you can redistribute it and/or modify it under the terms of
  the GNU Library General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your
  option) any later version.  This library is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Library General Public License for more details.
  You should have received a copy of the GNU Library General Public
  License along with this library; if not, write to the Free Software
  Foundation, 675 Mass Ave, Cambridge, MA 02139, USA.

  modified by Ulrich Drepper  (drepper@karlsruhe.gmd.de)
          and Anatoly Ivasyuk (anatoly@nick.csh.rit.edu)

  modified by Juergen Pfeifer (juergen.pfeifer@gmx.net)
*/

#include "internal.h"
#include "cursesw.h"

MODULE_ID("$Id: cursesw.cc,v 1.24 2001/12/08 21:02:33 tom Exp $")

#define COLORS_NEED_INITIALIZATION  -1
#define COLORS_NOT_INITIALIZED       0
#define COLORS_MONOCHROME            1
#define COLORS_ARE_REALLY_THERE      2

// declare static variables for the class
long NCursesWindow::count = 0L;
bool NCursesWindow::b_initialized = FALSE;

/*
 * The ncurses library has a fallback for vsscanf(), which may work...
 */
#if !(USE_STRSTREAM_VSCAN || USE_STRSTREAM_VSCAN_CAST)
#  undef  USE_STDIO_VSCAN
#  define USE_STDIO_VSCAN 1
#endif

#if defined(__GNUG__)
#  ifndef _IO_va_list
#    define _IO_va_list char *
#  endif
#endif

int
NCursesWindow::scanw(const char* fmt, ...)
{
    int result = ERR;
    char buf[BUFSIZ];

    if (::wgetnstr(w, buf, sizeof(buf)) != ERR) {
	va_list args;
	va_start(args, fmt);
#if USE_STDIO_VSCAN
	if (::vsscanf(buf, fmt, args) != -1)
	    result = OK;
#elif USE_STRSTREAM_VSCAN	/* powerpc, os390 */
	strstreambuf ss(buf, sizeof(buf));
	if (ss.vscan(fmt, args) != -1)
	    result = OK;
#elif USE_STRSTREAM_VSCAN_CAST	/* pre-gcc 3.0 */
	strstreambuf ss(buf, sizeof(buf));
	if (ss.vscan(fmt, (_IO_va_list)args) != -1)
	    result = OK;
#endif
	va_end(args);
    }
    return result;
}


int
NCursesWindow::scanw(int y, int x, const char* fmt, ...)
{
    int result = ERR;
    char buf[BUFSIZ];

    if (::wmove(w, y, x) != ERR) {
	if (::wgetnstr(w, buf, sizeof(buf)) != ERR) {
	    va_list args;
	    va_start(args, fmt);
#if USE_STDIO_VSCAN
	    if (::vsscanf(buf, fmt, args) != -1)
		result = OK;
#elif USE_STRSTREAM_VSCAN	/* powerpc, os390 */
	    strstreambuf ss(buf, sizeof(buf));
	    if (ss.vscan(fmt, args) != -1)
		result = OK;
#elif USE_STRSTREAM_VSCAN_CAST	/* pre-gcc 3.0 */
	    strstreambuf ss(buf, sizeof(buf));
	    if (ss.vscan(fmt, (_IO_va_list)args) != -1)
		result = OK;
#endif
	    va_end(args);
	}
    }
    return result;
}


int
NCursesWindow::printw(const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    char buf[BUFSIZ];
    ::vsprintf(buf, fmt, args);
    va_end(args);
    return waddstr(w, buf);
}


int
NCursesWindow::printw(int y, int x, const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int result = ::wmove(w, y, x);
    if (result == OK) {
	char buf[BUFSIZ];
	::vsprintf(buf, fmt, args);
	result = waddstr(w, buf);
    }
    va_end(args);
    return result;
}


void
NCursesWindow::init(void)
{
    leaveok(0);
    keypad(1);
    meta(1);
}

void
NCursesWindow::err_handler(const char *msg) const THROWS(NCursesException)
{
  THROW(new NCursesException(msg));
}

void
NCursesWindow::initialize() {
  if (!b_initialized) {
    ::initscr();
    b_initialized = TRUE;
    if (colorInitialized==COLORS_NEED_INITIALIZATION) {
      colorInitialized=COLORS_NOT_INITIALIZED;
      useColors();
    }
    ::noecho();
    ::cbreak();
  }
}

NCursesWindow::NCursesWindow() {
  initialize();

  w = (WINDOW *)0;
  init();
  alloced = FALSE;
  subwins = par = sib = 0;
  count++;
}

NCursesWindow::NCursesWindow(int lines, int cols, int begin_y, int begin_x)
{
    initialize();

    w = ::newwin(lines, cols, begin_y, begin_x);
    if (w == 0) {
	err_handler("Cannot construct window");
    }
    init();

    alloced = TRUE;
    subwins = par = sib = 0;
    count++;
}

NCursesWindow::NCursesWindow(WINDOW* &window)
{
    initialize();

    w = window;
    init();
    alloced = FALSE;
    subwins = par = sib = 0;
    count++;
}

NCursesWindow::NCursesWindow(NCursesWindow& win, int l, int c,
			     int begin_y, int begin_x, char absrel)
{
    initialize();
    if (absrel == 'a') { // absolute origin
	begin_y -= win.begy();
	begin_x -= win.begx();
    }

    // Even though we treat subwindows as a tree, the standard curses
    // library needs the `subwin' call to link to the parent in
    // order to correctly perform refreshes, etc.
    // Friendly enough, this also works for pads.
    w = ::derwin(win.w, l, c, begin_y, begin_x);
    if (w == 0) {
	err_handler("Cannot construct subwindow");
    }

    par = &win;
    sib = win.subwins;
    win.subwins = this;
    subwins = 0;
    alloced = TRUE;
    count++;
}

NCursesWindow::NCursesWindow(NCursesWindow& win,
				bool do_box NCURSES_PARAM_INIT(TRUE))
{
  initialize();
  w = :: derwin(win.w,win.height()-2,win.width()-2,1,1);
  if (w == 0) {
    err_handler("Cannot construct subwindow");
  }

  par = &win;
  sib = win.subwins;
  win.subwins = this;
  subwins = 0;
  alloced = TRUE;
  count++;

  if (do_box) {
    win.box();
    win.touchwin();
  }
}

NCursesWindow NCursesWindow::Clone() {
  WINDOW *d = ::dupwin(w);
  NCursesWindow W(d);
  W.subwins = subwins;
  W.sib = sib;
  W.par = par;
  W.alloced = alloced;
  return W;
}

typedef int (*RIPOFFINIT)(NCursesWindow&);
static RIPOFFINIT R_INIT[5];       // There can't be more
static int r_init_idx   = 0;
static RIPOFFINIT* prip = R_INIT;

extern "C" int _nc_ripoffline(int,int (*init)(WINDOW*,int));

NCursesWindow::NCursesWindow(WINDOW *win, int cols) {
  initialize();
  w = win;
  assert((w->_maxx+1)==cols);
  alloced = FALSE;
  subwins = par = sib = 0;
}

int NCursesWindow::ripoff_init(WINDOW *w, int cols)
{
  int res = ERR;

  RIPOFFINIT init = *prip++;
  if (init) {
    NCursesWindow* W = new NCursesWindow(w,cols);
    res = init(*W);
  }
  return res;
}

int NCursesWindow::ripoffline(int ripoff_lines,
			      int (*init)(NCursesWindow& win)) {
  int code = ::_nc_ripoffline(ripoff_lines,ripoff_init);
  if (code==OK && init && ripoff_lines) {
    R_INIT[r_init_idx++] = init;
  }
  return code;
}

bool
NCursesWindow::isDescendant(NCursesWindow& win) {
  for (NCursesWindow* p = subwins; p != NULL; p = p->sib) {
    if (p==&win)
      return TRUE;
    else {
      if (p->isDescendant(win))
	return TRUE;
    }
  }
  return FALSE;
}

void
NCursesWindow::kill_subwindows()
{
    for (NCursesWindow* p = subwins; p != 0; p = p->sib) {
	p->kill_subwindows();
	if (p->alloced) {
	    if (p->w != 0)
		::delwin(p->w);
	    p->alloced = FALSE;
	}
	p->w = 0; // cause a run-time error if anyone attempts to use...
    }
}


NCursesWindow::~NCursesWindow()
{
    kill_subwindows();

    if (par != 0) {  // Snip us from the parent's list of subwindows.
	NCursesWindow * win = par->subwins;
	NCursesWindow * trail = 0;
	for (;;) {
	    if (win == 0)
		break;
	    else if (win == this) {
		if (trail != 0)
		    trail->sib = win->sib;
		else
		    par->subwins = win->sib;
		break;
	    } else {
		trail = win;
		win = win->sib;
	    }
	}
    }

    if (alloced && w != 0)
	::delwin(w);

    if (alloced) {
      --count;
      if (count == 0) {
	::endwin();
      }
      else if (count < 0) { // cannot happen!
	err_handler("Too many windows destroyed");
      }
    }
}

// ---------------------------------------------------------------------
// Color stuff
//
int NCursesWindow::colorInitialized = COLORS_NOT_INITIALIZED;

void
NCursesWindow::useColors(void)
{
    if (colorInitialized == COLORS_NOT_INITIALIZED) {
      if (b_initialized) {
	if (::has_colors()) {
	  ::start_color();
	  colorInitialized = COLORS_ARE_REALLY_THERE;
	}
	else
	  colorInitialized = COLORS_MONOCHROME;
      }
      else
	colorInitialized = COLORS_NEED_INITIALIZATION;
    }
}

short
NCursesWindow::getcolor(int getback) const
{
    short fore, back;

    if (colorInitialized==COLORS_ARE_REALLY_THERE) {
      if (::pair_content((short)PAIR_NUMBER(w->_attrs), &fore, &back))
	err_handler("Can't get color pair");
    }
    else {
      // Monochrome means white on black
      back = COLOR_BLACK;
      fore = COLOR_WHITE;
    }
    return getback ? back : fore;
}

int NCursesWindow::NumberOfColors()
{
  if (colorInitialized==COLORS_ARE_REALLY_THERE)
    return COLORS;
  else
    return 1; // monochrome (actually there are two ;-)
}

short
NCursesWindow::getcolor() const
{
  if (colorInitialized==COLORS_ARE_REALLY_THERE)
    return PAIR_NUMBER(w->_attrs);
  else
    return 0; // we only have pair zero
}

int
NCursesWindow::setpalette(short fore, short back, short pair)
{
  if (colorInitialized==COLORS_ARE_REALLY_THERE)
    return ::init_pair(pair, fore, back);
  else
    return OK;
}

int
NCursesWindow::setpalette(short fore, short back)
{
  if (colorInitialized==COLORS_ARE_REALLY_THERE)
    return setpalette(fore, back, (short)PAIR_NUMBER(w->_attrs));
  else
    return OK;
}


int
NCursesWindow::setcolor(short pair)
{
  if (colorInitialized==COLORS_ARE_REALLY_THERE) {
    if ((pair < 1) || (pair > COLOR_PAIRS))
      err_handler("Can't set color pair");

    attroff(A_COLOR);
    attrset(COLOR_PAIR(pair));
  }
  return OK;
}

#if HAVE_HAS_KEY
extern "C" int _nc_has_mouse(void);

bool NCursesWindow::has_mouse() const {
  return ((::has_key(KEY_MOUSE) || ::_nc_has_mouse())
	  ? TRUE : FALSE);
}
#endif
