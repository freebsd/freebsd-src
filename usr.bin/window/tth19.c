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
static char sccsid[] = "@(#)tth19.c	8.1 (Berkeley) 6/6/93";
static char rcsid[] = "@(#)$FreeBSD$";
#endif /* not lint */

#include "ww.h"
#include "tt.h"
#include "char.h"

/*
kb|h19|heath|h19-b|h19b|heathkit|heath-19|z19|zenith:
	cr=^M:nl=^J:bl=^G:al=1*\EL:am:le=^H:bs:cd=\EJ:ce=\EK:
	cl=\EE:cm=\EY%+ %+ :co#80:dc=\EN:dl=1*\EM:do=\EB:
	ei=\EO:ho=\EH:im=\E@:li#24:mi:nd=\EC:as=\EF:ae=\EG:ms:
	ta=^I:pt:sr=\EI:se=\Eq:so=\Ep:up=\EA:vs=\Ex4:ve=\Ey4:
	kb=^h:ku=\EA:kd=\EB:kl=\ED:kr=\EC:kh=\EH:
	kn#8:k1=\ES:k2=\ET:k3=\EU:k4=\EV:k5=\EW:
	l6=blue:l7=red:l8=white:k6=\EP:k7=\EQ:k8=\ER:
	es:hs:ts=\Ej\Ex5\Ex1\EY8%+ \Eo:fs=\Ek\Ey5:ds=\Ey1:
*/

#define NCOL	80
#define NROW	24

#define G (WWM_GRP << WWC_MSHIFT)
short h19_frame[16] = {
	' ',	'`'|G,	'a'|G,	'e'|G,
	'`'|G,	'`'|G,	'f'|G,	'v'|G,
	'a'|G,	'd'|G,	'a'|G,	'u'|G,
	'c'|G,	't'|G,	's'|G,	'b'|G
};

extern struct tt_str *gen_VS;
extern struct tt_str *gen_VE;

int h19_msp10c;

#define PAD(ms10) { \
	register i; \
	for (i = ((ms10) + 5) / h19_msp10c; --i >= 0;) \
		ttputc('\0'); \
}
#define ICPAD() PAD((NCOL - tt.tt_col) * 1)	/* 0.1 ms per char */
#define ILPAD() PAD((NROW - tt.tt_row) * 10)	/* 1 ms per char */

#define H19_SETINSERT(m) ttesc((tt.tt_insert = (m)) ? '@' : 'O')

h19_setmodes(new)
register new;
{
	register diff;

	diff = new ^ tt.tt_modes;
	if (diff & WWM_REV)
		ttesc(new & WWM_REV ? 'p' : 'q');
	if (diff & WWM_GRP)
		ttesc(new & WWM_REV ? 'F' : 'G');
	tt.tt_modes = new;
}

h19_insline(n)
{
	while (--n >= 0) {
		ttesc('L');
		ILPAD();
	}
}

h19_delline(n)
{
	while (--n >= 0) {
		ttesc('M');
		ILPAD();
	}
}

h19_putc(c)
register char c;
{
	if (tt.tt_nmodes != tt.tt_modes)
		(*tt.tt_setmodes)(tt.tt_nmodes);
	if (tt.tt_insert)
		H19_SETINSERT(0);
	ttputc(c);
	if (++tt.tt_col == NCOL)
		tt.tt_col = NCOL - 1;
}

h19_write(p, n)
register char *p;
register n;
{
	if (tt.tt_nmodes != tt.tt_modes)
		(*tt.tt_setmodes)(tt.tt_nmodes);
	if (tt.tt_insert)
		H19_SETINSERT(0);
	ttwrite(p, n);
	tt.tt_col += n;
	if (tt.tt_col == NCOL)
		tt.tt_col = NCOL - 1;
}

h19_move(row, col)
register char row, col;
{
	if (tt.tt_row == row) {
		if (tt.tt_col == col)
			return;
		if (col == 0) {
			ttctrl('m');
			goto out;
		}
		if (tt.tt_col == col - 1) {
			ttesc('C');
			goto out;
		}
		if (tt.tt_col == col + 1) {
			ttctrl('h');
			goto out;
		}
	}
	if (tt.tt_col == col) {
		if (tt.tt_row == row + 1) {
			ttesc('A');
			goto out;
		}
		if (tt.tt_row == row - 1) {
			ttctrl('j');
			goto out;
		}
	}
	if (col == 0 && row == 0) {
		ttesc('H');
		goto out;
	}
	ttesc('Y');
	ttputc(' ' + row);
	ttputc(' ' + col);
out:
	tt.tt_col = col;
	tt.tt_row = row;
}

h19_start()
{
	if (gen_VS)
		ttxputs(gen_VS);
	ttesc('w');
	ttesc('E');
	tt.tt_col = tt.tt_row = 0;
	tt.tt_insert = 0;
	tt.tt_nmodes = tt.tt_modes = 0;
}

h19_end()
{
	if (tt.tt_insert)
		H19_SETINSERT(0);
	if (gen_VE)
		ttxputs(gen_VE);
	ttesc('v');
}

h19_clreol()
{
	ttesc('K');
}

h19_clreos()
{
	ttesc('J');
}

h19_clear()
{
	ttesc('E');
}

h19_inschar(c)
register char c;
{
	if (tt.tt_nmodes != tt.tt_modes)
		(*tt.tt_setmodes)(tt.tt_nmodes);
	if (!tt.tt_insert)
		H19_SETINSERT(1);
	ttputc(c);
	if (tt.tt_insert)
		ICPAD();
	if (++tt.tt_col == NCOL)
		tt.tt_col = NCOL - 1;
}

h19_delchar(n)
{
	while (--n >= 0)
		ttesc('N');
}

h19_scroll_down(n)
{
	h19_move(NROW - 1, 0);
	while (--n >= 0)
		ttctrl('j');
}

h19_scroll_up(n)
{
	h19_move(0, 0);
	while (--n >= 0)
		ttesc('I');
}

tt_h19()
{
	float cpms = (float) wwbaud / 10000;	/* char per ms */

	h19_msp10c = 10 / cpms;			/* ms per 10 char */
	gen_VS = ttxgetstr("vs");
	gen_VE = ttxgetstr("ve");

	tt.tt_start = h19_start;
	tt.tt_end = h19_end;

	tt.tt_insline = h19_insline;
	tt.tt_delline = h19_delline;
	tt.tt_inschar = h19_inschar;
	tt.tt_delchar = h19_delchar;
	tt.tt_clreol = h19_clreol;
	tt.tt_clreos = h19_clreos;
	tt.tt_clear = h19_clear;
	tt.tt_move = h19_move;
	tt.tt_write = h19_write;
	tt.tt_putc = h19_putc;
	tt.tt_scroll_down = h19_scroll_down;
	tt.tt_scroll_up = h19_scroll_up;
	tt.tt_setmodes = h19_setmodes;

	tt.tt_ncol = NCOL;
	tt.tt_nrow = NROW;
	tt.tt_availmodes = WWM_REV|WWM_GRP;
	tt.tt_frame = h19_frame;
	return 0;
}
