/*      $NetBSD: bus.h,v 1.12 1997/10/01 08:25:15 fvdl Exp $    */
/*-
 * $Id: bus.h,v 1.6 2007/08/09 11:23:32 katta Exp $
 *
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
 *
 *	from: src/sys/alpha/include/bus.h,v 1.5 1999/08/28 00:38:40 peter
 * $FreeBSD$
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/ktr.h>

#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <machine/bus.h>
#include <machine/cache.h>

static int	fdt_bs_map(void *, bus_addr_t, bus_size_t, int,
		    bus_space_handle_t *);

static struct bus_space fdt_space = {
	/* cookie */
	.bs_cookie =	(void *) 0,

	/* mapping/unmapping */
	.bs_map =	fdt_bs_map,
	.bs_unmap =	generic_bs_unmap,
	.bs_subregion =	generic_bs_subregion,

	/* allocation/deallocation */
	.bs_alloc =	generic_bs_alloc,
	.bs_free =	generic_bs_free,

	/* barrier */
	.bs_barrier =	generic_bs_barrier,

	/* read (single) */
	.bs_r_1 =	generic_bs_r_1,
	.bs_r_2 =	generic_bs_r_2,
	.bs_r_4 =	generic_bs_r_4,
	.bs_r_8 =	generic_bs_r_8,

	/* read multiple */
	.bs_rm_1 =	generic_bs_rm_1,
	.bs_rm_2 =	generic_bs_rm_2,
	.bs_rm_4 =	generic_bs_rm_4,
	.bs_rm_8 =	generic_bs_rm_8,

	/* read region */
	.bs_rr_1 =	generic_bs_rr_1,
	.bs_rr_2 =	generic_bs_rr_2,
	.bs_rr_4 =	generic_bs_rr_4,
	.bs_rr_8 =	generic_bs_rr_8,

	/* write (single) */
	.bs_w_1 =	generic_bs_w_1,
	.bs_w_2 =	generic_bs_w_2,
	.bs_w_4 =	generic_bs_w_4,
	.bs_w_8 =	generic_bs_w_8,

	/* write multiple */
	.bs_wm_1 =	generic_bs_wm_1,
	.bs_wm_2 =	generic_bs_wm_2,
	.bs_wm_4 =	generic_bs_wm_4,
	.bs_wm_8 =	generic_bs_wm_8,

	/* write region */
	.bs_wr_1 =	generic_bs_wr_1,
	.bs_wr_2 =	generic_bs_wr_2,
	.bs_wr_4 =	generic_bs_wr_4,
	.bs_wr_8 =	generic_bs_wr_8,

	/* set multiple */
	.bs_sm_1 =	generic_bs_sm_1,
	.bs_sm_2 =	generic_bs_sm_2,
	.bs_sm_4 =	generic_bs_sm_4,
	.bs_sm_8 =	generic_bs_sm_8,

	/* set region */
	.bs_sr_1 =	generic_bs_sr_1,
	.bs_sr_2 =	generic_bs_sr_2,
	.bs_sr_4 =	generic_bs_sr_4,
	.bs_sr_8 =	generic_bs_sr_8,

	/* copy */
	.bs_c_1 =	generic_bs_c_1,
	.bs_c_2 =	generic_bs_c_2,
	.bs_c_4 =	generic_bs_c_4,
	.bs_c_8 =	generic_bs_c_8,

	/* read (single) stream */
	.bs_r_1_s =	generic_bs_r_1,
	.bs_r_2_s =	generic_bs_r_2,
	.bs_r_4_s =	generic_bs_r_4,
	.bs_r_8_s =	generic_bs_r_8,

	/* read multiple stream */
	.bs_rm_1_s =	generic_bs_rm_1,
	.bs_rm_2_s =	generic_bs_rm_2,
	.bs_rm_4_s =	generic_bs_rm_4,
	.bs_rm_8_s =	generic_bs_rm_8,

	/* read region stream */
	.bs_rr_1_s =	generic_bs_rr_1,
	.bs_rr_2_s =	generic_bs_rr_2,
	.bs_rr_4_s =	generic_bs_rr_4,
	.bs_rr_8_s =	generic_bs_rr_8,

	/* write (single) stream */
	.bs_w_1_s =	generic_bs_w_1,
	.bs_w_2_s =	generic_bs_w_2,
	.bs_w_4_s =	generic_bs_w_4,
	.bs_w_8_s =	generic_bs_w_8,

	/* write multiple stream */
	.bs_wm_1_s =	generic_bs_wm_1,
	.bs_wm_2_s =	generic_bs_wm_2,
	.bs_wm_4_s =	generic_bs_wm_4,
	.bs_wm_8_s =	generic_bs_wm_8,

	/* write region stream */
	.bs_wr_1_s =	generic_bs_wr_1,
	.bs_wr_2_s =	generic_bs_wr_2,
	.bs_wr_4_s =	generic_bs_wr_4,
	.bs_wr_8_s =	generic_bs_wr_8,
};

/* generic bus_space tag */
bus_space_tag_t	mips_bus_space_fdt = &fdt_space;

static int
fdt_bs_map(void *t __unused, bus_addr_t addr, bus_size_t size __unused,
    int flags __unused, bus_space_handle_t *bshp)
{

	*bshp = MIPS_PHYS_TO_DIRECT_UNCACHED(addr);
	return (0);
}
