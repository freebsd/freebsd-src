/*
 * HISTORY
 * $Log:	scsi_tape.h,v $
 * 
 * julian - added some special stuff for some OLD scsi tapes (CIPHER
 *          ST150S)
 *
 * Revision 1.1.1.1  1993/06/12  14:57:27  rgrimes
 * Initial import, 0.1 + pk 0.2.4-B1
 *
 * 
 * Revision 1.2  1993/01/26  18:39:08  julian
 * add the 'write protected' bit in the device status struct.
 *
 * Revision 1.1  1992/09/26  22:10:21  julian
 * Initial revision
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

/* A special for the CIPHER ST150S(old drive) */
struct	blk_desc_cipher
{
	u_char	density;
	u_char	nblocks[3];
	u_char	reserved;
	u_char	blklen[3];
	u_char  sec:1;		/* soft error count */
	u_char	aui:1;		/* autoload inhibit */
	u_char  :6;
};



/**********************************************************************
			from the scsi2 spec
                Value Tracks Density(bpi) Code Type  Reference     Note
                0x1     9       800       NRZI  R    X3.22-1983    2
                0x2     9      1600       PE    R    X3.39-1986    2
                0x3     9      6250       GCR   R    X3.54-1986    2
                0x5    4/9     8000       GCR   C    X3.136-1986   1
                0x6     9      3200       PE    R    X3.157-1987   2
                0x7     4      6400       IMFM  C    X3.116-1986   1
                0x8     4      8000       GCR   CS   X3.158-1986   1
                0x9    18     37871       GCR   C    X3B5/87-099   2
                0xA    22      6667       MFM   C    X3B5/86-199   1
                0xB     4      1600       PE    C    X3.56-1986    1
                0xC    24     12690       GCR   C    HI-TC1        1,5
                0xD    24     25380       GCR   C    HI-TC2        1,5
                0xF    15     10000       GCR   C    QIC-120       1,5
                0x10   18     10000       GCR   C    QIC-150       1,5
                0x11   26     16000       GCR   C    QIC-320(525?) 1,5
                0x12   30     51667       RLL   C    QIC-1350      1,5
                0x13    1     61000       DDS   CS    X3B5/88-185A 4
                0x14    1     43245       RLL   CS    X3.202-1991  4
                0x15    1     45434       RLL   CS    ECMA TC17    4
                0x16   48     10000       MFM   C     X3.193-1990  1
                0x17   48     42500       MFM   C     X3B5/91-174  1

                where Code means:
                NRZI Non Return to Zero, change on ones
                GCR  Group Code Recording
                PE   Phase Encoded
                IMFM Inverted Modified Frequency Modulation
                MFM  Modified Frequency Modulation
                DDS  Dat Data Storage
                RLL  Run Length Encoding

                where Type means:
                R    Real-to-Real
                C    Cartridge
                CS   cassette

                where Notes means:
                1    Serial Recorded
                2    Parallel Recorded
                3    Old format know as QIC-11
                4    Helical Scan
                5    Not ANSI standard, rather industry standard.

********************************************************************/

#define	HALFINCH_800	0x01
#define	HALFINCH_1600	0x02
#define	HALFINCH_6250	0x03
#define QIC_24		0x05	/* may be bad, works for CIPHER ST150S XXX */
#define QIC_120		0x0f
#define QIC_150		0x10
#define QIC_320		0x11
#define QIC_525		0x11
#define QIC_1320	0x12


