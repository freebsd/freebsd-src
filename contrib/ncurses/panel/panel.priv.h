/* $Id: panel.priv.h,v 1.8 1997/10/21 10:19:37 juergen Exp $ */

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

typedef struct panelcons
{
  struct panelcons *above;
  struct panel *pan;
} PANELCONS;

#ifdef USE_RCS_IDS
#  define MODULE_ID(id) static const char Ident[] = id;
#else
#  define MODULE_ID(id) /*nothing*/
#endif

#define P_TOUCH  (0)
#define P_UPDATE (1)

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

extern void _nc_panel_link_bottom(PANEL*);
extern bool _nc_panel_is_linked(const PANEL*);
extern void _nc_calculate_obscure(void);
extern void _nc_free_obscure(PANEL*);
extern void _nc_override(const PANEL*,int);

#endif /* _PANEL_PRIV_H */
