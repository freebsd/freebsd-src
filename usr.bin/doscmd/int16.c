/*
 * Copyright (c) 1992, 1993, 1996
 *	Berkeley Software Design, Inc.  All rights reserved.
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
 *	This product includes software developed by Berkeley Software
 *	Design, Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI int16.c,v 2.2 1996/04/08 19:32:47 bostic Exp
 *
 * $FreeBSD: src/usr.bin/doscmd/int16.c,v 1.2 1999/08/28 01:00:17 peter Exp $
 */

#include "doscmd.h"

#define	K_NEXT		*(u_short *)0x41a
#define	K_FREE		*(u_short *)0x41c
#define	KbdEmpty()	(K_NEXT == K_FREE)

#define	HWM	16
volatile int	poll_cnt = 0;

void
wakeup_poll(void)
{
    if (poll_cnt <= 0)
	poll_cnt = HWM;
}

void
reset_poll(void)
{
    poll_cnt = HWM;
}

void
sleep_poll(void)
{
#if 0
    printf("sleep_poll: poll_cnt=%d\n", poll_cnt);
    if (poll_cnt == 14)
	tmode = 1;
#endif
    if (--poll_cnt <= 0) {
	poll_cnt = 0;
	while (KbdEmpty() && poll_cnt <= 0) {
#if 0
	    softint(0x28);
#endif
	    if (KbdEmpty() && poll_cnt <= 0)
		tty_pause();
	}
    }
}

void
int16(regcontext_t *REGS)
{               
    int c;

    if (!xmode && !raw_kbd) {
	if (vflag) dump_regs(REGS);
	fatal ("int16 func 0x%x only supported in X mode\n", R_AH);
    }
    switch(R_AH) {
    case 0x00:
    case 0x10: /* Get enhanced keystroke */
	poll_cnt = 16;
	while (KbdEmpty())
	    tty_pause();
	R_AX = KbdRead();
	break;

    case 0x01: /* Get keystroke */
    case 0x11: /* Get enhanced keystroke */
	if (!raw_kbd)
	    sleep_poll();
	
	if (KbdEmpty()) {
	    R_FLAGS |= PSL_Z;
	    break;
	}
	R_FLAGS &= ~PSL_Z;
	R_AX = KbdPeek();
	break;

    case 0x02:
	R_AL = tty_state();
	break;

    case 0x12:
	R_AH = tty_estate();
	R_AL = tty_state();
	break;

    case 0x03:		/* Set typematic and delay rate */
	break;

    case 0x05:
	KbdWrite(R_CX);
	break;

    case 0x55:
	R_AX = 0x43af;	/* Empirical value ... */
	break;

    case 0x92:
	R_AH = 0x00;
	break;

    case 0xa2:
	debug(D_HALF, "122-key keyboard support check\n");
	break;

    default:
	unknown_int2(0x16, R_AH, REGS);
	break;
    }
}
