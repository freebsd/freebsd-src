/*	$NetBSD: bus.h,v 1.12 1997/10/01 08:25:15 fvdl Exp $	*/

/*-
 * Copyright (c) 1996, 1997 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1996 Christopher G. Demetriou.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Christopher G. Demetriou
 *	for the NetBSD Project.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
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
/* $FreeBSD$ */

#ifndef _ALPHA_BUS_H_
#define _ALPHA_BUS_H_

/*
 * Bus address and size types
 */
typedef u_int64_t		bus_addr_t;
typedef u_int64_t		bus_size_t;
typedef struct alpha_busspace	*bus_space_tag_t;
typedef u_int32_t		bus_space_handle_t;

#define BUS_SPACE_MAXSIZE_24BIT	0xFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXSIZE	0xFFFFFFFFFFFFFFFF
#define BUS_SPACE_MAXADDR_24BIT	0xFFFFFF
#define BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
/* The largest address space known so far is 40 bits */
#define BUS_SPACE_MAXADDR	0xFFFFFFFFFUL

#define BUS_SPACE_UNRESTRICTED	(~0)

/*
 * Map a region of device bus space into CPU virtual address space.
 */

static __inline int bus_space_map(bus_space_tag_t t, bus_addr_t addr,
				  bus_size_t size, int flags,
				  bus_space_handle_t *bshp);

static __inline int
bus_space_map(bus_space_tag_t t __unused, bus_addr_t addr,
	      bus_size_t size __unused, int flags __unused,
	      bus_space_handle_t *bshp)
{

	*bshp = addr;
	return (0);
}

/*
 * Unmap a region of device bus space.
 */

static __inline void bus_space_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
				     bus_size_t size);

static __inline void
bus_space_unmap(bus_space_tag_t t __unused, bus_space_handle_t bsh __unused,
		bus_size_t size __unused)
{
}


/*
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

static __inline int bus_space_subregion(bus_space_tag_t t,
					bus_space_handle_t bsh,
					bus_size_t offset, bus_size_t size,
					bus_space_handle_t *nbshp);

static __inline int
bus_space_subregion(bus_space_tag_t t __unused, bus_space_handle_t bsh,
		    bus_size_t offset, bus_size_t size __unused,
		    bus_space_handle_t *nbshp)
{

	*nbshp = bsh + offset;
	return (0);
}


struct alpha_busspace;

struct alpha_busspace_ops {
    u_int8_t	(*abo_read_1)(struct alpha_busspace *space, size_t offset);
    u_int16_t	(*abo_read_2)(struct alpha_busspace *space, size_t offset);
    u_int32_t	(*abo_read_4)(struct alpha_busspace *space, size_t offset);

    void	(*abo_read_multi_1)(struct alpha_busspace *space,
				    size_t offset,
				    u_int8_t *addr, size_t count);
    void	(*abo_read_multi_2)(struct alpha_busspace *space,
				    size_t offset,
				    u_int16_t *addr, size_t count);
    void	(*abo_read_multi_4)(struct alpha_busspace *space,
				    size_t offset,
				    u_int32_t *addr, size_t count);

    void	(*abo_read_region_1)(struct alpha_busspace *space,
				     size_t offset,
				     u_int8_t *addr, size_t count);
    void	(*abo_read_region_2)(struct alpha_busspace *space,
				     size_t offset,
				     u_int16_t *addr, size_t count);
    void	(*abo_read_region_4)(struct alpha_busspace *space,
				     size_t offset,
				     u_int32_t *addr, size_t count);

    void	(*abo_write_1)(struct alpha_busspace *space, size_t offset,
			       u_int8_t value);
    void	(*abo_write_2)(struct alpha_busspace *space, size_t offset,
			       u_int16_t value);
    void	(*abo_write_4)(struct alpha_busspace *space, size_t offset,
			       u_int32_t value);

    void	(*abo_write_multi_1)(struct alpha_busspace *space,
				     size_t offset,
				     const u_int8_t *addr, size_t count);
    void	(*abo_write_multi_2)(struct alpha_busspace *space,
				     size_t offset,
				     const u_int16_t *addr, size_t count);
    void	(*abo_write_multi_4)(struct alpha_busspace *space,
				     size_t offset,
				     const u_int32_t *addr, size_t count);

    void	(*abo_write_region_1)(struct alpha_busspace *space,
				      size_t offset,
				      const u_int8_t *addr, size_t count);
    void	(*abo_write_region_2)(struct alpha_busspace *space,
				      size_t offset,
				      const u_int16_t *addr, size_t count);
    void	(*abo_write_region_4)(struct alpha_busspace *space,
				      size_t offset,
				      const u_int32_t *addr, size_t count);

    void	(*abo_set_multi_1)(struct alpha_busspace *space, size_t offset,
				   u_int8_t value, size_t count);
    void	(*abo_set_multi_2)(struct alpha_busspace *space, size_t offset,
				   u_int16_t value, size_t count);
    void	(*abo_set_multi_4)(struct alpha_busspace *space, size_t offset,
				   u_int32_t value, size_t count);

    void	(*abo_set_region_1)(struct alpha_busspace *space,
				    size_t offset,
				    u_int8_t value, size_t count);
    void	(*abo_set_region_2)(struct alpha_busspace *space,
				    size_t offset,
				    u_int16_t value, size_t count);
    void	(*abo_set_region_4)(struct alpha_busspace *space,
				    size_t offset,
				    u_int32_t value, size_t count);

    void	(*abo_copy_region_1)(struct alpha_busspace *space,
				     size_t offset1, size_t offset2,
				     size_t count);
    void	(*abo_copy_region_2)(struct alpha_busspace *space,
				     size_t offset1, size_t offset2,
				     size_t count);
    void	(*abo_copy_region_4)(struct alpha_busspace *space,
				     size_t offset1, size_t offset2,
				     size_t count);

    void	(*abo_barrier)(struct alpha_busspace *space, size_t offset,
			       size_t len, int flags);
};

struct alpha_busspace {
    struct alpha_busspace_ops *ab_ops;
};

/* Back-compat functions for old ISA drivers */

extern struct alpha_busspace *busspace_isa_io;
extern struct alpha_busspace *busspace_isa_mem;

#define inb(o)		bus_space_read_1(busspace_isa_io, o, 0)
#define inw(o)		bus_space_read_2(busspace_isa_io, o, 0)
#define inl(o)		bus_space_read_4(busspace_isa_io, o, 0)
#define outb(o, v)	bus_space_write_1(busspace_isa_io, o, 0, v)
#define outw(o, v)	bus_space_write_2(busspace_isa_io, o, 0, v)
#define outl(o, v)	bus_space_write_4(busspace_isa_io, o, 0, v)

#define readb(o)	bus_space_read_1(busspace_isa_mem, o, 0)
#define readw(o)	bus_space_read_2(busspace_isa_mem, o, 0)
#define readl(o)	bus_space_read_4(busspace_isa_mem, o, 0)
#define writeb(o, v)	bus_space_write_1(busspace_isa_mem, o, 0, v)
#define writew(o, v)	bus_space_write_2(busspace_isa_mem, o, 0, v)
#define writel(o, v)	bus_space_write_4(busspace_isa_mem, o, 0, v)

#define insb(o, a, c)	bus_space_read_multi_1(busspace_isa_io, o, 0, \
					       (void*)(a), c)
#define insw(o, a, c)	bus_space_read_multi_2(busspace_isa_io, o, 0, \
					       (void*)(a), c)
#define insl(o, a, c)	bus_space_read_multi_4(busspace_isa_io, o, 0, \
					       (void*)(a), c)

#define outsb(o, a, c)	bus_space_write_multi_1(busspace_isa_io, o, 0, \
						(void*)(a), c)
#define outsw(o, a, c)	bus_space_write_multi_2(busspace_isa_io, o, 0, \
						(void*)(a), c)
#define outsl(o, a, c)	bus_space_write_multi_4(busspace_isa_io, o, 0, \
						(void*)(a), c)

#define memcpy_fromio(d, s, c) \
	bus_space_read_region_1(busspace_isa_mem, (uintptr_t)(s), 0, d, c)
#define memcpy_toio(d, s, c) \
	bus_space_write_region_1(busspace_isa_mem, (uintptr_t)(d), 0, s, c)
#define memcpy_io(d, s, c) \
	bus_space_copy_region_1(busspace_isa_mem, (uintptr_t)(s), 0, d, 0, c)
#define memset_io(d, v, c) \
	bus_space_set_region_1(busspace_isa_mem, (uintptr_t)(d), 0, v, c)
#define memsetw_io(d, v, c) \
	bus_space_set_region_2(busspace_isa_mem, (uintptr_t)(d), 0, v, c)

static __inline void
memsetw(void *d, int val, size_t size)
{
    u_int16_t *sp = d;

    while (size--)
	*sp++ = val;
}

void busspace_generic_read_multi_1(struct alpha_busspace *space,
				   size_t offset,
				   u_int8_t *addr, size_t count);
void busspace_generic_read_multi_2(struct alpha_busspace *space,
				   size_t offset,
				   u_int16_t *addr, size_t count);
void busspace_generic_read_multi_4(struct alpha_busspace *space,
				   size_t offset,
				   u_int32_t *addr, size_t count);
void busspace_generic_read_region_1(struct alpha_busspace *space,
				    size_t offset,
				    u_int8_t *addr, size_t count);
void busspace_generic_read_region_2(struct alpha_busspace *space,
				    size_t offset,
				    u_int16_t *addr, size_t count);
void busspace_generic_read_region_4(struct alpha_busspace *space,
				    size_t offset,
				    u_int32_t *addr, size_t count);
void busspace_generic_write_multi_1(struct alpha_busspace *space,
				    size_t offset,
				    const u_int8_t *addr, size_t count);
void busspace_generic_write_multi_2(struct alpha_busspace *space,
				    size_t offset,
				    const u_int16_t *addr, size_t count);
void busspace_generic_write_multi_4(struct alpha_busspace *space,
				    size_t offset,
				    const u_int32_t *addr, size_t count);
void busspace_generic_write_region_1(struct alpha_busspace *space,
				     size_t offset,
				     const u_int8_t *addr, size_t count);
void busspace_generic_write_region_2(struct alpha_busspace *space,
				     size_t offset,
				     const u_int16_t *addr, size_t count);
void busspace_generic_write_region_4(struct alpha_busspace *space,
				     size_t offset,
				     const u_int32_t *addr, size_t count);
void busspace_generic_set_multi_1(struct alpha_busspace *space,
				  size_t offset,
				  u_int8_t value, size_t count);
void busspace_generic_set_multi_2(struct alpha_busspace *space,
				  size_t offset,
				  u_int16_t value, size_t count);
void busspace_generic_set_multi_4(struct alpha_busspace *space,
				  size_t offset,
				  u_int32_t value, size_t count);
void busspace_generic_set_region_1(struct alpha_busspace *space,
				   size_t offset,
				   u_int8_t value, size_t count);
void busspace_generic_set_region_2(struct alpha_busspace *space,
				   size_t offset,
				   u_int16_t value, size_t count);
void busspace_generic_set_region_4(struct alpha_busspace *space,
				   size_t offset,
				   u_int32_t value, size_t count);
void busspace_generic_copy_region_1(struct alpha_busspace *space,
				    size_t offset1,
				    size_t offset2,
				    size_t count);
void busspace_generic_copy_region_2(struct alpha_busspace *space,
				    size_t offset1,
				    size_t offset2,
				    size_t count);
void busspace_generic_copy_region_4(struct alpha_busspace *space,
				    size_t offset1,
				    size_t offset2,
				    size_t count);
void busspace_generic_barrier(struct alpha_busspace *space,
			      size_t offset, size_t len,
			      int flags);

#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

#define bus_space_read_1(t, h, o) \
	(t)->ab_ops->abo_read_1(t, (h)+(o))
#define bus_space_read_2(t, h, o) \
	(t)->ab_ops->abo_read_2(t, (h)+(o))
#define bus_space_read_4(t, h, o) \
	(t)->ab_ops->abo_read_4(t, (h)+(o))
#define bus_space_read_multi_1(t, h, o, a, c) \
	(t)->ab_ops->abo_read_multi_1(t, (h)+(o), a, c)
#define bus_space_read_multi_2(t, h, o, a, c) \
	(t)->ab_ops->abo_read_multi_2(t, (h)+(o), a, c)
#define bus_space_read_multi_4(t, h, o, a, c) \
	(t)->ab_ops->abo_read_multi_4(t, (h)+(o), a, c)
#define bus_space_read_region_1(t, h, o, a, c) \
	(t)->ab_ops->abo_read_region_1(t, (h)+(o), a, c)
#define bus_space_read_region_2(t, h, o, a, c) \
	(t)->ab_ops->abo_read_region_2(t, (h)+(o), a, c)
#define bus_space_read_region_4(t, h, o, a, c) \
	(t)->ab_ops->abo_read_region_4(t, (h)+(o), a, c)

#define bus_space_write_1(t, h, o, v) \
	(t)->ab_ops->abo_write_1(t, (h)+(o), v)
#define bus_space_write_2(t, h, o, v) \
	(t)->ab_ops->abo_write_2(t, (h)+(o), v)
#define bus_space_write_4(t, h, o, v) \
	(t)->ab_ops->abo_write_4(t, (h)+(o), v)
#define bus_space_write_multi_1(t, h, o, a, c) \
	(t)->ab_ops->abo_write_multi_1(t, (h)+(o), a, c)
#define bus_space_write_multi_2(t, h, o, a, c) \
	(t)->ab_ops->abo_write_multi_2(t, (h)+(o), a, c)
#define bus_space_write_multi_4(t, h, o, a, c) \
	(t)->ab_ops->abo_write_multi_4(t, (h)+(o), a, c)
#define bus_space_write_region_1(t, h, o, a, c) \
	(t)->ab_ops->abo_write_region_1(t, (h)+(o), a, c)
#define bus_space_write_region_2(t, h, o, a, c) \
	(t)->ab_ops->abo_write_region_2(t, (h)+(o), a, c)
#define bus_space_write_region_4(t, h, o, a, c) \
	(t)->ab_ops->abo_write_region_4(t, (h)+(o), a, c)
#define bus_space_set_multi_1(t, h, o, v, c) \
	(t)->ab_ops->abo_set_multi_1(t, (h)+(o), v, c)
#define bus_space_set_multi_2(t, h, o, v, c) \
	(t)->ab_ops->abo_set_multi_2(t, (h)+(o), v, c)
#define bus_space_set_multi_4(t, h, o, v, c) \
	(t)->ab_ops->abo_set_multi_4(t, (h)+(o), v, c)
#define bus_space_set_region_1(t, h, o, v, c) \
	(t)->ab_ops->abo_set_region_1(t, (h)+(o), v, c)
#define bus_space_set_region_2(t, h, o, v, c) \
	(t)->ab_ops->abo_set_region_2(t, (h)+(o), v, c)
#define bus_space_set_region_4(t, h, o, v, c) \
	(t)->ab_ops->abo_set_region_4(t, (h)+(o), v, c)

#define bus_space_copy_region_1(t, h1, o1, h2, o2, c) \
	(t)->ab_ops->abo_copy_region_1(t, (h1)+(o1), (h2)+(o2), c)
#define bus_space_copy_region_2(t, h1, o1, h2, o2, c) \
	(t)->ab_ops->abo_copy_region_2(t, (h1)+(o1), (h2)+(o2), c)
#define bus_space_copy_region_4(t, h1, o1, h2, o2, c) \
	(t)->ab_ops->abo_copy_region_4(t, (h1)+(o1), (h2)+(o2), c)

#define bus_space_barrier(t, h, o, l, f) \
	(t)->ab_ops->abo_barrier(t, (h)+(o), l, f)

/*
 * Stream accesses are the same as normal accesses on alpha; there are no
 * supported bus systems with an endianess different from the host one.
 */
#define	bus_space_read_stream_1(t, h, o)	bus_space_read_1((t), (h), (o))
#define	bus_space_read_stream_2(t, h, o)	bus_space_read_2((t), (h), (o))
#define	bus_space_read_stream_4(t, h, o)	bus_space_read_4((t), (h), (o))

#define	bus_space_read_multi_stream_1(t, h, o, a, c) \
	bus_space_read_multi_1((t), (h), (o), (a), (c))
#define	bus_space_read_multi_stream_2(t, h, o, a, c) \
	bus_space_read_multi_2((t), (h), (o), (a), (c))
#define	bus_space_read_multi_stream_4(t, h, o, a, c) \
	bus_space_read_multi_4((t), (h), (o), (a), (c))

#define	bus_space_write_stream_1(t, h, o, v) \
	bus_space_write_1((t), (h), (o), (v))
#define	bus_space_write_stream_2(t, h, o, v) \
	bus_space_write_2((t), (h), (o), (v))
#define	bus_space_write_stream_4(t, h, o, v) \
	bus_space_write_4((t), (h), (o), (v))

#define	bus_space_write_multi_stream_1(t, h, o, a, c) \
	bus_space_write_multi_1((t), (h), (o), (a), (c))
#define	bus_space_write_multi_stream_2(t, h, o, a, c) \
	bus_space_write_multi_2((t), (h), (o), (a), (c))
#define	bus_space_write_multi_stream_4(t, h, o, a, c) \
	bus_space_write_multi_4((t), (h), (o), (a), (c))

#define	bus_space_set_multi_stream_1(t, h, o, v, c) \
	bus_space_set_multi_1((t), (h), (o), (v), (c))
#define	bus_space_set_multi_stream_2(t, h, o, v, c) \
	bus_space_set_multi_2((t), (h), (o), (v), (c))
#define	bus_space_set_multi_stream_4(t, h, o, v, c) \
	bus_space_set_multi_4((t), (h), (o), (v), (c))

#define	bus_space_read_region_stream_1(t, h, o, a, c) \
	bus_space_read_region_1((t), (h), (o), (a), (c))
#define	bus_space_read_region_stream_2(t, h, o, a, c) \
	bus_space_read_region_2((t), (h), (o), (a), (c))
#define	bus_space_read_region_stream_4(t, h, o, a, c) \
	bus_space_read_region_4((t), (h), (o), (a), (c))

#define	bus_space_write_region_stream_1(t, h, o, a, c) \
	bus_space_write_region_1((t), (h), (o), (a), (c))
#define	bus_space_write_region_stream_2(t, h, o, a, c) \
	bus_space_write_region_2((t), (h), (o), (a), (c))
#define	bus_space_write_region_stream_4(t, h, o, a, c) \
	bus_space_write_region_4((t), (h), (o), (a), (c))

#define	bus_space_set_region_stream_1(t, h, o, v, c) \
	bus_space_set_region_1((t), (h), (o), (v), (c))
#define	bus_space_set_region_stream_2(t, h, o, v, c) \
	bus_space_set_region_2((t), (h), (o), (v), (c))
#define	bus_space_set_region_stream_4(t, h, o, v, c) \
	bus_space_set_region_4((t), (h), (o), (v), (c))

#define	bus_space_copy_region_stream_1(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_1((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_region_stream_2(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_2((t), (h1), (o1), (h2), (o2), (c))
#define	bus_space_copy_region_stream_4(t, h1, o1, h2, o2, c) \
	bus_space_copy_region_4((t), (h1), (o1), (h2), (o2), (c))

#include <machine/bus_dma.h>

#endif /* _ALPHA_BUS_H_ */
