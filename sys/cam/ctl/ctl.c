/*-
 * Copyright (c) 2003-2009 Silicon Graphics International Corp.
 * Copyright (c) 2012 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from the FreeBSD Foundation.
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
 * $Id$
 */
/*
 * CAM Target Layer, a SCSI device emulation subsystem.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */

#define _CTL_C

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/ctype.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/bio.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/module.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/smp.h>
#include <sys/endian.h>
#include <sys/sysctl.h>
#include <vm/uma.h>

#include <cam/cam.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_frontend_internal.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_scsi_all.h>
#include <cam/ctl/ctl_error.h>

struct ctl_softc *control_softc = NULL;

/*
 * Size and alignment macros needed for Copan-specific HA hardware.  These
 * can go away when the HA code is re-written, and uses busdma for any
 * hardware.
 */
#define	CTL_ALIGN_8B(target, source, type)				\
	if (((uint32_t)source & 0x7) != 0)				\
		target = (type)(source + (0x8 - ((uint32_t)source & 0x7)));\
	else								\
		target = (type)source;

#define	CTL_SIZE_8B(target, size)					\
	if ((size & 0x7) != 0)						\
		target = size + (0x8 - (size & 0x7));			\
	else								\
		target = size;

#define CTL_ALIGN_8B_MARGIN	16

/*
 * Template mode pages.
 */

/*
 * Note that these are default values only.  The actual values will be
 * filled in when the user does a mode sense.
 */
const static struct copan_debugconf_subpage debugconf_page_default = {
	DBGCNF_PAGE_CODE | SMPH_SPF,	/* page_code */
	DBGCNF_SUBPAGE_CODE,		/* subpage */
	{(sizeof(struct copan_debugconf_subpage) - 4) >> 8,
	 (sizeof(struct copan_debugconf_subpage) - 4) >> 0}, /* page_length */
	DBGCNF_VERSION,			/* page_version */
	{CTL_TIME_IO_DEFAULT_SECS>>8,
	 CTL_TIME_IO_DEFAULT_SECS>>0},	/* ctl_time_io_secs */
};

const static struct copan_debugconf_subpage debugconf_page_changeable = {
	DBGCNF_PAGE_CODE | SMPH_SPF,	/* page_code */
	DBGCNF_SUBPAGE_CODE,		/* subpage */
	{(sizeof(struct copan_debugconf_subpage) - 4) >> 8,
	 (sizeof(struct copan_debugconf_subpage) - 4) >> 0}, /* page_length */
	0,				/* page_version */
	{0xff,0xff},			/* ctl_time_io_secs */
};

const static struct scsi_da_rw_recovery_page rw_er_page_default = {
	/*page_code*/SMS_RW_ERROR_RECOVERY_PAGE,
	/*page_length*/sizeof(struct scsi_da_rw_recovery_page) - 2,
	/*byte3*/SMS_RWER_AWRE|SMS_RWER_ARRE,
	/*read_retry_count*/0,
	/*correction_span*/0,
	/*head_offset_count*/0,
	/*data_strobe_offset_cnt*/0,
	/*byte8*/SMS_RWER_LBPERE,
	/*write_retry_count*/0,
	/*reserved2*/0,
	/*recovery_time_limit*/{0, 0},
};

const static struct scsi_da_rw_recovery_page rw_er_page_changeable = {
	/*page_code*/SMS_RW_ERROR_RECOVERY_PAGE,
	/*page_length*/sizeof(struct scsi_da_rw_recovery_page) - 2,
	/*byte3*/0,
	/*read_retry_count*/0,
	/*correction_span*/0,
	/*head_offset_count*/0,
	/*data_strobe_offset_cnt*/0,
	/*byte8*/0,
	/*write_retry_count*/0,
	/*reserved2*/0,
	/*recovery_time_limit*/{0, 0},
};

const static struct scsi_format_page format_page_default = {
	/*page_code*/SMS_FORMAT_DEVICE_PAGE,
	/*page_length*/sizeof(struct scsi_format_page) - 2,
	/*tracks_per_zone*/ {0, 0},
	/*alt_sectors_per_zone*/ {0, 0},
	/*alt_tracks_per_zone*/ {0, 0},
	/*alt_tracks_per_lun*/ {0, 0},
	/*sectors_per_track*/ {(CTL_DEFAULT_SECTORS_PER_TRACK >> 8) & 0xff,
			        CTL_DEFAULT_SECTORS_PER_TRACK & 0xff},
	/*bytes_per_sector*/ {0, 0},
	/*interleave*/ {0, 0},
	/*track_skew*/ {0, 0},
	/*cylinder_skew*/ {0, 0},
	/*flags*/ SFP_HSEC,
	/*reserved*/ {0, 0, 0}
};

const static struct scsi_format_page format_page_changeable = {
	/*page_code*/SMS_FORMAT_DEVICE_PAGE,
	/*page_length*/sizeof(struct scsi_format_page) - 2,
	/*tracks_per_zone*/ {0, 0},
	/*alt_sectors_per_zone*/ {0, 0},
	/*alt_tracks_per_zone*/ {0, 0},
	/*alt_tracks_per_lun*/ {0, 0},
	/*sectors_per_track*/ {0, 0},
	/*bytes_per_sector*/ {0, 0},
	/*interleave*/ {0, 0},
	/*track_skew*/ {0, 0},
	/*cylinder_skew*/ {0, 0},
	/*flags*/ 0,
	/*reserved*/ {0, 0, 0}
};

const static struct scsi_rigid_disk_page rigid_disk_page_default = {
	/*page_code*/SMS_RIGID_DISK_PAGE,
	/*page_length*/sizeof(struct scsi_rigid_disk_page) - 2,
	/*cylinders*/ {0, 0, 0},
	/*heads*/ CTL_DEFAULT_HEADS,
	/*start_write_precomp*/ {0, 0, 0},
	/*start_reduced_current*/ {0, 0, 0},
	/*step_rate*/ {0, 0},
	/*landing_zone_cylinder*/ {0, 0, 0},
	/*rpl*/ SRDP_RPL_DISABLED,
	/*rotational_offset*/ 0,
	/*reserved1*/ 0,
	/*rotation_rate*/ {(CTL_DEFAULT_ROTATION_RATE >> 8) & 0xff,
			   CTL_DEFAULT_ROTATION_RATE & 0xff},
	/*reserved2*/ {0, 0}
};

const static struct scsi_rigid_disk_page rigid_disk_page_changeable = {
	/*page_code*/SMS_RIGID_DISK_PAGE,
	/*page_length*/sizeof(struct scsi_rigid_disk_page) - 2,
	/*cylinders*/ {0, 0, 0},
	/*heads*/ 0,
	/*start_write_precomp*/ {0, 0, 0},
	/*start_reduced_current*/ {0, 0, 0},
	/*step_rate*/ {0, 0},
	/*landing_zone_cylinder*/ {0, 0, 0},
	/*rpl*/ 0,
	/*rotational_offset*/ 0,
	/*reserved1*/ 0,
	/*rotation_rate*/ {0, 0},
	/*reserved2*/ {0, 0}
};

const static struct scsi_caching_page caching_page_default = {
	/*page_code*/SMS_CACHING_PAGE,
	/*page_length*/sizeof(struct scsi_caching_page) - 2,
	/*flags1*/ SCP_DISC | SCP_WCE,
	/*ret_priority*/ 0,
	/*disable_pf_transfer_len*/ {0xff, 0xff},
	/*min_prefetch*/ {0, 0},
	/*max_prefetch*/ {0xff, 0xff},
	/*max_pf_ceiling*/ {0xff, 0xff},
	/*flags2*/ 0,
	/*cache_segments*/ 0,
	/*cache_seg_size*/ {0, 0},
	/*reserved*/ 0,
	/*non_cache_seg_size*/ {0, 0, 0}
};

const static struct scsi_caching_page caching_page_changeable = {
	/*page_code*/SMS_CACHING_PAGE,
	/*page_length*/sizeof(struct scsi_caching_page) - 2,
	/*flags1*/ SCP_WCE | SCP_RCD,
	/*ret_priority*/ 0,
	/*disable_pf_transfer_len*/ {0, 0},
	/*min_prefetch*/ {0, 0},
	/*max_prefetch*/ {0, 0},
	/*max_pf_ceiling*/ {0, 0},
	/*flags2*/ 0,
	/*cache_segments*/ 0,
	/*cache_seg_size*/ {0, 0},
	/*reserved*/ 0,
	/*non_cache_seg_size*/ {0, 0, 0}
};

const static struct scsi_control_page control_page_default = {
	/*page_code*/SMS_CONTROL_MODE_PAGE,
	/*page_length*/sizeof(struct scsi_control_page) - 2,
	/*rlec*/0,
	/*queue_flags*/SCP_QUEUE_ALG_RESTRICTED,
	/*eca_and_aen*/0,
	/*flags4*/SCP_TAS,
	/*aen_holdoff_period*/{0, 0},
	/*busy_timeout_period*/{0, 0},
	/*extended_selftest_completion_time*/{0, 0}
};

const static struct scsi_control_page control_page_changeable = {
	/*page_code*/SMS_CONTROL_MODE_PAGE,
	/*page_length*/sizeof(struct scsi_control_page) - 2,
	/*rlec*/SCP_DSENSE,
	/*queue_flags*/SCP_QUEUE_ALG_MASK,
	/*eca_and_aen*/SCP_SWP,
	/*flags4*/0,
	/*aen_holdoff_period*/{0, 0},
	/*busy_timeout_period*/{0, 0},
	/*extended_selftest_completion_time*/{0, 0}
};

const static struct scsi_info_exceptions_page ie_page_default = {
	/*page_code*/SMS_INFO_EXCEPTIONS_PAGE,
	/*page_length*/sizeof(struct scsi_info_exceptions_page) - 2,
	/*info_flags*/SIEP_FLAGS_DEXCPT,
	/*mrie*/0,
	/*interval_timer*/{0, 0, 0, 0},
	/*report_count*/{0, 0, 0, 0}
};

const static struct scsi_info_exceptions_page ie_page_changeable = {
	/*page_code*/SMS_INFO_EXCEPTIONS_PAGE,
	/*page_length*/sizeof(struct scsi_info_exceptions_page) - 2,
	/*info_flags*/0,
	/*mrie*/0,
	/*interval_timer*/{0, 0, 0, 0},
	/*report_count*/{0, 0, 0, 0}
};

#define CTL_LBPM_LEN	(sizeof(struct ctl_logical_block_provisioning_page) - 4)

const static struct ctl_logical_block_provisioning_page lbp_page_default = {{
	/*page_code*/SMS_INFO_EXCEPTIONS_PAGE | SMPH_SPF,
	/*subpage_code*/0x02,
	/*page_length*/{CTL_LBPM_LEN >> 8, CTL_LBPM_LEN},
	/*flags*/0,
	/*reserved*/{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	/*descr*/{}},
	{{/*flags*/0,
	  /*resource*/0x01,
	  /*reserved*/{0, 0},
	  /*count*/{0, 0, 0, 0}},
	 {/*flags*/0,
	  /*resource*/0x02,
	  /*reserved*/{0, 0},
	  /*count*/{0, 0, 0, 0}},
	 {/*flags*/0,
	  /*resource*/0xf1,
	  /*reserved*/{0, 0},
	  /*count*/{0, 0, 0, 0}},
	 {/*flags*/0,
	  /*resource*/0xf2,
	  /*reserved*/{0, 0},
	  /*count*/{0, 0, 0, 0}}
	}
};

const static struct ctl_logical_block_provisioning_page lbp_page_changeable = {{
	/*page_code*/SMS_INFO_EXCEPTIONS_PAGE | SMPH_SPF,
	/*subpage_code*/0x02,
	/*page_length*/{CTL_LBPM_LEN >> 8, CTL_LBPM_LEN},
	/*flags*/0,
	/*reserved*/{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
	/*descr*/{}},
	{{/*flags*/0,
	  /*resource*/0,
	  /*reserved*/{0, 0},
	  /*count*/{0, 0, 0, 0}},
	 {/*flags*/0,
	  /*resource*/0,
	  /*reserved*/{0, 0},
	  /*count*/{0, 0, 0, 0}},
	 {/*flags*/0,
	  /*resource*/0,
	  /*reserved*/{0, 0},
	  /*count*/{0, 0, 0, 0}},
	 {/*flags*/0,
	  /*resource*/0,
	  /*reserved*/{0, 0},
	  /*count*/{0, 0, 0, 0}}
	}
};

/*
 * XXX KDM move these into the softc.
 */
static int rcv_sync_msg;
static uint8_t ctl_pause_rtr;

SYSCTL_NODE(_kern_cam, OID_AUTO, ctl, CTLFLAG_RD, 0, "CAM Target Layer");
static int worker_threads = -1;
SYSCTL_INT(_kern_cam_ctl, OID_AUTO, worker_threads, CTLFLAG_RDTUN,
    &worker_threads, 1, "Number of worker threads");
static int ctl_debug = CTL_DEBUG_NONE;
SYSCTL_INT(_kern_cam_ctl, OID_AUTO, debug, CTLFLAG_RWTUN,
    &ctl_debug, 0, "Enabled debug flags");

/*
 * Supported pages (0x00), Serial number (0x80), Device ID (0x83),
 * Extended INQUIRY Data (0x86), Mode Page Policy (0x87),
 * SCSI Ports (0x88), Third-party Copy (0x8F), Block limits (0xB0),
 * Block Device Characteristics (0xB1) and Logical Block Provisioning (0xB2)
 */
#define SCSI_EVPD_NUM_SUPPORTED_PAGES	10

static void ctl_isc_event_handler(ctl_ha_channel chanel, ctl_ha_event event,
				  int param);
static void ctl_copy_sense_data(union ctl_ha_msg *src, union ctl_io *dest);
static int ctl_init(void);
void ctl_shutdown(void);
static int ctl_open(struct cdev *dev, int flags, int fmt, struct thread *td);
static int ctl_close(struct cdev *dev, int flags, int fmt, struct thread *td);
static void ctl_ioctl_online(void *arg);
static void ctl_ioctl_offline(void *arg);
static int ctl_ioctl_lun_enable(void *arg, int lun_id);
static int ctl_ioctl_lun_disable(void *arg, int lun_id);
static int ctl_ioctl_do_datamove(struct ctl_scsiio *ctsio);
static int ctl_serialize_other_sc_cmd(struct ctl_scsiio *ctsio);
static int ctl_ioctl_submit_wait(union ctl_io *io);
static void ctl_ioctl_datamove(union ctl_io *io);
static void ctl_ioctl_done(union ctl_io *io);
static void ctl_ioctl_hard_startstop_callback(void *arg,
					      struct cfi_metatask *metatask);
static void ctl_ioctl_bbrread_callback(void *arg,struct cfi_metatask *metatask);
static int ctl_ioctl_fill_ooa(struct ctl_lun *lun, uint32_t *cur_fill_num,
			      struct ctl_ooa *ooa_hdr,
			      struct ctl_ooa_entry *kern_entries);
static int ctl_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
		     struct thread *td);
static int ctl_alloc_lun(struct ctl_softc *ctl_softc, struct ctl_lun *lun,
			 struct ctl_be_lun *be_lun);
static int ctl_free_lun(struct ctl_lun *lun);
static void ctl_create_lun(struct ctl_be_lun *be_lun);
static struct ctl_port * ctl_io_port(struct ctl_io_hdr *io_hdr);
/**
static void ctl_failover_change_pages(struct ctl_softc *softc,
				      struct ctl_scsiio *ctsio, int master);
**/

static int ctl_do_mode_select(union ctl_io *io);
static int ctl_pro_preempt(struct ctl_softc *softc, struct ctl_lun *lun,
			   uint64_t res_key, uint64_t sa_res_key,
			   uint8_t type, uint32_t residx,
			   struct ctl_scsiio *ctsio,
			   struct scsi_per_res_out *cdb,
			   struct scsi_per_res_out_parms* param);
static void ctl_pro_preempt_other(struct ctl_lun *lun,
				  union ctl_ha_msg *msg);
static void ctl_hndl_per_res_out_on_other_sc(union ctl_ha_msg *msg);
static int ctl_inquiry_evpd_supported(struct ctl_scsiio *ctsio, int alloc_len);
static int ctl_inquiry_evpd_serial(struct ctl_scsiio *ctsio, int alloc_len);
static int ctl_inquiry_evpd_devid(struct ctl_scsiio *ctsio, int alloc_len);
static int ctl_inquiry_evpd_eid(struct ctl_scsiio *ctsio, int alloc_len);
static int ctl_inquiry_evpd_mpp(struct ctl_scsiio *ctsio, int alloc_len);
static int ctl_inquiry_evpd_scsi_ports(struct ctl_scsiio *ctsio,
					 int alloc_len);
static int ctl_inquiry_evpd_block_limits(struct ctl_scsiio *ctsio,
					 int alloc_len);
static int ctl_inquiry_evpd_bdc(struct ctl_scsiio *ctsio, int alloc_len);
static int ctl_inquiry_evpd_lbp(struct ctl_scsiio *ctsio, int alloc_len);
static int ctl_inquiry_evpd(struct ctl_scsiio *ctsio);
static int ctl_inquiry_std(struct ctl_scsiio *ctsio);
static int ctl_get_lba_len(union ctl_io *io, uint64_t *lba, uint64_t *len);
static ctl_action ctl_extent_check(union ctl_io *io1, union ctl_io *io2,
    bool seq);
static ctl_action ctl_extent_check_seq(union ctl_io *io1, union ctl_io *io2);
static ctl_action ctl_check_for_blockage(struct ctl_lun *lun,
    union ctl_io *pending_io, union ctl_io *ooa_io);
static ctl_action ctl_check_ooa(struct ctl_lun *lun, union ctl_io *pending_io,
				union ctl_io *starting_io);
static int ctl_check_blocked(struct ctl_lun *lun);
static int ctl_scsiio_lun_check(struct ctl_lun *lun,
				const struct ctl_cmd_entry *entry,
				struct ctl_scsiio *ctsio);
//static int ctl_check_rtr(union ctl_io *pending_io, struct ctl_softc *softc);
static void ctl_failover(void);
static void ctl_clear_ua(struct ctl_softc *ctl_softc, uint32_t initidx,
			 ctl_ua_type ua_type);
static int ctl_scsiio_precheck(struct ctl_softc *ctl_softc,
			       struct ctl_scsiio *ctsio);
static int ctl_scsiio(struct ctl_scsiio *ctsio);

static int ctl_bus_reset(struct ctl_softc *ctl_softc, union ctl_io *io);
static int ctl_target_reset(struct ctl_softc *ctl_softc, union ctl_io *io,
			    ctl_ua_type ua_type);
static int ctl_lun_reset(struct ctl_lun *lun, union ctl_io *io,
			 ctl_ua_type ua_type);
static int ctl_abort_task(union ctl_io *io);
static int ctl_abort_task_set(union ctl_io *io);
static int ctl_i_t_nexus_reset(union ctl_io *io);
static void ctl_run_task(union ctl_io *io);
#ifdef CTL_IO_DELAY
static void ctl_datamove_timer_wakeup(void *arg);
static void ctl_done_timer_wakeup(void *arg);
#endif /* CTL_IO_DELAY */

static void ctl_send_datamove_done(union ctl_io *io, int have_lock);
static void ctl_datamove_remote_write_cb(struct ctl_ha_dt_req *rq);
static int ctl_datamove_remote_dm_write_cb(union ctl_io *io);
static void ctl_datamove_remote_write(union ctl_io *io);
static int ctl_datamove_remote_dm_read_cb(union ctl_io *io);
static void ctl_datamove_remote_read_cb(struct ctl_ha_dt_req *rq);
static int ctl_datamove_remote_sgl_setup(union ctl_io *io);
static int ctl_datamove_remote_xfer(union ctl_io *io, unsigned command,
				    ctl_ha_dt_cb callback);
static void ctl_datamove_remote_read(union ctl_io *io);
static void ctl_datamove_remote(union ctl_io *io);
static int ctl_process_done(union ctl_io *io);
static void ctl_lun_thread(void *arg);
static void ctl_thresh_thread(void *arg);
static void ctl_work_thread(void *arg);
static void ctl_enqueue_incoming(union ctl_io *io);
static void ctl_enqueue_rtr(union ctl_io *io);
static void ctl_enqueue_done(union ctl_io *io);
static void ctl_enqueue_isc(union ctl_io *io);
static const struct ctl_cmd_entry *
    ctl_get_cmd_entry(struct ctl_scsiio *ctsio, int *sa);
static const struct ctl_cmd_entry *
    ctl_validate_command(struct ctl_scsiio *ctsio);
static int ctl_cmd_applicable(uint8_t lun_type,
    const struct ctl_cmd_entry *entry);

/*
 * Load the serialization table.  This isn't very pretty, but is probably
 * the easiest way to do it.
 */
#include "ctl_ser_table.c"

/*
 * We only need to define open, close and ioctl routines for this driver.
 */
static struct cdevsw ctl_cdevsw = {
	.d_version =	D_VERSION,
	.d_flags =	0,
	.d_open =	ctl_open,
	.d_close =	ctl_close,
	.d_ioctl =	ctl_ioctl,
	.d_name =	"ctl",
};


MALLOC_DEFINE(M_CTL, "ctlmem", "Memory used for CTL");
MALLOC_DEFINE(M_CTLIO, "ctlio", "Memory used for CTL requests");

static int ctl_module_event_handler(module_t, int /*modeventtype_t*/, void *);

static moduledata_t ctl_moduledata = {
	"ctl",
	ctl_module_event_handler,
	NULL
};

DECLARE_MODULE(ctl, ctl_moduledata, SI_SUB_CONFIGURE, SI_ORDER_THIRD);
MODULE_VERSION(ctl, 1);

static struct ctl_frontend ioctl_frontend =
{
	.name = "ioctl",
};

static void
ctl_isc_handler_finish_xfer(struct ctl_softc *ctl_softc,
			    union ctl_ha_msg *msg_info)
{
	struct ctl_scsiio *ctsio;

	if (msg_info->hdr.original_sc == NULL) {
		printf("%s: original_sc == NULL!\n", __func__);
		/* XXX KDM now what? */
		return;
	}

	ctsio = &msg_info->hdr.original_sc->scsiio;
	ctsio->io_hdr.flags |= CTL_FLAG_IO_ACTIVE;
	ctsio->io_hdr.msg_type = CTL_MSG_FINISH_IO;
	ctsio->io_hdr.status = msg_info->hdr.status;
	ctsio->scsi_status = msg_info->scsi.scsi_status;
	ctsio->sense_len = msg_info->scsi.sense_len;
	ctsio->sense_residual = msg_info->scsi.sense_residual;
	ctsio->residual = msg_info->scsi.residual;
	memcpy(&ctsio->sense_data, &msg_info->scsi.sense_data,
	       sizeof(ctsio->sense_data));
	memcpy(&ctsio->io_hdr.ctl_private[CTL_PRIV_LBA_LEN].bytes,
	       &msg_info->scsi.lbalen, sizeof(msg_info->scsi.lbalen));
	ctl_enqueue_isc((union ctl_io *)ctsio);
}

static void
ctl_isc_handler_finish_ser_only(struct ctl_softc *ctl_softc,
				union ctl_ha_msg *msg_info)
{
	struct ctl_scsiio *ctsio;

	if (msg_info->hdr.serializing_sc == NULL) {
		printf("%s: serializing_sc == NULL!\n", __func__);
		/* XXX KDM now what? */
		return;
	}

	ctsio = &msg_info->hdr.serializing_sc->scsiio;
#if 0
	/*
	 * Attempt to catch the situation where an I/O has
	 * been freed, and we're using it again.
	 */
	if (ctsio->io_hdr.io_type == 0xff) {
		union ctl_io *tmp_io;
		tmp_io = (union ctl_io *)ctsio;
		printf("%s: %p use after free!\n", __func__,
		       ctsio);
		printf("%s: type %d msg %d cdb %x iptl: "
		       "%d:%d:%d:%d tag 0x%04x "
		       "flag %#x status %x\n",
			__func__,
			tmp_io->io_hdr.io_type,
			tmp_io->io_hdr.msg_type,
			tmp_io->scsiio.cdb[0],
			tmp_io->io_hdr.nexus.initid.id,
			tmp_io->io_hdr.nexus.targ_port,
			tmp_io->io_hdr.nexus.targ_target.id,
			tmp_io->io_hdr.nexus.targ_lun,
			(tmp_io->io_hdr.io_type ==
			CTL_IO_TASK) ?
			tmp_io->taskio.tag_num :
			tmp_io->scsiio.tag_num,
		        tmp_io->io_hdr.flags,
			tmp_io->io_hdr.status);
	}
#endif
	ctsio->io_hdr.msg_type = CTL_MSG_FINISH_IO;
	ctl_enqueue_isc((union ctl_io *)ctsio);
}

/*
 * ISC (Inter Shelf Communication) event handler.  Events from the HA
 * subsystem come in here.
 */
static void
ctl_isc_event_handler(ctl_ha_channel channel, ctl_ha_event event, int param)
{
	struct ctl_softc *softc;
	union ctl_io *io;
	struct ctl_prio *presio;
	ctl_ha_status isc_status;

	softc = control_softc;
	io = NULL;


#if 0
	printf("CTL: Isc Msg event %d\n", event);
#endif
	if (event == CTL_HA_EVT_MSG_RECV) {
		union ctl_ha_msg msg_info;

		isc_status = ctl_ha_msg_recv(CTL_HA_CHAN_CTL, &msg_info,
					     sizeof(msg_info), /*wait*/ 0);
#if 0
		printf("CTL: msg_type %d\n", msg_info.msg_type);
#endif
		if (isc_status != 0) {
			printf("Error receiving message, status = %d\n",
			       isc_status);
			return;
		}

		switch (msg_info.hdr.msg_type) {
		case CTL_MSG_SERIALIZE:
#if 0
			printf("Serialize\n");
#endif
			io = ctl_alloc_io_nowait(softc->othersc_pool);
			if (io == NULL) {
				printf("ctl_isc_event_handler: can't allocate "
				       "ctl_io!\n");
				/* Bad Juju */
				/* Need to set busy and send msg back */
				msg_info.hdr.msg_type = CTL_MSG_BAD_JUJU;
				msg_info.hdr.status = CTL_SCSI_ERROR;
				msg_info.scsi.scsi_status = SCSI_STATUS_BUSY;
				msg_info.scsi.sense_len = 0;
			        if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg_info,
				    sizeof(msg_info), 0) > CTL_HA_STATUS_SUCCESS){
				}
				goto bailout;
			}
			ctl_zero_io(io);
			// populate ctsio from msg_info
			io->io_hdr.io_type = CTL_IO_SCSI;
			io->io_hdr.msg_type = CTL_MSG_SERIALIZE;
			io->io_hdr.original_sc = msg_info.hdr.original_sc;
#if 0
			printf("pOrig %x\n", (int)msg_info.original_sc);
#endif
			io->io_hdr.flags |= CTL_FLAG_FROM_OTHER_SC |
					    CTL_FLAG_IO_ACTIVE;
			/*
			 * If we're in serialization-only mode, we don't
			 * want to go through full done processing.  Thus
			 * the COPY flag.
			 *
			 * XXX KDM add another flag that is more specific.
			 */
			if (softc->ha_mode == CTL_HA_MODE_SER_ONLY)
				io->io_hdr.flags |= CTL_FLAG_INT_COPY;
			io->io_hdr.nexus = msg_info.hdr.nexus;
#if 0
			printf("targ %d, port %d, iid %d, lun %d\n",
			       io->io_hdr.nexus.targ_target.id,
			       io->io_hdr.nexus.targ_port,
			       io->io_hdr.nexus.initid.id,
			       io->io_hdr.nexus.targ_lun);
#endif
			io->scsiio.tag_num = msg_info.scsi.tag_num;
			io->scsiio.tag_type = msg_info.scsi.tag_type;
			memcpy(io->scsiio.cdb, msg_info.scsi.cdb,
			       CTL_MAX_CDBLEN);
			if (softc->ha_mode == CTL_HA_MODE_XFER) {
				const struct ctl_cmd_entry *entry;

				entry = ctl_get_cmd_entry(&io->scsiio, NULL);
				io->io_hdr.flags &= ~CTL_FLAG_DATA_MASK;
				io->io_hdr.flags |=
					entry->flags & CTL_FLAG_DATA_MASK;
			}
			ctl_enqueue_isc(io);
			break;

		/* Performed on the Originating SC, XFER mode only */
		case CTL_MSG_DATAMOVE: {
			struct ctl_sg_entry *sgl;
			int i, j;

			io = msg_info.hdr.original_sc;
			if (io == NULL) {
				printf("%s: original_sc == NULL!\n", __func__);
				/* XXX KDM do something here */
				break;
			}
			io->io_hdr.msg_type = CTL_MSG_DATAMOVE;
			io->io_hdr.flags |= CTL_FLAG_IO_ACTIVE;
			/*
			 * Keep track of this, we need to send it back over
			 * when the datamove is complete.
			 */
			io->io_hdr.serializing_sc = msg_info.hdr.serializing_sc;

			if (msg_info.dt.sg_sequence == 0) {
				/*
				 * XXX KDM we use the preallocated S/G list
				 * here, but we'll need to change this to
				 * dynamic allocation if we need larger S/G
				 * lists.
				 */
				if (msg_info.dt.kern_sg_entries >
				    sizeof(io->io_hdr.remote_sglist) /
				    sizeof(io->io_hdr.remote_sglist[0])) {
					printf("%s: number of S/G entries "
					    "needed %u > allocated num %zd\n",
					    __func__,
					    msg_info.dt.kern_sg_entries,
					    sizeof(io->io_hdr.remote_sglist)/
					    sizeof(io->io_hdr.remote_sglist[0]));
				
					/*
					 * XXX KDM send a message back to
					 * the other side to shut down the
					 * DMA.  The error will come back
					 * through via the normal channel.
					 */
					break;
				}
				sgl = io->io_hdr.remote_sglist;
				memset(sgl, 0,
				       sizeof(io->io_hdr.remote_sglist));

				io->scsiio.kern_data_ptr = (uint8_t *)sgl;

				io->scsiio.kern_sg_entries =
					msg_info.dt.kern_sg_entries;
				io->scsiio.rem_sg_entries =
					msg_info.dt.kern_sg_entries;
				io->scsiio.kern_data_len =
					msg_info.dt.kern_data_len;
				io->scsiio.kern_total_len =
					msg_info.dt.kern_total_len;
				io->scsiio.kern_data_resid =
					msg_info.dt.kern_data_resid;
				io->scsiio.kern_rel_offset =
					msg_info.dt.kern_rel_offset;
				/*
				 * Clear out per-DMA flags.
				 */
				io->io_hdr.flags &= ~CTL_FLAG_RDMA_MASK;
				/*
				 * Add per-DMA flags that are set for this
				 * particular DMA request.
				 */
				io->io_hdr.flags |= msg_info.dt.flags &
						    CTL_FLAG_RDMA_MASK;
			} else
				sgl = (struct ctl_sg_entry *)
					io->scsiio.kern_data_ptr;

			for (i = msg_info.dt.sent_sg_entries, j = 0;
			     i < (msg_info.dt.sent_sg_entries +
			     msg_info.dt.cur_sg_entries); i++, j++) {
				sgl[i].addr = msg_info.dt.sg_list[j].addr;
				sgl[i].len = msg_info.dt.sg_list[j].len;

#if 0
				printf("%s: L: %p,%d -> %p,%d j=%d, i=%d\n",
				       __func__,
				       msg_info.dt.sg_list[j].addr,
				       msg_info.dt.sg_list[j].len,
				       sgl[i].addr, sgl[i].len, j, i);
#endif
			}
#if 0
			memcpy(&sgl[msg_info.dt.sent_sg_entries],
			       msg_info.dt.sg_list,
			       sizeof(*sgl) * msg_info.dt.cur_sg_entries);
#endif

			/*
			 * If this is the last piece of the I/O, we've got
			 * the full S/G list.  Queue processing in the thread.
			 * Otherwise wait for the next piece.
			 */
			if (msg_info.dt.sg_last != 0)
				ctl_enqueue_isc(io);
			break;
		}
		/* Performed on the Serializing (primary) SC, XFER mode only */
		case CTL_MSG_DATAMOVE_DONE: {
			if (msg_info.hdr.serializing_sc == NULL) {
				printf("%s: serializing_sc == NULL!\n",
				       __func__);
				/* XXX KDM now what? */
				break;
			}
			/*
			 * We grab the sense information here in case
			 * there was a failure, so we can return status
			 * back to the initiator.
			 */
			io = msg_info.hdr.serializing_sc;
			io->io_hdr.msg_type = CTL_MSG_DATAMOVE_DONE;
			io->io_hdr.status = msg_info.hdr.status;
			io->scsiio.scsi_status = msg_info.scsi.scsi_status;
			io->scsiio.sense_len = msg_info.scsi.sense_len;
			io->scsiio.sense_residual =msg_info.scsi.sense_residual;
			io->io_hdr.port_status = msg_info.scsi.fetd_status;
			io->scsiio.residual = msg_info.scsi.residual;
			memcpy(&io->scsiio.sense_data,&msg_info.scsi.sense_data,
			       sizeof(io->scsiio.sense_data));
			ctl_enqueue_isc(io);
			break;
		}

		/* Preformed on Originating SC, SER_ONLY mode */
		case CTL_MSG_R2R:
			io = msg_info.hdr.original_sc;
			if (io == NULL) {
				printf("%s: Major Bummer\n", __func__);
				return;
			} else {
#if 0
				printf("pOrig %x\n",(int) ctsio);
#endif
			}
			io->io_hdr.msg_type = CTL_MSG_R2R;
			io->io_hdr.serializing_sc = msg_info.hdr.serializing_sc;
			ctl_enqueue_isc(io);
			break;

		/*
		 * Performed on Serializing(i.e. primary SC) SC in SER_ONLY
		 * mode.
		 * Performed on the Originating (i.e. secondary) SC in XFER
		 * mode
		 */
		case CTL_MSG_FINISH_IO:
			if (softc->ha_mode == CTL_HA_MODE_XFER)
				ctl_isc_handler_finish_xfer(softc,
							    &msg_info);
			else
				ctl_isc_handler_finish_ser_only(softc,
								&msg_info);
			break;

		/* Preformed on Originating SC */
		case CTL_MSG_BAD_JUJU:
			io = msg_info.hdr.original_sc;
			if (io == NULL) {
				printf("%s: Bad JUJU!, original_sc is NULL!\n",
				       __func__);
				break;
			}
			ctl_copy_sense_data(&msg_info, io);
			/*
			 * IO should have already been cleaned up on other
			 * SC so clear this flag so we won't send a message
			 * back to finish the IO there.
			 */
			io->io_hdr.flags &= ~CTL_FLAG_SENT_2OTHER_SC;
			io->io_hdr.flags |= CTL_FLAG_IO_ACTIVE;

			/* io = msg_info.hdr.serializing_sc; */
			io->io_hdr.msg_type = CTL_MSG_BAD_JUJU;
			ctl_enqueue_isc(io);
			break;

		/* Handle resets sent from the other side */
		case CTL_MSG_MANAGE_TASKS: {
			struct ctl_taskio *taskio;
			taskio = (struct ctl_taskio *)ctl_alloc_io_nowait(
			    softc->othersc_pool);
			if (taskio == NULL) {
				printf("ctl_isc_event_handler: can't allocate "
				       "ctl_io!\n");
				/* Bad Juju */
				/* should I just call the proper reset func
				   here??? */
				goto bailout;
			}
			ctl_zero_io((union ctl_io *)taskio);
			taskio->io_hdr.io_type = CTL_IO_TASK;
			taskio->io_hdr.flags |= CTL_FLAG_FROM_OTHER_SC;
			taskio->io_hdr.nexus = msg_info.hdr.nexus;
			taskio->task_action = msg_info.task.task_action;
			taskio->tag_num = msg_info.task.tag_num;
			taskio->tag_type = msg_info.task.tag_type;
#ifdef CTL_TIME_IO
			taskio->io_hdr.start_time = time_uptime;
			getbintime(&taskio->io_hdr.start_bt);
#if 0
			cs_prof_gettime(&taskio->io_hdr.start_ticks);
#endif
#endif /* CTL_TIME_IO */
			ctl_run_task((union ctl_io *)taskio);
			break;
		}
		/* Persistent Reserve action which needs attention */
		case CTL_MSG_PERS_ACTION:
			presio = (struct ctl_prio *)ctl_alloc_io_nowait(
			    softc->othersc_pool);
			if (presio == NULL) {
				printf("ctl_isc_event_handler: can't allocate "
				       "ctl_io!\n");
				/* Bad Juju */
				/* Need to set busy and send msg back */
				goto bailout;
			}
			ctl_zero_io((union ctl_io *)presio);
			presio->io_hdr.msg_type = CTL_MSG_PERS_ACTION;
			presio->pr_msg = msg_info.pr;
			ctl_enqueue_isc((union ctl_io *)presio);
			break;
		case CTL_MSG_SYNC_FE:
			rcv_sync_msg = 1;
			break;
		default:
		        printf("How did I get here?\n");
		}
	} else if (event == CTL_HA_EVT_MSG_SENT) {
		if (param != CTL_HA_STATUS_SUCCESS) {
			printf("Bad status from ctl_ha_msg_send status %d\n",
			       param);
		}
		return;
	} else if (event == CTL_HA_EVT_DISCONNECT) {
		printf("CTL: Got a disconnect from Isc\n");
		return;
	} else {
		printf("ctl_isc_event_handler: Unknown event %d\n", event);
		return;
	}

bailout:
	return;
}

static void
ctl_copy_sense_data(union ctl_ha_msg *src, union ctl_io *dest)
{
	struct scsi_sense_data *sense;

	sense = &dest->scsiio.sense_data;
	bcopy(&src->scsi.sense_data, sense, sizeof(*sense));
	dest->scsiio.scsi_status = src->scsi.scsi_status;
	dest->scsiio.sense_len = src->scsi.sense_len;
	dest->io_hdr.status = src->hdr.status;
}

static void
ctl_est_ua(struct ctl_lun *lun, uint32_t initidx, ctl_ua_type ua)
{
	ctl_ua_type *pu;

	mtx_assert(&lun->lun_lock, MA_OWNED);
	pu = lun->pending_ua[initidx / CTL_MAX_INIT_PER_PORT];
	if (pu == NULL)
		return;
	pu[initidx % CTL_MAX_INIT_PER_PORT] |= ua;
}

static void
ctl_est_ua_all(struct ctl_lun *lun, uint32_t except, ctl_ua_type ua)
{
	int i, j;

	mtx_assert(&lun->lun_lock, MA_OWNED);
	for (i = 0; i < CTL_MAX_PORTS; i++) {
		if (lun->pending_ua[i] == NULL)
			continue;
		for (j = 0; j < CTL_MAX_INIT_PER_PORT; j++) {
			if (i * CTL_MAX_INIT_PER_PORT + j == except)
				continue;
			lun->pending_ua[i][j] |= ua;
		}
	}
}

static void
ctl_clr_ua(struct ctl_lun *lun, uint32_t initidx, ctl_ua_type ua)
{
	ctl_ua_type *pu;

	mtx_assert(&lun->lun_lock, MA_OWNED);
	pu = lun->pending_ua[initidx / CTL_MAX_INIT_PER_PORT];
	if (pu == NULL)
		return;
	pu[initidx % CTL_MAX_INIT_PER_PORT] &= ~ua;
}

static void
ctl_clr_ua_all(struct ctl_lun *lun, uint32_t except, ctl_ua_type ua)
{
	int i, j;

	mtx_assert(&lun->lun_lock, MA_OWNED);
	for (i = 0; i < CTL_MAX_PORTS; i++) {
		if (lun->pending_ua[i] == NULL)
			continue;
		for (j = 0; j < CTL_MAX_INIT_PER_PORT; j++) {
			if (i * CTL_MAX_INIT_PER_PORT + j == except)
				continue;
			lun->pending_ua[i][j] &= ~ua;
		}
	}
}

static int
ctl_ha_state_sysctl(SYSCTL_HANDLER_ARGS)
{
	struct ctl_softc *softc = (struct ctl_softc *)arg1;
	struct ctl_lun *lun;
	int error, value;

	if (softc->flags & CTL_FLAG_ACTIVE_SHELF)
		value = 0;
	else
		value = 1;

	error = sysctl_handle_int(oidp, &value, 0, req);
	if ((error != 0) || (req->newptr == NULL))
		return (error);

	mtx_lock(&softc->ctl_lock);
	if (value == 0)
		softc->flags |= CTL_FLAG_ACTIVE_SHELF;
	else
		softc->flags &= ~CTL_FLAG_ACTIVE_SHELF;
	STAILQ_FOREACH(lun, &softc->lun_list, links) {
		mtx_lock(&lun->lun_lock);
		ctl_est_ua_all(lun, -1, CTL_UA_ASYM_ACC_CHANGE);
		mtx_unlock(&lun->lun_lock);
	}
	mtx_unlock(&softc->ctl_lock);
	return (0);
}

static int
ctl_init(void)
{
	struct ctl_softc *softc;
	void *other_pool;
	struct ctl_port *port;
	int i, error, retval;
	//int isc_retval;

	retval = 0;
	ctl_pause_rtr = 0;
        rcv_sync_msg = 0;

	control_softc = malloc(sizeof(*control_softc), M_DEVBUF,
			       M_WAITOK | M_ZERO);
	softc = control_softc;

	softc->dev = make_dev(&ctl_cdevsw, 0, UID_ROOT, GID_OPERATOR, 0600,
			      "cam/ctl");

	softc->dev->si_drv1 = softc;

	/*
	 * By default, return a "bad LUN" peripheral qualifier for unknown
	 * LUNs.  The user can override this default using the tunable or
	 * sysctl.  See the comment in ctl_inquiry_std() for more details.
	 */
	softc->inquiry_pq_no_lun = 1;
	TUNABLE_INT_FETCH("kern.cam.ctl.inquiry_pq_no_lun",
			  &softc->inquiry_pq_no_lun);
	sysctl_ctx_init(&softc->sysctl_ctx);
	softc->sysctl_tree = SYSCTL_ADD_NODE(&softc->sysctl_ctx,
		SYSCTL_STATIC_CHILDREN(_kern_cam), OID_AUTO, "ctl",
		CTLFLAG_RD, 0, "CAM Target Layer");

	if (softc->sysctl_tree == NULL) {
		printf("%s: unable to allocate sysctl tree\n", __func__);
		destroy_dev(softc->dev);
		free(control_softc, M_DEVBUF);
		control_softc = NULL;
		return (ENOMEM);
	}

	SYSCTL_ADD_INT(&softc->sysctl_ctx,
		       SYSCTL_CHILDREN(softc->sysctl_tree), OID_AUTO,
		       "inquiry_pq_no_lun", CTLFLAG_RW,
		       &softc->inquiry_pq_no_lun, 0,
		       "Report no lun possible for invalid LUNs");

	mtx_init(&softc->ctl_lock, "CTL mutex", NULL, MTX_DEF);
	softc->io_zone = uma_zcreate("CTL IO", sizeof(union ctl_io),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	softc->open_count = 0;

	/*
	 * Default to actually sending a SYNCHRONIZE CACHE command down to
	 * the drive.
	 */
	softc->flags = CTL_FLAG_REAL_SYNC;

	/*
	 * In Copan's HA scheme, the "master" and "slave" roles are
	 * figured out through the slot the controller is in.  Although it
	 * is an active/active system, someone has to be in charge.
	 */
	SYSCTL_ADD_INT(&softc->sysctl_ctx, SYSCTL_CHILDREN(softc->sysctl_tree),
	    OID_AUTO, "ha_id", CTLFLAG_RDTUN, &softc->ha_id, 0,
	    "HA head ID (0 - no HA)");
	if (softc->ha_id == 0) {
		softc->flags |= CTL_FLAG_ACTIVE_SHELF;
		softc->is_single = 1;
		softc->port_offset = 0;
	} else
		softc->port_offset = (softc->ha_id - 1) * CTL_MAX_PORTS;
	softc->persis_offset = softc->port_offset * CTL_MAX_INIT_PER_PORT;

	STAILQ_INIT(&softc->lun_list);
	STAILQ_INIT(&softc->pending_lun_queue);
	STAILQ_INIT(&softc->fe_list);
	STAILQ_INIT(&softc->port_list);
	STAILQ_INIT(&softc->be_list);
	ctl_tpc_init(softc);

	if (ctl_pool_create(softc, "othersc", CTL_POOL_ENTRIES_OTHER_SC,
	                    &other_pool) != 0)
	{
		printf("ctl: can't allocate %d entry other SC pool, "
		       "exiting\n", CTL_POOL_ENTRIES_OTHER_SC);
		return (ENOMEM);
	}
	softc->othersc_pool = other_pool;

	if (worker_threads <= 0)
		worker_threads = max(1, mp_ncpus / 4);
	if (worker_threads > CTL_MAX_THREADS)
		worker_threads = CTL_MAX_THREADS;

	for (i = 0; i < worker_threads; i++) {
		struct ctl_thread *thr = &softc->threads[i];

		mtx_init(&thr->queue_lock, "CTL queue mutex", NULL, MTX_DEF);
		thr->ctl_softc = softc;
		STAILQ_INIT(&thr->incoming_queue);
		STAILQ_INIT(&thr->rtr_queue);
		STAILQ_INIT(&thr->done_queue);
		STAILQ_INIT(&thr->isc_queue);

		error = kproc_kthread_add(ctl_work_thread, thr,
		    &softc->ctl_proc, &thr->thread, 0, 0, "ctl", "work%d", i);
		if (error != 0) {
			printf("error creating CTL work thread!\n");
			ctl_pool_free(other_pool);
			return (error);
		}
	}
	error = kproc_kthread_add(ctl_lun_thread, softc,
	    &softc->ctl_proc, NULL, 0, 0, "ctl", "lun");
	if (error != 0) {
		printf("error creating CTL lun thread!\n");
		ctl_pool_free(other_pool);
		return (error);
	}
	error = kproc_kthread_add(ctl_thresh_thread, softc,
	    &softc->ctl_proc, NULL, 0, 0, "ctl", "thresh");
	if (error != 0) {
		printf("error creating CTL threshold thread!\n");
		ctl_pool_free(other_pool);
		return (error);
	}
	if (bootverbose)
		printf("ctl: CAM Target Layer loaded\n");

	/*
	 * Initialize the ioctl front end.
	 */
	ctl_frontend_register(&ioctl_frontend);
	port = &softc->ioctl_info.port;
	port->frontend = &ioctl_frontend;
	sprintf(softc->ioctl_info.port_name, "ioctl");
	port->port_type = CTL_PORT_IOCTL;
	port->num_requested_ctl_io = 100;
	port->port_name = softc->ioctl_info.port_name;
	port->port_online = ctl_ioctl_online;
	port->port_offline = ctl_ioctl_offline;
	port->onoff_arg = &softc->ioctl_info;
	port->lun_enable = ctl_ioctl_lun_enable;
	port->lun_disable = ctl_ioctl_lun_disable;
	port->targ_lun_arg = &softc->ioctl_info;
	port->fe_datamove = ctl_ioctl_datamove;
	port->fe_done = ctl_ioctl_done;
	port->max_targets = 15;
	port->max_target_id = 15;

	if (ctl_port_register(&softc->ioctl_info.port) != 0) {
		printf("ctl: ioctl front end registration failed, will "
		       "continue anyway\n");
	}

	SYSCTL_ADD_PROC(&softc->sysctl_ctx,SYSCTL_CHILDREN(softc->sysctl_tree),
	    OID_AUTO, "ha_state", CTLTYPE_INT | CTLFLAG_RWTUN,
	    softc, 0, ctl_ha_state_sysctl, "I", "HA state for this head");

#ifdef CTL_IO_DELAY
	if (sizeof(struct callout) > CTL_TIMER_BYTES) {
		printf("sizeof(struct callout) %zd > CTL_TIMER_BYTES %zd\n",
		       sizeof(struct callout), CTL_TIMER_BYTES);
		return (EINVAL);
	}
#endif /* CTL_IO_DELAY */

	return (0);
}

void
ctl_shutdown(void)
{
	struct ctl_softc *softc;
	struct ctl_lun *lun, *next_lun;

	softc = (struct ctl_softc *)control_softc;

	if (ctl_port_deregister(&softc->ioctl_info.port) != 0)
		printf("ctl: ioctl front end deregistration failed\n");

	mtx_lock(&softc->ctl_lock);

	/*
	 * Free up each LUN.
	 */
	for (lun = STAILQ_FIRST(&softc->lun_list); lun != NULL; lun = next_lun){
		next_lun = STAILQ_NEXT(lun, links);
		ctl_free_lun(lun);
	}

	mtx_unlock(&softc->ctl_lock);

	ctl_frontend_deregister(&ioctl_frontend);

#if 0
	ctl_shutdown_thread(softc->work_thread);
	mtx_destroy(&softc->queue_lock);
#endif

	ctl_tpc_shutdown(softc);
	uma_zdestroy(softc->io_zone);
	mtx_destroy(&softc->ctl_lock);

	destroy_dev(softc->dev);

	sysctl_ctx_free(&softc->sysctl_ctx);

	free(control_softc, M_DEVBUF);
	control_softc = NULL;

	if (bootverbose)
		printf("ctl: CAM Target Layer unloaded\n");
}

static int
ctl_module_event_handler(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		return (ctl_init());
	case MOD_UNLOAD:
		return (EBUSY);
	default:
		return (EOPNOTSUPP);
	}
}

/*
 * XXX KDM should we do some access checks here?  Bump a reference count to
 * prevent a CTL module from being unloaded while someone has it open?
 */
static int
ctl_open(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	return (0);
}

static int
ctl_close(struct cdev *dev, int flags, int fmt, struct thread *td)
{
	return (0);
}

int
ctl_port_enable(ctl_port_type port_type)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_port *port;

	if (softc->is_single == 0) {
		union ctl_ha_msg msg_info;
		int isc_retval;

#if 0
		printf("%s: HA mode, synchronizing frontend enable\n",
		        __func__);
#endif
		msg_info.hdr.msg_type = CTL_MSG_SYNC_FE;
	        if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg_info,
		        sizeof(msg_info), 1 )) > CTL_HA_STATUS_SUCCESS) {
			printf("Sync msg send error retval %d\n", isc_retval);
		}
		if (!rcv_sync_msg) {
			isc_retval=ctl_ha_msg_recv(CTL_HA_CHAN_CTL, &msg_info,
			        sizeof(msg_info), 1);
		}
#if 0
        	printf("CTL:Frontend Enable\n");
	} else {
		printf("%s: single mode, skipping frontend synchronization\n",
		        __func__);
#endif
	}

	STAILQ_FOREACH(port, &softc->port_list, links) {
		if (port_type & port->port_type)
		{
#if 0
			printf("port %d\n", port->targ_port);
#endif
			ctl_port_online(port);
		}
	}

	return (0);
}

int
ctl_port_disable(ctl_port_type port_type)
{
	struct ctl_softc *softc;
	struct ctl_port *port;

	softc = control_softc;

	STAILQ_FOREACH(port, &softc->port_list, links) {
		if (port_type & port->port_type)
			ctl_port_offline(port);
	}

	return (0);
}

/*
 * Returns 0 for success, 1 for failure.
 * Currently the only failure mode is if there aren't enough entries
 * allocated.  So, in case of a failure, look at num_entries_dropped,
 * reallocate and try again.
 */
int
ctl_port_list(struct ctl_port_entry *entries, int num_entries_alloced,
	      int *num_entries_filled, int *num_entries_dropped,
	      ctl_port_type port_type, int no_virtual)
{
	struct ctl_softc *softc;
	struct ctl_port *port;
	int entries_dropped, entries_filled;
	int retval;
	int i;

	softc = control_softc;

	retval = 0;
	entries_filled = 0;
	entries_dropped = 0;

	i = 0;
	mtx_lock(&softc->ctl_lock);
	STAILQ_FOREACH(port, &softc->port_list, links) {
		struct ctl_port_entry *entry;

		if ((port->port_type & port_type) == 0)
			continue;

		if ((no_virtual != 0)
		 && (port->virtual_port != 0))
			continue;

		if (entries_filled >= num_entries_alloced) {
			entries_dropped++;
			continue;
		}
		entry = &entries[i];

		entry->port_type = port->port_type;
		strlcpy(entry->port_name, port->port_name,
			sizeof(entry->port_name));
		entry->physical_port = port->physical_port;
		entry->virtual_port = port->virtual_port;
		entry->wwnn = port->wwnn;
		entry->wwpn = port->wwpn;

		i++;
		entries_filled++;
	}

	mtx_unlock(&softc->ctl_lock);

	if (entries_dropped > 0)
		retval = 1;

	*num_entries_dropped = entries_dropped;
	*num_entries_filled = entries_filled;

	return (retval);
}

static void
ctl_ioctl_online(void *arg)
{
	struct ctl_ioctl_info *ioctl_info;

	ioctl_info = (struct ctl_ioctl_info *)arg;

	ioctl_info->flags |= CTL_IOCTL_FLAG_ENABLED;
}

static void
ctl_ioctl_offline(void *arg)
{
	struct ctl_ioctl_info *ioctl_info;

	ioctl_info = (struct ctl_ioctl_info *)arg;

	ioctl_info->flags &= ~CTL_IOCTL_FLAG_ENABLED;
}

/*
 * Remove an initiator by port number and initiator ID.
 * Returns 0 for success, -1 for failure.
 */
int
ctl_remove_initiator(struct ctl_port *port, int iid)
{
	struct ctl_softc *softc = control_softc;

	mtx_assert(&softc->ctl_lock, MA_NOTOWNED);

	if (iid > CTL_MAX_INIT_PER_PORT) {
		printf("%s: initiator ID %u > maximun %u!\n",
		       __func__, iid, CTL_MAX_INIT_PER_PORT);
		return (-1);
	}

	mtx_lock(&softc->ctl_lock);
	port->wwpn_iid[iid].in_use--;
	port->wwpn_iid[iid].last_use = time_uptime;
	mtx_unlock(&softc->ctl_lock);

	return (0);
}

/*
 * Add an initiator to the initiator map.
 * Returns iid for success, < 0 for failure.
 */
int
ctl_add_initiator(struct ctl_port *port, int iid, uint64_t wwpn, char *name)
{
	struct ctl_softc *softc = control_softc;
	time_t best_time;
	int i, best;

	mtx_assert(&softc->ctl_lock, MA_NOTOWNED);

	if (iid >= CTL_MAX_INIT_PER_PORT) {
		printf("%s: WWPN %#jx initiator ID %u > maximum %u!\n",
		       __func__, wwpn, iid, CTL_MAX_INIT_PER_PORT);
		free(name, M_CTL);
		return (-1);
	}

	mtx_lock(&softc->ctl_lock);

	if (iid < 0 && (wwpn != 0 || name != NULL)) {
		for (i = 0; i < CTL_MAX_INIT_PER_PORT; i++) {
			if (wwpn != 0 && wwpn == port->wwpn_iid[i].wwpn) {
				iid = i;
				break;
			}
			if (name != NULL && port->wwpn_iid[i].name != NULL &&
			    strcmp(name, port->wwpn_iid[i].name) == 0) {
				iid = i;
				break;
			}
		}
	}

	if (iid < 0) {
		for (i = 0; i < CTL_MAX_INIT_PER_PORT; i++) {
			if (port->wwpn_iid[i].in_use == 0 &&
			    port->wwpn_iid[i].wwpn == 0 &&
			    port->wwpn_iid[i].name == NULL) {
				iid = i;
				break;
			}
		}
	}

	if (iid < 0) {
		best = -1;
		best_time = INT32_MAX;
		for (i = 0; i < CTL_MAX_INIT_PER_PORT; i++) {
			if (port->wwpn_iid[i].in_use == 0) {
				if (port->wwpn_iid[i].last_use < best_time) {
					best = i;
					best_time = port->wwpn_iid[i].last_use;
				}
			}
		}
		iid = best;
	}

	if (iid < 0) {
		mtx_unlock(&softc->ctl_lock);
		free(name, M_CTL);
		return (-2);
	}

	if (port->wwpn_iid[iid].in_use > 0 && (wwpn != 0 || name != NULL)) {
		/*
		 * This is not an error yet.
		 */
		if (wwpn != 0 && wwpn == port->wwpn_iid[iid].wwpn) {
#if 0
			printf("%s: port %d iid %u WWPN %#jx arrived"
			    " again\n", __func__, port->targ_port,
			    iid, (uintmax_t)wwpn);
#endif
			goto take;
		}
		if (name != NULL && port->wwpn_iid[iid].name != NULL &&
		    strcmp(name, port->wwpn_iid[iid].name) == 0) {
#if 0
			printf("%s: port %d iid %u name '%s' arrived"
			    " again\n", __func__, port->targ_port,
			    iid, name);
#endif
			goto take;
		}

		/*
		 * This is an error, but what do we do about it?  The
		 * driver is telling us we have a new WWPN for this
		 * initiator ID, so we pretty much need to use it.
		 */
		printf("%s: port %d iid %u WWPN %#jx '%s' arrived,"
		    " but WWPN %#jx '%s' is still at that address\n",
		    __func__, port->targ_port, iid, wwpn, name,
		    (uintmax_t)port->wwpn_iid[iid].wwpn,
		    port->wwpn_iid[iid].name);

		/*
		 * XXX KDM clear have_ca and ua_pending on each LUN for
		 * this initiator.
		 */
	}
take:
	free(port->wwpn_iid[iid].name, M_CTL);
	port->wwpn_iid[iid].name = name;
	port->wwpn_iid[iid].wwpn = wwpn;
	port->wwpn_iid[iid].in_use++;
	mtx_unlock(&softc->ctl_lock);

	return (iid);
}

static int
ctl_create_iid(struct ctl_port *port, int iid, uint8_t *buf)
{
	int len;

	switch (port->port_type) {
	case CTL_PORT_FC:
	{
		struct scsi_transportid_fcp *id =
		    (struct scsi_transportid_fcp *)buf;
		if (port->wwpn_iid[iid].wwpn == 0)
			return (0);
		memset(id, 0, sizeof(*id));
		id->format_protocol = SCSI_PROTO_FC;
		scsi_u64to8b(port->wwpn_iid[iid].wwpn, id->n_port_name);
		return (sizeof(*id));
	}
	case CTL_PORT_ISCSI:
	{
		struct scsi_transportid_iscsi_port *id =
		    (struct scsi_transportid_iscsi_port *)buf;
		if (port->wwpn_iid[iid].name == NULL)
			return (0);
		memset(id, 0, 256);
		id->format_protocol = SCSI_TRN_ISCSI_FORMAT_PORT |
		    SCSI_PROTO_ISCSI;
		len = strlcpy(id->iscsi_name, port->wwpn_iid[iid].name, 252) + 1;
		len = roundup2(min(len, 252), 4);
		scsi_ulto2b(len, id->additional_length);
		return (sizeof(*id) + len);
	}
	case CTL_PORT_SAS:
	{
		struct scsi_transportid_sas *id =
		    (struct scsi_transportid_sas *)buf;
		if (port->wwpn_iid[iid].wwpn == 0)
			return (0);
		memset(id, 0, sizeof(*id));
		id->format_protocol = SCSI_PROTO_SAS;
		scsi_u64to8b(port->wwpn_iid[iid].wwpn, id->sas_address);
		return (sizeof(*id));
	}
	default:
	{
		struct scsi_transportid_spi *id =
		    (struct scsi_transportid_spi *)buf;
		memset(id, 0, sizeof(*id));
		id->format_protocol = SCSI_PROTO_SPI;
		scsi_ulto2b(iid, id->scsi_addr);
		scsi_ulto2b(port->targ_port, id->rel_trgt_port_id);
		return (sizeof(*id));
	}
	}
}

static int
ctl_ioctl_lun_enable(void *arg, int lun_id)
{
	return (0);
}

static int
ctl_ioctl_lun_disable(void *arg, int lun_id)
{
	return (0);
}

/*
 * Data movement routine for the CTL ioctl frontend port.
 */
static int
ctl_ioctl_do_datamove(struct ctl_scsiio *ctsio)
{
	struct ctl_sg_entry *ext_sglist, *kern_sglist;
	struct ctl_sg_entry ext_entry, kern_entry;
	int ext_sglen, ext_sg_entries, kern_sg_entries;
	int ext_sg_start, ext_offset;
	int len_to_copy, len_copied;
	int kern_watermark, ext_watermark;
	int ext_sglist_malloced;
	int i, j;

	ext_sglist_malloced = 0;
	ext_sg_start = 0;
	ext_offset = 0;

	CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove\n"));

	/*
	 * If this flag is set, fake the data transfer.
	 */
	if (ctsio->io_hdr.flags & CTL_FLAG_NO_DATAMOVE) {
		ctsio->ext_data_filled = ctsio->ext_data_len;
		goto bailout;
	}

	/*
	 * To simplify things here, if we have a single buffer, stick it in
	 * a S/G entry and just make it a single entry S/G list.
	 */
	if (ctsio->io_hdr.flags & CTL_FLAG_EDPTR_SGLIST) {
		int len_seen;

		ext_sglen = ctsio->ext_sg_entries * sizeof(*ext_sglist);

		ext_sglist = (struct ctl_sg_entry *)malloc(ext_sglen, M_CTL,
							   M_WAITOK);
		ext_sglist_malloced = 1;
		if (copyin(ctsio->ext_data_ptr, ext_sglist,
				   ext_sglen) != 0) {
			ctl_set_internal_failure(ctsio,
						 /*sks_valid*/ 0,
						 /*retry_count*/ 0);
			goto bailout;
		}
		ext_sg_entries = ctsio->ext_sg_entries;
		len_seen = 0;
		for (i = 0; i < ext_sg_entries; i++) {
			if ((len_seen + ext_sglist[i].len) >=
			     ctsio->ext_data_filled) {
				ext_sg_start = i;
				ext_offset = ctsio->ext_data_filled - len_seen;
				break;
			}
			len_seen += ext_sglist[i].len;
		}
	} else {
		ext_sglist = &ext_entry;
		ext_sglist->addr = ctsio->ext_data_ptr;
		ext_sglist->len = ctsio->ext_data_len;
		ext_sg_entries = 1;
		ext_sg_start = 0;
		ext_offset = ctsio->ext_data_filled;
	}

	if (ctsio->kern_sg_entries > 0) {
		kern_sglist = (struct ctl_sg_entry *)ctsio->kern_data_ptr;
		kern_sg_entries = ctsio->kern_sg_entries;
	} else {
		kern_sglist = &kern_entry;
		kern_sglist->addr = ctsio->kern_data_ptr;
		kern_sglist->len = ctsio->kern_data_len;
		kern_sg_entries = 1;
	}


	kern_watermark = 0;
	ext_watermark = ext_offset;
	len_copied = 0;
	for (i = ext_sg_start, j = 0;
	     i < ext_sg_entries && j < kern_sg_entries;) {
		uint8_t *ext_ptr, *kern_ptr;

		len_to_copy = MIN(ext_sglist[i].len - ext_watermark,
				  kern_sglist[j].len - kern_watermark);

		ext_ptr = (uint8_t *)ext_sglist[i].addr;
		ext_ptr = ext_ptr + ext_watermark;
		if (ctsio->io_hdr.flags & CTL_FLAG_BUS_ADDR) {
			/*
			 * XXX KDM fix this!
			 */
			panic("need to implement bus address support");
#if 0
			kern_ptr = bus_to_virt(kern_sglist[j].addr);
#endif
		} else
			kern_ptr = (uint8_t *)kern_sglist[j].addr;
		kern_ptr = kern_ptr + kern_watermark;

		kern_watermark += len_to_copy;
		ext_watermark += len_to_copy;

		if ((ctsio->io_hdr.flags & CTL_FLAG_DATA_MASK) ==
		     CTL_FLAG_DATA_IN) {
			CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: copying %d "
					 "bytes to user\n", len_to_copy));
			CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: from %p "
					 "to %p\n", kern_ptr, ext_ptr));
			if (copyout(kern_ptr, ext_ptr, len_to_copy) != 0) {
				ctl_set_internal_failure(ctsio,
							 /*sks_valid*/ 0,
							 /*retry_count*/ 0);
				goto bailout;
			}
		} else {
			CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: copying %d "
					 "bytes from user\n", len_to_copy));
			CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: from %p "
					 "to %p\n", ext_ptr, kern_ptr));
			if (copyin(ext_ptr, kern_ptr, len_to_copy)!= 0){
				ctl_set_internal_failure(ctsio,
							 /*sks_valid*/ 0,
							 /*retry_count*/0);
				goto bailout;
			}
		}

		len_copied += len_to_copy;

		if (ext_sglist[i].len == ext_watermark) {
			i++;
			ext_watermark = 0;
		}

		if (kern_sglist[j].len == kern_watermark) {
			j++;
			kern_watermark = 0;
		}
	}

	ctsio->ext_data_filled += len_copied;

	CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: ext_sg_entries: %d, "
			 "kern_sg_entries: %d\n", ext_sg_entries,
			 kern_sg_entries));
	CTL_DEBUG_PRINT(("ctl_ioctl_do_datamove: ext_data_len = %d, "
			 "kern_data_len = %d\n", ctsio->ext_data_len,
			 ctsio->kern_data_len));


	/* XXX KDM set residual?? */
bailout:

	if (ext_sglist_malloced != 0)
		free(ext_sglist, M_CTL);

	return (CTL_RETVAL_COMPLETE);
}

/*
 * Serialize a command that went down the "wrong" side, and so was sent to
 * this controller for execution.  The logic is a little different than the
 * standard case in ctl_scsiio_precheck().  Errors in this case need to get
 * sent back to the other side, but in the success case, we execute the
 * command on this side (XFER mode) or tell the other side to execute it
 * (SER_ONLY mode).
 */
static int
ctl_serialize_other_sc_cmd(struct ctl_scsiio *ctsio)
{
	struct ctl_softc *softc;
	union ctl_ha_msg msg_info;
	struct ctl_lun *lun;
	int retval = 0;
	uint32_t targ_lun;

	softc = control_softc;

	targ_lun = ctsio->io_hdr.nexus.targ_mapped_lun;
	lun = softc->ctl_luns[targ_lun];
	if (lun==NULL)
	{
		/*
		 * Why isn't LUN defined? The other side wouldn't
		 * send a cmd if the LUN is undefined.
		 */
		printf("%s: Bad JUJU!, LUN is NULL!\n", __func__);

		/* "Logical unit not supported" */
		ctl_set_sense_data(&msg_info.scsi.sense_data,
				   lun,
				   /*sense_format*/SSD_TYPE_NONE,
				   /*current_error*/ 1,
				   /*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
				   /*asc*/ 0x25,
				   /*ascq*/ 0x00,
				   SSD_ELEM_NONE);

		msg_info.scsi.sense_len = SSD_FULL_SIZE;
		msg_info.scsi.scsi_status = SCSI_STATUS_CHECK_COND;
		msg_info.hdr.status = CTL_SCSI_ERROR | CTL_AUTOSENSE;
		msg_info.hdr.original_sc = ctsio->io_hdr.original_sc;
		msg_info.hdr.serializing_sc = NULL;
		msg_info.hdr.msg_type = CTL_MSG_BAD_JUJU;
	        if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg_info,
				sizeof(msg_info), 0 ) > CTL_HA_STATUS_SUCCESS) {
		}
		return(1);

	}

	mtx_lock(&lun->lun_lock);
    	TAILQ_INSERT_TAIL(&lun->ooa_queue, &ctsio->io_hdr, ooa_links);

	switch (ctl_check_ooa(lun, (union ctl_io *)ctsio,
		(union ctl_io *)TAILQ_PREV(&ctsio->io_hdr, ctl_ooaq,
		 ooa_links))) {
	case CTL_ACTION_BLOCK:
		ctsio->io_hdr.flags |= CTL_FLAG_BLOCKED;
		TAILQ_INSERT_TAIL(&lun->blocked_queue, &ctsio->io_hdr,
				  blocked_links);
		break;
	case CTL_ACTION_PASS:
	case CTL_ACTION_SKIP:
		if (softc->ha_mode == CTL_HA_MODE_XFER) {
			ctsio->io_hdr.flags |= CTL_FLAG_IS_WAS_ON_RTR;
			ctl_enqueue_rtr((union ctl_io *)ctsio);
		} else {

			/* send msg back to other side */
			msg_info.hdr.original_sc = ctsio->io_hdr.original_sc;
			msg_info.hdr.serializing_sc = (union ctl_io *)ctsio;
			msg_info.hdr.msg_type = CTL_MSG_R2R;
#if 0
			printf("2. pOrig %x\n", (int)msg_info.hdr.original_sc);
#endif
		        if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg_info,
			    sizeof(msg_info), 0 ) > CTL_HA_STATUS_SUCCESS) {
			}
		}
		break;
	case CTL_ACTION_OVERLAP:
		/* OVERLAPPED COMMANDS ATTEMPTED */
		ctl_set_sense_data(&msg_info.scsi.sense_data,
				   lun,
				   /*sense_format*/SSD_TYPE_NONE,
				   /*current_error*/ 1,
				   /*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
				   /*asc*/ 0x4E,
				   /*ascq*/ 0x00,
				   SSD_ELEM_NONE);

		msg_info.scsi.sense_len = SSD_FULL_SIZE;
		msg_info.scsi.scsi_status = SCSI_STATUS_CHECK_COND;
		msg_info.hdr.status = CTL_SCSI_ERROR | CTL_AUTOSENSE;
		msg_info.hdr.original_sc = ctsio->io_hdr.original_sc;
		msg_info.hdr.serializing_sc = NULL;
		msg_info.hdr.msg_type = CTL_MSG_BAD_JUJU;
#if 0
		printf("BAD JUJU:Major Bummer Overlap\n");
#endif
		TAILQ_REMOVE(&lun->ooa_queue, &ctsio->io_hdr, ooa_links);
		retval = 1;
		if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg_info,
		    sizeof(msg_info), 0 ) > CTL_HA_STATUS_SUCCESS) {
		}
		break;
	case CTL_ACTION_OVERLAP_TAG:
		/* TAGGED OVERLAPPED COMMANDS (NN = QUEUE TAG) */
		ctl_set_sense_data(&msg_info.scsi.sense_data,
				   lun,
				   /*sense_format*/SSD_TYPE_NONE,
				   /*current_error*/ 1,
				   /*sense_key*/ SSD_KEY_ILLEGAL_REQUEST,
				   /*asc*/ 0x4D,
				   /*ascq*/ ctsio->tag_num & 0xff,
				   SSD_ELEM_NONE);

		msg_info.scsi.sense_len = SSD_FULL_SIZE;
		msg_info.scsi.scsi_status = SCSI_STATUS_CHECK_COND;
		msg_info.hdr.status = CTL_SCSI_ERROR | CTL_AUTOSENSE;
		msg_info.hdr.original_sc = ctsio->io_hdr.original_sc;
		msg_info.hdr.serializing_sc = NULL;
		msg_info.hdr.msg_type = CTL_MSG_BAD_JUJU;
#if 0
		printf("BAD JUJU:Major Bummer Overlap Tag\n");
#endif
		TAILQ_REMOVE(&lun->ooa_queue, &ctsio->io_hdr, ooa_links);
		retval = 1;
		if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg_info,
		    sizeof(msg_info), 0 ) > CTL_HA_STATUS_SUCCESS) {
		}
		break;
	case CTL_ACTION_ERROR:
	default:
		/* "Internal target failure" */
		ctl_set_sense_data(&msg_info.scsi.sense_data,
				   lun,
				   /*sense_format*/SSD_TYPE_NONE,
				   /*current_error*/ 1,
				   /*sense_key*/ SSD_KEY_HARDWARE_ERROR,
				   /*asc*/ 0x44,
				   /*ascq*/ 0x00,
				   SSD_ELEM_NONE);

		msg_info.scsi.sense_len = SSD_FULL_SIZE;
		msg_info.scsi.scsi_status = SCSI_STATUS_CHECK_COND;
		msg_info.hdr.status = CTL_SCSI_ERROR | CTL_AUTOSENSE;
		msg_info.hdr.original_sc = ctsio->io_hdr.original_sc;
		msg_info.hdr.serializing_sc = NULL;
		msg_info.hdr.msg_type = CTL_MSG_BAD_JUJU;
#if 0
		printf("BAD JUJU:Major Bummer HW Error\n");
#endif
		TAILQ_REMOVE(&lun->ooa_queue, &ctsio->io_hdr, ooa_links);
		retval = 1;
		if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg_info,
		    sizeof(msg_info), 0 ) > CTL_HA_STATUS_SUCCESS) {
		}
		break;
	}
	mtx_unlock(&lun->lun_lock);
	return (retval);
}

static int
ctl_ioctl_submit_wait(union ctl_io *io)
{
	struct ctl_fe_ioctl_params params;
	ctl_fe_ioctl_state last_state;
	int done, retval;

	retval = 0;

	bzero(&params, sizeof(params));

	mtx_init(&params.ioctl_mtx, "ctliocmtx", NULL, MTX_DEF);
	cv_init(&params.sem, "ctlioccv");
	params.state = CTL_IOCTL_INPROG;
	last_state = params.state;

	io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = &params;

	CTL_DEBUG_PRINT(("ctl_ioctl_submit_wait\n"));

	/* This shouldn't happen */
	if ((retval = ctl_queue(io)) != CTL_RETVAL_COMPLETE)
		return (retval);

	done = 0;

	do {
		mtx_lock(&params.ioctl_mtx);
		/*
		 * Check the state here, and don't sleep if the state has
		 * already changed (i.e. wakeup has already occured, but we
		 * weren't waiting yet).
		 */
		if (params.state == last_state) {
			/* XXX KDM cv_wait_sig instead? */
			cv_wait(&params.sem, &params.ioctl_mtx);
		}
		last_state = params.state;

		switch (params.state) {
		case CTL_IOCTL_INPROG:
			/* Why did we wake up? */
			/* XXX KDM error here? */
			mtx_unlock(&params.ioctl_mtx);
			break;
		case CTL_IOCTL_DATAMOVE:
			CTL_DEBUG_PRINT(("got CTL_IOCTL_DATAMOVE\n"));

			/*
			 * change last_state back to INPROG to avoid
			 * deadlock on subsequent data moves.
			 */
			params.state = last_state = CTL_IOCTL_INPROG;

			mtx_unlock(&params.ioctl_mtx);
			ctl_ioctl_do_datamove(&io->scsiio);
			/*
			 * Note that in some cases, most notably writes,
			 * this will queue the I/O and call us back later.
			 * In other cases, generally reads, this routine
			 * will immediately call back and wake us up,
			 * probably using our own context.
			 */
			io->scsiio.be_move_done(io);
			break;
		case CTL_IOCTL_DONE:
			mtx_unlock(&params.ioctl_mtx);
			CTL_DEBUG_PRINT(("got CTL_IOCTL_DONE\n"));
			done = 1;
			break;
		default:
			mtx_unlock(&params.ioctl_mtx);
			/* XXX KDM error here? */
			break;
		}
	} while (done == 0);

	mtx_destroy(&params.ioctl_mtx);
	cv_destroy(&params.sem);

	return (CTL_RETVAL_COMPLETE);
}

static void
ctl_ioctl_datamove(union ctl_io *io)
{
	struct ctl_fe_ioctl_params *params;

	params = (struct ctl_fe_ioctl_params *)
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;

	mtx_lock(&params->ioctl_mtx);
	params->state = CTL_IOCTL_DATAMOVE;
	cv_broadcast(&params->sem);
	mtx_unlock(&params->ioctl_mtx);
}

static void
ctl_ioctl_done(union ctl_io *io)
{
	struct ctl_fe_ioctl_params *params;

	params = (struct ctl_fe_ioctl_params *)
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;

	mtx_lock(&params->ioctl_mtx);
	params->state = CTL_IOCTL_DONE;
	cv_broadcast(&params->sem);
	mtx_unlock(&params->ioctl_mtx);
}

static void
ctl_ioctl_hard_startstop_callback(void *arg, struct cfi_metatask *metatask)
{
	struct ctl_fe_ioctl_startstop_info *sd_info;

	sd_info = (struct ctl_fe_ioctl_startstop_info *)arg;

	sd_info->hs_info.status = metatask->status;
	sd_info->hs_info.total_luns = metatask->taskinfo.startstop.total_luns;
	sd_info->hs_info.luns_complete =
		metatask->taskinfo.startstop.luns_complete;
	sd_info->hs_info.luns_failed = metatask->taskinfo.startstop.luns_failed;

	cv_broadcast(&sd_info->sem);
}

static void
ctl_ioctl_bbrread_callback(void *arg, struct cfi_metatask *metatask)
{
	struct ctl_fe_ioctl_bbrread_info *fe_bbr_info;

	fe_bbr_info = (struct ctl_fe_ioctl_bbrread_info *)arg;

	mtx_lock(fe_bbr_info->lock);
	fe_bbr_info->bbr_info->status = metatask->status;
	fe_bbr_info->bbr_info->bbr_status = metatask->taskinfo.bbrread.status;
	fe_bbr_info->wakeup_done = 1;
	mtx_unlock(fe_bbr_info->lock);

	cv_broadcast(&fe_bbr_info->sem);
}

/*
 * Returns 0 for success, errno for failure.
 */
static int
ctl_ioctl_fill_ooa(struct ctl_lun *lun, uint32_t *cur_fill_num,
		   struct ctl_ooa *ooa_hdr, struct ctl_ooa_entry *kern_entries)
{
	union ctl_io *io;
	int retval;

	retval = 0;

	mtx_lock(&lun->lun_lock);
	for (io = (union ctl_io *)TAILQ_FIRST(&lun->ooa_queue); (io != NULL);
	     (*cur_fill_num)++, io = (union ctl_io *)TAILQ_NEXT(&io->io_hdr,
	     ooa_links)) {
		struct ctl_ooa_entry *entry;

		/*
		 * If we've got more than we can fit, just count the
		 * remaining entries.
		 */
		if (*cur_fill_num >= ooa_hdr->alloc_num)
			continue;

		entry = &kern_entries[*cur_fill_num];

		entry->tag_num = io->scsiio.tag_num;
		entry->lun_num = lun->lun;
#ifdef CTL_TIME_IO
		entry->start_bt = io->io_hdr.start_bt;
#endif
		bcopy(io->scsiio.cdb, entry->cdb, io->scsiio.cdb_len);
		entry->cdb_len = io->scsiio.cdb_len;
		if (io->io_hdr.flags & CTL_FLAG_BLOCKED)
			entry->cmd_flags |= CTL_OOACMD_FLAG_BLOCKED;

		if (io->io_hdr.flags & CTL_FLAG_DMA_INPROG)
			entry->cmd_flags |= CTL_OOACMD_FLAG_DMA;

		if (io->io_hdr.flags & CTL_FLAG_ABORT)
			entry->cmd_flags |= CTL_OOACMD_FLAG_ABORT;

		if (io->io_hdr.flags & CTL_FLAG_IS_WAS_ON_RTR)
			entry->cmd_flags |= CTL_OOACMD_FLAG_RTR;

		if (io->io_hdr.flags & CTL_FLAG_DMA_QUEUED)
			entry->cmd_flags |= CTL_OOACMD_FLAG_DMA_QUEUED;
	}
	mtx_unlock(&lun->lun_lock);

	return (retval);
}

static void *
ctl_copyin_alloc(void *user_addr, int len, char *error_str,
		 size_t error_str_len)
{
	void *kptr;

	kptr = malloc(len, M_CTL, M_WAITOK | M_ZERO);

	if (copyin(user_addr, kptr, len) != 0) {
		snprintf(error_str, error_str_len, "Error copying %d bytes "
			 "from user address %p to kernel address %p", len,
			 user_addr, kptr);
		free(kptr, M_CTL);
		return (NULL);
	}

	return (kptr);
}

static void
ctl_free_args(int num_args, struct ctl_be_arg *args)
{
	int i;

	if (args == NULL)
		return;

	for (i = 0; i < num_args; i++) {
		free(args[i].kname, M_CTL);
		free(args[i].kvalue, M_CTL);
	}

	free(args, M_CTL);
}

static struct ctl_be_arg *
ctl_copyin_args(int num_args, struct ctl_be_arg *uargs,
		char *error_str, size_t error_str_len)
{
	struct ctl_be_arg *args;
	int i;

	args = ctl_copyin_alloc(uargs, num_args * sizeof(*args),
				error_str, error_str_len);

	if (args == NULL)
		goto bailout;

	for (i = 0; i < num_args; i++) {
		args[i].kname = NULL;
		args[i].kvalue = NULL;
	}

	for (i = 0; i < num_args; i++) {
		uint8_t *tmpptr;

		args[i].kname = ctl_copyin_alloc(args[i].name,
			args[i].namelen, error_str, error_str_len);
		if (args[i].kname == NULL)
			goto bailout;

		if (args[i].kname[args[i].namelen - 1] != '\0') {
			snprintf(error_str, error_str_len, "Argument %d "
				 "name is not NUL-terminated", i);
			goto bailout;
		}

		if (args[i].flags & CTL_BEARG_RD) {
			tmpptr = ctl_copyin_alloc(args[i].value,
				args[i].vallen, error_str, error_str_len);
			if (tmpptr == NULL)
				goto bailout;
			if ((args[i].flags & CTL_BEARG_ASCII)
			 && (tmpptr[args[i].vallen - 1] != '\0')) {
				snprintf(error_str, error_str_len, "Argument "
				    "%d value is not NUL-terminated", i);
				goto bailout;
			}
			args[i].kvalue = tmpptr;
		} else {
			args[i].kvalue = malloc(args[i].vallen,
			    M_CTL, M_WAITOK | M_ZERO);
		}
	}

	return (args);
bailout:

	ctl_free_args(num_args, args);

	return (NULL);
}

static void
ctl_copyout_args(int num_args, struct ctl_be_arg *args)
{
	int i;

	for (i = 0; i < num_args; i++) {
		if (args[i].flags & CTL_BEARG_WR)
			copyout(args[i].kvalue, args[i].value, args[i].vallen);
	}
}

/*
 * Escape characters that are illegal or not recommended in XML.
 */
int
ctl_sbuf_printf_esc(struct sbuf *sb, char *str, int size)
{
	char *end = str + size;
	int retval;

	retval = 0;

	for (; *str && str < end; str++) {
		switch (*str) {
		case '&':
			retval = sbuf_printf(sb, "&amp;");
			break;
		case '>':
			retval = sbuf_printf(sb, "&gt;");
			break;
		case '<':
			retval = sbuf_printf(sb, "&lt;");
			break;
		default:
			retval = sbuf_putc(sb, *str);
			break;
		}

		if (retval != 0)
			break;

	}

	return (retval);
}

static void
ctl_id_sbuf(struct ctl_devid *id, struct sbuf *sb)
{
	struct scsi_vpd_id_descriptor *desc;
	int i;

	if (id == NULL || id->len < 4)
		return;
	desc = (struct scsi_vpd_id_descriptor *)id->data;
	switch (desc->id_type & SVPD_ID_TYPE_MASK) {
	case SVPD_ID_TYPE_T10:
		sbuf_printf(sb, "t10.");
		break;
	case SVPD_ID_TYPE_EUI64:
		sbuf_printf(sb, "eui.");
		break;
	case SVPD_ID_TYPE_NAA:
		sbuf_printf(sb, "naa.");
		break;
	case SVPD_ID_TYPE_SCSI_NAME:
		break;
	}
	switch (desc->proto_codeset & SVPD_ID_CODESET_MASK) {
	case SVPD_ID_CODESET_BINARY:
		for (i = 0; i < desc->length; i++)
			sbuf_printf(sb, "%02x", desc->identifier[i]);
		break;
	case SVPD_ID_CODESET_ASCII:
		sbuf_printf(sb, "%.*s", (int)desc->length,
		    (char *)desc->identifier);
		break;
	case SVPD_ID_CODESET_UTF8:
		sbuf_printf(sb, "%s", (char *)desc->identifier);
		break;
	}
}

static int
ctl_ioctl(struct cdev *dev, u_long cmd, caddr_t addr, int flag,
	  struct thread *td)
{
	struct ctl_softc *softc;
	int retval;

	softc = control_softc;

	retval = 0;

	switch (cmd) {
	case CTL_IO: {
		union ctl_io *io;
		void *pool_tmp;

		/*
		 * If we haven't been "enabled", don't allow any SCSI I/O
		 * to this FETD.
		 */
		if ((softc->ioctl_info.flags & CTL_IOCTL_FLAG_ENABLED) == 0) {
			retval = EPERM;
			break;
		}

		io = ctl_alloc_io(softc->ioctl_info.port.ctl_pool_ref);

		/*
		 * Need to save the pool reference so it doesn't get
		 * spammed by the user's ctl_io.
		 */
		pool_tmp = io->io_hdr.pool;
		memcpy(io, (void *)addr, sizeof(*io));
		io->io_hdr.pool = pool_tmp;

		/*
		 * No status yet, so make sure the status is set properly.
		 */
		io->io_hdr.status = CTL_STATUS_NONE;

		/*
		 * The user sets the initiator ID, target and LUN IDs.
		 */
		io->io_hdr.nexus.targ_port = softc->ioctl_info.port.targ_port;
		io->io_hdr.flags |= CTL_FLAG_USER_REQ;
		if ((io->io_hdr.io_type == CTL_IO_SCSI)
		 && (io->scsiio.tag_type != CTL_TAG_UNTAGGED))
			io->scsiio.tag_num = softc->ioctl_info.cur_tag_num++;

		retval = ctl_ioctl_submit_wait(io);

		if (retval != 0) {
			ctl_free_io(io);
			break;
		}

		memcpy((void *)addr, io, sizeof(*io));

		/* return this to our pool */
		ctl_free_io(io);

		break;
	}
	case CTL_ENABLE_PORT:
	case CTL_DISABLE_PORT:
	case CTL_SET_PORT_WWNS: {
		struct ctl_port *port;
		struct ctl_port_entry *entry;

		entry = (struct ctl_port_entry *)addr;
		
		mtx_lock(&softc->ctl_lock);
		STAILQ_FOREACH(port, &softc->port_list, links) {
			int action, done;

			action = 0;
			done = 0;

			if ((entry->port_type == CTL_PORT_NONE)
			 && (entry->targ_port == port->targ_port)) {
				/*
				 * If the user only wants to enable or
				 * disable or set WWNs on a specific port,
				 * do the operation and we're done.
				 */
				action = 1;
				done = 1;
			} else if (entry->port_type & port->port_type) {
				/*
				 * Compare the user's type mask with the
				 * particular frontend type to see if we
				 * have a match.
				 */
				action = 1;
				done = 0;

				/*
				 * Make sure the user isn't trying to set
				 * WWNs on multiple ports at the same time.
				 */
				if (cmd == CTL_SET_PORT_WWNS) {
					printf("%s: Can't set WWNs on "
					       "multiple ports\n", __func__);
					retval = EINVAL;
					break;
				}
			}
			if (action != 0) {
				/*
				 * XXX KDM we have to drop the lock here,
				 * because the online/offline operations
				 * can potentially block.  We need to
				 * reference count the frontends so they
				 * can't go away,
				 */
				mtx_unlock(&softc->ctl_lock);

				if (cmd == CTL_ENABLE_PORT) {
					ctl_port_online(port);
				} else if (cmd == CTL_DISABLE_PORT) {
					ctl_port_offline(port);
				}

				mtx_lock(&softc->ctl_lock);

				if (cmd == CTL_SET_PORT_WWNS)
					ctl_port_set_wwns(port,
					    (entry->flags & CTL_PORT_WWNN_VALID) ?
					    1 : 0, entry->wwnn,
					    (entry->flags & CTL_PORT_WWPN_VALID) ?
					    1 : 0, entry->wwpn);
			}
			if (done != 0)
				break;
		}
		mtx_unlock(&softc->ctl_lock);
		break;
	}
	case CTL_GET_PORT_LIST: {
		struct ctl_port *port;
		struct ctl_port_list *list;
		int i;

		list = (struct ctl_port_list *)addr;

		if (list->alloc_len != (list->alloc_num *
		    sizeof(struct ctl_port_entry))) {
			printf("%s: CTL_GET_PORT_LIST: alloc_len %u != "
			       "alloc_num %u * sizeof(struct ctl_port_entry) "
			       "%zu\n", __func__, list->alloc_len,
			       list->alloc_num, sizeof(struct ctl_port_entry));
			retval = EINVAL;
			break;
		}
		list->fill_len = 0;
		list->fill_num = 0;
		list->dropped_num = 0;
		i = 0;
		mtx_lock(&softc->ctl_lock);
		STAILQ_FOREACH(port, &softc->port_list, links) {
			struct ctl_port_entry entry, *list_entry;

			if (list->fill_num >= list->alloc_num) {
				list->dropped_num++;
				continue;
			}

			entry.port_type = port->port_type;
			strlcpy(entry.port_name, port->port_name,
				sizeof(entry.port_name));
			entry.targ_port = port->targ_port;
			entry.physical_port = port->physical_port;
			entry.virtual_port = port->virtual_port;
			entry.wwnn = port->wwnn;
			entry.wwpn = port->wwpn;
			if (port->status & CTL_PORT_STATUS_ONLINE)
				entry.online = 1;
			else
				entry.online = 0;

			list_entry = &list->entries[i];

			retval = copyout(&entry, list_entry, sizeof(entry));
			if (retval != 0) {
				printf("%s: CTL_GET_PORT_LIST: copyout "
				       "returned %d\n", __func__, retval);
				break;
			}
			i++;
			list->fill_num++;
			list->fill_len += sizeof(entry);
		}
		mtx_unlock(&softc->ctl_lock);

		/*
		 * If this is non-zero, we had a copyout fault, so there's
		 * probably no point in attempting to set the status inside
		 * the structure.
		 */
		if (retval != 0)
			break;

		if (list->dropped_num > 0)
			list->status = CTL_PORT_LIST_NEED_MORE_SPACE;
		else
			list->status = CTL_PORT_LIST_OK;
		break;
	}
	case CTL_DUMP_OOA: {
		struct ctl_lun *lun;
		union ctl_io *io;
		char printbuf[128];
		struct sbuf sb;

		mtx_lock(&softc->ctl_lock);
		printf("Dumping OOA queues:\n");
		STAILQ_FOREACH(lun, &softc->lun_list, links) {
			mtx_lock(&lun->lun_lock);
			for (io = (union ctl_io *)TAILQ_FIRST(
			     &lun->ooa_queue); io != NULL;
			     io = (union ctl_io *)TAILQ_NEXT(&io->io_hdr,
			     ooa_links)) {
				sbuf_new(&sb, printbuf, sizeof(printbuf),
					 SBUF_FIXEDLEN);
				sbuf_printf(&sb, "LUN %jd tag 0x%04x%s%s%s%s: ",
					    (intmax_t)lun->lun,
					    io->scsiio.tag_num,
					    (io->io_hdr.flags &
					    CTL_FLAG_BLOCKED) ? "" : " BLOCKED",
					    (io->io_hdr.flags &
					    CTL_FLAG_DMA_INPROG) ? " DMA" : "",
					    (io->io_hdr.flags &
					    CTL_FLAG_ABORT) ? " ABORT" : "",
			                    (io->io_hdr.flags &
		                        CTL_FLAG_IS_WAS_ON_RTR) ? " RTR" : "");
				ctl_scsi_command_string(&io->scsiio, NULL, &sb);
				sbuf_finish(&sb);
				printf("%s\n", sbuf_data(&sb));
			}
			mtx_unlock(&lun->lun_lock);
		}
		printf("OOA queues dump done\n");
		mtx_unlock(&softc->ctl_lock);
		break;
	}
	case CTL_GET_OOA: {
		struct ctl_lun *lun;
		struct ctl_ooa *ooa_hdr;
		struct ctl_ooa_entry *entries;
		uint32_t cur_fill_num;

		ooa_hdr = (struct ctl_ooa *)addr;

		if ((ooa_hdr->alloc_len == 0)
		 || (ooa_hdr->alloc_num == 0)) {
			printf("%s: CTL_GET_OOA: alloc len %u and alloc num %u "
			       "must be non-zero\n", __func__,
			       ooa_hdr->alloc_len, ooa_hdr->alloc_num);
			retval = EINVAL;
			break;
		}

		if (ooa_hdr->alloc_len != (ooa_hdr->alloc_num *
		    sizeof(struct ctl_ooa_entry))) {
			printf("%s: CTL_GET_OOA: alloc len %u must be alloc "
			       "num %d * sizeof(struct ctl_ooa_entry) %zd\n",
			       __func__, ooa_hdr->alloc_len,
			       ooa_hdr->alloc_num,sizeof(struct ctl_ooa_entry));
			retval = EINVAL;
			break;
		}

		entries = malloc(ooa_hdr->alloc_len, M_CTL, M_WAITOK | M_ZERO);
		if (entries == NULL) {
			printf("%s: could not allocate %d bytes for OOA "
			       "dump\n", __func__, ooa_hdr->alloc_len);
			retval = ENOMEM;
			break;
		}

		mtx_lock(&softc->ctl_lock);
		if (((ooa_hdr->flags & CTL_OOA_FLAG_ALL_LUNS) == 0)
		 && ((ooa_hdr->lun_num >= CTL_MAX_LUNS)
		  || (softc->ctl_luns[ooa_hdr->lun_num] == NULL))) {
			mtx_unlock(&softc->ctl_lock);
			free(entries, M_CTL);
			printf("%s: CTL_GET_OOA: invalid LUN %ju\n",
			       __func__, (uintmax_t)ooa_hdr->lun_num);
			retval = EINVAL;
			break;
		}

		cur_fill_num = 0;

		if (ooa_hdr->flags & CTL_OOA_FLAG_ALL_LUNS) {
			STAILQ_FOREACH(lun, &softc->lun_list, links) {
				retval = ctl_ioctl_fill_ooa(lun, &cur_fill_num,
					ooa_hdr, entries);
				if (retval != 0)
					break;
			}
			if (retval != 0) {
				mtx_unlock(&softc->ctl_lock);
				free(entries, M_CTL);
				break;
			}
		} else {
			lun = softc->ctl_luns[ooa_hdr->lun_num];

			retval = ctl_ioctl_fill_ooa(lun, &cur_fill_num,ooa_hdr,
						    entries);
		}
		mtx_unlock(&softc->ctl_lock);

		ooa_hdr->fill_num = min(cur_fill_num, ooa_hdr->alloc_num);
		ooa_hdr->fill_len = ooa_hdr->fill_num *
			sizeof(struct ctl_ooa_entry);
		retval = copyout(entries, ooa_hdr->entries, ooa_hdr->fill_len);
		if (retval != 0) {
			printf("%s: error copying out %d bytes for OOA dump\n", 
			       __func__, ooa_hdr->fill_len);
		}

		getbintime(&ooa_hdr->cur_bt);

		if (cur_fill_num > ooa_hdr->alloc_num) {
			ooa_hdr->dropped_num = cur_fill_num -ooa_hdr->alloc_num;
			ooa_hdr->status = CTL_OOA_NEED_MORE_SPACE;
		} else {
			ooa_hdr->dropped_num = 0;
			ooa_hdr->status = CTL_OOA_OK;
		}

		free(entries, M_CTL);
		break;
	}
	case CTL_CHECK_OOA: {
		union ctl_io *io;
		struct ctl_lun *lun;
		struct ctl_ooa_info *ooa_info;


		ooa_info = (struct ctl_ooa_info *)addr;

		if (ooa_info->lun_id >= CTL_MAX_LUNS) {
			ooa_info->status = CTL_OOA_INVALID_LUN;
			break;
		}
		mtx_lock(&softc->ctl_lock);
		lun = softc->ctl_luns[ooa_info->lun_id];
		if (lun == NULL) {
			mtx_unlock(&softc->ctl_lock);
			ooa_info->status = CTL_OOA_INVALID_LUN;
			break;
		}
		mtx_lock(&lun->lun_lock);
		mtx_unlock(&softc->ctl_lock);
		ooa_info->num_entries = 0;
		for (io = (union ctl_io *)TAILQ_FIRST(&lun->ooa_queue);
		     io != NULL; io = (union ctl_io *)TAILQ_NEXT(
		     &io->io_hdr, ooa_links)) {
			ooa_info->num_entries++;
		}
		mtx_unlock(&lun->lun_lock);

		ooa_info->status = CTL_OOA_SUCCESS;

		break;
	}
	case CTL_HARD_START:
	case CTL_HARD_STOP: {
		struct ctl_fe_ioctl_startstop_info ss_info;
		struct cfi_metatask *metatask;
		struct mtx hs_mtx;

		mtx_init(&hs_mtx, "HS Mutex", NULL, MTX_DEF);

		cv_init(&ss_info.sem, "hard start/stop cv" );

		metatask = cfi_alloc_metatask(/*can_wait*/ 1);
		if (metatask == NULL) {
			retval = ENOMEM;
			mtx_destroy(&hs_mtx);
			break;
		}

		if (cmd == CTL_HARD_START)
			metatask->tasktype = CFI_TASK_STARTUP;
		else
			metatask->tasktype = CFI_TASK_SHUTDOWN;

		metatask->callback = ctl_ioctl_hard_startstop_callback;
		metatask->callback_arg = &ss_info;

		cfi_action(metatask);

		/* Wait for the callback */
		mtx_lock(&hs_mtx);
		cv_wait_sig(&ss_info.sem, &hs_mtx);
		mtx_unlock(&hs_mtx);

		/*
		 * All information has been copied from the metatask by the
		 * time cv_broadcast() is called, so we free the metatask here.
		 */
		cfi_free_metatask(metatask);

		memcpy((void *)addr, &ss_info.hs_info, sizeof(ss_info.hs_info));

		mtx_destroy(&hs_mtx);
		break;
	}
	case CTL_BBRREAD: {
		struct ctl_bbrread_info *bbr_info;
		struct ctl_fe_ioctl_bbrread_info fe_bbr_info;
		struct mtx bbr_mtx;
		struct cfi_metatask *metatask;

		bbr_info = (struct ctl_bbrread_info *)addr;

		bzero(&fe_bbr_info, sizeof(fe_bbr_info));

		bzero(&bbr_mtx, sizeof(bbr_mtx));
		mtx_init(&bbr_mtx, "BBR Mutex", NULL, MTX_DEF);

		fe_bbr_info.bbr_info = bbr_info;
		fe_bbr_info.lock = &bbr_mtx;

		cv_init(&fe_bbr_info.sem, "BBR read cv");
		metatask = cfi_alloc_metatask(/*can_wait*/ 1);

		if (metatask == NULL) {
			mtx_destroy(&bbr_mtx);
			cv_destroy(&fe_bbr_info.sem);
			retval = ENOMEM;
			break;
		}
		metatask->tasktype = CFI_TASK_BBRREAD;
		metatask->callback = ctl_ioctl_bbrread_callback;
		metatask->callback_arg = &fe_bbr_info;
		metatask->taskinfo.bbrread.lun_num = bbr_info->lun_num;
		metatask->taskinfo.bbrread.lba = bbr_info->lba;
		metatask->taskinfo.bbrread.len = bbr_info->len;

		cfi_action(metatask);

		mtx_lock(&bbr_mtx);
		while (fe_bbr_info.wakeup_done == 0)
			cv_wait_sig(&fe_bbr_info.sem, &bbr_mtx);
		mtx_unlock(&bbr_mtx);

		bbr_info->status = metatask->status;
		bbr_info->bbr_status = metatask->taskinfo.bbrread.status;
		bbr_info->scsi_status = metatask->taskinfo.bbrread.scsi_status;
		memcpy(&bbr_info->sense_data,
		       &metatask->taskinfo.bbrread.sense_data,
		       MIN(sizeof(bbr_info->sense_data),
			   sizeof(metatask->taskinfo.bbrread.sense_data)));

		cfi_free_metatask(metatask);

		mtx_destroy(&bbr_mtx);
		cv_destroy(&fe_bbr_info.sem);

		break;
	}
	case CTL_DELAY_IO: {
		struct ctl_io_delay_info *delay_info;
#ifdef CTL_IO_DELAY
		struct ctl_lun *lun;
#endif /* CTL_IO_DELAY */

		delay_info = (struct ctl_io_delay_info *)addr;

#ifdef CTL_IO_DELAY
		mtx_lock(&softc->ctl_lock);

		if ((delay_info->lun_id >= CTL_MAX_LUNS)
		 || (softc->ctl_luns[delay_info->lun_id] == NULL)) {
			delay_info->status = CTL_DELAY_STATUS_INVALID_LUN;
		} else {
			lun = softc->ctl_luns[delay_info->lun_id];
			mtx_lock(&lun->lun_lock);

			delay_info->status = CTL_DELAY_STATUS_OK;

			switch (delay_info->delay_type) {
			case CTL_DELAY_TYPE_CONT:
				break;
			case CTL_DELAY_TYPE_ONESHOT:
				break;
			default:
				delay_info->status =
					CTL_DELAY_STATUS_INVALID_TYPE;
				break;
			}

			switch (delay_info->delay_loc) {
			case CTL_DELAY_LOC_DATAMOVE:
				lun->delay_info.datamove_type =
					delay_info->delay_type;
				lun->delay_info.datamove_delay =
					delay_info->delay_secs;
				break;
			case CTL_DELAY_LOC_DONE:
				lun->delay_info.done_type =
					delay_info->delay_type;
				lun->delay_info.done_delay =
					delay_info->delay_secs;
				break;
			default:
				delay_info->status =
					CTL_DELAY_STATUS_INVALID_LOC;
				break;
			}
			mtx_unlock(&lun->lun_lock);
		}

		mtx_unlock(&softc->ctl_lock);
#else
		delay_info->status = CTL_DELAY_STATUS_NOT_IMPLEMENTED;
#endif /* CTL_IO_DELAY */
		break;
	}
	case CTL_REALSYNC_SET: {
		int *syncstate;

		syncstate = (int *)addr;

		mtx_lock(&softc->ctl_lock);
		switch (*syncstate) {
		case 0:
			softc->flags &= ~CTL_FLAG_REAL_SYNC;
			break;
		case 1:
			softc->flags |= CTL_FLAG_REAL_SYNC;
			break;
		default:
			retval = EINVAL;
			break;
		}
		mtx_unlock(&softc->ctl_lock);
		break;
	}
	case CTL_REALSYNC_GET: {
		int *syncstate;

		syncstate = (int*)addr;

		mtx_lock(&softc->ctl_lock);
		if (softc->flags & CTL_FLAG_REAL_SYNC)
			*syncstate = 1;
		else
			*syncstate = 0;
		mtx_unlock(&softc->ctl_lock);

		break;
	}
	case CTL_SETSYNC:
	case CTL_GETSYNC: {
		struct ctl_sync_info *sync_info;
		struct ctl_lun *lun;

		sync_info = (struct ctl_sync_info *)addr;

		mtx_lock(&softc->ctl_lock);
		lun = softc->ctl_luns[sync_info->lun_id];
		if (lun == NULL) {
			mtx_unlock(&softc->ctl_lock);
			sync_info->status = CTL_GS_SYNC_NO_LUN;
		}
		/*
		 * Get or set the sync interval.  We're not bounds checking
		 * in the set case, hopefully the user won't do something
		 * silly.
		 */
		mtx_lock(&lun->lun_lock);
		mtx_unlock(&softc->ctl_lock);
		if (cmd == CTL_GETSYNC)
			sync_info->sync_interval = lun->sync_interval;
		else
			lun->sync_interval = sync_info->sync_interval;
		mtx_unlock(&lun->lun_lock);

		sync_info->status = CTL_GS_SYNC_OK;

		break;
	}
	case CTL_GETSTATS: {
		struct ctl_stats *stats;
		struct ctl_lun *lun;
		int i;

		stats = (struct ctl_stats *)addr;

		if ((sizeof(struct ctl_lun_io_stats) * softc->num_luns) >
		     stats->alloc_len) {
			stats->status = CTL_SS_NEED_MORE_SPACE;
			stats->num_luns = softc->num_luns;
			break;
		}
		/*
		 * XXX KDM no locking here.  If the LUN list changes,
		 * things can blow up.
		 */
		for (i = 0, lun = STAILQ_FIRST(&softc->lun_list); lun != NULL;
		     i++, lun = STAILQ_NEXT(lun, links)) {
			retval = copyout(&lun->stats, &stats->lun_stats[i],
					 sizeof(lun->stats));
			if (retval != 0)
				break;
		}
		stats->num_luns = softc->num_luns;
		stats->fill_len = sizeof(struct ctl_lun_io_stats) *
				 softc->num_luns;
		stats->status = CTL_SS_OK;
#ifdef CTL_TIME_IO
		stats->flags = CTL_STATS_FLAG_TIME_VALID;
#else
		stats->flags = CTL_STATS_FLAG_NONE;
#endif
		getnanouptime(&stats->timestamp);
		break;
	}
	case CTL_ERROR_INJECT: {
		struct ctl_error_desc *err_desc, *new_err_desc;
		struct ctl_lun *lun;

		err_desc = (struct ctl_error_desc *)addr;

		new_err_desc = malloc(sizeof(*new_err_desc), M_CTL,
				      M_WAITOK | M_ZERO);
		bcopy(err_desc, new_err_desc, sizeof(*new_err_desc));

		mtx_lock(&softc->ctl_lock);
		lun = softc->ctl_luns[err_desc->lun_id];
		if (lun == NULL) {
			mtx_unlock(&softc->ctl_lock);
			free(new_err_desc, M_CTL);
			printf("%s: CTL_ERROR_INJECT: invalid LUN %ju\n",
			       __func__, (uintmax_t)err_desc->lun_id);
			retval = EINVAL;
			break;
		}
		mtx_lock(&lun->lun_lock);
		mtx_unlock(&softc->ctl_lock);

		/*
		 * We could do some checking here to verify the validity
		 * of the request, but given the complexity of error
		 * injection requests, the checking logic would be fairly
		 * complex.
		 *
		 * For now, if the request is invalid, it just won't get
		 * executed and might get deleted.
		 */
		STAILQ_INSERT_TAIL(&lun->error_list, new_err_desc, links);

		/*
		 * XXX KDM check to make sure the serial number is unique,
		 * in case we somehow manage to wrap.  That shouldn't
		 * happen for a very long time, but it's the right thing to
		 * do.
		 */
		new_err_desc->serial = lun->error_serial;
		err_desc->serial = lun->error_serial;
		lun->error_serial++;

		mtx_unlock(&lun->lun_lock);
		break;
	}
	case CTL_ERROR_INJECT_DELETE: {
		struct ctl_error_desc *delete_desc, *desc, *desc2;
		struct ctl_lun *lun;
		int delete_done;

		delete_desc = (struct ctl_error_desc *)addr;
		delete_done = 0;

		mtx_lock(&softc->ctl_lock);
		lun = softc->ctl_luns[delete_desc->lun_id];
		if (lun == NULL) {
			mtx_unlock(&softc->ctl_lock);
			printf("%s: CTL_ERROR_INJECT_DELETE: invalid LUN %ju\n",
			       __func__, (uintmax_t)delete_desc->lun_id);
			retval = EINVAL;
			break;
		}
		mtx_lock(&lun->lun_lock);
		mtx_unlock(&softc->ctl_lock);
		STAILQ_FOREACH_SAFE(desc, &lun->error_list, links, desc2) {
			if (desc->serial != delete_desc->serial)
				continue;

			STAILQ_REMOVE(&lun->error_list, desc, ctl_error_desc,
				      links);
			free(desc, M_CTL);
			delete_done = 1;
		}
		mtx_unlock(&lun->lun_lock);
		if (delete_done == 0) {
			printf("%s: CTL_ERROR_INJECT_DELETE: can't find "
			       "error serial %ju on LUN %u\n", __func__, 
			       delete_desc->serial, delete_desc->lun_id);
			retval = EINVAL;
			break;
		}
		break;
	}
	case CTL_DUMP_STRUCTS: {
		int i, j, k;
		struct ctl_port *port;
		struct ctl_frontend *fe;

		mtx_lock(&softc->ctl_lock);
		printf("CTL Persistent Reservation information start:\n");
		for (i = 0; i < CTL_MAX_LUNS; i++) {
			struct ctl_lun *lun;

			lun = softc->ctl_luns[i];

			if ((lun == NULL)
			 || ((lun->flags & CTL_LUN_DISABLED) != 0))
				continue;

			for (j = 0; j < (CTL_MAX_PORTS * 2); j++) {
				if (lun->pr_keys[j] == NULL)
					continue;
				for (k = 0; k < CTL_MAX_INIT_PER_PORT; k++){
					if (lun->pr_keys[j][k] == 0)
						continue;
					printf("  LUN %d port %d iid %d key "
					       "%#jx\n", i, j, k,
					       (uintmax_t)lun->pr_keys[j][k]);
				}
			}
		}
		printf("CTL Persistent Reservation information end\n");
		printf("CTL Ports:\n");
		STAILQ_FOREACH(port, &softc->port_list, links) {
			printf("  Port %d '%s' Frontend '%s' Type %u pp %d vp %d WWNN "
			       "%#jx WWPN %#jx\n", port->targ_port, port->port_name,
			       port->frontend->name, port->port_type,
			       port->physical_port, port->virtual_port,
			       (uintmax_t)port->wwnn, (uintmax_t)port->wwpn);
			for (j = 0; j < CTL_MAX_INIT_PER_PORT; j++) {
				if (port->wwpn_iid[j].in_use == 0 &&
				    port->wwpn_iid[j].wwpn == 0 &&
				    port->wwpn_iid[j].name == NULL)
					continue;

				printf("    iid %u use %d WWPN %#jx '%s'\n",
				    j, port->wwpn_iid[j].in_use,
				    (uintmax_t)port->wwpn_iid[j].wwpn,
				    port->wwpn_iid[j].name);
			}
		}
		printf("CTL Port information end\n");
		mtx_unlock(&softc->ctl_lock);
		/*
		 * XXX KDM calling this without a lock.  We'd likely want
		 * to drop the lock before calling the frontend's dump
		 * routine anyway.
		 */
		printf("CTL Frontends:\n");
		STAILQ_FOREACH(fe, &softc->fe_list, links) {
			printf("  Frontend '%s'\n", fe->name);
			if (fe->fe_dump != NULL)
				fe->fe_dump();
		}
		printf("CTL Frontend information end\n");
		break;
	}
	case CTL_LUN_REQ: {
		struct ctl_lun_req *lun_req;
		struct ctl_backend_driver *backend;

		lun_req = (struct ctl_lun_req *)addr;

		backend = ctl_backend_find(lun_req->backend);
		if (backend == NULL) {
			lun_req->status = CTL_LUN_ERROR;
			snprintf(lun_req->error_str,
				 sizeof(lun_req->error_str),
				 "Backend \"%s\" not found.",
				 lun_req->backend);
			break;
		}
		if (lun_req->num_be_args > 0) {
			lun_req->kern_be_args = ctl_copyin_args(
				lun_req->num_be_args,
				lun_req->be_args,
				lun_req->error_str,
				sizeof(lun_req->error_str));
			if (lun_req->kern_be_args == NULL) {
				lun_req->status = CTL_LUN_ERROR;
				break;
			}
		}

		retval = backend->ioctl(dev, cmd, addr, flag, td);

		if (lun_req->num_be_args > 0) {
			ctl_copyout_args(lun_req->num_be_args,
				      lun_req->kern_be_args);
			ctl_free_args(lun_req->num_be_args,
				      lun_req->kern_be_args);
		}
		break;
	}
	case CTL_LUN_LIST: {
		struct sbuf *sb;
		struct ctl_lun *lun;
		struct ctl_lun_list *list;
		struct ctl_option *opt;

		list = (struct ctl_lun_list *)addr;

		/*
		 * Allocate a fixed length sbuf here, based on the length
		 * of the user's buffer.  We could allocate an auto-extending
		 * buffer, and then tell the user how much larger our
		 * amount of data is than his buffer, but that presents
		 * some problems:
		 *
		 * 1.  The sbuf(9) routines use a blocking malloc, and so
		 *     we can't hold a lock while calling them with an
		 *     auto-extending buffer.
 		 *
		 * 2.  There is not currently a LUN reference counting
		 *     mechanism, outside of outstanding transactions on
		 *     the LUN's OOA queue.  So a LUN could go away on us
		 *     while we're getting the LUN number, backend-specific
		 *     information, etc.  Thus, given the way things
		 *     currently work, we need to hold the CTL lock while
		 *     grabbing LUN information.
		 *
		 * So, from the user's standpoint, the best thing to do is
		 * allocate what he thinks is a reasonable buffer length,
		 * and then if he gets a CTL_LUN_LIST_NEED_MORE_SPACE error,
		 * double the buffer length and try again.  (And repeat
		 * that until he succeeds.)
		 */
		sb = sbuf_new(NULL, NULL, list->alloc_len, SBUF_FIXEDLEN);
		if (sb == NULL) {
			list->status = CTL_LUN_LIST_ERROR;
			snprintf(list->error_str, sizeof(list->error_str),
				 "Unable to allocate %d bytes for LUN list",
				 list->alloc_len);
			break;
		}

		sbuf_printf(sb, "<ctllunlist>\n");

		mtx_lock(&softc->ctl_lock);
		STAILQ_FOREACH(lun, &softc->lun_list, links) {
			mtx_lock(&lun->lun_lock);
			retval = sbuf_printf(sb, "<lun id=\"%ju\">\n",
					     (uintmax_t)lun->lun);

			/*
			 * Bail out as soon as we see that we've overfilled
			 * the buffer.
			 */
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<backend_type>%s"
					     "</backend_type>\n",
					     (lun->backend == NULL) ?  "none" :
					     lun->backend->name);

			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<lun_type>%d</lun_type>\n",
					     lun->be_lun->lun_type);

			if (retval != 0)
				break;

			if (lun->backend == NULL) {
				retval = sbuf_printf(sb, "</lun>\n");
				if (retval != 0)
					break;
				continue;
			}

			retval = sbuf_printf(sb, "\t<size>%ju</size>\n",
					     (lun->be_lun->maxlba > 0) ?
					     lun->be_lun->maxlba + 1 : 0);

			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<blocksize>%u</blocksize>\n",
					     lun->be_lun->blocksize);

			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<serial_number>");

			if (retval != 0)
				break;

			retval = ctl_sbuf_printf_esc(sb,
			    lun->be_lun->serial_num,
			    sizeof(lun->be_lun->serial_num));

			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "</serial_number>\n");
		
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<device_id>");

			if (retval != 0)
				break;

			retval = ctl_sbuf_printf_esc(sb,
			    lun->be_lun->device_id,
			    sizeof(lun->be_lun->device_id));

			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "</device_id>\n");

			if (retval != 0)
				break;

			if (lun->backend->lun_info != NULL) {
				retval = lun->backend->lun_info(lun->be_lun->be_lun, sb);
				if (retval != 0)
					break;
			}
			STAILQ_FOREACH(opt, &lun->be_lun->options, links) {
				retval = sbuf_printf(sb, "\t<%s>%s</%s>\n",
				    opt->name, opt->value, opt->name);
				if (retval != 0)
					break;
			}

			retval = sbuf_printf(sb, "</lun>\n");

			if (retval != 0)
				break;
			mtx_unlock(&lun->lun_lock);
		}
		if (lun != NULL)
			mtx_unlock(&lun->lun_lock);
		mtx_unlock(&softc->ctl_lock);

		if ((retval != 0)
		 || ((retval = sbuf_printf(sb, "</ctllunlist>\n")) != 0)) {
			retval = 0;
			sbuf_delete(sb);
			list->status = CTL_LUN_LIST_NEED_MORE_SPACE;
			snprintf(list->error_str, sizeof(list->error_str),
				 "Out of space, %d bytes is too small",
				 list->alloc_len);
			break;
		}

		sbuf_finish(sb);

		retval = copyout(sbuf_data(sb), list->lun_xml,
				 sbuf_len(sb) + 1);

		list->fill_len = sbuf_len(sb) + 1;
		list->status = CTL_LUN_LIST_OK;
		sbuf_delete(sb);
		break;
	}
	case CTL_ISCSI: {
		struct ctl_iscsi *ci;
		struct ctl_frontend *fe;

		ci = (struct ctl_iscsi *)addr;

		fe = ctl_frontend_find("iscsi");
		if (fe == NULL) {
			ci->status = CTL_ISCSI_ERROR;
			snprintf(ci->error_str, sizeof(ci->error_str),
			    "Frontend \"iscsi\" not found.");
			break;
		}

		retval = fe->ioctl(dev, cmd, addr, flag, td);
		break;
	}
	case CTL_PORT_REQ: {
		struct ctl_req *req;
		struct ctl_frontend *fe;

		req = (struct ctl_req *)addr;

		fe = ctl_frontend_find(req->driver);
		if (fe == NULL) {
			req->status = CTL_LUN_ERROR;
			snprintf(req->error_str, sizeof(req->error_str),
			    "Frontend \"%s\" not found.", req->driver);
			break;
		}
		if (req->num_args > 0) {
			req->kern_args = ctl_copyin_args(req->num_args,
			    req->args, req->error_str, sizeof(req->error_str));
			if (req->kern_args == NULL) {
				req->status = CTL_LUN_ERROR;
				break;
			}
		}

		retval = fe->ioctl(dev, cmd, addr, flag, td);

		if (req->num_args > 0) {
			ctl_copyout_args(req->num_args, req->kern_args);
			ctl_free_args(req->num_args, req->kern_args);
		}
		break;
	}
	case CTL_PORT_LIST: {
		struct sbuf *sb;
		struct ctl_port *port;
		struct ctl_lun_list *list;
		struct ctl_option *opt;
		int j;
		uint32_t plun;

		list = (struct ctl_lun_list *)addr;

		sb = sbuf_new(NULL, NULL, list->alloc_len, SBUF_FIXEDLEN);
		if (sb == NULL) {
			list->status = CTL_LUN_LIST_ERROR;
			snprintf(list->error_str, sizeof(list->error_str),
				 "Unable to allocate %d bytes for LUN list",
				 list->alloc_len);
			break;
		}

		sbuf_printf(sb, "<ctlportlist>\n");

		mtx_lock(&softc->ctl_lock);
		STAILQ_FOREACH(port, &softc->port_list, links) {
			retval = sbuf_printf(sb, "<targ_port id=\"%ju\">\n",
					     (uintmax_t)port->targ_port);

			/*
			 * Bail out as soon as we see that we've overfilled
			 * the buffer.
			 */
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<frontend_type>%s"
			    "</frontend_type>\n", port->frontend->name);
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<port_type>%d</port_type>\n",
					     port->port_type);
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<online>%s</online>\n",
			    (port->status & CTL_PORT_STATUS_ONLINE) ? "YES" : "NO");
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<port_name>%s</port_name>\n",
			    port->port_name);
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<physical_port>%d</physical_port>\n",
			    port->physical_port);
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "\t<virtual_port>%d</virtual_port>\n",
			    port->virtual_port);
			if (retval != 0)
				break;

			if (port->target_devid != NULL) {
				sbuf_printf(sb, "\t<target>");
				ctl_id_sbuf(port->target_devid, sb);
				sbuf_printf(sb, "</target>\n");
			}

			if (port->port_devid != NULL) {
				sbuf_printf(sb, "\t<port>");
				ctl_id_sbuf(port->port_devid, sb);
				sbuf_printf(sb, "</port>\n");
			}

			if (port->port_info != NULL) {
				retval = port->port_info(port->onoff_arg, sb);
				if (retval != 0)
					break;
			}
			STAILQ_FOREACH(opt, &port->options, links) {
				retval = sbuf_printf(sb, "\t<%s>%s</%s>\n",
				    opt->name, opt->value, opt->name);
				if (retval != 0)
					break;
			}

			if (port->lun_map != NULL) {
				sbuf_printf(sb, "\t<lun_map>on</lun_map>\n");
				for (j = 0; j < CTL_MAX_LUNS; j++) {
					plun = ctl_lun_map_from_port(port, j);
					if (plun >= CTL_MAX_LUNS)
						continue;
					sbuf_printf(sb,
					    "\t<lun id=\"%u\">%u</lun>\n",
					    j, plun);
				}
			}

			for (j = 0; j < CTL_MAX_INIT_PER_PORT; j++) {
				if (port->wwpn_iid[j].in_use == 0 ||
				    (port->wwpn_iid[j].wwpn == 0 &&
				     port->wwpn_iid[j].name == NULL))
					continue;

				if (port->wwpn_iid[j].name != NULL)
					retval = sbuf_printf(sb,
					    "\t<initiator id=\"%u\">%s</initiator>\n",
					    j, port->wwpn_iid[j].name);
				else
					retval = sbuf_printf(sb,
					    "\t<initiator id=\"%u\">naa.%08jx</initiator>\n",
					    j, port->wwpn_iid[j].wwpn);
				if (retval != 0)
					break;
			}
			if (retval != 0)
				break;

			retval = sbuf_printf(sb, "</targ_port>\n");
			if (retval != 0)
				break;
		}
		mtx_unlock(&softc->ctl_lock);

		if ((retval != 0)
		 || ((retval = sbuf_printf(sb, "</ctlportlist>\n")) != 0)) {
			retval = 0;
			sbuf_delete(sb);
			list->status = CTL_LUN_LIST_NEED_MORE_SPACE;
			snprintf(list->error_str, sizeof(list->error_str),
				 "Out of space, %d bytes is too small",
				 list->alloc_len);
			break;
		}

		sbuf_finish(sb);

		retval = copyout(sbuf_data(sb), list->lun_xml,
				 sbuf_len(sb) + 1);

		list->fill_len = sbuf_len(sb) + 1;
		list->status = CTL_LUN_LIST_OK;
		sbuf_delete(sb);
		break;
	}
	case CTL_LUN_MAP: {
		struct ctl_lun_map *lm  = (struct ctl_lun_map *)addr;
		struct ctl_port *port;

		mtx_lock(&softc->ctl_lock);
		if (lm->port >= CTL_MAX_PORTS ||
		    (port = softc->ctl_ports[lm->port]) == NULL) {
			mtx_unlock(&softc->ctl_lock);
			return (ENXIO);
		}
		mtx_unlock(&softc->ctl_lock); // XXX: port_enable sleeps
		if (lm->plun < CTL_MAX_LUNS) {
			if (lm->lun == UINT32_MAX)
				retval = ctl_lun_map_unset(port, lm->plun);
			else if (lm->lun < CTL_MAX_LUNS &&
			    softc->ctl_luns[lm->lun] != NULL)
				retval = ctl_lun_map_set(port, lm->plun, lm->lun);
			else
				return (ENXIO);
		} else if (lm->plun == UINT32_MAX) {
			if (lm->lun == UINT32_MAX)
				retval = ctl_lun_map_deinit(port);
			else
				retval = ctl_lun_map_init(port);
		} else
			return (ENXIO);
		break;
	}
	default: {
		/* XXX KDM should we fix this? */
#if 0
		struct ctl_backend_driver *backend;
		unsigned int type;
		int found;

		found = 0;

		/*
		 * We encode the backend type as the ioctl type for backend
		 * ioctls.  So parse it out here, and then search for a
		 * backend of this type.
		 */
		type = _IOC_TYPE(cmd);

		STAILQ_FOREACH(backend, &softc->be_list, links) {
			if (backend->type == type) {
				found = 1;
				break;
			}
		}
		if (found == 0) {
			printf("ctl: unknown ioctl command %#lx or backend "
			       "%d\n", cmd, type);
			retval = EINVAL;
			break;
		}
		retval = backend->ioctl(dev, cmd, addr, flag, td);
#endif
		retval = ENOTTY;
		break;
	}
	}
	return (retval);
}

uint32_t
ctl_get_initindex(struct ctl_nexus *nexus)
{
	if (nexus->targ_port < CTL_MAX_PORTS)
		return (nexus->initid.id +
			(nexus->targ_port * CTL_MAX_INIT_PER_PORT));
	else
		return (nexus->initid.id +
		       ((nexus->targ_port - CTL_MAX_PORTS) *
			CTL_MAX_INIT_PER_PORT));
}

uint32_t
ctl_get_resindex(struct ctl_nexus *nexus)
{
	return (nexus->initid.id + (nexus->targ_port * CTL_MAX_INIT_PER_PORT));
}

uint32_t
ctl_port_idx(int port_num)
{
	if (port_num < CTL_MAX_PORTS)
		return(port_num);
	else
		return(port_num - CTL_MAX_PORTS);
}

int
ctl_lun_map_init(struct ctl_port *port)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_lun *lun;
	uint32_t i;

	if (port->lun_map == NULL)
		port->lun_map = malloc(sizeof(uint32_t) * CTL_MAX_LUNS,
		    M_CTL, M_NOWAIT);
	if (port->lun_map == NULL)
		return (ENOMEM);
	for (i = 0; i < CTL_MAX_LUNS; i++)
		port->lun_map[i] = UINT32_MAX;
	if (port->status & CTL_PORT_STATUS_ONLINE) {
		STAILQ_FOREACH(lun, &softc->lun_list, links)
			port->lun_disable(port->targ_lun_arg, lun->lun);
	}
	return (0);
}

int
ctl_lun_map_deinit(struct ctl_port *port)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_lun *lun;

	if (port->lun_map == NULL)
		return (0);
	free(port->lun_map, M_CTL);
	port->lun_map = NULL;
	if (port->status & CTL_PORT_STATUS_ONLINE) {
		STAILQ_FOREACH(lun, &softc->lun_list, links)
			port->lun_enable(port->targ_lun_arg, lun->lun);
	}
	return (0);
}

int
ctl_lun_map_set(struct ctl_port *port, uint32_t plun, uint32_t glun)
{
	int status;
	uint32_t old;

	if (port->lun_map == NULL) {
		status = ctl_lun_map_init(port);
		if (status != 0)
			return (status);
	}
	old = port->lun_map[plun];
	port->lun_map[plun] = glun;
	if ((port->status & CTL_PORT_STATUS_ONLINE) && old >= CTL_MAX_LUNS)
		port->lun_enable(port->targ_lun_arg, plun);
	return (0);
}

int
ctl_lun_map_unset(struct ctl_port *port, uint32_t plun)
{
	uint32_t old;

	if (port->lun_map == NULL)
		return (0);
	old = port->lun_map[plun];
	port->lun_map[plun] = UINT32_MAX;
	if ((port->status & CTL_PORT_STATUS_ONLINE) && old < CTL_MAX_LUNS)
		port->lun_disable(port->targ_lun_arg, plun);
	return (0);
}

uint32_t
ctl_lun_map_from_port(struct ctl_port *port, uint32_t lun_id)
{

	if (port == NULL)
		return (UINT32_MAX);
	if (port->lun_map == NULL || lun_id >= CTL_MAX_LUNS)
		return (lun_id);
	return (port->lun_map[lun_id]);
}

uint32_t
ctl_lun_map_to_port(struct ctl_port *port, uint32_t lun_id)
{
	uint32_t i;

	if (port == NULL)
		return (UINT32_MAX);
	if (port->lun_map == NULL)
		return (lun_id);
	for (i = 0; i < CTL_MAX_LUNS; i++) {
		if (port->lun_map[i] == lun_id)
			return (i);
	}
	return (UINT32_MAX);
}

static struct ctl_port *
ctl_io_port(struct ctl_io_hdr *io_hdr)
{
	int port_num;

	port_num = io_hdr->nexus.targ_port;
	return (control_softc->ctl_ports[ctl_port_idx(port_num)]);
}

/*
 * Note:  This only works for bitmask sizes that are at least 32 bits, and
 * that are a power of 2.
 */
int
ctl_ffz(uint32_t *mask, uint32_t size)
{
	uint32_t num_chunks, num_pieces;
	int i, j;

	num_chunks = (size >> 5);
	if (num_chunks == 0)
		num_chunks++;
	num_pieces = MIN((sizeof(uint32_t) * 8), size);

	for (i = 0; i < num_chunks; i++) {
		for (j = 0; j < num_pieces; j++) {
			if ((mask[i] & (1 << j)) == 0)
				return ((i << 5) + j);
		}
	}

	return (-1);
}

int
ctl_set_mask(uint32_t *mask, uint32_t bit)
{
	uint32_t chunk, piece;

	chunk = bit >> 5;
	piece = bit % (sizeof(uint32_t) * 8);

	if ((mask[chunk] & (1 << piece)) != 0)
		return (-1);
	else
		mask[chunk] |= (1 << piece);

	return (0);
}

int
ctl_clear_mask(uint32_t *mask, uint32_t bit)
{
	uint32_t chunk, piece;

	chunk = bit >> 5;
	piece = bit % (sizeof(uint32_t) * 8);

	if ((mask[chunk] & (1 << piece)) == 0)
		return (-1);
	else
		mask[chunk] &= ~(1 << piece);

	return (0);
}

int
ctl_is_set(uint32_t *mask, uint32_t bit)
{
	uint32_t chunk, piece;

	chunk = bit >> 5;
	piece = bit % (sizeof(uint32_t) * 8);

	if ((mask[chunk] & (1 << piece)) == 0)
		return (0);
	else
		return (1);
}

static uint64_t
ctl_get_prkey(struct ctl_lun *lun, uint32_t residx)
{
	uint64_t *t;

	t = lun->pr_keys[residx/CTL_MAX_INIT_PER_PORT];
	if (t == NULL)
		return (0);
	return (t[residx % CTL_MAX_INIT_PER_PORT]);
}

static void
ctl_clr_prkey(struct ctl_lun *lun, uint32_t residx)
{
	uint64_t *t;

	t = lun->pr_keys[residx/CTL_MAX_INIT_PER_PORT];
	if (t == NULL)
		return;
	t[residx % CTL_MAX_INIT_PER_PORT] = 0;
}

static void
ctl_alloc_prkey(struct ctl_lun *lun, uint32_t residx)
{
	uint64_t *p;
	u_int i;

	i = residx/CTL_MAX_INIT_PER_PORT;
	if (lun->pr_keys[i] != NULL)
		return;
	mtx_unlock(&lun->lun_lock);
	p = malloc(sizeof(uint64_t) * CTL_MAX_INIT_PER_PORT, M_CTL,
	    M_WAITOK | M_ZERO);
	mtx_lock(&lun->lun_lock);
	if (lun->pr_keys[i] == NULL)
		lun->pr_keys[i] = p;
	else
		free(p, M_CTL);
}

static void
ctl_set_prkey(struct ctl_lun *lun, uint32_t residx, uint64_t key)
{
	uint64_t *t;

	t = lun->pr_keys[residx/CTL_MAX_INIT_PER_PORT];
	KASSERT(t != NULL, ("prkey %d is not allocated", residx));
	t[residx % CTL_MAX_INIT_PER_PORT] = key;
}

/*
 * ctl_softc, pool_name, total_ctl_io are passed in.
 * npool is passed out.
 */
int
ctl_pool_create(struct ctl_softc *ctl_softc, const char *pool_name,
		uint32_t total_ctl_io, void **npool)
{
#ifdef IO_POOLS
	struct ctl_io_pool *pool;

	pool = (struct ctl_io_pool *)malloc(sizeof(*pool), M_CTL,
					    M_NOWAIT | M_ZERO);
	if (pool == NULL)
		return (ENOMEM);

	snprintf(pool->name, sizeof(pool->name), "CTL IO %s", pool_name);
	pool->ctl_softc = ctl_softc;
	pool->zone = uma_zsecond_create(pool->name, NULL,
	    NULL, NULL, NULL, ctl_softc->io_zone);
	/* uma_prealloc(pool->zone, total_ctl_io); */

	*npool = pool;
#else
	*npool = ctl_softc->io_zone;
#endif
	return (0);
}

void
ctl_pool_free(struct ctl_io_pool *pool)
{

	if (pool == NULL)
		return;

#ifdef IO_POOLS
	uma_zdestroy(pool->zone);
	free(pool, M_CTL);
#endif
}

union ctl_io *
ctl_alloc_io(void *pool_ref)
{
	union ctl_io *io;
#ifdef IO_POOLS
	struct ctl_io_pool *pool = (struct ctl_io_pool *)pool_ref;

	io = uma_zalloc(pool->zone, M_WAITOK);
#else
	io = uma_zalloc((uma_zone_t)pool_ref, M_WAITOK);
#endif
	if (io != NULL)
		io->io_hdr.pool = pool_ref;
	return (io);
}

union ctl_io *
ctl_alloc_io_nowait(void *pool_ref)
{
	union ctl_io *io;
#ifdef IO_POOLS
	struct ctl_io_pool *pool = (struct ctl_io_pool *)pool_ref;

	io = uma_zalloc(pool->zone, M_NOWAIT);
#else
	io = uma_zalloc((uma_zone_t)pool_ref, M_NOWAIT);
#endif
	if (io != NULL)
		io->io_hdr.pool = pool_ref;
	return (io);
}

void
ctl_free_io(union ctl_io *io)
{
#ifdef IO_POOLS
	struct ctl_io_pool *pool;
#endif

	if (io == NULL)
		return;

#ifdef IO_POOLS
	pool = (struct ctl_io_pool *)io->io_hdr.pool;
	uma_zfree(pool->zone, io);
#else
	uma_zfree((uma_zone_t)io->io_hdr.pool, io);
#endif
}

void
ctl_zero_io(union ctl_io *io)
{
	void *pool_ref;

	if (io == NULL)
		return;

	/*
	 * May need to preserve linked list pointers at some point too.
	 */
	pool_ref = io->io_hdr.pool;
	memset(io, 0, sizeof(*io));
	io->io_hdr.pool = pool_ref;
}

/*
 * This routine is currently used for internal copies of ctl_ios that need
 * to persist for some reason after we've already returned status to the
 * FETD.  (Thus the flag set.)
 *
 * XXX XXX
 * Note that this makes a blind copy of all fields in the ctl_io, except
 * for the pool reference.  This includes any memory that has been
 * allocated!  That memory will no longer be valid after done has been
 * called, so this would be VERY DANGEROUS for command that actually does
 * any reads or writes.  Right now (11/7/2005), this is only used for immediate
 * start and stop commands, which don't transfer any data, so this is not a
 * problem.  If it is used for anything else, the caller would also need to
 * allocate data buffer space and this routine would need to be modified to
 * copy the data buffer(s) as well.
 */
void
ctl_copy_io(union ctl_io *src, union ctl_io *dest)
{
	void *pool_ref;

	if ((src == NULL)
	 || (dest == NULL))
		return;

	/*
	 * May need to preserve linked list pointers at some point too.
	 */
	pool_ref = dest->io_hdr.pool;

	memcpy(dest, src, MIN(sizeof(*src), sizeof(*dest)));

	dest->io_hdr.pool = pool_ref;
	/*
	 * We need to know that this is an internal copy, and doesn't need
	 * to get passed back to the FETD that allocated it.
	 */
	dest->io_hdr.flags |= CTL_FLAG_INT_COPY;
}

int
ctl_expand_number(const char *buf, uint64_t *num)
{
	char *endptr;
	uint64_t number;
	unsigned shift;

	number = strtoq(buf, &endptr, 0);

	switch (tolower((unsigned char)*endptr)) {
	case 'e':
		shift = 60;
		break;
	case 'p':
		shift = 50;
		break;
	case 't':
		shift = 40;
		break;
	case 'g':
		shift = 30;
		break;
	case 'm':
		shift = 20;
		break;
	case 'k':
		shift = 10;
		break;
	case 'b':
	case '\0': /* No unit. */
		*num = number;
		return (0);
	default:
		/* Unrecognized unit. */
		return (-1);
	}

	if ((number << shift) >> shift != number) {
		/* Overflow */
		return (-1);
	}
	*num = number << shift;
	return (0);
}


/*
 * This routine could be used in the future to load default and/or saved
 * mode page parameters for a particuar lun.
 */
static int
ctl_init_page_index(struct ctl_lun *lun)
{
	int i;
	struct ctl_page_index *page_index;
	const char *value;
	uint64_t ival;

	memcpy(&lun->mode_pages.index, page_index_template,
	       sizeof(page_index_template));

	for (i = 0; i < CTL_NUM_MODE_PAGES; i++) {

		page_index = &lun->mode_pages.index[i];
		/*
		 * If this is a disk-only mode page, there's no point in
		 * setting it up.  For some pages, we have to have some
		 * basic information about the disk in order to calculate the
		 * mode page data.
		 */
		if ((lun->be_lun->lun_type != T_DIRECT)
		 && (page_index->page_flags & CTL_PAGE_FLAG_DISK_ONLY))
			continue;

		switch (page_index->page_code & SMPH_PC_MASK) {
		case SMS_RW_ERROR_RECOVERY_PAGE: {
			if (page_index->subpage != SMS_SUBPAGE_PAGE_0)
				panic("subpage is incorrect!");
			memcpy(&lun->mode_pages.rw_er_page[CTL_PAGE_CURRENT],
			       &rw_er_page_default,
			       sizeof(rw_er_page_default));
			memcpy(&lun->mode_pages.rw_er_page[CTL_PAGE_CHANGEABLE],
			       &rw_er_page_changeable,
			       sizeof(rw_er_page_changeable));
			memcpy(&lun->mode_pages.rw_er_page[CTL_PAGE_DEFAULT],
			       &rw_er_page_default,
			       sizeof(rw_er_page_default));
			memcpy(&lun->mode_pages.rw_er_page[CTL_PAGE_SAVED],
			       &rw_er_page_default,
			       sizeof(rw_er_page_default));
			page_index->page_data =
				(uint8_t *)lun->mode_pages.rw_er_page;
			break;
		}
		case SMS_FORMAT_DEVICE_PAGE: {
			struct scsi_format_page *format_page;

			if (page_index->subpage != SMS_SUBPAGE_PAGE_0)
				panic("subpage is incorrect!");

			/*
			 * Sectors per track are set above.  Bytes per
			 * sector need to be set here on a per-LUN basis.
			 */
			memcpy(&lun->mode_pages.format_page[CTL_PAGE_CURRENT],
			       &format_page_default,
			       sizeof(format_page_default));
			memcpy(&lun->mode_pages.format_page[
			       CTL_PAGE_CHANGEABLE], &format_page_changeable,
			       sizeof(format_page_changeable));
			memcpy(&lun->mode_pages.format_page[CTL_PAGE_DEFAULT],
			       &format_page_default,
			       sizeof(format_page_default));
			memcpy(&lun->mode_pages.format_page[CTL_PAGE_SAVED],
			       &format_page_default,
			       sizeof(format_page_default));

			format_page = &lun->mode_pages.format_page[
				CTL_PAGE_CURRENT];
			scsi_ulto2b(lun->be_lun->blocksize,
				    format_page->bytes_per_sector);

			format_page = &lun->mode_pages.format_page[
				CTL_PAGE_DEFAULT];
			scsi_ulto2b(lun->be_lun->blocksize,
				    format_page->bytes_per_sector);

			format_page = &lun->mode_pages.format_page[
				CTL_PAGE_SAVED];
			scsi_ulto2b(lun->be_lun->blocksize,
				    format_page->bytes_per_sector);

			page_index->page_data =
				(uint8_t *)lun->mode_pages.format_page;
			break;
		}
		case SMS_RIGID_DISK_PAGE: {
			struct scsi_rigid_disk_page *rigid_disk_page;
			uint32_t sectors_per_cylinder;
			uint64_t cylinders;
#ifndef	__XSCALE__
			int shift;
#endif /* !__XSCALE__ */

			if (page_index->subpage != SMS_SUBPAGE_PAGE_0)
				panic("invalid subpage value %d",
				      page_index->subpage);

			/*
			 * Rotation rate and sectors per track are set
			 * above.  We calculate the cylinders here based on
			 * capacity.  Due to the number of heads and
			 * sectors per track we're using, smaller arrays
			 * may turn out to have 0 cylinders.  Linux and
			 * FreeBSD don't pay attention to these mode pages
			 * to figure out capacity, but Solaris does.  It
			 * seems to deal with 0 cylinders just fine, and
			 * works out a fake geometry based on the capacity.
			 */
			memcpy(&lun->mode_pages.rigid_disk_page[
			       CTL_PAGE_DEFAULT], &rigid_disk_page_default,
			       sizeof(rigid_disk_page_default));
			memcpy(&lun->mode_pages.rigid_disk_page[
			       CTL_PAGE_CHANGEABLE],&rigid_disk_page_changeable,
			       sizeof(rigid_disk_page_changeable));

			sectors_per_cylinder = CTL_DEFAULT_SECTORS_PER_TRACK *
				CTL_DEFAULT_HEADS;

			/*
			 * The divide method here will be more accurate,
			 * probably, but results in floating point being
			 * used in the kernel on i386 (__udivdi3()).  On the
			 * XScale, though, __udivdi3() is implemented in
			 * software.
			 *
			 * The shift method for cylinder calculation is
			 * accurate if sectors_per_cylinder is a power of
			 * 2.  Otherwise it might be slightly off -- you
			 * might have a bit of a truncation problem.
			 */
#ifdef	__XSCALE__
			cylinders = (lun->be_lun->maxlba + 1) /
				sectors_per_cylinder;
#else
			for (shift = 31; shift > 0; shift--) {
				if (sectors_per_cylinder & (1 << shift))
					break;
			}
			cylinders = (lun->be_lun->maxlba + 1) >> shift;
#endif

			/*
			 * We've basically got 3 bytes, or 24 bits for the
			 * cylinder size in the mode page.  If we're over,
			 * just round down to 2^24.
			 */
			if (cylinders > 0xffffff)
				cylinders = 0xffffff;

			rigid_disk_page = &lun->mode_pages.rigid_disk_page[
				CTL_PAGE_DEFAULT];
			scsi_ulto3b(cylinders, rigid_disk_page->cylinders);

			if ((value = ctl_get_opt(&lun->be_lun->options,
			    "rpm")) != NULL) {
				scsi_ulto2b(strtol(value, NULL, 0),
				     rigid_disk_page->rotation_rate);
			}

			memcpy(&lun->mode_pages.rigid_disk_page[CTL_PAGE_CURRENT],
			       &lun->mode_pages.rigid_disk_page[CTL_PAGE_DEFAULT],
			       sizeof(rigid_disk_page_default));
			memcpy(&lun->mode_pages.rigid_disk_page[CTL_PAGE_SAVED],
			       &lun->mode_pages.rigid_disk_page[CTL_PAGE_DEFAULT],
			       sizeof(rigid_disk_page_default));

			page_index->page_data =
				(uint8_t *)lun->mode_pages.rigid_disk_page;
			break;
		}
		case SMS_CACHING_PAGE: {
			struct scsi_caching_page *caching_page;

			if (page_index->subpage != SMS_SUBPAGE_PAGE_0)
				panic("invalid subpage value %d",
				      page_index->subpage);
			memcpy(&lun->mode_pages.caching_page[CTL_PAGE_DEFAULT],
			       &caching_page_default,
			       sizeof(caching_page_default));
			memcpy(&lun->mode_pages.caching_page[
			       CTL_PAGE_CHANGEABLE], &caching_page_changeable,
			       sizeof(caching_page_changeable));
			memcpy(&lun->mode_pages.caching_page[CTL_PAGE_SAVED],
			       &caching_page_default,
			       sizeof(caching_page_default));
			caching_page = &lun->mode_pages.caching_page[
			    CTL_PAGE_SAVED];
			value = ctl_get_opt(&lun->be_lun->options, "writecache");
			if (value != NULL && strcmp(value, "off") == 0)
				caching_page->flags1 &= ~SCP_WCE;
			value = ctl_get_opt(&lun->be_lun->options, "readcache");
			if (value != NULL && strcmp(value, "off") == 0)
				caching_page->flags1 |= SCP_RCD;
			memcpy(&lun->mode_pages.caching_page[CTL_PAGE_CURRENT],
			       &lun->mode_pages.caching_page[CTL_PAGE_SAVED],
			       sizeof(caching_page_default));
			page_index->page_data =
				(uint8_t *)lun->mode_pages.caching_page;
			break;
		}
		case SMS_CONTROL_MODE_PAGE: {
			struct scsi_control_page *control_page;

			if (page_index->subpage != SMS_SUBPAGE_PAGE_0)
				panic("invalid subpage value %d",
				      page_index->subpage);

			memcpy(&lun->mode_pages.control_page[CTL_PAGE_DEFAULT],
			       &control_page_default,
			       sizeof(control_page_default));
			memcpy(&lun->mode_pages.control_page[
			       CTL_PAGE_CHANGEABLE], &control_page_changeable,
			       sizeof(control_page_changeable));
			memcpy(&lun->mode_pages.control_page[CTL_PAGE_SAVED],
			       &control_page_default,
			       sizeof(control_page_default));
			control_page = &lun->mode_pages.control_page[
			    CTL_PAGE_SAVED];
			value = ctl_get_opt(&lun->be_lun->options, "reordering");
			if (value != NULL && strcmp(value, "unrestricted") == 0) {
				control_page->queue_flags &= ~SCP_QUEUE_ALG_MASK;
				control_page->queue_flags |= SCP_QUEUE_ALG_UNRESTRICTED;
			}
			memcpy(&lun->mode_pages.control_page[CTL_PAGE_CURRENT],
			       &lun->mode_pages.control_page[CTL_PAGE_SAVED],
			       sizeof(control_page_default));
			page_index->page_data =
				(uint8_t *)lun->mode_pages.control_page;
			break;

		}
		case SMS_INFO_EXCEPTIONS_PAGE: {
			switch (page_index->subpage) {
			case SMS_SUBPAGE_PAGE_0:
				memcpy(&lun->mode_pages.ie_page[CTL_PAGE_CURRENT],
				       &ie_page_default,
				       sizeof(ie_page_default));
				memcpy(&lun->mode_pages.ie_page[
				       CTL_PAGE_CHANGEABLE], &ie_page_changeable,
				       sizeof(ie_page_changeable));
				memcpy(&lun->mode_pages.ie_page[CTL_PAGE_DEFAULT],
				       &ie_page_default,
				       sizeof(ie_page_default));
				memcpy(&lun->mode_pages.ie_page[CTL_PAGE_SAVED],
				       &ie_page_default,
				       sizeof(ie_page_default));
				page_index->page_data =
					(uint8_t *)lun->mode_pages.ie_page;
				break;
			case 0x02: {
				struct ctl_logical_block_provisioning_page *page;

				memcpy(&lun->mode_pages.lbp_page[CTL_PAGE_DEFAULT],
				       &lbp_page_default,
				       sizeof(lbp_page_default));
				memcpy(&lun->mode_pages.lbp_page[
				       CTL_PAGE_CHANGEABLE], &lbp_page_changeable,
				       sizeof(lbp_page_changeable));
				memcpy(&lun->mode_pages.lbp_page[CTL_PAGE_SAVED],
				       &lbp_page_default,
				       sizeof(lbp_page_default));
				page = &lun->mode_pages.lbp_page[CTL_PAGE_SAVED];
				value = ctl_get_opt(&lun->be_lun->options,
				    "avail-threshold");
				if (value != NULL &&
				    ctl_expand_number(value, &ival) == 0) {
					page->descr[0].flags |= SLBPPD_ENABLED |
					    SLBPPD_ARMING_DEC;
					if (lun->be_lun->blocksize)
						ival /= lun->be_lun->blocksize;
					else
						ival /= 512;
					scsi_ulto4b(ival >> CTL_LBP_EXPONENT,
					    page->descr[0].count);
				}
				value = ctl_get_opt(&lun->be_lun->options,
				    "used-threshold");
				if (value != NULL &&
				    ctl_expand_number(value, &ival) == 0) {
					page->descr[1].flags |= SLBPPD_ENABLED |
					    SLBPPD_ARMING_INC;
					if (lun->be_lun->blocksize)
						ival /= lun->be_lun->blocksize;
					else
						ival /= 512;
					scsi_ulto4b(ival >> CTL_LBP_EXPONENT,
					    page->descr[1].count);
				}
				value = ctl_get_opt(&lun->be_lun->options,
				    "pool-avail-threshold");
				if (value != NULL &&
				    ctl_expand_number(value, &ival) == 0) {
					page->descr[2].flags |= SLBPPD_ENABLED |
					    SLBPPD_ARMING_DEC;
					if (lun->be_lun->blocksize)
						ival /= lun->be_lun->blocksize;
					else
						ival /= 512;
					scsi_ulto4b(ival >> CTL_LBP_EXPONENT,
					    page->descr[2].count);
				}
				value = ctl_get_opt(&lun->be_lun->options,
				    "pool-used-threshold");
				if (value != NULL &&
				    ctl_expand_number(value, &ival) == 0) {
					page->descr[3].flags |= SLBPPD_ENABLED |
					    SLBPPD_ARMING_INC;
					if (lun->be_lun->blocksize)
						ival /= lun->be_lun->blocksize;
					else
						ival /= 512;
					scsi_ulto4b(ival >> CTL_LBP_EXPONENT,
					    page->descr[3].count);
				}
				memcpy(&lun->mode_pages.lbp_page[CTL_PAGE_CURRENT],
				       &lun->mode_pages.lbp_page[CTL_PAGE_SAVED],
				       sizeof(lbp_page_default));
				page_index->page_data =
					(uint8_t *)lun->mode_pages.lbp_page;
			}}
			break;
		}
		case SMS_VENDOR_SPECIFIC_PAGE:{
			switch (page_index->subpage) {
			case DBGCNF_SUBPAGE_CODE: {
				struct copan_debugconf_subpage *current_page,
							       *saved_page;

				memcpy(&lun->mode_pages.debugconf_subpage[
				       CTL_PAGE_CURRENT],
				       &debugconf_page_default,
				       sizeof(debugconf_page_default));
				memcpy(&lun->mode_pages.debugconf_subpage[
				       CTL_PAGE_CHANGEABLE],
				       &debugconf_page_changeable,
				       sizeof(debugconf_page_changeable));
				memcpy(&lun->mode_pages.debugconf_subpage[
				       CTL_PAGE_DEFAULT],
				       &debugconf_page_default,
				       sizeof(debugconf_page_default));
				memcpy(&lun->mode_pages.debugconf_subpage[
				       CTL_PAGE_SAVED],
				       &debugconf_page_default,
				       sizeof(debugconf_page_default));
				page_index->page_data =
					(uint8_t *)lun->mode_pages.debugconf_subpage;

				current_page = (struct copan_debugconf_subpage *)
					(page_index->page_data +
					 (page_index->page_len *
					  CTL_PAGE_CURRENT));
				saved_page = (struct copan_debugconf_subpage *)
					(page_index->page_data +
					 (page_index->page_len *
					  CTL_PAGE_SAVED));
				break;
			}
			default:
				panic("invalid subpage value %d",
				      page_index->subpage);
				break;
			}
   			break;
		}
		default:
			panic("invalid page value %d",
			      page_index->page_code & SMPH_PC_MASK);
			break;
    	}
	}

	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_init_log_page_index(struct ctl_lun *lun)
{
	struct ctl_page_index *page_index;
	int i, j, k, prev;

	memcpy(&lun->log_pages.index, log_page_index_template,
	       sizeof(log_page_index_template));

	prev = -1;
	for (i = 0, j = 0, k = 0; i < CTL_NUM_LOG_PAGES; i++) {

		page_index = &lun->log_pages.index[i];
		/*
		 * If this is a disk-only mode page, there's no point in
		 * setting it up.  For some pages, we have to have some
		 * basic information about the disk in order to calculate the
		 * mode page data.
		 */
		if ((lun->be_lun->lun_type != T_DIRECT)
		 && (page_index->page_flags & CTL_PAGE_FLAG_DISK_ONLY))
			continue;

		if (page_index->page_code == SLS_LOGICAL_BLOCK_PROVISIONING &&
		     lun->backend->lun_attr == NULL)
			continue;

		if (page_index->page_code != prev) {
			lun->log_pages.pages_page[j] = page_index->page_code;
			prev = page_index->page_code;
			j++;
		}
		lun->log_pages.subpages_page[k*2] = page_index->page_code;
		lun->log_pages.subpages_page[k*2+1] = page_index->subpage;
		k++;
	}
	lun->log_pages.index[0].page_data = &lun->log_pages.pages_page[0];
	lun->log_pages.index[0].page_len = j;
	lun->log_pages.index[1].page_data = &lun->log_pages.subpages_page[0];
	lun->log_pages.index[1].page_len = k * 2;
	lun->log_pages.index[2].page_data = &lun->log_pages.lbp_page[0];
	lun->log_pages.index[2].page_len = 12*CTL_NUM_LBP_PARAMS;
	lun->log_pages.index[3].page_data = (uint8_t *)&lun->log_pages.stat_page;
	lun->log_pages.index[3].page_len = sizeof(lun->log_pages.stat_page);

	return (CTL_RETVAL_COMPLETE);
}

static int
hex2bin(const char *str, uint8_t *buf, int buf_size)
{
	int i;
	u_char c;

	memset(buf, 0, buf_size);
	while (isspace(str[0]))
		str++;
	if (str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
		str += 2;
	buf_size *= 2;
	for (i = 0; str[i] != 0 && i < buf_size; i++) {
		c = str[i];
		if (isdigit(c))
			c -= '0';
		else if (isalpha(c))
			c -= isupper(c) ? 'A' - 10 : 'a' - 10;
		else
			break;
		if (c >= 16)
			break;
		if ((i & 1) == 0)
			buf[i / 2] |= (c << 4);
		else
			buf[i / 2] |= c;
	}
	return ((i + 1) / 2);
}

/*
 * LUN allocation.
 *
 * Requirements:
 * - caller allocates and zeros LUN storage, or passes in a NULL LUN if he
 *   wants us to allocate the LUN and he can block.
 * - ctl_softc is always set
 * - be_lun is set if the LUN has a backend (needed for disk LUNs)
 *
 * Returns 0 for success, non-zero (errno) for failure.
 */
static int
ctl_alloc_lun(struct ctl_softc *ctl_softc, struct ctl_lun *ctl_lun,
	      struct ctl_be_lun *const be_lun)
{
	struct ctl_lun *nlun, *lun;
	struct scsi_vpd_id_descriptor *desc;
	struct scsi_vpd_id_t10 *t10id;
	const char *eui, *naa, *scsiname, *vendor, *value;
	int lun_number, i, lun_malloced;
	int devidlen, idlen1, idlen2 = 0, len;

	if (be_lun == NULL)
		return (EINVAL);

	/*
	 * We currently only support Direct Access or Processor LUN types.
	 */
	switch (be_lun->lun_type) {
	case T_DIRECT:
		break;
	case T_PROCESSOR:
		break;
	case T_SEQUENTIAL:
	case T_CHANGER:
	default:
		be_lun->lun_config_status(be_lun->be_lun,
					  CTL_LUN_CONFIG_FAILURE);
		break;
	}
	if (ctl_lun == NULL) {
		lun = malloc(sizeof(*lun), M_CTL, M_WAITOK);
		lun_malloced = 1;
	} else {
		lun_malloced = 0;
		lun = ctl_lun;
	}

	memset(lun, 0, sizeof(*lun));
	if (lun_malloced)
		lun->flags = CTL_LUN_MALLOCED;

	/* Generate LUN ID. */
	devidlen = max(CTL_DEVID_MIN_LEN,
	    strnlen(be_lun->device_id, CTL_DEVID_LEN));
	idlen1 = sizeof(*t10id) + devidlen;
	len = sizeof(struct scsi_vpd_id_descriptor) + idlen1;
	scsiname = ctl_get_opt(&be_lun->options, "scsiname");
	if (scsiname != NULL) {
		idlen2 = roundup2(strlen(scsiname) + 1, 4);
		len += sizeof(struct scsi_vpd_id_descriptor) + idlen2;
	}
	eui = ctl_get_opt(&be_lun->options, "eui");
	if (eui != NULL) {
		len += sizeof(struct scsi_vpd_id_descriptor) + 16;
	}
	naa = ctl_get_opt(&be_lun->options, "naa");
	if (naa != NULL) {
		len += sizeof(struct scsi_vpd_id_descriptor) + 16;
	}
	lun->lun_devid = malloc(sizeof(struct ctl_devid) + len,
	    M_CTL, M_WAITOK | M_ZERO);
	desc = (struct scsi_vpd_id_descriptor *)lun->lun_devid->data;
	desc->proto_codeset = SVPD_ID_CODESET_ASCII;
	desc->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_LUN | SVPD_ID_TYPE_T10;
	desc->length = idlen1;
	t10id = (struct scsi_vpd_id_t10 *)&desc->identifier[0];
	memset(t10id->vendor, ' ', sizeof(t10id->vendor));
	if ((vendor = ctl_get_opt(&be_lun->options, "vendor")) == NULL) {
		strncpy((char *)t10id->vendor, CTL_VENDOR, sizeof(t10id->vendor));
	} else {
		strncpy(t10id->vendor, vendor,
		    min(sizeof(t10id->vendor), strlen(vendor)));
	}
	strncpy((char *)t10id->vendor_spec_id,
	    (char *)be_lun->device_id, devidlen);
	if (scsiname != NULL) {
		desc = (struct scsi_vpd_id_descriptor *)(&desc->identifier[0] +
		    desc->length);
		desc->proto_codeset = SVPD_ID_CODESET_UTF8;
		desc->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_LUN |
		    SVPD_ID_TYPE_SCSI_NAME;
		desc->length = idlen2;
		strlcpy(desc->identifier, scsiname, idlen2);
	}
	if (eui != NULL) {
		desc = (struct scsi_vpd_id_descriptor *)(&desc->identifier[0] +
		    desc->length);
		desc->proto_codeset = SVPD_ID_CODESET_BINARY;
		desc->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_LUN |
		    SVPD_ID_TYPE_EUI64;
		desc->length = hex2bin(eui, desc->identifier, 16);
		desc->length = desc->length > 12 ? 16 :
		    (desc->length > 8 ? 12 : 8);
		len -= 16 - desc->length;
	}
	if (naa != NULL) {
		desc = (struct scsi_vpd_id_descriptor *)(&desc->identifier[0] +
		    desc->length);
		desc->proto_codeset = SVPD_ID_CODESET_BINARY;
		desc->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_LUN |
		    SVPD_ID_TYPE_NAA;
		desc->length = hex2bin(naa, desc->identifier, 16);
		desc->length = desc->length > 8 ? 16 : 8;
		len -= 16 - desc->length;
	}
	lun->lun_devid->len = len;

	mtx_lock(&ctl_softc->ctl_lock);
	/*
	 * See if the caller requested a particular LUN number.  If so, see
	 * if it is available.  Otherwise, allocate the first available LUN.
	 */
	if (be_lun->flags & CTL_LUN_FLAG_ID_REQ) {
		if ((be_lun->req_lun_id > (CTL_MAX_LUNS - 1))
		 || (ctl_is_set(ctl_softc->ctl_lun_mask, be_lun->req_lun_id))) {
			mtx_unlock(&ctl_softc->ctl_lock);
			if (be_lun->req_lun_id > (CTL_MAX_LUNS - 1)) {
				printf("ctl: requested LUN ID %d is higher "
				       "than CTL_MAX_LUNS - 1 (%d)\n",
				       be_lun->req_lun_id, CTL_MAX_LUNS - 1);
			} else {
				/*
				 * XXX KDM return an error, or just assign
				 * another LUN ID in this case??
				 */
				printf("ctl: requested LUN ID %d is already "
				       "in use\n", be_lun->req_lun_id);
			}
			if (lun->flags & CTL_LUN_MALLOCED)
				free(lun, M_CTL);
			be_lun->lun_config_status(be_lun->be_lun,
						  CTL_LUN_CONFIG_FAILURE);
			return (ENOSPC);
		}
		lun_number = be_lun->req_lun_id;
	} else {
		lun_number = ctl_ffz(ctl_softc->ctl_lun_mask, CTL_MAX_LUNS);
		if (lun_number == -1) {
			mtx_unlock(&ctl_softc->ctl_lock);
			printf("ctl: can't allocate LUN, out of LUNs\n");
			if (lun->flags & CTL_LUN_MALLOCED)
				free(lun, M_CTL);
			be_lun->lun_config_status(be_lun->be_lun,
						  CTL_LUN_CONFIG_FAILURE);
			return (ENOSPC);
		}
	}
	ctl_set_mask(ctl_softc->ctl_lun_mask, lun_number);

	mtx_init(&lun->lun_lock, "CTL LUN", NULL, MTX_DEF);
	lun->lun = lun_number;
	lun->be_lun = be_lun;
	/*
	 * The processor LUN is always enabled.  Disk LUNs come on line
	 * disabled, and must be enabled by the backend.
	 */
	lun->flags |= CTL_LUN_DISABLED;
	lun->backend = be_lun->be;
	be_lun->ctl_lun = lun;
	be_lun->lun_id = lun_number;
	atomic_add_int(&be_lun->be->num_luns, 1);
	if (be_lun->flags & CTL_LUN_FLAG_OFFLINE)
		lun->flags |= CTL_LUN_OFFLINE;

	if (be_lun->flags & CTL_LUN_FLAG_POWERED_OFF)
		lun->flags |= CTL_LUN_STOPPED;

	if (be_lun->flags & CTL_LUN_FLAG_INOPERABLE)
		lun->flags |= CTL_LUN_INOPERABLE;

	if (be_lun->flags & CTL_LUN_FLAG_PRIMARY)
		lun->flags |= CTL_LUN_PRIMARY_SC;

	value = ctl_get_opt(&be_lun->options, "readonly");
	if (value != NULL && strcmp(value, "on") == 0)
		lun->flags |= CTL_LUN_READONLY;

	lun->serseq = CTL_LUN_SERSEQ_OFF;
	if (be_lun->flags & CTL_LUN_FLAG_SERSEQ_READ)
		lun->serseq = CTL_LUN_SERSEQ_READ;
	value = ctl_get_opt(&be_lun->options, "serseq");
	if (value != NULL && strcmp(value, "on") == 0)
		lun->serseq = CTL_LUN_SERSEQ_ON;
	else if (value != NULL && strcmp(value, "read") == 0)
		lun->serseq = CTL_LUN_SERSEQ_READ;
	else if (value != NULL && strcmp(value, "off") == 0)
		lun->serseq = CTL_LUN_SERSEQ_OFF;

	lun->ctl_softc = ctl_softc;
#ifdef CTL_TIME_IO
	lun->last_busy = getsbinuptime();
#endif
	TAILQ_INIT(&lun->ooa_queue);
	TAILQ_INIT(&lun->blocked_queue);
	STAILQ_INIT(&lun->error_list);
	ctl_tpc_lun_init(lun);

	/*
	 * Initialize the mode and log page index.
	 */
	ctl_init_page_index(lun);
	ctl_init_log_page_index(lun);

	/*
	 * Now, before we insert this lun on the lun list, set the lun
	 * inventory changed UA for all other luns.
	 */
	STAILQ_FOREACH(nlun, &ctl_softc->lun_list, links) {
		mtx_lock(&nlun->lun_lock);
		ctl_est_ua_all(nlun, -1, CTL_UA_LUN_CHANGE);
		mtx_unlock(&nlun->lun_lock);
	}

	STAILQ_INSERT_TAIL(&ctl_softc->lun_list, lun, links);

	ctl_softc->ctl_luns[lun_number] = lun;

	ctl_softc->num_luns++;

	/* Setup statistics gathering */
	lun->stats.device_type = be_lun->lun_type;
	lun->stats.lun_number = lun_number;
	if (lun->stats.device_type == T_DIRECT)
		lun->stats.blocksize = be_lun->blocksize;
	else
		lun->stats.flags = CTL_LUN_STATS_NO_BLOCKSIZE;
	for (i = 0;i < CTL_MAX_PORTS;i++)
		lun->stats.ports[i].targ_port = i;

	mtx_unlock(&ctl_softc->ctl_lock);

	lun->be_lun->lun_config_status(lun->be_lun->be_lun, CTL_LUN_CONFIG_OK);
	return (0);
}

/*
 * Delete a LUN.
 * Assumptions:
 * - LUN has already been marked invalid and any pending I/O has been taken
 *   care of.
 */
static int
ctl_free_lun(struct ctl_lun *lun)
{
	struct ctl_softc *softc;
	struct ctl_lun *nlun;
	int i;

	softc = lun->ctl_softc;

	mtx_assert(&softc->ctl_lock, MA_OWNED);

	STAILQ_REMOVE(&softc->lun_list, lun, ctl_lun, links);

	ctl_clear_mask(softc->ctl_lun_mask, lun->lun);

	softc->ctl_luns[lun->lun] = NULL;

	if (!TAILQ_EMPTY(&lun->ooa_queue))
		panic("Freeing a LUN %p with outstanding I/O!!\n", lun);

	softc->num_luns--;

	/*
	 * Tell the backend to free resources, if this LUN has a backend.
	 */
	atomic_subtract_int(&lun->be_lun->be->num_luns, 1);
	lun->be_lun->lun_shutdown(lun->be_lun->be_lun);

	ctl_tpc_lun_shutdown(lun);
	mtx_destroy(&lun->lun_lock);
	free(lun->lun_devid, M_CTL);
	for (i = 0; i < CTL_MAX_PORTS; i++)
		free(lun->pending_ua[i], M_CTL);
	for (i = 0; i < 2 * CTL_MAX_PORTS; i++)
		free(lun->pr_keys[i], M_CTL);
	free(lun->write_buffer, M_CTL);
	if (lun->flags & CTL_LUN_MALLOCED)
		free(lun, M_CTL);

	STAILQ_FOREACH(nlun, &softc->lun_list, links) {
		mtx_lock(&nlun->lun_lock);
		ctl_est_ua_all(nlun, -1, CTL_UA_LUN_CHANGE);
		mtx_unlock(&nlun->lun_lock);
	}

	return (0);
}

static void
ctl_create_lun(struct ctl_be_lun *be_lun)
{
	struct ctl_softc *softc;

	softc = control_softc;

	/*
	 * ctl_alloc_lun() should handle all potential failure cases.
	 */
	ctl_alloc_lun(softc, NULL, be_lun);
}

int
ctl_add_lun(struct ctl_be_lun *be_lun)
{
	struct ctl_softc *softc = control_softc;

	mtx_lock(&softc->ctl_lock);
	STAILQ_INSERT_TAIL(&softc->pending_lun_queue, be_lun, links);
	mtx_unlock(&softc->ctl_lock);
	wakeup(&softc->pending_lun_queue);

	return (0);
}

int
ctl_enable_lun(struct ctl_be_lun *be_lun)
{
	struct ctl_softc *softc;
	struct ctl_port *port, *nport;
	struct ctl_lun *lun;
	int retval;

	lun = (struct ctl_lun *)be_lun->ctl_lun;
	softc = lun->ctl_softc;

	mtx_lock(&softc->ctl_lock);
	mtx_lock(&lun->lun_lock);
	if ((lun->flags & CTL_LUN_DISABLED) == 0) {
		/*
		 * eh?  Why did we get called if the LUN is already
		 * enabled?
		 */
		mtx_unlock(&lun->lun_lock);
		mtx_unlock(&softc->ctl_lock);
		return (0);
	}
	lun->flags &= ~CTL_LUN_DISABLED;
	mtx_unlock(&lun->lun_lock);

	for (port = STAILQ_FIRST(&softc->port_list); port != NULL; port = nport) {
		nport = STAILQ_NEXT(port, links);
		if ((port->status & CTL_PORT_STATUS_ONLINE) == 0 ||
		    port->lun_map != NULL)
			continue;

		/*
		 * Drop the lock while we call the FETD's enable routine.
		 * This can lead to a callback into CTL (at least in the
		 * case of the internal initiator frontend.
		 */
		mtx_unlock(&softc->ctl_lock);
		retval = port->lun_enable(port->targ_lun_arg, lun->lun);
		mtx_lock(&softc->ctl_lock);
		if (retval != 0) {
			printf("%s: FETD %s port %d returned error "
			       "%d for lun_enable on lun %jd\n",
			       __func__, port->port_name, port->targ_port,
			       retval, (intmax_t)lun->lun);
		}
	}

	mtx_unlock(&softc->ctl_lock);

	return (0);
}

int
ctl_disable_lun(struct ctl_be_lun *be_lun)
{
	struct ctl_softc *softc;
	struct ctl_port *port;
	struct ctl_lun *lun;
	int retval;

	lun = (struct ctl_lun *)be_lun->ctl_lun;
	softc = lun->ctl_softc;

	mtx_lock(&softc->ctl_lock);
	mtx_lock(&lun->lun_lock);
	if (lun->flags & CTL_LUN_DISABLED) {
		mtx_unlock(&lun->lun_lock);
		mtx_unlock(&softc->ctl_lock);
		return (0);
	}
	lun->flags |= CTL_LUN_DISABLED;
	mtx_unlock(&lun->lun_lock);

	STAILQ_FOREACH(port, &softc->port_list, links) {
		if ((port->status & CTL_PORT_STATUS_ONLINE) == 0 ||
		    port->lun_map != NULL)
			continue;
		mtx_unlock(&softc->ctl_lock);
		/*
		 * Drop the lock before we call the frontend's disable
		 * routine, to avoid lock order reversals.
		 *
		 * XXX KDM what happens if the frontend list changes while
		 * we're traversing it?  It's unlikely, but should be handled.
		 */
		retval = port->lun_disable(port->targ_lun_arg, lun->lun);
		mtx_lock(&softc->ctl_lock);
		if (retval != 0) {
			printf("%s: FETD %s port %d returned error "
			       "%d for lun_disable on lun %jd\n",
			       __func__, port->port_name, port->targ_port,
			       retval, (intmax_t)lun->lun);
		}
	}

	mtx_unlock(&softc->ctl_lock);

	return (0);
}

int
ctl_start_lun(struct ctl_be_lun *be_lun)
{
	struct ctl_lun *lun = (struct ctl_lun *)be_lun->ctl_lun;

	mtx_lock(&lun->lun_lock);
	lun->flags &= ~CTL_LUN_STOPPED;
	mtx_unlock(&lun->lun_lock);
	return (0);
}

int
ctl_stop_lun(struct ctl_be_lun *be_lun)
{
	struct ctl_lun *lun = (struct ctl_lun *)be_lun->ctl_lun;

	mtx_lock(&lun->lun_lock);
	lun->flags |= CTL_LUN_STOPPED;
	mtx_unlock(&lun->lun_lock);
	return (0);
}

int
ctl_lun_offline(struct ctl_be_lun *be_lun)
{
	struct ctl_lun *lun = (struct ctl_lun *)be_lun->ctl_lun;

	mtx_lock(&lun->lun_lock);
	lun->flags |= CTL_LUN_OFFLINE;
	mtx_unlock(&lun->lun_lock);
	return (0);
}

int
ctl_lun_online(struct ctl_be_lun *be_lun)
{
	struct ctl_lun *lun = (struct ctl_lun *)be_lun->ctl_lun;

	mtx_lock(&lun->lun_lock);
	lun->flags &= ~CTL_LUN_OFFLINE;
	mtx_unlock(&lun->lun_lock);
	return (0);
}

int
ctl_invalidate_lun(struct ctl_be_lun *be_lun)
{
	struct ctl_softc *softc;
	struct ctl_lun *lun;

	lun = (struct ctl_lun *)be_lun->ctl_lun;
	softc = lun->ctl_softc;

	mtx_lock(&lun->lun_lock);

	/*
	 * The LUN needs to be disabled before it can be marked invalid.
	 */
	if ((lun->flags & CTL_LUN_DISABLED) == 0) {
		mtx_unlock(&lun->lun_lock);
		return (-1);
	}
	/*
	 * Mark the LUN invalid.
	 */
	lun->flags |= CTL_LUN_INVALID;

	/*
	 * If there is nothing in the OOA queue, go ahead and free the LUN.
	 * If we have something in the OOA queue, we'll free it when the
	 * last I/O completes.
	 */
	if (TAILQ_EMPTY(&lun->ooa_queue)) {
		mtx_unlock(&lun->lun_lock);
		mtx_lock(&softc->ctl_lock);
		ctl_free_lun(lun);
		mtx_unlock(&softc->ctl_lock);
	} else
		mtx_unlock(&lun->lun_lock);

	return (0);
}

int
ctl_lun_inoperable(struct ctl_be_lun *be_lun)
{
	struct ctl_lun *lun = (struct ctl_lun *)be_lun->ctl_lun;

	mtx_lock(&lun->lun_lock);
	lun->flags |= CTL_LUN_INOPERABLE;
	mtx_unlock(&lun->lun_lock);
	return (0);
}

int
ctl_lun_operable(struct ctl_be_lun *be_lun)
{
	struct ctl_lun *lun = (struct ctl_lun *)be_lun->ctl_lun;

	mtx_lock(&lun->lun_lock);
	lun->flags &= ~CTL_LUN_INOPERABLE;
	mtx_unlock(&lun->lun_lock);
	return (0);
}

void
ctl_lun_capacity_changed(struct ctl_be_lun *be_lun)
{
	struct ctl_lun *lun = (struct ctl_lun *)be_lun->ctl_lun;

	mtx_lock(&lun->lun_lock);
	ctl_est_ua_all(lun, -1, CTL_UA_CAPACITY_CHANGED);
	mtx_unlock(&lun->lun_lock);
}

/*
 * Backend "memory move is complete" callback for requests that never
 * make it down to say RAIDCore's configuration code.
 */
int
ctl_config_move_done(union ctl_io *io)
{
	int retval;

	CTL_DEBUG_PRINT(("ctl_config_move_done\n"));
	KASSERT(io->io_hdr.io_type == CTL_IO_SCSI,
	    ("Config I/O type isn't CTL_IO_SCSI (%d)!", io->io_hdr.io_type));

	if ((io->io_hdr.port_status != 0) &&
	    ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_STATUS_NONE ||
	     (io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS)) {
		/*
		 * For hardware error sense keys, the sense key
		 * specific value is defined to be a retry count,
		 * but we use it to pass back an internal FETD
		 * error code.  XXX KDM  Hopefully the FETD is only
		 * using 16 bits for an error code, since that's
		 * all the space we have in the sks field.
		 */
		ctl_set_internal_failure(&io->scsiio,
					 /*sks_valid*/ 1,
					 /*retry_count*/
					 io->io_hdr.port_status);
	}

	if (((io->io_hdr.flags & CTL_FLAG_DATA_MASK) == CTL_FLAG_DATA_IN) ||
	    ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE &&
	     (io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS) ||
	    ((io->io_hdr.flags & CTL_FLAG_ABORT) != 0)) {
		/*
		 * XXX KDM just assuming a single pointer here, and not a
		 * S/G list.  If we start using S/G lists for config data,
		 * we'll need to know how to clean them up here as well.
		 */
		if (io->io_hdr.flags & CTL_FLAG_ALLOCATED)
			free(io->scsiio.kern_data_ptr, M_CTL);
		ctl_done(io);
		retval = CTL_RETVAL_COMPLETE;
	} else {
		/*
		 * XXX KDM now we need to continue data movement.  Some
		 * options:
		 * - call ctl_scsiio() again?  We don't do this for data
		 *   writes, because for those at least we know ahead of
		 *   time where the write will go and how long it is.  For
		 *   config writes, though, that information is largely
		 *   contained within the write itself, thus we need to
		 *   parse out the data again.
		 *
		 * - Call some other function once the data is in?
		 */
		if (ctl_debug & CTL_DEBUG_CDB_DATA)
			ctl_data_print(io);

		/*
		 * XXX KDM call ctl_scsiio() again for now, and check flag
		 * bits to see whether we're allocated or not.
		 */
		retval = ctl_scsiio(&io->scsiio);
	}
	return (retval);
}

/*
 * This gets called by a backend driver when it is done with a
 * data_submit method.
 */
void
ctl_data_submit_done(union ctl_io *io)
{
	/*
	 * If the IO_CONT flag is set, we need to call the supplied
	 * function to continue processing the I/O, instead of completing
	 * the I/O just yet.
	 *
	 * If there is an error, though, we don't want to keep processing.
	 * Instead, just send status back to the initiator.
	 */
	if ((io->io_hdr.flags & CTL_FLAG_IO_CONT) &&
	    (io->io_hdr.flags & CTL_FLAG_ABORT) == 0 &&
	    ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_STATUS_NONE ||
	     (io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS)) {
		io->scsiio.io_cont(io);
		return;
	}
	ctl_done(io);
}

/*
 * This gets called by a backend driver when it is done with a
 * configuration write.
 */
void
ctl_config_write_done(union ctl_io *io)
{
	uint8_t *buf;

	/*
	 * If the IO_CONT flag is set, we need to call the supplied
	 * function to continue processing the I/O, instead of completing
	 * the I/O just yet.
	 *
	 * If there is an error, though, we don't want to keep processing.
	 * Instead, just send status back to the initiator.
	 */
	if ((io->io_hdr.flags & CTL_FLAG_IO_CONT) &&
	    (io->io_hdr.flags & CTL_FLAG_ABORT) == 0 &&
	    ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_STATUS_NONE ||
	     (io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS)) {
		io->scsiio.io_cont(io);
		return;
	}
	/*
	 * Since a configuration write can be done for commands that actually
	 * have data allocated, like write buffer, and commands that have
	 * no data, like start/stop unit, we need to check here.
	 */
	if (io->io_hdr.flags & CTL_FLAG_ALLOCATED)
		buf = io->scsiio.kern_data_ptr;
	else
		buf = NULL;
	ctl_done(io);
	if (buf)
		free(buf, M_CTL);
}

void
ctl_config_read_done(union ctl_io *io)
{
	uint8_t *buf;

	/*
	 * If there is some error -- we are done, skip data transfer.
	 */
	if ((io->io_hdr.flags & CTL_FLAG_ABORT) != 0 ||
	    ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE &&
	     (io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)) {
		if (io->io_hdr.flags & CTL_FLAG_ALLOCATED)
			buf = io->scsiio.kern_data_ptr;
		else
			buf = NULL;
		ctl_done(io);
		if (buf)
			free(buf, M_CTL);
		return;
	}

	/*
	 * If the IO_CONT flag is set, we need to call the supplied
	 * function to continue processing the I/O, instead of completing
	 * the I/O just yet.
	 */
	if (io->io_hdr.flags & CTL_FLAG_IO_CONT) {
		io->scsiio.io_cont(io);
		return;
	}

	ctl_datamove(io);
}

/*
 * SCSI release command.
 */
int
ctl_scsi_release(struct ctl_scsiio *ctsio)
{
	int length, longid, thirdparty_id, resv_id;
	struct ctl_lun *lun;
	uint32_t residx;

	length = 0;
	resv_id = 0;

	CTL_DEBUG_PRINT(("ctl_scsi_release\n"));

	residx = ctl_get_resindex(&ctsio->io_hdr.nexus);
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	switch (ctsio->cdb[0]) {
	case RELEASE_10: {
		struct scsi_release_10 *cdb;

		cdb = (struct scsi_release_10 *)ctsio->cdb;

		if (cdb->byte2 & SR10_LONGID)
			longid = 1;
		else
			thirdparty_id = cdb->thirdparty_id;

		resv_id = cdb->resv_id;
		length = scsi_2btoul(cdb->length);
		break;
	}
	}


	/*
	 * XXX KDM right now, we only support LUN reservation.  We don't
	 * support 3rd party reservations, or extent reservations, which
	 * might actually need the parameter list.  If we've gotten this
	 * far, we've got a LUN reservation.  Anything else got kicked out
	 * above.  So, according to SPC, ignore the length.
	 */
	length = 0;

	if (((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0)
	 && (length > 0)) {
		ctsio->kern_data_ptr = malloc(length, M_CTL, M_WAITOK);
		ctsio->kern_data_len = length;
		ctsio->kern_total_len = length;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	if (length > 0)
		thirdparty_id = scsi_8btou64(ctsio->kern_data_ptr);

	mtx_lock(&lun->lun_lock);

	/*
	 * According to SPC, it is not an error for an intiator to attempt
	 * to release a reservation on a LUN that isn't reserved, or that
	 * is reserved by another initiator.  The reservation can only be
	 * released, though, by the initiator who made it or by one of
	 * several reset type events.
	 */
	if ((lun->flags & CTL_LUN_RESERVED) && (lun->res_idx == residx))
			lun->flags &= ~CTL_LUN_RESERVED;

	mtx_unlock(&lun->lun_lock);

	if (ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) {
		free(ctsio->kern_data_ptr, M_CTL);
		ctsio->io_hdr.flags &= ~CTL_FLAG_ALLOCATED;
	}

	ctl_set_success(ctsio);
	ctl_done((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_scsi_reserve(struct ctl_scsiio *ctsio)
{
	int extent, thirdparty, longid;
	int resv_id, length;
	uint64_t thirdparty_id;
	struct ctl_lun *lun;
	uint32_t residx;

	extent = 0;
	thirdparty = 0;
	longid = 0;
	resv_id = 0;
	length = 0;
	thirdparty_id = 0;

	CTL_DEBUG_PRINT(("ctl_reserve\n"));

	residx = ctl_get_resindex(&ctsio->io_hdr.nexus);
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	switch (ctsio->cdb[0]) {
	case RESERVE_10: {
		struct scsi_reserve_10 *cdb;

		cdb = (struct scsi_reserve_10 *)ctsio->cdb;

		if (cdb->byte2 & SR10_LONGID)
			longid = 1;
		else
			thirdparty_id = cdb->thirdparty_id;

		resv_id = cdb->resv_id;
		length = scsi_2btoul(cdb->length);
		break;
	}
	}

	/*
	 * XXX KDM right now, we only support LUN reservation.  We don't
	 * support 3rd party reservations, or extent reservations, which
	 * might actually need the parameter list.  If we've gotten this
	 * far, we've got a LUN reservation.  Anything else got kicked out
	 * above.  So, according to SPC, ignore the length.
	 */
	length = 0;

	if (((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0)
	 && (length > 0)) {
		ctsio->kern_data_ptr = malloc(length, M_CTL, M_WAITOK);
		ctsio->kern_data_len = length;
		ctsio->kern_total_len = length;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	if (length > 0)
		thirdparty_id = scsi_8btou64(ctsio->kern_data_ptr);

	mtx_lock(&lun->lun_lock);
	if ((lun->flags & CTL_LUN_RESERVED) && (lun->res_idx != residx)) {
		ctl_set_reservation_conflict(ctsio);
		goto bailout;
	}

	lun->flags |= CTL_LUN_RESERVED;
	lun->res_idx = residx;

	ctl_set_success(ctsio);

bailout:
	mtx_unlock(&lun->lun_lock);

	if (ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) {
		free(ctsio->kern_data_ptr, M_CTL);
		ctsio->io_hdr.flags &= ~CTL_FLAG_ALLOCATED;
	}

	ctl_done((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_start_stop(struct ctl_scsiio *ctsio)
{
	struct scsi_start_stop_unit *cdb;
	struct ctl_lun *lun;
	int retval;

	CTL_DEBUG_PRINT(("ctl_start_stop\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	retval = 0;

	cdb = (struct scsi_start_stop_unit *)ctsio->cdb;

	/*
	 * XXX KDM
	 * We don't support the immediate bit on a stop unit.  In order to
	 * do that, we would need to code up a way to know that a stop is
	 * pending, and hold off any new commands until it completes, one
	 * way or another.  Then we could accept or reject those commands
	 * depending on its status.  We would almost need to do the reverse
	 * of what we do below for an immediate start -- return the copy of
	 * the ctl_io to the FETD with status to send to the host (and to
	 * free the copy!) and then free the original I/O once the stop
	 * actually completes.  That way, the OOA queue mechanism can work
	 * to block commands that shouldn't proceed.  Another alternative
	 * would be to put the copy in the queue in place of the original,
	 * and return the original back to the caller.  That could be
	 * slightly safer..
	 */
	if ((cdb->byte2 & SSS_IMMED)
	 && ((cdb->how & SSS_START) == 0)) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 1,
				      /*bit_valid*/ 1,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	if ((lun->flags & CTL_LUN_PR_RESERVED)
	 && ((cdb->how & SSS_START)==0)) {
		uint32_t residx;

		residx = ctl_get_resindex(&ctsio->io_hdr.nexus);
		if (ctl_get_prkey(lun, residx) == 0
		 || (lun->pr_res_idx!=residx && lun->res_type < 4)) {

			ctl_set_reservation_conflict(ctsio);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
	}

	/*
	 * If there is no backend on this device, we can't start or stop
	 * it.  In theory we shouldn't get any start/stop commands in the
	 * first place at this level if the LUN doesn't have a backend.
	 * That should get stopped by the command decode code.
	 */
	if (lun->backend == NULL) {
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * XXX KDM Copan-specific offline behavior.
	 * Figure out a reasonable way to port this?
	 */
#ifdef NEEDTOPORT
	mtx_lock(&lun->lun_lock);

	if (((cdb->byte2 & SSS_ONOFFLINE) == 0)
	 && (lun->flags & CTL_LUN_OFFLINE)) {
		/*
		 * If the LUN is offline, and the on/offline bit isn't set,
		 * reject the start or stop.  Otherwise, let it through.
		 */
		mtx_unlock(&lun->lun_lock);
		ctl_set_lun_not_ready(ctsio);
		ctl_done((union ctl_io *)ctsio);
	} else {
		mtx_unlock(&lun->lun_lock);
#endif /* NEEDTOPORT */
		/*
		 * This could be a start or a stop when we're online,
		 * or a stop/offline or start/online.  A start or stop when
		 * we're offline is covered in the case above.
		 */
		/*
		 * In the non-immediate case, we send the request to
		 * the backend and return status to the user when
		 * it is done.
		 *
		 * In the immediate case, we allocate a new ctl_io
		 * to hold a copy of the request, and send that to
		 * the backend.  We then set good status on the
		 * user's request and return it immediately.
		 */
		if (cdb->byte2 & SSS_IMMED) {
			union ctl_io *new_io;

			new_io = ctl_alloc_io(ctsio->io_hdr.pool);
			ctl_copy_io((union ctl_io *)ctsio, new_io);
			retval = lun->backend->config_write(new_io);
			ctl_set_success(ctsio);
			ctl_done((union ctl_io *)ctsio);
		} else {
			retval = lun->backend->config_write(
				(union ctl_io *)ctsio);
		}
#ifdef NEEDTOPORT
	}
#endif
	return (retval);
}

/*
 * We support the SYNCHRONIZE CACHE command (10 and 16 byte versions), but
 * we don't really do anything with the LBA and length fields if the user
 * passes them in.  Instead we'll just flush out the cache for the entire
 * LUN.
 */
int
ctl_sync_cache(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct ctl_softc *softc;
	uint64_t starting_lba;
	uint32_t block_count;
	int retval;

	CTL_DEBUG_PRINT(("ctl_sync_cache\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	softc = lun->ctl_softc;
	retval = 0;

	switch (ctsio->cdb[0]) {
	case SYNCHRONIZE_CACHE: {
		struct scsi_sync_cache *cdb;
		cdb = (struct scsi_sync_cache *)ctsio->cdb;

		starting_lba = scsi_4btoul(cdb->begin_lba);
		block_count = scsi_2btoul(cdb->lb_count);
		break;
	}
	case SYNCHRONIZE_CACHE_16: {
		struct scsi_sync_cache_16 *cdb;
		cdb = (struct scsi_sync_cache_16 *)ctsio->cdb;

		starting_lba = scsi_8btou64(cdb->begin_lba);
		block_count = scsi_4btoul(cdb->lb_count);
		break;
	}
	default:
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);
		goto bailout;
		break; /* NOTREACHED */
	}

	/*
	 * We check the LBA and length, but don't do anything with them.
	 * A SYNCHRONIZE CACHE will cause the entire cache for this lun to
	 * get flushed.  This check will just help satisfy anyone who wants
	 * to see an error for an out of range LBA.
	 */
	if ((starting_lba + block_count) > (lun->be_lun->maxlba + 1)) {
		ctl_set_lba_out_of_range(ctsio);
		ctl_done((union ctl_io *)ctsio);
		goto bailout;
	}

	/*
	 * If this LUN has no backend, we can't flush the cache anyway.
	 */
	if (lun->backend == NULL) {
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);
		goto bailout;
	}

	/*
	 * Check to see whether we're configured to send the SYNCHRONIZE
	 * CACHE command directly to the back end.
	 */
	mtx_lock(&lun->lun_lock);
	if ((softc->flags & CTL_FLAG_REAL_SYNC)
	 && (++(lun->sync_count) >= lun->sync_interval)) {
		lun->sync_count = 0;
		mtx_unlock(&lun->lun_lock);
		retval = lun->backend->config_write((union ctl_io *)ctsio);
	} else {
		mtx_unlock(&lun->lun_lock);
		ctl_set_success(ctsio);
		ctl_done((union ctl_io *)ctsio);
	}

bailout:

	return (retval);
}

int
ctl_format(struct ctl_scsiio *ctsio)
{
	struct scsi_format *cdb;
	struct ctl_lun *lun;
	int length, defect_list_len;

	CTL_DEBUG_PRINT(("ctl_format\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	cdb = (struct scsi_format *)ctsio->cdb;

	length = 0;
	if (cdb->byte2 & SF_FMTDATA) {
		if (cdb->byte2 & SF_LONGLIST)
			length = sizeof(struct scsi_format_header_long);
		else
			length = sizeof(struct scsi_format_header_short);
	}

	if (((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0)
	 && (length > 0)) {
		ctsio->kern_data_ptr = malloc(length, M_CTL, M_WAITOK);
		ctsio->kern_data_len = length;
		ctsio->kern_total_len = length;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	defect_list_len = 0;

	if (cdb->byte2 & SF_FMTDATA) {
		if (cdb->byte2 & SF_LONGLIST) {
			struct scsi_format_header_long *header;

			header = (struct scsi_format_header_long *)
				ctsio->kern_data_ptr;

			defect_list_len = scsi_4btoul(header->defect_list_len);
			if (defect_list_len != 0) {
				ctl_set_invalid_field(ctsio,
						      /*sks_valid*/ 1,
						      /*command*/ 0,
						      /*field*/ 2,
						      /*bit_valid*/ 0,
						      /*bit*/ 0);
				goto bailout;
			}
		} else {
			struct scsi_format_header_short *header;

			header = (struct scsi_format_header_short *)
				ctsio->kern_data_ptr;

			defect_list_len = scsi_2btoul(header->defect_list_len);
			if (defect_list_len != 0) {
				ctl_set_invalid_field(ctsio,
						      /*sks_valid*/ 1,
						      /*command*/ 0,
						      /*field*/ 2,
						      /*bit_valid*/ 0,
						      /*bit*/ 0);
				goto bailout;
			}
		}
	}

	/*
	 * The format command will clear out the "Medium format corrupted"
	 * status if set by the configuration code.  That status is really
	 * just a way to notify the host that we have lost the media, and
	 * get them to issue a command that will basically make them think
	 * they're blowing away the media.
	 */
	mtx_lock(&lun->lun_lock);
	lun->flags &= ~CTL_LUN_INOPERABLE;
	mtx_unlock(&lun->lun_lock);

	ctl_set_success(ctsio);
bailout:

	if (ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) {
		free(ctsio->kern_data_ptr, M_CTL);
		ctsio->io_hdr.flags &= ~CTL_FLAG_ALLOCATED;
	}

	ctl_done((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_read_buffer(struct ctl_scsiio *ctsio)
{
	struct scsi_read_buffer *cdb;
	struct ctl_lun *lun;
	int buffer_offset, len;
	static uint8_t descr[4];
	static uint8_t echo_descr[4] = { 0 };

	CTL_DEBUG_PRINT(("ctl_read_buffer\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_read_buffer *)ctsio->cdb;

	if ((cdb->byte2 & RWB_MODE) != RWB_MODE_DATA &&
	    (cdb->byte2 & RWB_MODE) != RWB_MODE_ECHO_DESCR &&
	    (cdb->byte2 & RWB_MODE) != RWB_MODE_DESCR) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 1,
				      /*bit_valid*/ 1,
				      /*bit*/ 4);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	len = scsi_3btoul(cdb->length);
	buffer_offset = scsi_3btoul(cdb->offset);

	if (buffer_offset + len > CTL_WRITE_BUFFER_SIZE) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 6,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	if ((cdb->byte2 & RWB_MODE) == RWB_MODE_DESCR) {
		descr[0] = 0;
		scsi_ulto3b(CTL_WRITE_BUFFER_SIZE, &descr[1]);
		ctsio->kern_data_ptr = descr;
		len = min(len, sizeof(descr));
	} else if ((cdb->byte2 & RWB_MODE) == RWB_MODE_ECHO_DESCR) {
		ctsio->kern_data_ptr = echo_descr;
		len = min(len, sizeof(echo_descr));
	} else {
		if (lun->write_buffer == NULL) {
			lun->write_buffer = malloc(CTL_WRITE_BUFFER_SIZE,
			    M_CTL, M_WAITOK);
		}
		ctsio->kern_data_ptr = lun->write_buffer + buffer_offset;
	}
	ctsio->kern_data_len = len;
	ctsio->kern_total_len = len;
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;
	ctl_set_success(ctsio);
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_write_buffer(struct ctl_scsiio *ctsio)
{
	struct scsi_write_buffer *cdb;
	struct ctl_lun *lun;
	int buffer_offset, len;

	CTL_DEBUG_PRINT(("ctl_write_buffer\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_write_buffer *)ctsio->cdb;

	if ((cdb->byte2 & RWB_MODE) != RWB_MODE_DATA) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 1,
				      /*bit_valid*/ 1,
				      /*bit*/ 4);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	len = scsi_3btoul(cdb->length);
	buffer_offset = scsi_3btoul(cdb->offset);

	if (buffer_offset + len > CTL_WRITE_BUFFER_SIZE) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 6,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * If we've got a kernel request that hasn't been malloced yet,
	 * malloc it and tell the caller the data buffer is here.
	 */
	if ((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0) {
		if (lun->write_buffer == NULL) {
			lun->write_buffer = malloc(CTL_WRITE_BUFFER_SIZE,
			    M_CTL, M_WAITOK);
		}
		ctsio->kern_data_ptr = lun->write_buffer + buffer_offset;
		ctsio->kern_data_len = len;
		ctsio->kern_total_len = len;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	ctl_set_success(ctsio);
	ctl_done((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_write_same(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct ctl_lba_len_flags *lbalen;
	uint64_t lba;
	uint32_t num_blocks;
	int len, retval;
	uint8_t byte2;

	retval = CTL_RETVAL_COMPLETE;

	CTL_DEBUG_PRINT(("ctl_write_same\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	switch (ctsio->cdb[0]) {
	case WRITE_SAME_10: {
		struct scsi_write_same_10 *cdb;

		cdb = (struct scsi_write_same_10 *)ctsio->cdb;

		lba = scsi_4btoul(cdb->addr);
		num_blocks = scsi_2btoul(cdb->length);
		byte2 = cdb->byte2;
		break;
	}
	case WRITE_SAME_16: {
		struct scsi_write_same_16 *cdb;

		cdb = (struct scsi_write_same_16 *)ctsio->cdb;

		lba = scsi_8btou64(cdb->addr);
		num_blocks = scsi_4btoul(cdb->length);
		byte2 = cdb->byte2;
		break;
	}
	default:
		/*
		 * We got a command we don't support.  This shouldn't
		 * happen, commands should be filtered out above us.
		 */
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
		break; /* NOTREACHED */
	}

	/* NDOB and ANCHOR flags can be used only together with UNMAP */
	if ((byte2 & SWS_UNMAP) == 0 &&
	    (byte2 & (SWS_NDOB | SWS_ANCHOR)) != 0) {
		ctl_set_invalid_field(ctsio, /*sks_valid*/ 1,
		    /*command*/ 1, /*field*/ 1, /*bit_valid*/ 1, /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * The first check is to make sure we're in bounds, the second
	 * check is to catch wrap-around problems.  If the lba + num blocks
	 * is less than the lba, then we've wrapped around and the block
	 * range is invalid anyway.
	 */
	if (((lba + num_blocks) > (lun->be_lun->maxlba + 1))
	 || ((lba + num_blocks) < lba)) {
		ctl_set_lba_out_of_range(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/* Zero number of blocks means "to the last logical block" */
	if (num_blocks == 0) {
		if ((lun->be_lun->maxlba + 1) - lba > UINT32_MAX) {
			ctl_set_invalid_field(ctsio,
					      /*sks_valid*/ 0,
					      /*command*/ 1,
					      /*field*/ 0,
					      /*bit_valid*/ 0,
					      /*bit*/ 0);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
		num_blocks = (lun->be_lun->maxlba + 1) - lba;
	}

	len = lun->be_lun->blocksize;

	/*
	 * If we've got a kernel request that hasn't been malloced yet,
	 * malloc it and tell the caller the data buffer is here.
	 */
	if ((byte2 & SWS_NDOB) == 0 &&
	    (ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0) {
		ctsio->kern_data_ptr = malloc(len, M_CTL, M_WAITOK);;
		ctsio->kern_data_len = len;
		ctsio->kern_total_len = len;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	lbalen = (struct ctl_lba_len_flags *)&ctsio->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	lbalen->lba = lba;
	lbalen->len = num_blocks;
	lbalen->flags = byte2;
	retval = lun->backend->config_write((union ctl_io *)ctsio);

	return (retval);
}

int
ctl_unmap(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct scsi_unmap *cdb;
	struct ctl_ptr_len_flags *ptrlen;
	struct scsi_unmap_header *hdr;
	struct scsi_unmap_desc *buf, *end, *endnz, *range;
	uint64_t lba;
	uint32_t num_blocks;
	int len, retval;
	uint8_t byte2;

	retval = CTL_RETVAL_COMPLETE;

	CTL_DEBUG_PRINT(("ctl_unmap\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_unmap *)ctsio->cdb;

	len = scsi_2btoul(cdb->length);
	byte2 = cdb->byte2;

	/*
	 * If we've got a kernel request that hasn't been malloced yet,
	 * malloc it and tell the caller the data buffer is here.
	 */
	if ((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0) {
		ctsio->kern_data_ptr = malloc(len, M_CTL, M_WAITOK);;
		ctsio->kern_data_len = len;
		ctsio->kern_total_len = len;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	len = ctsio->kern_total_len - ctsio->kern_data_resid;
	hdr = (struct scsi_unmap_header *)ctsio->kern_data_ptr;
	if (len < sizeof (*hdr) ||
	    len < (scsi_2btoul(hdr->length) + sizeof(hdr->length)) ||
	    len < (scsi_2btoul(hdr->desc_length) + sizeof (*hdr)) ||
	    scsi_2btoul(hdr->desc_length) % sizeof(*buf) != 0) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 0,
				      /*command*/ 0,
				      /*field*/ 0,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		goto done;
	}
	len = scsi_2btoul(hdr->desc_length);
	buf = (struct scsi_unmap_desc *)(hdr + 1);
	end = buf + len / sizeof(*buf);

	endnz = buf;
	for (range = buf; range < end; range++) {
		lba = scsi_8btou64(range->lba);
		num_blocks = scsi_4btoul(range->length);
		if (((lba + num_blocks) > (lun->be_lun->maxlba + 1))
		 || ((lba + num_blocks) < lba)) {
			ctl_set_lba_out_of_range(ctsio);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
		if (num_blocks != 0)
			endnz = range + 1;
	}

	/*
	 * Block backend can not handle zero last range.
	 * Filter it out and return if there is nothing left.
	 */
	len = (uint8_t *)endnz - (uint8_t *)buf;
	if (len == 0) {
		ctl_set_success(ctsio);
		goto done;
	}

	mtx_lock(&lun->lun_lock);
	ptrlen = (struct ctl_ptr_len_flags *)
	    &ctsio->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	ptrlen->ptr = (void *)buf;
	ptrlen->len = len;
	ptrlen->flags = byte2;
	ctl_check_blocked(lun);
	mtx_unlock(&lun->lun_lock);

	retval = lun->backend->config_write((union ctl_io *)ctsio);
	return (retval);

done:
	if (ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) {
		free(ctsio->kern_data_ptr, M_CTL);
		ctsio->io_hdr.flags &= ~CTL_FLAG_ALLOCATED;
	}
	ctl_done((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

/*
 * Note that this function currently doesn't actually do anything inside
 * CTL to enforce things if the DQue bit is turned on.
 *
 * Also note that this function can't be used in the default case, because
 * the DQue bit isn't set in the changeable mask for the control mode page
 * anyway.  This is just here as an example for how to implement a page
 * handler, and a placeholder in case we want to allow the user to turn
 * tagged queueing on and off.
 *
 * The D_SENSE bit handling is functional, however, and will turn
 * descriptor sense on and off for a given LUN.
 */
int
ctl_control_page_handler(struct ctl_scsiio *ctsio,
			 struct ctl_page_index *page_index, uint8_t *page_ptr)
{
	struct scsi_control_page *current_cp, *saved_cp, *user_cp;
	struct ctl_lun *lun;
	int set_ua;
	uint32_t initidx;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	initidx = ctl_get_initindex(&ctsio->io_hdr.nexus);
	set_ua = 0;

	user_cp = (struct scsi_control_page *)page_ptr;
	current_cp = (struct scsi_control_page *)
		(page_index->page_data + (page_index->page_len *
		CTL_PAGE_CURRENT));
	saved_cp = (struct scsi_control_page *)
		(page_index->page_data + (page_index->page_len *
		CTL_PAGE_SAVED));

	mtx_lock(&lun->lun_lock);
	if (((current_cp->rlec & SCP_DSENSE) == 0)
	 && ((user_cp->rlec & SCP_DSENSE) != 0)) {
		/*
		 * Descriptor sense is currently turned off and the user
		 * wants to turn it on.
		 */
		current_cp->rlec |= SCP_DSENSE;
		saved_cp->rlec |= SCP_DSENSE;
		lun->flags |= CTL_LUN_SENSE_DESC;
		set_ua = 1;
	} else if (((current_cp->rlec & SCP_DSENSE) != 0)
		&& ((user_cp->rlec & SCP_DSENSE) == 0)) {
		/*
		 * Descriptor sense is currently turned on, and the user
		 * wants to turn it off.
		 */
		current_cp->rlec &= ~SCP_DSENSE;
		saved_cp->rlec &= ~SCP_DSENSE;
		lun->flags &= ~CTL_LUN_SENSE_DESC;
		set_ua = 1;
	}
	if ((current_cp->queue_flags & SCP_QUEUE_ALG_MASK) !=
	    (user_cp->queue_flags & SCP_QUEUE_ALG_MASK)) {
		current_cp->queue_flags &= ~SCP_QUEUE_ALG_MASK;
		current_cp->queue_flags |= user_cp->queue_flags & SCP_QUEUE_ALG_MASK;
		saved_cp->queue_flags &= ~SCP_QUEUE_ALG_MASK;
		saved_cp->queue_flags |= user_cp->queue_flags & SCP_QUEUE_ALG_MASK;
		set_ua = 1;
	}
	if ((current_cp->eca_and_aen & SCP_SWP) !=
	    (user_cp->eca_and_aen & SCP_SWP)) {
		current_cp->eca_and_aen &= ~SCP_SWP;
		current_cp->eca_and_aen |= user_cp->eca_and_aen & SCP_SWP;
		saved_cp->eca_and_aen &= ~SCP_SWP;
		saved_cp->eca_and_aen |= user_cp->eca_and_aen & SCP_SWP;
		set_ua = 1;
	}
	if (set_ua != 0)
		ctl_est_ua_all(lun, initidx, CTL_UA_MODE_CHANGE);
	mtx_unlock(&lun->lun_lock);

	return (0);
}

int
ctl_caching_sp_handler(struct ctl_scsiio *ctsio,
		     struct ctl_page_index *page_index, uint8_t *page_ptr)
{
	struct scsi_caching_page *current_cp, *saved_cp, *user_cp;
	struct ctl_lun *lun;
	int set_ua;
	uint32_t initidx;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	initidx = ctl_get_initindex(&ctsio->io_hdr.nexus);
	set_ua = 0;

	user_cp = (struct scsi_caching_page *)page_ptr;
	current_cp = (struct scsi_caching_page *)
		(page_index->page_data + (page_index->page_len *
		CTL_PAGE_CURRENT));
	saved_cp = (struct scsi_caching_page *)
		(page_index->page_data + (page_index->page_len *
		CTL_PAGE_SAVED));

	mtx_lock(&lun->lun_lock);
	if ((current_cp->flags1 & (SCP_WCE | SCP_RCD)) !=
	    (user_cp->flags1 & (SCP_WCE | SCP_RCD))) {
		current_cp->flags1 &= ~(SCP_WCE | SCP_RCD);
		current_cp->flags1 |= user_cp->flags1 & (SCP_WCE | SCP_RCD);
		saved_cp->flags1 &= ~(SCP_WCE | SCP_RCD);
		saved_cp->flags1 |= user_cp->flags1 & (SCP_WCE | SCP_RCD);
		set_ua = 1;
	}
	if (set_ua != 0)
		ctl_est_ua_all(lun, initidx, CTL_UA_MODE_CHANGE);
	mtx_unlock(&lun->lun_lock);

	return (0);
}

int
ctl_debugconf_sp_select_handler(struct ctl_scsiio *ctsio,
				struct ctl_page_index *page_index,
				uint8_t *page_ptr)
{
	uint8_t *c;
	int i;

	c = ((struct copan_debugconf_subpage *)page_ptr)->ctl_time_io_secs;
	ctl_time_io_secs =
		(c[0] << 8) |
		(c[1] << 0) |
		0;
	CTL_DEBUG_PRINT(("set ctl_time_io_secs to %d\n", ctl_time_io_secs));
	printf("set ctl_time_io_secs to %d\n", ctl_time_io_secs);
	printf("page data:");
	for (i=0; i<8; i++)
		printf(" %.2x",page_ptr[i]);
	printf("\n");
	return (0);
}

int
ctl_debugconf_sp_sense_handler(struct ctl_scsiio *ctsio,
			       struct ctl_page_index *page_index,
			       int pc)
{
	struct copan_debugconf_subpage *page;

	page = (struct copan_debugconf_subpage *)page_index->page_data +
		(page_index->page_len * pc);

	switch (pc) {
	case SMS_PAGE_CTRL_CHANGEABLE >> 6:
	case SMS_PAGE_CTRL_DEFAULT >> 6:
	case SMS_PAGE_CTRL_SAVED >> 6:
		/*
		 * We don't update the changable or default bits for this page.
		 */
		break;
	case SMS_PAGE_CTRL_CURRENT >> 6:
		page->ctl_time_io_secs[0] = ctl_time_io_secs >> 8;
		page->ctl_time_io_secs[1] = ctl_time_io_secs >> 0;
		break;
	default:
#ifdef NEEDTOPORT
		EPRINT(0, "Invalid PC %d!!", pc);
#endif /* NEEDTOPORT */
		break;
	}
	return (0);
}


static int
ctl_do_mode_select(union ctl_io *io)
{
	struct scsi_mode_page_header *page_header;
	struct ctl_page_index *page_index;
	struct ctl_scsiio *ctsio;
	int control_dev, page_len;
	int page_len_offset, page_len_size;
	union ctl_modepage_info *modepage_info;
	struct ctl_lun *lun;
	int *len_left, *len_used;
	int retval, i;

	ctsio = &io->scsiio;
	page_index = NULL;
	page_len = 0;
	retval = CTL_RETVAL_COMPLETE;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	if (lun->be_lun->lun_type != T_DIRECT)
		control_dev = 1;
	else
		control_dev = 0;

	modepage_info = (union ctl_modepage_info *)
		ctsio->io_hdr.ctl_private[CTL_PRIV_MODEPAGE].bytes;
	len_left = &modepage_info->header.len_left;
	len_used = &modepage_info->header.len_used;

do_next_page:

	page_header = (struct scsi_mode_page_header *)
		(ctsio->kern_data_ptr + *len_used);

	if (*len_left == 0) {
		free(ctsio->kern_data_ptr, M_CTL);
		ctl_set_success(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	} else if (*len_left < sizeof(struct scsi_mode_page_header)) {

		free(ctsio->kern_data_ptr, M_CTL);
		ctl_set_param_len_error(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);

	} else if ((page_header->page_code & SMPH_SPF)
		&& (*len_left < sizeof(struct scsi_mode_page_header_sp))) {

		free(ctsio->kern_data_ptr, M_CTL);
		ctl_set_param_len_error(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}


	/*
	 * XXX KDM should we do something with the block descriptor?
	 */
	for (i = 0; i < CTL_NUM_MODE_PAGES; i++) {

		if ((control_dev != 0)
		 && (lun->mode_pages.index[i].page_flags &
		     CTL_PAGE_FLAG_DISK_ONLY))
			continue;

		if ((lun->mode_pages.index[i].page_code & SMPH_PC_MASK) !=
		    (page_header->page_code & SMPH_PC_MASK))
			continue;

		/*
		 * If neither page has a subpage code, then we've got a
		 * match.
		 */
		if (((lun->mode_pages.index[i].page_code & SMPH_SPF) == 0)
		 && ((page_header->page_code & SMPH_SPF) == 0)) {
			page_index = &lun->mode_pages.index[i];
			page_len = page_header->page_length;
			break;
		}

		/*
		 * If both pages have subpages, then the subpage numbers
		 * have to match.
		 */
		if ((lun->mode_pages.index[i].page_code & SMPH_SPF)
		  && (page_header->page_code & SMPH_SPF)) {
			struct scsi_mode_page_header_sp *sph;

			sph = (struct scsi_mode_page_header_sp *)page_header;

			if (lun->mode_pages.index[i].subpage ==
			    sph->subpage) {
				page_index = &lun->mode_pages.index[i];
				page_len = scsi_2btoul(sph->page_length);
				break;
			}
		}
	}

	/*
	 * If we couldn't find the page, or if we don't have a mode select
	 * handler for it, send back an error to the user.
	 */
	if ((page_index == NULL)
	 || (page_index->select_handler == NULL)) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 0,
				      /*field*/ *len_used,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		free(ctsio->kern_data_ptr, M_CTL);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	if (page_index->page_code & SMPH_SPF) {
		page_len_offset = 2;
		page_len_size = 2;
	} else {
		page_len_size = 1;
		page_len_offset = 1;
	}

	/*
	 * If the length the initiator gives us isn't the one we specify in
	 * the mode page header, or if they didn't specify enough data in
	 * the CDB to avoid truncating this page, kick out the request.
	 */
	if ((page_len != (page_index->page_len - page_len_offset -
			  page_len_size))
	 || (*len_left < page_index->page_len)) {


		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 0,
				      /*field*/ *len_used + page_len_offset,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		free(ctsio->kern_data_ptr, M_CTL);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * Run through the mode page, checking to make sure that the bits
	 * the user changed are actually legal for him to change.
	 */
	for (i = 0; i < page_index->page_len; i++) {
		uint8_t *user_byte, *change_mask, *current_byte;
		int bad_bit;
		int j;

		user_byte = (uint8_t *)page_header + i;
		change_mask = page_index->page_data +
			      (page_index->page_len * CTL_PAGE_CHANGEABLE) + i;
		current_byte = page_index->page_data +
			       (page_index->page_len * CTL_PAGE_CURRENT) + i;

		/*
		 * Check to see whether the user set any bits in this byte
		 * that he is not allowed to set.
		 */
		if ((*user_byte & ~(*change_mask)) ==
		    (*current_byte & ~(*change_mask)))
			continue;

		/*
		 * Go through bit by bit to determine which one is illegal.
		 */
		bad_bit = 0;
		for (j = 7; j >= 0; j--) {
			if ((((1 << i) & ~(*change_mask)) & *user_byte) !=
			    (((1 << i) & ~(*change_mask)) & *current_byte)) {
				bad_bit = i;
				break;
			}
		}
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 0,
				      /*field*/ *len_used + i,
				      /*bit_valid*/ 1,
				      /*bit*/ bad_bit);
		free(ctsio->kern_data_ptr, M_CTL);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * Decrement these before we call the page handler, since we may
	 * end up getting called back one way or another before the handler
	 * returns to this context.
	 */
	*len_left -= page_index->page_len;
	*len_used += page_index->page_len;

	retval = page_index->select_handler(ctsio, page_index,
					    (uint8_t *)page_header);

	/*
	 * If the page handler returns CTL_RETVAL_QUEUED, then we need to
	 * wait until this queued command completes to finish processing
	 * the mode page.  If it returns anything other than
	 * CTL_RETVAL_COMPLETE (e.g. CTL_RETVAL_ERROR), then it should have
	 * already set the sense information, freed the data pointer, and
	 * completed the io for us.
	 */
	if (retval != CTL_RETVAL_COMPLETE)
		goto bailout_no_done;

	/*
	 * If the initiator sent us more than one page, parse the next one.
	 */
	if (*len_left > 0)
		goto do_next_page;

	ctl_set_success(ctsio);
	free(ctsio->kern_data_ptr, M_CTL);
	ctl_done((union ctl_io *)ctsio);

bailout_no_done:

	return (CTL_RETVAL_COMPLETE);

}

int
ctl_mode_select(struct ctl_scsiio *ctsio)
{
	int param_len, pf, sp;
	int header_size, bd_len;
	int len_left, len_used;
	struct ctl_page_index *page_index;
	struct ctl_lun *lun;
	int control_dev, page_len;
	union ctl_modepage_info *modepage_info;
	int retval;

	pf = 0;
	sp = 0;
	page_len = 0;
	len_used = 0;
	len_left = 0;
	retval = 0;
	bd_len = 0;
	page_index = NULL;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	if (lun->be_lun->lun_type != T_DIRECT)
		control_dev = 1;
	else
		control_dev = 0;

	switch (ctsio->cdb[0]) {
	case MODE_SELECT_6: {
		struct scsi_mode_select_6 *cdb;

		cdb = (struct scsi_mode_select_6 *)ctsio->cdb;

		pf = (cdb->byte2 & SMS_PF) ? 1 : 0;
		sp = (cdb->byte2 & SMS_SP) ? 1 : 0;

		param_len = cdb->length;
		header_size = sizeof(struct scsi_mode_header_6);
		break;
	}
	case MODE_SELECT_10: {
		struct scsi_mode_select_10 *cdb;

		cdb = (struct scsi_mode_select_10 *)ctsio->cdb;

		pf = (cdb->byte2 & SMS_PF) ? 1 : 0;
		sp = (cdb->byte2 & SMS_SP) ? 1 : 0;

		param_len = scsi_2btoul(cdb->length);
		header_size = sizeof(struct scsi_mode_header_10);
		break;
	}
	default:
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
		break; /* NOTREACHED */
	}

	/*
	 * From SPC-3:
	 * "A parameter list length of zero indicates that the Data-Out Buffer
	 * shall be empty. This condition shall not be considered as an error."
	 */
	if (param_len == 0) {
		ctl_set_success(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * Since we'll hit this the first time through, prior to
	 * allocation, we don't need to free a data buffer here.
	 */
	if (param_len < header_size) {
		ctl_set_param_len_error(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * Allocate the data buffer and grab the user's data.  In theory,
	 * we shouldn't have to sanity check the parameter list length here
	 * because the maximum size is 64K.  We should be able to malloc
	 * that much without too many problems.
	 */
	if ((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0) {
		ctsio->kern_data_ptr = malloc(param_len, M_CTL, M_WAITOK);
		ctsio->kern_data_len = param_len;
		ctsio->kern_total_len = param_len;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	switch (ctsio->cdb[0]) {
	case MODE_SELECT_6: {
		struct scsi_mode_header_6 *mh6;

		mh6 = (struct scsi_mode_header_6 *)ctsio->kern_data_ptr;
		bd_len = mh6->blk_desc_len;
		break;
	}
	case MODE_SELECT_10: {
		struct scsi_mode_header_10 *mh10;

		mh10 = (struct scsi_mode_header_10 *)ctsio->kern_data_ptr;
		bd_len = scsi_2btoul(mh10->blk_desc_len);
		break;
	}
	default:
		panic("Invalid CDB type %#x", ctsio->cdb[0]);
		break;
	}

	if (param_len < (header_size + bd_len)) {
		free(ctsio->kern_data_ptr, M_CTL);
		ctl_set_param_len_error(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * Set the IO_CONT flag, so that if this I/O gets passed to
	 * ctl_config_write_done(), it'll get passed back to
	 * ctl_do_mode_select() for further processing, or completion if
	 * we're all done.
	 */
	ctsio->io_hdr.flags |= CTL_FLAG_IO_CONT;
	ctsio->io_cont = ctl_do_mode_select;

	modepage_info = (union ctl_modepage_info *)
		ctsio->io_hdr.ctl_private[CTL_PRIV_MODEPAGE].bytes;

	memset(modepage_info, 0, sizeof(*modepage_info));

	len_left = param_len - header_size - bd_len;
	len_used = header_size + bd_len;

	modepage_info->header.len_left = len_left;
	modepage_info->header.len_used = len_used;

	return (ctl_do_mode_select((union ctl_io *)ctsio));
}

int
ctl_mode_sense(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	int pc, page_code, dbd, llba, subpage;
	int alloc_len, page_len, header_len, total_len;
	struct scsi_mode_block_descr *block_desc;
	struct ctl_page_index *page_index;
	int control_dev;

	dbd = 0;
	llba = 0;
	block_desc = NULL;
	page_index = NULL;

	CTL_DEBUG_PRINT(("ctl_mode_sense\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	if (lun->be_lun->lun_type != T_DIRECT)
		control_dev = 1;
	else
		control_dev = 0;

	switch (ctsio->cdb[0]) {
	case MODE_SENSE_6: {
		struct scsi_mode_sense_6 *cdb;

		cdb = (struct scsi_mode_sense_6 *)ctsio->cdb;

		header_len = sizeof(struct scsi_mode_hdr_6);
		if (cdb->byte2 & SMS_DBD)
			dbd = 1;
		else
			header_len += sizeof(struct scsi_mode_block_descr);

		pc = (cdb->page & SMS_PAGE_CTRL_MASK) >> 6;
		page_code = cdb->page & SMS_PAGE_CODE;
		subpage = cdb->subpage;
		alloc_len = cdb->length;
		break;
	}
	case MODE_SENSE_10: {
		struct scsi_mode_sense_10 *cdb;

		cdb = (struct scsi_mode_sense_10 *)ctsio->cdb;

		header_len = sizeof(struct scsi_mode_hdr_10);

		if (cdb->byte2 & SMS_DBD)
			dbd = 1;
		else
			header_len += sizeof(struct scsi_mode_block_descr);
		if (cdb->byte2 & SMS10_LLBAA)
			llba = 1;
		pc = (cdb->page & SMS_PAGE_CTRL_MASK) >> 6;
		page_code = cdb->page & SMS_PAGE_CODE;
		subpage = cdb->subpage;
		alloc_len = scsi_2btoul(cdb->length);
		break;
	}
	default:
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
		break; /* NOTREACHED */
	}

	/*
	 * We have to make a first pass through to calculate the size of
	 * the pages that match the user's query.  Then we allocate enough
	 * memory to hold it, and actually copy the data into the buffer.
	 */
	switch (page_code) {
	case SMS_ALL_PAGES_PAGE: {
		int i;

		page_len = 0;

		/*
		 * At the moment, values other than 0 and 0xff here are
		 * reserved according to SPC-3.
		 */
		if ((subpage != SMS_SUBPAGE_PAGE_0)
		 && (subpage != SMS_SUBPAGE_ALL)) {
			ctl_set_invalid_field(ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 1,
					      /*field*/ 3,
					      /*bit_valid*/ 0,
					      /*bit*/ 0);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}

		for (i = 0; i < CTL_NUM_MODE_PAGES; i++) {
			if ((control_dev != 0)
			 && (lun->mode_pages.index[i].page_flags &
			     CTL_PAGE_FLAG_DISK_ONLY))
				continue;

			/*
			 * We don't use this subpage if the user didn't
			 * request all subpages.
			 */
			if ((lun->mode_pages.index[i].subpage != 0)
			 && (subpage == SMS_SUBPAGE_PAGE_0))
				continue;

#if 0
			printf("found page %#x len %d\n",
			       lun->mode_pages.index[i].page_code &
			       SMPH_PC_MASK,
			       lun->mode_pages.index[i].page_len);
#endif
			page_len += lun->mode_pages.index[i].page_len;
		}
		break;
	}
	default: {
		int i;

		page_len = 0;

		for (i = 0; i < CTL_NUM_MODE_PAGES; i++) {
			/* Look for the right page code */
			if ((lun->mode_pages.index[i].page_code &
			     SMPH_PC_MASK) != page_code)
				continue;

			/* Look for the right subpage or the subpage wildcard*/
			if ((lun->mode_pages.index[i].subpage != subpage)
			 && (subpage != SMS_SUBPAGE_ALL))
				continue;

			/* Make sure the page is supported for this dev type */
			if ((control_dev != 0)
			 && (lun->mode_pages.index[i].page_flags &
			     CTL_PAGE_FLAG_DISK_ONLY))
				continue;

#if 0
			printf("found page %#x len %d\n",
			       lun->mode_pages.index[i].page_code &
			       SMPH_PC_MASK,
			       lun->mode_pages.index[i].page_len);
#endif

			page_len += lun->mode_pages.index[i].page_len;
		}

		if (page_len == 0) {
			ctl_set_invalid_field(ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 1,
					      /*field*/ 2,
					      /*bit_valid*/ 1,
					      /*bit*/ 5);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
		break;
	}
	}

	total_len = header_len + page_len;
#if 0
	printf("header_len = %d, page_len = %d, total_len = %d\n",
	       header_len, page_len, total_len);
#endif

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);
	ctsio->kern_sg_entries = 0;
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}

	switch (ctsio->cdb[0]) {
	case MODE_SENSE_6: {
		struct scsi_mode_hdr_6 *header;

		header = (struct scsi_mode_hdr_6 *)ctsio->kern_data_ptr;

		header->datalen = MIN(total_len - 1, 254);
		if (control_dev == 0) {
			header->dev_specific = 0x10; /* DPOFUA */
			if ((lun->flags & CTL_LUN_READONLY) ||
			    (lun->mode_pages.control_page[CTL_PAGE_CURRENT]
			    .eca_and_aen & SCP_SWP) != 0)
				    header->dev_specific |= 0x80; /* WP */
		}
		if (dbd)
			header->block_descr_len = 0;
		else
			header->block_descr_len =
				sizeof(struct scsi_mode_block_descr);
		block_desc = (struct scsi_mode_block_descr *)&header[1];
		break;
	}
	case MODE_SENSE_10: {
		struct scsi_mode_hdr_10 *header;
		int datalen;

		header = (struct scsi_mode_hdr_10 *)ctsio->kern_data_ptr;

		datalen = MIN(total_len - 2, 65533);
		scsi_ulto2b(datalen, header->datalen);
		if (control_dev == 0) {
			header->dev_specific = 0x10; /* DPOFUA */
			if ((lun->flags & CTL_LUN_READONLY) ||
			    (lun->mode_pages.control_page[CTL_PAGE_CURRENT]
			    .eca_and_aen & SCP_SWP) != 0)
				    header->dev_specific |= 0x80; /* WP */
		}
		if (dbd)
			scsi_ulto2b(0, header->block_descr_len);
		else
			scsi_ulto2b(sizeof(struct scsi_mode_block_descr),
				    header->block_descr_len);
		block_desc = (struct scsi_mode_block_descr *)&header[1];
		break;
	}
	default:
		panic("invalid CDB type %#x", ctsio->cdb[0]);
		break; /* NOTREACHED */
	}

	/*
	 * If we've got a disk, use its blocksize in the block
	 * descriptor.  Otherwise, just set it to 0.
	 */
	if (dbd == 0) {
		if (control_dev == 0)
			scsi_ulto3b(lun->be_lun->blocksize,
				    block_desc->block_len);
		else
			scsi_ulto3b(0, block_desc->block_len);
	}

	switch (page_code) {
	case SMS_ALL_PAGES_PAGE: {
		int i, data_used;

		data_used = header_len;
		for (i = 0; i < CTL_NUM_MODE_PAGES; i++) {
			struct ctl_page_index *page_index;

			page_index = &lun->mode_pages.index[i];

			if ((control_dev != 0)
			 && (page_index->page_flags &
			    CTL_PAGE_FLAG_DISK_ONLY))
				continue;

			/*
			 * We don't use this subpage if the user didn't
			 * request all subpages.  We already checked (above)
			 * to make sure the user only specified a subpage
			 * of 0 or 0xff in the SMS_ALL_PAGES_PAGE case.
			 */
			if ((page_index->subpage != 0)
			 && (subpage == SMS_SUBPAGE_PAGE_0))
				continue;

			/*
			 * Call the handler, if it exists, to update the
			 * page to the latest values.
			 */
			if (page_index->sense_handler != NULL)
				page_index->sense_handler(ctsio, page_index,pc);

			memcpy(ctsio->kern_data_ptr + data_used,
			       page_index->page_data +
			       (page_index->page_len * pc),
			       page_index->page_len);
			data_used += page_index->page_len;
		}
		break;
	}
	default: {
		int i, data_used;

		data_used = header_len;

		for (i = 0; i < CTL_NUM_MODE_PAGES; i++) {
			struct ctl_page_index *page_index;

			page_index = &lun->mode_pages.index[i];

			/* Look for the right page code */
			if ((page_index->page_code & SMPH_PC_MASK) != page_code)
				continue;

			/* Look for the right subpage or the subpage wildcard*/
			if ((page_index->subpage != subpage)
			 && (subpage != SMS_SUBPAGE_ALL))
				continue;

			/* Make sure the page is supported for this dev type */
			if ((control_dev != 0)
			 && (page_index->page_flags &
			     CTL_PAGE_FLAG_DISK_ONLY))
				continue;

			/*
			 * Call the handler, if it exists, to update the
			 * page to the latest values.
			 */
			if (page_index->sense_handler != NULL)
				page_index->sense_handler(ctsio, page_index,pc);

			memcpy(ctsio->kern_data_ptr + data_used,
			       page_index->page_data +
			       (page_index->page_len * pc),
			       page_index->page_len);
			data_used += page_index->page_len;
		}
		break;
	}
	}

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_lbp_log_sense_handler(struct ctl_scsiio *ctsio,
			       struct ctl_page_index *page_index,
			       int pc)
{
	struct ctl_lun *lun;
	struct scsi_log_param_header *phdr;
	uint8_t *data;
	uint64_t val;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	data = page_index->page_data;

	if (lun->backend->lun_attr != NULL &&
	    (val = lun->backend->lun_attr(lun->be_lun->be_lun, "blocksavail"))
	     != UINT64_MAX) {
		phdr = (struct scsi_log_param_header *)data;
		scsi_ulto2b(0x0001, phdr->param_code);
		phdr->param_control = SLP_LBIN | SLP_LP;
		phdr->param_len = 8;
		data = (uint8_t *)(phdr + 1);
		scsi_ulto4b(val >> CTL_LBP_EXPONENT, data);
		data[4] = 0x02; /* per-pool */
		data += phdr->param_len;
	}

	if (lun->backend->lun_attr != NULL &&
	    (val = lun->backend->lun_attr(lun->be_lun->be_lun, "blocksused"))
	     != UINT64_MAX) {
		phdr = (struct scsi_log_param_header *)data;
		scsi_ulto2b(0x0002, phdr->param_code);
		phdr->param_control = SLP_LBIN | SLP_LP;
		phdr->param_len = 8;
		data = (uint8_t *)(phdr + 1);
		scsi_ulto4b(val >> CTL_LBP_EXPONENT, data);
		data[4] = 0x01; /* per-LUN */
		data += phdr->param_len;
	}

	if (lun->backend->lun_attr != NULL &&
	    (val = lun->backend->lun_attr(lun->be_lun->be_lun, "poolblocksavail"))
	     != UINT64_MAX) {
		phdr = (struct scsi_log_param_header *)data;
		scsi_ulto2b(0x00f1, phdr->param_code);
		phdr->param_control = SLP_LBIN | SLP_LP;
		phdr->param_len = 8;
		data = (uint8_t *)(phdr + 1);
		scsi_ulto4b(val >> CTL_LBP_EXPONENT, data);
		data[4] = 0x02; /* per-pool */
		data += phdr->param_len;
	}

	if (lun->backend->lun_attr != NULL &&
	    (val = lun->backend->lun_attr(lun->be_lun->be_lun, "poolblocksused"))
	     != UINT64_MAX) {
		phdr = (struct scsi_log_param_header *)data;
		scsi_ulto2b(0x00f2, phdr->param_code);
		phdr->param_control = SLP_LBIN | SLP_LP;
		phdr->param_len = 8;
		data = (uint8_t *)(phdr + 1);
		scsi_ulto4b(val >> CTL_LBP_EXPONENT, data);
		data[4] = 0x02; /* per-pool */
		data += phdr->param_len;
	}

	page_index->page_len = data - page_index->page_data;
	return (0);
}

int
ctl_sap_log_sense_handler(struct ctl_scsiio *ctsio,
			       struct ctl_page_index *page_index,
			       int pc)
{
	struct ctl_lun *lun;
	struct stat_page *data;
	uint64_t rn, wn, rb, wb;
	struct bintime rt, wt;
	int i;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	data = (struct stat_page *)page_index->page_data;

	scsi_ulto2b(SLP_SAP, data->sap.hdr.param_code);
	data->sap.hdr.param_control = SLP_LBIN;
	data->sap.hdr.param_len = sizeof(struct scsi_log_stat_and_perf) -
	    sizeof(struct scsi_log_param_header);
	rn = wn = rb = wb = 0;
	bintime_clear(&rt);
	bintime_clear(&wt);
	for (i = 0; i < CTL_MAX_PORTS; i++) {
		rn += lun->stats.ports[i].operations[CTL_STATS_READ];
		wn += lun->stats.ports[i].operations[CTL_STATS_WRITE];
		rb += lun->stats.ports[i].bytes[CTL_STATS_READ];
		wb += lun->stats.ports[i].bytes[CTL_STATS_WRITE];
		bintime_add(&rt, &lun->stats.ports[i].time[CTL_STATS_READ]);
		bintime_add(&wt, &lun->stats.ports[i].time[CTL_STATS_WRITE]);
	}
	scsi_u64to8b(rn, data->sap.read_num);
	scsi_u64to8b(wn, data->sap.write_num);
	if (lun->stats.blocksize > 0) {
		scsi_u64to8b(wb / lun->stats.blocksize,
		    data->sap.recvieved_lba);
		scsi_u64to8b(rb / lun->stats.blocksize,
		    data->sap.transmitted_lba);
	}
	scsi_u64to8b((uint64_t)rt.sec * 1000 + rt.frac / (UINT64_MAX / 1000),
	    data->sap.read_int);
	scsi_u64to8b((uint64_t)wt.sec * 1000 + wt.frac / (UINT64_MAX / 1000),
	    data->sap.write_int);
	scsi_u64to8b(0, data->sap.weighted_num);
	scsi_u64to8b(0, data->sap.weighted_int);
	scsi_ulto2b(SLP_IT, data->it.hdr.param_code);
	data->it.hdr.param_control = SLP_LBIN;
	data->it.hdr.param_len = sizeof(struct scsi_log_idle_time) -
	    sizeof(struct scsi_log_param_header);
#ifdef CTL_TIME_IO
	scsi_u64to8b(lun->idle_time / SBT_1MS, data->it.idle_int);
#endif
	scsi_ulto2b(SLP_TI, data->ti.hdr.param_code);
	data->it.hdr.param_control = SLP_LBIN;
	data->ti.hdr.param_len = sizeof(struct scsi_log_time_interval) -
	    sizeof(struct scsi_log_param_header);
	scsi_ulto4b(3, data->ti.exponent);
	scsi_ulto4b(1, data->ti.integer);

	page_index->page_len = sizeof(*data);
	return (0);
}

int
ctl_log_sense(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	int i, pc, page_code, subpage;
	int alloc_len, total_len;
	struct ctl_page_index *page_index;
	struct scsi_log_sense *cdb;
	struct scsi_log_header *header;

	CTL_DEBUG_PRINT(("ctl_log_sense\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_log_sense *)ctsio->cdb;
	pc = (cdb->page & SLS_PAGE_CTRL_MASK) >> 6;
	page_code = cdb->page & SLS_PAGE_CODE;
	subpage = cdb->subpage;
	alloc_len = scsi_2btoul(cdb->length);

	page_index = NULL;
	for (i = 0; i < CTL_NUM_LOG_PAGES; i++) {
		page_index = &lun->log_pages.index[i];

		/* Look for the right page code */
		if ((page_index->page_code & SL_PAGE_CODE) != page_code)
			continue;

		/* Look for the right subpage or the subpage wildcard*/
		if (page_index->subpage != subpage)
			continue;

		break;
	}
	if (i >= CTL_NUM_LOG_PAGES) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 2,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	total_len = sizeof(struct scsi_log_header) + page_index->page_len;

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);
	ctsio->kern_sg_entries = 0;
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}

	header = (struct scsi_log_header *)ctsio->kern_data_ptr;
	header->page = page_index->page_code;
	if (page_index->subpage) {
		header->page |= SL_SPF;
		header->subpage = page_index->subpage;
	}
	scsi_ulto2b(page_index->page_len, header->datalen);

	/*
	 * Call the handler, if it exists, to update the
	 * page to the latest values.
	 */
	if (page_index->sense_handler != NULL)
		page_index->sense_handler(ctsio, page_index, pc);

	memcpy(header + 1, page_index->page_data, page_index->page_len);

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_read_capacity(struct ctl_scsiio *ctsio)
{
	struct scsi_read_capacity *cdb;
	struct scsi_read_capacity_data *data;
	struct ctl_lun *lun;
	uint32_t lba;

	CTL_DEBUG_PRINT(("ctl_read_capacity\n"));

	cdb = (struct scsi_read_capacity *)ctsio->cdb;

	lba = scsi_4btoul(cdb->addr);
	if (((cdb->pmi & SRC_PMI) == 0)
	 && (lba != 0)) {
		ctl_set_invalid_field(/*ctsio*/ ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 2,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	ctsio->kern_data_ptr = malloc(sizeof(*data), M_CTL, M_WAITOK | M_ZERO);
	data = (struct scsi_read_capacity_data *)ctsio->kern_data_ptr;
	ctsio->residual = 0;
	ctsio->kern_data_len = sizeof(*data);
	ctsio->kern_total_len = sizeof(*data);
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * If the maximum LBA is greater than 0xfffffffe, the user must
	 * issue a SERVICE ACTION IN (16) command, with the read capacity
	 * serivce action set.
	 */
	if (lun->be_lun->maxlba > 0xfffffffe)
		scsi_ulto4b(0xffffffff, data->addr);
	else
		scsi_ulto4b(lun->be_lun->maxlba, data->addr);

	/*
	 * XXX KDM this may not be 512 bytes...
	 */
	scsi_ulto4b(lun->be_lun->blocksize, data->length);

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_read_capacity_16(struct ctl_scsiio *ctsio)
{
	struct scsi_read_capacity_16 *cdb;
	struct scsi_read_capacity_data_long *data;
	struct ctl_lun *lun;
	uint64_t lba;
	uint32_t alloc_len;

	CTL_DEBUG_PRINT(("ctl_read_capacity_16\n"));

	cdb = (struct scsi_read_capacity_16 *)ctsio->cdb;

	alloc_len = scsi_4btoul(cdb->alloc_len);
	lba = scsi_8btou64(cdb->addr);

	if ((cdb->reladr & SRC16_PMI)
	 && (lba != 0)) {
		ctl_set_invalid_field(/*ctsio*/ ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 2,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	ctsio->kern_data_ptr = malloc(sizeof(*data), M_CTL, M_WAITOK | M_ZERO);
	data = (struct scsi_read_capacity_data_long *)ctsio->kern_data_ptr;

	if (sizeof(*data) < alloc_len) {
		ctsio->residual = alloc_len - sizeof(*data);
		ctsio->kern_data_len = sizeof(*data);
		ctsio->kern_total_len = sizeof(*data);
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	scsi_u64to8b(lun->be_lun->maxlba, data->addr);
	/* XXX KDM this may not be 512 bytes... */
	scsi_ulto4b(lun->be_lun->blocksize, data->length);
	data->prot_lbppbe = lun->be_lun->pblockexp & SRC16_LBPPBE;
	scsi_ulto2b(lun->be_lun->pblockoff & SRC16_LALBA_A, data->lalba_lbp);
	if (lun->be_lun->flags & CTL_LUN_FLAG_UNMAP)
		data->lalba_lbp[0] |= SRC16_LBPME | SRC16_LBPRZ;

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_get_lba_status(struct ctl_scsiio *ctsio)
{
	struct scsi_get_lba_status *cdb;
	struct scsi_get_lba_status_data *data;
	struct ctl_lun *lun;
	struct ctl_lba_len_flags *lbalen;
	uint64_t lba;
	uint32_t alloc_len, total_len;
	int retval;

	CTL_DEBUG_PRINT(("ctl_get_lba_status\n"));

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_get_lba_status *)ctsio->cdb;
	lba = scsi_8btou64(cdb->addr);
	alloc_len = scsi_4btoul(cdb->alloc_len);

	if (lba > lun->be_lun->maxlba) {
		ctl_set_lba_out_of_range(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	total_len = sizeof(*data) + sizeof(data->descr[0]);
	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);
	data = (struct scsi_get_lba_status_data *)ctsio->kern_data_ptr;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/* Fill dummy data in case backend can't tell anything. */
	scsi_ulto4b(4 + sizeof(data->descr[0]), data->length);
	scsi_u64to8b(lba, data->descr[0].addr);
	scsi_ulto4b(MIN(UINT32_MAX, lun->be_lun->maxlba + 1 - lba),
	    data->descr[0].length);
	data->descr[0].status = 0; /* Mapped or unknown. */

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;

	lbalen = (struct ctl_lba_len_flags *)&ctsio->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	lbalen->lba = lba;
	lbalen->len = total_len;
	lbalen->flags = 0;
	retval = lun->backend->config_read((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_read_defect(struct ctl_scsiio *ctsio)
{
	struct scsi_read_defect_data_10 *ccb10;
	struct scsi_read_defect_data_12 *ccb12;
	struct scsi_read_defect_data_hdr_10 *data10;
	struct scsi_read_defect_data_hdr_12 *data12;
	uint32_t alloc_len, data_len;
	uint8_t format;

	CTL_DEBUG_PRINT(("ctl_read_defect\n"));

	if (ctsio->cdb[0] == READ_DEFECT_DATA_10) {
		ccb10 = (struct scsi_read_defect_data_10 *)&ctsio->cdb;
		format = ccb10->format;
		alloc_len = scsi_2btoul(ccb10->alloc_length);
		data_len = sizeof(*data10);
	} else {
		ccb12 = (struct scsi_read_defect_data_12 *)&ctsio->cdb;
		format = ccb12->format;
		alloc_len = scsi_4btoul(ccb12->alloc_length);
		data_len = sizeof(*data12);
	}
	if (alloc_len == 0) {
		ctl_set_success(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	ctsio->kern_data_ptr = malloc(data_len, M_CTL, M_WAITOK | M_ZERO);
	if (data_len < alloc_len) {
		ctsio->residual = alloc_len - data_len;
		ctsio->kern_data_len = data_len;
		ctsio->kern_total_len = data_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	if (ctsio->cdb[0] == READ_DEFECT_DATA_10) {
		data10 = (struct scsi_read_defect_data_hdr_10 *)
		    ctsio->kern_data_ptr;
		data10->format = format;
		scsi_ulto2b(0, data10->length);
	} else {
		data12 = (struct scsi_read_defect_data_hdr_12 *)
		    ctsio->kern_data_ptr;
		data12->format = format;
		scsi_ulto2b(0, data12->generation);
		scsi_ulto4b(0, data12->length);
	}

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_report_tagret_port_groups(struct ctl_scsiio *ctsio)
{
	struct scsi_maintenance_in *cdb;
	int retval;
	int alloc_len, ext, total_len = 0, g, p, pc, pg, gs, os;
	int num_target_port_groups, num_target_ports;
	struct ctl_lun *lun;
	struct ctl_softc *softc;
	struct ctl_port *port;
	struct scsi_target_group_data *rtg_ptr;
	struct scsi_target_group_data_extended *rtg_ext_ptr;
	struct scsi_target_port_group_descriptor *tpg_desc;

	CTL_DEBUG_PRINT(("ctl_report_tagret_port_groups\n"));

	cdb = (struct scsi_maintenance_in *)ctsio->cdb;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	softc = lun->ctl_softc;

	retval = CTL_RETVAL_COMPLETE;

	switch (cdb->byte2 & STG_PDF_MASK) {
	case STG_PDF_LENGTH:
		ext = 0;
		break;
	case STG_PDF_EXTENDED:
		ext = 1;
		break;
	default:
		ctl_set_invalid_field(/*ctsio*/ ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 2,
				      /*bit_valid*/ 1,
				      /*bit*/ 5);
		ctl_done((union ctl_io *)ctsio);
		return(retval);
	}

	if (softc->is_single)
		num_target_port_groups = 1;
	else
		num_target_port_groups = NUM_TARGET_PORT_GROUPS;
	num_target_ports = 0;
	mtx_lock(&softc->ctl_lock);
	STAILQ_FOREACH(port, &softc->port_list, links) {
		if ((port->status & CTL_PORT_STATUS_ONLINE) == 0)
			continue;
		if (ctl_lun_map_to_port(port, lun->lun) >= CTL_MAX_LUNS)
			continue;
		num_target_ports++;
	}
	mtx_unlock(&softc->ctl_lock);

	if (ext)
		total_len = sizeof(struct scsi_target_group_data_extended);
	else
		total_len = sizeof(struct scsi_target_group_data);
	total_len += sizeof(struct scsi_target_port_group_descriptor) *
		num_target_port_groups +
	    sizeof(struct scsi_target_port_descriptor) *
		num_target_ports * num_target_port_groups;

	alloc_len = scsi_4btoul(cdb->length);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	ctsio->kern_sg_entries = 0;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	if (ext) {
		rtg_ext_ptr = (struct scsi_target_group_data_extended *)
		    ctsio->kern_data_ptr;
		scsi_ulto4b(total_len - 4, rtg_ext_ptr->length);
		rtg_ext_ptr->format_type = 0x10;
		rtg_ext_ptr->implicit_transition_time = 0;
		tpg_desc = &rtg_ext_ptr->groups[0];
	} else {
		rtg_ptr = (struct scsi_target_group_data *)
		    ctsio->kern_data_ptr;
		scsi_ulto4b(total_len - 4, rtg_ptr->length);
		tpg_desc = &rtg_ptr->groups[0];
	}

	mtx_lock(&softc->ctl_lock);
	pg = softc->port_offset / CTL_MAX_PORTS;
	if (softc->flags & CTL_FLAG_ACTIVE_SHELF) {
		if (softc->ha_mode == CTL_HA_MODE_ACT_STBY) {
			gs = TPG_ASYMMETRIC_ACCESS_OPTIMIZED;
			os = TPG_ASYMMETRIC_ACCESS_STANDBY;
		} else if (lun->flags & CTL_LUN_PRIMARY_SC) {
			gs = TPG_ASYMMETRIC_ACCESS_OPTIMIZED;
			os = TPG_ASYMMETRIC_ACCESS_NONOPTIMIZED;
		} else {
			gs = TPG_ASYMMETRIC_ACCESS_NONOPTIMIZED;
			os = TPG_ASYMMETRIC_ACCESS_OPTIMIZED;
		}
	} else {
		gs = TPG_ASYMMETRIC_ACCESS_STANDBY;
		os = TPG_ASYMMETRIC_ACCESS_OPTIMIZED;
	}
	for (g = 0; g < num_target_port_groups; g++) {
		tpg_desc->pref_state = (g == pg) ? gs : os;
		tpg_desc->support = TPG_AO_SUP | TPG_AN_SUP | TPG_S_SUP;
		scsi_ulto2b(g + 1, tpg_desc->target_port_group);
		tpg_desc->status = TPG_IMPLICIT;
		pc = 0;
		STAILQ_FOREACH(port, &softc->port_list, links) {
			if ((port->status & CTL_PORT_STATUS_ONLINE) == 0)
				continue;
			if (ctl_lun_map_to_port(port, lun->lun) >= CTL_MAX_LUNS)
				continue;
			p = port->targ_port % CTL_MAX_PORTS + g * CTL_MAX_PORTS;
			scsi_ulto2b(p, tpg_desc->descriptors[pc].
			    relative_target_port_identifier);
			pc++;
		}
		tpg_desc->target_port_count = pc;
		tpg_desc = (struct scsi_target_port_group_descriptor *)
		    &tpg_desc->descriptors[pc];
	}
	mtx_unlock(&softc->ctl_lock);

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return(retval);
}

int
ctl_report_supported_opcodes(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct scsi_report_supported_opcodes *cdb;
	const struct ctl_cmd_entry *entry, *sentry;
	struct scsi_report_supported_opcodes_all *all;
	struct scsi_report_supported_opcodes_descr *descr;
	struct scsi_report_supported_opcodes_one *one;
	int retval;
	int alloc_len, total_len;
	int opcode, service_action, i, j, num;

	CTL_DEBUG_PRINT(("ctl_report_supported_opcodes\n"));

	cdb = (struct scsi_report_supported_opcodes *)ctsio->cdb;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	retval = CTL_RETVAL_COMPLETE;

	opcode = cdb->requested_opcode;
	service_action = scsi_2btoul(cdb->requested_service_action);
	switch (cdb->options & RSO_OPTIONS_MASK) {
	case RSO_OPTIONS_ALL:
		num = 0;
		for (i = 0; i < 256; i++) {
			entry = &ctl_cmd_table[i];
			if (entry->flags & CTL_CMD_FLAG_SA5) {
				for (j = 0; j < 32; j++) {
					sentry = &((const struct ctl_cmd_entry *)
					    entry->execute)[j];
					if (ctl_cmd_applicable(
					    lun->be_lun->lun_type, sentry))
						num++;
				}
			} else {
				if (ctl_cmd_applicable(lun->be_lun->lun_type,
				    entry))
					num++;
			}
		}
		total_len = sizeof(struct scsi_report_supported_opcodes_all) +
		    num * sizeof(struct scsi_report_supported_opcodes_descr);
		break;
	case RSO_OPTIONS_OC:
		if (ctl_cmd_table[opcode].flags & CTL_CMD_FLAG_SA5) {
			ctl_set_invalid_field(/*ctsio*/ ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 1,
					      /*field*/ 2,
					      /*bit_valid*/ 1,
					      /*bit*/ 2);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
		total_len = sizeof(struct scsi_report_supported_opcodes_one) + 32;
		break;
	case RSO_OPTIONS_OC_SA:
		if ((ctl_cmd_table[opcode].flags & CTL_CMD_FLAG_SA5) == 0 ||
		    service_action >= 32) {
			ctl_set_invalid_field(/*ctsio*/ ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 1,
					      /*field*/ 2,
					      /*bit_valid*/ 1,
					      /*bit*/ 2);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
		total_len = sizeof(struct scsi_report_supported_opcodes_one) + 32;
		break;
	default:
		ctl_set_invalid_field(/*ctsio*/ ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 2,
				      /*bit_valid*/ 1,
				      /*bit*/ 2);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	alloc_len = scsi_4btoul(cdb->length);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	ctsio->kern_sg_entries = 0;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	switch (cdb->options & RSO_OPTIONS_MASK) {
	case RSO_OPTIONS_ALL:
		all = (struct scsi_report_supported_opcodes_all *)
		    ctsio->kern_data_ptr;
		num = 0;
		for (i = 0; i < 256; i++) {
			entry = &ctl_cmd_table[i];
			if (entry->flags & CTL_CMD_FLAG_SA5) {
				for (j = 0; j < 32; j++) {
					sentry = &((const struct ctl_cmd_entry *)
					    entry->execute)[j];
					if (!ctl_cmd_applicable(
					    lun->be_lun->lun_type, sentry))
						continue;
					descr = &all->descr[num++];
					descr->opcode = i;
					scsi_ulto2b(j, descr->service_action);
					descr->flags = RSO_SERVACTV;
					scsi_ulto2b(sentry->length,
					    descr->cdb_length);
				}
			} else {
				if (!ctl_cmd_applicable(lun->be_lun->lun_type,
				    entry))
					continue;
				descr = &all->descr[num++];
				descr->opcode = i;
				scsi_ulto2b(0, descr->service_action);
				descr->flags = 0;
				scsi_ulto2b(entry->length, descr->cdb_length);
			}
		}
		scsi_ulto4b(
		    num * sizeof(struct scsi_report_supported_opcodes_descr),
		    all->length);
		break;
	case RSO_OPTIONS_OC:
		one = (struct scsi_report_supported_opcodes_one *)
		    ctsio->kern_data_ptr;
		entry = &ctl_cmd_table[opcode];
		goto fill_one;
	case RSO_OPTIONS_OC_SA:
		one = (struct scsi_report_supported_opcodes_one *)
		    ctsio->kern_data_ptr;
		entry = &ctl_cmd_table[opcode];
		entry = &((const struct ctl_cmd_entry *)
		    entry->execute)[service_action];
fill_one:
		if (ctl_cmd_applicable(lun->be_lun->lun_type, entry)) {
			one->support = 3;
			scsi_ulto2b(entry->length, one->cdb_length);
			one->cdb_usage[0] = opcode;
			memcpy(&one->cdb_usage[1], entry->usage,
			    entry->length - 1);
		} else
			one->support = 1;
		break;
	}

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return(retval);
}

int
ctl_report_supported_tmf(struct ctl_scsiio *ctsio)
{
	struct scsi_report_supported_tmf *cdb;
	struct scsi_report_supported_tmf_data *data;
	int retval;
	int alloc_len, total_len;

	CTL_DEBUG_PRINT(("ctl_report_supported_tmf\n"));

	cdb = (struct scsi_report_supported_tmf *)ctsio->cdb;

	retval = CTL_RETVAL_COMPLETE;

	total_len = sizeof(struct scsi_report_supported_tmf_data);
	alloc_len = scsi_4btoul(cdb->length);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	ctsio->kern_sg_entries = 0;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	data = (struct scsi_report_supported_tmf_data *)ctsio->kern_data_ptr;
	data->byte1 |= RST_ATS | RST_ATSS | RST_CTSS | RST_LURS | RST_TRS;
	data->byte2 |= RST_ITNRS;

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_report_timestamp(struct ctl_scsiio *ctsio)
{
	struct scsi_report_timestamp *cdb;
	struct scsi_report_timestamp_data *data;
	struct timeval tv;
	int64_t timestamp;
	int retval;
	int alloc_len, total_len;

	CTL_DEBUG_PRINT(("ctl_report_timestamp\n"));

	cdb = (struct scsi_report_timestamp *)ctsio->cdb;

	retval = CTL_RETVAL_COMPLETE;

	total_len = sizeof(struct scsi_report_timestamp_data);
	alloc_len = scsi_4btoul(cdb->length);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	ctsio->kern_sg_entries = 0;

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	data = (struct scsi_report_timestamp_data *)ctsio->kern_data_ptr;
	scsi_ulto2b(sizeof(*data) - 2, data->length);
	data->origin = RTS_ORIG_OUTSIDE;
	getmicrotime(&tv);
	timestamp = (int64_t)tv.tv_sec * 1000 + tv.tv_usec / 1000;
	scsi_ulto4b(timestamp >> 16, data->timestamp);
	scsi_ulto2b(timestamp & 0xffff, &data->timestamp[4]);

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_persistent_reserve_in(struct ctl_scsiio *ctsio)
{
	struct scsi_per_res_in *cdb;
	int alloc_len, total_len = 0;
	/* struct scsi_per_res_in_rsrv in_data; */
	struct ctl_lun *lun;
	struct ctl_softc *softc;
	uint64_t key;

	CTL_DEBUG_PRINT(("ctl_persistent_reserve_in\n"));

	cdb = (struct scsi_per_res_in *)ctsio->cdb;

	alloc_len = scsi_2btoul(cdb->length);

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	softc = lun->ctl_softc;

retry:
	mtx_lock(&lun->lun_lock);
	switch (cdb->action) {
	case SPRI_RK: /* read keys */
		total_len = sizeof(struct scsi_per_res_in_keys) +
			lun->pr_key_count *
			sizeof(struct scsi_per_res_key);
		break;
	case SPRI_RR: /* read reservation */
		if (lun->flags & CTL_LUN_PR_RESERVED)
			total_len = sizeof(struct scsi_per_res_in_rsrv);
		else
			total_len = sizeof(struct scsi_per_res_in_header);
		break;
	case SPRI_RC: /* report capabilities */
		total_len = sizeof(struct scsi_per_res_cap);
		break;
	case SPRI_RS: /* read full status */
		total_len = sizeof(struct scsi_per_res_in_header) +
		    (sizeof(struct scsi_per_res_in_full_desc) + 256) *
		    lun->pr_key_count;
		break;
	default:
		panic("Invalid PR type %x", cdb->action);
	}
	mtx_unlock(&lun->lun_lock);

	ctsio->kern_data_ptr = malloc(total_len, M_CTL, M_WAITOK | M_ZERO);

	if (total_len < alloc_len) {
		ctsio->residual = alloc_len - total_len;
		ctsio->kern_data_len = total_len;
		ctsio->kern_total_len = total_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}

	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	mtx_lock(&lun->lun_lock);
	switch (cdb->action) {
	case SPRI_RK: { // read keys
        struct scsi_per_res_in_keys *res_keys;
		int i, key_count;

		res_keys = (struct scsi_per_res_in_keys*)ctsio->kern_data_ptr;

		/*
		 * We had to drop the lock to allocate our buffer, which
		 * leaves time for someone to come in with another
		 * persistent reservation.  (That is unlikely, though,
		 * since this should be the only persistent reservation
		 * command active right now.)
		 */
		if (total_len != (sizeof(struct scsi_per_res_in_keys) +
		    (lun->pr_key_count *
		     sizeof(struct scsi_per_res_key)))){
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			printf("%s: reservation length changed, retrying\n",
			       __func__);
			goto retry;
		}

		scsi_ulto4b(lun->PRGeneration, res_keys->header.generation);

		scsi_ulto4b(sizeof(struct scsi_per_res_key) *
			     lun->pr_key_count, res_keys->header.length);

		for (i = 0, key_count = 0; i < 2*CTL_MAX_INITIATORS; i++) {
			if ((key = ctl_get_prkey(lun, i)) == 0)
				continue;

			/*
			 * We used lun->pr_key_count to calculate the
			 * size to allocate.  If it turns out the number of
			 * initiators with the registered flag set is
			 * larger than that (i.e. they haven't been kept in
			 * sync), we've got a problem.
			 */
			if (key_count >= lun->pr_key_count) {
#ifdef NEEDTOPORT
				csevent_log(CSC_CTL | CSC_SHELF_SW |
					    CTL_PR_ERROR,
					    csevent_LogType_Fault,
					    csevent_AlertLevel_Yellow,
					    csevent_FRU_ShelfController,
					    csevent_FRU_Firmware,
				        csevent_FRU_Unknown,
					    "registered keys %d >= key "
					    "count %d", key_count,
					    lun->pr_key_count);
#endif
				key_count++;
				continue;
			}
			scsi_u64to8b(key, res_keys->keys[key_count].key);
			key_count++;
		}
		break;
	}
	case SPRI_RR: { // read reservation
		struct scsi_per_res_in_rsrv *res;
		int tmp_len, header_only;

		res = (struct scsi_per_res_in_rsrv *)ctsio->kern_data_ptr;

		scsi_ulto4b(lun->PRGeneration, res->header.generation);

		if (lun->flags & CTL_LUN_PR_RESERVED)
		{
			tmp_len = sizeof(struct scsi_per_res_in_rsrv);
			scsi_ulto4b(sizeof(struct scsi_per_res_in_rsrv_data),
				    res->header.length);
			header_only = 0;
		} else {
			tmp_len = sizeof(struct scsi_per_res_in_header);
			scsi_ulto4b(0, res->header.length);
			header_only = 1;
		}

		/*
		 * We had to drop the lock to allocate our buffer, which
		 * leaves time for someone to come in with another
		 * persistent reservation.  (That is unlikely, though,
		 * since this should be the only persistent reservation
		 * command active right now.)
		 */
		if (tmp_len != total_len) {
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			printf("%s: reservation status changed, retrying\n",
			       __func__);
			goto retry;
		}

		/*
		 * No reservation held, so we're done.
		 */
		if (header_only != 0)
			break;

		/*
		 * If the registration is an All Registrants type, the key
		 * is 0, since it doesn't really matter.
		 */
		if (lun->pr_res_idx != CTL_PR_ALL_REGISTRANTS) {
			scsi_u64to8b(ctl_get_prkey(lun, lun->pr_res_idx),
			    res->data.reservation);
		}
		res->data.scopetype = lun->res_type;
		break;
	}
	case SPRI_RC:     //report capabilities
	{
		struct scsi_per_res_cap *res_cap;
		uint16_t type_mask;

		res_cap = (struct scsi_per_res_cap *)ctsio->kern_data_ptr;
		scsi_ulto2b(sizeof(*res_cap), res_cap->length);
		res_cap->flags2 |= SPRI_TMV | SPRI_ALLOW_5;
		type_mask = SPRI_TM_WR_EX_AR |
			    SPRI_TM_EX_AC_RO |
			    SPRI_TM_WR_EX_RO |
			    SPRI_TM_EX_AC |
			    SPRI_TM_WR_EX |
			    SPRI_TM_EX_AC_AR;
		scsi_ulto2b(type_mask, res_cap->type_mask);
		break;
	}
	case SPRI_RS: { // read full status
		struct scsi_per_res_in_full *res_status;
		struct scsi_per_res_in_full_desc *res_desc;
		struct ctl_port *port;
		int i, len;

		res_status = (struct scsi_per_res_in_full*)ctsio->kern_data_ptr;

		/*
		 * We had to drop the lock to allocate our buffer, which
		 * leaves time for someone to come in with another
		 * persistent reservation.  (That is unlikely, though,
		 * since this should be the only persistent reservation
		 * command active right now.)
		 */
		if (total_len < (sizeof(struct scsi_per_res_in_header) +
		    (sizeof(struct scsi_per_res_in_full_desc) + 256) *
		     lun->pr_key_count)){
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			printf("%s: reservation length changed, retrying\n",
			       __func__);
			goto retry;
		}

		scsi_ulto4b(lun->PRGeneration, res_status->header.generation);

		res_desc = &res_status->desc[0];
		for (i = 0; i < 2*CTL_MAX_INITIATORS; i++) {
			if ((key = ctl_get_prkey(lun, i)) == 0)
				continue;

			scsi_u64to8b(key, res_desc->res_key.key);
			if ((lun->flags & CTL_LUN_PR_RESERVED) &&
			    (lun->pr_res_idx == i ||
			     lun->pr_res_idx == CTL_PR_ALL_REGISTRANTS)) {
				res_desc->flags = SPRI_FULL_R_HOLDER;
				res_desc->scopetype = lun->res_type;
			}
			scsi_ulto2b(i / CTL_MAX_INIT_PER_PORT,
			    res_desc->rel_trgt_port_id);
			len = 0;
			port = softc->ctl_ports[
			    ctl_port_idx(i / CTL_MAX_INIT_PER_PORT)];
			if (port != NULL)
				len = ctl_create_iid(port,
				    i % CTL_MAX_INIT_PER_PORT,
				    res_desc->transport_id);
			scsi_ulto4b(len, res_desc->additional_length);
			res_desc = (struct scsi_per_res_in_full_desc *)
			    &res_desc->transport_id[len];
		}
		scsi_ulto4b((uint8_t *)res_desc - (uint8_t *)&res_status->desc[0],
		    res_status->header.length);
		break;
	}
	default:
		/*
		 * This is a bug, because we just checked for this above,
		 * and should have returned an error.
		 */
		panic("Invalid PR type %x", cdb->action);
		break; /* NOTREACHED */
	}
	mtx_unlock(&lun->lun_lock);

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

static void
ctl_est_res_ua(struct ctl_lun *lun, uint32_t residx, ctl_ua_type ua)
{
	int off = lun->ctl_softc->persis_offset;

	if (residx >= off && residx < off + CTL_MAX_INITIATORS)
		ctl_est_ua(lun, residx - off, ua);
}

/*
 * Returns 0 if ctl_persistent_reserve_out() should continue, non-zero if
 * it should return.
 */
static int
ctl_pro_preempt(struct ctl_softc *softc, struct ctl_lun *lun, uint64_t res_key,
		uint64_t sa_res_key, uint8_t type, uint32_t residx,
		struct ctl_scsiio *ctsio, struct scsi_per_res_out *cdb,
		struct scsi_per_res_out_parms* param)
{
	union ctl_ha_msg persis_io;
	int retval, i;
	int isc_retval;

	retval = 0;

	mtx_lock(&lun->lun_lock);
	if (sa_res_key == 0) {
		if (lun->pr_res_idx == CTL_PR_ALL_REGISTRANTS) {
			/* validate scope and type */
			if ((cdb->scope_type & SPR_SCOPE_MASK) !=
			     SPR_LU_SCOPE) {
				mtx_unlock(&lun->lun_lock);
				ctl_set_invalid_field(/*ctsio*/ ctsio,
						      /*sks_valid*/ 1,
						      /*command*/ 1,
						      /*field*/ 2,
						      /*bit_valid*/ 1,
						      /*bit*/ 4);
				ctl_done((union ctl_io *)ctsio);
				return (1);
			}

		        if (type>8 || type==2 || type==4 || type==0) {
				mtx_unlock(&lun->lun_lock);
				ctl_set_invalid_field(/*ctsio*/ ctsio,
       	           				      /*sks_valid*/ 1,
						      /*command*/ 1,
						      /*field*/ 2,
						      /*bit_valid*/ 1,
						      /*bit*/ 0);
				ctl_done((union ctl_io *)ctsio);
				return (1);
		        }

			/*
			 * Unregister everybody else and build UA for
			 * them
			 */
			for(i=0; i < 2*CTL_MAX_INITIATORS; i++) {
				if (i == residx || ctl_get_prkey(lun, i) == 0)
					continue;

				ctl_clr_prkey(lun, i);
				ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
			}
			lun->pr_key_count = 1;
			lun->res_type = type;
			if (lun->res_type != SPR_TYPE_WR_EX_AR
			 && lun->res_type != SPR_TYPE_EX_AC_AR)
				lun->pr_res_idx = residx;

			/* send msg to other side */
			persis_io.hdr.nexus = ctsio->io_hdr.nexus;
			persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
			persis_io.pr.pr_info.action = CTL_PR_PREEMPT;
			persis_io.pr.pr_info.residx = lun->pr_res_idx;
			persis_io.pr.pr_info.res_type = type;
			memcpy(persis_io.pr.pr_info.sa_res_key,
			       param->serv_act_res_key,
			       sizeof(param->serv_act_res_key));
			if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL,
			     &persis_io, sizeof(persis_io), 0)) >
			     CTL_HA_STATUS_SUCCESS) {
				printf("CTL:Persis Out error returned "
				       "from ctl_ha_msg_send %d\n",
				       isc_retval);
			}
		} else {
			/* not all registrants */
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			ctl_set_invalid_field(ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 0,
					      /*field*/ 8,
					      /*bit_valid*/ 0,
					      /*bit*/ 0);
			ctl_done((union ctl_io *)ctsio);
			return (1);
		}
	} else if (lun->pr_res_idx == CTL_PR_ALL_REGISTRANTS
		|| !(lun->flags & CTL_LUN_PR_RESERVED)) {
		int found = 0;

		if (res_key == sa_res_key) {
			/* special case */
			/*
			 * The spec implies this is not good but doesn't
			 * say what to do. There are two choices either
			 * generate a res conflict or check condition
			 * with illegal field in parameter data. Since
			 * that is what is done when the sa_res_key is
			 * zero I'll take that approach since this has
			 * to do with the sa_res_key.
			 */
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			ctl_set_invalid_field(ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 0,
					      /*field*/ 8,
					      /*bit_valid*/ 0,
					      /*bit*/ 0);
			ctl_done((union ctl_io *)ctsio);
			return (1);
		}

		for (i=0; i < 2*CTL_MAX_INITIATORS; i++) {
			if (ctl_get_prkey(lun, i) != sa_res_key)
				continue;

			found = 1;
			ctl_clr_prkey(lun, i);
			lun->pr_key_count--;
			ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
		}
		if (!found) {
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			ctl_set_reservation_conflict(ctsio);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
		/* send msg to other side */
		persis_io.hdr.nexus = ctsio->io_hdr.nexus;
		persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
		persis_io.pr.pr_info.action = CTL_PR_PREEMPT;
		persis_io.pr.pr_info.residx = lun->pr_res_idx;
		persis_io.pr.pr_info.res_type = type;
		memcpy(persis_io.pr.pr_info.sa_res_key,
		       param->serv_act_res_key,
		       sizeof(param->serv_act_res_key));
		if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL,
		     &persis_io, sizeof(persis_io), 0)) >
		     CTL_HA_STATUS_SUCCESS) {
			printf("CTL:Persis Out error returned from "
			       "ctl_ha_msg_send %d\n", isc_retval);
		}
	} else {
		/* Reserved but not all registrants */
		/* sa_res_key is res holder */
		if (sa_res_key == ctl_get_prkey(lun, lun->pr_res_idx)) {
			/* validate scope and type */
			if ((cdb->scope_type & SPR_SCOPE_MASK) !=
			     SPR_LU_SCOPE) {
				mtx_unlock(&lun->lun_lock);
				ctl_set_invalid_field(/*ctsio*/ ctsio,
						      /*sks_valid*/ 1,
						      /*command*/ 1,
						      /*field*/ 2,
						      /*bit_valid*/ 1,
						      /*bit*/ 4);
				ctl_done((union ctl_io *)ctsio);
				return (1);
			}

			if (type>8 || type==2 || type==4 || type==0) {
				mtx_unlock(&lun->lun_lock);
				ctl_set_invalid_field(/*ctsio*/ ctsio,
						      /*sks_valid*/ 1,
						      /*command*/ 1,
						      /*field*/ 2,
						      /*bit_valid*/ 1,
						      /*bit*/ 0);
				ctl_done((union ctl_io *)ctsio);
				return (1);
			}

			/*
			 * Do the following:
			 * if sa_res_key != res_key remove all
			 * registrants w/sa_res_key and generate UA
			 * for these registrants(Registrations
			 * Preempted) if it wasn't an exclusive
			 * reservation generate UA(Reservations
			 * Preempted) for all other registered nexuses
			 * if the type has changed. Establish the new
			 * reservation and holder. If res_key and
			 * sa_res_key are the same do the above
			 * except don't unregister the res holder.
			 */

			for(i=0; i < 2*CTL_MAX_INITIATORS; i++) {
				if (i == residx || ctl_get_prkey(lun, i) == 0)
					continue;

				if (sa_res_key == ctl_get_prkey(lun, i)) {
					ctl_clr_prkey(lun, i);
					lun->pr_key_count--;
					ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
				} else if (type != lun->res_type
					&& (lun->res_type == SPR_TYPE_WR_EX_RO
					 || lun->res_type ==SPR_TYPE_EX_AC_RO)){
					ctl_est_res_ua(lun, i, CTL_UA_RES_RELEASE);
				}
			}
			lun->res_type = type;
			if (lun->res_type != SPR_TYPE_WR_EX_AR
			 && lun->res_type != SPR_TYPE_EX_AC_AR)
				lun->pr_res_idx = residx;
			else
				lun->pr_res_idx = CTL_PR_ALL_REGISTRANTS;

			persis_io.hdr.nexus = ctsio->io_hdr.nexus;
			persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
			persis_io.pr.pr_info.action = CTL_PR_PREEMPT;
			persis_io.pr.pr_info.residx = lun->pr_res_idx;
			persis_io.pr.pr_info.res_type = type;
			memcpy(persis_io.pr.pr_info.sa_res_key,
			       param->serv_act_res_key,
			       sizeof(param->serv_act_res_key));
			if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL,
			     &persis_io, sizeof(persis_io), 0)) >
			     CTL_HA_STATUS_SUCCESS) {
				printf("CTL:Persis Out error returned "
				       "from ctl_ha_msg_send %d\n",
				       isc_retval);
			}
		} else {
			/*
			 * sa_res_key is not the res holder just
			 * remove registrants
			 */
			int found=0;

			for (i=0; i < 2*CTL_MAX_INITIATORS; i++) {
				if (sa_res_key != ctl_get_prkey(lun, i))
					continue;

				found = 1;
				ctl_clr_prkey(lun, i);
				lun->pr_key_count--;
				ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
			}

			if (!found) {
				mtx_unlock(&lun->lun_lock);
				free(ctsio->kern_data_ptr, M_CTL);
				ctl_set_reservation_conflict(ctsio);
				ctl_done((union ctl_io *)ctsio);
		        	return (1);
			}
			persis_io.hdr.nexus = ctsio->io_hdr.nexus;
			persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
			persis_io.pr.pr_info.action = CTL_PR_PREEMPT;
			persis_io.pr.pr_info.residx = lun->pr_res_idx;
			persis_io.pr.pr_info.res_type = type;
			memcpy(persis_io.pr.pr_info.sa_res_key,
			       param->serv_act_res_key,
			       sizeof(param->serv_act_res_key));
			if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL,
			     &persis_io, sizeof(persis_io), 0)) >
			     CTL_HA_STATUS_SUCCESS) {
				printf("CTL:Persis Out error returned "
				       "from ctl_ha_msg_send %d\n",
				isc_retval);
			}
		}
	}

	lun->PRGeneration++;
	mtx_unlock(&lun->lun_lock);

	return (retval);
}

static void
ctl_pro_preempt_other(struct ctl_lun *lun, union ctl_ha_msg *msg)
{
	uint64_t sa_res_key;
	int i;

	sa_res_key = scsi_8btou64(msg->pr.pr_info.sa_res_key);

	if (lun->pr_res_idx == CTL_PR_ALL_REGISTRANTS
	 || lun->pr_res_idx == CTL_PR_NO_RESERVATION
	 || sa_res_key != ctl_get_prkey(lun, lun->pr_res_idx)) {
		if (sa_res_key == 0) {
			/*
			 * Unregister everybody else and build UA for
			 * them
			 */
			for(i=0; i < 2*CTL_MAX_INITIATORS; i++) {
				if (i == msg->pr.pr_info.residx ||
				    ctl_get_prkey(lun, i) == 0)
					continue;

				ctl_clr_prkey(lun, i);
				ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
			}

			lun->pr_key_count = 1;
			lun->res_type = msg->pr.pr_info.res_type;
			if (lun->res_type != SPR_TYPE_WR_EX_AR
			 && lun->res_type != SPR_TYPE_EX_AC_AR)
				lun->pr_res_idx = msg->pr.pr_info.residx;
		} else {
		        for (i=0; i < 2*CTL_MAX_INITIATORS; i++) {
				if (sa_res_key == ctl_get_prkey(lun, i))
					continue;

				ctl_clr_prkey(lun, i);
				lun->pr_key_count--;
				ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
			}
		}
	} else {
		for (i=0; i < 2*CTL_MAX_INITIATORS; i++) {
			if (i == msg->pr.pr_info.residx ||
			    ctl_get_prkey(lun, i) == 0)
				continue;

			if (sa_res_key == ctl_get_prkey(lun, i)) {
				ctl_clr_prkey(lun, i);
				lun->pr_key_count--;
				ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
			} else if (msg->pr.pr_info.res_type != lun->res_type
				&& (lun->res_type == SPR_TYPE_WR_EX_RO
				 || lun->res_type == SPR_TYPE_EX_AC_RO)) {
				ctl_est_res_ua(lun, i, CTL_UA_RES_RELEASE);
			}
		}
		lun->res_type = msg->pr.pr_info.res_type;
		if (lun->res_type != SPR_TYPE_WR_EX_AR
		 && lun->res_type != SPR_TYPE_EX_AC_AR)
			lun->pr_res_idx = msg->pr.pr_info.residx;
		else
			lun->pr_res_idx = CTL_PR_ALL_REGISTRANTS;
	}
	lun->PRGeneration++;

}


int
ctl_persistent_reserve_out(struct ctl_scsiio *ctsio)
{
	int retval;
	int isc_retval;
	u_int32_t param_len;
	struct scsi_per_res_out *cdb;
	struct ctl_lun *lun;
	struct scsi_per_res_out_parms* param;
	struct ctl_softc *softc;
	uint32_t residx;
	uint64_t res_key, sa_res_key, key;
	uint8_t type;
	union ctl_ha_msg persis_io;
	int    i;

	CTL_DEBUG_PRINT(("ctl_persistent_reserve_out\n"));

	retval = CTL_RETVAL_COMPLETE;

	cdb = (struct scsi_per_res_out *)ctsio->cdb;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	softc = lun->ctl_softc;

	/*
	 * We only support whole-LUN scope.  The scope & type are ignored for
	 * register, register and ignore existing key and clear.
	 * We sometimes ignore scope and type on preempts too!!
	 * Verify reservation type here as well.
	 */
	type = cdb->scope_type & SPR_TYPE_MASK;
	if ((cdb->action == SPRO_RESERVE)
	 || (cdb->action == SPRO_RELEASE)) {
		if ((cdb->scope_type & SPR_SCOPE_MASK) != SPR_LU_SCOPE) {
			ctl_set_invalid_field(/*ctsio*/ ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 1,
					      /*field*/ 2,
					      /*bit_valid*/ 1,
					      /*bit*/ 4);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}

		if (type>8 || type==2 || type==4 || type==0) {
			ctl_set_invalid_field(/*ctsio*/ ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 1,
					      /*field*/ 2,
					      /*bit_valid*/ 1,
					      /*bit*/ 0);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
	}

	param_len = scsi_4btoul(cdb->length);

	if ((ctsio->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0) {
		ctsio->kern_data_ptr = malloc(param_len, M_CTL, M_WAITOK);
		ctsio->kern_data_len = param_len;
		ctsio->kern_total_len = param_len;
		ctsio->kern_data_resid = 0;
		ctsio->kern_rel_offset = 0;
		ctsio->kern_sg_entries = 0;
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
	}

	param = (struct scsi_per_res_out_parms *)ctsio->kern_data_ptr;

	residx = ctl_get_resindex(&ctsio->io_hdr.nexus);
	res_key = scsi_8btou64(param->res_key.key);
	sa_res_key = scsi_8btou64(param->serv_act_res_key);

	/*
	 * Validate the reservation key here except for SPRO_REG_IGNO
	 * This must be done for all other service actions
	 */
	if ((cdb->action & SPRO_ACTION_MASK) != SPRO_REG_IGNO) {
		mtx_lock(&lun->lun_lock);
		if ((key = ctl_get_prkey(lun, residx)) != 0) {
			if (res_key != key) {
				/*
				 * The current key passed in doesn't match
				 * the one the initiator previously
				 * registered.
				 */
				mtx_unlock(&lun->lun_lock);
				free(ctsio->kern_data_ptr, M_CTL);
				ctl_set_reservation_conflict(ctsio);
				ctl_done((union ctl_io *)ctsio);
				return (CTL_RETVAL_COMPLETE);
			}
		} else if ((cdb->action & SPRO_ACTION_MASK) != SPRO_REGISTER) {
			/*
			 * We are not registered
			 */
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			ctl_set_reservation_conflict(ctsio);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		} else if (res_key != 0) {
			/*
			 * We are not registered and trying to register but
			 * the register key isn't zero.
			 */
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			ctl_set_reservation_conflict(ctsio);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
		mtx_unlock(&lun->lun_lock);
	}

	switch (cdb->action & SPRO_ACTION_MASK) {
	case SPRO_REGISTER:
	case SPRO_REG_IGNO: {

#if 0
		printf("Registration received\n");
#endif

		/*
		 * We don't support any of these options, as we report in
		 * the read capabilities request (see
		 * ctl_persistent_reserve_in(), above).
		 */
		if ((param->flags & SPR_SPEC_I_PT)
		 || (param->flags & SPR_ALL_TG_PT)
		 || (param->flags & SPR_APTPL)) {
			int bit_ptr;

			if (param->flags & SPR_APTPL)
				bit_ptr = 0;
			else if (param->flags & SPR_ALL_TG_PT)
				bit_ptr = 2;
			else /* SPR_SPEC_I_PT */
				bit_ptr = 3;

			free(ctsio->kern_data_ptr, M_CTL);
			ctl_set_invalid_field(ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 0,
					      /*field*/ 20,
					      /*bit_valid*/ 1,
					      /*bit*/ bit_ptr);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}

		mtx_lock(&lun->lun_lock);

		/*
		 * The initiator wants to clear the
		 * key/unregister.
		 */
		if (sa_res_key == 0) {
			if ((res_key == 0
			  && (cdb->action & SPRO_ACTION_MASK) == SPRO_REGISTER)
			 || ((cdb->action & SPRO_ACTION_MASK) == SPRO_REG_IGNO
			  && ctl_get_prkey(lun, residx) == 0)) {
				mtx_unlock(&lun->lun_lock);
				goto done;
			}

			ctl_clr_prkey(lun, residx);
			lun->pr_key_count--;

			if (residx == lun->pr_res_idx) {
				lun->flags &= ~CTL_LUN_PR_RESERVED;
				lun->pr_res_idx = CTL_PR_NO_RESERVATION;

				if ((lun->res_type == SPR_TYPE_WR_EX_RO
				  || lun->res_type == SPR_TYPE_EX_AC_RO)
				 && lun->pr_key_count) {
					/*
					 * If the reservation is a registrants
					 * only type we need to generate a UA
					 * for other registered inits.  The
					 * sense code should be RESERVATIONS
					 * RELEASED
					 */

					for (i = 0; i < CTL_MAX_INITIATORS;i++){
						if (ctl_get_prkey(lun, i +
						    softc->persis_offset) == 0)
							continue;
						ctl_est_ua(lun, i,
						    CTL_UA_RES_RELEASE);
					}
				}
				lun->res_type = 0;
			} else if (lun->pr_res_idx == CTL_PR_ALL_REGISTRANTS) {
				if (lun->pr_key_count==0) {
					lun->flags &= ~CTL_LUN_PR_RESERVED;
					lun->res_type = 0;
					lun->pr_res_idx = CTL_PR_NO_RESERVATION;
				}
			}
			persis_io.hdr.nexus = ctsio->io_hdr.nexus;
			persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
			persis_io.pr.pr_info.action = CTL_PR_UNREG_KEY;
			persis_io.pr.pr_info.residx = residx;
			if ((isc_retval = ctl_ha_msg_send(CTL_HA_CHAN_CTL,
			     &persis_io, sizeof(persis_io), 0 )) >
			     CTL_HA_STATUS_SUCCESS) {
				printf("CTL:Persis Out error returned from "
				       "ctl_ha_msg_send %d\n", isc_retval);
			}
		} else /* sa_res_key != 0 */ {

			/*
			 * If we aren't registered currently then increment
			 * the key count and set the registered flag.
			 */
			ctl_alloc_prkey(lun, residx);
			if (ctl_get_prkey(lun, residx) == 0)
				lun->pr_key_count++;
			ctl_set_prkey(lun, residx, sa_res_key);

			persis_io.hdr.nexus = ctsio->io_hdr.nexus;
			persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
			persis_io.pr.pr_info.action = CTL_PR_REG_KEY;
			persis_io.pr.pr_info.residx = residx;
			memcpy(persis_io.pr.pr_info.sa_res_key,
			       param->serv_act_res_key,
			       sizeof(param->serv_act_res_key));
			if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL,
			     &persis_io, sizeof(persis_io), 0)) >
			     CTL_HA_STATUS_SUCCESS) {
				printf("CTL:Persis Out error returned from "
				       "ctl_ha_msg_send %d\n", isc_retval);
			}
		}
		lun->PRGeneration++;
		mtx_unlock(&lun->lun_lock);

		break;
	}
	case SPRO_RESERVE:
#if 0
                printf("Reserve executed type %d\n", type);
#endif
		mtx_lock(&lun->lun_lock);
		if (lun->flags & CTL_LUN_PR_RESERVED) {
			/*
			 * if this isn't the reservation holder and it's
			 * not a "all registrants" type or if the type is
			 * different then we have a conflict
			 */
			if ((lun->pr_res_idx != residx
			  && lun->pr_res_idx != CTL_PR_ALL_REGISTRANTS)
			 || lun->res_type != type) {
				mtx_unlock(&lun->lun_lock);
				free(ctsio->kern_data_ptr, M_CTL);
				ctl_set_reservation_conflict(ctsio);
				ctl_done((union ctl_io *)ctsio);
				return (CTL_RETVAL_COMPLETE);
			}
			mtx_unlock(&lun->lun_lock);
		} else /* create a reservation */ {
			/*
			 * If it's not an "all registrants" type record
			 * reservation holder
			 */
			if (type != SPR_TYPE_WR_EX_AR
			 && type != SPR_TYPE_EX_AC_AR)
				lun->pr_res_idx = residx; /* Res holder */
			else
				lun->pr_res_idx = CTL_PR_ALL_REGISTRANTS;

			lun->flags |= CTL_LUN_PR_RESERVED;
			lun->res_type = type;

			mtx_unlock(&lun->lun_lock);

			/* send msg to other side */
			persis_io.hdr.nexus = ctsio->io_hdr.nexus;
			persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
			persis_io.pr.pr_info.action = CTL_PR_RESERVE;
			persis_io.pr.pr_info.residx = lun->pr_res_idx;
			persis_io.pr.pr_info.res_type = type;
			if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL,
			     &persis_io, sizeof(persis_io), 0)) >
			     CTL_HA_STATUS_SUCCESS) {
				printf("CTL:Persis Out error returned from "
				       "ctl_ha_msg_send %d\n", isc_retval);
			}
		}
		break;

	case SPRO_RELEASE:
		mtx_lock(&lun->lun_lock);
		if ((lun->flags & CTL_LUN_PR_RESERVED) == 0) {
			/* No reservation exists return good status */
			mtx_unlock(&lun->lun_lock);
			goto done;
		}
		/*
		 * Is this nexus a reservation holder?
		 */
		if (lun->pr_res_idx != residx
		 && lun->pr_res_idx != CTL_PR_ALL_REGISTRANTS) {
			/*
			 * not a res holder return good status but
			 * do nothing
			 */
			mtx_unlock(&lun->lun_lock);
			goto done;
		}

		if (lun->res_type != type) {
			mtx_unlock(&lun->lun_lock);
			free(ctsio->kern_data_ptr, M_CTL);
			ctl_set_illegal_pr_release(ctsio);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}

		/* okay to release */
		lun->flags &= ~CTL_LUN_PR_RESERVED;
		lun->pr_res_idx = CTL_PR_NO_RESERVATION;
		lun->res_type = 0;

		/*
		 * if this isn't an exclusive access
		 * res generate UA for all other
		 * registrants.
		 */
		if (type != SPR_TYPE_EX_AC
		 && type != SPR_TYPE_WR_EX) {
			for (i = 0; i < CTL_MAX_INITIATORS; i++) {
				if (i == residx ||
				    ctl_get_prkey(lun,
				     i + softc->persis_offset) == 0)
					continue;
				ctl_est_ua(lun, i, CTL_UA_RES_RELEASE);
			}
		}
		mtx_unlock(&lun->lun_lock);
		/* Send msg to other side */
		persis_io.hdr.nexus = ctsio->io_hdr.nexus;
		persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
		persis_io.pr.pr_info.action = CTL_PR_RELEASE;
		if ((isc_retval=ctl_ha_msg_send( CTL_HA_CHAN_CTL, &persis_io,
		     sizeof(persis_io), 0)) > CTL_HA_STATUS_SUCCESS) {
			printf("CTL:Persis Out error returned from "
			       "ctl_ha_msg_send %d\n", isc_retval);
		}
		break;

	case SPRO_CLEAR:
		/* send msg to other side */

		mtx_lock(&lun->lun_lock);
		lun->flags &= ~CTL_LUN_PR_RESERVED;
		lun->res_type = 0;
		lun->pr_key_count = 0;
		lun->pr_res_idx = CTL_PR_NO_RESERVATION;

		ctl_clr_prkey(lun, residx);
		for (i=0; i < 2*CTL_MAX_INITIATORS; i++)
			if (ctl_get_prkey(lun, i) != 0) {
				ctl_clr_prkey(lun, i);
				ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
			}
		lun->PRGeneration++;
		mtx_unlock(&lun->lun_lock);
		persis_io.hdr.nexus = ctsio->io_hdr.nexus;
		persis_io.hdr.msg_type = CTL_MSG_PERS_ACTION;
		persis_io.pr.pr_info.action = CTL_PR_CLEAR;
		if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL, &persis_io,
		     sizeof(persis_io), 0)) > CTL_HA_STATUS_SUCCESS) {
			printf("CTL:Persis Out error returned from "
			       "ctl_ha_msg_send %d\n", isc_retval);
		}
		break;

	case SPRO_PREEMPT:
	case SPRO_PRE_ABO: {
		int nretval;

		nretval = ctl_pro_preempt(softc, lun, res_key, sa_res_key, type,
					  residx, ctsio, cdb, param);
		if (nretval != 0)
			return (CTL_RETVAL_COMPLETE);
		break;
	}
	default:
		panic("Invalid PR type %x", cdb->action);
	}

done:
	free(ctsio->kern_data_ptr, M_CTL);
	ctl_set_success(ctsio);
	ctl_done((union ctl_io *)ctsio);

	return (retval);
}

/*
 * This routine is for handling a message from the other SC pertaining to
 * persistent reserve out. All the error checking will have been done
 * so only perorming the action need be done here to keep the two
 * in sync.
 */
static void
ctl_hndl_per_res_out_on_other_sc(union ctl_ha_msg *msg)
{
	struct ctl_lun *lun;
	struct ctl_softc *softc;
	int i;
	uint32_t targ_lun;

	softc = control_softc;

	targ_lun = msg->hdr.nexus.targ_mapped_lun;
	lun = softc->ctl_luns[targ_lun];
	mtx_lock(&lun->lun_lock);
	switch(msg->pr.pr_info.action) {
	case CTL_PR_REG_KEY:
		ctl_alloc_prkey(lun, msg->pr.pr_info.residx);
		if (ctl_get_prkey(lun, msg->pr.pr_info.residx) == 0)
			lun->pr_key_count++;
		ctl_set_prkey(lun, msg->pr.pr_info.residx,
		    scsi_8btou64(msg->pr.pr_info.sa_res_key));
		lun->PRGeneration++;
		break;

	case CTL_PR_UNREG_KEY:
		ctl_clr_prkey(lun, msg->pr.pr_info.residx);
		lun->pr_key_count--;

		/* XXX Need to see if the reservation has been released */
		/* if so do we need to generate UA? */
		if (msg->pr.pr_info.residx == lun->pr_res_idx) {
			lun->flags &= ~CTL_LUN_PR_RESERVED;
			lun->pr_res_idx = CTL_PR_NO_RESERVATION;

			if ((lun->res_type == SPR_TYPE_WR_EX_RO
			  || lun->res_type == SPR_TYPE_EX_AC_RO)
			 && lun->pr_key_count) {
				/*
				 * If the reservation is a registrants
				 * only type we need to generate a UA
				 * for other registered inits.  The
				 * sense code should be RESERVATIONS
				 * RELEASED
				 */

				for (i = 0; i < CTL_MAX_INITIATORS; i++) {
					if (ctl_get_prkey(lun, i +
					    softc->persis_offset) == 0)
						continue;

					ctl_est_ua(lun, i, CTL_UA_RES_RELEASE);
				}
			}
			lun->res_type = 0;
		} else if (lun->pr_res_idx == CTL_PR_ALL_REGISTRANTS) {
			if (lun->pr_key_count==0) {
				lun->flags &= ~CTL_LUN_PR_RESERVED;
				lun->res_type = 0;
				lun->pr_res_idx = CTL_PR_NO_RESERVATION;
			}
		}
		lun->PRGeneration++;
		break;

	case CTL_PR_RESERVE:
		lun->flags |= CTL_LUN_PR_RESERVED;
		lun->res_type = msg->pr.pr_info.res_type;
		lun->pr_res_idx = msg->pr.pr_info.residx;

		break;

	case CTL_PR_RELEASE:
		/*
		 * if this isn't an exclusive access res generate UA for all
		 * other registrants.
		 */
		if (lun->res_type != SPR_TYPE_EX_AC
		 && lun->res_type != SPR_TYPE_WR_EX) {
			for (i = 0; i < CTL_MAX_INITIATORS; i++)
				if (ctl_get_prkey(lun, i + softc->persis_offset) != 0)
					ctl_est_ua(lun, i, CTL_UA_RES_RELEASE);
		}

		lun->flags &= ~CTL_LUN_PR_RESERVED;
		lun->pr_res_idx = CTL_PR_NO_RESERVATION;
		lun->res_type = 0;
		break;

	case CTL_PR_PREEMPT:
		ctl_pro_preempt_other(lun, msg);
		break;
	case CTL_PR_CLEAR:
		lun->flags &= ~CTL_LUN_PR_RESERVED;
		lun->res_type = 0;
		lun->pr_key_count = 0;
		lun->pr_res_idx = CTL_PR_NO_RESERVATION;

		for (i=0; i < 2*CTL_MAX_INITIATORS; i++) {
			if (ctl_get_prkey(lun, i) == 0)
				continue;
			ctl_clr_prkey(lun, i);
			ctl_est_res_ua(lun, i, CTL_UA_REG_PREEMPT);
		}
		lun->PRGeneration++;
		break;
	}

	mtx_unlock(&lun->lun_lock);
}

int
ctl_read_write(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct ctl_lba_len_flags *lbalen;
	uint64_t lba;
	uint32_t num_blocks;
	int flags, retval;
	int isread;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	CTL_DEBUG_PRINT(("ctl_read_write: command: %#x\n", ctsio->cdb[0]));

	flags = 0;
	retval = CTL_RETVAL_COMPLETE;

	isread = ctsio->cdb[0] == READ_6  || ctsio->cdb[0] == READ_10
	      || ctsio->cdb[0] == READ_12 || ctsio->cdb[0] == READ_16;
	switch (ctsio->cdb[0]) {
	case READ_6:
	case WRITE_6: {
		struct scsi_rw_6 *cdb;

		cdb = (struct scsi_rw_6 *)ctsio->cdb;

		lba = scsi_3btoul(cdb->addr);
		/* only 5 bits are valid in the most significant address byte */
		lba &= 0x1fffff;
		num_blocks = cdb->length;
		/*
		 * This is correct according to SBC-2.
		 */
		if (num_blocks == 0)
			num_blocks = 256;
		break;
	}
	case READ_10:
	case WRITE_10: {
		struct scsi_rw_10 *cdb;

		cdb = (struct scsi_rw_10 *)ctsio->cdb;
		if (cdb->byte2 & SRW10_FUA)
			flags |= CTL_LLF_FUA;
		if (cdb->byte2 & SRW10_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_4btoul(cdb->addr);
		num_blocks = scsi_2btoul(cdb->length);
		break;
	}
	case WRITE_VERIFY_10: {
		struct scsi_write_verify_10 *cdb;

		cdb = (struct scsi_write_verify_10 *)ctsio->cdb;
		flags |= CTL_LLF_FUA;
		if (cdb->byte2 & SWV_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_4btoul(cdb->addr);
		num_blocks = scsi_2btoul(cdb->length);
		break;
	}
	case READ_12:
	case WRITE_12: {
		struct scsi_rw_12 *cdb;

		cdb = (struct scsi_rw_12 *)ctsio->cdb;
		if (cdb->byte2 & SRW12_FUA)
			flags |= CTL_LLF_FUA;
		if (cdb->byte2 & SRW12_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_4btoul(cdb->addr);
		num_blocks = scsi_4btoul(cdb->length);
		break;
	}
	case WRITE_VERIFY_12: {
		struct scsi_write_verify_12 *cdb;

		cdb = (struct scsi_write_verify_12 *)ctsio->cdb;
		flags |= CTL_LLF_FUA;
		if (cdb->byte2 & SWV_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_4btoul(cdb->addr);
		num_blocks = scsi_4btoul(cdb->length);
		break;
	}
	case READ_16:
	case WRITE_16: {
		struct scsi_rw_16 *cdb;

		cdb = (struct scsi_rw_16 *)ctsio->cdb;
		if (cdb->byte2 & SRW12_FUA)
			flags |= CTL_LLF_FUA;
		if (cdb->byte2 & SRW12_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_8btou64(cdb->addr);
		num_blocks = scsi_4btoul(cdb->length);
		break;
	}
	case WRITE_ATOMIC_16: {
		struct scsi_rw_16 *cdb;

		if (lun->be_lun->atomicblock == 0) {
			ctl_set_invalid_opcode(ctsio);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}

		cdb = (struct scsi_rw_16 *)ctsio->cdb;
		if (cdb->byte2 & SRW12_FUA)
			flags |= CTL_LLF_FUA;
		if (cdb->byte2 & SRW12_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_8btou64(cdb->addr);
		num_blocks = scsi_4btoul(cdb->length);
		if (num_blocks > lun->be_lun->atomicblock) {
			ctl_set_invalid_field(ctsio, /*sks_valid*/ 1,
			    /*command*/ 1, /*field*/ 12, /*bit_valid*/ 0,
			    /*bit*/ 0);
			ctl_done((union ctl_io *)ctsio);
			return (CTL_RETVAL_COMPLETE);
		}
		break;
	}
	case WRITE_VERIFY_16: {
		struct scsi_write_verify_16 *cdb;

		cdb = (struct scsi_write_verify_16 *)ctsio->cdb;
		flags |= CTL_LLF_FUA;
		if (cdb->byte2 & SWV_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_8btou64(cdb->addr);
		num_blocks = scsi_4btoul(cdb->length);
		break;
	}
	default:
		/*
		 * We got a command we don't support.  This shouldn't
		 * happen, commands should be filtered out above us.
		 */
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
		break; /* NOTREACHED */
	}

	/*
	 * The first check is to make sure we're in bounds, the second
	 * check is to catch wrap-around problems.  If the lba + num blocks
	 * is less than the lba, then we've wrapped around and the block
	 * range is invalid anyway.
	 */
	if (((lba + num_blocks) > (lun->be_lun->maxlba + 1))
	 || ((lba + num_blocks) < lba)) {
		ctl_set_lba_out_of_range(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * According to SBC-3, a transfer length of 0 is not an error.
	 * Note that this cannot happen with WRITE(6) or READ(6), since 0
	 * translates to 256 blocks for those commands.
	 */
	if (num_blocks == 0) {
		ctl_set_success(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/* Set FUA and/or DPO if caches are disabled. */
	if (isread) {
		if ((lun->mode_pages.caching_page[CTL_PAGE_CURRENT].flags1 &
		    SCP_RCD) != 0)
			flags |= CTL_LLF_FUA | CTL_LLF_DPO;
	} else {
		if ((lun->mode_pages.caching_page[CTL_PAGE_CURRENT].flags1 &
		    SCP_WCE) == 0)
			flags |= CTL_LLF_FUA;
	}

	lbalen = (struct ctl_lba_len_flags *)
	    &ctsio->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	lbalen->lba = lba;
	lbalen->len = num_blocks;
	lbalen->flags = (isread ? CTL_LLF_READ : CTL_LLF_WRITE) | flags;

	ctsio->kern_total_len = num_blocks * lun->be_lun->blocksize;
	ctsio->kern_rel_offset = 0;

	CTL_DEBUG_PRINT(("ctl_read_write: calling data_submit()\n"));

	retval = lun->backend->data_submit((union ctl_io *)ctsio);

	return (retval);
}

static int
ctl_cnw_cont(union ctl_io *io)
{
	struct ctl_scsiio *ctsio;
	struct ctl_lun *lun;
	struct ctl_lba_len_flags *lbalen;
	int retval;

	ctsio = &io->scsiio;
	ctsio->io_hdr.status = CTL_STATUS_NONE;
	ctsio->io_hdr.flags &= ~CTL_FLAG_IO_CONT;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	lbalen = (struct ctl_lba_len_flags *)
	    &ctsio->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	lbalen->flags &= ~CTL_LLF_COMPARE;
	lbalen->flags |= CTL_LLF_WRITE;

	CTL_DEBUG_PRINT(("ctl_cnw_cont: calling data_submit()\n"));
	retval = lun->backend->data_submit((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_cnw(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct ctl_lba_len_flags *lbalen;
	uint64_t lba;
	uint32_t num_blocks;
	int flags, retval;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	CTL_DEBUG_PRINT(("ctl_cnw: command: %#x\n", ctsio->cdb[0]));

	flags = 0;
	retval = CTL_RETVAL_COMPLETE;

	switch (ctsio->cdb[0]) {
	case COMPARE_AND_WRITE: {
		struct scsi_compare_and_write *cdb;

		cdb = (struct scsi_compare_and_write *)ctsio->cdb;
		if (cdb->byte2 & SRW10_FUA)
			flags |= CTL_LLF_FUA;
		if (cdb->byte2 & SRW10_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_8btou64(cdb->addr);
		num_blocks = cdb->length;
		break;
	}
	default:
		/*
		 * We got a command we don't support.  This shouldn't
		 * happen, commands should be filtered out above us.
		 */
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);

		return (CTL_RETVAL_COMPLETE);
		break; /* NOTREACHED */
	}

	/*
	 * The first check is to make sure we're in bounds, the second
	 * check is to catch wrap-around problems.  If the lba + num blocks
	 * is less than the lba, then we've wrapped around and the block
	 * range is invalid anyway.
	 */
	if (((lba + num_blocks) > (lun->be_lun->maxlba + 1))
	 || ((lba + num_blocks) < lba)) {
		ctl_set_lba_out_of_range(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * According to SBC-3, a transfer length of 0 is not an error.
	 */
	if (num_blocks == 0) {
		ctl_set_success(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/* Set FUA if write cache is disabled. */
	if ((lun->mode_pages.caching_page[CTL_PAGE_CURRENT].flags1 &
	    SCP_WCE) == 0)
		flags |= CTL_LLF_FUA;

	ctsio->kern_total_len = 2 * num_blocks * lun->be_lun->blocksize;
	ctsio->kern_rel_offset = 0;

	/*
	 * Set the IO_CONT flag, so that if this I/O gets passed to
	 * ctl_data_submit_done(), it'll get passed back to
	 * ctl_ctl_cnw_cont() for further processing.
	 */
	ctsio->io_hdr.flags |= CTL_FLAG_IO_CONT;
	ctsio->io_cont = ctl_cnw_cont;

	lbalen = (struct ctl_lba_len_flags *)
	    &ctsio->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	lbalen->lba = lba;
	lbalen->len = num_blocks;
	lbalen->flags = CTL_LLF_COMPARE | flags;

	CTL_DEBUG_PRINT(("ctl_cnw: calling data_submit()\n"));
	retval = lun->backend->data_submit((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_verify(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct ctl_lba_len_flags *lbalen;
	uint64_t lba;
	uint32_t num_blocks;
	int bytchk, flags;
	int retval;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	CTL_DEBUG_PRINT(("ctl_verify: command: %#x\n", ctsio->cdb[0]));

	bytchk = 0;
	flags = CTL_LLF_FUA;
	retval = CTL_RETVAL_COMPLETE;

	switch (ctsio->cdb[0]) {
	case VERIFY_10: {
		struct scsi_verify_10 *cdb;

		cdb = (struct scsi_verify_10 *)ctsio->cdb;
		if (cdb->byte2 & SVFY_BYTCHK)
			bytchk = 1;
		if (cdb->byte2 & SVFY_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_4btoul(cdb->addr);
		num_blocks = scsi_2btoul(cdb->length);
		break;
	}
	case VERIFY_12: {
		struct scsi_verify_12 *cdb;

		cdb = (struct scsi_verify_12 *)ctsio->cdb;
		if (cdb->byte2 & SVFY_BYTCHK)
			bytchk = 1;
		if (cdb->byte2 & SVFY_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_4btoul(cdb->addr);
		num_blocks = scsi_4btoul(cdb->length);
		break;
	}
	case VERIFY_16: {
		struct scsi_rw_16 *cdb;

		cdb = (struct scsi_rw_16 *)ctsio->cdb;
		if (cdb->byte2 & SVFY_BYTCHK)
			bytchk = 1;
		if (cdb->byte2 & SVFY_DPO)
			flags |= CTL_LLF_DPO;
		lba = scsi_8btou64(cdb->addr);
		num_blocks = scsi_4btoul(cdb->length);
		break;
	}
	default:
		/*
		 * We got a command we don't support.  This shouldn't
		 * happen, commands should be filtered out above us.
		 */
		ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * The first check is to make sure we're in bounds, the second
	 * check is to catch wrap-around problems.  If the lba + num blocks
	 * is less than the lba, then we've wrapped around and the block
	 * range is invalid anyway.
	 */
	if (((lba + num_blocks) > (lun->be_lun->maxlba + 1))
	 || ((lba + num_blocks) < lba)) {
		ctl_set_lba_out_of_range(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	/*
	 * According to SBC-3, a transfer length of 0 is not an error.
	 */
	if (num_blocks == 0) {
		ctl_set_success(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	lbalen = (struct ctl_lba_len_flags *)
	    &ctsio->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	lbalen->lba = lba;
	lbalen->len = num_blocks;
	if (bytchk) {
		lbalen->flags = CTL_LLF_COMPARE | flags;
		ctsio->kern_total_len = num_blocks * lun->be_lun->blocksize;
	} else {
		lbalen->flags = CTL_LLF_VERIFY | flags;
		ctsio->kern_total_len = 0;
	}
	ctsio->kern_rel_offset = 0;

	CTL_DEBUG_PRINT(("ctl_verify: calling data_submit()\n"));
	retval = lun->backend->data_submit((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_report_luns(struct ctl_scsiio *ctsio)
{
	struct ctl_softc *softc = control_softc;
	struct scsi_report_luns *cdb;
	struct scsi_report_luns_data *lun_data;
	struct ctl_lun *lun, *request_lun;
	struct ctl_port *port;
	int num_luns, retval;
	uint32_t alloc_len, lun_datalen;
	int num_filled, well_known;
	uint32_t initidx, targ_lun_id, lun_id;

	retval = CTL_RETVAL_COMPLETE;
	well_known = 0;

	cdb = (struct scsi_report_luns *)ctsio->cdb;
	port = ctl_io_port(&ctsio->io_hdr);

	CTL_DEBUG_PRINT(("ctl_report_luns\n"));

	mtx_lock(&softc->ctl_lock);
	num_luns = 0;
	for (targ_lun_id = 0; targ_lun_id < CTL_MAX_LUNS; targ_lun_id++) {
		if (ctl_lun_map_from_port(port, targ_lun_id) < CTL_MAX_LUNS)
			num_luns++;
	}
	mtx_unlock(&softc->ctl_lock);

	switch (cdb->select_report) {
	case RPL_REPORT_DEFAULT:
	case RPL_REPORT_ALL:
		break;
	case RPL_REPORT_WELLKNOWN:
		well_known = 1;
		num_luns = 0;
		break;
	default:
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 2,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (retval);
		break; /* NOTREACHED */
	}

	alloc_len = scsi_4btoul(cdb->length);
	/*
	 * The initiator has to allocate at least 16 bytes for this request,
	 * so he can at least get the header and the first LUN.  Otherwise
	 * we reject the request (per SPC-3 rev 14, section 6.21).
	 */
	if (alloc_len < (sizeof(struct scsi_report_luns_data) +
	    sizeof(struct scsi_report_luns_lundata))) {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 6,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (retval);
	}

	request_lun = (struct ctl_lun *)
		ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	lun_datalen = sizeof(*lun_data) +
		(num_luns * sizeof(struct scsi_report_luns_lundata));

	ctsio->kern_data_ptr = malloc(lun_datalen, M_CTL, M_WAITOK | M_ZERO);
	lun_data = (struct scsi_report_luns_data *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	initidx = ctl_get_initindex(&ctsio->io_hdr.nexus);

	mtx_lock(&softc->ctl_lock);
	for (targ_lun_id = 0, num_filled = 0; targ_lun_id < CTL_MAX_LUNS && num_filled < num_luns; targ_lun_id++) {
		lun_id = ctl_lun_map_from_port(port, targ_lun_id);
		if (lun_id >= CTL_MAX_LUNS)
			continue;
		lun = softc->ctl_luns[lun_id];
		if (lun == NULL)
			continue;

		if (targ_lun_id <= 0xff) {
			/*
			 * Peripheral addressing method, bus number 0.
			 */
			lun_data->luns[num_filled].lundata[0] =
				RPL_LUNDATA_ATYP_PERIPH;
			lun_data->luns[num_filled].lundata[1] = targ_lun_id;
			num_filled++;
		} else if (targ_lun_id <= 0x3fff) {
			/*
			 * Flat addressing method.
			 */
			lun_data->luns[num_filled].lundata[0] =
				RPL_LUNDATA_ATYP_FLAT | (targ_lun_id >> 8);
			lun_data->luns[num_filled].lundata[1] =
				(targ_lun_id & 0xff);
			num_filled++;
		} else if (targ_lun_id <= 0xffffff) {
			/*
			 * Extended flat addressing method.
			 */
			lun_data->luns[num_filled].lundata[0] =
			    RPL_LUNDATA_ATYP_EXTLUN | 0x12;
			scsi_ulto3b(targ_lun_id,
			    &lun_data->luns[num_filled].lundata[1]);
			num_filled++;
		} else {
			printf("ctl_report_luns: bogus LUN number %jd, "
			       "skipping\n", (intmax_t)targ_lun_id);
		}
		/*
		 * According to SPC-3, rev 14 section 6.21:
		 *
		 * "The execution of a REPORT LUNS command to any valid and
		 * installed logical unit shall clear the REPORTED LUNS DATA
		 * HAS CHANGED unit attention condition for all logical
		 * units of that target with respect to the requesting
		 * initiator. A valid and installed logical unit is one
		 * having a PERIPHERAL QUALIFIER of 000b in the standard
		 * INQUIRY data (see 6.4.2)."
		 *
		 * If request_lun is NULL, the LUN this report luns command
		 * was issued to is either disabled or doesn't exist. In that
		 * case, we shouldn't clear any pending lun change unit
		 * attention.
		 */
		if (request_lun != NULL) {
			mtx_lock(&lun->lun_lock);
			ctl_clr_ua(lun, initidx, CTL_UA_RES_RELEASE);
			mtx_unlock(&lun->lun_lock);
		}
	}
	mtx_unlock(&softc->ctl_lock);

	/*
	 * It's quite possible that we've returned fewer LUNs than we allocated
	 * space for.  Trim it.
	 */
	lun_datalen = sizeof(*lun_data) +
		(num_filled * sizeof(struct scsi_report_luns_lundata));

	if (lun_datalen < alloc_len) {
		ctsio->residual = alloc_len - lun_datalen;
		ctsio->kern_data_len = lun_datalen;
		ctsio->kern_total_len = lun_datalen;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * We set this to the actual data length, regardless of how much
	 * space we actually have to return results.  If the user looks at
	 * this value, he'll know whether or not he allocated enough space
	 * and reissue the command if necessary.  We don't support well
	 * known logical units, so if the user asks for that, return none.
	 */
	scsi_ulto4b(lun_datalen - 8, lun_data->length);

	/*
	 * We can only return SCSI_STATUS_CHECK_COND when we can't satisfy
	 * this request.
	 */
	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (retval);
}

int
ctl_request_sense(struct ctl_scsiio *ctsio)
{
	struct scsi_request_sense *cdb;
	struct scsi_sense_data *sense_ptr;
	struct ctl_softc *ctl_softc;
	struct ctl_lun *lun;
	uint32_t initidx;
	int have_error;
	scsi_sense_data_type sense_format;
	ctl_ua_type ua_type;

	cdb = (struct scsi_request_sense *)ctsio->cdb;

	ctl_softc = control_softc;
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	CTL_DEBUG_PRINT(("ctl_request_sense\n"));

	/*
	 * Determine which sense format the user wants.
	 */
	if (cdb->byte2 & SRS_DESC)
		sense_format = SSD_TYPE_DESC;
	else
		sense_format = SSD_TYPE_FIXED;

	ctsio->kern_data_ptr = malloc(sizeof(*sense_ptr), M_CTL, M_WAITOK);
	sense_ptr = (struct scsi_sense_data *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	/*
	 * struct scsi_sense_data, which is currently set to 256 bytes, is
	 * larger than the largest allowed value for the length field in the
	 * REQUEST SENSE CDB, which is 252 bytes as of SPC-4.
	 */
	ctsio->residual = 0;
	ctsio->kern_data_len = cdb->length;
	ctsio->kern_total_len = cdb->length;

	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * If we don't have a LUN, we don't have any pending sense.
	 */
	if (lun == NULL)
		goto no_sense;

	have_error = 0;
	initidx = ctl_get_initindex(&ctsio->io_hdr.nexus);
	/*
	 * Check for pending sense, and then for pending unit attentions.
	 * Pending sense gets returned first, then pending unit attentions.
	 */
	mtx_lock(&lun->lun_lock);
#ifdef CTL_WITH_CA
	if (ctl_is_set(lun->have_ca, initidx)) {
		scsi_sense_data_type stored_format;

		/*
		 * Check to see which sense format was used for the stored
		 * sense data.
		 */
		stored_format = scsi_sense_type(&lun->pending_sense[initidx]);

		/*
		 * If the user requested a different sense format than the
		 * one we stored, then we need to convert it to the other
		 * format.  If we're going from descriptor to fixed format
		 * sense data, we may lose things in translation, depending
		 * on what options were used.
		 *
		 * If the stored format is SSD_TYPE_NONE (i.e. invalid),
		 * for some reason we'll just copy it out as-is.
		 */
		if ((stored_format == SSD_TYPE_FIXED)
		 && (sense_format == SSD_TYPE_DESC))
			ctl_sense_to_desc((struct scsi_sense_data_fixed *)
			    &lun->pending_sense[initidx],
			    (struct scsi_sense_data_desc *)sense_ptr);
		else if ((stored_format == SSD_TYPE_DESC)
		      && (sense_format == SSD_TYPE_FIXED))
			ctl_sense_to_fixed((struct scsi_sense_data_desc *)
			    &lun->pending_sense[initidx],
			    (struct scsi_sense_data_fixed *)sense_ptr);
		else
			memcpy(sense_ptr, &lun->pending_sense[initidx],
			       MIN(sizeof(*sense_ptr),
			       sizeof(lun->pending_sense[initidx])));

		ctl_clear_mask(lun->have_ca, initidx);
		have_error = 1;
	} else
#endif
	{
		ua_type = ctl_build_ua(lun, initidx, sense_ptr, sense_format);
		if (ua_type != CTL_UA_NONE)
			have_error = 1;
		if (ua_type == CTL_UA_LUN_CHANGE) {
			mtx_unlock(&lun->lun_lock);
			mtx_lock(&ctl_softc->ctl_lock);
			ctl_clear_ua(ctl_softc, initidx, ua_type);
			mtx_unlock(&ctl_softc->ctl_lock);
			mtx_lock(&lun->lun_lock);
		}

	}
	mtx_unlock(&lun->lun_lock);

	/*
	 * We already have a pending error, return it.
	 */
	if (have_error != 0) {
		/*
		 * We report the SCSI status as OK, since the status of the
		 * request sense command itself is OK.
		 * We report 0 for the sense length, because we aren't doing
		 * autosense in this case.  We're reporting sense as
		 * parameter data.
		 */
		ctl_set_success(ctsio);
		ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
		ctsio->be_move_done = ctl_config_move_done;
		ctl_datamove((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

no_sense:

	/*
	 * No sense information to report, so we report that everything is
	 * okay.
	 */
	ctl_set_sense_data(sense_ptr,
			   lun,
			   sense_format,
			   /*current_error*/ 1,
			   /*sense_key*/ SSD_KEY_NO_SENSE,
			   /*asc*/ 0x00,
			   /*ascq*/ 0x00,
			   SSD_ELEM_NONE);

	/*
	 * We report 0 for the sense length, because we aren't doing
	 * autosense in this case.  We're reporting sense as parameter data.
	 */
	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_tur(struct ctl_scsiio *ctsio)
{

	CTL_DEBUG_PRINT(("ctl_tur\n"));

	ctl_set_success(ctsio);
	ctl_done((union ctl_io *)ctsio);

	return (CTL_RETVAL_COMPLETE);
}

#ifdef notyet
static int
ctl_cmddt_inquiry(struct ctl_scsiio *ctsio)
{

}
#endif

/*
 * SCSI VPD page 0x00, the Supported VPD Pages page.
 */
static int
ctl_inquiry_evpd_supported(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_supported_pages *pages;
	int sup_page_size;
	struct ctl_lun *lun;
	int p;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	sup_page_size = sizeof(struct scsi_vpd_supported_pages) *
	    SCSI_EVPD_NUM_SUPPORTED_PAGES;
	ctsio->kern_data_ptr = malloc(sup_page_size, M_CTL, M_WAITOK | M_ZERO);
	pages = (struct scsi_vpd_supported_pages *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (sup_page_size < alloc_len) {
		ctsio->residual = alloc_len - sup_page_size;
		ctsio->kern_data_len = sup_page_size;
		ctsio->kern_total_len = sup_page_size;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.  Need to change this
	 * to figure out whether the disk device is actually online or not.
	 */
	if (lun != NULL)
		pages->device = (SID_QUAL_LU_CONNECTED << 5) |
				lun->be_lun->lun_type;
	else
		pages->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;

	p = 0;
	/* Supported VPD pages */
	pages->page_list[p++] = SVPD_SUPPORTED_PAGES;
	/* Serial Number */
	pages->page_list[p++] = SVPD_UNIT_SERIAL_NUMBER;
	/* Device Identification */
	pages->page_list[p++] = SVPD_DEVICE_ID;
	/* Extended INQUIRY Data */
	pages->page_list[p++] = SVPD_EXTENDED_INQUIRY_DATA;
	/* Mode Page Policy */
	pages->page_list[p++] = SVPD_MODE_PAGE_POLICY;
	/* SCSI Ports */
	pages->page_list[p++] = SVPD_SCSI_PORTS;
	/* Third-party Copy */
	pages->page_list[p++] = SVPD_SCSI_TPC;
	if (lun != NULL && lun->be_lun->lun_type == T_DIRECT) {
		/* Block limits */
		pages->page_list[p++] = SVPD_BLOCK_LIMITS;
		/* Block Device Characteristics */
		pages->page_list[p++] = SVPD_BDC;
		/* Logical Block Provisioning */
		pages->page_list[p++] = SVPD_LBP;
	}
	pages->length = p;

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

/*
 * SCSI VPD page 0x80, the Unit Serial Number page.
 */
static int
ctl_inquiry_evpd_serial(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_unit_serial_number *sn_ptr;
	struct ctl_lun *lun;
	int data_len;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	data_len = 4 + CTL_SN_LEN;
	ctsio->kern_data_ptr = malloc(data_len, M_CTL, M_WAITOK | M_ZERO);
	sn_ptr = (struct scsi_vpd_unit_serial_number *)ctsio->kern_data_ptr;
	if (data_len < alloc_len) {
		ctsio->residual = alloc_len - data_len;
		ctsio->kern_data_len = data_len;
		ctsio->kern_total_len = data_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.  Need to change this
	 * to figure out whether the disk device is actually online or not.
	 */
	if (lun != NULL)
		sn_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				  lun->be_lun->lun_type;
	else
		sn_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;

	sn_ptr->page_code = SVPD_UNIT_SERIAL_NUMBER;
	sn_ptr->length = CTL_SN_LEN;
	/*
	 * If we don't have a LUN, we just leave the serial number as
	 * all spaces.
	 */
	if (lun != NULL) {
		strncpy((char *)sn_ptr->serial_num,
			(char *)lun->be_lun->serial_num, CTL_SN_LEN);
	} else
		memset(sn_ptr->serial_num, 0x20, CTL_SN_LEN);

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}


/*
 * SCSI VPD page 0x86, the Extended INQUIRY Data page.
 */
static int
ctl_inquiry_evpd_eid(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_extended_inquiry_data *eid_ptr;
	struct ctl_lun *lun;
	int data_len;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	data_len = sizeof(struct scsi_vpd_extended_inquiry_data);
	ctsio->kern_data_ptr = malloc(data_len, M_CTL, M_WAITOK | M_ZERO);
	eid_ptr = (struct scsi_vpd_extended_inquiry_data *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (data_len < alloc_len) {
		ctsio->residual = alloc_len - data_len;
		ctsio->kern_data_len = data_len;
		ctsio->kern_total_len = data_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.
	 */
	if (lun != NULL)
		eid_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				     lun->be_lun->lun_type;
	else
		eid_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;
	eid_ptr->page_code = SVPD_EXTENDED_INQUIRY_DATA;
	scsi_ulto2b(data_len - 4, eid_ptr->page_length);
	/*
	 * We support head of queue, ordered and simple tags.
	 */
	eid_ptr->flags2 = SVPD_EID_HEADSUP | SVPD_EID_ORDSUP | SVPD_EID_SIMPSUP;
	/*
	 * Volatile cache supported.
	 */
	eid_ptr->flags3 = SVPD_EID_V_SUP;

	/*
	 * This means that we clear the REPORTED LUNS DATA HAS CHANGED unit
	 * attention for a particular IT nexus on all LUNs once we report
	 * it to that nexus once.  This bit is required as of SPC-4.
	 */
	eid_ptr->flags4 = SVPD_EID_LUICLT;

	/*
	 * XXX KDM in order to correctly answer this, we would need
	 * information from the SIM to determine how much sense data it
	 * can send.  So this would really be a path inquiry field, most
	 * likely.  This can be set to a maximum of 252 according to SPC-4,
	 * but the hardware may or may not be able to support that much.
	 * 0 just means that the maximum sense data length is not reported.
	 */
	eid_ptr->max_sense_length = 0;

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_inquiry_evpd_mpp(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_mode_page_policy *mpp_ptr;
	struct ctl_lun *lun;
	int data_len;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	data_len = sizeof(struct scsi_vpd_mode_page_policy) +
	    sizeof(struct scsi_vpd_mode_page_policy_descr);

	ctsio->kern_data_ptr = malloc(data_len, M_CTL, M_WAITOK | M_ZERO);
	mpp_ptr = (struct scsi_vpd_mode_page_policy *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (data_len < alloc_len) {
		ctsio->residual = alloc_len - data_len;
		ctsio->kern_data_len = data_len;
		ctsio->kern_total_len = data_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.
	 */
	if (lun != NULL)
		mpp_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				     lun->be_lun->lun_type;
	else
		mpp_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;
	mpp_ptr->page_code = SVPD_MODE_PAGE_POLICY;
	scsi_ulto2b(data_len - 4, mpp_ptr->page_length);
	mpp_ptr->descr[0].page_code = 0x3f;
	mpp_ptr->descr[0].subpage_code = 0xff;
	mpp_ptr->descr[0].policy = SVPD_MPP_SHARED;

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

/*
 * SCSI VPD page 0x83, the Device Identification page.
 */
static int
ctl_inquiry_evpd_devid(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_device_id *devid_ptr;
	struct scsi_vpd_id_descriptor *desc;
	struct ctl_softc *softc;
	struct ctl_lun *lun;
	struct ctl_port *port;
	int data_len;
	uint8_t proto;

	softc = control_softc;

	port = softc->ctl_ports[ctl_port_idx(ctsio->io_hdr.nexus.targ_port)];
	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	data_len = sizeof(struct scsi_vpd_device_id) +
	    sizeof(struct scsi_vpd_id_descriptor) +
		sizeof(struct scsi_vpd_id_rel_trgt_port_id) +
	    sizeof(struct scsi_vpd_id_descriptor) +
		sizeof(struct scsi_vpd_id_trgt_port_grp_id);
	if (lun && lun->lun_devid)
		data_len += lun->lun_devid->len;
	if (port->port_devid)
		data_len += port->port_devid->len;
	if (port->target_devid)
		data_len += port->target_devid->len;

	ctsio->kern_data_ptr = malloc(data_len, M_CTL, M_WAITOK | M_ZERO);
	devid_ptr = (struct scsi_vpd_device_id *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (data_len < alloc_len) {
		ctsio->residual = alloc_len - data_len;
		ctsio->kern_data_len = data_len;
		ctsio->kern_total_len = data_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.
	 */
	if (lun != NULL)
		devid_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				     lun->be_lun->lun_type;
	else
		devid_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;
	devid_ptr->page_code = SVPD_DEVICE_ID;
	scsi_ulto2b(data_len - 4, devid_ptr->length);

	if (port->port_type == CTL_PORT_FC)
		proto = SCSI_PROTO_FC << 4;
	else if (port->port_type == CTL_PORT_ISCSI)
		proto = SCSI_PROTO_ISCSI << 4;
	else
		proto = SCSI_PROTO_SPI << 4;
	desc = (struct scsi_vpd_id_descriptor *)devid_ptr->desc_list;

	/*
	 * We're using a LUN association here.  i.e., this device ID is a
	 * per-LUN identifier.
	 */
	if (lun && lun->lun_devid) {
		memcpy(desc, lun->lun_devid->data, lun->lun_devid->len);
		desc = (struct scsi_vpd_id_descriptor *)((uint8_t *)desc +
		    lun->lun_devid->len);
	}

	/*
	 * This is for the WWPN which is a port association.
	 */
	if (port->port_devid) {
		memcpy(desc, port->port_devid->data, port->port_devid->len);
		desc = (struct scsi_vpd_id_descriptor *)((uint8_t *)desc +
		    port->port_devid->len);
	}

	/*
	 * This is for the Relative Target Port(type 4h) identifier
	 */
	desc->proto_codeset = proto | SVPD_ID_CODESET_BINARY;
	desc->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_PORT |
	    SVPD_ID_TYPE_RELTARG;
	desc->length = 4;
	scsi_ulto2b(ctsio->io_hdr.nexus.targ_port, &desc->identifier[2]);
	desc = (struct scsi_vpd_id_descriptor *)(&desc->identifier[0] +
	    sizeof(struct scsi_vpd_id_rel_trgt_port_id));

	/*
	 * This is for the Target Port Group(type 5h) identifier
	 */
	desc->proto_codeset = proto | SVPD_ID_CODESET_BINARY;
	desc->id_type = SVPD_ID_PIV | SVPD_ID_ASSOC_PORT |
	    SVPD_ID_TYPE_TPORTGRP;
	desc->length = 4;
	scsi_ulto2b(ctsio->io_hdr.nexus.targ_port / CTL_MAX_PORTS + 1,
	    &desc->identifier[2]);
	desc = (struct scsi_vpd_id_descriptor *)(&desc->identifier[0] +
	    sizeof(struct scsi_vpd_id_trgt_port_grp_id));

	/*
	 * This is for the Target identifier
	 */
	if (port->target_devid) {
		memcpy(desc, port->target_devid->data, port->target_devid->len);
	}

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_inquiry_evpd_scsi_ports(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct ctl_softc *softc = control_softc;
	struct scsi_vpd_scsi_ports *sp;
	struct scsi_vpd_port_designation *pd;
	struct scsi_vpd_port_designation_cont *pdc;
	struct ctl_lun *lun;
	struct ctl_port *port;
	int data_len, num_target_ports, iid_len, id_len, g, pg, p;
	int num_target_port_groups;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	if (softc->is_single)
		num_target_port_groups = 1;
	else
		num_target_port_groups = NUM_TARGET_PORT_GROUPS;
	num_target_ports = 0;
	iid_len = 0;
	id_len = 0;
	mtx_lock(&softc->ctl_lock);
	STAILQ_FOREACH(port, &softc->port_list, links) {
		if ((port->status & CTL_PORT_STATUS_ONLINE) == 0)
			continue;
		if (lun != NULL &&
		    ctl_lun_map_to_port(port, lun->lun) >= CTL_MAX_LUNS)
			continue;
		num_target_ports++;
		if (port->init_devid)
			iid_len += port->init_devid->len;
		if (port->port_devid)
			id_len += port->port_devid->len;
	}
	mtx_unlock(&softc->ctl_lock);

	data_len = sizeof(struct scsi_vpd_scsi_ports) + num_target_port_groups *
	    num_target_ports * (sizeof(struct scsi_vpd_port_designation) +
	     sizeof(struct scsi_vpd_port_designation_cont)) + iid_len + id_len;
	ctsio->kern_data_ptr = malloc(data_len, M_CTL, M_WAITOK | M_ZERO);
	sp = (struct scsi_vpd_scsi_ports *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (data_len < alloc_len) {
		ctsio->residual = alloc_len - data_len;
		ctsio->kern_data_len = data_len;
		ctsio->kern_total_len = data_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.  Need to change this
	 * to figure out whether the disk device is actually online or not.
	 */
	if (lun != NULL)
		sp->device = (SID_QUAL_LU_CONNECTED << 5) |
				  lun->be_lun->lun_type;
	else
		sp->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;

	sp->page_code = SVPD_SCSI_PORTS;
	scsi_ulto2b(data_len - sizeof(struct scsi_vpd_scsi_ports),
	    sp->page_length);
	pd = &sp->design[0];

	mtx_lock(&softc->ctl_lock);
	pg = softc->port_offset / CTL_MAX_PORTS;
	for (g = 0; g < num_target_port_groups; g++) {
		STAILQ_FOREACH(port, &softc->port_list, links) {
			if ((port->status & CTL_PORT_STATUS_ONLINE) == 0)
				continue;
			if (lun != NULL &&
			    ctl_lun_map_to_port(port, lun->lun) >= CTL_MAX_LUNS)
				continue;
			p = port->targ_port % CTL_MAX_PORTS + g * CTL_MAX_PORTS;
			scsi_ulto2b(p, pd->relative_port_id);
			if (port->init_devid && g == pg) {
				iid_len = port->init_devid->len;
				memcpy(pd->initiator_transportid,
				    port->init_devid->data, port->init_devid->len);
			} else
				iid_len = 0;
			scsi_ulto2b(iid_len, pd->initiator_transportid_length);
			pdc = (struct scsi_vpd_port_designation_cont *)
			    (&pd->initiator_transportid[iid_len]);
			if (port->port_devid && g == pg) {
				id_len = port->port_devid->len;
				memcpy(pdc->target_port_descriptors,
				    port->port_devid->data, port->port_devid->len);
			} else
				id_len = 0;
			scsi_ulto2b(id_len, pdc->target_port_descriptors_length);
			pd = (struct scsi_vpd_port_designation *)
			    ((uint8_t *)pdc->target_port_descriptors + id_len);
		}
	}
	mtx_unlock(&softc->ctl_lock);

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_inquiry_evpd_block_limits(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_block_limits *bl_ptr;
	struct ctl_lun *lun;
	int bs;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	ctsio->kern_data_ptr = malloc(sizeof(*bl_ptr), M_CTL, M_WAITOK | M_ZERO);
	bl_ptr = (struct scsi_vpd_block_limits *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (sizeof(*bl_ptr) < alloc_len) {
		ctsio->residual = alloc_len - sizeof(*bl_ptr);
		ctsio->kern_data_len = sizeof(*bl_ptr);
		ctsio->kern_total_len = sizeof(*bl_ptr);
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.  Need to change this
	 * to figure out whether the disk device is actually online or not.
	 */
	if (lun != NULL)
		bl_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				  lun->be_lun->lun_type;
	else
		bl_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;

	bl_ptr->page_code = SVPD_BLOCK_LIMITS;
	scsi_ulto2b(sizeof(*bl_ptr) - 4, bl_ptr->page_length);
	bl_ptr->max_cmp_write_len = 0xff;
	scsi_ulto4b(0xffffffff, bl_ptr->max_txfer_len);
	if (lun != NULL) {
		bs = lun->be_lun->blocksize;
		scsi_ulto4b(lun->be_lun->opttxferlen, bl_ptr->opt_txfer_len);
		if (lun->be_lun->flags & CTL_LUN_FLAG_UNMAP) {
			scsi_ulto4b(0xffffffff, bl_ptr->max_unmap_lba_cnt);
			scsi_ulto4b(0xffffffff, bl_ptr->max_unmap_blk_cnt);
			if (lun->be_lun->ublockexp != 0) {
				scsi_ulto4b((1 << lun->be_lun->ublockexp),
				    bl_ptr->opt_unmap_grain);
				scsi_ulto4b(0x80000000 | lun->be_lun->ublockoff,
				    bl_ptr->unmap_grain_align);
			}
		}
		scsi_ulto4b(lun->be_lun->atomicblock,
		    bl_ptr->max_atomic_transfer_length);
		scsi_ulto4b(0, bl_ptr->atomic_alignment);
		scsi_ulto4b(0, bl_ptr->atomic_transfer_length_granularity);
	}
	scsi_u64to8b(UINT64_MAX, bl_ptr->max_write_same_length);

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_inquiry_evpd_bdc(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_block_device_characteristics *bdc_ptr;
	struct ctl_lun *lun;
	const char *value;
	u_int i;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	ctsio->kern_data_ptr = malloc(sizeof(*bdc_ptr), M_CTL, M_WAITOK | M_ZERO);
	bdc_ptr = (struct scsi_vpd_block_device_characteristics *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (sizeof(*bdc_ptr) < alloc_len) {
		ctsio->residual = alloc_len - sizeof(*bdc_ptr);
		ctsio->kern_data_len = sizeof(*bdc_ptr);
		ctsio->kern_total_len = sizeof(*bdc_ptr);
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.  Need to change this
	 * to figure out whether the disk device is actually online or not.
	 */
	if (lun != NULL)
		bdc_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				  lun->be_lun->lun_type;
	else
		bdc_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;
	bdc_ptr->page_code = SVPD_BDC;
	scsi_ulto2b(sizeof(*bdc_ptr) - 4, bdc_ptr->page_length);
	if (lun != NULL &&
	    (value = ctl_get_opt(&lun->be_lun->options, "rpm")) != NULL)
		i = strtol(value, NULL, 0);
	else
		i = CTL_DEFAULT_ROTATION_RATE;
	scsi_ulto2b(i, bdc_ptr->medium_rotation_rate);
	if (lun != NULL &&
	    (value = ctl_get_opt(&lun->be_lun->options, "formfactor")) != NULL)
		i = strtol(value, NULL, 0);
	else
		i = 0;
	bdc_ptr->wab_wac_ff = (i & 0x0f);
	bdc_ptr->flags = SVPD_FUAB | SVPD_VBULS;

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_inquiry_evpd_lbp(struct ctl_scsiio *ctsio, int alloc_len)
{
	struct scsi_vpd_logical_block_prov *lbp_ptr;
	struct ctl_lun *lun;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	ctsio->kern_data_ptr = malloc(sizeof(*lbp_ptr), M_CTL, M_WAITOK | M_ZERO);
	lbp_ptr = (struct scsi_vpd_logical_block_prov *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;

	if (sizeof(*lbp_ptr) < alloc_len) {
		ctsio->residual = alloc_len - sizeof(*lbp_ptr);
		ctsio->kern_data_len = sizeof(*lbp_ptr);
		ctsio->kern_total_len = sizeof(*lbp_ptr);
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;
	ctsio->kern_sg_entries = 0;

	/*
	 * The control device is always connected.  The disk device, on the
	 * other hand, may not be online all the time.  Need to change this
	 * to figure out whether the disk device is actually online or not.
	 */
	if (lun != NULL)
		lbp_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				  lun->be_lun->lun_type;
	else
		lbp_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;

	lbp_ptr->page_code = SVPD_LBP;
	scsi_ulto2b(sizeof(*lbp_ptr) - 4, lbp_ptr->page_length);
	lbp_ptr->threshold_exponent = CTL_LBP_EXPONENT;
	if (lun != NULL && lun->be_lun->flags & CTL_LUN_FLAG_UNMAP) {
		lbp_ptr->flags = SVPD_LBP_UNMAP | SVPD_LBP_WS16 |
		    SVPD_LBP_WS10 | SVPD_LBP_RZ | SVPD_LBP_ANC_SUP;
		lbp_ptr->prov_type = SVPD_LBP_THIN;
	}

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

/*
 * INQUIRY with the EVPD bit set.
 */
static int
ctl_inquiry_evpd(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	struct scsi_inquiry *cdb;
	int alloc_len, retval;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_inquiry *)ctsio->cdb;
	alloc_len = scsi_2btoul(cdb->length);

	switch (cdb->page_code) {
	case SVPD_SUPPORTED_PAGES:
		retval = ctl_inquiry_evpd_supported(ctsio, alloc_len);
		break;
	case SVPD_UNIT_SERIAL_NUMBER:
		retval = ctl_inquiry_evpd_serial(ctsio, alloc_len);
		break;
	case SVPD_DEVICE_ID:
		retval = ctl_inquiry_evpd_devid(ctsio, alloc_len);
		break;
	case SVPD_EXTENDED_INQUIRY_DATA:
		retval = ctl_inquiry_evpd_eid(ctsio, alloc_len);
		break;
	case SVPD_MODE_PAGE_POLICY:
		retval = ctl_inquiry_evpd_mpp(ctsio, alloc_len);
		break;
	case SVPD_SCSI_PORTS:
		retval = ctl_inquiry_evpd_scsi_ports(ctsio, alloc_len);
		break;
	case SVPD_SCSI_TPC:
		retval = ctl_inquiry_evpd_tpc(ctsio, alloc_len);
		break;
	case SVPD_BLOCK_LIMITS:
		if (lun == NULL || lun->be_lun->lun_type != T_DIRECT)
			goto err;
		retval = ctl_inquiry_evpd_block_limits(ctsio, alloc_len);
		break;
	case SVPD_BDC:
		if (lun == NULL || lun->be_lun->lun_type != T_DIRECT)
			goto err;
		retval = ctl_inquiry_evpd_bdc(ctsio, alloc_len);
		break;
	case SVPD_LBP:
		if (lun == NULL || lun->be_lun->lun_type != T_DIRECT)
			goto err;
		retval = ctl_inquiry_evpd_lbp(ctsio, alloc_len);
		break;
	default:
err:
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 2,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		retval = CTL_RETVAL_COMPLETE;
		break;
	}

	return (retval);
}

/*
 * Standard INQUIRY data.
 */
static int
ctl_inquiry_std(struct ctl_scsiio *ctsio)
{
	struct scsi_inquiry_data *inq_ptr;
	struct scsi_inquiry *cdb;
	struct ctl_softc *softc;
	struct ctl_lun *lun;
	char *val;
	uint32_t alloc_len, data_len;
	ctl_port_type port_type;

	softc = control_softc;

	/*
	 * Figure out whether we're talking to a Fibre Channel port or not.
	 * We treat the ioctl front end, and any SCSI adapters, as packetized
	 * SCSI front ends.
	 */
	port_type = softc->ctl_ports[
	    ctl_port_idx(ctsio->io_hdr.nexus.targ_port)]->port_type;
	if (port_type == CTL_PORT_IOCTL || port_type == CTL_PORT_INTERNAL)
		port_type = CTL_PORT_SCSI;

	lun = ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	cdb = (struct scsi_inquiry *)ctsio->cdb;
	alloc_len = scsi_2btoul(cdb->length);

	/*
	 * We malloc the full inquiry data size here and fill it
	 * in.  If the user only asks for less, we'll give him
	 * that much.
	 */
	data_len = offsetof(struct scsi_inquiry_data, vendor_specific1);
	ctsio->kern_data_ptr = malloc(data_len, M_CTL, M_WAITOK | M_ZERO);
	inq_ptr = (struct scsi_inquiry_data *)ctsio->kern_data_ptr;
	ctsio->kern_sg_entries = 0;
	ctsio->kern_data_resid = 0;
	ctsio->kern_rel_offset = 0;

	if (data_len < alloc_len) {
		ctsio->residual = alloc_len - data_len;
		ctsio->kern_data_len = data_len;
		ctsio->kern_total_len = data_len;
	} else {
		ctsio->residual = 0;
		ctsio->kern_data_len = alloc_len;
		ctsio->kern_total_len = alloc_len;
	}

	/*
	 * If we have a LUN configured, report it as connected.  Otherwise,
	 * report that it is offline or no device is supported, depending 
	 * on the value of inquiry_pq_no_lun.
	 *
	 * According to the spec (SPC-4 r34), the peripheral qualifier
	 * SID_QUAL_LU_OFFLINE (001b) is used in the following scenario:
	 *
	 * "A peripheral device having the specified peripheral device type 
	 * is not connected to this logical unit. However, the device
	 * server is capable of supporting the specified peripheral device
	 * type on this logical unit."
	 *
	 * According to the same spec, the peripheral qualifier
	 * SID_QUAL_BAD_LU (011b) is used in this scenario:
	 *
	 * "The device server is not capable of supporting a peripheral
	 * device on this logical unit. For this peripheral qualifier the
	 * peripheral device type shall be set to 1Fh. All other peripheral
	 * device type values are reserved for this peripheral qualifier."
	 *
	 * Given the text, it would seem that we probably want to report that
	 * the LUN is offline here.  There is no LUN connected, but we can
	 * support a LUN at the given LUN number.
	 *
	 * In the real world, though, it sounds like things are a little
	 * different:
	 *
	 * - Linux, when presented with a LUN with the offline peripheral
	 *   qualifier, will create an sg driver instance for it.  So when
	 *   you attach it to CTL, you wind up with a ton of sg driver
	 *   instances.  (One for every LUN that Linux bothered to probe.)
	 *   Linux does this despite the fact that it issues a REPORT LUNs
	 *   to LUN 0 to get the inventory of supported LUNs.
	 *
	 * - There is other anecdotal evidence (from Emulex folks) about
	 *   arrays that use the offline peripheral qualifier for LUNs that
	 *   are on the "passive" path in an active/passive array.
	 *
	 * So the solution is provide a hopefully reasonable default
	 * (return bad/no LUN) and allow the user to change the behavior
	 * with a tunable/sysctl variable.
	 */
	if (lun != NULL)
		inq_ptr->device = (SID_QUAL_LU_CONNECTED << 5) |
				  lun->be_lun->lun_type;
	else if (softc->inquiry_pq_no_lun == 0)
		inq_ptr->device = (SID_QUAL_LU_OFFLINE << 5) | T_DIRECT;
	else
		inq_ptr->device = (SID_QUAL_BAD_LU << 5) | T_NODEVICE;

	/* RMB in byte 2 is 0 */
	inq_ptr->version = SCSI_REV_SPC4;

	/*
	 * According to SAM-3, even if a device only supports a single
	 * level of LUN addressing, it should still set the HISUP bit:
	 *
	 * 4.9.1 Logical unit numbers overview
	 *
	 * All logical unit number formats described in this standard are
	 * hierarchical in structure even when only a single level in that
	 * hierarchy is used. The HISUP bit shall be set to one in the
	 * standard INQUIRY data (see SPC-2) when any logical unit number
	 * format described in this standard is used.  Non-hierarchical
	 * formats are outside the scope of this standard.
	 *
	 * Therefore we set the HiSup bit here.
	 *
	 * The reponse format is 2, per SPC-3.
	 */
	inq_ptr->response_format = SID_HiSup | 2;

	inq_ptr->additional_length = data_len -
	    (offsetof(struct scsi_inquiry_data, additional_length) + 1);
	CTL_DEBUG_PRINT(("additional_length = %d\n",
			 inq_ptr->additional_length));

	inq_ptr->spc3_flags = SPC3_SID_3PC | SPC3_SID_TPGS_IMPLICIT;
	/* 16 bit addressing */
	if (port_type == CTL_PORT_SCSI)
		inq_ptr->spc2_flags = SPC2_SID_ADDR16;
	/* XXX set the SID_MultiP bit here if we're actually going to
	   respond on multiple ports */
	inq_ptr->spc2_flags |= SPC2_SID_MultiP;

	/* 16 bit data bus, synchronous transfers */
	if (port_type == CTL_PORT_SCSI)
		inq_ptr->flags = SID_WBus16 | SID_Sync;
	/*
	 * XXX KDM do we want to support tagged queueing on the control
	 * device at all?
	 */
	if ((lun == NULL)
	 || (lun->be_lun->lun_type != T_PROCESSOR))
		inq_ptr->flags |= SID_CmdQue;
	/*
	 * Per SPC-3, unused bytes in ASCII strings are filled with spaces.
	 * We have 8 bytes for the vendor name, and 16 bytes for the device
	 * name and 4 bytes for the revision.
	 */
	if (lun == NULL || (val = ctl_get_opt(&lun->be_lun->options,
	    "vendor")) == NULL) {
		strncpy(inq_ptr->vendor, CTL_VENDOR, sizeof(inq_ptr->vendor));
	} else {
		memset(inq_ptr->vendor, ' ', sizeof(inq_ptr->vendor));
		strncpy(inq_ptr->vendor, val,
		    min(sizeof(inq_ptr->vendor), strlen(val)));
	}
	if (lun == NULL) {
		strncpy(inq_ptr->product, CTL_DIRECT_PRODUCT,
		    sizeof(inq_ptr->product));
	} else if ((val = ctl_get_opt(&lun->be_lun->options, "product")) == NULL) {
		switch (lun->be_lun->lun_type) {
		case T_DIRECT:
			strncpy(inq_ptr->product, CTL_DIRECT_PRODUCT,
			    sizeof(inq_ptr->product));
			break;
		case T_PROCESSOR:
			strncpy(inq_ptr->product, CTL_PROCESSOR_PRODUCT,
			    sizeof(inq_ptr->product));
			break;
		default:
			strncpy(inq_ptr->product, CTL_UNKNOWN_PRODUCT,
			    sizeof(inq_ptr->product));
			break;
		}
	} else {
		memset(inq_ptr->product, ' ', sizeof(inq_ptr->product));
		strncpy(inq_ptr->product, val,
		    min(sizeof(inq_ptr->product), strlen(val)));
	}

	/*
	 * XXX make this a macro somewhere so it automatically gets
	 * incremented when we make changes.
	 */
	if (lun == NULL || (val = ctl_get_opt(&lun->be_lun->options,
	    "revision")) == NULL) {
		strncpy(inq_ptr->revision, "0001", sizeof(inq_ptr->revision));
	} else {
		memset(inq_ptr->revision, ' ', sizeof(inq_ptr->revision));
		strncpy(inq_ptr->revision, val,
		    min(sizeof(inq_ptr->revision), strlen(val)));
	}

	/*
	 * For parallel SCSI, we support double transition and single
	 * transition clocking.  We also support QAS (Quick Arbitration
	 * and Selection) and Information Unit transfers on both the
	 * control and array devices.
	 */
	if (port_type == CTL_PORT_SCSI)
		inq_ptr->spi3data = SID_SPI_CLOCK_DT_ST | SID_SPI_QAS |
				    SID_SPI_IUS;

	/* SAM-5 (no version claimed) */
	scsi_ulto2b(0x00A0, inq_ptr->version1);
	/* SPC-4 (no version claimed) */
	scsi_ulto2b(0x0460, inq_ptr->version2);
	if (port_type == CTL_PORT_FC) {
		/* FCP-2 ANSI INCITS.350:2003 */
		scsi_ulto2b(0x0917, inq_ptr->version3);
	} else if (port_type == CTL_PORT_SCSI) {
		/* SPI-4 ANSI INCITS.362:200x */
		scsi_ulto2b(0x0B56, inq_ptr->version3);
	} else if (port_type == CTL_PORT_ISCSI) {
		/* iSCSI (no version claimed) */
		scsi_ulto2b(0x0960, inq_ptr->version3);
	} else if (port_type == CTL_PORT_SAS) {
		/* SAS (no version claimed) */
		scsi_ulto2b(0x0BE0, inq_ptr->version3);
	}

	if (lun == NULL) {
		/* SBC-4 (no version claimed) */
		scsi_ulto2b(0x0600, inq_ptr->version4);
	} else {
		switch (lun->be_lun->lun_type) {
		case T_DIRECT:
			/* SBC-4 (no version claimed) */
			scsi_ulto2b(0x0600, inq_ptr->version4);
			break;
		case T_PROCESSOR:
		default:
			break;
		}
	}

	ctl_set_success(ctsio);
	ctsio->io_hdr.flags |= CTL_FLAG_ALLOCATED;
	ctsio->be_move_done = ctl_config_move_done;
	ctl_datamove((union ctl_io *)ctsio);
	return (CTL_RETVAL_COMPLETE);
}

int
ctl_inquiry(struct ctl_scsiio *ctsio)
{
	struct scsi_inquiry *cdb;
	int retval;

	CTL_DEBUG_PRINT(("ctl_inquiry\n"));

	cdb = (struct scsi_inquiry *)ctsio->cdb;
	if (cdb->byte2 & SI_EVPD)
		retval = ctl_inquiry_evpd(ctsio);
	else if (cdb->page_code == 0)
		retval = ctl_inquiry_std(ctsio);
	else {
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 2,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_done((union ctl_io *)ctsio);
		return (CTL_RETVAL_COMPLETE);
	}

	return (retval);
}

/*
 * For known CDB types, parse the LBA and length.
 */
static int
ctl_get_lba_len(union ctl_io *io, uint64_t *lba, uint64_t *len)
{
	if (io->io_hdr.io_type != CTL_IO_SCSI)
		return (1);

	switch (io->scsiio.cdb[0]) {
	case COMPARE_AND_WRITE: {
		struct scsi_compare_and_write *cdb;

		cdb = (struct scsi_compare_and_write *)io->scsiio.cdb;

		*lba = scsi_8btou64(cdb->addr);
		*len = cdb->length;
		break;
	}
	case READ_6:
	case WRITE_6: {
		struct scsi_rw_6 *cdb;

		cdb = (struct scsi_rw_6 *)io->scsiio.cdb;

		*lba = scsi_3btoul(cdb->addr);
		/* only 5 bits are valid in the most significant address byte */
		*lba &= 0x1fffff;
		*len = cdb->length;
		break;
	}
	case READ_10:
	case WRITE_10: {
		struct scsi_rw_10 *cdb;

		cdb = (struct scsi_rw_10 *)io->scsiio.cdb;

		*lba = scsi_4btoul(cdb->addr);
		*len = scsi_2btoul(cdb->length);
		break;
	}
	case WRITE_VERIFY_10: {
		struct scsi_write_verify_10 *cdb;

		cdb = (struct scsi_write_verify_10 *)io->scsiio.cdb;

		*lba = scsi_4btoul(cdb->addr);
		*len = scsi_2btoul(cdb->length);
		break;
	}
	case READ_12:
	case WRITE_12: {
		struct scsi_rw_12 *cdb;

		cdb = (struct scsi_rw_12 *)io->scsiio.cdb;

		*lba = scsi_4btoul(cdb->addr);
		*len = scsi_4btoul(cdb->length);
		break;
	}
	case WRITE_VERIFY_12: {
		struct scsi_write_verify_12 *cdb;

		cdb = (struct scsi_write_verify_12 *)io->scsiio.cdb;

		*lba = scsi_4btoul(cdb->addr);
		*len = scsi_4btoul(cdb->length);
		break;
	}
	case READ_16:
	case WRITE_16:
	case WRITE_ATOMIC_16: {
		struct scsi_rw_16 *cdb;

		cdb = (struct scsi_rw_16 *)io->scsiio.cdb;

		*lba = scsi_8btou64(cdb->addr);
		*len = scsi_4btoul(cdb->length);
		break;
	}
	case WRITE_VERIFY_16: {
		struct scsi_write_verify_16 *cdb;

		cdb = (struct scsi_write_verify_16 *)io->scsiio.cdb;

		*lba = scsi_8btou64(cdb->addr);
		*len = scsi_4btoul(cdb->length);
		break;
	}
	case WRITE_SAME_10: {
		struct scsi_write_same_10 *cdb;

		cdb = (struct scsi_write_same_10 *)io->scsiio.cdb;

		*lba = scsi_4btoul(cdb->addr);
		*len = scsi_2btoul(cdb->length);
		break;
	}
	case WRITE_SAME_16: {
		struct scsi_write_same_16 *cdb;

		cdb = (struct scsi_write_same_16 *)io->scsiio.cdb;

		*lba = scsi_8btou64(cdb->addr);
		*len = scsi_4btoul(cdb->length);
		break;
	}
	case VERIFY_10: {
		struct scsi_verify_10 *cdb;

		cdb = (struct scsi_verify_10 *)io->scsiio.cdb;

		*lba = scsi_4btoul(cdb->addr);
		*len = scsi_2btoul(cdb->length);
		break;
	}
	case VERIFY_12: {
		struct scsi_verify_12 *cdb;

		cdb = (struct scsi_verify_12 *)io->scsiio.cdb;

		*lba = scsi_4btoul(cdb->addr);
		*len = scsi_4btoul(cdb->length);
		break;
	}
	case VERIFY_16: {
		struct scsi_verify_16 *cdb;

		cdb = (struct scsi_verify_16 *)io->scsiio.cdb;

		*lba = scsi_8btou64(cdb->addr);
		*len = scsi_4btoul(cdb->length);
		break;
	}
	case UNMAP: {
		*lba = 0;
		*len = UINT64_MAX;
		break;
	}
	case SERVICE_ACTION_IN: {	/* GET LBA STATUS */
		struct scsi_get_lba_status *cdb;

		cdb = (struct scsi_get_lba_status *)io->scsiio.cdb;
		*lba = scsi_8btou64(cdb->addr);
		*len = UINT32_MAX;
		break;
	}
	default:
		return (1);
		break; /* NOTREACHED */
	}

	return (0);
}

static ctl_action
ctl_extent_check_lba(uint64_t lba1, uint64_t len1, uint64_t lba2, uint64_t len2,
    bool seq)
{
	uint64_t endlba1, endlba2;

	endlba1 = lba1 + len1 - (seq ? 0 : 1);
	endlba2 = lba2 + len2 - 1;

	if ((endlba1 < lba2) || (endlba2 < lba1))
		return (CTL_ACTION_PASS);
	else
		return (CTL_ACTION_BLOCK);
}

static int
ctl_extent_check_unmap(union ctl_io *io, uint64_t lba2, uint64_t len2)
{
	struct ctl_ptr_len_flags *ptrlen;
	struct scsi_unmap_desc *buf, *end, *range;
	uint64_t lba;
	uint32_t len;

	/* If not UNMAP -- go other way. */
	if (io->io_hdr.io_type != CTL_IO_SCSI ||
	    io->scsiio.cdb[0] != UNMAP)
		return (CTL_ACTION_ERROR);

	/* If UNMAP without data -- block and wait for data. */
	ptrlen = (struct ctl_ptr_len_flags *)
	    &io->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
	if ((io->io_hdr.flags & CTL_FLAG_ALLOCATED) == 0 ||
	    ptrlen->ptr == NULL)
		return (CTL_ACTION_BLOCK);

	/* UNMAP with data -- check for collision. */
	buf = (struct scsi_unmap_desc *)ptrlen->ptr;
	end = buf + ptrlen->len / sizeof(*buf);
	for (range = buf; range < end; range++) {
		lba = scsi_8btou64(range->lba);
		len = scsi_4btoul(range->length);
		if ((lba < lba2 + len2) && (lba + len > lba2))
			return (CTL_ACTION_BLOCK);
	}
	return (CTL_ACTION_PASS);
}

static ctl_action
ctl_extent_check(union ctl_io *io1, union ctl_io *io2, bool seq)
{
	uint64_t lba1, lba2;
	uint64_t len1, len2;
	int retval;

	if (ctl_get_lba_len(io2, &lba2, &len2) != 0)
		return (CTL_ACTION_ERROR);

	retval = ctl_extent_check_unmap(io1, lba2, len2);
	if (retval != CTL_ACTION_ERROR)
		return (retval);

	if (ctl_get_lba_len(io1, &lba1, &len1) != 0)
		return (CTL_ACTION_ERROR);

	return (ctl_extent_check_lba(lba1, len1, lba2, len2, seq));
}

static ctl_action
ctl_extent_check_seq(union ctl_io *io1, union ctl_io *io2)
{
	uint64_t lba1, lba2;
	uint64_t len1, len2;

	if (ctl_get_lba_len(io1, &lba1, &len1) != 0)
		return (CTL_ACTION_ERROR);
	if (ctl_get_lba_len(io2, &lba2, &len2) != 0)
		return (CTL_ACTION_ERROR);

	if (lba1 + len1 == lba2)
		return (CTL_ACTION_BLOCK);
	return (CTL_ACTION_PASS);
}

static ctl_action
ctl_check_for_blockage(struct ctl_lun *lun, union ctl_io *pending_io,
    union ctl_io *ooa_io)
{
	const struct ctl_cmd_entry *pending_entry, *ooa_entry;
	ctl_serialize_action *serialize_row;

	/*
	 * The initiator attempted multiple untagged commands at the same
	 * time.  Can't do that.
	 */
	if ((pending_io->scsiio.tag_type == CTL_TAG_UNTAGGED)
	 && (ooa_io->scsiio.tag_type == CTL_TAG_UNTAGGED)
	 && ((pending_io->io_hdr.nexus.targ_port ==
	      ooa_io->io_hdr.nexus.targ_port)
	  && (pending_io->io_hdr.nexus.initid.id ==
	      ooa_io->io_hdr.nexus.initid.id))
	 && ((ooa_io->io_hdr.flags & (CTL_FLAG_ABORT |
	      CTL_FLAG_STATUS_SENT)) == 0))
		return (CTL_ACTION_OVERLAP);

	/*
	 * The initiator attempted to send multiple tagged commands with
	 * the same ID.  (It's fine if different initiators have the same
	 * tag ID.)
	 *
	 * Even if all of those conditions are true, we don't kill the I/O
	 * if the command ahead of us has been aborted.  We won't end up
	 * sending it to the FETD, and it's perfectly legal to resend a
	 * command with the same tag number as long as the previous
	 * instance of this tag number has been aborted somehow.
	 */
	if ((pending_io->scsiio.tag_type != CTL_TAG_UNTAGGED)
	 && (ooa_io->scsiio.tag_type != CTL_TAG_UNTAGGED)
	 && (pending_io->scsiio.tag_num == ooa_io->scsiio.tag_num)
	 && ((pending_io->io_hdr.nexus.targ_port ==
	      ooa_io->io_hdr.nexus.targ_port)
	  && (pending_io->io_hdr.nexus.initid.id ==
	      ooa_io->io_hdr.nexus.initid.id))
	 && ((ooa_io->io_hdr.flags & (CTL_FLAG_ABORT |
	      CTL_FLAG_STATUS_SENT)) == 0))
		return (CTL_ACTION_OVERLAP_TAG);

	/*
	 * If we get a head of queue tag, SAM-3 says that we should
	 * immediately execute it.
	 *
	 * What happens if this command would normally block for some other
	 * reason?  e.g. a request sense with a head of queue tag
	 * immediately after a write.  Normally that would block, but this
	 * will result in its getting executed immediately...
	 *
	 * We currently return "pass" instead of "skip", so we'll end up
	 * going through the rest of the queue to check for overlapped tags.
	 *
	 * XXX KDM check for other types of blockage first??
	 */
	if (pending_io->scsiio.tag_type == CTL_TAG_HEAD_OF_QUEUE)
		return (CTL_ACTION_PASS);

	/*
	 * Ordered tags have to block until all items ahead of them
	 * have completed.  If we get called with an ordered tag, we always
	 * block, if something else is ahead of us in the queue.
	 */
	if (pending_io->scsiio.tag_type == CTL_TAG_ORDERED)
		return (CTL_ACTION_BLOCK);

	/*
	 * Simple tags get blocked until all head of queue and ordered tags
	 * ahead of them have completed.  I'm lumping untagged commands in
	 * with simple tags here.  XXX KDM is that the right thing to do?
	 */
	if (((pending_io->scsiio.tag_type == CTL_TAG_UNTAGGED)
	  || (pending_io->scsiio.tag_type == CTL_TAG_SIMPLE))
	 && ((ooa_io->scsiio.tag_type == CTL_TAG_HEAD_OF_QUEUE)
	  || (ooa_io->scsiio.tag_type == CTL_TAG_ORDERED)))
		return (CTL_ACTION_BLOCK);

	pending_entry = ctl_get_cmd_entry(&pending_io->scsiio, NULL);
	ooa_entry = ctl_get_cmd_entry(&ooa_io->scsiio, NULL);

	serialize_row = ctl_serialize_table[ooa_entry->seridx];

	switch (serialize_row[pending_entry->seridx]) {
	case CTL_SER_BLOCK:
		return (CTL_ACTION_BLOCK);
	case CTL_SER_EXTENT:
		return (ctl_extent_check(ooa_io, pending_io,
		    (lun->serseq == CTL_LUN_SERSEQ_ON)));
	case CTL_SER_EXTENTOPT:
		if ((lun->mode_pages.control_page[CTL_PAGE_CURRENT].queue_flags
		    & SCP_QUEUE_ALG_MASK) != SCP_QUEUE_ALG_UNRESTRICTED)
			return (ctl_extent_check(ooa_io, pending_io,
			    (lun->serseq == CTL_LUN_SERSEQ_ON)));
		return (CTL_ACTION_PASS);
	case CTL_SER_EXTENTSEQ:
		if (lun->serseq != CTL_LUN_SERSEQ_OFF)
			return (ctl_extent_check_seq(ooa_io, pending_io));
		return (CTL_ACTION_PASS);
	case CTL_SER_PASS:
		return (CTL_ACTION_PASS);
	case CTL_SER_BLOCKOPT:
		if ((lun->mode_pages.control_page[CTL_PAGE_CURRENT].queue_flags
		    & SCP_QUEUE_ALG_MASK) != SCP_QUEUE_ALG_UNRESTRICTED)
			return (CTL_ACTION_BLOCK);
		return (CTL_ACTION_PASS);
	case CTL_SER_SKIP:
		return (CTL_ACTION_SKIP);
	default:
		panic("invalid serialization value %d",
		      serialize_row[pending_entry->seridx]);
	}

	return (CTL_ACTION_ERROR);
}

/*
 * Check for blockage or overlaps against the OOA (Order Of Arrival) queue.
 * Assumptions:
 * - pending_io is generally either incoming, or on the blocked queue
 * - starting I/O is the I/O we want to start the check with.
 */
static ctl_action
ctl_check_ooa(struct ctl_lun *lun, union ctl_io *pending_io,
	      union ctl_io *starting_io)
{
	union ctl_io *ooa_io;
	ctl_action action;

	mtx_assert(&lun->lun_lock, MA_OWNED);

	/*
	 * Run back along the OOA queue, starting with the current
	 * blocked I/O and going through every I/O before it on the
	 * queue.  If starting_io is NULL, we'll just end up returning
	 * CTL_ACTION_PASS.
	 */
	for (ooa_io = starting_io; ooa_io != NULL;
	     ooa_io = (union ctl_io *)TAILQ_PREV(&ooa_io->io_hdr, ctl_ooaq,
	     ooa_links)){

		/*
		 * This routine just checks to see whether
		 * cur_blocked is blocked by ooa_io, which is ahead
		 * of it in the queue.  It doesn't queue/dequeue
		 * cur_blocked.
		 */
		action = ctl_check_for_blockage(lun, pending_io, ooa_io);
		switch (action) {
		case CTL_ACTION_BLOCK:
		case CTL_ACTION_OVERLAP:
		case CTL_ACTION_OVERLAP_TAG:
		case CTL_ACTION_SKIP:
		case CTL_ACTION_ERROR:
			return (action);
			break; /* NOTREACHED */
		case CTL_ACTION_PASS:
			break;
		default:
			panic("invalid action %d", action);
			break;  /* NOTREACHED */
		}
	}

	return (CTL_ACTION_PASS);
}

/*
 * Assumptions:
 * - An I/O has just completed, and has been removed from the per-LUN OOA
 *   queue, so some items on the blocked queue may now be unblocked.
 */
static int
ctl_check_blocked(struct ctl_lun *lun)
{
	union ctl_io *cur_blocked, *next_blocked;

	mtx_assert(&lun->lun_lock, MA_OWNED);

	/*
	 * Run forward from the head of the blocked queue, checking each
	 * entry against the I/Os prior to it on the OOA queue to see if
	 * there is still any blockage.
	 *
	 * We cannot use the TAILQ_FOREACH() macro, because it can't deal
	 * with our removing a variable on it while it is traversing the
	 * list.
	 */
	for (cur_blocked = (union ctl_io *)TAILQ_FIRST(&lun->blocked_queue);
	     cur_blocked != NULL; cur_blocked = next_blocked) {
		union ctl_io *prev_ooa;
		ctl_action action;

		next_blocked = (union ctl_io *)TAILQ_NEXT(&cur_blocked->io_hdr,
							  blocked_links);

		prev_ooa = (union ctl_io *)TAILQ_PREV(&cur_blocked->io_hdr,
						      ctl_ooaq, ooa_links);

		/*
		 * If cur_blocked happens to be the first item in the OOA
		 * queue now, prev_ooa will be NULL, and the action
		 * returned will just be CTL_ACTION_PASS.
		 */
		action = ctl_check_ooa(lun, cur_blocked, prev_ooa);

		switch (action) {
		case CTL_ACTION_BLOCK:
			/* Nothing to do here, still blocked */
			break;
		case CTL_ACTION_OVERLAP:
		case CTL_ACTION_OVERLAP_TAG:
			/*
			 * This shouldn't happen!  In theory we've already
			 * checked this command for overlap...
			 */
			break;
		case CTL_ACTION_PASS:
		case CTL_ACTION_SKIP: {
			const struct ctl_cmd_entry *entry;
			int isc_retval;

			/*
			 * The skip case shouldn't happen, this transaction
			 * should have never made it onto the blocked queue.
			 */
			/*
			 * This I/O is no longer blocked, we can remove it
			 * from the blocked queue.  Since this is a TAILQ
			 * (doubly linked list), we can do O(1) removals
			 * from any place on the list.
			 */
			TAILQ_REMOVE(&lun->blocked_queue, &cur_blocked->io_hdr,
				     blocked_links);
			cur_blocked->io_hdr.flags &= ~CTL_FLAG_BLOCKED;

			if (cur_blocked->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC){
				/*
				 * Need to send IO back to original side to
				 * run
				 */
				union ctl_ha_msg msg_info;

				msg_info.hdr.original_sc =
					cur_blocked->io_hdr.original_sc;
				msg_info.hdr.serializing_sc = cur_blocked;
				msg_info.hdr.msg_type = CTL_MSG_R2R;
				if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL,
				     &msg_info, sizeof(msg_info), 0)) >
				     CTL_HA_STATUS_SUCCESS) {
					printf("CTL:Check Blocked error from "
					       "ctl_ha_msg_send %d\n",
					       isc_retval);
				}
				break;
			}
			entry = ctl_get_cmd_entry(&cur_blocked->scsiio, NULL);

			/*
			 * Check this I/O for LUN state changes that may
			 * have happened while this command was blocked.
			 * The LUN state may have been changed by a command
			 * ahead of us in the queue, so we need to re-check
			 * for any states that can be caused by SCSI
			 * commands.
			 */
			if (ctl_scsiio_lun_check(lun, entry,
						 &cur_blocked->scsiio) == 0) {
				cur_blocked->io_hdr.flags |=
				                      CTL_FLAG_IS_WAS_ON_RTR;
				ctl_enqueue_rtr(cur_blocked);
			} else
				ctl_done(cur_blocked);
			break;
		}
		default:
			/*
			 * This probably shouldn't happen -- we shouldn't
			 * get CTL_ACTION_ERROR, or anything else.
			 */
			break;
		}
	}

	return (CTL_RETVAL_COMPLETE);
}

/*
 * This routine (with one exception) checks LUN flags that can be set by
 * commands ahead of us in the OOA queue.  These flags have to be checked
 * when a command initially comes in, and when we pull a command off the
 * blocked queue and are preparing to execute it.  The reason we have to
 * check these flags for commands on the blocked queue is that the LUN
 * state may have been changed by a command ahead of us while we're on the
 * blocked queue.
 *
 * Ordering is somewhat important with these checks, so please pay
 * careful attention to the placement of any new checks.
 */
static int
ctl_scsiio_lun_check(struct ctl_lun *lun,
    const struct ctl_cmd_entry *entry, struct ctl_scsiio *ctsio)
{
	struct ctl_softc *softc = lun->ctl_softc;
	int retval;
	uint32_t residx;

	retval = 0;

	mtx_assert(&lun->lun_lock, MA_OWNED);

	/*
	 * If this shelf is a secondary shelf controller, we have to reject
	 * any media access commands.
	 */
	if ((softc->flags & CTL_FLAG_ACTIVE_SHELF) == 0 &&
	    (entry->flags & CTL_CMD_FLAG_OK_ON_SECONDARY) == 0) {
		ctl_set_lun_standby(ctsio);
		retval = 1;
		goto bailout;
	}

	if (entry->pattern & CTL_LUN_PAT_WRITE) {
		if (lun->flags & CTL_LUN_READONLY) {
			ctl_set_sense(ctsio, /*current_error*/ 1,
			    /*sense_key*/ SSD_KEY_DATA_PROTECT,
			    /*asc*/ 0x27, /*ascq*/ 0x01, SSD_ELEM_NONE);
			retval = 1;
			goto bailout;
		}
		if ((lun->mode_pages.control_page[CTL_PAGE_CURRENT]
		    .eca_and_aen & SCP_SWP) != 0) {
			ctl_set_sense(ctsio, /*current_error*/ 1,
			    /*sense_key*/ SSD_KEY_DATA_PROTECT,
			    /*asc*/ 0x27, /*ascq*/ 0x02, SSD_ELEM_NONE);
			retval = 1;
			goto bailout;
		}
	}

	/*
	 * Check for a reservation conflict.  If this command isn't allowed
	 * even on reserved LUNs, and if this initiator isn't the one who
	 * reserved us, reject the command with a reservation conflict.
	 */
	residx = ctl_get_resindex(&ctsio->io_hdr.nexus);
	if ((lun->flags & CTL_LUN_RESERVED)
	 && ((entry->flags & CTL_CMD_FLAG_ALLOW_ON_RESV) == 0)) {
		if (lun->res_idx != residx) {
			ctl_set_reservation_conflict(ctsio);
			retval = 1;
			goto bailout;
		}
	}

	if ((lun->flags & CTL_LUN_PR_RESERVED) == 0 ||
	    (entry->flags & CTL_CMD_FLAG_ALLOW_ON_PR_RESV)) {
		/* No reservation or command is allowed. */;
	} else if ((entry->flags & CTL_CMD_FLAG_ALLOW_ON_PR_WRESV) &&
	    (lun->res_type == SPR_TYPE_WR_EX ||
	     lun->res_type == SPR_TYPE_WR_EX_RO ||
	     lun->res_type == SPR_TYPE_WR_EX_AR)) {
		/* The command is allowed for Write Exclusive resv. */;
	} else {
		/*
		 * if we aren't registered or it's a res holder type
		 * reservation and this isn't the res holder then set a
		 * conflict.
		 */
		if (ctl_get_prkey(lun, residx) == 0
		 || (residx != lun->pr_res_idx && lun->res_type < 4)) {
			ctl_set_reservation_conflict(ctsio);
			retval = 1;
			goto bailout;
		}

	}

	if ((lun->flags & CTL_LUN_OFFLINE)
	 && ((entry->flags & CTL_CMD_FLAG_OK_ON_OFFLINE) == 0)) {
		ctl_set_lun_not_ready(ctsio);
		retval = 1;
		goto bailout;
	}

	/*
	 * If the LUN is stopped, see if this particular command is allowed
	 * for a stopped lun.  Otherwise, reject it with 0x04,0x02.
	 */
	if ((lun->flags & CTL_LUN_STOPPED)
	 && ((entry->flags & CTL_CMD_FLAG_OK_ON_STOPPED) == 0)) {
		/* "Logical unit not ready, initializing cmd. required" */
		ctl_set_lun_stopped(ctsio);
		retval = 1;
		goto bailout;
	}

	if ((lun->flags & CTL_LUN_INOPERABLE)
	 && ((entry->flags & CTL_CMD_FLAG_OK_ON_INOPERABLE) == 0)) {
		/* "Medium format corrupted" */
		ctl_set_medium_format_corrupted(ctsio);
		retval = 1;
		goto bailout;
	}

bailout:
	return (retval);

}

static void
ctl_failover_io(union ctl_io *io, int have_lock)
{
	ctl_set_busy(&io->scsiio);
	ctl_done(io);
}

static void
ctl_failover(void)
{
	struct ctl_lun *lun;
	struct ctl_softc *softc;
	union ctl_io *next_io, *pending_io;
	union ctl_io *io;
	int lun_idx;

	softc = control_softc;

	mtx_lock(&softc->ctl_lock);
	/*
	 * Remove any cmds from the other SC from the rtr queue.  These
	 * will obviously only be for LUNs for which we're the primary.
	 * We can't send status or get/send data for these commands.
	 * Since they haven't been executed yet, we can just remove them.
	 * We'll either abort them or delete them below, depending on
	 * which HA mode we're in.
	 */
#ifdef notyet
	mtx_lock(&softc->queue_lock);
	for (io = (union ctl_io *)STAILQ_FIRST(&softc->rtr_queue);
	     io != NULL; io = next_io) {
		next_io = (union ctl_io *)STAILQ_NEXT(&io->io_hdr, links);
		if (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC)
			STAILQ_REMOVE(&softc->rtr_queue, &io->io_hdr,
				      ctl_io_hdr, links);
	}
	mtx_unlock(&softc->queue_lock);
#endif

	for (lun_idx=0; lun_idx < softc->num_luns; lun_idx++) {
		lun = softc->ctl_luns[lun_idx];
		if (lun==NULL)
			continue;

		/*
		 * Processor LUNs are primary on both sides.
		 * XXX will this always be true?
		 */
		if (lun->be_lun->lun_type == T_PROCESSOR)
			continue;

		if ((lun->flags & CTL_LUN_PRIMARY_SC)
		 && (softc->ha_mode == CTL_HA_MODE_SER_ONLY)) {
			printf("FAILOVER: primary lun %d\n", lun_idx);
		        /*
			 * Remove all commands from the other SC. First from the
			 * blocked queue then from the ooa queue. Once we have
			 * removed them. Call ctl_check_blocked to see if there
			 * is anything that can run.
			 */
			for (io = (union ctl_io *)TAILQ_FIRST(
			     &lun->blocked_queue); io != NULL; io = next_io) {

		        	next_io = (union ctl_io *)TAILQ_NEXT(
				    &io->io_hdr, blocked_links);

				if (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC) {
					TAILQ_REMOVE(&lun->blocked_queue,
						     &io->io_hdr,blocked_links);
					io->io_hdr.flags &= ~CTL_FLAG_BLOCKED;
					TAILQ_REMOVE(&lun->ooa_queue,
						     &io->io_hdr, ooa_links);

					ctl_free_io(io);
				}
			}

			for (io = (union ctl_io *)TAILQ_FIRST(&lun->ooa_queue);
	     		     io != NULL; io = next_io) {

		        	next_io = (union ctl_io *)TAILQ_NEXT(
				    &io->io_hdr, ooa_links);

				if (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC) {

					TAILQ_REMOVE(&lun->ooa_queue,
						&io->io_hdr,
					     	ooa_links);

					ctl_free_io(io);
				}
			}
			ctl_check_blocked(lun);
		} else if ((lun->flags & CTL_LUN_PRIMARY_SC)
			&& (softc->ha_mode == CTL_HA_MODE_XFER)) {

			printf("FAILOVER: primary lun %d\n", lun_idx);
			/*
			 * Abort all commands from the other SC.  We can't
			 * send status back for them now.  These should get
			 * cleaned up when they are completed or come out
			 * for a datamove operation.
			 */
			for (io = (union ctl_io *)TAILQ_FIRST(&lun->ooa_queue);
	     		     io != NULL; io = next_io) {
		        	next_io = (union ctl_io *)TAILQ_NEXT(
					&io->io_hdr, ooa_links);

				if (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC)
					io->io_hdr.flags |= CTL_FLAG_ABORT;
			}
		} else if (((lun->flags & CTL_LUN_PRIMARY_SC) == 0)
			&& (softc->ha_mode == CTL_HA_MODE_XFER)) {

			printf("FAILOVER: secondary lun %d\n", lun_idx);

			lun->flags |= CTL_LUN_PRIMARY_SC;

			/*
			 * We send all I/O that was sent to this controller
			 * and redirected to the other side back with
			 * busy status, and have the initiator retry it.
			 * Figuring out how much data has been transferred,
			 * etc. and picking up where we left off would be 
			 * very tricky.
			 *
			 * XXX KDM need to remove I/O from the blocked
			 * queue as well!
			 */
			for (pending_io = (union ctl_io *)TAILQ_FIRST(
			     &lun->ooa_queue); pending_io != NULL;
			     pending_io = next_io) {

				next_io =  (union ctl_io *)TAILQ_NEXT(
					&pending_io->io_hdr, ooa_links);

				pending_io->io_hdr.flags &=
					~CTL_FLAG_SENT_2OTHER_SC;

				if (pending_io->io_hdr.flags &
				    CTL_FLAG_IO_ACTIVE) {
					pending_io->io_hdr.flags |=
						CTL_FLAG_FAILOVER;
				} else {
					ctl_set_busy(&pending_io->scsiio);
					ctl_done(pending_io);
				}
			}

			ctl_est_ua_all(lun, -1, CTL_UA_ASYM_ACC_CHANGE);
		} else if (((lun->flags & CTL_LUN_PRIMARY_SC) == 0)
			&& (softc->ha_mode == CTL_HA_MODE_SER_ONLY)) {
			printf("FAILOVER: secondary lun %d\n", lun_idx);
			/*
			 * if the first io on the OOA is not on the RtR queue
			 * add it.
			 */
			lun->flags |= CTL_LUN_PRIMARY_SC;

			pending_io = (union ctl_io *)TAILQ_FIRST(
			    &lun->ooa_queue);
			if (pending_io==NULL) {
				printf("Nothing on OOA queue\n");
				continue;
			}

			pending_io->io_hdr.flags &= ~CTL_FLAG_SENT_2OTHER_SC;
			if ((pending_io->io_hdr.flags &
			     CTL_FLAG_IS_WAS_ON_RTR) == 0) {
				pending_io->io_hdr.flags |=
				    CTL_FLAG_IS_WAS_ON_RTR;
				ctl_enqueue_rtr(pending_io);
			}
#if 0
			else
			{
				printf("Tag 0x%04x is running\n",
				      pending_io->scsiio.tag_num);
			}
#endif

			next_io = (union ctl_io *)TAILQ_NEXT(
			    &pending_io->io_hdr, ooa_links);
			for (pending_io=next_io; pending_io != NULL;
			     pending_io = next_io) {
				pending_io->io_hdr.flags &=
				    ~CTL_FLAG_SENT_2OTHER_SC;
				next_io = (union ctl_io *)TAILQ_NEXT(
					&pending_io->io_hdr, ooa_links);
				if (pending_io->io_hdr.flags &
				    CTL_FLAG_IS_WAS_ON_RTR) {
#if 0
				        printf("Tag 0x%04x is running\n",
				      		pending_io->scsiio.tag_num);
#endif
					continue;
				}

				switch (ctl_check_ooa(lun, pending_io,
			            (union ctl_io *)TAILQ_PREV(
				    &pending_io->io_hdr, ctl_ooaq,
				    ooa_links))) {

				case CTL_ACTION_BLOCK:
					TAILQ_INSERT_TAIL(&lun->blocked_queue,
							  &pending_io->io_hdr,
							  blocked_links);
					pending_io->io_hdr.flags |=
					    CTL_FLAG_BLOCKED;
					break;
				case CTL_ACTION_PASS:
				case CTL_ACTION_SKIP:
					pending_io->io_hdr.flags |=
					    CTL_FLAG_IS_WAS_ON_RTR;
					ctl_enqueue_rtr(pending_io);
					break;
				case CTL_ACTION_OVERLAP:
					ctl_set_overlapped_cmd(
					    (struct ctl_scsiio *)pending_io);
					ctl_done(pending_io);
					break;
				case CTL_ACTION_OVERLAP_TAG:
					ctl_set_overlapped_tag(
					    (struct ctl_scsiio *)pending_io,
					    pending_io->scsiio.tag_num & 0xff);
					ctl_done(pending_io);
					break;
				case CTL_ACTION_ERROR:
				default:
					ctl_set_internal_failure(
						(struct ctl_scsiio *)pending_io,
						0,  // sks_valid
						0); //retry count
					ctl_done(pending_io);
					break;
				}
			}

			ctl_est_ua_all(lun, -1, CTL_UA_ASYM_ACC_CHANGE);
		} else {
			panic("Unhandled HA mode failover, LUN flags = %#x, "
			      "ha_mode = #%x", lun->flags, softc->ha_mode);
		}
	}
	ctl_pause_rtr = 0;
	mtx_unlock(&softc->ctl_lock);
}

static void
ctl_clear_ua(struct ctl_softc *ctl_softc, uint32_t initidx,
	     ctl_ua_type ua_type)
{
	struct ctl_lun *lun;
	ctl_ua_type *pu;

	mtx_assert(&ctl_softc->ctl_lock, MA_OWNED);

	STAILQ_FOREACH(lun, &ctl_softc->lun_list, links) {
		mtx_lock(&lun->lun_lock);
		pu = lun->pending_ua[initidx / CTL_MAX_INIT_PER_PORT];
		if (pu != NULL)
			pu[initidx % CTL_MAX_INIT_PER_PORT] &= ~ua_type;
		mtx_unlock(&lun->lun_lock);
	}
}

static int
ctl_scsiio_precheck(struct ctl_softc *softc, struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	const struct ctl_cmd_entry *entry;
	uint32_t initidx, targ_lun;
	int retval;

	retval = 0;

	lun = NULL;

	targ_lun = ctsio->io_hdr.nexus.targ_mapped_lun;
	if ((targ_lun < CTL_MAX_LUNS)
	 && ((lun = softc->ctl_luns[targ_lun]) != NULL)) {
		/*
		 * If the LUN is invalid, pretend that it doesn't exist.
		 * It will go away as soon as all pending I/O has been
		 * completed.
		 */
		mtx_lock(&lun->lun_lock);
		if (lun->flags & CTL_LUN_DISABLED) {
			mtx_unlock(&lun->lun_lock);
			lun = NULL;
			ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr = NULL;
			ctsio->io_hdr.ctl_private[CTL_PRIV_BACKEND_LUN].ptr = NULL;
		} else {
			ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr = lun;
			ctsio->io_hdr.ctl_private[CTL_PRIV_BACKEND_LUN].ptr =
				lun->be_lun;
			if (lun->be_lun->lun_type == T_PROCESSOR) {
				ctsio->io_hdr.flags |= CTL_FLAG_CONTROL_DEV;
			}

			/*
			 * Every I/O goes into the OOA queue for a
			 * particular LUN, and stays there until completion.
			 */
#ifdef CTL_TIME_IO
			if (TAILQ_EMPTY(&lun->ooa_queue)) {
				lun->idle_time += getsbinuptime() -
				    lun->last_busy;
			}
#endif
			TAILQ_INSERT_TAIL(&lun->ooa_queue, &ctsio->io_hdr,
			    ooa_links);
		}
	} else {
		ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr = NULL;
		ctsio->io_hdr.ctl_private[CTL_PRIV_BACKEND_LUN].ptr = NULL;
	}

	/* Get command entry and return error if it is unsuppotyed. */
	entry = ctl_validate_command(ctsio);
	if (entry == NULL) {
		if (lun)
			mtx_unlock(&lun->lun_lock);
		return (retval);
	}

	ctsio->io_hdr.flags &= ~CTL_FLAG_DATA_MASK;
	ctsio->io_hdr.flags |= entry->flags & CTL_FLAG_DATA_MASK;

	/*
	 * Check to see whether we can send this command to LUNs that don't
	 * exist.  This should pretty much only be the case for inquiry
	 * and request sense.  Further checks, below, really require having
	 * a LUN, so we can't really check the command anymore.  Just put
	 * it on the rtr queue.
	 */
	if (lun == NULL) {
		if (entry->flags & CTL_CMD_FLAG_OK_ON_ALL_LUNS) {
			ctsio->io_hdr.flags |= CTL_FLAG_IS_WAS_ON_RTR;
			ctl_enqueue_rtr((union ctl_io *)ctsio);
			return (retval);
		}

		ctl_set_unsupported_lun(ctsio);
		ctl_done((union ctl_io *)ctsio);
		CTL_DEBUG_PRINT(("ctl_scsiio_precheck: bailing out due to invalid LUN\n"));
		return (retval);
	} else {
		/*
		 * Make sure we support this particular command on this LUN.
		 * e.g., we don't support writes to the control LUN.
		 */
		if (!ctl_cmd_applicable(lun->be_lun->lun_type, entry)) {
			mtx_unlock(&lun->lun_lock);
			ctl_set_invalid_opcode(ctsio);
			ctl_done((union ctl_io *)ctsio);
			return (retval);
		}
	}

	initidx = ctl_get_initindex(&ctsio->io_hdr.nexus);

#ifdef CTL_WITH_CA
	/*
	 * If we've got a request sense, it'll clear the contingent
	 * allegiance condition.  Otherwise, if we have a CA condition for
	 * this initiator, clear it, because it sent down a command other
	 * than request sense.
	 */
	if ((ctsio->cdb[0] != REQUEST_SENSE)
	 && (ctl_is_set(lun->have_ca, initidx)))
		ctl_clear_mask(lun->have_ca, initidx);
#endif

	/*
	 * If the command has this flag set, it handles its own unit
	 * attention reporting, we shouldn't do anything.  Otherwise we
	 * check for any pending unit attentions, and send them back to the
	 * initiator.  We only do this when a command initially comes in,
	 * not when we pull it off the blocked queue.
	 *
	 * According to SAM-3, section 5.3.2, the order that things get
	 * presented back to the host is basically unit attentions caused
	 * by some sort of reset event, busy status, reservation conflicts
	 * or task set full, and finally any other status.
	 *
	 * One issue here is that some of the unit attentions we report
	 * don't fall into the "reset" category (e.g. "reported luns data
	 * has changed").  So reporting it here, before the reservation
	 * check, may be technically wrong.  I guess the only thing to do
	 * would be to check for and report the reset events here, and then
	 * check for the other unit attention types after we check for a
	 * reservation conflict.
	 *
	 * XXX KDM need to fix this
	 */
	if ((entry->flags & CTL_CMD_FLAG_NO_SENSE) == 0) {
		ctl_ua_type ua_type;
		scsi_sense_data_type sense_format;

		if (lun->flags & CTL_LUN_SENSE_DESC)
			sense_format = SSD_TYPE_DESC;
		else
			sense_format = SSD_TYPE_FIXED;

		ua_type = ctl_build_ua(lun, initidx, &ctsio->sense_data,
		    sense_format);
		if (ua_type != CTL_UA_NONE) {
			mtx_unlock(&lun->lun_lock);
			ctsio->scsi_status = SCSI_STATUS_CHECK_COND;
			ctsio->io_hdr.status = CTL_SCSI_ERROR | CTL_AUTOSENSE;
			ctsio->sense_len = SSD_FULL_SIZE;
			ctl_done((union ctl_io *)ctsio);
			return (retval);
		}
	}


	if (ctl_scsiio_lun_check(lun, entry, ctsio) != 0) {
		mtx_unlock(&lun->lun_lock);
		ctl_done((union ctl_io *)ctsio);
		return (retval);
	}

	/*
	 * XXX CHD this is where we want to send IO to other side if
	 * this LUN is secondary on this SC. We will need to make a copy
	 * of the IO and flag the IO on this side as SENT_2OTHER and the flag
	 * the copy we send as FROM_OTHER.
	 * We also need to stuff the address of the original IO so we can
	 * find it easily. Something similar will need be done on the other
	 * side so when we are done we can find the copy.
	 */
	if ((lun->flags & CTL_LUN_PRIMARY_SC) == 0) {
		union ctl_ha_msg msg_info;
		int isc_retval;

		ctsio->io_hdr.flags |= CTL_FLAG_SENT_2OTHER_SC;

		msg_info.hdr.msg_type = CTL_MSG_SERIALIZE;
		msg_info.hdr.original_sc = (union ctl_io *)ctsio;
#if 0
		printf("1. ctsio %p\n", ctsio);
#endif
		msg_info.hdr.serializing_sc = NULL;
		msg_info.hdr.nexus = ctsio->io_hdr.nexus;
		msg_info.scsi.tag_num = ctsio->tag_num;
		msg_info.scsi.tag_type = ctsio->tag_type;
		memcpy(msg_info.scsi.cdb, ctsio->cdb, CTL_MAX_CDBLEN);

		ctsio->io_hdr.flags &= ~CTL_FLAG_IO_ACTIVE;

		if ((isc_retval=ctl_ha_msg_send(CTL_HA_CHAN_CTL,
		    (void *)&msg_info, sizeof(msg_info), 0)) >
		    CTL_HA_STATUS_SUCCESS) {
			printf("CTL:precheck, ctl_ha_msg_send returned %d\n",
			       isc_retval);
			printf("CTL:opcode is %x\n", ctsio->cdb[0]);
		} else {
#if 0
			printf("CTL:Precheck sent msg, opcode is %x\n",opcode);
#endif
		}

		/*
		 * XXX KDM this I/O is off the incoming queue, but hasn't
		 * been inserted on any other queue.  We may need to come
		 * up with a holding queue while we wait for serialization
		 * so that we have an idea of what we're waiting for from
		 * the other side.
		 */
		mtx_unlock(&lun->lun_lock);
		return (retval);
	}

	switch (ctl_check_ooa(lun, (union ctl_io *)ctsio,
			      (union ctl_io *)TAILQ_PREV(&ctsio->io_hdr,
			      ctl_ooaq, ooa_links))) {
	case CTL_ACTION_BLOCK:
		ctsio->io_hdr.flags |= CTL_FLAG_BLOCKED;
		TAILQ_INSERT_TAIL(&lun->blocked_queue, &ctsio->io_hdr,
				  blocked_links);
		mtx_unlock(&lun->lun_lock);
		return (retval);
	case CTL_ACTION_PASS:
	case CTL_ACTION_SKIP:
		ctsio->io_hdr.flags |= CTL_FLAG_IS_WAS_ON_RTR;
		mtx_unlock(&lun->lun_lock);
		ctl_enqueue_rtr((union ctl_io *)ctsio);
		break;
	case CTL_ACTION_OVERLAP:
		mtx_unlock(&lun->lun_lock);
		ctl_set_overlapped_cmd(ctsio);
		ctl_done((union ctl_io *)ctsio);
		break;
	case CTL_ACTION_OVERLAP_TAG:
		mtx_unlock(&lun->lun_lock);
		ctl_set_overlapped_tag(ctsio, ctsio->tag_num & 0xff);
		ctl_done((union ctl_io *)ctsio);
		break;
	case CTL_ACTION_ERROR:
	default:
		mtx_unlock(&lun->lun_lock);
		ctl_set_internal_failure(ctsio,
					 /*sks_valid*/ 0,
					 /*retry_count*/ 0);
		ctl_done((union ctl_io *)ctsio);
		break;
	}
	return (retval);
}

const struct ctl_cmd_entry *
ctl_get_cmd_entry(struct ctl_scsiio *ctsio, int *sa)
{
	const struct ctl_cmd_entry *entry;
	int service_action;

	entry = &ctl_cmd_table[ctsio->cdb[0]];
	if (sa)
		*sa = ((entry->flags & CTL_CMD_FLAG_SA5) != 0);
	if (entry->flags & CTL_CMD_FLAG_SA5) {
		service_action = ctsio->cdb[1] & SERVICE_ACTION_MASK;
		entry = &((const struct ctl_cmd_entry *)
		    entry->execute)[service_action];
	}
	return (entry);
}

const struct ctl_cmd_entry *
ctl_validate_command(struct ctl_scsiio *ctsio)
{
	const struct ctl_cmd_entry *entry;
	int i, sa;
	uint8_t diff;

	entry = ctl_get_cmd_entry(ctsio, &sa);
	if (entry->execute == NULL) {
		if (sa)
			ctl_set_invalid_field(ctsio,
					      /*sks_valid*/ 1,
					      /*command*/ 1,
					      /*field*/ 1,
					      /*bit_valid*/ 1,
					      /*bit*/ 4);
		else
			ctl_set_invalid_opcode(ctsio);
		ctl_done((union ctl_io *)ctsio);
		return (NULL);
	}
	KASSERT(entry->length > 0,
	    ("Not defined length for command 0x%02x/0x%02x",
	     ctsio->cdb[0], ctsio->cdb[1]));
	for (i = 1; i < entry->length; i++) {
		diff = ctsio->cdb[i] & ~entry->usage[i - 1];
		if (diff == 0)
			continue;
		ctl_set_invalid_field(ctsio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ i,
				      /*bit_valid*/ 1,
				      /*bit*/ fls(diff) - 1);
		ctl_done((union ctl_io *)ctsio);
		return (NULL);
	}
	return (entry);
}

static int
ctl_cmd_applicable(uint8_t lun_type, const struct ctl_cmd_entry *entry)
{

	switch (lun_type) {
	case T_PROCESSOR:
		if (((entry->flags & CTL_CMD_FLAG_OK_ON_PROC) == 0) &&
		    ((entry->flags & CTL_CMD_FLAG_OK_ON_ALL_LUNS) == 0))
			return (0);
		break;
	case T_DIRECT:
		if (((entry->flags & CTL_CMD_FLAG_OK_ON_SLUN) == 0) &&
		    ((entry->flags & CTL_CMD_FLAG_OK_ON_ALL_LUNS) == 0))
			return (0);
		break;
	default:
		return (0);
	}
	return (1);
}

static int
ctl_scsiio(struct ctl_scsiio *ctsio)
{
	int retval;
	const struct ctl_cmd_entry *entry;

	retval = CTL_RETVAL_COMPLETE;

	CTL_DEBUG_PRINT(("ctl_scsiio cdb[0]=%02X\n", ctsio->cdb[0]));

	entry = ctl_get_cmd_entry(ctsio, NULL);

	/*
	 * If this I/O has been aborted, just send it straight to
	 * ctl_done() without executing it.
	 */
	if (ctsio->io_hdr.flags & CTL_FLAG_ABORT) {
		ctl_done((union ctl_io *)ctsio);
		goto bailout;
	}

	/*
	 * All the checks should have been handled by ctl_scsiio_precheck().
	 * We should be clear now to just execute the I/O.
	 */
	retval = entry->execute(ctsio);

bailout:
	return (retval);
}

/*
 * Since we only implement one target right now, a bus reset simply resets
 * our single target.
 */
static int
ctl_bus_reset(struct ctl_softc *softc, union ctl_io *io)
{
	return(ctl_target_reset(softc, io, CTL_UA_BUS_RESET));
}

static int
ctl_target_reset(struct ctl_softc *softc, union ctl_io *io,
		 ctl_ua_type ua_type)
{
	struct ctl_lun *lun;
	int retval;

	if (!(io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC)) {
		union ctl_ha_msg msg_info;

		io->io_hdr.flags |= CTL_FLAG_SENT_2OTHER_SC;
		msg_info.hdr.nexus = io->io_hdr.nexus;
		if (ua_type==CTL_UA_TARG_RESET)
			msg_info.task.task_action = CTL_TASK_TARGET_RESET;
		else
			msg_info.task.task_action = CTL_TASK_BUS_RESET;
		msg_info.hdr.msg_type = CTL_MSG_MANAGE_TASKS;
		msg_info.hdr.original_sc = NULL;
		msg_info.hdr.serializing_sc = NULL;
		if (CTL_HA_STATUS_SUCCESS != ctl_ha_msg_send(CTL_HA_CHAN_CTL,
		    (void *)&msg_info, sizeof(msg_info), 0)) {
		}
	}
	retval = 0;

	mtx_lock(&softc->ctl_lock);
	STAILQ_FOREACH(lun, &softc->lun_list, links)
		retval += ctl_lun_reset(lun, io, ua_type);
	mtx_unlock(&softc->ctl_lock);

	return (retval);
}

/*
 * The LUN should always be set.  The I/O is optional, and is used to
 * distinguish between I/Os sent by this initiator, and by other
 * initiators.  We set unit attention for initiators other than this one.
 * SAM-3 is vague on this point.  It does say that a unit attention should
 * be established for other initiators when a LUN is reset (see section
 * 5.7.3), but it doesn't specifically say that the unit attention should
 * be established for this particular initiator when a LUN is reset.  Here
 * is the relevant text, from SAM-3 rev 8:
 *
 * 5.7.2 When a SCSI initiator port aborts its own tasks
 *
 * When a SCSI initiator port causes its own task(s) to be aborted, no
 * notification that the task(s) have been aborted shall be returned to
 * the SCSI initiator port other than the completion response for the
 * command or task management function action that caused the task(s) to
 * be aborted and notification(s) associated with related effects of the
 * action (e.g., a reset unit attention condition).
 *
 * XXX KDM for now, we're setting unit attention for all initiators.
 */
static int
ctl_lun_reset(struct ctl_lun *lun, union ctl_io *io, ctl_ua_type ua_type)
{
	union ctl_io *xio;
#if 0
	uint32_t initidx;
#endif
#ifdef CTL_WITH_CA
	int i;
#endif

	mtx_lock(&lun->lun_lock);
	/*
	 * Run through the OOA queue and abort each I/O.
	 */
	for (xio = (union ctl_io *)TAILQ_FIRST(&lun->ooa_queue); xio != NULL;
	     xio = (union ctl_io *)TAILQ_NEXT(&xio->io_hdr, ooa_links)) {
		xio->io_hdr.flags |= CTL_FLAG_ABORT | CTL_FLAG_ABORT_STATUS;
	}

	/*
	 * This version sets unit attention for every
	 */
#if 0
	initidx = ctl_get_initindex(&io->io_hdr.nexus);
	ctl_est_ua_all(lun, initidx, ua_type);
#else
	ctl_est_ua_all(lun, -1, ua_type);
#endif

	/*
	 * A reset (any kind, really) clears reservations established with
	 * RESERVE/RELEASE.  It does not clear reservations established
	 * with PERSISTENT RESERVE OUT, but we don't support that at the
	 * moment anyway.  See SPC-2, section 5.6.  SPC-3 doesn't address
	 * reservations made with the RESERVE/RELEASE commands, because
	 * those commands are obsolete in SPC-3.
	 */
	lun->flags &= ~CTL_LUN_RESERVED;

#ifdef CTL_WITH_CA
	for (i = 0; i < CTL_MAX_INITIATORS; i++)
		ctl_clear_mask(lun->have_ca, i);
#endif
	mtx_unlock(&lun->lun_lock);

	return (0);
}

static void
ctl_abort_tasks_lun(struct ctl_lun *lun, uint32_t targ_port, uint32_t init_id,
    int other_sc)
{
	union ctl_io *xio;

	mtx_assert(&lun->lun_lock, MA_OWNED);

	/*
	 * Run through the OOA queue and attempt to find the given I/O.
	 * The target port, initiator ID, tag type and tag number have to
	 * match the values that we got from the initiator.  If we have an
	 * untagged command to abort, simply abort the first untagged command
	 * we come to.  We only allow one untagged command at a time of course.
	 */
	for (xio = (union ctl_io *)TAILQ_FIRST(&lun->ooa_queue); xio != NULL;
	     xio = (union ctl_io *)TAILQ_NEXT(&xio->io_hdr, ooa_links)) {

		if ((targ_port == UINT32_MAX ||
		     targ_port == xio->io_hdr.nexus.targ_port) &&
		    (init_id == UINT32_MAX ||
		     init_id == xio->io_hdr.nexus.initid.id)) {
			if (targ_port != xio->io_hdr.nexus.targ_port ||
			    init_id != xio->io_hdr.nexus.initid.id)
				xio->io_hdr.flags |= CTL_FLAG_ABORT_STATUS;
			xio->io_hdr.flags |= CTL_FLAG_ABORT;
			if (!other_sc && !(lun->flags & CTL_LUN_PRIMARY_SC)) {
				union ctl_ha_msg msg_info;

				msg_info.hdr.nexus = xio->io_hdr.nexus;
				msg_info.task.task_action = CTL_TASK_ABORT_TASK;
				msg_info.task.tag_num = xio->scsiio.tag_num;
				msg_info.task.tag_type = xio->scsiio.tag_type;
				msg_info.hdr.msg_type = CTL_MSG_MANAGE_TASKS;
				msg_info.hdr.original_sc = NULL;
				msg_info.hdr.serializing_sc = NULL;
				ctl_ha_msg_send(CTL_HA_CHAN_CTL,
				    (void *)&msg_info, sizeof(msg_info), 0);
			}
		}
	}
}

static int
ctl_abort_task_set(union ctl_io *io)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_lun *lun;
	uint32_t targ_lun;

	/*
	 * Look up the LUN.
	 */
	targ_lun = io->io_hdr.nexus.targ_mapped_lun;
	mtx_lock(&softc->ctl_lock);
	if ((targ_lun < CTL_MAX_LUNS) && (softc->ctl_luns[targ_lun] != NULL))
		lun = softc->ctl_luns[targ_lun];
	else {
		mtx_unlock(&softc->ctl_lock);
		return (1);
	}

	mtx_lock(&lun->lun_lock);
	mtx_unlock(&softc->ctl_lock);
	if (io->taskio.task_action == CTL_TASK_ABORT_TASK_SET) {
		ctl_abort_tasks_lun(lun, io->io_hdr.nexus.targ_port,
		    io->io_hdr.nexus.initid.id,
		    (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC) != 0);
	} else { /* CTL_TASK_CLEAR_TASK_SET */
		ctl_abort_tasks_lun(lun, UINT32_MAX, UINT32_MAX,
		    (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC) != 0);
	}
	mtx_unlock(&lun->lun_lock);
	return (0);
}

static int
ctl_i_t_nexus_reset(union ctl_io *io)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_lun *lun;
	uint32_t initidx, residx;

	initidx = ctl_get_initindex(&io->io_hdr.nexus);
	residx = ctl_get_resindex(&io->io_hdr.nexus);
	mtx_lock(&softc->ctl_lock);
	STAILQ_FOREACH(lun, &softc->lun_list, links) {
		mtx_lock(&lun->lun_lock);
		ctl_abort_tasks_lun(lun, io->io_hdr.nexus.targ_port,
		    io->io_hdr.nexus.initid.id,
		    (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC) != 0);
#ifdef CTL_WITH_CA
		ctl_clear_mask(lun->have_ca, initidx);
#endif
		if ((lun->flags & CTL_LUN_RESERVED) && (lun->res_idx == residx))
			lun->flags &= ~CTL_LUN_RESERVED;
		ctl_est_ua(lun, initidx, CTL_UA_I_T_NEXUS_LOSS);
		mtx_unlock(&lun->lun_lock);
	}
	mtx_unlock(&softc->ctl_lock);
	return (0);
}

static int
ctl_abort_task(union ctl_io *io)
{
	union ctl_io *xio;
	struct ctl_lun *lun;
	struct ctl_softc *softc;
#if 0
	struct sbuf sb;
	char printbuf[128];
#endif
	int found;
	uint32_t targ_lun;

	softc = control_softc;
	found = 0;

	/*
	 * Look up the LUN.
	 */
	targ_lun = io->io_hdr.nexus.targ_mapped_lun;
	mtx_lock(&softc->ctl_lock);
	if ((targ_lun < CTL_MAX_LUNS)
	 && (softc->ctl_luns[targ_lun] != NULL))
		lun = softc->ctl_luns[targ_lun];
	else {
		mtx_unlock(&softc->ctl_lock);
		return (1);
	}

#if 0
	printf("ctl_abort_task: called for lun %lld, tag %d type %d\n",
	       lun->lun, io->taskio.tag_num, io->taskio.tag_type);
#endif

	mtx_lock(&lun->lun_lock);
	mtx_unlock(&softc->ctl_lock);
	/*
	 * Run through the OOA queue and attempt to find the given I/O.
	 * The target port, initiator ID, tag type and tag number have to
	 * match the values that we got from the initiator.  If we have an
	 * untagged command to abort, simply abort the first untagged command
	 * we come to.  We only allow one untagged command at a time of course.
	 */
	for (xio = (union ctl_io *)TAILQ_FIRST(&lun->ooa_queue); xio != NULL;
	     xio = (union ctl_io *)TAILQ_NEXT(&xio->io_hdr, ooa_links)) {
#if 0
		sbuf_new(&sb, printbuf, sizeof(printbuf), SBUF_FIXEDLEN);

		sbuf_printf(&sb, "LUN %lld tag %d type %d%s%s%s%s: ",
			    lun->lun, xio->scsiio.tag_num,
			    xio->scsiio.tag_type,
			    (xio->io_hdr.blocked_links.tqe_prev
			    == NULL) ? "" : " BLOCKED",
			    (xio->io_hdr.flags &
			    CTL_FLAG_DMA_INPROG) ? " DMA" : "",
			    (xio->io_hdr.flags &
			    CTL_FLAG_ABORT) ? " ABORT" : "",
			    (xio->io_hdr.flags &
			    CTL_FLAG_IS_WAS_ON_RTR ? " RTR" : ""));
		ctl_scsi_command_string(&xio->scsiio, NULL, &sb);
		sbuf_finish(&sb);
		printf("%s\n", sbuf_data(&sb));
#endif

		if ((xio->io_hdr.nexus.targ_port != io->io_hdr.nexus.targ_port)
		 || (xio->io_hdr.nexus.initid.id != io->io_hdr.nexus.initid.id)
		 || (xio->io_hdr.flags & CTL_FLAG_ABORT))
			continue;

		/*
		 * If the abort says that the task is untagged, the
		 * task in the queue must be untagged.  Otherwise,
		 * we just check to see whether the tag numbers
		 * match.  This is because the QLogic firmware
		 * doesn't pass back the tag type in an abort
		 * request.
		 */
#if 0
		if (((xio->scsiio.tag_type == CTL_TAG_UNTAGGED)
		  && (io->taskio.tag_type == CTL_TAG_UNTAGGED))
		 || (xio->scsiio.tag_num == io->taskio.tag_num))
#endif
		/*
		 * XXX KDM we've got problems with FC, because it
		 * doesn't send down a tag type with aborts.  So we
		 * can only really go by the tag number...
		 * This may cause problems with parallel SCSI.
		 * Need to figure that out!!
		 */
		if (xio->scsiio.tag_num == io->taskio.tag_num) {
			xio->io_hdr.flags |= CTL_FLAG_ABORT;
			found = 1;
			if ((io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC) == 0 &&
			    !(lun->flags & CTL_LUN_PRIMARY_SC)) {
				union ctl_ha_msg msg_info;

				io->io_hdr.flags |= CTL_FLAG_SENT_2OTHER_SC;
				msg_info.hdr.nexus = io->io_hdr.nexus;
				msg_info.task.task_action = CTL_TASK_ABORT_TASK;
				msg_info.task.tag_num = io->taskio.tag_num;
				msg_info.task.tag_type = io->taskio.tag_type;
				msg_info.hdr.msg_type = CTL_MSG_MANAGE_TASKS;
				msg_info.hdr.original_sc = NULL;
				msg_info.hdr.serializing_sc = NULL;
#if 0
				printf("Sent Abort to other side\n");
#endif
				if (ctl_ha_msg_send(CTL_HA_CHAN_CTL,
				    (void *)&msg_info, sizeof(msg_info), 0) !=
				    CTL_HA_STATUS_SUCCESS) {
				}
			}
#if 0
			printf("ctl_abort_task: found I/O to abort\n");
#endif
		}
	}
	mtx_unlock(&lun->lun_lock);

	if (found == 0) {
		/*
		 * This isn't really an error.  It's entirely possible for
		 * the abort and command completion to cross on the wire.
		 * This is more of an informative/diagnostic error.
		 */
#if 0
		printf("ctl_abort_task: ABORT sent for nonexistent I/O: "
		       "%d:%d:%d:%d tag %d type %d\n",
		       io->io_hdr.nexus.initid.id,
		       io->io_hdr.nexus.targ_port,
		       io->io_hdr.nexus.targ_target.id,
		       io->io_hdr.nexus.targ_lun, io->taskio.tag_num,
		       io->taskio.tag_type);
#endif
	}
	return (0);
}

static void
ctl_run_task(union ctl_io *io)
{
	struct ctl_softc *softc = control_softc;
	int retval = 1;
	const char *task_desc;

	CTL_DEBUG_PRINT(("ctl_run_task\n"));

	KASSERT(io->io_hdr.io_type == CTL_IO_TASK,
	    ("ctl_run_task: Unextected io_type %d\n",
	     io->io_hdr.io_type));

	task_desc = ctl_scsi_task_string(&io->taskio);
	if (task_desc != NULL) {
#ifdef NEEDTOPORT
		csevent_log(CSC_CTL | CSC_SHELF_SW |
			    CTL_TASK_REPORT,
			    csevent_LogType_Trace,
			    csevent_Severity_Information,
			    csevent_AlertLevel_Green,
			    csevent_FRU_Firmware,
			    csevent_FRU_Unknown,
			    "CTL: received task: %s",task_desc);
#endif
	} else {
#ifdef NEEDTOPORT
		csevent_log(CSC_CTL | CSC_SHELF_SW |
			    CTL_TASK_REPORT,
			    csevent_LogType_Trace,
			    csevent_Severity_Information,
			    csevent_AlertLevel_Green,
			    csevent_FRU_Firmware,
			    csevent_FRU_Unknown,
			    "CTL: received unknown task "
			    "type: %d (%#x)",
			    io->taskio.task_action,
			    io->taskio.task_action);
#endif
	}
	switch (io->taskio.task_action) {
	case CTL_TASK_ABORT_TASK:
		retval = ctl_abort_task(io);
		break;
	case CTL_TASK_ABORT_TASK_SET:
	case CTL_TASK_CLEAR_TASK_SET:
		retval = ctl_abort_task_set(io);
		break;
	case CTL_TASK_CLEAR_ACA:
		break;
	case CTL_TASK_I_T_NEXUS_RESET:
		retval = ctl_i_t_nexus_reset(io);
		break;
	case CTL_TASK_LUN_RESET: {
		struct ctl_lun *lun;
		uint32_t targ_lun;

		targ_lun = io->io_hdr.nexus.targ_mapped_lun;
		mtx_lock(&softc->ctl_lock);
		if ((targ_lun < CTL_MAX_LUNS)
		 && (softc->ctl_luns[targ_lun] != NULL))
			lun = softc->ctl_luns[targ_lun];
		else {
			mtx_unlock(&softc->ctl_lock);
			retval = 1;
			break;
		}

		if (!(io->io_hdr.flags &
		    CTL_FLAG_FROM_OTHER_SC)) {
			union ctl_ha_msg msg_info;

			io->io_hdr.flags |=
				CTL_FLAG_SENT_2OTHER_SC;
			msg_info.hdr.msg_type =
				CTL_MSG_MANAGE_TASKS;
			msg_info.hdr.nexus = io->io_hdr.nexus;
			msg_info.task.task_action =
				CTL_TASK_LUN_RESET;
			msg_info.hdr.original_sc = NULL;
			msg_info.hdr.serializing_sc = NULL;
			if (CTL_HA_STATUS_SUCCESS !=
			    ctl_ha_msg_send(CTL_HA_CHAN_CTL,
			    (void *)&msg_info,
			    sizeof(msg_info), 0)) {
			}
		}

		retval = ctl_lun_reset(lun, io,
				       CTL_UA_LUN_RESET);
		mtx_unlock(&softc->ctl_lock);
		break;
	}
	case CTL_TASK_TARGET_RESET:
		retval = ctl_target_reset(softc, io, CTL_UA_TARG_RESET);
		break;
	case CTL_TASK_BUS_RESET:
		retval = ctl_bus_reset(softc, io);
		break;
	case CTL_TASK_PORT_LOGIN:
		break;
	case CTL_TASK_PORT_LOGOUT:
		break;
	default:
		printf("ctl_run_task: got unknown task management event %d\n",
		       io->taskio.task_action);
		break;
	}
	if (retval == 0)
		io->io_hdr.status = CTL_SUCCESS;
	else
		io->io_hdr.status = CTL_ERROR;
	ctl_done(io);
}

/*
 * For HA operation.  Handle commands that come in from the other
 * controller.
 */
static void
ctl_handle_isc(union ctl_io *io)
{
	int free_io;
	struct ctl_lun *lun;
	struct ctl_softc *softc;
	uint32_t targ_lun;

	softc = control_softc;

	targ_lun = io->io_hdr.nexus.targ_mapped_lun;
	lun = softc->ctl_luns[targ_lun];

	switch (io->io_hdr.msg_type) {
	case CTL_MSG_SERIALIZE:
		free_io = ctl_serialize_other_sc_cmd(&io->scsiio);
		break;
	case CTL_MSG_R2R: {
		const struct ctl_cmd_entry *entry;

		/*
		 * This is only used in SER_ONLY mode.
		 */
		free_io = 0;
		entry = ctl_get_cmd_entry(&io->scsiio, NULL);
		mtx_lock(&lun->lun_lock);
		if (ctl_scsiio_lun_check(lun,
		    entry, (struct ctl_scsiio *)io) != 0) {
			mtx_unlock(&lun->lun_lock);
			ctl_done(io);
			break;
		}
		io->io_hdr.flags |= CTL_FLAG_IS_WAS_ON_RTR;
		mtx_unlock(&lun->lun_lock);
		ctl_enqueue_rtr(io);
		break;
	}
	case CTL_MSG_FINISH_IO:
		if (softc->ha_mode == CTL_HA_MODE_XFER) {
			free_io = 0;
			ctl_done(io);
		} else {
			free_io = 1;
			mtx_lock(&lun->lun_lock);
			TAILQ_REMOVE(&lun->ooa_queue, &io->io_hdr,
				     ooa_links);
			ctl_check_blocked(lun);
			mtx_unlock(&lun->lun_lock);
		}
		break;
	case CTL_MSG_PERS_ACTION:
		ctl_hndl_per_res_out_on_other_sc(
			(union ctl_ha_msg *)&io->presio.pr_msg);
		free_io = 1;
		break;
	case CTL_MSG_BAD_JUJU:
		free_io = 0;
		ctl_done(io);
		break;
	case CTL_MSG_DATAMOVE:
		/* Only used in XFER mode */
		free_io = 0;
		ctl_datamove_remote(io);
		break;
	case CTL_MSG_DATAMOVE_DONE:
		/* Only used in XFER mode */
		free_io = 0;
		io->scsiio.be_move_done(io);
		break;
	default:
		free_io = 1;
		printf("%s: Invalid message type %d\n",
		       __func__, io->io_hdr.msg_type);
		break;
	}
	if (free_io)
		ctl_free_io(io);

}


/*
 * Returns the match type in the case of a match, or CTL_LUN_PAT_NONE if
 * there is no match.
 */
static ctl_lun_error_pattern
ctl_cmd_pattern_match(struct ctl_scsiio *ctsio, struct ctl_error_desc *desc)
{
	const struct ctl_cmd_entry *entry;
	ctl_lun_error_pattern filtered_pattern, pattern;

	pattern = desc->error_pattern;

	/*
	 * XXX KDM we need more data passed into this function to match a
	 * custom pattern, and we actually need to implement custom pattern
	 * matching.
	 */
	if (pattern & CTL_LUN_PAT_CMD)
		return (CTL_LUN_PAT_CMD);

	if ((pattern & CTL_LUN_PAT_MASK) == CTL_LUN_PAT_ANY)
		return (CTL_LUN_PAT_ANY);

	entry = ctl_get_cmd_entry(ctsio, NULL);

	filtered_pattern = entry->pattern & pattern;

	/*
	 * If the user requested specific flags in the pattern (e.g.
	 * CTL_LUN_PAT_RANGE), make sure the command supports all of those
	 * flags.
	 *
	 * If the user did not specify any flags, it doesn't matter whether
	 * or not the command supports the flags.
	 */
	if ((filtered_pattern & ~CTL_LUN_PAT_MASK) !=
	     (pattern & ~CTL_LUN_PAT_MASK))
		return (CTL_LUN_PAT_NONE);

	/*
	 * If the user asked for a range check, see if the requested LBA
	 * range overlaps with this command's LBA range.
	 */
	if (filtered_pattern & CTL_LUN_PAT_RANGE) {
		uint64_t lba1;
		uint64_t len1;
		ctl_action action;
		int retval;

		retval = ctl_get_lba_len((union ctl_io *)ctsio, &lba1, &len1);
		if (retval != 0)
			return (CTL_LUN_PAT_NONE);

		action = ctl_extent_check_lba(lba1, len1, desc->lba_range.lba,
					      desc->lba_range.len, FALSE);
		/*
		 * A "pass" means that the LBA ranges don't overlap, so
		 * this doesn't match the user's range criteria.
		 */
		if (action == CTL_ACTION_PASS)
			return (CTL_LUN_PAT_NONE);
	}

	return (filtered_pattern);
}

static void
ctl_inject_error(struct ctl_lun *lun, union ctl_io *io)
{
	struct ctl_error_desc *desc, *desc2;

	mtx_assert(&lun->lun_lock, MA_OWNED);

	STAILQ_FOREACH_SAFE(desc, &lun->error_list, links, desc2) {
		ctl_lun_error_pattern pattern;
		/*
		 * Check to see whether this particular command matches
		 * the pattern in the descriptor.
		 */
		pattern = ctl_cmd_pattern_match(&io->scsiio, desc);
		if ((pattern & CTL_LUN_PAT_MASK) == CTL_LUN_PAT_NONE)
			continue;

		switch (desc->lun_error & CTL_LUN_INJ_TYPE) {
		case CTL_LUN_INJ_ABORTED:
			ctl_set_aborted(&io->scsiio);
			break;
		case CTL_LUN_INJ_MEDIUM_ERR:
			ctl_set_medium_error(&io->scsiio);
			break;
		case CTL_LUN_INJ_UA:
			/* 29h/00h  POWER ON, RESET, OR BUS DEVICE RESET
			 * OCCURRED */
			ctl_set_ua(&io->scsiio, 0x29, 0x00);
			break;
		case CTL_LUN_INJ_CUSTOM:
			/*
			 * We're assuming the user knows what he is doing.
			 * Just copy the sense information without doing
			 * checks.
			 */
			bcopy(&desc->custom_sense, &io->scsiio.sense_data,
			      MIN(sizeof(desc->custom_sense),
				  sizeof(io->scsiio.sense_data)));
			io->scsiio.scsi_status = SCSI_STATUS_CHECK_COND;
			io->scsiio.sense_len = SSD_FULL_SIZE;
			io->io_hdr.status = CTL_SCSI_ERROR | CTL_AUTOSENSE;
			break;
		case CTL_LUN_INJ_NONE:
		default:
			/*
			 * If this is an error injection type we don't know
			 * about, clear the continuous flag (if it is set)
			 * so it will get deleted below.
			 */
			desc->lun_error &= ~CTL_LUN_INJ_CONTINUOUS;
			break;
		}
		/*
		 * By default, each error injection action is a one-shot
		 */
		if (desc->lun_error & CTL_LUN_INJ_CONTINUOUS)
			continue;

		STAILQ_REMOVE(&lun->error_list, desc, ctl_error_desc, links);

		free(desc, M_CTL);
	}
}

#ifdef CTL_IO_DELAY
static void
ctl_datamove_timer_wakeup(void *arg)
{
	union ctl_io *io;

	io = (union ctl_io *)arg;

	ctl_datamove(io);
}
#endif /* CTL_IO_DELAY */

void
ctl_datamove(union ctl_io *io)
{
	void (*fe_datamove)(union ctl_io *io);

	mtx_assert(&control_softc->ctl_lock, MA_NOTOWNED);

	CTL_DEBUG_PRINT(("ctl_datamove\n"));

#ifdef CTL_TIME_IO
	if ((time_uptime - io->io_hdr.start_time) > ctl_time_io_secs) {
		char str[256];
		char path_str[64];
		struct sbuf sb;

		ctl_scsi_path_string(io, path_str, sizeof(path_str));
		sbuf_new(&sb, str, sizeof(str), SBUF_FIXEDLEN);

		sbuf_cat(&sb, path_str);
		switch (io->io_hdr.io_type) {
		case CTL_IO_SCSI:
			ctl_scsi_command_string(&io->scsiio, NULL, &sb);
			sbuf_printf(&sb, "\n");
			sbuf_cat(&sb, path_str);
			sbuf_printf(&sb, "Tag: 0x%04x, type %d\n",
				    io->scsiio.tag_num, io->scsiio.tag_type);
			break;
		case CTL_IO_TASK:
			sbuf_printf(&sb, "Task I/O type: %d, Tag: 0x%04x, "
				    "Tag Type: %d\n", io->taskio.task_action,
				    io->taskio.tag_num, io->taskio.tag_type);
			break;
		default:
			printf("Invalid CTL I/O type %d\n", io->io_hdr.io_type);
			panic("Invalid CTL I/O type %d\n", io->io_hdr.io_type);
			break;
		}
		sbuf_cat(&sb, path_str);
		sbuf_printf(&sb, "ctl_datamove: %jd seconds\n",
			    (intmax_t)time_uptime - io->io_hdr.start_time);
		sbuf_finish(&sb);
		printf("%s", sbuf_data(&sb));
	}
#endif /* CTL_TIME_IO */

#ifdef CTL_IO_DELAY
	if (io->io_hdr.flags & CTL_FLAG_DELAY_DONE) {
		struct ctl_lun *lun;

		lun =(struct ctl_lun *)io->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

		io->io_hdr.flags &= ~CTL_FLAG_DELAY_DONE;
	} else {
		struct ctl_lun *lun;

		lun =(struct ctl_lun *)io->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
		if ((lun != NULL)
		 && (lun->delay_info.datamove_delay > 0)) {
			struct callout *callout;

			callout = (struct callout *)&io->io_hdr.timer_bytes;
			callout_init(callout, /*mpsafe*/ 1);
			io->io_hdr.flags |= CTL_FLAG_DELAY_DONE;
			callout_reset(callout,
				      lun->delay_info.datamove_delay * hz,
				      ctl_datamove_timer_wakeup, io);
			if (lun->delay_info.datamove_type ==
			    CTL_DELAY_TYPE_ONESHOT)
				lun->delay_info.datamove_delay = 0;
			return;
		}
	}
#endif

	/*
	 * This command has been aborted.  Set the port status, so we fail
	 * the data move.
	 */
	if (io->io_hdr.flags & CTL_FLAG_ABORT) {
		printf("ctl_datamove: tag 0x%04x on (%ju:%d:%ju:%d) aborted\n",
		       io->scsiio.tag_num,(uintmax_t)io->io_hdr.nexus.initid.id,
		       io->io_hdr.nexus.targ_port,
		       (uintmax_t)io->io_hdr.nexus.targ_target.id,
		       io->io_hdr.nexus.targ_lun);
		io->io_hdr.port_status = 31337;
		/*
		 * Note that the backend, in this case, will get the
		 * callback in its context.  In other cases it may get
		 * called in the frontend's interrupt thread context.
		 */
		io->scsiio.be_move_done(io);
		return;
	}

	/* Don't confuse frontend with zero length data move. */
	if (io->scsiio.kern_data_len == 0) {
		io->scsiio.be_move_done(io);
		return;
	}

	/*
	 * If we're in XFER mode and this I/O is from the other shelf
	 * controller, we need to send the DMA to the other side to
	 * actually transfer the data to/from the host.  In serialize only
	 * mode the transfer happens below CTL and ctl_datamove() is only
	 * called on the machine that originally received the I/O.
	 */
	if ((control_softc->ha_mode == CTL_HA_MODE_XFER)
	 && (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC)) {
		union ctl_ha_msg msg;
		uint32_t sg_entries_sent;
		int do_sg_copy;
		int i;

		memset(&msg, 0, sizeof(msg));
		msg.hdr.msg_type = CTL_MSG_DATAMOVE;
		msg.hdr.original_sc = io->io_hdr.original_sc;
		msg.hdr.serializing_sc = io;
		msg.hdr.nexus = io->io_hdr.nexus;
		msg.dt.flags = io->io_hdr.flags;
		/*
		 * We convert everything into a S/G list here.  We can't
		 * pass by reference, only by value between controllers.
		 * So we can't pass a pointer to the S/G list, only as many
		 * S/G entries as we can fit in here.  If it's possible for
		 * us to get more than CTL_HA_MAX_SG_ENTRIES S/G entries,
		 * then we need to break this up into multiple transfers.
		 */
		if (io->scsiio.kern_sg_entries == 0) {
			msg.dt.kern_sg_entries = 1;
			/*
			 * If this is in cached memory, flush the cache
			 * before we send the DMA request to the other
			 * controller.  We want to do this in either the
			 * read or the write case.  The read case is
			 * straightforward.  In the write case, we want to
			 * make sure nothing is in the local cache that
			 * could overwrite the DMAed data.
			 */
			if ((io->io_hdr.flags & CTL_FLAG_NO_DATASYNC) == 0) {
				/*
				 * XXX KDM use bus_dmamap_sync() here.
				 */
			}

			/*
			 * Convert to a physical address if this is a
			 * virtual address.
			 */
			if (io->io_hdr.flags & CTL_FLAG_BUS_ADDR) {
				msg.dt.sg_list[0].addr =
					io->scsiio.kern_data_ptr;
			} else {
				/*
				 * XXX KDM use busdma here!
				 */
#if 0
				msg.dt.sg_list[0].addr = (void *)
					vtophys(io->scsiio.kern_data_ptr);
#endif
			}

			msg.dt.sg_list[0].len = io->scsiio.kern_data_len;
			do_sg_copy = 0;
		} else {
			struct ctl_sg_entry *sgl;

			do_sg_copy = 1;
			msg.dt.kern_sg_entries = io->scsiio.kern_sg_entries;
			sgl = (struct ctl_sg_entry *)io->scsiio.kern_data_ptr;
			if ((io->io_hdr.flags & CTL_FLAG_NO_DATASYNC) == 0) {
				/*
				 * XXX KDM use bus_dmamap_sync() here.
				 */
			}
		}

		msg.dt.kern_data_len = io->scsiio.kern_data_len;
		msg.dt.kern_total_len = io->scsiio.kern_total_len;
		msg.dt.kern_data_resid = io->scsiio.kern_data_resid;
		msg.dt.kern_rel_offset = io->scsiio.kern_rel_offset;
		msg.dt.sg_sequence = 0;

		/*
		 * Loop until we've sent all of the S/G entries.  On the
		 * other end, we'll recompose these S/G entries into one
		 * contiguous list before passing it to the
		 */
		for (sg_entries_sent = 0; sg_entries_sent <
		     msg.dt.kern_sg_entries; msg.dt.sg_sequence++) {
			msg.dt.cur_sg_entries = MIN((sizeof(msg.dt.sg_list)/
				sizeof(msg.dt.sg_list[0])),
				msg.dt.kern_sg_entries - sg_entries_sent);

			if (do_sg_copy != 0) {
				struct ctl_sg_entry *sgl;
				int j;

				sgl = (struct ctl_sg_entry *)
					io->scsiio.kern_data_ptr;
				/*
				 * If this is in cached memory, flush the cache
				 * before we send the DMA request to the other
				 * controller.  We want to do this in either
				 * the * read or the write case.  The read
				 * case is straightforward.  In the write
				 * case, we want to make sure nothing is
				 * in the local cache that could overwrite
				 * the DMAed data.
				 */

				for (i = sg_entries_sent, j = 0;
				     i < msg.dt.cur_sg_entries; i++, j++) {
					if ((io->io_hdr.flags &
					     CTL_FLAG_NO_DATASYNC) == 0) {
						/*
						 * XXX KDM use bus_dmamap_sync()
						 */
					}
					if ((io->io_hdr.flags &
					     CTL_FLAG_BUS_ADDR) == 0) {
						/*
						 * XXX KDM use busdma.
						 */
#if 0
						msg.dt.sg_list[j].addr =(void *)
						       vtophys(sgl[i].addr);
#endif
					} else {
						msg.dt.sg_list[j].addr =
							sgl[i].addr;
					}
					msg.dt.sg_list[j].len = sgl[i].len;
				}
			}

			sg_entries_sent += msg.dt.cur_sg_entries;
			if (sg_entries_sent >= msg.dt.kern_sg_entries)
				msg.dt.sg_last = 1;
			else
				msg.dt.sg_last = 0;

			/*
			 * XXX KDM drop and reacquire the lock here?
			 */
			if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg,
			    sizeof(msg), 0) > CTL_HA_STATUS_SUCCESS) {
				/*
				 * XXX do something here.
				 */
			}

			msg.dt.sent_sg_entries = sg_entries_sent;
		}
		io->io_hdr.flags &= ~CTL_FLAG_IO_ACTIVE;
		if (io->io_hdr.flags & CTL_FLAG_FAILOVER)
			ctl_failover_io(io, /*have_lock*/ 0);

	} else {

		/*
		 * Lookup the fe_datamove() function for this particular
		 * front end.
		 */
		fe_datamove =
		    control_softc->ctl_ports[ctl_port_idx(io->io_hdr.nexus.targ_port)]->fe_datamove;

		fe_datamove(io);
	}
}

static void
ctl_send_datamove_done(union ctl_io *io, int have_lock)
{
	union ctl_ha_msg msg;
	int isc_status;

	memset(&msg, 0, sizeof(msg));

	msg.hdr.msg_type = CTL_MSG_DATAMOVE_DONE;
	msg.hdr.original_sc = io;
	msg.hdr.serializing_sc = io->io_hdr.serializing_sc;
	msg.hdr.nexus = io->io_hdr.nexus;
	msg.hdr.status = io->io_hdr.status;
	msg.scsi.tag_num = io->scsiio.tag_num;
	msg.scsi.tag_type = io->scsiio.tag_type;
	msg.scsi.scsi_status = io->scsiio.scsi_status;
	memcpy(&msg.scsi.sense_data, &io->scsiio.sense_data,
	       sizeof(io->scsiio.sense_data));
	msg.scsi.sense_len = io->scsiio.sense_len;
	msg.scsi.sense_residual = io->scsiio.sense_residual;
	msg.scsi.fetd_status = io->io_hdr.port_status;
	msg.scsi.residual = io->scsiio.residual;
	io->io_hdr.flags &= ~CTL_FLAG_IO_ACTIVE;

	if (io->io_hdr.flags & CTL_FLAG_FAILOVER) {
		ctl_failover_io(io, /*have_lock*/ have_lock);
		return;
	}

	isc_status = ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg, sizeof(msg), 0);
	if (isc_status > CTL_HA_STATUS_SUCCESS) {
		/* XXX do something if this fails */
	}

}

/*
 * The DMA to the remote side is done, now we need to tell the other side
 * we're done so it can continue with its data movement.
 */
static void
ctl_datamove_remote_write_cb(struct ctl_ha_dt_req *rq)
{
	union ctl_io *io;

	io = rq->context;

	if (rq->ret != CTL_HA_STATUS_SUCCESS) {
		printf("%s: ISC DMA write failed with error %d", __func__,
		       rq->ret);
		ctl_set_internal_failure(&io->scsiio,
					 /*sks_valid*/ 1,
					 /*retry_count*/ rq->ret);
	}

	ctl_dt_req_free(rq);

	/*
	 * In this case, we had to malloc the memory locally.  Free it.
	 */
	if ((io->io_hdr.flags & CTL_FLAG_AUTO_MIRROR) == 0) {
		int i;
		for (i = 0; i < io->scsiio.kern_sg_entries; i++)
			free(io->io_hdr.local_sglist[i].addr, M_CTL);
	}
	/*
	 * The data is in local and remote memory, so now we need to send
	 * status (good or back) back to the other side.
	 */
	ctl_send_datamove_done(io, /*have_lock*/ 0);
}

/*
 * We've moved the data from the host/controller into local memory.  Now we
 * need to push it over to the remote controller's memory.
 */
static int
ctl_datamove_remote_dm_write_cb(union ctl_io *io)
{
	int retval;

	retval = 0;

	retval = ctl_datamove_remote_xfer(io, CTL_HA_DT_CMD_WRITE,
					  ctl_datamove_remote_write_cb);

	return (retval);
}

static void
ctl_datamove_remote_write(union ctl_io *io)
{
	int retval;
	void (*fe_datamove)(union ctl_io *io);

	/*
	 * - Get the data from the host/HBA into local memory.
	 * - DMA memory from the local controller to the remote controller.
	 * - Send status back to the remote controller.
	 */

	retval = ctl_datamove_remote_sgl_setup(io);
	if (retval != 0)
		return;

	/* Switch the pointer over so the FETD knows what to do */
	io->scsiio.kern_data_ptr = (uint8_t *)io->io_hdr.local_sglist;

	/*
	 * Use a custom move done callback, since we need to send completion
	 * back to the other controller, not to the backend on this side.
	 */
	io->scsiio.be_move_done = ctl_datamove_remote_dm_write_cb;

	fe_datamove = control_softc->ctl_ports[ctl_port_idx(io->io_hdr.nexus.targ_port)]->fe_datamove;

	fe_datamove(io);

	return;

}

static int
ctl_datamove_remote_dm_read_cb(union ctl_io *io)
{
#if 0
	char str[256];
	char path_str[64];
	struct sbuf sb;
#endif

	/*
	 * In this case, we had to malloc the memory locally.  Free it.
	 */
	if ((io->io_hdr.flags & CTL_FLAG_AUTO_MIRROR) == 0) {
		int i;
		for (i = 0; i < io->scsiio.kern_sg_entries; i++)
			free(io->io_hdr.local_sglist[i].addr, M_CTL);
	}

#if 0
	scsi_path_string(io, path_str, sizeof(path_str));
	sbuf_new(&sb, str, sizeof(str), SBUF_FIXEDLEN);
	sbuf_cat(&sb, path_str);
	scsi_command_string(&io->scsiio, NULL, &sb);
	sbuf_printf(&sb, "\n");
	sbuf_cat(&sb, path_str);
	sbuf_printf(&sb, "Tag: 0x%04x, type %d\n",
		    io->scsiio.tag_num, io->scsiio.tag_type);
	sbuf_cat(&sb, path_str);
	sbuf_printf(&sb, "%s: flags %#x, status %#x\n", __func__,
		    io->io_hdr.flags, io->io_hdr.status);
	sbuf_finish(&sb);
	printk("%s", sbuf_data(&sb));
#endif


	/*
	 * The read is done, now we need to send status (good or bad) back
	 * to the other side.
	 */
	ctl_send_datamove_done(io, /*have_lock*/ 0);

	return (0);
}

static void
ctl_datamove_remote_read_cb(struct ctl_ha_dt_req *rq)
{
	union ctl_io *io;
	void (*fe_datamove)(union ctl_io *io);

	io = rq->context;

	if (rq->ret != CTL_HA_STATUS_SUCCESS) {
		printf("%s: ISC DMA read failed with error %d", __func__,
		       rq->ret);
		ctl_set_internal_failure(&io->scsiio,
					 /*sks_valid*/ 1,
					 /*retry_count*/ rq->ret);
	}

	ctl_dt_req_free(rq);

	/* Switch the pointer over so the FETD knows what to do */
	io->scsiio.kern_data_ptr = (uint8_t *)io->io_hdr.local_sglist;

	/*
	 * Use a custom move done callback, since we need to send completion
	 * back to the other controller, not to the backend on this side.
	 */
	io->scsiio.be_move_done = ctl_datamove_remote_dm_read_cb;

	/* XXX KDM add checks like the ones in ctl_datamove? */

	fe_datamove = control_softc->ctl_ports[ctl_port_idx(io->io_hdr.nexus.targ_port)]->fe_datamove;

	fe_datamove(io);
}

static int
ctl_datamove_remote_sgl_setup(union ctl_io *io)
{
	struct ctl_sg_entry *local_sglist, *remote_sglist;
	struct ctl_sg_entry *local_dma_sglist, *remote_dma_sglist;
	struct ctl_softc *softc;
	int retval;
	int i;

	retval = 0;
	softc = control_softc;

	local_sglist = io->io_hdr.local_sglist;
	local_dma_sglist = io->io_hdr.local_dma_sglist;
	remote_sglist = io->io_hdr.remote_sglist;
	remote_dma_sglist = io->io_hdr.remote_dma_sglist;

	if (io->io_hdr.flags & CTL_FLAG_AUTO_MIRROR) {
		for (i = 0; i < io->scsiio.kern_sg_entries; i++) {
			local_sglist[i].len = remote_sglist[i].len;

			/*
			 * XXX Detect the situation where the RS-level I/O
			 * redirector on the other side has already read the
			 * data off of the AOR RS on this side, and
			 * transferred it to remote (mirror) memory on the
			 * other side.  Since we already have the data in
			 * memory here, we just need to use it.
			 *
			 * XXX KDM this can probably be removed once we
			 * get the cache device code in and take the
			 * current AOR implementation out.
			 */
#ifdef NEEDTOPORT
			if ((remote_sglist[i].addr >=
			     (void *)vtophys(softc->mirr->addr))
			 && (remote_sglist[i].addr <
			     ((void *)vtophys(softc->mirr->addr) +
			     CacheMirrorOffset))) {
				local_sglist[i].addr = remote_sglist[i].addr -
					CacheMirrorOffset;
				if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) ==
				     CTL_FLAG_DATA_IN)
					io->io_hdr.flags |= CTL_FLAG_REDIR_DONE;
			} else {
				local_sglist[i].addr = remote_sglist[i].addr +
					CacheMirrorOffset;
			}
#endif
#if 0
			printf("%s: local %p, remote %p, len %d\n",
			       __func__, local_sglist[i].addr,
			       remote_sglist[i].addr, local_sglist[i].len);
#endif
		}
	} else {
		uint32_t len_to_go;

		/*
		 * In this case, we don't have automatically allocated
		 * memory for this I/O on this controller.  This typically
		 * happens with internal CTL I/O -- e.g. inquiry, mode
		 * sense, etc.  Anything coming from RAIDCore will have
		 * a mirror area available.
		 */
		len_to_go = io->scsiio.kern_data_len;

		/*
		 * Clear the no datasync flag, we have to use malloced
		 * buffers.
		 */
		io->io_hdr.flags &= ~CTL_FLAG_NO_DATASYNC;

		/*
		 * The difficult thing here is that the size of the various
		 * S/G segments may be different than the size from the
		 * remote controller.  That'll make it harder when DMAing
		 * the data back to the other side.
		 */
		for (i = 0; (i < sizeof(io->io_hdr.remote_sglist) /
		     sizeof(io->io_hdr.remote_sglist[0])) &&
		     (len_to_go > 0); i++) {
			local_sglist[i].len = MIN(len_to_go, 131072);
			CTL_SIZE_8B(local_dma_sglist[i].len,
				    local_sglist[i].len);
			local_sglist[i].addr =
				malloc(local_dma_sglist[i].len, M_CTL,M_WAITOK);

			local_dma_sglist[i].addr = local_sglist[i].addr;

			if (local_sglist[i].addr == NULL) {
				int j;

				printf("malloc failed for %zd bytes!",
				       local_dma_sglist[i].len);
				for (j = 0; j < i; j++) {
					free(local_sglist[j].addr, M_CTL);
				}
				ctl_set_internal_failure(&io->scsiio,
							 /*sks_valid*/ 1,
							 /*retry_count*/ 4857);
				retval = 1;
				goto bailout_error;
				
			}
			/* XXX KDM do we need a sync here? */

			len_to_go -= local_sglist[i].len;
		}
		/*
		 * Reset the number of S/G entries accordingly.  The
		 * original number of S/G entries is available in
		 * rem_sg_entries.
		 */
		io->scsiio.kern_sg_entries = i;

#if 0
		printf("%s: kern_sg_entries = %d\n", __func__,
		       io->scsiio.kern_sg_entries);
		for (i = 0; i < io->scsiio.kern_sg_entries; i++)
			printf("%s: sg[%d] = %p, %d (DMA: %d)\n", __func__, i,
			       local_sglist[i].addr, local_sglist[i].len,
			       local_dma_sglist[i].len);
#endif
	}


	return (retval);

bailout_error:

	ctl_send_datamove_done(io, /*have_lock*/ 0);

	return (retval);
}

static int
ctl_datamove_remote_xfer(union ctl_io *io, unsigned command,
			 ctl_ha_dt_cb callback)
{
	struct ctl_ha_dt_req *rq;
	struct ctl_sg_entry *remote_sglist, *local_sglist;
	struct ctl_sg_entry *remote_dma_sglist, *local_dma_sglist;
	uint32_t local_used, remote_used, total_used;
	int retval;
	int i, j;

	retval = 0;

	rq = ctl_dt_req_alloc();

	/*
	 * If we failed to allocate the request, and if the DMA didn't fail
	 * anyway, set busy status.  This is just a resource allocation
	 * failure.
	 */
	if ((rq == NULL)
	 && ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE))
		ctl_set_busy(&io->scsiio);

	if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE) {

		if (rq != NULL)
			ctl_dt_req_free(rq);

		/*
		 * The data move failed.  We need to return status back
		 * to the other controller.  No point in trying to DMA
		 * data to the remote controller.
		 */

		ctl_send_datamove_done(io, /*have_lock*/ 0);

		retval = 1;

		goto bailout;
	}

	local_sglist = io->io_hdr.local_sglist;
	local_dma_sglist = io->io_hdr.local_dma_sglist;
	remote_sglist = io->io_hdr.remote_sglist;
	remote_dma_sglist = io->io_hdr.remote_dma_sglist;
	local_used = 0;
	remote_used = 0;
	total_used = 0;

	if (io->io_hdr.flags & CTL_FLAG_REDIR_DONE) {
		rq->ret = CTL_HA_STATUS_SUCCESS;
		rq->context = io;
		callback(rq);
		goto bailout;
	}

	/*
	 * Pull/push the data over the wire from/to the other controller.
	 * This takes into account the possibility that the local and
	 * remote sglists may not be identical in terms of the size of
	 * the elements and the number of elements.
	 *
	 * One fundamental assumption here is that the length allocated for
	 * both the local and remote sglists is identical.  Otherwise, we've
	 * essentially got a coding error of some sort.
	 */
	for (i = 0, j = 0; total_used < io->scsiio.kern_data_len; ) {
		int isc_ret;
		uint32_t cur_len, dma_length;
		uint8_t *tmp_ptr;

		rq->id = CTL_HA_DATA_CTL;
		rq->command = command;
		rq->context = io;

		/*
		 * Both pointers should be aligned.  But it is possible
		 * that the allocation length is not.  They should both
		 * also have enough slack left over at the end, though,
		 * to round up to the next 8 byte boundary.
		 */
		cur_len = MIN(local_sglist[i].len - local_used,
			      remote_sglist[j].len - remote_used);

		/*
		 * In this case, we have a size issue and need to decrease
		 * the size, except in the case where we actually have less
		 * than 8 bytes left.  In that case, we need to increase
		 * the DMA length to get the last bit.
		 */
		if ((cur_len & 0x7) != 0) {
			if (cur_len > 0x7) {
				cur_len = cur_len - (cur_len & 0x7);
				dma_length = cur_len;
			} else {
				CTL_SIZE_8B(dma_length, cur_len);
			}

		} else
			dma_length = cur_len;

		/*
		 * If we had to allocate memory for this I/O, instead of using
		 * the non-cached mirror memory, we'll need to flush the cache
		 * before trying to DMA to the other controller.
		 *
		 * We could end up doing this multiple times for the same
		 * segment if we have a larger local segment than remote
		 * segment.  That shouldn't be an issue.
		 */
		if ((io->io_hdr.flags & CTL_FLAG_NO_DATASYNC) == 0) {
			/*
			 * XXX KDM use bus_dmamap_sync() here.
			 */
		}

		rq->size = dma_length;

		tmp_ptr = (uint8_t *)local_sglist[i].addr;
		tmp_ptr += local_used;

		/* Use physical addresses when talking to ISC hardware */
		if ((io->io_hdr.flags & CTL_FLAG_BUS_ADDR) == 0) {
			/* XXX KDM use busdma */
#if 0
			rq->local = vtophys(tmp_ptr);
#endif
		} else
			rq->local = tmp_ptr;

		tmp_ptr = (uint8_t *)remote_sglist[j].addr;
		tmp_ptr += remote_used;
		rq->remote = tmp_ptr;

		rq->callback = NULL;

		local_used += cur_len;
		if (local_used >= local_sglist[i].len) {
			i++;
			local_used = 0;
		}

		remote_used += cur_len;
		if (remote_used >= remote_sglist[j].len) {
			j++;
			remote_used = 0;
		}
		total_used += cur_len;

		if (total_used >= io->scsiio.kern_data_len)
			rq->callback = callback;

		if ((rq->size & 0x7) != 0) {
			printf("%s: warning: size %d is not on 8b boundary\n",
			       __func__, rq->size);
		}
		if (((uintptr_t)rq->local & 0x7) != 0) {
			printf("%s: warning: local %p not on 8b boundary\n",
			       __func__, rq->local);
		}
		if (((uintptr_t)rq->remote & 0x7) != 0) {
			printf("%s: warning: remote %p not on 8b boundary\n",
			       __func__, rq->local);
		}
#if 0
		printf("%s: %s: local %#x remote %#x size %d\n", __func__,
		       (command == CTL_HA_DT_CMD_WRITE) ? "WRITE" : "READ",
		       rq->local, rq->remote, rq->size);
#endif

		isc_ret = ctl_dt_single(rq);
		if (isc_ret == CTL_HA_STATUS_WAIT)
			continue;

		if (isc_ret == CTL_HA_STATUS_DISCONNECT) {
			rq->ret = CTL_HA_STATUS_SUCCESS;
		} else {
			rq->ret = isc_ret;
		}
		callback(rq);
		goto bailout;
	}

bailout:
	return (retval);

}

static void
ctl_datamove_remote_read(union ctl_io *io)
{
	int retval;
	int i;

	/*
	 * This will send an error to the other controller in the case of a
	 * failure.
	 */
	retval = ctl_datamove_remote_sgl_setup(io);
	if (retval != 0)
		return;

	retval = ctl_datamove_remote_xfer(io, CTL_HA_DT_CMD_READ,
					  ctl_datamove_remote_read_cb);
	if ((retval != 0)
	 && ((io->io_hdr.flags & CTL_FLAG_AUTO_MIRROR) == 0)) {
		/*
		 * Make sure we free memory if there was an error..  The
		 * ctl_datamove_remote_xfer() function will send the
		 * datamove done message, or call the callback with an
		 * error if there is a problem.
		 */
		for (i = 0; i < io->scsiio.kern_sg_entries; i++)
			free(io->io_hdr.local_sglist[i].addr, M_CTL);
	}

	return;
}

/*
 * Process a datamove request from the other controller.  This is used for
 * XFER mode only, not SER_ONLY mode.  For writes, we DMA into local memory
 * first.  Once that is complete, the data gets DMAed into the remote
 * controller's memory.  For reads, we DMA from the remote controller's
 * memory into our memory first, and then move it out to the FETD.
 */
static void
ctl_datamove_remote(union ctl_io *io)
{
	struct ctl_softc *softc;

	softc = control_softc;

	mtx_assert(&softc->ctl_lock, MA_NOTOWNED);

	/*
	 * Note that we look for an aborted I/O here, but don't do some of
	 * the other checks that ctl_datamove() normally does.
	 * We don't need to run the datamove delay code, since that should
	 * have been done if need be on the other controller.
	 */
	if (io->io_hdr.flags & CTL_FLAG_ABORT) {
		printf("%s: tag 0x%04x on (%d:%d:%d:%d) aborted\n", __func__,
		       io->scsiio.tag_num, io->io_hdr.nexus.initid.id,
		       io->io_hdr.nexus.targ_port,
		       io->io_hdr.nexus.targ_target.id,
		       io->io_hdr.nexus.targ_lun);
		io->io_hdr.port_status = 31338;
		ctl_send_datamove_done(io, /*have_lock*/ 0);
		return;
	}

	if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) == CTL_FLAG_DATA_OUT) {
		ctl_datamove_remote_write(io);
	} else if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) == CTL_FLAG_DATA_IN){
		ctl_datamove_remote_read(io);
	} else {
		union ctl_ha_msg msg;
		struct scsi_sense_data *sense;
		uint8_t sks[3];
		int retry_count;

		memset(&msg, 0, sizeof(msg));

		msg.hdr.msg_type = CTL_MSG_BAD_JUJU;
		msg.hdr.status = CTL_SCSI_ERROR;
		msg.scsi.scsi_status = SCSI_STATUS_CHECK_COND;

		retry_count = 4243;

		sense = &msg.scsi.sense_data;
		sks[0] = SSD_SCS_VALID;
		sks[1] = (retry_count >> 8) & 0xff;
		sks[2] = retry_count & 0xff;

		/* "Internal target failure" */
		scsi_set_sense_data(sense,
				    /*sense_format*/ SSD_TYPE_NONE,
				    /*current_error*/ 1,
				    /*sense_key*/ SSD_KEY_HARDWARE_ERROR,
				    /*asc*/ 0x44,
				    /*ascq*/ 0x00,
				    /*type*/ SSD_ELEM_SKS,
				    /*size*/ sizeof(sks),
				    /*data*/ sks,
				    SSD_ELEM_NONE);

		io->io_hdr.flags &= ~CTL_FLAG_IO_ACTIVE;
		if (io->io_hdr.flags & CTL_FLAG_FAILOVER) {
			ctl_failover_io(io, /*have_lock*/ 1);
			return;
		}

		if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg, sizeof(msg), 0) >
		    CTL_HA_STATUS_SUCCESS) {
			/* XXX KDM what to do if this fails? */
		}
		return;
	}
	
}

static int
ctl_process_done(union ctl_io *io)
{
	struct ctl_lun *lun;
	struct ctl_softc *softc = control_softc;
	void (*fe_done)(union ctl_io *io);
	uint32_t targ_port = ctl_port_idx(io->io_hdr.nexus.targ_port);

	CTL_DEBUG_PRINT(("ctl_process_done\n"));

	fe_done = softc->ctl_ports[targ_port]->fe_done;

#ifdef CTL_TIME_IO
	if ((time_uptime - io->io_hdr.start_time) > ctl_time_io_secs) {
		char str[256];
		char path_str[64];
		struct sbuf sb;

		ctl_scsi_path_string(io, path_str, sizeof(path_str));
		sbuf_new(&sb, str, sizeof(str), SBUF_FIXEDLEN);

		sbuf_cat(&sb, path_str);
		switch (io->io_hdr.io_type) {
		case CTL_IO_SCSI:
			ctl_scsi_command_string(&io->scsiio, NULL, &sb);
			sbuf_printf(&sb, "\n");
			sbuf_cat(&sb, path_str);
			sbuf_printf(&sb, "Tag: 0x%04x, type %d\n",
				    io->scsiio.tag_num, io->scsiio.tag_type);
			break;
		case CTL_IO_TASK:
			sbuf_printf(&sb, "Task I/O type: %d, Tag: 0x%04x, "
				    "Tag Type: %d\n", io->taskio.task_action,
				    io->taskio.tag_num, io->taskio.tag_type);
			break;
		default:
			printf("Invalid CTL I/O type %d\n", io->io_hdr.io_type);
			panic("Invalid CTL I/O type %d\n", io->io_hdr.io_type);
			break;
		}
		sbuf_cat(&sb, path_str);
		sbuf_printf(&sb, "ctl_process_done: %jd seconds\n",
			    (intmax_t)time_uptime - io->io_hdr.start_time);
		sbuf_finish(&sb);
		printf("%s", sbuf_data(&sb));
	}
#endif /* CTL_TIME_IO */

	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		break;
	case CTL_IO_TASK:
		if (bootverbose || (ctl_debug & CTL_DEBUG_INFO))
			ctl_io_error_print(io, NULL);
		if (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC)
			ctl_free_io(io);
		else
			fe_done(io);
		return (CTL_RETVAL_COMPLETE);
	default:
		panic("ctl_process_done: invalid io type %d\n",
		      io->io_hdr.io_type);
		break; /* NOTREACHED */
	}

	lun = (struct ctl_lun *)io->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;
	if (lun == NULL) {
		CTL_DEBUG_PRINT(("NULL LUN for lun %d\n",
				 io->io_hdr.nexus.targ_mapped_lun));
		goto bailout;
	}

	mtx_lock(&lun->lun_lock);

	/*
	 * Check to see if we have any errors to inject here.  We only
	 * inject errors for commands that don't already have errors set.
	 */
	if ((STAILQ_FIRST(&lun->error_list) != NULL) &&
	    ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) &&
	    ((io->io_hdr.flags & CTL_FLAG_STATUS_SENT) == 0))
		ctl_inject_error(lun, io);

	/*
	 * XXX KDM how do we treat commands that aren't completed
	 * successfully?
	 *
	 * XXX KDM should we also track I/O latency?
	 */
	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS &&
	    io->io_hdr.io_type == CTL_IO_SCSI) {
#ifdef CTL_TIME_IO
		struct bintime cur_bt;
#endif
		int type;

		if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) ==
		    CTL_FLAG_DATA_IN)
			type = CTL_STATS_READ;
		else if ((io->io_hdr.flags & CTL_FLAG_DATA_MASK) ==
		    CTL_FLAG_DATA_OUT)
			type = CTL_STATS_WRITE;
		else
			type = CTL_STATS_NO_IO;

		lun->stats.ports[targ_port].bytes[type] +=
		    io->scsiio.kern_total_len;
		lun->stats.ports[targ_port].operations[type]++;
#ifdef CTL_TIME_IO
		bintime_add(&lun->stats.ports[targ_port].dma_time[type],
		   &io->io_hdr.dma_bt);
		lun->stats.ports[targ_port].num_dmas[type] +=
		    io->io_hdr.num_dmas;
		getbintime(&cur_bt);
		bintime_sub(&cur_bt, &io->io_hdr.start_bt);
		bintime_add(&lun->stats.ports[targ_port].time[type], &cur_bt);
#endif
	}

	/*
	 * Remove this from the OOA queue.
	 */
	TAILQ_REMOVE(&lun->ooa_queue, &io->io_hdr, ooa_links);
#ifdef CTL_TIME_IO
	if (TAILQ_EMPTY(&lun->ooa_queue))
		lun->last_busy = getsbinuptime();
#endif

	/*
	 * Run through the blocked queue on this LUN and see if anything
	 * has become unblocked, now that this transaction is done.
	 */
	ctl_check_blocked(lun);

	/*
	 * If the LUN has been invalidated, free it if there is nothing
	 * left on its OOA queue.
	 */
	if ((lun->flags & CTL_LUN_INVALID)
	 && TAILQ_EMPTY(&lun->ooa_queue)) {
		mtx_unlock(&lun->lun_lock);
		mtx_lock(&softc->ctl_lock);
		ctl_free_lun(lun);
		mtx_unlock(&softc->ctl_lock);
	} else
		mtx_unlock(&lun->lun_lock);

bailout:

	/*
	 * If this command has been aborted, make sure we set the status
	 * properly.  The FETD is responsible for freeing the I/O and doing
	 * whatever it needs to do to clean up its state.
	 */
	if (io->io_hdr.flags & CTL_FLAG_ABORT)
		ctl_set_task_aborted(&io->scsiio);

	/*
	 * If enabled, print command error status.
	 * We don't print UAs unless debugging was enabled explicitly.
	 */
	do {
		if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS)
			break;
		if (!bootverbose && (ctl_debug & CTL_DEBUG_INFO) == 0)
			break;
		if ((ctl_debug & CTL_DEBUG_INFO) == 0 &&
		    ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SCSI_ERROR) &&
		     (io->scsiio.scsi_status == SCSI_STATUS_CHECK_COND)) {
			int error_code, sense_key, asc, ascq;

			scsi_extract_sense_len(&io->scsiio.sense_data,
			    io->scsiio.sense_len, &error_code, &sense_key,
			    &asc, &ascq, /*show_errors*/ 0);
			if (sense_key == SSD_KEY_UNIT_ATTENTION)
				break;
		}

		ctl_io_error_print(io, NULL);
	} while (0);

	/*
	 * Tell the FETD or the other shelf controller we're done with this
	 * command.  Note that only SCSI commands get to this point.  Task
	 * management commands are completed above.
	 *
	 * We only send status to the other controller if we're in XFER
	 * mode.  In SER_ONLY mode, the I/O is done on the controller that
	 * received the I/O (from CTL's perspective), and so the status is
	 * generated there.
	 * 
	 * XXX KDM if we hold the lock here, we could cause a deadlock
	 * if the frontend comes back in in this context to queue
	 * something.
	 */
	if ((softc->ha_mode == CTL_HA_MODE_XFER)
	 && (io->io_hdr.flags & CTL_FLAG_FROM_OTHER_SC)) {
		union ctl_ha_msg msg;

		memset(&msg, 0, sizeof(msg));
		msg.hdr.msg_type = CTL_MSG_FINISH_IO;
		msg.hdr.original_sc = io->io_hdr.original_sc;
		msg.hdr.nexus = io->io_hdr.nexus;
		msg.hdr.status = io->io_hdr.status;
		msg.scsi.scsi_status = io->scsiio.scsi_status;
		msg.scsi.tag_num = io->scsiio.tag_num;
		msg.scsi.tag_type = io->scsiio.tag_type;
		msg.scsi.sense_len = io->scsiio.sense_len;
		msg.scsi.sense_residual = io->scsiio.sense_residual;
		msg.scsi.residual = io->scsiio.residual;
		memcpy(&msg.scsi.sense_data, &io->scsiio.sense_data,
		       sizeof(io->scsiio.sense_data));
		/*
		 * We copy this whether or not this is an I/O-related
		 * command.  Otherwise, we'd have to go and check to see
		 * whether it's a read/write command, and it really isn't
		 * worth it.
		 */
		memcpy(&msg.scsi.lbalen,
		       &io->io_hdr.ctl_private[CTL_PRIV_LBA_LEN].bytes,
		       sizeof(msg.scsi.lbalen));

		if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg,
				sizeof(msg), 0) > CTL_HA_STATUS_SUCCESS) {
			/* XXX do something here */
		}

		ctl_free_io(io);
	} else 
		fe_done(io);

	return (CTL_RETVAL_COMPLETE);
}

#ifdef CTL_WITH_CA
/*
 * Front end should call this if it doesn't do autosense.  When the request
 * sense comes back in from the initiator, we'll dequeue this and send it.
 */
int
ctl_queue_sense(union ctl_io *io)
{
	struct ctl_lun *lun;
	struct ctl_port *port;
	struct ctl_softc *softc;
	uint32_t initidx, targ_lun;

	softc = control_softc;

	CTL_DEBUG_PRINT(("ctl_queue_sense\n"));

	/*
	 * LUN lookup will likely move to the ctl_work_thread() once we
	 * have our new queueing infrastructure (that doesn't put things on
	 * a per-LUN queue initially).  That is so that we can handle
	 * things like an INQUIRY to a LUN that we don't have enabled.  We
	 * can't deal with that right now.
	 */
	mtx_lock(&softc->ctl_lock);

	/*
	 * If we don't have a LUN for this, just toss the sense
	 * information.
	 */
	port = ctl_io_port(&ctsio->io_hdr);
	targ_lun = ctl_lun_map_from_port(port, io->io_hdr.nexus.targ_lun);
	if ((targ_lun < CTL_MAX_LUNS)
	 && (softc->ctl_luns[targ_lun] != NULL))
		lun = softc->ctl_luns[targ_lun];
	else
		goto bailout;

	initidx = ctl_get_initindex(&io->io_hdr.nexus);

	mtx_lock(&lun->lun_lock);
	/*
	 * Already have CA set for this LUN...toss the sense information.
	 */
	if (ctl_is_set(lun->have_ca, initidx)) {
		mtx_unlock(&lun->lun_lock);
		goto bailout;
	}

	memcpy(&lun->pending_sense[initidx], &io->scsiio.sense_data,
	       MIN(sizeof(lun->pending_sense[initidx]),
	       sizeof(io->scsiio.sense_data)));
	ctl_set_mask(lun->have_ca, initidx);
	mtx_unlock(&lun->lun_lock);

bailout:
	mtx_unlock(&softc->ctl_lock);

	ctl_free_io(io);

	return (CTL_RETVAL_COMPLETE);
}
#endif

/*
 * Primary command inlet from frontend ports.  All SCSI and task I/O
 * requests must go through this function.
 */
int
ctl_queue(union ctl_io *io)
{
	struct ctl_port *port;

	CTL_DEBUG_PRINT(("ctl_queue cdb[0]=%02X\n", io->scsiio.cdb[0]));

#ifdef CTL_TIME_IO
	io->io_hdr.start_time = time_uptime;
	getbintime(&io->io_hdr.start_bt);
#endif /* CTL_TIME_IO */

	/* Map FE-specific LUN ID into global one. */
	port = ctl_io_port(&io->io_hdr);
	io->io_hdr.nexus.targ_mapped_lun =
	    ctl_lun_map_from_port(port, io->io_hdr.nexus.targ_lun);

	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
	case CTL_IO_TASK:
		if (ctl_debug & CTL_DEBUG_CDB)
			ctl_io_print(io);
		ctl_enqueue_incoming(io);
		break;
	default:
		printf("ctl_queue: unknown I/O type %d\n", io->io_hdr.io_type);
		return (EINVAL);
	}

	return (CTL_RETVAL_COMPLETE);
}

#ifdef CTL_IO_DELAY
static void
ctl_done_timer_wakeup(void *arg)
{
	union ctl_io *io;

	io = (union ctl_io *)arg;
	ctl_done(io);
}
#endif /* CTL_IO_DELAY */

void
ctl_done(union ctl_io *io)
{

	/*
	 * Enable this to catch duplicate completion issues.
	 */
#if 0
	if (io->io_hdr.flags & CTL_FLAG_ALREADY_DONE) {
		printf("%s: type %d msg %d cdb %x iptl: "
		       "%d:%d:%d:%d tag 0x%04x "
		       "flag %#x status %x\n",
			__func__,
			io->io_hdr.io_type,
			io->io_hdr.msg_type,
			io->scsiio.cdb[0],
			io->io_hdr.nexus.initid.id,
			io->io_hdr.nexus.targ_port,
			io->io_hdr.nexus.targ_target.id,
			io->io_hdr.nexus.targ_lun,
			(io->io_hdr.io_type ==
			CTL_IO_TASK) ?
			io->taskio.tag_num :
			io->scsiio.tag_num,
		        io->io_hdr.flags,
			io->io_hdr.status);
	} else
		io->io_hdr.flags |= CTL_FLAG_ALREADY_DONE;
#endif

	/*
	 * This is an internal copy of an I/O, and should not go through
	 * the normal done processing logic.
	 */
	if (io->io_hdr.flags & CTL_FLAG_INT_COPY)
		return;

	/*
	 * We need to send a msg to the serializing shelf to finish the IO
	 * as well.  We don't send a finish message to the other shelf if
	 * this is a task management command.  Task management commands
	 * aren't serialized in the OOA queue, but rather just executed on
	 * both shelf controllers for commands that originated on that
	 * controller.
	 */
	if ((io->io_hdr.flags & CTL_FLAG_SENT_2OTHER_SC)
	 && (io->io_hdr.io_type != CTL_IO_TASK)) {
		union ctl_ha_msg msg_io;

		msg_io.hdr.msg_type = CTL_MSG_FINISH_IO;
		msg_io.hdr.serializing_sc = io->io_hdr.serializing_sc;
		if (ctl_ha_msg_send(CTL_HA_CHAN_CTL, &msg_io,
		    sizeof(msg_io), 0 ) != CTL_HA_STATUS_SUCCESS) {
		}
		/* continue on to finish IO */
	}
#ifdef CTL_IO_DELAY
	if (io->io_hdr.flags & CTL_FLAG_DELAY_DONE) {
		struct ctl_lun *lun;

		lun =(struct ctl_lun *)io->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

		io->io_hdr.flags &= ~CTL_FLAG_DELAY_DONE;
	} else {
		struct ctl_lun *lun;

		lun =(struct ctl_lun *)io->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

		if ((lun != NULL)
		 && (lun->delay_info.done_delay > 0)) {
			struct callout *callout;

			callout = (struct callout *)&io->io_hdr.timer_bytes;
			callout_init(callout, /*mpsafe*/ 1);
			io->io_hdr.flags |= CTL_FLAG_DELAY_DONE;
			callout_reset(callout,
				      lun->delay_info.done_delay * hz,
				      ctl_done_timer_wakeup, io);
			if (lun->delay_info.done_type == CTL_DELAY_TYPE_ONESHOT)
				lun->delay_info.done_delay = 0;
			return;
		}
	}
#endif /* CTL_IO_DELAY */

	ctl_enqueue_done(io);
}

int
ctl_isc(struct ctl_scsiio *ctsio)
{
	struct ctl_lun *lun;
	int retval;

	lun = (struct ctl_lun *)ctsio->io_hdr.ctl_private[CTL_PRIV_LUN].ptr;

	CTL_DEBUG_PRINT(("ctl_isc: command: %02x\n", ctsio->cdb[0]));

	CTL_DEBUG_PRINT(("ctl_isc: calling data_submit()\n"));

	retval = lun->backend->data_submit((union ctl_io *)ctsio);

	return (retval);
}


static void
ctl_work_thread(void *arg)
{
	struct ctl_thread *thr = (struct ctl_thread *)arg;
	struct ctl_softc *softc = thr->ctl_softc;
	union ctl_io *io;
	int retval;

	CTL_DEBUG_PRINT(("ctl_work_thread starting\n"));

	for (;;) {
		retval = 0;

		/*
		 * We handle the queues in this order:
		 * - ISC
		 * - done queue (to free up resources, unblock other commands)
		 * - RtR queue
		 * - incoming queue
		 *
		 * If those queues are empty, we break out of the loop and
		 * go to sleep.
		 */
		mtx_lock(&thr->queue_lock);
		io = (union ctl_io *)STAILQ_FIRST(&thr->isc_queue);
		if (io != NULL) {
			STAILQ_REMOVE_HEAD(&thr->isc_queue, links);
			mtx_unlock(&thr->queue_lock);
			ctl_handle_isc(io);
			continue;
		}
		io = (union ctl_io *)STAILQ_FIRST(&thr->done_queue);
		if (io != NULL) {
			STAILQ_REMOVE_HEAD(&thr->done_queue, links);
			/* clear any blocked commands, call fe_done */
			mtx_unlock(&thr->queue_lock);
			retval = ctl_process_done(io);
			continue;
		}
		io = (union ctl_io *)STAILQ_FIRST(&thr->incoming_queue);
		if (io != NULL) {
			STAILQ_REMOVE_HEAD(&thr->incoming_queue, links);
			mtx_unlock(&thr->queue_lock);
			if (io->io_hdr.io_type == CTL_IO_TASK)
				ctl_run_task(io);
			else
				ctl_scsiio_precheck(softc, &io->scsiio);
			continue;
		}
		if (!ctl_pause_rtr) {
			io = (union ctl_io *)STAILQ_FIRST(&thr->rtr_queue);
			if (io != NULL) {
				STAILQ_REMOVE_HEAD(&thr->rtr_queue, links);
				mtx_unlock(&thr->queue_lock);
				retval = ctl_scsiio(&io->scsiio);
				if (retval != CTL_RETVAL_COMPLETE)
					CTL_DEBUG_PRINT(("ctl_scsiio failed\n"));
				continue;
			}
		}

		/* Sleep until we have something to do. */
		mtx_sleep(thr, &thr->queue_lock, PDROP | PRIBIO, "-", 0);
	}
}

static void
ctl_lun_thread(void *arg)
{
	struct ctl_softc *softc = (struct ctl_softc *)arg;
	struct ctl_be_lun *be_lun;
	int retval;

	CTL_DEBUG_PRINT(("ctl_lun_thread starting\n"));

	for (;;) {
		retval = 0;
		mtx_lock(&softc->ctl_lock);
		be_lun = STAILQ_FIRST(&softc->pending_lun_queue);
		if (be_lun != NULL) {
			STAILQ_REMOVE_HEAD(&softc->pending_lun_queue, links);
			mtx_unlock(&softc->ctl_lock);
			ctl_create_lun(be_lun);
			continue;
		}

		/* Sleep until we have something to do. */
		mtx_sleep(&softc->pending_lun_queue, &softc->ctl_lock,
		    PDROP | PRIBIO, "-", 0);
	}
}

static void
ctl_thresh_thread(void *arg)
{
	struct ctl_softc *softc = (struct ctl_softc *)arg;
	struct ctl_lun *lun;
	struct ctl_be_lun *be_lun;
	struct scsi_da_rw_recovery_page *rwpage;
	struct ctl_logical_block_provisioning_page *page;
	const char *attr;
	uint64_t thres, val;
	int i, e;

	CTL_DEBUG_PRINT(("ctl_thresh_thread starting\n"));

	for (;;) {
		mtx_lock(&softc->ctl_lock);
		STAILQ_FOREACH(lun, &softc->lun_list, links) {
			be_lun = lun->be_lun;
			if ((lun->flags & CTL_LUN_DISABLED) ||
			    (lun->flags & CTL_LUN_OFFLINE) ||
			    lun->backend->lun_attr == NULL)
				continue;
			rwpage = &lun->mode_pages.rw_er_page[CTL_PAGE_CURRENT];
			if ((rwpage->byte8 & SMS_RWER_LBPERE) == 0)
				continue;
			e = 0;
			page = &lun->mode_pages.lbp_page[CTL_PAGE_CURRENT];
			for (i = 0; i < CTL_NUM_LBP_THRESH; i++) {
				if ((page->descr[i].flags & SLBPPD_ENABLED) == 0)
					continue;
				thres = scsi_4btoul(page->descr[i].count);
				thres <<= CTL_LBP_EXPONENT;
				switch (page->descr[i].resource) {
				case 0x01:
					attr = "blocksavail";
					break;
				case 0x02:
					attr = "blocksused";
					break;
				case 0xf1:
					attr = "poolblocksavail";
					break;
				case 0xf2:
					attr = "poolblocksused";
					break;
				default:
					continue;
				}
				mtx_unlock(&softc->ctl_lock); // XXX
				val = lun->backend->lun_attr(
				    lun->be_lun->be_lun, attr);
				mtx_lock(&softc->ctl_lock);
				if (val == UINT64_MAX)
					continue;
				if ((page->descr[i].flags & SLBPPD_ARMING_MASK)
				    == SLBPPD_ARMING_INC)
					e |= (val >= thres);
				else
					e |= (val <= thres);
			}
			mtx_lock(&lun->lun_lock);
			if (e) {
				if (lun->lasttpt == 0 ||
				    time_uptime - lun->lasttpt >= CTL_LBP_UA_PERIOD) {
					lun->lasttpt = time_uptime;
					ctl_est_ua_all(lun, -1, CTL_UA_THIN_PROV_THRES);
				}
			} else {
				lun->lasttpt = 0;
				ctl_clr_ua_all(lun, -1, CTL_UA_THIN_PROV_THRES);
			}
			mtx_unlock(&lun->lun_lock);
		}
		mtx_unlock(&softc->ctl_lock);
		pause("-", CTL_LBP_PERIOD * hz);
	}
}

static void
ctl_enqueue_incoming(union ctl_io *io)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_thread *thr;
	u_int idx;

	idx = (io->io_hdr.nexus.targ_port * 127 +
	       io->io_hdr.nexus.initid.id) % worker_threads;
	thr = &softc->threads[idx];
	mtx_lock(&thr->queue_lock);
	STAILQ_INSERT_TAIL(&thr->incoming_queue, &io->io_hdr, links);
	mtx_unlock(&thr->queue_lock);
	wakeup(thr);
}

static void
ctl_enqueue_rtr(union ctl_io *io)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_thread *thr;

	thr = &softc->threads[io->io_hdr.nexus.targ_mapped_lun % worker_threads];
	mtx_lock(&thr->queue_lock);
	STAILQ_INSERT_TAIL(&thr->rtr_queue, &io->io_hdr, links);
	mtx_unlock(&thr->queue_lock);
	wakeup(thr);
}

static void
ctl_enqueue_done(union ctl_io *io)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_thread *thr;

	thr = &softc->threads[io->io_hdr.nexus.targ_mapped_lun % worker_threads];
	mtx_lock(&thr->queue_lock);
	STAILQ_INSERT_TAIL(&thr->done_queue, &io->io_hdr, links);
	mtx_unlock(&thr->queue_lock);
	wakeup(thr);
}

static void
ctl_enqueue_isc(union ctl_io *io)
{
	struct ctl_softc *softc = control_softc;
	struct ctl_thread *thr;

	thr = &softc->threads[io->io_hdr.nexus.targ_mapped_lun % worker_threads];
	mtx_lock(&thr->queue_lock);
	STAILQ_INSERT_TAIL(&thr->isc_queue, &io->io_hdr, links);
	mtx_unlock(&thr->queue_lock);
	wakeup(thr);
}

/* Initialization and failover */

void
ctl_init_isc_msg(void)
{
	printf("CTL: Still calling this thing\n");
}

/*
 * Init component
 * 	Initializes component into configuration defined by bootMode
 *	(see hasc-sv.c)
 *  	returns hasc_Status:
 * 		OK
 *		ERROR - fatal error
 */
static ctl_ha_comp_status
ctl_isc_init(struct ctl_ha_component *c)
{
	ctl_ha_comp_status ret = CTL_HA_COMP_STATUS_OK;

	c->status = ret;
	return ret;
}

/* Start component
 * 	Starts component in state requested. If component starts successfully,
 *	it must set its own state to the requestrd state
 *	When requested state is HASC_STATE_HA, the component may refine it
 * 	by adding _SLAVE or _MASTER flags.
 *	Currently allowed state transitions are:
 *	UNKNOWN->HA		- initial startup
 *	UNKNOWN->SINGLE - initial startup when no parter detected
 *	HA->SINGLE		- failover
 * returns ctl_ha_comp_status:
 * 		OK	- component successfully started in requested state
 *		FAILED  - could not start the requested state, failover may
 * 			  be possible
 *		ERROR	- fatal error detected, no future startup possible
 */
static ctl_ha_comp_status
ctl_isc_start(struct ctl_ha_component *c, ctl_ha_state state)
{
	ctl_ha_comp_status ret = CTL_HA_COMP_STATUS_OK;

	printf("%s: go\n", __func__);

	// UNKNOWN->HA or UNKNOWN->SINGLE (bootstrap)
	if (c->state == CTL_HA_STATE_UNKNOWN ) {
		control_softc->is_single = 0;
		if (ctl_ha_msg_create(CTL_HA_CHAN_CTL, ctl_isc_event_handler)
		    != CTL_HA_STATUS_SUCCESS) {
			printf("ctl_isc_start: ctl_ha_msg_create failed.\n");
			ret = CTL_HA_COMP_STATUS_ERROR;
		}
	} else if (CTL_HA_STATE_IS_HA(c->state)
		&& CTL_HA_STATE_IS_SINGLE(state)){
		// HA->SINGLE transition
	        ctl_failover();
		control_softc->is_single = 1;
	} else {
		printf("ctl_isc_start:Invalid state transition %X->%X\n",
		       c->state, state);
		ret = CTL_HA_COMP_STATUS_ERROR;
	}
	if (CTL_HA_STATE_IS_SINGLE(state))
		control_softc->is_single = 1;

	c->state = state;
	c->status = ret;
	return ret;
}

/*
 * Quiesce component
 * The component must clear any error conditions (set status to OK) and
 * prepare itself to another Start call
 * returns ctl_ha_comp_status:
 * 	OK
 *	ERROR
 */
static ctl_ha_comp_status
ctl_isc_quiesce(struct ctl_ha_component *c)
{
	int ret = CTL_HA_COMP_STATUS_OK;

	ctl_pause_rtr = 1;
	c->status = ret;
	return ret;
}

struct ctl_ha_component ctl_ha_component_ctlisc =
{
	.name = "CTL ISC",
	.state = CTL_HA_STATE_UNKNOWN,
	.init = ctl_isc_init,
	.start = ctl_isc_start,
	.quiesce = ctl_isc_quiesce
};

/*
 *  vim: ts=8
 */
