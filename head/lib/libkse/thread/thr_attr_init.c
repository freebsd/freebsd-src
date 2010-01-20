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
 *
 * $FreeBSD$
 */

#include "namespace.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include "un-namespace.h"
#include "thr_private.h"

__weak_reference(_pthread_attr_init, pthread_attr_init);

int
_pthread_attr_init(pthread_attr_t *attr)
{
	int	ret;
	pthread_attr_t	pattr;

	/* Allocate memory for the attribute object: */
	if ((pattr = (pthread_attr_t) malloc(sizeof(struct pthread_attr))) == NULL)
		/* Insufficient memory: */
		ret = ENOMEM;
	else {
		/* Initialise the attribute object with the defaults: */
		memcpy(pattr, &_pthread_attr_default,
		    sizeof(struct pthread_attr));
		pattr->guardsize_attr = _thr_guard_default;
		pattr->stacksize_attr = _thr_stack_default;

		/* Return a pointer to the attribute object: */
		*attr = pattr;
		ret = 0;
	}
	return(ret);
}
