/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kdb.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/signal.h>

#include <machine/gdb_machdep.h>
#include <machine/pcb.h>

#include <machine/hid.h>
#include <machine/spr.h>

#include <machine/trap.h>

#include <gdb/gdb.h>
#include <gdb/gdb_int.h>

extern vm_offset_t __startkernel;

void *
gdb_cpu_getreg(int regnum, size_t *regsz)
{

	*regsz = gdb_cpu_regsz(regnum);

	if (kdb_thread == curthread) {
		if (regnum == 0 || (regnum >= 2 && regnum <= 31))
			return (kdb_frame->fixreg + regnum);
		if (regnum == 64)
			return (&kdb_frame->srr0);
		if (regnum == 67)
			return (&kdb_frame->lr);
	}

	if (regnum == 1)
		return (&kdb_thrctx->pcb_sp);
	if (regnum == 2 && *regsz == 8)
		return (&kdb_thrctx->pcb_toc);
	if (regnum >= 12 && regnum <= 31)
		return (kdb_thrctx->pcb_context + (regnum - 12));
	if (regnum == 64)
		return (&kdb_thrctx->pcb_lr);

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
#if defined(BOOKE)
	if (vector == EXC_DEBUG || vector == EXC_PGM)
		return (SIGTRAP);
#else
	if (vector == EXC_TRC || vector == EXC_RUNMODETRC)
		return (SIGTRAP);
#endif

	return (SIGEMT);
}

void
gdb_cpu_do_offsets(void)
{
	/*
	 * On PowerPC, .text starts at KERNBASE + SIZEOF_HEADERS and
	 * text segment at KERNBASE - SIZEOF_HEADERS.
	 * On PowerPC64, .text starts at KERNBASE and text segment at
	 * KERNBASE - 0x100.
	 * In both cases, the text segment offset is aligned to 64KB.
	 *
	 * The __startkernel variable holds the relocated KERNBASE offset.
	 * Thus, as long as SIZEOF_HEADERS doesn't get bigger than 0x100
	 * (which would lead to other issues), aligning __startkernel to
	 * 64KB gives the text segment offset.
	 *
	 * TODO: Add DataSeg to response. On PowerPC64 all sections reside
	 * in a single LOAD segment, but on PowerPC modifiable data reside
	 * in a separate segment, that GDB should also relocate.
	 */
	gdb_tx_begin(0);
	gdb_tx_str("TextSeg=");
	gdb_tx_varhex(__startkernel & ~0xffff);
	gdb_tx_end();
}
