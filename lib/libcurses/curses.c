/*
 * Copyright (c) 1981 Regents of the University of California.
 * All rights reserved.
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
static char sccsid[] = "@(#)curses.c	5.7 (Berkeley) 6/1/90";
#endif /* not lint */

/*
 * Define global variables
 *
 */
# include	"curses.h"

bool	_echoit		= TRUE,	/* set if stty indicates ECHO		*/
	_rawmode	= FALSE,/* set if stty indicates RAW mode	*/
	My_term		= FALSE,/* set if user specifies terminal type	*/
	_endwin		= FALSE;/* set if endwin has been called	*/

char	ttytype[50],		/* long name of tty			*/
	*Def_term	= "unknown";	/* default terminal type	*/

int	_tty_ch		= 0,	/* file channel which is a tty		*/
	LINES,			/* number of lines allowed on screen	*/
	COLS,			/* number of columns allowed on screen	*/
	_res_flg;		/* sgtty flags for reseting later	*/

WINDOW	*stdscr		= NULL,
	*curscr		= NULL;

# ifdef DEBUG
FILE	*outf;			/* debug output file			*/
# endif

SGTTY	_tty;			/* tty modes				*/

bool	AM, BS, CA, DA, DB, EO, HC, HZ, IN, MI, MS, NC, NS, OS, UL, XB, XN,
	XT, XS, XX;
char	*AL, *BC, *BT, *CD, *CE, *CL, *CM, *CR, *CS, *DC, *DL, *DM,
	*DO, *ED, *EI, *K0, *K1, *K2, *K3, *K4, *K5, *K6, *K7, *K8,
	*K9, *HO, *IC, *IM, *IP, *KD, *KE, *KH, *KL, *KR, *KS, *KU,
	*LL, *MA, *ND, *NL, *RC, *SC, *SE, *SF, *SO, *SR, *TA, *TE,
	*TI, *UC, *UE, *UP, *US, *VB, *VS, *VE, *AL_PARM, *DL_PARM,
	*UP_PARM, *DOWN_PARM, *LEFT_PARM, *RIGHT_PARM;
char	PC;

/*
 * From the tty modes...
 */

bool	GT, NONL, UPPERCASE, normtty, _pfast;
