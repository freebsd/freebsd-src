/*-
 * Copyright (c) 1998,1999,2000 Søren Schmidt
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification, immediately at the beginning of the file.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 */

/* MODE SENSE parameter header */ 
struct afd_header {
    u_int16_t	data_length;
    u_int8_t	medium_type;
#define MFD_2DD_UN		0x10
#define MFD_2DD			0x11
#define MFD_HD_UN		0x20
#define MFD_HD_12_98		0x22
#define MFD_HD_12		0x23
#define MFD_HD_144		0x24
#define MFD_UHD			0x31

#define MFD_UNKNOWN		0x00
#define MFD_NO_DISC		0x70
#define MFD_DOOR_OPEN		0x71
#define MFD_FMT_ERROR		0x72

    u_int8_t	reserved0	:7;
    u_int8_t	wp		:1;		/* write protect */
    u_int8_t	unused[4];
};

/* ATAPI Rewriteable drive Capabilities and Mechanical Status Page */
struct afd_cappage {
    u_int8_t	page_code	:6;
#define ATAPI_REWRITEABLE_CAP_PAGE	  0x05

    u_int8_t	reserved1_6	:1;
    u_int8_t	ps		:1;		/* page save supported */
    u_int8_t	page_length;			/* page length */
    u_int16_t	transfer_rate;			/* in kilobits per second */
    u_int8_t	heads;				/* number of heads */
    u_int8_t	sectors;			/* number of sectors pr track */
    u_int16_t	sector_size;			/* number of bytes per sector */
    u_int16_t	cylinders;			/* number of cylinders */
    u_int8_t	reserved10[10];
    u_int8_t	motor_delay;			/* motor off delay */
    u_int8_t	reserved21[7];
    u_int16_t	rpm;				/* rotations per minute */
    u_int8_t	reserved30[2];
};

struct afd_softc {
    struct atapi_softc		*atp;		/* controller structure */
    int				lun;		/* logical device unit */
    int				transfersize;	/* max size of each transfer */
    struct bio_queue_head	bio_queue;	/* queue of i/o requests */
    struct afd_header		header;		/* capabilities page info */
    struct afd_cappage		cap;		/* capabilities page info */
    struct disk			disk;		/* virtual drives */
    struct devstat		stats;
    dev_t			dev;		/* device place holder */
};

