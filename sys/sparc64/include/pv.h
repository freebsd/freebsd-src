/*-
 * Copyright (c) 1997 Berkeley Software Design, Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Berkeley Software Design Inc's name may not be used to endorse or
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BERKELEY SOFTWARE DESIGN INC ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL BERKELEY SOFTWARE DESIGN INC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	from BSDI: pmap.c,v 1.28.2.15 2000/04/27 03:10:31 cp Exp
 * $FreeBSD$
 */

#ifndef	_MACHINE_PV_H_
#define	_MACHINE_PV_H_

#define	PV_LOCK()
#define	PV_UNLOCK()

#define	ST_TTE			offsetof(struct stte, st_tte)
#define	ST_NEXT			offsetof(struct stte, st_next)
#define	ST_PREV			offsetof(struct stte, st_prev)

#define	TTE_DATA		offsetof(struct tte, tte_data)
#define	TTE_TAG			offsetof(struct tte, tte_tag)

#define	PVH_FIRST		offsetof(struct pv_head, pvh_first)

#define	PV_OFF(pa)		((vm_offset_t)(pa) - avail_start)
#define	PV_INDEX(pa)		(PV_OFF(pa) >> PAGE_SHIFT)
#define	PV_SHIFT		(3)

#define	casxp(pa, exp, src) \
	casxa((vm_offset_t *)pa, exp, src, ASI_PHYS_USE_EC)
#define	ldxp(pa)		ldxa(pa, ASI_PHYS_USE_EC)
#define	stxp(pa, val)		stxa(pa, ASI_PHYS_USE_EC, val)

struct pv_entry {
	vm_offset_t pv_next;
	vm_offset_t pv_prev;
};

struct pv_head {
	vm_offset_t pvh_first;
};

extern vm_offset_t pv_table;
extern u_long pv_generation;

static __inline vm_offset_t
pv_lookup(vm_offset_t pa)
{
	return (pv_table + (PV_INDEX(pa) << PV_SHIFT));
}

static __inline vm_offset_t
pvh_get_first(vm_offset_t pvh)
{
	return (ldxp(pvh + PVH_FIRST));
}

static __inline vm_offset_t
pv_get_next(vm_offset_t pstp)
{
	return (ldxp(pstp + ST_NEXT));
}

static __inline vm_offset_t
pv_get_prev(vm_offset_t pstp)
{
	return (ldxp(pstp + ST_PREV));
}

static __inline u_long
pv_get_tte_data(vm_offset_t pstp)
{
	return (ldxp(pstp + ST_TTE + TTE_DATA));
}

static __inline u_long
pv_get_tte_tag(vm_offset_t pstp)
{
	return (ldxp(pstp + ST_TTE + TTE_TAG));
}

#define	pv_get_tte(pstp) ({ \
	struct tte __tte; \
	__tte.tte_tag = pv_get_tte_tag(pstp); \
	__tte.tte_data = pv_get_tte_data(pstp); \
	__tte; \
})

static __inline void
pvh_set_first(vm_offset_t pvh, vm_offset_t first)
{
	stxp(pvh + PVH_FIRST, first);
}

static __inline void
pv_set_next(vm_offset_t pstp, vm_offset_t next)
{
	stxp(pstp + ST_NEXT, next);
}

static __inline void
pv_set_prev(vm_offset_t pstp, vm_offset_t prev)
{
	stxp(pstp + ST_PREV, prev);
}

static __inline void
pv_remove_phys(vm_offset_t pstp)
{
	vm_offset_t next;
	vm_offset_t prev;

	next = pv_get_next(pstp);
	prev = pv_get_prev(pstp);
	if (next != 0)
		pv_set_next(next, prev);
	stxp(prev, next);
}

static __inline void
pv_atomic_bit_clear(vm_offset_t pstp, u_long bits)
{
	vm_offset_t dp;
	vm_offset_t d1;
	vm_offset_t d2;
	vm_offset_t d3;

	dp = pstp + ST_TTE + TTE_DATA;
	for (d1 = ldxp(dp);; d1 = d3) {
		d2 = d1 & ~bits;
		d3 = casxp(dp, d1, d2);
		if (d1 == d3)
			break;
	}
}

static __inline void
pv_atomic_bit_set(vm_offset_t pstp, u_long bits)
{
	vm_offset_t dp;
	vm_offset_t d1;
	vm_offset_t d2;
	vm_offset_t d3;

	dp = pstp + ST_TTE + TTE_DATA;
	for (d1 = ldxp(dp);; d1 = d3) {
		d2 = d1 | bits;
		d3 = casxp(dp, d1, d2);
		if (d1 == d3)
			break;
	}
}

static __inline int
pv_atomic_bit_test(vm_offset_t pstp, u_long bits)
{
	vm_offset_t dp;

	dp = pstp + ST_TTE + TTE_DATA;
	return ((casxp(dp, 0, 0) & bits) != 0);
}

void pv_dump(vm_offset_t pvh);

void pv_insert(pmap_t pm, vm_offset_t pa, vm_offset_t va, struct stte *stp);
void pv_remove_virt(struct stte *stp);

void pv_bit_clear(vm_page_t m, u_long bits);
int pv_bit_count(vm_page_t m, u_long bits);
void pv_bit_set(vm_page_t m, u_long bits);
int pv_bit_test(vm_page_t m, u_long bits);

void pv_global_remove_all(vm_page_t m);

#endif /* !_MACHINE_PV_H_ */
