/*	$NecBSD: bsif.c,v 1.6 1997/10/31 17:43:40 honda Exp $	*/
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
 *
 * $FreeBSD: src/sys/i386/isa/bs/bsif.c,v 1.10.2.1 2000/08/24 08:06:08 kato Exp $
 */

#if	0
/* WARNING: Any bug report must contain BS_REL_VERSION */
#define BS_REL_VERSION	"NetBSD1.2/030"	/* major jump */
#endif

#ifdef __NetBSD__
#include <i386/Cbus/dev/bs/bsif.h>
#endif	/* __NetBSD__ */
#ifdef __FreeBSD__
#include "opt_bs.h"
#include "opt_pc98.h"
#include "bs.h"
#include <i386/isa/bs/bsif.h>
#endif	/* __FreeBSD__ */

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_debug.h>

#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

/**************************************************
 * DEVICE DECLARE
 **************************************************/
#ifdef __NetBSD__
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
static void bs_poll(struct cam_sim *sim);
static int bsattach(struct isa_device *);
static ointhand2_t bsintr;
static int bs_dmarangecheck __P((caddr_t, unsigned));

struct isa_driver bsdriver = {
	bsprobe,
	bsattach,
	"bs"
};
#if 0
struct scsi_device bs_dev = {
	NULL,	/* Use default error handler */
	NULL,	/* have a queue, served by this */
	NULL,	/* have no async handler */
	NULL,	/* Use default 'done' routine */
	"bs",
	0, {0, 0}
};
#endif
u_int32_t
bs_adapter_info(unit)
	int unit;
{
	return (1);
}
#if 0
static struct scsi_adapter pc98texa55bs = {
	bs_scsi_cmd,
	bs_scsi_minphys,
	bs_target_open,
	0,
	bs_adapter_info,
	"bs", {0, 0}
};
#endif
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
{
	struct bs_softc *bsc;
	int unit = dev->id_unit;
	u_int irq, drq;
	int i, rv = 0;

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
	callout_handle_init(&bsc->timeout_ch);
	bscdata[unit] = bsc;
	bsc->unit = unit;

	bsc->sc_cfgflags = DVCFG_MINOR(dev->id_flags);
	bsc->sc_hw = DVCFG_HW(&bshw_hwsel, DVCFG_MAJOR(dev->id_flags));
	if (bsc->sc_hw == NULL)
		return rv;

	if ((bsc->sc_hw->hw_flags & BSHW_SMFIFO) &&
			(dev->id_maddr != (caddr_t)MADDRUNK))
		bsc->sm_offset = (u_long) dev->id_maddr;
	else
		bsc->sm_offset = (u_long) 0;

	snprintf(bsc->sc_dvname, sizeof(bsc->sc_dvname), "bs%d", unit);

	if (dev->id_iobase == 0)
	{
		printf("%s: iobase not specified. Assume default port(0x%x)\n",
			bsc->sc_dvname, BSHW_DEFAULT_PORT);
		dev->id_iobase = BSHW_DEFAULT_PORT;
	}

	bsc->sc_iobase = dev->id_iobase;
	irq = IRQUNK;
	drq = DRQUNK;
	if (bshw_board_probe(bsc, &drq, &irq))
		goto bad;

	dev->id_irq = pc98_irq_ball[irq];
	dev->id_drq = (short)drq;

	/* initialize host queue and target info */
	bs_hostque_init(bsc);
	for (i = 0; i < NTARGETS; i++)
		if (i != bsc->sc_hostid)
			bs_init_target_info(bsc, i);

	/* initialize ccb queue */
	bs_init_ccbque(BS_MAX_CCB);

	/* scsi bus reset and restart */
	bsc->sc_hstate = BSC_BOOTUP;
	bsc->sc_retry = RETRIES;
	bsc->sc_wc = delaycount * 250;	/* about 1 sec */
	bs_reset_nexus(bsc);

	return BSHW_IOSZ;
bad:
	return rv;
}
#endif	/* __FreeBSD__ */

#ifdef __NetBSD__
int
bsprint(aux, name)
	void *aux;
	const char *name;
{

	if (name != NULL)
		printf("%s: scsibus ", name);
	return UNCONF;
}
#endif

#ifdef __FreeBSD__
static void
bs_poll(struct cam_sim *sim)
{
	bs_sequencer(cam_sim_softc(sim));
}

static int
bsattach(dev)
	struct isa_device *dev;
{
	int unit = dev->id_unit;
	struct bs_softc *bsc = bscdata[unit];
	struct cam_devq *devq;

	dev->id_ointr = bsintr;

	/*
	 * CAM support  HN2  MAX_START, MAX_TAGS xxxx
	 */
	devq = cam_simq_alloc(256/*MAX_START*/);
	if (devq == NULL)
		return 0;

	bsc->sim = cam_sim_alloc(bs_scsi_cmd, bs_poll, "bs",
				 bsc, unit, 1, 32/*MAX_TAGS*/, devq);
	if (bsc->sim == NULL) {
		cam_simq_free(devq);
		return 0;
	}

	if (xpt_bus_register(bsc->sim, 0) != CAM_SUCCESS) {
		free(bsc->sim, M_DEVBUF);
		return 0;
	}
	
	if (xpt_create_path(&bsc->path, /*periph*/NULL,
			    cam_sim_path(bsc->sim), CAM_TARGET_WILDCARD,
			    CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(bsc->sim));
		cam_sim_free(bsc->sim, /*free_simq*/TRUE);
		free(bsc->sim, M_DEVBUF);
		return 0;
	}
	bs_start_timeout(bsc);
	return 1;
}
#endif	/* __FreeBSD__ */

#ifdef __NetBSD__
int
bsintr(arg)
	void *arg;
{

	return bs_sequencer((struct bs_softc *)arg);
}
#endif	/* __NetBSD__ */

#ifdef __FreeBSD__
static void
bsintr(unit)
	int unit;
{
	(void)bs_sequencer(bscdata[unit]);
}
#endif	/* __FreeBSD__ */

/*****************************************************************
 * JULIAN SCSI <=> BS INTERFACE
 *****************************************************************/
#ifndef __FreeBSD__
static void
bs_scsi_minphys(bp)
	struct buf *bp;
{

	if (bp->b_bcount > BSDMABUFSIZ)
		bp->b_bcount = BSDMABUFSIZ;
	minphys(bp);
}
#endif
#if 0
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
#endif
/*****************************************************************
 * BS MEMORY ALLOCATION INTERFACE
 *****************************************************************/
#ifdef __NetBSD__
void
bs_alloc_buf(ti)
	struct targ_info *ti;
{
	struct bs_softc *bsc = ti->ti_bsc;
	caddr_t addr, physaddr;
	bus_dma_segment_t seg;
	int rseg, error;
	u_int pages;
	extern int cold;

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

	addr = NULL;
	error = bus_dmamem_alloc(bsc->sc_dmat, ti->bounce_size, NBPG, 0,
				 &seg, 1, &rseg, BUS_DMA_NOWAIT);
	if (rseg == 1 && error == 0)
		error = bus_dmamem_map(bsc->sc_dmat, &seg, rseg,
				       ti->bounce_size, &addr, BUS_DMA_NOWAIT);
	if (rseg != 1 || error != 0)
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

	endva = (vm_offset_t)round_page((unsigned long)(va+length));
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
		free(addr, M_DEVBUF);
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
