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
#include <machine/bus.h>

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


#include "opt_mecia.h"
#ifdef DEV_MECIA

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

#endif /* DEV_MECIA */
