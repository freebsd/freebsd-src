/*-
 * Copyright (c) 2006 IronPort Systems
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* PCI/PCI-X/PCIe bus interface for the LSI MegaSAS controllers */

#include "opt_mfi.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/select.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#include <dev/mfi/mfi_compat.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

#include <dev/mfi/mfireg.h>
#include <dev/mfi/mfi_ioctl.h>
#include <dev/mfi/mfivar.h>

static int	mfi_pci_probe(device_t);
static int	mfi_pci_attach(device_t);
static int	mfi_pci_detach(device_t);
static int	mfi_pci_suspend(device_t);
static int	mfi_pci_resume(device_t);
static void	mfi_pci_free(struct mfi_softc *);

static device_method_t mfi_methods[] = {
	DEVMETHOD(device_probe,		mfi_pci_probe),
	DEVMETHOD(device_attach,	mfi_pci_attach),
	DEVMETHOD(device_detach,	mfi_pci_detach),
	DEVMETHOD(device_suspend,	mfi_pci_suspend),
	DEVMETHOD(device_resume,	mfi_pci_resume),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	{ 0, 0 }
};

static driver_t mfi_pci_driver = {
	"mfi",
	mfi_methods,
	sizeof(struct mfi_softc)
};

static devclass_t	mfi_devclass;
DRIVER_MODULE(mfi, pci, mfi_pci_driver, mfi_devclass, 0, 0);

struct mfi_ident {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subvendor;
	uint16_t	subdevice;
	int		flags;
	const char	*desc;
} mfi_identifiers[] = {
	{0x1000, 0x0411, 0xffff, 0xffff, 0, "LSI MegaSAS 1064R"},
	{0x1028, 0x0015, 0xffff, 0xffff, 0, "Dell PERC 5/i"},
	{0, 0, 0, 0, 0, NULL}
};

static struct mfi_ident *
mfi_find_ident(device_t dev)
{
	struct mfi_ident *m;

	for (m = mfi_identifiers; m->vendor != 0; m++) {
		if ((m->vendor == pci_get_vendor(dev)) &&
		    (m->device == pci_get_device(dev)) &&
		    ((m->subvendor == pci_get_subvendor(dev)) ||
		    (m->subvendor == 0xffff)) &&
		    ((m->subdevice == pci_get_subdevice(dev)) ||
		    (m->subdevice == 0xffff)))
			return (m);
	}

	return (NULL);
}

static int
mfi_pci_probe(device_t dev)
{
	struct mfi_ident *id;

	if ((id = mfi_find_ident(dev)) != NULL) {
		device_set_desc(dev, id->desc);
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
mfi_pci_attach(device_t dev)
{
	struct mfi_softc *sc;
	struct mfi_ident *m;
	uint32_t command;
	int error;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->mfi_dev = dev;

	/* Verify that the adapter can be set up in PCI space */
	command = pci_read_config(dev, PCIR_COMMAND, 2);
	command |= PCIM_CMD_BUSMASTEREN;
	pci_write_config(dev, PCIR_COMMAND, command, 2);
	command = pci_read_config(dev, PCIR_COMMAND, 2);
	if ((command & PCIM_CMD_BUSMASTEREN) == 0) {
		device_printf(dev, "Can't enable PCI busmaster\n");
		return (ENXIO);
	}
	if ((command & PCIM_CMD_MEMEN) == 0) {
		device_printf(dev, "PCI memory window not available\n");
		return (ENXIO);
	}

	/* Allocate PCI registers */
	sc->mfi_regs_rid = PCIR_BAR(0);
	if ((sc->mfi_regs_resource = bus_alloc_resource_any(sc->mfi_dev,
	    SYS_RES_MEMORY, &sc->mfi_regs_rid, RF_ACTIVE)) == NULL) {
		device_printf(dev, "Cannot allocate PCI registers\n");
		return (ENXIO);
	}
	sc->mfi_btag = rman_get_bustag(sc->mfi_regs_resource);
	sc->mfi_bhandle = rman_get_bushandle(sc->mfi_regs_resource);

	error = ENOMEM;

	/* Allocate parent DMA tag */
	if (bus_dma_tag_create(	NULL,			/* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsize */
				BUS_SPACE_UNRESTRICTED,	/* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
				0,			/* flags */
				&sc->mfi_parent_dmat)) {
		device_printf(dev, "Cannot allocate parent DMA tag\n");
		goto out;
	}

	m = mfi_find_ident(dev);
	sc->mfi_flags = m->flags;

	error = mfi_attach(sc);
out:
	if (error) {
		mfi_free(sc);
		mfi_pci_free(sc);
	}

	return (error);
}

static int
mfi_pci_detach(device_t dev)
{
	struct mfi_softc *sc;
	struct mfi_ld *ld;
	int error;

	sc = device_get_softc(dev);

	if ((sc->mfi_flags & MFI_FLAGS_OPEN) != 0)
		return (EBUSY);

        while ((ld = TAILQ_FIRST(&sc->mfi_ld_tqh)) != NULL) {
                error = device_delete_child(dev, ld->ld_disk);
		if (error)
			return (error);
		TAILQ_REMOVE(&sc->mfi_ld_tqh, ld, ld_link);
		free(ld->ld_info, M_MFIBUF);
		free(ld, M_MFIBUF);
	}

	EVENTHANDLER_DEREGISTER(shutdown_final, sc->mfi_eh);

	mfi_shutdown(sc);
	mfi_free(sc);
	mfi_pci_free(sc);
	return (0);
}

static void
mfi_pci_free(struct mfi_softc *sc)
{

	if (sc->mfi_regs_resource != NULL) {
		bus_release_resource(sc->mfi_dev, SYS_RES_MEMORY,
		    sc->mfi_regs_rid, sc->mfi_regs_resource);
	}

	return;
}

static int
mfi_pci_suspend(device_t dev)
{

	return (EINVAL);
}

static int
mfi_pci_resume(device_t dev)
{

	return (EINVAL);
}
