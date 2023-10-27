/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2009-2020 Alexander Motin <mav@FreeBSD.org>
 * Copyright (c) 1997-2008 by Matthew Jacob
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice immediately at the beginning of the file, without modification,
 *    this list of conditions, and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
 * PCI specific probe and attach routines for Qlogic ISP SCSI adapters.
 * FreeBSD Version.
 */
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/bus.h>
#include <sys/stdint.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <dev/isp/isp_freebsd.h>

static uint32_t isp_pci_rd_reg_2400(ispsoftc_t *, int);
static void isp_pci_wr_reg_2400(ispsoftc_t *, int, uint32_t);
static uint32_t isp_pci_rd_reg_2600(ispsoftc_t *, int);
static void isp_pci_wr_reg_2600(ispsoftc_t *, int, uint32_t);
static void isp_pci_run_isr_2400(ispsoftc_t *);
static int isp_pci_mbxdma(ispsoftc_t *);
static void isp_pci_mbxdmafree(ispsoftc_t *);
static int isp_pci_irqsetup(ispsoftc_t *);

static struct ispmdvec mdvec_2400 = {
	isp_pci_run_isr_2400,
	isp_pci_rd_reg_2400,
	isp_pci_wr_reg_2400,
	isp_pci_mbxdma,
	isp_send_cmd,
	isp_pci_irqsetup,
	NULL
};

static struct ispmdvec mdvec_2500 = {
	isp_pci_run_isr_2400,
	isp_pci_rd_reg_2400,
	isp_pci_wr_reg_2400,
	isp_pci_mbxdma,
	isp_send_cmd,
	isp_pci_irqsetup,
	NULL
};

static struct ispmdvec mdvec_2600 = {
	isp_pci_run_isr_2400,
	isp_pci_rd_reg_2600,
	isp_pci_wr_reg_2600,
	isp_pci_mbxdma,
	isp_send_cmd,
	isp_pci_irqsetup,
	NULL
};

static struct ispmdvec mdvec_2700 = {
	isp_pci_run_isr_2400,
	isp_pci_rd_reg_2600,
	isp_pci_wr_reg_2600,
	isp_pci_mbxdma,
	isp_send_cmd,
	isp_pci_irqsetup,
	NULL
};

static struct ispmdvec mdvec_2800 = {
	isp_pci_run_isr_2400,
	isp_pci_rd_reg_2600,
	isp_pci_wr_reg_2600,
	isp_pci_mbxdma,
	isp_send_cmd,
	isp_pci_irqsetup,
	NULL
};

#ifndef	PCIM_CMD_INVEN
#define	PCIM_CMD_INVEN			0x10
#endif
#ifndef	PCIM_CMD_BUSMASTEREN
#define	PCIM_CMD_BUSMASTEREN		0x0004
#endif
#ifndef	PCIM_CMD_PERRESPEN
#define	PCIM_CMD_PERRESPEN		0x0040
#endif
#ifndef	PCIM_CMD_SEREN
#define	PCIM_CMD_SEREN			0x0100
#endif
#ifndef	PCIM_CMD_INTX_DISABLE
#define	PCIM_CMD_INTX_DISABLE		0x0400
#endif

#ifndef	PCIR_COMMAND
#define	PCIR_COMMAND			0x04
#endif

#ifndef	PCIR_CACHELNSZ
#define	PCIR_CACHELNSZ			0x0c
#endif

#ifndef	PCIR_LATTIMER
#define	PCIR_LATTIMER			0x0d
#endif

#ifndef	PCIR_ROMADDR
#define	PCIR_ROMADDR			0x30
#endif

#define	PCI_VENDOR_QLOGIC		0x1077

#define	PCI_PRODUCT_QLOGIC_ISP2422	0x2422
#define	PCI_PRODUCT_QLOGIC_ISP2432	0x2432
#define	PCI_PRODUCT_QLOGIC_ISP2532	0x2532
#define	PCI_PRODUCT_QLOGIC_ISP5432	0x5432
#define	PCI_PRODUCT_QLOGIC_ISP2031	0x2031
#define	PCI_PRODUCT_QLOGIC_ISP8031	0x8031
#define	PCI_PRODUCT_QLOGIC_ISP2684	0x2171
#define	PCI_PRODUCT_QLOGIC_ISP2692	0x2b61
#define	PCI_PRODUCT_QLOGIC_ISP2714	0x2071
#define	PCI_PRODUCT_QLOGIC_ISP2722	0x2261
#define	PCI_PRODUCT_QLOGIC_ISP2812	0x2281
#define	PCI_PRODUCT_QLOGIC_ISP2814	0x2081

#define	PCI_QLOGIC_ISP2422	\
	((PCI_PRODUCT_QLOGIC_ISP2422 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2432	\
	((PCI_PRODUCT_QLOGIC_ISP2432 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2532	\
	((PCI_PRODUCT_QLOGIC_ISP2532 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP5432	\
	((PCI_PRODUCT_QLOGIC_ISP5432 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2031	\
	((PCI_PRODUCT_QLOGIC_ISP2031 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP8031	\
	((PCI_PRODUCT_QLOGIC_ISP8031 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2684	\
	((PCI_PRODUCT_QLOGIC_ISP2684 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2692	\
	((PCI_PRODUCT_QLOGIC_ISP2692 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2714	\
	((PCI_PRODUCT_QLOGIC_ISP2714 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2722	\
	((PCI_PRODUCT_QLOGIC_ISP2722 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2812	\
	((PCI_PRODUCT_QLOGIC_ISP2812 << 16) | PCI_VENDOR_QLOGIC)
#define	PCI_QLOGIC_ISP2814	\
	((PCI_PRODUCT_QLOGIC_ISP2814 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_DFLT_LTNCY	0x40
#define	PCI_DFLT_LNSZ	0x10

static int isp_pci_probe (device_t);
static int isp_pci_attach (device_t);
static int isp_pci_detach (device_t);


struct isp_pcisoftc {
	ispsoftc_t			pci_isp;
	struct resource *		regs;
	struct resource *		regs1;
	struct resource *		regs2;
	struct {
		int				iqd;
		struct resource *		irq;
		void *				ih;
	} irq[ISP_MAX_IRQS];
	int				rtp;
	int				rgd;
	int				rtp1;
	int				rgd1;
	int				rtp2;
	int				rgd2;
	bus_dma_tag_t			dmat;
	int				msicount;
};


static device_method_t isp_pci_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		isp_pci_probe),
	DEVMETHOD(device_attach,	isp_pci_attach),
	DEVMETHOD(device_detach,	isp_pci_detach),
	{ 0, 0 }
};

static driver_t isp_pci_driver = {
	"isp", isp_pci_methods, sizeof (struct isp_pcisoftc)
};
static devclass_t isp_devclass;
DRIVER_MODULE(isp, pci, isp_pci_driver, isp_devclass, 0, 0);
MODULE_DEPEND(isp, cam, 1, 1, 1);
MODULE_DEPEND(isp, firmware, 1, 1, 1);
static int isp_nvports = 0;

static int
isp_pci_probe(device_t dev)
{
	switch ((pci_get_device(dev) << 16) | (pci_get_vendor(dev))) {
	case PCI_QLOGIC_ISP2422:
		device_set_desc(dev, "Qlogic ISP 2422 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP2432:
		device_set_desc(dev, "Qlogic ISP 2432 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP2532:
		device_set_desc(dev, "Qlogic ISP 2532 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP5432:
		device_set_desc(dev, "Qlogic ISP 5432 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP2031:
		device_set_desc(dev, "Qlogic ISP 2031 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP8031:
		device_set_desc(dev, "Qlogic ISP 8031 PCI FCoE Adapter");
		break;
	case PCI_QLOGIC_ISP2684:
		device_set_desc(dev, "Qlogic ISP 2684 PCI FC Adapter");
		break;
	case PCI_QLOGIC_ISP2692:
		device_set_desc(dev, "Qlogic ISP 2692 PCI FC Adapter");
		break;
	case PCI_QLOGIC_ISP2714:
		device_set_desc(dev, "Qlogic ISP 2714 PCI FC Adapter");
		break;
	case PCI_QLOGIC_ISP2722:
		device_set_desc(dev, "Qlogic ISP 2722 PCI FC Adapter");
		break;
	case PCI_QLOGIC_ISP2812:
		device_set_desc(dev, "Qlogic ISP 2812 PCI FC Adapter");
		break;
	case PCI_QLOGIC_ISP2814:
		device_set_desc(dev, "Qlogic ISP 2814 PCI FC Adapter");
		break;
	default:
		return (ENXIO);
	}
	if (isp_announced == 0 && bootverbose) {
		printf("Qlogic ISP Driver, FreeBSD Version %d.%d, "
		    "Core Version %d.%d\n",
		    ISP_PLATFORM_VERSION_MAJOR, ISP_PLATFORM_VERSION_MINOR,
		    ISP_CORE_VERSION_MAJOR, ISP_CORE_VERSION_MINOR);
		isp_announced++;
	}
	/*
	 * XXXX: Here is where we might load the f/w module
	 * XXXX: (or increase a reference count to it).
	 */
	return (BUS_PROBE_DEFAULT);
}

static void
isp_get_generic_options(device_t dev, ispsoftc_t *isp)
{
	int tval;

	tval = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev), "fwload_disable", &tval) == 0 && tval != 0) {
		isp->isp_confopts |= ISP_CFG_NORELOAD;
	}
	tval = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev), "ignore_nvram", &tval) == 0 && tval != 0) {
		isp->isp_confopts |= ISP_CFG_NONVRAM;
	}
	tval = 0;
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev), "debug", &tval);
	if (tval) {
		isp->isp_dblev = tval;
	} else {
		isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
	}
	if (bootverbose) {
		isp->isp_dblev |= ISP_LOGCONFIG|ISP_LOGINFO;
	}
	tval = -1;
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev), "vports", &tval);
	if (tval > 0 && tval <= 254) {
		isp_nvports = tval;
	}
	tval = 7;
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev), "quickboot_time", &tval);
	isp_quickboot_time = tval;
}

static void
isp_get_specific_options(device_t dev, int chan, ispsoftc_t *isp)
{
	const char *sptr;
	int tval = 0;
	char prefix[12], name[16];

	if (chan == 0)
		prefix[0] = 0;
	else
		snprintf(prefix, sizeof(prefix), "chan%d.", chan);
	snprintf(name, sizeof(name), "%siid", prefix);
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    name, &tval)) {
		ISP_FC_PC(isp, chan)->default_id = 109 - chan;
	} else {
		ISP_FC_PC(isp, chan)->default_id = tval - chan;
		isp->isp_confopts |= ISP_CFG_OWNLOOPID;
	}

	tval = -1;
	snprintf(name, sizeof(name), "%srole", prefix);
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    name, &tval) == 0) {
		switch (tval) {
		case ISP_ROLE_NONE:
		case ISP_ROLE_INITIATOR:
		case ISP_ROLE_TARGET:
		case ISP_ROLE_BOTH:
			device_printf(dev, "Chan %d setting role to 0x%x\n", chan, tval);
			break;
		default:
			tval = -1;
			break;
		}
	}
	if (tval == -1) {
		tval = ISP_DEFAULT_ROLES;
	}
	ISP_FC_PC(isp, chan)->def_role = tval;

	tval = 0;
	snprintf(name, sizeof(name), "%sfullduplex", prefix);
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    name, &tval) == 0 && tval != 0) {
		isp->isp_confopts |= ISP_CFG_FULL_DUPLEX;
	}
	sptr = NULL;
	snprintf(name, sizeof(name), "%stopology", prefix);
	if (resource_string_value(device_get_name(dev), device_get_unit(dev),
	    name, (const char **) &sptr) == 0 && sptr != NULL) {
		if (strcmp(sptr, "lport") == 0) {
			isp->isp_confopts |= ISP_CFG_LPORT;
		} else if (strcmp(sptr, "nport") == 0) {
			isp->isp_confopts |= ISP_CFG_NPORT;
		} else if (strcmp(sptr, "lport-only") == 0) {
			isp->isp_confopts |= ISP_CFG_LPORT_ONLY;
		} else if (strcmp(sptr, "nport-only") == 0) {
			isp->isp_confopts |= ISP_CFG_NPORT_ONLY;
		}
	}

#ifdef ISP_FCTAPE_OFF
	isp->isp_confopts |= ISP_CFG_NOFCTAPE;
#else
	isp->isp_confopts |= ISP_CFG_FCTAPE;
#endif

	tval = 0;
	snprintf(name, sizeof(name), "%snofctape", prefix);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    name, &tval);
	if (tval) {
		isp->isp_confopts &= ~ISP_CFG_FCTAPE;
		isp->isp_confopts |= ISP_CFG_NOFCTAPE;
	}

	tval = 0;
	snprintf(name, sizeof(name), "%sfctape", prefix);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    name, &tval);
	if (tval) {
		isp->isp_confopts &= ~ISP_CFG_NOFCTAPE;
		isp->isp_confopts |= ISP_CFG_FCTAPE;
	}


	/*
	 * Because the resource_*_value functions can neither return
	 * 64 bit integer values, nor can they be directly coerced
	 * to interpret the right hand side of the assignment as
	 * you want them to interpret it, we have to force WWN
	 * hint replacement to specify WWN strings with a leading
	 * 'w' (e..g w50000000aaaa0001). Sigh.
	 */
	sptr = NULL;
	snprintf(name, sizeof(name), "%sportwwn", prefix);
	tval = resource_string_value(device_get_name(dev), device_get_unit(dev),
	    name, (const char **) &sptr);
	if (tval == 0 && sptr != NULL && *sptr++ == 'w') {
		char *eptr = NULL;
		ISP_FC_PC(isp, chan)->def_wwpn = strtouq(sptr, &eptr, 16);
		if (eptr < sptr + 16 || ISP_FC_PC(isp, chan)->def_wwpn == -1) {
			device_printf(dev, "mangled portwwn hint '%s'\n", sptr);
			ISP_FC_PC(isp, chan)->def_wwpn = 0;
		}
	}

	sptr = NULL;
	snprintf(name, sizeof(name), "%snodewwn", prefix);
	tval = resource_string_value(device_get_name(dev), device_get_unit(dev),
	    name, (const char **) &sptr);
	if (tval == 0 && sptr != NULL && *sptr++ == 'w') {
		char *eptr = NULL;
		ISP_FC_PC(isp, chan)->def_wwnn = strtouq(sptr, &eptr, 16);
		if (eptr < sptr + 16 || ISP_FC_PC(isp, chan)->def_wwnn == 0) {
			device_printf(dev, "mangled nodewwn hint '%s'\n", sptr);
			ISP_FC_PC(isp, chan)->def_wwnn = 0;
		}
	}

	tval = -1;
	snprintf(name, sizeof(name), "%sloop_down_limit", prefix);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    name, &tval);
	if (tval >= 0 && tval < 0xffff) {
		ISP_FC_PC(isp, chan)->loop_down_limit = tval;
	} else {
		ISP_FC_PC(isp, chan)->loop_down_limit = isp_loop_down_limit;
	}

	tval = -1;
	snprintf(name, sizeof(name), "%sgone_device_time", prefix);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    name, &tval);
	if (tval >= 0 && tval < 0xffff) {
		ISP_FC_PC(isp, chan)->gone_device_time = tval;
	} else {
		ISP_FC_PC(isp, chan)->gone_device_time = isp_gone_device_time;
	}
}

static int
isp_pci_attach(device_t dev)
{
	struct isp_pcisoftc *pcs = device_get_softc(dev);
	ispsoftc_t *isp = &pcs->pci_isp;
	int i;
	uint32_t data, cmd, linesz;
	size_t psize, xsize;

	isp->isp_dev = dev;
	isp->isp_nchan = 1;
	mtx_init(&isp->isp_lock, "isp", NULL, MTX_DEF);

	/*
	 * Get Generic Options
	 */
	isp_nvports = 0;
	isp_get_generic_options(dev, isp);

	linesz = PCI_DFLT_LNSZ;
	pcs->regs = pcs->regs2 = NULL;
	pcs->rgd = pcs->rtp = 0;

	isp->isp_nchan += isp_nvports;
	switch (pci_get_devid(dev)) {
	case PCI_QLOGIC_ISP2422:
	case PCI_QLOGIC_ISP2432:
		isp->isp_did = 0x2400;
		isp->isp_mdvec = &mdvec_2400;
		isp->isp_type = ISP_HA_FC_2400;
		break;
	case PCI_QLOGIC_ISP2532:
		isp->isp_did = 0x2500;
		isp->isp_mdvec = &mdvec_2500;
		isp->isp_type = ISP_HA_FC_2500;
		break;
	case PCI_QLOGIC_ISP5432:
		isp->isp_did = 0x2500;
		isp->isp_mdvec = &mdvec_2500;
		isp->isp_type = ISP_HA_FC_2500;
		break;
	case PCI_QLOGIC_ISP2031:
	case PCI_QLOGIC_ISP8031:
		isp->isp_did = 0x2600;
		isp->isp_mdvec = &mdvec_2600;
		isp->isp_type = ISP_HA_FC_2600;
		break;
	case PCI_QLOGIC_ISP2684:
	case PCI_QLOGIC_ISP2692:
	case PCI_QLOGIC_ISP2714:
	case PCI_QLOGIC_ISP2722:
		isp->isp_did = 0x2700;
		isp->isp_mdvec = &mdvec_2700;
		isp->isp_type = ISP_HA_FC_2700;
		break;
	case PCI_QLOGIC_ISP2812:
	case PCI_QLOGIC_ISP2814:
		isp->isp_did = 0x2800;
		isp->isp_mdvec = &mdvec_2800;
		isp->isp_type = ISP_HA_FC_2800;
		break;
	default:
		device_printf(dev, "unknown device type\n");
		goto bad;
		break;
	}
	isp->isp_revision = pci_get_revid(dev);

	if (IS_26XX(isp)) {
		pcs->rtp = SYS_RES_MEMORY;
		pcs->rgd = PCIR_BAR(0);
		pcs->regs = bus_alloc_resource_any(dev, pcs->rtp, &pcs->rgd,
		    RF_ACTIVE);
		pcs->rtp1 = SYS_RES_MEMORY;
		pcs->rgd1 = PCIR_BAR(2);
		pcs->regs1 = bus_alloc_resource_any(dev, pcs->rtp1, &pcs->rgd1,
		    RF_ACTIVE);
		pcs->rtp2 = SYS_RES_MEMORY;
		pcs->rgd2 = PCIR_BAR(4);
		pcs->regs2 = bus_alloc_resource_any(dev, pcs->rtp2, &pcs->rgd2,
		    RF_ACTIVE);
	} else {
		pcs->rtp = SYS_RES_MEMORY;
		pcs->rgd = PCIR_BAR(1);
		pcs->regs = bus_alloc_resource_any(dev, pcs->rtp, &pcs->rgd,
		    RF_ACTIVE);
		if (pcs->regs == NULL) {
			pcs->rtp = SYS_RES_IOPORT;
			pcs->rgd = PCIR_BAR(0);
			pcs->regs = bus_alloc_resource_any(dev, pcs->rtp,
			    &pcs->rgd, RF_ACTIVE);
		}
	}
	if (pcs->regs == NULL) {
		device_printf(dev, "Unable to map any ports\n");
		goto bad;
	}
	if (bootverbose) {
		device_printf(dev, "Using %s space register mapping\n",
		    (pcs->rtp == SYS_RES_IOPORT)? "I/O" : "Memory");
	}
	isp->isp_regs = pcs->regs;
	isp->isp_regs2 = pcs->regs2;

	psize = sizeof(fcparam) * isp->isp_nchan;
	xsize = sizeof(struct isp_fc) * isp->isp_nchan;
	isp->isp_param = malloc(psize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (isp->isp_param == NULL) {
		device_printf(dev, "cannot allocate parameter data\n");
		goto bad;
	}
	isp->isp_osinfo.fc = malloc(xsize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (isp->isp_osinfo.fc == NULL) {
		device_printf(dev, "cannot allocate parameter data\n");
		goto bad;
	}

	/*
	 * Now that we know who we are (roughly) get/set specific options
	 */
	for (i = 0; i < isp->isp_nchan; i++) {
		isp_get_specific_options(dev, i, isp);
	}

	/*
	 * Make sure that SERR, PERR, WRITE INVALIDATE and BUSMASTER are set.
	 */
	cmd = pci_read_config(dev, PCIR_COMMAND, 2);
	cmd |= PCIM_CMD_SEREN | PCIM_CMD_PERRESPEN | PCIM_CMD_BUSMASTEREN | PCIM_CMD_INVEN;
	cmd &= ~PCIM_CMD_INTX_DISABLE;
	pci_write_config(dev, PCIR_COMMAND, cmd, 2);

	/*
	 * Make sure the Cache Line Size register is set sensibly.
	 */
	data = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	if (data == 0 || (linesz != PCI_DFLT_LNSZ && data != linesz)) {
		isp_prt(isp, ISP_LOGDEBUG0, "set PCI line size to %d from %d", linesz, data);
		data = linesz;
		pci_write_config(dev, PCIR_CACHELNSZ, data, 1);
	}

	/*
	 * Make sure the Latency Timer is sane.
	 */
	data = pci_read_config(dev, PCIR_LATTIMER, 1);
	if (data < PCI_DFLT_LTNCY) {
		data = PCI_DFLT_LTNCY;
		isp_prt(isp, ISP_LOGDEBUG0, "set PCI latency to %d", data);
		pci_write_config(dev, PCIR_LATTIMER, data, 1);
	}

	/*
	 * Make sure we've disabled the ROM.
	 */
	data = pci_read_config(dev, PCIR_ROMADDR, 4);
	data &= ~1;
	pci_write_config(dev, PCIR_ROMADDR, data, 4);

	/*
	 * Last minute checks...
	 */
	isp->isp_port = pci_get_function(dev);

	/*
	 * Make sure we're in reset state.
	 */
	ISP_LOCK(isp);
	if (isp_reinit(isp, 1) != 0) {
		ISP_UNLOCK(isp);
		goto bad;
	}
	ISP_UNLOCK(isp);
	if (isp_attach(isp)) {
		ISP_LOCK(isp);
		isp_shutdown(isp);
		ISP_UNLOCK(isp);
		goto bad;
	}
	return (0);

bad:
	for (i = 0; i < isp->isp_nirq; i++) {
		(void) bus_teardown_intr(dev, pcs->irq[i].irq, pcs->irq[i].ih);
		(void) bus_release_resource(dev, SYS_RES_IRQ, pcs->irq[i].iqd,
		    pcs->irq[0].irq);
	}
	if (pcs->msicount) {
		pci_release_msi(dev);
	}
	if (pcs->regs)
		(void) bus_release_resource(dev, pcs->rtp, pcs->rgd, pcs->regs);
	if (pcs->regs1)
		(void) bus_release_resource(dev, pcs->rtp1, pcs->rgd1, pcs->regs1);
	if (pcs->regs2)
		(void) bus_release_resource(dev, pcs->rtp2, pcs->rgd2, pcs->regs2);
	if (pcs->pci_isp.isp_param) {
		free(pcs->pci_isp.isp_param, M_DEVBUF);
		pcs->pci_isp.isp_param = NULL;
	}
	if (pcs->pci_isp.isp_osinfo.fc) {
		free(pcs->pci_isp.isp_osinfo.fc, M_DEVBUF);
		pcs->pci_isp.isp_osinfo.fc = NULL;
	}
	mtx_destroy(&isp->isp_lock);
	return (ENXIO);
}

static int
isp_pci_detach(device_t dev)
{
	struct isp_pcisoftc *pcs = device_get_softc(dev);
	ispsoftc_t *isp = &pcs->pci_isp;
	int i, status;

	status = isp_detach(isp);
	if (status)
		return (status);
	ISP_LOCK(isp);
	isp_shutdown(isp);
	ISP_UNLOCK(isp);
	for (i = 0; i < isp->isp_nirq; i++) {
		(void) bus_teardown_intr(dev, pcs->irq[i].irq, pcs->irq[i].ih);
		(void) bus_release_resource(dev, SYS_RES_IRQ, pcs->irq[i].iqd,
		    pcs->irq[i].irq);
	}
	if (pcs->msicount)
		pci_release_msi(dev);
	(void) bus_release_resource(dev, pcs->rtp, pcs->rgd, pcs->regs);
	if (pcs->regs1)
		(void) bus_release_resource(dev, pcs->rtp1, pcs->rgd1, pcs->regs1);
	if (pcs->regs2)
		(void) bus_release_resource(dev, pcs->rtp2, pcs->rgd2, pcs->regs2);
	isp_pci_mbxdmafree(isp);
	if (pcs->pci_isp.isp_param) {
		free(pcs->pci_isp.isp_param, M_DEVBUF);
		pcs->pci_isp.isp_param = NULL;
	}
	if (pcs->pci_isp.isp_osinfo.fc) {
		free(pcs->pci_isp.isp_osinfo.fc, M_DEVBUF);
		pcs->pci_isp.isp_osinfo.fc = NULL;
	}
	mtx_destroy(&isp->isp_lock);
	return (0);
}

#define	BXR2(isp, off)		bus_read_2((isp)->isp_regs, (off))
#define	BXW2(isp, off, v)	bus_write_2((isp)->isp_regs, (off), (v))
#define	BXR4(isp, off)		bus_read_4((isp)->isp_regs, (off))
#define	BXW4(isp, off, v)	bus_write_4((isp)->isp_regs, (off), (v))
#define	B2R4(isp, off)		bus_read_4((isp)->isp_regs2, (off))
#define	B2W4(isp, off, v)	bus_write_4((isp)->isp_regs2, (off), (v))

static void
isp_pci_run_isr_2400(ispsoftc_t *isp)
{
	uint32_t r2hisr;
	uint16_t isr, info;

	r2hisr = BXR4(isp, BIU2400_R2HSTS);
	isp_prt(isp, ISP_LOGDEBUG3, "RISC2HOST ISR 0x%x", r2hisr);
	if ((r2hisr & BIU_R2HST_INTR) == 0)
		return;
	isr = r2hisr & BIU_R2HST_ISTAT_MASK;
	info = (r2hisr >> 16);
	switch (isr) {
	case ISPR2HST_ROM_MBX_OK:
	case ISPR2HST_ROM_MBX_FAIL:
	case ISPR2HST_MBX_OK:
	case ISPR2HST_MBX_FAIL:
		isp_intr_mbox(isp, info);
		break;
	case ISPR2HST_ASYNC_EVENT:
		isp_intr_async(isp, info);
		break;
	case ISPR2HST_RSPQ_UPDATE:
		isp_intr_respq(isp);
		break;
	case ISPR2HST_RSPQ_UPDATE2:
#ifdef	ISP_TARGET_MODE
	case ISPR2HST_ATIO_RSPQ_UPDATE:
#endif
		isp_intr_respq(isp);
		/* FALLTHROUGH */
#ifdef	ISP_TARGET_MODE
	case ISPR2HST_ATIO_UPDATE:
	case ISPR2HST_ATIO_UPDATE2:
		isp_intr_atioq(isp);
#endif
		break;
	default:
		isp_prt(isp, ISP_LOGERR, "unknown interrupt 0x%x\n", r2hisr);
	}
	ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
}

static uint32_t
isp_pci_rd_reg_2400(ispsoftc_t *isp, int regoff)
{
	int block = regoff & _BLK_REG_MASK;

	switch (block) {
	case BIU_BLOCK:
		return (BXR4(isp, regoff));
	case MBOX_BLOCK:
		return (BXR2(isp, regoff));
	}
	isp_prt(isp, ISP_LOGERR, "unknown block read at 0x%x", regoff);
	return (0xffffffff);
}

static void
isp_pci_wr_reg_2400(ispsoftc_t *isp, int regoff, uint32_t val)
{
	int block = regoff & _BLK_REG_MASK;

	switch (block) {
	case BIU_BLOCK:
		BXW4(isp, regoff, val);
#ifdef MEMORYBARRIERW
		if (regoff == BIU2400_REQINP ||
		    regoff == BIU2400_RSPOUTP ||
		    regoff == BIU2400_PRI_REQINP ||
		    regoff == BIU2400_ATIO_RSPOUTP)
			MEMORYBARRIERW(isp, SYNC_REG, regoff, 4, -1)
		else
#endif
		MEMORYBARRIER(isp, SYNC_REG, regoff, 4, -1);
		return;
	case MBOX_BLOCK:
		BXW2(isp, regoff, val);
		MEMORYBARRIER(isp, SYNC_REG, regoff, 2, -1);
		return;
	}
	isp_prt(isp, ISP_LOGERR, "unknown block write at 0x%x", regoff);
}

static uint32_t
isp_pci_rd_reg_2600(ispsoftc_t *isp, int regoff)
{
	uint32_t rv;

	switch (regoff) {
	case BIU2400_PRI_REQINP:
	case BIU2400_PRI_REQOUTP:
		isp_prt(isp, ISP_LOGERR, "unknown register read at 0x%x",
		    regoff);
		rv = 0xffffffff;
		break;
	case BIU2400_REQINP:
		rv = B2R4(isp, 0x00);
		break;
	case BIU2400_REQOUTP:
		rv = B2R4(isp, 0x04);
		break;
	case BIU2400_RSPINP:
		rv = B2R4(isp, 0x08);
		break;
	case BIU2400_RSPOUTP:
		rv = B2R4(isp, 0x0c);
		break;
	case BIU2400_ATIO_RSPINP:
		rv = B2R4(isp, 0x10);
		break;
	case BIU2400_ATIO_RSPOUTP:
		rv = B2R4(isp, 0x14);
		break;
	default:
		rv = isp_pci_rd_reg_2400(isp, regoff);
		break;
	}
	return (rv);
}

static void
isp_pci_wr_reg_2600(ispsoftc_t *isp, int regoff, uint32_t val)
{
	int off;

	switch (regoff) {
	case BIU2400_PRI_REQINP:
	case BIU2400_PRI_REQOUTP:
		isp_prt(isp, ISP_LOGERR, "unknown register write at 0x%x",
		    regoff);
		return;
	case BIU2400_REQINP:
		off = 0x00;
		break;
	case BIU2400_REQOUTP:
		off = 0x04;
		break;
	case BIU2400_RSPINP:
		off = 0x08;
		break;
	case BIU2400_RSPOUTP:
		off = 0x0c;
		break;
	case BIU2400_ATIO_RSPINP:
		off = 0x10;
		break;
	case BIU2400_ATIO_RSPOUTP:
		off = 0x14;
		break;
	default:
		isp_pci_wr_reg_2400(isp, regoff, val);
		return;
	}
	B2W4(isp, off, val);
}


struct imush {
	bus_addr_t maddr;
	int error;
};

static void
imc(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct imush *imushp = (struct imush *) arg;

	if (!(imushp->error = error))
		imushp->maddr = segs[0].ds_addr;
}

static int
isp_pci_mbxdma(ispsoftc_t *isp)
{
	bus_dma_tag_t ptag;
	caddr_t base;
	uint32_t len;
	int i, error, cmap;
	bus_size_t slim;	/* segment size */
	struct imush im;
#ifdef	ISP_TARGET_MODE
	isp_ecmd_t *ecmd;
#endif

	/* Already been here? If so, leave... */
	if (isp->isp_xflist != NULL)
		return (0);
	if (isp->isp_rquest != NULL && isp->isp_maxcmds == 0)
		return (0);
	ISP_UNLOCK(isp);

	ptag = bus_get_dma_tag(isp->isp_osinfo.dev);
	if (sizeof (bus_size_t) > 4)
		slim = (bus_size_t) (1ULL << 32);
	else
		slim = (bus_size_t) (1UL << 31);

	if (isp->isp_rquest != NULL)
		goto gotmaxcmds;

	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	if (bus_dma_tag_create(ptag, QENTRY_LEN, slim,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    len, 1, len, 0, NULL, NULL, &isp->isp_osinfo.reqdmat)) {
		isp_prt(isp, ISP_LOGERR, "cannot create request DMA tag");
		goto bad;
	}
	if (bus_dmamem_alloc(isp->isp_osinfo.reqdmat, (void **)&base,
	    BUS_DMA_COHERENT, &isp->isp_osinfo.reqmap) != 0) {
		isp_prt(isp, ISP_LOGERR, "cannot allocate request DMA memory");
		bus_dma_tag_destroy(isp->isp_osinfo.reqdmat);
		goto bad;
	}
	isp->isp_rquest = base;
	im.error = 0;
	if (bus_dmamap_load(isp->isp_osinfo.reqdmat, isp->isp_osinfo.reqmap,
	    base, len, imc, &im, BUS_DMA_NOWAIT) || im.error) {
		isp_prt(isp, ISP_LOGERR, "error loading request DMA map %d", im.error);
		goto bad;
	}
	isp_prt(isp, ISP_LOGDEBUG0, "request area @ 0x%jx/0x%jx",
	    (uintmax_t)im.maddr, (uintmax_t)len);
	isp->isp_rquest_dma = im.maddr;

#ifdef	ISP_TARGET_MODE
	/*
	 * Allocate region for external DMA addressable command/status structures.
	 */
	len = N_XCMDS * XCMD_SIZE;
	if (bus_dma_tag_create(ptag, XCMD_SIZE, slim,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    len, 1, len, 0, NULL, NULL, &isp->isp_osinfo.ecmd_dmat)) {
		isp_prt(isp, ISP_LOGERR, "cannot create ECMD DMA tag");
		goto bad;
	}
	if (bus_dmamem_alloc(isp->isp_osinfo.ecmd_dmat, (void **)&base,
	    BUS_DMA_COHERENT, &isp->isp_osinfo.ecmd_map) != 0) {
		isp_prt(isp, ISP_LOGERR, "cannot allocate ECMD DMA memory");
		bus_dma_tag_destroy(isp->isp_osinfo.ecmd_dmat);
		goto bad;
	}
	isp->isp_osinfo.ecmd_base = (isp_ecmd_t *)base;
	im.error = 0;
	if (bus_dmamap_load(isp->isp_osinfo.ecmd_dmat, isp->isp_osinfo.ecmd_map,
	    base, len, imc, &im, BUS_DMA_NOWAIT) || im.error) {
		isp_prt(isp, ISP_LOGERR, "error loading ECMD DMA map %d", im.error);
		goto bad;
	}
	isp_prt(isp, ISP_LOGDEBUG0, "ecmd area @ 0x%jx/0x%jx",
	    (uintmax_t)im.maddr, (uintmax_t)len);

	isp->isp_osinfo.ecmd_dma = im.maddr;
	isp->isp_osinfo.ecmd_free = (isp_ecmd_t *)base;
	for (ecmd = isp->isp_osinfo.ecmd_free;
	    ecmd < &isp->isp_osinfo.ecmd_free[N_XCMDS]; ecmd++) {
		if (ecmd == &isp->isp_osinfo.ecmd_free[N_XCMDS - 1])
			ecmd->next = NULL;
		else
			ecmd->next = ecmd + 1;
	}
#endif

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (bus_dma_tag_create(ptag, QENTRY_LEN, slim,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    len, 1, len, 0, NULL, NULL, &isp->isp_osinfo.respdmat)) {
		isp_prt(isp, ISP_LOGERR, "cannot create response DMA tag");
		goto bad;
	}
	if (bus_dmamem_alloc(isp->isp_osinfo.respdmat, (void **)&base,
	    BUS_DMA_COHERENT, &isp->isp_osinfo.respmap) != 0) {
		isp_prt(isp, ISP_LOGERR, "cannot allocate response DMA memory");
		bus_dma_tag_destroy(isp->isp_osinfo.respdmat);
		goto bad;
	}
	isp->isp_result = base;
	im.error = 0;
	if (bus_dmamap_load(isp->isp_osinfo.respdmat, isp->isp_osinfo.respmap,
	    base, len, imc, &im, BUS_DMA_NOWAIT) || im.error) {
		isp_prt(isp, ISP_LOGERR, "error loading response DMA map %d", im.error);
		goto bad;
	}
	isp_prt(isp, ISP_LOGDEBUG0, "response area @ 0x%jx/0x%jx",
	    (uintmax_t)im.maddr, (uintmax_t)len);
	isp->isp_result_dma = im.maddr;

#ifdef	ISP_TARGET_MODE
	/*
	 * Allocate and map ATIO queue.
	 */
	len = ISP_QUEUE_SIZE(ATIO_QUEUE_LEN(isp));
	if (bus_dma_tag_create(ptag, QENTRY_LEN, slim,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    len, 1, len, 0, NULL, NULL, &isp->isp_osinfo.atiodmat)) {
		isp_prt(isp, ISP_LOGERR, "cannot create ATIO DMA tag");
		goto bad;
	}
	if (bus_dmamem_alloc(isp->isp_osinfo.atiodmat, (void **)&base,
	    BUS_DMA_COHERENT, &isp->isp_osinfo.atiomap) != 0) {
		isp_prt(isp, ISP_LOGERR, "cannot allocate ATIO DMA memory");
		bus_dma_tag_destroy(isp->isp_osinfo.atiodmat);
		goto bad;
	}
	isp->isp_atioq = base;
	im.error = 0;
	if (bus_dmamap_load(isp->isp_osinfo.atiodmat, isp->isp_osinfo.atiomap,
	    base, len, imc, &im, BUS_DMA_NOWAIT) || im.error) {
		isp_prt(isp, ISP_LOGERR, "error loading ATIO DMA map %d", im.error);
		goto bad;
	}
	isp_prt(isp, ISP_LOGDEBUG0, "ATIO area @ 0x%jx/0x%jx",
	    (uintmax_t)im.maddr, (uintmax_t)len);
	isp->isp_atioq_dma = im.maddr;
#endif

	if (bus_dma_tag_create(ptag, 64, slim,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    2*QENTRY_LEN, 1, 2*QENTRY_LEN, 0, NULL, NULL,
	    &isp->isp_osinfo.iocbdmat)) {
		goto bad;
	}
	if (bus_dmamem_alloc(isp->isp_osinfo.iocbdmat,
	    (void **)&base, BUS_DMA_COHERENT, &isp->isp_osinfo.iocbmap) != 0)
		goto bad;
	isp->isp_iocb = base;
	im.error = 0;
	if (bus_dmamap_load(isp->isp_osinfo.iocbdmat, isp->isp_osinfo.iocbmap,
	    base, 2*QENTRY_LEN, imc, &im, BUS_DMA_NOWAIT) || im.error)
		goto bad;
	isp->isp_iocb_dma = im.maddr;

	if (bus_dma_tag_create(ptag, 64, slim,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    ISP_FC_SCRLEN, 1, ISP_FC_SCRLEN, 0, NULL, NULL,
	    &isp->isp_osinfo.scdmat))
		goto bad;
	for (cmap = 0; cmap < isp->isp_nchan; cmap++) {
		struct isp_fc *fc = ISP_FC_PC(isp, cmap);
		if (bus_dmamem_alloc(isp->isp_osinfo.scdmat,
		    (void **)&base, BUS_DMA_COHERENT, &fc->scmap) != 0)
			goto bad;
		FCPARAM(isp, cmap)->isp_scratch = base;
		im.error = 0;
		if (bus_dmamap_load(isp->isp_osinfo.scdmat, fc->scmap,
		    base, ISP_FC_SCRLEN, imc, &im, BUS_DMA_NOWAIT) ||
		    im.error) {
			bus_dmamem_free(isp->isp_osinfo.scdmat,
			    base, fc->scmap);
			FCPARAM(isp, cmap)->isp_scratch = NULL;
			goto bad;
		}
		FCPARAM(isp, cmap)->isp_scdma = im.maddr;
		for (i = 0; i < INITIAL_NEXUS_COUNT; i++) {
			struct isp_nexus *n = malloc(sizeof (struct isp_nexus), M_DEVBUF, M_NOWAIT | M_ZERO);
			if (n == NULL) {
				while (fc->nexus_free_list) {
					n = fc->nexus_free_list;
					fc->nexus_free_list = n->next;
					free(n, M_DEVBUF);
				}
				goto bad;
			}
			n->next = fc->nexus_free_list;
			fc->nexus_free_list = n;
		}
	}

	if (isp->isp_maxcmds == 0) {
		ISP_LOCK(isp);
		return (0);
	}

gotmaxcmds:
	if (bus_dma_tag_create(ptag, 1, slim,
	    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
	    (ISP_NSEG64_MAX - 1) * PAGE_SIZE, ISP_NSEG64_MAX,
	    (ISP_NSEG64_MAX - 1) * PAGE_SIZE, 0,
	    busdma_lock_mutex, &isp->isp_lock, &isp->isp_osinfo.dmat))
		goto bad;
	len = isp->isp_maxcmds * sizeof (struct isp_pcmd);
	isp->isp_osinfo.pcmd_pool = (struct isp_pcmd *)
	    malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	for (i = 0; i < isp->isp_maxcmds; i++) {
		struct isp_pcmd *pcmd = &isp->isp_osinfo.pcmd_pool[i];
		error = bus_dmamap_create(isp->isp_osinfo.dmat, 0, &pcmd->dmap);
		if (error) {
			isp_prt(isp, ISP_LOGERR, "error %d creating per-cmd DMA maps", error);
			while (--i >= 0) {
				bus_dmamap_destroy(isp->isp_osinfo.dmat,
				    isp->isp_osinfo.pcmd_pool[i].dmap);
			}
			goto bad;
		}
		callout_init_mtx(&pcmd->wdog, &isp->isp_lock, 0);
		if (i == isp->isp_maxcmds-1)
			pcmd->next = NULL;
		else
			pcmd->next = &isp->isp_osinfo.pcmd_pool[i+1];
	}
	isp->isp_osinfo.pcmd_free = &isp->isp_osinfo.pcmd_pool[0];

	len = sizeof(isp_hdl_t) * ISP_HANDLE_NUM(isp);
	isp->isp_xflist = (isp_hdl_t *) malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	for (len = 0; len < ISP_HANDLE_NUM(isp) - 1; len++)
		isp->isp_xflist[len].cmd = &isp->isp_xflist[len+1];
	isp->isp_xffree = isp->isp_xflist;

	ISP_LOCK(isp);
	return (0);

bad:
	isp_pci_mbxdmafree(isp);
	ISP_LOCK(isp);
	return (1);
}

static void
isp_pci_mbxdmafree(ispsoftc_t *isp)
{
	int i;

	if (isp->isp_xflist != NULL) {
		free(isp->isp_xflist, M_DEVBUF);
		isp->isp_xflist = NULL;
	}
	if (isp->isp_osinfo.pcmd_pool != NULL) {
		for (i = 0; i < isp->isp_maxcmds; i++) {
			bus_dmamap_destroy(isp->isp_osinfo.dmat,
			    isp->isp_osinfo.pcmd_pool[i].dmap);
		}
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		isp->isp_osinfo.pcmd_pool = NULL;
	}
	if (isp->isp_osinfo.dmat) {
		bus_dma_tag_destroy(isp->isp_osinfo.dmat);
		isp->isp_osinfo.dmat = NULL;
	}
	for (i = 0; i < isp->isp_nchan; i++) {
		struct isp_fc *fc = ISP_FC_PC(isp, i);
		if (FCPARAM(isp, i)->isp_scdma != 0) {
			bus_dmamap_unload(isp->isp_osinfo.scdmat,
			    fc->scmap);
			FCPARAM(isp, i)->isp_scdma = 0;
		}
		if (FCPARAM(isp, i)->isp_scratch != NULL) {
			bus_dmamem_free(isp->isp_osinfo.scdmat,
			    FCPARAM(isp, i)->isp_scratch, fc->scmap);
			FCPARAM(isp, i)->isp_scratch = NULL;
		}
		while (fc->nexus_free_list) {
			struct isp_nexus *n = fc->nexus_free_list;
			fc->nexus_free_list = n->next;
			free(n, M_DEVBUF);
		}
	}
	if (isp->isp_osinfo.scdmat) {
		bus_dma_tag_destroy(isp->isp_osinfo.scdmat);
		isp->isp_osinfo.scdmat = NULL;
	}
	if (isp->isp_iocb_dma != 0) {
		bus_dmamap_unload(isp->isp_osinfo.iocbdmat,
		    isp->isp_osinfo.iocbmap);
		isp->isp_iocb_dma = 0;
	}
	if (isp->isp_iocb != NULL) {
		bus_dmamem_free(isp->isp_osinfo.iocbdmat,
		    isp->isp_iocb, isp->isp_osinfo.iocbmap);
		bus_dma_tag_destroy(isp->isp_osinfo.iocbdmat);
	}
#ifdef	ISP_TARGET_MODE
	if (isp->isp_atioq_dma != 0) {
		bus_dmamap_unload(isp->isp_osinfo.atiodmat,
		    isp->isp_osinfo.atiomap);
		isp->isp_atioq_dma = 0;
	}
	if (isp->isp_atioq != NULL) {
		bus_dmamem_free(isp->isp_osinfo.atiodmat, isp->isp_atioq,
		    isp->isp_osinfo.atiomap);
		bus_dma_tag_destroy(isp->isp_osinfo.atiodmat);
		isp->isp_atioq = NULL;
	}
#endif
	if (isp->isp_result_dma != 0) {
		bus_dmamap_unload(isp->isp_osinfo.respdmat,
		    isp->isp_osinfo.respmap);
		isp->isp_result_dma = 0;
	}
	if (isp->isp_result != NULL) {
		bus_dmamem_free(isp->isp_osinfo.respdmat, isp->isp_result,
		    isp->isp_osinfo.respmap);
		bus_dma_tag_destroy(isp->isp_osinfo.respdmat);
		isp->isp_result = NULL;
	}
#ifdef	ISP_TARGET_MODE
	if (isp->isp_osinfo.ecmd_dma != 0) {
		bus_dmamap_unload(isp->isp_osinfo.ecmd_dmat,
		    isp->isp_osinfo.ecmd_map);
		isp->isp_osinfo.ecmd_dma = 0;
	}
	if (isp->isp_osinfo.ecmd_base != NULL) {
		bus_dmamem_free(isp->isp_osinfo.ecmd_dmat, isp->isp_osinfo.ecmd_base,
		    isp->isp_osinfo.ecmd_map);
		bus_dma_tag_destroy(isp->isp_osinfo.ecmd_dmat);
		isp->isp_osinfo.ecmd_base = NULL;
	}
#endif
	if (isp->isp_rquest_dma != 0) {
		bus_dmamap_unload(isp->isp_osinfo.reqdmat,
		    isp->isp_osinfo.reqmap);
		isp->isp_rquest_dma = 0;
	}
	if (isp->isp_rquest != NULL) {
		bus_dmamem_free(isp->isp_osinfo.reqdmat, isp->isp_rquest,
		    isp->isp_osinfo.reqmap);
		bus_dma_tag_destroy(isp->isp_osinfo.reqdmat);
		isp->isp_rquest = NULL;
	}
}

static int
isp_pci_irqsetup(ispsoftc_t *isp)
{
	device_t dev = isp->isp_osinfo.dev;
	struct isp_pcisoftc *pcs = device_get_softc(dev);
	driver_intr_t *f;
	int i, max_irq;

	/* Allocate IRQs only once. */
	if (isp->isp_nirq > 0)
		return (0);

	ISP_UNLOCK(isp);
	if (ISP_CAP_MSIX(isp)) {
		max_irq = IS_26XX(isp) ? 3 : (IS_25XX(isp) ? 2 : 0);
		resource_int_value(device_get_name(dev),
		    device_get_unit(dev), "msix", &max_irq);
		max_irq = imin(ISP_MAX_IRQS, max_irq);
		pcs->msicount = imin(pci_msix_count(dev), max_irq);
		if (pcs->msicount > 0 &&
		    pci_alloc_msix(dev, &pcs->msicount) != 0)
			pcs->msicount = 0;
	}
	if (pcs->msicount == 0) {
		max_irq = 1;
		resource_int_value(device_get_name(dev),
		    device_get_unit(dev), "msi", &max_irq);
		max_irq = imin(1, max_irq);
		pcs->msicount = imin(pci_msi_count(dev), max_irq);
		if (pcs->msicount > 0 &&
		    pci_alloc_msi(dev, &pcs->msicount) != 0)
			pcs->msicount = 0;
	}
	for (i = 0; i < MAX(1, pcs->msicount); i++) {
		pcs->irq[i].iqd = i + (pcs->msicount > 0);
		pcs->irq[i].irq = bus_alloc_resource_any(dev, SYS_RES_IRQ,
		    &pcs->irq[i].iqd, RF_ACTIVE | RF_SHAREABLE);
		if (pcs->irq[i].irq == NULL) {
			device_printf(dev, "could not allocate interrupt\n");
			break;
		}
		if (i == 0)
			f = isp_platform_intr;
		else if (i == 1)
			f = isp_platform_intr_resp;
		else
			f = isp_platform_intr_atio;
		if (bus_setup_intr(dev, pcs->irq[i].irq, ISP_IFLAGS, NULL,
		    f, isp, &pcs->irq[i].ih)) {
			device_printf(dev, "could not setup interrupt\n");
			(void) bus_release_resource(dev, SYS_RES_IRQ,
			    pcs->irq[i].iqd, pcs->irq[i].irq);
			break;
		}
		if (pcs->msicount > 1) {
			bus_describe_intr(dev, pcs->irq[i].irq, pcs->irq[i].ih,
			    "%d", i);
		}
		isp->isp_nirq = i + 1;
	}
	ISP_LOCK(isp);

	return (isp->isp_nirq == 0);
}
