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
#include <machine/bwx.h>

static u_int8_t
bwx_read_1(struct alpha_busspace *space, size_t offset)
{
	struct bwx_space *bwx = (struct bwx_space *) space;
	alpha_mb();
	return ldbu(bwx->base + offset);
}

static u_int16_t
bwx_read_2(struct alpha_busspace *space, size_t offset)
{
	struct bwx_space *bwx = (struct bwx_space *) space;
	alpha_mb();
	return ldwu(bwx->base + offset);
}

static u_int32_t
bwx_read_4(struct alpha_busspace *space, size_t offset)
{
	struct bwx_space *bwx = (struct bwx_space *) space;
	alpha_mb();
	return ldl(bwx->base + offset);
}

static void
bwx_write_1(struct alpha_busspace *space, size_t offset, u_int8_t data)
{
	struct bwx_space *bwx = (struct bwx_space *) space;
	stb(bwx->base + offset, data);
	alpha_mb();
}

static void
bwx_write_2(struct alpha_busspace *space, size_t offset, u_int16_t data)
{
	struct bwx_space *bwx = (struct bwx_space *) space;
	stw(bwx->base + offset, data);
	alpha_mb();
}

static void
bwx_write_4(struct alpha_busspace *space, size_t offset, u_int32_t data)
{
	struct bwx_space *bwx = (struct bwx_space *) space;
	stl(bwx->base + offset, data);
	alpha_mb();
}

static struct alpha_busspace_ops bwx_space_ops = {
	bwx_read_1,
	bwx_read_2,
	bwx_read_4,

	busspace_generic_read_multi_1,
	busspace_generic_read_multi_2,
	busspace_generic_read_multi_4,

	busspace_generic_read_region_1,
	busspace_generic_read_region_2,
	busspace_generic_read_region_4,

	bwx_write_1,
	bwx_write_2,
	bwx_write_4,

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
bwx_init_space(struct bwx_space *bwx, u_int64_t base)
{
	bwx->ops = &bwx_space_ops;
	bwx->base = base;
}

