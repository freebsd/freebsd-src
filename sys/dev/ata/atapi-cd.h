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
 * $FreeBSD: src/sys/dev/ata/atapi-cd.h,v 1.46.2.1 2007/10/31 19:59:53 sos Exp $
 */

/* CDROM Table Of Contents */
#define MAXTRK 99
struct toc {
    struct ioc_toc_header       hdr;
    struct cd_toc_entry         tab[MAXTRK + 1];
};

/* DVD CSS authentication */
struct dvd_miscauth {
    u_int16_t   length;
    u_int16_t   reserved;
    u_int8_t    data[2048];
};

/* CDROM Audio Control Parameters Page */
struct audiopage {
    /* mode page data header */
    u_int16_t   data_length;
    u_int8_t    medium_type;
    u_int8_t    dev_spec;
    u_int8_t    unused[2];
    u_int16_t   blk_desc_len;

    /* audio control page */
    u_int8_t    page_code;
#define ATAPI_CDROM_AUDIO_PAGE      0x0e
#define ATAPI_CDROM_AUDIO_PAGE_MASK 0x4e

    u_int8_t    param_len;
    u_int8_t    flags;
#define CD_PA_SOTC      0x02
#define CD_PA_IMMED     0x04

    u_int8_t    reserved3;
    u_int8_t    reserved4;
    u_int8_t    reserved5;
    u_int16_t   lb_per_sec;
    struct port_control {
	u_int8_t        channels:4;
#define CHANNEL_0       1
#define CHANNEL_1       2
#define CHANNEL_2       4
#define CHANNEL_3       8

	u_int8_t        volume;
    } port[4];
};


/* CDROM Capabilities and Mechanical Status Page */
struct cappage {
    /* mode page data header */
    u_int16_t   data_length;
    u_int8_t    medium_type;
#define MST_TYPE_MASK_LOW       0x0f
#define MST_FMT_NONE            0x00
#define MST_DATA_120            0x01
#define MST_AUDIO_120           0x02
#define MST_COMB_120            0x03
#define MST_PHOTO_120           0x04
#define MST_DATA_80             0x05
#define MST_AUDIO_80            0x06
#define MST_COMB_80             0x07
#define MST_PHOTO_80            0x08

#define MST_TYPE_MASK_HIGH      0x70
#define MST_CDROM               0x00
#define MST_CDR                 0x10
#define MST_CDRW                0x20
#define MST_DVD                 0x40

#define MST_NO_DISC             0x70
#define MST_DOOR_OPEN           0x71
#define MST_FMT_ERROR           0x72

    u_int8_t    dev_spec;
    u_int16_t   unused;
    u_int16_t   blk_desc_len;

    /* capabilities page */
    u_int8_t    page_code;
#define ATAPI_CDROM_CAP_PAGE    0x2a

    u_int8_t    param_len;

    u_int16_t   media;
#define MST_READ_CDR            0x0001
#define MST_READ_CDRW           0x0002
#define MST_READ_PACKET         0x0004
#define MST_READ_DVDROM         0x0008
#define MST_READ_DVDR           0x0010
#define MST_READ_DVDRAM         0x0020
#define MST_WRITE_CDR           0x0100
#define MST_WRITE_CDRW          0x0200
#define MST_WRITE_TEST          0x0400
#define MST_WRITE_DVDR          0x1000
#define MST_WRITE_DVDRAM        0x2000

    u_int16_t   capabilities;
#define MST_AUDIO_PLAY          0x0001
#define MST_COMPOSITE           0x0002
#define MST_AUDIO_P1            0x0004
#define MST_AUDIO_P2            0x0008
#define MST_MODE2_f1            0x0010
#define MST_MODE2_f2            0x0020
#define MST_MULTISESSION        0x0040
#define MST_BURNPROOF           0x0080
#define MST_READ_CDDA           0x0100
#define MST_CDDA_STREAM         0x0200
#define MST_COMBINED_RW         0x0400
#define MST_CORRECTED_RW        0x0800
#define MST_SUPPORT_C2          0x1000
#define MST_ISRC                0x2000
#define MST_UPC                 0x4000

    u_int8_t    mechanism;
#define MST_LOCKABLE            0x01
#define MST_LOCKED              0x02
#define MST_PREVENT             0x04
#define MST_EJECT               0x08
#define MST_MECH_MASK           0xe0
#define MST_MECH_CADDY          0x00
#define MST_MECH_TRAY           0x20
#define MST_MECH_POPUP          0x40
#define MST_MECH_CHANGER        0x80
#define MST_MECH_CARTRIDGE      0xa0

    uint8_t     audio;
#define MST_SEP_VOL             0x01
#define MST_SEP_MUTE            0x02

    u_int16_t   max_read_speed;         /* max raw data rate in bytes/1000 */
    u_int16_t   max_vol_levels;         /* number of discrete volume levels */
    u_int16_t   buf_size;               /* internal buffer size in bytes/1024 */
    u_int16_t   cur_read_speed;         /* current data rate in bytes/1000  */

    u_int8_t    reserved3;
    u_int8_t    misc;

    u_int16_t   max_write_speed;        /* max raw data rate in bytes/1000 */
    u_int16_t   cur_write_speed;        /* current data rate in bytes/1000  */
    u_int16_t   copy_protect_rev;
    u_int16_t   reserved4;
};

#define CH_READY                0
#define CH_LOADING              1
#define CH_UNLOADING            2
#define CH_INITIALIZING         3

#define CD_IDLE                 0
#define CD_AUDIO_ACTIVE         1
#define CD_AUDIO_SCAN           2
#define CD_HOST_ACTIVE          3
#define CD_NO_STATE             7

/* CDROM Changer mechanism status structure */
struct changer {
    u_int8_t    current_slot    :5;     /* active changer slot */
    u_int8_t    mech_state      :2;     /* current changer state */

    u_int8_t    fault           :1;     /* fault in last operation */
    u_int8_t    reserved0       :5;
    u_int8_t    cd_state        :3;     /* current mechanism state */

    u_int8_t    current_lba[3];         /* current LBA */
    u_int8_t    slots;                  /* number of available slots */
    u_int16_t   table_length;           /* slot table length */
    struct {
	u_int8_t        changed :1;     /* media has changed in this slot */
	u_int8_t        unused  :6;
	u_int8_t        present :1;     /* slot has a CD present */
	u_int8_t        reserved0;
	u_int8_t        reserved1;
	u_int8_t        reserved2;
    } slot[32];
};

/* CDROM Write Parameters Mode Page (Burners ONLY) */
struct write_param {
    /* mode page data header */
    u_int16_t   data_length;
    u_int8_t    medium_type;
    u_int8_t    dev_spec;
    u_int8_t    unused[2];
    u_int16_t   blk_desc_len;

    /* write parameters page */
    u_int8_t    page_code;
#define ATAPI_CDROM_WRITE_PARAMETERS_PAGE      0x05

    u_int8_t    page_length;            /* 0x32 */
    u_int8_t    write_type      :4;     /* write stream type */
#define CDR_WTYPE_PACKET        0x00
#define CDR_WTYPE_TRACK         0x01
#define CDR_WTYPE_SESSION       0x02
#define CDR_WTYPE_RAW           0x03

    u_int8_t    test_write      :1;     /* test write enable */
    u_int8_t    link_size_valid :1;
    u_int8_t    burnproof       :1;     /* BurnProof enable */
    u_int8_t    reserved2_7     :1;
    u_int8_t    track_mode      :4;     /* track mode */
#define CDR_TMODE_AUDIO         0x00
#define CDR_TMODE_AUDIO_PREEMP  0x01
#define CDR_TMODE_ALLOW_COPY    0x02
#define CDR_TMODE_DATA          0x04
#define CDR_TMODE_QUAD_AUDIO    0x08

    u_int8_t    copy            :1;     /* generation stamp */
    u_int8_t    fp              :1;     /* fixed packet type */
    u_int8_t    session_type    :2;     /* session type */
#define CDR_SESS_NONE           0x00
#define CDR_SESS_FINAL          0x01
#define CDR_SESS_RESERVED       0x02
#define CDR_SESS_MULTI          0x03

    u_int8_t    datablock_type  :4;     /* data type code (see cdrio.h) */
    u_int8_t    reserved4_4567  :4;
    u_int8_t    link_size;
    u_int8_t    reserved6;
    u_int8_t    host_app_code   :6;     /* host application code */
    u_int8_t    reserved7_67    :2;
    u_int8_t    session_format;         /* session format */
#define CDR_SESS_CDROM          0x00
#define CDR_SESS_CDI            0x10
#define CDR_SESS_CDROM_XA       0x20

    u_int8_t    reserved9;
    u_int32_t   packet_size;            /* packet size in bytes */
    u_int16_t   audio_pause_length;     /* audio pause length in secs */
    u_int8_t    media_catalog_number[16];
    u_int8_t    isr_code[16];
    u_int8_t    sub_hdr_byte0;
    u_int8_t    sub_hdr_byte1;
    u_int8_t    sub_hdr_byte2;
    u_int8_t    sub_hdr_byte3;
    u_int8_t    vendor_specific_byte0;
    u_int8_t    vendor_specific_byte1;
    u_int8_t    vendor_specific_byte2;
    u_int8_t    vendor_specific_byte3;
} __packed;

/* CDROM Read Track Information structure */
struct acd_track_info {
    u_int16_t   data_length;
    u_int8_t    track_number;           /* current track number */
    u_int8_t    session_number;         /* current session number */
    u_int8_t    reserved4;
    u_int8_t    track_mode      :4;     /* mode of this track */
    u_int8_t    copy            :1;     /* generation stamp */
    u_int8_t    damage          :1;     /* damaged track */
    u_int8_t    reserved5_67    :2;
    u_int8_t    data_mode       :4;     /* data mode of this disc */
    u_int8_t    fp              :1;     /* fixed packet */
    u_int8_t    packet          :1;     /* packet track */
    u_int8_t    blank           :1;     /* blank (empty) track */
    u_int8_t    rt              :1;     /* reserved track */
    u_int8_t    nwa_valid       :1;     /* next_writeable_addr field valid */
    u_int8_t    reserved7_17    :7;
    u_int       track_start_addr;       /* start of this track */
    u_int       next_writeable_addr;    /* next writeable addr on this disc */
    u_int       free_blocks;            /* free block on this disc */
    u_int       fixed_packet_size;      /* size of packets on this track */
    u_int       track_length;           /* length of this track */
};

/* Structure describing an ATAPI CDROM device */
struct acd_softc {
    int                         flags;          /* device state flags */
#define         F_LOCKED                0x0001  /* this unit is locked */

    struct toc                  toc;            /* table of disc contents */
    struct audiopage            au;             /* audio page info */
    struct audiopage            aumask;         /* audio page mask */
    struct cappage              cap;            /* capabilities page info */
    struct cd_sub_channel_info  subchan;        /* subchannel info */
    struct changer              *changer_info;  /* changer info */
    struct acd_softc            **driver;       /* softc's of changer slots */
    int                         slot;           /* this instance slot number */
    time_t                      timestamp;      /* this instance timestamp */
    u_int32_t                   disk_size;      /* size of current media */
    u_int32_t                   block_size;     /* blocksize currently used */
    u_int32_t                   iomax;          /* Max I/O request (bytes) */
    struct g_geom               *gp;            /* geom instance */
    struct g_provider           *pp[MAXTRK+1];  /* providers */
};
