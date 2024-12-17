/*-
 * Copyright (c) 2024 Kyle Evans <kevans@FreeBSD.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "libder_private.h"

#include <stdlib.h>
#include <unistd.h>

/*
 * Sets up the context, returns NULL on error.
 */
struct libder_ctx *
libder_open(void)
{
	struct libder_ctx *ctx;

	ctx = malloc(sizeof(*ctx));
	if (ctx == NULL)
		return (NULL);

	/* Initialize */
	ctx->error = LDE_NONE;
	ctx->buffer_size = 0;
	ctx->verbose = 0;
	ctx->normalize = LIBDER_NORMALIZE_ALL;
	ctx->strict = true;
	ctx->abort = 0;

	return (ctx);
}

void
libder_abort(struct libder_ctx *ctx)
{

	ctx->abort = 1;
}

LIBDER_PRIVATE size_t
libder_get_buffer_size(struct libder_ctx *ctx)
{

	if (ctx->buffer_size == 0) {
		long psize;

		psize = sysconf(_SC_PAGESIZE);
		if (psize <= 0)
			psize = 4096;

		ctx->buffer_size = psize;
	}

	return (ctx->buffer_size);
}

uint64_t
libder_get_normalize(struct libder_ctx *ctx)
{

	return (ctx->normalize);
}

/*
 * Set the normalization flags; returns the previous value.
 */
uint64_t
libder_set_normalize(struct libder_ctx *ctx, uint64_t nmask)
{
	uint64_t old = ctx->normalize;

	ctx->normalize = (nmask & LIBDER_NORMALIZE_ALL);
	return (old);
}

bool
libder_get_strict(struct libder_ctx *ctx)
{

	return (ctx->strict);
}

bool
libder_set_strict(struct libder_ctx *ctx, bool strict)
{
	bool oval = ctx->strict;

	ctx->strict = strict;
	return (oval);
}

int
libder_get_verbose(struct libder_ctx *ctx)
{

	return (ctx->verbose);
}

int
libder_set_verbose(struct libder_ctx *ctx, int verbose)
{
	int oval = ctx->verbose;

	ctx->verbose = verbose;
	return (oval);
}

void
libder_close(struct libder_ctx *ctx)
{

	if (ctx == NULL)
		return;

	free(ctx);
}

