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

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
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
#include <dev/hwt/hwt_vm.h>
#include <dev/hwt/hwt_record.h>

#define	HWT_DEBUG
#undef	HWT_DEBUG

#ifdef	HWT_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static MALLOC_DEFINE(M_HWT_OWNER, "hwt_owner", "Hardware Trace");

struct hwt_context *
hwt_owner_lookup_ctx(struct hwt_owner *ho, pid_t pid)
{
	struct hwt_context *ctx;

	mtx_lock(&ho->mtx);
	LIST_FOREACH(ctx, &ho->hwts, next_hwts) {
		if (ctx->pid == pid) {
			mtx_unlock(&ho->mtx);
			return (ctx);
		}
	}
	mtx_unlock(&ho->mtx);

	return (NULL);
}

#if 0
struct hwt_context *
hwt_owner_lookup_ctx_by_cpu(struct hwt_owner *ho, int cpu)
{
	struct hwt_context *ctx;

	mtx_lock(&ho->mtx);
	LIST_FOREACH(ctx, &ho->hwts, next_hwts) {
		if (ctx->cpu == cpu) {
			mtx_unlock(&ho->mtx);
			return (ctx);
		}
	}
	mtx_unlock(&ho->mtx);

	return (NULL);
}
#endif

struct hwt_owner *
hwt_owner_alloc(struct proc *p)
{
	struct hwt_owner *ho;

	ho = malloc(sizeof(struct hwt_owner), M_HWT_OWNER,
	    M_WAITOK | M_ZERO);
	ho->p = p;

	LIST_INIT(&ho->hwts);
	mtx_init(&ho->mtx, "hwts", NULL, MTX_DEF);

	return (ho);
}

void
hwt_owner_shutdown(struct hwt_owner *ho)
{
	struct hwt_context *ctx;

	dprintf("%s: stopping hwt owner\n", __func__);

	while (1) {
		mtx_lock(&ho->mtx);
		ctx = LIST_FIRST(&ho->hwts);
		if (ctx)
			LIST_REMOVE(ctx, next_hwts);
		mtx_unlock(&ho->mtx);

		if (ctx == NULL)
			break;

		if (ctx->mode == HWT_MODE_THREAD)
			hwt_contexthash_remove(ctx);

		/*
		 * A hook could be still dealing with this ctx right here.
		 */

		HWT_CTX_LOCK(ctx);
		ctx->state = 0;
		HWT_CTX_UNLOCK(ctx);

		/* Ensure hooks invocation is now completed. */
		while (refcount_load(&ctx->refcnt) > 0)
			continue;

		/*
		 * Note that a thread could be still sleeping on msleep(9).
		 */

		hwt_backend_deinit(ctx);
		hwt_record_free_all(ctx);
		hwt_ctx_free(ctx);
	}

	hwt_ownerhash_remove(ho);
	free(ho, M_HWT_OWNER);
}
