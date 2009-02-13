/*-
 * Copyright (c) 2006, Maxime Henrion <mux@FreeBSD.org>
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
 * $Id$
 */

#include <assert.h>
#include <stdlib.h>

#include "attrstack.h"
#include "fattr.h"
#include "misc.h"

#define	ATTRSTACK_DEFSIZE	16	/* Initial size of the stack. */

struct attrstack {
	struct fattr **stack;
	size_t cur;
	size_t size;
};

struct attrstack *
attrstack_new(void)
{
	struct attrstack *as;

	as = xmalloc(sizeof(struct attrstack));
	as->stack = xmalloc(sizeof(struct fattr *) * ATTRSTACK_DEFSIZE);
	as->size = ATTRSTACK_DEFSIZE;
	as->cur = 0;
	return (as);
}

struct fattr *
attrstack_pop(struct attrstack *as)
{

	assert(as->cur > 0);
	return (as->stack[--as->cur]);
}

void
attrstack_push(struct attrstack *as, struct fattr *fa)
{

	if (as->cur >= as->size) {
		as->size *= 2;
		as->stack = xrealloc(as->stack,
		    sizeof(struct fattr *) * as->size);
	}
	as->stack[as->cur++] = fa;
}

size_t
attrstack_size(struct attrstack *as)
{

	return (as->cur);
}

void
attrstack_free(struct attrstack *as)
{

	assert(as->cur == 0);
	free(as->stack);
	free(as);
}
