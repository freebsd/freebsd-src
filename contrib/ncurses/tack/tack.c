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

MODULE_ID("$Id: tack.c,v 1.4 2005/09/17 19:49:16 tom Exp $")

/*
   This program is designed to test terminfo, not curses.  Therefore
   I have used as little of curses as possible.

   Pads associated with the following capabilities are used to set
   delay times in the handler:  (cr), (ind), (cub1), (ff), (tab).

   I use the (nxon) capability to set the tty handler with/without
   xon/xoff.  If (smxon)/(rmxon) is defined I will change the terminal
   too.

   (xon) inhibits the sending of delay characters in putp().
   If the terminal is defined with no padding then the (xon) boolean
   is a don't care.  In this case I recommend that it be reset.
 */

/*****************************************************************************
 *
 * Option processing
 *
 *****************************************************************************/

/* options and modes */
int debug_level;		/* debugging level */
int translate_mode;		/* translate tab, bs, cr, lf, ff */
int scan_mode;			/* use scan codes */
int char_mask;			/* either 0xFF else 0x7F, eight bit data mask */
int select_delay_type;		/* set handler delays for <cr><lf> */
int select_xon_xoff;		/* TTY driver XON/XOFF mode select */
int hex_out;			/* Display output in hex */
int send_reset_init;		/* Send the reset and initialization strings */
FILE *log_fp;			/* Terminal logfile */

/*****************************************************************************
 *
 * Menu definitions
 *
 *****************************************************************************/

static void tools_hex_echo(struct test_list *, int *, int *);
static void tools_debug(struct test_list *, int *, int *);

static char hex_echo_menu_entry[80];

static struct test_list tools_test_list[] = {
	{0, 0, 0, 0, "s) ANSI status reports", tools_status, 0},
	{0, 0, 0, 0, "g) ANSI SGR modes (bold, underline, reverse)", tools_sgr, 0},
	{0, 0, 0, 0, "c) ANSI character sets", tools_charset, 0},
	{0, 0, 0, 0, hex_echo_menu_entry, tools_hex_echo, 0},
	{0, 0, 0, 0, "e) echo tool", tools_report, 0},
	{1, 0, 0, 0, "r) reply tool", tools_report, 0},
	{0, 0, 0, 0, "p) performance testing", 0, &sync_menu},
	{0, 0, 0, 0, "i) send reset and init", menu_reset_init, 0},
	{0, 0, "u8) (u9", 0, "u) test ENQ/ACK handshake", sync_handshake, 0},
	{0, 0, 0, 0, "d) change debug level", tools_debug, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

static struct test_menu tools_menu = {
	0, 'q', 0, "Tools Menu", "tools",
	0, 0, tools_test_list, 0, 0, 0
};

static void tty_width(struct test_list *, int *, int *);
static void tty_delay(struct test_list *, int *, int *);
static void tty_xon(struct test_list *, int *, int *);
static void tty_trans(struct test_list *, int *, int *);
static void tty_show_state(struct test_menu *);

static char tty_width_menu[80];
static char tty_delay_menu[80];
static char tty_xon_menu[80];
static char tty_trans_menu[80];
static char enable_xon_xoff[] = {"x) enable xon/xoff"};
static char disable_xon_xoff[] = {"x) disable xon/xoff"};

static struct test_list tty_test_list[] = {
	{0, 0, 0, 0, tty_width_menu, tty_width, 0},
	{0, 0, 0, 0, tty_delay_menu, tty_delay, 0},
	{0, 0, 0, 0, tty_xon_menu, tty_xon, 0},
	{0, 0, 0, 0, tty_trans_menu, tty_trans, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

static struct test_menu tty_menu = {
	0, 'q', 0, "Terminal and driver configuration",
	"tty", 0,
	tty_show_state, tty_test_list, 0, 0, 0
};

struct test_menu edit_menu = {
	0, 'q', 0, "Edit terminfo menu",
	"edit", 0,
	0, edit_test_list, 0, 0, 0
};

static struct test_menu mode_menu = {
	0, 'n', 0, "Test modes and glitches:",
	"mode", "n) run standard tests",
	0, mode_test_list, 0, 0, 0
};

static struct test_menu acs_menu = {
	0, 'n', 0,
	"Test alternate character set and graphics rendition:",
	"acs", "n) run standard tests",
	0, acs_test_list, 0, 0, 0
};

static struct test_menu color_menu = {
	0, 'n', 0,
	"Test color:",
	"color", "n) run standard tests",
	0, color_test_list, 0, 0, 0
};

static struct test_menu crum_menu = {
	0, 'n', 0,
	"Test cursor movement:",
	"move", "n) run standard tests",
	0, crum_test_list, 0, 0, 0
};

static struct test_menu funkey_menu = {
	0, 'n', 0,
	"Test function keys:",
	"fkey", "n) run standard tests",
	sync_test, funkey_test_list, 0, 0, 0
};

static struct test_menu printer_menu = {
	0, 'n', 0,
	"Test printer:",
	"printer", "n) run standard tests",
	0, printer_test_list, 0, 0, 0
};

static void pad_gen(struct test_list *, int *, int *);

static struct test_menu pad_menu = {
	0, 'n', 0,
	"Test padding and string capabilities:",
	"pad", "n) run standard tests",
	sync_test, pad_test_list, 0, 0, 0
};

static struct test_list normal_test_list[] = {
	{0, 0, 0, 0, "e) edit terminfo", 0, &edit_menu},
	{0, 0, 0, 0, "i) send reset and init", menu_reset_init, 0},
	{MENU_NEXT, 0, 0, 0, "x) test modes and glitches", 0, &mode_menu},
	{MENU_NEXT, 0, 0, 0, "a) test alternate character set and graphic rendition", 0, &acs_menu},
	{MENU_NEXT, 0, 0, 0, "c) test color", 0, &color_menu},
	{MENU_NEXT, 0, 0, 0, "m) test cursor movement", 0, &crum_menu},
	{MENU_NEXT, 0, 0, 0, "f) test function keys", 0, &funkey_menu},
	{MENU_NEXT, 0, 0, 0, "p) test padding and string capabilities", 0, &pad_menu},
	{0, 0, 0, 0, "P) test printer", 0, &printer_menu},
	{MENU_MENU, 0, 0, 0, "/) test a specific capability", 0, 0},
	{0, 0, 0, 0, "t) auto generate pad delays", pad_gen, &pad_menu},
	{0, 0, "u8) (u9", 0, 0, sync_handshake, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};


static struct test_menu normal_menu = {
	0, 'n', 0, "Main test menu",
	"test", "n) run standard tests",
	0, normal_test_list, 0, 0, 0
};

static void start_tools(struct test_list *, int *, int *);
static void start_modes(struct test_list *, int *, int *);
static void start_basic(struct test_list *, int *, int *);
static void start_log(struct test_list *, int *, int *);

static char logging_menu_entry[80] = "l) start logging";

static struct test_list start_test_list[] = {
	{0, 0, 0, 0, "b) display basic information", start_basic, 0},
	{0, 0, 0, 0, "m) change modes", start_modes, 0},
	{0, 0, 0, 0, "t) tools", start_tools, 0},
	{MENU_COMPLETE, 0, 0, 0, "n) begin testing", 0, &normal_menu},
	{0, 0, 0, 0, logging_menu_entry, start_log, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};
	

static struct test_menu start_menu = {
	0, 'n', 0, "Main Menu", "tack", 0,
	0, start_test_list, 0, 0, 0
};

static struct test_list write_terminfo_list[] = {
	{0, 0, 0, 0, "w) write the current terminfo to a file", save_info, 0},
	{MENU_LAST, 0, 0, 0, 0, 0, 0}
};

/*****************************************************************************
 *
 * Menu command interpretation.
 *
 *****************************************************************************/

/*
**	tools_hex_echo(testlist, state, ch)
**
**	Flip the hex echo flag.
*/
static void
tools_hex_echo(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	if (hex_out) {
		hex_out = FALSE;
		strcpy(hex_echo_menu_entry,
			"h) enable hex output on echo tool");
	} else {
		hex_out = TRUE;
		strcpy(hex_echo_menu_entry,
			"h) disable hex output on echo tool");
	}
}

/*
**	tools_debug(testlist, state, ch)
**
**	Change the debug level.
*/
static void
tools_debug(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	char buf[32];

	ptext("Enter a new value: ");
	read_string(buf, sizeof(buf));
	if (buf[0]) {
		sscanf(buf, "%d", &debug_level);
	}
	sprintf(temp, "Debug level is now %d", debug_level);
	ptext(temp);
	*ch = REQUEST_PROMPT;
}

/*
**	start_tools(testlist, state, ch)
**
**	Run the generic test tools
*/
static void
start_tools(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	if (hex_out) {
		strcpy(hex_echo_menu_entry,
			"h) disable hex output on echo tool");
	} else {
		strcpy(hex_echo_menu_entry,
			"h) enable hex output on echo tool");
	}
	menu_display(&tools_menu, 0);
}

/*
**	tty_show_state()
**
**	Display the current state on the tty driver settings
*/
static void
tty_show_state(
	struct test_menu *menu GCC_UNUSED)
{
	put_crlf();
	(void) sprintf(temp,
		"Accepting %d bits, UNIX delays %d, XON/XOFF %sabled, speed %u, translate %s, scan-code mode %s.",
		(char_mask == ALLOW_PARITY) ? 8 : 7,
		select_delay_type,
		select_xon_xoff ? "en" : "dis",
		tty_baud_rate,
		translate_mode ? "on" : "off",
		scan_mode ? "on" : "off");
	ptextln(temp);
	put_crlf();
}

/*
**	tty_width(testlist, state, ch)
**
**	Change the character width
*/
static void
tty_width(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	if (char_mask == STRIP_PARITY) {
		char_mask = ALLOW_PARITY;
		strcpy(tty_width_menu, "7) treat terminal as 7-bit");
	} else {
		char_mask = STRIP_PARITY;
		strcpy(tty_width_menu, "8) treat terminal as 8-bit");
	}
}

/*
**	tty_delay(testlist, state, ch)
**
**	Change the delay for <cr><lf> in the TTY driver
*/
static void
tty_delay(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	if (select_delay_type) {
		select_delay_type = FALSE;
		strcpy(tty_delay_menu,
			"d) enable UNIX tty driver delays for <cr><lf>");
	} else {
		select_delay_type = TRUE;
		strcpy(tty_delay_menu,
			"d) disable UNIX tty driver delays for <cr><lf>");
	}
}

/*
**	tty_xon(testlist, state, ch)
**
**	Change the XON/XOFF flags in the TTY driver
*/
static void
tty_xon(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	if (select_xon_xoff) {
		if (needs_xon_xoff) {
			ptextln("This terminal is marked as needing XON/XOFF protocol with (nxon)");
		}
		if (exit_xon_mode) {
			tc_putp(exit_xon_mode);
		}
		xon_xoff = select_xon_xoff = FALSE;
		strcpy(tty_xon_menu, enable_xon_xoff);
	} else {
		if (enter_xon_mode) {
			tc_putp(enter_xon_mode);
		}
		xon_xoff = select_xon_xoff = TRUE;
		strcpy(tty_xon_menu, disable_xon_xoff);
	}
	tty_set();
}

/*
**	tty_trans(testlist, state, ch)
**
**	Change the translation mode for special characters
*/
static void
tty_trans(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	if (translate_mode) {
		translate_mode = FALSE;
		strcpy(tty_trans_menu,
			"t) use terminfo values for \\b\\f\\n\\r\\t");
	} else {
		translate_mode = TRUE;
		strcpy(tty_trans_menu,
			"t) override terminfo values for \\b\\f\\n\\r\\t");
	}
}

/*
**	pad_gen(testlist, state, ch)
**
**	Menu function for automatic pad generation
*/
static void
pad_gen(
	struct test_list *t,
	int *state GCC_UNUSED,
	int *ch)
{
	control_init();
	if (tty_can_sync == SYNC_NOT_TESTED) {
		verify_time();
	}
	auto_pad_mode = TRUE;
	menu_display(t->sub_menu, ch);
	auto_pad_mode = FALSE;
}

/*
**	start_modes(testlist, state, ch)
**
**	Change the TTY modes
*/
static void
start_modes(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{

	if (select_delay_type) {
		strcpy(tty_delay_menu,
			"d) disable UNIX tty driver delays for <cr><lf>");
	} else {
		strcpy(tty_delay_menu,
			"d) enable UNIX tty driver delays for <cr><lf>");
	}
	if (char_mask == ALLOW_PARITY) {
		strcpy(tty_width_menu,
			"7) treat terminal as 7-bit");
	} else {
		strcpy(tty_width_menu,
			"8) treat terminal as 8-bit");
	}
	if (select_xon_xoff) {
		strcpy(tty_xon_menu, disable_xon_xoff);
	} else {
		strcpy(tty_xon_menu, enable_xon_xoff);
	}
	if (translate_mode) {
		strcpy(tty_trans_menu,
			"t) override terminfo values for \\b\\f\\n\\r\\t");
	} else {
		strcpy(tty_trans_menu,
			"t) use terminfo values for \\b\\f\\n\\r\\t");
	}
	menu_display(&tty_menu, 0);
	tty_set();
}

/*
**	start_basic(testlist, state, ch)
**
**	Display basic terminal information
*/
static void
start_basic(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	display_basic();
	*ch = REQUEST_PROMPT;
}

/*
**	start_log(testlist, state, ch)
**
**	Start/stop in logging function
*/
static void
start_log(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch GCC_UNUSED)
{
	if (logging_menu_entry[5] == 'a') {
		ptextln("The log file will capture all characters sent to the terminal.");
		if ((log_fp = fopen("tack.log", "w"))) {
			ptextln("Start logging to file: tack.log");
			strcpy(logging_menu_entry, "l) stop logging");
		} else {
			ptextln("File open error: tack.log");
		}
	} else {
		if (log_fp) {
			fclose(log_fp);
			log_fp = 0;
		}
		ptextln("Terminal output logging stopped.");
		strcpy(logging_menu_entry, "l) start logging");
	}
}

/*
**	show_usage()
**
**	Tell the user how its done.
*/
void
show_usage(
	char *name)
{
	(void) fprintf(stderr, "usage: %s [-itV] [term]\n", name);
}

/*
**	print_version()
**
**	Print version and other useful information.
*/
void
print_version(void)
{
	printf("tack version %d.%02d\n", MAJOR_VERSION, MINOR_VERSION);
	printf("Copyright (C) 1997 Free Software Foundation, Inc.\n");
	printf("Tack comes with NO WARRANTY, to the extent permitted by law.\n");
	printf("You may redistribute copies of Tack under the terms of the\n");
	printf("GNU General Public License.  For more information about\n");
	printf("these matters, see the file named COPYING.\n");
}


/*****************************************************************************
 *
 * Main sequence
 *
 *****************************************************************************/

int
main(int argc, char *argv[])
{
	int i, j;
	char *term_variable;

	/* scan the option flags */
	send_reset_init = TRUE;
	translate_mode = FALSE;
	term_variable = getenv("TERM");
	tty_can_sync = SYNC_NOT_TESTED;
	for (i = 1; i < argc; i++) {
		if (argv[i][0] == '-') {
			for (j = 1; argv[i][j]; j++) {
				switch (argv[i][j]) {
				case 'V':
					print_version();
					return (1);
				case 'i':
					send_reset_init = FALSE;
					break;
				case 't':
					translate_mode = FALSE;
					break;
				default:
					show_usage(argv[0]);
					return (0);
				}
			}
		} else {
			term_variable = argv[i];
		}
	}
	(void) strcpy(tty_basename, term_variable);

	curses_setup(argv[0]);

	menu_can_scan(&normal_menu);	/* extract which caps can be tested */
	menu_display(&start_menu, 0);

	if (user_modified()) {
		sprintf(temp, "Hit y to save changes to file: %s  ? ",
			tty_basename);
		ptext(temp);
		if (wait_here() == 'y') {
			save_info(write_terminfo_list, &i, &j);
		}
	}

	put_str("\nTerminal test complete\n");
	bye_kids(0);
	return (0);
}
