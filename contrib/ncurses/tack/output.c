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
/* screen formatting and I/O utility functions */

#include <tack.h>
#include <time.h>

MODULE_ID("$Id: output.c,v 1.4 1999/06/16 00:46:53 tom Exp $")

/* globals */
long char_sent;			/* number of characters sent */
int char_count;			/* counts characters */
int line_count;			/* counts line feeds */
int expand_chars;		/* length of expand() string */
int replace_mode;		/* used to output replace mode padding */
int can_go_home;		/* TRUE if we can fashion a home command */
int can_clear_screen;		/* TRUE if we can somehow clear the screen */
int raw_characters_sent;	/* Total output characters */
int log_count;			/* Number of characters on a log line */

/* translate mode default strings */
#define TM_carriage_return	TM_string[0].value
#define TM_cursor_down		TM_string[1].value
#define TM_scroll_forward	TM_string[2].value
#define TM_newline		TM_string[3].value
#define TM_cursor_left		TM_string[4].value
#define TM_bell			TM_string[5].value
#define TM_form_feed		TM_string[6].value
#define TM_tab			TM_string[7].value

struct default_string_list TM_string[TM_last] = {
	{"cr", "\r", 0},
	{"cud1", "\n", 0},
	{"ind", "\n", 0},
	{"nel", "\r\n", 0},
	{"cub1", "\b", 0},
	{"bel", "\007", 0},
	{"ff", "\f", 0},
	{"ht", "\t", 0}
};

static const char *c0[32] = {
	"NUL", "SOH", "STX", "ETX", "EOT", "ENQ", "ACK",
	"BEL", "BS", "HT", "LF", "VT", "FF", "CR", "SO", "SI",
	"DLE", "DC1", "DC2", "DC3", "DC4", "NAK", "SYN", "ETB",
	"CAN", "EM", "SUB", "ESC", "FS", "GS", "RS", "US"
};

static const char *c1[32] = {
	"", "", "", "", "IND", "NEL", "SSA", "ESA",
	"HTS", "HTJ", "VTS", "PLD", "PLU", "RI", "SS2", "SS3",
	"DCS", "PU1", "PU2", "STS", "CCH", "MW", "SPA", "EPA",
	"", "", "", "CSI", "ST", "OSC", "PM", "APC"
};

int
getnext(int mask)
{				/* get the next character without scan mode
				   conversion */
	int ch;
	unsigned char buf;

	tc_putp(req_for_input);
	fflush(stdout);
	if (nodelay_read)
		while (1) {
			ch = read(fileno(stdin), &buf, 1);
			if (ch == -1)
				return EOF;
			if (ch == 1)
				return buf;
		}
	ch = getchar();
	if (ch == EOF)
		return EOF;
	return ch & mask;
}


int
getchp(int mask)
{				/* read a character with scan mode conversion */
	if (scan_mode) {
		tc_putp(req_for_input);
		fflush(stdout);
		return scan_key();
	} else
		return getnext(mask);
}

/*
**	tc_putch(c)
**
**	Output one character
*/
int
tc_putch(int c)
{
	char_sent++;
	raw_characters_sent++;
	putchar(c);
	if ((raw_characters_sent & 31) == 31) {
		fflush(stdout);
	}
	if (log_fp) {
		/* terminal output logging */
		c &= 0xff;
		if (c < 32) {
			fprintf(log_fp, "<%s>", c0[c]);
			log_count += 5;
		} else
		if (c < 127) {
			fprintf(log_fp, "%c", c);
			log_count += 1;
		} else {
			fprintf(log_fp, "<%02x>", c);
			log_count += 4;
		}
		if (c == '\n' || log_count >= 80) {
			fprintf(log_fp, "\n");
			log_count = 0;
		}
	}
	return (c);
}

/*
**	tt_tputs(string, reps)
**
**	Output a string with tputs() translation.
**	Use this function inside timing tests.
*/
void
tt_tputs(const char *string, int reps)
{
	int i;

	if (string) {
		for (i = 0; i < TT_MAX; i++) {
			if (i >= ttp) {
				tt_cap[i] = string;
				tt_affected[i] = reps;
				tt_count[i] = 1;
				tt_delay[i] = msec_cost(string, reps);
				ttp++;
				break;
			}
			if (string == tt_cap[i] && reps == tt_affected[i]) {
				tt_count[i]++;
				tt_delay_used += tt_delay[i];
				break;
			}
		}
		(void) tputs(string, reps, tc_putch);
	}
}

/*
**	tt_putp(string)
**
**	Output a string with tputs() translation.
**	Use this function inside timing tests.
*/
void
tt_putp(const char *string)
{
	tt_tputs(string, 1);
}

/*
**	tt_putparm(string, reps, arg1, arg2, ...)
**
**	Send tt_tputs(tparm(string, args...), reps)
**	Use this function inside timing tests.
*/
void
tt_putparm(
	NCURSES_CONST char *string,
	int reps,
	int arg1,
	int arg2)
{
	int i;

	if (string) {
		for (i = 0; i < TT_MAX; i++) {
			if (i >= ttp) {
				tt_cap[i] = string;
				tt_affected[i] = reps;
				tt_count[i] = 1;
				tt_delay[i] = msec_cost(string, reps);
				ttp++;
				break;
			}
			if (string == tt_cap[i] && reps == tt_affected[i]) {
				tt_count[i]++;
				tt_delay_used += tt_delay[i];
				break;
			}
		}
		(void) tputs(tparm((NCURSES_CONST char *)string, arg1, arg2), reps, tc_putch);
	}
}

/*
**	tc_putp(string)
**
**	Output a string with tputs() translation.
**	Use this function instead of putp() so we can track
**	the actual number of characters sent.
*/
int
tc_putp(const char *string)
{
	return tputs(string, 1, tc_putch);
}


void
put_this(int c)
{				/* output one character (with padding) */
	tc_putch(c);
	if (char_padding && replace_mode)
		tt_putp(char_padding);
}


void
put_cr(void)
{
	if (translate_mode && carriage_return) {
		tt_putp(carriage_return);
	} else {
		tt_putp(TM_carriage_return);
	}
	char_count = 0;
}


void
put_lf(void)
{				/* send a linefeed (only works in RAW or
				   CBREAK mode) */
	if (translate_mode && cursor_down) {
		tt_putp(cursor_down);
	} else {
		tt_putp(TM_cursor_down);
	}
	line_count++;
}


void
put_ind(void)
{				/* scroll forward (only works in RAW or
				   CBREAK mode) */
	if (translate_mode && scroll_forward) {
		tt_putp(scroll_forward);
	} else {
		tt_putp(TM_scroll_forward);
	}
	line_count++;
}

/*
**	put_crlf()
**
**	Send (nel)  or <cr> <lf>
*/
void
put_crlf(void)
{
	if (translate_mode && newline) {
		tt_putp(newline);
	} else {
		tt_putp(TM_newline);
	}
	char_count = 0;
	line_count++;
}

/*
**	put_new_lines(count)
**
**	Send a number of newlines. (nel)
*/
void
put_newlines(int n)
{
	while (n-- > 0) {
		put_crlf();
	}
}

/*
**	putchp(character)
**
**	Send one character to the terminal.
**	This function does translation of control characters.
*/
void
putchp(int c)
{
	switch (c) {
	case '\b':
		if (translate_mode && cursor_left) {
			tt_putp(cursor_left);
		} else {
			tt_putp(TM_cursor_left);
		}
		char_count--;
		break;
	case 7:
		if (translate_mode && bell) {
			tt_putp(bell);
		} else {
			tt_putp(TM_bell);
		}
		break;
	case '\f':
		if (translate_mode && form_feed) {
			tt_putp(form_feed);
		} else {
			tt_putp(TM_form_feed);
		}
		char_count = 0;
		line_count++;
		break;
	case '\n':
		put_crlf();
		break;
	case '\r':
		put_cr();
		break;
	case '\t':
		if (translate_mode && tab) {
			tt_putp(tab);
		} else {
			tt_putp(TM_tab);
		}
		char_count = ((char_count / 8) + 1) * 8;
		break;
	default:
		put_this(c);
		char_count++;
		break;
	}
}


void
put_str(const char *s)
{				/* send the string to the terminal */
	for (; *s; putchp(*s++));
}


void
putln(const char *s)
{				/* output a string followed by a CR LF */
	for (; *s; putchp(*s++));
	put_crlf();
}


void
put_columns(const char *s, int len, int w)
{				/* put out s in column format */
	int l;

	if (char_count + w > columns) {
		put_crlf();
	}
	l = char_count % w;
	if (l) {
		while (l < w) {
			putchp(' ');
			l++;
		}
	}
	if (char_count && char_count + len >= columns) {
		put_crlf();
	}
	l = char_count;
	put_str(s);
	char_count = l + len;
}


/*
**	ptext(string)
**
**	Output a string but do not assume the terminal will wrap to a
**	new line.  Break the line at a word boundry then send a CR LF.
**	This is more estetic on 40 column terminals.
*/
void
ptext(const char *s)
{
	const char *t;

	while (*s) {
		for (t = s + 1; *t > ' '; t++);
		if ((char_count != 0) && ((t - s) + char_count >= columns)) {
			put_crlf();
			while (*s == ' ')
				s++;
		}
		while (s < t) {
			putchp(*s++);
		}
	}
}


void
put_dec(char *f, int i)
{				/* print a line with a decimal number in it */
	char tm[128];

	sprintf(tm, f, i / 10, i % 10);
	ptext(tm);
}


void
three_digit(char *tx, int i)
{				/* convert the decimal number to a string of
				   at least 3 digits */
	if (i < 1000)
		sprintf(tx, "%d.%d", i / 10, i % 10);
	else
		sprintf(tx, "%d", i / 10);
}


void
ptextln(const char *s)
{				/* print the text using ptext() then add a CR
				   LF */
	ptext(s);
	put_crlf();
}


static void
expand_one(int ch, char **v)
{				/* expand one character */
	char *t = *v;

	if (ch & 0x80) {	/* dump it in octal (yuck) */
		*t++ = '\\';
		*t++ = '0' + ((ch >> 6) & 3);
		*t++ = '0' + ((ch >> 3) & 7);
		*t++ = '0' + (ch & 7);
		expand_chars += 4;
	} else if (ch == 127) {	/* DEL */
		*t++ = '^';
		*t++ = '?';
		expand_chars += 2;
	} else if (ch >= ' ') {
		*t++ = ch;
		expand_chars++;
	} else {	/* control characters */
		*t++ = '^';
		*t++ = ch + '@';
		expand_chars += 2;
	}
	*v = t;
}


char *
expand(const char *s)
{				/* convert the string to printable form */
	static char buf[4096];
	char *t, *v;
	int ch;

	if (magic_cookie_glitch <= 0 && exit_attribute_mode) {
		v = enter_reverse_mode;
	} else {
		v = NULL;
	}
	expand_chars = 0;
	t = buf;
	if (s) {
		for (; (ch = *s); s++) {
			if ((ch & 0x80) && v) {	/* print it in reverse video
						   mode */
				strcpy(t, liberated(tparm(v)));
				for (; *t; t++);
				expand_one(ch & 0x7f, &t);
				strcpy(t, liberated(tparm(exit_attribute_mode)));
				for (; *t; t++);
			} else {
				expand_one(ch, &t);
			}
		}
	}
	*t = '\0';
	return buf;
}


char *
print_expand(char *s)
{				/* convert the string to 7-bit printable form */
	static char buf[4096];
	char *t;
	int ch;

	expand_chars = 0;
	t = buf;
	if (s) {
		for (; (ch = *s); s++) {
			expand_one(ch, &t);
		}
	}
	*t = '\0';
	return buf;
}


char *
expand_to(char *s, int l)
{				/* expand s to length l */
	char *t;

	for (s = t = expand(s); *t; t++);
	for (; expand_chars < l; expand_chars++) {
		*t++ = ' ';
	}
	*t = '\0';
	return s;
}


char *
hex_expand_to(char *s, int l)
{				/* expand s to length l in hex */
	static char buf[4096];
	char *t;

	for (t = buf; *s; s++) {
		sprintf(t, "%02X ", *s & 0xff);
		t += 3;
		if (t - buf > (int) sizeof(buf) - 4) {
			break;
		}
	}
	for (; t - buf < l;) {
		*t++ = ' ';
	}
	*t = '\0';
	expand_chars = t - buf;
	return buf;
}


char *
expand_command(const char *c)
{				/* expand an ANSI escape sequence */
	static char buf[256];
	int i, j, ch;
	char *s;

	s = buf;
	for (i = FALSE; (ch = (*c & 0xff)); c++) {
		if (i) {
			*s++ = ' ';
		}
		i = TRUE;
		if (ch < 32) {
			j = c[1] & 0xff;
			if (ch == '\033' && j >= '@' && j <= '_') {
				ch = j - '@';
				c++;
				for (j = 0; (*s = c1[ch][j++]); s++);
			} else
				for (j = 0; (*s = c0[ch][j++]); s++);
		} else {
			*s++ = ch;
			j = c[1] & 0xff;
			if (ch >= '0' && ch <= '9' &&
				j >= '0' && j <= '9') {
				i = FALSE;
			}
		}
	}
	*s = '\0';
	return buf;
}

/*
**	go_home()
**
**	Move the cursor to the home position
*/
void
go_home(void)
{
	int i;

	if (cursor_home)
		tt_putp(cursor_home);
	else if (cursor_address)
		tt_putparm(cursor_address, lines, 0, 0);
	else if (row_address) {	/* use (vpa) */
		put_cr();
		tt_putparm(row_address, 1, 0, 0);
	} else if (cursor_up && cursor_to_ll) {
		tt_putp(cursor_to_ll);
		for (i = 1; i < lines; i++) {
			tt_putp(cursor_up);
		}
	} else {
		can_go_home = FALSE;
		return;
	}
	char_count = line_count = 0;
	can_go_home = TRUE;
}


void
home_down(void)
{				/* move the cursor to the lower left hand
				   corner */
	int i;

	if (cursor_to_ll)
		tt_putp(cursor_to_ll);
	else if (cursor_address)
		tt_putparm(cursor_address, lines, lines - 1, 0);
	else if (row_address) {	/* use (vpa) */
		put_cr();
		tt_putparm(row_address, 1, lines - 1, 0);
	} else if (cursor_down && cursor_home) {
		tt_putp(cursor_home);
		for (i = 1; i < lines; i++)
			tt_putp(cursor_down);
	} else
		return;
	char_count = 0;
	line_count = lines - 1;
}


void
put_clear(void)
{				/* clear the screen */
	int i;

	if (clear_screen)
		tt_tputs(clear_screen, lines);
	else if (clr_eos && can_go_home) {
		go_home();
		tt_tputs(clr_eos, lines);
	} else if (scroll_forward && !over_strike && (can_go_home || cursor_up)) {
		/* clear the screen by scrolling */
		put_cr();
		if (cursor_to_ll) {
			tt_putp(cursor_to_ll);
		} else if (cursor_address) {
			tt_putparm(cursor_address, lines, lines - 1, 0);
		} else if (row_address) {
			tt_putparm(row_address, 1, lines - 1, 0);
		} else {
			for (i = 1; i < lines; i++) {
				tt_putp(scroll_forward);
			}
		}
		for (i = 1; i < lines; i++) {
			tt_putp(scroll_forward);
		}
		if (can_go_home) {
			go_home();
		} else {
			for (i = 1; i < lines; i++) {
				tt_putp(cursor_up);
			}
		}
	} else {
		can_clear_screen = FALSE;
		return;
	}
	char_count = line_count = 0;
	can_clear_screen = TRUE;
}

/*
**	wait_here()
**
**	read one character from the input stream
**	If the terminal is not in RAW mode then this function will
**	wait for a <cr> or <lf>.
*/
int
wait_here(void)
{
	char ch, cc[64];
	char message[16];
	int i, j;

	for (i = 0; i < (int) sizeof(cc); i++) {
		cc[i] = ch = getchp(STRIP_PARITY);
		if (ch == '\r' || ch == '\n') {
			put_crlf();
			char_sent = 0;
			return cc[i ? i - 1 : 0];
		}
		if (ch >= ' ') {
			if (stty_query(TTY_CHAR_MODE)) {
				put_crlf();
				char_sent = 0;
				return ch;
			}
			continue;
		}
		if (ch == 023) {	/* Control S */
			/* ignore control S, but tell me about it */
			while (ch == 023 || ch == 021) {
				ch = getchp(STRIP_PARITY);
				if (i < (int) sizeof(cc))
					cc[++i] = ch;
			}
			put_str("\nThe terminal sent a ^S -");
			for (j = 0; j <= i; j++) {
				sprintf(message, " %02X", cc[j] & 0xFF);
				put_str(message);
			}
			put_crlf();
			i = -1;
		} else if (ch != 021) {	/* Not Control Q */
			/* could be abort character */
			spin_flush();
			if (tty_can_sync == SYNC_TESTED) {
				(void) tty_sync_error();
			} else {
				put_str("\n? ");
			}
		}
	}
	return '?';
}


/*
**	read_string(buffer, length)
**
**	Read a string of characters from the input stream.
*/
void
read_string(
	char *buf,
	int length)
{
	int ch, i;

	for (i = 0; i < length - 1; ) {
		ch = getchp(STRIP_PARITY);
		if (ch == '\r' || ch == '\n') {
			break;
		}
		if (ch == '\b' || ch == 127) {
			if (i) {
				putchp('\b');
				putchp(' ');
				putchp('\b');
				i--;
			}
		} else {
			buf[i++] = ch;
			putchp(ch);
		}
	}
	buf[i] = '\0';
	put_crlf();
	char_sent = 0;
}

/*
**	maybe_wait(lines)
**
**	wait if near the end of the screen, then clear screen
*/
void 
maybe_wait(int n)
{
	if (line_count + n >= lines) {
		if (char_sent != 0) {
			ptext("Go? ");
			(void) wait_here();
		}
		put_clear();
	} else {
		put_crlf();
	}
}
