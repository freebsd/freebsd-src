/*
 * Copyright (c) 2002, Jeffrey Roberson <jroberson@chesapeake.net>
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

/*
 * uma_dbg.c	Debugging features for UMA users
 *
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/queue.h>
#include <sys/lock.h>
#include <sys/mutex.h>

#include <machine/types.h>

#include <vm/uma.h>
#include <vm/uma_int.h>
#include <vm/uma_dbg.h>

static const u_int32_t uma_junk = 0xdeadc0de;

/*
 * Checks an item to make sure it hasn't been overwritten since freed.
 *
 * Complies with standard ctor arg/return
 *
 */
void
trash_ctor(void *mem, int size, void *arg)
{
	int cnt;
	u_int32_t *p;

	cnt = size / sizeof(uma_junk);

	for (p = mem; cnt > 0; cnt--, p++)
		if (*p != uma_junk)
			panic("Memory modified after free %p(%d)\n",
			    mem, size);
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard dtor arg/return
 *
 */
void
trash_dtor(void *mem, int size, void *arg)
{
	int cnt;
	u_int32_t *p;

	cnt = size / sizeof(uma_junk);

	for (p = mem; cnt > 0; cnt--, p++)
		*p = uma_junk;
}

/*
 * Fills an item with predictable garbage
 *
 * Complies with standard init arg/return
 *
 */
void
trash_init(void *mem, int size)
{
	trash_dtor(mem, size, NULL);
}

/*
 * Checks an item to make sure it hasn't been overwritten since it was freed.
 *
 * Complies with standard fini arg/return
 *
 */
void
trash_fini(void *mem, int size)
{
	trash_ctor(mem, size, NULL);
}
