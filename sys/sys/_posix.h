#ifndef _SYS__POSIX_H_
#define _SYS__POSIX_H_

/*-
 * Copyright (c) 1998 HD Associates, Inc.
 * All rights reserved.
 * contact: dufault@hda.com
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

/*
 * This is a stand alone header file to set up for feature specification
 * defined to take place before the inclusion of any standard header.
 * It should only handle pre-processor defines.
 *
 * See section B.2.7 of 1003.1b-1993 
 *
 */

#ifdef _KERNEL

#if !defined(KLD_MODULE)
#include "opt_posix.h"
#endif

/* Only kern_mib.c uses _POSIX_VERSION.  Introduce a kernel
 * one to avoid other pieces of the kernel getting dependant
 * on that.
 * XXX Complain if you think this dumb.
 */

/* Make P1003 structures visible for the kernel if
 * the P1003_1B option is in effect.
 */
#ifdef P1003_1B
#define _P1003_1B_VISIBLE
#ifndef _KPOSIX_VERSION
#define	_KPOSIX_VERSION		199309L
#endif
#endif

#ifndef _KPOSIX_VERSION
#define	_KPOSIX_VERSION		199009L
#endif

#define _P1003_1B_VISIBLE_HISTORICALLY

#else

/* Test for visibility of P1003.1B features:
 * If _POSIX_SOURCE and POSIX_C_SOURCE are completely undefined
 * they show up.
 *
 * If they specify a version including P1003.1B then they show up.
 *
 * (Two macros are added to permit hiding new extensions while 
 * keeping historic BSD features - that is not done now)
 *
 */

#if (!defined(_POSIX_SOURCE) && !defined(_POSIX_C_SOURCE)) || \
 (_POSIX_VERSION  >= 199309L && defined(_POSIX_C_SOURCE) && \
  _POSIX_C_SOURCE >= 199309L)
#define _P1003_1B_VISIBLE
#define _P1003_1B_VISIBLE_HISTORICALLY
#endif

#endif /* _KERNEL */
#endif /* _SYS__POSIX_H_ */
