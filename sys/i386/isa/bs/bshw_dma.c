/* $FreeBSD$ */
/*	$NecBSD: bshw_dma.c,v 1.3 1997/07/26 06:03:16 honda Exp $	*/
/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1994, 1995, 1996 NetBSD/pc98 porting staff.
 *  All rights reserved.
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
/*
 * Copyright (c) 1994, 1995, 1996 Naofumi HONDA.  All rights reserved.
 */

/*********************************************************
 * static declare.
 *********************************************************/
static void bshw_dmastart(struct bs_softc *);
static void bshw_dmadone(struct bs_softc *);

/**********************************************
 * UPPER INTERFACE FUNCS (all funcs exported)
 **********************************************/
void
bshw_dmaabort(bsc, ti)
	struct bs_softc *bsc;
	struct targ_info *ti;
{

	bshw_dmadone(bsc);
	bsc->sc_p.seglen = 0;
	bshw_set_count(bsc, 0);

	if (ti == NULL)
	{
		int i;
		struct targ_info *tmpti;

		for (i = 0; i < NTARGETS; i++)
			if ((tmpti = bsc->sc_ti[i]) != NULL)
				tmpti->ti_scsp.seglen = 0;
	}
	else
		ti->ti_scsp.seglen = 0;
}

/* DMA TRANSFER */
void
bs_dma_xfer(ti, direction)
	struct targ_info *ti;
	u_int direction;
{
	vm_offset_t va, endva, phys, nphys;
	struct bs_softc *bsc = ti->ti_bsc;
	struct sc_p *sp = &bsc->sc_p;

	bsc->sc_dmadir = direction;
	bshw_set_dma_trans(bsc, ti->ti_cfgflags);

	if (sp->seglen == 0)
	{
		phys = vtophys((vm_offset_t) sp->data);
		if (phys >= RAM_END)
		{
			/* setup segaddr */
			sp->segaddr = ti->bounce_phys;
			/* setup seglen */
			sp->seglen = sp->datalen;
			if (sp->seglen > ti->bounce_size)
				sp->seglen = ti->bounce_size;
			/* setup bufp */
			sp->bufp = ti->bounce_addr;
			if (bsc->sc_dmadir != BSHW_READ)
				bcopy(sp->data, sp->bufp, sp->seglen);
#ifdef	BS_STATICS
			bs_bounce_used[ti->ti_id]++;
#endif	/* BS_STATICS */
		}
		else
		{
			/* setup segaddr */
			sp->segaddr = (u_int8_t *) phys;
			/* setup seglen */
			endva = (vm_offset_t)round_page((unsigned long)(sp->data + sp->datalen));
			for (va = (vm_offset_t) sp->data; ; phys = nphys)
			{
				if ((va += BSHW_NBPG) >= endva)
				{
					sp->seglen = sp->datalen;
					break;
				}

				nphys = vtophys(va);
				if (phys + BSHW_NBPG != nphys || nphys >= RAM_END)
				{
					sp->seglen =
					    (u_int8_t *) trunc_page(va) - sp->data;
					break;
				}
			}
			/* setup bufp */
			sp->bufp = NULL;
		}
	}

	bshw_dmastart(bsc);
	bshw_set_count(bsc, sp->seglen);
}

void
bs_dma_xfer_end(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	struct sc_p *sp = &bsc->sc_p;
	u_int count, transbytes;

	bshw_dmadone(bsc);
	if (ti->ti_phase == DATAPHASE)
	{
		count = bshw_get_count(bsc);
		if (count < (u_int) sp->seglen)
		{
			transbytes = sp->seglen - count;
			if (sp->bufp)
			{
				if (bsc->sc_dmadir == BSHW_READ)
					bcopy(sp->bufp, sp->data, transbytes);
				sp->bufp += transbytes;
			}
			sp->seglen = count;
			sp->segaddr += transbytes;
			sp->data += transbytes;
			sp->datalen -= transbytes;
			return;
		}
		else if (count == (u_int) sp->seglen)
		{
			return;
		}

		bs_printf(ti, "xfer_end", "strange count");
		printf("port data %x seglen %x\n", count, sp->seglen);
	}
	else
		bs_printf(ti, "xfer_end", "extra dma interrupt");

	ti->ti_error |= BSDMAABNORMAL;
	sp->seglen = ti->ti_scsp.seglen = 0;	/* XXX */
}

/**********************************************
 * GENERIC DMA FUNCS
 **********************************************/
static u_int8_t dmapageport[4] = { 0x27, 0x21, 0x23, 0x25 };

/* common dma settings */
#undef	DMA1_SMSK
#define DMA1_SMSK	(0x15)
#undef	DMA1_MODE
#define DMA1_MODE	(0x17)
#undef	DMA1_FFC
#define DMA1_FFC	(0x19)
#undef	DMA37SM_SET
#define DMA37SM_SET	0x04
#undef	DMA1_CHN
#define DMA1_CHN(c)	(0x01 + ((c) << 2))

static void
bshw_dmastart(bsc)
	struct bs_softc *bsc;
{
	int chan = bsc->sc_dmachan;
	int waport;
	u_int8_t *phys = bsc->sc_p.segaddr;
	u_int nbytes = bsc->sc_p.seglen;

	/*
	 * Program one of DMA channels 0..3. These are
	 * byte mode channels.
	 */
	/* set dma channel mode, and reset address ff */
#ifdef __FreeBSD__
	if (need_pre_dma_flush)
		wbinvd();
#else	/* NetBSD/pc98 */
	if (bsc->sc_dmadir & BSHW_READ)
		cpu_cf_preRead(curcpu);
	else
		cpu_cf_preWrite(curcpu);
#endif

	if (bsc->sc_dmadir & BSHW_READ)
		outb(DMA1_MODE, DMA37MD_SINGLE | DMA37MD_WRITE | chan);
	else
		outb(DMA1_MODE, DMA37MD_SINGLE | DMA37MD_READ | chan);
	outb(DMA1_FFC, 0);

	/* send start address */
	waport = DMA1_CHN(chan);
	outb(waport, (u_int) phys);
	outb(waport, ((u_int) phys) >> 8);
	outb(dmapageport[chan], ((u_int) phys) >> 16);

	/* send count */
	outb(waport + 2, --nbytes);
	outb(waport + 2, nbytes >> 8);

	/* vendor unique hook */
	if (bsc->sc_hw->dma_start)
		(*bsc->sc_hw->dma_start)(bsc);

	outb(DMA1_SMSK, chan);
	BUS_IOW(cmd_port, CMDP_DMES);

	bsc->sc_flags |= BSDMASTART;
}

static void
bshw_dmadone(bsc)
	struct bs_softc *bsc;
{

	outb(DMA1_SMSK, (bsc->sc_dmachan | DMA37SM_SET));
	BUS_IOW(cmd_port, CMDP_DMER);

	/* vendor unique hook */
	if (bsc->sc_hw->dma_stop)
		(*bsc->sc_hw->dma_stop)(bsc);

#ifdef __FreeBSD__
	if (need_post_dma_flush)
		invd();
#else
	if (bsc->sc_dmadir & BSHW_READ)
		cpu_cf_postRead(curcpu);
	else
		cpu_cf_postWrite(curcpu);
#endif

	bsc->sc_flags &= (~BSDMASTART);
}

/**********************************************
 * VENDOR UNIQUE DMA FUNCS
 **********************************************/
static int
bshw_dma_init_texa(bsc)
	struct bs_softc *bsc;
{
	u_int8_t regval;

	if ((regval = read_wd33c93(bsc, 0x37)) & 0x08)
		return 0;

	write_wd33c93(bsc, 0x37, regval | 0x08);
	regval = read_wd33c93(bsc, 0x3f);
	write_wd33c93(bsc, 0x3f, regval | 0x08);
	return 1;
}

static int
bshw_dma_init_sc98(bsc)
	struct bs_softc *bsc;
{

	if (read_wd33c93(bsc, 0x37) & 0x08)
		return 0;

	/* If your card is SC98 with bios ver 1.01 or 1.02 under no PCI */
	write_wd33c93(bsc, 0x37, 0x1a);
	write_wd33c93(bsc, 0x3f, 0x1a);
#if	0
	/* only valid for IO */
	write_wd33c93(bsc, 0x40, 0xf4);
	write_wd33c93(bsc, 0x41, 0x9);
	write_wd33c93(bsc, 0x43, 0xff);
	write_wd33c93(bsc, 0x46, 0x4e);

	write_wd33c93(bsc, 0x48, 0xf4);
	write_wd33c93(bsc, 0x49, 0x9);
	write_wd33c93(bsc, 0x4b, 0xff);
	write_wd33c93(bsc, 0x4e, 0x4e);
#endif
	return 1;
}

static void
bshw_dma_start_sc98(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, 0x73, 0x32);
	write_wd33c93(bsc, 0x74, 0x23);
}

static void
bshw_dma_stop_sc98(bsc)
	struct bs_softc *bsc;
{

	write_wd33c93(bsc, 0x73, 0x43);
	write_wd33c93(bsc, 0x74, 0x34);
}

static void
bshw_dma_start_elecom(bsc)
	struct bs_softc *bsc;
{
	u_int8_t tmp = read_wd33c93(bsc, 0x4c);

	write_wd33c93(bsc, 0x32, tmp & 0xdf);
}

static void
bshw_dma_stop_elecom(bsc)
	struct bs_softc *bsc;
{
	u_int8_t tmp = read_wd33c93(bsc, 0x4c);

	write_wd33c93(bsc, 0x32, tmp | 0x20);
}
