/*
 *  $Id: checklist.c,v 1.124 2011/01/19 00:27:53 tom Exp $
 *
 *  checklist.c -- implements the checklist box
 *
 *  Copyright 2000-2010,2011	Thomas E. Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License, version 2.1
 *  as published by the Free Software Foundation.
 *
 *  This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this program; if not, write to
 *	Free Software Foundation, Inc.
 *	51 Franklin St., Fifth Floor
 *	Boston, MA 02110, USA.
 *
 *  An earlier version of this program lists as authors:
 *	Savio Lam (lam836@cs.cuhk.hk)
 *	Stuart Herbert - S.Herbert@sheffield.ac.uk: radiolist extension
 *	Alessandro Rubini - rubini@ipvvis.unipv.it: merged the two
 */

#include <dialog.h>
#include <dlg_keys.h>

static int list_width, check_x, item_x, checkflag;

#define MIN_HIGH  (1 + (5 * MARGIN))

#define LLEN(n) ((n) * CHECKBOX_TAGS)
#define ItemData(i)    &items[LLEN(i)]
#define ItemName(i)    items[LLEN(i)]
#define ItemText(i)    items[LLEN(i) + 1]
#define ItemStatus(i)  items[LLEN(i) + 2]
#define ItemHelp(i)    items[LLEN(i) + 3]

static void
print_arrows(WINDOW *win,
	     int box_x,
	     int box_y,
	     int scrollamt,
	     int choice,
	     int item_no,
	     int list_height)
{
    dlg_draw_scrollbar(win,
		       (long) (scrollamt),
		       (long) (scrollamt),
		       (long) (scrollamt + choice),
		       (long) (item_no),
		       box_x + check_x,
		       box_x + list_width,
		       box_y,
		       box_y + list_height + 1,
		       menubox_attr,
		       menubox_border_attr);
}

/*
 * Print list item.  The 'selected' parameter is true if 'choice' is the
 * current item.  That one is colored differently from the other items.
 */
static void
print_item(WINDOW *win,
	   DIALOG_LISTITEM * item,
	   const char *states,
	   int choice,
	   int selected)
{
    chtype save = dlg_get_attrs(win);
    int i;
    chtype attr = A_NORMAL;
    const int *cols;
    const int *indx;
    int limit;

    /* Clear 'residue' of last item */
    wattrset(win, menubox_attr);
    (void) wmove(win, choice, 0);
    for (i = 0; i < list_width; i++)
	(void) waddch(win, ' ');

    (void) wmove(win, choice, check_x);
    wattrset(win, selected ? check_selected_attr : check_attr);
    (void) wprintw(win,
		   (checkflag == FLAG_CHECK) ? "[%c]" : "(%c)",
		   states[item->state]);
    wattrset(win, menubox_attr);
    (void) waddch(win, ' ');

    if (strlen(item->name) != 0) {

	indx = dlg_index_wchars(item->name);

	wattrset(win, selected ? tag_key_selected_attr : tag_key_attr);
	(void) waddnstr(win, item->name, indx[1]);

	if ((int) strlen(item->name) > indx[1]) {
	    limit = dlg_limit_columns(item->name, (item_x - check_x - 6), 1);
	    if (limit > 1) {
		wattrset(win, selected ? tag_selected_attr : tag_attr);
		(void) waddnstr(win,
				item->name + indx[1],
				indx[limit] - indx[1]);
	    }
	}
    }

    if (strlen(item->text) != 0) {
	cols = dlg_index_columns(item->text);
	limit = dlg_limit_columns(item->text, (getmaxx(win) - item_x + 1), 0);

	if (limit > 0) {
	    (void) wmove(win, choice, item_x);
	    wattrset(win, selected ? item_selected_attr : item_attr);
	    dlg_print_text(win, item->text, cols[limit], &attr);
	}
    }

    if (selected) {
	dlg_item_help(item->help);
    }
    wattrset(win, save);
}

/*
 * This is an alternate interface to 'checklist' which allows the application
 * to read the list item states back directly without putting them in the
 * output buffer.  It also provides for more than two states over which the
 * check/radio box can display.
 */
int
dlg_checklist(const char *title,
	      const char *cprompt,
	      int height,
	      int width,
	      int list_height,
	      int item_no,
	      DIALOG_LISTITEM * items,
	      const char *states,
	      int flag,
	      int *current_item)
{
    /* *INDENT-OFF* */
    static DLG_KEYS_BINDING binding[] = {
	ENTERKEY_BINDINGS,
	DLG_KEYS_DATA( DLGK_FIELD_NEXT, KEY_RIGHT ),
	DLG_KEYS_DATA( DLGK_FIELD_NEXT, TAB ),
	DLG_KEYS_DATA( DLGK_FIELD_PREV, KEY_BTAB ),
	DLG_KEYS_DATA( DLGK_FIELD_PREV, KEY_LEFT ),
	DLG_KEYS_DATA( DLGK_ITEM_FIRST, KEY_HOME ),
	DLG_KEYS_DATA( DLGK_ITEM_LAST,	KEY_END ),
	DLG_KEYS_DATA( DLGK_ITEM_LAST,	KEY_LL ),
	DLG_KEYS_DATA( DLGK_ITEM_NEXT,	'+' ),
	DLG_KEYS_DATA( DLGK_ITEM_NEXT,	KEY_DOWN ),
	DLG_KEYS_DATA( DLGK_ITEM_NEXT,  CHR_NEXT ),
	DLG_KEYS_DATA( DLGK_ITEM_PREV,	'-' ),
	DLG_KEYS_DATA( DLGK_ITEM_PREV,	KEY_UP ),
	DLG_KEYS_DATA( DLGK_ITEM_PREV,  CHR_PREVIOUS ),
	DLG_KEYS_DATA( DLGK_PAGE_NEXT,	KEY_NPAGE ),
	DLG_KEYS_DATA( DLGK_PAGE_NEXT,	DLGK_MOUSE(KEY_NPAGE) ),
	DLG_KEYS_DATA( DLGK_PAGE_PREV,	KEY_PPAGE ),
	DLG_KEYS_DATA( DLGK_PAGE_PREV,	DLGK_MOUSE(KEY_PPAGE) ),
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

#ifdef KEY_RESIZE
    int old_height = height;
    int old_width = width;
#endif
    int i, j, key2, found, x, y, cur_x, cur_y, box_x, box_y;
    int key = 0, fkey;
    int button = dialog_state.visit_items ? -1 : dlg_defaultno_button();
    int choice = dlg_default_listitem(items);
    int scrollamt = 0;
    int max_choice;
    int was_mouse;
    int use_height;
    int use_width, name_width, text_width;
    int result = DLG_EXIT_UNKNOWN;
    int num_states;
    WINDOW *dialog, *list;
    char *prompt = dlg_strclone(cprompt);
    const char **buttons = dlg_ok_labels();

    dlg_does_output();
    dlg_tab_correct_str(prompt);

#ifdef KEY_RESIZE
  retry:
#endif

    use_height = list_height;
    if (use_height == 0) {
	use_width = dlg_calc_list_width(item_no, items) + 10;
	/* calculate height without items (4) */
	dlg_auto_size(title, prompt, &height, &width, MIN_HIGH, MAX(26, use_width));
	dlg_calc_listh(&height, &use_height, item_no);
    } else {
	dlg_auto_size(title, prompt, &height, &width, MIN_HIGH + use_height, 26);
    }
    dlg_button_layout(buttons, &width);
    dlg_print_size(height, width);
    dlg_ctl_size(height, width);

    /* we need at least two states */
    if (states == 0 || strlen(states) < 2)
	states = " *";
    num_states = (int) strlen(states);

    checkflag = flag;

    x = dlg_box_x_ordinate(width);
    y = dlg_box_y_ordinate(height);

    dialog = dlg_new_window(height, width, y, x);
    dlg_register_window(dialog, "checklist", binding);
    dlg_register_buttons(dialog, "checklist", buttons);

    dlg_mouse_setbase(x, y);

    dlg_draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    dlg_draw_bottom_box(dialog);
    dlg_draw_title(dialog, title);

    wattrset(dialog, dialog_attr);
    dlg_print_autowrap(dialog, prompt, height, width);

    list_width = width - 6;
    getyx(dialog, cur_y, cur_x);
    box_y = cur_y + 1;
    box_x = (width - list_width) / 2 - 1;

    /*
     * After displaying the prompt, we know how much space we really have.
     * Limit the list to avoid overwriting the ok-button.
     */
    if (use_height + MIN_HIGH > height - cur_y)
	use_height = height - MIN_HIGH - cur_y;
    if (use_height <= 0)
	use_height = 1;

    max_choice = MIN(use_height, item_no);

    /* create new window for the list */
    list = dlg_sub_window(dialog, use_height, list_width,
			  y + box_y + 1, x + box_x + 1);

    /* draw a box around the list items */
    dlg_draw_box(dialog, box_y, box_x,
		 use_height + 2 * MARGIN,
		 list_width + 2 * MARGIN,
		 menubox_border_attr, menubox_attr);

    text_width = 0;
    name_width = 0;
    /* Find length of longest item to center checklist */
    for (i = 0; i < item_no; i++) {
	text_width = MAX(text_width, dlg_count_columns(items[i].text));
	name_width = MAX(name_width, dlg_count_columns(items[i].name));
    }

    /* If the name+text is wider than the list is allowed, then truncate
     * one or both of them.  If the name is no wider than 1/4 of the list,
     * leave it intact.
     */
    use_width = (list_width - 6);
    if (text_width + name_width > use_width) {
	int need = (int) (0.25 * use_width);
	if (name_width > need) {
	    int want = (int) (use_width * ((double) name_width) /
			      (text_width + name_width));
	    name_width = (want > need) ? want : need;
	}
	text_width = use_width - name_width;
    }

    check_x = (use_width - (text_width + name_width)) / 2;
    item_x = name_width + check_x + 6;

    /* ensure we are scrolled to show the current choice */
    if (choice >= (max_choice + scrollamt)) {
	scrollamt = choice - max_choice + 1;
	choice = max_choice - 1;
    }
    /* Print the list */
    for (i = 0; i < max_choice; i++)
	print_item(list,
		   &items[i + scrollamt],
		   states,
		   i, i == choice);
    (void) wnoutrefresh(list);

    /* register the new window, along with its borders */
    dlg_mouse_mkbigregion(box_y + 1, box_x, use_height, list_width + 2,
			  KEY_MAX, 1, 1, 1 /* by lines */ );

    print_arrows(dialog,
		 box_x, box_y,
		 scrollamt, max_choice, item_no, use_height);

    dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);

    while (result == DLG_EXIT_UNKNOWN) {
	if (button < 0)		/* --visit-items */
	    wmove(dialog, box_y + choice + 1, box_x + check_x + 2);

	key = dlg_mouse_wgetch(dialog, &fkey);
	if (dlg_result_key(key, fkey, &result))
	    break;

	was_mouse = (fkey && is_DLGK_MOUSE(key));
	if (was_mouse)
	    key -= M_EVENT;

	if (was_mouse && (key >= KEY_MAX)) {
	    getyx(dialog, cur_y, cur_x);
	    i = (key - KEY_MAX);
	    if (i < max_choice) {
		/* De-highlight current item */
		print_item(list,
			   &items[scrollamt + choice],
			   states,
			   choice, FALSE);
		/* Highlight new item */
		choice = (key - KEY_MAX);
		print_item(list,
			   &items[scrollamt + choice],
			   states,
			   choice, TRUE);
		(void) wnoutrefresh(list);
		(void) wmove(dialog, cur_y, cur_x);

		key = ' ';	/* force the selected item to toggle */
	    } else {
		beep();
		continue;
	    }
	    fkey = FALSE;
	} else if (was_mouse && key >= KEY_MIN) {
	    key = dlg_lookup_key(dialog, key, &fkey);
	}

	/*
	 * A space toggles the item status.  We handle either a checklist
	 * (any number of items can be selected) or radio list (zero or one
	 * items can be selected).
	 */
	if (key == ' ') {
	    int current = scrollamt + choice;
	    int next = items[current].state + 1;

	    if (next >= num_states)
		next = 0;

	    getyx(dialog, cur_y, cur_x);
	    if (flag == FLAG_CHECK) {	/* checklist? */
		items[current].state = next;
		print_item(list,
			   &items[scrollamt + choice],
			   states,
			   choice, TRUE);
	    } else {		/* radiolist */
		for (i = 0; i < item_no; i++) {
		    if (i != current) {
			items[i].state = 0;
		    }
		}
		if (items[current].state) {
		    items[current].state = next ? next : 1;
		    print_item(list,
			       &items[current],
			       states,
			       choice, TRUE);
		} else {
		    items[current].state = 1;
		    for (i = 0; i < max_choice; i++)
			print_item(list,
				   &items[scrollamt + i],
				   states,
				   i, i == choice);
		}
	    }
	    (void) wnoutrefresh(list);
	    (void) wmove(dialog, cur_y, cur_x);
	    wrefresh(dialog);
	    continue;		/* wait for another key press */
	}

	/*
	 * Check if key pressed matches first character of any item tag in
	 * list.  If there is more than one match, we will cycle through
	 * each one as the same key is pressed repeatedly.
	 */
	found = FALSE;
	if (!fkey) {
	    if (button < 0 || !dialog_state.visit_items) {
		for (j = scrollamt + choice + 1; j < item_no; j++) {
		    if (dlg_match_char(dlg_last_getc(), items[j].name)) {
			found = TRUE;
			i = j - scrollamt;
			break;
		    }
		}
		if (!found) {
		    for (j = 0; j <= scrollamt + choice; j++) {
			if (dlg_match_char(dlg_last_getc(), items[j].name)) {
			    found = TRUE;
			    i = j - scrollamt;
			    break;
			}
		    }
		}
		if (found)
		    dlg_flush_getc();
	    } else if ((j = dlg_char_to_button(key, buttons)) >= 0) {
		button = j;
		ungetch('\n');
		continue;
	    }
	}

	/*
	 * A single digit (1-9) positions the selection to that line in the
	 * current screen.
	 */
	if (!found
	    && (key <= '9')
	    && (key > '0')
	    && (key - '1' < max_choice)) {
	    found = TRUE;
	    i = key - '1';
	}

	if (!found) {
	    if (fkey) {
		found = TRUE;
		switch (key) {
		case DLGK_ITEM_FIRST:
		    i = -scrollamt;
		    break;
		case DLGK_ITEM_LAST:
		    i = item_no - 1 - scrollamt;
		    break;
		case DLGK_PAGE_PREV:
		    if (choice)
			i = 0;
		    else if (scrollamt != 0)
			i = -MIN(scrollamt, max_choice);
		    else
			continue;
		    break;
		case DLGK_PAGE_NEXT:
		    i = MIN(choice + max_choice, item_no - scrollamt - 1);
		    break;
		case DLGK_ITEM_PREV:
		    i = choice - 1;
		    if (choice == 0 && scrollamt == 0)
			continue;
		    break;
		case DLGK_ITEM_NEXT:
		    i = choice + 1;
		    if (scrollamt + choice >= item_no - 1)
			continue;
		    break;
		default:
		    found = FALSE;
		    break;
		}
	    }
	}

	if (found) {
	    if (i != choice) {
		getyx(dialog, cur_y, cur_x);
		if (i < 0 || i >= max_choice) {
#if defined(NCURSES_VERSION_MAJOR) && NCURSES_VERSION_MAJOR < 5
		    /*
		     * Using wscrl to assist ncurses scrolling is not needed
		     * in version 5.x
		     */
		    if (i == -1) {
			if (use_height > 1) {
			    /* De-highlight current first item */
			    print_item(list,
				       &items[scrollamt],
				       states,
				       0, FALSE);
			    scrollok(list, TRUE);
			    wscrl(list, -1);
			    scrollok(list, FALSE);
			}
			scrollamt--;
			print_item(list,
				   &items[scrollamt],
				   states,
				   0, TRUE);
		    } else if (i == max_choice) {
			if (use_height > 1) {
			    /* De-highlight current last item before scrolling up */
			    print_item(list,
				       &items[scrollamt + max_choice - 1],
				       states,
				       max_choice - 1, FALSE);
			    scrollok(list, TRUE);
			    wscrl(list, 1);
			    scrollok(list, FALSE);
			}
			scrollamt++;
			print_item(list,
				   &items[scrollamt + max_choice - 1],
				   states,
				   max_choice - 1, TRUE);
		    } else
#endif
		    {
			if (i < 0) {
			    scrollamt += i;
			    choice = 0;
			} else {
			    choice = max_choice - 1;
			    scrollamt += (i - max_choice + 1);
			}
			for (i = 0; i < max_choice; i++) {
			    print_item(list,
				       &items[scrollamt + i],
				       states,
				       i, i == choice);
			}
		    }
		    (void) wnoutrefresh(list);
		    print_arrows(dialog,
				 box_x, box_y,
				 scrollamt, max_choice, item_no, use_height);
		} else {
		    /* De-highlight current item */
		    print_item(list,
			       &items[scrollamt + choice],
			       states,
			       choice, FALSE);
		    /* Highlight new item */
		    choice = i;
		    print_item(list,
			       &items[scrollamt + choice],
			       states,
			       choice, TRUE);
		    (void) wnoutrefresh(list);
		    print_arrows(dialog,
				 box_x, box_y,
				 scrollamt, max_choice, item_no, use_height);
		    (void) wmove(dialog, cur_y, cur_x);
		    wrefresh(dialog);
		}
	    }
	    continue;		/* wait for another key press */
	}

	if (fkey) {
	    switch (key) {
	    case DLGK_ENTER:
		result = dlg_ok_buttoncode(button);
		break;
	    case DLGK_FIELD_PREV:
		button = dlg_prev_button(buttons, button);
		dlg_draw_buttons(dialog, height - 2, 0, buttons, button,
				 FALSE, width);
		break;
	    case DLGK_FIELD_NEXT:
		button = dlg_next_button(buttons, button);
		dlg_draw_buttons(dialog, height - 2, 0, buttons, button,
				 FALSE, width);
		break;
#ifdef KEY_RESIZE
	    case KEY_RESIZE:
		/* reset data */
		height = old_height;
		width = old_width;
		/* repaint */
		dlg_clear();
		dlg_del_window(dialog);
		refresh();
		dlg_mouse_free_regions();
		goto retry;
#endif
	    default:
		if (was_mouse) {
		    if ((key2 = dlg_ok_buttoncode(key)) >= 0) {
			result = key2;
			break;
		    }
		    beep();
		}
	    }
	} else {
	    beep();
	}
    }

    dlg_del_window(dialog);
    dlg_mouse_free_regions();
    free(prompt);
    *current_item = (scrollamt + choice);
    return result;
}

/*
 * Display a dialog box with a list of options that can be turned on or off
 * The `flag' parameter is used to select between radiolist and checklist.
 */
int
dialog_checklist(const char *title,
		 const char *cprompt,
		 int height,
		 int width,
		 int list_height,
		 int item_no,
		 char **items,
		 int flag)
{
    int result;
    int i;
    DIALOG_LISTITEM *listitems;
    bool separate_output = ((flag == FLAG_CHECK)
			    && (dialog_vars.separate_output));
    bool show_status = FALSE;
    int current = 0;

    listitems = dlg_calloc(DIALOG_LISTITEM, (size_t) item_no + 1);
    assert_ptr(listitems, "dialog_checklist");

    for (i = 0; i < item_no; ++i) {
	listitems[i].name = ItemName(i);
	listitems[i].text = ItemText(i);
	listitems[i].help = ((dialog_vars.item_help)
			     ? ItemHelp(i)
			     : dlg_strempty());
	listitems[i].state = !dlg_strcmp(ItemStatus(i), "on");
    }
    dlg_align_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);

    result = dlg_checklist(title,
			   cprompt,
			   height,
			   width,
			   list_height,
			   item_no,
			   listitems,
			   NULL,
			   flag,
			   &current);

    switch (result) {
    case DLG_EXIT_OK:		/* FALLTHRU */
    case DLG_EXIT_EXTRA:
	show_status = TRUE;
	break;
    case DLG_EXIT_HELP:
	dlg_add_result("HELP ");
	show_status = dialog_vars.help_status;
	if (USE_ITEM_HELP(listitems[current].help)) {
	    if (show_status) {
		if (separate_output) {
		    dlg_add_string(listitems[current].help);
		    dlg_add_separator();
		} else {
		    dlg_add_quoted(listitems[current].help);
		}
	    } else {
		dlg_add_string(listitems[current].help);
	    }
	    result = DLG_EXIT_ITEM_HELP;
	} else {
	    if (show_status) {
		if (separate_output) {
		    dlg_add_string(listitems[current].name);
		    dlg_add_separator();
		} else {
		    dlg_add_quoted(listitems[current].name);
		}
	    } else {
		dlg_add_string(listitems[current].name);
	    }
	}
	break;
    }

    if (show_status) {
	for (i = 0; i < item_no; i++) {
	    if (listitems[i].state) {
		if (separate_output) {
		    dlg_add_string(listitems[i].name);
		    dlg_add_separator();
		} else {
		    if (dlg_need_separator())
			dlg_add_separator();
		    dlg_add_string(listitems[i].name);
		}
	    }
	}
    }

    dlg_free_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);
    free(listitems);
    return result;
}
