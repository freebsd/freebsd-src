// This may look like C code, but it is really -*- C++ -*-

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
Foundation, 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#ifndef _CursesWindow_h
#ifdef __GNUG__
#pragma interface
#endif
#define _CursesWindow_h

#include   <_G_config.h>

#if defined(__bsdi__) || defined(__NetBSD__) || defined(__FreeBSD__)
#define _begx begx
#define _begy begy
#define _maxx maxx
#define _maxy maxy
#endif

#if _G_HAVE_CURSES
// Even many system which mostly have C++-ready header files,
// do not have C++-ready curses.h.
extern "C" {
#include   <curses.h>
}

/* SCO 3.2v4 curses.h includes term.h, which defines lines as a macro.
   Undefine it here, because CursesWindow uses lines as a method.  */
#undef lines

// "Convert" macros to inlines, if needed.
#ifdef addch
inline int (addch)(char ch)  { return addch(ch); }
#undef addch
#endif
#ifdef addstr
/* The (char*) cast is to hack around missing const's */
inline int (addstr)(const char * str)  { return addstr((char*)str); }
#undef addstr
#endif
#ifdef clear
inline int (clear)()  { return clear(); }
#undef clear
#endif
#ifdef clearok
inline int (clearok)(WINDOW* win, int bf)  { return clearok(win, bf); }
#undef clearok
#else
extern "C" int clearok(WINDOW*, int);
#endif
#ifdef clrtobot
inline int (clrtobot)()  { return clrtobot(); }
#undef clrtobot
#endif
#ifdef clrtoeol
inline int (clrtoeol)()  { return clrtoeol(); }
#undef clrtoeol
#endif
#ifdef delch
inline int (delch)()  { return delch(); }
#undef delch
#endif
#ifdef deleteln
inline int (deleteln)()  { return deleteln(); }
#undef deleteln
#endif
#ifdef erase
inline int (erase)()  { return erase(); }
#undef erase
#endif
#ifdef flushok
inline int (flushok)(WINDOW* _win, int _bf)  { return flushok(_win, _bf); }
#undef flushok
#else
#define _no_flushok
#endif
#ifdef getch
inline int (getch)()  { return getch(); }
#undef getch
#endif
#ifdef getstr
inline int (getstr)(char *_str)  { return getstr(_str); }
#undef getstr
#endif
#ifdef getyx
inline void (getyx)(WINDOW* win, int& y, int& x) { getyx(win, y, x); }
#undef getyx
#endif
#ifdef inch
inline int (inch)()  { return inch(); }
#undef inch
#endif
#ifdef insch
inline int (insch)(char c)  { return insch(c); }
#undef insch
#endif
#ifdef insertln
inline int (insertln)()  { return insertln(); }
#undef insertln
#endif
#ifdef leaveok
inline int (leaveok)(WINDOW* win, int bf)  { return leaveok(win, bf); }
#undef leaveok
#else
extern "C" int leaveok(WINDOW* win, int bf);
#endif
#ifdef move
inline int (move)(int x, int y)  { return move(x, y); }
#undef move
#endif
#ifdef refresh
inline int (rfresh)()  { return refresh(); }
#undef refresh
#endif
#ifdef scrollok
inline int (scrollok)(WINDOW* win, int bf)  { return scrollok(win, bf); }
#undef scrollok
#else
#ifndef hpux
extern "C" int scrollok(WINDOW*, int);
#else
extern "C" int scrollok(WINDOW*, char);
#endif
#endif
#ifdef standend
inline int (standend)()  { return standend(); }
#undef standend
#endif
#ifdef standout
inline int (standout)()  { return standout(); }
#undef standout
#endif
#ifdef wstandend
inline int (wstandend)(WINDOW *win)  { return wstandend(win); }
#undef wstandend
#endif
#ifdef wstandout
inline int (wstandout)(WINDOW *win)  { return wstandout(win); }
#undef wstandout
#endif
#ifdef winch
inline int (winch)(WINDOW* win) { return winch(win); }
#undef winch
#endif

/* deal with conflicting macros in ncurses.h  which is SYSV based*/
#ifdef box
inline int _G_box(WINDOW* win, chtype v, chtype h) {return box(win, v, h); }
#undef box
inline int box(WINDOW* win, chtype v, chtype h) {return _G_box(win, v, h); }
#endif
#ifdef scroll
inline int (scroll)(WINDOW* win) { return scroll(win); }
#undef scroll
#endif
#ifdef touchwin
inline int (touchwin)(WINDOW* win) { return touchwin(win); }
#undef touchwin
#endif

#ifdef mvwaddch
inline int (mvwaddch)(WINDOW *win, int y, int x, char ch)
{ return mvwaddch(win, y, x, ch); }
#undef mvwaddch
#endif
#ifdef mvwaddstr
inline int (mvwaddstr)(WINDOW *win, int y, int x, const char * str)
{ return mvwaddstr(win, y, x, (char*)str); }
#undef mvwaddstr
#endif
#ifdef mvwdelch
inline int (mvwdelch)(WINDOW *win, int y, int x) { return mvwdelch(win, y, x);}
#undef mvwdelch
#endif
#ifdef mvwgetch
inline int (mvwgetch)(WINDOW *win, int y, int x) { return mvwgetch(win, y, x);}
#undef mvwgetch
#endif
#ifdef mvwgetstr
inline int (mvwgetstr)(WINDOW *win, int y, int x, char *str)
{return mvwgetstr(win,y,x, str);}
#undef mvwgetstr
#endif
#ifdef mvwinch
inline int (mvwinch)(WINDOW *win, int y, int x) { return mvwinch(win, y, x);}
#undef mvwinch
#endif
#ifdef mvwinsch
inline int (mvwinsch)(WINDOW *win, int y, int x, char c)
{ return mvwinsch(win, y, x, c); }
#undef mvwinsch
#endif

#ifdef mvaddch
inline int (mvaddch)(int y, int x, char ch)
{ return mvaddch(y, x, ch); }
#undef mvaddch
#endif
#ifdef mvaddstr
inline int (mvaddstr)(int y, int x, const char * str)
{ return mvaddstr(y, x, (char*)str); }
#undef mvaddstr
#endif
#ifdef mvdelch
inline int (mvdelch)(int y, int x) { return mvdelch(y, x);}
#undef mvdelch
#endif
#ifdef mvgetch
inline int (mvgetch)(int y, int x) { return mvgetch(y, x);}
#undef mvgetch
#endif
#ifdef mvgetstr
inline int (mvgetstr)(int y, int x, char *str) {return mvgetstr(y, x, str);}
#undef mvgetstr
#endif
#ifdef mvinch
inline int (mvinch)(int y, int x) { return mvinch(y, x);}
#undef mvinch
#endif
#ifdef mvinsch
inline int (mvinsch)(int y, int x, char c)
{ return mvinsch(y, x, c); }
#undef mvinsch
#endif

/*
 *
 * C++ class for windows.
 *
 *
 */

class CursesWindow 
{
protected:
  static int     count;           // count of all active windows:
                                  //   We rely on the c++ promise that
                                  //   all otherwise uninitialized
                                  //   static class vars are set to 0

  WINDOW *       w;               // the curses WINDOW

  int            alloced;         // true if we own the WINDOW

  CursesWindow*  par;             // parent, if subwindow
  CursesWindow*  subwins;         // head of subwindows list
  CursesWindow*  sib;             // next subwindow of parent

  void           kill_subwindows(); // disable all subwindows

public:
                 CursesWindow(WINDOW* &window);   // useful only for stdscr

                 CursesWindow(int lines,          // number of lines
                              int cols,           // number of columns
                              int begin_y,        // line origin
                              int begin_x);       // col origin

                 CursesWindow(CursesWindow& par,  // parent window
                              int lines,          // number of lines
                              int cols,           // number of columns
                              int by,             // absolute or relative
                              int bx,             //   origins:
                              char absrel = 'a'); // if `a', by & bx are
                                                  // absolute screen pos,
                                                  // else if `r', they are
                                                  // relative to par origin
                ~CursesWindow();

// terminal status
  int            lines(); // number of lines on terminal, *not* window
  int            cols();  // number of cols  on terminal, *not* window

// window status
  int            height(); // number of lines in this window
  int            width();  // number of cols in this window
  int            begx();   // smallest x coord in window
  int            begy();   // smallest y coord in window
  int            maxx();   // largest  x coord in window
  int            maxy();   // largest  x coord in window

// window positioning
  int            move(int y, int x);

// coordinate positioning
  void           getyx(int& y, int& x);
  int            mvcur(int sy, int ey, int sx, int ex);

// input
  int            getch();
  int            getstr(char * str);
  int            scanw(const char *, ...);

// input + positioning
  int            mvgetch(int y, int x);
  int            mvgetstr(int y, int x, char * str);
  int            mvscanw(int, int, const char*, ...);

// output
  int            addch(const char ch);
  int            addstr(const char * str);
  int            printw(const char * fmt, ...);
  int            inch();
  int            insch(char c);
  int            insertln();

// output + positioning
  int            mvaddch(int y, int x, char ch);
  int            mvaddstr(int y, int x, const char * str);
  int            mvprintw(int y, int x, const char * fmt, ...);
  int            mvinch(int y, int x);
  int            mvinsch(int y, int x, char ch);

// borders
  int            box(char vert, char  hor);

// erasure
  int            erase();
  int            clear();
  int            clearok(int bf);
  int            clrtobot();
  int            clrtoeol();
  int            delch();
  int            mvdelch(int y, int x);
  int            deleteln();

// screen control
  int            scroll();
  int            scrollok(int bf);
  int            touchwin();
  int            refresh();
  int            leaveok(int bf);
#ifndef _no_flushok
  int            flushok(int bf);
#endif
  int            standout();
  int            standend();

// multiple window control
  int            overlay(CursesWindow &win);
  int            overwrite(CursesWindow &win);


// traversal support
  CursesWindow*  child();
  CursesWindow*  sibling();
  CursesWindow*  parent();
};


inline int CursesWindow::begx()
{
  return w->_begx;
}

inline int CursesWindow::begy()
{
  return w->_begy;
}

inline int CursesWindow::maxx()
{
  return w->_maxx;
}

inline int CursesWindow::maxy()
{
  return w->_maxy;
}

inline int CursesWindow::height()
{
  return maxy() - begy() + 1;
}

inline int CursesWindow::width()
{
  return maxx() - begx() + 1;
}

inline int CursesWindow::box(char vert, char  hor)    
{
  return ::box(w, vert, hor); 
}

inline int CursesWindow::overlay(CursesWindow &win)         
{
  return ::overlay(w, win.w); 
}

inline int CursesWindow::overwrite(CursesWindow &win)       
{
  return ::overwrite(w, win.w); 
}

inline int CursesWindow::scroll()                     
{
  return ::scroll(w); 
}


inline int CursesWindow::touchwin()                   
{
  return ::touchwin(w); 
}

inline int CursesWindow::addch(const char ch)         
{
  return ::waddch(w, ch); 
}

inline int CursesWindow::addstr(const char * str)     
{
  // The (char*) cast is to hack around prototypes in curses.h that
  // have const missing in the parameter lists.  [E.g. SVR4]
  return ::waddstr(w, (char*)str); 
}

inline int CursesWindow::clear()                      
{
  return ::wclear(w); 
}

inline int CursesWindow::clrtobot()                   
{
  return ::wclrtobot(w); 
}

inline int CursesWindow::clrtoeol()                   
{
  return ::wclrtoeol(w); 
}

inline int CursesWindow::delch()                      
{
  return ::wdelch(w); 
}

inline int CursesWindow::deleteln()                   
{
  return ::wdeleteln(w); 
}

inline int CursesWindow::erase()                      
{
  return ::werase(w); 
}

inline int CursesWindow::getch()                      
{
  return ::wgetch(w); 
}

inline int CursesWindow::getstr(char * str)           
{
  return ::wgetstr(w, str); 
}

inline int CursesWindow::inch()                       
{
  return winch(w); 
}

inline int CursesWindow::insch(char c)               
{
  return ::winsch(w, c); 
}

inline int CursesWindow::insertln()                   
{
  return ::winsertln(w); 
}

inline int CursesWindow::move(int y, int x)           
{
  return ::wmove(w, y, x); 
}


inline int CursesWindow::mvcur(int sy, int ey, int sx, int ex)
{
  return ::mvcur(sy, ey, sx,ex);
}

inline int CursesWindow::mvaddch(int y, int x, char ch)
{
  return (::wmove(w, y, x)==ERR) ? ERR : ::waddch(w, ch);
}

inline int CursesWindow::mvgetch(int y, int x)
{
  return (::wmove(w, y, x)==ERR) ? ERR : ::wgetch(w);
}

inline int CursesWindow::mvaddstr(int y, int x, const char * str)
{
  return (::wmove(w, y, x)==ERR) ? ERR : ::waddstr(w, (char*)str);
}

inline int CursesWindow::mvgetstr(int y, int x, char * str)
{
  return (::wmove(w, y, x)==ERR) ? ERR : ::wgetstr(w, str);
}

inline int CursesWindow::mvinch(int y, int x)
{
  return (::wmove(w, y, x)==ERR) ? ERR : ::winch(w);
}

inline int CursesWindow::mvdelch(int y, int x)
{
  return (::wmove(w, y, x)==ERR) ? ERR : ::wdelch(w);
}

inline int CursesWindow::mvinsch(int y, int x, char ch)
{
  return (::wmove(w, y, x)==ERR) ? ERR : ::winsch(w, ch);
}

inline int CursesWindow::refresh()                   
{
  return ::wrefresh(w); 
}

inline int CursesWindow::clearok(int bf)             
{
  return ::clearok(w,bf); 
}

inline int CursesWindow::leaveok(int bf)             
{
  return ::leaveok(w,bf); 
}

inline int CursesWindow::scrollok(int bf)            
{
  return ::scrollok(w,bf); 
}

#ifndef _no_flushok
inline int CursesWindow::flushok(int bf)            
{
  return ::flushok(w, bf); 
}
#endif

inline void CursesWindow::getyx(int& y, int& x)       
{
  ::getyx(w, y, x); 
}

inline int CursesWindow::standout()                   
{
  return ::wstandout(w); 
}

inline int CursesWindow::standend()                   
{
  return ::wstandend(w); 
}

inline int CursesWindow::lines()                      
{
  return LINES; 
}

inline int CursesWindow::cols()                       
{
  return COLS; 
}

inline CursesWindow* CursesWindow::child()
{
  return subwins;
}

inline CursesWindow* CursesWindow::parent()
{
  return par;
}

inline CursesWindow* CursesWindow::sibling()
{
  return sib;
}

#endif /* _G_HAVE_CURSES */
#endif
