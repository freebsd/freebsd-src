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
 * $FreeBSD$
 */

#ifndef _I386_PCB_H_
#define _I386_PCB_H_

/*
 * Intel 386 process control block
 */
#include <machine/npx.h>

struct pcb {
	int	pcb_cr3;
	int	pcb_edi;
	int	pcb_esi;
	int	pcb_ebp;
	int	pcb_esp;
	int	pcb_ebx;
	int	pcb_eip;

	int     pcb_dr0;
	int     pcb_dr1;
	int     pcb_dr2;
	int     pcb_dr3;
	int     pcb_dr6;
	int     pcb_dr7;

	union	savefpu	pcb_save;
	u_char	pcb_flags;
#define	FP_SOFTFP	0x01	/* process using software fltng pnt emulator */
#define	PCB_DBREGS	0x02	/* process using debug registers */
#define	PCB_NPXTRAP	0x04	/* npx trap pending */
	caddr_t	pcb_onfault;	/* copyin/out fault recovery */
	int	pcb_gs;
	struct	pcb_ext	*pcb_ext;	/* optional pcb extension */
	u_long	__pcb_spare[3];	/* adjust to avoid core dump size changes */
};

/*
 * The pcb is augmented with machine-dependent additional data for
 * core dumps. For the i386: ???
 */
struct md_coredump {
};

#ifdef _KERNEL
void	savectx __P((struct pcb *));
#endif

#endif /* _I386_PCB_H_ */
