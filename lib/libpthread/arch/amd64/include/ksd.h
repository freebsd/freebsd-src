/*-
 * Copyright (C) 2003 David Xu <davidxu@freebsd.org>
 * Copyright (c) 2001 Daniel Eischen <deischen@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Neither the name of the author nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
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
 */

/*
 * $FreeBSD$
 */

#ifndef	_KSD_H_
#define	_KSD_H_

#include <sys/types.h>
#include <machine/sysarch.h>
extern int sysarch(int, void *);

struct kse;
struct pthread;

/*
 * Evaluates to the byte offset of the per-kse variable name.
 */
#define	__ksd_offset(name)	__offsetof(struct kse, name)

/*
 * Evaluates to the type of the per-kse variable name.
 */
#define	__ksd_type(name)	__typeof(((struct kse *)0)->name)


/*
 * Evaluates to the value of the per-kse variable name.
 */
#define	KSD_GET64(name) ({					\
	__ksd_type(name) __result;				\
								\
	u_long __i;						\
	__asm __volatile("movq %%gs:%1, %0"			\
	    : "=r" (__i)					\
	    : "m" (*(u_long *)(__ksd_offset(name))));		\
	__result = *(__ksd_type(name) *)&__i;			\
								\
	__result;						\
})

/*
 * Sets the value of the per-cpu variable name to value val.
 */
#define	KSD_SET64(name, val) ({					\
	__ksd_type(name) __val = (val);				\
								\
	u_long __i;						\
	__i = *(u_long *)&__val;					\
	__asm __volatile("movq %1,%%gs:%0"			\
	    : "=m" (*(u_long *)(__ksd_offset(name)))		\
	    : "r" (__i));					\
})

static __inline u_long
__ksd_readandclear64(volatile u_long *addr)
{
	u_long result;

	__asm __volatile (
	    "	xorq	%0, %0;"
	    "	xchgq	%%gs:%1, %0;"
	    "# __ksd_readandclear64"
	    : "=&r" (result)
	    : "m" (*addr));
	return (result);
}

#define	KSD_READANDCLEAR64(name) ({				\
	__ksd_type(name) __result;				\
								\
	__result = (__ksd_type(name))				\
	    __ksd_readandclear64((u_long *)__ksd_offset(name)); \
	__result;						\
})


#define	_ksd_curkse()		((struct kse *)KSD_GET64(k_mbx.km_udata))
#define	_ksd_curthread()	KSD_GET64(k_curthread)
#define	_ksd_get_tmbx()		KSD_GET64(k_mbx.km_curthread)
#define	_ksd_set_tmbx(value)	KSD_SET64(k_mbx.km_curthread, (void *)value);
#define	_ksd_readandclear_tmbx() KSD_READANDCLEAR64(k_mbx.km_curthread)


static __inline int
_ksd_create(struct ksd *ksd, void *base, int size)
{
	ksd->base = base;
	ksd->size = size;
	return (0);
}

static __inline void
_ksd_destroy(struct ksd *ksd)
{
	ksd->base = 0;
	ksd->size = 0;
}

static __inline int
_ksd_setprivate(struct ksd *ksd)
{
	return (sysarch(AMD64_SET_GSBASE, &ksd->base));
}

#endif
