/*
 * Copyright (c) 1983, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 */

#ifndef lint
static char sccsid[] = "@(#)ttgeneric.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] =
  "$FreeBSD$";
#endif /* not lint */

#include "ww.h"
#include "tt.h"

char PC, *BC, *UP;

	/* normal frame */
short gen_frame[16] = {
	' ', '|', '-', '+',
	'|', '|', '+', '+',
	'-', '+', '-', '+',
	'+', '+', '+', '+'
};

	/* ANSI graphics frame */
#define G (WWM_GRP << WWC_MSHIFT)
short ansi_frame[16] = {
	' ',	'x'|G,	'Q'|G,	'm'|G,
	'x'|G,	'x'|G,	'l'|G,	't'|G,
	'q'|G,	'j'|G,	'q'|G,	'v'|G,
	'k'|G,	'u'|G,	'w'|G,	'n'|G
};
struct tt_str ansi_AS = {
	"\033(0", 3
};

struct tt_str *gen_PC;
struct tt_str *gen_CM;
struct tt_str *gen_IM;
struct tt_str *gen_IC;
struct tt_str *gen_ICn;
struct tt_str *gen_IP;
struct tt_str *gen_EI;
struct tt_str *gen_DC;
struct tt_str *gen_DCn;
struct tt_str *gen_AL;
struct tt_str *gen_ALn;
struct tt_str *gen_DL;
struct tt_str *gen_DLn;
struct tt_str *gen_CE;
struct tt_str *gen_CD;
struct tt_str *gen_CL;
struct tt_str *gen_VS;
struct tt_str *gen_VE;
struct tt_str *gen_TI;
struct tt_str *gen_TE;
struct tt_str *gen_SO;
struct tt_str *gen_SE;
struct tt_str *gen_US;
struct tt_str *gen_UE;
struct tt_str *gen_LE;
struct tt_str *gen_ND;
struct tt_str *gen_UP;
struct tt_str *gen_DO;
struct tt_str *gen_BC;
struct tt_str *gen_NL;
struct tt_str *gen_CR;
struct tt_str *gen_HO;
struct tt_str *gen_AS;
struct tt_str *gen_AE;
struct tt_str *gen_XS;
struct tt_str *gen_XE;
struct tt_str *gen_SF;
struct tt_str *gen_SFn;
struct tt_str *gen_SR;
struct tt_str *gen_SRn;
struct tt_str *gen_CS;
char gen_MI;
char gen_MS;
char gen_AM;
char gen_OS;
char gen_BS;
char gen_DA;
char gen_DB;
char gen_NS;
char gen_XN;
int gen_CO;
int gen_LI;
int gen_UG;
int gen_SG;

gen_setinsert(new)
char new;
{
	if (new) {
		if (gen_IM)
			ttxputs(gen_IM);
	} else
		if (gen_EI)
			ttxputs(gen_EI);
	tt.tt_insert = new;
}

gen_setmodes(new)
register new;
{
	register diff;

	diff = new ^ tt.tt_modes;
	if (diff & WWM_REV) {
		if (new & WWM_REV) {
			if (gen_SO)
				ttxputs(gen_SO);
		} else
			if (gen_SE)
				ttxputs(gen_SE);
	}
	if (diff & WWM_UL) {
		if (new & WWM_UL) {
			if (gen_US)
				ttxputs(gen_US);
		} else
			if (gen_UE)
				ttxputs(gen_UE);
	}
	if (diff & WWM_GRP) {
		if (new & WWM_GRP) {
			if (gen_AS)
				ttxputs(gen_AS);
		} else
			if (gen_AE)
				ttxputs(gen_AE);
	}
	if (diff & WWM_USR) {
		if (new & WWM_USR) {
			if (gen_XS)
				ttxputs(gen_XS);
		} else
			if (gen_XE)
				ttxputs(gen_XE);
	}
	tt.tt_modes = new;
}

gen_insline(n)
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	if (gen_ALn)
		ttpgoto(gen_ALn, 0, n, gen_LI - tt.tt_row);
	else
		while (--n >= 0)
			tttputs(gen_AL, gen_LI - tt.tt_row);
}

gen_delline(n)
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	if (gen_DLn)
		ttpgoto(gen_DLn, 0, n, gen_LI - tt.tt_row);
	else
		while (--n >= 0)
			tttputs(gen_DL, gen_LI - tt.tt_row);
}

gen_putc(c)
register char c;
{
	if (tt.tt_insert)
		gen_setinsert(0);
	if (tt.tt_nmodes != tt.tt_modes)
		gen_setmodes(tt.tt_nmodes);
	ttputc(c);
	if (++tt.tt_col == gen_CO)
		if (gen_XN)
			tt.tt_col = tt.tt_row = -10;
		else if (gen_AM)
			tt.tt_col = 0, tt.tt_row++;
		else
			tt.tt_col--;
}

gen_write(p, n)
	register char *p;
	register n;
{
	if (tt.tt_insert)
		gen_setinsert(0);
	if (tt.tt_nmodes != tt.tt_modes)
		gen_setmodes(tt.tt_nmodes);
	ttwrite(p, n);
	tt.tt_col += n;
	if (tt.tt_col == gen_CO)
		if (gen_XN)
			tt.tt_col = tt.tt_row = -10;
		else if (gen_AM)
			tt.tt_col = 0, tt.tt_row++;
		else
			tt.tt_col--;
}

gen_move(row, col)
register int row, col;
{
	if (tt.tt_row == row && tt.tt_col == col)
		return;
	if (!gen_MI && tt.tt_insert)
		gen_setinsert(0);
	if (!gen_MS && tt.tt_modes)
		gen_setmodes(0);
	if (row < tt.tt_scroll_top || row > tt.tt_scroll_bot)
		gen_setscroll(0, tt.tt_nrow - 1);
	if (tt.tt_row == row) {
		if (col == 0) {
			ttxputs(gen_CR);
			goto out;
		}
		if (tt.tt_col == col - 1) {
			if (gen_ND) {
				ttxputs(gen_ND);
				goto out;
			}
		} else if (tt.tt_col == col + 1) {
			if (gen_LE) {
				ttxputs(gen_LE);
				goto out;
			}
		}
	}
	if (tt.tt_col == col) {
		if (tt.tt_row == row + 1) {
			if (gen_UP) {
				ttxputs(gen_UP);
				goto out;
			}
		} else if (tt.tt_row == row - 1) {
			ttxputs(gen_DO);
			goto out;
		}
	}
	if (gen_HO && col == 0 && row == 0) {
		ttxputs(gen_HO);
		goto out;
	}
	tttgoto(gen_CM, col, row);
out:
	tt.tt_col = col;
	tt.tt_row = row;
}

gen_start()
{
	if (gen_VS)
		ttxputs(gen_VS);
	if (gen_TI)
		ttxputs(gen_TI);
	ttxputs(gen_CL);
	tt.tt_col = tt.tt_row = 0;
	tt.tt_insert = 0;
	tt.tt_nmodes = tt.tt_modes = 0;
}

gen_end()
{
	if (tt.tt_insert)
		gen_setinsert(0);
	if (gen_TE)
		ttxputs(gen_TE);
	if (gen_VE)
		ttxputs(gen_VE);
}

gen_clreol()
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	tttputs(gen_CE, gen_CO - tt.tt_col);
}

gen_clreos()
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	tttputs(gen_CD, gen_LI - tt.tt_row);
}

gen_clear()
{
	if (tt.tt_modes)			/* for concept 100 */
		gen_setmodes(0);
	ttxputs(gen_CL);
}

gen_inschar(c)
register char c;
{
	if (!tt.tt_insert)
		gen_setinsert(1);
	if (tt.tt_nmodes != tt.tt_modes)
		gen_setmodes(tt.tt_nmodes);
	if (gen_IC)
		tttputs(gen_IC, gen_CO - tt.tt_col);
	ttputc(c);
	if (gen_IP)
		tttputs(gen_IP, gen_CO - tt.tt_col);
	if (++tt.tt_col == gen_CO)
		if (gen_XN)
			tt.tt_col = tt.tt_row = -10;
		else if (gen_AM)
			tt.tt_col = 0, tt.tt_row++;
		else
			tt.tt_col--;
}

gen_insspace(n)
{
	if (gen_ICn)
		ttpgoto(gen_ICn, 0, n, gen_CO - tt.tt_col);
	else
		while (--n >= 0)
			tttputs(gen_IC, gen_CO - tt.tt_col);
}

gen_delchar(n)
{
	if (gen_DCn)
		ttpgoto(gen_DCn, 0, n, gen_CO - tt.tt_col);
	else
		while (--n >= 0)
			tttputs(gen_DC, gen_CO - tt.tt_col);
}

gen_scroll_down(n)
{
	gen_move(tt.tt_scroll_bot, 0);
	if (gen_SFn)
		ttpgoto(gen_SFn, 0, n, n);
	else
		while (--n >= 0)
			ttxputs(gen_SF);
}

gen_scroll_up(n)
{
	gen_move(tt.tt_scroll_top, 0);
	if (gen_SRn)
		ttpgoto(gen_SRn, 0, n, n);
	else
		while (--n >= 0)
			ttxputs(gen_SR);
}

gen_setscroll(top, bot)
{
	tttgoto(gen_CS, bot, top);
	tt.tt_scroll_top = top;
	tt.tt_scroll_bot = bot;
	tt.tt_row = tt.tt_col = -10;
}

tt_generic()
{
	gen_PC = tttgetstr("pc");
	PC = gen_PC ? *gen_PC->ts_str : 0;

	gen_CM = ttxgetstr("cm");		/* may not work */
	gen_IM = ttxgetstr("im");
	gen_IC = tttgetstr("ic");
	gen_ICn = tttgetstr("IC");
	gen_IP = tttgetstr("ip");
	gen_EI = ttxgetstr("ei");
	gen_DC = tttgetstr("dc");
	gen_DCn = tttgetstr("DC");
	gen_AL = tttgetstr("al");
	gen_ALn = tttgetstr("AL");
	gen_DL = tttgetstr("dl");
	gen_DLn = tttgetstr("DL");
	gen_CE = tttgetstr("ce");
	gen_CD = tttgetstr("cd");
	gen_CL = ttxgetstr("cl");
	gen_VS = ttxgetstr("vs");
	gen_VE = ttxgetstr("ve");
	gen_TI = ttxgetstr("ti");
	gen_TE = ttxgetstr("te");
	gen_SO = ttxgetstr("so");
	gen_SE = ttxgetstr("se");
	gen_US = ttxgetstr("us");
	gen_UE = ttxgetstr("ue");
	gen_LE = ttxgetstr("le");
	gen_ND = ttxgetstr("nd");
	gen_UP = ttxgetstr("up");
	gen_DO = ttxgetstr("do");
	gen_BC = ttxgetstr("bc");
	gen_NL = ttxgetstr("nl");
	gen_CR = ttxgetstr("cr");
	gen_HO = ttxgetstr("ho");
	gen_AS = ttxgetstr("as");
	gen_AE = ttxgetstr("ae");
	gen_XS = ttxgetstr("XS");
	gen_XE = ttxgetstr("XE");
	gen_SF = ttxgetstr("sf");
	gen_SFn = ttxgetstr("SF");
	gen_SR = ttxgetstr("sr");
	gen_SRn = ttxgetstr("SR");
	gen_CS = ttxgetstr("cs");
	gen_MI = tgetflag("mi");
	gen_MS = tgetflag("ms");
	gen_AM = tgetflag("am");
	gen_OS = tgetflag("os");
	gen_BS = tgetflag("bs");
	gen_DA = tgetflag("da");
	gen_DB = tgetflag("db");
	gen_NS = tgetflag("ns");
	gen_XN = tgetflag("xn");
	gen_CO = tgetnum("co");
	gen_LI = tgetnum("li");
	gen_UG = tgetnum("ug");
	gen_SG = tgetnum("sg");
	if (gen_CL == 0 || gen_OS || gen_CM == 0)
		return -1;

	/*
	 * Deal with obsolete termcap fields.
	 */
	if (gen_LE == 0)
		if (gen_BC)
			gen_LE = gen_BC;
		else if (gen_BS) {
			static struct tt_str bc = { "\b", 1 };
			gen_BC = &bc;
		}
	if (gen_NL == 0) {
		static struct tt_str nl = { "\n", 1 };
		gen_NL = &nl;
	}
	if (gen_DO == 0)
		gen_DO = gen_NL;
	if (gen_CR == 0) {
		static struct tt_str cr = { "\r", 1 };
		gen_CR = &cr;
	}
	/*
	 * Most terminal will scroll with "nl", but very few specify "sf".
	 * We shouldn't use "do" here.
	 */
	if (gen_SF == 0 && !gen_NS)
		gen_SF = gen_NL;
	BC = gen_LE ? gen_LE->ts_str : 0;
	UP = gen_UP ? gen_UP->ts_str : 0;
	/*
	 * Fix up display attributes that we can't handle, or don't
	 * really exist.
	 */
	if (gen_SG > 0)
		gen_SO = 0;
	if (gen_UG > 0 || gen_US && gen_SO && ttstrcmp(gen_US, gen_SO) == 0)
		gen_US = 0;

	if (gen_IM && gen_IM->ts_n == 0) {
		free((char *) gen_IM);
		gen_IM = 0;
	}
	if (gen_EI && gen_EI->ts_n == 0) {
		free((char *) gen_EI);
		gen_EI = 0;
	}
	if (gen_IC && gen_IC->ts_n == 0) {
		free((char *) gen_IC);
		gen_IC = 0;
	}
	if (gen_IM)
		tt.tt_inschar = gen_inschar;
	else if (gen_IC)
		tt.tt_insspace = gen_insspace;
	if (gen_DC)
		tt.tt_delchar = gen_delchar;
	if (gen_AL)
		tt.tt_insline = gen_insline;
	if (gen_DL)
		tt.tt_delline = gen_delline;
	if (gen_CE)
		tt.tt_clreol = gen_clreol;
	if (gen_CD)
		tt.tt_clreos = gen_clreos;
	if (gen_SF)
		tt.tt_scroll_down = gen_scroll_down;
	/*
	 * Don't allow scroll_up if da or db but not cs.
	 * See comment in wwscroll.c.
	 */
	if (gen_SR && (gen_CS || !gen_DA && !gen_DB))
		tt.tt_scroll_up = gen_scroll_up;
	if (gen_CS)
		tt.tt_setscroll = gen_setscroll;
	if (gen_SO)
		tt.tt_availmodes |= WWM_REV;
	if (gen_US)
		tt.tt_availmodes |= WWM_UL;
	if (gen_AS)
		tt.tt_availmodes |= WWM_GRP;
	if (gen_XS)
		tt.tt_availmodes |= WWM_USR;
	tt.tt_wrap = gen_AM;
	tt.tt_retain = gen_DB;
	tt.tt_ncol = gen_CO;
	tt.tt_nrow = gen_LI;
	tt.tt_start = gen_start;
	tt.tt_end = gen_end;
	tt.tt_write = gen_write;
	tt.tt_putc = gen_putc;
	tt.tt_move = gen_move;
	tt.tt_clear = gen_clear;
	tt.tt_setmodes = gen_setmodes;
	tt.tt_frame = gen_AS && ttstrcmp(gen_AS, &ansi_AS) == 0 ?
		ansi_frame : gen_frame;
	return 0;
}
