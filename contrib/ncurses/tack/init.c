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
/* initialization and wrapup code */

#include <tack.h>

MODULE_ID("$Id: init.c,v 1.2 2000/05/13 19:58:48 Daniel.Weaver Exp $")

#if NCURSES_VERSION_MAJOR >= 5 || NCURSES_VERSION_PATCH >= 981219
#define _nc_get_curterm(p) _nc_get_tty_mode(p)
#endif

FILE *debug_fp;
char temp[1024];
char tty_basename[64];

void
put_name(const char *cap, const char *name)
{				/* send the cap name followed by the cap */
	if (cap) {
		ptext(name);
		tc_putp(cap);
	}
}

static void
report_cap(const char *tag, const char *s)
{				/* expand the cap or print *** missing *** */
	int i;

	ptext(tag);
	for (i = char_count; i < 13; i++) {
		putchp(' ');
	}
	put_str(" = ");
	if (s) {
		putln(expand(s));
	} else {
		putln("*** missing ***");
	}
}


void
reset_init(void)
{				/* send the reset and init strings */
	int i;

	ptext("Terminal reset");
	i = char_count;
	put_name(reset_1string, " (rs1)");
	put_name(reset_2string, " (rs2)");
	/* run the reset file */
	if (reset_file && reset_file[0]) {
		FILE *fp;
		int ch;

		can_test("rf", FLAG_TESTED);
		if ((fp = fopen(reset_file, "r"))) {	/* send the reset file */
			sprintf(temp, " (rf) %s", reset_file);
			ptextln(temp);
			while (1) {
				ch = getc(fp);
				if (ch == EOF)
					break;
				put_this(ch);
			}
			fclose(fp);
		} else {
			sprintf(temp, "\nCannot open reset file (rf) %s", reset_file);
			ptextln(temp);
		}
	}
	put_name(reset_3string, " (rs3)");
	if (i != char_count) {
		put_crlf();
	}
	ptext(" init");
	put_name(init_1string, " (is1)");
	put_name(init_2string, " (is2)");
	if (set_tab && clear_all_tabs && init_tabs != 8) {
		put_crlf();
		tc_putp(clear_all_tabs);
		for (char_count = 0; char_count < columns; char_count++) {
			put_this(' ');
			if ((char_count & 7) == 7) {
				tc_putp(set_tab);
			}
		}
		put_cr();
	}
	/* run the initialization file */
	if (init_file && init_file[0]) {
		FILE *fp;
		int ch;

		can_test("if", FLAG_TESTED);
		if ((fp = fopen(init_file, "r"))) {	/* send the init file */
			sprintf(temp, " (if) %s", init_file);
			ptextln(temp);
			while (1) {
				ch = getc(fp);
				if (ch == EOF)
					break;
				put_this(ch);
			}
			fclose(fp);
		} else {
			sprintf(temp, "\nCannot open init file (if) %s", init_file);
			ptextln(temp);
		}
	}
	if (init_prog) {
		can_test("iprog", FLAG_TESTED);
		(void) system(init_prog);
	}
	put_name(init_3string, " (is3)");

	fflush(stdout);
}

/*
**	display_basic()
**
**	display the basic terminal definitions
*/
void
display_basic(void)
{
	put_str("Name: ");
	putln(ttytype);

	report_cap("\\r ^M (cr)", carriage_return);
	report_cap("\\n ^J (ind)", scroll_forward);
	report_cap("\\b ^H (cub1)", cursor_left);
	report_cap("\\t ^I (ht)", tab);
/*      report_cap("\\f ^L (ff)", form_feed);	*/
	if (newline) {
		/* OK if missing */
		report_cap("      (nel)", newline);
	}
	report_cap("      (clear)", clear_screen);
	if (!cursor_home && cursor_address) {
		report_cap("(cup) (home)", tparm(cursor_address, 0, 0));
	} else {
		report_cap("      (home)", cursor_home);
	}
	report_cap("ENQ   (u9)", user9);
	report_cap("ACK   (u8)", user8);

	sprintf(temp, "\nTerminal size: %d x %d.  Baud rate: %ld.  Frame size: %d.%d", columns, lines, tty_baud_rate, tty_frame_size >> 1, (tty_frame_size & 1) * 5);
	putln(temp);
}

/*
**	curses_setup(exec_name)
**
**	Startup ncurses
*/
void
curses_setup(
	char *exec_name)
{
	int status;
	static TERMTYPE term;
	char tty_filename[2048];

	tty_init();

	/**
	   See if the terminal is in the terminfo data base.  This call has
	two useful benefits, 1) it returns the filename of the terminfo entry,
	and 2) it searches only terminfo's.  This allows us to abort before
	ncurses starts scanning the termcap file.
	**/
	if ((status = _nc_read_entry(tty_basename, tty_filename, &term)) == 0) {
		fprintf(stderr, "Terminal not found: TERM=%s\n", tty_basename);
		show_usage(exec_name);
		exit(1);
	}
	if (status == -1) {
		fprintf(stderr, "Terminfo database is inaccessible\n");
		exit(1);
	}

	/**
	   This call will load the terminfo data base and set the cur-term
	variable.  Only terminals that actually exist will get here so its
	OK to ignore errors.  This is a good thing since ncurses does not
	permit (os) or (gn) to be set.
	**/
	setupterm(tty_basename, 1, &status);

	/**
	   Get the current terminal definitions.  This must be done before
	getting the baudrate.
	**/
	_nc_get_curterm(&cur_term->Nttyb);
	tty_baud_rate = baudrate();
	tty_cps = (tty_baud_rate << 1) / tty_frame_size;

	/* set up the defaults */
	replace_mode = TRUE;
	scan_mode = 0;
	char_count = 0;
	select_delay_type = debug_level = 0;
	char_mask = (meta_on && meta_on[0] == '\0') ? ALLOW_PARITY : STRIP_PARITY;
	/* Don't change the XON/XOFF modes yet. */
	select_xon_xoff = initial_stty_query(TTY_XON_XOFF) ? 1 : needs_xon_xoff;

	fflush(stdout);	/* flush any output */
	tty_set();

	go_home();	/* set can_go_home */
	put_clear();	/* set can_clear_screen */

	if (send_reset_init) {
		reset_init();
	}

	/*
	   I assume that the reset and init strings may not have the correct
	   pads.  (Because that part of the test comes much later.)  Because
	   of this, I allow the terminal some time to catch up.
	*/
	fflush(stdout);	/* waste some time */
	sleep(1);	/* waste more time */
	charset_can_test();
	can_test("lines cols cr nxon rf if iprog rmp smcup rmcup", FLAG_CAN_TEST);
	edit_init();			/* initialize the edit data base */

	if (send_reset_init && enter_ca_mode) {
		tc_putp(enter_ca_mode);
		put_clear();	/* just in case we switched pages */
	}
	put_crlf();
	ptext("Using terminfo from: ");
	ptextln(tty_filename);
	put_crlf();

	if (tty_can_sync == SYNC_NEEDED) {
		verify_time();
	}

	display_basic();
}

/*
**	bye_kids(exit-condition)
**
**	Shutdown the terminal, clear the signals, and exit
*/
void
bye_kids(int n)
{				/* reset the tty and exit */
	ignoresig();
	if (send_reset_init) {
		if (exit_ca_mode) {
			tc_putp(exit_ca_mode);
		}
		if (initial_stty_query(TTY_XON_XOFF)) {
			if (enter_xon_mode) {
				tc_putp(enter_xon_mode);
			}
		} else if (exit_xon_mode) {
			tc_putp(exit_xon_mode);
		}
	}
	if (debug_fp) {
		fclose(debug_fp);
	}
	if (log_fp) {
		fclose(log_fp);
	}
	tty_reset();
	fclose(stdin);
	fclose(stdout);
	fclose(stderr);
	if (not_a_tty)
		sleep(1);
	exit(n);
}
