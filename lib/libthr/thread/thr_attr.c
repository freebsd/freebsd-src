/*
 * Copyright (c) 1995-1997 John Birrell <jb@cimlogic.com.au>.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
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
 *
 * $FreeBSD$
 */

/*
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
 * Copyright (c) 2003 Jeff Roberson <jeff@FreeBSD.org>
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

/* XXXTHR I rewrote the entire file, can we lose some of the copyrights? */

#include <sys/param.h>

#include <errno.h>
#include <pthread.h>
#include <pthread_np.h>
#include <stdlib.h>
#include <string.h>

#include "thr_private.h"

__weak_reference(_pthread_attr_destroy, pthread_attr_destroy);
__weak_reference(_pthread_attr_init, pthread_attr_init);
__weak_reference(_pthread_attr_setcreatesuspend_np,
    pthread_attr_setcreatesuspend_np);
__weak_reference(_pthread_attr_setdetachstate, pthread_attr_setdetachstate);
__weak_reference(_pthread_attr_setguardsize, pthread_attr_setguardsize);
__weak_reference(_pthread_attr_setinheritsched, pthread_attr_setinheritsched);
__weak_reference(_pthread_attr_setschedparam, pthread_attr_setschedparam);
__weak_reference(_pthread_attr_setschedpolicy, pthread_attr_setschedpolicy);
__weak_reference(_pthread_attr_setscope, pthread_attr_setscope);
__weak_reference(_pthread_attr_setstack, pthread_attr_setstack);
__weak_reference(_pthread_attr_setstackaddr, pthread_attr_setstackaddr);
__weak_reference(_pthread_attr_setstacksize, pthread_attr_setstacksize);
__weak_reference(_pthread_attr_get_np, pthread_attr_get_np);
__weak_reference(_pthread_attr_getdetachstate, pthread_attr_getdetachstate);
__weak_reference(_pthread_attr_getguardsize, pthread_attr_getguardsize);
__weak_reference(_pthread_attr_getinheritsched, pthread_attr_getinheritsched);
__weak_reference(_pthread_attr_getschedparam, pthread_attr_getschedparam);
__weak_reference(_pthread_attr_getschedpolicy, pthread_attr_getschedpolicy);
__weak_reference(_pthread_attr_getscope, pthread_attr_getscope);
__weak_reference(_pthread_attr_getstack, pthread_attr_getstack);
__weak_reference(_pthread_attr_getstackaddr, pthread_attr_getstackaddr);
__weak_reference(_pthread_attr_getstacksize, pthread_attr_getstacksize);

int
_pthread_attr_init(pthread_attr_t *attr)
{
	pthread_attr_t	pattr;

	if ((pattr = (pthread_attr_t)
	    malloc(sizeof(struct pthread_attr))) == NULL)
		return (ENOMEM);

	memcpy(pattr, &pthread_attr_default, sizeof(struct pthread_attr));
	*attr = pattr;

	return (0);
}

int
_pthread_attr_destroy(pthread_attr_t *attr)
{
	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	free(*attr);
	*attr = NULL;

	return (0);
}

int
_pthread_attr_setcreatesuspend_np(pthread_attr_t *attr)
{
	if (attr == NULL || *attr == NULL) {
		errno = EINVAL;
		return (-1);
	}
	(*attr)->suspend = PTHREAD_CREATE_SUSPENDED;

	return (0);
}

int
_pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
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

int
_pthread_attr_setguardsize(pthread_attr_t *attr, size_t guardsize)
{

	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	(*attr)->guardsize_attr = roundup(guardsize, _pthread_page_size);

	return (0);
}

int
_pthread_attr_setinheritsched(pthread_attr_t *attr, int sched_inherit)
{
	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	(*attr)->sched_inherit = sched_inherit;

	return (0);
}

int
_pthread_attr_setschedparam(pthread_attr_t *attr,
    const struct sched_param *param)
{
	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	if (param == NULL)
		return (ENOTSUP);

	if (param->sched_priority < PTHREAD_MIN_PRIORITY ||
	    param->sched_priority > PTHREAD_MAX_PRIORITY)
		return (ENOTSUP);

	(*attr)->prio = param->sched_priority;

	return (0);
}

int
_pthread_attr_setschedpolicy(pthread_attr_t *attr, int policy)
{
	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	if (policy < SCHED_FIFO || policy > SCHED_RR)
		return (ENOTSUP);

	(*attr)->sched_policy = policy;

	return (0);
}

int
_pthread_attr_setscope(pthread_attr_t *attr, int contentionscope)
{
	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	if (contentionscope != PTHREAD_SCOPE_PROCESS ||
	    contentionscope == PTHREAD_SCOPE_SYSTEM) 
		/* We don't support PTHREAD_SCOPE_SYSTEM. */
		return (ENOTSUP);

	(*attr)->flags |= contentionscope;

	return (0);
}

int
_pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr,
                        size_t stacksize)
{
	if (attr == NULL || *attr == NULL || stackaddr == NULL
	    || stacksize < PTHREAD_STACK_MIN)
		return (EINVAL);

	(*attr)->stackaddr_attr = stackaddr;
	(*attr)->stacksize_attr = stacksize;

	return (0);
}

int
_pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr)
{
	if (attr == NULL || *attr == NULL || stackaddr == NULL)
		return (EINVAL);

	(*attr)->stackaddr_attr = stackaddr;

	return (0);
}

int
_pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	if (attr == NULL || *attr == NULL || stacksize < PTHREAD_STACK_MIN)
		return (EINVAL);

	(*attr)->stacksize_attr = stacksize;

	return (0);
}

int
_pthread_attr_get_np(pthread_t pid, pthread_attr_t *dst)
{
	int	ret;

	if (pid == NULL || dst == NULL || *dst == NULL)
		return (EINVAL);

	if ((ret = _find_thread(pid)) != 0)
		return (ret);

	memcpy(*dst, &pid->attr, sizeof(struct pthread_attr));

	/*
	 * Special case, if stack address was not provided by caller
	 * of pthread_create(), then return address allocated internally
	 */
	if ((*dst)->stackaddr_attr == NULL)
		(*dst)->stackaddr_attr = pid->stack;

	return (0);
}

int
_pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate)
{

	if (attr == NULL || *attr == NULL || detachstate == NULL)
		return (EINVAL);

	/* Check if the detached flag is set: */
	if ((*attr)->flags & PTHREAD_DETACHED)
		*detachstate = PTHREAD_CREATE_DETACHED;
	else
		*detachstate = PTHREAD_CREATE_JOINABLE;

	return (0);
}

int
_pthread_attr_getguardsize(const pthread_attr_t *attr, size_t *guardsize)
{
	if (attr == NULL || *attr == NULL || guardsize == NULL)
		return (EINVAL);

	*guardsize = (*attr)->guardsize_attr;

	return (0);
}

int
_pthread_attr_getinheritsched(const pthread_attr_t *attr, int *sched_inherit)
{
	if (attr == NULL || *attr == NULL)
		return (EINVAL);

	*sched_inherit = (*attr)->sched_inherit;

	return (0);
}

int
_pthread_attr_getschedparam(const pthread_attr_t *attr, struct sched_param *param)
{
	if (attr == NULL || *attr == NULL || param == NULL)
		return (EINVAL);

	param->sched_priority = (*attr)->prio;

	return (0);
}

int
_pthread_attr_getschedpolicy(const pthread_attr_t *attr, int *policy)
{
	if (attr == NULL || *attr == NULL || policy == NULL)
		return (EINVAL);

	*policy = (*attr)->sched_policy;

	return (0);
}

int
_pthread_attr_getscope(const pthread_attr_t *attr, int *contentionscope)
{
	if (attr == NULL || *attr == NULL || contentionscope == NULL)
		return (EINVAL);

	*contentionscope = (*attr)->flags & PTHREAD_SCOPE_SYSTEM ?
	    PTHREAD_SCOPE_SYSTEM : PTHREAD_SCOPE_PROCESS;

	return (0);
}

int
_pthread_attr_getstack(const pthread_attr_t * __restrict attr,
                        void ** __restrict stackaddr,
                        size_t * __restrict stacksize)
{
	if (attr == NULL || *attr == NULL || stackaddr == NULL
	    || stacksize == NULL)
		return (EINVAL);

	*stackaddr = (*attr)->stackaddr_attr;
	*stacksize = (*attr)->stacksize_attr;

	return (0);
}

int
_pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr)
{
	if (attr == NULL || *attr == NULL || stackaddr == NULL)
		return (EINVAL);

	*stackaddr = (*attr)->stackaddr_attr;

	return (0);
}

int
_pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize)
{
	if (attr == NULL || *attr == NULL || stacksize  == NULL)
		return (EINVAL);

	*stacksize = (*attr)->stacksize_attr;

	return (0);
}
