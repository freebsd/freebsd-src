/*-
 * Copyright (c) 1998 - 2004 Søren Schmidt <sos@FreeBSD.org>
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
 * $FreeBSD: src/sys/dev/ata/ata-all.h,v 1.81.2.4 2005/03/01 19:37:41 gad Exp $
 */

/* ATA register defines */
#define ATA_DATA			0x00	/* data register */

#define ATA_ERROR			0x01	/* (R) error register */
#define		ATA_E_ILI		0x01	/* illegal length */
#define		ATA_E_NM		0x02	/* no media */
#define		ATA_E_ABORT		0x04	/* command aborted */
#define		ATA_E_MCR		0x08	/* media change request */
#define		ATA_E_IDNF		0x10	/* ID not found */
#define		ATA_E_MC		0x20	/* media changed */
#define		ATA_E_UNC		0x40	/* uncorrectable data */
#define		ATA_E_ICRC		0x80	/* UDMA crc error */
#define		ATA_E_MASK		0x0f	/* error mask */
#define		ATA_SK_MASK		0xf0	/* sense key mask */
#define		ATA_SK_NO_SENSE		0x00	/* no specific sense key info */
#define		ATA_SK_RECOVERED_ERROR	0x10	/* command OK, data recovered */
#define		ATA_SK_NOT_READY	0x20	/* no access to drive */
#define		ATA_SK_MEDIUM_ERROR	0x30	/* non-recovered data error */
#define		ATA_SK_HARDWARE_ERROR	0x40	/* non-recoverable HW failure */
#define		ATA_SK_ILLEGAL_REQUEST	0x50	/* invalid command param(s) */
#define		ATA_SK_UNIT_ATTENTION	0x60	/* media changed */
#define		ATA_SK_DATA_PROTECT	0x70	/* write protect */
#define		ATA_SK_BLANK_CHECK	0x80	/* blank check */
#define		ATA_SK_VENDOR_SPECIFIC	0x90	/* vendor specific skey */
#define		ATA_SK_COPY_ABORTED	0xa0	/* copy aborted */
#define		ATA_SK_ABORTED_COMMAND	0xb0	/* command aborted, try again */
#define		ATA_SK_EQUAL		0xc0	/* equal */
#define		ATA_SK_VOLUME_OVERFLOW	0xd0	/* volume overflow */
#define		ATA_SK_MISCOMPARE	0xe0	/* data dont match the medium */
#define		ATA_SK_RESERVED		0xf0

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

#define ATA_ALTSTAT			0x08	/* alternate status register */
#define ATA_ALTOFFSET			0x206	/* alternate registers offset */
#define ATA_PCCARD_ALTOFFSET		0x0e	/* do for PCCARD devices */
#define ATA_PC98_ALTOFFSET		0x10c	/* do for PC98 devices */
#define		ATA_A_IDS		0x02	/* disable interrupts */
#define		ATA_A_RESET		0x04	/* RESET controller */
#define		ATA_A_4BIT		0x08	/* 4 head bits */

/* ATAPI misc defines */
#define ATAPI_MAGIC_LSB			0x14
#define ATAPI_MAGIC_MSB			0xeb
#define ATAPI_P_READ			(ATA_S_DRQ | ATA_I_IN)
#define ATAPI_P_WRITE			(ATA_S_DRQ)
#define ATAPI_P_CMDOUT			(ATA_S_DRQ | ATA_I_CMD)
#define ATAPI_P_DONEDRQ			(ATA_S_DRQ | ATA_I_CMD | ATA_I_IN)
#define ATAPI_P_DONE			(ATA_I_CMD | ATA_I_IN)
#define ATAPI_P_ABORT			0

/* misc defines */
#define ATA_PRIMARY			0x1f0
#define ATA_SECONDARY			0x170
#define ATA_PC98_BANK			0x432
#define ATA_IOSIZE			0x08
#define ATA_PC98_IOSIZE			0x10
#define ATA_ALTIOSIZE			0x01
#define ATA_BMIOSIZE			0x08
#define ATA_PC98_BANKIOSIZE		0x01
#define ATA_IOADDR_RID			0
#define ATA_ALTADDR_RID			1
#define ATA_BMADDR_RID			0x20
#define ATA_PC98_ALTADDR_RID		8
#define ATA_PC98_BANKADDR_RID		9

#define ATA_IRQ_RID			0
#define ATA_DEV(device)			((device == ATA_MASTER) ? 0 : 1)

/* busmaster DMA related defines */
#define ATA_DMA_ENTRIES			256
#define ATA_DMA_EOT			0x80000000

#define ATA_BMCMD_PORT			0x09
#define		ATA_BMCMD_START_STOP	0x01
#define		ATA_BMCMD_WRITE_READ	0x08

#define ATA_BMDEVSPEC_0			0x0a
#define ATA_BMSTAT_PORT			0x0b
#define		ATA_BMSTAT_ACTIVE	0x01
#define		ATA_BMSTAT_ERROR	0x02
#define		ATA_BMSTAT_INTERRUPT	0x04
#define		ATA_BMSTAT_MASK		0x07
#define		ATA_BMSTAT_DMA_MASTER	0x20
#define		ATA_BMSTAT_DMA_SLAVE	0x40
#define		ATA_BMSTAT_DMA_SIMPLEX	0x80

#define ATA_BMDEVSPEC_1			0x0c
#define ATA_BMDTP_PORT			0x0d

#define ATA_IDX_ADDR			0x0e
#define ATA_IDX_DATA			0x0f
#define ATA_MAX_RES			0x10

#define ATA_INTR_FLAGS			(INTR_MPSAFE|INTR_TYPE_BIO|INTR_ENTROPY)
#define ATA_OP_CONTINUES		0
#define ATA_OP_FINISHED			1

#define ATA_MAX_28BIT_LBA		268435455

/* ATAPI request sense structure */
struct atapi_sense {
    u_int8_t	error_code	:7;		/* current or deferred errors */
    u_int8_t	valid		:1;		/* follows ATAPI spec */
    u_int8_t	segment;			/* Segment number */
    u_int8_t	sense_key	:4;		/* sense key */
    u_int8_t	reserved2_4	:1;		/* reserved */
    u_int8_t	ili		:1;		/* incorrect length indicator */
    u_int8_t	eom		:1;		/* end of medium */
    u_int8_t	filemark	:1;		/* filemark */
    u_int32_t	cmd_info __packed;		/* cmd information */
    u_int8_t	sense_length;			/* additional sense len (n-7) */
    u_int32_t	cmd_specific_info __packed;	/* additional cmd spec info */
    u_int8_t	asc;				/* additional sense code */
    u_int8_t	ascq;				/* additional sense code qual */
    u_int8_t	replaceable_unit_code;		/* replaceable unit code */
    u_int8_t	sk_specific	:7;		/* sense key specific */
    u_int8_t	sksv		:1;		/* sense key specific info OK */
    u_int8_t	sk_specific1;			/* sense key specific */
    u_int8_t	sk_specific2;			/* sense key specific */
};

struct ata_request {
    struct ata_device		*device;	/* ptr to device softc */
    void			*driver;	/* driver specific */

    union {
	struct {
	    u_int8_t		command;	/* command reg */
	    u_int8_t		feature;	/* feature reg */
	    u_int16_t		count;		/* count reg */
	    u_int64_t		lba;		/* lba reg */
	} ata;
	struct {
	    u_int8_t		ccb[16];	/* ATAPI command block */
	    struct atapi_sense	sense_data;	/* ATAPI request sense data */
	    u_int8_t		sense_key;	/* ATAPI request sense key */
	    u_int8_t		sense_cmd;	/* ATAPI saved command */
	} atapi;
    } u;

    u_int8_t			status;		/* ATA status */
    u_int8_t			error;		/* ATA error */
    u_int8_t			dmastat;	/* DMA status */

    u_int32_t			bytecount;	/* bytes to transfer */
    u_int32_t			transfersize;	/* bytes pr transfer */
    u_int32_t			donecount;	/* bytes transferred */
    caddr_t			data;		/* pointer to data buf */
    int				flags;
#define		ATA_R_CONTROL		0x0001
#define		ATA_R_READ		0x0002
#define		ATA_R_WRITE		0x0004
#define		ATA_R_DMA		0x0008

#define		ATA_R_ATAPI		0x0010
#define		ATA_R_QUIET		0x0020
#define		ATA_R_INTR_SEEN		0x0040
#define		ATA_R_TIMEOUT		0x0080

#define		ATA_R_ORDERED		0x0100
#define		ATA_R_IMMEDIATE		0x0200
#define		ATA_R_REQUEUE		0x0400

#define		ATA_R_DEBUG		0x1000

    void			(*callback)(struct ata_request *request);
    struct sema			done;		/* request done sema */
    int				retries;	/* retry count */
    int				timeout;	/* timeout for this cmd */
    struct callout		callout; 	/* callout management */
    int				result;		/* result error code */
    struct task			task;		/* task management */
    struct bio			*bio;		/* bio for this request */
    TAILQ_ENTRY(ata_request)	sequence;	/* sequence management */
    TAILQ_ENTRY(ata_request)	chain;		/* list management */
};

/* define this for debugging request processing */
#if 0
#define ATA_DEBUG_RQ(request, string) \
    { \
    if (request->flags & ATA_R_DEBUG) \
        ata_prtdev(request->device, "req=%p %s " string "\n", \
                   request, ata_cmd2str(request)); \
    }
#else
#define ATA_DEBUG_RQ(request, string)
#endif


/* structure describing an ATA/ATAPI device */
struct ata_device {
    struct ata_channel		*channel;
    int				unit;		/* unit number */
#define		ATA_MASTER		0x00
#define		ATA_SLAVE		0x10

    char			*name;		/* device name */
    struct ata_params		*param;		/* ata param structure */
    void			*softc;		/* ptr to softc for device */
    void			(*attach)(struct ata_device *atadev);
    void			(*detach)(struct ata_device *atadev);
    void			(*config)(struct ata_device *atadev);
    void			(*start)(struct ata_device *atadev);
    int				flags;
#define		ATA_D_USE_CHS		0x0001
#define		ATA_D_DETACHING		0x0002
#define		ATA_D_MEDIA_CHANGED	0x0004
#define		ATA_D_ENC_PRESENT	0x0008

    int				cmd;		/* last cmd executed */
    int				mode;		/* transfermode */
    void			(*setmode)(struct ata_device *atadev, int mode);
};

/* structure for holding DMA Physical Region Descriptors (PRD) entries */
struct ata_dma_prdentry {
    u_int32_t addr;
    u_int32_t count;
};  

/* structure used by the setprd function */
struct ata_dmasetprd_args {
    void *dmatab;
    int error;
};

/* structure holding DMA related information */
struct ata_dma {
    bus_dma_tag_t		dmatag;		/* parent DMA tag */
    bus_dma_tag_t		cdmatag;	/* control DMA tag */
    bus_dmamap_t		cdmamap;	/* control DMA map */
    bus_dma_tag_t		ddmatag;	/* data DMA tag */
    bus_dmamap_t		ddmamap;	/* data DMA map */
    void			*dmatab;	/* DMA transfer table */
    bus_addr_t			mdmatab;	/* bus address of dmatab */
    bus_dma_tag_t		wdmatag;	/* workspace DMA tag */
    bus_dmamap_t		wdmamap;	/* workspace DMA map */
    u_int8_t			*workspace;	/* workspace */
    bus_addr_t			wdmatab;	/* bus address of dmatab */

    u_int32_t			alignment;	/* DMA engine alignment */
    u_int32_t			boundary;	/* DMA engine boundary */
    u_int32_t			max_iosize;	/* DMA engine max IO size */
    u_int32_t			cur_iosize;	/* DMA engine current IO size */
    int				flags;
#define ATA_DMA_READ			0x01	/* transaction is a read */
#define ATA_DMA_LOADED			0x02	/* DMA tables etc loaded */
#define ATA_DMA_ACTIVE			0x04	/* DMA transfer in progress */

    void (*alloc)(struct ata_channel *ch);
    void (*free)(struct ata_channel *ch);
    void (*setprd)(void *xsc, bus_dma_segment_t *segs, int nsegs, int error);
    int (*load)(struct ata_device *atadev, caddr_t data, int32_t count,int dir);
    int (*unload)(struct ata_channel *ch);
    int (*start)(struct ata_channel *ch);
    int (*stop)(struct ata_channel *ch);
};

/* structure holding lowlevel functions */
struct ata_lowlevel {
    int (*begin_transaction)(struct ata_request *request);
    int (*end_transaction)(struct ata_request *request);
    void (*interrupt)(void *channel);
    void (*reset)(struct ata_channel *ch);
    int (*command)(struct ata_device *atadev, u_int8_t command, u_int64_t lba, u_int16_t count, u_int16_t feature);
};

/* structure holding resources for an ATA channel */
struct ata_resource {
    struct resource		*res;
    int				offset;
};

/* structure describing an ATA channel */
struct ata_channel {
    struct device		*dev;		/* device handle */
    int				unit;		/* channel number */
    struct ata_resource		r_io[ATA_MAX_RES];/* I/O resources */
    struct resource		*r_irq;		/* interrupt of this channel */
    void			*ih;		/* interrupt handle */
    struct ata_lowlevel		hw;		/* lowlevel HW functions */
    struct ata_dma		*dma;		/* DMA data / functions */
    int				flags;		/* channel flags */
#define		ATA_NO_SLAVE		0x01
#define		ATA_USE_16BIT		0x02
#define		ATA_USE_PC98GEOM	0x04
#define		ATA_ATAPI_DMA_RO	0x08
#define		ATA_48BIT_ACTIVE	0x10
#define		ATA_IMMEDIATE_MODE	0x20
#define		ATA_HWGONE		0x40

    struct ata_device		device[2];	/* devices on this channel */
#define		MASTER			0x00
#define		SLAVE			0x01

    int				devices;	/* what is present */
#define		ATA_ATA_MASTER		0x01
#define		ATA_ATA_SLAVE		0x02
#define		ATA_ATAPI_MASTER	0x04
#define		ATA_ATAPI_SLAVE		0x08

    struct mtx			state_mtx;	/* state lock */
    int				state;		/* ATA channel state */
#define		ATA_IDLE		0x0000
#define		ATA_ACTIVE		0x0001
#define		ATA_INTERRUPT		0x0002
#define		ATA_TIMEOUT		0x0004

    void			(*reset)(struct ata_channel *);
    int				(*locking)(struct ata_channel *, int);
#define		ATA_LF_LOCK		0x0001
#define		ATA_LF_UNLOCK		0x0002
#define		ATA_LF_WHICH		0x0004

    struct mtx			queue_mtx;	/* queue lock */
    TAILQ_HEAD(, ata_request)	ata_queue;	/* head of ATA queue */
    struct ata_request		*running;	/* currently running request */
};

/* disk bay/enclosure related */
#define		ATA_LED_OFF		0x00
#define		ATA_LED_RED		0x01
#define		ATA_LED_GREEN		0x02
#define		ATA_LED_ORANGE		0x03
#define		ATA_LED_MASK		0x03

/* externs */
extern devclass_t ata_devclass;
extern int ata_wc;
 
/* public prototypes */
/* ata-all.c: */
int ata_probe(device_t dev);
int ata_attach(device_t dev);
int ata_detach(device_t dev);
int ata_suspend(device_t dev);
int ata_resume(device_t dev);
void ata_udelay(int interval);
int ata_printf(struct ata_channel *ch, int device, const char *fmt, ...) __printflike(3, 4);
int ata_prtdev(struct ata_device *atadev, const char *fmt, ...) __printflike(2, 3);
void ata_set_name(struct ata_device *atadev, char *name, int lun);
void ata_free_name(struct ata_device *atadev);
int ata_get_lun(u_int32_t *map);
int ata_test_lun(u_int32_t *map, int lun);
void ata_free_lun(u_int32_t *map, int lun);
char *ata_mode2str(int mode);
int ata_pmode(struct ata_params *ap);
int ata_wmode(struct ata_params *ap);
int ata_umode(struct ata_params *ap);
int ata_limit_mode(struct ata_device *atadev, int mode, int maxmode);

/* ata-queue.c: */
int ata_reinit(struct ata_channel *ch);
void ata_start(struct ata_channel *ch);
int ata_controlcmd(struct ata_device *atadev, u_int8_t command, u_int16_t feature, u_int64_t lba, u_int16_t count);
int ata_atapicmd(struct ata_device *atadev, u_int8_t *ccb, caddr_t data, int count, int flags, int timeout);
void ata_queue_request(struct ata_request *request);
void ata_finish(struct ata_request *request);
void ata_catch_inflight(struct ata_channel *ch);
void ata_fail_requests(struct ata_channel *ch, struct ata_device *device);
char *ata_cmd2str(struct ata_request *request);

/* ata-lowlevel.c: */
void ata_generic_hw(struct ata_channel *ch);
int ata_generic_command(struct ata_device *atadev, u_int8_t command, u_int64_t lba, u_int16_t count, u_int16_t feature);

/* subdrivers */
void ad_attach(struct ata_device *atadev);
void acd_attach(struct ata_device *atadev);
void afd_attach(struct ata_device *atadev);
void ast_attach(struct ata_device *atadev);
void atapi_cam_attach_bus(struct ata_channel *ch);
void atapi_cam_detach_bus(struct ata_channel *ch);
void atapi_cam_reinit_bus(struct ata_channel *ch);

/* macros for alloc/free of ata_requests */
extern uma_zone_t ata_zone;
#define ata_alloc_request() uma_zalloc(ata_zone, M_NOWAIT | M_ZERO)
#define ata_free_request(request) uma_zfree(ata_zone, request)

/* macros to hide busspace uglyness */
#define ATA_INB(res, offset) \
	bus_space_read_1(rman_get_bustag((res)), \
			 rman_get_bushandle((res)), (offset))

#define ATA_INW(res, offset) \
	bus_space_read_2(rman_get_bustag((res)), \
			 rman_get_bushandle((res)), (offset))
#define ATA_INL(res, offset) \
	bus_space_read_4(rman_get_bustag((res)), \
			 rman_get_bushandle((res)), (offset))
#define ATA_INSW(res, offset, addr, count) \
	bus_space_read_multi_2(rman_get_bustag((res)), \
			       rman_get_bushandle((res)), \
			       (offset), (addr), (count))
#define ATA_INSW_STRM(res, offset, addr, count) \
	bus_space_read_multi_stream_2(rman_get_bustag((res)), \
				      rman_get_bushandle((res)), \
				      (offset), (addr), (count))
#define ATA_INSL(res, offset, addr, count) \
	bus_space_read_multi_4(rman_get_bustag((res)), \
			       rman_get_bushandle((res)), \
			       (offset), (addr), (count))
#define ATA_INSL_STRM(res, offset, addr, count) \
	bus_space_read_multi_stream_4(rman_get_bustag((res)), \
				      rman_get_bushandle((res)), \
				      (offset), (addr), (count))
#define ATA_OUTB(res, offset, value) \
	bus_space_write_1(rman_get_bustag((res)), \
			  rman_get_bushandle((res)), (offset), (value))
#define ATA_OUTW(res, offset, value) \
	bus_space_write_2(rman_get_bustag((res)), \
			  rman_get_bushandle((res)), (offset), (value))
#define ATA_OUTL(res, offset, value) \
	bus_space_write_4(rman_get_bustag((res)), \
			  rman_get_bushandle((res)), (offset), (value))
#define ATA_OUTSW(res, offset, addr, count) \
	bus_space_write_multi_2(rman_get_bustag((res)), \
				rman_get_bushandle((res)), \
				(offset), (addr), (count))
#define ATA_OUTSW_STRM(res, offset, addr, count) \
	bus_space_write_multi_stream_2(rman_get_bustag((res)), \
				       rman_get_bushandle((res)), \
				       (offset), (addr), (count))
#define ATA_OUTSL(res, offset, addr, count) \
	bus_space_write_multi_4(rman_get_bustag((res)), \
				rman_get_bushandle((res)), \
				(offset), (addr), (count))
#define ATA_OUTSL_STRM(res, offset, addr, count) \
	bus_space_write_multi_stream_4(rman_get_bustag((res)), \
				       rman_get_bushandle((res)), \
				       (offset), (addr), (count))

#define ATA_IDX_INB(ch, idx) \
	ATA_INB(ch->r_io[idx].res, ch->r_io[idx].offset)

#define ATA_IDX_INW(ch, idx) \
	ATA_INW(ch->r_io[idx].res, ch->r_io[idx].offset)

#define ATA_IDX_INL(ch, idx) \
	ATA_INL(ch->r_io[idx].res, ch->r_io[idx].offset)

#define ATA_IDX_INSW(ch, idx, addr, count) \
	ATA_INSW(ch->r_io[idx].res, ch->r_io[idx].offset, addr, count)

#define ATA_IDX_INSW_STRM(ch, idx, addr, count) \
	ATA_INSW_STRM(ch->r_io[idx].res, ch->r_io[idx].offset, addr, count)

#define ATA_IDX_INSL(ch, idx, addr, count) \
	ATA_INSL(ch->r_io[idx].res, ch->r_io[idx].offset, addr, count)

#define ATA_IDX_INSL_STRM(ch, idx, addr, count) \
	ATA_INSL_STRM(ch->r_io[idx].res, ch->r_io[idx].offset, addr, count)

#define ATA_IDX_OUTB(ch, idx, value) \
	ATA_OUTB(ch->r_io[idx].res, ch->r_io[idx].offset, value)

#define ATA_IDX_OUTW(ch, idx, value) \
	ATA_OUTW(ch->r_io[idx].res, ch->r_io[idx].offset, value)

#define ATA_IDX_OUTL(ch, idx, value) \
	ATA_OUTL(ch->r_io[idx].res, ch->r_io[idx].offset, value)

#define ATA_IDX_OUTSW(ch, idx, addr, count) \
	ATA_OUTSW(ch->r_io[idx].res, ch->r_io[idx].offset, addr, count)

#define ATA_IDX_OUTSW_STRM(ch, idx, addr, count) \
	ATA_OUTSW_STRM(ch->r_io[idx].res, ch->r_io[idx].offset, addr, count)

#define ATA_IDX_OUTSL(ch, idx, addr, count) \
	ATA_OUTSL(ch->r_io[idx].res, ch->r_io[idx].offset, addr, count)

#define ATA_IDX_OUTSL_STRM(ch, idx, addr, count) \
	ATA_OUTSL_STRM(ch->r_io[idx].res, ch->r_io[idx].offset, addr, count)
