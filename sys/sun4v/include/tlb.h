/*-
 * Copyright (c) 2001 Jake Burkholder.
 * All rights reserved.
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
 *
 * $FreeBSD: src/sys/sun4v/include/tlb.h,v 1.1.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

#ifndef	_MACHINE_TLB_H_
#define	_MACHINE_TLB_H_

#define	TLB_DIRECT_ADDRESS_BITS		(43)
#define	TLB_DIRECT_PAGE_BITS		(PAGE_SHIFT_4M)

#define	TLB_DIRECT_ADDRESS_MASK		((1UL << TLB_DIRECT_ADDRESS_BITS) - 1)
#define	TLB_DIRECT_PAGE_MASK		((1UL << TLB_DIRECT_PAGE_BITS) - 1)

#define	TLB_PHYS_TO_DIRECT(pa) \
	((pa) | VM_MIN_DIRECT_ADDRESS)
#define	TLB_DIRECT_TO_PHYS(va) \
	((va) & TLB_DIRECT_ADDRESS_MASK)
#define	TLB_DIRECT_TO_TTE_MASK \
	(TD_V | TD_4M | (TLB_DIRECT_ADDRESS_MASK - TLB_DIRECT_PAGE_MASK))

#define	TLB_DAR_SLOT_SHIFT		(3)
#define	TLB_DAR_SLOT(slot)		((slot) << TLB_DAR_SLOT_SHIFT)

#define	TAR_VPN_SHIFT			(13)
#define	TAR_CTX_MASK			((1 << TAR_VPN_SHIFT) - 1)

#define	TLB_TAR_VA(va)			((va) & ~TAR_CTX_MASK)
#define	TLB_TAR_CTX(ctx)		((ctx) & TAR_CTX_MASK)

#define	TLB_DEMAP_ID_SHIFT		(4)
#define	TLB_DEMAP_ID_PRIMARY		(0)
#define	TLB_DEMAP_ID_SECONDARY		(1)
#define	TLB_DEMAP_ID_NUCLEUS		(2)

#define	TLB_DEMAP_TYPE_SHIFT		(6)
#define	TLB_DEMAP_TYPE_PAGE		(0)
#define	TLB_DEMAP_TYPE_CONTEXT		(1)

#define	TLB_DEMAP_VA(va)		((va) & ~PAGE_MASK)
#define	TLB_DEMAP_ID(id)		((id) << TLB_DEMAP_ID_SHIFT)
#define	TLB_DEMAP_TYPE(type)		((type) << TLB_DEMAP_TYPE_SHIFT)

#define	TLB_DEMAP_PAGE			(TLB_DEMAP_TYPE(TLB_DEMAP_TYPE_PAGE))
#define	TLB_DEMAP_CONTEXT		(TLB_DEMAP_TYPE(TLB_DEMAP_TYPE_CONTEXT))

#define	TLB_DEMAP_PRIMARY		(TLB_DEMAP_ID(TLB_DEMAP_ID_PRIMARY))
#define	TLB_DEMAP_SECONDARY		(TLB_DEMAP_ID(TLB_DEMAP_ID_SECONDARY))
#define	TLB_DEMAP_NUCLEUS		(TLB_DEMAP_ID(TLB_DEMAP_ID_NUCLEUS))

#define	TLB_CTX_KERNEL			(0)
#define	TLB_CTX_USER_MIN		(1)
#define	TLB_CTX_USER_MAX		(8192)

typedef void tlb_flush_user_t(void);

struct pmap;
struct tlb_entry;

extern int kernel_tlb_slots;
extern struct tlb_entry *kernel_tlbs;

void	tlb_context_demap(struct pmap *pm);
void	tlb_page_demap(struct pmap *pm, vm_offset_t va);
void	tlb_range_demap(struct pmap *pm, vm_offset_t start, vm_offset_t end);

tlb_flush_user_t cheetah_tlb_flush_user;
tlb_flush_user_t spitfire_tlb_flush_user;

extern tlb_flush_user_t *tlb_flush_user;

#endif /* !_MACHINE_TLB_H_ */
