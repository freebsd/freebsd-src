/*	$NetBSD: SYS.h,v 1.19 2009/12/14 01:07:41 matt Exp $ */
/* $FreeBSD$ */

/*-
 * Copyright (c) 1996 Jonathan Stone
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
 *      This product includes software developed by Jonathan Stone for
 *      the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
 *	from: @(#)SYS.h	8.1 (Berkeley) 6/4/93
 */

#include <sys/syscall.h>

#include <machine/asm.h>

/*
 * If compiling for shared libs, Emit sysV ABI PIC segment pseudo-ops.
 *
 * i)  Emit .abicalls before .LEAF entrypoint, and .cpload/.cprestore after.
 * ii) Do interprocedure jumps indirectly via t9, with the side-effect of
 *     preserving the callee's entry address in t9.
 */
#ifndef __CHERI_PURE_CAPABILITY__
#ifdef __ABICALLS__
	.abicalls
# if defined(__mips_o32) || defined(__mips_o64)
#  define PIC_PROLOGUE(x)	SETUP_GP
#  define PIC_TAILCALL(l)	PTR_LA t9, _C_LABEL(l); jr t9
#  define PIC_RETURN()		j ra
# else
#  define PIC_PROLOGUE(x)	SETUP_GP64(t3, x)
#  define PIC_TAILCALL(l)	PTR_LA t9, _C_LABEL(l); RESTORE_GP64; jr t9
#  define PIC_RETURN()		RESTORE_GP64; j ra
# endif
#else
# define PIC_PROLOGUE(x)
# define PIC_TAILCALL(l)	j  _C_LABEL(l)
# define PIC_RETURN()		j ra
#endif /* __ABICALLS__ */
#else /* defined(__CHERI_PURE_CAPABILITY__) */
#ifdef PIC
# define PIC_PROLOGUE(x)
/*
 * XXXAR: current CHERI clang ignores any arithmetic expressions in a statement
 * if it encounters %pc_rel. Therefore we need to manually subtract 8 from
 * t9 (offset between daddiu and cgetpcc) to get the right value.
 *
 * This appears to have been fixed in the latest clang HEAD so we can remove
 * the extra subtraction after it has been merged into CHERI clang.
 *
 * FIXME: if the difference is at a 16 bit boundary this calculation is wrong.
 */
# define PIC_LOAD_CODE_PTR(capreg, gpr, l)		\
	lui		gpr, %pcrel_hi(l);		\
	daddiu		gpr, gpr, %pcrel_lo(l);		\
	daddiu		gpr, gpr, -8;			\
	cgetpcc		capreg;				\
	cincoffset	capreg, capreg, gpr;
# define PIC_TAILCALL(l)				\
	PIC_LOAD_CODE_PTR($c12, t9, _C_LABEL(l))	\
	cjr $c12;
# define PIC_CALL(l)					\
	PIC_LOAD_CODE_PTR($c12, t9, _C_LABEL(l))	\
	cjalr $c12, $c17;				\
	nop;
# define PIC_RETURN()		cjr $c17
#else
# define PIC_PROLOGUE(x)
# define PIC_TAILCALL(l)	j _C_LABEL(l)
# define PIC_CALL(l)				\
	dla			t9, l;		\
	cgetpccsetoffset	$c12, t9;	\
	cjalr			$c12, $c17;	\
	nop
# define PIC_RETURN()		cjr $c17
#endif
#endif /* defined(__CHERI_PURE_CAPABILITY__) */

# define SYSTRAP(x)	li v0,SYS_ ## x; syscall;

/*
 * Do a syscall that cannot fail (sync, get{p,u,g,eu,eg)id)
 */
#define RSYSCALL_NOERROR(x)						\
LEAF(__sys_ ## x);							\
	.weak _C_LABEL(x);						\
	_C_LABEL(x) = _C_LABEL(__CONCAT(__sys_,x));			\
	.weak _C_LABEL(__CONCAT(_,x));					\
	_C_LABEL(__CONCAT(_,x)) = _C_LABEL(__CONCAT(__sys_,x));		\
	SYSTRAP(x);							\
	j ra;								\
END(__sys_ ## x)

/*
 * Do a normal syscall.
 */
#define RSYSCALL(x)							\
LEAF(__sys_ ## x);							\
	.weak _C_LABEL(x);						\
	_C_LABEL(x) = _C_LABEL(__CONCAT(__sys_,x));			\
	.weak _C_LABEL(__CONCAT(_,x));					\
	_C_LABEL(__CONCAT(_,x)) = _C_LABEL(__CONCAT(__sys_,x));		\
	PIC_PROLOGUE(__sys_ ## x);					\
	SYSTRAP(x);							\
	bne a3,zero,err;						\
	PIC_RETURN();							\
err:									\
	PIC_TAILCALL(__cerror);						\
END(__sys_ ## x)

/*
 * Do a renamed or pseudo syscall (e.g., _exit()), where the entrypoint
 * and syscall name are not the same.
 */
#define PSEUDO_NOERROR(x)						\
LEAF(__sys_ ## x);							\
	.weak _C_LABEL(__CONCAT(_,x));					\
	_C_LABEL(__CONCAT(_,x)) = _C_LABEL(__CONCAT(__sys_,x));		\
	SYSTRAP(x);							\
	j ra;								\
END(__sys_ ## x)

#define PSEUDO(x)							\
LEAF(__sys_ ## x);							\
	.weak _C_LABEL(__CONCAT(_,x));					\
	_C_LABEL(__CONCAT(_,x)) = _C_LABEL(__CONCAT(__sys_,x));		\
	PIC_PROLOGUE(__sys_ ## x);					\
	SYSTRAP(x);							\
	bne a3,zero,err;						\
	PIC_RETURN();							\
err:									\
	PIC_TAILCALL(__cerror);						\
END(__sys_ ## x)

/* Do a system call where the _ioctl() is also custom */
#define NO_UNDERSCORE(x)							\
LEAF(__sys_ ## x);							\
	PIC_PROLOGUE(__sys_ ## x);					\
	SYSTRAP(x);							\
	bne a3,zero,err;						\
	PIC_RETURN();							\
err:									\
	PIC_TAILCALL(__cerror);						\
END(__sys_ ## x)
