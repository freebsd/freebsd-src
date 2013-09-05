/*
 *  $Id: buildlist.c,v 1.57 2013/03/17 13:46:30 tom Exp $
 *
 *  buildlist.c -- implements the buildlist dialog
 *
 *  Copyright 2012,2013	Thomas E. Dickey
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

#include <dialog.h>
#include <dlg_keys.h>

/*
 * Visually like menubox, but two columns.
 */

#define sLEFT         (-2)
#define sRIGHT        (-1)

#define KEY_TOGGLE    ' '
#define KEY_LEFTCOL   '^'
#define KEY_RIGHTCOL  '$'

#define MIN_HIGH  (1 + (5 * MARGIN))

typedef struct {
    WINDOW *win;
    int box_y;
    int box_x;
    int top_index;
    int cur_index;
} MY_DATA;

typedef struct {
    DIALOG_LISTITEM *items;
    int base_y;			/* base for mouse coordinates */
    int base_x;
    int use_height;		/* actual size of column box */
    int use_width;
    int item_no;
    int check_x;
    int item_x;
    MY_DATA list[2];
} ALL_DATA;

/*
 * Print list item.  The 'selected' parameter is true if 'choice' is the
 * current item.  That one is colored differently from the other items.
 */
static void
print_item(ALL_DATA * data,
	   WINDOW *win,
	   DIALOG_LISTITEM * item,
	   int choice,
	   int selected)
{
    chtype save = dlg_get_attrs(win);
    int i;
    bool both = (!dialog_vars.no_tags && !dialog_vars.no_items);
    bool first = TRUE;
    int climit = (data->item_x - data->check_x - 1);
    const char *show = (dialog_vars.no_items
			? item->name
			: item->text);

    /* Clear 'residue' of last item */
    (void) wattrset(win, menubox_attr);
    (void) wmove(win, choice, 0);
    for (i = 0; i < getmaxx(win); i++)
	(void) waddch(win, ' ');

    (void) wmove(win, choice, data->check_x);
    (void) wattrset(win, menubox_attr);

    if (both) {
	dlg_print_listitem(win, item->name, climit, first, selected);
	(void) waddch(win, ' ');
	first = FALSE;
    }

    (void) wmove(win, choice, data->item_x);
    climit = (getmaxx(win) - data->item_x + 1);
    dlg_print_listitem(win, show, climit, first, selected);

    if (selected) {
	dlg_item_help(item->help);
    }
    (void) wattrset(win, save);
}

/*
 * Prints either the left (unselected) or right (selected) list.
 */
static void
print_1_list(ALL_DATA * data,
	     int choice,
	     int selected)
{
    MY_DATA *moi = data->list + selected;
    WINDOW *win = moi->win;
    int i, j;
    int last = 0;
    int max_rows = getmaxy(win);

    for (i = j = 0; j < max_rows; i++) {
	int ii = i + moi->top_index;
	if (ii >= data->item_no) {
	    break;
	} else if (!(selected ^ (data->items[ii].state != 0))) {
	    print_item(data,
		       win,
		       &data->items[ii],
		       j, ii == choice);
	    last = ++j;
	}
    }
    if (wmove(win, last, 0) != ERR)
	wclrtobot(win);
    (void) wnoutrefresh(win);
}

/*
 * Return the previous item from the list, staying in the same column.  If no
 * further movement is possible, return the same choice as given.
 */
static int
prev_item(ALL_DATA * data, int choice, int selected)
{
    int result = choice;
    int n;

    for (n = choice - 1; n >= 0; --n) {
	if ((data->items[n].state != 0) == selected) {
	    result = n;
	    break;
	}
    }
    return result;
}

/*
 * Return true if the given choice is on the first page in the current column.
 */
static bool
stop_prev(ALL_DATA * data, int choice, int selected)
{
    return (prev_item(data, choice, selected) == choice);
}

static bool
check_hotkey(DIALOG_LISTITEM * items, int choice, int selected)
{
    bool result = FALSE;

    if ((items[choice].state != 0) == selected) {
	if (dlg_match_char(dlg_last_getc(),
			   (dialog_vars.no_tags
			    ? items[choice].text
			    : items[choice].name))) {
	    result = TRUE;
	}
    }
    return result;
}

/*
 * Return the next item from the list, staying in the same column.  If no
 * further movement is possible, return the same choice as given.
 */
static int
next_item(ALL_DATA * data, int choice, int selected)
{
    int result = choice;
    int n;

    for (n = choice + 1; n < data->item_no; ++n) {
	if ((data->items[n].state != 0) == selected) {
	    result = n;
	    break;
	}
    }
    dlg_trace_msg("next_item(%d) ->%d\n", choice, result);
    return result;
}

/*
 * Translate a choice from items[] to a row-number in an unbounded column,
 * starting at zero.
 */
static int
index2row(ALL_DATA * data, int choice, int selected)
{
    int result = -1;
    int n;
    for (n = 0; n < data->item_no; ++n) {
	if ((data->items[n].state != 0) == selected) {
	    ++result;
	}
	if (n == choice)
	    break;
    }
    return result;
}

/*
 * Return the first choice from items[] for the given column.
 */
static int
first_item(ALL_DATA * data, int selected)
{
    int result = -1;
    int n;

    for (n = 0; n < data->item_no; ++n) {
	if ((data->items[n].state != 0) == selected) {
	    result = n;
	    break;
	}
    }
    return result;
}

/*
 * Return the last choice from items[] for the given column.
 */
static int
last_item(ALL_DATA * data, int selected)
{
    int result = -1;
    int n;

    for (n = data->item_no - 1; n >= 0; --n) {
	if ((data->items[n].state != 0) == selected) {
	    result = n;
	    break;
	}
    }
    return result;
}

/*
 * Convert a row-number back to an item number, i.e., index into items[].
 */
static int
row2index(ALL_DATA * data, int row, int selected)
{
    int result = -1;
    int n;
    for (n = 0; n < data->item_no; ++n) {
	if ((data->items[n].state != 0) == selected) {
	    if (row-- <= 0) {
		result = n;
		break;
	    }
	}
    }
    return result;
}

static int
skip_rows(ALL_DATA * data, int row, int skip, int selected)
{
    int choice = row2index(data, row, selected);
    int result = row;
    int n;
    if (skip > 0) {
	for (n = choice + 1; n < data->item_no; ++n) {
	    if ((data->items[n].state != 0) == selected) {
		++result;
		if (--skip <= 0)
		    break;
	    }
	}
    } else if (skip < 0) {
	for (n = choice - 1; n >= 0; --n) {
	    if ((data->items[n].state != 0) == selected) {
		--result;
		if (++skip >= 0)
		    break;
	    }
	}
    }
    return result;
}

/*
 * Find the closest item in the given column starting with the given choice.
 */
static int
closest_item(ALL_DATA * data, int choice, int selected)
{
    int prev = choice;
    int next = choice;
    int result = choice;
    int n;

    for (n = choice; n >= 0; --n) {
	if ((data->items[n].state != 0) == selected) {
	    prev = n;
	    break;
	}
    }
    for (n = choice; n < data->item_no; ++n) {
	if ((data->items[n].state != 0) == selected) {
	    next = n;
	    break;
	}
    }
    if (prev != choice) {
	result = prev;
	if (next != choice) {
	    if ((choice - prev) > (next - choice)) {
		result = next;
	    }
	}
    } else if (next != choice) {
	result = next;
    }
    return result;
}

static void
print_both(ALL_DATA * data,
	   int choice)
{
    int selected;
    int cur_y, cur_x;
    WINDOW *dialog = wgetparent(data->list[0].win);

    getyx(dialog, cur_y, cur_x);
    for (selected = 0; selected < 2; ++selected) {
	MY_DATA *moi = data->list + selected;
	WINDOW *win = moi->win;
	int thumb_top = index2row(data, moi->top_index, selected);
	int thumb_max = index2row(data, -1, selected);
	int thumb_end = thumb_top + getmaxy(win);

	print_1_list(data, choice, selected);

	dlg_mouse_setcode(selected * KEY_MAX);
	dlg_draw_scrollbar(dialog,
			   (long) (moi->top_index),
			   (long) (thumb_top),
			   (long) MIN(thumb_end, thumb_max),
			   (long) thumb_max,
			   moi->box_x + data->check_x,
			   moi->box_x + getmaxx(win),
			   moi->box_y,
			   moi->box_y + getmaxy(win) + 1,
			   menubox_border2_attr,
			   menubox_border_attr);
    }
    (void) wmove(dialog, cur_y, cur_x);
    dlg_mouse_setcode(0);
}

static void
set_top_item(ALL_DATA * data, int value, int selected)
{
    if (value != data->list[selected].top_index) {
	dlg_trace_msg("set top of %s column to %d\n",
		      selected ? "right" : "left",
		      value);
	data->list[selected].top_index = value;
    }
}

/*
 * Adjust the top-index as needed to ensure that it and the given item are
 * visible.
 */
static void
fix_top_item(ALL_DATA * data, int cur_item, int selected)
{
    int top_item = data->list[selected].top_index;
    int cur_row = index2row(data, cur_item, selected);
    int top_row = index2row(data, top_item, selected);

    if (cur_row < top_row) {
	top_item = cur_item;
    } else if ((cur_row - top_row) > data->use_height) {
	top_item = row2index(data, cur_row + 1 - data->use_height, selected);
    }
    if (cur_row < data->use_height) {
	top_item = row2index(data, 0, selected);
    }
    dlg_trace_msg("fix_top_item(cur_item %d, selected %d) ->top_item %d\n",
		  cur_item, selected, top_item);
    set_top_item(data, top_item, selected);
}

/*
 * This is an alternate interface to 'buildlist' which allows the application
 * to read the list item states back directly without putting them in the
 * output buffer.
 */
int
dlg_buildlist(const char *title,
	      const char *cprompt,
	      int height,
	      int width,
	      int list_height,
	      int item_no,
	      DIALOG_LISTITEM * items,
	      const char *states,
	      int order_mode,
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
	DLG_KEYS_DATA( DLGK_PAGE_NEXT,	DLGK_MOUSE(KEY_NPAGE+KEY_MAX) ),
	DLG_KEYS_DATA( DLGK_PAGE_PREV,	KEY_PPAGE ),
	DLG_KEYS_DATA( DLGK_PAGE_PREV,	DLGK_MOUSE(KEY_PPAGE) ),
	DLG_KEYS_DATA( DLGK_PAGE_PREV,	DLGK_MOUSE(KEY_PPAGE+KEY_MAX) ),
	DLG_KEYS_DATA( DLGK_GRID_LEFT,	KEY_LEFTCOL ),
	DLG_KEYS_DATA( DLGK_GRID_RIGHT,	KEY_RIGHTCOL ),
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

#ifdef KEY_RESIZE
    int old_height = height;
    int old_width = width;
#endif
    ALL_DATA all;
    MY_DATA *data = all.list;
    int i, j, k, key2, found, x, y, cur_x, cur_y;
    int key = 0, fkey;
    bool save_visit = dialog_state.visit_items;
    int button;
    int cur_item;
    int was_mouse;
    int name_width, text_width, full_width, list_width;
    int result = DLG_EXIT_UNKNOWN;
    int num_states;
    bool first = TRUE;
    WINDOW *dialog;
    char *prompt = dlg_strclone(cprompt);
    const char **buttons = dlg_ok_labels();
    const char *widget_name = "buildlist";

    (void) order_mode;

    /*
     * Unlike other uses of --visit-items, we have two windows to visit.
     */
    if (dialog_state.visit_cols)
	dialog_state.visit_cols = 2;

    memset(&all, 0, sizeof(all));
    all.items = items;
    all.item_no = item_no;

    if (dialog_vars.default_item != 0) {
	cur_item = dlg_default_listitem(items);
    } else {
	if ((cur_item = first_item(&all, 0)) < 0)
	    cur_item = first_item(&all, 1);
    }
    button = (dialog_state.visit_items
	      ? (items[cur_item].state ? sRIGHT : sLEFT)
	      : dlg_default_button());

    dlg_does_output();
    dlg_tab_correct_str(prompt);

#ifdef KEY_RESIZE
  retry:
#endif

    all.use_height = list_height;
    all.use_width = (2 * (dlg_calc_list_width(item_no, items)
			  + 4
			  + 2 * MARGIN)
		     + 1);
    all.use_width = MAX(26, all.use_width);
    if (all.use_height == 0) {
	/* calculate height without items (4) */
	dlg_auto_size(title, prompt, &height, &width, MIN_HIGH, all.use_width);
	dlg_calc_listh(&height, &all.use_height, item_no);
    } else {
	dlg_auto_size(title, prompt,
		      &height, &width,
		      MIN_HIGH + all.use_height, all.use_width);
    }
    dlg_button_layout(buttons, &width);
    dlg_print_size(height, width);
    dlg_ctl_size(height, width);

    /* we need at least two states */
    if (states == 0 || strlen(states) < 2)
	states = " *";
    num_states = (int) strlen(states);

    x = dlg_box_x_ordinate(width);
    y = dlg_box_y_ordinate(height);

    dialog = dlg_new_window(height, width, y, x);
    dlg_register_window(dialog, widget_name, binding);
    dlg_register_buttons(dialog, widget_name, buttons);

    dlg_mouse_setbase(all.base_x = x, all.base_y = y);

    dlg_draw_box2(dialog, 0, 0, height, width, dialog_attr, border_attr, border2_attr);
    dlg_draw_bottom_box2(dialog, border_attr, border2_attr, dialog_attr);
    dlg_draw_title(dialog, title);

    (void) wattrset(dialog, dialog_attr);
    dlg_print_autowrap(dialog, prompt, height, width);

    list_width = (width - 6 * MARGIN - 2) / 2;
    getyx(dialog, cur_y, cur_x);
    data[0].box_y = cur_y + 1;
    data[0].box_x = MARGIN + 1;
    data[1].box_y = cur_y + 1;
    data[1].box_x = data[0].box_x + 1 + 2 * MARGIN + list_width;

    /*
     * After displaying the prompt, we know how much space we really have.
     * Limit the list to avoid overwriting the ok-button.
     */
    if (all.use_height + MIN_HIGH > height - cur_y)
	all.use_height = height - MIN_HIGH - cur_y;
    if (all.use_height <= 0)
	all.use_height = 1;

    for (k = 0; k < 2; ++k) {
	/* create new window for the list */
	data[k].win = dlg_sub_window(dialog, all.use_height, list_width,
				     y + data[k].box_y + 1,
				     x + data[k].box_x + 1);

	/* draw a box around the list items */
	dlg_draw_box(dialog, data[k].box_y, data[k].box_x,
		     all.use_height + 2 * MARGIN,
		     list_width + 2 * MARGIN,
		     menubox_border_attr, menubox_border2_attr);
    }

    text_width = 0;
    name_width = 0;
    /* Find length of longest item to center buildlist */
    for (i = 0; i < item_no; i++) {
	text_width = MAX(text_width, dlg_count_columns(items[i].text));
	name_width = MAX(name_width, dlg_count_columns(items[i].name));
    }

    /* If the name+text is wider than the list is allowed, then truncate
     * one or both of them.  If the name is no wider than 1/4 of the list,
     * leave it intact.
     */
    all.use_width = (list_width - 6 * MARGIN);
    if (dialog_vars.no_tags && !dialog_vars.no_items) {
	full_width = MIN(all.use_width, text_width);
    } else if (dialog_vars.no_items) {
	full_width = MIN(all.use_width, name_width);
    } else {
	if (text_width >= 0
	    && name_width >= 0
	    && all.use_width > 0
	    && text_width + name_width > all.use_width) {
	    int need = (int) (0.25 * all.use_width);
	    if (name_width > need) {
		int want = (int) (all.use_width * ((double) name_width) /
				  (text_width + name_width));
		name_width = (want > need) ? want : need;
	    }
	    text_width = all.use_width - name_width;
	}
	full_width = text_width + name_width;
    }

    all.check_x = (all.use_width - full_width) / 2;
    all.item_x = ((dialog_vars.no_tags
		   ? 0
		   : (dialog_vars.no_items
		      ? 0
		      : (name_width + 2)))
		  + all.check_x);

    /* ensure we are scrolled to show the current choice */
    j = MIN(all.use_height, item_no);
    for (i = 0; i < 2; ++i) {
	int top_item = 0;
	if ((items[cur_item].state != 0) == i) {
	    top_item = cur_item - j + 1;
	    if (top_item < 0)
		top_item = 0;
	    set_top_item(&all, top_item, i);
	} else {
	    set_top_item(&all, 0, i);
	}
    }

    /* register the new window, along with its borders */
    for (i = 0; i < 2; ++i) {
	dlg_mouse_mkbigregion(data[i].box_y + 1,
			      data[i].box_x,
			      all.use_height,
			      list_width + 2,
			      2 * KEY_MAX + (i * (1 + all.use_height)),
			      1, 1, 1 /* by lines */ );
    }

    dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);

    while (result == DLG_EXIT_UNKNOWN) {
	int which = (items[cur_item].state != 0);
	MY_DATA *moi = data + which;
	int at_top = index2row(&all, moi->top_index, which);
	int at_end = index2row(&all, -1, which);
	int at_bot = skip_rows(&all, at_top, all.use_height, which);

	dlg_trace_msg("\t** state %d:%d top %d (%d:%d:%d) %d\n",
		      cur_item, item_no - 1,
		      moi->top_index,
		      at_top, at_bot, at_end,
		      which);

	if (first) {
	    print_both(&all, cur_item);
	    dlg_trace_win(dialog);
	    first = FALSE;
	}

	if (button < 0) {	/* --visit-items */
	    int cur_row = index2row(&all, cur_item, which);
	    cur_y = (data[which].box_y
		     + cur_row
		     + 1);
	    if (at_top > 0)
		cur_y -= at_top;
	    cur_x = (data[which].box_x
		     + all.check_x + 1);
	    dlg_trace_msg("\t...visit row %d (%d,%d)\n", cur_row, cur_y, cur_x);
	    wmove(dialog, cur_y, cur_x);
	}

	key = dlg_mouse_wgetch(dialog, &fkey);
	if (dlg_result_key(key, fkey, &result))
	    break;

	was_mouse = (fkey && is_DLGK_MOUSE(key));
	if (was_mouse)
	    key -= M_EVENT;

	if (!was_mouse) {
	    ;
	} else if (key >= 2 * KEY_MAX) {
	    i = (key - 2 * KEY_MAX) % (1 + all.use_height);
	    j = (key - 2 * KEY_MAX) / (1 + all.use_height);
	    k = row2index(&all, i + at_top, j);
	    dlg_trace_msg("MOUSE column %d, row %d ->item %d\n", j, i, k);
	    if (k >= 0 && j < 2) {
		if (j != which) {
		    /*
		     * Mouse click was in the other column.
		     */
		    moi = data + j;
		    fix_top_item(&all, k, j);
		}
		which = j;
		at_top = index2row(&all, moi->top_index, which);
		at_bot = skip_rows(&all, at_top, all.use_height, which);
		cur_item = k;
		print_both(&all, cur_item);
		key = KEY_TOGGLE;	/* force the selected item to toggle */
	    } else {
		beep();
		continue;
	    }
	    fkey = FALSE;
	} else if (key >= KEY_MIN) {
	    if (key > KEY_MAX) {
		if (which == 0) {
		    key = KEY_RIGHTCOL;		/* switch to right-column */
		    fkey = FALSE;
		} else {
		    key -= KEY_MAX;
		}
	    } else {
		if (which == 1) {
		    key = KEY_LEFTCOL;	/* switch to left-column */
		    fkey = FALSE;
		}
	    }
	    key = dlg_lookup_key(dialog, key, &fkey);
	}

	/*
	 * A space toggles the item status.  Normally we put the cursor on
	 * the next available item in the same column.  But if there are no
	 * more items in the column, move the cursor to the other column.
	 */
	if (key == KEY_TOGGLE) {
	    int new_choice;
	    int new_state = items[cur_item].state + 1;

	    if ((new_choice = next_item(&all, cur_item, which)) == cur_item) {
		new_choice = prev_item(&all, cur_item, which);
	    }
	    dlg_trace_msg("cur_item %d, new_choice:%d\n", cur_item, new_choice);
	    if (new_state >= num_states)
		new_state = 0;

	    items[cur_item].state = new_state;
	    if (cur_item == moi->top_index) {
		set_top_item(&all, new_choice, which);
	    }

	    if (new_choice >= 0) {
		fix_top_item(&all, cur_item, !which);
		cur_item = new_choice;
	    }
	    print_both(&all, cur_item);
	    dlg_trace_win(dialog);
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
		for (j = cur_item + 1; j < item_no; j++) {
		    if (check_hotkey(items, j, which)) {
			found = TRUE;
			i = j;
			break;
		    }
		}
		if (!found) {
		    for (j = 0; j <= cur_item; j++) {
			if (check_hotkey(items, j, which)) {
			    found = TRUE;
			    i = j;
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
	    && (key - '1' < at_bot)) {
	    found = TRUE;
	    i = key - '1';
	}

	if (!found && fkey) {
	    switch (key) {
	    case DLGK_FIELD_PREV:
		if ((button == sRIGHT) && dialog_state.visit_items) {
		    key = DLGK_GRID_LEFT;
		    button = sLEFT;
		} else {
		    button = dlg_prev_button(buttons, button);
		    dlg_draw_buttons(dialog, height - 2, 0, buttons, button,
				     FALSE, width);
		    if (button == sRIGHT) {
			key = DLGK_GRID_RIGHT;
		    } else {
			continue;
		    }
		}
		break;
	    case DLGK_FIELD_NEXT:
		if ((button == sLEFT) && dialog_state.visit_items) {
		    key = DLGK_GRID_RIGHT;
		    button = sRIGHT;
		} else {
		    button = dlg_next_button(buttons, button);
		    dlg_draw_buttons(dialog, height - 2, 0, buttons, button,
				     FALSE, width);
		    if (button == sLEFT) {
			key = DLGK_GRID_LEFT;
		    } else {
			continue;
		    }
		}
		break;
	    }
	}

	if (!found && fkey) {
	    i = cur_item;
	    found = TRUE;
	    switch (key) {
	    case DLGK_GRID_LEFT:
		i = closest_item(&all, cur_item, 0);
		fix_top_item(&all, i, 0);
		break;
	    case DLGK_GRID_RIGHT:
		i = closest_item(&all, cur_item, 1);
		fix_top_item(&all, i, 1);
		break;
	    case DLGK_PAGE_PREV:
		if (cur_item > moi->top_index) {
		    i = moi->top_index;
		} else if (moi->top_index != 0) {
		    int temp = at_top;
		    if ((temp -= all.use_height) < 0)
			temp = 0;
		    i = row2index(&all, temp, which);
		}
		break;
	    case DLGK_PAGE_NEXT:
		if ((at_end - at_bot) < all.use_height) {
		    i = next_item(&all,
				  row2index(&all, at_end, which),
				  which);
		} else {
		    i = next_item(&all,
				  row2index(&all, at_bot, which),
				  which);
		    at_top = at_bot;
		    set_top_item(&all,
				 next_item(&all,
					   row2index(&all, at_top, which),
					   which),
				 which);
		    at_bot = skip_rows(&all, at_top, all.use_height, which);
		    at_bot = MIN(at_bot, at_end);
		}
		break;
	    case DLGK_ITEM_FIRST:
		i = first_item(&all, which);
		break;
	    case DLGK_ITEM_LAST:
		i = last_item(&all, which);
		break;
	    case DLGK_ITEM_PREV:
		i = prev_item(&all, cur_item, which);
		if (stop_prev(&all, cur_item, which))
		    continue;
		break;
	    case DLGK_ITEM_NEXT:
		i = next_item(&all, cur_item, which);
		break;
	    default:
		found = FALSE;
		break;
	    }
	}

	if (found) {
	    if (i != cur_item) {
		int now_at = index2row(&all, i, which);
		int oops = item_no;
		int old_item;

		dlg_trace_msg("<--CHOICE %d\n", i);
		dlg_trace_msg("<--topITM %d\n", moi->top_index);
		dlg_trace_msg("<--now_at %d\n", now_at);
		dlg_trace_msg("<--at_top %d\n", at_top);
		dlg_trace_msg("<--at_bot %d\n", at_bot);

		if (now_at >= at_bot) {
		    while (now_at >= at_bot) {
			if ((at_bot - at_top) >= all.use_height) {
			    set_top_item(&all,
					 next_item(&all, moi->top_index, which),
					 which);
			}
			at_top = index2row(&all, moi->top_index, which);
			at_bot = skip_rows(&all, at_top, all.use_height, which);

			dlg_trace_msg("...at_bot %d (now %d vs %d)\n",
				      at_bot, now_at, at_end);
			dlg_trace_msg("...topITM %d\n", moi->top_index);
			dlg_trace_msg("...at_top %d (diff %d)\n", at_top,
				      at_bot - at_top);

			if (at_bot >= at_end) {
			    /*
			     * If we bumped into the end, move the top-item
			     * down by one line so that we can display the
			     * last item in the list.
			     */
			    if ((at_bot - at_top) > all.use_height) {
				set_top_item(&all,
					     next_item(&all, moi->top_index, which),
					     which);
			    } else if (at_top > 0 &&
				       (at_bot - at_top) >= all.use_height) {
				set_top_item(&all,
					     next_item(&all, moi->top_index, which),
					     which);
			    }
			    break;
			}
			if (--oops < 0) {
			    dlg_trace_msg("OOPS-forward\n");
			    break;
			}
		    }
		} else if (now_at < at_top) {
		    while (now_at < at_top) {
			old_item = moi->top_index;
			set_top_item(&all,
				     prev_item(&all, moi->top_index, which),
				     which);
			at_top = index2row(&all, moi->top_index, which);

			dlg_trace_msg("...at_top %d (now %d)\n", at_top, now_at);
			dlg_trace_msg("...topITM %d\n", moi->top_index);

			if (moi->top_index >= old_item)
			    break;
			if (at_top <= now_at)
			    break;
			if (--oops < 0) {
			    dlg_trace_msg("OOPS-backward\n");
			    break;
			}
		    }
		}
		dlg_trace_msg("-->now_at %d\n", now_at);
		cur_item = i;
		print_both(&all, cur_item);
	    }
	    dlg_trace_win(dialog);
	    continue;		/* wait for another key press */
	}

	if (fkey) {
	    switch (key) {
	    case DLGK_ENTER:
		result = dlg_enter_buttoncode(button);
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

    dialog_state.visit_cols = save_visit;
    dlg_del_window(dialog);
    dlg_mouse_free_regions();
    free(prompt);
    *current_item = cur_item;
    return result;
}

/*
 * Display a dialog box with a list of options that can be turned on or off
 */
int
dialog_buildlist(const char *title,
		 const char *cprompt,
		 int height,
		 int width,
		 int list_height,
		 int item_no,
		 char **items,
		 int order_mode)
{
    int result;
    int i, j;
    DIALOG_LISTITEM *listitems;
    bool separate_output = dialog_vars.separate_output;
    bool show_status = FALSE;
    int current = 0;

    listitems = dlg_calloc(DIALOG_LISTITEM, (size_t) item_no + 1);
    assert_ptr(listitems, "dialog_buildlist");

    for (i = j = 0; i < item_no; ++i) {
	listitems[i].name = items[j++];
	listitems[i].text = (dialog_vars.no_items
			     ? dlg_strempty()
			     : items[j++]);
	listitems[i].state = !dlg_strcmp(items[j++], "on");
	listitems[i].help = ((dialog_vars.item_help)
			     ? items[j++]
			     : dlg_strempty());
    }
    dlg_align_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);

    result = dlg_buildlist(title,
			   cprompt,
			   height,
			   width,
			   list_height,
			   item_no,
			   listitems,
			   NULL,
			   order_mode,
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
		    dlg_add_quoted(listitems[i].name);
		}
	    }
	}
	dlg_add_last_key(-1);
    }

    dlg_free_columns(&listitems[0].text, (int) sizeof(DIALOG_LISTITEM), item_no);
    free(listitems);
    return result;
}
