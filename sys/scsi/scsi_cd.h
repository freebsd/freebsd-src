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
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 *
 *	$Id: scsi_cd.h,v 1.4 1993/08/21 20:01:52 rgrimes Exp $
 */

/*
 *	Define two bits always in the same place in byte 2 (flag byte)
 */
#define	CD_RELADDR	0x01
#define	CD_MSF		0x02

/*
 * SCSI command format
 */

struct scsi_read_capacity_cd
{
	u_char	op_code;
	u_char	byte2;
	u_char	addr_3;	/* Most Significant */
	u_char	addr_2;
	u_char	addr_1;
	u_char	addr_0;	/* Least Significant */
	u_char	unused[3];
	u_char	control;
};

struct scsi_pause
{
	u_char	op_code;
	u_char	byte2;
	u_char	unused[6];
	u_char	resume;
	u_char	control;
};
#define	PA_PAUSE	1
#define PA_RESUME	0

struct scsi_play_msf
{
	u_char	op_code;
	u_char	byte2;
	u_char	unused;
	u_char	start_m;
	u_char	start_s;
	u_char	start_f;
	u_char	end_m;
	u_char	end_s;
	u_char	end_f;
	u_char	control;
};

struct scsi_play_track
{
	u_char	op_code;
	u_char	byte2;
	u_char	unused[2];
	u_char	start_track;
	u_char	start_index;
	u_char	unused1;
	u_char	end_track;
	u_char	end_index;
	u_char	control;
};

struct scsi_play
{
	u_char	op_code;
	u_char	byte2;
	u_char	blk_addr[4];
	u_char	unused;
	u_char	xfer_len[2];
	u_char	control;
};

struct scsi_play_big	
{
	u_char	op_code;
	u_char	byte2;	/* same as above */
	u_char	blk_addr[4];
	u_char	xfer_len[4];
	u_char	unused;
	u_char	control;
};

struct scsi_play_rel_big
{
	u_char	op_code;
	u_char	byte2;	/* same as above */
	u_char	blk_addr[4];
	u_char	xfer_len[4];
	u_char	track;
	u_char	control;
};

struct scsi_read_header
{
	u_char	op_code;
	u_char	byte2;
	u_char	blk_addr[4];
	u_char	unused;
	u_char	data_len[2];
	u_char	control;
};

struct scsi_read_subchannel
{
	u_char	op_code;
	u_char	byte2;
	u_char	byte3;
#define	SRS_SUBQ	0x40
	u_char	subchan_format;
	u_char	unused[2];
	u_char	track;
	u_char	data_len[2];
	u_char	control;
};

struct scsi_read_toc
{
	u_char	op_code;
	u_char	byte2;
	u_char	unused[4];
	u_char	from_track;
	u_char	data_len[2];
	u_char	control;
};
;

struct scsi_read_cd_capacity
{
	u_char	op_code;
	u_char	byte2;
	u_char	addr_3;	/* Most Significant */
	u_char	addr_2;
	u_char	addr_1;
	u_char	addr_0;	/* Least Significant */
	u_char	unused[3];
	u_char	control;
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
	struct	audio_page
	{
		u_char	page_code;
#define	CD_PAGE_CODE	0x3F
#define	AUDIO_PAGE	0x0e
#define	CD_PAGE_PS	0x80
		u_char	param_len;
		u_char	flags;
#define		CD_PA_SOTC	0x02
#define		CD_PA_IMMED	0x04
		u_char	unused[2];
		u_char	format_lba;
#define		CD_PA_FORMAT_LBA	0x0F
#define		CD_PA_APR_VALID	0x80
		u_char	lb_per_sec[2];
		struct	port_control
		{
			u_char	channels;
#define	CHANNEL 0x0F
#define	CHANNEL_0 1
#define	CHANNEL_1 2
#define	CHANNEL_2 4
#define	CHANNEL_3 8
#define	LEFT_CHANNEL	CHANNEL_0
#define	RIGHT_CHANNEL	CHANNEL_1
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

