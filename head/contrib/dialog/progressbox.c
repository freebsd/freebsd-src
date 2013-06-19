/*
 *  $Id: progressbox.c,v 1.23 2012/12/21 10:00:05 tom Exp $
 *
 *  progressbox.c -- implements the progress box
 *
 *  Copyright 2005	Valery Reznic
 *  Copyright 2006-2012	Thomas E. Dickey
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1 of the
 *  License, or (at your option) any later version.
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

#define MIN_HIGH (4)
#define MIN_WIDE (10 + 2 * (2 + MARGIN))

typedef struct {
    DIALOG_CALLBACK obj;
    WINDOW *text;
    char line[MAX_LEN + 1];
    int is_eof;
} MY_OBJ;

/*
 * Return current line of text.
 */
static char *
get_line(MY_OBJ * obj)
{
    FILE *fp = obj->obj.input;
    int col = 0;
    int j, tmpint, ch;

    for (;;) {
	if ((ch = getc(fp)) == EOF) {
	    obj->is_eof = 1;
	    if (col) {
		break;
	    } else {
		return NULL;
	    }
	}
	if (ch == '\n')
	    break;
	if (ch == '\r')
	    break;
	if (col >= MAX_LEN)
	    continue;
	if ((ch == TAB) && (dialog_vars.tab_correct)) {
	    tmpint = dialog_state.tab_len
		- (col % dialog_state.tab_len);
	    for (j = 0; j < tmpint; j++) {
		if (col < MAX_LEN) {
		    obj->line[col] = ' ';
		    ++col;
		} else {
		    break;
		}
	    }
	} else {
	    obj->line[col] = (char) ch;
	    ++col;
	}
    }

    obj->line[col] = '\0';

    return obj->line;
}

/*
 * Print a new line of text.
 */
static void
print_line(MY_OBJ * obj, WINDOW *win, int row, int width)
{
    int i, y, x;
    char *line = obj->line;

    (void) wmove(win, row, 0);	/* move cursor to correct line */
    (void) waddch(win, ' ');
#ifdef NCURSES_VERSION
    (void) waddnstr(win, line, MIN((int) strlen(line), width - 2));
#else
    line[MIN((int) strlen(line), width - 2)] = '\0';
    waddstr(win, line);
#endif

    getyx(win, y, x);
    (void) y;
    /* Clear 'residue' of previous line */
    for (i = 0; i < width - x; i++)
	(void) waddch(win, ' ');
}

static int
pause_for_ok(WINDOW *dialog, int height, int width)
{
    /* *INDENT-OFF* */
    static DLG_KEYS_BINDING binding[] = {
	HELPKEY_BINDINGS,
	ENTERKEY_BINDINGS,
	TRAVERSE_BINDINGS,
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

    int button;
    int key = 0, fkey;
    int result = DLG_EXIT_UNKNOWN;
    const char **buttons = dlg_ok_label();
    int check;
    int save_nocancel = dialog_vars.nocancel;
    bool redraw = TRUE;

    dialog_vars.nocancel = TRUE;
    button = dlg_default_button();

    dlg_register_window(dialog, "progressbox", binding);
    dlg_register_buttons(dialog, "progressbox", buttons);

    dlg_draw_bottom_box2(dialog, border_attr, border2_attr, dialog_attr);
    mouse_mkbutton(height - 2, width / 2 - 4, 6, '\n');

    while (result == DLG_EXIT_UNKNOWN) {
	if (redraw) {
	    redraw = FALSE;
	    if (button < 0)
		button = 0;
	    dlg_draw_buttons(dialog,
			     height - 2, 0,
			     buttons, button,
			     FALSE, width);
	}

	key = dlg_mouse_wgetch(dialog, &fkey);
	if (dlg_result_key(key, fkey, &result))
	    break;

	if (!fkey && (check = dlg_char_to_button(key, buttons)) >= 0) {
	    result = dlg_ok_buttoncode(check);
	    break;
	}

	if (fkey) {
	    switch (key) {
	    case DLGK_FIELD_NEXT:
		button = dlg_next_button(buttons, button);
		redraw = TRUE;
		break;
	    case DLGK_FIELD_PREV:
		button = dlg_prev_button(buttons, button);
		redraw = TRUE;
		break;
	    case DLGK_ENTER:
		result = dlg_ok_buttoncode(button);
		break;
	    default:
		if (is_DLGK_MOUSE(key)) {
		    result = dlg_ok_buttoncode(key - M_EVENT);
		    if (result < 0)
			result = DLG_EXIT_OK;
		} else {
		    beep();
		}
		break;
	    }

	} else {
	    beep();
	}
    }
    dlg_unregister_window(dialog);

    dialog_vars.nocancel = save_nocancel;
    return result;
}

int
dlg_progressbox(const char *title,
		const char *cprompt,
		int height,
		int width,
		int pauseopt,
		FILE *fp)
{
    int i;
    int x, y, thigh;
    WINDOW *dialog, *text;
    MY_OBJ *obj;
    char *prompt = dlg_strclone(cprompt);
    int result;

    dlg_tab_correct_str(prompt);
    dlg_auto_size(title, prompt, &height, &width, MIN_HIGH, MIN_WIDE);
    dlg_print_size(height, width);
    dlg_ctl_size(height, width);

    x = dlg_box_x_ordinate(width);
    y = dlg_box_y_ordinate(height);
    thigh = height - (2 * MARGIN);

    dialog = dlg_new_window(height, width, y, x);

    dlg_draw_box2(dialog, 0, 0, height, width, dialog_attr, border_attr, border2_attr);
    dlg_draw_title(dialog, title);
    dlg_draw_helpline(dialog, FALSE);

    if (*prompt != '\0') {
	int y2, x2;

	(void) wattrset(dialog, dialog_attr);
	dlg_print_autowrap(dialog, prompt, height, width);
	getyx(dialog, y2, x2);
	(void) x2;
	++y2;
	wmove(dialog, y2, MARGIN);
	for (i = 0; i < getmaxx(dialog) - 2 * MARGIN; i++)
	    (void) waddch(dialog, dlg_boxchar(ACS_HLINE));
	y += y2;
	thigh -= y2;
    }

    /* Create window for text region, used for scrolling text */
    text = dlg_sub_window(dialog,
			  thigh,
			  width - (2 * MARGIN),
			  y + MARGIN,
			  x + MARGIN);

    (void) wrefresh(dialog);

    (void) wmove(dialog, thigh, (MARGIN + 1));
    (void) wnoutrefresh(dialog);

    obj = dlg_calloc(MY_OBJ, 1);
    assert_ptr(obj, "dlg_progressbox");

    obj->obj.input = fp;
    obj->obj.win = dialog;
    obj->text = text;

    dlg_attr_clear(text, thigh, getmaxx(text), dialog_attr);
    for (i = 0; get_line(obj); i++) {
	if (i < thigh) {
	    print_line(obj, text, i, width - (2 * MARGIN));
	} else {
	    scrollok(text, TRUE);
	    scroll(text);
	    scrollok(text, FALSE);
	    print_line(obj, text, thigh - 1, width - (2 * MARGIN));
	}
	(void) wrefresh(text);
	dlg_trace_win(dialog);
	if (obj->is_eof)
	    break;
    }

    if (pauseopt) {
	scrollok(text, TRUE);
	wscrl(text, 1 + MARGIN);
	(void) wrefresh(text);
	result = pause_for_ok(dialog, height, width);
    } else {
	wrefresh(dialog);
	result = DLG_EXIT_OK;
    }

    dlg_del_window(dialog);
    free(prompt);
    free(obj);

    return result;
}

/*
 * Display text from a stdin in a scrolling window.
 */
int
dialog_progressbox(const char *title, const char *cprompt, int height, int width)
{
    int result;
    result = dlg_progressbox(title,
			     cprompt,
			     height,
			     width,
			     FALSE,
			     dialog_state.pipe_input);
    dialog_state.pipe_input = 0;
    return result;
}
