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

#ifndef	_MACHINE_TTE_H_
#define	_MACHINE_TTE_H_

#define	TTE_SHIFT	(5)

#define	TD_SIZE_SHIFT	(61)
#define	TD_SOFT2_SHIFT	(50)
#define	TD_DIAG_SHIFT	(41)
#define	TD_PA_SHIFT	(13)
#define	TD_SOFT_SHIFT	(7)

#define	TD_SIZE_BITS	(2)
#define	TD_SOFT2_BITS	(9)
#define	TD_DIAG_BITS	(9)
#define	TD_PA_BITS	(28)
#define	TD_SOFT_BITS	(6)

#define	TD_SIZE_MASK	((1UL << TD_SIZE_BITS) - 1)
#define	TD_SOFT2_MASK	((1UL << TD_SOFT2_BITS) - 1)
#define	TD_DIAG_MASK	((1UL << TD_DIAG_BITS) - 1)
#define	TD_PA_MASK	((1UL << TD_PA_BITS) - 1)
#define	TD_SOFT_MASK	((1UL << TD_SOFT_BITS) - 1)

#define	TD_SIZE_SPREAD	(3)

#define	TS_EXEC		(1UL << 4)
#define	TS_REF		(1UL << 3)
#define	TS_PV		(1UL << 2)
#define	TS_W		(1UL << 1)
#define	TS_WIRED	(1UL << 0)

#define	TD_V		(1UL << 63)
#define	TD_8K		(0UL << TD_SIZE_SHIFT)
#define	TD_64K		(1UL << TD_SIZE_SHIFT)
#define	TD_512K		(2UL << TD_SIZE_SHIFT)
#define	TD_4M		(3UL << TD_SIZE_SHIFT)
#define	TD_NFO		(1UL << 60)
#define	TD_IE		(1UL << 59)
#define	TD_PA(pa)	((pa) & (TD_PA_MASK << TD_PA_SHIFT))
#define	TD_EXEC		(TS_EXEC << TD_SOFT_SHIFT)
#define	TD_REF		(TS_REF << TD_SOFT_SHIFT)
#define	TD_PV		(TS_PV << TD_SOFT_SHIFT)
#define	TD_SW		(TS_W << TD_SOFT_SHIFT)
#define	TD_WIRED	(TS_WIRED << TD_SOFT_SHIFT)
#define	TD_L		(1UL << 6)
#define	TD_CP		(1UL << 5)
#define	TD_CV		(1UL << 4)
#define	TD_E		(1UL << 3)
#define	TD_P		(1UL << 2)
#define	TD_W		(1UL << 1)
#define	TD_G		(1UL << 0)

#define	TV_VPN(va)	((va) >> PAGE_SHIFT)

#define	TTE_GET_SIZE(tp) \
	(((tp)->tte_data >> TD_SIZE_SHIFT) & TD_SIZE_MASK)
#define	TTE_GET_PAGE_SHIFT(tp) \
	(PAGE_SHIFT + (TTE_GET_SIZE(tp) * TD_SIZE_SPREAD))
#define	TTE_GET_PAGE_SIZE(tp) \
	(1 << TTE_GET_PAGE_SHIFT(tp))
#define	TTE_GET_PAGE_MASK(tp) \
	(TTE_GET_PAGE_SIZE(tp) - 1)

#define	TTE_GET_PA(tp) \
	((tp)->tte_data & (TD_PA_MASK << TD_PA_SHIFT))
#define	TTE_GET_VA(tp) \
	((tp)->tte_vpn << PAGE_SHIFT)
#define	TTE_GET_PMAP(tp) \
	((tp)->tte_pmap)
#define	TTE_ZERO(tp) \
	bzero(tp, sizeof(*tp))

struct tte {
	u_long	tte_vpn;
	u_long	tte_data;
	STAILQ_ENTRY(tte) tte_link;
	struct	pmap *tte_pmap;
};

static __inline int
tte_match_vpn(struct tte *tp, vm_offset_t vpn)
{
	return ((tp->tte_data & TD_V) != 0 && tp->tte_vpn == vpn);
}

static __inline int
tte_match(struct tte *tp, vm_offset_t va)
{
	return (tte_match_vpn(tp, va >> PAGE_SHIFT));
}

#endif /* !_MACHINE_TTE_H_ */
