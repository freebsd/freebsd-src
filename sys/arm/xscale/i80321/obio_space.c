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
 * bus_space functions for IQ80321 on-board devices
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/i80321/obio_space.c,v 1.4.2.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/bus.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(generic);
bs_protos(generic_armv4);

/*
 * The obio bus space tag.  This is constant for all instances, so
 * we never have to explicitly "create" it.
 */
struct bus_space obio_bs_tag = {
	/* cookie */
	(void *) 0,

	/* mapping/unmapping */
	generic_bs_map,
	generic_bs_unmap,
	generic_bs_subregion,

	/* allocation/deallocation */
	generic_bs_alloc,
	generic_bs_free,

	/* barrier */
	generic_bs_barrier,

	/* read (single) */
	generic_bs_r_1,
	generic_armv4_bs_r_2,
	generic_bs_r_4,
	NULL,

	/* read multiple */
	generic_bs_rm_1,
	NULL,
	NULL,
	NULL,

	/* read region */
	generic_bs_rr_1,
	NULL,
	NULL,
	NULL,

	/* write (single) */
	generic_bs_w_1,
	generic_armv4_bs_w_2,
	generic_bs_w_4,
	NULL,

	/* write multiple */
	generic_bs_wm_1,
	NULL,
	NULL,
	NULL,

	/* write region */
	NULL,
	NULL,
	NULL,
	NULL,

	/* set multiple */
	NULL,
	NULL,
	NULL,
	NULL,

	/* set region */
	NULL,
	NULL,
	NULL,
	NULL,

	/* copy */
	NULL,
	NULL,
	NULL,
	NULL,
};
