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
 *	$Id: ata-all.h,v 1.3 1999/03/01 21:03:15 sos Exp sos $
 */

/* ATA register defines */

#define ATA_DATA			0x00	/* data register */
#define	ATA_ERROR			0x01	/* (R) error register */
#define ATA_PRECOMP			0x01	/* (W) precompensation */
#define ATA_COUNT			0x02	/* sector count */
#define		ATA_I_CMD		0x01	/* cmd (1) | data (0) */
#define		ATA_I_IN		0x02	/* read (1) | write (0) */
#define		ATA_I_RELEASE		0x04	/* released bus (1) */

#define	ATA_SECTOR			0x03	/* sector # */
#define	ATA_CYL_LSB			0x04	/* cylinder# LSB */
#define	ATA_CYL_MSB			0x05	/* cylinder# MSB */
#define ATA_DRIVE			0x06	/* Sector/Drive/Head register */
#define		ATA_D_IBM		0xa0	/* 512 byte sectors, ECC */

#define ATA_CMD				0x07	/* command register */
#define		ATA_C_ATA_IDENTIFY	0xec	/* get ATA params */
#define		ATA_C_ATAPI_IDENTIFY	0xa1	/* get ATAPI params*/
#define		ATA_C_READ		0x20	/* read command */
#define		ATA_C_WRITE		0x30	/* write command */
#define		ATA_C_READ_MULTI	0xc4	/* read multi command */
#define		ATA_C_WRITE_MULTI	0xc5	/* write multi command */
#define		ATA_C_SET_MULTI		0xc6	/* set multi size command */
#define		ATA_C_PACKET_CMD	0xa0	/* set multi size command */

#define ATA_STATUS			0x07	/* status register */
#define		ATA_S_ERROR		0x01	/* error */
#define		ATA_S_INDEX		0x02	/* index */
#define		ATA_S_CORR		0x04	/* data corrected */
#define		ATA_S_DRQ		0x08	/* data request */
#define		ATA_S_DSC		0x10	/* drive Seek Completed */
#define		ATA_S_DWF		0x20	/* drive write fault */
#define		ATA_S_DRDY		0x40	/* drive ready */
#define		ATA_S_BSY		0x80	/* busy */

#define ATA_ALTPORT			0x206	/* alternate Status register */
#define 	ATA_A_IDS		0x02	/* disable interrupts */
#define		ATA_A_RESET		0x04	/* RESET controller */
#define 	ATA_A_4BIT		0x08	/* 4 head bits */

/* Misc defines */
#define	ATA_MASTER			0x00
#define	ATA_SLAVE			0x10
#define	ATA_IOSIZE			0x08

/* Devices types */
#define ATA_ATA_MASTER			0x01
#define ATA_ATA_SLAVE			0x02
#define ATA_ATAPI_MASTER		0x04
#define ATA_ATAPI_SLAVE			0x08

struct ata_params {
    int16_t	config;				/* general configuration bits */
    u_int16_t	cylinders;			/* number of cylinders */
    int16_t	reserved2;
    u_int16_t	heads;				/* # heads */
    int16_t	unfbytespertrk;			/* # unformatted bytes/track */
    int16_t	unfbytes;			/* # unformatted bytes/sector */
    u_int16_t	sectors;			/* # sectors/track */
    int16_t	vendorunique[3];
    int8_t	serial[20];			/* serial number */
    int16_t	buffertype;			/* buffer type */
#define	ATA_BT_SINGLEPORTSECTOR		1	/* 1 port, 1 sector buffer */
#define	ATA_BT_DUALPORTMULTI		2	/* 2 port, mult sector buffer */
#define	ATA_BT_DUALPORTMULTICACHE	3	/* above plus track cache */

    int16_t	buffersize;			/* buf size, 512-byte units */
    int16_t	necc;				/* ecc bytes appended */
    int8_t	revision[8];			/* firmware revision */
    int8_t	model[40];			/* model name */
    int8_t	nsecperint;			/* sectors per interrupt */
    int8_t	vendorunique1;
    int16_t	usedmovsd;			/* double word read/write? */
    int8_t	vendorunique2;
    int8_t	capability;			/* various capability bits */
    int16_t	cap_validate;			/* validation for above */
    int8_t	vendorunique3;
    int8_t	opiomode;			/* PIO modes 0-2 */
    int8_t	vendorunique4;
    int8_t	odmamode;			/* old DMA modes, not ATA-3 */
    int16_t	atavalid;			/* fields valid */
    int16_t	currcyls;
    int16_t	currheads;
    int16_t	currsectors;
    int16_t	currsize0;
    int16_t	currsize1;
    int8_t	currmultsect;
    int8_t	multsectvalid;
    int		lbasize;
    int16_t	dmasword;			/* obsolete in ATA-3 */
    int16_t	dmamword;			/* multiword DMA modes */
    int16_t	eidepiomodes;			/* advanced PIO modes */
    int16_t	eidedmamin;			/* fastest DMA timing */
    int16_t	eidedmanorm;			/* recommended DMA timing */
    int16_t	eidepioblind;			/* fastest possible blind PIO */
    int16_t	eidepioacked;			/* fastest possible IORDY PIO */
    int16_t	reserved69;
    int16_t	reserved70;
    int16_t	reserved71;
    int16_t	reserved72;
    int16_t	reserved73;
    int16_t	reserved74;
    int16_t	queuelen;
    int16_t	reserved76;
    int16_t	reserved77;
    int16_t	reserved78;
    int16_t	reserved79;
    int16_t	versmajor;
    int16_t	versminor;
    int16_t	featsupp1;
    int16_t	featsupp2;
    int16_t	featsupp3;
    int16_t	featenab1;
    int16_t	featenab2;
    int16_t	featenab3;
    int16_t	udmamode;			/* UltraDMA modes */
    int16_t	erasetime;
    int16_t	enherasetime;
    int16_t	apmlevel;
    int16_t	reserved92[34];
    int16_t	rmvcap;
    int16_t	securelevel;
};

/*
 * Structure describing an ATA device
 */
struct ata_softc {
    u_int32_t			unit;		/* this instance's number */
    u_int32_t			ioaddr;		/* port addr */
    u_int32_t			altioaddr;	/* alternate port addr */
    void    			*dmacookie;	/* handle for DMA services */
    int32_t			flags;		/* controller flags */
#define		ATA_F_SLAVE_ONLY	0x0001

    int32_t			devices;	/* what is present */
    u_int8_t			status;		/* last controller status */
    u_int8_t			error;		/* last controller error */

    int32_t			active;		/* active processing request */
#define		ATA_IDLE		0x0
#define		ATA_ACTIVE_ATA		0x1
#define		ATA_ACTIVE_ATAPI	0x2
#define		ATA_IGNORE_INTR		0x3

    struct buf_queue_head       ata_queue;      /* head of ATA queue */
    struct ata_params		*ata_parm[2];	/* ata device params */
    TAILQ_HEAD(, atapi_request) atapi_queue;    /* head of ATAPI queue */
    struct atapi_params		*atapi_parm[2];	/* atapi device params */

#ifdef DEVFS
    static void *devfs_token;
#endif
};

struct ata_request {
    struct ad_softc		*driver;	/* ptr to parent device */
    /*bla request bla*/
    u_int32_t                   flags;          /* drive flags */
#define         A_READ			0x0001

    u_int32_t                   bytecount;      /* bytes to transfer */
    u_int32_t                   donecount;      /* bytes transferred */
    u_int32_t                   currentsize;    /* size of current transfer */
    struct buf			*bp;		/* associated buf ptr */
    TAILQ_ENTRY(ata_request)	chain;          /* list management */
};

#define MAXATA	8

extern struct ata_softc *atadevices[];
 
/* public prototypes */
void ata_start(struct ata_softc *);
int32_t ata_wait(struct ata_softc *, u_int8_t);
int32_t atapi_wait(struct ata_softc *, u_int8_t);
void bpack(int8_t *, int8_t *, int32_t);

