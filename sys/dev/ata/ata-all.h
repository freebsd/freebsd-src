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

/* ATA register defines */
#define ATA_DATA			0x00	/* data register */
#define ATA_ERROR			0x01	/* (R) error register */
#define		ATA_E_NM		0x02	/* no media */
#define		ATA_E_ABORT		0x04	/* command aborted */
#define		ATA_E_MCR		0x08	/* media change request */
#define		ATA_E_IDNF		0x10	/* ID not found */
#define		ATA_E_MC		0x20	/* media changed */
#define		ATA_E_UNC		0x40	/* uncorrectable data */
#define		ATA_E_ICRC		0x80	/* UDMA crc error */

#define ATA_FEATURE			0x01	/* (W) feature register */
#define		ATA_F_DMA		0x01	/* enable DMA */
#define		ATA_F_OVL		0x02	/* enable overlap */

#define ATA_COUNT			0x02	/* (W) sector count */
#define ATA_IREASON			0x02	/* (R) interrupt reason */
#define		ATA_I_CMD		0x01	/* cmd (1) | data (0) */
#define		ATA_I_IN		0x02	/* read (1) | write (0) */
#define		ATA_I_RELEASE		0x04	/* released bus (1) */
#define		ATA_I_TAGMASK		0xf8	/* tag mask */

#define ATA_SECTOR			0x03	/* sector # */
#define ATA_CYL_LSB			0x04	/* cylinder# LSB */
#define ATA_CYL_MSB			0x05	/* cylinder# MSB */
#define ATA_DRIVE			0x06	/* Sector/Drive/Head register */
#define		ATA_D_LBA		0x40	/* use LBA addressing */
#define		ATA_D_IBM		0xa0	/* 512 byte sectors, ECC */

#define ATA_CMD				0x07	/* command register */
#define		ATA_C_NOP		0x00	/* NOP command */
#define		    ATA_C_F_FLUSHQUEUE	0x00	/* flush queued cmd's */
#define		    ATA_C_F_AUTOPOLL	0x01	/* start autopoll function */
#define		ATA_C_ATAPI_RESET	0x08	/* reset ATAPI device */
#define		ATA_C_READ		0x20	/* read command */
#define		ATA_C_READ48		0x24	/* read command */
#define		ATA_C_READ_DMA48	0x25	/* read w/DMA command */
#define		ATA_C_READ_DMA_QUEUED48	0x26	/* read w/DMS QUEUED command */
#define		ATA_C_READ_MUL48	0x29	/* read multi command */
#define		ATA_C_WRITE		0x30	/* write command */
#define		ATA_C_WRITE48		0x34	/* write command */
#define		ATA_C_WRITE_DMA48	0x35	/* write w/DMA command */
#define		ATA_C_WRITE_DMA_QUEUED48 0x36	/* write w/DMA QUEUED command */
#define		ATA_C_WRITE_MUL48	0x39	/* write multi command */
#define		ATA_C_PACKET_CMD	0xa0	/* packet command */
#define		ATA_C_ATAPI_IDENTIFY	0xa1	/* get ATAPI params*/
#define		ATA_C_SERVICE		0xa2	/* service command */
#define		ATA_C_READ_MUL		0xc4	/* read multi command */
#define		ATA_C_WRITE_MUL		0xc5	/* write multi command */
#define		ATA_C_SET_MULTI		0xc6	/* set multi size command */
#define		ATA_C_READ_DMA_QUEUED	0xc7	/* read w/DMA QUEUED command */
#define		ATA_C_READ_DMA		0xc8	/* read w/DMA command */
#define		ATA_C_WRITE_DMA		0xca	/* write w/DMA command */
#define		ATA_C_WRITE_DMA_QUEUED	0xcc	/* write w/DMA QUEUED command */
#define		ATA_C_SLEEP		0xe6	/* sleep command */
#define		ATA_C_FLUSHCACHE	0xe7	/* flush cache to disk */
#define		ATA_C_FLUSHCACHE48	0xea	/* flush cache to disk */
#define		ATA_C_ATA_IDENTIFY	0xec	/* get ATA params */
#define		ATA_C_SETFEATURES	0xef	/* features command */
#define		    ATA_C_F_SETXFER	0x03	/* set transfer mode */
#define		    ATA_C_F_ENAB_WCACHE	0x02	/* enable write cache */
#define		    ATA_C_F_DIS_WCACHE	0x82	/* disable write cache */
#define		    ATA_C_F_ENAB_RCACHE	0xaa	/* enable readahead cache */
#define		    ATA_C_F_DIS_RCACHE	0x55	/* disable readahead cache */
#define		    ATA_C_F_ENAB_RELIRQ	0x5d	/* enable release interrupt */
#define		    ATA_C_F_DIS_RELIRQ	0xdd	/* disable release interrupt */
#define		    ATA_C_F_ENAB_SRVIRQ	0x5e	/* enable service interrupt */
#define		    ATA_C_F_DIS_SRVIRQ	0xde	/* disable service interrupt */

#define ATA_STATUS			0x07	/* status register */
#define		ATA_S_ERROR		0x01	/* error */
#define		ATA_S_INDEX		0x02	/* index */
#define		ATA_S_CORR		0x04	/* data corrected */
#define		ATA_S_DRQ		0x08	/* data request */
#define		ATA_S_DSC		0x10	/* drive seek completed */
#define		ATA_S_SERVICE		0x10	/* drive needs service */
#define		ATA_S_DWF		0x20	/* drive write fault */
#define		ATA_S_DMA		0x20	/* DMA ready */
#define		ATA_S_READY		0x40	/* drive ready */
#define		ATA_S_BUSY		0x80	/* busy */

#define ATA_ALTOFFSET			0x206	/* alternate registers offset */
#define ATA_PCCARD_ALTOFFSET		0x0e	/* do for PCCARD devices */
#define ATA_ALTIOSIZE			0x01	/* alternate registers size */
#define		ATA_A_IDS		0x02	/* disable interrupts */
#define		ATA_A_RESET		0x04	/* RESET controller */
#define		ATA_A_4BIT		0x08	/* 4 head bits */

/* misc defines */
#define ATA_MASTER			0x00
#define ATA_SLAVE			0x10
#define ATA_IOSIZE			0x08
#define ATA_OP_FINISHED			0x00
#define ATA_OP_CONTINUES		0x01
#define ATA_DEV(device)			((device == ATA_MASTER) ? 0 : 1)
#define ATA_PARAM(scp, device)		(scp->dev_param[ATA_DEV(device)])

/* busmaster DMA related defines */
#define ATA_BM_OFFSET1			0x08
#define ATA_DMA_ENTRIES			256
#define ATA_DMA_EOT			0x80000000

#define ATA_BMCMD_PORT			0x00
#define ATA_BMCMD_START_STOP		0x01
#define ATA_BMCMD_WRITE_READ		0x08

#define ATA_BMSTAT_PORT			0x02
#define ATA_BMSTAT_ACTIVE		0x01
#define ATA_BMSTAT_ERROR		0x02
#define ATA_BMSTAT_INTERRUPT		0x04
#define ATA_BMSTAT_MASK			0x07
#define ATA_BMSTAT_DMA_MASTER		0x20
#define ATA_BMSTAT_DMA_SLAVE		0x40
#define ATA_BMSTAT_DMA_SIMPLEX		0x80

#define ATA_BMDTP_PORT			0x04

#define ATA_BMIOSIZE			0x20

/* structure for holding DMA address data */
struct ata_dmaentry {
	u_int32_t base;
	u_int32_t count;
};  

/* ATA/ATAPI device parameter information */
struct ata_params {
/*000*/	u_int16_t	packet_size	:2;	/* packet command size */
#define ATAPI_PSIZE_12			0	/* 12 bytes */
#define ATAPI_PSIZE_16			1	/* 16 bytes */

    	u_int16_t	incomplete	:1;
    	u_int16_t			:2;
    	u_int16_t	drq_type	:2;	/* DRQ type */
#define ATAPI_DRQT_MPROC		0	/* cpu	  3 ms delay */
#define ATAPI_DRQT_INTR			1	/* intr	 10 ms delay */
#define ATAPI_DRQT_ACCEL		2	/* accel 50 us delay */

    	u_int16_t	removable	:1;	/* device is removable */
    	u_int16_t	type		:5;	/* device type */
#define ATAPI_TYPE_DIRECT		0	/* disk/floppy */
#define ATAPI_TYPE_TAPE			1	/* streaming tape */
#define ATAPI_TYPE_CDROM		5	/* CD-ROM device */
#define ATAPI_TYPE_OPTICAL		7	/* optical disk */

    	u_int16_t			:2;
    	u_int16_t	cmd_protocol	:1;	/* command protocol */
#define ATA_PROTO_ATA			0
#define ATA_PROTO_ATAPI			1

/*001*/	u_int16_t	cylinders;		/* # of cylinders */
	u_int16_t	reserved2;
/*003*/	u_int16_t	heads;			/* # heads */
	u_int16_t	obsolete4;
	u_int16_t	obsolete5;
/*006*/	u_int16_t	sectors;		/* # sectors/track */
/*007*/	u_int16_t	vendor7[3];
/*010*/	u_int8_t	serial[20];		/* serial number */
	u_int16_t	retired20;
	u_int16_t	retired21;
	u_int16_t	obsolete22;
/*023*/	u_int8_t	revision[8];		/* firmware revision */
/*027*/	u_int8_t	model[40];		/* model name */
/*047*/	u_int16_t	sectors_intr:8;		/* sectors per interrupt */
	u_int16_t	:8;

/*048*/	u_int16_t	usedmovsd;		/* double word read/write? */
/*049*/	u_int16_t	retired49:8;
	u_int16_t	support_dma	:1;	/* DMA supported */
	u_int16_t	support_lba	:1;	/* LBA supported */
	u_int16_t	disable_iordy	:1;	/* IORDY may be disabled */
	u_int16_t	support_iordy	:1;	/* IORDY supported */
	u_int16_t	softreset	:1;	/* needs softreset when busy */
	u_int16_t	stdby_ovlap	:1;	/* standby/overlap supported */
	u_int16_t	support_queueing:1;	/* supports queuing overlap */
	u_int16_t	support_idma	:1;	/* interleaved DMA supported */

/*050*/	u_int16_t	device_stdby_min:1;
	u_int16_t	:13;
	u_int16_t	capability_one:1;
	u_int16_t	capability_zero:1;

/*051*/	u_int16_t	vendor51:8;
	u_int16_t	retired_piomode:8;	/* PIO modes 0-2 */
/*052*/	u_int16_t	vendor52:8;
	u_int16_t	retired_dmamode:8;	/* DMA modes, not ATA-3 */
/*053*/	u_int16_t	atavalid;		/* fields valid */
#define ATA_FLAG_54_58			1	/* words 54-58 valid */
#define ATA_FLAG_64_70			2	/* words 64-70 valid */
#define ATA_FLAG_88			4	/* word 88 valid */

	u_int16_t	obsolete54[5];
/*059*/	u_int16_t	multi_count:8;
	u_int16_t	multi_valid:1;
	u_int16_t	:7;

/*060*/	u_int32_t	lba_size;	
	u_int16_t	obsolete62;
/*063*/	u_int16_t	mwdmamodes;		/* multiword DMA modes */ 
/*064*/	u_int16_t	apiomodes;		/* advanced PIO modes */ 

/*065*/	u_int16_t	mwdmamin;		/* min. M/W DMA time/word ns */
/*066*/	u_int16_t	mwdmarec;		/* rec. M/W DMA time ns */
/*067*/	u_int16_t	pioblind;		/* min. PIO cycle w/o flow */
/*068*/	u_int16_t	pioiordy;		/* min. PIO cycle IORDY flow */
	u_int16_t	reserved69;
	u_int16_t	reserved70;
/*071*/	u_int16_t	rlsovlap;		/* rel time (us) for overlap */
/*072*/	u_int16_t	rlsservice;		/* rel time (us) for service */
	u_int16_t	reserved73;
	u_int16_t	reserved74;

/*075*/	u_int16_t	queuelen:5;
	u_int16_t	:11;

	u_int16_t	reserved76;
	u_int16_t	reserved77;
	u_int16_t	reserved78;
	u_int16_t	reserved79;
/*080*/	u_int16_t	version_major;
/*081*/	u_int16_t	version_minor;
	struct {
/*082/085*/ u_int16_t	smart:1;
	    u_int16_t	security:1;
	    u_int16_t	removable:1;
	    u_int16_t	power_mngt:1;
	    u_int16_t	packet:1;
	    u_int16_t	write_cache:1;
	    u_int16_t	look_ahead:1;
	    u_int16_t	release_irq:1;
	    u_int16_t	service_irq:1;
	    u_int16_t	reset:1;
	    u_int16_t	protected:1;
	    u_int16_t	:1;
	    u_int16_t	write_buffer:1;
	    u_int16_t	read_buffer:1;
	    u_int16_t	nop:1;
	    u_int16_t	:1;

/*083/086*/ u_int16_t	microcode:1;
	    u_int16_t	queued:1;
	    u_int16_t	cfa:1;
	    u_int16_t	apm:1;
	    u_int16_t	notify:1;
	    u_int16_t	standby:1;
	    u_int16_t	spinup:1;
	    u_int16_t	:1;
	    u_int16_t	max_security:1;
	    u_int16_t	auto_acoustic:1;
	    u_int16_t	address48:1;
	    u_int16_t	config_overlay:1;
	    u_int16_t	flush_cache:1;
	    u_int16_t	flush_cache48:1;
	    u_int16_t	support_one:1;
	    u_int16_t	support_zero:1;

/*084/087*/ u_int16_t	smart_error_log:1;
	    u_int16_t	smart_self_test:1;
	    u_int16_t	media_serial_no:1;
	    u_int16_t	media_card_pass:1;
	    u_int16_t	streaming:1;
	    u_int16_t	logging:1;
	    u_int16_t	:8;
	    u_int16_t	extended_one:1;
	    u_int16_t	extended_zero:1;
	} support, enabled;

/*088*/	u_int16_t	udmamodes;		/* UltraDMA modes */
/*089*/	u_int16_t	erase_time;
/*090*/	u_int16_t	enhanced_erase_time;
/*091*/	u_int16_t	apm_value;
/*092*/	u_int16_t	master_passwd_revision;

/*093*/	u_int16_t	hwres_master	:8;
	u_int16_t	hwres_slave	:5;
	u_int16_t	hwres_cblid	:1;
	u_int16_t	hwres_valid:2;

/*094*/	u_int16_t	current_acoustic:8;
	u_int16_t	vendor_acoustic:8;

/*095*/	u_int16_t	stream_min_req_size;
/*096*/	u_int16_t	stream_transfer_time;
/*097*/	u_int16_t	stream_access_latency;
/*098*/	u_int32_t	stream_granularity;
/*100*/	u_int64_t	lba_size48;
	u_int16_t	reserved104[23];
/*127*/	u_int16_t	removable_status;
/*128*/	u_int16_t	security_status;
	u_int16_t	reserved129[31];
/*160*/	u_int16_t	cfa_powermode1;
	u_int16_t	reserved161[14];
/*176*/	u_int16_t	media_serial[30];
	u_int16_t	reserved206[49];
/*255*/	u_int16_t	integrity;
};

/* structure describing an ATA device */
struct ata_softc {
    struct device		*dev;		/* device handle */
    int				channel;	/* channel on this controller */
    struct resource		*r_io;		/* io addr resource handle */
    struct resource		*r_altio;	/* altio addr resource handle */
    struct resource		*r_bmio;	/* bmio addr resource handle */
    struct resource		*r_irq;		/* interrupt of this channel */
    void			*ih;		/* interrupt handle */
    u_int32_t			ioaddr;		/* physical port addr */
    u_int32_t			altioaddr;	/* physical alt port addr */
    u_int32_t			bmaddr;		/* physical bus master port */
    u_int32_t			chiptype;	/* pciid of controller chip */
    u_int32_t			alignment;	/* dma engine min alignment */
    struct ata_params		*dev_param[2];	/* ptr to devices params */
    void			*dev_softc[2];	/* ptr to devices softc's */
    int 			mode[2];	/* transfer mode for devices */
#define 	ATA_PIO			0x00
#define 	ATA_PIO0		0x08
#define		ATA_PIO1		0x09
#define		ATA_PIO2		0x0a
#define		ATA_PIO3		0x0b
#define		ATA_PIO4		0x0c
#define		ATA_DMA			0x10
#define		ATA_WDMA2		0x22
#define		ATA_UDMA		0x40
#define		ATA_UDMA2		0x42
#define		ATA_UDMA4		0x44
#define		ATA_UDMA5		0x45
#define		ATA_UDMA6		0x46

    int				flags;		/* controller flags */
#define		ATA_DMA_ACTIVE		0x01
#define		ATA_ATAPI_DMA_RO	0x02
#define		ATA_USE_16BIT		0x04
#define		ATA_NO_SLAVE		0x08
#define		ATA_ATTACHED		0x10
#define		ATA_QUEUED		0x20

    int				devices;	/* what is present */
#define		ATA_ATA_MASTER		0x01
#define		ATA_ATA_SLAVE		0x02
#define		ATA_ATAPI_MASTER	0x04
#define		ATA_ATAPI_SLAVE		0x08

    u_int8_t			status;		/* last controller status */
    u_int8_t			error;		/* last controller error */
    int				active;		/* active processing request */
#define		ATA_IDLE		0x0000
#define		ATA_IMMEDIATE		0x0001
#define		ATA_WAIT_INTR		0x0002
#define		ATA_WAIT_READY		0x0004
#define		ATA_WAIT_MASK		0x0007
#define		ATA_USE_CHS		0x0008
#define		ATA_ACTIVE		0x0010
#define		ATA_ACTIVE_ATA		0x0020
#define		ATA_ACTIVE_ATAPI	0x0040
#define		ATA_REINITING		0x0080


    TAILQ_HEAD(, ad_request)	ata_queue;	/* head of ATA queue */
    TAILQ_HEAD(, atapi_request) atapi_queue;	/* head of ATAPI queue */
    void			*running;	/* currently running request */
};

/* externs */
extern devclass_t ata_devclass;
 
/* public prototypes */
void ata_start(struct ata_softc *);
void ata_reset(struct ata_softc *, int *);
int ata_reinit(struct ata_softc *);
int ata_wait(struct ata_softc *, int, u_int8_t);
int ata_command(struct ata_softc *, int, u_int8_t, u_int64_t, u_int16_t, u_int8_t, int);
int ata_printf(struct ata_softc *, int, const char *, ...) __printflike(3, 4);
int ata_get_lun(u_int32_t *);
void ata_free_lun(u_int32_t *, int);
char *ata_mode2str(int);
int ata_pmode(struct ata_params *);
int ata_wmode(struct ata_params *);
int ata_umode(struct ata_params *);
#if NPCI > 0
int ata_find_dev(device_t, u_int32_t, u_int32_t);
#endif

void *ata_dmaalloc(struct ata_softc *, int);
void ata_dmainit(struct ata_softc *, int, int, int, int);
int ata_dmasetup(struct ata_softc *, int, struct ata_dmaentry *, caddr_t, int);
void ata_dmastart(struct ata_softc *, int, struct ata_dmaentry *, int);
int ata_dmastatus(struct ata_softc *);
int ata_dmadone(struct ata_softc *);
