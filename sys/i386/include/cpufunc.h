/*-
 * Copyright (c) 1993 The Regents of the University of California.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 * Functions to provide access to special i386 instructions.
 * This in included in sys/systm.h, and that file should be
 * used in preference to this.
 */

#ifndef _MACHINE_CPUFUNC_H_
#define	_MACHINE_CPUFUNC_H_

#include <sys/cdefs.h>
#include <machine/psl.h>

__BEGIN_DECLS
#define readb(va)	(*(volatile u_int8_t *) (va))
#define readw(va)	(*(volatile u_int16_t *) (va))
#define readl(va)	(*(volatile u_int32_t *) (va))

#define writeb(va, d)	(*(volatile u_int8_t *) (va) = (d))
#define writew(va, d)	(*(volatile u_int16_t *) (va) = (d))
#define writel(va, d)	(*(volatile u_int32_t *) (va) = (d))

#define	CRITICAL_FORK	(read_eflags() | PSL_I)

#ifdef	__GNUC__

#ifdef SWTCH_OPTIM_STATS
extern	int	tlb_flush_count;	/* XXX */
#endif

static __inline void
breakpoint(void)
{
	__asm __volatile("int $3");
}

static __inline u_int
bsfl(u_int mask)
{
	u_int	result;

	__asm __volatile("bsfl %1,%0" : "=r" (result) : "rm" (mask));
	return (result);
}

static __inline u_int
bsrl(u_int mask)
{
	u_int	result;

	__asm __volatile("bsrl %1,%0" : "=r" (result) : "rm" (mask));
	return (result);
}

static __inline void
disable_intr(void)
{
	__asm __volatile("cli" : : : "memory");
}

static __inline void
enable_intr(void)
{
	__asm __volatile("sti");
}

#define	HAVE_INLINE_FFS

static __inline int
ffs(int mask)
{
	/*
	 * Note that gcc-2's builtin ffs would be used if we didn't declare
	 * this inline or turn off the builtin.  The builtin is faster but
	 * broken in gcc-2.4.5 and slower but working in gcc-2.5 and later
	 * versions.
	 */
	 return (mask == 0 ? mask : bsfl((u_int)mask) + 1);
}

#define	HAVE_INLINE_FLS

static __inline int
fls(int mask)
{
	return (mask == 0 ? mask : bsrl((u_int)mask) + 1);
}

#if __GNUC__ < 2

#define	inb(port)		inbv(port)
#define	outb(port, data)	outbv(port, data)

#else /* __GNUC >= 2 */

/*
 * The following complications are to get around gcc not having a
 * constraint letter for the range 0..255.  We still put "d" in the
 * constraint because "i" isn't a valid constraint when the port
 * isn't constant.  This only matters for -O0 because otherwise
 * the non-working version gets optimized away.
 * 
 * Use an expression-statement instead of a conditional expression
 * because gcc-2.6.0 would promote the operands of the conditional
 * and produce poor code for "if ((inb(var) & const1) == const2)".
 *
 * The unnecessary test `(port) < 0x10000' is to generate a warning if
 * the `port' has type u_short or smaller.  Such types are pessimal.
 * This actually only works for signed types.  The range check is
 * careful to avoid generating warnings.
 */
#define	inb(port) __extension__ ({					\
	u_char	_data;							\
	if (__builtin_constant_p(port) && ((port) & 0xffff) < 0x100	\
	    && (port) < 0x10000)					\
		_data = inbc(port);					\
	else								\
		_data = inbv(port);					\
	_data; })

#define	outb(port, data) (						\
	__builtin_constant_p(port) && ((port) & 0xffff) < 0x100		\
	&& (port) < 0x10000						\
	? outbc(port, data) : outbv(port, data))

static __inline u_char
inbc(u_int port)
{
	u_char	data;

	__asm __volatile("inb %1,%0" : "=a" (data) : "id" ((u_short)(port)));
	return (data);
}

static __inline void
outbc(u_int port, u_char data)
{
	__asm __volatile("outb %0,%1" : : "a" (data), "id" ((u_short)(port)));
}

#endif /* __GNUC <= 2 */

static __inline u_char
inbv(u_int port)
{
	u_char	data;
	/*
	 * We use %%dx and not %1 here because i/o is done at %dx and not at
	 * %edx, while gcc generates inferior code (movw instead of movl)
	 * if we tell it to load (u_short) port.
	 */
	__asm __volatile("inb %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

static __inline u_int
inl(u_int port)
{
	u_int	data;

	__asm __volatile("inl %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

static __inline void
insb(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insb"
			 : "+D" (addr), "+c" (cnt)
			 : "d" (port)
			 : "memory");
}

static __inline void
insw(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insw"
			 : "+D" (addr), "+c" (cnt)
			 : "d" (port)
			 : "memory");
}

static __inline void
insl(u_int port, void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; insl"
			 : "+D" (addr), "+c" (cnt)
			 : "d" (port)
			 : "memory");
}

static __inline void
invd(void)
{
	__asm __volatile("invd");
}

#if defined(SMP) && defined(_KERNEL)

/*
 * When using APIC IPI's, invlpg() is not simply the invlpg instruction
 * (this is a bug) and the inlining cost is prohibitive since the call
 * executes into the IPI transmission system.
 */
void	invlpg		__P((u_int addr));
void	invltlb		__P((void));

static __inline void
cpu_invlpg(void *addr)
{
	__asm __volatile("invlpg %0" : : "m" (*(char *)addr) : "memory");
}

static __inline void
cpu_invltlb(void)
{
	u_int	temp;
	/*
	 * This should be implemented as load_cr3(rcr3()) when load_cr3()
	 * is inlined.
	 */
	__asm __volatile("movl %%cr3, %0; movl %0, %%cr3" : "=r" (temp)
			 : : "memory");
#if defined(SWTCH_OPTIM_STATS)
	++tlb_flush_count;
#endif
}

#else /* !(SMP && _KERNEL) */

static __inline void
invlpg(u_int addr)
{
	__asm __volatile("invlpg %0" : : "m" (*(char *)addr) : "memory");
}

static __inline void
invltlb(void)
{
	u_int	temp;
	/*
	 * This should be implemented as load_cr3(rcr3()) when load_cr3()
	 * is inlined.
	 */
	__asm __volatile("movl %%cr3, %0; movl %0, %%cr3" : "=r" (temp)
			 : : "memory");
#ifdef SWTCH_OPTIM_STATS
	++tlb_flush_count;
#endif
}

#endif /* SMP && _KERNEL */

static __inline u_short
inw(u_int port)
{
	u_short	data;

	__asm __volatile("inw %%dx,%0" : "=a" (data) : "d" (port));
	return (data);
}

static __inline void
outbv(u_int port, u_char data)
{
	u_char	al;
	/*
	 * Use an unnecessary assignment to help gcc's register allocator.
	 * This make a large difference for gcc-1.40 and a tiny difference
	 * for gcc-2.6.0.  For gcc-1.40, al had to be ``asm("ax")'' for
	 * best results.  gcc-2.6.0 can't handle this.
	 */
	al = data;
	__asm __volatile("outb %0,%%dx" : : "a" (al), "d" (port));
}

static __inline void
outl(u_int port, u_int data)
{
	/*
	 * outl() and outw() aren't used much so we haven't looked at
	 * possible micro-optimizations such as the unnecessary
	 * assignment for them.
	 */
	__asm __volatile("outl %0,%%dx" : : "a" (data), "d" (port));
}

static __inline void
outsb(u_int port, const void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsb"
			 : "+S" (addr), "+c" (cnt)
			 : "d" (port));
}

static __inline void
outsw(u_int port, const void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsw"
			 : "+S" (addr), "+c" (cnt)
			 : "d" (port));
}

static __inline void
outsl(u_int port, const void *addr, size_t cnt)
{
	__asm __volatile("cld; rep; outsl"
			 : "+S" (addr), "+c" (cnt)
			 : "d" (port));
}

static __inline void
outw(u_int port, u_short data)
{
	__asm __volatile("outw %0,%%dx" : : "a" (data), "d" (port));
}

static __inline u_int
rcr2(void)
{
	u_int	data;

	__asm __volatile("movl %%cr2,%0" : "=r" (data));
	return (data);
}

static __inline u_int
read_eflags(void)
{
	u_int	ef;

	__asm __volatile("pushfl; popl %0" : "=r" (ef));
	return (ef);
}

static __inline void
do_cpuid(u_int ax, u_int *p)
{
	__asm __volatile(
	"cpuid"
	: "=a" (p[0]), "=b" (p[1]), "=c" (p[2]), "=d" (p[3])
	:  "0" (ax)
	);
}

static __inline u_int64_t
rdmsr(u_int msr)
{
	u_int64_t rv;

	__asm __volatile("rdmsr" : "=A" (rv) : "c" (msr));
	return (rv);
}

static __inline u_int64_t
rdpmc(u_int pmc)
{
	u_int64_t rv;

	__asm __volatile("rdpmc" : "=A" (rv) : "c" (pmc));
	return (rv);
}

static __inline u_int64_t
rdtsc(void)
{
	u_int64_t rv;

	__asm __volatile("rdtsc" : "=A" (rv));
	return (rv);
}

static __inline void
wbinvd(void)
{
	__asm __volatile("wbinvd");
}

static __inline void
write_eflags(u_int ef)
{
	__asm __volatile("pushl %0; popfl" : : "r" (ef));
}

static __inline void
wrmsr(u_int msr, u_int64_t newval)
{
	__asm __volatile("wrmsr" : : "A" (newval), "c" (msr));
}

static __inline u_int
rfs(void)
{
	u_int sel;
	__asm __volatile("movl %%fs,%0" : "=rm" (sel));
	return (sel);
}

static __inline u_int
rgs(void)
{
	u_int sel;
	__asm __volatile("movl %%gs,%0" : "=rm" (sel));
	return (sel);
}

static __inline void
load_fs(u_int sel)
{
	__asm __volatile("movl %0,%%fs" : : "rm" (sel));
}

static __inline void
load_gs(u_int sel)
{
	__asm __volatile("movl %0,%%gs" : : "rm" (sel));
}

static __inline u_int
rdr0(void)
{
	u_int	data;
	__asm __volatile("movl %%dr0,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr0(u_int sel)
{
	__asm __volatile("movl %0,%%dr0" : : "r" (sel));
}

static __inline u_int
rdr1(void)
{
	u_int	data;
	__asm __volatile("movl %%dr1,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr1(u_int sel)
{
	__asm __volatile("movl %0,%%dr1" : : "r" (sel));
}

static __inline u_int
rdr2(void)
{
	u_int	data;
	__asm __volatile("movl %%dr2,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr2(u_int sel)
{
	__asm __volatile("movl %0,%%dr2" : : "r" (sel));
}

static __inline u_int
rdr3(void)
{
	u_int	data;
	__asm __volatile("movl %%dr3,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr3(u_int sel)
{
	__asm __volatile("movl %0,%%dr3" : : "r" (sel));
}

static __inline u_int
rdr4(void)
{
	u_int	data;
	__asm __volatile("movl %%dr4,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr4(u_int sel)
{
	__asm __volatile("movl %0,%%dr4" : : "r" (sel));
}

static __inline u_int
rdr5(void)
{
	u_int	data;
	__asm __volatile("movl %%dr5,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr5(u_int sel)
{
	__asm __volatile("movl %0,%%dr5" : : "r" (sel));
}

static __inline u_int
rdr6(void)
{
	u_int	data;
	__asm __volatile("movl %%dr6,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr6(u_int sel)
{
	__asm __volatile("movl %0,%%dr6" : : "r" (sel));
}

static __inline u_int
rdr7(void)
{
	u_int	data;
	__asm __volatile("movl %%dr7,%0" : "=r" (data));
	return (data);
}

static __inline void
load_dr7(u_int sel)
{
	__asm __volatile("movl %0,%%dr7" : : "r" (sel));
}

static __inline critical_t
cpu_critical_enter(void)
{
	critical_t eflags;

	eflags = read_eflags();
	disable_intr();
	return (eflags);
}

static __inline void
cpu_critical_exit(critical_t eflags)
{
	write_eflags(eflags);
}

#else /* !__GNUC__ */

int	breakpoint	__P((void));
u_int	bsfl		__P((u_int mask));
u_int	bsrl		__P((u_int mask));
void	disable_intr	__P((void));
void	do_cpuid	__P((u_int ax, u_int *p));
void	enable_intr	__P((void));
u_char	inb		__P((u_int port));
u_int	inl		__P((u_int port));
void	insb		__P((u_int port, void *addr, size_t cnt));
void	insl		__P((u_int port, void *addr, size_t cnt));
void	insw		__P((u_int port, void *addr, size_t cnt));
void	invd		__P((void));
void	invlpg		__P((u_int addr));
void	invltlb		__P((void));
u_short	inw		__P((u_int port));
void	outb		__P((u_int port, u_char data));
void	outl		__P((u_int port, u_int data));
void	outsb		__P((u_int port, void *addr, size_t cnt));
void	outsl		__P((u_int port, void *addr, size_t cnt));
void	outsw		__P((u_int port, void *addr, size_t cnt));
void	outw		__P((u_int port, u_short data));
u_int	rcr2		__P((void));
u_int64_t rdmsr		__P((u_int msr));
u_int64_t rdpmc		__P((u_int pmc));
u_int64_t rdtsc		__P((void));
u_int	read_eflags	__P((void));
void	wbinvd		__P((void));
void	write_eflags	__P((u_int ef));
void	wrmsr		__P((u_int msr, u_int64_t newval));
u_int	rfs		__P((void));
u_int	rgs		__P((void));
void	load_fs		__P((u_int sel));
void	load_gs		__P((u_int sel));
critical_t cpu_critical_enter __P((void));
void	cpu_critical_exit __P((critical_t eflags));

#endif	/* __GNUC__ */

void	load_cr0	__P((u_int cr0));
void	load_cr3	__P((u_int cr3));
void	load_cr4	__P((u_int cr4));
void	ltr		__P((u_short sel));
u_int	rcr0		__P((void));
u_int	rcr3		__P((void));
u_int	rcr4		__P((void));
void    reset_dbregs    __P((void));
__END_DECLS

#endif /* !_MACHINE_CPUFUNC_H_ */
