/*	$NetBSD: mainbus_io.c,v 1.13 2003/07/15 00:24:47 lukem Exp $	*/

/*-
 * Copyright (c) 1997 Mark Brinicombe.
 * All rights reserved.
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
 *	This product includes software developed by Mark Brinicombe.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * bus_space I/O functions for mainbus
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/arm/arm/nexus_io.c,v 1.7 2006/11/19 23:46:50 sam Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>


#include <machine/bus.h>
#include <machine/pmap.h>

/* Proto types for all the bus_space structure functions */
vm_offset_t lala;
bs_protos(nexus);
/* Declare the mainbus bus space tag */

struct bus_space mainbus_bs_tag = {
	/* cookie */
	NULL,

	/* mapping/unmapping */
	nexus_bs_map,
	nexus_bs_unmap,
	nexus_bs_subregion,

	/* allocation/deallocation */
	nexus_bs_alloc,
	nexus_bs_free,

	/* barrier */
	nexus_bs_barrier,

	/* read (single) */
	nexus_bs_r_1,
	nexus_bs_r_2,
	nexus_bs_r_4,
	NULL,

	/* read multiple */
	NULL,
	nexus_bs_rm_2,
	NULL,
	NULL,

	/* read region */
	NULL,
	NULL,
	NULL,
	NULL,

	/* write (single) */
	nexus_bs_w_1,
	nexus_bs_w_2,
	nexus_bs_w_4,
	NULL,

	/* write multiple */
	nexus_bs_wm_1,
	nexus_bs_wm_2,
	NULL,
	NULL,

	/* write region */
	NULL,
	NULL,
	NULL,
	NULL,

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

/* bus space functions */

int
nexus_bs_map(void *t, bus_addr_t bpa, bus_size_t size, int cacheable, 
    bus_space_handle_t *bshp)
{
	return(0);
}

int
nexus_bs_alloc(t, rstart, rend, size, alignment, boundary, cacheable,
    bpap, bshp)
	void *t;
	bus_addr_t rstart, rend;
	bus_size_t size, alignment, boundary;
	int cacheable;
	bus_addr_t *bpap;
	bus_space_handle_t *bshp;
{
	panic("mainbus_bs_alloc(): Help!");
}


void
nexus_bs_unmap(void *t, bus_space_handle_t h, bus_size_t size)
{
	/*
	 * Temporary implementation
	 */
}

void    
nexus_bs_free(t, bsh, size)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t size;
{

	panic("mainbus_bs_free(): Help!");
	/* mainbus_bs_unmap() does all that we need to do. */
/*	mainbus_bs_unmap(t, bsh, size);*/
}

int
nexus_bs_subregion(t, bsh, offset, size, nbshp)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, size;
	bus_space_handle_t *nbshp;
{

	*nbshp = bsh + offset;
	return (0);
}

int
nexus_bs_mmap(struct cdev *dev, vm_offset_t off, vm_paddr_t *addr, int prot)
{
	*addr = off;
	return (0);
}

void
nexus_bs_barrier(t, bsh, offset, len, flags)
	void *t;
	bus_space_handle_t bsh;
	bus_size_t offset, len;
	int flags;
{
}	

/* End of mainbus_io.c */
