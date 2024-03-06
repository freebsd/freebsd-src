/*-
 * SPDX-License-Identifier: BSD-4-Clause
 *
 * Copyright (c) 2003 Craig Rodrigues <rodrigc@attbi.com>.
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
 *	This product includes software developed by Craig Rodrigues.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CRAIG RODRIGUES AND CONTRIBUTORS ``AS IS'' AND
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
 */

/*
 * Copyright (c) 1998 Daniel Eischen <eischen@vigrid.com>.
 * Copyright (C) 2001 Jason Evans <jasone@freebsd.org>.
 * Copyright (c) 2002,2003 Alexey Zelkin <phantom@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice(s), this list of conditions and the following disclaimer
 *    unmodified other than the allowable addition of one or more
 *    copyright notices.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice(s), this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER(S) ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDER(S) BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE
 * OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1996 John Birrell <jb@cimlogic.com.au>.
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
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
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
 */

#include "namespace.h"
#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <pthread_np.h>
#include <sys/sysctl.h>
#include "un-namespace.h"

#include "thr_private.h"

static size_t	_get_kern_cpuset_size(void);

__weak_reference(_thr_attr_destroy, _pthread_attr_destroy);
__weak_reference(_thr_attr_destroy, pthread_attr_destroy);

int
_thr_attr_destroy(pthread_attr_t *attr)
{

	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	free((*attr)->cpuset);
	free(*attr);
	*attr = NULL;
	return (0);
}

__weak_reference(_thr_attr_get_np, pthread_attr_get_np);
__weak_reference(_thr_attr_get_np, _pthread_attr_get_np);

int
_thr_attr_get_np(pthread_t pthread, pthread_attr_t *dstattr)
{
	struct pthread_attr *dst;
	struct pthread *curthread;
	cpuset_t *cpuset;
	size_t kern_size;
	int error;

	if (pthread == NULL || dstattr == NULL || (dst = *dstattr) == NULL)
		return (EINVAL);

	kern_size = _get_kern_cpuset_size();
	if (dst->cpuset == NULL) {
		if ((cpuset = malloc(kern_size)) == NULL)
			return (ENOMEM);
	} else
		cpuset = dst->cpuset;

	curthread = _get_curthread();
	/* Arg 0 is to include dead threads. */
	if ((error = _thr_find_thread(curthread, pthread, 0)) != 0)
		goto free_and_exit;

	error = cpuset_getaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, TID(pthread),
	    kern_size, cpuset);
	if (error == -1) {
		THR_THREAD_UNLOCK(curthread, pthread);
		error = errno;
		goto free_and_exit;
	}

	/*
	 * From this point on, we can't fail, so we can start modifying 'dst'.
	 */

	*dst = pthread->attr;
	if ((pthread->flags & THR_FLAGS_DETACHED) != 0)
		dst->flags |= PTHREAD_DETACHED;

	THR_THREAD_UNLOCK(curthread, pthread);

	dst->cpuset = cpuset;
	dst->cpusetsize = kern_size;
	return (0);

free_and_exit:
	if (dst->cpuset == NULL)
		free(cpuset);
	return (error);
}

__weak_reference(_thr_attr_getdetachstate, pthread_attr_getdetachstate);
__weak_reference(_thr_attr_getdetachstate, _pthread_attr_getdetachstate);

int
_thr_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{

	if (attr == NULL || *attr == NULL || detachstate == NULL)
		return (EINVAL);

	if (((*attr)->flags & PTHREAD_DETACHED) != 0)
		*detachstate = PTHREAD_CREATE_DETACHED;
	else
		*detachstate = PTHREAD_CREATE_JOINABLE;
	return (0);
}

__weak_reference(_thr_attr_getguardsize, pthread_attr_getguardsize);
__weak_reference(_thr_attr_getguardsize, _pthread_attr_getguardsize);

int
_thr_attr_getguardsize(const pthread_attr_t * __restrict attr,
    size_t * __restrict guardsize)
{

	if (attr == NULL || *attr == NULL || guardsize == NULL)
		return (EINVAL);

	*guardsize = (*attr)->guardsize_attr;
	return (0);
}

__weak_reference(_thr_attr_getinheritsched, pthread_attr_getinheritsched);
__weak_reference(_thr_attr_getinheritsched, _pthread_attr_getinheritsched);

int
_thr_attr_getinheritsched(const pthread_attr_t * __restrict attr,
    int * __restrict sched_inherit)
{

	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	*sched_inherit = (*attr)->sched_inherit;
	return (0);
}

__weak_reference(_thr_attr_getschedparam, pthread_attr_getschedparam);
__weak_reference(_thr_attr_getschedparam, _pthread_attr_getschedparam);

int
_thr_attr_getschedparam(const pthread_attr_t * __restrict attr,
    struct sched_param * __restrict param)
{

	if (attr == NULL || *attr == NULL || param == NULL)
		return (EINVAL);

	param->sched_priority = (*attr)->prio;
	return (0);
}

__weak_reference(_thr_attr_getschedpolicy, pthread_attr_getschedpolicy);
__weak_reference(_thr_attr_getschedpolicy, _pthread_attr_getschedpolicy);

int
_thr_attr_getschedpolicy(const pthread_attr_t * __restrict attr,
    int * __restrict policy)
{

	if (attr == NULL || *attr == NULL || policy == NULL)
		return (EINVAL);

	*policy = (*attr)->sched_policy;
	return (0);
}

__weak_reference(_thr_attr_getscope, pthread_attr_getscope);
__weak_reference(_thr_attr_getscope, _pthread_attr_getscope);

int
_thr_attr_getscope(const pthread_attr_t * __restrict attr,
    int * __restrict contentionscope)
{

	if (attr == NULL || *attr == NULL || contentionscope == NULL)
		return (EINVAL);

	*contentionscope = ((*attr)->flags & PTHREAD_SCOPE_SYSTEM) != 0 ?
	    PTHREAD_SCOPE_SYSTEM : PTHREAD_SCOPE_PROCESS;
	return (0);
}

__weak_reference(_pthread_attr_getstack, pthread_attr_getstack);

int
_pthread_attr_getstack(const pthread_attr_t * __restrict attr,
    void ** __restrict stackaddr, size_t * __restrict stacksize)
{

	if (attr == NULL || *attr == NULL || stackaddr == NULL ||
	    stacksize == NULL)
		return (EINVAL);

	*stackaddr = (*attr)->stackaddr_attr;
	*stacksize = (*attr)->stacksize_attr;
	return (0);
}

__weak_reference(_thr_attr_getstackaddr, pthread_attr_getstackaddr);
__weak_reference(_thr_attr_getstackaddr, _pthread_attr_getstackaddr);

int
_thr_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{

	if (attr == NULL || *attr == NULL || stackaddr == NULL)
		return (EINVAL);

	*stackaddr = (*attr)->stackaddr_attr;
	return (0);
}

__weak_reference(_thr_attr_getstacksize, pthread_attr_getstacksize);
__weak_reference(_thr_attr_getstacksize, _pthread_attr_getstacksize);

int
_thr_attr_getstacksize(const pthread_attr_t * __restrict attr,
    size_t * __restrict stacksize)
{

	if (attr == NULL || *attr == NULL || stacksize == NULL)
		return (EINVAL);

	*stacksize = (*attr)->stacksize_attr;
	return (0);
}

__weak_reference(_thr_attr_init, pthread_attr_init);
__weak_reference(_thr_attr_init, _pthread_attr_init);

int
_thr_attr_init(pthread_attr_t *attr)
{
	pthread_attr_t pattr;

	_thr_check_init();

	if ((pattr = malloc(sizeof(*pattr))) == NULL)
		return (ENOMEM);

	memcpy(pattr, &_pthread_attr_default, sizeof(*pattr));
	*attr = pattr;
	return (0);
}

__weak_reference(_pthread_attr_setcreatesuspend_np,			\
    pthread_attr_setcreatesuspend_np);

int
_pthread_attr_setcreatesuspend_np(pthread_attr_t *attr)
{

	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	(*attr)->suspend = THR_CREATE_SUSPENDED;
	return (0);
}

__weak_reference(_thr_attr_setdetachstate, pthread_attr_setdetachstate);
__weak_reference(_thr_attr_setdetachstate, _pthread_attr_setdetachstate);

int
_thr_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{

	if (attr == NULL || *attr == NULL ||
	    (detachstate != PTHREAD_CREATE_DETACHED &&
	    detachstate != PTHREAD_CREATE_JOINABLE))
		return (EINVAL);

	if (detachstate == PTHREAD_CREATE_DETACHED)
		(*attr)->flags |= PTHREAD_DETACHED;
	else
		(*attr)->flags &= ~PTHREAD_DETACHED;
	return (0);
}

__weak_reference(_thr_attr_setguardsize, pthread_attr_setguardsize);
__weak_reference(_thr_attr_setguardsize, _pthread_attr_setguardsize);

int
_thr_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{

	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	(*attr)->guardsize_attr = guardsize;
	return (0);
}

__weak_reference(_thr_attr_setinheritsched, pthread_attr_setinheritsched);
__weak_reference(_thr_attr_setinheritsched, _pthread_attr_setinheritsched);

int
_thr_attr_setinheritsched(pthread_attr_t *attr, int sched_inherit)
{

	if (attr == NULL || *attr == NULL ||
	    (sched_inherit != PTHREAD_INHERIT_SCHED &&
	    sched_inherit != PTHREAD_EXPLICIT_SCHED))
		return (EINVAL);

	(*attr)->sched_inherit = sched_inherit;
	return (0);
}

__weak_reference(_thr_attr_setschedparam, pthread_attr_setschedparam);
__weak_reference(_thr_attr_setschedparam, _pthread_attr_setschedparam);

int
_thr_attr_setschedparam(pthread_attr_t * __restrict attr,
    const struct sched_param * __restrict param)
{
	int policy;

	if (attr == NULL || *attr == NULL || param == NULL)
		return (EINVAL);

	policy = (*attr)->sched_policy;

	if (policy == SCHED_FIFO || policy == SCHED_RR) {
		if (param->sched_priority < _thr_priorities[policy-1].pri_min ||
		    param->sched_priority > _thr_priorities[policy-1].pri_max)
			return (EINVAL);
	} else {
		/*
		 * Ignore it for SCHED_OTHER now, patches for glib ports
		 * are wrongly using M:N thread library's internal macro
		 * THR_MIN_PRIORITY and THR_MAX_PRIORITY.
		 */
	}

	(*attr)->prio = param->sched_priority;

	return (0);
}

__weak_reference(_thr_attr_setschedpolicy, pthread_attr_setschedpolicy);
__weak_reference(_thr_attr_setschedpolicy, _pthread_attr_setschedpolicy);

int
_thr_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{

	if (attr == NULL || *attr == NULL ||
	    policy < SCHED_FIFO || policy > SCHED_RR)
		return (EINVAL);

	(*attr)->sched_policy = policy;
	(*attr)->prio = _thr_priorities[policy-1].pri_default;
	return (0);
}

__weak_reference(_thr_attr_setscope, pthread_attr_setscope);
__weak_reference(_thr_attr_setscope, _pthread_attr_setscope);

int
_thr_attr_setscope(pthread_attr_t *attr, int contentionscope)
{

	if (attr == NULL || *attr == NULL ||
	    (contentionscope != PTHREAD_SCOPE_PROCESS &&
	    contentionscope != PTHREAD_SCOPE_SYSTEM))
		return (EINVAL);

	if (contentionscope == PTHREAD_SCOPE_SYSTEM)
		(*attr)->flags |= PTHREAD_SCOPE_SYSTEM;
	else
		(*attr)->flags &= ~PTHREAD_SCOPE_SYSTEM;
	return (0);
}

__weak_reference(_pthread_attr_setstack, pthread_attr_setstack);

int
_pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr,
    size_t stacksize)
{

	if (attr == NULL || *attr == NULL || stackaddr == NULL ||
	    stacksize < PTHREAD_STACK_MIN)
		return (EINVAL);

	(*attr)->stackaddr_attr = stackaddr;
	(*attr)->stacksize_attr = stacksize;
	return (0);
}

__weak_reference(_thr_attr_setstackaddr, pthread_attr_setstackaddr);
__weak_reference(_thr_attr_setstackaddr, _pthread_attr_setstackaddr);

int
_thr_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{

	if (attr == NULL || *attr == NULL || stackaddr == NULL)
		return (EINVAL);

	(*attr)->stackaddr_attr = stackaddr;
	return (0);
}

__weak_reference(_thr_attr_setstacksize, pthread_attr_setstacksize);
__weak_reference(_thr_attr_setstacksize, _pthread_attr_setstacksize);

int
_thr_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{

	if (attr == NULL || *attr == NULL || stacksize < PTHREAD_STACK_MIN)
		return (EINVAL);

	(*attr)->stacksize_attr = stacksize;
	return (0);
}

static size_t
_get_kern_cpuset_size(void)
{
	static int kern_cpuset_size = 0;

	if (kern_cpuset_size == 0) {
		size_t len;

		len = sizeof(kern_cpuset_size);
		if (sysctlbyname("kern.sched.cpusetsizemin", &kern_cpuset_size,
		    &len, NULL, 0) != 0 &&
		    sysctlbyname("kern.sched.cpusetsize", &kern_cpuset_size,
		    &len, NULL, 0) != 0)
			PANIC("failed to get sysctl kern.sched.cpusetsize");
	}

	return (kern_cpuset_size);
}

__weak_reference(_pthread_attr_setaffinity_np, pthread_attr_setaffinity_np);

int
_pthread_attr_setaffinity_np(pthread_attr_t *pattr, size_t cpusetsize,
    const cpuset_t *cpusetp)
{
	pthread_attr_t attr;
	size_t kern_size;

	if (pattr == NULL || (attr = (*pattr)) == NULL)
		return (EINVAL);

	if (cpusetsize == 0 || cpusetp == NULL) {
		if (attr->cpuset != NULL) {
			free(attr->cpuset);
			attr->cpuset = NULL;
			attr->cpusetsize = 0;
		}
		return (0);
	}

	kern_size = _get_kern_cpuset_size();
	/* Kernel rejects small set, we check it here too. */
	if (cpusetsize < kern_size)
		return (ERANGE);
	if (cpusetsize > kern_size) {
		/* Kernel checks invalid bits, we check it here too. */
		size_t i;

		for (i = kern_size; i < cpusetsize; ++i)
			if (((const char *)cpusetp)[i] != 0)
				return (EINVAL);
	}
	if (attr->cpuset == NULL) {
		attr->cpuset = malloc(kern_size);
		if (attr->cpuset == NULL)
			return (errno);
		attr->cpusetsize = kern_size;
	}
	memcpy(attr->cpuset, cpusetp, kern_size);
	return (0);
}

__weak_reference(_pthread_attr_getaffinity_np, pthread_attr_getaffinity_np);

int
_pthread_attr_getaffinity_np(const pthread_attr_t *pattr, size_t cpusetsize,
    cpuset_t *cpusetp)
{
	pthread_attr_t attr;

	if (pattr == NULL || (attr = (*pattr)) == NULL)
		return (EINVAL);

	/* Kernel rejects small set, we check it here too. */
	size_t kern_size = _get_kern_cpuset_size();
	if (cpusetsize < kern_size)
		return (ERANGE);
	if (attr->cpuset != NULL)
		memcpy(cpusetp, attr->cpuset, MIN(cpusetsize,
		    attr->cpusetsize));
	else
		memset(cpusetp, -1, kern_size);
	if (cpusetsize > kern_size)
		memset(((char *)cpusetp) + kern_size, 0,
		    cpusetsize - kern_size);
	return (0);
}
