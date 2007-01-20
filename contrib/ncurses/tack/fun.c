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

MODULE_ID("$Id: fun.c,v 1.9 2006/11/26 00:15:53 tom Exp $")

/*
 * Test the function keys on the terminal.  The code for echo tests
 * lives here too.
 */

static void funkey_keys(struct test_list *, int *, int *);
static void funkey_meta(struct test_list *, int *, int *);
static void funkey_label(struct test_list *, int *, int *);
static void funkey_prog(struct test_list *, int *, int *);
static void funkey_local(struct test_list *, int *, int *);

struct test_list funkey_test_list[] = {
	{0, 0, 0, 0, "e) edit terminfo", 0, &edit_menu},
	{MENU_CLEAR + FLAG_FUNCTION_KEY, 0, 0, 0, "f) show a list of function keys", show_report, 0},
	{MENU_NEXT | MENU_CLEAR, 0, "smkx) (rmkx", 0,
		"k) test function keys", funkey_keys, 0},
	{MENU_NEXT, 10, "km", "smm rmm", 0, funkey_meta, 0},
	{MENU_NEXT, 8, "nlab) (smln) (pln) (rmln", "lw lh", 0, funkey_label, 0},
	{MENU_NEXT, 2, "pfx", 0, 0, funkey_prog, 0},
	{MENU_NEXT, 2, "pfloc", 0, 0, funkey_local, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

static void printer_on(struct test_list *, int *, int *);
static void printer_mc0(struct test_list *, int *, int *);

struct test_list printer_test_list[] = {
	{0, 0, 0, 0, "e) edit terminfo", 0, &edit_menu},
	{MENU_NEXT | MENU_CLEAR, 0, "mc4) (mc5) (mc5i", 0, 0, printer_on, 0},
	{MENU_NEXT | MENU_CLEAR, 0, "mc0", 0, 0, printer_mc0, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

/* local definitions */
static const char **fk_name;
static char **fkval;
static char **fk_label;		/* function key labels (if any) */
static int *fk_tested;
static int num_strings = 0;

static int fkmax = 1;		/* length of longest key */
static int got_labels = 0;	/* true if we have some labels */
static int key_count = 0;
static int end_state;

/* unknown function keys */
#define MAX_FK_UNK 50
static char *fk_unknown[MAX_FK_UNK];
static int fk_length[MAX_FK_UNK];
static int funk;

/*
 * Initialize arrays that depend on the actual number of strings.
 */
static void
alloc_strings(void)
{
	if (num_strings != MAX_STRINGS) {
		num_strings = MAX_STRINGS;
		fk_name = (const char **)calloc(num_strings, sizeof(const char *));
		fkval = (char **)calloc(num_strings, sizeof(char *));
		fk_label = (char **)calloc(num_strings, sizeof(char *));
		fk_tested = (int *)calloc(num_strings, sizeof(int));
	}
}

/*
**	keys_tested(first-time, show-help, hex-output)
**
**	Display a list of the keys not tested.
*/
static void
keys_tested(
	int first_time,
	int show_help,
	int hex_output)
{
	int i, l;
	char outbuf[256];

	alloc_strings();
	put_clear();
	tty_set();
	flush_input();
	if (got_labels) {
		putln("Function key labels:");
		for (i = 0; i < key_count; ++i) {
			if (fk_label[i]) {
				sprintf(outbuf, "%s %s",
					fk_name[i] ? fk_name[i] : "??", fk_label[i]);
				put_columns(outbuf, (int) strlen(outbuf), 16);
			}
		}
		put_newlines(2);
	}
	if (funk) {
		putln("The following keys are not defined:");
		for (i = 0; i < funk; ++i) {
			put_columns(fk_unknown[i], fk_length[i], 16);
		}
		put_mode(exit_attribute_mode);
		put_newlines(2);
	}
	if (first_time) {
		putln("The following keys are defined:");
	} else {
		putln("The following keys have not been tested:");
	}
	if (scan_mode) {
		for (i = 0; scan_down[i]; i++) {
			if (!scan_tested[i]) {
				if (hex_output) {
					strcpy(outbuf, hex_expand_to(scan_down[i], 3));
				} else {
					strcpy(outbuf, expand(scan_down[i]));
				}
				l = expand_chars;
				if (hex_output) {
					strcat(outbuf, hex_expand_to(scan_up[i], 3));
				} else {
					strcat(outbuf, expand(scan_up[i]));
				}
				expand_chars += l;
				l = strlen(scan_name[i]);
				if (((char_count + 16) & ~15) +
					((expand_chars + 7) & ~7) + l >= columns) {
					put_crlf();
				} else
				if (char_count + 24 > columns) {
					put_crlf();
				} else if (char_count) {
					putchp(' ');
				}
				put_columns(outbuf, expand_chars, 16);
				put_columns(scan_name[i], l, 8);
			}
		}
	} else {
		for (i = 0; i < key_count; i++) {
			if (!fk_tested[i]) {
				if (hex_output) {
					strcpy(outbuf, hex_expand_to(fkval[i], 3));
				} else {
					strcpy(outbuf, expand(fkval[i]));
				}
				l = strlen(fk_name[i]);
				if (((char_count + 16) & ~15) +
					((expand_chars + 7) & ~7) + l >= columns) {
					put_crlf();
				} else
				if (char_count + 24 > columns) {
					put_crlf();
				} else
				if (char_count) {
					putchp(' ');
				}
				put_columns(outbuf, expand_chars, 16);
				put_columns(fk_name[i], l, 8);
			}
		}
	}
	put_newlines(2);
	if (show_help) {
		ptextln("Hit any function key.  Type 'end' to quit.  Type ? to update the display.");
		put_crlf();
	}
}

/*
**	enter_key(name, value, label)
**
**	Enter a function key into the data base
*/
void
enter_key(
	const char *name,
	char *value,
	char *lab)
{
	int j;

	alloc_strings();
	if (value) {
		j = strlen(value);
		fkmax = fkmax > j ? fkmax : j;
		/* do not permit duplicates */
		for (j = 0; j < key_count; j++) {
			if (!strcmp(fk_name[j], name)) {
				return;
			}
		}
		fkval[key_count] = value;
		fk_tested[key_count] = 0;
		fk_label[key_count] = lab;
		fk_name[key_count++] = name;
		if (lab) {
			got_labels = TRUE;
		}
	}
}


static void
fresh_line(void)
{				/* clear the line for a new function key line */
	if (over_strike) {
		put_crlf();
	} else {
		put_cr();
		if (clr_eol) {
			tc_putp(clr_eol);
		} else {
			put_str("                    \r");
		}
	}
}


static int
end_funky(int ch)
{				/* return true if this is the end */
	switch (ch) {
	case 'e':
	case 'E':
		end_state = 'e';
		break;
	case 'n':
	case 'N':
		if (end_state == 'e') {
			end_state = 'n';
		} else {
			end_state = 0;
		}
		break;
	case 'd':
	case 'D':
		if (end_state == 'n') {
			end_state = 'd';
		} else {
			end_state = 0;
		}
		break;
	case 'l':
	case 'L':
		if (end_state == 'l') {
			end_state = '?';
		} else {
			end_state = 'l';
		}
		break;
	default:
		end_state = 0;
		break;
	}
	return end_state == 'd';
}


static int
found_match(char *s, int hx, int cc)
{				/* return true if this string is a match */
	int j, f;
	char outbuf[256];

	alloc_strings();
	if (!*s) {
		return 0;
	}
	if (scan_mode) {
		for (j = f = 0; scan_down[j]; j++) {
			if (scan_length[j] == 0) {
				continue;
			}
			if (!strncmp(s, scan_down[j], scan_length[j])) {
				if (!f) {	/* first match */
					put_cr();
					if (hx) {
						put_str(hex_expand_to(s, 10));
					} else {
						put_str(expand_to(s, 10));
					}
					f = 1;
				}
				(void) end_funky(scan_name[j][0]);
				put_str(" ");
				put_str(scan_name[j]);
				scan_tested[j] = 1;
				s += scan_length[j];
				if (strncmp(s, scan_up[j], scan_length[j])) {
					put_str(" scan down");
				} else {
					s += scan_length[j];
				}
				if (!*s) {
					break;
				}
				j = -1;
			}
			if (!strncmp(s, scan_up[j], scan_length[j])) {
				if (!f) {	/* first match */
					put_cr();
					if (hx) {
						put_str(hex_expand_to(s, 10));
					} else {
						put_str(expand_to(s, 10));
					}
					f = 1;
				}
				put_str(" ");
				put_str(scan_name[j]);
				put_str(" scan up");
				s += scan_length[j];
				if (!*s) {
					break;
				}
				j = -1;
			}
		}
	} else {
		for (j = f = 0; j < key_count; j++) {
			if (!strcmp(s, fkval[j])) {
				if (!f) {	/* first match */
					put_cr();
					if (hx) {
						put_str(hex_expand_to(s, 10));
					} else {
						put_str(expand_to(s, 10));
					}
					f = 1;
				}
				sprintf(outbuf, " (%s)", fk_name[j]);
				put_str(outbuf);
				if (fk_label[j]) {
					sprintf(outbuf, " <%s>", fk_label[j]);
					put_str(outbuf);
				}
				fk_tested[j] = 1;
			}
		}
	}
	if (end_state == '?') {
		keys_tested(0, 1, hx);
		tty_raw(cc, char_mask);
		end_state = 0;
	}
	return f;
}


static int
found_exit(char *keybuf, int hx, int cc)
{				/* return true if the user wants to exit */
	int j, k;
	char *s;


	if (scan_mode) {
		if (*keybuf == '\0') {
			return TRUE;
		}
	} else {
		/* break is a special case */
		if (*keybuf == '\0') {
			fresh_line();
			tty_set();
			ptext("Hit X to exit: ");
			if (wait_here() == 'X') {
				return TRUE;
			}
			keys_tested(0, 1, hx);
			tty_raw(cc, char_mask);
			return FALSE;
		}
		/* is this the end? */
		for (k = 0; (j = (keybuf[k] & STRIP_PARITY)); k++) {
			if (end_funky(j)) {
				return TRUE;
			}
		}

		j = TRUE;	/* does he need an updated list? */
		for (k = 0; keybuf[k]; k++) {
			j &= (keybuf[k] & STRIP_PARITY) == '?';
		}
		if (j || end_state == '?') {
			keys_tested(0, 1, hx);
			tty_raw(cc, char_mask);
			end_state = 0;
			return FALSE;
		}
	}

	put_cr();
	if (hx) {
		s = hex_expand_to(keybuf, 10);
	} else {
		s = expand_to(keybuf, 10);
	}
	sprintf(temp, "%s Unknown", s);
	put_str(temp);
	for (j = 0; j < MAX_FK_UNK; j++) {
		if (j == funk) {
			fk_length[funk] = expand_chars;
			if ((fk_unknown[funk] = (char *)malloc(strlen(s) + 1))) {
				strcpy(fk_unknown[funk++], s);
			}
			break;
		}
		if (fk_length[j] == expand_chars) {
			if (!strcmp(fk_unknown[j], s)) {
				break;
			}
		}
	}
	return FALSE;
}

/*
**	funkey_keys(test_list, status, ch)
**
**	Test function keys
*/
static void
funkey_keys(
	struct test_list *t,
	int *state,
	int *ch)
{
	char keybuf[256];

	if (keypad_xmit) {
		tc_putp(keypad_xmit);
	}
	keys_tested(1, 1, hex_out);	/* also clears screen */
	keybuf[0] = '\0';
	end_state = 0;
	if (scan_mode) {
		fkmax = scan_max;
	}
	tty_raw(0, char_mask);
	while (end_state != 'd') {
		read_key(keybuf, sizeof(keybuf));
		fresh_line();
		if (found_match(keybuf, hex_out, 0)) {
			continue;
		}
		if (found_exit(keybuf, hex_out, 0)) {
			break;
		}
	}
	if (keypad_local) {
		tc_putp(keypad_local);
	}
	keys_tested(0, 0, hex_out);
	ptext("Function key test ");
	generic_done_message(t, state, ch);
}

int
tty_meta_prep(void)
{				/* print a warning before the meta key test */
	if (not_a_tty) {
		return 0;
	}
	if (initial_stty_query(TTY_8_BIT)) {
		return 0;
	}
	ptext("The meta key test must be run with the");
	ptext(" terminal set for 8 data bits.  Two stop bits");
	ptext(" may also be needed for correct display.  I will");
	ptext(" transmit 8 bit data but if the terminal is set for");
	ptextln(" 7 bit data, garbage may appear on the screen.");
	return 1;
}

/*
**	funkey_meta(test_list, status, ch)
**
**	Test meta key (km) (smm) (rmm)
*/
static void
funkey_meta(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, j, k, len;
	char outbuf[256];

	if (has_meta_key) {
		put_crlf();
		if (char_mask != ALLOW_PARITY) {
			if (tty_meta_prep()) {
				ptext("\nHit any key to continue > ");
				(void) wait_here();
				put_crlf();
			}
		}
		ptext("Begin meta key test. (km) (smm) (rmm)  Hit any key");
		ptext(" with the meta key.  The character will be");
		ptext(" displayed in hex.  If the meta key is working");
		ptext(" then the most significant bit will be set.  Type");
		ptextln(" 'end' to exit.");
		tty_raw(1, ALLOW_PARITY);
		tc_putp(meta_on);

		for (i = j = k = len = 0; i != 'e' || j != 'n' || k != 'd';) {
			i = j;
			j = k;
			k = getchp(ALLOW_PARITY);
			if (k == EOF) {
				break;
			}
			if ((len += 3) >= columns) {
				put_crlf();
				len = 3;
			}
			sprintf(outbuf, "%02X ", k);
			put_str(outbuf);
			k &= STRIP_PARITY;
		}
		tc_putp(meta_off);
		put_crlf();
		tty_set();
		put_crlf();
	} else {
		ptext("(km) Has-meta-key is not set.  ");
	}
	generic_done_message(t, state, ch);
}

/*
**	funkey_label(test_list, status, ch)
**
**	Test labels (nlab) (smln) (pln) (rmln) (lw) (lh)
*/
static void
funkey_label(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i;
	char outbuf[256];

	if (num_labels == -1) {
		ptextln("Your terminal has no labels. (nlab)");
	} else {
		sprintf(temp, "Your terminal has %d labels (nlab) that are %d characters wide (lw) and %d lines high (lh)",
			num_labels, label_width, label_height);
		ptext(temp);
		ptextln(" Testing (smln) (pln) (rmln)");
		if (label_on) {
			tc_putp(label_on);
		}
		if (label_width <= 0) {
			label_width = sizeof(outbuf) - 1;
		}
		for (i = 1; i <= num_labels; i++) {
			sprintf(outbuf, "L%d..............................", i);
			outbuf[label_width] = '\0';
			tc_putp(TPARM_2(plab_norm, i, outbuf));
		}
		if (label_off) {
			ptext("Hit any key to remove the labels: ");
			(void) wait_here();
			tc_putp(label_off);
		}
	}
	generic_done_message(t, state, ch);
}

/*
**	funkey_prog(test_list, status, ch)
**
**	Test program function keys (pfx)
*/
static void
funkey_prog(
	struct test_list *t,
	int *state,
	int *ch)
{
	int i, fk;
	char mm[256];

	fk = 1;	/* use function key 1 for now */
	if (pkey_xmit) {
		/* test program function key */
		sprintf(temp,
			"(pfx) Set function key %d to transmit abc\\n", fk);
		ptextln(temp);
		tc_putp(TPARM_2(pkey_xmit, fk, "abc\n"));
		sprintf(temp, "Hit function key %d\n", fk);
		ptextln(temp);
		for (i = 0; i < 4; ++i)
			mm[i] = getchp(STRIP_PARITY);
		mm[i] = '\0';
		put_crlf();
		if (mm[0] != 'a' || mm[1] != 'b' || mm[2] != 'c') {
			sprintf(temp, "Error string received was: %s", expand(mm));
			ptextln(temp);
		} else {
			putln("Thank you\n");
		}
		flush_input();
		if (key_f1) {
			tc_putp(TPARM_2(pkey_xmit, fk, key_f1));
		}
	} else {
		ptextln("Function key transmit (pfx), not present.");
	}
	generic_done_message(t, state, ch);
}

/*
**	funkey_local(test_list, status, ch)
**
**	Test program local function keys (pfloc)
*/
static void
funkey_local(
	struct test_list *t,
	int *state,
	int *ch)
{
	int fk;

	fk = 1;
	if (pkey_local) {
		/* test local function key */
		sprintf(temp,
			"(pfloc) Set function key %d to execute a clear and print \"Done!\"", fk);
		ptextln(temp);
		sprintf(temp, "%sDone!", liberated(clear_screen));
		tc_putp(TPARM_2(pkey_local, fk, temp));
		sprintf(temp, "Hit function key %d.  Then hit return.", fk);
		ptextln(temp);
		(void) wait_here();
		flush_input();
		if (key_f1 && pkey_xmit) {
			tc_putp(TPARM_2(pkey_xmit, fk, key_f1));
		}
	} else {
		ptextln("Function key execute local (pfloc), not present.");
	}

	generic_done_message(t, state, ch);
}

/*
**	printer_on(test_list, status, ch)
**
**	Test printer on/off (mc4) (mc5) (mc5i)
*/
static void
printer_on(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (!prtr_on || !prtr_off) {
		ptextln("Printer on/off missing. (mc5) (mc4)");
	} else if (prtr_silent) {
		ptextln("Your printer is silent. (mc5i) is set.");
		tc_putp(prtr_on);
		ptextln("This line should be on the printer but not your screen. (mc5)");
		tc_putp(prtr_off);
		ptextln("This line should be only on the screen. (mc4)");
	} else {
		ptextln("Your printer is not silent. (mc5i) is reset.");
		tc_putp(prtr_on);
		ptextln("This line should be on the printer and the screen. (mc5)");
		tc_putp(prtr_off);
		ptextln("This line should only be on the screen. (mc4)");
	}
	generic_done_message(t, state, ch);
}

/*
**	printer_mc0(test_list, status, ch)
**
**	Test screen print (mc0)
*/
static void
printer_mc0(
	struct test_list *t,
	int *state,
	int *ch)
{
	if (print_screen) {
		ptext("I am going to send the contents of the screen to");
		ptext(" the printer, then wait for a keystroke from you.");
		ptext("  All of the text that appears on the screen");
		ptextln(" should be printed. (mc0)");
		tc_putp(print_screen);
	} else {
		ptext("(mc0) Print-screen is not present.  ");
	}
	generic_done_message(t, state, ch);
}


static void
line_pattern(void)
{				/* put up a pattern that will help count the
				   number of lines */
	int i, j;

	put_clear();
	if (over_strike) {
		for (i = 0; i < 100; i++) {
			if (i) {
				put_crlf();
			}
			for (j = i / 10; j; j--) {
				put_this(' ');
			}
			put_this('0' + ((i + 1) % 10));
		}
	} else	/* I assume it will scroll */ {
		for (i = 100; i; i--) {
			sprintf(temp, "\r\n%d", i);
			put_str(temp);
		}
	}
}


static void
column_pattern(void)
{				/* put up a pattern that will help count the
				   number of columns */
	int i, j;

	put_clear();
	for (i = 0; i < 20; i++) {
		for (j = 1; j < 10; j++) {
			put_this('0' + j);
		}
		put_this('.');
	}
}

/*
**	report_help()
**
**	Print the help text for the echo tests
*/
static void
report_help(int crx)
{
	ptextln("The following commands may also be entered:");
	ptextln(" clear   clear screen.");
	ptextln(" columns print a test pattern to help count screen width.");
	ptextln(" lines   print a test pattern to help count screen length.");
	ptextln(" end     exit.");
	ptextln(" echo    redisplay last report.");
	if (crx) {
		ptextln(" hex     redisplay last report in hex.");
	} else {
		ptextln(" hex     toggle hex display mode.");
	}
	ptextln(" help    display this list.");
	ptextln(" high    toggle forced high bit (0x80).");
	ptextln(" scan    toggle scan mode.");
	ptextln(" one     echo one character after <cr> or <lf> as is. (report mode)");
	ptextln(" two     echo two characters after <cr> or <lf> as is.");
	ptextln(" all     echo all characters after <cr> or <lf> as is. (echo mode)");
}

/*
**	tools_report(testlist, state, ch)
**
**	Run the echo tool and report tool
*/
void
tools_report(
	struct test_list *t,
	int *state GCC_UNUSED,
	int *pch GCC_UNUSED)
{
	int i, j, ch, crp, crx, high_bit, save_scan_mode, hex_display;
	char buf[1024];
	char txt[8];

	hex_display = hex_out;
	put_clear();
	if ((crx = (t->flags & 255)) == 1) {
		ptext("Characters after a CR or LF will be echoed as");
		ptextln(" is.  All other characters will be expanded.");
		report_help(crx);
	} else {	/* echo test */
		ptextln("Begin echo test.");
		report_help(crx);
	}
	memset(txt, 0, sizeof(txt));
	save_scan_mode = scan_mode;
	tty_raw(1, char_mask);
	for (i = crp = high_bit = 0;;) {
		ch = getchp(char_mask);
		if (ch == EOF) {
			break;
		}
		if (i >= (int) sizeof(buf) - 1) {
			i = 0;
		}
		buf[i++] = ch;
		buf[i] = '\0';
		for (j = 0; j < (int) sizeof(txt) - 1; j++) {
			txt[j] = txt[j + 1];
		}
		txt[sizeof(txt) - 1] = ch & STRIP_PARITY;
		if (crx == 0) {	/* echo test */
			if (hex_display) {
				ptext(hex_expand_to(&buf[i - 1], 3));
			} else {
				tc_putch(ch | high_bit);
			}
		} else /* status report test */
		if (ch == '\n' || ch == '\r') {
			put_crlf();
			crp = 0;
		} else if (crp++ < crx) {
			tc_putch(ch | high_bit);
		} else {
			put_str(expand(&buf[i - 1]));
		}
		if (!strncmp(&txt[sizeof(txt) - 7], "columns", 7)) {
			column_pattern();
			buf[i = 0] = '\0';
			crp = 0;
		}
		if (!strncmp(&txt[sizeof(txt) - 5], "lines", 5)) {
			line_pattern();
			buf[i = 0] = '\0';
			crp = 0;
		}
		if (!strncmp(&txt[sizeof(txt) - 5], "clear", 5)) {
			put_clear();
			buf[i = 0] = '\0';
			crp = 0;
		}
		if (!strncmp(&txt[sizeof(txt) - 4], "high", 4)) {
			high_bit ^= 0x80;
			if (high_bit) {
				ptextln("\nParity bit set");
			} else {
				ptextln("\nParity bit reset");
			}
		}
		if (!strncmp(&txt[sizeof(txt) - 4], "help", 4)) {
			put_crlf();
			report_help(crx);
		}
		if (!strncmp(&txt[sizeof(txt) - 4], "echo", 4)) {
			/* display the last status report */
			/* clear bypass condition on Tek terminals */
			put_crlf();
			if (i >= 4) {
				buf[i -= 4] = '\0';
			}
			put_str(expand(buf));
		}
		if (save_scan_mode &&
			!strncmp(&txt[sizeof(txt) - 4], "scan", 4)) {
			/* toggle scan mode */
			scan_mode = !scan_mode;
		}
		if (!strncmp(&txt[sizeof(txt) - 3], "end", 3))
			break;
		if (!strncmp(&txt[sizeof(txt) - 3], "hex", 3)) {
			if (crx) {
				/* display the last status report in hex */
				/* clear bypass condition on Tek terminals */
				put_crlf();
				if (i >= 3) {
					buf[i -= 3] = '\0';
				}
				put_str(hex_expand_to(buf, 3));
			} else {
				hex_display = !hex_display;
			}
		}
		if (!strncmp(&txt[sizeof(txt) - 3], "two", 3))
			crx = 2;
		if (!strncmp(&txt[sizeof(txt) - 3], "one", 3))
			crx = 1;
		if (!strncmp(&txt[sizeof(txt) - 3], "all", 3))
			crx = 0;
	}
	scan_mode = save_scan_mode;
	put_crlf();
	tty_set();
	if (crx) {
		ptextln("End of status report test.");
	} else {
		ptextln("End of echo test.");
	}
}
