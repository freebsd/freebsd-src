/*
 * Written by Julian Elischer (julian@tfs.com)
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

struct scsi_read_capacity_cd
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	addr_3;	/* Most Significant */
	u_char	addr_2;
	u_char	addr_1;
	u_char	addr_0;	/* Least Significant */
	u_char	unused[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;	
};

struct scsi_pause
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	unused[6];
	u_char	resume:1;
	u_char	:7;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};
#define	PA_PAUSE	1
#define PA_RESUME	0

struct scsi_play_msf
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	unused;
	u_char	start_m;
	u_char	start_s;
	u_char	start_f;
	u_char	end_m;
	u_char	end_s;
	u_char	end_f;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_play_track
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	unused[2];
	u_char	start_track;
	u_char	start_index;
	u_char	unused1;
	u_char	end_track;
	u_char	end_index;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_play
{
	u_char	op_code;
	u_char	reladdr:1;
	u_char	:4;
	u_char	lun:3;
	u_char	blk_addr[4];
	u_char	unused;
	u_char	xfer_len[2];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_play_big	
{
	u_char	op_code;
	u_char	reladdr:1;
	u_char	:4;
	u_char	lun:3;
	u_char	blk_addr[4];
	u_char	xfer_len[4];
	u_char	unused;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_play_rel_big
{
	u_char	op_code;
	u_char	reladdr:1;
	u_char	:4;
	u_char	lun:3;
	u_char	blk_addr[4];
	u_char	xfer_len[4];
	u_char	track;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_read_header
{
	u_char	op_code;
	u_char	:1;
	u_char	msf:1;
	u_char	:3;
	u_char	lun:3;
	u_char	blk_addr[4];
	u_char	unused;
	u_char	data_len[2];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_read_subchannel
{
	u_char	op_code;
	u_char	:1;
	u_char	msf:1;
	u_char	:3;
	u_char	lun:3;
	u_char	:6;
	u_char	subQ:1;
	u_char	:1;
	u_char	subchan_format;
	u_char	unused[2];
	u_char	track;
	u_char	data_len[2];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};

struct scsi_read_toc
{
	u_char	op_code;
	u_char	:1;
	u_char	msf:1;
	u_char	:3;
	u_char	lun:3;
	u_char	unused[4];
	u_char	from_track;
	u_char	data_len[2];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
};
;

struct scsi_read_cd_capacity
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	addr_3;	/* Most Significant */
	u_char	addr_2;
	u_char	addr_1;
	u_char	addr_0;	/* Least Significant */
	u_char	unused[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;	
};

/*
 * Opcodes
 */

#define READ_CD_CAPACITY	0x25	/* slightly different from disk */
#define READ_SUBCHANNEL		0x42	/* cdrom read Subchannel */
#define READ_TOC		0x43	/* cdrom read TOC */
#define READ_HEADER		0x44	/* cdrom read header */
#define PLAY			0x45	/* cdrom play  'play audio' mode */
#define PLAY_MSF		0x47	/* cdrom play Min,Sec,Frames mode */
#define PLAY_TRACK		0x48	/* cdrom play track/index mode */
#define PLAY_TRACK_REL		0x49	/* cdrom play track/index mode */
#define PAUSE			0x4b	/* cdrom pause in 'play audio' mode */
#define PLAY_BIG		0xa5	/* cdrom pause in 'play audio' mode */
#define PLAY_TRACK_REL_BIG	0xa9	/* cdrom play track/index mode */


struct	cd_inquiry_data	/* in case there is some special info */
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

struct scsi_read_cd_cap_data
{
	u_char	addr_3;	/* Most significant */
	u_char	addr_2;
	u_char	addr_1;
	u_char	addr_0;	/* Least significant */
	u_char	length_3;	/* Most significant */
	u_char	length_2;
	u_char	length_1;
	u_char	length_0;	/* Least significant */
};

union	cd_pages
{
#define	AUDIO_PAGE	0x0e
	struct	audio_page
	{
		u_char	page_code:6;
		u_char	:1;
		u_char	ps:1;
		u_char	param_len;
		u_char	:1;
		u_char	sotc:1;
		u_char	immed:1;
		u_char	:5;
		u_char	unused[2];
		u_char	format_lba:4;
		u_char	:3;
		u_char	apr_valid:1;
		u_char	lb_per_sec[2];
		struct	port_control
		{
			u_char	channels:4;
#define	CHANNEL_0 1
#define	CHANNEL_1 2
#define	CHANNEL_2 4
#define	CHANNEL_3 8
#define	LEFT_CHANNEL	CHANNEL_0
#define	RIGHT_CHANNEL	CHANNEL_1
			u_char	:4;
			u_char	volume;
		} port[4];
#define	LEFT_PORT	0
#define	RIGHT_PORT	1
	}audio;
};

struct cd_mode_data
{
	struct scsi_mode_header header;
	struct blk_desc blk_desc;
	union cd_pages page;
};

