/*	$NetBSD: obiovar.h,v 1.4 2003/06/16 17:40:53 thorpej Exp $	*/

/*-
 * Copyright (c) 2002, 2003 Wasabi Systems, Inc.
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
 *
 * $FreeBSD$
 *
 */

#ifndef _PXAVAR_H_
#define	_PXAVAR_H_

#include <sys/rman.h>

struct obio_softc {
	bus_space_tag_t obio_bst;	/* bus space tag */
	struct rman	obio_mem;
	struct rman	obio_irq;
};

extern bus_space_tag_t	base_tag;
extern bus_space_tag_t	obio_tag;
void		pxa_obio_tag_init(void);
bus_space_tag_t	pxa_bus_tag_alloc(bus_addr_t);

uint32_t	pxa_gpio_get_function(int);
uint32_t	pxa_gpio_set_function(int, uint32_t);
int		pxa_gpio_setup_intrhandler(const char *, driver_filter_t *,
		    driver_intr_t *, void *, int, int, void **);
void		pxa_gpio_mask_irq(int);
void		pxa_gpio_unmask_irq(int);
int		pxa_gpio_get_next_irq(void);

struct dmac_channel;

struct dmac_descriptor {
	uint32_t	ddadr;
	uint32_t	dsadr;
	uint32_t	dtadr;
	uint32_t	dcmd;
};
#define	DMACD_SET_DESCRIPTOR(d, dadr)	do { d->ddadr = dadr; } while (0)
#define	DMACD_SET_SOURCE(d, sadr)	do { d->dsadr = sadr; } while (0)
#define	DMACD_SET_TARGET(d, tadr)	do { d->dtadr = tadr; } while (0)
#define	DMACD_SET_COMMAND(d, cmd)	do { d->dcmd = cmd; } while (0)

#define	DMAC_PRIORITY_HIGHEST	1
#define	DMAC_PRIORITY_HIGH	2
#define	DMAC_PRIORITY_LOW	3

int	pxa_dmac_alloc(int, struct dmac_channel **, int);
void	pxa_dmac_release(struct dmac_channel *);
int	pxa_dmac_transfer(struct dmac_channel *, bus_addr_t);
int	pxa_dmac_transfer_single(struct dmac_channel *,
		    bus_addr_t, bus_addr_t, uint32_t);
int	pxa_dmac_transfer_done(struct dmac_channel *);
int	pxa_dmac_transfer_failed(struct dmac_channel *);

enum pxa_device_ivars {
	PXA_IVAR_BASE,
};

enum smi_device_ivars {
	SMI_IVAR_PHYSBASE,
};

#define	PXA_ACCESSOR(var, ivar, type)	\
	__BUS_ACCESSOR(pxa, var, PXA, ivar, type)

PXA_ACCESSOR(base,	BASE,	u_long)

#undef	PXA_ACCESSOR

#define	SMI_ACCESSOR(var, ivar, type)	\
	__BUS_ACCESSOR(smi, var, SMI, ivar, type)

SMI_ACCESSOR(physbase,	PHYSBASE,	bus_addr_t)

#undef CSR_ACCESSOR

#endif /* _PXAVAR_H_ */
