/*-
 * PCI specific probe and attach routines for Qlogic ISP SCSI adapters.
 * FreeBSD Version.
 *
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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <dev/ofw/ofw_bus.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sparc64/sbus/sbusvar.h>

#include <dev/isp/isp_freebsd.h>

static uint16_t isp_sbus_rd_reg(ispsoftc_t *, int);
static void isp_sbus_wr_reg(ispsoftc_t *, int, uint16_t);
static int
isp_sbus_rd_isr(ispsoftc_t *, uint16_t *, uint16_t *, uint16_t *);
static int isp_sbus_mbxdma(ispsoftc_t *);
static int
isp_sbus_dmasetup(ispsoftc_t *, XS_T *, ispreq_t *, uint16_t *, uint16_t);
static void
isp_sbus_dmateardown(ispsoftc_t *, XS_T *, uint16_t);

static void isp_sbus_reset1(ispsoftc_t *);
static void isp_sbus_dumpregs(ispsoftc_t *, const char *);

static struct ispmdvec mdvec = {
	isp_sbus_rd_isr,
	isp_sbus_rd_reg,
	isp_sbus_wr_reg,
	isp_sbus_mbxdma,
	isp_sbus_dmasetup,
	isp_sbus_dmateardown,
	NULL,
	isp_sbus_reset1,
	isp_sbus_dumpregs,
	NULL,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};

static int isp_sbus_probe (device_t);
static int isp_sbus_attach (device_t);


struct isp_sbussoftc {
	ispsoftc_t			sbus_isp;
	device_t			sbus_dev;
	struct resource *		sbus_reg;
	bus_space_tag_t			sbus_st;
	bus_space_handle_t		sbus_sh;
	void *				ih;
	int16_t				sbus_poff[_NREG_BLKS];
	bus_dma_tag_t			dmat;
	bus_dmamap_t			*dmaps;
	sdparam				sbus_param;
	struct ispmdvec			sbus_mdvec;
	struct resource *		sbus_ires;
};

extern ispfwfunc *isp_get_firmware_p;

static device_method_t isp_sbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		isp_sbus_probe),
	DEVMETHOD(device_attach,	isp_sbus_attach),
	{ 0, 0 }
};
static void isp_sbus_intr(void *);

static driver_t isp_sbus_driver = {
	"isp", isp_sbus_methods, sizeof (struct isp_sbussoftc)
};
static devclass_t isp_devclass;
DRIVER_MODULE(isp, sbus, isp_sbus_driver, isp_devclass, 0, 0);

static int
isp_sbus_probe(device_t dev)
{
	int found = 0;
	const char *name = ofw_bus_get_name(dev);
	if (strcmp(name, "SUNW,isp") == 0 ||
	    strcmp(name, "QLGC,isp") == 0 ||
	    strcmp(name, "ptisp") == 0 ||
	    strcmp(name, "PTI,ptisp") == 0) {
		found++;
	}
	if (!found)
		return (ENXIO);
	
	if (isp_announced == 0 && bootverbose) {
		printf("Qlogic ISP Driver, FreeBSD Version %d.%d, "
		    "Core Version %d.%d\n",
		    ISP_PLATFORM_VERSION_MAJOR, ISP_PLATFORM_VERSION_MINOR,
		    ISP_CORE_VERSION_MAJOR, ISP_CORE_VERSION_MINOR);
		isp_announced++;
	}
	return (0);
}

static int
isp_sbus_attach(device_t dev)
{
	struct resource *regs;
	int tval, iqd, isp_debug, role, rid, ispburst;
	struct isp_sbussoftc *sbs;
	ispsoftc_t *isp = NULL;
	int locksetup = 0;

	/*
	 * Figure out if we're supposed to skip this one.
	 * If we are, we actually go to ISP_ROLE_NONE.
	 */

	tval = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "disable", &tval) == 0 && tval) {
		device_printf(dev, "device is disabled\n");
		/* but return 0 so the !$)$)*!$*) unit isn't reused */
		return (0);
	}
	
	role = 0;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
	    "role", &role) == 0 &&
	    ((role & ~(ISP_ROLE_INITIATOR|ISP_ROLE_TARGET)) == 0)) {
		device_printf(dev, "setting role to 0x%x\n", role);
	} else {
#ifdef	ISP_TARGET_MODE
		role = ISP_ROLE_INITIATOR|ISP_ROLE_TARGET;
#else
		role = ISP_DEFAULT_ROLES;
#endif
	}

	sbs = malloc(sizeof (*sbs), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (sbs == NULL) {
		device_printf(dev, "cannot allocate softc\n");
		return (ENOMEM);
	}

	regs = NULL;
	iqd = 0;

	rid = 0;
	regs =
	    bus_alloc_resource_any(dev, SYS_RES_MEMORY, &rid, RF_ACTIVE);
	if (regs == 0) {
		device_printf(dev, "unable to map registers\n");
		goto bad;
	}
	sbs->sbus_dev = dev;
	sbs->sbus_reg = regs;
	sbs->sbus_st = rman_get_bustag(regs);
	sbs->sbus_sh = rman_get_bushandle(regs);
	sbs->sbus_mdvec = mdvec;

	sbs->sbus_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	sbs->sbus_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = SBUS_MBOX_REGS_OFF;
	sbs->sbus_poff[SXP_BLOCK >> _BLK_REG_SHFT] = SBUS_SXP_REGS_OFF;
	sbs->sbus_poff[RISC_BLOCK >> _BLK_REG_SHFT] = SBUS_RISC_REGS_OFF;
	sbs->sbus_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;
	isp = &sbs->sbus_isp;
	isp->isp_mdvec = &sbs->sbus_mdvec;
	isp->isp_bustype = ISP_BT_SBUS;
	isp->isp_type = ISP_HA_SCSI_UNKNOWN;
	isp->isp_param = &sbs->sbus_param;
	isp->isp_revision = 0;	/* XXX */
	isp->isp_role = role;
	isp->isp_dev = dev;

	/*
	 * Get the clock frequency and convert it from HZ to MHz,
	 * rounding up. This defaults to 25MHz if there isn't a
	 * device specific one in the OFW device tree.
	 */
	sbs->sbus_mdvec.dv_clock = (sbus_get_clockfreq(dev) + 500000)/1000000;

	/*
	 * Now figure out what the proper burst sizes, etc., to use.
	 * Unfortunately, there is no ddi_dma_burstsizes here which
	 * walks up the tree finding the limiting burst size node (if
	 * any). We just use what's here for isp.
	 */
	ispburst = sbus_get_burstsz(dev);
	if (ispburst == 0) {
		ispburst = SBUS_BURST_32 - 1;
	}
	sbs->sbus_mdvec.dv_conf1 =  0;
	if (ispburst & (1 << 5)) {
		sbs->sbus_mdvec.dv_conf1 = BIU_SBUS_CONF1_FIFO_32;
	} else if (ispburst & (1 << 4)) {
		sbs->sbus_mdvec.dv_conf1 = BIU_SBUS_CONF1_FIFO_16;
	} else if (ispburst & (1 << 3)) {
		sbs->sbus_mdvec.dv_conf1 =
		    BIU_SBUS_CONF1_BURST8 | BIU_SBUS_CONF1_FIFO_8;
	}
	if (sbs->sbus_mdvec.dv_conf1) {
		sbs->sbus_mdvec.dv_conf1 |= BIU_BURST_ENABLE;
	}

	/*
	 * Some early versions of the PTI SBus adapter
	 * would fail in trying to download (via poking)
	 * FW. We give up on them.
	 */
	if (strcmp("PTI,ptisp", ofw_bus_get_name(dev)) == 0 ||
	    strcmp("ptisp", ofw_bus_get_name(dev)) == 0) {
		isp->isp_confopts |= ISP_CFG_NORELOAD;
	}

	/*
	 * We don't trust NVRAM on SBus cards
	 */
	isp->isp_confopts |= ISP_CFG_NONVRAM;


	/*
	 * Try and find firmware for this device.
	 */

	if (isp_get_firmware_p) {
		(*isp_get_firmware_p)(0, 0, 0x1000, &sbs->sbus_mdvec.dv_ispfw);
	}

	iqd = 0;
	sbs->sbus_ires = bus_alloc_resource_any(dev, SYS_RES_IRQ, &iqd,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sbs->sbus_ires == NULL) {
		device_printf(dev, "could not allocate interrupt\n");
		goto bad;
	}

	tval = 0;
        if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "fwload_disable", &tval) == 0 && tval != 0) {
		isp->isp_confopts |= ISP_CFG_NORELOAD;
	}

	isp->isp_osinfo.default_id = -1;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "iid", &tval) == 0) {
		isp->isp_osinfo.default_id = tval;
		isp->isp_confopts |= ISP_CFG_OWNLOOPID;
	}
	if (isp->isp_osinfo.default_id == -1) {
		/*
		 * XXX: should be a way to get properties w/o having
		 * XXX: to call OF_xxx functions
		 */
		isp->isp_osinfo.default_id = 7;
	}

	isp_debug = 0;
        (void) resource_int_value(device_get_name(dev), device_get_unit(dev),
            "debug", &isp_debug);

	/* Make sure the lock is set up. */
	mtx_init(&isp->isp_osinfo.lock, "isp", NULL, MTX_DEF);
	locksetup++;

	if (bus_setup_intr(dev, sbs->sbus_ires, ISP_IFLAGS,
	    isp_sbus_intr, isp, &sbs->ih)) {
		device_printf(dev, "could not setup interrupt\n");
		goto bad;
	}

	/*
	 * Set up logging levels.
	 */
	if (isp_debug) {
		isp->isp_dblev = isp_debug;
	} else {
		isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
	}
	if (bootverbose)
		isp->isp_dblev |= ISP_LOGCONFIG|ISP_LOGINFO;

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
	/*
	 * XXXX: Here is where we might unload the f/w module
	 * XXXX: (or decrease the reference count to it).
	 */
	ISP_UNLOCK(isp);
	return (0);

bad:

	if (sbs && sbs->ih) {
		(void) bus_teardown_intr(dev, sbs->sbus_ires, sbs->ih);
	}

	if (locksetup && isp) {
		mtx_destroy(&isp->isp_osinfo.lock);
	}

	if (sbs && sbs->sbus_ires) {
		bus_release_resource(dev, SYS_RES_IRQ, iqd, sbs->sbus_ires);
	}


	if (regs) {
		(void) bus_release_resource(dev, 0, 0, regs);
	}

	if (sbs) {
		if (sbs->sbus_isp.isp_param)
			free(sbs->sbus_isp.isp_param, M_DEVBUF);
		free(sbs, M_DEVBUF);
	}

	/*
	 * XXXX: Here is where we might unload the f/w module
	 * XXXX: (or decrease the reference count to it).
	 */
	return (ENXIO);
}

static void
isp_sbus_intr(void *arg)
{
	ispsoftc_t *isp = arg;
	uint16_t isr, sema, mbox;

	ISP_LOCK(isp);
	isp->isp_intcnt++;
	if (ISP_READ_ISR(isp, &isr, &sema, &mbox) == 0) {
		isp->isp_intbogus++;
	} else {
		int iok = isp->isp_osinfo.intsok;
		isp->isp_osinfo.intsok = 0;
		isp_intr(isp, isr, sema, mbox);
		isp->isp_osinfo.intsok = iok;
	}
	ISP_UNLOCK(isp);
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_sbussoftc *)a)->sbus_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xff))

#define	BXR2(sbc, off)		\
	bus_space_read_2(sbc->sbus_st, sbc->sbus_sh, off)

static int
isp_sbus_rd_isr(ispsoftc_t *isp, uint16_t *isrp,
    uint16_t *semap, uint16_t *mbp)
{
	struct isp_sbussoftc *sbc = (struct isp_sbussoftc *) isp;
	uint16_t isr, sema;

	isr = BXR2(sbc, IspVirt2Off(isp, BIU_ISR));
	sema = BXR2(sbc, IspVirt2Off(isp, BIU_SEMA));
	isp_prt(isp, ISP_LOGDEBUG3, "ISR 0x%x SEMA 0x%x", isr, sema);
	isr &= INT_PENDING_MASK(isp);
	sema &= BIU_SEMA_LOCK;
	if (isr == 0 && sema == 0) {
		return (0);
	}
	*isrp = isr;
	if ((*semap = sema) != 0) {
		*mbp = BXR2(sbc, IspVirt2Off(isp, OUTMAILBOX0));
	}
	return (1);
}

static uint16_t
isp_sbus_rd_reg(ispsoftc_t *isp, int regoff)
{
	uint16_t rval;
	struct isp_sbussoftc *sbs = (struct isp_sbussoftc *) isp;
	int offset = sbs->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	rval = bus_space_read_2(sbs->sbus_st, sbs->sbus_sh, offset);
	isp_prt(isp, ISP_LOGDEBUG3,
	    "isp_sbus_rd_reg(off %x) = %x", regoff, rval);
	return (rval);
}

static void
isp_sbus_wr_reg(ispsoftc_t *isp, int regoff, uint16_t val)
{
	struct isp_sbussoftc *sbs = (struct isp_sbussoftc *) isp;
	int offset = sbs->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	isp_prt(isp, ISP_LOGDEBUG3,
	    "isp_sbus_wr_reg(off %x) = %x", regoff, val);
	bus_space_write_2(sbs->sbus_st, sbs->sbus_sh, offset, val);
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
	}
}

/*
 * Should be BUS_SPACE_MAXSIZE, but MAXPHYS is larger than BUS_SPACE_MAXSIZE
 */
#define ISP_NSEGS ((MAXPHYS / PAGE_SIZE) + 1)  

static int
isp_sbus_mbxdma(ispsoftc_t *isp)
{
	struct isp_sbussoftc *sbs = (struct isp_sbussoftc *)isp;
	caddr_t base;
	uint32_t len;
	int i, error, ns;
	struct imush im;

	/*
	 * Already been here? If so, leave...
	 */
	if (isp->isp_rquest) {
		return (0);
	}

	ISP_UNLOCK(isp);

	if (bus_dma_tag_create(NULL, 1, BUS_SPACE_MAXADDR_24BIT+1,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR_32BIT,
	    NULL, NULL, BUS_SPACE_MAXSIZE_32BIT, ISP_NSEGS,
	    BUS_SPACE_MAXADDR_24BIT, 0, busdma_lock_mutex, &Giant,
	    &sbs->dmat)) {
		isp_prt(isp, ISP_LOGERR, "could not create master dma tag");
		ISP_LOCK(isp);
		return(1);
	}

	len = sizeof (XS_T **) * isp->isp_maxcmds;
	isp->isp_xflist = (XS_T **) malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (isp->isp_xflist == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot alloc xflist array");
		ISP_LOCK(isp);
		return (1);
	}
	len = sizeof (bus_dmamap_t) * isp->isp_maxcmds;
	sbs->dmaps = (bus_dmamap_t *) malloc(len, M_DEVBUF,  M_WAITOK);
	if (sbs->dmaps == NULL) {
		isp_prt(isp, ISP_LOGERR, "can't alloc dma map storage");
		free(isp->isp_xflist, M_DEVBUF);
		ISP_LOCK(isp);
		return (1);
	}

	/*
	 * Allocate and map the request, result queues, plus FC scratch area.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	len += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));

	ns = (len / PAGE_SIZE) + 1;
	if (bus_dma_tag_create(sbs->dmat, QENTRY_LEN, BUS_SPACE_MAXADDR_24BIT+1,
	    BUS_SPACE_MAXADDR_32BIT, BUS_SPACE_MAXADDR_32BIT, NULL, NULL,
	    len, ns, BUS_SPACE_MAXADDR_24BIT, 0, busdma_lock_mutex, &Giant,
	    &isp->isp_cdmat)) {
		isp_prt(isp, ISP_LOGERR,
		    "cannot create a dma tag for control spaces");
		free(sbs->dmaps, M_DEVBUF);
		free(isp->isp_xflist, M_DEVBUF);
		ISP_LOCK(isp);
		return (1);
	}

	if (bus_dmamem_alloc(isp->isp_cdmat, (void **)&base, BUS_DMA_NOWAIT,
	    &isp->isp_cdmap) != 0) {
		isp_prt(isp, ISP_LOGERR,
		    "cannot allocate %d bytes of CCB memory", len);
		bus_dma_tag_destroy(isp->isp_cdmat);
		free(isp->isp_xflist, M_DEVBUF);
		free(sbs->dmaps, M_DEVBUF);
		ISP_LOCK(isp);
		return (1);
	}

	for (i = 0; i < isp->isp_maxcmds; i++) {
		error = bus_dmamap_create(sbs->dmat, 0, &sbs->dmaps[i]);
		if (error) {
			isp_prt(isp, ISP_LOGERR,
			    "error %d creating per-cmd DMA maps", error);
			while (--i >= 0) {
				bus_dmamap_destroy(sbs->dmat, sbs->dmaps[i]);
			}
			goto bad;
		}
	}

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
	ISP_LOCK(isp);
	isp->isp_result = base;
	return (0);

bad:
	bus_dmamem_free(isp->isp_cdmat, base, isp->isp_cdmap);
	bus_dma_tag_destroy(isp->isp_cdmat);
	free(isp->isp_xflist, M_DEVBUF);
	free(sbs->dmaps, M_DEVBUF);
	ISP_LOCK(isp);
	isp->isp_rquest = NULL;
	return (1);
}

typedef struct {
	ispsoftc_t *isp;
	void *cmd_token;
	void *rq;
	uint16_t *nxtip;
	uint16_t optr;
	int error;
} mush_t;

#define	MUSHERR_NOQENTRIES	-2


static void dma2(void *, bus_dma_segment_t *, int, int);

static void
dma2(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	ispsoftc_t *isp;
	struct ccb_scsiio *csio;
	struct isp_sbussoftc *sbs;
	bus_dmamap_t *dp;
	bus_dma_segment_t *eseg;
	ispreq_t *rq;
	int seglim, datalen;
	uint16_t nxti;

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
	sbs = (struct isp_sbussoftc *)mp->isp;
	dp = &sbs->dmaps[isp_handle_index(rq->req_handle)];
	nxti = *mp->nxtip;

	if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(sbs->dmat, *dp, BUS_DMASYNC_PREREAD);
	} else {
		bus_dmamap_sync(sbs->dmat, *dp, BUS_DMASYNC_PREWRITE);
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

	eseg = dm_segs + nseg;

	while (datalen != 0 && rq->req_seg_count < seglim && dm_segs != eseg) {
		rq->req_dataseg[rq->req_seg_count].ds_base = dm_segs->ds_addr;
		rq->req_dataseg[rq->req_seg_count].ds_count = dm_segs->ds_len;
		datalen -= dm_segs->ds_len;
		rq->req_seg_count++;
		dm_segs++;
	}

	while (datalen > 0 && dm_segs != eseg) {
		uint16_t onxti;
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
			    dm_segs->ds_addr;
			crq->req_dataseg[seglim].ds_count =
			    dm_segs->ds_len;
			rq->req_seg_count++;
			dm_segs++;
			seglim++;
			datalen -= dm_segs->ds_len;
		}
		isp_put_cont_req(isp, crq, cqe);
		MEMORYBARRIER(isp, SYNC_REQUEST, onxti, QENTRY_LEN);
	}
	*mp->nxtip = nxti;
}

static int
isp_sbus_dmasetup(ispsoftc_t *isp, struct ccb_scsiio *csio, ispreq_t *rq,
	uint16_t *nxtip, uint16_t optr)
{
	struct isp_sbussoftc *sbs = (struct isp_sbussoftc *)isp;
	ispreq_t *qep;
	bus_dmamap_t *dp = NULL;
	mush_t mush, *mp;
	void (*eptr)(void *, bus_dma_segment_t *, int, int);

	qep = (ispreq_t *) ISP_QUEUE_ENTRY(isp->isp_rquest, isp->isp_reqidx);
	eptr = dma2;


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
			int error, s;
			dp = &sbs->dmaps[isp_handle_index(rq->req_handle)];
			s = splsoftvm();
			error = bus_dmamap_load(sbs->dmat, *dp,
			    csio->data_ptr, csio->dxfer_len, eptr, mp, 0);
			if (error == EINPROGRESS) {
				bus_dmamap_unload(sbs->dmat, *dp);
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
			splx(s);
		} else {
			/* Pointer to physical buffer */
			struct bus_dma_segment seg;
			seg.ds_addr = (bus_addr_t)csio->data_ptr;
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
	switch (rq->req_header.rqs_entry_type) {
	case RQSTYPE_REQUEST:
		isp_put_request(isp, rq, qep);
		break;
	case RQSTYPE_CMDONLY:
		isp_put_extended_request(isp, (ispextreq_t *)rq,
		    (ispextreq_t *)qep);
		break;
	}
	return (CMD_QUEUED);
}

static void
isp_sbus_dmateardown(ispsoftc_t *isp, XS_T *xs, uint16_t handle)
{
	struct isp_sbussoftc *sbs = (struct isp_sbussoftc *)isp;
	bus_dmamap_t *dp = &sbs->dmaps[isp_handle_index(handle)];
	if ((xs->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(sbs->dmat, *dp, BUS_DMASYNC_POSTREAD);
	} else {
		bus_dmamap_sync(sbs->dmat, *dp, BUS_DMASYNC_POSTWRITE);
	}
	bus_dmamap_unload(sbs->dmat, *dp);
}


static void
isp_sbus_reset1(ispsoftc_t *isp)
{
	/* enable interrupts */
	ENABLE_INTS(isp);
}

static void
isp_sbus_dumpregs(ispsoftc_t *isp, const char *msg)
{
	if (msg)
		printf("%s: %s\n", device_get_nameunit(isp->isp_dev), msg);
	else
		printf("%s:\n", device_get_nameunit(isp->isp_dev));
	printf("    biu_conf1=%x", ISP_READ(isp, BIU_CONF1));
	printf(" biu_icr=%x biu_isr=%x biu_sema=%x ", ISP_READ(isp, BIU_ICR),
	    ISP_READ(isp, BIU_ISR), ISP_READ(isp, BIU_SEMA));
	printf("risc_hccr=%x\n", ISP_READ(isp, HCCR));


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
	printf("    mbox regs: %x %x %x %x %x\n",
	    ISP_READ(isp, OUTMAILBOX0), ISP_READ(isp, OUTMAILBOX1),
	    ISP_READ(isp, OUTMAILBOX2), ISP_READ(isp, OUTMAILBOX3),
	    ISP_READ(isp, OUTMAILBOX4));
}
