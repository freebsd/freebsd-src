/*-
 * Copyright (c) 2009 Yahoo! Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Communications core for LSI MPT2 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/selinfo.h>
#include <sys/module.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/bio.h>
#include <sys/malloc.h>
#include <sys/uio.h>
#include <sys/sysctl.h>
#include <sys/sglist.h>
#include <sys/endian.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_periph.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>
#if __FreeBSD_version >= 900026
#include <cam/scsi/smp_all.h>
#endif

#include <dev/mps/mpi/mpi2_type.h>
#include <dev/mps/mpi/mpi2.h>
#include <dev/mps/mpi/mpi2_ioc.h>
#include <dev/mps/mpi/mpi2_sas.h>
#include <dev/mps/mpi/mpi2_cnfg.h>
#include <dev/mps/mpi/mpi2_init.h>
#include <dev/mps/mpsvar.h>
#include <dev/mps/mps_table.h>

struct mpssas_target {
	uint16_t	handle;
	uint8_t		linkrate;
	uint64_t	devname;
	uint64_t	sasaddr;
	uint32_t	devinfo;
	uint16_t	encl_handle;
	uint16_t	encl_slot;
	uint16_t	parent_handle;
	int		flags;
#define MPSSAS_TARGET_INABORT	(1 << 0)
#define MPSSAS_TARGET_INRESET	(1 << 1)
#define MPSSAS_TARGET_INCHIPRESET (1 << 2)
#define MPSSAS_TARGET_INRECOVERY 0x7
	uint16_t	tid;
};

struct mpssas_softc {
	struct mps_softc	*sc;
	u_int			flags;
#define MPSSAS_IN_DISCOVERY	(1 << 0)
#define MPSSAS_IN_STARTUP	(1 << 1)
#define MPSSAS_DISCOVERY_TIMEOUT_PENDING	(1 << 2)
#define MPSSAS_QUEUE_FROZEN	(1 << 3)
	struct mpssas_target	*targets;
	struct cam_devq		*devq;
	struct cam_sim		*sim;
	struct cam_path		*path;
	struct intr_config_hook	sas_ich;
	struct callout		discovery_callout;
	u_int			discovery_timeouts;
	struct mps_event_handle	*mpssas_eh;
};

struct mpssas_devprobe {
	struct mps_config_params	params;
	u_int			state;
#define MPSSAS_PROBE_DEV1	0x01
#define MPSSAS_PROBE_DEV2	0x02
#define MPSSAS_PROBE_PHY	0x03
#define MPSSAS_PROBE_EXP	0x04
#define MPSSAS_PROBE_PHY2	0x05
#define MPSSAS_PROBE_EXP2	0x06
	struct mpssas_target	target;
};

#define MPSSAS_DISCOVERY_TIMEOUT	20
#define MPSSAS_MAX_DISCOVERY_TIMEOUTS	10 /* 200 seconds */

MALLOC_DEFINE(M_MPSSAS, "MPSSAS", "MPS SAS memory");

static struct mpssas_target * mpssas_alloc_target(struct mpssas_softc *,
    struct mpssas_target *);
static struct mpssas_target * mpssas_find_target(struct mpssas_softc *, int,
     uint16_t);
static void mpssas_announce_device(struct mpssas_softc *,
     struct mpssas_target *);
static void mpssas_startup(void *data);
static void mpssas_discovery_end(struct mpssas_softc *sassc);
static void mpssas_discovery_timeout(void *data);
static void mpssas_prepare_remove(struct mpssas_softc *,
    MPI2_EVENT_SAS_TOPO_PHY_ENTRY *);
static void mpssas_remove_device(struct mps_softc *, struct mps_command *);
static void mpssas_remove_complete(struct mps_softc *, struct mps_command *);
static void mpssas_action(struct cam_sim *sim, union ccb *ccb);
static void mpssas_poll(struct cam_sim *sim);
static void mpssas_probe_device(struct mps_softc *sc, uint16_t handle);
static void mpssas_probe_device_complete(struct mps_softc *sc,
     struct mps_config_params *params);
static void mpssas_scsiio_timeout(void *data);
static void mpssas_abort_complete(struct mps_softc *sc, struct mps_command *cm);
static void mpssas_recovery(struct mps_softc *, struct mps_command *);
static int mpssas_map_tm_request(struct mps_softc *sc, struct mps_command *cm);
static void mpssas_issue_tm_request(struct mps_softc *sc,
				    struct mps_command *cm);
static void mpssas_tm_complete(struct mps_softc *sc, struct mps_command *cm,
			       int error);
static int mpssas_complete_tm_request(struct mps_softc *sc,
				      struct mps_command *cm, int free_cm);
static void mpssas_action_scsiio(struct mpssas_softc *, union ccb *);
static void mpssas_scsiio_complete(struct mps_softc *, struct mps_command *);
#if __FreeBSD_version >= 900026
static void mpssas_smpio_complete(struct mps_softc *sc, struct mps_command *cm);
static void mpssas_send_smpcmd(struct mpssas_softc *sassc, union ccb *ccb,
			       uint64_t sasaddr);
static void mpssas_action_smpio(struct mpssas_softc *sassc, union ccb *ccb);
#endif /* __FreeBSD_version >= 900026 */
static void mpssas_resetdev(struct mpssas_softc *, struct mps_command *);
static void mpssas_action_resetdev(struct mpssas_softc *, union ccb *);
static void mpssas_resetdev_complete(struct mps_softc *, struct mps_command *);
static void mpssas_freeze_device(struct mpssas_softc *, struct mpssas_target *);
static void mpssas_unfreeze_device(struct mpssas_softc *, struct mpssas_target *) __unused;

static struct mpssas_target *
mpssas_alloc_target(struct mpssas_softc *sassc, struct mpssas_target *probe)
{
	struct mpssas_target *target;
	int start;

	mps_dprint(sassc->sc, MPS_TRACE, "%s\n", __func__);

	/*
	 * If it's not a sata or sas target, CAM won't be able to see it.  Put
	 * it into a high-numbered slot so that it's accessible but not
	 * interrupting the target numbering sequence of real drives.
	 */
	if ((probe->devinfo & (MPI2_SAS_DEVICE_INFO_SSP_TARGET |
	    MPI2_SAS_DEVICE_INFO_STP_TARGET | MPI2_SAS_DEVICE_INFO_SATA_DEVICE))
	    == 0) {
		start = 200;
	} else {
		/*
		 * Use the enclosure number and slot number as a hint for target
		 * numbering.  If that doesn't produce a sane result, search the
		 * entire space.
		 */
#if 0
		start = probe->encl_handle * 16 + probe->encl_slot;
#else
		start = probe->encl_slot;
#endif
		if (start >= sassc->sc->facts->MaxTargets)
			start = 0;
	}

	target = mpssas_find_target(sassc, start, 0);

	/*
	 * Nothing found on the first pass, try a second pass that searches the
	 * entire space.
	 */
	if (target == NULL)
		target = mpssas_find_target(sassc, 0, 0);

	return (target);
}

static struct mpssas_target *
mpssas_find_target(struct mpssas_softc *sassc, int start, uint16_t handle)
{
	struct mpssas_target *target;
	int i;

	for (i = start; i < sassc->sc->facts->MaxTargets; i++) {
		target = &sassc->targets[i];
		if (target->handle == handle)
			return (target);
	}

	return (NULL);
}

/*
 * Start the probe sequence for a given device handle.  This will not
 * block.
 */
static void
mpssas_probe_device(struct mps_softc *sc, uint16_t handle)
{
	struct mpssas_devprobe *probe;
	struct mps_config_params *params;
	MPI2_CONFIG_EXTENDED_PAGE_HEADER *hdr;
	int error;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	probe = malloc(sizeof(*probe), M_MPSSAS, M_NOWAIT | M_ZERO);
	if (probe == NULL) {
		mps_dprint(sc, MPS_FAULT, "Out of memory starting probe\n");
		return;
	}
	params = &probe->params;
	hdr = &params->hdr.Ext;

	params->action = MPI2_CONFIG_ACTION_PAGE_HEADER;
	params->page_address = MPI2_SAS_DEVICE_PGAD_FORM_HANDLE | handle;
	hdr->ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_DEVICE;
	hdr->ExtPageLength = 0;
	hdr->PageNumber = 0;
	hdr->PageVersion = 0;
	params->buffer = NULL;
	params->length = 0;
	params->callback = mpssas_probe_device_complete;
	params->cbdata = probe;
	probe->target.handle = handle;
	probe->state = MPSSAS_PROBE_DEV1;

	if ((error = mps_read_config_page(sc, params)) != 0) {
		free(probe, M_MPSSAS);
		mps_dprint(sc, MPS_FAULT, "Failure starting device probe\n");
		return;
	}
}

static void
mpssas_probe_device_complete(struct mps_softc *sc,
    struct mps_config_params *params)
{
	MPI2_CONFIG_EXTENDED_PAGE_HEADER *hdr;
	struct mpssas_devprobe *probe;
	int error;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	hdr = &params->hdr.Ext;
	probe = params->cbdata;

	switch (probe->state) {
	case MPSSAS_PROBE_DEV1:
	case MPSSAS_PROBE_PHY:
	case MPSSAS_PROBE_EXP:
		if (params->status != MPI2_IOCSTATUS_SUCCESS) {
			mps_dprint(sc, MPS_FAULT,
			    "Probe Failure 0x%x state %d\n", params->status,
			    probe->state);
			free(probe, M_MPSSAS);
			return;
		}
		params->action = MPI2_CONFIG_ACTION_PAGE_READ_CURRENT;
		params->length = hdr->ExtPageLength * 4;
		params->buffer = malloc(params->length, M_MPSSAS,
		    M_ZERO|M_NOWAIT);
		if (params->buffer == NULL) {
			mps_dprint(sc, MPS_FAULT, "Out of memory at state "
			   "0x%x, size 0x%x\n", probe->state, params->length);
			free(probe, M_MPSSAS);
			return;
		}
		if (probe->state == MPSSAS_PROBE_DEV1)
			probe->state = MPSSAS_PROBE_DEV2;
		else if (probe->state == MPSSAS_PROBE_PHY)
			probe->state = MPSSAS_PROBE_PHY2;
		else if (probe->state == MPSSAS_PROBE_EXP)
			probe->state = MPSSAS_PROBE_EXP2;
		error = mps_read_config_page(sc, params);
		break;
	case MPSSAS_PROBE_DEV2:
	{
		MPI2_CONFIG_PAGE_SAS_DEV_0 *buf;

		if (params->status != MPI2_IOCSTATUS_SUCCESS) {
			mps_dprint(sc, MPS_FAULT,
			    "Probe Failure 0x%x state %d\n", params->status,
			    probe->state);
			free(params->buffer, M_MPSSAS);
			free(probe, M_MPSSAS);
			return;
		}
		buf = params->buffer;
		mps_print_sasdev0(sc, buf);

		probe->target.devname = mps_to_u64(&buf->DeviceName);
		probe->target.devinfo = buf->DeviceInfo;
		probe->target.encl_handle = buf->EnclosureHandle;
		probe->target.encl_slot = buf->Slot;
		probe->target.sasaddr = mps_to_u64(&buf->SASAddress);
		probe->target.parent_handle = buf->ParentDevHandle;

		if (buf->DeviceInfo & MPI2_SAS_DEVICE_INFO_DIRECT_ATTACH) {
			params->page_address =
			    MPI2_SAS_PHY_PGAD_FORM_PHY_NUMBER | buf->PhyNum;
			hdr->ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_PHY;
			hdr->PageNumber = 0;
			probe->state = MPSSAS_PROBE_PHY;
		} else {
			params->page_address =
			    MPI2_SAS_EXPAND_PGAD_FORM_HNDL_PHY_NUM |
			    buf->ParentDevHandle | (buf->PhyNum << 16);
			hdr->ExtPageType = MPI2_CONFIG_EXTPAGETYPE_SAS_EXPANDER;
			hdr->PageNumber = 1;
			probe->state = MPSSAS_PROBE_EXP;
		}
		params->action = MPI2_CONFIG_ACTION_PAGE_HEADER;
		hdr->ExtPageLength = 0;
		hdr->PageVersion = 0;
		params->buffer = NULL;
		params->length = 0;
		free(buf, M_MPSSAS);
		error = mps_read_config_page(sc, params);
		break;
	}
	case MPSSAS_PROBE_PHY2:
	case MPSSAS_PROBE_EXP2:
	{
		MPI2_CONFIG_PAGE_SAS_PHY_0 *phy;
		MPI2_CONFIG_PAGE_EXPANDER_1 *exp;
		struct mpssas_softc *sassc;
		struct mpssas_target *targ;
		char devstring[80];
		uint16_t handle;

		if (params->status != MPI2_IOCSTATUS_SUCCESS) {
			mps_dprint(sc, MPS_FAULT,
			    "Probe Failure 0x%x state %d\n", params->status,
			    probe->state);
			free(params->buffer, M_MPSSAS);
			free(probe, M_MPSSAS);
			return;
		}

		if (probe->state == MPSSAS_PROBE_PHY2) {
			phy = params->buffer;
			mps_print_sasphy0(sc, phy);
			probe->target.linkrate = phy->NegotiatedLinkRate & 0xf;
		} else {
			exp = params->buffer;
			mps_print_expander1(sc, exp);
			probe->target.linkrate = exp->NegotiatedLinkRate & 0xf;
		}
		free(params->buffer, M_MPSSAS);

		sassc = sc->sassc;
		handle = probe->target.handle;
		if ((targ = mpssas_find_target(sassc, 0, handle)) != NULL) {
			mps_printf(sc, "Ignoring dup device handle 0x%04x\n",
			    handle);
			free(probe, M_MPSSAS);
			return;
		}
		if ((targ = mpssas_alloc_target(sassc, &probe->target)) == NULL) {
			mps_printf(sc, "Target table overflow, handle 0x%04x\n",
			    handle);
			free(probe, M_MPSSAS);
			return;
		}

		*targ = probe->target;	/* Copy the attributes */
		targ->tid = targ - sassc->targets;
		mps_describe_devinfo(targ->devinfo, devstring, 80);
		if (bootverbose)
			mps_printf(sc, "Found device <%s> <%s> <0x%04x> "
			    "<%d/%d>\n", devstring,
			    mps_describe_table(mps_linkrate_names,
			    targ->linkrate), targ->handle, targ->encl_handle,
			    targ->encl_slot);

		free(probe, M_MPSSAS);
		mpssas_announce_device(sassc, targ);
		break;
	}
	default:
		printf("what?\n");
	}
}

/*
 * The MPT2 firmware performs debounce on the link to avoid transient link errors
 * and false removals.  When it does decide that link has been lost and a device
 * need to go away, it expects that the host will perform a target reset and then
 * an op remove.  The reset has the side-effect of aborting any outstanding
 * requests for the device, which is required for the op-remove to succeed.  It's
 * not clear if the host should check for the device coming back alive after the
 * reset.
 */
static void
mpssas_prepare_remove(struct mpssas_softc *sassc, MPI2_EVENT_SAS_TOPO_PHY_ENTRY *phy)
{
	MPI2_SCSI_TASK_MANAGE_REQUEST *req;
	struct mps_softc *sc;
	struct mps_command *cm;
	struct mpssas_target *targ = NULL;
	uint16_t handle;

	mps_dprint(sassc->sc, MPS_TRACE, "%s\n", __func__);

	handle = phy->AttachedDevHandle;
	targ = mpssas_find_target(sassc, 0, handle);
	if (targ == NULL)
		/* We don't know about this device? */
		return;

	sc = sassc->sc;
	cm = mps_alloc_command(sc);
	if (cm == NULL) {
		mps_printf(sc, "comand alloc failure in mpssas_prepare_remove\n");
		return;
	}

	req = (MPI2_SCSI_TASK_MANAGE_REQUEST *)cm->cm_req;
	req->DevHandle = targ->handle;
	req->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	req->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;

	/* SAS Hard Link Reset / SATA Link Reset */
	req->MsgFlags = MPI2_SCSITASKMGMT_MSGFLAGS_LINK_RESET;

	cm->cm_data = NULL;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	cm->cm_complete = mpssas_remove_device;
	cm->cm_targ = targ;
	mpssas_issue_tm_request(sc, cm);
}

static void
mpssas_remove_device(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_SCSI_TASK_MANAGE_REPLY *reply;
	MPI2_SAS_IOUNIT_CONTROL_REQUEST *req;
	struct mpssas_target *targ;
	uint16_t handle;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	reply = (MPI2_SCSI_TASK_MANAGE_REPLY *)cm->cm_reply;
	handle = cm->cm_targ->handle;

	mpssas_complete_tm_request(sc, cm, /*free_cm*/ 0);

	if (reply->IOCStatus != MPI2_IOCSTATUS_SUCCESS) {
		mps_printf(sc, "Failure 0x%x reseting device 0x%04x\n", 
		   reply->IOCStatus, handle);
		mps_free_command(sc, cm);
		return;
	}

	mps_printf(sc, "Reset aborted %d commands\n", reply->TerminationCount);
	mps_free_reply(sc, cm->cm_reply_data);

	/* Reuse the existing command */
	req = (MPI2_SAS_IOUNIT_CONTROL_REQUEST *)cm->cm_req;
	req->Function = MPI2_FUNCTION_SAS_IO_UNIT_CONTROL;
	req->Operation = MPI2_SAS_OP_REMOVE_DEVICE;
	req->DevHandle = handle;
	cm->cm_data = NULL;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	cm->cm_flags &= ~MPS_CM_FLAGS_COMPLETE;
	cm->cm_complete = mpssas_remove_complete;

	mps_map_command(sc, cm);

	mps_dprint(sc, MPS_INFO, "clearing target handle 0x%04x\n", handle);
	targ = mpssas_find_target(sc->sassc, 0, handle);
	if (targ != NULL) {
		targ->handle = 0x0;
		mpssas_announce_device(sc->sassc, targ);
	}
}

static void
mpssas_remove_complete(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_SAS_IOUNIT_CONTROL_REPLY *reply;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	reply = (MPI2_SAS_IOUNIT_CONTROL_REPLY *)cm->cm_reply;

	mps_printf(sc, "mpssas_remove_complete on target 0x%04x,"
	   " IOCStatus= 0x%x\n", cm->cm_targ->tid, reply->IOCStatus);

	mps_free_command(sc, cm);
}

static void
mpssas_evt_handler(struct mps_softc *sc, uintptr_t data,
    MPI2_EVENT_NOTIFICATION_REPLY *event)
{
	struct mpssas_softc *sassc;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	sassc = sc->sassc;
	mps_print_evt_sas(sc, event);

	switch (event->Event) {
	case MPI2_EVENT_SAS_DISCOVERY:
	{
		MPI2_EVENT_DATA_SAS_DISCOVERY *data;

		data = (MPI2_EVENT_DATA_SAS_DISCOVERY *)&event->EventData;

		if (data->ReasonCode & MPI2_EVENT_SAS_DISC_RC_STARTED)
			mps_dprint(sc, MPS_TRACE,"SAS discovery start event\n");
		if (data->ReasonCode & MPI2_EVENT_SAS_DISC_RC_COMPLETED) {
			mps_dprint(sc, MPS_TRACE, "SAS discovery end event\n");
			sassc->flags &= ~MPSSAS_IN_DISCOVERY;
			mpssas_discovery_end(sassc);
		}
		break;
	}
	case MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST:
	{
		MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST *data;
		MPI2_EVENT_SAS_TOPO_PHY_ENTRY *phy;
		int i;

		data = (MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST *)
		    &event->EventData;

		if (data->ExpStatus == MPI2_EVENT_SAS_TOPO_ES_ADDED) {
			if (bootverbose)
				printf("Expander found at enclosure %d\n",
				    data->EnclosureHandle);
			mpssas_probe_device(sc, data->ExpanderDevHandle);
		}

		for (i = 0; i < data->NumEntries; i++) {
			phy = &data->PHY[i];
			switch (phy->PhyStatus & MPI2_EVENT_SAS_TOPO_RC_MASK) {
			case MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED:
				mpssas_probe_device(sc, phy->AttachedDevHandle);
				break;
			case MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING:
				mpssas_prepare_remove(sassc, phy);
				break;
			case MPI2_EVENT_SAS_TOPO_RC_PHY_CHANGED:
			case MPI2_EVENT_SAS_TOPO_RC_NO_CHANGE:
			case MPI2_EVENT_SAS_TOPO_RC_DELAY_NOT_RESPONDING:
			default:
				break;
			}
		}

		break;
	}
	case MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE:
		break;
	default:
		break;
	}

	mps_free_reply(sc, data);
}

static int
mpssas_register_events(struct mps_softc *sc)
{
	uint8_t events[16];

	bzero(events, 16);
	setbit(events, MPI2_EVENT_SAS_DEVICE_STATUS_CHANGE);
	setbit(events, MPI2_EVENT_SAS_DISCOVERY);
	setbit(events, MPI2_EVENT_SAS_BROADCAST_PRIMITIVE);
	setbit(events, MPI2_EVENT_SAS_INIT_DEVICE_STATUS_CHANGE);
	setbit(events, MPI2_EVENT_SAS_INIT_TABLE_OVERFLOW);
	setbit(events, MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST);
	setbit(events, MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE);

	mps_register_events(sc, events, mpssas_evt_handler, NULL,
	    &sc->sassc->mpssas_eh);

	return (0);
}

int
mps_attach_sas(struct mps_softc *sc)
{
	struct mpssas_softc *sassc;
	int error = 0;
	int num_sim_reqs;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	sassc = malloc(sizeof(struct mpssas_softc), M_MPT2, M_WAITOK|M_ZERO);
	sassc->targets = malloc(sizeof(struct mpssas_target) *
	    sc->facts->MaxTargets, M_MPT2, M_WAITOK|M_ZERO);
	sc->sassc = sassc;
	sassc->sc = sc;

	/*
	 * Tell CAM that we can handle 5 fewer requests than we have
	 * allocated.  If we allow the full number of requests, all I/O
	 * will halt when we run out of resources.  Things work fine with
	 * just 1 less request slot given to CAM than we have allocated.
	 * We also need a couple of extra commands so that we can send down
	 * abort, reset, etc. requests when commands time out.  Otherwise
	 * we could wind up in a situation with sc->num_reqs requests down
	 * on the card and no way to send an abort.
	 *
	 * XXX KDM need to figure out why I/O locks up if all commands are
	 * used.
	 */
	num_sim_reqs = sc->num_reqs - 5;

	if ((sassc->devq = cam_simq_alloc(num_sim_reqs)) == NULL) {
		mps_dprint(sc, MPS_FAULT, "Cannot allocate SIMQ\n");
		error = ENOMEM;
		goto out;
	}

	sassc->sim = cam_sim_alloc(mpssas_action, mpssas_poll, "mps", sassc,
	    device_get_unit(sc->mps_dev), &sc->mps_mtx, num_sim_reqs,
	    num_sim_reqs, sassc->devq);
	if (sassc->sim == NULL) {
		mps_dprint(sc, MPS_FAULT, "Cannot allocate SIM\n");
		error = EINVAL;
		goto out;
	}

	/*
	 * XXX There should be a bus for every port on the adapter, but since
	 * we're just going to fake the topology for now, we'll pretend that
	 * everything is just a target on a single bus.
	 */
	mps_lock(sc);
	if ((error = xpt_bus_register(sassc->sim, sc->mps_dev, 0)) != 0) {
		mps_dprint(sc, MPS_FAULT, "Error %d registering SCSI bus\n",
		    error);
		mps_unlock(sc);
		goto out;
	}

	/*
	 * Assume that discovery events will start right away.  Freezing
	 * the simq will prevent the CAM boottime scanner from running
	 * before discovery is complete.
	 */
	sassc->flags = MPSSAS_IN_STARTUP | MPSSAS_IN_DISCOVERY;
	xpt_freeze_simq(sassc->sim, 1);

	mps_unlock(sc);

	callout_init(&sassc->discovery_callout, 1 /*mpsafe*/);
	sassc->discovery_timeouts = 0;

	mpssas_register_events(sc);
out:
	if (error)
		mps_detach_sas(sc);
	return (error);
}

int
mps_detach_sas(struct mps_softc *sc)
{
	struct mpssas_softc *sassc;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	if (sc->sassc == NULL)
		return (0);

	sassc = sc->sassc;

	/* Make sure CAM doesn't wedge if we had to bail out early. */
	mps_lock(sc);
	if (sassc->flags & MPSSAS_IN_STARTUP)
		xpt_release_simq(sassc->sim, 1);
	mps_unlock(sc);

	if (sassc->mpssas_eh != NULL)
		mps_deregister_events(sc, sassc->mpssas_eh);

	mps_lock(sc);

	if (sassc->sim != NULL) {
		xpt_bus_deregister(cam_sim_path(sassc->sim));
		cam_sim_free(sassc->sim, FALSE);
	}
	mps_unlock(sc);

	if (sassc->devq != NULL)
		cam_simq_free(sassc->devq);

	free(sassc->targets, M_MPT2);
	free(sassc, M_MPT2);
	sc->sassc = NULL;

	return (0);
}

static void
mpssas_discovery_end(struct mpssas_softc *sassc)
{
	struct mps_softc *sc = sassc->sc;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	if (sassc->flags & MPSSAS_DISCOVERY_TIMEOUT_PENDING)
		callout_stop(&sassc->discovery_callout);

	if ((sassc->flags & MPSSAS_IN_STARTUP) != 0) {
		mps_dprint(sc, MPS_INFO,
		    "mpssas_discovery_end: removing confighook\n");
		sassc->flags &= ~MPSSAS_IN_STARTUP;
		xpt_release_simq(sassc->sim, 1);
	}
#if 0
	mpssas_announce_device(sassc, NULL);
#endif

}

static void
mpssas_announce_device(struct mpssas_softc *sassc, struct mpssas_target *targ)
{
	union ccb *ccb;
	int bus, tid, lun;

	/*
	 * Force a rescan, a hackish way to announce devices.
	 * XXX Doing a scan on an individual device is hackish in that it
	 *     won't scan the LUNs.
	 * XXX Does it matter if any of this fails?
	 */
	bus = cam_sim_path(sassc->sim);
	if (targ != NULL) {
		tid = targ->tid;
		lun = 0;
	} else {
		tid = CAM_TARGET_WILDCARD;
		lun = CAM_LUN_WILDCARD;
	}
	ccb = xpt_alloc_ccb_nowait();
	if (ccb == NULL)
		return;
	if (xpt_create_path(&ccb->ccb_h.path, xpt_periph, bus, tid,
	    CAM_LUN_WILDCARD) != CAM_REQ_CMP) { 
		xpt_free_ccb(ccb);
		return;
	}
	mps_dprint(sassc->sc, MPS_INFO, "Triggering rescan of %d:%d:-1\n",
	    bus, tid);
	xpt_rescan(ccb);
}

static void
mpssas_startup(void *data)
{
	struct mpssas_softc *sassc = data;

	mps_dprint(sassc->sc, MPS_TRACE, "%s\n", __func__);

	mps_lock(sassc->sc);
	if ((sassc->flags & MPSSAS_IN_DISCOVERY) == 0) {
		mpssas_discovery_end(sassc);
	} else {
		if (sassc->discovery_timeouts < MPSSAS_MAX_DISCOVERY_TIMEOUTS) {
			sassc->flags |= MPSSAS_DISCOVERY_TIMEOUT_PENDING;
			callout_reset(&sassc->discovery_callout,
			    MPSSAS_DISCOVERY_TIMEOUT * hz,
			    mpssas_discovery_timeout, sassc);
			sassc->discovery_timeouts++;
		} else {
			mps_dprint(sassc->sc, MPS_FAULT,
			    "Discovery timed out, continuing.\n");
			sassc->flags &= ~MPSSAS_IN_DISCOVERY;
			mpssas_discovery_end(sassc);
		}
	}
	mps_unlock(sassc->sc);

	return;
}

static void
mpssas_discovery_timeout(void *data)
{
	struct mpssas_softc *sassc = data;
	struct mps_softc *sc;

	sc = sassc->sc;
	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	mps_lock(sc);
	mps_printf(sc,
	    "Timeout waiting for discovery, interrupts may not be working!\n");
	sassc->flags &= ~MPSSAS_DISCOVERY_TIMEOUT_PENDING;

	/* Poll the hardware for events in case interrupts aren't working */
	mps_intr_locked(sc);
	mps_unlock(sc);

	/* Check the status of discovery and re-arm the timeout if needed */
	mpssas_startup(sassc);
}

static void
mpssas_action(struct cam_sim *sim, union ccb *ccb)
{
	struct mpssas_softc *sassc;

	sassc = cam_sim_softc(sim);

	mps_dprint(sassc->sc, MPS_TRACE, "%s func 0x%x\n", __func__,
	    ccb->ccb_h.func_code);

	switch (ccb->ccb_h.func_code) {
	case XPT_PATH_INQ:
	{
		struct ccb_pathinq *cpi = &ccb->cpi;

		cpi->version_num = 1;
		cpi->hba_inquiry = PI_SDTR_ABLE|PI_TAG_ABLE|PI_WIDE_16;
		cpi->target_sprt = 0;
		cpi->hba_misc = PIM_NOBUSRESET;
		cpi->hba_eng_cnt = 0;
		cpi->max_target = sassc->sc->facts->MaxTargets - 1;
		cpi->max_lun = 0;
		cpi->initiator_id = 255;
		strncpy(cpi->sim_vid, "FreeBSD", SIM_IDLEN);
		strncpy(cpi->hba_vid, "LSILogic", HBA_IDLEN);
		strncpy(cpi->dev_name, cam_sim_name(sim), DEV_IDLEN);
		cpi->unit_number = cam_sim_unit(sim);
		cpi->bus_id = cam_sim_bus(sim);
		cpi->base_transfer_speed = 150000;
		cpi->transport = XPORT_SAS;
		cpi->transport_version = 0;
		cpi->protocol = PROTO_SCSI;
		cpi->protocol_version = SCSI_REV_SPC;
		cpi->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_GET_TRAN_SETTINGS:
	{
		struct ccb_trans_settings	*cts;
		struct ccb_trans_settings_sas	*sas;
		struct ccb_trans_settings_scsi	*scsi;
		struct mpssas_target *targ;

		cts = &ccb->cts;
		sas = &cts->xport_specific.sas;
		scsi = &cts->proto_specific.scsi;

		targ = &sassc->targets[cts->ccb_h.target_id];
		if (targ->handle == 0x0) {
			cts->ccb_h.status = CAM_TID_INVALID;
			break;
		}

		cts->protocol_version = SCSI_REV_SPC2;
		cts->transport = XPORT_SAS;
		cts->transport_version = 0;

		sas->valid = CTS_SAS_VALID_SPEED;
		switch (targ->linkrate) {
		case 0x08:
			sas->bitrate = 150000;
			break;
		case 0x09:
			sas->bitrate = 300000;
			break;
		case 0x0a:
			sas->bitrate = 600000;
			break;
		default:
			sas->valid = 0;
		}

		cts->protocol = PROTO_SCSI;
		scsi->valid = CTS_SCSI_VALID_TQ;
		scsi->flags = CTS_SCSI_FLAGS_TAG_ENB;

		cts->ccb_h.status = CAM_REQ_CMP;
		break;
	}
	case XPT_CALC_GEOMETRY:
		cam_calc_geometry(&ccb->ccg, /*extended*/1);
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_RESET_DEV:
		mpssas_action_resetdev(sassc, ccb);
		return;
	case XPT_RESET_BUS:
	case XPT_ABORT:
	case XPT_TERM_IO:
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case XPT_SCSI_IO:
		mpssas_action_scsiio(sassc, ccb);
		return;
#if __FreeBSD_version >= 900026
	case XPT_SMP_IO:
		mpssas_action_smpio(sassc, ccb);
		return;
#endif /* __FreeBSD_version >= 900026 */
	default:
		ccb->ccb_h.status = CAM_FUNC_NOTAVAIL;
		break;
	}
	xpt_done(ccb);

}

#if 0
static void
mpssas_resettimeout_complete(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_SCSI_TASK_MANAGE_REPLY *resp;
	uint16_t code;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	resp = (MPI2_SCSI_TASK_MANAGE_REPLY *)cm->cm_reply;
	code = resp->ResponseCode;

	mps_free_command(sc, cm);
	mpssas_unfreeze_device(sassc, targ);

	if (code != MPI2_SCSITASKMGMT_RSP_TM_COMPLETE) {
		mps_reset_controller(sc);
	}

	return;
}
#endif

static void
mpssas_scsiio_timeout(void *data)
{
	union ccb *ccb;
	struct mps_softc *sc;
	struct mps_command *cm;
	struct mpssas_target *targ;
#if 0
	char cdb_str[(SCSI_MAX_CDBLEN * 3) + 1];
#endif

	cm = (struct mps_command *)data;
	sc = cm->cm_sc;

	/*
	 * Run the interrupt handler to make sure it's not pending.  This
	 * isn't perfect because the command could have already completed
	 * and been re-used, though this is unlikely.
	 */
	mps_lock(sc);
	mps_intr_locked(sc);
	if (cm->cm_state == MPS_CM_STATE_FREE) {
		mps_unlock(sc);
		return;
	}

	ccb = cm->cm_complete_data;
	targ = cm->cm_targ;
	if (targ == 0x00)
		/* Driver bug */
		targ = &sc->sassc->targets[ccb->ccb_h.target_id];

	xpt_print(ccb->ccb_h.path, "SCSI command timeout on device handle "
		  "0x%04x SMID %d\n", targ->handle, cm->cm_desc.Default.SMID);
	/*
	 * XXX KDM this is useful for debugging purposes, but the existing
	 * scsi_op_desc() implementation can't handle a NULL value for
	 * inq_data.  So this will remain commented out until I bring in
	 * those changes as well.
	 */
#if 0
	xpt_print(ccb->ccb_h.path, "Timed out command: %s. CDB %s\n",
		  scsi_op_desc((ccb->ccb_h.flags & CAM_CDB_POINTER) ?
		  		ccb->csio.cdb_io.cdb_ptr[0] :
				ccb->csio.cdb_io.cdb_bytes[0], NULL),
		  scsi_cdb_string((ccb->ccb_h.flags & CAM_CDB_POINTER) ?
				   ccb->csio.cdb_io.cdb_ptr :
				   ccb->csio.cdb_io.cdb_bytes, cdb_str,
		  		   sizeof(cdb_str)));
#endif

	/* Inform CAM about the timeout and that recovery is starting. */
#if 0
	if ((targ->flags & MPSSAS_TARGET_INRECOVERY) == 0) {
		mpssas_freeze_device(sc->sassc, targ);
		ccb->ccb_h.status = CAM_CMD_TIMEOUT;
		xpt_done(ccb);
	}
#endif
	mpssas_freeze_device(sc->sassc, targ);
	ccb->ccb_h.status = CAM_CMD_TIMEOUT;

	/*
	 * recycle the command into recovery so that there's no risk of
	 * command allocation failure.
	 */
	cm->cm_state = MPS_CM_STATE_TIMEDOUT;
	mpssas_recovery(sc, cm);
	mps_unlock(sc);
}

static void
mpssas_abort_complete(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_SCSI_TASK_MANAGE_REQUEST *req;

	req = (MPI2_SCSI_TASK_MANAGE_REQUEST *)cm->cm_req;

	mps_printf(sc, "%s: abort request on handle %#04x SMID %d "
		   "complete\n", __func__, req->DevHandle, req->TaskMID);

	mpssas_complete_tm_request(sc, cm, /*free_cm*/ 1);
}

static void
mpssas_recovery(struct mps_softc *sc, struct mps_command *abort_cm)
{
	struct mps_command *cm;
	MPI2_SCSI_TASK_MANAGE_REQUEST *req, *orig_req;

	cm = mps_alloc_command(sc);
	if (cm == NULL) {
		mps_printf(sc, "%s: command allocation failure\n", __func__);
		return;
	}

	cm->cm_targ = abort_cm->cm_targ;
	cm->cm_complete = mpssas_abort_complete;

	req = (MPI2_SCSI_TASK_MANAGE_REQUEST *)cm->cm_req;
	orig_req = (MPI2_SCSI_TASK_MANAGE_REQUEST *)abort_cm->cm_req;
	req->DevHandle = abort_cm->cm_targ->handle;
	req->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	req->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_ABORT_TASK;
	memcpy(req->LUN, orig_req->LUN, sizeof(req->LUN));
	req->TaskMID = abort_cm->cm_desc.Default.SMID;

	cm->cm_data = NULL;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;

	mpssas_issue_tm_request(sc, cm);

}

/*
 * Can return 0 or EINPROGRESS on success.  Any other value means failure.
 */
static int
mpssas_map_tm_request(struct mps_softc *sc, struct mps_command *cm)
{
	int error;

	error = 0;

	cm->cm_flags |= MPS_CM_FLAGS_ACTIVE;
	error = mps_map_command(sc, cm);
	if ((error == 0)
	 || (error == EINPROGRESS))
		sc->tm_cmds_active++;

	return (error);
}

static void
mpssas_issue_tm_request(struct mps_softc *sc, struct mps_command *cm)
{
	int freeze_queue, send_command, error;

	freeze_queue = 0;
	send_command = 0;
	error = 0;

	mtx_assert(&sc->mps_mtx, MA_OWNED);

	/*
	 * If there are no other pending task management commands, go
	 * ahead and send this one.  There is a small amount of anecdotal
	 * evidence that sending lots of task management commands at once
	 * may cause the controller to lock up.  Or, if the user has
	 * configured the driver (via the allow_multiple_tm_cmds variable) to
	 * not serialize task management commands, go ahead and send the
	 * command if even other task management commands are pending.
	 */
	if (TAILQ_FIRST(&sc->tm_list) == NULL) {
		send_command = 1;
		freeze_queue = 1;
	} else if (sc->allow_multiple_tm_cmds != 0)
		send_command = 1;

	TAILQ_INSERT_TAIL(&sc->tm_list, cm, cm_link);
	if (send_command != 0) {
		/*
		 * Freeze the SIM queue while we issue the task management
		 * command.  According to the Fusion-MPT 2.0 spec, task
		 * management requests are serialized, and so the host
		 * should not send any I/O requests while task management
		 * requests are pending.
		 */
		if (freeze_queue != 0)
			xpt_freeze_simq(sc->sassc->sim, 1);

		error = mpssas_map_tm_request(sc, cm);

		/*
		 * At present, there is no error path back from
		 * mpssas_map_tm_request() (which calls mps_map_command())
		 * when cm->cm_data == NULL.  But since there is a return
		 * value, we check it just in case the implementation
		 * changes later.
		 */
		if ((error != 0)
		 && (error != EINPROGRESS))
			mpssas_tm_complete(sc, cm,
			    MPI2_SCSITASKMGMT_RSP_TM_FAILED);
	}
}

static void
mpssas_tm_complete(struct mps_softc *sc, struct mps_command *cm, int error)
{
	MPI2_SCSI_TASK_MANAGE_REPLY *resp;

	resp = (MPI2_SCSI_TASK_MANAGE_REPLY *)cm->cm_reply;

	resp->ResponseCode = error;

	/*
	 * Call the callback for this command, it will be
	 * removed from the list and freed via the callback.
	 */
	cm->cm_complete(sc, cm);
}

/*
 * Complete a task management request.  The basic completion operation will
 * always succeed.  Returns status for sending any further task management
 * commands that were queued.
 */
static int
mpssas_complete_tm_request(struct mps_softc *sc, struct mps_command *cm,
			   int free_cm)
{
	int error;

	error = 0;

	mtx_assert(&sc->mps_mtx, MA_OWNED);

	TAILQ_REMOVE(&sc->tm_list, cm, cm_link);
	cm->cm_flags &= ~MPS_CM_FLAGS_ACTIVE;
	sc->tm_cmds_active--;

	if (free_cm != 0)
		mps_free_command(sc, cm);

	if (TAILQ_FIRST(&sc->tm_list) == NULL) {
		/*
		 * Release the SIM queue, we froze it when we sent the first
		 * task management request.
		 */
		xpt_release_simq(sc->sassc->sim, 1);
	} else if ((sc->tm_cmds_active == 0)
		|| (sc->allow_multiple_tm_cmds != 0)) {
		int error;
		struct mps_command *cm2;

restart_traversal:

		/*
		 * We don't bother using TAILQ_FOREACH_SAFE here, but
		 * rather use the standard version and just restart the
		 * list traversal if we run into the error case.
		 * TAILQ_FOREACH_SAFE allows safe removal of the current
		 * list element, but if you have a queue of task management
		 * commands, all of which have mapping errors, you'll end
		 * up with recursive calls to this routine and so you could
		 * wind up removing more than just the current list element.
		 */
		TAILQ_FOREACH(cm2, &sc->tm_list, cm_link) {
			MPI2_SCSI_TASK_MANAGE_REQUEST *req;

			/* This command is active, no need to send it again */
			if (cm2->cm_flags & MPS_CM_FLAGS_ACTIVE)
				continue;

			req = (MPI2_SCSI_TASK_MANAGE_REQUEST *)cm2->cm_req;

			mps_printf(sc, "%s: sending deferred task management "
			    "request for handle %#04x SMID %d\n", __func__,
			    req->DevHandle, req->TaskMID);

			error = mpssas_map_tm_request(sc, cm2);

			/*
			 * Check for errors.  If we had an error, complete
			 * this command with an error, and keep going through
			 * the list until we are able to send at least one
			 * command or all of them are completed with errors.
			 *
			 * We don't want to wind up in a situation where
			 * we're stalled out with no way for queued task
			 * management commands to complete.
			 *
			 * Note that there is not currently an error path
			 * back from mpssas_map_tm_request() (which calls
			 * mps_map_command()) when cm->cm_data == NULL.
			 * But we still want to check for errors here in
			 * case the implementation changes, or in case
			 * there is some reason for a data payload here.
			 */
			if ((error != 0)
			 && (error != EINPROGRESS)) {
				mpssas_tm_complete(sc, cm,
				    MPI2_SCSITASKMGMT_RSP_TM_FAILED);

				/*
				 * If we don't currently have any commands
				 * active, go back to the beginning and see
				 * if there are any more that can be started.
				 * Otherwise, we're done here.
				 */
				if (sc->tm_cmds_active == 0)
					goto restart_traversal;
				else
					break;
			}

			/*
			 * If the user only wants one task management command
			 * active at a time, we're done, since we've
			 * already successfully sent a command at this point.
			 */
			if (sc->allow_multiple_tm_cmds == 0)
				break;
		}
	}

	return (error);
}

static void
mpssas_action_scsiio(struct mpssas_softc *sassc, union ccb *ccb)
{
	MPI2_SCSI_IO_REQUEST *req;
	struct ccb_scsiio *csio;
	struct mps_softc *sc;
	struct mpssas_target *targ;
	struct mps_command *cm;

	mps_dprint(sassc->sc, MPS_TRACE, "%s\n", __func__);

	sc = sassc->sc;

	csio = &ccb->csio;
	targ = &sassc->targets[csio->ccb_h.target_id];
	if (targ->handle == 0x0) {
		csio->ccb_h.status = CAM_SEL_TIMEOUT;
		xpt_done(ccb);
		return;
	}

	cm = mps_alloc_command(sc);
	if (cm == NULL) {
		if ((sassc->flags & MPSSAS_QUEUE_FROZEN) == 0) {
			xpt_freeze_simq(sassc->sim, 1);
			sassc->flags |= MPSSAS_QUEUE_FROZEN;
		}
		ccb->ccb_h.status &= ~CAM_SIM_QUEUED;
		ccb->ccb_h.status |= CAM_REQUEUE_REQ;
		xpt_done(ccb);
		return;
	}

	req = (MPI2_SCSI_IO_REQUEST *)cm->cm_req;
	req->DevHandle = targ->handle;
	req->Function = MPI2_FUNCTION_SCSI_IO_REQUEST;
	req->MsgFlags = 0;
	req->SenseBufferLowAddress = cm->cm_sense_busaddr;
	req->SenseBufferLength = MPS_SENSE_LEN;
	req->SGLFlags = 0;
	req->ChainOffset = 0;
	req->SGLOffset0 = 24;	/* 32bit word offset to the SGL */
	req->SGLOffset1= 0;
	req->SGLOffset2= 0;
	req->SGLOffset3= 0;
	req->SkipCount = 0;
	req->DataLength = csio->dxfer_len;
	req->BidirectionalDataLength = 0;
	req->IoFlags = csio->cdb_len;
	req->EEDPFlags = 0;

	/* Note: BiDirectional transfers are not supported */
	switch (csio->ccb_h.flags & CAM_DIR_MASK) {
	case CAM_DIR_IN:
		req->Control = MPI2_SCSIIO_CONTROL_READ;
		cm->cm_flags |= MPS_CM_FLAGS_DATAIN;
		break;
	case CAM_DIR_OUT:
		req->Control = MPI2_SCSIIO_CONTROL_WRITE;
		cm->cm_flags |= MPS_CM_FLAGS_DATAOUT;
		break;
	case CAM_DIR_NONE:
	default:
		req->Control = MPI2_SCSIIO_CONTROL_NODATATRANSFER;
		break;
	}

	/*
	 * It looks like the hardware doesn't require an explicit tag
	 * number for each transaction.  SAM Task Management not supported
	 * at the moment.
	 */
	switch (csio->tag_action) {
	case MSG_HEAD_OF_Q_TAG:
		req->Control |= MPI2_SCSIIO_CONTROL_HEADOFQ;
		break;
	case MSG_ORDERED_Q_TAG:
		req->Control |= MPI2_SCSIIO_CONTROL_ORDEREDQ;
		break;
	case MSG_ACA_TASK:
		req->Control |= MPI2_SCSIIO_CONTROL_ACAQ;
		break;
	case CAM_TAG_ACTION_NONE:
	case MSG_SIMPLE_Q_TAG:
	default:
		req->Control |= MPI2_SCSIIO_CONTROL_SIMPLEQ;
		break;
	}

	/* XXX Need to handle multi-level LUNs */
	if (csio->ccb_h.target_lun > 255) {
		mps_free_command(sc, cm);
		ccb->ccb_h.status = CAM_LUN_INVALID;
		xpt_done(ccb);
		return;
	}
	req->LUN[1] = csio->ccb_h.target_lun;

	if (csio->ccb_h.flags & CAM_CDB_POINTER)
		bcopy(csio->cdb_io.cdb_ptr, &req->CDB.CDB32[0], csio->cdb_len);
	else
		bcopy(csio->cdb_io.cdb_bytes, &req->CDB.CDB32[0],csio->cdb_len);
	req->IoFlags = csio->cdb_len;

	/*
	 * XXX need to handle S/G lists and physical addresses here.
	 */
	cm->cm_data = csio->data_ptr;
	cm->cm_length = csio->dxfer_len;
	cm->cm_sge = &req->SGL;
	cm->cm_sglsize = (32 - 24) * 4;
	cm->cm_desc.SCSIIO.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_SCSI_IO;
	cm->cm_desc.SCSIIO.DevHandle = targ->handle;
	cm->cm_complete = mpssas_scsiio_complete;
	cm->cm_complete_data = ccb;
	cm->cm_targ = targ;

	callout_reset(&cm->cm_callout, (ccb->ccb_h.timeout * hz) / 1000,
	   mpssas_scsiio_timeout, cm);

	mps_map_command(sc, cm);
	return;
}

static void
mpssas_scsiio_complete(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_SCSI_IO_REPLY *rep;
	union ccb *ccb;
	struct mpssas_softc *sassc;
	u_int sense_len;
	int dir = 0;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	callout_stop(&cm->cm_callout);

	sassc = sc->sassc;
	ccb = cm->cm_complete_data;
	rep = (MPI2_SCSI_IO_REPLY *)cm->cm_reply;

	if (cm->cm_data != NULL) {
		if (cm->cm_flags & MPS_CM_FLAGS_DATAIN)
			dir = BUS_DMASYNC_POSTREAD;
		else if (cm->cm_flags & MPS_CM_FLAGS_DATAOUT)
			dir = BUS_DMASYNC_POSTWRITE;;
		bus_dmamap_sync(sc->buffer_dmat, cm->cm_dmamap, dir);
		bus_dmamap_unload(sc->buffer_dmat, cm->cm_dmamap);
	}

	if (sassc->flags & MPSSAS_QUEUE_FROZEN) {
		ccb->ccb_h.flags |= CAM_RELEASE_SIMQ;
		sassc->flags &= ~MPSSAS_QUEUE_FROZEN;
	}

	/* Take the fast path to completion */
	if (cm->cm_reply == NULL) {
		ccb->ccb_h.status = CAM_REQ_CMP;
		ccb->csio.scsi_status = SCSI_STATUS_OK;
		mps_free_command(sc, cm);
		xpt_done(ccb);
		return;
	}

	mps_dprint(sc, MPS_INFO, "(%d:%d:%d) IOCStatus= 0x%x, "
	    "ScsiStatus= 0x%x, SCSIState= 0x%x TransferCount= 0x%x\n",
	    xpt_path_path_id(ccb->ccb_h.path),
	    xpt_path_target_id(ccb->ccb_h.path),
	    xpt_path_lun_id(ccb->ccb_h.path), rep->IOCStatus,
	    rep->SCSIStatus, rep->SCSIState, rep->TransferCount);

	switch (rep->IOCStatus & MPI2_IOCSTATUS_MASK) {
	case MPI2_IOCSTATUS_BUSY:
	case MPI2_IOCSTATUS_INSUFFICIENT_RESOURCES:
		/*
		 * The controller is overloaded, try waiting a bit for it
		 * to free up.
		 */
		ccb->ccb_h.status = CAM_BUSY;
		break;
	case MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN:
		ccb->csio.resid = cm->cm_length - rep->TransferCount;
		/* FALLTHROUGH */
	case MPI2_IOCSTATUS_SUCCESS:
	case MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR:
		ccb->ccb_h.status = CAM_REQ_CMP;
		break;
	case MPI2_IOCSTATUS_SCSI_DATA_OVERRUN:
		/* resid is ignored for this condition */
		ccb->csio.resid = 0;
		ccb->ccb_h.status = CAM_DATA_RUN_ERR;
		break;
	case MPI2_IOCSTATUS_SCSI_INVALID_DEVHANDLE:
	case MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		ccb->ccb_h.status = CAM_DEV_NOT_THERE;
		break;
	case MPI2_IOCSTATUS_SCSI_TASK_TERMINATED:
		/*
		 * This is one of the responses that comes back when an I/O
		 * has been aborted.  If it is because of a timeout that we
		 * initiated, just set the status to CAM_CMD_TIMEOUT.
		 * Otherwise set it to CAM_REQ_ABORTED.  The effect on the
		 * command is the same (it gets retried, subject to the
		 * retry counter), the only difference is what gets printed
		 * on the console.
		 */
		if (cm->cm_state == MPS_CM_STATE_TIMEDOUT)
			ccb->ccb_h.status = CAM_CMD_TIMEOUT;
		else
			ccb->ccb_h.status = CAM_REQ_ABORTED;
		break;
	case MPI2_IOCSTATUS_SCSI_IOC_TERMINATED:
	case MPI2_IOCSTATUS_SCSI_EXT_TERMINATED:
		ccb->ccb_h.status = CAM_REQ_ABORTED;
		break;
	case MPI2_IOCSTATUS_INVALID_SGL:
		mps_print_scsiio_cmd(sc, cm);
		ccb->ccb_h.status = CAM_UNREC_HBA_ERROR;
		break;
	case MPI2_IOCSTATUS_INVALID_FUNCTION:
	case MPI2_IOCSTATUS_INTERNAL_ERROR:
	case MPI2_IOCSTATUS_INVALID_VPID:
	case MPI2_IOCSTATUS_INVALID_FIELD:
	case MPI2_IOCSTATUS_INVALID_STATE:
	case MPI2_IOCSTATUS_OP_STATE_NOT_SUPPORTED:
	case MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR:
	case MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR:
	case MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
	case MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
	default:
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
	}


	if ((rep->SCSIState & MPI2_SCSI_STATE_NO_SCSI_STATUS) == 0) {
		ccb->csio.scsi_status = rep->SCSIStatus;

		switch (rep->SCSIStatus) {
		case MPI2_SCSI_STATUS_TASK_SET_FULL:
		case MPI2_SCSI_STATUS_CHECK_CONDITION:
			ccb->ccb_h.status = CAM_SCSI_STATUS_ERROR;
			break;
		case MPI2_SCSI_STATUS_COMMAND_TERMINATED:
		case MPI2_SCSI_STATUS_TASK_ABORTED:
			ccb->ccb_h.status = CAM_REQ_ABORTED;
			break;
		case MPI2_SCSI_STATUS_GOOD:
		default:
			break;
		}
	}

	if (rep->SCSIState & MPI2_SCSI_STATE_AUTOSENSE_VALID) {
		sense_len = MIN(rep->SenseCount,
		    sizeof(struct scsi_sense_data));
		if (sense_len < rep->SenseCount)
			ccb->csio.sense_resid = rep->SenseCount - sense_len;
		bcopy(cm->cm_sense, &ccb->csio.sense_data, sense_len);
		ccb->ccb_h.status |= CAM_AUTOSNS_VALID;
	}

	if (rep->SCSIState & MPI2_SCSI_STATE_AUTOSENSE_FAILED)
		ccb->ccb_h.status = CAM_AUTOSENSE_FAIL;

	if (rep->SCSIState & MPI2_SCSI_STATE_RESPONSE_INFO_VALID)
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;

	mps_free_command(sc, cm);
	xpt_done(ccb);
}

#if __FreeBSD_version >= 900026
static void
mpssas_smpio_complete(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_SMP_PASSTHROUGH_REPLY *rpl;
	MPI2_SMP_PASSTHROUGH_REQUEST *req;
	uint64_t sasaddr;
	union ccb *ccb;

	ccb = cm->cm_complete_data;
	rpl = (MPI2_SMP_PASSTHROUGH_REPLY *)cm->cm_reply;
	if (rpl == NULL) {
		mps_dprint(sc, MPS_INFO, "%s: NULL cm_reply!\n", __func__);
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		goto bailout;
	}

	req = (MPI2_SMP_PASSTHROUGH_REQUEST *)cm->cm_req;
	sasaddr = le32toh(req->SASAddress.Low);
	sasaddr |= ((uint64_t)(le32toh(req->SASAddress.High))) << 32;

	if ((rpl->IOCStatus & MPI2_IOCSTATUS_MASK) != MPI2_IOCSTATUS_SUCCESS ||
	    rpl->SASStatus != MPI2_SASSTATUS_SUCCESS) {
		mps_dprint(sc, MPS_INFO, "%s: IOCStatus %04x SASStatus %02x\n",
		    __func__, rpl->IOCStatus, rpl->SASStatus);
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;
		goto bailout;
	}

	mps_dprint(sc, MPS_INFO, "%s: SMP request to SAS address "
		   "%#jx completed successfully\n", __func__,
		   (uintmax_t)sasaddr);

	if (ccb->smpio.smp_response[2] == SMP_FR_ACCEPTED)
		ccb->ccb_h.status = CAM_REQ_CMP;
	else
		ccb->ccb_h.status = CAM_SMP_STATUS_ERROR;

bailout:
	/*
	 * We sync in both directions because we had DMAs in the S/G list
	 * in both directions.
	 */
	bus_dmamap_sync(sc->buffer_dmat, cm->cm_dmamap,
			BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->buffer_dmat, cm->cm_dmamap);
	mps_free_command(sc, cm);
	xpt_done(ccb);
}

static void
mpssas_send_smpcmd(struct mpssas_softc *sassc, union ccb *ccb, uint64_t sasaddr)
{
	struct mps_command *cm;
	uint8_t *request, *response;
	MPI2_SMP_PASSTHROUGH_REQUEST *req;
	struct mps_softc *sc;
	struct sglist *sg;
	int error;

	sc = sassc->sc;
	sg = NULL;
	error = 0;

	/*
	 * XXX We don't yet support physical addresses here.
	 */
	if (ccb->ccb_h.flags & (CAM_DATA_PHYS|CAM_SG_LIST_PHYS)) {
		mps_printf(sc, "%s: physical addresses not supported\n",
			   __func__);
		ccb->ccb_h.status = CAM_REQ_INVALID;
		xpt_done(ccb);
		return;
	}

	/*
	 * If the user wants to send an S/G list, check to make sure they
	 * have single buffers.
	 */
	if (ccb->ccb_h.flags & CAM_SCATTER_VALID) {
		/*
		 * The chip does not support more than one buffer for the
		 * request or response.
		 */
	 	if ((ccb->smpio.smp_request_sglist_cnt > 1)
		  || (ccb->smpio.smp_response_sglist_cnt > 1)) {
			mps_printf(sc, "%s: multiple request or response "
				   "buffer segments not supported for SMP\n",
				   __func__);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			xpt_done(ccb);
			return;
		}

		/*
		 * The CAM_SCATTER_VALID flag was originally implemented
		 * for the XPT_SCSI_IO CCB, which only has one data pointer.
		 * We have two.  So, just take that flag to mean that we
		 * might have S/G lists, and look at the S/G segment count
		 * to figure out whether that is the case for each individual
		 * buffer.
		 */
		if (ccb->smpio.smp_request_sglist_cnt != 0) {
			bus_dma_segment_t *req_sg;

			req_sg = (bus_dma_segment_t *)ccb->smpio.smp_request;
			request = (uint8_t *)req_sg[0].ds_addr;
		} else
			request = ccb->smpio.smp_request;

		if (ccb->smpio.smp_response_sglist_cnt != 0) {
			bus_dma_segment_t *rsp_sg;

			rsp_sg = (bus_dma_segment_t *)ccb->smpio.smp_response;
			response = (uint8_t *)rsp_sg[0].ds_addr;
		} else
			response = ccb->smpio.smp_response;
	} else {
		request = ccb->smpio.smp_request;
		response = ccb->smpio.smp_response;
	}

	cm = mps_alloc_command(sc);
	if (cm == NULL) {
		mps_printf(sc, "%s: cannot allocate command\n", __func__);
		ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
		xpt_done(ccb);
		return;
	}

	req = (MPI2_SMP_PASSTHROUGH_REQUEST *)cm->cm_req;
	bzero(req, sizeof(*req));
	req->Function = MPI2_FUNCTION_SMP_PASSTHROUGH;

	/* Allow the chip to use any route to this SAS address. */
	req->PhysicalPort = 0xff;

	req->RequestDataLength = ccb->smpio.smp_request_len;
	req->SGLFlags = 
	    MPI2_SGLFLAGS_SYSTEM_ADDRESS_SPACE | MPI2_SGLFLAGS_SGL_TYPE_MPI;

	mps_dprint(sc, MPS_INFO, "%s: sending SMP request to SAS "
		   "address %#jx\n", __func__, (uintmax_t)sasaddr);

	mpi_init_sge(cm, req, &req->SGL);

	/*
	 * Set up a uio to pass into mps_map_command().  This allows us to
	 * do one map command, and one busdma call in there.
	 */
	cm->cm_uio.uio_iov = cm->cm_iovec;
	cm->cm_uio.uio_iovcnt = 2;
	cm->cm_uio.uio_segflg = UIO_SYSSPACE;

	/*
	 * The read/write flag isn't used by busdma, but set it just in
	 * case.  This isn't exactly accurate, either, since we're going in
	 * both directions.
	 */
	cm->cm_uio.uio_rw = UIO_WRITE;

	cm->cm_iovec[0].iov_base = request;
	cm->cm_iovec[0].iov_len = req->RequestDataLength;
	cm->cm_iovec[1].iov_base = response;
	cm->cm_iovec[1].iov_len = ccb->smpio.smp_response_len;

	cm->cm_uio.uio_resid = cm->cm_iovec[0].iov_len +
			       cm->cm_iovec[1].iov_len;

	/*
	 * Trigger a warning message in mps_data_cb() for the user if we
	 * wind up exceeding two S/G segments.  The chip expects one
	 * segment for the request and another for the response.
	 */
	cm->cm_max_segs = 2;

	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	cm->cm_complete = mpssas_smpio_complete;
	cm->cm_complete_data = ccb;

	/*
	 * Tell the mapping code that we're using a uio, and that this is
	 * an SMP passthrough request.  There is a little special-case
	 * logic there (in mps_data_cb()) to handle the bidirectional
	 * transfer.  
	 */
	cm->cm_flags |= MPS_CM_FLAGS_USE_UIO | MPS_CM_FLAGS_SMP_PASS |
			MPS_CM_FLAGS_DATAIN | MPS_CM_FLAGS_DATAOUT;

	/* The chip data format is little endian. */
	req->SASAddress.High = htole32(sasaddr >> 32);
	req->SASAddress.Low = htole32(sasaddr);

	/*
	 * XXX Note that we don't have a timeout/abort mechanism here.
	 * From the manual, it looks like task management requests only
	 * work for SCSI IO and SATA passthrough requests.  We may need to
	 * have a mechanism to retry requests in the event of a chip reset
	 * at least.  Hopefully the chip will insure that any errors short
	 * of that are relayed back to the driver.
	 */
	error = mps_map_command(sc, cm);
	if ((error != 0) && (error != EINPROGRESS)) {
		mps_printf(sc, "%s: error %d returned from mps_map_command()\n",
			   __func__, error);
		goto bailout_error;
	}

	return;

bailout_error:
	mps_free_command(sc, cm);
	ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
	xpt_done(ccb);
	return;

}

static void
mpssas_action_smpio(struct mpssas_softc *sassc, union ccb *ccb)
{
	struct mps_softc *sc;
	struct mpssas_target *targ;
	uint64_t sasaddr = 0;

	sc = sassc->sc;

	/*
	 * Make sure the target exists.
	 */
	targ = &sassc->targets[ccb->ccb_h.target_id];
	if (targ->handle == 0x0) {
		mps_printf(sc, "%s: target %d does not exist!\n", __func__,
			   ccb->ccb_h.target_id);
		ccb->ccb_h.status = CAM_SEL_TIMEOUT;
		xpt_done(ccb);
		return;
	}

	/*
	 * If this device has an embedded SMP target, we'll talk to it
	 * directly.
	 * figure out what the expander's address is.
	 */
	if ((targ->devinfo & MPI2_SAS_DEVICE_INFO_SMP_TARGET) != 0)
		sasaddr = targ->sasaddr;

	/*
	 * If we don't have a SAS address for the expander yet, try
	 * grabbing it from the page 0x83 information cached in the
	 * transport layer for this target.  LSI expanders report the
	 * expander SAS address as the port-associated SAS address in
	 * Inquiry VPD page 0x83.  Maxim expanders don't report it in page
	 * 0x83.
	 *
	 * XXX KDM disable this for now, but leave it commented out so that
	 * it is obvious that this is another possible way to get the SAS
	 * address.
	 *
	 * The parent handle method below is a little more reliable, and
	 * the other benefit is that it works for devices other than SES
	 * devices.  So you can send a SMP request to a da(4) device and it
	 * will get routed to the expander that device is attached to.
	 * (Assuming the da(4) device doesn't contain an SMP target...)
	 */
#if 0
	if (sasaddr == 0)
		sasaddr = xpt_path_sas_addr(ccb->ccb_h.path);
#endif

	/*
	 * If we still don't have a SAS address for the expander, look for
	 * the parent device of this device, which is probably the expander.
	 */
	if (sasaddr == 0) {
		struct mpssas_target *parent_target;

		if (targ->parent_handle == 0x0) {
			mps_printf(sc, "%s: handle %d does not have a valid "
				   "parent handle!\n", __func__, targ->handle);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			goto bailout;
		}
		parent_target = mpssas_find_target(sassc, 0,
						   targ->parent_handle);

		if (parent_target == NULL) {
			mps_printf(sc, "%s: handle %d does not have a valid "
				   "parent target!\n", __func__, targ->handle);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			goto bailout;
		}

		if ((parent_target->devinfo &
		     MPI2_SAS_DEVICE_INFO_SMP_TARGET) == 0) {
			mps_printf(sc, "%s: handle %d parent %d does not "
				   "have an SMP target!\n", __func__,
				   targ->handle, parent_target->handle);
			ccb->ccb_h.status = CAM_REQ_INVALID;
			goto bailout;

		}

		sasaddr = parent_target->sasaddr;
	}

	if (sasaddr == 0) {
		mps_printf(sc, "%s: unable to find SAS address for handle %d\n",
			   __func__, targ->handle);
		ccb->ccb_h.status = CAM_REQ_INVALID;
		goto bailout;
	}
	mpssas_send_smpcmd(sassc, ccb, sasaddr);

	return;

bailout:
	xpt_done(ccb);

}

#endif /* __FreeBSD_version >= 900026 */

static void
mpssas_action_resetdev(struct mpssas_softc *sassc, union ccb *ccb)
{
	struct mps_softc *sc;
	struct mps_command *cm;
	struct mpssas_target *targ;

	sc = sassc->sc;
	targ = &sassc->targets[ccb->ccb_h.target_id];

	if (targ->flags & MPSSAS_TARGET_INRECOVERY) {
		ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
		xpt_done(ccb);
		return;
	}

	cm = mps_alloc_command(sc);
	if (cm == NULL) {
		mps_printf(sc, "%s: cannot alloc command\n", __func__);
		ccb->ccb_h.status = CAM_RESRC_UNAVAIL;
		xpt_done(ccb);
		return;
	}

	cm->cm_targ = targ;
	cm->cm_complete = mpssas_resetdev_complete;
	cm->cm_complete_data = ccb;

	mpssas_resetdev(sassc, cm);
}

static void
mpssas_resetdev(struct mpssas_softc *sassc, struct mps_command *cm)
{
	MPI2_SCSI_TASK_MANAGE_REQUEST *req;
	struct mps_softc *sc;

	mps_dprint(sassc->sc, MPS_TRACE, "%s\n", __func__);

	sc = sassc->sc;

	req = (MPI2_SCSI_TASK_MANAGE_REQUEST *)cm->cm_req;
	req->DevHandle = cm->cm_targ->handle;
	req->Function = MPI2_FUNCTION_SCSI_TASK_MGMT;
	req->TaskType = MPI2_SCSITASKMGMT_TASKTYPE_TARGET_RESET;

	/* SAS Hard Link Reset / SATA Link Reset */
	req->MsgFlags = MPI2_SCSITASKMGMT_MSGFLAGS_LINK_RESET;

	cm->cm_data = NULL;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;

	mpssas_issue_tm_request(sc, cm);
}

static void
mpssas_resetdev_complete(struct mps_softc *sc, struct mps_command *cm)
{
	MPI2_SCSI_TASK_MANAGE_REPLY *resp;
	union ccb *ccb;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	resp = (MPI2_SCSI_TASK_MANAGE_REPLY *)cm->cm_reply;
	ccb = cm->cm_complete_data;

	printf("resetdev complete IOCStatus= 0x%x ResponseCode= 0x%x\n",
	    resp->IOCStatus, resp->ResponseCode);

	if (resp->ResponseCode == MPI2_SCSITASKMGMT_RSP_TM_COMPLETE)
		ccb->ccb_h.status = CAM_REQ_CMP;
	else
		ccb->ccb_h.status = CAM_REQ_CMP_ERR;

	mpssas_complete_tm_request(sc, cm, /*free_cm*/ 1);

	xpt_done(ccb);
}

static void
mpssas_poll(struct cam_sim *sim)
{
	struct mpssas_softc *sassc;

	sassc = cam_sim_softc(sim);
	mps_intr_locked(sassc->sc);
}

static void
mpssas_freeze_device(struct mpssas_softc *sassc, struct mpssas_target *targ)
{
}

static void
mpssas_unfreeze_device(struct mpssas_softc *sassc, struct mpssas_target *targ)
{
}

