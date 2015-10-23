/*-
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
/*
 * SBus specific probe and attach routines for Qlogic ISP SCSI adapters.
 * FreeBSD Version.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/linker.h>
#include <sys/firmware.h>
#include <sys/bus.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/resource.h>

#include <dev/ofw/ofw_bus.h>
#include <dev/ofw/openfirm.h>

#include <machine/bus.h>
#include <machine/ofw_machdep.h>
#include <machine/resource.h>
#include <sys/rman.h>
#include <sparc64/sbus/sbusvar.h>

#include <dev/isp/isp_freebsd.h>

static uint32_t isp_sbus_rd_reg(ispsoftc_t *, int);
static void isp_sbus_wr_reg(ispsoftc_t *, int, uint32_t);
static int isp_sbus_rd_isr(ispsoftc_t *, uint16_t *, uint16_t *, uint16_t *);
static int isp_sbus_mbxdma(ispsoftc_t *);
static int isp_sbus_dmasetup(ispsoftc_t *, XS_T *, void *);


static void isp_sbus_reset0(ispsoftc_t *);
static void isp_sbus_reset1(ispsoftc_t *);
static void isp_sbus_dumpregs(ispsoftc_t *, const char *);

static struct ispmdvec mdvec = {
	isp_sbus_rd_isr,
	isp_sbus_rd_reg,
	isp_sbus_wr_reg,
	isp_sbus_mbxdma,
	isp_sbus_dmasetup,
	isp_common_dmateardown,
	isp_sbus_reset0,
	isp_sbus_reset1,
	isp_sbus_dumpregs,
	NULL,
	BIU_BURST_ENABLE|BIU_PCI_CONF1_FIFO_64
};

static int isp_sbus_probe (device_t);
static int isp_sbus_attach (device_t);
static int isp_sbus_detach (device_t);


#define	ISP_SBD(isp)	((struct isp_sbussoftc *)isp)->sbus_dev
struct isp_sbussoftc {
	ispsoftc_t			sbus_isp;
	device_t			sbus_dev;
	struct resource *		regs;
	void *				irq;
	int				iqd;
	int				rgd;
	void *				ih;
	int16_t				sbus_poff[_NREG_BLKS];
	sdparam				sbus_param;
	struct isp_spi			sbus_spi;
	struct ispmdvec			sbus_mdvec;
};


static device_method_t isp_sbus_methods[] = {
	/* Device interface */
	DEVMETHOD(device_probe,		isp_sbus_probe),
	DEVMETHOD(device_attach,	isp_sbus_attach),
	DEVMETHOD(device_detach,	isp_sbus_detach),
	{ 0, 0 }
};

static driver_t isp_sbus_driver = {
	"isp", isp_sbus_methods, sizeof (struct isp_sbussoftc)
};
static devclass_t isp_devclass;
DRIVER_MODULE(isp, sbus, isp_sbus_driver, isp_devclass, 0, 0);
MODULE_DEPEND(isp, cam, 1, 1, 1);
MODULE_DEPEND(isp, firmware, 1, 1, 1);

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
	int tval, isp_debug, role, ispburst, default_id;
	struct isp_sbussoftc *sbs;
	ispsoftc_t *isp = NULL;
	int locksetup = 0;
	int ints_setup = 0;

	sbs = device_get_softc(dev);
	if (sbs == NULL) {
		device_printf(dev, "cannot get softc\n");
		return (ENOMEM);
	}

	sbs->sbus_dev = dev;
	sbs->sbus_mdvec = mdvec;

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
		role = ISP_DEFAULT_ROLES;
	}

	sbs->irq = sbs->regs = NULL;
	sbs->rgd = sbs->iqd = 0;

	sbs->regs = bus_alloc_resource_any(dev, SYS_RES_MEMORY, &sbs->rgd,
	    RF_ACTIVE);
	if (sbs->regs == NULL) {
		device_printf(dev, "unable to map registers\n");
		goto bad;
	}

	sbs->sbus_poff[BIU_BLOCK >> _BLK_REG_SHFT] = BIU_REGS_OFF;
	sbs->sbus_poff[MBOX_BLOCK >> _BLK_REG_SHFT] = SBUS_MBOX_REGS_OFF;
	sbs->sbus_poff[SXP_BLOCK >> _BLK_REG_SHFT] = SBUS_SXP_REGS_OFF;
	sbs->sbus_poff[RISC_BLOCK >> _BLK_REG_SHFT] = SBUS_RISC_REGS_OFF;
	sbs->sbus_poff[DMA_BLOCK >> _BLK_REG_SHFT] = DMA_REGS_OFF;
	isp = &sbs->sbus_isp;
	isp->isp_bus_tag = rman_get_bustag(sbs->regs);
	isp->isp_bus_handle = rman_get_bushandle(sbs->regs);
	isp->isp_mdvec = &sbs->sbus_mdvec;
	isp->isp_bustype = ISP_BT_SBUS;
	isp->isp_type = ISP_HA_SCSI_UNKNOWN;
	isp->isp_param = &sbs->sbus_param;
	isp->isp_osinfo.pc.ptr = &sbs->sbus_spi;
	isp->isp_revision = 0;	/* XXX */
	isp->isp_dev = dev;
	isp->isp_nchan = 1;
	ISP_SET_PC(isp, 0, def_role, role);

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
	 * We don't trust NVRAM on SBus cards
	 */
	isp->isp_confopts |= ISP_CFG_NONVRAM;

	/*
	 * Mark things if we're a PTI SBus adapter.
	 */
	if (strcmp("PTI,ptisp", ofw_bus_get_name(dev)) == 0 ||
	    strcmp("ptisp", ofw_bus_get_name(dev)) == 0) {
		SDPARAM(isp, 0)->isp_ptisp = 1;
	}

	isp->isp_osinfo.fw = firmware_get("isp_1000");
	if (isp->isp_osinfo.fw) {
		union {
			const void *cp;
			uint16_t *sp;
		} stupid;
		stupid.cp = isp->isp_osinfo.fw->data;
		isp->isp_mdvec->dv_ispfw = stupid.sp;
	}

	tval = 0;
        if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "fwload_disable", &tval) == 0 && tval != 0) {
		isp->isp_confopts |= ISP_CFG_NORELOAD;
	}

	default_id = -1;
	if (resource_int_value(device_get_name(dev), device_get_unit(dev),
            "iid", &tval) == 0) {
		default_id = tval;
		isp->isp_confopts |= ISP_CFG_OWNLOOPID;
	}
	if (default_id == -1) {
		default_id = OF_getscsinitid(dev);
	}
	ISP_SPI_PC(isp, 0)->iid = default_id;

	isp_debug = 0;
        (void) resource_int_value(device_get_name(dev), device_get_unit(dev),
            "debug", &isp_debug);

	/* Make sure the lock is set up. */
	mtx_init(&isp->isp_osinfo.lock, "isp", NULL, MTX_DEF);
	locksetup++;

	sbs->irq = bus_alloc_resource_any(dev, SYS_RES_IRQ, &sbs->iqd,
	    RF_ACTIVE | RF_SHAREABLE);
	if (sbs->irq == NULL) {
		device_printf(dev, "could not allocate interrupt\n");
		goto bad;
	}

	if (isp_setup_intr(dev, sbs->irq, ISP_IFLAGS, NULL, isp_platform_intr,
	    isp, &sbs->ih)) {
		device_printf(dev, "could not setup interrupt\n");
		goto bad;
	}
	ints_setup++;

	/*
	 * Set up logging levels.
	 */
	if (isp_debug) {
		isp->isp_dblev = isp_debug;
	} else {
		isp->isp_dblev = ISP_LOGWARN|ISP_LOGERR;
	}
	if (bootverbose) {
		isp->isp_dblev |= ISP_LOGCONFIG|ISP_LOGINFO;
	}

	/*
	 * Make sure we're in reset state.
	 */
	ISP_LOCK(isp);
	isp_reset(isp, 1);
	if (isp->isp_state != ISP_RESETSTATE) {
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		goto bad;
	}
	isp_init(isp);
	if (isp->isp_state == ISP_INITSTATE) {
		isp->isp_state = ISP_RUNSTATE;
	}
	ISP_UNLOCK(isp);
	if (isp_attach(isp)) {
		ISP_LOCK(isp);
		isp_uninit(isp);
		ISP_UNLOCK(isp);
		goto bad;
	}
	return (0);

bad:

	if (sbs && ints_setup) {
		(void) bus_teardown_intr(dev, sbs->irq, sbs->ih);
	}

	if (sbs && sbs->irq) {
		bus_release_resource(dev, SYS_RES_IRQ, sbs->iqd, sbs->irq);
	}

	if (locksetup && isp) {
		mtx_destroy(&isp->isp_osinfo.lock);
	}

	if (sbs->regs) {
		(void) bus_release_resource(dev, SYS_RES_MEMORY, sbs->rgd,
		    sbs->regs);
	}
	return (ENXIO);
}

static int
isp_sbus_detach(device_t dev)
{
	struct isp_sbussoftc *sbs;
	ispsoftc_t *isp;
	int status;

	sbs = device_get_softc(dev);
	if (sbs == NULL) {
		return (ENXIO);
	}
	isp = (ispsoftc_t *) sbs;
	status = isp_detach(isp);
	if (status)
		return (status);
	ISP_LOCK(isp);
	isp_uninit(isp);
	if (sbs->ih) {
		(void) bus_teardown_intr(dev, sbs->irq, sbs->ih);
	}
	ISP_UNLOCK(isp);
	mtx_destroy(&isp->isp_osinfo.lock);
	(void) bus_release_resource(dev, SYS_RES_IRQ, sbs->iqd, sbs->irq);
	(void) bus_release_resource(dev, SYS_RES_MEMORY, sbs->rgd, sbs->regs);
	return (0);
}

#define	IspVirt2Off(a, x)	\
	(((struct isp_sbussoftc *)a)->sbus_poff[((x) & _BLK_REG_MASK) >> \
	_BLK_REG_SHFT] + ((x) & 0xff))

#define	BXR2(sbc, off)		\
	bus_space_read_2(isp->isp_bus_tag, isp->isp_bus_handle, off)

static int
isp_sbus_rd_isr(ispsoftc_t *isp, uint16_t *isrp, uint16_t *semap, uint16_t *info)
{
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
	if ((*semap = sema) != 0)
		*info = BXR2(sbc, IspVirt2Off(isp, OUTMAILBOX0));
	return (1);
}

static uint32_t
isp_sbus_rd_reg(ispsoftc_t *isp, int regoff)
{
	uint16_t rval;
	struct isp_sbussoftc *sbs = (struct isp_sbussoftc *) isp;
	int offset = sbs->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	rval = bus_space_read_2(isp->isp_bus_tag, isp->isp_bus_handle, offset);
	isp_prt(isp, ISP_LOGDEBUG3,
	    "isp_sbus_rd_reg(off %x) = %x", regoff, rval);
	return (rval);
}

static void
isp_sbus_wr_reg(ispsoftc_t *isp, int regoff, uint32_t val)
{
	struct isp_sbussoftc *sbs = (struct isp_sbussoftc *) isp;
	int offset = sbs->sbus_poff[(regoff & _BLK_REG_MASK) >> _BLK_REG_SHFT];
	offset += (regoff & 0xff);
	isp_prt(isp, ISP_LOGDEBUG3,
	    "isp_sbus_wr_reg(off %x) = %x", regoff, val);
	bus_space_write_2(isp->isp_bus_tag, isp->isp_bus_handle, offset, val);
	MEMORYBARRIER(isp, SYNC_REG, offset, 2, -1);
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

static int
isp_sbus_mbxdma(ispsoftc_t *isp)
{
	caddr_t base;
	uint32_t len;
	int i, error;
	struct imush im;

	/*
	 * Already been here? If so, leave...
	 */
	if (isp->isp_rquest) {
		return (0);
	}

	ISP_UNLOCK(isp);

	len = sizeof (struct isp_pcmd) * isp->isp_maxcmds;
	isp->isp_osinfo.pcmd_pool = (struct isp_pcmd *)
	    malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (isp->isp_osinfo.pcmd_pool == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot alloc pcmd pool");
		ISP_LOCK(isp);
		return (1);
	}

	len = sizeof (isp_hdl_t *) * isp->isp_maxcmds;
	isp->isp_xflist = (isp_hdl_t *) malloc(len, M_DEVBUF, M_WAITOK | M_ZERO);
	if (isp->isp_xflist == NULL) {
		isp_prt(isp, ISP_LOGERR, "cannot alloc xflist array");
		ISP_LOCK(isp);
		return (1);
	}
	for (len = 0; len < isp->isp_maxcmds - 1; len++) {
		isp->isp_xflist[len].cmd = &isp->isp_xflist[len+1];
	}
	isp->isp_xffree = isp->isp_xflist;
	len = sizeof (bus_dmamap_t) * isp->isp_maxcmds;

	if (isp_dma_tag_create(BUS_DMA_ROOTARG(ISP_SBD(isp)), 1,
	    BUS_SPACE_MAXADDR_24BIT+1, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR_32BIT, NULL, NULL, BUS_SPACE_MAXSIZE_32BIT,
	    ISP_NSEG_MAX, BUS_SPACE_MAXADDR_24BIT, 0, &isp->isp_osinfo.dmat)) {
		isp_prt(isp, ISP_LOGERR, "could not create master dma tag");
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		free(isp->isp_xflist, M_DEVBUF);
		ISP_LOCK(isp);
		return(1);
	}

	/*
	 * Allocate and map the request, result queues, plus FC scratch area.
	 */
	len = ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	len += ISP_QUEUE_SIZE(RESULT_QUEUE_LEN(isp));

	if (isp_dma_tag_create(isp->isp_osinfo.dmat, QENTRY_LEN,
	    BUS_SPACE_MAXADDR_24BIT+1, BUS_SPACE_MAXADDR_32BIT,
	    BUS_SPACE_MAXADDR_32BIT, NULL, NULL, len, 1,
	    BUS_SPACE_MAXADDR_24BIT, 0, &isp->isp_osinfo.cdmat)) {
		isp_prt(isp, ISP_LOGERR,
		    "cannot create a dma tag for control spaces");
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		free(isp->isp_xflist, M_DEVBUF);
		ISP_LOCK(isp);
		return (1);
	}

	if (bus_dmamem_alloc(isp->isp_osinfo.cdmat, (void **)&base, BUS_DMA_NOWAIT | BUS_DMA_COHERENT,
	    &isp->isp_osinfo.cdmap) != 0) {
		isp_prt(isp, ISP_LOGERR,
		    "cannot allocate %d bytes of CCB memory", len);
		bus_dma_tag_destroy(isp->isp_osinfo.cdmat);
		free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
		free(isp->isp_xflist, M_DEVBUF);
		ISP_LOCK(isp);
		return (1);
	}

	for (i = 0; i < isp->isp_maxcmds; i++) {
		struct isp_pcmd *pcmd = &isp->isp_osinfo.pcmd_pool[i];
		error = bus_dmamap_create(isp->isp_osinfo.dmat, 0, &pcmd->dmap);
		if (error) {
			isp_prt(isp, ISP_LOGERR,
			    "error %d creating per-cmd DMA maps", error);
			while (--i >= 0) {
				bus_dmamap_destroy(isp->isp_osinfo.dmat,
				    isp->isp_osinfo.pcmd_pool[i].dmap);
			}
			goto bad;
		}
		callout_init_mtx(&pcmd->wdog, &isp->isp_osinfo.lock, 0);
		if (i == isp->isp_maxcmds-1) {
			pcmd->next = NULL;
		} else {
			pcmd->next = &isp->isp_osinfo.pcmd_pool[i+1];
		}
	}
	isp->isp_osinfo.pcmd_free = &isp->isp_osinfo.pcmd_pool[0];

	im.isp = isp;
	im.error = 0;
	bus_dmamap_load(isp->isp_osinfo.cdmat, isp->isp_osinfo.cdmap, base, len, imc, &im, 0);
	if (im.error) {
		isp_prt(isp, ISP_LOGERR,
		    "error %d loading dma map for control areas", im.error);
		goto bad;
	}

	isp->isp_rquest = base;
	base += ISP_QUEUE_SIZE(RQUEST_QUEUE_LEN(isp));
	isp->isp_result = base;
	ISP_LOCK(isp);
	return (0);

bad:
	bus_dmamem_free(isp->isp_osinfo.cdmat, base, isp->isp_osinfo.cdmap);
	bus_dma_tag_destroy(isp->isp_osinfo.cdmat);
	free(isp->isp_xflist, M_DEVBUF);
	free(isp->isp_osinfo.pcmd_pool, M_DEVBUF);
	isp->isp_rquest = NULL;
	ISP_LOCK(isp);
	return (1);
}

typedef struct {
	ispsoftc_t *isp;
	void *cmd_token;
	void *rq;	/* original request */
	int error;
	bus_size_t mapsize;
} mush_t;

#define	MUSHERR_NOQENTRIES	-2

static void dma2(void *, bus_dma_segment_t *, int, int);

static void
dma2(void *arg, bus_dma_segment_t *dm_segs, int nseg, int error)
{
	mush_t *mp;
	ispsoftc_t *isp;
	struct ccb_scsiio *csio;
	isp_ddir_t ddir;
	ispreq_t *rq;

	mp = (mush_t *) arg;
	if (error) {
		mp->error = error;
		return;
	}
	csio = mp->cmd_token;
	isp = mp->isp;
	rq = mp->rq;
	if (nseg) {
		if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
			bus_dmamap_sync(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREREAD);
			ddir = ISP_FROM_DEVICE;
		} else if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_OUT) {
			bus_dmamap_sync(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap, BUS_DMASYNC_PREWRITE);
			ddir = ISP_TO_DEVICE;
		} else {
			ddir = ISP_NOXFR;
		}
	} else {
		dm_segs = NULL;
		nseg = 0;
		ddir = ISP_NOXFR;
	}

	if (isp_send_cmd(isp, rq, dm_segs, nseg, XS_XFRLEN(csio), ddir, NULL) != CMD_QUEUED) {
		mp->error = MUSHERR_NOQENTRIES;
	}
}

static int
isp_sbus_dmasetup(ispsoftc_t *isp, struct ccb_scsiio *csio, void *ff)
{
	mush_t mush, *mp;
	void (*eptr)(void *, bus_dma_segment_t *, int, int);
	int error;

	mp = &mush;
	mp->isp = isp;
	mp->cmd_token = csio;
	mp->rq = ff;
	mp->error = 0;
	mp->mapsize = 0;

	eptr = dma2;

	error = bus_dmamap_load_ccb(isp->isp_osinfo.dmat,
	    PISP_PCMD(csio)->dmap, (union ccb *)csio, eptr, mp, 0);
	if (error == EINPROGRESS) {
		bus_dmamap_unload(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap);
		mp->error = EINVAL;
		isp_prt(isp, ISP_LOGERR,
		    "deferred dma allocation not supported");
	} else if (error && mp->error == 0) {
#ifdef	DIAGNOSTIC
		isp_prt(isp, ISP_LOGERR, "error %d in dma mapping code", error);
#endif
		mp->error = error;
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
	return (CMD_QUEUED);
}

static void
isp_sbus_reset0(ispsoftc_t *isp)
{
	ISP_DISABLE_INTS(isp);
}

static void
isp_sbus_reset1(ispsoftc_t *isp)
{
	ISP_ENABLE_INTS(isp);
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
