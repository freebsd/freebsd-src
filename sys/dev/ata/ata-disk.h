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
 * $FreeBSD: src/sys/dev/ata/ata-disk.h,v 1.22.2.1 2000/03/14 20:25:08 sos Exp $
 */

/* structure describing an ATA disk */
struct ad_softc {  
    struct ata_softc		*controller;	/* ptr to parent ctrl */
    int32_t			unit;		/* ATA_MASTER or ATA_SLAVE */
    int32_t			lun;		/* logical unit number */
    u_int32_t			total_secs;	/* total # of sectors (LBA) */
    u_int8_t			heads;
    u_int8_t			sectors;
    u_int32_t			transfersize;	/* size of each transfer */
    u_int32_t			num_tags;	/* number of tags supported */
    u_int32_t			flags;		/* drive flags */
#define		AD_F_LABELLING		0x0001		
#define		AD_F_LBA_ENABLED	0x0002
#define		AD_F_32B_ENABLED	0x0004
#define		AD_F_TAG_ENABLED	0x0008

    struct buf_queue_head	queue;		/* head of request queue */
    struct devstat		stats;		/* devstat entry */
    struct disk			disk;		/* disklabel/slice stuff */
    dev_t			dev1, dev2;	/* device place holder */
};

struct ad_request {
    struct ad_softc		*device;	/* ptr to parent device */
    u_int32_t			blockaddr;	/* block number */
    u_int32_t			bytecount;	/* bytes to transfer */
    u_int32_t			donecount;	/* bytes transferred */
    u_int32_t			currentsize;	/* size of current transfer */
    struct callout_handle       timeout_handle; /* handle for untimeout */ 
    int32_t			retries;	/* retry count */
    int32_t			flags;
#define		ADR_F_READ		0x0001
#define		ADR_F_ERROR		0x0002
#define		ADR_F_DMA_USED		0x0004
#define		ADR_F_FORCE_PIO		0x0008

    int8_t			*data;		/* pointer to data buf */
    struct buf			*bp;		/* associated buf ptr */
    u_int8_t			tag;		/* tag ID of this request */
    TAILQ_ENTRY(ad_request)	chain;		/* list management */
};

void ad_attach(struct ata_softc *, int32_t);
void ad_detach(struct ad_softc *);
void ad_start(struct ad_softc *);
void ad_transfer(struct ad_request *);
int32_t ad_interrupt(struct ad_request *);
void ad_reinit(struct ad_softc *);
