
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

#ifndef _SYS_EBPF_PROBE_H_
#define _SYS_EBPF_PROBE_H_

#include <sys/ebpf_defines.h>

typedef uint32_t ebpf_probe_id_t;

#define EBPF_PROBE_FIRST (0)

struct ebpf_probe_name
{
	char tracer[EBPF_PROBE_NAME_MAX];
	char provider[EBPF_PROBE_NAME_MAX];
	char module[EBPF_PROBE_NAME_MAX];
	char function[EBPF_PROBE_NAME_MAX];
	char name[EBPF_PROBE_NAME_MAX];
};

#ifdef _KERNEL
#ifdef EBPF_HOOKS

#include <ck_queue.h>

struct proc;

struct ebpf_probe
{
	ebpf_probe_id_t id;
	int active;
	void (*activate)(struct ebpf_probe *, void *);
	struct ebpf_probe_name name;
	size_t arglen;
	CK_SLIST_ENTRY(ebpf_probe) hash_link;
	LIST_ENTRY(ebpf_probe) id_link;
	TAILQ_ENTRY(ebpf_probe) list_link;
};

typedef int ebpf_fire_t(struct ebpf_probe *, void *, uintptr_t , uintptr_t,
    uintptr_t, uintptr_t, uintptr_t, uintptr_t);

typedef void *ebpf_probe_clone_t(struct ebpf_probe *, void *);
typedef void ebpf_probe_release_t(struct ebpf_probe *, void *);

typedef int (*ebpf_probe_cb)(struct ebpf_probe *, void *);

int ebpf_get_probe_by_name(struct ebpf_probe_name *name, ebpf_probe_cb cb,
    void *arg);
int ebpf_next_probe(ebpf_probe_id_t, ebpf_probe_cb cb, void *arg);

struct ebpf_module
{
	ebpf_fire_t * fire;
	ebpf_probe_clone_t *clone_probe;
	ebpf_probe_release_t *release_probe;
};

void ebpf_probe_register(void *);
void ebpf_probe_deregister(void *);

struct ebpf_probe * ebpf_activate_probe(ebpf_probe_id_t, void *);

void ebpf_module_register(const struct ebpf_module *);
void ebpf_module_deregister(void);

void ebpf_clone_proc_probes(struct proc *parent, struct proc *newproc);
void ebpf_free_proc_probes(struct proc *p);

int ebpf_syscall_probe_fire(struct ebpf_probe *, uintptr_t arg0, uintptr_t arg1,    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);

int ebpf_probe_fire(struct ebpf_probe *, void *, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5);

#define _EBPF_PROBE(name) __CONCAT(name, _probe_def)

#define EBPF_PROBE_DEFINE(probeName) \
	static struct ebpf_probe _EBPF_PROBE(probeName) = { \
		.name = __XSTRING(probeName), \
	}; \
	SYSINIT(__CONCAT(epf_, __CONCAT(probeName, _register)), SI_SUB_DTRACE, SI_ORDER_SECOND, \
		ebpf_probe_register, &_EBPF_PROBE(probeName)); \
	SYSUNINIT(__CONCAT(epf_, __CONCAT(probeName, _deregister)), SI_SUB_DTRACE, SI_ORDER_SECOND, \
		ebpf_probe_deregister, &_EBPF_PROBE(probeName)); \
	struct hack

#define EBPF_PROBE_ACTIVE(name) \
	(_EBPF_PROBE(name).active)

#define EBPF_PROBE_FIRE6(name, arg0, arg1, arg2, arg3, arg4, arg5) \
	do { \
		if (_EBPF_PROBE(name).active) { \
			ebpf_probe_fire(&_EBPF_PROBE(name), \
			    (uintptr_t)arg0, (uintptr_t)arg1, (uintptr_t)arg2, \
			    (uintptr_t)arg3, (uintptr_t)arg4, (uintptr_t)arg5); \
		} \
	} while (0)

#else

#define EBPF_PROBE_DEFINE(probeName) struct hack

#define EBPF_PROBE_FIRE6(name, arg0, arg1, arg2, arg3, arg4, arg5) do { \
		(void)arg0; \
		(void)arg1; \
		(void)arg2; \
		(void)arg3; \
		(void)arg4; \
		(void)arg5; \
	} while (0)

#endif

#define EBPF_PROBE_FIRE1(name, arg0) \
	EBPF_PROBE_FIRE6(name, arg0, 0, 0, 0, 0, 0)

#define EBPF_PROBE_FIRE2(name, arg0, arg1) \
	EBPF_PROBE_FIRE6(name, arg0, arg1, 0, 0, 0, 0)

#define EBPF_PROBE_FIRE4(name, arg0, arg1, arg2, arg3) \
	EBPF_PROBE_FIRE6(name, arg0, arg1, arg2, arg3, 0, 0)

extern struct ebpf_probe ebpf_syscall_probe[];

#define EBPF_SYSCALL_FIRE(n, arg, len) \
	( ((n) < SYS_MAXSYSCALL && ebpf_syscall_probe[n].active) ? \
	    ebpf_syscall_probe_fire(&ebpf_syscall_probe[n], \
	    (uintptr_t)(arg), sizeof(register_t) * (len), \
		0, 0, 0, 0) \
	  : EBPF_ACTION_CONTINUE )

#endif
#endif
