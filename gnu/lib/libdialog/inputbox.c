/*
 *  inputbox.c -- implements the input box
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


#include <dialog.h>
#include "dialog.priv.h"


/*
 * Display a dialog box for inputing a string
 */
int dialog_inputbox(unsigned char *title, unsigned char *prompt, int height, int width, unsigned char *result)
{
  int i, x, y, box_y, box_x, box_width, first,
      key = 0, button = -1;
  unsigned char instr[MAX_LEN+1];
  WINDOW *dialog;

  /* center dialog box on screen */
  x = (COLS - width)/2;
  y = (LINES - height)/2;

#ifdef HAVE_NCURSES
  if (use_shadow)
    draw_shadow(stdscr, y, x, height, width);
#endif
  dialog = newwin(height, width, y, x);
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
  print_autowrap(dialog, prompt, width, 1, 3);

  /* Draw the input field box */
  box_width = width-6;
  getyx(dialog, y, x);
  box_y = y + 2;
  box_x = (width - box_width)/2;
  draw_box(dialog, y+1, box_x-1, 3, box_width+2, border_attr, dialog_attr);

  x = width/2-11;
  y = height-2;
  print_button(dialog, "Cancel", y, x+14, FALSE);
  print_button(dialog, "  OK  ", y, x, TRUE);

  first = 1;
  while (key != ESC) {

    if (button == -1) {    /* Input box selected */
      key = line_edit(dialog, box_y, box_x, box_width, dialog_attr, first, instr);
      first = 0;
    }
    else
      key = wgetch(dialog);

    switch (key) {
      case 'O':
      case 'o':
        delwin(dialog);
	strcpy(result, instr);
        return 0;
      case 'C':
      case 'c':
        delwin(dialog);
        return 1;
      case KEY_UP:
      case KEY_LEFT:
      case KEY_BTAB:
        switch (button) {
	  case -1:
            button = 1;    /* Indicates "Cancel" button is selected */
	    print_button(dialog, "  OK  ", y, x, FALSE);
	    print_button(dialog, "Cancel", y, x+14, TRUE);
            wrefresh(dialog);
	    break;
          case 0:
            button = -1;   /* Indicates input box is selected */
	    print_button(dialog, "Cancel", y, x+14, FALSE);
	    print_button(dialog, "  OK  ", y, x, TRUE);
            break;
          case 1:
	    button = 0;    /* Indicates "OK" button is selected */
	    print_button(dialog, "Cancel", y, x+14, FALSE);
	    print_button(dialog, "  OK  ", y, x, TRUE);
            wrefresh(dialog);
            break;
        }
        break;
      case TAB:
      case KEY_DOWN:
      case KEY_RIGHT:
        switch (button) {
	  case -1:
	    button = 0;    /* Indicates "OK" button is selected */
	    print_button(dialog, "Cancel", y, x+14, FALSE);
	    print_button(dialog, "  OK  ", y, x, TRUE);
            wrefresh(dialog);
            break;
          case 0:
            button = 1;    /* Indicates "Cancel" button is selected */
	    print_button(dialog, "  OK  ", y, x, FALSE);
	    print_button(dialog, "Cancel", y, x+14, TRUE);
            wrefresh(dialog);
	    break;
          case 1:
            button = -1;   /* Indicates input box is selected */
	    print_button(dialog, "Cancel", y, x+14, FALSE);
	    print_button(dialog, "  OK  ", y, x, TRUE);
            break;
        }
        break;
      case ' ':
      case '\n':
        delwin(dialog);
	if (button < 1)
	  strcpy(result, instr);
        return (button == -1 ? 0 : button);
      case ESC:
        break;
    }
  }

  delwin(dialog);
  return -1;    /* ESC pressed */
}
/* End of dialog_inputbox() */
