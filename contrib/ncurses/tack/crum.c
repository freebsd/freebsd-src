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

MODULE_ID("$Id: crum.c,v 1.2 1999/08/21 23:09:08 tom Exp $")

/*
 * Test cursor movement.
 */

static void crum_clear(struct test_list *t, int *state, int *ch);
static void crum_home(struct test_list *t, int *state, int *ch);
static void crum_ll(struct test_list *t, int *state, int *ch);
static void crum_move(struct test_list *t, int *state, int *ch);
static void crum_os(struct test_list *t, int *state, int *ch);

static char crum_text[5][80];

struct test_list crum_test_list[] = {
	{0, 0, 0, 0, "e) edit terminfo", 0, &edit_menu},
	{MENU_NEXT, 0, "clear", 0, 0, crum_clear, 0},
	{MENU_NEXT, 0, "home", 0, 0, crum_home, 0},
	{MENU_NEXT, 0, "ll", 0, 0, crum_ll, 0},
	{MENU_NEXT, 0, crum_text[0], "home cuu1", 0, crum_move, 0},
	{MENU_NEXT + 1, 0, crum_text[1], "cub1 cud1 cuf1 cuu1", 0, crum_move, 0},
	{MENU_NEXT + 2, 0, crum_text[2], "cub cud cuf cuu", 0, crum_move, 0},
	{MENU_NEXT + 3, 0, crum_text[3], "vpa hpa", 0, crum_move, 0},
	{MENU_NEXT + 4, 0, crum_text[4], "cup", 0, crum_move, 0},
	{MENU_NEXT, 0, "cup", "os", 0, crum_os, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

/*
**	move_to(from-row, from-column, to-row, to-column, selection)
**
**	move the cursor from (rf, cf) to (rt, ct) using sel
*/
static void
move_to(
	int rf,
	int cf,
	int rt,
	int ct,
	int sel)
{
	char *s;

	if (sel & 16) {	/* use (cup) */
		s = tparm(cursor_address, rt, ct);
		tputs(s, lines, tc_putch);
		return;
	}
	if (sel & 8) {	/* use (hpa) (vpa) */
		if (column_address) {
			s = tparm(column_address, ct);
			tputs(s, 1, tc_putch);
			cf = ct;
		}
		if (row_address) {
			s = tparm(row_address, rt);
			tputs(s, 1, tc_putch);
			rf = rt;
		}
	}
	if (sel & 4) {	/* paramiterized relative cursor movement */
		if (parm_right_cursor)
			if (cf < ct) {
				s = tparm(parm_right_cursor, ct - cf);
				tputs(s, ct - cf, tc_putch);
				cf = ct;
			}
		if (parm_left_cursor)
			if (cf > ct) {
				s = tparm(parm_left_cursor, cf - ct);
				tputs(s, cf - ct, tc_putch);
				cf = ct;
			}
		if (parm_down_cursor)
			if (rf < rt) {
				s = tparm(parm_down_cursor, rt - rf);
				tputs(s, rt - rf, tc_putch);
				rf = rt;
			}
		if (parm_up_cursor)
			if (rf > rt) {
				s = tparm(parm_up_cursor, rf - rt);
				tputs(s, rf - rt, tc_putch);
				rf = rt;
			}
	}
	if (sel & 2) {
		if (cursor_left)
			while (cf > ct) {
				tc_putp(cursor_left);
				cf--;
			}
		/*
		   do vertical motion next.  Just in case cursor_down has a
		   side effect of changing the column.  This could happen if
		   the tty handler translates NL to CRNL.
		*/
		if (cursor_down)
			while (rf < rt) {
				tc_putp(cursor_down);
				rf++;
			}
		if (cursor_up)
			while (rf > rt) {
				tc_putp(cursor_up);
				rf--;
			}
		if (cursor_right)
			while (cf < ct) {
				tc_putp(cursor_right);
				cf++;
			}
	}
	/* last chance */
	if (rf > rt) {
		if (can_go_home) {	/* a bit drastic but ... */
			go_home();
			cf = 0;
			rf = 0;
		} else if (cursor_up) {
			while (rf > rt) {
				tc_putp(cursor_up);
				rf--;
			}
		}
	}
	if (ct == 0 && rt > rf) {
		put_crlf();
		cf = 0;
		rf++;
	}
	if (ct == 0 && cf != 0) {
		put_cr();
		cf = 0;
	}
	while (rf < rt) {
		put_lf();
		rf++;
	}
	while (cf > ct) {
		put_str("\b");
		cf--;
	}
	if (cursor_right) {
		while (cf < ct) {
			tc_putp(cursor_right);
			cf++;
		}
	} else {
		/* go ahead and trash my display */
		while (cf < ct) {
			putchp(' ');
			cf++;
		}
	}
}

/*
**	display_it(selection, text)
**
**	print the display using sel
*/
static void
display_it(
	int sel,
	char *txt)
{
	int i, done_line;

	put_clear();
	go_home();
	put_newlines(2);
	ptextln("    The top line should be alternating <'s and >'s");
	ptextln("    The left side should be alternating A's and V's");
	ptext("    Testing ");
	ptext(txt);
	put_cr();

	/* horizontal */
	move_to(done_line = line_count, 0, 0, 2, sel);
	for (i = 4; i < columns - 2; i += 2) {
		putchp('>');
		move_to(0, i - 1, 0, i, sel);
	}
	putchp('>');
	i -= 2;
	move_to(0, i + 1, 0, i - 1, sel);
	for (; i > 2; i -= 2) {
		putchp('<');
		move_to(0, i, 0, i - 3, sel);
	}
	putchp('<');

	/* vertical */
	move_to(0, 2, 0, 0, sel);
	for (i = 2; i < lines - 1; i += 2) {
		putchp('V');
		move_to(i - 2, 1, i, 0, sel);
	}
	putchp('V');
	i -= 2;
	move_to(i, 1, i + 1, 0, sel);
	for (; i > 0; i -= 2) {
		putchp('A');
		move_to(i + 1, 1, i - 1, 0, sel);
	}
	putchp('A');
	move_to(i + 1, 1, 0, 0, sel);	/* go home first */
	move_to(0, 0, done_line + 1, 3, sel);
	put_str(txt);
	put_str(" Done. ");
}

/*
**	crum_clear(test_list, status, ch)
**
**	(clear) test Clear screen
*/
static void
crum_clear(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i;

	if (clear_screen) {
		for (i = lines; i > 1; i--) {
			putln("garbage");
		}
		put_clear();
		ptextln("This line should start in the home position.");
		ptext("The rest of the screen should be clear.  ");
	} else {
		ptextln("(clear) Clear screen is not defined.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	crum_home(test_list, status, ch)
**
**	(home) test Home cursor
*/
static void
crum_home(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (cursor_home) {
		put_clear();
		put_newlines(lines / 2);
		go_home();
		put_crlf();
		ptext("The bottom line should have text.");
		go_home();
		put_newlines(lines - 1);
		ptext("This line is on the bottom.");
		go_home();
		ptextln("This line starts in the home position.");
		put_crlf();
	} else {
		ptextln("(home) Home cursor is not defined.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	crum_ll(test_list, status, ch)
**
**	(ll) test Last line
*/
static void
crum_ll(
	struct test_list *t,
	int *state,
	int *ch)
{
	/*
	   (ll) may be simulated with (cup).  Don't complain if (cup) is present.
	*/
	if (cursor_to_ll) {
		put_clear();
		put_str("This line could be anywhere.");
		tc_putp(cursor_to_ll);
		ptext("This line should be on the bottom");
		go_home();
		put_crlf();
	} else
	if (cursor_address) {
		return;
	} else {
		ptextln("(ll) Move to last line is not defined.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	crum_move(test_list, status, ch)
**
**	(*) test all cursor move commands
*/
static void
crum_move(
	struct test_list *t,
	int *state,
	int *ch)
{
	char buf[80];
	int n;

	switch (n = (t->flags & 15)) {
	case 0:
		sprintf(buf, " (cr) (nel) (cub1)%s",
			cursor_home ? " (home)" : (cursor_up ? " (cuu1)" : ""));
		break;
	case 1:
		sprintf(buf, "%s%s%s%s", cursor_left ? " (cub1)" : "",
			cursor_down ? " (cud1)" : "", cursor_right ? " (cuf1)" : "",
			cursor_up ? " (cuu1)" : "");
		if (buf[0] == '\0') {
			ptext("    (cub1) (cud1) (cuf1) (cuu1) not defined.");
		}
		break;
	case 2:
		sprintf(buf, "%s%s%s%s", parm_left_cursor ? " (cub)" : "",
			parm_down_cursor ? " (cud)" : "",
			parm_right_cursor ? " (cuf)" : "",
			parm_up_cursor ? " (cuu)" : "");
		if (buf[0] == '\0') {
			ptext("    (cub) (cud) (cuf) (cuu) not defined.");
		}
		break;
	case 3:
		sprintf(buf, "%s%s", row_address ? " (vpa)" : "",
			column_address ? " (hpa)" : "");
		if (buf[0] == '\0') {
			ptext("    (vpa) (hpa) not defined.");
		}
		break;
	case 4:
		if (!cursor_address) {
			ptext("    (cup) not defined.  ");
			generic_done_message(t, state, ch);
			return;
		}
		strcpy(buf, " (cup)");
		break;
	}
	if (buf[0] == '\0') {
		put_str("  Done. ");
	} else {
		can_test(buf, FLAG_TESTED);
		strcpy(crum_text[n], &buf[2]);
		crum_text[n][strlen(buf) - 3] = '\0';

		display_it(1 << n, buf);
	}
	*ch = wait_here();
	if (*ch != 'r') {
		put_clear();
	}
}

/*
**	crum_os(test_list, status, ch)
**
**	(cup) test Cursor position on overstrike terminals
*/
static void
crum_os(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i;

	if (cursor_address && over_strike) {
		put_clear();
		for (i = 0; i < columns - 2; i++) {
			tc_putch('|');
		}
		for (i = 1; i < lines - 2; i++) {
			put_crlf();
			tc_putch('_');
		}
		for (i = 0; i < columns - 2; i++) {
			tputs(tparm(cursor_address, 0, i), lines, tc_putch);
			tc_putch('+');
		}
		for (i = 0; i < lines - 2; i++) {
			tputs(tparm(cursor_address, i, 0), lines, tc_putch);
			tc_putch(']');
			tc_putch('_');
		}
		go_home();
		put_newlines(3);
		ptext("    All the characters should look the same.  ");
		generic_done_message(t, state, ch);
		put_clear();
	}
}
