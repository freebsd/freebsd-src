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
#define		ATA_D_LBA		0x40	/* use LBA adressing */
#define		ATA_D_IBM		0xa0	/* 512 byte sectors, ECC */

#define ATA_CMD				0x07	/* command register */
#define		ATA_C_NOP		0x00	/* NOP command */
#define		    ATA_C_F_FLUSHQUEUE	0x00	/* flush queued cmd's */
#define		    ATA_C_F_AUTOPOLL	0x01	/* start autopoll function */
#define		ATA_C_ATAPI_RESET	0x08	/* reset ATAPI device */
#define		ATA_C_READ		0x20	/* read command */
#define		ATA_C_WRITE		0x30	/* write command */
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
#define		ATA_C_FLUSHCACHE	0xe7	/* flush cache to disk */
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
    u_int8_t    cmdsize         :2;             /* packet command size */
#define         ATAPI_PSIZE_12          0       /* 12 bytes */
#define         ATAPI_PSIZE_16          1       /* 16 bytes */

    u_int8_t                    :3;
    u_int8_t    drqtype         :2;             /* DRQ type */
#define         ATAPI_DRQT_MPROC        0       /* cpu    3 ms delay */
#define         ATAPI_DRQT_INTR         1       /* intr  10 ms delay */
#define         ATAPI_DRQT_ACCEL        2       /* accel 50 us delay */

    u_int8_t    removable       :1;             /* device is removable */
    u_int8_t    device_type     :5;             /* device type */
#define         ATAPI_TYPE_DIRECT       0       /* disk/floppy */
#define         ATAPI_TYPE_TAPE         1       /* streaming tape */
#define         ATAPI_TYPE_CDROM        5       /* CD-ROM device */
#define         ATAPI_TYPE_OPTICAL      7       /* optical disk */

    u_int8_t                    :1;
    u_int8_t    proto           :2;             /* command protocol */
#define         ATAPI_PROTO_ATAPI       2

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
#define		ATA_FLAG_54_58	      1		/* words 54-58 valid */
#define		ATA_FLAG_64_70	      2		/* words 64-70 valid */
#define		ATA_FLAG_88	      4		/* word 88 valid */

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
    u_int16_t	reserved161[84];
    u_int16_t	integrity;
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
#define		ATA_UDMA2		0x42
#define		ATA_UDMA4		0x44
#define		ATA_UDMA5		0x45

    int				flags;		/* controller flags */
#define		ATA_DMA_ACTIVE		0x01
#define		ATA_ATAPI_DMA_RO	0x02
#define		ATA_USE_16BIT		0x04
#define		ATA_ATTACHED		0x08
#define		ATA_QUEUED		0x10
#define		ATA_RAID		0x20

    int				devices;	/* what is present */
#define		ATA_ATA_MASTER		0x01
#define		ATA_ATA_SLAVE		0x02
#define		ATA_ATAPI_MASTER	0x04
#define		ATA_ATAPI_SLAVE		0x08

    u_int8_t			status;		/* last controller status */
    u_int8_t			error;		/* last controller error */
    int				active;		/* active processing request */
#define		ATA_IDLE		0x0
#define		ATA_IMMEDIATE		0x1
#define		ATA_WAIT_INTR		0x2
#define		ATA_WAIT_READY		0x3
#define		ATA_ACTIVE		0x4
#define		ATA_ACTIVE_ATA		0x5
#define		ATA_ACTIVE_ATAPI	0x6
#define		ATA_REINITING		0x7

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
int ata_command(struct ata_softc *, int, u_int8_t, u_int16_t, u_int8_t, u_int8_t, u_int8_t, u_int8_t, int);
int ata_printf(struct ata_softc *, int, const char *, ...) __printflike(3, 4);
int ata_get_lun(u_int32_t *);
void ata_free_lun(u_int32_t *, int);
char *ata_mode2str(int);
int ata_pio2mode(int);
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
