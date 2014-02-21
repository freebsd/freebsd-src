/*-
 * Copyright (c) 2009 Marcel Moolenaar
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/types.h>
#include <machine/bus.h>
#include <vm/vm.h>
#include <vm/pmap.h>

extern u_long ia64_port_base;

#define __PIO_ADDR(port)        \
        (void *)(ia64_port_base | (((port) & 0xfffc) << 10) | ((port) & 0xFFF))

int
bus_space_map(bus_space_tag_t bst, bus_addr_t addr, bus_size_t size,
    int flags __unused, bus_space_handle_t *bshp)
{

        *bshp = (__predict_false(bst == IA64_BUS_SPACE_IO))
            ? addr : (uintptr_t)pmap_mapdev(addr, size);
        return (0);
}


void
bus_space_unmap(bus_space_tag_t bst __unused, bus_space_handle_t bsh,
    bus_size_t size)
{

	pmap_unmapdev(bsh, size);
}

uint8_t
bus_space_read_io_1(u_long port)
{
	uint8_t v;

	ia64_mf();
	v = ia64_ld1(__PIO_ADDR(port));
	ia64_mf_a();
	ia64_mf();
	return (v);
}

uint16_t
bus_space_read_io_2(u_long port)
{
	uint16_t v;

	ia64_mf();
	v = ia64_ld2(__PIO_ADDR(port));
	ia64_mf_a();
	ia64_mf();
	return (v);
}

uint32_t
bus_space_read_io_4(u_long port)
{
	uint32_t v;

	ia64_mf();
	v = ia64_ld4(__PIO_ADDR(port));
	ia64_mf_a();
	ia64_mf();
	return (v);
}

#if 0
uint64_t
bus_space_read_io_8(u_long port)
{
}
#endif

void
bus_space_write_io_1(u_long port, uint8_t val)
{

	ia64_mf();
	ia64_st1(__PIO_ADDR(port), val);
	ia64_mf_a();
	ia64_mf();
}

void
bus_space_write_io_2(u_long port, uint16_t val)
{

	ia64_mf();
	ia64_st2(__PIO_ADDR(port), val);
	ia64_mf_a();
	ia64_mf();
}

void
bus_space_write_io_4(u_long port, uint32_t val)
{

	ia64_mf();
	ia64_st4(__PIO_ADDR(port), val);
	ia64_mf_a();
	ia64_mf();
}

#if 0
void
bus_space_write_io_8(u_long port, uint64_t val)
{
}
#endif

void
bus_space_read_multi_io_1(u_long port, uint8_t *ptr, size_t count)
{

	while (count-- > 0)
		*ptr++ = bus_space_read_io_1(port);
}

void
bus_space_read_multi_io_2(u_long port, uint16_t *ptr, size_t count)
{

	while (count-- > 0)
		*ptr++ = bus_space_read_io_2(port);
}

void
bus_space_read_multi_io_4(u_long port, uint32_t *ptr, size_t count)
{

	while (count-- > 0)
		*ptr++ = bus_space_read_io_4(port);
}

#if 0
void
bus_space_read_multi_io_8(u_long port, uint64_t *ptr, size_t count)
{
}
#endif

void
bus_space_write_multi_io_1(u_long port, const uint8_t *ptr, size_t count)
{

	while (count-- > 0)
		bus_space_write_io_1(port, *ptr++);
}

void
bus_space_write_multi_io_2(u_long port, const uint16_t *ptr, size_t count)
{

	while (count-- > 0)
		bus_space_write_io_2(port, *ptr++);
}

void
bus_space_write_multi_io_4(u_long port, const uint32_t *ptr, size_t count)
{

	while (count-- > 0)
		bus_space_write_io_4(port, *ptr++);
}

#if 0
void
bus_space_write_multi_io_8(u_long port, const uint64_t *ptr, size_t count)
{
}
#endif

void
bus_space_read_region_io_1(u_long port, uint8_t *ptr, size_t count)
{

	while (count-- > 0) {
		*ptr++ = bus_space_read_io_1(port);
		port += 1;
	}
}

void
bus_space_read_region_io_2(u_long port, uint16_t *ptr, size_t count) 
{

	while (count-- > 0) {
		*ptr++ = bus_space_read_io_2(port);
		port += 2;
	}
}

void
bus_space_read_region_io_4(u_long port, uint32_t *ptr, size_t count) 
{

	while (count-- > 0) {
		*ptr++ = bus_space_read_io_4(port);
		port += 4;
	}
}

#if 0
void bus_space_read_region_io_8(u_long, uint64_t *, size_t);
#endif

void
bus_space_write_region_io_1(u_long port, const uint8_t *ptr, size_t count)
{

	while (count-- > 0) {
		bus_space_write_io_1(port, *ptr++);
		port += 1;
	}
}

void
bus_space_write_region_io_2(u_long port, const uint16_t *ptr, size_t count)
{

	while (count-- > 0) {
		bus_space_write_io_2(port, *ptr++);
		port += 2;
	}
}

void
bus_space_write_region_io_4(u_long port, const uint32_t *ptr, size_t count)
{

	while (count-- > 0) {
		bus_space_write_io_4(port, *ptr++);
		port += 4;
	}
}

#if 0
void
bus_space_write_region_io_8(u_long port, const uint64_t *ptr, size_t count)
{
}
#endif

void
bus_space_set_region_io_1(u_long port, uint8_t val, size_t count)
{

	while (count-- > 0) {
		bus_space_write_io_1(port, val);
		port += 1;
	}
}

void
bus_space_set_region_io_2(u_long port, uint16_t val, size_t count)
{

	while (count-- > 0) {
		bus_space_write_io_2(port, val);
		port += 2;
	}
}

void
bus_space_set_region_io_4(u_long port, uint32_t val, size_t count)
{

	while (count-- > 0) {
		bus_space_write_io_4(port, val);
		port += 4;
	}
}

#if 0
void
bus_space_set_region_io_8(u_long port, uint64_t val, size_t count)
{
}
#endif

void 
bus_space_copy_region_io_1(u_long src, u_long dst, size_t count) 
{
	long delta;
	uint8_t val;

	if (src < dst) {
		src += count - 1;
		dst += count - 1;
		delta = -1;
	} else
		delta = 1;

	while (count-- > 0) {
		val = bus_space_read_io_1(src);
		bus_space_write_io_1(dst, val);
		src += delta;
		dst += delta;
	}
}

void 
bus_space_copy_region_io_2(u_long src, u_long dst, size_t count) 
{
	long delta;
	uint16_t val;

	if (src < dst) {
		src += 2 * (count - 1);
		dst += 2 * (count - 1);
		delta = -2;
	} else
		delta = 2;

	while (count-- > 0) {
		val = bus_space_read_io_2(src);
		bus_space_write_io_2(dst, val);
		src += delta;
		dst += delta;
	}
}

void 
bus_space_copy_region_io_4(u_long src, u_long dst, size_t count) 
{
	long delta;
	uint32_t val;

	if (src < dst) {
		src += 4 * (count - 1);
		dst += 4 * (count - 1);
		delta = -4;
	} else
		delta = 4;

	while (count-- > 0) {
		val = bus_space_read_io_4(src);
		bus_space_write_io_4(dst, val);
		src += delta;
		dst += delta;
	}
}

#if 0
void
bus_space_copy_region_io_8(u_long src, u_long dst, size_t count)
{
}
#endif
