/*-
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
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/bus.h>
#include <sys/stdint.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/malloc.h>
#include <sys/uio.h>

#ifdef __sparc64__
#include <dev/ofw/openfirm.h>
#include <machine/ofw_machdep.h>
#endif

#include <dev/isp/isp_freebsd.h>

static uint32_t isp_pci_rd_reg(ispsoftc_t *, int);
static void isp_pci_wr_reg(ispsoftc_t *, int, uint32_t);
static uint32_t isp_pci_rd_reg_1080(ispsoftc_t *, int);
static void isp_pci_wr_reg_1080(ispsoftc_t *, int, uint32_t);
static uint32_t isp_pci_rd_reg_2400(ispsoftc_t *, int);
static void isp_pci_wr_reg_2400(ispsoftc_t *, int, uint32_t);
static uint32_t isp_pci_rd_reg_2600(ispsoftc_t *, int);
static void isp_pci_wr_reg_2600(ispsoftc_t *, int, uint32_t);
static int isp_pci_rd_isr(ispsoftc_t *, uint16_t *, uint16_t *, uint16_t *);
static int isp_pci_rd_isr_2300(ispsoftc_t *, uint16_t *, uint16_t *, uint16_t *);
static int isp_pci_rd_isr_2400(ispsoftc_t *, uint16_t *, uint16_t *, uint16_t *);
static int isp_pci_mbxdma(ispsoftc_t *);
static int isp_pci_dmasetup(ispsoftc_t *, XS_T *, void *);


static void isp_pci_reset0(ispsoftc_t *);
static void isp_pci_reset1(ispsoftc_t *);
static void isp_pci_dumpregs(ispsoftc_t *, const char *);

static struct ispmdvec mdvec = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	NULL,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};

static struct ispmdvec mdvec_1080 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	NULL,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};

static struct ispmdvec mdvec_12160 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs,
	NULL,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};

static struct ispmdvec mdvec_2100 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs
};

static struct ispmdvec mdvec_2200 = {
	isp_pci_rd_isr,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs
};

static struct ispmdvec mdvec_2300 = {
	isp_pci_rd_isr_2300,
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	isp_pci_dumpregs
};

static struct ispmdvec mdvec_2400 = {
	isp_pci_rd_isr_2400,
	isp_pci_rd_reg_2400,
	isp_pci_wr_reg_2400,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	NULL
};

static struct ispmdvec mdvec_2500 = {
	isp_pci_rd_isr_2400,
	isp_pci_rd_reg_2400,
	isp_pci_wr_reg_2400,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
	NULL
};

static struct ispmdvec mdvec_2600 = {
	isp_pci_rd_isr_2400,
	isp_pci_rd_reg_2600,
	isp_pci_wr_reg_2600,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	isp_common_dmateardown,
	isp_pci_reset0,
	isp_pci_reset1,
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

#ifndef	PCI_VENDOR_QLOGIC
#define	PCI_VENDOR_QLOGIC		0x1077
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1020
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1080
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP10160
#define	PCI_PRODUCT_QLOGIC_ISP10160	0x1016
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP12160
#define	PCI_PRODUCT_QLOGIC_ISP12160	0x1216
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1240
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1280
#define	PCI_PRODUCT_QLOGIC_ISP1280	0x1280
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2100
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2200
#define	PCI_PRODUCT_QLOGIC_ISP2200	0x2200
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2300
#define	PCI_PRODUCT_QLOGIC_ISP2300	0x2300
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2312
#define	PCI_PRODUCT_QLOGIC_ISP2312	0x2312
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2322
#define	PCI_PRODUCT_QLOGIC_ISP2322	0x2322
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2422
#define	PCI_PRODUCT_QLOGIC_ISP2422	0x2422
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2432
#define	PCI_PRODUCT_QLOGIC_ISP2432	0x2432
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2532
#define	PCI_PRODUCT_QLOGIC_ISP2532	0x2532
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP6312
#define	PCI_PRODUCT_QLOGIC_ISP6312	0x6312
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP6322
#define	PCI_PRODUCT_QLOGIC_ISP6322	0x6322
#endif

#ifndef        PCI_PRODUCT_QLOGIC_ISP5432
#define        PCI_PRODUCT_QLOGIC_ISP5432      0x5432
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2031
#define	PCI_PRODUCT_QLOGIC_ISP2031	0x2031
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP8031
#define	PCI_PRODUCT_QLOGIC_ISP8031	0x8031
#endif

#define        PCI_QLOGIC_ISP5432      \
       ((PCI_PRODUCT_QLOGIC_ISP5432 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1020	\
	((PCI_PRODUCT_QLOGIC_ISP1020 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1080	\
	((PCI_PRODUCT_QLOGIC_ISP1080 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP10160	\
	((PCI_PRODUCT_QLOGIC_ISP10160 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP12160	\
	((PCI_PRODUCT_QLOGIC_ISP12160 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1240	\
	((PCI_PRODUCT_QLOGIC_ISP1240 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1280	\
	((PCI_PRODUCT_QLOGIC_ISP1280 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2100	\
	((PCI_PRODUCT_QLOGIC_ISP2100 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2200	\
	((PCI_PRODUCT_QLOGIC_ISP2200 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2300	\
	((PCI_PRODUCT_QLOGIC_ISP2300 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2312	\
	((PCI_PRODUCT_QLOGIC_ISP2312 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2322	\
	((PCI_PRODUCT_QLOGIC_ISP2322 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2422	\
	((PCI_PRODUCT_QLOGIC_ISP2422 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2432	\
	((PCI_PRODUCT_QLOGIC_ISP2432 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2532	\
	((PCI_PRODUCT_QLOGIC_ISP2532 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP6312	\
	((PCI_PRODUCT_QLOGIC_ISP6312 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP6322	\
	((PCI_PRODUCT_QLOGIC_ISP6322 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2031	\
	((PCI_PRODUCT_QLOGIC_ISP2031 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP8031	\
	((PCI_PRODUCT_QLOGIC_ISP8031 << 16) | PCI_VENDOR_QLOGIC)

/*
 * Odd case for some AMI raid cards... We need to *not* attach to this.
 */
#define	AMI_RAID_SUBVENDOR_ID	0x101e

#define	PCI_DFLT_LTNCY	0x40
#define	PCI_DFLT_LNSZ	0x10

static int isp_pci_probe (device_t);
static int isp_pci_attach (device_t);
static int isp_pci_detach (device_t);


#define	ISP_PCD(isp)	((struct isp_pcisoftc *)isp)->pci_dev
struct isp_pcisoftc {
	ispsoftc_t			pci_isp;
	device_t			pci_dev;
	struct resource *		regs;
	struct resource *		regs1;
	struct resource *		regs2;
	void *				irq;
	int				iqd;
	int				rtp;
	int				rgd;
	int				rtp1;
	int				rgd1;
	int				rtp2;
	int				rgd2;
	void *				ih;
	int16_t				pci_poff[_NREG_BLKS];
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
	case PCI_QLOGIC_ISP1020:
		device_set_desc(dev, "Qlogic ISP 1020/1040 PCI SCSI Adapter");
		break;
	case PCI_QLOGIC_ISP1080:
		device_set_desc(dev, "Qlogic ISP 1080 PCI SCSI Adapter");
		break;
	case PCI_QLOGIC_ISP1240:
		device_set_desc(dev, "Qlogic ISP 1240 PCI SCSI Adapter");
		break;
	case PCI_QLOGIC_ISP1280:
		device_set_desc(dev, "Qlogic ISP 1280 PCI SCSI Adapter");
		break;
	case PCI_QLOGIC_ISP10160:
		device_set_desc(dev, "Qlogic ISP 10160 PCI SCSI Adapter");
		break;
	case PCI_QLOGIC_ISP12160:
		if (pci_get_subvendor(dev) == AMI_RAID_SUBVENDOR_ID) {
			return (ENXIO);
		}
		device_set_desc(dev, "Qlogic ISP 12160 PCI SCSI Adapter");
		break;
	case PCI_QLOGIC_ISP2100:
		device_set_desc(dev, "Qlogic ISP 2100 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP2200:
		device_set_desc(dev, "Qlogic ISP 2200 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP2300:
		device_set_desc(dev, "Qlogic ISP 2300 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP2312:
		device_set_desc(dev, "Qlogic ISP 2312 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP2322:
		device_set_desc(dev, "Qlogic ISP 2322 PCI FC-AL Adapter");
		break;
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
	case PCI_QLOGIC_ISP6312:
		device_set_desc(dev, "Qlogic ISP 6312 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP6322:
		device_set_desc(dev, "Qlogic ISP 6322 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP2031:
		device_set_desc(dev, "Qlogic ISP 2031 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP8031:
		device_set_desc(dev, "Qlogic ISP 8031 PCI FCoE Adapter");
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
		if (IS_FC(isp)) {
			ISP_FC_PC(isp, chan)->default_id = 109 - chan;
		} else {
#ifdef __sparc64__
			ISP_SPI_PC(isp, chan)->iid = OF_getscsinitid(dev);
#else
			ISP_SPI_PC(isp, chan)->iid = 7;
#endif
		}
	} else {
		if (IS_FC(isp)) {
			ISP_FC_PC(isp, chan)->default_id = tval - chan;
		} else {
			ISP_SPI_PC(isp, chan)->iid = tval;
		}
		isp->isp_confopts |= ISP_CFG_OWNLOOPID;
	}

	if (IS_SCSI(isp))
		return;

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
	sptr = 0;
	snprintf(name, sizeof(name), "%stopology", prefix);
	if (resource_string_value(device_get_name(dev), device_get_unit(dev),
	    name, (const char **) &sptr) == 0 && sptr != 0) {
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

	tval = 0;
	snprintf(name, sizeof(name), "%snofctape", prefix);
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    name, &tval);
	if (tval) {
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
	sptr = 0;
	snprintf(name, sizeof(name), "%sportwwn", prefix);
	tval = resource_string_value(device_get_name(dev), device_get_unit(dev),
	    name, (const char **) &sptr);
	if (tval == 0 && sptr != 0 && *sptr++ == 'w') {
		char *eptr = 0;
		ISP_FC_PC(isp, chan)->def_wwpn = strtouq(sptr, &eptr, 16);
		if (eptr < sptr + 16 || ISP_FC_PC(isp, chan)->def_wwpn == -1) {
			device_printf(dev, "mangled portwwn hint '%s'\n", sptr);
			ISP_FC_PC(isp, chan)->def_wwpn = 0;
		}
	}

	sptr = 0;
	snprintf(name, sizeof(name), "%snodewwn", prefix);
	tval = resource_string_value(device_get_name(dev), device_get_unit(dev),
	    name, (const char **) &sptr);
	if (tval == 0 && sptr != 0 && *sptr++ == 'w') {
		char *eptr = 0;
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
	int i, locksetup = 0;
	uint32_t data, cmd, linesz, did;
	struct isp_pcisoftc *pcs;
	ispsoftc_t *isp;
	size_t psize, xsize;
	char fwname[32];

	pcs = device_get_softc(dev);
	if (pcs == NULL) {
		device_printf(dev, "cannot get softc\n");
		return (ENOMEM);
	}
	memset(pcs, 0, sizeof (*pcs));

	pcs->pci_dev = dev;
	isp = &pcs->pci_isp;
	isp->isp_dev = dev;
	isp->isp_nchan = 1;
	if (sizeof (bus_addr_t) > 4)
		isp->isp_osinfo.sixtyfourbit = 1;

	/*
	 * Get Generic Options
	 */
	isp_nvports = 0;
	isp_get_generic_options(dev, isp);

	linesz = PCI_DFLT_LNSZ;
	pcs->irq = pcs->regs = pcs->regs2 = NULL;
	pcs->rgd = pcs->rtp = pcs->iqd = 0;

	pcs->pci_dev = dev;
	pcs->pci_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS_OFF;
	pcs->pci_poff[SXP_BLOCK >> _BLK_REG_SHFT] = PCI_SXP_REGS_OFF;
	pcs->pci_poff[RISC_BLOCK >> _BLK_REG_SHFT] = PCI_RISC_REGS_OFF;
	pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;

	switch (pci_get_devid(dev)) {
	case PCI_QLOGIC_ISP1020:
		did = 0x1040;
		isp->isp_mdvec = &mdvec;
		isp->isp_type = ISP_HA_SCSI_UNKNOWN;
		break;
	case PCI_QLOGIC_ISP1080:
		did = 0x1080;
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1080;
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = ISP1080_DMA_REGS_OFF;
		break;
	case PCI_QLOGIC_ISP1240:
		did = 0x1080;
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1240;
		isp->isp_nchan = 2;
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = ISP1080_DMA_REGS_OFF;
		break;
	case PCI_QLOGIC_ISP1280:
		did = 0x1080;
		isp->isp_mdvec = &mdvec_1080;
		isp->isp_type = ISP_HA_SCSI_1280;
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = ISP1080_DMA_REGS_OFF;
		break;
	case PCI_QLOGIC_ISP10160:
		did = 0x12160;
		isp->isp_mdvec = &mdvec_12160;
		isp->isp_type = ISP_HA_SCSI_10160;
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = ISP1080_DMA_REGS_OFF;
		break;
	case PCI_QLOGIC_ISP12160:
		did = 0x12160;
		isp->isp_nchan = 2;
		isp->isp_mdvec = &mdvec_12160;
		isp->isp_type = ISP_HA_SCSI_12160;
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = ISP1080_DMA_REGS_OFF;
		break;
	case PCI_QLOGIC_ISP2100:
		did = 0x2100;
		isp->isp_mdvec = &mdvec_2100;
		isp->isp_type = ISP_HA_FC_2100;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2100_OFF;
		if (pci_get_revid(dev) < 3) {
			/*
			 * XXX: Need to get the actual revision
			 * XXX: number of the 2100 FB. At any rate,
			 * XXX: lower cache line size for early revision
			 * XXX; boards.
			 */
			linesz = 1;
		}
		break;
	case PCI_QLOGIC_ISP2200:
		did = 0x2200;
		isp->isp_mdvec = &mdvec_2200;
		isp->isp_type = ISP_HA_FC_2200;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2100_OFF;
		break;
	case PCI_QLOGIC_ISP2300:
		did = 0x2300;
		isp->isp_mdvec = &mdvec_2300;
		isp->isp_type = ISP_HA_FC_2300;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2300_OFF;
		break;
	case PCI_QLOGIC_ISP2312:
	case PCI_QLOGIC_ISP6312:
		did = 0x2300;
		isp->isp_mdvec = &mdvec_2300;
		isp->isp_type = ISP_HA_FC_2312;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2300_OFF;
		break;
	case PCI_QLOGIC_ISP2322:
	case PCI_QLOGIC_ISP6322:
		did = 0x2322;
		isp->isp_mdvec = &mdvec_2300;
		isp->isp_type = ISP_HA_FC_2322;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2300_OFF;
		break;
	case PCI_QLOGIC_ISP2422:
	case PCI_QLOGIC_ISP2432:
		did = 0x2400;
		isp->isp_nchan += isp_nvports;
		isp->isp_mdvec = &mdvec_2400;
		isp->isp_type = ISP_HA_FC_2400;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2400_OFF;
		break;
	case PCI_QLOGIC_ISP2532:
		did = 0x2500;
		isp->isp_nchan += isp_nvports;
		isp->isp_mdvec = &mdvec_2500;
		isp->isp_type = ISP_HA_FC_2500;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2400_OFF;
		break;
	case PCI_QLOGIC_ISP5432:
		did = 0x2500;
		isp->isp_mdvec = &mdvec_2500;
		isp->isp_type = ISP_HA_FC_2500;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2400_OFF;
		break;
	case PCI_QLOGIC_ISP2031:
	case PCI_QLOGIC_ISP8031:
		did = 0x2600;
		isp->isp_nchan += isp_nvports;
		isp->isp_mdvec = &mdvec_2600;
		isp->isp_type = ISP_HA_FC_2600;
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS2400_OFF;
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

	if (IS_FC(isp)) {
		psize = sizeof (fcparam);
		xsize = sizeof (struct isp_fc);
	} else {
		psize = sizeof (sdparam);
		xsize = sizeof (struct isp_spi);
	}
	psize *= isp->isp_nchan;
	xsize *= isp->isp_nchan;
	isp->isp_param = malloc(psize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (isp->isp_param == NULL) {
		device_printf(dev, "cannot allocate parameter data\n");
		goto bad;
	}
	isp->isp_osinfo.pc.ptr = malloc(xsize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (isp->isp_osinfo.pc.ptr == NULL) {
		device_printf(dev, "cannot allocate parameter data\n");
		goto bad;
	}

	/*
	 * Now that we know who we are (roughly) get/set specific options
	 */
	for (i = 0; i < isp->isp_nchan; i++) {
		isp_get_specific_options(dev, i, isp);
	}

	isp->isp_osinfo.fw = NULL;
	if (isp->isp_osinfo.fw == NULL) {
		snprintf(fwname, sizeof (fwname), "isp_%04x", did);
		isp->isp_osinfo.fw = firmware_get(fwname);
	}
	if (isp->isp_osinfo.fw != NULL) {
		isp_prt(isp, ISP_LOGCONFIG, "loaded firmware %s", fwname);
		isp->isp_mdvec->dv_ispfw = isp->isp_osinfo.fw->data;
	}

	/*
	 * Make sure that SERR, PERR, WRITE INVALIDATE and BUSMASTER are set.
	 */
	cmd = pci_read_config(dev, PCIR_COMMAND, 2);
	cmd |= PCIM_CMD_SEREN | PCIM_CMD_PERRESPEN | PCIM_CMD_BUSMASTEREN | PCIM_CMD_INVEN;
	if (IS_2300(isp)) {	/* per QLogic errata */
		cmd &= ~PCIM_CMD_INVEN;
	}
	if (IS_2322(isp) || pci_get_devid(dev) == PCI_QLOGIC_ISP6312) {
		cmd &= ~PCIM_CMD_INTX_DISABLE;
	}
	if (IS_24XX(isp)) {
		cmd &= ~PCIM_CMD_INTX_DISABLE;
	}
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

	if (IS_26XX(isp)) {
		/* 26XX chips support only MSI-X, so start from them. */
		pcs->msicount = imin(pci_msix_count(dev), 1);
		if (pcs->msicount > 0 &&
		    (i = pci_alloc_msix(dev, &pcs->msicount)) == 0) {
			pcs->iqd = 1;
		} else {
			pcs->msicount = 0;
		}
	}
	if (pcs->msicount == 0 && (IS_24XX(isp) || IS_2322(isp))) {
		/*
		 * Older chips support both MSI and MSI-X, but I have
		 * feeling that older firmware may not support MSI-X,
		 * but we have no way to check the firmware flag here.
		 */
		pcs->msicount = imin(pci_msi_count(dev), 1);
		if (pcs->msicount > 0 &&
		    pci_alloc_msi(dev, &pcs->msicount) == 0) {
			pcs->iqd = 1;
		} else {
			pcs->msicount = 0;
		}
	}
	pcs->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &pcs->iqd, RF_ACTIVE | RF_SHAREABLE);
	if (pcs->irq == NULL) {
		device_printf(dev, "could not allocate interrupt\n");
		goto bad;
	}

	/* Make sure the lock is set up. */
	mtx_init(&isp->isp_osinfo.lock, "isp", NULL, MTX_DEF);
	locksetup++;

	if (isp_setup_intr(dev, pcs->irq, ISP_IFLAGS, NULL, isp_platform_intr, isp, &pcs->ih)) {
		device_printf(dev, "could not setup interrupt\n");
		goto bad;
	}

	/*
	 * Last minute checks...
	 */
	if (IS_23XX(isp) || IS_24XX(isp)) {
		isp->isp_port = pci_get_function(dev);
	}

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
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		goto bad;
	}
	return (0);

bad:
	if (pcs->ih) {
		(void) bus_teardown_intr(dev, pcs->irq, pcs->ih);
	}
	if (locksetup) {
		mtx_destroy(&isp->isp_osinfo.lock);
	}
	if (pcs->irq) {
		(void) bus_release_resource(dev, SYS_RES_IRQ, pcs->iqd, pcs->irq);
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
	if (pcs->pci_isp.isp_osinfo.pc.ptr) {
		free(pcs->pci_isp.isp_osinfo.pc.ptr, M_DEVBUF);
		pcs->pci_isp.isp_osinfo.pc.ptr = NULL;
	}
	return (ENXIO);
}

static int
isp_pci_detach(device_t dev)
{
	struct isp_pcisoftc *pcs;
	ispsoftc_t *isp;
	int status;

	pcs = device_get_softc(dev);
	if (pcs == NULL) {
		return (ENXIO);
	}
	isp = (ispsoftc_t *) pcs;
	status = isp_detach(isp);
	if (status)
		return (status);
	ISP_LOCK(isp);
	isp_uninit(isp);
	if (pcs->ih) {
		(void) bus_teardown_intr(dev, pcs->irq, pcs->ih);
	}
	ISP_UNLOCK(isp);
	mtx_destroy(&isp->isp_osinfo.lock);
	(void) bus_release_resource(dev, SYS_RES_IRQ, pcs->iqd, pcs->irq);
	if (pcs->msicount) {
		pci_release_msi(dev);
	}
	(void) bus_release_resource(dev, pcs->rtp, pcs->rgd, pcs->regs);
	if (pcs->regs1)
		(void) bus_release_resource(dev, pcs->rtp1, pcs->rgd1, pcs->regs1);
	if (pcs->regs2)
		(void) bus_release_resource(dev, pcs->rtp2, pcs->rgd2, pcs->regs2);
	/*
	 * XXX: THERE IS A LOT OF LEAKAGE HERE
	 */
	if (pcs->pci_isp.isp_param) {
		free(pcs->pci_isp.isp_param, M_DEVBUF);
		pcs->pci_isp.isp_param = NULL;
	}
	if (pcs->pci_isp.isp_osinfo.pc.ptr) {
		free(pcs->pci_isp.isp_osinfo.pc.ptr, M_DEVBUF);
		pcs->pci_isp.isp_osinfo.pc.ptr = NULL;
	}
	return (0);
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_pcisoftc *)a)->pci_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xfff))

#define	BXR2(isp, off)		bus_read_2((isp)->isp_regs, (off))
#define	BXW2(isp, off, v)	bus_write_2((isp)->isp_regs, (off), (v))
#define	BXR4(isp, off)		bus_read_4((isp)->isp_regs, (off))
#define	BXW4(isp, off, v)	bus_write_4((isp)->isp_regs, (off), (v))
#define	B2R4(isp, off)		bus_read_4((isp)->isp_regs2, (off))
#define	B2W4(isp, off, v)	bus_write_4((isp)->isp_regs2, (off), (v))

static ISP_INLINE int
isp_pci_rd_debounced(ispsoftc_t *isp, int off, uint16_t *rp)
{
	uint32_t val0, val1;
	int i = 0;

	do {
		val0 = BXR2(isp, IspVirt2Off(isp, off));
		val1 = BXR2(isp, IspVirt2Off(isp, off));
	} while (val0 != val1 && ++i < 1000);
	if (val0 != val1) {
		return (1);
	}
	*rp = val0;
	return (0);
}

static int
isp_pci_rd_isr(ispsoftc_t *isp, uint16_t *isrp, uint16_t *semap, uint16_t *info)
{
	uint16_t isr, sema;

	if (IS_2100(isp)) {
		if (isp_pci_rd_debounced(isp, BIU_ISR, &isr)) {
		    return (0);
		}
		if (isp_pci_rd_debounced(isp, BIU_SEMA, &sema)) {
		    return (0);
		}
	} else {
		isr = BXR2(isp, IspVirt2Off(isp, BIU_ISR));
		sema = BXR2(isp, IspVirt2Off(isp, BIU_SEMA));
	}
	isp_prt(isp, ISP_LOGDEBUG3, "ISR 0x%x SEMA 0x%x", isr, sema);
	isr &= INT_PENDING_MASK(isp);
	sema &= BIU_SEMA_LOCK;
	if (isr == 0 && sema == 0) {
		return (0);
	}
	*isrp = isr;
	if ((*semap = sema) != 0) {
		if (IS_2100(isp)) {
			if (isp_pci_rd_debounced(isp, OUTMAILBOX0, info)) {
				return (0);
			}
		} else {
			*info = BXR2(isp, IspVirt2Off(isp, OUTMAILBOX0));
		}
	}
	return (1);
}

static int
isp_pci_rd_isr_2300(ispsoftc_t *isp, uint16_t *isrp, uint16_t *semap, uint16_t *info)
{
	uint32_t hccr, r2hisr;

	if (!(BXR2(isp, IspVirt2Off(isp, BIU_ISR) & BIU2100_ISR_RISC_INT))) {
		*isrp = 0;
		return (0);
	}
	r2hisr = BXR4(isp, IspVirt2Off(isp, BIU_R2HSTSLO));
	isp_prt(isp, ISP_LOGDEBUG3, "RISC2HOST ISR 0x%x", r2hisr);
	if ((r2hisr & BIU_R2HST_INTR) == 0) {
		*isrp = 0;
		return (0);
	}
	switch ((*isrp = r2hisr & BIU_R2HST_ISTAT_MASK)) {
	case ISPR2HST_ROM_MBX_OK:
	case ISPR2HST_ROM_MBX_FAIL:
	case ISPR2HST_MBX_OK:
	case ISPR2HST_MBX_FAIL:
	case ISPR2HST_ASYNC_EVENT:
		*semap = 1;
		break;
	case ISPR2HST_RIO_16:
		*info = ASYNC_RIO16_1;
		*semap = 1;
		return (1);
	case ISPR2HST_FPOST:
		*info = ASYNC_CMD_CMPLT;
		*semap = 1;
		return (1);
	case ISPR2HST_FPOST_CTIO:
		*info = ASYNC_CTIO_DONE;
		*semap = 1;
		return (1);
	case ISPR2HST_RSPQ_UPDATE:
		*semap = 0;
		break;
	default:
		hccr = ISP_READ(isp, HCCR);
		if (hccr & HCCR_PAUSE) {
			ISP_WRITE(isp, HCCR, HCCR_RESET);
			isp_prt(isp, ISP_LOGERR, "RISC paused at interrupt (%x->%x)", hccr, ISP_READ(isp, HCCR));
			ISP_WRITE(isp, BIU_ICR, 0);
		} else {
			isp_prt(isp, ISP_LOGERR, "unknown interrupt 0x%x\n", r2hisr);
		}
		return (0);
	}
	*info = (r2hisr >> 16);
	return (1);
}

static int
isp_pci_rd_isr_2400(ispsoftc_t *isp, uint16_t *isrp, uint16_t *semap, uint16_t *info)
{
	uint32_t r2hisr;

	r2hisr = BXR4(isp, IspVirt2Off(isp, BIU2400_R2HSTSLO));
	isp_prt(isp, ISP_LOGDEBUG3, "RISC2HOST ISR 0x%x", r2hisr);
	if ((r2hisr & BIU_R2HST_INTR) == 0) {
		*isrp = 0;
		return (0);
	}
	switch ((*isrp = r2hisr & BIU_R2HST_ISTAT_MASK)) {
	case ISPR2HST_ROM_MBX_OK:
	case ISPR2HST_ROM_MBX_FAIL:
	case ISPR2HST_MBX_OK:
	case ISPR2HST_MBX_FAIL:
	case ISPR2HST_ASYNC_EVENT:
		*semap = 1;
		break;
	case ISPR2HST_RSPQ_UPDATE:
	case ISPR2HST_RSPQ_UPDATE2:
	case ISPR2HST_ATIO_UPDATE:
	case ISPR2HST_ATIO_RSPQ_UPDATE:
	case ISPR2HST_ATIO_UPDATE2:
		*semap = 0;
		break;
	default:
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
		isp_prt(isp, ISP_LOGERR, "unknown interrupt 0x%x\n", r2hisr);
		return (0);
	}
	*info = (r2hisr >> 16);
	return (1);
}

static uint32_t
isp_pci_rd_reg(ispsoftc_t *isp, int regoff)
{
	uint16_t rv;
	int oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = BXR2(isp, IspVirt2Off(isp, BIU_CONF1));
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oldconf | BIU_PCI_CONF1_SXP);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	}
	rv = BXR2(isp, IspVirt2Off(isp, regoff));
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oldconf);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	}
	return (rv);
}

static void
isp_pci_wr_reg(ispsoftc_t *isp, int regoff, uint32_t val)
{
	int oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = BXR2(isp, IspVirt2Off(isp, BIU_CONF1));
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1),
		    oldconf | BIU_PCI_CONF1_SXP);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	}
	BXW2(isp, IspVirt2Off(isp, regoff), val);
	MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 2, -1);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oldconf);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	}

}

static uint32_t
isp_pci_rd_reg_1080(ispsoftc_t *isp, int regoff)
{
	uint32_t rv, oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		uint32_t tc;
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = BXR2(isp, IspVirt2Off(isp, BIU_CONF1));
		tc = oc & ~BIU_PCI1080_CONF1_DMA;
		if (regoff & SXP_BANK1_SELECT)
			tc |= BIU_PCI1080_CONF1_SXP1;
		else
			tc |= BIU_PCI1080_CONF1_SXP0;
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), tc);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(isp, IspVirt2Off(isp, BIU_CONF1));
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), 
		    oc | BIU_PCI1080_CONF1_DMA);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	}
	rv = BXR2(isp, IspVirt2Off(isp, regoff));
	if (oc) {
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oc);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	}
	return (rv);
}

static void
isp_pci_wr_reg_1080(ispsoftc_t *isp, int regoff, uint32_t val)
{
	int oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		uint32_t tc;
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = BXR2(isp, IspVirt2Off(isp, BIU_CONF1));
		tc = oc & ~BIU_PCI1080_CONF1_DMA;
		if (regoff & SXP_BANK1_SELECT)
			tc |= BIU_PCI1080_CONF1_SXP1;
		else
			tc |= BIU_PCI1080_CONF1_SXP0;
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), tc);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(isp, IspVirt2Off(isp, BIU_CONF1));
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), 
		    oc | BIU_PCI1080_CONF1_DMA);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	}
	BXW2(isp, IspVirt2Off(isp, regoff), val);
	MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 2, -1);
	if (oc) {
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oc);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2, -1);
	}
}

static uint32_t
isp_pci_rd_reg_2400(ispsoftc_t *isp, int regoff)
{
	uint32_t rv;
	int block = regoff & _BLK_REG_MASK;

	switch (block) {
	case BIU_BLOCK:
		break;
	case MBOX_BLOCK:
		return (BXR2(isp, IspVirt2Off(isp, regoff)));
	case SXP_BLOCK:
		isp_prt(isp, ISP_LOGERR, "SXP_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	case RISC_BLOCK:
		isp_prt(isp, ISP_LOGERR, "RISC_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	case DMA_BLOCK:
		isp_prt(isp, ISP_LOGERR, "DMA_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	default:
		isp_prt(isp, ISP_LOGERR, "unknown block read at 0x%x", regoff);
		return (0xffffffff);
	}

	switch (regoff) {
	case BIU2400_FLASH_ADDR:
	case BIU2400_FLASH_DATA:
	case BIU2400_ICR:
	case BIU2400_ISR:
	case BIU2400_CSR:
	case BIU2400_REQINP:
	case BIU2400_REQOUTP:
	case BIU2400_RSPINP:
	case BIU2400_RSPOUTP:
	case BIU2400_PRI_REQINP:
	case BIU2400_PRI_REQOUTP:
	case BIU2400_ATIO_RSPINP:
	case BIU2400_ATIO_RSPOUTP:
	case BIU2400_HCCR:
	case BIU2400_GPIOD:
	case BIU2400_GPIOE:
	case BIU2400_HSEMA:
		rv = BXR4(isp, IspVirt2Off(isp, regoff));
		break;
	case BIU2400_R2HSTSLO:
		rv = BXR4(isp, IspVirt2Off(isp, regoff));
		break;
	case BIU2400_R2HSTSHI:
		rv = BXR4(isp, IspVirt2Off(isp, regoff)) >> 16;
		break;
	default:
		isp_prt(isp, ISP_LOGERR, "unknown register read at 0x%x",
		    regoff);
		rv = 0xffffffff;
		break;
	}
	return (rv);
}

static void
isp_pci_wr_reg_2400(ispsoftc_t *isp, int regoff, uint32_t val)
{
	int block = regoff & _BLK_REG_MASK;

	switch (block) {
	case BIU_BLOCK:
		break;
	case MBOX_BLOCK:
		BXW2(isp, IspVirt2Off(isp, regoff), val);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 2, -1);
		return;
	case SXP_BLOCK:
		isp_prt(isp, ISP_LOGERR, "SXP_BLOCK write at 0x%x", regoff);
		return;
	case RISC_BLOCK:
		isp_prt(isp, ISP_LOGERR, "RISC_BLOCK write at 0x%x", regoff);
		return;
	case DMA_BLOCK:
		isp_prt(isp, ISP_LOGERR, "DMA_BLOCK write at 0x%x", regoff);
		return;
	default:
		isp_prt(isp, ISP_LOGERR, "unknown block write at 0x%x", regoff);
		break;
	}

	switch (regoff) {
	case BIU2400_FLASH_ADDR:
	case BIU2400_FLASH_DATA:
	case BIU2400_ICR:
	case BIU2400_ISR:
	case BIU2400_CSR:
	case BIU2400_REQINP:
	case BIU2400_REQOUTP:
	case BIU2400_RSPINP:
	case BIU2400_RSPOUTP:
	case BIU2400_PRI_REQINP:
	case BIU2400_PRI_REQOUTP:
	case BIU2400_ATIO_RSPINP:
	case BIU2400_ATIO_RSPOUTP:
	case BIU2400_HCCR:
	case BIU2400_GPIOD:
	case BIU2400_GPIOE:
	case BIU2400_HSEMA:
		BXW4(isp, IspVirt2Off(isp, regoff), val);
#ifdef MEMORYBARRIERW
		if (regoff == BIU2400_REQINP ||
		    regoff == BIU2400_RSPOUTP ||
		    regoff == BIU2400_PRI_REQINP ||
		    regoff == BIU2400_ATIO_RSPOUTP)
			MEMORYBARRIERW(isp, SYNC_REG,
			    IspVirt2Off(isp, regoff), 4, -1)
		else
#endif
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 4, -1);
		break;
	default:
		isp_prt(isp, ISP_LOGERR, "unknown register write at 0x%x",
		    regoff);
		break;
	}
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
	caddr_t base;
	uint32_t len, nsegs;
	int i, error, cmap = 0;
	bus_size_t slim;	/* segment size */
	bus_addr_t llim;	/* low limit of unavailable dma */
	bus_addr_t hlim;	/* high limit of unavailable dma */
	struct imush im;
	isp_ecmd_t *ecmd;

	/*
	 * Already been here? If so, leave...
	 */
	if (isp->isp_rquest) {
		return (0);
	}
	ISP_UNLOCK(isp);

	if (isp->isp_maxcmds == 0) {
		isp_prt(isp, ISP_LOGERR, "maxcmds not set");
		ISP_LOCK(isp);
		return (1);
	}

	hlim = BUS_SPACE_MAXADDR;
	if (IS_ULTRA2(isp) || IS_FC(isp) || IS_1240(isp)) {
		if (sizeof (bus_size_t) > 4) {
			slim = (bus_size_t) (1ULL << 32);
		} else {
			slim = (bus_size_t) (1UL << 31);
		}
		llim = BUS_SPACE_MAXADDR;
	} else {
		llim = BUS_SPACE_MAXADDR_32BIT;
		slim = (1UL << 24);
	}

	len = isp->isp_maxcmds * sizeof (struct isp_pcmd);
	isp->isp_osinfo.pcmd_pool = (struct isp_pcmd *) malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);

	if (isp->isp_osinfo.sixtyfourbit) {
		nsegs = ISP_NSEG64_MAX;
	} else {
		nsegs = ISP_NSEG_MAX;
	}

	if (isp_dma_tag_create(BUS_DMA_ROOTARG(ISP_PCD(isp)), 1, slim, llim, hlim, NULL, NULL, BUS_SPACE_MAXSIZE, nsegs, slim, 0, &isp->isp_osinfo.dmat)) {
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		ISP_LOCK(isp);
		isp_prt(isp, ISP_LOGERR, "could not create master dma tag");
		return (1);
	}

	len = sizeof (isp_hdl_t) * isp->isp_maxcmds;
	isp->isp_xflist = (isp_hdl_t *) malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	for (len = 0; len < isp->isp_maxcmds - 1; len++) {
		isp->isp_xflist[len].cmd = &isp->isp_xflist[len+1];
	}
	isp->isp_xffree = isp->isp_xflist;

	/*
	 * Allocate and map the request queue and a region for external
	 * DMA addressable command/status structures (22XX and later).
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	if (isp->isp_type >= ISP_HA_FC_2200)
		len += (N_XCMDS * XCMD_SIZE);
	if (isp_dma_tag_create(isp->isp_osinfo.dmat, QENTRY_LEN, slim,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    len, 1, len, 0, &isp->isp_osinfo.reqdmat)) {
		isp_prt(isp, ISP_LOGERR, "cannot create request DMA tag");
		goto bad1;
	}
	if (bus_dmamem_alloc(isp->isp_osinfo.reqdmat, (void **)&base,
	    BUS_DMA_COHERENT, &isp->isp_osinfo.reqmap) != 0) {
		isp_prt(isp, ISP_LOGERR, "cannot allocate request DMA memory");
		bus_dma_tag_destroy(isp->isp_osinfo.reqdmat);
		goto bad1;
	}
	isp->isp_rquest = base;
	im.error = 0;
	if (bus_dmamap_load(isp->isp_osinfo.reqdmat, isp->isp_osinfo.reqmap,
	    base, len, imc, &im, 0) || im.error) {
		isp_prt(isp, ISP_LOGERR, "error loading request DMA map %d", im.error);
		goto bad1;
	}
	isp_prt(isp, ISP_LOGDEBUG0, "request area @ 0x%jx/0x%jx",
	    (uintmax_t)im.maddr, (uintmax_t)len);
	isp->isp_rquest_dma = im.maddr;
	base += ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	im.maddr += ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	if (isp->isp_type >= ISP_HA_FC_2200) {
		isp->isp_osinfo.ecmd_dma = im.maddr;
		isp->isp_osinfo.ecmd_free = (isp_ecmd_t *)base;
		isp->isp_osinfo.ecmd_base = isp->isp_osinfo.ecmd_free;
		for (ecmd = isp->isp_osinfo.ecmd_free;
		    ecmd < &isp->isp_osinfo.ecmd_free[N_XCMDS]; ecmd++) {
			if (ecmd == &isp->isp_osinfo.ecmd_free[N_XCMDS - 1])
				ecmd->next = NULL;
			else
				ecmd->next = ecmd + 1;
		}
	}

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (isp_dma_tag_create(isp->isp_osinfo.dmat, QENTRY_LEN, slim,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
	    len, 1, len, 0, &isp->isp_osinfo.respdmat)) {
		isp_prt(isp, ISP_LOGERR, "cannot create response DMA tag");
		goto bad1;
	}
	if (bus_dmamem_alloc(isp->isp_osinfo.respdmat, (void **)&base,
	    BUS_DMA_COHERENT, &isp->isp_osinfo.respmap) != 0) {
		isp_prt(isp, ISP_LOGERR, "cannot allocate response DMA memory");
		bus_dma_tag_destroy(isp->isp_osinfo.respdmat);
		goto bad1;
	}
	isp->isp_result = base;
	im.error = 0;
	if (bus_dmamap_load(isp->isp_osinfo.respdmat, isp->isp_osinfo.respmap,
	    base, len, imc, &im, 0) || im.error) {
		isp_prt(isp, ISP_LOGERR, "error loading response DMA map %d", im.error);
		goto bad1;
	}
	isp_prt(isp, ISP_LOGDEBUG0, "response area @ 0x%jx/0x%jx",
	    (uintmax_t)im.maddr, (uintmax_t)len);
	isp->isp_result_dma = im.maddr;

#ifdef	ISP_TARGET_MODE
	/*
	 * Allocate and map ATIO queue on 24xx with target mode.
	 */
	if (IS_24XX(isp)) {
		len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
		if (isp_dma_tag_create(isp->isp_osinfo.dmat, QENTRY_LEN, slim,
		    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR, NULL, NULL,
		    len, 1, len, 0, &isp->isp_osinfo.atiodmat)) {
			isp_prt(isp, ISP_LOGERR, "cannot create ATIO DMA tag");
			goto bad1;
		}
		if (bus_dmamem_alloc(isp->isp_osinfo.atiodmat, (void **)&base,
		    BUS_DMA_COHERENT, &isp->isp_osinfo.atiomap) != 0) {
			isp_prt(isp, ISP_LOGERR, "cannot allocate ATIO DMA memory");
			bus_dma_tag_destroy(isp->isp_osinfo.atiodmat);
			goto bad1;
		}
		isp->isp_atioq = base;
		im.error = 0;
		if (bus_dmamap_load(isp->isp_osinfo.atiodmat, isp->isp_osinfo.atiomap,
		    base, len, imc, &im, 0) || im.error) {
			isp_prt(isp, ISP_LOGERR, "error loading ATIO DMA map %d", im.error);
			goto bad;
		}
		isp_prt(isp, ISP_LOGDEBUG0, "ATIO area @ 0x%jx/0x%jx",
		    (uintmax_t)im.maddr, (uintmax_t)len);
		isp->isp_atioq_dma = im.maddr;
	}
#endif

	if (IS_FC(isp)) {
		if (isp_dma_tag_create(isp->isp_osinfo.dmat, 64, slim,
		    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
		    2*QENTRY_LEN, 1, 2*QENTRY_LEN, 0, &isp->isp_osinfo.iocbdmat)) {
			goto bad;
		}
		if (bus_dmamem_alloc(isp->isp_osinfo.iocbdmat,
		    (void **)&base, BUS_DMA_COHERENT, &isp->isp_osinfo.iocbmap) != 0)
			goto bad;
		isp->isp_iocb = base;
		im.error = 0;
		if (bus_dmamap_load(isp->isp_osinfo.iocbdmat, isp->isp_osinfo.iocbmap,
		    base, 2*QENTRY_LEN, imc, &im, 0) || im.error)
			goto bad;
		isp->isp_iocb_dma = im.maddr;

		if (isp_dma_tag_create(isp->isp_osinfo.dmat, 64, slim,
		    BUS_SPACE_MAXADDR, BUS_SPACE_MAXADDR, NULL, NULL,
		    ISP_FC_SCRLEN, 1, ISP_FC_SCRLEN, 0, &isp->isp_osinfo.scdmat))
			goto bad;
		for (cmap = 0; cmap < isp->isp_nchan; cmap++) {
			struct isp_fc *fc = ISP_FC_PC(isp, cmap);
			if (bus_dmamem_alloc(isp->isp_osinfo.scdmat,
			    (void **)&base, BUS_DMA_COHERENT, &fc->scmap) != 0)
				goto bad;
			FCPARAM(isp, cmap)->isp_scratch = base;
			im.error = 0;
			if (bus_dmamap_load(isp->isp_osinfo.scdmat, fc->scmap,
			    base, ISP_FC_SCRLEN, imc, &im, 0) || im.error) {
				bus_dmamem_free(isp->isp_osinfo.scdmat,
				    base, fc->scmap);
				goto bad;
			}
			FCPARAM(isp, cmap)->isp_scdma = im.maddr;
			if (!IS_2100(isp)) {
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
		}
	}

	for (i = 0; i < isp->isp_maxcmds; i++) {
		struct isp_pcmd *pcmd = &isp->isp_osinfo.pcmd_pool[i];
		error = bus_dmamap_create(isp->isp_osinfo.dmat, 0, &pcmd->dmap);
		if (error) {
			isp_prt(isp, ISP_LOGERR, "error %d creating per-cmd DMA maps", error);
			while (--i >= 0) {
				bus_dmamap_destroy(isp->isp_osinfo.dmat, isp->isp_osinfo.pcmd_pool[i].dmap);
			}
			goto bad;
		}
		callout_init_mtx(&pcmd->wdog, &isp->isp_osinfo.lock, 0);
		if (i == isp->isp_maxcmds-1) {
			pcmd->next = NULL;
		} else {
			pcmd->next = &isp->isp_osinfo.pcmd_pool[i+1];
		}
	}
	isp->isp_osinfo.pcmd_free = &isp->isp_osinfo.pcmd_pool[0];
	ISP_LOCK(isp);
	return (0);

bad:
	if (IS_FC(isp)) {
		while (--cmap >= 0) {
			struct isp_fc *fc = ISP_FC_PC(isp, cmap);
			bus_dmamap_unload(isp->isp_osinfo.scdmat, fc->scmap);
			bus_dmamem_free(isp->isp_osinfo.scdmat,
			    FCPARAM(isp, cmap)->isp_scratch, fc->scmap);
			while (fc->nexus_free_list) {
				struct isp_nexus *n = fc->nexus_free_list;
				fc->nexus_free_list = n->next;
				free(n, M_DEVBUF);
			}
		}
		bus_dma_tag_destroy(isp->isp_osinfo.scdmat);
		bus_dmamap_unload(isp->isp_osinfo.iocbdmat, isp->isp_osinfo.iocbmap);
		bus_dmamem_free(isp->isp_osinfo.iocbdmat, isp->isp_iocb,
		    isp->isp_osinfo.iocbmap);
		bus_dma_tag_destroy(isp->isp_osinfo.iocbdmat);
	}
bad1:
	if (isp->isp_rquest_dma != 0) {
		bus_dmamap_unload(isp->isp_osinfo.reqdmat,
		    isp->isp_osinfo.reqmap);
	}
	if (isp->isp_rquest != NULL) {
		bus_dmamem_free(isp->isp_osinfo.reqdmat, isp->isp_rquest,
		    isp->isp_osinfo.reqmap);
		bus_dma_tag_destroy(isp->isp_osinfo.reqdmat);
	}
	if (isp->isp_result_dma != 0) {
		bus_dmamap_unload(isp->isp_osinfo.respdmat,
		    isp->isp_osinfo.respmap);
	}
	if (isp->isp_result != NULL) {
		bus_dmamem_free(isp->isp_osinfo.respdmat, isp->isp_result,
		    isp->isp_osinfo.respmap);
		bus_dma_tag_destroy(isp->isp_osinfo.respdmat);
	}
#ifdef	ISP_TARGET_MODE
	if (IS_24XX(isp)) {
		if (isp->isp_atioq_dma != 0) {
			bus_dmamap_unload(isp->isp_osinfo.atiodmat,
			    isp->isp_osinfo.atiomap);
		}
		if (isp->isp_atioq != NULL) {
			bus_dmamem_free(isp->isp_osinfo.reqdmat, isp->isp_atioq,
			    isp->isp_osinfo.atiomap);
			bus_dma_tag_destroy(isp->isp_osinfo.atiodmat);
		}
	}
#endif
	free(isp->isp_xflist, M_DEVBUF);
	free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
	isp->isp_rquest = NULL;
	ISP_LOCK(isp);
	return (1);
}

typedef struct {
	ispsoftc_t *isp;
	void *cmd_token;
	void *rq;	/* original request */
	int error;
	bus_size_t mapsize;
} mush_t;

#define	MUSHERR_NOQENTRIES	-2

#ifdef	ISP_TARGET_MODE
static void tdma2_2(void *, bus_dma_segment_t *, int, bus_size_t, int);
static void tdma2(void *, bus_dma_segment_t *, int, int);

static void
tdma2_2(void *arg, bus_dma_segment_t *dm_segs, int nseg, bus_size_t mapsize, int error)
{
	mush_t *mp;
	mp = (mush_t *)arg;
	mp->mapsize = mapsize;
	tdma2(arg, dm_segs, nseg, error);
}

static void
tdma2(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	ispsoftc_t *isp;
	struct ccb_scsiio *csio;
	isp_ddir_t ddir;
	ispreq_t *rq;

	mp = (mush_t *) arg;
	if (error) {
		mp->error = error;
		return;
	}
	csio = mp->cmd_token;
	isp = mp->isp;
	rq = mp->rq;
	if (nseg) {
		if (isp->isp_osinfo.sixtyfourbit) {
			if (nseg >= ISP_NSEG64_MAX) {
				isp_prt(isp, ISP_LOGERR, "number of segments (%d) exceed maximum we can support (%d)", nseg, ISP_NSEG64_MAX);
				mp->error = EFAULT;
				return;
			}
			if (rq->req_header.rqs_entry_type == RQSTYPE_CTIO2) {
				rq->req_header.rqs_entry_type = RQSTYPE_CTIO3;
			}
		} else {
			if (nseg >= ISP_NSEG_MAX) {
				isp_prt(isp, ISP_LOGERR, "number of segments (%d) exceed maximum we can support (%d)", nseg, ISP_NSEG_MAX);
				mp->error = EFAULT;
				return;
			}
		}
		if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			bus_dmamap_sync(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREWRITE);
			ddir = ISP_TO_DEVICE;
		} else if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT) {
			bus_dmamap_sync(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREREAD);
			ddir = ISP_FROM_DEVICE;
		} else {
			dm_segs = NULL;
			nseg = 0;
			ddir = ISP_NOXFR;
		}
	} else {
		dm_segs = NULL;
		nseg = 0;
		ddir = ISP_NOXFR;
	}

	error = isp_send_tgt_cmd(isp, rq, dm_segs, nseg, XS_XFRLEN(csio), ddir, &csio->sense_data, csio->sense_len);
	switch (error) {
	case CMD_EAGAIN:
		mp->error = MUSHERR_NOQENTRIES;
	case CMD_QUEUED:
		break;
	default:
		mp->error = EIO;
	}
}
#endif

static void dma2_2(void *, bus_dma_segment_t *, int, bus_size_t, int);
static void dma2(void *, bus_dma_segment_t *, int, int);

static void
dma2_2(void *arg, bus_dma_segment_t *dm_segs, int nseg, bus_size_t mapsize, int error)
{
	mush_t *mp;
	mp = (mush_t *)arg;
	mp->mapsize = mapsize;
	dma2(arg, dm_segs, nseg, error);
}

static void
dma2(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	ispsoftc_t *isp;
	struct ccb_scsiio *csio;
	isp_ddir_t ddir;
	ispreq_t *rq;

	mp = (mush_t *) arg;
	if (error) {
		mp->error = error;
		return;
	}
	csio = mp->cmd_token;
	isp = mp->isp;
	rq = mp->rq;
	if (nseg) {
		if (isp->isp_osinfo.sixtyfourbit) {
			if (nseg >= ISP_NSEG64_MAX) {
				isp_prt(isp, ISP_LOGERR, "number of segments (%d) exceed maximum we can support (%d)", nseg, ISP_NSEG64_MAX);
				mp->error = EFAULT;
				return;
			}
			if (rq->req_header.rqs_entry_type == RQSTYPE_T2RQS) {
				rq->req_header.rqs_entry_type = RQSTYPE_T3RQS;
			} else if (rq->req_header.rqs_entry_type == RQSTYPE_REQUEST) {
				rq->req_header.rqs_entry_type = RQSTYPE_A64;
			}
		} else {
			if (nseg >= ISP_NSEG_MAX) {
				isp_prt(isp, ISP_LOGERR, "number of segments (%d) exceed maximum we can support (%d)", nseg, ISP_NSEG_MAX);
				mp->error = EFAULT;
				return;
			}
		}
		if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			bus_dmamap_sync(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREREAD);
			ddir = ISP_FROM_DEVICE;
		} else if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT) {
			bus_dmamap_sync(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREWRITE);
			ddir = ISP_TO_DEVICE;
		} else {
			ddir = ISP_NOXFR;
		}
	} else {
		dm_segs = NULL;
		nseg = 0;
		ddir = ISP_NOXFR;
	}

	error = isp_send_cmd(isp, rq, dm_segs, nseg, XS_XFRLEN(csio), ddir, (ispds64_t *)csio->req_map);
	switch (error) {
	case CMD_EAGAIN:
		mp->error = MUSHERR_NOQENTRIES;
		break;
	case CMD_QUEUED:
		break;
	default:
		mp->error = EIO;
		break;
	}
}

static int
isp_pci_dmasetup(ispsoftc_t *isp, struct ccb_scsiio *csio, void *ff)
{
	mush_t mush, *mp;
	void (*eptr)(void *, bus_dma_segment_t *, int, int);
	void (*eptr2)(void *, bus_dma_segment_t *, int, bus_size_t, int);
	int error;

	mp = &mush;
	mp->isp = isp;
	mp->cmd_token = csio;
	mp->rq = ff;
	mp->error = 0;
	mp->mapsize = 0;

#ifdef	ISP_TARGET_MODE
	if (csio->ccb_h.func_code == XPT_CONT_TARGET_IO) {
		eptr = tdma2;
		eptr2 = tdma2_2;
	} else
#endif
	{
		eptr = dma2;
		eptr2 = dma2_2;
	}


	error = bus_dmamap_load_ccb(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap,
	    (union ccb *)csio, eptr, mp, 0);
	if (error == EINPROGRESS) {
		bus_dmamap_unload(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap);
		mp->error = EINVAL;
		isp_prt(isp, ISP_LOGERR, "deferred dma allocation not supported");
	} else if (error && mp->error == 0) {
#ifdef	DIAGNOSTIC
		isp_prt(isp, ISP_LOGERR, "error %d in dma mapping code", error);
#endif
		mp->error = error;
	}
	if (mp->error) {
		int retval = CMD_COMPLETE;
		if (mp->error == MUSHERR_NOQENTRIES) {
			retval = CMD_EAGAIN;
		} else if (mp->error == EFBIG) {
			csio->ccb_h.status = CAM_REQ_TOO_BIG;
		} else if (mp->error == EINVAL) {
			csio->ccb_h.status = CAM_REQ_INVALID;
		} else {
			csio->ccb_h.status = CAM_UNREC_HBA_ERROR;
		}
		return (retval);
	}
	return (CMD_QUEUED);
}

static void
isp_pci_reset0(ispsoftc_t *isp)
{
	ISP_DISABLE_INTS(isp);
}

static void
isp_pci_reset1(ispsoftc_t *isp)
{
	if (!IS_24XX(isp)) {
		/* Make sure the BIOS is disabled */
		isp_pci_wr_reg(isp, HCCR, PCI_HCCR_CMD_BIOS);
	}
	/* and enable interrupts */
	ISP_ENABLE_INTS(isp);
}

static void
isp_pci_dumpregs(ispsoftc_t *isp, const char *msg)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *)isp;
	if (msg)
		printf("%s: %s\n", device_get_nameunit(isp->isp_dev), msg);
	else
		printf("%s:\n", device_get_nameunit(isp->isp_dev));
	if (IS_SCSI(isp))
		printf("    biu_conf1=%x", ISP_READ(isp, BIU_CONF1));
	else
		printf("    biu_csr=%x", ISP_READ(isp, BIU2100_CSR));
	printf(" biu_icr=%x biu_isr=%x biu_sema=%x ", ISP_READ(isp, BIU_ICR),
	    ISP_READ(isp, BIU_ISR), ISP_READ(isp, BIU_SEMA));
	printf("risc_hccr=%x\n", ISP_READ(isp, HCCR));


	if (IS_SCSI(isp)) {
		ISP_WRITE(isp, HCCR, HCCR_CMD_PAUSE);
		printf("    cdma_conf=%x cdma_sts=%x cdma_fifostat=%x\n",
			ISP_READ(isp, CDMA_CONF), ISP_READ(isp, CDMA_STATUS),
			ISP_READ(isp, CDMA_FIFO_STS));
		printf("    ddma_conf=%x ddma_sts=%x ddma_fifostat=%x\n",
			ISP_READ(isp, DDMA_CONF), ISP_READ(isp, DDMA_STATUS),
			ISP_READ(isp, DDMA_FIFO_STS));
		printf("    sxp_int=%x sxp_gross=%x sxp(scsi_ctrl)=%x\n",
			ISP_READ(isp, SXP_INTERRUPT),
			ISP_READ(isp, SXP_GROSS_ERR),
			ISP_READ(isp, SXP_PINS_CTRL));
		ISP_WRITE(isp, HCCR, HCCR_CMD_RELEASE);
	}
	printf("    mbox regs: %x %x %x %x %x\n",
	    ISP_READ(isp, OUTMAILBOX0), ISP_READ(isp, OUTMAILBOX1),
	    ISP_READ(isp, OUTMAILBOX2), ISP_READ(isp, OUTMAILBOX3),
	    ISP_READ(isp, OUTMAILBOX4));
	printf("    PCI Status Command/Status=%x\n",
	    pci_read_config(pcs->pci_dev, PCIR_COMMAND, 1));
}
