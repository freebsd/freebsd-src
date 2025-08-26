/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2025 Ruslan Bukin <br@bsdpad.com>
 *
 * This work was supported by Innovate UK project 105694, "Digital Security
 * by Design (DSbD) Technology Platform Prototype".
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

/* Hardware Trace (HWT) framework. */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/ioccom.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
#include <sys/smp.h>
#include <sys/hwt.h>

#include <dev/hwt/hwt_hook.h>
#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_contexthash.h>
#include <dev/hwt/hwt_config.h>
#include <dev/hwt/hwt_cpu.h>
#include <dev/hwt/hwt_thread.h>
#include <dev/hwt/hwt_owner.h>
#include <dev/hwt/hwt_ownerhash.h>
#include <dev/hwt/hwt_backend.h>
#include <dev/hwt/hwt_record.h>
#include <dev/hwt/hwt_ioctl.h>
#include <dev/hwt/hwt_vm.h>

#define	HWT_IOCTL_DEBUG
#undef	HWT_IOCTL_DEBUG

#ifdef	HWT_IOCTL_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

/* No real reason for these limitations just sanity checks. */
#define	HWT_MAXBUFSIZE		(32UL * 1024 * 1024 * 1024) /* 32 GB */

static MALLOC_DEFINE(M_HWT_IOCTL, "hwt_ioctl", "Hardware Trace");

/*
 * Check if owner process *o can trace target process *t.
 */

static int
hwt_priv_check(struct proc *o, struct proc *t)
{
	struct ucred *oc, *tc;
	int error;
	int i;

	PROC_LOCK(o);
	oc = o->p_ucred;
	crhold(oc);
	PROC_UNLOCK(o);

	PROC_LOCK_ASSERT(t, MA_OWNED);
	tc = t->p_ucred;
	crhold(tc);

	error = 0;

	/*
	 * The effective uid of the HWT owner should match at least one
	 * of the effective / real / saved uids of the target process.
	 */

	if (oc->cr_uid != tc->cr_uid &&
	    oc->cr_uid != tc->cr_svuid &&
	    oc->cr_uid != tc->cr_ruid) {
		error = EPERM;
		goto done;
	}

	/*
	 * Everyone of the target's group ids must be in the owner's
	 * group list.
	 */
	for (i = 0; i < tc->cr_ngroups; i++)
		if (!groupmember(tc->cr_groups[i], oc)) {
			error = EPERM;
			goto done;
		}
	if (!groupmember(tc->cr_gid, oc) ||
	    !groupmember(tc->cr_rgid, oc) ||
	    !groupmember(tc->cr_svgid, oc)) {
		error = EPERM;
		goto done;
	}

done:
	crfree(tc);
	crfree(oc);

	return (error);
}

static int
hwt_ioctl_alloc_mode_thread(struct thread *td, struct hwt_owner *ho,
    struct hwt_backend *backend, struct hwt_alloc *halloc)
{
	struct thread **threads, *td1;
	struct hwt_record_entry *entry;
	struct hwt_context *ctx, *ctx1;
	struct hwt_thread *thr;
	char path[MAXPATHLEN];
	struct proc *p;
	int thread_id;
	int error;
	int cnt;
	int i;

	/* Check if the owner have this pid configured already. */
	ctx = hwt_owner_lookup_ctx(ho, halloc->pid);
	if (ctx)
		return (EEXIST);

	/* Allocate a new HWT context. */
	error = hwt_ctx_alloc(&ctx);
	if (error)
		return (error);
	ctx->bufsize = halloc->bufsize;
	ctx->pid = halloc->pid;
	ctx->hwt_backend = backend;
	ctx->hwt_owner = ho;
	ctx->mode = HWT_MODE_THREAD;
	ctx->hwt_td = td;
	ctx->kqueue_fd = halloc->kqueue_fd;

	error = copyout(&ctx->ident, halloc->ident, sizeof(int));
	if (error) {
		hwt_ctx_free(ctx);
		return (error);
	}

	/* Now get the victim proc. */
	p = pfind(halloc->pid);
	if (p == NULL) {
		hwt_ctx_free(ctx);
		return (ENXIO);
	}

	/* Ensure we can trace it. */
	error = hwt_priv_check(td->td_proc, p);
	if (error) {
		PROC_UNLOCK(p);
		hwt_ctx_free(ctx);
		return (error);
	}

	/* Ensure it is not being traced already. */
	ctx1 = hwt_contexthash_lookup(p);
	if (ctx1) {
		refcount_release(&ctx1->refcnt);
		PROC_UNLOCK(p);
		hwt_ctx_free(ctx);
		return (EEXIST);
	}

	/* Allocate hwt threads and buffers. */

	cnt = 0;

	FOREACH_THREAD_IN_PROC(p, td1) {
		cnt += 1;
	}

	KASSERT(cnt > 0, ("no threads"));

	threads = malloc(sizeof(struct thread *) * cnt, M_HWT_IOCTL,
	    M_NOWAIT | M_ZERO);
	if (threads == NULL) {
		PROC_UNLOCK(p);
		hwt_ctx_free(ctx);
		return (ENOMEM);
	}

	i = 0;

	FOREACH_THREAD_IN_PROC(p, td1) {
		threads[i++] = td1;
	}

	ctx->proc = p;
	PROC_UNLOCK(p);

	for (i = 0; i < cnt; i++) {
		thread_id = atomic_fetchadd_int(&ctx->thread_counter, 1);
		sprintf(path, "hwt_%d_%d", ctx->ident, thread_id);

		error = hwt_thread_alloc(&thr, path, ctx->bufsize,
		    ctx->hwt_backend->kva_req);
		if (error) {
			free(threads, M_HWT_IOCTL);
			hwt_ctx_free(ctx);
			return (error);
		}
		/* Allocate backend-specific thread data. */
		error = hwt_backend_thread_alloc(ctx, thr);
		if (error != 0) {
			dprintf("%s: failed to allocate thread backend data\n",
			    __func__);
			free(threads, M_HWT_IOCTL);
			hwt_ctx_free(ctx);
			return (error);
		}

		/*
		 * Insert a THREAD_CREATE record so userspace picks up
		 * the thread's tracing buffers.
		 */
		entry = hwt_record_entry_alloc();
		entry->record_type = HWT_RECORD_THREAD_CREATE;
		entry->thread_id = thread_id;

		thr->vm->ctx = ctx;
		thr->td = threads[i];
		thr->ctx = ctx;
		thr->backend = ctx->hwt_backend;
		thr->thread_id = thread_id;

		HWT_CTX_LOCK(ctx);
		hwt_thread_insert(ctx, thr, entry);
		HWT_CTX_UNLOCK(ctx);
	}

	free(threads, M_HWT_IOCTL);

	error = hwt_backend_init(ctx);
	if (error) {
		hwt_ctx_free(ctx);
		return (error);
	}

	/* hwt_owner_insert_ctx? */
	mtx_lock(&ho->mtx);
	LIST_INSERT_HEAD(&ho->hwts, ctx, next_hwts);
	mtx_unlock(&ho->mtx);

	/*
	 * Hooks are now in action after this, but the ctx is not in RUNNING
	 * state.
	 */
	hwt_contexthash_insert(ctx);

	p = pfind(halloc->pid);
	if (p) {
		p->p_flag2 |= P2_HWT;
		PROC_UNLOCK(p);
	}

	return (0);
}

static int
hwt_ioctl_alloc_mode_cpu(struct thread *td, struct hwt_owner *ho,
    struct hwt_backend *backend, struct hwt_alloc *halloc)
{
	struct hwt_context *ctx;
	struct hwt_cpu *cpu;
	struct hwt_vm *vm;
	char path[MAXPATHLEN];
	size_t cpusetsize;
	cpuset_t cpu_map;
	int cpu_count = 0;
	int cpu_id;
	int error;

	CPU_ZERO(&cpu_map);
	cpusetsize = min(halloc->cpusetsize, sizeof(cpuset_t));
	error = copyin(halloc->cpu_map, &cpu_map, cpusetsize);
	if (error)
		return (error);

	CPU_FOREACH_ISSET(cpu_id, &cpu_map) {
#ifdef SMP
		/* Ensure CPU is not halted. */
		if (CPU_ISSET(cpu_id, &hlt_cpus_mask))
			return (ENXIO);
#endif
#if 0
		/* TODO: Check if the owner have this cpu configured already. */
		ctx = hwt_owner_lookup_ctx_by_cpu(ho, halloc->cpu);
		if (ctx)
			return (EEXIST);
#endif

		cpu_count++;
	}

	if (cpu_count == 0)
		return (ENODEV);

	/* Allocate a new HWT context. */
	error = hwt_ctx_alloc(&ctx);
	if (error)
		return (error);
	ctx->bufsize = halloc->bufsize;
	ctx->hwt_backend = backend;
	ctx->hwt_owner = ho;
	ctx->mode = HWT_MODE_CPU;
	ctx->cpu_map = cpu_map;
	ctx->hwt_td = td;
	ctx->kqueue_fd = halloc->kqueue_fd;

	error = copyout(&ctx->ident, halloc->ident, sizeof(int));
	if (error) {
		hwt_ctx_free(ctx);
		return (error);
	}

	CPU_FOREACH_ISSET(cpu_id, &cpu_map) {
		sprintf(path, "hwt_%d_%d", ctx->ident, cpu_id);
		error = hwt_vm_alloc(ctx->bufsize, ctx->hwt_backend->kva_req,
		    path, &vm);
		if (error) {
			/* TODO: remove all allocated cpus. */
			hwt_ctx_free(ctx);
			return (error);
		}

		cpu = hwt_cpu_alloc();
		cpu->cpu_id = cpu_id;
		cpu->vm = vm;

		vm->cpu = cpu;
		vm->ctx = ctx;

		HWT_CTX_LOCK(ctx);
		hwt_cpu_insert(ctx, cpu);
		HWT_CTX_UNLOCK(ctx);
	}

	error = hwt_backend_init(ctx);
	if (error) {
		/* TODO: remove all allocated cpus. */
		hwt_ctx_free(ctx);
		return (error);
	}

	/* hwt_owner_insert_ctx? */
	mtx_lock(&ho->mtx);
	LIST_INSERT_HEAD(&ho->hwts, ctx, next_hwts);
	mtx_unlock(&ho->mtx);

	hwt_record_kernel_objects(ctx);

	return (0);
}

static int
hwt_ioctl_alloc(struct thread *td, struct hwt_alloc *halloc)
{
	char backend_name[HWT_BACKEND_MAXNAMELEN];
	struct hwt_backend *backend;
	struct hwt_owner *ho;
	int error;

	if (halloc->bufsize > HWT_MAXBUFSIZE)
		return (EINVAL);
	if (halloc->bufsize % PAGE_SIZE)
		return (EINVAL);
	if (halloc->backend_name == NULL)
		return (EINVAL);

	error = copyinstr(halloc->backend_name, (void *)backend_name,
	    HWT_BACKEND_MAXNAMELEN, NULL);
	if (error)
		return (error);

	backend = hwt_backend_lookup(backend_name);
	if (backend == NULL)
		return (ENODEV);

	/* First get the owner. */
	ho = hwt_ownerhash_lookup(td->td_proc);
	if (ho == NULL) {
		/* Create a new owner. */
		ho = hwt_owner_alloc(td->td_proc);
		if (ho == NULL)
			return (ENOMEM);
		hwt_ownerhash_insert(ho);
	}

	switch (halloc->mode) {
	case HWT_MODE_THREAD:
		error = hwt_ioctl_alloc_mode_thread(td, ho, backend, halloc);
		break;
	case HWT_MODE_CPU:
		error = hwt_ioctl_alloc_mode_cpu(td, ho, backend, halloc);
		break;
	default:
		error = ENXIO;
	};

	return (error);
}

int
hwt_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flags,
    struct thread *td)
{
	int error;

	switch (cmd) {
	case HWT_IOC_ALLOC:
		/* Allocate HWT context. */
		error = hwt_ioctl_alloc(td, (struct hwt_alloc *)addr);
		return (error);
	default:
		return (ENXIO);
	};
}
