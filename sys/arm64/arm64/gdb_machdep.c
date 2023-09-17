/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 The FreeBSD Foundation
 *
 * This software was developed by Mitchell Horne under sponsorship from
 * the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 * notice, this list of conditions and the following disclaimer in the
 * documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
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

#include <machine/armreg.h>
#include <machine/frame.h>
#include <machine/gdb_machdep.h>
#include <machine/pcb.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{

	*regsz = gdb_cpu_regsz(regnum);

	if (kdb_thread == curthread) {
		switch (regnum) {
		case GDB_REG_LR:   return (&kdb_frame->tf_lr);
		case GDB_REG_SP:   return (&kdb_frame->tf_sp);
		case GDB_REG_PC:   return (&kdb_frame->tf_elr);
		case GDB_REG_CSPR: return (&kdb_frame->tf_spsr);
		}
	}
	switch (regnum) {
	case GDB_REG_SP: return (&kdb_thrctx->pcb_sp);
	case GDB_REG_PC: /* FALLTHROUGH */
	case GDB_REG_LR: return (&kdb_thrctx->pcb_x[PCB_LR]);
	default:
		if (regnum >= GDB_REG_X19 && regnum <= GDB_REG_X29)
			return (&kdb_thrctx->pcb_x[regnum - GDB_REG_X19]);
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
		case GDB_REG_PC: kdb_frame->tf_elr = regval; break;
		case GDB_REG_SP: kdb_frame->tf_sp  = regval; break;
		default:
			if (regnum >= GDB_REG_X0 && regnum <= GDB_REG_X29) {
				kdb_frame->tf_x[regnum] = regval;
			}
			break;
		}
	}
	switch (regnum) {
	case GDB_REG_PC: /* FALLTHROUGH */
	case GDB_REG_LR: kdb_thrctx->pcb_x[PCB_LR] = regval; break;
	case GDB_REG_SP: kdb_thrctx->pcb_sp = regval; break;
	default:
		if (regnum >= GDB_REG_X19 && regnum <= GDB_REG_X29) {
			kdb_thrctx->pcb_x[regnum - GDB_REG_X19] = regval;
		}
		break;
	}
}

int
gdb_cpu_signal(int type, int code __unused)
{

	switch (type) {
	case EXCP_WATCHPT_EL1:
	case EXCP_SOFTSTP_EL1:
	case EXCP_BRK:
		return (SIGTRAP);
	}
	return (SIGEMT);
}

void
gdb_cpu_stop_reason(int type, int code __unused)
{

	if (type == EXCP_WATCHPT_EL1) {
		gdb_tx_str("watch:");
		gdb_tx_varhex((uintmax_t)READ_SPECIALREG(far_el1));
		gdb_tx_char(';');
	}
}
