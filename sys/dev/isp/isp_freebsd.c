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
 * Platform (FreeBSD) dependent common attachment code for Qlogic adapters.
 */
#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");
#include <dev/isp/isp_freebsd.h>
#include <sys/unistd.h>
#include <sys/kthread.h>
#include <machine/stdarg.h>	/* for use by isp_prt below */
#include <sys/conf.h>
#include <sys/module.h>
#include <sys/ioccom.h>
#include <dev/isp/isp_ioctl.h>
#if	__FreeBSD_version >= 500000
#include <sys/sysctl.h>
#else
#include <sys/devicestat.h>
#endif
#include <cam/cam_periph.h>
#include <cam/cam_xpt_periph.h>

#if !defined(CAM_NEW_TRAN_CODE) && __FreeBSD_version >= 700025
#define	CAM_NEW_TRAN_CODE	1
#endif


MODULE_VERSION(isp, 1);
MODULE_DEPEND(isp, cam, 1, 1, 1);
int isp_announced = 0;
int isp_fabric_hysteresis = 5;
int isp_loop_down_limit = 300;	/* default loop down limit */
int isp_change_is_bad = 0;	/* "changed" devices are bad */
int isp_quickboot_time = 15;	/* don't wait more than N secs for loop up */
int isp_gone_device_time = 30;	/* grace time before reporting device lost */
static const char *roles[4] = {
    "(none)", "Target", "Initiator", "Target/Initiator"
};
static const char prom3[] =
    "PortID 0x%06x Departed from Target %u because of %s";

static void isp_freeze_loopdown(ispsoftc_t *, char *);
static d_ioctl_t ispioctl;
static void isp_intr_enable(void *);
static void isp_cam_async(void *, uint32_t, struct cam_path *, void *);
static void isp_poll(struct cam_sim *);
static timeout_t isp_watchdog;
static timeout_t isp_ldt;
static void isp_kthread(void *);
static void isp_action(struct cam_sim *, union ccb *);

#if __FreeBSD_version < 700000
ispfwfunc *isp_get_firmware_p = NULL;
#endif

#if __FreeBSD_version < 500000  
#define ISP_CDEV_MAJOR	248
static struct cdevsw isp_cdevsw = {
	/* open */	nullopen,
	/* close */	nullclose,
	/* read */	noread,
	/* write */	nowrite,
	/* ioctl */	ispioctl,
	/* poll */	nopoll,
	/* mmap */	nommap,
	/* strategy */	nostrategy,
	/* name */	"isp",
	/* maj */	ISP_CDEV_MAJOR,
	/* dump */	nodump,
	/* psize */	nopsize,
	/* flags */	D_TAPE,
};
#define	isp_sysctl_update(x)	do { ; } while (0)
#else
static struct cdevsw isp_cdevsw = {
	.d_version =	D_VERSION,
#if	__FreeBSD_version < 700037
	.d_flags =	D_NEEDGIANT,
#endif
	.d_ioctl =	ispioctl,
	.d_name =	"isp",
};
static void isp_sysctl_update(ispsoftc_t *);
#endif

static ispsoftc_t *isplist = NULL;

void
isp_attach(ispsoftc_t *isp)
{
	int primary, secondary;
	struct ccb_setasync csa;
	struct cam_devq *devq;
	struct cam_sim *sim;
	struct cam_path *path;

	/*
	 * Establish (in case of 12X0) which bus is the primary.
	 */

	primary = 0;
	secondary = 1;

	/*
	 * Create the device queue for our SIM(s).
	 */
	devq = cam_simq_alloc(isp->isp_maxcmds);
	if (devq == NULL) {
		return;
	}

	/*
	 * Construct our SIM entry.
	 */
	sim = isp_sim_alloc(isp_action, isp_poll, "isp", isp,
	    device_get_unit(isp->isp_dev), 1, isp->isp_maxcmds, devq);
	if (sim == NULL) {
		cam_simq_free(devq);
		return;
	}

	isp->isp_osinfo.ehook.ich_func = isp_intr_enable;
	isp->isp_osinfo.ehook.ich_arg = isp;
	ISP_UNLOCK(isp);
	if (config_intrhook_establish(&isp->isp_osinfo.ehook) != 0) {
		ISP_LOCK(isp);
		cam_sim_free(sim, TRUE);
		isp_prt(isp, ISP_LOGERR,
		    "could not establish interrupt enable hook");
		return;
	}
	ISP_LOCK(isp);

	if (xpt_bus_register(sim, primary) != CAM_SUCCESS) {
		cam_sim_free(sim, TRUE);
		return;
	}

	if (xpt_create_path(&path, NULL, cam_sim_path(sim),
	    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		xpt_bus_deregister(cam_sim_path(sim));
		cam_sim_free(sim, TRUE);
		config_intrhook_disestablish(&isp->isp_osinfo.ehook);
		return;
	}

	xpt_setup_ccb(&csa.ccb_h, path, 5);
	csa.ccb_h.func_code = XPT_SASYNC_CB;
	csa.event_enable = AC_LOST_DEVICE;
	csa.callback = isp_cam_async;
	csa.callback_arg = sim;
	xpt_action((union ccb *)&csa);
	isp->isp_sim = sim;
	isp->isp_path = path;

	/*
	 * If we have a second channel, construct SIM entry for that.
	 */
	if (IS_DUALBUS(isp)) {
		sim = isp_sim_alloc(isp_action, isp_poll, "isp", isp,
		    device_get_unit(isp->isp_dev), 1, isp->isp_maxcmds, devq);
		if (sim == NULL) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			cam_simq_free(devq);
			config_intrhook_disestablish(&isp->isp_osinfo.ehook);
			return;
		}
		if (xpt_bus_register(sim, secondary) != CAM_SUCCESS) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			cam_sim_free(sim, TRUE);
			config_intrhook_disestablish(&isp->isp_osinfo.ehook);
			return;
		}

		if (xpt_create_path(&path, NULL, cam_sim_path(sim),
		    CAM_TARGET_WILDCARD, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			xpt_bus_deregister(cam_sim_path(isp->isp_sim));
			xpt_free_path(isp->isp_path);
			xpt_bus_deregister(cam_sim_path(sim));
			cam_sim_free(sim, TRUE);
			config_intrhook_disestablish(&isp->isp_osinfo.ehook);
			return;
		}

		xpt_setup_ccb(&csa.ccb_h, path, 5);
		csa.ccb_h.func_code = XPT_SASYNC_CB;
		csa.event_enable = AC_LOST_DEVICE;
		csa.callback = isp_cam_async;
		csa.callback_arg = sim;
		xpt_action((union ccb *)&csa);
		isp->isp_sim2 = sim;
		isp->isp_path2 = path;
	}

	/*
	 * Create device nodes
	 */
	ISP_UNLOCK(isp);
	(void) make_dev(&isp_cdevsw, device_get_unit(isp->isp_dev), UID_ROOT,
	    GID_OPERATOR, 0600, "%s", device_get_nameunit(isp->isp_dev));
	isp_sysctl_update(isp);
	ISP_LOCK(isp);

	if (isp->isp_role != ISP_ROLE_NONE) {
		isp->isp_state = ISP_RUNSTATE;
		ISP_ENABLE_INTS(isp);
	}
	if (isplist == NULL) {
		isplist = isp;
	} else {
		ispsoftc_t *tmp = isplist;
		while (tmp->isp_osinfo.next) {
			tmp = tmp->isp_osinfo.next;
		}
		tmp->isp_osinfo.next = isp;
	}

	/*
	 * Create a kernel thread for fibre channel instances.
	 */
	if (IS_FC(isp)) {
		isp_callout_init(&isp->isp_osinfo.ldt);
		isp_callout_init(&isp->isp_osinfo.gdt);
		ISP_UNLOCK(isp);
#if __FreeBSD_version >= 500000  
		if (kthread_create(isp_kthread, isp, &isp->isp_osinfo.kproc,
		    RFHIGHPID, 0, "%s: fc_thrd",
		    device_get_nameunit(isp->isp_dev)))
#else
		if (kthread_create(isp_kthread, isp, &isp->isp_osinfo.kproc,
		    "%s: fc_thrd", device_get_nameunit(isp->isp_dev)))
#endif
		{
			ISP_LOCK(isp);
			xpt_bus_deregister(cam_sim_path(sim));
			cam_sim_free(sim, TRUE);
			config_intrhook_disestablish(&isp->isp_osinfo.ehook);
			isp_prt(isp, ISP_LOGERR, "could not create kthread");
			return;
		}
		ISP_LOCK(isp);
		/*
		 * We start by being "loop down" if we have an initiator role
		 */
		if (isp->isp_role & ISP_ROLE_INITIATOR) {
			isp_freeze_loopdown(isp, "isp_attach");
			isp->isp_osinfo.ldt_running = 1;
			callout_reset(&isp->isp_osinfo.ldt,
			    isp_quickboot_time * hz, isp_ldt, isp);
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			   "Starting Initial Loop Down Timer");
		}
	}
}

static void
isp_freeze_loopdown(ispsoftc_t *isp, char *msg)
{
	if (isp->isp_osinfo.simqfrozen == 0) {
		isp_prt(isp, ISP_LOGDEBUG0, "%s: freeze simq (loopdown)", msg);
		isp->isp_osinfo.simqfrozen |= SIMQFRZ_LOOPDOWN;
		xpt_freeze_simq(isp->isp_sim, 1);
	} else {
		isp_prt(isp, ISP_LOGDEBUG0, "%s: mark frozen (loopdown)", msg);
		isp->isp_osinfo.simqfrozen |= SIMQFRZ_LOOPDOWN;
	}
}


#if __FreeBSD_version < 500000  
#define	_DEV	dev_t
#define	_IOP	struct proc
#else
#define	_IOP	struct thread
#define	_DEV	struct cdev *
#endif

static int
ispioctl(_DEV dev, u_long c, caddr_t addr, int flags, _IOP *td)
{
	ispsoftc_t *isp;
	int nr, retval = ENOTTY;

	isp = isplist;
	while (isp) {
		if (minor(dev) == device_get_unit(isp->isp_dev)) {
			break;
		}
		isp = isp->isp_osinfo.next;
	}
	if (isp == NULL) {
		return (ENXIO);
	}
	
	switch (c) {
#ifdef	ISP_FW_CRASH_DUMP
	case ISP_GET_FW_CRASH_DUMP:
		if (IS_FC(isp)) {
			uint16_t *ptr = FCPARAM(isp)->isp_dump_data;
			size_t sz;

			retval = 0;
			if (IS_2200(isp)) {
				sz = QLA2200_RISC_IMAGE_DUMP_SIZE;
			} else {
				sz = QLA2300_RISC_IMAGE_DUMP_SIZE;
			}
			if (ptr && *ptr) {
				void *uaddr = *((void **) addr);
				if (copyout(ptr, uaddr, sz)) {
					retval = EFAULT;
				} else {
					*ptr = 0;
				}
			} else {
				retval = ENXIO;
			}
		}
		break;
	case ISP_FORCE_CRASH_DUMP:
		if (IS_FC(isp)) {
			ISP_LOCK(isp);
			isp_freeze_loopdown(isp,
			    "ispioctl(ISP_FORCE_CRASH_DUMP)");
			isp_fw_dump(isp);
			isp_reinit(isp);
			ISP_UNLOCK(isp);
			retval = 0;
		}
		break;
#endif
	case ISP_SDBLEV:
	{
		int olddblev = isp->isp_dblev;
		isp->isp_dblev = *(int *)addr;
		*(int *)addr = olddblev;
		retval = 0;
		break;
	}
	case ISP_GETROLE:
		*(int *)addr = isp->isp_role;
		retval = 0;
		break;
	case ISP_SETROLE:
		nr = *(int *)addr;
		if (nr & ~(ISP_ROLE_INITIATOR|ISP_ROLE_TARGET)) {
			retval = EINVAL;
			break;
		}
		*(int *)addr = isp->isp_role;
		isp->isp_role = nr;
		/* FALLTHROUGH */
	case ISP_RESETHBA:
		ISP_LOCK(isp);
		isp_reinit(isp);
		ISP_UNLOCK(isp);
		retval = 0;
		break;
	case ISP_RESCAN:
		if (IS_FC(isp)) {
			ISP_LOCK(isp);
			if (isp_fc_runstate(isp, 5 * 1000000)) {
				retval = EIO;
			} else {
				retval = 0;
			}
			ISP_UNLOCK(isp);
		}
		break;
	case ISP_FC_LIP:
		if (IS_FC(isp)) {
			ISP_LOCK(isp);
			if (isp_control(isp, ISPCTL_SEND_LIP, 0)) {
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
		if (ifc->loopid < 0 || ifc->loopid >= MAX_FC_TARG) {
			retval = EINVAL;
			break;
		}
		lp = &FCPARAM(isp)->portdb[ifc->loopid];
		if (lp->state == FC_PORTDB_STATE_VALID) {
			ifc->role = lp->roles;
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

		MEMZERO(sp, sizeof (*sp));
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
		MEMZERO(hba, sizeof (*hba));

		hba->fc_fw_major = ISP_FW_MAJORX(isp->isp_fwrev);
		hba->fc_fw_minor = ISP_FW_MINORX(isp->isp_fwrev);
		hba->fc_fw_micro = ISP_FW_MICROX(isp->isp_fwrev);
		if (IS_FC(isp)) {
			hba->fc_speed = FCPARAM(isp)->isp_gbspeed;
			hba->fc_scsi_supported = 1;
			hba->fc_topology = FCPARAM(isp)->isp_topo + 1;
			hba->fc_loopid = FCPARAM(isp)->isp_loopid;
			hba->nvram_node_wwn = FCPARAM(isp)->isp_wwnn_nvram;
			hba->nvram_port_wwn = FCPARAM(isp)->isp_wwpn_nvram;
			hba->active_node_wwn = ISP_NODEWWN(isp);
			hba->active_port_wwn = ISP_PORTWWN(isp);
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

		memset(&mbs, 0, sizeof (mbs));
		needmarker = retval = 0;
		loopid = fct->loopid;
		if (FCPARAM(isp)->isp_2klogin == 0) {
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
				isp->isp_sendmarker |= 1;
			}
			ISP_LOCK(isp);
			retval = isp_control(isp, ISPCTL_RUN_MBOXCMD, &mbs);
			ISP_UNLOCK(isp);
			if (retval)
				retval = EIO;
		}
		break;
	}
	default:
		break;
	}
	return (retval);
}

#if __FreeBSD_version >= 500000
static void
isp_sysctl_update(ispsoftc_t *isp)
{
	struct sysctl_ctx_list *ctx =
	    device_get_sysctl_ctx(isp->isp_osinfo.dev);
	struct sysctl_oid *tree = device_get_sysctl_tree(isp->isp_osinfo.dev);

	if (IS_SCSI(isp)) {
		return;
	}

	snprintf(isp->isp_osinfo.sysctl_info.fc.wwnn,
	    sizeof (isp->isp_osinfo.sysctl_info.fc.wwnn), "0x%08x%08x",
	    (uint32_t) (ISP_NODEWWN(isp) >> 32), (uint32_t) ISP_NODEWWN(isp));

	snprintf(isp->isp_osinfo.sysctl_info.fc.wwpn,
	    sizeof (isp->isp_osinfo.sysctl_info.fc.wwpn), "0x%08x%08x",
	    (uint32_t) (ISP_PORTWWN(isp) >> 32), (uint32_t) ISP_PORTWWN(isp));

	SYSCTL_ADD_STRING(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	       "wwnn", CTLFLAG_RD, isp->isp_osinfo.sysctl_info.fc.wwnn, 0,
	       "World Wide Node Name");

	SYSCTL_ADD_STRING(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	       "wwpn", CTLFLAG_RD, isp->isp_osinfo.sysctl_info.fc.wwpn, 0,
	       "World Wide Port Name");

	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "loop_down_limit",
	    CTLFLAG_RW, &isp->isp_osinfo.loop_down_limit, 0,
	    "How long to wait for loop to come back up");

	SYSCTL_ADD_UINT(ctx, SYSCTL_CHILDREN(tree), OID_AUTO,
	    "gone_device_time",
	    CTLFLAG_RW, &isp->isp_osinfo.gone_device_time, 0,
	    "How long to wait for a device to reappear");
}
#endif

static void
isp_intr_enable(void *arg)
{
	ispsoftc_t *isp = arg;
	ISP_LOCK(isp);
	if (isp->isp_role != ISP_ROLE_NONE) {
		ISP_ENABLE_INTS(isp);
	}
	ISP_UNLOCK(isp);
	/* Release our hook so that the boot can continue. */
	config_intrhook_disestablish(&isp->isp_osinfo.ehook);
}

/*
 * Put the target mode functions here, because some are inlines
 */

#ifdef	ISP_TARGET_MODE

static __inline int is_lun_enabled(ispsoftc_t *, int, lun_id_t);
static __inline int are_any_luns_enabled(ispsoftc_t *, int);
static __inline tstate_t *get_lun_statep(ispsoftc_t *, int, lun_id_t);
static __inline void rls_lun_statep(ispsoftc_t *, tstate_t *);
static __inline atio_private_data_t *isp_get_atpd(ispsoftc_t *, int);
static cam_status
create_lun_state(ispsoftc_t *, int, struct cam_path *, tstate_t **);
static void destroy_lun_state(ispsoftc_t *, tstate_t *);
static int isp_en_lun(ispsoftc_t *, union ccb *);
static void isp_ledone(ispsoftc_t *, lun_entry_t *);
static cam_status isp_abort_tgt_ccb(ispsoftc_t *, union ccb *);
static timeout_t isp_refire_putback_atio;
static void isp_complete_ctio(union ccb *);
static void isp_target_putback_atio(union ccb *);
static void isp_target_start_ctio(ispsoftc_t *, union ccb *);
static int isp_handle_platform_atio(ispsoftc_t *, at_entry_t *);
static int isp_handle_platform_atio2(ispsoftc_t *, at2_entry_t *);
static int isp_handle_platform_ctio(ispsoftc_t *, void *);
static int isp_handle_platform_notify_scsi(ispsoftc_t *, in_entry_t *);
static int isp_handle_platform_notify_fc(ispsoftc_t *, in_fcentry_t *);

static __inline int
is_lun_enabled(ispsoftc_t *isp, int bus, lun_id_t lun)
{
	tstate_t *tptr;
	tptr = isp->isp_osinfo.lun_hash[LUN_HASH_FUNC(isp, bus, lun)];
	if (tptr == NULL) {
		return (0);
	}
	do {
		if (tptr->lun == (lun_id_t) lun && tptr->bus == bus) {
			return (1);
		}
	} while ((tptr = tptr->next) != NULL);
	return (0);
}

static __inline int
are_any_luns_enabled(ispsoftc_t *isp, int port)
{
	int lo, hi;
	if (IS_DUALBUS(isp)) {
		lo = (port * (LUN_HASH_SIZE >> 1));
		hi = lo + (LUN_HASH_SIZE >> 1);
	} else {
		lo = 0;
		hi = LUN_HASH_SIZE;
	}
	for (lo = 0; lo < hi; lo++) {
		if (isp->isp_osinfo.lun_hash[lo]) {
			return (1);
		}
	}
	return (0);
}

static __inline tstate_t *
get_lun_statep(ispsoftc_t *isp, int bus, lun_id_t lun)
{
	tstate_t *tptr = NULL;

	if (lun == CAM_LUN_WILDCARD) {
		if (isp->isp_osinfo.tmflags[bus] & TM_WILDCARD_ENABLED) {
			tptr = &isp->isp_osinfo.tsdflt[bus];
			tptr->hold++;
			return (tptr);
		}
		return (NULL);
	} else {
		tptr = isp->isp_osinfo.lun_hash[LUN_HASH_FUNC(isp, bus, lun)];
		if (tptr == NULL) {
			return (NULL);
		}
	}

	do {
		if (tptr->lun == lun && tptr->bus == bus) {
			tptr->hold++;
			return (tptr);
		}
	} while ((tptr = tptr->next) != NULL);
	return (tptr);
}

static __inline void
rls_lun_statep(ispsoftc_t *isp, tstate_t *tptr)
{
	if (tptr->hold)
		tptr->hold--;
}

static __inline atio_private_data_t *
isp_get_atpd(ispsoftc_t *isp, int tag)
{
	atio_private_data_t *atp;
	for (atp = isp->isp_osinfo.atpdp;
	    atp < &isp->isp_osinfo.atpdp[ATPDPSIZE]; atp++) {
		if (atp->tag == tag)
			return (atp);
	}
	return (NULL);
}

static cam_status
create_lun_state(ispsoftc_t *isp, int bus,
    struct cam_path *path, tstate_t **rslt)
{
	cam_status status;
	lun_id_t lun;
	int hfx;
	tstate_t *tptr, *new;

	lun = xpt_path_lun_id(path);
	if (lun < 0) {
		return (CAM_LUN_INVALID);
	}
	if (is_lun_enabled(isp, bus, lun)) {
		return (CAM_LUN_ALRDY_ENA);
	}
	new = (tstate_t *) malloc(sizeof (tstate_t), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (new == NULL) {
		return (CAM_RESRC_UNAVAIL);
	}

	status = xpt_create_path(&new->owner, NULL, xpt_path_path_id(path),
	    xpt_path_target_id(path), xpt_path_lun_id(path));
	if (status != CAM_REQ_CMP) {
		free(new, M_DEVBUF);
		return (status);
	}
	new->bus = bus;
	new->lun = lun;
	SLIST_INIT(&new->atios);
	SLIST_INIT(&new->inots);
	new->hold = 1;

	hfx = LUN_HASH_FUNC(isp, new->bus, new->lun);
	tptr = isp->isp_osinfo.lun_hash[hfx];
	if (tptr == NULL) {
		isp->isp_osinfo.lun_hash[hfx] = new;
	} else {
		while (tptr->next)
			tptr = tptr->next;
		tptr->next = new;
	}
	*rslt = new;
	return (CAM_REQ_CMP);
}

static __inline void
destroy_lun_state(ispsoftc_t *isp, tstate_t *tptr)
{
	int hfx;
	tstate_t *lw, *pw;

	if (tptr->hold) {
		return;
	}
	hfx = LUN_HASH_FUNC(isp, tptr->bus, tptr->lun);
	pw = isp->isp_osinfo.lun_hash[hfx];
	if (pw == NULL) {
		return;
	} else if (pw->lun == tptr->lun && pw->bus == tptr->bus) {
		isp->isp_osinfo.lun_hash[hfx] = pw->next;
	} else {
		lw = pw;
		pw = lw->next;
		while (pw) {
			if (pw->lun == tptr->lun && pw->bus == tptr->bus) {
				lw->next = pw->next;
				break;
			}
			lw = pw;
			pw = pw->next;
		}
		if (pw == NULL) {
			return;
		}
	}
	free(tptr, M_DEVBUF);
}

/*
 * Enable luns.
 */
static int
isp_en_lun(ispsoftc_t *isp, union ccb *ccb)
{
	struct ccb_en_lun *cel = &ccb->cel;
	tstate_t *tptr = NULL;
	uint32_t seq;
	int bus, cmd, av, wildcard, tm_on;
	lun_id_t lun;
	target_id_t tgt;

	bus = XS_CHANNEL(ccb);
	if (bus > 1) {
		xpt_print(ccb->ccb_h.path, "illegal bus %d\n", bus);
		ccb->ccb_h.status = CAM_PATH_INVALID;
		return (-1);
	}
	tgt = ccb->ccb_h.target_id;
	lun = ccb->ccb_h.target_lun;

	if (isp->isp_dblev & ISP_LOGTDEBUG0) {
		xpt_print(ccb->ccb_h.path, "%sabling lun 0x%x on channel %d\n",
	    	    cel->enable? "en" : "dis", lun, bus);
	}

	if ((lun != CAM_LUN_WILDCARD) &&
	    (lun < 0 || lun >= (lun_id_t) isp->isp_maxluns)) {
		ccb->ccb_h.status = CAM_LUN_INVALID;
		return (-1);
	}

	if (IS_SCSI(isp)) {
		sdparam *sdp = isp->isp_param;
		sdp += bus;
		if (tgt != CAM_TARGET_WILDCARD &&
		    tgt != sdp->isp_initiator_id) {
			ccb->ccb_h.status = CAM_TID_INVALID;
			return (-1);
		}
	} else {
		/*
		 * There's really no point in doing this yet w/o multi-tid
		 * capability. Even then, it's problematic.
		 */
#if	0
		if (tgt != CAM_TARGET_WILDCARD &&
		    tgt != FCPARAM(isp)->isp_iid) {
			ccb->ccb_h.status = CAM_TID_INVALID;
			return (-1);
		}
#endif
		/*
		 * This is as a good a place as any to check f/w capabilities.
		 */
		if (FCPARAM(isp)->isp_tmode == 0) {
			xpt_print(ccb->ccb_h.path,
			    "firmware does not support target mode\n");
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			return (-1);
		}
		/*
		 * XXX: We *could* handle non-SCCLUN f/w, but we'd have to
		 * XXX: dork with our already fragile enable/disable code.
		 */
		if (FCPARAM(isp)->isp_sccfw == 0) {
			xpt_print(ccb->ccb_h.path,
			    "firmware not SCCLUN capable\n");
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			return (-1);
		}
	}

	if (tgt == CAM_TARGET_WILDCARD) {
		if (lun == CAM_LUN_WILDCARD) {
			wildcard = 1;
		} else {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			return (-1);
		}
	} else {
		wildcard = 0;
	}

	tm_on = (isp->isp_osinfo.tmflags[bus] & TM_TMODE_ENABLED) != 0;

	/*
	 * Next check to see whether this is a target/lun wildcard action.
	 *
	 * If so, we know that we can accept commands for luns that haven't
	 * been enabled yet and send them upstream. Otherwise, we have to
	 * handle them locally (if we see them at all).
	 */

	if (wildcard) {
		tptr = &isp->isp_osinfo.tsdflt[bus];
		if (cel->enable) {
			if (tm_on) {
				ccb->ccb_h.status = CAM_LUN_ALRDY_ENA;
				return (-1);
			}
			ccb->ccb_h.status =
			    xpt_create_path(&tptr->owner, NULL,
			    xpt_path_path_id(ccb->ccb_h.path),
			    xpt_path_target_id(ccb->ccb_h.path),
			    xpt_path_lun_id(ccb->ccb_h.path));
			if (ccb->ccb_h.status != CAM_REQ_CMP) {
				return (-1);
			}
			SLIST_INIT(&tptr->atios);
			SLIST_INIT(&tptr->inots);
			isp->isp_osinfo.tmflags[bus] |= TM_WILDCARD_ENABLED;
		} else {
			if (tm_on == 0) {
				ccb->ccb_h.status = CAM_REQ_CMP;
				return (-1);
			}
			if (tptr->hold) {
				ccb->ccb_h.status = CAM_SCSI_BUSY;
				return (-1);
			}
			xpt_free_path(tptr->owner);
			isp->isp_osinfo.tmflags[bus] &= ~TM_WILDCARD_ENABLED;
		}
	}

	/*
	 * Now check to see whether this bus needs to be
	 * enabled/disabled with respect to target mode.
	 */
	av = bus << 31;
	if (cel->enable && tm_on == 0) {
		av |= ENABLE_TARGET_FLAG;
		av = isp_control(isp, ISPCTL_TOGGLE_TMODE, &av);
		if (av) {
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			if (wildcard) {
				isp->isp_osinfo.tmflags[bus] &=
				    ~TM_WILDCARD_ENABLED;
				xpt_free_path(tptr->owner);
			}
			return (-1);
		}
		isp->isp_osinfo.tmflags[bus] |= TM_TMODE_ENABLED;
		xpt_print(ccb->ccb_h.path, "Target Mode Enabled\n");
	} else if (cel->enable == 0 && tm_on && wildcard) {
		if (are_any_luns_enabled(isp, bus)) {
			ccb->ccb_h.status = CAM_SCSI_BUSY;
			return (-1);
		}
		av = isp_control(isp, ISPCTL_TOGGLE_TMODE, &av);
		if (av) {
			ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
			return (-1);
		}
		isp->isp_osinfo.tmflags[bus] &= ~TM_TMODE_ENABLED;
		xpt_print(ccb->ccb_h.path, "Target Mode Disabled\n");
	}

	if (wildcard) {
		ccb->ccb_h.status = CAM_REQ_CMP;
		return (-1);
	}

	/*
	 * Find an empty slot
	 */
	for (seq = 0; seq < NLEACT; seq++) {
		if (isp->isp_osinfo.leact[seq] == 0) {
			break;
		}
	}
	if (seq >= NLEACT) {
		ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
		return (-1);
		
	}
	isp->isp_osinfo.leact[seq] = ccb;

	if (cel->enable) {
		ccb->ccb_h.status =
		    create_lun_state(isp, bus, ccb->ccb_h.path, &tptr);
		if (ccb->ccb_h.status != CAM_REQ_CMP) {
			isp->isp_osinfo.leact[seq] = 0;
			return (-1);
		}
	} else {
		tptr = get_lun_statep(isp, bus, lun);
		if (tptr == NULL) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			return (-1);
		}
	}

	if (cel->enable) {
		int c, n, ulun = lun;

		cmd = RQSTYPE_ENABLE_LUN;
		c = DFLT_CMND_CNT;
		n = DFLT_INOT_CNT;
		if (IS_FC(isp) && lun != 0) {
			cmd = RQSTYPE_MODIFY_LUN;
			n = 0;
			/*
		 	 * For SCC firmware, we only deal with setting
			 * (enabling or modifying) lun 0.
			 */
			ulun = 0;
		}
		if (isp_lun_cmd(isp, cmd, bus, tgt, ulun, c, n, seq+1) == 0) {
			rls_lun_statep(isp, tptr);
			ccb->ccb_h.status = CAM_REQ_INPROG;
			return (seq);
		}
	} else {
		int c, n, ulun = lun;

		cmd = -RQSTYPE_MODIFY_LUN;
		c = DFLT_CMND_CNT;
		n = DFLT_INOT_CNT;
		if (IS_FC(isp) && lun != 0) {
			n = 0;
			/*
		 	 * For SCC firmware, we only deal with setting
			 * (enabling or modifying) lun 0.
			 */
			ulun = 0;
		}
		if (isp_lun_cmd(isp, cmd, bus, tgt, ulun, c, n, seq+1) == 0) {
			rls_lun_statep(isp, tptr);
			ccb->ccb_h.status = CAM_REQ_INPROG;
			return (seq);
		}
	}
	rls_lun_statep(isp, tptr);
	xpt_print(ccb->ccb_h.path, "isp_lun_cmd failed\n");
	isp->isp_osinfo.leact[seq] = 0;
	ccb->ccb_h.status = CAM_REQ_CMP_ERR;
	return (-1);
}

static void
isp_ledone(ispsoftc_t *isp, lun_entry_t *lep)
{
	const char lfmt[] = "now %sabled for target mode\n";
	union ccb *ccb;
	uint32_t seq;
	tstate_t *tptr;
	int av;
	struct ccb_en_lun *cel;

	seq = lep->le_reserved - 1;
	if (seq >= NLEACT) {
		isp_prt(isp, ISP_LOGERR,
		    "seq out of range (%u) in isp_ledone", seq);
		return;
	}
	ccb = isp->isp_osinfo.leact[seq];
	if (ccb == 0) {
		isp_prt(isp, ISP_LOGERR,
		    "no ccb for seq %u in isp_ledone", seq);
		return;
	}
	cel = &ccb->cel;
	tptr = get_lun_statep(isp, XS_CHANNEL(ccb), XS_LUN(ccb));
	if (tptr == NULL) {
		xpt_print(ccb->ccb_h.path, "null tptr in isp_ledone\n");
		isp->isp_osinfo.leact[seq] = 0;
		return;
	}

	if (lep->le_status != LUN_OK) {
		xpt_print(ccb->ccb_h.path,
		    "ENABLE/MODIFY LUN returned 0x%x\n", lep->le_status);
err:
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		rls_lun_statep(isp, tptr);
		isp->isp_osinfo.leact[seq] = 0;
		xpt_done(ccb);
		return;
	} else {
		isp_prt(isp, ISP_LOGTDEBUG0,
		    "isp_ledone: ENABLE/MODIFY done okay");
	}


	if (cel->enable) {
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_print(ccb->ccb_h.path, lfmt, "en");
		rls_lun_statep(isp, tptr);
		isp->isp_osinfo.leact[seq] = 0;
		xpt_done(ccb);
		return;
	}

	if (lep->le_header.rqs_entry_type == RQSTYPE_MODIFY_LUN) {
		if (isp_lun_cmd(isp, -RQSTYPE_ENABLE_LUN, XS_CHANNEL(ccb),
		    XS_TGT(ccb), XS_LUN(ccb), 0, 0, seq+1)) {
			xpt_print(ccb->ccb_h.path,
			    "isp_ledone: isp_lun_cmd failed\n");
			goto err;
		}
		rls_lun_statep(isp, tptr);
		return;
	}

	xpt_print(ccb->ccb_h.path, lfmt, "dis");
	rls_lun_statep(isp, tptr);
	destroy_lun_state(isp, tptr);
	ccb->ccb_h.status = CAM_REQ_CMP;
	isp->isp_osinfo.leact[seq] = 0;
	xpt_done(ccb);
	if (are_any_luns_enabled(isp, XS_CHANNEL(ccb)) == 0) {
		int bus = XS_CHANNEL(ccb);
		av = bus << 31;
		av = isp_control(isp, ISPCTL_TOGGLE_TMODE, &av);
		if (av) {
			isp_prt(isp, ISP_LOGWARN,
			    "disable target mode on channel %d failed", bus);
		}
		isp->isp_osinfo.tmflags[bus] &= ~TM_TMODE_ENABLED;
	}
}


static cam_status
isp_abort_tgt_ccb(ispsoftc_t *isp, union ccb *ccb)
{
	tstate_t *tptr;
	struct ccb_hdr_slist *lp;
	struct ccb_hdr *curelm;
	int found, *ctr;
	union ccb *accb = ccb->cab.abort_ccb;

	xpt_print(ccb->ccb_h.path, "aborting ccb %p\n", accb);
	if (accb->ccb_h.target_id != CAM_TARGET_WILDCARD) {
		int badpath = 0;
		if (IS_FC(isp) && (accb->ccb_h.target_id != 
		    ((fcparam *) isp->isp_param)->isp_loopid)) {
			badpath = 1;
		} else if (IS_SCSI(isp) && (accb->ccb_h.target_id != 
		    ((sdparam *) isp->isp_param)->isp_initiator_id)) {
			badpath = 1;
		}
		if (badpath) {
			/*
			 * Being restrictive about target ids is really about
			 * making sure we're aborting for the right multi-tid
			 * path. This doesn't really make much sense at present.
			 */
#if	0
			return (CAM_PATH_INVALID);
#endif
		}
	}
	tptr = get_lun_statep(isp, XS_CHANNEL(ccb), accb->ccb_h.target_lun);
	if (tptr == NULL) {
		xpt_print(ccb->ccb_h.path, "can't get statep\n");
		return (CAM_PATH_INVALID);
	}
	if (accb->ccb_h.func_code == XPT_ACCEPT_TARGET_IO) {
		lp = &tptr->atios;
		ctr = &tptr->atio_count;
	} else if (accb->ccb_h.func_code == XPT_IMMED_NOTIFY) {
		lp = &tptr->inots;
		ctr = &tptr->inot_count;
	} else {
		rls_lun_statep(isp, tptr);
		xpt_print(ccb->ccb_h.path, "bad function code %d\n",
		    accb->ccb_h.func_code);
		return (CAM_UA_ABORT);
	}
	curelm = SLIST_FIRST(lp);
	found = 0;
	if (curelm == &accb->ccb_h) {
		found = 1;
		SLIST_REMOVE_HEAD(lp, sim_links.sle);
	} else {
		while(curelm != NULL) {
			struct ccb_hdr *nextelm;

			nextelm = SLIST_NEXT(curelm, sim_links.sle);
			if (nextelm == &accb->ccb_h) {
				found = 1;
				SLIST_NEXT(curelm, sim_links.sle) =
				    SLIST_NEXT(nextelm, sim_links.sle);
				break;
			}
			curelm = nextelm;
		}
	}
	rls_lun_statep(isp, tptr);
	if (found) {
		(*ctr)--;
		accb->ccb_h.status = CAM_REQ_ABORTED;
		xpt_done(accb);
		return (CAM_REQ_CMP);
	}
	xpt_print(ccb->ccb_h.path, "ccb %p not found\n", accb);
	return (CAM_PATH_INVALID);
}

static void
isp_target_start_ctio(ispsoftc_t *isp, union ccb *ccb)
{
	void *qe;
	struct ccb_scsiio *cso = &ccb->csio;
	uint32_t nxti, optr, handle;
	uint8_t local[QENTRY_LEN];


	if (isp_getrqentry(isp, &nxti, &optr, &qe)) {
		xpt_print(ccb->ccb_h.path,
		    "Request Queue Overflow in isp_target_start_ctio\n");
		XS_SETERR(ccb, CAM_REQUEUE_REQ);
		goto out;
	}
	memset(local, 0, QENTRY_LEN);

	/*
	 * We're either moving data or completing a command here.
	 */

	if (IS_FC(isp)) {
		atio_private_data_t *atp;
		ct2_entry_t *cto = (ct2_entry_t *) local;

		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO2;
		cto->ct_header.rqs_entry_count = 1;
		if (FCPARAM(isp)->isp_2klogin) {
			((ct2e_entry_t *)cto)->ct_iid = cso->init_id;
		} else {
			cto->ct_iid = cso->init_id;
			if (FCPARAM(isp)->isp_sccfw == 0) {
				cto->ct_lun = ccb->ccb_h.target_lun;
			}
		}

		atp = isp_get_atpd(isp, cso->tag_id);
		if (atp == NULL) {
			xpt_print(ccb->ccb_h.path,
			    "cannot find private data adjunct for tag %x\n",
			    cso->tag_id);
			XS_SETERR(ccb, CAM_REQ_CMP_ERR);
			goto out;
		}

		cto->ct_rxid = cso->tag_id;
		if (cso->dxfer_len == 0) {
			cto->ct_flags |= CT2_FLAG_MODE1 | CT2_NO_DATA;
			if (ccb->ccb_h.flags & CAM_SEND_STATUS) {
				cto->ct_flags |= CT2_SENDSTATUS;
				cto->rsp.m1.ct_scsi_status = cso->scsi_status;
				cto->ct_resid =
				    atp->orig_datalen - atp->bytes_xfered;
				if (cto->ct_resid < 0) {
					cto->rsp.m1.ct_scsi_status |=
					    CT2_DATA_OVER;
				} else if (cto->ct_resid > 0) {
					cto->rsp.m1.ct_scsi_status |=
					    CT2_DATA_UNDER;
				}
			}
			if ((ccb->ccb_h.flags & CAM_SEND_SENSE) != 0) {
				int m = min(cso->sense_len, MAXRESPLEN);
				memcpy(cto->rsp.m1.ct_resp,
				    &cso->sense_data, m);
				cto->rsp.m1.ct_senselen = m;
				cto->rsp.m1.ct_scsi_status |= CT2_SNSLEN_VALID;
			}
		} else {
			cto->ct_flags |= CT2_FLAG_MODE0;
			if ((cso->ccb_h.flags & CAM_DIR_MASK) == CAM_DIR_IN) {
				cto->ct_flags |= CT2_DATA_IN;
			} else {
				cto->ct_flags |= CT2_DATA_OUT;
			}
			cto->ct_reloff = atp->bytes_xfered;
			if ((ccb->ccb_h.flags & CAM_SEND_STATUS) != 0) {
				cto->ct_flags |= CT2_SENDSTATUS;
				cto->rsp.m0.ct_scsi_status = cso->scsi_status;
				cto->ct_resid =
				    atp->orig_datalen -
				    (atp->bytes_xfered + cso->dxfer_len);
				if (cto->ct_resid < 0) {
					cto->rsp.m0.ct_scsi_status |=
					    CT2_DATA_OVER;
				} else if (cto->ct_resid > 0) {
					cto->rsp.m0.ct_scsi_status |=
					    CT2_DATA_UNDER;
				}
			} else {
				atp->last_xframt = cso->dxfer_len;
			}
			/*
			 * If we're sending data and status back together,
			 * we can't also send back sense data as well.
			 */
			ccb->ccb_h.flags &= ~CAM_SEND_SENSE;
		}

		if (cto->ct_flags & CT2_SENDSTATUS) {
			isp_prt(isp, ISP_LOGTDEBUG0,
			    "CTIO2[%x] STATUS %x origd %u curd %u resid %u",
			    cto->ct_rxid, cso->scsi_status, atp->orig_datalen,
			    cso->dxfer_len, cto->ct_resid);
			cto->ct_flags |= CT2_CCINCR;
			atp->state = ATPD_STATE_LAST_CTIO;
		} else {
			atp->state = ATPD_STATE_CTIO;
		}
		cto->ct_timeout = 10;
	} else {
		ct_entry_t *cto = (ct_entry_t *) local;

		cto->ct_header.rqs_entry_type = RQSTYPE_CTIO;
		cto->ct_header.rqs_entry_count = 1;
		cto->ct_iid = cso->init_id;
		cto->ct_iid |= XS_CHANNEL(ccb) << 7;
		cto->ct_tgt = ccb->ccb_h.target_id;
		cto->ct_lun = ccb->ccb_h.target_lun;
		cto->ct_fwhandle = AT_GET_HANDLE(cso->tag_id);
		if (AT_HAS_TAG(cso->tag_id)) {
			cto->ct_tag_val = (uint8_t) AT_GET_TAG(cso->tag_id);
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
			cto->ct_resid = cso->resid;
			isp_prt(isp, ISP_LOGTDEBUG0,
			    "CTIO[%x] SCSI STATUS 0x%x resid %d tag_id %x",
			    cto->ct_fwhandle, cso->scsi_status, cso->resid,
			    cso->tag_id);
		}
		ccb->ccb_h.flags &= ~CAM_SEND_SENSE;
		cto->ct_timeout = 10;
	}

	if (isp_save_xs_tgt(isp, ccb, &handle)) {
		xpt_print(ccb->ccb_h.path,
		    "No XFLIST pointers for isp_target_start_ctio\n");
		XS_SETERR(ccb, CAM_REQUEUE_REQ);
		goto out;
	}


	/*
	 * Call the dma setup routines for this entry (and any subsequent
	 * CTIOs) if there's data to move, and then tell the f/w it's got
	 * new things to play with. As with isp_start's usage of DMA setup,
	 * any swizzling is done in the machine dependent layer. Because
	 * of this, we put the request onto the queue area first in native
	 * format.
	 */

	if (IS_FC(isp)) {
		ct2_entry_t *cto = (ct2_entry_t *) local;
		cto->ct_syshandle = handle;
	} else {
		ct_entry_t *cto = (ct_entry_t *) local;
		cto->ct_syshandle = handle;
	}

	switch (ISP_DMASETUP(isp, cso, (ispreq_t *) local, &nxti, optr)) {
	case CMD_QUEUED:
		ISP_ADD_REQUEST(isp, nxti);
		ccb->ccb_h.status |= CAM_SIM_QUEUED;
		return;

	case CMD_EAGAIN:
		XS_SETERR(ccb, CAM_REQUEUE_REQ);
		break;

	default:
		break;
	}
	isp_destroy_tgt_handle(isp, handle);

out:
	xpt_done(ccb);
}

static void
isp_refire_putback_atio(void *arg)
{
	int s = splcam();
	isp_target_putback_atio(arg);
	splx(s);
}

static void
isp_target_putback_atio(union ccb *ccb)
{
	ispsoftc_t *isp;
	struct ccb_scsiio *cso;
	uint32_t nxti, optr;
	void *qe;

	isp = XS_ISP(ccb);

	if (isp_getrqentry(isp, &nxti, &optr, &qe)) {
		xpt_print(ccb->ccb_h.path,
		    "isp_target_putback_atio: Request Queue Overflow\n"); 
		(void) timeout(isp_refire_putback_atio, ccb, 10);
		return;
	}
	memset(qe, 0, QENTRY_LEN);
	cso = &ccb->csio;
	if (IS_FC(isp)) {
		at2_entry_t local, *at = &local;
		MEMZERO(at, sizeof (at2_entry_t));
		at->at_header.rqs_entry_type = RQSTYPE_ATIO2;
		at->at_header.rqs_entry_count = 1;
		if (FCPARAM(isp)->isp_sccfw) {
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
		MEMZERO(at, sizeof (at_entry_t));
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
	ISP_TDQE(isp, "isp_target_putback_atio", (int) optr, qe);
	ISP_ADD_REQUEST(isp, nxti);
	isp_complete_ctio(ccb);
}

static void
isp_complete_ctio(union ccb *ccb)
{
	if ((ccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_INPROG) {
		ccb->ccb_h.status |= CAM_REQ_CMP;
	}
	ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	xpt_done(ccb);
}

/*
 * Handle ATIO stuff that the generic code can't.
 * This means handling CDBs.
 */

static int
isp_handle_platform_atio(ispsoftc_t *isp, at_entry_t *aep)
{
	tstate_t *tptr;
	int status, bus, iswildcard;
	struct ccb_accept_tio *atiop;

	/*
	 * The firmware status (except for the QLTM_SVALID bit)
	 * indicates why this ATIO was sent to us.
	 *
	 * If QLTM_SVALID is set, the firware has recommended Sense Data.
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
		return (0);
	}
	if ((status & ~QLTM_SVALID) != AT_CDB) {
		isp_prt(isp, ISP_LOGWARN, "bad atio (0x%x) leaked to platform",
		    status);
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return (0);
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
			return (0);
		}
		iswildcard = 1;
	} else {
		iswildcard = 0;
	}

	atiop = (struct ccb_accept_tio *) SLIST_FIRST(&tptr->atios);
	if (atiop == NULL) {
		/*
		 * Because we can't autofeed sense data back with
		 * a command for parallel SCSI, we can't give back
		 * a CHECK CONDITION. We'll give back a QUEUE FULL status
		 * instead. This works out okay because the only time we
		 * should, in fact, get this, is in the case that we've
		 * run out of ATIOS.
		 */
		xpt_print(tptr->owner,
		    "no ATIOS for lun %d from initiator %d on channel %d\n",
		    aep->at_lun, GET_IID_VAL(aep->at_iid), bus);
		if (aep->at_flags & AT_TQAE)
			isp_endcmd(isp, aep, SCSI_STATUS_QUEUE_FULL, 0);
		else
			isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		rls_lun_statep(isp, tptr);
		return (0);
	}
	SLIST_REMOVE_HEAD(&tptr->atios, sim_links.sle);
	tptr->atio_count--;
	isp_prt(isp, ISP_LOGTDEBUG0, "Take FREE ATIO lun %d, count now %d",
	    aep->at_lun, tptr->atio_count);
	if (iswildcard) {
		atiop->ccb_h.target_id = aep->at_tgt;
		atiop->ccb_h.target_lun = aep->at_lun;
	}
	if (aep->at_flags & AT_NODISC) {
		atiop->ccb_h.flags = CAM_DIS_DISCONNECT;
	} else {
		atiop->ccb_h.flags = 0;
	}

	if (status & QLTM_SVALID) {
		size_t amt = imin(QLTM_SENSELEN, sizeof (atiop->sense_data));
		atiop->sense_len = amt;
		MEMCPY(&atiop->sense_data, aep->at_sense, amt);
	} else {
		atiop->sense_len = 0;
	}

	atiop->init_id = GET_IID_VAL(aep->at_iid);
	atiop->cdb_len = aep->at_cdblen;
	MEMCPY(atiop->cdb_io.cdb_bytes, aep->at_cdb, aep->at_cdblen);
	atiop->ccb_h.status = CAM_CDB_RECVD;
	/*
	 * Construct a tag 'id' based upon tag value (which may be 0..255)
	 * and the handle (which we have to preserve).
	 */
	AT_MAKE_TAGID(atiop->tag_id, bus, device_get_unit(isp->isp_dev), aep);
	if (aep->at_flags & AT_TQAE) {
		atiop->tag_action = aep->at_tag_type;
		atiop->ccb_h.status |= CAM_TAG_ACTION_VALID;
	}
	xpt_done((union ccb*)atiop);
	isp_prt(isp, ISP_LOGTDEBUG0,
	    "ATIO[%x] CDB=0x%x bus %d iid%d->lun%d tag 0x%x ttype 0x%x %s",
	    aep->at_handle, aep->at_cdb[0] & 0xff, GET_BUS_VAL(aep->at_iid),
	    GET_IID_VAL(aep->at_iid), aep->at_lun, aep->at_tag_val & 0xff,
	    aep->at_tag_type, (aep->at_flags & AT_NODISC)?
	    "nondisc" : "disconnecting");
	rls_lun_statep(isp, tptr);
	return (0);
}

static int
isp_handle_platform_atio2(ispsoftc_t *isp, at2_entry_t *aep)
{
	lun_id_t lun;
	tstate_t *tptr;
	struct ccb_accept_tio *atiop;
	atio_private_data_t *atp;

	/*
	 * The firmware status (except for the QLTM_SVALID bit)
	 * indicates why this ATIO was sent to us.
	 *
	 * If QLTM_SVALID is set, the firware has recommended Sense Data.
	 */
	if ((aep->at_status & ~QLTM_SVALID) != AT_CDB) {
		isp_prt(isp, ISP_LOGWARN,
		    "bogus atio (0x%x) leaked to platform", aep->at_status);
		isp_endcmd(isp, aep, SCSI_STATUS_BUSY, 0);
		return (0);
	}

	if (FCPARAM(isp)->isp_sccfw) {
		lun = aep->at_scclun;
	} else {
		lun = aep->at_lun;
	}
	tptr = get_lun_statep(isp, 0, lun);
	if (tptr == NULL) {
		isp_prt(isp, ISP_LOGTDEBUG0,
		    "[0x%x] no state pointer for lun %d", aep->at_rxid, lun);
		tptr = get_lun_statep(isp, 0, CAM_LUN_WILDCARD);
		if (tptr == NULL) {
			isp_endcmd(isp, aep,
			    SCSI_STATUS_CHECK_COND | ECMD_SVALID |
			    (0x5 << 12) | (0x25 << 16), 0);
			return (0);
		}
	}

	atp = isp_get_atpd(isp, 0);
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
		xpt_print(tptr->owner,
		    "no %s for lun %d from initiator %d\n",
		    (atp == NULL && atiop == NULL)? "ATIO2s *or* ATPS" :
		    ((atp == NULL)? "ATPs" : "ATIO2s"), lun, aep->at_iid);
		rls_lun_statep(isp, tptr);
		isp_endcmd(isp, aep, SCSI_STATUS_QUEUE_FULL, 0);
		return (0);
	}
	atp->state = ATPD_STATE_ATIO;
	SLIST_REMOVE_HEAD(&tptr->atios, sim_links.sle);
	tptr->atio_count--;
	isp_prt(isp, ISP_LOGTDEBUG0, "Take FREE ATIO lun %d, count now %d",
	    lun, tptr->atio_count);

	if (tptr == &isp->isp_osinfo.tsdflt[0]) {
		atiop->ccb_h.target_id = FCPARAM(isp)->isp_loopid;
		atiop->ccb_h.target_lun = lun;
	}
	/*
	 * We don't get 'suggested' sense data as we do with SCSI cards.
	 */
	atiop->sense_len = 0;

	atiop->init_id = aep->at_iid;
	atiop->cdb_len = ATIO2_CDBLEN;
	MEMCPY(atiop->cdb_io.cdb_bytes, aep->at_cdb, ATIO2_CDBLEN);
	atiop->ccb_h.status = CAM_CDB_RECVD;
	atiop->tag_id = aep->at_rxid;
	switch (aep->at_taskflags & ATIO2_TC_ATTR_MASK) {
	case ATIO2_TC_ATTR_SIMPLEQ:
		atiop->tag_action = MSG_SIMPLE_Q_TAG;
		break;
        case ATIO2_TC_ATTR_HEADOFQ:
		atiop->tag_action = MSG_HEAD_OF_Q_TAG;
		break;
        case ATIO2_TC_ATTR_ORDERED:
		atiop->tag_action = MSG_ORDERED_Q_TAG;
		break;
        case ATIO2_TC_ATTR_ACAQ:		/* ?? */
	case ATIO2_TC_ATTR_UNTAGGED:
	default:
		atiop->tag_action = 0;
		break;
	}
	atiop->ccb_h.flags = CAM_TAG_ACTION_VALID;

	atp->tag = atiop->tag_id;
	atp->lun = lun;
	atp->orig_datalen = aep->at_datalen;
	atp->last_xframt = 0;
	atp->bytes_xfered = 0;
	atp->state = ATPD_STATE_CAM;
	xpt_done((union ccb*)atiop);

	isp_prt(isp, ISP_LOGTDEBUG0,
	    "ATIO2[%x] CDB=0x%x iid%d->lun%d tattr 0x%x datalen %u",
	    aep->at_rxid, aep->at_cdb[0] & 0xff, aep->at_iid,
	    lun, aep->at_taskflags, aep->at_datalen);
	rls_lun_statep(isp, tptr);
	return (0);
}

static int
isp_handle_platform_ctio(ispsoftc_t *isp, void *arg)
{
	union ccb *ccb;
	int sentstatus, ok, notify_cam, resid = 0;
	uint16_t tval;

	/*
	 * CTIO and CTIO2 are close enough....
	 */

	ccb = isp_find_xs_tgt(isp, ((ct_entry_t *)arg)->ct_syshandle);
	KASSERT((ccb != NULL), ("null ccb in isp_handle_platform_ctio"));
	isp_destroy_tgt_handle(isp, ((ct_entry_t *)arg)->ct_syshandle);

	if (IS_FC(isp)) {
		ct2_entry_t *ct = arg;
		atio_private_data_t *atp = isp_get_atpd(isp, ct->ct_rxid);
		if (atp == NULL) {
			isp_prt(isp, ISP_LOGERR,
			    "cannot find adjunct for %x after I/O",
			    ct->ct_rxid);
			return (0);
		}
		sentstatus = ct->ct_flags & CT2_SENDSTATUS;
		ok = (ct->ct_status & ~QLTM_SVALID) == CT_OK;
		if (ok && sentstatus && (ccb->ccb_h.flags & CAM_SEND_SENSE)) {
			ccb->ccb_h.status |= CAM_SENT_SENSE;
		}
		notify_cam = ct->ct_header.rqs_seqno & 0x1;
		if ((ct->ct_flags & CT2_DATAMASK) != CT2_NO_DATA) {
			resid = ct->ct_resid;
			atp->bytes_xfered += (atp->last_xframt - resid);
			atp->last_xframt = 0;
		}
		if (sentstatus || !ok) {
			atp->tag = 0;
		}
		isp_prt(isp, ok? ISP_LOGTDEBUG0 : ISP_LOGWARN,
		    "CTIO2[%x] sts 0x%x flg 0x%x sns %d resid %d %s",
		    ct->ct_rxid, ct->ct_status, ct->ct_flags,
		    (ccb->ccb_h.status & CAM_SENT_SENSE) != 0,
		    resid, sentstatus? "FIN" : "MID");
		tval = ct->ct_rxid;

		/* XXX: should really come after isp_complete_ctio */
		atp->state = ATPD_STATE_PDON;
	} else {
		ct_entry_t *ct = arg;
		sentstatus = ct->ct_flags & CT_SENDSTATUS;
		ok = (ct->ct_status  & ~QLTM_SVALID) == CT_OK;
		/*
		 * We *ought* to be able to get back to the original ATIO
		 * here, but for some reason this gets lost. It's just as
		 * well because it's squirrelled away as part of periph
		 * private data.
		 *
		 * We can live without it as long as we continue to use
		 * the auto-replenish feature for CTIOs.
		 */
		notify_cam = ct->ct_header.rqs_seqno & 0x1;
		if (ct->ct_status & QLTM_SVALID) {
			char *sp = (char *)ct;
			sp += CTIO_SENSE_OFFSET;
			ccb->csio.sense_len =
			    min(sizeof (ccb->csio.sense_data), QLTM_SENSELEN);
			MEMCPY(&ccb->csio.sense_data, sp, ccb->csio.sense_len);
			ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
		}
		if ((ct->ct_flags & CT_DATAMASK) != CT_NO_DATA) {
			resid = ct->ct_resid;
		}
		isp_prt(isp, ISP_LOGTDEBUG0,
		    "CTIO[%x] tag %x iid %d lun %d sts %x flg %x resid %d %s",
		    ct->ct_fwhandle, ct->ct_tag_val, ct->ct_iid, ct->ct_lun,
		    ct->ct_status, ct->ct_flags, resid,
		    sentstatus? "FIN" : "MID");
		tval = ct->ct_fwhandle;
	}
	ccb->csio.resid += resid;

	/*
	 * We're here either because intermediate data transfers are done
	 * and/or the final status CTIO (which may have joined with a
	 * Data Transfer) is done.
	 *
	 * In any case, for this platform, the upper layers figure out
	 * what to do next, so all we do here is collect status and
	 * pass information along. Any DMA handles have already been
	 * freed.
	 */
	if (notify_cam == 0) {
		isp_prt(isp, ISP_LOGTDEBUG0, "  INTER CTIO[0x%x] done", tval);
		return (0);
	}

	isp_prt(isp, ISP_LOGTDEBUG0, "%s CTIO[0x%x] done",
	    (sentstatus)? "  FINAL " : "MIDTERM ", tval);

	if (!ok) {
		isp_target_putback_atio(ccb);
	} else {
		isp_complete_ctio(ccb);

	}
	return (0);
}

static int
isp_handle_platform_notify_scsi(ispsoftc_t *isp, in_entry_t *inp)
{
	return (0);	/* XXXX */
}

static int
isp_handle_platform_notify_fc(ispsoftc_t *isp, in_fcentry_t *inp)
{

	switch (inp->in_status) {
	case IN_PORT_LOGOUT:
		isp_prt(isp, ISP_LOGWARN, "port logout of iid %d",
		   inp->in_iid);
		break;
	case IN_PORT_CHANGED:
		isp_prt(isp, ISP_LOGWARN, "port changed for iid %d",
		   inp->in_iid);
		break;
	case IN_GLOBAL_LOGO:
		isp_prt(isp, ISP_LOGINFO, "all ports logged out");
		break;
	case IN_ABORT_TASK:
	{
		atio_private_data_t *atp = isp_get_atpd(isp, inp->in_seqid);
		struct ccb_immed_notify *inot = NULL;

		if (atp) {
			tstate_t *tptr = get_lun_statep(isp, 0, atp->lun);
			if (tptr) {
				inot = (struct ccb_immed_notify *)
				    SLIST_FIRST(&tptr->inots);
				if (inot) {
					tptr->inot_count--;
					SLIST_REMOVE_HEAD(&tptr->inots,
					    sim_links.sle);
					isp_prt(isp, ISP_LOGTDEBUG0,
					    "Take FREE INOT count now %d",
					    tptr->inot_count);
				}
			}
			isp_prt(isp, ISP_LOGWARN,
			   "abort task RX_ID %x IID %d state %d",
			   inp->in_seqid, inp->in_iid, atp->state);
		} else {
			isp_prt(isp, ISP_LOGWARN,
			   "abort task RX_ID %x from iid %d, state unknown",
			   inp->in_seqid, inp->in_iid);
		}
		if (inot) {
			inot->initiator_id = inp->in_iid;
			inot->sense_len = 0;
			inot->message_args[0] = MSG_ABORT_TAG;
			inot->message_args[1] = inp->in_seqid & 0xff;
			inot->message_args[2] = (inp->in_seqid >> 8) & 0xff;
			inot->ccb_h.status = CAM_MESSAGE_RECV;
			xpt_done((union ccb *)inot);
		}
		break;
	}
	default:
		break;
	}
	return (0);
}
#endif

static void
isp_cam_async(void *cbarg, uint32_t code, struct cam_path *path, void *arg)
{
	struct cam_sim *sim;
	ispsoftc_t *isp;

	sim = (struct cam_sim *)cbarg;
	isp = (ispsoftc_t *) cam_sim_softc(sim);
	switch (code) {
	case AC_LOST_DEVICE:
		if (IS_SCSI(isp)) {
			uint16_t oflags, nflags;
			sdparam *sdp = isp->isp_param;
			int tgt;

			tgt = xpt_path_target_id(path);
			if (tgt >= 0) {
				sdp += cam_sim_bus(sim);
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
				isp->isp_update |= (1 << cam_sim_bus(sim));
				(void) isp_control(isp,
				    ISPCTL_UPDATE_PARAMS, NULL);
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


static int isp_watchdog_work(ispsoftc_t *, XS_T *);

static int
isp_watchdog_work(ispsoftc_t *isp, XS_T *xs)
{
	uint32_t handle;

	/*
	 * We've decided this command is dead. Make sure we're not trying
	 * to kill a command that's already dead by getting it's handle and
	 * and seeing whether it's still alive.
	 */
	handle = isp_find_handle(isp, xs);
	if (handle) {
		uint32_t isr;
		uint16_t sema, mbox;

		if (XS_CMD_DONE_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG1,
			    "watchdog found done cmd (handle 0x%x)", handle);
			return (1);;
		}

		if (XS_CMD_WDOG_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG2,
			    "recursive watchdog (handle 0x%x)", handle);
			return (1);
		}

		XS_CMD_S_WDOG(xs);
		if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
			isp_intr(isp, isr, sema, mbox);
		}
		if (XS_CMD_DONE_P(xs)) {
			isp_prt(isp, ISP_LOGDEBUG2,
			    "watchdog cleanup for handle 0x%x", handle);
			isp_free_pcmd(isp, (union ccb *)xs);
			xpt_done((union ccb *) xs);
		} else if (XS_CMD_GRACE_P(xs)) {
			/*
			 * Make sure the command is *really* dead before we
			 * release the handle (and DMA resources) for reuse.
			 */
			(void) isp_control(isp, ISPCTL_ABORT_CMD, xs);

			/*
			 * After this point, the comamnd is really dead.
			 */
			if (XS_XFRLEN(xs)) {
				ISP_DMAFREE(isp, xs, handle);
                	} 
			isp_destroy_handle(isp, handle);
			xpt_print(xs->ccb_h.path,
			    "watchdog timeout for handle 0x%x\n", handle);
			XS_SETERR(xs, CAM_CMD_TIMEOUT);
			XS_CMD_C_WDOG(xs);
			isp_done(xs);
		} else {
			XS_CMD_C_WDOG(xs);
			xs->ccb_h.timeout_ch = timeout(isp_watchdog, xs, hz);
			XS_CMD_S_GRACE(xs);
			isp->isp_sendmarker |= 1 << XS_CHANNEL(xs);
		}
		return (1);
	}
	return (0);
}

static void
isp_watchdog(void *arg)
{
	ispsoftc_t *isp;
	XS_T *xs = arg;
	int r;

	for (r = 0, isp = isplist; r && isp; isp = isp->isp_osinfo.next) {
		ISP_LOCK(isp);
		r = isp_watchdog_work(isp, xs);
		ISP_UNLOCK(isp);
	}
	if (isp == NULL) {
		printf("isp_watchdog: nobody had %p active\n", arg);
	}
}


#if __FreeBSD_version >= 600000  
static void
isp_make_here(ispsoftc_t *isp, int tgt)
{
	union ccb *ccb;
	/*
	 * Allocate a CCB, create a wildcard path for this bus,
	 * and schedule a rescan.
	 */
	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL) {
		isp_prt(isp, ISP_LOGWARN, "unable to alloc CCB for rescan");
		return;
	}
	if (xpt_create_path(&ccb->ccb_h.path, xpt_periph,
	    cam_sim_path(isp->isp_sim), tgt, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
		isp_prt(isp, ISP_LOGWARN, "unable to create path for rescan");
		xpt_free_ccb(ccb);
		return;
	}
	xpt_rescan(ccb);
}

static void
isp_make_gone(ispsoftc_t *isp, int tgt)
{
	struct cam_path *tp;
	if (xpt_create_path(&tp, NULL, cam_sim_path(isp->isp_sim), tgt,
	    CAM_LUN_WILDCARD) == CAM_REQ_CMP) {
		xpt_async(AC_LOST_DEVICE, tp, NULL);
		xpt_free_path(tp);
	}
}
#else
#define	isp_make_here(isp, tgt)	do { ; } while (0)
#define	isp_make_gone(isp, tgt)	do { ; } while (0)
#endif


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
	ispsoftc_t *isp = arg;
	fcportdb_t *lp;
	int dbidx, tgt, more_to_do = 0;

	ISP_LOCK(isp);
	isp_prt(isp, ISP_LOGDEBUG0, "GDT timer expired");
	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &FCPARAM(isp)->portdb[dbidx];

		if (lp->state != FC_PORTDB_STATE_ZOMBIE) {
			continue;
		}
		if (lp->ini_map_idx == 0) {
			continue;
		}
		if (lp->new_reserved == 0) {
			continue;
		}
		lp->new_reserved -= 1;
		if (lp->new_reserved != 0) {
			more_to_do++;
			continue;
		}
		tgt = lp->ini_map_idx - 1;
		FCPARAM(isp)->isp_ini_map[tgt] = 0;
		lp->ini_map_idx = 0;
		lp->state = FC_PORTDB_STATE_NIL;
		isp_prt(isp, ISP_LOGCONFIG, prom3, lp->portid, tgt,
		    "Gone Device Timeout");
		isp_make_gone(isp, tgt);
	}
	if (more_to_do) {
		isp->isp_osinfo.gdt_running = 1;
		callout_reset(&isp->isp_osinfo.gdt, hz, isp_gdt, isp);
	} else {
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		    "stopping Gone Device Timer");
		isp->isp_osinfo.gdt_running = 0;
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
	ispsoftc_t *isp = arg;
	fcportdb_t *lp;
	int dbidx, tgt;

	ISP_LOCK(isp);

	isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0, "Loop Down Timer expired");

	/*
	 * Notify to the OS all targets who we now consider have departed.
	 */
	for (dbidx = 0; dbidx < MAX_FC_TARG; dbidx++) {
		lp = &FCPARAM(isp)->portdb[dbidx];

		if (lp->state != FC_PORTDB_STATE_PROBATIONAL) {
			continue;
		}
		if (lp->ini_map_idx == 0) {
			continue;
		}

		/*
		 * XXX: CLEAN UP AND COMPLETE ANY PENDING COMMANDS FIRST!
		 */

		/*
		 * Mark that we've announced that this device is gone....
		 */
		lp->reserved = 1;

		/*
		 * but *don't* change the state of the entry. Just clear
		 * any target id stuff and announce to CAM that the
		 * device is gone. This way any necessary PLOGO stuff
		 * will happen when loop comes back up.
		 */

		tgt = lp->ini_map_idx - 1;
		FCPARAM(isp)->isp_ini_map[tgt] = 0;
		lp->ini_map_idx = 0;
		isp_prt(isp, ISP_LOGCONFIG, prom3, lp->portid, tgt,
		    "Loop Down Timeout");
		isp_make_gone(isp, tgt);
	}

	/*
	 * The loop down timer has expired. Wake up the kthread
	 * to notice that fact (or make it false).
	 */
	isp->isp_osinfo.loop_down_time = isp->isp_osinfo.loop_down_limit+1;
	wakeup(ISP_KT_WCHAN(isp));
	ISP_UNLOCK(isp);
}

static void
isp_kthread(void *arg)
{
	ispsoftc_t *isp = arg;
	int slp = 0;
#if __FreeBSD_version < 500000  
        int s = splcam();
#elif __FreeBSD_version < 700037
	mtx_lock(&Giant);
#else
	mtx_lock(&isp->isp_osinfo.lock);
#endif
	/*
	 * The first loop is for our usage where we have yet to have
	 * gotten good fibre channel state.
	 */
	for (;;) {
		int wasfrozen, lb, lim;

		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		    "isp_kthread: checking FC state");
		isp->isp_osinfo.mbox_sleep_ok = 1;
		lb = isp_fc_runstate(isp, 250000);
		isp->isp_osinfo.mbox_sleep_ok = 0;
		if (lb) {
			/*
			 * Increment loop down time by the last sleep interval
			 */
			isp->isp_osinfo.loop_down_time += slp;

			if (lb < 0) {
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "kthread: FC loop not up (down count %d)",
				    isp->isp_osinfo.loop_down_time);
			} else {
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "kthread: FC got to %d (down count %d)",
				    lb, isp->isp_osinfo.loop_down_time);
			}


			/*
			 * If we've never seen loop up and we've waited longer
			 * than quickboot time, or we've seen loop up but we've
			 * waited longer than loop_down_limit, give up and go
			 * to sleep until loop comes up.
			 */
			if (FCPARAM(isp)->loop_seen_once == 0) {
				lim = isp_quickboot_time;
			} else {
				lim = isp->isp_osinfo.loop_down_limit;
			}
			if (isp->isp_osinfo.loop_down_time >= lim) {
				isp_freeze_loopdown(isp, "loop limit hit");
				slp = 0;
			} else if (isp->isp_osinfo.loop_down_time < 10) {
				slp = 1;
			} else if (isp->isp_osinfo.loop_down_time < 30) {
				slp = 5;
			} else if (isp->isp_osinfo.loop_down_time < 60) {
				slp = 10;
			} else if (isp->isp_osinfo.loop_down_time < 120) {
				slp = 20;
			} else {
				slp = 30;
			}

		} else {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "isp_kthread: FC state OK");
			isp->isp_osinfo.loop_down_time = 0;
			slp = 0;
		}

		/*
		 * If we'd frozen the simq, unfreeze it now so that CAM
		 * can start sending us commands. If the FC state isn't
		 * okay yet, they'll hit that in isp_start which will
		 * freeze the queue again.
		 */
		wasfrozen = isp->isp_osinfo.simqfrozen & SIMQFRZ_LOOPDOWN;
		isp->isp_osinfo.simqfrozen &= ~SIMQFRZ_LOOPDOWN;
		if (wasfrozen && isp->isp_osinfo.simqfrozen == 0) {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "isp_kthread: releasing simq");
			xpt_release_simq(isp->isp_sim, 1);
		}
		isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
		    "isp_kthread: sleep time %d", slp);
#if __FreeBSD_version < 700037
		tsleep(ISP_KT_WCHAN(isp), PRIBIO, "ispf", slp * hz);
#else
		msleep(ISP_KT_WCHAN(isp), &isp->isp_osinfo.lock,
		    PRIBIO, "ispf", slp * hz);
#endif
		/*
		 * If slp is zero, we're waking up for the first time after
		 * things have been okay. In this case, we set a deferral state
		 * for all commands and delay hysteresis seconds before starting
		 * the FC state evaluation. This gives the loop/fabric a chance
		 * to settle.
		 */
		if (slp == 0 && isp->isp_osinfo.hysteresis) {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			    "isp_kthread: sleep hysteresis tick time %d",
			    isp->isp_osinfo.hysteresis * hz);
#if __FreeBSD_version < 700037
			(void) tsleep(&isp_fabric_hysteresis, PRIBIO, "ispT",
			    (isp->isp_osinfo.hysteresis * hz));
#else
			(void) msleep(&isp_fabric_hysteresis,
			    &isp->isp_osinfo.lock, PRIBIO, "ispT",
			    (isp->isp_osinfo.hysteresis * hz));
#endif
		}
	}
#if __FreeBSD_version < 500000  
	splx(s);
#elif __FreeBSD_version < 700037
	mtx_unlock(&Giant);
#else
	mtx_unlock(&isp->isp_osinfo.lock);
#endif
}

#if __FreeBSD_version < 500000  
static void isp_action_wrk(struct cam_sim *, union ccb *);
static void
isp_action(struct cam_sim *sim, union ccb *ccb)
{
	ispsoftc_t *isp = (ispsoftc_t *)cam_sim_softc(sim);
	ISP_LOCK(isp);
	isp_action_wrk(sim, ccb);
	ISP_UNLOCK(isp);
}
#define	isp_action isp_action_wrk
#endif

static void
isp_action(struct cam_sim *sim, union ccb *ccb)
{
	int bus, tgt, ts, error, lim;
	ispsoftc_t *isp;
	struct ccb_trans_settings *cts;

	CAM_DEBUG(ccb->ccb_h.path, CAM_DEBUG_TRACE, ("isp_action\n"));
	
	isp = (ispsoftc_t *)cam_sim_softc(sim);
	if (isp->isp_state != ISP_RUNSTATE &&
	    ccb->ccb_h.func_code == XPT_SCSI_IO) {
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
			cam_release_devq(ccb->ccb_h.path,
			    RELSIM_RELEASE_AFTER_TIMEOUT, 0, 250, 0);
			xpt_done(ccb);
			break;
		}
		error = isp_start((XS_T *) ccb);
		switch (error) {
		case CMD_QUEUED:
			XS_CMD_S_CLEAR(ccb);
			ccb->ccb_h.status |= CAM_SIM_QUEUED;
			if (ccb->ccb_h.timeout == CAM_TIME_INFINITY) {
				break;
			}
			ts = ccb->ccb_h.timeout;
			if (ts == CAM_TIME_DEFAULT) {
				ts = 60*1000;
			}
			ts = isp_mstohz(ts);
			callout_reset(&PISP_PCMD(ccb)->wdog, ts,
			    isp_watchdog, ccb);
			break;
		case CMD_RQLATER:
			/*
			 * Handle initial and subsequent loop down cases
			 */
			if (FCPARAM(isp)->loop_seen_once == 0) {
				lim = isp_quickboot_time;
			} else {
				lim = isp->isp_osinfo.loop_down_limit;
			}
			if (isp->isp_osinfo.loop_down_time >= lim) {
				isp_prt(isp, ISP_LOGDEBUG0,
				    "%d.%d downtime (%d) > lim (%d)",
				    XS_TGT(ccb), XS_LUN(ccb),
				    isp->isp_osinfo.loop_down_time, lim);
				ccb->ccb_h.status =
				    CAM_SEL_TIMEOUT|CAM_DEV_QFRZN;
				xpt_freeze_devq(ccb->ccb_h.path, 1);
				isp_free_pcmd(isp, ccb);
				xpt_done(ccb);
				break;
			}
			isp_prt(isp, ISP_LOGDEBUG0,
			    "%d.%d retry later", XS_TGT(ccb), XS_LUN(ccb));
			/*
			 * Otherwise, retry in a while.
			 */
			cam_freeze_devq(ccb->ccb_h.path);
			cam_release_devq(ccb->ccb_h.path,
			    RELSIM_RELEASE_AFTER_TIMEOUT, 0, 1000, 0);
			XS_SETERR(ccb, CAM_REQUEUE_REQ);
			isp_free_pcmd(isp, ccb);
			xpt_done(ccb);
			break;
		case CMD_EAGAIN:
			XS_SETERR(ccb, CAM_REQUEUE_REQ);
			isp_free_pcmd(isp, ccb);
			xpt_done(ccb);
			break;
		case CMD_COMPLETE:
			isp_done((struct ccb_scsiio *) ccb);
			break;
		default:
			isp_prt(isp, ISP_LOGERR,
			    "What's this? 0x%x at %d in file %s",
			    error, __LINE__, __FILE__);
			XS_SETERR(ccb, CAM_REQ_CMP_ERR);
			isp_free_pcmd(isp, ccb);
			xpt_done(ccb);
		}
		break;

#ifdef	ISP_TARGET_MODE
	case XPT_EN_LUN:		/* Enable LUN as a target */
	{
		int seq, i;
		seq = isp_en_lun(isp, ccb);
		if (seq < 0) {
			xpt_done(ccb);
			break;
		}
		for (i = 0; isp->isp_osinfo.leact[seq] && i < 30 * 1000; i++) {
			uint32_t isr;
			uint16_t sema, mbox;
			if (ISP_READ_ISR(isp, &isr, &sema, &mbox)) {
				isp_intr(isp, isr, sema, mbox);
			}
			DELAY(1000);
		}
		break;
	}
	case XPT_NOTIFY_ACK:		/* recycle notify ack */
	case XPT_IMMED_NOTIFY:		/* Add Immediate Notify Resource */
	case XPT_ACCEPT_TARGET_IO:	/* Add Accept Target IO Resource */
	{
		tstate_t *tptr =
		    get_lun_statep(isp, XS_CHANNEL(ccb), ccb->ccb_h.target_lun);
		if (tptr == NULL) {
			ccb->ccb_h.status = CAM_LUN_INVALID;
			xpt_done(ccb);
			break;
		}
		ccb->ccb_h.sim_priv.entries[0].field = 0;
		ccb->ccb_h.sim_priv.entries[1].ptr = isp;
		ccb->ccb_h.flags = 0;

		if (ccb->ccb_h.func_code == XPT_ACCEPT_TARGET_IO) {
			/*
			 * Note that the command itself may not be done-
			 * it may not even have had the first CTIO sent.
			 */
			tptr->atio_count++;
			isp_prt(isp, ISP_LOGTDEBUG0,
			    "Put FREE ATIO, lun %d, count now %d",
			    ccb->ccb_h.target_lun, tptr->atio_count);
			SLIST_INSERT_HEAD(&tptr->atios, &ccb->ccb_h,
			    sim_links.sle);
		} else if (ccb->ccb_h.func_code == XPT_IMMED_NOTIFY) {
			tptr->inot_count++;
			isp_prt(isp, ISP_LOGTDEBUG0,
			    "Put FREE INOT, lun %d, count now %d",
			    ccb->ccb_h.target_lun, tptr->inot_count);
			SLIST_INSERT_HEAD(&tptr->inots, &ccb->ccb_h,
			    sim_links.sle);
		} else {
			isp_prt(isp, ISP_LOGWARN, "Got Notify ACK");;
		}
		rls_lun_statep(isp, tptr);
		ccb->ccb_h.status = CAM_REQ_INPROG;
		break;
	}
	case XPT_CONT_TARGET_IO:
	{
		isp_target_start_ctio(isp, ccb);
		break;
	}
#endif
	case XPT_RESET_DEV:		/* BDR the specified SCSI device */

		bus = cam_sim_bus(xpt_path_sim(ccb->ccb_h.path));
		tgt = ccb->ccb_h.target_id;
		tgt |= (bus << 16);

		error = isp_control(isp, ISPCTL_RESET_DEV, &tgt);
		if (error) {
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		} else {
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
		case XPT_IMMED_NOTIFY:
        		ccb->ccb_h.status = isp_abort_tgt_ccb(isp, ccb);
			break;
		case XPT_CONT_TARGET_IO:
			isp_prt(isp, ISP_LOGERR, "cannot abort CTIOs yet");
			ccb->ccb_h.status = CAM_UA_ABORT;
			break;
#endif
		case XPT_SCSI_IO:
			error = isp_control(isp, ISPCTL_ABORT_CMD, ccb);
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
		xpt_done(ccb);
		break;
	}
#ifdef	CAM_NEW_TRAN_CODE
#define	IS_CURRENT_SETTINGS(c)	(c->type == CTS_TYPE_CURRENT_SETTINGS)
#else
#define	IS_CURRENT_SETTINGS(c)	(c->flags & CCB_TRANS_CURRENT_SETTINGS)
#endif
	case XPT_SET_TRAN_SETTINGS:	/* Nexus Settings */
		cts = &ccb->cts;
		if (!IS_CURRENT_SETTINGS(cts)) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
		tgt = cts->ccb_h.target_id;
		if (IS_SCSI(isp)) {
#ifndef	CAM_NEW_TRAN_CODE
			sdparam *sdp = isp->isp_param;
			uint16_t *dptr;

			bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));

			sdp += bus;
			/*
			 * We always update (internally) from goal_flags
			 * so any request to change settings just gets
			 * vectored to that location.
			 */
			dptr = &sdp->isp_devparam[tgt].goal_flags;

			/*
			 * Note that these operations affect the
			 * the goal flags (goal_flags)- not
			 * the current state flags. Then we mark
			 * things so that the next operation to
			 * this HBA will cause the update to occur.
			 */
			if (cts->valid & CCB_TRANS_DISC_VALID) {
				if ((cts->flags & CCB_TRANS_DISC_ENB) != 0) {
					*dptr |= DPARM_DISC;
				} else {
					*dptr &= ~DPARM_DISC;
				}
			}
			if (cts->valid & CCB_TRANS_TQ_VALID) {
				if ((cts->flags & CCB_TRANS_TAG_ENB) != 0) {
					*dptr |= DPARM_TQING;
				} else {
					*dptr &= ~DPARM_TQING;
				}
			}
			if (cts->valid & CCB_TRANS_BUS_WIDTH_VALID) {
				switch (cts->bus_width) {
				case MSG_EXT_WDTR_BUS_16_BIT:
					*dptr |= DPARM_WIDE;
					break;
				default:
					*dptr &= ~DPARM_WIDE;
				}
			}
			/*
			 * Any SYNC RATE of nonzero and SYNC_OFFSET
			 * of nonzero will cause us to go to the
			 * selected (from NVRAM) maximum value for
			 * this device. At a later point, we'll
			 * allow finer control.
			 */
			if ((cts->valid & CCB_TRANS_SYNC_RATE_VALID) &&
			    (cts->valid & CCB_TRANS_SYNC_OFFSET_VALID) &&
			    (cts->sync_offset > 0)) {
				*dptr |= DPARM_SYNC;
			} else {
				*dptr &= ~DPARM_SYNC;
			}
			*dptr |= DPARM_SAFE_DFLT;
#else
			struct ccb_trans_settings_scsi *scsi =
			    &cts->proto_specific.scsi;
			struct ccb_trans_settings_spi *spi =
			    &cts->xport_specific.spi;
			sdparam *sdp = isp->isp_param;
			uint16_t *dptr;

			if (spi->valid == 0 && scsi->valid == 0) {
				ccb->ccb_h.status = CAM_REQ_CMP;
				xpt_done(ccb);
				break;
			}
				
			bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));
			sdp += bus;
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
			if ((spi->valid & CTS_SPI_VALID_SYNC_OFFSET) &&
			    (spi->valid & CTS_SPI_VALID_SYNC_RATE) &&
			    (spi->sync_period && spi->sync_offset)) {
				*dptr |= DPARM_SYNC;
				/*
				 * XXX: CHECK FOR LEGALITY
				 */
				sdp->isp_devparam[tgt].goal_period =
				    spi->sync_period;
				sdp->isp_devparam[tgt].goal_offset =
				    spi->sync_offset;
			} else {
				*dptr &= ~DPARM_SYNC;
			}
#endif
			isp_prt(isp, ISP_LOGDEBUG0,
			    "SET (%d.%d.%d) to flags %x off %x per %x",
			    bus, tgt, cts->ccb_h.target_lun,
			    sdp->isp_devparam[tgt].goal_flags,
			    sdp->isp_devparam[tgt].goal_offset,
			    sdp->isp_devparam[tgt].goal_period);
			sdp->isp_devparam[tgt].dev_update = 1;
			isp->isp_update |= (1 << bus);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	case XPT_GET_TRAN_SETTINGS:
		cts = &ccb->cts;
		tgt = cts->ccb_h.target_id;
		if (IS_FC(isp)) {
#ifndef	CAM_NEW_TRAN_CODE
			/*
			 * a lot of normal SCSI things don't make sense.
			 */
			cts->flags = CCB_TRANS_TAG_ENB | CCB_TRANS_DISC_ENB;
			cts->valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
			/*
			 * How do you measure the width of a high
			 * speed serial bus? Well, in bytes.
			 *
			 * Offset and period make no sense, though, so we set
			 * (above) a 'base' transfer speed to be gigabit.
			 */
			cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
#else
			fcparam *fcp = isp->isp_param;
			struct ccb_trans_settings_scsi *scsi =
			    &cts->proto_specific.scsi;
			struct ccb_trans_settings_fc *fc =
			    &cts->xport_specific.fc;

			cts->protocol = PROTO_SCSI;
			cts->protocol_version = SCSI_REV_2;
			cts->transport = XPORT_FC;
			cts->transport_version = 0;

			scsi->valid = CTS_SCSI_VALID_TQ;
			scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;
			fc->valid = CTS_FC_VALID_SPEED;
			if (fcp->isp_gbspeed == 2) {
				fc->bitrate = 200000;
			} else {
				fc->bitrate = 100000;
			}
			if (tgt > 0 && tgt < MAX_FC_TARG) {
				fcportdb_t *lp = &fcp->portdb[tgt];
				fc->wwnn = lp->node_wwn;
				fc->wwpn = lp->port_wwn;
				fc->port = lp->portid;
				fc->valid |= CTS_FC_VALID_WWNN |
				    CTS_FC_VALID_WWPN | CTS_FC_VALID_PORT;
			}
#endif
		} else {
#ifdef	CAM_NEW_TRAN_CODE
			struct ccb_trans_settings_scsi *scsi =
			    &cts->proto_specific.scsi;
			struct ccb_trans_settings_spi *spi =
			    &cts->xport_specific.spi;
#endif
			sdparam *sdp = isp->isp_param;
			int bus = cam_sim_bus(xpt_path_sim(cts->ccb_h.path));
			uint16_t dval, pval, oval;

			sdp += bus;

			if (IS_CURRENT_SETTINGS(cts)) {
				sdp->isp_devparam[tgt].dev_refresh = 1;
				isp->isp_update |= (1 << bus);
				(void) isp_control(isp, ISPCTL_UPDATE_PARAMS,
				    NULL);
				dval = sdp->isp_devparam[tgt].actv_flags;
				oval = sdp->isp_devparam[tgt].actv_offset;
				pval = sdp->isp_devparam[tgt].actv_period;
			} else {
				dval = sdp->isp_devparam[tgt].nvrm_flags;
				oval = sdp->isp_devparam[tgt].nvrm_offset;
				pval = sdp->isp_devparam[tgt].nvrm_period;
			}

#ifndef	CAM_NEW_TRAN_CODE
			cts->flags &= ~(CCB_TRANS_DISC_ENB|CCB_TRANS_TAG_ENB);

			if (dval & DPARM_DISC) {
				cts->flags |= CCB_TRANS_DISC_ENB;
			}
			if (dval & DPARM_TQING) {
				cts->flags |= CCB_TRANS_TAG_ENB;
			}
			if (dval & DPARM_WIDE) {
				cts->bus_width = MSG_EXT_WDTR_BUS_16_BIT;
			} else {
				cts->bus_width = MSG_EXT_WDTR_BUS_8_BIT;
			}
			cts->valid = CCB_TRANS_BUS_WIDTH_VALID |
			    CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;

			if ((dval & DPARM_SYNC) && oval != 0) {
				cts->sync_period = pval;
				cts->sync_offset = oval;
				cts->valid |=
				    CCB_TRANS_SYNC_RATE_VALID |
				    CCB_TRANS_SYNC_OFFSET_VALID;
			}
#else
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
#endif
			isp_prt(isp, ISP_LOGDEBUG0,
			    "GET %s (%d.%d.%d) to flags %x off %x per %x",
			    IS_CURRENT_SETTINGS(cts)? "ACTIVE" : "NVRAM",
			    bus, tgt, cts->ccb_h.target_lun, dval, oval, pval);
		}
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;

	case XPT_CALC_GEOMETRY:
#if __FreeBSD_version < 500000  
	{
		struct ccb_calc_geometry *ccg;
		u_int32_t secs_per_cylinder;
		u_int32_t size_mb;

		ccg = &ccb->ccg;
		if (ccg->block_size == 0) {
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			break;
		}
		size_mb = ccg->volume_size /((1024L * 1024L) / ccg->block_size);
		if (size_mb > 1024) {
			ccg->heads = 255;
			ccg->secs_per_track = 63;
		} else {
			ccg->heads = 64;
			ccg->secs_per_track = 32;
		}
		secs_per_cylinder = ccg->heads * ccg->secs_per_track;
		ccg->cylinders = ccg->volume_size / secs_per_cylinder;
		ccb->ccb_h.status = CAM_REQ_CMP;
		xpt_done(ccb);
		break;
	}
#else
	{
		cam_calc_geometry(&ccb->ccg, /*extended*/1);
		xpt_done(ccb);
		break;
	}
#endif
	case XPT_RESET_BUS:		/* Reset the specified bus */
		bus = cam_sim_bus(sim);
		error = isp_control(isp, ISPCTL_RESET_BUS, &bus);
		if (error)
			ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		else {
			if (bootverbose) {
				xpt_print(ccb->ccb_h.path, "reset bus\n");
			}
			if (cam_sim_bus(sim) && isp->isp_path2 != NULL)
				xpt_async(AC_BUS_RESET, isp->isp_path2, NULL);
			else if (isp->isp_path != NULL)
				xpt_async(AC_BUS_RESET, isp->isp_path, NULL);
			ccb->ccb_h.status = CAM_REQ_CMP;
		}
		xpt_done(ccb);
		break;

	case XPT_TERM_IO:		/* Terminate the I/O process */
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		break;

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
		if (IS_FC(isp)) {
			cpi->hba_misc = PIM_NOBUSRESET;
			/*
			 * Because our loop ID can shift from time to time,
			 * make our initiator ID out of range of our bus.
			 */
			cpi->initiator_id = cpi->max_target + 1;

			/*
			 * Set base transfer capabilities for Fibre Channel.
			 * Technically not correct because we don't know
			 * what media we're running on top of- but we'll
			 * look good if we always say 100MB/s.
			 */
			if (FCPARAM(isp)->isp_gbspeed == 2)
				cpi->base_transfer_speed = 200000;
			else
				cpi->base_transfer_speed = 100000;
			cpi->hba_inquiry = PI_TAG_ABLE;
#ifdef	CAM_NEW_TRAN_CODE
			cpi->transport = XPORT_FC;
			cpi->transport_version = 0;
#endif
		} else {
			sdparam *sdp = isp->isp_param;
			sdp += cam_sim_bus(xpt_path_sim(cpi->ccb_h.path));
			cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
			cpi->hba_misc = 0;
			cpi->initiator_id = sdp->isp_initiator_id;
			cpi->base_transfer_speed = 3300;
#ifdef	CAM_NEW_TRAN_CODE
			cpi->transport = XPORT_SPI;
			cpi->transport_version = 2;
#endif
		}
#ifdef	CAM_NEW_TRAN_CODE
		cpi->protocol = PROTO_SCSI;
		cpi->protocol_version = SCSI_REV_2;
#endif
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
isp_done(struct ccb_scsiio *sccb)
{
	ispsoftc_t *isp = XS_ISP(sccb);

	if (XS_NOERR(sccb))
		XS_SETERR(sccb, CAM_REQ_CMP);

	if ((sccb->ccb_h.status & CAM_STATUS_MASK) == CAM_REQ_CMP &&
	    (sccb->scsi_status != SCSI_STATUS_OK)) {
		sccb->ccb_h.status &= ~CAM_STATUS_MASK;
		if ((sccb->scsi_status == SCSI_STATUS_CHECK_COND) && 
		    (sccb->ccb_h.status & CAM_AUTOSNS_VALID) == 0) {
			sccb->ccb_h.status |= CAM_AUTOSENSE_FAIL;
		} else {
			sccb->ccb_h.status |= CAM_SCSI_STATUS_ERROR;
		}
	}

	sccb->ccb_h.status &= ~CAM_SIM_QUEUED;
	if ((sccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		isp_prt(isp, ISP_LOGDEBUG0,
		    "target %d lun %d CAM status 0x%x SCSI status 0x%x",
		    XS_TGT(sccb), XS_LUN(sccb), sccb->ccb_h.status,
		    sccb->scsi_status);
		if ((sccb->ccb_h.status & CAM_DEV_QFRZN) == 0) {
			sccb->ccb_h.status |= CAM_DEV_QFRZN;
			xpt_freeze_devq(sccb->ccb_h.path, 1);
		}
	}

	if ((CAM_DEBUGGED(sccb->ccb_h.path, ISPDDB)) &&
	    (sccb->ccb_h.status & CAM_STATUS_MASK) != CAM_REQ_CMP) {
		xpt_print(sccb->ccb_h.path,
		    "cam completion status 0x%x\n", sccb->ccb_h.status);
	}

	XS_CMD_S_DONE(sccb);
	if (XS_CMD_WDOG_P(sccb) == 0) {
		untimeout(isp_watchdog, sccb, sccb->ccb_h.timeout_ch);
		if (XS_CMD_GRACE_P(sccb)) {
			isp_prt(isp, ISP_LOGDEBUG2,
			    "finished command on borrowed time");
		}
		XS_CMD_S_CLEAR(sccb);
		isp_free_pcmd(isp, (union ccb *) sccb);
		xpt_done((union ccb *) sccb);
	}
}

int
isp_async(ispsoftc_t *isp, ispasync_t cmd, void *arg)
{
	int bus, rv = 0;
	static const char prom[] =
	    "PortID 0x%06x handle 0x%x role %s %s\n"
	    "      WWNN 0x%08x%08x WWPN 0x%08x%08x";
	static const char prom2[] =
	    "PortID 0x%06x handle 0x%x role %s %s tgt %u\n"
	    "      WWNN 0x%08x%08x WWPN 0x%08x%08x";
	char *msg = NULL;
	target_id_t tgt;
	fcportdb_t *lp;
	struct cam_path *tmppath;

	switch (cmd) {
	case ISPASYNC_NEW_TGT_PARAMS:
	{
#ifdef	CAM_NEW_TRAN_CODE
		struct ccb_trans_settings_scsi *scsi;
		struct ccb_trans_settings_spi *spi;
#endif
		int flags, tgt;
		sdparam *sdp = isp->isp_param;
		struct ccb_trans_settings cts;

		memset(&cts, 0, sizeof (struct ccb_trans_settings));

		tgt = *((int *)arg);
		bus = (tgt >> 16) & 0xffff;
		tgt &= 0xffff;
		sdp += bus;
		if (xpt_create_path(&tmppath, NULL,
		    cam_sim_path(bus? isp->isp_sim2 : isp->isp_sim),
		    tgt, CAM_LUN_WILDCARD) != CAM_REQ_CMP) {
			isp_prt(isp, ISP_LOGWARN,
			    "isp_async cannot make temp path for %d.%d",
			    tgt, bus);
			rv = -1;
			break;
		}
		flags = sdp->isp_devparam[tgt].actv_flags;
#ifdef	CAM_NEW_TRAN_CODE
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
#else
		cts.flags = CCB_TRANS_CURRENT_SETTINGS;
		cts.valid = CCB_TRANS_DISC_VALID | CCB_TRANS_TQ_VALID;
		if (flags & DPARM_DISC) {
			cts.flags |= CCB_TRANS_DISC_ENB;
		}
		if (flags & DPARM_TQING) {
			cts.flags |= CCB_TRANS_TAG_ENB;
		}
		cts.valid |= CCB_TRANS_BUS_WIDTH_VALID;
		cts.bus_width = (flags & DPARM_WIDE)?
		    MSG_EXT_WDTR_BUS_8_BIT : MSG_EXT_WDTR_BUS_16_BIT;
		cts.sync_period = sdp->isp_devparam[tgt].actv_period;
		cts.sync_offset = sdp->isp_devparam[tgt].actv_offset;
		if (flags & DPARM_SYNC) {
			cts.valid |=
			    CCB_TRANS_SYNC_RATE_VALID |
			    CCB_TRANS_SYNC_OFFSET_VALID;
		}
#endif
		isp_prt(isp, ISP_LOGDEBUG2,
		    "NEW_TGT_PARAMS bus %d tgt %d period %x offset %x flags %x",
		    bus, tgt, sdp->isp_devparam[tgt].actv_period,
		    sdp->isp_devparam[tgt].actv_offset, flags);
		xpt_setup_ccb(&cts.ccb_h, tmppath, 1);
		xpt_async(AC_TRANSFER_NEG, tmppath, &cts);
		xpt_free_path(tmppath);
		break;
	}
	case ISPASYNC_BUS_RESET:
		bus = *((int *)arg);
		isp_prt(isp, ISP_LOGINFO, "SCSI bus reset on bus %d detected",
		    bus);
		if (bus > 0 && isp->isp_path2) {
			xpt_async(AC_BUS_RESET, isp->isp_path2, NULL);
		} else if (isp->isp_path) {
			xpt_async(AC_BUS_RESET, isp->isp_path, NULL);
		}
		break;
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
		if (msg == NULL) {
			msg = "LOOP Down";
		}
		if (isp->isp_path) {
			isp_freeze_loopdown(isp, msg);
		}
		if (isp->isp_osinfo.ldt_running == 0) {
			isp->isp_osinfo.ldt_running = 1;
			callout_reset(&isp->isp_osinfo.ldt,
			    isp->isp_osinfo.loop_down_limit * hz, isp_ldt, isp);
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			   "starting Loop Down Timer");
		}
		isp_prt(isp, ISP_LOGINFO, msg);
		break;
	case ISPASYNC_LOOP_UP:
		/*
		 * Now we just note that Loop has come up. We don't
		 * actually do anything because we're waiting for a
		 * Change Notify before activating the FC cleanup
		 * thread to look at the state of the loop again.
		 */
		isp_prt(isp, ISP_LOGINFO, "Loop UP");
		break;
	case ISPASYNC_DEV_ARRIVED:
		lp = arg;
		lp->reserved = 0;
		if ((isp->isp_role & ISP_ROLE_INITIATOR) &&
		    (lp->roles & (SVC3_TGT_ROLE >> SVC3_ROLE_SHIFT))) {
			int dbidx = lp - FCPARAM(isp)->portdb;
			int i;

			for (i = 0; i < MAX_FC_TARG; i++) {
				if (i >= FL_ID && i <= SNS_ID) {
					continue;
				}
				if (FCPARAM(isp)->isp_ini_map[i] == 0) {
					break;
				}
			}
			if (i < MAX_FC_TARG) {
				FCPARAM(isp)->isp_ini_map[i] = dbidx + 1;
				lp->ini_map_idx = i + 1;
			} else {
				isp_prt(isp, ISP_LOGWARN, "out of target ids");
				isp_dump_portdb(isp);
			}
		}
		if (lp->ini_map_idx) {
			tgt = lp->ini_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		            roles[lp->roles], "arrived at", tgt,
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
			isp_make_here(isp, tgt);
		} else {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
		            roles[lp->roles], "arrived",
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_DEV_CHANGED:
		lp = arg;
		if (isp_change_is_bad) {
			lp->state = FC_PORTDB_STATE_NIL;
			if (lp->ini_map_idx) {
				tgt = lp->ini_map_idx - 1;
				FCPARAM(isp)->isp_ini_map[tgt] = 0;
				lp->ini_map_idx = 0;
				isp_prt(isp, ISP_LOGCONFIG, prom3,
				    lp->portid, tgt, "change is bad");
				isp_make_gone(isp, tgt);
			} else {
				isp_prt(isp, ISP_LOGCONFIG, prom,
				    lp->portid, lp->handle,
				    roles[lp->roles],
				    "changed and departed",
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			}
		} else {
			lp->portid = lp->new_portid;
			lp->roles = lp->new_roles;
			if (lp->ini_map_idx) {
				int t = lp->ini_map_idx - 1;
				FCPARAM(isp)->isp_ini_map[t] =
				    (lp - FCPARAM(isp)->portdb) + 1;
				tgt = lp->ini_map_idx - 1;
				isp_prt(isp, ISP_LOGCONFIG, prom2,
				    lp->portid, lp->handle,
				    roles[lp->roles], "changed at", tgt,
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			} else {
				isp_prt(isp, ISP_LOGCONFIG, prom,
				    lp->portid, lp->handle,
				    roles[lp->roles], "changed",
				    (uint32_t) (lp->node_wwn >> 32),
				    (uint32_t) lp->node_wwn,
				    (uint32_t) (lp->port_wwn >> 32),
				    (uint32_t) lp->port_wwn);
			}
		}
		break;
	case ISPASYNC_DEV_STAYED:
		lp = arg;
		if (lp->ini_map_idx) {
			tgt = lp->ini_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		    	    roles[lp->roles], "stayed at", tgt,
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		} else {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
		    	    roles[lp->roles], "stayed",
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_DEV_GONE:
		lp = arg;
		/*
		 * If this has a virtual target and we haven't marked it
		 * that we're going to have isp_gdt tell the OS it's gone,
		 * set the isp_gdt timer running on it.
		 *
		 * If it isn't marked that isp_gdt is going to get rid of it,
		 * announce that it's gone.
		 */
		if (lp->ini_map_idx && lp->reserved == 0) {
			lp->reserved = 1;
			lp->new_reserved = isp->isp_osinfo.gone_device_time;
			lp->state = FC_PORTDB_STATE_ZOMBIE;
			if (isp->isp_osinfo.gdt_running == 0) {
				isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
				    "starting Gone Device Timer");
				isp->isp_osinfo.gdt_running = 1;
				callout_reset(&isp->isp_osinfo.gdt, hz,
				    isp_gdt, isp);
			}
			tgt = lp->ini_map_idx - 1;
			isp_prt(isp, ISP_LOGCONFIG, prom2,
			    lp->portid, lp->handle,
		            roles[lp->roles], "gone zombie at", tgt,
		    	    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
		    	    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		} else if (lp->reserved == 0) {
			isp_prt(isp, ISP_LOGCONFIG, prom,
			    lp->portid, lp->handle,
			    roles[lp->roles], "departed",
			    (uint32_t) (lp->node_wwn >> 32),
			    (uint32_t) lp->node_wwn,
			    (uint32_t) (lp->port_wwn >> 32),
			    (uint32_t) lp->port_wwn);
		}
		break;
	case ISPASYNC_CHANGE_NOTIFY:
	{
		char *msg;
		if (arg == ISPASYNC_CHANGE_PDB) {
			msg = "Port Database Changed";
		} else if (arg == ISPASYNC_CHANGE_SNS) {
			msg = "Name Server Database Changed";
		} else {
			msg = "Other Change Notify";
		}
		/*
		 * If the loop down timer is running, cancel it.
		 */
		if (isp->isp_osinfo.ldt_running) {
			isp_prt(isp, ISP_LOGSANCFG|ISP_LOGDEBUG0,
			   "Stopping Loop Down Timer");
			isp->isp_osinfo.ldt_running = 0;
			callout_stop(&isp->isp_osinfo.ldt);
		}
		isp_prt(isp, ISP_LOGINFO, msg);
		isp_freeze_loopdown(isp, msg);
		wakeup(ISP_KT_WCHAN(isp));
		break;
	}
#ifdef	ISP_TARGET_MODE
	case ISPASYNC_TARGET_NOTIFY:
	{
		tmd_notify_t *nt = arg;
		isp_prt(isp, ISP_LOGALL,
		    "target notify code 0x%x", nt->nt_ncode);
		break;
	}
	case ISPASYNC_TARGET_ACTION:
		switch (((isphdr_t *)arg)->rqs_entry_type) {
		default:
			isp_prt(isp, ISP_LOGWARN,
			   "event 0x%x for unhandled target action",
			    ((isphdr_t *)arg)->rqs_entry_type);
			break;
		case RQSTYPE_NOTIFY:
			if (IS_SCSI(isp)) {
				rv = isp_handle_platform_notify_scsi(isp,
				    (in_entry_t *) arg);
			} else {
				rv = isp_handle_platform_notify_fc(isp,
				    (in_fcentry_t *) arg);
			}
			break;
		case RQSTYPE_ATIO:
			rv = isp_handle_platform_atio(isp, (at_entry_t *) arg);
			break;
		case RQSTYPE_ATIO2:
			rv = isp_handle_platform_atio2(isp, (at2_entry_t *)arg);
			break;
		case RQSTYPE_CTIO3:
		case RQSTYPE_CTIO2:
		case RQSTYPE_CTIO:
			rv = isp_handle_platform_ctio(isp, arg);
			break;
		case RQSTYPE_ENABLE_LUN:
		case RQSTYPE_MODIFY_LUN:
			isp_ledone(isp, (lun_entry_t *) arg);
			break;
		}
		break;
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
                isp_prt(isp, ISP_LOGERR,
                    "Internal Firmware Error on bus %d @ RISC Address 0x%x",
                    mbox6, mbox1);
#ifdef	ISP_FW_CRASH_DUMP
		mbox1 = isp->isp_osinfo.mbox_sleep_ok;
		isp->isp_osinfo.mbox_sleep_ok = 0;
		if (IS_FC(isp)) {
			FCPARAM(isp)->isp_fwstate = FW_CONFIG_WAIT;
			FCPARAM(isp)->isp_loopstate = LOOP_NIL;
			isp_freeze_loopdown(isp, "f/w crash");
			isp_fw_dump(isp);
		}
		isp_reinit(isp);
		isp->isp_osinfo.mbox_sleep_ok = mbox1;
#else
		mbox1 = isp->isp_osinfo.mbox_sleep_ok;
		isp->isp_osinfo.mbox_sleep_ok = 0;
		isp_reinit(isp);
		isp->isp_osinfo.mbox_sleep_ok = mbox1;
#endif
		isp_async(isp, ISPASYNC_FW_RESTARTED, NULL);
		break;
	}
	case ISPASYNC_UNHANDLED_RESPONSE:
		break;
	default:
		isp_prt(isp, ISP_LOGERR, "unknown isp_async event %d", cmd);
		break;
	}
	return (rv);
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

void
isp_prt(ispsoftc_t *isp, int level, const char *fmt, ...)
{
	va_list ap;
	if (level != ISP_LOGALL && (level & isp->isp_dblev) == 0) {
		return;
	}
	printf("%s: ", device_get_nameunit(isp->isp_dev));
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
#if __FreeBSD_version < 700037
			tsleep(&isp->isp_mbxworkp, PRIBIO, "ispmbx_sleep",
			    isp_mstohz(ms));
#else
			msleep(&isp->isp_mbxworkp, &isp->isp_osinfo.lock,
			    PRIBIO, "ispmbx_sleep", isp_mstohz(ms));
#endif
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
				USEC_DELAY(100);
			}
			if (isp->isp_osinfo.mboxcmd_done) {
				break;
			}
		}
	}
	if (isp->isp_osinfo.mboxcmd_done == 0) {
		isp_prt(isp, ISP_LOGWARN,
		    "%s Mailbox Command (0x%x) Timeout (%uus)",
		    isp->isp_osinfo.mbox_sleep_ok? "Interrupting" : "Polled",
		    isp->isp_lastmbxcmd, usecs);
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
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_POSTREAD);
	} else {
		bus_dmamap_sync(isp->isp_osinfo.dmat,
		    PISP_PCMD(csio)->dmap, BUS_DMASYNC_POSTWRITE);
	}
	bus_dmamap_unload(isp->isp_osinfo.dmat, PISP_PCMD(csio)->dmap);
}
