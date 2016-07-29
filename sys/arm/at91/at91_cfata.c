/*-
 * Copyright (c) 2008 Deglitch Networks, Stanislav Sedov
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
 *
 * Driver for the AT91RM9200 CompactFlash controller operating in a
 * common memory mode. Interrupts are driven by polling. The driver
 * implements an ATA bridge and attached ATA channel driver on top
 * of it.
 * NOTE WELL: this driver uses polling mode. To achieve an acceptable
 * operating speed you will probably want to use HZ=2000 in kernel
 * config.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>

#include <sys/ata.h>
#include <dev/ata/ata-all.h>
#include <ata_if.h>

struct at91_cfata_softc {
	device_t		dev;
	struct resource		*mem_res;
	struct resource		irq;
	void			(*isr_cb)(void *);
	void			*isr_arg;
	struct callout		tick;
};

static int	at91_cfata_detach(device_t dev);
static void	at91_cfata_callout(void *arg);

static int
at91_cfata_probe(device_t dev)
{

	device_set_desc_copy(dev, "AT91RM9200 CompactFlash controller");
	return (0);
}

static int
at91_cfata_attach(device_t dev)
{
	struct at91_cfata_softc *sc;
	int rid, error;

	sc = device_get_softc(dev);
	sc->dev = dev;
	rid = 0;
	error = 0;
	sc->mem_res = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid,
	    RF_ACTIVE);
	if (sc->mem_res == NULL)
		return (ENOMEM);

	/* XXX: init CF controller? */

	callout_init(&sc->tick, 1);	/* Callout to poll the device. */
	device_add_child(dev, "ata", -1);
	bus_generic_attach(dev);
	return (0);
}

static int
at91_cfata_detach(device_t dev)
{
	struct at91_cfata_softc *sc;

	sc = device_get_softc(dev);
	bus_generic_detach(sc->dev);
	if (sc->mem_res != NULL) {
		bus_release_resource(dev, SYS_RES_MEMORY,
		    rman_get_rid(sc->mem_res), sc->mem_res);
		sc->mem_res = NULL;
	}
	return (0);
}

static struct resource *
ata_at91_alloc_resource(device_t dev, device_t child, int type, int *rid,
    u_long start, u_long end, u_long count, u_int flags)
{
	struct at91_cfata_softc *sc = device_get_softc(dev);

	KASSERT(type == SYS_RES_IRQ && *rid == ATA_IRQ_RID,
	    ("[at91_cfata, %d]: illegal resource request (type %u rid %u)",
	    __LINE__, type, *rid));
	return (&sc->irq);
}

static int
ata_at91_release_resource(device_t dev, device_t child, int type, int rid,
    struct resource *r)
{

	KASSERT(type == SYS_RES_IRQ && rid == ATA_IRQ_RID,
	    ("[at91_cfata, %d]: illegal resource request (type %u rid %u)",
	    __LINE__, type, rid));
	return (0);
}


static int
ata_at91_setup_intr(device_t dev, device_t child, struct resource *irq,
    int flags, driver_filter_t *filt,
    driver_intr_t *function, void *argument, void **cookiep)
{
	struct at91_cfata_softc *sc = device_get_softc(dev);

	KASSERT(sc->isr_cb == NULL,
	    ("[at91_cfata, %d]: overwriting the old handler", __LINE__));
	sc->isr_cb = function;
	sc->isr_arg = argument;
	*cookiep = sc;
	callout_reset(&sc->tick, 1, at91_cfata_callout, sc);
	return (0);
}

static int
ata_at91_teardown_intr(device_t dev, device_t child, struct resource *irq,
    void *cookie)
{
	struct at91_cfata_softc *sc = device_get_softc(dev);

	sc->isr_cb = NULL;
	sc->isr_arg = NULL;
	return (0);
}

static void
at91_cfata_callout(void *arg)
{
	struct at91_cfata_softc *sc;

	sc = (struct at91_cfata_softc *)arg;
	if (sc->isr_cb != NULL)
		sc->isr_cb(sc->isr_arg);
	callout_reset(&sc->tick, 1, at91_cfata_callout, sc);
}

static int
at91_channel_probe(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);

	ch->unit = 0;
	ch->flags |= ATA_USE_16BIT | ATA_NO_SLAVE;
	device_set_desc_copy(dev, "ATA channel 0");

	return (ata_probe(dev));
}

static int
at91_channel_attach(device_t dev)
{
	struct at91_cfata_softc *sc = device_get_softc(device_get_parent(dev));
	struct ata_channel *ch = device_get_softc(dev);
	int i;

	for (i = 0; i < ATA_MAX_RES; i++)
		ch->r_io[i].res = sc->mem_res;

	/*
	 * CF+ Specification.
	 * 6.1.3 Memory Mapped Addressing.
	 */
	ch->r_io[ATA_DATA].offset = 0x00;
	ch->r_io[ATA_FEATURE].offset = 0x01;
	ch->r_io[ATA_COUNT].offset = 0x02;
	ch->r_io[ATA_SECTOR].offset = 0x03;
	ch->r_io[ATA_CYL_LSB].offset = 0x04;
	ch->r_io[ATA_CYL_MSB].offset = 0x05;
	ch->r_io[ATA_DRIVE].offset = 0x06;
	ch->r_io[ATA_COMMAND].offset = 0x07;
	ch->r_io[ATA_ERROR].offset = 0x01;
	ch->r_io[ATA_IREASON].offset = 0x02;
	ch->r_io[ATA_STATUS].offset = 0x07;
	ch->r_io[ATA_ALTSTAT].offset = 0x0e;
	ch->r_io[ATA_CONTROL].offset = 0x0e;

	/* Should point at the base of registers. */
	ch->r_io[ATA_IDX_ADDR].offset = 0x0;

	ata_generic_hw(dev);
	return (ata_attach(dev));
}

static device_method_t at91_cfata_methods[] = {
	/* Device interface. */
	DEVMETHOD(device_probe,			at91_cfata_probe),
	DEVMETHOD(device_attach,		at91_cfata_attach),
	DEVMETHOD(device_detach,		at91_cfata_detach),
	DEVMETHOD(device_shutdown,		bus_generic_shutdown),
	DEVMETHOD(device_suspend,		bus_generic_suspend),
	DEVMETHOD(device_resume,		bus_generic_resume),

	/* ATA bus methods. */
	DEVMETHOD(bus_alloc_resource,		ata_at91_alloc_resource),
	DEVMETHOD(bus_release_resource,		ata_at91_release_resource),
	DEVMETHOD(bus_activate_resource,	bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,
	    bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,		ata_at91_setup_intr),
	DEVMETHOD(bus_teardown_intr,		ata_at91_teardown_intr),

	{ 0, 0 }
};

devclass_t at91_cfata_devclass;

static driver_t at91_cfata_driver = {
	"at91_cfata",
	at91_cfata_methods,
	sizeof(struct at91_cfata_softc),
};

DRIVER_MODULE(at91_cfata, atmelarm, at91_cfata_driver, at91_cfata_devclass, 0,
    0);
MODULE_VERSION(at91_cfata, 1);
MODULE_DEPEND(at91_cfata, ata, 1, 1, 1);

/*
 * ATA channel driver.
 */
static device_method_t at91_channel_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,		at91_channel_probe),
	DEVMETHOD(device_attach,	at91_channel_attach),
	DEVMETHOD(device_detach,	ata_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),
	DEVMETHOD(device_suspend,	ata_suspend),
	DEVMETHOD(device_resume,	ata_resume),

	{ 0, 0 }
};

driver_t at91_channel_driver = {
	"ata",
	at91_channel_methods,
	sizeof(struct ata_channel),
};

DRIVER_MODULE(ata, at91_cfata, at91_channel_driver, ata_devclass, 0, 0);
