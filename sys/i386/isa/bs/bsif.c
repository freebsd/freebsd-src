/*
 * Copyright (c) HONDA Naofumi, KATO Takenori, 1996.  All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer as
 *    the first lines of this file unmodified.
 * 2. Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * The name of the author may not be used to endorse or promote products
 * derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#if	0
/* WARNING: Any bug report must contain BS_REL_VERSION */
#define BS_REL_VERSION	"NetBSD1.2/030"	/* major jump */
#endif

#ifdef __NetBSD__
#include <dev/isa/bs/bsif.h>
#endif	/* __NetBSD__ */
#ifdef __FreeBSD__
#include "bs.h"
#include <i386/isa/bs/bsif.h>
#endif	/* __FreeBSD__ */

/**************************************************
 * DEVICE DECLARE
 **************************************************/
#ifdef __NetBSD__
int bsintr __P((void *));
static int bsprint __P((void *, char *));
static void bs_scsi_minphys __P((struct buf *));

struct cfdriver bs_cd = {
	NULL, "bs", DV_DULL
};

struct scsi_device bs_dev = {
	NULL,	/* Use default error handler */
	NULL,	/* have a queue, served by this */
	NULL,	/* have no async handler */
	NULL,	/* Use default 'done' routine */
};

struct scsi_adapter pc98texa55bs = {
	bs_scsi_cmd,
	bs_scsi_minphys,
	bs_target_open,
	0,
};
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
static int bsprobe __P((struct isa_device *));
static int bsattach __P((struct isa_device *));
static int bsprint __P((void *, char *));
static void bs_scsi_minphys __P((struct buf *));
static int bs_dmarangecheck __P((caddr_t, unsigned));

struct isa_driver bsdriver = {
	bsprobe,
	bsattach,
	"bs"
};

struct scsi_device bs_dev = {
	NULL,	/* Use default error handler */
	NULL,	/* have a queue, served by this */
	NULL,	/* have no async handler */
	NULL,	/* Use default 'done' routine */
	"bs",
	0, {0, 0}
};

u_int32_t
bs_adapter_info(unit)
	int unit;
{
	return (1);
}

struct scsi_adapter pc98texa55bs = {
	bs_scsi_cmd,
	bs_scsi_minphys,
	bs_target_open,
	0,
	bs_adapter_info,
	"bs", {0, 0}
};

static u_short pc98_irq_ball[16] = {
	IRQ0, IRQ1, IRQ2, IRQ3, IRQ4, IRQ5, IRQ6, IRQ7,
	IRQ8, IRQ9, IRQ10, IRQ11, IRQ12, IRQ13, IRQ14, IRQ15
};

static struct bs_softc *bscdata[NBS];
#endif	/* __FreeBSD__ */

/*****************************************************************
 * OS <=> BS INTERFACE
 *****************************************************************/
#ifdef __FreeBSD__
static int
bsprobe(dev)
	struct isa_device *dev;
#else	/* __NetBSD__ */
int
bsprobe(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
#endif	/* __NetBSD__ */
{
#ifdef __FreeBSD__
	struct bs_softc *bsc;
	int unit = dev->id_unit;
#else	/* __NetBSD__ */
	struct bs_softc *bsc = (void *) self;
	struct isa_attach_args *ia = aux;
	bus_chipset_tag_t bc = ia->ia_bc;
#endif	/* __NetBSD__ */
	u_int irq, drq;
	int i, rv = 0;

#ifdef __FreeBSD__
	if (unit >= NBS) {
		printf("bs%d: unit number too high\n", unit);
		return rv;
	}
	/*
	 * Allocate a storage for us
	 */
	if (bscdata[unit]) {
		printf("bs%d: memory already allocated\n", unit);
		return rv;
	}
	if (!(bsc = malloc(sizeof(struct bs_softc), M_TEMP, M_NOWAIT))) {
		printf("bs%d cannot malloc!\n", unit);
		return rv;
	}
	bzero(bsc, sizeof(struct bs_softc));
	bscdata[unit] = bsc;
	bsc->unit = unit;
#endif	/* __FreeBSD__ */

#ifdef __FreeBSD__
	bsc->sc_cfgflags = DVCFG_MINOR(dev->id_flags);
	bsc->sc_hw = DVCFG_HW(&bshw_hwsel, DVCFG_MAJOR(dev->id_flags));
#else	/* __NetBSD__ */
	bsc->sc_cfgflags = DVCFG_MINOR(ia->ia_cfgflags);
	bsc->sc_hw = DVCFG_HW(&bshw_hwsel, DVCFG_MAJOR(ia->ia_cfgflags));
#endif	/* __NetBSD__ */
	if (bsc->sc_hw == NULL)
		return rv;

#ifdef __FreeBSD__
	if ((bsc->sc_hw->hw_flags & BSHW_SMFIFO) &&
			(dev->id_maddr != (caddr_t)MADDRUNK))
		bsc->sm_vaddr = (u_int8_t *) dev->id_maddr;
	else
		bsc->sm_vaddr = (u_int8_t *) MADDRUNK;
#else	/* __NetBSD__ */
	if ((bsc->sc_hw->hw_flags & BSHW_SMFIFO) && (ia->ia_maddr != MADDRUNK))
	{
		ia->ia_maddr &= ~((NBPG * 2) - 1);
		ia->ia_maddr += NBPG;
		ia->ia_msize = NBPG;
		if (bus_mem_map(bc, ia->ia_maddr, NBPG, 0, &bsc->sc_memh))
			return 0;
		bsc->sm_vaddr = (u_int8_t *) bsc->sc_memh;	/* XXX */
	}
	else
	{
		bsc->sm_vaddr = (u_int8_t *) MADDRUNK;
		ia->ia_msize = 0;
	}
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
	sprintf(bsc->sc_dvname, "bs%d", unit);
#else	/* __NetBSD__ */
	strcpy(bsc->sc_dvname, bsc->sc_dev.dv_xname);
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
	if (dev->id_iobase == 0)
#else	/* __NetBSD__ */
	if (ia->ia_iobase == IOBASEUNK)
#endif	/* __NetBSD__ */
	{
		printf("%s: iobase not specified. Assume default port(0x%x)\n",
			bsc->sc_dvname, BSHW_DEFAULT_PORT);
#ifdef __FreeBSD__
		dev->id_iobase = BSHW_DEFAULT_PORT;
#else	/* __NetBSD__ */
		ia->ia_iobase = BSHW_DEFAULT_PORT;
#endif	/* __NetBSD__ */
	}

#ifdef __FreeBSD__
	bsc->sc_iobase = dev->id_iobase;
#else	/* __NetBSD__ */
	bsc->sc_iobase = ia->ia_iobase;
	bsc->sc_bc = bc;
	bsc->sc_delayioh = ia->ia_delayioh;
	if (bus_io_map(bsc->sc_bc, bsc->sc_iobase, BSHW_IOSZ, &bsc->sc_ioh))
		return rv;
#endif	/* __NetBSD__ */

#ifdef	__FreeBSD__
	irq = IRQUNK;
	drq = DRQUNK;
#else	/* __NetBSD__ */
	irq = ia->ia_irq;
	drq = ia->ia_drq;
#endif	/* __NetBSD__ */
	if (bshw_board_probe(bsc, &drq, &irq))
		goto bad;

#ifdef __FreeBSD__
	dev->id_irq = pc98_irq_ball[irq];
	dev->id_drq = (short)drq;
#else	/* __NetBSD__ */
	ia->ia_irq = irq;
	ia->ia_drq = drq;
#endif	/* __NetBSD__ */

	/* initialize host queue and target info */
	bs_hostque_init(bsc);
	for (i = 0; i < NTARGETS; i++)
		if (i != bsc->sc_hostid)
			bs_init_target_info(bsc, i);

	/* initialize ccb queue */
	bs_init_ccbque(BS_MAX_CCB);

#ifdef __NetBSD__
	/* init port data */
	ia->ia_iosize = BSHW_IOSZ;
#endif	/* __NetBSD__ */

	/* scsi bus reset and restart */
	bsc->sc_hstate = BSC_BOOTUP;
	bsc->sc_retry = RETRIES;
	bsc->sc_wc = delaycount * 250;	/* about 1 sec */
	bs_reset_nexus(bsc);

#ifdef __FreeBSD__
	return BSHW_IOSZ;
bad:
	return rv;
#else	/* __NetBSD__ */
	rv = 1;
bad:
	bus_io_unmap(bsc->sc_bc, bsc->sc_ioh, BSHW_IOSZ);
	return rv;
#endif	/* __NetBSD__ */
}

static int
bsprint(aux, name)
	void *aux;
	char *name;
{

	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}

#ifdef __FreeBSD__
static int
bsattach(dev)
	struct isa_device *dev;
#else	/* __NetBSD__ */
void
bsattach(parent, self, aux)
	struct device *parent;
	struct device *self;
	void *aux;
#endif	/* __NetBSD__ */
{
#ifdef __FreeBSD__
	int unit = dev->id_unit;
	struct bs_softc *bsc = bscdata[unit];
	struct scsibus_data *scbus;
#else	/* __NetBSD__ */
	struct bs_softc *bsc = (void *) self;
	struct isa_attach_args *ia = aux;

	printf("\n");
#endif	/* __NetBSD__ */

#ifdef	__NetBSD__
	bsc->sc_iobase = ia->ia_iobase;
	bsc->sc_bc = ia->ia_bc;
	bsc->sc_delayioh = ia->ia_delayioh;
	if (bus_io_map(bsc->sc_bc, bsc->sc_iobase, BSHW_IOSZ, &bsc->sc_ioh))
		panic("%s: bus io map failed\n", bsc->sc_dev.dv_xname);
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
	bsc->sc_link.adapter_unit = unit;
	bsc->sc_link.adapter_targ = bsc->sc_hostid;
	bsc->sc_link.flags = SDEV_BOUNCE;
	bsc->sc_link.opennings = XSMAX;
#else	/* __NetBSD__ */
	bsc->sc_link.adapter_target = bsc->sc_hostid;
	bsc->sc_link.openings = XSMAX;
#endif	/* __NetBSD__ */
	bsc->sc_link.adapter_softc = bsc;
	bsc->sc_link.adapter = &pc98texa55bs;
	bsc->sc_link.device = &bs_dev;

#ifdef __FreeBSD__
	/*
	 * Prepare the scsibus_data area for the upperlevel
	 * scsi code.
	 */
	scbus = scsi_alloc_bus();
	if (!scbus)
		return 0;
	scbus->adapter_link = &bsc->sc_link;
	/*
	 * ask the adapter what subunits are present
	 */
	scsi_attachdevs(scbus);
#else	/* __NetBSD__ */
	bsc->sc_ih = isa_intr_establish(ia->ia_ic, ia->ia_irq, IST_EDGE,
	     IPL_BIO, bsintr, bsc);
	config_found(self, &bsc->sc_link, bsprint);
#endif	/* __NetBSD__ */
	bs_start_timeout(bsc);
#ifdef __FreeBSD__
	return 1;
#endif	/* __FreeBSD__ */
}

#ifdef __NetBSD__
int
bsintr(arg)
	void *arg;
{

	return bs_sequencer((struct bs_softc *)arg);
}
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
void
bsintr(unit)
	int unit;
{
	(void)bs_sequencer(bscdata[unit]);
}
#endif	/* __FreeBSD__ */

/*****************************************************************
 * JULIAN SCSI <=> BS INTERFACE
 *****************************************************************/
static void
bs_scsi_minphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > BSDMABUFSIZ)
		bp->b_bcount = BSDMABUFSIZ;
	minphys(bp);
}

XSBS_INT32T
bs_target_open(sc, cf)
	struct scsi_link *sc;
	struct cfdata *cf;
{
	u_int target = sc->target;
	struct bs_softc *bsc = (struct bs_softc *) (sc->adapter_softc);
	struct targ_info *ti = bsc->sc_ti[target];
	u_int flags;

	if ((bsc->sc_openf & (1 << target)) == 0)
		return ENODEV;

	if ((flags = cf->cf_flags) == 0)
		flags = BS_SCSI_DEFCFG;

	bs_setup_ctrl(ti, (u_int)sc->quirks, flags);
	return 0;
}

/*****************************************************************
 * BS MEMORY ALLOCATION INTERFACE
 *****************************************************************/
#ifdef __NetBSD__
void
bs_alloc_buf(ti)
	struct targ_info *ti;
{
	extern int cold;
	caddr_t addr, physaddr;
	u_int pages;

	/* XXX:
	 * strategy change!
	 * A) total memory >= 16M at boot: MAXBSIZE * 7 = 112k.
	 * B) others:  4K * 7 = 28 K.
	 */
	if (get_sysinfo(SYSINFO_MEMLEVEL) == MEM_LEVEL1 && cold != 0)
		pages = 4;
	else
		pages = 1;

	ti->bounce_size = NBPG * pages;
	if ((addr = alloc_bounce_buffer(ti->bounce_size)) == NULL)
	{
		ti->bounce_size = NBPG;
		if ((addr = malloc(NBPG, M_DEVBUF, M_NOWAIT)) == NULL)
			goto bad;
	}

	physaddr = (caddr_t) vtophys(addr);
	if ((u_int) physaddr >= RAM_END)
	{
		/* XXX: mem from malloc only! */
		free(addr, M_DEVBUF);
		goto bad;
	}

	ti->bounce_addr = (u_int8_t *) addr;
	ti->bounce_phys = (u_int8_t *) physaddr;
	return;

bad:
	bs_printf(ti, "bs_alloc_buf", "no phys bounce buffer");
	printf("WARNING: this target is dislocated\n");
}
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
static int bs_dmarangecheck(caddr_t va, unsigned length)
{
	vm_offset_t phys, priorpage = 0, endva;

	endva = (vm_offset_t)round_page(va+length);
	for (; va < (caddr_t)endva; va += PAGE_SIZE) {
		phys = trunc_page(pmap_extract(pmap_kernel(), (vm_offset_t)va));
		if (phys == 0)
			panic("bs_dmarangecheck: no physical page present");
		if (phys >= RAM_END)
			return 1;
		if (priorpage) {
			if (priorpage + PAGE_SIZE != phys)
				return 1;
		}
		priorpage = phys;
	}
	return 0;
}

void
bs_alloc_buf(ti)
	struct targ_info *ti;
{
	caddr_t addr, physaddr;

#if BS_BOUNCE_SIZE != 0
	ti->bounce_size = BS_BOUNCE_SIZE;
#else
	ti->bounce_size = BSHW_NBPG;
#endif
	/* Try malloc() first.  It works better if it works. */
	addr = malloc(ti->bounce_size, M_DEVBUF, M_NOWAIT);
	if (addr != NULL) {
		if (bs_dmarangecheck(addr, ti->bounce_size) == 0) {
			physaddr = (caddr_t) vtophys(addr);
			ti->bounce_addr = (u_int8_t *) addr;
			ti->bounce_phys = (u_int8_t *) physaddr;
			return;
		}
		free(buf, M_DEVBUF);
	}
	addr = contigmalloc(ti->bounce_size, M_DEVBUF, M_NOWAIT,
						0ul, RAM_END, 1ul, 0x10000ul);
	if (addr == NULL)
		goto bad;

	physaddr = (caddr_t) vtophys(addr);
	if ((u_int) physaddr >= RAM_END)
	{
		/* XXX:
		 * must free memory !
		 */
		goto bad;
	}

	ti->bounce_addr = (u_int8_t *) addr;
	ti->bounce_phys = (u_int8_t *) physaddr;
	return;

bad:
	bs_printf(ti, "bs_alloc_buf", "no phys bounce buffer");
	printf("WARNING: this target is dislocated\n");
}
#endif	/* __FreeBSD__ */
