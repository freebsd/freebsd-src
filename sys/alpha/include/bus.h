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

/*
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

#define BUS_SPACE_UNRESTRICTED	(~0UL)

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

/*
 * Flags used in various bus DMA methods.
 */
#define	BUS_DMA_WAITOK		0x00	/* safe to sleep (pseudo-flag) */
#define	BUS_DMA_NOWAIT		0x01	/* not safe to sleep */
#define	BUS_DMA_ALLOCNOW	0x02	/* perform resource allocation now */
#define	BUS_DMA_COHERENT	0x04	/* hint: map memory in a coherent way */
#define	BUS_DMA_ZERO		0x08	/* allocate zero'ed memory */
#define	BUS_DMA_ISA		0x10	/* map memory for ISA dma */
#define	BUS_DMA_BUS2		0x20	/* placeholders for bus functions... */
#define	BUS_DMA_BUS3		0x40
#define	BUS_DMA_BUS4		0x80

/* Forwards needed by prototypes below. */
struct mbuf;
struct uio;

/*
 *	Operations performed by bus_dmamap_sync().
 */
typedef int bus_dmasync_op_t;
#define	BUS_DMASYNC_PREREAD	1
#define	BUS_DMASYNC_POSTREAD	2
#define	BUS_DMASYNC_PREWRITE	4
#define	BUS_DMASYNC_POSTWRITE	8

/*
 *	bus_dma_tag_t
 *
 *	A machine-dependent opaque type describing the characteristics
 *	of how to perform DMA mappings.  This structure encapsultes
 *	information concerning address and alignment restrictions, number
 *	of S/G	segments, amount of data per S/G segment, etc.
 */
typedef struct bus_dma_tag	*bus_dma_tag_t;

/*
 *	bus_dmamap_t
 *
 *	DMA mapping instance information.
 */
typedef struct bus_dmamap	*bus_dmamap_t;

/*
 *	bus_dma_segment_t
 *
 *	Describes a single contiguous DMA transaction.  Values
 *	are suitable for programming into DMA registers.
 */
typedef struct bus_dma_segment {
	bus_addr_t	ds_addr;	/* DMA address */
	bus_size_t	ds_len;		/* length of transfer */
} bus_dma_segment_t;

/*
 * A function that returns 1 if the address cannot be accessed by
 * a device and 0 if it can be.
 */
typedef int bus_dma_filter_t(void *, bus_addr_t);

/*
 * A function that performs driver-specific syncronization on behalf of
 * busdma.
 */
typedef enum {
	BUS_DMA_LOCK	= 0x01,
	BUS_DMA_UNLOCK	= 0x02,
} bus_dma_lock_op_t;

typedef void bus_dma_lock_t(void *, bus_dma_lock_op_t);

/*
 * Allocate a device specific dma_tag encapsulating the constraints of
 * the parent tag in addition to other restrictions specified:
 *
 *	alignment:	alignment for segments.
 *	boundary:	Boundary that segments cannot cross.
 *	lowaddr:	Low restricted address that cannot appear in a mapping.
 *	highaddr:	High restricted address that cannot appear in a mapping.
 *	filtfunc:	An optional function to further test if an address
 *			within the range of lowaddr and highaddr cannot appear
 *			in a mapping.
 *	filtfuncarg:	An argument that will be passed to filtfunc in addition
 *			to the address to test.
 *	maxsize:	Maximum mapping size supported by this tag.
 *	nsegments:	Number of discontinuities allowed in maps.
 *	maxsegsz:	Maximum size of a segment in the map.
 *	flags:		Bus DMA flags.
 *	dmat:		A pointer to set to a valid dma tag should the return
 *			value of this function indicate success.
 */
/* XXX Should probably allow specification of alignment */
int bus_dma_tag_create(bus_dma_tag_t parent, bus_size_t alignemnt,
		       bus_size_t boundary, bus_addr_t lowaddr,
		       bus_addr_t highaddr, bus_dma_filter_t *filtfunc,
		       void *filtfuncarg, bus_size_t maxsize, int nsegments,
		       bus_size_t maxsegsz, int flags, bus_dma_lock_t *lockfunc,
		       void *lockfuncarg, bus_dma_tag_t *dmat);

int bus_dma_tag_destroy(bus_dma_tag_t dmat);

/*
 * Allocate a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int bus_dmamap_create(bus_dma_tag_t dmat, int flags, bus_dmamap_t *mapp);

/*
 * Destroy  a handle for mapping from kva/uva/physical
 * address space into bus device space.
 */
int bus_dmamap_destroy(bus_dma_tag_t dmat, bus_dmamap_t map);

/*
 * Allocate a piece of memory that can be efficiently mapped into
 * bus device space based on the constraints lited in the dma tag.
 * A dmamap to for use with dmamap_load is also allocated.
 */
int bus_dmamem_alloc(bus_dma_tag_t dmat, void** vaddr, int flags,
		     bus_dmamap_t *mapp);

/*
 * Free a piece of memory and it's allociated dmamap, that was allocated
 * via bus_dmamem_alloc.
 */
void bus_dmamem_free(bus_dma_tag_t dmat, void *vaddr, bus_dmamap_t map);

/*
 * A function that processes a successfully loaded dma map or an error
 * from a delayed load map.
 */
typedef void bus_dmamap_callback_t(void *, bus_dma_segment_t *, int, int);

/*
 * Map the buffer buf into bus space using the dmamap map.
 */
int bus_dmamap_load(bus_dma_tag_t dmat, bus_dmamap_t map, void *buf,
		    bus_size_t buflen, bus_dmamap_callback_t *callback,
		    void *callback_arg, int flags);

/*
 * Like bus_dmamap_callback but includes map size in bytes.  This is
 * defined as a separate interface to maintain compatiiblity for users
 * of bus_dmamap_callback_t--at some point these interfaces should be merged.
 */
typedef void bus_dmamap_callback2_t(void *, bus_dma_segment_t *, int, bus_size_t, int);
/*
 * Like bus_dmamap_load but for mbufs.  Note the use of the
 * bus_dmamap_callback2_t interface.
 */
int bus_dmamap_load_mbuf(bus_dma_tag_t dmat, bus_dmamap_t map,
			 struct mbuf *mbuf,
			 bus_dmamap_callback2_t *callback, void *callback_arg,
			 int flags);
/*
 * Like bus_dmamap_load but for uios.  Note the use of the
 * bus_dmamap_callback2_t interface.
 */
int bus_dmamap_load_uio(bus_dma_tag_t dmat, bus_dmamap_t map,
			struct uio *ui,
			bus_dmamap_callback2_t *callback, void *callback_arg,
			int flags);

/*
 * Perform a syncronization operation on the given map.
 */
void _bus_dmamap_sync(bus_dma_tag_t, bus_dmamap_t, bus_dmasync_op_t);
#define bus_dmamap_sync(dmat, dmamap, op) 		\
	if ((dmamap) != NULL)				\
		_bus_dmamap_sync(dmat, dmamap, op)

/*
 * Release the mapping held by map.
 */
void _bus_dmamap_unload(bus_dma_tag_t dmat, bus_dmamap_t map);
#define bus_dmamap_unload(dmat, dmamap) 		\
	if ((dmamap) != NULL)				\
		_bus_dmamap_unload(dmat, dmamap)

/*
 * Generic helper function for manipulating mutexes.
 */
void busdma_lock_mutex(void *arg, bus_dma_lock_op_t op);

#endif /* _ALPHA_BUS_H_ */
