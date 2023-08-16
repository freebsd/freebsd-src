/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2021 Microsoft Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/mutex.h>
#include <sys/systm.h>

#include "gdma_util.h"


void
init_completion(struct completion *c)
{
	memset(c, 0, sizeof(*c));
	mtx_init(&c->lock, "gdma_completion", NULL, MTX_DEF);
	c->done = 0;
}

void
free_completion(struct completion *c)
{
	mtx_destroy(&c->lock);
}

void
complete(struct completion *c)
{
	mtx_lock(&c->lock);
	c->done++;
	mtx_unlock(&c->lock);
	wakeup(c);
}

void
wait_for_completion(struct completion *c)
{
	mtx_lock(&c->lock);
	while (c->done == 0)
		mtx_sleep(c, &c->lock, 0, "gdma_wfc", 0);
	c->done--;
	mtx_unlock(&c->lock);
}

/*
 * Return: 0 if completed, a non-zero value if timed out.
 */
int
wait_for_completion_timeout(struct completion *c, int timeout)
{
	int ret;

	mtx_lock(&c->lock);

	if (c->done == 0)
		mtx_sleep(c, &c->lock, 0, "gdma_wfc", timeout);

	if (c->done > 0) {
		c->done--;
		ret = 0;
	} else {
		ret = 1;
	}

	mtx_unlock(&c->lock);

	return (ret);
}
