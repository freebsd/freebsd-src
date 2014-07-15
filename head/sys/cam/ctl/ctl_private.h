/*-
 * Copyright (c) 2003, 2004, 2005, 2008 Silicon Graphics International Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions, and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    substantially similar to the "NO WARRANTY" disclaimer below
 *    ("Disclaimer") and any redistribution must be conditioned upon
 *    including a substantially similar Disclaimer requirement for further
 *    binary redistribution.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGES.
 *
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_private.h#7 $
 * $FreeBSD$
 */
/*
 * CAM Target Layer driver private data structures/definitions.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#ifndef	_CTL_PRIVATE_H_
#define	_CTL_PRIVATE_H_

/*
 * SCSI vendor and product names.
 */
#define	CTL_VENDOR		"FREEBSD "
#define	CTL_DIRECT_PRODUCT	"CTLDISK         "
#define	CTL_PROCESSOR_PRODUCT	"CTLPROCESSOR    "
#define	CTL_UNKNOWN_PRODUCT	"CTLDEVICE       "

struct ctl_fe_ioctl_startstop_info {
	struct cv			sem;
	struct ctl_hard_startstop_info	hs_info;
};

struct ctl_fe_ioctl_bbrread_info {
	struct cv			sem;
	struct ctl_bbrread_info		*bbr_info;
	int				wakeup_done;
	struct mtx			*lock;
};

typedef enum {
	CTL_IOCTL_INPROG,
	CTL_IOCTL_DATAMOVE,
	CTL_IOCTL_DONE
} ctl_fe_ioctl_state;

struct ctl_fe_ioctl_params {
	struct cv		sem;
	struct mtx		ioctl_mtx;
	ctl_fe_ioctl_state	state;
};

#define	CTL_POOL_ENTRIES_INTERNAL	200
#define	CTL_POOL_ENTRIES_EMERGENCY	300
#define CTL_POOL_ENTRIES_OTHER_SC   200

typedef enum {
	CTL_POOL_INTERNAL,
	CTL_POOL_FETD,
	CTL_POOL_EMERGENCY,
	CTL_POOL_4OTHERSC
} ctl_pool_type;

typedef enum {
	CTL_POOL_FLAG_NONE	= 0x00,
	CTL_POOL_FLAG_INVALID	= 0x01
} ctl_pool_flags;

struct ctl_io_pool {
	ctl_pool_type			type;
	ctl_pool_flags			flags;
	uint32_t			id;
	struct ctl_softc		*ctl_softc;
	uint32_t			refcount;
	uint64_t			total_allocated;
	uint64_t			total_freed;
	int32_t				total_ctl_io;
	int32_t				free_ctl_io;
	STAILQ_HEAD(, ctl_io_hdr)	free_queue;
	STAILQ_ENTRY(ctl_io_pool)	links;
};

typedef enum {
	CTL_IOCTL_FLAG_NONE	= 0x00,
	CTL_IOCTL_FLAG_ENABLED	= 0x01
} ctl_ioctl_flags;

struct ctl_ioctl_info {
	ctl_ioctl_flags		flags;
	uint32_t		cur_tag_num;
	struct ctl_port		port;
	char			port_name[24];
};

typedef enum {
	CTL_SER_BLOCK,
	CTL_SER_EXTENT,
	CTL_SER_PASS,
	CTL_SER_SKIP
} ctl_serialize_action;

typedef enum {
	CTL_ACTION_BLOCK,
	CTL_ACTION_OVERLAP,
	CTL_ACTION_OVERLAP_TAG,
	CTL_ACTION_PASS,
	CTL_ACTION_SKIP,
	CTL_ACTION_ERROR
} ctl_action;

/*
 * WARNING:  Keep the bottom nibble here free, we OR in the data direction
 * flags for each command.
 *
 * Note:  "OK_ON_ALL_LUNS" == we don't have to have a lun configured
 *        "OK_ON_BOTH"     == we have to have a lun configured
 *        "SA5"            == command has 5-bit service action at byte 1
 */
typedef enum {
	CTL_CMD_FLAG_NONE		= 0x0000,
	CTL_CMD_FLAG_NO_SENSE		= 0x0010,
	CTL_CMD_FLAG_OK_ON_ALL_LUNS	= 0x0020,
	CTL_CMD_FLAG_ALLOW_ON_RESV	= 0x0040,
	CTL_CMD_FLAG_OK_ON_PROC		= 0x0100,
	CTL_CMD_FLAG_OK_ON_SLUN		= 0x0200,
	CTL_CMD_FLAG_OK_ON_BOTH		= 0x0300,
	CTL_CMD_FLAG_OK_ON_STOPPED	= 0x0400,
	CTL_CMD_FLAG_OK_ON_INOPERABLE	= 0x0800,
	CTL_CMD_FLAG_OK_ON_OFFLINE	= 0x1000,
	CTL_CMD_FLAG_OK_ON_SECONDARY	= 0x2000,
	CTL_CMD_FLAG_ALLOW_ON_PR_RESV	= 0x4000,
	CTL_CMD_FLAG_SA5		= 0x8000
} ctl_cmd_flags;

typedef enum {
	CTL_SERIDX_TUR	= 0,
	CTL_SERIDX_READ,
	CTL_SERIDX_WRITE,
	CTL_SERIDX_UNMAP,
	CTL_SERIDX_MD_SNS,
	CTL_SERIDX_MD_SEL,
	CTL_SERIDX_RQ_SNS,
	CTL_SERIDX_INQ,
	CTL_SERIDX_RD_CAP,
	CTL_SERIDX_RES,
	CTL_SERIDX_LOG_SNS,
	CTL_SERIDX_FORMAT,
	CTL_SERIDX_START,
	/* TBD: others to be filled in as needed */
	CTL_SERIDX_COUNT, /* LAST, not a normal code, provides # codes */
	CTL_SERIDX_INVLD = CTL_SERIDX_COUNT
} ctl_seridx;

typedef int	ctl_opfunc(struct ctl_scsiio *ctsio);

struct ctl_cmd_entry {
	ctl_opfunc		*execute;
	ctl_seridx		seridx;
	ctl_cmd_flags		flags;
	ctl_lun_error_pattern	pattern;
	uint8_t			length;		/* CDB length */
	uint8_t			usage[15];	/* Mask of allowed CDB bits
						 * after the opcode byte. */
};

typedef enum {
	CTL_LUN_NONE		= 0x000,
	CTL_LUN_CONTROL		= 0x001,
	CTL_LUN_RESERVED	= 0x002,
	CTL_LUN_INVALID		= 0x004,
	CTL_LUN_DISABLED	= 0x008,
	CTL_LUN_MALLOCED	= 0x010,
	CTL_LUN_STOPPED		= 0x020,
	CTL_LUN_INOPERABLE	= 0x040,
	CTL_LUN_OFFLINE		= 0x080,
	CTL_LUN_PR_RESERVED	= 0x100,
	CTL_LUN_PRIMARY_SC	= 0x200,
	CTL_LUN_SENSE_DESC	= 0x400
} ctl_lun_flags;

typedef enum {
	CTLBLOCK_FLAG_NONE	= 0x00,
	CTLBLOCK_FLAG_INVALID	= 0x01
} ctlblock_flags;

union ctl_softcs {
	struct ctl_softc	*ctl_softc;
	struct ctlblock_softc	*ctlblock_softc;
};

/*
 * Mode page defaults.
 */
#if 0
/*
 * These values make Solaris trim off some of the capacity.
 */
#define	CTL_DEFAULT_SECTORS_PER_TRACK	63
#define	CTL_DEFAULT_HEADS		255
/*
 * These values seem to work okay.
 */
#define	CTL_DEFAULT_SECTORS_PER_TRACK	63
#define	CTL_DEFAULT_HEADS		16
/*
 * These values work reasonably well.
 */
#define	CTL_DEFAULT_SECTORS_PER_TRACK	512
#define	CTL_DEFAULT_HEADS		64
#endif

/*
 * Solaris is somewhat picky about how many heads and sectors per track you
 * have defined in mode pages 3 and 4.  These values seem to cause Solaris
 * to get the capacity more or less right when you run the format tool.
 * They still have problems when dealing with devices larger than 1TB,
 * but there isn't anything we can do about that.
 *
 * For smaller LUN sizes, this ends up causing the number of cylinders to
 * work out to 0.  Solaris actually recognizes that and comes up with its
 * own bogus geometry to fit the actual capacity of the drive.  They really
 * should just give up on geometry and stick to the read capacity
 * information alone for modern disk drives.
 *
 * One thing worth mentioning about Solaris' mkfs command is that it
 * doesn't like sectors per track values larger than 256.  512 seems to
 * work okay for format, but causes problems when you try to make a
 * filesystem.
 *
 * Another caveat about these values:  the product of these two values
 * really should be a power of 2.  This is because of the simplistic
 * shift-based calculation that we have to use on the i386 platform to
 * calculate the number of cylinders here.  (If you use a divide, you end
 * up calling __udivdi3(), which is a hardware FP call on the PC.  On the
 * XScale, it is done in software, so you can do that from inside the
 * kernel.)
 *
 * So for the current values (256 S/T, 128 H), we get 32768, which works
 * very nicely for calculating cylinders.
 *
 * If you want to change these values so that their product is no longer a
 * power of 2, re-visit the calculation in ctl_init_page_index().  You may
 * need to make it a bit more complicated to get the number of cylinders
 * right.
 */
#define	CTL_DEFAULT_SECTORS_PER_TRACK	256
#define	CTL_DEFAULT_HEADS		128

#define	CTL_DEFAULT_ROTATION_RATE	10000

struct ctl_page_index;

typedef int	ctl_modesen_handler(struct ctl_scsiio *ctsio,
				    struct ctl_page_index *page_index,
				    int pc);
typedef int	ctl_modesel_handler(struct ctl_scsiio *ctsio,
				    struct ctl_page_index *page_index,
				    uint8_t *page_ptr);

typedef enum {
	CTL_PAGE_FLAG_NONE	 = 0x00,
	CTL_PAGE_FLAG_DISK_ONLY	 = 0x01
} ctl_page_flags;

struct ctl_page_index {
	uint8_t			page_code;
	uint8_t			subpage;
	uint16_t		page_len;
	uint8_t			*page_data;
	ctl_page_flags		page_flags;
	ctl_modesen_handler	*sense_handler;
	ctl_modesel_handler	*select_handler;
};

#define	CTL_PAGE_CURRENT	0x00
#define	CTL_PAGE_CHANGEABLE	0x01
#define	CTL_PAGE_DEFAULT	0x02
#define	CTL_PAGE_SAVED		0x03

static const struct ctl_page_index page_index_template[] = {
	{SMS_FORMAT_DEVICE_PAGE, 0, sizeof(struct scsi_format_page), NULL,
	 CTL_PAGE_FLAG_DISK_ONLY, NULL, NULL},
	{SMS_RIGID_DISK_PAGE, 0, sizeof(struct scsi_rigid_disk_page), NULL,
	 CTL_PAGE_FLAG_DISK_ONLY, NULL, NULL},
	{SMS_CACHING_PAGE, 0, sizeof(struct scsi_caching_page), NULL,
	 CTL_PAGE_FLAG_DISK_ONLY, NULL, NULL},
	{SMS_CONTROL_MODE_PAGE, 0, sizeof(struct scsi_control_page), NULL,
	 CTL_PAGE_FLAG_NONE, NULL, ctl_control_page_handler},
   	{SMS_VENDOR_SPECIFIC_PAGE | SMPH_SPF, PWR_SUBPAGE_CODE,
	 sizeof(struct copan_power_subpage), NULL, CTL_PAGE_FLAG_NONE,
	 ctl_power_sp_sense_handler, ctl_power_sp_handler},
	{SMS_VENDOR_SPECIFIC_PAGE | SMPH_SPF, APS_SUBPAGE_CODE,
	 sizeof(struct copan_aps_subpage), NULL, CTL_PAGE_FLAG_NONE,
	 NULL, ctl_aps_sp_handler},
	{SMS_VENDOR_SPECIFIC_PAGE | SMPH_SPF, DBGCNF_SUBPAGE_CODE,
	 sizeof(struct copan_debugconf_subpage), NULL, CTL_PAGE_FLAG_NONE,
	 ctl_debugconf_sp_sense_handler, ctl_debugconf_sp_select_handler},
};

#define	CTL_NUM_MODE_PAGES sizeof(page_index_template)/   \
			   sizeof(page_index_template[0])

struct ctl_mode_pages {
	struct scsi_format_page		format_page[4];
	struct scsi_rigid_disk_page	rigid_disk_page[4];
	struct scsi_caching_page	caching_page[4];
	struct scsi_control_page	control_page[4];
	struct copan_power_subpage	power_subpage[4];
	struct copan_aps_subpage	aps_subpage[4];
	struct copan_debugconf_subpage	debugconf_subpage[4];
	struct ctl_page_index		index[CTL_NUM_MODE_PAGES];
};

struct ctl_pending_sense {
	ctl_ua_type		ua_pending;
	struct scsi_sense_data	sense;
};

struct ctl_lun_delay_info {
	ctl_delay_type		datamove_type;
	uint32_t		datamove_delay;
	ctl_delay_type		done_type;
	uint32_t		done_delay;
};

typedef enum {
	CTL_ERR_INJ_NONE	= 0x00,
	CTL_ERR_INJ_ABORTED	= 0x01
} ctl_err_inject_flags;

typedef enum {
	CTL_PR_FLAG_NONE	= 0x00,
	CTL_PR_FLAG_REGISTERED	= 0x01,
	CTL_PR_FLAG_ACTIVE_RES	= 0x02
} ctl_per_res_flags;

struct ctl_per_res_info {
	struct scsi_per_res_key res_key;
	uint8_t  registered;
};

#define CTL_PR_ALL_REGISTRANTS  0xFFFF
#define CTL_PR_NO_RESERVATION   0xFFF0

struct ctl_devid {
	int		len;
	uint8_t		data[];
};

/*
 * For report target port groups.
 */
#define NUM_TARGET_PORT_GROUPS	2

struct ctl_lun {
	struct mtx			lun_lock;
	struct ctl_id			target;
	uint64_t			lun;
	ctl_lun_flags			flags;
	STAILQ_HEAD(,ctl_error_desc)	error_list;
	uint64_t			error_serial;
	struct ctl_softc		*ctl_softc;
	struct ctl_be_lun		*be_lun;
	struct ctl_backend_driver	*backend;
	int				io_count;
	struct ctl_lun_delay_info	delay_info;
	int				sync_interval;
	int				sync_count;
	TAILQ_HEAD(ctl_ooaq, ctl_io_hdr)  ooa_queue;
	TAILQ_HEAD(ctl_blockq,ctl_io_hdr) blocked_queue;
	STAILQ_ENTRY(ctl_lun)		links;
	STAILQ_ENTRY(ctl_lun)		run_links;
	struct ctl_nexus		rsv_nexus;
	uint32_t			have_ca[CTL_MAX_INITIATORS >> 5];
	struct ctl_pending_sense	pending_sense[CTL_MAX_INITIATORS];
	struct ctl_mode_pages		mode_pages;
	struct ctl_lun_io_stats		stats;
	struct ctl_per_res_info		per_res[2*CTL_MAX_INITIATORS];
	unsigned int			PRGeneration;
	int				pr_key_count;
	uint16_t        		pr_res_idx;
	uint8_t				res_type;
	uint8_t				write_buffer[524288];
	struct ctl_devid		*lun_devid;
};

typedef enum {
	CTL_FLAG_REAL_SYNC	= 0x02,
	CTL_FLAG_MASTER_SHELF	= 0x04
} ctl_gen_flags;

#define CTL_MAX_THREADS		16

struct ctl_thread {
	struct mtx_padalign queue_lock;
	struct ctl_softc	*ctl_softc;
	struct thread		*thread;
	STAILQ_HEAD(, ctl_io_hdr) incoming_queue;
	STAILQ_HEAD(, ctl_io_hdr) rtr_queue;
	STAILQ_HEAD(, ctl_io_hdr) done_queue;
	STAILQ_HEAD(, ctl_io_hdr) isc_queue;
};

struct ctl_softc {
	struct mtx ctl_lock;
	struct cdev *dev;
	int open_count;
	struct ctl_id target;
	int num_disks;
	int num_luns;
	ctl_gen_flags flags;
	ctl_ha_mode ha_mode;
	int inquiry_pq_no_lun;
	struct sysctl_ctx_list sysctl_ctx;
	struct sysctl_oid *sysctl_tree;
	struct ctl_ioctl_info ioctl_info;
	struct ctl_io_pool *internal_pool;
	struct ctl_io_pool *emergency_pool;
	struct ctl_io_pool *othersc_pool;
	struct proc *ctl_proc;
	int targ_online;
	uint32_t ctl_lun_mask[CTL_MAX_LUNS >> 5];
	struct ctl_lun *ctl_luns[CTL_MAX_LUNS];
	uint32_t ctl_port_mask;
	uint64_t aps_locked_lun;
	STAILQ_HEAD(, ctl_lun) lun_list;
	STAILQ_HEAD(, ctl_be_lun) pending_lun_queue;
	uint32_t num_frontends;
	STAILQ_HEAD(, ctl_frontend) fe_list;
	uint32_t num_ports;
	STAILQ_HEAD(, ctl_port) port_list;
	struct ctl_port *ctl_ports[CTL_MAX_PORTS];
	uint32_t num_backends;
	STAILQ_HEAD(, ctl_backend_driver) be_list;
	struct mtx pool_lock;
	uint32_t num_pools;
	uint32_t cur_pool_id;
	STAILQ_HEAD(, ctl_io_pool) io_pools;
	time_t last_print_jiffies;
	uint32_t skipped_prints;
	struct ctl_thread threads[CTL_MAX_THREADS];
};

#ifdef _KERNEL

extern const struct ctl_cmd_entry ctl_cmd_table[256];

uint32_t ctl_get_initindex(struct ctl_nexus *nexus);
int ctl_pool_create(struct ctl_softc *ctl_softc, ctl_pool_type pool_type,
		    uint32_t total_ctl_io, struct ctl_io_pool **npool);
void ctl_pool_free(struct ctl_io_pool *pool);
int ctl_scsi_release(struct ctl_scsiio *ctsio);
int ctl_scsi_reserve(struct ctl_scsiio *ctsio);
int ctl_start_stop(struct ctl_scsiio *ctsio);
int ctl_sync_cache(struct ctl_scsiio *ctsio);
int ctl_format(struct ctl_scsiio *ctsio);
int ctl_read_buffer(struct ctl_scsiio *ctsio);
int ctl_write_buffer(struct ctl_scsiio *ctsio);
int ctl_write_same(struct ctl_scsiio *ctsio);
int ctl_unmap(struct ctl_scsiio *ctsio);
int ctl_mode_select(struct ctl_scsiio *ctsio);
int ctl_mode_sense(struct ctl_scsiio *ctsio);
int ctl_read_capacity(struct ctl_scsiio *ctsio);
int ctl_read_capacity_16(struct ctl_scsiio *ctsio);
int ctl_read_write(struct ctl_scsiio *ctsio);
int ctl_cnw(struct ctl_scsiio *ctsio);
int ctl_report_luns(struct ctl_scsiio *ctsio);
int ctl_request_sense(struct ctl_scsiio *ctsio);
int ctl_tur(struct ctl_scsiio *ctsio);
int ctl_verify(struct ctl_scsiio *ctsio);
int ctl_inquiry(struct ctl_scsiio *ctsio);
int ctl_persistent_reserve_in(struct ctl_scsiio *ctsio);
int ctl_persistent_reserve_out(struct ctl_scsiio *ctsio);
int ctl_report_tagret_port_groups(struct ctl_scsiio *ctsio);
int ctl_report_supported_opcodes(struct ctl_scsiio *ctsio);
int ctl_report_supported_tmf(struct ctl_scsiio *ctsio);
int ctl_report_timestamp(struct ctl_scsiio *ctsio);
int ctl_isc(struct ctl_scsiio *ctsio);

#endif	/* _KERNEL */

#endif	/* _CTL_PRIVATE_H_ */

/*
 * vim: ts=8
 */
