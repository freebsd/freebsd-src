/*	$NetBSD: i82365_isa.c,v 1.11 1998/06/09 07:25:00 thorpej Exp $	*/
/* $FreeBSD$ */

#define	PCICISADEBUG

/*
 * Copyright (c) 1997 Marc Horowitz.  All rights reserved.
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
 *	This product includes software developed by Marc Horowitz.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/kernel.h>
#include <sys/queue.h>
#include <sys/types.h>

#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>

#ifdef __FreeBSD__
#include <i386/isa/isa_device.h>
typedef int isa_chipset_tag_t;
#define ia_msize ia_memsize
#define delay(x) DELAY(x)
#else
#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>
#endif

#include <dev/pccard/pccardreg.h>
#include <dev/pccard/pccardvar.h>
#include <dev/pccard/pccardchip.h>

#include <dev/pcic/i82365reg.h>
#include <dev/pcic/i82365var.h>
#include <dev/pcic/i82365_isavar.h>

#ifdef PCICISADEBUG
int	pcicisa_debug = 0 /* XXX */ ;
#define	DPRINTF(arg) if (pcicisa_debug) printf arg;
#else
#define	DPRINTF(arg)
#endif

int	pcic_isa_probe __P((struct device *, struct cfdata *, void *));
void	pcic_isa_attach __P((struct device *, struct device *, void *));

void	*pcic_isa_chip_intr_establish __P((pccard_chipset_handle_t,
	    struct pccard_function *, int, int (*) (void *), void *));
void	pcic_isa_chip_intr_disestablish __P((pccard_chipset_handle_t, void *));

#ifdef __FreeBSD__
struct cfattach pcic_isa_ca = {
    sizeof(struct pcic_softc), pcic_isa_probe, pcic_isa_attach
};
#else
struct cfattach pcic_isa_ca = {
	sizeof(struct pcic_softc), pcic_isa_probe, pcic_isa_attach
};
#endif
static struct pccard_chip_functions pcic_isa_functions = {
	pcic_chip_mem_alloc,
	pcic_chip_mem_free,
	pcic_chip_mem_map,
	pcic_chip_mem_unmap,

	pcic_chip_io_alloc,
	pcic_chip_io_free,
	pcic_chip_io_map,
	pcic_chip_io_unmap,

	pcic_isa_chip_intr_establish,
	pcic_isa_chip_intr_disestablish,

	pcic_chip_socket_enable,
	pcic_chip_socket_disable,
};

int
pcic_isa_probe(parent, match, aux)
	struct device *parent;
	struct cfdata *match;
	void *aux;
{
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_handle_t ioh, memh;
	int val, found;

	/* Disallow wildcarded i/o address. */
#ifdef __FreeBSD__
	if (ia->ia_iobase == ISACF_IOBASE_DEFAULT)
#else
	if (ia->ia_iobase == ISACF_PORT_DEFAULT)
#endif
		return (0);

	if (bus_space_map(iot, ia->ia_iobase, PCIC_IOSIZE, 0, &ioh))
		return (0);

	if (ia->ia_msize == -1)
		ia->ia_msize = PCIC_MEMSIZE;
#ifdef __FreeBSD__
	if (bus_space_map(ia->ia_memt, kvtop(ia->ia_membase), ia->ia_msize, 0, &memh))
#else
	if (bus_space_map(ia->ia_memt, ia->ia_maddr, ia->ia_msize, 0, &memh))
#endif
		return (0);

	found = 0;

	/*
	 * this could be done with a loop, but it would violate the
	 * abstraction
	 */

	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C0SA + PCIC_IDENT);

	val = bus_space_read_1(iot, ioh, PCIC_REG_DATA);

	if (pcic_ident_ok(val))
		found++;


	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C0SB + PCIC_IDENT);

	val = bus_space_read_1(iot, ioh, PCIC_REG_DATA);

	if (pcic_ident_ok(val))
		found++;


	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C1SA + PCIC_IDENT);

	val = bus_space_read_1(iot, ioh, PCIC_REG_DATA);

	if (pcic_ident_ok(val))
		found++;


	bus_space_write_1(iot, ioh, PCIC_REG_INDEX, C1SB + PCIC_IDENT);

	val = bus_space_read_1(iot, ioh, PCIC_REG_DATA);

	if (pcic_ident_ok(val))
		found++;


	bus_space_unmap(iot, ioh, PCIC_IOSIZE);

	bus_space_unmap(ia->ia_memt, memh, ia->ia_msize);

	if (!found)
		return (0);

	ia->ia_iosize = PCIC_IOSIZE;

	return (1);
}

void
pcic_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct pcic_softc *sc = (void *) self;
	struct isa_attach_args *ia = aux;
	isa_chipset_tag_t ic = ia->ia_ic;
	bus_space_tag_t iot = ia->ia_iot;
	bus_space_tag_t memt = ia->ia_memt;
	bus_space_handle_t ioh;
	bus_space_handle_t memh;

	/* Map i/o space. */
	if (bus_space_map(iot, ia->ia_iobase, ia->ia_iosize, 0, &ioh)) {
		printf(": can't map i/o space\n");
		return;
	}

	/* Map mem space. */
#ifdef __FreeBSD__
	if (bus_space_map(memt, kvtop(ia->ia_membase), ia->ia_memsize, 0, &memh)) {
#else
	if (bus_space_map(memt, ia->ia_maddr, ia->ia_msize, 0, &memh)) {
#endif
		printf(": can't map mem space\n");
		return;
	}
#ifdef __FreeBSD__
	sc->membase = kvtop(ia->ia_membase);
#else
	sc->membase = ia->ia_maddr;
#endif
	sc->subregionmask = (1 << (ia->ia_msize / PCIC_MEM_PAGESIZE)) - 1;

	sc->intr_est = ic;
	sc->pct = (pccard_chipset_tag_t) & pcic_isa_functions;

	sc->iot = iot;
	sc->ioh = ioh;
	sc->memt = memt;
	sc->memh = memh;

	/*
	 * allocate an irq.  it will be used by both controllers.  I could
	 * use two different interrupts, but interrupts are relatively
	 * scarce, shareable, and for PCIC controllers, very infrequent.
	 */

	if ((sc->irq = ia->ia_irq) == IRQUNK) {
		if (isa_intr_alloc(ic,
		    PCIC_CSC_INTR_IRQ_VALIDMASK & pcic_isa_intr_alloc_mask,
		    IST_EDGE, &sc->irq)) {
			printf("\n%s: can't allocate interrupt\n",
			    sc->dev.dv_xname);
			return;
		}
		printf(": using irq %d", sc->irq);
	}
	printf("\n");

	pcic_attach(sc);

	pcic_isa_bus_width_probe (sc, iot, ioh, ia->ia_iobase, ia->ia_iosize);

	sc->ih = isa_intr_establish(ic, sc->irq, IST_EDGE, IPL_TTY,
	    pcic_intr, sc);
	if (sc->ih == NULL) {
		printf("%s: can't establish interrupt\n", sc->dev.dv_xname);
		return;
	}

	pcic_attach_sockets(sc);
}
