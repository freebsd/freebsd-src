/* $FreeBSD$ */
/* $NetBSD: pal.s,v 1.12 1998/02/27 03:44:53 thorpej Exp $ */

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

/*
 * The various OSF PALcode routines.
 *
 * The following code is originally derived from pages: (I) 6-5 - (I) 6-7
 * and (III) 2-1 - (III) 2-25 of "Alpha Architecture Reference Manual" by
 * Richard L. Sites.
 *
 * Updates taken from pages: (II-B) 2-1 - (II-B) 2-33 of "Alpha AXP
 * Architecture Reference Manual, Second Edition" by Richard L. Sites
 * and Richard T. Witek.
 */

#include <machine/asm.h>

__KERNEL_RCSID(1, "$NetBSD: pal.s,v 1.12 1998/02/27 03:44:53 thorpej Exp $");

inc2:	.stabs	__FILE__,132,0,0,inc2
	.text
	.loc	1 __LINE__

/*
 * alpha_rpcc: read process cycle counter (XXX INSTRUCTION, NOT PALcode OP)
 */
        .text
LEAF(alpha_rpcc,1)
        rpcc    v0
        RET
        END(alpha_rpcc)

/*
 * alpha_mb: memory barrier (XXX INSTRUCTION, NOT PALcode OP)
 */
	.text
LEAF(alpha_mb,0)
	mb
	RET
	END(alpha_mb)

/*
 * alpha_wmb: write memory barrier (XXX INSTRUCTION, NOT PALcode OP)
 */
	.text
LEAF(alpha_wmb,0)
	/* wmb XXX */
	mb /* XXX */
	RET
	END(alpha_wmb)

/*
 * alpha_amask: read architecture features (XXX INSTRUCTION, NOT PALcode OP)
 *
 * Arguments:
 *	a0	bitmask of features to test
 *
 * Returns:
 *	v0	bitmask - bit is _cleared_ if feature is supported
 */
	.text
LEAF(alpha_amask,1)
	amask	a0, v0
	RET
	END(alpha_amask)

/*
 * alpha_implver: read implementation version (XXX INSTRUCTION, NOT PALcode OP)
 *
 * Returns:
 *	v0	implementation version - see <machine/alpha_cpu.h>
 */
	.text
LEAF(alpha_implver,0)
#if 0
	implver	0x1, v0
#else
	.long	0x47e03d80	/* XXX gas(1) does the Wrong Thing */
#endif
	RET
	END(alpha_implver)

/*
 * alpha_pal_imb: I-Stream memory barrier. [UNPRIVILEGED]
 * (Makes instruction stream coherent with data stream.)
 */
	.text
LEAF(alpha_pal_imb,0)
	call_pal PAL_imb
	RET
	END(alpha_pal_imb)

/*
 * alpha_pal_cflush: Cache flush [PRIVILEGED]
 *
 * Flush the entire physical page specified by the PFN specified in
 * a0 from any data caches associated with the current processor.
 *
 * Arguments:
 *	a0	page frame number of page to flush
 */
	.text
LEAF(alpha_pal_cflush,1)
	call_pal PAL_cflush
	RET
	END(alpha_pal_cflush)

/*
 * alpha_pal_draina: Drain aborts. [PRIVILEGED]
 */
	.text
LEAF(alpha_pal_draina,0)
	call_pal PAL_draina
	RET
	END(alpha_pal_draina)

/*
 * alpha_pal_halt: Halt the processor. [PRIVILEGED]
 */
	.text
LEAF(alpha_pal_halt,0)
	call_pal PAL_halt
	br	zero,alpha_pal_halt	/* Just in case */
	RET
	END(alpha_pal_halt)

/*
 * alpha_pal_rdmces: Read MCES processor register. [PRIVILEGED]
 *
 * Return:
 *	v0	current MCES value
 */
	.text
LEAF(alpha_pal_rdmces,1)
	call_pal PAL_OSF1_rdmces
	RET
	END(alpha_pal_rdmces)

/*
 * alpha_pal_rdps: Read processor status. [PRIVILEGED]
 *
 * Return:
 *	v0	current PS value
 */
	.text
LEAF(alpha_pal_rdps,0)
	call_pal PAL_OSF1_rdps
	RET
	END(alpha_pal_rdps)

/*
 * alpha_pal_rdusp: Read user stack pointer. [PRIVILEGED]
 *
 * Return:
 *	v0	current user stack pointer
 */
	.text
LEAF(alpha_pal_rdusp,0)
	call_pal PAL_OSF1_rdusp
	RET
	END(alpha_pal_rdusp)

/*
 * alpha_pal_rdval: Read system value. [PRIVILEGED]
 *
 * Returns the sysvalue in v0, allowing access to a 64-bit
 * per-processor value for use by the operating system.
 *
 * Return:
 *	v0	sysvalue
 */
	.text
LEAF(alpha_pal_rdval,0)
	call_pal PAL_OSF1_rdval
	RET
	END(alpha_pal_rdval)

/*
 * alpha_pal_swpipl: Swap Interrupt priority level. [PRIVILEGED]
 * _alpha_pal_swpipl: Same, from profiling code. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new IPL
 *
 * Return:
 *	v0	old IPL
 */
	.text
LEAF(alpha_pal_swpipl,1)
	call_pal PAL_OSF1_swpipl
	RET
	END(alpha_pal_swpipl)

LEAF_NOPROFILE(_alpha_pal_swpipl,1)
	call_pal PAL_OSF1_swpipl
	RET
	END(_alpha_pal_swpipl)

/*
 * alpha_pal_tbi: Translation buffer invalidate. [PRIVILEGED]
 *
 * Arguments:
 *	a0	operation selector
 *	a1	address to operate on (if necessary)
 */
	.text
LEAF(alpha_pal_tbi,2)
	call_pal PAL_OSF1_tbi
	RET
	END(alpha_pal_tbi)

/*
 * alpha_pal_whami: Who am I? [PRIVILEGED]
 *
 * Return:
 *	v0	processor number
 */
	.text
LEAF(alpha_pal_whami,0)
	call_pal PAL_OSF1_whami
	RET
	END(alpha_pal_whami)

/*
 * alpha_pal_wrent: Write system entry address. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new vector
 *	a1	vector selector
 */
	.text
LEAF(alpha_pal_wrent,2)
	call_pal PAL_OSF1_wrent
	RET
	END(alpha_pal_wrent)

/*
 * alpha_pal_wrfen: Write floating-point enable. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new enable value (val & 0x1 -> enable).
 */
	.text
LEAF(alpha_pal_wrfen,1)
	call_pal PAL_OSF1_wrfen
	RET
	END(alpha_pal_wrfen)

/*
 * alpha_pal_wripir: Write interprocessor interrupt request. [PRIVILEGED]
 *
 * Generate an interprocessor interrupt on the processor specified by
 * processor number in a0.
 *
 * Arguments:
 *	a0	processor to interrupt
 */
	.text
LEAF(alpha_pal_wripir,1)
	call_pal PAL_ipir
	RET
	END(alpha_pal_wripir)

/*
 * alpha_pal_wrusp: Write user stack pointer. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new user stack pointer
 */
	.text
LEAF(alpha_pal_wrusp,1)
	call_pal PAL_OSF1_wrusp
	RET
	END(alpha_pal_wrusp)

/*
 * alpha_pal_wrvptptr: Write virtual page table pointer. [PRIVILEGED]
 *
 * Arguments:
 *	a0	new virtual page table pointer
 */
	.text
LEAF(alpha_pal_wrvptptr,1)
	call_pal PAL_OSF1_wrvptptr
	RET
	END(alpha_pal_wrvptptr)

/*
 * alpha_pal_wrmces: Write MCES processor register. [PRIVILEGED]
 *
 * Arguments:
 *	a0	value to write to MCES
 */
	.text
LEAF(alpha_pal_wrmces,1)
	call_pal PAL_OSF1_wrmces
	RET
	END(alpha_pal_wrmces)

/*
 * alpha_pal_wrval: Write system value. [PRIVILEGED]
 *
 * Write the value passed in a0 to this processor's sysvalue.
 *
 * Arguments:
 *	a0	value to write to sysvalue
 */
LEAF(alpha_pal_wrval,1)
	call_pal PAL_OSF1_wrval
	RET
	END(alpha_pal_wrval)

/*
 * alpha_pal_swpctx: Swap context. [PRIVILEGED]
 *
 * Switch to a new process context.
 *
 * Arguments:
 *	a0	physical address of hardware PCB describing context
 *
 * Returns:
 *	v0	physical address of hardware PCB describing previous context
 */
LEAF(alpha_pal_swpctx,1)
	call_pal PAL_OSF1_swpctx
	RET
	END(alpha_pal_swpctx)


/*
 * alpha_pal_wrperfmon:	  Write perf monitor [PRIVILEGED]
 *
 * Enables / disables performance monitoring hardware
 *
 * Arguments:
 *	a0	function type
 *
 *	a1	function parameter
 *
 * Returns:
 *	v0	0 (failure) or 1 (success)
 */
LEAF(alpha_pal_wrperfmon,2)
	call_pal PAL_OSF1_wrperfmon
	RET
	END(alpha_pal_wrperfmon)

	
