/*
 *  dialog.h -- common declarations for all dialog modules
 *
 *  AUTHOR: Savio Lam (lam836@cs.cuhk.hk)
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */


#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

#if defined(LOCALE)
#include <locale.h>
#endif

  
/*
 * Change these if you want
 */
#define USE_SHADOW TRUE
#define USE_COLORS TRUE

#define ESC 27
#define TAB 9
#define BUF_SIZE (10*1024)

#ifndef MIN
#define MIN(x,y) (x < y ? x : y)
#endif
#ifndef MAX
#define MAX(x,y) (x > y ? x : y)
#endif

#ifndef ctrl
#define ctrl(a)          ((a) - 'a' + 1)
#endif

#ifndef HAVE_NCURSES
#ifndef ACS_ULCORNER
#define ACS_ULCORNER '+'
#endif
#ifndef ACS_LLCORNER
#define ACS_LLCORNER '+'
#endif
#ifndef ACS_URCORNER
#define ACS_URCORNER '+'
#endif
#ifndef ACS_LRCORNER
#define ACS_LRCORNER '+'
#endif
#ifndef ACS_HLINE
#define ACS_HLINE '-'
#endif
#ifndef ACS_VLINE
#define ACS_VLINE '|'
#endif
#ifndef ACS_LTEE
#define ACS_LTEE '+'
#endif
#ifndef ACS_RTEE
#define ACS_RTEE '+'
#endif
#ifndef ACS_UARROW
#define ACS_UARROW '^'
#endif
#ifndef ACS_DARROW
#define ACS_DARROW 'v'
#endif
#endif    /* HAVE_NCURSES */

/*
 * Global variables
 */
#ifdef __DIALOG_MAIN__

#ifdef HAVE_NCURSES

/* use colors by default? */
bool use_colors = USE_COLORS;

/* shadow dialog boxes by default?
   Note that 'use_shadow' implies 'use_colors' */
bool use_shadow = USE_SHADOW;

#endif


/*
 * Attribute values, default is for mono display
 */
chtype attributes[] = {
  A_NORMAL,       /* screen_attr */
  A_NORMAL,       /* shadow_attr */
  A_REVERSE,      /* dialog_attr */
  A_REVERSE,      /* title_attr */
  A_REVERSE,      /* border_attr */
  A_BOLD,         /* button_active_attr */
  A_DIM,          /* button_inactive_attr */
  A_UNDERLINE,    /* button_key_active_attr */
  A_UNDERLINE,    /* button_key_inactive_attr */
  A_NORMAL,       /* button_label_active_attr */
  A_NORMAL,       /* button_label_inactive_attr */
  A_REVERSE,      /* inputbox_attr */
  A_REVERSE,      /* inputbox_border_attr */
  A_REVERSE,      /* searchbox_attr */
  A_REVERSE,      /* searchbox_title_attr */
  A_REVERSE,      /* searchbox_border_attr */
  A_REVERSE,      /* position_indicator_attr */
  A_REVERSE,      /* menubox_attr */
  A_REVERSE,      /* menubox_border_attr */
  A_REVERSE,      /* item_attr */
  A_NORMAL,       /* item_selected_attr */
  A_REVERSE,      /* tag_attr */
  A_REVERSE,      /* tag_selected_attr */
  A_NORMAL,       /* tag_key_attr */
  A_BOLD,         /* tag_key_selected_attr */
  A_REVERSE,      /* check_attr */
  A_REVERSE,      /* check_selected_attr */
  A_REVERSE,      /* uarrow_attr */
  A_REVERSE       /* darrow_attr */
};

#else

#ifdef HAVE_NCURSES
extern bool use_colors;
#endif

#endif    /* __DIALOG_MAIN__ */



#ifdef HAVE_NCURSES

/*
 * Function prototypes
 */
#ifdef __DIALOG_MAIN__

extern int parse_rc(void);

#endif    /* __DIALOG_MAIN__ */

#endif


#ifdef HAVE_NCURSES
void color_setup(void);
#endif

void attr_clear(WINDOW *win, int height, int width, chtype attr);
void print_autowrap(WINDOW *win, unsigned char *prompt, int height, int width, int maxwidth,
		    int y, int x, int center, int rawmode);
void print_button(WINDOW *win, unsigned char *label, int y, int x, int selected);
FILE *raw_popen(const char *program, char * const *argv, const char *type);
int raw_pclose(FILE *iop);
void display_helpfile(void);
void display_helpline(WINDOW *w, int y, int width);
void print_arrows(WINDOW *dialog, int scroll, int menu_height, int item_no, int box_x,
		  int box_y, int tag_x, int cur_x, int cur_y);

