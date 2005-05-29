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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <isa/isavar.h>

#include <cam/scsi/scsi_all.h>

#include <dev/dpt/dpt.h>

#ifdef notyet
static void	dpt_isa_identify	(driver_t *, device_t);
#endif
static int	dpt_isa_probe		(device_t);
static int	dpt_isa_attach		(device_t);
static int	dpt_isa_detach		(device_t);

static int	dpt_isa_valid_irq	(int);
static int	dpt_isa_valid_ioport	(int);

static int
dpt_isa_valid_irq (int irq)
{
	switch (irq) {
		case 11:
		case 12:
		case 14:
		case 15:
			return (0);
		default:
			return (1);
	};
	return (1);
}

static int
dpt_isa_valid_ioport (int ioport)
{
	switch (ioport) {
		case 0x170:
		case 0x1f0:
		case 0x230:
		case 0x330:
			return (0);
		default:
			return (1);
	};
	return (1);
}

#ifdef notyet
static void
dpt_isa_identify (driver_t *driver, device_t parent)
{
	device_t	child;
	dpt_conf_t *	conf;
	int		isa_bases[] = { 0x1f0, 0x170, 0x330, 0x230, 0 };
	int		i;

	for (i = 0; isa_bases[i]; i++) {
		conf = dpt_pio_get_conf(isa_bases[i]);
	        if (!conf) {
			if (bootverbose)
				device_printf(parent, "dpt: dpt_pio_get_conf(%x) failed.\n",
					isa_bases[i]);
			continue;
		}

		child = BUS_ADD_CHILD(parent, ISA_ORDER_SPECULATIVE, "dpt", -1);
		if (child == 0) {
			device_printf(parent, "dpt: BUS_ADD_CHILD() failed!\n");
			continue;
		}
		device_set_driver(child, driver);
		bus_set_resource(child, SYS_RES_IOPORT, 0, isa_bases[i], 0x9);
	}
	return;
}
#endif

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

	if (dpt_isa_valid_ioport(io_base))
		;

	conf = dpt_pio_get_conf(io_base);
	if (!conf) {
		printf("dpt: dpt_pio_get_conf() failed.\n");
		return (ENXIO);
	}

	if (dpt_isa_valid_irq(conf->IRQ))
		;

	device_set_desc(dev, "ISA DPT SCSI controller");
	bus_set_resource(dev, SYS_RES_IRQ, 0, conf->IRQ, 1);
	bus_set_resource(dev, SYS_RES_DRQ, 0, ((8 - conf->DMA_channel) & 7), 1);

	return 0;
}

static int
dpt_isa_attach (device_t dev)
{
	dpt_softc_t *	dpt;
	int		s;
	int		error = 0;

	dpt = device_get_softc(dev);


	dpt->io_rid = 0;
	dpt->io_type = SYS_RES_IOPORT;
	dpt->irq_rid = 0;

	error = dpt_alloc_resources(dev);
	if (error) {
		goto bad;
	}

	dpt->drq_rid = 0;
	dpt->drq_res = bus_alloc_resource_any(dev, SYS_RES_DRQ, &dpt->drq_rid,
					RF_ACTIVE);
	if (!dpt->drq_res) {
		device_printf(dev, "No DRQ!\n");
		error = ENOMEM;
		goto bad;
	}
	isa_dma_acquire(rman_get_start(dpt->drq_res));
	isa_dmacascade(rman_get_start(dpt->drq_res));

	dpt_alloc(dev);

	/* Allocate a dmatag representing the capabilities of this attachment */
	if (bus_dma_tag_create( /* parent    */	NULL,
				/* alignemnt */	1,
				/* boundary  */	0,
				/* lowaddr   */	BUS_SPACE_MAXADDR_32BIT,
				/* highaddr  */	BUS_SPACE_MAXADDR,
				/* filter    */	NULL,
				/* filterarg */	NULL,
				/* maxsize   */	BUS_SPACE_MAXSIZE_32BIT,
				/* nsegments */	~0,
				/* maxsegsz  */	BUS_SPACE_MAXSIZE_32BIT,
				/* flags     */	0,
				/* lockfunc  */ busdma_lock_mutex,
				/* lockarg   */ &Giant,
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

	if (bus_setup_intr(dev, dpt->irq_res, INTR_TYPE_CAM | INTR_ENTROPY,
			   dpt_intr, dpt, &dpt->ih)) {
		device_printf(dev, "Unable to register interrupt handler\n");
		error = ENXIO;
		goto bad;
	}

	return (error);

 bad:
	if (dpt->drq_res) {
		isa_dma_release(rman_get_start(dpt->drq_res));
	}

	dpt_release_resources(dev);

	if (dpt)
		dpt_free(dpt);

	return (error);
}

static int
dpt_isa_detach (device_t dev)
{
	dpt_softc_t *	dpt;
	int		dma;
	int		error;

	dpt = device_get_softc(dev);

	dma = rman_get_start(dpt->drq_res);
	error = dpt_detach(dev);
	isa_dma_release(dma);

	return (error);
}


static device_method_t dpt_isa_methods[] = {
	/* Device interface */
#ifdef notyet
	DEVMETHOD(device_identify,	dpt_isa_identify),
#endif
	DEVMETHOD(device_probe,		dpt_isa_probe),
	DEVMETHOD(device_attach,	dpt_isa_attach),
	DEVMETHOD(device_detach,	dpt_isa_detach),

	{ 0, 0 }
};

static driver_t dpt_isa_driver = {
	"dpt",
	dpt_isa_methods,
	sizeof(dpt_softc_t),
};

DRIVER_MODULE(dpt, isa, dpt_isa_driver, dpt_devclass, 0, 0);
