/*-
 * Copyright (c) 2009 Yahoo! Inc.
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

/* PCI/PCI-X/PCIe bus interface for the Avago Tech (LSI) MPT3 controllers */

/* TODO Move headers to mprvar */
#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/malloc.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pci_private.h>

#include <dev/mpr/mpi/mpi2_type.h>
#include <dev/mpr/mpi/mpi2.h>
#include <dev/mpr/mpi/mpi2_ioc.h>
#include <dev/mpr/mpi/mpi2_cnfg.h>
#include <dev/mpr/mpi/mpi2_tool.h>

#include <sys/queue.h>
#include <sys/kthread.h>
#include <dev/mpr/mpr_ioctl.h>
#include <dev/mpr/mprvar.h>

static int	mpr_pci_probe(device_t);
static int	mpr_pci_attach(device_t);
static int	mpr_pci_detach(device_t);
static int	mpr_pci_suspend(device_t);
static int	mpr_pci_resume(device_t);
static void	mpr_pci_free(struct mpr_softc *);
static int	mpr_alloc_msix(struct mpr_softc *sc, int msgs);
static int	mpr_alloc_msi(struct mpr_softc *sc, int msgs);

static device_method_t mpr_methods[] = {
	DEVMETHOD(device_probe,		mpr_pci_probe),
	DEVMETHOD(device_attach,	mpr_pci_attach),
	DEVMETHOD(device_detach,	mpr_pci_detach),
	DEVMETHOD(device_suspend,	mpr_pci_suspend),
	DEVMETHOD(device_resume,	mpr_pci_resume),
	DEVMETHOD(bus_print_child,	bus_generic_print_child),
	DEVMETHOD(bus_driver_added,	bus_generic_driver_added),
	{ 0, 0 }
};

static driver_t mpr_pci_driver = {
	"mpr",
	mpr_methods,
	sizeof(struct mpr_softc)
};

static devclass_t	mpr_devclass;
DRIVER_MODULE(mpr, pci, mpr_pci_driver, mpr_devclass, 0, 0);
MODULE_DEPEND(mpr, cam, 1, 1, 1);

struct mpr_ident {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subvendor;
	uint16_t	subdevice;
	u_int		flags;
	const char	*desc;
} mpr_identifiers[] = {
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI25_MFGPAGE_DEVID_SAS3004,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3004" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI25_MFGPAGE_DEVID_SAS3008,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3008" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI25_MFGPAGE_DEVID_SAS3108_1,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3108_1" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI25_MFGPAGE_DEVID_SAS3108_2,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3108_2" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI25_MFGPAGE_DEVID_SAS3108_5,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3108_5" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI25_MFGPAGE_DEVID_SAS3108_6,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3108_6" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI26_MFGPAGE_DEVID_SAS3316_1,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3316_1" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI26_MFGPAGE_DEVID_SAS3316_2,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3316_2" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI26_MFGPAGE_DEVID_SAS3324_1,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3324_1" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI26_MFGPAGE_DEVID_SAS3324_2,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3324_2" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI26_MFGPAGE_DEVID_SAS3216,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3216" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI26_MFGPAGE_DEVID_SAS3224,
	    0xffff, 0xffff, 0, "Avago Technologies (LSI) SAS3224" },
	{ 0, 0, 0, 0, 0, NULL }
};

static struct mpr_ident *
mpr_find_ident(device_t dev)
{
	struct mpr_ident *m;

	for (m = mpr_identifiers; m->vendor != 0; m++) {
		if (m->vendor != pci_get_vendor(dev))
			continue;
		if (m->device != pci_get_device(dev))
			continue;
		if ((m->subvendor != 0xffff) &&
		    (m->subvendor != pci_get_subvendor(dev)))
			continue;
		if ((m->subdevice != 0xffff) &&
		    (m->subdevice != pci_get_subdevice(dev)))
			continue;
		return (m);
	}

	return (NULL);
}

static int
mpr_pci_probe(device_t dev)
{
	struct mpr_ident *id;

	if ((id = mpr_find_ident(dev)) != NULL) {
		device_set_desc(dev, id->desc);
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
mpr_pci_attach(device_t dev)
{
	struct mpr_softc *sc;
	struct mpr_ident *m;
	int error;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->mpr_dev = dev;
	m = mpr_find_ident(dev);
	sc->mpr_flags = m->flags;

	/* Twiddle basic PCI config bits for a sanity check */
	pci_enable_busmaster(dev);

	/* Allocate the System Interface Register Set */
	sc->mpr_regs_rid = PCIR_BAR(1);
	if ((sc->mpr_regs_resource = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &sc->mpr_regs_rid, RF_ACTIVE)) == NULL) {
		mpr_printf(sc, "Cannot allocate PCI registers\n");
		return (ENXIO);
	}
	sc->mpr_btag = rman_get_bustag(sc->mpr_regs_resource);
	sc->mpr_bhandle = rman_get_bushandle(sc->mpr_regs_resource);

	/* Allocate the parent DMA tag */
	if (bus_dma_tag_create( bus_get_dma_tag(dev),	/* parent */
				1, 0,			/* algnmnt, boundary */
				BUS_SPACE_MAXADDR,	/* lowaddr */
				BUS_SPACE_MAXADDR,	/* highaddr */
				NULL, NULL,		/* filter, filterarg */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsize */
				BUS_SPACE_UNRESTRICTED,	/* nsegments */
				BUS_SPACE_MAXSIZE_32BIT,/* maxsegsize */
				0,			/* flags */
				NULL, NULL,		/* lockfunc, lockarg */
				&sc->mpr_parent_dmat)) {
		mpr_printf(sc, "Cannot allocate parent DMA tag\n");
		mpr_pci_free(sc);
		return (ENOMEM);
	}

	if ((error = mpr_attach(sc)) != 0)
		mpr_pci_free(sc);

	return (error);
}

int
mpr_pci_setup_interrupts(struct mpr_softc *sc)
{
	device_t dev;
	int i, error, msgs;

	dev = sc->mpr_dev;
	error = ENXIO;
	if ((sc->disable_msix == 0) &&
	    ((msgs = pci_msix_count(dev)) >= MPR_MSI_COUNT))
		error = mpr_alloc_msix(sc, MPR_MSI_COUNT);
	if ((error != 0) && (sc->disable_msi == 0) &&
	    ((msgs = pci_msi_count(dev)) >= MPR_MSI_COUNT))
		error = mpr_alloc_msi(sc, MPR_MSI_COUNT);

	if (error != 0) {
		sc->mpr_flags |= MPR_FLAGS_INTX;
		sc->mpr_irq_rid[0] = 0;
		sc->mpr_irq[0] = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->mpr_irq_rid[0],  RF_SHAREABLE | RF_ACTIVE);
		if (sc->mpr_irq[0] == NULL) {
			mpr_printf(sc, "Cannot allocate INTx interrupt\n");
			return (ENXIO);
		}
		error = bus_setup_intr(dev, sc->mpr_irq[0],
		    INTR_TYPE_BIO | INTR_MPSAFE, NULL, mpr_intr, sc,
		    &sc->mpr_intrhand[0]);
		if (error)
			mpr_printf(sc, "Cannot setup INTx interrupt\n");
	} else {
		sc->mpr_flags |= MPR_FLAGS_MSI;
		for (i = 0; i < MPR_MSI_COUNT; i++) {
			sc->mpr_irq_rid[i] = i + 1;
			sc->mpr_irq[i] = bus_alloc_resource_any(dev,
			    SYS_RES_IRQ, &sc->mpr_irq_rid[i], RF_ACTIVE);
			if (sc->mpr_irq[i] == NULL) {
				mpr_printf(sc,
				    "Cannot allocate MSI interrupt\n");
				return (ENXIO);
			}
			error = bus_setup_intr(dev, sc->mpr_irq[i],
			    INTR_TYPE_BIO | INTR_MPSAFE, NULL, mpr_intr_msi,
			    sc, &sc->mpr_intrhand[i]);
			if (error) {
				mpr_printf(sc,
				    "Cannot setup MSI interrupt %d\n", i);
				break;
			}
		}
	}

	return (error);
}

static int
mpr_pci_detach(device_t dev)
{
	struct mpr_softc *sc;
	int error;

	sc = device_get_softc(dev);

	if ((error = mpr_free(sc)) != 0)
		return (error);

	mpr_pci_free(sc);
	return (0);
}

static void
mpr_pci_free(struct mpr_softc *sc)
{
	int i;

	if (sc->mpr_parent_dmat != NULL) {
		bus_dma_tag_destroy(sc->mpr_parent_dmat);
	}

	if (sc->mpr_flags & MPR_FLAGS_MSI) {
		for (i = 0; i < MPR_MSI_COUNT; i++) {
			if (sc->mpr_irq[i] != NULL) {
				bus_teardown_intr(sc->mpr_dev, sc->mpr_irq[i],
				    sc->mpr_intrhand[i]);
				bus_release_resource(sc->mpr_dev, SYS_RES_IRQ,
				    sc->mpr_irq_rid[i], sc->mpr_irq[i]);
			}
		}
		pci_release_msi(sc->mpr_dev);
	}

	if (sc->mpr_flags & MPR_FLAGS_INTX) {
		bus_teardown_intr(sc->mpr_dev, sc->mpr_irq[0],
		    sc->mpr_intrhand[0]);
		bus_release_resource(sc->mpr_dev, SYS_RES_IRQ,
		    sc->mpr_irq_rid[0], sc->mpr_irq[0]);
	}

	if (sc->mpr_regs_resource != NULL) {
		bus_release_resource(sc->mpr_dev, SYS_RES_MEMORY,
		    sc->mpr_regs_rid, sc->mpr_regs_resource);
	}

	return;
}

static int
mpr_pci_suspend(device_t dev)
{
	return (EINVAL);
}

static int
mpr_pci_resume(device_t dev)
{
	return (EINVAL);
}

static int
mpr_alloc_msix(struct mpr_softc *sc, int msgs)
{
	int error;

	error = pci_alloc_msix(sc->mpr_dev, &msgs);
	return (error);
}

static int
mpr_alloc_msi(struct mpr_softc *sc, int msgs)
{
	int error;

	error = pci_alloc_msi(sc->mpr_dev, &msgs);
	return (error);
}

int
mpr_pci_restore(struct mpr_softc *sc)
{
	struct pci_devinfo *dinfo;

	mpr_dprint(sc, MPR_TRACE, "%s\n", __func__);

	dinfo = device_get_ivars(sc->mpr_dev);
	if (dinfo == NULL) {
		mpr_dprint(sc, MPR_FAULT, "%s: NULL dinfo\n", __func__);
		return (EINVAL);
	}

	pci_cfg_restore(sc->mpr_dev, dinfo);
	return (0);
}

