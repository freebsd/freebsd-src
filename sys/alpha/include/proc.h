/* $FreeBSD$ */
/* From: NetBSD: proc.h,v 1.3 1997/04/06 08:47:36 cgd Exp */

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#ifndef _MACHINE_PROC_H_
#define	_MACHINE_PROC_H_

/*
 * Machine-dependent part of the proc struct for the Alpha.
 */

struct mdbpt {
	vm_offset_t	addr;
	u_int32_t	contents;
};

struct mdthread {
	u_long		md_flags;
	struct pcb	*md_pcbpaddr;	/* phys addr of the pcb */
	struct mdbpt	md_sstep[2];	/* two single step breakpoints */
	u_int64_t	md_hae;		/* user HAE register value */
	void            *osf_sigtramp;  /* user-level signal trampoline */
	u_int		md_kernnest;	/* nesting level in the kernel */
};

#define	MDP_FPUSED	0x0001		/* Process used the FPU */
#define MDP_STEP1	0x0002		/* Single step normal instruction */
#define MDP_STEP2	0x0004		/* Single step branch instruction */
#define MDP_HAEUSED	0x0008		/* Process used the HAE */
#define MDP_UAC_NOPRINT	0x0010		/* Don't print unaligned traps */
#define MDP_UAC_NOFIX	0x0020		/* Don't fixup unaligned traps */
#define MDP_UAC_SIGBUS	0x0040		/* Deliver SIGBUS upon
					   unaligned access */
#define MDP_UAC_MASK	(MDP_UAC_NOPRINT | MDP_UAC_NOFIX | MDP_UAC_SIGBUS)

struct mdproc {
};

#endif /* !_MACHINE_PROC_H_ */
