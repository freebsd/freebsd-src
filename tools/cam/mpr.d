inline string scsi_op[int k] =
	k == 0x00 ? "TEST UNIT READY" :
	k == 0x01 ? "REZERO UNIT" :
	k == 0x03 ? "REQUEST SENSE" :
	k == 0x04 ? "FORMAT UNIT" :
	k == 0x05 ? "READ BLOCK LIMITS" :
	k == 0x07 ? "REASSIGN BLOCKS" :
	k == 0x08 ? "READ(6)" :
	k == 0x0a ? "WRITE(6)" :
	k == 0x0b ? "SEEK(6)" :
	k == 0x0f ? "READ REVERSE(6)" :
	k == 0x10 ? "WRITE FILEMARKS(6)" :
	k == 0x11 ? "SPACE(6)" :
	k == 0x12 ? "INQUIRY" :
	k == 0x14 ? "RECOVER BUFFERED DATA" :
	k == 0x15 ? "MODE SELECT(6)" :
	k == 0x16 ? "RESERVE(6)" :
	k == 0x17 ? "RELEASE(6)" :
	k == 0x18 ? "COPY" :
	k == 0x19 ? "ERASE(6)" :
	k == 0x1a ? "MODE SENSE(6)" :
	k == 0x1b ? "START STOP UNIT" :
	k == 0x1c ? "RECEIVE DIAGNOSTIC RESULTS" :
	k == 0x1d ? "SEND DIAGNOSTIC" :
	k == 0x1e ? "PREVENT ALLOW MEDIUM REMOVAL" :
	k == 0x24 ? "SET WINDOW" :
	k == 0x25 ? "READ CAPACITY(10)" :
	k == 0x28 ? "READ(10)" :
	k == 0x29 ? "READ GENERATION" :
	k == 0x2a ? "WRITE(10)" :
	k == 0x2b ? "SEEK(10)" :
	k == 0x2c ? "ERASE(10)" :
	k == 0x2e ? "WRITE AND VERIFY(10)" :
	k == 0x2f ? "VERIFY(10)" :
	k == 0x30 ? "SEARCH DATA HIGH(10)" :
	k == 0x31 ? "SEARCH DATA EQUAL(10)" :
	k == 0x32 ? "SEARCH DATA LOW(10)" :
	k == 0x33 ? "SET LIMITS(10)" :
	k == 0x35 ? "SYNCHRONIZE CACHE(10)" :
	k == 0x36 ? "LOCK UNLOCK CACHE(10)" :
	k == 0x37 ? "READ DEFECT DATA(10)" :
	k == 0x39 ? "COMPARE" :
	k == 0x3a ? "COPY AND VERIFY" :
	k == 0x3b ? "WRITE BUFFER" :
	k == 0x3c ? "READ BUFFER(10)" :
	k == 0x3e ? "READ LONG(10)" :
	k == 0x3f ? "WRITE LONG(10)" :
	k == 0x40 ? "CHANGE DEFINITION" :
	k == 0x41 ? "WRITE SAME(10)" :
	k == 0x42 ? "UNMAP" :
	k == 0x48 ? "SANITIZE" :
	k == 0x4c ? "LOG SELECT" :
	k == 0x4d ? "LOG SENSE" :
	k == 0x50 ? "XDWRITE(10)" :
	k == 0x51 ? "XPWRITE(10)" :
	k == 0x52 ? "XDREAD(10)" :
	k == 0x53 ? "XDWRITEREAD(10)" :
	k == 0x55 ? "MODE SELECT(10)" :
	k == 0x56 ? "RESERVE(10)" :
	k == 0x57 ? "RELEASE(10)" :
	k == 0x5a ? "MODE SENSE(10)" :
	k == 0x5e ? "PERSISTENT RESERVE IN" :
	k == 0x5f ? "PERSISTENT RESERVE OUT" :
	k == 0x7e ? "extended CDB" :
	k == 0x7f ? "variable length CDB (more than 16 bytes)" :
	k == 0x80 ? "XDWRITE EXTENDED(16)" :
	k == 0x81 ? "REBUILD(16)" :
	k == 0x82 ? "REGENERATE(16)" :
	k == 0x83 ? "Third-party Copy OUT" :
	k == 0x84 ? "Third-party Copy IN" :
	k == 0x85 ? "ATA PASS-THROUGH(16)" :
	k == 0x86 ? "ACCESS CONTROL IN" :
	k == 0x87 ? "ACCESS CONTROL OUT" :
	k == 0x88 ? "READ(16)" :
	k == 0x89 ? "COMPARE AND WRITE" :
	k == 0x8a ? "WRITE(16)" :
	k == 0x8b ? "ORWRITE" :
	k == 0x8c ? "READ ATTRIBUTE" :
	k == 0x8d ? "WRITE ATTRIBUTE" :
	k == 0x8e ? "WRITE AND VERIFY(16)" :
	k == 0x8f ? "VERIFY(16)" :
	k == 0x90 ? "PRE-FETCH(16)" :
	k == 0x91 ? "SYNCHRONIZE CACHE(16)" :
	k == 0x92 ? "LOCK UNLOCK CACHE(16)" :
	k == 0x93 ? "WRITE SAME(16)" :
	k == 0x94 ? "ZBC OUT" :
	k == 0x95 ? "ZBC IN" :
	k == 0x9a ? "WRITE STREAM(16)" :
	k == 0x9b ? "READ BUFFER(16)" :
	k == 0x9c ? "WRITE ATOMIC(16)" :
	k == 0x9e ? "SERVICE ACTION IN(16)" :
	k == 0xa0 ? "REPORT LUNS" :
	k == 0xa1 ? "ATA PASS-THROUGH(12)" :
	k == 0xa2 ? "SECURITY PROTOCOL IN" :
	k == 0xa3 ? "MAINTENANCE IN" :
	k == 0xa4 ? "MAINTENANCE OUT" :
	k == 0xa7 ? "MOVE MEDIUM ATTACHED" :
	k == 0xa8 ? "READ(12)" :
	k == 0xaa ? "WRITE(12)" :
	k == 0xae ? "WRITE AND VERIFY(12)" :
	k == 0xaf ? "VERIFY(12)" :
	k == 0xb3 ? "SET LIMITS(12)" :
	k == 0xb4 ? "READ ELEMENT STATUS ATTACHED" :
	k == 0xb5 ? "SECURITY PROTOCOL OUT" :
	k == 0xb7 ? "READ DEFECT DATA(12)" :
	k == 0xba ? "REDUNDANCY GROUP (IN)" :
	k == 0xbb ? "REDUNDANCY GROUP (OUT)" :
	k == 0xbc ? "SPARE (IN)" :
	k == 0xbd ? "SPARE (OUT)" :
	k == 0xbe ? "VOLUME SET (IN)" :
	k == 0xbf ? "VOLUME SET (OUT)" :
	"Unknown";

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

inline int CAM_CDB_POINTER = 1;
inline int XPT_SCSI_IO = 0x01;
inline int XPT_ATA_IO = 0x18;
inline int XPT_NVME_IO = 0x1c;
inline int XPT_NVME_ADMIN = 0x1f;

/*
 * key >> 5 gives the group:
 * Group 0:  six byte commands
 * Group 1:  ten byte commands
 * Group 2:  ten byte commands
 * Group 3:  reserved (7e and 7f are de-facto 32 bytes though)
 * Group 4:  sixteen byte commands
 * Group 5:  twelve byte commands
 * Group 6:  vendor specific
 * Group 7:  vendor specific
 */
inline int scsi_cdb_len[int key] =
        key == 0 ? 6 :
        key == 1 ? 10 :
        key == 2 ? 10 :
        key == 3 ? 1 :          /* reserved */
        key == 4 ? 16 :
        key == 5 ? 12 :
        key == 6 ? 1 :          /* reserved */
        /* key == 7 */ 1;       /* reserved */

inline int MPI2_IOCSTATUS_MASK                        =(0x7FFF);
inline int MPI2_IOCSTATUS_SUCCESS                     =(0x0000);
inline int MPI2_IOCSTATUS_INVALID_FUNCTION            =(0x0001);
inline int MPI2_IOCSTATUS_BUSY                        =(0x0002);
inline int MPI2_IOCSTATUS_INVALID_SGL                 =(0x0003);
inline int MPI2_IOCSTATUS_INTERNAL_ERROR              =(0x0004);
inline int MPI2_IOCSTATUS_INVALID_VPID                =(0x0005);
inline int MPI2_IOCSTATUS_INSUFFICIENT_RESOURCES      =(0x0006);
inline int MPI2_IOCSTATUS_INVALID_FIELD               =(0x0007);
inline int MPI2_IOCSTATUS_INVALID_STATE               =(0x0008);
inline int MPI2_IOCSTATUS_OP_STATE_NOT_SUPPORTED      =(0x0009);
inline int MPI2_IOCSTATUS_CONFIG_INVALID_ACTION       =(0x0020);
inline int MPI2_IOCSTATUS_CONFIG_INVALID_TYPE         =(0x0021);
inline int MPI2_IOCSTATUS_CONFIG_INVALID_PAGE         =(0x0022);
inline int MPI2_IOCSTATUS_CONFIG_INVALID_DATA         =(0x0023);
inline int MPI2_IOCSTATUS_CONFIG_NO_DEFAULTS          =(0x0024);
inline int MPI2_IOCSTATUS_CONFIG_CANT_COMMIT          =(0x0025);
inline int MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR        =(0x0040);
inline int MPI2_IOCSTATUS_SCSI_INVALID_DEVHANDLE      =(0x0042);
inline int MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE       =(0x0043);
inline int MPI2_IOCSTATUS_SCSI_DATA_OVERRUN           =(0x0044);
inline int MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN          =(0x0045);
inline int MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR          =(0x0046);
inline int MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR         =(0x0047);
inline int MPI2_IOCSTATUS_SCSI_TASK_TERMINATED        =(0x0048);
inline int MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH      =(0x0049);
inline int MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED       =(0x004A);
inline int MPI2_IOCSTATUS_SCSI_IOC_TERMINATED         =(0x004B);
inline int MPI2_IOCSTATUS_SCSI_EXT_TERMINATED         =(0x004C);
inline int MPI2_IOCSTATUS_EEDP_GUARD_ERROR            =(0x004D);
inline int MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR          =(0x004E);
inline int MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR          =(0x004F);
inline int MPI2_IOCSTATUS_TARGET_INVALID_IO_INDEX     =(0x0062);
inline int MPI2_IOCSTATUS_TARGET_ABORTED              =(0x0063);
inline int MPI2_IOCSTATUS_TARGET_NO_CONN_RETRYABLE    =(0x0064);
inline int MPI2_IOCSTATUS_TARGET_NO_CONNECTION        =(0x0065);
inline int MPI2_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH  =(0x006A);
inline int MPI2_IOCSTATUS_TARGET_DATA_OFFSET_ERROR    =(0x006D);
inline int MPI2_IOCSTATUS_TARGET_TOO_MUCH_WRITE_DATA  =(0x006E);
inline int MPI2_IOCSTATUS_TARGET_IU_TOO_SHORT         =(0x006F);
inline int MPI2_IOCSTATUS_TARGET_ACK_NAK_TIMEOUT      =(0x0070);
inline int MPI2_IOCSTATUS_TARGET_NAK_RECEIVED         =(0x0071);
inline int MPI2_IOCSTATUS_SAS_SMP_REQUEST_FAILED      =(0x0090);
inline int MPI2_IOCSTATUS_SAS_SMP_DATA_OVERRUN        =(0x0091);
inline int MPI2_IOCSTATUS_DIAGNOSTIC_RELEASED         =(0x00A0);
inline int MPI2_IOCSTATUS_RAID_ACCEL_ERROR            =(0x00B0);

inline int MPI2_IOCSTATUS_FLAG_LOG_INFO_AVAILABLE     =(0x8000);

inline string mpi2_iocstatus_str[int key] = 
	key == MPI2_IOCSTATUS_SUCCESS ? "MPI2_IOCSTATUS_SUCCESS" :
	key == MPI2_IOCSTATUS_INVALID_FUNCTION ? "MPI2_IOCSTATUS_INVALID_FUNCTION" :
	key == MPI2_IOCSTATUS_BUSY ? "MPI2_IOCSTATUS_BUSY" :
	key == MPI2_IOCSTATUS_INVALID_SGL ? "MPI2_IOCSTATUS_INVALID_SGL" :
	key == MPI2_IOCSTATUS_INTERNAL_ERROR ? "MPI2_IOCSTATUS_INTERNAL_ERROR" :
	key == MPI2_IOCSTATUS_INVALID_VPID ? "MPI2_IOCSTATUS_INVALID_VPID" :
	key == MPI2_IOCSTATUS_INSUFFICIENT_RESOURCES ? "MPI2_IOCSTATUS_INSUFFICIENT_RESOURCES" :
	key == MPI2_IOCSTATUS_INVALID_FIELD ? "MPI2_IOCSTATUS_INVALID_FIELD" :
	key == MPI2_IOCSTATUS_INVALID_STATE ? "MPI2_IOCSTATUS_INVALID_STATE" :
	key == MPI2_IOCSTATUS_OP_STATE_NOT_SUPPORTED ? "MPI2_IOCSTATUS_OP_STATE_NOT_SUPPORTED" :
	key == MPI2_IOCSTATUS_CONFIG_INVALID_ACTION ? "MPI2_IOCSTATUS_CONFIG_INVALID_ACTION" :
	key == MPI2_IOCSTATUS_CONFIG_INVALID_TYPE ? "MPI2_IOCSTATUS_CONFIG_INVALID_TYPE" :
	key == MPI2_IOCSTATUS_CONFIG_INVALID_PAGE ? "MPI2_IOCSTATUS_CONFIG_INVALID_PAGE" :
	key == MPI2_IOCSTATUS_CONFIG_INVALID_DATA ? "MPI2_IOCSTATUS_CONFIG_INVALID_DATA" :
	key == MPI2_IOCSTATUS_CONFIG_NO_DEFAULTS ? "MPI2_IOCSTATUS_CONFIG_NO_DEFAULTS" :
	key == MPI2_IOCSTATUS_CONFIG_CANT_COMMIT ? "MPI2_IOCSTATUS_CONFIG_CANT_COMMIT" :
	key == MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR ? "MPI2_IOCSTATUS_SCSI_RECOVERED_ERROR" :
	key == MPI2_IOCSTATUS_SCSI_INVALID_DEVHANDLE ? "MPI2_IOCSTATUS_SCSI_INVALID_DEVHANDLE" :
	key == MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE ? "MPI2_IOCSTATUS_SCSI_DEVICE_NOT_THERE" :
	key == MPI2_IOCSTATUS_SCSI_DATA_OVERRUN ? "MPI2_IOCSTATUS_SCSI_DATA_OVERRUN" :
	key == MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN ? "MPI2_IOCSTATUS_SCSI_DATA_UNDERRUN" :
	key == MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR ? "MPI2_IOCSTATUS_SCSI_IO_DATA_ERROR" :
	key == MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR ? "MPI2_IOCSTATUS_SCSI_PROTOCOL_ERROR" :
	key == MPI2_IOCSTATUS_SCSI_TASK_TERMINATED ? "MPI2_IOCSTATUS_SCSI_TASK_TERMINATED" :
	key == MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH ? "MPI2_IOCSTATUS_SCSI_RESIDUAL_MISMATCH" :
	key == MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED ? "MPI2_IOCSTATUS_SCSI_TASK_MGMT_FAILED" :
	key == MPI2_IOCSTATUS_SCSI_IOC_TERMINATED ? "MPI2_IOCSTATUS_SCSI_IOC_TERMINATED" :
	key == MPI2_IOCSTATUS_SCSI_EXT_TERMINATED ? "MPI2_IOCSTATUS_SCSI_EXT_TERMINATED" :
	key == MPI2_IOCSTATUS_EEDP_GUARD_ERROR ? "MPI2_IOCSTATUS_EEDP_GUARD_ERROR" :
	key == MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR ? "MPI2_IOCSTATUS_EEDP_REF_TAG_ERROR" :
	key == MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR ? "MPI2_IOCSTATUS_EEDP_APP_TAG_ERROR" :
	key == MPI2_IOCSTATUS_TARGET_INVALID_IO_INDEX ? "MPI2_IOCSTATUS_TARGET_INVALID_IO_INDEX" :
	key == MPI2_IOCSTATUS_TARGET_ABORTED ? "MPI2_IOCSTATUS_TARGET_ABORTED" :
	key == MPI2_IOCSTATUS_TARGET_NO_CONN_RETRYABLE ? "MPI2_IOCSTATUS_TARGET_NO_CONN_RETRYABLE" :
	key == MPI2_IOCSTATUS_TARGET_NO_CONNECTION ? "MPI2_IOCSTATUS_TARGET_NO_CONNECTION" :
	key == MPI2_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH ? "MPI2_IOCSTATUS_TARGET_XFER_COUNT_MISMATCH" :
	key == MPI2_IOCSTATUS_TARGET_DATA_OFFSET_ERROR ? "MPI2_IOCSTATUS_TARGET_DATA_OFFSET_ERROR" :
	key == MPI2_IOCSTATUS_TARGET_TOO_MUCH_WRITE_DATA ? "MPI2_IOCSTATUS_TARGET_TOO_MUCH_WRITE_DATA" :
	key == MPI2_IOCSTATUS_TARGET_IU_TOO_SHORT ? "MPI2_IOCSTATUS_TARGET_IU_TOO_SHORT" :
	key == MPI2_IOCSTATUS_TARGET_ACK_NAK_TIMEOUT ? "MPI2_IOCSTATUS_TARGET_ACK_NAK_TIMEOUT" :
	key == MPI2_IOCSTATUS_TARGET_NAK_RECEIVED ? "MPI2_IOCSTATUS_TARGET_NAK_RECEIVED" :
	key == MPI2_IOCSTATUS_SAS_SMP_REQUEST_FAILED ? "MPI2_IOCSTATUS_SAS_SMP_REQUEST_FAILED" :
	key == MPI2_IOCSTATUS_SAS_SMP_DATA_OVERRUN ? "MPI2_IOCSTATUS_SAS_SMP_DATA_OVERRUN" :
	key == MPI2_IOCSTATUS_DIAGNOSTIC_RELEASED ? "MPI2_IOCSTATUS_DIAGNOSTIC_RELEASED" :
	key == MPI2_IOCSTATUS_RAID_ACCEL_ERROR ? "MPI2_IOCSTATUS_RAID_ACCEL_ERROR" :
	"MPI2_unknown value";



/*
 * arg0 union *ccb
 * arg1 mpr_command *cm
 * arg2 sassc->flags
 * arg3 device_info
 */
cam::mpr:complete
{
	this->ccb = (union ccb *)arg0;
        this->func = this->ccb->ccb_h.func_code & 0xff;
        this->periph = this->ccb->ccb_h.path->periph;
	this->cm = (struct mpr_command *)arg1;
	this->rep = (MPI2_SCSI_IO_REPLY *)this->cm->cm_reply;
	this->sassc_flags = arg2;
	this->device_info = arg3;
        this->trace = 0;
	this->do_fast = 0;
}


cam::mpr:complete
/this->periph->unit_number == 1 || this->periph->unit_number == 2/
{
	this->trace = 1;
}

cam::mpr:complete
/this->trace && this->rep != NULL/
{
	this->IOCStatus = /* le16toh */ this->rep->IOCStatus & MPI2_IOCSTATUS_MASK;
}

cam::mpr:complete
/this->trace && this->rep == NULL/
{
/*	printf("mpr: da%d: FAST", this->periph->unit_number); */
	this->trace = 0
}

cam::mpr:complete
/this->func == XPT_SCSI_IO/
{
        this->hdr = &this->ccb->ccb_h;
        this->csio = &this->ccb->csio;
        this->cdb = this->hdr->flags & CAM_CDB_POINTER ?
                this->csio->cdb_io.cdb_ptr :
                &this->csio->cdb_io.cdb_bytes[0];
        this->cdb_len = this->csio->cdb_len ? this->csio->cdb_len :
                scsi_cdb_len[this->cdb[0] >> 5];
}

cam::mpr:complete
/this->trace && this->rep != NULL && this->func == XPT_SCSI_IO/
{
	printf("mpr: da%d: SLOW CDB: %s Status: %s", this->periph->unit_number, scsi_op[this->cdb[0]],
	    mpi2_iocstatus_str[this->IOCStatus]);
}
