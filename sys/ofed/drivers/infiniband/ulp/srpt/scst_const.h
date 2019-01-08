/*
 *  include/scst_const.h
 *
 *  Copyright (C) 2004 - 2009 Vladislav Bolkhovitin <vst@vlnb.net>
 *  Copyright (C) 2007 - 2009 ID7 Ltd.
 *
 *  Contains common SCST constants.
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation, version 2
 *  of the License.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 */

#ifndef __SCST_CONST_H
#define __SCST_CONST_H

#include <scsi/scsi.h>

#define SCST_CONST_VERSION "$Revision: 804 $"

/*** Shared constants between user and kernel spaces ***/

/* Max size of CDB */
#define SCST_MAX_CDB_SIZE            16

/* Max size of various names */
#define SCST_MAX_NAME		     50

/*
 * Size of sense sufficient to carry standard sense data.
 * Warning! It's allocated on stack!
 */
#define SCST_STANDARD_SENSE_LEN      17

/* Max size of sense */
#define SCST_SENSE_BUFFERSIZE        96

/*************************************************************
 ** Allowed delivery statuses for cmd's delivery_status
 *************************************************************/

#define SCST_CMD_DELIVERY_SUCCESS	0
#define SCST_CMD_DELIVERY_FAILED	-1
#define SCST_CMD_DELIVERY_ABORTED	-2

/*************************************************************
 ** Values for task management functions
 *************************************************************/
#define SCST_ABORT_TASK              0
#define SCST_ABORT_TASK_SET          1
#define SCST_CLEAR_ACA               2
#define SCST_CLEAR_TASK_SET          3
#define SCST_LUN_RESET               4
#define SCST_TARGET_RESET            5

/** SCST extensions **/

/*
 * Notifies about I_T nexus loss event in the corresponding session.
 * Aborts all tasks there, resets the reservation, if any, and sets
 * up the I_T Nexus loss UA.
 */
#define SCST_NEXUS_LOSS_SESS         6

/* Aborts all tasks in the corresponding session */
#define SCST_ABORT_ALL_TASKS_SESS    7

/*
 * Notifies about I_T nexus loss event. Aborts all tasks in all sessions
 * of the tgt, resets the reservations, if any,  and sets up the I_T Nexus
 * loss UA.
 */
#define SCST_NEXUS_LOSS              8

/* Aborts all tasks in all sessions of the tgt */
#define SCST_ABORT_ALL_TASKS         9

/*
 * Internal TM command issued by SCST in scst_unregister_session(). It is the
 * same as SCST_NEXUS_LOSS_SESS, except:
 *  - it doesn't call task_mgmt_affected_cmds_done()
 *  - it doesn't call task_mgmt_fn_done()
 *  - it doesn't queue NEXUS LOSS UA.
 *
 * Target driver shall NEVER use it!!
 */
#define SCST_UNREG_SESS_TM           10

/*************************************************************
 ** Values for mgmt cmd's status field. Codes taken from iSCSI
 *************************************************************/
#define SCST_MGMT_STATUS_SUCCESS		0
#define SCST_MGMT_STATUS_TASK_NOT_EXIST		-1
#define SCST_MGMT_STATUS_LUN_NOT_EXIST		-2
#define SCST_MGMT_STATUS_FN_NOT_SUPPORTED	-5
#define SCST_MGMT_STATUS_REJECTED		-255
#define SCST_MGMT_STATUS_FAILED			-129

/*************************************************************
 ** SCSI task attribute queue types
 *************************************************************/
enum scst_cmd_queue_type {
	SCST_CMD_QUEUE_UNTAGGED = 0,
	SCST_CMD_QUEUE_SIMPLE,
	SCST_CMD_QUEUE_ORDERED,
	SCST_CMD_QUEUE_HEAD_OF_QUEUE,
	SCST_CMD_QUEUE_ACA
};

/*************************************************************
 ** Data direction aliases
 *************************************************************/
#define SCST_DATA_UNKNOWN			0
#define SCST_DATA_WRITE				1
#define SCST_DATA_READ				2
#define SCST_DATA_NONE				3

/*************************************************************
 ** Name of the "Default" security group
 *************************************************************/
#define SCST_DEFAULT_ACG_NAME			"Default"

/*************************************************************
 ** Sense manipulation and examination
 *************************************************************/
#define SCST_LOAD_SENSE(key_asc_ascq) key_asc_ascq

#define SCST_SENSE_VALID(sense)  ((sense != NULL) && \
				  ((((const uint8_t *)(sense))[0] & 0x70) == 0x70))

#define SCST_NO_SENSE(sense)     ((sense != NULL) && \
				  (((const uint8_t *)(sense))[2] == 0))

static inline int scst_is_ua_sense(const uint8_t *sense)
{
	return SCST_SENSE_VALID(sense) && (sense[2] == UNIT_ATTENTION);
}

/*************************************************************
 ** Sense data for the appropriate errors. Can be used with
 ** scst_set_cmd_error()
 *************************************************************/
#define scst_sense_no_sense			NO_SENSE,        0x00, 0
#define scst_sense_hardw_error			HARDWARE_ERROR,  0x44, 0
#define scst_sense_aborted_command		ABORTED_COMMAND, 0x00, 0
#define scst_sense_invalid_opcode		ILLEGAL_REQUEST, 0x20, 0
#define scst_sense_invalid_field_in_cdb		ILLEGAL_REQUEST, 0x24, 0
#define scst_sense_invalid_field_in_parm_list	ILLEGAL_REQUEST, 0x26, 0
#define scst_sense_reset_UA			UNIT_ATTENTION,  0x29, 0
#define scst_sense_nexus_loss_UA		UNIT_ATTENTION,  0x29, 0x7
#define scst_sense_saving_params_unsup		ILLEGAL_REQUEST, 0x39, 0
#define scst_sense_lun_not_supported		ILLEGAL_REQUEST, 0x25, 0
#define scst_sense_data_protect			DATA_PROTECT,    0x00, 0
#define scst_sense_miscompare_error		MISCOMPARE,      0x1D, 0
#define scst_sense_block_out_range_error	ILLEGAL_REQUEST, 0x21, 0
#define scst_sense_medium_changed_UA		UNIT_ATTENTION,  0x28, 0
#define scst_sense_read_error			MEDIUM_ERROR,    0x11, 0
#define scst_sense_write_error			MEDIUM_ERROR,    0x03, 0
#define scst_sense_not_ready			NOT_READY,       0x04, 0x10
#define scst_sense_invalid_message		ILLEGAL_REQUEST, 0x49, 0
#define scst_sense_cleared_by_another_ini_UA	UNIT_ATTENTION,  0x2F, 0
#define scst_sense_capacity_data_changed	UNIT_ATTENTION,  0x2A, 0x9
#define scst_sense_reported_luns_data_changed	UNIT_ATTENTION,  0x3F, 0xE

/*************************************************************
 * SCSI opcodes not listed anywhere else
 *************************************************************/
#ifndef REPORT_DEVICE_IDENTIFIER
#define REPORT_DEVICE_IDENTIFIER    0xA3
#endif
#ifndef INIT_ELEMENT_STATUS
#define INIT_ELEMENT_STATUS         0x07
#endif
#ifndef INIT_ELEMENT_STATUS_RANGE
#define INIT_ELEMENT_STATUS_RANGE   0x37
#endif
#ifndef PREVENT_ALLOW_MEDIUM
#define PREVENT_ALLOW_MEDIUM        0x1E
#endif
#ifndef READ_ATTRIBUTE
#define READ_ATTRIBUTE              0x8C
#endif
#ifndef REQUEST_VOLUME_ADDRESS
#define REQUEST_VOLUME_ADDRESS      0xB5
#endif
#ifndef WRITE_ATTRIBUTE
#define WRITE_ATTRIBUTE             0x8D
#endif
#ifndef WRITE_VERIFY_16
#define WRITE_VERIFY_16             0x8E
#endif
#ifndef VERIFY_6
#define VERIFY_6                    0x13
#endif
#ifndef VERIFY_12
#define VERIFY_12                   0xAF
#endif
#ifndef READ_16
#define READ_16               0x88
#endif
#ifndef WRITE_16
#define WRITE_16              0x8a
#endif
#ifndef VERIFY_16
#define VERIFY_16	      0x8f
#endif
#ifndef SERVICE_ACTION_IN
#define SERVICE_ACTION_IN     0x9e
#endif
#ifndef SAI_READ_CAPACITY_16
/* values for service action in */
#define	SAI_READ_CAPACITY_16  0x10
#endif
#ifndef MI_REPORT_TARGET_PGS
/* values for maintenance in */
#define MI_REPORT_TARGET_PGS  0x0a
#endif
#ifndef REPORT_LUNS
#define REPORT_LUNS           0xa0
#endif

/*************************************************************
 **  SCSI Architecture Model (SAM) Status codes. Taken from SAM-3 draft
 **  T10/1561-D Revision 4 Draft dated 7th November 2002.
 *************************************************************/
#define SAM_STAT_GOOD            0x00
#define SAM_STAT_CHECK_CONDITION 0x02
#define SAM_STAT_CONDITION_MET   0x04
#define SAM_STAT_BUSY            0x08
#define SAM_STAT_INTERMEDIATE    0x10
#define SAM_STAT_INTERMEDIATE_CONDITION_MET 0x14
#define SAM_STAT_RESERVATION_CONFLICT 0x18
#define SAM_STAT_COMMAND_TERMINATED 0x22	/* obsolete in SAM-3 */
#define SAM_STAT_TASK_SET_FULL   0x28
#define SAM_STAT_ACA_ACTIVE      0x30
#define SAM_STAT_TASK_ABORTED    0x40

/*************************************************************
 ** Control byte field in CDB
 *************************************************************/
#ifndef CONTROL_BYTE_LINK_BIT
#define CONTROL_BYTE_LINK_BIT       0x01
#endif
#ifndef CONTROL_BYTE_NACA_BIT
#define CONTROL_BYTE_NACA_BIT       0x04
#endif

/*************************************************************
 ** Byte 1 in INQUIRY CDB
 *************************************************************/
#define SCST_INQ_EVPD                0x01

/*************************************************************
 ** Byte 3 in Standard INQUIRY data
 *************************************************************/
#define SCST_INQ_BYTE3               3

#define SCST_INQ_NORMACA_BIT         0x20

/*************************************************************
 ** Byte 2 in RESERVE_10 CDB
 *************************************************************/
#define SCST_RES_3RDPTY              0x10
#define SCST_RES_LONGID              0x02

/*************************************************************
 ** Values for the control mode page TST field
 *************************************************************/
#define SCST_CONTR_MODE_ONE_TASK_SET  0
#define SCST_CONTR_MODE_SEP_TASK_SETS 1

/*******************************************************************
 ** Values for the control mode page QUEUE ALGORITHM MODIFIER field
 *******************************************************************/
#define SCST_CONTR_MODE_QUEUE_ALG_RESTRICTED_REORDER   0
#define SCST_CONTR_MODE_QUEUE_ALG_UNRESTRICTED_REORDER 1

/*************************************************************
 ** Values for the control mode page D_SENSE field
 *************************************************************/
#define SCST_CONTR_MODE_FIXED_SENSE  0
#define SCST_CONTR_MODE_DESCR_SENSE 1

/*************************************************************
 ** Misc SCSI constants
 *************************************************************/
#define SCST_SENSE_ASC_UA_RESET      0x29
#define READ_CAP_LEN		     8
#define READ_CAP16_LEN		     12
#define BYTCHK			     0x02
#define POSITION_LEN_SHORT           20
#define POSITION_LEN_LONG            32

/*************************************************************
 ** Various timeouts
 *************************************************************/
#define SCST_DEFAULT_TIMEOUT			(60 * HZ)

#define SCST_GENERIC_CHANGER_TIMEOUT		(3 * HZ)
#define SCST_GENERIC_CHANGER_LONG_TIMEOUT	(14000 * HZ)

#define SCST_GENERIC_PROCESSOR_TIMEOUT		(3 * HZ)
#define SCST_GENERIC_PROCESSOR_LONG_TIMEOUT	(14000 * HZ)

#define SCST_GENERIC_TAPE_SMALL_TIMEOUT		(3 * HZ)
#define SCST_GENERIC_TAPE_REG_TIMEOUT		(900 * HZ)
#define SCST_GENERIC_TAPE_LONG_TIMEOUT		(14000 * HZ)

#define SCST_GENERIC_MODISK_SMALL_TIMEOUT	(3 * HZ)
#define SCST_GENERIC_MODISK_REG_TIMEOUT		(900 * HZ)
#define SCST_GENERIC_MODISK_LONG_TIMEOUT	(14000 * HZ)

#define SCST_GENERIC_DISK_SMALL_TIMEOUT		(3 * HZ)
#define SCST_GENERIC_DISK_REG_TIMEOUT		(60 * HZ)
#define SCST_GENERIC_DISK_LONG_TIMEOUT		(3600 * HZ)

#define SCST_GENERIC_RAID_TIMEOUT		(3 * HZ)
#define SCST_GENERIC_RAID_LONG_TIMEOUT		(14000 * HZ)

#define SCST_GENERIC_CDROM_SMALL_TIMEOUT	(3 * HZ)
#define SCST_GENERIC_CDROM_REG_TIMEOUT		(900 * HZ)
#define SCST_GENERIC_CDROM_LONG_TIMEOUT		(14000 * HZ)

#define SCST_MAX_OTHER_TIMEOUT			(14000 * HZ)

#endif /* __SCST_CONST_H */
