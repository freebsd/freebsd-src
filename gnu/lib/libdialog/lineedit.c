/*
 *  Changes Copyright (C) 1995 by Andrey A. Chernov, Moscow
 *
 *  Original Copyright:
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

static void redraw_field(WINDOW *dialog, int box_y, int box_x, int flen, int box_width, unsigned char instr[], int input_x, int scroll, chtype attr, chtype old_attr, int fexit, int attr_mask);

/*
 * Line editor
 */
int line_edit(WINDOW* dialog, int box_y, int box_x, int flen, int box_width, chtype attr, int first, unsigned char *result, int attr_mask)
{
  int i, key;
  chtype old_attr;
  static int input_x, scroll;
  static unsigned char instr[MAX_LEN+1];
  unsigned char erase_char = erasechar();
  unsigned char kill_char = killchar();
#ifdef notyet
  unsignec char werase_char = cur_term->Ottyb.c_cc[VWERASE];
#endif

  old_attr = getattrs(dialog);
  keypad(dialog, TRUE);

  if (first) {
    memset(instr, 0, sizeof(instr));
    strcpy(instr, result);
    i = strlen(instr);
/*    input_x = i % box_width;*/
    input_x = (i > box_width) ? box_width - 1 : i;
/*    scroll = i - input_x;*/
    scroll = (i > box_width) ? i - box_width + 1: 0;
  }
  redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);

  for (;;) {
    wattrset(dialog, attr);
    wrefresh(dialog);
    key = wgetch(dialog);
    switch (key) {
      case ctrl('q'):
	goto ret;
	break;
      case KEY_F(1):
	display_helpfile();
	break;
      case TAB:
      case KEY_BTAB:
      case KEY_UP:
      case KEY_DOWN:
      case ESC:
      case '\r':
      case '\n':
	for (i = strlen(instr) - 1; i >= scroll + input_x && instr[i] == ' '; i--)
	  instr[i] = '\0';
	if (key == '\r')
	  key = '\n';
	goto ret;
      case '\025':
      case '\030':
      kill_it:
	input_x = scroll = 0;
	/* fall through */
      case '\013':
      case KEY_EOL:
	memset(instr + scroll + input_x, '\0', sizeof(instr) - scroll - input_x);
	redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);
	continue;
      case '\001':
      case KEY_HOME:
	input_x = scroll = 0;
	redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);
	continue;
      case '\005':
      case KEY_END:
	for (i = strlen(instr) - 1; i >= scroll + input_x && instr[i] == ' '; i--)
	  instr[i] = '\0';
	i++;
	input_x = i % box_width;
	scroll = i - input_x;
	redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);
	continue;
      case '\002':
      case KEY_LEFT:
	if (input_x || scroll) {
	  if (!input_x) {
	    int oldscroll = scroll;
	    scroll = scroll < box_width-1 ? 0 : scroll-(box_width-1);
	    input_x = oldscroll - 1 - scroll;
	    redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);
	  } else {
	    input_x--;
	    wmove(dialog, box_y, input_x + box_x);
	  }
	} else
	  beep();
	continue;
      case '\006':
      case KEY_RIGHT:
	  if (   scroll+input_x < MAX_LEN
	      && (flen < 0 || scroll+input_x < flen)
	     ) {
	    if (!instr[scroll+input_x])
	      instr[scroll+input_x] = ' ';
	    if (input_x == box_width-1) {
	      scroll++;
	      redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);
	    }
	    else {
	      wmove(dialog, box_y, input_x + box_x);
	      waddch(dialog, instr[scroll+input_x]);
	      input_x++;
	    }
	  } else
	    beep(); /* Alarm user about overflow */
	continue;
      case '\b':
      case '\177':
      case KEY_BACKSPACE:
      erase_it:
	if (input_x || scroll) {
	  i = strlen(instr);
	  memmove(instr+scroll+input_x-1, instr+scroll+input_x, i-(scroll+input_x)+1);
	  if (!input_x) {
	    int oldscroll = scroll;
	    scroll = scroll < box_width-1 ? 0 : scroll-(box_width-1);
	    input_x = oldscroll - 1 - scroll;
	  } else
	    input_x--;
	  redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);
	} else
	  beep();
	continue;
      case '\004':
      case KEY_DC:
	for (i = strlen(instr) - 1; i >= scroll + input_x && instr[i] == ' '; i--)
	  instr[i] = '\0';
	i++;
	if (i == 0) {
	  beep();
	  continue;
	}
	memmove(instr+scroll+input_x, instr+scroll+input_x+1, i-(scroll+input_x));
	redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);
	continue;
      default:
	if (CCEQ(key, erase_char))
	  goto erase_it;
	if (CCEQ(key, kill_char))
	  goto kill_it;
	if (key < 0x100 && isprint(key)) {
	  for (i = strlen(instr) - 1; i >= scroll + input_x && instr[i] == ' '; i--)
	    instr[i] = '\0';
	  i++;
	  if (i < MAX_LEN && (flen < 0 || scroll+input_x < flen)) {
	    if (flen < 0 || i < flen)
	      memmove(instr+scroll+input_x+1, instr+scroll+input_x, i-(scroll+input_x));
	    instr[scroll+input_x] = key;
	    if (input_x == box_width-1 && (flen < 0 || i < flen))
	      scroll++;
	    else
	      input_x++;
	    redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, FALSE, attr_mask);
	  } else
	    beep(); /* Alarm user about overflow */
	  continue;
	}
      }
    }
ret:
    redraw_field(dialog, box_y, box_x, flen, box_width, instr, input_x, scroll, attr, old_attr, TRUE, attr_mask);
    wrefresh(dialog);
    strcpy(result, instr);
    return key;
}

static void
redraw_field(WINDOW *dialog, int box_y, int box_x, int flen, int box_width, unsigned char instr[], int input_x, int scroll, chtype attr, chtype old_attr, int fexit, int attr_mask)
{
  int i, fix_len;

  wattrset(dialog, fexit ? old_attr : attr);
  wmove(dialog, box_y, box_x);
  fix_len = flen >= 0 ? MIN(flen-scroll,box_width) : box_width;
  for (i = 0; i < fix_len; i++)
      waddch(dialog, instr[scroll+i] ? ((attr_mask & DITEM_NO_ECHO) ? '*' : instr[scroll+i]) : ' ');
  wattrset(dialog, old_attr);
  for ( ; i < box_width; i++)
      waddch(dialog, instr[scroll+i] ? ((attr_mask & DITEM_NO_ECHO) ? '*' : instr[scroll+i]) : ' ');
  wmove(dialog, box_y, input_x + box_x);
}
