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
 * $FreeBSD: src/sys/amd64/include/sysarch.h,v 1.24.18.1 2008/11/25 02:59:29 kensmith Exp $
 */

/*
 * Architecture specific syscalls (AMD64)
 */
#ifndef _MACHINE_SYSARCH_H_
#define _MACHINE_SYSARCH_H_

#define	I386_GET_FSBASE		7
#define	I386_SET_FSBASE		8
#define	I386_GET_GSBASE		9
#define	I386_SET_GSBASE		10

/* Leave space for 0-127 for to avoid translating syscalls */
#define	AMD64_GET_FSBASE	128
#define	AMD64_SET_FSBASE	129
#define	AMD64_GET_GSBASE	130
#define	AMD64_SET_GSBASE	131

#ifndef _KERNEL
#include <sys/cdefs.h>

__BEGIN_DECLS
int amd64_get_fsbase(void **);
int amd64_get_gsbase(void **);
int amd64_set_fsbase(void *);
int amd64_set_gsbase(void *);
int sysarch(int, void *);
__END_DECLS
#endif

#endif /* !_MACHINE_SYSARCH_H_ */
