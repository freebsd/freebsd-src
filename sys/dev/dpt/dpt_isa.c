/*-
 * Copyright (c) 2000 Matthew N. Dodd <winter@jurai.net>
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
 *	$FreeBSD$
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus_pio.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <cam/scsi/scsi_all.h>

#include <dev/dpt/dpt.h>

static int	dpt_isa_probe		(device_t);
static int	dpt_isa_attach		(device_t);

static int
dpt_isa_probe (device_t dev)
{
	dpt_conf_t *	conf;
	u_int32_t	io_base;

	/* No pnp support */
	if (isa_get_vendorid(dev))
		return (ENXIO);

	if ((io_base = bus_get_resource_start(dev, SYS_RES_IOPORT, 0)) == 0)
		return (ENXIO);

	conf = dpt_pio_get_conf(io_base);
        if (!conf) {
                printf("dpt: dpt_pio_get_conf() failed.\n");
                return (ENXIO);
        }

	device_set_desc(dev, "ISA DPT SCSI controller");
	bus_set_resource(dev, SYS_RES_IRQ, 0, conf->IRQ, 1);
	bus_set_resource(dev, SYS_RES_DRQ, 0, ((8 - conf->DMA_channel) & 7), 1);

	return 0;
}

static int
dpt_isa_attach (device_t dev)
{
	dpt_softc_t *	dpt = NULL;
	struct resource *io = 0;
	struct resource *irq = 0;
	struct resource *drq = 0;
	int		s;
	int		rid;
	void *		ih;
	int		error = 0;

	rid = 0;
	io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!io) {
		device_printf(dev, "No I/O space?!\n");
		error = ENOMEM;
		goto bad;
	}

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!irq) {
		device_printf(dev, "No IRQ!\n");
		error = ENOMEM;
		goto bad;
	}

	rid = 0;
	drq = bus_alloc_resource(dev, SYS_RES_DRQ, &rid, 0, ~0, 1, RF_ACTIVE);
	if (!drq) {
		device_printf(dev, "No DRQ?!\n");
		error = ENOMEM;
		goto bad;
	}

	dpt = dpt_alloc(dev, rman_get_bustag(io), rman_get_bushandle(io));
	if (dpt == NULL) {
		error = ENXIO;
		goto bad;
	}

	isa_dmacascade(rman_get_start(drq));

	/* Allocate a dmatag representing the capabilities of this attachment */
	if (bus_dma_tag_create( /* parent    */	NULL,
				/* alignemnt */	1,
				/* boundary  */	0,
				/* lowaddr   */	BUS_SPACE_MAXADDR_24BIT,
				/* highaddr  */	BUS_SPACE_MAXADDR,
				/* filter    */	NULL,
				/* filterarg */	NULL,
				/* maxsize   */	BUS_SPACE_MAXSIZE_32BIT,
				/* nsegments */	~0,
				/* maxsegsz  */	BUS_SPACE_MAXSIZE_32BIT,
				/* flags     */	0,
				&dpt->parent_dmat) != 0) {
		error = ENXIO;
		goto bad;
	}

	s = splcam();

	if (dpt_init(dpt) != 0) {
		splx(s);
		error = ENXIO;
		goto bad;
	}

	/* Register with the XPT */
	dpt_attach(dpt);

	splx(s);

	if (bus_setup_intr(dev, irq, INTR_TYPE_CAM | INTR_ENTROPY, dpt_intr,
			   dpt, &ih)) {
		device_printf(dev, "Unable to register interrupt handler\n");
		error = ENXIO;
		goto bad;
	}

	return (error);

 bad:
	if (dpt)
		dpt_free(dpt);
	if (io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
	if (irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, irq);
	if (drq)
		bus_release_resource(dev, SYS_RES_DRQ, 0, drq);

	return (error);
}

static device_method_t dpt_isa_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		dpt_isa_probe),
	DEVMETHOD(device_attach,	dpt_isa_attach),

	{ 0, 0 }
};

static driver_t dpt_isa_driver = {
	"dpt",
	dpt_isa_methods,
	sizeof(dpt_softc_t),
};

static devclass_t dpt_devclass;

DRIVER_MODULE(dpt, isa, dpt_isa_driver, dpt_devclass, 0, 0);
