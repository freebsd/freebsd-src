/*-
 * Copyright (c) 1999 Luoqi Chen <luoqi@freebsd.org>
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
 * $FreeBSD: src/sys/i386/include/globals.h,v 1.5.2.1 2000/05/16 06:58:10 dillon Exp $
 */

#ifndef	_MACHINE_GLOBALS_H_
#define	_MACHINE_GLOBALS_H_

#ifdef _KERNEL

#define	GLOBAL_LVALUE(name, type) \
	(*(type *)_global_ptr_##name())
	
#define	GLOBAL_RVALUE(name, type) \
	((type)_global_##name())

/* non-volatile version */
#define	GLOBAL_LVALUE_NV(name, type) \
	(*(type *)_global_ptr_##name##_nv())
	
#define	GLOBAL_RVALUE_NV(name, type) \
	((type)_global_##name##_nv())

#define	GLOBAL_FUNC(name) \
	static __inline void *_global_ptr_##name(void) { \
		void *val; \
		__asm __volatile("movl $gd_" #name ",%0;" \
			"addl %%fs:globaldata,%0" : "=r" (val)); \
		return (val); \
	} \
	static __inline void *_global_ptr_##name##_nv(void) { \
		void *val; \
		__asm("movl $gd_" #name ",%0;" \
			"addl %%fs:globaldata,%0" : "=r" (val)); \
		return (val); \
	} \
	static __inline int _global_##name(void) { \
		int val; \
		__asm __volatile("movl %%fs:gd_" #name ",%0" : "=r" (val)); \
		return (val); \
	} \
	static __inline int _global_##name##_nv(void) { \
		int val; \
		__asm("movl %%fs:gd_" #name ",%0" : "=r" (val)); \
		return (val); \
	} \
	static __inline void _global_##name##_set(int val) { \
		__asm __volatile("movl %0,%%fs:gd_" #name : : "r" (val)); \
	} \
	static __inline void _global_##name##_set_nv(int val) { \
		__asm("movl %0,%%fs:gd_" #name : : "r" (val)); \
	}

#if defined(SMP) || defined(KLD_MODULE) || defined(ACTUALLY_LKM_NOT_KERNEL)
/*
 * The following set of macros works for UP kernel as well, but for maximum
 * performance we allow the global variables to be accessed directly. On the
 * other hand, kernel modules should always use these macros to maintain
 * portability between UP and SMP kernels.
 */
#define	curproc		GLOBAL_RVALUE_NV(curproc, struct proc *)
#define	curpcb		GLOBAL_RVALUE_NV(curpcb, struct pcb *)
#define	npxproc		GLOBAL_LVALUE(npxproc, struct proc *)
#define	common_tss	GLOBAL_LVALUE(common_tss, struct i386tss)
#define	switchtime	GLOBAL_LVALUE(switchtime, struct timeval)
#define	switchticks	GLOBAL_LVALUE(switchticks, int)

#define	common_tssd	GLOBAL_LVALUE(common_tssd, struct segment_descriptor)
#define	tss_gdt		GLOBAL_LVALUE(tss_gdt, struct segment_descriptor *)
#define	astpending	GLOBAL_LVALUE(astpending, u_int)

#ifdef USER_LDT
#define	currentldt	GLOBAL_LVALUE(currentldt, int)
#endif

#ifdef SMP
#define	cpuid		GLOBAL_RVALUE(cpuid, u_int)
#define	other_cpus	GLOBAL_LVALUE(other_cpus, u_int)
#define	inside_intr	GLOBAL_LVALUE(inside_intr, int)
#define	prv_CMAP1	GLOBAL_LVALUE(prv_CMAP1, pt_entry_t *)
#define	prv_CMAP2	GLOBAL_LVALUE(prv_CMAP2, pt_entry_t *)
#define	prv_CMAP3	GLOBAL_LVALUE(prv_CMAP3, pt_entry_t *)
#define	prv_PMAP1	GLOBAL_LVALUE(prv_PMAP1, pt_entry_t *)
#define	prv_CADDR1	GLOBAL_RVALUE(prv_CADDR1, caddr_t)
#define	prv_CADDR2	GLOBAL_RVALUE(prv_CADDR2, caddr_t)
#define	prv_CADDR3	GLOBAL_RVALUE(prv_CADDR3, caddr_t)
#define	prv_PADDR1	GLOBAL_RVALUE(prv_PADDR1, unsigned *)
#endif
#endif	/*UP kernel*/

GLOBAL_FUNC(curproc)
GLOBAL_FUNC(astpending)
GLOBAL_FUNC(curpcb)
GLOBAL_FUNC(npxproc)
GLOBAL_FUNC(common_tss)
GLOBAL_FUNC(switchtime)
GLOBAL_FUNC(switchticks)

GLOBAL_FUNC(common_tssd)
GLOBAL_FUNC(tss_gdt)

#ifdef USER_LDT
GLOBAL_FUNC(currentldt)
#endif

#ifdef SMP
GLOBAL_FUNC(cpuid)
GLOBAL_FUNC(other_cpus)
GLOBAL_FUNC(inside_intr)
GLOBAL_FUNC(prv_CMAP1)
GLOBAL_FUNC(prv_CMAP2)
GLOBAL_FUNC(prv_CMAP3)
GLOBAL_FUNC(prv_PMAP1)
GLOBAL_FUNC(prv_CADDR1)
GLOBAL_FUNC(prv_CADDR2)
GLOBAL_FUNC(prv_CADDR3)
GLOBAL_FUNC(prv_PADDR1)
#endif

#define	SET_CURPROC(x)	(_global_curproc_set_nv((int)x))

#endif	/* _KERNEL */

#endif	/* !_MACHINE_GLOBALS_H_ */
