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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/signal.h>

#include <machine/gdb_machdep.h>
#include <machine/pcb.h>
#include <machine/reg.h>

#include <gdb/gdb.h>

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{

	*regsz = gdb_cpu_regsz(regnum);
	switch (regnum) {
	case 9:  return (&kdb_thrctx->pcb_context[0]);
	case 10: return (&kdb_thrctx->pcb_context[1]);
	case 11: return (&kdb_thrctx->pcb_context[2]);
	case 12: return (&kdb_thrctx->pcb_context[3]);
	case 13: return (&kdb_thrctx->pcb_context[4]);
	case 14: return (&kdb_thrctx->pcb_context[5]);
	case 15: return (&kdb_thrctx->pcb_context[6]);
	case 30: return (&kdb_thrctx->pcb_hw.apcb_ksp);
	case 64: return (&kdb_thrctx->pcb_context[7]);
	}
	return (NULL);
}

void
gdb_cpu_setreg(int regnum, register_t val)
{
	switch (regnum) {
	}
}

int
gdb_cpu_signal(int entry, int code)
{
	switch (entry) {
	case ALPHA_KENTRY_INT:
	case ALPHA_KENTRY_ARITH:
		return (SIGILL);	/* Can this happen? */
	case ALPHA_KENTRY_MM:
		switch (code) {
		case ALPHA_MMCSR_INVALTRANS:
			return (SIGSEGV);
		case ALPHA_MMCSR_ACCESS:
		case ALPHA_MMCSR_FOR:
		case ALPHA_MMCSR_FOE:
		case ALPHA_MMCSR_FOW:
			return (SIGBUS);
		}
	case ALPHA_KENTRY_IF:
		switch (code) {
		case ALPHA_IF_CODE_BUGCHK:
		case ALPHA_IF_CODE_BPT:
			return (SIGTRAP);
		case ALPHA_IF_CODE_GENTRAP:
		case ALPHA_IF_CODE_FEN:
		case ALPHA_IF_CODE_OPDEC:
			return (SIGILL);
		}
	case ALPHA_KENTRY_UNA:
		return (SIGSEGV);
	case ALPHA_KENTRY_SYS:
		return (SIGILL);
	}
	return (SIGILL);
}
