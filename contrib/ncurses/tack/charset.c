/*
** Copyright (C) 1991, 1997-2000 Free Software Foundation, Inc.
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
** the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
** Boston, MA 02111-1307, USA.
*/

#include <tack.h>

MODULE_ID("$Id: charset.c,v 1.8 2001/06/18 18:44:26 tom Exp $")

/*
	Menu definitions for alternate character set and SGR tests.
*/

static void charset_bel(struct test_list *t, int *state, int *ch);
static void charset_flash(struct test_list *t, int *state, int *ch);
static void charset_civis(struct test_list *t, int *state, int *ch);
static void charset_cvvis(struct test_list *t, int *state, int *ch);
static void charset_cnorm(struct test_list *t, int *state, int *ch);
static void charset_hs(struct test_list *t, int *state, int *ch);
static void charset_status(struct test_list *t, int *state, int *ch);
static void charset_dsl(struct test_list *t, int *state, int *ch);
static void charset_enacs(struct test_list *t, int *state, int *ch);
static void charset_smacs(struct test_list *t, int *state, int *ch);
static void charset_attributes(struct test_list *t, int *state, int *ch);
static void charset_sgr(struct test_list *t, int *state, int *ch);

struct test_list acs_test_list[] = {
	{0, 0, 0, 0, "e) edit terminfo", 0, &edit_menu},
	{MENU_NEXT, 3, "bel", 0, 0, charset_bel, 0},
	{MENU_NEXT, 3, "flash", 0, 0, charset_flash, 0},
	{MENU_NEXT, 3, "civis", 0, 0, charset_civis, 0},
	{MENU_NEXT, 3, "cvvis", 0, 0, charset_cvvis, 0},
	{MENU_NEXT, 3, "cnorm", 0, 0, charset_cnorm, 0},
	{MENU_NEXT, 3, "hs", 0, 0, charset_hs, 0},
	{MENU_NEXT, 3, "tsl) (fsl) (wsl", "hs", 0, charset_status, 0},
	{MENU_NEXT, 3, "dsl", "hs", 0, charset_dsl, 0},
	{MENU_NEXT, 0, "acsc) (enacs) (smacs) (rmacs", 0, 0, charset_enacs, 0},
	{MENU_NEXT, 0, "smacs) (rmacs", 0, 0, charset_smacs, 0},
	{MENU_NEXT, 11, 0, 0, 0, charset_attributes, 0},
	{MENU_NEXT, 11, "sgr) (sgr0", "ma", 0, charset_sgr, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

const struct mode_list alt_modes[] = {
	{"normal", "(sgr0)", "(sgr0)", 1},
	{"standout", "(smso)", "(rmso)", 2},
	{"underline", "(smul)", "(rmul)", 4},
	{"reverse", "(rev)", "(sgr0)", 8},
	{"blink", "(blink)", "(sgr0)", 16},
	{"dim", "(dim)", "(sgr0)", 32},
	{"bold", "(bold)", "(sgr0)", 64},
	{"invis", "(invis)", "(sgr0)", 128},
	{"protect", "(prot)", "(sgr0)", 256},
	{"altcharset", "(smacs)", "(rmacs)", 512}
};

/* On many terminals the underline attribute is the last scan line.
   This is OK unless the following line is reverse video.
   Then the underline attribute does not show up.  The following map
   will reorder the display so that the underline attribute will
   show up. */
const int mode_map[10] = {0, 1, 3, 4, 5, 6, 7, 8, 9, 2};

struct graphics_pair {
	unsigned char c;
	const char *name;
};

static struct graphics_pair glyph[] = {
	{'+', "arrow pointing right"},
	{',', "arrow pointing left"},
	{'.', "arrow pointing down"},
	{'0', "solid square block"},
	{'i', "lantern symbol"},
	{'-', "arrow pointing up"},
	{'`', "diamond"},
	{'a', "checker board (stipple)"},
	{'f', "degree symbol"},
	{'g', "plus/minus"},
	{'h', "board of squares"},
	{'j', "lower right corner"},
	{'k', "upper right corner"},
	{'l', "upper left corner"},
	{'m', "lower left corner"},
	{'n', "plus"},
	{'o', "scan line 1"},
	{'p', "scan line 3"},
	{'q', "horizontal line"},
	{'r', "scan line 7"},
	{'s', "scan line 9"},
	{'t', "left tee (|-)"},
	{'u', "right tee (-|)"},
	{'v', "bottom tee(_|_)"},
	{'w', "top tee (T)"},
	{'x', "vertical line"},
	{'y', "less/equal"},
	{'z', "greater/equal"},
	{'{', "Pi"},
	{'|', "not equal"},
	{'}', "UK pound sign"},
	{'~', "bullet"},
	{'\0', "\0"}
};

/*
**	charset_hs(test_list, status, ch)
**
**	(hs) test Has status line
*/
static void
charset_hs(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (has_status_line != 1) {
		ptext("(hs) Has-status line is not defined.  ");
		generic_done_message(t, state, ch);
	}
}

/*
**	charset_status(test_list, status, ch)
**
**	(tsl) (fsl) (wsl) test Status line
*/
static void
charset_status(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, max;
	char *s;
	static char m[] = "*** status line *** 123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.123456789.";

	if (has_status_line != 1) {
		return;
	}
	put_clear();
	max = width_status_line == -1 ? columns : width_status_line;
	sprintf(temp, "Terminal has status line of %d characters", max);
	ptextln(temp);

	put_str("This line s");
	s = tparm(to_status_line, 0);
	tc_putp(s);
	for (i = 0; i < max; i++)
		putchp(m[i]);
	tc_putp(from_status_line);
	putln("hould not be broken.");
	ptextln("If the previous line is not a complete sentence then (tsl) to-status-line, (fsl) from-status-line, or (wsl) width-of-status-line is incorrect."  );
	generic_done_message(t, state, ch);
}

/*
**	charset_dsl(test_list, status, ch)
**
**	(dsl) test Disable status line
*/
static void
charset_dsl(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (has_status_line != 1) {
		return;
	}
	if (dis_status_line) {
		ptextln("Disable status line (dsl)");
		tc_putp(dis_status_line);
		ptext("If you can still see the status line then (dsl) disable-status-line has failed.  ");
	} else {
		ptext("(dsl) Disable-status-line is not defined.  ");
	}
	generic_done_message(t, state, ch);
}


void 
eat_cookie(void)
{				/* put a blank if this is not a magic cookie
				   terminal */
	if (magic_cookie_glitch < 1)
		putchp(' ');
}


void 
put_mode(char *s)
{				/* send the attribute string (with or without
				   % execution) */
	tc_putp(tparm(s));		/* allow % execution */
}


void
set_attr(int a)
{				/* set the attribute from the bits in a */
	int i, b[32];

	if (magic_cookie_glitch > 0) {
		char_count += magic_cookie_glitch;
	}
	if (a == 0 && exit_attribute_mode) {
		put_mode(exit_attribute_mode);
		return;
	}
	for (i = 0; i < 31; i++) {
		b[i] = (a >> i) & 1;
	}
	tc_putp(tparm(set_attributes, b[1], b[2], b[3], b[4], b[5],
			b[6], b[7], b[8], b[9]));
}

/*
**	charset_sgr(test_list, status, ch)
**
**	(sgr) test Set Graphics Rendition
*/
static void
charset_sgr(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j;

	if (!set_attributes) {
		ptext("(sgr) Set-graphics-rendition is not defined.  ");
		generic_done_message(t, state, ch);
		return;
	}
	if (!exit_attribute_mode) {
		ptextln("(sgr0) Set-graphics-rendition-zero is not defined.");
		/* go ahead and test anyway */
	}
	ptext("Test video attributes (sgr)");

	for (i = 0; i < (int) (sizeof(alt_modes) / sizeof(struct mode_list));
		i++) {
		put_crlf();
		sprintf(temp, "%d %-20s", i, alt_modes[i].name);
		put_str(temp);
		set_attr(alt_modes[i].number);
		sprintf(temp, "%s", alt_modes[i].name);
		put_str(temp);
		set_attr(0);
	}

	putln("\n\nDouble mode test");
	for (i = 0; i <= 9; i++) {
		sprintf(temp, " %2d ", mode_map[i]);
		put_str(temp);
	}
	for (i = 0; i <= 9; i++) {
		put_crlf();
		sprintf(temp, "%d", mode_map[i]);
		put_str(temp);
		for (j = 0; j <= 9; j++) {
			eat_cookie();
			set_attr((1 << mode_map[i]) | (1 << mode_map[j]));
			put_str("Aa");
			set_attr(0);
			if (j < 9)
				eat_cookie();
		}
	}
	put_crlf();

#ifdef max_attributes 
	if (max_attributes >= 0) {
		sprintf(temp, "(ma) Maximum attributes %d  ", max_attributes);
		ptext(temp);
	}
#endif
	generic_done_message(t, state, ch);
}

/*
**	test_one_attr(mode-number, begin-string, end-string)
**
**	Display one attribute line.
*/
static void
test_one_attr(
	int n,
	char *begin_mode,
	char *end_mode)
{
	int i;

	sprintf(temp, "%-10s %s ", alt_modes[n].name, alt_modes[n].begin_mode);
	ptext(temp);
	for (; char_count < 19;) {
		putchp(' ');
	}
	if (begin_mode) {
		putchp('.');
		put_mode(begin_mode);
		put_str(alt_modes[n].name);
		for (i = strlen(alt_modes[n].name); i < 13; i++) {
			putchp(' ');
		}
		if (end_mode) {
			put_mode(end_mode);
			sprintf(temp, ". %s", alt_modes[n].end_mode);
		} else {
			set_attr(0);
			strcpy(temp, ". (sgr)");
		}
		ptextln(temp);
	} else {
		for (i = 0; i < magic_cookie_glitch; i++)
			putchp('*');
		put_str("*** missing ***");
		for (i = 0; i < magic_cookie_glitch; i++)
			putchp('*');
		put_crlf();
	}
}

/*
**	charset_attributes(test_list, status, ch)
**
**	Test SGR
*/
static void
charset_attributes(
	struct test_list *t,
	int *state,
	int *ch)
{
	putln("Test video attributes");
	test_one_attr(1, enter_standout_mode, exit_standout_mode);
	test_one_attr(2, enter_underline_mode, exit_underline_mode);
	test_one_attr(9, enter_alt_charset_mode, exit_alt_charset_mode);
	if (!exit_attribute_mode && !set_attributes) {
		ptextln("(sgr0) exit attribute mode is not defined.");
		generic_done_message(t, state, ch);
		return;
	}
	test_one_attr(3, enter_reverse_mode, exit_attribute_mode);
	test_one_attr(4, enter_blink_mode, exit_attribute_mode);
	test_one_attr(5, enter_dim_mode, exit_attribute_mode);
	test_one_attr(6, enter_bold_mode, exit_attribute_mode);
	test_one_attr(7, enter_secure_mode, exit_attribute_mode);
	test_one_attr(8, enter_protected_mode, exit_attribute_mode);
	generic_done_message(t, state, ch);
}

#define GLYPHS 256

/*
**	charset_smacs(test_list, status, ch)
**
**	display all possible acs characters
**	(smacs) (rmacs)
*/
static void
charset_smacs(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, c;

	if (enter_alt_charset_mode) {
		put_clear();
		ptextln("The following characters are available. (smacs) (rmacs)");
		for (i = ' '; i <= '`'; i += 32) {
			put_crlf();
			put_mode(exit_alt_charset_mode);
			for (c = 0; c < 32; c++) {
				putchp(c + i);
			}
			put_crlf();
			put_mode(enter_alt_charset_mode);
			for (c = 0; c < 32; c++) {
				putchp(c + i);
			}
			put_mode(exit_alt_charset_mode);
			put_crlf();
		}
		put_mode(exit_alt_charset_mode);
		put_crlf();
		generic_done_message(t, state, ch);
	}
}


static void
test_acs(
	int attr)
{				/* alternate character set */
	int i, j;
	char valid_glyph[GLYPHS];
	char acs_table[GLYPHS];
	static unsigned char vt100[] = "`afgjklmnopqrstuvwxyz{|}~";

	line_count = 0;
	for (i = 0; i < GLYPHS; i++) {
		valid_glyph[i] = FALSE;
		acs_table[i] = i;
	}
	if (acs_chars) {
		sprintf(temp, "Alternate character set map: %s",
			expand(acs_chars));
		putln(temp);
		for (i = 0; acs_chars[i]; i += 2) {
			if (acs_chars[i + 1] == 0) {
				break;
			}
			for (j = 0;; j++) {
				if (glyph[j].c == (unsigned char) acs_chars[i]) {
					acs_table[glyph[j].c] = acs_chars[i + 1];
					valid_glyph[glyph[j].c] = TRUE;
					break;
				}
				if (glyph[j].name[0] == '\0') {
					if (isgraph(UChar(acs_chars[i]))) {
						sprintf(temp, "    %c",
							acs_chars[i]);
					} else {
						sprintf(temp, " 0x%02x",
							UChar(acs_chars[i]));
					}
					strcpy(&temp[5], " *** has no mapping ***");
					putln(temp);
					break;
				}
			}
		}
	} else {
		ptextln("acs_chars not defined (acsc)");
		/* enable the VT-100 graphics characters (default) */
		for (i = 0; vt100[i]; i++) {
			valid_glyph[vt100[i]] = TRUE;
		}
	}
	if (attr) {
		set_attr(attr);
	}
	_nc_init_acs();	/* puts 'ena_acs' and incidentally links acs_map[] */
	for (i = 0; glyph[i].name[0]; i++) {
		if (valid_glyph[glyph[i].c]) {
			put_mode(enter_alt_charset_mode);
			put_this(acs_table[glyph[i].c]);
			char_count++;
			put_mode(exit_alt_charset_mode);
			if (magic_cookie_glitch >= 1) {
				sprintf(temp, " %-30.30s", glyph[i].name);
				put_str(temp);
				if (char_count + 33 >= columns)
					put_crlf();
			} else {
				sprintf(temp, " %-24.24s", glyph[i].name);
				put_str(temp);
				if (char_count + 26 >= columns)
					put_crlf();
			}
			if (line_count >= lines) {
				(void) wait_here();
				put_clear();
			}
		}
	}
	if (char_count > 1) {
		put_crlf();
	}
#ifdef ACS_ULCORNER
	maybe_wait(5);
	put_mode(enter_alt_charset_mode);
	put_this(ACS_ULCORNER);
	put_this(ACS_TTEE);
	put_this(ACS_URCORNER);
	put_this(ACS_ULCORNER);
	put_this(ACS_HLINE);
	put_this(ACS_URCORNER);
	char_count += 6;
	put_mode(exit_alt_charset_mode);
	put_crlf();
	put_mode(enter_alt_charset_mode);
	put_this(ACS_LTEE);
	put_this(ACS_PLUS);
	put_this(ACS_RTEE);
	put_this(ACS_VLINE);
	if (magic_cookie_glitch >= 1)
		put_this(' ');
	else {
		put_mode(exit_alt_charset_mode);
		put_this(' ');
		put_mode(enter_alt_charset_mode);
	}
	put_this(ACS_VLINE);
	char_count += 6;
	put_mode(exit_alt_charset_mode);
	put_str("   Here are 2 boxes");
	put_crlf();
	put_mode(enter_alt_charset_mode);
	put_this(ACS_LLCORNER);
	put_this(ACS_BTEE);
	put_this(ACS_LRCORNER);
	put_this(ACS_LLCORNER);
	put_this(ACS_HLINE);
	put_this(ACS_LRCORNER);
	char_count += 6;
	put_mode(exit_alt_charset_mode);
	put_crlf();
#endif
}

/*
**	charset_bel(test_list, status, ch)
**
**	(bel) test Bell
*/
static void
charset_bel(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (bell) {
		ptextln("Testing bell (bel)");
		tc_putp(bell);
		ptext("If you did not hear the Bell then (bel) has failed.  ");
	} else {
		ptext("(bel) Bell is not defined.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	charset_flash(test_list, status, ch)
**
**	(flash) test Visual bell
*/
static void
charset_flash(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (flash_screen) {
		ptextln("Testing visual bell (flash)");
		tc_putp(flash_screen);
		ptext("If you did not see the screen flash then (flash) has failed.  ");
	} else {
		ptext("(flash) Flash is not defined.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	charset_civis(test_list, status, ch)
**
**	(civis) test Cursor invisible
*/
static void
charset_civis(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (cursor_normal) {
		if (cursor_invisible) {
			ptext("(civis) Turn off the cursor.  ");
			tc_putp(cursor_invisible);
			ptext("If you can still see the cursor then (civis) has failed.  ");
		} else {
			ptext("(civis) Cursor-invisible is not defined.  ");
		}
		generic_done_message(t, state, ch);
		tc_putp(cursor_normal);
	}
}

/*
**	charset_cvvis(test_list, status, ch)
**
**	(cvvis) test Cursor very visible
*/
static void
charset_cvvis(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (cursor_normal) {
		if (cursor_visible) {
			ptext("(cvvis) Make cursor very visible.  ");
			tc_putp(cursor_visible);
			ptext("If the cursor is not very visible then (cvvis) has failed.  ");
		} else {
			ptext("(cvvis) Cursor-very-visible is not defined.  ");
		}
		generic_done_message(t, state, ch);
		tc_putp(cursor_normal);
	}
}

/*
**	charset_cnorm(test_list, status, ch)
**
**	(cnorm) test Cursor normal
*/
static void
charset_cnorm(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (cursor_normal) {
		ptext("(cnorm) Normal cursor.  ");
		tc_putp(cursor_normal);
		ptext("If the cursor is not normal then (cnorm) has failed.  ");
	} else {
		ptext("(cnorm) Cursor-normal is not defined.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	charset_enacs(test_list, status, ch)
**
**	test Alternate character set mode and alternate characters
**	(acsc) (enacs) (smacs) (rmacs)
*/
static void
charset_enacs(
	struct test_list *t,
	int *state,
	int *ch)
{
	int c, i;

	if (enter_alt_charset_mode || acs_chars) {
		c = 0;
		while (1) {
			put_clear();
			/*
			   for terminals that use separate fonts for
			   attributes (such as X windows) the line
			   drawing characters must be checked for
			   each font.
			*/
			if (c >= '0' && c <= '9') {
				test_acs(alt_modes[c - '0'].number);
				set_attr(0);
			} else {
				test_acs(0);
			}

			while (1) {
				ptextln("[r] to repeat, [012345789] to test with attributes on, [?] for a list of attributes, anything else to go to next test.  ");
				generic_done_message(t, state, ch);
				if (*ch != '?') {
					break;
				}
				for (i = 0; i <= 9; i++) {
					sprintf(temp, " %d %s %s", i, alt_modes[i].begin_mode,
						alt_modes[i].name);
					ptextln(temp);
				}
			}
			if (*ch >= '0' && *ch <= '9') {
				c = *ch;
			} else
			if (*ch != 'r') {
				break;
			}
		}
	} else {
		ptext("(smacs) Enter-alt-char-set-mode and (acsc) Alternate-char-set are not defined.  ");
		generic_done_message(t, state, ch);
	}
}

/*
**	charset_can_test()
**
**	Initialize the can_test data base
*/
void
charset_can_test(void)
{
	int i;

	for (i = 0; i < 9; i++) {
		can_test(alt_modes[i].begin_mode, FLAG_CAN_TEST);
		can_test(alt_modes[i].end_mode, FLAG_CAN_TEST);
	}
}
