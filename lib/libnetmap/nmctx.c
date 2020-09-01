/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (C) 2018 Universita` di Pisa
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright
 *      notice, this list of conditions and the following disclaimer.
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <net/netmap_user.h>
#define LIBNETMAP_NOTHREADSAFE
#include "libnetmap.h"

static void
nmctx_default_error(struct nmctx *ctx, const char *errmsg)
{
	fprintf(stderr, "%s\n", errmsg);
}

static void *
nmctx_default_malloc(struct nmctx *ctx, size_t sz)
{
	(void)ctx;
	return malloc(sz);
}

static void
nmctx_default_free(struct nmctx *ctx, void *p)
{
	(void)ctx;
	free(p);
}

static struct nmctx nmctx_global = {
	.verbose = 1,
	.error = nmctx_default_error,
	.malloc = nmctx_default_malloc,
	.free = nmctx_default_free,
	.lock = NULL,
};

static struct nmctx *nmctx_default = &nmctx_global;

struct nmctx *
nmctx_get(void)
{
	return nmctx_default;
}

struct nmctx *
nmctx_set_default(struct nmctx *ctx)
{
	struct nmctx *old = nmctx_default;
	nmctx_default = ctx;
	return old;
}

#define MAXERRMSG 1000
void
nmctx_ferror(struct nmctx *ctx, const char *fmt, ...)
{
	char errmsg[MAXERRMSG];
	va_list ap;
	int rv;

	if (!ctx->verbose)
		return;

	va_start(ap, fmt);
	rv = vsnprintf(errmsg, MAXERRMSG, fmt, ap);
	va_end(ap);

	if (rv > 0) {
		if (rv < MAXERRMSG) {
			ctx->error(ctx, errmsg);
		} else {
			ctx->error(ctx, "error message too long");
		}
	} else {
		ctx->error(ctx, "internal error");
	}
}

void *
nmctx_malloc(struct nmctx *ctx, size_t sz)
{
	return ctx->malloc(ctx, sz);
}

void
nmctx_free(struct nmctx *ctx, void *p)
{
	ctx->free(ctx, p);
}

void
nmctx_lock(struct nmctx *ctx)
{
	if (ctx->lock != NULL)
		ctx->lock(ctx, 1);
}

void
nmctx_unlock(struct nmctx *ctx)
{
	if (ctx->lock != NULL)
		ctx->lock(ctx, 0);
}
