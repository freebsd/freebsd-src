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
 * Line editor
 */
int line_edit(WINDOW* dialog, int box_y, int box_x, int box_width, chtype attr, int first, unsigned char *result)
{
  int i, key;
  static int input_x, scroll;
  static unsigned char instr[MAX_LEN+1];

  if (first) {
    keypad(dialog, TRUE);
    memset(instr, 0, sizeof(instr));
    input_x = scroll = 0;
  }

  wattrset(dialog, attr);
  wmove(dialog, box_y, box_x + input_x);
  wrefresh(dialog);
  for (;;) {
    key = wgetch(dialog);
    switch (key) {
      case TAB:
      case KEY_BTAB:
      case KEY_UP:
      case KEY_DOWN:
      case ESC:
      case '\n':
	for (i = strlen(instr) - 1; i >= scroll + input_x && instr[i] == ' '; i--)
	  instr[i] = '\0';
	goto ret;
      case KEY_HOME:
	input_x = scroll = 0;
	wmove(dialog, box_y, box_x);
	for (i = 0; i < box_width; i++)
	  waddch(dialog, instr[i] ? instr[i] : ' ');
	wmove(dialog, box_y, box_x);
	wrefresh(dialog);
	continue;
      case KEY_END:
	for (i = strlen(instr) - 1; i >= scroll + input_x && instr[i] == ' '; i--)
	  instr[i] = '\0';
	i++;
	input_x = i % box_width;
	scroll = i - input_x;
	wmove(dialog, box_y, box_x);
	for (i = 0; i < box_width; i++)
	  waddch(dialog, instr[scroll+i] ? instr[scroll+i] : ' ');
	wmove(dialog, box_y, input_x + box_x);
	wrefresh(dialog);
	continue;
      case KEY_LEFT:
	if (input_x || scroll) {
	  wattrset(dialog, inputbox_attr);
	  if (!input_x) {
	    int oldscroll = scroll;
	    scroll = scroll < box_width-1 ? 0 : scroll-(box_width-1);
	    wmove(dialog, box_y, box_x);
	    for (i = 0; i < box_width; i++)
	      waddch(dialog, instr[scroll+input_x+i] ? instr[scroll+input_x+i] : ' ');
	    input_x = oldscroll - 1 - scroll;
	  }
	  else
	    input_x--;
	  wmove(dialog, box_y, input_x + box_x);
	  wrefresh(dialog);
	}
	continue;
      case KEY_RIGHT:
	  if (scroll+input_x < MAX_LEN) {
	    wattrset(dialog, inputbox_attr);
	    if (!instr[scroll+input_x])
	      instr[scroll+input_x] = ' ';
	    if (input_x == box_width-1) {
	      scroll++;
	      wmove(dialog, box_y, box_x);
	      for (i = 0; i < box_width; i++)
		waddch(dialog, instr[scroll+i] ? instr[scroll+i] : ' ');
	      wmove(dialog, box_y, box_x + box_width - 1);
	    }
	    else {
	      wmove(dialog, box_y, input_x + box_x);
	      waddch(dialog, instr[scroll+input_x]);
	      input_x++;
	    }
	    wrefresh(dialog);
	  } else
	    flash(); /* Alarm user about overflow */
	continue;
      case KEY_BACKSPACE:
      case KEY_DC:
	if (input_x || scroll) {
	  i = strlen(instr);
	  memmove(instr+scroll+input_x-1, instr+scroll+input_x, i-scroll+input_x+1);
	  wattrset(dialog, inputbox_attr);
	  if (!input_x) {
	    int oldscroll = scroll;
	    scroll = scroll < box_width-1 ? 0 : scroll-(box_width-1);
	    wmove(dialog, box_y, box_x);
	    for (i = 0; i < box_width; i++)
	      waddch(dialog, instr[scroll+input_x+i] ? instr[scroll+input_x+i] : ' ');
	    input_x = oldscroll - 1 - scroll;
	  }
	  else
	    input_x--;
	  wmove(dialog, box_y, input_x + box_x);
	  for (i = input_x; i < box_width; i++)
	    waddch(dialog, instr[scroll+i] ? instr[scroll+i] : ' ');
	  wmove(dialog, box_y, input_x + box_x);
	  wrefresh(dialog);
	}
	continue;
      default:
	if (key < 0x100 && isprint(key)) {
	  for (i = strlen(instr) - 1; i >= scroll + input_x && instr[i] == ' '; i--)
	    instr[i] = '\0';
	  i++;
	  if (i < MAX_LEN) {
	    memmove(instr+scroll+input_x+1, instr+scroll+input_x, i-scroll+input_x);
	    wattrset(dialog, inputbox_attr);
	    instr[scroll+input_x] = key;
	    if (input_x == box_width-1) {
	      scroll++;
	      wmove(dialog, box_y, box_x);
	      for (i = 0; i < box_width-1; i++)
		waddch(dialog, instr[scroll+i]);
	    }
	    else {
	      wmove(dialog, box_y, input_x + box_x);
	      for (i = input_x; i < box_width; i++)
		waddch(dialog, instr[scroll+i] ? instr[scroll+i] : ' ');
	      wmove(dialog, box_y, ++input_x + box_x);
	    }
	    wrefresh(dialog);
	  } else
	    flash(); /* Alarm user about overflow */
	  continue;
	}
      }
    }
ret:
    strcpy(result, instr);
    return key;
}
