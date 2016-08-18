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
int isp_loop_down_limit = 60;	/* default loop down limit */
int isp_quickboot_time = 7;	/* don't wait more than N secs for loop up */
int isp_gone_device_time = 30;	/* grace time before reporting device lost */
static const char prom3[] = "Chan %d [%u] PortID 0x%06x Departed because of %s";

static void isp_freeze_loopdown(ispsoftc_t *, int);
static void isp_loop_changed(ispsoftc_t *isp, int chan);
static d_ioctl_t ispioctl;
static void isp_intr_enable(void *);
static void isp_cam_async(void *, uint32_t, struct cam_path *, void *);
static void isp_poll(struct cam_sim *);
static timeout_t isp_watchdog;
static timeout_t isp_gdt;
static task_fn_t isp_gdt_task;
static void isp_kthread(void *);
static void isp_action(struct cam_sim *, union ccb *);
static int isp_timer_count;
static void isp_timer(void *);

static struct cdevsw isp_cdevsw = {
	.d_version =	D_VERSION,
	.d_ioctl =	ispioctl,
	.d_name =	"isp",
};

static int
isp_role_sysctl(SYSCTL_HANDLER_ARGS)
{
	ispsoftc_t *isp = (ispsoftc_t *)arg1;
	int chan = arg2;
	int error, old, value;

	value = FCPARAM(isp, chan)->role;

	error = sysctl_handle_int(oidp, &value, 0, req);
	if ((error != 0) || (req->newptr == NULL))
		return (error);

	if (value < ISP_ROLE_NONE || value > ISP_ROLE_BOTH)
		return (EINVAL);

	ISP_LOCK(isp);
	old = FCPARAM(isp, chan)->role;

	/* We don't allow target mode switch from here. */
	value = (old & ISP_ROLE_TARGET) | (value & ISP_ROLE_INITIATOR);

	/* If nothing has changed -- we are done. */
	if (value == old) {
		ISP_UNLOCK(isp);
		return (0);
	}

	/* Actually change the role. */
	error = isp_control(isp, ISPCTL_CHANGE_ROLE, chan, value);
	ISP_UNLOCK(isp);
	return (error);
}

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
	} else {
		fcparam *fcp = FCPARAM(isp, chan);
		struct isp_fc *fc = ISP_FC_PC(isp, chan);
		struct sysctl_ctx_list *ctx = device_get_sysctl_ctx(isp->isp_osinfo.dev);
		struct sysctl_oid *tree = device_get_sysctl_tree(isp->isp_osinfo.dev);
		char name[16];

		ISP_LOCK(isp);
		fc->sim = sim;
		fc->path = path;
		fc->isp = isp;
		fc->ready = 1;

		callout_init_mtx(&fc->gdt, &isp->isp_osinfo.lock, 0);
		TASK_INIT(&fc->gtask, 1, isp_gdt_task, fc);
		isp_loop_changed(isp, chan);
		ISP_UNLOCK(isp);
		if (THREAD_CREATE(isp_kthread, fc, &fc->kproc, 0, 0, "%s: fc_thrd%d", device_get_nameunit(isp->isp_osinfo.dev), chan)) {
			xpt_free_path(fc->path);
			ISP_LOCK(isp);
			xpt_bus_deregister(cam_sim_path(fc->sim));
			ISP_UNLOCK(isp);
			cam_sim_free(fc->sim, FALSE);
			return (ENOMEM);
		}
		fc->num_threads += 1;
		if (chan > 0) {
			snprintf(name, sizeof(name), "chan%d", chan);
			tree = SYSCTL_ADD_NODE(ctx, SYSCTL_CHILDREN(tree),
			    OID_AUTO, name, CTLFLAG_RW, 0, "Virtual channel");
		}
		SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "wwnn", CTLFLAG_RD, &fcp->isp_wwnn,
		    "World Wide Node Name");
		SYSCTL_ADD_QUAD(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "wwpn", CTLFLAG_RD, &fcp->isp_wwpn,
		    "World Wide Port Name");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "loop_down_limit", CTLFLAG_RW, &fc->loop_down_limit, 0,
		    "Loop Down Limit");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "gone_device_time", CTLFLAG_RW, &fc->gone_device_time, 0,
		    "Gone Device Time");
#if defined(ISP_TARGET_MODE) && defined(DEBUG)
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "inject_lost_data_frame", CTLFLAG_RW, &fc->inject_lost_data_frame, 0,
		    "Cause a Lost Frame on a Read");
#endif
		SYSCTL_ADD_PROC(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "role", CTLTYPE_INT | CTLFLAG_RW, isp, chan,
		    isp_role_sysctl, "I", "Current role");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "speed", CTLFLAG_RD, &fcp->isp_gbspeed, 0,
		    "Connection speed in gigabits");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "linkstate", CTLFLAG_RD, &fcp->isp_linkstate, 0,
		    "Link state");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "fwstate", CTLFLAG_RD, &fcp->isp_fwstate, 0,
		    "Firmware state");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "loopstate", CTLFLAG_RD, &fcp->isp_loopstate, 0,
		    "Loop state");
		SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
		    "topo", CTLFLAG_RD, &fcp->isp_topo, 0,
		    "Connection topology");
	}
	return (0);
}

static void
isp_detach_chan(ispsoftc_t *isp, int chan)
{
	struct cam_sim *sim;
	struct cam_path *path;
	struct ccb_setasync csa;
	int *num_threads;

	ISP_GET_PC(isp, chan, sim, sim);
	ISP_GET_PC(isp, chan, path, path);
	ISP_GET_PC_ADDR(isp, chan, num_threads, num_threads);

	xpt_setup_ccb(&csa.ccb_h, path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = 0;
	csa.callback = isp_cam_async;
	csa.callback_arg = sim;
	xpt_action((union ccb *)&csa);
	xpt_free_path(path);
	xpt_bus_deregister(cam_sim_path(sim));
	cam_sim_free(sim, FALSE);

	/* Wait for the channel's spawned threads to exit. */
	wakeup(isp->isp_osinfo.pc.ptr);
	while (*num_threads != 0)
		mtx_sleep(isp, &isp->isp_osinfo.lock, PRIBIO, "isp_reap", 100);
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

		ISP_GET_PC(isp, chan, sim, sim);
		ISP_GET_PC(isp, chan, path, path);
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
	int chan;

	ISP_LOCK(isp);
	for (chan = isp->isp_nchan - 1; chan >= 0; chan -= 1) {
		ISP_GET_PC(isp, chan, sim, sim);
		if (sim->refcount > 2) {
			ISP_UNLOCK(isp);
			return (EBUSY);
		}
	}
	/* Tell spawned threads that we're exiting. */
	isp->isp_osinfo.is_exiting = 1;
	if (isp->isp_osinfo.timer_active) {
		callout_stop(&isp->isp_osinfo.tmo);
		isp->isp_osinfo.timer_active = 0;
	}
	for (chan = isp->isp_nchan - 1; chan >= 0; chan -= 1)
		isp_detach_chan(isp, chan);
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
isp_freeze_loopdown(ispsoftc_t *isp, int chan)
{
	if (IS_FC(isp)) {
		struct isp_fc *fc = ISP_FC_PC(isp, chan);
		if (fc->simqfrozen == 0) {
			isp_prt(isp, ISP_LOGDEBUG0,
			    "Chan %d Freeze simq (loopdown)", chan);
			fc->simqfrozen = SIMQFRZ_LOOPDOWN;
#if __FreeBSD_version >= 1000039
			xpt_hold_boot();
#endif
			xpt_freeze_simq(fc->sim, 1);
		} else {
			isp_prt(isp, ISP_LOGDEBUG0,
			    "Chan %d Mark simq frozen (loopdown)", chan);
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
			isp_prt(isp, ISP_LOGDEBUG0,
			    "Chan %d Release simq", chan);
			xpt_release_simq(fc->sim, 1);
#if __FreeBSD_version >= 1000039
			xpt_release_boot();
#endif
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
			*(int *)addr = ISP_ROLE_INITIATOR;
		}
		retval = 0;
		break;
	case ISP_SETROLE:
		if (IS_SCSI(isp))
			break;
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
		ISP_LOCK(isp);
		*(int *)addr = FCPARAM(isp, chan)->role;
		retval = isp_control(isp, ISPCTL_CHANGE_ROLE, chan, nr);
		ISP_UNLOCK(isp);
		retval = 0;
		break;

	case ISP_RESETHBA:
		ISP_LOCK(isp);
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
			if (isp_fc_runstate(isp, chan, 5 * 1000000) != LOOP_READY) {
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
		if (lp->state != FC_PORTDB_STATE_NIL) {
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
		uint16_t nphdl;
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
		nphdl = fct->loopid;
		ISP_LOCK(isp);
		if (IS_24XX(isp)) {
			void *reqp;
			uint8_t resp[QENTRY_LEN];
			isp24xx_tmf_t tmf;
			isp24xx_statusreq_t sp;
			fcparam *fcp = FCPARAM(isp, chan);
			fcportdb_t *lp;
			int i;

			for (i = 0; i < MAX_FC_TARG; i++) {
				lp = &fcp->portdb[i];
				if (lp->handle == nphdl) {
					break;
				}
			}
			if (i == MAX_FC_TARG) {
				retval = ENXIO;
				ISP_UNLOCK(isp);
				break;
			}
			ISP_MEMZERO(&tmf, sizeof(tmf));
			tmf.tmf_header.rqs_entry_type = RQSTYPE_TSK_MGMT;
			tmf.tmf_header.rqs_entry_count = 1;
			tmf.tmf_nphdl = lp->handle;
			tmf.tmf_delay = 2;
			tmf.tmf_timeout = 4;
			tmf.tmf_tidlo = lp->portid;
			tmf.tmf_tidhi = lp->portid >> 16;
			tmf.tmf_vpidx = ISP_GET_VPIDX(isp, chan);
			tmf.tmf_lun[1] = fct->lun & 0xff;
			if (fct->lun >= 256) {
				tmf.tmf_lun[0] = 0x40 | (fct->lun >> 8);
			}
			switch (fct->action) {
			case IPT_CLEAR_ACA:
				tmf.tmf_flags = ISP24XX_TMF_CLEAR_ACA;
				break;
			case IPT_TARGET_RESET:
				tmf.tmf_flags = ISP24XX_TMF_TARGET_RESET;
				needmarker = 1;
				break;
			case IPT_LUN_RESET:
				tmf.tmf_flags = ISP24XX_TMF_LUN_RESET;
				needmarker = 1;
				break;
			case IPT_CLEAR_TASK_SET:
				tmf.tmf_flags = ISP24XX_TMF_CLEAR_TASK_SET;
				needmarker = 1;
				break;
			case IPT_ABORT_TASK_SET:
				tmf.tmf_flags = ISP24XX_TMF_ABORT_TASK_SET;
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

			/* Prepare space for response in memory */
			memset(resp, 0xff, sizeof(resp));
			tmf.tmf_handle = isp_allocate_handle(isp, resp,
			    ISP_HANDLE_CTRL);
			if (tmf.tmf_handle == 0) {
				isp_prt(isp, ISP_LOGERR,
				    "%s: TMF of Chan %d out of handles",
				    __func__, chan);
				ISP_UNLOCK(isp);
				retval = ENOMEM;
				break;
			}

			/* Send request and wait for response. */
			reqp = isp_getrqentry(isp);
			if (reqp == NULL) {
				isp_prt(isp, ISP_LOGERR,
				    "%s: TMF of Chan %d out of rqent",
				    __func__, chan);
				isp_destroy_handle(isp, tmf.tmf_handle);
				ISP_UNLOCK(isp);
				retval = EIO;
				break;
			}
			isp_put_24xx_tmf(isp, &tmf, (isp24xx_tmf_t *)reqp);
			if (isp->isp_dblev & ISP_LOGDEBUG1)
				isp_print_bytes(isp, "IOCB TMF", QENTRY_LEN, reqp);
			ISP_SYNC_REQUEST(isp);
			if (msleep(resp, &isp->isp_lock, 0, "TMF", 5*hz) == EWOULDBLOCK) {
				isp_prt(isp, ISP_LOGERR,
				    "%s: TMF of Chan %d timed out",
				    __func__, chan);
				isp_destroy_handle(isp, tmf.tmf_handle);
				ISP_UNLOCK(isp);
				retval = EIO;
				break;
			}
			if (isp->isp_dblev & ISP_LOGDEBUG1)
				isp_print_bytes(isp, "IOCB TMF response", QENTRY_LEN, resp);
			isp_get_24xx_response(isp, (isp24xx_statusreq_t *)resp, &sp);

			if (sp.req_completion_status != 0)
				retval = EIO;
			else if (needmarker)
				fcp->sendmarker = 1;
		} else {
			MBSINIT(&mbs, 0, MBLOGALL, 0);
			if (ISP_CAP_2KLOGIN(isp) == 0) {
				nphdl <<= 8;
			}
			switch (fct->action) {
			case IPT_CLEAR_ACA:
				mbs.param[0] = MBOX_CLEAR_ACA;
				mbs.param[1] = nphdl;
				mbs.param[2] = fct->lun;
				break;
			case IPT_TARGET_RESET:
				mbs.param[0] = MBOX_TARGET_RESET;
				mbs.param[1] = nphdl;
				needmarker = 1;
				break;
			case IPT_LUN_RESET:
				mbs.param[0] = MBOX_LUN_RESET;
				mbs.param[1] = nphdl;
				mbs.param[2] = fct->lun;
				needmarker = 1;
				break;
			case IPT_CLEAR_TASK_SET:
				mbs.param[0] = MBOX_CLEAR_TASK_SET;
				mbs.param[1] = nphdl;
				mbs.param[2] = fct->lun;
				needmarker = 1;
				break;
			case IPT_ABORT_TASK_SET:
				mbs.param[0] = MBOX_ABORT_TASK_SET;
				mbs.param[1] = nphdl;
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
	if (IS_FC(isp)) {
		for (chan = 0; chan < isp->isp_nchan; chan++) {
			if (FCPARAM(isp, chan)->role != ISP_ROLE_NONE) {
				ISP_ENABLE_INTS(isp);
				break;
			}
		}
	} else {
		ISP_ENABLE_INTS(isp);
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
static void isp_disable_lun(ispsoftc_t *, union ccb *);
static timeout_t isp_refire_putback_atio;
static timeout_t isp_refire_notify_ack;
static void isp_complete_ctio(union ccb *);
static void isp_target_putback_atio(union ccb *);
enum Start_Ctio_How { FROM_CAM, FROM_TIMER, FROM_SRR, FROM_CTIO_DONE };
static void isp_target_start_ctio(ispsoftc_t *, union ccb *, enum Start_Ctio_How);
static void isp_handle_platform_atio2(ispsoftc_t *, at2_entry_t *);
static void isp_handle_platform_atio7(ispsoftc_t *, at7_entry_t *);
static void isp_handle_platform_ctio(ispsoftc_t *, void *);
static void isp_handle_platform_notify_fc(ispsoftc_t *, in_fcentry_t *);
static void isp_handle_platform_notify_24xx(ispsoftc_t *, in_fcentry_24xx_t *);
static int isp_handle_platform_target_notify_ack(ispsoftc_t *, isp_notify_t *, uint32_t rsp);
static void isp_handle_platform_target_tmf(ispsoftc_t *, isp_notify_t *);
static void isp_target_mark_aborted(ispsoftc_t *, union ccb *);
static void isp_target_mark_aborted_early(ispsoftc_t *, tstate_t *, uint32_t);

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
		xpt_print(tptr->owner, "ATP: [0x%x] origdlen %u bytes_xfrd %u lun %x nphdl 0x%04x s_id 0x%06x d_id 0x%06x oxid 0x%04x state %s\n",
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
		if (ISP_MAX_LUNS(isp) > 0 && lun >= ISP_MAX_LUNS(isp)) {
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

static void
isp_enable_lun(ispsoftc_t *isp, union ccb *ccb)
{
	tstate_t *tptr;
	int bus;
	target_id_t target;
	lun_id_t lun;

	if (!IS_FC(isp) || !ISP_CAP_TMODE(isp) || !ISP_CAP_SCCFW(isp)) {
		xpt_print(ccb->ccb_h.path, "Target mode is not supported\n");
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		xpt_done(ccb);
		return;
	}

	/*
	 * We only support either target and lun both wildcard
	 * or target and lun both non-wildcard.
	 */
	bus = XS_CHANNEL(ccb);
	target = ccb->ccb_h.target_id;
	lun = ccb->ccb_h.target_lun;
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG0|ISP_LOGCONFIG, ccb->ccb_h.path,
	    "enabling lun %jx\n", (uintmax_t)lun);
	if ((target == CAM_TARGET_WILDCARD) != (lun == CAM_LUN_WILDCARD)) {
		ccb->ccb_h.status = CAM_LUN_INVALID;
		xpt_done(ccb);
		return;
	}

	/* Create the state pointer. It should not already exist. */
	tptr = get_lun_statep(isp, bus, lun);
	if (tptr) {
		ccb->ccb_h.status = CAM_LUN_ALRDY_ENA;
		xpt_done(ccb);
		return;
	}
	ccb->ccb_h.status = create_lun_state(isp, bus, ccb->ccb_h.path, &tptr);
	if (ccb->ccb_h.status != CAM_REQ_CMP) {
		xpt_done(ccb);
		return;
	}

	rls_lun_statep(isp, tptr);
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
}

static void
isp_disable_lun(ispsoftc_t *isp, union ccb *ccb)
{
	tstate_t *tptr = NULL;
	int bus;
	target_id_t target;
	lun_id_t lun;

	bus = XS_CHANNEL(ccb);
	target = ccb->ccb_h.target_id;
	lun = ccb->ccb_h.target_lun;
	ISP_PATH_PRT(isp, ISP_LOGTDEBUG0|ISP_LOGCONFIG, ccb->ccb_h.path,
	    "disabling lun %jx\n", (uintmax_t)lun);
	if ((target == CAM_TARGET_WILDCARD) != (lun == CAM_LUN_WILDCARD)) {
		ccb->ccb_h.status = CAM_LUN_INVALID;
		xpt_done(ccb);
		return;
	}

	/* Find the state pointer. */
	if ((tptr = get_lun_statep(isp, bus, lun)) == NULL) {
		ccb->ccb_h.status = CAM_PATH_INVALID;
		xpt_done(ccb);
		return;
	}

	destroy_lun_state(isp, tptr);
	ccb->ccb_h.status = CAM_REQ_CMP;
	xpt_done(ccb);
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
			isp_prt(isp, ISP_LOGTDEBUG0, "[0x%x] now sending synthesized status orig_dl=%u xfered=%u bit=%u",
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
			cto->ct_timeout = (XS_TIME(ccb) + 999) / 1000;
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
		} else {
			ct2_entry_t *cto = (ct2_entry_t *) local;

			if (isp->isp_osinfo.sixtyfourbit)
				cto->ct_header.rqs_entry_type = RQSTYPE_CTIO3;
			else
				cto->ct_header.rqs_entry_type = RQSTYPE_CTIO2;
			cto->ct_header.rqs_entry_count = 1;
			cto->ct_header.rqs_seqno |= ATPD_SEQ_NOTIFY_CAM;
			ATPD_SET_SEQNO(cto, atp);
			if (ISP_CAP_2KLOGIN(isp)) {
				((ct2e_entry_t *)cto)->ct_iid = atp->nphdl;
			} else {
				cto->ct_iid = atp->nphdl;
				if (ISP_CAP_SCCFW(isp) == 0) {
					cto->ct_lun = ccb->ccb_h.target_lun;
				}
			}
			cto->ct_timeout = (XS_TIME(ccb) + 999) / 1000;
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
		}

		if (isp_get_pcmd(isp, ccb)) {
			ISP_PATH_PRT(isp, ISP_LOGWARN, ccb->ccb_h.path, "out of PCMDs\n");
			TAILQ_INSERT_HEAD(&tptr->waitq, &ccb->ccb_h, periph_links.tqe); 
			break;
		}
		handle = isp_allocate_handle(isp, ccb, ISP_HANDLE_TARGET);
		if (handle == 0) {
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
		} else {
			ct2_entry_t *cto = (ct2_entry_t *) local;
			cto->ct_syshandle = handle;
		}

		dmaresult = ISP_DMASETUP(isp, cso, (ispreq_t *) local);
		if (dmaresult != CMD_QUEUED) {
			isp_destroy_handle(isp, handle);
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
	at2_entry_t local, *at = &local;

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
	ISP_MEMZERO(at, sizeof (at2_entry_t));
	at->at_header.rqs_entry_type = RQSTYPE_ATIO2;
	at->at_header.rqs_entry_count = 1;
	if (ISP_CAP_SCCFW(isp)) {
		at->at_scclun = (uint16_t) ccb->ccb_h.target_lun;
#if __FreeBSD_version < 1000700
		if (at->at_scclun >= 256)
			at->at_scclun |= 0x4000;
#endif
	} else {
		at->at_lun = (uint8_t) ccb->ccb_h.target_lun;
	}
	at->at_status = CT_OK;
	at->at_rxid = cso->tag_id;
	at->at_iid = cso->ccb_h.target_id;
	isp_put_atio2(isp, at, qe);
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

static void
isp_handle_platform_atio2(ispsoftc_t *isp, at2_entry_t *aep)
{
	fcparam *fcp;
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

	fcp = FCPARAM(isp, 0);
	if (ISP_CAP_SCCFW(isp)) {
		lun = aep->at_scclun;
#if __FreeBSD_version < 1000700
		lun &= 0x3fff;
#endif
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
	atiop->ccb_h.target_id = fcp->isp_loopid;
	atiop->ccb_h.target_lun = lun;

	/*
	 * We don't get 'suggested' sense data as we do with SCSI cards.
	 */
	atiop->sense_len = 0;

	/*
	 * If we're not in the port database, add ourselves.
	 */
	if (IS_2100(isp))
		atiop->init_id = nphdl;
	else {
		if ((isp_find_pdb_by_handle(isp, 0, nphdl, &lp) == 0 ||
		     lp->state == FC_PORTDB_STATE_ZOMBIE)) {
			uint64_t wwpn =
				(((uint64_t) aep->at_wwpn[0]) << 48) |
				(((uint64_t) aep->at_wwpn[1]) << 32) |
				(((uint64_t) aep->at_wwpn[2]) << 16) |
				(((uint64_t) aep->at_wwpn[3]) <<  0);
			isp_add_wwn_entry(isp, 0, wwpn, INI_NONE,
			    nphdl, PORT_ANY, 0);
			if (fcp->isp_loopstate > LOOP_LTEST_DONE)
				fcp->isp_loopstate = LOOP_LTEST_DONE;
			isp_async(isp, ISPASYNC_CHANGE_NOTIFY, 0,
			    ISPASYNC_CHANGE_PDB, nphdl, 0x06, 0xff);
			isp_find_pdb_by_handle(isp, 0, nphdl, &lp);
		}
		atiop->init_id = FC_PORTDB_TGT(isp, 0, lp);
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
	atp->nphdl = nphdl;
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
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
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
	lun_id_t lun;
	uint16_t chan, nphdl = NIL_HANDLE;
	uint32_t did, sid;
	fcportdb_t *lp;
	tstate_t *tptr;
	struct ccb_accept_tio *atiop;
	atio_private_data_t *atp = NULL;
	atio_private_data_t *oatp;
	inot_private_data_t *ntp;

	did = (aep->at_hdr.d_id[0] << 16) | (aep->at_hdr.d_id[1] << 8) | aep->at_hdr.d_id[2];
	sid = (aep->at_hdr.s_id[0] << 16) | (aep->at_hdr.s_id[1] << 8) | aep->at_hdr.s_id[2];
#if __FreeBSD_version >= 1000700
	lun = CAM_EXTLUN_BYTE_SWIZZLE(be64dec(aep->at_cmnd.fcp_cmnd_lun));
#else
	lun = (aep->at_cmnd.fcp_cmnd_lun[0] & 0x3f << 8) |
	    aep->at_cmnd.fcp_cmnd_lun[1];
#endif

	/*
	 * Find the N-port handle, and Virtual Port Index for this command.
	 *
	 * If we can't, we're somewhat in trouble because we can't actually respond w/o that information.
	 * We also, as a matter of course, need to know the WWN of the initiator too.
	 */
	if (ISP_CAP_MULTI_ID(isp) && isp->isp_nchan > 1) {
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
			if (NANOTIME_SUB(&now, &isp->isp_init_time) > 2000000000ULL) {
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
	if (isp_find_pdb_by_portid(isp, chan, sid, &lp) == 0) {
		/*
		 * If we're not in the port database terminate the exchange.
		 */
		isp_prt(isp, ISP_LOGTINFO, "%s: [RX_ID 0x%x] D_ID 0x%06x found on Chan %d for S_ID 0x%06x wasn't in PDB already",
		    __func__, aep->at_rxid, did, chan, sid);
		isp_dump_portdb(isp, chan);
		isp_endcmd(isp, aep, NIL_HANDLE, chan, ECMD_TERMINATE, 0);
		return;
	}
	nphdl = lp->handle;

	/*
	 * Get the tstate pointer
	 */
	tptr = get_lun_statep(isp, chan, lun);
	if (tptr == NULL) {
		tptr = get_lun_statep(isp, chan, CAM_LUN_WILDCARD);
		if (tptr == NULL) {
			isp_prt(isp, ISP_LOGWARN,
			    "%s: [0x%x] no state pointer for lun %jx or wildcard",
			    __func__, aep->at_rxid, (uintmax_t)lun);
			if (lun == 0) {
				isp_endcmd(isp, aep, nphdl, chan, SCSI_STATUS_BUSY, 0);
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
	atiop->init_id = FC_PORTDB_TGT(isp, chan, lp);
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
	isp_prt(isp, ISP_LOGTDEBUG0, "ATIO7[0x%x] CDB=0x%x lun %jx datalen %u",
	    aep->at_rxid, atp->cdb0, (uintmax_t)lun, atp->orig_datalen);
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

	handle = ((ct2_entry_t *)arg)->ct_syshandle;
	ccb = isp_find_xs(isp, handle);
	if (ccb == NULL) {
		isp_print_bytes(isp, "null ccb in isp_handle_platform_ctio", QENTRY_LEN, arg);
		return;
	}
	isp_destroy_handle(isp, handle);
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
	} else {
		atp = isp_find_atpd(isp, tptr, ((ct2_entry_t *)arg)->ct_rxid);
	}
	if (atp == NULL) {
		/*
		 * XXX: isp_clear_commands() generates fake CTIO with zero
		 * ct_rxid value, filling only ct_syshandle.  Workaround
		 * that using tag_id from the CCB, pointed by ct_syshandle.
		 */
		atp = isp_find_atpd(isp, tptr, ccb->csio.tag_id);
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
	} else {
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
		uint16_t nphdl, lun;
		uint32_t sid;
		uint64_t wwn;
		atio_private_data_t *atp;
		fcportdb_t *lp;
		struct ccb_immediate_notify *inot = NULL;

		if (ISP_CAP_SCCFW(isp)) {
			lun = inp->in_scclun;
#if __FreeBSD_version < 1000700
			lun &= 0x3fff;
#endif
		} else {
			lun = inp->in_lun;
		}
		if (ISP_CAP_2KLOGIN(isp)) {
			nphdl = ((in_fcentry_e_t *)inp)->in_iid;
		} else {
			nphdl = inp->in_iid;
		}
		if (isp_find_pdb_by_handle(isp, 0, nphdl, &lp)) {
			wwn = lp->port_wwn;
			sid = lp->portid;
		} else {
			wwn = INI_ANY;
			sid = PORT_ANY;
		}
		tptr = get_lun_statep(isp, 0, lun);
		if (tptr == NULL) {
			tptr = get_lun_statep(isp, 0, CAM_LUN_WILDCARD);
			if (tptr == NULL) {
				isp_prt(isp, ISP_LOGWARN, "ABORT TASK for lun %x, but no tstate", lun);
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
			nt->nt_nphdl = nphdl;
			nt->nt_sid = sid;
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
	char *msg = NULL;
	uint8_t *ptr = (uint8_t *)inot;
	uint64_t wwpn = INI_NONE, wwnn = INI_NONE;

	nphdl = inot->in_nphdl;
	if (nphdl != NIL_HANDLE) {
		portid = inot->in_portid_hi << 16 | inot->in_portid_lo;
	} else {
		portid = PORT_ANY;
	}

	switch (inot->in_status) {
	case IN24XX_ELS_RCVD:
	{
		char buf[16];
		int chan = ISP_GET_VPIDX(isp, inot->in_vpidx);

		/*
		 * Note that we're just getting notification that an ELS was received
		 * (possibly with some associated information sent upstream). This is
		 * *not* the same as being given the ELS frame to accept or reject.
		 */
		switch (inot->in_status_subcode) {
		case LOGO:
			msg = "LOGO";
			wwpn = be64dec(&ptr[IN24XX_PLOGI_WWPN_OFF]);
			isp_del_wwn_entry(isp, chan, wwpn, nphdl, portid);
			break;
		case PRLO:
			msg = "PRLO";
			break;
		case PLOGI:
			msg = "PLOGI";
			wwnn = be64dec(&ptr[IN24XX_PLOGI_WWNN_OFF]);
			wwpn = be64dec(&ptr[IN24XX_PLOGI_WWPN_OFF]);
			isp_add_wwn_entry(isp, chan, wwpn, wwnn,
			    nphdl, portid, prli_options);
			break;
		case PRLI:
			msg = "PRLI";
			prli_options = inot->in_prli_options;
			if (inot->in_flags & IN24XX_FLAG_PN_NN_VALID)
				wwnn = be64dec(&ptr[IN24XX_PRLI_WWNN_OFF]);
			wwpn = be64dec(&ptr[IN24XX_PRLI_WWPN_OFF]);
			isp_add_wwn_entry(isp, chan, wwpn, wwnn,
			    nphdl, portid, prli_options);
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
		msg = "PORT LOGOUT";
		if (isp_find_pdb_by_handle(isp, ISP_GET_VPIDX(isp, inot->in_vpidx), nphdl, &lp)) {
			isp_del_wwn_entry(isp, ISP_GET_VPIDX(isp, inot->in_vpidx), lp->port_wwn, nphdl, lp->portid);
		}
		/* FALLTHROUGH */
	case IN24XX_PORT_CHANGED:
		if (msg == NULL)
			msg = "PORT CHANGED";
		/* FALLTHROUGH */
	case IN24XX_LIP_RESET:
		if (msg == NULL)
			msg = "LIP RESET";
		isp_prt(isp, ISP_LOGINFO, "Chan %d %s (sub-status 0x%x) for N-port handle 0x%x", ISP_GET_VPIDX(isp, inot->in_vpidx), msg, inot->in_status_subcode, nphdl);

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
		if (msg == NULL)
			msg = "SRR RCVD";
		/* FALLTHROUGH */
#endif
	case IN24XX_LINK_RESET:
		if (msg == NULL)
			msg = "LINK RESET";
	case IN24XX_LINK_FAILED:
		if (msg == NULL)
			msg = "LINK FAILED";
	default:
		isp_prt(isp, ISP_LOGWARN, "Chan %d %s", ISP_GET_VPIDX(isp, inot->in_vpidx), msg);
		isp_async(isp, ISPASYNC_TARGET_NOTIFY_ACK, inot);
		break;
	}
}

static int
isp_handle_platform_target_notify_ack(ispsoftc_t *isp, isp_notify_t *mp, uint32_t rsp)
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
		if (isp_find_pdb_by_portid(isp, mp->nt_channel, sid, &lp)) {
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
		if (rsp != 0) {
			cto->ct_scsi_status |= (FCP_RSPLEN_VALID << 8);
			cto->rsp.m1.ct_resplen = 4;
			ISP_MEMZERO(cto->rsp.m1.ct_resp, sizeof (cto->rsp.m1.ct_resp));
			cto->rsp.m1.ct_resp[0] = rsp & 0xff;
			cto->rsp.m1.ct_resp[1] = (rsp >> 8) & 0xff;
			cto->rsp.m1.ct_resp[2] = (rsp >> 16) & 0xff;
			cto->rsp.m1.ct_resp[3] = (rsp >> 24) & 0xff;
		}
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

	if (isp_find_pdb_by_portid(isp, notify->nt_channel, notify->nt_sid, &lp) == 0 &&
	    isp_find_pdb_by_handle(isp, notify->nt_channel, notify->nt_nphdl, &lp) == 0) {
		inot->initiator_id = CAM_TARGET_WILDCARD;
	} else {
		inot->initiator_id = FC_PORTDB_TGT(isp, notify->nt_channel, lp);
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
	case NT_QUERY_TASK_SET:
		inot->arg = MSG_QUERY_TASK_SET;
		break;
	case NT_QUERY_ASYNC_EVENT:
		inot->arg = MSG_QUERY_ASYNC_EVENT;
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
				nflags &= DPARM_SAFE_DFLT;
				if (isp->isp_loaded_fw) {
					nflags |= DPARM_NARROW | DPARM_ASYNC;
				}
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
	uint16_t isr, sema, info;

	if (ISP_READ_ISR(isp, &isr, &sema, &info))
		isp_intr(isp, isr, sema, info);
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
		uint16_t isr, sema, info;
		if (ISP_READ_ISR(isp, &isr, &sema, &info) != 0)
			isp_intr(isp, isr, sema, info);
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
	xpt_rescan(ccb);
}

static void
isp_make_gone(ispsoftc_t *isp, fcportdb_t *fcp, int chan, int tgt)
{
	struct cam_path *tp;
	struct isp_fc *fc = ISP_FC_PC(isp, chan);

	if (xpt_create_path(&tp, NULL, cam_sim_path(fc->sim), tgt, CAM_LUN_WILDCARD) == CAM_REQ_CMP) {
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
	struct ac_contract ac;
	struct ac_device_changed *adc;
	int dbidx, more_to_do = 0;

	ISP_LOCK(isp);
	isp_prt(isp, ISP_LOGDEBUG0, "Chan %d GDT timer expired", chan);
	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &FCPARAM(isp, chan)->portdb[dbidx];

		if (lp->state != FC_PORTDB_STATE_ZOMBIE) {
			continue;
		}
		if (lp->gone_timer != 0) {
			lp->gone_timer -= 1;
			more_to_do++;
			continue;
		}
		isp_prt(isp, ISP_LOGCONFIG, prom3, chan, dbidx, lp->portid, "Gone Device Timeout");
		if (lp->is_target) {
			lp->is_target = 0;
			isp_make_gone(isp, lp, chan, dbidx);
		}
		if (lp->is_initiator) {
			lp->is_initiator = 0;
			ac.contract_number = AC_CONTRACT_DEV_CHG;
			adc = (struct ac_device_changed *) ac.contract_data;
			adc->wwpn = lp->port_wwn;
			adc->port = lp->portid;
			adc->target = dbidx;
			adc->arrived = 0;
			xpt_async(AC_CONTRACT, fc->path, &ac);
		}
		lp->state = FC_PORTDB_STATE_NIL;
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
 * When loop goes down we remember the time and freeze CAM command queue.
 * During some time period we are trying to reprobe the loop.  But if we
 * fail, we tell the OS that devices have gone away and drop the freeze.
 *
 * We don't clear the devices out of our port database because, when loop
 * come back up, we have to do some actual cleanup with the chip at that
 * point (implicit PLOGO, e.g., to get the chip's port database state right).
 */
static void
isp_loop_changed(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	struct isp_fc *fc = ISP_FC_PC(isp, chan);

	if (fc->loop_down_time)
		return;
	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Chan %d Loop changed", chan);
	if (fcp->role & ISP_ROLE_INITIATOR)
		isp_freeze_loopdown(isp, chan);
	fc->loop_dead = 0;
	fc->loop_down_time = time_uptime;
	wakeup(fc);
}

static void
isp_loop_up(ispsoftc_t *isp, int chan)
{
	struct isp_fc *fc = ISP_FC_PC(isp, chan);

	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Chan %d Loop is up", chan);
	fc->loop_seen_once = 1;
	fc->loop_dead = 0;
	fc->loop_down_time = 0;
	isp_unfreeze_loopdown(isp, chan);
}

static void
isp_loop_dead(ispsoftc_t *isp, int chan)
{
	fcparam *fcp = FCPARAM(isp, chan);
	struct isp_fc *fc = ISP_FC_PC(isp, chan);
	fcportdb_t *lp;
	struct ac_contract ac;
	struct ac_device_changed *adc;
	int dbidx, i;

	isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Chan %d Loop is dead", chan);

	/*
	 * Notify to the OS all targets who we now consider have departed.
	 */
	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &fcp->portdb[dbidx];

		if (lp->state == FC_PORTDB_STATE_NIL)
			continue;

		/*
		 * XXX: CLEAN UP AND COMPLETE ANY PENDING COMMANDS FIRST!
		 */
		for (i = 0; i < isp->isp_maxcmds; i++) {
			struct ccb_scsiio *xs;

			if (ISP_H2HT(isp->isp_xflist[i].handle) != ISP_HANDLE_INITIATOR) {
				continue;
			}
			if ((xs = isp->isp_xflist[i].cmd) == NULL) {
				continue;
                        }
			if (dbidx != XS_TGT(xs)) {
				continue;
			}
			isp_prt(isp, ISP_LOGWARN, "command handle 0x%x for %d.%d.%jx orphaned by loop down timeout",
			    isp->isp_xflist[i].handle, chan, XS_TGT(xs),
			    (uintmax_t)XS_LUN(xs));
		}

		isp_prt(isp, ISP_LOGCONFIG, prom3, chan, dbidx, lp->portid, "Loop Down Timeout");
		if (lp->is_target) {
			lp->is_target = 0;
			isp_make_gone(isp, lp, chan, dbidx);
		}
		if (lp->is_initiator) {
			lp->is_initiator = 0;
			ac.contract_number = AC_CONTRACT_DEV_CHG;
			adc = (struct ac_device_changed *) ac.contract_data;
			adc->wwpn = lp->port_wwn;
			adc->port = lp->portid;
			adc->target = dbidx;
			adc->arrived = 0;
			xpt_async(AC_CONTRACT, fc->path, &ac);
		}
	}

	isp_unfreeze_loopdown(isp, chan);
	fc->loop_dead = 1;
	fc->loop_down_time = 0;
}

static void
isp_kthread(void *arg)
{
	struct isp_fc *fc = arg;
	ispsoftc_t *isp = fc->isp;
	int chan = fc - isp->isp_osinfo.pc.fc;
	int slp = 0, d;
	int lb, lim;

	mtx_lock(&isp->isp_osinfo.lock);

	while (isp->isp_osinfo.is_exiting == 0) {
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0,
		    "Chan %d Checking FC state", chan);
		lb = isp_fc_runstate(isp, chan, 250000);
		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0,
		    "Chan %d FC got to %s state", chan,
		    isp_fc_loop_statename(lb));

		/*
		 * Our action is different based upon whether we're supporting
		 * Initiator mode or not. If we are, we might freeze the simq
		 * when loop is down and set all sorts of different delays to
		 * check again.
		 *
		 * If not, we simply just wait for loop to come up.
		 */
		if (lb == LOOP_READY || lb < 0) {
			slp = 0;
		} else {
			/*
			 * If we've never seen loop up and we've waited longer
			 * than quickboot time, or we've seen loop up but we've
			 * waited longer than loop_down_limit, give up and go
			 * to sleep until loop comes up.
			 */
			if (fc->loop_seen_once == 0)
				lim = isp_quickboot_time;
			else
				lim = fc->loop_down_limit;
			d = time_uptime - fc->loop_down_time;
			if (d >= lim)
				slp = 0;
			else if (d < 10)
				slp = 1;
			else if (d < 30)
				slp = 5;
			else if (d < 60)
				slp = 10;
			else if (d < 120)
				slp = 20;
			else
				slp = 30;
		}

		if (slp == 0) {
			if (lb == LOOP_READY)
				isp_loop_up(isp, chan);
			else
				isp_loop_dead(isp, chan);
		}

		isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0,
		    "Chan %d sleep for %d seconds", chan, slp);
		msleep(fc, &isp->isp_osinfo.lock, PRIBIO, "ispf", slp * hz);
	}
	fc->num_threads -= 1;
	mtx_unlock(&isp->isp_osinfo.lock);
	kthread_exit();
}

static void
isp_action(struct cam_sim *sim, union ccb *ccb)
{
	int bus, tgt, ts, error;
	ispsoftc_t *isp;
	struct ccb_trans_settings *cts;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("isp_action\n"));

	isp = (ispsoftc_t *)cam_sim_softc(sim);
	mtx_assert(&isp->isp_lock, MA_OWNED);
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
				isp_done((struct ccb_scsiio *) ccb);
				break;
			}
		}
		ccb->csio.req_map = NULL;
#ifdef	DIAGNOSTIC
		if (ccb->ccb_h.target_id >= ISP_MAX_TARGETS(isp)) {
			xpt_print(ccb->ccb_h.path, "invalid target\n");
			ccb->ccb_h.status = CAM_PATH_INVALID;
		} else if (ISP_MAX_LUNS(isp) > 0 &&
		    ccb->ccb_h.target_lun >= ISP_MAX_LUNS(isp)) {
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
			ccb->ccb_h.status = CAM_REQUEUE_REQ;
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
			 * We get this result if the loop isn't ready
			 * or if the device in question has gone zombie.
			 */
			if (ISP_FC_PC(isp, bus)->loop_dead) {
				isp_prt(isp, ISP_LOGDEBUG0,
				    "%d.%jx loop is dead",
				    XS_TGT(ccb), (uintmax_t)XS_LUN(ccb));
				ccb->ccb_h.status = CAM_SEL_TIMEOUT;
				isp_done((struct ccb_scsiio *) ccb);
				break;
			}
			isp_prt(isp, ISP_LOGDEBUG0, "%d.%jx retry later",
			    XS_TGT(ccb), (uintmax_t)XS_LUN(ccb));
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
		if (isp_handle_platform_target_notify_ack(isp, &ntp->rd.nt,
		    (ccb->ccb_h.flags & CAM_SEND_STATUS) ? ccb->cna2.arg : 0)) {
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
		bus = cam_sim_bus(xpt_path_sim(ccb->ccb_h.path));
		tgt = ccb->ccb_h.target_id;
		tgt |= (bus << 16);

		error = isp_control(isp, ISPCTL_RESET_DEV, bus, tgt);
		if (error) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		} else {
			/*
			 * If we have a FC device, reset the Command
			 * Reference Number, because the target will expect
			 * that we re-start the CRN at 1 after a reset.
			 */
			if (IS_FC(isp))
				isp_fcp_reset_crn(isp, bus, tgt, /*tgt_set*/ 1);

			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		break;
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

			cts->protocol = PROTO_SCSI;
			cts->protocol_version = SCSI_REV_2;
			cts->transport = XPORT_FC;
			cts->transport_version = 0;

			scsi->valid = CTS_SCSI_VALID_TQ;
			scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;
			fc->valid = CTS_FC_VALID_SPEED;
			fc->bitrate = 100000;
			fc->bitrate *= fcp->isp_gbspeed;
			if (tgt < MAX_FC_TARG) {
				fcportdb_t *lp = &fcp->portdb[tgt];
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
				if (fcp->role != ISP_ROLE_BOTH) {
					rchange = 1;
					newrole = ISP_ROLE_BOTH;
				}
				break;
			}
			if (rchange) {
				ISP_PATH_PRT(isp, ISP_LOGCONFIG, ccb->ccb_h.path, "changing role on from %d to %d\n", fcp->role, newrole);
				if (isp_control(isp, ISPCTL_CHANGE_ROLE,
				    bus, newrole) != 0) {
					ccb->ccb_h.status = CAM_REQ_CMP_ERR;
					xpt_done(ccb);
					break;
				}
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
		if (IS_FC(isp) && ISP_CAP_TMODE(isp) && ISP_CAP_SCCFW(isp))
			cpi->target_sprt = PIT_PROCESSOR | PIT_DISCONNECT | PIT_TERM_IO;
		else
#endif
			cpi->target_sprt = 0;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = ISP_MAX_TARGETS(isp) - 1;
		cpi->max_lun = ISP_MAX_LUNS(isp) == 0 ?
		    255 : ISP_MAX_LUNS(isp) - 1;
		cpi->bus_id = cam_sim_bus(sim);
		if (isp->isp_osinfo.sixtyfourbit)
			cpi->maxio = (ISP_NSEG64_MAX - 1) * PAGE_SIZE;
		else
			cpi->maxio = (ISP_NSEG_MAX - 1) * PAGE_SIZE;

		bus = cam_sim_bus(xpt_path_sim(cpi->ccb_h.path));
		if (IS_FC(isp)) {
			fcparam *fcp = FCPARAM(isp, bus);

			cpi->hba_misc = PIM_NOBUSRESET | PIM_UNMAPPED;
#if __FreeBSD_version >= 1000700
			cpi->hba_misc |= PIM_EXTLUNS;
#endif
#if __FreeBSD_version >= 1000039
			cpi->hba_misc |= PIM_NOSCAN;
#endif

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
			isp_prt(isp, ISP_LOGDEBUG0,
			    "target %d lun %jx CAM status 0x%x SCSI status 0x%x",
			    XS_TGT(sccb), (uintmax_t)XS_LUN(sccb),
			    sccb->ccb_h.status, sccb->scsi_status);
		else if ((IS_FC(isp))
		      && (XS_TGT(sccb) < MAX_FC_TARG)) {
			fcparam *fcp;

			fcp = FCPARAM(isp, XS_CHANNEL(sccb));
			fcp->portdb[XS_TGT(sccb)].is_target = 0;
		}
		if ((sccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
			sccb->ccb_h.status |= CAM_DEV_QFRZN;
			xpt_freeze_devq(sccb->ccb_h.path, 1);
		}
	}

	if ((CAM_DEBUGGED(sccb->ccb_h.path, ISPDDB)) && (sccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		xpt_print(sccb->ccb_h.path, "cam completion status 0x%x\n", sccb->ccb_h.status);
	}

	if (ISP_PCMD(sccb)) {
		if (callout_active(&PISP_PCMD(sccb)->wdog))
			callout_stop(&PISP_PCMD(sccb)->wdog);
		isp_free_pcmd(isp, (union ccb *) sccb);
	}
	xpt_done((union ccb *) sccb);
}

void
isp_async(ispsoftc_t *isp, ispasync_t cmd, ...)
{
	int bus;
	static const char prom[] = "Chan %d [%d] WWPN 0x%16jx PortID 0x%06x handle 0x%x %s %s";
	char buf[64];
	char *msg = NULL;
	target_id_t tgt;
	fcportdb_t *lp;
	struct isp_fc *fc;
	struct cam_path *tmppath;
	struct ac_contract ac;
	struct ac_device_changed *adc;
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
		if (msg == NULL)
			msg = "LIP Received";
		/* FALLTHROUGH */
	case ISPASYNC_LOOP_RESET:
		if (msg == NULL)
			msg = "LOOP Reset";
		/* FALLTHROUGH */
	case ISPASYNC_LOOP_DOWN:
		if (msg == NULL)
			msg = "LOOP Down";
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		va_end(ap);
		isp_fcp_reset_crn(isp, bus, /*tgt*/0, /*tgt_set*/ 0);
		isp_loop_changed(isp, bus);
		isp_prt(isp, ISP_LOGINFO, "Chan %d %s", bus, msg);
		break;
	case ISPASYNC_LOOP_UP:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		va_end(ap);
		isp_loop_changed(isp, bus);
		isp_prt(isp, ISP_LOGINFO, "Chan %d Loop UP", bus);
		break;
	case ISPASYNC_DEV_ARRIVED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);
		tgt = FC_PORTDB_TGT(isp, bus, lp);
		isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
		isp_prt(isp, ISP_LOGCONFIG, prom, bus, tgt, lp->port_wwn, lp->portid, lp->handle, buf, "arrived");
		if ((FCPARAM(isp, bus)->role & ISP_ROLE_INITIATOR) &&
		    (lp->prli_word3 & PRLI_WD3_TARGET_FUNCTION)) {
			lp->is_target = 1;
			isp_fcp_reset_crn(isp, bus, tgt, /*tgt_set*/ 1);
			isp_make_here(isp, lp, bus, tgt);
		}
		if ((FCPARAM(isp, bus)->role & ISP_ROLE_TARGET) &&
		    (lp->prli_word3 & PRLI_WD3_INITIATOR_FUNCTION)) {
			lp->is_initiator = 1;
			ac.contract_number = AC_CONTRACT_DEV_CHG;
			adc = (struct ac_device_changed *) ac.contract_data;
			adc->wwpn = lp->port_wwn;
			adc->port = lp->portid;
			adc->target = tgt;
			adc->arrived = 1;
			xpt_async(AC_CONTRACT, fc->path, &ac);
		}
		break;
	case ISPASYNC_DEV_CHANGED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);
		tgt = FC_PORTDB_TGT(isp, bus, lp);
		isp_gen_role_str(buf, sizeof (buf), lp->new_prli_word3);
		isp_prt(isp, ISP_LOGCONFIG, prom, bus, tgt, lp->port_wwn, lp->new_portid, lp->handle, buf, "changed");
changed:
		if (lp->is_target !=
		    ((FCPARAM(isp, bus)->role & ISP_ROLE_INITIATOR) &&
		     (lp->new_prli_word3 & PRLI_WD3_TARGET_FUNCTION))) {
			lp->is_target = !lp->is_target;
			if (lp->is_target) {
				isp_fcp_reset_crn(isp, bus, tgt, /*tgt_set*/ 1);
				isp_make_here(isp, lp, bus, tgt);
			} else {
				isp_make_gone(isp, lp, bus, tgt);
				isp_fcp_reset_crn(isp, bus, tgt, /*tgt_set*/ 1);
			}
		}
		if (lp->is_initiator !=
		    ((FCPARAM(isp, bus)->role & ISP_ROLE_TARGET) &&
		     (lp->new_prli_word3 & PRLI_WD3_INITIATOR_FUNCTION))) {
			lp->is_initiator = !lp->is_initiator;
			ac.contract_number = AC_CONTRACT_DEV_CHG;
			adc = (struct ac_device_changed *) ac.contract_data;
			adc->wwpn = lp->port_wwn;
			adc->port = lp->portid;
			adc->target = tgt;
			adc->arrived = lp->is_initiator;
			xpt_async(AC_CONTRACT, fc->path, &ac);
		}
		break;
	case ISPASYNC_DEV_STAYED:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);
		tgt = FC_PORTDB_TGT(isp, bus, lp);
		isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
		isp_prt(isp, ISP_LOGCONFIG, prom, bus, tgt, lp->port_wwn, lp->portid, lp->handle, buf, "stayed");
		goto changed;
	case ISPASYNC_DEV_GONE:
		va_start(ap, cmd);
		bus = va_arg(ap, int);
		lp = va_arg(ap, fcportdb_t *);
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);
		tgt = FC_PORTDB_TGT(isp, bus, lp);
		/*
		 * If this has a virtual target or initiator set the isp_gdt
		 * timer running on it to delay its departure.
		 */
		isp_gen_role_str(buf, sizeof (buf), lp->prli_word3);
		if (lp->is_target || lp->is_initiator) {
			lp->state = FC_PORTDB_STATE_ZOMBIE;
			lp->gone_timer = fc->gone_device_time;
			isp_prt(isp, ISP_LOGCONFIG, prom, bus, tgt, lp->port_wwn, lp->portid, lp->handle, buf, "gone zombie");
			if (fc->ready && !callout_active(&fc->gdt)) {
				isp_prt(isp, ISP_LOG_SANCFG|ISP_LOGDEBUG0, "Chan %d Starting Gone Device Timer with %u seconds time now %lu", bus, lp->gone_timer, (unsigned long)time_uptime);
				callout_reset(&fc->gdt, hz, isp_gdt, fc);
			}
			break;
		}
		isp_prt(isp, ISP_LOGCONFIG, prom, bus, tgt, lp->port_wwn, lp->portid, lp->handle, buf, "gone");
		break;
	case ISPASYNC_CHANGE_NOTIFY:
	{
		char *msg;
		int evt, nphdl, nlstate, portid, reason;

		va_start(ap, cmd);
		bus = va_arg(ap, int);
		evt = va_arg(ap, int);
		if (evt == ISPASYNC_CHANGE_PDB) {
			nphdl = va_arg(ap, int);
			nlstate = va_arg(ap, int);
			reason = va_arg(ap, int);
		} else if (evt == ISPASYNC_CHANGE_SNS) {
			portid = va_arg(ap, int);
		} else {
			nphdl = NIL_HANDLE;
			nlstate = reason = 0;
		}
		va_end(ap);
		fc = ISP_FC_PC(isp, bus);

		if (evt == ISPASYNC_CHANGE_PDB) {
			msg = "Port Database Changed";
			isp_prt(isp, ISP_LOGINFO,
			    "Chan %d %s (nphdl 0x%x state 0x%x reason 0x%x)",
			    bus, msg, nphdl, nlstate, reason);
		} else if (evt == ISPASYNC_CHANGE_SNS) {
			msg = "Name Server Database Changed";
			isp_prt(isp, ISP_LOGINFO, "Chan %d %s (PortID 0x%06x)",
			    bus, msg, portid);
		} else {
			msg = "Other Change Notify";
			isp_prt(isp, ISP_LOGINFO, "Chan %d %s", bus, msg);
		}
		isp_loop_changed(isp, bus);
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
		case NT_QUERY_TASK_SET:
		case NT_QUERY_ASYNC_EVENT:
			/*
			 * These are task management functions.
			 */
			isp_handle_platform_target_tmf(isp, notify);
			break;
		case NT_BUS_RESET:
		case NT_LIP_RESET:
		case NT_LINK_UP:
		case NT_LINK_DOWN:
		case NT_HBA_RESET:
			/*
			 * No action need be taken here.
			 */
			break;
		case NT_GLOBAL_LOGOUT:
		case NT_LOGOUT:
			/*
			 * This is device arrival/departure notification
			 */
			isp_handle_platform_target_notify_ack(isp, notify, 0);
			break;
		default:
			isp_prt(isp, ISP_LOGALL, "target notify code 0x%x", notify->nt_ncode);
			isp_handle_platform_target_notify_ack(isp, notify, 0);
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
			if (IS_24XX(isp)) {
				isp_handle_platform_notify_24xx(isp, (in_fcentry_24xx_t *) hp);
			} else {
				isp_handle_platform_notify_fc(isp, (in_fcentry_t *) hp);
			}
			break;
		case RQSTYPE_ATIO:
			isp_handle_platform_atio7(isp, (at7_entry_t *) hp);
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
				if (isp_find_pdb_by_handle(isp, chan, abts->abts_nphdl, &lp)) {
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

uint64_t
isp_default_wwn(ispsoftc_t * isp, int chan, int isactive, int iswwnn)
{
	uint64_t seed;
	struct isp_fc *fc = ISP_FC_PC(isp, chan);

	/* First try to use explicitly configured WWNs. */
	seed = iswwnn ? fc->def_wwnn : fc->def_wwpn;
	if (seed)
		return (seed);

	/* Otherwise try to use WWNs from NVRAM. */
	if (isactive) {
		seed = iswwnn ? FCPARAM(isp, chan)->isp_wwnn_nvram :
		    FCPARAM(isp, chan)->isp_wwpn_nvram;
		if (seed)
			return (seed);
	}

	/* If still no WWNs, try to steal them from the first channel. */
	if (chan > 0) {
		seed = iswwnn ? ISP_FC_PC(isp, 0)->def_wwnn :
		    ISP_FC_PC(isp, 0)->def_wwpn;
		if (seed == 0) {
			seed = iswwnn ? FCPARAM(isp, 0)->isp_wwnn_nvram :
			    FCPARAM(isp, 0)->isp_wwpn_nvram;
		}
	}

	/* If still nothing -- improvise. */
	if (seed == 0) {
		seed = 0x400000007F000000ull + device_get_unit(isp->isp_dev);
		if (!iswwnn)
			seed ^= 0x0100000000000000ULL;
	}

	/* For additional channels we have to improvise even more. */
	if (!iswwnn && chan > 0) {
		/*
		 * We'll stick our channel number plus one first into bits
		 * 57..59 and thence into bits 52..55 which allows for 8 bits
		 * of channel which is enough for our maximum of 255 channels.
		 */
		seed ^= 0x0100000000000000ULL;
		seed ^= ((uint64_t) (chan + 1) & 0xf) << 56;
		seed ^= ((uint64_t) ((chan + 1) >> 4) & 0xf) << 52;
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
				uint16_t isr, sema, info;
				if (isp->isp_osinfo.mboxcmd_done) {
					break;
				}
				if (ISP_READ_ISR(isp, &isr, &sema, &info)) {
					isp_intr(isp, isr, sema, info);
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
	uint16_t isr, sema, info;

	ISP_LOCK(isp);
	isp->isp_intcnt++;
	if (ISP_READ_ISR(isp, &isr, &sema, &info))
		isp_intr(isp, isr, sema, info);
	else
		isp->isp_intbogus++;
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
isp_fcp_reset_crn(ispsoftc_t *isp, int chan, uint32_t tgt, int tgt_set)
{
	struct isp_fc *fc = ISP_FC_PC(isp, chan);
	struct isp_nexus *nxp;
	int i;

	if (tgt_set == 0)
		isp_prt(isp, ISP_LOGDEBUG0,
		    "Chan %d resetting CRN on all targets", chan);
	else
		isp_prt(isp, ISP_LOGDEBUG0,
		    "Chan %d resetting CRN on target %u", chan, tgt);

	for (i = 0; i < NEXUS_HASH_WIDTH; i++) {
		for (nxp = fc->nexus_hash[i]; nxp != NULL; nxp = nxp->next) {
			if (tgt_set == 0 || tgt == nxp->tgt)
				nxp->crnseed = 0;
		}
	}
}

int
isp_fcp_next_crn(ispsoftc_t *isp, uint8_t *crnp, XS_T *cmd)
{
	lun_id_t lun;
	uint32_t chan, tgt;
	struct isp_fc *fc;
	struct isp_nexus *nxp;
	int idx;

	if (IS_2100(isp))
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
	if (nxp->crnseed == 0)
		nxp->crnseed = 1;
	PISP_PCMD(cmd)->crn = nxp->crnseed;
	*crnp = nxp->crnseed++;
	return (0);
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
