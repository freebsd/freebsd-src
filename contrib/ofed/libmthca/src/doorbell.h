/*
 * Copyright (c) 2004, 2005 Topspin Communications.  All rights reserved.
 *
 * This software is available to you under a choice of one of two
 * licenses.  You may choose to be licensed under the terms of the GNU
 * General Public License (GPL) Version 2, available from the file
 * COPYING in the main directory of this source tree, or the
 * OpenIB.org BSD license below:
 *
 *     Redistribution and use in source and binary forms, with or
 *     without modification, are permitted provided that the following
 *     conditions are met:
 *
 *      - Redistributions of source code must retain the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer.
 *
 *      - Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials
 *        provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS
 * BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN
 * ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef DOORBELL_H
#define DOORBELL_H

#ifdef __i386__

static inline void mthca_write64(uint32_t val[2], struct mthca_context *ctx, int offset)
{
	/* i386 stack is aligned to 8 bytes, so this should be OK: */
	uint8_t xmmsave[8] __attribute__((aligned(8)));

	asm volatile (
		"movlps %%xmm0,(%0); \n\t"
		"movlps (%1),%%xmm0; \n\t"
		"movlps %%xmm0,(%2); \n\t"
		"movlps (%0),%%xmm0; \n\t"
		:
		: "r" (xmmsave), "r" (val), "r" (ctx->uar + offset)
		: "memory" );
}

static inline void mthca_write_db_rec(uint32_t val[2], uint32_t *db)
{
	/* i386 stack is aligned to 8 bytes, so this should be OK: */
	uint8_t xmmsave[8] __attribute__((aligned(8)));

	asm volatile (
		"movlps %%xmm0,(%0); \n\t"
		"movlps (%1),%%xmm0; \n\t"
		"movlps %%xmm0,(%2); \n\t"
		"movlps (%0),%%xmm0; \n\t"
		:
		: "r" (xmmsave), "r" (val), "r" (db)
		: "memory" );
}

#elif SIZEOF_LONG == 8

#if __BYTE_ORDER == __LITTLE_ENDIAN
#  define MTHCA_PAIR_TO_64(val) ((uint64_t) val[1] << 32 | val[0])
#elif __BYTE_ORDER == __BIG_ENDIAN
#  define MTHCA_PAIR_TO_64(val) ((uint64_t) val[0] << 32 | val[1])
#else
#  error __BYTE_ORDER not defined
#endif

static inline void mthca_write64(uint32_t val[2], struct mthca_context *ctx, int offset)
{
	*(volatile uint64_t *) (ctx->uar + offset) = MTHCA_PAIR_TO_64(val);
}

static inline void mthca_write_db_rec(uint32_t val[2], uint32_t *db)
{
	*(volatile uint64_t *) db = MTHCA_PAIR_TO_64(val);
}

#else

static inline void mthca_write64(uint32_t val[2], struct mthca_context *ctx, int offset)
{
	pthread_spin_lock(&ctx->uar_lock);
	*(volatile uint32_t *) (ctx->uar + offset)     = val[0];
	*(volatile uint32_t *) (ctx->uar + offset + 4) = val[1];
	pthread_spin_unlock(&ctx->uar_lock);
}

static inline void mthca_write_db_rec(uint32_t val[2], uint32_t *db)
{
	*(volatile uint32_t *) db       = val[0];
	mb();
	*(volatile uint32_t *) (db + 1) = val[1];
}

#endif

#endif /* MTHCA_H */
