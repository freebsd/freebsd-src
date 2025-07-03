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
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/hwt.h>

#include <vm/vm.h>

#include <dev/hwt/hwt_hook.h>
#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_contexthash.h>
#include <dev/hwt/hwt_config.h>
#include <dev/hwt/hwt_thread.h>
#include <dev/hwt/hwt_record.h>
#include <dev/hwt/hwt_cpu.h>

#define	HWT_CPU_DEBUG
#undef	HWT_CPU_DEBUG

#ifdef	HWT_CPU_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static MALLOC_DEFINE(M_HWT_CPU, "hwt_cpu", "HWT cpu");

struct hwt_cpu *
hwt_cpu_alloc(void)
{
	struct hwt_cpu *cpu;

	cpu = malloc(sizeof(struct hwt_cpu), M_HWT_CPU, M_WAITOK | M_ZERO);

	return (cpu);
}

void
hwt_cpu_free(struct hwt_cpu *cpu)
{

	free(cpu, M_HWT_CPU);
}

struct hwt_cpu *
hwt_cpu_first(struct hwt_context *ctx)
{
	struct hwt_cpu *cpu;

	HWT_CTX_ASSERT_LOCKED(ctx);

	cpu = TAILQ_FIRST(&ctx->cpus);

	KASSERT(cpu != NULL, ("cpu is NULL"));

	return (cpu);
}

struct hwt_cpu *
hwt_cpu_get(struct hwt_context *ctx, int cpu_id)
{
	struct hwt_cpu *cpu, *tcpu;

	HWT_CTX_ASSERT_LOCKED(ctx);

	TAILQ_FOREACH_SAFE(cpu, &ctx->cpus, next, tcpu) {
		KASSERT(cpu != NULL, ("cpu is NULL"));
		if (cpu->cpu_id == cpu_id) {
			return cpu;
		}
	}

	return (NULL);
}

void
hwt_cpu_insert(struct hwt_context *ctx, struct hwt_cpu *cpu)
{

	HWT_CTX_ASSERT_LOCKED(ctx);

	TAILQ_INSERT_TAIL(&ctx->cpus, cpu, next);
}
