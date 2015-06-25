/*-
 * Copyright (c) 1997-2009 by Matthew Jacob
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
 * Platform (FreeBSD) dependent common attachment code for Qlogic adapters.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <dev/isp/isp_freebsd.h>
#include <sys/unistd.h>
#include <sys/kthread.h>
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/ioccom.h>
#include <dev/isp/isp_ioctl.h>
#include <sys/devicestat.h>
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>

#if	__FreeBSD_version < 800002 
#define	THREAD_CREATE	kthread_create
#else
#define	THREAD_CREATE	kproc_create
#endif

MODULE_VERSION(isp, 1);
MODULE_DEPEND(isp, cam, 1, 1, 1);
int isp_announced = 0;
int isp_fabric_hysteresis = 5;
int isp_loop_down_limit = 60;	/* default loop down limit */
int isp_change_is_bad = 0;	/* "changed" devices are bad */
int isp_quickboot_time = 7;	/* don't wait more than N secs for loop up */
int isp_gone_device_time = 30;	/* grace time before reporting device lost */
int isp_autoconfig = 1;		/* automatically attach/detach devices */
static const char prom3[] = "Chan %d PortID 0x%06x Departed from Target %u because of %s";

static void isp_freeze_loopdown(ispsoftc_t *, int, char *);
static d_ioctl_t ispioctl;
static void isp_intr_enable(void *);
static void isp_cam_async(void *, uint32_t, struct cam_path *, void *);
static void isp_poll(struct cam_sim *);
static timeout_t isp_watchdog;
static timeout_t isp_gdt;
static task_fn_t isp_gdt_task;
static timeout_t isp_ldt;
static task_fn_t isp_ldt_task;
static void isp_kthread(void *);
static void isp_action(struct cam_sim *, union ccb *);
#ifdef	ISP_INTERNAL_TARGET
static void isp_target_thread_pi(void *);
static void isp_target_thread_fc(void *);
#endif
static int isp_timer_count;
static void isp_timer(void *);

static struct cdevsw isp_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	ispioctl,
	.d_name =	"isp",
};

static int
isp_attach_chan(ispsoftc_t *isp, struct cam_devq *devq, int chan)
{
	struct ccb_setasync csa;
	struct cam_sim *sim;
	struct cam_path *path;

	/*
	 * Construct our SIM entry.
	 */
	sim = cam_sim_alloc(isp_action, isp_poll, "isp", isp, device_get_unit(isp->isp_dev), &isp->isp_osinfo.lock, isp->isp_maxcmds, isp->isp_maxcmds, devq);

	if (sim == NULL) {
		return (ENOMEM);
	}

	ISP_LOCK(isp);
	if (xpt_bus_register(sim, isp->isp_dev, chan) != CAM_SUCCESS) {
		ISP_UNLOCK(isp);
		cam_sim_free(sim, FALSE);
		return (EIO);
	}
	ISP_UNLOCK(isp);
	if (xpt_create_path(&path, NULL, cam_sim_path(sim), CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		ISP_LOCK(isp);
		xpt_bus_deregister(cam_sim_path(sim));
		ISP_UNLOCK(isp);
		cam_sim_free(sim, FALSE);
		return (ENXIO);
	}
	xpt_setup_ccb(&csa.ccb_h, path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = isp_cam_async;
	csa.callback_arg = sim;

	ISP_LOCK(isp);
	xpt_action((union ccb *)&csa);
	ISP_UNLOCK(isp);

	if (IS_SCSI(isp)) {
		struct isp_spi *spi = ISP_SPI_PC(isp, chan);
		spi->sim = sim;
		spi->path = path;
#ifdef	ISP_INTERNAL_TARGET
		ISP_SET_PC(isp, chan, proc_active, 1);
		if (THREAD_CREATE(isp_target_thread_pi, spi, &spi->target_proc, 0, 0, "%s: isp_test_tgt%d", device_get_nameunit(isp->isp_osinfo.dev), chan)) {
			ISP_SET_PC(isp, chan, proc_active, 0);
			isp_prt(isp, ISP_LOGERR, "cannot create test target thread");
		}
#endif
	} else {
		fcparam *fcp = FCPARAM(isp, chan);
		struct isp_fc *fc = ISP_FC_PC(isp, chan);

		ISP_LOCK(isp);
		fc->sim = sim;
		fc->path = path;
		fc->isp = isp;
		fc->ready = 1;

		callout_init_mtx(&fc->ldt, &isp->isp_osinfo.lock, 0);
		callout_init_mtx(&fc->gdt, &isp->isp_osinfo.lock, 0);
		TASK_INIT(&fc->ltask, 1, isp_ldt_task, fc);
		TASK_INIT(&fc->gtask, 1, isp_gdt_task, fc);

		/*
		 * We start by being "loop down" if we have an initiator role
		 */
		if (fcp->role & ISP_ROLE_INITIATOR) {
			isp_freeze_loopdown(isp, chan, "isp_attach");
			callout_reset(&fc->ldt, isp_quickboot_time * hz, isp_ldt, fc);
			isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Starting Initial Loop Down Timer @ %lu", (unsigned long) time_uptime);
		}
		ISP_UNLOCK(isp);
		if (THREAD_CREATE(isp_kthread, fc, &fc->kproc, 0, 0, "%s: fc_thrd%d", device_get_nameunit(isp->isp_osinfo.dev), chan)) {
			xpt_free_path(fc->path);
			ISP_LOCK(isp);
			if (callout_active(&fc->ldt))
				callout_stop(&fc->ldt);
			xpt_bus_deregister(cam_sim_path(fc->sim));
			ISP_UNLOCK(isp);
			cam_sim_free(fc->sim, FALSE);
			return (ENOMEM);
		}
#ifdef	ISP_INTERNAL_TARGET
		ISP_SET_PC(isp, chan, proc_active, 1);
		if (THREAD_CREATE(isp_target_thread_fc, fc, &fc->target_proc, 0, 0, "%s: isp_test_tgt%d", device_get_nameunit(isp->isp_osinfo.dev), chan)) {
			ISP_SET_PC(isp, chan, proc_active, 0);
			isp_prt(isp, ISP_LOGERR, "cannot create test target thread");
		}
#endif
		if (chan == 0) {
			struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(isp->isp_osinfo.dev);
			struct sysctl_oid *tree = device_get_sysctl_tree(isp->isp_osinfo.dev);
			SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "wwnn", CTLFLAG_RD, &FCPARAM(isp, 0)->isp_wwnn, "World Wide Node Name");
			SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "wwpn", CTLFLAG_RD, &FCPARAM(isp, 0)->isp_wwpn, "World Wide Port Name");
			SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "loop_down_limit", CTLFLAG_RW, &ISP_FC_PC(isp, 0)->loop_down_limit, 0, "Loop Down Limit");
			SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "gone_device_time", CTLFLAG_RW, &ISP_FC_PC(isp, 0)->gone_device_time, 0, "Gone Device Time");
#if defined(ISP_TARGET_MODE) && defined(DEBUG)
			SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO, "inject_lost_data_frame", CTLFLAG_RW, &ISP_FC_PC(isp, 0)->inject_lost_data_frame, 0, "Cause a Lost Frame on a Read");
#endif
		}
	}
	return (0);
}

int
isp_attach(ispsoftc_t *isp)
{
	const char *nu = device_get_nameunit(isp->isp_osinfo.dev);
	int du = device_get_unit(isp->isp_dev);
	int chan;

	isp->isp_osinfo.ehook.ich_func = isp_intr_enable;
	isp->isp_osinfo.ehook.ich_arg = isp;
	/*
	 * Haha. Set this first, because if we're loaded as a module isp_intr_enable
	 * will be called right awawy, which will clear isp_osinfo.ehook_active,
	 * which would be unwise to then set again later.
	 */
	isp->isp_osinfo.ehook_active = 1;
	if (config_intrhook_establish(&isp->isp_osinfo.ehook) != 0) {
		isp_prt(isp, ISP_LOGERR, "could not establish interrupt enable hook");
		return (-EIO);
	}

	/*
	 * Create the device queue for our SIM(s).
	 */
	isp->isp_osinfo.devq = cam_simq_alloc(isp->isp_maxcmds);
	if (isp->isp_osinfo.devq == NULL) {
		config_intrhook_disestablish(&isp->isp_osinfo.ehook);
		return (EIO);
	}

	for (chan = 0; chan < isp->isp_nchan; chan++) {
		if (isp_attach_chan(isp, isp->isp_osinfo.devq, chan)) {
			goto unwind;
		}
	}

	callout_init_mtx(&isp->isp_osinfo.tmo, &isp->isp_osinfo.lock, 0);
	isp_timer_count = hz >> 2;
	callout_reset(&isp->isp_osinfo.tmo, isp_timer_count, isp_timer, isp);
	isp->isp_osinfo.timer_active = 1;

	isp->isp_osinfo.cdev = make_dev(&isp_cdevsw, du, UID_ROOT, GID_OPERATOR, 0600, "%s", nu);
	if (isp->isp_osinfo.cdev) {
		isp->isp_osinfo.cdev->si_drv1 = isp;
	}
	return (0);

unwind:
	while (--chan >= 0) {
		struct cam_sim *sim;
		struct cam_path *path;
		if (IS_FC(isp)) {
			sim = ISP_FC_PC(isp, chan)->sim;
			path = ISP_FC_PC(isp, chan)->path;
		} else {
			sim = ISP_SPI_PC(isp, chan)->sim;
			path = ISP_SPI_PC(isp, chan)->path;
		}
		xpt_free_path(path);
		ISP_LOCK(isp);
		xpt_bus_deregister(cam_sim_path(sim));
		ISP_UNLOCK(isp);
		cam_sim_free(sim, FALSE);
	}
	if (isp->isp_osinfo.ehook_active) {
		config_intrhook_disestablish(&isp->isp_osinfo.ehook);
		isp->isp_osinfo.ehook_active = 0;
	}
	if (isp->isp_osinfo.cdev) {
		destroy_dev(isp->isp_osinfo.cdev);
		isp->isp_osinfo.cdev = NULL;
	}
	cam_simq_free(isp->isp_osinfo.devq);
	isp->isp_osinfo.devq = NULL;
	return (-1);
}

int
isp_detach(ispsoftc_t *isp)
{
	struct cam_sim *sim;
	struct cam_path *path;
	struct ccb_setasync csa;
	int chan;

	ISP_LOCK(isp);
	for (chan = isp->isp_nchan - 1; chan >= 0; chan -= 1) {
		if (IS_FC(isp)) {
			sim = ISP_FC_PC(isp, chan)->sim;
			path = ISP_FC_PC(isp, chan)->path;
		} else {
			sim = ISP_SPI_PC(isp, chan)->sim;
			path = ISP_SPI_PC(isp, chan)->path;
		}
		if (sim->refcount > 2) {
			ISP_UNLOCK(isp);
			return (EBUSY);
		}
	}
	if (isp->isp_osinfo.timer_active) {
		callout_stop(&isp->isp_osinfo.tmo);
		isp->isp_osinfo.timer_active = 0;
	}
	for (chan = isp->isp_nchan - 1; chan >= 0; chan -= 1) {
		if (IS_FC(isp)) {
			sim = ISP_FC_PC(isp, chan)->sim;
			path = ISP_FC_PC(isp, chan)->path;
		} else {
			sim = ISP_SPI_PC(isp, chan)->sim;
			path = ISP_SPI_PC(isp, chan)->path;
		}
		xpt_setup_ccb(&csa.ccb_h, path, 5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = 0;
		csa.callback = isp_cam_async;
		csa.callback_arg = sim;
		ISP_LOCK(isp);
		xpt_action((union ccb *)&csa);
		ISP_UNLOCK(isp);
		xpt_free_path(path);
		xpt_bus_deregister(cam_sim_path(sim));
		cam_sim_free(sim, FALSE);
	}
	ISP_UNLOCK(isp);
	if (isp->isp_osinfo.cdev) {
		destroy_dev(isp->isp_osinfo.cdev);
		isp->isp_osinfo.cdev = NULL;
	}
	if (isp->isp_osinfo.ehook_active) {
		config_intrhook_disestablish(&isp->isp_osinfo.ehook);
		isp->isp_osinfo.ehook_active = 0;
	}
	if (isp->isp_osinfo.devq != NULL) {
		cam_simq_free(isp->isp_osinfo.devq);
		isp->isp_osinfo.devq = NULL;
	}
	return (0);
}

static void
isp_freeze_loopdown(ispsoftc_t *isp, int chan, char *msg)
{
	if (IS_FC(isp)) {
		struct isp_fc *fc = ISP_FC_PC(isp, chan);
		if (fc->simqfrozen == 0) {
			isp_prt(isp, ISP_LOGDEBUG0, "%s: freeze simq (loopdown) chan %d", msg, chan);
			fc->simqfrozen = SIMQFRZ_LOOPDOWN;
			xpt_freeze_simq(fc->sim, 1);
		} else {
			isp_prt(isp, ISP_LOGDEBUG0, "%s: mark frozen (loopdown) chan %d", msg, chan);
			fc->simqfrozen |= SIMQFRZ_LOOPDOWN;
		}
	}
}

static void
isp_unfreeze_loopdown(ispsoftc_t *isp, int chan)
{
	if (IS_FC(isp)) {
		struct isp_fc *fc = ISP_FC_PC(isp, chan);
		int wasfrozen = fc->simqfrozen & SIMQFRZ_LOOPDOWN;
		fc->simqfrozen &= ~SIMQFRZ_LOOPDOWN;
		if (wasfrozen && fc->simqfrozen == 0) {
			isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "%s: Chan %d releasing simq", __func__, chan);
			xpt_release_simq(fc->sim, 1);
		}
	}
}


static int
ispioctl(struct cdev *dev, u_long c, caddr_t addr, int flags, struct thread *td)
{
	ispsoftc_t *isp;
	int nr, chan, retval = ENOTTY;

	isp = dev->si_drv1;

	switch (c) {
	case ISP_SDBLEV:
	{
		int olddblev = isp->isp_dblev;
		isp->isp_dblev = *(int *)addr;
		*(int *)addr = olddblev;
		retval = 0;
		break;
	}
	case ISP_GETROLE:
		chan = *(int *)addr;
		if (chan < 0 || chan >= isp->isp_nchan) {
			retval = -ENXIO;
			break;
		}
		if (IS_FC(isp)) {
			*(int *)addr = FCPARAM(isp, chan)->role;
		} else {
			*(int *)addr = SDPARAM(isp, chan)->role;
		}
		retval = 0;
		break;
	case ISP_SETROLE:
		nr = *(int *)addr;
		chan = nr >> 8;
		if (chan < 0 || chan >= isp->isp_nchan) {
			retval = -ENXIO;
			break;
		}
		nr &= 0xff;
		if (nr & ~(ISP_ROLE_INITIATOR|ISP_ROLE_TARGET)) {
			retval = EINVAL;
			break;
		}
		if (IS_FC(isp)) {
			/*
			 * We don't really support dual role at present on FC cards.
			 *
			 * We should, but a bunch of things are currently broken,
			 * so don't allow it.
			 */
			if (nr == ISP_ROLE_BOTH) {
				isp_prt(isp, ISP_LOGERR, "cannot support dual role at present");
				retval = EINVAL;
				break;
			}
			*(int *)addr = FCPARAM(isp, chan)->role;
#ifdef	ISP_INTERNAL_TARGET
			ISP_LOCK(isp);
			retval = isp_fc_change_role(isp, chan, nr);
			ISP_UNLOCK(isp);
#else
			FCPARAM(isp, chan)->role = nr;
#endif
		} else {
			*(int *)addr = SDPARAM(isp, chan)->role;
			SDPARAM(isp, chan)->role = nr;
		}
		retval = 0;
		break;

	case ISP_RESETHBA:
		ISP_LOCK(isp);
#ifdef	ISP_TARGET_MODE
		isp_del_all_wwn_entries(isp, ISP_NOCHAN);
#endif
		isp_reinit(isp, 0);
		ISP_UNLOCK(isp);
		retval = 0;
		break;

	case ISP_RESCAN:
		if (IS_FC(isp)) {
			chan = *(int *)addr;
			if (chan < 0 || chan >= isp->isp_nchan) {
				retval = -ENXIO;
				break;
			}
			ISP_LOCK(isp);
			if (isp_fc_runstate(isp, chan, 5 * 1000000)) {
				retval = EIO;
			} else {
				retval = 0;
			}
			ISP_UNLOCK(isp);
		}
		break;

	case ISP_FC_LIP:
		if (IS_FC(isp)) {
			chan = *(int *)addr;
			if (chan < 0 || chan >= isp->isp_nchan) {
				retval = -ENXIO;
				break;
			}
			ISP_LOCK(isp);
			if (isp_control(isp, ISPCTL_SEND_LIP, chan)) {
				retval = EIO;
			} else {
				retval = 0;
			}
			ISP_UNLOCK(isp);
		}
		break;
	case ISP_FC_GETDINFO:
	{
		struct isp_fc_device *ifc = (struct isp_fc_device *) addr;
		fcportdb_t *lp;

		if (IS_SCSI(isp)) {
			break;
		}
		if (ifc->loopid >= MAX_FC_TARG) {
			retval = EINVAL;
			break;
		}
		lp = &FCPARAM(isp, ifc->chan)->portdb[ifc->loopid];
		if (lp->state == FC_PORTDB_STATE_VALID || lp->target_mode) {
			ifc->role = (lp->prli_word3 & SVC3_ROLE_MASK) >> SVC3_ROLE_SHIFT;
			ifc->loopid = lp->handle;
			ifc->portid = lp->portid;
			ifc->node_wwn = lp->node_wwn;
			ifc->port_wwn = lp->port_wwn;
			retval = 0;
		} else {
			retval = ENODEV;
		}
		break;
	}
	case ISP_GET_STATS:
	{
		isp_stats_t *sp = (isp_stats_t *) addr;

		ISP_MEMZERO(sp, sizeof (*sp));
		sp->isp_stat_version = ISP_STATS_VERSION;
		sp->isp_type = isp->isp_type;
		sp->isp_revision = isp->isp_revision;
		ISP_LOCK(isp);
		sp->isp_stats[ISP_INTCNT] = isp->isp_intcnt;
		sp->isp_stats[ISP_INTBOGUS] = isp->isp_intbogus;
		sp->isp_stats[ISP_INTMBOXC] = isp->isp_intmboxc;
		sp->isp_stats[ISP_INGOASYNC] = isp->isp_intoasync;
		sp->isp_stats[ISP_RSLTCCMPLT] = isp->isp_rsltccmplt;
		sp->isp_stats[ISP_FPHCCMCPLT] = isp->isp_fphccmplt;
		sp->isp_stats[ISP_RSCCHIWAT] = isp->isp_rscchiwater;
		sp->isp_stats[ISP_FPCCHIWAT] = isp->isp_fpcchiwater;
		ISP_UNLOCK(isp);
		retval = 0;
		break;
	}
	case ISP_CLR_STATS:
		ISP_LOCK(isp);
		isp->isp_intcnt = 0;
		isp->isp_intbogus = 0;
		isp->isp_intmboxc = 0;
		isp->isp_intoasync = 0;
		isp->isp_rsltccmplt = 0;
		isp->isp_fphccmplt = 0;
		isp->isp_rscchiwater = 0;
		isp->isp_fpcchiwater = 0;
		ISP_UNLOCK(isp);
		retval = 0;
		break;
	case ISP_FC_GETHINFO:
	{
		struct isp_hba_device *hba = (struct isp_hba_device *) addr;
		int chan = hba->fc_channel;

		if (chan < 0 || chan >= isp->isp_nchan) {
			retval = ENXIO;
			break;
		}
		hba->fc_fw_major = ISP_FW_MAJORX(isp->isp_fwrev);
		hba->fc_fw_minor = ISP_FW_MINORX(isp->isp_fwrev);
		hba->fc_fw_micro = ISP_FW_MICROX(isp->isp_fwrev);
		hba->fc_nchannels = isp->isp_nchan;
		if (IS_FC(isp)) {
			hba->fc_nports = MAX_FC_TARG;
			hba->fc_speed = FCPARAM(isp, hba->fc_channel)->isp_gbspeed;
			hba->fc_topology = FCPARAM(isp, chan)->isp_topo + 1;
			hba->fc_loopid = FCPARAM(isp, chan)->isp_loopid;
			hba->nvram_node_wwn = FCPARAM(isp, chan)->isp_wwnn_nvram;
			hba->nvram_port_wwn = FCPARAM(isp, chan)->isp_wwpn_nvram;
			hba->active_node_wwn = FCPARAM(isp, chan)->isp_wwnn;
			hba->active_port_wwn = FCPARAM(isp, chan)->isp_wwpn;
		} else {
			hba->fc_nports = MAX_TARGETS;
			hba->fc_speed = 0;
			hba->fc_topology = 0;
			hba->nvram_node_wwn = 0ull;
			hba->nvram_port_wwn = 0ull;
			hba->active_node_wwn = 0ull;
			hba->active_port_wwn = 0ull;
		}
		retval = 0;
		break;
	}
	case ISP_TSK_MGMT:
	{
		int needmarker;
		struct isp_fc_tsk_mgmt *fct = (struct isp_fc_tsk_mgmt *) addr;
		uint16_t loopid;
		mbreg_t mbs;

		if (IS_SCSI(isp)) {
			break;
		}

		chan = fct->chan;
		if (chan < 0 || chan >= isp->isp_nchan) {
			retval = -ENXIO;
			break;
		}

		needmarker = retval = 0;
		loopid = fct->loopid;
		ISP_LOCK(isp);
		if (IS_24XX(isp)) {
			uint8_t local[QENTRY_LEN];
			isp24xx_tmf_t *tmf;
			isp24xx_statusreq_t *sp;
			fcparam *fcp = FCPARAM(isp, chan);
			fcportdb_t *lp;
			int i;

			for (i = 0; i < MAX_FC_TARG; i++) {
				lp = &fcp->portdb[i];
				if (lp->handle == loopid) {
					break;
				}
			}
			if (i == MAX_FC_TARG) {
				retval = ENXIO;
				ISP_UNLOCK(isp);
				break;
			}
			/* XXX VALIDATE LP XXX */
			tmf = (isp24xx_tmf_t *) local;
			ISP_MEMZERO(tmf, QENTRY_LEN);
			tmf->tmf_header.rqs_entry_type = RQSTYPE_TSK_MGMT;
			tmf->tmf_header.rqs_entry_count = 1;
			tmf->tmf_nphdl = lp->handle;
			tmf->tmf_delay = 2;
			tmf->tmf_timeout = 2;
			tmf->tmf_tidlo = lp->portid;
			tmf->tmf_tidhi = lp->portid >> 16;
			tmf->tmf_vpidx = ISP_GET_VPIDX(isp, chan);
			tmf->tmf_lun[1] = fct->lun & 0xff;
			if (fct->lun >= 256) {
				tmf->tmf_lun[0] = 0x40 | (fct->lun >> 8);
			}
			switch (fct->action) {
			case IPT_CLEAR_ACA:
				tmf->tmf_flags = ISP24XX_TMF_CLEAR_ACA;
				break;
			case IPT_TARGET_RESET:
				tmf->tmf_flags = ISP24XX_TMF_TARGET_RESET;
				needmarker = 1;
				break;
			case IPT_LUN_RESET:
				tmf->tmf_flags = ISP24XX_TMF_LUN_RESET;
				needmarker = 1;
				break;
			case IPT_CLEAR_TASK_SET:
				tmf->tmf_flags = ISP24XX_TMF_CLEAR_TASK_SET;
				needmarker = 1;
				break;
			case IPT_ABORT_TASK_SET:
				tmf->tmf_flags = ISP24XX_TMF_ABORT_TASK_SET;
				needmarker = 1;
				break;
			default:
				retval = EINVAL;
				break;
			}
			if (retval) {
				ISP_UNLOCK(isp);
				break;
			}
			MBSINIT(&mbs, MBOX_EXEC_COMMAND_IOCB_A64, MBLOGALL, 5000000);
			mbs.param[1] = QENTRY_LEN;
			mbs.param[2] = DMA_WD1(fcp->isp_scdma);
			mbs.param[3] = DMA_WD0(fcp->isp_scdma);
			mbs.param[6] = DMA_WD3(fcp->isp_scdma);
			mbs.param[7] = DMA_WD2(fcp->isp_scdma);

			if (FC_SCRATCH_ACQUIRE(isp, chan)) {
				ISP_UNLOCK(isp);
				retval = ENOMEM;
				break;
			}
			isp_put_24xx_tmf(isp, tmf, fcp->isp_scratch);
			MEMORYBARRIER(isp, SYNC_SFORDEV, 0, QENTRY_LEN, chan);
			sp = (isp24xx_statusreq_t *) local;
			sp->req_completion_status = 1;
			retval = isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
			MEMORYBARRIER(isp, SYNC_SFORCPU, QENTRY_LEN, QENTRY_LEN, chan);
			isp_get_24xx_response(isp, &((isp24xx_statusreq_t *)fcp->isp_scratch)[1], sp);
			FC_SCRATCH_RELEASE(isp, chan);
			if (retval || sp->req_completion_status != 0) {
				FC_SCRATCH_RELEASE(isp, chan);
				retval = EIO;
			}
			if (retval == 0) {
				if (needmarker) {
					fcp->sendmarker = 1;
				}
			}
		} else {
			MBSINIT(&mbs, 0, MBLOGALL, 0);
			if (ISP_CAP_2KLOGIN(isp) == 0) {
				loopid <<= 8;
			}
			switch (fct->action) {
			case IPT_CLEAR_ACA:
				mbs.param[0] = MBOX_CLEAR_ACA;
				mbs.param[1] = loopid;
				mbs.param[2] = fct->lun;
				break;
			case IPT_TARGET_RESET:
				mbs.param[0] = MBOX_TARGET_RESET;
				mbs.param[1] = loopid;
				needmarker = 1;
				break;
			case IPT_LUN_RESET:
				mbs.param[0] = MBOX_LUN_RESET;
				mbs.param[1] = loopid;
				mbs.param[2] = fct->lun;
				needmarker = 1;
				break;
			case IPT_CLEAR_TASK_SET:
				mbs.param[0] = MBOX_CLEAR_TASK_SET;
				mbs.param[1] = loopid;
				mbs.param[2] = fct->lun;
				needmarker = 1;
				break;
			case IPT_ABORT_TASK_SET:
				mbs.param[0] = MBOX_ABORT_TASK_SET;
				mbs.param[1] = loopid;
				mbs.param[2] = fct->lun;
				needmarker = 1;
				break;
			default:
				retval = EINVAL;
				break;
			}
			if (retval == 0) {
				if (needmarker) {
					FCPARAM(isp, chan)->sendmarker = 1;
				}
				retval = isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
				if (retval) {
					retval = EIO;
				}
			}
		}
		ISP_UNLOCK(isp);
		break;
	}
	default:
		break;
	}
	return (retval);
}

static void
isp_intr_enable(void *arg)
{
	int chan;
	ispsoftc_t *isp = arg;
	ISP_LOCK(isp);
	for (chan = 0; chan < isp->isp_nchan; chan++) {
		if (IS_FC(isp)) {
			if (FCPARAM(isp, chan)->role != ISP_ROLE_NONE) {
				ISP_ENABLE_INTS(isp);
				break;
			}
		} else {
			if (SDPARAM(isp, chan)->role != ISP_ROLE_NONE) {
				ISP_ENABLE_INTS(isp);
				break;
			}
		}
	}
	isp->isp_osinfo.ehook_active = 0;
	ISP_UNLOCK(isp);
	/* Release our hook so that the boot can continue. */
	config_intrhook_disestablish(&isp->isp_osinfo.ehook);
}

/*
 * Local Inlines
 */

static ISP_INLINE int isp_get_pcmd(ispsoftc_t *, union ccb *);
static ISP_INLINE void isp_free_pcmd(ispsoftc_t *, union ccb *);

static ISP_INLINE int
isp_get_pcmd(ispsoftc_t *isp, union ccb *ccb)
{
	ISP_PCMD(ccb) = isp->isp_osinfo.pcmd_free;
	if (ISP_PCMD(ccb) == NULL) {
		return (-1);
	}
	isp->isp_osinfo.pcmd_free = ((struct isp_pcmd *)ISP_PCMD(ccb))->next;
	return (0);
}

static ISP_INLINE void
isp_free_pcmd(ispsoftc_t *isp, union ccb *ccb)
{
	if (ISP_PCMD(ccb)) {
#ifdef	ISP_TARGET_MODE
		PISP_PCMD(ccb)->datalen = 0;
		PISP_PCMD(ccb)->totslen = 0;
		PISP_PCMD(ccb)->cumslen = 0;
		PISP_PCMD(ccb)->crn = 0;
#endif
		PISP_PCMD(ccb)->next = isp->isp_osinfo.pcmd_free;
		isp->isp_osinfo.pcmd_free = ISP_PCMD(ccb);
		ISP_PCMD(ccb) = NULL;
	}
}

/*
 * Put the target mode functions here, because some are inlines
 */
#ifdef	ISP_TARGET_MODE
static ISP_INLINE void isp_tmlock(ispsoftc_t *, const char *);
static ISP_INLINE void isp_tmunlk(ispsoftc_t *);
static ISP_INLINE int is_any_lun_enabled(ispsoftc_t *, int);
static ISP_INLINE int is_lun_enabled(ispsoftc_t *, int, lun_id_t);
static ISP_INLINE tstate_t *get_lun_statep(ispsoftc_t *, int, lun_id_t);
static ISP_INLINE tstate_t *get_lun_statep_from_tag(ispsoftc_t *, int, uint32_t);
static ISP_INLINE void rls_lun_statep(ispsoftc_t *, tstate_t *);
static ISP_INLINE inot_private_data_t *get_ntp_from_tagdata(ispsoftc_t *, uint32_t, uint32_t, tstate_t **);
static ISP_INLINE atio_private_data_t *isp_get_atpd(ispsoftc_t *, tstate_t *, uint32_t);
static ISP_INLINE atio_private_data_t *isp_find_atpd(ispsoftc_t *, tstate_t *, uint32_t);
static ISP_INLINE void isp_put_atpd(ispsoftc_t *, tstate_t *, atio_private_data_t *);
static ISP_INLINE inot_private_data_t *isp_get_ntpd(ispsoftc_t *, tstate_t *);
static ISP_INLINE inot_private_data_t *isp_find_ntpd(ispsoftc_t *, tstate_t *, uint32_t, uint32_t);
static ISP_INLINE void isp_put_ntpd(ispsoftc_t *, tstate_t *, inot_private_data_t *);
static cam_status create_lun_state(ispsoftc_t *, int, struct cam_path *, tstate_t **);
static void destroy_lun_state(ispsoftc_t *, tstate_t *);
static void isp_enable_lun(ispsoftc_t *, union ccb *);
static cam_status isp_enable_deferred_luns(ispsoftc_t *, int);
static cam_status isp_enable_deferred(ispsoftc_t *, int, lun_id_t);
static void isp_disable_lun(ispsoftc_t *, union ccb *);
static int isp_enable_target_mode(ispsoftc_t *, int);
static int isp_disable_target_mode(ispsoftc_t *, int);
static void isp_ledone(ispsoftc_t *, lun_entry_t *);
static timeout_t isp_refire_putback_atio;
static timeout_t isp_refire_notify_ack;
static void isp_complete_ctio(union ccb *);
static void isp_target_putback_atio(union ccb *);
enum Start_Ctio_How { FROM_CAM, FROM_TIMER, FROM_SRR, FROM_CTIO_DONE };
static void isp_target_start_ctio(ispsoftc_t *, union ccb *, enum Start_Ctio_How);
static void isp_handle_platform_atio(ispsoftc_t *, at_entry_t *);
static void isp_handle_platform_atio2(ispsoftc_t *, at2_entry_t *);
static void isp_handle_platform_atio7(ispsoftc_t *, at7_entry_t *);
static void isp_handle_platform_ctio(ispsoftc_t *, void *);
static void isp_handle_platform_notify_scsi(ispsoftc_t *, in_entry_t *);
static void isp_handle_platform_notify_fc(ispsoftc_t *, in_fcentry_t *);
static void isp_handle_platform_notify_24xx(ispsoftc_t *, in_fcentry_24xx_t *);
static int isp_handle_platform_target_notify_ack(ispsoftc_t *, isp_notify_t *);
static void isp_handle_platform_target_tmf(ispsoftc_t *, isp_notify_t *);
static void isp_target_mark_aborted(ispsoftc_t *, union ccb *);
static void isp_target_mark_aborted_early(ispsoftc_t *, tstate_t *, uint32_t);

static ISP_INLINE void
isp_tmlock(ispsoftc_t *isp, const char *msg)
{
	while (isp->isp_osinfo.tmbusy) {
		isp->isp_osinfo.tmwanted = 1;
		mtx_sleep(isp, &isp->isp_lock, PRIBIO, msg, 0);
	}
	isp->isp_osinfo.tmbusy = 1;
}

static ISP_INLINE void
isp_tmunlk(ispsoftc_t *isp)
{
	isp->isp_osinfo.tmbusy = 0;
	if (isp->isp_osinfo.tmwanted) {
		isp->isp_osinfo.tmwanted = 0;
		wakeup(isp);
	}
}

static ISP_INLINE int
is_any_lun_enabled(ispsoftc_t *isp, int bus)
{
	struct tslist *lhp;
	int i;

	for (i = 0; i < LUN_HASH_SIZE; i++) {
		ISP_GET_PC_ADDR(isp, bus, lun_hash[i], lhp);
		if (SLIST_FIRST(lhp))
			return (1);
	}
	return (0);
}

static ISP_INLINE int
is_lun_enabled(ispsoftc_t *isp, int bus, lun_id_t lun)
{
	tstate_t *tptr;
	struct tslist *lhp;

	ISP_GET_PC_ADDR(isp, bus, lun_hash[LUN_HASH_FUNC(lun)], lhp);
	SLIST_FOREACH(tptr, lhp, next) {
		if (tptr->ts_lun == lun) {
			return (1);
		}
	}
	return (0);
}

static void
dump_tstates(ispsoftc_t *isp, int bus)
{
	int i, j;
	struct tslist *lhp;
	tstate_t *tptr = NULL;

	if (bus >= isp->isp_nchan) {
		return;
	}
	for (i = 0; i < LUN_HASH_SIZE; i++) {
		ISP_GET_PC_ADDR(isp, bus, lun_hash[i], lhp);
		j = 0;
		SLIST_FOREACH(tptr, lhp, next) {
			xpt_print(tptr->owner, "[%d, %d] atio_cnt=%d inot_cnt=%d\n", i, j, tptr->atio_count, tptr->inot_count);
			j++;
		}
	}
}

static ISP_INLINE tstate_t *
get_lun_statep(ispsoftc_t *isp, int bus, lun_id_t lun)
{
	tstate_t *tptr = NULL;
	struct tslist *lhp;

	if (bus < isp->isp_nchan) {
		ISP_GET_PC_ADDR(isp, bus, lun_hash[LUN_HASH_FUNC(lun)], lhp);
		SLIST_FOREACH(tptr, lhp, next) {
			if (tptr->ts_lun == lun) {
				tptr->hold++;
				return (tptr);
			}
		}
	}
	return (NULL);
}

static ISP_INLINE tstate_t *
get_lun_statep_from_tag(ispsoftc_t *isp, int bus, uint32_t tagval)
{
	tstate_t *tptr = NULL;
	atio_private_data_t *atp;
	struct tslist *lhp;
	int i;

	if (bus < isp->isp_nchan && tagval != 0) {
		for (i = 0; i < LUN_HASH_SIZE; i++) {
			ISP_GET_PC_ADDR(isp, bus, lun_hash[i], lhp);
			SLIST_FOREACH(tptr, lhp, next) {
				atp = isp_find_atpd(isp, tptr, tagval);
				if (atp) {
					tptr->hold++;
					return (tptr);
				}
			}
		}
	}
	return (NULL);
}

static ISP_INLINE inot_private_data_t *
get_ntp_from_tagdata(ispsoftc_t *isp, uint32_t tag_id, uint32_t seq_id, tstate_t **rslt)
{
	inot_private_data_t *ntp;
	tstate_t *tptr;
	struct tslist *lhp;
	int bus, i;

	for (bus = 0; bus < isp->isp_nchan; bus++) {
		for (i = 0; i < LUN_HASH_SIZE; i++) {
			ISP_GET_PC_ADDR(isp, bus, lun_hash[i], lhp);
			SLIST_FOREACH(tptr, lhp, next) {
				ntp = isp_find_ntpd(isp, tptr, tag_id, seq_id);
				if (ntp) {
					*rslt = tptr;
					tptr->hold++;
					return (ntp);
				}
			}
		}
	}
	return (NULL);
}

static ISP_INLINE void
rls_lun_statep(ispsoftc_t *isp, tstate_t *tptr)
{
	KASSERT((tptr->hold), ("tptr not held"));
	tptr->hold--;
}

static void
isp_tmcmd_restart(ispsoftc_t *isp)
{
	inot_private_data_t *ntp;
	inot_private_data_t *restart_queue;
	tstate_t *tptr;
	union ccb *ccb;
	struct tslist *lhp;
	int bus, i;

	for (bus = 0; bus < isp->isp_nchan; bus++) {
		for (i = 0; i < LUN_HASH_SIZE; i++) {
			ISP_GET_PC_ADDR(isp, bus, lun_hash[i], lhp);
			SLIST_FOREACH(tptr, lhp, next) {
				if ((restart_queue = tptr->restart_queue) != NULL)
					tptr->restart_queue = NULL;
				while (restart_queue) {
					ntp = restart_queue;
					restart_queue = ntp->rd.nt.nt_hba;
					if (IS_24XX(isp)) {
						isp_prt(isp, ISP_LOGTDEBUG0, "%s: restarting resrc deprived %x", __func__, ((at7_entry_t *)ntp->rd.data)->at_rxid);
						isp_handle_platform_atio7(isp, (at7_entry_t *) ntp->rd.data);
					} else {
						isp_prt(isp, ISP_LOGTDEBUG0, "%s: restarting resrc deprived %x", __func__, ((at2_entry_t *)ntp->rd.data)->at_rxid);
						isp_handle_platform_atio2(isp, (at2_entry_t *) ntp->rd.data);
					}
					isp_put_ntpd(isp, tptr, ntp);
					if (tptr->restart_queue && restart_queue != NULL) {
						ntp = tptr->restart_queue;
						tptr->restart_queue = restart_queue;
						while (restart_queue->rd.nt.nt_hba) {
							restart_queue = restart_queue->rd.nt.nt_hba;
						}
						restart_queue->rd.nt.nt_hba = ntp;
						break;
					}
				}
				/*
				 * We only need to do this once per tptr
				 */
				if (!TAILQ_EMPTY(&tptr->waitq)) {
					ccb = (union ccb *)TAILQ_LAST(&tptr->waitq, isp_ccbq);
					TAILQ_REMOVE(&tptr->waitq, &ccb->ccb_h, periph_links.tqe);
					isp_target_start_ctio(isp, ccb, FROM_TIMER);
				}
			}
		}
	}
}

static ISP_INLINE atio_private_data_t *
isp_get_atpd(ispsoftc_t *isp, tstate_t *tptr, uint32_t tag)
{
	atio_private_data_t *atp;

	atp = LIST_FIRST(&tptr->atfree);
	if (atp) {
		LIST_REMOVE(atp, next);
		atp->tag = tag;
		LIST_INSERT_HEAD(&tptr->atused[ATPDPHASH(tag)], atp, next);
	}
	return (atp);
}

static ISP_INLINE atio_private_data_t *
isp_find_atpd(ispsoftc_t *isp, tstate_t *tptr, uint32_t tag)
{
	atio_private_data_t *atp;

	LIST_FOREACH(atp, &tptr->atused[ATPDPHASH(tag)], next) {
		if (atp->tag == tag)
			return (atp);
	}
	return (NULL);
}

static ISP_INLINE void
isp_put_atpd(ispsoftc_t *isp, tstate_t *tptr, atio_private_data_t *atp)
{
	if (atp->ests) {
		isp_put_ecmd(isp, atp->ests);
	}
	LIST_REMOVE(atp, next);
	memset(atp, 0, sizeof (*atp));
	LIST_INSERT_HEAD(&tptr->atfree, atp, next);
}

static void
isp_dump_atpd(ispsoftc_t *isp, tstate_t *tptr)
{
	atio_private_data_t *atp;
	const char *states[8] = { "Free", "ATIO", "CAM", "CTIO", "LAST_CTIO", "PDON", "?6", "7" };

	for (atp = tptr->atpool; atp < &tptr->atpool[ATPDPSIZE]; atp++) {
		xpt_print(tptr->owner, "ATP: [0x%x] origdlen %u bytes_xfrd %u lun %u nphdl 0x%04x s_id 0x%06x d_id 0x%06x oxid 0x%04x state %s\n",
		    atp->tag, atp->orig_datalen, atp->bytes_xfered, atp->lun, atp->nphdl, atp->sid, atp->portid, atp->oxid, states[atp->state & 0x7]);
	}
}


static ISP_INLINE inot_private_data_t *
isp_get_ntpd(ispsoftc_t *isp, tstate_t *tptr)
{
	inot_private_data_t *ntp;
	ntp = tptr->ntfree;
	if (ntp) {
		tptr->ntfree = ntp->next;
	}
	return (ntp);
}

static ISP_INLINE inot_private_data_t *
isp_find_ntpd(ispsoftc_t *isp, tstate_t *tptr, uint32_t tag_id, uint32_t seq_id)
{
	inot_private_data_t *ntp;
	for (ntp = tptr->ntpool; ntp < &tptr->ntpool[ATPDPSIZE]; ntp++) {
		if (ntp->rd.tag_id == tag_id && ntp->rd.seq_id == seq_id) {
			return (ntp);
		}
	}
	return (NULL);
}

static ISP_INLINE void
isp_put_ntpd(ispsoftc_t *isp, tstate_t *tptr, inot_private_data_t *ntp)
{
	ntp->rd.tag_id = ntp->rd.seq_id = 0;
	ntp->next = tptr->ntfree;
	tptr->ntfree = ntp;
}

static cam_status
create_lun_state(ispsoftc_t *isp, int bus, struct cam_path *path, tstate_t **rslt)
{
	cam_status status;
	lun_id_t lun;
	struct tslist *lhp;
	tstate_t *tptr;
	int i;

	lun = xpt_path_lun_id(path);
	if (lun != CAM_LUN_WILDCARD) {
		if (lun >= ISP_MAX_LUNS(isp)) {
			return (CAM_LUN_INVALID);
		}
	}
	if (is_lun_enabled(isp, bus, lun)) {
		return (CAM_LUN_ALRDY_ENA);
	}
	tptr = malloc(sizeof (tstate_t), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (tptr == NULL) {
		return (CAM_RESRC_UNAVAIL);
	}
	tptr->ts_lun = lun;
	status = xpt_create_path(&tptr->owner, NULL, xpt_path_path_id(path), xpt_path_target_id(path), lun);
	if (status != CAM_REQ_CMP) {
		free(tptr, M_DEVBUF);
		return (status);
	}
	SLIST_INIT(&tptr->atios);
	SLIST_INIT(&tptr->inots);
	TAILQ_INIT(&tptr->waitq);
	LIST_INIT(&tptr->atfree);
	for (i = ATPDPSIZE-1; i >= 0; i--)
		LIST_INSERT_HEAD(&tptr->atfree, &tptr->atpool[i], next);
	for (i = 0; i < ATPDPHASHSIZE; i++)
		LIST_INIT(&tptr->atused[i]);
	for (i = 0; i < ATPDPSIZE-1; i++)
		tptr->ntpool[i].next = &tptr->ntpool[i+1];
	tptr->ntfree = tptr->ntpool;
	tptr->hold = 1;
	ISP_GET_PC_ADDR(isp, bus, lun_hash[LUN_HASH_FUNC(lun)], lhp);
	SLIST_INSERT_HEAD(lhp, tptr, next);
	*rslt = tptr;
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, path, "created tstate\n");
	return (CAM_REQ_CMP);
}

static ISP_INLINE void
destroy_lun_state(ispsoftc_t *isp, tstate_t *tptr)
{
	union ccb *ccb;
	struct tslist *lhp;

	KASSERT((tptr->hold != 0), ("tptr is not held"));
	KASSERT((tptr->hold == 1), ("tptr still held (%d)", tptr->hold));
	do {
		ccb = (union ccb *)SLIST_FIRST(&tptr->atios);
		if (ccb) {
			SLIST_REMOVE_HEAD(&tptr->atios, sim_links.sle);
			ccb->ccb_h.status = CAM_REQ_ABORTED;
			xpt_done(ccb);
		}
	} while (ccb);
	do {
		ccb = (union ccb *)SLIST_FIRST(&tptr->inots);
		if (ccb) {
			SLIST_REMOVE_HEAD(&tptr->inots, sim_links.sle);
			ccb->ccb_h.status = CAM_REQ_ABORTED;
			xpt_done(ccb);
		}
	} while (ccb);
	ISP_GET_PC_ADDR(isp, cam_sim_bus(xpt_path_sim(tptr->owner)), lun_hash[LUN_HASH_FUNC(tptr->ts_lun)], lhp);
	SLIST_REMOVE(lhp, tptr, tstate, next);
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, tptr->owner, "destroyed tstate\n");
	xpt_free_path(tptr->owner);
	free(tptr, M_DEVBUF);
}

/*
 * Enable a lun.
 */
static void
isp_enable_lun(ispsoftc_t *isp, union ccb *ccb)
{
	tstate_t *tptr = NULL;
	int bus, tm_enabled, target_role;
	target_id_t target;
	lun_id_t lun;


	/*
	 * We only support either a wildcard target/lun or a target ID of zero and a non-wildcard lun
	 */
	bus = XS_CHANNEL(ccb);
	target = ccb->ccb_h.target_id;
	lun = ccb->ccb_h.target_lun;
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG0|ISP_LOGCONFIG, ccb->ccb_h.path, "enabling lun %u\n", lun);
	if (target != CAM_TARGET_WILDCARD && target != 0) {
		ccb->ccb_h.status = CAM_TID_INVALID;
		xpt_done(ccb);
		return;
	}
	if (target == CAM_TARGET_WILDCARD && lun != CAM_LUN_WILDCARD) {
		ccb->ccb_h.status = CAM_LUN_INVALID;
		xpt_done(ccb);
		return;
	}

	if (target != CAM_TARGET_WILDCARD && lun == CAM_LUN_WILDCARD) {
		ccb->ccb_h.status = CAM_LUN_INVALID;
		xpt_done(ccb);
		return;
	}
	if (isp->isp_dblev & ISP_LOGTDEBUG0) {
		xpt_print(ccb->ccb_h.path, "enabling lun 0x%x on channel %d\n", lun, bus);
	}

	/*
	 * Wait until we're not busy with the lun enables subsystem
	 */
	isp_tmlock(isp, "isp_enable_lun");

	/*
	 * This is as a good a place as any to check f/w capabilities.
	 */

	if (IS_FC(isp)) {
		if (ISP_CAP_TMODE(isp) == 0) {
			xpt_print(ccb->ccb_h.path, "firmware does not support target mode\n");
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			goto done;
		}
		/*
		 * We *could* handle non-SCCLUN f/w, but we'd have to
		 * dork with our already fragile enable/disable code.
		 */
		if (ISP_CAP_SCCFW(isp) == 0) {
			xpt_print(ccb->ccb_h.path, "firmware not SCCLUN capable\n");
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			goto done;
		}

		target_role = (FCPARAM(isp, bus)->role & ISP_ROLE_TARGET) != 0;

	} else {
		target_role = (SDPARAM(isp, bus)->role & ISP_ROLE_TARGET) != 0;
	}

	/*
	 * Create the state pointer.
	 * It should not already exist.
	 */
	tptr = get_lun_statep(isp, bus, lun);
	if (tptr) {
		ccb->ccb_h.status = CAM_LUN_ALRDY_ENA;
		goto done;
	}
	ccb->ccb_h.status = create_lun_state(isp, bus, ccb->ccb_h.path, &tptr);
	if (ccb->ccb_h.status != CAM_REQ_CMP) {
		goto done;
	}

	/*
	 * We have a tricky maneuver to perform here.
	 *
	 * If target mode isn't already enabled here,
	 * *and* our current role includes target mode,
	 * we enable target mode here.
	 *
	 */
	ISP_GET_PC(isp, bus, tm_enabled, tm_enabled);
	if (tm_enabled == 0 && target_role != 0) {
		if (isp_enable_target_mode(isp, bus)) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			destroy_lun_state(isp, tptr);
			tptr = NULL;
			goto done;
		}
		tm_enabled = 1;
	}

	/*
	 * Now check to see whether this bus is in target mode already.
	 *
	 * If not, a later role change into target mode will finish the job.
	 */
	if (tm_enabled == 0) {
		ISP_SET_PC(isp, bus, tm_enable_defer, 1);
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_print(ccb->ccb_h.path, "Target Mode not enabled yet- lun enable deferred\n");
		goto done1;
	}

	/*
	 * Enable the lun.
	 */
	ccb->ccb_h.status = isp_enable_deferred(isp, bus, lun);

done:
	if (ccb->ccb_h.status != CAM_REQ_CMP)  {
		if (tptr) {
			destroy_lun_state(isp, tptr);
			tptr = NULL;
		}
	} else {
		tptr->enabled = 1;
	}
done1:
	if (tptr) {
		rls_lun_statep(isp, tptr);
	}

	/*
	 * And we're outta here....
	 */
	isp_tmunlk(isp);
	xpt_done(ccb);
}

static cam_status
isp_enable_deferred_luns(ispsoftc_t *isp, int bus)
{
	tstate_t *tptr = NULL;
	struct tslist *lhp;
	int i, n;


	ISP_GET_PC(isp, bus, tm_enabled, i);
	if (i == 1) {
		return (CAM_REQ_CMP);
	}
	ISP_GET_PC(isp, bus, tm_enable_defer, i);
	if (i == 0) {
		return (CAM_REQ_CMP);
	}
	/*
	 * If this succeeds, it will set tm_enable
	 */
	if (isp_enable_target_mode(isp, bus)) {
		return (CAM_REQ_CMP_ERR);
	}
	isp_tmlock(isp, "isp_enable_deferred_luns");
	for (n = i = 0; i < LUN_HASH_SIZE; i++) {
		ISP_GET_PC_ADDR(isp, bus, lun_hash[i], lhp);
		SLIST_FOREACH(tptr, lhp, next) {
			tptr->hold++;
			if (tptr->enabled == 0) {
				if (isp_enable_deferred(isp, bus, tptr->ts_lun) == CAM_REQ_CMP) {
					tptr->enabled = 1;
					n++;
				}
			} else {
				n++;
			}
			tptr->hold--;
		}
	}
	isp_tmunlk(isp);
	if (n == 0) {
		return (CAM_REQ_CMP_ERR);
	}
	ISP_SET_PC(isp, bus, tm_enable_defer, 0);
	return (CAM_REQ_CMP);
}

static cam_status
isp_enable_deferred(ispsoftc_t *isp, int bus, lun_id_t lun)
{
	cam_status status;
	int luns_already_enabled;

	ISP_GET_PC(isp, bus, tm_luns_enabled, luns_already_enabled);
	isp_prt(isp, ISP_LOGTINFO, "%s: bus %d lun %u luns_enabled %d", __func__, bus, lun, luns_already_enabled);
	if (IS_24XX(isp) || (IS_FC(isp) && luns_already_enabled)) {
		status = CAM_REQ_CMP;
	} else {
		int cmd_cnt, not_cnt;

		if (IS_23XX(isp)) {
			cmd_cnt = DFLT_CMND_CNT;
			not_cnt = DFLT_INOT_CNT;
		} else {
			cmd_cnt = 64;
			not_cnt = 8;
		}
		status = CAM_REQ_INPROG;
		isp->isp_osinfo.rptr = &status;
		if (isp_lun_cmd(isp, RQSTYPE_ENABLE_LUN, bus, lun == CAM_LUN_WILDCARD? 0 : lun, cmd_cnt, not_cnt)) {
			status = CAM_RESRC_UNAVAIL;
		} else {
			mtx_sleep(&status, &isp->isp_lock, PRIBIO, "isp_enable_deferred", 0);
		}
		isp->isp_osinfo.rptr = NULL;
	}
	if (status == CAM_REQ_CMP) {
		ISP_SET_PC(isp, bus, tm_luns_enabled, 1);
		isp_prt(isp, ISP_LOGCONFIG|ISP_LOGTINFO, "bus %d lun %u now enabled for target mode", bus, lun);
	}
	return (status);
}

static void
isp_disable_lun(ispsoftc_t *isp, union ccb *ccb)
{
	tstate_t *tptr = NULL;
	int bus;
	cam_status status;
	target_id_t target;
	lun_id_t lun;

	bus = XS_CHANNEL(ccb);
	target = ccb->ccb_h.target_id;
	lun = ccb->ccb_h.target_lun;
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG0|ISP_LOGCONFIG, ccb->ccb_h.path, "disabling lun %u\n", lun);
	if (target != CAM_TARGET_WILDCARD && target != 0) {
		ccb->ccb_h.status = CAM_TID_INVALID;
		xpt_done(ccb);
		return;
	}

	if (target == CAM_TARGET_WILDCARD && lun != CAM_LUN_WILDCARD) {
		ccb->ccb_h.status = CAM_LUN_INVALID;
		xpt_done(ccb);
		return;
	}

	if (target != CAM_TARGET_WILDCARD && lun == CAM_LUN_WILDCARD) {
		ccb->ccb_h.status = CAM_LUN_INVALID;
		xpt_done(ccb);
		return;
	}

	/*
	 * See if we're busy disabling a lun now.
	 */
	isp_tmlock(isp, "isp_disable_lun");
	status = CAM_REQ_INPROG;

	/*
	 * Find the state pointer.
	 */
	if ((tptr = get_lun_statep(isp, bus, lun)) == NULL) {
		status = CAM_PATH_INVALID;
		goto done;
	}

	/*
	 * If we're a 24XX card, we're done.
	 */
	if (IS_24XX(isp)) {
		status = CAM_REQ_CMP;
		goto done;
	}

	/*
	 * For SCC FW, we only deal with lun zero.
	 */
	if (IS_FC(isp) && lun > 0) {
		status = CAM_REQ_CMP;
		goto done;
	}
	isp->isp_osinfo.rptr = &status;
	if (isp_lun_cmd(isp, RQSTYPE_ENABLE_LUN, bus, lun, 0, 0)) {
		status = CAM_RESRC_UNAVAIL;
	} else {
		mtx_sleep(ccb, &isp->isp_lock, PRIBIO, "isp_disable_lun", 0);
	}
	isp->isp_osinfo.rptr = NULL;
done:
	if (status == CAM_REQ_CMP) {
		tptr->enabled = 0;
		/*
		 * If we have no more luns enabled for this bus,
		 * delete all tracked wwns for it (if we are FC), 
		 * and disable target mode.
		 */
		if (is_any_lun_enabled(isp, bus) == 0) {
			isp_del_all_wwn_entries(isp, bus);
			if (isp_disable_target_mode(isp, bus)) {
				status = CAM_REQ_CMP_ERR;
			}
		}
	}
	ccb->ccb_h.status = status;
	if (status == CAM_REQ_CMP) {
		destroy_lun_state(isp, tptr);
		xpt_print(ccb->ccb_h.path, "lun now disabled for target mode\n");
	} else {
		if (tptr)
			rls_lun_statep(isp, tptr);
	}
	isp_tmunlk(isp);
	xpt_done(ccb);
}

static int
isp_enable_target_mode(ispsoftc_t *isp, int bus)
{
	int tm_enabled;

	ISP_GET_PC(isp, bus, tm_enabled, tm_enabled);
	if (tm_enabled != 0) {
		return (0);
	}
	if (IS_SCSI(isp)) {
		mbreg_t mbs;
		MBSINIT(&mbs, MBOX_ENABLE_TARGET_MODE, MBLOGALL, 0);
		mbs.param[0] = MBOX_ENABLE_TARGET_MODE;
		mbs.param[1] = ENABLE_TARGET_FLAG|ENABLE_TQING_FLAG;
		mbs.param[2] = bus << 7;
		if (isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs) < 0 || mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR, "Unable to enable Target Role on Bus %d", bus);
			return (EIO);
		}
	}
	ISP_SET_PC(isp, bus, tm_enabled, 1);
	isp_prt(isp, ISP_LOGINFO, "Target Role enabled on Bus %d", bus);
	return (0);
}

static int
isp_disable_target_mode(ispsoftc_t *isp, int bus)
{
	int tm_enabled;

	ISP_GET_PC(isp, bus, tm_enabled, tm_enabled);
	if (tm_enabled == 0) {
		return (0);
	}
	if (IS_SCSI(isp)) {
		mbreg_t mbs;
		MBSINIT(&mbs, MBOX_ENABLE_TARGET_MODE, MBLOGALL, 0);
		mbs.param[2] = bus << 7;
		if (isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs) < 0 || mbs.param[0] != MBOX_COMMAND_COMPLETE) {
			isp_prt(isp, ISP_LOGERR, "Unable to disable Target Role on Bus %d", bus);
			return (EIO);
		}
	}
	ISP_SET_PC(isp, bus, tm_enabled, 0);
	isp_prt(isp, ISP_LOGINFO, "Target Role disabled on Bus %d", bus);
	return (0);
}

static void
isp_ledone(ispsoftc_t *isp, lun_entry_t *lep)
{
	uint32_t *rptr;

	rptr = isp->isp_osinfo.rptr;
	if (lep->le_status != LUN_OK) {
		isp_prt(isp, ISP_LOGERR, "ENABLE/MODIFY LUN returned 0x%x", lep->le_status);
		if (rptr) {
			*rptr = CAM_REQ_CMP_ERR;
			wakeup_one(rptr);
		}
	} else {
		if (rptr) {
			*rptr = CAM_REQ_CMP;
			wakeup_one(rptr);
		}
	}
}

static void
isp_target_start_ctio(ispsoftc_t *isp, union ccb *ccb, enum Start_Ctio_How how)
{
	int fctape, sendstatus, resid;
	tstate_t *tptr;
	fcparam *fcp;
	atio_private_data_t *atp;
	struct ccb_scsiio *cso;
	uint32_t dmaresult, handle, xfrlen, sense_length, tmp;
	uint8_t local[QENTRY_LEN];

	tptr = get_lun_statep(isp, XS_CHANNEL(ccb), XS_LUN(ccb));
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, XS_CHANNEL(ccb), CAM_LUN_WILDCARD);
		if (tptr == NULL) {
			isp_prt(isp, ISP_LOGERR, "%s: [0x%x] cannot find tstate pointer", __func__, ccb->csio.tag_id);
			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
			xpt_done(ccb);
			return;
		}
	}
	isp_prt(isp, ISP_LOGTDEBUG0, "%s: ENTRY[0x%x] how %u xfrlen %u sendstatus %d sense_len %u", __func__, ccb->csio.tag_id, how, ccb->csio.dxfer_len,
	    (ccb->ccb_h.flags & CAM_SEND_STATUS) != 0, ((ccb->ccb_h.flags & CAM_SEND_SENSE)? ccb->csio.sense_len : 0));

	switch (how) {
	case FROM_TIMER:
	case FROM_CAM:
		/*
		 * Insert at the tail of the list, if any, waiting CTIO CCBs
		 */
		TAILQ_INSERT_TAIL(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
		break;
	case FROM_SRR:
	case FROM_CTIO_DONE:
		TAILQ_INSERT_HEAD(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
		break;
	}

	while (TAILQ_FIRST(&tptr->waitq) != NULL) {
		ccb = (union ccb *) TAILQ_FIRST(&tptr->waitq);
		TAILQ_REMOVE(&tptr->waitq, &ccb->ccb_h, periph_links.tqe);

		cso = &ccb->csio;
		xfrlen = cso->dxfer_len;
		if (xfrlen == 0) {
			if ((ccb->ccb_h.flags & CAM_SEND_STATUS) == 0) {
				ISP_PATH_PRT(isp, ISP_LOGERR, ccb->ccb_h.path, "a data transfer length of zero but no status to send is wrong\n");
				ccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(ccb);
				continue;
			}
		}

		atp = isp_find_atpd(isp, tptr, cso->tag_id);
		if (atp == NULL) {
			isp_prt(isp, ISP_LOGERR, "%s: [0x%x] cannot find private data adjunct in %s", __func__, cso->tag_id, __func__);
			isp_dump_atpd(isp, tptr);
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			xpt_done(ccb);
			continue;
		}

		/*
		 * Is this command a dead duck?
		 */
		if (atp->dead) {
			isp_prt(isp, ISP_LOGERR, "%s: [0x%x] not sending a CTIO for a dead command", __func__, cso->tag_id);
			ccb->ccb_h.status = CAM_REQ_ABORTED;
			xpt_done(ccb);
			continue;
		}

		/*
		 * Check to make sure we're still in target mode.
		 */
		fcp = FCPARAM(isp, XS_CHANNEL(ccb));
		if ((fcp->role & ISP_ROLE_TARGET) == 0) {
			isp_prt(isp, ISP_LOGERR, "%s: [0x%x] stopping sending a CTIO because we're no longer in target mode", __func__, cso->tag_id);
			ccb->ccb_h.status = CAM_PROVIDE_FAIL;
			xpt_done(ccb);
			continue;
		}

		/*
		 * We're only handling ATPD_CCB_OUTSTANDING outstanding CCB at a time (one of which
		 * could be split into two CTIOs to split data and status).
		 */
		if (atp->ctcnt >= ATPD_CCB_OUTSTANDING) {
			isp_prt(isp, ISP_LOGTINFO, "[0x%x] handling only %d CCBs at a time (flags for this ccb: 0x%x)", cso->tag_id, ATPD_CCB_OUTSTANDING, ccb->ccb_h.flags);
			TAILQ_INSERT_HEAD(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
			break;
		}

		/*
		 * Does the initiator expect FC-Tape style responses?
		 */
		if ((atp->word3 & PRLI_WD3_RETRY) && fcp->fctape_enabled) {
			fctape = 1;
		} else {
			fctape = 0;
		}

		/*
		 * If we already did the data xfer portion of a CTIO that sends data
		 * and status, don't do it again and do the status portion now.
		 */
		if (atp->sendst) {
			isp_prt(isp, ISP_LOGTINFO, "[0x%x] now sending synthesized status orig_dl=%u xfered=%u bit=%u",
			    cso->tag_id, atp->orig_datalen, atp->bytes_xfered, atp->bytes_in_transit);
			xfrlen = 0;	/* we already did the data transfer */
			atp->sendst = 0;
		}
		if (ccb->ccb_h.flags & CAM_SEND_STATUS) {
			sendstatus = 1;
		} else {
			sendstatus = 0;
		}

		if (ccb->ccb_h.flags & CAM_SEND_SENSE) {
			KASSERT((sendstatus != 0), ("how can you have CAM_SEND_SENSE w/o CAM_SEND_STATUS?"));
			/*
			 * Sense length is not the entire sense data structure size. Periph
			 * drivers don't seem to be setting sense_len to reflect the actual
			 * size. We'll peek inside to get the right amount.
			 */
			sense_length = cso->sense_len;

			/*
			 * This 'cannot' happen
			 */
			if (sense_length > (XCMD_SIZE - MIN_FCP_RESPONSE_SIZE)) {
				sense_length = XCMD_SIZE - MIN_FCP_RESPONSE_SIZE;
			}
		} else {
			sense_length = 0;
		}

		memset(local, 0, QENTRY_LEN);

		/*
		 * Check for overflow
		 */
		tmp = atp->bytes_xfered + atp->bytes_in_transit + xfrlen;
		if (tmp > atp->orig_datalen) {
			isp_prt(isp, ISP_LOGERR, "%s: [0x%x] data overflow by %u bytes", __func__, cso->tag_id, tmp - atp->orig_datalen);
			ccb->ccb_h.status = CAM_DATA_RUN_ERR;
			xpt_done(ccb);
			continue;
		}

		if (IS_24XX(isp)) {
			ct7_entry_t *cto = (ct7_entry_t *) local;

			cto->ct_header.rqs_entry_type = RQSTYPE_CTIO7;
			cto->ct_header.rqs_entry_count = 1;
			cto->ct_header.rqs_seqno |= ATPD_SEQ_NOTIFY_CAM;
			ATPD_SET_SEQNO(cto, atp);
			cto->ct_nphdl = atp->nphdl;
			cto->ct_rxid = atp->tag;
			cto->ct_iid_lo = atp->portid;
			cto->ct_iid_hi = atp->portid >> 16;
			cto->ct_oxid = atp->oxid;
			cto->ct_vpidx = ISP_GET_VPIDX(isp, XS_CHANNEL(ccb));
			cto->ct_timeout = 120;
			cto->ct_flags = atp->tattr << CT7_TASK_ATTR_SHIFT;

			/*
			 * Mode 1, status, no data. Only possible when we are sending status, have
			 * no data to transfer, and any sense data can fit into a ct7_entry_t.
			 *
			 * Mode 2, status, no data. We have to use this in the case that
			 * the sense data won't fit into a ct7_entry_t.
			 *
			 */
			if (sendstatus && xfrlen == 0) {
				cto->ct_flags |= CT7_SENDSTATUS | CT7_NO_DATA;
				resid = atp->orig_datalen - atp->bytes_xfered - atp->bytes_in_transit;
				if (sense_length <= MAXRESPLEN_24XX) {
					if (resid < 0) {
						cto->ct_resid = -resid;
					} else if (resid > 0) {
						cto->ct_resid = resid;
					}
					cto->ct_flags |= CT7_FLAG_MODE1;
					cto->ct_scsi_status = cso->scsi_status;
					if (resid < 0) {
						cto->ct_scsi_status |= (FCP_RESID_OVERFLOW << 8);
					} else if (resid > 0) {
						cto->ct_scsi_status |= (FCP_RESID_UNDERFLOW << 8);
					}
					if (fctape) {
						cto->ct_flags |= CT7_CONFIRM|CT7_EXPLCT_CONF;
					}
					if (sense_length) {
						cto->ct_scsi_status |= (FCP_SNSLEN_VALID << 8);
						cto->rsp.m1.ct_resplen = cto->ct_senselen = sense_length;
						memcpy(cto->rsp.m1.ct_resp, &cso->sense_data, sense_length);
					}
				} else {
					bus_addr_t addr;
					char buf[XCMD_SIZE];
					fcp_rsp_iu_t *rp;

					if (atp->ests == NULL) {
						atp->ests = isp_get_ecmd(isp);
						if (atp->ests == NULL) {
							TAILQ_INSERT_HEAD(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
							break;
						}
					}
					memset(buf, 0, sizeof (buf));
					rp = (fcp_rsp_iu_t *)buf;
					if (fctape) {
						cto->ct_flags |= CT7_CONFIRM|CT7_EXPLCT_CONF;
						rp->fcp_rsp_bits |= FCP_CONF_REQ;
					}
					cto->ct_flags |= CT7_FLAG_MODE2;
	        			rp->fcp_rsp_scsi_status = cso->scsi_status;
					if (resid < 0) {
						rp->fcp_rsp_resid = -resid;
						rp->fcp_rsp_bits |= FCP_RESID_OVERFLOW;
					} else if (resid > 0) {
						rp->fcp_rsp_resid = resid;
						rp->fcp_rsp_bits |= FCP_RESID_UNDERFLOW;
					}
					if (sense_length) {
	        				rp->fcp_rsp_snslen = sense_length;
						cto->ct_senselen = sense_length;
						rp->fcp_rsp_bits |= FCP_SNSLEN_VALID;
						isp_put_fcp_rsp_iu(isp, rp, atp->ests);
						memcpy(((fcp_rsp_iu_t *)atp->ests)->fcp_rsp_extra, &cso->sense_data, sense_length);
					} else {
						isp_put_fcp_rsp_iu(isp, rp, atp->ests);
					}
					if (isp->isp_dblev & ISP_LOGTDEBUG1) {
						isp_print_bytes(isp, "FCP Response Frame After Swizzling", MIN_FCP_RESPONSE_SIZE + sense_length, atp->ests);
					}
					addr = isp->isp_osinfo.ecmd_dma;
					addr += ((((isp_ecmd_t *)atp->ests) - isp->isp_osinfo.ecmd_base) * XCMD_SIZE);
					isp_prt(isp, ISP_LOGTDEBUG0, "%s: ests base %p vaddr %p ecmd_dma %jx addr %jx len %u", __func__, isp->isp_osinfo.ecmd_base, atp->ests,
					    (uintmax_t) isp->isp_osinfo.ecmd_dma, (uintmax_t)addr, MIN_FCP_RESPONSE_SIZE + sense_length);
					cto->rsp.m2.ct_datalen = MIN_FCP_RESPONSE_SIZE + sense_length;
					cto->rsp.m2.ct_fcp_rsp_iudata.ds_base = DMA_LO32(addr);
					cto->rsp.m2.ct_fcp_rsp_iudata.ds_basehi = DMA_HI32(addr);
					cto->rsp.m2.ct_fcp_rsp_iudata.ds_count = MIN_FCP_RESPONSE_SIZE + sense_length;
				}
				if (sense_length) {
					isp_prt(isp, ISP_LOGTDEBUG0, "%s: CTIO7[0x%x] seq %u nc %d CDB0=%x sstatus=0x%x flags=0x%x resid=%d slen %u sense: %x %x/%x/%x", __func__,
					    cto->ct_rxid, ATPD_GET_SEQNO(cto), ATPD_GET_NCAM(cto), atp->cdb0, cto->ct_scsi_status, cto->ct_flags, cto->ct_resid, sense_length,
					    cso->sense_data.error_code, cso->sense_data.sense_buf[1], cso->sense_data.sense_buf[11], cso->sense_data.sense_buf[12]);
				} else {
					isp_prt(isp, ISP_LOGDEBUG0, "%s: CTIO7[0x%x] seq %u nc %d CDB0=%x sstatus=0x%x flags=0x%x resid=%d", __func__,
					    cto->ct_rxid, ATPD_GET_SEQNO(cto), ATPD_GET_NCAM(cto), atp->cdb0, cto->ct_scsi_status, cto->ct_flags, cto->ct_resid);
				}
				atp->state = ATPD_STATE_LAST_CTIO;
			}

			/*
			 * Mode 0 data transfers, *possibly* with status.
			 */
			if (xfrlen != 0) {
				cto->ct_flags |= CT7_FLAG_MODE0;
				if ((cso->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
					cto->ct_flags |= CT7_DATA_IN;
				} else {
					cto->ct_flags |= CT7_DATA_OUT;
				}

				cto->rsp.m0.reloff = atp->bytes_xfered + atp->bytes_in_transit;
				cto->rsp.m0.ct_xfrlen = xfrlen;

#ifdef	DEBUG
				if (ISP_FC_PC(isp, XS_CHANNEL(ccb))->inject_lost_data_frame && xfrlen > ISP_FC_PC(isp, XS_CHANNEL(ccb))->inject_lost_data_frame) {
					isp_prt(isp, ISP_LOGWARN, "%s: truncating data frame with xfrlen %d to %d", __func__, xfrlen, xfrlen - (xfrlen >> 2));
					ISP_FC_PC(isp, XS_CHANNEL(ccb))->inject_lost_data_frame = 0;
					cto->rsp.m0.ct_xfrlen -= xfrlen >> 2;
				}
#endif
				if (sendstatus) {
					resid = atp->orig_datalen - atp->bytes_xfered - xfrlen;
					if (cso->scsi_status == SCSI_STATUS_OK && resid == 0 /* && fctape == 0 */) {
						cto->ct_flags |= CT7_SENDSTATUS;
						atp->state = ATPD_STATE_LAST_CTIO;
						if (fctape) {
							cto->ct_flags |= CT7_CONFIRM|CT7_EXPLCT_CONF;
						}
					} else {
						atp->sendst = 1;	/* send status later */
						cto->ct_header.rqs_seqno &= ~ATPD_SEQ_NOTIFY_CAM;
						atp->state = ATPD_STATE_CTIO;
					}
				} else {
					atp->state = ATPD_STATE_CTIO;
				}
				isp_prt(isp, ISP_LOGTDEBUG0, "%s: CTIO7[0x%x] seq %u nc %d CDB0=%x sstatus=0x%x flags=0x%x xfrlen=%u off=%u", __func__,
				    cto->ct_rxid, ATPD_GET_SEQNO(cto), ATPD_GET_NCAM(cto), atp->cdb0, cto->ct_scsi_status, cto->ct_flags, xfrlen, atp->bytes_xfered);
			}
		} else if (IS_FC(isp)) {
			ct2_entry_t *cto = (ct2_entry_t *) local;

			if (isp->isp_osinfo.sixtyfourbit)
				cto->ct_header.rqs_entry_type = RQSTYPE_CTIO3;
			else
				cto->ct_header.rqs_entry_type = RQSTYPE_CTIO2;
			cto->ct_header.rqs_entry_count = 1;
			cto->ct_header.rqs_seqno |= ATPD_SEQ_NOTIFY_CAM;
			ATPD_SET_SEQNO(cto, atp);
			if (ISP_CAP_2KLOGIN(isp) == 0) {
				((ct2e_entry_t *)cto)->ct_iid = cso->init_id;
			} else {
				cto->ct_iid = cso->init_id;
				if (ISP_CAP_SCCFW(isp) == 0) {
					cto->ct_lun = ccb->ccb_h.target_lun;
				}
			}
			cto->ct_timeout = 10;
			cto->ct_rxid = cso->tag_id;

			/*
			 * Mode 1, status, no data. Only possible when we are sending status, have
			 * no data to transfer, and the sense length can fit in the ct7_entry.
			 *
			 * Mode 2, status, no data. We have to use this in the case the response
			 * length won't fit into a ct2_entry_t.
			 *
			 * We'll fill out this structure with information as if this were a
			 * Mode 1. The hardware layer will create the Mode 2 FCP RSP IU as
			 * needed based upon this.
			 */
			if (sendstatus && xfrlen == 0) {
				cto->ct_flags |= CT2_SENDSTATUS | CT2_NO_DATA;
				resid = atp->orig_datalen - atp->bytes_xfered - atp->bytes_in_transit;
				if (sense_length <= MAXRESPLEN) {
					if (resid < 0) {
						cto->ct_resid = -resid;
					} else if (resid > 0) {
						cto->ct_resid = resid;
					}
					cto->ct_flags |= CT2_FLAG_MODE1;
					cto->rsp.m1.ct_scsi_status = cso->scsi_status;
					if (resid < 0) {
						cto->rsp.m1.ct_scsi_status |= CT2_DATA_OVER;
					} else if (resid > 0) {
						cto->rsp.m1.ct_scsi_status |= CT2_DATA_UNDER;
					}
					if (fctape) {
						cto->ct_flags |= CT2_CONFIRM;
					}
					if (sense_length) {
						cto->rsp.m1.ct_scsi_status |= CT2_SNSLEN_VALID;
						cto->rsp.m1.ct_resplen = cto->rsp.m1.ct_senselen = sense_length;
						memcpy(cto->rsp.m1.ct_resp, &cso->sense_data, sense_length);
					}
				} else {
					bus_addr_t addr;
					char buf[XCMD_SIZE];
					fcp_rsp_iu_t *rp;

					if (atp->ests == NULL) {
						atp->ests = isp_get_ecmd(isp);
						if (atp->ests == NULL) {
							TAILQ_INSERT_HEAD(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
							break;
						}
					}
					memset(buf, 0, sizeof (buf));
					rp = (fcp_rsp_iu_t *)buf;
					if (fctape) {
						cto->ct_flags |= CT2_CONFIRM;
						rp->fcp_rsp_bits |= FCP_CONF_REQ;
					}
					cto->ct_flags |= CT2_FLAG_MODE2;
	        			rp->fcp_rsp_scsi_status = cso->scsi_status;
					if (resid < 0) {
						rp->fcp_rsp_resid = -resid;
						rp->fcp_rsp_bits |= FCP_RESID_OVERFLOW;
					} else if (resid > 0) {
						rp->fcp_rsp_resid = resid;
						rp->fcp_rsp_bits |= FCP_RESID_UNDERFLOW;
					}
					if (sense_length) {
	        				rp->fcp_rsp_snslen = sense_length;
						rp->fcp_rsp_bits |= FCP_SNSLEN_VALID;
						isp_put_fcp_rsp_iu(isp, rp, atp->ests);
						memcpy(((fcp_rsp_iu_t *)atp->ests)->fcp_rsp_extra, &cso->sense_data, sense_length);
					} else {
						isp_put_fcp_rsp_iu(isp, rp, atp->ests);
					}
					if (isp->isp_dblev & ISP_LOGTDEBUG1) {
						isp_print_bytes(isp, "FCP Response Frame After Swizzling", MIN_FCP_RESPONSE_SIZE + sense_length, atp->ests);
					}
					addr = isp->isp_osinfo.ecmd_dma;
					addr += ((((isp_ecmd_t *)atp->ests) - isp->isp_osinfo.ecmd_base) * XCMD_SIZE);
					isp_prt(isp, ISP_LOGTDEBUG0, "%s: ests base %p vaddr %p ecmd_dma %jx addr %jx len %u", __func__, isp->isp_osinfo.ecmd_base, atp->ests,
					    (uintmax_t) isp->isp_osinfo.ecmd_dma, (uintmax_t)addr, MIN_FCP_RESPONSE_SIZE + sense_length);
					cto->rsp.m2.ct_datalen = MIN_FCP_RESPONSE_SIZE + sense_length;
					if (isp->isp_osinfo.sixtyfourbit) {
						cto->rsp.m2.u.ct_fcp_rsp_iudata_64.ds_base = DMA_LO32(addr);
						cto->rsp.m2.u.ct_fcp_rsp_iudata_64.ds_basehi = DMA_HI32(addr);
						cto->rsp.m2.u.ct_fcp_rsp_iudata_64.ds_count = MIN_FCP_RESPONSE_SIZE + sense_length;
					} else {
						cto->rsp.m2.u.ct_fcp_rsp_iudata_32.ds_base = DMA_LO32(addr);
						cto->rsp.m2.u.ct_fcp_rsp_iudata_32.ds_count = MIN_FCP_RESPONSE_SIZE + sense_length;
					}
				}
				if (sense_length) {
					isp_prt(isp, ISP_LOGTDEBUG0, "%s: CTIO2[0x%x] seq %u nc %d CDB0=%x sstatus=0x%x flags=0x%x resid=%d sense: %x %x/%x/%x", __func__,
					    cto->ct_rxid, ATPD_GET_SEQNO(cto), ATPD_GET_NCAM(cto), atp->cdb0, cso->scsi_status, cto->ct_flags, cto->ct_resid,
					    cso->sense_data.error_code, cso->sense_data.sense_buf[1], cso->sense_data.sense_buf[11], cso->sense_data.sense_buf[12]);
				} else {
					isp_prt(isp, ISP_LOGTDEBUG0, "%s: CTIO2[0x%x] seq %u nc %d CDB0=%x sstatus=0x%x flags=0x%x resid=%d", __func__, cto->ct_rxid,
					    ATPD_GET_SEQNO(cto), ATPD_GET_NCAM(cto), atp->cdb0, cso->scsi_status, cto->ct_flags, cto->ct_resid);
				}
				atp->state = ATPD_STATE_LAST_CTIO;
			}

			if (xfrlen != 0) {
				cto->ct_flags |= CT2_FLAG_MODE0;
				if ((cso->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
					cto->ct_flags |= CT2_DATA_IN;
				} else {
					cto->ct_flags |= CT2_DATA_OUT;
				}

				cto->ct_reloff = atp->bytes_xfered + atp->bytes_in_transit;
				cto->rsp.m0.ct_xfrlen = xfrlen;

				if (sendstatus) {
					resid = atp->orig_datalen - atp->bytes_xfered - xfrlen;
					if (cso->scsi_status == SCSI_STATUS_OK && resid == 0 /*&& fctape == 0*/) {
						cto->ct_flags |= CT2_SENDSTATUS;
						atp->state = ATPD_STATE_LAST_CTIO;
						if (fctape) {
							cto->ct_flags |= CT2_CONFIRM;
						}
					} else {
						atp->sendst = 1;	/* send status later */
						cto->ct_header.rqs_seqno &= ~ATPD_SEQ_NOTIFY_CAM;
						atp->state = ATPD_STATE_CTIO;
					}
				} else {
					atp->state = ATPD_STATE_CTIO;
				}
			}
			isp_prt(isp, ISP_LOGTDEBUG0, "%s: CTIO2[%x] seq %u nc %d CDB0=%x scsi status %x flags %x resid %d xfrlen %u offset %u", __func__, cto->ct_rxid,
			    ATPD_GET_SEQNO(cto), ATPD_GET_NCAM(cto), atp->cdb0, cso->scsi_status, cto->ct_flags, cto->ct_resid, cso->dxfer_len, atp->bytes_xfered);
		} else {
			ct_entry_t *cto = (ct_entry_t *) local;

			cto->ct_header.rqs_entry_type = RQSTYPE_CTIO;
			cto->ct_header.rqs_entry_count = 1;
			cto->ct_header.rqs_seqno |= ATPD_SEQ_NOTIFY_CAM;
			ATPD_SET_SEQNO(cto, atp);
			cto->ct_iid = cso->init_id;
			cto->ct_iid |= XS_CHANNEL(ccb) << 7;
			cto->ct_tgt = ccb->ccb_h.target_id;
			cto->ct_lun = ccb->ccb_h.target_lun;
			cto->ct_fwhandle = cso->tag_id;
			if (atp->rxid) {
				cto->ct_tag_val = atp->rxid;
				cto->ct_flags |= CT_TQAE;
			}
			if (ccb->ccb_h.flags & CAM_DIS_DISCONNECT) {
				cto->ct_flags |= CT_NODISC;
			}
			if (cso->dxfer_len == 0) {
				cto->ct_flags |= CT_NO_DATA;
			} else if ((cso->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
				cto->ct_flags |= CT_DATA_IN;
			} else {
				cto->ct_flags |= CT_DATA_OUT;
			}
			if (ccb->ccb_h.flags & CAM_SEND_STATUS) {
				cto->ct_flags |= CT_SENDSTATUS|CT_CCINCR;
				cto->ct_scsi_status = cso->scsi_status;
				cto->ct_resid = atp->orig_datalen - atp->bytes_xfered - atp->bytes_in_transit - xfrlen;
				isp_prt(isp, ISP_LOGTDEBUG0, "%s: CTIO[%x] seq %u nc %d scsi status %x resid %d tag_id %x", __func__,
				    cto->ct_fwhandle, ATPD_GET_SEQNO(cto), ATPD_GET_NCAM(cto), cso->scsi_status, cso->resid, cso->tag_id);
			}
			ccb->ccb_h.flags &= ~CAM_SEND_SENSE;
			cto->ct_timeout = 10;
		}

		if (isp_get_pcmd(isp, ccb)) {
			ISP_PATH_PRT(isp, ISP_LOGWARN, ccb->ccb_h.path, "out of PCMDs\n");
			TAILQ_INSERT_HEAD(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
			break;
		}
		if (isp_allocate_xs_tgt(isp, ccb, &handle)) {
			ISP_PATH_PRT(isp, ISP_LOGWARN, ccb->ccb_h.path, "No XFLIST pointers for %s\n", __func__);
			TAILQ_INSERT_HEAD(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
			isp_free_pcmd(isp, ccb);
			break;
		}
		atp->bytes_in_transit += xfrlen;
		PISP_PCMD(ccb)->datalen = xfrlen;


		/*
		 * Call the dma setup routines for this entry (and any subsequent
		 * CTIOs) if there's data to move, and then tell the f/w it's got
		 * new things to play with. As with isp_start's usage of DMA setup,
		 * any swizzling is done in the machine dependent layer. Because
		 * of this, we put the request onto the queue area first in native
		 * format.
		 */

		if (IS_24XX(isp)) {
			ct7_entry_t *cto = (ct7_entry_t *) local;
			cto->ct_syshandle = handle;
		} else if (IS_FC(isp)) {
			ct2_entry_t *cto = (ct2_entry_t *) local;
			cto->ct_syshandle = handle;
		} else {
			ct_entry_t *cto = (ct_entry_t *) local;
			cto->ct_syshandle = handle;
		}

		dmaresult = ISP_DMASETUP(isp, cso, (ispreq_t *) local);
		if (dmaresult != CMD_QUEUED) {
			isp_destroy_tgt_handle(isp, handle);
			isp_free_pcmd(isp, ccb);
			if (dmaresult == CMD_EAGAIN) {
				TAILQ_INSERT_HEAD(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
				break;
			}
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			xpt_done(ccb);
			continue;
		}
		isp->isp_nactive++;
		ccb->ccb_h.status = CAM_REQ_INPROG | CAM_SIM_QUEUED;
		if (xfrlen) {
			ccb->ccb_h.spriv_field0 = atp->bytes_xfered;
		} else {
			ccb->ccb_h.spriv_field0 = ~0;
		}
		atp->ctcnt++;
		atp->seqno++;
	}
	rls_lun_statep(isp, tptr);
}

static void
isp_refire_putback_atio(void *arg)
{
	union ccb *ccb = arg;

	ISP_ASSERT_LOCKED((ispsoftc_t *)XS_ISP(ccb));
	isp_target_putback_atio(ccb);
}

static void
isp_refire_notify_ack(void *arg)
{
	isp_tna_t *tp  = arg;
	ispsoftc_t *isp = tp->isp;

	ISP_ASSERT_LOCKED(isp);
	if (isp_notify_ack(isp, tp->not)) {
		callout_schedule(&tp->timer, 5);
	} else {
		free(tp, M_DEVBUF);
	}
}


static void
isp_target_putback_atio(union ccb *ccb)
{
	ispsoftc_t *isp;
	struct ccb_scsiio *cso;
	void *qe;

	isp = XS_ISP(ccb);

	qe = isp_getrqentry(isp);
	if (qe == NULL) {
		xpt_print(ccb->ccb_h.path,
		    "%s: Request Queue Overflow\n", __func__);
		callout_reset(&PISP_PCMD(ccb)->wdog, 10,
		    isp_refire_putback_atio, ccb);
		return;
	}
	memset(qe, 0, QENTRY_LEN);
	cso = &ccb->csio;
	if (IS_FC(isp)) {
		at2_entry_t local, *at = &local;
		ISP_MEMZERO(at, sizeof (at2_entry_t));
		at->at_header.rqs_entry_type = RQSTYPE_ATIO2;
		at->at_header.rqs_entry_count = 1;
		if (ISP_CAP_SCCFW(isp)) {
			at->at_scclun = (uint16_t) ccb->ccb_h.target_lun;
		} else {
			at->at_lun = (uint8_t) ccb->ccb_h.target_lun;
		}
		at->at_status = CT_OK;
		at->at_rxid = cso->tag_id;
		at->at_iid = cso->ccb_h.target_id;
		isp_put_atio2(isp, at, qe);
	} else {
		at_entry_t local, *at = &local;
		ISP_MEMZERO(at, sizeof (at_entry_t));
		at->at_header.rqs_entry_type = RQSTYPE_ATIO;
		at->at_header.rqs_entry_count = 1;
		at->at_iid = cso->init_id;
		at->at_iid |= XS_CHANNEL(ccb) << 7;
		at->at_tgt = cso->ccb_h.target_id;
		at->at_lun = cso->ccb_h.target_lun;
		at->at_status = CT_OK;
		at->at_tag_val = AT_GET_TAG(cso->tag_id);
		at->at_handle = AT_GET_HANDLE(cso->tag_id);
		isp_put_atio(isp, at, qe);
	}
	ISP_TDQE(isp, "isp_target_putback_atio", isp->isp_reqidx, qe);
	ISP_SYNC_REQUEST(isp);
	isp_complete_ctio(ccb);
}

static void
isp_complete_ctio(union ccb *ccb)
{
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_INPROG) {
		ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
		xpt_done(ccb);
	}
}

/*
 * Handle ATIO stuff that the generic code can't.
 * This means handling CDBs.
 */

static void
isp_handle_platform_atio(ispsoftc_t *isp, at_entry_t *aep)
{
	tstate_t *tptr;
	int status, bus;
	struct ccb_accept_tio *atiop;
	atio_private_data_t *atp;

	/*
	 * The firmware status (except for the QLTM_SVALID bit)
	 * indicates why this ATIO was sent to us.
	 *
	 * If QLTM_SVALID is set, the firmware has recommended Sense Data.
	 *
	 * If the DISCONNECTS DISABLED bit is set in the flags field,
	 * we're still connected on the SCSI bus.
	 */
	status = aep->at_status;
	if ((status & ~QLTM_SVALID) == AT_PHASE_ERROR) {
		/*
		 * Bus Phase Sequence error. We should have sense data
		 * suggested by the f/w. I'm not sure quite yet what
		 * to do about this for CAM.
		 */
		isp_prt(isp, ISP_LOGWARN, "PHASE ERROR");
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return;
	}
	if ((status & ~QLTM_SVALID) != AT_CDB) {
		isp_prt(isp, ISP_LOGWARN, "bad atio (0x%x) leaked to platform", status);
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return;
	}

	bus = GET_BUS_VAL(aep->at_iid);
	tptr = get_lun_statep(isp, bus, aep->at_lun);
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, bus, CAM_LUN_WILDCARD);
		if (tptr == NULL) {
			/*
			 * Because we can't autofeed sense data back with
			 * a command for parallel SCSI, we can't give back
			 * a CHECK CONDITION. We'll give back a BUSY status
			 * instead. This works out okay because the only
			 * time we should, in fact, get this, is in the
			 * case that somebody configured us without the
			 * blackhole driver, so they get what they deserve.
			 */
			isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
			return;
		}
	}

	atp = isp_get_atpd(isp, tptr, aep->at_handle);
	atiop = (struct ccb_accept_tio *) SLIST_FIRST(&tptr->atios);
	if (atiop == NULL || atp == NULL) {
		/*
		 * Because we can't autofeed sense data back with
		 * a command for parallel SCSI, we can't give back
		 * a CHECK CONDITION. We'll give back a QUEUE FULL status
		 * instead. This works out okay because the only time we
		 * should, in fact, get this, is in the case that we've
		 * run out of ATIOS.
		 */
		xpt_print(tptr->owner, "no %s for lun %d from initiator %d\n", (atp == NULL && atiop == NULL)? "ATIOs *or* ATPS" :
		    ((atp == NULL)? "ATPs" : "ATIOs"), aep->at_lun, aep->at_iid);
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		if (atp) {
			isp_put_atpd(isp, tptr, atp);
		}
		rls_lun_statep(isp, tptr);
		return;
	}
	atp->rxid = aep->at_tag_val;
	atp->state = ATPD_STATE_ATIO;
	SLIST_REMOVE_HEAD(&tptr->atios, sim_links.sle);
	tptr->atio_count--;
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG2, atiop->ccb_h.path, "Take FREE ATIO count now %d\n", tptr->atio_count);
	atiop->ccb_h.target_id = aep->at_tgt;
	atiop->ccb_h.target_lun = aep->at_lun;
	if (aep->at_flags & AT_NODISC) {
		atiop->ccb_h.flags |= CAM_DIS_DISCONNECT;
	} else {
		atiop->ccb_h.flags &= ~CAM_DIS_DISCONNECT;
	}

	if (status & QLTM_SVALID) {
		size_t amt = ISP_MIN(QLTM_SENSELEN, sizeof (atiop->sense_data));
		atiop->sense_len = amt;
		ISP_MEMCPY(&atiop->sense_data, aep->at_sense, amt);
	} else {
		atiop->sense_len = 0;
	}

	atiop->init_id = GET_IID_VAL(aep->at_iid);
	atiop->cdb_len = aep->at_cdblen;
	ISP_MEMCPY(atiop->cdb_io.cdb_bytes, aep->at_cdb, aep->at_cdblen);
	atiop->ccb_h.status = CAM_CDB_RECVD;
	/*
	 * Construct a tag 'id' based upon tag value (which may be 0..255)
	 * and the handle (which we have to preserve).
	 */
	atiop->tag_id = atp->tag;
	if (aep->at_flags & AT_TQAE) {
		atiop->tag_action = aep->at_tag_type;
		atiop->ccb_h.status |= CAM_TAG_ACTION_VALID;
	}
	atp->orig_datalen = 0;
	atp->bytes_xfered = 0;
	atp->lun = aep->at_lun;
	atp->nphdl = aep->at_iid;
	atp->portid = PORT_NONE;
	atp->oxid = 0;
	atp->cdb0 = atiop->cdb_io.cdb_bytes[0];
	atp->tattr = aep->at_tag_type;
	atp->state = ATPD_STATE_CAM;
	isp_prt(isp, ISP_LOGTDEBUG0, "ATIO[0x%x] CDB=0x%x lun %d", aep->at_tag_val, atp->cdb0, atp->lun);
	rls_lun_statep(isp, tptr);
}

static void
isp_handle_platform_atio2(ispsoftc_t *isp, at2_entry_t *aep)
{
	lun_id_t lun;
	fcportdb_t *lp;
	tstate_t *tptr;
	struct ccb_accept_tio *atiop;
	uint16_t nphdl;
	atio_private_data_t *atp;
	inot_private_data_t *ntp;

	/*
	 * The firmware status (except for the QLTM_SVALID bit)
	 * indicates why this ATIO was sent to us.
	 *
	 * If QLTM_SVALID is set, the firmware has recommended Sense Data.
	 */
	if ((aep->at_status & ~QLTM_SVALID) != AT_CDB) {
		isp_prt(isp, ISP_LOGWARN, "bogus atio (0x%x) leaked to platform", aep->at_status);
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return;
	}

	if (ISP_CAP_SCCFW(isp)) {
		lun = aep->at_scclun;
	} else {
		lun = aep->at_lun;
	}
	if (ISP_CAP_2KLOGIN(isp)) {
		nphdl = ((at2e_entry_t *)aep)->at_iid;
	} else {
		nphdl = aep->at_iid;
	}
	tptr = get_lun_statep(isp, 0, lun);
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, 0, CAM_LUN_WILDCARD);
		if (tptr == NULL) {
			isp_prt(isp, ISP_LOGWARN, "%s: [0x%x] no state pointer for lun %d or wildcard", __func__, aep->at_rxid, lun);
			if (lun == 0) {
				isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
			} else {
				isp_endcmd(isp, aep, SCSI_STATUS_CHECK_COND | ECMD_SVALID | (0x5 << 12) | (0x25 << 16), 0);
			}
			return;
		}
	}

	/*
	 * Start any commands pending resources first.
	 */
	if (tptr->restart_queue) {
		inot_private_data_t *restart_queue = tptr->restart_queue;
		tptr->restart_queue = NULL;
		while (restart_queue) {
			ntp = restart_queue;
			restart_queue = ntp->rd.nt.nt_hba;
			isp_prt(isp, ISP_LOGTDEBUG0, "%s: restarting resrc deprived %x", __func__, ((at2_entry_t *)ntp->rd.data)->at_rxid);
			isp_handle_platform_atio2(isp, (at2_entry_t *) ntp->rd.data);
			isp_put_ntpd(isp, tptr, ntp);
			/*
			 * If a recursion caused the restart queue to start to fill again,
			 * stop and splice the new list on top of the old list and restore
			 * it and go to noresrc.
			 */
			if (tptr->restart_queue) {
				ntp = tptr->restart_queue;
				tptr->restart_queue = restart_queue;
				while (restart_queue->rd.nt.nt_hba) {
					restart_queue = restart_queue->rd.nt.nt_hba;
				}
				restart_queue->rd.nt.nt_hba = ntp;
				goto noresrc;
			}
		}
	}

	atiop = (struct ccb_accept_tio *) SLIST_FIRST(&tptr->atios);
	if (atiop == NULL) {
		goto noresrc;
	}

	atp = isp_get_atpd(isp, tptr, aep->at_rxid);
	if (atp == NULL) {
		goto noresrc;
	}

	atp->state = ATPD_STATE_ATIO;
	SLIST_REMOVE_HEAD(&tptr->atios, sim_links.sle);
	tptr->atio_count--;
	isp_prt(isp, ISP_LOGTDEBUG2, "Take FREE ATIO count now %d", tptr->atio_count);
	atiop->ccb_h.target_id = FCPARAM(isp, 0)->isp_loopid;
	atiop->ccb_h.target_lun = lun;

	/*
	 * We don't get 'suggested' sense data as we do with SCSI cards.
	 */
	atiop->sense_len = 0;
	if (ISP_CAP_2KLOGIN(isp)) {
		/*
		 * NB: We could not possibly have 2K logins if we
		 * NB: also did not have SCC FW.
		 */
		atiop->init_id = ((at2e_entry_t *)aep)->at_iid;
	} else {
		atiop->init_id = aep->at_iid;
	}

	/*
	 * If we're not in the port database, add ourselves.
	 */
	if (!IS_2100(isp) && isp_find_pdb_by_loopid(isp, 0, atiop->init_id, &lp) == 0) {
    		uint64_t iid =
			(((uint64_t) aep->at_wwpn[0]) << 48) |
			(((uint64_t) aep->at_wwpn[1]) << 32) |
			(((uint64_t) aep->at_wwpn[2]) << 16) |
			(((uint64_t) aep->at_wwpn[3]) <<  0);
		/*
		 * However, make sure we delete ourselves if otherwise
		 * we were there but at a different loop id.
		 */
		if (isp_find_pdb_by_wwn(isp, 0, iid, &lp)) {
			isp_del_wwn_entry(isp, 0, iid, lp->handle, lp->portid);
		}
		isp_add_wwn_entry(isp, 0, iid, atiop->init_id, PORT_ANY, 0);
	}
	atiop->cdb_len = ATIO2_CDBLEN;
	ISP_MEMCPY(atiop->cdb_io.cdb_bytes, aep->at_cdb, ATIO2_CDBLEN);
	atiop->ccb_h.status = CAM_CDB_RECVD;
	atiop->tag_id = atp->tag;
	switch (aep->at_taskflags & ATIO2_TC_ATTR_MASK) {
	case ATIO2_TC_ATTR_SIMPLEQ:
		atiop->ccb_h.flags |= CAM_TAG_ACTION_VALID;
		atiop->tag_action = MSG_SIMPLE_Q_TAG;
		break;
	case ATIO2_TC_ATTR_HEADOFQ:
		atiop->ccb_h.flags |= CAM_TAG_ACTION_VALID;
		atiop->tag_action = MSG_HEAD_OF_Q_TAG;
		break;
	case ATIO2_TC_ATTR_ORDERED:
		atiop->ccb_h.flags |= CAM_TAG_ACTION_VALID;
		atiop->tag_action = MSG_ORDERED_Q_TAG;
		break;
	case ATIO2_TC_ATTR_ACAQ:		/* ?? */
	case ATIO2_TC_ATTR_UNTAGGED:
	default:
		atiop->tag_action = 0;
		break;
	}

	atp->orig_datalen = aep->at_datalen;
	atp->bytes_xfered = 0;
	atp->lun = lun;
	atp->nphdl = atiop->init_id;
	atp->sid = PORT_ANY;
	atp->oxid = aep->at_oxid;
	atp->cdb0 = aep->at_cdb[0];
	atp->tattr = aep->at_taskflags & ATIO2_TC_ATTR_MASK;
	atp->state = ATPD_STATE_CAM;
	xpt_done((union ccb *)atiop);
	isp_prt(isp, ISP_LOGTDEBUG0, "ATIO2[0x%x] CDB=0x%x lun %d datalen %u", aep->at_rxid, atp->cdb0, lun, atp->orig_datalen);
	rls_lun_statep(isp, tptr);
	return;
noresrc:
	ntp = isp_get_ntpd(isp, tptr);
	if (ntp == NULL) {
		rls_lun_statep(isp, tptr);
		isp_endcmd(isp, aep, nphdl, 0, SCSI_STATUS_BUSY, 0);
		return;
	}
	memcpy(ntp->rd.data, aep, QENTRY_LEN);
	ntp->rd.nt.nt_hba = tptr->restart_queue;
	tptr->restart_queue = ntp;
	rls_lun_statep(isp, tptr);
}

static void
isp_handle_platform_atio7(ispsoftc_t *isp, at7_entry_t *aep)
{
	int cdbxlen;
	uint16_t lun, chan, nphdl = NIL_HANDLE;
	uint32_t did, sid;
	uint64_t wwn = INI_NONE;
	fcportdb_t *lp;
	tstate_t *tptr;
	struct ccb_accept_tio *atiop;
	atio_private_data_t *atp = NULL;
	atio_private_data_t *oatp;
	inot_private_data_t *ntp;

	did = (aep->at_hdr.d_id[0] << 16) | (aep->at_hdr.d_id[1] << 8) | aep->at_hdr.d_id[2];
	sid = (aep->at_hdr.s_id[0] << 16) | (aep->at_hdr.s_id[1] << 8) | aep->at_hdr.s_id[2];
	lun = (aep->at_cmnd.fcp_cmnd_lun[0] << 8) | aep->at_cmnd.fcp_cmnd_lun[1];

	/*
	 * Find the N-port handle, and Virtual Port Index for this command.
	 *
	 * If we can't, we're somewhat in trouble because we can't actually respond w/o that information.
	 * We also, as a matter of course, need to know the WWN of the initiator too.
	 */
	if (ISP_CAP_MULTI_ID(isp)) {
		/*
		 * Find the right channel based upon D_ID
		 */
		isp_find_chan_by_did(isp, did, &chan);

		if (chan == ISP_NOCHAN) {
			NANOTIME_T now;

			/*
			 * If we don't recognizer our own D_DID, terminate the exchange, unless we're within 2 seconds of startup
			 * It's a bit tricky here as we need to stash this command *somewhere*.
			 */
			GET_NANOTIME(&now);
			if (NANOTIME_SUB(&isp->isp_init_time, &now) > 2000000000ULL) {
				isp_prt(isp, ISP_LOGWARN, "%s: [RX_ID 0x%x] D_ID %x not found on any channel- dropping", __func__, aep->at_rxid, did);
				isp_endcmd(isp, aep, NIL_HANDLE, ISP_NOCHAN, ECMD_TERMINATE, 0);
				return;
			}
			tptr = get_lun_statep(isp, 0, 0);
			if (tptr == NULL) {
				tptr = get_lun_statep(isp, 0, CAM_LUN_WILDCARD);
				if (tptr == NULL) {
					isp_prt(isp, ISP_LOGWARN, "%s: [RX_ID 0x%x] D_ID %x not found on any channel and no tptr- dropping", __func__, aep->at_rxid, did);
					isp_endcmd(isp, aep, NIL_HANDLE, ISP_NOCHAN, ECMD_TERMINATE, 0);
					return;
				}
			}
			isp_prt(isp, ISP_LOGWARN, "%s: [RX_ID 0x%x] D_ID %x not found on any channel- deferring", __func__, aep->at_rxid, did);
			goto noresrc;
		}
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: [RX_ID 0x%x] D_ID 0x%06x found on Chan %d for S_ID 0x%06x", __func__, aep->at_rxid, did, chan, sid);
	} else {
		chan = 0;
	}

	/*
	 * Find the PDB entry for this initiator
	 */
	if (isp_find_pdb_by_sid(isp, chan, sid, &lp) == 0) {
		/*
		 * If we're not in the port database terminate the exchange.
		 */
		isp_prt(isp, ISP_LOGTINFO, "%s: [RX_ID 0x%x] D_ID 0x%06x found on Chan %d for S_ID 0x%06x wasn't in PDB already",
		    __func__, aep->at_rxid, did, chan, sid);
		isp_endcmd(isp, aep, NIL_HANDLE, chan, ECMD_TERMINATE, 0);
		return;
	}
	nphdl = lp->handle;
	wwn = lp->port_wwn;

	/*
	 * Get the tstate pointer
	 */
	tptr = get_lun_statep(isp, chan, lun);
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, chan, CAM_LUN_WILDCARD);
		if (tptr == NULL) {
			isp_prt(isp, ISP_LOGWARN, "%s: [0x%x] no state pointer for lun %d or wildcard", __func__, aep->at_rxid, lun);
			if (lun == 0) {
				isp_endcmd(isp, aep, nphdl, SCSI_STATUS_BUSY, 0);
			} else {
				isp_endcmd(isp, aep, nphdl, chan, SCSI_STATUS_CHECK_COND | ECMD_SVALID | (0x5 << 12) | (0x25 << 16), 0);
			}
			return;
		}
	}

	/*
	 * Start any commands pending resources first.
	 */
	if (tptr->restart_queue) {
		inot_private_data_t *restart_queue = tptr->restart_queue;
		tptr->restart_queue = NULL;
		while (restart_queue) {
			ntp = restart_queue;
			restart_queue = ntp->rd.nt.nt_hba;
			isp_prt(isp, ISP_LOGTDEBUG0, "%s: restarting resrc deprived %x", __func__, ((at7_entry_t *)ntp->rd.data)->at_rxid);
			isp_handle_platform_atio7(isp, (at7_entry_t *) ntp->rd.data);
			isp_put_ntpd(isp, tptr, ntp);
			/*
			 * If a recursion caused the restart queue to start to fill again,
			 * stop and splice the new list on top of the old list and restore
			 * it and go to noresrc.
			 */
			if (tptr->restart_queue) {
				isp_prt(isp, ISP_LOGTDEBUG0, "%s: restart queue refilling", __func__);
				if (restart_queue) {
					ntp = tptr->restart_queue;
					tptr->restart_queue = restart_queue;
					while (restart_queue->rd.nt.nt_hba) {
						restart_queue = restart_queue->rd.nt.nt_hba;
					}
					restart_queue->rd.nt.nt_hba = ntp;
				}
				goto noresrc;
			}
		}
	}

	/*
	 * If the f/w is out of resources, just send a BUSY status back.
	 */
	if (aep->at_rxid == AT7_NORESRC_RXID) {
		rls_lun_statep(isp, tptr);
		isp_endcmd(isp, aep, nphdl, chan, SCSI_BUSY, 0);
		return;
	}

	/*
	 * If we're out of resources, just send a BUSY status back.
	 */
	atiop = (struct ccb_accept_tio *) SLIST_FIRST(&tptr->atios);
	if (atiop == NULL) {
		isp_prt(isp, ISP_LOGTDEBUG0, "[0x%x] out of atios", aep->at_rxid);
		goto noresrc;
	}

	oatp = isp_find_atpd(isp, tptr, aep->at_rxid);
	if (oatp) {
		isp_prt(isp, ISP_LOGTDEBUG0, "[0x%x] tag wraparound in isp_handle_platforms_atio7 (N-Port Handle 0x%04x S_ID 0x%04x OX_ID 0x%04x) oatp state %d",
		    aep->at_rxid, nphdl, sid, aep->at_hdr.ox_id, oatp->state);
		/*
		 * It's not a "no resource" condition- but we can treat it like one
		 */
		goto noresrc;
	}
	atp = isp_get_atpd(isp, tptr, aep->at_rxid);
	if (atp == NULL) {
		isp_prt(isp, ISP_LOGTDEBUG0, "[0x%x] out of atps", aep->at_rxid);
		goto noresrc;
	}
	atp->word3 = lp->prli_word3;
	atp->state = ATPD_STATE_ATIO;
	SLIST_REMOVE_HEAD(&tptr->atios, sim_links.sle);
	tptr->atio_count--;
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG2, atiop->ccb_h.path, "Take FREE ATIO count now %d\n", tptr->atio_count);
	atiop->init_id = nphdl;
	atiop->ccb_h.target_id = FCPARAM(isp, chan)->isp_loopid;
	atiop->ccb_h.target_lun = lun;
	atiop->sense_len = 0;
	cdbxlen = aep->at_cmnd.fcp_cmnd_alen_datadir >> FCP_CMND_ADDTL_CDBLEN_SHIFT;
	if (cdbxlen) {
		isp_prt(isp, ISP_LOGWARN, "additional CDBLEN ignored");
	}
	cdbxlen = sizeof (aep->at_cmnd.cdb_dl.sf.fcp_cmnd_cdb);
	ISP_MEMCPY(atiop->cdb_io.cdb_bytes, aep->at_cmnd.cdb_dl.sf.fcp_cmnd_cdb, cdbxlen);
	atiop->cdb_len = cdbxlen;
	atiop->ccb_h.status = CAM_CDB_RECVD;
	atiop->tag_id = atp->tag;
	switch (aep->at_cmnd.fcp_cmnd_task_attribute & FCP_CMND_TASK_ATTR_MASK) {
	case FCP_CMND_TASK_ATTR_SIMPLE:
		atiop->ccb_h.flags |= CAM_TAG_ACTION_VALID;
		atiop->tag_action = MSG_SIMPLE_Q_TAG;
		break;
	case FCP_CMND_TASK_ATTR_HEAD:
		atiop->ccb_h.flags |= CAM_TAG_ACTION_VALID;
		atiop->tag_action = MSG_HEAD_OF_Q_TAG;
		break;
	case FCP_CMND_TASK_ATTR_ORDERED:
		atiop->ccb_h.flags |= CAM_TAG_ACTION_VALID;
		atiop->tag_action = MSG_ORDERED_Q_TAG;
		break;
	default:
		/* FALLTHROUGH */
	case FCP_CMND_TASK_ATTR_ACA:
	case FCP_CMND_TASK_ATTR_UNTAGGED:
		atiop->tag_action = 0;
		break;
	}
	atp->orig_datalen = aep->at_cmnd.cdb_dl.sf.fcp_cmnd_dl;
	atp->bytes_xfered = 0;
	atp->lun = lun;
	atp->nphdl = nphdl;
	atp->portid = sid;
	atp->oxid = aep->at_hdr.ox_id;
	atp->rxid = aep->at_hdr.rx_id;
	atp->cdb0 = atiop->cdb_io.cdb_bytes[0];
	atp->tattr = aep->at_cmnd.fcp_cmnd_task_attribute & FCP_CMND_TASK_ATTR_MASK;
	atp->state = ATPD_STATE_CAM;
	isp_prt(isp, ISP_LOGTDEBUG0, "ATIO7[0x%x] CDB=0x%x lun %d datalen %u", aep->at_rxid, atp->cdb0, lun, atp->orig_datalen);
	xpt_done((union ccb *)atiop);
	rls_lun_statep(isp, tptr);
	return;
noresrc:
	if (atp) {
		isp_put_atpd(isp, tptr, atp);
	}
	ntp = isp_get_ntpd(isp, tptr);
	if (ntp == NULL) {
		rls_lun_statep(isp, tptr);
		isp_endcmd(isp, aep, nphdl, chan, SCSI_STATUS_BUSY, 0);
		return;
	}
	memcpy(ntp->rd.data, aep, QENTRY_LEN);
	ntp->rd.nt.nt_hba = tptr->restart_queue;
	tptr->restart_queue = ntp;
	rls_lun_statep(isp, tptr);
}


/*
 * Handle starting an SRR (sequence retransmit request)
 * We get here when we've gotten the immediate notify
 * and the return of all outstanding CTIOs for this
 * transaction.
 */
static void
isp_handle_srr_start(ispsoftc_t *isp, tstate_t *tptr, atio_private_data_t *atp)
{
	in_fcentry_24xx_t *inot;
	uint32_t srr_off, ccb_off, ccb_len, ccb_end;
	union ccb *ccb;

	inot = (in_fcentry_24xx_t *)atp->srr;
	srr_off = inot->in_srr_reloff_lo | (inot->in_srr_reloff_hi << 16);
	ccb = atp->srr_ccb;
	atp->srr_ccb = NULL;
	atp->nsrr++;
	if (ccb == NULL) {
		isp_prt(isp, ISP_LOGWARN, "SRR[0x%x] null ccb", atp->tag);
		goto fail;
	}

	ccb_off = ccb->ccb_h.spriv_field0;
	ccb_len = ccb->csio.dxfer_len;
        ccb_end = (ccb_off == ~0)? ~0 : ccb_off + ccb_len;

	switch (inot->in_srr_iu) {
	case R_CTL_INFO_SOLICITED_DATA:
		/*
		 * We have to restart a FCP_DATA data out transaction
		 */
		atp->sendst = 0;
		atp->bytes_xfered = srr_off;
		if (ccb_len == 0) {
			isp_prt(isp, ISP_LOGWARN, "SRR[0x%x] SRR offset 0x%x but current CCB doesn't transfer data", atp->tag, srr_off);
			goto mdp;
		}
 		if (srr_off < ccb_off || ccb_off > srr_off + ccb_len) {
			isp_prt(isp, ISP_LOGWARN, "SRR[0x%x] SRR offset 0x%x not covered by current CCB data range [0x%x..0x%x]", atp->tag, srr_off, ccb_off, ccb_end);
			goto mdp;
		}
		isp_prt(isp, ISP_LOGWARN, "SRR[0x%x] SRR offset 0x%x covered by current CCB data range [0x%x..0x%x]", atp->tag, srr_off, ccb_off, ccb_end);
		break;
	case R_CTL_INFO_COMMAND_STATUS:
		isp_prt(isp, ISP_LOGTINFO, "SRR[0x%x] Got an FCP RSP SRR- resending status", atp->tag);
		atp->sendst = 1;
		/*
		 * We have to restart a FCP_RSP IU transaction
		 */
		break;
	case R_CTL_INFO_DATA_DESCRIPTOR:
		/*
		 * We have to restart an FCP DATA in transaction
		 */
		isp_prt(isp, ISP_LOGWARN, "Got an FCP DATA IN SRR- dropping");
		goto fail;
		
	default:
		isp_prt(isp, ISP_LOGWARN, "Got an unknown information (%x) SRR- dropping", inot->in_srr_iu);
		goto fail;
	}

	/*
	 * We can't do anything until this is acked, so we might as well start it now.
	 * We aren't going to do the usual asynchronous ack issue because we need
	 * to make sure this gets on the wire first.
	 */
	if (isp_notify_ack(isp, inot)) {
		isp_prt(isp, ISP_LOGWARN, "could not push positive ack for SRR- you lose");
		goto fail;
	}
	isp_target_start_ctio(isp, ccb, FROM_SRR);
	return;
fail:
	inot->in_reserved = 1;
	isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot);
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	ccb->ccb_h.status |= CAM_REQ_CMP_ERR;
	isp_complete_ctio(ccb);
	return;
mdp:
	if (isp_notify_ack(isp, inot)) {
		isp_prt(isp, ISP_LOGWARN, "could not push positive ack for SRR- you lose");
		goto fail;
	}
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;
	ccb->ccb_h.status = CAM_MESSAGE_RECV;
	/*
	 * This is not a strict interpretation of MDP, but it's close
	 */
	ccb->csio.msg_ptr = &ccb->csio.sense_data.sense_buf[SSD_FULL_SIZE - 16];
	ccb->csio.msg_len = 7;
	ccb->csio.msg_ptr[0] = MSG_EXTENDED;
	ccb->csio.msg_ptr[1] = 5;
	ccb->csio.msg_ptr[2] = 0;	/* modify data pointer */
	ccb->csio.msg_ptr[3] = srr_off >> 24;
	ccb->csio.msg_ptr[4] = srr_off >> 16;
	ccb->csio.msg_ptr[5] = srr_off >> 8;
	ccb->csio.msg_ptr[6] = srr_off;
	isp_complete_ctio(ccb);
}


static void
isp_handle_srr_notify(ispsoftc_t *isp, void *inot_raw)
{
	tstate_t *tptr;
	in_fcentry_24xx_t *inot = inot_raw;
	atio_private_data_t *atp;
	uint32_t tag = inot->in_rxid;
	uint32_t bus = inot->in_vpidx;

	if (!IS_24XX(isp)) {
		isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot_raw);
		return;
	}

	tptr = get_lun_statep_from_tag(isp, bus, tag);
	if (tptr == NULL) {
		isp_prt(isp, ISP_LOGERR, "%s: cannot find tptr for tag %x in SRR Notify", __func__, tag);
		isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot);
		return;
	}
	atp = isp_find_atpd(isp, tptr, tag);
	if (atp == NULL) {
		rls_lun_statep(isp, tptr);
		isp_prt(isp, ISP_LOGERR, "%s: cannot find adjunct for %x in SRR Notify", __func__, tag);
		isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot);
		return;
	}
	atp->srr_notify_rcvd = 1;
	memcpy(atp->srr, inot, sizeof (atp->srr));
	isp_prt(isp, ISP_LOGTINFO /* ISP_LOGTDEBUG0 */, "SRR[0x%x] inot->in_rxid flags 0x%x srr_iu=%x reloff 0x%x", inot->in_rxid, inot->in_flags, inot->in_srr_iu,
	    inot->in_srr_reloff_lo | (inot->in_srr_reloff_hi << 16));
	if (atp->srr_ccb)
		isp_handle_srr_start(isp, tptr, atp);
	rls_lun_statep(isp, tptr);
}

static void
isp_handle_platform_ctio(ispsoftc_t *isp, void *arg)
{
	union ccb *ccb;
	int sentstatus = 0, ok = 0, notify_cam = 0, resid = 0, failure = 0;
	tstate_t *tptr = NULL;
	atio_private_data_t *atp = NULL;
	int bus;
	uint32_t handle, moved_data = 0, data_requested;

	/*
	 * CTIO handles are 16 bits.
	 * CTIO2 and CTIO7 are 32 bits.
	 */

	if (IS_SCSI(isp)) {
		handle = ((ct_entry_t *)arg)->ct_syshandle;
	} else {
		handle = ((ct2_entry_t *)arg)->ct_syshandle;
	}
	ccb = isp_find_xs_tgt(isp, handle);
	if (ccb == NULL) {
		isp_print_bytes(isp, "null ccb in isp_handle_platform_ctio", QENTRY_LEN, arg);
		return;
	}
	isp_destroy_tgt_handle(isp, handle);
	data_requested = PISP_PCMD(ccb)->datalen;
	isp_free_pcmd(isp, ccb);
	if (isp->isp_nactive) {
		isp->isp_nactive--;
	}

	bus = XS_CHANNEL(ccb);
	tptr = get_lun_statep(isp, bus, XS_LUN(ccb));
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, bus, CAM_LUN_WILDCARD);
	}
	if (tptr == NULL) {
		isp_prt(isp, ISP_LOGERR, "%s: cannot find tptr for tag %x after I/O", __func__, ccb->csio.tag_id);
		return;
	}

	if (IS_24XX(isp)) {
		atp = isp_find_atpd(isp, tptr, ((ct7_entry_t *)arg)->ct_rxid);
	} else if (IS_FC(isp)) {
		atp = isp_find_atpd(isp, tptr, ((ct2_entry_t *)arg)->ct_rxid);
	} else {
		atp = isp_find_atpd(isp, tptr, ((ct_entry_t *)arg)->ct_fwhandle);
	}
	if (atp == NULL) {
		rls_lun_statep(isp, tptr);
		isp_prt(isp, ISP_LOGERR, "%s: cannot find adjunct for %x after I/O", __func__, ccb->csio.tag_id);
		return;
	}
	KASSERT((atp->ctcnt > 0), ("ctio count not greater than zero"));
	atp->bytes_in_transit -= data_requested;
	atp->ctcnt -= 1;
	ccb->ccb_h.status &= ~CAM_STATUS_MASK;

	if (IS_24XX(isp)) {
		ct7_entry_t *ct = arg;

		if (ct->ct_nphdl == CT7_SRR) {
			atp->srr_ccb = ccb;
			if (atp->srr_notify_rcvd)
				isp_handle_srr_start(isp, tptr, atp);
			rls_lun_statep(isp, tptr);
			return;
		}
		if (ct->ct_nphdl == CT_HBA_RESET) {
			failure = CAM_UNREC_HBA_ERROR;
		} else {
			sentstatus = ct->ct_flags & CT7_SENDSTATUS;
			ok = (ct->ct_nphdl == CT7_OK);
			notify_cam = (ct->ct_header.rqs_seqno & ATPD_SEQ_NOTIFY_CAM) != 0;
			if ((ct->ct_flags & CT7_DATAMASK) != CT7_NO_DATA) {
				resid = ct->ct_resid;
				moved_data = data_requested - resid;
			}
		}
		isp_prt(isp, ok? ISP_LOGTDEBUG0 : ISP_LOGWARN, "%s: CTIO7[%x] seq %u nc %d sts 0x%x flg 0x%x sns %d resid %d %s", __func__, ct->ct_rxid, ATPD_GET_SEQNO(ct),
		   notify_cam, ct->ct_nphdl, ct->ct_flags, (ccb->ccb_h.status & CAM_SENT_SENSE) != 0, resid, sentstatus? "FIN" : "MID");
	} else if (IS_FC(isp)) {
		ct2_entry_t *ct = arg;
		if (ct->ct_status == CT_SRR) {
			atp->srr_ccb = ccb;
			if (atp->srr_notify_rcvd)
				isp_handle_srr_start(isp, tptr, atp);
			rls_lun_statep(isp, tptr);
			isp_target_putback_atio(ccb);
			return;
		}
		if (ct->ct_status == CT_HBA_RESET) {
			failure = CAM_UNREC_HBA_ERROR;
		} else {
			sentstatus = ct->ct_flags & CT2_SENDSTATUS;
			ok = (ct->ct_status & ~QLTM_SVALID) == CT_OK;
			notify_cam = (ct->ct_header.rqs_seqno & ATPD_SEQ_NOTIFY_CAM) != 0;
			if ((ct->ct_flags & CT2_DATAMASK) != CT2_NO_DATA) {
				resid = ct->ct_resid;
				moved_data = data_requested - resid;
			}
		}
		isp_prt(isp, ok? ISP_LOGTDEBUG0 : ISP_LOGWARN, "%s: CTIO2[%x] seq %u nc %d sts 0x%x flg 0x%x sns %d resid %d %s", __func__, ct->ct_rxid, ATPD_GET_SEQNO(ct),
		    notify_cam, ct->ct_status, ct->ct_flags, (ccb->ccb_h.status & CAM_SENT_SENSE) != 0, resid, sentstatus? "FIN" : "MID");
	} else {
		ct_entry_t *ct = arg;

		if (ct->ct_status == (CT_HBA_RESET & 0xff)) {
			failure = CAM_UNREC_HBA_ERROR;
		} else {
			sentstatus = ct->ct_flags & CT_SENDSTATUS;
			ok = (ct->ct_status  & ~QLTM_SVALID) == CT_OK;
			notify_cam = (ct->ct_header.rqs_seqno & ATPD_SEQ_NOTIFY_CAM) != 0;
		}
		if ((ct->ct_flags & CT_DATAMASK) != CT_NO_DATA) {
			resid = ct->ct_resid;
			moved_data = data_requested - resid;
		}
		isp_prt(isp, ISP_LOGTDEBUG0, "%s: CTIO[%x] seq %u nc %d tag %x S_ID 0x%x lun %d sts %x flg %x resid %d %s", __func__, ct->ct_fwhandle, ATPD_GET_SEQNO(ct),
		    notify_cam, ct->ct_tag_val, ct->ct_iid, ct->ct_lun, ct->ct_status, ct->ct_flags, resid, sentstatus? "FIN" : "MID");
	}
	if (ok) {
		if (moved_data) {
			atp->bytes_xfered += moved_data;
			ccb->csio.resid = atp->orig_datalen - atp->bytes_xfered - atp->bytes_in_transit;
		}
		if (sentstatus && (ccb->ccb_h.flags & CAM_SEND_SENSE)) {
			ccb->ccb_h.status |= CAM_SENT_SENSE;
		}
		ccb->ccb_h.status |= CAM_REQ_CMP;
	} else {
		notify_cam = 1;
		if (failure == CAM_UNREC_HBA_ERROR)
			ccb->ccb_h.status |= CAM_UNREC_HBA_ERROR;
		else
			ccb->ccb_h.status |= CAM_REQ_CMP_ERR;
	}
	atp->state = ATPD_STATE_PDON;
	rls_lun_statep(isp, tptr);

	/*
	 * We never *not* notify CAM when there has been any error (ok == 0),
	 * so we never need to do an ATIO putback if we're not notifying CAM.
	 */
	isp_prt(isp, ISP_LOGTDEBUG0, "%s CTIO[0x%x] done (ok=%d nc=%d nowsendstatus=%d ccb ss=%d)",
	    (sentstatus)? "  FINAL " : "MIDTERM ", atp->tag, ok, notify_cam, atp->sendst, (ccb->ccb_h.flags & CAM_SEND_STATUS) != 0);
	if (notify_cam == 0) {
		if (atp->sendst) {
			isp_target_start_ctio(isp, ccb, FROM_CTIO_DONE);
		}
		return;
	}

	/*
	 * We're telling CAM we're done with this CTIO transaction.
	 *
	 * 24XX cards never need an ATIO put back.
	 *
	 * Other cards need one put back only on error.
	 * In the latter case, a timeout will re-fire
	 * and try again in case we didn't have
	 * queue resources to do so at first. In any case,
	 * once the putback is done we do the completion
	 * call.
	 */
	if (ok || IS_24XX(isp)) {
		isp_complete_ctio(ccb);
	} else {
		isp_target_putback_atio(ccb);
	}
}

static void
isp_handle_platform_notify_scsi(ispsoftc_t *isp, in_entry_t *inot)
{
	isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot);
}

static void
isp_handle_platform_notify_fc(ispsoftc_t *isp, in_fcentry_t *inp)
{
	int needack = 1;
	switch (inp->in_status) {
	case IN_PORT_LOGOUT:
		/*
		 * XXX: Need to delete this initiator's WWN from the database
		 * XXX: Need to send this LOGOUT upstream
		 */
		isp_prt(isp, ISP_LOGWARN, "port logout of S_ID 0x%x", inp->in_iid);
		break;
	case IN_PORT_CHANGED:
		isp_prt(isp, ISP_LOGWARN, "port changed for S_ID 0x%x", inp->in_iid);
		break;
	case IN_GLOBAL_LOGO:
		isp_del_all_wwn_entries(isp, 0);
		isp_prt(isp, ISP_LOGINFO, "all ports logged out");
		break;
	case IN_ABORT_TASK:
	{
		tstate_t *tptr;
		uint16_t lun;
		uint32_t loopid;
		uint64_t wwn;
		atio_private_data_t *atp;
		fcportdb_t *lp;
		struct ccb_immediate_notify *inot = NULL;

		if (ISP_CAP_SCCFW(isp)) {
			lun = inp->in_scclun;
		} else {
			lun = inp->in_lun;
		}
		if (ISP_CAP_2KLOGIN(isp)) {
			loopid = ((in_fcentry_e_t *)inp)->in_iid;
		} else {
			loopid = inp->in_iid;
		}
		if (isp_find_pdb_by_loopid(isp, 0, loopid, &lp)) {
			wwn = lp->port_wwn;
		} else {
			wwn = INI_ANY;
		}
		tptr = get_lun_statep(isp, 0, lun);
		if (tptr == NULL) {
			tptr = get_lun_statep(isp, 0, CAM_LUN_WILDCARD);
			if (tptr == NULL) {
				isp_prt(isp, ISP_LOGWARN, "ABORT TASK for lun %u- but no tstate", lun);
				return;
			}
		}
		atp = isp_find_atpd(isp, tptr, inp->in_seqid);

		if (atp) {
			inot = (struct ccb_immediate_notify *) SLIST_FIRST(&tptr->inots);
			isp_prt(isp, ISP_LOGTDEBUG0, "ABORT TASK RX_ID %x WWN 0x%016llx state %d", inp->in_seqid, (unsigned long long) wwn, atp->state);
			if (inot) {
				tptr->inot_count--;
				SLIST_REMOVE_HEAD(&tptr->inots, sim_links.sle);
				ISP_PATH_PRT(isp, ISP_LOGTDEBUG2, inot->ccb_h.path, "%s: Take FREE INOT count now %d\n", __func__, tptr->inot_count);
			} else {
				ISP_PATH_PRT(isp, ISP_LOGWARN, tptr->owner, "out of INOT structures\n");
			}
		} else {
			ISP_PATH_PRT(isp, ISP_LOGWARN, tptr->owner, "abort task RX_ID %x from wwn 0x%016llx, state unknown\n", inp->in_seqid, wwn);
		}
		if (inot) {
			isp_notify_t tmp, *nt = &tmp;
			ISP_MEMZERO(nt, sizeof (isp_notify_t));
    			nt->nt_hba = isp;
			nt->nt_tgt = FCPARAM(isp, 0)->isp_wwpn;
			nt->nt_wwn = wwn;
			nt->nt_nphdl = loopid;
			nt->nt_sid = PORT_ANY;
			nt->nt_did = PORT_ANY;
    			nt->nt_lun = lun;
            		nt->nt_need_ack = 1;
    			nt->nt_channel = 0;
    			nt->nt_ncode = NT_ABORT_TASK;
    			nt->nt_lreserved = inot;
			isp_handle_platform_target_tmf(isp, nt);
			needack = 0;
		}
		rls_lun_statep(isp, tptr);
		break;
	}
	default:
		break;
	}
	if (needack) {
		isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inp);
	}
}

static void
isp_handle_platform_notify_24xx(ispsoftc_t *isp, in_fcentry_24xx_t *inot)
{
	uint16_t nphdl;
	uint16_t prli_options = 0;
	uint32_t portid;
	fcportdb_t *lp;
	uint8_t *ptr = NULL;
	uint64_t wwn;

	nphdl = inot->in_nphdl;
	if (nphdl != NIL_HANDLE) {
		portid = inot->in_portid_hi << 16 | inot->in_portid_lo;
	} else {
		portid = PORT_ANY;
	}

	switch (inot->in_status) {
	case IN24XX_ELS_RCVD:
	{
		char buf[16], *msg;
		int chan = ISP_GET_VPIDX(isp, inot->in_vpidx);

		/*
		 * Note that we're just getting notification that an ELS was received
		 * (possibly with some associated information sent upstream). This is
		 * *not* the same as being given the ELS frame to accept or reject.
		 */
		switch (inot->in_status_subcode) {
		case LOGO:
			msg = "LOGO";
			if (ISP_FW_NEWER_THAN(isp, 4, 0, 25)) {
				ptr = (uint8_t *)inot;  /* point to unswizzled entry! */
				wwn =	(((uint64_t) ptr[IN24XX_LOGO_WWPN_OFF])   << 56) |
					(((uint64_t) ptr[IN24XX_LOGO_WWPN_OFF+1]) << 48) |
					(((uint64_t) ptr[IN24XX_LOGO_WWPN_OFF+2]) << 40) |
					(((uint64_t) ptr[IN24XX_LOGO_WWPN_OFF+3]) << 32) |
					(((uint64_t) ptr[IN24XX_LOGO_WWPN_OFF+4]) << 24) |
					(((uint64_t) ptr[IN24XX_LOGO_WWPN_OFF+5]) << 16) |
					(((uint64_t) ptr[IN24XX_LOGO_WWPN_OFF+6]) <<  8) |
					(((uint64_t) ptr[IN24XX_LOGO_WWPN_OFF+7]));
			} else {
				wwn = INI_ANY;
			}
			isp_del_wwn_entry(isp, chan, wwn, nphdl, portid);
			break;
		case PRLO:
			msg = "PRLO";
			break;
		case PLOGI:
		case PRLI:
			/*
			 * Treat PRLI the same as PLOGI and make a database entry for it.
			 */
			if (inot->in_status_subcode == PLOGI) {
				msg = "PLOGI";
			} else {
				prli_options = inot->in_prli_options;
				msg = "PRLI";
			}
			if (ISP_FW_NEWER_THAN(isp, 4, 0, 25)) {
				ptr = (uint8_t *)inot;  /* point to unswizzled entry! */
				wwn =	(((uint64_t) ptr[IN24XX_PLOGI_WWPN_OFF])   << 56) |
					(((uint64_t) ptr[IN24XX_PLOGI_WWPN_OFF+1]) << 48) |
					(((uint64_t) ptr[IN24XX_PLOGI_WWPN_OFF+2]) << 40) |
					(((uint64_t) ptr[IN24XX_PLOGI_WWPN_OFF+3]) << 32) |
					(((uint64_t) ptr[IN24XX_PLOGI_WWPN_OFF+4]) << 24) |
					(((uint64_t) ptr[IN24XX_PLOGI_WWPN_OFF+5]) << 16) |
					(((uint64_t) ptr[IN24XX_PLOGI_WWPN_OFF+6]) <<  8) |
					(((uint64_t) ptr[IN24XX_PLOGI_WWPN_OFF+7]));
			} else {
				wwn = INI_NONE;
			}
			isp_add_wwn_entry(isp, chan, wwn, nphdl, portid, prli_options);
			break;
		case PDISC:
			msg = "PDISC";
			break;
		case ADISC:
			msg = "ADISC";
			break;
		default:
			ISP_SNPRINTF(buf, sizeof (buf), "ELS 0x%x", inot->in_status_subcode);
			msg = buf;
			break;
		}
		if (inot->in_flags & IN24XX_FLAG_PUREX_IOCB) {
			isp_prt(isp, ISP_LOGERR, "%s Chan %d ELS N-port handle %x PortID 0x%06x marked as needing a PUREX response", msg, chan, nphdl, portid);
			break;
		}
		isp_prt(isp, ISP_LOGTDEBUG0, "%s Chan %d ELS N-port handle %x PortID 0x%06x RX_ID 0x%x OX_ID 0x%x", msg, chan, nphdl, portid,
		    inot->in_rxid, inot->in_oxid);
		isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot);
		break;
	}

	case IN24XX_PORT_LOGOUT:
		ptr = "PORT LOGOUT";
		if (isp_find_pdb_by_loopid(isp, ISP_GET_VPIDX(isp, inot->in_vpidx), nphdl, &lp)) {
			isp_del_wwn_entry(isp, ISP_GET_VPIDX(isp, inot->in_vpidx), lp->port_wwn, nphdl, lp->portid);
		}
		/* FALLTHROUGH */
	case IN24XX_PORT_CHANGED:
		if (ptr == NULL) {
			ptr = "PORT CHANGED";
		}
		/* FALLTHROUGH */
	case IN24XX_LIP_RESET: 
		if (ptr == NULL) {
			ptr = "LIP RESET";
		}
		isp_prt(isp, ISP_LOGINFO, "Chan %d %s (sub-status 0x%x) for N-port handle 0x%x", ISP_GET_VPIDX(isp, inot->in_vpidx), ptr, inot->in_status_subcode, nphdl);

		/*
		 * All subcodes here are irrelevant. What is relevant
		 * is that we need to terminate all active commands from
		 * this initiator (known by N-port handle).
		 */
		/* XXX IMPLEMENT XXX */
		isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot);
		break;

	case IN24XX_SRR_RCVD:
#ifdef	ISP_TARGET_MODE
		isp_handle_srr_notify(isp, inot);
		break;
#else
		if (ptr == NULL) {
			ptr = "SRR RCVD";
		}
		/* FALLTHROUGH */
#endif
	case IN24XX_LINK_RESET:
		if (ptr == NULL) {
			ptr = "LINK RESET";
		}
	case IN24XX_LINK_FAILED:
		if (ptr == NULL) {
			ptr = "LINK FAILED";
		}
	default:
		isp_prt(isp, ISP_LOGWARN, "Chan %d %s", ISP_GET_VPIDX(isp, inot->in_vpidx), ptr);
		isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot);
		break;
	}
}

static int
isp_handle_platform_target_notify_ack(ispsoftc_t *isp, isp_notify_t *mp)
{

	if (isp->isp_state != ISP_RUNSTATE) {
		isp_prt(isp, ISP_LOGTINFO, "Notify Code 0x%x (qevalid=%d) acked- h/w not ready (dropping)", mp->nt_ncode, mp->nt_lreserved != NULL);
		return (0);
	}

	/*
	 * This case is for a Task Management Function, which shows up as an ATIO7 entry.
	 */
	if (IS_24XX(isp) && mp->nt_lreserved && ((isphdr_t *)mp->nt_lreserved)->rqs_entry_type == RQSTYPE_ATIO) {
		ct7_entry_t local, *cto = &local;
		at7_entry_t *aep = (at7_entry_t *)mp->nt_lreserved;
		fcportdb_t *lp;
		uint32_t sid;
		uint16_t nphdl;

		sid = (aep->at_hdr.s_id[0] << 16) | (aep->at_hdr.s_id[1] << 8) | aep->at_hdr.s_id[2];
		if (isp_find_pdb_by_sid(isp, mp->nt_channel, sid, &lp)) {
			nphdl = lp->handle;
		} else {
			nphdl = NIL_HANDLE;
		}
		ISP_MEMZERO(&local, sizeof (local));
		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO7;
		cto->ct_header.rqs_entry_count = 1;
		cto->ct_nphdl = nphdl;
		cto->ct_rxid = aep->at_rxid;
		cto->ct_vpidx = mp->nt_channel;
		cto->ct_iid_lo = sid;
		cto->ct_iid_hi = sid >> 16;
		cto->ct_oxid = aep->at_hdr.ox_id;
		cto->ct_flags = CT7_SENDSTATUS|CT7_NOACK|CT7_NO_DATA|CT7_FLAG_MODE1;
		cto->ct_flags |= (aep->at_ta_len >> 12) << CT7_TASK_ATTR_SHIFT;
		return (isp_target_put_entry(isp, &local));
	}

	/*
	 * This case is for a responding to an ABTS frame
	 */
	if (IS_24XX(isp) && mp->nt_lreserved && ((isphdr_t *)mp->nt_lreserved)->rqs_entry_type == RQSTYPE_ABTS_RCVD) {

		/*
		 * Overload nt_need_ack here to mark whether we've terminated the associated command.
		 */
		if (mp->nt_need_ack) {
			uint8_t storage[QENTRY_LEN];
			ct7_entry_t *cto = (ct7_entry_t *) storage;
			abts_t *abts = (abts_t *)mp->nt_lreserved;

			ISP_MEMZERO(cto, sizeof (ct7_entry_t));
			isp_prt(isp, ISP_LOGTDEBUG0, "%s: [%x] terminating after ABTS received", __func__, abts->abts_rxid_task);
			cto->ct_header.rqs_entry_type = RQSTYPE_CTIO7;
			cto->ct_header.rqs_entry_count = 1;
			cto->ct_nphdl = mp->nt_nphdl;
			cto->ct_rxid = abts->abts_rxid_task;
			cto->ct_iid_lo = mp->nt_sid;
			cto->ct_iid_hi = mp->nt_sid >> 16;
			cto->ct_oxid = abts->abts_ox_id;
			cto->ct_vpidx = mp->nt_channel;
			cto->ct_flags = CT7_NOACK|CT7_TERMINATE;
			if (isp_target_put_entry(isp, cto)) {
				return (ENOMEM);
			}
			mp->nt_need_ack = 0;
		}
		if (isp_acknak_abts(isp, mp->nt_lreserved, 0) == ENOMEM) {
			return (ENOMEM);
		} else {
			return (0);
		}
	}

	/*
	 * Handle logout cases here
	 */
	if (mp->nt_ncode == NT_GLOBAL_LOGOUT) {
		isp_del_all_wwn_entries(isp, mp->nt_channel);
	}

	if (mp->nt_ncode == NT_LOGOUT) {
		if (!IS_2100(isp) && IS_FC(isp)) {
			isp_del_wwn_entries(isp, mp);
		}
	}

	/*
	 * General purpose acknowledgement
	 */
	if (mp->nt_need_ack) {
		isp_prt(isp, ISP_LOGTINFO, "Notify Code 0x%x (qevalid=%d) being acked", mp->nt_ncode, mp->nt_lreserved != NULL);
		/*
		 * Don't need to use the guaranteed send because the caller can retry
		 */
		return (isp_notify_ack(isp, mp->nt_lreserved));
	}
	return (0);
}

/*
 * Handle task management functions.
 *
 * We show up here with a notify structure filled out.
 *
 * The nt_lreserved tag points to the original queue entry
 */
static void
isp_handle_platform_target_tmf(ispsoftc_t *isp, isp_notify_t *notify)
{
	tstate_t *tptr;
	fcportdb_t *lp;
	struct ccb_immediate_notify *inot;
	inot_private_data_t *ntp = NULL;
	lun_id_t lun;

	isp_prt(isp, ISP_LOGTDEBUG0, "%s: code 0x%x sid  0x%x tagval 0x%016llx chan %d lun 0x%x", __func__, notify->nt_ncode,
	    notify->nt_sid, (unsigned long long) notify->nt_tagval, notify->nt_channel, notify->nt_lun);
	/*
	 * NB: This assignment is necessary because of tricky type conversion.
	 * XXX: This is tricky and I need to check this. If the lun isn't known
	 * XXX: for the task management function, it does not of necessity follow
	 * XXX: that it should go up stream to the wildcard listener.
	 */
	if (notify->nt_lun == LUN_ANY) {
		lun = CAM_LUN_WILDCARD;
	} else {
		lun = notify->nt_lun;
	}
	tptr = get_lun_statep(isp, notify->nt_channel, lun);
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, notify->nt_channel, CAM_LUN_WILDCARD);
		if (tptr == NULL) {
			isp_prt(isp, ISP_LOGWARN, "%s: no state pointer found for chan %d lun 0x%x", __func__, notify->nt_channel, lun);
			goto bad;
		}
	}
	inot = (struct ccb_immediate_notify *) SLIST_FIRST(&tptr->inots);
	if (inot == NULL) {
		isp_prt(isp, ISP_LOGWARN, "%s: out of immediate notify structures for chan %d lun 0x%x", __func__, notify->nt_channel, lun);
		goto bad;
	}

	if (isp_find_pdb_by_sid(isp, notify->nt_channel, notify->nt_sid, &lp) == 0) {
		inot->initiator_id = CAM_TARGET_WILDCARD;
	} else {
		inot->initiator_id = lp->handle;
	}
	inot->seq_id = notify->nt_tagval;
	inot->tag_id = notify->nt_tagval >> 32;

	switch (notify->nt_ncode) {
	case NT_ABORT_TASK:
		isp_target_mark_aborted_early(isp, tptr, inot->tag_id);
		inot->arg = MSG_ABORT_TASK;
		break;
	case NT_ABORT_TASK_SET:
		isp_target_mark_aborted_early(isp, tptr, TAG_ANY);
		inot->arg = MSG_ABORT_TASK_SET;
		break;
	case NT_CLEAR_ACA:
		inot->arg = MSG_CLEAR_ACA;
		break;
	case NT_CLEAR_TASK_SET:
		inot->arg = MSG_CLEAR_TASK_SET;
		break;
	case NT_LUN_RESET:
		inot->arg = MSG_LOGICAL_UNIT_RESET;
		break;
	case NT_TARGET_RESET:
		inot->arg = MSG_TARGET_RESET;
		break;
	default:
		isp_prt(isp, ISP_LOGWARN, "%s: unknown TMF code 0x%x for chan %d lun 0x%x", __func__, notify->nt_ncode, notify->nt_channel, lun);
		goto bad;
	}

	ntp = isp_get_ntpd(isp, tptr);
	if (ntp == NULL) {
		isp_prt(isp, ISP_LOGWARN, "%s: out of inotify private structures", __func__);
		goto bad;
	}
	ISP_MEMCPY(&ntp->rd.nt, notify, sizeof (isp_notify_t));
	if (notify->nt_lreserved) {
		ISP_MEMCPY(&ntp->rd.data, notify->nt_lreserved, QENTRY_LEN);
		ntp->rd.nt.nt_lreserved = &ntp->rd.data;
	}
	ntp->rd.seq_id = notify->nt_tagval;
	ntp->rd.tag_id = notify->nt_tagval >> 32;

	tptr->inot_count--;
	SLIST_REMOVE_HEAD(&tptr->inots, sim_links.sle);
	rls_lun_statep(isp, tptr);
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG2, inot->ccb_h.path, "%s: Take FREE INOT count now %d\n", __func__, tptr->inot_count);
	inot->ccb_h.status = CAM_MESSAGE_RECV;
	xpt_done((union ccb *)inot);
	return;
bad:
	if (tptr) {
		rls_lun_statep(isp, tptr);
	}
	if (notify->nt_need_ack && notify->nt_lreserved) {
		if (((isphdr_t *)notify->nt_lreserved)->rqs_entry_type == RQSTYPE_ABTS_RCVD) {
			if (isp_acknak_abts(isp, notify->nt_lreserved, ENOMEM)) {
				isp_prt(isp, ISP_LOGWARN, "you lose- unable to send an ACKNAK");
			}
		} else {
			isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, notify->nt_lreserved);
		}
	}
}

/*
 * Find the associated private data and mark it as dead so
 * we don't try to work on it any further.
 */
static void
isp_target_mark_aborted(ispsoftc_t *isp, union ccb *ccb)
{
	tstate_t *tptr;
	atio_private_data_t *atp;
	union ccb *accb = ccb->cab.abort_ccb;

	tptr = get_lun_statep(isp, XS_CHANNEL(accb), XS_LUN(accb));
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, XS_CHANNEL(accb), CAM_LUN_WILDCARD);
		if (tptr == NULL) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			return;
		}
	}

	atp = isp_find_atpd(isp, tptr, accb->atio.tag_id);
	if (atp == NULL) {
		ccb->ccb_h.status = CAM_REQ_INVALID;
	} else {
		atp->dead = 1;
		ccb->ccb_h.status = CAM_REQ_CMP;
	}
	rls_lun_statep(isp, tptr);
}

static void
isp_target_mark_aborted_early(ispsoftc_t *isp, tstate_t *tptr, uint32_t tag_id)
{
	atio_private_data_t *atp;
	inot_private_data_t *restart_queue = tptr->restart_queue;

	/*
	 * First, clean any commands pending restart
	 */
	tptr->restart_queue = NULL;
	while (restart_queue) {
		uint32_t this_tag_id;
		inot_private_data_t *ntp = restart_queue;

		restart_queue = ntp->rd.nt.nt_hba;

		if (IS_24XX(isp)) {
			this_tag_id = ((at7_entry_t *)ntp->rd.data)->at_rxid;
		} else {
			this_tag_id = ((at2_entry_t *)ntp->rd.data)->at_rxid;
		}
		if ((uint64_t)tag_id == TAG_ANY || tag_id == this_tag_id) {
			isp_put_ntpd(isp, tptr, ntp);
		} else {
			ntp->rd.nt.nt_hba = tptr->restart_queue;
			tptr->restart_queue = ntp;
		}
	}

	/*
	 * Now mark other ones dead as well.
	 */
	for (atp = tptr->atpool; atp < &tptr->atpool[ATPDPSIZE]; atp++) {
		if ((uint64_t)tag_id == TAG_ANY || atp->tag == tag_id) {
			atp->dead = 1;
		}
	}
}


#ifdef	ISP_INTERNAL_TARGET
//#define	ISP_SEPARATE_STATUS	1
#define	ISP_MULTI_CCBS		1
#if defined(ISP_MULTI_CCBS) && !defined(ISP_SEPARATE_STATUS)
#define	ISP_SEPARATE_STATUS 1
#endif

typedef struct periph_private_data_t {
	union ccb *ccb;			/* original ATIO or Immediate Notify */
	unsigned long	offset;		/* current offset */
	int		sequence;	/* current CTIO sequence */
	int		ctio_cnt;	/* current # of ctio's outstanding */
	int
		status_sent	: 1,
		on_queue	: 1;	/* on restart queue */
} ppd_t;
/*
 * Each ATIO we allocate will have periph private data associated with it
 * that maintains per-command state. This private to each ATIO.
 */
#define	ATIO_PPD(ccb)		((ppd_t *)(((struct ccb_hdr *)ccb)->ppriv_ptr0))
/*
 * Each CTIO we send downstream will get a pointer to the ATIO itself
 * so that on completion we can retrieve that pointer.
 */
#define	ccb_atio		ppriv_ptr1
#define	ccb_inot		ppriv_ptr1

/*
 * Each CTIO we send downstream will contain a sequence number
 */
#define	CTIO_SEQ(ccb)		ccb->ccb_h.ppriv_field0

#define	MAX_ISP_TARG_TRANSFER	(2 << 20)
#define	NISP_TARG_CMDS		64
#define	NISP_TARG_NOTIFIES	64
#define	DISK_SHIFT		9
#define	JUNK_SIZE		256
#define	MULTI_CCB_DATA_LIM	8192
//#define	MULTI_CCB_DATA_CNT	64
#define	MULTI_CCB_DATA_CNT	8

extern u_int vm_kmem_size;
static int ca;
static uint32_t disk_size;
static uint8_t *disk_data = NULL;
static uint8_t *junk_data;
static MALLOC_DEFINE(M_ISPTARG, "ISPTARG", "ISP TARGET data");
struct isptarg_softc {
	/* CCBs (CTIOs, ATIOs, INOTs) pending on the controller */
	struct isp_ccbq		work_queue;
	struct isp_ccbq		rework_queue;
	struct isp_ccbq		running_queue;
	struct isp_ccbq		inot_queue;
	struct cam_periph       *periph;
	struct cam_path	 	*path;
	ispsoftc_t		*isp;
};
static periph_ctor_t	isptargctor;
static periph_dtor_t	isptargdtor;
static periph_start_t	isptargstart;
static periph_init_t	isptarginit;
static void		isptarg_done(struct cam_periph *, union ccb *);
static void		isptargasync(void *, u_int32_t, struct cam_path *, void *);


static int isptarg_rwparm(uint8_t *, uint8_t *, uint64_t, uint32_t, uint8_t **, uint32_t *, int *);

static struct periph_driver isptargdriver =
{
	isptarginit, "isptarg", TAILQ_HEAD_INITIALIZER(isptargdriver.units), 0
};

static void
isptarginit(void)
{
}

static void
isptargnotify(ispsoftc_t *isp, union ccb *iccb, struct ccb_immediate_notify *inot)
{
	struct ccb_notify_acknowledge *ack = &iccb->cna2;

	ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, inot->ccb_h.path, "%s: [0x%x] immediate notify for 0x%x from 0x%x status 0x%x arg 0x%x\n", __func__,
	    inot->tag_id, inot->initiator_id, inot->seq_id, inot->ccb_h.status, inot->arg);
	ack->ccb_h.func_code = XPT_NOTIFY_ACKNOWLEDGE;
	ack->ccb_h.flags = 0;
	ack->ccb_h.retry_count = 0;
	ack->ccb_h.cbfcnp = isptarg_done;
	ack->ccb_h.timeout = 0;
	ack->ccb_h.ccb_inot = inot;
	ack->tag_id = inot->tag_id;
	ack->seq_id = inot->seq_id;
	ack->initiator_id = inot->initiator_id;
	xpt_action(iccb);
}

static void
isptargstart(struct cam_periph *periph, union ccb *iccb)
{
	const uint8_t niliqd[SHORT_INQUIRY_LENGTH] = {
		0x7f, 0x0, 0x5, 0x2, 32, 0, 0, 0x32,
		'F', 'R', 'E', 'E', 'B', 'S', 'D', ' ',
		'S', 'C', 'S', 'I', ' ', 'N', 'U', 'L',
		'L', ' ', 'D', 'E', 'V', 'I', 'C', 'E',
		'0', '0', '0', '1'
	};
	const uint8_t iqd[SHORT_INQUIRY_LENGTH] = {
		0, 0x0, 0x5, 0x2, 32, 0, 0, 0x32,
		'F', 'R', 'E', 'E', 'B', 'S', 'D', ' ',
		'S', 'C', 'S', 'I', ' ', 'M', 'E', 'M',
		'O', 'R', 'Y', ' ', 'D', 'I', 'S', 'K',
		'0', '0', '0', '1'
	};
	int r, i, more = 0, last, is_data_cmd = 0, is_write;
	char *queue;
	struct isptarg_softc *softc = periph->softc;
	struct ccb_scsiio *csio;
	lun_id_t return_lun;
	struct ccb_accept_tio *atio;
	uint8_t *cdb, *ptr, status;
	uint8_t *data_ptr;
	uint32_t data_len, flags;
	struct ccb_hdr *ccbh;
	    
	mtx_assert(periph->sim->mtx, MA_OWNED);
	ISP_PATH_PRT(softc->isp, ISP_LOGTDEBUG1, iccb->ccb_h.path, "%s: function code 0x%x INOTQ=%c WORKQ=%c REWORKQ=%c\n", __func__, iccb->ccb_h.func_code,
	    TAILQ_FIRST(&softc->inot_queue)? 'y' : 'n', TAILQ_FIRST(&softc->work_queue)? 'y' : 'n', TAILQ_FIRST(&softc->rework_queue)? 'y' : 'n');
	/*
	 * Check for immediate notifies first
	 */
	ccbh = TAILQ_FIRST(&softc->inot_queue);
	if (ccbh) {
		TAILQ_REMOVE(&softc->inot_queue, ccbh, periph_links.tqe);
		if (TAILQ_FIRST(&softc->inot_queue) || TAILQ_FIRST(&softc->work_queue) || TAILQ_FIRST(&softc->rework_queue)) {
			xpt_schedule(periph, 1);
		}
		isptargnotify(softc->isp, iccb, (struct ccb_immediate_notify *)ccbh);
		return;
	}

	/*
	 * Check the rework (continuation) work queue first.
	 */
	ccbh = TAILQ_FIRST(&softc->rework_queue);
	if (ccbh) {
		atio = (struct ccb_accept_tio *)ccbh;
		TAILQ_REMOVE(&softc->rework_queue, ccbh, periph_links.tqe);
		more = TAILQ_FIRST(&softc->work_queue) || TAILQ_FIRST(&softc->rework_queue);
		queue = "rework";
	} else {
		ccbh = TAILQ_FIRST(&softc->work_queue);
		if (ccbh == NULL) {
			xpt_release_ccb(iccb);
			return;
		}
		atio = (struct ccb_accept_tio *)ccbh;
		TAILQ_REMOVE(&softc->work_queue, ccbh, periph_links.tqe);
		more = TAILQ_FIRST(&softc->work_queue) != NULL;
		queue = "work";
	}
	ATIO_PPD(atio)->on_queue = 0;

	if (atio->tag_id == 0xffffffff || atio->ccb_h.func_code != XPT_ACCEPT_TARGET_IO) {
		panic("BAD ATIO");
	}

	data_len = is_write = 0;
	data_ptr = NULL;
	csio = &iccb->csio;
	status = SCSI_STATUS_OK;
	flags = CAM_SEND_STATUS;
	memset(&atio->sense_data, 0, sizeof (atio->sense_data));
	cdb = atio->cdb_io.cdb_bytes;
	ISP_PATH_PRT(softc->isp, ISP_LOGTDEBUG0, ccbh->path, "%s: [0x%x] processing ATIO from %s queue initiator 0x%x CDB=0x%x data_offset=%u\n", __func__, atio->tag_id,
	    queue, atio->init_id, cdb[0], ATIO_PPD(atio)->offset);

	return_lun = XS_LUN(atio);
	if (return_lun != 0) {
		xpt_print(atio->ccb_h.path, "[0x%x] Non-Zero Lun %d: cdb0=0x%x\n", atio->tag_id, return_lun, cdb[0]);
		if (cdb[0] != INQUIRY && cdb[0] != REPORT_LUNS && cdb[0] != REQUEST_SENSE) {
			status = SCSI_STATUS_CHECK_COND;
			SDFIXED(atio->sense_data)->error_code = SSD_ERRCODE_VALID|SSD_CURRENT_ERROR;
			SDFIXED(atio->sense_data)->flags = SSD_KEY_ILLEGAL_REQUEST;
			SDFIXED(atio->sense_data)->add_sense_code = 0x25;	/* LOGICAL UNIT NOT SUPPORTED */
			atio->sense_len = SSD_MIN_SIZE;
		}
		return_lun = CAM_LUN_WILDCARD;
	}

	switch (cdb[0]) {
	case REQUEST_SENSE:
		flags |= CAM_DIR_IN;
		data_len = sizeof (atio->sense_data);
		junk_data[0] = SSD_ERRCODE_VALID|SSD_CURRENT_ERROR|SSD_KEY_NO_SENSE;
		memset(junk_data+1, 0, data_len-1);
		if (data_len > cdb[4]) {
			data_len = cdb[4];
		}
		if (data_len) {
			data_ptr = junk_data;
		}
		break;
	case WRITE_6:
	case WRITE_10:
	case WRITE_12:
	case WRITE_16:
		is_write = 1;
		/* FALLTHROUGH */
	case READ_6:
	case READ_10:
	case READ_12:
	case READ_16:
		is_data_cmd = 1;
		r = isptarg_rwparm(cdb, disk_data, disk_size, ATIO_PPD(atio)->offset, &data_ptr, &data_len, &last);
		if (r != 0) {
			status = SCSI_STATUS_CHECK_COND;
			SDFIXED(atio->sense_data)->error_code = SSD_ERRCODE_VALID|SSD_CURRENT_ERROR;
			SDFIXED(atio->sense_data)->flags = SSD_KEY_ILLEGAL_REQUEST;
			if (r == -1) {
				SDFIXED(atio->sense_data)->add_sense_code = 0x21;	/* LOGICAL BLOCK ADDRESS OUT OF RANGE */
			} else {
				SDFIXED(atio->sense_data)->add_sense_code = 0x20;	/* INVALID COMMAND OPERATION CODE */
			}
			atio->sense_len = SSD_MIN_SIZE;
		} else {
#ifdef	ISP_SEPARATE_STATUS
			if (last && data_len) {
				last = 0;
			}
#endif
			if (last == 0) {
				flags &= ~CAM_SEND_STATUS;
			}
			if (data_len) {
				ATIO_PPD(atio)->offset += data_len;
				if (is_write)
					flags |= CAM_DIR_OUT;
				else
					flags |= CAM_DIR_IN;
			} else {
				flags |= CAM_DIR_NONE;
			}
		}
		break;
	case INQUIRY:
		flags |= CAM_DIR_IN;
		if (cdb[1] || cdb[2] || cdb[3]) {
			status = SCSI_STATUS_CHECK_COND;
			SDFIXED(atio->sense_data)->error_code = SSD_ERRCODE_VALID|SSD_CURRENT_ERROR;
			SDFIXED(atio->sense_data)->flags = SSD_KEY_UNIT_ATTENTION;
			SDFIXED(atio->sense_data)->add_sense_code = 0x24;	/* INVALID FIELD IN CDB */
			atio->sense_len = SSD_MIN_SIZE;
			break;
		}
		data_len = sizeof (iqd);
		if (data_len > cdb[4]) {
			data_len = cdb[4];
		}
		if (data_len) {
			if (XS_LUN(iccb) != 0) {
				memcpy(junk_data, niliqd, sizeof (iqd));
			} else {
				memcpy(junk_data, iqd, sizeof (iqd));
			}
			data_ptr = junk_data;
		}
		break;
	case TEST_UNIT_READY:
		flags |= CAM_DIR_NONE;
		if (ca) {
			ca = 0;
			status = SCSI_STATUS_CHECK_COND;
			SDFIXED(atio->sense_data)->error_code = SSD_ERRCODE_VALID|SSD_CURRENT_ERROR;
			SDFIXED(atio->sense_data)->flags = SSD_KEY_UNIT_ATTENTION;
			SDFIXED(atio->sense_data)->add_sense_code = 0x29;	/* POWER ON, RESET, OR BUS DEVICE RESET OCCURRED */
			atio->sense_len = SSD_MIN_SIZE;
		}
		break;
	case SYNCHRONIZE_CACHE:
	case START_STOP:
	case RESERVE:
	case RELEASE:
	case VERIFY_10:
		flags |= CAM_DIR_NONE;
		break;

	case READ_CAPACITY:
		flags |= CAM_DIR_IN;
		if (cdb[2] || cdb[3] || cdb[4] || cdb[5]) {
			status = SCSI_STATUS_CHECK_COND;
			SDFIXED(atio->sense_data)->error_code = SSD_ERRCODE_VALID|SSD_CURRENT_ERROR;
			SDFIXED(atio->sense_data)->flags = SSD_KEY_ILLEGAL_REQUEST;
			SDFIXED(atio->sense_data)->add_sense_code =  0x24;	/* INVALID FIELD IN CDB */
			atio->sense_len = SSD_MIN_SIZE;
			break;
		}
		if (cdb[8] & 0x1) { /* PMI */
			junk_data[0] = 0xff;
			junk_data[1] = 0xff;
			junk_data[2] = 0xff;
			junk_data[3] = 0xff;
		} else {
			uint64_t last_blk = (disk_size >> DISK_SHIFT) - 1;
			if (last_blk < 0xffffffffULL) {
			    junk_data[0] = (last_blk >> 24) & 0xff;
			    junk_data[1] = (last_blk >> 16) & 0xff;
			    junk_data[2] = (last_blk >>  8) & 0xff;
			    junk_data[3] = (last_blk) & 0xff;
			} else {
			    junk_data[0] = 0xff;
			    junk_data[1] = 0xff;
			    junk_data[2] = 0xff;
			    junk_data[3] = 0xff;
			}
		}
		junk_data[4] = ((1 << DISK_SHIFT) >> 24) & 0xff;
		junk_data[5] = ((1 << DISK_SHIFT) >> 16) & 0xff;
		junk_data[6] = ((1 << DISK_SHIFT) >>  8) & 0xff;
		junk_data[7] = ((1 << DISK_SHIFT)) & 0xff;
		data_ptr = junk_data;
		data_len = 8;
		break;
	case REPORT_LUNS:
		flags |= CAM_DIR_IN;
		memset(junk_data, 0, JUNK_SIZE);
		junk_data[0] = (1 << 3) >> 24;
		junk_data[1] = (1 << 3) >> 16;
		junk_data[2] = (1 << 3) >> 8;
		junk_data[3] = (1 << 3);
		ptr = NULL;
		for (i = 0; i < 1; i++) {
			ptr = &junk_data[8 + (i << 3)];
			if (i >= 256) {
				ptr[0] = 0x40 | ((i >> 8) & 0x3f);
			}
			ptr[1] = i;
		}
		data_ptr = junk_data;
		data_len = (ptr + 8) - junk_data;
		break;

	default:
		flags |= CAM_DIR_NONE;
		status = SCSI_STATUS_CHECK_COND;
		SDFIXED(atio->sense_data)->error_code = SSD_ERRCODE_VALID|SSD_CURRENT_ERROR;
		SDFIXED(atio->sense_data)->flags = SSD_KEY_ILLEGAL_REQUEST;
		SDFIXED(atio->sense_data)->add_sense_code = 0x20;	/* INVALID COMMAND OPERATION CODE */
		atio->sense_len = SSD_MIN_SIZE;
		break;
	}

	/*
	 * If we are done with the transaction, tell the
	 * controller to send status and perform a CMD_CMPLT.
	 * If we have associated sense data, see if we can
	 * send that too.
	 */
	if (status == SCSI_STATUS_CHECK_COND) {
		flags |= CAM_SEND_SENSE;
		csio->sense_len = atio->sense_len;
		csio->sense_data = atio->sense_data;
		flags &= ~CAM_DIR_MASK;
		data_len = 0;
		data_ptr = NULL;
	}
	cam_fill_ctio(csio, 0, isptarg_done, flags, MSG_SIMPLE_Q_TAG, atio->tag_id, atio->init_id, status, data_ptr, data_len, 30 * hz);
	iccb->ccb_h.target_id = atio->ccb_h.target_id;
	iccb->ccb_h.target_lun = return_lun;
	iccb->ccb_h.ccb_atio = atio;
	CTIO_SEQ(iccb) = ATIO_PPD(atio)->sequence++;
	ATIO_PPD(atio)->ctio_cnt++;
	if (flags & CAM_SEND_STATUS) {
		KASSERT((ATIO_PPD(atio)->status_sent == 0), ("we have already sent status for 0x%x in %s", atio->tag_id, __func__));
		ATIO_PPD(atio)->status_sent = 1;
	}
	ISP_PATH_PRT(softc->isp, ISP_LOGTDEBUG0, atio->ccb_h.path, "%s: sending downstream for  0x%x sequence %u len %u flags %x\n", __func__, atio->tag_id, CTIO_SEQ(iccb), data_len, flags);
	xpt_action(iccb);

	if ((atio->ccb_h.status & CAM_DEV_QFRZN) != 0) {
		cam_release_devq(periph->path, 0, 0, 0, 0); 
		atio->ccb_h.status &= ~CAM_DEV_QFRZN;
	}
#ifdef	ISP_MULTI_CCBS
	if (is_data_cmd && ATIO_PPD(atio)->status_sent == 0 && ATIO_PPD(atio)->ctio_cnt < MULTI_CCB_DATA_CNT && ATIO_PPD(atio)->on_queue == 0) {
		ISP_PATH_PRT(softc->isp, ISP_LOGTDEBUG0, atio->ccb_h.path, "%s: more still to do for 0x%x\n", __func__, atio->tag_id);
		TAILQ_INSERT_TAIL(&softc->rework_queue, &atio->ccb_h, periph_links.tqe); 
		ATIO_PPD(atio)->on_queue = 1;
		more = 1;
	}
#endif
	if (more) {
		xpt_schedule(periph, 1);
	}
}

static cam_status
isptargctor(struct cam_periph *periph, void *arg)
{
	struct isptarg_softc *softc;

	softc = (struct isptarg_softc *)arg;
	periph->softc = softc;
	softc->periph = periph;
	softc->path = periph->path;
	ISP_PATH_PRT(softc->isp, ISP_LOGTDEBUG1, periph->path, "%s called\n", __func__);
	return (CAM_REQ_CMP);
}

static void
isptargdtor(struct cam_periph *periph)
{
	struct isptarg_softc *softc;
	softc = (struct isptarg_softc *)periph->softc;
	ISP_PATH_PRT(softc->isp, ISP_LOGTDEBUG1, periph->path, "%s called\n", __func__);
	softc->periph = NULL;
	softc->path = NULL;
	periph->softc = NULL;
}

static void
isptarg_done(struct cam_periph *periph, union ccb *ccb)
{
	struct isptarg_softc *softc;
	ispsoftc_t *isp;
	uint32_t newoff;
	struct ccb_accept_tio *atio;
	struct ccb_immediate_notify *inot;
	cam_status status;

	softc = (struct isptarg_softc *)periph->softc;
	isp = softc->isp;
	status = ccb->ccb_h.status & CAM_STATUS_MASK;

	switch (ccb->ccb_h.func_code) {
	case XPT_ACCEPT_TARGET_IO:
		atio = (struct ccb_accept_tio *) ccb;
		ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, ccb->ccb_h.path, "[0x%x] ATIO seen in %s\n", atio->tag_id, __func__);
		memset(ATIO_PPD(atio), 0, sizeof (ppd_t));
		TAILQ_INSERT_TAIL(&softc->work_queue, &ccb->ccb_h, periph_links.tqe); 
		ATIO_PPD(atio)->on_queue = 1;
		xpt_schedule(periph, 1);
		break;
	case XPT_IMMEDIATE_NOTIFY:
		inot = (struct ccb_immediate_notify *) ccb;
		ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, ccb->ccb_h.path, "[0x%x] INOT for 0x%x seen in %s\n", inot->tag_id, inot->seq_id, __func__);
		TAILQ_INSERT_TAIL(&softc->inot_queue, &ccb->ccb_h, periph_links.tqe); 
		xpt_schedule(periph, 1);
		break;
	case XPT_CONT_TARGET_IO:
		atio = ccb->ccb_h.ccb_atio;
		KASSERT((ATIO_PPD(atio)->ctio_cnt != 0), ("ctio zero when finishing a CTIO"));
		ATIO_PPD(atio)->ctio_cnt--;
		if ((ccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
			switch (ccb->ccb_h.status & CAM_STATUS_MASK) {
			case CAM_MESSAGE_RECV:
				newoff = (ccb->csio.msg_ptr[3] << 24) | (ccb->csio.msg_ptr[4] << 16) | (ccb->csio.msg_ptr[5] << 8) | (ccb->csio.msg_ptr[6]);
				ISP_PATH_PRT(isp, ISP_LOGWARN, ccb->ccb_h.path, "[0x%x] got message to return to reset offset to 0x%x at sequence %u\n", atio->tag_id, newoff, CTIO_SEQ(ccb));
				ATIO_PPD(atio)->offset = newoff;
				ATIO_PPD(atio)->status_sent = 0;
				if (ATIO_PPD(atio)->on_queue == 0) {
					TAILQ_INSERT_TAIL(&softc->rework_queue, &atio->ccb_h, periph_links.tqe); 
					ATIO_PPD(atio)->on_queue = 1;
				}
				xpt_schedule(periph, 1);
				break;
			default:
				cam_error_print(ccb, CAM_ESF_ALL, CAM_EPF_ALL);
				xpt_action((union ccb *)atio);
				break;
			}
		} else if ((ccb->ccb_h.flags & CAM_SEND_STATUS) == 0) {
			ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, ccb->ccb_h.path, "[0x%x] MID CTIO sequence %u seen in %s\n", atio->tag_id, CTIO_SEQ(ccb), __func__);
			if (ATIO_PPD(atio)->status_sent == 0 && ATIO_PPD(atio)->on_queue == 0) {
				TAILQ_INSERT_TAIL(&softc->rework_queue, &atio->ccb_h, periph_links.tqe); 
				ATIO_PPD(atio)->on_queue = 1;
			}
			xpt_schedule(periph, 1);
		} else {
			KASSERT((ATIO_PPD(atio)->ctio_cnt == 0), ("ctio count still %d when we think we've sent the STATUS ctio", ATIO_PPD(atio)->ctio_cnt));
			ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, ccb->ccb_h.path, "[0x%x] FINAL CTIO sequence %u seen in %s\n", atio->tag_id, CTIO_SEQ(ccb), __func__);
			xpt_action((union ccb *)atio);
		}
		if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
			cam_release_devq(ccb->ccb_h.path, 0, 0, 0, 0); 
			ccb->ccb_h.status &= ~CAM_DEV_QFRZN;
		}
		xpt_release_ccb(ccb);
		break;
	case XPT_NOTIFY_ACKNOWLEDGE:
		if ((ccb->ccb_h.status & CAM_DEV_QFRZN) != 0) {
			cam_release_devq(ccb->ccb_h.path, 0, 0, 0, 0); 
			ccb->ccb_h.status &= ~CAM_DEV_QFRZN;
		}
		inot = ccb->ccb_h.ccb_inot;
		ISP_PATH_PRT(isp, ISP_LOGTDEBUG1, inot->ccb_h.path, "[0x%x] recycle notify for tag 0x%x\n", inot->tag_id, inot->seq_id);
		xpt_release_ccb(ccb);
		xpt_action((union ccb *)inot);
		break;
	default:
		xpt_print(ccb->ccb_h.path, "unexpected code 0x%x\n", ccb->ccb_h.func_code);
		break;
	}
}

static void
isptargasync(void *callback_arg, u_int32_t code, struct cam_path *path, void *arg)
{
	struct ac_contract *acp = arg;
	struct ac_device_changed *fc = (struct ac_device_changed *) acp->contract_data;

	if (code != AC_CONTRACT) {
		return;
	}
	xpt_print(path, "0x%016llx Port ID 0x%06x %s\n", (unsigned long long) fc->wwpn, fc->port, fc->arrived? "arrived" : "departed");
}

static void
isp_target_thread(ispsoftc_t *isp, int chan)
{
	union ccb *ccb = NULL;
	int i;
	void *wchan;
	cam_status status;
	struct isptarg_softc *softc = NULL;
	struct cam_periph *periph = NULL, *wperiph = NULL;
	struct cam_path *path, *wpath;
	struct cam_sim *sim;

	if (disk_data == NULL) {
		disk_size = roundup2(vm_kmem_size >> 1, (1ULL << 20));
		if (disk_size < (50 << 20)) {
			disk_size = 50 << 20;
		}
		disk_data = malloc(disk_size, M_ISPTARG, M_WAITOK | M_ZERO);
		if (disk_data == NULL) {
			isp_prt(isp, ISP_LOGERR, "%s: could not allocate disk data", __func__);
			goto out;
		}
		isp_prt(isp, ISP_LOGINFO, "allocated a %ju MiB disk", (uintmax_t) (disk_size >> 20));
	}
	junk_data = malloc(JUNK_SIZE, M_ISPTARG, M_WAITOK | M_ZERO);
	if (junk_data == NULL) {
		isp_prt(isp, ISP_LOGERR, "%s: could not allocate junk", __func__);
		goto out;
	}


	softc = malloc(sizeof (*softc), M_ISPTARG, M_WAITOK | M_ZERO);
	if (softc == NULL) {
		isp_prt(isp, ISP_LOGERR, "%s: could not allocate softc", __func__);
		goto out;
	}
	TAILQ_INIT(&softc->work_queue);
	TAILQ_INIT(&softc->rework_queue);
	TAILQ_INIT(&softc->running_queue);
	TAILQ_INIT(&softc->inot_queue);
	softc->isp = isp;

	periphdriver_register(&isptargdriver);
	ISP_GET_PC(isp, chan, sim, sim);
	ISP_GET_PC(isp, chan, path,  path);
	status = xpt_create_path(&wpath, NULL, cam_sim_path(sim), CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD);
	if (status != CAM_REQ_CMP) {
		isp_prt(isp, ISP_LOGERR, "%s: could not allocate wildcard path", __func__);
		return;
	}
	status = xpt_create_path(&path, NULL, cam_sim_path(sim), 0, 0);
	if (status != CAM_REQ_CMP) {
		xpt_free_path(wpath);
		isp_prt(isp, ISP_LOGERR, "%s: could not allocate path", __func__);
		return;
	}

	ISP_LOCK(isp);
	status = cam_periph_alloc(isptargctor, NULL, isptargdtor, isptargstart, "isptarg", CAM_PERIPH_BIO, wpath, NULL, 0, softc);
	if (status != CAM_REQ_CMP) {
		ISP_UNLOCK(isp);
		isp_prt(isp, ISP_LOGERR, "%s: cam_periph_alloc for wildcard failed", __func__);
		goto out;
	}
	wperiph = cam_periph_find(wpath, "isptarg");
	if (wperiph == NULL) {
		ISP_UNLOCK(isp);
		isp_prt(isp, ISP_LOGERR, "%s: wildcard periph already allocated but doesn't exist", __func__);
		goto out;
	}

	status = cam_periph_alloc(isptargctor, NULL, isptargdtor, isptargstart, "isptarg", CAM_PERIPH_BIO, path, NULL, 0, softc);
	if (status != CAM_REQ_CMP) {
		ISP_UNLOCK(isp);
		isp_prt(isp, ISP_LOGERR, "%s: cam_periph_alloc failed", __func__);
		goto out;
	}

	periph = cam_periph_find(path, "isptarg");
	if (periph == NULL) {
		ISP_UNLOCK(isp);
		isp_prt(isp, ISP_LOGERR, "%s: periph already allocated but doesn't exist", __func__);
		goto out;
	}

	status = xpt_register_async(AC_CONTRACT, isptargasync, isp, wpath);
	if (status != CAM_REQ_CMP) {
		ISP_UNLOCK(isp);
		isp_prt(isp, ISP_LOGERR, "%s: xpt_register_async failed", __func__);
		goto out;
	}

	ISP_UNLOCK(isp);

	ccb = xpt_alloc_ccb();

	/*
	 * Make sure role is none.
	 */
	xpt_setup_ccb(&ccb->ccb_h, periph->path, 10);
	ccb->ccb_h.func_code = XPT_SET_SIM_KNOB;
	ccb->knob.xport_specific.fc.role = KNOB_ROLE_NONE;
	ccb->knob.xport_specific.fc.valid = KNOB_VALID_ROLE;

	ISP_LOCK(isp);
	xpt_action(ccb);
	ISP_UNLOCK(isp);

	/*
	 * Now enable luns
	 */
	xpt_setup_ccb(&ccb->ccb_h, periph->path, 10);
	ccb->ccb_h.func_code = XPT_EN_LUN;
	ccb->cel.enable = 1;
	ISP_LOCK(isp);
	xpt_action(ccb);
	ISP_UNLOCK(isp);
	if (ccb->ccb_h.status != CAM_REQ_CMP) {
		xpt_free_ccb(ccb);
		xpt_print(periph->path, "failed to enable lun (0x%x)\n", ccb->ccb_h.status);
		goto out;
	}

	xpt_setup_ccb(&ccb->ccb_h, wperiph->path, 10);
	ccb->ccb_h.func_code = XPT_EN_LUN;
	ccb->cel.enable = 1;
	ISP_LOCK(isp);
	xpt_action(ccb);
	ISP_UNLOCK(isp);
	if (ccb->ccb_h.status != CAM_REQ_CMP) {
		xpt_free_ccb(ccb);
		xpt_print(wperiph->path, "failed to enable lun (0x%x)\n", ccb->ccb_h.status);
		goto out;
	}
	xpt_free_ccb(ccb);

	/*
	 * Add resources
	 */
	ISP_GET_PC_ADDR(isp, chan, target_proc, wchan);
	for (i = 0; i < 4; i++) {
		ccb = malloc(sizeof (*ccb), M_ISPTARG, M_WAITOK | M_ZERO);
		xpt_setup_ccb(&ccb->ccb_h, wperiph->path, 1);
		ccb->ccb_h.func_code = XPT_ACCEPT_TARGET_IO;
		ccb->ccb_h.cbfcnp = isptarg_done;
		ccb->ccb_h.ppriv_ptr0 = malloc(sizeof (ppd_t), M_ISPTARG, M_WAITOK | M_ZERO);
		ISP_LOCK(isp);
		xpt_action(ccb);
		ISP_UNLOCK(isp);
	}
	for (i = 0; i < NISP_TARG_CMDS; i++) {
		ccb = malloc(sizeof (*ccb), M_ISPTARG, M_WAITOK | M_ZERO);
		xpt_setup_ccb(&ccb->ccb_h, periph->path, 1);
		ccb->ccb_h.func_code = XPT_ACCEPT_TARGET_IO;
		ccb->ccb_h.cbfcnp = isptarg_done;
		ccb->ccb_h.ppriv_ptr0 = malloc(sizeof (ppd_t), M_ISPTARG, M_WAITOK | M_ZERO);
		ISP_LOCK(isp);
		xpt_action(ccb);
		ISP_UNLOCK(isp);
	}
	for (i = 0; i < 4; i++) {
		ccb = malloc(sizeof (*ccb), M_ISPTARG, M_WAITOK | M_ZERO);
		xpt_setup_ccb(&ccb->ccb_h, wperiph->path, 1);
		ccb->ccb_h.func_code = XPT_IMMEDIATE_NOTIFY;
		ccb->ccb_h.cbfcnp = isptarg_done;
		ISP_LOCK(isp);
		xpt_action(ccb);
		ISP_UNLOCK(isp);
	}
	for (i = 0; i < NISP_TARG_NOTIFIES; i++) {
		ccb = malloc(sizeof (*ccb), M_ISPTARG, M_WAITOK | M_ZERO);
		xpt_setup_ccb(&ccb->ccb_h, periph->path, 1);
		ccb->ccb_h.func_code = XPT_IMMEDIATE_NOTIFY;
		ccb->ccb_h.cbfcnp = isptarg_done;
		ISP_LOCK(isp);
		xpt_action(ccb);
		ISP_UNLOCK(isp);
	}

	/*
	 * Now turn it all back on
	 */
	xpt_setup_ccb(&ccb->ccb_h, periph->path, 10);
	ccb->ccb_h.func_code = XPT_SET_SIM_KNOB;
	ccb->knob.xport_specific.fc.valid = KNOB_VALID_ROLE;
	ccb->knob.xport_specific.fc.role = KNOB_ROLE_TARGET;
	ISP_LOCK(isp);
	xpt_action(ccb);
	ISP_UNLOCK(isp);

	/*
	 * Okay, while things are still active, sleep...
	 */
	ISP_LOCK(isp);
	for (;;) {
		ISP_GET_PC(isp, chan, proc_active, i);
		if (i == 0) {
			break;
		}
		msleep(wchan, &isp->isp_lock, PUSER, "tsnooze", 0);
	}
	ISP_UNLOCK(isp);

out:
	if (wperiph) {
		cam_periph_invalidate(wperiph);
	}
	if (periph) {
		cam_periph_invalidate(periph);
	}
	if (junk_data) {
		free(junk_data, M_ISPTARG);
	}
	if (disk_data) {
		free(disk_data, M_ISPTARG);
	}
	if (softc) {
		free(softc, M_ISPTARG);
	}
	xpt_free_path(path);
	xpt_free_path(wpath);
}

static void
isp_target_thread_pi(void *arg)
{
	struct isp_spi *pi = arg;
	isp_target_thread(cam_sim_softc(pi->sim), cam_sim_bus(pi->sim));
}

static void
isp_target_thread_fc(void *arg)
{
	struct isp_fc *fc = arg;
	isp_target_thread(cam_sim_softc(fc->sim), cam_sim_bus(fc->sim));
}

static int
isptarg_rwparm(uint8_t *cdb, uint8_t *dp, uint64_t dl, uint32_t offset, uint8_t **kp, uint32_t *tl, int *lp)
{
	uint32_t cnt, curcnt;
	uint64_t lba;

	switch (cdb[0]) {
	case WRITE_16:
	case READ_16:
		cnt =	(((uint32_t)cdb[10]) <<  24) |
			(((uint32_t)cdb[11]) <<  16) |
			(((uint32_t)cdb[12]) <<   8) |
			((uint32_t)cdb[13]);

		lba =	(((uint64_t)cdb[2]) << 56) |
			(((uint64_t)cdb[3]) << 48) |
			(((uint64_t)cdb[4]) << 40) |
			(((uint64_t)cdb[5]) << 32) |
			(((uint64_t)cdb[6]) << 24) |
			(((uint64_t)cdb[7]) << 16) |
			(((uint64_t)cdb[8]) <<  8) |
			((uint64_t)cdb[9]);
		break;
	case WRITE_12:
	case READ_12:
		cnt =	(((uint32_t)cdb[6]) <<  16) |
			(((uint32_t)cdb[7]) <<   8) |
			((u_int32_t)cdb[8]);

		lba =	(((uint32_t)cdb[2]) << 24) |
			(((uint32_t)cdb[3]) << 16) |
			(((uint32_t)cdb[4]) <<  8) |
			((uint32_t)cdb[5]);
		break;
	case WRITE_10:
	case READ_10:
		cnt =	(((uint32_t)cdb[7]) <<  8) |
			((u_int32_t)cdb[8]);

		lba =	(((uint32_t)cdb[2]) << 24) |
			(((uint32_t)cdb[3]) << 16) |
			(((uint32_t)cdb[4]) <<  8) |
			((uint32_t)cdb[5]);
		break;
	case WRITE_6:
	case READ_6:
		cnt = cdb[4];
		if (cnt == 0) {
			cnt = 256;
		}
		lba =	(((uint32_t)cdb[1] & 0x1f) << 16) |
			(((uint32_t)cdb[2]) << 8) |
			((uint32_t)cdb[3]);
		break;
	default:
		return (-1);
	}

	cnt <<= DISK_SHIFT;
	lba <<= DISK_SHIFT;

	if (offset == cnt) {
		*lp = 1;
		return (0);
	}

	if (lba + cnt > dl) {
		return (-2);
	}

	curcnt = MAX_ISP_TARG_TRANSFER;
	if (offset + curcnt >= cnt) {
		curcnt = cnt - offset;
		*lp = 1;
	} else {
		*lp = 0;
	}
#ifdef	ISP_MULTI_CCBS
	if (curcnt > MULTI_CCB_DATA_LIM)
		curcnt = MULTI_CCB_DATA_LIM;
#endif
	*tl = curcnt;
	*kp = &dp[lba + offset];
	return (0);
}

#endif
#endif

static void
isp_cam_async(void *cbarg, uint32_t code, struct cam_path *path, void *arg)
{
	struct cam_sim *sim;
	int bus, tgt;
	ispsoftc_t *isp;

	sim = (struct cam_sim *)cbarg;
	isp = (ispsoftc_t *) cam_sim_softc(sim);
	bus = cam_sim_bus(sim);
	tgt = xpt_path_target_id(path);

	switch (code) {
	case AC_LOST_DEVICE:
		if (IS_SCSI(isp)) {
			uint16_t oflags, nflags;
			sdparam *sdp = SDPARAM(isp, bus);

			if (tgt >= 0) {
				nflags = sdp->isp_devparam[tgt].nvrm_flags;
#ifndef	ISP_TARGET_MODE
				nflags &= DPARM_SAFE_DFLT;
				if (isp->isp_loaded_fw) {
					nflags |= DPARM_NARROW | DPARM_ASYNC;
				}
#else
				nflags = DPARM_DEFAULT;
#endif
				oflags = sdp->isp_devparam[tgt].goal_flags;
				sdp->isp_devparam[tgt].goal_flags = nflags;
				sdp->isp_devparam[tgt].dev_update = 1;
				sdp->update = 1;
				(void) isp_control(isp, ISPCTL_UPDATE_PARAMS, bus);
				sdp->isp_devparam[tgt].goal_flags = oflags;
			}
		}
		break;
	default:
		isp_prt(isp, ISP_LOGWARN, "isp_cam_async: Code 0x%x", code);
		break;
	}
}

static void
isp_poll(struct cam_sim *sim)
{
	ispsoftc_t *isp = cam_sim_softc(sim);
	uint32_t isr;
	uint16_t sema, mbox;

	if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
		isp_intr(isp, isr, sema, mbox);
	}
}


static void
isp_watchdog(void *arg)
{
	struct ccb_scsiio *xs = arg;
	ispsoftc_t *isp;
	uint32_t ohandle = ISP_HANDLE_FREE, handle;

	isp = XS_ISP(xs);

	handle = isp_find_handle(isp, xs);

	/*
	 * Hand crank the interrupt code just to be sure the command isn't stuck somewhere.
	 */
	if (handle != ISP_HANDLE_FREE) {
		uint32_t isr;
		uint16_t sema, mbox;
		if (ISP_READ_ISR(isp, &isr, &sema, &mbox) != 0) {
			isp_intr(isp, isr, sema, mbox);
		}
		ohandle = handle;
		handle = isp_find_handle(isp, xs);
	}
	if (handle != ISP_HANDLE_FREE) {
		/*
		 * Try and make sure the command is really dead before
		 * we release the handle (and DMA resources) for reuse.
		 *
		 * If we are successful in aborting the command then
		 * we're done here because we'll get the command returned
		 * back separately.
		 */
		if (isp_control(isp, ISPCTL_ABORT_CMD, xs) == 0) {
			return;
		}

		/*
		 * Note that after calling the above, the command may in
		 * fact have been completed.
		 */
		xs = isp_find_xs(isp, handle);

		/*
		 * If the command no longer exists, then we won't
		 * be able to find the xs again with this handle.
		 */
		if (xs == NULL) {
			return;
		}

		/*
		 * After this point, the command is really dead.
		 */
		if (XS_XFRLEN(xs)) {
			ISP_DMAFREE(isp, xs, handle);
		} 
		isp_destroy_handle(isp, handle);
		isp_prt(isp, ISP_LOGERR, "%s: timeout for handle 0x%x", __func__, handle);
		xs->ccb_h.status &= ~CAM_STATUS_MASK;
		xs->ccb_h.status |= CAM_CMD_TIMEOUT;
		isp_prt_endcmd(isp, xs);
		isp_done(xs);
	} else {
		if (ohandle != ISP_HANDLE_FREE) {
			isp_prt(isp, ISP_LOGWARN, "%s: timeout for handle 0x%x, recovered during interrupt", __func__, ohandle);
		} else {
			isp_prt(isp, ISP_LOGWARN, "%s: timeout for handle already free", __func__);
		}
	}
}

static void
isp_make_here(ispsoftc_t *isp, fcportdb_t *fcp, int chan, int tgt)
{
	union ccb *ccb;
	struct isp_fc *fc = ISP_FC_PC(isp, chan);

	if (isp_autoconfig == 0) {
		return;
	}

	/*
	 * Allocate a CCB, create a wildcard path for this target and schedule a rescan.
	 */
	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		isp_prt(isp, ISP_LOGWARN, "Chan %d unable to alloc CCB for rescan", chan);
		return;
	}
	if (xpt_create_path(&ccb->ccb_h.path, NULL, cam_sim_path(fc->sim),
	    tgt, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		isp_prt(isp, ISP_LOGWARN, "unable to create path for rescan");
		xpt_free_ccb(ccb);
		return;
	}

	/*
	 * Since we're about to issue a rescan, mark this device as not
	 * reported gone.
	 */
	fcp->reported_gone = 0;

	xpt_rescan(ccb);
}

static void
isp_make_gone(ispsoftc_t *isp, fcportdb_t *fcp, int chan, int tgt)
{
	struct cam_path *tp;
	struct isp_fc *fc = ISP_FC_PC(isp, chan);

	if (isp_autoconfig == 0) {
		return;
	}
	if (xpt_create_path(&tp, NULL, cam_sim_path(fc->sim), tgt, CAM_LUN_WILDCARD) == CAM_REQ_CMP) {
		/*
		 * We're about to send out the lost device async
		 * notification, so indicate that we have reported it gone.
		 */
		fcp->reported_gone = 1;
		xpt_async(AC_LOST_DEVICE, tp, NULL);
		xpt_free_path(tp);
	}
}

/*
 * Gone Device Timer Function- when we have decided that a device has gone
 * away, we wait a specific period of time prior to telling the OS it has
 * gone away.
 *
 * This timer function fires once a second and then scans the port database
 * for devices that are marked dead but still have a virtual target assigned.
 * We decrement a counter for that port database entry, and when it hits zero,
 * we tell the OS the device has gone away.
 */
static void
isp_gdt(void *arg)
{
	struct isp_fc *fc = arg;
	taskqueue_enqueue(taskqueue_thread, &fc->gtask);
}

static void
isp_gdt_task(void *arg, int pending)
{
	struct isp_fc *fc = arg;
	ispsoftc_t *isp = fc->isp;
	int chan = fc - isp->isp_osinfo.pc.fc;
	fcportdb_t *lp;
	int dbidx, tgt, more_to_do = 0;

	ISP_LOCK(isp);
	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d GDT timer expired", chan);
	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &FCPARAM(isp, chan)->portdb[dbidx];

		if (lp->state != FC_PORTDB_STATE_ZOMBIE) {
			continue;
		}
		if (lp->dev_map_idx == 0 || lp->target_mode) {
			continue;
		}
		if (lp->gone_timer != 0) {
			isp_prt(isp, ISP_LOG_SANCFG, "%s: Chan %d more to do for target %u (timer=%u)", __func__, chan, lp->dev_map_idx - 1, lp->gone_timer);
			lp->gone_timer -= 1;
			more_to_do++;
			continue;
		}
		tgt = lp->dev_map_idx - 1;
		FCPARAM(isp, chan)->isp_dev_map[tgt] = 0;
		lp->dev_map_idx = 0;
		lp->state = FC_PORTDB_STATE_NIL;
		isp_prt(isp, ISP_LOGCONFIG, prom3, chan, lp->portid, tgt, "Gone Device Timeout");
		isp_make_gone(isp, lp, chan, tgt);
	}
	if (fc->ready) {
		if (more_to_do) {
			callout_reset(&fc->gdt, hz, isp_gdt, fc);
		} else {
			callout_deactivate(&fc->gdt);
			isp_prt(isp, ISP_LOG_SANCFG, "Chan %d Stopping Gone Device Timer @ %lu", chan, (unsigned long) time_uptime);
		}
	}
	ISP_UNLOCK(isp);
}

/*
 * Loop Down Timer Function- when loop goes down, a timer is started and
 * and after it expires we come here and take all probational devices that
 * the OS knows about and the tell the OS that they've gone away.
 * 
 * We don't clear the devices out of our port database because, when loop
 * come back up, we have to do some actual cleanup with the chip at that
 * point (implicit PLOGO, e.g., to get the chip's port database state right).
 */
static void
isp_ldt(void *arg)
{
	struct isp_fc *fc = arg;
	taskqueue_enqueue(taskqueue_thread, &fc->ltask);
}

static void
isp_ldt_task(void *arg, int pending)
{
	struct isp_fc *fc = arg;
	ispsoftc_t *isp = fc->isp;
	int chan = fc - isp->isp_osinfo.pc.fc;
	fcportdb_t *lp;
	int dbidx, tgt, i;

	ISP_LOCK(isp);
	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Chan %d Loop Down Timer expired @ %lu", chan, (unsigned long) time_uptime);
	callout_deactivate(&fc->ldt);

	/*
	 * Notify to the OS all targets who we now consider have departed.
	 */
	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &FCPARAM(isp, chan)->portdb[dbidx];

		if (lp->state != FC_PORTDB_STATE_PROBATIONAL) {
			continue;
		}
		if (lp->dev_map_idx == 0 || lp->target_mode) {
			continue;
		}

		/*
		 * XXX: CLEAN UP AND COMPLETE ANY PENDING COMMANDS FIRST!
		 */


		for (i = 0; i < isp->isp_maxcmds; i++) {
			struct ccb_scsiio *xs;

			if (!ISP_VALID_HANDLE(isp, isp->isp_xflist[i].handle)) {
				continue;
			}
			if ((xs = isp->isp_xflist[i].cmd) == NULL) {
				continue;
                        }
			if (dbidx != (FCPARAM(isp, chan)->isp_dev_map[XS_TGT(xs)] - 1)) {
				continue;
			}
			isp_prt(isp, ISP_LOGWARN, "command handle 0x%x for %d.%d.%d orphaned by loop down timeout",
			    isp->isp_xflist[i].handle, chan, XS_TGT(xs), XS_LUN(xs));
		}

		/*
		 * Mark that we've announced that this device is gone....
		 */
		lp->announced = 1;

		/*
		 * but *don't* change the state of the entry. Just clear
		 * any target id stuff and announce to CAM that the
		 * device is gone. This way any necessary PLOGO stuff
		 * will happen when loop comes back up.
		 */

		tgt = lp->dev_map_idx - 1;
		FCPARAM(isp, chan)->isp_dev_map[tgt] = 0;
		lp->dev_map_idx = 0;
		lp->state = FC_PORTDB_STATE_NIL;
		isp_prt(isp, ISP_LOGCONFIG, prom3, chan, lp->portid, tgt, "Loop Down Timeout");
		isp_make_gone(isp, lp, chan, tgt);
	}

	if (FCPARAM(isp, chan)->role & ISP_ROLE_INITIATOR) {
		isp_unfreeze_loopdown(isp, chan);
	}
	/*
	 * The loop down timer has expired. Wake up the kthread
	 * to notice that fact (or make it false).
	 */
	fc->loop_dead = 1;
	fc->loop_down_time = fc->loop_down_limit+1;
	wakeup(fc);
	ISP_UNLOCK(isp);
}

static void
isp_kthread(void *arg)
{
	struct isp_fc *fc = arg;
	ispsoftc_t *isp = fc->isp;
	int chan = fc - isp->isp_osinfo.pc.fc;
	int slp = 0;

	mtx_lock(&isp->isp_osinfo.lock);

	for (;;) {
		int lb, lim;

		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "%s: Chan %d checking FC state", __func__, chan);
		lb = isp_fc_runstate(isp, chan, 250000);

		/*
		 * Our action is different based upon whether we're supporting
		 * Initiator mode or not. If we are, we might freeze the simq
		 * when loop is down and set all sorts of different delays to
		 * check again.
		 *
		 * If not, we simply just wait for loop to come up.
		 */
		if (lb && (FCPARAM(isp, chan)->role & ISP_ROLE_INITIATOR)) {
			/*
			 * Increment loop down time by the last sleep interval
			 */
			fc->loop_down_time += slp;

			if (lb < 0) {
				isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "%s: Chan %d FC loop not up (down count %d)", __func__, chan, fc->loop_down_time);
			} else {
				isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "%s: Chan %d FC got to %d (down count %d)", __func__, chan, lb, fc->loop_down_time);
			}

			/*
			 * If we've never seen loop up and we've waited longer
			 * than quickboot time, or we've seen loop up but we've
			 * waited longer than loop_down_limit, give up and go
			 * to sleep until loop comes up.
			 */
			if (FCPARAM(isp, chan)->loop_seen_once == 0) {
				lim = isp_quickboot_time;
			} else {
				lim = fc->loop_down_limit;
			}
			if (fc->loop_down_time >= lim) {
				isp_freeze_loopdown(isp, chan, "loop limit hit");
				slp = 0;
			} else if (fc->loop_down_time < 10) {
				slp = 1;
			} else if (fc->loop_down_time < 30) {
				slp = 5;
			} else if (fc->loop_down_time < 60) {
				slp = 10;
			} else if (fc->loop_down_time < 120) {
				slp = 20;
			} else {
				slp = 30;
			}

		} else if (lb) {
			isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "%s: Chan %d FC Loop Down", __func__, chan);
			fc->loop_down_time += slp;
			if (fc->loop_down_time > 300)
				slp = 0;
			else
				slp = 60;
		} else {
			isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "%s: Chan %d FC state OK", __func__, chan);
			fc->loop_down_time = 0;
			slp = 0;
		}


		/*
		 * If this is past the first loop up or the loop is dead and if we'd frozen the simq, unfreeze it
		 * now so that CAM can start sending us commands.
		 *
		 * If the FC state isn't okay yet, they'll hit that in isp_start which will freeze the queue again
		 * or kill the commands, as appropriate.
		 */

		if (FCPARAM(isp, chan)->loop_seen_once || fc->loop_dead) {
			isp_unfreeze_loopdown(isp, chan);
		}

		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "%s: Chan %d sleep time %d", __func__, chan, slp);

		msleep(fc, &isp->isp_osinfo.lock, PRIBIO, "ispf", slp * hz);

		/*
		 * If slp is zero, we're waking up for the first time after
		 * things have been okay. In this case, we set a deferral state
		 * for all commands and delay hysteresis seconds before starting
		 * the FC state evaluation. This gives the loop/fabric a chance
		 * to settle.
		 */
		if (slp == 0 && fc->hysteresis) {
			isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "%s: Chan %d sleep hysteresis ticks %d", __func__, chan, fc->hysteresis * hz);
			mtx_unlock(&isp->isp_osinfo.lock);
			pause("ispt", fc->hysteresis * hz);
			mtx_lock(&isp->isp_osinfo.lock);
		}
	}
	mtx_unlock(&isp->isp_osinfo.lock);
}

static void
isp_action(struct cam_sim *sim, union ccb *ccb)
{
	int bus, tgt, ts, error, lim;
	ispsoftc_t *isp;
	struct ccb_trans_settings *cts;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("isp_action\n"));

	isp = (ispsoftc_t *)cam_sim_softc(sim);
	mtx_assert(&isp->isp_lock, MA_OWNED);

	if (isp->isp_state != ISP_RUNSTATE && ccb->ccb_h.func_code == XPT_SCSI_IO) {
		isp_init(isp);
		if (isp->isp_state != ISP_INITSTATE) {
			/*
			 * Lie. Say it was a selection timeout.
			 */
			ccb->ccb_h.status = CAM_SEL_TIMEOUT | CAM_DEV_QFRZN;
			xpt_freeze_devq(ccb->ccb_h.path, 1);
			xpt_done(ccb);
			return;
		}
		isp->isp_state = ISP_RUNSTATE;
	}
	isp_prt(isp, ISP_LOGDEBUG2, "isp_action code %x", ccb->ccb_h.func_code);
	ISP_PCMD(ccb) = NULL;

	switch (ccb->ccb_h.func_code) {
	case XPT_SCSI_IO:	/* Execute the requested I/O operation */
		bus = XS_CHANNEL(ccb);
		/*
		 * Do a couple of preliminary checks...
		 */
		if ((ccb->ccb_h.flags & CAM_CDB_POINTER) != 0) {
			if ((ccb->ccb_h.flags & CAM_CDB_PHYS) != 0) {
				ccb->ccb_h.status = CAM_REQ_INVALID;
				xpt_done(ccb);
				break;
			}
		}
		ccb->csio.req_map = NULL;
#ifdef	DIAGNOSTIC
		if (ccb->ccb_h.target_id > (ISP_MAX_TARGETS(isp) - 1)) {
			xpt_print(ccb->ccb_h.path, "invalid target\n");
			ccb->ccb_h.status = CAM_PATH_INVALID;
		} else if (ccb->ccb_h.target_lun > (ISP_MAX_LUNS(isp) - 1)) {
			xpt_print(ccb->ccb_h.path, "invalid lun\n");
			ccb->ccb_h.status = CAM_PATH_INVALID;
		}
		if (ccb->ccb_h.status == CAM_PATH_INVALID) {
			xpt_done(ccb);
			break;
		}
#endif
		ccb->csio.scsi_status = SCSI_STATUS_OK;
		if (isp_get_pcmd(isp, ccb)) {
			isp_prt(isp, ISP_LOGWARN, "out of PCMDs");
			cam_freeze_devq(ccb->ccb_h.path);
			cam_release_devq(ccb->ccb_h.path, RELSIM_RELEASE_AFTER_TIMEOUT, 0, 250, 0);
			xpt_done(ccb);
			break;
		}
		error = isp_start((XS_T *) ccb);
		switch (error) {
		case CMD_QUEUED:
			ccb->ccb_h.status |= CAM_SIM_QUEUED;
			if (ccb->ccb_h.timeout == CAM_TIME_INFINITY) {
				break;
			}
			ts = ccb->ccb_h.timeout;
			if (ts == CAM_TIME_DEFAULT) {
				ts = 60*1000;
			}
			ts = isp_mstohz(ts);
			callout_reset(&PISP_PCMD(ccb)->wdog, ts, isp_watchdog, ccb);
			break;
		case CMD_RQLATER:
			/*
			 * We get this result for FC devices if the loop state isn't ready yet
			 * or if the device in question has gone zombie on us.
			 *
			 * If we've never seen Loop UP at all, we requeue this request and wait
			 * for the initial loop up delay to expire.
			 */
			lim = ISP_FC_PC(isp, bus)->loop_down_limit;
			if (FCPARAM(isp, bus)->loop_seen_once == 0 || ISP_FC_PC(isp, bus)->loop_down_time >= lim) {
				if (FCPARAM(isp, bus)->loop_seen_once == 0) {
					isp_prt(isp, ISP_LOGDEBUG0, "%d.%d loop not seen yet @ %lu", XS_TGT(ccb), XS_LUN(ccb), (unsigned long) time_uptime);
				} else {
					isp_prt(isp, ISP_LOGDEBUG0, "%d.%d downtime (%d) > lim (%d)", XS_TGT(ccb), XS_LUN(ccb), ISP_FC_PC(isp, bus)->loop_down_time, lim);
				}
				ccb->ccb_h.status = CAM_SEL_TIMEOUT|CAM_DEV_QFRZN;
				xpt_freeze_devq(ccb->ccb_h.path, 1);
				isp_free_pcmd(isp, ccb);
				xpt_done(ccb);
				break;
			}
			isp_prt(isp, ISP_LOGDEBUG0, "%d.%d retry later", XS_TGT(ccb), XS_LUN(ccb));
			cam_freeze_devq(ccb->ccb_h.path);
			cam_release_devq(ccb->ccb_h.path, RELSIM_RELEASE_AFTER_TIMEOUT, 0, 1000, 0);
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			isp_free_pcmd(isp, ccb);
			xpt_done(ccb);
			break;
		case CMD_EAGAIN:
			isp_free_pcmd(isp, ccb);
			cam_freeze_devq(ccb->ccb_h.path);
			cam_release_devq(ccb->ccb_h.path, RELSIM_RELEASE_AFTER_TIMEOUT, 0, 100, 0);
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			xpt_done(ccb);
			break;
		case CMD_COMPLETE:
			isp_done((struct ccb_scsiio *) ccb);
			break;
		default:
			isp_prt(isp, ISP_LOGERR, "What's this? 0x%x at %d in file %s", error, __LINE__, __FILE__);
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
			isp_free_pcmd(isp, ccb);
			xpt_done(ccb);
		}
		break;

#ifdef	ISP_TARGET_MODE
	case XPT_EN_LUN:		/* Enable/Disable LUN as a target */
		if (ccb->cel.enable) {
			isp_enable_lun(isp, ccb);
		} else {
			isp_disable_lun(isp, ccb);
		}
		break;
	case XPT_IMMED_NOTIFY:
	case XPT_IMMEDIATE_NOTIFY:	/* Add Immediate Notify Resource */
	case XPT_ACCEPT_TARGET_IO:	/* Add Accept Target IO Resource */
	{
		tstate_t *tptr = get_lun_statep(isp, XS_CHANNEL(ccb), ccb->ccb_h.target_lun);
		if (tptr == NULL) {
			tptr = get_lun_statep(isp, XS_CHANNEL(ccb), CAM_LUN_WILDCARD);
		}
		if (tptr == NULL) {
			const char *str;
			uint32_t tag;

			if (ccb->ccb_h.func_code == XPT_IMMEDIATE_NOTIFY) {
				str = "XPT_IMMEDIATE_NOTIFY";
				tag = ccb->cin1.seq_id;
			} else {
				tag = ccb->atio.tag_id;
				str = "XPT_ACCEPT_TARGET_IO";
			}
			ISP_PATH_PRT(isp, ISP_LOGWARN, ccb->ccb_h.path, "%s: [0x%x] no state pointer found for %s\n", __func__, tag, str);
			dump_tstates(isp, XS_CHANNEL(ccb));
			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
			break;
		}
		ccb->ccb_h.spriv_field0 = 0;
		ccb->ccb_h.spriv_ptr1 = isp;

		if (ccb->ccb_h.func_code == XPT_ACCEPT_TARGET_IO) {
			if (ccb->atio.tag_id) {
				atio_private_data_t *atp = isp_find_atpd(isp, tptr, ccb->atio.tag_id);
				if (atp) {
					isp_put_atpd(isp, tptr, atp);
				}
			}
			tptr->atio_count++;
			SLIST_INSERT_HEAD(&tptr->atios, &ccb->ccb_h, sim_links.sle);
			ISP_PATH_PRT(isp, ISP_LOGTDEBUG2, ccb->ccb_h.path, "Put FREE ATIO (tag id 0x%x), count now %d\n",
			    ccb->atio.tag_id, tptr->atio_count);
			ccb->atio.tag_id = 0;
		} else if (ccb->ccb_h.func_code == XPT_IMMEDIATE_NOTIFY) {
			if (ccb->cin1.tag_id) {
				inot_private_data_t *ntp = isp_find_ntpd(isp, tptr, ccb->cin1.tag_id, ccb->cin1.seq_id);
				if (ntp) {
					isp_put_ntpd(isp, tptr, ntp);
				}
			}
			tptr->inot_count++;
			SLIST_INSERT_HEAD(&tptr->inots, &ccb->ccb_h, sim_links.sle);
			ISP_PATH_PRT(isp, ISP_LOGTDEBUG2, ccb->ccb_h.path, "Put FREE INOT, (seq id 0x%x) count now %d\n",
			    ccb->cin1.seq_id, tptr->inot_count);
			ccb->cin1.seq_id = 0;
		} else if (ccb->ccb_h.func_code == XPT_IMMED_NOTIFY) {
			tptr->inot_count++;
			SLIST_INSERT_HEAD(&tptr->inots, &ccb->ccb_h, sim_links.sle);
			ISP_PATH_PRT(isp, ISP_LOGTDEBUG2, ccb->ccb_h.path, "Put FREE INOT, (seq id 0x%x) count now %d\n",
			    ccb->cin1.seq_id, tptr->inot_count);
			ccb->cin1.seq_id = 0;
		}
		rls_lun_statep(isp, tptr);
		ccb->ccb_h.status = CAM_REQ_INPROG;
		break;
	}
	case XPT_NOTIFY_ACK:
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		break;
	case XPT_NOTIFY_ACKNOWLEDGE:		/* notify ack */
	{
		tstate_t *tptr;
		inot_private_data_t *ntp;

		/*
		 * XXX: Because we cannot guarantee that the path information in the notify acknowledge ccb
		 * XXX: matches that for the immediate notify, we have to *search* for the notify structure
		 */
		/*
		 * All the relevant path information is in the associated immediate notify
		 */
		ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, ccb->ccb_h.path, "%s: [0x%x] NOTIFY ACKNOWLEDGE for 0x%x seen\n", __func__, ccb->cna2.tag_id, ccb->cna2.seq_id);
		ntp = get_ntp_from_tagdata(isp, ccb->cna2.tag_id, ccb->cna2.seq_id, &tptr);
		if (ntp == NULL) {
			ISP_PATH_PRT(isp, ISP_LOGWARN, ccb->ccb_h.path, "%s: [0x%x] XPT_NOTIFY_ACKNOWLEDGE of 0x%x cannot find ntp private data\n", __func__,
			     ccb->cna2.tag_id, ccb->cna2.seq_id);
			ccb->ccb_h.status = CAM_DEV_NOT_THERE;
			xpt_done(ccb);
			break;
		}
		if (isp_handle_platform_target_notify_ack(isp, &ntp->rd.nt)) {
			rls_lun_statep(isp, tptr);
			cam_freeze_devq(ccb->ccb_h.path);
			cam_release_devq(ccb->ccb_h.path, RELSIM_RELEASE_AFTER_TIMEOUT, 0, 1000, 0);
			ccb->ccb_h.status &= ~CAM_STATUS_MASK;
			ccb->ccb_h.status |= CAM_REQUEUE_REQ;
			break;
		}
		isp_put_ntpd(isp, tptr, ntp);
		rls_lun_statep(isp, tptr);
		ccb->ccb_h.status = CAM_REQ_CMP;
		ISP_PATH_PRT(isp, ISP_LOGTDEBUG0, ccb->ccb_h.path, "%s: [0x%x] calling xpt_done for tag 0x%x\n", __func__, ccb->cna2.tag_id, ccb->cna2.seq_id);
		xpt_done(ccb);
		break;
	}
	case XPT_CONT_TARGET_IO:
		isp_target_start_ctio(isp, ccb, FROM_CAM);
		break;
#endif
	case XPT_RESET_DEV:		/* BDR the specified SCSI device */
	{
		struct isp_fc *fc;

		bus = cam_sim_bus(xpt_path_sim(ccb->ccb_h.path));
		tgt = ccb->ccb_h.target_id;
		tgt |= (bus << 16);
		if (IS_FC(isp))
			fc = ISP_FC_PC(isp, bus);
		else
			fc = NULL;

		error = isp_control(isp, ISPCTL_RESET_DEV, bus, tgt);
		if (error) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		} else {
			/*
			 * If we have a FC device, reset the Command
			 * Reference Number, because the target will expect
			 * that we re-start the CRN at 1 after a reset.
			 */
			if (fc != NULL)
				isp_fcp_reset_crn(fc, tgt, /*tgt_set*/ 1);

			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		break;
	}
	case XPT_ABORT:			/* Abort the specified CCB */
	{
		union ccb *accb = ccb->cab.abort_ccb;
		switch (accb->ccb_h.func_code) {
#ifdef	ISP_TARGET_MODE
		case XPT_ACCEPT_TARGET_IO:
			isp_target_mark_aborted(isp, ccb);
			break;
#endif
		case XPT_SCSI_IO:
			error = isp_control(isp, ISPCTL_ABORT_CMD, accb);
			if (error) {
				ccb->ccb_h.status = CAM_UA_ABORT;
			} else {
				ccb->ccb_h.status = CAM_REQ_CMP;
			}
			break;
		default:
			ccb->ccb_h.status = CAM_REQ_INVALID;
			break;
		}
		/*
		 * This is not a queued CCB, so the caller expects it to be
		 * complete when control is returned.
		 */
		break;
	}
#define	IS_CURRENT_SETTINGS(c)	(c->type == CTS_TYPE_CURRENT_SETTINGS)
	case XPT_SET_TRAN_SETTINGS:	/* Nexus Settings */
		cts = &ccb->cts;
		if (!IS_CURRENT_SETTINGS(cts)) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
		tgt = cts->ccb_h.target_id;
		bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));
		if (IS_SCSI(isp)) {
			struct ccb_trans_settings_scsi *scsi = &cts->proto_specific.scsi;
			struct ccb_trans_settings_spi *spi = &cts->xport_specific.spi;
			sdparam *sdp = SDPARAM(isp, bus);
			uint16_t *dptr;

			if (spi->valid == 0 && scsi->valid == 0) {
				ccb->ccb_h.status = CAM_REQ_CMP;
				xpt_done(ccb);
				break;
			}

			/*
			 * We always update (internally) from goal_flags
			 * so any request to change settings just gets
			 * vectored to that location.
			 */
			dptr = &sdp->isp_devparam[tgt].goal_flags;

			if ((spi->valid & CTS_SPI_VALID_DISC) != 0) {
				if ((spi->flags & CTS_SPI_FLAGS_DISC_ENB) != 0)
					*dptr |= DPARM_DISC;
				else
					*dptr &= ~DPARM_DISC;
			}

			if ((scsi->valid & CTS_SCSI_VALID_TQ) != 0) {
				if ((scsi->flags & CTS_SCSI_FLAGS_TAG_ENB) != 0)
					*dptr |= DPARM_TQING;
				else
					*dptr &= ~DPARM_TQING;
			}

			if ((spi->valid & CTS_SPI_VALID_BUS_WIDTH) != 0) {
				if (spi->bus_width == MSG_EXT_WDTR_BUS_16_BIT)
					*dptr |= DPARM_WIDE;
				else
					*dptr &= ~DPARM_WIDE;
			}

			/*
			 * XXX: FIX ME
			 */
			if ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) && (spi->valid & CTS_SPI_VALID_SYNC_RATE) && (spi->sync_period && spi->sync_offset)) {
				*dptr |= DPARM_SYNC;
				/*
				 * XXX: CHECK FOR LEGALITY
				 */
				sdp->isp_devparam[tgt].goal_period = spi->sync_period;
				sdp->isp_devparam[tgt].goal_offset = spi->sync_offset;
			} else {
				*dptr &= ~DPARM_SYNC;
			}
			isp_prt(isp, ISP_LOGDEBUG0, "SET (%d.%d.%d) to flags %x off %x per %x", bus, tgt, cts->ccb_h.target_lun, sdp->isp_devparam[tgt].goal_flags,
			    sdp->isp_devparam[tgt].goal_offset, sdp->isp_devparam[tgt].goal_period);
			sdp->isp_devparam[tgt].dev_update = 1;
			sdp->update = 1;
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	case XPT_GET_TRAN_SETTINGS:
		cts = &ccb->cts;
		tgt = cts->ccb_h.target_id;
		bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));
		if (IS_FC(isp)) {
			fcparam *fcp = FCPARAM(isp, bus);
			struct ccb_trans_settings_scsi *scsi = &cts->proto_specific.scsi;
			struct ccb_trans_settings_fc *fc = &cts->xport_specific.fc;
			unsigned int hdlidx;

			cts->protocol = PROTO_SCSI;
			cts->protocol_version = SCSI_REV_2;
			cts->transport = XPORT_FC;
			cts->transport_version = 0;

			scsi->valid = CTS_SCSI_VALID_TQ;
			scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;
			fc->valid = CTS_FC_VALID_SPEED;
			fc->bitrate = 100000;
			fc->bitrate *= fcp->isp_gbspeed;
			hdlidx = fcp->isp_dev_map[tgt] - 1;
			if (hdlidx < MAX_FC_TARG) {
				fcportdb_t *lp = &fcp->portdb[hdlidx];
				fc->wwnn = lp->node_wwn;
				fc->wwpn = lp->port_wwn;
				fc->port = lp->portid;
				fc->valid |= CTS_FC_VALID_WWNN | CTS_FC_VALID_WWPN | CTS_FC_VALID_PORT;
			}
		} else {
			struct ccb_trans_settings_scsi *scsi = &cts->proto_specific.scsi;
			struct ccb_trans_settings_spi *spi = &cts->xport_specific.spi;
			sdparam *sdp = SDPARAM(isp, bus);
			uint16_t dval, pval, oval;

			if (IS_CURRENT_SETTINGS(cts)) {
				sdp->isp_devparam[tgt].dev_refresh = 1;
				sdp->update = 1;
				(void) isp_control(isp, ISPCTL_UPDATE_PARAMS, bus);
				dval = sdp->isp_devparam[tgt].actv_flags;
				oval = sdp->isp_devparam[tgt].actv_offset;
				pval = sdp->isp_devparam[tgt].actv_period;
			} else {
				dval = sdp->isp_devparam[tgt].nvrm_flags;
				oval = sdp->isp_devparam[tgt].nvrm_offset;
				pval = sdp->isp_devparam[tgt].nvrm_period;
			}

			cts->protocol = PROTO_SCSI;
			cts->protocol_version = SCSI_REV_2;
			cts->transport = XPORT_SPI;
			cts->transport_version = 2;

			spi->valid = 0;
			scsi->valid = 0;
			spi->flags = 0;
			scsi->flags = 0;
			if (dval & DPARM_DISC) {
				spi->flags |= CTS_SPI_FLAGS_DISC_ENB;
			}
			if ((dval & DPARM_SYNC) && oval && pval) {
				spi->sync_offset = oval;
				spi->sync_period = pval;
			} else {
				spi->sync_offset = 0;
				spi->sync_period = 0;
			}
			spi->valid |= CTS_SPI_VALID_SYNC_OFFSET;
			spi->valid |= CTS_SPI_VALID_SYNC_RATE;
			spi->valid |= CTS_SPI_VALID_BUS_WIDTH;
			if (dval & DPARM_WIDE) {
				spi->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			} else {
				spi->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
			}
			if (cts->ccb_h.target_lun != CAM_LUN_WILDCARD) {
				scsi->valid = CTS_SCSI_VALID_TQ;
				if (dval & DPARM_TQING) {
					scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;
				}
				spi->valid |= CTS_SPI_VALID_DISC;
			}
			isp_prt(isp, ISP_LOGDEBUG0, "GET %s (%d.%d.%d) to flags %x off %x per %x", IS_CURRENT_SETTINGS(cts)? "ACTIVE" : "NVRAM",
			    bus, tgt, cts->ccb_h.target_lun, dval, oval, pval);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_CALC_GEOMETRY:
		cam_calc_geometry(&ccb->ccg, 1);
		xpt_done(ccb);
		break;

	case XPT_RESET_BUS:		/* Reset the specified bus */
		bus = cam_sim_bus(sim);
		error = isp_control(isp, ISPCTL_RESET_BUS, bus);
		if (error) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
			xpt_done(ccb);
			break;
		}
		if (bootverbose) {
			xpt_print(ccb->ccb_h.path, "reset bus on channel %d\n", bus);
		}
		if (IS_FC(isp)) {
			xpt_async(AC_BUS_RESET, ISP_FC_PC(isp, bus)->path, 0);
		} else {
			xpt_async(AC_BUS_RESET, ISP_SPI_PC(isp, bus)->path, 0);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_TERM_IO:		/* Terminate the I/O process */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

	case XPT_SET_SIM_KNOB:		/* Set SIM knobs */
	{
		struct ccb_sim_knob *kp = &ccb->knob;
		fcparam *fcp;

		if (!IS_FC(isp)) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}

		bus = cam_sim_bus(xpt_path_sim(kp->ccb_h.path));
		fcp = FCPARAM(isp, bus);

		if (kp->xport_specific.fc.valid & KNOB_VALID_ADDRESS) {
			fcp->isp_wwnn = ISP_FC_PC(isp, bus)->def_wwnn = kp->xport_specific.fc.wwnn;
			fcp->isp_wwpn = ISP_FC_PC(isp, bus)->def_wwpn = kp->xport_specific.fc.wwpn;
			isp_prt(isp, ISP_LOGALL, "Setting Channel %d wwns to 0x%jx 0x%jx", bus, fcp->isp_wwnn, fcp->isp_wwpn);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		if (kp->xport_specific.fc.valid & KNOB_VALID_ROLE) {
			int rchange = 0;
			int newrole = 0;

			switch (kp->xport_specific.fc.role) {
			case KNOB_ROLE_NONE:
				if (fcp->role != ISP_ROLE_NONE) {
					rchange = 1;
					newrole = ISP_ROLE_NONE;
				}
				break;
			case KNOB_ROLE_TARGET:
				if (fcp->role != ISP_ROLE_TARGET) {
					rchange = 1;
					newrole = ISP_ROLE_TARGET;
				}
				break;
			case KNOB_ROLE_INITIATOR:
				if (fcp->role != ISP_ROLE_INITIATOR) {
					rchange = 1;
					newrole = ISP_ROLE_INITIATOR;
				}
				break;
			case KNOB_ROLE_BOTH:
#if 0
				if (fcp->role != ISP_ROLE_BOTH) {
					rchange = 1;
					newrole = ISP_ROLE_BOTH;
				}
#else
				/*
				 * We don't really support dual role at present on FC cards.
				 *
				 * We should, but a bunch of things are currently broken,
				 * so don't allow it.
				 */
				isp_prt(isp, ISP_LOGERR, "cannot support dual role at present");
				ccb->ccb_h.status = CAM_REQ_INVALID;
#endif
				break;
			}
			if (rchange) {
				ISP_PATH_PRT(isp, ISP_LOGCONFIG, ccb->ccb_h.path, "changing role on from %d to %d\n", fcp->role, newrole);
#ifdef	ISP_TARGET_MODE
				ISP_SET_PC(isp, bus, tm_enabled, 0);
				ISP_SET_PC(isp, bus, tm_luns_enabled, 0);
#endif
				if (isp_fc_change_role(isp, bus, newrole) != 0) {
					ccb->ccb_h.status = CAM_REQ_CMP_ERR;
					xpt_done(ccb);
					break;
				}
#ifdef	ISP_TARGET_MODE
				if (newrole == ISP_ROLE_TARGET || newrole == ISP_ROLE_BOTH) {
					/*
					 * Give the new role a chance to complain and settle
					 */
					msleep(isp, &isp->isp_lock, PRIBIO, "taking a breather", 2);
					ccb->ccb_h.status = isp_enable_deferred_luns(isp, bus);
				}
#endif
			}
		}
		xpt_done(ccb);
		break;
	}
	case XPT_GET_SIM_KNOB:		/* Get SIM knobs */
	{
		struct ccb_sim_knob *kp = &ccb->knob;

		if (IS_FC(isp)) {
			fcparam *fcp;

			bus = cam_sim_bus(xpt_path_sim(kp->ccb_h.path));
			fcp = FCPARAM(isp, bus);

			kp->xport_specific.fc.wwnn = fcp->isp_wwnn;
			kp->xport_specific.fc.wwpn = fcp->isp_wwpn;
			switch (fcp->role) {
			case ISP_ROLE_NONE:
				kp->xport_specific.fc.role = KNOB_ROLE_NONE;
				break;
			case ISP_ROLE_TARGET:
				kp->xport_specific.fc.role = KNOB_ROLE_TARGET;
				break;
			case ISP_ROLE_INITIATOR:
				kp->xport_specific.fc.role = KNOB_ROLE_INITIATOR;
				break;
			case ISP_ROLE_BOTH:
				kp->xport_specific.fc.role = KNOB_ROLE_BOTH;
				break;
			}
			kp->xport_specific.fc.valid = KNOB_VALID_ADDRESS | KNOB_VALID_ROLE;
			ccb->ccb_h.status = CAM_REQ_CMP;
		} else {
			ccb->ccb_h.status = CAM_REQ_INVALID;
		}
		xpt_done(ccb);
		break;
	}
	case XPT_PATH_INQ:		/* Path routing inquiry */
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
#ifdef	ISP_TARGET_MODE
		cpi->target_sprt = PIT_PROCESSOR | PIT_DISCONNECT | PIT_TERM_IO;
#else
		cpi->target_sprt = 0;
#endif
		cpi->hba_eng_cnt = 0;
		cpi->max_target = ISP_MAX_TARGETS(isp) - 1;
		cpi->max_lun = ISP_MAX_LUNS(isp) - 1;
		cpi->bus_id = cam_sim_bus(sim);
		if (isp->isp_osinfo.sixtyfourbit)
			cpi->maxio = (ISP_NSEG64_MAX - 1) * PAGE_SIZE;
		else
			cpi->maxio = (ISP_NSEG_MAX - 1) * PAGE_SIZE;

		bus = cam_sim_bus(xpt_path_sim(cpi->ccb_h.path));
		if (IS_FC(isp)) {
			fcparam *fcp = FCPARAM(isp, bus);

			cpi->hba_misc = PIM_NOBUSRESET | PIM_UNMAPPED;

			/*
			 * Because our loop ID can shift from time to time,
			 * make our initiator ID out of range of our bus.
			 */
			cpi->initiator_id = cpi->max_target + 1;

			/*
			 * Set base transfer capabilities for Fibre Channel, for this HBA.
			 */
			if (IS_25XX(isp)) {
				cpi->base_transfer_speed = 8000000;
			} else if (IS_24XX(isp)) {
				cpi->base_transfer_speed = 4000000;
			} else if (IS_23XX(isp)) {
				cpi->base_transfer_speed = 2000000;
			} else {
				cpi->base_transfer_speed = 1000000;
			}
			cpi->hba_inquiry = PI_TAG_ABLE;
			cpi->transport = XPORT_FC;
			cpi->transport_version = 0;
			cpi->xport_specific.fc.wwnn = fcp->isp_wwnn;
			cpi->xport_specific.fc.wwpn = fcp->isp_wwpn;
			cpi->xport_specific.fc.port = fcp->isp_portid;
			cpi->xport_specific.fc.bitrate = fcp->isp_gbspeed * 1000;
		} else {
			sdparam *sdp = SDPARAM(isp, bus);
			cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
			cpi->hba_misc = PIM_UNMAPPED;
			cpi->initiator_id = sdp->isp_initiator_id;
			cpi->base_transfer_speed = 3300;
			cpi->transport = XPORT_SPI;
			cpi->transport_version = 2;
		}
		cpi->protocol = PROTO_SCSI;
		cpi->protocol_version = SCSI_REV_2;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "Qlogic", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
	default:
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;
	}
}

#define	ISPDDB	(CAM_DEBUG_INFO|CAM_DEBUG_TRACE|CAM_DEBUG_CDB)

void
isp_done(XS_T *sccb)
{
	ispsoftc_t *isp = XS_ISP(sccb);
	uint32_t status;

	if (XS_NOERR(sccb))
		XS_SETERR(sccb, CAM_REQ_CMP);

	if ((sccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP && (sccb->scsi_status != SCSI_STATUS_OK)) {
		sccb->ccb_h.status &= ~CAM_STATUS_MASK;
		if ((sccb->scsi_status == SCSI_STATUS_CHECK_COND) && (sccb->ccb_h.status & CAM_AUTOSNS_VALID) == 0) {
			sccb->ccb_h.status |= CAM_AUTOSENSE_FAIL;
		} else {
			sccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
		}
	}

	sccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	status = sccb->ccb_h.status & CAM_STATUS_MASK;
	if (status != CAM_REQ_CMP) {
		if (status != CAM_SEL_TIMEOUT)
			isp_prt(isp, ISP_LOGDEBUG0, "target %d lun %d CAM status 0x%x SCSI status 0x%x", XS_TGT(sccb), XS_LUN(sccb), sccb->ccb_h.status, sccb->scsi_status);
		else if ((IS_FC(isp))
		      && (XS_TGT(sccb) < MAX_FC_TARG)) {
			fcparam *fcp;
			int hdlidx;

			fcp = FCPARAM(isp, XS_CHANNEL(sccb));
			hdlidx = fcp->isp_dev_map[XS_TGT(sccb)] - 1;
			/*
			 * Note that we have reported that this device is
			 * gone.  If it reappears, we'll need to issue a
			 * rescan.
			 */
			if (hdlidx > 0 && hdlidx < MAX_FC_TARG)
				fcp->portdb[hdlidx].reported_gone = 1;
		}
		if ((sccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
			sccb->ccb_h.status |= CAM_DEV_QFRZN;
			xpt_freeze_devq(sccb->ccb_h.path, 1);
		}
	}

	if ((CAM_DEBUGGED(sccb->ccb_h.path, ISPDDB)) && (sccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		xpt_print(sccb->ccb_h.path, "cam completion status 0x%x\n", sccb->ccb_h.status);
	}

	if (callout_active(&PISP_PCMD(sccb)->wdog))
		callout_stop(&PISP_PCMD(sccb)->wdog);
	isp_free_pcmd(isp, (union ccb *) sccb);
	xpt_done((union ccb *) sccb);
}

void
isp_async(ispsoftc_t *isp, ispasync_t cmd, ...)
{
	int bus;
	static const char prom0[] = "Chan %d PortID 0x%06x handle 0x%x %s %s WWPN 0x%08x%08x";
	static const char prom2[] = "Chan %d PortID 0x%06x handle 0x%x %s %s tgt %u WWPN 0x%08x%08x";
	char buf[64];
	char *msg = NULL;
	target_id_t tgt;
	fcportdb_t *lp;
	struct isp_fc *fc;
	struct cam_path *tmppath;
	va_list ap;

	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
	{
		struct ccb_trans_settings_scsi *scsi;
		struct ccb_trans_settings_spi *spi;
		int flags, tgt;
		sdparam *sdp;
		struct ccb_trans_settings cts;

		memset(&cts, 0, sizeof (struct ccb_trans_settings));

		va_start(ap, cmd);
		bus = va_arg(ap, int);
		tgt = va_arg(ap, int);
		va_end(ap);
		sdp = SDPARAM(isp, bus);

		if (xpt_create_path(&tmppath, NULL, cam_sim_path(ISP_SPI_PC(isp, bus)->sim), tgt, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			isp_prt(isp, ISP_LOGWARN, "isp_async cannot make temp path for %d.%d", tgt, bus);
			break;
		}
		flags = sdp->isp_devparam[tgt].actv_flags;
		cts.type = CTS_TYPE_CURRENT_SETTINGS;
		cts.protocol = PROTO_SCSI;
		cts.transport = XPORT_SPI;

		scsi = &cts.proto_specific.scsi;
		spi = &cts.xport_specific.spi;

		if (flags & DPARM_TQING) {
			scsi->valid |= CTS_SCSI_VALID_TQ;
			scsi->flags |= CTS_SCSI_FLAGS_TAG_ENB;
		}

		if (flags & DPARM_DISC) {
			spi->valid |= CTS_SPI_VALID_DISC;
			spi->flags |= CTS_SPI_FLAGS_DISC_ENB;
		}
		spi->flags |= CTS_SPI_VALID_BUS_WIDTH;
		if (flags & DPARM_WIDE) {
			spi->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
		} else {
			spi->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
		}
		if (flags & DPARM_SYNC) {
			spi->valid |= CTS_SPI_VALID_SYNC_RATE;
			spi->valid |= CTS_SPI_VALID_SYNC_OFFSET;
			spi->sync_period = sdp->isp_devparam[tgt].actv_period;
			spi->sync_offset = sdp->isp_devparam[tgt].actv_offset;
		}
		isp_prt(isp, ISP_LOGDEBUG2, "NEW_TGT_PARAMS bus %d tgt %d period %x offset %x flags %x", bus, tgt, sdp->isp_devparam[tgt].actv_period, sdp->isp_devparam[tgt].actv_offset, flags);
		xpt_setup_ccb(&cts.ccb_h, tmppath, 1);
		xpt_async(AC_TRANSFER_NEG, tmppath, &cts);
		xpt_free_path(tmppath);
		break;
	}
	case ISPASYNC_BUS_RESET:
	{
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		va_end(ap);
		isp_prt(isp, ISP_LOGINFO, "SCSI bus reset on bus %d detected", bus);
		if (IS_FC(isp)) {
			xpt_async(AC_BUS_RESET, ISP_FC_PC(isp, bus)->path, NULL);
		} else {
			xpt_async(AC_BUS_RESET, ISP_SPI_PC(isp, bus)->path, NULL);
		}
		break;
	}
	case ISPASYNC_LIP:
		if (msg == NULL) {
			msg = "LIP Received";
		}
		/* FALLTHROUGH */
	case ISPASYNC_LOOP_RESET:
		if (msg == NULL) {
			msg = "LOOP Reset";
		}
		/* FALLTHROUGH */
	case ISPASYNC_LOOP_DOWN:
	{
		if (msg == NULL) {
			msg = "LOOP Down";
		}
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		va_end(ap);

		FCPARAM(isp, bus)->link_active = 0;

		fc = ISP_FC_PC(isp, bus);
		if (cmd == ISPASYNC_LOOP_DOWN && fc->ready) {
			/*
			 * We don't do any simq freezing if we are only in target mode
			 */
			if (FCPARAM(isp, bus)->role & ISP_ROLE_INITIATOR) {
				if (fc->path) {
					isp_freeze_loopdown(isp, bus, msg);
				}
				if (!callout_active(&fc->ldt)) {
					callout_reset(&fc->ldt, fc->loop_down_limit * hz, isp_ldt, fc);
					isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Starting Loop Down Timer @ %lu", (unsigned long) time_uptime);
				}
			}
		}
		isp_fcp_reset_crn(fc, /*tgt*/0, /*tgt_set*/ 0);

		isp_prt(isp, ISP_LOGINFO, "Chan %d: %s", bus, msg);
		break;
	}
	case ISPASYNC_LOOP_UP:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);
		/*
		 * Now we just note that Loop has come up. We don't
		 * actually do anything because we're waiting for a
		 * Change Notify before activating the FC cleanup
		 * thread to look at the state of the loop again.
		 */
		FCPARAM(isp, bus)->link_active = 1;
		fc->loop_dead = 0;
		fc->loop_down_time = 0;
		isp_prt(isp, ISP_LOGINFO, "Chan %d Loop UP", bus);
		break;
	case ISPASYNC_DEV_ARRIVED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);
		lp->announced = 0;
		lp->gone_timer = 0;
		if ((FCPARAM(isp, bus)->role & ISP_ROLE_INITIATOR) && (lp->prli_word3 & PRLI_WD3_TARGET_FUNCTION)) {
			int dbidx = lp - FCPARAM(isp, bus)->portdb;
			int i;

			for (i = 0; i < MAX_FC_TARG; i++) {
				if (i >= FL_ID && i <= SNS_ID) {
					continue;
				}
				if (FCPARAM(isp, bus)->isp_dev_map[i] == 0) {
					break;
				}
			}
			if (i < MAX_FC_TARG) {
				FCPARAM(isp, bus)->isp_dev_map[i] = dbidx + 1;
				lp->dev_map_idx = i + 1;
			} else {
				isp_prt(isp, ISP_LOGWARN, "out of target ids");
				isp_dump_portdb(isp, bus);
			}
		}
		isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
		if (lp->dev_map_idx) {
			tgt = lp->dev_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2, bus, lp->portid, lp->handle, buf, "arrived at", tgt, (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
			isp_fcp_reset_crn(fc, tgt, /*tgt_set*/ 1);
			isp_make_here(isp, lp, bus, tgt);
		} else {
			isp_prt(isp, ISP_LOGCONFIG, prom0, bus, lp->portid, lp->handle, buf, "arrived", (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_DEV_CHANGED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);
		lp->announced = 0;
		lp->gone_timer = 0;
		if (isp_change_is_bad) {
			lp->state = FC_PORTDB_STATE_NIL;
			if (lp->dev_map_idx) {
				tgt = lp->dev_map_idx - 1;
				FCPARAM(isp, bus)->isp_dev_map[tgt] = 0;
				lp->dev_map_idx = 0;
				isp_prt(isp, ISP_LOGCONFIG, prom3, bus, lp->portid, tgt, "change is bad");
				isp_make_gone(isp, lp, bus, tgt);
			} else {
				isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
				isp_prt(isp, ISP_LOGCONFIG, prom0, bus, lp->portid, lp->handle, buf, "changed and departed",
				    (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
			}
		} else {
			lp->portid = lp->new_portid;
			lp->prli_word3 = lp->new_prli_word3;
			isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
			if (lp->dev_map_idx) {
				int t = lp->dev_map_idx - 1;
				FCPARAM(isp, bus)->isp_dev_map[t] = (lp - FCPARAM(isp, bus)->portdb) + 1;
				tgt = lp->dev_map_idx - 1;
				isp_prt(isp, ISP_LOGCONFIG, prom2, bus, lp->portid, lp->handle, buf, "changed at", tgt,
				    (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
				isp_fcp_reset_crn(fc, tgt, /*tgt_set*/ 1);
			} else {
				isp_prt(isp, ISP_LOGCONFIG, prom0, bus, lp->portid, lp->handle, buf, "changed", (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
			}
		}
		break;
	case ISPASYNC_DEV_STAYED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
		if (lp->dev_map_idx) {
			fc = ISP_FC_PC(isp, bus);
			tgt = lp->dev_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2, bus, lp->portid, lp->handle, buf, "stayed at", tgt,
		    	    (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
			/*
			 * Only issue a rescan if we've actually reported
			 * that this device is gone.
			 */
			if (lp->reported_gone != 0) {
				isp_prt(isp, ISP_LOGCONFIG, prom2, bus, lp->portid, lp->handle, buf, "rescanned at", tgt, 
				    (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
				isp_make_here(isp, lp, bus, tgt);
			}
		} else {
			isp_prt(isp, ISP_LOGCONFIG, prom0, bus, lp->portid, lp->handle, buf, "stayed",
		    	    (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_DEV_GONE:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);
		/*
		 * If this has a virtual target and we haven't marked it
		 * that we're going to have isp_gdt tell the OS it's gone,
		 * set the isp_gdt timer running on it.
		 *
		 * If it isn't marked that isp_gdt is going to get rid of it,
		 * announce that it's gone.
		 *
		 */
		isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
		if (lp->dev_map_idx && lp->announced == 0) {
			lp->announced = 1;
			lp->state = FC_PORTDB_STATE_ZOMBIE;
			lp->gone_timer = ISP_FC_PC(isp, bus)->gone_device_time;
			if (fc->ready && !callout_active(&fc->gdt)) {
				isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Chan %d Starting Gone Device Timer with %u seconds time now %lu", bus, lp->gone_timer, (unsigned long)time_uptime);
				callout_reset(&fc->gdt, hz, isp_gdt, fc);
			}
			tgt = lp->dev_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2, bus, lp->portid, lp->handle, buf, "gone zombie at", tgt, (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
			isp_fcp_reset_crn(fc, tgt, /*tgt_set*/ 1);
		} else if (lp->announced == 0) {
			isp_prt(isp, ISP_LOGCONFIG, prom0, bus, lp->portid, lp->handle, buf, "departed", (uint32_t) (lp->port_wwn >> 32), (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_CHANGE_NOTIFY:
	{
		char *msg;
		int evt, nphdl, nlstate, reason;

		va_start(ap, cmd);
		bus = va_arg(ap, int);
		evt = va_arg(ap, int);
		if (IS_24XX(isp) && evt == ISPASYNC_CHANGE_PDB) {
			nphdl = va_arg(ap, int);
			nlstate = va_arg(ap, int);
			reason = va_arg(ap, int);
		} else {
			nphdl = NIL_HANDLE;
			nlstate = reason = 0;
		}
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);

		if (evt == ISPASYNC_CHANGE_PDB) {
			msg = "Chan %d Port Database Changed";
		} else if (evt == ISPASYNC_CHANGE_SNS) {
			msg = "Chan %d Name Server Database Changed";
		} else {
			msg = "Chan %d Other Change Notify";
		}

		/*
		 * If the loop down timer is running, cancel it.
		 */
		if (fc->ready && callout_active(&fc->ldt)) {
			isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Stopping Loop Down Timer @ %lu", (unsigned long) time_uptime);
			callout_stop(&fc->ldt);
		}
		isp_prt(isp, ISP_LOGINFO, msg, bus);
		if (FCPARAM(isp, bus)->role & ISP_ROLE_INITIATOR) {
			isp_freeze_loopdown(isp, bus, msg);
		}
		wakeup(fc);
		break;
	}
#ifdef	ISP_TARGET_MODE
	case ISPASYNC_TARGET_NOTIFY:
	{
		isp_notify_t *notify;
		va_start(ap, cmd);
		notify = va_arg(ap, isp_notify_t *);
		va_end(ap);
		switch (notify->nt_ncode) {
		case NT_ABORT_TASK:
		case NT_ABORT_TASK_SET:
		case NT_CLEAR_ACA:
		case NT_CLEAR_TASK_SET:
		case NT_LUN_RESET:
		case NT_TARGET_RESET:
			/*
			 * These are task management functions.
			 */
			isp_handle_platform_target_tmf(isp, notify);
			break;
		case NT_BUS_RESET:
		case NT_LIP_RESET:
		case NT_LINK_UP:
		case NT_LINK_DOWN:
			/*
			 * No action need be taken here.
			 */
			break;
		case NT_HBA_RESET:
			isp_del_all_wwn_entries(isp, ISP_NOCHAN);
			break;
		case NT_GLOBAL_LOGOUT:
		case NT_LOGOUT:
			/*
			 * This is device arrival/departure notification
			 */
			isp_handle_platform_target_notify_ack(isp, notify);
			break;
		case NT_ARRIVED:
		{
			struct ac_contract ac;
			struct ac_device_changed *fc;

			ac.contract_number = AC_CONTRACT_DEV_CHG;
			fc = (struct ac_device_changed *) ac.contract_data;
			fc->wwpn = notify->nt_wwn;
			fc->port = notify->nt_sid;
			fc->target = notify->nt_nphdl;
			fc->arrived = 1;
			xpt_async(AC_CONTRACT, ISP_FC_PC(isp, notify->nt_channel)->path, &ac);
			break;
		}
		case NT_DEPARTED:
		{
			struct ac_contract ac;
			struct ac_device_changed *fc;

			ac.contract_number = AC_CONTRACT_DEV_CHG;
			fc = (struct ac_device_changed *) ac.contract_data;
			fc->wwpn = notify->nt_wwn;
			fc->port = notify->nt_sid;
			fc->target = notify->nt_nphdl;
			fc->arrived = 0;
			xpt_async(AC_CONTRACT, ISP_FC_PC(isp, notify->nt_channel)->path, &ac);
			break;
		}
		default:
			isp_prt(isp, ISP_LOGALL, "target notify code 0x%x", notify->nt_ncode);
			isp_handle_platform_target_notify_ack(isp, notify);
			break;
		}
		break;
	}
	case ISPASYNC_TARGET_NOTIFY_ACK:
	{
		void *inot;
		va_start(ap, cmd);
		inot = va_arg(ap, void *);
		va_end(ap);
		if (isp_notify_ack(isp, inot)) {
			isp_tna_t *tp = malloc(sizeof (*tp), M_DEVBUF, M_NOWAIT);
			if (tp) {
				tp->isp = isp;
				if (inot) {
					memcpy(tp->data, inot, sizeof (tp->data));
					tp->not = tp->data;
				} else {
					tp->not = NULL;
				}
				callout_init_mtx(&tp->timer, &isp->isp_lock, 0);
				callout_reset(&tp->timer, 5,
				    isp_refire_notify_ack, tp);
			} else {
				isp_prt(isp, ISP_LOGERR, "you lose- cannot allocate a notify refire");
			}
		}
		break;
	}
	case ISPASYNC_TARGET_ACTION:
	{
		isphdr_t *hp;

		va_start(ap, cmd);
		hp = va_arg(ap, isphdr_t *);
		va_end(ap);
		switch (hp->rqs_entry_type) {
		default:
			isp_prt(isp, ISP_LOGWARN, "%s: unhandled target action 0x%x", __func__, hp->rqs_entry_type);
			break;
		case RQSTYPE_NOTIFY:
			if (IS_SCSI(isp)) {
				isp_handle_platform_notify_scsi(isp, (in_entry_t *) hp);
			} else if (IS_24XX(isp)) {
				isp_handle_platform_notify_24xx(isp, (in_fcentry_24xx_t *) hp);
			} else {
				isp_handle_platform_notify_fc(isp, (in_fcentry_t *) hp);
			}
			break;
		case RQSTYPE_ATIO:
			if (IS_24XX(isp)) {
				isp_handle_platform_atio7(isp, (at7_entry_t *) hp);
			} else {
				isp_handle_platform_atio(isp, (at_entry_t *) hp);
			}
			break;
		case RQSTYPE_ATIO2:
			isp_handle_platform_atio2(isp, (at2_entry_t *) hp);
			break;
		case RQSTYPE_CTIO7:
		case RQSTYPE_CTIO3:
		case RQSTYPE_CTIO2:
		case RQSTYPE_CTIO:
			isp_handle_platform_ctio(isp, hp);
			break;
		case RQSTYPE_ABTS_RCVD:
		{
			abts_t *abts = (abts_t *)hp;
			isp_notify_t notify, *nt = &notify;
			tstate_t *tptr;
			fcportdb_t *lp;
			uint16_t chan;
			uint32_t sid, did;

			did = (abts->abts_did_hi << 16) | abts->abts_did_lo;
			sid = (abts->abts_sid_hi << 16) | abts->abts_sid_lo;
			ISP_MEMZERO(nt, sizeof (isp_notify_t));

			nt->nt_hba = isp;
			nt->nt_did = did;
			nt->nt_nphdl = abts->abts_nphdl;
			nt->nt_sid = sid;
			isp_find_chan_by_did(isp, did, &chan);
			if (chan == ISP_NOCHAN) {
				nt->nt_tgt = TGT_ANY;
			} else {
				nt->nt_tgt = FCPARAM(isp, chan)->isp_wwpn;
				if (isp_find_pdb_by_loopid(isp, chan, abts->abts_nphdl, &lp)) {
					nt->nt_wwn = lp->port_wwn;
				} else {
					nt->nt_wwn = INI_ANY;
				}
			}
			/*
			 * Try hard to find the lun for this command.
			 */
			tptr = get_lun_statep_from_tag(isp, chan, abts->abts_rxid_task);
			if (tptr) {
				nt->nt_lun = tptr->ts_lun;
				rls_lun_statep(isp, tptr);
			} else {
				nt->nt_lun = LUN_ANY;
			}
			nt->nt_need_ack = 1;
			nt->nt_tagval = abts->abts_rxid_task;
			nt->nt_tagval |= (((uint64_t) abts->abts_rxid_abts) << 32);
			if (abts->abts_rxid_task == ISP24XX_NO_TASK) {
				isp_prt(isp, ISP_LOGTINFO, "[0x%x] ABTS from N-Port handle 0x%x Port 0x%06x has no task id (rx_id 0x%04x ox_id 0x%04x)",
				    abts->abts_rxid_abts, abts->abts_nphdl, sid, abts->abts_rx_id, abts->abts_ox_id);
			} else {
				isp_prt(isp, ISP_LOGTINFO, "[0x%x] ABTS from N-Port handle 0x%x Port 0x%06x for task 0x%x (rx_id 0x%04x ox_id 0x%04x)",
				    abts->abts_rxid_abts, abts->abts_nphdl, sid, abts->abts_rxid_task, abts->abts_rx_id, abts->abts_ox_id);
			}
			nt->nt_channel = chan;
			nt->nt_ncode = NT_ABORT_TASK;
			nt->nt_lreserved = hp;
			isp_handle_platform_target_tmf(isp, nt);
			break;
		}
		case RQSTYPE_ENABLE_LUN:
		case RQSTYPE_MODIFY_LUN:
			isp_ledone(isp, (lun_entry_t *) hp);
			break;
		}
		break;
	}
#endif
	case ISPASYNC_FW_CRASH:
	{
		uint16_t mbox1, mbox6;
		mbox1 = ISP_READ(isp, OUTMAILBOX1);
		if (IS_DUALBUS(isp)) { 
			mbox6 = ISP_READ(isp, OUTMAILBOX6);
		} else {
			mbox6 = 0;
		}
		isp_prt(isp, ISP_LOGERR, "Internal Firmware Error on bus %d @ RISC Address 0x%x", mbox6, mbox1);
		mbox1 = isp->isp_osinfo.mbox_sleep_ok;
		isp->isp_osinfo.mbox_sleep_ok = 0;
		isp_reinit(isp, 1);
		isp->isp_osinfo.mbox_sleep_ok = mbox1;
		isp_async(isp, ISPASYNC_FW_RESTARTED, NULL);
		break;
	}
	default:
		isp_prt(isp, ISP_LOGERR, "unknown isp_async event %d", cmd);
		break;
	}
}


/*
 * Locks are held before coming here.
 */
void
isp_uninit(ispsoftc_t *isp)
{
	if (IS_24XX(isp)) {
		ISP_WRITE(isp, BIU2400_HCCR, HCCR_2400_CMD_RESET);
	} else {
		ISP_WRITE(isp, HCCR, HCCR_CMD_RESET);
	}
	ISP_DISABLE_INTS(isp);
}

/*
 * When we want to get the 'default' WWNs (when lacking NVRAM), we pick them
 * up from our platform default (defww{p|n}n) and morph them based upon
 * channel.
 * 
 * When we want to get the 'active' WWNs, we get NVRAM WWNs and then morph them
 * based upon channel.
 */

uint64_t
isp_default_wwn(ispsoftc_t * isp, int chan, int isactive, int iswwnn)
{
	uint64_t seed;
	struct isp_fc *fc = ISP_FC_PC(isp, chan);

	/*
	 * If we're asking for a active WWN, the default overrides get
	 * returned, otherwise the NVRAM value is picked.
	 * 
	 * If we're asking for a default WWN, we just pick the default override.
	 */
	if (isactive) {
		seed = iswwnn ? fc->def_wwnn : fc->def_wwpn;
		if (seed) {
			return (seed);
		}
		seed = iswwnn ? FCPARAM(isp, chan)->isp_wwnn_nvram : FCPARAM(isp, chan)->isp_wwpn_nvram;
		if (seed) {
			return (seed);
		}
		return (0x400000007F000009ull);
	}

	seed = iswwnn ? fc->def_wwnn : fc->def_wwpn;

	/*
	 * For channel zero just return what we have. For either ACTIVE or
	 * DEFAULT cases, we depend on default override of NVRAM values for
	 * channel zero.
	 */
	if (chan == 0) {
		return (seed);
	}

	/*
	 * For other channels, we are doing one of three things:
	 * 
	 * 1. If what we have now is non-zero, return it. Otherwise we morph
	 * values from channel 0. 2. If we're here for a WWPN we synthesize
	 * it if Channel 0's wwpn has a type 2 NAA. 3. If we're here for a
	 * WWNN we synthesize it if Channel 0's wwnn has a type 2 NAA.
	 */

	if (seed) {
		return (seed);
	}
	seed = iswwnn ? ISP_FC_PC(isp, 0)->def_wwnn : ISP_FC_PC(isp, 0)->def_wwpn;
	if (seed == 0)
		seed = iswwnn ? FCPARAM(isp, 0)->isp_wwnn_nvram : FCPARAM(isp, 0)->isp_wwpn_nvram;

	if (((seed >> 60) & 0xf) == 2) {
		/*
		 * The type 2 NAA fields for QLogic cards appear be laid out
		 * thusly:
		 * 
		 * bits 63..60 NAA == 2 bits 59..57 unused/zero bit 56
		 * port (1) or node (0) WWN distinguishor bit 48
		 * physical port on dual-port chips (23XX/24XX)
		 * 
		 * This is somewhat nutty, particularly since bit 48 is
		 * irrelevant as they assign separate serial numbers to
		 * different physical ports anyway.
		 * 
		 * We'll stick our channel number plus one first into bits
		 * 57..59 and thence into bits 52..55 which allows for 8 bits
		 * of channel which is comfortably more than our maximum
		 * (126) now.
		 */
		seed &= ~0x0FF0000000000000ULL;
		if (iswwnn == 0) {
			seed |= ((uint64_t) (chan + 1) & 0xf) << 56;
			seed |= ((uint64_t) ((chan + 1) >> 4) & 0xf) << 52;
		}
	} else {
		seed = 0;
	}
	return (seed);
}

void
isp_prt(ispsoftc_t *isp, int level, const char *fmt, ...)
{
	int loc;
	char lbuf[200];
	va_list ap;

	if (level != ISP_LOGALL && (level & isp->isp_dblev) == 0) {
		return;
	}
	snprintf(lbuf, sizeof (lbuf), "%s: ", device_get_nameunit(isp->isp_dev));
	loc = strlen(lbuf);
	va_start(ap, fmt);
	vsnprintf(&lbuf[loc], sizeof (lbuf) - loc - 1, fmt, ap); 
	va_end(ap);
	printf("%s\n", lbuf);
}

void
isp_xs_prt(ispsoftc_t *isp, XS_T *xs, int level, const char *fmt, ...)
{
	va_list ap;
	if (level != ISP_LOGALL && (level & isp->isp_dblev) == 0) {
		return;
	}
	xpt_print_path(xs->ccb_h.path);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf("\n");
}

uint64_t
isp_nanotime_sub(struct timespec *b, struct timespec *a)
{
	uint64_t elapsed;
	struct timespec x = *b;
	timespecsub(&x, a);
	elapsed = GET_NANOSEC(&x);
	if (elapsed == 0)
		elapsed++;
	return (elapsed);
}

int
isp_mbox_acquire(ispsoftc_t *isp)
{
	if (isp->isp_osinfo.mboxbsy) {
		return (1);
	} else {
		isp->isp_osinfo.mboxcmd_done = 0;
		isp->isp_osinfo.mboxbsy = 1;
		return (0);
	}
}

void
isp_mbox_wait_complete(ispsoftc_t *isp, mbreg_t *mbp)
{
	unsigned int usecs = mbp->timeout;
	unsigned int max, olim, ilim;

	if (usecs == 0) {
		usecs = MBCMD_DEFAULT_TIMEOUT;
	}
	max = isp->isp_mbxwrk0 + 1;

	if (isp->isp_osinfo.mbox_sleep_ok) {
		unsigned int ms = (usecs + 999) / 1000;

		isp->isp_osinfo.mbox_sleep_ok = 0;
		isp->isp_osinfo.mbox_sleeping = 1;
		for (olim = 0; olim < max; olim++) {
			msleep(&isp->isp_mbxworkp, &isp->isp_osinfo.lock, PRIBIO, "ispmbx_sleep", isp_mstohz(ms));
			if (isp->isp_osinfo.mboxcmd_done) {
				break;
			}
		}
		isp->isp_osinfo.mbox_sleep_ok = 1;
		isp->isp_osinfo.mbox_sleeping = 0;
	} else {
		for (olim = 0; olim < max; olim++) {
			for (ilim = 0; ilim < usecs; ilim += 100) {
				uint32_t isr;
				uint16_t sema, mbox;
				if (isp->isp_osinfo.mboxcmd_done) {
					break;
				}
				if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
					isp_intr(isp, isr, sema, mbox);
					if (isp->isp_osinfo.mboxcmd_done) {
						break;
					}
				}
				ISP_DELAY(100);
			}
			if (isp->isp_osinfo.mboxcmd_done) {
				break;
			}
		}
	}
	if (isp->isp_osinfo.mboxcmd_done == 0) {
		isp_prt(isp, ISP_LOGWARN, "%s Mailbox Command (0x%x) Timeout (%uus) (started @ %s:%d)",
		    isp->isp_osinfo.mbox_sleep_ok? "Interrupting" : "Polled", isp->isp_lastmbxcmd, usecs, mbp->func, mbp->lineno);
		mbp->param[0] = MBOX_TIMEOUT;
		isp->isp_osinfo.mboxcmd_done = 1;
	}
}

void
isp_mbox_notify_done(ispsoftc_t *isp)
{
	if (isp->isp_osinfo.mbox_sleeping) {
		wakeup(&isp->isp_mbxworkp);
	}
	isp->isp_osinfo.mboxcmd_done = 1;
}

void
isp_mbox_release(ispsoftc_t *isp)
{
	isp->isp_osinfo.mboxbsy = 0;
}

int
isp_fc_scratch_acquire(ispsoftc_t *isp, int chan)
{
	int ret = 0;
	if (isp->isp_osinfo.pc.fc[chan].fcbsy) {
		ret = -1;
	} else {
		isp->isp_osinfo.pc.fc[chan].fcbsy = 1;
	}
	return (ret);
}

int
isp_mstohz(int ms)
{
	int hz;
	struct timeval t;
	t.tv_sec = ms / 1000;
	t.tv_usec = (ms % 1000) * 1000;
	hz = tvtohz(&t);
	if (hz < 0) {
		hz = 0x7fffffff;
	}
	if (hz == 0) {
		hz = 1;
	}
	return (hz);
}

void
isp_platform_intr(void *arg)
{
	ispsoftc_t *isp = arg;
	uint32_t isr;
	uint16_t sema, mbox;

	ISP_LOCK(isp);
	isp->isp_intcnt++;
	if (ISP_READ_ISR(isp, &isr, &sema, &mbox) == 0) {
		isp->isp_intbogus++;
	} else {
		isp_intr(isp, isr, sema, mbox);
	}
	ISP_UNLOCK(isp);
}

void
isp_common_dmateardown(ispsoftc_t *isp, struct ccb_scsiio *csio, uint32_t hdl)
{
	if ((csio->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
		bus_dmamap_sync(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap, BUS_DMASYNC_POSTREAD);
	} else {
		bus_dmamap_sync(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap, BUS_DMASYNC_POSTWRITE);
	}
	bus_dmamap_unload(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap);
}

/*
 * Reset the command reference number for all LUNs on a specific target
 * (needed when a target arrives again) or for all targets on a port
 * (needed for events like a LIP).
 */
void
isp_fcp_reset_crn(struct isp_fc *fc, uint32_t tgt, int tgt_set)
{
	int i;
	struct isp_nexus *nxp;

	if (tgt_set == 0)
		isp_prt(fc->isp, ISP_LOG_SANCFG, "resetting CRN on all targets");
	else
		isp_prt(fc->isp, ISP_LOG_SANCFG, "resetting CRN target %u", tgt);

	for (i = 0; i < NEXUS_HASH_WIDTH; i++) {
		nxp = fc->nexus_hash[i];
		while (nxp) {
			if ((tgt_set != 0) && (tgt == nxp->tgt))
				nxp->crnseed = 0;

			nxp = nxp->next;
		}
	}
}

int
isp_fcp_next_crn(ispsoftc_t *isp, uint8_t *crnp, XS_T *cmd)
{
	uint32_t chan, tgt, lun;
	struct isp_fc *fc;
	struct isp_nexus *nxp;
	int idx;

	if (isp->isp_type < ISP_HA_FC_2300)
		return (0);

	chan = XS_CHANNEL(cmd);
	tgt = XS_TGT(cmd);
	lun = XS_LUN(cmd);
	fc = &isp->isp_osinfo.pc.fc[chan];
	idx = NEXUS_HASH(tgt, lun);
	nxp = fc->nexus_hash[idx];

	while (nxp) {
		if (nxp->tgt == tgt && nxp->lun == lun)
			break;
		nxp = nxp->next;
	}
	if (nxp == NULL) {
		nxp = fc->nexus_free_list;
		if (nxp == NULL) {
			nxp = malloc(sizeof (struct isp_nexus), M_DEVBUF, M_ZERO|M_NOWAIT);
			if (nxp == NULL) {
				return (-1);
			}
		} else {
			fc->nexus_free_list = nxp->next;
		}
		nxp->tgt = tgt;
		nxp->lun = lun;
		nxp->next = fc->nexus_hash[idx];
		fc->nexus_hash[idx] = nxp;
	}
	if (nxp) {
		if (nxp->crnseed == 0)
			nxp->crnseed = 1;
		if (cmd)
			PISP_PCMD(cmd)->crn = nxp->crnseed;
		*crnp = nxp->crnseed++;
		return (0);
	}
	return (-1);
}

/*
 * We enter with the lock held
 */
void
isp_timer(void *arg)
{
	ispsoftc_t *isp = arg;
#ifdef	ISP_TARGET_MODE
	isp_tmcmd_restart(isp);
#endif
	callout_reset(&isp->isp_osinfo.tmo, isp_timer_count, isp_timer, isp);
}

isp_ecmd_t *
isp_get_ecmd(ispsoftc_t *isp)
{
	isp_ecmd_t *ecmd = isp->isp_osinfo.ecmd_free;
	if (ecmd) {
		isp->isp_osinfo.ecmd_free = ecmd->next;
	}
	return (ecmd);
}

void
isp_put_ecmd(ispsoftc_t *isp, isp_ecmd_t *ecmd)
{
	ecmd->next = isp->isp_osinfo.ecmd_free;
	isp->isp_osinfo.ecmd_free = ecmd;
}
