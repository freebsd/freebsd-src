/*
 *  $Id: menubox.c,v 1.118 2010/01/17 22:24:11 tom Exp $
 *
 *  menubox.c -- implements the menu box
 *
 *  Copyright 2000-2009,2010	Thomas E. Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public Licens, version 2.1e
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
 *  An earlier version of this program lists as authors
 *	Savio Lam (lam836@cs.cuhk.hk)
 */

#include <dialog.h>
#include <dlg_keys.h>

static int menu_width, tag_x, item_x;

typedef enum {
    Unselected = 0,
    Selected,
    Editing
} Mode;

#define MIN_HIGH  (1 + (5 * MARGIN))

#define INPUT_ROWS     3	/* rows per inputmenu entry */

#define LLEN(n) ((n) * MENUBOX_TAGS)
#define ItemName(i)    items[LLEN(i)]
#define ItemText(i)    items[LLEN(i) + 1]
#define ItemHelp(i)    items[LLEN(i) + 2]

#define RowHeight(i) (is_inputmenu ? ((i) * INPUT_ROWS) : ((i) * 1))
#define ItemToRow(i) (is_inputmenu ? ((i) * INPUT_ROWS + 1) : (i))
#define RowToItem(i) (is_inputmenu ? ((i) / INPUT_ROWS + 0) : (i))

static void
print_arrows(WINDOW *win,
	     int box_x,
	     int box_y,
	     int scrollamt,
	     int max_choice,
	     int item_no,
	     int menu_height)
{
    dlg_draw_scrollbar(win,
		       scrollamt,
		       scrollamt,
		       scrollamt + max_choice,
		       item_no,
		       box_x,
		       box_x + menu_width,
		       box_y,
		       box_y + menu_height + 1,
		       menubox_attr,
		       menubox_border_attr);
}

/*
 * Print the tag of a menu-item
 */
static void
print_tag(WINDOW *win,
	  DIALOG_LISTITEM * item,
	  int choice,
	  Mode selected,
	  bool is_inputmenu)
{
    int my_x = item_x;
    int my_y = ItemToRow(choice);
    int tag_width = (my_x - tag_x - GUTTER);
    const int *cols;
    const int *indx;
    int limit;
    int prefix;

    cols = dlg_index_columns(item->name);
    indx = dlg_index_wchars(item->name);
    limit = dlg_count_wchars(item->name);
    prefix = (indx[1] - indx[0]);

    /* highlight first char of the tag to be special */
    (void) wmove(win, my_y, tag_x);
    wattrset(win, selected ? tag_key_selected_attr : tag_key_attr);
    if (strlen(item->name) != 0)
	(void) waddnstr(win, item->name, prefix);
    /* print rest of the string */
    wattrset(win, selected ? tag_selected_attr : tag_attr);
    if ((int) strlen(item->name) > prefix) {
	limit = dlg_limit_columns(item->name, tag_width, 1);
	if (limit > 0)
	    (void) waddnstr(win, item->name + indx[1], indx[limit] - indx[1]);
    }
}

/*
 * Print menu item
 */
static void
print_item(WINDOW *win,
	   DIALOG_LISTITEM * items,
	   int choice,
	   Mode selected,
	   bool is_inputmenu)
{
    chtype save = getattrs(win);
    int n;
    int my_width = menu_width;
    int my_x = item_x;
    int my_y = ItemToRow(choice);
    chtype attr = A_NORMAL;
    chtype textchar;
    chtype bordchar;

    switch (selected) {
    default:
    case Unselected:
	textchar = item_attr;
	bordchar = item_attr;
	break;
    case Selected:
	textchar = item_selected_attr;
	bordchar = item_selected_attr;
	break;
    case Editing:
	textchar = inputbox_attr;
	bordchar = dialog_attr;
	break;
    }

    /* Clear 'residue' of last item and mark current current item */
    if (is_inputmenu) {
	wattrset(win, (selected != Unselected) ? item_selected_attr : item_attr);
	for (n = my_y - 1; n < my_y + INPUT_ROWS - 1; n++) {
	    wmove(win, n, 0);
	    wprintw(win, "%*s", my_width, " ");
	}
    } else {
	wattrset(win, menubox_attr);
	wmove(win, my_y, 0);
	wprintw(win, "%*s", my_width, " ");
    }

    print_tag(win, items, choice, selected, is_inputmenu);

    /* Draw the input field box (only for inputmenu) */
    (void) wmove(win, my_y, my_x);
    if (is_inputmenu) {
	my_width -= 1;
	dlg_draw_box(win, my_y - 1, my_x, INPUT_ROWS, my_width - my_x - tag_x,
		     bordchar,
		     bordchar);
	my_width -= 1;
	++my_x;
    }

    /* print actual item */
    wmove(win, my_y, my_x);
    wattrset(win, textchar);
    dlg_print_text(win, items->text, my_width - my_x, &attr);

    if (selected) {
	dlg_item_help(items->help);
    }
    wattrset(win, save);
}

/*
 * Allow the user to edit the text of a menu entry.
 */
static int
input_menu_edit(WINDOW *win,
		DIALOG_LISTITEM * items,
		int choice,
		char **resultp)
{
    chtype save = getattrs(win);
    char *result;
    int offset = 0;
    int key = 0, fkey = 0;
    int first = TRUE;
    /* see above */
    bool is_inputmenu = TRUE;
    int y = ItemToRow(choice);
    int code = TRUE;
    int max_len = dlg_max_input(MAX((int) strlen(items->text) + 1, MAX_LEN));

    result = dlg_malloc(char, (size_t) max_len);
    assert_ptr(result, "input_menu_edit");

    /* original item is used to initialize the input string. */
    result[0] = '\0';
    strcpy(result, items->text);

    print_item(win, items, choice, Editing, TRUE);

    /* taken out of inputbox.c - but somewhat modified */
    for (;;) {
	if (!first)
	    key = dlg_mouse_wgetch(win, &fkey);
	if (dlg_edit_string(result, &offset, key, fkey, first)) {
	    dlg_show_string(win, result, offset, inputbox_attr,
			    y, item_x + 1, menu_width - item_x - 3,
			    FALSE, first);
	    first = FALSE;
	} else if (key == ESC || key == TAB) {
	    code = FALSE;
	    break;
	} else {
	    break;
	}
    }
    print_item(win, items, choice, Selected, TRUE);
    wattrset(win, save);

    *resultp = result;
    return code;
}

static int
handle_button(int code, DIALOG_LISTITEM * items, int choice)
{
    switch (code) {
    case DLG_EXIT_OK:		/* FALLTHRU */
    case DLG_EXIT_EXTRA:
	dlg_add_string(items[choice].name);
	break;
    case DLG_EXIT_HELP:
	dlg_add_result("HELP ");
	if (USE_ITEM_HELP(items[choice].help)) {
	    dlg_add_string(items[choice].help);
	    code = DLG_EXIT_ITEM_HELP;
	} else {
	    dlg_add_string(items[choice].name);
	}
	break;
    }
    return code;
}

static int
dlg_renamed_menutext(DIALOG_LISTITEM * items, int current, char *newtext)
{
    if (dialog_vars.input_result)
	dialog_vars.input_result[0] = '\0';
    dlg_add_result("RENAMED ");
    dlg_add_string(items[current].name);
    dlg_add_result(" ");
    dlg_add_string(newtext);
    return DLG_EXIT_EXTRA;
}

static int
dlg_dummy_menutext(DIALOG_LISTITEM * items, int current, char *newtext)
{
    (void) items;
    (void) current;
    (void) newtext;
    return DLG_EXIT_ERROR;
}

/*
 * This is an alternate interface to 'menu' which allows the application
 * to read the list item states back directly without putting them in the
 * output buffer.
 */
int
dlg_menu(const char *title,
	 const char *cprompt,
	 int height,
	 int width,
	 int menu_height,
	 int item_no,
	 DIALOG_LISTITEM * items,
	 int *current_item,
	 DIALOG_INPUTMENU rename_menutext)
{
    /* *INDENT-OFF* */
    static DLG_KEYS_BINDING binding[] = {
	ENTERKEY_BINDINGS,
	DLG_KEYS_DATA( DLGK_FIELD_NEXT,	' ' ),
	DLG_KEYS_DATA( DLGK_FIELD_NEXT,	KEY_RIGHT ),
	DLG_KEYS_DATA( DLGK_FIELD_NEXT,	TAB ),
	DLG_KEYS_DATA( DLGK_FIELD_PREV,	KEY_BTAB ),
	DLG_KEYS_DATA( DLGK_FIELD_PREV,	KEY_LEFT ),
	DLG_KEYS_DATA( DLGK_ITEM_NEXT,	'+' ),
	DLG_KEYS_DATA( DLGK_ITEM_NEXT,	KEY_DOWN ),
	DLG_KEYS_DATA( DLGK_ITEM_NEXT,  CHR_NEXT ),
	DLG_KEYS_DATA( DLGK_ITEM_PREV,	'-' ),
	DLG_KEYS_DATA( DLGK_ITEM_PREV,	KEY_UP ),
	DLG_KEYS_DATA( DLGK_ITEM_PREV,  CHR_PREVIOUS ),
	DLG_KEYS_DATA( DLGK_PAGE_FIRST,	KEY_HOME ),
	DLG_KEYS_DATA( DLGK_PAGE_LAST,	KEY_END ),
	DLG_KEYS_DATA( DLGK_PAGE_LAST,	KEY_LL ),
	DLG_KEYS_DATA( DLGK_PAGE_NEXT,	KEY_NPAGE ),
	DLG_KEYS_DATA( DLGK_PAGE_PREV,	KEY_PPAGE ),
	END_KEYS_BINDING
    };
    static DLG_KEYS_BINDING binding2[] = {
	INPUTSTR_BINDINGS,
	ENTERKEY_BINDINGS,
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

#ifdef KEY_RESIZE
    int old_height = height;
    int old_width = width;
#endif
    int i, j, x, y, cur_x, cur_y, box_x, box_y;
    int key = 0, fkey;
    int button = dialog_state.visit_items ? -1 : dlg_defaultno_button();
    int choice = dlg_default_listitem(items);
    int result = DLG_EXIT_UNKNOWN;
    int scrollamt = 0;
    int max_choice, min_width;
    int found;
    int use_height, use_width, name_width, text_width;
    WINDOW *dialog, *menu;
    char *prompt = dlg_strclone(cprompt);
    const char **buttons = dlg_ok_labels();
    bool is_inputmenu = (rename_menutext == dlg_renamed_menutext);

    dlg_does_output();
    dlg_tab_correct_str(prompt);

#ifdef KEY_RESIZE
  retry:
#endif

    use_height = menu_height;
    if (use_height == 0) {
	min_width = dlg_calc_list_width(item_no, items) + 10;
	/* calculate height without items (4) */
	dlg_auto_size(title, prompt, &height, &width, MIN_HIGH, MAX(26, min_width));
	dlg_calc_listh(&height, &use_height, item_no);
    } else {
	dlg_auto_size(title, prompt, &height, &width, MIN_HIGH + use_height, 26);
    }
    dlg_button_layout(buttons, &width);
    dlg_print_size(height, width);
    dlg_ctl_size(height, width);

    x = dlg_box_x_ordinate(width);
    y = dlg_box_y_ordinate(height);

    dialog = dlg_new_window(height, width, y, x);
    dlg_register_window(dialog, "menubox", binding);
    dlg_register_buttons(dialog, "menubox", buttons);

    dlg_mouse_setbase(x, y);

    dlg_draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    dlg_draw_bottom_box(dialog);
    dlg_draw_title(dialog, title);

    wattrset(dialog, dialog_attr);
    dlg_print_autowrap(dialog, prompt, height, width);

    menu_width = width - 6;
    getyx(dialog, cur_y, cur_x);
    box_y = cur_y + 1;
    box_x = (width - menu_width) / 2 - 1;

    /*
     * After displaying the prompt, we know how much space we really have.
     * Limit the list to avoid overwriting the ok-button.
     */
    if (use_height + MIN_HIGH > height - cur_y)
	use_height = height - MIN_HIGH - cur_y;
    if (use_height <= 0)
	use_height = 1;

    /* Find out maximal number of displayable items at once. */
    max_choice = MIN(use_height,
		     RowHeight(item_no));
    if (is_inputmenu)
	max_choice /= INPUT_ROWS;

    /* create new window for the menu */
    menu = dlg_sub_window(dialog, use_height, menu_width,
			  y + box_y + 1,
			  x + box_x + 1);
    dlg_register_window(menu, "menu", binding2);
    dlg_register_buttons(menu, "menu", buttons);

    /* draw a box around the menu items */
    dlg_draw_box(dialog, box_y, box_x, use_height + 2, menu_width + 2,
		 menubox_border_attr, menubox_attr);

    name_width = 0;
    text_width = 0;

    /* Find length of longest item to center menu  *
     * only if --menu was given, using --inputmenu *
     * won't be centered.                         */
    for (i = 0; i < item_no; i++) {
	name_width = MAX(name_width, dlg_count_columns(items[i].name));
	text_width = MAX(text_width, dlg_count_columns(items[i].text));
    }

    /* If the name+text is wider than the list is allowed, then truncate
     * one or both of them.  If the name is no wider than 30% of the list,
     * leave it intact.
     *
     * FIXME: the gutter width and name/list ratio should be configurable.
     */
    use_width = (menu_width - GUTTER);
    if (text_width + name_width > use_width) {
	int need = (int) (0.30 * use_width);
	if (name_width > need) {
	    int want = (int) (use_width
			      * ((double) name_width)
			      / (text_width + name_width));
	    name_width = (want > need) ? want : need;
	}
	text_width = use_width - name_width;
    }

    tag_x = (is_inputmenu
	     ? 0
	     : (use_width - text_width - name_width) / 2);
    item_x = name_width + tag_x + GUTTER;

    if (choice - scrollamt >= max_choice) {
	scrollamt = choice - (max_choice - 1);
	choice = max_choice - 1;
    }

    /* Print the menu */
    for (i = 0; i < max_choice; i++) {
	print_item(menu,
		   &items[i + scrollamt],
		   i,
		   (i == choice) ? Selected : Unselected,
		   is_inputmenu);
    }
    (void) wnoutrefresh(menu);

    /* register the new window, along with its borders */
    dlg_mouse_mkbigregion(box_y + 1, box_x, use_height + 2, menu_width + 2,
			  KEY_MAX, 1, 1, 1 /* by lines */ );

    print_arrows(dialog, box_x, box_y, scrollamt, max_choice, item_no, use_height);

    dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);

    while (result == DLG_EXIT_UNKNOWN) {
	if (button < 0)		/* --visit-items */
	    wmove(dialog, box_y + ItemToRow(choice) + 1, box_x + tag_x + 1);

	key = dlg_mouse_wgetch(dialog, &fkey);
	if (dlg_result_key(key, fkey, &result))
	    break;

	found = FALSE;
	if (fkey) {
	    /*
	     * Allow a mouse-click on a box to switch selection to that box.
	     * Handling a button click is a little more complicated, since we
	     * push a KEY_ENTER back onto the input stream so we'll put the
	     * cursor at the right place before handling the "keypress".
	     */
	    if (key >= DLGK_MOUSE(KEY_MAX)) {
		key -= DLGK_MOUSE(KEY_MAX);
		i = RowToItem(key);
		if (i < max_choice) {
		    found = TRUE;
		} else {
		    beep();
		    continue;
		}
	    } else if (is_DLGK_MOUSE(key)
		       && dlg_ok_buttoncode(key - M_EVENT) >= 0) {
		button = (key - M_EVENT);
		ungetch('\n');
		continue;
	    }
	} else {
	    /*
	     * Check if key pressed matches first character of any item tag in
	     * list.  If there is more than one match, we will cycle through
	     * each one as the same key is pressed repeatedly.
	     */
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
	}

	if (!found && fkey) {
	    found = TRUE;
	    switch (key) {
	    case DLGK_PAGE_FIRST:
		i = -scrollamt;
		break;
	    case DLGK_PAGE_LAST:
		i = item_no - 1 - scrollamt;
		break;
	    case DLGK_MOUSE(KEY_PPAGE):
	    case DLGK_PAGE_PREV:
		if (choice)
		    i = 0;
		else if (scrollamt != 0)
		    i = -MIN(scrollamt, max_choice);
		else
		    continue;
		break;
	    case DLGK_MOUSE(KEY_NPAGE):
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
			    print_item(menu,
				       &items[scrollamt],
				       0, Unselected, is_inputmenu);
			    scrollok(menu, TRUE);
			    wscrl(menu, -RowHeight(1));
			    scrollok(menu, FALSE);
			}
			scrollamt--;
			print_item(menu,
				   &items[scrollamt],
				   0, Selected, is_inputmenu);
		    } else if (i == max_choice) {
			if (use_height > 1) {
			    /* De-highlight current last item before scrolling up */
			    print_item(menu,
				       &items[scrollamt + max_choice - 1],
				       max_choice - 1,
				       Unselected,
				       is_inputmenu);
			    scrollok(menu, TRUE);
			    wscrl(menu, RowHeight(1));
			    scrollok(menu, FALSE);
			}
			scrollamt++;
			print_item(menu,
				   &items[scrollamt + max_choice - 1],
				   max_choice - 1, TRUE,
				   is_inputmenu);
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
			    print_item(menu,
				       &items[scrollamt + i],
				       i,
				       (i == choice) ? Selected : Unselected,
				       is_inputmenu);
			}
		    }
		    /* Clean bottom lines */
		    if (is_inputmenu) {
			int spare_lines, x_count;
			spare_lines = use_height % INPUT_ROWS;
			wattrset(menu, menubox_attr);
			for (; spare_lines; spare_lines--) {
			    wmove(menu, use_height - spare_lines, 0);
			    for (x_count = 0; x_count < menu_width;
				 x_count++) {
				waddch(menu, ' ');
			    }
			}
		    }
		    (void) wnoutrefresh(menu);
		    print_arrows(dialog,
				 box_x, box_y,
				 scrollamt, max_choice, item_no, use_height);
		} else {
		    /* De-highlight current item */
		    print_item(menu,
			       &items[scrollamt + choice],
			       choice,
			       Unselected,
			       is_inputmenu);
		    /* Highlight new item */
		    choice = i;
		    print_item(menu,
			       &items[scrollamt + choice],
			       choice,
			       Selected,
			       is_inputmenu);
		    (void) wnoutrefresh(menu);
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
	    case DLGK_ENTER:
		result = dlg_ok_buttoncode(button);

		/*
		 * If dlg_menu() is called from dialog_menu(), we want to
		 * capture the results into dialog_vars.input_result, but not
		 * if dlg_menu() is called directly from an application.  We
		 * can check this by testing if rename_menutext is the function
		 * pointer owned by dialog_menu().  It would be nicer to have
		 * this logic inside dialog_menu(), but that cannot be done
		 * since we would lose compatibility for the results reported
		 * after input_menu_edit().
		 */
		if (result == DLG_EXIT_ERROR) {
		    result = DLG_EXIT_UNKNOWN;
		} else if (is_inputmenu
			   || rename_menutext == dlg_dummy_menutext) {
		    result = handle_button(result,
					   items,
					   scrollamt + choice);
		}

		/*
		 * If we have a rename_menutext function, interpret the Extra
		 * button as a request to rename the menu's text.  If that
		 * function doesn't return "Unknown", we will exit from this
		 * function.  Usually that is done for dialog_menu(), so the
		 * shell script can use the updated value.  If it does return
		 * "Unknown", update the list item only.  A direct caller of
		 * dlg_menu() can free the renamed value - we cannot.
		 */
		if (is_inputmenu && result == DLG_EXIT_EXTRA) {
		    char *tmp;

		    if (input_menu_edit(menu,
					&items[scrollamt + choice],
					choice,
					&tmp)) {
			result = rename_menutext(items, scrollamt + choice, tmp);
			if (result == DLG_EXIT_UNKNOWN) {
			    items[scrollamt + choice].text = tmp;
			} else {
			    free(tmp);
			}
		    } else {
			result = DLG_EXIT_UNKNOWN;
			print_item(menu,
				   &items[scrollamt + choice],
				   choice,
				   Selected,
				   is_inputmenu);
			(void) wnoutrefresh(menu);
			free(tmp);
		    }

		    if (result == DLG_EXIT_UNKNOWN) {
			dlg_draw_buttons(dialog, height - 2, 0,
					 buttons, button, FALSE, width);
		    }
		}
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
		flash();
		break;
	    }
	}
    }

    dlg_mouse_free_regions();
    dlg_unregister_window(menu);
    dlg_del_window(dialog);
    free(prompt);

    *current_item = scrollamt + choice;
    return result;
}

/*
 * Display a menu for choosing among a number of options
 */
int
dialog_menu(const char *title,
	    const char *cprompt,
	    int height,
	    int width,
	    int menu_height,
	    int item_no,
	    char **items)
{
    int result;
    int choice;
    int i;
    DIALOG_LISTITEM *listitems;

    listitems = dlg_calloc(DIALOG_LISTITEM, (size_t) item_no + 1);
    assert_ptr(listitems, "dialog_menu");

    for (i = 0; i < item_no; ++i) {
	listitems[i].name = ItemName(i);
	listitems[i].text = ItemText(i);
	listitems[i].help = ((dialog_vars.item_help)
			     ? ItemHelp(i)
			     : dlg_strempty());
    }
    dlg_align_columns(&listitems[0].text, sizeof(DIALOG_LISTITEM), item_no);

    result = dlg_menu(title,
		      cprompt,
		      height,
		      width,
		      menu_height,
		      item_no,
		      listitems,
		      &choice,
		      dialog_vars.input_menu ? dlg_renamed_menutext : dlg_dummy_menutext);

    dlg_free_columns(&listitems[0].text, sizeof(DIALOG_LISTITEM), item_no);
    free(listitems);
    return result;
}
