/*
 * HISTORY
 * $Log: scsi_all.h,v $
 * Revision 1.2  1992/11/20  23:07:13  julian
 * add a definition for device type T_NODEVICE
 *
 * Revision 1.1  1992/09/26  22:14:02  julian
 * Initial revision
 *
 *
 * PATCHES MAGIC                LEVEL   PATCH THAT GOT US HERE
 * --------------------         -----   ----------------------
 * CURRENT PATCH LEVEL:         1       00098
 * --------------------         -----   ----------------------
 *
 * 16 Feb 93	Julian Elischer		ADDED for SCSI system
 * 
 */

/*
 * SCSI general  interface description
 */


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
 */

/*
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

/*
 * SCSI command format
 */


struct scsi_generic
{
	u_char	opcode;
	u_char	bytes[11];
};

struct scsi_test_unit_ready
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	unused[3];
	u_char	link:1;
	u_char	flag:4;
	u_char	:3;
};

struct scsi_send_diag
{
	u_char	op_code;
	u_char	uol:1;
	u_char	dol:1;
	u_char	selftest:1;
	u_char	:1;
	u_char	pf:1;
	u_char	lun:3;
	u_char	unused[1];
	u_char	paramlen[2];
	u_char	link:1;
	u_char	flag:4;
	u_char	:3;
};

struct scsi_sense
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;	
	u_char	unused[2];
	u_char	length;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_inquiry
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;	
	u_char	unused[2];
	u_char	length;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_mode_sense
{
	u_char	op_code;
	u_char	:3;
	u_char	dbd:1;
	u_char	rsvd:1;
	u_char	lun:3;	
	u_char	page_code:6;
	u_char	page_ctrl:2;
	u_char	unused;
	u_char	length;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_mode_sense_big
{
	u_char	op_code;
	u_char	:3;
	u_char	dbd:1;
	u_char	rsvd:1;
	u_char	lun:3;	
	u_char	page_code:6;
	u_char	page_ctrl:2;
	u_char	unused[4];
	u_char	length[2];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_mode_select
{
	u_char	op_code;
	u_char	sp:1;
	u_char	:3;
	u_char	pf:1;
	u_char	lun:3;	
	u_char	unused[2];
	u_char	length;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_mode_select_big
{
	u_char	op_code;
	u_char	sp:1;
	u_char	:3;
	u_char	pf:1;
	u_char	lun:3;	
	u_char	unused[5];
	u_char	length[2];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_reserve
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;	
	u_char	unused[2];
	u_char	length;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_release
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;	
	u_char	unused[2];
	u_char	length;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_prevent
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	unused[2];
	u_char	prevent:1;
	u_char	:7;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};
#define	PR_PREVENT 1
#define PR_ALLOW   0

/*
 * Opcodes
 */

#define	TEST_UNIT_READY		0x00
#define REQUEST_SENSE		0x03
#define INQUIRY			0x12
#define MODE_SELECT		0x15
#define MODE_SENSE		0x1a
#define START_STOP		0x1b
#define RESERVE      		0x16
#define RELEASE      		0x17
#define PREVENT_ALLOW		0x1e
#define POSITION_TO_ELEMENT	0x2b
#define	MODE_SENSE_BIG		0x54
#define	MODE_SELECT_BIG		0x55
#define MOVE_MEDIUM     	0xa5
#define READ_ELEMENT_STATUS	0xb8


/*
 * sense data format
 */
#define T_DIRECT	0
#define T_SEQUENTIAL	1
#define T_PRINTER	2
#define T_PROCESSOR	3
#define T_WORM		4
#define T_READONLY	5
#define T_SCANNER 	6
#define T_OPTICAL 	7
#define T_NODEVICE	0x1F

#define T_CHANGER	8
#define T_COMM		9

#define T_REMOV		1
#define	T_FIXED		0

struct scsi_inquiry_data
{
	u_char	device_type:5;
	u_char	device_qualifier:3;
	u_char	dev_qual2:7;
	u_char	removable:1;
	u_char	ansii_version:3;
	u_char	:5;
	u_char	response_format;
	u_char	additional_length;
	u_char	unused[2];
	u_char	:3;
	u_char	can_link:1;
	u_char	can_sync:1;
	u_char	:3;
	char	vendor[8];
	char	product[16];
	char	revision[4];
	u_char	extra[8];
};


struct	scsi_sense_data
{
	u_char	error_code:4;
	u_char	error_class:3;
	u_char	valid:1;
	union
	{
		struct
		{
			u_char	blockhi:5;
			u_char	vendor:3;
			u_char	blockmed;
			u_char	blocklow;
		} unextended;
		struct
		{
			u_char	segment;
			u_char	sense_key:4;
			u_char	:1;
			u_char	ili:1;
			u_char	eom:1;
			u_char	filemark:1;
			u_char	info[4];
			u_char	extra_len;
			/* allocate enough room to hold new stuff
			u_char	cmd_spec_info[4];
			u_char	add_sense_code;
			u_char	add_sense_code_qual;
			u_char	fru;
			u_char	sense_key_spec_1:7;
			u_char	sksv:1;
			u_char	sense_key_spec_2;
			u_char	sense_key_spec_3;
			( by increasing 16 to 26 below) */
			u_char	extra_bytes[26];
		} extended;
	}ext;
};
struct	scsi_sense_data_new
{
	u_char	error_code:7;
	u_char	valid:1;
	union
	{
		struct
		{
			u_char	blockhi:5;
			u_char	vendor:3;
			u_char	blockmed;
			u_char	blocklow;
		} unextended;
		struct
		{
			u_char	segment;
			u_char	sense_key:4;
			u_char	:1;
			u_char	ili:1;
			u_char	eom:1;
			u_char	filemark:1;
			u_char	info[4];
			u_char	extra_len;
			u_char	cmd_spec_info[4];
			u_char	add_sense_code;
			u_char	add_sense_code_qual;
			u_char	fru;
			u_char	sense_key_spec_1:7;
			u_char	sksv:1;
			u_char	sense_key_spec_2;
			u_char	sense_key_spec_3;
			u_char	extra_bytes[16];
		} extended;
	}ext;
};

struct	blk_desc
{
	u_char	density;
	u_char	nblocks[3];
	u_char	reserved;
	u_char	blklen[3];
};

struct scsi_mode_header
{
	u_char	data_length;	/* Sense data length */
	u_char	medium_type;
	u_char	dev_spec;
	u_char	blk_desc_len;
};

struct scsi_mode_header_big
{
	u_char	data_length[2];	/* Sense data length */
	u_char	medium_type;
	u_char	dev_spec;
	u_char	unused[2];
	u_char	blk_desc_len[2];
};


/*
 * Status Byte
 */
#define	SCSI_OK		0x00
#define	SCSI_CHECK		0x02
#define	SCSI_BUSY		0x08	
#define SCSI_INTERM		0x10
