/*
 * SCSI tape interface description
 */

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
 *
 *	$FreeBSD$
 */
#ifndef	SCSI_SCSI_TAPE_H
#define SCSI_SCSI_TAPE_H 1



/*
 * SCSI command formats
 */


struct scsi_rw_tape
{
	u_char	op_code;
	u_char	byte2;
#define	SRWT_FIXED	0x01
	u_char	len[3];
	u_char	control;
};

struct scsi_space
{
	u_char	op_code;
	u_char	byte2;
#define	SS_CODE	0x03
	u_char	number[3];
	u_char	control;
};
#define SP_BLKS	0
#define SP_FILEMARKS 1
#define SP_SEQ_FILEMARKS 2
#define	SP_EOM	3

struct scsi_write_filemarks
{
	u_char	op_code;
	u_char	byte2;
	u_char	number[3];
	u_char	control;
};

struct scsi_rewind
{
	u_char	op_code;
	u_char	byte2;
#define	SR_IMMED	0x01
	u_char	unused[3];
	u_char	control;
};

/*
** Tape erase - AKL: Andreas Klemm <andreas@knobel.gun.de>
*/
struct scsi_erase
{
	u_char	op_code;
	u_char	byte2;
#define	SE_LONG		0x01	/*
				** Archive Viper 2525 doesn't allow short
				** erase, other tapes possibly don't allow
				** that, too.
				*/
#define	SE_IMMED	0x02
	u_char	unused[3];
	u_char	control;
};

struct scsi_load
{
	u_char	op_code;
	u_char	byte2;
#define	SL_IMMED	0x01
	u_char	unused[2];
	u_char	how;
	u_char	control;
};
#define LD_UNLOAD 0
#define LD_LOAD 1
#define LD_RETEN 2


struct scsi_blk_limits
{
	u_char	op_code;
	u_char	byte2;
	u_char	unused[3];
	u_char	control;
};

/*
 * Opcodes
 */

#define REWIND			0x01
#define	READ_BLK_LIMITS		0x05
#define	READ_COMMAND_TAPE	0x08
#define WRITE_COMMAND_TAPE	0x0a
#define	WRITE_FILEMARKS		0x10
#define	SPACE			0x11
#define	ERASE			0x19
#define LOAD_UNLOAD		0x1b



struct scsi_blk_limits_data
{
	u_char	reserved;
	u_char	max_length_2;	/* Most significant */
	u_char	max_length_1;
	u_char	max_length_0;	/* Least significant */
	u_char	min_length_1;	/* Most significant */
	u_char	min_length_0;	/* Least significant */
};

/* defines for the device specific byte in the mode select/sense header */
#define	SMH_DSP_SPEED		0x0F
#define	SMH_DSP_BUFF_MODE	0x70
#define	SMH_DSP_BUFF_MODE_OFF	0x00
#define	SMH_DSP_BUFF_MODE_ON	0x10
#define	SMH_DSP_BUFF_MODE_MLTI	0x20
#define	SMH_DSP_WRITE_PROT	0x80

/* A special for the CIPHER ST150S(old drive) */
struct	blk_desc_cipher
{
	u_char	density;
	u_char	nblocks[3];
	u_char	reserved;
	u_char	blklen[3];
	u_char  other;
#define ST150_SEC	0x01	/* soft error count */
#define	SR150_AUI	0x02	/* autoload inhibit */
};

/*
 * This structure defines the various mode pages that tapes know about.
 */
#define PAGE_HEADERLEN 2
struct	tape_pages
{
	u_char pg_code;	/* page code    */
#define ST_PAGE_CONFIGURATION	0x10
#define ST_PAGE_MEDIUM_PART	0x11
#define ST_PAGE_MEDIUM_PART2	0x12
#define ST_PAGE_MEDIUM_PART3	0x13
#define ST_PAGE_MEDIUM_PART4	0x14
#define ST_P_CODE	0x3F	/* page code */
#define ST_P_PS	0x80		/* page savable */
	u_char pg_length;	/* page length  */
	union
	{
		struct
		{
			u_char active_format;	/* active format for density*/
#define ST_P_CAP 0x40	/* change active Partition */
#define ST_P_CAF 0x20	/* change active format */
#define ST_P_AF	 0x1F	/* active format */
			u_char active_partition; /* */
			u_char write_buffer_full_ratio; /* highwater writing*/
			u_char read_buffer_empty_ratio; /* lowwater reading*/
			u_char write_delay_high; /* # 100mSecs before flush*/
			u_char write_delay_low;	/* of buffer to the media */
			u_char flags1;		/* various single bit flags */
#define	ST_P_DBR	0x80 /* supports data-buffer recovery */
#define	ST_P_BIS	0x40 /* supports Block_ID */
#define	ST_P_RSmk	0x20 /* Reports setmarks during reads and spaces */
#define	ST_P_AVC	0x10 /* Supports Automatic Velocity Control */
#define	ST_P_SOCF	0x0C /* Stop On Consecutive Filemarks, */
#define	ST_P_RBO	0x02 /* Recoverd Buffered Data order, 1 = LIFO */
#define	ST_P_REW	0x01 /* Report Early Warning (see SEW) */
			u_char gap_size;	/*I/B gap,  1=min 0=default */
			u_char flags2;		/* various single bit flags */
#define	ST_P_EOD	0xE0 /* What is and EOD....*/
#define	ST_P_EOD_DEF 	0x00 /* Drive's default	*/
#define	ST_P_EOD_FMT	0x20 /* define by format */
#define	ST_P_EOD_SOCF	0x40 /* define by SOCF (above) */
#define	ST_P_EEG	0x10 /* use EOD above */
#define	ST_P_SEW	0x04 /* Synchronise at Early warning.. flush buffers*/
			u_char 	early_warn_high;/* buf size at early warning */
			u_char 	early_warn_med;	/* after early warning, only */
			u_char 	early_warn_low;	/* buufer this much data */
			u_char	data_compress_alg; /* 0 = off, 1 = default */
			u_char	reserved;	/* The standard says so */
		} configuration;
		struct
		{
#define ST_MAXPARTS 16 /*for now*/
			u_char	max_add_parts; /* that drive allows */
			u_char	parts_defined; /* max min(ST_MAXPARTS,max_add_parts) */
			u_char	flags;
#define	ST_P_FDP 0x80
#define	ST_P_SDP 0x40
#define	ST_P_IDP 0x20
#define	ST_P_PSUM 0x18	/* units of part defs.. */
#define	ST_P_PSUM_BYTES		0x0	/* units of part defs.. */
#define	ST_P_PSUM_KBYTES	0x08	/* units of part defs.. */
#define	ST_P_PSUM_MBYTES	0x10	/* units of part defs.. */
			u_char	medium_format_recog;
#define	ST_P_REC_NONE 0x00
#define	ST_P_REC_FMT 0x01	/* can recognise format of new media */
#define	ST_P_REC_PART 0x02  /* can recognise partitions of new media */
#define	ST_P_REC_FMT_PART 0x03	/* can recognise format and parts */
			u_char	reserved1;
			u_char	reserved2;
			struct
			{
				u_char	high;
				u_char	low;
			}part[ST_MAXPARTS];
    		} medium_partition;
		struct
		{
			struct
			{
				u_char	high;
				u_char	low;
			}part[ST_MAXPARTS];
    		} medium_partition_extra;
	}pages;
};


#endif /*SCSI_SCSI_TAPE_H*/
