/*
 * Copyright (c) 1981, 1993
 *	The Regents of the University of California.  All rights reserved.
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
static char sccsid[] = "@(#)curses.c	8.2 (Berkeley) 1/2/94";
#endif /* not lint */

#include <curses.h>

/* Private. */
int	__echoit = 1;			/* If stty indicates ECHO. */
int	__pfast;
int	__rawmode = 0;			/* If stty indicates RAW mode. */
int	__noqch = 0;			/* 
					 * If terminal doesn't have 
					 * insert/delete line capabilities 
					 * for quick change on refresh.
					 */
int     __usecs = 0;                    /*
					 * Use change scroll region, if
					 * no insert/delete capabilities
					 */
char	AM, BS, CA, DA, EO, HC, IN, MI, MS, NC, NS, OS, PC,
	UL, XB, XN, XT, XS, XX;
char	*AL, *BC, *BT, *CD, *CE, *CL, *CM, *CR, *CS, *DC, *DL,
	*DM, *DO, *ED, *EI, *K0, *K1, *K2, *K3, *K4, *K5, *K6,
	*K7, *K8, *K9, *HO, *IC, *IM, *IP, *KD, *KE, *KH, *KL,
	*KR, *KS, *KU, *LL, *MA, *ND, *NL, *RC, *SC, *SE, *SF,
	*SO, *SR, *TA, *TE, *TI, *UC, *UE, *UP, *US, *VB, *VS,
	*VE, *al, *dl, *sf, *sr,
	*AL_PARM, *DL_PARM, *UP_PARM, *DOWN_PARM, *LEFT_PARM,
	*RIGHT_PARM;
/*
 * Public.
 *
 * XXX
 * UPPERCASE isn't used by libcurses, and is left for backward
 * compatibility only.
 */
WINDOW	*curscr;			/* Current screen. */
WINDOW	*stdscr;			/* Standard screen. */
int	 COLS;				/* Columns on the screen. */
int	 LINES;				/* Lines on the screen. */
int	 My_term = 0;			/* Use Def_term regardless. */
char	*Def_term = "unknown";		/* Default terminal type. */
char	 GT;				/* Gtty indicates tabs. */
char	 NONL;				/* Term can't hack LF doing a CR. */
char	 UPPERCASE;			/* Terminal is uppercase only. */
