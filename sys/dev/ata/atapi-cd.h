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

/* CDROM Table Of Contents */
#define MAXTRK 99
struct toc {
    struct ioc_toc_header	hdr;
    struct cd_toc_entry		tab[MAXTRK + 1];
};

/* CDROM Audio Control Parameters Page */
struct audiopage {
    /* mode page data header */
    u_int16_t	data_length;
    u_int8_t	medium_type;
    u_int8_t	dev_spec;
    u_int8_t	unused[2];
    u_int16_t	blk_desc_len;

    /* audio control page */
    u_int8_t	page_code;
#define ATAPI_CDROM_AUDIO_PAGE	    0x0e
#define ATAPI_CDROM_AUDIO_PAGE_MASK 0x4e

    u_int8_t	param_len;
    u_int8_t	flags;
#define CD_PA_SOTC	0x02
#define CD_PA_IMMED	0x04

    u_int8_t	reserved3;
    u_int8_t	reserved4;
    u_int8_t	reserved5;
    u_int16_t	lb_per_sec;
    struct port_control {
	u_int8_t	channels:4;
#define CHANNEL_0	1
#define CHANNEL_1	2
#define CHANNEL_2	4
#define CHANNEL_3	8

	u_int8_t	volume;
    } port[4];
};

/* CDROM Capabilities and Mechanical Status Page */
struct cappage {
    /* mode page data header */
    u_int16_t	data_length;
    u_int8_t	medium_type;
#define MST_TYPE_MASK_LOW	0x0f
#define MST_FMT_NONE		0x00
#define MST_DATA_120		0x01
#define MST_AUDIO_120		0x02
#define MST_COMB_120		0x03
#define MST_PHOTO_120		0x04
#define MST_DATA_80		0x05
#define MST_AUDIO_80		0x06
#define MST_COMB_80		0x07
#define MST_PHOTO_80		0x08

#define MST_TYPE_MASK_HIGH	0x70
#define MST_CDROM		0x00
#define MST_CDR			0x10
#define MST_CDRW		0x20

#define MST_NO_DISC		0x70
#define MST_DOOR_OPEN		0x71
#define MST_FMT_ERROR		0x72

    u_int8_t	dev_spec;
    u_int8_t	unused[2];
    u_int16_t	blk_desc_len;

    /* capabilities page */
    u_int8_t	page_code;
#define ATAPI_CDROM_CAP_PAGE	0x2a

    u_int8_t	param_len;
    u_int8_t	read_cdr	:1;	/* supports CD-R read */
    u_int8_t	read_cdrw	:1;	/* supports CD-RW read */
    u_int8_t	read_packet	:1;	/* supports reading packet tracks */
    u_int8_t	read_dvdrom	:1;	/* supports DVD-ROM read */
    u_int8_t	read_dvdr	:1;	/* supports DVD-R read */
    u_int8_t	read_dvdram	:1;	/* supports DVD-RAM read */
    u_int8_t	reserved2_67	:2;
    u_int8_t	write_cdr	:1;	/* supports CD-R write */
    u_int8_t	write_cdrw	:1;	/* supports CD-RW write */
    u_int8_t	test_write	:1;	/* supports test writing */
    u_int8_t	reserved3_3	:1;
    u_int8_t	write_dvdr	:1;	/* supports DVD-R write */
    u_int8_t	write_dvdram	:1;	/* supports DVD-RAM write */
    u_int8_t	reserved3_67	:2;
    u_int8_t	audio_play	:1;	/* audio play supported */
    u_int8_t	composite	:1;	/* composite audio/video supported */
    u_int8_t	dport1		:1;	/* digital audio on port 1 */
    u_int8_t	dport2		:1;	/* digital audio on port 2 */
    u_int8_t	mode2_form1	:1;	/* mode 2 form 1 (XA) read */
    u_int8_t	mode2_form2	:1;	/* mode 2 form 2 format */
    u_int8_t	multisession	:1;	/* multi-session photo-CD */
    u_int8_t			:1;
    u_int8_t	cd_da		:1;	/* audio-CD read supported */
    u_int8_t	cd_da_stream	:1;	/* CD-DA streaming */
    u_int8_t	rw		:1;	/* combined R-W subchannels */
    u_int8_t	rw_corr		:1;	/* R-W subchannel data corrected */
    u_int8_t	c2		:1;	/* C2 error pointers supported */
    u_int8_t	isrc		:1;	/* can return the ISRC info */
    u_int8_t	upc		:1;	/* can return the catalog number UPC */
    u_int8_t			:1;
    u_int8_t	lock		:1;	/* can be locked */
    u_int8_t	locked		:1;	/* current lock state */
    u_int8_t	prevent		:1;	/* prevent jumper installed */
    u_int8_t	eject		:1;	/* can eject */
    u_int8_t			:1;
    u_int8_t	mech		:3;	/* loading mechanism type */
#define MST_MECH_CADDY		0
#define MST_MECH_TRAY		1
#define MST_MECH_POPUP		2
#define MST_MECH_CHANGER	4
#define MST_MECH_CARTRIDGE	5

    u_int8_t	sep_vol		:1;	/* independent volume of channels */
    u_int8_t	sep_mute	:1;	/* independent mute of channels */
    u_int8_t:6;

    u_int16_t	max_read_speed;		/* max raw data rate in bytes/1000 */
    u_int16_t	max_vol_levels;		/* number of discrete volume levels */
    u_int16_t	buf_size;		/* internal buffer size in bytes/1024 */
    u_int16_t	cur_read_speed;		/* current data rate in bytes/1000  */

    u_int8_t	reserved3;
    u_int8_t	bckf		:1;	/* data valid on failing edge of BCK */
    u_int8_t	rch		:1;	/* high LRCK indicates left channel */
    u_int8_t	lsbf		:1;	/* set if LSB first */
    u_int8_t	dlen		:2;
#define MST_DLEN_32		0
#define MST_DLEN_16		1
#define MST_DLEN_24		2
#define MST_DLEN_24_I2S		3

    u_int8_t			:3;
    u_int16_t	max_write_speed;	/* max raw data rate in bytes/1000 */
    u_int16_t	cur_write_speed;	/* current data rate in bytes/1000  */
};

/* CDROM Changer mechanism status structure */
struct changer {
    u_int8_t	current_slot	:5;	/* active changer slot */
    u_int8_t	mech_state	:2;	/* current changer state */
#define CH_READY		0
#define CH_LOADING		1
#define CH_UNLOADING		2
#define CH_INITIALIZING		3

    u_int8_t	fault		:1;	/* fault in last operation */
    u_int8_t	reserved0	:5;
    u_int8_t	cd_state	:3;	/* current mechanism state */
#define CD_IDLE			0
#define CD_AUDIO_ACTIVE		1
#define CD_AUDIO_SCAN		2
#define CD_HOST_ACTIVE		3
#define CD_NO_STATE		7

    u_int8_t	current_lba[3];		/* current LBA */
    u_int8_t	slots;			/* number of available slots */
    u_int16_t	table_length;		/* slot table length */
    struct {
	u_int8_t	changed :1;	/* media has changed in this slot */
	u_int8_t	unused	:6;
	u_int8_t	present :1;	/* slot has a CD present */
	u_int8_t	reserved0;
	u_int8_t	reserved1;
	u_int8_t	reserved2;
    } slot[32];
};

/* CDROM Write Parameters Mode Page (Burners ONLY) */
struct write_param {
    /* mode page data header */
    u_int16_t	data_length;
    u_int8_t	medium_type;
    u_int8_t	dev_spec;
    u_int8_t	unused[2];
    u_int16_t	blk_desc_len;

    /* write parameters page */
    u_int8_t	page_code;
#define ATAPI_CDROM_WRITE_PARAMETERS_PAGE      0x05

    u_int8_t	page_length;		/* 0x32 */
    u_int8_t	write_type	:4;	/* write stream type */
#define CDR_WTYPE_PACKET	0x00
#define CDR_WTYPE_TRACK		0x01
#define CDR_WTYPE_SESSION	0x02
#define CDR_WTYPE_RAW		0x03

    u_int8_t	test_write	:1;	/* test write enable */
    u_int8_t	reserved2_567	:3;
    u_int8_t	track_mode	:4;	/* track mode */
#define CDR_TMODE_AUDIO		0x00
#define CDR_TMODE_AUDIO_PREEMP	0x01
#define CDR_TMODE_ALLOW_COPY	0x02
#define CDR_TMODE_DATA		0x04
#define CDR_TMODE_QUAD_AUDIO	0x08

    u_int8_t	copy		:1;	/* generation stamp */
    u_int8_t	fp		:1;	/* fixed packet type */
    u_int8_t	multi_session	:2;	/* multi-session type */
#define CDR_MSES_NONE		0x00
#define CDR_MSES_FINAL		0x01
#define CDR_MSES_RESERVED	0x02
#define CDR_MSES_MULTI		0x03

    u_int8_t	data_block_type :4;	/* data block type code */
#define CDR_DB_RAW		0x0	/* 2352 bytes of raw data */
#define CDR_DB_RAW_PQ		0x1	/* 2368 bytes raw data + P/Q subchan */
#define CDR_DB_RAW_PW		0x2	/* 2448 bytes raw data + P-W subchan */
#define CDR_DB_RAW_PW_R		0x3	/* 2448 bytes raw data + P-W raw sub */
#define CDR_DB_RES_4		0x4	/* reserved */
#define CDR_DB_RES_5		0x5	/* reserved */
#define CDR_DB_RES_6		0x6	/* reserved */
#define CDR_DB_VS_7		0x7	/* vendor specific */
#define CDR_DB_ROM_MODE1	0x8	/* 2048 bytes Mode 1 (ISO/IEC 10149) */
#define CDR_DB_ROM_MODE2	0x9	/* 2336 bytes Mode 2 (ISO/IEC 10149) */
#define CDR_DB_XA_MODE1		0xa	/* 2048 bytes Mode 1 (CD-ROM XA 1) */
#define CDR_DB_XA_MODE2_F1	0xb	/* 2056 bytes Mode 2 (CD-ROM XA 1) */
#define CDR_DB_XA_MODE2_F2	0xc	/* 2324 bytes Mode 2 (CD-ROM XA 2) */
#define CDR_DB_XA_MODE2_MIX	0xd	/* 2332 bytes Mode 2 (CD-ROM XA 1/2) */
#define CDR_DB_RES_14		0xe	/* reserved */
#define CDR_DB_VS_15		0xf	/* vendor specific */

    u_int8_t	reserved4_4567	:4;
    u_int8_t	reserved5;
    u_int8_t	reserved6;
    u_int8_t	host_app_code	:6;	/* host application code */
    u_int8_t	reserved7_67	:2;
    u_int8_t	session_format;		/* session format */
#define CDR_SESS_CDROM		0x00
#define CDR_SESS_CDI		0x10
#define CDR_SESS_CDROM_XA	0x20

    u_int8_t	reserved9;
    u_int32_t	packet_size;		/* packet size in bytes */
    u_int16_t	audio_pause_length;	/* audio pause length in secs */
    u_int8_t	media_catalog_number[16];
    u_int8_t	isr_code[16];
    u_int8_t	sub_hdr_byte0;
    u_int8_t	sub_hdr_byte1;
    u_int8_t	sub_hdr_byte2;
    u_int8_t	sub_hdr_byte3;
/*
    u_int8_t	vendor_specific_byte0;
    u_int8_t	vendor_specific_byte1;
    u_int8_t	vendor_specific_byte2;
    u_int8_t	vendor_specific_byte3;
*/
} __attribute__((packed));

/* CDROM Read Track Information structure */
struct acd_track_info {
    u_int16_t	data_length;
    u_int8_t	track_number;		/* current track number */
    u_int8_t	session_number;		/* current session number */
    u_int8_t	reserved4;
    u_int8_t	track_mode	:4;	/* mode of this track */
    u_int8_t	copy		:1;	/* generation stamp */
    u_int8_t	damage		:1;	/* damaged track */
    u_int8_t	reserved5_67	:2;
    u_int8_t	data_mode	:4;	/* data mode of this disc */
    u_int8_t	fp		:1;	/* fixed packet */
    u_int8_t	packet		:1;	/* packet track */
    u_int8_t	blank		:1;	/* blank (empty) track */
    u_int8_t	rt		:1;	/* reserved track */
    u_int8_t	nwa_valid	:1;	/* next_writeable_addr field valid */
    u_int8_t	reserved7_17	:7;
    u_int	track_start_addr;	/* start of this track */
    u_int	next_writeable_addr;	/* next writeable addr on this disc */
    u_int	free_blocks;		/* free block on this disc */
    u_int	fixed_packet_size;	/* size of packets on this track */
    u_int	track_length;		/* length of this track */
};

/* Structure describing an ATAPI CDROM device */
struct acd_softc {
    struct atapi_softc		*atp;		/* controller structure */
    int				lun;		/* logical device unit */
    int				flags;		/* device state flags */
#define 	F_LOCKED		0x0001	/* this unit is locked */

    struct buf_queue_head	bio_queue;	/* Queue of i/o requests */
    struct toc			toc;		/* table of disc contents */
    struct {
	u_int32_t	volsize;		/* volume size in blocks */
	u_int32_t	blksize;		/* block size in bytes */
    } info;
    struct audiopage		au;		/* audio page info */
    struct audiopage		aumask;		/* audio page mask */
    struct cappage		cap;		/* capabilities page info */
    struct {					/* subchannel info */
	u_int8_t	void0;
	u_int8_t	audio_status;
	u_int16_t	data_length;
	u_int8_t	data_format;
	u_int8_t	control;
	u_int8_t	track;
	u_int8_t	indx;
	u_int32_t	abslba;
	u_int32_t	rellba;
    } subchan;
    struct changer		*changer_info;	/* changer info */
    struct acd_softc		**driver;	/* softc's of changer slots */
    int				slot;		/* this instance slot number */
    time_t			timestamp;	/* this instance timestamp */
    int				block_size;	/* blocksize currently used */
    struct disklabel		disklabel;	/* fake disk label */
    struct devstat		*stats;		/* devstat entry */
    dev_t			dev1, dev2;	/* device place holders */
};
