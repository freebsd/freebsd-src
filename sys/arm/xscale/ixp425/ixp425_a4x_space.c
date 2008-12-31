/*	$NetBSD: ixp425_a4x_space.c,v 1.2 2005/12/11 12:16:51 christos Exp $	*/

/*
 * Copyright 2003 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Steve C. Woodford for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
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
 * Bus space tag for 8/16-bit devices on 32-bit bus.
 * all registers are located at the address of multiple of 4.
 *
 * Based on pxa2x0_a4x_space.c
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/xscale/ixp425/ixp425_a4x_space.c,v 1.1.4.1.2.1 2008/11/25 02:59:29 kensmith Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>

#include <machine/pcb.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/pmap.h>
#include <vm/vm_page.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(a4x);
bs_protos(generic);
bs_protos(generic_armv4);

struct bus_space ixp425_a4x_bs_tag = {
	/* cookie */
	.bs_cookie	= (void *) 0,

	/* mapping/unmapping */
	.bs_map		= generic_bs_map,
	.bs_unmap	= generic_bs_unmap,
	.bs_subregion	= generic_bs_subregion,

	/* allocation/deallocation */
	.bs_alloc	= generic_bs_alloc,	/* XXX not implemented */
	.bs_free	= generic_bs_free,	/* XXX not implemented */

	/* barrier */
	.bs_barrier	= generic_bs_barrier,

	/* read (single) */
	.bs_r_1		= a4x_bs_r_1,
	.bs_r_2		= a4x_bs_r_2,
	.bs_r_4		= a4x_bs_r_4,

	/* read multiple */
	.bs_rm_1	= a4x_bs_rm_1,
	.bs_rm_2	= a4x_bs_rm_2,

	/* read region */
	/* XXX not implemented */

	/* write (single) */
	.bs_w_1		= a4x_bs_w_1,
	.bs_w_2		= a4x_bs_w_2,
	.bs_w_4		= a4x_bs_w_4,

	/* write multiple */
	.bs_wm_1	= a4x_bs_wm_1,
	.bs_wm_2	= a4x_bs_wm_2,

	/* write region */
	/* XXX not implemented */

	/* set multiple */
	/* XXX not implemented */

	/* set region */
	/* XXX not implemented */

	/* copy */
	/* XXX not implemented */
};
