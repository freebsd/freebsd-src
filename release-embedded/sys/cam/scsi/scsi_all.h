/*-
 * Largely written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 * $FreeBSD$
 */

/*
 * SCSI general  interface description
 */

#ifndef	_SCSI_SCSI_ALL_H
#define	_SCSI_SCSI_ALL_H 1

#include <sys/cdefs.h>
#include <machine/stdarg.h>

#ifdef _KERNEL
/*
 * This is the number of seconds we wait for devices to settle after a SCSI
 * bus reset.
 */
extern int scsi_delay;
#endif /* _KERNEL */

/*
 * SCSI command format
 */

/*
 * Define dome bits that are in ALL (or a lot of) scsi commands
 */
#define	SCSI_CTL_LINK		0x01
#define	SCSI_CTL_FLAG		0x02
#define	SCSI_CTL_VENDOR		0xC0
#define	SCSI_CMD_LUN		0xA0	/* these two should not be needed */
#define	SCSI_CMD_LUN_SHIFT	5	/* LUN in the cmd is no longer SCSI */

#define	SCSI_MAX_CDBLEN		16	/* 
					 * 16 byte commands are in the 
					 * SCSI-3 spec 
					 */
#if defined(CAM_MAX_CDBLEN) && (CAM_MAX_CDBLEN < SCSI_MAX_CDBLEN)
#error "CAM_MAX_CDBLEN cannot be less than SCSI_MAX_CDBLEN"
#endif

/* 6byte CDBs special case 0 length to be 256 */
#define	SCSI_CDB6_LEN(len)	((len) == 0 ? 256 : len)

/*
 * This type defines actions to be taken when a particular sense code is
 * received.  Right now, these flags are only defined to take up 16 bits,
 * but can be expanded in the future if necessary.
 */
typedef enum {
	SS_NOP      = 0x000000,	/* Do nothing */
	SS_RETRY    = 0x010000,	/* Retry the command */
	SS_FAIL     = 0x020000,	/* Bail out */
	SS_START    = 0x030000,	/* Send a Start Unit command to the device,
				 * then retry the original command.
				 */
	SS_TUR      = 0x040000,	/* Send a Test Unit Ready command to the
				 * device, then retry the original command.
				 */
	SS_MASK     = 0xff0000
} scsi_sense_action;

typedef enum {
	SSQ_NONE		= 0x0000,
	SSQ_DECREMENT_COUNT	= 0x0100,  /* Decrement the retry count */
	SSQ_MANY		= 0x0200,  /* send lots of recovery commands */
	SSQ_RANGE		= 0x0400,  /*
					    * This table entry represents the
					    * end of a range of ASCQs that
					    * have identical error actions
					    * and text.
					    */
	SSQ_PRINT_SENSE		= 0x0800,
	SSQ_UA			= 0x1000,  /* Broadcast UA. */
	SSQ_RESCAN		= 0x2000,  /* Rescan target for LUNs. */
	SSQ_LOST		= 0x4000,  /* Destroy the LUNs. */
	SSQ_MASK		= 0xff00
} scsi_sense_action_qualifier;

/* Mask for error status values */
#define	SS_ERRMASK	0xff

/* The default, retyable, error action */
#define	SS_RDEF		SS_RETRY|SSQ_DECREMENT_COUNT|SSQ_PRINT_SENSE|EIO

/* The retyable, error action, with table specified error code */
#define	SS_RET		SS_RETRY|SSQ_DECREMENT_COUNT|SSQ_PRINT_SENSE

/* Fatal error action, with table specified error code */
#define	SS_FATAL	SS_FAIL|SSQ_PRINT_SENSE

struct scsi_generic
{
	u_int8_t opcode;
	u_int8_t bytes[11];
};

struct scsi_request_sense
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SRS_DESC	0x01
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_test_unit_ready
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[3];
	u_int8_t control;
};

struct scsi_receive_diag {
	uint8_t opcode;
	uint8_t byte2;
#define SRD_PCV		0x01
	uint8_t page_code;
	uint8_t length[2]; 
	uint8_t control;
};

struct scsi_send_diag {
	uint8_t opcode;
	uint8_t byte2;
#define SSD_UNITOFFL				0x01
#define SSD_DEVOFFL				0x02
#define SSD_SELFTEST				0x04
#define SSD_PF					0x10
#define SSD_SELF_TEST_CODE_MASK			0xE0
#define SSD_SELF_TEST_CODE_SHIFT		5
#define		SSD_SELF_TEST_CODE_NONE		0x00
#define		SSD_SELF_TEST_CODE_BG_SHORT	0x01
#define		SSD_SELF_TEST_CODE_BG_EXTENDED	0x02
#define		SSD_SELF_TEST_CODE_BG_ABORT	0x04
#define		SSD_SELF_TEST_CODE_FG_SHORT	0x05
#define		SSD_SELF_TEST_CODE_FG_EXTENDED	0x06
	uint8_t	reserved;
	uint8_t	length[2];
	uint8_t control;
};

struct scsi_sense
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_inquiry
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SI_EVPD 	0x01
#define	SI_CMDDT	0x02
	u_int8_t page_code;
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_mode_sense_6
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SMS_DBD				0x08
	u_int8_t page;
#define	SMS_PAGE_CODE 			0x3F
#define	SMS_VENDOR_SPECIFIC_PAGE	0x00
#define	SMS_DISCONNECT_RECONNECT_PAGE	0x02
#define	SMS_FORMAT_DEVICE_PAGE		0x03
#define	SMS_GEOMETRY_PAGE		0x04
#define	SMS_CACHE_PAGE			0x08
#define	SMS_PERIPHERAL_DEVICE_PAGE	0x09
#define	SMS_CONTROL_MODE_PAGE		0x0A
#define	SMS_PROTO_SPECIFIC_PAGE		0x19
#define	SMS_INFO_EXCEPTIONS_PAGE	0x1C
#define	SMS_ALL_PAGES_PAGE		0x3F
#define	SMS_PAGE_CTRL_MASK		0xC0
#define	SMS_PAGE_CTRL_CURRENT 		0x00
#define	SMS_PAGE_CTRL_CHANGEABLE 	0x40
#define	SMS_PAGE_CTRL_DEFAULT 		0x80
#define	SMS_PAGE_CTRL_SAVED 		0xC0
	u_int8_t subpage;
#define	SMS_SUBPAGE_PAGE_0		0x00
#define	SMS_SUBPAGE_ALL			0xff
	u_int8_t length;
	u_int8_t control;
};

struct scsi_mode_sense_10
{
	u_int8_t opcode;
	u_int8_t byte2;		/* same bits as small version */
#define	SMS10_LLBAA			0x10
	u_int8_t page; 		/* same bits as small version */
	u_int8_t subpage;
	u_int8_t unused[3];
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_mode_select_6
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SMS_SP	0x01
#define	SMS_PF	0x10
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_mode_select_10
{
	u_int8_t opcode;
	u_int8_t byte2;		/* same bits as small version */
	u_int8_t unused[5];
	u_int8_t length[2];
	u_int8_t control;
};

/*
 * When sending a mode select to a tape drive, the medium type must be 0.
 */
struct scsi_mode_hdr_6
{
	u_int8_t datalen;
	u_int8_t medium_type;
	u_int8_t dev_specific;
	u_int8_t block_descr_len;
};

struct scsi_mode_hdr_10
{
	u_int8_t datalen[2];
	u_int8_t medium_type;
	u_int8_t dev_specific;
	u_int8_t reserved[2];
	u_int8_t block_descr_len[2];
};

struct scsi_mode_block_descr
{
	u_int8_t density_code;
	u_int8_t num_blocks[3];
	u_int8_t reserved;
	u_int8_t block_len[3];
};

struct scsi_per_res_in
{
	u_int8_t opcode;
	u_int8_t action;
#define	SPRI_RK	0x00
#define	SPRI_RR	0x01
#define	SPRI_RC	0x02
#define	SPRI_RS	0x03
	u_int8_t reserved[5];
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_per_res_in_header
{
	u_int8_t generation[4];
	u_int8_t length[4];
};

struct scsi_per_res_key
{
	u_int8_t key[8];
};

struct scsi_per_res_in_keys
{
	struct scsi_per_res_in_header header;
	struct scsi_per_res_key keys[0];
};

struct scsi_per_res_cap
{
	uint8_t length[2];
	uint8_t flags1;
#define	SPRI_CRH	0x10
#define	SPRI_SIP_C	0x08
#define	SPRI_ATP_C	0x04
#define	SPRI_PTPL_C	0x01
	uint8_t flags2;
#define	SPRI_TMV	0x80
#define	SPRI_PTPL_A	0x01
	uint8_t type_mask[2];
#define	SPRI_TM_WR_EX_AR	0x8000
#define	SPRI_TM_EX_AC_RO	0x4000
#define	SPRI_TM_WR_EX_RO	0x2000
#define	SPRI_TM_EX_AC		0x0800
#define	SPRI_TM_WR_EX		0x0200
#define	SPRI_TM_EX_AC_AR	0x0001
	uint8_t reserved[2];
};

struct scsi_per_res_in_rsrv_data
{
	uint8_t reservation[8];
	uint8_t obsolete1[4];
	uint8_t reserved;
	uint8_t scopetype;
#define	SPRT_WE    0x01
#define	SPRT_EA    0x03
#define	SPRT_WERO  0x05
#define	SPRT_EARO  0x06
#define	SPRT_WEAR  0x07
#define	SPRT_EAAR  0x08
	uint8_t obsolete2[2];
};

struct scsi_per_res_in_rsrv
{
	struct scsi_per_res_in_header header;
	struct scsi_per_res_in_rsrv_data data;
};

struct scsi_per_res_out
{
	u_int8_t opcode;
	u_int8_t action;
#define	SPRO_REGISTER		0x00
#define	SPRO_RESERVE		0x01
#define	SPRO_RELEASE		0x02
#define	SPRO_CLEAR		0x03
#define	SPRO_PREEMPT		0x04
#define	SPRO_PRE_ABO		0x05
#define	SPRO_REG_IGNO		0x06
#define	SPRO_REG_MOVE		0x07
#define	SPRO_ACTION_MASK	0x1f
	u_int8_t scope_type;
#define	SPR_SCOPE_MASK		0xf0
#define	SPR_LU_SCOPE		0x00
#define	SPR_TYPE_MASK		0x0f
#define	SPR_TYPE_WR_EX		0x01
#define	SPR_TYPE_EX_AC		0x03
#define	SPR_TYPE_WR_EX_RO	0x05
#define	SPR_TYPE_EX_AC_RO	0x06
#define	SPR_TYPE_WR_EX_AR	0x07
#define	SPR_TYPE_EX_AC_AR	0x08
	u_int8_t reserved[2];
	u_int8_t length[4];
	u_int8_t control;
};

struct scsi_per_res_out_parms
{
	struct scsi_per_res_key res_key;
	u_int8_t serv_act_res_key[8];
	u_int8_t obsolete1[4];
	u_int8_t flags;
#define	SPR_SPEC_I_PT		0x08
#define	SPR_ALL_TG_PT		0x04
#define	SPR_APTPL		0x01
	u_int8_t reserved1;
	u_int8_t obsolete2[2];
};


struct scsi_log_sense
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SLS_SP				0x01
#define	SLS_PPC				0x02
	u_int8_t page;
#define	SLS_PAGE_CODE 			0x3F
#define	SLS_ALL_PAGES_PAGE		0x00
#define	SLS_OVERRUN_PAGE		0x01
#define	SLS_ERROR_WRITE_PAGE		0x02
#define	SLS_ERROR_READ_PAGE		0x03
#define	SLS_ERROR_READREVERSE_PAGE	0x04
#define	SLS_ERROR_VERIFY_PAGE		0x05
#define	SLS_ERROR_NONMEDIUM_PAGE	0x06
#define	SLS_ERROR_LASTN_PAGE		0x07
#define	SLS_SELF_TEST_PAGE		0x10
#define	SLS_IE_PAGE			0x2f
#define	SLS_PAGE_CTRL_MASK		0xC0
#define	SLS_PAGE_CTRL_THRESHOLD		0x00
#define	SLS_PAGE_CTRL_CUMULATIVE	0x40
#define	SLS_PAGE_CTRL_THRESH_DEFAULT	0x80
#define	SLS_PAGE_CTRL_CUMUL_DEFAULT	0xC0
	u_int8_t reserved[2];
	u_int8_t paramptr[2];
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_log_select
{
	u_int8_t opcode;
	u_int8_t byte2;
/*	SLS_SP				0x01 */
#define	SLS_PCR				0x02
	u_int8_t page;
/*	SLS_PAGE_CTRL_MASK		0xC0 */
/*	SLS_PAGE_CTRL_THRESHOLD		0x00 */
/*	SLS_PAGE_CTRL_CUMULATIVE	0x40 */
/*	SLS_PAGE_CTRL_THRESH_DEFAULT	0x80 */
/*	SLS_PAGE_CTRL_CUMUL_DEFAULT	0xC0 */
	u_int8_t reserved[4];
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_log_header
{
	u_int8_t page;
	u_int8_t reserved;
	u_int8_t datalen[2];
};

struct scsi_log_param_header {
	u_int8_t param_code[2];
	u_int8_t param_control;
#define	SLP_LP				0x01
#define	SLP_LBIN			0x02
#define	SLP_TMC_MASK			0x0C
#define	SLP_TMC_ALWAYS			0x00
#define	SLP_TMC_EQUAL			0x04
#define	SLP_TMC_NOTEQUAL		0x08
#define	SLP_TMC_GREATER			0x0C
#define	SLP_ETC				0x10
#define	SLP_TSD				0x20
#define	SLP_DS				0x40
#define	SLP_DU				0x80
	u_int8_t param_len;
};

struct scsi_control_page {
	u_int8_t page_code;
	u_int8_t page_length;
	u_int8_t rlec;
#define	SCP_RLEC			0x01	/*Report Log Exception Cond*/
#define	SCP_GLTSD			0x02	/*Global Logging target
						  save disable */
#define	SCP_DSENSE			0x04	/*Descriptor Sense */
#define	SCP_DPICZ			0x08	/*Disable Prot. Info Check
						  if Prot. Field is Zero */
#define	SCP_TMF_ONLY			0x10	/*TM Functions Only*/
#define	SCP_TST_MASK			0xE0	/*Task Set Type Mask*/
#define	SCP_TST_ONE			0x00	/*One Task Set*/
#define	SCP_TST_SEPARATE		0x20	/*Separate Task Sets*/
	u_int8_t queue_flags;
#define	SCP_QUEUE_ALG_MASK		0xF0
#define	SCP_QUEUE_ALG_RESTRICTED	0x00
#define	SCP_QUEUE_ALG_UNRESTRICTED	0x10
#define	SCP_QUEUE_ERR			0x02	/*Queued I/O aborted for CACs*/
#define	SCP_QUEUE_DQUE			0x01	/*Queued I/O disabled*/
	u_int8_t eca_and_aen;
#define	SCP_EECA			0x80	/*Enable Extended CA*/
#define	SCP_RAENP			0x04	/*Ready AEN Permission*/
#define	SCP_UAAENP			0x02	/*UA AEN Permission*/
#define	SCP_EAENP			0x01	/*Error AEN Permission*/
	u_int8_t reserved;
	u_int8_t aen_holdoff_period[2];
};

struct scsi_cache_page {
	u_int8_t page_code;
#define	SCHP_PAGE_SAVABLE		0x80	/* Page is savable */
	u_int8_t page_length;
	u_int8_t cache_flags;
#define	SCHP_FLAGS_WCE			0x04	/* Write Cache Enable */
#define	SCHP_FLAGS_MF			0x02	/* Multiplication factor */
#define	SCHP_FLAGS_RCD			0x01	/* Read Cache Disable */
	u_int8_t rw_cache_policy;
	u_int8_t dis_prefetch[2];
	u_int8_t min_prefetch[2];
	u_int8_t max_prefetch[2];
	u_int8_t max_prefetch_ceil[2];
};

/*
 * XXX KDM
 * Updated version of the cache page, as of SBC.  Update this to SBC-3 and
 * rationalize the two.
 */
struct scsi_caching_page {
	uint8_t page_code;
#define	SMS_CACHING_PAGE		0x08
	uint8_t page_length;
	uint8_t flags1;
#define	SCP_IC		0x80
#define	SCP_ABPF	0x40
#define	SCP_CAP		0x20
#define	SCP_DISC	0x10
#define	SCP_SIZE	0x08
#define	SCP_WCE		0x04
#define	SCP_MF		0x02
#define	SCP_RCD		0x01
	uint8_t ret_priority;
	uint8_t disable_pf_transfer_len[2];
	uint8_t min_prefetch[2];
	uint8_t max_prefetch[2];
	uint8_t max_pf_ceiling[2];
	uint8_t flags2;
#define	SCP_FSW		0x80
#define	SCP_LBCSS	0x40
#define	SCP_DRA		0x20
#define	SCP_VS1		0x10
#define	SCP_VS2		0x08
	uint8_t cache_segments;
	uint8_t cache_seg_size[2];
	uint8_t reserved;
	uint8_t non_cache_seg_size[3];
};

/*
 * XXX KDM move this off to a vendor shim.
 */
struct copan_power_subpage {
	uint8_t page_code;
#define	PWR_PAGE_CODE		0x00
	uint8_t subpage;
#define	PWR_SUBPAGE_CODE	0x02
	uint8_t page_length[2];
	uint8_t page_version;
#define	PWR_VERSION		    0x01
	uint8_t total_luns;
	uint8_t max_active_luns;
#define	PWR_DFLT_MAX_LUNS	    0x07
	uint8_t reserved[25];
};

/*
 * XXX KDM move this off to a vendor shim.
 */
struct copan_aps_subpage {
	uint8_t page_code;
#define	APS_PAGE_CODE		0x00
	uint8_t subpage;
#define	APS_SUBPAGE_CODE	0x03
	uint8_t page_length[2];
	uint8_t page_version;
#define	APS_VERSION		    0x00
	uint8_t lock_active;
#define	APS_LOCK_ACTIVE	    0x01
#define	APS_LOCK_INACTIVE	0x00
	uint8_t reserved[26];
};

/*
 * XXX KDM move this off to a vendor shim.
 */
struct copan_debugconf_subpage {
	uint8_t page_code;
#define DBGCNF_PAGE_CODE		0x00
	uint8_t subpage;
#define DBGCNF_SUBPAGE_CODE	0xF0
	uint8_t page_length[2];
	uint8_t page_version;
#define DBGCNF_VERSION			0x00
	uint8_t ctl_time_io_secs[2];
};


struct scsi_info_exceptions_page {
	u_int8_t page_code;
#define	SIEP_PAGE_SAVABLE		0x80	/* Page is savable */
	u_int8_t page_length;
	u_int8_t info_flags;
#define	SIEP_FLAGS_PERF			0x80
#define	SIEP_FLAGS_EBF			0x20
#define	SIEP_FLAGS_EWASC		0x10
#define	SIEP_FLAGS_DEXCPT		0x08
#define	SIEP_FLAGS_TEST			0x04
#define	SIEP_FLAGS_EBACKERR		0x02
#define	SIEP_FLAGS_LOGERR		0x01
	u_int8_t mrie;
	u_int8_t interval_timer[4];
	u_int8_t report_count[4];
};

struct scsi_proto_specific_page {
	u_int8_t page_code;
#define	SPSP_PAGE_SAVABLE		0x80	/* Page is savable */
	u_int8_t page_length;
	u_int8_t protocol;
#define	SPSP_PROTO_FC			0x00
#define	SPSP_PROTO_SPI			0x01
#define	SPSP_PROTO_SSA			0x02
#define	SPSP_PROTO_1394			0x03
#define	SPSP_PROTO_RDMA			0x04
#define	SPSP_PROTO_ISCSI		0x05
#define	SPSP_PROTO_SAS			0x06
#define	SPSP_PROTO_ADT			0x07
#define	SPSP_PROTO_ATA			0x08
#define	SPSP_PROTO_NONE			0x0f
};

struct scsi_reserve
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SR_EXTENT	0x01
#define	SR_ID_MASK	0x0e
#define	SR_3RDPTY	0x10
#define	SR_LUN_MASK	0xe0
	u_int8_t resv_id;
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_reserve_10 {
	uint8_t	opcode;
	uint8_t	byte2;
#define	SR10_3RDPTY	0x10
#define	SR10_LONGID	0x02
#define	SR10_EXTENT	0x01
	uint8_t resv_id;
	uint8_t thirdparty_id;
	uint8_t reserved[3];
	uint8_t length[2];
	uint8_t control;
};


struct scsi_release
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t resv_id;
	u_int8_t unused[1];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_release_10 {
	uint8_t opcode;
	uint8_t byte2;
	uint8_t resv_id;
	uint8_t thirdparty_id;
	uint8_t reserved[3];
	uint8_t length[2];
	uint8_t control;
};

struct scsi_prevent
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t how;
	u_int8_t control;
};
#define	PR_PREVENT 0x01
#define	PR_ALLOW   0x00

struct scsi_sync_cache
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SSC_IMMED	0x02
#define	SSC_RELADR	0x01
	u_int8_t begin_lba[4];
	u_int8_t reserved;
	u_int8_t lb_count[2];
	u_int8_t control;	
};

struct scsi_sync_cache_16
{
	uint8_t opcode;
	uint8_t byte2;
	uint8_t begin_lba[8];
	uint8_t lb_count[4];
	uint8_t reserved;
	uint8_t control;
};

struct scsi_format {
	uint8_t opcode;
	uint8_t byte2;
#define	SF_LONGLIST		0x20
#define	SF_FMTDATA		0x10
#define	SF_CMPLIST		0x08
#define	SF_FORMAT_MASK		0x07
#define	SF_FORMAT_BLOCK		0x00
#define	SF_FORMAT_LONG_BLOCK	0x03
#define	SF_FORMAT_BFI		0x04
#define	SF_FORMAT_PHYS		0x05
	uint8_t vendor;
	uint8_t interleave[2];
	uint8_t control;
};

struct scsi_format_header_short {
	uint8_t reserved;
#define	SF_DATA_FOV	0x80
#define	SF_DATA_DPRY	0x40
#define	SF_DATA_DCRT	0x20
#define	SF_DATA_STPF	0x10
#define	SF_DATA_IP	0x08
#define	SF_DATA_DSP	0x04
#define	SF_DATA_IMMED	0x02
#define	SF_DATA_VS	0x01
	uint8_t byte2;
	uint8_t defect_list_len[2];
};

struct scsi_format_header_long {
	uint8_t reserved;
	uint8_t byte2;
	uint8_t reserved2[2];
	uint8_t defect_list_len[4];
};

struct scsi_changedef
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused1;
	u_int8_t how;
	u_int8_t unused[4];
	u_int8_t datalen;
	u_int8_t control;
};

struct scsi_read_buffer
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	RWB_MODE		0x07
#define	RWB_MODE_HDR_DATA	0x00
#define	RWB_MODE_VENDOR		0x01
#define	RWB_MODE_DATA		0x02
#define	RWB_MODE_DOWNLOAD	0x04
#define	RWB_MODE_DOWNLOAD_SAVE	0x05
        u_int8_t buffer_id;
        u_int8_t offset[3];
        u_int8_t length[3];
        u_int8_t control;
};

struct scsi_write_buffer
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t buffer_id;
	u_int8_t offset[3];
	u_int8_t length[3];
	u_int8_t control;
};

struct scsi_rw_6
{
	u_int8_t opcode;
	u_int8_t addr[3];
/* only 5 bits are valid in the MSB address byte */
#define	SRW_TOPADDR	0x1F
	u_int8_t length;
	u_int8_t control;
};

struct scsi_rw_10
{
	u_int8_t opcode;
#define	SRW10_RELADDR	0x01
/* EBP defined for WRITE(10) only */
#define	SRW10_EBP	0x04
#define	SRW10_FUA	0x08
#define	SRW10_DPO	0x10
	u_int8_t byte2;
	u_int8_t addr[4];
	u_int8_t reserved;
	u_int8_t length[2];
	u_int8_t control;
};

struct scsi_rw_12
{
	u_int8_t opcode;
#define	SRW12_RELADDR	0x01
#define	SRW12_FUA	0x08
#define	SRW12_DPO	0x10
	u_int8_t byte2;
	u_int8_t addr[4];
	u_int8_t length[4];
	u_int8_t reserved;
	u_int8_t control;
};

struct scsi_rw_16
{
	u_int8_t opcode;
#define	SRW16_RELADDR	0x01
#define	SRW16_FUA	0x08
#define	SRW16_DPO	0x10
	u_int8_t byte2;
	u_int8_t addr[8];
	u_int8_t length[4];
	u_int8_t reserved;
	u_int8_t control;
};

struct scsi_write_same_10
{
	uint8_t	opcode;
	uint8_t	byte2;
#define	SWS_LBDATA	0x02
#define	SWS_PBDATA	0x04
#define	SWS_UNMAP	0x08
#define	SWS_ANCHOR	0x10
	uint8_t	addr[4];
	uint8_t	group;
	uint8_t	length[2];
	uint8_t	control;
};

struct scsi_write_same_16
{
	uint8_t	opcode;
	uint8_t	byte2;
	uint8_t	addr[8];
	uint8_t	length[4];
	uint8_t	group;
	uint8_t	control;
};

struct scsi_unmap
{
	uint8_t	opcode;
	uint8_t	byte2;
#define	SU_ANCHOR	0x01
	uint8_t	reserved[4];
	uint8_t	group;
	uint8_t	length[2];
	uint8_t	control;
};

struct scsi_write_verify_10
{
	uint8_t	opcode;
	uint8_t	byte2;
#define	SWV_BYTCHK		0x02
#define	SWV_DPO			0x10
#define	SWV_WRPROECT_MASK	0xe0
	uint8_t	addr[4];
	uint8_t	group;
	uint8_t length[2];
	uint8_t	control;
};

struct scsi_write_verify_12
{
	uint8_t	opcode;
	uint8_t	byte2;
	uint8_t	addr[4];
	uint8_t	length[4];
	uint8_t	group;
	uint8_t	control;
};

struct scsi_write_verify_16
{
	uint8_t	opcode;
	uint8_t	byte2;
	uint8_t	addr[8];
	uint8_t	length[4];
	uint8_t	group;
	uint8_t	control;
};


struct scsi_start_stop_unit
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SSS_IMMED		0x01
	u_int8_t reserved[2];
	u_int8_t how;
#define	SSS_START		0x01
#define	SSS_LOEJ		0x02
#define	SSS_PC_MASK		0xf0
#define	SSS_PC_START_VALID	0x00
#define	SSS_PC_ACTIVE		0x10
#define	SSS_PC_IDLE		0x20
#define	SSS_PC_STANDBY		0x30
#define	SSS_PC_LU_CONTROL	0x70
#define	SSS_PC_FORCE_IDLE_0	0xa0
#define	SSS_PC_FORCE_STANDBY_0	0xb0
	u_int8_t control;
};

struct ata_pass_12 {
	u_int8_t opcode;
	u_int8_t protocol;
#define	AP_PROTO_HARD_RESET	(0x00 << 1)
#define	AP_PROTO_SRST		(0x01 << 1)
#define	AP_PROTO_NON_DATA	(0x03 << 1)
#define	AP_PROTO_PIO_IN		(0x04 << 1)
#define	AP_PROTO_PIO_OUT	(0x05 << 1)
#define	AP_PROTO_DMA		(0x06 << 1)
#define	AP_PROTO_DMA_QUEUED	(0x07 << 1)
#define	AP_PROTO_DEVICE_DIAG	(0x08 << 1)
#define	AP_PROTO_DEVICE_RESET	(0x09 << 1)
#define	AP_PROTO_UDMA_IN	(0x0a << 1)
#define	AP_PROTO_UDMA_OUT	(0x0b << 1)
#define	AP_PROTO_FPDMA		(0x0c << 1)
#define	AP_PROTO_RESP_INFO	(0x0f << 1)
#define	AP_MULTI	0xe0
	u_int8_t flags;
#define	AP_T_LEN	0x03
#define	AP_BB		0x04
#define	AP_T_DIR	0x08
#define	AP_CK_COND	0x20
#define	AP_OFFLINE	0x60
	u_int8_t features;
	u_int8_t sector_count;
	u_int8_t lba_low;
	u_int8_t lba_mid;
	u_int8_t lba_high;
	u_int8_t device;
	u_int8_t command;
	u_int8_t reserved;
	u_int8_t control;
};

struct scsi_maintenance_in
{
        uint8_t  opcode;
        uint8_t  byte2;
#define SERVICE_ACTION_MASK  0x1f
#define SA_RPRT_TRGT_GRP     0x0a
        uint8_t  reserved[4];
	uint8_t  length[4];
	uint8_t  reserved1;
	uint8_t  control;
};

struct ata_pass_16 {
	u_int8_t opcode;
	u_int8_t protocol;
#define	AP_EXTEND	0x01
	u_int8_t flags;
#define	AP_FLAG_TLEN_NO_DATA	(0 << 0)
#define	AP_FLAG_TLEN_FEAT	(1 << 0)
#define	AP_FLAG_TLEN_SECT_CNT	(2 << 0)
#define	AP_FLAG_TLEN_STPSIU	(3 << 0)
#define	AP_FLAG_BYT_BLOK_BYTES	(0 << 2)  
#define	AP_FLAG_BYT_BLOK_BLOCKS	(1 << 2)  
#define	AP_FLAG_TDIR_TO_DEV	(0 << 3)  
#define	AP_FLAG_TDIR_FROM_DEV	(1 << 3)  
#define	AP_FLAG_CHK_COND	(1 << 5)  
	u_int8_t features_ext;
	u_int8_t features;
	u_int8_t sector_count_ext;
	u_int8_t sector_count;
	u_int8_t lba_low_ext;
	u_int8_t lba_low;
	u_int8_t lba_mid_ext;
	u_int8_t lba_mid;
	u_int8_t lba_high_ext;
	u_int8_t lba_high;
	u_int8_t device;
	u_int8_t command;
	u_int8_t control;
};

#define	SC_SCSI_1 0x01
#define	SC_SCSI_2 0x03

/*
 * Opcodes
 */

#define	TEST_UNIT_READY		0x00
#define	REQUEST_SENSE		0x03
#define	READ_6			0x08
#define	WRITE_6			0x0A
#define	INQUIRY			0x12
#define	MODE_SELECT_6		0x15
#define	MODE_SENSE_6		0x1A
#define	START_STOP_UNIT		0x1B
#define	START_STOP		0x1B
#define	RESERVE      		0x16
#define	RELEASE      		0x17
#define	RECEIVE_DIAGNOSTIC	0x1C
#define	SEND_DIAGNOSTIC		0x1D
#define	PREVENT_ALLOW		0x1E
#define	READ_CAPACITY		0x25
#define	READ_10			0x28
#define	WRITE_10		0x2A
#define	POSITION_TO_ELEMENT	0x2B
#define	WRITE_VERIFY_10		0x2E
#define	VERIFY_10		0x2F
#define	SYNCHRONIZE_CACHE	0x35
#define	READ_DEFECT_DATA_10	0x37
#define	WRITE_BUFFER            0x3B
#define	READ_BUFFER             0x3C
#define	CHANGE_DEFINITION	0x40
#define	WRITE_SAME_10		0x41
#define	UNMAP			0x42
#define	LOG_SELECT		0x4C
#define	LOG_SENSE		0x4D
#define	MODE_SELECT_10		0x55
#define	RESERVE_10		0x56
#define	RELEASE_10		0x57
#define	MODE_SENSE_10		0x5A
#define	PERSISTENT_RES_IN	0x5E
#define	PERSISTENT_RES_OUT	0x5F
#define	ATA_PASS_16		0x85
#define	READ_16			0x88
#define	WRITE_16		0x8A
#define	WRITE_VERIFY_16		0x8E
#define	SYNCHRONIZE_CACHE_16	0x91
#define	WRITE_SAME_16		0x93
#define	SERVICE_ACTION_IN	0x9E
#define	REPORT_LUNS		0xA0
#define	ATA_PASS_12		0xA1
#define	MAINTENANCE_IN		0xA3
#define	MAINTENANCE_OUT		0xA4
#define	MOVE_MEDIUM     	0xA5
#define	READ_12			0xA8
#define	WRITE_12		0xAA
#define	WRITE_VERIFY_12		0xAE
#define	READ_ELEMENT_STATUS	0xB8
#define	READ_CD			0xBE

/* Maintenance In Service Action Codes */
#define	REPORT_IDENTIFYING_INFRMATION		0x05
#define	REPORT_TARGET_PORT_GROUPS		0x0A
#define	REPORT_ALIASES				0x0B
#define	REPORT_SUPPORTED_OPERATION_CODES	0x0C
#define	REPORT_SUPPORTED_TASK_MANAGEMENT_FUNCTIONS	0x0D
#define	REPORT_PRIORITY				0x0E
#define	REPORT_TIMESTAMP			0x0F
#define	MANAGEMENT_PROTOCOL_IN			0x10
/* Maintenance Out Service Action Codes */
#define	SET_IDENTIFY_INFORMATION		0x06
#define	SET_TARGET_PORT_GROUPS			0x0A
#define	CHANGE_ALIASES				0x0B
#define	SET_PRIORITY				0x0E
#define	SET_TIMESTAMP				0x0F
#define	MANGAEMENT_PROTOCOL_OUT			0x10

/*
 * Device Types
 */
#define	T_DIRECT	0x00
#define	T_SEQUENTIAL	0x01
#define	T_PRINTER	0x02
#define	T_PROCESSOR	0x03
#define	T_WORM		0x04
#define	T_CDROM		0x05
#define	T_SCANNER	0x06
#define	T_OPTICAL 	0x07
#define	T_CHANGER	0x08
#define	T_COMM		0x09
#define	T_ASC0		0x0a
#define	T_ASC1		0x0b
#define	T_STORARRAY	0x0c
#define	T_ENCLOSURE	0x0d
#define	T_RBC		0x0e
#define	T_OCRW		0x0f
#define	T_OSD		0x11
#define	T_ADC		0x12
#define	T_NODEVICE	0x1f
#define	T_ANY		0xff	/* Used in Quirk table matches */

#define	T_REMOV		1
#define	T_FIXED		0

/*
 * This length is the initial inquiry length used by the probe code, as    
 * well as the length necessary for scsi_print_inquiry() to function 
 * correctly.  If either use requires a different length in the future, 
 * the two values should be de-coupled.
 */
#define	SHORT_INQUIRY_LENGTH	36

struct scsi_inquiry_data
{
	u_int8_t device;
#define	SID_TYPE(inq_data) ((inq_data)->device & 0x1f)
#define	SID_QUAL(inq_data) (((inq_data)->device & 0xE0) >> 5)
#define	SID_QUAL_LU_CONNECTED	0x00	/*
					 * The specified peripheral device
					 * type is currently connected to
					 * logical unit.  If the target cannot
					 * determine whether or not a physical
					 * device is currently connected, it
					 * shall also use this peripheral
					 * qualifier when returning the INQUIRY
					 * data.  This peripheral qualifier
					 * does not mean that the device is
					 * ready for access by the initiator.
					 */
#define	SID_QUAL_LU_OFFLINE	0x01	/*
					 * The target is capable of supporting
					 * the specified peripheral device type
					 * on this logical unit; however, the
					 * physical device is not currently
					 * connected to this logical unit.
					 */
#define	SID_QUAL_RSVD		0x02
#define	SID_QUAL_BAD_LU		0x03	/*
					 * The target is not capable of
					 * supporting a physical device on
					 * this logical unit. For this
					 * peripheral qualifier the peripheral
					 * device type shall be set to 1Fh to
					 * provide compatibility with previous
					 * versions of SCSI. All other
					 * peripheral device type values are
					 * reserved for this peripheral
					 * qualifier.
					 */
#define	SID_QUAL_IS_VENDOR_UNIQUE(inq_data) ((SID_QUAL(inq_data) & 0x08) != 0)
	u_int8_t dev_qual2;
#define	SID_QUAL2	0x7F
#define	SID_IS_REMOVABLE(inq_data) (((inq_data)->dev_qual2 & 0x80) != 0)
	u_int8_t version;
#define	SID_ANSI_REV(inq_data) ((inq_data)->version & 0x07)
#define		SCSI_REV_0		0
#define		SCSI_REV_CCS		1
#define		SCSI_REV_2		2
#define		SCSI_REV_SPC		3
#define		SCSI_REV_SPC2		4
#define		SCSI_REV_SPC3		5
#define		SCSI_REV_SPC4		6

#define	SID_ECMA	0x38
#define	SID_ISO		0xC0
	u_int8_t response_format;
#define	SID_AENC	0x80
#define	SID_TrmIOP	0x40
#define	SID_NormACA	0x20
#define	SID_HiSup	0x10
	u_int8_t additional_length;
#define	SID_ADDITIONAL_LENGTH(iqd)					\
	((iqd)->additional_length +					\
	__offsetof(struct scsi_inquiry_data, additional_length) + 1)
	u_int8_t spc3_flags;
#define	SPC3_SID_PROTECT	0x01
#define	SPC3_SID_3PC		0x08
#define	SPC3_SID_TPGS_MASK	0x30
#define	SPC3_SID_TPGS_IMPLICIT	0x10
#define	SPC3_SID_TPGS_EXPLICIT	0x20
#define	SPC3_SID_ACC		0x40
#define	SPC3_SID_SCCS		0x80
	u_int8_t spc2_flags;
#define	SPC2_SID_ADDR16		0x01
#define	SPC2_SID_MChngr 	0x08
#define	SPC2_SID_MultiP 	0x10
#define	SPC2_SID_EncServ	0x40
#define	SPC2_SID_BQueue		0x80

#define	INQ_DATA_TQ_ENABLED(iqd)				\
    ((SID_ANSI_REV(iqd) < SCSI_REV_SPC2)? ((iqd)->flags & SID_CmdQue) :	\
    (((iqd)->flags & SID_CmdQue) && !((iqd)->spc2_flags & SPC2_SID_BQueue)) || \
    (!((iqd)->flags & SID_CmdQue) && ((iqd)->spc2_flags & SPC2_SID_BQueue)))

	u_int8_t flags;
#define	SID_SftRe	0x01
#define	SID_CmdQue	0x02
#define	SID_Linked	0x08
#define	SID_Sync	0x10
#define	SID_WBus16	0x20
#define	SID_WBus32	0x40
#define	SID_RelAdr	0x80
#define	SID_VENDOR_SIZE   8
	char	 vendor[SID_VENDOR_SIZE];
#define	SID_PRODUCT_SIZE  16
	char	 product[SID_PRODUCT_SIZE];
#define	SID_REVISION_SIZE 4
	char	 revision[SID_REVISION_SIZE];
	/*
	 * The following fields were taken from SCSI Primary Commands - 2
	 * (SPC-2) Revision 14, Dated 11 November 1999
	 */
#define	SID_VENDOR_SPECIFIC_0_SIZE	20
	u_int8_t vendor_specific0[SID_VENDOR_SPECIFIC_0_SIZE];
	/*
	 * An extension of SCSI Parallel Specific Values
	 */
#define	SID_SPI_IUS		0x01
#define	SID_SPI_QAS		0x02
#define	SID_SPI_CLOCK_ST	0x00
#define	SID_SPI_CLOCK_DT	0x04
#define	SID_SPI_CLOCK_DT_ST	0x0C
#define	SID_SPI_MASK		0x0F
	u_int8_t spi3data;
	u_int8_t reserved2;
	/*
	 * Version Descriptors, stored 2 byte values.
	 */
	u_int8_t version1[2];
	u_int8_t version2[2];
	u_int8_t version3[2];
	u_int8_t version4[2];
	u_int8_t version5[2];
	u_int8_t version6[2];
	u_int8_t version7[2];
	u_int8_t version8[2];

	u_int8_t reserved3[22];

#define	SID_VENDOR_SPECIFIC_1_SIZE	160
	u_int8_t vendor_specific1[SID_VENDOR_SPECIFIC_1_SIZE];
};

/*
 * This structure is more suited to initiator operation, because the
 * maximum number of supported pages is already allocated.
 */
struct scsi_vpd_supported_page_list
{
	u_int8_t device;
	u_int8_t page_code;
#define	SVPD_SUPPORTED_PAGE_LIST	0x00
#define	SVPD_SUPPORTED_PAGES_HDR_LEN	4
	u_int8_t reserved;
	u_int8_t length;	/* number of VPD entries */
#define	SVPD_SUPPORTED_PAGES_SIZE	251
	u_int8_t list[SVPD_SUPPORTED_PAGES_SIZE];
};

/*
 * This structure is more suited to target operation, because the
 * number of supported pages is left to the user to allocate.
 */
struct scsi_vpd_supported_pages
{
	u_int8_t device;
	u_int8_t page_code;
	u_int8_t reserved;
#define	SVPD_SUPPORTED_PAGES	0x00
	u_int8_t length;
	u_int8_t page_list[0];
};


struct scsi_vpd_unit_serial_number
{
	u_int8_t device;
	u_int8_t page_code;
#define	SVPD_UNIT_SERIAL_NUMBER	0x80
	u_int8_t reserved;
	u_int8_t length; /* serial number length */
#define	SVPD_SERIAL_NUM_SIZE 251
	u_int8_t serial_num[SVPD_SERIAL_NUM_SIZE];
};

struct scsi_vpd_device_id
{
	u_int8_t device;
	u_int8_t page_code;
#define	SVPD_DEVICE_ID			0x83
#define	SVPD_DEVICE_ID_MAX_SIZE		252
#define	SVPD_DEVICE_ID_HDR_LEN \
    __offsetof(struct scsi_vpd_device_id, desc_list)
	u_int8_t length[2];
	u_int8_t desc_list[];
};

struct scsi_vpd_id_descriptor
{
	u_int8_t	proto_codeset;
#define	SCSI_PROTO_FC		0x00
#define	SCSI_PROTO_SPI		0x01
#define	SCSI_PROTO_SSA		0x02
#define	SCSI_PROTO_1394		0x03
#define	SCSI_PROTO_RDMA		0x04
#define	SCSI_PROTO_ISCSI	0x05
#define	SCSI_PROTO_SAS		0x06
#define	SCSI_PROTO_ADT		0x07
#define	SCSI_PROTO_ATA		0x08
#define	SVPD_ID_PROTO_SHIFT	4
#define	SVPD_ID_CODESET_BINARY	0x01
#define	SVPD_ID_CODESET_ASCII	0x02
#define	SVPD_ID_CODESET_UTF8	0x03
#define	SVPD_ID_CODESET_MASK	0x0f
	u_int8_t	id_type;
#define	SVPD_ID_PIV		0x80
#define	SVPD_ID_ASSOC_LUN	0x00
#define	SVPD_ID_ASSOC_PORT	0x10
#define	SVPD_ID_ASSOC_TARGET	0x20
#define	SVPD_ID_ASSOC_MASK	0x30
#define	SVPD_ID_TYPE_VENDOR	0x00
#define	SVPD_ID_TYPE_T10	0x01
#define	SVPD_ID_TYPE_EUI64	0x02
#define	SVPD_ID_TYPE_NAA	0x03
#define	SVPD_ID_TYPE_RELTARG	0x04
#define	SVPD_ID_TYPE_TPORTGRP	0x05
#define	SVPD_ID_TYPE_LUNGRP	0x06
#define	SVPD_ID_TYPE_MD5_LUN_ID	0x07
#define	SVPD_ID_TYPE_SCSI_NAME	0x08
#define	SVPD_ID_TYPE_MASK	0x0f
	u_int8_t	reserved;
	u_int8_t	length;
#define	SVPD_DEVICE_ID_DESC_HDR_LEN \
    __offsetof(struct scsi_vpd_id_descriptor, identifier) 
	u_int8_t	identifier[];
};

struct scsi_vpd_id_t10
{
	u_int8_t	vendor[8];
	u_int8_t	vendor_spec_id[0];
};

struct scsi_vpd_id_eui64
{
	u_int8_t	ieee_company_id[3];
	u_int8_t	extension_id[5];
};

struct scsi_vpd_id_naa_basic
{
	uint8_t naa;
	/* big endian, packed:
	uint8_t	naa : 4;
	uint8_t naa_desig : 4;
	*/
#define	SVPD_ID_NAA_NAA_SHIFT		4
#define	SVPD_ID_NAA_IEEE_EXT		0x02
#define	SVPD_ID_NAA_LOCAL_REG		0x03
#define	SVPD_ID_NAA_IEEE_REG		0x05
#define	SVPD_ID_NAA_IEEE_REG_EXT	0x06
	uint8_t	naa_data[];
};

struct scsi_vpd_id_naa_ieee_extended_id
{
	uint8_t naa;
	uint8_t vendor_specific_id_a;
	uint8_t ieee_company_id[3];
	uint8_t vendor_specific_id_b[4];
};

struct scsi_vpd_id_naa_local_reg
{
	uint8_t naa;
	uint8_t local_value[7];
};

struct scsi_vpd_id_naa_ieee_reg
{
	uint8_t naa;
	uint8_t reg_value[7];
	/* big endian, packed:
	uint8_t naa_basic : 4;
	uint8_t ieee_company_id_0 : 4;
	uint8_t ieee_company_id_1[2];
	uint8_t ieee_company_id_2 : 4;
	uint8_t vendor_specific_id_0 : 4;
	uint8_t vendor_specific_id_1[4];
	*/
};

struct scsi_vpd_id_naa_ieee_reg_extended
{
	uint8_t naa;
	uint8_t reg_value[15];
	/* big endian, packed:
	uint8_t naa_basic : 4;
	uint8_t ieee_company_id_0 : 4;
	uint8_t ieee_company_id_1[2];
	uint8_t ieee_company_id_2 : 4;
	uint8_t vendor_specific_id_0 : 4;
	uint8_t vendor_specific_id_1[4];
	uint8_t vendor_specific_id_ext[8];
	*/
};

struct scsi_vpd_id_rel_trgt_port_id
{
	uint8_t obsolete[2];
	uint8_t rel_trgt_port_id[2];
};

struct scsi_vpd_id_trgt_port_grp_id
{
	uint8_t reserved[2];
	uint8_t trgt_port_grp[2];
};

struct scsi_vpd_id_lun_grp_id
{
	uint8_t reserved[2];
	uint8_t log_unit_grp[2];
};

struct scsi_vpd_id_md5_lun_id
{
	uint8_t lun_id[16];
};

struct scsi_vpd_id_scsi_name
{
	uint8_t name_string[256];
};

struct scsi_service_action_in
{
	uint8_t opcode;
	uint8_t service_action;
	uint8_t action_dependent[13];
	uint8_t control;
};

struct scsi_diag_page {
	uint8_t page_code;
	uint8_t page_specific_flags;
	uint8_t length[2];
	uint8_t params[0];
};

/*
 * ATA Information VPD Page based on
 * T10/2126-D Revision 04
 */
#define SVPD_ATA_INFORMATION		0x89

/*
 * Block Device Characteristics VPD Page based on
 * T10/1799-D Revision 31
 */
struct scsi_vpd_block_characteristics
{
	u_int8_t device;
	u_int8_t page_code;
#define SVPD_BDC			0xB1
	u_int8_t page_length[2];
	u_int8_t medium_rotation_rate[2];
#define SVPD_BDC_RATE_NOT_REPORTED	0x00
#define SVPD_BDC_RATE_NON_ROTATING	0x01
	u_int8_t reserved1;
	u_int8_t nominal_form_factor;
#define SVPD_BDC_FORM_NOT_REPORTED	0x00
#define SVPD_BDC_FORM_5_25INCH		0x01
#define SVPD_BDC_FORM_3_5INCH		0x02
#define SVPD_BDC_FORM_2_5INCH		0x03
#define SVPD_BDC_FORM_1_5INCH		0x04
#define SVPD_BDC_FORM_LESSTHAN_1_5INCH	0x05
	u_int8_t reserved2[56];
};

/*
 * Logical Block Provisioning VPD Page based on
 * T10/1799-D Revision 31
 */
struct scsi_vpd_logical_block_prov
{
	u_int8_t device;
	u_int8_t page_code;
#define	SVPD_LBP		0xB2
	u_int8_t page_length[2];
#define SVPD_LBP_PL_BASIC	0x04
	u_int8_t threshold_exponent;
	u_int8_t flags;
#define SVPD_LBP_UNMAP		0x80
#define SVPD_LBP_WS16		0x40
#define SVPD_LBP_WS10		0x20
#define SVPD_LBP_RZ		0x04
#define SVPD_LBP_ANC_SUP	0x02
#define SVPD_LBP_DP		0x01
	u_int8_t prov_type;
#define SVPD_LBP_RESOURCE	0x01
#define SVPD_LBP_THIN		0x02
	u_int8_t reserved;
	/*
	 * Provisioning Group Descriptor can be here if SVPD_LBP_DP is set
	 * Its size can be determined from page_length - 4
	 */
};

/*
 * Block Limits VDP Page based on
 * T10/1799-D Revision 31
 */
struct scsi_vpd_block_limits
{
	u_int8_t device;
	u_int8_t page_code;
#define	SVPD_BLOCK_LIMITS	0xB0
	u_int8_t page_length[2];
#define SVPD_BL_PL_BASIC	0x10
#define SVPD_BL_PL_TP		0x3C
	u_int8_t reserved1;
	u_int8_t max_cmp_write_len;
	u_int8_t opt_txfer_len_grain[2];
	u_int8_t max_txfer_len[4];
	u_int8_t opt_txfer_len[4];
	u_int8_t max_prefetch[4];
	u_int8_t max_unmap_lba_cnt[4];
	u_int8_t max_unmap_blk_cnt[4];
	u_int8_t opt_unmap_grain[4];
	u_int8_t unmap_grain_align[4];
	u_int8_t max_write_same_length[8];
	u_int8_t reserved2[20];
};

struct scsi_read_capacity
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SRC_RELADR	0x01
	u_int8_t addr[4];
	u_int8_t unused[2];
	u_int8_t pmi;
#define	SRC_PMI		0x01
	u_int8_t control;
};

struct scsi_read_capacity_16
{
	uint8_t opcode;
#define	SRC16_SERVICE_ACTION	0x10
	uint8_t service_action;
	uint8_t addr[8];
	uint8_t alloc_len[4];
#define	SRC16_PMI		0x01
#define	SRC16_RELADR		0x02
	uint8_t reladr;
	uint8_t control;
};

struct scsi_read_capacity_data
{
	u_int8_t addr[4];
	u_int8_t length[4];
};

struct scsi_read_capacity_data_long
{
	uint8_t addr[8];
	uint8_t length[4];
#define	SRC16_PROT_EN		0x01
#define	SRC16_P_TYPE		0x0e
#define	SRC16_PTYPE_1		0x00
#define	SRC16_PTYPE_2		0x02
#define	SRC16_PTYPE_3		0x04
	uint8_t prot;
#define	SRC16_LBPPBE		0x0f
#define	SRC16_PI_EXPONENT	0xf0
#define	SRC16_PI_EXPONENT_SHIFT	4
	uint8_t prot_lbppbe;
#define	SRC16_LALBA		0x3f
#define	SRC16_LBPRZ		0x40
#define	SRC16_LBPME		0x80
/*
 * Alternate versions of these macros that are intended for use on a 16-bit
 * version of the lalba_lbp field instead of the array of 2 8 bit numbers.
 */
#define	SRC16_LALBA_A		0x3fff
#define	SRC16_LBPRZ_A		0x4000
#define	SRC16_LBPME_A		0x8000
	uint8_t lalba_lbp[2];
	uint8_t	reserved[16];
};

struct scsi_report_luns
{
	uint8_t opcode;
	uint8_t reserved1;
#define	RPL_REPORT_DEFAULT	0x00
#define	RPL_REPORT_WELLKNOWN	0x01
#define	RPL_REPORT_ALL		0x02
	uint8_t select_report;
	uint8_t reserved2[3];
	uint8_t length[4];
	uint8_t reserved3;
	uint8_t control;
};

struct scsi_report_luns_lundata {
	uint8_t lundata[8];
#define	RPL_LUNDATA_PERIPH_BUS_MASK	0x3f
#define	RPL_LUNDATA_FLAT_LUN_MASK	0x3f
#define	RPL_LUNDATA_FLAT_LUN_BITS	0x06
#define	RPL_LUNDATA_LUN_TARG_MASK	0x3f
#define	RPL_LUNDATA_LUN_BUS_MASK	0xe0
#define	RPL_LUNDATA_LUN_LUN_MASK	0x1f
#define	RPL_LUNDATA_EXT_LEN_MASK	0x30
#define	RPL_LUNDATA_EXT_EAM_MASK	0x0f
#define	RPL_LUNDATA_EXT_EAM_WK		0x01
#define	RPL_LUNDATA_EXT_EAM_NOT_SPEC	0x0f
#define	RPL_LUNDATA_ATYP_MASK	0xc0	/* MBZ for type 0 lun */
#define	RPL_LUNDATA_ATYP_PERIPH	0x00
#define	RPL_LUNDATA_ATYP_FLAT	0x40
#define	RPL_LUNDATA_ATYP_LUN	0x80
#define	RPL_LUNDATA_ATYP_EXTLUN	0xc0
};

struct scsi_report_luns_data {
	u_int8_t length[4];	/* length of LUN inventory, in bytes */
	u_int8_t reserved[4];	/* unused */
	/*
	 * LUN inventory- we only support the type zero form for now.
	 */
	struct scsi_report_luns_lundata luns[0];
};

struct scsi_target_group
{
	uint8_t opcode;
	uint8_t service_action;
#define	STG_PDF_LENGTH		0x00
#define	RPL_PDF_EXTENDED	0x20
	uint8_t reserved1[4];
	uint8_t length[4];
	uint8_t reserved2;
	uint8_t control;
};

struct scsi_target_port_descriptor {
	uint8_t	reserved[2];
	uint8_t	relative_target_port_identifier[2];
	uint8_t desc_list[];
};

struct scsi_target_port_group_descriptor {
	uint8_t	pref_state;
#define	TPG_PRIMARY				0x80
#define	TPG_ASYMMETRIC_ACCESS_STATE_MASK	0xf
#define	TPG_ASYMMETRIC_ACCESS_OPTIMIZED		0x0
#define	TPG_ASYMMETRIC_ACCESS_NONOPTIMIZED	0x1
#define	TPG_ASYMMETRIC_ACCESS_STANDBY		0x2
#define	TPG_ASYMMETRIC_ACCESS_UNAVAILABLE	0x3
#define	TPG_ASYMMETRIC_ACCESS_LBA_DEPENDENT	0x4
#define	TPG_ASYMMETRIC_ACCESS_OFFLINE		0xE
#define	TPG_ASYMMETRIC_ACCESS_TRANSITIONING	0xF
	uint8_t support;
#define	TPG_AO_SUP	0x01
#define	TPG_AN_SUP	0x02
#define	TPG_S_SUP	0x04
#define	TPG_U_SUP	0x08
#define	TPG_LBD_SUP	0x10
#define	TPG_O_SUP	0x40
#define	TPG_T_SUP	0x80
	uint8_t target_port_group[2];
	uint8_t reserved;
	uint8_t status;
#define TPG_UNAVLBL      0
#define TPG_SET_BY_STPG  0x01
#define TPG_IMPLICIT     0x02
	uint8_t vendor_specific;
	uint8_t	target_port_count;
	struct scsi_target_port_descriptor descriptors[];
};

struct scsi_target_group_data {
	uint8_t length[4];	/* length of returned data, in bytes */
	struct scsi_target_port_group_descriptor groups[];
};

struct scsi_target_group_data_extended {
	uint8_t length[4];	/* length of returned data, in bytes */
	uint8_t format_type;	/* STG_PDF_LENGTH or RPL_PDF_EXTENDED */
	uint8_t	implicit_transition_time;
	uint8_t reserved[2];
	struct scsi_target_port_group_descriptor groups[];
};


typedef enum {
	SSD_TYPE_NONE,
	SSD_TYPE_FIXED,
	SSD_TYPE_DESC
} scsi_sense_data_type;

typedef enum {
	SSD_ELEM_NONE,
	SSD_ELEM_SKIP,
	SSD_ELEM_DESC,
	SSD_ELEM_SKS,
	SSD_ELEM_COMMAND,
	SSD_ELEM_INFO,
	SSD_ELEM_FRU,
	SSD_ELEM_STREAM,
	SSD_ELEM_MAX
} scsi_sense_elem_type;


struct scsi_sense_data
{
	uint8_t error_code;
	/*
	 * SPC-4 says that the maximum length of sense data is 252 bytes.
	 * So this structure is exactly 252 bytes log.
	 */
#define	SSD_FULL_SIZE 252
	uint8_t sense_buf[SSD_FULL_SIZE - 1];
	/*
	 * XXX KDM is this still a reasonable minimum size?
	 */
#define	SSD_MIN_SIZE 18
	/*
	 * Maximum value for the extra_len field in the sense data.
	 */
#define	SSD_EXTRA_MAX 244
};

/*
 * Fixed format sense data.
 */
struct scsi_sense_data_fixed
{
	u_int8_t error_code;
#define	SSD_ERRCODE			0x7F
#define		SSD_CURRENT_ERROR	0x70
#define		SSD_DEFERRED_ERROR	0x71
#define	SSD_ERRCODE_VALID	0x80	
	u_int8_t segment;
	u_int8_t flags;
#define	SSD_KEY				0x0F
#define		SSD_KEY_NO_SENSE	0x00
#define		SSD_KEY_RECOVERED_ERROR	0x01
#define		SSD_KEY_NOT_READY	0x02
#define		SSD_KEY_MEDIUM_ERROR	0x03
#define		SSD_KEY_HARDWARE_ERROR	0x04
#define		SSD_KEY_ILLEGAL_REQUEST	0x05
#define		SSD_KEY_UNIT_ATTENTION	0x06
#define		SSD_KEY_DATA_PROTECT	0x07
#define		SSD_KEY_BLANK_CHECK	0x08
#define		SSD_KEY_Vendor_Specific	0x09
#define		SSD_KEY_COPY_ABORTED	0x0a
#define		SSD_KEY_ABORTED_COMMAND	0x0b		
#define		SSD_KEY_EQUAL		0x0c
#define		SSD_KEY_VOLUME_OVERFLOW	0x0d
#define		SSD_KEY_MISCOMPARE	0x0e
#define		SSD_KEY_COMPLETED	0x0f			
#define	SSD_ILI		0x20
#define	SSD_EOM		0x40
#define	SSD_FILEMARK	0x80
	u_int8_t info[4];
	u_int8_t extra_len;
	u_int8_t cmd_spec_info[4];
	u_int8_t add_sense_code;
	u_int8_t add_sense_code_qual;
	u_int8_t fru;
	u_int8_t sense_key_spec[3];
#define	SSD_SCS_VALID		0x80
#define	SSD_FIELDPTR_CMD	0x40
#define	SSD_BITPTR_VALID	0x08
#define	SSD_BITPTR_VALUE	0x07
	u_int8_t extra_bytes[14];
#define	SSD_FIXED_IS_PRESENT(sense, length, field) 			\
	((length >= (offsetof(struct scsi_sense_data_fixed, field) +	\
	sizeof(sense->field))) ? 1 :0)
#define	SSD_FIXED_IS_FILLED(sense, field) 				\
	((((offsetof(struct scsi_sense_data_fixed, field) +		\
	sizeof(sense->field)) -						\
	(offsetof(struct scsi_sense_data_fixed, extra_len) +		\
	sizeof(sense->extra_len))) <= sense->extra_len) ? 1 : 0)
};

/*
 * Descriptor format sense data definitions.
 * Introduced in SPC-3.
 */
struct scsi_sense_data_desc 
{
	uint8_t	error_code;
#define	SSD_DESC_CURRENT_ERROR	0x72
#define	SSD_DESC_DEFERRED_ERROR	0x73
	uint8_t sense_key;
	uint8_t	add_sense_code;
	uint8_t	add_sense_code_qual;
	uint8_t	reserved[3];
	/*
	 * Note that SPC-4, section 4.5.2.1 says that the extra_len field
	 * must be less than or equal to 244.
	 */
	uint8_t	extra_len;
	uint8_t	sense_desc[0];
#define	SSD_DESC_IS_PRESENT(sense, length, field) 			\
	((length >= (offsetof(struct scsi_sense_data_desc, field) +	\
	sizeof(sense->field))) ? 1 :0)
};

struct scsi_sense_desc_header
{
	uint8_t desc_type;
	uint8_t length;
};
/*
 * The information provide in the Information descriptor is device type or
 * command specific information, and defined in a command standard.
 *
 * Note that any changes to the field names or positions in this structure,
 * even reserved fields, should be accompanied by an examination of the
 * code in ctl_set_sense() that uses them.
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
struct scsi_sense_info
{
	uint8_t	desc_type;
#define	SSD_DESC_INFO	0x00
	uint8_t	length;
	uint8_t	byte2;
#define	SSD_INFO_VALID	0x80
	uint8_t	reserved;
	uint8_t	info[8];
};

/*
 * Command-specific information depends on the command for which the
 * reported condition occured.
 *
 * Note that any changes to the field names or positions in this structure,
 * even reserved fields, should be accompanied by an examination of the
 * code in ctl_set_sense() that uses them.
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
struct scsi_sense_command
{
	uint8_t	desc_type;
#define	SSD_DESC_COMMAND	0x01
	uint8_t	length;
	uint8_t	reserved[2];
	uint8_t	command_info[8];
};

/*
 * Sense key specific descriptor.  The sense key specific data format
 * depends on the sense key in question.
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
struct scsi_sense_sks
{
	uint8_t	desc_type;
#define	SSD_DESC_SKS		0x02
	uint8_t	length;
	uint8_t reserved1[2];
	uint8_t	sense_key_spec[3];
#define	SSD_SKS_VALID		0x80
	uint8_t reserved2;
};

/*
 * This is used for the Illegal Request sense key (0x05) only.
 */
struct scsi_sense_sks_field
{
	uint8_t	byte0;
#define	SSD_SKS_FIELD_VALID	0x80
#define	SSD_SKS_FIELD_CMD	0x40
#define	SSD_SKS_BPV		0x08
#define	SSD_SKS_BIT_VALUE	0x07
	uint8_t	field[2];
};


/* 
 * This is used for the Hardware Error (0x04), Medium Error (0x03) and
 * Recovered Error (0x01) sense keys.
 */
struct scsi_sense_sks_retry
{
	uint8_t byte0;
#define	SSD_SKS_RETRY_VALID	0x80
	uint8_t actual_retry_count[2];
};

/*
 * Used with the NO Sense (0x00) or Not Ready (0x02) sense keys.
 */
struct scsi_sense_sks_progress
{
	uint8_t byte0;
#define	SSD_SKS_PROGRESS_VALID	0x80
	uint8_t progress[2];
#define	SSD_SKS_PROGRESS_DENOM	0x10000
};

/*
 * Used with the Copy Aborted (0x0a) sense key.
 */
struct scsi_sense_sks_segment
{
	uint8_t byte0;
#define	SSD_SKS_SEGMENT_VALID	0x80
#define	SSD_SKS_SEGMENT_SD	0x20
#define	SSD_SKS_SEGMENT_BPV	0x08
#define	SSD_SKS_SEGMENT_BITPTR	0x07
	uint8_t field[2];
};

/*
 * Used with the Unit Attention (0x06) sense key.
 *
 * This is currently used to indicate that the unit attention condition
 * queue has overflowed (when the overflow bit is set).
 */
struct scsi_sense_sks_overflow
{
	uint8_t byte0;
#define	SSD_SKS_OVERFLOW_VALID	0x80
#define	SSD_SKS_OVERFLOW_SET	0x01
	uint8_t	reserved[2];
};

/*
 * This specifies which component is associated with the sense data.  There
 * is no standard meaning for the fru value.
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
struct scsi_sense_fru
{
	uint8_t	desc_type;
#define	SSD_DESC_FRU		0x03
	uint8_t	length;
	uint8_t reserved;
	uint8_t fru;
};

/*
 * Used for Stream commands, defined in SSC-4.
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
 
struct scsi_sense_stream
{
	uint8_t	desc_type;
#define	SSD_DESC_STREAM		0x04
	uint8_t	length;
	uint8_t	reserved;
	uint8_t	byte3;
#define	SSD_DESC_STREAM_FM	0x80
#define	SSD_DESC_STREAM_EOM	0x40
#define	SSD_DESC_STREAM_ILI	0x20
};

/*
 * Used for Block commands, defined in SBC-3.
 *
 * This is currently (as of SBC-3) only used for the Incorrect Length
 * Indication (ILI) bit, which says that the data length requested in the
 * READ LONG or WRITE LONG command did not match the length of the logical
 * block.
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
struct scsi_sense_block
{
	uint8_t	desc_type;
#define	SSD_DESC_BLOCK		0x05
	uint8_t	length;
	uint8_t	reserved;
	uint8_t	byte3;
#define	SSD_DESC_BLOCK_ILI	0x20
};

/*
 * Used for Object-Based Storage Devices (OSD-3).
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
struct scsi_sense_osd_objid
{
	uint8_t	desc_type;
#define	SSD_DESC_OSD_OBJID	0x06
	uint8_t	length;
	uint8_t	reserved[6];
	/*
	 * XXX KDM provide the bit definitions here?  There are a lot of
	 * them, and we don't have an OSD driver yet.
	 */
	uint8_t	not_init_cmds[4];
	uint8_t	completed_cmds[4];
	uint8_t	partition_id[8];
	uint8_t	object_id[8];
};

/*
 * Used for Object-Based Storage Devices (OSD-3).
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
struct scsi_sense_osd_integrity
{
	uint8_t	desc_type;
#define	SSD_DESC_OSD_INTEGRITY	0x07
	uint8_t	length;
	uint8_t	integ_check_val[32];
};

/*
 * Used for Object-Based Storage Devices (OSD-3).
 *
 * Maximum descriptors allowed: 1 (as of SPC-4)
 */
struct scsi_sense_osd_attr_id
{
	uint8_t	desc_type;
#define	SSD_DESC_OSD_ATTR_ID	0x08
	uint8_t	length;
	uint8_t	reserved[2];
	uint8_t	attr_desc[0];
};

/*
 * Used with Sense keys No Sense (0x00) and Not Ready (0x02).
 *
 * Maximum descriptors allowed: 32 (as of SPC-4)
 */
struct scsi_sense_progress
{
	uint8_t	desc_type;
#define	SSD_DESC_PROGRESS	0x0a
	uint8_t	length;
	uint8_t	sense_key;
	uint8_t	add_sense_code;
	uint8_t	add_sense_code_qual;
	uint8_t reserved;
	uint8_t	progress[2];
};

/*
 * This is typically forwarded as the result of an EXTENDED COPY command.
 *
 * Maximum descriptors allowed: 2 (as of SPC-4)
 */
struct scsi_sense_forwarded
{
	uint8_t	desc_type;
#define	SSD_DESC_FORWARDED	0x0c
	uint8_t	length;
	uint8_t	byte2;
#define	SSD_FORWARDED_FSDT	0x80
#define	SSD_FORWARDED_SDS_MASK	0x0f
#define	SSD_FORWARDED_SDS_UNK	0x00
#define	SSD_FORWARDED_SDS_EXSRC	0x01
#define	SSD_FORWARDED_SDS_EXDST	0x02
};

/*
 * Vendor-specific sense descriptor.  The desc_type field will be in the
 * range bewteen MIN and MAX inclusive.
 */
struct scsi_sense_vendor
{
	uint8_t	desc_type;
#define	SSD_DESC_VENDOR_MIN	0x80
#define	SSD_DESC_VENDOR_MAX	0xff
	uint8_t length;
	uint8_t	data[0];
};

struct scsi_mode_header_6
{
	u_int8_t data_length;	/* Sense data length */
	u_int8_t medium_type;
	u_int8_t dev_spec;
	u_int8_t blk_desc_len;
};

struct scsi_mode_header_10
{
	u_int8_t data_length[2];/* Sense data length */
	u_int8_t medium_type;
	u_int8_t dev_spec;
	u_int8_t unused[2];
	u_int8_t blk_desc_len[2];
};

struct scsi_mode_page_header
{
	u_int8_t page_code;
#define	SMPH_PS		0x80
#define	SMPH_SPF	0x40
#define	SMPH_PC_MASK	0x3f
	u_int8_t page_length;
};

struct scsi_mode_page_header_sp
{
	uint8_t page_code;
	uint8_t subpage;
	uint8_t page_length[2];
};


struct scsi_mode_blk_desc
{
	u_int8_t density;
	u_int8_t nblocks[3];
	u_int8_t reserved;
	u_int8_t blklen[3];
};

#define	SCSI_DEFAULT_DENSITY	0x00	/* use 'default' density */
#define	SCSI_SAME_DENSITY	0x7f	/* use 'same' density- >= SCSI-2 only */


/*
 * Status Byte
 */
#define	SCSI_STATUS_OK			0x00
#define	SCSI_STATUS_CHECK_COND		0x02
#define	SCSI_STATUS_COND_MET		0x04
#define	SCSI_STATUS_BUSY		0x08
#define	SCSI_STATUS_INTERMED		0x10
#define	SCSI_STATUS_INTERMED_COND_MET	0x14
#define	SCSI_STATUS_RESERV_CONFLICT	0x18
#define	SCSI_STATUS_CMD_TERMINATED	0x22	/* Obsolete in SAM-2 */
#define	SCSI_STATUS_QUEUE_FULL		0x28
#define	SCSI_STATUS_ACA_ACTIVE		0x30
#define	SCSI_STATUS_TASK_ABORTED	0x40

struct scsi_inquiry_pattern {
	u_int8_t   type;
	u_int8_t   media_type;
#define	SIP_MEDIA_REMOVABLE	0x01
#define	SIP_MEDIA_FIXED		0x02
	const char *vendor;
	const char *product;
	const char *revision;
}; 

struct scsi_static_inquiry_pattern {
	u_int8_t   type;
	u_int8_t   media_type;
	char       vendor[SID_VENDOR_SIZE+1];
	char       product[SID_PRODUCT_SIZE+1];
	char       revision[SID_REVISION_SIZE+1];
};

struct scsi_sense_quirk_entry {
	struct scsi_inquiry_pattern	inq_pat;
	int				num_sense_keys;
	int				num_ascs;
	struct sense_key_table_entry	*sense_key_info;
	struct asc_table_entry		*asc_info;
};

struct sense_key_table_entry {
	u_int8_t    sense_key;
	u_int32_t   action;
	const char *desc;
};

struct asc_table_entry {
	u_int8_t    asc;
	u_int8_t    ascq;
	u_int32_t   action;
	const char *desc;
};

struct op_table_entry {
	u_int8_t    opcode;
	u_int32_t   opmask;
	const char  *desc;
};

struct scsi_op_quirk_entry {
	struct scsi_inquiry_pattern	inq_pat;
	int				num_ops;
	struct op_table_entry		*op_table;
};

typedef enum {
	SSS_FLAG_NONE		= 0x00,
	SSS_FLAG_PRINT_COMMAND	= 0x01
} scsi_sense_string_flags;

struct ccb_scsiio;
struct cam_periph;
union  ccb;
#ifndef _KERNEL
struct cam_device;
#endif

extern const char *scsi_sense_key_text[];

struct sbuf;

__BEGIN_DECLS
void scsi_sense_desc(int sense_key, int asc, int ascq,
		     struct scsi_inquiry_data *inq_data,
		     const char **sense_key_desc, const char **asc_desc);
scsi_sense_action scsi_error_action(struct ccb_scsiio* csio,
				    struct scsi_inquiry_data *inq_data,
				    u_int32_t sense_flags);
const char *	scsi_status_string(struct ccb_scsiio *csio);

void scsi_desc_iterate(struct scsi_sense_data_desc *sense, u_int sense_len,
		       int (*iter_func)(struct scsi_sense_data_desc *sense,
					u_int, struct scsi_sense_desc_header *,
					void *), void *arg);
uint8_t *scsi_find_desc(struct scsi_sense_data_desc *sense, u_int sense_len,
			uint8_t desc_type);
void scsi_set_sense_data(struct scsi_sense_data *sense_data, 
			 scsi_sense_data_type sense_format, int current_error,
			 int sense_key, int asc, int ascq, ...) ;
void scsi_set_sense_data_va(struct scsi_sense_data *sense_data,
			    scsi_sense_data_type sense_format,
			    int current_error, int sense_key, int asc,
			    int ascq, va_list ap);
int scsi_get_sense_info(struct scsi_sense_data *sense_data, u_int sense_len,
			uint8_t info_type, uint64_t *info,
			int64_t *signed_info);
int scsi_get_sks(struct scsi_sense_data *sense_data, u_int sense_len,
		 uint8_t *sks);
int scsi_get_block_info(struct scsi_sense_data *sense_data, u_int sense_len,
			struct scsi_inquiry_data *inq_data,
			uint8_t *block_bits);
int scsi_get_stream_info(struct scsi_sense_data *sense_data, u_int sense_len,
			 struct scsi_inquiry_data *inq_data,
			 uint8_t *stream_bits);
void scsi_info_sbuf(struct sbuf *sb, uint8_t *cdb, int cdb_len,
		    struct scsi_inquiry_data *inq_data, uint64_t info);
void scsi_command_sbuf(struct sbuf *sb, uint8_t *cdb, int cdb_len,
		       struct scsi_inquiry_data *inq_data, uint64_t csi);
void scsi_progress_sbuf(struct sbuf *sb, uint16_t progress);
int scsi_sks_sbuf(struct sbuf *sb, int sense_key, uint8_t *sks);
void scsi_fru_sbuf(struct sbuf *sb, uint64_t fru);
void scsi_stream_sbuf(struct sbuf *sb, uint8_t stream_bits, uint64_t info);
void scsi_block_sbuf(struct sbuf *sb, uint8_t block_bits, uint64_t info);
void scsi_sense_info_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			  u_int sense_len, uint8_t *cdb, int cdb_len,
			  struct scsi_inquiry_data *inq_data,
			  struct scsi_sense_desc_header *header);

void scsi_sense_command_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			     u_int sense_len, uint8_t *cdb, int cdb_len,
			     struct scsi_inquiry_data *inq_data,
			     struct scsi_sense_desc_header *header);
void scsi_sense_sks_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			 u_int sense_len, uint8_t *cdb, int cdb_len,
			 struct scsi_inquiry_data *inq_data,
			 struct scsi_sense_desc_header *header);
void scsi_sense_fru_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			 u_int sense_len, uint8_t *cdb, int cdb_len,
			 struct scsi_inquiry_data *inq_data,
			 struct scsi_sense_desc_header *header);
void scsi_sense_stream_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			    u_int sense_len, uint8_t *cdb, int cdb_len,
			    struct scsi_inquiry_data *inq_data,
			    struct scsi_sense_desc_header *header);
void scsi_sense_block_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			   u_int sense_len, uint8_t *cdb, int cdb_len,
			   struct scsi_inquiry_data *inq_data,
			   struct scsi_sense_desc_header *header);
void scsi_sense_progress_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			      u_int sense_len, uint8_t *cdb, int cdb_len,
			      struct scsi_inquiry_data *inq_data,
			      struct scsi_sense_desc_header *header);
void scsi_sense_generic_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			     u_int sense_len, uint8_t *cdb, int cdb_len,
			     struct scsi_inquiry_data *inq_data,
			     struct scsi_sense_desc_header *header);
void scsi_sense_desc_sbuf(struct sbuf *sb, struct scsi_sense_data *sense,
			  u_int sense_len, uint8_t *cdb, int cdb_len,
			  struct scsi_inquiry_data *inq_data,
			  struct scsi_sense_desc_header *header);
scsi_sense_data_type scsi_sense_type(struct scsi_sense_data *sense_data);

void scsi_sense_only_sbuf(struct scsi_sense_data *sense, u_int sense_len,
			  struct sbuf *sb, char *path_str,
			  struct scsi_inquiry_data *inq_data, uint8_t *cdb,
			  int cdb_len);

#ifdef _KERNEL
int		scsi_command_string(struct ccb_scsiio *csio, struct sbuf *sb);
int		scsi_sense_sbuf(struct ccb_scsiio *csio, struct sbuf *sb,
				scsi_sense_string_flags flags);
char *		scsi_sense_string(struct ccb_scsiio *csio,
				  char *str, int str_len);
void		scsi_sense_print(struct ccb_scsiio *csio);
int 		scsi_vpd_supported_page(struct cam_periph *periph,
					uint8_t page_id);
#else /* _KERNEL */
int		scsi_command_string(struct cam_device *device,
				    struct ccb_scsiio *csio, struct sbuf *sb);
int		scsi_sense_sbuf(struct cam_device *device, 
				struct ccb_scsiio *csio, struct sbuf *sb,
				scsi_sense_string_flags flags);
char *		scsi_sense_string(struct cam_device *device, 
				  struct ccb_scsiio *csio,
				  char *str, int str_len);
void		scsi_sense_print(struct cam_device *device, 
				 struct ccb_scsiio *csio, FILE *ofile);
#endif /* _KERNEL */

const char *	scsi_op_desc(u_int16_t opcode, 
			     struct scsi_inquiry_data *inq_data);
char *		scsi_cdb_string(u_int8_t *cdb_ptr, char *cdb_string,
				size_t len);

void		scsi_print_inquiry(struct scsi_inquiry_data *inq_data);
void		scsi_print_inquiry_short(struct scsi_inquiry_data *inq_data);

u_int		scsi_calc_syncsrate(u_int period_factor);
u_int		scsi_calc_syncparam(u_int period);

typedef int	(*scsi_devid_checkfn_t)(uint8_t *);
int		scsi_devid_is_naa_ieee_reg(uint8_t *bufp);
int		scsi_devid_is_sas_target(uint8_t *bufp);
int		scsi_devid_is_lun_eui64(uint8_t *bufp);
int		scsi_devid_is_lun_naa(uint8_t *bufp);
int		scsi_devid_is_lun_name(uint8_t *bufp);
int		scsi_devid_is_lun_t10(uint8_t *bufp);
struct scsi_vpd_id_descriptor *
		scsi_get_devid(struct scsi_vpd_device_id *id, uint32_t len,
			       scsi_devid_checkfn_t ck_fn);

void		scsi_test_unit_ready(struct ccb_scsiio *csio, u_int32_t retries,
				     void (*cbfcnp)(struct cam_periph *, 
						    union ccb *),
				     u_int8_t tag_action, 
				     u_int8_t sense_len, u_int32_t timeout);

void		scsi_request_sense(struct ccb_scsiio *csio, u_int32_t retries,
				   void (*cbfcnp)(struct cam_periph *, 
						  union ccb *),
				   void *data_ptr, u_int8_t dxfer_len,
				   u_int8_t tag_action, u_int8_t sense_len,
				   u_int32_t timeout);

void		scsi_inquiry(struct ccb_scsiio *csio, u_int32_t retries,
			     void (*cbfcnp)(struct cam_periph *, union ccb *),
			     u_int8_t tag_action, u_int8_t *inq_buf, 
			     u_int32_t inq_len, int evpd, u_int8_t page_code,
			     u_int8_t sense_len, u_int32_t timeout);

void		scsi_mode_sense(struct ccb_scsiio *csio, u_int32_t retries,
				void (*cbfcnp)(struct cam_periph *,
					       union ccb *),
				u_int8_t tag_action, int dbd,
				u_int8_t page_code, u_int8_t page,
				u_int8_t *param_buf, u_int32_t param_len,
				u_int8_t sense_len, u_int32_t timeout);

void		scsi_mode_sense_len(struct ccb_scsiio *csio, u_int32_t retries,
				    void (*cbfcnp)(struct cam_periph *,
						   union ccb *),
				    u_int8_t tag_action, int dbd,
				    u_int8_t page_code, u_int8_t page,
				    u_int8_t *param_buf, u_int32_t param_len,
				    int minimum_cmd_size, u_int8_t sense_len,
				    u_int32_t timeout);

void		scsi_mode_select(struct ccb_scsiio *csio, u_int32_t retries,
				 void (*cbfcnp)(struct cam_periph *,
						union ccb *),
				 u_int8_t tag_action, int scsi_page_fmt,
				 int save_pages, u_int8_t *param_buf,
				 u_int32_t param_len, u_int8_t sense_len,
				 u_int32_t timeout);

void		scsi_mode_select_len(struct ccb_scsiio *csio, u_int32_t retries,
				     void (*cbfcnp)(struct cam_periph *,
						    union ccb *),
				     u_int8_t tag_action, int scsi_page_fmt,
				     int save_pages, u_int8_t *param_buf,
				     u_int32_t param_len, int minimum_cmd_size,
				     u_int8_t sense_len, u_int32_t timeout);

void		scsi_log_sense(struct ccb_scsiio *csio, u_int32_t retries,
			       void (*cbfcnp)(struct cam_periph *, union ccb *),
			       u_int8_t tag_action, u_int8_t page_code,
			       u_int8_t page, int save_pages, int ppc,
			       u_int32_t paramptr, u_int8_t *param_buf,
			       u_int32_t param_len, u_int8_t sense_len,
			       u_int32_t timeout);

void		scsi_log_select(struct ccb_scsiio *csio, u_int32_t retries,
				void (*cbfcnp)(struct cam_periph *,
				union ccb *), u_int8_t tag_action,
				u_int8_t page_code, int save_pages,
				int pc_reset, u_int8_t *param_buf,
				u_int32_t param_len, u_int8_t sense_len,
				u_int32_t timeout);

void		scsi_prevent(struct ccb_scsiio *csio, u_int32_t retries,
			     void (*cbfcnp)(struct cam_periph *, union ccb *),
			     u_int8_t tag_action, u_int8_t action,
			     u_int8_t sense_len, u_int32_t timeout);

void		scsi_read_capacity(struct ccb_scsiio *csio, u_int32_t retries,
				   void (*cbfcnp)(struct cam_periph *, 
				   union ccb *), u_int8_t tag_action, 
				   struct scsi_read_capacity_data *,
				   u_int8_t sense_len, u_int32_t timeout);
void		scsi_read_capacity_16(struct ccb_scsiio *csio, uint32_t retries,
				      void (*cbfcnp)(struct cam_periph *,
				      union ccb *), uint8_t tag_action,
				      uint64_t lba, int reladr, int pmi,
				      uint8_t *rcap_buf, int rcap_buf_len,
				      uint8_t sense_len, uint32_t timeout);

void		scsi_report_luns(struct ccb_scsiio *csio, u_int32_t retries,
				 void (*cbfcnp)(struct cam_periph *, 
				 union ccb *), u_int8_t tag_action, 
				 u_int8_t select_report,
				 struct scsi_report_luns_data *rpl_buf,
				 u_int32_t alloc_len, u_int8_t sense_len,
				 u_int32_t timeout);

void		scsi_report_target_group(struct ccb_scsiio *csio, u_int32_t retries,
				 void (*cbfcnp)(struct cam_periph *, 
				 union ccb *), u_int8_t tag_action, 
				 u_int8_t pdf,
				 void *buf,
				 u_int32_t alloc_len, u_int8_t sense_len,
				 u_int32_t timeout);

void		scsi_set_target_group(struct ccb_scsiio *csio, u_int32_t retries,
				 void (*cbfcnp)(struct cam_periph *, 
				 union ccb *), u_int8_t tag_action, void *buf,
				 u_int32_t alloc_len, u_int8_t sense_len,
				 u_int32_t timeout);

void		scsi_synchronize_cache(struct ccb_scsiio *csio, 
				       u_int32_t retries,
				       void (*cbfcnp)(struct cam_periph *, 
				       union ccb *), u_int8_t tag_action, 
				       u_int32_t begin_lba, u_int16_t lb_count,
				       u_int8_t sense_len, u_int32_t timeout);

void scsi_receive_diagnostic_results(struct ccb_scsiio *csio, u_int32_t retries,
				     void (*cbfcnp)(struct cam_periph *,
						    union ccb*),
				     uint8_t tag_action, int pcv,
				     uint8_t page_code, uint8_t *data_ptr,
				     uint16_t allocation_length,
				     uint8_t sense_len, uint32_t timeout);

void scsi_send_diagnostic(struct ccb_scsiio *csio, u_int32_t retries,
			  void (*cbfcnp)(struct cam_periph *, union ccb *),
			  uint8_t tag_action, int unit_offline,
			  int device_offline, int self_test, int page_format,
			  int self_test_code, uint8_t *data_ptr,
			  uint16_t param_list_length, uint8_t sense_len,
			  uint32_t timeout);

void scsi_read_buffer(struct ccb_scsiio *csio, u_int32_t retries,
			void (*cbfcnp)(struct cam_periph *, union ccb*),
			uint8_t tag_action, int mode,
			uint8_t buffer_id, u_int32_t offset,
			uint8_t *data_ptr, uint32_t allocation_length,
			uint8_t sense_len, uint32_t timeout);

void scsi_write_buffer(struct ccb_scsiio *csio, u_int32_t retries,
			void (*cbfcnp)(struct cam_periph *, union ccb *),
			uint8_t tag_action, int mode,
			uint8_t buffer_id, u_int32_t offset,
			uint8_t *data_ptr, uint32_t param_list_length,
			uint8_t sense_len, uint32_t timeout);

#define	SCSI_RW_READ	0x0001
#define	SCSI_RW_WRITE	0x0002
#define	SCSI_RW_DIRMASK	0x0003
#define	SCSI_RW_BIO	0x1000
void scsi_read_write(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, int readop, u_int8_t byte2, 
		     int minimum_cmd_size, u_int64_t lba,
		     u_int32_t block_count, u_int8_t *data_ptr,
		     u_int32_t dxfer_len, u_int8_t sense_len,
		     u_int32_t timeout);

void scsi_write_same(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, u_int8_t byte2, 
		     int minimum_cmd_size, u_int64_t lba,
		     u_int32_t block_count, u_int8_t *data_ptr,
		     u_int32_t dxfer_len, u_int8_t sense_len,
		     u_int32_t timeout);

void scsi_ata_identify(struct ccb_scsiio *csio, u_int32_t retries,
		       void (*cbfcnp)(struct cam_periph *, union ccb *),
		       u_int8_t tag_action, u_int8_t *data_ptr,
		       u_int16_t dxfer_len, u_int8_t sense_len,
		       u_int32_t timeout);

void scsi_ata_trim(struct ccb_scsiio *csio, u_int32_t retries,
	           void (*cbfcnp)(struct cam_periph *, union ccb *),
	           u_int8_t tag_action, u_int16_t block_count,
	           u_int8_t *data_ptr, u_int16_t dxfer_len,
	           u_int8_t sense_len, u_int32_t timeout);

void scsi_ata_pass_16(struct ccb_scsiio *csio, u_int32_t retries,
		      void (*cbfcnp)(struct cam_periph *, union ccb *),
		      u_int32_t flags, u_int8_t tag_action,
		      u_int8_t protocol, u_int8_t ata_flags, u_int16_t features,
		      u_int16_t sector_count, uint64_t lba, u_int8_t command,
		      u_int8_t control, u_int8_t *data_ptr, u_int16_t dxfer_len,
		      u_int8_t sense_len, u_int32_t timeout);

void scsi_unmap(struct ccb_scsiio *csio, u_int32_t retries,
		void (*cbfcnp)(struct cam_periph *, union ccb *),
		u_int8_t tag_action, u_int8_t byte2,
		u_int8_t *data_ptr, u_int16_t dxfer_len,
		u_int8_t sense_len, u_int32_t timeout);

void scsi_start_stop(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, int start, int load_eject,
		     int immediate, u_int8_t sense_len, u_int32_t timeout);

int		scsi_inquiry_match(caddr_t inqbuffer, caddr_t table_entry);
int		scsi_static_inquiry_match(caddr_t inqbuffer,
					  caddr_t table_entry);
int		scsi_devid_match(uint8_t *rhs, size_t rhs_len,
				 uint8_t *lhs, size_t lhs_len);

void scsi_extract_sense(struct scsi_sense_data *sense, int *error_code,
			int *sense_key, int *asc, int *ascq);
int scsi_extract_sense_ccb(union ccb *ccb, int *error_code, int *sense_key,
			   int *asc, int *ascq);
void scsi_extract_sense_len(struct scsi_sense_data *sense,
			    u_int sense_len, int *error_code, int *sense_key,
			    int *asc, int *ascq, int show_errors);
int scsi_get_sense_key(struct scsi_sense_data *sense, u_int sense_len,
		       int show_errors);
int scsi_get_asc(struct scsi_sense_data *sense, u_int sense_len,
		 int show_errors);
int scsi_get_ascq(struct scsi_sense_data *sense, u_int sense_len,
		  int show_errors);
static __inline void scsi_ulto2b(u_int32_t val, u_int8_t *bytes);
static __inline void scsi_ulto3b(u_int32_t val, u_int8_t *bytes);
static __inline void scsi_ulto4b(u_int32_t val, u_int8_t *bytes);
static __inline void scsi_u64to8b(u_int64_t val, u_int8_t *bytes);
static __inline uint32_t scsi_2btoul(const uint8_t *bytes);
static __inline uint32_t scsi_3btoul(const uint8_t *bytes);
static __inline int32_t scsi_3btol(const uint8_t *bytes);
static __inline uint32_t scsi_4btoul(const uint8_t *bytes);
static __inline uint64_t scsi_8btou64(const uint8_t *bytes);
static __inline void *find_mode_page_6(struct scsi_mode_header_6 *mode_header);
static __inline void *find_mode_page_10(struct scsi_mode_header_10 *mode_header);

static __inline void
scsi_ulto2b(u_int32_t val, u_int8_t *bytes)
{

	bytes[0] = (val >> 8) & 0xff;
	bytes[1] = val & 0xff;
}

static __inline void
scsi_ulto3b(u_int32_t val, u_int8_t *bytes)
{

	bytes[0] = (val >> 16) & 0xff;
	bytes[1] = (val >> 8) & 0xff;
	bytes[2] = val & 0xff;
}

static __inline void
scsi_ulto4b(u_int32_t val, u_int8_t *bytes)
{

	bytes[0] = (val >> 24) & 0xff;
	bytes[1] = (val >> 16) & 0xff;
	bytes[2] = (val >> 8) & 0xff;
	bytes[3] = val & 0xff;
}

static __inline void
scsi_u64to8b(u_int64_t val, u_int8_t *bytes)
{

	bytes[0] = (val >> 56) & 0xff;
	bytes[1] = (val >> 48) & 0xff;
	bytes[2] = (val >> 40) & 0xff;
	bytes[3] = (val >> 32) & 0xff;
	bytes[4] = (val >> 24) & 0xff;
	bytes[5] = (val >> 16) & 0xff;
	bytes[6] = (val >> 8) & 0xff;
	bytes[7] = val & 0xff;
}

static __inline uint32_t
scsi_2btoul(const uint8_t *bytes)
{
	uint32_t rv;

	rv = (bytes[0] << 8) |
	     bytes[1];
	return (rv);
}

static __inline uint32_t
scsi_3btoul(const uint8_t *bytes)
{
	uint32_t rv;

	rv = (bytes[0] << 16) |
	     (bytes[1] << 8) |
	     bytes[2];
	return (rv);
}

static __inline int32_t 
scsi_3btol(const uint8_t *bytes)
{
	uint32_t rc = scsi_3btoul(bytes);
 
	if (rc & 0x00800000)
		rc |= 0xff000000;

	return (int32_t) rc;
}

static __inline uint32_t
scsi_4btoul(const uint8_t *bytes)
{
	uint32_t rv;

	rv = (bytes[0] << 24) |
	     (bytes[1] << 16) |
	     (bytes[2] << 8) |
	     bytes[3];
	return (rv);
}

static __inline uint64_t
scsi_8btou64(const uint8_t *bytes)
{
        uint64_t rv;
 
	rv = (((uint64_t)bytes[0]) << 56) |
	     (((uint64_t)bytes[1]) << 48) |
	     (((uint64_t)bytes[2]) << 40) |
	     (((uint64_t)bytes[3]) << 32) |
	     (((uint64_t)bytes[4]) << 24) |
	     (((uint64_t)bytes[5]) << 16) |
	     (((uint64_t)bytes[6]) << 8) |
	     bytes[7];
	return (rv);
}

/*
 * Given the pointer to a returned mode sense buffer, return a pointer to
 * the start of the first mode page.
 */
static __inline void *
find_mode_page_6(struct scsi_mode_header_6 *mode_header)
{
	void *page_start;

	page_start = (void *)((u_int8_t *)&mode_header[1] +
			      mode_header->blk_desc_len);

	return(page_start);
}

static __inline void *
find_mode_page_10(struct scsi_mode_header_10 *mode_header)
{
	void *page_start;

	page_start = (void *)((u_int8_t *)&mode_header[1] +
			       scsi_2btoul(mode_header->blk_desc_len));

	return(page_start);
}

__END_DECLS

#endif /*_SCSI_SCSI_ALL_H*/
