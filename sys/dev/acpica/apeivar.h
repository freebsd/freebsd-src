/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2020 Alexander Motin <mav@FreeBSD.org>
 * Copyright (c) 2025 Netflix, Inc.
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

#ifndef __APEIVAR_H__
#define	__APEIVAR_H__

#include "apei_if.h"

struct resource_map *apei_map_register(device_t dev, ACPI_GENERIC_ADDRESS *gas);
struct resource_map *apei_map_memory(device_t dev, rman_res_t start,
    rman_res_t len);
int	apei_unmap_register(device_t dev, struct resource_map *map);

#ifdef __i386__
static __inline uint64_t
bus_space_read_8(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t offset)
{
	return (bus_space_read_4(tag, bsh, offset) |
	    ((uint64_t)bus_space_read_4(tag, bsh, offset + 4)) << 32);
}
static __inline void
bus_space_write_8(bus_space_tag_t tag, bus_space_handle_t bsh,
    bus_size_t offset, uint64_t val)
{
	bus_space_write_4(tag, bsh, offset, val);
	bus_space_write_4(tag, bsh, offset + 4, val >> 32);
}
#endif

#endif /* !__APEIVAR_H__ */
