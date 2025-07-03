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
#include <sys/malloc.h>
#include <sys/mutex.h>
#include <sys/lock.h>
#include <sys/hwt.h>

#include <vm/vm.h>

#include <dev/hwt/hwt_hook.h>
#include <dev/hwt/hwt_context.h>
#include <dev/hwt/hwt_contexthash.h>
#include <dev/hwt/hwt_config.h>
#include <dev/hwt/hwt_thread.h>
#include <dev/hwt/hwt_record.h>

#define	HWT_MAXCONFIGSIZE	PAGE_SIZE

#define	HWT_CONFIG_DEBUG
#undef	HWT_CONFIG_DEBUG

#ifdef	HWT_CONFIG_DEBUG
#define	dprintf(fmt, ...)	printf(fmt, ##__VA_ARGS__)
#else
#define	dprintf(fmt, ...)
#endif

static MALLOC_DEFINE(M_HWT_CONFIG, "hwt_config", "HWT config");

int
hwt_config_set(struct thread *td, struct hwt_context *ctx,
    struct hwt_set_config *sconf)
{
	size_t config_size;
	void *old_config;
	void *config;
	int error;

	config_size = sconf->config_size;
	if (config_size == 0)
		return (0);

	if (config_size > HWT_MAXCONFIGSIZE)
		return (EFBIG);

	config = malloc(config_size, M_HWT_CONFIG, M_WAITOK | M_ZERO);

	error = copyin(sconf->config, config, config_size);
	if (error) {
		free(config, M_HWT_CONFIG);
		return (error);
	}

	HWT_CTX_LOCK(ctx);
	old_config = ctx->config;
	ctx->config = config;
	ctx->config_size = sconf->config_size;
	ctx->config_version = sconf->config_version;
	HWT_CTX_UNLOCK(ctx);

	if (old_config != NULL)
		free(old_config, M_HWT_CONFIG);

	return (error);
}

void
hwt_config_free(struct hwt_context *ctx)
{

	if (ctx->config == NULL)
		return;

	free(ctx->config, M_HWT_CONFIG);

	ctx->config = NULL;
}
