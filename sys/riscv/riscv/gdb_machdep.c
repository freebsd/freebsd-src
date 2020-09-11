/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2021 Mitchell Horne <mhorne@FreeBSD.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signal.h>

#include <machine/frame.h>
#include <machine/gdb_machdep.h>
#include <machine/pcb.h>
#include <machine/riscvreg.h>

#include <gdb/gdb.h>

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{
	*regsz = gdb_cpu_regsz(regnum);

	if (kdb_thread == curthread) {
		switch (regnum) {
		case GDB_REG_RA:	return (&kdb_frame->tf_ra);
		case GDB_REG_PC:	return (&kdb_frame->tf_sepc);
		case GDB_REG_SSTATUS:	return (&kdb_frame->tf_sstatus);
		case GDB_REG_STVAL:	return (&kdb_frame->tf_stval);
		case GDB_REG_SCAUSE:	return (&kdb_frame->tf_scause);
		default:
			if (regnum >= GDB_REG_A0 && regnum < GDB_REG_S2)
				return (&kdb_frame->tf_a[regnum - GDB_REG_A0]);
			if (regnum >= GDB_REG_T0 && regnum < GDB_REG_FP)
				return (&kdb_frame->tf_t[regnum - GDB_REG_T0]);
			if (regnum >= GDB_REG_T3 && regnum < GDB_REG_PC)
				return (&kdb_frame->tf_t[regnum - GDB_REG_T3]);
			break;
		}
	}
	switch (regnum) {
	case GDB_REG_PC: /* FALLTHROUGH */
	case GDB_REG_RA: return (&kdb_thrctx->pcb_ra);
	case GDB_REG_SP: return (&kdb_thrctx->pcb_sp);
	case GDB_REG_GP: return (&kdb_thrctx->pcb_gp);
	case GDB_REG_TP: return (&kdb_thrctx->pcb_tp);
	case GDB_REG_FP: return (&kdb_thrctx->pcb_s[0]);
	case GDB_REG_S1: return (&kdb_thrctx->pcb_s[1]);
	default:
		if (regnum >= GDB_REG_S2 && regnum < GDB_REG_T3)
			return (&kdb_thrctx->pcb_s[regnum - GDB_REG_S2]);
		break;
	}

	return (NULL);
}

void
gdb_cpu_setreg(int regnum, void *val)
{
	register_t regval = *(register_t *)val;

	/* For curthread, keep the pcb and trapframe in sync. */
	if (kdb_thread == curthread) {
		switch (regnum) {
		case GDB_REG_PC:	kdb_frame->tf_sepc = regval; break;
		case GDB_REG_RA:	kdb_frame->tf_ra = regval; break;
		case GDB_REG_SP:	kdb_frame->tf_sp = regval; break;
		case GDB_REG_GP:	kdb_frame->tf_gp = regval; break;
		case GDB_REG_TP:	kdb_frame->tf_tp = regval; break;
		case GDB_REG_FP:	kdb_frame->tf_s[0] = regval; break;
		case GDB_REG_S1:	kdb_frame->tf_s[1] = regval; break;
		case GDB_REG_SSTATUS:	kdb_frame->tf_sstatus = regval; break;
		case GDB_REG_STVAL:	kdb_frame->tf_stval = regval; break;
		case GDB_REG_SCAUSE:	kdb_frame->tf_scause = regval; break;
		default:
			if (regnum >= GDB_REG_A0 && regnum < GDB_REG_S2)
				kdb_frame->tf_a[regnum - GDB_REG_A0] = regval;
			if (regnum >= GDB_REG_S2 && regnum < GDB_REG_T3)
				kdb_frame->tf_s[regnum - GDB_REG_S2] = regval;
			if (regnum >= GDB_REG_T0 && regnum < GDB_REG_FP)
				kdb_frame->tf_t[regnum - GDB_REG_T0] = regval;
			if (regnum >= GDB_REG_T3 && regnum < GDB_REG_PC)
				kdb_frame->tf_t[regnum - GDB_REG_T3] = regval;
			break;
		}
	}
	switch (regnum) {
	case GDB_REG_PC: /* FALLTHROUGH */
	case GDB_REG_RA: kdb_thrctx->pcb_ra = regval; break;
	case GDB_REG_SP: kdb_thrctx->pcb_sp = regval; break;
	case GDB_REG_GP: kdb_thrctx->pcb_gp = regval; break;
	case GDB_REG_TP: kdb_thrctx->pcb_tp = regval; break;
	case GDB_REG_FP: kdb_thrctx->pcb_s[0] = regval; break;
	case GDB_REG_S1: kdb_thrctx->pcb_s[1] = regval; break;
	default:
		if (regnum >= GDB_REG_S2 && regnum < GDB_REG_T3)
			kdb_thrctx->pcb_s[regnum - GDB_REG_S2] = regval;
		break;
	}
}

int
gdb_cpu_signal(int type, int code)
{

	if (type == SCAUSE_BREAKPOINT)
		return (SIGTRAP);

	return (SIGEMT);
}
