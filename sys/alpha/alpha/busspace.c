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

#include <sys/cdefs.h>			/* RCS ID & Copyright macro defns */

#include <sys/param.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/kobj.h>

#include <machine/bus.h>
#include "busspace_if.h"

void
busspace_generic_read_multi_1(struct alpha_busspace *space, size_t offset,
			      u_int8_t *addr, size_t count)
{
	while (count--) {
		*addr++ = space->ab_ops->abo_read_1(space, offset);
	}
}

void
busspace_generic_read_multi_2(struct alpha_busspace *space, size_t offset,
			      u_int16_t *addr, size_t count)
{
	while (count--) {
		*addr++ = space->ab_ops->abo_read_2(space, offset);
	}
}

void
busspace_generic_read_multi_4(struct alpha_busspace *space, size_t offset,
			      u_int32_t *addr, size_t count)
{
	while (count--) {
		*addr++ = space->ab_ops->abo_read_4(space, offset);
	}
}

void
busspace_generic_read_region_1(struct alpha_busspace *space, size_t offset,
			       u_int8_t *addr, size_t count)
{
	while (count--) {
		*addr++ = space->ab_ops->abo_read_1(space, offset);
		offset += 1;
	}
}

void
busspace_generic_read_region_2(struct alpha_busspace *space, size_t offset,
			       u_int16_t *addr, size_t count)
{
	while (count--) {
		*addr++ = space->ab_ops->abo_read_2(space, offset);
		offset += 2;
	}
}

void
busspace_generic_read_region_4(struct alpha_busspace *space, size_t offset,
			       u_int32_t *addr, size_t count)
{
	while (count--) {
		*addr++ = space->ab_ops->abo_read_4(space, offset);
		offset += 4;
	}
}

void
busspace_generic_write_multi_1(struct alpha_busspace *space, size_t offset,
			       const u_int8_t *addr, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_1(space, offset, *addr++);
	}
}

void
busspace_generic_write_multi_2(struct alpha_busspace *space, size_t offset,
			       const u_int16_t *addr, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_2(space, offset, *addr++);
	}
}

void
busspace_generic_write_multi_4(struct alpha_busspace *space, size_t offset,
			       const u_int32_t *addr, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_4(space, offset, *addr++);
	}
}

void
busspace_generic_write_region_1(struct alpha_busspace *space, size_t offset,
			       const u_int8_t *addr, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_1(space, offset, *addr++);
		offset += 1;
	}
}

void
busspace_generic_write_region_2(struct alpha_busspace *space, size_t offset,
			       const u_int16_t *addr, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_2(space, offset, *addr++);
		offset += 2;
	}
}

void
busspace_generic_write_region_4(struct alpha_busspace *space, size_t offset,
			       const u_int32_t *addr, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_4(space, offset, *addr++);
		offset += 4;
	}
}

void
busspace_generic_set_multi_1(struct alpha_busspace *space, size_t offset,
			     u_int8_t value, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_1(space, offset, value);
	}
}

void
busspace_generic_set_multi_2(struct alpha_busspace *space, size_t offset,
			     u_int16_t value, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_2(space, offset, value);
	}
}

void
busspace_generic_set_multi_4(struct alpha_busspace *space, size_t offset,
			     u_int32_t value, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_4(space, offset, value);
	}
}

void
busspace_generic_set_region_1(struct alpha_busspace *space, size_t offset,
			      u_int8_t value, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_1(space, offset, value);
		offset += 1;
	}
}

void
busspace_generic_set_region_2(struct alpha_busspace *space, size_t offset,
			      u_int16_t value, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_2(space, offset, value);
		offset += 2;
	}
}

void
busspace_generic_set_region_4(struct alpha_busspace *space, size_t offset,
			      u_int32_t value, size_t count)
{
	while (count--) {
		space->ab_ops->abo_write_4(space, offset, value);
		offset += 4;
	}
}

void
busspace_generic_copy_region_1(struct alpha_busspace *space,
			       size_t offset1, size_t offset2, size_t count)
{
	u_int8_t value;
	if (offset1 > offset2) {
		while (count--) {
			value = space->ab_ops->abo_read_1(space, offset1);
			space->ab_ops->abo_write_1(space, offset2, value);
			offset1 += 1;
			offset2 += 1;
		}
	} else {
		offset1 += count - 1;
		offset2 += count - 1;
		while (count--) {
			value = space->ab_ops->abo_read_1(space, offset1);
			space->ab_ops->abo_write_1(space, offset2, value);
			offset1 -= 1;
			offset2 -= 1;
		}
	}
}

void
busspace_generic_copy_region_2(struct alpha_busspace *space,
			       size_t offset1, size_t offset2, size_t count)
{
	u_int16_t value;
	if (offset1 > offset2) {
		while (count--) {
			value = space->ab_ops->abo_read_1(space, offset1);
			space->ab_ops->abo_write_1(space, offset2, value);
			offset1 += 2;
			offset2 += 2;
		}
	} else {
		offset1 += 2*(count - 1);
		offset2 += 2*(count - 1);
		while (count--) {
			value = space->ab_ops->abo_read_2(space, offset1);
			space->ab_ops->abo_write_2(space, offset2, value);
			offset1 -= 2;
			offset2 -= 2;
		}
	}
}

void
busspace_generic_copy_region_4(struct alpha_busspace *space,
			       size_t offset1, size_t offset2, size_t count)
{
	u_int32_t value;
	if (offset1 > offset2) {
		while (count--) {
			value = space->ab_ops->abo_read_4(space, offset1);
			space->ab_ops->abo_write_4(space, offset2, value);
			offset1 += 4;
			offset2 += 4;
		}
	} else {
		offset1 += 4*(count - 1);
		offset2 += 4*(count - 1);
		while (count--) {
			value = space->ab_ops->abo_read_4(space, offset1);
			space->ab_ops->abo_write_4(space, offset2, value);
			offset1 -= 4;
			offset2 -= 4;
		}
	}
}

void
busspace_generic_barrier(struct alpha_busspace *space, size_t offset, size_t len, int flags)
{
	if (flags & BUS_SPACE_BARRIER_READ)
		alpha_mb();
	else
		alpha_wmb();
}
