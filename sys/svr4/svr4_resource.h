/*	Derived from:
 *	$NetBSD: svr4_resource.h,v 1.1 1998/11/28 21:53:02 christos Exp $	*/

/*-
 * Original copyright:
 *
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Christos Zoulas.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 * 
 * $FreeBSD: src/sys/svr4/svr4_resource.h,v 1.3 1999/08/28 00:51:19 peter Exp $
 */

/*
 * Portions of this code derived from software contributed to the
 * FreeBSD Project by Mark Newton.
 * 
 * Copyright (c) 1999 Mark Newton
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
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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

#ifndef	_SVR4_RESOURCE_H_
#define	_SVR4_RESOURCE_H_

#define	SVR4_RLIMIT_CPU		0
#define	SVR4_RLIMIT_FSIZE	1
#define	SVR4_RLIMIT_DATA	2
#define	SVR4_RLIMIT_STACK	3
#define	SVR4_RLIMIT_CORE	4
#define	SVR4_RLIMIT_NOFILE	5
#define	SVR4_RLIMIT_VMEM	6
#define	SVR4_RLIMIT_AS		SVR4_RLIMIT_VMEM
#define	SVR4_RLIM_NLIMITS	7

typedef u_int32_t svr4_rlim_t;

#define	SVR4_RLIM_SAVED_CUR	0x7ffffffd
#define	SVR4_RLIM_SAVED_MAX	0x7ffffffe
#define	SVR4_RLIM_INFINITY	0x7fffffff

struct svr4_rlimit {
	svr4_rlim_t	rlim_cur;
	svr4_rlim_t	rlim_max;
};

typedef u_int64_t svr4_rlim64_t;

#define	SVR4_RLIM64_SAVED_CUR	((svr4_rlim64_t) -1)
#define	SVR4_RLIM64_SAVED_MAX	((svr4_rlim64_t) -2)
#define	SVR4_RLIM64_INFINITY	((svr4_rlim64_t) -3)

struct svr4_rlimit64 {
	svr4_rlim64_t	rlim_cur;
	svr4_rlim64_t	rlim_max;
};

#endif /* !_SVR4_RESOURCE_H_ */
