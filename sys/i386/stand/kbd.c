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
 *	from: @(#)kbd.c	7.4 (Berkeley) 5/4/91
 *	$Id: kbd.c,v 1.2 1993/10/16 18:49:31 rgrimes Exp $
 */

#define	L		0x01	/* locking function */
#define	SHF		0x02	/* keyboard shift */
#define	ALT		0x04	/* alternate shift -- alternate chars */
#define	NUM		0x08	/* numeric shift  cursors vs. numeric */
#define	CTL		0x10	/* control shift  -- allows ctl function */
#define	CPS		0x20	/* caps shift -- swaps case of letter */
#define	ASCII		0x40	/* ascii code for this key */
#define	STP		0x80	/* stop output */

typedef unsigned char u_char;

u_char inb();

#ifdef notdef
u_char action[] = {
0,     ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan  0- 7 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan  8-15 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan 16-23 */
ASCII, ASCII, ASCII, ASCII, ASCII,   CTL, ASCII, ASCII,		/* scan 24-31 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan 32-39 */
ASCII, ASCII, SHF  , ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan 40-47 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,  SHF,  ASCII,		/* scan 48-55 */
  ALT, ASCII, CPS|L,     0,     0, ASCII,     0,     0,		/* scan 56-63 */
    0,     0,     0,     0,     0, NUM|L, STP|L, ASCII,		/* scan 64-71 */
ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII, ASCII,		/* scan 72-79 */
ASCII, ASCII, ASCII, ASCII,     0,     0,     0,     0,		/* scan 80-87 */
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,	} ;

u_char unshift[] = {	/* no shift */
0,     033  , '1'  , '2'  , '3'  , '4'  , '5'  , '6'  ,		/* scan  0- 7 */
'7'  , '8'  , '9'  , '0'  , '-'  , '='  , 0177 ,'\t'  ,		/* scan  8-15 */

'q'  , 'w'  , 'e'  , 'r'  , 't'  , 'y'  , 'u'  , 'i'  ,		/* scan 16-23 */
'o'  , 'p'  , '['  , ']'  , '\r' , CTL  , 'a'  , 's'  ,		/* scan 24-31 */

'd'  , 'f'  , 'g'  , 'h'  , 'j'  , 'k'  , 'l'  , ';'  ,		/* scan 32-39 */
'\'' , '`'  , SHF  , '\\' , 'z'  , 'x'  , 'c'  , 'v'  ,		/* scan 40-47 */

'b'  , 'n'  , 'm'  , ','  , '.'  , '/'  , SHF  ,   '*',		/* scan 48-55 */
ALT  , ' '  , CPS|L,     0,     0, ' '  ,     0,     0,		/* scan 56-63 */

    0,     0,     0,     0,     0, NUM|L, STP|L,   '7',		/* scan 64-71 */
  '8',   '9',   '-',   '4',   '5',   '6',   '+',   '1',		/* scan 72-79 */

  '2',   '3',   '0',   '.',     0,     0,     0,     0,		/* scan 80-87 */
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,	} ;

u_char shift[] = {	/* shift shift */
0,     033  , '!'  , '@'  , '#'  , '$'  , '%'  , '^'  ,		/* scan  0- 7 */
'&'  , '*'  , '('  , ')'  , '_'  , '+'  , 0177 ,'\t'  ,		/* scan  8-15 */
'Q'  , 'W'  , 'E'  , 'R'  , 'T'  , 'Y'  , 'U'  , 'I'  ,		/* scan 16-23 */
'O'  , 'P'  , '['  , ']'  , '\r' , CTL  , 'A'  , 'S'  ,		/* scan 24-31 */
'D'  , 'F'  , 'G'  , 'H'  , 'J'  , 'K'  , 'L'  , ':'  ,		/* scan 32-39 */
'"'  , '~'  , SHF  , '|'  , 'Z'  , 'X'  , 'C'  , 'V'  ,		/* scan 40-47 */
'B'  , 'N'  , 'M'  , '<'  , '>'  , '?'  , SHF  ,   '*',		/* scan 48-55 */
ALT  , ' '  , CPS|L,     0,     0, ' '  ,     0,     0,		/* scan 56-63 */
    0,     0,     0,     0,     0, NUM|L, STP|L,   '7',		/* scan 64-71 */
  '8',   '9',   '-',   '4',   '5',   '6',   '+',   '1',		/* scan 72-79 */
  '2',   '3',   '0',   '.',     0,     0,     0,     0,		/* scan 80-87 */
0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,	} ;

u_char ctl[] = {	/* CTL shift */
0,     033  , '!'  , 000  , '#'  , '$'  , '%'  , 036  ,		/* scan  0- 7 */
'&'  , '*'  , '('  , ')'  , 037  , '+'  , 034  ,'\177',		/* scan  8-15 */
021  , 027  , 005  , 022  , 024  , 031  , 025  , 011  ,		/* scan 16-23 */
017  , 020  , 033  , 035  , '\r' , CTL  , 001  , 013  ,		/* scan 24-31 */
004  , 006  , 007  , 010  , 012  , 013  , 014  , ';'  ,		/* scan 32-39 */
'\'' , '`'  , SHF  , 034  , 032  , 030  , 003  , 026  ,		/* scan 40-47 */
002  , 016  , 015  , '<'  , '>'  , '?'  , SHF  ,   '*',		/* scan 48-55 */
ALT  , ' '  , CPS|L,     0,     0, ' '  ,     0,     0,		/* scan 56-63 */
CPS|L,     0,     0,     0,     0,     0,     0,     0,		/* scan 64-71 */
    0,     0,     0,     0,     0,     0,     0,     0,		/* scan 72-79 */
    0,     0,     0,     0,     0,     0,     0,     0,		/* scan 80-87 */
    0,     0,   033, '7'  , '4'  , '1'  ,     0, NUM|L,		/* scan 88-95 */
'8'  , '5'  , '2'  ,     0, STP|L, '9'  , '6'  , '3'  ,		/*scan  96-103*/
'.'  ,     0, '*'  , '-'  , '+'  ,     0,     0,     0,		/*scan 104-111*/
0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0,	} ;

#ifdef notdef
struct key {
	u_short action;		/* how this key functions */
	char	ascii[8];	/* ascii result character indexed by shifts */
};
#endif

u_char shfts, ctls, alts, caps, num, stp;
#endif

#define	KBSTATP	0x64	/* kbd status port */
#define		KBS_INP_BUF_FUL	0x02	/* kbd char ready */
#define	KBDATAP	0x60	/* kbd data port */
#define	KBSTATUSPORT	0x61	/* kbd status */

u_char odt, bdt;

u_char kbd() {
	u_char dt, brk, act;
	
loop:
	while(inb(0x64)&1 == 0);
	dt = inb(0x60);
	do {
		while(inb(0x64)&1 == 0);
	} while(dt == inb(0x60));
	odt = dt;

	brk = dt & 0x80 ; dt = dt & 0x7f ;

#ifdef notdef
	act = action[dt];
	if (act&SHF) {
		if(brk)	shfts = 0; else shfts = 1;
	}
	if (act&ALT) {
		if(brk)	alts = 0; else alts = 1;
	}
	if (act&NUM) {
		if (act&L) {
			if(!brk) num ^= 1;
		} else if(brk)	num = 0; else num = 1;
	}
	if (act&CTL) {
		if(brk)	ctls = 0; else ctls = 1;
	}
	if (act&CPS) {
		if (act&L) {
			if(!brk) caps ^= 1;
		} else if(brk)	caps = 0; else caps = 1;
	}
	if (act&STP) {
		if (act&L) {
			if(!brk) stp ^= 1;
		} else if(brk)	stp = 0; else stp = 1;
	}
	if(ctls && alts && dt == 83) exit();
	if ((act&ASCII) && !brk) {
		u_char chr;

		if (shfts){
			 chr = shift[dt] ; } else {
		if (ctls) {
			chr = ctl[dt] ; } else {
		chr = unshift[dt] ; } }
		if (caps && (chr >= 'a' && chr <= 'z')) {
			chr -= 'a' - 'A' ;
		}
		/*do
			while(inb(0x64)&1 == 0) ;
		while (inb(0x60) == (chr | 0x80));
		while(inb(0x64)&1 == 1) inb(0x60);A*/
		return(chr);
	}
#else
	if (brk)
		return(1);
#endif
	goto loop;
}

scankbd() {
u_char c;
	
#ifdef notdef
	c = inb(0x60);
	if (c == 83) exit();
	/*if (c == 0xaa) return (0);
	if (c == 0xfa) return (0);*/

	if (bdt == 0) {  bdt = c&0x7f; return(0); }

	if(odt) return(1);

	c &= 0x7f;
	
	if (bdt == c) return(0);
	odt = c;
#endif
	return(1);
}

kbdreset()
{
	u_char c;

	/* Enable interrupts and keyboard controller */
	while (inb(0x64)&2); outb(0x64,0x60);
	while (inb(0x64)&2); outb(0x60,0x4D);

	/* Start keyboard stuff RESET */
	while (inb(0x64)&2);	/* wait input ready */
	outb(0x60,0xFF);	/* RESET */

	while((c=inb(0x60))!=0xFA) ;

	/* While we are here, defeat gatea20 */
	while (inb(0x64)&2);	/* wait input ready */
	outb(0x64,0xd1);	
	while (inb(0x64)&2);	/* wait input ready */
	outb(0x60,0xdf);	
	while (inb(0x64)&2);	/* wait input ready */
	odt = bdt = 0;
	inb(0x60);
}

#ifdef notdef;
u_char getchar() {
	u_char c;

	c = kbd();
	if (c == '\b' || c == '\177') return(c);
	if (c == '\r') c = '\n';
	putchar(c);
	return(c);
}
#endif

reset_cpu() {

	while (inb(0x64)&2);	/* wait input ready */
	outb(0x64,0xFE);	/* Reset Command */
	wait(4000000);
	/* NOTREACHED */
}
