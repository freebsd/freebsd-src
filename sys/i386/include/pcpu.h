/*-
 * Copyright (c) Peter Wemm <peter@netplex.com.au>
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

#ifndef _MACHINE_PCPU_H_
#define _MACHINE_PCPU_H_

#ifdef _KERNEL

#ifndef	__GNUC__

#ifndef	lint
#error	gcc or lint is required to use this file
#else /* lint */
#define	__PCPU_PTR(name)
#define	__PCPU_GET(name)
#define	__PCPU_SET(name, val)
#define	PCPU_GET(member)	__PCPU_GET(pc_ ## member)
#define	PCPU_PTR(member)	__PCPU_PTR(pc_ ## member)
#define	PCPU_SET(member, val)	__PCPU_SET(pc_ ## member, val)
#define	PCPU_MD_FIELDS		\
	int foo;		\
	char bar
#endif /* lint */

#else	/* __GNUC__ */

#include <machine/segments.h>
#include <machine/tss.h>

/*
 * The SMP parts are setup in pmap.c and locore.s for the BSP, and
 * mp_machdep.c sets up the data for the AP's to "see" when they awake.
 * The reason for doing it via a struct is so that an array of pointers
 * to each CPU's data can be set up for things like "check curproc on all
 * other processors"
 */
#define	PCPU_MD_FIELDS							\
	struct	pcpu *pc_prvspace;		/* Self-reference */	\
	struct	i386tss pc_common_tss;					\
	struct	segment_descriptor pc_common_tssd;			\
	struct	segment_descriptor *pc_tss_gdt;				\
	int	pc_currentldt;						\
	u_int32_t pc_int_pending;	/* master int pending flag */   \
	u_int32_t pc_ipending;	/* pending slow interrupts */		\
	u_int32_t pc_fpending;	/* pending fast interrupts */		\
	u_int32_t pc_spending 	/* pending soft interrupts */

/*
 * Evaluates to the byte offset of the per-cpu variable name.
 */
#define	__pcpu_offset(name)						\
	__offsetof(struct pcpu, name)

/*
 * Evaluates to the type of the per-cpu variable name.
 */
#define	__pcpu_type(name)						\
	__typeof(((struct pcpu *)0)->name)

/*
 * Evaluates to the address of the per-cpu variable name.
 */
#define	__PCPU_PTR(name) ({						\
	__pcpu_type(name) *__p;						\
									\
	__asm __volatile("movl %%fs:%1,%0; addl %2,%0"			\
	    : "=r" (__p)						\
	    : "m" (*(struct pcpu *)(__pcpu_offset(pc_prvspace))),	\
	      "i" (__pcpu_offset(name)));				\
									\
	__p;								\
})

/*
 * Evaluates to the value of the per-cpu variable name.
 */
#define	__PCPU_GET(name) ({						\
	__pcpu_type(name) __result;					\
									\
	if (sizeof(__result) == 1) {					\
		u_char __b;						\
		__asm __volatile("movb %%fs:%1,%0"			\
		    : "=r" (__b)					\
		    : "m" (*(u_char *)(__pcpu_offset(name))));		\
		__result = *(__pcpu_type(name) *)&__b;			\
	} else if (sizeof(__result) == 2) {				\
		u_short __w;						\
		__asm __volatile("movw %%fs:%1,%0"			\
		    : "=r" (__w)					\
		    : "m" (*(u_short *)(__pcpu_offset(name))));		\
		__result = *(__pcpu_type(name) *)&__w;			\
	} else if (sizeof(__result) == 4) {				\
		u_int __i;						\
		__asm __volatile("movl %%fs:%1,%0"			\
		    : "=r" (__i)					\
		    : "m" (*(u_int *)(__pcpu_offset(name))));		\
		__result = *(__pcpu_type(name) *)&__i;			\
	} else {							\
		__result = *__PCPU_PTR(name);				\
	}								\
									\
	__result;							\
})

/*
 * Sets the value of the per-cpu variable name to value val.
 */
#define	__PCPU_SET(name, val) ({					\
	__pcpu_type(name) __val = (val);				\
									\
	if (sizeof(__val) == 1) {					\
		u_char __b;						\
		__b = *(u_char *)&__val;				\
		__asm __volatile("movb %1,%%fs:%0"			\
		    : "=m" (*(u_char *)(__pcpu_offset(name)))		\
		    : "r" (__b));					\
	} else if (sizeof(__val) == 2) {				\
		u_short __w;						\
		__w = *(u_short *)&__val;				\
		__asm __volatile("movw %1,%%fs:%0"			\
		    : "=m" (*(u_short *)(__pcpu_offset(name)))		\
		    : "r" (__w));					\
	} else if (sizeof(__val) == 4) {				\
		u_int __i;						\
		__i = *(u_int *)&__val;					\
		__asm __volatile("movl %1,%%fs:%0"			\
		    : "=m" (*(u_int *)(__pcpu_offset(name)))		\
		    : "r" (__i));					\
	} else {							\
		*__PCPU_PTR(name) = __val;				\
	}								\
})

#define	PCPU_GET(member)	__PCPU_GET(pc_ ## member)
#define	PCPU_PTR(member)	__PCPU_PTR(pc_ ## member)
#define	PCPU_SET(member, val)	__PCPU_SET(pc_ ## member, val)

#endif	/* __GNUC__ */

#endif	/* _KERNEL */

#endif	/* ! _MACHINE_PCPU_H_ */
