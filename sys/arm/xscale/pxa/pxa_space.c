/*	$NetBSD: obio_space.c,v 1.6 2003/07/15 00:25:05 lukem Exp $	*/

/*-
 * Copyright (c) 2001, 2002, 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * bus_space functions for PXA devices
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/bus.h>

#include <arm/xscale/pxa/pxareg.h>
#include <arm/xscale/pxa/pxavar.h>

static MALLOC_DEFINE(M_PXATAG, "PXA bus_space tags", "Bus_space tags for PXA");

/* Prototypes for all the bus_space structure functions */
bs_protos(generic);
bs_protos(pxa);

/*
 * The obio bus space tag.  This is constant for all instances, so
 * we never have to explicitly "create" it.
 */
struct bus_space _base_tag = {
	/* cookie */
	.bs_privdata	= NULL,

	/* mapping/unmapping */
	.bs_map		= generic_bs_map,
	.bs_unmap	= generic_bs_unmap,
	.bs_subregion	= generic_bs_subregion,

	/* allocation/deallocation */
	.bs_alloc	= generic_bs_alloc,
	.bs_free	= generic_bs_free,

	/* barrier */
	.bs_barrier	= generic_bs_barrier,

	/* read (single) */
	.bs_r_1		= pxa_bs_r_1,
	.bs_r_2		= pxa_bs_r_2,
	.bs_r_4		= pxa_bs_r_4,
	.bs_r_8		= BS_UNIMPLEMENTED,

	/* read multiple */
	.bs_rm_1	= pxa_bs_rm_1,
	.bs_rm_2	= pxa_bs_rm_2,
	.bs_rm_4	= BS_UNIMPLEMENTED,
	.bs_rm_8	= BS_UNIMPLEMENTED,

	/* read region */
	.bs_rr_1	= pxa_bs_rr_1,
	.bs_rr_2	= BS_UNIMPLEMENTED,
	.bs_rr_4	= BS_UNIMPLEMENTED,
	.bs_rr_8	= BS_UNIMPLEMENTED,

	/* write (single) */
	.bs_w_1		= pxa_bs_w_1,
	.bs_w_2		= pxa_bs_w_2,
	.bs_w_4		= pxa_bs_w_4,
	.bs_w_8		= BS_UNIMPLEMENTED,

	/* write multiple */
	.bs_wm_1	= pxa_bs_wm_1,
	.bs_wm_2	= pxa_bs_wm_2,
	.bs_wm_4	= BS_UNIMPLEMENTED,
	.bs_wm_8	= BS_UNIMPLEMENTED,

	/* write region */
	.bs_wr_1	= BS_UNIMPLEMENTED,
	.bs_wr_2	= BS_UNIMPLEMENTED,
	.bs_wr_4	= BS_UNIMPLEMENTED,
	.bs_wr_8	= BS_UNIMPLEMENTED,

	/* set multiple */
	.bs_sm_1	= BS_UNIMPLEMENTED,
	.bs_sm_2	= BS_UNIMPLEMENTED,
	.bs_sm_4	= BS_UNIMPLEMENTED,
	.bs_sm_8	= BS_UNIMPLEMENTED,

	/* set region */
	.bs_sr_1	= BS_UNIMPLEMENTED,
	.bs_sr_2	= BS_UNIMPLEMENTED,
	.bs_sr_4	= BS_UNIMPLEMENTED,
	.bs_sr_8	= BS_UNIMPLEMENTED,

	/* copy */
	.bs_c_1		= BS_UNIMPLEMENTED,
	.bs_c_2		= BS_UNIMPLEMENTED,
	.bs_c_4		= BS_UNIMPLEMENTED,
	.bs_c_8		= BS_UNIMPLEMENTED,

	/* read stream (single) */
	.bs_r_1_s	= BS_UNIMPLEMENTED,
	.bs_r_2_s	= BS_UNIMPLEMENTED,
	.bs_r_4_s	= BS_UNIMPLEMENTED,
	.bs_r_8_s	= BS_UNIMPLEMENTED,

	/* read multiple stream */
	.bs_rm_1_s	= BS_UNIMPLEMENTED,
	.bs_rm_2_s	= BS_UNIMPLEMENTED,
	.bs_rm_4_s	= BS_UNIMPLEMENTED,
	.bs_rm_8_s	= BS_UNIMPLEMENTED,

	/* read region stream */
	.bs_rr_1_s	= BS_UNIMPLEMENTED,
	.bs_rr_2_s	= BS_UNIMPLEMENTED,
	.bs_rr_4_s	= BS_UNIMPLEMENTED,
	.bs_rr_8_s	= BS_UNIMPLEMENTED,

	/* write stream (single) */
	.bs_w_1_s	= BS_UNIMPLEMENTED, 
	.bs_w_2_s	= BS_UNIMPLEMENTED, 
	.bs_w_4_s	= BS_UNIMPLEMENTED, 
	.bs_w_8_s	= BS_UNIMPLEMENTED, 

	/* write multiple stream */
	.bs_wm_1_s	= BS_UNIMPLEMENTED,
	.bs_wm_2_s	= BS_UNIMPLEMENTED,
	.bs_wm_4_s	= BS_UNIMPLEMENTED,
	.bs_wm_8_s	= BS_UNIMPLEMENTED,

	/* write region stream */
	.bs_wr_1_s	= BS_UNIMPLEMENTED,
	.bs_wr_2_s	= BS_UNIMPLEMENTED,
	.bs_wr_4_s	= BS_UNIMPLEMENTED,
	.bs_wr_8_s	= BS_UNIMPLEMENTED,
};

static struct bus_space	_obio_tag;

bus_space_tag_t		base_tag = &_base_tag;
bus_space_tag_t		obio_tag = NULL;

void
pxa_obio_tag_init()
{

	bcopy(&_base_tag, &_obio_tag, sizeof(struct bus_space));
	_obio_tag.bs_privdata = (void *)PXA2X0_PERIPH_OFFSET;
	obio_tag = &_obio_tag;
}

bus_space_tag_t
pxa_bus_tag_alloc(bus_addr_t offset)
{
	struct	bus_space *tag;

	tag = (struct bus_space *)malloc(sizeof(struct bus_space), M_PXATAG,
	    M_WAITOK);

	bcopy(&_base_tag, tag, sizeof(struct bus_space));
	tag->bs_privdata = (void *)offset;

	return ((bus_space_tag_t)tag);
}


#define	READ_SINGLE(type, proto, base)					\
	type								\
	proto(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t offset)	\
	{								\
		bus_addr_t	tag_offset;				\
		type		value;					\
		tag_offset = (bus_addr_t)tag->bs_privdata;		\
		value = base(NULL, bsh + tag_offset, offset);		\
		return (value);						\
	}

READ_SINGLE(u_int8_t,  pxa_bs_r_1, generic_bs_r_1)
READ_SINGLE(u_int16_t, pxa_bs_r_2, generic_bs_r_2)
READ_SINGLE(u_int32_t, pxa_bs_r_4, generic_bs_r_4)

#undef READ_SINGLE

#define	WRITE_SINGLE(type, proto, base)					\
	void								\
	proto(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t offset,	\
	    type value)							\
	{								\
		bus_addr_t	tag_offset;				\
		tag_offset = (bus_addr_t)tag->bs_privdata;		\
		base(NULL, bsh + tag_offset, offset, value);		\
	}

WRITE_SINGLE(u_int8_t,  pxa_bs_w_1, generic_bs_w_1)
WRITE_SINGLE(u_int16_t, pxa_bs_w_2, generic_bs_w_2)
WRITE_SINGLE(u_int32_t, pxa_bs_w_4, generic_bs_w_4)

#undef WRITE_SINGLE

#define	READ_MULTI(type, proto, base)					\
	void								\
	proto(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t offset,	\
	    type *dest, bus_size_t count)				\
	{								\
		bus_addr_t	tag_offset;				\
		tag_offset = (bus_addr_t)tag->bs_privdata;		\
		base(NULL, bsh + tag_offset, offset, dest, count);	\
	}

READ_MULTI(u_int8_t,  pxa_bs_rm_1, generic_bs_rm_1)
READ_MULTI(u_int16_t, pxa_bs_rm_2, generic_bs_rm_2)

READ_MULTI(u_int8_t,  pxa_bs_rr_1, generic_bs_rr_1)

#undef READ_MULTI

#define	WRITE_MULTI(type, proto, base)					\
	void								\
	proto(bus_space_tag_t tag, bus_space_handle_t bsh, bus_size_t offset,	\
	    const type *src, bus_size_t count)				\
	{								\
		bus_addr_t	tag_offset;				\
		tag_offset = (bus_addr_t)tag->bs_privdata;		\
		base(NULL, bsh + tag_offset, offset, src, count);	\
	}

WRITE_MULTI(u_int8_t,  pxa_bs_wm_1, generic_bs_wm_1)
WRITE_MULTI(u_int16_t, pxa_bs_wm_2, generic_bs_wm_2)

#undef WRITE_MULTI
