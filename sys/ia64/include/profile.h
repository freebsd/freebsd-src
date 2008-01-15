/*-
 * Copyright (c) 2004 Marcel Moolenaar
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 * $FreeBSD: src/sys/ia64/include/profile.h,v 1.12 2005/01/06 22:18:23 imp Exp $
 */

#ifndef _MACHINE_PROFILE_H_
#define	_MACHINE_PROFILE_H_

#define	_MCOUNT_DECL	void __mcount
#define	MCOUNT

#define	FUNCTION_ALIGNMENT	32

typedef unsigned long	fptrdiff_t;

#ifdef _KERNEL
/*
 * The following two macros do splhigh and splx respectively.
 */
#define	MCOUNT_ENTER(s)	s = intr_disable()
#define	MCOUNT_EXIT(s)	intr_restore(s)
#define	MCOUNT_DECL(s)	register_t s;

void bintr(void);
void btrap(void);
void eintr(void);
void user(void);

#define	MCOUNT_FROMPC_USER(pc)		\
	((pc < (uintfptr_t)VM_MAXUSER_ADDRESS) ? ~0UL : pc)

#define	MCOUNT_FROMPC_INTR(pc)		(~0UL)

_MCOUNT_DECL(uintfptr_t, uintfptr_t);

#else /* !_KERNEL */

typedef unsigned long	uintfptr_t;

#endif

#endif /* _MACHINE_PROFILE_H_ */
