/*-
 * Copyright (c) 2000,2001 Søren Schmidt
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

#ifndef _SYS_ATA_H_
#define _SYS_ATA_H_

#include <sys/ioccom.h>

/* ATA/ATAPI device parameter information */
struct ata_params {
    u_int8_t    cmdsize         :2;             /* packet command size */
#define ATAPI_PSIZE_12                 0       /* 12 bytes */
#define ATAPI_PSIZE_16                 1       /* 16 bytes */

    u_int8_t                    :3;
    u_int8_t    drqtype         :2;             /* DRQ type */
#define ATAPI_DRQT_MPROC        	0       /* cpu    3 ms delay */
#define ATAPI_DRQT_INTR         	1       /* intr  10 ms delay */
#define ATAPI_DRQT_ACCEL        	2       /* accel 50 us delay */

    u_int8_t    removable       :1;             /* device is removable */
    u_int8_t    device_type     :5;             /* device type */
#define ATAPI_TYPE_DIRECT       	0       /* disk/floppy */
#define ATAPI_TYPE_TAPE         	1       /* streaming tape */
#define ATAPI_TYPE_CDROM        	5       /* CD-ROM device */
#define ATAPI_TYPE_OPTICAL      	7       /* optical disk */

    u_int8_t                    :1;
    u_int8_t    proto           :2;             /* command protocol */
#define ATAPI_PROTO_ATAPI       	2

    u_int16_t	cylinders;			/* number of cylinders */
    u_int16_t	reserved2;
    u_int16_t	heads;				/* # heads */
    u_int16_t	unfbytespertrk;			/* # unformatted bytes/track */
    u_int16_t	unfbytes;			/* # unformatted bytes/sector */
    u_int16_t	sectors;			/* # sectors/track */
    u_int16_t	vendorunique0[3];
    u_int8_t	serial[20];			/* serial number */
    u_int16_t	buffertype;			/* buffer type */
#define ATA_BT_SINGLEPORTSECTOR		1	/* 1 port, 1 sector buffer */
#define ATA_BT_DUALPORTMULTI		2	/* 2 port, mult sector buffer */
#define ATA_BT_DUALPORTMULTICACHE	3	/* above plus track cache */

    u_int16_t	buffersize;			/* buf size, 512-byte units */
    u_int16_t	necc;				/* ecc bytes appended */
    u_int8_t	revision[8];			/* firmware revision */
    u_int8_t	model[40];			/* model name */
    u_int8_t	nsecperint;			/* sectors per interrupt */
    u_int8_t	vendorunique1;
    u_int16_t	usedmovsd;			/* double word read/write? */

    u_int8_t	vendorcap;			/* vendor capabilities */
    u_int8_t	dmaflag		:1;		/* DMA supported - always 1 */
    u_int8_t	lbaflag		:1;		/* LBA supported - always 1 */
    u_int8_t	iordydis	:1;		/* IORDY may be disabled */
    u_int8_t	iordyflag	:1;		/* IORDY supported */
    u_int8_t	softreset	:1;		/* needs softreset when busy */
    u_int8_t	stdby_ovlap	:1;		/* standby/overlap supported */
    u_int8_t	queueing	:1;		/* supports queuing overlap */
    u_int8_t	idmaflag	:1;		/* interleaved DMA supported */
    u_int16_t	capvalidate;			/* validation for above */

    u_int8_t	vendorunique3;
    u_int8_t	opiomode;			/* PIO modes 0-2 */
    u_int8_t	vendorunique4;
    u_int8_t	odmamode;			/* old DMA modes, not ATA-3 */

    u_int16_t	atavalid;			/* fields valid */
#define	ATA_FLAG_54_58	      		1	/* words 54-58 valid */
#define	ATA_FLAG_64_70	      		2	/* words 64-70 valid */
#define	ATA_FLAG_88	      		4	/* word 88 valid */

    u_int16_t	currcyls;
    u_int16_t	currheads;
    u_int16_t	currsectors;
    u_int16_t	currsize0;
    u_int16_t	currsize1;
    u_int8_t	currmultsect;
    u_int8_t	multsectvalid;
    u_int32_t	lbasize;

    u_int16_t	sdmamodes;			/* singleword DMA modes */ 
    u_int16_t	wdmamodes;			/* multiword DMA modes */ 
    u_int16_t	apiomodes;			/* advanced PIO modes */ 

    u_int16_t	mwdmamin;			/* min. M/W DMA time/word ns */
    u_int16_t	mwdmarec;			/* rec. M/W DMA time ns */
    u_int16_t	pioblind;			/* min. PIO cycle w/o flow */
    u_int16_t	pioiordy;			/* min. PIO cycle IORDY flow */

    u_int16_t	reserved69;
    u_int16_t	reserved70;
    u_int16_t	rlsovlap;			/* rel time (us) for overlap */
    u_int16_t	rlsservice;			/* rel time (us) for service */
    u_int16_t	reserved73;
    u_int16_t	reserved74;
    u_int16_t	queuelen:5;
    u_int16_t	:11;
    u_int16_t	reserved76;
    u_int16_t	reserved77;
    u_int16_t	reserved78;
    u_int16_t	reserved79;
    u_int16_t	versmajor;
    u_int16_t	versminor;
    u_int16_t	featsupp1;	/* 82 */
    u_int16_t	supmicrocode:1;
    u_int16_t	supqueued:1;
    u_int16_t	supcfa:1;
    u_int16_t	supapm:1;
    u_int16_t	suprmsn:1;
    u_int16_t	:11;
    u_int16_t	featsupp3;	/* 84 */
    u_int16_t	featenab1;	/* 85 */
    u_int16_t	enabmicrocode:1;
    u_int16_t	enabqueued:1;
    u_int16_t	enabcfa:1;
    u_int16_t	enabapm:1;
    u_int16_t	enabrmsn:1;
    u_int16_t	:11;
    u_int16_t	featenab3;	/* 87 */
    u_int16_t	udmamodes;			/* UltraDMA modes */
    u_int16_t	erasetime;
    u_int16_t	enherasetime;
    u_int16_t	apmlevel;
    u_int16_t	masterpasswdrev;
    u_int16_t	masterhwres	:8;
    u_int16_t	slavehwres	:5;
    u_int16_t	cblid		:1;
    u_int16_t	reserved93_1415	:2;
    u_int16_t	reserved94[32];
    u_int16_t	rmvstat;
    u_int16_t	securstat;
    u_int16_t	reserved129[30];
    u_int16_t	cfapwrmode;
    u_int16_t	reserved160[85];
    u_int16_t	integrity;
    u_int16_t	reserved246[10];
};

#define	ATA_MODE_MASK		0x0f
#define	ATA_DMA_MASK		0xf0
#define ATA_PIO			0x00
#define ATA_PIO0		0x08
#define	ATA_PIO1		0x09
#define	ATA_PIO2		0x0a
#define	ATA_PIO3		0x0b
#define	ATA_PIO4		0x0c
#define	ATA_DMA			0x10
#define	ATA_WDMA		0x20
#define	ATA_WDMA2		0x22
#define	ATA_UDMA		0x40
#define	ATA_UDMA2		0x42
#define	ATA_UDMA4		0x44
#define	ATA_UDMA5		0x45

struct ata_cmd {
    int 			channel;
    int				cmd;
#define ATAGPARM      	 	1
#define ATAGMODE        	2
#define ATASMODE        	3
#define ATAREINIT       	4
#define ATAATTACH       	5
#define ATADETACH       	6

    union {
	struct {
	    int			mode[2];
	} mode;
	struct 	{
	    int			type[2];
	    char		name[2][32];
	    struct ata_params	params[2];
	} param;
    } u;
};

#define IOCATA			_IOWR('a',  1, struct ata_cmd)

#endif /* _SYS_ATA_H_ */
