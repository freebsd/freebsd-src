/*
 *  $Id: progressbox.c,v 1.13 2011/06/27 08:18:20 tom Exp $
 *
 *  progressbox.c -- implements the progress box
 *
 *  Copyright 2005	Valery Reznic
 *  Copyright 2006-2011	Thomas E. Dickey
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

    while (1) {
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
	if ((ch == TAB) && (dialog_vars.tab_correct)) {
	    tmpint = dialog_state.tab_len
		- (col % dialog_state.tab_len);
	    for (j = 0; j < tmpint; j++) {
		if (col < MAX_LEN)
		    obj->line[col] = ' ';
		++col;
	    }
	} else {
	    obj->line[col] = (char) ch;
	    ++col;
	}
	if (col >= MAX_LEN)
	    break;
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
	DLG_KEYS_DATA( DLGK_ENTER,	' ' ),
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

    int button = 0;
    int key = 0, fkey;
    int result = DLG_EXIT_UNKNOWN;
    const char **buttons = dlg_ok_label();
    int check;

    dlg_register_window(dialog, "progressbox", binding);
    dlg_register_buttons(dialog, "progressbox", buttons);

    dlg_draw_bottom_box(dialog);
    mouse_mkbutton(height - 2, width / 2 - 4, 6, '\n');
    dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);

    while (result == DLG_EXIT_UNKNOWN) {
	key = dlg_mouse_wgetch(dialog, &fkey);
	if (dlg_result_key(key, fkey, &result))
	    break;

	if (!fkey && (check = dlg_char_to_button(key, buttons)) >= 0) {
	    result = check ? DLG_EXIT_HELP : DLG_EXIT_OK;
	    break;
	}

	if (fkey) {
	    switch (key) {
	    case DLGK_ENTER:
		result = button ? DLG_EXIT_HELP : DLG_EXIT_OK;
		break;
	    case DLGK_MOUSE(0):
		result = DLG_EXIT_OK;
		break;
	    case DLGK_MOUSE(1):
		result = DLG_EXIT_HELP;
		break;
	    default:
		beep();
		break;
	    }
	} else {
	    beep();
	}
    }
    dlg_unregister_window(dialog);
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

    dlg_draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    dlg_draw_title(dialog, title);
    dlg_draw_helpline(dialog, FALSE);

    if (*prompt != '\0') {
	int y2, x2;

	wattrset(dialog, dialog_attr);
	dlg_print_autowrap(dialog, prompt, height, width);
	getyx(dialog, y2, x2);
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

    return DLG_EXIT_OK;
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
