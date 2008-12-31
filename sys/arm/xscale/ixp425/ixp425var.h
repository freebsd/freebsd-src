/*	$NetBSD: ixp425var.h,v 1.10 2006/04/10 03:36:03 simonb Exp $ */

/*
 * Copyright (c) 2003
 *	Ichiro FUKUHARA <ichiro@ichiro.org>.
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
 *	This product includes software developed by Ichiro FUKUHARA.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ICHIRO FUKUHARA ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL ICHIRO FUKUHARA OR THE VOICES IN HIS HEAD BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD: src/sys/arm/xscale/ixp425/ixp425var.h,v 1.4.6.1 2008/11/25 02:59:29 kensmith Exp $
 *
 */

#ifndef _IXP425VAR_H_
#define _IXP425VAR_H_

#include <sys/conf.h>
#include <sys/queue.h>

#include <machine/bus.h>

#include <dev/pci/pcivar.h>
#include <sys/rman.h>

struct ixp425_softc {
	device_t sc_dev;
	bus_space_tag_t sc_iot;
	bus_space_handle_t sc_gpio_ioh;
	bus_space_handle_t sc_exp_ioh;

	u_int32_t sc_intrmask;

	struct rman sc_irq_rman;
	struct rman sc_mem_rman;
	bus_dma_tag_t sc_dmat;
};

struct ixppcib_softc {
	device_t                sc_dev;
	
	u_int                   sc_bus;
	
	struct resource         *sc_csr;
	struct resource         *sc_mem;
	
	struct rman             sc_io_rman;
	struct rman             sc_mem_rman;
	struct rman             sc_irq_rman;
	
	struct bus_space        sc_pci_memt;
	struct bus_space        sc_pci_iot;
	bus_dma_tag_t 		sc_dmat;
};

#define EXP_BUS_WRITE_4(sc, reg, data) \
	bus_space_write_4(sc->sc_iot, sc->sc_exp_ioh, reg, data)
#define EXP_BUS_READ_4(sc, reg) \
	bus_space_read_4(sc->sc_iot, sc->sc_exp_ioh, reg)

#define	GPIO_CONF_WRITE_4(sc, reg, data)	\
	bus_space_write_4(sc->sc_iot, sc->sc_gpio_ioh, reg, data)
#define	GPIO_CONF_READ_4(sc, reg) \
	bus_space_read_4(sc->sc_iot, sc->sc_gpio_ioh, reg)

extern struct bus_space ixp425_bs_tag;
extern struct bus_space ixp425_a4x_bs_tag;

void	ixp425_io_bs_init(bus_space_tag_t, void *);
void	ixp425_mem_bs_init(bus_space_tag_t, void *);

uint32_t ixp425_sdram_size(void);

int	ixp425_md_route_interrupt(device_t, device_t, int);
void	ixp425_md_attach(device_t);

int	getvbase(uint32_t, uint32_t, uint32_t *);

struct ixp425_ivar {
	uint32_t	addr;
	int		irq;
};
#define	IXP425_IVAR(d)	((struct ixp425_ivar *) device_get_ivars(d))

enum {
	IXP425_IVAR_ADDR,		/* base physical address */
	IXP425_IVAR_IRQ			/* irq/gpio pin assignment */
};

#endif /* _IXP425VAR_H_ */
