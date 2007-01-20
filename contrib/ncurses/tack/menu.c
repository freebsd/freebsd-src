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
** the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
** Boston, MA 02110-1301, USA
*/

#include <tack.h>

MODULE_ID("$Id: menu.c,v 1.3 2005/09/17 19:49:16 tom Exp $")

/*
   Menu control
 */

static void test_byname(struct test_menu *, int *, int *);

struct test_list *augment_test;
char prompt_string[80];	/* menu prompt storage */

/*
**	menu_prompt()
**
**	Print the menu prompt string.
*/
void
menu_prompt(void)
{
	ptext(&prompt_string[1]);
}

/*
**	menu_test_loop(test-structure, state, control-character)
**
**	This function implements the repeat test function.
*/
static void
menu_test_loop(
	struct test_list *test,
	int *state,
	int *ch)
{
	int nch, p;

	if ((test->flags & MENU_REP_MASK) && (augment_test != test)) {
		/* set the augment variable (first time only) */
		p = (test->flags >> 8) & 15;
		if ((test->flags & MENU_REP_MASK) == MENU_LM1) {
			augment = lines - 1;
		} else
		if ((test->flags & MENU_ONE_MASK) == MENU_ONE) {
			augment = 1;
		} else
		if ((test->flags & MENU_LC_MASK) == MENU_lines) {
			augment = lines * p / 10;
		} else
		if ((test->flags & MENU_LC_MASK) == MENU_columns) {
			augment = columns * p / 10;
		} else {
			augment = 1;
		}
		augment_test = test;
		set_augment_txt();
	}
	do {
		if ((test->flags | *state) & MENU_CLEAR) {
			put_clear();
		} else
		if (line_count + test->lines_needed >= lines) {
			put_clear();
		}
		nch = 0;
		if (test->test_procedure) {
			/* The procedure takes precedence so I can pass
			   the menu entry as an argument.
			*/
			can_test(test->caps_done, FLAG_TESTED);
			can_test(test->caps_tested, FLAG_TESTED);
			test->test_procedure(test, state, &nch);
		} else
		if (test->sub_menu) {
			/* nested menu's */
			menu_display(test->sub_menu, &nch);
			*state = 0;
			if (nch == 'q' || nch == 's') {
				/* Quit and skip are killed here */
				nch = '?';
			}
		} else {
			break;	/* cya */
		}
		if (nch == '\r' || nch == '\n' || nch == 'n') {
			nch = 0;
			break;
		}
	} while (nch == 'r');
	*ch = nch;
}

/*
**	menu_display(menu-structure, flags)
**
**	This function implements menu control.
*/
void
menu_display(
	struct test_menu *menu,
	int *last_ch)
{
	int test_state = 0, run_standard_tests;
	int hot_topic, ch = 0, nch = 0;
	struct test_list *mt;
	struct test_list *repeat_tests = 0;
	int repeat_state = 0;
	int prompt_length;

	prompt_length = strlen(prompt_string);
	if (menu->ident) {
		sprintf(&prompt_string[prompt_length], "/%s", menu->ident);
	}
	hot_topic = menu->default_action;
	run_standard_tests = menu->standard_tests ?
		menu->standard_tests[0] : -1;
	if (!last_ch) {
		last_ch = &ch;
	}
	while (1) {
		if (ch == 0) {
			/* Display the menu */
			put_crlf();
			if (menu->menu_function) {
				/*
				   this function may be used to restrict menu
				   entries.  If used it must print the title.
				*/
				menu->menu_function(menu);
			} else
			if (menu->menu_title) {
				ptextln(menu->menu_title);
			}
			for (mt = menu->tests; (mt->flags & MENU_LAST) == 0; mt++) {
				if (mt->menu_entry) {
					ptext(" ");
					ptextln(mt->menu_entry);
				}
			}
			if (menu->standard_tests) {
				ptext(" ");
				ptextln(menu->standard_tests);
				ptextln(" r) repeat test");
				ptextln(" s) skip to next test");
			}
			ptextln(" q) quit");
			ptextln(" ?) help");
		}
		if (ch == 0 || ch == REQUEST_PROMPT) {
			put_crlf();
			ptext(&prompt_string[1]);
			if (hot_topic) {
				ptext(" [");
				putchp(hot_topic);
				ptext("]");
			}
			ptext(" > ");
			/* read a character */
			ch = wait_here();
		}
		if (ch == '\r' || ch == '\n') {
			ch = hot_topic;
		}
		if (ch == 'q') {
			break;
		}
		if (ch == '?') {
			ch = 0;
			continue;
		}
		nch = ch;
		ch = 0;
		/* Run one of the standard tests (by request) */
		for (mt = menu->tests; (mt->flags & MENU_LAST) == 0; mt++) {
			if (mt->menu_entry && (nch == mt->menu_entry[0])) {
				if (mt->flags & MENU_MENU) {
					test_byname(menu, &test_state, &nch);
				} else {
					menu_test_loop(mt, &test_state, &nch);
				}
				ch = nch;
				if ((mt->flags & MENU_COMPLETE) && ch == 0) {
					/* top level */
					hot_topic = 'q';
					ch = '?';
				}
			}
		}
		if (menu->standard_tests && nch == 'r') {
			menu->resume_tests = repeat_tests;
			test_state = repeat_state;
			nch = run_standard_tests;
		}
		if (nch == run_standard_tests) {
			if (!(mt = menu->resume_tests)) {
				mt = menu->tests;
			}
			if (mt->flags & MENU_LAST) {
				mt = menu->tests;
			}
			/* Run the standard test suite */
			for ( ; (mt->flags & MENU_LAST) == 0; ) {
				if ((mt->flags & MENU_NEXT) == MENU_NEXT) {
					repeat_tests = mt;
					repeat_state = test_state;
					nch = run_standard_tests;
					menu_test_loop(mt, &test_state, &nch);
					if (nch != 0 && nch != 'n') {
						ch = nch;
						break;
					}
					if (test_state & MENU_STOP) {
						break;
					}
				}
				mt++;
			}
			if (ch == 0) {
				ch = hot_topic;
			}
			menu->resume_tests = mt;
			menu->resume_state = test_state;
			menu->resume_char = ch;

			if (ch == run_standard_tests) {
				/* pop up a level */
				break;
			}
		}
	}
	*last_ch = ch;
	prompt_string[prompt_length] = '\0';
}

/*
**	generic_done_message(test_list)
**
**	Print the Done message and request input.
*/
void
generic_done_message(
	struct test_list *test,
	int *state,
	int *ch)
{
	char done_message[128];

	if (test->caps_done) {
		sprintf(done_message, "(%s) Done ", test->caps_done);
		ptext(done_message);
	} else {
		ptext("Done ");
	}
	*ch = wait_here();
	if (*ch == '\r' || *ch == '\n' || *ch == 'n') {
		*ch = 0;
	}
	if (*ch == 's') {
		*state |= MENU_STOP;
		*ch = 0;
	}
}

/*
**	menu_clear_screen(test, state, ch)
**
**	Just clear the screen.
*/
void
menu_clear_screen(
	struct test_list *test GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	put_clear();
}

/*
**	menu_reset_init(test, state, ch)
**
**	Send the reset and init strings.
*/
void
menu_reset_init(
	struct test_list *test GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	reset_init();
	put_crlf();
}

/*
**	subtest_menu(test, state, ch)
**
**	Scan the menu looking for something to execute
**	Return TRUE if we found anything.
*/
int
subtest_menu(
	struct test_list *test,
	int *state,
	int *ch)
{
	struct test_list *mt;

	if (*ch) {
		for (mt = test; (mt->flags & MENU_LAST) == 0; mt++) {
			if (mt->menu_entry && (*ch == mt->menu_entry[0])) {
				*ch = 0;
				menu_test_loop(mt, state, ch);
				return TRUE;
			}
		}
	}
	return FALSE;
}

/*
**	menu_can_scan(menu-structure)
**
**	Recursively scan the menu tree and find which cap names can be tested.
*/
void
menu_can_scan(
	const struct test_menu *menu)
{
	struct test_list *mt;

	for (mt = menu->tests; (mt->flags & MENU_LAST) == 0; mt++) {
		can_test(mt->caps_done, FLAG_CAN_TEST);
		can_test(mt->caps_tested, FLAG_CAN_TEST);
		if (!(mt->test_procedure)) {
			if (mt->sub_menu) {
				menu_can_scan(mt->sub_menu);
			}
		}
	}
}

/*
**	menu_search(menu-structure, cap)
**
**	Recursively search the menu tree and execute any tests that use cap.
*/
static void
menu_search(
	struct test_menu *menu,
	int *state,
	int *ch,
	char *cap)
{
	struct test_list *mt;
	int nch;

	for (mt = menu->tests; (mt->flags & MENU_LAST) == 0; mt++) {
		nch = 0;
		if (cap_match(mt->caps_done, cap)
			|| cap_match(mt->caps_tested, cap)) {
			menu_test_loop(mt, state, &nch);
		}
		if (!(mt->test_procedure)) {
			if (mt->sub_menu) {
				menu_search(mt->sub_menu, state, &nch, cap);
			}
		}
		if (*state & MENU_STOP) {
			break;
		}
		if (nch != 0 && nch != 'n') {
			*ch = nch;
			break;
		}
	}
}

/*
**	test_byname(menu, state, ch)
**
**	Get a cap name then run all tests that use that cap.
*/
static void
test_byname(
	struct test_menu *menu,
	int *state GCC_UNUSED,
	int *ch)
{
	int test_state = 0;
	char cap[32];

	if (tty_can_sync == SYNC_NOT_TESTED) {
		verify_time();
	}
	ptext("enter name: ");
	read_string(cap, sizeof(cap));
	if (cap[0]) {
		menu_search(menu, &test_state, ch, cap);
	}
	*ch = '?';
}
