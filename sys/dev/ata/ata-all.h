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
#define		ATA_C_READ_DMA_QUEUED48 0x26	/* read w/DMA QUEUED command */
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
#define		    ATA_C_F_ENAB_WCACHE 0x02	/* enable write cache */
#define		    ATA_C_F_DIS_WCACHE	0x82	/* disable write cache */
#define		    ATA_C_F_ENAB_RCACHE 0xaa	/* enable readahead cache */
#define		    ATA_C_F_DIS_RCACHE	0x55	/* disable readahead cache */
#define		    ATA_C_F_ENAB_RELIRQ 0x5d	/* enable release interrupt */
#define		    ATA_C_F_DIS_RELIRQ	0xdd	/* disable release interrupt */
#define		    ATA_C_F_ENAB_SRVIRQ 0x5e	/* enable service interrupt */
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

#define ATA_ALTSTAT			0x00	/* alternate status register */
#define ATA_ALTOFFSET			0x206	/* alternate registers offset */
#define ATA_PCCARD_ALTOFFSET		0x0e	/* do for PCCARD devices */
#define		ATA_A_IDS		0x02	/* disable interrupts */
#define		ATA_A_RESET		0x04	/* RESET controller */
#define		ATA_A_4BIT		0x08	/* 4 head bits */

/* misc defines */
#define ATA_PRIMARY			0x1f0
#define ATA_SECONDARY			0x170
#define ATA_IOSIZE			0x08
#define ATA_ALTIOSIZE			0x01
#define ATA_BMIOSIZE			0x08
#define ATA_OP_FINISHED			0x00
#define ATA_OP_CONTINUES		0x01
#define ATA_IOADDR_RID			0
#define ATA_ALTADDR_RID			1
#define ATA_BMADDR_RID			2
#define ATA_IRQ_RID			0
#define ATA_DEV(device)			((device == ATA_MASTER) ? 0 : 1)

/* busmaster DMA related defines */
#define ATA_DMA_ENTRIES			256
#define ATA_DMA_EOT			0x80000000

#define ATA_BMCMD_PORT			0x00
#define		ATA_BMCMD_START_STOP	0x01
#define		ATA_BMCMD_WRITE_READ	0x08

#define ATA_BMDEVSPEC_0			0x01

#define ATA_BMSTAT_PORT			0x02
#define		ATA_BMSTAT_ACTIVE	0x01
#define		ATA_BMSTAT_ERROR	0x02
#define		ATA_BMSTAT_INTERRUPT	0x04
#define		ATA_BMSTAT_MASK		0x07
#define		ATA_BMSTAT_DMA_MASTER	0x20
#define		ATA_BMSTAT_DMA_SLAVE	0x40
#define		ATA_BMSTAT_DMA_SIMPLEX	0x80

#define ATA_BMDEVSPEC_1			0x03
#define ATA_BMDTP_PORT			0x04

/* structure for holding DMA address data */
struct ata_dmaentry {
    u_int32_t base;
    u_int32_t count;
};  

/* structure describing an ATA/ATAPI device */
struct ata_device {
    struct ata_channel		*channel;
    int				unit;		/* unit number */
#define ATA_MASTER			0x00
#define ATA_SLAVE			0x10

    char			*name;		/* device name */
    struct ata_params		*param;		/* ata param structure */
    void			*driver;	/* ptr to driver for device */
    int				flags;
#define		ATA_D_USE_CHS		0x0001
#define		ATA_D_DETACHING		0x0002
#define		ATA_D_MEDIA_CHANGED	0x0004

    int				mode;		/* transfermode */
    int				cmd;		/* last cmd executed */
    void			*result;	/* misc data */
};

/* structure describing an ATA channel */
struct ata_channel {
    struct device		*dev;		/* device handle */
    int				unit;		/* channel number */
    struct resource		*r_io;		/* io addr resource handle */
    struct resource		*r_altio;	/* altio addr resource handle */
    struct resource		*r_bmio;	/* bmio addr resource handle */
    struct resource		*r_irq;		/* interrupt of this channel */
    void			*ih;		/* interrupt handle */
    int (*intr_func)(struct ata_channel *);	/* interrupt function */
    u_int32_t			chiptype;	/* pciid of controller chip */
    u_int32_t			alignment;	/* dma engine min alignment */
    int				flags;		/* controller flags */
#define		ATA_NO_SLAVE		0x01
#define		ATA_USE_16BIT		0x02
#define		ATA_ATAPI_DMA_RO	0x04
#define		ATA_QUEUED		0x08
#define		ATA_DMA_ACTIVE		0x10

    struct ata_device		device[2];	/* devices on this channel */
#define		MASTER			0x00
#define		SLAVE			0x01

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
#define		ATA_ACTIVE		0x0010
#define		ATA_ACTIVE_ATA		0x0020
#define		ATA_ACTIVE_ATAPI	0x0040
#define		ATA_CONTROL		0x0080

    TAILQ_HEAD(, ad_request)	ata_queue;	/* head of ATA queue */
    TAILQ_HEAD(, atapi_request) atapi_queue;	/* head of ATAPI queue */
    void			*running;	/* currently running request */
};

/* disk bay/drawer related */
#define		ATA_LED_OFF		0x00
#define		ATA_LED_RED		0x01
#define		ATA_LED_GREEN		0x02
#define		ATA_LED_ORANGE		0x03

/* externs */
extern devclass_t ata_devclass;
 
/* public prototypes */
int ata_probe(device_t);
int ata_attach(device_t);
int ata_detach(device_t);
int ata_resume(device_t);

void ata_start(struct ata_channel *);
void ata_reset(struct ata_channel *);
int ata_reinit(struct ata_channel *);
int ata_wait(struct ata_device *, u_int8_t);
int ata_command(struct ata_device *, u_int8_t, u_int64_t, u_int16_t, u_int8_t, int);
void ata_drawerleds(struct ata_device *, u_int8_t);
int ata_printf(struct ata_channel *, int, const char *, ...) __printflike(3, 4);
int ata_prtdev(struct ata_device *, const char *, ...) __printflike(2, 3);
void ata_set_name(struct ata_device *, char *, int);
void ata_free_name(struct ata_device *);
int ata_get_lun(u_int32_t *);
int ata_test_lun(u_int32_t *, int);
void ata_free_lun(u_int32_t *, int);
char *ata_mode2str(int);
int ata_pmode(struct ata_params *);
int ata_wmode(struct ata_params *);
int ata_umode(struct ata_params *);
int ata_find_dev(device_t, u_int32_t, u_int32_t);

void *ata_dmaalloc(struct ata_channel *, int);
void ata_dmainit(struct ata_channel *, int, int, int, int);
int ata_dmasetup(struct ata_channel *, int, struct ata_dmaentry *, caddr_t, int);
void ata_dmastart(struct ata_channel *, int, struct ata_dmaentry *, int);
int ata_dmastatus(struct ata_channel *);
int ata_dmadone(struct ata_channel *);

/* macros for locking a channel */
#define ATA_LOCK_CH(ch, value)\
	atomic_cmpset_int(&(ch)->active, ATA_IDLE, (value))

#define ATA_SLEEPLOCK_CH(ch, value)\
	while (!atomic_cmpset_int(&(ch)->active, ATA_IDLE, (value)))\
	    tsleep((caddr_t)&(ch), PRIBIO, "atalck", 1);

#define ATA_FORCELOCK_CH(ch, value)\
	(ch)->active = value;

#define ATA_UNLOCK_CH(ch)\
	(ch)->active = ATA_IDLE

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
#define ATA_INSL(res, offset, addr, count) \
	bus_space_read_multi_4(rman_get_bustag((res)), \
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
#define ATA_OUTSL(res, offset, addr, count) \
	bus_space_write_multi_4(rman_get_bustag((res)), \
				rman_get_bushandle((res)), \
				(offset), (addr), (count))
