/* $Id: $ */
/* release_6_2_99 */
/*
 * PCI specific probe and attach routines for Qlogic ISP SCSI adapters.
 * FreeBSD 2.X Version.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998, 1999 by Matthew Jacob
 * NASA/Ames Research Center
 * All rights reserved.
 *---------------------------------------
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
 * 3. The name of the author may not be used to endorse or promote products
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
#include <dev/isp/isp_freebsd.h>
#include <dev/isp/asm_pci.h>
#include <sys/malloc.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_extern.h>


#include <pci/pcireg.h>
#include <pci/pcivar.h>

static u_int16_t isp_pci_rd_reg __P((struct ispsoftc *, int));
static void isp_pci_wr_reg __P((struct ispsoftc *, int, u_int16_t));
#ifndef ISP_DISABLE_1080_SUPPORT
static u_int16_t isp_pci_rd_reg_1080 __P((struct ispsoftc *, int));
static void isp_pci_wr_reg_1080 __P((struct ispsoftc *, int, u_int16_t));
#endif
static int isp_pci_mbxdma __P((struct ispsoftc *));
static int isp_pci_dmasetup __P((struct ispsoftc *, ISP_SCSI_XFER_T *,
	ispreq_t *, u_int8_t *, u_int8_t));
typedef u_int16_t pci_port_t;

static void isp_pci_reset1 __P((struct ispsoftc *));
static void isp_pci_dumpregs __P((struct ispsoftc *));

#ifndef ISP_DISABLE_1020_SUPPORT
static struct ispmdvec mdvec = {
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	NULL,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP_RISC_CODE,
	ISP_CODE_LENGTH,
	ISP_CODE_ORG,
	ISP_CODE_VERSION,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64,
	0
};
#endif

#ifndef ISP_DISABLE_1080_SUPPORT
static struct ispmdvec mdvec_1080 = {
	isp_pci_rd_reg_1080,
	isp_pci_wr_reg_1080,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	NULL,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP1080_RISC_CODE,
	ISP1080_CODE_LENGTH,
	ISP1080_CODE_ORG,
	ISP1080_CODE_VERSION,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64,
	0
};
#endif

#ifndef ISP_DISABLE_2100_SUPPORT
static struct ispmdvec mdvec_2100 = {
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	NULL,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	ISP2100_RISC_CODE,
	ISP2100_CODE_LENGTH,
	ISP2100_CODE_ORG,
	ISP2100_CODE_VERSION,
	0,			/* Irrelevant to the 2100 */
	0
};

static struct ispmdvec mdvec_2200 = {
	isp_pci_rd_reg,
	isp_pci_wr_reg,
	isp_pci_mbxdma,
	isp_pci_dmasetup,
	NULL,
	NULL,
	isp_pci_reset1,
	isp_pci_dumpregs,
	NULL,
	0,
	ISP2100_CODE_ORG,
	0,
	0,			/* Irrelevant to the 2100 */
	0
};
#endif

#ifndef	SCSI_ISP_PREFER_MEM_MAP
#ifdef	__alpha__
#define	SCSI_ISP_PREFER_MEM_MAP	0
#else
#define	SCSI_ISP_PREFER_MEM_MAP	1
#endif
#endif

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
#define	PCI_VENDOR_QLOGIC	0x1077
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1020
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1080
#define	PCI_PRODUCT_QLOGIC_ISP1080	0x1080
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1240
#define	PCI_PRODUCT_QLOGIC_ISP1240	0x1240
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2100
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP2200
#define	PCI_PRODUCT_QLOGIC_ISP2200	0x2200
#endif

#define	PCI_QLOGIC_ISP	((PCI_PRODUCT_QLOGIC_ISP1020 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1080	\
	((PCI_PRODUCT_QLOGIC_ISP1080 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP1240	\
	((PCI_PRODUCT_QLOGIC_ISP1240 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2100	\
	((PCI_PRODUCT_QLOGIC_ISP2100 << 16) | PCI_VENDOR_QLOGIC)

#define	PCI_QLOGIC_ISP2200	\
	((PCI_PRODUCT_QLOGIC_ISP2200 << 16) | PCI_VENDOR_QLOGIC)

#define	IO_MAP_REG	0x10
#define	MEM_MAP_REG	0x14

#define	PCI_DFLT_LTNCY	0x40
#define	PCI_DFLT_LNSZ	0x10

static char *isp_pci_probe __P((pcici_t tag, pcidi_t type));
static void isp_pci_attach __P((pcici_t config_d, int unit));

/* This distinguishing define is not right, but it does work */
 
#define	IO_SPACE_MAPPING	0
#define	MEM_SPACE_MAPPING	1
typedef int bus_space_tag_t;
typedef u_long bus_space_handle_t;
typedef unsigned int            __uintptr_t;
typedef __uintptr_t     uintptr_t;
#ifdef __alpha__
#define	bus_space_read_2(st, sh, offset)	\
	alpha_mb(),
	(st == IO_SPACE_MAPPING)? \
		inw((pci_port_t)sh + offset) : readw((pci_port_t)sh + offset)
#define	bus_space_write_2(st, sh, offset, val)	\
	((st == IO_SPACE_MAPPING)? outw((pci_port_t)sh + offset, val) : \
                writew((pci_port_t)sh + offset, val)), alpha_mb()
#else
#define	bus_space_read_2(st, sh, offset)	\
	(st == IO_SPACE_MAPPING)? \
		inw((pci_port_t)sh + offset) : *((u_int16_t *)(uintptr_t)sh)
#define	bus_space_write_2(st, sh, offset, val)	\
	if (st == IO_SPACE_MAPPING) outw((pci_port_t)sh + offset, val); else \
		*((u_int16_t *)(uintptr_t)sh) = val
#endif

struct isp_pcisoftc {
	struct ispsoftc			pci_isp;
        pcici_t				pci_id;
	bus_space_tag_t			pci_st;
	bus_space_handle_t		pci_sh;
	int16_t				pci_poff[_NREG_BLKS];
};

static u_long ispunit;

static struct pci_device isp_pci_driver = {
	"isp",
	isp_pci_probe,
	isp_pci_attach,
	&ispunit,
	NULL
};
DATA_SET (pcidevice_set, isp_pci_driver);


static char *
isp_pci_probe(pcici_t tag, pcidi_t type)
{
	static int oneshot = 1;
	char *x;

        switch (type) {
#ifndef	ISP_DISABLE_1020_SUPPORT
	case PCI_QLOGIC_ISP:
		x = "Qlogic ISP 1020/1040 PCI SCSI Adapter";
		break;
#endif
#ifndef	ISP_DISABLE_1080_SUPPORT
	case PCI_QLOGIC_ISP1080:
		x = "Qlogic ISP 1080 PCI SCSI Adapter";
		break;
	case PCI_QLOGIC_ISP1240:
		x = "Qlogic ISP 1240 PCI SCSI Adapter";
		break;
#endif
#ifndef	ISP_DISABLE_2100_SUPPORT
	case PCI_QLOGIC_ISP2200:
		x = "Qlogic ISP 2200 PCI FC-AL Adapter";
		break;
	case PCI_QLOGIC_ISP2100:
		x = "Qlogic ISP 2100 PCI FC-AL Adapter";
		break;
#endif
	default:
		return (NULL);
	}
	if (oneshot) {
		oneshot = 0;
		printf("%s Version %d.%d, Core Version %d.%d\n", PVS,
		    ISP_PLATFORM_VERSION_MAJOR, ISP_PLATFORM_VERSION_MINOR,
		    ISP_CORE_VERSION_MAJOR, ISP_CORE_VERSION_MINOR);
	}
	return (x);
}


#if	SCSI_ISP_PREFER_MEM_MAP == 1
#define	prefer_mem_map	1
#else
#define	prefer_mem_map	0
#endif

static void
isp_pci_attach(pcici_t cfid, int unit)
{
	int mapped;
	pci_port_t io_port;
	u_int32_t data, linesz, psize, basetype;
	struct isp_pcisoftc *pcs;
	struct ispsoftc *isp;
	vm_offset_t vaddr, paddr;
	struct ispmdvec *mdvp;
	long i;
	ISP_LOCKVAL_DECL;


	pcs = malloc(sizeof (struct isp_pcisoftc), M_DEVBUF, M_NOWAIT);
	if (pcs == NULL) {
		printf("isp%d: cannot allocate softc\n", unit);
		return;
	}
	bzero(pcs, sizeof (struct isp_pcisoftc));

	/*
	 * Figure out which we should try first - memory mapping or i/o mapping?
	 */

	vaddr = paddr = NULL;
	mapped = 0;
	linesz = PCI_DFLT_LNSZ;

	/*
	 * Note that pci_conf_read is a 32 bit word aligned function.
	 */
	data = pci_conf_read(cfid, PCIR_COMMAND);
	if (prefer_mem_map) {
		if (data & PCI_COMMAND_MEM_ENABLE) {
			if (pci_map_mem(cfid, MEM_MAP_REG, &vaddr, &paddr)) {
				pcs->pci_st = MEM_SPACE_MAPPING;
				pcs->pci_sh = vaddr;
				mapped++;
			}
		}
		if (mapped == 0 && (data & PCI_COMMAND_IO_ENABLE)) {
			if (pci_map_port(cfid, PCI_MAP_REG_START, &io_port)) {
				pcs->pci_st = IO_SPACE_MAPPING;
				pcs->pci_sh = io_port;
				mapped++;
			}
		}
	} else {
		if (data & PCI_COMMAND_IO_ENABLE) {
			if (pci_map_port(cfid, PCI_MAP_REG_START, &io_port)) {
				pcs->pci_st = IO_SPACE_MAPPING;
				pcs->pci_sh = io_port;
				mapped++;
			}
		}
		if (mapped == 0 && (data & PCI_COMMAND_MEM_ENABLE)) {
			if (pci_map_mem(cfid, MEM_MAP_REG, &vaddr, &paddr)) {
				pcs->pci_st = MEM_SPACE_MAPPING;
				pcs->pci_sh = vaddr;
				mapped++;
			}
		}
	}
	if (mapped == 0) {
		printf("isp%d: unable to map any ports!\n", unit);
		free(pcs, M_DEVBUF);
		return;
	}
	if (bootverbose)
		printf("isp%d: using %s space register mapping\n", unit,
		    pcs->pci_st == IO_SPACE_MAPPING? "I/O" : "Memory");

	data = pci_conf_read(cfid, PCI_ID_REG);
	pcs->pci_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = PCI_MBOX_REGS_OFF;
	pcs->pci_poff[SXP_BLOCK >> _BLK_REG_SHFT] = PCI_SXP_REGS_OFF;
	pcs->pci_poff[RISC_BLOCK >> _BLK_REG_SHFT] = PCI_RISC_REGS_OFF;
	pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;
	/*
 	 * GCC!
	 */
	mdvp = &mdvec;
	basetype = ISP_HA_SCSI_UNKNOWN;
	psize = sizeof (sdparam);
#ifndef	ISP_DISABLE_1020_SUPPORT
	if (data == PCI_QLOGIC_ISP) {
		mdvp = &mdvec;
		basetype = ISP_HA_SCSI_UNKNOWN;
		psize = sizeof (sdparam);
	}
#endif
#ifndef	ISP_DISABLE_1080_SUPPORT
	if (data == PCI_QLOGIC_ISP1080) {
		mdvp = &mdvec_1080;
		basetype = ISP_HA_SCSI_1080;
		psize = sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
	if (data == PCI_QLOGIC_ISP1240) {
		mdvp = &mdvec_1080;
		basetype = ISP_HA_SCSI_12X0;
		psize = 2 * sizeof (sdparam);
		pcs->pci_poff[DMA_BLOCK >> _BLK_REG_SHFT] =
		    ISP1080_DMA_REGS_OFF;
	}
#endif
#ifndef	ISP_DISABLE_2100_SUPPORT
	if (data == PCI_QLOGIC_ISP2100) {
		mdvp = &mdvec_2100;
		basetype = ISP_HA_FC_2100;
		psize = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
		data = pci_conf_read(cfid, PCI_CLASS_REG);
		if ((data & 0xff) < 3) {
			/*
			 * XXX: Need to get the actual revision
			 * XXX: number of the 2100 FB. At any rate,
			 * XXX: lower cache line size for early revision
			 * XXX; boards.
			 */
			linesz = 1;
		}
	}
	if (data == PCI_QLOGIC_ISP2200) {
		mdvp = &mdvec_2200;
		basetype = ISP_HA_FC_2200;
		psize = sizeof (fcparam);
		pcs->pci_poff[MBOX_BLOCK >> _BLK_REG_SHFT] =
		    PCI_MBOX_REGS2100_OFF;
	}
#endif
	isp = &pcs->pci_isp;
	isp->isp_param = malloc(psize, M_DEVBUF, M_NOWAIT);
	if (isp->isp_param == NULL) {
		printf("isp%d: cannot allocate parameter data\n", unit);
		return;
	}
	bzero(isp->isp_param, psize);
	isp->isp_mdvec = mdvp;
	isp->isp_type = basetype;
	(void) sprintf(isp->isp_name, "isp%d", unit);
	isp->isp_osinfo.unit = unit;
	/*
	 * poor man's attempt at pseudo randomness.
	 */
	i = (long) isp;
	i >>= 5;
	i &= 0x7;
	/*
	 * This isn't very random, but it's the best we can do for
	 * the real edge case of cards that don't have WWNs.
	 */
	isp->isp_osinfo.seed += ((int) cfid.tag);
	while (version[i])
		isp->isp_osinfo.seed += (int) version[i++];
	isp->isp_osinfo.seed <<= 8;
	isp->isp_osinfo.seed += (unit + 1);

	ISP_LOCK(isp);
	data = pci_conf_read(cfid, PCIR_COMMAND);
	data |=	PCIM_CMD_SEREN		|
		PCIM_CMD_PERRESPEN	|
		PCIM_CMD_BUSMASTEREN	|
		PCIM_CMD_INVEN;
	pci_conf_write(cfid, PCIR_COMMAND, data);
	data = pci_conf_read(cfid, PCIR_CACHELNSZ);
	if ((data & ~0xffff) != ((PCI_DFLT_LTNCY << 8) | linesz)) {
		data &= ~0xffff;
		data |= (PCI_DFLT_LTNCY << 8) | linesz;
		pci_conf_write(cfid, PCIR_CACHELNSZ, data);
		printf("%s: set PCI line size to %d\n", isp->isp_name, linesz);
		printf("%s: set PCI latency to %d\n", isp->isp_name,
		    PCI_DFLT_LTNCY);
	}

	/*
	 * Make sure we've disabled the ROM.
	 */
	data = pci_conf_read(cfid, PCIR_ROMADDR);
	data &= ~1;
	pci_conf_write(cfid, PCIR_ROMADDR, data);
	ISP_UNLOCK(isp);
	if (pci_map_int(cfid, (void (*)(void *))isp_intr,
	    (void *)isp, &IMASK) == 0) {
		printf("%s: could not map interrupt\n", isp->isp_name);
		free(pcs, M_DEVBUF);
		return;
	}

	pcs->pci_id = cfid;
#ifdef	SCSI_ISP_NO_FWLOAD_MASK
	if (SCSI_ISP_NO_FWLOAD_MASK && (SCSI_ISP_NO_FWLOAD_MASK & (1 << unit)))
		isp->isp_confopts |= ISP_CFG_NORELOAD;
#endif
#ifdef	SCSI_ISP_NO_NVRAM_MASK
	if (SCSI_ISP_NO_NVRAM_MASK && (SCSI_ISP_NO_NVRAM_MASK & (1 << unit))) {
		printf("%s: ignoring NVRAM\n", isp->isp_name);
		isp->isp_confopts |= ISP_CFG_NONVRAM;
	}
#endif
#ifdef	SCSI_ISP_FCDUPLEX
	if (IS_FC(isp)) {
		if (SCSI_ISP_FCDUPLEX && (SCSI_ISP_FCDUPLEX & (1 << unit))) {
			isp->isp_confopts |= ISP_CFG_FULL_DUPLEX;
		}
	}
#endif
	ISP_LOCK(isp);
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		(void) pci_unmap_int(cfid);
		ISP_UNLOCK(isp);
		free(pcs, M_DEVBUF);
		return;
	}
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		/* If we're a Fibre Channel Card, we allow deferred attach */
		if (IS_SCSI(isp)) {
			isp_uninit(isp);
			(void) pci_unmap_int(cfid); /* Does nothing */
			ISP_UNLOCK(isp);
			free(pcs, M_DEVBUF);
			return;
		}
	}
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		/* If we're a Fibre Channel Card, we allow deferred attach */
		if (IS_SCSI(isp)) {
			isp_uninit(isp);
			(void) pci_unmap_int(cfid); /* Does nothing */
			ISP_UNLOCK(isp);
			free(pcs, M_DEVBUF);
			return;
		}
	}
	ISP_UNLOCK(isp);
#ifdef __alpha__
	/*
	 * THIS SHOULD NOT HAVE TO BE HERE
	 */
	alpha_register_pci_scsi(cfid->bus, cfid->slot, isp->isp_sim);
#endif	
}

static u_int16_t
isp_pci_rd_reg(struct ispsoftc *isp, int regoff)
{
	u_int16_t rv;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oldconf | BIU_PCI_CONF1_SXP);
	}
	offset = pcs->pci_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	rv = bus_space_read_2(pcs->pci_st, pcs->pci_sh, offset);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		isp_pci_wr_reg(isp, BIU_CONF1, oldconf);
	}
	return (rv);
}

static void
isp_pci_wr_reg(struct ispsoftc *isp, int regoff, u_int16_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oldconf = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldconf = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oldconf | BIU_PCI_CONF1_SXP);
	}
	offset = pcs->pci_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, offset, val);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		isp_pci_wr_reg(isp, BIU_CONF1, oldconf);
	}
}

#ifndef	ISP_DISABLE_1080_SUPPORT
static u_int16_t
isp_pci_rd_reg_1080(struct ispsoftc *isp, int regoff)
{
	u_int16_t rv;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oc | BIU_PCI1080_CONF1_SXP);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oc | BIU_PCI1080_CONF1_DMA);
	}
	offset = pcs->pci_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	rv = bus_space_read_2(pcs->pci_st, pcs->pci_sh, offset);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    ((regoff & _BLK_REG_MASK) == DMA_BLOCK)) {
		isp_pci_wr_reg(isp, BIU_CONF1, oc);
	}
	return (rv);
}

static void
isp_pci_wr_reg_1080(struct ispsoftc *isp, int regoff, u_int16_t val)
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oc = 0;

	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK) {
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oc = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oc | BIU_PCI1080_CONF1_SXP);
	} else if ((regoff & _BLK_REG_MASK) == DMA_BLOCK) {
		oc = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oc | BIU_PCI1080_CONF1_DMA);
	}
	offset = pcs->pci_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, offset, val);
	if ((regoff & _BLK_REG_MASK) == SXP_BLOCK ||
	    ((regoff & _BLK_REG_MASK) == DMA_BLOCK)) {
		isp_pci_wr_reg(isp, BIU_CONF1, oc);
	}
}
#endif

static int
isp_pci_mbxdma(struct ispsoftc *isp)
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	u_int32_t len;
	int rseg;

	/* XXXX CHECK FOR ALIGNMENT */
	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN);
	isp->isp_rquest = malloc(len, M_DEVBUF, M_NOWAIT);
	if (isp->isp_rquest == NULL) {
		printf("%s: cannot malloc request queue\n", isp->isp_name);
		return (1);
	}
	isp->isp_rquest_dma = vtophys(isp->isp_rquest);

#if	0
	printf("RQUEST=0x%x (0x%x)...", isp->isp_rquest, isp->isp_rquest_dma);
#endif

	/*
	 * Allocate and map the result queue.
	 */
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN);
	isp->isp_result = malloc(len, M_DEVBUF, M_NOWAIT);
	if (isp->isp_result == NULL) {
		free(isp->isp_rquest, M_DEVBUF);
		printf("%s: cannot malloc result queue\n", isp->isp_name);
		return (1);
	}
	isp->isp_result_dma = vtophys(isp->isp_result);
#if	0
	printf("RESULT=0x%x (0x%x)\n", isp->isp_result, isp->isp_result_dma);
#endif
	if (isp->isp_type & ISP_HA_FC) {
		fcparam *fcp = isp->isp_param;
		len = ISP2100_SCRLEN;
		fcp->isp_scratch = (caddr_t)
		    malloc(ISP2100_SCRLEN, M_DEVBUF, M_NOWAIT);
		if (fcp->isp_scratch == NULL) {
			printf("%s: cannot alloc scratch\n", isp->isp_name);
			return (1);
		}
		fcp->isp_scdma = vtophys(fcp->isp_scratch);
	}
	return (0);
}

static int
isp_pci_dmasetup(struct ispsoftc *isp, ISP_SCSI_XFER_T *xs,
	ispreq_t *rq, u_int8_t *iptrp, u_int8_t optr)
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	ispcontreq_t *crq;
	vm_offset_t vaddr;
	int drq, seglim;
	u_int32_t paddr, nextpaddr, datalen, size, *ctrp;

	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		return (CMD_QUEUED);
	}

	if (xs->flags & SCSI_DATA_IN) {
		drq = REQFLAG_DATA_IN;
	} else {
		drq = REQFLAG_DATA_OUT;
	}

	if (isp->isp_type & ISP_HA_FC) {
		seglim = ISP_RQDSEG_T2;
		((ispreqt2_t *)rq)->req_totalcnt = XS_XFRLEN(xs);
		((ispreqt2_t *)rq)->req_flags |= drq;
	} else {
		seglim = ISP_RQDSEG;
		rq->req_flags |= drq;
	}

	datalen = XS_XFRLEN(xs);
	vaddr = (vm_offset_t) xs->data;
	paddr = vtophys(vaddr);

	while (datalen != 0 && rq->req_seg_count < seglim) {
		if (isp->isp_type & ISP_HA_FC) {
			ispreqt2_t *rq2 = (ispreqt2_t *)rq;
			rq2->req_dataseg[rq2->req_seg_count].ds_base = paddr;
			ctrp = &rq2->req_dataseg[rq2->req_seg_count].ds_count;
		} else {
			rq->req_dataseg[rq->req_seg_count].ds_base = paddr;
			ctrp = &rq->req_dataseg[rq->req_seg_count].ds_count;
		}
		nextpaddr = paddr;
		*(ctrp) = 0;

		while (datalen != 0 && paddr == nextpaddr) {
			nextpaddr = (paddr & (~PAGE_MASK)) + PAGE_SIZE;
			size = nextpaddr - paddr;
			if (size > datalen)
				size = datalen;
			*(ctrp) += size;
			vaddr += size;
			datalen -= size;
			if (datalen != 0)
				paddr = vtophys(vaddr);

		}
#if	0
		if (isp->isp_type & ISP_HA_FC) {
			ispreqt2_t *rq2 = (ispreqt2_t *)rq;
			printf("%s: seg0[%d] cnt 0x%x paddr 0x%08x\n",
			    isp->isp_name, rq->req_seg_count,
			    rq2->req_dataseg[rq2->req_seg_count].ds_count,
			    rq2->req_dataseg[rq2->req_seg_count].ds_base);
		} else {
			printf("%s: seg0[%d] cnt 0x%x paddr 0x%08x\n",
			    isp->isp_name, rq->req_seg_count,
			    rq->req_dataseg[rq->req_seg_count].ds_count,
			    rq->req_dataseg[rq->req_seg_count].ds_base);
		}
#endif
		rq->req_seg_count++;
	}



	if (datalen == 0)
		return (CMD_QUEUED);

	paddr = vtophys(vaddr);
	while (datalen > 0) {
		crq = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, *iptrp);
		*iptrp = ISP_NXT_QENTRY(*iptrp, RQUEST_QUEUE_LEN);
		if (*iptrp == optr) {
			printf("%s: Request Queue Overflow\n", isp->isp_name);
			XS_SETERR(xs, HBA_BOTCH);
			return (CMD_EAGAIN);
		}
		rq->req_header.rqs_entry_count++;
		bzero((void *)crq, sizeof (*crq));
		crq->req_header.rqs_entry_count = 1;
		crq->req_header.rqs_entry_type = RQSTYPE_DATASEG;

		for (seglim = 0; datalen != 0 && seglim < ISP_CDSEG; seglim++) {
			crq->req_dataseg[seglim].ds_base = paddr;
			ctrp = &crq->req_dataseg[seglim].ds_count;
			*(ctrp) = 0;
			nextpaddr = paddr;
			while (datalen != 0 && paddr == nextpaddr) {
				nextpaddr = (paddr & (~PAGE_MASK)) + PAGE_SIZE;
				size = nextpaddr - paddr;
				if (size > datalen)
					size = datalen;
				*(ctrp) += size;
				vaddr += size;
				datalen -= size;
				if (datalen != 0)
					paddr = vtophys(vaddr);
			}
#if	0
			printf("%s: seg%d[%d] cnt 0x%x paddr 0x%08x\n",
			    isp->isp_name, rq->req_header.rqs_entry_count-1,
			    seglim, crq->req_dataseg[seglim].ds_count,
			    crq->req_dataseg[seglim].ds_base);
#endif
			rq->req_seg_count++;
		}
	}

	return (CMD_QUEUED);
}

static void
isp_pci_reset1(struct ispsoftc *isp)
{
	/* Make sure the BIOS is disabled */
	isp_pci_wr_reg(isp, HCCR, PCI_HCCR_CMD_BIOS);
}

static void
isp_pci_dumpregs(struct ispsoftc *isp)
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	printf("%s: PCI Status Command/Status=%lx\n", pci->pci_isp.isp_name,
	    pci_conf_read(pci->pci_id, PCIR_COMMAND));
}
