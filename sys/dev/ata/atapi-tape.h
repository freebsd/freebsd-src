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
 *	$Id: atapi-tape.h,v 1.3 1999/03/07 21:49:14 sos Exp $
 */

/* MODE SENSE parameter header */
struct ast_header {
    u_int8_t	data_length;        	/* total length of data */
    u_int8_t	medium_type;       	/* medium type (if any) */
    u_int8_t	dsp;           		/* device specific parameter */
    u_int8_t	bdl;           		/* block Descriptor Length */
};

/* ATAPI tape drive Capabilities and Mechanical Status Page */
#define ATAPI_TAPE_CAP_PAGE     0x2a

struct ast_cappage {
    u_int8_t	page_code	:6;	/* page code == 0x2a */
    u_int8_t	reserved1_67	:2;
    u_int8_t	page_length;        	/* page Length == 0x12 */
    u_int8_t	reserved2;
    u_int8_t    reserved3;
    u_int8_t	readonly	:1;	/* read Only Mode */
    u_int8_t	reserved4_1234	:4;
    u_int8_t	reverse		:1;	/* supports reverse direction */
    u_int8_t	reserved4_67	:2;
    u_int8_t	reserved5_012	:3;
    u_int8_t	eformat		:1;	/* supports ERASE formatting */
    u_int8_t	reserved5_4	:1;
    u_int8_t	qfa     	:1;	/* supports QFA formats */
    u_int8_t	reserved5_67   	:2;
    u_int8_t	lock        	:1;	/* supports locking media */
    u_int8_t	locked      	:1;	/* the media is locked */
    u_int8_t	prevent     	:1;	/* defaults to prevent state */
    u_int8_t	eject       	:1;	/* supports eject */
    u_int8_t	disconnect	:1;	/* can break request > ctl */
    u_int8_t	reserved6_5    	:1;
    u_int8_t	ecc     	:1;	/* supports error correction */
    u_int8_t	compress    	:1;	/* supports data compression */
    u_int8_t	reserved7_0 	:1;
    u_int8_t	blk512      	:1;	/* supports 512b block size */
    u_int8_t	blk1024     	:1;	/* supports 1024b block size */
    u_int8_t	reserved7_3456 	:4;
    u_int8_t	slowb       	:1;	/* restricts byte count */
    u_int16_t	max_speed;     		/* supported speed in KBps */
    u_int16_t	max_defects;   		/* max stored defect entries */
    u_int16_t	ctl;           		/* continuous transfer limit */
    u_int16_t	speed;         		/* current Speed, in KBps */
    u_int16_t	buffer_size;        	/* buffer Size, in 512 bytes */
    u_int8_t	reserved18;
    u_int8_t	reserved19;
};

struct ast_softc {
    struct atapi_softc 		*atp;          	/* controller structure */
    int32_t			lun;           	/* logical device unit */
    int32_t			flags;         	/* device state flags */
    int32_t			blksize;	/* block size (512 | 1024) */
    struct buf_queue_head	buf_queue;    	/* queue of i/o requests */
    struct atapi_params		*param;     	/* drive parameters table */
    struct ast_header		header;       	/* MODE SENSE param header */
    struct ast_cappage 		cap;         	/* capabilities page info */
    struct devstat		stats;		/* devstat entry */
#ifdef  DEVFS
    void    			*cdevs_token;
    void    			*bdevs_token;
#endif
};
