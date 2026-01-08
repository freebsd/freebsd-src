/*-
 * Copyrtight (c) 2026 Netflix, Inc
 *
 * SPDX-License-Expression: BSD-2-Clause
 */

inline string xpt_action_string[int key] =
	key ==  0 ? "XPT_NOOP" :
	key ==  1 ? "XPT_SCSI_IO" :
	key ==  2 ? "XPT_GDEV_TYPE" :
	key ==  3 ? "XPT_GDEVLIST" :
	key ==  4 ? "XPT_PATH_INQ" :
	key ==  5 ? "XPT_REL_SIMQ" :
	key ==  6 ? "XPT_SASYNC_CB" :
	key ==  7 ? "XPT_SDEV_TYPE" :
	key ==  8 ? "XPT_SCAN_BUS" :
	key ==  9 ? "XPT_DEV_MATCH" :
	key == 10 ? "XPT_DEBUG" :
	key == 11 ? "XPT_PATH_STATS" :
	key == 12 ? "XPT_GDEV_STATS" :
	key == 13 ? "XPT_0X0d" :
	key == 14 ? "XPT_DEV_ADVINFO" :
	key == 15 ? "XPT_ASYNC" :
	key == 16 ? "XPT_ABORT" :
	key == 17 ? "XPT_RESET_BUS" :
	key == 18 ? "XPT_RESET_DEV" :
	key == 19 ? "XPT_TERM_IO" :
	key == 20 ? "XPT_SCAN_LUN" :
	key == 21 ? "XPT_GET_TRAN_SETTINGS" :
	key == 22 ? "XPT_SET_TRAN_SETTINGS" :
	key == 23 ? "XPT_CALC_GEOMETRY" :
	key == 24 ? "XPT_ATA_IO" :
	key == 25 ? "XPT_SET_SIM_KNOB" :
	key == 26 ? "XPT_GET_SIM_KNOB" :
	key == 27 ? "XPT_SMP_IO" :
	key == 28 ? "XPT_NVME_IO" :
	key == 29 ? "XPT_MMC_IO" :
	key == 30 ? "XPT_SCAN_TGT" :
	key == 31 ? "XPT_NVME_ADMIN" :
	"Too big" ;

inline string xpt_async_string[int key] =
	key == 0x1    ? "AC_BUS_RESET" :
	key == 0x2    ? "AC_UNSOL_RESEL" :
	key == 0x4    ? "AC_0x4" :
	key == 0x8    ? "AC_SENT_AEN" :
	key == 0x10   ? "AC_SENT_BDR" :
	key == 0x20   ? "AC_PATH_REGISTERED" :
	key == 0x40   ? "AC_PATH_DEREGISTERED" :
	key == 0x80   ? "AC_FOUND_DEVICE" :
	key == 0x100  ? "AC_LOST_DEVICE" :
	key == 0x200  ? "AC_TRANSFER_NEG" :
	key == 0x400  ? "AC_INQ_CHANGED" :
	key == 0x800  ? "AC_GETDEV_CHANGED" :
	key == 0x1000 ? "AC_CONTRACT" :
	key == 0x2000 ? "AC_ADVINFO_CHANGED" :
	key == 0x4000 ? "AC_UNIT_ATTENTION" :
	"AC UNKNOWN";


inline int CAM_CDB_POINTER = 1;

inline int XPT_OP_MASK		= 0xff;
inline int XPT_NOOP		= 0x00;
inline int XPT_SCSI_IO		= 0x01;
inline int XPT_GDEV_TYPE	= 0x02;
inline int XPT_GDEVLIST		= 0x03;
inline int XPT_PATH_INQ		= 0x04;
inline int XPT_REL_SIMQ		= 0x05;
inline int XPT_SASYNC_CB	= 0x06;
inline int XPT_SDEV_TYPE	= 0x07;
inline int XPT_SCAN_BUS		= 0x08;
inline int XPT_DEV_MATCH	= 0x09;
inline int XPT_DEBUG		= 0x0a;
inline int XPT_PATH_STATS	= 0x0b;
inline int XPT_GDEV_STATS	= 0x0c;
inline int XPT_DEV_ADVINFO	= 0x0e;
inline int XPT_ASYNC		= 0x0f;
inline int XPT_ABORT		= 0x10;
inline int XPT_RESET_BUS	= 0x11;
inline int XPT_RESET_DEV	= 0x12;
inline int XPT_TERM_IO		= 0x13;
inline int XPT_SCAN_LUN		= 0x14;
inline int XPT_GET_TRAN_SETTINGS = 0x15;
inline int XPT_SET_TRAN_SETTINGS = 0x16;
inline int XPT_CALC_GEOMETRY	= 0x17;
inline int XPT_ATA_IO		= 0x18;
inline int XPT_SET_SIM_KNOB	= 0x19;
inline int XPT_GET_SIM_KNOB	= 0x1a;
inline int XPT_SMP_IO		= 0x1b;
inline int XPT_NVME_IO		= 0x1c;
inline int XPT_MMC_IO		= 0x1c;
inline int XPT_SCAN_TGT		= 0x1e;
inline int XPT_NVME_ADMIN	= 0x1f;
inline int XPT_ENG_INQ		= 0x20;
inline int XPT_ENG_EXEC		= 0x21;
inline int XPT_EN_LUN		= 0x30;
inline int XPT_TARGET_IO	= 0x31;
inline int XPT_ACCEPT_TARGET_IO	= 0x32;
inline int XPT_CONT_TARGET_IO	= 0x33;
inline int XPT_IMMED_NOTIFY	= 0x34;
inline int XPT_NOTIFY_ACK	= 0x35;
inline int XPT_IMMEDIATE_NOTIFY	= 0x36;
inline int XPT_NOTIFY_ACKNOWLEDGE = 0x37;
inline int XPT_REPROBE_LUN	= 0x38;
inline int XPT_MMC_SET_TRAN_SETTINGS = 0x40;
inline int XPT_MMC_GET_TRAN_SETTINGS = 0x41;

inline int XPT_FC_QUEUED	= 0x100;
inline int XPT_FC_USER_CCB	= 0x200;
inline int XPT_FC_XPT_ONLY	= 0x400;
inline int XPT_FC_DEV_QUEUED	= 0x800;

inline int PROTO_UNKNOWN = 0;
inline int PROTO_UNSPECIFIED = 1;
inline int PROTO_SCSI = 2;
inline int PROTO_ATA = 3;
inline int PROTO_ATAPI = 4;
inline int PROTO_SATAPM = 5;
inline int PROTO_SEMB = 6;
inline int PROTO_NVME = 7;
inline int PROTO_MMCSD = 8;

inline int XPORT_UNKNOWN = 0;
inline int XPORT_UNSPECIFIED = 1;
inline int XPORT_SPI = 2;
inline int XPORT_FC = 3;
inline int XPORT_SSA = 4;
inline int XPORT_USB = 5;
inline int XPORT_PPB = 6;
inline int XPORT_ATA = 7;
inline int XPORT_SAS = 8;
inline int XPORT_SATA = 9;
inline int XPORT_ISCSI = 10;
inline int XPORT_SRP = 11;
inline int XPORT_NVME = 12;
inline int XPORT_MMCSD = 13;
inline int XPORT_NVMF = 14;
inline int XPORT_UFSHCI = 15;

inline int CAM_REQ_INPROG	= 0x00;
inline int CAM_REQ_CMP		= 0x01;
inline int CAM_REQ_ABORTED	= 0x02;
inline int CAM_UA_ABORT		= 0x03;
inline int CAM_REQ_CMP_ERR	= 0x04;
inline int CAM_BUSY		= 0x05;
inline int CAM_REQ_INVALID	= 0x06;
inline int CAM_PATH_INVALID	= 0x07;
inline int CAM_DEV_NOT_THERE	= 0x08;
inline int CAM_UA_TERMIO	= 0x09;
inline int CAM_SEL_TIMEOUT	= 0x0a;
inline int CAM_CMD_TIMEOUT	= 0x0b;
inline int CAM_SCSI_STATUS_ERROR = 0x0c;
inline int CAM_MSG_REJECT_REC	= 0x0d;
inline int CAM_SCSI_BUS_RESET	= 0x0e;
inline int CAM_UNCOR_PARITY	= 0x0f;
inline int CAM_AUTOSENSE_FAIL	= 0x10;
inline int CAM_NO_HBA		= 0x11;
inline int CAM_DATA_RUN_ERR	= 0x12;
inline int CAM_UNEXP_BUSFREE	= 0x13;
inline int CAM_SEQUENCE_FAIL	= 0x14;
inline int CAM_CCB_LEN_ERR	= 0x15;
inline int CAM_PROVIDE_FAIL	= 0x16;
inline int CAM_BDR_SENT		= 0x17;
inline int CAM_REQ_TERMIO	= 0x18;
inline int CAM_UNREC_HBA_ERROR	= 0x19;
inline int CAM_REQ_TOO_BIG	= 0x1a;
inline int CAM_REQUEUE_REQ	= 0x1b;
inline int CAM_ATA_STATUS_ERROR	= 0x1c;
inline int CAM_SCSI_IT_NEXUS_LOST = 0x1d;
inline int CAM_SMP_STATUS_ERROR	= 0x1e;
inline int CAM_REQ_SOFTTIMEOUT	= 0x1f;
inline int CAM_NVME_STATUS_ERROR = 0x20;
inline int CAM_IDE		= 0x33;
inline int CAM_RESRC_UNAVAIL	= 0x34;
inline int CAM_UNACKED_EVENT	= 0x35;
inline int CAM_MESSAGE_RECV	= 0x36;
inline int CAM_INVALID_CDB	= 0x37;
inline int CAM_LUN_INVALID	= 0x38;
inline int CAM_TID_INVALID	= 0x39;
inline int CAM_FUNC_NOTAVAIL	= 0x3a;
inline int CAM_NO_NEXUS		= 0x3b;
inline int CAM_IID_INVALID	= 0x3c;
inline int CAM_CDB_RECVD	= 0x3d;
inline int CAM_LUN_ALRDY_ENA	= 0x3e;
inline int CAM_SCSI_BUSY	= 0x3f;

inline int CAM_DEV_QFRZN	= 0x40;
inline int CAM_AUTOSNS_VALID	= 0x80;
inline int CAM_RELEASE_SIMQ	= 0x100;
inline int CAM_SIM_QUEUED	= 0x200;
inline int CAM_QOS_VALID	= 0x400;
inline int CAM_STATUS_MASK	= 0x3F;
inline int CAM_SENT_SENSE	= 0x40000000;
