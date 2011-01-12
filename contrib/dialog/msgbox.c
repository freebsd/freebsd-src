/*
 *  $Id: msgbox.c,v 1.64 2010/01/15 10:50:17 tom Exp $
 *
 *  msgbox.c -- implements the message box and info box
 *
 *  Copyright 2000-2009,2010	Thomas E. Dickey
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
 */

#include <dialog.h>
#include <dlg_keys.h>

/*
 * Display a message box. Program will pause and display an "OK" button
 * if the parameter 'pauseopt' is non-zero.
 */
int
dialog_msgbox(const char *title, const char *cprompt, int height, int width,
	      int pauseopt)
{
    /* *INDENT-OFF* */
    static DLG_KEYS_BINDING binding[] = {
	ENTERKEY_BINDINGS,
	DLG_KEYS_DATA( DLGK_ENTER,	' ' ),
	SCROLLKEY_BINDINGS,
	DLG_KEYS_DATA( DLGK_FIELD_NEXT,	KEY_DOWN ),
	DLG_KEYS_DATA( DLGK_FIELD_NEXT, KEY_RIGHT ),
	DLG_KEYS_DATA( DLGK_FIELD_NEXT, TAB ),
	DLG_KEYS_DATA( DLGK_FIELD_PREV,	KEY_UP ),
	DLG_KEYS_DATA( DLGK_FIELD_PREV, KEY_BTAB ),
	DLG_KEYS_DATA( DLGK_FIELD_PREV, KEY_LEFT ),
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

    int x, y, last = 0, page;
    int button = 0;
    int key = 0, fkey;
    int result = DLG_EXIT_UNKNOWN;
    WINDOW *dialog = 0;
    char *prompt = dlg_strclone(cprompt);
    const char **buttons = dlg_ok_label();
    int offset = 0;
    int check;
    bool show = TRUE;
    int min_width = (pauseopt == 1 ? 12 : 0);

#ifdef KEY_RESIZE
    int req_high = height;
    int req_wide = width;
  restart:
#endif

    dlg_button_layout(buttons, &min_width);

    dlg_tab_correct_str(prompt);
    dlg_auto_size(title, prompt, &height, &width,
		  (pauseopt == 1 ? 2 : 0),
		  min_width);
    dlg_print_size(height, width);
    dlg_ctl_size(height, width);

    x = dlg_box_x_ordinate(width);
    y = dlg_box_y_ordinate(height);

#ifdef KEY_RESIZE
    if (dialog != 0)
	dlg_move_window(dialog, height, width, y, x);
    else
#endif
    {
	dialog = dlg_new_window(height, width, y, x);
	dlg_register_window(dialog, "msgbox", binding);
	dlg_register_buttons(dialog, "msgbox", buttons);
    }
    page = height - (1 + 3 * MARGIN);

    dlg_mouse_setbase(x, y);

    dlg_draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    dlg_draw_title(dialog, title);

    wattrset(dialog, dialog_attr);

    if (pauseopt) {
	dlg_draw_bottom_box(dialog);
	mouse_mkbutton(height - 2, width / 2 - 4, 6, '\n');
	dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);

	while (result == DLG_EXIT_UNKNOWN) {
	    if (show) {
		last = dlg_print_scrolled(dialog, prompt, offset,
					  page, width, pauseopt);
		show = FALSE;
	    }
	    key = dlg_mouse_wgetch(dialog, &fkey);
	    if (dlg_result_key(key, fkey, &result))
		break;

	    if (!fkey && (check = dlg_char_to_button(key, buttons)) >= 0) {
		result = check ? DLG_EXIT_HELP : DLG_EXIT_OK;
		break;
	    }

	    if (fkey) {
		switch (key) {
#ifdef KEY_RESIZE
		case KEY_RESIZE:
		    dlg_clear();
		    height = req_high;
		    width = req_wide;
		    show = TRUE;
		    goto restart;
#endif
		case DLGK_FIELD_NEXT:
		    button = dlg_next_button(buttons, button);
		    if (button < 0)
			button = 0;
		    dlg_draw_buttons(dialog,
				     height - 2, 0,
				     buttons, button,
				     FALSE, width);
		    break;
		case DLGK_FIELD_PREV:
		    button = dlg_prev_button(buttons, button);
		    if (button < 0)
			button = 0;
		    dlg_draw_buttons(dialog,
				     height - 2, 0,
				     buttons, button,
				     FALSE, width);
		    break;
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
		    if (dlg_check_scrolled(key,
					   last,
					   page,
					   &show,
					   &offset) == 0)
			break;
		    beep();
		    break;
		}
	    } else {
		beep();
	    }
	}
    } else {
	dlg_print_scrolled(dialog, prompt, offset, page, width, pauseopt);
	wrefresh(dialog);
	result = DLG_EXIT_OK;
    }

    dlg_del_window(dialog);
    dlg_mouse_free_regions();
    free(prompt);
    return result;
}
