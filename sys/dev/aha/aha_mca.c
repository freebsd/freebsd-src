/*-
 * Copyright (c) 1999 Matthew N. Dodd <winter@jurai.net>
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
 * $FreeBSD$
 *
 * Based on aha_isa.c
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>

#include <sys/module.h>
#include <sys/bus.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <i386/isa/isa_dma.h>

#include <dev/mca/mca_busreg.h>
#include <dev/mca/mca_busvar.h>

#include <dev/aha/ahareg.h>

static struct mca_ident aha_mca_devs[] = {
	{ 0x0f1f, "Adaptec AHA-1640 SCSI Adapter" },
	{ 0, NULL },
};

#define AHA_MCA_IOPORT_POS		MCA_ADP_POS(MCA_POS1)
# define AHA_MCA_IOPORT_MASK1		0x07
# define AHA_MCA_IOPORT_MASK2		0xc0
# define AHA_MCA_IOPORT_SIZE		0x03
# define AHA_MCA_IOPORT(pos)		(0x30 + \
					(((u_int32_t)pos & \
						AHA_MCA_IOPORT_MASK1) << 8) + \
					(((u_int32_t)pos & \
						AHA_MCA_IOPORT_MASK2) >> 4))

#define AHA_MCA_DRQ_POS			MCA_ADP_POS(MCA_POS3)
# define AHA_MCA_DRQ_MASK		0x0f
# define AHA_MCA_DRQ(pos)		(pos & AHA_MCA_DRQ_MASK)

#define AHA_MCA_IRQ_POS			MCA_ADP_POS(MCA_POS2)
# define AHA_MCA_IRQ_MASK		0x07
# define AHA_MCA_IRQ(pos)		((pos & AHA_MCA_IRQ_MASK) + 8)

/*
 * Not needed as the board knows its config
 * internally and the ID will be fetched 
 * via AOP_INQUIRE_SETUP_INFO command.
 */
#define AHA_MCA_SCSIID_POS		MCA_ADP_POS(MCA_POS2)
#define AHA_MCA_SCSIID_MASK		0xe0
#define AHA_MCA_SCSIID(pos)		((pos & AHA_MCA_SCSIID_MASK) >> 5)

static int
aha_mca_probe (device_t dev)
{
	const char *	desc;
	mca_id_t	id = mca_get_id(dev);
	u_int32_t	iobase = 0;
	u_int32_t	iosize = 0;
	u_int8_t	drq = 0;
	u_int8_t	irq = 0;
	u_int8_t	pos;

	desc = mca_match_id(id, aha_mca_devs);
	if (!desc)
		return (ENXIO);
	device_set_desc(dev, desc);

	pos = mca_pos_read(dev, AHA_MCA_IOPORT_POS);
	iobase = AHA_MCA_IOPORT(pos);
	iosize = AHA_MCA_IOPORT_SIZE;

	pos = mca_pos_read(dev, AHA_MCA_DRQ_POS);
	drq = AHA_MCA_DRQ(pos);

	pos = mca_pos_read(dev, AHA_MCA_IRQ_POS);
	irq = AHA_MCA_IRQ(pos);

	mca_add_iospace(dev, iobase, iosize);
	mca_add_drq(dev, drq);
	mca_add_irq(dev, irq);

	aha_unit++;

	return (0);
}

static int
aha_mca_attach (device_t dev)
{
	struct aha_softc *	sc = NULL;
	struct resource *	io = NULL;
	struct resource *	irq = NULL;
	struct resource *	drq = NULL;
	int			error = 0;
	int			unit = device_get_unit(dev);
	int			rid;
	void *			ih;

	rid = 0;
	io = bus_alloc_resource(dev, SYS_RES_IOPORT, &rid,
				0, ~0, 1, RF_ACTIVE);
	if (!io) {
		device_printf(dev, "No I/O space?!\n");
		error = ENOMEM;
		goto bad;
	}

	rid = 0;
	irq = bus_alloc_resource(dev, SYS_RES_IRQ, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (irq == NULL) {
		device_printf(dev, "No IRQ?!\n");
		error = ENOMEM;
		goto bad;
	}

	rid = 0;
	drq = bus_alloc_resource(dev, SYS_RES_DRQ, &rid,
				 0, ~0, 1, RF_ACTIVE);
	if (drq == NULL) {
		device_printf(dev, "No DRQ?!\n");
		error = ENOMEM;
		goto bad;
	}

	sc = aha_alloc(unit, rman_get_bustag(io), rman_get_bushandle(io));
	if (sc == NULL) {
		device_printf(dev, "aha_alloc() failed!\n");
		error = ENOMEM;
		goto bad;
	}

	error = aha_probe(sc);
	if (error) {
		device_printf(dev, "aha_probe() failed!\n");
		goto bad;
	}

	error = aha_fetch_adapter_info(sc);
	if (error) {
		device_printf(dev, "aha_fetch_adapter_info() failed!\n");
		goto bad;
	}

	isa_dmacascade(rman_get_start(drq));

	error = bus_dma_tag_create(/* parent	*/	NULL,
				   /* alignemnt	*/	1,
				   /* boundary	*/	0,
				   			BUS_SPACE_MAXADDR_24BIT,
				   /* highaddr	*/	BUS_SPACE_MAXADDR,
				   			NULL,
							NULL,
				   /* maxsize	*/	BUS_SPACE_MAXSIZE_24BIT,
				   /* nsegments	*/	BUS_SPACE_UNRESTRICTED,
				   /* maxsegsz	*/	BUS_SPACE_MAXSIZE_24BIT,
				   /* flags	*/	0,
							&sc->parent_dmat);
	if (error) {
		device_printf(dev, "bus_dma_tag_create() failed!\n");
		goto bad;
	}			      

	error = aha_init(sc);
	if (error) {
		device_printf(dev, "aha_init() failed\n");
		goto bad;
	}

	error = aha_attach(sc);
	if (error) {
		device_printf(dev, "aha_attach() failed\n");
		goto bad;
	}

	error = bus_setup_intr(dev, irq, INTR_TYPE_CAM, aha_intr, sc, &ih);
	if (error) {
		device_printf(dev, "Unable to register interrupt handler\n");
		goto bad;
	}

	return (0);

bad:
	aha_free(sc);

	if (io)
		bus_release_resource(dev, SYS_RES_IOPORT, 0, io);
	if (irq)
		bus_release_resource(dev, SYS_RES_IRQ, 0, irq);
	if (drq)
		bus_release_resource(dev, SYS_RES_DRQ, 0, drq);

	return (error);
}

static device_method_t aha_mca_methods[] = {
	DEVMETHOD(device_probe,         aha_mca_probe),
	DEVMETHOD(device_attach,        aha_mca_attach),

	{ 0, 0 }
};

static driver_t aha_mca_driver = {
	"aha",
	aha_mca_methods,
	1,
/*
	sizeof(struct aha_softc *),
 */
};

static devclass_t aha_devclass;

DRIVER_MODULE(aha, mca, aha_mca_driver, aha_devclass, 0, 0);
