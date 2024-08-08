/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright © 2023 Elliott Mitchell
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
 */

#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/errno.h>
#include <sys/interrupt.h>
#include <sys/intrtab.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

MALLOC_DECLARE(M_INTRTAB);
MALLOC_DEFINE(M_INTRTAB, "intrtab", "interrupt table handling");

/*
 * The resource manager for interrupt numbers
 */
static struct rman *mgr;

/*
 * The main table
 */
static interrupt_t **table;

void
intrtab_setup(struct rman *newmgr)
{

	if (mgr != NULL || table != NULL)
		printf("%s(): called after already initialized!", __func__);

	mgr = newmgr;
}

void
intrtab_init(void)
{

	table = mallocarray(mgr->rm_end - mgr->rm_start, sizeof(interrupt_t *),
	    M_INTRTAB, M_WAITOK | M_ZERO);
	table -= mgr->rm_start;
}

/*
 * Allocate a block of interrupt numbers
 */
struct resource *
intrtab_alloc_intr(device_t dev, u_int count)
{

	return (rman_reserve_resource(mgr, 0, ~0, count,
	    RF_ACTIVE | RF_UNMAPPED, dev));
}

/*
 * Release a block of interrupt numbers
 */
void
intrtab_release_intr(struct resource *range)
{
	u_long i;

	if (range == NULL || !rman_is_region_manager(range, mgr))
		return;

	for (i = rman_get_start(range); i <= rman_get_end(range); ++i)
		table[i] = NULL;

	rman_release_resource(range);
}

int
intrtab_set(struct resource *res, u_int intr, interrupt_t *new,
    const interrupt_t *const old)
{
	interrupt_t **entr;

	MPASS(intr2event(new) == NULL || _intr2event(new)->ie_irq == intr);

	if (!rman_is_region_manager(res, mgr))
		return (EINVAL);

	if (intr < rman_get_start(res) || intr > rman_get_end(res))
		return (EINVAL);

	entr = table + intr;

	if (*entr != old)
		return (EEXIST);

	*entr = new;
	return (0);
}

interrupt_t *
intrtab_lookup(u_int intr)
{

	if (intr - mgr->rm_start > mgr->rm_end - mgr->rm_start)
		return (NULL);

	return (table[intr]);
}
