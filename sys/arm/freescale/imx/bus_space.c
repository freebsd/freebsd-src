/*-
 * Copyright (C) 2012, 2013 FreeBSD Foundation
 * All rights reserved.
 *
 * This software was developed by Oleksandr Tymoshenko under sponsorship
 * from the FreeBSD Foundation.
 * Portions of this software were developed by Oleksandr Rybalko
 * under sponsorship from the FreeBSD Foundation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of MARVELL nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>

#include <machine/bus.h>

/* Prototypes for all the bus_space structure functions */
bs_protos(generic);
bs_protos(generic_armv4);

struct bus_space _base_tag = {
	/* cookie */
	.bs_cookie	= (void *) 0,

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
	.bs_r_1		= generic_bs_r_1,
	.bs_r_2		= generic_armv4_bs_r_2,
	.bs_r_4		= generic_bs_r_4,
	.bs_r_8		= NULL,

	/* read multiple */
	.bs_rm_1	= generic_bs_rm_1,
	.bs_rm_2	= generic_armv4_bs_rm_2,
	.bs_rm_4	= generic_bs_rm_4,
	.bs_rm_8	= NULL,

	/* read region */
	.bs_rr_1	= generic_bs_rr_1,
	.bs_rr_2	= generic_armv4_bs_rr_2,
	.bs_rr_4	= generic_bs_rr_4,
	.bs_rr_8	= NULL,

	/* write (single) */
	.bs_w_1		= generic_bs_w_1,
	.bs_w_2		= generic_armv4_bs_w_2,
	.bs_w_4		= generic_bs_w_4,
	.bs_w_8		= NULL,

	/* write multiple */
	.bs_wm_1	= generic_bs_wm_1,
	.bs_wm_2	= generic_armv4_bs_wm_2,
	.bs_wm_4	= generic_bs_wm_4,
	.bs_wm_8	= NULL,

	/* write region */
	.bs_wr_1	= generic_bs_wr_1,
	.bs_wr_2	= generic_armv4_bs_wr_2,
	.bs_wr_4	= generic_bs_wr_4,
	.bs_wr_8	= NULL,

	/* read multiple stream */
	.bs_rm_1_s 	= generic_bs_rm_1,
	.bs_rm_2_s 	= generic_armv4_bs_rm_2,
	.bs_rm_4_s 	= generic_bs_rm_4,
	.bs_rm_8_s 	= NULL,

	/* write multiple stream */
	.bs_wm_1_s 	= generic_bs_wm_1,
	.bs_wm_2_s 	= generic_armv4_bs_wm_2,
	.bs_wm_4_s 	= generic_bs_wm_4,
	.bs_wm_8_s 	= NULL,

	/* set multiple */
	/* XXX not implemented */

	/* set region */
	.bs_sr_1	= NULL,
	.bs_sr_2	= generic_armv4_bs_sr_2,
	.bs_sr_4	= generic_bs_sr_4,
	.bs_sr_8	= NULL,

	/* copy */
	.bs_c_1		= NULL,
	.bs_c_2		= generic_armv4_bs_c_2,
	.bs_c_4		= NULL,
	.bs_c_8		= NULL,
};

bus_space_tag_t fdtbus_bs_tag = &_base_tag;
