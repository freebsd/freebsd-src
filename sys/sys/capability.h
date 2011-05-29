/*-
 * Copyright (c) 2008-2010 Robert N. M. Watson
 * All rights reserved.
 *
 * This software was developed at the University of Cambridge Computer
 * Laboratory with support from a grant from Google, Inc.
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
 * Definitions for FreeBSD capabilities facility.
 */
#ifndef _SYS_CAPABILITY_H_
#define	_SYS_CAPABILITY_H_

#include <sys/cdefs.h>
#include <sys/types.h>

#ifdef _KERNEL

#define IN_CAPABILITY_MODE(td) (td->td_ucred->cr_flags & CRED_FLAG_CAPMODE)

#else /* !_KERNEL */

__BEGIN_DECLS

/*
 * cap_enter(): Cause the process to enter capability mode, which will
 * prevent it from directly accessing global namespaces.  System calls will
 * be limited to process-local, process-inherited, or file descriptor
 * operations.  If already in capability mode, a no-op.
 *
 * Currently, process-inherited operations are not properly handled -- in
 * particular, we're interested in things like waitpid(2), kill(2), etc,
 * being properly constrained.  One possible solution is to introduce process
 * descriptors.
 */
int	cap_enter(void);

/*
 * cap_getmode(): Are we in capability mode?
 */
int	cap_getmode(u_int* modep);

__END_DECLS

#endif /* !_KERNEL */

#endif /* !_SYS_CAPABILITY_H_ */
