/*-
 * Copyright (c) 2006 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/*
 * Compact Flash Support for the Avila Gateworks XScale boards.
 * The CF slot is operated in "True IDE" mode. Registers are on
 * the Expansion Bus connected to CS1 and CS2. Interrupts are
 * tied to GPIO pin 12.  No DMA, just PIO.
 *
 * The ADI Pronghorn Metro is very similar. It use CS3 and CS4 and
 * GPIO pin 0 for interrupts.
 *
 * See also http://www.intel.com/design/network/applnots/302456.htm.
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/time.h>
#include <sys/bus.h>
#include <sys/resource.h>
#include <sys/rman.h>
#include <sys/sysctl.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/resource.h>
#include <machine/intr.h>
#include <arm/xscale/ixp425/ixp425reg.h>
#include <arm/xscale/ixp425/ixp425var.h>

#include <sys/ata.h>
#include <sys/sema.h>
#include <sys/taskqueue.h>
#include <vm/uma.h>
#include <dev/ata/ata-all.h>
#include <ata_if.h>

#define	AVILA_IDE_GPIN		12		/* GPIO pin # */
#define	AVILA_IDE_IRQ		IXP425_INT_GPIO_12
#define	AVILA_IDE_CTRL		0x06		/* control register */

#define	PRONGHORN_IDE_GPIN	0	/* GPIO pin # */
#define	PRONGHORN_IDE_IRQ	IXP425_INT_GPIO_0
#define	PRONGHORN_IDE_CNTRL	0x06	/* control register */

struct ata_avila_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_exp_ioh;	/* Exp Bus config registers */
	bus_space_handle_t	sc_ioh;		/* CS1/3 data registers */
	bus_space_handle_t	sc_alt_ioh;	/* CS2/4 data registers */
	struct bus_space	sc_expbus_tag;
	struct resource		sc_ata;		/* hand-crafted for ATA */
	struct resource		sc_alt_ata;	/* hand-crafted for ATA */
	u_int32_t		sc_16bit_off;	/* EXP_TIMING_CSx_OFFSET */
	int			sc_rid;		/* rid for IRQ */
	struct resource		*sc_irq;	/* IRQ resource */
	void			*sc_ih;		/* interrupt handler */
	struct {
		void	(*cb)(void *);
		void	*arg;
	} sc_intr[1];			/* NB: 1/channel */
};

static void ata_avila_intr(void *);
bs_protos(ata);
static	void ata_bs_rm_2_s(void *, bus_space_handle_t, bus_size_t,
		u_int16_t *, bus_size_t);
static	void ata_bs_wm_2_s(void *, bus_space_handle_t, bus_size_t,
		const u_int16_t *, bus_size_t);

static int
ata_avila_probe(device_t dev)
{
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));

	/* XXX any way to check? */
	if (EXP_BUS_READ_4(sa, EXP_TIMING_CS2_OFFSET) != 0)
		device_set_desc_copy(dev, "Gateworks Avila IDE/CF Controller");
	else
		device_set_desc_copy(dev,
		    "ADI Pronghorn Metro IDE/CF Controller");
	return 0;
}

static int
ata_avila_attach(device_t dev)
{
	struct ata_avila_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));
	u_int32_t alt_t_off, ide_gpin, ide_gptype, ide_irq;

	sc->sc_dev = dev;
	/* NB: borrow from parent */
	sc->sc_iot = sa->sc_iot;
	sc->sc_exp_ioh = sa->sc_exp_ioh;
	if (EXP_BUS_READ_4(sc, EXP_TIMING_CS2_OFFSET) != 0) {
		/* Avila board */
		if (bus_space_map(sc->sc_iot, IXP425_EXP_BUS_CS1_HWBASE,
		    IXP425_EXP_BUS_CS1_SIZE, 0, &sc->sc_ioh))
			panic("%s: unable to map Expansion Bus CS1 window",
			    __func__);
		if (bus_space_map(sc->sc_iot, IXP425_EXP_BUS_CS2_HWBASE,
		    IXP425_EXP_BUS_CS2_SIZE, 0, &sc->sc_alt_ioh))
			panic("%s: unable to map Expansion Bus CS2 window",
			    __func__);
		ide_gpin = AVILA_IDE_GPIN;
		ide_gptype = GPIO_TYPE(ide_gpin, GPIO_TYPE_EDG_RISING);
		ide_irq = AVILA_IDE_IRQ;
		sc->sc_16bit_off = EXP_TIMING_CS1_OFFSET;
		alt_t_off = EXP_TIMING_CS2_OFFSET;
	} else {
		/* Pronghorn */
		if (bus_space_map(sc->sc_iot, IXP425_EXP_BUS_CS3_HWBASE,
		    IXP425_EXP_BUS_CS3_SIZE, 0, &sc->sc_ioh))
			panic("%s: unable to map Expansion Bus CS3 window",
			    __func__);
		if (bus_space_map(sc->sc_iot, IXP425_EXP_BUS_CS4_HWBASE,
		    IXP425_EXP_BUS_CS4_SIZE, 0, &sc->sc_alt_ioh))
			panic("%s: unable to map Expansion Bus CS4 window",
			    __func__);
		ide_gpin = PRONGHORN_IDE_GPIN;
		ide_gptype = GPIO_TYPE(ide_gpin, GPIO_TYPE_ACT_HIGH);
		ide_irq = PRONGHORN_IDE_IRQ;
		sc->sc_16bit_off = EXP_TIMING_CS3_OFFSET;
		alt_t_off = EXP_TIMING_CS4_OFFSET;
	}

	/*
	 * Craft special resource for ATA bus space ops
	 * that go through the expansion bus and require
	 * special hackery to ena/dis 16-bit operations.
	 *
	 * XXX probably should just make this generic for
	 * accessing the expansion bus.
	 */
	sc->sc_expbus_tag.bs_cookie = sc;	/* NB: backpointer */
	/* read single */
	sc->sc_expbus_tag.bs_r_1	= ata_bs_r_1,
	sc->sc_expbus_tag.bs_r_2	= ata_bs_r_2,
	/* read multiple */
	sc->sc_expbus_tag.bs_rm_2	= ata_bs_rm_2,
	sc->sc_expbus_tag.bs_rm_2_s	= ata_bs_rm_2_s,
	/* write (single) */
	sc->sc_expbus_tag.bs_w_1	= ata_bs_w_1,
	sc->sc_expbus_tag.bs_w_2	= ata_bs_w_2,
	/* write multiple */
	sc->sc_expbus_tag.bs_wm_2	= ata_bs_wm_2,
	sc->sc_expbus_tag.bs_wm_2_s	= ata_bs_wm_2_s,

	rman_set_bustag(&sc->sc_ata, &sc->sc_expbus_tag);
	rman_set_bushandle(&sc->sc_ata, sc->sc_ioh);
	rman_set_bustag(&sc->sc_alt_ata, &sc->sc_expbus_tag);
	rman_set_bushandle(&sc->sc_alt_ata, sc->sc_alt_ioh);

	GPIO_CONF_WRITE_4(sa, IXP425_GPIO_GPOER, 
	    GPIO_CONF_READ_4(sa, IXP425_GPIO_GPOER) | (1<<ide_gpin));
	/* set interrupt type */
	GPIO_CONF_WRITE_4(sa, GPIO_TYPE_REG(ide_gpin),
	    (GPIO_CONF_READ_4(sa, GPIO_TYPE_REG(ide_gpin)) &~
	     GPIO_TYPE(ide_gpin, GPIO_TYPE_MASK)) | ide_gptype);

	/* clear ISR */
	GPIO_CONF_WRITE_4(sa, IXP425_GPIO_GPISR, (1<<ide_gpin));

	/* configure CS1/3 window, leaving timing unchanged */
	EXP_BUS_WRITE_4(sc, sc->sc_16bit_off,
	    EXP_BUS_READ_4(sc, sc->sc_16bit_off) |
	        EXP_BYTE_EN | EXP_WR_EN | EXP_BYTE_RD16 | EXP_CS_EN);
	/* configure CS2/4 window, leaving timing unchanged */
	EXP_BUS_WRITE_4(sc, alt_t_off,
	    EXP_BUS_READ_4(sc, alt_t_off) |
	        EXP_BYTE_EN | EXP_WR_EN | EXP_BYTE_RD16 | EXP_CS_EN);

	/* setup interrupt */
	sc->sc_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_rid,
	    ide_irq, ide_irq, 1, RF_ACTIVE);
	if (!sc->sc_irq)
		panic("Unable to allocate irq %u.\n", ide_irq);
	bus_setup_intr(dev, sc->sc_irq,
	    INTR_TYPE_BIO | INTR_MPSAFE | INTR_ENTROPY,
	    NULL, ata_avila_intr, sc, &sc->sc_ih);

	/* attach channel on this controller */
	device_add_child(dev, "ata", devclass_find_free_unit(ata_devclass, 0));
	bus_generic_attach(dev);

	return 0;
}

static int
ata_avila_detach(device_t dev)
{
	struct ata_avila_softc *sc = device_get_softc(dev);
	device_t *children;
	int nc;

	/* XXX quiesce gpio? */

	/* detach & delete all children */
	if (device_get_children(dev, &children, &nc) == 0) {
	    if (nc > 0)
		    device_delete_child(dev, children[0]);
	    free(children, M_TEMP);
	}

	bus_teardown_intr(dev, sc->sc_irq, sc->sc_ih);
	bus_release_resource(dev, SYS_RES_IRQ, sc->sc_rid, sc->sc_irq);

	return 0;
}

static void
ata_avila_intr(void *xsc)
{
	struct ata_avila_softc *sc = xsc;

	if (sc->sc_intr[0].cb != NULL)
		sc->sc_intr[0].cb(sc->sc_intr[0].arg);
}

static struct resource *
ata_avila_alloc_resource(device_t dev, device_t child, int type, int *rid,
		       u_long start, u_long end, u_long count, u_int flags)
{
	struct ata_avila_softc *sc = device_get_softc(dev);

	KASSERT(type == SYS_RES_IRQ && *rid == ATA_IRQ_RID,
	    ("type %u rid %u start %lu end %lu count %lu flags %u",
	     type, *rid, start, end, count, flags));

	/* doesn't matter what we return so reuse the real thing */
	return sc->sc_irq;
}

static int
ata_avila_release_resource(device_t dev, device_t child, int type, int rid,
			 struct resource *r)
{
	KASSERT(type == SYS_RES_IRQ && rid == ATA_IRQ_RID,
	    ("type %u rid %u", type, rid));
	return 0;
}

static int
ata_avila_setup_intr(device_t dev, device_t child, struct resource *irq, 
		   int flags, driver_filter_t *filt,
		   driver_intr_t *function, void *argument, void **cookiep)
{
	struct ata_avila_softc *sc = device_get_softc(dev);
	int unit = ((struct ata_channel *)device_get_softc(child))->unit;

	KASSERT(unit == 0, ("unit %d", unit));
	sc->sc_intr[unit].cb = function;
	sc->sc_intr[unit].arg = argument;
	*cookiep = sc;
	return 0;
}

static int
ata_avila_teardown_intr(device_t dev, device_t child, struct resource *irq,
		      void *cookie)
{
	struct ata_avila_softc *sc = device_get_softc(dev);
	int unit = ((struct ata_channel *)device_get_softc(child))->unit;

	KASSERT(unit == 0, ("unit %d", unit));
	sc->sc_intr[unit].cb = NULL;
	sc->sc_intr[unit].arg = NULL;
	return 0;
}

/*
 * Bus space accessors for CF-IDE PIO operations.
 */

/*
 * Enable/disable 16-bit ops on the expansion bus.
 */
static void __inline
enable_16(struct ata_avila_softc *sc)
{
	EXP_BUS_WRITE_4(sc, sc->sc_16bit_off,
	    EXP_BUS_READ_4(sc, sc->sc_16bit_off) &~ EXP_BYTE_EN);
	DELAY(100);		/* XXX? */
}

static void __inline
disable_16(struct ata_avila_softc *sc)
{
	DELAY(100);		/* XXX? */
	EXP_BUS_WRITE_4(sc, sc->sc_16bit_off,
	    EXP_BUS_READ_4(sc, sc->sc_16bit_off) | EXP_BYTE_EN);
}

uint8_t
ata_bs_r_1(void *t, bus_space_handle_t h, bus_size_t o)
{
	struct ata_avila_softc *sc = t;

	return bus_space_read_1(sc->sc_iot, h, o);
}

void
ata_bs_w_1(void *t, bus_space_handle_t h, bus_size_t o, u_int8_t v)
{
	struct ata_avila_softc *sc = t;

	bus_space_write_1(sc->sc_iot, h, o, v);
}

uint16_t
ata_bs_r_2(void *t, bus_space_handle_t h, bus_size_t o)
{
	struct ata_avila_softc *sc = t;
	uint16_t v;

	enable_16(sc);
	v = bus_space_read_2(sc->sc_iot, h, o);
	disable_16(sc);
	return v;
}

void
ata_bs_w_2(void *t, bus_space_handle_t h, bus_size_t o, uint16_t v)
{
	struct ata_avila_softc *sc = t;

	enable_16(sc);
	bus_space_write_2(sc->sc_iot, h, o, v);
	disable_16(sc);
}

void
ata_bs_rm_2(void *t, bus_space_handle_t h, bus_size_t o,
	u_int16_t *d, bus_size_t c)
{
	struct ata_avila_softc *sc = t;

	enable_16(sc);
	bus_space_read_multi_2(sc->sc_iot, h, o, d, c);
	disable_16(sc);
}

void
ata_bs_wm_2(void *t, bus_space_handle_t h, bus_size_t o,
	const u_int16_t *d, bus_size_t c)
{
	struct ata_avila_softc *sc = t;

	enable_16(sc);
	bus_space_write_multi_2(sc->sc_iot, h, o, d, c);
	disable_16(sc);
}

/* XXX workaround ata driver by (incorrectly) byte swapping stream cases */

void
ata_bs_rm_2_s(void *t, bus_space_handle_t h, bus_size_t o,
	u_int16_t *d, bus_size_t c)
{
	struct ata_avila_softc *sc = t;
	uint16_t v;
	bus_size_t i;

	enable_16(sc);
#if 1
	for (i = 0; i < c; i++) {
		v = bus_space_read_2(sc->sc_iot, h, o);
		d[i] = bswap16(v);
	}
#else
	bus_space_read_multi_stream_2(sc->sc_iot, h, o, d, c);
#endif
	disable_16(sc);
}

void
ata_bs_wm_2_s(void *t, bus_space_handle_t h, bus_size_t o,
	const u_int16_t *d, bus_size_t c)
{
	struct ata_avila_softc *sc = t;
	bus_size_t i;

	enable_16(sc);
#if 1
	for (i = 0; i < c; i++)
		bus_space_write_2(sc->sc_iot, h, o, bswap16(d[i]));
#else
	bus_space_write_multi_stream_2(sc->sc_iot, h, o, d, c);
#endif
	disable_16(sc);
}

static device_method_t ata_avila_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,             ata_avila_probe),
	DEVMETHOD(device_attach,            ata_avila_attach),
	DEVMETHOD(device_detach,            ata_avila_detach),
	DEVMETHOD(device_shutdown,          bus_generic_shutdown),
	DEVMETHOD(device_suspend,           bus_generic_suspend),
	DEVMETHOD(device_resume,            bus_generic_resume),

	/* bus methods */
	DEVMETHOD(bus_alloc_resource,       ata_avila_alloc_resource),
	DEVMETHOD(bus_release_resource,     ata_avila_release_resource),
	DEVMETHOD(bus_activate_resource,    bus_generic_activate_resource),
	DEVMETHOD(bus_deactivate_resource,  bus_generic_deactivate_resource),
	DEVMETHOD(bus_setup_intr,           ata_avila_setup_intr),
	DEVMETHOD(bus_teardown_intr,        ata_avila_teardown_intr),

	{ 0, 0 }
};

devclass_t ata_avila_devclass;

static driver_t ata_avila_driver = {
	"ata_avila",
	ata_avila_methods,
	sizeof(struct ata_avila_softc),
};

DRIVER_MODULE(ata_avila, ixp, ata_avila_driver, ata_avila_devclass, 0, 0);
MODULE_VERSION(ata_avila, 1);
MODULE_DEPEND(ata_avila, ata, 1, 1, 1);

static int
avila_channel_probe(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);

	ch->unit = 0;
	ch->flags |= ATA_USE_16BIT | ATA_NO_SLAVE;
	device_set_desc_copy(dev, "ATA channel 0");

	return ata_probe(dev);
}

static int
avila_channel_attach(device_t dev)
{
	struct ata_avila_softc *sc = device_get_softc(device_get_parent(dev));
	struct ata_channel *ch = device_get_softc(dev);
	int i;

	for (i = 0; i < ATA_MAX_RES; i++)
		ch->r_io[i].res = &sc->sc_ata;

	ch->r_io[ATA_DATA].offset = ATA_DATA;
	ch->r_io[ATA_FEATURE].offset = ATA_FEATURE;
	ch->r_io[ATA_COUNT].offset = ATA_COUNT;
	ch->r_io[ATA_SECTOR].offset = ATA_SECTOR;
	ch->r_io[ATA_CYL_LSB].offset = ATA_CYL_LSB;
	ch->r_io[ATA_CYL_MSB].offset = ATA_CYL_MSB;
	ch->r_io[ATA_DRIVE].offset = ATA_DRIVE;
	ch->r_io[ATA_COMMAND].offset = ATA_COMMAND;
	ch->r_io[ATA_ERROR].offset = ATA_FEATURE;
	/* NB: should be used only for ATAPI devices */
	ch->r_io[ATA_IREASON].offset = ATA_COUNT;
	ch->r_io[ATA_STATUS].offset = ATA_COMMAND;

	/* NB: the control and alt status registers are special */
	ch->r_io[ATA_ALTSTAT].res = &sc->sc_alt_ata;
	ch->r_io[ATA_ALTSTAT].offset = AVILA_IDE_CTRL;
	ch->r_io[ATA_CONTROL].res = &sc->sc_alt_ata;
	ch->r_io[ATA_CONTROL].offset = AVILA_IDE_CTRL;

	/* NB: by convention this points at the base of registers */
	ch->r_io[ATA_IDX_ADDR].offset = 0;

	ata_generic_hw(dev);
	return ata_attach(dev);
}

static device_method_t avila_channel_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,     avila_channel_probe),
	DEVMETHOD(device_attach,    avila_channel_attach),
	DEVMETHOD(device_detach,    ata_detach),
	DEVMETHOD(device_shutdown,  bus_generic_shutdown),
	DEVMETHOD(device_suspend,   ata_suspend),
	DEVMETHOD(device_resume,    ata_resume),

	{ 0, 0 }
};

driver_t avila_channel_driver = {
	"ata",
	avila_channel_methods,
	sizeof(struct ata_channel),
};
DRIVER_MODULE(ata, ata_avila, avila_channel_driver, ata_devclass, 0, 0);
