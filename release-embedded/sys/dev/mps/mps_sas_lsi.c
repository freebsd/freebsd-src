/*-
 * Copyright (c) 2011, 2012 LSI Corp.
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
 *
 * LSI MPT-Fusion Host Adapter FreeBSD
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

/* Communications core for LSI MPT2 */

/* TODO Move headers to mpsvar */
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
#include <sys/endian.h>
#include <sys/queue.h>
#include <sys/kthread.h>
#include <sys/taskqueue.h>
#include <sys/sbuf.h>

#include <machine/bus.h>
#include <machine/resource.h>
#include <sys/rman.h>

#include <machine/stdarg.h>

#include <cam/cam.h>
#include <cam/cam_ccb.h>
#include <cam/cam_debug.h>
#include <cam/cam_sim.h>
#include <cam/cam_xpt_sim.h>
#include <cam/cam_xpt_periph.h>
#include <cam/cam_periph.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_message.h>

#include <dev/mps/mpi/mpi2_type.h>
#include <dev/mps/mpi/mpi2.h>
#include <dev/mps/mpi/mpi2_ioc.h>
#include <dev/mps/mpi/mpi2_sas.h>
#include <dev/mps/mpi/mpi2_cnfg.h>
#include <dev/mps/mpi/mpi2_init.h>
#include <dev/mps/mpi/mpi2_raid.h>
#include <dev/mps/mpi/mpi2_tool.h>
#include <dev/mps/mps_ioctl.h>
#include <dev/mps/mpsvar.h>
#include <dev/mps/mps_table.h>
#include <dev/mps/mps_sas.h>

/* For Hashed SAS Address creation for SATA Drives */
#define MPT2SAS_SN_LEN 20
#define MPT2SAS_MN_LEN 40

struct mps_fw_event_work {
	u16			event;
	void			*event_data;
	TAILQ_ENTRY(mps_fw_event_work)	ev_link;
};

union _sata_sas_address {
	u8 wwid[8];
	struct {
		u32 high;
		u32 low;
	} word;
};

/*
 * define the IDENTIFY DEVICE structure
 */
struct _ata_identify_device_data {
	u16 reserved1[10];	/* 0-9 */
	u16 serial_number[10];	/* 10-19 */
	u16 reserved2[7];	/* 20-26 */
	u16 model_number[20];	/* 27-46*/
	u16 reserved3[209];	/* 47-255*/
};
static u32 event_count;
static void mpssas_fw_work(struct mps_softc *sc,
    struct mps_fw_event_work *fw_event);
static void mpssas_fw_event_free(struct mps_softc *,
    struct mps_fw_event_work *);
static int mpssas_add_device(struct mps_softc *sc, u16 handle, u8 linkrate);
static int mpssas_get_sata_identify(struct mps_softc *sc, u16 handle,
    Mpi2SataPassthroughReply_t *mpi_reply, char *id_buffer, int sz,
    u32 devinfo);
int mpssas_get_sas_address_for_sata_disk(struct mps_softc *sc,
    u64 *sas_address, u16 handle, u32 device_info);
static int mpssas_volume_add(struct mps_softc *sc,
    u16 handle);

void
mpssas_evt_handler(struct mps_softc *sc, uintptr_t data,
    MPI2_EVENT_NOTIFICATION_REPLY *event)
{
	struct mps_fw_event_work *fw_event;
	u16 sz;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);
	mps_print_evt_sas(sc, event);
	mpssas_record_event(sc, event);

	fw_event = malloc(sizeof(struct mps_fw_event_work), M_MPT2,
	     M_ZERO|M_NOWAIT);
	if (!fw_event) {
		printf("%s: allocate failed for fw_event\n", __func__);
		return;
	}
	sz = le16toh(event->EventDataLength) * 4;
	fw_event->event_data = malloc(sz, M_MPT2, M_ZERO|M_NOWAIT);
	if (!fw_event->event_data) {
		printf("%s: allocate failed for event_data\n", __func__);
		free(fw_event, M_MPT2);
		return;
	}

	bcopy(event->EventData, fw_event->event_data, sz);
	fw_event->event = event->Event;
	if ((event->Event == MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST ||
	    event->Event == MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE ||
	    event->Event == MPI2_EVENT_IR_CONFIGURATION_CHANGE_LIST) &&
	    sc->track_mapping_events)
		sc->pending_map_events++;

	/*
	 * When wait_for_port_enable flag is set, make sure that all the events
	 * are processed. Increment the startup_refcount and decrement it after
	 * events are processed.
	 */
	if ((event->Event == MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST ||
	    event->Event == MPI2_EVENT_IR_CONFIGURATION_CHANGE_LIST) &&
	    sc->wait_for_port_enable)
		mpssas_startup_increment(sc->sassc);

	TAILQ_INSERT_TAIL(&sc->sassc->ev_queue, fw_event, ev_link);
	taskqueue_enqueue(sc->sassc->ev_tq, &sc->sassc->ev_task);

}

static void
mpssas_fw_event_free(struct mps_softc *sc, struct mps_fw_event_work *fw_event)
{

	free(fw_event->event_data, M_MPT2);
	free(fw_event, M_MPT2);
}

/**
 * _mps_fw_work - delayed task for processing firmware events
 * @sc: per adapter object
 * @fw_event: The fw_event_work object
 * Context: user.
 *
 * Return nothing.
 */
static void
mpssas_fw_work(struct mps_softc *sc, struct mps_fw_event_work *fw_event)
{
	struct mpssas_softc *sassc;
	sassc = sc->sassc;

	mps_dprint(sc, MPS_EVENT, "(%d)->(%s) Working on  Event: [%x]\n",
			event_count++,__func__,fw_event->event);
	switch (fw_event->event) {
	case MPI2_EVENT_SAS_TOPOLOGY_CHANGE_LIST: 
	{
		MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST *data;
		MPI2_EVENT_SAS_TOPO_PHY_ENTRY *phy;
		int i;

		data = (MPI2_EVENT_DATA_SAS_TOPOLOGY_CHANGE_LIST *)
		    fw_event->event_data;

		mps_mapping_topology_change_event(sc, fw_event->event_data);

		for (i = 0; i < data->NumEntries; i++) {
			phy = &data->PHY[i];
			switch (phy->PhyStatus & MPI2_EVENT_SAS_TOPO_RC_MASK) {
			case MPI2_EVENT_SAS_TOPO_RC_TARG_ADDED:
				if (mpssas_add_device(sc,
				    le16toh(phy->AttachedDevHandle), phy->LinkRate)){
					printf("%s: failed to add device with "
					    "handle 0x%x\n", __func__,
					    le16toh(phy->AttachedDevHandle));
					mpssas_prepare_remove(sassc, le16toh(
						phy->AttachedDevHandle));
				}
				break;
			case MPI2_EVENT_SAS_TOPO_RC_TARG_NOT_RESPONDING:
				mpssas_prepare_remove(sassc,le16toh( 
					phy->AttachedDevHandle));
				break;
			case MPI2_EVENT_SAS_TOPO_RC_PHY_CHANGED:
			case MPI2_EVENT_SAS_TOPO_RC_NO_CHANGE:
			case MPI2_EVENT_SAS_TOPO_RC_DELAY_NOT_RESPONDING:
			default:
				break;
			}
		}
		/*
		 * refcount was incremented for this event in
		 * mpssas_evt_handler.  Decrement it here because the event has
		 * been processed.
		 */
		mpssas_startup_decrement(sassc);
		break;
	}
	case MPI2_EVENT_SAS_DISCOVERY:
	{
		MPI2_EVENT_DATA_SAS_DISCOVERY *data;

		data = (MPI2_EVENT_DATA_SAS_DISCOVERY *)fw_event->event_data;

		if (data->ReasonCode & MPI2_EVENT_SAS_DISC_RC_STARTED)
			mps_dprint(sc, MPS_TRACE,"SAS discovery start event\n");
		if (data->ReasonCode & MPI2_EVENT_SAS_DISC_RC_COMPLETED) {
			mps_dprint(sc, MPS_TRACE,"SAS discovery stop event\n");
			sassc->flags &= ~MPSSAS_IN_DISCOVERY;
			mpssas_discovery_end(sassc);
		}
		break;
	}
	case MPI2_EVENT_SAS_ENCL_DEVICE_STATUS_CHANGE:
	{
		Mpi2EventDataSasEnclDevStatusChange_t *data;
		data = (Mpi2EventDataSasEnclDevStatusChange_t *)
		    fw_event->event_data;
		mps_mapping_enclosure_dev_status_change_event(sc,
		    fw_event->event_data);
		break;
	}
	case MPI2_EVENT_IR_CONFIGURATION_CHANGE_LIST:
	{
		Mpi2EventIrConfigElement_t *element;
		int i;
		u8 foreign_config;
		Mpi2EventDataIrConfigChangeList_t *event_data;
		struct mpssas_target *targ;
		unsigned int id;

		event_data = fw_event->event_data;
		foreign_config = (le32toh(event_data->Flags) &
		    MPI2_EVENT_IR_CHANGE_FLAGS_FOREIGN_CONFIG) ? 1 : 0;

		element =
		    (Mpi2EventIrConfigElement_t *)&event_data->ConfigElement[0];
		id = mps_mapping_get_raid_id_from_handle
		    (sc, element->VolDevHandle);

		mps_mapping_ir_config_change_event(sc, event_data);

		for (i = 0; i < event_data->NumElements; i++, element++) {
			switch (element->ReasonCode) {
			case MPI2_EVENT_IR_CHANGE_RC_VOLUME_CREATED:
			case MPI2_EVENT_IR_CHANGE_RC_ADDED:
				if (!foreign_config) {
					if (mpssas_volume_add(sc, le16toh(element->VolDevHandle))){
						printf("%s: failed to add RAID "
						    "volume with handle 0x%x\n",
						    __func__, le16toh(element->
						    VolDevHandle));
					}
				}
				break;
			case MPI2_EVENT_IR_CHANGE_RC_VOLUME_DELETED:
			case MPI2_EVENT_IR_CHANGE_RC_REMOVED:
				/*
				 * Rescan after volume is deleted or removed.
				 */
				if (!foreign_config) {
					if (id == MPS_MAP_BAD_ID) {
						printf("%s: could not get ID "
						    "for volume with handle "
						    "0x%04x\n", __func__,
						    le16toh(element->VolDevHandle));
						break;
					}
					
					targ = &sassc->targets[id];
					targ->handle = 0x0;
					targ->encl_slot = 0x0;
					targ->encl_handle = 0x0;
					targ->exp_dev_handle = 0x0;
					targ->phy_num = 0x0;
					targ->linkrate = 0x0;
					mpssas_rescan_target(sc, targ);
					printf("RAID target id 0x%x removed\n",
					    targ->tid);
				}
				break;
			case MPI2_EVENT_IR_CHANGE_RC_PD_CREATED:
			case MPI2_EVENT_IR_CHANGE_RC_HIDE:
				/*
				 * Phys Disk of a volume has been created.  Hide
				 * it from the OS.
				 */
				targ = mpssas_find_target_by_handle(sassc, 0, element->PhysDiskDevHandle);
				if (targ == NULL) 
					break;
				
				/* Set raid component flags only if it is not WD.
				 * OR WrapDrive with WD_HIDE_ALWAYS/WD_HIDE_IF_VOLUME is set in NVRAM
				 */
				if((!sc->WD_available) ||
				((sc->WD_available && 
				(sc->WD_hide_expose == MPS_WD_HIDE_ALWAYS)) ||
				(sc->WD_valid_config && (sc->WD_hide_expose ==
				MPS_WD_HIDE_IF_VOLUME)))) {
					targ->flags |= MPS_TARGET_FLAGS_RAID_COMPONENT;
				}
				mpssas_rescan_target(sc, targ);
				
				break;
			case MPI2_EVENT_IR_CHANGE_RC_PD_DELETED:
				/*
				 * Phys Disk of a volume has been deleted.
				 * Expose it to the OS.
				 */
				if (mpssas_add_device(sc,
				    le16toh(element->PhysDiskDevHandle), 0)){
					printf("%s: failed to add device with "
					    "handle 0x%x\n", __func__,
					    le16toh(element->PhysDiskDevHandle));
					mpssas_prepare_remove(sassc, le16toh(element->
					    PhysDiskDevHandle));
				}
				break;
			}
		}
		/*
		 * refcount was incremented for this event in
		 * mpssas_evt_handler.  Decrement it here because the event has
		 * been processed.
		 */
		mpssas_startup_decrement(sassc);
		break;
	}
	case MPI2_EVENT_IR_VOLUME:
	{
		Mpi2EventDataIrVolume_t *event_data = fw_event->event_data;

		/*
		 * Informational only.
		 */
		mps_dprint(sc, MPS_EVENT, "Received IR Volume event:\n");
		switch (event_data->ReasonCode) {
		case MPI2_EVENT_IR_VOLUME_RC_SETTINGS_CHANGED:
  			mps_dprint(sc, MPS_EVENT, "   Volume Settings "
  			    "changed from 0x%x to 0x%x for Volome with "
 			    "handle 0x%x", le32toh(event_data->PreviousValue),
 			    le32toh(event_data->NewValue),
 			    le16toh(event_data->VolDevHandle));
			break;
		case MPI2_EVENT_IR_VOLUME_RC_STATUS_FLAGS_CHANGED:
  			mps_dprint(sc, MPS_EVENT, "   Volume Status "
  			    "changed from 0x%x to 0x%x for Volome with "
 			    "handle 0x%x", le32toh(event_data->PreviousValue),
 			    le32toh(event_data->NewValue),
 			    le16toh(event_data->VolDevHandle));
			break;
		case MPI2_EVENT_IR_VOLUME_RC_STATE_CHANGED:
  			mps_dprint(sc, MPS_EVENT, "   Volume State "
  			    "changed from 0x%x to 0x%x for Volome with "
 			    "handle 0x%x", le32toh(event_data->PreviousValue),
 			    le32toh(event_data->NewValue),
 			    le16toh(event_data->VolDevHandle));
				u32 state;
				struct mpssas_target *targ;
				state = le32toh(event_data->NewValue);
				switch (state) {
				case MPI2_RAID_VOL_STATE_MISSING:
				case MPI2_RAID_VOL_STATE_FAILED:
					mpssas_prepare_volume_remove(sassc, event_data->
							VolDevHandle);
					break;
		 
				case MPI2_RAID_VOL_STATE_ONLINE:
				case MPI2_RAID_VOL_STATE_DEGRADED:
				case MPI2_RAID_VOL_STATE_OPTIMAL:
					targ = mpssas_find_target_by_handle(sassc, 0, event_data->VolDevHandle);
					if (targ) {
						printf("%s %d: Volume handle 0x%x is already added \n",
							       	__func__, __LINE__ , event_data->VolDevHandle);
						break;
					}
					if (mpssas_volume_add(sc, le16toh(event_data->VolDevHandle))) {
						printf("%s: failed to add RAID "
							"volume with handle 0x%x\n",
							__func__, le16toh(event_data->
							VolDevHandle));
					}
					break;
				default:
					break;
				}
			break;
		default:
			break;
		}
		break;
	}
	case MPI2_EVENT_IR_PHYSICAL_DISK:
	{
		Mpi2EventDataIrPhysicalDisk_t *event_data =
		    fw_event->event_data;
		struct mpssas_target *targ;

		/*
		 * Informational only.
		 */
		mps_dprint(sc, MPS_EVENT, "Received IR Phys Disk event:\n");
		switch (event_data->ReasonCode) {
		case MPI2_EVENT_IR_PHYSDISK_RC_SETTINGS_CHANGED:
  			mps_dprint(sc, MPS_EVENT, "   Phys Disk Settings "
  			    "changed from 0x%x to 0x%x for Phys Disk Number "
  			    "%d and handle 0x%x at Enclosure handle 0x%x, Slot "
 			    "%d", le32toh(event_data->PreviousValue),
 			    le32toh(event_data->NewValue),
 				event_data->PhysDiskNum,
 			    le16toh(event_data->PhysDiskDevHandle),
 			    le16toh(event_data->EnclosureHandle), le16toh(event_data->Slot));
			break;
		case MPI2_EVENT_IR_PHYSDISK_RC_STATUS_FLAGS_CHANGED:
  			mps_dprint(sc, MPS_EVENT, "   Phys Disk Status changed "
  			    "from 0x%x to 0x%x for Phys Disk Number %d and "
  			    "handle 0x%x at Enclosure handle 0x%x, Slot %d",
 				le32toh(event_data->PreviousValue),
 			    le32toh(event_data->NewValue), event_data->PhysDiskNum,
 			    le16toh(event_data->PhysDiskDevHandle),
 			    le16toh(event_data->EnclosureHandle), le16toh(event_data->Slot));
			break;
		case MPI2_EVENT_IR_PHYSDISK_RC_STATE_CHANGED:
  			mps_dprint(sc, MPS_EVENT, "   Phys Disk State changed "
  			    "from 0x%x to 0x%x for Phys Disk Number %d and "
  			    "handle 0x%x at Enclosure handle 0x%x, Slot %d",
 				le32toh(event_data->PreviousValue),
 			    le32toh(event_data->NewValue), event_data->PhysDiskNum,
 			    le16toh(event_data->PhysDiskDevHandle),
 			    le16toh(event_data->EnclosureHandle), le16toh(event_data->Slot));
			switch (event_data->NewValue) {
				case MPI2_RAID_PD_STATE_ONLINE:
				case MPI2_RAID_PD_STATE_DEGRADED:
				case MPI2_RAID_PD_STATE_REBUILDING:
				case MPI2_RAID_PD_STATE_OPTIMAL:
				case MPI2_RAID_PD_STATE_HOT_SPARE:
					targ = mpssas_find_target_by_handle(sassc, 0, 
							event_data->PhysDiskDevHandle);
					if (targ) {
						if(!sc->WD_available) {
							targ->flags |= MPS_TARGET_FLAGS_RAID_COMPONENT;
							printf("%s %d: Found Target for handle 0x%x.  \n",
							__func__, __LINE__ , event_data->PhysDiskDevHandle);
						} else if ((sc->WD_available && 
							(sc->WD_hide_expose == MPS_WD_HIDE_ALWAYS)) ||
        						(sc->WD_valid_config && (sc->WD_hide_expose ==
        						MPS_WD_HIDE_IF_VOLUME))) {
							targ->flags |= MPS_TARGET_FLAGS_RAID_COMPONENT;
							printf("%s %d: WD: Found Target for handle 0x%x.  \n",
							__func__, __LINE__ , event_data->PhysDiskDevHandle);
						}
 					}  		
				break;
				case MPI2_RAID_PD_STATE_OFFLINE:
				case MPI2_RAID_PD_STATE_NOT_CONFIGURED:
				case MPI2_RAID_PD_STATE_NOT_COMPATIBLE:
				default:
					targ = mpssas_find_target_by_handle(sassc, 0, 
							event_data->PhysDiskDevHandle);
					if (targ) {
						targ->flags |= ~MPS_TARGET_FLAGS_RAID_COMPONENT;
						printf("%s %d: Found Target for handle 0x%x.  \n",
						__func__, __LINE__ , event_data->PhysDiskDevHandle);
					}
				break;
			}
		default:
			break;
		}
		break;
	}
	case MPI2_EVENT_IR_OPERATION_STATUS:
	{
		Mpi2EventDataIrOperationStatus_t *event_data =
		    fw_event->event_data;

		/*
		 * Informational only.
		 */
		mps_dprint(sc, MPS_EVENT, "Received IR Op Status event:\n");
		mps_dprint(sc, MPS_EVENT, "   RAID Operation of %d is %d "
		    "percent complete for Volume with handle 0x%x",
		    event_data->RAIDOperation, event_data->PercentComplete,
		    le16toh(event_data->VolDevHandle));
		break;
	}
	case MPI2_EVENT_LOG_ENTRY_ADDED:
	{
		pMpi2EventDataLogEntryAdded_t	logEntry;
		uint16_t			logQualifier;
		uint8_t				logCode;

		logEntry = (pMpi2EventDataLogEntryAdded_t)fw_event->event_data;
		logQualifier = logEntry->LogEntryQualifier;

		if (logQualifier == MPI2_WD_LOG_ENTRY) {
			logCode = logEntry->LogData[0];

			switch (logCode) {
			case MPI2_WD_SSD_THROTTLING:
				printf("WarpDrive Warning: IO Throttling has "
				    "occurred in the WarpDrive subsystem. "
				    "Check WarpDrive documentation for "
				    "additional details\n");
				break;
			case MPI2_WD_DRIVE_LIFE_WARN:
				printf("WarpDrive Warning: Program/Erase "
				    "Cycles for the WarpDrive subsystem in "
				    "degraded range. Check WarpDrive "
				    "documentation for additional details\n");
				break;
			case MPI2_WD_DRIVE_LIFE_DEAD:
				printf("WarpDrive Fatal Error: There are no "
				    "Program/Erase Cycles for the WarpDrive "
				    "subsystem. The storage device will be in "
				    "read-only mode. Check WarpDrive "
				    "documentation for additional details\n");
				break;
			case MPI2_WD_RAIL_MON_FAIL:
				printf("WarpDrive Fatal Error: The Backup Rail "
				    "Monitor has failed on the WarpDrive "
				    "subsystem. Check WarpDrive documentation "
				    "for additional details\n");
				break;
			default:
				break;
			}
		}
		break;
	}
	case MPI2_EVENT_SAS_DEVICE_STATUS_CHANGE:
	case MPI2_EVENT_SAS_BROADCAST_PRIMITIVE:
	default:
		mps_dprint(sc, MPS_TRACE,"Unhandled event 0x%0X\n",
		    fw_event->event);
		break;

	}
	mps_dprint(sc, MPS_EVENT, "(%d)->(%s) Event Free: [%x]\n",event_count,__func__, fw_event->event);
	mpssas_fw_event_free(sc, fw_event);
}

void
mpssas_firmware_event_work(void *arg, int pending)
{
	struct mps_fw_event_work *fw_event;
	struct mps_softc *sc;

	sc = (struct mps_softc *)arg;
	mps_lock(sc);
	while ((fw_event = TAILQ_FIRST(&sc->sassc->ev_queue)) != NULL) {
		TAILQ_REMOVE(&sc->sassc->ev_queue, fw_event, ev_link);
		mpssas_fw_work(sc, fw_event);
	}
	mps_unlock(sc);
}

static int
mpssas_add_device(struct mps_softc *sc, u16 handle, u8 linkrate){
	char devstring[80];
	struct mpssas_softc *sassc;
	struct mpssas_target *targ;
	Mpi2ConfigReply_t mpi_reply;
	Mpi2SasDevicePage0_t config_page;
	uint64_t sas_address, sata_sas_address;
	uint64_t parent_sas_address = 0;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);
	u32 device_info, parent_devinfo = 0;
	unsigned int id;
	int ret;
	int error = 0;
	struct mpssas_lun *lun;

	sassc = sc->sassc;
	mpssas_startup_increment(sassc);
	if ((mps_config_get_sas_device_pg0(sc, &mpi_reply, &config_page,
	     MPI2_SAS_DEVICE_PGAD_FORM_HANDLE, handle))) {
		printf("%s: error reading SAS device page0\n", __func__);
		error = ENXIO;
		goto out;
	}

	device_info = le32toh(config_page.DeviceInfo);

	if (((device_info & MPI2_SAS_DEVICE_INFO_SMP_TARGET) == 0)
	 && (le16toh(config_page.ParentDevHandle) != 0)) {
		Mpi2ConfigReply_t tmp_mpi_reply;
		Mpi2SasDevicePage0_t parent_config_page;

		if ((mps_config_get_sas_device_pg0(sc, &tmp_mpi_reply,
		     &parent_config_page, MPI2_SAS_DEVICE_PGAD_FORM_HANDLE,
		     le16toh(config_page.ParentDevHandle)))) {
			printf("%s: error reading SAS device %#x page0\n",
			       __func__, le16toh(config_page.ParentDevHandle));
		} else {
			parent_sas_address = parent_config_page.SASAddress.High;
			parent_sas_address = (parent_sas_address << 32) |
				parent_config_page.SASAddress.Low;
			parent_devinfo = le32toh(parent_config_page.DeviceInfo);
		}
	}
	/* TODO Check proper endianess */
	sas_address = config_page.SASAddress.High;
	sas_address = (sas_address << 32) | 
	    config_page.SASAddress.Low;

	if ((ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_MASK_MAPPING_MODE)
		    == MPI2_IOCPAGE8_FLAGS_DEVICE_PERSISTENCE_MAPPING) {
		if (device_info & MPI2_SAS_DEVICE_INFO_SATA_DEVICE) {
			ret = mpssas_get_sas_address_for_sata_disk(sc,
			    &sata_sas_address, handle, device_info);
			if (!ret)
				id = mps_mapping_get_sas_id(sc,
				    sata_sas_address, handle);
			else
				id = mps_mapping_get_sas_id(sc,
				    sas_address, handle);
		} else
			id = mps_mapping_get_sas_id(sc, sas_address,
			    handle);
	} else
		id = mps_mapping_get_sas_id(sc, sas_address, handle);

	if (id == MPS_MAP_BAD_ID) {
		printf("failure at %s:%d/%s()! Could not get ID for device "
		    "with handle 0x%04x\n", __FILE__, __LINE__, __func__,
		    handle);
		error = ENXIO;
		goto out;
	}

	if (mpssas_check_id(sassc, id) != 0) {
		device_printf(sc->mps_dev, "Excluding target id %d\n", id);
		error = ENXIO;
		goto out;
	}

	mps_dprint(sc, MPS_MAPPING, "SAS Address from SAS device page0 = %jx\n",
	    sas_address);
	targ = &sassc->targets[id];
	targ->devinfo = device_info;
	targ->devname = le32toh(config_page.DeviceName.High);
	targ->devname = (targ->devname << 32) | 
	    le32toh(config_page.DeviceName.Low);
	targ->encl_handle = le16toh(config_page.EnclosureHandle);
	targ->encl_slot = le16toh(config_page.Slot);
	targ->handle = handle;
	targ->parent_handle = le16toh(config_page.ParentDevHandle);
	targ->sasaddr = mps_to_u64(&config_page.SASAddress);
	targ->parent_sasaddr = le64toh(parent_sas_address);
	targ->parent_devinfo = parent_devinfo;
	targ->tid = id;
	targ->linkrate = (linkrate>>4);
	targ->flags = 0;
	TAILQ_INIT(&targ->commands);
	TAILQ_INIT(&targ->timedout_commands);
	while(!SLIST_EMPTY(&targ->luns)) {
		lun = SLIST_FIRST(&targ->luns);
		SLIST_REMOVE_HEAD(&targ->luns, lun_link);
		free(lun, M_MPT2);
	}
	SLIST_INIT(&targ->luns);

	mps_describe_devinfo(targ->devinfo, devstring, 80);
	mps_dprint(sc, MPS_MAPPING, "Found device <%s> <%s> <0x%04x> <%d/%d>\n", devstring,
	    mps_describe_table(mps_linkrate_names, targ->linkrate),
	    targ->handle, targ->encl_handle, targ->encl_slot);

#if __FreeBSD_version < 1000039
	if ((sassc->flags & MPSSAS_IN_STARTUP) == 0)
#endif
		mpssas_rescan_target(sc, targ);
	mps_dprint(sc, MPS_MAPPING, "Target id 0x%x added\n", targ->tid);
out:
	mpssas_startup_decrement(sassc);
	return (error);
	
}
	
int
mpssas_get_sas_address_for_sata_disk(struct mps_softc *sc,
    u64 *sas_address, u16 handle, u32 device_info)
{
	Mpi2SataPassthroughReply_t mpi_reply;
	int i, rc, try_count;
	u32 *bufferptr;
	union _sata_sas_address hash_address;
	struct _ata_identify_device_data ata_identify;
	u8 buffer[MPT2SAS_MN_LEN + MPT2SAS_SN_LEN];
	u32 ioc_status;
	u8 sas_status;

	memset(&ata_identify, 0, sizeof(ata_identify));
	try_count = 0;
	do {
		rc = mpssas_get_sata_identify(sc, handle, &mpi_reply,
		    (char *)&ata_identify, sizeof(ata_identify), device_info);
		try_count++;
		ioc_status = le16toh(mpi_reply.IOCStatus)
		    & MPI2_IOCSTATUS_MASK;
		sas_status = mpi_reply.SASStatus;
	} while ((rc == -EAGAIN || ioc_status || sas_status) &&
	    (try_count < 5));

	if (rc == 0 && !ioc_status && !sas_status) {
		mps_dprint(sc, MPS_MAPPING, "%s: got SATA identify successfully "
			   "for handle = 0x%x with try_count = %d\n",
			   __func__, handle, try_count);
	} else {
		mps_dprint(sc, MPS_MAPPING, "%s: handle = 0x%x failed\n",
			   __func__, handle);
		return -1;
	}
	/* Copy & byteswap the 40 byte model number to a buffer */
	for (i = 0; i < MPT2SAS_MN_LEN; i += 2) {
		buffer[i] = ((u8 *)ata_identify.model_number)[i + 1];
		buffer[i + 1] = ((u8 *)ata_identify.model_number)[i];
	}
	/* Copy & byteswap the 20 byte serial number to a buffer */
	for (i = 0; i < MPT2SAS_SN_LEN; i += 2) {
		buffer[MPT2SAS_MN_LEN + i] =
			((u8 *)ata_identify.serial_number)[i + 1];
		buffer[MPT2SAS_MN_LEN + i + 1] =
			((u8 *)ata_identify.serial_number)[i];
	}
	bufferptr = (u32 *)buffer;
	/* There are 60 bytes to hash down to 8. 60 isn't divisible by 8,
	 * so loop through the first 56 bytes (7*8),
	 * and then add in the last dword.
	 */
	hash_address.word.low  = 0;
	hash_address.word.high = 0;
	for (i = 0; (i < ((MPT2SAS_MN_LEN+MPT2SAS_SN_LEN)/8)); i++) {
		hash_address.word.low += *bufferptr;
		bufferptr++;
		hash_address.word.high += *bufferptr;
		bufferptr++;
	}
	/* Add the last dword */
	hash_address.word.low += *bufferptr;
	/* Make sure the hash doesn't start with 5, because it could clash
	 * with a SAS address. Change 5 to a D.
	 */
	if ((hash_address.word.high & 0x000000F0) == (0x00000050))
		hash_address.word.high |= 0x00000080;
	*sas_address = (u64)hash_address.wwid[0] << 56 |
	    (u64)hash_address.wwid[1] << 48 | (u64)hash_address.wwid[2] << 40 |
	    (u64)hash_address.wwid[3] << 32 | (u64)hash_address.wwid[4] << 24 |
	    (u64)hash_address.wwid[5] << 16 | (u64)hash_address.wwid[6] <<  8 |
	    (u64)hash_address.wwid[7];
	return 0;
}

static int
mpssas_get_sata_identify(struct mps_softc *sc, u16 handle,
    Mpi2SataPassthroughReply_t *mpi_reply, char *id_buffer, int sz, u32 devinfo)
{
	Mpi2SataPassthroughRequest_t *mpi_request;
	Mpi2SataPassthroughReply_t *reply;
	struct mps_command *cm;
	char *buffer;
	int error = 0;

	buffer = malloc( sz, M_MPT2, M_NOWAIT | M_ZERO);
	if (!buffer)
		return ENOMEM;

	if ((cm = mps_alloc_command(sc)) == NULL) {
		free(buffer, M_MPT2);
		return (EBUSY);
	}
	mpi_request = (MPI2_SATA_PASSTHROUGH_REQUEST *)cm->cm_req;
	bzero(mpi_request,sizeof(MPI2_SATA_PASSTHROUGH_REQUEST));
	mpi_request->Function = MPI2_FUNCTION_SATA_PASSTHROUGH;
	mpi_request->VF_ID = 0;
	mpi_request->DevHandle = htole16(handle);
	mpi_request->PassthroughFlags = (MPI2_SATA_PT_REQ_PT_FLAGS_PIO |
	    MPI2_SATA_PT_REQ_PT_FLAGS_READ);
	mpi_request->DataLength = htole32(sz);
	mpi_request->CommandFIS[0] = 0x27;
	mpi_request->CommandFIS[1] = 0x80;
	mpi_request->CommandFIS[2] =  (devinfo &
	    MPI2_SAS_DEVICE_INFO_ATAPI_DEVICE) ? 0xA1 : 0xEC;
	cm->cm_sge = &mpi_request->SGL;
	cm->cm_sglsize = sizeof(MPI2_SGE_IO_UNION);
	cm->cm_flags = MPS_CM_FLAGS_SGE_SIMPLE | MPS_CM_FLAGS_DATAIN;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	cm->cm_data = buffer;
	cm->cm_length = htole32(sz);
 	error = mps_wait_command(sc, cm, 60, CAN_SLEEP);
	reply = (Mpi2SataPassthroughReply_t *)cm->cm_reply;
	if (error || (reply == NULL)) {
		/* FIXME */
 		/*
 		 * If the request returns an error then we need to do a diag
 		 * reset
 		 */ 
 		printf("%s: request for page completed with error %d",
		    __func__, error);
		error = ENXIO;
		goto out;
	}
	bcopy(buffer, id_buffer, sz);
	bcopy(reply, mpi_reply, sizeof(Mpi2SataPassthroughReply_t));
	if ((le16toh(reply->IOCStatus) & MPI2_IOCSTATUS_MASK) !=
	    MPI2_IOCSTATUS_SUCCESS) {
		printf("%s: error reading SATA PASSTHRU; iocstatus = 0x%x\n",
		    __func__, reply->IOCStatus);
		error = ENXIO;
		goto out;
	}
out:
	mps_free_command(sc, cm);
	free(buffer, M_MPT2);	
	return (error);
}

static int
mpssas_volume_add(struct mps_softc *sc, u16 handle)
{
	struct mpssas_softc *sassc;
	struct mpssas_target *targ;
	u64 wwid;
	unsigned int id;
	int error = 0;
	struct mpssas_lun *lun;

	sassc = sc->sassc;
	mpssas_startup_increment(sassc);
	/* wwid is endian safe */
	mps_config_get_volume_wwid(sc, handle, &wwid);
	if (!wwid) {
		printf("%s: invalid WWID; cannot add volume to mapping table\n",
		    __func__);
		error = ENXIO;
		goto out;
	}

	id = mps_mapping_get_raid_id(sc, wwid, handle);
	if (id == MPS_MAP_BAD_ID) {
		printf("%s: could not get ID for volume with handle 0x%04x and "
		    "WWID 0x%016llx\n", __func__, handle,
		    (unsigned long long)wwid);
		error = ENXIO;
		goto out;
	}

	targ = &sassc->targets[id];
	targ->tid = id;
	targ->handle = handle;
	targ->devname = wwid;
	TAILQ_INIT(&targ->commands);
	TAILQ_INIT(&targ->timedout_commands);
	while(!SLIST_EMPTY(&targ->luns)) {
		lun = SLIST_FIRST(&targ->luns);
		SLIST_REMOVE_HEAD(&targ->luns, lun_link);
		free(lun, M_MPT2);
	}
	SLIST_INIT(&targ->luns);
#if __FreeBSD_version < 1000039
	if ((sassc->flags & MPSSAS_IN_STARTUP) == 0)
#endif
		mpssas_rescan_target(sc, targ);
	mps_dprint(sc, MPS_MAPPING, "RAID target id %d added (WWID = 0x%jx)\n",
	    targ->tid, wwid);
out:
	mpssas_startup_decrement(sassc);
	return (error);
}

/**
 * mpssas_ir_shutdown - IR shutdown notification
 * @sc: per adapter object
 *
 * Sending RAID Action to alert the Integrated RAID subsystem of the IOC that
 * the host system is shutting down.
 *
 * Return nothing.
 */
void
mpssas_ir_shutdown(struct mps_softc *sc)
{
	u16 volume_mapping_flags;
	u16 ioc_pg8_flags = le16toh(sc->ioc_pg8.Flags);
	struct dev_mapping_table *mt_entry;
	u32 start_idx, end_idx;
	unsigned int id, found_volume = 0;
	struct mps_command *cm;
	Mpi2RaidActionRequest_t	*action;

	mps_dprint(sc, MPS_TRACE, "%s\n", __func__);

	/* is IR firmware build loaded? */
	if (!sc->ir_firmware)
		return;

	/* are there any volumes?  Look at IR target IDs. */
	// TODO-later, this should be looked up in the RAID config structure
	// when it is implemented.
	volume_mapping_flags = le16toh(sc->ioc_pg8.IRVolumeMappingFlags) &
	    MPI2_IOCPAGE8_IRFLAGS_MASK_VOLUME_MAPPING_MODE;
	if (volume_mapping_flags == MPI2_IOCPAGE8_IRFLAGS_LOW_VOLUME_MAPPING) {
		start_idx = 0;
		if (ioc_pg8_flags & MPI2_IOCPAGE8_FLAGS_RESERVED_TARGETID_0)
			start_idx = 1;
	} else
		start_idx = sc->max_devices - sc->max_volumes;
	end_idx = start_idx + sc->max_volumes - 1;

	for (id = start_idx; id < end_idx; id++) {
		mt_entry = &sc->mapping_table[id];
		if ((mt_entry->physical_id != 0) &&
		    (mt_entry->missing_count == 0)) {
			found_volume = 1;
			break;
		}
	}

	if (!found_volume)
		return;

	if ((cm = mps_alloc_command(sc)) == NULL) {
		printf("%s: command alloc failed\n", __func__);
		return;
	}

	action = (MPI2_RAID_ACTION_REQUEST *)cm->cm_req;
	action->Function = MPI2_FUNCTION_RAID_ACTION;
	action->Action = MPI2_RAID_ACTION_SYSTEM_SHUTDOWN_INITIATED;
	cm->cm_desc.Default.RequestFlags = MPI2_REQ_DESCRIPT_FLAGS_DEFAULT_TYPE;
	mps_lock(sc);
 	mps_wait_command(sc, cm, 5, CAN_SLEEP);
	mps_unlock(sc);

	/*
	 * Don't check for reply, just leave.
	 */
	if (cm)
		mps_free_command(sc, cm);
}
