/*
 * $Id: calendar.c,v 1.59 2010/01/18 09:50:44 tom Exp $
 *
 *  calendar.c -- implements the calendar box
 *
 *  Copyright 2001-2009,2010	Thomas E. Dickey
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

#include <time.h>

#define ONE_DAY  (60 * 60 * 24)

#define MON_WIDE 4		/* width of a month-name */
#define DAY_HIGH 6		/* maximum lines in day-grid */
#define DAY_WIDE (8 * MON_WIDE)	/* width of the day-grid */
#define HDR_HIGH 1		/* height of cells with month/year */
#define BTN_HIGH 1		/* height of button-row excluding margin */

/* two more lines: titles for day-of-week and month/year boxes */
#define MIN_HIGH (DAY_HIGH + 2 + HDR_HIGH + BTN_HIGH + (7 * MARGIN))
#define MIN_WIDE (DAY_WIDE + (4 * MARGIN))

typedef enum {
    sMONTH = -3
    ,sYEAR = -2
    ,sDAY = -1
} STATES;

struct _box;

typedef int (*BOX_DRAW) (struct _box *, struct tm *);

typedef struct _box {
    WINDOW *parent;
    WINDOW *window;
    int x;
    int y;
    int width;
    int height;
    BOX_DRAW box_draw;
} BOX;

static const char *
nameOfDayOfWeek(int n)
{
    static const char *table[7]
#ifndef ENABLE_NLS
    =
    {
	"Sunday",
	"Monday",
	"Tuesday",
	"Wednesday",
	"Thursday",
	"Friday",
	"Saturday"
    }
#endif
     ;
    const char *result = 0;

    if (n >= 0 && n < 7) {
#ifdef ENABLE_NLS
	if (table[n] == 0) {
	    nl_item items[7] =
	    {
		ABDAY_1, ABDAY_2, ABDAY_3, ABDAY_4, ABDAY_5, ABDAY_6, ABDAY_7
	    };
	    table[n] = nl_langinfo(items[n]);
	}
#endif
	result = table[n];
    }
    if (result == 0) {
	result = "?";
    }
    return result;
}

static const char *
nameOfMonth(int n)
{
    static const char *table[12]
#ifndef ENABLE_NLS
    =
    {
	"January",
	"February",
	"March",
	"April",
	"May",
	"June",
	"July",
	"August",
	"September",
	"October",
	"November",
	"December"
    }
#endif
     ;
    const char *result = 0;

    if (n >= 0 && n < 12) {
#ifdef ENABLE_NLS
	if (table[n] == 0) {
	    nl_item items[12] =
	    {
		MON_1, MON_2, MON_3, MON_4, MON_5, MON_6,
		MON_7, MON_8, MON_9, MON_10, MON_11, MON_12
	    };
	    table[n] = nl_langinfo(items[n]);
	}
#endif
	result = table[n];
    }
    if (result == 0) {
	result = "?";
    }
    return result;
}

static int
days_in_month(struct tm *current, int offset /* -1, 0, 1 */ )
{
    static const int nominal[] =
    {
	31, 28, 31, 30, 31, 30,
	31, 31, 30, 31, 30, 31
    };
    int year = current->tm_year;
    int month = current->tm_mon + offset;
    int result;

    while (month < 0) {
	month += 12;
	year -= 1;
    }
    while (month >= 12) {
	month -= 12;
	year += 1;
    }
    result = nominal[month];
    if (month == 1)
	result += ((year % 4) == 0);
    return result;
}

static int
days_in_year(struct tm *current, int offset /* -1, 0, 1 */ )
{
    int year = current->tm_year + 1900 + offset;

    return ((year % 4) == 0) ? 366 : 365;
}

static int
day_cell_number(struct tm *current)
{
    int cell;
    cell = current->tm_mday - ((6 + current->tm_mday - current->tm_wday) % 7);
    if ((current->tm_mday - 1) % 7 != current->tm_wday)
	cell += 6;
    else
	cell--;
    return cell;
}

static int
next_or_previous(int key, int two_d)
{
    int result = 0;

    switch (key) {
    case DLGK_GRID_UP:
	result = two_d ? -7 : -1;
	break;
    case DLGK_GRID_LEFT:
	result = -1;
	break;
    case DLGK_GRID_DOWN:
	result = two_d ? 7 : 1;
	break;
    case DLGK_GRID_RIGHT:
	result = 1;
	break;
    default:
	beep();
	break;
    }
    return result;
}

/*
 * Draw the day-of-month selection box
 */
static int
draw_day(BOX * data, struct tm *current)
{
    int cell_wide = MON_WIDE;
    int y, x, this_x = 0;
    int save_y = 0, save_x = 0;
    int day = current->tm_mday;
    int mday;
    int week;
    int last = days_in_month(current, 0);
    int prev = days_in_month(current, -1);

    werase(data->window);
    dlg_draw_box(data->parent,
		 data->y - MARGIN, data->x - MARGIN,
		 data->height + (2 * MARGIN), data->width + (2 * MARGIN),
		 menubox_border_attr, menubox_attr);	/* border of daybox */

    wattrset(data->window, menubox_attr);	/* daynames headline */
    for (x = 0; x < 7; x++) {
	mvwprintw(data->window,
		  0, (x + 1) * cell_wide, "%*.*s ",
		  cell_wide - 1,
		  cell_wide - 1,
		  nameOfDayOfWeek(x));
    }

    mday = ((6 + current->tm_mday - current->tm_wday) % 7) - 7;
    if (mday <= -7)
	mday += 7;
    /* mday is now in the range -6 to 0. */
    week = (current->tm_yday + 6 + mday - current->tm_mday) / 7;

    for (y = 1; mday < last; y++) {
	wattrset(data->window, menubox_attr);	/* weeknumbers headline */
	mvwprintw(data->window,
		  y, 0,
		  "%*d ",
		  cell_wide - 1,
		  ++week);
	for (x = 0; x < 7; x++) {
	    this_x = 1 + (x + 1) * cell_wide;
	    ++mday;
	    if (wmove(data->window, y, this_x) == ERR)
		continue;
	    wattrset(data->window, item_attr);	/* not selected days */
	    if (mday == day) {
		wattrset(data->window, item_selected_attr);	/* selected day */
		save_y = y;
		save_x = this_x;
	    }
	    if (mday > 0) {
		if (mday <= last) {
		    wprintw(data->window, "%*d", cell_wide - 2, mday);
		} else if (mday == day) {
		    wprintw(data->window, "%*d", cell_wide - 2, mday - last);
		}
	    } else if (mday == day) {
		wprintw(data->window, "%*d", cell_wide - 2, mday + prev);
	    }
	}
	wmove(data->window, save_y, save_x);
    }
    /* just draw arrows - scrollbar is unsuitable here */
    dlg_draw_arrows(data->parent, TRUE, TRUE,
		    data->x + ARROWS_COL,
		    data->y - 1,
		    data->y + data->height);

    return 0;
}

/*
 * Draw the month-of-year selection box
 */
static int
draw_month(BOX * data, struct tm *current)
{
    int month;

    month = current->tm_mon + 1;

    wattrset(data->parent, dialog_attr);	/* Headline "Month" */
    (void) mvwprintw(data->parent, data->y - 2, data->x - 1, _("Month"));
    dlg_draw_box(data->parent,
		 data->y - 1, data->x - 1,
		 data->height + 2, data->width + 2,
		 menubox_border_attr, menubox_attr);	/* borders of monthbox */
    wattrset(data->window, item_attr);	/* color the month selection */
    mvwprintw(data->window, 0, 0, "%s", nameOfMonth(month - 1));
    wmove(data->window, 0, 0);
    return 0;
}

/*
 * Draw the year selection box
 */
static int
draw_year(BOX * data, struct tm *current)
{
    int year = current->tm_year + 1900;

    wattrset(data->parent, dialog_attr);	/* Headline "Year" */
    (void) mvwprintw(data->parent, data->y - 2, data->x - 1, _("Year"));
    dlg_draw_box(data->parent,
		 data->y - 1, data->x - 1,
		 data->height + 2, data->width + 2,
		 menubox_border_attr, menubox_attr);	/* borders of yearbox */
    wattrset(data->window, item_attr);	/* color the year selection */
    mvwprintw(data->window, 0, 0, "%4d", year);
    wmove(data->window, 0, 0);
    return 0;
}

static int
init_object(BOX * data,
	    WINDOW *parent,
	    int x, int y,
	    int width, int height,
	    BOX_DRAW box_draw,
	    int code)
{
    data->parent = parent;
    data->x = x;
    data->y = y;
    data->width = width;
    data->height = height;
    data->box_draw = box_draw;

    data->window = derwin(data->parent,
			  data->height, data->width,
			  data->y, data->x);
    if (data->window == 0)
	return -1;
    (void) keypad(data->window, TRUE);

    dlg_mouse_setbase(getbegx(parent), getbegy(parent));
    if (code == 'D') {
	dlg_mouse_mkbigregion(y + 1, x + MON_WIDE, height - 1, width - MON_WIDE,
			      KEY_MAX, 1, MON_WIDE, 3);
    } else {
	dlg_mouse_mkregion(y, x, height, width, code);
    }

    return 0;
}

static int
CleanupResult(int code, WINDOW *dialog, char *prompt, DIALOG_VARS * save_vars)
{
    if (dialog != 0)
	dlg_del_window(dialog);
    dlg_mouse_free_regions();
    if (prompt != 0)
	free(prompt);
    dlg_restore_vars(save_vars);

    return code;
}

#define DrawObject(data) (data)->box_draw(data, &current)

/*
 * Display a dialog box for entering a date
 */
int
dialog_calendar(const char *title,
		const char *subtitle,
		int height,
		int width,
		int day,
		int month,
		int year)
{
    /* *INDENT-OFF* */
    static DLG_KEYS_BINDING binding[] = {
	ENTERKEY_BINDINGS,
	DLG_KEYS_DATA( DLGK_ENTER,	' ' ),
	DLG_KEYS_DATA( DLGK_FIELD_NEXT, TAB ),
	DLG_KEYS_DATA( DLGK_FIELD_PREV, KEY_BTAB ),
	DLG_KEYS_DATA( DLGK_GRID_DOWN,	'j' ),
	DLG_KEYS_DATA( DLGK_GRID_DOWN,	DLGK_MOUSE(KEY_NPAGE) ),
	DLG_KEYS_DATA( DLGK_GRID_DOWN,	KEY_DOWN ),
	DLG_KEYS_DATA( DLGK_GRID_DOWN,	KEY_NPAGE ),
	DLG_KEYS_DATA( DLGK_GRID_LEFT,	'-' ),
	DLG_KEYS_DATA( DLGK_GRID_LEFT,  'h' ),
	DLG_KEYS_DATA( DLGK_GRID_LEFT,  CHR_BACKSPACE ),
	DLG_KEYS_DATA( DLGK_GRID_LEFT,  CHR_PREVIOUS ),
	DLG_KEYS_DATA( DLGK_GRID_LEFT,  KEY_LEFT ),
	DLG_KEYS_DATA( DLGK_GRID_RIGHT,	'+' ),
	DLG_KEYS_DATA( DLGK_GRID_RIGHT, 'l' ),
	DLG_KEYS_DATA( DLGK_GRID_RIGHT, CHR_NEXT ),
	DLG_KEYS_DATA( DLGK_GRID_RIGHT, KEY_NEXT ),
	DLG_KEYS_DATA( DLGK_GRID_RIGHT, KEY_RIGHT ),
	DLG_KEYS_DATA( DLGK_GRID_UP,	'k' ),
	DLG_KEYS_DATA( DLGK_GRID_UP,	KEY_PPAGE ),
	DLG_KEYS_DATA( DLGK_GRID_UP,	KEY_PREVIOUS ),
	DLG_KEYS_DATA( DLGK_GRID_UP,	KEY_UP ),
	DLG_KEYS_DATA( DLGK_GRID_UP,  	DLGK_MOUSE(KEY_PPAGE) ),
	END_KEYS_BINDING
    };
    /* *INDENT-ON* */

#ifdef KEY_RESIZE
    int old_height = height;
    int old_width = width;
#endif
    BOX dy_box, mn_box, yr_box;
    int fkey;
    int key = 0;
    int key2;
    int step;
    int button;
    int result = DLG_EXIT_UNKNOWN;
    WINDOW *dialog;
    time_t now_time = time((time_t *) 0);
    struct tm current;
    int state = dlg_defaultno_button();
    const char **buttons = dlg_ok_labels();
    char *prompt = dlg_strclone(subtitle);
    int mincols = MIN_WIDE;
    char buffer[MAX_LEN];
    DIALOG_VARS save_vars;

    dlg_save_vars(&save_vars);
    dialog_vars.separate_output = TRUE;

    dlg_does_output();

    now_time = time((time_t *) 0);
    current = *localtime(&now_time);
    if (day < 0)
	day = current.tm_mday;
    if (month < 0)
	month = current.tm_mon + 1;
    if (year < 0)
	year = current.tm_year + 1900;

    /* compute a struct tm that matches the day/month/year parameters */
    if (((year -= 1900) > 0) && (year < 200)) {
	/* ugly, but I'd like to run this on older machines w/o mktime -TD */
	for (;;) {
	    if (year > current.tm_year) {
		now_time += ONE_DAY * days_in_year(&current, 0);
	    } else if (year < current.tm_year) {
		now_time -= ONE_DAY * days_in_year(&current, -1);
	    } else if (month > current.tm_mon + 1) {
		now_time += ONE_DAY * days_in_month(&current, 0);
	    } else if (month < current.tm_mon + 1) {
		now_time -= ONE_DAY * days_in_month(&current, -1);
	    } else if (day > current.tm_mday) {
		now_time += ONE_DAY;
	    } else if (day < current.tm_mday) {
		now_time -= ONE_DAY;
	    } else {
		break;
	    }
	    current = *localtime(&now_time);
	}
    }
    dlg_button_layout(buttons, &mincols);

#ifdef KEY_RESIZE
  retry:
#endif

    dlg_auto_size(title, prompt, &height, &width, 0, mincols);
    height += MIN_HIGH - 1;
    dlg_print_size(height, width);
    dlg_ctl_size(height, width);

    dialog = dlg_new_window(height, width,
			    dlg_box_y_ordinate(height),
			    dlg_box_x_ordinate(width));
    dlg_register_window(dialog, "calendar", binding);
    dlg_register_buttons(dialog, "calendar", buttons);

    /* mainbox */
    dlg_draw_box(dialog, 0, 0, height, width, dialog_attr, border_attr);
    dlg_draw_bottom_box(dialog);
    dlg_draw_title(dialog, title);

    wattrset(dialog, dialog_attr);	/* text mainbox */
    dlg_print_autowrap(dialog, prompt, height, width);

    /* compute positions of day, month and year boxes */
    memset(&dy_box, 0, sizeof(dy_box));
    memset(&mn_box, 0, sizeof(mn_box));
    memset(&yr_box, 0, sizeof(yr_box));

    if (init_object(&dy_box,
		    dialog,
		    (width - DAY_WIDE) / 2,
		    1 + (height - (DAY_HIGH + BTN_HIGH + (5 * MARGIN))),
		    DAY_WIDE,
		    DAY_HIGH + 1,
		    draw_day,
		    'D') < 0
	|| DrawObject(&dy_box) < 0) {
	return CleanupResult(DLG_EXIT_ERROR, dialog, prompt, &save_vars);
    }

    if (init_object(&mn_box,
		    dialog,
		    dy_box.x,
		    dy_box.y - (HDR_HIGH + 2 * MARGIN),
		    (DAY_WIDE / 2) - MARGIN,
		    HDR_HIGH,
		    draw_month,
		    'M') < 0
	|| DrawObject(&mn_box) < 0) {
	return CleanupResult(DLG_EXIT_ERROR, dialog, prompt, &save_vars);
    }

    if (init_object(&yr_box,
		    dialog,
		    dy_box.x + mn_box.width + 2,
		    mn_box.y,
		    mn_box.width,
		    mn_box.height,
		    draw_year,
		    'Y') < 0
	|| DrawObject(&yr_box) < 0) {
	return CleanupResult(DLG_EXIT_ERROR, dialog, prompt, &save_vars);
    }

    while (result == DLG_EXIT_UNKNOWN) {
	BOX *obj = (state == sDAY ? &dy_box
		    : (state == sMONTH ? &mn_box :
		       (state == sYEAR ? &yr_box : 0)));

	button = (state < 0) ? 0 : state;
	dlg_draw_buttons(dialog, height - 2, 0, buttons, button, FALSE, width);
	if (obj != 0)
	    dlg_set_focus(dialog, obj->window);

	key = dlg_mouse_wgetch(dialog, &fkey);
	if (dlg_result_key(key, fkey, &result))
	    break;

	if (fkey && (key >= DLGK_MOUSE(KEY_MIN) && key <= DLGK_MOUSE(KEY_MAX))) {
	    key = dlg_lookup_key(dialog, key - M_EVENT, &fkey);
	}

	if ((key2 = dlg_char_to_button(key, buttons)) >= 0) {
	    result = key2;
	} else if (fkey) {
	    /* handle function-keys */
	    switch (key) {
	    case DLGK_MOUSE('D'):
		state = sDAY;
		break;
	    case DLGK_MOUSE('M'):
		state = sMONTH;
		break;
	    case DLGK_MOUSE('Y'):
		state = sYEAR;
		break;
	    case DLGK_ENTER:
		result = dlg_ok_buttoncode(button);
		break;
	    case DLGK_FIELD_PREV:
		state = dlg_prev_ok_buttonindex(state, sMONTH);
		break;
	    case DLGK_FIELD_NEXT:
		state = dlg_next_ok_buttonindex(state, sMONTH);
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
		step = 0;
		key2 = -1;
		if (is_DLGK_MOUSE(key)) {
		    if ((key2 = dlg_ok_buttoncode(key - M_EVENT)) >= 0) {
			result = key2;
			break;
		    } else if (key >= DLGK_MOUSE(KEY_MAX)) {
			state = sDAY;
			obj = &dy_box;
			key2 = 1;
			step = (key
				- DLGK_MOUSE(KEY_MAX)
				- day_cell_number(&current));
		    }
		}
		if (obj != 0) {
		    if (key2 < 0)
			step = next_or_previous(key, (obj == &dy_box));
		    if (step != 0) {
			struct tm old = current;

			/* see comment regarding mktime -TD */
			if (obj == &dy_box) {
			    now_time += ONE_DAY * step;
			} else if (obj == &mn_box) {
			    if (step > 0)
				now_time += ONE_DAY *
				    days_in_month(&current, 0);
			    else
				now_time -= ONE_DAY *
				    days_in_month(&current, -1);
			} else if (obj == &yr_box) {
			    if (step > 0)
				now_time += (ONE_DAY
					     * days_in_year(&current, 0));
			    else
				now_time -= (ONE_DAY
					     * days_in_year(&current, -1));
			}

			current = *localtime(&now_time);

			if (obj != &dy_box
			    && (current.tm_mday != old.tm_mday
				|| current.tm_mon != old.tm_mon
				|| current.tm_year != old.tm_year))
			    DrawObject(&dy_box);
			if (obj != &mn_box && current.tm_mon != old.tm_mon)
			    DrawObject(&mn_box);
			if (obj != &yr_box && current.tm_year != old.tm_year)
			    DrawObject(&yr_box);
			(void) DrawObject(obj);
		    }
		} else if (state >= 0) {
		    if (next_or_previous(key, FALSE) < 0)
			state = dlg_prev_ok_buttonindex(state, sMONTH);
		    else if (next_or_previous(key, FALSE) > 0)
			state = dlg_next_ok_buttonindex(state, sMONTH);
		}
		break;
	    }
	}
    }

#define DefaultFormat(dst, src) \
	sprintf(dst, "%02d/%02d/%0d", \
		src.tm_mday, src.tm_mon + 1, src.tm_year + 1900)
#ifdef HAVE_STRFTIME
    if (dialog_vars.date_format != 0) {
       size_t used = strftime(buffer,
			      sizeof(buffer) - 1,
			      dialog_vars.date_format,
			      &current);
	if (used == 0 || *buffer == '\0')
	    DefaultFormat(buffer, current);
    } else
#endif
	DefaultFormat(buffer, current);

    dlg_add_result(buffer);
    dlg_add_separator();

    return CleanupResult(result, dialog, prompt, &save_vars);
}
