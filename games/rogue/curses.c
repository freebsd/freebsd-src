/*
 * Copyright (c) 1988, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Timothy C. Stoehr.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#ifndef lint
static char sccsid[] = "@(#)curses.c	8.1 (Berkeley) 5/31/93";
#endif /* not lint */

/*
 * curses.c
 *
 * This source herein may be modified and/or distributed by anybody who
 * so desires, with the following restrictions:
 *    1.)  No portion of this notice shall be removed.
 *    2.)  Credit shall not be taken for the creation of this source.
 *    3.)  This code is not to be traded, sold, or used for personal
 *         gain or profit.
 *
 */

#ifdef CURSES

/* The following is a curses emulation package suitable for the rogue program
 * in which it is included.  No other suitability is claimed or suspected.
 * Only those routines currently needed by this rogue program are included.
 * This is being provided for those systems that don't have a suitable
 * curses package and want to run this rogue program.
 *
 * Compile the entire program with -DCURSES to incorporate this package.
 *
 * The following is NOT supported:
 *   "%D", "%B", "%n", or "%>" inside a cursor motion (cm) termcap string.
 *   Terminals in which the cursor motion addresses the row differently from
 *       the column, as in ":cm=\E%2,%3" or ":cm=\EY%+x;%+y"
 *   Termcap database stored in the TERMCAP environ variable as returned
 *       from md_getenv().  Only the termcap file name can be stored there.
 *       See the comments for md_getenv() in machdep.c.
 *   Terminals without non-destructive backspace.  Backspace (^H) is used
 *       for cursor motion regardless of any termcap entries.
 *   The ":tc=" termcap entry is ignored.
 *
 * Suggestions:
 *   Use line-feed as your termcap "do" entry: ":do=^J", ":do=\012" or
 *      ":do=\n"  This will help cursor motion optimization.  If line-feed
 *      won't work, then a short escape sequence will do.
 */

#include <stdio.h>
#include "rogue.h"

boolean tc_tname();

#define BS 010
#define LF 012
#define CR 015
#define ESC '\033'
#define TAB '\011'

#define ST_MASK 0x80
#define BUFLEN 256

char terminal[DROWS][DCOLS];
char buffer[DROWS][DCOLS];
char *tc_file;

char cm_esc[16];
char cm_sep[16];
char cm_end[16];
boolean cm_reverse = 0;
boolean cm_two = 0;
boolean cm_three = 0;
boolean cm_char = 0;
short cm_inc = 0;

boolean screen_dirty;
boolean lines_dirty[DROWS];
boolean buf_stand_out = 0;
boolean term_stand_out = 0;

int LINES = DROWS;
int COLS = DCOLS;
WINDOW scr_buf;
WINDOW *curscr = &scr_buf;

char *CL = (char *) 0;
char *CM = (char *) 0;
char *UC = (char *) 0;	/* UP */
char *DO = (char *) 0;
char *VS = "";
char *VE = "";
char *TI = "";
char *TE = "";
char *SO = "";
char *SE = "";

short cur_row;
short cur_col;

initscr()
{
	clear();
	get_term_info();
	printf("%s%s", TI, VS);
}

endwin()
{
	printf("%s%s", TE, VE);
	md_cbreak_no_echo_nonl(0);
}

move(row, col)
short row, col;
{
	curscr->_cury = row;
	curscr->_curx = col;
	screen_dirty = 1;
}

mvaddstr(row, col, str)
short row, col;
char *str;
{
	move(row, col);
	addstr(str);
}

addstr(str)
char *str;
{
	while (*str) {
		addch((int) *str++);
	}
}

addch(ch)
register int ch;
{
	short row, col;

	row = curscr->_cury;
	col = curscr->_curx++;

	if (buf_stand_out) {
		ch |= ST_MASK;
	}
	buffer[row][col] = (char) ch;
	lines_dirty[row] = 1;
	screen_dirty = 1;
}

mvaddch(row, col, ch)
short row, col;
int ch;
{
	move(row, col);
	addch(ch);
}

refresh()
{
	register i, j, line;
	short old_row, old_col, first_row;

	if (screen_dirty) {

		old_row = curscr->_cury;
		old_col = curscr->_curx;
		first_row = cur_row;

		for (i = 0; i < DROWS; i++) {
			line = (first_row + i) % DROWS;
			if (lines_dirty[line]) {
				for (j = 0; j < DCOLS; j++) {
					if (buffer[line][j] != terminal[line][j]) {
						put_char_at(line, j, buffer[line][j]);
					}
				}
				lines_dirty[line] = 0;
			}
		}
		put_cursor(old_row, old_col);
		screen_dirty = 0;
		fflush(stdout);
	}
}

wrefresh(scr)
WINDOW *scr;
{
	short i, col;

	printf("%s", CL);
	cur_row = cur_col = 0;

	for (i = 0; i < DROWS; i++) {
		col = 0;
		while (col < DCOLS) {
			while ((col < DCOLS) && (buffer[i][col] == ' ')) {
				col++;
			}
			if (col < DCOLS) {
				put_cursor(i, col);
			}
			while ((col < DCOLS) && (buffer[i][col] != ' ')) {
				put_st_char((int) buffer[i][col]);
				cur_col++;
				col++;
			}
		}
	}
	put_cursor(curscr->_cury, curscr->_curx);
	fflush(stdout);
	scr = scr;		/* make lint happy */
}

mvinch(row, col)
short row, col;
{
	move(row, col);
	return((int) buffer[row][col]);
}

clear()
{
	printf("%s", CL);
	fflush(stdout);
	cur_row = cur_col = 0;
	move(0, 0);
	clear_buffers();
}

clrtoeol()
{
	short row, col;

	row = curscr->_cury;

	for (col = curscr->_curx; col < DCOLS; col++) {
		buffer[row][col] = ' ';
	}
	lines_dirty[row] = 1;
}

standout()
{
	buf_stand_out = 1;
}

standend()
{
	buf_stand_out = 0;
}

crmode()
{
	md_cbreak_no_echo_nonl(1);
}

noecho()
{
	/* crmode() takes care of this */
}

nonl()
{
	/* crmode() takes care of this */
}

clear_buffers()
{
	register i, j;

	screen_dirty = 0;

	for (i = 0; i < DROWS; i++) {
		lines_dirty[i] = 0;
		for (j = 0; j < DCOLS; j++) {
			terminal[i][j] = ' ';
			buffer[i][j] = ' ';
		}
	}
}

put_char_at(row, col, ch)
register row, col, ch;
{
	put_cursor(row, col);
	put_st_char(ch);
	terminal[row][col] = (char) ch;
	cur_col++;
}

put_cursor(row, col)
register row, col;
{
	register i, rdif, cdif;
	short ch, t;

	rdif = (row > cur_row) ? row - cur_row : cur_row - row;
	cdif = (col > cur_col) ? col - cur_col : cur_col - col;

	if (((row > cur_row) && DO) || ((cur_row > row) && UC)) {
		if ((rdif < 4) && (cdif < 4)) {
			for (i = 0; i < rdif; i++) {
				printf("%s", ((row < cur_row) ? UC : DO));
			}
			cur_row = row;
			if (col == cur_col) {
				return;
			}
		}
	}
	if (row == cur_row) {
		if (cdif <= 6) {
		for (i = 0; i < cdif; i++) {
				ch = (col < cur_col) ? BS :
						terminal[row][cur_col + i];
				put_st_char((int) ch);
			}
			cur_row = row;
			cur_col = col;
			return;
		}
	}
	cur_row = row;
	cur_col = col;

	row += cm_inc;
	col += cm_inc;

	if (cm_reverse) {
		t = row;
		row = col;
		col = t;
	}
	if (cm_two) {
		printf("%s%02d%s%02d%s", cm_esc, row, cm_sep, col, cm_end);
	} else if (cm_three) {
		printf("%s%03d%s%03d%s", cm_esc, row, cm_sep, col, cm_end);
	} else if (cm_char) {
		printf("%s%c%s%c%s", cm_esc, row, cm_sep, col, cm_end);
	} else {
		printf("%s%d%s%d%s", cm_esc, row, cm_sep, col, cm_end);
	}
}

put_st_char(ch)
register ch;
{
	if ((ch & ST_MASK) && (!term_stand_out)) {
		ch &= ~ST_MASK;
		printf("%s%c", SO, ch);
		term_stand_out = 1;
	} else if ((!(ch & ST_MASK)) && term_stand_out) {
		printf("%s%c", SE, ch);
		term_stand_out = 0;
	} else {
		ch &= ~ST_MASK;
		putchar(ch);
	}
}

get_term_info()
{
	FILE *fp;
	char *term, *tcf;
	char buf[BUFLEN];

	if (tcf = md_getenv("TERMCAP")) {
		if (strlen(tcf) > 40) {
			clean_up("TERMCAP file name too long");
		}
		tc_file = tcf;
	} else {
		if (!(tc_file = md_gdtcf())) {
			clean_up("I need a termcap file");
		}
	}

	if (!(term = md_getenv("TERM"))) {
		clean_up("Cannot find TERM variable in environ");
	}
	if ((fp = fopen(tc_file, "r")) == NULL) {
		sprintf(buf, "Cannot open TERMCAP file: %s", tc_file);
		clean_up(buf);
	}

	if (!tc_tname(fp, term, buf)) {
		sprintf(buf, "Cannot find TERM type: %s in TERMCAP file: %s", term,
			tc_file);
		clean_up(buf);
	}
	tc_gtdata(fp, buf);
	fclose(fp);
}

boolean
tc_tname(fp, term, buf)
FILE *fp;
char *term;
char *buf;
{
	short i, j;
	boolean found = 0;
	char *fg;

	while (!found) {
		i = 0;
		fg = fgets(buf, BUFLEN, fp);
		if (fg != NULL) {
			if (	(buf[0] != '#') && (buf[0] != ' ') && (buf[0] != TAB) &&
					(buf[0] != CR) && (buf[0] != LF)) {
				while (buf[i] && (!found)) {
					j = 0;
					while (buf[i] == term[j]) {
						i++;
						j++;
					}
					if ((!term[j]) && ((buf[i] == '|') || (buf[i] == ':'))) {
						found = 1;
					} else {
						while (buf[i] && (buf[i] != '|') && (buf[i] != ':')) {
							i++;
						}
						if (buf[i]) {
							i++;
						}
					}
				}
			}
		} else {
			break;
		}
	}
	return(found);
}

tc_gtdata(fp, buf)
FILE *fp;
char *buf;
{
	short i;
	boolean first = 1;

	do {
		if (!first) {
			if ((buf[0] != TAB) && (buf[0] != ' ')) {
				break;
			}
		}
		first = 0;
		i = 0;
		while (buf[i]) {
			while (buf[i] && (buf[i] != ':')) {
				i++;
			}
			if (buf[i] == ':') {
				if (!strncmp(buf + i, ":cl=", 4)) {
					tc_gets(buf + i, &CL);
				} else if (!strncmp(buf + i, ":cm=", 4)) {
					tc_gets(buf + i, &CM);
				} else if (!strncmp(buf + i, ":up=", 4)) {
					tc_gets(buf + i, &UC);
				} else if (!strncmp(buf + i, ":do=", 4)) {
					tc_gets(buf + i, &DO);
				} else if (!strncmp(buf + i, ":vs=", 4)) {
					tc_gets(buf + i, &VS);
				} else if (!strncmp(buf + i, ":ve=", 4)) {
					tc_gets(buf + i, &VE);
				} else if (!strncmp(buf + i, ":ti=", 4)) {
					tc_gets(buf + i, &TI);
				} else if (!strncmp(buf + i, ":te=", 4)) {
					tc_gets(buf + i, &TE);
				} else if (!strncmp(buf + i, ":vs=", 4)) {
					tc_gets(buf + i, &VS);
				} else if (!strncmp(buf + i, ":ve=", 4)) {
					tc_gets(buf + i, &VE);
				} else if (!strncmp(buf + i, ":so=", 4)) {
					tc_gets(buf + i, &SO);
				} else if (!strncmp(buf + i, ":se=", 4)) {
					tc_gets(buf + i, &SE);
				} else if (!strncmp(buf + i, ":li#", 4)) {
					tc_gnum(buf + i, &LINES);
				} else if (!strncmp(buf + i, ":co#", 4)) {
					tc_gnum(buf + i, &COLS);
				}
				i++;
			}
		}
	} while (fgets(buf, BUFLEN, fp) != NULL);

	if ((!CM) || (!CL)) {
		clean_up("Terminal and termcap must have cm and cl");
	}
	tc_cmget();
}

tc_gets(ibuf, tcstr)
char *ibuf;
char **tcstr;
{
	short i, j, k, n;
	char obuf[BUFLEN];

	i = 4;
	j = 0;

	while (ibuf[i] && is_digit(ibuf[i])) {
		i++;
	}

	while (ibuf[i] && (ibuf[i] != ':')) {
		if (ibuf[i] == '\\') {
			i++;
			switch(ibuf[i]) {
			case 'E':
				obuf[j] = ESC;
				i++;
				break;
			case 'n':
				obuf[j] = LF;
				i++;
				break;
			case 'r':
				obuf[j] = CR;
				i++;
				break;
			case 'b':
				obuf[j] = BS;
				i++;
				break;
			case 't':
				obuf[j] = TAB;
				i++;
				break;
			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				n = 0;
				k = 0;
				while (k < 3 && ibuf[i] && is_digit(ibuf[i])) {
					n = (8 * n) + (ibuf[i] - '0');
					i++;
					k++;
				}
				obuf[j] = (char) n;
				break;
			default:
				obuf[j] = ibuf[i];
				i++;
			}
		} else if (ibuf[i] == '^') {
			obuf[j] = ibuf[i+1] - 64;
			i += 2;
		} else {
			obuf[j] = ibuf[i++];
		}
		j++;
	}
	obuf[j] = 0;
	if (!(*tcstr = md_malloc(j + 1))) {
		clean_up("cannot alloc() memory");
	}
	(void) strcpy(*tcstr, obuf);
}

tc_gnum(ibuf, n)
char *ibuf;
int *n;
{
	short i;
	int r = 0;

	i = 4;

	while (is_digit(ibuf[i])) {
		r = (r * 10) + (ibuf[i] - '0');
		i++;
	}
	*n = r;
}

tstp()
{
	endwin();
	md_tstp();

	start_window();
	printf("%s%s", TI, VS);
	wrefresh(curscr);
	md_slurp();
}

tc_cmget()
{
	short i = 0, j = 0, rc_spec = 0;

	while (CM[i] && (CM[i] != '%') && (j < 15)) {
		cm_esc[j++] = CM[i++];
	}
	cm_esc[j] = 0;

	while (CM[i] && (rc_spec < 2)) {
		if (CM[i] == '%') {
			i++;
			switch(CM[i]) {
			case 'd':
				rc_spec++;
				break;
			case 'i':
				cm_inc = 1;
				break;
			case '2':
				cm_two = 1;
				rc_spec++;
				break;
			case '3':
				cm_three = 1;
				rc_spec++;
				break;
			case '.':
				cm_char = 1;
				rc_spec++;
				break;
			case 'r':
				cm_reverse = 1;
				break;
			case '+':
				i++;
				cm_inc = CM[i];
				cm_char = 1;
				rc_spec++;
				break;
			}
			i++;
		} else {
			j = 0;
			while (CM[i] && (CM[i] != '%')) {
				cm_sep[j++] = CM[i++];
			}
			cm_sep[j] = 0;
		}
	}

	j = 0;
	if (rc_spec == 2) {
		while (CM[i] && (j < 15)) {
			cm_end[j++] = CM[i++];
		}
	}
	cm_end[j] = 0;
}

#endif
