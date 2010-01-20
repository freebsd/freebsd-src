/*
 *  yesno.c -- implements the yes/no box
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dialog.h>
#include "dialog.priv.h"

/* Actual work function */
static int dialog_yesno_proc(unsigned char *title, unsigned char *prompt,
			     int height, int width, int yesdefault);

/*
 * Display a dialog box with two buttons - Yes and No
 */
int
dialog_yesno(unsigned char *title, unsigned char *prompt, int height, int width)
{
  return dialog_yesno_proc(title, prompt, height, width, TRUE);
}

/*
 * Display a dialog box with two buttons - No and Yes
 */
int
dialog_noyes(unsigned char *title, unsigned char *prompt, int height, int width)
{
  return dialog_yesno_proc(title, prompt, height, width, FALSE);
}

static int
dialog_yesno_proc(unsigned char *title, unsigned char *prompt, int height, int width, int yesdefault)
{
  int i, j, x, y, key, button;
  WINDOW *dialog;
  char *tmphlp;

  /* disable helpline */
  tmphlp = get_helpline();
  use_helpline(NULL);

  if (height < 0)
	height = strheight(prompt)+4;
  if (width < 0) {
	i = strwidth(prompt);
	j = ((title != NULL) ? strwidth(title) : 0);
	width = MAX(i,j)+4;
  }
  width = MAX(width,23);

  if (width > COLS)
	width = COLS;
  if (height > LINES)
	height = LINES;
  /* center dialog box on screen */
  x = DialogX ? DialogX : (COLS - width)/2;
  y = DialogY ? DialogY : (LINES - height)/2;

#ifdef HAVE_NCURSES
  if (use_shadow)
    draw_shadow(stdscr, y, x, height, width);
#endif
  dialog = newwin(height, width, y, x);
  if (dialog == NULL) {
    endwin();
    fprintf(stderr, "\nnewwin(%d,%d,%d,%d) failed, maybe wrong dims\n", height,width,y,x);
    exit(1);
  }
  keypad(dialog, TRUE);

  draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
  wattrset(dialog, border_attr);
  wmove(dialog, height-3, 0);
  waddch(dialog, ACS_LTEE);
  for (i = 0; i < width-2; i++)
    waddch(dialog, ACS_HLINE);
  wattrset(dialog, dialog_attr);
  waddch(dialog, ACS_RTEE);
  wmove(dialog, height-2, 1);
  for (i = 0; i < width-2; i++)
    waddch(dialog, ' ');

  if (title != NULL) {
    wattrset(dialog, title_attr);
    wmove(dialog, 0, (width - strlen(title))/2 - 1);
    waddch(dialog, ' ');
    waddstr(dialog, title);
    waddch(dialog, ' ');
  }
  wattrset(dialog, dialog_attr);
  wmove(dialog, 1, 2);
  print_autowrap(dialog, prompt, height-1, width-2, width, 1, 2, TRUE, FALSE);

  display_helpline(dialog, height-1, width);

  x = width/2-10;
  y = height-2;

  /* preset button 0 or 1 for YES or NO as the default */
  key = 0;
  button = !yesdefault;
  while (key != ESC) {
    print_button(dialog, "  No  ", y, x+13, button);
    print_button(dialog, " Yes " , y, x, !button);
    if (button)
	wmove(dialog, y, x+16);
    else
	wmove(dialog, y, x+2);
    wrefresh(dialog);

    key = wgetch(dialog);
    switch (key) {
      case 'Y':
      case 'y':
        delwin(dialog);
	restore_helpline(tmphlp);
        return 0;
      case 'N':
      case 'n':
        delwin(dialog);
	restore_helpline(tmphlp);
        return 1;
      case KEY_BTAB:
      case TAB:
      case KEY_UP:
      case KEY_DOWN:
      case KEY_LEFT:
      case KEY_RIGHT:
        button = !button;
        /* redrawn at the loop's entry */
        break;
      case ' ':
      case '\r':
      case '\n':
        delwin(dialog);
	restore_helpline(tmphlp);
        return button;
      case ESC:
        break;
    case KEY_F(1):
    case '?':
	display_helpfile();
	break;
    }
  }

  delwin(dialog);
  restore_helpline(tmphlp);
  return -1;    /* ESC pressed */
}
/* End of dialog_yesno() */
