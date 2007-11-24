/*	$NecBSD: bshw_machdep.c,v 1.8.12.6 2001/06/29 06:28:05 honda Exp $	*/

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
/*	$NetBSD$	*/

/*-
 * [NetBSD for NEC PC-98 series]
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	NetBSD/pc98 porting staff. All rights reserved.
 * 
 *  Copyright (c) 1994, 1995, 1996, 1997, 1998, 1999, 2000, 2001
 *	Naofumi HONDA.  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *  3. The name of the author may not be used to endorse or promote products
 *     derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include "opt_ddb.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#if defined(__FreeBSD__) && __FreeBSD_version > 500001
#include <sys/bio.h>
#endif	/* __ FreeBSD__ */
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/malloc.h>
#include <sys/errno.h>

#include <vm/vm.h>

#ifdef __NetBSD__
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <dev/scsipi/scsi_all.h>
#include <dev/scsipi/scsipi_all.h>
#include <dev/scsipi/scsiconf.h>
#include <dev/scsipi/scsi_disk.h>

#include <machine/dvcfg.h>
#include <machine/physio_proc.h>

#include <i386/Cbus/dev/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <i386/Cbus/dev/ct/ctvar.h>
#include <i386/Cbus/dev/ct/ct_machdep.h>
#include <i386/Cbus/dev/ct/bshwvar.h>
#endif /* __NetBSD__ */

#ifdef __FreeBSD__
#include <machine/bus.h>
#include <machine/clock.h>
#include <machine/md_var.h>

#include <compat/netbsd/dvcfg.h>
#include <compat/netbsd/physio_proc.h>

#include <cam/scsi/scsi_low.h>

#include <dev/ic/wd33c93reg.h>
#include <dev/ct/ctvar.h>
#include <dev/ct/ct_machdep.h>
#include <dev/ct/bshwvar.h>

#include <vm/pmap.h>
#endif /* __FreeBSD__ */

#define	BSHW_IO_CONTROL_FLAGS	0

u_int bshw_io_control = BSHW_IO_CONTROL_FLAGS;
int bshw_data_read_bytes = 4096;
int bshw_data_write_bytes = 4096;

/*********************************************************
 * OS dep part
 *********************************************************/
#ifdef	__NetBSD__
#define	BSHW_PAGE_SIZE NBPG
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
#define	BSHW_PAGE_SIZE PAGE_SIZE
typedef	unsigned long vaddr_t;
#endif /* __FreeBSD__ */

/*********************************************************
 * GENERIC MACHDEP FUNCTIONS
 *********************************************************/
void
bshw_synch_setup(ct, ti)
	struct ct_softc *ct;
	struct targ_info *ti;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct ct_targ_info *cti = (void *) ti;
	struct bshw_softc *bs = ct->ct_hw;
	struct bshw *hw = bs->sc_hw;

	if (hw->hw_sregaddr == 0)
		return;

	ct_cr_write_1(chp, hw->hw_sregaddr + ti->ti_id, cti->cti_syncreg);
	if (hw->hw_flags & BSHW_DOUBLE_DMACHAN)
	{
		ct_cr_write_1(chp, hw->hw_sregaddr + ti->ti_id + 8, 
			      cti->cti_syncreg);
	}
}

void
bshw_bus_reset(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct bshw_softc *bs = ct->ct_hw;
	struct bshw *hw = bs->sc_hw;
	bus_addr_t offs;
	u_int8_t regv;
	int i;

	/* open hardware busmaster mode */
	if (hw->hw_dma_init != NULL && ((*hw->hw_dma_init)(ct)) != 0)
	{
		printf("%s: change mode using external DMA (%x)\n",
		    slp->sl_xname, (u_int)ct_cr_read_1(chp, 0x37));
	}

	/* clear hardware synch registers */
	offs = hw->hw_sregaddr;
	if (offs != 0)
	{
		for (i = 0; i < 8; i ++, offs ++)
		{
			ct_cr_write_1(chp, offs, 0);
			if ((hw->hw_flags & BSHW_DOUBLE_DMACHAN) != 0)
				ct_cr_write_1(chp, offs + 8, 0);
		}
	}

	/* disable interrupt & assert reset */
	regv = ct_cr_read_1(chp, wd3s_mbank);
	regv |= MBR_RST;
	regv &= ~MBR_IEN;
	ct_cr_write_1(chp, wd3s_mbank, regv);

	SCSI_LOW_DELAY(500000);

	/* reset signal off */
	regv &= ~MBR_RST;
	ct_cr_write_1(chp, wd3s_mbank, regv);

	/* interrupt enable */
	regv |= MBR_IEN;
	ct_cr_write_1(chp, wd3s_mbank, regv);
}

/* probe */
int
bshw_read_settings(chp, bs)
	struct ct_bus_access_handle *chp;
	struct bshw_softc *bs;
{
	static int irq_tbl[] = { 3, 5, 6, 9, 12, 13 };

	bs->sc_hostid = (ct_cr_read_1(chp, wd3s_auxc) & AUXCR_HIDM);
	bs->sc_irq = irq_tbl[(ct_cr_read_1(chp, wd3s_auxc) >> 3) & 7];
	bs->sc_drq = ct_cmdp_read_1(chp) & 3;
	return 0;
}

/*********************************************************
 * DMA PIO TRANSFER (SMIT)
 *********************************************************/
#define	LC_SMIT_TIMEOUT	2	/* 2 sec: timeout for a fifo status ready */
#define	LC_SMIT_OFFSET	0x1000
#define	LC_FSZ		DEV_BSIZE
#define	LC_SFSZ		0x0c
#define	LC_REST		(LC_FSZ - LC_SFSZ)

#define	BSHW_LC_FSET	0x36
#define	BSHW_LC_FCTRL	0x44
#define	FCTRL_EN	0x01
#define	FCTRL_WRITE	0x02

#define	SF_ABORT	0x08
#define	SF_RDY		0x10

static __inline void bshw_lc_smit_start(struct ct_softc *, int, u_int);
static __inline void bshw_lc_smit_stop(struct ct_softc *);
static int bshw_lc_smit_fstat(struct ct_softc *, int, int);

static __inline void
bshw_lc_smit_stop(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	ct_cr_write_1(chp, BSHW_LC_FCTRL, 0);
	ct_cmdp_write_1(chp, CMDP_DMER);
}

static __inline void
bshw_lc_smit_start(ct, count, direction)
	struct ct_softc *ct;
	int count;
	u_int direction;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	u_int8_t pval, val;

	val = ct_cr_read_1(chp, BSHW_LC_FSET);
	cthw_set_count(chp, count);

	pval = FCTRL_EN;
	if (direction == SCSI_LOW_WRITE)
		pval |= (val & 0xe0) | FCTRL_WRITE;
	ct_cr_write_1(chp, BSHW_LC_FCTRL, pval);
	ct_cr_write_1(chp, wd3s_cmd, WD3S_TFR_INFO);
}

static int
bshw_lc_smit_fstat(ct, wc, read)
	struct ct_softc *ct;
	int wc, read;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	u_int8_t stat;

	while (wc -- > 0)
	{
		chp->ch_bus_weight(chp);
		stat = ct_cmdp_read_1(chp);
		if (read == SCSI_LOW_READ)
		{
			if ((stat & SF_RDY) != 0)
				return 0;
			if ((stat & SF_ABORT) != 0)
				return EIO;
		}
		else
		{
			if ((stat & SF_ABORT) != 0)
				return EIO;
			if ((stat & SF_RDY) != 0)
				return 0;
		}
	}

	printf("%s: SMIT fifo status timeout\n", ct->sc_sclow.sl_xname);
	return EIO;
}

void
bshw_smit_xfer_stop(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct bshw_softc *bs = ct->ct_hw;
	struct targ_info *ti;
	struct sc_p *sp = &slp->sl_scp;
	u_int count;

	bshw_lc_smit_stop(ct);

	ti = slp->sl_Tnexus;
	if (ti == NULL)
		return;

	if (ti->ti_phase == PH_DATA)
	{
		count = cthw_get_count(&ct->sc_ch);
		if (count < bs->sc_sdatalen)
		{
			if (sp->scp_direction == SCSI_LOW_READ &&
			    count != bs->sc_edatalen)
				goto bad;

			count = bs->sc_sdatalen - count;
			if (count > (u_int) sp->scp_datalen)
				goto bad;

			sp->scp_data += count;
			sp->scp_datalen -= count;
		}
		else if (count > bs->sc_sdatalen)
		{
bad:
			printf("%s: smit_xfer_end: cnt error\n", slp->sl_xname);
			slp->sl_error |= PDMAERR;
		}
		scsi_low_data_finish(slp);
	}
	else
	{
		printf("%s: smit_xfer_end: phase miss\n", slp->sl_xname);
		slp->sl_error |= PDMAERR;
	}
}

int
bshw_smit_xfer_start(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct bshw_softc *bs = ct->ct_hw;
	struct sc_p *sp = &slp->sl_scp;
	struct targ_info *ti = slp->sl_Tnexus;
	struct ct_targ_info *cti = (void *) ti;
	u_int datalen, count, io_control;
	int wc;
	u_int8_t *data;

	io_control = bs->sc_io_control | bshw_io_control;
	if ((io_control & BSHW_SMIT_BLOCK) != 0)
		return EINVAL;

	if ((slp->sl_scp.scp_datalen % DEV_BSIZE) != 0)
		return EINVAL;

	datalen = sp->scp_datalen;
	if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
	{
		if ((io_control & BSHW_READ_INTERRUPT_DRIVEN) != 0 &&
		     datalen > bshw_data_read_bytes)
			datalen = bshw_data_read_bytes;
	}
	else 
	{
	        if ((io_control & BSHW_WRITE_INTERRUPT_DRIVEN) != 0 &&
		    datalen > bshw_data_write_bytes)
			datalen = bshw_data_write_bytes;
	}

	bs->sc_sdatalen = datalen;
	data = sp->scp_data;
 	wc = LC_SMIT_TIMEOUT * 1024 * 1024;

	ct_cr_write_1(chp, wd3s_ctrl, ct->sc_creg | CR_DMA);
	bshw_lc_smit_start(ct, datalen, sp->scp_direction);

	if (sp->scp_direction == SCSI_LOW_READ)
	{
		do
		{
			if (bshw_lc_smit_fstat(ct, wc, SCSI_LOW_READ))
				break;

			count = (datalen > LC_FSZ ? LC_FSZ : datalen);
			bus_space_read_region_4(chp->ch_memt, chp->ch_memh,
				LC_SMIT_OFFSET, (u_int32_t *) data, count >> 2);
			data += count;
			datalen -= count;
		}
		while (datalen > 0);

		bs->sc_edatalen = datalen;
	}
	else
	{
		do
		{
			if (bshw_lc_smit_fstat(ct, wc, SCSI_LOW_WRITE))
				break;
			if (cti->cti_syncreg == 0)
			{
				/* XXX:
				 * If async transfer, reconfirm a scsi phase
				 * again. Unless C bus might hang up.
			 	 */
				if (bshw_lc_smit_fstat(ct, wc, SCSI_LOW_WRITE))
					break;
			}

			count = (datalen > LC_SFSZ ? LC_SFSZ : datalen);
			bus_space_write_region_4(chp->ch_memt, chp->ch_memh,
				LC_SMIT_OFFSET, (u_int32_t *) data, count >> 2);
			data += count;
			datalen -= count;

			if (bshw_lc_smit_fstat(ct, wc, SCSI_LOW_WRITE))
				break;

			count = (datalen > LC_REST ? LC_REST : datalen);
			bus_space_write_region_4(chp->ch_memt, chp->ch_memh,
						 LC_SMIT_OFFSET + LC_SFSZ, 
						 (u_int32_t *) data, count >> 2);
			data += count;
			datalen -= count;
		}
		while (datalen > 0);
	}
	return 0;
}

/*********************************************************
 * DMA TRANSFER (BS)
 *********************************************************/
static __inline void bshw_dma_write_1 \
	(struct ct_bus_access_handle *, bus_addr_t, u_int8_t);
static void bshw_dmastart(struct ct_softc *);
static void bshw_dmadone(struct ct_softc *);

int
bshw_dma_xfer_start(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct sc_p *sp = &slp->sl_scp;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	struct bshw_softc *bs = ct->ct_hw;
	vaddr_t va, endva, phys, nphys;
	u_int io_control;

	io_control = bs->sc_io_control | bshw_io_control;
	if ((io_control & BSHW_DMA_BLOCK) != 0 && sp->scp_datalen < 256)
		return EINVAL;

	ct_cr_write_1(chp, wd3s_ctrl, ct->sc_creg | CR_DMA);
	phys = vtophys((vaddr_t) sp->scp_data);
	if (phys >= bs->sc_minphys)
	{
		/* setup segaddr */
		bs->sc_segaddr = bs->sc_bounce_phys;
		/* setup seglen */
		bs->sc_seglen = sp->scp_datalen;
		if (bs->sc_seglen > bs->sc_bounce_size)
			bs->sc_seglen = bs->sc_bounce_size;
		/* setup bufp */
		bs->sc_bufp = bs->sc_bounce_addr;
		if (sp->scp_direction == SCSI_LOW_WRITE)
			bcopy(sp->scp_data, bs->sc_bufp, bs->sc_seglen);
	}
	else
	{
		/* setup segaddr */
		bs->sc_segaddr = (u_int8_t *) phys;
		/* setup seglen */
		endva = (vaddr_t) round_page((vaddr_t) sp->scp_data + sp->scp_datalen);
		for (va = (vaddr_t) sp->scp_data; ; phys = nphys)
		{
			if ((va += BSHW_PAGE_SIZE) >= endva)
			{
				bs->sc_seglen = sp->scp_datalen;
				break;
			}

			nphys = vtophys(va);
			if (phys + BSHW_PAGE_SIZE != nphys || nphys >= bs->sc_minphys)
			{
				bs->sc_seglen =
				    (u_int8_t *) trunc_page(va) - sp->scp_data;
				break;
			}
		}
		/* setup bufp */
		bs->sc_bufp = NULL;
	}

	bshw_dmastart(ct);
	cthw_set_count(chp, bs->sc_seglen);
	ct_cr_write_1(chp, wd3s_cmd, WD3S_TFR_INFO);
	return 0;
}

void
bshw_dma_xfer_stop(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct sc_p *sp = &slp->sl_scp;
	struct bshw_softc *bs = ct->ct_hw;
	struct targ_info *ti;
	u_int count, transbytes;

	bshw_dmadone(ct);

 	ti = slp->sl_Tnexus;
	if (ti == NULL)
		return;

	if (ti->ti_phase == PH_DATA)
	{
		count = cthw_get_count(&ct->sc_ch);
		if (count < (u_int) bs->sc_seglen)
		{
			transbytes = bs->sc_seglen - count;
			if (bs->sc_bufp != NULL &&
			    sp->scp_direction == SCSI_LOW_READ)
				bcopy(bs->sc_bufp, sp->scp_data, transbytes);

			sp->scp_data += transbytes;
			sp->scp_datalen -= transbytes;
		}
		else if (count > (u_int) bs->sc_seglen)
		{
			printf("%s: port data %x != seglen %x\n",
				slp->sl_xname, count, bs->sc_seglen);
			slp->sl_error |= PDMAERR;
		}

		scsi_low_data_finish(slp);
	}
	else
	{
		printf("%s: extra DMA interrupt\n", slp->sl_xname);
		slp->sl_error |= PDMAERR;
	}

	bs->sc_bufp = NULL;
}

/* common dma settings */
#undef	DMA1_SMSK
#define DMA1_SMSK	(0x15)
#undef	DMA1_MODE
#define DMA1_MODE	(0x17)
#undef	DMA1_FFC
#define DMA1_FFC	(0x19)
#undef	DMA1_CHN
#define DMA1_CHN(c)	(0x01 + ((c) << 2))

#define DMA37SM_SET	0x04
#define	DMA37MD_WRITE	0x04
#define	DMA37MD_READ	0x08
#define	DMA37MD_SINGLE	0x40

static bus_addr_t dmapageport[4] = { 0x27, 0x21, 0x23, 0x25 };

static __inline void 
bshw_dma_write_1(chp, port, val)
	struct ct_bus_access_handle *chp;
	bus_addr_t port;
	u_int8_t val;
{

	CT_BUS_WEIGHT(chp);
	outb(port, val);
}

static void
bshw_dmastart(ct)
	struct ct_softc *ct;
{
	struct scsi_low_softc *slp = &ct->sc_sclow;
	struct bshw_softc *bs = ct->ct_hw;
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	int chan = bs->sc_drq;
	bus_addr_t waport;
	u_int8_t regv, *phys = bs->sc_segaddr;
	u_int nbytes = bs->sc_seglen;

	/* flush cpu cache */
	(*bs->sc_dmasync_before) (ct);

	/*
	 * Program one of DMA channels 0..3. These are
	 * byte mode channels.
	 */
	/* set dma channel mode, and reset address ff */

	if (slp->sl_scp.scp_direction == SCSI_LOW_READ)
		regv = DMA37MD_WRITE | DMA37MD_SINGLE | chan;
	else
		regv = DMA37MD_READ | DMA37MD_SINGLE | chan;

	bshw_dma_write_1(chp, DMA1_MODE, regv);
	bshw_dma_write_1(chp, DMA1_FFC, 0);

	/* send start address */
	waport = DMA1_CHN(chan);
	bshw_dma_write_1(chp, waport, (u_int) phys);
	bshw_dma_write_1(chp, waport, ((u_int) phys) >> 8);
	bshw_dma_write_1(chp, dmapageport[chan], ((u_int) phys) >> 16);

	/* send count */
	bshw_dma_write_1(chp, waport + 2, --nbytes);
	bshw_dma_write_1(chp, waport + 2, nbytes >> 8);

	/* vendor unique hook */
	if (bs->sc_hw->hw_dma_start)
		(*bs->sc_hw->hw_dma_start)(ct);

	bshw_dma_write_1(chp, DMA1_SMSK, chan);
	ct_cmdp_write_1(chp, CMDP_DMES);
}

static void
bshw_dmadone(ct)
	struct ct_softc *ct;
{
	struct bshw_softc *bs = ct->ct_hw;
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	bshw_dma_write_1(chp, DMA1_SMSK, (bs->sc_drq | DMA37SM_SET));
	ct_cmdp_write_1(chp, CMDP_DMER);

	/* vendor unique hook */
	if (bs->sc_hw->hw_dma_stop)
		(*bs->sc_hw->hw_dma_stop) (ct);

	/* flush cpu cache */
	(*bs->sc_dmasync_after) (ct);
}

/**********************************************
 * VENDOR UNIQUE DMA FUNCS
 **********************************************/
static int bshw_dma_init_sc98(struct ct_softc *);
static void bshw_dma_start_sc98(struct ct_softc *);
static void bshw_dma_stop_sc98(struct ct_softc *);
static int bshw_dma_init_texa(struct ct_softc *);
static void bshw_dma_start_elecom(struct ct_softc *);
static void bshw_dma_stop_elecom(struct ct_softc *);

static int
bshw_dma_init_texa(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	u_int8_t regval;

	if ((regval = ct_cr_read_1(chp, 0x37)) & 0x08)
		return 0;

	ct_cr_write_1(chp, 0x37, regval | 0x08);
	regval = ct_cr_read_1(chp, 0x3f);
	ct_cr_write_1(chp, 0x3f, regval | 0x08);
	return 1;
}

static int
bshw_dma_init_sc98(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	if (ct_cr_read_1(chp, 0x37) & 0x08)
		return 0;

	/* If your card is SC98 with bios ver 1.01 or 1.02 under no PCI */
	ct_cr_write_1(chp, 0x37, 0x1a);
	ct_cr_write_1(chp, 0x3f, 0x1a);
#if	0
	/* only valid for IO */
	ct_cr_write_1(chp, 0x40, 0xf4);
	ct_cr_write_1(chp, 0x41, 0x9);
	ct_cr_write_1(chp, 0x43, 0xff);
	ct_cr_write_1(chp, 0x46, 0x4e);

	ct_cr_write_1(chp, 0x48, 0xf4);
	ct_cr_write_1(chp, 0x49, 0x9);
	ct_cr_write_1(chp, 0x4b, 0xff);
	ct_cr_write_1(chp, 0x4e, 0x4e);
#endif
	return 1;
}

static void
bshw_dma_start_sc98(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	ct_cr_write_1(chp, 0x73, 0x32);
	ct_cr_write_1(chp, 0x74, 0x23);
}

static void
bshw_dma_stop_sc98(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;

	ct_cr_write_1(chp, 0x73, 0x43);
	ct_cr_write_1(chp, 0x74, 0x34);
}

static void
bshw_dma_start_elecom(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	u_int8_t tmp = ct_cr_read_1(chp, 0x4c);

	ct_cr_write_1(chp, 0x32, tmp & 0xdf);
}

static void
bshw_dma_stop_elecom(ct)
	struct ct_softc *ct;
{
	struct ct_bus_access_handle *chp = &ct->sc_ch;
	u_int8_t tmp = ct_cr_read_1(chp, 0x4c);

	ct_cr_write_1(chp, 0x32, tmp | 0x20);
}

static struct bshw bshw_generic = {
	BSHW_SYNC_RELOAD,

	0,

	NULL,
	NULL,
	NULL,
};

static struct bshw bshw_sc98 = {
	BSHW_DOUBLE_DMACHAN,

	0x60,

	bshw_dma_init_sc98,
	bshw_dma_start_sc98,
	bshw_dma_stop_sc98,
};

static struct bshw bshw_texa = {
	BSHW_DOUBLE_DMACHAN,

	0x60,

	bshw_dma_init_texa,
	NULL,
	NULL,
};

static struct bshw bshw_elecom = {
	0,

	0x38,

	NULL,
	bshw_dma_start_elecom,
	bshw_dma_stop_elecom,
};

static struct bshw bshw_lc_smit = {
	BSHW_SMFIFO | BSHW_DOUBLE_DMACHAN,

	0x60,

	NULL,
	NULL,
	NULL,
};

static struct bshw bshw_lha20X = {
	BSHW_DOUBLE_DMACHAN,

	0x60,

	NULL,
	NULL,
	NULL,
};

/* hw tabs */
static dvcfg_hw_t bshw_hwsel_array[] = {
/* 0x00 */	&bshw_generic,
/* 0x01 */	&bshw_sc98,
/* 0x02 */	&bshw_texa,
/* 0x03 */	&bshw_elecom,
/* 0x04 */	&bshw_lc_smit,
/* 0x05 */	&bshw_lha20X,
};

struct dvcfg_hwsel bshw_hwsel = {
	DVCFG_HWSEL_SZ(bshw_hwsel_array),
	bshw_hwsel_array
};
