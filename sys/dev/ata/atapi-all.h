/*-
 * Copyright (c) 1998,1999,2000,2001,2002 Søren Schmidt <sos@FreeBSD.org>
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

/* ATAPI misc defines */
#define ATAPI_MAGIC_LSB			0x14
#define ATAPI_MAGIC_MSB			0xeb
#define ATAPI_P_READ			(ATA_S_DRQ | ATA_I_IN)
#define ATAPI_P_WRITE			(ATA_S_DRQ)
#define ATAPI_P_CMDOUT			(ATA_S_DRQ | ATA_I_CMD)
#define ATAPI_P_DONEDRQ			(ATA_S_DRQ | ATA_I_CMD | ATA_I_IN)
#define ATAPI_P_DONE			(ATA_I_CMD | ATA_I_IN)
#define ATAPI_P_ABORT			0

/* error register bits */
#define ATAPI_E_MASK			0x0f	/* error mask */
#define ATAPI_E_ILI			0x01	/* illegal length indication */
#define ATAPI_E_EOM			0x02	/* end of media detected */
#define ATAPI_E_ABRT			0x04	/* command aborted */
#define ATAPI_E_MCR			0x08	/* media change requested */
#define ATAPI_SK_MASK			0xf0	/* sense key mask */
#define ATAPI_SK_NO_SENSE		0x00	/* no specific sense key info */
#define ATAPI_SK_RECOVERED_ERROR	0x10	/* command OK, data recovered */
#define ATAPI_SK_NOT_READY		0x20	/* no access to drive */
#define ATAPI_SK_MEDIUM_ERROR		0x30	/* non-recovered data error */
#define ATAPI_SK_HARDWARE_ERROR		0x40	/* non-recoverable HW failure */
#define ATAPI_SK_ILLEGAL_REQUEST	0x50	/* invalid command param(s) */
#define ATAPI_SK_UNIT_ATTENTION		0x60	/* media changed */
#define ATAPI_SK_DATA_PROTECT		0x70	/* write protect */
#define ATAPI_SK_BLANK_CHECK		0x80	/* blank check */
#define ATAPI_SK_VENDOR_SPECIFIC	0x90	/* vendor specific skey */
#define ATAPI_SK_COPY_ABORTED		0xa0	/* copy aborted */
#define ATAPI_SK_ABORTED_COMMAND	0xb0	/* command aborted, try again */
#define ATAPI_SK_EQUAL			0xc0	/* equal */
#define ATAPI_SK_VOLUME_OVERFLOW	0xd0	/* volume overflow */
#define ATAPI_SK_MISCOMPARE		0xe0	/* data dont match the medium */
#define ATAPI_SK_RESERVED		0xf0

/* ATAPI commands */
#define ATAPI_TEST_UNIT_READY		0x00	/* check if device is ready */
#define ATAPI_REZERO			0x01	/* rewind */
#define ATAPI_REQUEST_SENSE		0x03	/* get sense data */
#define ATAPI_FORMAT			0x04	/* format unit */
#define ATAPI_READ			0x08	/* read data */
#define ATAPI_WRITE			0x0a	/* write data */
#define ATAPI_WEOF			0x10	/* write filemark */
#define	    WF_WRITE				0x01
#define ATAPI_SPACE			0x11	/* space command */
#define	    SP_FM				0x01
#define	    SP_EOD				0x03
#define ATAPI_MODE_SELECT		0x15	/* mode select */
#define ATAPI_ERASE			0x19	/* erase */
#define ATAPI_MODE_SENSE		0x1a	/* mode sense */
#define ATAPI_START_STOP		0x1b	/* start/stop unit */
#define	    SS_LOAD				0x01
#define	    SS_RETENSION			0x02
#define	    SS_EJECT				0x04
#define ATAPI_PREVENT_ALLOW		0x1e	/* media removal */
#define ATAPI_READ_CAPACITY		0x25	/* get volume capacity */
#define ATAPI_READ_BIG			0x28	/* read data */
#define ATAPI_WRITE_BIG			0x2a	/* write data */
#define ATAPI_LOCATE			0x2b	/* locate to position */
#define ATAPI_READ_POSITION		0x34	/* read position */
#define ATAPI_SYNCHRONIZE_CACHE		0x35	/* flush buf, close channel */
#define ATAPI_WRITE_BUFFER		0x3b	/* write device buffer */
#define ATAPI_READ_BUFFER		0x3c	/* read device buffer */
#define ATAPI_READ_SUBCHANNEL		0x42	/* get subchannel info */
#define ATAPI_READ_TOC			0x43	/* get table of contents */
#define ATAPI_PLAY_10			0x45	/* play by lba */
#define ATAPI_PLAY_MSF			0x47	/* play by MSF address */
#define ATAPI_PLAY_TRACK		0x48	/* play by track number */
#define ATAPI_PAUSE			0x4b	/* pause audio operation */
#define ATAPI_READ_DISK_INFO		0x51	/* get disk info structure */
#define ATAPI_READ_TRACK_INFO		0x52	/* get track info structure */
#define ATAPI_RESERVE_TRACK		0x53	/* reserve track */
#define ATAPI_SEND_OPC_INFO		0x54	/* send OPC structurek */
#define ATAPI_MODE_SELECT_BIG		0x55	/* set device parameters */
#define ATAPI_REPAIR_TRACK		0x58	/* repair track */
#define ATAPI_READ_MASTER_CUE		0x59	/* read master CUE info */
#define ATAPI_MODE_SENSE_BIG		0x5a	/* get device parameters */
#define ATAPI_CLOSE_TRACK		0x5b	/* close track/session */
#define ATAPI_READ_BUFFER_CAPACITY	0x5c	/* get buffer capicity */
#define ATAPI_SEND_CUE_SHEET		0x5d	/* send CUE sheet */
#define ATAPI_BLANK			0xa1	/* blank the media */
#define ATAPI_SEND_KEY			0xa3	/* send DVD key structure */
#define ATAPI_REPORT_KEY		0xa4	/* get DVD key structure */
#define ATAPI_PLAY_12			0xa5	/* play by lba */
#define ATAPI_LOAD_UNLOAD		0xa6	/* changer control command */
#define ATAPI_READ_STRUCTURE		0xad	/* get DVD structure */
#define ATAPI_PLAY_CD			0xb4	/* universal play command */
#define ATAPI_SET_SPEED			0xbb	/* set drive speed */
#define ATAPI_MECH_STATUS		0xbd	/* get changer status */
#define ATAPI_READ_CD			0xbe	/* read data */
#define ATAPI_POLL_DSC			0xff	/* poll DSC status bit */

/* ATAPI request sense structure */   
struct atapi_reqsense {
    u_int8_t	error_code	:7;		/* current or deferred errors */
    u_int8_t	valid		:1;		/* follows ATAPI spec */
    u_int8_t	segment;			/* Segment number */
    u_int8_t	sense_key	:4;		/* sense key */
    u_int8_t	reserved2_4	:1;		/* reserved */
    u_int8_t	ili		:1;		/* incorrect length indicator */
    u_int8_t	eom		:1;		/* end of medium */
    u_int8_t	filemark	:1;		/* filemark */
						/* cmd information */
    u_int32_t	cmd_info __attribute__((packed));
    u_int8_t	sense_length;			/* additional sense len (n-7) */
						/* additional cmd spec info */
    u_int32_t	cmd_specific_info __attribute__((packed));
    u_int8_t	asc;				/* additional sense code */
    u_int8_t	ascq;				/* additional sense code qual */
    u_int8_t	replaceable_unit_code;		/* replaceable unit code */
    u_int8_t	sk_specific	:7;		/* sense key specific */
    u_int8_t	sksv		:1;		/* sense key specific info OK */
    u_int8_t	sk_specific1;			/* sense key specific */
    u_int8_t	sk_specific2;			/* sense key specific */
};  

typedef int atapi_callback_t(struct atapi_request *);

struct atapi_request {
    struct ata_device		*device;	/* ptr to parent softc */
    u_int8_t			ccb[16];	/* command control block */
    int				ccbsize;	/* size of ccb (12 | 16) */
    u_int32_t			bytecount;	/* bytes to transfer */
    u_int32_t			donecount;	/* bytes transferred */
    int				timeout;	/* timeout for this cmd */
    struct callout_handle	timeout_handle; /* handle for untimeout */
    int				retries;	/* retry count */
    int				result;		/* result of this cmd */
    int				error;		/* result translated to errno */
    struct atapi_reqsense	sense;		/* sense data if error */
    int				flags;
#define		ATPR_F_READ		0x0001
#define		ATPR_F_DMA_USED		0x0002
#define		ATPR_F_AT_HEAD		0x0004
#define		ATPR_F_INTERNAL		0x0008
#define		ATPR_F_QUIET		0x0010

    caddr_t			data;		/* pointer to data buf */
    atapi_callback_t		*callback;	/* ptr to callback func */
    void			*driver;	/* driver specific */
    TAILQ_ENTRY(atapi_request)	chain;		/* list management */
};

void atapi_attach(struct ata_device *);
void atapi_detach(struct ata_device *);
void atapi_reinit(struct ata_device *);
void atapi_start(struct ata_device *);
int atapi_transfer(struct atapi_request *);
int atapi_interrupt(struct atapi_request *);
int atapi_queue_cmd(struct ata_device *, int8_t [], caddr_t, int, int, int, atapi_callback_t, void *);
int atapi_test_ready(struct ata_device *);
int atapi_wait_dsc(struct ata_device *, int);
void atapi_request_sense(struct ata_device *, struct atapi_reqsense *);
void atapi_dump(char *, void *, int);
int acdattach(struct ata_device *);
void acddetach(struct ata_device *);
void acd_start(struct ata_device *);
int afdattach(struct ata_device *);
void afddetach(struct ata_device *);
void afd_start(struct ata_device *);
int astattach(struct ata_device *);
void astdetach(struct ata_device *);
void ast_start(struct ata_device *);
