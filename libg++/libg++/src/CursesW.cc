/* 
Copyright (C) 1989, 1992 Free Software Foundation
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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/
#ifdef __GNUG__
#pragma implementation
#endif
#include <stdio.h>
#include <stdarg.h>
#include <builtin.h>
#ifndef _OLD_STREAMS
#include <strstream.h>
#endif
// Include CurseW.h and/or curses.h *after* iostream includes,
// because curses.h defines a clear macro that conflicts with iostream. Sigh.
#include <CursesW.h>

#if _G_HAVE_CURSES

#ifndef OK
#define OK (1)
#endif

int CursesWindow::count = 0;

/*
 * C++ interface to curses library.
 *
 */

#if !defined(_IO_MAGIC) && !defined(HAVE_VSCANF) &&!defined vsscanf
extern "C" int _doscan(FILE *, const char*, va_list args);

static int vsscanf(char *buf, const char * fmt, va_list args)
{
  FILE b;
#ifdef _IOSTRG
  b._flag = _IOREAD|_IOSTRG;
#else
  b._flag = _IOREAD;
#endif
  b._base = (unsigned char*)buf;
  b._ptr = (unsigned char*)buf;
  b._cnt = BUFSIZ;
  return _doscan(&b, fmt, args);
}
#endif

/*
 * varargs functions are handled conservatively:
 * They interface directly into the underlying 
 * _doscan, _doprnt and/or vfprintf routines rather than
 * assume that such things are handled compatibly in the curses library
 */

int CursesWindow::scanw(const char * fmt, ...)
{
  va_list args;
  va_start(args, fmt);
#ifdef VMS
  int result = wscanw(w , fmt , args);
#else /* NOT VMS */
  char buf[BUFSIZ];
  int result = wgetstr(w, buf);
  if (result == OK) {

#ifdef _IO_MAGIC /* GNU iostreams */
    strstreambuf ss(buf, BUFSIZ);
    result = ss.vscan(fmt, args);
#else
    result = vsscanf(buf, fmt, args);
#endif
  }
#endif /* !VMS */
  va_end(args);
  return result;
}

int CursesWindow::mvscanw(int y, int x, const char * fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  char buf[BUFSIZ];
  int result = wmove(w, y, x);
  if (result == OK)
#ifdef VMS
  result=wscanw(w , fmt , args);
#else /* !VMS */
 {
    result = wgetstr(w, buf);
    if (result == OK) {
#ifdef _IO_MAGIC /* GNU iostreams */
    strstreambuf ss(buf, BUFSIZ);
    result = ss.vscan(fmt, args);
#else
    result = vsscanf(buf, fmt, args);
#endif
  }
  }
#endif /* !VMS */
  va_end(args);
  return result;
}

int CursesWindow::printw(const char * fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  char buf[BUFSIZ];
  vsprintf(buf, fmt, args);
  va_end(args);
  return waddstr(w, buf);
}


int CursesWindow::mvprintw(int y, int x, const char * fmt, ...)
{
  va_list args;
  va_start(args, fmt);
  int result = wmove(w, y, x);
  if (result == OK)
  {
    char buf[BUFSIZ];
    vsprintf(buf, fmt, args);
    result = waddstr(w, buf);
  }
  va_end(args);
  return result;
}

CursesWindow::CursesWindow(int lines, int cols, int begin_y, int begin_x)
{
  if (count==0)
    initscr();

  w = newwin(lines, cols, begin_y, begin_x);
  if (w == 0)
  {
    (*lib_error_handler)("CursesWindow", "Cannot construct window");
  }

  alloced = 1;
  subwins = par = sib = 0;
  count++;
}

CursesWindow::CursesWindow(WINDOW* &window)
{
  if (count==0)
    initscr();

  w = window;
  alloced = 0;
  subwins = par = sib = 0;
  count++;
}

CursesWindow::CursesWindow(CursesWindow& win, int l, int c, 
                           int by, int bx, char absrel)
{

  if (absrel == 'r') // relative origin
  {
    by += win.begy();
    bx += win.begx();
  }

  // Even though we treat subwindows as a tree, the standard curses
  // library needs the `subwin' call to link to the root in
  // order to correctly perform refreshes, etc.

  CursesWindow* root = &win;
  while (root->par != 0) root = root->par;

  w = subwin(root->w, l, c, by, bx);
  if (w == 0)
  {
    (*lib_error_handler)("CursesWindow", "Cannot construct subwindow");
  }

  par = &win;
  sib = win.subwins;
  win.subwins = this;
  subwins = 0;
  alloced = 1;
  count++;
}


void CursesWindow::kill_subwindows()
{
  for (CursesWindow* p = subwins; p != 0; p = p->sib)
  {
    p->kill_subwindows();
    if (p->alloced)
    {
      if (p->w != 0)
        ::delwin(p->w);
      p->alloced = 0;
    }
    p->w = 0; // cause a run-time error if anyone attempts to use...
  }
}
    
CursesWindow::~CursesWindow() 
{
  kill_subwindows();

  if (par != 0)   // Snip us from the parent's list of subwindows.
  {
    CursesWindow * win = par->subwins;
    CursesWindow * trail = 0;
    for (;;)
    {
      if (win == 0)
        break;
      else if (win == this)
      {
        if (trail != 0)
          trail->sib = win->sib;
        else
          par->subwins = win->sib;
        break;
      }
      else
      {
        trail = win;
        win = win->sib;
      }
    }
  }

  if (alloced && w != 0) 
    delwin(w);

  --count;
  if (count == 0) 
    endwin();
  else if (count < 0) // cannot happen!
  {
    (*lib_error_handler)("CursesWindow", "Too many windows destroyed");
  }
}

#endif /* _G_HAVE_CURSES */
