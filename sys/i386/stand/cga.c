/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	from: @(#)cga.c	5.3 (Berkeley) 4/28/91
 *	$Id: cga.c,v 1.2 1993/10/16 18:49:25 rgrimes Exp $
 */

#include "param.h"

#define	COL		80
#define	ROW		25
#define	CHR		2
#define MONO_BASE	0x3B4
#define MONO_BUF	0xB0000
#define CGA_BASE	0x3D4
#define CGA_BUF		0xB8000

static u_char	att = 0x7 ;
u_char *Crtat = (u_char *)CGA_BUF;

static unsigned int addr_6845 = CGA_BASE;
cursor(pos)
int pos;
{
	outb(addr_6845,14);
	outb(addr_6845+1,pos >> 8);
	outb(addr_6845,15);
	outb(addr_6845+1,pos&0xff);
}

sput(c)
u_char c;
{

	static u_char *crtat = 0;
	unsigned cursorat; u_short was;
	u_char *cp;

	if (crtat == 0) {

		/* XXX probe to find if a color or monochrome display */
		was = *(u_short *)Crtat;
		*(u_short *)Crtat = 0xA55A;
		if (*(u_short *)Crtat != 0xA55A) {
			Crtat = (u_char *) MONO_BUF;
			addr_6845 = MONO_BASE;
		}
		*(u_short *)Crtat = was;

		/* Extract cursor location */
		outb(addr_6845,14);
		cursorat = inb(addr_6845+1)<<8 ;
		outb(addr_6845,15);
		cursorat |= inb(addr_6845+1);

		if(cursorat <= COL*ROW) {
			crtat = Crtat + cursorat*CHR;
			/* att = crtat[1];	/* use current attribute present */
		} else	crtat = Crtat;

		/* clean display */
		for (cp = crtat; cp < Crtat+ROW*COL*CHR; cp += 2) {
			cp[0] = ' ';
			cp[1] = att;
		}
	}

	switch (c) {

	case '\t':
		do
			sput(' ');
		while ((int)crtat % (8*CHR));
		break;

	case '\010':
		crtat -= CHR;
		break;

	case '\r':
		crtat -= (crtat - Crtat) % (COL*CHR);
		break;

	case '\n':
		crtat += COL*CHR ;
		break;

	default:
		crtat[0] = c;
		crtat[1] = att;
		crtat += CHR;
		break ;
	}

#ifndef SMALL
	/* implement a scroll */
	if (crtat >= Crtat+COL*ROW*CHR) {
		/* move text up */
		bcopy(Crtat+COL*CHR, Crtat, COL*(ROW-1)*CHR);

		/* clear line */
		for (cp = Crtat+ COL*(ROW-1)*CHR;
			cp < Crtat + COL*ROW*CHR ; cp += 2)
			cp[0] = ' ';

		crtat -= COL*CHR ;
	}
#endif

	cursor((crtat-Crtat)/CHR);
}
