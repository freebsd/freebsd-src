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

MODULE_ID("$Id: ansi.c,v 1.9 2001/06/18 18:44:17 tom Exp $")

/*
 * Standalone tests for ANSI terminals.  Three entry points:
 * test_ansi_graphics(), test_ansi_reports() and test_ansi_sgr().
 */

/*****************************************************************************
 *
 * Test ANSI status reports
 *
 *****************************************************************************/

/* ASCII control characters */
#define A_DC1 0x11		/* Control Q */
#define A_DC3 0x13		/* Control S */
#define A_ESC 0x1b
#define A_DCS 0x90
#define A_CSI 0x9b
#define A_ST  0x9c

#define MAX_MODES 256

static char default_bank[] = "\033(B\017";
static int private_use, ape, terminal_class;
static short ansi_value[256];
static unsigned char ansi_buf[512], pack_buf[512];

struct ansi_reports {
	int lvl, final;
	const char *text;
	const char *request;
};

static struct ansi_reports report_list[] = {
	{0, 'c', "(DA) Primary device attributes", "\033[0c"},
	{1, 0, "(DSR) Terminal status", "\033[5n"},
	{1, 'R', "(DSR) Cursor position", "\033[6n"},
	{62, 0, "(DA) Secondary device attributes", "\033[>0c"},
	{62, 0, "(DSR) Printer status", "\033[?15n"},
	{62, 0, "(DSR) Function key definition", "\033[?25n"},
	{62, 0, "(DSR) Keyboard language", "\033[?26n"},
	{63, 0, "(DECRQSS) Data destination", "\033P$q$}\033\\"},
	{63, 0, "(DECRQSS) Status line type", "\033P$q$~\033\\"},
	{63, 0, "(DECRQSS) Erase attribute", "\033P$q\"q\033\\"},
	{63, 0, "(DECRQSS) Personality", "\033P$q\"p\033\\"},
	{63, 0, "(DECRQSS) Top and bottom margins", "\033P$qr\033\\"},
	{63, 0, "(DECRQSS) Character attributes", "\033P$qm\033\\"},
	{63, 0, "(DECRQSS) Illegal request", "\033P$q@\033\\"},
	{63, 0, "(DECRQUPSS) User pref supplemental set", "\033[&u"},
	{63, 0, "(DECRQPSR) Cursor information", "\033[1$w"},
	{63, 0, "(DECRQPSR) Tab stop information", "\033[2$w"},
	{64, 0, "(DA) Tertiary device attributes", "\033[=0c"},
	{64, 0, "(DSR) Extended cursor position", "\033[?6n"},
	{64, 0, "(DSR) Macro space", "\033[?62n"},
	{64, 0, "(DSR) Memory checksum", "\033[?63n"},
	{64, 0, "(DSR) Data integrity", "\033[?75n"},
	{64, 0, "(DSR) Multiple session status", "\033[?85n"},
	{64, 0, "(DECRQSS) Attribute change extent", "\033P$q*x\033\\"},
	{64, 0, "(DECRQSS) Columns per page", "\033P$q$|\033\\"},
	{64, 0, "(DECRQSS) Lines per page", "\033P$qt\033\\"},
	{64, 0, "(DECRQSS) Lines per screen", "\033P$q*|\033\\"},
	{64, 0, "(DECRQSS) Left and right margins", "\033P$qs\033\\"},
	{64, 0, "(DECRQSS) Local functions", "\033P$q+q\033\\"},
	{64, 0, "(DECRQSS) Local function key control", "\033P$q=}\033\\"},
	{64, 0, "(DECRQSS) Select modifier key reporting", "\033P$q+r\033\\"},
	{64, 0, "(DECRQDE) Window report", "\033[\"v"},
	{0, 0, 0, 0}
};

struct request_control {
	const char *text;
	const char *expect;
	const char *request;
	const char *set_mode;
	const char *reset_mode;
};

/* Request control function selection or setting */
static const struct request_control rqss[] = {
	{"Data sent to screen", "0", "$}", "\033[0$}", 0},
	{"Data sent to disabled status line", "0", "$}", 0, 0},
	{"\033[0$~\033[1$}", "\033[0$}", 0, 0, 0},
	{"Data sent to enabled status line", "1", "$}", 0, 0},
	{"\033[2$~\033[1$}", "\033[0$}", 0, 0, 0},
	{"Disable status line", "0", "$~", "\033[0$~", 0},
	{"Top status line", "1", "$~", "\033[1$~", 0},
	{"Bottom status line", "2", "$~", "\033[2$~", 0},
	{"Erasable character", "0", "\"q", "\033[0\"q", 0},
	{"Nonerasable character", "1", "\"q", "\033[1\"q", "\033[0\"q"},
	{"Top and bottom margins", "3;10", "r", "\0337\033[3;10r", 0},
	{"\033[r\0338", 0, 0, 0, 0},
	{"Top and bottom margins", "default", "r", "\0337\033[r", "\0338"},
	{"Character attributes, dim, bold", "1", "m", "\033[2;1m", "\033[m"},
	{"Character attributes, bold, dim", "2", "m", "\033[1;2m", "\033[m"},
	{"Character attributes, under, rev", "4;7", "m", "\033[4;7m", "\033[m"},
	{"Character attributes, color", "35;42", "m", "\033[35;42m", "\033[m"},
	{"All character attributes", "", "m", "\033[1;2;3;4;5;6;7;8;9m", 0},
	{"\033[m", 0, 0, 0, 0},
	{0, 0, 0, 0, 0}
};


/*
**	read_ansi()
**
**	read an ANSI status report from terminal
*/
static void
read_ansi(void)
{
	int ch, i, j, last_escape;

	fflush(stdout);
	read_key((char *)ansi_buf, sizeof(ansi_buf));
	/* Throw away control characters inside CSI sequences.
	   Convert two character 7-bit sequences into 8-bit sequences. */
	for (i = j = last_escape = 0; (ch = ansi_buf[i]) != 0; i++) {
		if (ch == A_ESC) {
			if (last_escape == A_ESC) {
				pack_buf[j++] = A_ESC;
			}
			last_escape = A_ESC;
		} else
		if (last_escape == A_ESC && ch >= '@' && ch <= '_') {
			pack_buf[j++] = last_escape = ch + 0x40;
		} else
		if (last_escape != A_CSI || (ch > 0x20 && ch != 0x80)) {
			if (last_escape == A_ESC) {
				pack_buf[j++] = A_ESC;
			}
			if (ch > 0x80 && ch < 0xa0) {
				last_escape = ch;
			}
			pack_buf[j++] = ch;
		}
	}
	if (last_escape == A_ESC) {
		pack_buf[j++] = A_ESC;
	}
	pack_buf[j] = '\0';
	return;
}

/*
**	valid_mode(expected)
**
**	read a terminal mode status report and parse the result
**	Return TRUE if we got the expected terminating character.
*/
static int
valid_mode(int expected)
{
	unsigned char *s;
	int ch, terminator;

	read_ansi();

	ape = 0;
	ch = UChar(pack_buf[0]);
	ansi_value[0] = 0;
	if (ch != A_CSI && ch != A_DCS)
		return FALSE;

	s = pack_buf + 1;
	private_use = 0;
	if ((*s >= '<') & (*s <= '?')) {
		private_use = *s++;
	}
	terminator = 0;
	for (; (ch = *s); s++) {
		if (ch >= '0' && ch <= '9')
			ansi_value[ape] = ansi_value[ape] * 10 + ch - '0';
		else if (ch == ';' || ch == ':')
			ansi_value[++ape] = 0;
		else if (ch >= '<' && ch <= '?')
			private_use = ch;
		else if (ch >= ' ')
			terminator = (terminator << 8) | ch;
		else
			break;
	}
	return terminator == expected;
}

/*
**	read_reports()
**
**	read all the reports in the ANSI report structure
*/
static int
read_reports(void)
{
	int i, j, k, tc, vcr, lc;
	char *s;
	const char *t;

	lc = 5;
	terminal_class = tc = 0;
	for (i = 0; report_list[i].text; i++, lc++) {
		if (terminal_class < report_list[i].lvl &&
			tc < report_list[i].lvl) {
			put_crlf();
			menu_prompt();
			ptext("/status [q] > ");
			j = wait_here();
			if (j != 'n' && j != 'N')
				return 0;
			tc = report_list[i].lvl;
			lc = 1;
		} else if (lc + 2 >= lines) {
			put_crlf();
			ptext("Hit any key to continue ");
			(void) wait_here();
			lc = 1;
		}
		sprintf(temp, "%s (%s) ", report_list[i].text,
			expand_command(report_list[i].request));
		ptext(temp);
		for (j = strlen(temp); j < 49; j++)
			putchp(' ');
		tc_putp(report_list[i].request);
		vcr = 0;
		if (report_list[i].final == 0) {
			read_ansi();
		} else if (valid_mode(report_list[i].final))
			switch (report_list[i].final) {
			case 'c':
				terminal_class = ansi_value[0];
				break;
			case 'R':
				vcr = TRUE;
				break;
			}
		j = UChar(pack_buf[0]);
		if (j != A_CSI && j != A_DCS) {
			put_crlf();
			t = "*** The above request gives illegal response ***";
			ptext(t);
			for (j = strlen(t); j < 49; j++)
				putchp(' ');
		}
		s = expand((const char *)ansi_buf);
		if (char_count + expand_chars >= columns) {
			put_str("\r\n        ");
			lc++;
		}
		putln(s);
		if (vcr) {	/* find out how big the screen is */
			tc_putp(report_list[i].request);
			if (!valid_mode('R'))
				continue;
			j = ansi_value[0];
			k = ansi_value[1];
			tc_putp("\033[255B\033[255C\033[6n");
			if (!valid_mode('R'))
				continue;
			sprintf(temp, "\033[%d;%dH", j, k);
			tc_putp(temp);
			ptext("(DSR) Screen size (CSI 6 n)");
			for (j = char_count; j < 50; j++)
				putchp(' ');
			sprintf(temp, "%d x %d", ansi_value[1], ansi_value[0]);
			ptextln(temp);

		}
	}
	menu_prompt();
	ptext("/status r->repeat test, <return> to continue > ");
	return wait_here();
}

/*
**	request_cfss()
**
**	Request Control function selection or settings
*/
static int
request_cfss(void)
{
	int i, j, k, l, ch;
	char *s;

	put_clear();
	ptextln("Request                         Expected  Received");
	put_crlf();
	for (i = 0; rqss[i].text; i++) {
		ptext(rqss[i].text);
		j = strlen(rqss[i].text) + strlen(rqss[i].expect);
		putchp(' ');
		for (j++; j < 40; j++)
			putchp(' ');
		ptext(rqss[i].expect);
		putchp(' ');
		tc_putp(rqss[i].set_mode);
		sprintf(temp, "\033P$q%s\033\\", rqss[i].request);
		tc_putp(temp);
		read_ansi();
		tc_putp(rqss[i].reset_mode);
		putchp(' ');
		for (j = 0; ansi_buf[j]; j++) {
			if (ansi_buf[j] == 'r') {
				for (k = j++; (ch = UChar(ansi_buf[k])) != 0; k++)
					if (ch == A_ESC) {
						break;
					} else if (ch == A_ST) {
						break;
					}
				ansi_buf[k] = '\0';
				s = expand((const char *)&ansi_buf[j]);
				if (char_count + expand_chars >= columns)
					put_str("\r\n        ");
				put_str(s);
			}
		}
		put_crlf();
	}
	/* calculate the valid attributes */
	ptext("Valid attributes:         0");
	j = 0;
	for (i = 1; i < 20; i++) {
		sprintf(temp, "\033[0;%dm\033P$qm\033\\", i);
		tc_putp(temp);
		(void) valid_mode('m');
		if (ape > 0) {
			j = i;
			sprintf(temp, "\033[0m; %d", i);
			tc_putp(temp);
		}
	}
	put_crlf();
	/* calculate how many parameters can be sent */
	ptext("Max number of parameters: ");
	sprintf(temp, "%dm\033P$qm\033\\", j);
	l = -1;
	if (j > 0)
		for (l = 1; l < 33; l++) {
			tc_putp("\033[0");
			for (ch = 1; ch <= l; ch++)
				put_this(';');
			tc_putp(temp);
			(void) valid_mode('m');
			if (ape == 0)
				break;
		}
	tc_putp("\033[m");
	if (l >= 0) {
		sprintf(temp, "%d", l);
		ptext(temp);
	} else
		ptext("unknown");
	put_crlf();
	return wait_here();
}

/*
**	mode_display(puc, mode, initial, set, reset)
**
**	print the mode display entry
*/
static void
mode_display(const char *p, int n, int c, char s, char r)
{
	int k;

	sprintf(temp, "%s%d (%c, %c, %c)", p, n, c, s, r);
	k = strlen(temp);
	if (char_count + k >= columns)
		put_crlf();
	for (; k < 14; k++)
		putchp(' ');
	put_str(temp);
}

/*
**	terminal_state()
**
**	test DECRQM status reports
*/
static void
terminal_state(void)
{
	static const char *puc[] = {"", "<", "=", ">", "?", 0};

	int i, j, k, l, modes_found;
	char *s;
	char buf[256], tms[256];
	int mode_puc[MAX_MODES], mode_number[MAX_MODES];
	char set_value[MAX_MODES], reset_value[MAX_MODES];
	char current_value[MAX_MODES];

	ptext("Testing terminal mode status. (CSI 0 $ p)");
	tc_putp("\033[0$p");
	modes_found = 0;
	tms[0] = '\0';
	if (valid_mode(('$' << 8) | 'y')) {
		for (i = 0; puc[i]; i++) {
			put_crlf();
			if (i) {
				sprintf(temp, "Private use: %c", puc[i][0]);
			} else {
				strcpy(temp, "Standard modes:");
			}
			k = strlen(temp);
			ptext(temp);
			for (j = 0; j < (int) sizeof(buf); buf[j++] = ' ')
				;
			for (j = l = 0; j < 255 && j - l < 50; j++) {
				sprintf(temp, "\033[%s%d$p", puc[i], j);
				tc_putp(temp);
				if (!valid_mode(('$' << 8) | 'y')) {
					/* not valid, save terminating value */
					s = expand((const char *)ansi_buf);
					sprintf(tms, "%s%s%d %s  ", tms,
						puc[i], j, s);
					break;
				}
				if (private_use != puc[i][0])
					break;
				if (ansi_value[0] != j)
					break;
				if (ansi_value[1]) {
					l = j;
					if (k > 70) {
						buf[k] = '\0';
						put_crlf();
						ptextln(buf);
						for (k = 0; k < (int) sizeof(buf);) {
							buf[k++] = ' ';
						}
						k = 0;
					}
					sprintf(temp, " %d", j);
					ptext(temp);
					k += strlen(temp);
					buf[k - 1] = ansi_value[1] + '0';
					if (modes_found >= MAX_MODES)
						continue;
					current_value[modes_found] =
						ansi_value[1] + '0';
					/* some modes never return */
					if ((i == 0 && j == 13)	/* control execution */
						|| (puc[i][0] == '?' && j == 2))	/* VT52 */
						set_value[modes_found] =
							reset_value[modes_found] = '-';
					else
						set_value[modes_found] =
							reset_value[modes_found] = ' ';
					mode_puc[modes_found] = i;
					mode_number[modes_found++] = j;
				}
			}
			buf[k] = '\0';
			if (buf[k - 1] != ' ') {
				put_crlf();
				ptext(buf);
			}
		}

	if ((i = modes_found) != 0) {
		put_crlf();
		put_crlf();
		if (tms[0]) {
			ptextln(tms);
		}
		ptext("Hit 'Y' to test mode set/reset states: ");
		i = wait_here();
	}
	if (i == 'y' || i == 'Y')
		while (1) {
#ifdef STATUSFIX
			FILE *fp;

#ifdef TEDANSI
			fp = fopen("ted.ansi", "w");
#else
			fp = fopen("/dev/console", "w");
#endif
#endif
			for (i = j = 0; j < modes_found; j = ++i >> 1) {
				if (set_value[j] == '-')
					continue;
				k = (current_value[j] ^ i) & 1;
				sprintf(temp, "\033[%s%d%c\033[%s%d$p",
					puc[mode_puc[j]], mode_number[j],
					k ? 'l' : 'h',
					puc[mode_puc[j]], mode_number[j]);
#ifdef STATUSFIX
				if (fp) {
					fprintf(fp, "%s\n", expand(temp));
					fflush(fp);
				}
#endif
				tc_putp(temp);
				if (!valid_mode(('$' << 8) | 'y'))
					continue;
				if (k) {
					reset_value[j] = ansi_value[1] + '0';
				} else {
					set_value[j] = ansi_value[1] + '0';
				}
			}
			put_str("\033[30l");	/* added for GORT bug
						   (WY-185) */
#ifdef STATUSFIX
			if (fp)
				fclose(fp);
#endif
			tty_set();
			/* print the results */
			put_clear();
			putln("mode (initial, set, reset)");
			for (j = 0; j < modes_found; j++) {
				mode_display(puc[mode_puc[j]], mode_number[j],
					current_value[j], set_value[j], reset_value[j]);
			}
			ptext("\n\nHit 'R' to repeat test.  'S' to sort results: ");
			i = wait_here();
			if (i == 's' || i == 'S') {	/* print the same stuff,
							   sorted by
							   current_value */
				put_crlf();
				for (i = '1'; i <= '4'; i++) {
					for (j = 0; j < modes_found; j++) {
						if (current_value[j] == i)
							mode_display(puc[mode_puc[j]],
								mode_number[j], current_value[j],
								set_value[j], reset_value[j]);
					}
				}
				ptext("\n\nHit 'R' to repeat test: ");
				i = wait_here();
			}
			if (i != 'r' && i != 'R')
				break;
			tty_raw(1, char_mask);
		}
	} else {
		tty_set();
	}
}


/*
**	ansi_report_help()
**
**	Display the informational data for the ANSI report test.
*/
static void
ansi_report_help(void)
{
	ptext("Begin ANSI status report testing. ");
	ptext(" Parity bit set will be displayed in reverse video. ");
	ptext(" If the terminal hangs, hit any alphabetic key. ");
	ptextln(" Use n to continue testing.  Use q to quit.");
	put_crlf();
}

/*
**	test_ansi_reports()
**
**	Test the ANSI status report functions
*/
void
tools_status(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	int i;

	put_clear();
	ansi_report_help();
	tty_raw(1, char_mask);

	do {
		i = read_reports();
		if (i != 'r' && i != 'R') {
			*ch = i;
			return;
		}
	} while (i);

	if (terminal_class >= 63) {
		do {
			i = request_cfss();
		} while (i == 'r' || i == 'R');
		*ch = i;
		terminal_state();
	} else {
		tty_set();
	}
}


/*
**	display_sgr()
**
**	Test a range of ANSI sgr attributes
**	puc -> Private Use Character
*/
static void 
display_sgr(int puc)
{
	int k;

	temp[0] = puc;
	temp[1] = '\0';
	for (k = 0; k < 80; k++) {
		if (char_count + 8 > 80)
			put_crlf();
		else if (char_count + 8 > columns)
			put_crlf();
		else if (k > 0)
			printf(" ");
		printf("\033[%s%dmMode %2d\033[0m", temp, k, k);
		char_count += 8;
		if (puc == '\0') {
			if (k == 19)
				printf("\033[10m");
			if (k == 39)
				printf("\033[37m");
			if (k == 49)
				printf("\033[40m");
		}
	}
	put_crlf();
	if (puc == '<')
		printf("\033[<1m");
	else if (puc)
		printf("\033[%s0m", temp);
	set_attr(0);
}

/*
**	print_sgr20(on, off)
**
**	print the sgr line for sgr20()
*/
static void 
print_sgr20(int on, int off)
{
	if (char_count > columns - 13) {
		put_crlf();
	} else if (char_count) {
		put_str("  ");
	}
	char_count += 11;
	printf("%d/%d \033[%dmon\033[%dm off\033[0m", on, off, on, off);
}

/*
**	sgr20(void)
**
**	display the enter/exit attributes 1-9 and 20-29
*/
static void 
sgr20(void)
{
	int k;

	put_crlf();
	ptextln("Test enter/exit attributes 1-9 and 21-29.");
	for (k = 1; k < 10; k++) {
		print_sgr20(k, k + 20);
	}
	print_sgr20(1, 22);	/* bold */
	print_sgr20(2, 22);	/* dim */
	print_sgr20(8, 22);	/* blank */
	printf("\033[0m");
	set_attr(0);
}

/*
**	tools_sgr(testlist, state, ch)
**
**	Run the ANSI graphics rendition mode tool
**	Return the last character typed.
*/
void
tools_sgr(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *ch)
{
	int k;

	put_clear();
	for (k = 0;;) {
		display_sgr(k);
		put_crlf();
		menu_prompt();
		ptext("/sgr Enter =><?r [<cr>] > ");
		k = wait_here();
		if ((k == 'r') || (k == 'R')) {
			k = 0;
		} else if ((k < '<') || (k > '?')) {
			break;
		}
	}
	sgr20();

	put_newlines(2);
	*ch = REQUEST_PROMPT;
}

/*****************************************************************************
 *
 * Test ANSI graphics
 *
 *****************************************************************************/
/*
**	select_bank(bank)
**
**	select a graphics character set for ANSI terminals
*/
static void
select_bank(char *bank)
{
	tc_putp(bank);
	switch (bank[1] & 3) {
	case 0:
		putchp('O' & 0x1f);	/* control O */
		break;
	case 1:
		putchp('N' & 0x1f);	/* control N */
		tc_putp("\033~");
		break;
	case 2:
		tc_putp("\033n\033}");
		break;
	case 3:
		tc_putp("\033o\033|");
		break;
	}
}

/*
**	show_characters(bank, bias)
**
**	print the ANSI graphics characters
*/
static void
show_characters(char *bank, int bias)
{
	int i;

	sprintf(temp, "G%d GL   ", bank[1] & 3);
	ptext(temp);
	select_bank(bank);
	for (i = ' '; i < 0x80; i++) {
		if (char_count >= columns ||
			(i != ' ' && (i & 31) == 0))
			put_str("\n        ");
		putchp(i + bias);
	}
	select_bank(default_bank);
	put_str("   DEL <");
	select_bank(bank);
	putchp(0x7f + bias);
	select_bank(default_bank);
	putchp('>');
	put_crlf();
	put_crlf();
}


/* ANSI graphics test
        94     96   character sets
   G0   (      ,
   G1   )      -
   G2   *      .
   G3   +      /

Standard Definitions
   A    UK
   B    US ASCII

Dec extended definitions
   0    Special graphics

 */

/*
**	tools_charset(testlist, state, ch)
**
**	Run the ANSI alt-charset mode tool
*/
void
tools_charset(
	struct test_list *t GCC_UNUSED,
	int *state GCC_UNUSED,
	int *chp GCC_UNUSED)
{
	int j, ch;
	char bank[32];

	put_clear();
	ptext("Enter the bank ()*+,-./ followed by the character set");
	ptext(" 0123456789:;<=>? for private use, and");
	ptextln(" @A...Z[\\]^_`a...z{|}~ for standard sets.");
	strcpy(bank, "\033)0");
	for (; bank[0];) {
		put_crlf();
		show_characters(bank, 0);

		/* G0 will not print in GR */
		if (bank[1] & 3) {
			show_characters(bank, 0x80);
		}
		ptext("bank+set> ");
		for (j = 1; (ch = getchp(char_mask)); j++) {
			if (ch == EOF)
				break;
			putchp(ch);
			if (j == 1 && ch > '/')
				j++;
			bank[j] = ch;
			if (ch < ' ' || ch > '/')
				break;
			if (j + 1 >= (int) sizeof(bank))
				break;
		}
		if (j == 1)
			break;
		if (bank[j] < '0' || bank[j] > '~')
			break;
		bank[j + 1] = '\0';
	}
	put_crlf();
}
