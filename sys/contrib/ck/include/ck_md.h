/*
 * Copyright 2011-2012 Samy Al Bahra.
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

#ifndef CK_MD_H
#define CK_MD_H

#ifndef CK_MD_CACHELINE
#define CK_MD_CACHELINE (64)
#endif

#ifndef CK_MD_PAGESIZE
#define CK_MD_PAGESIZE (4096)
#endif

#ifndef CK_MD_RTM_DISABLE
#define CK_MD_RTM_DISABLE
#endif /* CK_MD_RTM_DISABLE */

#ifndef CK_MD_POINTER_PACK_DISABLE
#define CK_MD_POINTER_PACK_DISABLE
#endif /* CK_MD_POINTER_PACK_DISABLE */

#ifndef CK_MD_VMA_BITS_UNKNOWN 
#define CK_MD_VMA_BITS_UNKNOWN 
#endif /* CK_MD_VMA_BITS_UNKNOWN */

#ifndef CK_MD_RMO
#define CK_MD_RMO
#endif /* CK_MD_RMO */

#define CK_VERSION "0.5.2"
#define CK_GIT_SHA ""

/*
 * CK expects those, which are normally defined by the build system.
 */
#if defined(__i386__) && !defined(__x86__)
#define __x86__
#elif defined(__sparc64__) && !defined(__sparcv9__)
#define __sparcv9__
#elif defined(__powerpc64__) && !defined(__ppc64__)
#define __ppc64__
#elif defined(__powerpc__) && !defined(__ppc__)
#define __ppc__
#endif
#endif /* CK_MD_H */
