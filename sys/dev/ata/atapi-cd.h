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
 *	$Id: atapi-cd.h,v 1.3 1999/03/01 21:03:15 sos Exp sos $
 */

/*
 * CDROM Table Of Contents
 */
#define MAXTRK 99
struct toc {
	struct ioc_toc_header hdr;
	struct cd_toc_entry tab[MAXTRK + 1];
};

/*
 * CDROM Audio Control Parameters Page
 */
struct audiopage {
    /* Mode Page data header */
    u_int16_t	data_length;
    u_int8_t	medium_type;
    u_int8_t	dev_spec;
    u_int8_t	unused[2];
    u_int16_t	blk_desc_len;

    /* Audio control page */
    u_int8_t	page_code;
#define ATAPI_CDROM_AUDIO_PAGE      0x0e
#define ATAPI_CDROM_AUDIO_PAGE_MASK 0x4e

    u_int8_t	param_len;
    u_int8_t	flags;
#define CD_PA_SOTC      0x02
#define CD_PA_IMMED     0x04

    u_int8_t	reserved3;
    u_int8_t	reserved4;
    u_int8_t	reserved5;
    u_int16_t	lb_per_sec;
    struct port_control {
    	u_int8_t channels:4;
#define CHANNEL_0       1
#define CHANNEL_1       2
#define CHANNEL_2       4
#define CHANNEL_3       8
    	u_int8_t volume;
    } port[4];
};

/*
 * CDROM Capabilities and Mechanical Status Page
 */
struct cappage {
    /* Mode data header */
    u_int16_t	data_length;
    u_int8_t	medium_type;		/* Present media type */
#define MST_TYPE_MASK_LOW	0x0f
#define MST_FMT_NONE		0x00
#define MST_DATA_120    	0x01
#define MST_AUDIO_120  		0x02
#define MST_COMB_120    	0x03
#define MST_PHOTO_120   	0x04
#define MST_DATA_80     	0x05
#define MST_AUDIO_80    	0x06
#define MST_COMB_80     	0x07
#define MST_PHOTO_80    	0x08

#define MST_TYPE_MASK_HIGH	0x70
#define MST_CDROM		0x00
#define MST_CDR	 		0x10
#define MST_CDRW		0x20

#define MST_NO_DISC     	0x70
#define MST_DOOR_OPEN  	 	0x71
#define MST_FMT_ERROR   	0x72

    u_int8_t	dev_spec;
    u_int8_t	unused[2];
    u_int16_t	blk_desc_len;

    /* Capabilities page */
    u_int8_t	page_code;
#define ATAPI_CDROM_CAP_PAGE	0x2a

    u_int8_t	param_len;
    u_int8_t	read_cdr	:1;	/* Supports CD-R read */
    u_int8_t	read_cdrw	:1;	/* Supports CD-RW read */
    u_int8_t	method2		:1;	/* Supports reading packet tracks */
    u_int8_t	byte2_37	:5;
    u_int8_t	write_cdr	:1;	/* Supports CD-R write */
    u_int8_t	write_cdrw	:1;	/* Supports CD-RW write */
    u_int8_t	test_write	:1;	/* Supports test writing */
    u_int8_t	byte3_37	:5;
    u_int8_t	audio_play	:1;	/* Audio play supported */
    u_int8_t	composite	:1;	/* Composite audio/video supported */
    u_int8_t	dport1		:1;	/* Digital audio on port 1 */
    u_int8_t	dport2		:1;	/* Digital audio on port 2 */
    u_int8_t	mode2_form1	:1;	/* Mode 2 form 1 (XA) read */
    u_int8_t	mode2_form2	:1;	/* Mode 2 form 2 format */
    u_int8_t	multisession	:1;	/* Multi-session photo-CD */
    u_int8_t			:1;
    u_int8_t	cd_da		:1;	/* Audio-CD read supported */
    u_int8_t	cd_da_stream	:1;	/* CD-DA streaming */
    u_int8_t	rw		:1;	/* Combined R-W subchannels */
    u_int8_t	rw_corr		:1;	/* R-W subchannel data corrected */
    u_int8_t	c2		:1;	/* C2 error pointers supported */
    u_int8_t	isrc		:1;	/* Can return the ISRC info */
    u_int8_t	upc		:1;	/* Can return the catalog number UPC */
    u_int8_t			:1;
    u_int8_t	lock		:1;	/* Can be locked */
    u_int8_t	locked		:1;	/* Current lock state */
    u_int8_t	prevent		:1;	/* Prevent jumper installed */
    u_int8_t	eject		:1;	/* Can eject */
    u_int8_t			:1;
    u_int8_t	mech		:3;	/* Loading mechanism type */
#define MST_MECH_CADDY      	0
#define MST_MECH_TRAY       	1
#define MST_MECH_POPUP      	2
#define MST_MECH_CHANGER    	4
#define MST_MECH_CARTRIDGE  	5

    u_int8_t	sep_vol		:1;	/* Independent volume of channels */
    u_int8_t	sep_mute	:1;	/* Independent mute of channels */
    u_int8_t:6;

    u_int16_t	max_speed;		/* Max raw data rate in bytes/1000 */
    u_int16_t	max_vol_levels;		/* Number of discrete volume levels */
    u_int16_t	buf_size;		/* Internal buffer size in bytes/1024 */
    u_int16_t	cur_speed;		/* Current data rate in bytes/1000  */

    u_int8_t	reserved3;
    u_int8_t	bckf		:1;	/* Data valid on failing edge of BCK */
    u_int8_t	rch		:1;	/* High LRCK indicates left channel */
    u_int8_t	lsbf		:1;	/* Set if LSB first */
    u_int8_t	dlen		:2;
#define MST_DLEN_32         	0
#define MST_DLEN_16         	1
#define MST_DLEN_24         	2
#define MST_DLEN_24_I2S     	3

    u_int8_t			:3;
    u_int8_t	reserved4[2];
};

/*
 * CDROM Changer mechanism status structure
 */
struct changer {
    u_int8_t	current_slot	:5;	/* Active changer slot */
    u_int8_t	mech_state	:2;	/* Current changer state */
#define CH_READY		0
#define CH_LOADING		1
#define CH_UNLOADING		2
#define CH_INITIALIZING		3

    u_int8_t	fault		:1;	/* Fault in last operation */
    u_int8_t	reserved0	:5;
    u_int8_t	cd_state	:3;	/* Current mechanism state */
#define CD_IDLE			0
#define CD_AUDIO_ACTIVE		1
#define CD_AUDIO_SCAN		2
#define CD_HOST_ACTIVE		3
#define CD_NO_STATE		7

    u_int8_t	current_lba[3];		/* Current LBA */
    u_int8_t	slots;			/* Number of available slots */
    u_int16_t	table_length;		/* Slot table length */
    struct {
    	u_int8_t changed	:1;	/* Media has changed in this slot */
    	u_int8_t unused		:6;
    	u_int8_t present	:1;	/* Slot has a CD present */
    	u_int8_t reserved0;
    	u_int8_t reserved1;
    	u_int8_t reserved2;
    } slot[32];
};

/*
 * CDROM Write Parameters Mode Page (Burners ONLY)
 */
struct write_param {
    /* Mode Page data header */
    u_int16_t	data_length;
    u_int8_t	medium_type;
    u_int8_t	dev_spec;
    u_int8_t	unused[2];
    u_int16_t	blk_desc_len;

    /* Write Parameters mode page */
    u_int8_t	page_code;
#define ATAPI_CDROM_WRITE_PARAMETERS_PAGE      0x0e

    u_int8_t	page_length;		/* 0x32 */
    u_int8_t	write_type	:4;	/* Write stream type */
#define	CDR_WTYPE_PACKET	0x00
#define	CDR_WTYPE_TRACK		0x01
#define	CDR_WTYPE_SESSION	0x02
#define	CDR_WTYPE_RAW		0x03

    u_int8_t	test_write	:1;	/* Test write enable */
    u_int8_t	reserved2_567	:3;
    u_int8_t	track_mode	:4;	/* Track mode */
#define CDR_TMODE_AUDIO		0x01
#define CDR_TMODE_INCR_DATA	0x01
#define CDR_TMODE_ALLOW_COPY	0x02
#define CDR_TMODE_DATA		0x04
#define CDR_TMODE_QUAD_AUDIO	0x08

    u_int8_t	copy		:1;	/* Generation stamp */
    u_int8_t	fp		:1;	/* Fixed packet type */
    u_int8_t	multi_session	:2;	/* Multi-session type */
#define	CDR_MSES_NONE		0x00
#define	CDR_MSES_FINAL		0x01
#define	CDR_MSES_RESERVED	0x02
#define	CDR_MSES_NULTI		0x03

    u_int8_t	data_block_type	:4;	/* Data block type code */
#define CDR_DB_RAW		0x0	/* 2352 bytes of raw data */
#define	CDR_DB_RAW_PQ		0x1	/* 2368 bytes raw data + P/Q subchan */
#define	CDR_DB_RAW_PW		0x2	/* 2448 bytes raw data + P-W subchan */
#define	CDR_DB_RAW_PW_R		0x3	/* 2448 bytes raw data + P-W raw sub */
#define	CDR_DB_RES_4		0x4	/* Reserved */
#define	CDR_DB_RES_5		0x5	/* Reserved */
#define	CDR_DB_RES_6		0x6	/* Reserved */
#define	CDR_DB_VS_7		0x7	/* Vendor specific */
#define	CDR_DB_ROM_MODE1	0x8	/* 2048 bytes Mode 1 (ISO/IEC 10149) */
#define	CDR_DB_ROM_MODE2	0x9	/* 2336 bytes Mode 2 (ISO/IEC 10149) */
#define	CDR_DB_XA_MODE1		0x10	/* 2048 bytes Mode 1 (CD-ROM XA 1) */
#define	CDR_DB_XA_MODE2_F1	0x11	/* 2056 bytes Mode 2 (CD-ROM XA 1) */
#define	CDR_DB_XA_MODE2_F2	0x12	/* 2324 bytes Mode 2 (CD-ROM XA 2) */
#define	CDR_DB_XA_MODE2_MIX	0x13	/* 2332 bytes Mode 2 (CD-ROM XA 1/2) */
#define	CDR_DB_RES_14		0x14	/* Reserved */
#define	CDR_DB_VS_15		0x15	/* Vendor specific */

    u_int8_t	reserved4_4567	:4;
    u_int8_t	reserved5;
    u_int8_t	reserved6;
    u_int8_t	host_app_code	:6;	/* Host application code */
    u_int8_t	reserved7_67	:2;
    u_int8_t	session_format;		/* Session format */
#define CDR_SESS_CDROM		0x00
#define CDR_SESS_CDI		0x10
#define CDR_SESS_CDROM_XA	0x20

    u_int8_t	reserved9;
    u_int32_t	packet_size;		/* Packet size in bytes */
    u_int16_t	audio_pause_length;	/* Audio pause length in secs */
    u_int8_t	media_catalog_number[16];
    u_int8_t	isr_code[16];
    u_int8_t	sub_hdr_byte0;
    u_int8_t	sub_hdr_byte1;
    u_int8_t	sub_hdr_byte2;
    u_int8_t	sub_hdr_byte3;
/*
    u_int8_t 	vendor_specific_byte0;
    u_int8_t 	vendor_specific_byte1;
    u_int8_t 	vendor_specific_byte2;
    u_int8_t 	vendor_specific_byte3;
*/

} __attribute__((packed));
/*
 * CDROM Read Track Information structure
 */
struct acd_track_info {
    u_int16_t	data_length;
    u_int8_t	track_number;		/* Current track number */
    u_int8_t	session_number;		/* Current session number */
    u_int8_t	reserved4;
    u_int8_t	track_mode	:4;	/* Mode of this track */
    u_int8_t	copy		:1;	/* Generation stamp */
    u_int8_t	damage		:1;	/* Damaged track */
    u_int8_t	reserved5_67	:2;
    u_int8_t	data_mode	:4;	/* Data mode of this disc */
    u_int8_t	fp		:1;	/* Fixed packet */
    u_int8_t	packet		:1;	/* Packet track */
    u_int8_t	blank		:1;	/* Blank (empty) track */
    u_int8_t	rt		:1;	/* Reserved track */
    u_int8_t	nwa_valid	:1;	/* next_writeable_addr field valid */
    u_int8_t	reserved7_17	:7;
    u_int	track_start_addr;	/* Start of this track */
    u_int	next_writeable_addr;	/* Next writeable addr on this disc */
    u_int	free_blocks;		/* Free block on this disc */
    u_int	fixed_packet_size;	/* Size of packets on this track */
    u_int	track_length;		/* Length of this track */
};

/*
 * Structure describing an ATAPI CDROM device
 */
struct acd_softc {
    struct atapi_softc 		*atp;		/* Controller structure */
    int32_t			lun;		/* Logical device unit */
    int32_t			flags;		/* Device state flags */
    int32_t			refcnt;		/* The number of raw opens */
    struct buf_queue_head	buf_queue;	/* Queue of i/o requests */
    struct toc 			toc;		/* Table of disc contents */
    struct {
    	u_int32_t volsize;			/* Volume size in blocks */
    	u_int32_t blksize;			/* Block size in bytes */
    }      info;
    struct audiopage 		au;		/* Audio page info */
    struct cappage 		cap;		/* Capabilities page info */
    struct audiopage 		aumask;		/* Audio page mask */
    struct {					/* Subchannel info */
    	u_int8_t void0;
    	u_int8_t audio_status;
    	u_int16_t data_length;
    	u_int8_t data_format;
    	u_int8_t control;
    	u_int8_t track;
    	u_int8_t indx;
    	u_int32_t abslba;
    	u_int32_t rellba;
    } subchan;
    struct changer 		*changer_info;	/* Changer info */
    int32_t 			slot;		/* This lun's slot number */
    u_int32_t			block_size;	/* Blocksize currently used */
    u_int8_t			dummy;		/* Use dummy writes */
    u_int8_t			speed;		/* Select drive speed */
    u_int32_t			next_writeable_lba; /* Next writable position */
    struct wormio_prepare_track	preptrack;	/* Scratch region */
    struct devstat 		*stats;		/* devstat entry */
#ifdef	DEVFS
    void *ra_devfs_token;
    void *rc_devfs_token;
    void *a_devfs_token;
    void *c_devfs_token;
#endif
};

#define CDRIOCBLANK     	_IO('c',100)    /* Blank a CDRW disc */
#define CDRIOCNEXTWRITEABLEADDR	_IOR('c',101,int)   
