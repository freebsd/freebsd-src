/*
 * Copyright (c) 1996
 *      Michael Smith, All rights reserved.
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
 * from: BSDI doscmd.c,v 2.3 1996/04/08 19:32:30 bostic Exp
 *
 * $FreeBSD$
 */


#include <stdarg.h>

#include "doscmd.h"

/* debug output goes here */
FILE *debugf = stderr;

/* see doscmd.h for flag names */
int debug_flags = D_ALWAYS;

/* include register dumps when reporting unknown interrupts */
int vflag = 0;

/* interrupts to trace */
#define	BPW	(sizeof(u_long) << 3)
u_long debug_ints[256/BPW];

/* Debug flag manipulation */
void
debug_set(int x)
{
    x &= 0xff;
    debug_ints[x/BPW] |= 1 << (x & (BPW - 1));
}

void
debug_unset(int x)
{
    x &= 0xff;
    debug_ints[x/BPW] &= ~(1 << (x & (BPW - 1)));
}

u_long
debug_isset(int x)
{
    x &= 0xff;
    return(debug_ints[x/BPW] & (1 << (x & (BPW - 1))));
}


/*
** Emit a debugging message if (flags) matches the current
** debugging mode.
*/
void
debug (int flags, char *fmt, ...)
{
    va_list args;

    if (flags & (debug_flags & ~0xff)) {
	if ((debug_flags & 0xff) == 0
	    && (flags & (D_ITRAPS|D_TRAPS))
	    && !debug_isset(flags & 0xff))
	    return;
	va_start (args, fmt);
	vfprintf (debugf, fmt, args);
	va_end (args);
    }
}

/*
** Emit a terminal error message and exit
*/
void
fatal (char *fmt, ...)
{
    va_list args;

    dead = 1;

    if (xmode) {
	char buf[1024];
	char *m;

	va_start (args, fmt);
	vfprintf (debugf, fmt, args);
	vsprintf (buf, fmt, args);
	va_end (args);
	
	tty_move(23, 0);
	for (m = buf; *m; ++m)
	    tty_write(*m, 0x0400);

	tty_move(24, 0);
	for (m = "(PRESS <CTRL-ALT> ANY MOUSE BUTTON TO exit)"; *m; ++m)
	    tty_write(*m, 0x0900);
	tty_move(-1, -1);
	for (;;)
	    tty_pause();
    }

    va_start (args, fmt);
    fprintf (debugf, "doscmd: fatal error ");
    vfprintf (debugf, fmt, args);
    va_end (args);
    quit (1);
}

/*
** Emit a register dump (usually when dying)
*/
void
dump_regs(regcontext_t *REGS)
{
    u_char	*addr;
    int		i;
    char	buf[100];

    debug (D_ALWAYS, "\n");
    debug (D_ALWAYS, "ax=%04x bx=%04x cx=%04x dx=%04x\n", R_AX, R_BX, R_CX, R_DX);
    debug (D_ALWAYS, "si=%04x di=%04x sp=%04x bp=%04x\n", R_SI, R_DI, R_SP, R_BP);
    debug (D_ALWAYS, "cs=%04x ss=%04x ds=%04x es=%04x\n", R_CS, R_SS, R_DS, R_ES);
    debug (D_ALWAYS, "ip=%x eflags=%lx\n", R_IP, R_EFLAGS);

    addr = (u_char *)MAKEPTR(R_CS, R_IP);

    for (i = 0; i < 16; i++)
	debug (D_ALWAYS, "%02x ", addr[i]);
    debug (D_ALWAYS, "\n");

    addr = (char *)MAKEPTR(R_CS, R_IP);
    i386dis(R_CS, R_IP, addr, buf, 0);

    debug (D_ALWAYS, "%s\n", buf);
}

/*
** Unknown interrupt error messages
*/
void
unknown_int2(int maj, int min, regcontext_t *REGS)
{
    if (vflag) dump_regs(REGS);
    printf("Unknown interrupt %02x function %02x\n", maj, min);
    R_FLAGS |= PSL_C;
}

void
unknown_int3(int maj, int min, int sub, regcontext_t *REGS)
{
    if (vflag) dump_regs(REGS);
    printf("Unknown interrupt %02x function %02x subfunction %02x\n",
	   maj, min, sub);
    R_FLAGS |= PSL_C;
}

void
unknown_int4(int maj, int min, int sub, int ss, regcontext_t *REGS)
{
    if (vflag) dump_regs(REGS);
    printf("Unknown interrupt %02x function %02x subfunction %02x %02x\n",
	   maj, min, sub, ss);
    R_FLAGS |= PSL_C;
}

