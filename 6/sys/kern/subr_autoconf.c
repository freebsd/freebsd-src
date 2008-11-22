/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)subr_autoconf.c	8.1 (Berkeley) 6/10/93
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/systm.h>

/*
 * Autoconfiguration subroutines.
 */

/*
 * "Interrupt driven config" functions.
 */
static TAILQ_HEAD(, intr_config_hook) intr_config_hook_list =
	TAILQ_HEAD_INITIALIZER(intr_config_hook_list);
static struct mtx intr_config_hook_lock;
MTX_SYSINIT(intr_config_hook, &intr_config_hook_lock, "intr config", MTX_DEF);

/* ARGSUSED */
static void run_interrupt_driven_config_hooks(void *dummy);

static void
run_interrupt_driven_config_hooks(dummy)
	void *dummy;
{
	struct intr_config_hook *hook_entry, *next_entry;

	mtx_lock(&intr_config_hook_lock);
	TAILQ_FOREACH_SAFE(hook_entry, &intr_config_hook_list, ich_links,
	    next_entry) {
		next_entry = TAILQ_NEXT(hook_entry, ich_links);
		mtx_unlock(&intr_config_hook_lock);
		(*hook_entry->ich_func)(hook_entry->ich_arg);
		mtx_lock(&intr_config_hook_lock);
	}

	while (!TAILQ_EMPTY(&intr_config_hook_list)) {
		msleep(&intr_config_hook_list, &intr_config_hook_lock, PCONFIG,
		    "conifhk", 0);
	}
	mtx_unlock(&intr_config_hook_lock);
}
SYSINIT(intr_config_hooks, SI_SUB_INT_CONFIG_HOOKS, SI_ORDER_FIRST,
	run_interrupt_driven_config_hooks, NULL)

/*
 * Register a hook that will be called after "cold"
 * autoconfiguration is complete and interrupts can
 * be used to complete initialization.
 */
int
config_intrhook_establish(hook)
	struct intr_config_hook *hook;
{
	struct intr_config_hook *hook_entry;

	mtx_lock(&intr_config_hook_lock);
	TAILQ_FOREACH(hook_entry, &intr_config_hook_list, ich_links)
		if (hook_entry == hook)
			break;
	if (hook_entry != NULL) {
		mtx_unlock(&intr_config_hook_lock);
		printf("config_intrhook_establish: establishing an "
		       "already established hook.\n");
		return (1);
	}
	TAILQ_INSERT_TAIL(&intr_config_hook_list, hook, ich_links);
	mtx_unlock(&intr_config_hook_lock);
	if (cold == 0)
		/* XXX Sufficient for modules loaded after initial config??? */
		run_interrupt_driven_config_hooks(NULL);	
	return (0);
}

void
config_intrhook_disestablish(hook)
	struct intr_config_hook *hook;
{
	struct intr_config_hook *hook_entry;

	mtx_lock(&intr_config_hook_lock);
	TAILQ_FOREACH(hook_entry, &intr_config_hook_list, ich_links)
		if (hook_entry == hook)
			break;
	if (hook_entry == NULL)
		panic("config_intrhook_disestablish: disestablishing an "
		      "unestablished hook");

	TAILQ_REMOVE(&intr_config_hook_list, hook, ich_links);

	/* Wakeup anyone watching the list */
	wakeup(&intr_config_hook_list);
	mtx_unlock(&intr_config_hook_lock);
}
