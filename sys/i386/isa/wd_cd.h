/*-
 * Copyright (c) 1998, 1999 Søren Schmidt
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
	u_short data_length;
	u_char medium_type;
	u_char dev_spec;
	u_char unused[2];
	u_short blk_desc_len;

	/* Audio control page */
	u_char page_code;
#define CDROM_AUDIO_PAGE      0x0e
#define CDROM_AUDIO_PAGE_MASK 0x4e

	u_char param_len;
	u_char flags;
#define CD_PA_SOTC      0x02
#define CD_PA_IMMED     0x04

	u_char reserved3;
	u_char reserved4;
	u_char reserved5;
	u_short lb_per_sec;
	struct port_control {
		u_char channels:4;
#define CHANNEL_0       1
#define CHANNEL_1       2
#define CHANNEL_2       4
#define CHANNEL_3       8
		u_char volume;
	}            port[4];
};

/*
 * CDROM Capabilities and Mechanical Status Page
 */
struct cappage {
	/* Mode data header */
	u_short data_length;
	u_char medium_type;		/* Present media type */
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

	u_char dev_spec;
	u_char unused[2];
	u_short blk_desc_len;

	/* Capabilities page */
	u_char page_code;
#define ATAPI_CDROM_CAP_PAGE        0x2a

	u_char param_len;
	u_char read_cdr:1;		/* Supports CD-R read */
	u_char read_cdrw:1;		/* Supports CD-RW read */
	u_char method2:1;		/* Supports reading packet tracks */
	u_char byte2_37:5;
	u_char write_cdr:1;		/* Supports CD-R write */
	u_char write_cdrw:1;		/* Supports CD-RW write */
	u_char test_write:1;		/* Supports test writing */
	u_char byte3_37:5;
	u_char audio_play:1;		/* Audio play supported */
	u_char composite:1;		/* Composite audio/video supported */
	u_char dport1:1;		/* Digital audio on port 1 */
	u_char dport2:1;		/* Digital audio on port 2 */
	u_char mode2_form1:1;		/* Mode 2 form 1 (XA) read */
	u_char mode2_form2:1;		/* Mode 2 form 2 format */
	u_char multisession:1;		/* Multi-session photo-CD */
	u_char:1;
	u_char cd_da:1;			/* Audio-CD read supported */
	u_char cd_da_stream:1;		/* CD-DA streaming */
	u_char rw:1;			/* Combined R-W subchannels */
	u_char rw_corr:1;		/* R-W subchannel data corrected */
	u_char c2:1;			/* C2 error pointers supported */
	u_char isrc:1;			/* Can return the ISRC info */
	u_char upc:1;			/* Can return the catalog number UPC */
	u_char:1;
	u_char lock:1;			/* Can be locked */
	u_char locked:1;		/* Current lock state */
	u_char prevent:1;		/* Prevent jumper installed */
	u_char eject:1;			/* Can eject */
	u_char:1;
	u_char mech:3;			/* Loading mechanism type */
#define MST_MECH_CADDY      0
#define MST_MECH_TRAY       1
#define MST_MECH_POPUP      2
#define MST_MECH_CHANGER    4
#define MST_MECH_CARTRIDGE  5

	u_char sep_vol:1;		/* Independent volume of channels */
	u_char sep_mute:1;		/* Independent mute of channels */
	u_char:6;

	u_short max_speed;		/* Max raw data rate in bytes/1000 */
	u_short max_vol_levels;		/* Number of discrete volume levels */
	u_short buf_size;		/* Internal buffer size in bytes/1024 */
	u_short cur_speed;		/* Current data rate in bytes/1000  */

	u_char reserved3;
	u_char bckf:1;			/* Data valid on failing edge of BCK */
	u_char rch:1;			/* High LRCK indicates left channel */
	u_char lsbf:1;			/* Set if LSB first */
	u_char dlen:2;
#define MST_DLEN_32         0
#define MST_DLEN_16         1
#define MST_DLEN_24         2
#define MST_DLEN_24_I2S     3

	u_char:3;
	u_char reserved4[2];
};

/*
 * CDROM Changer mechanism status structure
 */
struct changer {
	u_char current_slot:5;		/* Active changer slot */
	u_char mech_state:2;		/* Current changer state */
#define CH_READY	0
#define CH_LOADING	1
#define CH_UNLOADING	2
#define CH_INITIALIZING	3

	u_char fault:1;			/* Fault in last operation */
	u_char reserved0:5;
	u_char cd_state:3;		/* Current mechanism state */
#define CD_IDLE		0
#define CD_AUDIO_ACTIVE	1
#define CD_AUDIO_SCAN	2
#define CD_HOST_ACTIVE	3
#define CD_NO_STATE	7

	u_char current_lba[3];		/* Current LBA */
	u_char slots;			/* Number of available slots */
	u_short table_length;		/* Slot table length */
	struct {
		u_char changed:1;	/* Media has changed in this slot */
		u_char unused:6;
		u_char present:1;	/* Slot has a CD present */
		u_char reserved0;
		u_char reserved1;
		u_char reserved2;
	}      slot[32];
};

/*
 * CDROM Write Parameters Mode Page (Burners ONLY)
 */
struct write_param {
	/* Mode Page data header */
	u_short data_length;
	u_char medium_type;
	u_char dev_spec;
	u_char unused[2];
	u_short blk_desc_len;

	/* Write Parameters mode page */
	u_char page_code;		/* 0x05 */
	u_char page_length;		/* 0x32 */
	u_char write_type:4;		/* Write stream type */
#define	CDR_WTYPE_PACKET	0x00
#define	CDR_WTYPE_TRACK		0x01
#define	CDR_WTYPE_SESSION	0x02
#define	CDR_WTYPE_RAW		0x03

	u_char test_write:1;		/* Test write enable */
	u_char reserved2_567:3;
	u_char track_mode:4;		/* Track mode */
#define CDR_TMODE_AUDIO		0x01
#define CDR_TMODE_INCR_DATA	0x01
#define CDR_TMODE_ALLOW_COPY	0x02
#define CDR_TMODE_DATA		0x04
#define CDR_TMODE_QUAD_AUDIO	0x08

	u_char copy:1;			/* Generation stamp */
	u_char fp:1;			/* Fixed packet type */
	u_char multi_session:2;		/* Multi-session type */
#define	CDR_MSES_NONE		0x00
#define	CDR_MSES_FINAL		0x01
#define	CDR_MSES_RESERVED	0x02
#define	CDR_MSES_NULTI		0x03

	u_char data_block_type:4;	/* Data block type code */
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

	u_char reserved4_4567:4;
	u_char reserved5;
	u_char reserved6;
	u_char host_app_code:6;		/* Host application code */
	u_char reserved7_67:2;
	u_char session_format;		/* Session format */
#define CDR_SESS_CDROM		0x00
#define CDR_SESS_CDI		0x10
#define CDR_SESS_CDROM_XA	0x20

	u_char reserved9;
	u_int packet_size;		/* Packet size in bytes */
	u_short audio_pause_length;	/* Audio pause length in secs */
	u_char media_catalog_number[16];
	u_char isr_code[16];
	u_char sub_hdr_byte0;
	u_char sub_hdr_byte1;
	u_char sub_hdr_byte2;
	u_char sub_hdr_byte3;
/*
	u_char 	vendor_specific_byte0;
	u_char 	vendor_specific_byte1;
	u_char 	vendor_specific_byte2;
	u_char 	vendor_specific_byte3;
*/

} __attribute__((packed));
/*
 * CDROM Read Track Information structure
 */
struct acd_track_info {
        u_short data_length;
	u_char	track_number;		/* Current track number */
	u_char	session_number;		/* Current session number */
	u_char	reserved4;
	u_char	track_mode:4;		/* Mode of this track */
	u_char	copy:1;			/* Generation stamp */
	u_char	damage:1;		/* Damaged track */
	u_char	reserved5_67:2;
	u_char	data_mode:4;		/* Data mode of this disc */
	u_char	fp:1;			/* Fixed packet */
	u_char	packet:1;		/* Packet track */
	u_char	blank:1;		/* Blank (empty) track */
	u_char	rt:1;			/* Reserved track */
	u_char	nwa_valid:1;		/* next_writeable_addr field valid */
	u_char	reserved7_17:7;
	u_int	track_start_addr;	/* Start of this track */
	u_int	next_writeable_addr;	/* Next writeable addr on this disc */
	u_int	free_blocks;		/* Free block on this disc */
	u_int	fixed_packet_size;	/* Size of packets on this track */
	u_int	track_length;		/* Length of this track */
};

/*
 * Structure describing an ATAPI CDROM device
 */
struct acd {
	int unit;			/* IDE bus drive unit */
	int lun;			/* Logical device unit */
	int flags;			/* Device state flags */
	int refcnt;			/* The number of raw opens */
	struct atapi *ata;		/* Controller structure */
	struct buf_queue_head buf_queue;	/* Queue of i/o requests */
	struct atapi_params *param;	/* Drive parameters table */
	struct toc toc;			/* Table of disc contents */
	struct {
		u_long volsize;		/* Volume size in blocks */
		u_long blksize;		/* Block size in bytes */
	}      info;
	struct audiopage au;		/* Audio page info */
	struct cappage cap;		/* Capabilities page info */
	struct audiopage aumask;	/* Audio page mask */
	struct {			/* Subchannel info */
		u_char void0;
		u_char audio_status;
		u_short data_length;
		u_char data_format;
		u_char control;
		u_char track;
		u_char indx;
		u_long abslba;
		u_long rellba;
	} subchan;
	struct changer *changer_info;	/* Changer info */
	int slot;			/* This lun's slot number */
	struct devstat *device_stats;	/* Devstat parameters */
	u_int block_size;		/* Blocksize currently used */
	u_char dummy;			/* Use dummy writes */
	u_char speed;			/* Select drive speed */
	u_int next_writeable_lba;	/* Next writable position */
	struct wormio_prepare_track preptrack;	/* Scratch region */
};
