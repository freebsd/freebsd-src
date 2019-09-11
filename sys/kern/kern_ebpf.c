
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
#include <sys/sx.h>
#include <sys/syscall.h>

#include <machine/atomic.h>

#define	PROBE_HASH_SIZE_SHIFT 16
#define	PROBE_HASH_SIZE (1 << PROBE_HASH_SIZE_SHIFT)

static CK_SLIST_HEAD(probe_head, ebpf_probe) probe_hashtable[PROBE_HASH_SIZE];

static ebpf_fire_t dummy_fire;

static struct ebpf_module dummy_module = {
	.fire = dummy_fire,
};

static struct ebpf_module * ebpf_module_callbacks = &dummy_module;
static struct sx ebpf_sx;

struct ebpf_probe syscall_probes[SYS_MAXSYSCALL];
static void ebpf_register_syscall_probes(void);

static int
ebpf_is_loaded(void)
{

	return ebpf_module_callbacks != &dummy_module;
}

static void
ebpf_init(void *arg)
{
	int i;

	sx_init(&ebpf_sx, "ebpf_sx");

	for (i = 0; i < PROBE_HASH_SIZE; ++i) {
		CK_SLIST_INIT(&probe_hashtable[i]);
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
		if (ebpf_syscall_probe[i].name == NULL)
			continue;

		ebpf_probe_register(&ebpf_syscall_probe[i]);
	}
}

static uint32_t
probe_hash(const char *name)
{
	uint32_t hash;

	hash = murmur3_32_hash(name, strlen(name), 0);
	return (hash & (PROBE_HASH_SIZE - 1));
}

/*
 * This could only possibly be called during a race with ebpf.ko unload.  Just
 * do nothing if we're called.
 */
static int
dummy_fire(struct ebpf_probe *probe, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
{

	return (EBPF_ACTION_CONTINUE);
}

void
ebpf_module_register(struct ebpf_module *mod)
{

	KASSERT (!ebpf_is_loaded(), ("ebpf.ko loaded twice"));

	atomic_store_rel_ptr((uintptr_t*)&ebpf_module_callbacks, (uintptr_t)mod);

	printf("EBPF module registered @ %p (%p)\n", mod, ebpf_module_callbacks);
}

void
ebpf_module_deregister()
{
	KASSERT (ebpf_is_loaded(), ("ebpf.ko unloaded twice"));

	sx_xlock(&ebpf_sx);
	atomic_store_rel_ptr((uintptr_t*)&ebpf_module_callbacks, (uintptr_t)&dummy_module);
	sx_xunlock(&ebpf_sx);

	printf("EBPF module deregistered; revert to %p\n", ebpf_module_callbacks);
}

void
ebpf_probe_register(void *arg)
{
	struct ebpf_probe *probe;
	uint32_t hash;

	probe = arg;
	hash = probe_hash(probe->name);

	sx_xlock(&ebpf_sx);
	probe->active = 0;
	CK_SLIST_INSERT_HEAD(&probe_hashtable[hash], probe, hash_link);
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
ebpf_find_probe(const char *name)
{
	struct ebpf_probe *probe, *next;
	uint32_t hash;

	hash = probe_hash(name);

	sx_slock(&ebpf_sx);
	CK_SLIST_FOREACH_SAFE(probe, &probe_hashtable[hash], hash_link, next) {
		if (strcmp(name, probe->name) == 0) {
			goto out;
		}
	}

out:
	sx_sunlock(&ebpf_sx);
	return probe;
}

void
ebpf_probe_drain(struct ebpf_probe *probe)
{

	sx_xlock(&ebpf_sx);
	probe->active = 0;
	sx_xunlock(&ebpf_sx);
}

int
ebpf_probe_fire(struct ebpf_probe *probe, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
{
	int ret = EBPF_ACTION_CONTINUE;

	sx_xlock(&ebpf_sx);
	if (probe->active)
		ret = ebpf_module_callbacks->fire(probe, arg0, arg1, arg2, arg3, arg4, arg5);
	sx_xunlock(&ebpf_sx);

	return (ret);
}
