/* $FreeBSD$ */
/*	$NecBSD: busiosubr.c,v 1.30.4.4 1999/08/28 02:25:35 honda Exp $	*/
/*	$NetBSD$	*/

/*
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1996, 1997, 1998
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <machine/bus.h>

static MALLOC_DEFINE(M_BUSSPACEHANDLE, "busspacehandle", "Bus space handle");

_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_io,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_io,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_io,u_int32_t,4)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_DA_mem,u_int32_t,4)

_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_io,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_io,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_io,u_int32_t,4)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int8_t,1)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(SBUS_RA_mem,u_int32_t,4)

struct bus_space_tag SBUS_io_space_tag = {
	BUS_SPACE_IO,

	/* direct bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_io,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_io,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_io,u_int32_t,4),
	},

	/* relocate bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_io,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_io,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_io,u_int32_t,4),
	}
};

struct bus_space_tag SBUS_mem_space_tag = {
	BUS_SPACE_MEM,

	/* direct bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int32_t,4),
	},

	/* relocate bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int32_t,4),
	}
};


#include "mecia.h"
#if NMECIA > 0

_BUS_SPACE_CALL_FUNCS_PROTO(NEPC_DA_io,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(NEPC_DA_io,u_int32_t,4)

_BUS_SPACE_CALL_FUNCS_PROTO(NEPC_RA_io,u_int16_t,2)
_BUS_SPACE_CALL_FUNCS_PROTO(NEPC_RA_io,u_int32_t,4)

struct bus_space_tag NEPC_io_space_tag = {
	BUS_SPACE_IO,

	/* direct bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_io,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(NEPC_DA_io,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(NEPC_DA_io,u_int32_t,4),
	},

	/* relocate bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_io,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(NEPC_RA_io,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(NEPC_RA_io,u_int32_t,4),
	}
};

struct bus_space_tag NEPC_mem_space_tag = {
	BUS_SPACE_MEM,

	/* direct bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_DA_mem,u_int32_t,4),
	},

	/* relocate bus access methods */
	{
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int8_t,1),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int16_t,2),
		_BUS_SPACE_CALL_FUNCS_TAB(SBUS_RA_mem,u_int32_t,4),
	}
};

#endif /* NMECIA > 0 */

/*************************************************************************
 * map init
 *************************************************************************/
static __inline void
bus_space_iat_init(bus_space_handle_t bsh)
{
	int i;

	for (i = 0; i < bsh->bsh_maxiatsz; i++)
		bsh->bsh_iat[i] = bsh->bsh_base + i;
}

/*************************************************************************
 * handle allocation
 *************************************************************************/
int
i386_bus_space_handle_alloc(bus_space_tag_t t, bus_addr_t bpa, bus_size_t size,
			    bus_space_handle_t *bshp)
{
	bus_space_handle_t bsh;

	bsh = (bus_space_handle_t) malloc(sizeof (*bsh), M_BUSSPACEHANDLE,
					  M_NOWAIT | M_ZERO);
	if (bsh == NULL)
		return ENOMEM;

	bsh->bsh_maxiatsz = BUS_SPACE_IAT_MAXSIZE;
	bsh->bsh_iatsz = 0;
	bsh->bsh_base = bpa;
	bsh->bsh_sz = size;
	bsh->bsh_res = NULL;
	bsh->bsh_ressz = 0;
	bus_space_iat_init(bsh);

	bsh->bsh_bam = t->bs_da;		/* default: direct access */

	*bshp = bsh;
	return 0;
}

void
i386_bus_space_handle_free(bus_space_tag_t t, bus_space_handle_t bsh,
			   size_t size)
{

	free(bsh, M_BUSSPACEHANDLE);
}

/*************************************************************************
 * map
 *************************************************************************/
void
i386_memio_unmap(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{

	i386_bus_space_handle_free(t, bsh, bsh->bsh_sz);
}

void
i386_memio_free(bus_space_tag_t t, bus_space_handle_t bsh, bus_size_t size)
{

	/* i386_memio_unmap() does all that we need to do. */
	i386_memio_unmap(t, bsh, bsh->bsh_sz);
}

int
i386_memio_subregion(bus_space_tag_t t, bus_space_handle_t pbsh,
		     bus_size_t offset, bus_size_t size,
		     bus_space_handle_t *tbshp)
{
	int i, error = 0;
	bus_space_handle_t bsh;
	bus_addr_t pbase;

	pbase = pbsh->bsh_base + offset;
	switch (t->bs_tag) {
	case BUS_SPACE_IO:
		if (pbsh->bsh_iatsz > 0) {
			if (offset >= pbsh->bsh_iatsz || 
			    offset + size > pbsh->bsh_iatsz)
				return EINVAL;
			pbase = pbsh->bsh_base;
		}
		break;

	case BUS_SPACE_MEM:
		if (pbsh->bsh_iatsz > 0)
			return EINVAL;
		if (offset > pbsh->bsh_sz || offset + size > pbsh->bsh_sz)
			return EINVAL;
		break;

	default:
		panic("i386_memio_subregion: bad bus space tag");
		break;
	}

	error = i386_bus_space_handle_alloc(t, pbase, size, &bsh);
	if (error != 0)
		return error;

	switch (t->bs_tag) {
	case BUS_SPACE_IO:
		if (pbsh->bsh_iatsz > 0) {
			for (i = 0; i < size; i ++)
				bsh->bsh_iat[i] = pbsh->bsh_iat[i + offset];
			bsh->bsh_iatsz = size;
		} else if (pbsh->bsh_base > bsh->bsh_base ||
		         pbsh->bsh_base + pbsh->bsh_sz <
		         bsh->bsh_base + bsh->bsh_sz) {
			i386_bus_space_handle_free(t, bsh, size);
			return EINVAL;
		}
		break;

	case BUS_SPACE_MEM:
		break;
	}

	if (pbsh->bsh_iatsz > 0)
		bsh->bsh_bam = t->bs_ra;	/* relocate access */
	*tbshp = bsh;
	return error;
}
