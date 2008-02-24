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
 * $FreeBSD: src/sys/sun4v/include/tte.h,v 1.2 2006/10/09 04:45:18 kmacy Exp $
 */

#ifndef	_MACHINE_TTE_H_
#define	_MACHINE_TTE_H_

#define	TTE_SHIFT	(4)

#define	TD_SOFT2_SHIFT	(50)
#define	TD_DIAG_SHIFT	(41)
#define	TD_PA_SHIFT	(13)
#define	TD_SOFT_SHIFT	(7)

#define	TD_SOFT2_BITS	(9)
#define	TD_DIAG_BITS	(9)
#define	TD_PA_BITS	(42)
#define	TD_SOFT_BITS	(6)

#define	TD_SOFT2_MASK	((1UL << TD_SOFT2_BITS) - 1)
#define	TD_DIAG_MASK	((1UL << TD_DIAG_BITS) - 1)
#define	TD_PA_MASK	((1UL << TD_PA_BITS) - 1)
#define	TD_SOFT_MASK	((1UL << TD_SOFT_BITS) - 1)

#define	TTE8K		(0UL)
#define	TTE64K		(1UL)
#define	TTE512K	        (2UL)
#define	TTE4M		(3UL)
#define	TTE32M		(4UL)
#define	TTE256M	        (5UL)
#define	TTE2G		(6UL)
#define	TTE16G		(7UL)

#define	TD_PA(pa)	((pa) & (TD_PA_MASK << TD_PA_SHIFT))
/* NOTE: bit 6 of TD_SOFT will be sign-extended if used as an immediate. */
#define	TD_FAKE		((1UL << 5) << TD_SOFT_SHIFT)
#define	TD_EXEC		((1UL << 4) << TD_SOFT_SHIFT)
#define	TD_REF		((1UL << 3) << TD_SOFT_SHIFT)
#define	TD_PV		((1UL << 2) << TD_SOFT_SHIFT)
#define	TD_SW		((1UL << 1) << TD_SOFT_SHIFT)
#define	TD_WIRED	((1UL << 0) << TD_SOFT_SHIFT)
#define	TD_L		(1UL << 6)
#define	TD_CP		(1UL << 5)
#define	TD_CV		(1UL << 4)
#define	TD_E		(1UL << 3)
#define	TD_P		(1UL << 2)
#define	TD_W		(1UL << 1)
#define	TD_G		(1UL << 0)


#define	TTE_GET_PAGE_SIZE(tp) \
	(1 << TTE_GET_PAGE_SHIFT(tp))
#define	TTE_GET_PAGE_MASK(tp) \
	(TTE_GET_PAGE_SIZE(tp) - 1)

#define	TTE_GET_PA(tte_data) \
	(tte_data & (TD_PA_MASK << TD_PA_SHIFT))
#define	TTE_GET_VPN(tp) \
	((tp)->tte_vpn >> TV_SIZE_BITS)
#define	TTE_GET_VA(tp) \
	(TTE_GET_VPN(tp) << TTE_GET_PAGE_SHIFT(tp))
#define	TTE_GET_PMAP(tp) \
	(((tp)->tte_data & TD_P) != 0 ? \
	 (kernel_pmap) : \
	 (PHYS_TO_VM_PAGE(pmap_kextract((vm_offset_t)(tp)))->md.pmap))
#define	TTE_ZERO(tp) \
	memset(tp, 0, sizeof(*tp))

struct pmap;
#define PTE_SHIFT       (3)
#define PT_SHIFT        (PAGE_SHIFT - PTE_SHIFT)

#define	VTD_SOFT_SHIFT	(56)

#define	VTD_V		(1UL << 63)
#define	VTD_NFO		(1UL << 62)
#define	VTD_PA(pa)	((pa) & (TD_PA_MASK << TD_PA_SHIFT))
#define	VTD_IE		(1UL << 12)
#define	VTD_E		(1UL << 11)
#define	VTD_CP		(1UL << 10)
#define	VTD_CV		(1UL << 9)
#define	VTD_P		(1UL << 8)
#define	VTD_X		(1UL << 7)
#define	VTD_W		(1UL << 6)

#define	VTD_REF		(1UL << 5)
#define	VTD_SW_W	(1UL << 4)
#define	VTD_MANAGED	(1UL << 58)
#define	VTD_WIRED	(1UL << 57)
#define	VTD_LOCK	(1UL << 56)



#define	VTD_8K		TTE8K 
#define	VTD_64K		TTE64K 
#define	VTD_512K	TTE512K 
#define	VTD_4M		TTE4M 
#define	VTD_32M		TTE32M 
#define	VTD_256M	TTE256M 


/*
 * sparc64 compatibility for the loader
 */

#define	TD_SIZE_SHIFT	(61)
#define	TS_4M		(3UL)

#define	TD_V		(1UL << 63)
#define	TD_4M		(TS_4M << TD_SIZE_SHIFT)

/*
 * default flags for kernel pages
 */
#define TTE_KERNEL            VTD_V | VTD_CP | VTD_CV | VTD_P | VTD_X | VTD_W | VTD_SW_W | VTD_REF | VTD_WIRED
#define TTE_KERNEL_MINFLAGS   VTD_P
#define TTE_MINFLAGS          VTD_V | VTD_CP | VTD_CV

#define VTD_SIZE_BITS   (4)
#define VTD_SIZE_MASK   ((1 << VTD_SIZE_BITS) - 1)


#define	TTE_SIZE_SPREAD	(3)
#define	TTE_PAGE_SHIFT(sz) \
	(PAGE_SHIFT + ((sz) * TTE_SIZE_SPREAD))
#define	TTE_GET_SIZE(tte_data) \
	(tte_data & VTD_SIZE_MASK)
#define	TTE_GET_PAGE_SHIFT(tte_data) \
	TTE_PAGE_SHIFT(TTE_GET_SIZE(tte_data))

#ifdef notyet
typedef union {
	struct tte {
		unsigned int	v:1;		/* <63> valid */
		unsigned int	nfo:1;		/* <62> non-fault only */
		unsigned int	sw:3;	        /* <61:59> sw */
		unsigned int    managed:1;      /* <58> managed */
		unsigned int    wired:1;        /* <57> wired */
		unsigned int	lock:1;		/* <56> sw - locked */
		unsigned long	pa:43;	        /* <55:13> pa */
		unsigned int	ie:1;		/* <12> 1=invert endianness */
		unsigned int	e:1;		/* <11> side effect */
		unsigned int	cp:1;		/* <10> physically cache */
		unsigned int	cv:1;		/* <9> virtually cache */
		unsigned int	p:1;		/* <8> privilege required */
		unsigned int	x:1;		/* <7> execute perm */
		unsigned int	w:1;		/* <6> write perm */
		unsigned int	ref:1;		/* <5> sw - ref */
		unsigned int	wr_perm:1;	/* <4> sw - write perm */
		unsigned int	rsvd:1;		/* <3> reserved */
		unsigned int	sz:3;		/* <2:0> pagesize */
	} tte_bit;
	uint64_t		ll;
} tte_t;
#endif

#define	tte_val 	tte_bit.v		/* use < 0 check in asm */
#define	tte_size	tte_bit.sz
#define	tte_nfo		tte_bit.nfo
#define	tte_ie		tte_bit.ie		
#define	tte_wired       tte_bit.wired
#define	tte_pa	        tte_bit.pa
#define	tte_ref		tte_bit.ref
#define	tte_wr_perm	tte_bit.wr_perm
#define	tte_exec_perm	tte_bit.x
#define	tte_lock	tte_bit.lock
#define	tte_cp		tte_bit.cp
#define	tte_cv		tte_bit.cv
#define	tte_se		tte_bit.e
#define	tte_priv	tte_bit.p
#define	tte_hwwr	tte_bit.w

#define	TTE_IS_VALID(ttep)	((ttep)->tte_inthi < 0)
#define	TTE_SET_INVALID(ttep)	((ttep)->tte_val = 0)
#define	TTE_IS_8K(ttep)		(TTE_CSZ(ttep) == TTE8K)
#define	TTE_IS_WIRED(ttep)	((ttep)->tte_wired)
#define	TTE_IS_WRITABLE(ttep)	((ttep)->tte_wr_perm)
#define	TTE_IS_EXECUTABLE(ttep)	((ttep)->tte_exec_perm)
#define	TTE_IS_PRIVILEGED(ttep)	((ttep)->tte_priv)
#define	TTE_IS_NOSYNC(ttep)	((ttep)->tte_no_sync)
#define	TTE_IS_LOCKED(ttep)	((ttep)->tte_lock)
#define	TTE_IS_SIDEFFECT(ttep)	((ttep)->tte_se)
#define	TTE_IS_NFO(ttep)	((ttep)->tte_nfo)

#define	TTE_IS_REF(ttep)	((ttep)->tte_ref)
#define	TTE_IS_MOD(ttep)	((ttep)->tte_hwwr)
#define	TTE_IS_IE(ttep)		((ttep)->tte_ie)
#define	TTE_SET_SUSPEND(ttep)	((ttep)->tte_suspend = 1)
#define	TTE_CLR_SUSPEND(ttep)	((ttep)->tte_suspend = 0)
#define	TTE_IS_SUSPEND(ttep)	((ttep)->tte_suspend)
#define	TTE_SET_REF(ttep)	((ttep)->tte_ref = 1)
#define	TTE_CLR_REF(ttep)	((ttep)->tte_ref = 0)
#define	TTE_SET_LOCKED(ttep)	((ttep)->tte_lock = 1)
#define	TTE_CLR_LOCKED(ttep)	((ttep)->tte_lock = 0)
#define	TTE_SET_MOD(ttep)	((ttep)->tte_hwwr = 1)
#define	TTE_CLR_MOD(ttep)	((ttep)->tte_hwwr = 0)
#define	TTE_SET_RM(ttep)						\
	(((ttep)->tte_intlo) =						\
	(ttep)->tte_intlo | TTE_HWWR_INT | TTE_REF_INT)
#define	TTE_CLR_RM(ttep)						\
	(((ttep)->tte_intlo) =						\
	(ttep)->tte_intlo & ~(TTE_HWWR_INT | TTE_REF_INT))

#define	TTE_SET_WRT(ttep)	((ttep)->tte_wr_perm = 1)
#define	TTE_CLR_WRT(ttep)	((ttep)->tte_wr_perm = 0)
#define	TTE_SET_EXEC(ttep)	((ttep)->tte_exec_perm = 1)
#define	TTE_CLR_EXEC(ttep)	((ttep)->tte_exec_perm = 0)
#define	TTE_SET_PRIV(ttep)	((ttep)->tte_priv = 1)
#define	TTE_CLR_PRIV(ttep)	((ttep)->tte_priv = 0)

#define	TTE_BSZS_SHIFT(sz)	((sz) * 3)

struct pmap;

void tte_clear_phys_bit(vm_page_t m, uint64_t flags);

void tte_set_phys_bit(vm_page_t m, uint64_t flags);

boolean_t tte_get_phys_bit(vm_page_t m, uint64_t flags);

void tte_clear_virt_bit(struct pmap *pmap, vm_offset_t va, uint64_t flags);

void tte_set_virt_bit(struct pmap *pmap, vm_offset_t va, uint64_t flags);

boolean_t tte_get_virt_bit(struct pmap *pmap, vm_offset_t va, uint64_t flags);

#endif /* !_MACHINE_TTE_H_ */
