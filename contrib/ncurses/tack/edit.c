/*
** Copyright (C) 1997 Free Software Foundation, Inc.
**
** This file is part of TACK.
**
** TACK is free software; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation; either version 2, or (at your option)
** any later version.
**
** TACK is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with TACK; see the file COPYING.  If not, write to
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA
*/

#include <tack.h>
#include <time.h>
#include <tic.h>

MODULE_ID("$Id: edit.c,v 1.11 2006/06/24 21:22:42 tom Exp $")

/*
 * Terminfo edit features
 */
static void show_info(struct test_list *, int *, int *);
static void show_value(struct test_list *, int *, int *);
static void show_untested(struct test_list *, int *, int *);
static void show_changed(struct test_list *, int *, int *);

#define SHOW_VALUE	1
#define SHOW_EDIT	2
#define SHOW_DELETE	3

struct test_list edit_test_list[] = {
	{MENU_CLEAR, 0, 0, 0, "i) display current terminfo", show_info, 0},
	{0, 0, 0, 0, "w) write the current terminfo to a file", save_info, 0},
	{SHOW_VALUE, 3, 0, 0, "v) show value of a selected cap", show_value, 0},
	{SHOW_EDIT, 4, 0, 0, "e) edit value of a selected cap", show_value, 0},
	{SHOW_DELETE, 3, 0, 0, "d) delete string", show_value, 0},
	{0, 3, 0, 0, "m) show caps that have been modified", show_changed, 0},
	{MENU_CLEAR + FLAG_CAN_TEST, 0, 0, 0, "c) show caps that can be tested", show_report, 0},
	{MENU_CLEAR + FLAG_TESTED, 0, 0, 0, "t) show caps that have been tested", show_report, 0},
	{MENU_CLEAR + FLAG_FUNCTION_KEY, 0, 0, 0, "f) show a list of function keys", show_report, 0},
	{MENU_CLEAR, 0, 0, 0, "u) show caps defined that can not be tested", show_untested, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

static char change_pad_text[MAX_CHANGES][80];
static struct test_list change_pad_list[MAX_CHANGES] = {
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

static void build_change_menu(struct test_menu *);
static void change_one_entry(struct test_list *, int *, int *);

struct test_menu change_pad_menu = {
	0, 'q', 0,
	"Select cap name", "change", 0,
	build_change_menu, change_pad_list, 0, 0, 0
};

static TERMTYPE	original_term;		/* terminal type description */

static char flag_boolean[BOOLCOUNT];	/* flags for booleans */
static char flag_numerics[NUMCOUNT];	/* flags for numerics */
static char *flag_strings;		/* flags for strings */
static int *label_strings;
static int xon_index;			/* Subscript for (xon) */
static int xon_shadow;

static int start_display;		/* the display has just started */
static int display_lines;		/* number of lines displayed */

static void
alloc_arrays(void)
{
	if (flag_strings == 0) {
		label_strings = (int *)calloc(MAX_STRINGS, sizeof(int));
		flag_strings = (char *)calloc(MAX_STRINGS, sizeof(char));
	}
}

/*
**	send_info_string(str)
**
**	Return the terminfo string prefixed by the correct separator
*/
static void
send_info_string(
	const char *str,
	int *ch)
{
	int len;

	if (display_lines == -1) {
		return;
	}
	len = strlen(str);
	if (len + char_count + 3 >= columns) {
		if (start_display == 0) {
			put_str(",");
		}
		put_crlf();
		if (++display_lines > lines) {
			ptext("-- more -- ");
			*ch = wait_here();
			if (*ch == 'q') {
				display_lines = -1;
				return;
			}
			display_lines = 0;
		}
		if (len >= columns) {
			/* if the terminal does not (am) then this loses */
			if (columns) {
				display_lines += ((strlen(str) + 3) / columns) + 1;
			}
			put_str("   ");
			put_str(str);
			start_display = 0;
			return;
		}
		ptext("   ");
	} else
	if (start_display == 0) {
		ptext(", ");
	} else {
		ptext("   ");
	}
	ptext(str);
	start_display = 0;
}

/*
**	show_info(test_list, status, ch)
**
**	Display the current terminfo
*/
static void
show_info(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	int i;
	char buf[1024];

	display_lines = 1;
	start_display = 1;
	for (i = 0; i < BOOLCOUNT; i++) {
		if ((i == xon_index) ? xon_shadow : CUR Booleans[i]) {
			send_info_string(boolnames[i], ch);
		}
	}
	for (i = 0; i < NUMCOUNT; i++) {
		if (CUR Numbers[i] >= 0) {
			sprintf(buf, "%s#%d", numnames[i], CUR Numbers[i]);
			send_info_string(buf, ch);
		}
	}
	for (i = 0; i < MAX_STRINGS; i++) {
		if (CUR Strings[i]) {
			sprintf(buf, "%s=%s", STR_NAME(i),
				print_expand(CUR Strings[i]));
			send_info_string(buf, ch);
		}
	}
	put_newlines(2);
	*ch = REQUEST_PROMPT;
}

/*
**	save_info_string(str, fp)
**
**	Write the terminfo string prefixed by the correct separator
*/
static void
save_info_string(
	const char *str,
	FILE *fp)
{
	int len;

	len = strlen(str);
	if (len + display_lines >= 77) {
		if (display_lines > 0) {
			(void) fprintf(fp, "\n\t");
		}
		display_lines = 8;
	} else
	if (display_lines > 0) {
		(void) fprintf(fp, " ");
		display_lines++;
	} else {
		(void) fprintf(fp, "\t");
		display_lines = 8;
	}
	(void) fprintf(fp, "%s,", str);
	display_lines += len + 1;
}

/*
**	save_info(test_list, status, ch)
**
**	Write the current terminfo to a file
*/
void
save_info(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i;
	FILE *fp;
	time_t now;
	char buf[1024];

	if ((fp = fopen(tty_basename, "w")) == (FILE *) NULL) {
		(void) sprintf(temp, "can't open: %s", tty_basename);
		ptextln(temp);
		generic_done_message(t, state, ch);
		return;
	}
	time(&now);
	/* Note: ctime() returns a newline at the end of the string */
	(void) fprintf(fp, "# Terminfo created by TACK for TERM=%s on %s",
		tty_basename, ctime(&now));
	(void) fprintf(fp, "%s|%s,\n", tty_basename, longname());

	display_lines = 0;
	for (i = 0; i < BOOLCOUNT; i++) {
		if (i == xon_index ? xon_shadow : CUR Booleans[i]) {
			save_info_string(boolnames[i], fp);
		}
	}
	for (i = 0; i < NUMCOUNT; i++) {
		if (CUR Numbers[i] >= 0) {
			sprintf(buf, "%s#%d", numnames[i], CUR Numbers[i]);
			save_info_string(buf, fp);
		}
	}
	for (i = 0; i < MAX_STRINGS; i++) {
		if (CUR Strings[i]) {
			sprintf(buf, "%s=%s", STR_NAME(i),
				_nc_tic_expand(CUR Strings[i], TRUE, TRUE));
			save_info_string(buf, fp);
		}
	}
	(void) fprintf(fp, "\n");
	(void) fclose(fp);
	sprintf(temp, "Terminfo saved as file: %s", tty_basename);
	ptextln(temp);
}

/*
**	show_value(test_list, status, ch)
**
**	Display the value of a selected cap
*/
static void
show_value(
	struct test_list *t,
	int *state GCC_UNUSED,
	int *ch)
{
	struct name_table_entry const *nt;
	char *s;
	int n, op, b;
	char buf[1024];
	char tmp[1024];

	ptext("enter name: ");
	read_string(buf, 80);
	if (buf[0] == '\0' || buf[1] == '\0') {
		*ch = buf[0];
		return;
	}
	if (line_count + 2 >= lines) {
		put_clear();
	}
	op = t->flags & 255;
	if ((nt = _nc_find_entry(buf, _nc_info_hash_table))) {
		switch (nt->nte_type) {
		case BOOLEAN:
			if (op == SHOW_DELETE) {
				if (nt->nte_index == xon_index) {
					xon_shadow = 0;
				} else {
					CUR Booleans[nt->nte_index] = 0;
				}
				return;
			}
			b = nt->nte_index == xon_index ? xon_shadow :
				CUR Booleans[nt->nte_index];
			sprintf(temp, "boolean  %s %s", buf,
				b ? "True" : "False");
			break;
		case STRING:
			if (op == SHOW_DELETE) {
				CUR Strings[nt->nte_index] = (char *) 0;
				return;
			}
			if (CUR Strings[nt->nte_index]) {
				sprintf(temp, "string  %s %s", buf,
					expand(CUR Strings[nt->nte_index]));
			} else {
				sprintf(temp, "undefined string %s", buf);
			}
			break;
		case NUMBER:
			if (op == SHOW_DELETE) {
				CUR Numbers[nt->nte_index] = -1;
				return;
			}
			sprintf(temp, "numeric  %s %d", buf,
				CUR Numbers[nt->nte_index]);
			break;
		default:
			sprintf(temp, "unknown");
			break;
		}
		ptextln(temp);
	} else {
		sprintf(temp, "Cap not found: %s", buf);
		ptextln(temp);
		return;
	}
	if (op != SHOW_EDIT) {
		return;
	}
	if (nt->nte_type == BOOLEAN) {
		ptextln("Value flipped");
		if (nt->nte_index == xon_index) {
			xon_shadow = !xon_shadow;
		} else {
			CUR Booleans[nt->nte_index] = !CUR Booleans[nt->nte_index];
		}
		return;
	}
	ptextln("Enter new value");
	read_string(buf, sizeof(buf));

	switch (nt->nte_type) {
	case STRING:
		_nc_reset_input((FILE *) 0, buf);
		_nc_trans_string(tmp, tmp + sizeof(tmp));
		s = (char *)malloc(strlen(tmp) + 1);
		strcpy(s, tmp);
		CUR Strings[nt->nte_index] = s;
		sprintf(temp, "new string value  %s", nt->nte_name);
		ptextln(temp);
		ptextln(expand(CUR Strings[nt->nte_index]));
		break;
	case NUMBER:
		if (sscanf(buf, "%d", &n) == 1) {
			CUR Numbers[nt->nte_index] = n;
			sprintf(temp, "new numeric value  %s %d",
				nt->nte_name, n);
			ptextln(temp);
		} else {
			sprintf(temp, "Illegal number: %s", buf);
			ptextln(temp);
		}
		break;
	default:
		break;
	}
}

/*
**	get_string_cap_byname(name, long_name)
**
**	Given a cap name, find the value
**	Errors are quietly ignored.
*/
char *
get_string_cap_byname(
	const char *name,
	const char **long_name)
{
	struct name_table_entry const *nt;

	if ((nt = _nc_find_entry(name, _nc_info_hash_table))) {
		if (nt->nte_type == STRING) {
			*long_name = strfnames[nt->nte_index];
			return (CUR Strings[nt->nte_index]);
		}
	}
	*long_name = "??";
	return (char *) 0;
}

/*
**	get_string_cap_byvalue(value)
**
**	Given a capability string, find its position in the data base.
**	Return the index or -1 if not found.
*/
int
get_string_cap_byvalue(
	const char *value)
{
	int i;

	if (value) {
		for (i = 0; i < MAX_STRINGS; i++) {
			if (CUR Strings[i] == value) {
				return i;
			}
		}
		/* search for translated strings */
		for (i = 0; i < TM_last; i++) {
			if (TM_string[i].value == value) {
				return TM_string[i].index;
			}
		}
	}
	return -1;
}

/*
**	show_changed(test_list, status, ch)
**
**	Display a list of caps that have been changed.
*/
static void
show_changed(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	int i, header = 1, v;
	const char *a;
	const char *b;
	static char title[] = "                     old value   cap  new value";
	char abuf[1024];

	for (i = 0; i < BOOLCOUNT; i++) {
		v = (i == xon_index) ? xon_shadow : CUR Booleans[i];
		if (original_term.Booleans[i] != v) {
			if (header) {
				ptextln(title);
				header = 0;
			}
			sprintf(temp, "%30d %6s %d",
				original_term.Booleans[i], boolnames[i], v);
			ptextln(temp);
		}
	}
	for (i = 0; i < NUMCOUNT; i++) {
		if (original_term.Numbers[i] != CUR Numbers[i]) {
			if (header) {
				ptextln(title);
				header = 0;
			}
			sprintf(temp, "%30d %6s %d",
				original_term.Numbers[i], numnames[i],
				CUR Numbers[i]);
			ptextln(temp);
		}
	}
	for (i = 0; i < MAX_STRINGS; i++) {
		a = original_term.Strings[i] ? original_term.Strings[i] : "";
		b = CUR Strings[i] ?  CUR Strings[i] : "";
		if (strcmp(a, b)) {
			if (header) {
				ptextln(title);
				header = 0;
			}
			strcpy(abuf, _nc_tic_expand(a, TRUE, TRUE));
			sprintf(temp, "%30s %6s %s", abuf, STR_NAME(i),
				_nc_tic_expand(b, TRUE, TRUE));
			putln(temp);
		}
	}
	if (header) {
		ptextln("No changes");
	}
	put_crlf();
	*ch = REQUEST_PROMPT;
}

/*
**	user_modified()
**
**	Return TRUE if the user has modified the terminfo
*/
int
user_modified(void)
{
	const char *a, *b;
	int i, v;

	for (i = 0; i < BOOLCOUNT; i++) {
		v = (i == xon_index) ? xon_shadow : CUR Booleans[i];
		if (original_term.Booleans[i] != v) {
			return TRUE;
		}
	}
	for (i = 0; i < NUMCOUNT; i++) {
		if (original_term.Numbers[i] != CUR Numbers[i]) {
			return TRUE;
		}
	}
	for (i = 0; i < MAX_STRINGS; i++) {
		a = original_term.Strings[i] ? original_term.Strings[i] : "";
		b = CUR Strings[i] ?  CUR Strings[i] : "";
		if (strcmp(a, b)) {
			return TRUE;
		}
	}
	return FALSE;
}

/*****************************************************************************
 *
 * Maintain the list of capabilities that can be tested
 *
 *****************************************************************************/

/*
**	mark_cap(name, flag)
**
**	Mark the cap data base with the flag provided.
*/
static void
mark_cap(
	char *name,
	int flag)
{
	struct name_table_entry const *nt;

	alloc_arrays();
	if ((nt = _nc_find_entry(name, _nc_info_hash_table))) {
		switch (nt->nte_type) {
		case BOOLEAN:
			flag_boolean[nt->nte_index] |= flag;
			break;
		case STRING:
			flag_strings[nt->nte_index] |= flag;
			break;
		case NUMBER:
			flag_numerics[nt->nte_index] |= flag;
			break;
		default:
			sprintf(temp, "unknown cap type (%s)", name);
			ptextln(temp);
			break;
		}
	} else {
		sprintf(temp, "Cap not found: %s", name);
		ptextln(temp);
		(void) wait_here();
	}
}

/*
**	can_test(name-list, flags)
**
**	Scan the name list and get the names.
**	Enter each name into the can-test data base.
**	<space> ( and ) may be used as separators.
*/
void
can_test(
	const char *s,
	int flags)
{
	int ch, j;
	char name[32];

	if (s) {
		for (j = 0; (name[j] = ch = *s); s++) {
			if (ch == ' ' || ch == ')' || ch == '(') {
				if (j) {
					name[j] = '\0';
					mark_cap(name, flags);
				}
				j = 0;
			} else {
				j++;
			}
		}
		if (j) {
			mark_cap(name, flags);
		}
	}
}

/*
**	cap_index(name-list, index-list)
**
**	Scan the name list and return a list of indexes.
**	<space> ( and ) may be used as separators.
**	This list is terminated with -1.
*/
void
cap_index(
	const char *s,
	int *inx)
{
	struct name_table_entry const *nt;
	int ch, j;
	char name[32];

	if (s) {
		for (j = 0; ; s++) {
			name[j] = ch = *s;
			if (ch == ' ' || ch == ')' || ch == '(' || ch == 0) {
				if (j) {
					name[j] = '\0';
					if ((nt = _nc_find_entry(name,
						_nc_info_hash_table)) &&
						(nt->nte_type == STRING)) {
						*inx++ = nt->nte_index;
					}
				}
				if (ch == 0) {
					break;
				}
				j = 0;
			} else {
				j++;
			}
		}
	}
	*inx = -1;
}

/*
**	cap_match(name-list, cap)
**
**	Scan the name list and see if the cap is in the list.
**	Return TRUE if we find an exact match.
**	<space> ( and ) may be used as separators.
*/
int
cap_match(
	const char *names,
	const char *cap)
{
	char *s;
	int c, l, t;

	if (names) {
		l = strlen(cap);
		while ((s = strstr(names, cap))) {
			c = (names == s) ? 0 : *(s - 1);
			t = s[l];
			if ((c == 0 || c == ' ' || c == '(') &&
				(t == 0 || t == ' ' || t == ')')) {
				return TRUE;
			}
			if (t == 0) {
				break;
			}
			names = s + l;
		}
	}
	return FALSE;
}

/*
**	show_report(test_list, status, ch)
**
**	Display a list of caps that can be tested
*/
void
show_report(
	struct test_list *t,
	int *state GCC_UNUSED,
	int *ch)
{
	int i, j, nc, flag;
	const char *s;
	const char **nx = malloc(BOOLCOUNT + NUMCOUNT + MAX_STRINGS);

	alloc_arrays();
	flag = t->flags & 255;
	nc = 0;
	for (i = 0; i < BOOLCOUNT; i++) {
		if (flag_boolean[i] & flag) {
			nx[nc++] = boolnames[i];
		}
	}
	for (i = 0; i < NUMCOUNT; i++) {
		if (flag_numerics[i] & flag) {
			nx[nc++] = numnames[i];
		}
	}
	for (i = 0; i < MAX_STRINGS; i++) {
		if (flag_strings[i] & flag) {
			nx[nc++] = STR_NAME(i);
		}
	}
	/* sort */
	for (i = 0; i < nc - 1; i++) {
		for (j = i + 1; j < nc; j++) {
			if (strcmp(nx[i], nx[j]) > 0) {
				s = nx[i];
				nx[i] = nx[j];
				nx[j] = s;
			}
		}
	}
	if (flag & FLAG_FUNCTION_KEY) {
		ptextln("The following function keys can be tested:");
	} else
	if (flag & FLAG_CAN_TEST) {
		ptextln("The following capabilities can be tested:");
	} else
	if (flag & FLAG_TESTED) {
		ptextln("The following capabilities have been tested:");
	}
	put_crlf();
	for (i = 0; i < nc; i++) {
		sprintf(temp, "%s ", nx[i]);
		ptext(temp);
	}
	put_newlines(1);
	*ch = REQUEST_PROMPT;
	free (nx);
}

/*
**	show_untested(test_list, status, ch)
**
**	Display a list of caps that are defined but cannot be tested.
**	Don't bother to sort this list.
*/
static void
show_untested(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	int i;

	alloc_arrays();
	ptextln("Caps that are defined but cannot be tested:");
	for (i = 0; i < BOOLCOUNT; i++) {
		if (flag_boolean[i] == 0 && CUR Booleans[i]) {
			sprintf(temp, "%s ", boolnames[i]);
			ptext(temp);
		}
	}
	for (i = 0; i < NUMCOUNT; i++) {
		if (flag_numerics[i] == 0 && CUR Numbers[i] >= 0) {
			sprintf(temp, "%s ", numnames[i]);
			ptext(temp);
		}
	}
	for (i = 0; i < MAX_STRINGS; i++) {
		if (flag_strings[i] == 0 && CUR Strings[i]) {
			sprintf(temp, "%s ", STR_NAME(i));
			ptext(temp);
		}
	}
	put_newlines(1);
	*ch = REQUEST_PROMPT;
}

/*
**	edit_init()
**
**	Initialize the function key data base
*/
void
edit_init(void)
{
	int i, j, lc;
	char *lab;
	struct name_table_entry const *nt;

	alloc_arrays();

	_nc_copy_termtype(&original_term, &cur_term->type);
	for (i = 0; i < BOOLCOUNT; i++) {
		original_term.Booleans[i] = CUR Booleans[i];
	}
	for (i = 0; i < NUMCOUNT; i++) {
		original_term.Numbers[i] = CUR Numbers[i];
	}
	/* scan for labels */
	for (i = lc = 0; i < MAX_STRINGS; i++) {
		original_term.Strings[i] = CUR Strings[i];
		if (strncmp(STR_NAME(i), "lf", 2) == 0) {
			flag_strings[i] |= FLAG_LABEL;
			if (CUR Strings[i]) {
				label_strings[lc++] = i;
			}
		}
	}
	/* scan for function keys */
	for (i = 0; i < MAX_STRINGS; i++) {
		const char *this_name = STR_NAME(i);
		if ((this_name[0] == 'k') && strcmp(this_name, "kmous")) {
			flag_strings[i] |= FLAG_FUNCTION_KEY;
			lab = (char *) 0;
			for (j = 0; j < lc; j++) {
				if (!strcmp(this_name,
					STR_NAME(label_strings[j]))) {
					lab = CUR Strings[label_strings[j]];
					break;
				}
			}
			enter_key(this_name, CUR Strings[i], lab);
		}
	}
	/* Lookup the translated strings */
	for (i = 0; i < TM_last; i++) {
		if ((nt = _nc_find_entry(TM_string[i].name,
			_nc_info_hash_table)) && (nt->nte_type == STRING)) {
			TM_string[i].index = nt->nte_index;
		} else {
			sprintf(temp, "TM_string lookup failed for: %s",
				TM_string[i].name);
			ptextln(temp);
		}
	}
	if ((nt = _nc_find_entry("xon", _nc_info_hash_table)) != 0) {
		xon_index = nt->nte_index;
	}
	xon_shadow = xon_xoff;
	free(label_strings);
}

/*
**	change_one_entry(test_list, status, ch)
**
**	Change the padding on the selected cap
*/
static void
change_one_entry(
	struct test_list *test,
	int *state,
	int *chp)
{
	struct name_table_entry const *nt;
	int i, j, x, star, slash,  v, dot, ch;
	const char *s;
	char *t, *p;
	const char *current_string;
	char buf[1024];
	char pad[1024];

	i = test->flags & 255;
	if (i == 255) {
		/* read the cap name from the user */
		ptext("enter name: ");
		read_string(pad, 32);
		if (pad[0] == '\0' || pad[1] == '\0') {
			*chp = pad[0];
			return;
		}
		if ((nt = _nc_find_entry(pad, _nc_info_hash_table)) &&
			(nt->nte_type == STRING)) {
			x = nt->nte_index;
			current_string = CUR Strings[x];
		} else {
			sprintf(temp, "%s is not a string capability", pad);
			ptext(temp);
			generic_done_message(test, state, chp);
			return;
		}
	} else {
		x = tx_index[i];
		current_string = tx_cap[i];
		strcpy(pad, STR_NAME(x));
	}
	if (!current_string) {
		ptextln("That string is not currently defined.  Please enter a new value, including the padding delay:");
		read_string(buf, sizeof(buf));
		_nc_reset_input((FILE *) 0, buf);
		_nc_trans_string(pad, pad + sizeof(pad));
		t = (char *)malloc(strlen(pad) + 1);
		strcpy(t, pad);
		CUR Strings[x] = t;
		sprintf(temp, "new string value  %s", STR_NAME(x));
		ptextln(temp);
		ptextln(expand(t));
		return;
	}
	sprintf(buf, "Current value: (%s) %s", pad, _nc_tic_expand(current_string, TRUE, TRUE));
	putln(buf);
	ptextln("Enter new pad.  0 for no pad.  CR for no change.");
	read_string(buf, 32);
	if (buf[0] == '\0' || (buf[1] == '\0' && isalpha(UChar(buf[0])))) {
		*chp = buf[0];
		return;
	}
	star = slash = FALSE;
	for (j = v = dot = 0; (ch = buf[j]); j++) {
		if (ch >= '0' && ch <= '9') {
			v = ch - '0' + v * 10;
			if (dot) {
				dot++;
			}
		} else if (ch == '*') {
			star = TRUE;
		} else if (ch == '/') {
			slash = TRUE;
		} else if (ch == '.') {
			dot = 1;
		} else {
			sprintf(temp, "Illegal character: %c", ch);
			ptextln(temp);
			ptext("General format:  99.9*/  ");
			generic_done_message(test, state, chp);
			return;
		}
	}
	while (dot > 2) {
		v /= 10;
		dot--;
	}
	if (dot == 2) {
		sprintf(pad, "%d.%d%s%s", v / 10, v % 10,
				star ? "*" : "", slash ? "/" : "");
	} else {
		sprintf(pad, "%d%s%s",
			v, star ? "*" : "", slash ? "/" : "");
	}
	s = current_string;
	t = buf;
	for (v = 0; (ch = *t = *s++); t++) {
		if (v == '$' && ch == '<') {
			while ((ch = *s++) && (ch != '>'));
			for (p = pad; (*++t = *p++); );
			*t++ = '>';
			while ((*t++ = *s++));
			pad[0] = '\0';
			break;
		}
		v = ch;
	}
	if (pad[0]) {
		sprintf(t, "$<%s>", pad);
	}
	if ((t = (char *)malloc(strlen(buf) + 1))) {
		strcpy(t, buf);
		CUR Strings[x] = t;
		if (i != 255) {
			tx_cap[i] = t;
		}
	}
	generic_done_message(test, state, chp);
}

/*
**	build_change_menu(menu_list)
**
**	Build the change pad menu list
*/
static void
build_change_menu(
	struct test_menu *m)
{
	int i, j, k;
	char *s;

	for (i = j = 0; i < txp; i++) {
		if ((k = tx_index[i]) >= 0) {
			s = _nc_tic_expand(tx_cap[i], TRUE, TRUE);
			s[40] = '\0';
			sprintf(change_pad_text[j], "%c) (%s) %s",
				'a' + j, STR_NAME(k), s);
			change_pad_list[j].flags = i;
			change_pad_list[j].lines_needed = 4;
			change_pad_list[j].menu_entry = change_pad_text[j];
			change_pad_list[j].test_procedure = change_one_entry;
			j++;
		}
	}
	strcpy(change_pad_text[j], "z) enter name");
	change_pad_list[j].flags = 255;
	change_pad_list[j].lines_needed = 4;
	change_pad_list[j].menu_entry = change_pad_text[j];
	change_pad_list[j].test_procedure = change_one_entry;
	j++;
	change_pad_list[j].flags = MENU_LAST;
	if (m->menu_title) {
		put_crlf();
		ptextln(m->menu_title);
	}
}
