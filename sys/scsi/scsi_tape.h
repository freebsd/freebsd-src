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
 *	$Id: scsi_tape.h,v 1.8 93/08/31 21:40:16 julian Exp Locker: julian $
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
} rw_tape;

struct scsi_space
{
	u_char	op_code;
	u_char	byte2;
#define	SS_CODE	0x03
	u_char	number[3];
	u_char	control;
} space;
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
} write_filemarks;

struct scsi_rewind
{
	u_char	op_code;
	u_char	byte2;
#define	SR_IMMED	0x01
	u_char	unused[3];
	u_char	control;
} rewind;

struct scsi_load
{
	u_char	op_code;
	u_char	byte2;
#define	SL_IMMED	0x01
	u_char	unused[2];
	u_char	how;
	u_char	control;
} load;
#define LD_UNLOAD 0
#define LD_LOAD 1
#define LD_RETEN 2


struct scsi_blk_limits
{
	u_char	op_code;
	u_char	byte2;
	u_char	unused[3];
	u_char	control;
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
#define	QIC_11		0x04	/* from Archive 150S Theory of Op. XXX	*/
#define QIC_24		0x05	/* may be bad, works for CIPHER ST150S XXX */
#define QIC_120		0x0f
#define QIC_150		0x10
#define QIC_320		0x11
#define QIC_525		0x11
#define QIC_1320	0x12
#define DDS		0x13
#define DAT_1		0x13

#endif /*SCSI_SCSI_TAPE_H*/
