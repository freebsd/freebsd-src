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

#include <machine/globaldata.h>
#include <machine/globals.h>

/*
 * Machine-dependent part of the proc struct for the Alpha.
 */

struct mdproc {
	u_long		md_flags;
	vm_offset_t	md_bspstore;	/* initial ar.bspstore */
	struct	trapframe *md_tf;	/* trap/syscall registers */
};

#define	MDP_FPUSED	0x0001		/* Process used the FPU */
#define MDP_UAC_NOPRINT	0x0010		/* Don't print unaligned traps */
#define MDP_UAC_NOFIX	0x0020		/* Don't fixup unaligned traps */
#define MDP_UAC_SIGBUS	0x0040		/* Deliver SIGBUS upon
					   unaligned access */
#define MDP_UAC_MASK	(MDP_UAC_NOPRINT | MDP_UAC_NOFIX | MDP_UAC_SIGBUS)
