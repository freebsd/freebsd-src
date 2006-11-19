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
 * There are 1 or 2 optional CF slots operated in "True IDE" mode.
 * Registers are on the Expansion Bus connected to CS1.  Interrupts
 * are tied to GPIO pin 12.  No DMA, just PIO.
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

#define	AVILA_IDE_GPIN	12		/* GPIO pin # */
#define	AVILA_IDE_IRQ	IXP425_INT_GPIO_12
#define	AVILA_IDE_CTRL	0x1e		/* control register */

struct ata_avila_softc {
	device_t		sc_dev;
	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_exp_ioh;	/* Exp Bus config registers */
	bus_space_handle_t	sc_ioh;		/* CS1 data registers */
	struct bus_space	sc_expbus_tag;
	struct resource		sc_ata;		/* hand-crafted for ATA */
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
	/* XXX any way to check? */
	device_set_desc_copy(dev, "Gateworks Avila IDE/CF Controller");
	return 0;
}

static int
ata_avila_attach(device_t dev)
{
	struct ata_avila_softc *sc = device_get_softc(dev);
	struct ixp425_softc *sa = device_get_softc(device_get_parent(dev));

	sc->sc_dev = dev;
	/* NB: borrow from parent */
	sc->sc_iot = sa->sc_iot;
	sc->sc_exp_ioh = sa->sc_exp_ioh;
	if (bus_space_map(sc->sc_iot,
	    IXP425_EXP_BUS_CS1_HWBASE, IXP425_EXP_BUS_CS1_SIZE, 0, &sc->sc_ioh))
		panic("%s: unable to map Expansion Bus CS1 window", __func__);

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

	GPIO_CONF_WRITE_4(sa, IXP425_GPIO_GPOER, 
	    GPIO_CONF_READ_4(sa, IXP425_GPIO_GPOER) | (1<<AVILA_IDE_GPIN));
	/* interrupt is active low */
	GPIO_CONF_WRITE_4(sa, GPIO_TYPE_REG(AVILA_IDE_GPIN),
	    GPIO_CONF_READ_4(sa, GPIO_TYPE_REG(AVILA_IDE_GPIN) |
	    GPIO_TYPE(AVILA_IDE_GPIN, GPIO_TYPE_ACT_LOW)));

	/* clear ISR */
	GPIO_CONF_WRITE_4(sa, IXP425_GPIO_GPISR, (1<<AVILA_IDE_GPIN));

	/* configure CS1 window, leaving timing unchanged */
	EXP_BUS_WRITE_4(sc, EXP_TIMING_CS1_OFFSET,
	    EXP_BUS_READ_4(sc, EXP_TIMING_CS1_OFFSET) |
	        EXP_BYTE_EN | EXP_WR_EN | EXP_BYTE_RD16 | EXP_CS_EN);

	/* setup interrupt */
	sc->sc_irq = bus_alloc_resource(dev, SYS_RES_IRQ, &sc->sc_rid,
	    AVILA_IDE_IRQ, AVILA_IDE_IRQ, 1, RF_ACTIVE);
	if (!sc->sc_irq)
		panic("Unable to allocate irq %u.\n", AVILA_IDE_IRQ);
	bus_setup_intr(dev, sc->sc_irq,
	    INTR_TYPE_BIO | INTR_MPSAFE | INTR_ENTROPY,
	    ata_avila_intr, sc, &sc->sc_ih);

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
		   int flags, driver_intr_t *function, void *argument,
		   void **cookiep)
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
	EXP_BUS_WRITE_4(sc, EXP_TIMING_CS1_OFFSET,
	    EXP_BUS_READ_4(sc, EXP_TIMING_CS1_OFFSET) &~ EXP_BYTE_EN);
	DELAY(100);		/* XXX? */
}

static void __inline
disable_16(struct ata_avila_softc *sc)
{
	DELAY(100);		/* XXX? */
	EXP_BUS_WRITE_4(sc, EXP_TIMING_CS1_OFFSET,
	    EXP_BUS_READ_4(sc, EXP_TIMING_CS1_OFFSET) | EXP_BYTE_EN);
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
	/* alias this; required by ata_generic_status */
	ch->r_io[ATA_ALTSTAT].offset = ch->r_io[ATA_STATUS].offset;

	/* NB: the control register is special */
	ch->r_io[ATA_CONTROL].offset = AVILA_IDE_CTRL;

	/* NB: by convention this points at the base of registers */
	ch->r_io[ATA_IDX_ADDR].offset = 0;

	ata_generic_hw(dev);
	return ata_attach(dev);
}

/* XXX override ata_generic_reset to handle non-standard status */
static void
avila_channel_reset(device_t dev)
{
	struct ata_channel *ch = device_get_softc(dev);
	u_int8_t ostat0 = 0, stat0 = 0;
	u_int8_t err = 0, lsb = 0, msb = 0;
	int mask = 0, timeout;

	/* do we have any signs of ATA/ATAPI HW being present ? */
	ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | ATA_MASTER);
	DELAY(10);
	ostat0 = ATA_IDX_INB(ch, ATA_STATUS);
	if ((ostat0 & 0xf8) != 0xf8 && ostat0 != 0xa5) {
		stat0 = ATA_S_BUSY;
		mask |= 0x01;
	}

	if (bootverbose)
		device_printf(dev, "%s: reset tp1 mask=%02x ostat0=%02x\n",
		    __func__, mask, ostat0);

	/* if nothing showed up there is no need to get any further */
	/* XXX SOS is that too strong?, we just might loose devices here */
	ch->devices = 0;
	if (!mask)
		return;

	/* reset (both) devices on this channel */
	ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_D_LBA | ATA_MASTER);
	DELAY(10);
	ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_IDS | ATA_A_RESET);
	ata_udelay(10000); 
	ATA_IDX_OUTB(ch, ATA_CONTROL, ATA_A_IDS);
	ata_udelay(100000);
	ATA_IDX_INB(ch, ATA_ERROR);

	/* wait for BUSY to go inactive */
	for (timeout = 0; timeout < 310; timeout++) {
		if ((mask & 0x01) && (stat0 & ATA_S_BUSY)) {
			ATA_IDX_OUTB(ch, ATA_DRIVE, ATA_D_IBM | ATA_MASTER);
			DELAY(10);
			err = ATA_IDX_INB(ch, ATA_ERROR);
			lsb = ATA_IDX_INB(ch, ATA_CYL_LSB);
			msb = ATA_IDX_INB(ch, ATA_CYL_MSB);
			stat0 = ATA_IDX_INB(ch, ATA_STATUS);
			if (bootverbose)
				device_printf(dev,
				    "%s: stat0=0x%02x err=0x%02x lsb=0x%02x "
				    "msb=0x%02x\n", __func__,
				    stat0, err, lsb, msb);
			if (stat0 == err && lsb == err && msb == err &&
			    timeout > (stat0 & ATA_S_BUSY ? 100 : 10))
				mask &= ~0x01;
			if (!(stat0 & ATA_S_BUSY)) {
				if ((err & 0x7f) == ATA_E_ILI || err == 0) {
					if (lsb == ATAPI_MAGIC_LSB &&
					    msb == ATAPI_MAGIC_MSB) {
						ch->devices |= ATA_ATAPI_MASTER;
					} else if (stat0 & ATA_S_READY) {
						ch->devices |= ATA_ATA_MASTER;
					}
				} else if ((stat0 & 0x0f) &&
				    err == lsb && err == msb) {
					stat0 |= ATA_S_BUSY;
				}
			}
		}
		if (mask == 0x00)       /* nothing to wait for */
			break;
		/* wait for master */
		if (!(stat0 & ATA_S_BUSY) || (stat0 == 0xff && timeout > 10))
			break;
		ata_udelay(100000);
	}

	if (bootverbose)
		device_printf(dev, "%s: reset tp2 stat0=%02x devices=0x%b\n",
		    __func__, stat0, ch->devices,
		    "\20\4ATAPI_SLAVE\3ATAPI_MASTER\2ATA_SLAVE\1ATA_MASTER");
}

static device_method_t avila_channel_methods[] = {
	/* device interface */
	DEVMETHOD(device_probe,     avila_channel_probe),
	DEVMETHOD(device_attach,    avila_channel_attach),
	DEVMETHOD(device_detach,    ata_detach),
	DEVMETHOD(device_shutdown,  bus_generic_shutdown),
	DEVMETHOD(device_suspend,   ata_suspend),
	DEVMETHOD(device_resume,    ata_resume),

	DEVMETHOD(ata_reset,	    avila_channel_reset),

	{ 0, 0 }
};

driver_t avila_channel_driver = {
	"ata",
	avila_channel_methods,
	sizeof(struct ata_channel),
};
DRIVER_MODULE(ata, ata_avila, avila_channel_driver, ata_devclass, 0, 0);
