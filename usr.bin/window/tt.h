/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Edward Wang at The University of California, Berkeley.
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
 *
 *	@(#)tt.h	3.27 (Berkeley) 6/6/90
 */

/*
 * Interface structure for the terminal drivers.
 */
struct tt {
		/* startup and cleanup */
	int (*tt_start)();
	int (*tt_end)();

		/* terminal functions */
	int (*tt_move)();
	int (*tt_insline)();
	int (*tt_delline)();
	int (*tt_inschar)();
	int (*tt_insspace)();
	int (*tt_delchar)();
	int (*tt_write)();		/* write a whole block */
	int (*tt_putc)();		/* write one character */
	int (*tt_clreol)();
	int (*tt_clreos)();
	int (*tt_clear)();
	int (*tt_scroll_down)();
	int (*tt_scroll_up)();
	int (*tt_setscroll)();		/* set scrolling region */
	int (*tt_setmodes)();		/* set display modes */
	int (*tt_set_token)();		/* define a token */
	int (*tt_put_token)();		/* refer to a defined token */

		/* internal variables */
	char tt_modes;			/* the current display modes */
	char tt_nmodes;			/* the new modes for next write */
	char tt_insert;			/* currently in insert mode */
	int tt_row;			/* cursor row */
	int tt_col;			/* cursor column */
	int tt_scroll_top;		/* top of scrolling region */
	int tt_scroll_bot;		/* bottom of scrolling region */

		/* terminal info */
	int tt_nrow;			/* number of display rows */
	int tt_ncol;			/* number of display columns */
	char tt_availmodes;		/* the display modes supported */
	char tt_wrap;			/* has auto wrap around */
	char tt_retain;			/* can retain below (db flag) */
	short tt_padc;			/* the pad character */
	int tt_ntoken;			/* number of compression tokens */
	int tt_token_min;		/* minimun token size */
	int tt_token_max;		/* maximum token size */
	int tt_set_token_cost;		/* cost in addition to string */
	int tt_put_token_cost;		/* constant cost */

		/* the frame characters */
	short *tt_frame;

		/* the output routine */
	int (*tt_flush)();
};
struct tt tt;

/*
 * tt_padc is used by the compression routine.
 * It is a short to allow the driver to indicate that there is no padding.
 */
#define TT_PADC_NONE 0x100

/*
 * List of terminal drivers.
 */
struct tt_tab {
	char *tt_name;
	int tt_len;
	int (*tt_func)();
};
extern struct tt_tab tt_tab[];

/*
 * Clean interface to termcap routines.
 * Too may t's.
 */
char tt_strings[1024];		/* string buffer */
char *tt_strp;			/* pointer for it */

struct tt_str {
	char *ts_str;
	int ts_n;
};

struct tt_str *tttgetstr();
struct tt_str *ttxgetstr();	/* tgetstr() and expand delays */

int tttputc();
#define tttputs(s, n)	tputs((s)->ts_str, (n), tttputc)
#define ttxputs(s)	ttwrite((s)->ts_str, (s)->ts_n)

/*
 * Buffered output without stdio.
 * These variables have different meanings from the ww_ob* variables.
 * But I'm too lazy to think up different names.
 */
char *tt_ob;
char *tt_obp;
char *tt_obe;
#define ttputc(c)	(tt_obp < tt_obe ? (*tt_obp++ = (c)) \
				: ((*tt.tt_flush)(), *tt_obp++ = (c)))

/*
 * Convenience macros for the drivers
 * They require char.h
 */
#define ttctrl(c)	ttputc(ctrl(c))
#define ttesc(c)	(ttctrl('['), ttputc(c))
