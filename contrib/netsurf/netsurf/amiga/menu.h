/*
 * Copyright 2008,2009,2013 Chris Young <chris@unsatisfactorysoftware.co.uk>
 *
 * This file is part of NetSurf, http://www.netsurf-browser.org/
 *
 * NetSurf is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * NetSurf is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef AMIGA_MENU_H
#define AMIGA_MENU_H
#include <exec/types.h>
#include <intuition/intuition.h>
#include <libraries/gadtools.h>
#include "content/hlcache.h"

/* Maximum number of hotlist items (somewhat arbitrary value) */
#define AMI_HOTLIST_ITEMS 60

/* Maximum number of ARexx menu items (somewhat arbitrary value) */
#define AMI_MENU_AREXX_ITEMS 20

/* enum menu structure, has to be here as we need it below. */
enum {
	/* Project menu */
	M_PROJECT = 0,
	 M_NEWWIN,
	 M_NEWTAB,
	 M_BAR_P1,
	 M_OPEN,
	 M_SAVEAS,
	  M_SAVESRC,
	  M_SAVETXT,
	  M_SAVECOMP,
	  M_SAVEIFF,
#ifdef WITH_PDF_EXPORT
	  M_SAVEPDF,
#endif
	 M_BAR_P2,
 	 M_PRINT,
	 M_BAR_P3,
	 M_CLOSETAB,
	 M_CLOSEWIN,
	 M_BAR_P4,
	 M_ABOUT,
	 M_BAR_P5,
	 M_QUIT,
	/* Edit menu */
	M_EDIT,
	 M_CUT,
	 M_COPY,
	 M_PASTE,
	 M_BAR_E1,
	 M_SELALL,
	 M_CLEAR,
	 M_BAR_E2,
	 M_UNDO,
	 M_REDO,
	/* Browser menu */
	M_BROWSER,
	 M_FIND,
	 M_BAR_B1,
	 M_HISTLOCL,
	 M_HISTGLBL,
	 M_BAR_B2,
	 M_COOKIES,
	 M_BAR_B3,
	 M_SCALE,
	  M_SCALEDEC,
	  M_SCALENRM,
	  M_SCALEINC,
	 M_IMAGES,
	  M_IMGFORE,
	  M_IMGBACK,
#if defined(WITH_JS) || defined(WITH_MOZJS)
	 M_JS,
#endif
	 M_BAR_B4,
	 M_REDRAW,
	/* Hotlist menu */
	M_HOTLIST,
	 M_HLADD,
	 M_HLSHOW,
	 M_BAR_H1, // 47
	 AMI_MENU_HOTLIST, /* Where the hotlist entries start */
	 AMI_MENU_HOTLIST_MAX = AMI_MENU_HOTLIST + AMI_HOTLIST_ITEMS,
	/* Settings menu */
	M_PREFS,
	 M_PREDIT,
	 M_BAR_S1,
	 M_SNAPSHOT,
	 M_PRSAVE,
	/* ARexx menu */
	M_AREXX,
	 M_AREXXEX,
	 M_BAR_A1,
	 AMI_MENU_AREXX,
	 AMI_MENU_AREXX_MAX = AMI_MENU_AREXX + AMI_MENU_AREXX_ITEMS
};

/* We can get away with AMI_MENU_MAX falling short as it is
 * only used for freeing the UTF-8 converted menu labels */
#define AMI_MENU_MAX AMI_MENU_AREXX

/* The Intuition menu numbers of some menus we might need to modify */
#define AMI_MENU_SAVEAS_TEXT FULLMENUNUM(0,4,1)
#define AMI_MENU_SAVEAS_COMPLETE FULLMENUNUM(0,4,2)
#define AMI_MENU_SAVEAS_IFF FULLMENUNUM(0,4,3)
#define AMI_MENU_SAVEAS_PDF FULLMENUNUM(0,4,4)
#define AMI_MENU_CLOSETAB FULLMENUNUM(0,8,0)
#define AMI_MENU_CUT FULLMENUNUM(1,0,0)
#define AMI_MENU_COPY FULLMENUNUM(1,1,0)
#define AMI_MENU_PASTE FULLMENUNUM(1,2,0)
#define AMI_MENU_SELECTALL FULLMENUNUM(1,4,0)
#define AMI_MENU_CLEAR FULLMENUNUM(1,5,0)
#define AMI_MENU_UNDO FULLMENUNUM(1,8,0)
#define AMI_MENU_REDO FULLMENUNUM(1,9,0)
#define AMI_MENU_FIND FULLMENUNUM(2,0,0)
#define AMI_MENU_FOREIMG FULLMENUNUM(2,8,0)
#define AMI_MENU_BACKIMG FULLMENUNUM(2,8,1)
#define AMI_MENU_JS FULLMENUNUM(2,9,0)

/* A special value for ami_menu_window_close */
#define AMI_MENU_WINDOW_CLOSE_ALL (void *)1

struct gui_window;
struct gui_window_2;

struct gui_window_2 *ami_menu_window_close;
bool ami_menu_check_toggled;

void ami_free_menulabs(struct gui_window_2 *gwin);
struct NewMenu *ami_create_menu(struct gui_window_2 *gwin);
void ami_menu_refresh(struct gui_window_2 *gwin);
void ami_menu_update_checked(struct gui_window_2 *gwin);
void ami_menu_update_disabled(struct gui_window *g, hlcache_handle *c);
void ami_menu_free_glyphs(void);
#endif
