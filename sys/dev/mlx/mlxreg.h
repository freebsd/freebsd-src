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
 *	$FreeBSD$
 */

/*
 * Selected command codes.
 */
#define MLX_CMD_ENQUIRY		0x53
#define MLX_CMD_ENQUIRY2	0x1c
#define MLX_CMD_ENQSYSDRIVE	0x19
#define MLX_CMD_READOLDSG	0xb6
#define MLX_CMD_WRITEOLDSG	0xb7
#define MLX_CMD_FLUSH		0x0a
#define MLX_CMD_LOGOP		0x72
#define MLX_CMD_REBUILDASYNC	0x16
#define MLX_CMD_CHECKASYNC	0x1e
#define MLX_CMD_REBUILDSTAT	0x0c
#define MLX_CMD_STOPCHANNEL	0x13
#define MLX_CMD_STARTCHANNEL	0x12

/*
 * Status values.
 */
#define MLX_STATUS_OK		0x0000
#define MLX_STATUS_RDWROFFLINE	0x0002	/* read/write claims drive is offline */
#define MLX_STATUS_WEDGED	0xdead	/* controller not listening */
#define MLX_STATUS_BUSY		0xffff	/* command is in controller */

/*
 * Scatter-gather list format, type 1, kind 00.
 */
struct mlx_sgentry 
{
    u_int32_t	sg_addr;
    u_int32_t	sg_count;
} __attribute__ ((packed));

/*
 * Command result buffers, as placed in system memory by the controller.
 */
struct mlx_enquiry	/* MLX_CMD_ENQUIRY */
{
    u_int8_t		me_num_sys_drvs;
    u_int8_t		res1[3];
    u_int32_t		me_drvsize[32];
    u_int16_t		me_flash_age;
    u_int8_t		me_status_flags;
#define MLX_ENQ_SFLAG_DEFWRERR	(1<<0)	/* deferred write error indicator */
#define MLX_ENQ_SFLAG_BATTLOW	(1<<1)	/* battery low */
    u_int8_t		res2;
    u_int8_t		me_fwminor;
    u_int8_t		me_fwmajor;
    u_int8_t		me_rebuild_flag;
    u_int8_t		me_max_commands;
    u_int8_t		me_offline_sd_count;
    u_int8_t		res3;
    u_int16_t		me_event_log_seq_num;
    u_int8_t		me_critical_sd_count;
    u_int8_t		res4[3];
    u_int8_t		me_dead_count;
    u_int8_t		res5;
    u_int8_t		me_rebuild_count;
    u_int8_t		me_misc_flags;
#define MLX_ENQ_MISC_BBU	(1<<3)	/* battery backup present */
    struct 
    {
	u_int8_t	dd_targ;
	u_int8_t	dd_chan;
    } __attribute__ ((packed)) me_dead[20];
} __attribute__ ((packed));

struct mlx_enquiry2	/* MLX_CMD_ENQUIRY2 */
{
    u_int32_t		me_hardware_id;
    u_int32_t		me_firmware_id;
    u_int32_t		res1;
    u_int8_t		me_configured_channels;
    u_int8_t		me_actual_channels;
    u_int8_t		me_max_targets;
    u_int8_t		me_max_tags;
    u_int8_t		me_max_sys_drives;
    u_int8_t		me_max_arms;
    u_int8_t		me_max_spans;
    u_int8_t		res2;
    u_int32_t		res3;
    u_int32_t		me_mem_size;
    u_int32_t		me_cache_size;
    u_int32_t		me_flash_size;
    u_int32_t		me_nvram_size;
    u_int16_t		me_mem_type;
    u_int16_t		me_clock_speed;
    u_int16_t		me_mem_speed;
    u_int16_t		me_hardware_speed;
    u_int8_t		res4[10];
    u_int16_t		me_max_commands;
    u_int16_t		me_max_sg;
    u_int16_t		me_max_dp;
    u_int16_t		me_max_iod;
    u_int16_t		me_max_comb;
    u_int8_t		me_latency;
    u_int8_t		res5;
    u_int8_t		me_scsi_timeout;
    u_int8_t		res6;
    u_int16_t		me_min_freelines;
    u_int8_t		res7[8];
    u_int8_t		me_rate_const;
    u_int8_t		res8[11];
    u_int16_t		me_physblk;
    u_int16_t		me_logblk;
    u_int16_t		me_maxblk;
    u_int16_t		me_blocking_factor;
    u_int16_t		me_cacheline;
    u_int8_t		me_scsi_cap;
    u_int8_t		res9[5];
    u_int16_t		me_fimware_build;
    u_int8_t		me_fault_mgmt_type;
    u_int8_t		res10;
    u_int32_t		me_firmware_features;
    u_int8_t		res11[8];
} __attribute__ ((packed));

struct mlx_enq_sys_drive /* MLX_CMD_ENQSYSDRIVE returns an array of 32 of these */
{
    u_int32_t		sd_size;
    u_int8_t		sd_state;
    u_int8_t		sd_raidlevel;
    u_int16_t		res1;
} __attribute__ ((packed));

struct mlx_eventlog_entry	/* MLX_CMD_LOGOP/MLX_LOGOP_GET */
{
    u_int8_t		el_type;
    u_int8_t		el_length;
    u_char		el_target:5;
    u_char		el_channel:3;
    u_char		el_lun:6;
    u_char		res1:2;
    u_int16_t		el_seqno;
    u_char		el_errorcode:7;
    u_char		el_valid:1;
    u_int8_t		el_segment;
    u_char		el_sensekey:4;
    u_char		res2:1;
    u_char		el_ILI:1;
    u_char		el_EOM:1;
    u_char		el_filemark:1;
    u_int8_t		el_information[4];
    u_int8_t		el_addsense;
    u_int8_t		el_csi[4];
    u_int8_t		el_asc;
    u_int8_t		el_asq;
    u_int8_t		res3[12];
} __attribute__ ((packed));

#define MLX_LOGOP_GET		0x00	/* operation codes for MLX_CMD_LOGOP */
#define MLX_LOGMSG_SENSE	0x00	/* log message contents codes */

struct mlx_rebuild_stat	/* MLX_CMD_REBUILDSTAT */
{
    u_int32_t	rb_drive;
    u_int32_t	rb_size;
    u_int32_t	rb_remaining;
} __attribute__ ((packed));

