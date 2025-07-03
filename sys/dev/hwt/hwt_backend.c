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
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/hwt.h>

#include <dev/hwt/hwt_hook.h>
#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_config.h>
#include <dev/hwt/hwt_thread.h>
#include <dev/hwt/hwt_backend.h>

#define	HWT_BACKEND_DEBUG
#undef	HWT_BACKEND_DEBUG

#ifdef	HWT_BACKEND_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static struct mtx hwt_backend_mtx;

struct hwt_backend_entry {
	struct hwt_backend *backend;
	LIST_ENTRY(hwt_backend_entry) next;
};

static LIST_HEAD(, hwt_backend_entry)	hwt_backends;

static MALLOC_DEFINE(M_HWT_BACKEND, "hwt_backend", "HWT backend");

int
hwt_backend_init(struct hwt_context *ctx)
{
	int error;

	dprintf("%s\n", __func__);

	error = ctx->hwt_backend->ops->hwt_backend_init(ctx);

	return (error);
}

void
hwt_backend_deinit(struct hwt_context *ctx)
{

	dprintf("%s\n", __func__);

	ctx->hwt_backend->ops->hwt_backend_deinit(ctx);
}

int
hwt_backend_configure(struct hwt_context *ctx, int cpu_id, int thread_id)
{
	int error;

	dprintf("%s\n", __func__);

	error = ctx->hwt_backend->ops->hwt_backend_configure(ctx, cpu_id,
	    thread_id);

	return (error);
}

void
hwt_backend_enable(struct hwt_context *ctx, int cpu_id)
{

	dprintf("%s\n", __func__);

	ctx->hwt_backend->ops->hwt_backend_enable(ctx, cpu_id);
}

void
hwt_backend_disable(struct hwt_context *ctx, int cpu_id)
{

	dprintf("%s\n", __func__);

	ctx->hwt_backend->ops->hwt_backend_disable(ctx, cpu_id);
}

void
hwt_backend_enable_smp(struct hwt_context *ctx)
{

	dprintf("%s\n", __func__);

	ctx->hwt_backend->ops->hwt_backend_enable_smp(ctx);
}

void
hwt_backend_disable_smp(struct hwt_context *ctx)
{

	dprintf("%s\n", __func__);

	ctx->hwt_backend->ops->hwt_backend_disable_smp(ctx);
}

void __unused
hwt_backend_dump(struct hwt_context *ctx, int cpu_id)
{

	dprintf("%s\n", __func__);

	ctx->hwt_backend->ops->hwt_backend_dump(cpu_id);
}

int
hwt_backend_read(struct hwt_context *ctx, struct hwt_vm *vm, int *ident,
    vm_offset_t *offset, uint64_t *data)
{
	int error;

	dprintf("%s\n", __func__);

	error = ctx->hwt_backend->ops->hwt_backend_read(vm, ident,
	    offset, data);

	return (error);
}

struct hwt_backend *
hwt_backend_lookup(const char *name)
{
	struct hwt_backend_entry *entry;
	struct hwt_backend *backend;

	HWT_BACKEND_LOCK();
	LIST_FOREACH(entry, &hwt_backends, next) {
		backend = entry->backend;
		if (strcmp(backend->name, name) == 0) {
			HWT_BACKEND_UNLOCK();
			return (backend);
		}
	}
	HWT_BACKEND_UNLOCK();

	return (NULL);
}

int
hwt_backend_register(struct hwt_backend *backend)
{
	struct hwt_backend_entry *entry;

	if (backend == NULL ||
	    backend->name == NULL ||
	    backend->ops == NULL)
		return (EINVAL);

	entry = malloc(sizeof(struct hwt_backend_entry), M_HWT_BACKEND,
	    M_WAITOK | M_ZERO);
	entry->backend = backend;

	HWT_BACKEND_LOCK();
	LIST_INSERT_HEAD(&hwt_backends, entry, next);
	HWT_BACKEND_UNLOCK();

	return (0);
}

int
hwt_backend_unregister(struct hwt_backend *backend)
{
	struct hwt_backend_entry *entry, *tmp;

	if (backend == NULL)
		return (EINVAL);

	/* TODO: check if not in use */

	HWT_BACKEND_LOCK();
	LIST_FOREACH_SAFE(entry, &hwt_backends, next, tmp) {
		if (entry->backend == backend) {
			LIST_REMOVE(entry, next);
			HWT_BACKEND_UNLOCK();
			free(entry, M_HWT_BACKEND);
			return (0);
		}
	}
	HWT_BACKEND_UNLOCK();

	return (ENOENT);
}

void
hwt_backend_load(void)
{

	mtx_init(&hwt_backend_mtx, "hwt backend", NULL, MTX_DEF);
	LIST_INIT(&hwt_backends);
}

void
hwt_backend_unload(void)
{

	/* TODO: ensure all unregistered */

	mtx_destroy(&hwt_backend_mtx);
}

void
hwt_backend_stop(struct hwt_context *ctx)
{
	dprintf("%s\n", __func__);

	ctx->hwt_backend->ops->hwt_backend_stop(ctx);
}

int
hwt_backend_svc_buf(struct hwt_context *ctx, void *data, size_t data_size,
    int data_version)
{
	int error;

	dprintf("%s\n", __func__);

	error = ctx->hwt_backend->ops->hwt_backend_svc_buf(ctx, data, data_size,
	    data_version);

	return (error);
}

int
hwt_backend_thread_alloc(struct hwt_context *ctx, struct hwt_thread *thr)
{
	int error;

	dprintf("%s\n", __func__);

	if (ctx->hwt_backend->ops->hwt_backend_thread_alloc == NULL)
		return (0);
	KASSERT(thr->private == NULL,
		    ("%s: thread private data is not NULL\n", __func__));
	error = ctx->hwt_backend->ops->hwt_backend_thread_alloc(thr);

	return (error);
}

void
hwt_backend_thread_free(struct hwt_thread *thr)
{
	dprintf("%s\n", __func__);

	if (thr->backend->ops->hwt_backend_thread_free == NULL)
		return;
	KASSERT(thr->private != NULL,
		    ("%s: thread private data is NULL\n", __func__));
	thr->backend->ops->hwt_backend_thread_free(thr);

	return;
}
