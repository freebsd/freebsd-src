/*-
 * Copyright (c) 2020 Emmanuel Vadot <manu@FreeBSD.org>
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
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */


#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/eventhandler.h>
#include <sys/sx.h>

#include <linux/compat.h>
#include <linux/shrinker.h>

TAILQ_HEAD(, shrinker) lkpi_shrinkers = TAILQ_HEAD_INITIALIZER(lkpi_shrinkers);
static struct sx sx_shrinker;

int
linuxkpi_register_shrinker(struct shrinker *s)
{

	KASSERT(s != NULL, ("NULL shrinker"));
	KASSERT(s->count_objects != NULL, ("NULL shrinker"));
	KASSERT(s->scan_objects != NULL, ("NULL shrinker"));
	sx_xlock(&sx_shrinker);
	TAILQ_INSERT_TAIL(&lkpi_shrinkers, s, next);
	sx_xunlock(&sx_shrinker);
	return (0);
}

void
linuxkpi_unregister_shrinker(struct shrinker *s)
{

	sx_xlock(&sx_shrinker);
	TAILQ_REMOVE(&lkpi_shrinkers, s, next);
	sx_xunlock(&sx_shrinker);
}

void
linuxkpi_synchronize_shrinkers(void)
{

	sx_xlock(&sx_shrinker);
	sx_xunlock(&sx_shrinker);
}

#define	SHRINKER_BATCH	512

static void
shrinker_shrink(struct shrinker *s)
{
	struct shrink_control sc;
	unsigned long can_free;
	unsigned long batch;
	unsigned long scanned = 0;
	unsigned long ret;

	can_free = s->count_objects(s, &sc);
	if (can_free <= 0)
		return;

	batch = s->batch ? s->batch : SHRINKER_BATCH;
	while (scanned <= can_free) {
		sc.nr_to_scan = batch;
		ret = s->scan_objects(s, &sc);
		if (ret == SHRINK_STOP)
			break;
		scanned += batch;
	}
}

static void
linuxkpi_vm_lowmem(void *arg __unused)
{
	struct shrinker *s;

	sx_xlock(&sx_shrinker);
	TAILQ_FOREACH(s, &lkpi_shrinkers, next) {
		shrinker_shrink(s);
	}
	sx_xunlock(&sx_shrinker);
}

static eventhandler_tag lowmem_tag;

static void
linuxkpi_sysinit_shrinker(void *arg __unused)
{

	sx_init(&sx_shrinker, "lkpi-shrinker");
	lowmem_tag = EVENTHANDLER_REGISTER(vm_lowmem, linuxkpi_vm_lowmem,
	    NULL, EVENTHANDLER_PRI_FIRST);
}

static void
linuxkpi_sysuninit_shrinker(void *arg __unused)
{

	sx_destroy(&sx_shrinker);
	EVENTHANDLER_DEREGISTER(vm_lowmem, lowmem_tag);
}

SYSINIT(linuxkpi_shrinker, SI_SUB_DRIVERS, SI_ORDER_ANY,
    linuxkpi_sysinit_shrinker, NULL);
SYSUNINIT(linuxkpi_shrinker, SI_SUB_DRIVERS, SI_ORDER_ANY,
    linuxkpi_sysuninit_shrinker, NULL);
