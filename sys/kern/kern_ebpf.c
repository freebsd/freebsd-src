
/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Ryan Stone
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
 */

#include <sys/param.h>
#include <sys/cdefs.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/ebpf_probe.h>
#include <sys/epoch.h>
#include <sys/hash.h>
#include <sys/malloc.h>
#include <sys/sx.h>
#include <sys/syscall.h>

#include <machine/atomic.h>

struct ebpf_proc_probe {
	int probe_id;
	struct ebpf_probe *probe;
	void *module_state;
	RB_ENTRY(ebpf_proc_probe) link;
};

static void ebpf_active_syscall_probe(struct ebpf_probe *, void *);

static int ebpf_proc_probe_cmp(
    struct ebpf_proc_probe *, struct ebpf_proc_probe *);

RB_GENERATE_STATIC(
    ebpf_proc_probe_tree, ebpf_proc_probe, link, ebpf_proc_probe_cmp);

static MALLOC_DEFINE(M_EBPF_HOOKS, "ebpf_hooks", "ebpf hooks implementation");

#define PROBE_HASH_SIZE_SHIFT 16
#define PROBE_HASH_SIZE (1 << PROBE_HASH_SIZE_SHIFT)

static CK_SLIST_HEAD(probe_head, ebpf_probe) probe_hashtable[PROBE_HASH_SIZE];
static LIST_HEAD(, ebpf_probe) probe_id_hashtable[PROBE_HASH_SIZE];
static TAILQ_HEAD(, ebpf_probe) probe_list = TAILQ_HEAD_INITIALIZER(probe_list);

static ebpf_fire_t dummy_fire;

static struct ebpf_module dummy_module = {
	.fire = dummy_fire,
};

static const struct ebpf_module *ebpf_module_callbacks = &dummy_module;
static struct sx ebpf_sx;
SX_SYSINIT_FLAGS(ebpf_sx, &ebpf_sx, "ebpx_sx", SX_DUPOK);

struct ebpf_probe syscall_probes[SYS_MAXSYSCALL];
static void ebpf_register_syscall_probes(void);

static int next_id = EBPF_PROBE_FIRST + 1;

static int
ebpf_is_loaded(void)
{

	return ebpf_module_callbacks != &dummy_module;
}

static void
ebpf_init(void *arg)
{
	int i;

	for (i = 0; i < PROBE_HASH_SIZE; ++i) {
		CK_SLIST_INIT(&probe_hashtable[i]);
		LIST_INIT(&probe_id_hashtable[i]);
	}

	ebpf_register_syscall_probes();
}
SYSINIT(ebpf_init, SI_SUB_DTRACE, SI_ORDER_FIRST, ebpf_init, NULL);

#include "kern/ebpf_syscall_probes.c"

static void
ebpf_register_syscall_probes(void)
{
	int i;

	for (i = 0; i < nitems(ebpf_syscall_probe); ++i) {
		if (ebpf_syscall_probe[i].name.name[0] == '\0')
			continue;

		ebpf_syscall_probe[i].activate = ebpf_active_syscall_probe;
		ebpf_probe_register(&ebpf_syscall_probe[i]);
	}
}

static uint32_t
probe_hash(struct ebpf_probe_name *name)
{
	uint32_t hash;

	hash = murmur3_32_hash(name->tracer, sizeof(name->tracer), 0);
	hash = murmur3_32_hash(name->provider, sizeof(name->provider), hash);
	hash = murmur3_32_hash(name->module, sizeof(name->module), hash);
	hash = murmur3_32_hash(name->function, sizeof(name->function), hash);
	hash = murmur3_32_hash(name->name, sizeof(name->name), hash);
	return (hash & (PROBE_HASH_SIZE - 1));
}

static int
probe_name_cmp(struct ebpf_probe_name *a, struct ebpf_probe_name *b)
{
	int cmp;

	cmp = memcmp(a->tracer, b->tracer, sizeof(a->tracer));
	if (cmp != 0) {
		return (cmp);
	}

	cmp = memcmp(a->provider, b->provider, sizeof(a->tracer));
	if (cmp != 0) {
		return (cmp);
	}

	cmp = memcmp(a->module, b->module, sizeof(a->tracer));
	if (cmp != 0) {
		return (cmp);
	}

	cmp = memcmp(a->function, b->function, sizeof(a->tracer));
	if (cmp != 0) {
		return (cmp);
	}

	return (memcmp(a->name, b->name, sizeof(a->tracer)));
}

static uint32_t
probe_id_hash(uint32_t id)
{

	return (id & (PROBE_HASH_SIZE - 1));
}

/*
 * This could only possibly be called during a race with ebpf.ko unload.  Just
 * do nothing if we're called.
 */
static int
dummy_fire(struct ebpf_probe *probe, void *a, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
{

	return (EBPF_ACTION_CONTINUE);
}

void
ebpf_module_register(const struct ebpf_module *mod)
{

	KASSERT(!ebpf_is_loaded(), ("ebpf.ko loaded twice"));

	atomic_store_rel_ptr(
	    (uintptr_t *)&ebpf_module_callbacks, (uintptr_t)mod);

	printf(
	    "EBPF module registered @ %p (%p)\n", mod, ebpf_module_callbacks);
}

void
ebpf_module_deregister()
{
	KASSERT(ebpf_is_loaded(), ("ebpf.ko unloaded twice"));

	sx_xlock(&ebpf_sx);
	atomic_store_rel_ptr(
	    (uintptr_t *)&ebpf_module_callbacks, (uintptr_t)&dummy_module);
	sx_xunlock(&ebpf_sx);

	printf(
	    "EBPF module deregistered; revert to %p\n", ebpf_module_callbacks);
}

void
ebpf_probe_register(void *arg)
{
	struct ebpf_probe *probe;
	uint32_t hash;

	probe = arg;
	hash = probe_hash(&probe->name);

	sx_xlock(&ebpf_sx);
	probe->id = next_id;
	next_id++;

	probe->active = 0;
	CK_SLIST_INSERT_HEAD(&probe_hashtable[hash], probe, hash_link);

	hash = probe_id_hash(probe->id);
	LIST_INSERT_HEAD(&probe_id_hashtable[hash], probe, id_link);
	TAILQ_INSERT_TAIL(&probe_list, probe, list_link);
	sx_xunlock(&ebpf_sx);
}

void
ebpf_probe_deregister(void *arg)
{
	/*
	 * XXX locking here must be rethought for deregister:
	 * - Deregister probe from ebpf.ko safely
	 * - Drain firing probes via epoch_wait.
	 */

	printf("%s unimplemented\n", __func__);
}

struct ebpf_probe *
ebpf_activate_probe(ebpf_probe_id_t id, void *state)
{
	struct ebpf_probe *probe = NULL;
	uint32_t hash; 
	
	probe->activate = &ebpf_active_syscall_probe;
	hash = probe_id_hash(id);

	sx_slock(&ebpf_sx);
	LIST_FOREACH (probe, &probe_id_hashtable[hash], id_link) {
		if (id == probe->id) {
			probe->activate(probe, state);
			atomic_add_int(&probe->active, 1);
			break;
		}
	}
	sx_sunlock(&ebpf_sx);
	return (probe);
}

static void
ebpf_active_syscall_probe(struct ebpf_probe *probe, void *state)
{
	struct ebpf_proc_probe *pp;
	struct proc *proc;

	pp = malloc(sizeof(*pp), M_EBPF_HOOKS, M_WAITOK);
	pp->probe_id = probe->id;
	pp->probe = probe;
	pp->module_state = state;

	proc = curthread->td_proc;

	sx_xlock(&proc->p_ebpf_lock);
	RB_INSERT(ebpf_proc_probe_tree, &proc->p_ebpf_probes, pp);
	sx_xunlock(&proc->p_ebpf_lock);
}

int
ebpf_get_probe_by_name(
    struct ebpf_probe_name *name, ebpf_probe_cb cb, void *arg)
{
	struct ebpf_probe *probe;
	int ret;
	uint32_t hash;

	hash = probe_hash(name);

	sx_slock(&ebpf_sx);
	CK_SLIST_FOREACH(probe, &probe_hashtable[hash], hash_link)
	{
		if (probe_name_cmp(name, &probe->name) == 0) {
			ret = cb(probe, arg);
			goto done;
		}
	}
	ret = ENOENT;

done:
	sx_sunlock(&ebpf_sx);
	return (ret);
}

int
ebpf_next_probe(ebpf_probe_id_t id, ebpf_probe_cb cb, void *arg)
{
	struct ebpf_probe *probe;
	uint32_t hash;
	int ret;

	sx_slock(&ebpf_sx);
	if (id == EBPF_PROBE_FIRST) {
		probe = TAILQ_FIRST(&probe_list);
		if (probe != NULL) {
			ret = cb(probe, arg);
		} else {
			ret = ECHILD;
		}

		goto done;
	}

	hash = probe_id_hash(id);

	LIST_FOREACH (probe, &probe_id_hashtable[hash], id_link) {
		if (id == probe->id) {
			probe = TAILQ_NEXT(probe, list_link);
			if (probe != NULL) {
				ret = cb(probe, arg);
			} else {
				ret = ECHILD;
			}
			goto done;
		}
	}
	ret = ENOENT;

done:
	sx_sunlock(&ebpf_sx);
	return (ret);
}

static int
ebpf_syscall_probe_fire(struct ebpf_probe *probe, uintptr_t arg0,
    uintptr_t arg1, uintptr_t arg2, uintptr_t arg3, uintptr_t arg4,
    uintptr_t arg5)
{
	struct proc *proc;
	struct ebpf_proc_probe lookup, *pp;
	int ret = EBPF_ACTION_CONTINUE;

	proc = curthread->td_proc;

	sx_slock(&proc->p_ebpf_lock);

	lookup.probe_id = probe->id;
	pp = RB_FIND(ebpf_proc_probe_tree, &proc->p_ebpf_probes, &lookup);
	if (pp != NULL) {
		ret = ebpf_module_callbacks->fire(probe, pp->module_state, arg0,
		    arg1, arg2, arg3, arg4, arg5);
	}

	sx_sunlock(&proc->p_ebpf_lock);

	return (ret);
}

int
ebpf_probe_fire(struct ebpf_probe *probe, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
{
	struct proc *proc;
	struct ebpf_proc_probe lookup, *pp;
	int ret = EBPF_ACTION_CONTINUE;
	
	proc = curthread->td_proc;
	pp = RB_FIND(ebpf_proc_probe_tree, &proc->p_ebpf_probes, &lookup);
	if (pp != NULL) {
		ret = ebpf_module_callbacks->fire(probe, pp->module_state, arg0,
		    arg1, arg2, arg3, arg4, arg5);
	}

	return (ret);
}

static int
ebpf_proc_probe_cmp(struct ebpf_proc_probe *a, struct ebpf_proc_probe *b)
{

	return (a->probe_id - b->probe_id);
}

void
ebpf_clone_proc_probes(struct proc *parent, struct proc *newproc)
{
	struct ebpf_proc_probe *pp, *new_pp;

	sx_slock(&ebpf_sx);
	sx_slock(&parent->p_ebpf_lock);
	RB_FOREACH (pp, ebpf_proc_probe_tree, &parent->p_ebpf_probes) {
		new_pp = malloc(sizeof(*new_pp), M_EBPF_HOOKS, M_WAITOK);

		atomic_add_int(&pp->probe->active, 1);
		new_pp->probe_id = pp->probe_id;
		new_pp->probe = pp->probe;
		new_pp->module_state = ebpf_module_callbacks->clone_probe(
		    pp->probe, pp->module_state);

		RB_INSERT(
		    ebpf_proc_probe_tree, &newproc->p_ebpf_probes, new_pp);
	}
	sx_sunlock(&parent->p_ebpf_lock);
	sx_sunlock(&ebpf_sx);
}

void
ebpf_free_proc_probes(struct proc *proc)
{
	struct ebpf_proc_probe *pp, *next;

	sx_xlock(&ebpf_sx);
	sx_xlock(&proc->p_ebpf_lock);
	RB_FOREACH_SAFE (pp, ebpf_proc_probe_tree, &proc->p_ebpf_probes, next) {
		atomic_add_int(&pp->probe->active, -1);
		ebpf_module_callbacks->release_probe(
		    pp->probe, pp->module_state);

		RB_REMOVE(ebpf_proc_probe_tree, &proc->p_ebpf_probes, pp);
		free(pp, M_EBPF_HOOKS);
	}
	sx_xunlock(&proc->p_ebpf_lock);
	sx_xunlock(&ebpf_sx);
}
