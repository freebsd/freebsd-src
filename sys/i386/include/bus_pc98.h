/* $FreeBSD$ */
/*	$NecBSD: busio.h,v 3.25.4.2.2.1 2000/06/12 03:53:08 honda Exp $	*/
/*	$NetBSD: bus.h,v 1.12 1997/10/01 08:25:15 fvdl Exp $	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1997, 1998
 *	NetBSD/pc98 porting staff. All rights reserved.
 *
 * [Ported for FreeBSD]
 *  Copyright (c) 2001
 *	TAKAHASHI Yoshihiro. All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 1997, 1998
 *	Naofumi HONDA.  All rights reserved.
 *
 * This module support generic bus address relocation mechanism.
 * To reduce a function call overhead, we employ pascal call methods.
 */

#ifndef _I386_BUS_PC98_H_
#define _I386_BUS_PC98_H_

#include <sys/systm.h>

#include <machine/cpufunc.h>

/*
 * Bus address and size types
 */
typedef u_int bus_addr_t;
typedef u_int bus_size_t;

#define BUS_SPACE_MAXSIZE_24BIT	0xFFFFFF
#define BUS_SPACE_MAXSIZE_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXSIZE	(64 * 1024) /* Maximum supported size */
#define BUS_SPACE_MAXADDR_24BIT	0xFFFFFF
#define BUS_SPACE_MAXADDR_32BIT 0xFFFFFFFF
#define BUS_SPACE_MAXADDR	0xFFFFFFFF

#define BUS_SPACE_UNRESTRICTED	(~0)

#define BUS_SPACE_IAT_MAXSIZE	32

/*
 * Access methods for bus resources and address space.
 */
struct resource;

/*
 * bus space tag
 */
#define	_PASCAL_CALL	__P((void))

#define	_BUS_SPACE_CALL_FUNCS_TAB(NAME,TYPE,BWN) \
	NAME##_space_read_##BWN##, 				\
	NAME##_space_read_multi_##BWN##, 			\
	NAME##_space_read_region_##BWN##,			\
	NAME##_space_write_##BWN##, 				\
	NAME##_space_write_multi_##BWN##, 			\
	NAME##_space_write_region_##BWN##,			\
	NAME##_space_set_multi_##BWN##,				\
	NAME##_space_set_region_##BWN##,			\
	NAME##_space_copy_region_##BWN

#define	_BUS_SPACE_CALL_FUNCS_PROTO(NAME,TYPE,BWN) \
	TYPE NAME##_space_read_##BWN _PASCAL_CALL;		\
	void NAME##_space_read_multi_##BWN _PASCAL_CALL;	\
	void NAME##_space_read_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_write_##BWN _PASCAL_CALL;		\
	void NAME##_space_write_multi_##BWN _PASCAL_CALL;	\
	void NAME##_space_write_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_set_multi_##BWN _PASCAL_CALL;		\
	void NAME##_space_set_region_##BWN _PASCAL_CALL;	\
	void NAME##_space_copy_region_##BWN _PASCAL_CALL;

#define	_BUS_SPACE_CALL_FUNCS(NAME,TYPE,BWN) \
	TYPE (*##NAME##_read_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_read_multi_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_read_region_##BWN) _PASCAL_CALL;	\
	void (*##NAME##_write_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_write_multi_##BWN) _PASCAL_CALL;	\
	void (*##NAME##_write_region_##BWN) _PASCAL_CALL;	\
	void (*##NAME##_set_multi_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_set_region_##BWN) _PASCAL_CALL;		\
	void (*##NAME##_copy_region_##BWN) _PASCAL_CALL;	

struct bus_space_access_methods {
	/* 8 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int8_t,1)

	/* 16 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int16_t,2)

	/* 32 bits access methods */
	_BUS_SPACE_CALL_FUNCS(bs,u_int32_t,4)
};

struct bus_space_tag {
#define	BUS_SPACE_IO	0
#define	BUS_SPACE_MEM	1
	u_int	bs_tag;			/* bus space flags */

	struct bus_space_access_methods bs_da;	/* direct access */
	struct bus_space_access_methods bs_ra;	/* relocate access */
#if	0
	struct bus_space_access_methods bs_ida;	/* indexed direct access */
#endif
};
typedef struct bus_space_tag *bus_space_tag_t;

/*
 * Values for the i386 bus space tag, not to be used directly by MI code.
 */
extern struct bus_space_tag SBUS_io_space_tag;
extern struct bus_space_tag SBUS_mem_space_tag;

#define I386_BUS_SPACE_IO	(&SBUS_io_space_tag)
#define I386_BUS_SPACE_MEM	(&SBUS_mem_space_tag)

/*
 * bus space handle
 */
struct bus_space_handle {
	bus_addr_t	bsh_base;
	size_t		bsh_sz;

	bus_addr_t	bsh_iat[BUS_SPACE_IAT_MAXSIZE];
	size_t		bsh_maxiatsz;
	size_t		bsh_iatsz;

	struct resource	**bsh_res;
	size_t		bsh_ressz;

	struct bus_space_access_methods bsh_bam;
};
typedef struct bus_space_handle *bus_space_handle_t;

/*
 * Allocate/Free bus_space_handle
 */
int i386_bus_space_handle_alloc(bus_space_tag_t t, bus_addr_t bpa,
				bus_size_t size, bus_space_handle_t *bshp);
void i386_bus_space_handle_free(bus_space_tag_t t, bus_space_handle_t bsh,
				size_t size);

/*
 *      int bus_space_unmap (bus_space_tag_t t,
 *          bus_space_handle_t bsh, bus_size_t size);
 *
 * Unmap a region of bus space.
 */

void i386_memio_unmap(bus_space_tag_t t, bus_space_handle_t bsh,
		       bus_size_t size);

#define bus_space_unmap(t, h, s)					\
	i386_memio_unmap((t), (h), (s))

/*
 *      int bus_space_subregion (bus_space_tag_t t,
 *          bus_space_handle_t bsh, bus_size_t offset, bus_size_t size,
 *          bus_space_handle_t *nbshp);
 *
 * Get a new handle for a subregion of an already-mapped area of bus space.
 */

int i386_memio_subregion(bus_space_tag_t t, bus_space_handle_t bsh,
			 bus_size_t offset, bus_size_t size,
			 bus_space_handle_t *nbshp);

#define bus_space_subregion(t, h, o, s, nhp)				\
	i386_memio_subregion((t), (h), (o), (s), (nhp))

/*
 *      int bus_space_free (bus_space_tag_t t,
 *          bus_space_handle_t bsh, bus_size_t size);
 *
 * Free a region of bus space.
 */

void i386_memio_free(bus_space_tag_t t, bus_space_handle_t bsh,
		     bus_size_t size);

#define bus_space_free(t, h, s)						\
	i386_memio_free((t), (h), (s))

/*
 * Access methods for bus resources and address space.
 */
#define	_BUS_ACCESS_METHODS_PROTO(TYPE,BWN) \
	static __inline TYPE bus_space_read_##BWN 			\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t offset));	\
	static __inline void bus_space_read_multi_##BWN			\
	__P((bus_space_tag_t, bus_space_handle_t,			\
	     bus_size_t, TYPE *, size_t));				\
	static __inline void bus_space_read_region_##BWN		\
	__P((bus_space_tag_t, bus_space_handle_t,			\
	     bus_size_t, TYPE *, size_t));				\
	static __inline void bus_space_write_##BWN			\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t, TYPE));	\
	static __inline void bus_space_write_multi_##BWN		\
	__P((bus_space_tag_t, bus_space_handle_t,			\
	     bus_size_t, const TYPE *, size_t));			\
	static __inline void bus_space_write_region_##BWN		\
	__P((bus_space_tag_t, bus_space_handle_t,			\
	     bus_size_t, const TYPE *, size_t));			\
	static __inline void bus_space_set_multi_##BWN			\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t, TYPE, size_t));\
	static __inline void bus_space_set_region_##BWN			\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t, TYPE, size_t));\
	static __inline void bus_space_copy_region_##BWN		\
	__P((bus_space_tag_t, bus_space_handle_t, bus_size_t,		\
	     bus_space_handle_t, bus_size_t, size_t));

_BUS_ACCESS_METHODS_PROTO(u_int8_t,1)
_BUS_ACCESS_METHODS_PROTO(u_int16_t,2)
_BUS_ACCESS_METHODS_PROTO(u_int32_t,4)

/*
 * read methods
 */
#define	_BUS_SPACE_READ(TYPE,BWN)				\
static __inline TYPE						\
bus_space_read_##BWN##(tag, bsh, offset)			\
	bus_space_tag_t tag;					\
	bus_space_handle_t bsh;					\
	bus_size_t offset;					\
{								\
	register TYPE result;					\
								\
	__asm __volatile("call *%2"  				\
			:"=a" (result),				\
			 "=d" (offset)				\
			:"o" (bsh->bsh_bam.bs_read_##BWN),	\
			 "b" (bsh),				\
			 "1" (offset)				\
			);					\
								\
	return result;						\
}

_BUS_SPACE_READ(u_int8_t,1)
_BUS_SPACE_READ(u_int16_t,2)
_BUS_SPACE_READ(u_int32_t,4)

/*
 * write methods
 */
#define	_BUS_SPACE_WRITE(TYPE,BWN)				\
static __inline void						\
bus_space_write_##BWN##(tag, bsh, offset, val)			\
	bus_space_tag_t tag;					\
	bus_space_handle_t bsh;					\
	bus_size_t offset;					\
	TYPE val;						\
{								\
								\
	__asm __volatile("call *%1"				\
			:"=d" (offset)				\
			:"o" (bsh->bsh_bam.bs_write_##BWN),	\
			 "a" (val),				\
			 "b" (bsh),				\
			 "0" (offset)				\
			);					\
}								

_BUS_SPACE_WRITE(u_int8_t,1)
_BUS_SPACE_WRITE(u_int16_t,2)
_BUS_SPACE_WRITE(u_int32_t,4)

/*
 * multi read
 */
#define	_BUS_SPACE_READ_MULTI(TYPE,BWN)					\
static __inline void							\
bus_space_read_multi_##BWN##(tag, bsh, offset, buf, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE *buf;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%3"					\
			:"=c" (cnt),					\
			 "=d" (offset),					\
			 "=D" (buf)					\
			:"o" (bsh->bsh_bam.bs_read_multi_##BWN),	\
			 "b" (bsh),					\
			 "0" (cnt),					\
			 "1" (offset),					\
			 "2" (buf)					\
			:"memory");					\
}

_BUS_SPACE_READ_MULTI(u_int8_t,1)
_BUS_SPACE_READ_MULTI(u_int16_t,2)
_BUS_SPACE_READ_MULTI(u_int32_t,4)

/*
 * multi write
 */
#define	_BUS_SPACE_WRITE_MULTI(TYPE,BWN)				\
static __inline void							\
bus_space_write_multi_##BWN##(tag, bsh, offset, buf, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	const TYPE *buf;						\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%3"					\
			:"=c" (cnt),					\
			 "=d" (offset),					\
			 "=S" (buf)					\
			:"o" (bsh->bsh_bam.bs_write_multi_##BWN),	\
			 "b" (bsh),					\
			 "0" (cnt),					\
			 "1" (offset),					\
			 "2" (buf)					\
			);						\
}

_BUS_SPACE_WRITE_MULTI(u_int8_t,1)
_BUS_SPACE_WRITE_MULTI(u_int16_t,2)
_BUS_SPACE_WRITE_MULTI(u_int32_t,4)

/*
 * region read
 */
#define	_BUS_SPACE_READ_REGION(TYPE,BWN)				\
static __inline void							\
bus_space_read_region_##BWN##(tag, bsh, offset, buf, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE *buf;						\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%3"					\
			:"=c" (cnt),					\
			 "=d" (offset),					\
			 "=D" (buf)					\
			:"o" (bsh->bsh_bam.bs_read_region_##BWN),	\
			 "b" (bsh),					\
			 "0" (cnt),					\
			 "1" (offset),					\
			 "2" (buf)					\
			:"memory");					\
}

_BUS_SPACE_READ_REGION(u_int8_t,1)
_BUS_SPACE_READ_REGION(u_int16_t,2)
_BUS_SPACE_READ_REGION(u_int32_t,4)

/*
 * region write
 */
#define	_BUS_SPACE_WRITE_REGION(TYPE,BWN)				\
static __inline void							\
bus_space_write_region_##BWN##(tag, bsh, offset, buf, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	const TYPE *buf;						\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%3"					\
			:"=c" (cnt),					\
			 "=d" (offset),					\
			 "=S" (buf)					\
			:"o" (bsh->bsh_bam.bs_write_region_##BWN),	\
			 "b" (bsh),					\
			 "0" (cnt),					\
			 "1" (offset),					\
			 "2" (buf)					\
			);						\
}

_BUS_SPACE_WRITE_REGION(u_int8_t,1)
_BUS_SPACE_WRITE_REGION(u_int16_t,2)
_BUS_SPACE_WRITE_REGION(u_int32_t,4)

/*
 * multi set
 */
#define	_BUS_SPACE_SET_MULTI(TYPE,BWN)					\
static __inline void							\
bus_space_set_multi_##BWN##(tag, bsh, offset, val, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE val;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%2"					\
			:"=c" (cnt),					\
			 "=d" (offset)					\
			:"o" (bsh->bsh_bam.bs_set_multi_##BWN),		\
			 "a" (val),					\
			 "b" (bsh),					\
			 "0" (cnt),					\
			 "1" (offset)					\
			);						\
}

_BUS_SPACE_SET_MULTI(u_int8_t,1)
_BUS_SPACE_SET_MULTI(u_int16_t,2)
_BUS_SPACE_SET_MULTI(u_int32_t,4)

/*
 * region set
 */
#define	_BUS_SPACE_SET_REGION(TYPE,BWN)					\
static __inline void							\
bus_space_set_region_##BWN##(tag, bsh, offset, val, cnt) 		\
	bus_space_tag_t tag;						\
	bus_space_handle_t bsh;						\
	bus_size_t offset;						\
	TYPE val;							\
	size_t cnt;							\
{									\
									\
	__asm __volatile("call *%2"					\
			:"=c" (cnt),					\
			 "=d" (offset)					\
			:"o" (bsh->bsh_bam.bs_set_region_##BWN),	\
			 "a" (val),					\
			 "b" (bsh),					\
			 "0" (cnt),					\
			 "1" (offset)					\
			);						\
}

_BUS_SPACE_SET_REGION(u_int8_t,1)
_BUS_SPACE_SET_REGION(u_int16_t,2)
_BUS_SPACE_SET_REGION(u_int32_t,4)

/*
 * copy
 */
#define	_BUS_SPACE_COPY_REGION(BWN)					\
static __inline void							\
bus_space_copy_region_##BWN##(tag, sbsh, src, dbsh, dst, cnt)		\
	bus_space_tag_t tag;						\
	bus_space_handle_t sbsh;					\
	bus_size_t src;							\
	bus_space_handle_t dbsh;					\
	bus_size_t dst;							\
	size_t cnt;							\
{									\
									\
	if (dbsh->bsh_bam.bs_copy_region_1 != sbsh->bsh_bam.bs_copy_region_1) \
		panic("bus_space_copy_region: funcs mismatch (ENOSUPPORT)");\
									\
	__asm __volatile("call *%3"					\
			:"=c" (cnt),					\
			 "=S" (src),					\
			 "=D" (dst)					\
			:"o" (dbsh->bsh_bam.bs_copy_region_##BWN),	\
			 "a" (sbsh),					\
			 "b" (dbsh),					\
			 "0" (cnt),					\
			 "1" (src),					\
			 "2" (dst)					\
			);						\
}

_BUS_SPACE_COPY_REGION(1)
_BUS_SPACE_COPY_REGION(2)
_BUS_SPACE_COPY_REGION(4)

/*
 * Bus read/write barrier methods.
 *
 *	void bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t bsh,
 *			       bus_size_t offset, bus_size_t len, int flags);
 *
 *
 * Note that BUS_SPACE_BARRIER_WRITE doesn't do anything other than
 * prevent reordering by the compiler; all Intel x86 processors currently
 * retire operations outside the CPU in program order.
 */
#define	BUS_SPACE_BARRIER_READ	0x01		/* force read barrier */
#define	BUS_SPACE_BARRIER_WRITE	0x02		/* force write barrier */

static __inline void
bus_space_barrier(bus_space_tag_t tag, bus_space_handle_t bsh,
		  bus_size_t offset, bus_size_t len, int flags)
{
	if (flags & BUS_SPACE_BARRIER_READ)
		__asm __volatile("lock; addl $0,0(%%esp)" : : : "memory");
	else
		__asm __volatile("" : : : "memory");
}

#endif /* _I386_BUS_PC98_H_ */
