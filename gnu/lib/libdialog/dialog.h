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

#define HAVE_NCURSES

#ifdef HAVE_NCURSES
#include <ncurses.h>

#else

#ifdef ultrix
#include <cursesX.h>
#else
#include <curses.h>
#endif

#endif

#define VERSION "0.4"
#define MAX_LEN 2048

#ifdef HAVE_NCURSES
extern bool use_shadow;
void draw_shadow(WINDOW *win, int y, int x, int height, int width);
#endif
void draw_box(WINDOW *win, int y, int x, int height, int width, chtype box, chtype border);
int line_edit(WINDOW* dialog, int box_y, int box_x, int box_width, chtype attrs, int first, unsigned char *result);

void dialog_create_rc(unsigned char *filename);
int dialog_yesno(unsigned char *title, unsigned char *prompt, int height, int width);
int dialog_msgbox(unsigned char *title, unsigned char *prompt, int height, int width, int pause);
int dialog_textbox(unsigned char *title, unsigned char *file, int height, int width);
int dialog_menu(unsigned char *title, unsigned char *prompt, int height, int width, int menu_height, int item_no, unsigned char **items, unsigned char *result);
int dialog_checklist(unsigned char *title, unsigned char *prompt, int height, int width, int list_height, int item_no, unsigned char **items, unsigned char *result);
int dialog_radiolist(char *title, char *prompt, int height, int width, int list_height, int item_no, unsigned char **items, unsigned char *result);
int dialog_inputbox(unsigned char *title, unsigned char *prompt, int height, int width, unsigned char *result);
void dialog_clear(void);
void dialog_update(void);
void init_dialog(void);
void end_dialog(void);
