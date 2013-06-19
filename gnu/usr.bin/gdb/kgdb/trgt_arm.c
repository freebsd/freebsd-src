/*
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

#include <sys/types.h>
#ifndef CROSS_DEBUGGER
#include <machine/pcb.h>
#include <machine/frame.h>
#include <machine/armreg.h>
#endif
#include <err.h>
#include <kvm.h>
#include <string.h>

#include <defs.h>
#include <target.h>
#include <gdbthread.h>
#include <inferior.h>
#include <regcache.h>
#include <frame-unwind.h>
#include <arm-tdep.h>

#include "kgdb.h"

CORE_ADDR
kgdb_trgt_core_pcb(u_int cpuid)
{
	return (kgdb_trgt_stop_pcb(cpuid, sizeof(struct pcb)));
}

void
kgdb_trgt_fetch_registers(int regno __unused)
{
#ifndef CROSS_DEBUGGER
	struct kthr *kt;
	struct pcb pcb;
	int i, reg;

	kt = kgdb_thr_lookup_tid(ptid_get_pid(inferior_ptid));
	if (kt == NULL)
		return;
	if (kvm_read(kvm, kt->pcb, &pcb, sizeof(pcb)) != sizeof(pcb)) {
		warnx("kvm_read: %s", kvm_geterr(kvm));
		memset(&pcb, 0, sizeof(pcb));
	}
	for (i = ARM_A1_REGNUM + 8; i <= ARM_SP_REGNUM; i++) {
		supply_register(i, (char *)&pcb.un_32.pcb32_r8 +
		    (i - (ARM_A1_REGNUM + 8 )) * 4);
	}
	if (pcb.un_32.pcb32_sp != 0) {
		for (i = 0; i < 4; i++) {
			if (kvm_read(kvm, pcb.un_32.pcb32_sp + (i) * 4,
			    &reg, 4) != 4) {
				warnx("kvm_read: %s", kvm_geterr(kvm));
				break;
			}
			supply_register(ARM_A1_REGNUM + 4 + i, (char *)&reg);
		}
		if (kvm_read(kvm, pcb.un_32.pcb32_sp + 4 * 4, &reg, 4) != 4)
			warnx("kvm_read :%s", kvm_geterr(kvm));
		else
			supply_register(ARM_PC_REGNUM, (char *)&reg);
	}
#endif
}

void
kgdb_trgt_store_registers(int regno __unused)
{
	fprintf_unfiltered(gdb_stderr, "XXX: %s\n", __func__);
}

void
kgdb_trgt_new_objfile(struct objfile *objfile)
{
}

#ifndef CROSS_DEBUGGER
struct kgdb_frame_cache {
	CORE_ADDR	fp;
	CORE_ADDR	sp;
};

static int kgdb_trgt_frame_offset[26] = {
	offsetof(struct trapframe, tf_r0),
	offsetof(struct trapframe, tf_r1),
	offsetof(struct trapframe, tf_r2),
	offsetof(struct trapframe, tf_r3),
	offsetof(struct trapframe, tf_r4),
	offsetof(struct trapframe, tf_r5),
	offsetof(struct trapframe, tf_r6),
	offsetof(struct trapframe, tf_r7),
	offsetof(struct trapframe, tf_r8),
	offsetof(struct trapframe, tf_r9),
	offsetof(struct trapframe, tf_r10),
	offsetof(struct trapframe, tf_r11),
	offsetof(struct trapframe, tf_r12),
	offsetof(struct trapframe, tf_svc_sp),
	offsetof(struct trapframe, tf_svc_lr),
	offsetof(struct trapframe, tf_pc),
	-1, -1, -1, -1, -1, -1, -1, -1, -1,
	offsetof(struct trapframe, tf_spsr)
};

static struct kgdb_frame_cache *
kgdb_trgt_frame_cache(struct frame_info *next_frame, void **this_cache)
{
	char buf[MAX_REGISTER_SIZE];
	struct kgdb_frame_cache *cache;

	cache = *this_cache;
	if (cache == NULL) {
		cache = FRAME_OBSTACK_ZALLOC(struct kgdb_frame_cache);
		*this_cache = cache;
		frame_unwind_register(next_frame, ARM_SP_REGNUM, buf);
		cache->sp = extract_unsigned_integer(buf,
		    register_size(current_gdbarch, ARM_SP_REGNUM));
		frame_unwind_register(next_frame, ARM_FP_REGNUM, buf);
		cache->fp = extract_unsigned_integer(buf,
		    register_size(current_gdbarch, ARM_FP_REGNUM));
	}
	return (cache);
}

static int is_undef;

static void
kgdb_trgt_trapframe_this_id(struct frame_info *next_frame, void **this_cache,
    struct frame_id *this_id)
{
	struct kgdb_frame_cache *cache;

	cache = kgdb_trgt_frame_cache(next_frame, this_cache);
	*this_id = frame_id_build(cache->fp, 0);
}

static void
kgdb_trgt_trapframe_prev_register(struct frame_info *next_frame,
    void **this_cache, int regnum, int *optimizedp, enum lval_type *lvalp,
    CORE_ADDR *addrp, int *realnump, void *valuep)
{
	char dummy_valuep[MAX_REGISTER_SIZE];
	struct kgdb_frame_cache *cache;
	int ofs, regsz;
	int is_undefined = 0;

	regsz = register_size(current_gdbarch, regnum);

	if (valuep == NULL)
		valuep = dummy_valuep;
	memset(valuep, 0, regsz);
	*optimizedp = 0;
	*addrp = 0;
	*lvalp = not_lval;
	*realnump = -1;

	ofs = (regnum >= 0 && regnum <= ARM_PS_REGNUM)
	    ? kgdb_trgt_frame_offset[regnum] : -1;
	if (ofs == -1)
		return;

	cache = kgdb_trgt_frame_cache(next_frame, this_cache);

	if (is_undef && (regnum == ARM_SP_REGNUM || regnum == ARM_PC_REGNUM)) {
		*addrp = cache->sp + offsetof(struct trapframe, tf_spsr);
		target_read_memory(*addrp, valuep, regsz);
		is_undefined = 1;
		ofs = kgdb_trgt_frame_offset[ARM_SP_REGNUM];

	}
	*addrp = cache->sp + ofs;
	*lvalp = lval_memory;
	target_read_memory(*addrp, valuep, regsz);

	if (is_undefined) {
		*addrp = *(unsigned int *)valuep + (regnum == ARM_SP_REGNUM ?
		    0 : 8);
		target_read_memory(*addrp, valuep, regsz);

	}
}

static const struct frame_unwind kgdb_trgt_trapframe_unwind = {
        UNKNOWN_FRAME,
        &kgdb_trgt_trapframe_this_id,
        &kgdb_trgt_trapframe_prev_register
};
#endif

const struct frame_unwind *
kgdb_trgt_trapframe_sniffer(struct frame_info *next_frame)
{
#ifndef CROSS_DEBUGGER
	char *pname;
	CORE_ADDR pc;

	pc = frame_pc_unwind(next_frame);
	pname = NULL;
	find_pc_partial_function(pc, &pname, NULL, NULL);
	if (pname == NULL) {
		is_undef = 0;
		return (NULL);
	}
	if (!strcmp(pname, "undefinedinstruction"))
		is_undef = 1;
	if (strcmp(pname, "Laddress_exception_entry") == 0 ||
	    strcmp(pname, "undefined_entry") == 0 ||
	    strcmp(pname, "exception_exit") == 0 ||
	    strcmp(pname, "Laddress_exception_msg") == 0 ||
	    strcmp(pname, "irq_entry") == 0)
		return (&kgdb_trgt_trapframe_unwind);
	if (!strcmp(pname, "undefinedinstruction"))
		is_undef = 1;
	else
		is_undef = 0;
#endif
	return (NULL);
}
