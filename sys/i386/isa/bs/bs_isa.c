/*	$NecBSD: bs_isa.c,v 1.3 1997/10/31 17:43:35 honda Exp $	*/
/*	$NetBSD$	*/
/*
 * [NetBSD for NEC PC98 series]
 *  Copyright (c) 1995, 1996 NetBSD/pc98 porting staff.
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

#include <i386/Cbus/dev/bs/bsif.h>

static int bs_isa_probe __P((struct device *, void *, void *));
static void bs_isa_attach __P((struct device *, struct device *, void *));
static void bs_args_copy 
	__P((struct bs_softc *, struct isa_attach_args *, struct bshw *));

struct cfattach bs_isa_ca = {
	sizeof(struct bs_softc), bs_isa_probe, bs_isa_attach
};

static void
bs_args_copy(bsc, ia, hw)
	struct bs_softc *bsc;
	struct isa_attach_args *ia;
	struct bshw *hw;
{
	
	bsc->sc_hw = hw;
	bsc->sc_iot = ia->ia_iot;
	bsc->sc_memt = ia->ia_memt;
	bsc->sc_dmat = ia->ia_dmat;
	bsc->sc_delaybah = ia->ia_delaybah;		/* should be die */
	bsc->sc_iobase = ia->ia_iobase;
	if (ia->ia_maddr != MADDRUNK)
		bsc->sm_offset = BSHW_SMITFIFO_OFFSET;
	else
		bsc->sm_offset = 0;

	bsc->sc_cfgflags = DVCFG_MINOR(ia->ia_cfgflags);
	strcpy(bsc->sc_dvname, bsc->sc_dev.dv_xname);
}

static int
bs_isa_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct bs_softc *bsc = (void *) match;
	struct isa_attach_args *ia = aux;
	bus_space_handle_t ioh, memh = NULL;
	bus_space_tag_t iot, memt;
	struct bshw *hw;
	u_int irq, drq;
	int rv = 0;

	hw = DVCFG_HW(&bshw_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	if (hw == NULL)
		return rv;

	iot = ia->ia_iot;
	memt = ia->ia_memt;
	if (ia->ia_iobase == IOBASEUNK)
	{
		printf("%s: iobase not specified. Assume default port(0x%x)\n",
			bsc->sc_dvname, BSHW_DEFAULT_PORT);
		ia->ia_iobase = BSHW_DEFAULT_PORT;
	}

	if (bus_space_map(iot, ia->ia_iobase, BSHW_IOSZ, 0, &ioh))
		return rv;

	if ((hw->hw_flags & BSHW_SMFIFO) != 0 && (ia->ia_maddr != MADDRUNK))
	{
		ia->ia_maddr = (ia->ia_maddr & (~((NBPG * 2) - 1))) + NBPG;
		ia->ia_msize = NBPG;
		if (bus_space_map(memt, ia->ia_maddr, NBPG, 0, &memh))
		{
			bus_space_unmap(iot, ioh, BSHW_IOSZ);
			return 0;
		}
	}
	else
		ia->ia_maddr = MADDRUNK;

	irq = IRQUNK;
	drq = DRQUNK;
	bsc->sc_ioh = ioh;
	bsc->sc_memh = memh;
	bs_args_copy(bsc, ia, hw);
	if (bshw_board_probe(bsc, &drq, &irq))
		goto bad;

	ia->ia_irq = irq;
	ia->ia_drq = drq;
	ia->ia_iosize = BSHW_IOSZ;
	rv = 1;

bad:
	bus_space_unmap(iot, bsc->sc_ioh, BSHW_IOSZ);
	if (ia->ia_maddr != MADDRUNK)
		bus_space_unmap(memt, bsc->sc_memh, ia->ia_msize);
	return rv;
}

static void
bs_isa_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	extern struct scsi_adapter pc98texa55bs;
	extern struct scsi_device bs_dev;
	struct bs_softc *bsc = (void *) self;
	struct isa_attach_args *ia = aux;
	bus_space_tag_t iot, memt;
	struct bshw *hw;
	int i;

	printf("\n");

	hw = DVCFG_HW(&bshw_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
	iot = ia->ia_iot;
	memt = ia->ia_memt;

	if (bus_space_map(iot, ia->ia_iobase, BSHW_IOSZ, 0, &bsc->sc_ioh))
		panic("%s: bus io map failed\n", bsc->sc_dev.dv_xname);
	
	if (ia->ia_maddr != MADDRUNK &&
	    bus_space_map(memt, ia->ia_maddr, NBPG, 0, &bsc->sc_memh))
		panic("%s: bus mem map failed\n", bsc->sc_dev.dv_xname);

	if (isa_dmamap_create(NULL, ia->ia_drq, MAXBSIZE, BUS_DMA_NOWAIT))
	{
		printf("%s: can't set up ISA DMA map\n", bsc->sc_dev.dv_xname);
		return;
	}

	/* system initialize */
	bs_args_copy(bsc, ia, hw);
	bs_hostque_init(bsc);
	for (i = 0; i < NTARGETS; i++)
	{
		if (i != bsc->sc_hostid)
			bs_init_target_info(bsc, i);
	}

	bs_init_ccbque(BS_MAX_CCB);
	bsc->sc_hstate = BSC_BOOTUP;
	bsc->sc_retry = RETRIES;
	bsc->sc_wc = delaycount * 250;
	bs_reset_nexus(bsc);

	/* link upper layer */
	bsc->sc_link.adapter_target = bsc->sc_hostid;
	bsc->sc_link.openings = XSMAX;
	bsc->sc_link.max_target = 7;
	bsc->sc_link.channel = SCSI_CHANNEL_ONLY_ONE;
	bsc->sc_link.adapter_softc = bsc;
	bsc->sc_link.adapter = &pc98texa55bs;
	bsc->sc_link.device = &bs_dev;

	config_found(self, &bsc->sc_link, bsprint);

	bsc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	     				IPL_BIO, bsintr, bsc);
	bs_start_timeout(bsc);
}
