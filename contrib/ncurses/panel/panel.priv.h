/* $Id: panel.priv.h,v 1.10 1999/09/29 15:21:58 juergen Exp $ */

#ifndef _PANEL_PRIV_H
#define _PANEL_PRIV_H

#if HAVE_CONFIG_H
#  include <ncurses_cfg.h>
#endif

#include <stdlib.h>
#include <string.h>
#include <assert.h>

#if HAVE_LIBDMALLOC
#  include <dmalloc.h>    /* Gray Watson's library */
#endif

#if HAVE_LIBDBMALLOC
#  include <dbmalloc.h>   /* Conor Cahill's library */
#endif

#include <nc_panel.h>
#include "panel.h"

#if ( CC_HAS_INLINE_FUNCS && !defined(TRACE) )
#  define INLINE inline
#else
#  define INLINE
#endif

#ifdef USE_RCS_IDS
#  define MODULE_ID(id) static const char Ident[] = id;
#else
#  define MODULE_ID(id) /*nothing*/
#endif


#ifdef TRACE
   extern const char *_nc_my_visbuf(const void *);
#  ifdef TRACE_TXT
#    define USER_PTR(ptr) _nc_visbuf((const char *)ptr)
#  else
#    define USER_PTR(ptr) _nc_my_visbuf((const char *)ptr)
#  endif

   extern void _nc_dPanel(const char*, const PANEL*);
   extern void _nc_dStack(const char*, int, const PANEL*);
   extern void _nc_Wnoutrefresh(const PANEL*);
   extern void _nc_Touchpan(const PANEL*);
   extern void _nc_Touchline(const PANEL*, int, int);

#  define dBug(x) _tracef x
#  define dPanel(text,pan) _nc_dPanel(text,pan)
#  define dStack(fmt,num,pan) _nc_dStack(fmt,num,pan)
#  define Wnoutrefresh(pan) _nc_Wnoutrefresh(pan)
#  define Touchpan(pan) _nc_Touchpan(pan)
#  define Touchline(pan,start,count) _nc_Touchline(pan,start,count)
#else /* !TRACE */
#  define dBug(x)
#  define dPanel(text,pan)
#  define dStack(fmt,num,pan)
#  define Wnoutrefresh(pan) wnoutrefresh((pan)->win)
#  define Touchpan(pan) touchwin((pan)->win)
#  define Touchline(pan,start,count) touchline((pan)->win,start,count)
#endif

#define _nc_stdscr_pseudo_panel _nc_panelhook()->stdscr_pseudo_panel
#define _nc_top_panel _nc_panelhook()->top_panel
#define _nc_bottom_panel _nc_panelhook()->bottom_panel

#define EMPTY_STACK() (_nc_top_panel==_nc_bottom_panel)
#define Is_Bottom(p)  (((p)!=(PANEL*)0) && !EMPTY_STACK() && (_nc_bottom_panel->above==(p))) 
#define Is_Top(p) (((p)!=(PANEL*)0) && !EMPTY_STACK() && (_nc_top_panel==(p)))
#define Is_Pseudo(p) ((p) && ((p)==_nc_bottom_panel))

/*+-------------------------------------------------------------------------
	_nc_panel_is_linked(pan) - check to see if panel is in the stack
--------------------------------------------------------------------------*/
/* This works! The only case where it would fail is, when the list has
   only one element. But this could only be the pseudo panel at the bottom */
#define _nc_panel_is_linked(p) ((((p)->above!=(PANEL*)0)||((p)->below!=(PANEL*)0)||((p)==_nc_bottom_panel)) ? TRUE : FALSE)

#define PSTARTX(pan) ((pan)->win->_begx)
#define PENDX(pan)   ((pan)->win->_begx + getmaxx((pan)->win))
#define PSTARTY(pan) ((pan)->win->_begy)
#define PENDY(pan)   ((pan)->win->_begy + getmaxy((pan)->win))

/*+-------------------------------------------------------------------------
	PANELS_OVERLAPPED(pan1,pan2) - check panel overlapped
---------------------------------------------------------------------------*/
#define PANELS_OVERLAPPED(pan1,pan2) \
(( !(pan1) || !(pan2) || \
       PSTARTY(pan1) >= PENDY(pan2) || PENDY(pan1) <= PSTARTY(pan2) ||\
       PSTARTX(pan1) >= PENDX(pan2) || PENDX(pan1) <= PSTARTX(pan2) ) \
     ? FALSE : TRUE)


#define PANEL_UPDATE(pan,panstart) { int y; PANEL* pan2 = panstart;\
   if (!pan2) {\
      Touchpan(pan);\
      pan2 = _nc_bottom_panel;\
   }\
   while(pan2) {\
      if ((pan2 != pan) && PANELS_OVERLAPPED(pan,pan2)) {\
	for(y = PSTARTY(pan); y < PENDY(pan); y++) {\
	  if( (y >= PSTARTY(pan2)) && (y < PENDY(pan2)) &&\
	      ((is_linetouched(pan->win,y - PSTARTY(pan)) == TRUE)) )\
	    Touchline(pan2,y - PSTARTY(pan2),1);\
	}\
      }\
      pan2 = pan2->above;\
    }\
}
#endif /* _PANEL_PRIV_H */
