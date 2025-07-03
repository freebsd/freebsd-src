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
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/refcount.h>
#include <sys/hwt.h>

#include <dev/hwt/hwt_hook.h>
#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_contexthash.h>
#include <dev/hwt/hwt_config.h>
#include <dev/hwt/hwt_thread.h>
#include <dev/hwt/hwt_owner.h>
#include <dev/hwt/hwt_backend.h>
#include <dev/hwt/hwt_record.h>
#include <dev/hwt/hwt_vm.h>

#define	HWT_DEBUG
#undef	HWT_DEBUG

#ifdef	HWT_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static void
hwt_switch_in(struct thread *td)
{
	struct hwt_context *ctx;
	struct hwt_thread *thr;
	struct proc *p;
	int cpu_id;

	p = td->td_proc;

	cpu_id = PCPU_GET(cpuid);

	ctx = hwt_contexthash_lookup(p);
	if (ctx == NULL)
		return;

	if (ctx->state != CTX_STATE_RUNNING) {
		hwt_ctx_put(ctx);
		return;
	}

	thr = hwt_thread_lookup(ctx, td);
	if (thr == NULL) {
		hwt_ctx_put(ctx);
		return;
	}

	dprintf("%s: thr %p index %d tid %d on cpu_id %d\n", __func__, thr,
	    thr->thread_id, td->td_tid, cpu_id);

	hwt_backend_configure(ctx, cpu_id, thr->thread_id);
	hwt_backend_enable(ctx, cpu_id);

	hwt_ctx_put(ctx);
}

static void
hwt_switch_out(struct thread *td)
{
	struct hwt_context *ctx;
	struct hwt_thread *thr;
	struct proc *p;
	int cpu_id;

	p = td->td_proc;

	cpu_id = PCPU_GET(cpuid);

	ctx = hwt_contexthash_lookup(p);
	if (ctx == NULL)
		return;

	if (ctx->state != CTX_STATE_RUNNING) {
		hwt_ctx_put(ctx);
		return;
	}
	thr = hwt_thread_lookup(ctx, td);
	if (thr == NULL) {
		hwt_ctx_put(ctx);
		return;
	}

	dprintf("%s: thr %p index %d tid %d on cpu_id %d\n", __func__, thr,
	    thr->thread_id, td->td_tid, cpu_id);

	hwt_backend_disable(ctx, cpu_id);

	hwt_ctx_put(ctx);
}

static void
hwt_hook_thread_exit(struct thread *td)
{
	struct hwt_context *ctx;
	struct hwt_thread *thr;
	struct proc *p;
	int cpu_id;

	p = td->td_proc;

	cpu_id = PCPU_GET(cpuid);

	ctx = hwt_contexthash_lookup(p);
	if (ctx == NULL)
		return;

	thr = hwt_thread_lookup(ctx, td);
	if (thr == NULL) {
		hwt_ctx_put(ctx);
		return;
	}

	thr->state = HWT_THREAD_STATE_EXITED;

	dprintf("%s: thr %p index %d tid %d on cpu_id %d\n", __func__, thr,
	    thr->thread_id, td->td_tid, cpu_id);

	if (ctx->state == CTX_STATE_RUNNING)
		hwt_backend_disable(ctx, cpu_id);

	hwt_ctx_put(ctx);
}

static void
hwt_hook_mmap(struct thread *td)
{
	struct hwt_context *ctx;
	struct hwt_thread *thr;
	struct proc *p;
	int pause;

	p = td->td_proc;

	ctx = hwt_contexthash_lookup(p);
	if (ctx == NULL)
		return;

	/* The ctx state could be any here. */

	pause = ctx->pause_on_mmap ? 1 : 0;

	thr = hwt_thread_lookup(ctx, td);
	if (thr == NULL) {
		hwt_ctx_put(ctx);
		return;
	}

	/*
	 * msleep(9) atomically releases the mtx lock, so take refcount
	 * to ensure that thr is not destroyed.
	 * It could not be destroyed prior to this call as we are holding ctx
	 * refcnt.
	 */
	refcount_acquire(&thr->refcnt);
	hwt_ctx_put(ctx);

	if (pause) {
		HWT_THR_LOCK(thr);
		msleep(thr, &thr->mtx, PCATCH, "hwt-mmap", 0);
		HWT_THR_UNLOCK(thr);
	}

	if (refcount_release(&thr->refcnt))
		hwt_thread_free(thr);
}

static int
hwt_hook_thread_create(struct thread *td)
{
	struct hwt_record_entry *entry;
	struct hwt_context *ctx;
	struct hwt_thread *thr;
	char path[MAXPATHLEN];
	size_t bufsize;
	struct proc *p;
	int thread_id, kva_req;
	int error;

	p = td->td_proc;

	/* Step 1. Get CTX and collect information needed. */
	ctx = hwt_contexthash_lookup(p);
	if (ctx == NULL)
		return (ENXIO);
	thread_id = atomic_fetchadd_int(&ctx->thread_counter, 1);
	bufsize = ctx->bufsize;
	kva_req = ctx->hwt_backend->kva_req;
	sprintf(path, "hwt_%d_%d", ctx->ident, thread_id);
	hwt_ctx_put(ctx);

	/* Step 2. Allocate some memory without holding ctx ref. */
	error = hwt_thread_alloc(&thr, path, bufsize, kva_req);
	if (error) {
		printf("%s: could not allocate thread, error %d\n",
		    __func__, error);
		return (error);
	}

	entry = hwt_record_entry_alloc();
	entry->record_type = HWT_RECORD_THREAD_CREATE;
	entry->thread_id = thread_id;

	/* Step 3. Get CTX once again. */
	ctx = hwt_contexthash_lookup(p);
	if (ctx == NULL) {
		hwt_record_entry_free(entry);
		hwt_thread_free(thr);
		/* ctx->thread_counter does not matter. */
		return (ENXIO);
	}
	/* Allocate backend-specific thread data. */
	error = hwt_backend_thread_alloc(ctx, thr);
	if (error != 0) {
		dprintf("%s: failed to allocate backend thread data\n",
			    __func__);
		return (error);
	}

	thr->vm->ctx = ctx;
	thr->ctx = ctx;
	thr->backend = ctx->hwt_backend;
	thr->thread_id = thread_id;
	thr->td = td;

	HWT_CTX_LOCK(ctx);
	hwt_thread_insert(ctx, thr, entry);
	HWT_CTX_UNLOCK(ctx);

	/* Notify userspace. */
	hwt_record_wakeup(ctx);

	hwt_ctx_put(ctx);

	return (0);
}

static void
hwt_hook_handler(struct thread *td, int func, void *arg)
{
	struct proc *p;

	p = td->td_proc;
	if ((p->p_flag2 & P2_HWT) == 0)
		return;

	switch (func) {
	case HWT_SWITCH_IN:
		hwt_switch_in(td);
		break;
	case HWT_SWITCH_OUT:
		hwt_switch_out(td);
		break;
	case HWT_THREAD_CREATE:
		hwt_hook_thread_create(td);
		break;
	case HWT_THREAD_SET_NAME:
		/* TODO. */
		break;
	case HWT_THREAD_EXIT:
		hwt_hook_thread_exit(td);
		break;
	case HWT_EXEC:
	case HWT_MMAP:
		hwt_record_td(td, arg, M_WAITOK | M_ZERO);
		hwt_hook_mmap(td);
		break;
	case HWT_RECORD:
		hwt_record_td(td, arg, M_WAITOK | M_ZERO);
		break;
	};
}

void
hwt_hook_load(void)
{

	hwt_hook = hwt_hook_handler;
}

void
hwt_hook_unload(void)
{

	hwt_hook = NULL;
}
