/*
 * Copyright 2003 by Peter Grehan. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/*
 *  A driver for the PIC found in the Heathrow/Paddington MacIO chips.
 * This was superseded by an OpenPIC in the Keylargo and beyond 
 * MacIO versions.
 *
 *  The device is initially located in the OpenFirmware device tree
 * in the earliest stage of the nexus probe. However, no device registers
 * are touched until the actual h/w is probed later on during the
 * MacIO probe. At that point, any interrupt sources that were allocated 
 * prior to this are activated.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/kernel.h>

#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/intr.h>
#include <machine/intr_machdep.h>
#include <machine/md_var.h>
#include <machine/nexusvar.h>
#include <machine/pio.h>
#include <machine/resource.h>

#include <vm/vm.h>
#include <vm/pmap.h>

#include <sys/rman.h>

#include <powerpc/powermac/maciovar.h>
#include <powerpc/powermac/hrowpicvar.h>

#include "pic_if.h"

/*
 * Device interface.
 */
static int		hrowpic_probe(device_t);
static int		hrowpic_attach(device_t);

/*
 * PIC interface.
 */
static struct resource	*hrowpic_allocate_intr(device_t, device_t, int *,
                            u_long, u_int);
static int		hrowpic_setup_intr(device_t, device_t,
                            struct resource *, int, driver_intr_t, void *,
                            void **);
static int		hrowpic_teardown_intr(device_t, device_t,
                            struct resource *, void *);
static int		hrowpic_release_intr(device_t dev, device_t, int,
                            struct resource *res);

/*
 * MacIO interface
 */
static int	hrowpic_macio_probe(device_t);
static int	hrowpic_macio_attach(device_t);

/*
 * Local routines
 */
static void	hrowpic_intr(void);
static void	hrowpic_ext_enable_irq(uintptr_t);
static void	hrowpic_ext_disable_irq(uintptr_t);
static void	hrowpic_toggle_irq(struct hrowpic_softc *sc, int, int);

/*
 * Interrupt controller softc. There should only be one.
 */
static struct hrowpic_softc  *hpicsoftc;

/*
 * Driver methods.
 */
static device_method_t  hrowpic_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         hrowpic_probe),
	DEVMETHOD(device_attach,        hrowpic_attach),

	/* PIC interface */
	DEVMETHOD(pic_allocate_intr,    hrowpic_allocate_intr),
	DEVMETHOD(pic_setup_intr,       hrowpic_setup_intr),
	DEVMETHOD(pic_teardown_intr,    hrowpic_teardown_intr),
	DEVMETHOD(pic_release_intr,     hrowpic_release_intr),

	{ 0, 0 }
};

static driver_t hrowpic_driver = {
	"hrowpic",
	hrowpic_methods,
	sizeof(struct hrowpic_softc)
};

static devclass_t hrowpic_devclass;

DRIVER_MODULE(hrowpic, nexus, hrowpic_driver, hrowpic_devclass, 0, 0);

static int
hrowpic_probe(device_t dev)
{
	char    *type, *compatible;

	type = nexus_get_device_type(dev);
	compatible = nexus_get_compatible(dev);

	if (strcmp(type, "interrupt-controller"))
		return (ENXIO);

	if (strcmp(compatible, "heathrow")) {
		return (ENXIO);
	}

	device_set_desc(dev, "Heathrow interrupt controller");
	return (0);
}

static int
hrowpic_attach(device_t dev)
{
	struct hrowpic_softc *sc;

	sc = device_get_softc(dev);

	sc->sc_rman.rm_type = RMAN_ARRAY;
	sc->sc_rman.rm_descr = device_get_nameunit(dev);

	if (rman_init(&sc->sc_rman) != 0 ||
	    rman_manage_region(&sc->sc_rman, 0, HROWPIC_IRQMAX-1) != 0) {
		device_printf(dev, "could not set up resource management");
		return (ENXIO);
        }	

	intr_init(hrowpic_intr, HROWPIC_IRQMAX, hrowpic_ext_enable_irq, 
	    hrowpic_ext_disable_irq);

	KASSERT(hpicsoftc == NULL, ("hrowpic: h/w already probed"));
	hpicsoftc = sc;

	return (0);
}

/*
 * PIC interface
 */
static struct resource *
hrowpic_allocate_intr(device_t picdev, device_t child, int *rid, u_long intr,
    u_int flags)
{
	struct  hrowpic_softc *sc;
	struct  resource *rv;
	int     needactivate;

	sc = device_get_softc(picdev);
	needactivate = flags & RF_ACTIVE;
	flags &= ~RF_ACTIVE;

	rv = rman_reserve_resource(&sc->sc_rman, intr, intr, 1, flags, child);
	if (rv == NULL) {
		device_printf(picdev, "interrupt reservation failed for %s\n",
		    device_get_nameunit(child));
		return (NULL);
	}
	
	return (rv);
}

static int
hrowpic_setup_intr(device_t picdev, device_t child, struct resource *res,
    int flags, driver_intr_t *intr, void *arg, void **cookiep)
{
	struct  hrowpic_softc *sc;
	int error;

	sc = device_get_softc(picdev);

	if ((res->r_flags & RF_SHAREABLE) == 0)
		flags |= INTR_EXCL;

	/*
	 * We depend here on rman_activate_resource() being idempotent.
	 */
	error = rman_activate_resource(res);
	if (error)
		return (error);

	error = inthand_add(device_get_nameunit(child), res->r_start, intr,
	    arg, flags, cookiep);

	if (!error) {
		/*
		 * Record irq request, and enable if h/w has been probed
		 */
		sc->sc_irq[res->r_start] = 1;
		if (sc->sc_memr) {
			hrowpic_toggle_irq(sc, res->r_start, 1);
		}
	}

	return (error);
}

static int
hrowpic_teardown_intr(device_t picdev, device_t child, struct resource *res,
    void *ih)
{
	int     error;

	error = rman_deactivate_resource(res);
	if (error)
		return (error);

	error = inthand_remove(res->r_start, ih);

	return (error);
}

static int
hrowpic_release_intr(device_t picdev, device_t child, int rid,
    struct resource *res)
{
	int     error;

	if (rman_get_flags(res) & RF_ACTIVE) {
		error = bus_deactivate_resource(child, SYS_RES_IRQ, rid, res);
		if (error)
			return (error);
	}

	return (rman_release_resource(res));
}

/*
 * Interrupt interface
 */
static void
hrowpic_write_reg(struct hrowpic_softc *sc, u_int reg, u_int bank, 
    u_int32_t val)
{
	if (bank == HPIC_PRIMARY)
		reg += HPIC_1ST_OFFSET;

	bus_space_write_4(sc->sc_bt, sc->sc_bh, reg, val);

	/*
	 * XXX Issue a read to force the write to complete
	 */
	bus_space_read_4(sc->sc_bt, sc->sc_bh, reg);
}

static u_int32_t
hrowpic_read_reg(struct hrowpic_softc *sc, u_int reg, u_int bank)
{
	if (bank == HPIC_PRIMARY)
		reg += HPIC_1ST_OFFSET;

	return (bus_space_read_4(sc->sc_bt, sc->sc_bh, reg));
}

static void
hrowpic_clear_all(struct hrowpic_softc *sc)
{
	/*
	 * Disable all interrupt sources and clear outstanding interrupts
	 */
	hrowpic_write_reg(sc, HPIC_ENABLE, HPIC_PRIMARY, 0);
	hrowpic_write_reg(sc, HPIC_CLEAR,  HPIC_PRIMARY, 0xffffffff);
	hrowpic_write_reg(sc, HPIC_ENABLE, HPIC_SECONDARY, 0);
	hrowpic_write_reg(sc, HPIC_CLEAR,  HPIC_SECONDARY, 0xffffffff);
}

static void
hrowpic_toggle_irq(struct hrowpic_softc *sc, int irq, int enable)
{
	u_int roffset;
	u_int rbit;

	KASSERT((irq > 0) && (irq < HROWPIC_IRQMAX), ("en irq out of range"));

	/*
	 * Calculate prim/sec register bank for the IRQ, update soft copy,
	 * and enable the IRQ as an interrupt source
	 */
	roffset = HPIC_INT_TO_BANK(irq);
	rbit = HPIC_INT_TO_REGBIT(irq);

	if (enable)
		sc->sc_softreg[roffset] |= (1 << rbit);
	else
		sc->sc_softreg[roffset] &= ~(1 << rbit);
		
	hrowpic_write_reg(sc, HPIC_ENABLE, roffset, sc->sc_softreg[roffset]);
}

static void
hrowpic_intr(void)
{
	int irq_lo, irq_hi;
	int i;
	struct hrowpic_softc *sc;

	sc = hpicsoftc;

	/*
	 * Loop through both interrupt sources until they are empty.
	 * XXX simplistic code, far from optimal.
	 */
	do {
		irq_lo = hrowpic_read_reg(sc, HPIC_STATUS, HPIC_PRIMARY);
		if (irq_lo) {
			hrowpic_write_reg(sc, HPIC_CLEAR, HPIC_PRIMARY,
			    irq_lo);
			for (i = 0; i < HROWPIC_IRQ_REGNUM; i++) {
				if (irq_lo & (1 << i)) {
					/*
					 * Disable IRQ and call handler
					 */
					hrowpic_toggle_irq(sc, i, 0);
					intr_handle(i);
				}
			}

		}

		irq_hi = hrowpic_read_reg(sc, HPIC_STATUS, HPIC_SECONDARY);
		if (irq_hi) {
			hrowpic_write_reg(sc, HPIC_CLEAR, HPIC_SECONDARY,
			    irq_hi);
			for (i = 0; i < HROWPIC_IRQ_REGNUM; i++) {
				if (irq_hi & (1 << i)) {
					/*
					 * Disable IRQ and call handler
					 */
					hrowpic_toggle_irq(sc,
					    i + HROWPIC_IRQ_REGNUM, 0);
					intr_handle(i + HROWPIC_IRQ_REGNUM);
				}
			}
		}
	} while (irq_lo && irq_hi);
}

static void
hrowpic_ext_enable_irq(uintptr_t irq)
{
	hrowpic_toggle_irq(hpicsoftc, irq, 1);
}

static void
hrowpic_ext_disable_irq(uintptr_t irq)
{
	hrowpic_toggle_irq(hpicsoftc, irq, 0);
}


/*
 * MacIO interface
 */

static device_method_t  hrowpic_macio_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,         hrowpic_macio_probe),
	DEVMETHOD(device_attach,        hrowpic_macio_attach),

	{ 0, 0 },
};

static driver_t hrowpic_macio_driver = {
	"hrowpicmacio",
	hrowpic_macio_methods,
	0
};

static devclass_t hrowpic_macio_devclass;

DRIVER_MODULE(hrowpicmacio, macio, hrowpic_macio_driver, 
    hrowpic_macio_devclass, 0, 0);

static int
hrowpic_macio_probe(device_t dev)
{
        char *type = macio_get_devtype(dev);

	/*
	 * OpenPIC cells have a type of "open-pic", so this
	 * is sufficient to identify a Heathrow cell
	 */
        if (strcmp(type, "interrupt-controller") != 0)
                return (ENXIO);

	/*
	 * The description was already printed out in the nexus
	 * probe, so don't do it again here
	 */
        device_set_desc(dev, "Heathrow MacIO interrupt cell");
	device_quiet(dev);
        return (0);
}

static int
hrowpic_macio_attach(device_t dev)
{
	struct hrowpic_softc *sc = hpicsoftc;
	int rid;
	int i;

	KASSERT(sc != NULL, ("pic not nexus-probed\n"));
	sc->sc_maciodev = dev;

	rid = 0;
	sc->sc_memr = bus_alloc_resource(dev, SYS_RES_MEMORY, &rid, 0, ~0, 1,
	    RF_ACTIVE);

	if (sc->sc_memr == NULL) {
		device_printf(dev, "Could not alloc mem resource!\n");
		return (ENXIO);
	}

	sc->sc_bt = rman_get_bustag(sc->sc_memr);
	sc->sc_bh = rman_get_bushandle(sc->sc_memr);

	hrowpic_clear_all(sc);

	/*
	 * Enable all IRQs that were requested before the h/w
	 * was probed
	 */
	for (i = 0; i < HROWPIC_IRQMAX; i++)
		if (sc->sc_irq[i]) {
			hrowpic_toggle_irq(sc, i, 1);
		}

	return (0);
}
