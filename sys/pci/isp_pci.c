/* $FreeBSD$ */
/* $Id: isp_pci.c,v 1.9 1998/04/17 17:44:36 mjacob Exp $ */
/*
 * PCI specific probe and attach routines for Qlogic ISP SCSI adapters.
 * FreeBSD Version.
 *
 *---------------------------------------
 * Copyright (c) 1997, 1998 by Matthew Jacob
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
#include <pci.h>
#if	NPCI > 0

#include <dev/isp/isp_freebsd.h>
#include <dev/isp/asm_pci.h>

#include <pci/pcireg.h>
#include <pci/pcivar.h>

static u_int16_t isp_pci_rd_reg __P((struct ispsoftc *, int));
static void isp_pci_wr_reg __P((struct ispsoftc *, int, u_int16_t));
static int isp_pci_mbxdma __P((struct ispsoftc *));
static int isp_pci_dmasetup __P((struct ispsoftc *, ISP_SCSI_XFER_T *,
	ispreq_t *, u_int8_t *, u_int8_t));

static void isp_pci_reset1 __P((struct ispsoftc *));
static void isp_pci_dumpregs __P((struct ispsoftc *));

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
	BIU_PCI_CONF1_FIFO_64 | BIU_BURST_ENABLE,
	60	/* MAGIC- all known PCI card implementations are 60MHz */
};

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
	BIU_PCI_CONF1_FIFO_64 | BIU_BURST_ENABLE,
	60	/* MAGIC- all known PCI card implementations are 60MHz */
};

#ifndef	PCIM_CMD_INVEN
#define	PCIM_CMD_INVEN	0x10
#endif
#ifndef	PCIM_CMD_BUSMASTEREN
#define	PCIM_CMD_BUSMASTEREN	0x0004
#endif

#ifndef	PCI_VENDOR_QLOGIC
#define	PCI_VENDOR_QLOGIC		0x1077
#endif

#ifndef	PCI_PRODUCT_QLOGIC_ISP1020
#define	PCI_PRODUCT_QLOGIC_ISP1020	0x1020
#endif

#define	PCI_QLOGIC_ISP	\
	((PCI_PRODUCT_QLOGIC_ISP1020 << 16) | PCI_VENDOR_QLOGIC)

#ifndef	PCI_PRODUCT_QLOGIC_ISP2100
#define	PCI_PRODUCT_QLOGIC_ISP2100	0x2100
#endif

#define	PCI_QLOGIC_ISP2100	\
	((PCI_PRODUCT_QLOGIC_ISP2100 << 16) | PCI_VENDOR_QLOGIC)

#define IO_MAP_REG	0x10
#define MEM_MAP_REG	0x14


static char *isp_pci_probe __P((pcici_t tag, pcidi_t type));
static void isp_pci_attach __P((pcici_t config_d, int unit));

 

#define	I386_BUS_SPACE_IO	0
#define	I386_BUS_SPACE_MEM	1
typedef int bus_space_tag_t;
typedef u_long bus_space_handle_t;
#define	bus_space_read_2(st, sh, offset)	\
	(st == I386_BUS_SPACE_IO)? \
		inw((u_int16_t)sh + offset) : *((u_int16_t *) sh)
#define	bus_space_write_2(st, sh, offset, val)	\
	if (st == I386_BUS_SPACE_IO) outw((u_int16_t)sh + offset, val); else \
		*((u_int16_t *) sh) = val


struct isp_pcisoftc {
	struct ispsoftc		pci_isp;
        pcici_t			pci_id;
	bus_space_tag_t		pci_st;
	bus_space_handle_t	pci_sh;
	union {
		sdparam	_x;
		struct {
			fcparam _a;
			char _b[ISP2100_SCRLEN];
		} _y;
	} _z;
};

static u_long isp_unit;

struct pci_device isp_pci_driver = {
	"isp",
	isp_pci_probe,
	isp_pci_attach,
	&isp_unit,
	NULL
};
DATA_SET (pcidevice_set, isp_pci_driver);


static char *
isp_pci_probe(tag, type)
	pcici_t tag;
	pcidi_t type;
{       
	static int oneshot = 1;
	char *x;

        switch (type) {
	case PCI_QLOGIC_ISP:
		x = "Qlogic ISP 10X0 PCI SCSI Adapter";
		break;
	case PCI_QLOGIC_ISP2100:
		x = "Qlogic ISP 2100 PCI FC-AL Adapter";
		break;
	default:
		return (NULL);
	}
	if (oneshot) {
		oneshot = 0;
		printf("***Qlogic ISP Driver, FreeBSD NonCam Version\n***%s\n",
			ISP_VERSION_STRING);
	}
	return (x);
}


static void    
isp_pci_attach(config_id, unit)
        pcici_t config_id;
        int unit;
{
	int mapped;
	u_int16_t io_port;
	u_int32_t data;
	struct isp_pcisoftc *pcs;
	struct ispsoftc *isp;
	vm_offset_t vaddr, paddr;
	ISP_LOCKVAL_DECL;


	pcs = malloc(sizeof (struct isp_pcisoftc), M_DEVBUF, M_NOWAIT);
	if (pcs == NULL) {
		printf("isp%ld: cannot allocate softc\n", unit);
		return;
	}
	bzero(pcs, sizeof (struct isp_pcisoftc));

	vaddr = paddr = NULL;
	mapped = 0;
	data = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
	if (mapped == 0 && (data & PCI_COMMAND_IO_ENABLE)) {
		if (pci_map_port(config_id, PCI_MAP_REG_START, &io_port)) {
			pcs->pci_st = I386_BUS_SPACE_IO;
			pcs->pci_sh = io_port;
			mapped++;
		}
	}
	if (mapped == 0 && (data & PCI_COMMAND_MEM_ENABLE)) {
		if (pci_map_mem(config_id, PCI_MAP_REG_START, &vaddr, &paddr)) {
			pcs->pci_st = I386_BUS_SPACE_MEM;
			pcs->pci_sh = vaddr;
			mapped++;
		}
	}
	if (mapped == 0) {
		printf("isp%ld: unable to map any ports!\n", unit);
		free(pcs, M_DEVBUF);
		return;
	}
	printf("isp%d: using %s space register mapping\n", unit,
		pcs->pci_st == I386_BUS_SPACE_IO? "I/O" : "Memory");

	isp = &pcs->pci_isp;
	(void) sprintf(isp->isp_name, "isp%d", unit);
	isp->isp_osinfo.unit = unit;

	data = pci_conf_read(config_id, PCI_ID_REG);
	if (data == PCI_QLOGIC_ISP) {
		isp->isp_mdvec = &mdvec;
		isp->isp_type = ISP_HA_SCSI_UNKNOWN;
		isp->isp_param = &pcs->_z._x;
	} else if (data == PCI_QLOGIC_ISP2100) {
		isp->isp_mdvec = &mdvec_2100;
		isp->isp_type = ISP_HA_FC_2100;
		isp->isp_param = &pcs->_z._y._a;

		ISP_LOCK;
		data = pci_conf_read(config_id, PCI_COMMAND_STATUS_REG);
		data |= PCIM_CMD_BUSMASTEREN | PCIM_CMD_INVEN;
		pci_conf_write(config_id, PCI_COMMAND_STATUS_REG, data);

		/*
		 * Wierd- we need to clear the lsb in offset 0x30 to take the
		 * chip out of reset state.
		 */
		data = pci_conf_read(config_id, 0x30);
		data &= ~1;
		pci_conf_write(config_id, 0x30, data);
		ISP_UNLOCK;
	} else {
		free(pcs, M_DEVBUF);
		return;
	}

	if (pci_map_int(config_id, (void (*)(void *))isp_intr,
	    (void *)isp, &IMASK) == 0) {
		printf("%s: could not map interrupt\n");
		free(pcs, M_DEVBUF);
		return;
	}

	pcs->pci_id = config_id;
	ISP_LOCK;
	isp_reset(isp);
	if (isp->isp_state != ISP_RESETSTATE) {
		ISP_UNLOCK;
		free(pcs, M_DEVBUF);
		return;
	}
	isp_init(isp);
	if (isp->isp_state != ISP_INITSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK;
		free(pcs, M_DEVBUF);
		return;
	}
	isp_attach(isp);
	if (isp->isp_state != ISP_RUNSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK;
		free(pcs, M_DEVBUF);
		return;
	}
	ISP_UNLOCK;
}

#define  PCI_BIU_REGS_OFF		BIU_REGS_OFF

static u_int16_t
isp_pci_rd_reg(isp, regoff)
	struct ispsoftc *isp;
	int regoff;
{
	u_int16_t rv;
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oldsxp = 0;

	if ((regoff & BIU_BLOCK) != 0) {
		offset = PCI_BIU_REGS_OFF;
	} else if ((regoff & MBOX_BLOCK) != 0) {
		if (isp->isp_type & ISP_HA_SCSI)
			offset = PCI_MBOX_REGS_OFF;
		else
			offset = PCI_MBOX_REGS2100_OFF;
	} else if ((regoff & SXP_BLOCK) != 0) {
		offset = PCI_SXP_REGS_OFF;
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldsxp = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oldsxp & ~BIU_PCI_CONF1_SXP);
	} else {
		offset = PCI_RISC_REGS_OFF;
	}
	regoff &= 0xff;
	offset += regoff;
	rv = bus_space_read_2(pcs->pci_st, pcs->pci_sh, offset);
	if ((regoff & SXP_BLOCK) != 0) {
		isp_pci_wr_reg(isp, BIU_CONF1, oldsxp);
	}
	return (rv);
}

static void
isp_pci_wr_reg(isp, regoff, val)
	struct ispsoftc *isp;
	int regoff;
	u_int16_t val;
{
	struct isp_pcisoftc *pcs = (struct isp_pcisoftc *) isp;
	int offset, oldsxp = 0;
	if ((regoff & BIU_BLOCK) != 0) {
		offset = PCI_BIU_REGS_OFF;
	} else if ((regoff & MBOX_BLOCK) != 0) {
		if (isp->isp_type & ISP_HA_SCSI)
			offset = PCI_MBOX_REGS_OFF;
		else
			offset = PCI_MBOX_REGS2100_OFF;
	} else if ((regoff & SXP_BLOCK) != 0) {
		offset = PCI_SXP_REGS_OFF;
		/*
		 * We will assume that someone has paused the RISC processor.
		 */
		oldsxp = isp_pci_rd_reg(isp, BIU_CONF1);
		isp_pci_wr_reg(isp, BIU_CONF1, oldsxp & ~BIU_PCI_CONF1_SXP);
	} else {
		offset = PCI_RISC_REGS_OFF;
	}
	regoff &= 0xff;
	offset += regoff;
	bus_space_write_2(pcs->pci_st, pcs->pci_sh, offset, val);
	if ((regoff & SXP_BLOCK) != 0) {
		isp_pci_wr_reg(isp, BIU_CONF1, oldsxp);
	}
}

static int
isp_pci_mbxdma(isp)
	struct ispsoftc *isp;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	u_int32_t len;
	int rseg;

	/* XXXX CHECK FOR ALIGNMENT */
	/*
	 * Allocate and map the request queue.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
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
	len = ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));
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
		fcp->isp_scratch = (volatile caddr_t) &pci->_z._y._b;
		fcp->isp_scdma = vtophys(fcp->isp_scratch);
	}
	return (0);
}

static int
isp_pci_dmasetup(isp, xs, rq, iptrp, optr)
	struct ispsoftc *isp;
	ISP_SCSI_XFER_T *xs;
	ispreq_t *rq;
	u_int8_t *iptrp;
	u_int8_t optr;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	ispcontreq_t *crq;
	vm_offset_t vaddr;
	int drq, seglim;
	u_int32_t paddr, nextpaddr, datalen, size, *ctrp;

	if (xs->datalen == 0) {
		rq->req_seg_count = 1;
		return (0);
	}

	if (xs->flags & SCSI_DATA_IN) {
		drq = REQFLAG_DATA_IN;
	} else {
		drq = REQFLAG_DATA_OUT;
	}

	if (isp->isp_type & ISP_HA_FC) {
		seglim = ISP_RQDSEG_T2;
		((ispreqt2_t *)rq)->req_totalcnt = xs->datalen;
		((ispreqt2_t *)rq)->req_flags |= drq;
	} else {
		seglim = ISP_RQDSEG;
		rq->req_flags |= drq;
	}

	datalen = xs->datalen;;
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
		return (0);

	paddr = vtophys(vaddr);
	while (datalen > 0) {
		crq = (ispcontreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, *iptrp);
		*iptrp = (*iptrp + 1) & (RQUEST_QUEUE_LEN(isp) - 1);
		if (*iptrp == optr) {
			printf("%s: Request Queue Overflow\n", isp->isp_name);
			return (EFBIG);
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

	return (0);
}

static void
isp_pci_reset1(isp)
	struct ispsoftc *isp;
{
	/* Make sure the BIOS is disabled */
	isp_pci_wr_reg(isp, HCCR, PCI_HCCR_CMD_BIOS);
}

static void
isp_pci_dumpregs(isp)
	struct ispsoftc *isp;
{
	struct isp_pcisoftc *pci = (struct isp_pcisoftc *)isp;
	printf("%s: PCI Status Command/Status=%x\n", pci->pci_isp.isp_name,
	    pci_conf_read(pci->pci_id, PCI_COMMAND_STATUS_REG));
}
#endif
