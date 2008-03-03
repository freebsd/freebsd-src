/*-
 * Copyright (c) 2006 Marcel Moolenaar
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signal.h>

#include <machine/gdb_machdep.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include <machine/hid.h>
#include <machine/spr.h>

#include <machine/trap.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{

	*regsz = gdb_cpu_regsz(regnum);

	if (kdb_thread == curthread) {
		switch (regnum) {
		case 0: return (&kdb_frame->fixreg[0]);
		case 2: return (&kdb_frame->fixreg[2]);
		case 3: return (&kdb_frame->fixreg[3]);
		case 4: return (&kdb_frame->fixreg[4]);
		case 5: return (&kdb_frame->fixreg[5]);
		case 6: return (&kdb_frame->fixreg[6]);
		case 7: return (&kdb_frame->fixreg[7]);
		case 8: return (&kdb_frame->fixreg[8]);
		case 9: return (&kdb_frame->fixreg[9]);
		case 10: return (&kdb_frame->fixreg[10]);
		case 11: return (&kdb_frame->fixreg[11]);
		case 12: return (&kdb_frame->fixreg[12]);
		case 13: return (&kdb_frame->fixreg[13]);
		case 64: return (&kdb_frame->srr0);
		case 67: return (&kdb_frame->lr);

		}
	}

	switch (regnum) {
	case 1: return (&kdb_thrctx->pcb_sp);
	case 14: return (&kdb_thrctx->pcb_context[0]);
	case 15: return (&kdb_thrctx->pcb_context[1]);
	case 16: return (&kdb_thrctx->pcb_context[2]);
	case 17: return (&kdb_thrctx->pcb_context[3]);
	case 18: return (&kdb_thrctx->pcb_context[4]);
	case 19: return (&kdb_thrctx->pcb_context[5]);
	case 20: return (&kdb_thrctx->pcb_context[6]);
	case 21: return (&kdb_thrctx->pcb_context[7]);
	case 22: return (&kdb_thrctx->pcb_context[8]);
	case 23: return (&kdb_thrctx->pcb_context[9]);
	case 24: return (&kdb_thrctx->pcb_context[10]);
	case 25: return (&kdb_thrctx->pcb_context[11]);
	case 26: return (&kdb_thrctx->pcb_context[12]);
	case 27: return (&kdb_thrctx->pcb_context[13]);
	case 28: return (&kdb_thrctx->pcb_context[14]);
	case 29: return (&kdb_thrctx->pcb_context[15]);
	case 30: return (&kdb_thrctx->pcb_context[16]);
	case 31: return (&kdb_thrctx->pcb_context[17]);
	case 64: return (&kdb_thrctx->pcb_lr);
	}

	return (NULL);
}

void
gdb_cpu_setreg(int regnum, void *val)
{

	switch (regnum) {
	case GDB_REG_PC:
		break;
	}
}

int
gdb_cpu_signal(int vector, int dummy __unused)
{
#ifdef E500
	if (vector == EXC_DEBUG || vector == EXC_PGM)
		return (SIGTRAP);
#else
	if (vector == EXC_TRC || vector == EXC_RUNMODETRC)
		return (SIGTRAP);
#endif

	if (vector <= 255)
		return (vector);
	else
		return (SIGEMT);
}
