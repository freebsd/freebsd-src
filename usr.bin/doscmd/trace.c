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
 *	BSDI trace.c,v 2.2 1996/04/08 19:33:07 bostic Exp
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include "doscmd.h"
#include "trap.h"

int		tmode = 0;

static u_short	*saddr;
static u_char	*iaddr, ibyte;

/* locals */
static void		printtrace(regcontext_t *REGS, char *buf);
static inline void	showstate(long, long, char);

/*
 * Before exiting to VM86 mode:
 * 1) Always set the trap flag.
 * 2) If this is a POPF or IRET instruction, set the trap flag in the saved
 *    flag state on the stack.
 * On enterint from VM86 mode:
 * 1) Restore the trap flag from our saved flag state.
 * 2) If we just finished a POPF or IRET unstruction, patch the saved flag
 *    state on the stack.
 */

int tracetype;

int
resettrace(regcontext_t *REGS)
{
    if ((R_EFLAGS & PSL_VM) == 0)		/* invalid unless handling a vm86 process */
	return (0);

/* XXX */ return 1;

    switch (tracetype) {
    case 1:
	R_EFLAGS &= ~PSL_T;
	tracetype = 0;
	return (1);

    case 2:
	if ((u_char *)MAKEPTR(R_CS, R_IP - 1) == iaddr)
	    R_IP --;
	*iaddr = ibyte;
	tracetype = 0;
	return (1);

    case 3:
    case 4:
	R_EFLAGS &= ~PSL_T;
	*saddr &= ~PSL_T;
	tracetype = 0;
	return (1);
    }
    return (0);
}

void
tracetrap(regcontext_t *REGS)
{
    u_char *addr;
    int n;
    char buf[100];
    
    if ((R_EFLAGS & PSL_VM) == 0)
	return;
    
    addr = (u_char *)MAKEPTR(R_CS, R_IP);

    n = i386dis(R_CS, R_IP, addr, buf, 0);
    printtrace(REGS, buf);

/* XXX */
    R_EFLAGS |= PSL_T;
    return;
/* XXX */
    

    switch (addr[0]) {
    case REPNZ:
    case REPZ:
	tracetype = 2;
	iaddr = (u_char *)MAKEPTR(R_CS, R_IP + n);
	break;
    case PUSHF:
	tracetype = 4;
	saddr = (u_short *)MAKEPTR(R_SS, R_SP - 2);
	break;
    case POPF:
	tracetype = 3;
	saddr = (u_short *)MAKEPTR(R_SS, R_SP + 0);
	break;
    case IRET:
	tracetype = 3;
	saddr = (u_short *)MAKEPTR(R_SS, R_SP + 4);
#if 0
	printf("IRET: %04x %04x %04x\n",
	       ((u_short *)MAKEPTR(R_SS, R_SP))[0],
	       ((u_short *)MAKEPTR(R_SS, R_SP))[1],
	       ((u_short *)MAKEPTR(R_SS, R_SP))[2]);
#endif	
	break;
    case OPSIZ:
	switch (addr[1]) {
	case PUSHF:
	    tracetype = 4;
	    saddr = (u_short *)MAKEPTR(R_SS, R_SP - 4);
	    break;
	case POPF:
	    tracetype = 3;
	    saddr = (u_short *)MAKEPTR(R_SS, R_SP + 0);
	    break;
	case IRET:
	    tracetype = 3;
	    saddr = (u_short *)MAKEPTR(R_SS, R_SP + 8);
	    break;
	default:
	    tracetype = 1;
	    break;
	}
    default:
	tracetype = 1;
	break;
    }

    switch (tracetype) {
    case 1:
    case 4:
	if (R_EFLAGS & PSL_T)
	    tracetype = 0;
	else
	    R_EFLAGS |= PSL_T;
	break;
    case 2:
	if (*iaddr == TRACETRAP)
	    tracetype = 0;
	else {
	    ibyte = *iaddr;
	    *iaddr = TRACETRAP;
	}
	break;
    case 3:
	R_EFLAGS |= PSL_T;
	if (*saddr & PSL_T)
	    tracetype = 0;
	else
	    *saddr |= PSL_T;
	break;
    }
}

static inline void
showstate(long flags, long flag, char f)
{
    putc((flags & flag) ? f : ' ', debugf);
}

static void
printtrace(regcontext_t *REGS, char *buf)
{

    static int first = 1;
#if BIG_DEBUG
    u_char *addr = (u_char *)MAKEPTR(R_CS, R_IP);
#endif

    if (first) {
	fprintf(debugf, "%4s:%4s "
#if BIG_DEBUG
	       ".. .. .. .. .. .. "
#endif
	       "%-30s "
	       "%4s %4s %4s %4s %4s %4s %4s %4s %4s %4s %4s\n",
		"CS", "IP", "instruction",
		"AX", "BX", "CX", "DX",
		"DI", "SI", "SP", "BP",
		"SS", "DS", "ES");
	first = 0;
    }

    fprintf(debugf, "%04x:%04x "
#if BIG_DEBUG
	    "%02x %02x %02x %02x %02x %02x "
#endif
	    "%-30s "
	    "%04x %04x %04x %04x %04x %04x %04x %04x %04x %04x %04x ",
	    R_CS, R_IP,
#if BIG_DEBUG
	    addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
#endif
	    buf,
	    R_AX, R_BX, R_CX, R_DX, R_DI, R_SI, R_SP, R_BP, R_SS, R_DS, R_ES);
#if 0
    fprintf(debugf, "%04x %04x %04x %04x ",
	    ((u_short *)VECPTR(0x0D760FCA-14))[0],
	    ((u_short *)VECPTR(0x0D760FCA-14))[1],
	    ((u_short *)VECPTR(0x0D760F7A+8))[0],
	    ((u_short *)VECPTR(0x0D760F7A+8))[1]);
#endif
    showstate(R_EFLAGS, PSL_C, 'C');
    showstate(R_EFLAGS, PSL_PF, 'P');
    showstate(R_EFLAGS, PSL_AF, 'c');
    showstate(R_EFLAGS, PSL_Z, 'Z');
    showstate(R_EFLAGS, PSL_N, 'N');
    showstate(R_EFLAGS, PSL_T, 'T');
    showstate(R_EFLAGS, PSL_I, 'I');
    showstate(R_EFLAGS, PSL_D, 'D');
    showstate(R_EFLAGS, PSL_V, 'V');
    showstate(R_EFLAGS, PSL_NT, 'n');
    showstate(R_EFLAGS, PSL_RF, 'r');
    showstate(R_EFLAGS, PSL_VM, 'v');
    showstate(R_EFLAGS, PSL_AC, 'a');
    showstate(R_EFLAGS, PSL_VIF, 'i');
    showstate(R_EFLAGS, PSL_VIP, 'p');
    putc('\n', debugf);
}
