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

#define	TTE_SHIFT	(4)

#define	TT_CTX_SHIFT	(48)
#define	TT_VA_SHIFT	(22)
#define	TT_VPN_SHIFT	(9)

#define	TT_CTX_SIZE	(13)
#define	TT_VA_SIZE	(42)

#define	TT_CTX_MASK	((1UL << TT_CTX_SIZE) - 1)
#define	TT_VA_MASK	((1UL << TT_VA_SIZE) - 1)

#define	TT_G		(1UL << 63)
#define	TT_CTX(ctx)	(((u_long)(ctx) & TT_CTX_MASK) << TT_CTX_SHIFT)
#define	TT_VA(va)	((u_long)(va) >> TT_VA_SHIFT)

#define	TD_SIZE_SHIFT	(61)
#define	TD_SOFT2_SHIFT	(50)
#define	TD_DIAG_SHIFT	(41)
#define	TD_PA_SHIFT	(13)
#define	TD_SOFT_SHIFT	(7)

#define	TD_SIZE_SIZE	(2)
#define	TD_SOFT2_SIZE	(9)
#define	TD_DIAG_SIZE	(9)
#define	TD_PA_SIZE	(28)
#define	TD_SOFT_SIZE	(6)

#define	TD_SIZE_MASK	(((1UL << TD_SIZE_SIZE) - 1) << TD_SIZE_SHIFT)
#define	TD_SOFT2_MASK	(((1UL << TD_SOFT2_SIZE) - 1) << TD_SOFT2_SHIFT)
#define	TD_DIAG_MASK	(((1UL << TD_DIAG_SIZE) - 1) << TD_DIAG_SHIFT)
#define	TD_PA_MASK	(((1UL << TD_PA_SIZE) - 1) << TD_PA_SHIFT)
#define	TD_SOFT_MASK	(((1UL << TD_SOFT_SIZE) - 1) << TD_SOFT_SHIFT)

#define	TD_VA_LOW_SHIFT	TD_SOFT2_SHIFT
#define	TD_VA_LOW_MASK	TD_SOFT2_MASK

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
#define	TD_VPN_LOW(vpn)	((vpn << TD_SOFT2_SHIFT) & TD_SOFT2_MASK)
#define	TD_VA_LOW(va)	(TD_VPN_LOW((va) >> PAGE_SHIFT))
#define	TD_PA(pa)	((pa) & TD_PA_MASK)
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

#define	TT_GET_CTX(tag)	(((tag) >> TT_CTX_SHIFT) & TT_CTX_MASK)
#define	TD_GET_SIZE(d)	(((d) >> TD_SIZE_SHIFT) & 3)
#define	TD_GET_PA(d)	((d) & TD_PA_MASK)
#define	TD_GET_TLB(d)	(((d) & TD_EXEC) ? (TLB_DTLB | TLB_ITLB) : TLB_DTLB)

struct tte {
	u_long	tte_tag;
	u_long	tte_data;
};

static __inline vm_offset_t
tte_get_vpn(struct tte tte)
{
	return (((tte.tte_tag & TT_VA_MASK) << TT_VPN_SHIFT) |
	    ((tte.tte_data & TD_VA_LOW_MASK) >> TD_VA_LOW_SHIFT));
}

static __inline vm_offset_t
tte_get_va(struct tte tte)
{
	return (tte_get_vpn(tte) << PAGE_SHIFT);
}

static __inline int
tte_match(struct tte tte, vm_offset_t va)
{
	return ((tte.tte_data & TD_V) != 0 &&
	    ((tte.tte_tag ^ TT_VA(va)) & TT_VA_MASK) == 0 &&
	    ((tte.tte_data ^ TD_VA_LOW(va)) & TD_VA_LOW_MASK) == 0);
}

#endif /* !_MACHINE_TTE_H_ */
