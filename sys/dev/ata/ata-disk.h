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

/* structure describing an ATA disk request */
struct ad_request {
    struct ad_softc		*device;	/* ptr to parent device */
    u_int32_t			blockaddr;	/* block number */
    u_int32_t			bytecount;	/* bytes to transfer */
    u_int32_t			donecount;	/* bytes transferred */
    u_int32_t			currentsize;	/* size of current transfer */
    struct callout_handle       timeout_handle; /* handle for untimeout */ 
    int				retries;	/* retry count */
    int				flags;
#define		ADR_F_READ		0x0001
#define		ADR_F_ERROR		0x0002
#define		ADR_F_DMA_USED		0x0004
#define		ADR_F_QUEUED		0x0008
#define		ADR_F_FORCE_PIO		0x0010
#define		ADR_F_FLUSHCACHE	0x0020

    caddr_t			data;		/* pointer to data buf */
    struct bio			*bp;		/* associated bio ptr */
    u_int8_t			tag;		/* tag ID of this request */
    int				serv;		/* request had service */
    struct ata_dmaentry		*dmatab;	/* DMA transfer table */
    TAILQ_ENTRY(ad_request)	chain;		/* list management */
};

/* structure describing an ATA disk */
struct ad_softc {  
    struct ata_softc		*controller;	/* ptr to parent ctrl */
    int				unit;		/* ATA_MASTER or ATA_SLAVE */
    int				lun;		/* logical unit number */
    u_int32_t			total_secs;	/* total # of sectors (LBA) */
    u_int8_t			heads;
    u_int8_t			sectors;
    u_int32_t			transfersize;	/* size of each transfer */
    int				num_tags;	/* number of tags supported */
    int				flags;		/* drive flags */
#define		AD_F_LABELLING		0x0001		
#define		AD_F_LBA_ENABLED	0x0002
#define		AD_F_32B_ENABLED	0x0004
#define		AD_F_TAG_ENABLED	0x0008

    struct ad_request		*tags[32];	/* tag array of requests */
    int				outstanding;	/* tags not serviced yet */
    struct bio_queue_head	queue;		/* head of request queue */
    struct devstat		stats;		/* devstat entry */
    struct disk			disk;		/* disklabel/slice stuff */
    dev_t			dev1, dev2;	/* device place holder */
};

void ad_attach(struct ata_softc *, int);
void ad_detach(struct ad_softc *);
void ad_start(struct ad_softc *);
int ad_transfer(struct ad_request *);
int ad_interrupt(struct ad_request *);
int ad_service(struct ad_softc *, int);
void ad_reinit(struct ad_softc *);
