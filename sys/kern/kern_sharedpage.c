/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2010, 2012 Konstantin Belousov <kib@FreeBSD.org>
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Konstantin Belousov
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
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

#include <sys/cdefs.h>
#include "opt_vm.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/rwlock.h>
#include <sys/stddef.h>
#include <sys/sysent.h>
#include <sys/sysctl.h>
#include <sys/vdso.h>

#include <vm/vm.h>
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>

static struct sx shared_page_alloc_sx;
static vm_object_t shared_page_obj;
static int shared_page_free;
char *shared_page_mapping;

#ifdef RANDOM_FENESTRASX
static struct vdso_fxrng_generation *fxrng_shpage_mapping;

static bool fxrng_enabled = true;
SYSCTL_BOOL(_debug, OID_AUTO, fxrng_vdso_enable, CTLFLAG_RWTUN, &fxrng_enabled,
    0, "Enable FXRNG VDSO");
#endif

void
shared_page_write(int base, int size, const void *data)
{

	bcopy(data, shared_page_mapping + base, size);
}

static int
shared_page_alloc_locked(int size, int align)
{
	int res;

	res = roundup(shared_page_free, align);
	if (res + size >= IDX_TO_OFF(shared_page_obj->size))
		res = -1;
	else
		shared_page_free = res + size;
	return (res);
}

int
shared_page_alloc(int size, int align)
{
	int res;

	sx_xlock(&shared_page_alloc_sx);
	res = shared_page_alloc_locked(size, align);
	sx_xunlock(&shared_page_alloc_sx);
	return (res);
}

int
shared_page_fill(int size, int align, const void *data)
{
	int res;

	sx_xlock(&shared_page_alloc_sx);
	res = shared_page_alloc_locked(size, align);
	if (res != -1)
		shared_page_write(res, size, data);
	sx_xunlock(&shared_page_alloc_sx);
	return (res);
}

static void
shared_page_init(void *dummy __unused)
{
	vm_page_t m;
	vm_offset_t addr;

	sx_init(&shared_page_alloc_sx, "shpsx");
	shared_page_obj = vm_pager_allocate(OBJT_PHYS, 0, PAGE_SIZE,
	    VM_PROT_DEFAULT, 0, NULL);
	VM_OBJECT_WLOCK(shared_page_obj);
	m = vm_page_grab(shared_page_obj, 0, VM_ALLOC_ZERO);
	VM_OBJECT_WUNLOCK(shared_page_obj);
	vm_page_valid(m);
	vm_page_xunbusy(m);
	addr = kva_alloc(PAGE_SIZE);
	pmap_qenter(addr, &m, 1);
	shared_page_mapping = (char *)addr;
}

SYSINIT(shp, SI_SUB_EXEC, SI_ORDER_FIRST, (sysinit_cfunc_t)shared_page_init,
    NULL);

/*
 * Push the timehands update to the shared page.
 *
 * The lockless update scheme is similar to the one used to update the
 * in-kernel timehands, see sys/kern/kern_tc.c:tc_windup() (which
 * calls us after the timehands are updated).
 */
static void
timehands_update(struct vdso_sv_tk *svtk)
{
	struct vdso_timehands th;
	struct vdso_timekeep *tk;
	uint32_t enabled, idx;

	enabled = tc_fill_vdso_timehands(&th);
	th.th_gen = 0;
	idx = svtk->sv_timekeep_curr;
	if (++idx >= VDSO_TH_NUM)
		idx = 0;
	svtk->sv_timekeep_curr = idx;
	if (++svtk->sv_timekeep_gen == 0)
		svtk->sv_timekeep_gen = 1;

	tk = (struct vdso_timekeep *)(shared_page_mapping +
	    svtk->sv_timekeep_off);
	tk->tk_th[idx].th_gen = 0;
	atomic_thread_fence_rel();
	if (enabled)
		tk->tk_th[idx] = th;
	atomic_store_rel_32(&tk->tk_th[idx].th_gen, svtk->sv_timekeep_gen);
	atomic_store_rel_32(&tk->tk_current, idx);

	/*
	 * The ordering of the assignment to tk_enabled relative to
	 * the update of the vdso_timehands is not important.
	 */
	tk->tk_enabled = enabled;
}

#ifdef COMPAT_FREEBSD32
static void
timehands_update32(struct vdso_sv_tk *svtk)
{
	struct vdso_timehands32 th;
	struct vdso_timekeep32 *tk;
	uint32_t enabled, idx;

	enabled = tc_fill_vdso_timehands32(&th);
	th.th_gen = 0;
	idx = svtk->sv_timekeep_curr;
	if (++idx >= VDSO_TH_NUM)
		idx = 0;
	svtk->sv_timekeep_curr = idx;
	if (++svtk->sv_timekeep_gen == 0)
		svtk->sv_timekeep_gen = 1;

	tk = (struct vdso_timekeep32 *)(shared_page_mapping +
	    svtk->sv_timekeep_off);
	tk->tk_th[idx].th_gen = 0;
	atomic_thread_fence_rel();
	if (enabled)
		tk->tk_th[idx] = th;
	atomic_store_rel_32(&tk->tk_th[idx].th_gen, svtk->sv_timekeep_gen);
	atomic_store_rel_32(&tk->tk_current, idx);
	tk->tk_enabled = enabled;
}
#endif

/*
 * This is hackish, but easiest way to avoid creating list structures
 * that needs to be iterated over from the hardclock interrupt
 * context.
 */
static struct vdso_sv_tk *host_svtk;
#ifdef COMPAT_FREEBSD32
static struct vdso_sv_tk *compat32_svtk;
#endif

void
timekeep_push_vdso(void)
{

	if (host_svtk != NULL)
		timehands_update(host_svtk);
#ifdef COMPAT_FREEBSD32
	if (compat32_svtk != NULL)
		timehands_update32(compat32_svtk);
#endif
}

struct vdso_sv_tk *
alloc_sv_tk(void)
{
	struct vdso_sv_tk *svtk;
	int tk_base;
	uint32_t tk_ver;

	tk_ver = VDSO_TK_VER_CURR;
	svtk = malloc(sizeof(struct vdso_sv_tk), M_TEMP, M_WAITOK | M_ZERO);
	tk_base = shared_page_alloc(sizeof(struct vdso_timekeep) +
	    sizeof(struct vdso_timehands) * VDSO_TH_NUM, 16);
	KASSERT(tk_base != -1, ("tk_base -1 for native"));
	shared_page_write(tk_base + offsetof(struct vdso_timekeep, tk_ver),
	    sizeof(uint32_t), &tk_ver);
	svtk->sv_timekeep_off = tk_base;
	timekeep_push_vdso();
	return (svtk);
}

#ifdef COMPAT_FREEBSD32
struct vdso_sv_tk *
alloc_sv_tk_compat32(void)
{
	struct vdso_sv_tk *svtk;
	int tk_base;
	uint32_t tk_ver;

	svtk = malloc(sizeof(struct vdso_sv_tk), M_TEMP, M_WAITOK | M_ZERO);
	tk_ver = VDSO_TK_VER_CURR;
	tk_base = shared_page_alloc(sizeof(struct vdso_timekeep32) +
	    sizeof(struct vdso_timehands32) * VDSO_TH_NUM, 16);
	KASSERT(tk_base != -1, ("tk_base -1 for 32bit"));
	shared_page_write(tk_base + offsetof(struct vdso_timekeep32,
	    tk_ver), sizeof(uint32_t), &tk_ver);
	svtk->sv_timekeep_off = tk_base;
	timekeep_push_vdso();
	return (svtk);
}
#endif

#ifdef RANDOM_FENESTRASX
void
fxrng_push_seed_generation(uint64_t gen)
{
	if (fxrng_shpage_mapping == NULL || !fxrng_enabled)
		return;
	KASSERT(gen < INT32_MAX,
	    ("fxrng seed version shouldn't roll over a 32-bit counter "
	     "for approximately 456,000 years"));
	atomic_store_rel_32(&fxrng_shpage_mapping->fx_generation32,
	    (uint32_t)gen);
}

static void
alloc_sv_fxrng_generation(void)
{
	int base;

	/*
	 * Allocate a full cache line for the fxrng root generation (64-bit
	 * counter, or truncated 32-bit counter on ILP32 userspace).  It is
	 * important that the line is not shared with frequently dirtied data,
	 * and the shared page allocator lacks a __read_mostly mechanism.
	 * However, PAGE_SIZE is typically large relative to the amount of
	 * stuff we've got in it so far, so maybe the possible waste isn't an
	 * issue.
	 */
	base = shared_page_alloc(CACHE_LINE_SIZE, CACHE_LINE_SIZE);
	KASSERT(base != -1, ("%s: base allocation failed", __func__));
	fxrng_shpage_mapping = (void *)(shared_page_mapping + base);
	*fxrng_shpage_mapping = (struct vdso_fxrng_generation) {
		.fx_vdso_version = VDSO_FXRNG_VER_CURR,
	};
}
#endif /* RANDOM_FENESTRASX */

void
exec_sysvec_init(void *param)
{
	struct sysentvec *sv;
	u_int flags;
	int res;

	sv = param;
	flags = sv->sv_flags;
	if ((flags & SV_SHP) == 0)
		return;
	MPASS(sv->sv_shared_page_obj == NULL);
	MPASS(sv->sv_shared_page_base != 0);

	sv->sv_shared_page_obj = shared_page_obj;
	if ((flags & SV_ABI_MASK) == SV_ABI_FREEBSD) {
		if ((flags & SV_DSO_SIG) != 0) {
			res = shared_page_fill((uintptr_t)sv->sv_szsigcode,
			    16, sv->sv_sigcode);
			if (res == -1)
				panic("copying vdso to shared page");
			sv->sv_vdso_offset = res;
			sv->sv_sigcode_offset = res + sv->sv_sigcodeoff;
		} else {
			res = shared_page_fill(*(sv->sv_szsigcode),
			    16, sv->sv_sigcode);
			if (res == -1)
				panic("copying sigtramp to shared page");
			sv->sv_sigcode_offset = res;
		}
	}
	if ((flags & SV_TIMEKEEP) != 0) {
#ifdef COMPAT_FREEBSD32
		if ((flags & SV_ILP32) != 0) {
			if ((flags & SV_ABI_MASK) == SV_ABI_FREEBSD) {
				KASSERT(compat32_svtk == NULL,
				    ("Compat32 already registered"));
				compat32_svtk = alloc_sv_tk_compat32();
			} else {
				KASSERT(compat32_svtk != NULL,
				    ("Compat32 not registered"));
			}
			sv->sv_timekeep_offset = compat32_svtk->sv_timekeep_off;
		} else {
#endif
			if ((flags & SV_ABI_MASK) == SV_ABI_FREEBSD) {
				KASSERT(host_svtk == NULL,
				    ("Host already registered"));
				host_svtk = alloc_sv_tk();
			} else {
				KASSERT(host_svtk != NULL,
				    ("Host not registered"));
			}
			sv->sv_timekeep_offset = host_svtk->sv_timekeep_off;
#ifdef COMPAT_FREEBSD32
		}
#endif
	}
#ifdef RANDOM_FENESTRASX
	if ((flags & (SV_ABI_MASK | SV_RNG_SEED_VER)) ==
	    (SV_ABI_FREEBSD | SV_RNG_SEED_VER)) {
		/*
		 * Only allocate a single VDSO entry for multiple sysentvecs,
		 * i.e., native and COMPAT32.
		 */
		if (fxrng_shpage_mapping == NULL)
			alloc_sv_fxrng_generation();
		sv->sv_fxrng_gen_offset =
		    (char *)fxrng_shpage_mapping - shared_page_mapping;
	}
#endif
}

void
exec_sysvec_init_secondary(struct sysentvec *sv, struct sysentvec *sv2)
{
	MPASS((sv2->sv_flags & SV_ABI_MASK) == (sv->sv_flags & SV_ABI_MASK));
	MPASS((sv2->sv_flags & SV_TIMEKEEP) == (sv->sv_flags & SV_TIMEKEEP));
	MPASS((sv2->sv_flags & SV_SHP) != 0 && (sv->sv_flags & SV_SHP) != 0);
	MPASS((sv2->sv_flags & SV_DSO_SIG) == (sv->sv_flags & SV_DSO_SIG));
	MPASS((sv2->sv_flags & SV_RNG_SEED_VER) ==
	    (sv->sv_flags & SV_RNG_SEED_VER));

	sv2->sv_shared_page_obj = sv->sv_shared_page_obj;
	sv2->sv_sigcode_offset = sv->sv_sigcode_offset;
	sv2->sv_vdso_offset = sv->sv_vdso_offset;
	if ((sv2->sv_flags & SV_ABI_MASK) != SV_ABI_FREEBSD)
		return;
	sv2->sv_timekeep_offset = sv->sv_timekeep_offset;
	sv2->sv_fxrng_gen_offset = sv->sv_fxrng_gen_offset;
}
