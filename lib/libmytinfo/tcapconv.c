/*
 * tcapconv.c
 *
 * By Ross Ridge
 * Public Domain
 * 92/02/01 07:30:20
 *
 */

#include "defs.h"
#define SINGLE
#include <term.h>

#ifdef USE_SCCS_IDS
static const char SCCSid[] = "@(#) mytinfo tcapconv.c 3.2 92/02/01 public domain, By Ross Ridge";
#endif

extern char **_sstrcodes[], **_sstrnames[];

static char *C_CR = "\r";
static char *C_LF = "\n";
static char *C_BS = "\b";
/* static char *C_FF = "\f"; */
static char *C_HT = "\t";
/* static char *C_VT = "\013"; */
/* static char *C_NL = "\r\n"; */

#define DEF(s) ((s) != (char *) -1 && (s) != NULL)
#define NOTSET(s) ((s) == (char *) -1)

/*
 * This routine fills in caps that either had defaults under termcap or
 * can be manufactured from obsolete termcap capabilities.
 */

void
_tcapdefault() {
	char buf[MAX_LINE * 2 + 2];
	int set_scroll_forward_to_lf;

	if (NOTSET(carriage_return)) {
		if (carriage_return_delay > 0) {
			sprintf(buf, "%s$<%d>", C_CR, carriage_return_delay);
			carriage_return = _addstr(buf);
		} else
			carriage_return = _addstr(C_CR);
	}
	if (NOTSET(cursor_left)) {
		if (backspace_delay > 0) {
			sprintf(buf, "%s$<%d>", C_BS, backspace_delay);
			cursor_left = _addstr(buf);
		} else if (backspaces_with_bs == 1)
			cursor_left = _addstr(C_BS);
		else if (DEF(backspace_if_not_bs))
			cursor_left = _addstr(backspace_if_not_bs);
	}
/* vi doesn't use "do", but it does seems to use nl (or '\n') instead */
	if (NOTSET(cursor_down)) {
		if (DEF(linefeed_if_not_lf))
			cursor_down = _addstr(linefeed_if_not_lf);
		else if (linefeed_is_newline != 1) {
			if (new_line_delay > 0) {
				sprintf(buf, "%s$<%d>", C_LF, new_line_delay);
				cursor_down = _addstr(buf);
			} else
				cursor_down = _addstr(C_LF);
		}
	}
	set_scroll_forward_to_lf = 0;
	if (NOTSET(scroll_forward) && crt_without_scrolling != 1) {
		set_scroll_forward_to_lf = 1;
		if (DEF(linefeed_if_not_lf))
			scroll_forward = _addstr(linefeed_if_not_lf);
		else if (linefeed_is_newline != 1) {
			if (new_line_delay > 0) {
				sprintf(buf, "%s$<%d>", C_LF, new_line_delay);
				scroll_forward = _addstr(buf);
			} else
				scroll_forward = _addstr(C_LF);
		}
	}
	if (NOTSET(newline)) {
		if (linefeed_is_newline == 1) {
			if (new_line_delay > 0) {
				sprintf(buf, "%s$<%d>", C_LF, new_line_delay);
				newline = _addstr(buf);
			} else
				newline = _addstr(C_LF);
		} else if (DEF(carriage_return) && carriage_return_delay <= 0) {
			if (set_scroll_forward_to_lf) {
				strncpy(buf, carriage_return, MAX_LINE-2);
				buf[MAX_LINE-1] = '\0';
				strncat(buf, scroll_forward, MAX_LINE-strlen(buf)-1);
			} else if (DEF(linefeed_if_not_lf)) {
				strncpy(buf, carriage_return, MAX_LINE-2);
				buf[MAX_LINE-1] = '\0';
				strncat(buf, linefeed_if_not_lf, MAX_LINE-strlen(buf)-1);
			}
			else if (new_line_delay > 0)
				sprintf(buf, "%s%s$<%d>", carriage_return, C_LF, new_line_delay);
			else {
				strncpy(buf, carriage_return, MAX_LINE-2);
				buf[MAX_LINE-1] = '\0';
				strncat(buf, C_LF, MAX_LINE-strlen(buf)-1);
			}
			buf[MAX_LINE-1] = '\0';
			newline = _addstr(buf);
		}
	}

/*
 * We wait until know to decide if we've got a working cr because even
 * one that doesn't work can be used for newline. Unfortunately the
 * space allocated for it is wasted.
 */
	if (return_does_clr_eol == 1 || no_correctly_working_cr == 1)
		carriage_return = NULL;

/*
 * supposedly most termcap entries have ta now and '\t' is no longer a
 * default, but it doesn't seem to be true...
 */
	if (NOTSET(tab)) {
		if (horizontal_tab_delay > 0) {
			sprintf(buf, "%s$<%d>", C_HT, horizontal_tab_delay);
			tab = _addstr(buf);
		} else
			tab = _addstr(C_HT);
	}
#if 0
/* probably not needed and might confuse some programmes */
	if (NOTSET(form_feed)) {
		if (form_feed_delay > 0) {
			sprintf(buf, "%s$<%d>", C_FF, form_feed_delay);
			form_feed = _addstr(buf);
		} else
			form_feed = _addstr(C_FF);
	}
#endif
	if (init_tabs == -1 && has_hardware_tabs == 1)
		init_tabs = 8;

	if (NOTSET(key_backspace))
		key_backspace = _addstr(C_BS);
	if (NOTSET(key_left))
		key_left = _addstr(C_BS);
	if (NOTSET(key_down))
		key_down = _addstr(C_LF);
}

void
_tcapconv() {
	char buf[MAX_LINE+1];

	if (GNU_tab_width > 0 && init_tabs == -1)
		init_tabs = GNU_tab_width;

	if (GNU_has_meta_key == 1 && has_meta_key == -1)
		has_meta_key = 1;

/*
 * this is some what a kludge, but should work unless someone breaks
 * conventions.
 */
	if (DEF(other_non_function_keys)) {
		register char *s;
		static char *o;
		static char name[MAX_NAME] = "k";
		char *str;
		int ind;

		s = strcpy(buf, other_non_function_keys);
		while(*s != '\0') {
			o = s;
			while(*s != ',' && *s != '\0')
				s++;
			if (*s != '\0')
				*s++ = '\0';
			ind = _findstrcode(o);
			if (ind == -1)
				continue;
			str = _term_buf.strs[ind];
			if (!DEF(str))
				continue;
			strncpy(name + 1, strnames[ind], MAX_NAME - 2);
			ind = _findstrname(name);
			if (ind == -1)
				continue;
			if (!NOTSET(_term_buf.strs[ind]))
				continue;
			_term_buf.strs[ind] = _addstr(str);
		}
	}
}
