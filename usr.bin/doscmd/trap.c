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
 *	BSDI trap.c,v 2.3 1996/04/08 19:33:08 bostic Exp
 *
 * $Id: trap.c,v 1.10 1996/10/02 00:31:43 miff Exp $
 */

#include "doscmd.h"
#include "trap.h"

/* 
** When the emulator is very busy, it's often common for
** SIGALRM to be missed, leading to missed screen updates.
**
** We update this counter every time a DOS interrupt is processed and
** if it hits a certain threshold, force an update.
**
** When updates occur, the counter is zeroed.
*/
static int	update_counter = 0;
#define		BUSY_UPDATES	2000

/*
** handle interrupts passed to us by the kernel
*/
void
fake_int(regcontext_t *REGS, int intnum)
{
    if (R_CS == 0xF000 || (ivec[intnum] >> 16) == 0xF000) {
	if (R_CS != 0xF000)
	    intnum = ((u_char *)VECPTR(ivec[intnum]))[1];
	debug (D_ITRAPS|intnum, "int %02x:%02x %04x:%04x/%08x\n",
	       intnum, R_AH, R_CS, R_IP, ivec[intnum]);
	switch (intnum) {
	case 0x2f: 			/* multiplex interrupt */
	    int2f(&REGS->sc); 
	    break;
	case 0xff: 			/* doscmd special */
	    intff(REGS); 
	    break;
	default:			/* should not get here */
	    if (vflag) dump_regs(REGS);
	    fatal("no interrupt set up for 0x%02x\n", intnum);
	}
	debug (D_ITRAPS|intnum, "\n");
	return;
    }

user_int:
    debug (D_TRAPS|intnum, "INT %02x:%02x [%04x:%04x] %04x %04x %04x %04x from %04x:%04x\n",
	   intnum, R_AH, ivec[intnum] >> 16, ivec[intnum] & 0xffff,
	   R_AX, R_BX, R_CX, R_DX, R_CS, R_IP);

#if 0
    if ((intnum == 0x13) && (*(u_char *)VECPTR(ivec[intnum]) != 0xf4)) {
#if 1
        char *addr; /*= (char *)VECPTR(ivec[intnum]);*/
        int i, l, j;
        char buf[100];

        R_CS = 0x2c7; 
        R_IP = 0x14f9;
        addr = (char *)N_GETPTR(R_CS, R_IP);

        printf("\n");
        for (i = 0; i < 100; i++) {
            l = i386dis(R_CS, R_IP, addr, buf, 0);
            printf("%04x:%04x  %s\t;",R_CS,R_IP,buf);
            for (j = 0; j < l; j++)
                printf(" %02x", (u_char)addr[j]);
            printf("\n");
            R_IP += l;
            addr += l;
        }
        exit (0);
#else
        tmode = 1;
#endif
    }
#endif

    if (intnum == 0)
	dump_regs(REGS);

    if (ivec[intnum] == 0) {		/* uninitialised interrupt? */
	if (vflag) dump_regs(REGS);
	fatal("Call to uninitialised interrupt 0x%02x\n", intnum);
    }

    /*
     * This is really ugly, but when DOS boots, it seems to loop
     * for a while on INT 16:11 INT 21:3E INT 2A:82
     * INT 21:3E is a close(), which seems like something one would
     * not sit on for ever, so we will allow it to reset our POLL count.
     */
    if (intnum == 0x21 && R_AX == 0x3E)
	reset_poll();

    /* stack for and call the interrupt in vm86 space */
    N_PUSH((R_FLAGS & ~PSL_I) | (R_EFLAGS & PSL_VIF ? PSL_I : 0), REGS);
    N_PUSH(R_CS, REGS);
    N_PUSH(R_IP, REGS);
    R_EFLAGS &= ~PSL_VIF;		/* disable interrupts */
    N_PUTVEC(R_CS, R_IP, ivec[intnum]);
}

/* make this read a little more intuitively */
#define ipadvance(c,n)	SET16(c->sc_eip, GET16(c->sc_eip) + n)	/* move %ip along */

#ifdef USE_VM86
/* entry from NetBSD-style vm86 */
void
sigurg(struct sigframe *sf)
{
#define	sc	(&sf->sf_sc)
    int intnum;
    u_char *addr;
    int rep;
    int port;
    callback_t func;

#if 0
    printf("ivec08 = %08x\n", ivec[0x08]);
#endif

    if (tmode)
	resettrace(sc);

    switch (VM86_TYPE(sf->sf_code)) {
    case VM86_INTx:
	intnum = VM86_ARG(sf->sf_code);
	switch (intnum) {
	case 0x2f:
	    switch (GET8H(sc->sc_eax)) {
	    case 0x11:
		debug (D_TRAPS|0x2f, "INT 2F:%04x\n", GET16(sc->sc_eax));
		if (int2f_11(sc)) {
		    /* Skip over int 2f:11 */
		    goto out;
		}
		break;
	    case 0x43:
		debug (D_TRAPS|0x2f, "INT 2F:%04x\n", GET16(sc->sc_eax));
		if (int2f_43(sc)) {
		    /* Skip over int 2f:43 */
		    goto out;
		}
		break;
	    }
	    break;
	}
	fake_int(sc, intnum);
	break;
    case VM86_UNKNOWN:
	/*XXXXX failed vector also gets here without IP adjust*/

	addr = (u_char *)GETPTR(sc->sc_cs, sc->sc_eip);
	rep = 1;

	debug (D_TRAPS2, "%04x:%04x [%02x]", GET16(sc->sc_cs), GET16(sc->sc_eip), addr[0]);
	switch (addr[0]) {
	case TRACETRAP:
	    ipadvance(sc,1);
	    fake_int(sc, 3);
	    break;
	case INd:
	    port = addr[1];
	    ipadvance(sc,2);
	    inb(sc, port);
	    break;
	case OUTd:
	    port = addr[1];
	    ipadvance(sc,2);
	    outb(sc, port);
	    break;
	case INdX:
	    port = addr[1];
	    ipadvance(sc,2);
	    inx(sc, port);
	    break;
	case OUTdX:
	    port = addr[1];
	    ipadvance(sc,2);
	    outx(sc, port);
	    break;
	case IN:
	    ipadvance(sc,1);
	    inb(sc, GET16(sc->sc_edx));
	    break;
	case INX:
	    ipadvance(sc,1);
	    inx(sc, GET16(sc->sc_edx));
	    break;
	case OUT:
	    ipadvance(sc,1);
	    outb(sc, GET16(sc->sc_edx));
	    break;
	case OUTX:
	    ipadvance(sc,1);
	    outx(sc, GET16(sc->sc_edx));
	    break;
	case OUTSB:
	    ipadvance(sc,1);
	    while (rep-- > 0)
		outsb(sc, GET16(sc->sc_edx));
	    break;
	case OUTSW:
	    ipadvance(sc,1);
	    while (rep-- > 0)
		outsx(sc, GET16(sc->sc_edx));
	    break;
	case INSB:
	    ipadvance(sc,1);
	    while (rep-- > 0)
		insb(sc, GET16(sc->sc_edx));
	    break;
	case INSW:
	    ipadvance(sc,1);
	    while (rep-- > 0)
		insx(sc, GET16(sc->sc_edx));
	    break;
	case LOCK:
	    debug(D_TRAPS2, "lock\n");
	    ipadvance(sc,1);
	    break;
	case HLT:	/* BIOS entry points populated with HLT */
	    func = find_callback(GETVEC(sc->sc_cs, sc->sc_eip));
	    if (func) {
		ipadvance(sc,);
		SET16(sc->sc_eip, GET16(sc->sc_eip) + 1);
		func(sc);
		break;
	    }
	default:
	    dump_regs(sc);
	    fatal("unsupported instruction\n");
	}
		break;
	default:
		dump_regs(sc);
		printf("code = %04x\n", sf->sf_code);
		fatal("unrecognized vm86 trap\n");
	}

out:
    	if (tmode)
		tracetrap(sc);
#undef	sc
#undef  ipadvance
}

#else /* USE_VM86 */

/* entry from FreeBSD, BSD/OS vm86 */
void
sigbus(struct sigframe *sf)
{
    u_char		*addr;
    int			tempflags,okflags;
    int			intnum;
    int			port;
    callback_t		func;
    regcontext_t	*REGS = (regcontext_t *)(&sf->sf_sc);

    if (!(R_EFLAGS && PSL_VM))
	fatal("SIGBUS in the emulator\n");

    if (sf->sf_code != 0) {
        fatal("SIGBUS code %d, trapno: %d, err: %d\n",
            sf->sf_code, sf->sf_sc.sc_trapno, sf->sf_sc.sc_err); 
    }

    addr = (u_char *)GETPTR(R_CS, R_IP);

    if (tmode)
	resettrace(REGS);

    if ((R_EFLAGS & (PSL_VIP | PSL_VIF)) == (PSL_VIP | PSL_VIF)) {
        if (n_pending < 1) {
            fatal("Pending interrupts out of sync\n");
            exit(1);
        }
        resume_interrupt();
        goto out;
    }
/*    printf("%p\n", addr); fflush(stdout); */
    debug (D_TRAPS2, "%04x:%04x [%02x %02x %02x] ", R_CS, R_IP, (int)addr[0], (int)addr[1], (int)addr[2]);
#if 0
    if ((int)addr[0] == 0x67) {
        int i;
        printf("HERE\n"); fflush(stdout);
        printf("addr: %p\n", REGS); fflush(stdout);
        for (i = 0; i < 21 * 4; i++) {
            printf("%d: %x\n", i, ((u_char *)REGS)[i]);
            fflush(stdout);
        }
        printf("Trapno, error: %p %p\n", REGS->sc.sc_trapno, REGS->sc.sc_err);
        fflush(stdout);
        dump_regs(REGS);
    }
#endif
    
        switch (addr[0]) {				/* what was that again dear? */

        case CLI:
	    debug (D_TRAPS2, "cli\n");
	    R_IP++;
            R_EFLAGS &= ~PSL_VIP;
	    break;

	case STI:
	    debug (D_TRAPS2, "sti\n");
	    R_IP++;
            R_EFLAGS |= PSL_VIP;
#if 0
	    if (update_counter++ > BUSY_UPDATES)
		sigalrm(sf);
#endif
	    break;
	    
	case PUSHF:
	    debug (D_TRAPS2, "pushf <- 0x%x\n", R_EFLAGS);
	    R_IP++;
            N_PUSH((R_FLAGS & ~PSL_I) | (R_EFLAGS & PSL_VIF ? PSL_I : 0), REGS);
	    break;

	case IRET:
	    R_IP = N_POP(REGS);				/* get new cs:ip off the stack */
	    R_CS = N_POP(REGS);
	    debug (D_TRAPS2, "iret to %04x:%04x ", R_CS, R_IP);
	    /* FALLTHROUGH */					/* 'safe' flag pop operation */

	case POPF:
/* XXX */
	    fatal("popf/iret in emulator");

	    if (addr[0] == POPF)
		R_IP++;
	    
	    tempflags = N_POP(REGS);				/* get flags from stack */
	    okflags =  (PSL_ALLCC | PSL_T | PSL_D | PSL_V);	/* flags we consider OK */
	    R_FLAGS = ((R_FLAGS & ~okflags) |			/* keep state of non-OK flags */
		       (tempflags & okflags));			/* pop state of OK flags */

	    IntState = tempflags & PSL_I;			/* restore pseudo PSL_I flag */
	    debug(D_TRAPS2, "popf -> 0x%x\n", R_EFLAGS);
	    break;

	case TRACETRAP:
	    debug(D_TRAPS2, "ttrap\n");
	    R_IP++;
	    fake_int(REGS, 3);
	    break;

	case INTn:
	    intnum = addr[1];
	    R_IP += 2;			/* nobody else will do it for us */
	    switch (intnum) {
	    case 0x2f:
		switch (R_AH) {		/* function number */
		case 0x11:
		    debug (D_TRAPS|0x2f, "INT 2F:%04x\n", R_AX);
		    if (int2f_11(REGS)) {
			/* Skip over int 2f:11 */
			goto out;
		    }
		    break;
		case 0x43:
		    debug (D_TRAPS|0x2f, "INT 2F:%04x\n", R_AX);
		    if (int2f_43(REGS)) {
			/* Skip over int 2f:43 */
			goto out;
		    }
		    break;
		}
		break;
	    }
	    fake_int(REGS, intnum);
	    break;

	case INd:		/* XXX implement in/out */
	    R_IP += 2;
	    port = addr[1];
	    inb(REGS, port);
	    break;
	case IN:
	    R_IP++;
	    inb(REGS,R_DX);
	    break;
	case INX:
	    R_IP++;
	    inx(REGS,R_DX);
	    break;
	case INdX:
	    R_IP += 2;
	    port = addr[1];
	    inx(REGS, port);
	    break;
	case INSB:
	    R_IP++;
	    printf("(missed) INSB <- 0x%02x\n",R_DX);
	    break;
	case INSW:
	    R_IP++;
	    printf("(missed) INSW <- 0x%02x\n",R_DX);
	    break;

	case OUTd:
	    R_IP += 2;
	    port = addr[1];
	    outb(REGS, port);
	    break;
	case OUTdX:
	    R_IP += 2;
	    port = addr[1];
	    outx(REGS, port);
	    break;
	case OUT:
	    R_IP++;
	    outb(REGS, R_DX);
	    break;
	case OUTX:
	    R_IP++;
	    outx(REGS, R_DX);
	    break;	    
	case OUTSB:
	    R_IP++;
	    printf("(missed) OUTSB -> 0x%02x\n",R_DX);
	    break;	    
	case OUTSW:
	    R_IP++;
	    printf("(missed) OUTSW -> 0x%02x\n",R_DX);
/* tmode = 1; */
	    break;

	case LOCK:
	    debug(D_TRAPS2, "lock\n");
	    R_IP++;
	    break;

	case HLT:	/* BIOS entry points populated with HLT */
	    func = find_callback(N_GETVEC(R_CS, R_IP));
	    if (func) {
		R_IP++;					/* pass HLT opcode */
		func(REGS);
/*		dump_regs(REGS); */
#if 0
		update_counter += 5;
		if (update_counter > BUSY_UPDATES)
		    sigalrm(sf);
#endif
		break;
	    }
/*            if (R_EFLAGS & PSL_VIF) { */
                R_IP++;
                tty_pause();
                goto out;
/*            } */
            /* FALLTHRU */

	default:
	    dump_regs(REGS);
	    fatal("unsupported instruction\n");
	}

out:
    	if (tmode)
	    tracetrap(REGS);
}
#endif /* USE_VM86 */

void
sigtrace(struct sigframe *sf)
{
    int			x;
    regcontext_t	*REGS = (regcontext_t *)(&sf->sf_sc);

    if (R_EFLAGS & PSL_VM) {
	debug(D_ALWAYS, "Currently in DOS\n");
	dump_regs(REGS);
	for (x = 0; x < 16; ++x)
	    debug(D_ALWAYS, " %02x", *(unsigned char *)x);
	putc('\n', debugf);
    } else {
	debug(D_ALWAYS, "Currently in the emulator\n");
	sigalrm(sf);
    }
}

void
sigtrap(struct sigframe *sf)
{   
    int			intnum;
    int			trapno;
    regcontext_t	*REGS = (regcontext_t *)(&sf->sf_sc);

    if ((R_EFLAGS & PSL_VM) == 0) {
	dump_regs(REGS);
	fatal("%04x:%08x Sigtrap in protected mode\n", R_CS, R_IP);
    }

    if (tmode)
	if (resettrace(REGS))
	    goto doh;

#ifdef __FreeBSD__
    trapno = sf->sf_code;			/* XXX GROSTIC HACK ALERT */
#else
    trapno = sc->sc_trapno;
#endif
    if (trapno == T_BPTFLT)
	intnum = 3;  
    else
	intnum = 1;

    N_PUSH((R_FLAGS & ~PSL_I) | (R_EFLAGS & PSL_VIF ? PSL_I : 0), REGS);
    N_PUSH(R_CS, REGS);
    N_PUSH(R_IP, REGS);
    R_FLAGS &= ~PSL_T;
    N_PUTVEC(R_CS, R_IP, ivec[intnum]);

doh:
    if (tmode)
	tracetrap(REGS);
}

void
breakpoint(struct sigframe *sf)
{
    regcontext_t	*REGS = (regcontext_t *)(&sf->sf_sc);

    if (R_EFLAGS & PSL_VM)
	printf("doscmd ");
    printf("breakpoint: %04x\n", *(u_short *)0x8e64);

    __asm__ volatile("mov 0, %eax");
    __asm__ volatile(".byte 0x0f");		/* MOV DR6,EAX */
    __asm__ volatile(".byte 0x21");
    __asm__ volatile(".byte 0x1b");
}

/*
** periodic updates
*/
void
sigalrm(struct sigframe *sf)
{
    regcontext_t	*REGS = (regcontext_t *)(&sf->sf_sc);

    if (tmode)
	resettrace(REGS);

/*     debug(D_ALWAYS,"tick %d", update_counter); */
    update_counter = 0;			/* remember we've updated */
    video_update(&REGS->sc);
    hardint(0x08);
/*     debug(D_ALWAYS,"\n"); */

    if (tmode)
	tracetrap(REGS);
}

void
sigill(struct sigframe *sf)
{
    regcontext_t	*REGS = (regcontext_t *)(&sf->sf_sc);

    fprintf(stderr, "Signal %d from DOS program\n", sf->sf_signum);
    dump_regs(REGS);
    fatal("%04x:%04x Illegal instruction\n", R_CS, R_IP);

}

void
sigfpe(struct sigframe *sf)
{
    regcontext_t	*REGS = (regcontext_t *)(&sf->sf_sc);

    if (R_EFLAGS & PSL_VM) {
	fake_int(REGS, 0);	/* call handler XXX rather bogus, eh? */
	return;
    }
    dump_regs(REGS);
    fatal("%04x:%04x Floating point fault in emulator.\n", R_CS, R_IP);
}
