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
#include <time.h>

MODULE_ID("$Id: sync.c,v 1.2 2000/03/04 20:28:16 tom Exp $")

/* terminal-synchronization and performance tests */

static void sync_home(struct test_list *, int *, int *);
static void sync_lines(struct test_list *, int *, int *);
static void sync_clear(struct test_list *, int *, int *);
static void sync_summary(struct test_list *, int *, int *);

struct test_list sync_test_list[] = {
	{MENU_NEXT, 0, 0, 0, "b) baud rate test", sync_home, 0},
	{MENU_NEXT, 0, 0, 0, "l) scroll performance", sync_lines, 0},
	{MENU_NEXT, 0, 0, 0, "c) clear screen performance", sync_clear, 0},
	{MENU_NEXT, 0, 0, 0, "p) summary of results", sync_summary, 0},
	{0, 0, 0, 0, txt_longer_test_time, longer_test_time, 0},
	{0, 0, 0, 0, txt_shorter_test_time, shorter_test_time, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

struct test_menu sync_menu = {
	0, 'n', 0,
	"Performance tests", "perf", "n) run standard tests",
	sync_test, sync_test_list, 0, 0, 0
};

int tty_can_sync;		/* TRUE if tty_sync_error() returned FALSE */
int tty_newline_rate;		/* The number of newlines per second */
int tty_clear_rate;		/* The number of clear-screens per second */
int tty_cps;			/* The number of characters per second */

#define TTY_ACK_SIZE 64

int ACK_terminator;		/* terminating ACK character */
int ACK_length;			/* length of ACK string */
const char *tty_ENQ;		/* enquire string */
char tty_ACK[TTY_ACK_SIZE];	/* ACK response, set by tty_sync_error() */

/*****************************************************************************
 *
 * Terminal synchronization.
 *
 *	These functions handle the messy business of enq-ack handshaking
 *	for timing purposes.
 *
 *****************************************************************************/

int
tty_sync_error(void)
{
	int ch, trouble, ack;

	trouble = FALSE;
	for (;;) {
		tt_putp(tty_ENQ);	/* send ENQ */
		ch = getnext(STRIP_PARITY);
		event_start(TIME_SYNC);	/* start the timer */

		/*
		   The timer doesn't start until we get the first character.
		   After that I expect to get the remaining characters of
		   the acknowledge string in a short period of time.  If
		   that is not true then these characters are coming from
		   the user and we need to send the ENQ sequence out again.
		*/
		for (ack = 0; ; ) {
			if (ack < TTY_ACK_SIZE - 2) {
				tty_ACK[ack] = ch;
				tty_ACK[ack + 1] = '\0';
			}
			if (ch == ACK_terminator) {
				return trouble;
			}
			if (++ack >= ACK_length) {
				return trouble;
			}
			ch = getnext(STRIP_PARITY);
			if (event_time(TIME_SYNC) > 400000) {
				break;
			}
		}

		set_attr(0);	/* just in case */
		put_crlf();
		if (trouble) {
			/* The terminal won't sync.  Life is not good. */
			return TRUE;
		}
		put_str(" -- sync -- ");
		trouble = TRUE;
	}
}

/*
**	flush_input()
**
**	Throw away any output.
*/
void 
flush_input(void)
{
	if (tty_can_sync == SYNC_TESTED && ACK_terminator >= 0) {
		(void) tty_sync_error();
	} else {
		spin_flush();
	}
}

/*
**	probe_enq_ok()
**
**	does the terminal do enq/ack handshaking?
*/
static void 
probe_enq_ok(void)
{
	int tc, len, ulen;

	put_str("Testing ENQ/ACK, standby...");
	fflush(stdout);
	can_test("u8 u9", FLAG_TESTED);

	tty_ENQ = user9 ? user9 : "\005";
	tc_putp(tty_ENQ);
	event_start(TIME_SYNC);	/* start the timer */
	read_key(tty_ACK, TTY_ACK_SIZE - 1);

	if (event_time(TIME_SYNC) > 400000 || tty_ACK[0] == '\0') {
		/* These characters came from the user.  Sigh. */
		tty_can_sync = SYNC_FAILED;
		ptext("\nThis program expects the ENQ sequence to be");
		ptext(" answered with the ACK character.  This will help");
		ptext(" the program reestablish synchronization when");
		ptextln(" the terminal is overrun with data.");
		ptext("\nENQ sequence from (u9): ");
		putln(expand(tty_ENQ));
		ptext("ACK received: ");
		putln(expand(tty_ACK));
		len = user8 ? strlen(user8) : 0;
		sprintf(temp, "Length of ACK %d.  Expected length of ACK %d.",
			(int) strlen(tty_ACK), len);
		ptextln(temp);
		if (len) {
			temp[0] = user8[len - 1];
			temp[1] = '\0';
			ptext("Terminating character found in (u8): ");
			putln(expand(temp));
		}
		return;
	}

	tty_can_sync = SYNC_TESTED;
	if ((len = strlen(tty_ACK)) == 1) {
		/* single character acknowledge string */
		ACK_terminator = tty_ACK[0];
		ACK_length = 4096;
		return;
	}
	tc = tty_ACK[len - 1];
	if (user8) {
		ulen = strlen(user8);
		if (tc == user8[ulen - 1]) {
			/* ANSI style acknowledge string */
			ACK_terminator = tc;
			ACK_length = 4096;
			return;
		}
	}
	/* fixed length acknowledge string */
	ACK_length = len;
	ACK_terminator = -2;
}

/*
**	verify_time()
**
**	verify that the time tests are ready to run.
**	If the baud rate is not set then compute it.
*/
void
verify_time(void)
{
	int status, ch;

	if (tty_can_sync == SYNC_FAILED) {
		return;
	}
	probe_enq_ok();
	put_crlf();
	if (tty_can_sync == SYNC_TESTED) {
		put_crlf();
		if (ACK_terminator >= 0) {
			ptext("ACK terminating character: ");
			temp[0] = ACK_terminator;
			temp[1] = '\0';
			ptextln(expand(temp));
		} else {
			sprintf(temp, "Fixed length ACK, %d characters",
				ACK_length);
			ptextln(temp);
		}
	}
	if (tty_baud_rate == 0) {
		sync_home(&sync_test_list[0], &status, &ch);
	}
}

/*****************************************************************************
 *
 * Terminal performance tests
 *
 *	Find out how fast the terminal can:
 *	  1) accept characters
 *	  2) scroll the screen
 *	  3) clear the screen
 *
 *****************************************************************************/

/*
**	sync_home(test_list, status, ch)
**
**	Baudrate test
*/
void
sync_home(
	struct test_list *t,
	int *state,
	int *ch)
{
	int j, k;
	unsigned long rate;

	if (!cursor_home && !cursor_address && !row_address) {
		ptext("Terminal can not home cursor.  ");
		generic_done_message(t, state, ch);
		return;
	}
	if (skip_pad_test(t, state, ch,
		"(home) Start baudrate search")) {
		return;
	}
	pad_test_startup(1);
	do {
		go_home();
		for (j = 1; j < lines; j++) {
			for (k = 0; k < columns; k++) {
				if (k & 0xF) {
					put_this(letter);
				} else {
					put_this('.');
				}
			}
			SLOW_TERMINAL_EXIT;
		}
		NEXT_LETTER;
	} while(still_testing());
	pad_test_shutdown(t, auto_right_margin == 0);
	/* note:  tty_frame_size is the real framesize times two.
	   This takes care of half bits. */
	rate = (tx_cps * tty_frame_size) >> 1;
	if (rate > tty_baud_rate) {
		tty_baud_rate = rate;
	}
	if (tx_cps > tty_cps) {
		tty_cps = tx_cps;
	}
	sprintf(temp, "%d characters per second.  Baudrate %d  ", tx_cps, j);
	ptext(temp);
	generic_done_message(t, state, ch);
}

/*
**	sync_lines(test_list, status, ch)
**
**	How many newlines/second?
*/
static void
sync_lines(
	struct test_list *t,
	int *state,
	int *ch)
{
	int j;

	if (skip_pad_test(t, state, ch,
		"(nel) Start scroll performance test")) {
		return;
	}
	pad_test_startup(0);
	repeats = 100;
	do {
		sprintf(temp, "%d", test_complete);
		put_str(temp);
		put_newlines(repeats);
	} while(still_testing());
	pad_test_shutdown(t, 0);
	j = sliding_scale(tx_count[0], 1000000, usec_run_time);
	if (j > tty_newline_rate) {
		tty_newline_rate = j;
	}
	sprintf(temp, "%d linefeeds per second.  ", j);
	ptext(temp);
	generic_done_message(t, state, ch);
}

/*
**	sync_clear(test_list, status, ch)
**
**	How many clear-screens/second?
*/
static void
sync_clear(
	struct test_list *t,
	int *state,
	int *ch)
{
	int j;

	if (!clear_screen) {
		ptext("Terminal can not clear-screen.  ");
		generic_done_message(t, state, ch);
		return;
	}
	if (skip_pad_test(t, state, ch,
		"(clear) Start clear-screen performance test")) {
		return;
	}
	pad_test_startup(0);
	repeats = 20;
	do {
		sprintf(temp, "%d", test_complete);
		put_str(temp);
		for (j = 0; j < repeats; j++) {
			put_clear();
		}
	} while(still_testing());
	pad_test_shutdown(t, 0);
	j = sliding_scale(tx_count[0], 1000000, usec_run_time);
	if (j > tty_clear_rate) {
		tty_clear_rate = j;
	}
	sprintf(temp, "%d clear-screens per second.  ", j);
	ptext(temp);
	generic_done_message(t, state, ch);
}

/*
**	sync_summary(test_list, status, ch)
**
**	Print out the test results.
*/
static void
sync_summary(
	struct test_list *t,
	int *state,
	int *ch)
{
	char size[32];

	put_crlf();
	ptextln("Terminal  size    characters/sec linefeeds/sec  clears/sec");
	sprintf(size, "%dx%d", columns, lines);
	sprintf(temp, "%-10s%-11s%11d   %11d %11d", tty_basename, size,
		tty_cps, tty_newline_rate, tty_clear_rate);
	ptextln(temp);
	generic_done_message(t, state, ch);
}

/*
**	sync_test(menu)
**
**	Run at the beginning of the pad tests and function key tests
*/
void
sync_test(
	struct test_menu *menu)
{
	control_init();
	if (tty_can_sync == SYNC_NOT_TESTED) {
		verify_time();
	}
	if (menu->menu_title) {
		put_crlf();
		ptextln(menu->menu_title);
	}
}

/*
**	sync_handshake(test_list, status, ch)
**
**	Test or retest the ENQ/ACK handshake
*/
void
sync_handshake(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	tty_can_sync = SYNC_NOT_TESTED;
	verify_time();
}
