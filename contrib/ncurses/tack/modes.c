/*
** Copyright (C) 1991, 1997 Free Software Foundation, Inc.
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

MODULE_ID("$Id: modes.c,v 1.1 1998/01/10 00:29:53 tom Exp $")

/*
 * Tests boolean flags and terminal modes.
 */
static void subtest_os(struct test_list *, int *, int *);
static void subtest_rmam(struct test_list *, int *, int *);
static void subtest_smam(struct test_list *, int *, int *);
static void subtest_am(struct test_list *, int *, int *);
static void subtest_ul(struct test_list *, int *, int *);
static void subtest_uc(struct test_list *, int *, int *);
static void subtest_bw(struct test_list *, int *, int *);
static void subtest_xenl(struct test_list *, int *, int *);
static void subtest_eo(struct test_list *, int *, int *);
static void subtest_xmc(struct test_list *, int *, int *);
static void subtest_xhp(struct test_list *, int *, int *);
static void subtest_mir(struct test_list *, int *, int *);
static void subtest_msgr(struct test_list *, int *, int *);
static void subtest_tbc(struct test_list *, int *, int *);
static void subtest_xt(struct test_list *, int *, int *);
static void subtest_hts(struct test_list *, int *, int *);
static void subtest_cbt(struct test_list *, int *, int *);
static void subtest_in(struct test_list *, int *, int *);
static void subtest_dadb(struct test_list *, int *, int *);

struct test_list mode_test_list[] = {
	{0, 0, 0, 0, "e) edit terminfo", 0, &edit_menu},
	{MENU_NEXT, 3, "os", 0, 0, subtest_os, 0},
	{MENU_NEXT, 1, "rmam", 0, 0, subtest_rmam, 0},
	{MENU_NEXT, 1, "smam", 0, 0, subtest_smam, 0},
	{MENU_NEXT, 1, "am", 0, 0, subtest_am, 0},
	{MENU_NEXT, 3, "ul", 0, 0, subtest_ul, 0},
	{MENU_NEXT, 3, "uc", 0, 0, subtest_uc, 0},
	{MENU_NEXT, 3, "bw", 0, 0, subtest_bw, 0},
	{MENU_NEXT, 4, "xenl", 0, 0, subtest_xenl, 0},
	{MENU_NEXT, 3, "eo", 0, 0, subtest_eo, 0},
	{MENU_NEXT, 3, "xmc", 0, 0, subtest_xmc, 0},
	{MENU_NEXT, 3, "xhp", 0, 0, subtest_xhp, 0},
	{MENU_NEXT, 6, "mir", 0, 0, subtest_mir, 0},
	{MENU_NEXT, 6, "msgr", 0, 0, subtest_msgr, 0},
	{MENU_NEXT | MENU_CLEAR, 0, "tbc", "it", 0, subtest_tbc, 0},
	{MENU_NEXT | MENU_CLEAR, 0, "hts", "it", 0, subtest_hts, 0},
	{MENU_NEXT, 4, "xt", "it", 0, subtest_xt, 0},
	{MENU_NEXT, 1, "cbt", "it", 0, subtest_cbt, 0},
	{MENU_NEXT, 6, "in", 0, 0, subtest_in, 0},
	{MENU_NEXT, 1, "da) (db", 0, 0, subtest_dadb, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

/*
**	subtest_os(test_list, status, ch)
**
**	test over strike mode (os)
*/
static void
subtest_os(
	struct test_list *t,
	int *state,
	int *ch)
{
	ptext("(os) should be true, not false.");
	put_cr();
	ptextln("(os) should be           false.");
	sprintf(temp, "(os) over-strike is %s in the data base.  ",
		over_strike ? "true" : "false");
	ptext(temp);
	generic_done_message(t, state, ch);
}

/*
**	subtest_rmam(test_list, status, ch)
**
**	test exit automatic margins mode (rmam)
*/
static void
subtest_rmam(
	struct test_list *t,
	int *state,
	int *ch)
{
	int j;

	if (!exit_am_mode) {
		ptext("(rmam) not present.  ");
	} else
	if (!can_go_home) {
		ptext("(rmam) not tested, no way to home cursor.  ");
	} else
	if (over_strike) {
		put_clear();
		go_home();
		tc_putp(exit_am_mode);
		ptext("\n(rmam) will     reset (am)");
		go_home();
		for (j = 0; j < columns; j++)
			put_this(' ');
		ptext("(rmam) will not reset (am)");
		go_home();
		put_newlines(2);
	} else {
		put_clear();
		go_home();
		tc_putp(exit_am_mode);
		ptext("\n(rmam) will reset (am)");
		go_home();
		for (j = 0; j < columns; j++)
			put_this(' ');
		ptext("(rmam) will not reset (am) ");
		go_home();
		put_str("                          ");
		go_home();
		put_newlines(2);
	}
	ptext("Exit-automatic-margins ");
	generic_done_message(t, state, ch);
}

/*
**	subtest_smam(test_list, status, ch)
**
**	test enter automatic margins mode (smam)
*/
static void
subtest_smam(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j;

	if (!enter_am_mode) {
		ptext("(smam) not present.  ");
	} else
	if (!can_go_home) {
		ptext("(smam) not tested, no way to home cursor.  ");
	} else
	if (over_strike) {
		put_clear();
		go_home();
		tc_putp(enter_am_mode);
		ptext("\n(smam) will ");
		i = char_count;
		ptext("not set (am)");
		go_home();
		for (j = -i; j < columns; j++)
			put_this(' ');
		put_str("@@@");
		put_newlines(2);
	} else {
		put_clear();
		go_home();
		tc_putp(enter_am_mode);
		ptext("\n(smam) will not set (am)");
		go_home();
		for (j = 0; j < columns; j++)
			put_this(' ');
		ptext("(smam) will set (am)    ");
		go_home();
		put_str("                          ");
		put_newlines(2);
	}
	ptext("Enter-automatic-margins ");
	generic_done_message(t, state, ch);
}

/*
**	subtest_am(test_list, status, ch)
**
**	test automatic margins (am)
*/
static void
subtest_am(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j;

	if (!can_go_home) {
		ptextln("(am) not tested, no way to home cursor.  ");
	} else
	if (over_strike) {
		put_clear();
		go_home();
		ptext("\n(am) should ");
		i = char_count;
		ptext("not be set");
		go_home();
		for (j = -i; j < columns; j++)
			put_this(' ');
		put_str("@@@");
		go_home();
		put_newlines(2);
		sprintf(temp, "(am) is %s in the data base",
			auto_right_margin ? "true" : "false");
		ptextln(temp);
	} else {
		put_clear();
		go_home();
		ptext("\n(am) should not be set");
		go_home();
		for (j = 0; j < columns; j++)
			put_this(' ');
		ptext("(am) should be set    ");
		go_home();
		put_str("                       \n\n");
		sprintf(temp, "(am) is %s in the data base",
			auto_right_margin ? "true" : "false");
		ptextln(temp);
	}
	ptext("Automatic-right-margin ");
	generic_done_message(t, state, ch);
}

/* Note: uprint() sends underscore back-space character, and
        ucprint() sends character back-space underscore.  */

/*
**	uprint(string)
**
**	underline string for (ul) test
*/
static void
uprint(const char *s)
{
	if (s) {
		while (*s) {
			put_str("_\b");
			putchp(*s++);
		}
	}
}

/*
**	ucprint(string)
**
**	underline string for (uc) test
*/
static void
ucprint(const char *s)
{
	if (s) {
		while (*s) {
			putchp(*s++);
			putchp('\b');
			tc_putp(underline_char);
		}
	}
}

/*
**	subtest_ul(test_list, status, ch)
**
**	test transparent underline (ul)
*/
static void
subtest_ul(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (!over_strike) {
		/* (ul) is used only if (os) is reset */
		put_crlf();
		sprintf(temp, "This text should %sbe underlined.",
			transparent_underline ? "" : "not ");
		uprint(temp);
		put_crlf();
		ptextln("If the above line is not underlined the (ul) should be false.");
		sprintf(temp, "(ul) Transparent-underline is %s in the data base",
			transparent_underline ? "true" : "false");
		ptextln(temp);
		generic_done_message(t, state, ch);
	}
}

/*
**	subtest_uc(test_list, status, ch)
**
**	test underline character (uc)
*/
static void
subtest_uc(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (!over_strike) {
		if (underline_char) {
			ucprint("This text should be underlined.");
			put_crlf();
			ptextln("If the above text is not underlined the (uc) has failed.");
			ptext("Underline-character ");
		} else {
			ptext("(uc) underline-character is not defined.  ");
		}
		generic_done_message(t, state, ch);
	}
}

/*
**	subtest_bw(test_list, status, ch)
**
**	test auto left margin (bw)
*/
static void
subtest_bw(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j;

	if (over_strike) {
		/* test (bw) */
		ptext("\n(bw) should ");
		i = char_count;
		ptextln("not be set.");
		for (j = i; j < columns; j++)
			put_str("\b");
		put_str("@@@");
		put_crlf();
		sprintf(temp, "(bw) Auto-left-margin is %s in the data base",
			auto_left_margin ? "true" : "false");
		ptextln(temp);
	} else {
		/* test (bw) */
		ptextln("(bw) should not be set.");
		for (i = 12; i < columns; i++)
			put_str("\b");
		if (delete_character) {
			for (i = 0; i < 4; i++)
				tc_putp(delete_character);
		} else {
			put_str("   ");
		}
		put_crlf();
		sprintf(temp, "(bw) Auto-left-margin is %s in the data base",
			auto_left_margin ? "true" : "false");
		ptextln(temp);
	}
	generic_done_message(t, state, ch);
}

/*
**	subtest_tbc(test_list, status, ch)
**
**	test clear tabs (tbc)
*/
static void
subtest_tbc(
	struct test_list *t,
	int *state,
	int *ch)
{
	int tabat;		/* the tab spacing we end up with */
	int i;

	if (clear_all_tabs && !set_tab) {
		ptext("(tbc) Clear-all-tabs is defined but (hts) set-tab is not.  ");
		ptext("Once the tabs are cleared there is no way to set them.  ");
	} else
	if (clear_all_tabs) {
		tabat = set_tab ? 8 : init_tabs;
		tc_putp(clear_all_tabs);
		ptext("Clear tabs (tbc)");
		go_home();
		put_crlf();
		putchp('\t');
		putchp('T');
		go_home();
		put_newlines(2);
		for (i = 0; i < columns; i++) {
			if (i == tabat) {
				putchp('T');
			} else {
				putchp('.');
			}
		}
		go_home();
		ptext("\n\n\nIf the above two lines have T's in the same column then (tbc) has failed.  ");
	} else {
		ptext("(tbc) Clear-all-tabs is not defined.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	subtest_hts(test_list, status, ch)
**
**	(ht) and set tabs with (hts)
*/
static void
subtest_hts(
	struct test_list *t,
	int *state,
	int *ch)
{
	int tabat;		/* the tab spacing we end up with */
	int i;

	tabat = init_tabs;
	if (set_tab) {
		ptext("Tabs set with (hts)");
		put_crlf();
		for (i = 1; i < columns; i++) {
			if (i % 8 == 1) {
				tc_putp(set_tab);
			}
			putchp(' ');
		}
		tabat = 8;
	} else {
		sprintf(temp, "(hts) Set-tabs not defined.  (it) Initial-tabs at %d", init_tabs);
		ptext(temp);
	}
	go_home();
	put_newlines(2);
	if (tabat <= 0) {
		tabat = 8;
	}
	for (i = tabat; i < columns; i += tabat) {
		putchp('\t');
		putchp('T');
	}
	go_home();
	put_newlines(3);
	for (i = 1; i < columns; i++) {
		putchp('.');
	}
	go_home();
	put_newlines(3);
	for (i = tabat; i < columns; i += tabat) {
		putchp('\t');
		putchp('T');
	}
	go_home();
	put_newlines(4);
	putchp('.');
	for (i = 2; i < columns; i++) {
		if (i % tabat == 1) {
			putchp('T');
		} else {
			putchp('.');
		}
	}
	go_home();
	put_newlines(5);
	if (set_tab) {
		ptextln("If the last two lines are not the same then (hts) has failed.");
	} else
	if (init_tabs > 0) {
		ptextln("If the last two lines are not the same then (it) is wrong.");
	} else {
		ptextln("If the last two lines are the same then maybe you do have tabs and (it) should be changed.");
	}
	generic_done_message(t, state, ch);
}

/*
**	subtest_xt(test_list, status, ch)
**
**	(xt) glitch
*/
static void
subtest_xt(
	struct test_list *t,
	int *state,
	int *ch)
{
	int tabat;		/* the tab spacing we end up with */
	int cc;

	tabat = set_tab ? 8 : init_tabs;
	if (!over_strike && (tabat > 0)) {
		ptext("(xt) should not ");
		put_cr();
		ptext("(xt) should");
		cc = char_count;
		while (cc < 16) {
			putchp('\t');
			cc = ((cc / tabat) + 1) * tabat;
		}
		putln("be set.");
		sprintf(temp, "(xt) Destructive-tab is %s in the data base.",
			dest_tabs_magic_smso ? "true" : "false");
		ptextln(temp);
		generic_done_message(t, state, ch);
	}
}

/*
**	subtest_cbt(test_list, status, ch)
**
**	(cbt) back tab
*/
static void
subtest_cbt(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i;

	if (back_tab) {
		put_clear();
		ptext("Back-tab (cbt)");
		go_home();
		put_crlf();
		for (i = 1; i < columns; i++) {
			putchp(' ');
		}
		for (i = 0; i < columns; i += 8) {
			tc_putp(back_tab);
			putchp('T');
			tc_putp(back_tab);
		}
		go_home();
		put_newlines(2);
		for (i = 1; i < columns; i++) {
			if (i % 8 == 1) {
				putchp('T');
			} else {
				putchp(' ');
			}
		}
		go_home();
		put_newlines(3);
		ptextln("The preceding two lines should be the same.");
	} else {
		ptextln("(cbt) Back-tab not present");
	}
	generic_done_message(t, state, ch);
}

/*
**	subtest_xenl(test_list, status, ch)
**
**	(xenl) eat newline glitch
*/
static void
subtest_xenl(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j, k;

	if (over_strike) {
		/* test (xenl) on overstrike terminals */
		if (!can_go_home || !can_clear_screen) {
			ptextln("(xenl) Newline-glitch not tested, can't home cursor and clear.");
			generic_done_message(t, state, ch);
			return;
		}
		put_clear();
		/*
		   this test must be done in raw mode.  Otherwise UNIX will
		   translate CR to CRLF.
		*/
		if (stty_query(TTY_OUT_TRANS))
			tty_raw(1, char_mask);
		ptext("\nreset (xenl). Does ");
		i = char_count;
		put_str("not ignore CR, does ");
		k = char_count;
		put_str("not ignore LF");
		go_home();
		for (j = 0; j < columns; j++)
			put_this(' ');
		put_cr();
		for (j = 0; j < i; j++)
			putchp(' ');
		put_str("@@@\n@@");
		go_home();
		for (j = 0; j < columns; j++)
			put_this(' ');
		put_lf();
		for (j = 0; j < k; j++)
			putchp(' ');
		put_str("@@@\r@@");
		tty_set();
		go_home();
		put_newlines(4);
		sprintf(temp, "(xenl) Newline-glitch is %s in the data base",
			eat_newline_glitch ? "true" : "false");
		ptextln(temp);
	} else {
		/* test (xenl) when (os) is reset */
		if (!can_go_home) {
			ptextln("(xenl) Newline-glitch not tested, can't home cursor");
			generic_done_message(t, state, ch);
			return;
		}
		/* (xenl) test */
		put_clear();
		/*
		   this test must be done in raw mode.  Otherwise
		   UNIX will translate CR to CRLF.
		*/
		if (stty_query(TTY_OUT_TRANS))
			tty_raw(1, char_mask);
		for (j = 0; j < columns; j++)
			put_this(' ');
		put_cr();
		ptext("(xenl) should be set. Does not ignore CR");
		go_home();
		put_crlf();
		for (j = 0; j < columns; j++)
			put_this(' ');
		put_lf();	/* test (cud1) */
		ptext("(xenl) should be set. Ignores (cud1)");
		go_home();
		put_newlines(3);
		if (scroll_forward && cursor_down &&
			strcmp(scroll_forward, cursor_down)) {
			for (j = 0; j < columns; j++)
				put_this(' ');
			put_ind();	/* test (ind) */
			ptext("(xenl) should be set. Ignores (ind)");
			go_home();
			put_newlines(5);
		}
		tty_set();
		ptextln("If you don't see text above telling you to set it, (xenl) should be false");
		sprintf(temp, "(xenl) Newline-glitch is %s in the data base",
			eat_newline_glitch ? "true" : "false");
		ptextln(temp);
	}
	generic_done_message(t, state, ch);
}

/*
**	subtest_eo(test_list, status, ch)
**
**	(eo) erase overstrike
*/
static void
subtest_eo(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (transparent_underline || over_strike || underline_char) {
		ptext("(eo) should ");
		if (underline_char) {
			ucprint("not");
		} else {
			uprint("not");
		}
		put_cr();
		ptextln("(eo) should     be set");
		sprintf(temp, "\n(eo) Erase-overstrike is %s in the data base",
			erase_overstrike ? "true" : "false");
		ptextln(temp);
		generic_done_message(t, state, ch);
	}
}

/*
**	subtest_xmc(test_list, status, ch)
**
**	(xmc) magic cookie glitch
*/
static void
subtest_xmc(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j;

	if (enter_standout_mode) {
		sprintf(temp, "\n(xmc) Magic-cookie-glitch is %d in the data base", magic_cookie_glitch);
		ptextln(temp);
		j = magic_cookie_glitch * 8;
		for (i = 0; i < j; i++) {
			put_str(" ");
		}
		ptextln("        These two lines should line up.");
		if (j > 0) {
			char_count += j;
		}
		for (i = 0; i < 4; i++) {
			put_mode(enter_standout_mode);
			putchp(' ');
			put_mode(exit_standout_mode);
			putchp(' ');
		}
		ptextln("These two lines should line up.");
		ptext("If they don't line up then (xmc) magic-cookie-glitch should be greater than zero.  ");
		generic_done_message(t, state, ch);
	}
}

/*
**	subtest_xhp(test_list, status, ch)
**
**	(xhp) erase does not clear standout mode
*/
static void
subtest_xhp(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (enter_standout_mode) {
		put_crlf();
		put_mode(enter_standout_mode);
		put_str("Stand out");
		put_mode(exit_standout_mode);
		put_cr();
		ptextln("If any part of this line is standout then (xhp) should be set.");
		sprintf(temp, "(xhp) Erase-standout-glitch is %s in the data base",
			ceol_standout_glitch ? "true" : "false");
		ptextln(temp);
		generic_done_message(t, state, ch);
	}
}

/*
**	subtest_mir(test_list, status, ch)
**
**	(mir) move in insert mode
*/
static void
subtest_mir(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i;
	char *s;

	if (enter_insert_mode && exit_insert_mode && cursor_address) {
		put_clear();
		i = line_count;
		put_str("\nXXX\nXXX\nXXX\nXXX");
		tc_putp(enter_insert_mode);
		s = tparm(cursor_address, i + 1, 0);
		tputs(s, lines, tc_putch);
		putchp('X');
		s = tparm(cursor_address, i + 2, 1);
		tputs(s, lines, tc_putch);
		putchp('X');
		s = tparm(cursor_address, i + 3, 2);
		tputs(s, lines, tc_putch);
		putchp('X');
		s = tparm(cursor_address, i + 4, 3);
		tputs(s, lines, tc_putch);
		putchp('X');
		tc_putp(exit_insert_mode);
		put_newlines(2);
		ptextln("If you see a 4 by 4 block of X's then (mir) should be true.");
		sprintf(temp, "(mir) Move-in-insert-mode is %s in the data base",
			move_insert_mode ? "true" : "false");
		ptextln(temp);
	} else {
		ptext("(mir) Move-in-insert-mode not tested, ");
		if (!enter_insert_mode) {
			ptext("(smir) ");
		}
		if (!exit_insert_mode) {
			ptext("(rmir) ");
		}
		if (!cursor_address) {
			ptext("(cup) ");
		}
		ptext("not present.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	subtest_msgr(test_list, status, ch)
**
**	(msgr) move in sgr mode
*/
static void
subtest_msgr(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i;

	if (cursor_address &&
		((enter_standout_mode && exit_standout_mode) ||
		(enter_alt_charset_mode && exit_alt_charset_mode))) {
		put_crlf();
		i = line_count + 1;
		tputs(tparm(cursor_address, i, 0), lines, tc_putch);
		put_mode(enter_alt_charset_mode);
		put_crlf();
		/*
		   some versions of the wy-120 can not clear lines or
		   screen when in alt charset mode.  If (el) and (ed)
		   are defined then I can test them.  If they are not
		   defined then they can not break (msgr)
		*/
		tc_putp(clr_eos);
		tc_putp(clr_eol);
		put_mode(exit_alt_charset_mode);
		put_mode(enter_standout_mode);
		putchp('X');
		tputs(tparm(cursor_address, i + 2, 1), lines, tc_putch);
		putchp('X');
		tputs(tparm(cursor_address, i + 3, 2), lines, tc_putch);
		putchp('X');
		tputs(tparm(cursor_address, i + 4, 3), lines, tc_putch);
		putchp('X');
		put_mode(exit_standout_mode);
		put_crlf();
		tc_putp(clr_eos);	/* OK if missing */
		put_crlf();
		ptextln("If you see a diagonal line of standout X's then (msgr) should be true.  If any of the blanks are standout then (msgr) should be false.");
		sprintf(temp, "(msgr) Move-in-SGR-mode is %s in the data base",
			move_standout_mode ? "true" : "false");
		ptextln(temp);
	} else {
		ptextln("(smso) (rmso) (smacs) (rmacs) missing; (msgr) Move-in-SGR-mode not tested.");
	}
	generic_done_message(t, state, ch);
}

/*
**	subtest_in(test_list, status, ch)
**
**	(in) insert null glitch
*/
static void
subtest_in(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (enter_insert_mode && exit_insert_mode) {
		ptextln("\nTesting (in) with (smir) and (rmir)");
		putln("\tIf these two lines line up ...");
		put_str("\tIf these two lines line up ...");
		put_cr();
		tc_putp(enter_insert_mode);
		putchp(' ');
		tc_putp(exit_insert_mode);
		ptext("\nthen (in) should be set.  ");
		sprintf(temp,
			"(in) Insert-null-glitch is %s in the data base.",
			insert_null_glitch ? "true" : "false");
		ptextln(temp);
		generic_done_message(t, state, ch);
	}
}

/*
**	subtest_dadb(test_list, status, ch)
**
**	(da) (db) data above, (db) data below
*/
static void
subtest_dadb(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (can_clear_screen && scroll_reverse && scroll_forward) {
		put_clear();
		if (scroll_reverse)
			ptext("(da) Data-above should be set\r");
		home_down();
		if (scroll_forward)
			ptext("(db) Data-below should be set\r");
		tc_putp(scroll_forward);
		go_home();
		tc_putp(scroll_reverse);
		tc_putp(scroll_reverse);
		home_down();
		tc_putp(scroll_forward);
		go_home();
		ptextln("\n\n\n\n\nIf the top line is blank then (da) should be false.");
		ptextln("If the bottom line is blank then (db) should be false.");
		sprintf(temp, "\n(da) Data-above is %s, and (db) Data-below is %s, in the data base.",
			memory_above ? "true" : "false",
			memory_below ? "true" : "false");
		ptextln(temp);
		line_count = lines;
	} else {
		ptextln("(da) Data-above, (db) Data-below not tested, scrolls or (clear) is missing.");
	}
	generic_done_message(t, state, ch);
}
