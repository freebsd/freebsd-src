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
 *	from: BSDI: pmap.v9.h,v 1.10.2.6 1999/08/23 22:18:44 cp Exp
 * $FreeBSD$
 */

#ifndef	_MACHINE_TSB_H_
#define	_MACHINE_TSB_H_

#define	TSB_USER_MIN_ADDRESS		(UPT_MIN_ADDRESS)

#define	TSB_MASK_WIDTH			(6)

#define	TSB_PRIMARY_BUCKET_SHIFT	(2)
#define	TSB_PRIMARY_BUCKET_SIZE		(1 << TSB_PRIMARY_BUCKET_SHIFT)
#define	TSB_PRIMARY_BUCKET_MASK		(TSB_PRIMARY_BUCKET_SIZE - 1)
#define	TSB_SECONDARY_BUCKET_SHIFT	(3)
#define	TSB_SECONDARY_BUCKET_SIZE	(1 << TSB_SECONDARY_BUCKET_SHIFT)
#define	TSB_SECONDARY_BUCKET_MASK	(TSB_SECONDARY_BUCKET_SIZE - 1)

#define	TSB_PRIMARY_STTE_SHIFT \
	(STTE_SHIFT + TSB_PRIMARY_BUCKET_SHIFT)
#define	TSB_PRIMARY_STTE_MASK		((1 << TSB_PRIMARY_STTE_SHIFT) - 1)

#define	TSB_LEVEL1_BUCKET_MASK \
	((TSB_SECONDARY_BUCKET_MASK & ~TSB_PRIMARY_BUCKET_MASK) << \
	    (PAGE_SHIFT - TSB_PRIMARY_BUCKET_SHIFT))
#define	TSB_LEVEL1_BUCKET_SHIFT \
	(TSB_BUCKET_SPREAD_SHIFT + \
	    (TSB_SECONDARY_BUCKET_SHIFT - TSB_PRIMARY_BUCKET_SHIFT))

#define	TSB_BUCKET_SPREAD_SHIFT		(2)

#define	TSB_DEPTH			(7)

#define	TSB_KERNEL_MASK \
	(((KVA_PAGES * PAGE_SIZE_4M) >> STTE_SHIFT) - 1)
#define	TSB_KERNEL_VA_MASK		(TSB_KERNEL_MASK << STTE_SHIFT)

extern struct stte *tsb_kernel;
extern vm_offset_t tsb_kernel_phys;

static __inline struct stte *
tsb_base(u_int level)
{
	vm_offset_t base;
	size_t len;

	if (level == 0)
		base = TSB_USER_MIN_ADDRESS;
	else {
		len = 1UL << ((level * TSB_BUCKET_SPREAD_SHIFT) +
		    TSB_MASK_WIDTH + TSB_SECONDARY_BUCKET_SHIFT +
		    STTE_SHIFT);
		base = TSB_USER_MIN_ADDRESS + len;
	}
	return (struct stte *)base;
}

static __inline u_long
tsb_bucket_shift(u_int level)
{
	return (level == 0 ?
	    TSB_PRIMARY_BUCKET_SHIFT : TSB_SECONDARY_BUCKET_SHIFT);
}

static __inline u_long
tsb_bucket_size(u_int level)
{
	return (1UL << tsb_bucket_shift(level));
}

static __inline u_long
tsb_bucket_mask(u_int level)
{
	return (tsb_bucket_size(level) - 1);
}

static __inline u_long
tsb_mask_width(u_int level)
{
	return ((level * TSB_BUCKET_SPREAD_SHIFT) + TSB_MASK_WIDTH);
}

static __inline u_long
tsb_mask(u_int level)
{
	return ((1UL << tsb_mask_width(level)) - 1);
}

static __inline u_int
tsb_tlb_slot(u_int level)
{
	return (level == 0 ?
	    TLB_SLOT_TSB_USER_PRIMARY : TLB_SLOT_TSB_USER_SECONDARY);
}

static __inline vm_offset_t
tsb_stte_vtophys(pmap_t pm, struct stte *stp)
{
	vm_offset_t va;
	u_long data;

	va = (vm_offset_t)stp;
	if (pm == kernel_pmap)
		return (tsb_kernel_phys + (va - (vm_offset_t)tsb_kernel));

	if (trunc_page(va) == TSB_USER_MIN_ADDRESS)
		data = pm->pm_stte.st_tte.tte_data;
	else
		data = ldxa(TLB_DAR_SLOT(tsb_tlb_slot(1)),
		    ASI_DTLB_DATA_ACCESS_REG);
	return ((vm_offset_t)((TD_PA(data)) + (va & PAGE_MASK)));
}

static __inline struct stte *
tsb_vpntobucket(vm_offset_t vpn, u_int level)
{
	return (tsb_base(level) +
	    ((vpn & tsb_mask(level)) << tsb_bucket_shift(level)));
}

static __inline struct stte *
tsb_vtobucket(vm_offset_t va, u_int level)
{
	return (tsb_vpntobucket(va >> PAGE_SHIFT, level));
}

static __inline struct stte *
tsb_kvpntostte(vm_offset_t vpn)
{
	return (&tsb_kernel[vpn & TSB_KERNEL_MASK]);
}

static __inline struct stte *
tsb_kvtostte(vm_offset_t va)
{
	return (tsb_kvpntostte(va >> PAGE_SHIFT));
}

struct	stte *tsb_get_bucket(pmap_t pm, u_int level, vm_offset_t va,
			     int allocate);
int	tsb_miss(pmap_t pm, u_int type, struct mmuframe *mf);
struct	tte tsb_page_alloc(pmap_t pm, vm_offset_t va);
void	tsb_page_fault(pmap_t pm, int level, vm_offset_t va, struct stte *stp);
void	tsb_page_init(void *va, int level);
struct	stte *tsb_stte_lookup(pmap_t pm, vm_offset_t va);
struct	stte *tsb_stte_promote(pmap_t pm, vm_offset_t va, struct stte *stp);
void	tsb_stte_remove(struct stte *stp);
struct	stte *tsb_tte_enter(pmap_t pm, vm_offset_t va, struct tte tte);
void	tsb_tte_local_remove(struct tte *tp);

extern vm_offset_t tsb_bootstrap_pages[];
extern int tsb_bootstrap_index;

#endif /* !_MACHINE_TSB_H_ */
