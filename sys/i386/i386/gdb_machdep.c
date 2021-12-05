/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
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
#include <sys/proc.h>
#include <sys/signal.h>

#include <machine/endian.h>
#include <machine/frame.h>
#include <machine/gdb_machdep.h>
#include <machine/pcb.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{
	static uint32_t _kcodesel = GSEL(GCODE_SEL, SEL_KPL);
	static uint32_t _kdatasel = GSEL(GDATA_SEL, SEL_KPL);
	static uint32_t _kprivsel = GSEL(GPRIV_SEL, SEL_KPL);

	*regsz = gdb_cpu_regsz(regnum);

	if (kdb_thread == curthread) {
		switch (regnum) {
		case 0:	return (&kdb_frame->tf_eax);
		case 1:	return (&kdb_frame->tf_ecx);
		case 2:	return (&kdb_frame->tf_edx);
		case 9: return (&kdb_frame->tf_eflags);
		case 10: return (&kdb_frame->tf_cs);
		case 12: return (&kdb_frame->tf_ds);
		case 13: return (&kdb_frame->tf_es);
		case 14: return (&kdb_frame->tf_fs);
		}
	}
	switch (regnum) {
	case 3:  return (&kdb_thrctx->pcb_ebx);
	case 4:  return (&kdb_thrctx->pcb_esp);
	case 5:  return (&kdb_thrctx->pcb_ebp);
	case 6:  return (&kdb_thrctx->pcb_esi);
	case 7:  return (&kdb_thrctx->pcb_edi);
	case 8:  return (&kdb_thrctx->pcb_eip);
	case 10: return (&_kcodesel);
	case 11: return (&_kdatasel);
	case 12: return (&_kdatasel);
	case 13: return (&_kdatasel);
	case 14: return (&_kprivsel);
	case 15: return (&kdb_thrctx->pcb_gs);
	}
	return (NULL);
}

void
gdb_cpu_setreg(int regnum, void *val)
{

	switch (regnum) {
	case GDB_REG_PC:
		kdb_thrctx->pcb_eip = *(register_t *)val;
		if (kdb_thread  == curthread)
			kdb_frame->tf_eip = *(register_t *)val;
	}
}

int
gdb_cpu_signal(int type, int code)
{

	switch (type) {
	case T_BPTFLT: return (SIGTRAP);
	case T_ARITHTRAP: return (SIGFPE);
	case T_PROTFLT: return (SIGSEGV);
	case T_TRCTRAP: return (SIGTRAP);
	case T_PAGEFLT: return (SIGSEGV);
	case T_DIVIDE: return (SIGFPE);
	case T_NMI: return (SIGTRAP);
	case T_FPOPFLT: return (SIGILL);
	case T_TSSFLT: return (SIGSEGV);
	case T_SEGNPFLT: return (SIGSEGV);
	case T_STKFLT: return (SIGSEGV);
	case T_XMMFLT: return (SIGFPE);
	}
	return (SIGEMT);
}

void
gdb_cpu_stop_reason(int type, int code)
{
	uintmax_t val;

	val = 0;
	if (type == T_TRCTRAP) {
		/* NB: 'code' contains the value of dr6 at the trap. */
		if ((code & DBREG_DR6_B(0)) != 0) {
			val = rdr0();
		}
		if ((code & DBREG_DR6_B(1)) != 0) {
			val = rdr1();
		}
		if ((code & DBREG_DR6_B(2)) != 0) {
			val = rdr2();
		}
		if ((code & DBREG_DR6_B(3)) != 0) {
			val = rdr3();
		}

		/*
		 * TODO: validate the bits in DR7 to differentiate between a
		 * watchpoint trap and a hardware breakpoint trap (currently
		 * unsupported).
		 */
		if (val != 0) {
			gdb_tx_str("watch:");
			gdb_tx_varhex(val);
			gdb_tx_char(';');
		}
	}
}
