/*-
 * Copyright (c) 2006 The FreeBSD Project. All rights reserved.
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

#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/nameser.h>
#include <resolv.h>
#include <stdlib.h>

#include "namespace.h"
#include "reentrant.h"
#include "un-namespace.h"

#undef _res

struct __res_state _res;

static thread_key_t res_key;
static once_t res_init_once = ONCE_INITIALIZER;
static int res_thr_keycreated = 0;

static void
free_res(void *ptr)
{
	res_state statp = ptr;

	if (statp->_u._ext.ext != NULL)
		res_ndestroy(statp);
	free(statp);
}

static void
res_keycreate(void)
{
	res_thr_keycreated = thr_keycreate(&res_key, free_res) == 0;
}

res_state
__res_state(void)
{
	res_state statp;

	if (thr_main() != 0)
		return (&_res);

	if (thr_once(&res_init_once, res_keycreate) != 0 ||
	    !res_thr_keycreated)
		return (&_res);

	statp = thr_getspecific(res_key);
	if (statp != NULL)
		return (statp);
	statp = calloc(1, sizeof(*statp));
	if (statp == NULL)
		return (&_res);
#ifdef __BIND_RES_TEXT
	statp->options = RES_TIMEOUT;			/* Motorola, et al. */
#endif
	if (thr_setspecific(res_key, statp) == 0)
		return (statp);
	free(statp);
	return (&_res);
}
