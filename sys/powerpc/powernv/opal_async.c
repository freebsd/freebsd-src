/*-
 * Copyright (C) 2019 Justin Hibbits
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/eventhandler.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/vmem.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include "opal.h"

#include <machine/cpufunc.h>

/*
 * Manage asynchronous tokens for the OPAL abstraction layer.
 *
 * Only a finite number of in-flight tokens are supported by OPAL, so we must be
 * careful managing this.  The basic design uses the vmem subsystem as a general
 * purpose allocator, with wrappers to manage expected behaviors and
 * requirements.
 */
static vmem_t *async_token_pool;

static void opal_handle_async_completion(void *, struct opal_msg *);

struct async_completion {
	volatile uint64_t 	completed;
	struct opal_msg 	msg;
};

struct async_completion *completions;

/* Setup the token pool. */
int
opal_init_async_tokens(int count)
{
	/* Only allow one initialization */
	if (async_token_pool != NULL)
		return (EINVAL);

	async_token_pool = vmem_create("OPAL Async", 0, count, 1, 1,
	    M_WAITOK | M_FIRSTFIT);
	completions = malloc(count * sizeof(struct async_completion),
	    M_DEVBUF, M_WAITOK | M_ZERO);

	EVENTHANDLER_REGISTER(OPAL_ASYNC_COMP, opal_handle_async_completion,
	    NULL, EVENTHANDLER_PRI_ANY);

	return (0);
}

int
opal_alloc_async_token(void)
{
	vmem_addr_t token;

	vmem_alloc(async_token_pool, 1, M_FIRSTFIT | M_WAITOK, &token);
	completions[token].completed = false;

	return (token);
}

void
opal_free_async_token(int token)
{

	vmem_free(async_token_pool, token, 1);
}

/*
 * Wait for the operation watched by the token to complete.  Return the result
 * of the operation, error if it returns early.
 */
int
opal_wait_completion(void *buf, uint64_t size, int token)
{
	int err;

	do {
		err = opal_call(OPAL_CHECK_ASYNC_COMPLETION,
		    vtophys(buf), size, token);
		if (err == OPAL_BUSY) {
			if (completions[token].completed) {
				atomic_thread_fence_acq();
				memcpy(buf, &completions[token].msg, size);
				return (OPAL_SUCCESS);
			}
		}
		DELAY(100);
	} while (err == OPAL_BUSY);

	return (err);
}

static void opal_handle_async_completion(void *arg, struct opal_msg *msg)
{
	int token;

	token = msg->params[0];
	memcpy(&completions[token].msg, msg, sizeof(*msg));
	atomic_thread_fence_rel();
	completions[token].completed = true;
}
