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

#ifdef __GNUC__

static __inline void
breakpoint(void)
{
	return;
}

#endif

/*
 * Bogus interrupt manipulation
 */
static __inline void
disable_intr(void)
{
	u_int32_t	msr;

	msr = 0;
	__asm __volatile(
		"mfmsr %0\n\t"
		"rlwinm %0, %0, 0, 17, 15\n\t"
		"mtmsr %0"
		: "+r" (msr));

	return;
}

static __inline void
enable_intr(void)
{
	u_int32_t	msr;

	msr = 0;
	__asm __volatile(
		"mfmsr %0\n\t"
		"ori %0, %0, 0x8000\n\t"
		"mtmsr %0"
		: "+r" (msr));

	return;
}

static __inline u_int
save_intr(void)
{
	u_int	msr;

	__asm __volatile("mfmsr %0" : "=r" (msr));

	return msr;
}

static __inline critical_t
critical_enter(void)
{
	return ((critical_t)save_intr());
}

static __inline void
restore_intr(u_int msr)
{
	__asm __volatile("mtmsr %0" : : "r" (msr));

	return;
}

static __inline void
critical_exit(critical_t msr)
{
	return (restore_intr((u_int)msr));
}

static __inline void
powerpc_mb(void)
{
	__asm __volatile("eieio;" : : : "memory");
}

static __inline void
*powerpc_get_globalp(void)
{
	void *ret;

	__asm __volatile("mfsprg %0, 0" : "=r" (ret));

	return(ret);
}

#endif /* _KERNEL */

#endif /* !_MACHINE_CPUFUNC_H_ */
