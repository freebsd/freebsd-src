/*
 *  checklist.c -- implements the checklist box
 *
 *  AUTHOR: Savio Lam (lam836@cs.cuhk.hk)
 *
 *	Substantial rennovation:  12/18/95, Jordan K. Hubbard
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


static void print_item(WINDOW *win, unsigned char *tag, unsigned char *item, int status, int choice, int selected,
		       dialogMenuItem *me);

#define DREF(di, item)		((di) ? &((di)[(item)]) : NULL)

static int list_width, check_x, item_x;


/*
 * Display a dialog box with a list of options that can be turned on or off
 */
int
dialog_checklist(unsigned char *title, unsigned char *prompt, int height, int width,
		 int list_height, int cnt, void *it, unsigned char *result)
{
    int i, j, x, y, cur_x, cur_y, box_x, box_y, key = 0, button = 0, choice = 0,
	l, k, scroll = 0, max_choice, item_no = 0, *status;
    int redraw_menu = FALSE;
    char okButton, cancelButton;
    WINDOW *dialog, *list;
    unsigned char **items = NULL;
    dialogMenuItem *ditems;
    
    /* Allocate space for storing item on/off status */
    if ((status = alloca(sizeof(int) * abs(cnt))) == NULL) {
	endwin();
	fprintf(stderr, "\nCan't allocate memory in dialog_checklist().\n");
	exit(-1);
    }
    
draw:
    /* Previous calling syntax, e.g. just a list of strings? */
    if (cnt >= 0) {
	items = it;
	ditems = NULL;
	item_no = cnt;
	/* Initializes status */
	for (i = 0; i < item_no; i++)
	    status[i] = !strcasecmp(items[i*3 + 2], "on");
    }
    /* It's the new specification format - fake the rest of the code out */
    else {
	item_no = abs(cnt);
	ditems = it;
	if (!items)
	    items = (unsigned char **)alloca((item_no * 3) * sizeof(unsigned char *));
	
	/* Initializes status */
	for (i = 0; i < item_no; i++) {
	    status[i] = ditems[i].checked ? ditems[i].checked(&ditems[i]) : FALSE;
	    items[i*3] = ditems[i].prompt;
	    items[i*3 + 1] = ditems[i].title;
	    items[i*3 + 2] = status[i] ? "on" : "off";
	}
    }
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
	fprintf(stderr, "\nnewwin(%d,%d,%d,%d) failed, maybe wrong dims\n", height,width, y, x);
	return -1;
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
    print_autowrap(dialog, prompt, height - 1, width - 2, width, 1, 2, TRUE, FALSE);
    
    list_width = width - 6;
    getyx(dialog, cur_y, cur_x);
    box_y = cur_y + 1;
    box_x = (width - list_width) / 2 - 1;
    
    /* create new window for the list */
    list = subwin(dialog, list_height, list_width, y + box_y + 1, x + box_x + 1);
    if (list == NULL) {
	delwin(dialog);
	endwin();
	fprintf(stderr, "\nsubwin(dialog,%d,%d,%d,%d) failed, maybe wrong dims\n", list_height, list_width,
		y + box_y + 1, x + box_x + 1);
	return -1;
    }
    keypad(list, TRUE);
    
    /* draw a box around the list items */
    draw_box(dialog, box_y, box_x, list_height + 2, list_width + 2, menubox_border_attr, menubox_attr);
    
    check_x = (list_width - check_x) / 2;
    item_x = check_x + item_x + 6;
    
    /* Print the list */
    for (i = 0; i < max_choice; i++)
	print_item(list, items[i*3], items[i*3 + 1], status[i], i, i == choice, DREF(ditems, i));
    wnoutrefresh(list);
    print_arrows(dialog, scroll, list_height, item_no, box_x, box_y, check_x + 4, cur_x, cur_y);
    
    display_helpline(dialog, height-1, width);
    
    x = width/2-11;
    y = height-2;
    /* Is this a fancy new style argument string where we get to override
     * the buttons, or an old style one where they're fixed?
     */
    if (ditems && result) {
	cancelButton = toupper(ditems[CANCEL_BUTTON].prompt[0]);
	print_button(dialog, ditems[CANCEL_BUTTON].prompt, y, x + strlen(ditems[OK_BUTTON].prompt) + 5,
		     ditems[CANCEL_BUTTON].checked ? ditems[CANCEL_BUTTON].checked(&ditems[CANCEL_BUTTON]) : FALSE);
	okButton = toupper(ditems[OK_BUTTON].prompt[0]);
	print_button(dialog, ditems[OK_BUTTON].prompt, y, x,
		     ditems[OK_BUTTON].checked ? ditems[OK_BUTTON].checked(&ditems[OK_BUTTON]) : TRUE);
    }
    else {
	cancelButton = 'C';
	print_button(dialog, "Cancel", y, x + 14, FALSE);
	okButton = 'O';
	print_button(dialog, "  OK  ", y, x, TRUE);
    }
    wrefresh(dialog);
    
    while (key != ESC) {
	key = wgetch(dialog);
	
	/* Shortcut to OK? */
	if (toupper(key) == okButton) {
	    if (ditems && result && ditems[OK_BUTTON].fire) {
		int st;
		WINDOW *save;

		save = dupwin(newscr);
		st = ditems[OK_BUTTON].fire(&ditems[OK_BUTTON]);
		if (st & DITEM_RESTORE) {
		    touchwin(save);
		    wrefresh(save);
		}
		delwin(save);
	    }
	    else {
		*result = '\0';
		for (i = 0; i < item_no; i++) {
		    if (status[i]) {
			strcat(result, items[i*3]);
			strcat(result, "\n");
		    }
		}
	    }
	    delwin(list);
	    delwin(dialog);
	    return 0;
	}
	/* Shortcut to cancel? */
	else if (toupper(key) == cancelButton) {
	    if (ditems && result && ditems[CANCEL_BUTTON].fire) {
		int st;
		WINDOW *save;

		save = dupwin(newscr);
		st = ditems[CANCEL_BUTTON].fire(&ditems[CANCEL_BUTTON]);
		if (st & DITEM_RESTORE) {
		    touchwin(save);
		    wrefresh(save);
		    wmove(dialog, cur_y, cur_x);
		}
		delwin(save);
	    }
	    delwin(list);
	    delwin(dialog);
	    return 1;
	}
	
	/* Check if key pressed matches first character of any item tag in list */
	for (i = 0; i < max_choice; i++)
	    if (key < 0x100 && toupper(key) == toupper(items[(scroll+i)*3][0]))
		break;
	
	if (i < max_choice || (key >= '1' && key <= MIN('9', '0'+max_choice)) || key == KEY_UP ||
	    key == KEY_DOWN || key == ' ' || key == '+' || key == '-' ) {

	    if (key >= '1' && key <= MIN('9', '0'+max_choice))
		i = key - '1';
	    
	    else if (key == KEY_UP || key == '-') {
		if (!choice) {
		    if (scroll) {
			/* Scroll list down */
			getyx(dialog, cur_y, cur_x);    /* Save cursor position */
			if (list_height > 1) {
			    /* De-highlight current first item before scrolling down */
			    print_item(list, items[scroll*3], items[scroll*3 + 1], status[scroll], 0, FALSE, DREF(ditems, scroll));
			    scrollok(list, TRUE);
			    wscrl(list, -1);
			    scrollok(list, FALSE);
			}
			scroll--;
			print_item(list, items[scroll*3], items[scroll*3 + 1], status[scroll], 0, TRUE, DREF(ditems, scroll));
			wnoutrefresh(list);
			print_arrows(dialog, scroll, list_height, item_no, box_x, box_y, check_x + 4, cur_x, cur_y);
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
			/* Scroll list up */
			getyx(dialog, cur_y, cur_x);    /* Save cursor position */
			if (list_height > 1) {
			    /* De-highlight current last item before scrolling up */
			    print_item(list, items[(scroll+max_choice-1)*3], items[(scroll+max_choice-1)*3 + 1],
				       status[scroll+max_choice-1], max_choice-1, FALSE, DREF(ditems, scroll + max_choice - 1));
			    scrollok(list, TRUE);
			    scroll(list);
			    scrollok(list, FALSE);
			}
			scroll++;
			print_item(list, items[(scroll+max_choice-1)*3], items[(scroll+max_choice-1)*3 + 1],
				   status[scroll+max_choice-1], max_choice-1, TRUE, DREF(ditems, scroll + max_choice - 1));
			wnoutrefresh(list);
			print_arrows(dialog, scroll, list_height, item_no, box_x, box_y, check_x + 4, cur_x, cur_y);
			wrefresh(dialog);
		    }
		    continue;    /* wait for another key press */
		}
		else
		    i = choice + 1;
	    }
	    else if (key == ' ') {    /* Toggle item status */
		char lbra = 0, rbra = 0, mark = 0;
		
		if (ditems) {
		    if (ditems[scroll + choice].fire) {
			int st;
			WINDOW *save;

			save = dupwin(newscr);
			st = ditems[scroll + choice].fire(&ditems[scroll + choice]);
			if (st & DITEM_RESTORE) {
			    touchwin(save);
			    wrefresh(save);
			}
			if (st & DITEM_REDRAW) {
			    wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */
			    for (i = 0; i < item_no; i++)
				status[i] = ditems[i].checked ? ditems[i].checked(&ditems[i]) : FALSE;
			    for (i = 0; i < max_choice; i++) {
				print_item(list, items[(scroll + i) * 3], items[(scroll + i) * 3 + 1],
					   status[scroll + i], i, i == choice, DREF(ditems, scroll + i));
			    }
			    wnoutrefresh(list);
			    print_arrows(dialog, scroll, list_height, item_no, box_x, box_y, check_x + 4,
					 cur_x, cur_y);
			    wrefresh(dialog);
			}
			else if (st & DITEM_RECREATE) {
			    delwin(save);
			    delwin(list);
			    delwin(dialog);
			    goto draw;
			}
			delwin(save);
			if (st & DITEM_LEAVE_MENU) {
			    /* Allow a fire action to take us out of the menu */
			    key = ESC;
			    break;
			}
		    }
		    status[scroll + choice] = ditems[scroll + choice].checked ?
			ditems[scroll + choice].checked(&ditems[scroll + choice]) : FALSE;
		    lbra = ditems[scroll + choice].lbra;
		    rbra = ditems[scroll + choice].rbra;
		    mark = ditems[scroll + choice].mark;
		}
		else
		    status[scroll + choice] = !status[scroll + choice];
		getyx(dialog, cur_y, cur_x);    /* Save cursor position */
		wmove(list, choice, check_x);
		wattrset(list, check_selected_attr);
		if (!lbra)
		    lbra = '[';
		if (!rbra)
		    rbra = ']';
		if (!mark)
		    mark = 'X';
		wprintw(list, "%c%c%c", lbra, status[scroll+choice] ? mark : ' ', rbra);
		wnoutrefresh(list);
		wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */
		wrefresh(dialog);
		continue;    /* wait for another key press */
	    }
	    
	    if (i != choice) {
		/* De-highlight current item */
		getyx(dialog, cur_y, cur_x);    /* Save cursor position */
		print_item(list, items[(scroll+choice)*3], items[(scroll+choice)*3 + 1], status[scroll+choice], choice, FALSE,
			   DREF(ditems, scroll + choice));
		
		/* Highlight new item */
		choice = i;
		print_item(list, items[(scroll+choice)*3], items[(scroll+choice)*3 + 1], status[scroll+choice], choice, TRUE,
			   DREF(ditems, scroll + choice));
		wnoutrefresh(list);
		wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */
		wrefresh(dialog);
	    }
	    continue;    /* wait for another key press */
	}
	
	switch (key) {
	case KEY_PPAGE:	/* can we go up? */
	    if (scroll > height - 4)
		scroll -= (height-4);
	    else
		scroll = 0;
	    redraw_menu = TRUE;
	    break;
	    
	case KEY_NPAGE:      /* can we go down a full page? */
	    if (scroll + list_height >= item_no-1 - list_height) {
		scroll = item_no - list_height;
		if (scroll < 0)
		    scroll = 0;
	    }
	    else
		scroll += list_height;
	    redraw_menu = TRUE;
	    break;
	    
	case KEY_HOME:      /* go to the top */
	    scroll = 0;
	    choice = 0;
	    redraw_menu = TRUE;
	    break;
	    
	case KEY_END:      /* Go to the bottom */
	    scroll = item_no - list_height;
	    if (scroll < 0)
		scroll = 0;
	    choice = max_choice - 1;
	    redraw_menu = TRUE;
	    break;
	    
	    /* swap the selection of OK/Cancel buttons */
	case TAB:
	case KEY_BTAB:
	case KEY_LEFT:
	case KEY_RIGHT:
	    button = !button;
	    if (ditems && result) {
		if (button) {
		    print_button(dialog, ditems[OK_BUTTON].prompt, y, x,
				 ditems[OK_BUTTON].checked ? ditems[OK_BUTTON].checked(&ditems[OK_BUTTON]) : !button);
		    print_button(dialog, ditems[CANCEL_BUTTON].prompt, y, x + strlen(ditems[OK_BUTTON].prompt) + 5,
				 ditems[CANCEL_BUTTON].checked ? ditems[CANCEL_BUTTON].checked(&ditems[CANCEL_BUTTON]) : button);
		}
		else {
		    print_button(dialog, ditems[CANCEL_BUTTON].prompt, y, x + strlen(ditems[OK_BUTTON].prompt) + 5,
				 ditems[CANCEL_BUTTON].checked ? ditems[CANCEL_BUTTON].checked(&ditems[CANCEL_BUTTON]) : button);
		    print_button(dialog, ditems[OK_BUTTON].prompt, y, x,
				 ditems[OK_BUTTON].checked ? ditems[OK_BUTTON].checked(&ditems[OK_BUTTON]) : !button);
		}
	    }
	    else {
		if (button) {
		    print_button(dialog, "  OK  ", y, x, !button);
		    print_button(dialog, "Cancel", y, x + 14, button);
		}
		else {
		    print_button(dialog, "Cancel", y, x + 14, button);
		    print_button(dialog, "  OK  ", y, x, !button);
		}
	    }
	    wrefresh(dialog);
	    break;
	    
	    /* Select either the OK or Cancel button */
	case '\n':
	case '\r':
	    if (!button && result) {
		if (ditems && ditems[button ? CANCEL_BUTTON : OK_BUTTON].fire) {
		    int st;
		    WINDOW *save = dupwin(newscr);

		    st = ditems[button ? CANCEL_BUTTON : OK_BUTTON].fire(&ditems[button ? CANCEL_BUTTON : OK_BUTTON]);
		    if (st & DITEM_RESTORE) {
			touchwin(save);
			wrefresh(save);
		    }
		    delwin(save);
		    if (st == DITEM_FAILURE)
			continue;
		}
		else {
		    *result = '\0';
		    for (i = 0; i < item_no; i++) {
			if (status[i]) {
			    strcat(result, items[i*3]);
			    strcat(result, "\n");
			}
		    }
		}
	    }
	    delwin(list);
	    delwin(dialog);
	    return button;
	    break;
	    
	    /* Let me outta here! */
	case ESC:
	    break;
	    
	    /* Help! */
	case KEY_F(1):
	case '?':
	    display_helpfile();
	break;
	}
	
	if (redraw_menu) {
	    for (i = 0; i < max_choice; i++)
		print_item(list, items[(scroll+i)*3], items[(scroll+i)*3 + 1], status[scroll+i], i, i == choice,
			   DREF(ditems, scroll + i));
	    wnoutrefresh(list);
	    print_arrows(dialog, scroll, list_height, item_no, box_x, box_y, check_x + 4, cur_x, cur_y);
	    wrefresh(dialog);
	    redraw_menu = FALSE;
	}
    }
    delwin(list);
    delwin(dialog);
    return -1;    /* ESC pressed */
}
/* End of dialog_checklist() */


/*
 * Print list item
 */
static void
print_item(WINDOW *win, unsigned char *tag, unsigned char *item, int status, int choice, int selected,
	   dialogMenuItem *me)
{
    int i;
    
    /* Clear 'residue' of last item */
    wattrset(win, menubox_attr);
    wmove(win, choice, 0);
    for (i = 0; i < list_width; i++)
	waddch(win, ' ');
    wmove(win, choice, check_x);
    wattrset(win, selected ? check_selected_attr : check_attr);
    wprintw(win, "%c%c%c",  me && me->lbra ? me->lbra : '[',
	    status ? me && me->mark ? me->mark : 'X' : ' ',
	    me && me->rbra ? me->rbra : ']');
    wattrset(win, menubox_attr);
    waddch(win, ' ');
    wattrset(win, selected ? tag_key_selected_attr : tag_key_attr);
    waddch(win, tag[0]);
    wattrset(win, selected ? tag_selected_attr : tag_attr);
    waddstr(win, tag + 1);
    wmove(win, choice, item_x);
    wattrset(win, selected ? item_selected_attr : item_attr);
    waddstr(win, item);
    /* If have a selection handler for this, call it */
    if (me && me->selected) {
	wrefresh(win);
	me->selected(me, selected);
    }
}
/* End of print_item() */
