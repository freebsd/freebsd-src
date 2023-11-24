/*
 *  $Id: yesno.c,v 1.71 2020/11/23 00:48:08 tom Exp $
 *
 *  yesno.c -- implements the yes/no box
 *
 *  Copyright 1999-2019,2020	Thomas E. Dickey
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
 *  An earlier version of this program lists as authors
 *	Savio Lam (lam836@cs.cuhk.hk)
 */

#include <dialog.h>
#include <dlg_keys.h>

/*
 * Display a dialog box with two buttons - Yes and No.
 */
int
dialog_yesno(const char *title, const char *cprompt, int height, int width)
{
    /* *INDENT-OFF* */
    static DLG_KEYS_BINDING binding[] = {
	HELPKEY_BINDINGS,
	ENTERKEY_BINDINGS,
	SCROLLKEY_BINDINGS,
	TRAVERSE_BINDINGS,
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

    int x, y;
    int key, fkey;
    int button = dlg_default_button();
    WINDOW *dialog = 0;
    int result = DLG_EXIT_UNKNOWN;
    char *prompt;
    const char **buttons = dlg_yes_labels();
    int min_width = 25;
    bool show = TRUE;
    int page, last = 0, offset = 0;

#ifdef KEY_RESIZE
    int req_high = height;
    int req_wide = width;
#endif

    DLG_TRACE(("# yesno args:\n"));
    DLG_TRACE2S("title", title);
    DLG_TRACE2S("message", cprompt);
    DLG_TRACE2N("height", height);
    DLG_TRACE2N("width", width);

#ifdef KEY_RESIZE
  restart:
#endif
    prompt = dlg_strclone(cprompt);
    dlg_tab_correct_str(prompt);
    dlg_button_layout(buttons, &min_width);
    dlg_auto_size(title, prompt, &height, &width, 2, min_width);
    dlg_print_size(height, width);
    dlg_ctl_size(height, width);

    x = dlg_box_x_ordinate(width);
    y = dlg_box_y_ordinate(height);

    dialog = dlg_new_window(height, width, y, x);
    dlg_register_window(dialog, "yesno", binding);
    dlg_register_buttons(dialog, "yesno", buttons);

    dlg_draw_box2(dialog, 0, 0, height, width, dialog_attr, border_attr, border2_attr);
    dlg_draw_bottom_box2(dialog, border_attr, border2_attr, dialog_attr);
    dlg_draw_title(dialog, title);
    dlg_draw_helpline(dialog, FALSE);

    dlg_attrset(dialog, dialog_attr);

    page = height - (1 + 3 * MARGIN);
    dlg_draw_buttons(dialog,
		     height - 2 * MARGIN, 0,
		     buttons, button, FALSE, width);

    while (result == DLG_EXIT_UNKNOWN) {
	int code;

	if (show) {
	    last = dlg_print_scrolled(dialog, prompt, offset,
				      page, width, TRUE);
	    dlg_trace_win(dialog);
	    show = FALSE;
	}
	key = dlg_mouse_wgetch(dialog, &fkey);
	if (dlg_result_key(key, fkey, &result)) {
	    if (!dlg_button_key(result, &button, &key, &fkey))
		break;
	}
	if ((code = dlg_char_to_button(key, buttons)) >= 0) {
	    result = dlg_ok_buttoncode(code);
	    break;
	}
	/* handle function keys */
	if (fkey) {
	    switch (key) {
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
	    case DLGK_LEAVE:
		result = dlg_yes_buttoncode(button);
		break;
#ifdef KEY_RESIZE
	    case KEY_RESIZE:
		dlg_will_resize(dialog);
		height = req_high;
		width = req_wide;
		show = TRUE;
		free(prompt);
		_dlg_resize_cleanup(dialog);
		goto restart;
#endif
	    default:
		if (is_DLGK_MOUSE(key)) {
		    result = dlg_yes_buttoncode(key - M_EVENT);
		    if (result < 0)
			result = DLG_EXIT_OK;
		} else if (dlg_check_scrolled(key, last, page,
					      &show, &offset) != 0) {
		    beep();
		}
		break;
	    }
	} else if (key > 0) {
	    beep();
	}
    }
    dlg_add_last_key(-1);

    dlg_del_window(dialog);
    dlg_mouse_free_regions();
    free(prompt);
    return result;
}
