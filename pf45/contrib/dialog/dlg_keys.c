/*
 *  $Id: dlg_keys.c,v 1.26 2009/02/22 16:19:51 tom Exp $
 *
 * dlg_keys.c -- runtime binding support for dialog
 *
 * Copyright 2006-2007,2009 Thomas E. Dickey
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

#define LIST_BINDINGS struct _list_bindings

LIST_BINDINGS {
    LIST_BINDINGS *link;
    WINDOW *win;		/* window on which widget gets input */
    const char *name;		/* widget name */
    bool buttons;		/* true only for dlg_register_buttons() */
    DLG_KEYS_BINDING *binding;	/* list of bindings */
};

static LIST_BINDINGS *all_bindings;
static const DLG_KEYS_BINDING end_keys_binding = END_KEYS_BINDING;

/*
 * For a given named widget's window, associate a binding table.
 */
void
dlg_register_window(WINDOW *win, const char *name, DLG_KEYS_BINDING * binding)
{
    LIST_BINDINGS *p, *q;

    for (p = all_bindings, q = 0; p != 0; q = p, p = p->link) {
	if (p->win == win && !strcmp(p->name, name)) {
	    p->binding = binding;
	    return;
	}
    }
    /* add built-in bindings at the end of the list (see compare_bindings). */
    if ((p = dlg_calloc(LIST_BINDINGS, 1)) != 0) {
	p->win = win;
	p->name = name;
	p->binding = binding;
	if (q != 0)
	    q->link = p;
	else
	    all_bindings = p;
    }
}

/*
 * Unlike dlg_lookup_key(), this looks for either widget-builtin or rc-file
 * definitions, depending on whether 'win' is null.
 */
static int
key_is_bound(WINDOW *win, const char *name, int curses_key, int function_key)
{
    LIST_BINDINGS *p;

    for (p = all_bindings; p != 0; p = p->link) {
	if (p->win == win && !dlg_strcmp(p->name, name)) {
	    int n;
	    for (n = 0; p->binding[n].is_function_key >= 0; ++n) {
		if (p->binding[n].curses_key == curses_key
		    && p->binding[n].is_function_key == function_key) {
		    return TRUE;
		}
	    }
	}
    }
    return FALSE;
}

/*
 * Call this function after dlg_register_window(), for the list of button
 * labels associated with the widget.
 *
 * Ensure that dlg_lookup_key() will not accidentally translate a key that
 * we would like to use for a button abbreviation to some other key, e.g.,
 * h/j/k/l for navigation into a cursor key.  Do this by binding the key
 * to itself.
 *
 * See dlg_char_to_button().
 */
void
dlg_register_buttons(WINDOW *win, const char *name, const char **buttons)
{
    int n;
    LIST_BINDINGS *p;
    DLG_KEYS_BINDING *q;

    if (buttons == 0)
	return;

    for (n = 0; buttons[n] != 0; ++n) {
	int curses_key = dlg_button_to_char(buttons[n]);

	/* ignore multibyte characters */
	if (curses_key >= KEY_MIN)
	    continue;

	/* if it is not bound in the widget, skip it (no conflicts) */
	if (!key_is_bound(win, name, curses_key, FALSE))
	    continue;

#ifdef HAVE_RC_FILE
	/* if it is bound in the rc-file, skip it */
	if (key_is_bound(0, name, curses_key, FALSE))
	    continue;
#endif

	if ((p = dlg_calloc(LIST_BINDINGS, 1)) != 0) {
	    if ((q = dlg_calloc(DLG_KEYS_BINDING, 2)) != 0) {
		q[0].is_function_key = 0;
		q[0].curses_key = curses_key;
		q[0].dialog_key = curses_key;
		q[1] = end_keys_binding;

		p->win = win;
		p->name = name;
		p->buttons = TRUE;
		p->binding = q;

		/* put these at the beginning, to override the widget's table */
		p->link = all_bindings;
		all_bindings = p;
	    } else {
		free(p);
	    }
	}
    }
}

/*
 * Remove the bindings for a given window.
 */
void
dlg_unregister_window(WINDOW *win)
{
    LIST_BINDINGS *p, *q;

    for (p = all_bindings, q = 0; p != 0; p = p->link) {
	if (p->win == win) {
	    if (q != 0) {
		q->link = p->link;
	    } else {
		all_bindings = p->link;
	    }
	    /* the user-defined and buttons-bindings all are length=1 */
	    if (p->binding[1].is_function_key < 0)
		free(p->binding);
	    free(p);
	    dlg_unregister_window(win);
	    break;
	}
	q = p;
    }
}

/*
 * Call this after wgetch(), using the same window pointer and passing
 * the curses-key.
 *
 * If there is no binding associated with the widget, it simply returns
 * the given curses-key.
 *
 * Parameters:
 *	win is the window on which the wgetch() was done.
 *	curses_key is the value returned by wgetch().
 *	fkey in/out (on input, it is true if curses_key is a function key,
 *		and on output, it is true if the result is a function key).
 */
int
dlg_lookup_key(WINDOW *win, int curses_key, int *fkey)
{
    LIST_BINDINGS *p;
    int n;

    /*
     * Ignore mouse clicks, since they are already encoded properly.
     */
#ifdef KEY_MOUSE
    if (*fkey != 0 && curses_key == KEY_MOUSE) {
	;
    } else
#endif
	/*
	 * Ignore resize events, since they are already encoded properly.
	 */
#ifdef KEY_RESIZE
    if (*fkey != 0 && curses_key == KEY_RESIZE) {
	;
    } else
#endif
    if (*fkey == 0 || curses_key < KEY_MAX) {
	for (p = all_bindings; p != 0; p = p->link) {
	    if (p->win == win || p->win == 0) {
		int function_key = (*fkey != 0);
		for (n = 0; p->binding[n].is_function_key >= 0; ++n) {
		    if (p->buttons
			&& !function_key
			&& p->binding[n].curses_key == (int) dlg_toupper(curses_key)) {
			*fkey = 0;
			return p->binding[n].dialog_key;
		    }
		    if (p->binding[n].curses_key == curses_key
			&& p->binding[n].is_function_key == function_key) {
			*fkey = p->binding[n].dialog_key;
			return *fkey;
		    }
		}
	    }
	}
    }
    return curses_key;
}

/*
 * Test a dialog internal keycode to see if it corresponds to one of the push
 * buttons on the widget such as "OK".
 *
 * This is only useful if there are user-defined key bindings, since there are
 * no built-in bindings that map directly to DLGK_OK, etc.
 *
 * See also dlg_ok_buttoncode().
 */
int
dlg_result_key(int dialog_key, int fkey GCC_UNUSED, int *resultp)
{
    int done = FALSE;

#ifdef HAVE_RC_FILE
    if (fkey) {
	switch ((DLG_KEYS_ENUM) dialog_key) {
	case DLGK_OK:
	    *resultp = DLG_EXIT_OK;
	    done = TRUE;
	    break;
	case DLGK_CANCEL:
	    if (!dialog_vars.nocancel) {
		*resultp = DLG_EXIT_CANCEL;
		done = TRUE;
	    }
	    break;
	case DLGK_EXTRA:
	    if (dialog_vars.extra_button) {
		*resultp = DLG_EXIT_EXTRA;
		done = TRUE;
	    }
	    break;
	case DLGK_HELP:
	    if (dialog_vars.help_button) {
		*resultp = DLG_EXIT_HELP;
		done = TRUE;
	    }
	    break;
	case DLGK_ESC:
	    *resultp = DLG_EXIT_ESC;
	    done = TRUE;
	    break;
	default:
	    break;
	}
    } else
#endif
    if (dialog_key == ESC) {
	*resultp = DLG_EXIT_ESC;
	done = TRUE;
    } else if (dialog_key == ERR) {
	*resultp = DLG_EXIT_ERROR;
	done = TRUE;
    }

    return done;
}

#ifdef HAVE_RC_FILE
typedef struct {
    const char *name;
    int code;
} CODENAME;

#define CURSES_NAME(upper) { #upper, KEY_ ## upper }
#define COUNT_CURSES  sizeof(curses_names)/sizeof(curses_names[0])
static const CODENAME curses_names[] =
{
    CURSES_NAME(DOWN),
    CURSES_NAME(UP),
    CURSES_NAME(LEFT),
    CURSES_NAME(RIGHT),
    CURSES_NAME(HOME),
    CURSES_NAME(BACKSPACE),
    CURSES_NAME(F0),
    CURSES_NAME(DL),
    CURSES_NAME(IL),
    CURSES_NAME(DC),
    CURSES_NAME(IC),
    CURSES_NAME(EIC),
    CURSES_NAME(CLEAR),
    CURSES_NAME(EOS),
    CURSES_NAME(EOL),
    CURSES_NAME(SF),
    CURSES_NAME(SR),
    CURSES_NAME(NPAGE),
    CURSES_NAME(PPAGE),
    CURSES_NAME(STAB),
    CURSES_NAME(CTAB),
    CURSES_NAME(CATAB),
    CURSES_NAME(ENTER),
    CURSES_NAME(PRINT),
    CURSES_NAME(LL),
    CURSES_NAME(A1),
    CURSES_NAME(A3),
    CURSES_NAME(B2),
    CURSES_NAME(C1),
    CURSES_NAME(C3),
    CURSES_NAME(BTAB),
    CURSES_NAME(BEG),
    CURSES_NAME(CANCEL),
    CURSES_NAME(CLOSE),
    CURSES_NAME(COMMAND),
    CURSES_NAME(COPY),
    CURSES_NAME(CREATE),
    CURSES_NAME(END),
    CURSES_NAME(EXIT),
    CURSES_NAME(FIND),
    CURSES_NAME(HELP),
    CURSES_NAME(MARK),
    CURSES_NAME(MESSAGE),
    CURSES_NAME(MOVE),
    CURSES_NAME(NEXT),
    CURSES_NAME(OPEN),
    CURSES_NAME(OPTIONS),
    CURSES_NAME(PREVIOUS),
    CURSES_NAME(REDO),
    CURSES_NAME(REFERENCE),
    CURSES_NAME(REFRESH),
    CURSES_NAME(REPLACE),
    CURSES_NAME(RESTART),
    CURSES_NAME(RESUME),
    CURSES_NAME(SAVE),
    CURSES_NAME(SBEG),
    CURSES_NAME(SCANCEL),
    CURSES_NAME(SCOMMAND),
    CURSES_NAME(SCOPY),
    CURSES_NAME(SCREATE),
    CURSES_NAME(SDC),
    CURSES_NAME(SDL),
    CURSES_NAME(SELECT),
    CURSES_NAME(SEND),
    CURSES_NAME(SEOL),
    CURSES_NAME(SEXIT),
    CURSES_NAME(SFIND),
    CURSES_NAME(SHELP),
    CURSES_NAME(SHOME),
    CURSES_NAME(SIC),
    CURSES_NAME(SLEFT),
    CURSES_NAME(SMESSAGE),
    CURSES_NAME(SMOVE),
    CURSES_NAME(SNEXT),
    CURSES_NAME(SOPTIONS),
    CURSES_NAME(SPREVIOUS),
    CURSES_NAME(SPRINT),
    CURSES_NAME(SREDO),
    CURSES_NAME(SREPLACE),
    CURSES_NAME(SRIGHT),
    CURSES_NAME(SRSUME),
    CURSES_NAME(SSAVE),
    CURSES_NAME(SSUSPEND),
    CURSES_NAME(SUNDO),
    CURSES_NAME(SUSPEND),
    CURSES_NAME(UNDO),
};

#define DIALOG_NAME(upper) { #upper, DLGK_ ## upper }
#define COUNT_DIALOG  sizeof(dialog_names)/sizeof(dialog_names[0])
static const CODENAME dialog_names[] =
{
    DIALOG_NAME(OK),
    DIALOG_NAME(CANCEL),
    DIALOG_NAME(EXTRA),
    DIALOG_NAME(HELP),
    DIALOG_NAME(ESC),
    DIALOG_NAME(PAGE_FIRST),
    DIALOG_NAME(PAGE_LAST),
    DIALOG_NAME(PAGE_NEXT),
    DIALOG_NAME(PAGE_PREV),
    DIALOG_NAME(ITEM_FIRST),
    DIALOG_NAME(ITEM_LAST),
    DIALOG_NAME(ITEM_NEXT),
    DIALOG_NAME(ITEM_PREV),
    DIALOG_NAME(FIELD_FIRST),
    DIALOG_NAME(FIELD_LAST),
    DIALOG_NAME(FIELD_NEXT),
    DIALOG_NAME(FIELD_PREV),
    DIALOG_NAME(GRID_UP),
    DIALOG_NAME(GRID_DOWN),
    DIALOG_NAME(GRID_LEFT),
    DIALOG_NAME(GRID_RIGHT),
    DIALOG_NAME(DELETE_LEFT),
    DIALOG_NAME(DELETE_RIGHT),
    DIALOG_NAME(DELETE_ALL),
    DIALOG_NAME(ENTER),
    DIALOG_NAME(BEGIN),
    DIALOG_NAME(FINAL),
    DIALOG_NAME(SELECT)
};

static char *
skip_white(char *s)
{
    while (*s != '\0' && isspace(UCH(*s)))
	++s;
    return s;
}

static char *
skip_black(char *s)
{
    while (*s != '\0' && !isspace(UCH(*s)))
	++s;
    return s;
}

/*
 * Find a user-defined binding, given the curses key code.
 */
static DLG_KEYS_BINDING *
find_binding(char *widget, int curses_key)
{
    LIST_BINDINGS *p;
    DLG_KEYS_BINDING *result = 0;

    for (p = all_bindings; p != 0; p = p->link) {
	if (p->win == 0
	    && !dlg_strcmp(p->name, widget)
	    && p->binding->curses_key == curses_key) {
	    result = p->binding;
	    break;
	}
    }
    return result;
}

/*
 * Built-in bindings have a nonzero "win" member, and the associated binding
 * table can have more than one entry.  We keep those last, since lookups will
 * find the user-defined bindings first and use those.
 *
 * Sort "*" (all-widgets) entries past named widgets, since those are less
 * specific.
 */
static int
compare_bindings(LIST_BINDINGS * a, LIST_BINDINGS * b)
{
    int result = 0;
    if (a->win == b->win) {
	if (!strcmp(a->name, b->name)) {
	    result = a->binding[0].curses_key - b->binding[0].curses_key;
	} else if (!strcmp(b->name, "*")) {
	    result = -1;
	} else if (!strcmp(a->name, "*")) {
	    result = 1;
	} else {
	    result = dlg_strcmp(a->name, b->name);
	}
    } else if (b->win) {
	result = -1;
    } else {
	result = 1;
    }
    return result;
}

/*
 * Find a user-defined binding, given the curses key code.  If it does not
 * exist, create a new one, inserting it into the linked list, keeping it
 * sorted to simplify lookups for user-defined bindings that can override
 * the built-in bindings.
 */
static DLG_KEYS_BINDING *
make_binding(char *widget, int curses_key, int is_function, int dialog_key)
{
    LIST_BINDINGS *entry = 0;
    DLG_KEYS_BINDING *data = 0;
    char *name;
    LIST_BINDINGS *p, *q;
    DLG_KEYS_BINDING *result = find_binding(widget, curses_key);

    if (result == 0
	&& (entry = dlg_calloc(LIST_BINDINGS, 1)) != 0
	&& (data = dlg_calloc(DLG_KEYS_BINDING, 2)) != 0
	&& (name = dlg_strclone(widget)) != 0) {

	entry->name = name;
	entry->binding = data;

	data[0].is_function_key = is_function;
	data[0].curses_key = curses_key;
	data[0].dialog_key = dialog_key;

	data[1] = end_keys_binding;

	for (p = all_bindings, q = 0; p != 0; q = p, p = p->link) {
	    if (compare_bindings(entry, p) < 0) {
		break;
	    }
	}
	if (q != 0) {
	    q->link = entry;
	} else {
	    all_bindings = entry;
	}
	if (p != 0) {
	    entry->link = p;
	}
	result = data;
    } else if (entry != 0) {
	free(entry);
	if (data)
	    free(data);
    }

    return result;
}

/*
 * Parse the parameters of the "bindkeys" configuration-file entry.  This
 * expects widget name which may be "*", followed by curses key definition and
 * then dialog key definition.
 *
 * The curses key "should" be one of the names (ignoring case) from
 * curses_names[], but may also be a single control character (prefix "^" or
 * "~" depending on whether it is C0 or C1), or an escaped single character.
 * Binding a printable character with dialog is possible but not useful.
 *
 * The dialog key must be one of the names from dialog_names[].
 */
int
dlg_parse_bindkey(char *params)
{
    char *p = skip_white(params);
    char *q;
    bool escaped = FALSE;
    int modified = 0;
    int result = FALSE;
    unsigned xx;
    char *widget;
    int is_function = FALSE;
    int curses_key;
    int dialog_key;

    curses_key = -1;
    dialog_key = -1;
    widget = p;

    p = skip_black(p);
    if (p != widget && *p != '\0') {
	*p++ = '\0';
	q = p;
	while (*p != '\0' && curses_key < 0) {
	    if (escaped) {
		escaped = FALSE;
		curses_key = *p;
	    } else if (*p == '\\') {
		escaped = TRUE;
	    } else if (modified) {
		if (*p == '?') {
		    curses_key = ((modified == '^')
				  ? 127
				  : 255);
		} else {
		    curses_key = ((modified == '^')
				  ? (*p & 0x1f)
				  : ((*p & 0x1f) | 0x80));
		}
	    } else if (*p == '^') {
		modified = *p;
	    } else if (*p == '~') {
		modified = *p;
	    } else if (isspace(UCH(*p))) {
		break;
	    }
	    ++p;
	}
	if (!isspace(UCH(*p))) {
	    ;
	} else {
	    *p++ = '\0';
	    if (curses_key < 0) {
		char fprefix[2];
		char check[2];
		int keynumber;
		if (sscanf(q, "%[Ff]%d%c", fprefix, &keynumber, check) == 2) {
		    curses_key = KEY_F(keynumber);
		    is_function = TRUE;
		} else {
		    for (xx = 0; xx < COUNT_CURSES; ++xx) {
			if (!dlg_strcmp(curses_names[xx].name, q)) {
			    curses_key = curses_names[xx].code;
			    is_function = TRUE;
			    break;
			}
		    }
		}
	    }
	}
	q = skip_white(p);
	p = skip_black(q);
	if (p != q) {
	    for (xx = 0; xx < COUNT_DIALOG; ++xx) {
		if (!dlg_strcmp(dialog_names[xx].name, q)) {
		    dialog_key = dialog_names[xx].code;
		    break;
		}
	    }
	}
	if (*widget != '\0'
	    && curses_key >= 0
	    && dialog_key >= 0
	    && make_binding(widget, curses_key, is_function, dialog_key) != 0) {
	    result = TRUE;
	}
    }
    return result;
}

static void
dump_curses_key(FILE *fp, int curses_key)
{
    if (curses_key > KEY_MIN) {
	unsigned n;
	bool found = FALSE;
	for (n = 0; n < COUNT_CURSES; ++n) {
	    if (curses_names[n].code == curses_key) {
		fprintf(fp, "%s", curses_names[n].name);
		found = TRUE;
		break;
	    }
	}
	if (!found) {
	    if (curses_key >= KEY_F(0)) {
		fprintf(fp, "F%d", curses_key - KEY_F(0));
	    } else {
		fprintf(fp, "curses%d", curses_key);
	    }
	}
    } else if (curses_key >= 0 && curses_key < 32) {
	fprintf(fp, "^%c", curses_key + 64);
    } else if (curses_key == 127) {
	fprintf(fp, "^?");
    } else if (curses_key >= 128 && curses_key < 160) {
	fprintf(fp, "~%c", curses_key - 64);
    } else if (curses_key == 255) {
	fprintf(fp, "~?");
    } else {
	fprintf(fp, "\\%c", curses_key);
    }
}

static void
dump_dialog_key(FILE *fp, int dialog_key)
{
    unsigned n;
    bool found = FALSE;
    for (n = 0; n < COUNT_DIALOG; ++n) {
	if (dialog_names[n].code == dialog_key) {
	    fputs(dialog_names[n].name, fp);
	    found = TRUE;
	    break;
	}
    }
    if (!found) {
	fprintf(fp, "dialog%d", dialog_key);
    }
}

static void
dump_one_binding(FILE *fp, const char *widget, DLG_KEYS_BINDING * binding)
{
    fprintf(fp, "bindkey %s ", widget);
    dump_curses_key(fp, binding->curses_key);
    fputc(' ', fp);
    dump_dialog_key(fp, binding->dialog_key);
    fputc('\n', fp);
}

void
dlg_dump_keys(FILE *fp)
{
    LIST_BINDINGS *p;
    const char *last = "";
    unsigned n;
    unsigned count = 0;

    for (p = all_bindings; p != 0; p = p->link) {
	if (p->win == 0) {
	    ++count;
	}
    }
    if (count != 0) {
	for (p = all_bindings, n = 0; p != 0; p = p->link) {
	    if (p->win == 0) {
		if (dlg_strcmp(last, p->name)) {
		    fprintf(fp, "\n# key bindings for %s widgets\n",
			    !strcmp(p->name, "*") ? "all" : p->name);
		    last = p->name;
		}
		dump_one_binding(fp, p->name, p->binding);
	    }
	}
    }
}
#endif /* HAVE_RC_FILE */
