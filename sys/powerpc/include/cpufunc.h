/*-
 * Copyright (c) 1998 Doug Rabson
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
 * $FreeBSD$
 */

#ifndef _MACHINE_CPUFUNC_H_
#define	_MACHINE_CPUFUNC_H_

#ifdef _KERNEL

#include <sys/types.h>

#include <machine/psl.h>

#define	CRITICAL_FORK	(mfmsr() | PSL_EE)

#ifdef __GNUC__

static __inline void
breakpoint(void)
{

	return;
}

#endif

/* CPU register mangling inlines */

static __inline void
mtmsr(unsigned int value)
{

	__asm __volatile ("mtmsr %0" :: "r"(value));
}

static __inline unsigned int
mfmsr(void)
{
	unsigned int	value;

	__asm __volatile ("mfmsr %0" : "=r"(value));

	return (value);
}

static __inline void
mtdec(unsigned int value)
{

	__asm __volatile ("mtdec %0" :: "r"(value));
}

static __inline unsigned int
mfdec(void)
{
	unsigned int	value;

	__asm __volatile ("mfdec %0" : "=r"(value));

	return (value);
}

/*
 * Bogus interrupt manipulation
 */
static __inline void
disable_intr(void)
{
	unsigned int	msr;

	msr = mfmsr();
	mtmsr(msr & ~PSL_EE);
}

static __inline void
enable_intr(void)
{
	unsigned int	msr;

	msr = mfmsr();
	mtmsr(msr | PSL_EE);
}

static __inline unsigned int
save_intr(void)
{
	unsigned int	msr;

	msr = mfmsr();

	return msr;
}

static __inline critical_t
cpu_critical_enter(void)
{

	return ((critical_t)save_intr());
}

static __inline void
restore_intr(unsigned int msr)
{

	mtmsr(msr);
}

static __inline void
cpu_critical_exit(critical_t msr)
{

	return (restore_intr((unsigned int)msr));
}

static __inline void
powerpc_mb(void)
{

	__asm __volatile("eieio; sync" : : : "memory");
}

static __inline struct pcpu *
powerpc_get_pcpup(void)
{
	struct pcpu	*ret;

	__asm ("mfsprg %0, 0" : "=r"(ret));

	return(ret);
}

#endif /* _KERNEL */

#endif /* !_MACHINE_CPUFUNC_H_ */
