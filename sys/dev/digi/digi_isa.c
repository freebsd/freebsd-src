/*-
 * Copyright (c) 2001 Brian Somers <brian@Awfulhak.org>
 *   based on work by Slawa Olhovchenkov
 *                    John Prince <johnp@knight-trosoft.com>
 *                    Eric Hernes
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
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*-
 * TODO:
 *	Figure out how to make the non-Xi boards use memory addresses other
 *	than 0xd0000 !!!
 */

#include <sys/param.h>

#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/tty.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <sys/rman.h>
#include <machine/resource.h>
#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/digiio.h>
#include <dev/digi/digireg.h>
#include <dev/digi/digi.h>

/* Valid i/o addresses are any of these with either 0 or 4 added */
static u_long digi_validio[] = {
	0x100, 0x110, 0x120, 0x200, 0x220, 0x300, 0x320
};
#define DIGI_NVALIDIO	(sizeof(digi_validio) / sizeof(digi_validio[0]))
#define	IO_SIZE		0x04

static u_long digi_validmem[] = {
	0x80000, 0x88000, 0x90000, 0x98000, 0xa0000, 0xa8000, 0xb0000, 0xb8000,
	0xc0000, 0xc8000, 0xd0000, 0xd8000, 0xe0000, 0xe8000, 0xf0000, 0xf8000,
	0xf0000000, 0xf1000000, 0xf2000000, 0xf3000000, 0xf4000000, 0xf5000000,
	0xf6000000, 0xf7000000, 0xf8000000, 0xf9000000, 0xfa000000, 0xfb000000,
	0xfc000000, 0xfd000000, 0xfe000000, 0xff000000
};
#define DIGI_NVALIDMEM	(sizeof(digi_validmem) / sizeof(digi_validmem[0]))

static u_char *
digi_isa_setwin(struct digi_softc *sc, unsigned int addr)
{
	outb(sc->wport, sc->window = FEPWIN | (addr >> sc->win_bits));
	return (sc->vmem + (addr % sc->win_size));
}

static u_char *
digi_xi_setwin(struct digi_softc *sc, unsigned int addr)
{
	outb(sc->wport, sc->window = FEPMEM);
	return (sc->vmem + addr);
}

static void
digi_isa_hidewin(struct digi_softc *sc)
{
	outb(sc->wport, sc->window = 0);
	/* outb(sc->port, 0); */
}

static void
digi_isa_towin(struct digi_softc *sc, int win)
{
	outb(sc->wport, sc->window = win);
}

static void
digi_xi_towin(struct digi_softc *sc, int win)
{
	outb(sc->wport, sc->window = FEPMEM);
}

/*
 * sc->port should be set and its resource allocated.
 */
static int
digi_isa_check(struct digi_softc *sc)
{
	int i, ident;

	sc->name = NULL;

	/* Invasive probe - reset the card */
	outb(sc->port, FEPRST);
	for (i = 0; (inb(sc->port) & FEPMASK) != FEPRST; i++) {
		if (i == hz / 10)
			return (0);
		digi_delay(sc, "digirst", 1);
	}
	DLOG(DIGIDB_INIT, (sc->dev, "got reset after %d iterations\n", i));

	ident = inb(sc->port);

	/*
	 * NOTE, this probe is all wrong.  I haven't got the data sheets !
	 */

	DLOG(DIGIDB_INIT, (sc->dev, "board type is 0x%x\n", ident));
	if (ident & 0x1) {
		switch (ident) {
		case 0x05:
		case 0x15:
		case 0x25:
		case 0x35:
			sc->model = PCXI;
			sc->csigs = &digi_xixe_signals;
			switch (ident & 0x30) {
			case 0:
				sc->name = "Digiboard PC/Xi 64K";
				sc->mem_seg = 0xf000;
				sc->win_size = 0x10000;
				sc->win_bits = 16;
				break;
			case 0x10:
				sc->name = "Digiboard PC/Xi 128K";
				sc->mem_seg = 0xE000;
				sc->win_size = 0x20000;
				sc->win_bits = 17;
				break;
			case 0x20:
				sc->name = "Digiboard PC/Xi 256K";
				sc->mem_seg = 0xC000;
				sc->win_size = 0x40000;
				sc->win_bits = 18;
				break;
			case 0x30:
				sc->name = "Digiboard PC/Xi 512K";
				sc->mem_seg = 0x8000;
				sc->win_size = 0x80000;
				sc->win_bits = 19;
				break;
			}
			sc->wport = sc->port;
			sc->module = "Xe";

			sc->setwin = digi_xi_setwin;
			sc->hidewin = digi_isa_hidewin;
			sc->towin = digi_xi_towin;
			break;

		case 0xf5:
			sc->name = "Digiboard PC/Xem";
			sc->model = PCXEM;
			sc->csigs = &digi_normal_signals;
			sc->win_size = 0x8000;
			sc->win_bits = 15;
			sc->wport = sc->port + 1;
			sc->module = "Xem";

			sc->setwin = digi_isa_setwin;
			sc->hidewin = digi_isa_hidewin;
			sc->towin = digi_isa_towin;
			break;
		}
	} else {
		outb(sc->port, 1);
		ident = inb(sc->port);

		if (ident & 0x1) {
			device_printf(sc->dev, "PC/Xm is unsupported\n");
			return (0);
		}

		sc->mem_seg = 0xf000;

		if (!(ident & 0xc0)) {
			sc->name = "Digiboard PC/Xe 64K";
			sc->model = PCXE;
			sc->csigs = &digi_xixe_signals;
			sc->win_size = 0x10000;
			sc->win_bits = 16;
			sc->wport = sc->port;
		} else {
			sc->name = "Digiboard PC/Xe 64/8K (windowed)";
			sc->model = PCXEVE;
			sc->csigs = &digi_normal_signals;
			sc->win_size = 0x2000;
			sc->win_bits = 13;
			sc->wport = sc->port + 1;
		}
		sc->module = "Xe";

		sc->setwin = digi_isa_setwin;
		sc->hidewin = digi_isa_hidewin;
		sc->towin = digi_isa_towin;
	}

	return (sc->name != NULL);
}

static int
digi_isa_probe(device_t dev)
{
	struct digi_softc *sc = device_get_softc(dev);
	int i;

	KASSERT(sc, ("digi%d: softc not allocated in digi_isa_probe\n",
	    device_get_unit(dev)));

	bzero(sc, sizeof(*sc));
	sc->status = DIGI_STATUS_NOTINIT;
	sc->dev = dev;
	sc->res.unit = device_get_unit(dev);
	if (sc->res.unit >= 16) {
		/* Don't overflow our control mask */
		device_printf(dev, "At most 16 digiboards may be used\n");
		return (ENXIO);
	}
	DLOG(DIGIDB_INIT, (sc->dev, "probing on isa bus\n"));

	/* Check that we've got a valid i/o address */
	if ((sc->port = bus_get_resource_start(dev, SYS_RES_IOPORT, 0)) == 0) {
		DLOG(DIGIDB_INIT, (sc->dev, "io address not given\n"));
		return (ENXIO);
	}
	for (i = 0; i < DIGI_NVALIDIO; i++)
		if (sc->port == digi_validio[i] ||
		    sc->port == digi_validio[i] + 4)
			break;
	if (i == DIGI_NVALIDIO) {
		device_printf(dev, "0x%03x: Invalid i/o address\n", sc->port);
		return (ENXIO);
	}

	/* Ditto for our memory address */
	if ((sc->pmem = bus_get_resource_start(dev, SYS_RES_MEMORY, 0)) == 0)
		return (ENXIO);
	for (i = 0; i < DIGI_NVALIDMEM; i++)
		if (sc->pmem == digi_validmem[i])
			break;
	if (i == DIGI_NVALIDMEM) {
		device_printf(dev, "0x%lx: Invalid memory address\n", sc->pmem);
		return (ENXIO);
	}
	if ((sc->pmem & 0xfffffful) != sc->pmem) {
		device_printf(dev, "0x%lx: Memory address not supported\n",
		    sc->pmem);
		return (ENXIO);
	}
	sc->vmem = (u_char *)sc->pmem;

	DLOG(DIGIDB_INIT, (sc->dev, "isa? port 0x%03x mem 0x%lx\n",
	    sc->port, sc->pmem));

	/* Temporarily map our io ports */
	sc->res.iorid = 0;
	sc->res.io = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->res.iorid,
	    0ul, ~0ul, IO_SIZE, RF_ACTIVE);
	if (sc->res.io == NULL)
		return (ENXIO);

	/* Check the type of card and get internal memory characteristics */
	if (!digi_isa_check(sc)) {
		bus_release_resource(dev, SYS_RES_IOPORT, sc->res.iorid,
		    sc->res.io);
		return (ENXIO);
	}

	/* Temporarily map our memory */
	sc->res.mrid = 0;
	sc->res.mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->res.mrid,
	    0ul, ~0ul, sc->win_size, 0);
	if (sc->res.mem == NULL) {
		device_printf(dev, "0x%lx: Memory range is in use\n", sc->pmem);
		bus_release_resource(dev, SYS_RES_IOPORT, sc->res.iorid,
		    sc->res.io);
		return (ENXIO);
	}

	outb(sc->port, FEPCLR);		/* drop RESET */
	sc->hidewin(sc);	/* set initial sc->window */

	bus_release_resource(dev, SYS_RES_MEMORY, sc->res.mrid, sc->res.mem);
	bus_release_resource(dev, SYS_RES_IOPORT, sc->res.iorid, sc->res.io);

	/* Let digi_isa_attach() know what we've found */
	bus_set_resource(dev, SYS_RES_IOPORT, 0, sc->port, IO_SIZE);
	bus_set_resource(dev, SYS_RES_MEMORY, 0, sc->pmem, sc->win_size);

	DLOG(DIGIDB_INIT, (sc->dev, "Probe returns -10\n"));

	return (-10);		/* Other drivers are preferred for now */
}

static int
digi_isa_attach(device_t dev)
{
	struct digi_softc *sc = device_get_softc(dev);
	int i, t, res;
	u_char *ptr;
	int reset;
	u_long msize, iosize;
	long scport;

	KASSERT(sc, ("digi%d: softc not allocated in digi_isa_attach\n",
	    device_get_unit(dev)));

	res = ENXIO;
	bzero(sc, sizeof(*sc));
	sc->status = DIGI_STATUS_NOTINIT;
	sc->dev = dev;
	sc->res.unit = device_get_unit(dev);
	DLOG(DIGIDB_INIT, (sc->dev, "attaching\n"));

	bus_get_resource(dev, SYS_RES_IOPORT, 0, &scport, &iosize);
	bus_get_resource(dev, SYS_RES_MEMORY, 0, &sc->pmem, &msize);
	sc->port = scport;
	/* sc->altpin = !!(device_get_flags(dev) & DGBFLAG_ALTPIN); */

	/* Allocate resources (verified in digi_isa_probe()) */
	sc->res.iorid = 0;
	sc->res.io = bus_alloc_resource(dev, SYS_RES_IOPORT, &sc->res.iorid,
	    0ul, ~0ul, iosize, RF_ACTIVE);
	if (sc->res.io == NULL)
		return (ENXIO);

	/* Check the type of card and get internal memory characteristics */
	DLOG(DIGIDB_INIT, (sc->dev, "Checking card type\n"));
	if (!digi_isa_check(sc))
		goto failed;

	callout_handle_init(&sc->callout);
	callout_handle_init(&sc->inttest);

	sc->res.mrid = 0;
	sc->res.mem = bus_alloc_resource(dev, SYS_RES_MEMORY, &sc->res.mrid,
	    0ul, ~0ul, msize, RF_ACTIVE);
	if (sc->res.mem == NULL) {
		device_printf(dev, "0x%lx: Memory range is in use\n", sc->pmem);
		sc->hidewin(sc);
		goto failed;
	}

	/* map memory */
	sc->vmem = pmap_mapdev(sc->pmem, msize);

	DLOG(DIGIDB_INIT, (sc->dev, "internal memory segment 0x%x\n",
	    sc->mem_seg));

	/* Start by resetting the card */
	reset = FEPRST;
	if (sc->model == PCXI)
		reset |= FEPMEM;

	outb(sc->port, reset);
	for (i = 0; (inb(sc->port) & FEPMASK) != reset; i++) {
		if (i == hz / 10) {
			device_printf(dev, "1st reset failed\n");
			sc->hidewin(sc);
			goto failed;
		}
		digi_delay(sc, "digirst1", 1);
	}
	DLOG(DIGIDB_INIT, (sc->dev, "got reset after %d iterations\n", i));

	if (sc->model != PCXI) {
		t = (sc->pmem >> 8) & 0xffe0;
		if (sc->model == PCXEVE)
			t |= 0x10;		/* enable windowing */
		outb(sc->port + 2, t & 0xff);
		outb(sc->port + 3, t >> 8);
	}

	if (sc->model == PCXI || sc->model == PCXE) {
		outb(sc->port, FEPRST | FEPMEM);
		for (i = 0; (inb(sc->port) & FEPMASK) != FEPRST; i++) {
			if (i == hz / 10) {
				device_printf(dev,
				    "memory reservation failed (0x%02x)\n",
				    inb(sc->port));
				sc->hidewin(sc);
				goto failed;
			}
			digi_delay(sc, "digirst2", 1);
		}
		DLOG(DIGIDB_INIT, (sc->dev, "got memory after %d iterations\n",
		    i));
	}

	DLOG(DIGIDB_INIT, (sc->dev, "short memory test\n"));
	ptr = sc->setwin(sc, BOTWIN);
	vD(ptr) = 0xA55A3CC3;
	if (vD(ptr) != 0xA55A3CC3) {
		device_printf(dev, "1st memory test failed\n");
		sc->hidewin(sc);
		goto failed;
	}
	DLOG(DIGIDB_INIT, (sc->dev, "1st memory test ok\n"));

	ptr = sc->setwin(sc, TOPWIN);
	vD(ptr) = 0x5AA5C33C;
	if (vD(ptr) != 0x5AA5C33C) {
		device_printf(dev, "2nd memory test failed\n");
		sc->hidewin(sc);
		goto failed;
	}
	DLOG(DIGIDB_INIT, (sc->dev, "2nd memory test ok\n"));

	ptr = sc->setwin(sc, BIOSCODE + ((0xf000 - sc->mem_seg) << 4));
	vD(ptr) = 0x5AA5C33C;
	if (vD(ptr) != 0x5AA5C33C) {
		device_printf(dev, "3rd (BIOS) memory test failed\n");
		sc->hidewin(sc);
		goto failed;
	}
	DLOG(DIGIDB_INIT, (sc->dev, "3rd memory test ok\n"));

	if ((res = digi_attach(sc)) == 0)
		return (0);

failed:
	if (sc->res.mem != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY, sc->res.mrid,
		    sc->res.mem);
		sc->res.mem = NULL;
	}
	if (sc->res.io != NULL) {
		bus_release_resource(dev, SYS_RES_IOPORT, sc->res.iorid,
		    sc->res.io);
		sc->res.io = NULL;
	}

	return (res);
}

static device_method_t digi_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe, digi_isa_probe),
	DEVMETHOD(device_attach, digi_isa_attach),
	DEVMETHOD(device_detach, digi_detach),
	DEVMETHOD(device_shutdown, digi_shutdown),

	DEVMETHOD_END
};

static driver_t digi_isa_drv = {
	"digi",
	digi_isa_methods,
	sizeof(struct digi_softc),
};
DRIVER_MODULE(digi, isa, digi_isa_drv, digi_devclass, 0, 0);
