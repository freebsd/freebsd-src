/*-
 * Copyright (c) 1999 Michael Smith
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      $FreeBSD: src/sys/dev/amr/amrreg.h,v 1.1 1999/10/07 02:23:12 msmith Exp $
 */

/*
 * Mailbox commands
 */
#define AMR_CMD_LREAD	0x01
#define AMR_CMD_LWRITE	0x02
#define AMR_CMD_ENQUIRY	0x05
#define AMR_CMD_FLUSH	0x0a
#define AMR_CMD_CONFIG	0xa1
#define AMR_CONFIG_PRODINFO	0x0e
#define AMR_CONFIG_ENQ3		0x0f
#define AMR_CONFIG_ENQ3_SOLICITED_NOTIFY	0x01
#define AMR_CONFIG_ENQ3_SOLICITED_FULL		0x02
#define AMR_CONFIG_ENQ3_UNSOLICITED		0x03

/*
 * Command results
 */
#define AMR_STATUS_SUCCESS	0x00
#define AMR_STATUS_ABORTED	0x02
#define AMR_STATUS_FAILED	0x80

/*
 * Quartz doorbell registers
 */
#define AMR_QIDB		0x20
#define AMR_QODB		0x2c
#define AMR_QIDB_SUBMIT		0x00000001	/* mailbox ready for work */
#define AMR_QIDB_ACK		0x00000002	/* mailbox done */
#define AMR_QODB_READY		0x10001234	/* work ready to be processed */

/*
 * Standard I/O registers
 */
#define AMR_SCMD		0x10	/* command/ack register (write) */
#define AMR_SMBOX_BUSY		0x10	/* mailbox status (read) */
#define AMR_STOGGLE		0x11	/* interrupt enable bit here */
#define AMR_SMBOX_0		0x14	/* mailbox physical address low byte */
#define AMR_SMBOX_1		0x15
#define AMR_SMBOX_2		0x16
#define AMR_SMBOX_3		0x17	/*                          high byte */
#define AMR_SMBOX_ENABLE	0x18	/* atomic mailbox address enable */
#define AMR_SINTR		0x1a	/* interrupt status */

/*
 * Standard I/O magic numbers
 */
#define AMR_SCMD_POST		0x10	/* -> SCMD to initiate action on mailbox */
#define AMR_SCMD_ACKINTR	0x08	/* -> SCMD to ack mailbox retrieved */
#define AMR_STOGL_IENABLE	0xc0	/* in STOGGLE */
#define AMR_SINTR_VALID		0x40	/* in SINTR */
#define AMR_SMBOX_BUSYFLAG	0x10	/* in SMBOX_BUSY */
#define AMR_SMBOX_ADDR		0x00	/* -> SMBOX_ENABLE */

/*
 * Old Enquiry results
 */
#define AMR_8LD_MAXDRIVES	8
#define AMR_8LD_MAXCHAN		5
#define AMR_8LD_MAXTARG		15
#define AMR_8LD_MAXPHYSDRIVES	(AMR_8LD_MAXCHAN * AMR_8LD_MAXTARG)

struct amr_adapter_info
{
    u_int8_t	aa_maxio;
    u_int8_t	aa_rebuild_rate;
    u_int8_t	aa_maxtargchan;
    u_int8_t	aa_channels;
    u_int8_t	aa_firmware[4];
    u_int16_t	aa_flashage;
    u_int8_t	aa_chipsetvalue;
    u_int8_t	aa_memorysize;
    u_int8_t	aa_cacheflush;
    u_int8_t	aa_bios[4];
    u_int8_t	res1[7];
} __attribute__ ((packed));

struct amr_logdrive_info
{
    u_int8_t	al_numdrives;
    u_int8_t	res1[3];
    u_int32_t	al_size[AMR_8LD_MAXDRIVES];
    u_int8_t	al_properties[AMR_8LD_MAXDRIVES];
    u_int8_t	al_state[AMR_8LD_MAXDRIVES];
} __attribute__ ((packed));

struct amr_physdrive_info
{
    u_int8_t	ap_state[AMR_8LD_MAXPHYSDRIVES];
    u_int8_t	res1;
} __attribute__ ((packed));

struct amr_enquiry
{
    struct amr_adapter_info	ae_adapter;
    struct amr_logdrive_info	ae_ldrv;
    struct amr_physdrive_info	ae_pdrv;
} __attribute__ ((packed));

struct amr_prodinfo
{
    u_int32_t	ap_size;		/* current size in bytes (not including resvd) */
    u_int32_t	ap_configsig;		/* default is 0x00282008, indicating 0x28 maximum
					 * logical drives, 0x20 maximum stripes and 0x08
					 * maximum spans */
    u_int8_t	ap_firmware[16];	/* printable identifiers */
    u_int8_t	ap_bios[16];
    u_int8_t	ap_product[80];
    u_int8_t	ap_maxio;		/* maximum number of concurrent commands supported */
    u_int8_t	ap_nschan;		/* number of SCSI channels present */
    u_int8_t	ap_fcloops;		/* number of fibre loops present */
    u_int8_t	ap_memtype;		/* memory type */
    u_int32_t	ap_signature;
    u_int16_t	ap_memsize;		/* onboard memory in MB */
    u_int16_t	ap_subsystem;		/* subsystem identifier */
    u_int16_t	ap_subvendor;		/* subsystem vendor ID */
    u_int8_t	ap_numnotifyctr;	/* number of notify counters */
} __attribute__((packed));

#define AMR_MBOX_CMDSIZE	0x10	/* portion worth copying for controller */

struct amr_mailbox
{
    u_int8_t	mb_command;
    u_int8_t	mb_ident;
    u_int16_t	mb_blkcount;
    u_int32_t	mb_lba;
    u_int32_t	mb_physaddr;
    u_int8_t	mb_drive;
    u_int8_t	mb_nsgelem;
    u_int8_t	res1;
    u_int8_t	mb_busy;
    u_int8_t	mb_nstatus;
    u_int8_t	mb_status;
    u_int8_t	mb_completed[46];
    u_int8_t	mb_poll;
    u_int8_t	mb_ack;
    u_int8_t	res2[16];
} __attribute__ ((packed));

struct amr_mailbox64
{
    u_int32_t		mb64_segment;	/* for 64-bit controllers */
    struct amr_mailbox	mb;
} __attribute__ ((packed));

struct amr_mailbox_ioctl
{
    u_int8_t	mb_command;
    u_int8_t	mb_ident;
    u_int8_t	mb_channel;
    u_int8_t	mb_param;
    u_int8_t	res1[4];
    u_int32_t	mb_physaddr;
    u_int8_t	mb_drive;
    u_int8_t	mb_nsgelem;
    u_int8_t	res2;
    u_int8_t	mb_busy;
    u_int8_t	mb_nstatus;
    u_int8_t	mb_completed[46];
    u_int8_t	mb_poll;
    u_int8_t	mb_ack;
    u_int8_t	res3[16];
} __attribute__ ((packed));

struct amr_sgentry
{
    u_int32_t	sg_addr;
    u_int32_t	sg_count;
} __attribute__ ((packed));


