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
 *	$Id: ata-all.h,v 1.6 1999/04/18 20:48:15 sos Exp $
 */

/* ATA register defines */
#define ATA_DATA			0x00	/* data register */
#define	ATA_ERROR			0x01	/* (R) error register */
#define ATA_FEATURE			0x01	/* (W) feature register */
#define		ATA_F_DMA		0x01	/* enable DMA */
#define		ATA_F_OVL		0x02	/* enable overlap */

#define ATA_COUNT			0x02	/* (W) sector count */
#define ATA_IREASON			0x02	/* (R) interrupt reason */
#define		ATA_I_CMD		0x01	/* cmd (1) | data (0) */
#define		ATA_I_IN		0x02	/* read (1) | write (0) */
#define		ATA_I_RELEASE		0x04	/* released bus (1) */
#define		ATA_I_TAGMASK		0xf8	/* tag mask */

#define	ATA_SECTOR			0x03	/* sector # */
#define	ATA_CYL_LSB			0x04	/* cylinder# LSB */
#define	ATA_CYL_MSB			0x05	/* cylinder# MSB */
#define ATA_DRIVE			0x06	/* Sector/Drive/Head register */
#define		ATA_D_LBA		0x40	/* use LBA adressing */
#define		ATA_D_IBM		0xa0	/* 512 byte sectors, ECC */

#define ATA_CMD				0x07	/* command register */
#define		ATA_C_ATA_IDENTIFY	0xec	/* get ATA params */
#define		ATA_C_ATAPI_IDENTIFY	0xa1	/* get ATAPI params*/
#define		ATA_C_READ		0x20	/* read command */
#define		ATA_C_WRITE		0x30	/* write command */
#define		ATA_C_READ_MULTI	0xc4	/* read multi command */
#define		ATA_C_WRITE_MULTI	0xc5	/* write multi command */
#define		ATA_C_SET_MULTI		0xc6	/* set multi size command */
#define		ATA_C_READ_DMA		0xc8	/* read w/DMA command */
#define		ATA_C_WRITE_DMA		0xca	/* write w/DMA command */
#define		ATA_C_PACKET_CMD	0xa0	/* packet command */
#define		ATA_C_SETFEATURES	0xef	/* features command */
#define		    ATA_C_FEA_SETXFER	0x03	/* set transfer mode */

#define ATA_STATUS			0x07	/* status register */
#define		ATA_S_ERROR		0x01	/* error */
#define		ATA_S_INDEX		0x02	/* index */
#define		ATA_S_CORR		0x04	/* data corrected */
#define		ATA_S_DRQ		0x08	/* data request */
#define		ATA_S_DSC		0x10	/* drive seek completed */
#define		ATA_S_SERV		0x10	/* drive needs service */
#define		ATA_S_DWF		0x20	/* drive write fault */
#define		ATA_S_DMRD		0x20	/* DMA ready */
#define		ATA_S_DRDY		0x40	/* drive ready */
#define		ATA_S_BSY		0x80	/* busy */

#define ATA_ALTPORT			0x206	/* alternate Status register */
#define 	ATA_A_IDS		0x02	/* disable interrupts */
#define		ATA_A_RESET		0x04	/* RESET controller */
#define 	ATA_A_4BIT		0x08	/* 4 head bits */

/* misc defines */
#define	ATA_MASTER			0x00
#define	ATA_SLAVE			0x10
#define	ATA_IOSIZE			0x08
#define ATA_OP_FINISHED			0x00
#define ATA_OP_CONTINUES		0x01

/* devices types */
#define ATA_ATA_MASTER			0x01
#define ATA_ATA_SLAVE			0x02
#define ATA_ATAPI_MASTER		0x04
#define ATA_ATAPI_SLAVE			0x08

/* busmaster DMA related defines */
#define ATA_BM_OFFSET1			0x08
#define ATA_DMA_ENTRIES			256
#define ATA_DMA_EOT			0x80000000

#define ATA_BMCMD_PORT			0x00
#define ATA_BMCMD_START_STOP		0x01
#define ATA_BMCMD_WRITE_READ		0x08

#define ATA_BMSTAT_PORT			0x02
#define ATA_BMSTAT_MASK			0x07
#define ATA_BMSTAT_ACTIVE		0x01
#define ATA_BMSTAT_ERROR		0x02
#define ATA_BMSTAT_INTERRUPT		0x04
#define ATA_BMSTAT_DMA_MASTER		0x20
#define ATA_BMSTAT_DMA_SLAVE		0x40

#define ATA_BMDTP_PORT			0x04

#define ATA_WDMA2			0x22
#define ATA_UDMA2			0x42

/* structure for holding DMA address data */
struct ata_dmaentry {
        u_int32_t base;
        u_int32_t count;
};  

/* structure describing an ATA device */
struct ata_softc {
    int32_t			unit;		/* unit on this controller */
    int32_t			lun;		/* logical unit # */
    struct device		*dev;		/* device handle */
    int32_t			ioaddr;		/* port addr */
    int32_t			altioaddr;	/* alternate port addr */
    int32_t			bmaddr;		/* bus master DMA port */
    struct ata_dmaentry		*dmatab[2];	/* DMA transfer tables */
    int32_t			flags;		/* controller flags */
    int32_t			devices;	/* what is present */
    u_int8_t			status;		/* last controller status */
    u_int8_t			error;		/* last controller error */
    int32_t			active;		/* active processing request */
#define		ATA_IDLE		0x0
#define		ATA_IMMEDIATE		0x0
#define		ATA_WAIT_INTR		0x1
#define		ATA_IGNORE_INTR		0x2
#define		ATA_ACTIVE_ATA		0x3
#define		ATA_ACTIVE_ATAPI	0x4

    TAILQ_HEAD(, ad_request) 	ata_queue;	/* head of ATA queue */
    TAILQ_HEAD(, atapi_request) atapi_queue;    /* head of ATAPI queue */
};

#define MAXATA	8

extern struct ata_softc *atadevices[];
 
/* public prototypes */
void ata_start(struct ata_softc *);
int32_t ata_wait(struct ata_softc *, int32_t, u_int8_t);
int32_t ata_command(struct ata_softc *, int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, u_int32_t, int32_t);
int32_t ata_dmainit(struct ata_softc *, int32_t, int32_t, int32_t, int32_t);
int32_t ata_dmasetup(struct ata_softc *, int32_t, int8_t *, int32_t, int32_t);
void ata_dmastart(struct ata_softc *, int32_t);
int32_t ata_dmastatus(struct ata_softc *, int32_t);
int32_t ata_dmadone(struct ata_softc *, int32_t);
void bswap(int8_t *, int32_t);
void btrim(int8_t *, int32_t);
void bpack(int8_t *, int8_t *, int32_t);

