/*-
 * Copyright (c) 1998,1999 Søren Schmidt
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
 *	$Id: atapi-all.h,v 1.2 1999/03/01 12:11:01 sos Exp $
 */

/* ATAPI misc defines */
#define	ATAPI_MAGIC_LSB			0x14
#define	ATAPI_MAGIC_MSB			0xeb
#define ATAPI_P_READ			(ATA_S_DRQ | ATA_I_IN)
#define ATAPI_P_WRITE			(ATA_S_DRQ)
#define ATAPI_P_CMDOUT			(ATA_S_DRQ | ATA_I_CMD)
#define ATAPI_P_ABORT			0
#define ATAPI_P_DONE			(ATA_I_IN | ATA_I_CMD)

/* error register bits */
#define ATAPI_E_ILI			0x01	/* illegal length indication */
#define ATAPI_E_EOM         		0x02    /* end of media detected */
#define ATAPI_E_ABRT        		0x04    /* command aborted */
#define ATAPI_E_MCR         		0x08	/* media change requested */
#define ATAPI_SK_MASK             	0xf0    /* sense key mask */
#define ATAPI_SK_NO_SENSE         	0x00    /* no specific sense key info */
#define ATAPI_SK_RECOVERED_ERROR  	0x10    /* command OK, data recovered */
#define ATAPI_SK_NOT_READY        	0x20    /* no access to drive */
#define ATAPI_SK_MEDIUM_ERROR     	0x30    /* non-recovered data error */
#define ATAPI_SK_HARDWARE_ERROR   	0x40    /* non-recoverable HW failure */
#define ATAPI_SK_ILLEGAL_REQUEST  	0x50    /* invalid command param(s) */
#define ATAPI_SK_UNIT_ATTENTION   	0x60    /* media changed */
#define ATAPI_SK_DATA_PROTECT     	0x70    /* reading read-protected sec */
#define ATAPI_SK_BLANK_CHECK  		0x80    /* blank check */
#define ATAPI_SK_VENDOR_SPECIFIC 	0x90    /* vendor specific skey */
#define ATAPI_SK_COPY_ABORTED  		0xa0    /* copy aborted */
#define ATAPI_SK_ABORTED_COMMAND  	0xb0    /* command aborted, try again */
#define ATAPI_SK_EQUAL			0xc0	/* equal */
#define ATAPI_SK_VOLUME_OVERFLOW	0xd0	/* volume overflow */
#define ATAPI_SK_MISCOMPARE		0xe0    /* data dont match the medium */
#define ATAPI_SK_RESERVED		0xf0

/* ATAPI commands */
#define ATAPI_TEST_UNIT_READY   	0x00    /* check if device is ready */
#define ATAPI_REZERO_UNIT   		0x01    /* reinit device */
#define ATAPI_REQUEST_SENSE    		0x03    /* get sense data */
#define ATAPI_START_STOP       	 	0x1b    /* start/stop the media */
#define ATAPI_PREVENT_ALLOW    	 	0x1e    /* media removal */
#define ATAPI_READ_CAPACITY    	 	0x25    /* get volume capacity */
#define ATAPI_READ_BIG         	 	0x28    /* read data */
#define ATAPI_WRITE_BIG			0x2a    /* write data */
#define ATAPI_SYNCHRONIZE_CACHE		0x35    /* flush buf, close channel */
#define ATAPI_READ_SUBCHANNEL   	0x42    /* get subchannel info */
#define ATAPI_READ_TOC          	0x43    /* get table of contents */
#define ATAPI_PLAY_MSF          	0x47    /* play by MSF address */
#define ATAPI_PLAY_TRACK        	0x48    /* play by track number */
#define ATAPI_PAUSE             	0x4b    /* stop/start audio operation */
#define ATAPI_READ_TRACK_INFO   	0x52    /* get track info structure */
#define ATAPI_MODE_SELECT		0x55    /* set device parameters */
#define ATAPI_MODE_SENSE        	0x5a    /* get device parameters */
#define ATAPI_CLOSE_TRACK      	 	0x5b    /* close track/session */
#define ATAPI_BLANK			0xa1	/* blank (erase) media */
#define ATAPI_PLAY_BIG          	0xa5    /* play by lba */
#define ATAPI_LOAD_UNLOAD       	0xa6    /* changer control command */
#define ATAPI_PLAY_CD           	0xb4    /* universal play command */
#define ATAPI_MECH_STATUS       	0xbd    /* get changer status */
#define ATAPI_READ_CD           	0xbe    /* read data */

/* ATAPI device parameter information */
struct atapi_params {
    u_int	cmdsize 	:2;		/* packet command size */
#define		ATAPI_PSIZE_12		0	/* 12 bytes */
#define		ATAPI_PSIZE_16		1	/* 16 bytes */

    u_int 			:3;
    u_int	drqtype 	:2;		/* DRQ type */
#define 	ATAPI_DRQT_MPROC	0	/* cpu    3 ms delay */
#define 	ATAPI_DRQT_INTR		1	/* intr  10 ms delay */
#define 	ATAPI_DRQT_ACCEL	2	/* accel 50 us delay */

    u_int	removable 	:1;		/* device is removable */
    u_int	device_type 	:5;		/* device type */
#define		ATAPI_TYPE_DIRECT	0	/* disk/floppy */
#define 	ATAPI_TYPE_TAPE		1	/* streaming tape */
#define	 	ATAPI_TYPE_CDROM	5	/* CD-ROM device */
#define		ATAPI_TYPE_OPTICAL	7	/* optical disk */

    u_int 			:1;
    u_int	proto 		:2;		/* command protocol */
#define 	ATAPI_PROTO_ATAPI	2

    int16_t	reserved1[9];
    int8_t	serial[20];			/* serial number */
    int16_t	reserved2[3];
    int8_t	revision[8];			/* firmware revision */
    int8_t	model[40];			/* model name */
    int16_t	reserved3[2];
    u_int8_t	vendor_cap;			/* vendor capabilities */
    u_int8_t	dmaflag 	:1;		/* DMA supported */
    u_int8_t	lbaflag		:1;		/* LBA supported - always 1 */
    u_int8_t	iordydis	:1;		/* IORDY can be disabled */
    u_int8_t	iordyflag	:1;		/* IORDY supported */
    u_int8_t 			:1;
    u_int8_t	ovlapflag	:1;		/* overlap supported */
    u_int8_t		 	:1;
    u_int8_t	idmaflag	:1;		/* interleaved DMA supported */
    int16_t	reserved4;
    u_int16_t	pio_timing;			/* PIO cycle timing */
    u_int16_t	dma_timing;			/* DMA cycle timing */
    u_int16_t	flags;
#define 	ATAPI_FLAG_54_58	1	/* words 54-58 valid */
#define 	ATAPI_FLAG_64_70	2	/* words 64-70 valid */

    int16_t	reserved5[8];
    u_int8_t	swdma_flag;			/* singleword DMA supported */
    u_int8_t	swdma_active;			/* singleword DMA active */
    u_int8_t	mwdma_flag;			/* multiword DMA supported */
    u_int8_t	mwdma_active;			/* multiword DMA active */
    u_int8_t	apio_flag;			/* advanced PIO supported */
    u_int8_t	reserved6;
    u_int16_t	mwdma_min;			/* min. M/W DMA time/word ns */
    u_int16_t	mwdma_dflt;			/* rec. M/W DMA time ns */
    u_int16_t	pio_nfctl_min;			/* min. PIO cycle w/o flow */
    u_int16_t	pio_iordy_min;			/* min. PIO cycle IORDY flow */
    int16_t	reserved7[2];
    u_int16_t	rls_ovlap;			/* rel time (us) for overlap */
    u_int16_t	rls_service;			/* rel time (us) for service */
};

struct atapi_softc {
    struct ata_softc            *controller;    /* ptr to parent ctrl */
    struct atapi_params         *atapi_parm;    /* ata device params */
    int32_t                     unit;           /* ATA_MASTER or ATA_SLAVE */
    u_int32_t                   flags;          /* drive flags */
};

typedef void atapi_callback_t(struct atapi_request *);

struct atapi_request {
    struct atapi_softc		*device;	/* ptr to parent device */
    void			*driver;	/* ptr to calling driver */
    u_int8_t			ccb[16];	/* command control block */
    int32_t			ccbsize;	/* size of ccb (12 | 16) */
    int32_t			flags;
#define		A_READ			0x0001

    u_int32_t                   bytecount;      /* bytes to transfer */
    u_int32_t                   result;		/* result code */
    int8_t			*data;		/* pointer to data buf */
    struct buf			*bp;		/* associated buf ptr */
    atapi_callback_t		*callback;	/* ptr to callback func */
    TAILQ_ENTRY(atapi_request)	chain;		/* list management */
};

void atapi_transfer(struct atapi_request *);
void atapi_interrupt(struct atapi_request *);
int atapi_queue_cmd(struct atapi_softc *, int8_t [], void *, int32_t, int32_t, atapi_callback_t, void *, struct buf *);
void atapi_error(struct atapi_softc *, int32_t);
void atapi_dump(int8_t *, void *, int32_t);

