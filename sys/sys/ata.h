/*-
 * Copyright (c) 2000,2001 Søren Schmidt <sos@FreeBSD.org>
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

#ifndef _SYS_ATA_H_
#define _SYS_ATA_H_

#include <sys/ioccom.h>

/* ATA/ATAPI device parameter information */
struct ata_params {
/*000*/	u_int16_t	packet_size	:2;	/* packet command size */
#define ATAPI_PSIZE_12			0	/* 12 bytes */
#define ATAPI_PSIZE_16			1	/* 16 bytes */

    	u_int16_t	incomplete	:1;
    	u_int16_t			:2;
    	u_int16_t	drq_type	:2;	/* DRQ type */
#define ATAPI_DRQT_MPROC		0	/* cpu	  3 ms delay */
#define ATAPI_DRQT_INTR			1	/* intr	 10 ms delay */
#define ATAPI_DRQT_ACCEL		2	/* accel 50 us delay */

    	u_int16_t	removable	:1;	/* device is removable */
    	u_int16_t	type		:5;	/* device type */
#define ATAPI_TYPE_DIRECT		0	/* disk/floppy */
#define ATAPI_TYPE_TAPE			1	/* streaming tape */
#define ATAPI_TYPE_CDROM		5	/* CD-ROM device */
#define ATAPI_TYPE_OPTICAL		7	/* optical disk */

    	u_int16_t			:2;
    	u_int16_t	cmd_protocol	:1;	/* command protocol */
#define ATA_PROTO_ATA			0
#define ATA_PROTO_ATAPI			1

/*001*/	u_int16_t	cylinders;		/* # of cylinders */
	u_int16_t	reserved2;
/*003*/	u_int16_t	heads;			/* # heads */
	u_int16_t	obsolete4;
	u_int16_t	obsolete5;
/*006*/	u_int16_t	sectors;		/* # sectors/track */
/*007*/	u_int16_t	vendor7[3];
/*010*/	u_int8_t	serial[20];		/* serial number */
	u_int16_t	retired20;
	u_int16_t	retired21;
	u_int16_t	obsolete22;
/*023*/	u_int8_t	revision[8];		/* firmware revision */
/*027*/	u_int8_t	model[40];		/* model name */
/*047*/	u_int16_t	sectors_intr:8;		/* sectors per interrupt */
	u_int16_t	:8;

/*048*/	u_int16_t	usedmovsd;		/* double word read/write? */
/*049*/	u_int16_t	retired49:8;
	u_int16_t	support_dma	:1;	/* DMA supported */
	u_int16_t	support_lba	:1;	/* LBA supported */
	u_int16_t	disable_iordy	:1;	/* IORDY may be disabled */
	u_int16_t	support_iordy	:1;	/* IORDY supported */
	u_int16_t	softreset	:1;	/* needs softreset when busy */
	u_int16_t	stdby_ovlap	:1;	/* standby/overlap supported */
	u_int16_t	support_queueing:1;	/* supports queuing overlap */
	u_int16_t	support_idma	:1;	/* interleaved DMA supported */

/*050*/	u_int16_t	device_stdby_min:1;
	u_int16_t	:13;
	u_int16_t	capability_one:1;
	u_int16_t	capability_zero:1;

/*051*/	u_int16_t	vendor51:8;
	u_int16_t	retired_piomode:8;	/* PIO modes 0-2 */
/*052*/	u_int16_t	vendor52:8;
	u_int16_t	retired_dmamode:8;	/* DMA modes, not ATA-3 */
/*053*/	u_int16_t	atavalid;		/* fields valid */
#define ATA_FLAG_54_58			1	/* words 54-58 valid */
#define ATA_FLAG_64_70			2	/* words 64-70 valid */
#define ATA_FLAG_88			4	/* word 88 valid */

	u_int16_t	obsolete54[5];
/*059*/	u_int16_t	multi_count:8;
	u_int16_t	multi_valid:1;
	u_int16_t	:7;

/*060*/	u_int32_t	lba_size;	
	u_int16_t	obsolete62;
/*063*/	u_int16_t	mwdmamodes;		/* multiword DMA modes */ 
/*064*/	u_int16_t	apiomodes;		/* advanced PIO modes */ 

/*065*/	u_int16_t	mwdmamin;		/* min. M/W DMA time/word ns */
/*066*/	u_int16_t	mwdmarec;		/* rec. M/W DMA time ns */
/*067*/	u_int16_t	pioblind;		/* min. PIO cycle w/o flow */
/*068*/	u_int16_t	pioiordy;		/* min. PIO cycle IORDY flow */
	u_int16_t	reserved69;
	u_int16_t	reserved70;
/*071*/	u_int16_t	rlsovlap;		/* rel time (us) for overlap */
/*072*/	u_int16_t	rlsservice;		/* rel time (us) for service */
	u_int16_t	reserved73;
	u_int16_t	reserved74;

/*075*/	u_int16_t	queuelen:5;
	u_int16_t	:11;

	u_int16_t	reserved76;
	u_int16_t	reserved77;
	u_int16_t	reserved78;
	u_int16_t	reserved79;
/*080*/	u_int16_t	version_major;
/*081*/	u_int16_t	version_minor;
	struct {
/*082/085*/ u_int16_t	smart:1;
	    u_int16_t	security:1;
	    u_int16_t	removable:1;
	    u_int16_t	power_mngt:1;
	    u_int16_t	packet:1;
	    u_int16_t	write_cache:1;
	    u_int16_t	look_ahead:1;
	    u_int16_t	release_irq:1;
	    u_int16_t	service_irq:1;
	    u_int16_t	reset:1;
	    u_int16_t	protected:1;
	    u_int16_t	:1;
	    u_int16_t	write_buffer:1;
	    u_int16_t	read_buffer:1;
	    u_int16_t	nop:1;
	    u_int16_t	:1;

/*083/086*/ u_int16_t	microcode:1;
	    u_int16_t	queued:1;
	    u_int16_t	cfa:1;
	    u_int16_t	apm:1;
	    u_int16_t	notify:1;
	    u_int16_t	standby:1;
	    u_int16_t	spinup:1;
	    u_int16_t	:1;
	    u_int16_t	max_security:1;
	    u_int16_t	auto_acoustic:1;
	    u_int16_t	address48:1;
	    u_int16_t	config_overlay:1;
	    u_int16_t	flush_cache:1;
	    u_int16_t	flush_cache48:1;
	    u_int16_t	support_one:1;
	    u_int16_t	support_zero:1;

/*084/087*/ u_int16_t	smart_error_log:1;
	    u_int16_t	smart_self_test:1;
	    u_int16_t	media_serial_no:1;
	    u_int16_t	media_card_pass:1;
	    u_int16_t	streaming:1;
	    u_int16_t	logging:1;
	    u_int16_t	:8;
	    u_int16_t	extended_one:1;
	    u_int16_t	extended_zero:1;
	} support, enabled;

/*088*/	u_int16_t	udmamodes;		/* UltraDMA modes */
/*089*/	u_int16_t	erase_time;
/*090*/	u_int16_t	enhanced_erase_time;
/*091*/	u_int16_t	apm_value;
/*092*/	u_int16_t	master_passwd_revision;

/*093*/	u_int16_t	hwres_master	:8;
	u_int16_t	hwres_slave	:5;
	u_int16_t	hwres_cblid	:1;
	u_int16_t	hwres_valid:2;

/*094*/	u_int16_t	current_acoustic:8;
	u_int16_t	vendor_acoustic:8;

/*095*/	u_int16_t	stream_min_req_size;
/*096*/	u_int16_t	stream_transfer_time;
/*097*/	u_int16_t	stream_access_latency;
/*098*/	u_int32_t	stream_granularity;
/*100*/	u_int64_t	lba_size48;
	u_int16_t	reserved104[23];
/*127*/	u_int16_t	removable_status;
/*128*/	u_int16_t	security_status;
	u_int16_t	reserved129[31];
/*160*/	u_int16_t	cfa_powermode1;
	u_int16_t	reserved161[14];
/*176*/	u_int16_t	media_serial[30];
	u_int16_t	reserved206[49];
/*255*/	u_int16_t	integrity;
};

#define ATA_MODE_MASK		0x0f
#define ATA_DMA_MASK		0xf0
#define ATA_PIO			0x00
#define ATA_PIO0		0x08
#define ATA_PIO1		0x09
#define ATA_PIO2		0x0a
#define ATA_PIO3		0x0b
#define ATA_PIO4		0x0c
#define ATA_DMA			0x10
#define ATA_WDMA		0x20
#define ATA_WDMA2		0x22
#define ATA_UDMA		0x40
#define ATA_UDMA2		0x42
#define ATA_UDMA4		0x44
#define ATA_UDMA5		0x45
#define ATA_UDMA6		0x46

struct ata_cmd {
    int				channel;
    int				device;
    int				cmd;
#define ATAGPARM		1
#define ATAGMODE		2
#define ATASMODE		3
#define ATAREINIT		4
#define ATAATTACH		5
#define ATADETACH		6
#define ATAPICMD		7
#define ATAREBUILD		8

    union {
	struct {
	    int			mode[2];
	} mode;
	struct {
	    int			type[2];
	    char		name[2][32];
	    struct ata_params	params[2];
	} param;
	struct {
	    char		ccb[16];
	    caddr_t		data;
	    int			count;
	    int			flags;
#define ATAPI_CMD_CTRL			0x00
#define ATAPI_CMD_READ			0x01
#define ATAPI_CMD_WRITE			0x02

	    int			timeout;
	    int			error;
	    char		sense_data[18];
	} atapi;
    } u;
};

#define IOCATA			_IOWR('a',  1, struct ata_cmd)

#endif /* _SYS_ATA_H_ */
