/*-
 * Copyright (c) 2000 Doug Rabson
 * All rights reserved.
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
 *
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/kobj.h>

#include <machine/bus.h>
#include <machine/swiz.h>

static u_int8_t
swiz_read_1(struct alpha_busspace *space, size_t offset)
{
	struct swiz_space *swiz = (struct swiz_space *) space;
	alpha_mb();
	if (swiz->sethae)
		offset = swiz->sethae(swiz->arg, offset);
	return SPARSE_READ_BYTE(swiz->base, offset);
}

static u_int16_t
swiz_read_2(struct alpha_busspace *space, size_t offset)
{
	struct swiz_space *swiz = (struct swiz_space *) space;
	alpha_mb();
	if (swiz->sethae)
		offset = swiz->sethae(swiz->arg, offset);
	return SPARSE_READ_WORD(swiz->base, offset);
}

static u_int32_t
swiz_read_4(struct alpha_busspace *space, size_t offset)
{
	struct swiz_space *swiz = (struct swiz_space *) space;
	alpha_mb();
	if (swiz->sethae)
		offset = swiz->sethae(swiz->arg, offset);
	return SPARSE_READ_LONG(swiz->base, offset);
}

static void
swiz_write_1(struct alpha_busspace *space, size_t offset, u_int8_t data)
{
	struct swiz_space *swiz = (struct swiz_space *) space;
	if (swiz->sethae)
		offset = swiz->sethae(swiz->arg, offset);
	SPARSE_WRITE_BYTE(swiz->base, offset, data);
	alpha_mb();
}

static void
swiz_write_2(struct alpha_busspace *space, size_t offset, u_int16_t data)
{
	struct swiz_space *swiz = (struct swiz_space *) space;
	if (swiz->sethae)
		offset = swiz->sethae(swiz->arg, offset);
	SPARSE_WRITE_WORD(swiz->base, offset, data);
	alpha_mb();
}

static void
swiz_write_4(struct alpha_busspace *space, size_t offset, u_int32_t data)
{
	struct swiz_space *swiz = (struct swiz_space *) space;
	if (swiz->sethae)
		offset = swiz->sethae(swiz->arg, offset);
	SPARSE_WRITE_LONG(swiz->base, offset, data);
	alpha_mb();
}

static struct alpha_busspace_ops swiz_space_ops = {
	swiz_read_1,
	swiz_read_2,
	swiz_read_4,

	busspace_generic_read_multi_1,
	busspace_generic_read_multi_2,
	busspace_generic_read_multi_4,

	busspace_generic_read_region_1,
	busspace_generic_read_region_2,
	busspace_generic_read_region_4,

	swiz_write_1,
	swiz_write_2,
	swiz_write_4,

	busspace_generic_write_multi_1,
	busspace_generic_write_multi_2,
	busspace_generic_write_multi_4,

	busspace_generic_write_region_1,
	busspace_generic_write_region_2,
	busspace_generic_write_region_4,

	busspace_generic_set_multi_1,
	busspace_generic_set_multi_2,
	busspace_generic_set_multi_4,
    
	busspace_generic_set_region_1,
	busspace_generic_set_region_2,
	busspace_generic_set_region_4,
    
	busspace_generic_copy_region_1,
	busspace_generic_copy_region_2,
	busspace_generic_copy_region_4,

	busspace_generic_barrier,
};

void
swiz_init_space(struct swiz_space *swiz, u_int64_t base)
{
	swiz->ops = &swiz_space_ops;
	swiz->base = base;
	swiz->sethae = 0;
	swiz->arg = 0;
}

void swiz_init_space_hae(struct swiz_space *swiz, u_int64_t base,
			 swiz_sethae_fn sethae, void *arg)
{
	swiz->ops = &swiz_space_ops;
	swiz->base = base;
	swiz->sethae = sethae;
	swiz->arg = arg;
}

