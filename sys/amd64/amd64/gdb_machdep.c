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
#include <sys/proc.h>
#include <sys/signal.h>

#include <machine/frame.h>
#include <machine/gdb_machdep.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/trap.h>

#include <gdb/gdb.h>

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{
	struct trapframe *tf = kdb_frame;

	*regsz = gdb_cpu_regsz(regnum);
	switch (regnum) {
	case 0:  return (&tf->tf_rax);
	case 1:  return (&tf->tf_rbx);
	case 2:  return (&tf->tf_rcx);
	case 3:  return (&tf->tf_rdx);
	case 4:  return (&tf->tf_rsi);
	case 5:  return (&tf->tf_rdi);
	case 6:  return (&tf->tf_rbp);
	case 7:  return (&tf->tf_rsp);
	case 8:  return (&tf->tf_r8);
	case 9:  return (&tf->tf_r9);
	case 10: return (&tf->tf_r10);
	case 11: return (&tf->tf_r11);
	case 12: return (&tf->tf_r12);
	case 13: return (&tf->tf_r13);
	case 14: return (&tf->tf_r14);
	case 15: return (&tf->tf_r15);
	case 16: return (&tf->tf_rip);
	case 17: return (&tf->tf_rflags);
	case 18: return (&tf->tf_cs);
	case 19: return (&tf->tf_ss);
	}
	return (NULL);
}

void
gdb_cpu_setreg(int regnum, register_t val)
{
	struct trapframe *tf = kdb_frame;

	switch (regnum) {
	case GDB_REG_PC: tf->tf_rip = val; break;
	}
}
