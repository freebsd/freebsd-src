/*
 *  checklist.c -- implements the checklist box
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


static void print_item(WINDOW *win, unsigned char *tag, unsigned char *item, int status, int choice, int selected);


static int list_width, check_x, item_x;


/*
 * Display a dialog box with a list of options that can be turned on or off
 */
int dialog_checklist(unsigned char *title, unsigned char *prompt, int height, int width, int list_height, int item_no, unsigned char **items, unsigned char *result)
{
  int i, j, x, y, cur_x, cur_y, box_x, box_y, key = 0, button = 0, choice = 0,
      l, k, scroll = 0, max_choice, *status;
  WINDOW *dialog, *list;

  /* Allocate space for storing item on/off status */
  if ((status = malloc(sizeof(int)*item_no)) == NULL) {
    endwin();
    fprintf(stderr, "\nCan't allocate memory in dialog_checklist().\n");
    exit(-1);
  }
  /* Initializes status */
  for (i = 0; i < item_no; i++)
    status[i] = !strcasecmp(items[i*3 + 2], "on");

  max_choice = MIN(list_height, item_no);

  check_x = 0;
  item_x = 0;
  /* Find length of longest item in order to center checklist */
  for (i = 0; i < item_no; i++) {
    l = strlen(items[i*3]);
    for (j = 0; j < item_no; j++) {
      k = strlen(items[j*3 + 1]);
      check_x = MAX(check_x, l + k + 6);
    }
    item_x = MAX(item_x, l);
  }
  if (height < 0)
	height = strheight(prompt)+list_height+4+2;
  if (width < 0) {
	i = strwidth(prompt);
	j = ((title != NULL) ? strwidth(title) : 0);
	width = MAX(i,j);
	width = MAX(width,check_x+4)+4;
  }
  width = MAX(width,24);

  if (width > COLS)
	width = COLS;
  if (height > LINES)
	height = LINES;
  /* center dialog box on screen */
  x = (COLS - width)/2;
  y = (LINES - height)/2;
  
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

  list_width = width-6;
  getyx(dialog, cur_y, cur_x);
  box_y = cur_y + 1;
  box_x = (width - list_width)/2 - 1;

  /* create new window for the list */
  list = subwin(dialog, list_height, list_width, y + box_y + 1, x + box_x + 1);
  if (list == NULL) {
    endwin();
    fprintf(stderr, "\nsubwin(dialog,%d,%d,%d,%d) failed, maybe wrong dims\n", list_height,list_width,y+box_y+1,x+box_x+1);
    exit(1);
  }
  keypad(list, TRUE);

  /* draw a box around the list items */
  draw_box(dialog, box_y, box_x, list_height+2, list_width+2, menubox_border_attr, menubox_attr);

  check_x = (list_width - check_x) / 2;
  item_x = check_x + item_x + 6;

  /* Print the list */
  for (i = 0; i < max_choice; i++)
    print_item(list, items[i*3], items[i*3 + 1], status[i], i, i == choice);
  wnoutrefresh(list);

  if (list_height < item_no) {
    wattrset(dialog, darrow_attr);
    wmove(dialog, box_y + list_height + 1, box_x + check_x + 5);
    waddch(dialog, ACS_DARROW);
    wmove(dialog, box_y + list_height + 1, box_x + check_x + 6);
    waddstr(dialog, "(+)");
  }

  display_helpline(dialog, height-1, width);

  x = width/2-11;
  y = height-2;
  print_button(dialog, "Cancel", y, x+14, FALSE);
  print_button(dialog, "  OK  ", y, x, TRUE);
  wrefresh(dialog);

  while (key != ESC) {
    key = wgetch(dialog);
    /* Check if key pressed matches first character of any item tag in list */
    for (i = 0; i < max_choice; i++)
      if (key < 0x100 && toupper(key) == toupper(items[(scroll+i)*3][0]))
        break;

    if (i < max_choice || (key >= '1' && key <= MIN('9', '0'+max_choice)) || 
        key == KEY_UP || key == KEY_DOWN || key == ' ' ||
        key == '+' || key == '-' ) {
      if (key >= '1' && key <= MIN('9', '0'+max_choice))
        i = key - '1';
      else if (key == KEY_UP || key == '-') {
        if (!choice) {
          if (scroll) {
#ifdef BROKEN_WSCRL
    /* wscrl() in ncurses 1.8.1 seems to be broken, causing a segmentation
       violation when scrolling windows of height = 4, so scrolling is not
       used for now */
            scroll--;
            getyx(dialog, cur_y, cur_x);    /* Save cursor position */
            /* Reprint list to scroll down */
            for (i = 0; i < max_choice; i++)
              print_item(list, items[(scroll+i)*3], items[(scroll+i)*3 + 1], status[scroll+i], i, i == choice);

#else

            /* Scroll list down */
            getyx(dialog, cur_y, cur_x);    /* Save cursor position */
            if (list_height > 1) {
              /* De-highlight current first item before scrolling down */
              print_item(list, items[scroll*3], items[scroll*3 + 1], status[scroll], 0, FALSE);
              scrollok(list, TRUE);
              wscrl(list, -1);
              scrollok(list, FALSE);
            }
            scroll--;
            print_item(list, items[scroll*3], items[scroll*3 + 1], status[scroll], 0, TRUE);
#endif
            wnoutrefresh(list);

            /* print the up/down arrows */
            wmove(dialog, box_y, box_x + check_x + 5);
            wattrset(dialog, scroll ? uarrow_attr : menubox_attr);
            waddch(dialog, scroll ? ACS_UARROW : ACS_HLINE);
            wmove(dialog, box_y, box_x + check_x + 6);
            waddch(dialog, scroll ? '(' : ACS_HLINE);
            wmove(dialog, box_y, box_x + check_x + 7);
            waddch(dialog, scroll ? '-' : ACS_HLINE);
            wmove(dialog, box_y, box_x + check_x + 8);
            waddch(dialog, scroll ? ')' : ACS_HLINE);
            wattrset(dialog, darrow_attr);
            wmove(dialog, box_y + list_height + 1, box_x + check_x + 5);
            waddch(dialog, ACS_DARROW);
            wmove(dialog, box_y + list_height + 1, box_x + check_x + 6);
            waddch(dialog, '(');
            wmove(dialog, box_y + list_height + 1, box_x + check_x + 7);
            waddch(dialog, '+');
            wmove(dialog, box_y + list_height + 1, box_x + check_x + 8);
            waddch(dialog, ')');
            wmove(dialog, cur_y, cur_x);  /* Restore cursor position */
            wrefresh(dialog);
          }
          continue;    /* wait for another key press */
        }
        else
          i = choice - 1;
      }
      else if (key == KEY_DOWN || key == '+') {
        if (choice == max_choice - 1) {
          if (scroll+choice < item_no-1) {
#ifdef BROKEN_WSCRL
    /* wscrl() in ncurses 1.8.1 seems to be broken, causing a segmentation
       violation when scrolling windows of height = 4, so scrolling is not
       used for now */
            scroll++;
            getyx(dialog, cur_y, cur_x);    /* Save cursor position */
            /* Reprint list to scroll up */
            for (i = 0; i < max_choice; i++)
              print_item(list, items[(scroll+i)*3], items[(scroll+i)*3 + 1], status[scroll+i], i, i == choice);

#else

            /* Scroll list up */
            getyx(dialog, cur_y, cur_x);    /* Save cursor position */
            if (list_height > 1) {
              /* De-highlight current last item before scrolling up */
              print_item(list, items[(scroll+max_choice-1)*3], items[(scroll+max_choice-1)*3 + 1], status[scroll+max_choice-1], max_choice-1, FALSE);
              scrollok(list, TRUE);
              scroll(list);
              scrollok(list, FALSE);
            }
            scroll++;
            print_item(list, items[(scroll+max_choice-1)*3], items[(scroll+max_choice-1)*3 + 1], status[scroll+max_choice-1], max_choice-1, TRUE);
#endif
            wnoutrefresh(list);

            /* print the up/down arrows */
            wattrset(dialog, uarrow_attr);
            wmove(dialog, box_y, box_x + check_x + 5);
            waddch(dialog, ACS_UARROW);
            wmove(dialog, box_y, box_x + check_x + 6);
            waddstr(dialog, "(-)");
            wmove(dialog, box_y + list_height + 1, box_x + check_x + 5);
            wattrset(dialog, scroll+choice < item_no-1 ? darrow_attr : menubox_border_attr);
            waddch(dialog, scroll+choice < item_no-1 ? ACS_DARROW : ACS_HLINE);
            wmove(dialog, box_y + list_height + 1, box_x + check_x + 6);
            waddch(dialog, scroll+choice < item_no-1 ? '(' : ACS_HLINE);
            wmove(dialog, box_y + list_height + 1, box_x + check_x + 7);
            waddch(dialog, scroll+choice < item_no-1 ? '+' : ACS_HLINE);
            wmove(dialog, box_y + list_height + 1, box_x + check_x + 8);
            waddch(dialog, scroll+choice < item_no-1 ? ')' : ACS_HLINE);
            wmove(dialog, cur_y, cur_x);  /* Restore cursor position */
            wrefresh(dialog);
          }
          continue;    /* wait for another key press */
        }
        else
          i = choice + 1;
      }
      else if (key == ' ') {    /* Toggle item status */
        status[scroll+choice] = !status[scroll+choice];
        getyx(dialog, cur_y, cur_x);    /* Save cursor position */
        wmove(list, choice, check_x);
        wattrset(list, check_selected_attr);
        wprintw(list, "[%c]", status[scroll+choice] ? 'X' : ' ');
        wnoutrefresh(list);
        wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */
        wrefresh(dialog);
        continue;    /* wait for another key press */
      }

      if (i != choice) {
        /* De-highlight current item */
        getyx(dialog, cur_y, cur_x);    /* Save cursor position */
        print_item(list, items[(scroll+choice)*3], items[(scroll+choice)*3 + 1], status[scroll+choice], choice, FALSE);

        /* Highlight new item */
        choice = i;
        print_item(list, items[(scroll+choice)*3], items[(scroll+choice)*3 + 1], status[scroll+choice], choice, TRUE);
        wnoutrefresh(list);
        wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */
        wrefresh(dialog);
      }
      continue;    /* wait for another key press */
    }

    switch (key) {
      case 'O':
      case 'o':
        delwin(dialog);
	*result = '\0';
        for (i = 0; i < item_no; i++)
	  if (status[i]) {
	    strcat(result, items[i*3]);
	    strcat(result, "\n");
	  }
        free(status);
        return 0;
      case 'C':
      case 'c':
        delwin(dialog);
        free(status);
        return 1;
      case TAB:
      case KEY_BTAB:
      case KEY_LEFT:
      case KEY_RIGHT:
        if (!button) {
          button = 1;    /* Indicates "Cancel" button is selected */
          print_button(dialog, "  OK  ", y, x, FALSE);
          print_button(dialog, "Cancel", y, x+14, TRUE);
        }
        else {
          button = 0;    /* Indicates "OK" button is selected */
          print_button(dialog, "Cancel", y, x+14, FALSE);
          print_button(dialog, "  OK  ", y, x, TRUE);
        }
        wrefresh(dialog);
        break;
      case ' ':
      case '\n':
      case '\r':
        delwin(dialog);
	if (!button) {
	  *result = '\0';
          for (i = 0; i < item_no; i++)
	    if (status[i]) {
	      strcat(result, items[i*3]);
	      strcat(result, "\n");
	    }
	}
        free(status);
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
  free(status);
  return -1;    /* ESC pressed */
}
/* End of dialog_checklist() */


/*
 * Print list item
 */
static void print_item(WINDOW *win, unsigned char *tag, unsigned char *item, int status, int choice, int selected)
{
  int i;

  /* Clear 'residue' of last item */
  wattrset(win, menubox_attr);
  wmove(win, choice, 0);
  for (i = 0; i < list_width; i++)
    waddch(win, ' ');
  wmove(win, choice, check_x);
  wattrset(win, selected ? check_selected_attr : check_attr);
  wprintw(win, "[%c]", status ? 'X' : ' ');
  wattrset(win, menubox_attr);
  waddch(win, ' ');
  wattrset(win, selected ? tag_key_selected_attr : tag_key_attr);
  waddch(win, tag[0]);
  wattrset(win, selected ? tag_selected_attr : tag_attr);
  waddstr(win, tag + 1);
  wmove(win, choice, item_x);
  wattrset(win, selected ? item_selected_attr : item_attr);
  waddstr(win, item);
}
/* End of print_item() */
