/* $FreeBSD$ */

/*
 * Copyright (c) 1997, 2000 by Matthew Jacob
 * NASA AMES Research Center.
 * All rights reserved.
 *
 * Based in part upon a prototype version by Jason Thorpe
 * Copyright (c) 1996 by Jason Thorpe.
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
 * Autoconfiguration and support routines for the TurboLaser System Bus
 * found on AlphaServer 8200 and 8400 systems.
 */

#include "opt_simos.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/malloc.h>

#include <machine/rpb.h>
#include <machine/cpuconf.h>

#include <alpha/tlsb/tlsbreg.h>
#include <alpha/tlsb/tlsbvar.h>

struct tlsb_device *tlsb_primary_cpu = NULL;

#define KV(_addr)	((caddr_t)ALPHA_PHYS_TO_K0SEG((_addr)))

struct tlsb_softc {
	driver_intr_t * zsc_intr;
	void *		zsc_arg;
	driver_intr_t * sub_intr;
	device_t	tlsb_dev;
	int		tlsb_map;
};

static void tlsb_add_child(struct tlsb_softc *, struct tlsb_device *);
static char *tlsb_node_type_str(u_int32_t);
static void tlsb_intr(void *, u_long);

static struct tlsb_softc *	tlsb0_softc = NULL;
static devclass_t		tlsb_devclass;

/*
 * Device methods
 */
static int tlsb_probe(device_t);
static int tlsb_print_child(device_t, device_t);
static int tlsb_read_ivar(device_t, device_t, int, u_long *);
static int tlsb_setup_intr(device_t, device_t, struct resource *, int,
    driver_intr_t *, void *, void **);
static int
tlsb_teardown_intr(device_t, device_t, struct resource *, void *);

static device_method_t tlsb_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		tlsb_probe),
	DEVMETHOD(device_attach,	bus_generic_attach),
	DEVMETHOD(device_detach,	bus_generic_detach),
	DEVMETHOD(device_shutdown,	bus_generic_shutdown),

	/* Bus interface */
	DEVMETHOD(bus_print_child,	tlsb_print_child),
	DEVMETHOD(bus_read_ivar,	tlsb_read_ivar),
	DEVMETHOD(bus_write_ivar,	bus_generic_write_ivar),
	DEVMETHOD(bus_setup_intr,	tlsb_setup_intr),
	DEVMETHOD(bus_teardown_intr,	tlsb_teardown_intr),

	{ 0, 0 }
};

static driver_t tlsb_driver = {
	"tlsb", tlsb_methods, sizeof (struct tlsb_softc),
};

/*
 * At 'probe' time, we add all the devices which we know about to the
 * bus.  The generic attach routine will probe and attach them if they
 * are alive.
 */
static int
tlsb_probe(device_t dev)
{
	struct tlsb_softc *sc = device_get_softc(dev);
	struct tlsb_device *tdev;
	u_int32_t tldev;
	int node;

	device_set_desc(dev, "TurboLaser Backplane Bus");

	sc->tlsb_dev = dev;
	tlsb0_softc = sc;
	set_iointr(tlsb_intr);

	/*
	 * Attempt to find all devices on the bus, including
	 * CPUs, memory modules, and I/O modules.
	 */

	for (node = 0; node <= TLSB_NODE_MAX; ++node) {
		/*
		 * Check for invalid address.
		 */
#ifdef SIMOS
		if (node != 0 && node != 8) {
			continue;
		} else if (node == 0) {
			tldev = TLDEV_DTYPE_SCPU4;
		} else {
			tldev = TLDEV_DTYPE_KFTIA;
		}
#else
		if (badaddr(TLSB_NODE_REG_ADDR(node, TLDEV), sizeof(u_int32_t)))
			continue;
		tldev = TLSB_GET_NODEREG(node, TLDEV);
#endif
		if (tldev == 0) {
			/* Nothing at this node. */
			continue;
		}
		tdev = (struct tlsb_device *)
		    malloc(sizeof (struct tlsb_device), M_DEVBUF, M_NOWAIT);

		if (!tdev) {
			printf("tlsb_probe: unable to malloc softc\n");
			continue;
		}

		sc->tlsb_map |= (1 << node);
		tdev->td_node = node;
		tdev->td_tldev = tldev;
		tlsb_add_child(sc, tdev);
	}
	return (0);
}

static int
tlsb_print_child(device_t dev, device_t child)
{
	struct tlsb_device* tdev = DEVTOTLSB(child);
	int retval = 0;

	retval += bus_print_child_header(dev, child);
	retval += printf(" at %s node %d\n",
	    device_get_nameunit(dev), tdev->td_node);
	return (retval);
}

static int
tlsb_read_ivar(device_t dev, device_t child, int index, u_long *result)
{
	struct tlsb_device *tdev = DEVTOTLSB(child);

	switch (index) {
	case TLSB_IVAR_NODE:
		*result = tdev->td_node;
		break;

	case TLSB_IVAR_DTYPE:
		*result = TLDEV_DTYPE(tdev->td_tldev);
		break;

	case TLSB_IVAR_SWREV:
		*result = TLDEV_SWREV(tdev->td_tldev);
		break;

	case TLSB_IVAR_HWREV:
		*result = TLDEV_HWREV(tdev->td_tldev);
		break;
	}
	return (ENOENT);
}

static int
tlsb_setup_intr(device_t dev, device_t child, struct resource *i, int f,
		driver_intr_t *intr, void *arg, void **c)
{
	if (strncmp(device_get_name(child), "zsc", 3) == 0) {
		if (tlsb0_softc->zsc_intr)
			return (EBUSY);
		tlsb0_softc->zsc_intr = intr;
		tlsb0_softc->zsc_arg = arg;
		return (0);
	} else if (strncmp(device_get_name(device_get_parent(child)), "kft", 3)
	    == 0) {
		if (tlsb0_softc->sub_intr == NULL)
			tlsb0_softc->sub_intr = intr;
		return (0);
	} else {
		return (ENXIO);
	}
}

static int
tlsb_teardown_intr(device_t dev, device_t child, struct resource *i, void *c)
{
	if (strncmp(device_get_name(child), "zsc", 3) == 0) {
		tlsb0_softc->zsc_intr = NULL;
		return (0);
	} else if (strncmp(device_get_name(device_get_parent(child)), "kft", 3)
	    == 0) {
		tlsb0_softc->sub_intr = NULL;
		return (0);
	} else {
		return (ENXIO);
	}
}

static void
tlsb_intr(void *frame, u_long vector)
{
	if (vector && tlsb0_softc->sub_intr)
		(*tlsb0_softc->sub_intr)((void *)vector);
}

static void
tlsb_add_child(struct tlsb_softc *tlsb, struct tlsb_device *tdev)
{
	static int kftproto, memproto, cpuproto;
	u_int32_t dtype = tdev->td_tldev & TLDEV_DTYPE_MASK;
	int i, unit, ordr, units = 1;
	char *dn;
	device_t cd;

	/*
	 * We want CPU and Memory boards to configure first, and we want the
	 * I/O boards to configure in reverse slot number order. This is
	 * further complicated by the possibility of dual CPU nodes.
	 */
	ordr = tdev->td_node << 1;

	switch (dtype) {
	case TLDEV_DTYPE_KFTHA:
	case TLDEV_DTYPE_KFTIA:
		ordr = 16 + (TLSB_NODE_MAX - tdev->td_node);
		dn = "kft";
		unit = kftproto++;
		break;
	case TLDEV_DTYPE_MS7CC:
		dn = "tlsbmem";
		unit = memproto++;
		break;
	case TLDEV_DTYPE_SCPU4:
	case TLDEV_DTYPE_SCPU16:
		dn = "tlsbcpu";
		unit = cpuproto++;
		break;
	case TLDEV_DTYPE_DCPU4:
	case TLDEV_DTYPE_DCPU16:
		units = 2;
		dn = "tlsbcpu";
		unit = cpuproto;
		cpuproto += 2;
		break;
	default:
		printf("tlsb_add_child: unknown TLSB node type 0x%x\n", dtype);
		return;
	}

	for (i = 0; i < units; i++, unit++) {
		cd = device_add_child_ordered(tlsb->tlsb_dev, ordr, dn, unit);
		if (cd == NULL) {
			return;
		}
		device_set_ivars(cd, tdev);
		device_set_desc(cd, tlsb_node_type_str(dtype));
	}
}

static char *
tlsb_node_type_str(u_int32_t dtype)
{
	static char	tmp[64];

	switch (dtype & TLDEV_DTYPE_MASK) {
	case TLDEV_DTYPE_KFTHA:
		return ("KFTHA I/O interface");

	case TLDEV_DTYPE_KFTIA:
		return ("KFTIA I/O interface");

	case TLDEV_DTYPE_MS7CC:
		return ("MS7CC Memory Module");

	case TLDEV_DTYPE_SCPU4:
		return ("Single CPU, 4MB cache");

	case TLDEV_DTYPE_SCPU16:
		return ("Single CPU, 16MB cache");

	case TLDEV_DTYPE_DCPU4:
		return ("Dual CPU, 4MB cache");

	case TLDEV_DTYPE_DCPU16:
		return ("Dual CPU, 16MB cache");

	default:
		bzero(tmp, sizeof(tmp));
		snprintf(tmp, sizeof(tmp), "unknown, type 0x%x", dtype);
		return (tmp);
	}
	/* NOTREACHED */
}
DRIVER_MODULE(tlsb, root, tlsb_driver, tlsb_devclass, 0, 0);
