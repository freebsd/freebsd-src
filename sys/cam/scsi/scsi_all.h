/*
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
 * $FreeBSD: src/sys/cam/scsi/scsi_all.h,v 1.14.2.3 2000/08/14 05:42:33 kbyanc Exp $
 */

/*
 * SCSI general  interface description
 */

#ifndef	_SCSI_SCSI_ALL_H
#define _SCSI_SCSI_ALL_H 1

#include <sys/cdefs.h>

#ifdef _KERNEL
#include "opt_scsi.h"
/*
 * This is the number of seconds we wait for devices to settle after a SCSI
 * bus reset.
 */
#ifndef SCSI_DELAY
#define SCSI_DELAY 2000
#endif
/*
 * If someone sets this to 0, we assume that they want the minimum
 * allowable bus settle delay.  All devices need _some_ sort of bus settle
 * delay, so we'll set it to a minimum value of 100ms.
 */
#if (SCSI_DELAY == 0)
#undef SCSI_DELAY
#define SCSI_DELAY 100
#endif

/*
 * Make sure the user isn't using seconds instead of milliseconds.
 */
#if (SCSI_DELAY < 100)
#error "SCSI_DELAY is in milliseconds, not seconds!  Please use a larger value"
#endif
#endif /* _KERNEL */

/*
 * SCSI command format
 */

/*
 * Define dome bits that are in ALL (or a lot of) scsi commands
 */
#define SCSI_CTL_LINK		0x01
#define SCSI_CTL_FLAG		0x02
#define SCSI_CTL_VENDOR		0xC0
#define	SCSI_CMD_LUN		0xA0	/* these two should not be needed */
#define	SCSI_CMD_LUN_SHIFT	5	/* LUN in the cmd is no longer SCSI */

#define SCSI_MAX_CDBLEN		16	/* 
					 * 16 byte commands are in the 
					 * SCSI-3 spec 
					 */
#if defined(CAM_MAX_CDBLEN) && (CAM_MAX_CDBLEN < SCSI_MAX_CDBLEN)
#error "CAM_MAX_CDBLEN cannot be less than SCSI_MAX_CDBLEN"
#endif

/* 6byte CDBs special case 0 length to be 256 */
#define SCSI_CDB6_LEN(len)	((len) == 0 ? 256 : len)

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
	SS_MANUAL   = 0x050000,	/* 
				 * This error must be handled manually,
				 * i.e. the code must look at the asc and 
				 * ascq values and determine the proper
				 * course of action.
				 */
	SS_TURSTART = 0x060000, /*
				 * Send a Test Unit Ready command to the
				 * device, and if that fails, send a start 
				 * unit.
				 */
	SS_MASK     = 0xff0000
} scsi_sense_action;

typedef enum {
	SSQ_NONE		= 0x0000,
	SSQ_DECREMENT_COUNT	= 0x0100,  /* Decrement the retry count */
	SSQ_MANY		= 0x0200,  /* send lots of recovery commands */
	SSQ_RANGE		= 0x0400,  /*
					    * Yes, this is a hack.  Basically,
					    * if this flag is set then it
					    * represents an ascq range.  The
					    * "correct" way to implement the
					    * ranges might be to add a special
					    * field to the sense code table,
					    * but that would take up a lot of
					    * additional space.  This solution
					    * isn't as elegant, but is more 
					    * space efficient.
					    */
	SSQ_PRINT_SENSE		= 0x0800,
	SSQ_MASK		= 0xff00
} scsi_sense_action_qualifier;

/* Mask for error status values */
#define SS_ERRMASK	0xff

/* The default error action */
#define SS_DEF		SS_RETRY|SSQ_DECREMENT_COUNT|SSQ_PRINT_SENSE|EIO

/* Default error action, without an error return value */
#define SS_NEDEF	SS_RETRY|SSQ_DECREMENT_COUNT|SSQ_PRINT_SENSE

/* Default error action, without sense printing or an error return value */
#define SS_NEPDEF	SS_RETRY|SSQ_DECREMENT_COUNT

struct scsi_generic
{
	u_int8_t opcode;
	u_int8_t bytes[11];
};

struct scsi_request_sense
{
	u_int8_t opcode;
	u_int8_t byte2;
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

struct scsi_send_diag
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SSD_UOL		0x01
#define	SSD_DOL		0x02
#define	SSD_SELFTEST	0x04
#define	SSD_PF		0x10
	u_int8_t unused[1];
	u_int8_t paramlen[2];
	u_int8_t control;
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
#define	SI_EVPD 0x01
	u_int8_t page_code;
	u_int8_t reserved;
	u_int8_t length;
	u_int8_t control;
};

struct scsi_mode_sense_6
{
	u_int8_t opcode;
	u_int8_t byte2;
#define	SMS_DBD				0x08
	u_int8_t page;
#define	SMS_PAGE_CODE 			0x3F
#define SMS_VENDOR_SPECIFIC_PAGE	0x00
#define SMS_DISCONNECT_RECONNECT_PAGE	0x02
#define SMS_PERIPHERAL_DEVICE_PAGE	0x09
#define SMS_CONTROL_MODE_PAGE		0x0A
#define SMS_ALL_PAGES_PAGE		0x3F
#define	SMS_PAGE_CTRL_MASK		0xC0
#define	SMS_PAGE_CTRL_CURRENT 		0x00
#define	SMS_PAGE_CTRL_CHANGEABLE 	0x40
#define	SMS_PAGE_CTRL_DEFAULT 		0x80
#define	SMS_PAGE_CTRL_SAVED 		0xC0
	u_int8_t unused;
	u_int8_t length;
	u_int8_t control;
};

struct scsi_mode_sense_10
{
	u_int8_t opcode;
	u_int8_t byte2;		/* same bits as small version */
	u_int8_t page; 		/* same bits as small version */
	u_int8_t unused[4];
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

struct scsi_control_page {
	u_int8_t page_code;
	u_int8_t page_length;
	u_int8_t rlec;
#define SCB_RLEC			0x01	/*Report Log Exception Cond*/
	u_int8_t queue_flags;
#define SCP_QUEUE_ALG_MASK		0xF0
#define SCP_QUEUE_ALG_RESTRICTED	0x00
#define SCP_QUEUE_ALG_UNRESTRICTED	0x10
#define SCP_QUEUE_ERR			0x02	/*Queued I/O aborted for CACs*/
#define SCP_QUEUE_DQUE			0x01	/*Queued I/O disabled*/
	u_int8_t eca_and_aen;
#define SCP_EECA			0x80	/*Enable Extended CA*/
#define SCP_RAENP			0x04	/*Ready AEN Permission*/
#define SCP_UAAENP			0x02	/*UA AEN Permission*/
#define SCP_EAENP			0x01	/*Error AEN Permission*/
	u_int8_t reserved;
	u_int8_t aen_holdoff_period[2];
};

struct scsi_reserve
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
};

struct scsi_release
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t unused[2];
	u_int8_t length;
	u_int8_t control;
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
#define PR_ALLOW   0x00

struct scsi_sync_cache
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t begin_lba[4];
	u_int8_t reserved;
	u_int8_t lb_count[2];
	u_int8_t control;	
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
#define SRW10_FUA	0x08
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
#define SRW12_FUA	0x08
#define	SRW12_DPO	0x10
	u_int8_t byte2;
	u_int8_t addr[4];
	u_int8_t reserved;
	u_int8_t length[4];
	u_int8_t control;
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
	u_int8_t control;
};

#define SC_SCSI_1 0x01
#define SC_SCSI_2 0x03

/*
 * Opcodes
 */

#define	TEST_UNIT_READY		0x00
#define REQUEST_SENSE		0x03
#define	READ_6			0x08
#define WRITE_6			0x0a
#define INQUIRY			0x12
#define MODE_SELECT_6		0x15
#define MODE_SENSE_6		0x1a
#define START_STOP_UNIT		0x1b
#define START_STOP		0x1b
#define RESERVE      		0x16
#define RELEASE      		0x17
#define	RECEIVE_DIAGNOSTIC	0x1c
#define	SEND_DIAGNOSTIC		0x1d
#define PREVENT_ALLOW		0x1e
#define	READ_CAPACITY		0x25
#define	READ_10			0x28
#define WRITE_10		0x2a
#define POSITION_TO_ELEMENT	0x2b
#define	SYNCHRONIZE_CACHE	0x35
#define	WRITE_BUFFER            0x3b
#define	READ_BUFFER             0x3c
#define	CHANGE_DEFINITION	0x40
#define	MODE_SELECT_10		0x55
#define	MODE_SENSE_10		0x5A
#define MOVE_MEDIUM     	0xa5
#define READ_12			0xa8
#define WRITE_12		0xaa
#define READ_ELEMENT_STATUS	0xb8


/*
 * Device Types
 */
#define T_DIRECT	0x00
#define T_SEQUENTIAL	0x01
#define T_PRINTER	0x02
#define T_PROCESSOR	0x03
#define T_WORM		0x04
#define T_CDROM		0x05
#define T_SCANNER 	0x06
#define T_OPTICAL 	0x07
#define T_CHANGER	0x08
#define T_COMM		0x09
#define T_ASC0		0x0a
#define T_ASC1		0x0b
#define	T_STORARRAY	0x0c
#define	T_ENCLOSURE	0x0d
#define	T_RBC		0x0e
#define	T_OCRW		0x0f
#define T_NODEVICE	0x1F
#define	T_ANY		0xFF	/* Used in Quirk table matches */

#define T_REMOV		1
#define	T_FIXED		0

/*
 * This length is the initial inquiry length used by the probe code, as    
 * well as the legnth necessary for scsi_print_inquiry() to function 
 * correctly.  If either use requires a different length in the future, 
 * the two values should be de-coupled.
 */
#define	SHORT_INQUIRY_LENGTH	36

struct scsi_inquiry_data
{
	u_int8_t device;
#define	SID_TYPE(inq_data) ((inq_data)->device & 0x1f)
#define	SID_QUAL(inq_data) (((inq_data)->device & 0xE0) >> 5)
#define	SID_QUAL_LU_CONNECTED	0x00	/* The specified peripheral device
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
#define	SID_QUAL_LU_OFFLINE	0x01	/* The target is capable of supporting
					 * the specified peripheral device type
					 * on this logical unit; however, the
					 * physical device is not currently
					 * connected to this logical unit.
					 */
#define SID_QUAL_RSVD		0x02
#define	SID_QUAL_BAD_LU		0x03	/* The target is not capable of
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
#define SID_ANSI_REV(inq_data) ((inq_data)->version & 0x07)
#define		SCSI_REV_0		0
#define		SCSI_REV_CCS		1
#define		SCSI_REV_2		2
#define		SCSI_REV_3		3
#define		SCSI_REV_SPC2		4

#define SID_ECMA	0x38
#define SID_ISO		0xC0
	u_int8_t response_format;
#define SID_AENC	0x80
#define SID_TrmIOP	0x40
	u_int8_t additional_length;
	u_int8_t reserved[2];
	u_int8_t flags;
#define	SID_SftRe	0x01
#define	SID_CmdQue	0x02
#define	SID_Linked	0x08
#define	SID_Sync	0x10
#define	SID_WBus16	0x20
#define	SID_WBus32	0x40
#define	SID_RelAdr	0x80
#define SID_VENDOR_SIZE   8
	char	 vendor[SID_VENDOR_SIZE];
#define SID_PRODUCT_SIZE  16
	char	 product[SID_PRODUCT_SIZE];
#define SID_REVISION_SIZE 4
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

struct scsi_vpd_unit_serial_number
{
	u_int8_t device;
	u_int8_t page_code;
#define SVPD_UNIT_SERIAL_NUMBER	0x80
	u_int8_t reserved;
	u_int8_t length; /* serial number length */
#define SVPD_SERIAL_NUM_SIZE 251
	u_int8_t serial_num[SVPD_SERIAL_NUM_SIZE];
};

struct scsi_read_capacity
{
	u_int8_t opcode;
	u_int8_t byte2;
	u_int8_t addr[4];
	u_int8_t unused[3];
	u_int8_t control;
};

struct scsi_read_capacity_data
{
	u_int8_t addr[4];
	u_int8_t length[4];
};

struct scsi_sense_data
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
#define		SSD_KEY_RESERVED	0x0f			
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
#define SSD_FIELDPTR_CMD	0x40
#define SSD_BITPTR_VALID	0x08
#define SSD_BITPTR_VALUE	0x07
#define SSD_MIN_SIZE 18
	u_int8_t extra_bytes[14];
#define SSD_FULL_SIZE sizeof(struct scsi_sense_data)
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
	u_int8_t page_length;
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
#define SCSI_STATUS_INTERMED		0x10
#define SCSI_STATUS_INTERMED_COND_MET	0x14
#define SCSI_STATUS_RESERV_CONFLICT	0x18
#define SCSI_STATUS_CMD_TERMINATED	0x22
#define SCSI_STATUS_QUEUE_FULL		0x28

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
	int				num_ascs;
	struct asc_table_entry		*asc_info;
};

struct asc_table_entry {
	u_int8_t    asc;
	u_int8_t    ascq;
	u_int32_t   action;
#if !defined(SCSI_NO_SENSE_STRINGS)
	const char *desc;
#endif
};

struct op_table_entry {
	u_int8_t    opcode;
	u_int16_t   opmask;
	const char  *desc;
};

struct scsi_op_quirk_entry {
	struct scsi_inquiry_pattern	inq_pat;
	int				num_ops;
	struct op_table_entry		*op_table;
};


struct ccb_scsiio;
struct cam_periph;
union  ccb;
#ifndef _KERNEL
struct cam_device;
#endif

extern const char *scsi_sense_key_text[];

__BEGIN_DECLS
const char * 	scsi_sense_desc(int asc, int ascq,
				struct scsi_inquiry_data *inq_data);
scsi_sense_action scsi_error_action(int asc, int ascq, 
				    struct scsi_inquiry_data *inq_data);
#ifdef _KERNEL
void		scsi_sense_print(struct ccb_scsiio *csio);
int		scsi_interpret_sense(union ccb *ccb, 
				     u_int32_t sense_flags,
				     u_int32_t *relsim_flags, 
				     u_int32_t *reduction,
				     u_int32_t *timeout,
				     scsi_sense_action error_action);
#else
char *		scsi_sense_string(struct cam_device *device, 
				  struct ccb_scsiio *csio,
				  char *str, int str_len);
void		scsi_sense_print(struct cam_device *device, 
				 struct ccb_scsiio *csio, FILE *ofile);
int		scsi_interpret_sense(struct cam_device *device,
				     union ccb *ccb,
				     u_int32_t sense_flags,
				     u_int32_t *relsim_flags, 
				     u_int32_t *reduction,
				     u_int32_t *timeout,
				     scsi_sense_action error_action);
#endif /* _KERNEL */

#define	SF_RETRY_UA	0x01
#define SF_NO_PRINT	0x02
#define SF_QUIET_IR	0x04	/* Be quiet about Illegal Request reponses */
#define SF_PRINT_ALWAYS	0x08
#define SF_RETRY_SELTO	0x10	/* Retry selection timeouts */


const char *	scsi_op_desc(u_int16_t opcode, 
			     struct scsi_inquiry_data *inq_data);
char *		scsi_cdb_string(u_int8_t *cdb_ptr, char *cdb_string,
				size_t len);

void		scsi_print_inquiry(struct scsi_inquiry_data *inq_data);

u_int		scsi_calc_syncsrate(u_int period_factor);
u_int		scsi_calc_syncparam(u_int period);
	
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

void		scsi_mode_select(struct ccb_scsiio *csio, u_int32_t retries,
				 void (*cbfcnp)(struct cam_periph *,
						union ccb *),
				 u_int8_t tag_action, int scsi_page_fmt,
				 int save_pages, u_int8_t *param_buf,
				 u_int32_t param_len, u_int8_t sense_len,
				 u_int32_t timeout);

void		scsi_read_capacity(struct ccb_scsiio *csio, u_int32_t retries,
				   void (*cbfcnp)(struct cam_periph *, 
				   union ccb *), u_int8_t tag_action, 
				   struct scsi_read_capacity_data *rcap_buf,
				   u_int8_t sense_len, u_int32_t timeout);

void		scsi_prevent(struct ccb_scsiio *csio, u_int32_t retries,
			     void (*cbfcnp)(struct cam_periph *, union ccb *),
			     u_int8_t tag_action, u_int8_t action,
			     u_int8_t sense_len, u_int32_t timeout);

void		scsi_synchronize_cache(struct ccb_scsiio *csio, 
				       u_int32_t retries,
				       void (*cbfcnp)(struct cam_periph *, 
				       union ccb *), u_int8_t tag_action, 
				       u_int32_t begin_lba, u_int16_t lb_count,
				       u_int8_t sense_len, u_int32_t timeout);

void scsi_read_write(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, int readop, u_int8_t byte2, 
		     int minimum_cmd_size, u_int32_t lba,
		     u_int32_t block_count, u_int8_t *data_ptr,
		     u_int32_t dxfer_len, u_int8_t sense_len,
		     u_int32_t timeout);

void scsi_start_stop(struct ccb_scsiio *csio, u_int32_t retries,
		     void (*cbfcnp)(struct cam_periph *, union ccb *),
		     u_int8_t tag_action, int start, int load_eject,
		     int immediate, u_int8_t sense_len, u_int32_t timeout);

int		scsi_inquiry_match(caddr_t inqbuffer, caddr_t table_entry);
int		scsi_static_inquiry_match(caddr_t inqbuffer,
					  caddr_t table_entry);

static __inline void scsi_extract_sense(struct scsi_sense_data *sense,
					int *error_code, int *sense_key,
					int *asc, int *ascq);
static __inline void scsi_ulto2b(u_int32_t val, u_int8_t *bytes);
static __inline void scsi_ulto3b(u_int32_t val, u_int8_t *bytes);
static __inline void scsi_ulto4b(u_int32_t val, u_int8_t *bytes);
static __inline u_int32_t scsi_2btoul(u_int8_t *bytes);
static __inline u_int32_t scsi_3btoul(u_int8_t *bytes);
static __inline int32_t scsi_3btol(u_int8_t *bytes);
static __inline u_int32_t scsi_4btoul(u_int8_t *bytes);
static __inline void *find_mode_page_6(struct scsi_mode_header_6 *mode_header);
static __inline void *find_mode_page_10(struct scsi_mode_header_10 *mode_header);

static __inline void scsi_extract_sense(struct scsi_sense_data *sense,
					int *error_code, int *sense_key,
					int *asc, int *ascq)
{
	*error_code = sense->error_code & SSD_ERRCODE;
	*sense_key = sense->flags & SSD_KEY;
	*asc = (sense->extra_len >= 5) ? sense->add_sense_code : 0;
	*ascq = (sense->extra_len >= 6) ? sense->add_sense_code_qual : 0;
}

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

static __inline u_int32_t
scsi_2btoul(u_int8_t *bytes)
{
	u_int32_t rv;

	rv = (bytes[0] << 8) |
	     bytes[1];
	return (rv);
}

static __inline u_int32_t
scsi_3btoul(u_int8_t *bytes)
{
	u_int32_t rv;

	rv = (bytes[0] << 16) |
	     (bytes[1] << 8) |
	     bytes[2];
	return (rv);
}

static __inline int32_t 
scsi_3btol(u_int8_t *bytes)
{
	u_int32_t rc = scsi_3btoul(bytes);
 
	if (rc & 0x00800000)
		rc |= 0xff000000;

	return (int32_t) rc;
}

static __inline u_int32_t
scsi_4btoul(u_int8_t *bytes)
{
	u_int32_t rv;

	rv = (bytes[0] << 24) |
	     (bytes[1] << 16) |
	     (bytes[2] << 8) |
	     bytes[3];
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
