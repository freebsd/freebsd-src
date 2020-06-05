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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/event.h>
#include <sys/resource.h>

#include <sys/ebpf.h>
#include <sys/ebpf_param.h>
#include <sys/ebpf_probe.h>
#include <dev/ebpf/ebpf_platform.h>
#include <dev/ebpf/ebpf_dev_platform.h>
#include <dev/ebpf/ebpf_dev_freebsd.h>
#include <dev/ebpf/ebpf_dev_probe.h>
#include <dev/ebpf/ebpf_internal.h>
#include <dev/ebpf/ebpf_prog.h>
#include <dev/ebpf/ebpf_probe_syscall.h>

#include <sys/refcount.h>

struct ebpf_probe_state {
	struct ebpf_probe *probe, (*activate)(ebpf_probe_id_t, void);
	struct ebpf_prog *prog;
	ebpf_file *fp;
	int jit;
	uint32_t refcount;
};

static const struct ebpf_probe_ops *probe_ops[] = {
	[EBPF_PROG_TYPE_VFS] = &vfs_probe_ops,
};

int
ebpf_probe_attach(
    ebpf_probe_id_t id, struct ebpf_prog *prog, ebpf_file *fp, int jit)
{
	struct ebpf_probe *probe;
	struct ebpf_probe_state *state;

	state = ebpf_calloc(sizeof(*state), 1);
	if (state == NULL)
		return (ENOMEM);

	ebpf_refcount_init(&state->refcount, 1);
	state->jit = jit;
	state->prog = prog;
	state->fp = fp;

	probe = ebpf_activate_probe(id, state);
	if (probe == NULL) {
		ebpf_free(state);
		return (ENOENT);
	}

	state->probe = probe;

	return (0);
}

static void *
ebpf_probe_clone(struct ebpf_probe *probe, void *a)
{
	struct ebpf_probe_state *state;

	state = a;
	ebpf_refcount_acquire(&state->refcount);

	return (state);
}

static void
ebpf_probe_release(struct ebpf_probe *probe, void *a)
{
	struct ebpf_probe_state *state;

	state = a;

	if (refcount_release(&state->refcount)) {
		ebpf_fdrop(state->fp, curthread);
		ebpf_free(state);
	}
}

static int
ebpf_probe_reserve_cpu(struct ebpf_prog *prog, struct ebpf_vm_state *vm_state)
{

	KASSERT(prog->type < nitems(probe_ops),
	    ("ebpf program type %d out of bounds", prog->type));

	return (probe_ops[prog->type]->reserve_cpu(vm_state));
}

static void
ebpf_probe_release_cpu(struct ebpf_prog *prog, struct ebpf_vm_state *vm_state)
{

	KASSERT(prog->type < nitems(probe_ops),
	    ("ebpf program type %d out of bounds", prog->type));

	probe_ops[prog->type]->release_cpu(vm_state);
}

static void
ebpf_vm_init_state(struct ebpf_vm_state *vm_state)
{

	vm_state->prog_fp = NULL;
	vm_state->next_prog = NULL;
	vm_state->deferred_func = NULL;
	vm_state->num_tail_calls = 0;
}

static int
ebpf_fire(struct ebpf_probe *probe, void *a, uintptr_t arg0, uintptr_t arg1,
    uintptr_t arg2, uintptr_t arg3, uintptr_t arg4, uintptr_t arg5)
{
	struct thread *td;
	struct ebpf_probe_state *state;
	struct ebpf_prog *prog;
	ebpf_file *prog_fp;
	struct ebpf_vm_state vm_state;
	int error, ret;

	state = a;

	ebpf_vm_init_state(&vm_state);

	vm_state.next_prog = state->prog;
	vm_state.next_prog_arg = (void *)arg0;

	prog_fp = NULL;

	td = curthread;

	KASSERT(td->td_ebpf_state == NULL, ("Thread recursed into ebpf"));
	td->td_ebpf_state = &vm_state;

	while (vm_state.next_prog != NULL && vm_state.num_tail_calls < 32) {
		prog = vm_state.next_prog;
		vm_state.next_prog = NULL;

		error = ebpf_probe_reserve_cpu(prog, &vm_state);
		if (error != 0) {
			// XXX this only applies to VFS program types...
			ebpf_probe_set_errno(error);
			return (EBPF_ACTION_RETURN);
		}

		ret = ebpf_prog_run(vm_state.next_prog_arg, prog);

		ebpf_probe_release_cpu(prog, &vm_state);

		/* Drop reference on program we just ran. */
		if (prog_fp != NULL) {
			ebpf_fdrop(prog_fp, td);
		}

		/* Grab pointer to program we will run on next iteration */
		prog_fp = vm_state.prog_fp;
		vm_state.prog_fp = NULL;

		if (vm_state.deferred_func != 0) {
			vm_state.deferred_func(&vm_state);
			vm_state.deferred_func = NULL;
		}
		vm_state.num_tail_calls++;
	}

	/* Could happen if we exceed the tail call limit */
	if (prog_fp != NULL) {
		ebpf_fdrop(prog_fp, td);
	}

	td->td_ebpf_state = NULL;

	return (ret);
}

static const struct ebpf_module ebpf_mod_callbacks = {
	.fire = ebpf_fire,
	.clone_probe = ebpf_probe_clone,
	.release_probe = ebpf_probe_release,
};

void
ebpf_probe_init(void)
{
	int i;

	for (i = 0; i < nitems(probe_ops); ++i) {
		if (probe_ops[i] != NULL)
			probe_ops[i]->init();
	}

	ebpf_module_register(&ebpf_mod_callbacks);
}

void
ebpf_probe_fini(void)
{
	int i;

	ebpf_module_deregister();

	for (i = 0; i < nitems(probe_ops); ++i) {
		if (probe_ops[i] != NULL)
			probe_ops[i]->fini();
	}
}
