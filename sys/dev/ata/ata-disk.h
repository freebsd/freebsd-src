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
 *	$Id: ata-disk.h,v 1.1 1999/03/01 21:19:18 sos Exp $
 */

/* Structure describing an ATA disk */
struct ad_softc {  
    struct ata_softc		*controller;	/* ptr to parent ctrl */
    struct ata_params		*ata_parm;	/* ata device params */
    struct diskslices 		*slices;
    int32_t			unit;		/* ATA_MASTER or ATA_SLAVE */
    int32_t			lun;		/* logical unit number */
    u_int16_t			cylinders;	/* disk geometry (probed) */
    u_int8_t  			heads;
    u_int8_t			sectors;
    u_int32_t			total_secs;	/* total # of sectors (LBA) */
    u_int32_t			transfersize;	/* size of each transfer */
    u_int32_t			currentsize;	/* size of current transfer */
    struct buf_queue_head 	queue;		/* head of request queue */
    u_int32_t			bytecount;	/* bytes to transfer */
    u_int32_t			donecount;	/* bytes transferred */
    u_int32_t			active;		/* active processing request */
    u_int32_t			flags;		/* drive flags */
    struct devstat 		stats;		/* devstat entry */
#define		AD_F_LABELLING		0x0001		
#ifdef DEVFS
    void			*cdevs_token;
    void			*bdevs_token;
#endif
};

void ad_transfer(struct buf *);
void ad_interrupt(struct buf *);

