/*
 * Copyright (c) 2005 David Xu <davidxu@freebsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
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
 * $FreeBSD$
 *
 */

#include "namespace.h"
#include <errno.h>
#include "un-namespace.h"
#include "thr_private.h"

int _pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr,
	int *pshared);
int _pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared);


__weak_reference(_pthread_mutexattr_getpshared, pthread_mutexattr_getpshared);
__weak_reference(_pthread_mutexattr_setpshared, pthread_mutexattr_setpshared);

int
_pthread_mutexattr_getpshared(const pthread_mutexattr_t *attr,
	int *pshared)
{
	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	pshared = PTHREAD_PROCESS_PRIVATE;
	return (0);
}

int
_pthread_mutexattr_setpshared(pthread_mutexattr_t *attr, int pshared)
{
	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	if  (pshared != PTHREAD_PROCESS_PRIVATE)
		return (EINVAL);
	return (0);
}
