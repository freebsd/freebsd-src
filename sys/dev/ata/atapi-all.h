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
 *	$Id: atapi-all.h,v 1.3 1999/03/07 21:49:14 sos Exp $
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
#define ATAPI_SK_DATA_PROTECT     	0x70    /* write protect */
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
    u_int8_t	cmdsize 	:2;		/* packet command size */
#define		ATAPI_PSIZE_12		0	/* 12 bytes */
#define		ATAPI_PSIZE_16		1	/* 16 bytes */

    u_int8_t 			:3;
    u_int8_t	drqtype 	:2;		/* DRQ type */
#define 	ATAPI_DRQT_MPROC	0	/* cpu    3 ms delay */
#define 	ATAPI_DRQT_INTR		1	/* intr  10 ms delay */
#define 	ATAPI_DRQT_ACCEL	2	/* accel 50 us delay */

    u_int8_t	removable 	:1;		/* device is removable */
    u_int8_t	device_type 	:5;		/* device type */
#define		ATAPI_TYPE_DIRECT	0	/* disk/floppy */
#define 	ATAPI_TYPE_TAPE		1	/* streaming tape */
#define	 	ATAPI_TYPE_CDROM	5	/* CD-ROM device */
#define		ATAPI_TYPE_OPTICAL	7	/* optical disk */

    u_int8_t 			:1;
    u_int8_t	proto 		:2;		/* command protocol */
#define 	ATAPI_PROTO_ATAPI	2

    int16_t	reserved1;
    int16_t	reserved2;
    int16_t	reserved3;
    int16_t	reserved4;
    int16_t	reserved5;
    int16_t	reserved6;
    int16_t	reserved7;
    int16_t	reserved8;
    int16_t	reserved9;
    int8_t	serial[20];			/* serial number */
    int16_t	reserved20;
    int16_t	reserved21;
    int16_t	reserved22;
    int8_t	revision[8];			/* firmware revision */
    int8_t	model[40];			/* model name */
    int16_t	reserved47;
    int16_t	reserved48;

    u_int8_t	vendorcap;			/* vendor capabilities */
    u_int8_t	dmaflag 	:1;		/* DMA supported */
    u_int8_t	lbaflag		:1;		/* LBA supported - always 1 */
    u_int8_t	iordydis	:1;		/* IORDY can be disabled */
    u_int8_t	iordyflag	:1;		/* IORDY supported */
    u_int8_t 			:1;
    u_int8_t	ovlapflag	:1;		/* overlap supported */
    u_int8_t		 	:1;
    u_int8_t	idmaflag	:1;		/* interleaved DMA supported */
    int16_t	capvalidate;			/* validation for above */

    u_int16_t	piotiming;			/* PIO cycle timing */
    u_int16_t	dmatiming;			/* DMA cycle timing */

    u_int16_t	atavalid;			/* fields valid */
#define 	ATAPI_FLAG_54_58	1	/* words 54-58 valid */
#define 	ATAPI_FLAG_64_70	2	/* words 64-70 valid */

    int16_t	reserved54[8];

    int16_t     sdmamodes;                      /* singleword DMA modes */
    int16_t     wdmamodes;                      /* multiword DMA modes */
    int16_t     apiomodes;                      /* advanced PIO modes */ 

    u_int16_t	mwdmamin;			/* min. M/W DMA time/word ns */
    u_int16_t	mwdmarec;			/* rec. M/W DMA time ns */
    u_int16_t	pioblind;			/* min. PIO cycle w/o flow */
    u_int16_t	pioiordy;			/* min. PIO cycle IORDY flow */

    int16_t	reserved69;
    int16_t	reserved70;
    u_int16_t	rlsovlap;			/* rel time (us) for overlap */
    u_int16_t	rlsservice;			/* rel time (us) for service */
    int16_t     reserved73;
    int16_t     reserved74;
    int16_t     queuelen;
    int16_t     reserved76;
    int16_t     reserved77;
    int16_t     reserved78;
    int16_t     reserved79;
    int16_t     versmajor;
    int16_t     versminor;
    int16_t     featsupp1;
    int16_t     featsupp2;
    int16_t     featsupp3;
    int16_t     featenab1;
    int16_t     featenab2;
    int16_t     featenab3;
    int16_t     udmamodes;			/* UltraDMA modes */
    int16_t     erasetime;
    int16_t     enherasetime;
    int16_t     apmlevel;
    int16_t     reserved92[34];
    int16_t     rmvcap;
    int16_t     securelevel;
};

/* ATAPI REQUEST SENSE structure */   
struct reqsense {
    u_int8_t    error_code      :7;     /* current or deferred errors */
    u_int8_t    valid           :1;     /* follows QIC-157C */ 
    u_int8_t    reserved1;              /* Segment number - reserved */
    u_int8_t    sense_key       :4;     /* sense key */
    u_int8_t    reserved2_4     :1;     /* reserved */
    u_int8_t    ili             :1;     /* incorrect length indicator */
    u_int8_t    eom             :1;     /* end of medium */
    u_int8_t    filemark        :1;     /* filemark */
    u_int8_t    info __attribute__((packed)); /* cmd specific info */
    u_int8_t    asl;                    /* additional sense length (n-7) */
    u_int8_t    command_specific;       /* additional cmd specific info */
    u_int8_t    asc;                    /* additional sense code */
    u_int8_t    ascq;                   /* additional sense code qualifier */
    u_int8_t    replaceable_unit_code;  /* field replaceable unit code */
    u_int8_t    sk_specific1    :7;     /* sense key specific */
    u_int8_t    sksv            :1;     /* sense key specific info valid */
    u_int8_t    sk_specific2;           /* sense key specific */
    u_int8_t    sk_specific3;           /* sense key Specific */
    u_int8_t    pad[2];                 /* padding */
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
#define		ATAPI_F_DMA_ENABLED	0x0002
#define		ATAPI_F_DMA_USED	0x0004

    u_int32_t                   bytecount;      /* bytes to transfer */
    u_int32_t                   result;		/* result code */
    int8_t			*data;		/* pointer to data buf */
    struct buf			*bp;		/* associated buf ptr */
    atapi_callback_t		*callback;	/* ptr to callback func */
    TAILQ_ENTRY(atapi_request)	chain;		/* list management */
};

void atapi_transfer(struct atapi_request *);
int32_t atapi_interrupt(struct atapi_request *);
int32_t atapi_queue_cmd(struct atapi_softc *, int8_t [], void *, int32_t, int32_t, atapi_callback_t, void *, struct buf *);
void atapi_error(struct atapi_softc *, int32_t);
void atapi_dump(int8_t *, void *, int32_t);

