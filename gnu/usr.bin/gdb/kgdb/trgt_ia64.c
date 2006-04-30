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
#include <machine/frame.h>
#include <machine/md_var.h>
#include <machine/pcb.h>
#include <err.h>
#include <kvm.h>
#include <string.h>

#include <defs.h>
#include <target.h>
#include <gdbthread.h>
#include <inferior.h>
#include <regcache.h>
#include <frame-unwind.h>
#include <ia64-tdep.h>

#include "kgdb.h"

void
kgdb_trgt_fetch_registers(int regno __unused)
{
	struct kthr *kt;
	struct pcb pcb;
	uint64_t r;

	kt = kgdb_thr_lookup_tid(ptid_get_tid(inferior_ptid));
	if (kt == NULL)
		return;
	if (kvm_read(kvm, kt->pcb, &pcb, sizeof(pcb)) != sizeof(pcb)) {
		warnx("kvm_read: %s", kvm_geterr(kvm));
		memset(&pcb, 0, sizeof(pcb));
	}

	/* Registers 0-127: general registers. */
	supply_register(IA64_GR1_REGNUM, (char *)&pcb.pcb_special.gp);
	supply_register(IA64_GR4_REGNUM, (char *)&pcb.pcb_preserved.gr4);
	supply_register(IA64_GR5_REGNUM, (char *)&pcb.pcb_preserved.gr5);
	supply_register(IA64_GR6_REGNUM, (char *)&pcb.pcb_preserved.gr6);
	supply_register(IA64_GR7_REGNUM, (char *)&pcb.pcb_preserved.gr7);
	supply_register(IA64_GR12_REGNUM, (char *)&pcb.pcb_special.sp);
	supply_register(IA64_GR12_REGNUM+1, (char *)&pcb.pcb_special.tp);

	/* Registers 128-255: floating-point registers. */
	supply_register(IA64_FR2_REGNUM, (char *)&pcb.pcb_preserved_fp.fr2);
	supply_register(IA64_FR2_REGNUM+1, (char *)&pcb.pcb_preserved_fp.fr3);
	supply_register(IA64_FR2_REGNUM+2, (char *)&pcb.pcb_preserved_fp.fr4);
	supply_register(IA64_FR2_REGNUM+3, (char *)&pcb.pcb_preserved_fp.fr5);
	supply_register(IA64_FR16_REGNUM, (char *)&pcb.pcb_preserved_fp.fr16);
	supply_register(IA64_FR16_REGNUM+1, (char*)&pcb.pcb_preserved_fp.fr17);
	supply_register(IA64_FR16_REGNUM+2, (char*)&pcb.pcb_preserved_fp.fr18);
	supply_register(IA64_FR16_REGNUM+3, (char*)&pcb.pcb_preserved_fp.fr19);
	supply_register(IA64_FR16_REGNUM+4, (char*)&pcb.pcb_preserved_fp.fr20);
	supply_register(IA64_FR16_REGNUM+5, (char*)&pcb.pcb_preserved_fp.fr21);
	supply_register(IA64_FR16_REGNUM+6, (char*)&pcb.pcb_preserved_fp.fr22);
	supply_register(IA64_FR16_REGNUM+7, (char*)&pcb.pcb_preserved_fp.fr23);
	supply_register(IA64_FR16_REGNUM+8, (char*)&pcb.pcb_preserved_fp.fr24);
	supply_register(IA64_FR16_REGNUM+9, (char*)&pcb.pcb_preserved_fp.fr25);
	supply_register(IA64_FR16_REGNUM+10,(char*)&pcb.pcb_preserved_fp.fr26);
	supply_register(IA64_FR16_REGNUM+11,(char*)&pcb.pcb_preserved_fp.fr27);
	supply_register(IA64_FR16_REGNUM+12,(char*)&pcb.pcb_preserved_fp.fr28);
	supply_register(IA64_FR16_REGNUM+13,(char*)&pcb.pcb_preserved_fp.fr29);
	supply_register(IA64_FR16_REGNUM+14,(char*)&pcb.pcb_preserved_fp.fr30);
	supply_register(IA64_FR16_REGNUM+15,(char*)&pcb.pcb_preserved_fp.fr31);

	/* Registers 320-327: branch registers. */
	if (pcb.pcb_special.__spare == ~0UL)
		supply_register(IA64_BR0_REGNUM, (char *)&pcb.pcb_special.rp);
	supply_register(IA64_BR1_REGNUM, (char *)&pcb.pcb_preserved.br1);
	supply_register(IA64_BR2_REGNUM, (char *)&pcb.pcb_preserved.br2);
	supply_register(IA64_BR3_REGNUM, (char *)&pcb.pcb_preserved.br3);
	supply_register(IA64_BR4_REGNUM, (char *)&pcb.pcb_preserved.br4);
	supply_register(IA64_BR5_REGNUM, (char *)&pcb.pcb_preserved.br5);

	/* Registers 328-333: misc. other registers. */
	supply_register(IA64_PR_REGNUM, (char *)&pcb.pcb_special.pr);
	if (pcb.pcb_special.__spare == ~0UL) {
		r = pcb.pcb_special.iip + ((pcb.pcb_special.psr >> 41) & 3);
		supply_register(IA64_IP_REGNUM, (char *)&r);
		supply_register(IA64_CFM_REGNUM, (char *)&pcb.pcb_special.cfm);
	} else {
		supply_register(IA64_IP_REGNUM, (char *)&pcb.pcb_special.rp);
		supply_register(IA64_CFM_REGNUM, (char *)&pcb.pcb_special.pfs);
	}

	/* Registers 334-461: application registers. */
	supply_register(IA64_RSC_REGNUM, (char *)&pcb.pcb_special.rsc);
	r = pcb.pcb_special.bspstore;
	if (pcb.pcb_special.__spare == ~0UL)
		r += pcb.pcb_special.ndirty;
	else
		r = ia64_bsp_adjust(r, IA64_CFM_SOF(pcb.pcb_special.pfs) -
		    IA64_CFM_SOL(pcb.pcb_special.pfs));
	supply_register(IA64_BSP_REGNUM, (char *)&r);
	supply_register(IA64_BSPSTORE_REGNUM, (char *)&r);
	supply_register(IA64_RNAT_REGNUM, (char *)&pcb.pcb_special.rnat);
	supply_register(IA64_UNAT_REGNUM, (char *)&pcb.pcb_special.unat);
	supply_register(IA64_FPSR_REGNUM, (char *)&pcb.pcb_special.fpsr);
	if (pcb.pcb_special.__spare == ~0UL)
		supply_register(IA64_PFS_REGNUM, (char *)&pcb.pcb_special.pfs);
	supply_register(IA64_LC_REGNUM, (char *)&pcb.pcb_preserved.lc);
}

void
kgdb_trgt_store_registers(int regno __unused)
{
	fprintf_unfiltered(gdb_stderr, "XXX: %s\n", __func__);
}

struct kgdb_frame_cache {
	CORE_ADDR	bsp;
	CORE_ADDR	ip;
	CORE_ADDR	sp;
	CORE_ADDR	saved_bsp;
};

#define	SPECIAL(x)	offsetof(struct trapframe, tf_special)		\
			+ offsetof(struct _special, x)
#define	SCRATCH(x)	offsetof(struct trapframe, tf_scratch)		\
			+ offsetof(struct _caller_saved, x)
#define	SCRATCH_FP(x)	offsetof(struct trapframe, tf_scratch_fp)	\
			+ offsetof(struct _caller_saved_fp, x)

static int kgdb_trgt_frame_ofs_gr[32] = {
	-1,					/* gr0 */
	SPECIAL(gp),
	SCRATCH(gr2),   SCRATCH(gr3),
	-1, -1, -1, -1,				/* gr4-gr7 */
	SCRATCH(gr8),   SCRATCH(gr9),   SCRATCH(gr10),  SCRATCH(gr11),
	SPECIAL(sp),    SPECIAL(tp),
	SCRATCH(gr14),  SCRATCH(gr15),  SCRATCH(gr16),  SCRATCH(gr17),
	SCRATCH(gr18),  SCRATCH(gr19),  SCRATCH(gr20),  SCRATCH(gr21),
	SCRATCH(gr22),  SCRATCH(gr23),  SCRATCH(gr24),  SCRATCH(gr25),
	SCRATCH(gr26),  SCRATCH(gr27),  SCRATCH(gr28),  SCRATCH(gr29),
	SCRATCH(gr30),  SCRATCH(gr31)
};

static int kgdb_trgt_frame_ofs_fr[32] = {
	-1,					/* fr0: constant 0.0 */
	-1,					/* fr1: constant 1.0 */
	-1, -1, -1, -1,				/* fr2-fr5 */
	SCRATCH_FP(fr6), SCRATCH_FP(fr7), SCRATCH_FP(fr8), SCRATCH_FP(fr9),
	SCRATCH_FP(fr10), SCRATCH_FP(fr11), SCRATCH_FP(fr12), SCRATCH_FP(fr13),
	SCRATCH_FP(fr14), SCRATCH_FP(fr15)
};

static int kgdb_trgt_frame_ofs_br[8] = {
        SPECIAL(rp),
	-1, -1, -1, -1, -1,			/* br1-br5 */
	SCRATCH(br6), SCRATCH(br7)
};

static int kgdb_trgt_frame_ofs_ar[49] = {
	/* ar0-ar15 */
	SPECIAL(rsc),
	-1,					/* ar.bsp */
	SPECIAL(bspstore), SPECIAL(rnat),
	-1, -1, -1, -1, -1,			/* ar20-ar24 */
	SCRATCH(csd), SCRATCH(ssd),
	-1, -1, -1, -1, -1,			/* ar27-ar31 */
	SCRATCH(ccv),
	-1, -1, -1,				/* ar33-ar35 */
	SPECIAL(unat),
	-1, -1, -1,				/* ar37-ar39 */
	SPECIAL(fpsr),
	-1, -1, -1, -1, -1, -1, -1,		/* ar41-ar47 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* ar48-ar55 */
	-1, -1, -1, -1, -1, -1, -1, -1,		/* ar56-ar63 */
	SPECIAL(pfs)
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
		frame_unwind_register(next_frame, IA64_BSP_REGNUM, buf);
		cache->bsp = extract_unsigned_integer(buf,
		    register_size(current_gdbarch, IA64_BSP_REGNUM));
		cache->ip = frame_func_unwind(next_frame);
		frame_unwind_register(next_frame, SP_REGNUM, buf);
		cache->sp = extract_unsigned_integer(buf,
		    register_size(current_gdbarch, SP_REGNUM));
	}
	return (cache);
}

static void
kgdb_trgt_trapframe_this_id(struct frame_info *next_frame, void **this_cache,
    struct frame_id *this_id)
{
	struct kgdb_frame_cache *cache;

	cache = kgdb_trgt_frame_cache(next_frame, this_cache);
	*this_id = frame_id_build_special(cache->sp, cache->ip, cache->bsp);
}

static void
kgdb_trgt_trapframe_prev_register(struct frame_info *next_frame,
    void **this_cache, int regnum, int *optimizedp, enum lval_type *lvalp,
    CORE_ADDR *addrp, int *realnump, void *valuep)
{
	char buf[MAX_REGISTER_SIZE];
	char dummy_valuep[MAX_REGISTER_SIZE];
	struct kgdb_frame_cache *cache;
	CORE_ADDR bsp;
	int ofs, regsz;

	regsz = register_size(current_gdbarch, regnum);

	if (valuep == NULL)
		valuep = dummy_valuep;
	memset(valuep, 0, regsz);
	*optimizedp = 0;
	*addrp = 0;
	*lvalp = not_lval;
	*realnump = -1;

	cache = kgdb_trgt_frame_cache(next_frame, this_cache);

	if (regnum == IA64_BSP_REGNUM) {
		if (cache->saved_bsp == 0) {
			target_read_memory(cache->sp + 16 + SPECIAL(bspstore),
			    buf, regsz);
			bsp = extract_unsigned_integer(buf, regsz);
			target_read_memory(cache->sp + 16 + SPECIAL(ndirty),
			    buf, regsz);
			bsp += extract_unsigned_integer(buf, regsz);
			cache->saved_bsp = bsp;
		}
		store_unsigned_integer(valuep, regsz, cache->saved_bsp);
		return;
	}
	if (regnum == IA64_PR_REGNUM)
		ofs = SPECIAL(pr);
	else if (regnum == IA64_IP_REGNUM)
		ofs = SPECIAL(iip);
	else if (regnum == IA64_PSR_REGNUM)
		ofs = SPECIAL(psr);
	else if (regnum == IA64_CFM_REGNUM)
		ofs = SPECIAL(cfm);
	else if (regnum >= IA64_GR0_REGNUM && regnum <= IA64_GR31_REGNUM)
		ofs = kgdb_trgt_frame_ofs_gr[regnum - IA64_GR0_REGNUM];
	else if (regnum >= IA64_FR0_REGNUM && regnum <= IA64_FR15_REGNUM)
		ofs = kgdb_trgt_frame_ofs_fr[regnum - IA64_FR0_REGNUM];
	else if (regnum >= IA64_BR0_REGNUM && regnum <= IA64_BR7_REGNUM)
		ofs = kgdb_trgt_frame_ofs_br[regnum - IA64_BR0_REGNUM];
	else if (regnum >= IA64_RSC_REGNUM && regnum <= IA64_PFS_REGNUM)
		ofs = kgdb_trgt_frame_ofs_ar[regnum - IA64_RSC_REGNUM];
	else
		ofs = -1;
	if (ofs == -1)
		return;

	*addrp = cache->sp + 16 + ofs;
	*lvalp = lval_memory;
	target_read_memory(*addrp, valuep, regsz);
}

static const struct frame_unwind kgdb_trgt_trapframe_unwind = {
        UNKNOWN_FRAME,
        &kgdb_trgt_trapframe_this_id,
        &kgdb_trgt_trapframe_prev_register
};

const struct frame_unwind *
kgdb_trgt_trapframe_sniffer(struct frame_info *next_frame)
{
	char *pname;
	CORE_ADDR ip;

	ip = frame_func_unwind(next_frame);
	pname = NULL;
	find_pc_partial_function(ip, &pname, NULL, NULL);
	if (pname == NULL)
		return (NULL);
	if (strncmp(pname, "ivt_", 4) == 0)
		return (&kgdb_trgt_trapframe_unwind);
	/* printf("%s: %lx =%s\n", __func__, ip, pname); */
	return (NULL);
}
