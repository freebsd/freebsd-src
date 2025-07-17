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
#include <sys/bitstring.h>
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/mman.h>
#include <sys/mutex.h>
#include <sys/refcount.h>
#include <sys/rwlock.h>
#include <sys/hwt.h>

#include <dev/hwt/hwt_hook.h>
#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_config.h>
#include <dev/hwt/hwt_thread.h>
#include <dev/hwt/hwt_owner.h>
#include <dev/hwt/hwt_vm.h>
#include <dev/hwt/hwt_cpu.h>

#define	HWT_DEBUG
#undef	HWT_DEBUG

#ifdef	HWT_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static MALLOC_DEFINE(M_HWT_CTX, "hwt_ctx", "Hardware Trace");

static bitstr_t *ident_set;
static int ident_set_size;
static struct mtx ident_set_mutex;

static int
hwt_ctx_ident_alloc(int *new_ident)
{

	mtx_lock(&ident_set_mutex);
	bit_ffc(ident_set, ident_set_size, new_ident);
	if (*new_ident == -1) {
		mtx_unlock(&ident_set_mutex);
		return (ENOMEM);
	}
	bit_set(ident_set, *new_ident);
	mtx_unlock(&ident_set_mutex);

	return (0);
}

static void
hwt_ctx_ident_free(int ident)
{

	mtx_lock(&ident_set_mutex);
	bit_clear(ident_set, ident);
	mtx_unlock(&ident_set_mutex);
}

int
hwt_ctx_alloc(struct hwt_context **ctx0)
{
	struct hwt_context *ctx;
	int error;

	ctx = malloc(sizeof(struct hwt_context), M_HWT_CTX, M_WAITOK | M_ZERO);

	TAILQ_INIT(&ctx->records);
	TAILQ_INIT(&ctx->threads);
	TAILQ_INIT(&ctx->cpus);
	mtx_init(&ctx->mtx, "ctx", NULL, MTX_SPIN);
	mtx_init(&ctx->rec_mtx, "ctx_rec", NULL, MTX_DEF);
	refcount_init(&ctx->refcnt, 0);

	error = hwt_ctx_ident_alloc(&ctx->ident);
	if (error) {
		printf("could not allocate ident bit str\n");
		return (error);
	}

	*ctx0 = ctx;

	return (0);
}

static void
hwt_ctx_free_cpus(struct hwt_context *ctx)
{
	struct hwt_cpu *cpu;

	do {
		HWT_CTX_LOCK(ctx);
		cpu = TAILQ_FIRST(&ctx->cpus);
		if (cpu)
			TAILQ_REMOVE(&ctx->cpus, cpu, next);
		HWT_CTX_UNLOCK(ctx);

		if (cpu == NULL)
			break;

		/* TODO: move vm_free() to cpu_free()? */
		hwt_vm_free(cpu->vm);
		hwt_cpu_free(cpu);
	} while (1);
}

static void
hwt_ctx_free_threads(struct hwt_context *ctx)
{
	struct hwt_thread *thr;

	dprintf("%s: remove threads\n", __func__);

	do {
		HWT_CTX_LOCK(ctx);
		thr = TAILQ_FIRST(&ctx->threads);
		if (thr)
			TAILQ_REMOVE(&ctx->threads, thr, next);
		HWT_CTX_UNLOCK(ctx);

		if (thr == NULL)
			break;

		HWT_THR_LOCK(thr);
		/* TODO: check if thr is sleeping before waking it up. */
		wakeup(thr);
		HWT_THR_UNLOCK(thr);

		if (refcount_release(&thr->refcnt))
			hwt_thread_free(thr);
	} while (1);
}

void
hwt_ctx_free(struct hwt_context *ctx)
{

	if (ctx->mode == HWT_MODE_CPU)
		hwt_ctx_free_cpus(ctx);
	else
		hwt_ctx_free_threads(ctx);

	hwt_config_free(ctx);
	hwt_ctx_ident_free(ctx->ident);
	free(ctx, M_HWT_CTX);
}

void
hwt_ctx_put(struct hwt_context *ctx)
{

	refcount_release(&ctx->refcnt);
}

void
hwt_ctx_load(void)
{

	ident_set_size = (1 << 8);
	ident_set = bit_alloc(ident_set_size, M_HWT_CTX, M_WAITOK);
	mtx_init(&ident_set_mutex, "ident set", NULL, MTX_DEF);
}

void
hwt_ctx_unload(void)
{

	mtx_destroy(&ident_set_mutex);
	free(ident_set, M_HWT_CTX);
}
