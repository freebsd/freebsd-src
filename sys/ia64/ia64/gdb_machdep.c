/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHORS ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/ia64/ia64/gdb_machdep.c,v 1.4 2005/01/06 22:18:22 imp Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signal.h>

#include <machine/gdb_machdep.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{
	static uint64_t synth;
	uint64_t cfm;

	*regsz = gdb_cpu_regsz(regnum);
	switch (regnum) {
	/* Registers 0-127: general registers. */
	case 1:  return (&kdb_thrctx->pcb_special.gp);
	case 4:  return (&kdb_thrctx->pcb_preserved.gr4);
	case 5:  return (&kdb_thrctx->pcb_preserved.gr5);
	case 6:  return (&kdb_thrctx->pcb_preserved.gr6);
	case 7:  return (&kdb_thrctx->pcb_preserved.gr7);
	case 12: return (&kdb_thrctx->pcb_special.sp);
	case 13: return (&kdb_thrctx->pcb_special.tp);
	/* Registers 128-255: floating-point registers. */
	case 130: return (&kdb_thrctx->pcb_preserved_fp.fr2);
	case 131: return (&kdb_thrctx->pcb_preserved_fp.fr3);
	case 132: return (&kdb_thrctx->pcb_preserved_fp.fr4);
	case 133: return (&kdb_thrctx->pcb_preserved_fp.fr5);
	case 144: return (&kdb_thrctx->pcb_preserved_fp.fr16);
	case 145: return (&kdb_thrctx->pcb_preserved_fp.fr17);
	case 146: return (&kdb_thrctx->pcb_preserved_fp.fr18);
	case 147: return (&kdb_thrctx->pcb_preserved_fp.fr19);
	case 148: return (&kdb_thrctx->pcb_preserved_fp.fr20);
	case 149: return (&kdb_thrctx->pcb_preserved_fp.fr21);
	case 150: return (&kdb_thrctx->pcb_preserved_fp.fr22);
	case 151: return (&kdb_thrctx->pcb_preserved_fp.fr23);
	case 152: return (&kdb_thrctx->pcb_preserved_fp.fr24);
	case 153: return (&kdb_thrctx->pcb_preserved_fp.fr25);
	case 154: return (&kdb_thrctx->pcb_preserved_fp.fr26);
	case 155: return (&kdb_thrctx->pcb_preserved_fp.fr27);
	case 156: return (&kdb_thrctx->pcb_preserved_fp.fr28);
	case 157: return (&kdb_thrctx->pcb_preserved_fp.fr29);
	case 158: return (&kdb_thrctx->pcb_preserved_fp.fr30);
	case 159: return (&kdb_thrctx->pcb_preserved_fp.fr31);
	/* Registers 320-327: branch registers. */
	case 320:
		if (kdb_thrctx->pcb_special.__spare == ~0UL)
			return (&kdb_thrctx->pcb_special.rp);
		break;
	case 321: return (&kdb_thrctx->pcb_preserved.br1);
	case 322: return (&kdb_thrctx->pcb_preserved.br2);
	case 323: return (&kdb_thrctx->pcb_preserved.br3);
	case 324: return (&kdb_thrctx->pcb_preserved.br4);
	case 325: return (&kdb_thrctx->pcb_preserved.br5);
	/* Registers 328-333: misc. other registers. */
	case 330: return (&kdb_thrctx->pcb_special.pr);
	case 331:
		if (kdb_thrctx->pcb_special.__spare == ~0UL) {
			synth = kdb_thrctx->pcb_special.iip;
			synth += (kdb_thrctx->pcb_special.psr >> 41) & 3;
			return (&synth);
		}
		return (&kdb_thrctx->pcb_special.rp);
	case 333:
		if (kdb_thrctx->pcb_special.__spare == ~0UL)
			return (&kdb_thrctx->pcb_special.cfm);
		return (&kdb_thrctx->pcb_special.pfs);
	/* Registers 334-461: application registers. */
	case 350: return (&kdb_thrctx->pcb_special.rsc);
	case 351: /* bsp */
	case 352: /* bspstore. */
		synth = kdb_thrctx->pcb_special.bspstore;
		if (kdb_thrctx->pcb_special.__spare == ~0UL) {
			synth += kdb_thrctx->pcb_special.ndirty;
		} else {
			cfm = kdb_thrctx->pcb_special.pfs;
			synth = ia64_bsp_adjust(synth,
			    IA64_CFM_SOF(cfm) - IA64_CFM_SOL(cfm));
		}
		return (&synth);
	case 353: return (&kdb_thrctx->pcb_special.rnat);
	case 370: return (&kdb_thrctx->pcb_special.unat);
	case 374: return (&kdb_thrctx->pcb_special.fpsr);
	case 398:
		if (kdb_thrctx->pcb_special.__spare == ~0UL)
			return (&kdb_thrctx->pcb_special.pfs);
		break;
	case 399: return (&kdb_thrctx->pcb_preserved.lc);
	}
	return (NULL);
}

void
gdb_cpu_setreg(int regnum, void *val)
{

	switch (regnum) {
	case GDB_REG_PC: break;
	}
}

int
gdb_cpu_signal(int vector, int dummy __unused)
{

	if (vector == IA64_VEC_BREAK || vector == IA64_VEC_SINGLE_STEP_TRAP)
		return (SIGTRAP);
	/* Add 100 so GDB won't translate the vector into signal names. */
	return (vector + 100);
}

int
gdb_cpu_query(void)
{
#if 0
	uint64_t bspstore, *kstack;
#endif
	uintmax_t slot;

	if (!gdb_rx_equal("Part:dirty:read::"))
		return (0);

	if (gdb_rx_varhex(&slot) < 0) {
		gdb_tx_err(EINVAL);
		return (-1);
	}

	gdb_tx_err(EINVAL);
	return (-1);

#if 0
	/* slot is unsigned. No need to test for negative values. */
	if (slot >= (kdb_frame->tf_special.ndirty >> 3)) {
		return (-1);
	}

	/*
	 * If the trapframe describes a kernel entry, bspstore holds
	 * the address of the user backing store. Calculate the right
	 * kernel stack address. See also ptrace_machdep().
	 */
	bspstore = kdb_frame->tf_special.bspstore;
	kstack = (bspstore >= IA64_RR_BASE(5)) ? (uint64_t*)bspstore :
	    (uint64_t*)(kdb_thread->td_kstack + (bspstore & 0x1ffUL));
	gdb_tx_begin('\0');
	gdb_tx_mem((void*)(kstack + slot), 8);
	gdb_tx_end();
	return (1);
#endif
}
