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
 *	from: @(#)pcb.h	5.10 (Berkeley) 5/12/91
 *	$Id: pcb.h,v 1.12 1996/04/18 21:34:53 phk Exp $
 */

#ifndef _I386_PCB_H_
#define _I386_PCB_H_

/*
 * Intel 386 process control block
 */
#include <machine/tss.h>
#include <machine/npx.h>

struct pcb {
	struct	i386tss pcb_tss;
#define	pcb_ksp	pcb_tss.tss_esp0
#define	pcb_ptd	pcb_tss.tss_cr3
#define	pcb_cr3	pcb_ptd
#define	pcb_pc	pcb_tss.tss_eip
#define	pcb_psl	pcb_tss.tss_eflags
#define	pcb_usp	pcb_tss.tss_esp
#define	pcb_fp	pcb_tss.tss_ebp
#ifdef	notyet
	u_char	pcb_iomap[NPORT/sizeof(u_char)]; /* i/o port bitmap */
#endif
	caddr_t	pcb_ldt;		/* per process (user) LDT */
	int	pcb_ldt_len;		/* number of LDT entries */
	struct	save87	pcb_savefpu;	/* floating point state for 287/387 */
/*
 * Software pcb (extension)
 */
	u_char	pcb_flags;
#define	FP_SOFTFP	0x01	/* process using software fltng pnt emulator */
	u_char	pcb_inl;	/* intr_nesting_level at context switch */
	caddr_t	pcb_onfault;	/* copyin/out fault recovery */
};

/*
 * The pcb is augmented with machine-dependent additional data for
 * core dumps. For the i386: ???
 */
struct md_coredump {
};

#ifdef KERNEL
extern struct pcb *curpcb;		/* our current running pcb */
void	savectx __P((struct pcb*, int *));
#endif

#endif /* _I386_PCB_H_ */
