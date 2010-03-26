/*	$OpenBSD: pio.h,v 1.2 1998/09/15 10:50:12 pefo Exp $	*/

/*
 * Copyright (c) 1995-1999 Per Fogelstrom.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Per Fogelstrom.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 *	JNPR: cpufunc.h,v 1.5 2007/08/09 11:23:32 katta
 * $FreeBSD$
 */

#ifndef _MACHINE_CPUFUNC_H_
#define	_MACHINE_CPUFUNC_H_

#include <sys/types.h>
#include <machine/cpuregs.h>

/* 
 * These functions are required by user-land atomi ops
 */ 

static __inline void
mips_barrier(void)
{
	__asm __volatile (".set noreorder\n\t"
			  "nop\n\t"
			  "nop\n\t"
			  "nop\n\t"
			  "nop\n\t"
			  "nop\n\t"
			  "nop\n\t"
			  "nop\n\t"
			  "nop\n\t"
			  ".set reorder\n\t"
			  : : : "memory");
}

static __inline void
mips_wbflush(void)
{
	__asm __volatile ("sync" : : : "memory");
	mips_barrier();
#if 0
	__asm __volatile("mtc0 %0, $12\n" /* MIPS_COP_0_STATUS */
	   : : "r" (flag));
#endif
}

static __inline void
mips_read_membar(void)
{
	/* Nil */
}

static __inline void
mips_write_membar(void)
{
	mips_wbflush();
}

#ifdef _KERNEL

static __inline void
mips_tlbp(void)
{
	__asm __volatile ("tlbp");
	mips_barrier();
#if 0
	register_t ret;
	register_t tmp;

	__asm __volatile("mfc0	%0, $12\n" /* MIPS_COP_0_STATUS */
	 		 "and	%1, %0, $~1\n" /* MIPS_SR_INT_IE */
			 "mtc0	%1, $12\n" /* MIPS_COP_0_STATUS */
			 : "=r" (ret), "=r" (tmp));
	return (ret);
#endif
}

static __inline void
mips_tlbr(void)
{
	__asm __volatile ("tlbr");
	mips_barrier();
}

static __inline void
mips_tlbwi(void)
{
	__asm __volatile ("tlbwi");
	mips_barrier();
#if 0
	__asm __volatile("mfc %0, $12\n" /* MIPS_COP_0_STATUS */
	    		 "or  %0, %0, $1\n" /* MIPS_SR_INT_IE */
			 "mtc0 %0, $12\n" /* MIPS_COP_0_STATUS */
			 : "=r" (tmp));
#endif
}

static __inline void
mips_tlbwr(void)
{
	__asm __volatile ("tlbwr");
	mips_barrier();
}


#if 0	/* XXX mips64 */

#define	MIPS_RDRW64_COP0(n,r)					\
static __inline uint64_t					\
mips_rd_ ## n (void)						\
{								\
	int v0;							\
	__asm __volatile ("dmfc0 %[v0], $"__XSTRING(r)";"	\
			  : [v0] "=&r"(v0));			\
	mips_barrier();						\
	return (v0);						\
}								\
static __inline void						\
mips_wr_ ## n (uint64_t a0)					\
{								\
	__asm __volatile ("dmtc0 %[a0], $"__XSTRING(r)";"	\
			 __XSTRING(COP0_SYNC)";"		\
			 "nop;"					\
			 "nop;"					\
			 :					\
			 : [a0] "r"(a0));			\
	mips_barrier();						\
} struct __hack

MIPS_RDRW64_COP0(entrylo0, MIPS_COP_0_TLB_LO0);
MIPS_RDRW64_COP0(entrylo1, MIPS_COP_0_TLB_LO1);
MIPS_RDRW64_COP0(entryhi, MIPS_COP_0_TLB_HI);
MIPS_RDRW64_COP0(pagemask, MIPS_COP_0_TLB_PG_MASK);
MIPS_RDRW64_COP0(xcontext, MIPS_COP_0_TLB_XCONTEXT);

#undef	MIPS_RDRW64_COP0
#endif

#define	MIPS_RDRW32_COP0(n,r)					\
static __inline uint32_t					\
mips_rd_ ## n (void)						\
{								\
	int v0;							\
	__asm __volatile ("mfc0 %[v0], $"__XSTRING(r)";"	\
			  : [v0] "=&r"(v0));			\
	mips_barrier();						\
	return (v0);						\
}								\
static __inline void						\
mips_wr_ ## n (uint32_t a0)					\
{								\
	__asm __volatile ("mtc0 %[a0], $"__XSTRING(r)";"	\
			 __XSTRING(COP0_SYNC)";"		\
			 "nop;"					\
			 "nop;"					\
			 :					\
			 : [a0] "r"(a0));			\
	mips_barrier();						\
} struct __hack

#define	MIPS_RDRW32_COP0_SEL(n,r,s)					\
static __inline uint32_t					\
mips_rd_ ## n ## s(void)						\
{								\
	int v0;							\
	__asm __volatile ("mfc0 %[v0], $"__XSTRING(r)", "__XSTRING(s)";"	\
			  : [v0] "=&r"(v0));			\
	mips_barrier();						\
	return (v0);						\
}								\
static __inline void						\
mips_wr_ ## n ## s(uint32_t a0)					\
{								\
	__asm __volatile ("mtc0 %[a0], $"__XSTRING(r)", "__XSTRING(s)";"	\
			 __XSTRING(COP0_SYNC)";"		\
			 "nop;"					\
			 "nop;"					\
			 :					\
			 : [a0] "r"(a0));			\
	mips_barrier();						\
} struct __hack

#ifdef TARGET_OCTEON
static __inline void mips_sync_icache (void)
{
	__asm __volatile (
		".set push\n"
		".set mips64\n"
		".word 0x041f0000\n"		/* xxx ICACHE */
		"nop\n"
		".set pop\n"
		: : );
}
#endif

MIPS_RDRW32_COP0(compare, MIPS_COP_0_COMPARE);
MIPS_RDRW32_COP0(config, MIPS_COP_0_CONFIG);
MIPS_RDRW32_COP0_SEL(config, MIPS_COP_0_CONFIG, 1);
MIPS_RDRW32_COP0_SEL(config, MIPS_COP_0_CONFIG, 2);
MIPS_RDRW32_COP0_SEL(config, MIPS_COP_0_CONFIG, 3);
MIPS_RDRW32_COP0(count, MIPS_COP_0_COUNT);
MIPS_RDRW32_COP0(index, MIPS_COP_0_TLB_INDEX);
MIPS_RDRW32_COP0(wired, MIPS_COP_0_TLB_WIRED);
MIPS_RDRW32_COP0(cause, MIPS_COP_0_CAUSE);
MIPS_RDRW32_COP0(status, MIPS_COP_0_STATUS);

/* XXX: Some of these registers are specific to MIPS32. */
MIPS_RDRW32_COP0(entrylo0, MIPS_COP_0_TLB_LO0);
MIPS_RDRW32_COP0(entrylo1, MIPS_COP_0_TLB_LO1);
MIPS_RDRW32_COP0(entrylow, MIPS_COP_0_TLB_LOW);
MIPS_RDRW32_COP0(entryhi, MIPS_COP_0_TLB_HI);
MIPS_RDRW32_COP0(pagemask, MIPS_COP_0_TLB_PG_MASK);
MIPS_RDRW32_COP0(prid, MIPS_COP_0_PRID);
MIPS_RDRW32_COP0(watchlo, MIPS_COP_0_WATCH_LO);
MIPS_RDRW32_COP0_SEL(watchlo, MIPS_COP_0_WATCH_LO, 1);
MIPS_RDRW32_COP0_SEL(watchlo, MIPS_COP_0_WATCH_LO, 2);
MIPS_RDRW32_COP0_SEL(watchlo, MIPS_COP_0_WATCH_LO, 3);
MIPS_RDRW32_COP0(watchhi, MIPS_COP_0_WATCH_HI);
MIPS_RDRW32_COP0_SEL(watchhi, MIPS_COP_0_WATCH_HI, 1);
MIPS_RDRW32_COP0_SEL(watchhi, MIPS_COP_0_WATCH_HI, 2);
MIPS_RDRW32_COP0_SEL(watchhi, MIPS_COP_0_WATCH_HI, 3);

MIPS_RDRW32_COP0_SEL(perfcnt, MIPS_COP_0_PERFCNT, 0);
MIPS_RDRW32_COP0_SEL(perfcnt, MIPS_COP_0_PERFCNT, 1);
MIPS_RDRW32_COP0_SEL(perfcnt, MIPS_COP_0_PERFCNT, 2);
MIPS_RDRW32_COP0_SEL(perfcnt, MIPS_COP_0_PERFCNT, 3);


#undef	MIPS_RDRW32_COP0

static __inline register_t
intr_disable(void)
{
	register_t s;

	s = mips_rd_status();
	mips_wr_status(s & ~MIPS_SR_INT_IE);

	return (s);
}

static __inline register_t
intr_enable(void)
{
	register_t s;

	s = mips_rd_status();
	mips_wr_status(s | MIPS_SR_INT_IE);

	return (s);
}

#define	intr_restore(s)	mips_wr_status((s))

static __inline void
breakpoint(void)
{
	__asm __volatile ("break");
}

#if defined(__GNUC__) && !defined(__mips_o32)
static inline uint64_t
mips3_ld(const volatile uint64_t *va)
{
	uint64_t rv;

#if defined(_LP64)
	rv = *va;
#else
	__asm volatile("ld	%0,0(%1)" : "=d"(rv) : "r"(va));
#endif

	return (rv);
}

static inline void
mips3_sd(volatile uint64_t *va, uint64_t v)
{
#if defined(_LP64)
	*va = v;
#else
	__asm volatile("sd	%0,0(%1)" :: "r"(v), "r"(va));
#endif
}
#else
uint64_t mips3_ld(volatile uint64_t *va);
void mips3_sd(volatile uint64_t *, uint64_t);
#endif	/* __GNUC__ */

#endif /* _KERNEL */

#define	readb(va)	(*(volatile uint8_t *) (va))
#define	readw(va)	(*(volatile uint16_t *) (va))
#define	readl(va)	(*(volatile uint32_t *) (va))
 
#define	writeb(va, d)	(*(volatile uint8_t *) (va) = (d))
#define	writew(va, d)	(*(volatile uint16_t *) (va) = (d))
#define	writel(va, d)	(*(volatile uint32_t *) (va) = (d))

/*
 * I/O macros.
 */

#define	outb(a,v)	(*(volatile unsigned char*)(a) = (v))
#define	out8(a,v)	(*(volatile unsigned char*)(a) = (v))
#define	outw(a,v)	(*(volatile unsigned short*)(a) = (v))
#define	out16(a,v)	outw(a,v)
#define	outl(a,v)	(*(volatile unsigned int*)(a) = (v))
#define	out32(a,v)	outl(a,v)
#define	inb(a)		(*(volatile unsigned char*)(a))
#define	in8(a)		(*(volatile unsigned char*)(a))
#define	inw(a)		(*(volatile unsigned short*)(a))
#define	in16(a)		inw(a)
#define	inl(a)		(*(volatile unsigned int*)(a))
#define	in32(a)		inl(a)

#define	out8rb(a,v)	(*(volatile unsigned char*)(a) = (v))
#define	out16rb(a,v)	(__out16rb((volatile uint16_t *)(a), v))
#define	out32rb(a,v)	(__out32rb((volatile uint32_t *)(a), v))
#define	in8rb(a)	(*(volatile unsigned char*)(a))
#define	in16rb(a)	(__in16rb((volatile uint16_t *)(a)))
#define	in32rb(a)	(__in32rb((volatile uint32_t *)(a)))

#define	_swap_(x)	(((x) >> 24) | ((x) << 24) | \
	    (((x) >> 8) & 0xff00) | (((x) & 0xff00) << 8))

static __inline void __out32rb(volatile uint32_t *, uint32_t);
static __inline void __out16rb(volatile uint16_t *, uint16_t);
static __inline uint32_t __in32rb(volatile uint32_t *);
static __inline uint16_t __in16rb(volatile uint16_t *);

static __inline void
__out32rb(volatile uint32_t *a, uint32_t v)
{
	uint32_t _v_ = v;

	_v_ = _swap_(_v_);
	out32(a, _v_);
}

static __inline void
__out16rb(volatile uint16_t *a, uint16_t v)
{
	uint16_t _v_;

	_v_ = ((v >> 8) & 0xff) | (v << 8);
	out16(a, _v_);
}

static __inline uint32_t
__in32rb(volatile uint32_t *a)
{
	uint32_t _v_;

	_v_ = in32(a);
	_v_ = _swap_(_v_);
	return _v_;
}

static __inline uint16_t
__in16rb(volatile uint16_t *a)
{
	uint16_t _v_;

	_v_ = in16(a);
	_v_ = ((_v_ >> 8) & 0xff) | (_v_ << 8);
	return _v_;
}

void insb(uint8_t *, uint8_t *,int);
void insw(uint16_t *, uint16_t *,int);
void insl(uint32_t *, uint32_t *,int);
void outsb(uint8_t *, const uint8_t *,int);
void outsw(uint16_t *, const uint16_t *,int);
void outsl(uint32_t *, const uint32_t *,int);
u_int loadandclear(volatile u_int *addr);

#endif /* !_MACHINE_CPUFUNC_H_ */
