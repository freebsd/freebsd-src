/*
 *  $Id: treeview.c,v 1.43 2020/11/23 00:38:31 tom Exp $
 *
 *  treeview.c -- implements the treeview dialog
 *
 *  Copyright 2012-2019,2020	Thomas E. Dickey
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
 */

#include <dlg_internals.h>
#include <dlg_keys.h>

#define INDENT 3
#define MIN_HIGH  (1 + (5 * MARGIN))

typedef struct {
    /* the outer-window */
    WINDOW *dialog;
    bool is_check;
    int box_y;
    int box_x;
    int check_x;
    int item_x;
    int use_height;
    int use_width;
    /* the inner-window */
    WINDOW *list;
    DIALOG_LISTITEM *items;
    int item_no;
    int *depths;
    const char *states;
} ALL_DATA;

/*
 * Print list item.  The 'selected' parameter is true if 'choice' is the
 * current item.  That one is colored differently from the other items.
 */
static void
print_item(ALL_DATA * data,
	   DIALOG_LISTITEM * item,
	   const char *states,
	   int depths,
	   int choice,
	   int selected)
{
    WINDOW *win = data->list;
    chtype save = dlg_get_attrs(win);
    int i;
    bool first = TRUE;
    int climit = (getmaxx(win) - data->check_x + 1);
    const char *show = (dialog_vars.no_items
			? item->name
			: item->text);

    /* Clear 'residue' of last item */
    dlg_attrset(win, menubox_attr);
    (void) wmove(win, choice, 0);
    for (i = 0; i < data->use_width; i++)
	(void) waddch(win, ' ');

    (void) wmove(win, choice, data->check_x);
    dlg_attrset(win, selected ? check_selected_attr : check_attr);
    (void) wprintw(win,
		   data->is_check ? "[%c]" : "(%c)",
		   states[item->state]);
    dlg_attrset(win, menubox_attr);

    dlg_attrset(win, selected ? item_selected_attr : item_attr);
    for (i = 0; i < depths; ++i) {
	int j;
	(void) wmove(win, choice, data->item_x + INDENT * i);
	(void) waddch(win, ACS_VLINE);
	for (j = INDENT - 1; j > 0; --j)
	    (void) waddch(win, ' ');
    }
    (void) wmove(win, choice, data->item_x + INDENT * depths);

    dlg_print_listitem(win, show, climit, first, selected);

    if (selected) {
	dlg_item_help(item->help);
    }
    dlg_attrset(win, save);
}

static void
print_list(ALL_DATA * data,
	   int choice,
	   int scrollamt,
	   int max_choice)
{
    int i;
    int cur_y, cur_x;

    getyx(data->dialog, cur_y, cur_x);

    for (i = 0; i < max_choice; i++) {
	print_item(data,
		   &data->items[scrollamt + i],
		   data->states,
		   data->depths[scrollamt + i],
		   i, i == choice);
    }
    (void) wnoutrefresh(data->list);

    dlg_draw_scrollbar(data->dialog,
		       (long) (scrollamt),
		       (long) (scrollamt),
		       (long) (scrollamt + max_choice),
		       (long) (data->item_no),
		       data->box_x + data->check_x,
		       data->box_x + data->use_width,
		       data->box_y,
		       data->box_y + data->use_height + 1,
		       menubox_border2_attr,
		       menubox_border_attr);

    (void) wmove(data->dialog, cur_y, cur_x);
}

static bool
check_hotkey(DIALOG_LISTITEM * items, int choice)
{
    bool result = FALSE;

    if (dlg_match_char(dlg_last_getc(),
		       (dialog_vars.no_tags
			? items[choice].text
			: items[choice].name))) {
	result = TRUE;
    }
    return result;
}

/*
 * This is an alternate interface to 'treeview' which allows the application
 * to read the list item states back directly without putting them in the
 * output buffer.
 */
int
dlg_treeview(const char *title,
	     const char *cprompt,
	     int height,
	     int width,
	     int list_height,
	     int item_no,
	     DIALOG_LISTITEM * items,
	     const char *states,
	     int *depths,
	     int flag,
	     int *current_item)
{
    /* *INDENT-OFF* */
    static DLG_KEYS_BINDING binding[] = {
	HELPKEY_BINDINGS,
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
	TOGGLEKEY_BINDINGS,
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

#ifdef KEY_RESIZE
    int old_height = height;
    int old_width = width;
#endif
    ALL_DATA all;
    int i, j, key2, found, x, y, cur_y, box_x, box_y;
    int key, fkey;
    int button = dialog_state.visit_items ? -1 : dlg_default_button();
    int choice = dlg_default_listitem(items);
    int scrollamt = 0;
    int max_choice;
    int use_height;
    int use_width, name_width, text_width, tree_width;
    int result = DLG_EXIT_UNKNOWN;
    int num_states;
    WINDOW *dialog, *list;
    char *prompt = dlg_strclone(cprompt);
    const char **buttons = dlg_ok_labels();
    const char *widget_name;

    /* we need at least two states */
    if (states == 0 || strlen(states) < 2)
	states = " *";
    num_states = (int) strlen(states);

    dialog_state.plain_buttons = TRUE;

    memset(&all, 0, sizeof(all));
    all.items = items;
    all.item_no = item_no;
    all.states = states;
    all.depths = depths;

    dlg_does_output();
    dlg_tab_correct_str(prompt);

    /*
     * If this is a radiobutton list, ensure that no more than one item is
     * selected initially.  Allow none to be selected, since some users may
     * wish to provide this flavor.
     */
    if (flag == FLAG_RADIO) {
	bool first = TRUE;

	for (i = 0; i < item_no; i++) {
	    if (items[i].state) {
		if (first) {
		    first = FALSE;
		} else {
		    items[i].state = 0;
		}
	    }
	}
    } else {
	all.is_check = TRUE;
    }
    widget_name = "treeview";
#ifdef KEY_RESIZE
  retry:
#endif

    use_height = list_height;
    use_width = dlg_calc_list_width(item_no, items) + 10;
    use_width = MAX(26, use_width);
    if (use_height == 0) {
	/* calculate height without items (4) */
	dlg_auto_size(title, prompt, &height, &width, MIN_HIGH, use_width);
	dlg_calc_listh(&height, &use_height, item_no);
    } else {
	dlg_auto_size(title, prompt, &height, &width, MIN_HIGH + use_height, use_width);
    }
    dlg_button_layout(buttons, &width);
    dlg_print_size(height, width);
    dlg_ctl_size(height, width);

    x = dlg_box_x_ordinate(width);
    y = dlg_box_y_ordinate(height);

    dialog = dlg_new_window(height, width, y, x);
    dlg_register_window(dialog, widget_name, binding);
    dlg_register_buttons(dialog, widget_name, buttons);

    dlg_mouse_setbase(x, y);

    dlg_draw_box2(dialog, 0, 0, height, width, dialog_attr, border_attr, border2_attr);
    dlg_draw_bottom_box2(dialog, border_attr, border2_attr, dialog_attr);
    dlg_draw_title(dialog, title);

    dlg_attrset(dialog, dialog_attr);
    dlg_print_autowrap(dialog, prompt, height, width);

    all.use_width = width - 4;
    cur_y = getcury(dialog);
    box_y = cur_y + 1;
    box_x = (width - all.use_width) / 2 - 1;

    /*
     * After displaying the prompt, we know how much space we really have.
     * Limit the list to avoid overwriting the ok-button.
     */
    use_height = height - MIN_HIGH - cur_y;
    if (use_height <= 0)
	use_height = 1;

    max_choice = MIN(use_height, item_no);

    /* create new window for the list */
    list = dlg_sub_window(dialog, use_height, all.use_width,
			  y + box_y + 1, x + box_x + 1);

    /* draw a box around the list items */
    dlg_draw_box(dialog, box_y, box_x,
		 use_height + 2 * MARGIN,
		 all.use_width + 2 * MARGIN,
		 menubox_border_attr, menubox_border2_attr);

    text_width = 0;
    name_width = 0;
    tree_width = 0;
    /* Find length of longest item to center treeview */
    for (i = 0; i < item_no; i++) {
	tree_width = MAX(tree_width, INDENT * depths[i]);
	text_width = MAX(text_width, dlg_count_columns(items[i].text));
	name_width = MAX(name_width, dlg_count_columns(items[i].name));
    }
    if (dialog_vars.no_tags && !dialog_vars.no_items) {
	tree_width += text_width;
    } else if (dialog_vars.no_items) {
	tree_width += name_width;
    } else {
	tree_width += (text_width + name_width);
    }

    use_width = (all.use_width - 4);
    tree_width = MIN(tree_width, all.use_width);

    all.check_x = (use_width - tree_width) / 2;
    all.item_x = ((dialog_vars.no_tags
		   ? 0
		   : (dialog_vars.no_items
		      ? 0
		      : (2 + name_width)))
		  + all.check_x + 4);

    /* ensure we are scrolled to show the current choice */
    if (choice >= (max_choice + scrollamt)) {
	scrollamt = choice - max_choice + 1;
	choice = max_choice - 1;
    }

    /* register the new window, along with its borders */
    dlg_mouse_mkbigregion(box_y + 1, box_x,
			  use_height, all.use_width + 2,
			  KEY_MAX, 1, 1, 1 /* by lines */ );

    all.dialog = dialog;
    all.box_x = box_x;
    all.box_y = box_y;
    all.use_height = use_height;
    all.list = list;
    print_list(&all, choice, scrollamt, max_choice);

    dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);

    dlg_trace_win(dialog);

    while (result == DLG_EXIT_UNKNOWN) {
	int was_mouse;

	if (button < 0)		/* --visit-items */
	    wmove(dialog, box_y + choice + 1, box_x + all.check_x + 2);

	key = dlg_mouse_wgetch(dialog, &fkey);
	if (dlg_result_key(key, fkey, &result)) {
	    if (!dlg_button_key(result, &button, &key, &fkey))
		break;
	}

	was_mouse = (fkey && is_DLGK_MOUSE(key));
	if (was_mouse)
	    key -= M_EVENT;

	if (was_mouse && (key >= KEY_MAX)) {
	    i = (key - KEY_MAX);
	    if (i < max_choice) {
		choice = (key - KEY_MAX);
		print_list(&all, choice, scrollamt, max_choice);

		key = DLGK_TOGGLE;	/* force the selected item to toggle */
	    } else {
		beep();
		continue;
	    }
	    fkey = FALSE;
	} else if (was_mouse && key >= KEY_MIN) {
	    key = dlg_lookup_key(dialog, key, &fkey);
	}

	/*
	 * A space toggles the item status.
	 */
	if (key == DLGK_TOGGLE) {
	    int current = scrollamt + choice;
	    int next = items[current].state + 1;

	    if (next >= num_states)
		next = 0;

	    if (flag == FLAG_CHECK) {	/* checklist? */
		items[current].state = next;
	    } else {
		for (i = 0; i < item_no; i++) {
		    if (i != current) {
			items[i].state = 0;
		    }
		}
		if (items[current].state) {
		    items[current].state = next ? next : 1;
		} else {
		    items[current].state = 1;
		}
	    }
	    print_list(&all, choice, scrollamt, max_choice);
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
		    if (check_hotkey(items, j)) {
			found = TRUE;
			i = j - scrollamt;
			break;
		    }
		}
		if (!found) {
		    for (j = 0; j <= scrollamt + choice; j++) {
			if (check_hotkey(items, j)) {
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
		if (i < 0 || i >= max_choice) {
		    if (i < 0) {
			scrollamt += i;
			choice = 0;
		    } else {
			choice = max_choice - 1;
			scrollamt += (i - max_choice + 1);
		    }
		    print_list(&all, choice, scrollamt, max_choice);
		} else {
		    choice = i;
		    print_list(&all, choice, scrollamt, max_choice);
		}
	    }
	    continue;		/* wait for another key press */
	}

	if (fkey) {
	    switch (key) {
	    case DLGK_ENTER:
		result = dlg_enter_buttoncode(button);
		break;
	    case DLGK_LEAVE:
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
		dlg_will_resize(dialog);
		/* reset data */
		height = old_height;
		width = old_width;
		/* repaint */
		_dlg_resize_cleanup(dialog);
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
	} else if (key > 0) {
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
 * Display a set of items as a tree.
 */
int
dialog_treeview(const char *title,
		const char *cprompt,
		int height,
		int width,
		int list_height,
		int item_no,
		char **items,
		int flag)
{
    int result;
    int i, j;
    DIALOG_LISTITEM *listitems;
    int *depths;
    bool show_status = FALSE;
    int current = 0;
    char *help_result;

    DLG_TRACE(("# treeview args:\n"));
    DLG_TRACE2S("title", title);
    DLG_TRACE2S("message", cprompt);
    DLG_TRACE2N("height", height);
    DLG_TRACE2N("width", width);
    DLG_TRACE2N("lheight", list_height);
    DLG_TRACE2N("llength", item_no);
    /* FIXME dump the items[][] too */
    DLG_TRACE2N("flag", flag);

    listitems = dlg_calloc(DIALOG_LISTITEM, (size_t) item_no + 1);
    assert_ptr(listitems, "dialog_treeview");

    depths = dlg_calloc(int, (size_t) item_no + 1);
    assert_ptr(depths, "dialog_treeview");

    for (i = j = 0; i < item_no; ++i) {
	listitems[i].name = items[j++];
	listitems[i].text = (dialog_vars.no_items
			     ? dlg_strempty()
			     : items[j++]);
	listitems[i].state = !dlg_strcmp(items[j++], "on");
	depths[i] = atoi(items[j++]);
	listitems[i].help = ((dialog_vars.item_help)
			     ? items[j++]
			     : dlg_strempty());
    }
    dlg_align_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);

    result = dlg_treeview(title,
			  cprompt,
			  height,
			  width,
			  list_height,
			  item_no,
			  listitems,
			  NULL,
			  depths,
			  flag,
			  &current);

    switch (result) {
    case DLG_EXIT_OK:		/* FALLTHRU */
    case DLG_EXIT_EXTRA:
	show_status = TRUE;
	break;
    case DLG_EXIT_HELP:
	dlg_add_help_listitem(&result, &help_result, &listitems[current]);
	if ((show_status = dialog_vars.help_status)) {
	    if (dialog_vars.separate_output) {
		dlg_add_string(help_result);
		dlg_add_separator();
	    } else {
		dlg_add_quoted(help_result);
	    }
	} else {
	    dlg_add_string(help_result);
	}
	break;
    }

    if (show_status) {
	for (i = 0; i < item_no; i++) {
	    if (listitems[i].state) {
		if (dlg_need_separator())
		    dlg_add_separator();
		if (dialog_vars.separate_output) {
		    dlg_add_string(listitems[i].name);
		} else {
		    if (flag == FLAG_CHECK)
			dlg_add_quoted(listitems[i].name);
		    else
			dlg_add_string(listitems[i].name);
		}
	    }
	}
	AddLastKey();
    }

    dlg_free_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);
    free(depths);
    free(listitems);
    return result;
}
