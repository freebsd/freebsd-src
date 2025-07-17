/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2013  Zhixiang Yu <zcore@freebsd.org>
 * Copyright (c) 2015-2016 Alexander Motin <mav@FreeBSD.org>
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
 */

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
#include <pthread_np.h>
#include <inttypes.h>
#include <md5.h>

#include "bhyverun.h"
#include "config.h"
#include "debug.h"
#include "pci_emul.h"
#ifdef BHYVE_SNAPSHOT
#include "snapshot.h"
#endif
#include "ahci.h"
#include "block_if.h"

#define	DEF_PORTS	6	/* Intel ICH8 AHCI supports 6 ports */
#define	MAX_PORTS	32	/* AHCI supports 32 ports */

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
#define	REPORT_LUNS		0xA0
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
#define	ATA_SATA_SF_AN			0x05
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

#define AHCI_PORT_IDENT 20 + 1

struct ahci_ioreq {
	struct blockif_req io_req;
	struct ahci_port *io_pr;
	STAILQ_ENTRY(ahci_ioreq) io_flist;
	TAILQ_ENTRY(ahci_ioreq) io_blist;
	uint8_t *cfis;
	uint8_t *dsm;
	uint32_t len;
	uint32_t done;
	int slot;
	int more;
	int readop;
};

struct ahci_port {
	struct blockif_ctxt *bctx;
	struct pci_ahci_softc *pr_sc;
	struct ata_params ata_ident;
	uint8_t *cmd_lst;
	uint8_t *rfis;
	int port;
	int atapi;
	int reset;
	int waitforclear;
	int mult_sectors;
	uint8_t xfermode;
	uint8_t err_cfis[20];
	uint8_t sense_key;
	uint8_t asc;
	u_int ccs;
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
	TAILQ_HEAD(ahci_bhead, ahci_ioreq) iobhd;
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

static void ahci_handle_next_trim(struct ahci_port *p, int slot, uint8_t *cfis,
    uint8_t *buf, uint32_t len, uint32_t done);
static void ahci_handle_port(struct ahci_port *p);

static inline void lba_to_msf(uint8_t *buf, int lba)
{
	lba += 150;
	buf[0] = (lba / 75) / 60;
	buf[1] = (lba / 75) % 60;
	buf[2] = lba % 75;
}

/*
 * Generate HBA interrupts on global IS register write.
 */
static void
ahci_generate_intr(struct pci_ahci_softc *sc, uint32_t mask)
{
	struct pci_devinst *pi = sc->asc_pi;
	struct ahci_port *p;
	int i, nmsg;
	uint32_t mmask;

	/* Update global IS from PxIS/PxIE. */
	for (i = 0; i < sc->ports; i++) {
		p = &sc->port[i];
		if (p->is & p->ie)
			sc->is |= (1 << i);
	}
	DPRINTF("%s(%08x) %08x", __func__, mask, sc->is);

	/* If there is nothing enabled -- clear legacy interrupt and exit. */
	if (sc->is == 0 || (sc->ghc & AHCI_GHC_IE) == 0) {
		if (sc->lintr) {
			pci_lintr_deassert(pi);
			sc->lintr = 0;
		}
		return;
	}

	/* If there is anything and no MSI -- assert legacy interrupt. */
	nmsg = pci_msi_maxmsgnum(pi);
	if (nmsg == 0) {
		if (!sc->lintr) {
			sc->lintr = 1;
			pci_lintr_assert(pi);
		}
		return;
	}

	/* Assert respective MSIs for ports that were touched. */
	for (i = 0; i < nmsg; i++) {
		if (sc->ports <= nmsg || i < nmsg - 1)
			mmask = 1 << i;
		else
			mmask = 0xffffffff << i;
		if (sc->is & mask && mmask & mask)
			pci_generate_msi(pi, i);
	}
}

/*
 * Generate HBA interrupt on specific port event.
 */
static void
ahci_port_intr(struct ahci_port *p)
{
	struct pci_ahci_softc *sc = p->pr_sc;
	struct pci_devinst *pi = sc->asc_pi;
	int nmsg;

	DPRINTF("%s(%d) %08x/%08x %08x", __func__,
	    p->port, p->is, p->ie, sc->is);

	/* If there is nothing enabled -- we are done. */
	if ((p->is & p->ie) == 0)
		return;

	/* In case of non-shared MSI always generate interrupt. */
	nmsg = pci_msi_maxmsgnum(pi);
	if (sc->ports <= nmsg || p->port < nmsg - 1) {
		sc->is |= (1 << p->port);
		if ((sc->ghc & AHCI_GHC_IE) == 0)
			return;
		pci_generate_msi(pi, p->port);
		return;
	}

	/* If IS for this port is already set -- do nothing. */
	if (sc->is & (1 << p->port))
		return;

	sc->is |= (1 << p->port);

	/* If interrupts are enabled -- generate one. */
	if ((sc->ghc & AHCI_GHC_IE) == 0)
		return;
	if (nmsg > 0) {
		pci_generate_msi(pi, nmsg - 1);
	} else if (!sc->lintr) {
		sc->lintr = 1;
		pci_lintr_assert(pi);
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
		irq = (fis[1] & (1 << 6)) ? AHCI_P_IX_DHR : 0;
		break;
	case FIS_TYPE_SETDEVBITS:
		offset = 0x58;
		len = 8;
		irq = (fis[1] & (1 << 6)) ? AHCI_P_IX_SDB : 0;
		break;
	case FIS_TYPE_PIOSETUP:
		offset = 0x20;
		len = 20;
		irq = (fis[1] & (1 << 6)) ? AHCI_P_IX_PS : 0;
		break;
	default:
		EPRINTLN("unsupported fis type %d", ft);
		return;
	}
	if (fis[2] & ATA_S_ERROR) {
		p->waitforclear = 1;
		irq |= AHCI_P_IX_TFE;
	}
	memcpy(p->rfis + offset, fis, len);
	if (irq) {
		if (~p->is & irq) {
			p->is |= irq;
			ahci_port_intr(p);
		}
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
ahci_write_fis_sdb(struct ahci_port *p, int slot, uint8_t *cfis, uint32_t tfd)
{
	uint8_t fis[8];
	uint8_t error;

	error = (tfd >> 8) & 0xff;
	tfd &= 0x77;
	memset(fis, 0, sizeof(fis));
	fis[0] = FIS_TYPE_SETDEVBITS;
	fis[1] = (1 << 6);
	fis[2] = tfd;
	fis[3] = error;
	if (fis[2] & ATA_S_ERROR) {
		p->err_cfis[0] = slot;
		p->err_cfis[2] = tfd;
		p->err_cfis[3] = error;
		memcpy(&p->err_cfis[4], cfis + 4, 16);
	} else {
		*(uint32_t *)(fis + 4) = (1 << slot);
		p->sact &= ~(1 << slot);
	}
	p->tfd &= ~0x77;
	p->tfd |= tfd;
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
	if (fis[2] & ATA_S_ERROR) {
		p->err_cfis[0] = 0x80;
		p->err_cfis[2] = tfd & 0xff;
		p->err_cfis[3] = error;
		memcpy(&p->err_cfis[4], cfis + 4, 16);
	} else
		p->ci &= ~(1 << slot);
	p->tfd = tfd;
	ahci_write_fis(p, FIS_TYPE_REGD2H, fis);
}

static void
ahci_write_fis_d2h_ncq(struct ahci_port *p, int slot)
{
	uint8_t fis[20];

	p->tfd = ATA_S_READY | ATA_S_DSC;
	memset(fis, 0, sizeof(fis));
	fis[0] = FIS_TYPE_REGD2H;
	fis[1] = 0;			/* No interrupt */
	fis[2] = p->tfd;		/* Status */
	fis[3] = 0;			/* No error */
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
ahci_check_stopped(struct ahci_port *p)
{
	/*
	 * If we are no longer processing the command list and nothing
	 * is in-flight, clear the running bit, the current command
	 * slot, the command issue and active bits.
	 */
	if (!(p->cmd & AHCI_P_CMD_ST)) {
		if (p->pending == 0) {
			p->ccs = 0;
			p->cmd &= ~(AHCI_P_CMD_CR | AHCI_P_CMD_CCS_MASK);
			p->ci = 0;
			p->sact = 0;
			p->waitforclear = 0;
		}
	}
}

static void
ahci_port_stop(struct ahci_port *p)
{
	struct ahci_ioreq *aior;
	uint8_t *cfis;
	int slot;
	int error;

	assert(pthread_mutex_isowned_np(&p->pr_sc->mtx));

	TAILQ_FOREACH(aior, &p->iobhd, io_blist) {
		/*
		 * Try to cancel the outstanding blockif request.
		 */
		error = blockif_cancel(p->bctx, &aior->io_req);
		if (error != 0)
			continue;

		slot = aior->slot;
		cfis = aior->cfis;
		if (cfis[2] == ATA_WRITE_FPDMA_QUEUED ||
		    cfis[2] == ATA_READ_FPDMA_QUEUED ||
		    cfis[2] == ATA_SEND_FPDMA_QUEUED)
			p->sact &= ~(1 << slot);	/* NCQ */
		else
			p->ci &= ~(1 << slot);

		/*
		 * This command is now done.
		 */
		p->pending &= ~(1 << slot);

		/*
		 * Delete the blockif request from the busy list
		 */
		TAILQ_REMOVE(&p->iobhd, aior, io_blist);

		/*
		 * Move the blockif request back to the free list
		 */
		STAILQ_INSERT_TAIL(&p->iofhd, aior, io_flist);
	}

	ahci_check_stopped(p);
}

static void
ahci_port_reset(struct ahci_port *pr)
{
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
	pr->ssts = ATA_SS_DET_PHY_ONLINE | ATA_SS_IPM_ACTIVE;
	if (pr->sctl & ATA_SC_SPD_MASK)
		pr->ssts |= (pr->sctl & ATA_SC_SPD_MASK);
	else
		pr->ssts |= ATA_SS_SPD_GEN3;
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
		sc->port[i].cmd = (AHCI_P_CMD_SUD | AHCI_P_CMD_POD);
		if (sc->port[i].bctx)
			sc->port[i].cmd |= AHCI_P_CMD_CPS;
		sc->port[i].sctl = 0;
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

/*
 * Build up the iovec based on the PRDT, 'done' and 'len'.
 */
static void
ahci_build_iov(struct ahci_port *p, struct ahci_ioreq *aior,
    struct ahci_prdt_entry *prdt, uint16_t prdtl)
{
	struct blockif_req *breq = &aior->io_req;
	uint32_t dbcsz, extra, left, skip, todo;
	int i, j;

	assert(aior->len >= aior->done);

	/* Copy part of PRDT between 'done' and 'len' bytes into the iov. */
	skip = aior->done;
	left = aior->len - aior->done;
	todo = 0;
	for (i = 0, j = 0; i < prdtl && j < BLOCKIF_IOV_MAX && left > 0;
	    i++, prdt++) {
		dbcsz = (prdt->dbc & DBCMASK) + 1;
		/* Skip already done part of the PRDT */
		if (dbcsz <= skip) {
			skip -= dbcsz;
			continue;
		}
		dbcsz -= skip;
		if (dbcsz > left)
			dbcsz = left;
		breq->br_iov[j].iov_base = paddr_guest2host(ahci_ctx(p->pr_sc),
		    prdt->dba + skip, dbcsz);
		breq->br_iov[j].iov_len = dbcsz;
		todo += dbcsz;
		left -= dbcsz;
		skip = 0;
		j++;
	}

	/* If we got limited by IOV length, round I/O down to sector size. */
	if (j == BLOCKIF_IOV_MAX) {
		extra = todo % blockif_sectsz(p->bctx);
		todo -= extra;
		assert(todo > 0);
		while (extra > 0) {
			if (breq->br_iov[j - 1].iov_len > extra) {
				breq->br_iov[j - 1].iov_len -= extra;
				break;
			}
			extra -= breq->br_iov[j - 1].iov_len;
			j--;
		}
	}

	breq->br_iovcnt = j;
	breq->br_resid = todo;
	aior->done += todo;
	aior->more = (aior->done < aior->len && i < prdtl);
}

static void
ahci_handle_rw(struct ahci_port *p, int slot, uint8_t *cfis, uint32_t done)
{
	struct ahci_ioreq *aior;
	struct blockif_req *breq;
	struct ahci_prdt_entry *prdt;
	struct ahci_cmd_hdr *hdr;
	uint64_t lba;
	uint32_t len;
	int err, first, ncq, readop;

	prdt = (struct ahci_prdt_entry *)(cfis + 0x80);
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	ncq = 0;
	readop = 1;
	first = (done == 0);

	if (cfis[2] == ATA_WRITE || cfis[2] == ATA_WRITE48 ||
	    cfis[2] == ATA_WRITE_MUL || cfis[2] == ATA_WRITE_MUL48 ||
	    cfis[2] == ATA_WRITE_DMA || cfis[2] == ATA_WRITE_DMA48 ||
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
	} else if (cfis[2] == ATA_READ48 || cfis[2] == ATA_WRITE48 ||
	    cfis[2] == ATA_READ_MUL48 || cfis[2] == ATA_WRITE_MUL48 ||
	    cfis[2] == ATA_READ_DMA48 || cfis[2] == ATA_WRITE_DMA48) {
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

	/* Pull request off free list */
	aior = STAILQ_FIRST(&p->iofhd);
	assert(aior != NULL);
	STAILQ_REMOVE_HEAD(&p->iofhd, io_flist);

	aior->cfis = cfis;
	aior->slot = slot;
	aior->len = len;
	aior->done = done;
	aior->readop = readop;
	breq = &aior->io_req;
	breq->br_offset = lba + done;
	ahci_build_iov(p, aior, prdt, hdr->prdtl);

	/* Mark this command in-flight. */
	p->pending |= 1 << slot;

	/* Stuff request onto busy list. */
	TAILQ_INSERT_HEAD(&p->iobhd, aior, io_blist);

	if (ncq && first)
		ahci_write_fis_d2h_ncq(p, slot);

	if (readop)
		err = blockif_read(p->bctx, breq);
	else
		err = blockif_write(p->bctx, breq);
	assert(err == 0);
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
	STAILQ_REMOVE_HEAD(&p->iofhd, io_flist);
	aior->cfis = cfis;
	aior->slot = slot;
	aior->len = 0;
	aior->done = 0;
	aior->more = 0;
	breq = &aior->io_req;

	/*
	 * Mark this command in-flight.
	 */
	p->pending |= 1 << slot;

	/*
	 * Stuff request onto busy list
	 */
	TAILQ_INSERT_HEAD(&p->iobhd, aior, io_blist);

	err = blockif_flush(p->bctx, breq);
	assert(err == 0);
}

static inline unsigned int
read_prdt(struct ahci_port *p, int slot, uint8_t *cfis, void *buf,
    unsigned int size)
{
	struct ahci_cmd_hdr *hdr;
	struct ahci_prdt_entry *prdt;
	uint8_t *to;
	unsigned int len;
	int i;

	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	len = size;
	to = buf;
	prdt = (struct ahci_prdt_entry *)(cfis + 0x80);
	for (i = 0; i < hdr->prdtl && len; i++) {
		uint8_t *ptr;
		uint32_t dbcsz;
		unsigned int sublen;

		dbcsz = (prdt->dbc & DBCMASK) + 1;
		ptr = paddr_guest2host(ahci_ctx(p->pr_sc), prdt->dba, dbcsz);
		sublen = MIN(len, dbcsz);
		memcpy(to, ptr, sublen);
		len -= sublen;
		to += sublen;
		prdt++;
	}
	return (size - len);
}

static void
ahci_handle_dsm_trim(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint32_t len;
	int ncq;
	uint8_t *buf;
	unsigned int nread;

	buf = NULL;
	if (cfis[2] == ATA_DATA_SET_MANAGEMENT) {
		len = (uint16_t)cfis[13] << 8 | cfis[12];
		len *= 512;
		ncq = 0;
	} else { /* ATA_SEND_FPDMA_QUEUED */
		len = (uint16_t)cfis[11] << 8 | cfis[3];
		len *= 512;
		ncq = 1;
	}

	/* Support for only a single block is advertised via IDENTIFY. */
	if (len > 512) {
		goto invalid_command;
	}

	buf = malloc(len);
	nread = read_prdt(p, slot, cfis, buf, len);
	if (nread != len) {
		goto invalid_command;
	}
	ahci_handle_next_trim(p, slot, cfis, buf, len, 0);
	return;

invalid_command:
	free(buf);
	if (ncq) {
		ahci_write_fis_d2h_ncq(p, slot);
		ahci_write_fis_sdb(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
	} else {
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
	}
}

static void
ahci_handle_next_trim(struct ahci_port *p, int slot, uint8_t *cfis,
    uint8_t *buf, uint32_t len, uint32_t done)
{
	struct ahci_ioreq *aior;
	struct blockif_req *breq;
	uint8_t *entry;
	uint64_t elba;
	uint32_t elen;
	int err;
	bool first, ncq;

	first = (done == 0);
	if (cfis[2] == ATA_DATA_SET_MANAGEMENT) {
		ncq = false;
	} else { /* ATA_SEND_FPDMA_QUEUED */
		ncq = true;
	}

	/* Find the next range to TRIM. */
	while (done < len) {
		entry = &buf[done];
		elba = ((uint64_t)entry[5] << 40) |
		    ((uint64_t)entry[4] << 32) |
		    ((uint64_t)entry[3] << 24) |
		    ((uint64_t)entry[2] << 16) |
		    ((uint64_t)entry[1] << 8) |
		    entry[0];
		elen = (uint16_t)entry[7] << 8 | entry[6];
		done += 8;
		if (elen != 0)
			break;
	}

	/* All remaining ranges were empty. */
	if (done == len) {
		free(buf);
		if (ncq) {
			if (first)
				ahci_write_fis_d2h_ncq(p, slot);
			ahci_write_fis_sdb(p, slot, cfis,
			    ATA_S_READY | ATA_S_DSC);
		} else {
			ahci_write_fis_d2h(p, slot, cfis,
			    ATA_S_READY | ATA_S_DSC);
		}
		if (!first) {
			p->pending &= ~(1 << slot);
			ahci_check_stopped(p);
			ahci_handle_port(p);
		}
		return;
	}

	/*
	 * Pull request off free list
	 */
	aior = STAILQ_FIRST(&p->iofhd);
	assert(aior != NULL);
	STAILQ_REMOVE_HEAD(&p->iofhd, io_flist);
	aior->cfis = cfis;
	aior->slot = slot;
	aior->len = len;
	aior->done = done;
	aior->dsm = buf;
	aior->more = (len != done);

	breq = &aior->io_req;
	breq->br_offset = elba * blockif_sectsz(p->bctx);
	breq->br_resid = elen * blockif_sectsz(p->bctx);

	/*
	 * Mark this command in-flight.
	 */
	p->pending |= 1 << slot;

	/*
	 * Stuff request onto busy list
	 */
	TAILQ_INSERT_HEAD(&p->iobhd, aior, io_blist);

	if (ncq && first)
		ahci_write_fis_d2h_ncq(p, slot);

	err = blockif_delete(p->bctx, breq);
	assert(err == 0);
}

static inline void
write_prdt(struct ahci_port *p, int slot, uint8_t *cfis, void *buf,
    unsigned int size)
{
	struct ahci_cmd_hdr *hdr;
	struct ahci_prdt_entry *prdt;
	uint8_t *from;
	unsigned int len;
	int i;

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
		sublen = MIN(len, dbcsz);
		memcpy(ptr, from, sublen);
		len -= sublen;
		from += sublen;
		prdt++;
	}
	hdr->prdbc = size - len;
}

static void
ahci_checksum(uint8_t *buf, int size)
{
	int i;
	uint8_t sum = 0;

	for (i = 0; i < size - 1; i++)
		sum += buf[i];
	buf[size - 1] = 0x100 - sum;
}

static void
ahci_handle_read_log(struct ahci_port *p, int slot, uint8_t *cfis)
{
	struct ahci_cmd_hdr *hdr;
	uint32_t buf[128];
	uint8_t *buf8 = (uint8_t *)buf;
	uint16_t *buf16 = (uint16_t *)buf;

	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	if (p->atapi || hdr->prdtl == 0 || cfis[5] != 0 ||
	    cfis[9] != 0 || cfis[12] != 1 || cfis[13] != 0) {
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
		return;
	}

	memset(buf, 0, sizeof(buf));
	if (cfis[4] == 0x00) {	/* Log directory */
		buf16[0x00] = 1; /* Version -- 1 */
		buf16[0x10] = 1; /* NCQ Command Error Log -- 1 page */
		buf16[0x13] = 1; /* SATA NCQ Send and Receive Log -- 1 page */
	} else if (cfis[4] == 0x10) {	/* NCQ Command Error Log */
		memcpy(buf8, p->err_cfis, sizeof(p->err_cfis));
		ahci_checksum(buf8, sizeof(buf));
	} else if (cfis[4] == 0x13) {	/* SATA NCQ Send and Receive Log */
		if (blockif_candelete(p->bctx) && !blockif_is_ro(p->bctx)) {
			buf[0x00] = 1;	/* SFQ DSM supported */
			buf[0x01] = 1;	/* SFQ DSM TRIM supported */
		}
	} else {
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
		return;
	}

	if (cfis[2] == ATA_READ_LOG_EXT)
		ahci_write_fis_piosetup(p);
	write_prdt(p, slot, cfis, (void *)buf, sizeof(buf));
	ahci_write_fis_d2h(p, slot, cfis, ATA_S_DSC | ATA_S_READY);
}

static void
handle_identify(struct ahci_port *p, int slot, uint8_t *cfis)
{
	struct ahci_cmd_hdr *hdr;

	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	if (p->atapi || hdr->prdtl == 0) {
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
	} else {
		ahci_write_fis_piosetup(p);
		write_prdt(p, slot, cfis, (void*)&p->ata_ident, sizeof(struct ata_params));
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_DSC | ATA_S_READY);
	}
}

static void
ata_identify_init(struct ahci_port* p, int atapi)
{
	struct ata_params* ata_ident = &p->ata_ident;

	if (atapi) {
		ata_ident->config = ATA_PROTO_ATAPI | ATA_ATAPI_TYPE_CDROM |
		    ATA_ATAPI_REMOVABLE | ATA_DRQ_FAST;
		ata_ident->capabilities1 = ATA_SUPPORT_LBA |
			ATA_SUPPORT_DMA;
		ata_ident->capabilities2 = (1 << 14 | 1);
		ata_ident->atavalid = ATA_FLAG_64_70 | ATA_FLAG_88;
		ata_ident->obsolete62 = 0x3f;
		ata_ident->mwdmamodes = 7;
		if (p->xfermode & ATA_WDMA0)
			ata_ident->mwdmamodes |= (1 << ((p->xfermode & 7) + 8));
		ata_ident->apiomodes = 3;
		ata_ident->mwdmamin = 0x0078;
		ata_ident->mwdmarec = 0x0078;
		ata_ident->pioblind = 0x0078;
		ata_ident->pioiordy = 0x0078;
		ata_ident->satacapabilities = (ATA_SATA_GEN1 | ATA_SATA_GEN2 | ATA_SATA_GEN3);
		ata_ident->satacapabilities2 = ((p->ssts & ATA_SS_SPD_MASK) >> 3);
		ata_ident->satasupport = ATA_SUPPORT_NCQ_STREAM;
		ata_ident->version_major = 0x3f0;
		ata_ident->support.command1 = (ATA_SUPPORT_POWERMGT | ATA_SUPPORT_PACKET |
			ATA_SUPPORT_RESET | ATA_SUPPORT_NOP);
		ata_ident->support.command2 = (1 << 14);
		ata_ident->support.extension = (1 << 14);
		ata_ident->enabled.command1 = (ATA_SUPPORT_POWERMGT | ATA_SUPPORT_PACKET |
			ATA_SUPPORT_RESET | ATA_SUPPORT_NOP);
		ata_ident->enabled.extension = (1 << 14);
		ata_ident->udmamodes = 0x7f;
		if (p->xfermode & ATA_UDMA0)
			ata_ident->udmamodes |= (1 << ((p->xfermode & 7) + 8));
		ata_ident->transport_major = 0x1020;
		ata_ident->integrity = 0x00a5;
	} else {
		uint64_t sectors;
		int sectsz, psectsz, psectoff, candelete, ro;
		uint16_t cyl;
		uint8_t sech, heads;

		ro = blockif_is_ro(p->bctx);
		candelete = blockif_candelete(p->bctx);
		sectsz = blockif_sectsz(p->bctx);
		sectors = blockif_size(p->bctx) / sectsz;
		blockif_chs(p->bctx, &cyl, &heads, &sech);
		blockif_psectsz(p->bctx, &psectsz, &psectoff);
		ata_ident->config = ATA_DRQ_FAST;
		ata_ident->cylinders = cyl;
		ata_ident->heads = heads;
		ata_ident->sectors = sech;

		ata_ident->sectors_intr = (0x8000 | 128);
		ata_ident->tcg = 0;

		ata_ident->capabilities1 = ATA_SUPPORT_DMA |
			ATA_SUPPORT_LBA | ATA_SUPPORT_IORDY;
		ata_ident->capabilities2 = (1 << 14);
		ata_ident->atavalid = ATA_FLAG_64_70 | ATA_FLAG_88;
		if (p->mult_sectors)
			ata_ident->multi = (ATA_MULTI_VALID | p->mult_sectors);
		if (sectors <= 0x0fffffff) {
			ata_ident->lba_size_1 = sectors;
			ata_ident->lba_size_2 = (sectors >> 16);
		} else {
			ata_ident->lba_size_1 = 0xffff;
			ata_ident->lba_size_2 = 0x0fff;
		}
		ata_ident->mwdmamodes = 0x7;
		if (p->xfermode & ATA_WDMA0)
			ata_ident->mwdmamodes |= (1 << ((p->xfermode & 7) + 8));
		ata_ident->apiomodes = 0x3;
		ata_ident->mwdmamin = 0x0078;
		ata_ident->mwdmarec = 0x0078;
		ata_ident->pioblind = 0x0078;
		ata_ident->pioiordy = 0x0078;
		ata_ident->support3 = 0;
		ata_ident->queue = 31;
		ata_ident->satacapabilities = (ATA_SATA_GEN1 | ATA_SATA_GEN2 | ATA_SATA_GEN3 |
			ATA_SUPPORT_NCQ);
		ata_ident->satacapabilities2 = (ATA_SUPPORT_RCVSND_FPDMA_QUEUED |
			(p->ssts & ATA_SS_SPD_MASK) >> 3);
		ata_ident->version_major = 0x3f0;
		ata_ident->version_minor = 0x28;
		ata_ident->support.command1 = (ATA_SUPPORT_POWERMGT | ATA_SUPPORT_WRITECACHE |
			ATA_SUPPORT_LOOKAHEAD | ATA_SUPPORT_NOP);
		ata_ident->support.command2 = (ATA_SUPPORT_ADDRESS48 | ATA_SUPPORT_FLUSHCACHE |
			ATA_SUPPORT_FLUSHCACHE48 | 1 << 14);
		ata_ident->support.extension = (1 << 14);
		ata_ident->enabled.command1 = (ATA_SUPPORT_POWERMGT | ATA_SUPPORT_WRITECACHE |
			ATA_SUPPORT_LOOKAHEAD | ATA_SUPPORT_NOP);
		ata_ident->enabled.command2 = (ATA_SUPPORT_ADDRESS48 | ATA_SUPPORT_FLUSHCACHE |
			ATA_SUPPORT_FLUSHCACHE48 | 1 << 15);
		ata_ident->enabled.extension = (1 << 14);
		ata_ident->udmamodes = 0x7f;
		if (p->xfermode & ATA_UDMA0)
			ata_ident->udmamodes |= (1 << ((p->xfermode & 7) + 8));
		ata_ident->lba_size48_1 = sectors;
		ata_ident->lba_size48_2 = (sectors >> 16);
		ata_ident->lba_size48_3 = (sectors >> 32);
		ata_ident->lba_size48_4 = (sectors >> 48);

		if (candelete && !ro) {
			ata_ident->support3 |= ATA_SUPPORT_RZAT | ATA_SUPPORT_DRAT;
			ata_ident->max_dsm_blocks = 1;
			ata_ident->support_dsm = ATA_SUPPORT_DSM_TRIM;
		}
		ata_ident->pss = ATA_PSS_VALID_VALUE;
		ata_ident->lsalign = 0x4000;
		if (psectsz > sectsz) {
			ata_ident->pss |= ATA_PSS_MULTLS;
			ata_ident->pss |= ffsl(psectsz / sectsz) - 1;
			ata_ident->lsalign |= (psectoff / sectsz);
		}
		if (sectsz > 512) {
			ata_ident->pss |= ATA_PSS_LSSABOVE512;
			ata_ident->lss_1 = sectsz / 2;
			ata_ident->lss_2 = ((sectsz / 2) >> 16);
		}
		ata_ident->support2 = (ATA_SUPPORT_RWLOGDMAEXT | 1 << 14);
		ata_ident->enabled2 = (ATA_SUPPORT_RWLOGDMAEXT | 1 << 14);
		ata_ident->transport_major = 0x1020;
		ata_ident->integrity = 0x00a5;
	}
	ahci_checksum((uint8_t*)ata_ident, sizeof(struct ata_params));
}

static void
handle_atapi_identify(struct ahci_port *p, int slot, uint8_t *cfis)
{
	if (!p->atapi) {
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
	} else {
		ahci_write_fis_piosetup(p);
		write_prdt(p, slot, cfis, (void *)&p->ata_ident, sizeof(struct ata_params));
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_DSC | ATA_S_READY);
	}
}

static void
atapi_inquiry(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t buf[36];
	uint8_t *acmd;
	unsigned int len;
	uint32_t tfd;

	acmd = cfis + 0x40;

	if (acmd[1] & 1) {		/* VPD */
		if (acmd[2] == 0) {	/* Supported VPD pages */
			buf[0] = 0x05;
			buf[1] = 0;
			buf[2] = 0;
			buf[3] = 1;
			buf[4] = 0;
			len = 4 + buf[3];
		} else {
			p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
			p->asc = 0x24;
			tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
			cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
			ahci_write_fis_d2h(p, slot, cfis, tfd);
			return;
		}
	} else {
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
	}

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
	unsigned int len;

	acmd = cfis + 0x40;

	len = be16dec(acmd + 7);
	format = acmd[9] >> 6;
	switch (format) {
	case 0:
	{
		size_t size;
		int msf;
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
		size_t size;
		int msf;
		uint64_t sectors;
		uint8_t *bp, buf[50];

		msf = (acmd[1] >> 1) & 1;
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
atapi_report_luns(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t buf[16];

	memset(buf, 0, sizeof(buf));
	buf[3] = 8;

	cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
	write_prdt(p, slot, cfis, buf, sizeof(buf));
	ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
}

static void
atapi_read(struct ahci_port *p, int slot, uint8_t *cfis, uint32_t done)
{
	struct ahci_ioreq *aior;
	struct ahci_cmd_hdr *hdr;
	struct ahci_prdt_entry *prdt;
	struct blockif_req *breq;
	uint8_t *acmd;
	uint64_t lba;
	uint32_t len;
	int err;

	acmd = cfis + 0x40;
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
	prdt = (struct ahci_prdt_entry *)(cfis + 0x80);

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
	STAILQ_REMOVE_HEAD(&p->iofhd, io_flist);
	aior->cfis = cfis;
	aior->slot = slot;
	aior->len = len;
	aior->done = done;
	aior->readop = 1;
	breq = &aior->io_req;
	breq->br_offset = lba + done;
	ahci_build_iov(p, aior, prdt, hdr->prdtl);

	/* Mark this command in-flight. */
	p->pending |= 1 << slot;

	/* Stuff request onto busy list. */
	TAILQ_INSERT_HEAD(&p->iobhd, aior, io_blist);

	err = blockif_read(p->bctx, breq);
	assert(err == 0);
}

static void
atapi_request_sense(struct ahci_port *p, int slot, uint8_t *cfis)
{
	uint8_t buf[64];
	uint8_t *acmd;
	unsigned int len;

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
	unsigned int len;

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
		unsigned int len;

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
		DPRINTF("");
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
	case REPORT_LUNS:
		atapi_report_luns(p, slot, cfis);
		break;
	case READ_10:
	case READ_12:
		atapi_read(p, slot, cfis, 0);
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

	p->tfd |= ATA_S_BUSY;
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
		ahci_write_fis_d2h(p, slot, cfis, p->tfd);
		break;
	case ATA_READ:
	case ATA_WRITE:
	case ATA_READ48:
	case ATA_WRITE48:
	case ATA_READ_MUL:
	case ATA_WRITE_MUL:
	case ATA_READ_MUL48:
	case ATA_WRITE_MUL48:
	case ATA_READ_DMA:
	case ATA_WRITE_DMA:
	case ATA_READ_DMA48:
	case ATA_WRITE_DMA48:
	case ATA_READ_FPDMA_QUEUED:
	case ATA_WRITE_FPDMA_QUEUED:
		ahci_handle_rw(p, slot, cfis, 0);
		break;
	case ATA_FLUSHCACHE:
	case ATA_FLUSHCACHE48:
		ahci_handle_flush(p, slot, cfis);
		break;
	case ATA_DATA_SET_MANAGEMENT:
		if (cfis[11] == 0 && cfis[3] == ATA_DSM_TRIM &&
		    cfis[13] == 0 && cfis[12] == 1) {
			ahci_handle_dsm_trim(p, slot, cfis);
			break;
		}
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
		break;
	case ATA_SEND_FPDMA_QUEUED:
		if ((cfis[13] & 0x1f) == ATA_SFPDMA_DSM &&
		    cfis[17] == 0 && cfis[16] == ATA_DSM_TRIM &&
		    cfis[11] == 0 && cfis[3] == 1) {
			ahci_handle_dsm_trim(p, slot, cfis);
			break;
		}
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
		break;
	case ATA_READ_LOG_EXT:
	case ATA_READ_LOG_DMA_EXT:
		ahci_handle_read_log(p, slot, cfis);
		break;
	case ATA_SECURITY_FREEZE_LOCK:
	case ATA_SMART_CMD:
	case ATA_NOP:
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
		break;
	case ATA_CHECK_POWER_MODE:
		cfis[12] = 0xff;	/* always on */
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
		break;
	case ATA_STANDBY_CMD:
	case ATA_STANDBY_IMMEDIATE:
	case ATA_IDLE_CMD:
	case ATA_IDLE_IMMEDIATE:
	case ATA_SLEEP:
	case ATA_READ_VERIFY:
	case ATA_READ_VERIFY48:
		ahci_write_fis_d2h(p, slot, cfis, ATA_S_READY | ATA_S_DSC);
		break;
	case ATA_ATAPI_IDENTIFY:
		handle_atapi_identify(p, slot, cfis);
		break;
	case ATA_PACKET_CMD:
		if (!p->atapi) {
			ahci_write_fis_d2h(p, slot, cfis,
			    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
		} else
			handle_packet_cmd(p, slot, cfis);
		break;
	default:
		EPRINTLN("Unsupported cmd:%02x", cfis[2]);
		ahci_write_fis_d2h(p, slot, cfis,
		    (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR);
		break;
	}
}

static void
ahci_handle_slot(struct ahci_port *p, int slot)
{
	struct ahci_cmd_hdr *hdr;
#ifdef AHCI_DEBUG
	struct ahci_prdt_entry *prdt;
#endif
	struct pci_ahci_softc *sc;
	uint8_t *cfis;
#ifdef AHCI_DEBUG
	int cfl, i;
#endif

	sc = p->pr_sc;
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);
#ifdef AHCI_DEBUG
	cfl = (hdr->flags & 0x1f) * 4;
#endif
	cfis = paddr_guest2host(ahci_ctx(sc), hdr->ctba,
			0x80 + hdr->prdtl * sizeof(struct ahci_prdt_entry));
#ifdef AHCI_DEBUG
	prdt = (struct ahci_prdt_entry *)(cfis + 0x80);

	DPRINTF("cfis:");
	for (i = 0; i < cfl; i++) {
		if (i % 10 == 0)
			DPRINTF("");
		DPRINTF("%02x ", cfis[i]);
	}
	DPRINTF("");

	for (i = 0; i < hdr->prdtl; i++) {
		DPRINTF("%d@%08"PRIx64"", prdt->dbc & 0x3fffff, prdt->dba);
		prdt++;
	}
#endif

	if (cfis[0] != FIS_TYPE_REGH2D) {
		EPRINTLN("Not a H2D FIS:%02x", cfis[0]);
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

	if (!(p->cmd & AHCI_P_CMD_ST))
		return;

	/*
	 * Search for any new commands to issue ignoring those that
	 * are already in-flight.  Stop if device is busy or in error.
	 */
	for (; (p->ci & ~p->pending) != 0; p->ccs = ((p->ccs + 1) & 31)) {
		if ((p->tfd & (ATA_S_BUSY | ATA_S_DRQ)) != 0)
			break;
		if (p->waitforclear)
			break;
		if ((p->ci & ~p->pending & (1 << p->ccs)) != 0) {
			p->cmd &= ~AHCI_P_CMD_CCS_MASK;
			p->cmd |= p->ccs << AHCI_P_CMD_CCS_SHIFT;
			ahci_handle_slot(p, p->ccs);
		}
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
	uint8_t *cfis, *dsm;
	int slot, ncq;

	DPRINTF("%s %d", __func__, err);

	ncq = 0;
	aior = br->br_param;
	p = aior->io_pr;
	cfis = aior->cfis;
	slot = aior->slot;
	sc = p->pr_sc;
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + slot * AHCI_CL_SIZE);

	if (cfis[2] == ATA_WRITE_FPDMA_QUEUED ||
	    cfis[2] == ATA_READ_FPDMA_QUEUED ||
	    cfis[2] == ATA_SEND_FPDMA_QUEUED)
		ncq = 1;
	dsm = aior->dsm;
	aior->dsm = NULL;

	pthread_mutex_lock(&sc->mtx);

	/*
	 * Delete the blockif request from the busy list
	 */
	TAILQ_REMOVE(&p->iobhd, aior, io_blist);

	/*
	 * Move the blockif request back to the free list
	 */
	STAILQ_INSERT_TAIL(&p->iofhd, aior, io_flist);

	if (!err)
		hdr->prdbc = aior->done;

	if (!err && aior->more) {
		if (dsm != NULL)
			ahci_handle_next_trim(p, slot, cfis, dsm,
			    aior->len, aior->done);
		else
			ahci_handle_rw(p, slot, cfis, aior->done);
		goto out;
	}

	if (!err)
		tfd = ATA_S_READY | ATA_S_DSC;
	else
		tfd = (ATA_E_ABORT << 8) | ATA_S_READY | ATA_S_ERROR;
	if (ncq)
		ahci_write_fis_sdb(p, slot, cfis, tfd);
	else
		ahci_write_fis_d2h(p, slot, cfis, tfd);

	/*
	 * This command is now complete.
	 */
	p->pending &= ~(1 << slot);

	ahci_check_stopped(p);
	ahci_handle_port(p);
	free(dsm);
out:
	pthread_mutex_unlock(&sc->mtx);
	DPRINTF("%s exit", __func__);
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
	int slot;

	DPRINTF("%s %d", __func__, err);

	aior = br->br_param;
	p = aior->io_pr;
	cfis = aior->cfis;
	slot = aior->slot;
	sc = p->pr_sc;
	hdr = (struct ahci_cmd_hdr *)(p->cmd_lst + aior->slot * AHCI_CL_SIZE);

	pthread_mutex_lock(&sc->mtx);

	/*
	 * Delete the blockif request from the busy list
	 */
	TAILQ_REMOVE(&p->iobhd, aior, io_blist);

	/*
	 * Move the blockif request back to the free list
	 */
	STAILQ_INSERT_TAIL(&p->iofhd, aior, io_flist);

	if (!err)
		hdr->prdbc = aior->done;

	if (!err && aior->more) {
		atapi_read(p, slot, cfis, aior->done);
		goto out;
	}

	if (!err) {
		tfd = ATA_S_READY | ATA_S_DSC;
	} else {
		p->sense_key = ATA_SENSE_ILLEGAL_REQUEST;
		p->asc = 0x21;
		tfd = (p->sense_key << 12) | ATA_S_READY | ATA_S_ERROR;
	}
	cfis[4] = (cfis[4] & ~7) | ATA_I_CMD | ATA_I_IN;
	ahci_write_fis_d2h(p, slot, cfis, tfd);

	/*
	 * This command is now complete.
	 */
	p->pending &= ~(1 << slot);

	ahci_check_stopped(p);
	ahci_handle_port(p);
out:
	pthread_mutex_unlock(&sc->mtx);
	DPRINTF("%s exit", __func__);
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
		STAILQ_INSERT_TAIL(&pr->iofhd, vr, io_flist);
	}

	TAILQ_INIT(&pr->iobhd);
}

static void
pci_ahci_port_write(struct pci_ahci_softc *sc, uint64_t offset, uint64_t value)
{
	int port = (offset - AHCI_OFFSET) / AHCI_STEP;
	offset = (offset - AHCI_OFFSET) % AHCI_STEP;
	struct ahci_port *p = &sc->port[port];

	DPRINTF("pci_ahci_port %d: write offset 0x%"PRIx64" value 0x%"PRIx64"",
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
		ahci_port_intr(p);
		break;
	case AHCI_P_IE:
		p->ie = value & 0xFDC000FF;
		ahci_port_intr(p);
		break;
	case AHCI_P_CMD:
	{
		p->cmd &= ~(AHCI_P_CMD_ST | AHCI_P_CMD_SUD | AHCI_P_CMD_POD |
		    AHCI_P_CMD_CLO | AHCI_P_CMD_FRE | AHCI_P_CMD_APSTE |
		    AHCI_P_CMD_ATAPI | AHCI_P_CMD_DLAE | AHCI_P_CMD_ALPE |
		    AHCI_P_CMD_ASP | AHCI_P_CMD_ICC_MASK);
		p->cmd |= (AHCI_P_CMD_ST | AHCI_P_CMD_SUD | AHCI_P_CMD_POD |
		    AHCI_P_CMD_CLO | AHCI_P_CMD_FRE | AHCI_P_CMD_APSTE |
		    AHCI_P_CMD_ATAPI | AHCI_P_CMD_DLAE | AHCI_P_CMD_ALPE |
		    AHCI_P_CMD_ASP | AHCI_P_CMD_ICC_MASK) & value;

		if (!(value & AHCI_P_CMD_ST)) {
			ahci_port_stop(p);
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
			p->tfd &= ~(ATA_S_BUSY | ATA_S_DRQ);
			p->cmd &= ~AHCI_P_CMD_CLO;
		}

		if (value & AHCI_P_CMD_ICC_MASK) {
			p->cmd &= ~AHCI_P_CMD_ICC_MASK;
		}

		ahci_handle_port(p);
		break;
	}
	case AHCI_P_TFD:
	case AHCI_P_SIG:
	case AHCI_P_SSTS:
		EPRINTLN("pci_ahci_port: read only registers 0x%"PRIx64"", offset);
		break;
	case AHCI_P_SCTL:
		p->sctl = value;
		if (!(p->cmd & AHCI_P_CMD_ST)) {
			if (value & ATA_SC_DET_RESET)
				ahci_port_reset(p);
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
	DPRINTF("pci_ahci_host: write offset 0x%"PRIx64" value 0x%"PRIx64"",
		offset, value);

	switch (offset) {
	case AHCI_CAP:
	case AHCI_PI:
	case AHCI_VS:
	case AHCI_CAP2:
		DPRINTF("pci_ahci_host: read only registers 0x%"PRIx64"", offset);
		break;
	case AHCI_GHC:
		if (value & AHCI_GHC_HR) {
			ahci_reset(sc);
			break;
		}
		if (value & AHCI_GHC_IE)
			sc->ghc |= AHCI_GHC_IE;
		else
			sc->ghc &= ~AHCI_GHC_IE;
		ahci_generate_intr(sc, 0xffffffff);
		break;
	case AHCI_IS:
		sc->is &= ~value;
		ahci_generate_intr(sc, value);
		break;
	default:
		break;
	}
}

static void
pci_ahci_write(struct pci_devinst *pi, int baridx, uint64_t offset, int size,
    uint64_t value)
{
	struct pci_ahci_softc *sc = pi->pi_arg;

	assert(baridx == 5);
	assert((offset % 4) == 0 && size == 4);

	pthread_mutex_lock(&sc->mtx);

	if (offset < AHCI_OFFSET)
		pci_ahci_host_write(sc, offset, value);
	else if (offset < (uint64_t)AHCI_OFFSET + sc->ports * AHCI_STEP)
		pci_ahci_port_write(sc, offset, value);
	else
		EPRINTLN("pci_ahci: unknown i/o write offset 0x%"PRIx64"", offset);

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
	DPRINTF("pci_ahci_host: read offset 0x%"PRIx64" value 0x%x",
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

	DPRINTF("pci_ahci_port %d: read offset 0x%"PRIx64" value 0x%x",
		port, offset, value);

	return value;
}

static uint64_t
pci_ahci_read(struct pci_devinst *pi, int baridx, uint64_t regoff, int size)
{
	struct pci_ahci_softc *sc = pi->pi_arg;
	uint64_t offset;
	uint32_t value;

	assert(baridx == 5);
	assert(size == 1 || size == 2 || size == 4);
	assert((regoff & (size - 1)) == 0);

	pthread_mutex_lock(&sc->mtx);

	offset = regoff & ~0x3;	    /* round down to a multiple of 4 bytes */
	if (offset < AHCI_OFFSET)
		value = pci_ahci_host_read(sc, offset);
	else if (offset < (uint64_t)AHCI_OFFSET + sc->ports * AHCI_STEP)
		value = pci_ahci_port_read(sc, offset);
	else {
		value = 0;
		EPRINTLN("pci_ahci: unknown i/o read offset 0x%"PRIx64"",
		    regoff);
	}
	value >>= 8 * (regoff & 0x3);

	pthread_mutex_unlock(&sc->mtx);

	return (value);
}

/*
 * Each AHCI controller has a "port" node which contains nodes for
 * each port named after the decimal number of the port (no leading
 * zeroes).  Port nodes contain a "type" ("hd" or "cd"), as well as
 * options for blockif.  For example:
 *
 * pci.0.1.0
 *          .device="ahci"
 *          .port
 *               .0
 *                 .type="hd"
 *                 .path="/path/to/image"
 */
static int
pci_ahci_legacy_config_port(nvlist_t *nvl, int port, const char *type,
    const char *opts)
{
	char node_name[sizeof("XX")];
	nvlist_t *port_nvl;

	snprintf(node_name, sizeof(node_name), "%d", port);
	port_nvl = create_relative_config_node(nvl, node_name);
	set_config_value_node(port_nvl, "type", type);
	return (blockif_legacy_config(port_nvl, opts));
}

static int
pci_ahci_legacy_config(nvlist_t *nvl, const char *opts)
{
	nvlist_t *ports_nvl;
	const char *type;
	char *next, *next2, *str, *tofree;
	int p, ret;

	if (opts == NULL)
		return (0);

	ports_nvl = create_relative_config_node(nvl, "port");
	ret = 1;
	tofree = str = strdup(opts);
	for (p = 0; p < MAX_PORTS && str != NULL; p++, str = next) {
		/* Identify and cut off type of present port. */
		if (strncmp(str, "hd:", 3) == 0) {
			type = "hd";
			str += 3;
		} else if (strncmp(str, "cd:", 3) == 0) {
			type = "cd";
			str += 3;
		} else
			type = NULL;

		/* Find and cut off the next port options. */
		next = strstr(str, ",hd:");
		next2 = strstr(str, ",cd:");
		if (next == NULL || (next2 != NULL && next2 < next))
			next = next2;
		if (next != NULL) {
			next[0] = 0;
			next++;
		}

		if (str[0] == 0)
			continue;

		if (type == NULL) {
			EPRINTLN("Missing or invalid type for port %d: \"%s\"",
			    p, str);
			goto out;
		}

		if (pci_ahci_legacy_config_port(ports_nvl, p, type, str) != 0)
			goto out;
	}
	ret = 0;
out:
	free(tofree);
	return (ret);
}

static int
pci_ahci_cd_legacy_config(nvlist_t *nvl, const char *opts)
{
	nvlist_t *ports_nvl;

	ports_nvl = create_relative_config_node(nvl, "port");
	return (pci_ahci_legacy_config_port(ports_nvl, 0, "cd", opts));
}

static int
pci_ahci_hd_legacy_config(nvlist_t *nvl, const char *opts)
{
	nvlist_t *ports_nvl;

	ports_nvl = create_relative_config_node(nvl, "port");
	return (pci_ahci_legacy_config_port(ports_nvl, 0, "hd", opts));
}

static int
pci_ahci_init(struct pci_devinst *pi, nvlist_t *nvl)
{
	char bident[sizeof("XXX:XXX:XXX")];
	char node_name[sizeof("XX")];
	struct blockif_ctxt *bctxt;
	struct pci_ahci_softc *sc;
	int atapi, ret, slots, p;
	MD5_CTX mdctx;
	u_char digest[16];
	const char *path, *type, *value;
	nvlist_t *ports_nvl, *port_nvl;

	ret = 0;

#ifdef AHCI_DEBUG
	dbg = fopen("/tmp/log", "w+");
#endif

	sc = calloc(1, sizeof(struct pci_ahci_softc));
	pi->pi_arg = sc;
	sc->asc_pi = pi;
	pthread_mutex_init(&sc->mtx, NULL);
	sc->ports = 0;
	sc->pi = 0;
	slots = 32;

	ports_nvl = find_relative_config_node(nvl, "port");
	for (p = 0; ports_nvl != NULL && p < MAX_PORTS; p++) {
		struct ata_params *ata_ident = &sc->port[p].ata_ident;
		char ident[AHCI_PORT_IDENT];

		snprintf(node_name, sizeof(node_name), "%d", p);
		port_nvl = find_relative_config_node(ports_nvl, node_name);
		if (port_nvl == NULL)
			continue;

		type = get_config_value_node(port_nvl, "type");
		if (type == NULL)
			continue;

		if (strcmp(type, "hd") == 0)
			atapi = 0;
		else
			atapi = 1;

		/*
		 * Attempt to open the backing image. Use the PCI slot/func
		 * and the port number for the identifier string.
		 */
		snprintf(bident, sizeof(bident), "%u:%u:%u", pi->pi_slot,
		    pi->pi_func, p);

		bctxt = blockif_open(port_nvl, bident);
		if (bctxt == NULL) {
			sc->ports = p;
			ret = 1;
			goto open_fail;
		}

		ret = blockif_add_boot_device(pi, bctxt);
		if (ret) {
			sc->ports = p;
			goto open_fail;
		}

		sc->port[p].bctx = bctxt;
		sc->port[p].pr_sc = sc;
		sc->port[p].port = p;
		sc->port[p].atapi = atapi;

		/*
		 * Create an identifier for the backing file.
		 * Use parts of the md5 sum of the filename
		 */
		path = get_config_value_node(port_nvl, "path");
		MD5Init(&mdctx);
		MD5Update(&mdctx, path, strlen(path));
		MD5Final(digest, &mdctx);
		snprintf(ident, AHCI_PORT_IDENT,
			"BHYVE-%02X%02X-%02X%02X-%02X%02X",
			digest[0], digest[1], digest[2], digest[3], digest[4],
			digest[5]);

		memset(ata_ident, 0, sizeof(struct ata_params));
		ata_string((uint8_t*)&ata_ident->serial, ident, 20);
		ata_string((uint8_t*)&ata_ident->revision, "001", 8);
		if (atapi)
			ata_string((uint8_t*)&ata_ident->model, "BHYVE SATA DVD ROM", 40);
		else
			ata_string((uint8_t*)&ata_ident->model, "BHYVE SATA DISK", 40);
		value = get_config_value_node(port_nvl, "nmrr");
		if (value != NULL)
			ata_ident->media_rotation_rate = atoi(value);
		value = get_config_value_node(port_nvl, "ser");
		if (value != NULL)
			ata_string((uint8_t*)(&ata_ident->serial), value, 20);
		value = get_config_value_node(port_nvl, "rev");
		if (value != NULL)
			ata_string((uint8_t*)(&ata_ident->revision), value, 8);
		value = get_config_value_node(port_nvl, "model");
		if (value != NULL)
			ata_string((uint8_t*)(&ata_ident->model), value, 40);
		ata_identify_init(&sc->port[p], atapi);

		/*
		 * Allocate blockif request structures and add them
		 * to the free list
		 */
		pci_ahci_ioreq_init(&sc->port[p]);

		sc->pi |= (1 << p);
		if (sc->port[p].ioqsz < slots)
			slots = sc->port[p].ioqsz;
	}
	sc->ports = p;

	/* Intel ICH8 AHCI */
	--slots;
	if (sc->ports < DEF_PORTS)
		sc->ports = DEF_PORTS;
	sc->cap = AHCI_CAP_64BIT | AHCI_CAP_SNCQ | AHCI_CAP_SSNTF |
	    AHCI_CAP_SMPS | AHCI_CAP_SSS | AHCI_CAP_SALP |
	    AHCI_CAP_SAL | AHCI_CAP_SCLO | (0x3 << AHCI_CAP_ISS_SHIFT)|
	    AHCI_CAP_PMD | AHCI_CAP_SSC | AHCI_CAP_PSC |
	    (slots << AHCI_CAP_NCS_SHIFT) | AHCI_CAP_SXS | (sc->ports - 1);

	sc->vs = 0x10300;
	sc->cap2 = AHCI_CAP2_APST;
	ahci_reset(sc);

	pci_set_cfgdata16(pi, PCIR_DEVICE, 0x2821);
	pci_set_cfgdata16(pi, PCIR_VENDOR, 0x8086);
	pci_set_cfgdata8(pi, PCIR_CLASS, PCIC_STORAGE);
	pci_set_cfgdata8(pi, PCIR_SUBCLASS, PCIS_STORAGE_SATA);
	pci_set_cfgdata8(pi, PCIR_PROGIF, PCIP_STORAGE_SATA_AHCI_1_0);
	p = MIN(sc->ports, 16);
	p = flsl(p) - ((p & (p - 1)) ? 0 : 1);
	pci_emul_add_msicap(pi, 1 << p);
	pci_emul_alloc_bar(pi, 5, PCIBAR_MEM32,
	    AHCI_OFFSET + sc->ports * AHCI_STEP);

	pci_lintr_request(pi);

open_fail:
	if (ret) {
		for (p = 0; p < sc->ports; p++) {
			if (sc->port[p].bctx != NULL)
				blockif_close(sc->port[p].bctx);
		}
		free(sc);
	}

	return (ret);
}

#ifdef BHYVE_SNAPSHOT
static int
pci_ahci_snapshot(struct vm_snapshot_meta *meta)
{
	int i, ret;
	void *bctx;
	struct pci_devinst *pi;
	struct pci_ahci_softc *sc;
	struct ahci_port *port;

	pi = meta->dev_data;
	sc = pi->pi_arg;

	/* TODO: add mtx lock/unlock */

	SNAPSHOT_VAR_OR_LEAVE(sc->ports, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->cap, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->ghc, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->is, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->pi, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->vs, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->ccc_ctl, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->ccc_pts, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->em_loc, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->em_ctl, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->cap2, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->bohc, meta, ret, done);
	SNAPSHOT_VAR_OR_LEAVE(sc->lintr, meta, ret, done);

	for (i = 0; i < MAX_PORTS; i++) {
		port = &sc->port[i];

		if (meta->op == VM_SNAPSHOT_SAVE)
			bctx = port->bctx;

		SNAPSHOT_VAR_OR_LEAVE(bctx, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->port, meta, ret, done);

		/* Mostly for restore; save is ensured by the lines above. */
		if (((bctx == NULL) && (port->bctx != NULL)) ||
		    ((bctx != NULL) && (port->bctx == NULL))) {
			EPRINTLN("%s: ports not matching", __func__);
			ret = EINVAL;
			goto done;
		}

		if (port->bctx == NULL)
			continue;

		if (port->port != i) {
			EPRINTLN("%s: ports not matching: "
			    "actual: %d expected: %d", __func__, port->port, i);
			ret = EINVAL;
			goto done;
		}

		SNAPSHOT_GUEST2HOST_ADDR_OR_LEAVE(pi->pi_vmctx, port->cmd_lst,
			AHCI_CL_SIZE * AHCI_MAX_SLOTS, false, meta, ret, done);
		SNAPSHOT_GUEST2HOST_ADDR_OR_LEAVE(pi->pi_vmctx, port->rfis, 256,
		    false, meta, ret, done);

		SNAPSHOT_VAR_OR_LEAVE(port->ata_ident, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->atapi, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->reset, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->waitforclear, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->mult_sectors, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->xfermode, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->err_cfis, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->sense_key, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->asc, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->ccs, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->pending, meta, ret, done);

		SNAPSHOT_VAR_OR_LEAVE(port->clb, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->clbu, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->fb, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->fbu, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->ie, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->cmd, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->unused0, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->tfd, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->sig, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->ssts, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->sctl, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->serr, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->sact, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->ci, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->sntf, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->fbs, meta, ret, done);
		SNAPSHOT_VAR_OR_LEAVE(port->ioqsz, meta, ret, done);

		assert(TAILQ_EMPTY(&port->iobhd));
	}

done:
	return (ret);
}

static int
pci_ahci_pause(struct pci_devinst *pi)
{
	struct pci_ahci_softc *sc;
	struct blockif_ctxt *bctxt;
	int i;

	sc = pi->pi_arg;

	for (i = 0; i < MAX_PORTS; i++) {
		bctxt = sc->port[i].bctx;
		if (bctxt == NULL)
			continue;

		blockif_pause(bctxt);
	}

	return (0);
}

static int
pci_ahci_resume(struct pci_devinst *pi)
{
	struct pci_ahci_softc *sc;
	struct blockif_ctxt *bctxt;
	int i;

	sc = pi->pi_arg;

	for (i = 0; i < MAX_PORTS; i++) {
		bctxt = sc->port[i].bctx;
		if (bctxt == NULL)
			continue;

		blockif_resume(bctxt);
	}

	return (0);
}
#endif	/* BHYVE_SNAPSHOT */

/*
 * Use separate emulation names to distinguish drive and atapi devices
 */
static const struct pci_devemu pci_de_ahci = {
	.pe_emu =	"ahci",
	.pe_init =	pci_ahci_init,
	.pe_legacy_config = pci_ahci_legacy_config,
	.pe_barwrite =	pci_ahci_write,
	.pe_barread =	pci_ahci_read,
#ifdef BHYVE_SNAPSHOT
	.pe_snapshot =	pci_ahci_snapshot,
	.pe_pause =	pci_ahci_pause,
	.pe_resume =	pci_ahci_resume,
#endif
};
PCI_EMUL_SET(pci_de_ahci);

static const struct pci_devemu pci_de_ahci_hd = {
	.pe_emu =	"ahci-hd",
	.pe_legacy_config = pci_ahci_hd_legacy_config,
	.pe_alias =	"ahci",
};
PCI_EMUL_SET(pci_de_ahci_hd);

static const struct pci_devemu pci_de_ahci_cd = {
	.pe_emu =	"ahci-cd",
	.pe_legacy_config = pci_ahci_cd_legacy_config,
	.pe_alias =	"ahci",
};
PCI_EMUL_SET(pci_de_ahci_cd);
