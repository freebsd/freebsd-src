#ifndef _G_curses_h

#include <_G_config.h>

#if _G_HAVE_CURSES

#ifdef __curses_h_recursive
#include_next <curses.h>
#else
#define __curses_h_recursive

extern "C" {
#include_next <curses.h>

/* Some systems (SVR4 for example) allow the definition of CHTYPE to set the
   type of some arguments to the curses functions.  It can be set to "char"
   to save space, or it can be set to something longer to store both a
   character and some attributes.  By default they do not define CHTYPE,
   and when CHTYPE is not defined, the default type is "unsigned long" instead
   of the traditional "char".  However, SVR4 <curses.h> does define
   _VR3_COMPAT_CODE, so we can use that to detect when we should use the SVR4
   default if CHTYPE is not defined.  For other systems, just default to the
   traditional default "char". */

#ifdef CHTYPE
      typedef CHTYPE _G_chtype;		/* Use specified type. */
#else
#ifdef _VR3_COMPAT_CODE
      typedef unsigned long _G_chtype;	/* SVR4 default is "unsigned long" */
#elif defined(hpux)
      typedef unsigned int _G_chtype;
#else
      typedef char _G_chtype;		/* Traditional default is "char" */
#endif
#endif

/* Some args are conceptually const, but SVR4 (and others?) get it wrong. */
#define _C_const /* const */

WINDOW * (newwin)(int, int, int, int);
WINDOW * (subwin)(WINDOW *, int, int, int, int);
WINDOW * (initscr)();
int      (box) (WINDOW*, _G_chtype, _G_chtype);
int      (delwin)(WINDOW*);
int      (getcurx)(WINDOW*);
int      (getcury)(WINDOW*);
int      (mvcur)(int, int, int, int);
int      (overlay)(WINDOW*, WINDOW*);
int      (overwrite)(WINDOW*, WINDOW*);
int      (scroll)(WINDOW*);
int      (touchwin)(WINDOW*);
int      (waddch)(WINDOW*, _G_chtype);
int      (waddstr) _G_ARGS((WINDOW*, const char*));
int      (wclear)(WINDOW*);
int      (wclrtobot)(WINDOW*);
int      (wclrtoeol)(WINDOW*);
int      (wdelch)(WINDOW*);
int      (wdeleteln)(WINDOW*);
int      (werase)(WINDOW*);
int      (wgetch)(WINDOW*);
int      (wgetstr)(WINDOW*, char*);
int      (winsch)(WINDOW*, _G_chtype);
int      (winsertln)(WINDOW*);
int      (wmove)(WINDOW*, int, int);
int      (wrefresh)(WINDOW*);
int      (wstandend)(WINDOW*);
int      (wstandout)(WINDOW*);

// SVR4 rather inanely bundles the format-string parameter with the '...'.
// This breaks VMS, and I don't want to penalize VMS for being right for once!

int      (wprintw)(WINDOW*, _G_CURSES_FORMAT_ARG ...);
int      (mvwprintw)(WINDOW*, int y, int x, _G_CURSES_FORMAT_ARG ...);
int      (wscanw)(WINDOW*, _G_CURSES_FORMAT_ARG ...);
int      (mvwscanw)(WINDOW*, int, int, _G_CURSES_FORMAT_ARG ...);
int      (endwin)();

}
#define _G_curses_h
#endif
#endif /* _G_HAVE_CURSES */
#endif /* _G_curses_h */
