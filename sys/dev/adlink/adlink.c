/*-
 * Copyright (c) 2003 Poul-Henning Kamp
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
 * 3. The names of the authors may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
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
 * $FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <pci_if.h>
#include <vm/vm.h>
#include <vm/pmap.h>

/*
 * We sample one channel (= 16 bits) at 1 msps giving 2Mbyte/sec,
 * 50 pages will give us about 1/10 second buffering.
 */
#define NRING	50

#define IN4(sc, offset) bus_space_read_4(sc->t_io, sc->h_io, offset)

struct info {
	int			nring;
	off_t			o_ring;
};

struct softc {
	device_t		device;
	void			*intrhand;
	struct resource		*r0, *r1, *ri;
	bus_space_tag_t		t0, t1;
	bus_space_handle_t	h0, h1;
	dev_t			dev;

	struct info		*info;

	int			idx;
	void			*ring[NRING];
	vm_paddr_t		phys[NRING];
	int			stat[NRING];
};

static int
adlink_open(dev_t dev, int oflags, int devtype, struct thread *td)
{
	static int once;
	struct softc *sc;
	int i;
	uint32_t u;

	if (once)
		return (0);
	once = 1;

	sc = dev->si_drv1;
	sc->info = malloc(PAGE_SIZE, M_DEVBUF, M_ZERO | M_WAITOK);
	sc->info->nring = NRING;
	sc->info->o_ring = PAGE_SIZE;
	for (i = 0; i < NRING; i++) {
		sc->ring[i] = malloc(PAGE_SIZE, M_DEVBUF, M_ZERO | M_WAITOK);
		sc->phys[i] = vtophys(sc->ring[i]);
	}

	bus_space_write_4(sc->t0, sc->h0, 0x38, 0x00004000);
	bus_space_write_4(sc->t1, sc->h1, 0x00, 1);
	bus_space_write_4(sc->t1, sc->h1, 0x04, 10);
	bus_space_write_4(sc->t1, sc->h1, 0x08, 0);
	bus_space_write_4(sc->t1, sc->h1, 0x0c, 0);
	bus_space_write_4(sc->t1, sc->h1, 0x10, 0);
	bus_space_write_4(sc->t1, sc->h1, 0x18, 3);
	bus_space_write_4(sc->t1, sc->h1, 0x20, 2);

	bus_space_write_4(sc->t0, sc->h0, 0x24, sc->phys[i]);
	bus_space_write_4(sc->t0, sc->h0, 0x28, PAGE_SIZE);

	u = bus_space_read_4(sc->t0, sc->h0, 0x3c);
	bus_space_write_4(sc->t0, sc->h0, 0x3c, u | 0x00000600);

	bus_space_write_4(sc->t1, sc->h1, 0x1c, 1);
	return (0);
}

static int
adlink_mmap(dev_t dev, vm_offset_t offset, vm_paddr_t *paddr, int nprot)
{
	int i;
	struct softc *sc;

	sc = dev->si_drv1;
	if (nprot != VM_PROT_READ)
		return (-1);
	if (offset == 0) {
		*paddr = vtophys(sc->info);
		return (0);
	}
	i = (offset - sc->info->o_ring) / PAGE_SIZE;
	if (i >= NRING)
		return (-1);
	*paddr = vtophys(sc->ring[i]);
	return (0);
}

static void
adlink_intr(void *arg)
{
	struct softc *sc;
	uint32_t u;
	int i;

	sc = arg;
	u = bus_space_read_4(sc->t0, sc->h0, 0x38);
	if (!(u & 0x00800000))
		return;
	bus_space_write_4(sc->t0, sc->h0, 0x38, u | 0x003f4000);

	sc->stat[sc->idx] = 1;
	i = (++sc->idx) % NRING;
	sc->idx = i;
	bus_space_write_4(sc->t0, sc->h0, 0x24, sc->phys[i]);
	bus_space_write_4(sc->t0, sc->h0, 0x28, PAGE_SIZE);
}

static struct cdevsw adlink_cdevsw = {
	.d_open =	adlink_open,
	.d_close =	nullclose,
	.d_mmap =	adlink_mmap,
	.d_name =	"adlink",
};

static devclass_t adlink_devclass;

static int
adlink_probe(device_t self)
{

	if (pci_get_devid(self) != 0x80da10e8)
		return (ENXIO);
	device_set_desc(self, "Adlink PCI-9812 4 ch 12 bit 20 msps");
	return (0);
}

static int
adlink_attach(device_t self)
{
	struct softc *sc;
	int rid, i;

	sc = device_get_softc(self);
	bzero(sc, sizeof *sc);
	sc->device = self;

	rid = 0x10;
	sc->r0 = bus_alloc_resource(self, SYS_RES_IOPORT, &rid,
	    0, ~0, 1, RF_ACTIVE);
	if (sc->r0 == NULL)
		return(ENODEV);
	sc->t0 = rman_get_bustag(sc->r0);
	sc->h0 = rman_get_bushandle(sc->r0);
	printf("Res0 %x %x\n", sc->t0, sc->h0);

	rid = 0x14;
	sc->r1 =  bus_alloc_resource(self, SYS_RES_IOPORT, &rid, 
            0, ~0, 1, RF_ACTIVE);
	if (sc->r1 == NULL)
		return(ENODEV);
	sc->t1 = rman_get_bustag(sc->r1);
	sc->h1 = rman_get_bushandle(sc->r1);
	printf("Res1 %x %x\n", sc->t1, sc->h1);

	rid = 0x0;
	sc->ri =  bus_alloc_resource(self, SYS_RES_IRQ, &rid, 
            0, ~0, 1, RF_ACTIVE | RF_SHAREABLE);
	if (sc->ri == NULL)
		return (ENODEV);

	i = bus_setup_intr(self, sc->ri, INTR_TYPE_MISC,
	    adlink_intr, sc, &sc->intrhand);

	if (i)
		return (ENODEV);

	sc->dev = make_dev(&adlink_cdevsw, device_get_unit(self),
	    UID_ROOT, GID_WHEEL, 0444, "adlink%d", device_get_unit(self));
	sc->dev->si_drv1 = sc;

	return (0);
}

static device_method_t adlink_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		adlink_probe),
	DEVMETHOD(device_attach,	adlink_attach),
	DEVMETHOD(device_suspend,	bus_generic_suspend),
	DEVMETHOD(device_resume,	bus_generic_resume),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	{0, 0}
};
 
static driver_t adlink_driver = {
	"adlink",
	adlink_methods,
	sizeof(struct softc)
};

DRIVER_MODULE(adlink, pci, adlink_driver, adlink_devclass, 0, 0);
