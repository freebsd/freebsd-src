/* Shared between kernel & process */
/* $FreeBSD$ */

#ifndef	_SYS_WORMIO_H_
#define	_SYS_WORMIO_H_

#include <sys/ioccom.h>

/***************************************************************\
* Ioctls for the WORM drive					*
\***************************************************************/


/*
 * Prepare disk-wide parameters.
 */

struct wormio_prepare_disk
{
	int dummy;		/* use dummy writes, laser turned off */
	int speed;		/* drive speed selection */
};

#define WORMIOCPREPDISK		_IOW('W', 20, struct wormio_prepare_disk)

/*
 * Prepare track-specific parameters.
 */

struct wormio_prepare_track
{
	int audio;		/* audio track (data track if 0) */
	int preemp;		/* audio with preemphasis */
#define BLOCK_RAW             0 /* 2352 bytes, raw data */
#define BLOCK_RAWPQ           1 /* 2368 bytes, raw data with P and Q subchannels */
#define BLOCK_RAWPW           2 /* 2448 bytes, raw data with P-W subchannel appended */
#define BLOCK_MODE_1          8 /* 2048 bytes, mode 1 (ISO/IEC 10149) */
#define BLOCK_MODE_2          9 /* 2336 bytes, mode 2 (ISO/IEC 10149) */
#define BLOCK_MODE_2_FORM_1  10 /* 2048 bytes, CD-ROM XA form 1 */
#define BLOCK_MODE_2_FORM_1b 11 /* 2056 bytes, CD-ROM XA form 1 */
#define BLOCK_MODE_2_FORM_2  12 /* 2324 bytes, CD-ROM XA form 2 */
#define BLOCK_MODE_2_FORM_2b 13 /* 2332 bytes, CD-ROM XA form 2 */
        int track_type;         /* defines the number of bytes in a block */
#define COPY_INHIBIT    0       /* no copy allowed */
#define COPY_PERMITTED  1       /* track can be copied */
#define COPY_SCMS       2       /* alternate copy */
        int copy_bits;          /* define the possibilities for copying */
        int track_number;
        char ISRC_country[2];   /* country code (2 chars) */
        char ISRC_owner[3];     /* owner code (3 chars) */
        int ISRC_year;          /* year of recording */
        char ISRC_serial[5];    /* serial number */
};
#define WORMIOCPREPTRACK	_IOW('W', 21, struct wormio_prepare_track)


/*
 * Fixation: write leadins and leadouts.  Select table-of-contents
 * type for this session.  If onp is != 0, another session will be
 * opened.
 */

struct wormio_fixation
{
	int toc_type;		/* TOC type */
	int onp;		/* open next program area */
};

#define WORMIOCFIXATION		_IOW('W', 22, struct wormio_fixation)

/* 
 * Finalize track
 */
#define WORMIOCFINISHTRACK      _IO('W', 23)


struct wormio_session_info {
    u_short lead_in;
    u_short lead_out;
};
#define WORMIOCREADSESSIONINFO  _IOR('W', 31, struct wormio_session_info)

struct wormio_write_session {
    int toc_type;
    int onp;
    int lofp;
    int length;
    char catalog[13];
    u_char *track_desc;
};
#define WORMIOCWRITESESSION     _IOW('W', 32, struct wormio_write_session)

struct wormio_first_writable_addr {
    int track;
    int mode;
    int raw;
    int audio;
    int *addr;
};
#define WORMIOCFIRSTWRITABLEADDR  _IOWR('W', 33, struct wormio_first_writable_addr)

#define CDRIOCBLANK		_IO('c', 100)
#define CDRIOCNEXTWRITEABLEADDR	_IOR('c', 101, int)

/* Errors/warnings */
#define WORM_SEQUENCE_ERROR                  1
#define WORM_DUMMY_BLOCKS_ADDED              2
#define WORM_CALIBRATION_AREA_ALMOST_FULL    3
#define WORM_CALIBRATION_AREA_FULL           4
#define WORM_BUFFER_UNDERRUN                 5
#define WORM_ABSORPTION_CONTROL_ERROR        6
#define WORM_END_OF_MEDIUM                   7
#define WORM_OPTIMUM_POWER_CALIBRATION_ERROR 8

#define WORMIOERROR            _IOR('W', 24, int)

#endif /* !_SYS_WORMIO_H_ */
