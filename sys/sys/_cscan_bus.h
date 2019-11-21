/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2019 Andrew Turner
 *
 * This software was developed by SRI International and the University of
 * Cambridge Computer Laboratory (Department of Computer Science and
 * Technology) under DARPA contract HR0011-18-C-0016 ("ECATS"), as part of the
 * DARPA SSITH research programme.
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
 * $FreeBSD$
 */

#ifndef _SYS__CSAN_BUS_H_
#define	_SYS__CSAN_BUS_H_

#define	KCSAN_BS_MULTI(rw, width, type)					\
	void kcsan_bus_space_##rw##_multi_##width(bus_space_tag_t, 	\
	    bus_space_handle_t, bus_size_t, type *, bus_size_t);	\
	void kcsan_bus_space_##rw##_multi_stream_##width(bus_space_tag_t, \
	    bus_space_handle_t, bus_size_t, type *, bus_size_t);	\
	void kcsan_bus_space_##rw##_region_##width(bus_space_tag_t,	\
	    bus_space_handle_t, bus_size_t, type *, bus_size_t);	\
	void kcsan_bus_space_##rw##_region_stream_##width(bus_space_tag_t, \
	    bus_space_handle_t, bus_size_t, type *, bus_size_t)

#define	KCSAN_BS_READ(width, type)					\
	type kcsan_bus_space_read_##width(bus_space_tag_t, 		\
	    bus_space_handle_t, bus_size_t);				\
	type kcsan_bus_space_read_stream_##width(bus_space_tag_t,	\
	    bus_space_handle_t, bus_size_t);				\
	KCSAN_BS_MULTI(read, width, type)

#define	KCSAN_BS_WRITE(width, type)					\
	void kcsan_bus_space_write_##width(bus_space_tag_t, 		\
	    bus_space_handle_t, bus_size_t, type);			\
	void kcsan_bus_space_write_stream_##width(bus_space_tag_t,	\
	    bus_space_handle_t, bus_size_t, type);			\
	KCSAN_BS_MULTI(write, width, const type)

#define	KCSAN_BS_SET(width, type)					\
	void kcsan_bus_space_set_multi_##width(bus_space_tag_t, 	\
	    bus_space_handle_t, bus_size_t, type, bus_size_t);		\
	void kcsan_bus_space_set_multi_stream_##width(bus_space_tag_t, \
	    bus_space_handle_t, bus_size_t, type, bus_size_t);		\
	void kcsan_bus_space_set_region_##width(bus_space_tag_t,	\
	    bus_space_handle_t, bus_size_t, type, bus_size_t);		\
	void kcsan_bus_space_set_region_stream_##width(bus_space_tag_t, \
	    bus_space_handle_t, bus_size_t, type, bus_size_t)

#define	KCSAN_BS_COPY(width, type)					\
	void kcsan_bus_space_copy_region_##width(bus_space_tag_t,	\
	    bus_space_handle_t,	bus_size_t, bus_space_handle_t,		\
	    bus_size_t, bus_size_t);					\
	void kcsan_bus_space_copy_region_stream_##width(bus_space_tag_t, \
	    bus_space_handle_t,	bus_size_t, bus_space_handle_t,		\
	    bus_size_t, bus_size_t);

#define	KCSAN_BS(width, type)						\
	KCSAN_BS_READ(width, type);					\
	KCSAN_BS_WRITE(width, type);					\
	KCSAN_BS_SET(width, type);					\
	KCSAN_BS_COPY(width, type)

KCSAN_BS(1, uint8_t);
KCSAN_BS(2, uint16_t);
KCSAN_BS(4, uint32_t);
KCSAN_BS(8, uint64_t);

int kcsan_bus_space_map(bus_space_tag_t, bus_addr_t, bus_size_t, int,
    bus_space_handle_t *);
void kcsan_bus_space_unmap(bus_space_tag_t, bus_space_handle_t, bus_size_t);
int kcsan_bus_space_subregion(bus_space_tag_t, bus_space_handle_t, bus_size_t,
    bus_size_t, bus_space_handle_t *);
int kcsan_bus_space_alloc(bus_space_tag_t, bus_addr_t, bus_addr_t,
    bus_size_t, bus_size_t, bus_size_t, int, bus_addr_t *,
    bus_space_handle_t *);
void kcsan_bus_space_free(bus_space_tag_t, bus_space_handle_t, bus_size_t);
void kcsan_bus_space_barrier(bus_space_tag_t, bus_space_handle_t, bus_size_t,
    bus_size_t, int);

#ifndef KCSAN_RUNTIME

#define	bus_space_map			kcsan_bus_space_map
#define	bus_space_unmap			kcsan_bus_space_unmap
#define	bus_space_subregion		kcsan_bus_space_subregion
#define	bus_space_alloc			kcsan_bus_space_alloc
#define	bus_space_free			kcsan_bus_space_free
#define	bus_space_barrier		kcsan_bus_space_barrier

#define	bus_space_read_1		kcsan_bus_space_read_1
#define	bus_space_read_stream_1		kcsan_bus_space_read_stream_1
#define	bus_space_read_multi_1		kcsan_bus_space_read_multi_1
#define	bus_space_read_multi_stream_1	kcsan_bus_space_read_multi_stream_1
#define	bus_space_read_region_1		kcsan_bus_space_read_region_1
#define	bus_space_read_region_stream_1	kcsan_bus_space_read_region_stream_1
#define	bus_space_write_1		kcsan_bus_space_write_1
#define	bus_space_write_stream_1	kcsan_bus_space_write_stream_1
#define	bus_space_write_multi_1		kcsan_bus_space_write_multi_1
#define	bus_space_write_multi_stream_1	kcsan_bus_space_write_multi_stream_1
#define	bus_space_write_region_1	kcsan_bus_space_write_region_1
#define	bus_space_write_region_stream_1	kcsan_bus_space_write_region_stream_1
#define	bus_space_set_multi_1		kcsan_bus_space_set_multi_1
#define	bus_space_set_multi_stream_1	kcsan_bus_space_set_multi_stream_1
#define	bus_space_set_region_1		kcsan_bus_space_set_region_1
#define	bus_space_set_region_stream_1	kcsan_bus_space_set_region_stream_1
#define	bus_space_copy_multi_1		kcsan_bus_space_copy_multi_1
#define	bus_space_copy_multi_stream_1	kcsan_bus_space_copy_multi_stream_1

#define	bus_space_read_2		kcsan_bus_space_read_2
#define	bus_space_read_stream_2		kcsan_bus_space_read_stream_2
#define	bus_space_read_multi_2		kcsan_bus_space_read_multi_2
#define	bus_space_read_multi_stream_2	kcsan_bus_space_read_multi_stream_2
#define	bus_space_read_region_2		kcsan_bus_space_read_region_2
#define	bus_space_read_region_stream_2	kcsan_bus_space_read_region_stream_2
#define	bus_space_write_2		kcsan_bus_space_write_2
#define	bus_space_write_stream_2	kcsan_bus_space_write_stream_2
#define	bus_space_write_multi_2		kcsan_bus_space_write_multi_2
#define	bus_space_write_multi_stream_2	kcsan_bus_space_write_multi_stream_2
#define	bus_space_write_region_2	kcsan_bus_space_write_region_2
#define	bus_space_write_region_stream_2	kcsan_bus_space_write_region_stream_2
#define	bus_space_set_multi_2		kcsan_bus_space_set_multi_2
#define	bus_space_set_multi_stream_2	kcsan_bus_space_set_multi_stream_2
#define	bus_space_set_region_2		kcsan_bus_space_set_region_2
#define	bus_space_set_region_stream_2	kcsan_bus_space_set_region_stream_2
#define	bus_space_copy_multi_2		kcsan_bus_space_copy_multi_2
#define	bus_space_copy_multi_stream_2	kcsan_bus_space_copy_multi_stream_2

#define	bus_space_read_4		kcsan_bus_space_read_4
#define	bus_space_read_stream_4		kcsan_bus_space_read_stream_4
#define	bus_space_read_multi_4		kcsan_bus_space_read_multi_4
#define	bus_space_read_multi_stream_4	kcsan_bus_space_read_multi_stream_4
#define	bus_space_read_region_4		kcsan_bus_space_read_region_4
#define	bus_space_read_region_stream_4	kcsan_bus_space_read_region_stream_4
#define	bus_space_write_4		kcsan_bus_space_write_4
#define	bus_space_write_stream_4	kcsan_bus_space_write_stream_4
#define	bus_space_write_multi_4		kcsan_bus_space_write_multi_4
#define	bus_space_write_multi_stream_4	kcsan_bus_space_write_multi_stream_4
#define	bus_space_write_region_4	kcsan_bus_space_write_region_4
#define	bus_space_write_region_stream_4	kcsan_bus_space_write_region_stream_4
#define	bus_space_set_multi_4		kcsan_bus_space_set_multi_4
#define	bus_space_set_multi_stream_4	kcsan_bus_space_set_multi_stream_4
#define	bus_space_set_region_4		kcsan_bus_space_set_region_4
#define	bus_space_set_region_stream_4	kcsan_bus_space_set_region_stream_4
#define	bus_space_copy_multi_4		kcsan_bus_space_copy_multi_4
#define	bus_space_copy_multi_stream_4	kcsan_bus_space_copy_multi_stream_4

#define	bus_space_read_8		kcsan_bus_space_read_8
#define	bus_space_read_stream_8		kcsan_bus_space_read_stream_8
#define	bus_space_read_multi_8		kcsan_bus_space_read_multi_8
#define	bus_space_read_multi_stream_8	kcsan_bus_space_read_multi_stream_8
#define	bus_space_read_region_8		kcsan_bus_space_read_region_8
#define	bus_space_read_region_stream_8	kcsan_bus_space_read_region_stream_8
#define	bus_space_write_8		kcsan_bus_space_write_8
#define	bus_space_write_stream_8	kcsan_bus_space_write_stream_8
#define	bus_space_write_multi_8		kcsan_bus_space_write_multi_8
#define	bus_space_write_multi_stream_8	kcsan_bus_space_write_multi_stream_8
#define	bus_space_write_region_8	kcsan_bus_space_write_region_8
#define	bus_space_write_region_stream_8	kcsan_bus_space_write_region_stream_8
#define	bus_space_set_multi_8		kcsan_bus_space_set_multi_8
#define	bus_space_set_multi_stream_8	kcsan_bus_space_set_multi_stream_8
#define	bus_space_set_region_8		kcsan_bus_space_set_region_8
#define	bus_space_set_region_stream_8	kcsan_bus_space_set_region_stream_8
#define	bus_space_copy_multi_8		kcsan_bus_space_copy_multi_8
#define	bus_space_copy_multi_stream_8	kcsan_bus_space_copy_multi_stream_8

#endif /* !KCSAN_RUNTIME */

#endif /* !_SYS__CSAN_BUS_H_ */
