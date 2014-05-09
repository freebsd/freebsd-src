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

/* PCI/PCI-X/PCIe bus interface for the LSI MPT2 controllers */

/* TODO Move headers to mpsvar */
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

#include <dev/mps/mpi/mpi2_type.h>
#include <dev/mps/mpi/mpi2.h>
#include <dev/mps/mpi/mpi2_ioc.h>
#include <dev/mps/mpi/mpi2_cnfg.h>
#include <dev/mps/mpi/mpi2_tool.h>

#include <sys/queue.h>
#include <sys/kthread.h>
#include <dev/mps/mps_ioctl.h>
#include <dev/mps/mpsvar.h>

static int	mps_pci_probe(device_t);
static int	mps_pci_attach(device_t);
static int	mps_pci_detach(device_t);
static int	mps_pci_suspend(device_t);
static int	mps_pci_resume(device_t);
static void	mps_pci_free(struct mps_softc *);
static int	mps_alloc_msix(struct mps_softc *sc, int msgs);
static int	mps_alloc_msi(struct mps_softc *sc, int msgs);

static device_method_t mps_methods[] = {
	DEVMETHOD(device_probe,		mps_pci_probe),
	DEVMETHOD(device_attach,	mps_pci_attach),
	DEVMETHOD(device_detach,	mps_pci_detach),
	DEVMETHOD(device_suspend,	mps_pci_suspend),
	DEVMETHOD(device_resume,	mps_pci_resume),

	DEVMETHOD_END
};

static driver_t mps_pci_driver = {
	"mps",
	mps_methods,
	sizeof(struct mps_softc)
};

static devclass_t	mps_devclass;
DRIVER_MODULE(mps, pci, mps_pci_driver, mps_devclass, 0, 0);
MODULE_DEPEND(mps, cam, 1, 1, 1);

struct mps_ident {
	uint16_t	vendor;
	uint16_t	device;
	uint16_t	subvendor;
	uint16_t	subdevice;
	u_int		flags;
	const char	*desc;
} mps_identifiers[] = {
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2004,
	    0xffff, 0xffff, 0, "LSI SAS2004" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2008,
	    0xffff, 0xffff, 0, "LSI SAS2008" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_1,
	    0xffff, 0xffff, 0, "LSI SAS2108" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_2,
	    0xffff, 0xffff, 0, "LSI SAS2108" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2108_3,
	    0xffff, 0xffff, 0, "LSI SAS2108" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2116_1,
	    0xffff, 0xffff, 0, "LSI SAS2116" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2116_2,
	    0xffff, 0xffff, 0, "LSI SAS2116" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_1,
	    0xffff, 0xffff, 0, "LSI SAS2208" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_2,
	    0xffff, 0xffff, 0, "LSI SAS2208" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_3,
	    0xffff, 0xffff, 0, "LSI SAS2208" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_4,
	    0xffff, 0xffff, 0, "LSI SAS2208" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_5,
	    0xffff, 0xffff, 0, "LSI SAS2208" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2208_6,
	    0xffff, 0xffff, 0, "LSI SAS2208" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_1,
	    0xffff, 0xffff, 0, "LSI SAS2308" },
	// Add Customer specific vender/subdevice id before generic
	// (0xffff) vender/subdevice id.
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_2,
	    0x8086, 0x3516, 0, "Intel(R) Integrated RAID Module RMS25JB080" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_2,
	    0x8086, 0x3517, 0, "Intel(R) Integrated RAID Module RMS25JB040" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_2,
	    0x8086, 0x3518, 0, "Intel(R) Integrated RAID Module RMS25KB080" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_2,
	    0x8086, 0x3519, 0, "Intel(R) Integrated RAID Module RMS25KB040" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_2,
	    0xffff, 0xffff, 0, "LSI SAS2308" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SAS2308_3,
	    0xffff, 0xffff, 0, "LSI SAS2308" },
	{ MPI2_MFGPAGE_VENDORID_LSI, MPI2_MFGPAGE_DEVID_SSS6200,
	    0xffff, 0xffff, MPS_FLAGS_WD_AVAILABLE, "LSI SSS6200" },
	{ 0, 0, 0, 0, 0, NULL }
};

static struct mps_ident *
mps_find_ident(device_t dev)
{
	struct mps_ident *m;

	for (m = mps_identifiers; m->vendor != 0; m++) {
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
mps_pci_probe(device_t dev)
{
	struct mps_ident *id;

	if ((id = mps_find_ident(dev)) != NULL) {
		device_set_desc(dev, id->desc);
		return (BUS_PROBE_DEFAULT);
	}
	return (ENXIO);
}

static int
mps_pci_attach(device_t dev)
{
	struct mps_softc *sc;
	struct mps_ident *m;
	int error;

	sc = device_get_softc(dev);
	bzero(sc, sizeof(*sc));
	sc->mps_dev = dev;
	m = mps_find_ident(dev);
	sc->mps_flags = m->flags;

	/* Twiddle basic PCI config bits for a sanity check */
	pci_enable_busmaster(dev);

	/* Allocate the System Interface Register Set */
	sc->mps_regs_rid = PCIR_BAR(1);
	if ((sc->mps_regs_resource = bus_alloc_resource_any(dev,
	    SYS_RES_MEMORY, &sc->mps_regs_rid, RF_ACTIVE)) == NULL) {
		mps_printf(sc, "Cannot allocate PCI registers\n");
		return (ENXIO);
	}
	sc->mps_btag = rman_get_bustag(sc->mps_regs_resource);
	sc->mps_bhandle = rman_get_bushandle(sc->mps_regs_resource);

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
				&sc->mps_parent_dmat)) {
		mps_printf(sc, "Cannot allocate parent DMA tag\n");
		mps_pci_free(sc);
		return (ENOMEM);
	}

	if ((error = mps_attach(sc)) != 0)
		mps_pci_free(sc);

	return (error);
}

int
mps_pci_setup_interrupts(struct mps_softc *sc)
{
	device_t dev;
	int i, error, msgs;

	dev = sc->mps_dev;
	error = ENXIO;
	if ((sc->disable_msix == 0) &&
	    ((msgs = pci_msix_count(dev)) >= MPS_MSI_COUNT))
		error = mps_alloc_msix(sc, MPS_MSI_COUNT);
	if ((error != 0) && (sc->disable_msi == 0) &&
	    ((msgs = pci_msi_count(dev)) >= MPS_MSI_COUNT))
		error = mps_alloc_msi(sc, MPS_MSI_COUNT);

	if (error != 0) {
		sc->mps_flags |= MPS_FLAGS_INTX;
		sc->mps_irq_rid[0] = 0;
		sc->mps_irq[0] = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &sc->mps_irq_rid[0],  RF_SHAREABLE | RF_ACTIVE);
		if (sc->mps_irq[0] == NULL) {
			mps_printf(sc, "Cannot allocate INTx interrupt\n");
			return (ENXIO);
		}
		error = bus_setup_intr(dev, sc->mps_irq[0],
		    INTR_TYPE_BIO | INTR_MPSAFE, NULL, mps_intr, sc,
		    &sc->mps_intrhand[0]);
		if (error)
			mps_printf(sc, "Cannot setup INTx interrupt\n");
	} else {
		sc->mps_flags |= MPS_FLAGS_MSI;
		for (i = 0; i < MPS_MSI_COUNT; i++) {
			sc->mps_irq_rid[i] = i + 1;
			sc->mps_irq[i] = bus_alloc_resource_any(dev,
			    SYS_RES_IRQ, &sc->mps_irq_rid[i], RF_ACTIVE);
			if (sc->mps_irq[i] == NULL) {
				mps_printf(sc,
				    "Cannot allocate MSI interrupt\n");
				return (ENXIO);
			}
			error = bus_setup_intr(dev, sc->mps_irq[i],
			    INTR_TYPE_BIO | INTR_MPSAFE, NULL, mps_intr_msi,
			    sc, &sc->mps_intrhand[i]);
			if (error) {
				mps_printf(sc,
				    "Cannot setup MSI interrupt %d\n", i);
				break;
			}
		}
	}

	return (error);
}

static int
mps_pci_detach(device_t dev)
{
	struct mps_softc *sc;
	int error;

	sc = device_get_softc(dev);

	if ((error = mps_free(sc)) != 0)
		return (error);

	mps_pci_free(sc);
	return (0);
}

static void
mps_pci_free(struct mps_softc *sc)
{
	int i;

	if (sc->mps_parent_dmat != NULL) {
		bus_dma_tag_destroy(sc->mps_parent_dmat);
	}

	if (sc->mps_flags & MPS_FLAGS_MSI) {
		for (i = 0; i < MPS_MSI_COUNT; i++) {
			if (sc->mps_irq[i] != NULL) {
				bus_teardown_intr(sc->mps_dev, sc->mps_irq[i],
				    sc->mps_intrhand[i]);
				bus_release_resource(sc->mps_dev, SYS_RES_IRQ,
				    sc->mps_irq_rid[i], sc->mps_irq[i]);
			}
		}
		pci_release_msi(sc->mps_dev);
	}

	if (sc->mps_flags & MPS_FLAGS_INTX) {
		bus_teardown_intr(sc->mps_dev, sc->mps_irq[0],
		    sc->mps_intrhand[0]);
		bus_release_resource(sc->mps_dev, SYS_RES_IRQ,
		    sc->mps_irq_rid[0], sc->mps_irq[0]);
	}

	if (sc->mps_regs_resource != NULL) {
		bus_release_resource(sc->mps_dev, SYS_RES_MEMORY,
		    sc->mps_regs_rid, sc->mps_regs_resource);
	}

	return;
}

static int
mps_pci_suspend(device_t dev)
{
	return (EINVAL);
}

static int
mps_pci_resume(device_t dev)
{
	return (EINVAL);
}

static int
mps_alloc_msix(struct mps_softc *sc, int msgs)
{
	int error;

	error = pci_alloc_msix(sc->mps_dev, &msgs);
	return (error);
}

static int
mps_alloc_msi(struct mps_softc *sc, int msgs)
{
	int error;

	error = pci_alloc_msi(sc->mps_dev, &msgs);
	return (error);
}

int
mps_pci_restore(struct mps_softc *sc)
{
	struct pci_devinfo *dinfo;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	dinfo = device_get_ivars(sc->mps_dev);
	if (dinfo == NULL) {
		mps_dprint(sc, MPS_FAULT, "%s: NULL dinfo\n", __func__);
		return (EINVAL);
	}

	pci_cfg_restore(sc->mps_dev, dinfo);
	return (0);
}

