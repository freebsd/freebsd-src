/*
 * Copyright (c) 2011 Konstantin Belousov <kib@FreeBSD.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/ucontext.h>
#include <errno.h>
#include <stdlib.h>

int
__getcontextx_size(void)
{
	size_t size;

	size = sizeof(ucontext_t);
	size += sizeof(struct arm64_reg_context); /* Space for ARM64_CTX_END */

	return (size);
}

int
__fillcontextx2(char *ctx)
{
	struct arm64_reg_context *reg_ctx;
	ucontext_t *ucp;

	ucp = (ucontext_t *)ctx;
	ucp->uc_mcontext.mc_ptr = (uint64_t)(ucp + 1);

	reg_ctx = (struct arm64_reg_context *)ucp->uc_mcontext.mc_ptr;
	reg_ctx->ctx_id = ARM64_CTX_END;
	reg_ctx->ctx_size = sizeof(struct arm64_reg_context);

	return (0);
}

int
__fillcontextx(char *ctx)
{
	ucontext_t *ucp;

	ucp = (ucontext_t *)ctx;
	if (getcontext(ucp) == -1)
		return (-1);
	__fillcontextx2(ctx);
	return (0);
}

__weak_reference(__getcontextx, getcontextx);

ucontext_t *
__getcontextx(void)
{
	char *ctx;
	int error;

	ctx = malloc(__getcontextx_size());
	if (ctx == NULL)
		return (NULL);
	if (__fillcontextx(ctx) == -1) {
		error = errno;
		free(ctx);
		errno = error;
		return (NULL);
	}
	return ((ucontext_t *)ctx);
}
