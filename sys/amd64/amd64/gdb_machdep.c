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
#include <sys/reg.h>
#include <sys/signal.h>

#include <machine/cpufunc.h>
#include <machine/frame.h>
#include <machine/gdb_machdep.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <machine/psl.h>
#include <machine/specialreg.h>
#include <machine/trap.h>
#include <machine/frame.h>
#include <machine/endian.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{
	static uint32_t _kcodesel = GSEL(GCODE_SEL, SEL_KPL);
	static uint32_t _kdatasel = GSEL(GDATA_SEL, SEL_KPL);

	*regsz = gdb_cpu_regsz(regnum);

	if (kdb_thread  == curthread) {
		switch (regnum) {
		case GDB_REG_RAX: return (&kdb_frame->tf_rax);
		case GDB_REG_RCX: return (&kdb_frame->tf_rcx);
		case GDB_REG_RDX: return (&kdb_frame->tf_rdx);
		case GDB_REG_RSI: return (&kdb_frame->tf_rsi);
		case GDB_REG_RDI: return (&kdb_frame->tf_rdi);
		case GDB_REG_R8:  return (&kdb_frame->tf_r8);
		case GDB_REG_R9:  return (&kdb_frame->tf_r9);
		case GDB_REG_R10: return (&kdb_frame->tf_r10);
		case GDB_REG_R11: return (&kdb_frame->tf_r11);
		case GDB_REG_RFLAGS: return (&kdb_frame->tf_rflags);
		case GDB_REG_CS:  return (&kdb_frame->tf_cs);
		case GDB_REG_SS:  return (&kdb_frame->tf_ss);
		}
	}
	switch (regnum) {
	case GDB_REG_RBX: return (&kdb_thrctx->pcb_rbx);
	case GDB_REG_RBP: return (&kdb_thrctx->pcb_rbp);
	case GDB_REG_RSP: return (&kdb_thrctx->pcb_rsp);
	case GDB_REG_R12: return (&kdb_thrctx->pcb_r12);
	case GDB_REG_R13: return (&kdb_thrctx->pcb_r13);
	case GDB_REG_R14: return (&kdb_thrctx->pcb_r14);
	case GDB_REG_R15: return (&kdb_thrctx->pcb_r15);
	case GDB_REG_PC:  return (&kdb_thrctx->pcb_rip);
	case GDB_REG_CS:  return (&_kcodesel);
	case GDB_REG_SS:  return (&_kdatasel);
	}
	return (NULL);
}

void
gdb_cpu_setreg(int regnum, void *val)
{
	register_t regval = *(register_t *)val;

	/*
	 * Write registers to the trapframe and pcb, if applicable.
	 * Some scratch registers are not tracked by the pcb.
	 */
	if (kdb_thread == curthread) {
		switch (regnum) {
		case GDB_REG_RAX: kdb_frame->tf_rax = regval; break;
		case GDB_REG_RBX: kdb_frame->tf_rbx = regval; break;
		case GDB_REG_RCX: kdb_frame->tf_rcx = regval; break;
		case GDB_REG_RDX: kdb_frame->tf_rdx = regval; break;
		case GDB_REG_RSI: kdb_frame->tf_rsi = regval; break;
		case GDB_REG_RDI: kdb_frame->tf_rdi = regval; break;
		case GDB_REG_RBP: kdb_frame->tf_rbp = regval; break;
		case GDB_REG_RSP: kdb_frame->tf_rsp = regval; break;
		case GDB_REG_R8:  kdb_frame->tf_r8  = regval; break;
		case GDB_REG_R9:  kdb_frame->tf_r9  = regval; break;
		case GDB_REG_R10: kdb_frame->tf_r10 = regval; break;
		case GDB_REG_R11: kdb_frame->tf_r11 = regval; break;
		case GDB_REG_R12: kdb_frame->tf_r12 = regval; break;
		case GDB_REG_R13: kdb_frame->tf_r13 = regval; break;
		case GDB_REG_R14: kdb_frame->tf_r14 = regval; break;
		case GDB_REG_R15: kdb_frame->tf_r15 = regval; break;
		case GDB_REG_PC:  kdb_frame->tf_rip = regval; break;
		}
	}
	switch (regnum) {
	case GDB_REG_RBX: kdb_thrctx->pcb_rbx = regval; break;
	case GDB_REG_RBP: kdb_thrctx->pcb_rbp = regval; break;
	case GDB_REG_RSP: kdb_thrctx->pcb_rsp = regval; break;
	case GDB_REG_R12: kdb_thrctx->pcb_r12 = regval; break;
	case GDB_REG_R13: kdb_thrctx->pcb_r13 = regval; break;
	case GDB_REG_R14: kdb_thrctx->pcb_r14 = regval; break;
	case GDB_REG_R15: kdb_thrctx->pcb_r15 = regval; break;
	case GDB_REG_PC:  kdb_thrctx->pcb_rip = regval; break;
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

void *
gdb_begin_write(void)
{

	return (disable_wp() ? &gdb_begin_write : NULL);
}

void
gdb_end_write(void *arg)
{

	restore_wp(arg != NULL);
}
