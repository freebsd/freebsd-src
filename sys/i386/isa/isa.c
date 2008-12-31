/*-
 * Copyright (c) 1998 Doug Rabson
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD: src/sys/i386/isa/isa.c,v 1.151.6.1 2008/11/25 02:59:29 kensmith Exp $");

/*-
 * Modifications for Intel architecture by Garrett A. Wollman.
 * Copyright 1998 Massachusetts Institute of Technology
 *
 * Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby
 * granted, provided that both the above copyright notice and this
 * permission notice appear in all copies, that both the above
 * copyright notice and this permission notice appear in all
 * supporting documentation, and that the name of M.I.T. not be used
 * in advertising or publicity pertaining to distribution of the
 * software without specific, written prior permission.  M.I.T. makes
 * no representations about the suitability of this software for any
 * purpose.  It is provided "as is" without express or implied
 * warranty.
 * 
 * THIS SOFTWARE IS PROVIDED BY M.I.T. ``AS IS''.  M.I.T. DISCLAIMS
 * ALL EXPRESS OR IMPLIED WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. IN NO EVENT
 * SHALL M.I.T. BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <machine/bus.h>
#include <sys/rman.h>
#ifdef PC98
#include <sys/systm.h>
#endif

#include <machine/resource.h>

#include <isa/isavar.h>
#include <isa/isa_common.h>

void
isa_init(device_t dev)
{
}

/*
 * This implementation simply passes the request up to the parent
 * bus, which in our case is the special i386 nexus, substituting any
 * configured values if the caller defaulted.  We can get away with
 * this because there is no special mapping for ISA resources on an Intel
 * platform.  When porting this code to another architecture, it may be
 * necessary to interpose a mapping layer here.
 */
struct resource *
isa_alloc_resource(device_t bus, device_t child, int type, int *rid,
		   u_long start, u_long end, u_long count, u_int flags)
{
	/*
	 * Consider adding a resource definition.
	 */
	int passthrough = (device_get_parent(child) != bus);
	int isdefault = (start == 0UL && end == ~0UL);
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;
	struct resource_list_entry *rle;
	
	if (!passthrough && !isdefault) {
		rle = resource_list_find(rl, type, *rid);
		if (!rle) {
			if (*rid < 0)
				return 0;
			switch (type) {
			case SYS_RES_IRQ:
				if (*rid >= ISA_NIRQ)
					return 0;
				break;
			case SYS_RES_DRQ:
				if (*rid >= ISA_NDRQ)
					return 0;
				break;
			case SYS_RES_MEMORY:
				if (*rid >= ISA_NMEM)
					return 0;
				break;
			case SYS_RES_IOPORT:
				if (*rid >= ISA_NPORT)
					return 0;
				break;
			default:
				return 0;
			}
			resource_list_add(rl, type, *rid, start, end, count);
		}
	}

	return resource_list_alloc(rl, bus, child, type, rid,
				   start, end, count, flags);
}

#ifdef PC98
/*
 * Indirection support.  The type of bus_space_handle_t is
 * defined in sys/i386/include/bus_pc98.h.
 */
struct resource *
isa_alloc_resourcev(device_t child, int type, int *rid,
		    bus_addr_t *res, bus_size_t count, u_int flags)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;

	device_t	bus = device_get_parent(child);
	bus_addr_t	start;
	bus_space_handle_t bh;
	struct resource *re;
	struct resource	**bsre;
	int		i, j, k, linear_cnt, ressz, bsrid;

	start = bus_get_resource_start(child, type, *rid);

	linear_cnt = count;
	ressz = 1;
	for (i = 1; i < count; ++i) {
		if (res[i] != res[i - 1] + 1) {
			if (i < linear_cnt)
				linear_cnt = i;
			++ressz;
		}
	}

	re = isa_alloc_resource(bus, child, type, rid,
				start + res[0], start + res[linear_cnt - 1],
				linear_cnt, flags);
	if (re == NULL)
		return NULL;

	bsre = malloc(sizeof (struct resource *) * ressz, M_DEVBUF, M_NOWAIT);
	if (bsre == NULL) {
		resource_list_release(rl, bus, child, type, *rid, re);
		return NULL;
	}
	bsre[0] = re;

	for (i = linear_cnt, k = 1; i < count; i = j, k++) {
		for (j = i + 1; j < count; j++) {
			if (res[j] != res[j - 1] + 1)
				break;
		}
		bsrid = *rid + k;
		bsre[k] = isa_alloc_resource(bus, child, type, &bsrid,
			start + res[i], start + res[j - 1], j - i, flags);
		if (bsre[k] == NULL) {
			for (k--; k >= 0; k--)
				resource_list_release(rl, bus, child, type,
						      *rid + k, bsre[k]);
			free(bsre, M_DEVBUF);
			return NULL;
		}
	}

	bh = rman_get_bushandle(re);
	bh->bsh_res = bsre;
	bh->bsh_ressz = ressz;

	return re;
}

int
isa_load_resourcev(struct resource *re, bus_addr_t *res, bus_size_t count)
{
	bus_addr_t	start;
	bus_space_handle_t bh;
	int		i;

	bh = rman_get_bushandle(re);
	if (count > bh->bsh_maxiatsz) {
		printf("isa_load_resourcev: map size too large\n");
		return EINVAL;
	}

	start = rman_get_start(re);
	for (i = 0; i < bh->bsh_maxiatsz; i++) {
		if (i < count)
			bh->bsh_iat[i] = start + res[i];
		else
			bh->bsh_iat[i] = start;
	}

	bh->bsh_iatsz = count;
	bh->bsh_bam = rman_get_bustag(re)->bs_ra;	/* relocate access */

	return 0;
}
#endif	/* PC98 */

int
isa_release_resource(device_t bus, device_t child, int type, int rid,
		     struct resource *r)
{
	struct isa_device* idev = DEVTOISA(child);
	struct resource_list *rl = &idev->id_resources;
#ifdef PC98
	/*
	 * Indirection support.  The type of bus_space_handle_t is
	 * defined in sys/i386/include/bus_pc98.h.
	 */
	int	i;
	bus_space_handle_t bh;

	if (type == SYS_RES_MEMORY || type == SYS_RES_IOPORT) {
		bh = rman_get_bushandle(r);
		if (bh != NULL) {
			for (i = 1; i < bh->bsh_ressz; i++)
				resource_list_release(rl, bus, child, type,
						      rid + i, bh->bsh_res[i]);
			if (bh->bsh_res != NULL)
				free(bh->bsh_res, M_DEVBUF);
		}
	}
#endif
	return resource_list_release(rl, bus, child, type, rid, r);
}

/*
 * We can't use the bus_generic_* versions of these methods because those
 * methods always pass the bus param as the requesting device, and we need
 * to pass the child (the i386 nexus knows about this and is prepared to
 * deal).
 */
int
isa_setup_intr(device_t bus, device_t child, struct resource *r, int flags,
	       driver_filter_t filter, void (*ihand)(void *), void *arg,
	       void **cookiep)
{
	return (BUS_SETUP_INTR(device_get_parent(bus), child, r, flags,
			       filter, ihand, arg, cookiep));
}

int
isa_teardown_intr(device_t bus, device_t child, struct resource *r,
		  void *cookie)
{
	return (BUS_TEARDOWN_INTR(device_get_parent(bus), child, r, cookie));
}

/*
 * On this platform, isa can also attach to the legacy bus.
 */
DRIVER_MODULE(isa, legacy, isa_driver, isa_devclass, 0, 0);
