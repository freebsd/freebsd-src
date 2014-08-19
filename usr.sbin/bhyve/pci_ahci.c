/*-
 * Copyright (c) 2013  Zhixiang Yu <zcore@freebsd.org>
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND
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
 *
 * $FreeBSD$
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/linker_set.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/disk.h>
#include <sys/ata.h>
#include <sys/endian.h>

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <inttypes.h>

#include "bhyverun.h"
#include "pci_emul.h"
#include "ahci.h"
#include "block_if.h"

#define	MAX_PORTS	6	/* Intel ICH8 AHCI supports 6 ports */

#define	PxSIG_ATA	0x00000101 /* ATA drive */
#define	PxSIG_ATAPI	0xeb140101 /* ATAPI drive */

enum sata_fis_type {
	FIS_TYPE_REGH2D		= 0x27,	/* Register FIS - host to device */
	FIS_TYPE_REGD2H		= 0x34,	/* Register FIS - device to host */
	FIS_TYPE_DMAACT		= 0x39,	/* DMA activate FIS - device to host */
	FIS_TYPE_DMASETUP	= 0x41,	/* DMA setup FIS - bidirectional */
	FIS_TYPE_DATA		= 0x46,	/* Data FIS - bidirectional */
	FIS_TYPE_BIST		= 0x58,	/* BIST activate FIS - bidirectional */
	FIS_TYPE_PIOSETUP	= 0x5F,	/* PIO setup FIS - device to host */
	FIS_TYPE_SETDEVBITS	= 0xA1,	/* Set dev bits FIS - device to host */
};

/*
 * SCSI opcodes
 */
#define	TEST_UNIT_READY		0x00
#define	REQUEST_SENSE		0x03
#define	INQUIRY			0x12
#define	START_STOP_UNIT		0x1B
#define	PREVENT_ALLOW		0x1E
#define	READ_CAPACITY		0x25
#define	READ_10			0x28
#define	POSITION_TO_ELEMENT	0x2B
#define	READ_TOC		0x43
#define	GET_EVENT_STATUS_NOTIFICATION 0x4A
#define	MODE_SENSE_10		0x5A
#define	READ_12			0xA8
#define	READ_CD			0xBE

/*
 * SCSI mode page codes
 */
#define	MODEPAGE_RW_ERROR_RECOVERY	0x01
#define	MODEPAGE_CD_CAPABILITIES	0x2A

/*
 * ATA commands
 */
#define	ATA_SF_ENAB_SATA_SF		0x10
#define		ATA_SATA_SF_AN		0x05
#define	ATA_SF_DIS_SATA_SF		0x90

/*
 * Debug printf
 */
#ifdef AHCI_DEBUG
static FILE *dbg;
#define DPRINTF(format, arg...)	do{fprintf(dbg, format, ##arg);fflush(dbg);}while(0)
#else
#define DPRINTF(format, arg...)
#endif
#define WPRINTF(format, arg...) printf(format, ##arg)

struct ahci_ioreq {
	struct blockif_req io_req;
	struct ahci_port *io_pr;
	STAILQ_ENTRY(ahci_ioreq) io_list;
	uint8_t *cfis;
	uint32_t len;
	uint32_t done;
	int slot;
	int prdtl;
};

struct ahci_port {
	struct blockif_ctxt *bctx;
	struct pci_ahci_softc *pr_sc;
	uint8_t *cmd_lst;
	uint8_t *rfis;
	int atapi;
	int reset;
	int mult_sectors;
	uint8_t xfermode;
	uint8_t sense_key;
	uint8_t asc;
	uint32_t pending;

	uint32_t clb;
	uint32_t clbu;
	uint32_t fb;
	uint32_t fbu;
	uint32_t is;
	uint32_t ie;
	uint32_t cmd;
	uint32_t unused0;
	uint32_t tfd;
	uint32_t sig;
	uint32_t ssts;
	uint32_t sctl;
	uint32_t serr;
	uint32_t sact;
	uint32_t ci;
	uint32_t sntf;
	uint32_t fbs;

	/*
	 * i/o request info
	 */
	struct ahci_ioreq *ioreq;
	int ioqsz;
	STAILQ_HEAD(ahci_fhead, ahci_ioreq) iofhd;
};

struct ahci_cmd_hdr {
	uint16_t flags;
	uint16_t prdtl;
	uint32_t prdbc;
	uint64_t ctba;
	uint32_t reserved[4];
};

struct ahci_prdt_entry {
	uint64_t dba;
	uint32_t reserved;
#define	DBCMASK		0x3fffff
	uint32_t dbc;
};

struct pci_ahci_softc {
	struct pci_devinst *asc_pi;
	pthread_mutex_t	mtx;
	int ports;
	uint32_t cap;
	uint32_t ghc;
	uint32_t is;
	uint32_t pi;
	uint32_t vs;
	uint32_t ccc_ctl;
	uint32_t ccc_pts;
	uint32_t em_loc;
	uint32_t em_ctl;
	uint32_t cap2;
	uint32_t bohc;
	uint32_t lintr;
	struct ahci_port port[MAX_PORTS];
};
#define	ahci_ctx(sc)	((sc)->asc_pi->pi_vmctx)

static inline void lba_to_msf(uint8_t *buf, int lba)
{
	lba += 150;
	buf[0] = (lba / 75) / 60;
	buf[1] = (lba / 75) % 60;
	buf[2] = lba % 75;
}

/*
 * generate HBA intr depending on whether or not ports within
 * the controller have an interrupt pending.
 */
static void
ahci_generate_intr(struct pci_ahci_softc *sc)
{
	struct pci_devinst *pi;
	int i;

	pi = sc->asc_pi;

	for (i = 0; i < sc->ports; i++) {
		struct ahci_port *pr;
		pr = &sc->port[i];
		if (pr->is & pr->ie)
			sc->is |= (1 << i);
	}

	DPRINTF("%s %x\n", __func__, sc->is);

	if (sc->is && (sc->ghc & AHCI_GHC_IE)) {		
		if (pci_msi_enabled(pi)) {
			/*
			 * Generate an MSI interrupt on every edge
			 */
			pci_generate_msi(pi, 0);
		} else if (!sc->lintr) {
			/*
			 * Only generate a pin-based interrupt if one wasn't
			 * in progress
			 */
			sc->lintr = 1;
			pci_lintr_assert(pi);
		}
	} else if (sc->lintr) {
		/*
		 * No interrupts: deassert pin-based signal if it had
		 * been asserted
		 */
		pci_lintr_deassert(pi);
		sc->lintr = 0;
	}
}

static void
ahci_write_fis(struct ahci_port *p, enum sata_fis_type ft, uint8_t *fis)
{
	int offset, len, irq;

	if (p->rfis == NULL || !(p->cmd & AHCI_P_CMD_FRE))
		return;

	switch (ft) {
	case FIS_TYPE_REGD2H:
		offset = 0x40;
		len = 20;
		irq = AHCI_P_IX_DHR;
		break;
	case FIS_TYPE_SETDEVBITS:
		offset = 0x58;
		len = 8;
		irq = AHCI_P_IX_SDB;
		break;
	case FIS_TYPE_PIOSETUP:
		offset = 0x20;
		len = 20;
		irq = 0;
		break;
	default:
		WPRINTF("unsupported fis type %d\n", ft);
		return;
	}
	memcpy(p->rfis + offset, fis, len);
	if (irq) {
		p->is |= irq;
		ahci_generate_intr(p->pr_sc);
	}
}

static void
ahci_write_fis_piosetup(struct ahci_port *p)
{
	uint8_t fis[20];

	memset(fis, 0, sizeof(fis));
	fis[0] = FIS_TYPE_PIOSETUP;
	ahci_write_fis(p, FIS_TYPE_PIOSETUP, fis);
}

static void
ahci_write_fis_sdb(struct ahci_port *p, int slot, uint32_t tfd)
{
	uint8_t fis[8];
	uint8_t error;

	error = (tfd >> 8) & 0xff;
	memset(fis, 0, sizeof(fis));
	fis[0] = error;
	fis[2] = tfd & 0x77;
	*(uint32_t *)(fis + 4) = (1 << slot);
	if (fis[2] & ATA_S_ERROR)
		p->is |= AHCI_P_IX_TFE;
	p->tfd = tfd;
	ahci_write_fis(p, FIS_TYPE_SETDEVBITS, fis);
}

static void
ahci_write_fis_d2h(struct ahci_port *p, int slot, uint8_t *cfis, uint32_t tfd)
{
	uint8_t fis[20];
	uint8_t error;

	error = (tfd >> 8) & 0xff;
	memset(fis, 0, sizeof(fis));
	fis[0] = FIS_TYPE_REGD2H;
	fis[1] = (1 << 6);
	fis[2] = tfd & 0xff;
	fis[3] = error;
	fis[4] = cfis[4];
	fis[5] = cfis[5];
	fis[6] = cfis[6];
	fis[7] = cfis[7];
	fis[8] = cfis[8];
	fis[9] = cfis[9];
	fis[10] = cfis[10];
	fis[11] = cfis[11];
	fis[12] = cfis[12];
	fis[13] = cfis[13];
	if (fis[2] & ATA_S_ERROR)
		p->is |= AHCI_P_IX_TFE;
	p->tfd = tfd;
	p->ci &= ~(1 << slot);
	ahci_write_fis(p, FIS_TYPE_REGD2H, fis);
}

static void
ahci_write_reset_fis_d2h(struct ahci_port *p)
{
	uint8_t fis[20];

	memset(fis, 0, sizeof(fis));
	fis[0] = FIS_TYPE_REGD2H;
	fis[3] = 1;
	fis[4] = 1;
	if (p->atapi) {
		fis[5] = 0x14;
		fis[6] = 0xeb;
	}
	fis[12] = 1;
	ahci_write_fis(p, FIS_TYPE_REGD2H, fis);
}

static void
ahci_port_reset(struct ahci_port *pr)
{
	pr->sctl = 0;
	pr->serr = 0;
	pr->sact = 0;
	pr->xfermode = ATA_UDMA6;
	pr->mult_sectors = 128;

	if (!pr->bctx) {
		pr->ssts = ATA_SS_DET_NO_DEVICE;
		pr->sig = 0xFFFFFFFF;
		pr->tfd = 0x7F;
		return;
	}
	pr->ssts = ATA_SS_DET_PHY_ONLINE | ATA_SS_SPD_GEN2 |
		ATA_SS_IPM_ACTIVE;
	pr->tfd = (1 << 8) | ATA_S_DSC | ATA_S_DMA;
	if (!pr->atapi) {
		pr->sig = PxSIG_ATA;
		pr->tfd |= ATA_S_READY;
	} else
		pr->sig = PxSIG_ATAPI;
	ahci_write_reset_fis_d2h(pr);
}

static void
ahci_reset(struct pci_ahci_softc *sc)
{
	int i;

	sc->ghc = AHCI_GHC_AE;
	sc->is = 0;

	if (sc->lintr) {
		pci_lintr_deassert(sc->asc_pi);
		sc->lintr = 0;
	}

	for (i = 0; i < sc->ports; i++) {
		sc->port[i].ie = 0;
		sc->port[i].is = 0;
		ahci_port_reset(&sc->port[i]);
	}
}

static void
ata_string(uint8_t *dest, const char *src, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (*src)
			dest[i ^ 1] = *src++;
		else
			dest[i ^ 1] = ' ';
	}
}

static void
atapi_string(uint8_t *dest, const char *src, int len)
{
	int i;

	for (i = 0; i < len; i++) {
		if (*src)
			dest[i] = *src++;
		else
			dest[i] = ' ';
	}
}

static void
ahci_handle_dma(struct ahci_port *p, int slot, uint8_t *cfis, uint32_t done,
    int seek)
{
	struct ahci_ioreq *aior;
	struct blockif_req *breq;
	struct pci_ahci_softc *sc;
	struct ahci_prdt_entry *prdt;
	struct ahci_cmd_hdr *hdr;
	uint64_t lba;
	uint32_t len;
	int i, err, iovcnt, ncq, readop;

	sc = p->pr_sc;
	prdt = (struct ahci_prdt_entry *)(cfis + 0x80);
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	ncq = 0;
	readop = 1;

	prdt += seek;
	if (cfis[2] == ATA_WRITE_DMA || cfis[2] == ATA_WRITE_DMA48 ||
			cfis[2] == ATA_WRITE_FPDMA_QUEUED)
		readop = 0;

	if (cfis[2] == ATA_WRITE_FPDMA_QUEUED ||
			cfis[2] == ATA_READ_FPDMA_QUEUED) {
		lba = ((uint64_t)cfis[10] << 40) |
			((uint64_t)cfis[9] << 32) |
			((uint64_t)cfis[8] << 24) |
			((uint64_t)cfis[6] << 16) |
			((uint64_t)cfis[5] << 8) |
			cfis[4];
		len = cfis[11] << 8 | cfis[3];
		if (!len)
			len = 65536;
		ncq = 1;
	} else if (cfis[2] == ATA_READ_DMA48 || cfis[2] == ATA_WRITE_DMA48) {
		lba = ((uint64_t)cfis[10] << 40) |
			((uint64_t)cfis[9] << 32) |
			((uint64_t)cfis[8] << 24) |
			((uint64_t)cfis[6] << 16) |
			((uint64_t)cfis[5] << 8) |
			cfis[4];
		len = cfis[13] << 8 | cfis[12];
		if (!len)
			len = 65536;
	} else {
		lba = ((cfis[7] & 0xf) << 24) | (cfis[6] << 16) |
			(cfis[5] << 8) | cfis[4];
		len = cfis[12];
		if (!len)
			len = 256;
	}
	lba *= blockif_sectsz(p->bctx);
	len *= blockif_sectsz(p->bctx);

	/*
	 * Pull request off free list
	 */
	aior = STAILQ_FIRST(&p->iofhd);
	assert(aior != NULL);
	STAILQ_REMOVE_HEAD(&p->iofhd, io_list);
	aior->cfis = cfis;
	aior->slot = slot;
	aior->len = len;
	aior->done = done;
	breq = &aior->io_req;
	breq->br_offset = lba + done;
	iovcnt = hdr->prdtl - seek;
	if (iovcnt > BLOCKIF_IOV_MAX) {
		aior->prdtl = iovcnt - BLOCKIF_IOV_MAX;
		iovcnt = BLOCKIF_IOV_MAX;
		/*
		 * Mark this command in-flight.
		 */
		p->pending |= 1 << slot;
	} else
		aior->prdtl = 0;
	breq->br_iovcnt = iovcnt;

	/*
	 * Build up the iovec based on the prdt
	 */
	for (i = 0; i < iovcnt; i++) {
		uint32_t dbcsz;

		dbcsz = (prdt->dbc & DBCMASK) + 1;
		breq->br_iov[i].iov_base = paddr_guest2host(ahci_ctx(sc),
		    prdt->dba, dbcsz);
		breq->br_iov[i].iov_len = dbcsz;
		aior->done += dbcsz;
		prdt++;
	}
	if (readop)
		err = blockif_read(p->bctx, breq);
	else
		err = blockif_write(p->bctx, breq);
	assert(err == 0);

	if (ncq)
		p->ci &= ~(1 << slot);
}

static void
ahci_handle_flush(struct ahci_port *p, int slot, uint8_t *cfis)
{
	struct ahci_ioreq *aior;
	struct blockif_req *breq;
	int err;

	/*
	 * Pull request off free list
	 */
	aior = STAILQ_FIRST(&p->iofhd);
	assert(aior != NULL);
	STAILQ_REMOVE_HEAD(&p->iofhd, io_list);
	aior->cfis = cfis;
	aior->slot = slot;
	aior->len = 0;
	aior->done = 0;
	aior->prdtl = 0;
	breq = &aior->io_req;

	err = blockif_flush(p->bctx, breq);
	assert(err == 0);
}

static inline void
write_prdt(struct ahci_port *p, int slot, uint8_t *cfis,
		void *buf, int size)
{
	struct ahci_cmd_hdr *hdr;
	struct ahci_prdt_entry *prdt;
	void *from;
	int i, len;

	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	len = size;
	from = buf;
	prdt = (struct ahci_prdt_entry *)(cfis + 0x80);
	for (i = 0; i < hdr->prdtl && len; i++) {
		uint8_t *ptr;
		uint32_t dbcsz;
		int sublen;

		dbcsz = (prdt->dbc & DBCMASK) + 1;
		ptr = paddr_guest2host(ahci_ctx(p->pr_sc), prdt->dba, dbcsz);
		sublen = len < dbcsz ? len : dbcsz;
		memcpy(ptr, from, sublen);
		len -= sublen;
		from += sublen;
		prdt++;
	}
	hdr->prdbc = size - len;
}

static void
handle_identify(struct ahci_port *p, int slot, uint8_t *cfis)
{
	struct ahci_cmd_hdr *hdr;

	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	if (p->atapi || hdr->prdtl == 0) {
		p->tfd = (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR;
		p->is |= AHCI_P_IX_TFE;
	} else {
		uint16_t buf[256];
		uint64_t sectors;
		uint16_t cyl;
		uint8_t sech, heads;

		sectors = blockif_size(p->bctx) / blockif_sectsz(p->bctx);
		blockif_chs(p->bctx, &cyl, &heads, &sech);
		memset(buf, 0, sizeof(buf));
		buf[0] = 0x0040;
		buf[1] = cyl;
		buf[3] = heads;
		buf[6] = sech;
		/* TODO emulate different serial? */
		ata_string((uint8_t *)(buf+10), "123456", 20);
		ata_string((uint8_t *)(buf+23), "001", 8);
		ata_string((uint8_t *)(buf+27), "BHYVE SATA DISK", 40);
		buf[47] = (0x8000 | 128);
		buf[48] = 0x1;
		buf[49] = (1 << 8 | 1 << 9 | 1 << 11);
		buf[50] = (1 << 14);
		buf[53] = (1 << 1 | 1 << 2);
		if (p->mult_sectors)
			buf[59] = (0x100 | p->mult_sectors);
		buf[60] = sectors;
		buf[61] = (sectors >> 16);
		buf[63] = 0x7;
		if (p->xfermode & ATA_WDMA0)
			buf[63] |= (1 << ((p->xfermode & 7) + 8));
		buf[64] = 0x3;
		buf[65] = 100;
		buf[66] = 100;
		buf[67] = 100;
		buf[68] = 100;
		buf[75] = 31;
		buf[76] = (1 << 8 | 1 << 2);
		buf[80] = 0x1f0;
		buf[81] = 0x28;
		buf[82] = (1 << 5 | 1 << 14);
		buf[83] = (1 << 10 | 1 << 12 | 1 << 13 | 1 << 14);
		buf[84] = (1 << 14);
		buf[85] = (1 << 5 | 1 << 14);
		buf[86] = (1 << 10 | 1 << 12 | 1 << 13);
		buf[87] = (1 << 14);
		buf[88] = 0x7f;
		if (p->xfermode & ATA_UDMA0)
			buf[88] |= (1 << ((p->xfermode & 7) + 8));
		buf[93] = (1 | 1 <<14);
		buf[100] = sectors;
		buf[101] = (sectors >> 16);
		buf[102] = (sectors >> 32);
		buf[103] = (sectors >> 48);
		ahci_write_fis_piosetup(p);
		write_prdt(p, slot, cfis, (void *)buf, sizeof(buf));
		p->tfd = ATA_S_DSC | ATA_S_READY;
		p->is |= AHCI_P_IX_DP;
	}
	p->ci &= ~(1 << slot);
	ahci_generate_intr(p->pr_sc);
}

static void
handle_atapi_identify(struct ahci_port *p, int slot, uint8_t *cfis)
{
	if (!p->atapi) {
		p->tfd = (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR;
		p->is |= AHCI_P_IX_TFE;
	} else {
		uint16_t buf[256];

		memset(buf, 0, sizeof(buf));
		buf[0] = (2 << 14 | 5 << 8 | 1 << 7 | 2 << 5);
		/* TODO emulate different serial? */
		ata_string((uint8_t *)(buf+10), "123456", 20);
		ata_string((uint8_t *)(buf+23), "001", 8);
		ata_string((uint8_t *)(buf+27), "BHYVE SATA DVD ROM", 40);
		buf[49] = (1 << 9 | 1 << 8);
		buf[50] = (1 << 14 | 1);
		buf[53] = (1 << 2 | 1 << 1);
		buf[62] = 0x3f;
		buf[63] = 7;
		buf[64] = 3;
		buf[65] = 100;
		buf[66] = 100;
		buf[67] = 100;
		buf[68] = 100;
		buf[76] = (1 << 2 | 1 << 1);
		buf[78] = (1 << 5);
		buf[80] = (0x1f << 4);
		buf[82] = (1 << 4);
		buf[83] = (1 << 14);
		buf[84] = (1 << 14);
		buf[85] = (1 << 4);
		buf[87] = (1 << 14);
		buf[88] = (1 << 14 | 0x7f);
		ahci_write_fis_piosetup(p);
		write_prdt(p, slot, cfis, (void *)buf, sizeof(buf));
		p->tfd = ATA_S_DSC | ATA_S_READY;
		p->is |= AHCI_P_IX_DHR;
	}
	p->ci &= ~(1 << slot);
	ahci_generate_intr(p->pr_sc);
}

static void
atapi_inquiry(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t buf[36];
	uint8_t *acmd;
	int len;

	acmd = cfis + 0x40;

	buf[0] = 0x05;
	buf[1] = 0x80;
	buf[2] = 0x00;
	buf[3] = 0x21;
	buf[4] = 31;
	buf[5] = 0;
	buf[6] = 0;
	buf[7] = 0;
	atapi_string(buf + 8, "BHYVE", 8);
	atapi_string(buf + 16, "BHYVE DVD-ROM", 16);
	atapi_string(buf + 32, "001", 4);

	len = sizeof(buf);
	if (len > acmd[4])
		len = acmd[4];
	cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
	write_prdt(p, slot, cfis, buf, len);
	ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
}

static void
atapi_read_capacity(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t buf[8];
	uint64_t sectors;

	sectors = blockif_size(p->bctx) / 2048;
	be32enc(buf, sectors - 1);
	be32enc(buf + 4, 2048);
	cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
	write_prdt(p, slot, cfis, buf, sizeof(buf));
	ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
}

static void
atapi_read_toc(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t *acmd;
	uint8_t format;
	int len;

	acmd = cfis + 0x40;

	len = be16dec(acmd + 7);
	format = acmd[9] >> 6;
	switch (format) {
	case 0:
	{
		int msf, size;
		uint64_t sectors;
		uint8_t start_track, buf[20], *bp;

		msf = (acmd[1] >> 1) & 1;
		start_track = acmd[6];
		if (start_track > 1 && start_track != 0xaa) {
			uint32_t tfd;
			p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
			p->asc = 0x24;
			tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
			cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
			ahci_write_fis_d2h(p, slot, cfis, tfd);
			return;
		}
		bp = buf + 2;
		*bp++ = 1;
		*bp++ = 1;
		if (start_track <= 1) {
			*bp++ = 0;
			*bp++ = 0x14;
			*bp++ = 1;
			*bp++ = 0;
			if (msf) {
				*bp++ = 0;
				lba_to_msf(bp, 0);
				bp += 3;
			} else {
				*bp++ = 0;
				*bp++ = 0;
				*bp++ = 0;
				*bp++ = 0;
			}
		}
		*bp++ = 0;
		*bp++ = 0x14;
		*bp++ = 0xaa;
		*bp++ = 0;
		sectors = blockif_size(p->bctx) / blockif_sectsz(p->bctx);
		sectors >>= 2;
		if (msf) {
			*bp++ = 0;
			lba_to_msf(bp, sectors);
			bp += 3;
		} else {
			be32enc(bp, sectors);
			bp += 4;
		}
		size = bp - buf;
		be16enc(buf, size - 2);
		if (len > size)
			len = size;
		write_prdt(p, slot, cfis, buf, len);
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
		break;
	}
	case 1:
	{
		uint8_t buf[12];

		memset(buf, 0, sizeof(buf));
		buf[1] = 0xa;
		buf[2] = 0x1;
		buf[3] = 0x1;
		if (len > sizeof(buf))
			len = sizeof(buf);
		write_prdt(p, slot, cfis, buf, len);
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
		break;
	}
	case 2:
	{
		int msf, size;
		uint64_t sectors;
		uint8_t start_track, *bp, buf[50];

		msf = (acmd[1] >> 1) & 1;
		start_track = acmd[6];
		bp = buf + 2;
		*bp++ = 1;
		*bp++ = 1;

		*bp++ = 1;
		*bp++ = 0x14;
		*bp++ = 0;
		*bp++ = 0xa0;
		*bp++ = 0;
		*bp++ = 0;
		*bp++ = 0;
		*bp++ = 0;
		*bp++ = 1;
		*bp++ = 0;
		*bp++ = 0;

		*bp++ = 1;
		*bp++ = 0x14;
		*bp++ = 0;
		*bp++ = 0xa1;
		*bp++ = 0;
		*bp++ = 0;
		*bp++ = 0;
		*bp++ = 0;
		*bp++ = 1;
		*bp++ = 0;
		*bp++ = 0;

		*bp++ = 1;
		*bp++ = 0x14;
		*bp++ = 0;
		*bp++ = 0xa2;
		*bp++ = 0;
		*bp++ = 0;
		*bp++ = 0;
		sectors = blockif_size(p->bctx) / blockif_sectsz(p->bctx);
		sectors >>= 2;
		if (msf) {
			*bp++ = 0;
			lba_to_msf(bp, sectors);
			bp += 3;
		} else {
			be32enc(bp, sectors);
			bp += 4;
		}

		*bp++ = 1;
		*bp++ = 0x14;
		*bp++ = 0;
		*bp++ = 1;
		*bp++ = 0;
		*bp++ = 0;
		*bp++ = 0;
		if (msf) {
			*bp++ = 0;
			lba_to_msf(bp, 0);
			bp += 3;
		} else {
			*bp++ = 0;
			*bp++ = 0;
			*bp++ = 0;
			*bp++ = 0;
		}

		size = bp - buf;
		be16enc(buf, size - 2);
		if (len > size)
			len = size;
		write_prdt(p, slot, cfis, buf, len);
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
		break;
	}
	default:
	{
		uint32_t tfd;

		p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
		p->asc = 0x24;
		tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		ahci_write_fis_d2h(p, slot, cfis, tfd);
		break;
	}
	}
}

static void
atapi_read(struct ahci_port *p, int slot, uint8_t *cfis,
		uint32_t done, int seek)
{
	struct ahci_ioreq *aior;
	struct ahci_cmd_hdr *hdr;
	struct ahci_prdt_entry *prdt;
	struct blockif_req *breq;
	struct pci_ahci_softc *sc;
	uint8_t *acmd;
	uint64_t lba;
	uint32_t len;
	int i, err, iovcnt;

	sc = p->pr_sc;
	acmd = cfis + 0x40;
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	prdt = (struct ahci_prdt_entry *)(cfis + 0x80);

	prdt += seek;
	lba = be32dec(acmd + 2);
	if (acmd[0] == READ_10)
		len = be16dec(acmd + 7);
	else
		len = be32dec(acmd + 6);
	if (len == 0) {
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
	}
	lba *= 2048;
	len *= 2048;

	/*
	 * Pull request off free list
	 */
	aior = STAILQ_FIRST(&p->iofhd);
	assert(aior != NULL);
	STAILQ_REMOVE_HEAD(&p->iofhd, io_list);
	aior->cfis = cfis;
	aior->slot = slot;
	aior->len = len;
	aior->done = done;
	breq = &aior->io_req;
	breq->br_offset = lba + done;
	iovcnt = hdr->prdtl - seek;
	if (iovcnt > BLOCKIF_IOV_MAX) {
		aior->prdtl = iovcnt - BLOCKIF_IOV_MAX;
		iovcnt = BLOCKIF_IOV_MAX;
	} else
		aior->prdtl = 0;
	breq->br_iovcnt = iovcnt;

	/*
	 * Build up the iovec based on the prdt
	 */
	for (i = 0; i < iovcnt; i++) {
		uint32_t dbcsz;

		dbcsz = (prdt->dbc & DBCMASK) + 1;
		breq->br_iov[i].iov_base = paddr_guest2host(ahci_ctx(sc),
		    prdt->dba, dbcsz);
		breq->br_iov[i].iov_len = dbcsz;
		aior->done += dbcsz;
		prdt++;
	}
	err = blockif_read(p->bctx, breq);
	assert(err == 0);
}

static void
atapi_request_sense(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t buf[64];
	uint8_t *acmd;
	int len;

	acmd = cfis + 0x40;
	len = acmd[4];
	if (len > sizeof(buf))
		len = sizeof(buf);
	memset(buf, 0, len);
	buf[0] = 0x70 | (1 << 7);
	buf[2] = p->sense_key;
	buf[7] = 10;
	buf[12] = p->asc;
	write_prdt(p, slot, cfis, buf, len);
	cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
	ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
}

static void
atapi_start_stop_unit(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t *acmd = cfis + 0x40;
	uint32_t tfd;

	switch (acmd[4] & 3) {
	case 0:
	case 1:
	case 3:
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		tfd = ATA_S_READY | ATA_S_DSC;
		break;
	case 2:
		/* TODO eject media */
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
		p->asc = 0x53;
		tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
		break;
	}
	ahci_write_fis_d2h(p, slot, cfis, tfd);
}

static void
atapi_mode_sense(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t *acmd;
	uint32_t tfd;
	uint8_t pc, code;
	int len;

	acmd = cfis + 0x40;
	len = be16dec(acmd + 7);
	pc = acmd[2] >> 6;
	code = acmd[2] & 0x3f;

	switch (pc) {
	case 0:
		switch (code) {
		case MODEPAGE_RW_ERROR_RECOVERY:
		{
			uint8_t buf[16];

			if (len > sizeof(buf))
				len = sizeof(buf);

			memset(buf, 0, sizeof(buf));
			be16enc(buf, 16 - 2);
			buf[2] = 0x70;
			buf[8] = 0x01;
			buf[9] = 16 - 10;
			buf[11] = 0x05;
			write_prdt(p, slot, cfis, buf, len);
			tfd = ATA_S_READY | ATA_S_DSC;
			break;
		}
		case MODEPAGE_CD_CAPABILITIES:
		{
			uint8_t buf[30];

			if (len > sizeof(buf))
				len = sizeof(buf);

			memset(buf, 0, sizeof(buf));
			be16enc(buf, 30 - 2);
			buf[2] = 0x70;
			buf[8] = 0x2A;
			buf[9] = 30 - 10;
			buf[10] = 0x08;
			buf[12] = 0x71;
			be16enc(&buf[18], 2);
			be16enc(&buf[20], 512);
			write_prdt(p, slot, cfis, buf, len);
			tfd = ATA_S_READY | ATA_S_DSC;
			break;
		}
		default:
			goto error;
			break;
		}
		break;
	case 3:
		p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
		p->asc = 0x39;
		tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
		break;
error:
	case 1:
	case 2:
		p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
		p->asc = 0x24;
		tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
		break;
	}
	cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
	ahci_write_fis_d2h(p, slot, cfis, tfd);
}

static void
atapi_get_event_status_notification(struct ahci_port *p, int slot,
    uint8_t *cfis)
{
	uint8_t *acmd;
	uint32_t tfd;

	acmd = cfis + 0x40;

	/* we don't support asynchronous operation */
	if (!(acmd[1] & 1)) {
		p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
		p->asc = 0x24;
		tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
	} else {
		uint8_t buf[8];
		int len;

		len = be16dec(acmd + 7);
		if (len > sizeof(buf))
			len = sizeof(buf);

		memset(buf, 0, sizeof(buf));
		be16enc(buf, 8 - 2);
		buf[2] = 0x04;
		buf[3] = 0x10;
		buf[5] = 0x02;
		write_prdt(p, slot, cfis, buf, len);
		tfd = ATA_S_READY | ATA_S_DSC;
	}
	cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
	ahci_write_fis_d2h(p, slot, cfis, tfd);
}

static void
handle_packet_cmd(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t *acmd;

	acmd = cfis + 0x40;

#ifdef AHCI_DEBUG
	{
		int i;
		DPRINTF("ACMD:");
		for (i = 0; i < 16; i++)
			DPRINTF("%02x ", acmd[i]);
		DPRINTF("\n");
	}
#endif

	switch (acmd[0]) {
	case TEST_UNIT_READY:
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
		break;
	case INQUIRY:
		atapi_inquiry(p, slot, cfis);
		break;
	case READ_CAPACITY:
		atapi_read_capacity(p, slot, cfis);
		break;
	case PREVENT_ALLOW:
		/* TODO */
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
		break;
	case READ_TOC:
		atapi_read_toc(p, slot, cfis);
		break;
	case READ_10:
	case READ_12:
		atapi_read(p, slot, cfis, 0, 0);
		break;
	case REQUEST_SENSE:
		atapi_request_sense(p, slot, cfis);
		break;
	case START_STOP_UNIT:
		atapi_start_stop_unit(p, slot, cfis);
		break;
	case MODE_SENSE_10:
		atapi_mode_sense(p, slot, cfis);
		break;
	case GET_EVENT_STATUS_NOTIFICATION:
		atapi_get_event_status_notification(p, slot, cfis);
		break;
	default:
		cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
		p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
		p->asc = 0x20;
		ahci_write_fis_d2h(p, slot, cfis, (p->sense_key << 12) |
				ATA_S_READY | ATA_S_ERROR);
		break;
	}
}

static void
ahci_handle_cmd(struct ahci_port *p, int slot, uint8_t *cfis)
{

	switch (cfis[2]) {
	case ATA_ATA_IDENTIFY:
		handle_identify(p, slot, cfis);
		break;
	case ATA_SETFEATURES:
	{
		switch (cfis[3]) {
		case ATA_SF_ENAB_SATA_SF:
			switch (cfis[12]) {
			case ATA_SATA_SF_AN:
				p->tfd = ATA_S_DSC | ATA_S_READY;
				break;
			default:
				p->tfd = ATA_S_ERROR | ATA_S_READY;
				p->tfd |= (ATA_ERROR_ABORT << 8);
				break;
			}
			break;
		case ATA_SF_ENAB_WCACHE:
		case ATA_SF_DIS_WCACHE:
		case ATA_SF_ENAB_RCACHE:
		case ATA_SF_DIS_RCACHE:
			p->tfd = ATA_S_DSC | ATA_S_READY;
			break;
		case ATA_SF_SETXFER:
		{
			switch (cfis[12] & 0xf8) {
			case ATA_PIO:
			case ATA_PIO0:
				break;
			case ATA_WDMA0:
			case ATA_UDMA0:
				p->xfermode = (cfis[12] & 0x7);
				break;
			}
			p->tfd = ATA_S_DSC | ATA_S_READY;
			break;
		}
		default:
			p->tfd = ATA_S_ERROR | ATA_S_READY;
			p->tfd |= (ATA_ERROR_ABORT << 8);
			break;
		}
		ahci_write_fis_d2h(p, slot, cfis, p->tfd);
		break;
	}
	case ATA_SET_MULTI:
		if (cfis[12] != 0 &&
			(cfis[12] > 128 || (cfis[12] & (cfis[12] - 1)))) {
			p->tfd = ATA_S_ERROR | ATA_S_READY;
			p->tfd |= (ATA_ERROR_ABORT << 8);
		} else {
			p->mult_sectors = cfis[12];
			p->tfd = ATA_S_DSC | ATA_S_READY;
		}
		p->is |= AHCI_P_IX_DP;
		p->ci &= ~(1 << slot);
		ahci_generate_intr(p->pr_sc);
		break;
	case ATA_READ_DMA:
	case ATA_WRITE_DMA:
	case ATA_READ_DMA48:
	case ATA_WRITE_DMA48:
	case ATA_READ_FPDMA_QUEUED:
	case ATA_WRITE_FPDMA_QUEUED:
		ahci_handle_dma(p, slot, cfis, 0, 0);
		break;
	case ATA_FLUSHCACHE:
	case ATA_FLUSHCACHE48:
		ahci_handle_flush(p, slot, cfis);
		break;
	case ATA_STANDBY_CMD:
		break;
	case ATA_NOP:
	case ATA_STANDBY_IMMEDIATE:
	case ATA_IDLE_IMMEDIATE:
	case ATA_SLEEP:
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
		break;
	case ATA_ATAPI_IDENTIFY:
		handle_atapi_identify(p, slot, cfis);
		break;
	case ATA_PACKET_CMD:
		if (!p->atapi) {
			p->tfd = (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR;
			p->is |= AHCI_P_IX_TFE;
			p->ci &= ~(1 << slot);
			ahci_generate_intr(p->pr_sc);
		} else
			handle_packet_cmd(p, slot, cfis);
		break;
	default:
		WPRINTF("Unsupported cmd:%02x\n", cfis[2]);
		p->tfd = (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR;
		p->is |= AHCI_P_IX_TFE;
		p->ci &= ~(1 << slot);
		ahci_generate_intr(p->pr_sc);
		break;
	}
}

static void
ahci_handle_slot(struct ahci_port *p, int slot)
{
	struct ahci_cmd_hdr *hdr;
	struct ahci_prdt_entry *prdt;
	struct pci_ahci_softc *sc;
	uint8_t *cfis;
	int cfl;

	sc = p->pr_sc;
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	cfl = (hdr->flags & 0x1f) * 4;
	cfis = paddr_guest2host(ahci_ctx(sc), hdr->ctba,
			0x80 + hdr->prdtl * sizeof(struct ahci_prdt_entry));
	prdt = (struct ahci_prdt_entry *)(cfis + 0x80);

#ifdef AHCI_DEBUG
	DPRINTF("\ncfis:");
	for (i = 0; i < cfl; i++) {
		if (i % 10 == 0)
			DPRINTF("\n");
		DPRINTF("%02x ", cfis[i]);
	}
	DPRINTF("\n");

	for (i = 0; i < hdr->prdtl; i++) {
		DPRINTF("%d@%08"PRIx64"\n", prdt->dbc & 0x3fffff, prdt->dba);
		prdt++;
	}
#endif

	if (cfis[0] != FIS_TYPE_REGH2D) {
		WPRINTF("Not a H2D FIS:%02x\n", cfis[0]);
		return;
	}

	if (cfis[1] & 0x80) {
		ahci_handle_cmd(p, slot, cfis);
	} else {
		if (cfis[15] & (1 << 2))
			p->reset = 1;
		else if (p->reset) {
			p->reset = 0;
			ahci_port_reset(p);
		}
		p->ci &= ~(1 << slot);
	}
}

static void
ahci_handle_port(struct ahci_port *p)
{
	int i;

	if (!(p->cmd & AHCI_P_CMD_ST))
		return;

	/*
	 * Search for any new commands to issue ignoring those that
	 * are already in-flight.
	 */
	for (i = 0; (i < 32) && p->ci; i++) {
		if ((p->ci & (1 << i)) && !(p->pending & (1 << i)))
			ahci_handle_slot(p, i);
	}
}

/*
 * blockif callback routine - this runs in the context of the blockif
 * i/o thread, so the mutex needs to be acquired.
 */
static void
ata_ioreq_cb(struct blockif_req *br, int err)
{
	struct ahci_cmd_hdr *hdr;
	struct ahci_ioreq *aior;
	struct ahci_port *p;
	struct pci_ahci_softc *sc;
	uint32_t tfd;
	uint8_t *cfis;
	int pending, slot, ncq;

	DPRINTF("%s %d\n", __func__, err);

	ncq = 0;
	aior = br->br_param;
	p = aior->io_pr;
	cfis = aior->cfis;
	slot = aior->slot;
	pending = aior->prdtl;
	sc = p->pr_sc;
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);

	if (cfis[2] == ATA_WRITE_FPDMA_QUEUED ||
			cfis[2] == ATA_READ_FPDMA_QUEUED)
		ncq = 1;

	pthread_mutex_lock(&sc->mtx);

	/*
	 * Move the blockif request back to the free list
	 */
	STAILQ_INSERT_TAIL(&p->iofhd, aior, io_list);

	if (pending && !err) {
		ahci_handle_dma(p, slot, cfis, aior->done,
		    hdr->prdtl - pending);
		goto out;
	}

	if (!err && aior->done == aior->len) {
		tfd = ATA_S_READY | ATA_S_DSC;
		if (ncq)
			hdr->prdbc = 0;
		else
			hdr->prdbc = aior->len;
	} else {
		tfd = (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR;
		hdr->prdbc = 0;
		if (ncq)
			p->serr |= (1 << slot);
	}

	/*
	 * This command is now complete.
	 */
	p->pending &= ~(1 << slot);

	if (ncq) {
		p->sact &= ~(1 << slot);
		ahci_write_fis_sdb(p, slot, tfd);
	} else
		ahci_write_fis_d2h(p, slot, cfis, tfd);

out:
	pthread_mutex_unlock(&sc->mtx);
	DPRINTF("%s exit\n", __func__);
}

static void
atapi_ioreq_cb(struct blockif_req *br, int err)
{
	struct ahci_cmd_hdr *hdr;
	struct ahci_ioreq *aior;
	struct ahci_port *p;
	struct pci_ahci_softc *sc;
	uint8_t *cfis;
	uint32_t tfd;
	int pending, slot;

	DPRINTF("%s %d\n", __func__, err);

	aior = br->br_param;
	p = aior->io_pr;
	cfis = aior->cfis;
	slot = aior->slot;
	pending = aior->prdtl;
	sc = p->pr_sc;
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + aior->slot * AHCI_CL_SIZE);

	pthread_mutex_lock(&sc->mtx);

	/*
	 * Move the blockif request back to the free list
	 */
	STAILQ_INSERT_TAIL(&p->iofhd, aior, io_list);

	if (pending && !err) {
		atapi_read(p, slot, cfis, aior->done, hdr->prdtl - pending);
		goto out;
	}

	if (!err && aior->done == aior->len) {
		tfd = ATA_S_READY | ATA_S_DSC;
		hdr->prdbc = aior->len;
	} else {
		p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
		p->asc = 0x21;
		tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
		hdr->prdbc = 0;
	}

	cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
	ahci_write_fis_d2h(p, slot, cfis, tfd);

out:
	pthread_mutex_unlock(&sc->mtx);
	DPRINTF("%s exit\n", __func__);
}

static void
pci_ahci_ioreq_init(struct ahci_port *pr)
{
	struct ahci_ioreq *vr;
	int i;

	pr->ioqsz = blockif_queuesz(pr->bctx);
	pr->ioreq = calloc(pr->ioqsz, sizeof(struct ahci_ioreq));
	STAILQ_INIT(&pr->iofhd);

	/*
	 * Add all i/o request entries to the free queue
	 */
	for (i = 0; i < pr->ioqsz; i++) {
		vr = &pr->ioreq[i];
		vr->io_pr = pr;
		if (!pr->atapi)
			vr->io_req.br_callback = ata_ioreq_cb;
		else
			vr->io_req.br_callback = atapi_ioreq_cb;
		vr->io_req.br_param = vr;
		STAILQ_INSERT_TAIL(&pr->iofhd, vr, io_list);
	}
}

static void
pci_ahci_port_write(struct pci_ahci_softc *sc, uint64_t offset, uint64_t value)
{
	int port = (offset - AHCI_OFFSET) / AHCI_STEP;
	offset = (offset - AHCI_OFFSET) % AHCI_STEP;
	struct ahci_port *p = &sc->port[port];

	DPRINTF("pci_ahci_port %d: write offset 0x%"PRIx64" value 0x%"PRIx64"\n",
		port, offset, value);

	switch (offset) {
	case AHCI_P_CLB:
		p->clb = value;
		break;
	case AHCI_P_CLBU:
		p->clbu = value;
		break;
	case AHCI_P_FB:
		p->fb = value;
		break;
	case AHCI_P_FBU:
		p->fbu = value;
		break;
	case AHCI_P_IS:
		p->is &= ~value;
		break;
	case AHCI_P_IE:
		p->ie = value & 0xFDC000FF;
		ahci_generate_intr(sc);
		break;
	case AHCI_P_CMD:
	{
		p->cmd = value;
		
		if (!(value & AHCI_P_CMD_ST)) {
			p->cmd &= ~(AHCI_P_CMD_CR | AHCI_P_CMD_CCS_MASK);
			p->ci = 0;
			p->sact = 0;
		} else {
			uint64_t clb;

			p->cmd |= AHCI_P_CMD_CR;
			clb = (uint64_t)p->clbu << 32 | p->clb;
			p->cmd_lst = paddr_guest2host(ahci_ctx(sc), clb,
					AHCI_CL_SIZE * AHCI_MAX_SLOTS);
		}

		if (value & AHCI_P_CMD_FRE) {
			uint64_t fb;

			p->cmd |= AHCI_P_CMD_FR;
			fb = (uint64_t)p->fbu << 32 | p->fb;
			/* we don't support FBSCP, so rfis size is 256Bytes */
			p->rfis = paddr_guest2host(ahci_ctx(sc), fb, 256);
		} else {
			p->cmd &= ~AHCI_P_CMD_FR;
		}

		if (value & AHCI_P_CMD_CLO) {
			p->tfd = 0;
			p->cmd &= ~AHCI_P_CMD_CLO;
		}

		ahci_handle_port(p);
		break;
	}
	case AHCI_P_TFD:
	case AHCI_P_SIG:
	case AHCI_P_SSTS:
		WPRINTF("pci_ahci_port: read only registers 0x%"PRIx64"\n", offset);
		break;
	case AHCI_P_SCTL:
		if (!(p->cmd & AHCI_P_CMD_ST)) {
			if (value & ATA_SC_DET_RESET)
				ahci_port_reset(p);
			p->sctl = value;
		}
		break;
	case AHCI_P_SERR:
		p->serr &= ~value;
		break;
	case AHCI_P_SACT:
		p->sact |= value;
		break;
	case AHCI_P_CI:
		p->ci |= value;
		ahci_handle_port(p);
		break;
	case AHCI_P_SNTF:
	case AHCI_P_FBS:
	default:
		break;
	}
}

static void
pci_ahci_host_write(struct pci_ahci_softc *sc, uint64_t offset, uint64_t value)
{
	DPRINTF("pci_ahci_host: write offset 0x%"PRIx64" value 0x%"PRIx64"\n",
		offset, value);

	switch (offset) {
	case AHCI_CAP:
	case AHCI_PI:
	case AHCI_VS:
	case AHCI_CAP2:
		DPRINTF("pci_ahci_host: read only registers 0x%"PRIx64"\n", offset);
		break;
	case AHCI_GHC:
		if (value & AHCI_GHC_HR)
			ahci_reset(sc);
		else if (value & AHCI_GHC_IE) {
			sc->ghc |= AHCI_GHC_IE;
			ahci_generate_intr(sc);
		}
		break;
	case AHCI_IS:
		sc->is &= ~value;
		ahci_generate_intr(sc);
		break;
	default:
		break;
	}
}

static void
pci_ahci_write(struct vmctx *ctx, int vcpu, struct pci_devinst *pi,
		int baridx, uint64_t offset, int size, uint64_t value)
{
	struct pci_ahci_softc *sc = pi->pi_arg;

	assert(baridx == 5);
	assert(size == 4);

	pthread_mutex_lock(&sc->mtx);

	if (offset < AHCI_OFFSET)
		pci_ahci_host_write(sc, offset, value);
	else if (offset < AHCI_OFFSET + sc->ports * AHCI_STEP)
		pci_ahci_port_write(sc, offset, value);
	else
		WPRINTF("pci_ahci: unknown i/o write offset 0x%"PRIx64"\n", offset);

	pthread_mutex_unlock(&sc->mtx);
}

static uint64_t
pci_ahci_host_read(struct pci_ahci_softc *sc, uint64_t offset)
{
	uint32_t value;

	switch (offset) {
	case AHCI_CAP:
	case AHCI_GHC:
	case AHCI_IS:
	case AHCI_PI:
	case AHCI_VS:
	case AHCI_CCCC:
	case AHCI_CCCP:
	case AHCI_EM_LOC:
	case AHCI_EM_CTL:
	case AHCI_CAP2:
	{
		uint32_t *p = &sc->cap;
		p += (offset - AHCI_CAP) / sizeof(uint32_t);
		value = *p;
		break;
	}
	default:
		value = 0;
		break;
	}
	DPRINTF("pci_ahci_host: read offset 0x%"PRIx64" value 0x%x\n",
		offset, value);

	return (value);
}

static uint64_t
pci_ahci_port_read(struct pci_ahci_softc *sc, uint64_t offset)
{
	uint32_t value;
	int port = (offset - AHCI_OFFSET) / AHCI_STEP;
	offset = (offset - AHCI_OFFSET) % AHCI_STEP;

	switch (offset) {
	case AHCI_P_CLB:
	case AHCI_P_CLBU:
	case AHCI_P_FB:
	case AHCI_P_FBU:
	case AHCI_P_IS:
	case AHCI_P_IE:
	case AHCI_P_CMD:
	case AHCI_P_TFD:
	case AHCI_P_SIG:
	case AHCI_P_SSTS:
	case AHCI_P_SCTL:
	case AHCI_P_SERR:
	case AHCI_P_SACT:
	case AHCI_P_CI:
	case AHCI_P_SNTF:
	case AHCI_P_FBS:
	{
		uint32_t *p= &sc->port[port].clb;
		p += (offset - AHCI_P_CLB) / sizeof(uint32_t);
		value = *p;
		break;
	}
	default:
		value = 0;
		break;
	}

	DPRINTF("pci_ahci_port %d: read offset 0x%"PRIx64" value 0x%x\n",
		port, offset, value);

	return value;
}

static uint64_t
pci_ahci_read(struct vmctx *ctx, int vcpu, struct pci_devinst *pi, int baridx,
    uint64_t offset, int size)
{
	struct pci_ahci_softc *sc = pi->pi_arg;
	uint32_t value;

	assert(baridx == 5);
	assert(size == 4);

	pthread_mutex_lock(&sc->mtx);

	if (offset < AHCI_OFFSET)
		value = pci_ahci_host_read(sc, offset);
	else if (offset < AHCI_OFFSET + sc->ports * AHCI_STEP)
		value = pci_ahci_port_read(sc, offset);
	else {
		value = 0;
		WPRINTF("pci_ahci: unknown i/o read offset 0x%"PRIx64"\n", offset);
	}

	pthread_mutex_unlock(&sc->mtx);

	return (value);
}

static int
pci_ahci_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts, int atapi)
{
	char bident[sizeof("XX:X:X")];
	struct blockif_ctxt *bctxt;
	struct pci_ahci_softc *sc;
	int ret, slots;

	ret = 0;

	if (opts == NULL) {
		fprintf(stderr, "pci_ahci: backing device required\n");
		return (1);
	}

#ifdef AHCI_DEBUG
	dbg = fopen("/tmp/log", "w+");
#endif

	sc = calloc(1, sizeof(struct pci_ahci_softc));
	pi->pi_arg = sc;
	sc->asc_pi = pi;
	sc->ports = MAX_PORTS;

	/*
	 * Only use port 0 for a backing device. All other ports will be
	 * marked as unused
	 */
	sc->port[0].atapi = atapi;

	/*
	 * Attempt to open the backing image. Use the PCI
	 * slot/func for the identifier string.
	 */
	snprintf(bident, sizeof(bident), "%d:%d", pi->pi_slot, pi->pi_func);
	bctxt = blockif_open(opts, bident);
	if (bctxt == NULL) {       	
		ret = 1;
		goto open_fail;
	}	
	sc->port[0].bctx = bctxt;
	sc->port[0].pr_sc = sc;

	/*
	 * Allocate blockif request structures and add them
	 * to the free list
	 */
	pci_ahci_ioreq_init(&sc->port[0]);

	pthread_mutex_init(&sc->mtx, NULL);

	/* Intel ICH8 AHCI */
	slots = sc->port[0].ioqsz;
	if (slots > 32)
		slots = 32;
	--slots;
	sc->cap = AHCI_CAP_64BIT | AHCI_CAP_SNCQ | AHCI_CAP_SSNTF |
	    AHCI_CAP_SMPS | AHCI_CAP_SSS | AHCI_CAP_SALP |
	    AHCI_CAP_SAL | AHCI_CAP_SCLO | (0x3 << AHCI_CAP_ISS_SHIFT)|
	    AHCI_CAP_PMD | AHCI_CAP_SSC | AHCI_CAP_PSC |
	    (slots << AHCI_CAP_NCS_SHIFT) | AHCI_CAP_SXS | (sc->ports - 1);

	/* Only port 0 implemented */
	sc->pi = 1;
	sc->vs = 0x10300;
	sc->cap2 = AHCI_CAP2_APST;
	ahci_reset(sc);

	pci_set_cfgdata16(pi, PCIR_DEVICE, 0x2821);
	pci_set_cfgdata16(pi, PCIR_VENDOR, 0x8086);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata8(pi, PCIR_SUBCLASS, PCIS_STORAGE_SATA);
	pci_set_cfgdata8(pi, PCIR_PROGIF, PCIP_STORAGE_SATA_AHCI_1_0);
	pci_emul_add_msicap(pi, 1);
	pci_emul_alloc_bar(pi, 5, PCIBAR_MEM32,
	    AHCI_OFFSET + sc->ports * AHCI_STEP);

	pci_lintr_request(pi);

open_fail:
	if (ret) {
		blockif_close(sc->port[0].bctx);
		free(sc);
	}

	return (ret);
}

static int
pci_ahci_hd_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{

	return (pci_ahci_init(ctx, pi, opts, 0));
}

static int
pci_ahci_atapi_init(struct vmctx *ctx, struct pci_devinst *pi, char *opts)
{

	return (pci_ahci_init(ctx, pi, opts, 1));
}

/*
 * Use separate emulation names to distinguish drive and atapi devices
 */
struct pci_devemu pci_de_ahci_hd = {
	.pe_emu =	"ahci-hd",
	.pe_init =	pci_ahci_hd_init,
	.pe_barwrite =	pci_ahci_write,
	.pe_barread =	pci_ahci_read
};
PCI_EMUL_SET(pci_de_ahci_hd);

struct pci_devemu pci_de_ahci_cd = {
	.pe_emu =	"ahci-cd",
	.pe_init =	pci_ahci_atapi_init,
	.pe_barwrite =	pci_ahci_write,
	.pe_barread =	pci_ahci_read
};
PCI_EMUL_SET(pci_de_ahci_cd);
