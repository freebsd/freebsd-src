/*-
 * Copyright (c) 1998 - 2007 Søren Schmidt <sos@FreeBSD.org>
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
 * $FreeBSD: src/sys/dev/ata/atapi-tape.h,v 1.25.6.1 2008/11/25 02:59:29 kensmith Exp $
 */

/* ATAPI tape drive Capabilities and Mechanical Status Page */
struct ast_cappage {
    /* mode page data header */
    u_int8_t    data_length;                    /* total length of data */
    u_int8_t    medium_type;                    /* medium type (if any) */
    u_int8_t    reserved        :4;
    u_int8_t    mode            :3;             /* buffering mode */
    u_int8_t    write_protect   :1;             /* media is writeprotected */
    u_int8_t    blk_desc_len;                   /* block Descriptor Length */

    /* capabilities page */
    u_int8_t    page_code       :6;
#define ATAPI_TAPE_CAP_PAGE     0x2a

    u_int8_t    reserved0_6     :1;
    u_int8_t    ps              :1;             /* parameters saveable */
    u_int8_t    page_length;                    /* page Length == 0x12 */
    u_int8_t    reserved2;
    u_int8_t    reserved3;
    u_int8_t    readonly        :1;             /* read Only Mode */
    u_int8_t    reserved4_1234  :4;
    u_int8_t    reverse         :1;             /* supports reverse direction */
    u_int8_t    reserved4_67    :2;
    u_int8_t    reserved5_012   :3;
    u_int8_t    eformat         :1;             /* supports ERASE formatting */
    u_int8_t    reserved5_4     :1;
    u_int8_t    qfa             :1;             /* supports QFA formats */
    u_int8_t    reserved5_67    :2;
    u_int8_t    lock            :1;             /* supports locking media */
    u_int8_t    locked          :1;             /* the media is locked */
    u_int8_t    prevent         :1;             /* defaults to prevent state */
    u_int8_t    eject           :1;             /* supports eject */
    u_int8_t    disconnect      :1;             /* can break request > ctl */
    u_int8_t    reserved6_5     :1;
    u_int8_t    ecc             :1;             /* supports error correction */
    u_int8_t    compress        :1;             /* supports data compression */
    u_int8_t    reserved7_0     :1;
    u_int8_t    blk512          :1;             /* supports 512b block size */
    u_int8_t    blk1024         :1;             /* supports 1024b block size */
    u_int8_t    reserved7_3456  :4;
    u_int8_t    blk32k          :1;             /* supports 32kb block size */
    u_int16_t   max_speed;                      /* supported speed in KBps */
    u_int16_t   max_defects;                    /* max stored defect entries */
    u_int16_t   ctl;                            /* continuous transfer limit */
    u_int16_t   speed;                          /* current Speed, in KBps */
    u_int16_t   buffer_size;                    /* buffer Size, in 512 bytes */
    u_int8_t    reserved18;
    u_int8_t    reserved19;
};

/* ATAPI OnStream ADR data transfer mode page (ADR unique) */
struct ast_transferpage {
    /* mode page data header */
    u_int8_t    data_length;                    /* total length of data */
    u_int8_t    medium_type;                    /* medium type (if any) */
    u_int8_t    dsp;                            /* device specific parameter */
    u_int8_t    blk_desc_len;                   /* block Descriptor Length */

    /* data transfer page */
    u_int8_t    page_code       :6;
#define ATAPI_TAPE_TRANSFER_PAGE     0x30

    u_int8_t    reserved0_6     :1;
    u_int8_t    ps              :1;             /* parameters saveable */
    u_int8_t    page_length;                    /* page Length == 0x02 */
    u_int8_t    reserved2;
    u_int8_t    read32k         :1;             /* 32k blk size (data only) */
    u_int8_t    read32k5        :1;             /* 32.5k blk size (data&AUX) */
    u_int8_t    reserved3_23    :2;
    u_int8_t    write32k        :1;             /* 32k blk size (data only) */
    u_int8_t    write32k5       :1;             /* 32.5k blk size (data&AUX) */
    u_int8_t    reserved3_6     :1;
    u_int8_t    streaming       :1;             /* streaming mode enable */
};

/* ATAPI OnStream ADR vendor identification mode page (ADR unique) */
struct ast_identifypage {
    /* mode page data header */
    u_int8_t    data_length;                    /* total length of data */
    u_int8_t    medium_type;                    /* medium type (if any) */
    u_int8_t    dsp;                            /* device specific parameter */
    u_int8_t    blk_desc_len;                   /* block Descriptor Length */

    /* data transfer page */
    u_int8_t    page_code       :6;
#define ATAPI_TAPE_IDENTIFY_PAGE     0x36

    u_int8_t    reserved0_6     :1;
    u_int8_t    ps              :1;             /* parameters saveable */
    u_int8_t    page_length;                    /* page Length == 0x06 */
    u_int8_t    ident[4];                       /* host id string */
    u_int8_t    reserved6;
    u_int8_t    reserved7;
};

/* ATAPI read position structure */
struct ast_readposition {
    u_int8_t    reserved0_05    :6;
    u_int8_t    eop             :1;             /* end of partition */
    u_int8_t    bop             :1;             /* beginning of partition */
    u_int8_t    reserved1;
    u_int8_t    reserved2;
    u_int8_t    reserved3;
    u_int32_t   host;                           /* frame address in buffer */
    u_int32_t   tape;                           /* frame address on tape */
    u_int8_t    reserved12;
    u_int8_t    reserved13;
    u_int8_t    reserved14;
    u_int8_t    blks_in_buf;                    /* blocks in buffer */
    u_int8_t    reserved16;
    u_int8_t    reserved17;
    u_int8_t    reserved18;
    u_int8_t    reserved19;
};

struct ast_softc {
    int                         flags;          /* device state flags */
#define         F_CTL_WARN              0x0001  /* warned about CTL wrong? */
#define         F_WRITEPROTECT          0x0002  /* media is writeprotected */
#define         F_DATA_WRITTEN          0x0004  /* data has been written */
#define         F_FM_WRITTEN            0x0008  /* filemark has been written */
#define         F_ONSTREAM              0x0100  /* OnStream ADR device */

    int                         blksize;        /* block size (512 | 1024) */
    struct atapi_params         *param;         /* drive parameters table */
    struct ast_cappage          cap;            /* capabilities page info */
    struct devstat              *stats;         /* devstat entry */
    struct cdev                 *dev1, *dev2;   /* device place holders */
};
