/* $FreeBSD$ */
/*
 * Copyright (c) 2000 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Autoconfiguration and support routines for the main backplane bus
 * for Rawhide (Alpha 4100) systems.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/rpb.h>
#include <machine/cpuconf.h>

#include <alpha/mcbus/mcbusreg.h>
#include <alpha/mcbus/mcbusvar.h>

struct mcbus_device *mcbus_primary_cpu = NULL;

#define KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))
#define	NO_MCPCIA_AT(mid, gid) \
	(badaddr((void *)KV(MCPCIA_BRIDGE_ADDR(gid, mid)), sizeof (u_int32_t)))

struct mcbus_softc {
	device_t	mcbus_dev;
	driver_intr_t * sub_intr;
	u_int8_t	mcbus_types[MCBUS_MID_MAX];
};

static void mcbus_add_child(struct mcbus_softc *, struct mcbus_device *);
static void mcbus_intr(void *, u_long);

static struct mcbus_softc *	mcbus0_softc = NULL;
static devclass_t		mcbus_devclass;

/*
 * Device methods
 */
static int mcbus_probe(device_t);
static int mcbus_print_child(device_t, device_t);
static int mcbus_read_ivar(device_t, device_t, int, u_long *);
static int mcbus_setup_intr(device_t, device_t, struct resource *, int,
    driver_intr_t *, void *, void **);
static int
mcbus_teardown_intr(device_t, device_t, struct resource *, void *);

static device_method_t mcbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		mcbus_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	mcbus_print_child),
	DEVMETHOD(bus_read_ivar,	mcbus_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_setup_intr,	mcbus_setup_intr),
	DEVMETHOD(bus_teardown_intr,	mcbus_teardown_intr),

	{ 0, 0 }
};

static driver_t mcbus_driver = {
	"mcbus", mcbus_methods, sizeof (struct mcbus_softc),
};

/*
 * Tru64 UNIX (formerly Digital UNIX (formerly DEC OSF/1)) probes for MCPCIAs
 * in the following order:
 *
 *	5, 4, 7, 6
 *
 * This is so that the built-in CD-ROM on the internal 53c810 is always
 * dka500.  We probe them in the same order, for consistency.
 */
static const int mcbus_mcpcia_probe_order[] = { 5, 4, 7, 6 };

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
mcbus_probe(device_t dev)
{
	struct mcbus_softc *sc = device_get_softc(dev);
	struct mcbus_device *mdev;
	int i, mid, gid;

	device_set_desc(dev, "MCBUS Backplane Bus");

	/*
	 * XXX A note about GIDs...
	 * XXX If we ever support more than one MCBUS, we'll
	 * XXX have to probe for them, and map them to unit
	 * XXX numbers.
	 */

	sc->mcbus_dev = dev;
	mcbus0_softc = sc;
	set_iointr(mcbus_intr);
	gid = MCBUS_GID_FROM_INSTANCE(0);

	mcbus0_softc->mcbus_types[0] = MCBUS_TYPE_RES;
	for (mid = 1; mid <= MCBUS_MID_MAX; mid++) {
		mcbus0_softc->mcbus_types[mid] = MCBUS_TYPE_UNK;
	}

	/*
	 * First, add 'memory' children to probe.
	 */
	mdev = (struct mcbus_device *)
	    malloc(sizeof (struct mcbus_device), M_DEVBUF, M_NOWAIT);
	if (!mdev) {
		printf("mcbus_probe: unable to malloc softc for memory dev\n");
		return (ENOMEM);
	}
	mdev->ma_gid = gid;
	mdev->ma_mid = 1;
	mdev->ma_order = MCPCIA_PER_MCBUS;
	mdev->ma_type = MCBUS_TYPE_MEM;
	mcbus0_softc->mcbus_types[1] = MCBUS_TYPE_MEM;
	mcbus_add_child(sc, mdev);

	/*
	 * Now add I/O (MCPCIA) modules to probe (but only if they're there).
	 */
	for (i = 0; i < MCPCIA_PER_MCBUS; ++i) {
		mid = mcbus_mcpcia_probe_order[i];

		if (NO_MCPCIA_AT(mid, gid)) {
			continue;
		}
		mdev = (struct mcbus_device *)
		    malloc(sizeof (struct mcbus_device), M_DEVBUF, M_NOWAIT);
		if (!mdev) {
			printf("mcbus_probe: unable to malloc for MCPCIA\n");
			continue;
		}
		mdev->ma_gid = gid;
		mdev->ma_mid = mid;
		mdev->ma_order = i;
		mdev->ma_type = MCBUS_TYPE_PCI;
		mcbus0_softc->mcbus_types[1] = MCBUS_TYPE_PCI;
		mcbus_add_child(sc, mdev);
	}

	/*
	 * XXX: ToDo Add CPU nodes.
	 */

	return (0);
}

static int
mcbus_print_child(device_t dev, device_t child)
{
	struct mcbus_device *mdev = DEVTOMCBUS(child);
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(" at %s gid %d mid %d\n",
	    device_get_nameunit(dev), mdev->ma_gid, mdev->ma_mid);
	return (retval);
}

static int
mcbus_read_ivar(device_t dev, device_t child, int index, u_long *result)
{
	struct mcbus_device *mdev = DEVTOMCBUS(child);

	switch (index) {
	case MCBUS_IVAR_MID:
		*result = mdev->ma_mid;
		break;

	case MCBUS_IVAR_GID:
		*result = mdev->ma_gid;
		break;

	case MCBUS_IVAR_TYPE:
		*result = mdev->ma_type;
		break;

	}
	return (ENOENT);
}

static int
mcbus_setup_intr(device_t dev, device_t child, struct resource *r, int f,
    driver_intr_t *intr, void *a, void **ac)
{
	if (strncmp(device_get_name(child), "pcib", 6) == 0) {
		if (mcbus0_softc->sub_intr == NULL)
			mcbus0_softc->sub_intr = intr;
		return (0);
	} else {
		return (ENXIO);
	}
}

static int
mcbus_teardown_intr(device_t dev, device_t child, struct resource *i, void *c)
{
	if (strncmp(device_get_name(child), "pcib", 6) == 0) {
		mcbus0_softc->sub_intr = NULL;
		return (0);
	} else {
		return (ENXIO);
	}
}

static void
mcbus_intr(void *frame, u_long vector)
{
	if (vector && mcbus0_softc->sub_intr)
		(*mcbus0_softc->sub_intr)((void *)vector);
}

static void
mcbus_add_child(struct mcbus_softc *mcbus, struct mcbus_device *mdev)
{
	static int mcpciaproto, memproto, cpuproto;
	device_t cd;
	int un;
	char *dn, *ds;

	switch (mdev->ma_type) {
	case MCBUS_TYPE_PCI:
		dn = "pcib";
		ds = "MCPCIA PCI Bus Bridge";
		un = mcpciaproto++;
		break;
	case MCBUS_TYPE_MEM:
		dn = "mcmem";
		un = memproto++;
		ds = "MCBUS Memory Module";
		break;
	case MCBUS_TYPE_CPU:
		dn = "mccpu";
		un = cpuproto++;
		ds = "MCBUS Processor Module";
		break;
	default:
		printf("mcbus_add_child: unknown MCBUS type 0x%x\n",
		    mdev->ma_type);
		return;
	}

	cd = device_add_child_ordered(mcbus->mcbus_dev, mdev->ma_type, dn, un);
	if (cd == NULL) {
		return;
	}
	device_set_ivars(cd, mdev);
	device_set_desc(cd, ds);
}
DRIVER_MODULE(mcbus, root, mcbus_driver, mcbus_devclass, 0, 0);
