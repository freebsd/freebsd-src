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

#if HAVE_SYS_TIME_H
#include <sys/time.h>
#endif

MODULE_ID("$Id: control.c,v 1.3 2000/03/04 21:10:59 tom Exp $")

/* terminfo test program control subroutines */

#if HAVE_GETTIMEOFDAY
#define MY_TIMER struct timeval
#else
#define MY_TIMER time_t
#endif

/* globals */
int test_complete;		/* counts number of tests completed */

char txt_longer_test_time[80];	/* +) use longer time */
char txt_shorter_test_time[80];	/* -) use shorter time */
int pad_test_duration = 1;	/* number of seconds for a pad test */
int auto_pad_mode;		/* run the time tests */
int no_alarm_event;		/* TRUE if the alarm has not gone off yet */
int usec_run_time;		/* length of last test in microseconds */
MY_TIMER stop_watch[MAX_TIMERS]; /* Hold the start timers */

char txt_longer_augment[80];	/* >) use bigger augment */
char txt_shorter_augment[80];	/* <) use smaller augment */

/* caps under test data base */
int tt_delay_max;		/* max number of milliseconds we can delay */
int tt_delay_used;		/* number of milliseconds consumed in delay */
const char *tt_cap[TT_MAX];	/* value of string */
int tt_affected[TT_MAX];	/* lines or columns effected (repetition factor) */
int tt_count[TT_MAX];		/* Number of times sent */
int tt_delay[TT_MAX];		/* Number of milliseconds delay */
int ttp;			/* number of entries used */

/* Saved value of the above data base */
const char *tx_cap[TT_MAX];	/* value of string */
int tx_affected[TT_MAX];	/* lines or columns effected (repetition factor) */
int tx_count[TT_MAX];		/* Number of times sent */
int tx_index[TT_MAX];		/* String index */
int tx_delay[TT_MAX];		/* Number of milliseconds delay */
int txp;			/* number of entries used */
int tx_characters;		/* printing characters sent by test */
int tx_cps;			/* characters per second */
struct test_list *tx_source;	/* The test that generated this data */

extern struct test_menu pad_menu;	/* Pad menu structure */
extern struct test_list pad_test_list[];

#define RESULT_BLOCK		1024
static int blocks;		/* number of result blocks available */
static struct test_results *results;	/* pointer to next available */
struct test_results *pads[STRCOUNT];	/* save pad results here */

/*
**	event_start(number)
**
**	Begin the stopwatch at the current time-of-day.
*/
void
event_start(int n)
{
#if HAVE_GETTIMEOFDAY
	(void) gettimeofday(&stop_watch[n], (struct timezone *)0);
#else
	stop_watch[n] = time((time_t *)0);
#endif
}

/*
**	event_time(number)
**
**	Return the number of milliseconds since this stop watch began.
*/
long
event_time(int n)
{
#if HAVE_GETTIMEOFDAY
	MY_TIMER current_time;

	(void) gettimeofday(&current_time, (struct timezone *)0);
	return ((current_time.tv_sec - stop_watch[n].tv_sec) * 1000000)
		+ current_time.tv_usec - stop_watch[n].tv_usec;
#else
	return (time((time_t *)0) - stop_watch[n]) * 1000;
#endif
}

/*****************************************************************************
 *
 * Execution control for string capability tests
 *
 *****************************************************************************/

/*
**	get_next_block()
**
**	Get a results block for pad test data.
*/
static struct test_results *
get_next_block(void)
{
	if (blocks <= 0) {
		results = (struct test_results *)
			malloc(sizeof(struct test_results) * RESULT_BLOCK);
		if (!results) {
			ptextln("Malloc failed");
			return (struct test_results *) 0;
		}
		blocks = RESULT_BLOCK;
	}
	blocks--;
	return results++;
}

/*
**	set_augment_txt()
**
**	Initialize the augment menu selections
*/
void
set_augment_txt(void)
{
	sprintf(txt_longer_augment,
		">) Change lines/characters effected to %d", augment << 1);
	sprintf(txt_shorter_augment,
		"<) Change lines/characters effected to %d", augment >> 1);
}

void
control_init(void)
{
	sprintf(txt_longer_test_time, "+) Change test time to %d seconds",
		pad_test_duration + 1);
	sprintf(txt_shorter_test_time, "-) Change test time to %d seconds",
		pad_test_duration - 1);
	set_augment_txt();
}

/*
**	msec_cost(cap, affected-count)
**
**	Return the number of milliseconds delay needed by the cap.
*/
int
msec_cost(
	const char *const cap,
	int affcnt)
{
	int dec, value, total, star, ch;
	const char *cp;

	if (!cap) {
		return 0;
	}
	total = 0;
	for (cp = cap; *cp; cp++) {
		if (*cp == '$' && cp[1] == '<') {
			star = 1;
			value = dec = 0;
			for (cp += 2; (ch = *cp); cp++) {
				if (ch >= '0' && ch <= '9') {
					value = value * 10 + (ch - '0');
					dec *= 10;
				} else
				if (ch == '.') {
					dec = 1;
				} else
				if (ch == '*') {
					star = affcnt;
				} else
				if (ch == '>') {
					break;
				}
			}
			if (dec > 1) {
				total += (value * star) / dec;
			} else {
				total += (value * star);
			}
		}
	}
	return total;
}

/*
**	liberated(cap)
**
**	Return the cap without padding
*/
char *
liberated(char *cap)
{
	static char cb[1024];
	char *ts, *ls;

	cb[0] = '\0';
	ls = NULL;
	if (cap) {
		for (ts = cb; (*ts = *cap); ++cap) {
			if (*cap == '$' && cap[1] == '<') {
				ls = ts;
			}
			++ts;
			if (*cap == '>') {
				if (ls) {
					ts = ls;
					ls = NULL;
				}
			}
		}
	}
	return cb;
}

/*
**	page_loop()
**
**	send CR/LF or go home and bump letter
*/
void
page_loop(void)
{
	if (line_count + 2 >= lines) {
		NEXT_LETTER;
		go_home();
	} else {
		put_crlf();
	}
}

/*
**	skip_pad_test(test-list-entry, state, ch, text)
**
**	Print the start test line.  Handle start up commands.
**	Return TRUE if a return is requested.
*/
int
skip_pad_test(
	struct test_list *test,
	int *state,
	int *ch,
	const char *text)
{
	char rep_text[16];

	while(1) {
		if (text) {
			ptext(text);
		}
		if ((test->flags & MENU_LC_MASK)) {
			sprintf(rep_text, " *%d", augment);
			ptext(rep_text);
		}
		ptext(" [n] > ");
		*ch = wait_here();
		if (*ch == 's') {
			/* Skip is converted to next */
			*ch = 'n';
			return TRUE;
		}
		if (*ch == 'q') {
			/* Quit is converted to help */
			*ch = '?';
			return TRUE;
		}
		if (*ch == '\r' || *ch == '\n' || *ch == 'n' || *ch == 'r') {
			/* this is the only response that allows the test to run */
			*ch = 0;
		}
		if (subtest_menu(pad_test_list, state, ch)) {
			continue;
		}
		return (*ch != 0);
	}
}

/*
**	pad_done_message(test_list)
**
**	Print the Done message and request input.
*/
void
pad_done_message(
	struct test_list *test,
	int *state,
	int *ch)
{
	int default_action = 0;
	char done_message[128];
	char rep_text[16];

	while (1) {
		if ((test->flags & MENU_LC_MASK)) {
			sprintf(rep_text, "*%d", augment);
		} else {
			rep_text[0] = '\0';
		}
		if (test->caps_done) {
			sprintf(done_message, "(%s)%s Done ", test->caps_done,
			rep_text);
			ptext(done_message);
		} else {
			if (rep_text[0]) {
				ptext(rep_text);
				ptext(" ");
			}
			ptext("Done ");
		}
		if (debug_level & 2) {
			dump_test_stats(test, state, ch);
		} else {
			*ch = wait_here();
		}
		if (*ch == '\r' || *ch == '\n') {
			*ch = default_action;
			return;
		}
		if (*ch == 's' || *ch == 'n') {
			*ch = 0;
			return;
		}
		if (strchr(pad_repeat_test, *ch)) {
			/* default action is now repeat */
			default_action = 'r';
		}
		if (subtest_menu(pad_test_list, state, ch)) {
			continue;
		}
		return;
	}
}

/*
**	sliding_scale(dividend, factor, divisor)
**
**	Return (dividend * factor) / divisor
*/
int
sliding_scale(
	int dividend,
	int factor,
	int divisor)
{
	double d = dividend;

	if (divisor) {
		d = (d * (double) factor) / (double) divisor;
		return (int) (d + 0.5);
	}
	return 0;
}

/*
**	pad_test_startup()
**
**	Do the stuff needed to begin a test.
*/
void
pad_test_startup(
	int do_clear)
{
	if (do_clear) {
		put_clear();
	}
	repeats = augment;
	raw_characters_sent = 0;
	test_complete = ttp = char_count = tt_delay_used = 0;
	letter = letters[letter_number = 0];
	if (pad_test_duration <= 0) {
		pad_test_duration = 1;
	}
	tt_delay_max = pad_test_duration * 1000;
	set_alarm_clock(pad_test_duration);
	event_start(TIME_TEST);
}

/*
**	still_testing()
**
**	This function is called to see if the test loop should be terminated.
*/
int
still_testing(void)
{
	fflush(stdout);
	test_complete++;
	return EXIT_CONDITION;
}

/*
**	pad_test_shutdown()
**
**	Do the stuff needed to end a test.
*/
void
pad_test_shutdown(
	struct test_list *t,
	int crlf)
{
	int i;
	int counts;			/* total counts */
	int ss;				/* Save string index */
	int cpo;			/* characters per operation */
	int delta;			/* difference in characters */
	int bogus;			/* Time is inaccurate */
	struct test_results *r;		/* Results of current test */
	int ss_index[TT_MAX];		/* String index */

	if (tty_can_sync == SYNC_TESTED) {
		bogus = tty_sync_error();
	} else {
		bogus = 1;
	}
	usec_run_time = event_time(TIME_TEST);
	tx_source = t;
	tx_characters = raw_characters_sent;
	tx_cps = sliding_scale(tx_characters, 1000000, usec_run_time);

	/* save the data base */
	for (txp = ss = counts = 0; txp < ttp; txp++) {
		tx_cap[txp]   = tt_cap[txp];
		tx_count[txp] = tt_count[txp];
		tx_delay[txp] = tt_delay[txp];
		tx_affected[txp] = tt_affected[txp];
		tx_index[txp] = get_string_cap_byvalue(tt_cap[txp]);
		if (tx_index[txp] >= 0) {
			if (cap_match(t->caps_done, strnames[tx_index[txp]])) {
				ss_index[ss++] = txp;
				counts += tx_count[txp];
			}
		}
	}

	if (crlf) {
		put_crlf();
	}
	if (counts == 0 || tty_cps == 0 || bogus) {
		/* nothing to do */
		return;
	}
	/* calculate the suggested pad times */
	delta = usec_run_time - sliding_scale(tx_characters, 1000000, tty_cps);
	if (delta < 0) {
		/* probably should bump tx_characters */
		delta = 0;
	}
	cpo = delta / counts;
	for (i = 0; i < ss; i++) {
		if (!(r = get_next_block())) {
			return;
		}
		r->next = pads[tx_index[ss_index[i]]];
		pads[tx_index[ss_index[i]]] = r;
		r->test = t;
		r->reps = tx_affected[ss_index[i]];
		r->delay = cpo;
	}
}

/*
**	show_cap_results(index)
**
**	Display the previous results
*/
static void
show_cap_results(
	int x)
{
	struct test_results *r;		/* a result */
	int delay;

	if ((r = pads[x])) {
		sprintf(temp, "(%s)", strnames[x]);
		ptext(temp);
		while (r) {
			sprintf(temp, "$<%d>", r->delay / 1000);
			put_columns(temp, strlen(temp), 10);
			r = r->next;
		}
		r = pads[x];
		while (r) {
			if (r->reps > 1) {
				delay = r->delay / (r->reps * 100);
				sprintf(temp, "$<%d.%d*>", delay / 10, delay % 10);
				put_columns(temp, strlen(temp), 10);
			}
			r = r->next;
		}
		put_crlf();
	}
}

/*
**	dump_test_stats(test_list, status, ch)
**
**	Dump the statistics about the last test
*/
void
dump_test_stats(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j;
	char tbuf[32];
	int x[32];

	put_crlf();
	if (tx_source && tx_source->caps_done) {
		cap_index(tx_source->caps_done, x);
		if (x[0] >= 0) {
			sprintf(temp, "Caps summary for (%s)",
				tx_source->caps_done);
			ptextln(temp);
			for (i = 0; x[i] >= 0; i++) {
				show_cap_results(x[i]);
			}
			put_crlf();
		}
	}
	sprintf(tbuf, "%011u", usec_run_time);
	sprintf(temp, "Test time: %d.%s, characters per second %d, characters %d",
		usec_run_time / 1000000, &tbuf[5], tx_cps, tx_characters);
	ptextln(temp);
	for (i = 0; i < txp; i++) {
		if ((j = get_string_cap_byvalue(tx_cap[i])) >= 0) {
			sprintf(tbuf, "(%s)", strnames[j]);
		} else {
			strcpy(tbuf, "(?)");
		}
		sprintf(temp, "%8d  %3d  $<%3d>  %8s %s",
			tx_count[i], tx_affected[i], tx_delay[i],
			tbuf, expand(tx_cap[i]));
		putln(temp);
	}
	generic_done_message(t, state, ch);
}

/*
**	longer_test_time(test_list, status, ch)
**
**	Extend the number of seconds for each test.
*/
void
longer_test_time(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	pad_test_duration += 1;
	sprintf(txt_longer_test_time, "+) Change test time to %d seconds",
		pad_test_duration + 1);
	sprintf(txt_shorter_test_time, "-) Change test time to %d seconds",
		pad_test_duration - 1);
	sprintf(temp, "Tests will run for %d seconds", pad_test_duration);
	ptext(temp);
	*ch = REQUEST_PROMPT;
}

/*
**	shorter_test_time(test_list, status, ch)
**
**	Shorten the number of seconds for each test.
*/
void
shorter_test_time(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	if (pad_test_duration > 1) {
		pad_test_duration -= 1;
		sprintf(txt_longer_test_time, "+) Change test time to %d seconds",
			pad_test_duration + 1);
		sprintf(txt_shorter_test_time, "-) Change test time to %d seconds",
			pad_test_duration - 1);
	}
	sprintf(temp, "Tests will run for %d second%s", pad_test_duration,
		pad_test_duration > 1 ? "s" : "");
	ptext(temp);
	*ch = REQUEST_PROMPT;
}

/*
**	longer_augment(test_list, status, ch)
**
**	Lengthen the number of lines/characters effected
*/
void
longer_augment(
	struct test_list *t,
	int *state GCC_UNUSED,
	int *ch)
{
	augment <<= 1;
	set_augment_txt();
	if (augment_test) {
		t = augment_test;
	}
	sprintf(temp, "The pad tests will effect %d %s.", augment,
		((t->flags & MENU_LC_MASK) == MENU_lines) ?
		"lines" : "characters");
	ptextln(temp);
	*ch = REQUEST_PROMPT;
}

/*
**	shorter_augment(test_list, status, ch)
**
**	Shorten the number of lines/characters effected
*/
void
shorter_augment(
	struct test_list *t,
	int *state GCC_UNUSED,
	int *ch)
{
	if (augment > 1) {
		/* don't let the augment go to zero */
		augment >>= 1;
	}
	set_augment_txt();
	if (augment_test) {
		t = augment_test;
	}
	sprintf(temp, "The pad tests will effect %d %s.", augment,
		((t->flags & MENU_LC_MASK) == MENU_lines) ?
		"lines" : "characters");
	ptextln(temp);
	*ch = REQUEST_PROMPT;
}
