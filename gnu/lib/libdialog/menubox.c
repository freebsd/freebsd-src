/*
 *  menubox.c -- implements the menu box
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
#include <ncurses.h>

static void print_item(WINDOW *win, unsigned char *tag, unsigned char *item, int choice, int selected,
		       dialogMenuItem *me);

#define DREF(di, item)		((di) ? &((di)[(item)]) : NULL)

static int menu_width, tag_x, item_x;

/*
 * Display a menu for choosing among a number of options
 */
int
dialog_menu(unsigned char *title, unsigned char *prompt, int height, int width, int menu_height,
	    int cnt, void *it, unsigned char *result, int *ch, int *sc)
{
    int i, j, x, y, cur_x, cur_y, box_x, box_y, key = 0, button = 0, choice = 0,
	l, k, scroll = 0, max_choice, item_no, redraw_menu = FALSE;
    char okButton, cancelButton;
    WINDOW *dialog, *menu;
    unsigned char **items = NULL;
    dialogMenuItem *ditems;
    
draw:
    if (ch)  /* restore menu item info */
	choice = *ch;
    if (sc)
	scroll = *sc;
    
    /* If item_no is a positive integer, use old item specification format */
    if (cnt >= 0) {
	items = it;
	ditems = NULL;
	item_no = cnt;
    }
    /* It's the new specification format - fake the rest of the code out */
    else {
	item_no = abs(cnt);
	ditems = it;
	if (!items)
	    items = (unsigned char **)alloca((item_no * 2) * sizeof(unsigned char *));
	
	/* Initializes status */
	for (i = 0; i < item_no; i++) {
	    items[i*2] = ditems[i].prompt;
	    items[i*2 + 1] = ditems[i].title;
	}
    }
    max_choice = MIN(menu_height, item_no);
    
    tag_x = 0;
    item_x = 0;
    /* Find length of longest item in order to center menu */
    for (i = 0; i < item_no; i++) {
	l = strlen(items[i*2]);
	for (j = 0; j < item_no; j++) {
	    k = strlen(items[j*2 + 1]);
	    tag_x = MAX(tag_x, l + k + 2);
	}
	item_x = MAX(item_x, l);
    }
    if (height < 0)
	height = strheight(prompt)+menu_height+4+2;
    if (width < 0) {
	i = strwidth(prompt);
	j = ((title != NULL) ? strwidth(title) : 0);
	width = MAX(i,j);
	width = MAX(width,tag_x+4)+4;
    }
    width = MAX(width,24);
    
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
    print_autowrap(dialog, prompt, height-1, width-2, width, 1, 2, TRUE, FALSE);
    
    menu_width = width-6;
    getyx(dialog, cur_y, cur_x);
    box_y = cur_y + 1;
    box_x = (width - menu_width)/2 - 1;
    
    /* create new window for the menu */
    menu = subwin(dialog, menu_height, menu_width, y + box_y + 1, x + box_x + 1);
    if (menu == NULL) {
	delwin(dialog);
	endwin();
	fprintf(stderr, "\nsubwin(dialog,%d,%d,%d,%d) failed, maybe wrong dims\n", menu_height,menu_width,y+box_y+1,x+box_x+1);
	return -1;
    }
    keypad(menu, TRUE);
    
    /* draw a box around the menu items */
    draw_box(dialog, box_y, box_x, menu_height+2, menu_width+2, menubox_border_attr, menubox_attr);
    
    tag_x = (menu_width - tag_x) / 2;
    item_x = tag_x + item_x + 2;
    
    /* Print the menu */
    for (i = 0; i < max_choice; i++)
	print_item(menu, items[(scroll+i)*2], items[(scroll+i)*2 + 1], i, i == choice, DREF(ditems, scroll + i));
    wnoutrefresh(menu);
    print_arrows(dialog, scroll, menu_height, item_no, box_x, box_y, tag_x, cur_x, cur_y);
    
    display_helpline(dialog, height-1, width);
    
    x = width/2-11;
    y = height-2;
    
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
		int status;
		WINDOW *save;

		save = dupwin(newscr);
		status = ditems[OK_BUTTON].fire(&ditems[OK_BUTTON]);
		if (status & DITEM_RESTORE) {
		    touchwin(save);
		    wrefresh(save);
		}
		delwin(save);
	    }
	    else
		strcpy(result, items[(scroll + choice) * 2]);
	    delwin(menu);
	    delwin(dialog);
	    return 0;
	}
	/* Shortcut to cancel? */
	else if (toupper(key) == cancelButton) {
	    if (ditems && result && ditems[CANCEL_BUTTON].fire) {
		int status;
		WINDOW *save;

		save = dupwin(newscr);
		status = ditems[CANCEL_BUTTON].fire(&ditems[CANCEL_BUTTON]);
		if (status & DITEM_RESTORE) {
		    touchwin(save);
		    wrefresh(save);
		}
		delwin(save);
	    }
	    delwin(menu);
	    delwin(dialog);
	    return 1;
	}
	
	/* Check if key pressed matches first character of any item tag in menu */
	for (i = 0; i < max_choice; i++)
	    if (key < 0x100 && toupper(key) == toupper(items[(scroll+i)*2][0]))
		break;
	
	if (i < max_choice || (key >= '1' && key <= MIN('9', '0'+max_choice)) ||
	    key == KEY_UP || key == KEY_DOWN || key == '-' || key == '+') {
	    if (key >= '1' && key <= MIN('9', '0'+max_choice))
		i = key - '1';
	    else if (key == KEY_UP || key == '-') {
		if (!choice) {
		    if (scroll) {
			/* Scroll menu down */
			getyx(dialog, cur_y, cur_x);    /* Save cursor position */
			if (menu_height > 1) {
			    /* De-highlight current first item before scrolling down */
			    print_item(menu, items[scroll*2], items[scroll*2 + 1], 0, FALSE, DREF(ditems, scroll));
			    scrollok(menu, TRUE);
			    wscrl(menu, -1);
			    scrollok(menu, FALSE);
			}
			scroll--;
			print_item(menu, items[scroll*2], items[scroll*2 + 1], 0, TRUE, DREF(ditems, scroll));
			wnoutrefresh(menu);
			print_arrows(dialog, scroll, menu_height, item_no, box_x, box_y, tag_x, cur_x, cur_y);
			wrefresh(dialog);
		    }
		    continue;    /* wait for another key press */
		}
		else
		    i = choice - 1;
	    }
	    else if (key == KEY_DOWN || key == '+')
		if (choice == max_choice - 1) {
		    if (scroll+choice < item_no-1) {
			/* Scroll menu up */
			getyx(dialog, cur_y, cur_x);    /* Save cursor position */
			if (menu_height > 1) {
			    /* De-highlight current last item before scrolling up */
			    print_item(menu, items[(scroll + max_choice - 1) * 2], items[(scroll + max_choice - 1) * 2 + 1],
				       max_choice-1, FALSE, DREF(ditems, scroll + max_choice - 1));
			    scrollok(menu, TRUE);
			    scroll(menu);
			    scrollok(menu, FALSE);
			}
			scroll++;
			print_item(menu, items[(scroll + max_choice - 1) * 2], items[(scroll + max_choice - 1) * 2 + 1],
				   max_choice - 1, TRUE, DREF(ditems, scroll + max_choice - 1));
			wnoutrefresh(menu);
			print_arrows(dialog, scroll, menu_height, item_no, box_x, box_y, tag_x, cur_x, cur_y);
			wrefresh(dialog);
		    }
		    continue;    /* wait for another key press */
		}
		else
		    i = choice + 1;
	    
	    if (i != choice) {
		/* De-highlight current item */
		getyx(dialog, cur_y, cur_x);    /* Save cursor position */
		print_item(menu, items[(scroll + choice) * 2], items[(scroll + choice) * 2 + 1], choice, FALSE,
			   DREF(ditems, scroll + choice));
		
		/* Highlight new item */
		choice = i;
		print_item(menu, items[(scroll + choice) * 2], items[(scroll + choice) * 2 + 1], choice, TRUE,
			   DREF(ditems, scroll + choice));
		wnoutrefresh(menu);
		wmove(dialog, cur_y, cur_x);  /* Restore cursor to previous position */
		wrefresh(dialog);
	    }
	    continue;    /* wait for another key press */
	}
	
	/* save info about menu item position */
	if (ch)
	    *ch = choice;
	if (sc)
	    *sc = scroll;
	
	switch (key) {
	case KEY_PPAGE:
	    if (scroll > height-4) {	/* can we go up? */
		scroll -= (height-4);
	    } else {
		scroll = 0;
	    }
	    redraw_menu = TRUE;
	    break;
	    
	case KEY_NPAGE:
	    if (scroll + menu_height >= item_no-1 - menu_height) { /* can we go down a full page? */
		scroll = item_no - menu_height;
		if (scroll < 0) scroll = 0;
	    } else {
		scroll += menu_height;
	    }
	    redraw_menu = TRUE;
	    break;
	    
	case KEY_HOME:
	    scroll = 0;
	    choice = 0;
	    redraw_menu = TRUE;
	    break;
	    
	case KEY_END:
	    scroll = item_no - menu_height;
	    if (scroll < 0) scroll = 0;
	    choice = max_choice - 1;
	    redraw_menu = TRUE;
	    break;
	    
	case KEY_BTAB:
	case TAB:
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
	    
	case ' ':
	case '\r':
	case '\n':
	    if (!button) {
		/* A fire routine can do just about anything to the screen, so be prepared
		   to accept some hints as to what to do in the aftermath. */
		if (ditems && ditems[scroll + choice].fire) {
		    int status;
		    WINDOW *save;

		    save = dupwin(newscr);
		    status = ditems[scroll + choice].fire(&ditems[scroll + choice]);
		    if (status & DITEM_RESTORE) {
			touchwin(save);
			wrefresh(save);
		    }
		    if (status & DITEM_RECREATE) {
			delwin(menu);
			delwin(dialog);
			delwin(save);
			goto draw;
		    }
		    delwin(save);
		    if (status & DITEM_CONTINUE)
			continue;
		}
		else if (result)
		    strcpy(result, items[(scroll+choice)*2]);
	    }
	    delwin(menu);
	    delwin(dialog);
	    return button;
	    
	case ESC:
	    break;
	    
	case KEY_F(1):
	case '?':
	    display_helpfile();
	break;
	}
	
	if (redraw_menu) {
	    for (i = 0; i < max_choice; i++) {
		print_item(menu, items[(scroll + i) * 2], items[(scroll + i) * 2 + 1], i, i == choice,
			   DREF(ditems, scroll + i));
	    }
	    wnoutrefresh(menu);
	    print_arrows(dialog, scroll, menu_height, item_no, box_x, box_y, tag_x, cur_x, cur_y);
	    wrefresh(dialog);
	    redraw_menu = FALSE;
	}
    }
    
    delwin(menu);
    delwin(dialog);
    return -1;    /* ESC pressed */
}
/* End of dialog_menu() */


/*
 * Print menu item
 */
static void
print_item(WINDOW *win, unsigned char *tag, unsigned char *item, int choice, int selected, dialogMenuItem *me)
{
    int i;
    
    /* Clear 'residue' of last item */
    wattrset(win, menubox_attr);
    wmove(win, choice, 0);
    for (i = 0; i < menu_width; i++)
	waddch(win, ' ');
    wmove(win, choice, tag_x);
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
