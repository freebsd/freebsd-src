/*-
 * Copyright (c) 2004 Michael Telahun Makonnen <mtm@FreeBSD.Org>
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

#include <pthread.h>
#include <stdlib.h>

#include "thr_private.h"

__weak_reference(_pthread_barrierattr_destroy, pthread_barrierattr_destroy);
__weak_reference(_pthread_barrierattr_init, pthread_barrierattr_init);
__weak_reference(_pthread_barrierattr_getpshared,
    pthread_barrierattr_getpshared);
__weak_reference(_pthread_barrierattr_setpshared,
    pthread_barrierattr_setpshared);

int
_pthread_barrierattr_destroy(pthread_barrierattr_t *attr)
{
	if (*attr == NULL)
		return (EINVAL);
	free(*attr);
	*attr = NULL;
	return (0);
}

int
_pthread_barrierattr_init(pthread_barrierattr_t *attr)
{
	*attr =
	    (pthread_barrierattr_t)malloc(sizeof(struct pthread_barrierattr));
	if ((*attr) == NULL)
		return (ENOMEM);
	(*attr)->ba_pshared = PTHREAD_PROCESS_PRIVATE;
	return (0);
}

int
_pthread_barrierattr_getpshared(const pthread_barrierattr_t *attr, int *pshared)
{
	if (*attr == NULL)
		return (EINVAL);
	*pshared = (*attr)->ba_pshared;
	return (0);
}

int
_pthread_barrierattr_setpshared(pthread_barrierattr_t *attr, int pshared)
{
	if (*attr == NULL || (pshared != PTHREAD_PROCESS_PRIVATE &&
	    pshared != PTHREAD_PROCESS_SHARED))
		return (EINVAL);
	(*attr)->ba_pshared = pshared;
	return (0);
}
