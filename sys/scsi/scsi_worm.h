#ifndef _SCSI_SCSI_WORM_H
#define _SCSI_SCSI_WORM_H

#define PAGE_HEADERLEN 2

/*
 * Opcodes
 */

#define REZERO_UNIT		0x01	/* re-init; XXX belongs to scsi_all? */
#define SYNCHRONIZE_CACHE	0x35	/* flush write buffer, close wr chn */
#define FIRST_WRITEABLE_ADDR	0xe2	/* return first available LBA */
#define RESERVE_TRACK		0xe4	/* reserve a track for later write */
#define READ_TRACK_INFORMATION	0xe5	/* get info for a particular track */
#define WRITE_TRACK		0xe6	/* open the write channel */
#define LOAD_UNLOAD		0xe7	/* resembles part of START_STOP */
#define FIXATION		0xe9	/* write leadin/leadout */

struct scsi_rezero_unit
{
	u_char	op_code;
	u_char	byte2;
	u_char	reserved[3];
	u_char	control;
};

struct scsi_synchronize_cache
{
	u_char	op_code;
	u_char	byte2;
	u_char	reserved[7];
	u_char	control;
};

/* struct scsi_first_writeable_address; */

struct scsi_reserve_track
{
	u_char	op_code;
	u_char	byte2;
	u_char	reserved[3];
	u_char	reserve_length_3; /* MSB */
	u_char	reserve_length_2;
	u_char	reserve_length_1;
	u_char	reserve_length_0; /* LSB */
	u_char	control;
};

/* struct scsi_read_track_information; */

struct scsi_write_track
{
	u_char	op_code;
	u_char	byte2;
	u_char	reserved[3];
	u_char	track_number;	/* 0 means: use next available */
	u_char	mode;
#define WORM_TRACK_MODE_RAW	0x08
#define WORM_TRACK_MODE_AUDIO	0x04
#define WORM_TRACK_MODE_MODE1	0x01 /* also audio with preemphasis */
#define WORM_TRACK_MODE_MODE2	0x02
	u_char	transfer_length_1; /* number of blocks to transfer, MSB */
	u_char	transfer_length_0; /* LSB */
	u_char	control;
#define WORM_TRACK_CONTROL_MIX	0x40 /* mixed mode blocks */
};

struct scsi_load_unload
{
	u_char	op_code;
	u_char	byte2;
	u_char	reserved[6];
	u_char	load;
#define WORM_LOAD_MEDIUM	0x01
	u_char	control;
};

struct scsi_fixation
{
	u_char	op_code;
	u_char	byte2;
	u_char	reserved[6];
	u_char	action;
#define WORM_FIXATION_ONP	0x08 /* open next program area (new session) */
#define WORM_TOC_TYPE_AUDIO	0x00
#define WORM_TOC_TYPE_CDROM	0x01
#define WORM_TOC_TYPE_CDROM_1	0x02 /* CD-ROM, first track mode 1 (?) */
#define WORM_TOC_TYPE_CDROM_2	0x03 /* CD-ROM, first track mode 2 */
#define WORM_TOC_TYPE_CDI      	0x04
	u_char	control;
};

#endif /* _SCSI_SCSI_WORM_H */
