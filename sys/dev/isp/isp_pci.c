/*-
 * Copyright (c) 1997-2006 by Matthew Jacob
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
__FBSDID("$FreeBSD: src/sys/dev/isp/isp_pci.c,v 1.148 2007/06/26 23:08:57 mjacob Exp $");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/module.h>
#if __FreeBSD_version >= 700000  
#include <sys/linker.h>
#include <sys/firmware.h>
#endif
#include <sys/bus.h>
#if __FreeBSD_version < 500000  
#include <pci/pcireg.h>
#include <pci/pcivar.h>
#include <machine/bus_memio.h>
#include <machine/bus_pio.h>
#else
#include <sys/stdint.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#endif
#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sys/malloc.h>

#include <dev/isp/isp_freebsd.h>

#if __FreeBSD_version < 500000  
#define	BUS_PROBE_DEFAULT	0
#endif

static uint32_t isp_pci_rd_reg(ispsoftc_t *, int);
static void isp_pci_wr_reg(ispsoftc_t *, int, uint32_t);
static uint32_t isp_pci_rd_reg_1080(ispsoftc_t *, int);
static void isp_pci_wr_reg_1080(ispsoftc_t *, int, uint32_t);
static uint32_t isp_pci_rd_reg_2400(ispsoftc_t *, int);
static void isp_pci_wr_reg_2400(ispsoftc_t *, int, uint32_t);
static int
isp_pci_rd_isr(ispsoftc_t *, uint32_t *, uint16_t *, uint16_t *);
static int
isp_pci_rd_isr_2300(ispsoftc_t *, uint32_t *, uint16_t *, uint16_t *);
static int
isp_pci_rd_isr_2400(ispsoftc_t *, uint32_t *, uint16_t *, uint16_t *);
static int isp_pci_mbxdma(ispsoftc_t *);
static int
isp_pci_dmasetup(ispsoftc_t *, XS_T *, ispreq_t *, uint32_t *, uint32_t);


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

#ifndef	PCI_PRODUCT_QLOGIC_ISP6312
#define	PCI_PRODUCT_QLOGIC_ISP6312	0x6312
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP6322
#define	PCI_PRODUCT_QLOGIC_ISP6322	0x6322
#endif


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

#define	PCI_QLOGIC_ISP6312	\
	((PCI_PRODUCT_QLOGIC_ISP6312 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP6322	\
	((PCI_PRODUCT_QLOGIC_ISP6322 << 16) | PCI_VENDOR_QLOGIC)

/*
 * Odd case for some AMI raid cards... We need to *not* attach to this.
 */
#define	AMI_RAID_SUBVENDOR_ID	0x101e

#define	IO_MAP_REG	0x10
#define	MEM_MAP_REG	0x14

#define	PCI_DFLT_LTNCY	0x40
#define	PCI_DFLT_LNSZ	0x10

static int isp_pci_probe (device_t);
static int isp_pci_attach (device_t);
static int isp_pci_detach (device_t);


#define	ISP_PCD(isp)	((struct isp_pcisoftc *)isp)->pci_dev
struct isp_pcisoftc {
	ispsoftc_t			pci_isp;
	device_t			pci_dev;
	struct resource *		pci_reg;
	void *				ih;
	int16_t				pci_poff[_NREG_BLKS];
	bus_dma_tag_t			dmat;
#if __FreeBSD_version > 700025
	int				msicount;
#endif
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
#if __FreeBSD_version < 700000  
extern ispfwfunc *isp_get_firmware_p;
#endif

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
	case PCI_QLOGIC_ISP6312:
		device_set_desc(dev, "Qlogic ISP 6312 PCI FC-AL Adapter");
		break;
	case PCI_QLOGIC_ISP6322:
		device_set_desc(dev, "Qlogic ISP 6322 PCI FC-AL Adapter");
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

#if __FreeBSD_version < 500000  
static void
isp_get_generic_options(device_t dev, ispsoftc_t *isp)
{
	int bitmap, unit;

	unit = device_get_unit(dev);
	if (getenv_int("isp_disable", &bitmap)) {
		if (bitmap & (1 << unit)) {
			isp->isp_osinfo.disabled = 1;
			return;
		}
	}
	if (getenv_int("isp_no_fwload", &bitmap)) {
		if (bitmap & (1 << unit))
			isp->isp_confopts |= ISP_CFG_NORELOAD;
	}
	if (getenv_int("isp_fwload", &bitmap)) {
		if (bitmap & (1 << unit))
			isp->isp_confopts &= ~ISP_CFG_NORELOAD;
	}
	if (getenv_int("isp_no_nvram", &bitmap)) {
		if (bitmap & (1 << unit))
			isp->isp_confopts |= ISP_CFG_NONVRAM;
	}
	if (getenv_int("isp_nvram", &bitmap)) {
		if (bitmap & (1 << unit))
			isp->isp_confopts &= ~ISP_CFG_NONVRAM;
	}

	bitmap = 0;
	(void) getenv_int("isp_debug", &bitmap);
	if (bitmap) {
		isp->isp_dblev = bitmap;
	} else {
		isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
	}
	if (bootverbose) {
		isp->isp_dblev |= ISP_LOGCONFIG|ISP_LOGINFO;
	}

	bitmap = 0;
	if (getenv_int("role", &bitmap)) {
		isp->isp_role = bitmap;
	} else {
		isp->isp_role = ISP_DEFAULT_ROLES;
	}

}

static void
isp_get_pci_options(device_t dev, int *m1, int *m2)
{
	int bitmap;
	int unit = device_get_unit(dev);

	*m1 = PCIM_CMD_MEMEN;
	*m2 = PCIM_CMD_PORTEN;
	if (getenv_int("isp_mem_map", &bitmap)) {
		if (bitmap & (1 << unit)) {
			*m1 = PCIM_CMD_MEMEN;
			*m2 = PCIM_CMD_PORTEN;
		}
	}
	bitmap = 0;
	if (getenv_int("isp_io_map", &bitmap)) {
		if (bitmap & (1 << unit)) {
			*m1 = PCIM_CMD_PORTEN;
			*m2 = PCIM_CMD_MEMEN;
		}
	}
}

static void
isp_get_specific_options(device_t dev, ispsoftc_t *isp)
{
	uint64_t wwn;
	int bitmap;
	int unit = device_get_unit(dev);


	if (IS_SCSI(isp)) {
		return;
	}

	if (getenv_int("isp_fcduplex", &bitmap)) {
		if (bitmap & (1 << unit))
			isp->isp_confopts |= ISP_CFG_FULL_DUPLEX;
	}
	if (getenv_int("isp_no_fcduplex", &bitmap)) {
		if (bitmap & (1 << unit))
			isp->isp_confopts &= ~ISP_CFG_FULL_DUPLEX;
	}
	if (getenv_int("isp_nport", &bitmap)) {
		if (bitmap & (1 << unit))
			isp->isp_confopts |= ISP_CFG_NPORT;
	}

	/*
	 * Because the resource_*_value functions can neither return
	 * 64 bit integer values, nor can they be directly coerced
	 * to interpret the right hand side of the assignment as
	 * you want them to interpret it, we have to force WWN
	 * hint replacement to specify WWN strings with a leading
	 * 'w' (e..g w50000000aaaa0001). Sigh.
	 */
	if (getenv_quad("isp_portwwn", &wwn)) {
		isp->isp_osinfo.default_port_wwn = wwn;
		isp->isp_confopts |= ISP_CFG_OWNWWPN;
	}
	if (isp->isp_osinfo.default_port_wwn == 0) {
		isp->isp_osinfo.default_port_wwn = 0x400000007F000009ull;
	}

	if (getenv_quad("isp_nodewwn", &wwn)) {
		isp->isp_osinfo.default_node_wwn = wwn;
		isp->isp_confopts |= ISP_CFG_OWNWWNN;
	}
	if (isp->isp_osinfo.default_node_wwn == 0) {
		isp->isp_osinfo.default_node_wwn = 0x400000007F000009ull;
	}

	bitmap = 0;
	(void) getenv_int("isp_fabric_hysteresis", &bitmap);
	if (bitmap >= 0 && bitmap < 256) {
		isp->isp_osinfo.hysteresis = bitmap;
	} else {
		isp->isp_osinfo.hysteresis = isp_fabric_hysteresis;
	}

	bitmap = 0;
	(void) getenv_int("isp_loop_down_limit", &bitmap);
	if (bitmap >= 0 && bitmap < 0xffff) {
		isp->isp_osinfo.loop_down_limit = bitmap;
	} else {
		isp->isp_osinfo.loop_down_limit = isp_loop_down_limit;
	}

	bitmap = 0;
	(void) getenv_int("isp_gone_device_time", &bitmap);
	if (bitmap >= 0 && bitmap < 0xffff) {
		isp->isp_osinfo.gone_device_time = bitmap;
	} else {
		isp->isp_osinfo.gone_device_time = isp_gone_device_time;
	}
#ifdef	ISP_FW_CRASH_DUMP
	bitmap = 0;
	if (getenv_int("isp_fw_dump_enable", &bitmap)) {
		if (bitmap & (1 << unit) {
			size_t amt = 0;
			if (IS_2200(isp)) {
				amt = QLA2200_RISC_IMAGE_DUMP_SIZE;
			} else if (IS_23XX(isp)) {
				amt = QLA2300_RISC_IMAGE_DUMP_SIZE;
			}
			if (amt) {
				FCPARAM(isp)->isp_dump_data =
				    malloc(amt, M_DEVBUF, M_WAITOK);
				memset(FCPARAM(isp)->isp_dump_data, 0, amt);
			} else {
				device_printf(dev,
				    "f/w crash dumps not supported for card\n");
			}
		}
	}
#endif
}
#else
static void
isp_get_generic_options(device_t dev, ispsoftc_t *isp)
{
	int tval;

	/*
	 * Figure out if we're supposed to skip this one.
	 */
	tval = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "disable", &tval) == 0 && tval) {
		device_printf(dev, "disabled at user request\n");
		isp->isp_osinfo.disabled = 1;
		return;
	}
	
	tval = -1;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "role", &tval) == 0 && tval != -1) {
		tval &= (ISP_ROLE_INITIATOR|ISP_ROLE_TARGET);
		isp->isp_role = tval;
		device_printf(dev, "setting role to 0x%x\n", isp->isp_role);
	} else {
#ifdef	ISP_TARGET_MODE
		isp->isp_role = ISP_ROLE_TARGET;
#else
		isp->isp_role = ISP_DEFAULT_ROLES;
#endif
	}

	tval = 0;
        if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "fwload_disable", &tval) == 0 && tval != 0) {
		isp->isp_confopts |= ISP_CFG_NORELOAD;
	}
	tval = 0;
        if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "ignore_nvram", &tval) == 0 && tval != 0) {
		isp->isp_confopts |= ISP_CFG_NONVRAM;
	}

	tval = 0;
        (void) resource_int_value(device_get_name(dev), device_get_unit(dev),
            "debug", &tval);
	if (tval) {
		isp->isp_dblev = tval;
	} else {
		isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
	}
	if (bootverbose) {
		isp->isp_dblev |= ISP_LOGCONFIG|ISP_LOGINFO;
	}

}

static void
isp_get_pci_options(device_t dev, int *m1, int *m2)
{
	int tval;
	/*
	 * Which we should try first - memory mapping or i/o mapping?
	 *
	 * We used to try memory first followed by i/o on alpha, otherwise
	 * the reverse, but we should just try memory first all the time now.
	 */
	*m1 = PCIM_CMD_MEMEN;
	*m2 = PCIM_CMD_PORTEN;

	tval = 0;
        if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "prefer_iomap", &tval) == 0 && tval != 0) {
		*m1 = PCIM_CMD_PORTEN;
		*m2 = PCIM_CMD_MEMEN;
	}
	tval = 0;
        if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "prefer_memmap", &tval) == 0 && tval != 0) {
		*m1 = PCIM_CMD_MEMEN;
		*m2 = PCIM_CMD_PORTEN;
	}
}

static void
isp_get_specific_options(device_t dev, ispsoftc_t *isp)
{
	const char *sptr;
	int tval;

	isp->isp_osinfo.default_id = -1;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "iid", &tval) == 0) {
		isp->isp_osinfo.default_id = tval;
		isp->isp_confopts |= ISP_CFG_OWNLOOPID;
	}
	if (isp->isp_osinfo.default_id == -1) {
		if (IS_FC(isp)) {
			isp->isp_osinfo.default_id = 109;
		} else {
			isp->isp_osinfo.default_id = 7;
		}
	}

	if (IS_SCSI(isp)) {
		return;
	}

	tval = 0;
        if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "fullduplex", &tval) == 0 && tval != 0) {
		isp->isp_confopts |= ISP_CFG_FULL_DUPLEX;
	}
#ifdef	ISP_FW_CRASH_DUMP
	tval = 0;
        if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "fw_dump_enable", &tval) == 0 && tval != 0) {
		size_t amt = 0;
		if (IS_2200(isp)) {
			amt = QLA2200_RISC_IMAGE_DUMP_SIZE;
		} else if (IS_23XX(isp)) {
			amt = QLA2300_RISC_IMAGE_DUMP_SIZE;
		}
		if (amt) {
			FCPARAM(isp)->isp_dump_data =
			    malloc(amt, M_DEVBUF, M_WAITOK | M_ZERO);
		} else {
			device_printf(dev,
			    "f/w crash dumps not supported for this model\n");
		}
	}
#endif
	sptr = 0;
        if (resource_string_value(device_get_name(dev), device_get_unit(dev),
            "topology", (const char **) &sptr) == 0 && sptr != 0) {
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

	/*
	 * Because the resource_*_value functions can neither return
	 * 64 bit integer values, nor can they be directly coerced
	 * to interpret the right hand side of the assignment as
	 * you want them to interpret it, we have to force WWN
	 * hint replacement to specify WWN strings with a leading
	 * 'w' (e..g w50000000aaaa0001). Sigh.
	 */
	sptr = 0;
	tval = resource_string_value(device_get_name(dev), device_get_unit(dev),
            "portwwn", (const char **) &sptr);
	if (tval == 0 && sptr != 0 && *sptr++ == 'w') {
		char *eptr = 0;
		isp->isp_osinfo.default_port_wwn = strtouq(sptr, &eptr, 16);
		if (eptr < sptr + 16 || isp->isp_osinfo.default_port_wwn == 0) {
			device_printf(dev, "mangled portwwn hint '%s'\n", sptr);
			isp->isp_osinfo.default_port_wwn = 0;
		} else {
			isp->isp_confopts |= ISP_CFG_OWNWWPN;
		}
	}
	if (isp->isp_osinfo.default_port_wwn == 0) {
		isp->isp_osinfo.default_port_wwn = 0x400000007F000009ull;
	}

	sptr = 0;
	tval = resource_string_value(device_get_name(dev), device_get_unit(dev),
            "nodewwn", (const char **) &sptr);
	if (tval == 0 && sptr != 0 && *sptr++ == 'w') {
		char *eptr = 0;
		isp->isp_osinfo.default_node_wwn = strtouq(sptr, &eptr, 16);
		if (eptr < sptr + 16 || isp->isp_osinfo.default_node_wwn == 0) {
			device_printf(dev, "mangled nodewwn hint '%s'\n", sptr);
			isp->isp_osinfo.default_node_wwn = 0;
		} else {
			isp->isp_confopts |= ISP_CFG_OWNWWNN;
		}
	}
	if (isp->isp_osinfo.default_node_wwn == 0) {
		isp->isp_osinfo.default_node_wwn = 0x400000007F000009ull;
	}


	tval = 0;
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "hysteresis", &tval);
	if (tval >= 0 && tval < 256) {
		isp->isp_osinfo.hysteresis = tval;
	} else {
		isp->isp_osinfo.hysteresis = isp_fabric_hysteresis;
	}

	tval = -1;
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "loop_down_limit", &tval);
	if (tval >= 0 && tval < 0xffff) {
		isp->isp_osinfo.loop_down_limit = tval;
	} else {
		isp->isp_osinfo.loop_down_limit = isp_loop_down_limit;
	}

	tval = -1;
	(void) resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "gone_device_time", &tval);
	if (tval >= 0 && tval < 0xffff) {
		isp->isp_osinfo.gone_device_time = tval;
	} else {
		isp->isp_osinfo.gone_device_time = isp_gone_device_time;
	}
}
#endif

static int
isp_pci_attach(device_t dev)
{
	struct resource *regs, *irq;
	int rtp, rgd, iqd, m1, m2;
	uint32_t data, cmd, linesz, psize, basetype;
	struct isp_pcisoftc *pcs;
	ispsoftc_t *isp = NULL;
	struct ispmdvec *mdvp;
#if __FreeBSD_version >= 500000  
	int locksetup = 0;
#endif

	pcs = device_get_softc(dev);
	if (pcs == NULL) {
		device_printf(dev, "cannot get softc\n");
		return (ENOMEM);
	}
	memset(pcs, 0, sizeof (*pcs));
	pcs->pci_dev = dev;
	isp = &pcs->pci_isp;

	/*
	 * Get Generic Options
	 */
	isp_get_generic_options(dev, isp);

	/*
	 * Check to see if options have us disabled
	 */
	if (isp->isp_osinfo.disabled) {
		/*
		 * But return zero to preserve unit numbering
		 */
		return (0);
	}

	/*
	 * Get PCI options- which in this case are just mapping preferences.
	 */
	isp_get_pci_options(dev, &m1, &m2);

	linesz = PCI_DFLT_LNSZ;
	irq = regs = NULL;
	rgd = rtp = iqd = 0;

	cmd = pci_read_config(dev, PCIR_COMMAND, 2);
	if (cmd & m1) {
		rtp = (m1 == PCIM_CMD_MEMEN)? SYS_RES_MEMORY : SYS_RES_IOPORT;
		rgd = (m1 == PCIM_CMD_MEMEN)? MEM_MAP_REG : IO_MAP_REG;
		regs = bus_alloc_resource_any(dev, rtp, &rgd, RF_ACTIVE);
	}
	if (regs == NULL && (cmd & m2)) {
		rtp = (m2 == PCIM_CMD_MEMEN)? SYS_RES_MEMORY : SYS_RES_IOPORT;
		rgd = (m2 == PCIM_CMD_MEMEN)? MEM_MAP_REG : IO_MAP_REG;
		regs = bus_alloc_resource_any(dev, rtp, &rgd, RF_ACTIVE);
	}
	if (regs == NULL) {
		device_printf(dev, "unable to map any ports\n");
		goto bad;
	}
	if (bootverbose) {
		device_printf(dev, "using %s space register mapping\n",
		    (rgd == IO_MAP_REG)? "I/O" : "Memory");
	}
	pcs->pci_dev = dev;
	pcs->pci_reg = regs;
	isp->isp_bus_tag = rman_get_bustag(regs);
	isp->isp_bus_handle = rman_get_bushandle(regs);

	pcs->pci_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS_OFF;
	pcs->pci_poff[SXP_BLOCK >> _BLK_REG_SHFT] = PCI_SXP_REGS_OFF;
	pcs->pci_poff[RISC_BLOCK >> _BLK_REG_SHFT] = PCI_RISC_REGS_OFF;
	pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;
	mdvp = &mdvec;
	basetype = ISP_HA_SCSI_UNKNOWN;
	psize = sizeof (sdparam);
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP1020) {
		mdvp = &mdvec;
		basetype = ISP_HA_SCSI_UNKNOWN;
		psize = sizeof (sdparam);
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP1080) {
		mdvp = &mdvec_1080;
		basetype = ISP_HA_SCSI_1080;
		psize = sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP1240) {
		mdvp = &mdvec_1080;
		basetype = ISP_HA_SCSI_1240;
		psize = 2 * sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP1280) {
		mdvp = &mdvec_1080;
		basetype = ISP_HA_SCSI_1280;
		psize = 2 * sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP10160) {
		mdvp = &mdvec_12160;
		basetype = ISP_HA_SCSI_10160;
		psize = sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP12160) {
		mdvp = &mdvec_12160;
		basetype = ISP_HA_SCSI_12160;
		psize = 2 * sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP2100) {
		mdvp = &mdvec_2100;
		basetype = ISP_HA_FC_2100;
		psize = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
		if (pci_get_revid(dev) < 3) {
			/*
			 * XXX: Need to get the actual revision
			 * XXX: number of the 2100 FB. At any rate,
			 * XXX: lower cache line size for early revision
			 * XXX; boards.
			 */
			linesz = 1;
		}
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP2200) {
		mdvp = &mdvec_2200;
		basetype = ISP_HA_FC_2200;
		psize = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP2300) {
		mdvp = &mdvec_2300;
		basetype = ISP_HA_FC_2300;
		psize = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP2312 ||
	    pci_get_devid(dev) == PCI_QLOGIC_ISP6312) {
		mdvp = &mdvec_2300;
		basetype = ISP_HA_FC_2312;
		psize = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP2322 ||
	    pci_get_devid(dev) == PCI_QLOGIC_ISP6322) {
		mdvp = &mdvec_2300;
		basetype = ISP_HA_FC_2322;
		psize = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2300_OFF;
	}
	if (pci_get_devid(dev) == PCI_QLOGIC_ISP2422 ||
	    pci_get_devid(dev) == PCI_QLOGIC_ISP2432) {
		mdvp = &mdvec_2400;
		basetype = ISP_HA_FC_2400;
		psize = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2400_OFF;
	}
	isp = &pcs->pci_isp;
	isp->isp_param = malloc(psize, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (isp->isp_param == NULL) {
		device_printf(dev, "cannot allocate parameter data\n");
		goto bad;
	}
	isp->isp_mdvec = mdvp;
	isp->isp_type = basetype;
	isp->isp_revision = pci_get_revid(dev);
	isp->isp_dev = dev;

	/*
	 * Now that we know who we are (roughly) get/set specific options
	 */
	isp_get_specific_options(dev, isp);

#if __FreeBSD_version >= 700000  
	/*
	 * Try and find firmware for this device.
	 */
	{
		char fwname[32];
		unsigned int did = pci_get_device(dev);

		/*
		 * Map a few pci ids to fw names
		 */
		switch (did) {
		case PCI_PRODUCT_QLOGIC_ISP1020:
			did = 0x1040;
			break;
		case PCI_PRODUCT_QLOGIC_ISP1240:
			did = 0x1080;
			break;
		case PCI_PRODUCT_QLOGIC_ISP10160:
		case PCI_PRODUCT_QLOGIC_ISP12160:
			did = 0x12160;
			break;
		case PCI_PRODUCT_QLOGIC_ISP6312:
		case PCI_PRODUCT_QLOGIC_ISP2312:
			did = 0x2300;
			break;
		case PCI_PRODUCT_QLOGIC_ISP6322:
			did = 0x2322;
			break;
		case PCI_PRODUCT_QLOGIC_ISP2422:
		case PCI_PRODUCT_QLOGIC_ISP2432:
			did = 0x2400;
			break;
		default:
			break;
		}

		isp->isp_osinfo.fw = NULL;
		if (isp->isp_role & ISP_ROLE_TARGET) {
			snprintf(fwname, sizeof (fwname), "isp_%04x_it", did);
			isp->isp_osinfo.fw = firmware_get(fwname);
		}
		if (isp->isp_osinfo.fw == NULL) {
			snprintf(fwname, sizeof (fwname), "isp_%04x", did);
			isp->isp_osinfo.fw = firmware_get(fwname);
		}
		if (isp->isp_osinfo.fw != NULL) {
			isp->isp_mdvec->dv_ispfw = isp->isp_osinfo.fw->data;
		}
	}
#else
	if (isp_get_firmware_p) {
		int device = (int) pci_get_device(dev);
#ifdef	ISP_TARGET_MODE
		(*isp_get_firmware_p)(0, 1, device, &mdvp->dv_ispfw);
#else
		(*isp_get_firmware_p)(0, 0, device, &mdvp->dv_ispfw);
#endif
	}
#endif

	/*
	 * Make sure that SERR, PERR, WRITE INVALIDATE and BUSMASTER
	 * are set.
	 */
	cmd |= PCIM_CMD_SEREN | PCIM_CMD_PERRESPEN |
		PCIM_CMD_BUSMASTEREN | PCIM_CMD_INVEN;

	if (IS_2300(isp)) {	/* per QLogic errata */
		cmd &= ~PCIM_CMD_INVEN;
	}

	if (IS_2322(isp) || pci_get_devid(dev) == PCI_QLOGIC_ISP6312) {
		cmd &= ~PCIM_CMD_INTX_DISABLE;
	}

#ifdef	WE_KNEW_WHAT_WE_WERE_DOING
	if (IS_24XX(isp)) {
		int reg;

		cmd &= ~PCIM_CMD_INTX_DISABLE;

		/*
		 * Is this a PCI-X card? If so, set max read byte count.
		 */
		if (pci_find_extcap(dev, PCIY_PCIX, &reg) == 0) {
			uint16_t pxcmd;
			reg += 2;

			pxcmd = pci_read_config(dev, reg, 2);
			pxcmd &= ~0xc;
			pxcmd |= 0x8;
			pci_write_config(dev, reg, 2, pxcmd);
		}

		/*
		 * Is this a PCI Express card? If so, set max read byte count.
		 */
		if (pci_find_extcap(dev, PCIY_EXPRESS, &reg) == 0) {
			uint16_t pectl;

			reg += 0x8;
			pectl = pci_read_config(dev, reg, 2);
			pectl &= ~0x7000;
			pectl |= 0x4000;
			pci_write_config(dev, reg, 2, pectl);
		}
	}
#else
	if (IS_24XX(isp)) {
		cmd &= ~PCIM_CMD_INTX_DISABLE;
	}
#endif

	pci_write_config(dev, PCIR_COMMAND, cmd, 2);

	/*
	 * Make sure the Cache Line Size register is set sensibly.
	 */
	data = pci_read_config(dev, PCIR_CACHELNSZ, 1);
	if (data == 0 || (linesz != PCI_DFLT_LNSZ && data != linesz)) {
		isp_prt(isp, ISP_LOGCONFIG, "set PCI line size to %d from %d",
		    linesz, data);
		data = linesz;
		pci_write_config(dev, PCIR_CACHELNSZ, data, 1);
	}

	/*
	 * Make sure the Latency Timer is sane.
	 */
	data = pci_read_config(dev, PCIR_LATTIMER, 1);
	if (data < PCI_DFLT_LTNCY) {
		data = PCI_DFLT_LTNCY;
		isp_prt(isp, ISP_LOGCONFIG, "set PCI latency to %d", data);
		pci_write_config(dev, PCIR_LATTIMER, data, 1);
	}

	/*
	 * Make sure we've disabled the ROM.
	 */
	data = pci_read_config(dev, PCIR_ROMADDR, 4);
	data &= ~1;
	pci_write_config(dev, PCIR_ROMADDR, data, 4);
#if __FreeBSD_version > 700025
	if (IS_24XX(isp) || IS_2322(isp)) {
		pcs->msicount = pci_msi_count(dev);
		if (pcs->msicount > 1) {
			pcs->msicount = 1;
		}
		if (pci_alloc_msi(dev, &pcs->msicount) == 0) {
			iqd = 1;
		} else {
			iqd = 0;
		}
	}
#else
	iqd = 0;
#endif
	irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &iqd,
	    RF_ACTIVE | RF_SHAREABLE);
	if (irq == NULL) {
		device_printf(dev, "could not allocate interrupt\n");
		goto bad;
	}

#if __FreeBSD_version >= 500000  
	/* Make sure the lock is set up. */
	mtx_init(&isp->isp_osinfo.lock, "isp", NULL, MTX_DEF);
	locksetup++;
#endif

	if (isp_setup_intr(dev, irq, ISP_IFLAGS, NULL, isp_platform_intr, isp,
	    &pcs->ih)) {
		device_printf(dev, "could not setup interrupt\n");
		goto bad;
	}

	/*
	 * Last minute checks...
	 */
	if (IS_23XX(isp) || IS_24XX(isp)) {
		isp->isp_port = pci_get_function(dev);
	}

	if (IS_23XX(isp)) {
		/*
		 * Can't tell if ROM will hang on 'ABOUT FIRMWARE' command.
		 */
		isp->isp_touched = 1;
	}

	/*
	 * Make sure we're in reset state.
	 */
	ISP_LOCK(isp);
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK(isp);
		goto bad;
	}
	isp_init(isp);
	if (isp->isp_role != ISP_ROLE_NONE && isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		goto bad;
	}
	isp_attach(isp);
	if (isp->isp_role != ISP_ROLE_NONE && isp->isp_state != ISP_RUNSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		goto bad;
	}
	ISP_UNLOCK(isp);
	return (0);

bad:
	if (pcs && pcs->ih) {
		(void) bus_teardown_intr(dev, irq, pcs->ih);
	}
#if __FreeBSD_version >= 500000  
	if (locksetup && isp) {
		mtx_destroy(&isp->isp_osinfo.lock);
	}
#endif
	if (irq) {
		(void) bus_release_resource(dev, SYS_RES_IRQ, iqd, irq);
	}
#if __FreeBSD_version > 700025
	if (pcs && pcs->msicount) {
		pci_release_msi(dev);
	}
#endif
	if (regs) {
		(void) bus_release_resource(dev, rtp, rgd, regs);
	}
	if (pcs) {
		if (pcs->pci_isp.isp_param) {
#ifdef	ISP_FW_CRASH_DUMP
			if (IS_FC(isp) && FCPARAM(isp)->isp_dump_data) {
				free(FCPARAM(isp)->isp_dump_data, M_DEVBUF);
			}
#endif
			free(pcs->pci_isp.isp_param, M_DEVBUF);
		}
	}
	return (ENXIO);
}

static int
isp_pci_detach(device_t dev)
{
	struct isp_pcisoftc *pcs;
	ispsoftc_t *isp;

	pcs = device_get_softc(dev);
	if (pcs == NULL) {
		return (ENXIO);
	}
	isp = (ispsoftc_t *) pcs;
	ISP_DISABLE_INTS(isp);
	return (0);
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_pcisoftc *)a)->pci_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xfff))

#define	BXR2(isp, off)		\
	bus_space_read_2(isp->isp_bus_tag, isp->isp_bus_handle, off)
#define	BXW2(isp, off, v)	\
	bus_space_write_2(isp->isp_bus_tag, isp->isp_bus_handle, off, v)
#define	BXR4(isp, off)		\
	bus_space_read_4(isp->isp_bus_tag, isp->isp_bus_handle, off)
#define	BXW4(isp, off, v)	\
	bus_space_write_4(isp->isp_bus_tag, isp->isp_bus_handle, off, v)


static __inline int
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
isp_pci_rd_isr(ispsoftc_t *isp, uint32_t *isrp, uint16_t *semap, uint16_t *mbp)
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
			if (isp_pci_rd_debounced(isp, OUTMAILBOX0, mbp)) {
				return (0);
			}
		} else {
			*mbp = BXR2(isp, IspVirt2Off(isp, OUTMAILBOX0));
		}
	}
	return (1);
}

static int
isp_pci_rd_isr_2300(ispsoftc_t *isp, uint32_t *isrp,
    uint16_t *semap, uint16_t *mbox0p)
{
	uint32_t hccr;
	uint32_t r2hisr;

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
	switch (r2hisr & BIU_R2HST_ISTAT_MASK) {
	case ISPR2HST_ROM_MBX_OK:
	case ISPR2HST_ROM_MBX_FAIL:
	case ISPR2HST_MBX_OK:
	case ISPR2HST_MBX_FAIL:
	case ISPR2HST_ASYNC_EVENT:
		*isrp = r2hisr & 0xffff;
		*mbox0p = (r2hisr >> 16);
		*semap = 1;
		return (1);
	case ISPR2HST_RIO_16:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_RIO1;
		*semap = 1;
		return (1);
	case ISPR2HST_FPOST:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_CMD_CMPLT;
		*semap = 1;
		return (1);
	case ISPR2HST_FPOST_CTIO:
		*isrp = r2hisr & 0xffff;
		*mbox0p = ASYNC_CTIO_DONE;
		*semap = 1;
		return (1);
	case ISPR2HST_RSPQ_UPDATE:
		*isrp = r2hisr & 0xffff;
		*mbox0p = 0;
		*semap = 0;
		return (1);
	default:
		hccr = ISP_READ(isp, HCCR);
		if (hccr & HCCR_PAUSE) {
			ISP_WRITE(isp, HCCR, HCCR_RESET);
			isp_prt(isp, ISP_LOGERR,
			    "RISC paused at interrupt (%x->%x)", hccr,
			    ISP_READ(isp, HCCR));
			ISP_WRITE(isp, BIU_ICR, 0);
		} else {
			isp_prt(isp, ISP_LOGERR, "unknown interrupt 0x%x\n",
			    r2hisr);
		}
		return (0);
	}
}

static int
isp_pci_rd_isr_2400(ispsoftc_t *isp, uint32_t *isrp,
    uint16_t *semap, uint16_t *mbox0p)
{
	uint32_t r2hisr;

	r2hisr = BXR4(isp, IspVirt2Off(isp, BIU2400_R2HSTSLO));
	isp_prt(isp, ISP_LOGDEBUG3, "RISC2HOST ISR 0x%x", r2hisr);
	if ((r2hisr & BIU2400_R2HST_INTR) == 0) {
		*isrp = 0;
		return (0);
	}
	switch (r2hisr & BIU2400_R2HST_ISTAT_MASK) {
	case ISP2400R2HST_ROM_MBX_OK:
	case ISP2400R2HST_ROM_MBX_FAIL:
	case ISP2400R2HST_MBX_OK:
	case ISP2400R2HST_MBX_FAIL:
	case ISP2400R2HST_ASYNC_EVENT:
		*isrp = r2hisr & 0xffff;
		*mbox0p = (r2hisr >> 16);
		*semap = 1;
		return (1);
	case ISP2400R2HST_RSPQ_UPDATE:
	case ISP2400R2HST_ATIO_RSPQ_UPDATE:
	case ISP2400R2HST_ATIO_RQST_UPDATE:
		*isrp = r2hisr & 0xffff;
		*mbox0p = 0;
		*semap = 0;
		return (1);
	default:
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_CLEAR_RISC_INT);
		isp_prt(isp, ISP_LOGERR, "unknown interrupt 0x%x\n", r2hisr);
		return (0);
	}
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
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1),
		    oldconf | BIU_PCI_CONF1_SXP);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	rv = BXR2(isp, IspVirt2Off(isp, regoff));
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oldconf);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
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
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	BXW2(isp, IspVirt2Off(isp, regoff), val);
	MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 2);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oldconf);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}

}

static uint32_t
isp_pci_rd_reg_1080(ispsoftc_t *isp, int regoff)
{
	uint32_t rv, oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    (regoff & _BLK_REG_MASK) == (SXP_BLOCK|SXP_BANK1_SELECT)) {
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
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(isp, IspVirt2Off(isp, BIU_CONF1));
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), 
		    oc | BIU_PCI1080_CONF1_DMA);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	rv = BXR2(isp, IspVirt2Off(isp, regoff));
	if (oc) {
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oc);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	return (rv);
}

static void
isp_pci_wr_reg_1080(ispsoftc_t *isp, int regoff, uint32_t val)
{
	int oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    (regoff & _BLK_REG_MASK) == (SXP_BLOCK|SXP_BANK1_SELECT)) {
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
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = BXR2(isp, IspVirt2Off(isp, BIU_CONF1));
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), 
		    oc | BIU_PCI1080_CONF1_DMA);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
	}
	BXW2(isp, IspVirt2Off(isp, regoff), val);
	MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 2);
	if (oc) {
		BXW2(isp, IspVirt2Off(isp, BIU_CONF1), oc);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, BIU_CONF1), 2);
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
		isp_prt(isp, ISP_LOGWARN, "SXP_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	case RISC_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "RISC_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	case DMA_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "DMA_BLOCK read at 0x%x", regoff);
		return (0xffffffff);
	default:
		isp_prt(isp, ISP_LOGWARN, "unknown block read at 0x%x", regoff);
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
	case BIU2400_PRI_RQINP:
	case BIU2400_PRI_RSPINP:
	case BIU2400_ATIO_RSPINP:
	case BIU2400_ATIO_REQINP:
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
		isp_prt(isp, ISP_LOGERR,
		    "isp_pci_rd_reg_2400: unknown offset %x", regoff);
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
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 2);
		return;
	case SXP_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "SXP_BLOCK write at 0x%x", regoff);
		return;
	case RISC_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "RISC_BLOCK write at 0x%x", regoff);
		return;
	case DMA_BLOCK:
		isp_prt(isp, ISP_LOGWARN, "DMA_BLOCK write at 0x%x", regoff);
		return;
	default:
		isp_prt(isp, ISP_LOGWARN, "unknown block write at 0x%x",
		    regoff);
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
	case BIU2400_PRI_RQINP:
	case BIU2400_PRI_RSPINP:
	case BIU2400_ATIO_RSPINP:
	case BIU2400_ATIO_REQINP:
	case BIU2400_HCCR:
	case BIU2400_GPIOD:
	case BIU2400_GPIOE:
	case BIU2400_HSEMA:
		BXW4(isp, IspVirt2Off(isp, regoff), val);
		MEMORYBARRIER(isp, SYNC_REG, IspVirt2Off(isp, regoff), 4);
		break;
	default:
		isp_prt(isp, ISP_LOGERR,
		    "isp_pci_wr_reg_2400: bad offset 0x%x", regoff);
		break;
	}
}


struct imush {
	ispsoftc_t *isp;
	int error;
};

static void imc(void *, bus_dma_segment_t *, int, int);

static void
imc(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{
	struct imush *imushp = (struct imush *) arg;
	if (error) {
		imushp->error = error;
	} else {
		ispsoftc_t *isp =imushp->isp;
		bus_addr_t addr = segs->ds_addr;

		isp->isp_rquest_dma = addr;
		addr += ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
		isp->isp_result_dma = addr;
		if (IS_FC(isp)) {
			addr += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
			FCPARAM(isp)->isp_scdma = addr;
		}
	}
}

static int
isp_pci_mbxdma(ispsoftc_t *isp)
{
	caddr_t base;
	uint32_t len;
	int i, error, ns;
	bus_size_t slim;	/* segment size */
	bus_addr_t llim;	/* low limit of unavailable dma */
	bus_addr_t hlim;	/* high limit of unavailable dma */
	struct imush im;

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
	isp->isp_osinfo.pcmd_pool =
		(struct isp_pcmd *) malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (isp->isp_osinfo.pcmd_pool == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot allocate pcmds");
		ISP_LOCK(isp);
		return (1);
	}

	/*
	 * XXX: We don't really support 64 bit target mode for parallel scsi yet
	 */
#ifdef	ISP_TARGET_MODE
	if (IS_SCSI(isp) && sizeof (bus_addr_t) > 4) {
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		ISP_LOCK(isp);
		isp_prt(isp, ISP_LOGERR, "we cannot do DAC for SPI cards yet");
		return (1);
	}
#endif

	if (isp_dma_tag_create(BUS_DMA_ROOTARG(ISP_PCD(isp)), 1,
	    slim, llim, hlim, NULL, NULL, BUS_SPACE_MAXSIZE, ISP_NSEGS,
	    slim, 0, &isp->isp_osinfo.dmat)) {
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		ISP_LOCK(isp);
		isp_prt(isp, ISP_LOGERR, "could not create master dma tag");
		return (1);
	}


	len = sizeof (XS_T **) * isp->isp_maxcmds;
	isp->isp_xflist = (XS_T **) malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (isp->isp_xflist == NULL) {
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		ISP_LOCK(isp);
		isp_prt(isp, ISP_LOGERR, "cannot alloc xflist array");
		return (1);
	}
#ifdef	ISP_TARGET_MODE
	len = sizeof (void **) * isp->isp_maxcmds;
	isp->isp_tgtlist = (void **) malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (isp->isp_tgtlist == NULL) {
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		free(isp->isp_xflist, M_DEVBUF);
		ISP_LOCK(isp);
		isp_prt(isp, ISP_LOGERR, "cannot alloc tgtlist array");
		return (1);
	}
#endif

	/*
	 * Allocate and map the request, result queues, plus FC scratch area.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	len += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
	if (IS_FC(isp)) {
		len += ISP2100_SCRLEN;
	}

	ns = (len / PAGE_SIZE) + 1;
	/*
	 * Create a tag for the control spaces- force it to within 32 bits.
	 */
	if (isp_dma_tag_create(isp->isp_osinfo.dmat, QENTRY_LEN, slim,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR,
	    NULL, NULL, len, ns, slim, 0, &isp->isp_cdmat)) {
		isp_prt(isp, ISP_LOGERR,
		    "cannot create a dma tag for control spaces");
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		free(isp->isp_xflist, M_DEVBUF);
#ifdef	ISP_TARGET_MODE
		free(isp->isp_tgtlist, M_DEVBUF);
#endif
		ISP_LOCK(isp);
		return (1);
	}

	if (bus_dmamem_alloc(isp->isp_cdmat, (void **)&base, BUS_DMA_NOWAIT,
	    &isp->isp_cdmap) != 0) {
		isp_prt(isp, ISP_LOGERR,
		    "cannot allocate %d bytes of CCB memory", len);
		bus_dma_tag_destroy(isp->isp_cdmat);
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		free(isp->isp_xflist, M_DEVBUF);
#ifdef	ISP_TARGET_MODE
		free(isp->isp_tgtlist, M_DEVBUF);
#endif
		ISP_LOCK(isp);
		return (1);
	}

	for (i = 0; i < isp->isp_maxcmds; i++) {
		struct isp_pcmd *pcmd = &isp->isp_osinfo.pcmd_pool[i];
		error = bus_dmamap_create(isp->isp_osinfo.dmat, 0, &pcmd->dmap);
		if (error) {
			isp_prt(isp, ISP_LOGERR,
			    "error %d creating per-cmd DMA maps", error);
			while (--i >= 0) {
				bus_dmamap_destroy(isp->isp_osinfo.dmat,
				    isp->isp_osinfo.pcmd_pool[i].dmap);
			}
			goto bad;
		}
		isp_callout_init(&pcmd->wdog);
		if (i == isp->isp_maxcmds-1) {
			pcmd->next = NULL;
		} else {
			pcmd->next = &isp->isp_osinfo.pcmd_pool[i+1];
		}
	}
	isp->isp_osinfo.pcmd_free = &isp->isp_osinfo.pcmd_pool[0];

	im.isp = isp;
	im.error = 0;
	bus_dmamap_load(isp->isp_cdmat, isp->isp_cdmap, base, len, imc, &im, 0);
	if (im.error) {
		isp_prt(isp, ISP_LOGERR,
		    "error %d loading dma map for control areas", im.error);
		goto bad;
	}

	isp->isp_rquest = base;
	base += ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	isp->isp_result = base;
	if (IS_FC(isp)) {
		base += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
		FCPARAM(isp)->isp_scratch = base;
	}
	ISP_LOCK(isp);
	return (0);

bad:
	bus_dmamem_free(isp->isp_cdmat, base, isp->isp_cdmap);
	bus_dma_tag_destroy(isp->isp_cdmat);
	free(isp->isp_xflist, M_DEVBUF);
#ifdef	ISP_TARGET_MODE
	free(isp->isp_tgtlist, M_DEVBUF);
#endif
	free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
	isp->isp_rquest = NULL;
	ISP_LOCK(isp);
	return (1);
}

typedef struct {
	ispsoftc_t *isp;
	void *cmd_token;
	void *rq;
	uint32_t *nxtip;
	uint32_t optr;
	int error;
} mush_t;

#define	MUSHERR_NOQENTRIES	-2

#ifdef	ISP_TARGET_MODE
/*
 * We need to handle DMA for target mode differently from initiator mode.
 * 
 * DMA mapping and construction and submission of CTIO Request Entries
 * and rendevous for completion are very tightly coupled because we start
 * out by knowing (per platform) how much data we have to move, but we
 * don't know, up front, how many DMA mapping segments will have to be used
 * cover that data, so we don't know how many CTIO Request Entries we
 * will end up using. Further, for performance reasons we may want to
 * (on the last CTIO for Fibre Channel), send status too (if all went well).
 *
 * The standard vector still goes through isp_pci_dmasetup, but the callback
 * for the DMA mapping routines comes here instead with the whole transfer
 * mapped and a pointer to a partially filled in already allocated request
 * queue entry. We finish the job.
 */
static void tdma_mk(void *, bus_dma_segment_t *, int, int);
static void tdma_mkfc(void *, bus_dma_segment_t *, int, int);

#define	STATUS_WITH_DATA	1

static void
tdma_mk(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	struct ccb_scsiio *csio;
	ispsoftc_t *isp;
	ct_entry_t *cto, *qe;
	uint8_t scsi_status;
	uint32_t curi, nxti, handle;
	uint32_t sflags;
	int32_t resid;
	int nth_ctio, nctios, send_status;

	mp = (mush_t *) arg;
	if (error) {
		mp->error = error;
		return;
	}

	isp = mp->isp;
	csio = mp->cmd_token;
	cto = mp->rq;
	curi = isp->isp_reqidx;
	qe = (ct_entry_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, curi);

	cto->ct_xfrlen = 0;
	cto->ct_seg_count = 0;
	cto->ct_header.rqs_entry_count = 1;
	MEMZERO(cto->ct_dataseg, sizeof(cto->ct_dataseg));

	if (nseg == 0) {
		cto->ct_header.rqs_seqno = 1;
		isp_prt(isp, ISP_LOGTDEBUG1,
		    "CTIO[%x] lun%d iid%d tag %x flgs %x sts %x ssts %x res %d",
		    cto->ct_fwhandle, csio->ccb_h.target_lun, cto->ct_iid,
		    cto->ct_tag_val, cto->ct_flags, cto->ct_status,
		    cto->ct_scsi_status, cto->ct_resid);
		ISP_TDQE(isp, "tdma_mk[no data]", curi, cto);
		isp_put_ctio(isp, cto, qe);
		return;
	}

	nctios = nseg / ISP_RQDSEG;
	if (nseg % ISP_RQDSEG) {
		nctios++;
	}

	/*
	 * Save syshandle, and potentially any SCSI status, which we'll
	 * reinsert on the last CTIO we're going to send.
	 */

	handle = cto->ct_syshandle;
	cto->ct_syshandle = 0;
	cto->ct_header.rqs_seqno = 0;
	send_status = (cto->ct_flags & CT_SENDSTATUS) != 0;

	if (send_status) {
		sflags = cto->ct_flags & (CT_SENDSTATUS | CT_CCINCR);
		cto->ct_flags &= ~(CT_SENDSTATUS | CT_CCINCR);
		/*
		 * Preserve residual.
		 */
		resid = cto->ct_resid;

		/*
		 * Save actual SCSI status.
		 */
		scsi_status = cto->ct_scsi_status;

#ifndef	STATUS_WITH_DATA
		sflags |= CT_NO_DATA;
		/*
		 * We can't do a status at the same time as a data CTIO, so
		 * we need to synthesize an extra CTIO at this level.
		 */
		nctios++;
#endif
	} else {
		sflags = scsi_status = resid = 0;
	}

	cto->ct_resid = 0;
	cto->ct_scsi_status = 0;

	if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		   PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREREAD);
	} else {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREWRITE);
	}

	nxti = *mp->nxtip;

	for (nth_ctio = 0; nth_ctio < nctios; nth_ctio++) {
		int seglim;

		seglim = nseg;
		if (seglim) {
			int seg;

			if (seglim > ISP_RQDSEG)
				seglim = ISP_RQDSEG;

			for (seg = 0; seg < seglim; seg++, nseg--) {
				/*
				 * Unlike normal initiator commands, we don't
				 * do any swizzling here.
				 */
				cto->ct_dataseg[seg].ds_count = dm_segs->ds_len;
				cto->ct_dataseg[seg].ds_base = dm_segs->ds_addr;
				cto->ct_xfrlen += dm_segs->ds_len;
				dm_segs++;
			}
			cto->ct_seg_count = seg;
		} else {
			/*
			 * This case should only happen when we're sending an
			 * extra CTIO with final status.
			 */
			if (send_status == 0) {
				isp_prt(isp, ISP_LOGWARN,
				    "tdma_mk ran out of segments");
				mp->error = EINVAL;
				return;
			}
		}

		/*
		 * At this point, the fields ct_lun, ct_iid, ct_tagval,
		 * ct_tagtype, and ct_timeout have been carried over
		 * unchanged from what our caller had set.
		 * 
		 * The dataseg fields and the seg_count fields we just got
		 * through setting. The data direction we've preserved all
		 * along and only clear it if we're now sending status.
		 */

		if (nth_ctio == nctios - 1) {
			/*
			 * We're the last in a sequence of CTIOs, so mark
			 * this CTIO and save the handle to the CCB such that
			 * when this CTIO completes we can free dma resources
			 * and do whatever else we need to do to finish the
			 * rest of the command. We *don't* give this to the
			 * firmware to work on- the caller will do that.
			 */

			cto->ct_syshandle = handle;
			cto->ct_header.rqs_seqno = 1;

			if (send_status) {
				cto->ct_scsi_status = scsi_status;
				cto->ct_flags |= sflags;
				cto->ct_resid = resid;
			}
			if (send_status) {
				isp_prt(isp, ISP_LOGTDEBUG1,
				    "CTIO[%x] lun%d iid %d tag %x ct_flags %x "
				    "scsi status %x resid %d",
				    cto->ct_fwhandle, csio->ccb_h.target_lun,
				    cto->ct_iid, cto->ct_tag_val, cto->ct_flags,
				    cto->ct_scsi_status, cto->ct_resid);
			} else {
				isp_prt(isp, ISP_LOGTDEBUG1,
				    "CTIO[%x] lun%d iid%d tag %x ct_flags 0x%x",
				    cto->ct_fwhandle, csio->ccb_h.target_lun,
				    cto->ct_iid, cto->ct_tag_val,
				    cto->ct_flags);
			}
			isp_put_ctio(isp, cto, qe);
			ISP_TDQE(isp, "last tdma_mk", curi, cto);
			if (nctios > 1) {
				MEMORYBARRIER(isp, SYNC_REQUEST,
				    curi, QENTRY_LEN);
			}
		} else {
			ct_entry_t *oqe = qe;

			/*
			 * Make sure syshandle fields are clean
			 */
			cto->ct_syshandle = 0;
			cto->ct_header.rqs_seqno = 0;

			isp_prt(isp, ISP_LOGTDEBUG1,
			    "CTIO[%x] lun%d for ID%d ct_flags 0x%x",
			    cto->ct_fwhandle, csio->ccb_h.target_lun,
			    cto->ct_iid, cto->ct_flags);

			/*
			 * Get a new CTIO
			 */
			qe = (ct_entry_t *)
			    ISP_QUEUE_ENTRY(isp->isp_rquest, nxti);
			nxti = ISP_NXT_QENTRY(nxti, RQUEST_QUEUE_LEN(isp));
			if (nxti == mp->optr) {
				isp_prt(isp, ISP_LOGTDEBUG0,
				    "Queue Overflow in tdma_mk");
				mp->error = MUSHERR_NOQENTRIES;
				return;
			}

			/*
			 * Now that we're done with the old CTIO,
			 * flush it out to the request queue.
			 */
			ISP_TDQE(isp, "dma_tgt_fc", curi, cto);
			isp_put_ctio(isp, cto, oqe);
			if (nth_ctio != 0) {
				MEMORYBARRIER(isp, SYNC_REQUEST, curi,
				    QENTRY_LEN);
			}
			curi = ISP_NXT_QENTRY(curi, RQUEST_QUEUE_LEN(isp));

			/*
			 * Reset some fields in the CTIO so we can reuse
			 * for the next one we'll flush to the request
			 * queue.
			 */
			cto->ct_header.rqs_entry_type = RQSTYPE_CTIO;
			cto->ct_header.rqs_entry_count = 1;
			cto->ct_header.rqs_flags = 0;
			cto->ct_status = 0;
			cto->ct_scsi_status = 0;
			cto->ct_xfrlen = 0;
			cto->ct_resid = 0;
			cto->ct_seg_count = 0;
			MEMZERO(cto->ct_dataseg, sizeof(cto->ct_dataseg));
		}
	}
	*mp->nxtip = nxti;
}

/*
 * We don't have to do multiple CTIOs here. Instead, we can just do
 * continuation segments as needed. This greatly simplifies the code
 * improves performance.
 */

static void
tdma_mkfc(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	struct ccb_scsiio *csio;
	ispsoftc_t *isp;
	ct2_entry_t *cto, *qe;
	uint32_t curi, nxti;
	ispds_t *ds;
	ispds64_t *ds64;
	int segcnt, seglim;

	mp = (mush_t *) arg;
	if (error) {
		mp->error = error;
		return;
	}

	isp = mp->isp;
	csio = mp->cmd_token;
	cto = mp->rq;

	curi = isp->isp_reqidx;
	qe = (ct2_entry_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, curi);

	if (nseg == 0) {
		if ((cto->ct_flags & CT2_FLAG_MMASK) != CT2_FLAG_MODE1) {
			isp_prt(isp, ISP_LOGWARN,
			    "dma2_tgt_fc, a status CTIO2 without MODE1 "
			    "set (0x%x)", cto->ct_flags);
			mp->error = EINVAL;
			return;
		}
		/*
		 * We preserve ct_lun, ct_iid, ct_rxid. We set the data
		 * flags to NO DATA and clear relative offset flags.
		 * We preserve the ct_resid and the response area.
		 */
		cto->ct_header.rqs_seqno = 1;
		cto->ct_seg_count = 0;
		cto->ct_reloff = 0;
		isp_prt(isp, ISP_LOGTDEBUG1,
		    "CTIO2[%x] lun %d->iid%d flgs 0x%x sts 0x%x ssts "
		    "0x%x res %d", cto->ct_rxid, csio->ccb_h.target_lun,
		    cto->ct_iid, cto->ct_flags, cto->ct_status,
		    cto->rsp.m1.ct_scsi_status, cto->ct_resid);
		if (FCPARAM(isp)->isp_2klogin) {
			isp_put_ctio2e(isp,
			    (ct2e_entry_t *)cto, (ct2e_entry_t *)qe);
		} else {
			isp_put_ctio2(isp, cto, qe);
		}
		ISP_TDQE(isp, "dma2_tgt_fc[no data]", curi, qe);
		return;
	}

	if ((cto->ct_flags & CT2_FLAG_MMASK) != CT2_FLAG_MODE0) {
		isp_prt(isp, ISP_LOGERR,
		    "dma2_tgt_fc, a data CTIO2 without MODE0 set "
		    "(0x%x)", cto->ct_flags);
		mp->error = EINVAL;
		return;
	}


	nxti = *mp->nxtip;

	/*
	 * Check to see if we need to DAC addressing or not.
	 *
	 * Any address that's over the 4GB boundary causes this
	 * to happen.
	 */
	segcnt = nseg;
	if (sizeof (bus_addr_t) > 4) {
		for (segcnt = 0; segcnt < nseg; segcnt++) {
			uint64_t addr = dm_segs[segcnt].ds_addr;
			if (addr >= 0x100000000LL) {
				break;
			}
		}
	}
	if (segcnt != nseg) {
		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO3;
		seglim = ISP_RQDSEG_T3;
		ds64 = &cto->rsp.m0.u.ct_dataseg64[0];
		ds = NULL;
	} else {
		seglim = ISP_RQDSEG_T2;
		ds64 = NULL;
		ds = &cto->rsp.m0.u.ct_dataseg[0];
	}
	cto->ct_seg_count = 0;

	/*
	 * Set up the CTIO2 data segments.
	 */
	for (segcnt = 0; cto->ct_seg_count < seglim && segcnt < nseg;
	    cto->ct_seg_count++, segcnt++) {
		if (ds64) {
			ds64->ds_basehi =
			    ((uint64_t) (dm_segs[segcnt].ds_addr) >> 32);
			ds64->ds_base = dm_segs[segcnt].ds_addr;
			ds64->ds_count = dm_segs[segcnt].ds_len;
			ds64++;
		} else {
			ds->ds_base = dm_segs[segcnt].ds_addr;
			ds->ds_count = dm_segs[segcnt].ds_len;
			ds++;
		}
		cto->rsp.m0.ct_xfrlen += dm_segs[segcnt].ds_len;
#if __FreeBSD_version < 500000  
		isp_prt(isp, ISP_LOGTDEBUG1,
		    "isp_send_ctio2: ent0[%d]0x%llx:%llu",
		    cto->ct_seg_count, (uint64_t)dm_segs[segcnt].ds_addr,
		    (uint64_t)dm_segs[segcnt].ds_len);
#else
		isp_prt(isp, ISP_LOGTDEBUG1,
		    "isp_send_ctio2: ent0[%d]0x%jx:%ju",
		    cto->ct_seg_count, (uintmax_t)dm_segs[segcnt].ds_addr,
		    (uintmax_t)dm_segs[segcnt].ds_len);
#endif
	}

	while (segcnt < nseg) {
		uint32_t curip;
		int seg;
		ispcontreq_t local, *crq = &local, *qep;

		qep = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, nxti);
		curip = nxti;
		nxti = ISP_NXT_QENTRY(curip, RQUEST_QUEUE_LEN(isp));
		if (nxti == mp->optr) {
			isp_prt(isp, ISP_LOGTDEBUG0,
			    "tdma_mkfc: request queue overflow");
			mp->error = MUSHERR_NOQENTRIES;
			return;
		}
		cto->ct_header.rqs_entry_count++;
		MEMZERO((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		if (cto->ct_header.rqs_entry_type == RQSTYPE_CTIO3) {
			seglim = ISP_CDSEG64;
			ds = NULL;
			ds64 = &((ispcontreq64_t *)crq)->req_dataseg[0];
			crq->req_header.rqs_entry_type = RQSTYPE_A64_CONT;
		} else {
			seglim = ISP_CDSEG;
			ds = &crq->req_dataseg[0];
			ds64 = NULL;
			crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;
		}
		for (seg = 0; segcnt < nseg && seg < seglim;
		    segcnt++, seg++) {
			if (ds64) {
				ds64->ds_basehi =
				  ((uint64_t) (dm_segs[segcnt].ds_addr) >> 32);
				ds64->ds_base = dm_segs[segcnt].ds_addr;
				ds64->ds_count = dm_segs[segcnt].ds_len;
				ds64++;
			} else {
				ds->ds_base = dm_segs[segcnt].ds_addr;
				ds->ds_count = dm_segs[segcnt].ds_len;
				ds++;
			}
#if __FreeBSD_version < 500000  
			isp_prt(isp, ISP_LOGTDEBUG1,
			    "isp_send_ctio2: ent%d[%d]%llx:%llu",
			    cto->ct_header.rqs_entry_count-1, seg,
			    (uint64_t)dm_segs[segcnt].ds_addr,
			    (uint64_t)dm_segs[segcnt].ds_len);
#else
			isp_prt(isp, ISP_LOGTDEBUG1,
			    "isp_send_ctio2: ent%d[%d]%jx:%ju",
			    cto->ct_header.rqs_entry_count-1, seg,
			    (uintmax_t)dm_segs[segcnt].ds_addr,
			    (uintmax_t)dm_segs[segcnt].ds_len);
#endif
			cto->rsp.m0.ct_xfrlen += dm_segs[segcnt].ds_len;
			cto->ct_seg_count++;
		}
		MEMORYBARRIER(isp, SYNC_REQUEST, curip, QENTRY_LEN);
		isp_put_cont_req(isp, crq, qep);
		ISP_TDQE(isp, "cont entry", curi, qep);
	}

	/*
	 * No do final twiddling for the CTIO itself.
	 */
	cto->ct_header.rqs_seqno = 1;
	isp_prt(isp, ISP_LOGTDEBUG1,
	    "CTIO2[%x] lun %d->iid%d flgs 0x%x sts 0x%x ssts 0x%x resid %d",
	    cto->ct_rxid, csio->ccb_h.target_lun, (int) cto->ct_iid,
	    cto->ct_flags, cto->ct_status, cto->rsp.m1.ct_scsi_status,
	    cto->ct_resid);
	if (FCPARAM(isp)->isp_2klogin) {
		isp_put_ctio2e(isp, (ct2e_entry_t *)cto, (ct2e_entry_t *)qe);
	} else {
		isp_put_ctio2(isp, cto, qe);
	}
	ISP_TDQE(isp, "last dma2_tgt_fc", curi, qe);
	*mp->nxtip = nxti;
}
#endif

static void dma_2400(void *, bus_dma_segment_t *, int, int);
static void dma2_a64(void *, bus_dma_segment_t *, int, int);
static void dma2(void *, bus_dma_segment_t *, int, int);

static void
dma_2400(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	ispsoftc_t *isp;
	struct ccb_scsiio *csio;
	bus_dma_segment_t *eseg;
	ispreqt7_t *rq;
	int seglim, datalen;
	uint32_t nxti;

	mp = (mush_t *) arg;
	if (error) {
		mp->error = error;
		return;
	}

	if (nseg < 1) {
		isp_prt(mp->isp, ISP_LOGERR, "bad segment count (%d)", nseg);
		mp->error = EFAULT;
		return;
	}

	csio = mp->cmd_token;
	isp = mp->isp;
	rq = mp->rq;
	nxti = *mp->nxtip;

	if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREREAD);
	} else {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREWRITE);
	}
	datalen = XS_XFRLEN(csio);

	/*
	 * We're passed an initial partially filled in entry that
	 * has most fields filled in except for data transfer
	 * related values.
	 *
	 * Our job is to fill in the initial request queue entry and
	 * then to start allocating and filling in continuation entries
	 * until we've covered the entire transfer.
	 */

	rq->req_header.rqs_entry_type = RQSTYPE_T7RQS;
	rq->req_dl = datalen;
	if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		rq->req_alen_datadir = 0x2;
	} else {
		rq->req_alen_datadir = 0x1;
	}

	eseg = dm_segs + nseg;

	rq->req_dataseg.ds_base = DMA_LO32(dm_segs->ds_addr);
	rq->req_dataseg.ds_basehi = DMA_HI32(dm_segs->ds_addr);
	rq->req_dataseg.ds_count = dm_segs->ds_len;

	datalen -= dm_segs->ds_len;

	dm_segs++;
	rq->req_seg_count++;

	while (datalen > 0 && dm_segs != eseg) {
		uint32_t onxti;
		ispcontreq64_t local, *crq = &local, *cqe;

		cqe = (ispcontreq64_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, nxti);
		onxti = nxti;
		nxti = ISP_NXT_QENTRY(onxti, RQUEST_QUEUE_LEN(isp));
		if (nxti == mp->optr) {
			isp_prt(isp, ISP_LOGDEBUG0, "Request Queue Overflow++");
			mp->error = MUSHERR_NOQENTRIES;
			return;
		}
		rq->req_header.rqs_entry_count++;
		MEMZERO((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		crq->req_header.rqs_entry_type = RQSTYPE_A64_CONT;

		seglim = 0;
		while (datalen > 0 && seglim < ISP_CDSEG64 && dm_segs != eseg) {
			crq->req_dataseg[seglim].ds_base =
			    DMA_LO32(dm_segs->ds_addr);
			crq->req_dataseg[seglim].ds_basehi =
			    DMA_HI32(dm_segs->ds_addr);
			crq->req_dataseg[seglim].ds_count =
			    dm_segs->ds_len;
			rq->req_seg_count++;
			dm_segs++;
			seglim++;
			datalen -= dm_segs->ds_len;
		}
		if (isp->isp_dblev & ISP_LOGDEBUG1) {
			isp_print_bytes(isp, "Continuation", QENTRY_LEN, crq);
		}
		isp_put_cont64_req(isp, crq, cqe);
		MEMORYBARRIER(isp, SYNC_REQUEST, onxti, QENTRY_LEN);
	}
	*mp->nxtip = nxti;
}

static void
dma2_a64(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	ispsoftc_t *isp;
	struct ccb_scsiio *csio;
	bus_dma_segment_t *eseg;
	ispreq64_t *rq;
	int seglim, datalen;
	uint32_t nxti;

	mp = (mush_t *) arg;
	if (error) {
		mp->error = error;
		return;
	}

	if (nseg < 1) {
		isp_prt(mp->isp, ISP_LOGERR, "bad segment count (%d)", nseg);
		mp->error = EFAULT;
		return;
	}
	csio = mp->cmd_token;
	isp = mp->isp;
	rq = mp->rq;
	nxti = *mp->nxtip;

	if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREREAD);
	} else {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREWRITE);
	}
	datalen = XS_XFRLEN(csio);

	/*
	 * We're passed an initial partially filled in entry that
	 * has most fields filled in except for data transfer
	 * related values.
	 *
	 * Our job is to fill in the initial request queue entry and
	 * then to start allocating and filling in continuation entries
	 * until we've covered the entire transfer.
	 */

	if (IS_FC(isp)) {
		rq->req_header.rqs_entry_type = RQSTYPE_T3RQS;
		seglim = ISP_RQDSEG_T3;
		((ispreqt3_t *)rq)->req_totalcnt = datalen;
		if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			((ispreqt3_t *)rq)->req_flags |= REQFLAG_DATA_IN;
		} else {
			((ispreqt3_t *)rq)->req_flags |= REQFLAG_DATA_OUT;
		}
	} else {
		rq->req_header.rqs_entry_type = RQSTYPE_A64;
		if (csio->cdb_len > 12) {
			seglim = 0;
		} else {
			seglim = ISP_RQDSEG_A64;
		}
		if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			rq->req_flags |= REQFLAG_DATA_IN;
		} else {
			rq->req_flags |= REQFLAG_DATA_OUT;
		}
	}

	eseg = dm_segs + nseg;

	while (datalen != 0 && rq->req_seg_count < seglim && dm_segs != eseg) {
		if (IS_FC(isp)) {
			ispreqt3_t *rq3 = (ispreqt3_t *)rq;
			rq3->req_dataseg[rq3->req_seg_count].ds_base =
			    DMA_LO32(dm_segs->ds_addr);
			rq3->req_dataseg[rq3->req_seg_count].ds_basehi =
			    DMA_HI32(dm_segs->ds_addr);
			rq3->req_dataseg[rq3->req_seg_count].ds_count =
			    dm_segs->ds_len;
		} else {
			rq->req_dataseg[rq->req_seg_count].ds_base =
			    DMA_LO32(dm_segs->ds_addr);
			rq->req_dataseg[rq->req_seg_count].ds_basehi =
			    DMA_HI32(dm_segs->ds_addr);
			rq->req_dataseg[rq->req_seg_count].ds_count =
			    dm_segs->ds_len;
		}
		datalen -= dm_segs->ds_len;
		rq->req_seg_count++;
		dm_segs++;
	}

	while (datalen > 0 && dm_segs != eseg) {
		uint32_t onxti;
		ispcontreq64_t local, *crq = &local, *cqe;

		cqe = (ispcontreq64_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, nxti);
		onxti = nxti;
		nxti = ISP_NXT_QENTRY(onxti, RQUEST_QUEUE_LEN(isp));
		if (nxti == mp->optr) {
			isp_prt(isp, ISP_LOGDEBUG0, "Request Queue Overflow++");
			mp->error = MUSHERR_NOQENTRIES;
			return;
		}
		rq->req_header.rqs_entry_count++;
		MEMZERO((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		crq->req_header.rqs_entry_type = RQSTYPE_A64_CONT;

		seglim = 0;
		while (datalen > 0 && seglim < ISP_CDSEG64 && dm_segs != eseg) {
			crq->req_dataseg[seglim].ds_base =
			    DMA_LO32(dm_segs->ds_addr);
			crq->req_dataseg[seglim].ds_basehi =
			    DMA_HI32(dm_segs->ds_addr);
			crq->req_dataseg[seglim].ds_count =
			    dm_segs->ds_len;
			rq->req_seg_count++;
			dm_segs++;
			seglim++;
			datalen -= dm_segs->ds_len;
		}
		if (isp->isp_dblev & ISP_LOGDEBUG1) {
			isp_print_bytes(isp, "Continuation", QENTRY_LEN, crq);
		}
		isp_put_cont64_req(isp, crq, cqe);
		MEMORYBARRIER(isp, SYNC_REQUEST, onxti, QENTRY_LEN);
	}
	*mp->nxtip = nxti;
}

static void
dma2(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	ispsoftc_t *isp;
	struct ccb_scsiio *csio;
	bus_dma_segment_t *eseg;
	ispreq_t *rq;
	int seglim, datalen;
	uint32_t nxti;

	mp = (mush_t *) arg;
	if (error) {
		mp->error = error;
		return;
	}

	if (nseg < 1) {
		isp_prt(mp->isp, ISP_LOGERR, "bad segment count (%d)", nseg);
		mp->error = EFAULT;
		return;
	}
	csio = mp->cmd_token;
	isp = mp->isp;
	rq = mp->rq;
	nxti = *mp->nxtip;

	if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREREAD);
	} else {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREWRITE);
	}

	datalen = XS_XFRLEN(csio);

	/*
	 * We're passed an initial partially filled in entry that
	 * has most fields filled in except for data transfer
	 * related values.
	 *
	 * Our job is to fill in the initial request queue entry and
	 * then to start allocating and filling in continuation entries
	 * until we've covered the entire transfer.
	 */

	if (IS_FC(isp)) {
		seglim = ISP_RQDSEG_T2;
		((ispreqt2_t *)rq)->req_totalcnt = datalen;
		if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			((ispreqt2_t *)rq)->req_flags |= REQFLAG_DATA_IN;
		} else {
			((ispreqt2_t *)rq)->req_flags |= REQFLAG_DATA_OUT;
		}
	} else {
		if (csio->cdb_len > 12) {
			seglim = 0;
		} else {
			seglim = ISP_RQDSEG;
		}
		if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			rq->req_flags |= REQFLAG_DATA_IN;
		} else {
			rq->req_flags |= REQFLAG_DATA_OUT;
		}
	}

	eseg = dm_segs + nseg;

	while (datalen != 0 && rq->req_seg_count < seglim && dm_segs != eseg) {
		if (IS_FC(isp)) {
			ispreqt2_t *rq2 = (ispreqt2_t *)rq;
			rq2->req_dataseg[rq2->req_seg_count].ds_base =
			    DMA_LO32(dm_segs->ds_addr);
			rq2->req_dataseg[rq2->req_seg_count].ds_count =
			    dm_segs->ds_len;
		} else {
			rq->req_dataseg[rq->req_seg_count].ds_base =
				DMA_LO32(dm_segs->ds_addr);
			rq->req_dataseg[rq->req_seg_count].ds_count =
				dm_segs->ds_len;
		}
		datalen -= dm_segs->ds_len;
		rq->req_seg_count++;
		dm_segs++;
	}

	while (datalen > 0 && dm_segs != eseg) {
		uint32_t onxti;
		ispcontreq_t local, *crq = &local, *cqe;

		cqe = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, nxti);
		onxti = nxti;
		nxti = ISP_NXT_QENTRY(onxti, RQUEST_QUEUE_LEN(isp));
		if (nxti == mp->optr) {
			isp_prt(isp, ISP_LOGDEBUG0, "Request Queue Overflow++");
			mp->error = MUSHERR_NOQENTRIES;
			return;
		}
		rq->req_header.rqs_entry_count++;
		MEMZERO((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;

		seglim = 0;
		while (datalen > 0 && seglim < ISP_CDSEG && dm_segs != eseg) {
			crq->req_dataseg[seglim].ds_base =
			    DMA_LO32(dm_segs->ds_addr);
			crq->req_dataseg[seglim].ds_count =
			    dm_segs->ds_len;
			rq->req_seg_count++;
			dm_segs++;
			seglim++;
			datalen -= dm_segs->ds_len;
		}
		if (isp->isp_dblev & ISP_LOGDEBUG1) {
			isp_print_bytes(isp, "Continuation", QENTRY_LEN, crq);
		}
		isp_put_cont_req(isp, crq, cqe);
		MEMORYBARRIER(isp, SYNC_REQUEST, onxti, QENTRY_LEN);
	}
	*mp->nxtip = nxti;
}

/*
 */
static int
isp_pci_dmasetup(ispsoftc_t *isp, struct ccb_scsiio *csio, ispreq_t *rq,
	uint32_t *nxtip, uint32_t optr)
{
	ispreq_t *qep;
	mush_t mush, *mp;
	void (*eptr)(void *, bus_dma_segment_t *, int, int);

	qep = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, isp->isp_reqidx);
#ifdef	ISP_TARGET_MODE
	if (csio->ccb_h.func_code == XPT_CONT_TARGET_IO) {
		if (IS_FC(isp)) {
			eptr = tdma_mkfc;
		} else {
			eptr = tdma_mk;
		}
		if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_NONE ||
		    (csio->dxfer_len == 0)) {
			mp = &mush;
			mp->isp = isp;
			mp->cmd_token = csio;
			mp->rq = rq;	/* really a ct_entry_t or ct2_entry_t */
			mp->nxtip = nxtip;
			mp->optr = optr;
			mp->error = 0;
			(*eptr)(mp, NULL, 0, 0);
			goto mbxsync;
		}
	} else
#endif
	if (IS_24XX(isp)) {
		eptr = dma_2400;
	} else if (sizeof (bus_addr_t) > 4) {
		eptr = dma2_a64;
	} else {
		eptr = dma2;
	}


	if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_NONE ||
	    (csio->dxfer_len == 0)) {
		rq->req_seg_count = 1;
		goto mbxsync;
	}

	/*
	 * Do a virtual grapevine step to collect info for
	 * the callback dma allocation that we have to use...
	 */
	mp = &mush;
	mp->isp = isp;
	mp->cmd_token = csio;
	mp->rq = rq;
	mp->nxtip = nxtip;
	mp->optr = optr;
	mp->error = 0;

	if ((csio->ccb_h.flags & CAM_SCATTER_VALID) == 0) {
		if ((csio->ccb_h.flags & CAM_DATA_PHYS) == 0) {
			int error;
#if __FreeBSD_version < 500000
			int s = splsoftvm();
#endif
			error = bus_dmamap_load(isp->isp_osinfo.dmat,
			    PISP_PCMD(csio)->dmap, csio->data_ptr,
			    csio->dxfer_len, eptr, mp, 0);
#if __FreeBSD_version < 500000
			splx(s);
#endif
			if (error == EINPROGRESS) {
				bus_dmamap_unload(isp->isp_osinfo.dmat,
				    PISP_PCMD(csio)->dmap);
				mp->error = EINVAL;
				isp_prt(isp, ISP_LOGERR,
				    "deferred dma allocation not supported");
			} else if (error && mp->error == 0) {
#ifdef	DIAGNOSTIC
				isp_prt(isp, ISP_LOGERR,
				    "error %d in dma mapping code", error);
#endif
				mp->error = error;
			}
		} else {
			/* Pointer to physical buffer */
			struct bus_dma_segment seg;
			seg.ds_addr = (bus_addr_t)(vm_offset_t)csio->data_ptr;
			seg.ds_len = csio->dxfer_len;
			(*eptr)(mp, &seg, 1, 0);
		}
	} else {
		struct bus_dma_segment *segs;

		if ((csio->ccb_h.flags & CAM_DATA_PHYS) != 0) {
			isp_prt(isp, ISP_LOGERR,
			    "Physical segment pointers unsupported");
			mp->error = EINVAL;
		} else if ((csio->ccb_h.flags & CAM_SG_LIST_PHYS) == 0) {
			isp_prt(isp, ISP_LOGERR,
			    "Virtual segment addresses unsupported");
			mp->error = EINVAL;
		} else {
			/* Just use the segments provided */
			segs = (struct bus_dma_segment *) csio->data_ptr;
			(*eptr)(mp, segs, csio->sglist_cnt, 0);
		}
	}
	if (mp->error) {
		int retval = CMD_COMPLETE;
		if (mp->error == MUSHERR_NOQENTRIES) {
			retval = CMD_EAGAIN;
		} else if (mp->error == EFBIG) {
			XS_SETERR(csio, CAM_REQ_TOO_BIG);
		} else if (mp->error == EINVAL) {
			XS_SETERR(csio, CAM_REQ_INVALID);
		} else {
			XS_SETERR(csio, CAM_UNREC_HBA_ERROR);
		}
		return (retval);
	}
mbxsync:
	if (isp->isp_dblev & ISP_LOGDEBUG1) {
		isp_print_bytes(isp, "Request Queue Entry", QENTRY_LEN, rq);
	}
	switch (rq->req_header.rqs_entry_type) {
	case RQSTYPE_REQUEST:
		isp_put_request(isp, rq, qep);
		break;
	case RQSTYPE_CMDONLY:
		isp_put_extended_request(isp, (ispextreq_t *)rq,
		    (ispextreq_t *)qep);
		break;
	case RQSTYPE_T2RQS:
		if (FCPARAM(isp)->isp_2klogin) {
			isp_put_request_t2e(isp,
			    (ispreqt2e_t *) rq, (ispreqt2e_t *) qep);
		} else {
			isp_put_request_t2(isp,
			    (ispreqt2_t *) rq, (ispreqt2_t *) qep);
		}
		break;
	case RQSTYPE_T3RQS:
		if (FCPARAM(isp)->isp_2klogin) {
			isp_put_request_t3e(isp,
			    (ispreqt3e_t *) rq, (ispreqt3e_t *) qep);
			break;
		}
		/* FALLTHROUGH */
	case RQSTYPE_A64:
		isp_put_request_t3(isp, (ispreqt3_t *) rq, (ispreqt3_t *) qep);
		break;
	case RQSTYPE_T7RQS:
		isp_put_request_t7(isp, (ispreqt7_t *) rq, (ispreqt7_t *) qep);
		break;
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
