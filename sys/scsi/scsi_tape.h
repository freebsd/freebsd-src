/*
 * HISTORY
 * $Log: scsi_tape.h,v $
 * Revision 1.2  1993/01/26  18:39:08  julian
 * add the 'write protected' bit in the device status struct.
 *
 * Revision 1.1  1992/09/26  22:10:21  julian
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
 */

/*
 * SCSI command format
 */


struct scsi_rw_tape
{
	u_char	op_code;
	u_char	fixed:1;
	u_char	:4;	
	u_char	lun:3;
	u_char	len[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
} rw_tape;

struct scsi_space
{
	u_char	op_code;
	u_char	code:2;
	u_char	:3;
	u_char	lun:3;
	u_char	number[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
} space;
#define SP_BLKS	0
#define SP_FILEMARKS 1
#define SP_SEQ_FILEMARKS 2
#define	SP_EOM	3

struct scsi_write_filemarks
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	number[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
} write_filemarks;

struct scsi_rewind
{
	u_char	op_code;
	u_char	immed:1;
	u_char	:4;
	u_char	lun:3;
	u_char	unused[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
} rewind;

struct scsi_load
{
	u_char	op_code;
	u_char	immed:1;
	u_char	:4;
	u_char	lun:3;
	u_char	unused[2];
	u_char	load:1;
	u_char	reten:1;
	u_char	:6;
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
} load;
#define LD_UNLOAD 0
#define LD_LOAD 1

struct scsi_blk_limits
{
	u_char	op_code;
	u_char	:5;
	u_char	lun:3;
	u_char	unused[3];
	u_char	link:1;
	u_char	flag:1;
	u_char	:6;
} blk_limits;

/*
 * Opcodes
 */

#define REWIND			0x01
#define	READ_BLK_LIMITS		0x05
#define	READ_COMMAND_TAPE	0x08
#define WRITE_COMMAND_TAPE	0x0a
#define	WRITE_FILEMARKS		0x10
#define	SPACE			0x11
#define LOAD_UNLOAD		0x1b /* same as above */



struct scsi_blk_limits_data
{
	u_char	reserved;
	u_char	max_length_2;	/* Most significant */
	u_char	max_length_1;
	u_char	max_length_0;	/* Least significant */
	u_char	min_length_1;	/* Most significant */
	u_char	min_length_0;	/* Least significant */
};

struct	scsi_mode_header_tape
{
	u_char  data_length;    /* Sense data length */
	u_char  medium_type;
	u_char	speed:4;
	u_char	buf_mode:3;
	u_char	write_protected:1;
	u_char  blk_desc_len;
};


#define QIC_120     0x0f
#define QIC_150     0x10
#define QIC_320     0x11
#define QIC_525     0x11
#define QIC_1320     0x12


