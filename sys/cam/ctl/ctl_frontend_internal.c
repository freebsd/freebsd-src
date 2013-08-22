/*-
 * Copyright (c) 2004, 2005 Silicon Graphics International Corp.
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
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_frontend_internal.c#5 $
 */
/*
 * CTL kernel internal frontend target driver.  This allows kernel-level
 * clients to send commands into CTL.
 *
 * This has elements of a FETD (e.g. it has to set tag numbers, initiator,
 * port, target, and LUN) and elements of an initiator (LUN discovery and
 * probing, error recovery, command initiation).  Even though this has some
 * initiator type elements, this is not intended to be a full fledged
 * initiator layer.  It is only intended to send a limited number of
 * commands to a well known target layer.
 *
 * To be able to fulfill the role of a full initiator layer, it would need
 * a whole lot more functionality.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 *
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_frontend.h>
#include <cam/ctl/ctl_frontend_internal.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_util.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_mem_pool.h>
#include <cam/ctl/ctl_debug.h>
#include <cam/ctl/ctl_scsi_all.h>
#include <cam/ctl/ctl_error.h>

/*
 * Task structure:
 *  - overall metatask, different potential metatask types (e.g. forced
 *    shutdown, gentle shutdown)
 *  - forced shutdown metatask:
 *     - states:  report luns, pending, done?
 *     - list of luns pending, with the relevant I/O for that lun attached.
 *       This would allow moving ahead on LUNs with no errors, and going
 *       into error recovery on LUNs with problems.  Per-LUN states might
 *       include inquiry, stop/offline, done.
 *
 * Use LUN enable for LUN list instead of getting it manually?  We'd still
 * need inquiry data for each LUN.
 *
 * How to handle processor LUN w.r.t. found/stopped counts?
 */
#ifdef oldapi
typedef enum {
	CFI_TASK_NONE,
	CFI_TASK_SHUTDOWN,
	CFI_TASK_STARTUP
} cfi_tasktype;

struct cfi_task_startstop {
	int total_luns;
	int luns_complete;
	int luns_failed;
	cfi_cb_t callback;
	void *callback_arg;
	/* XXX KDM add more fields here */
};

union cfi_taskinfo {
	struct cfi_task_startstop startstop;
};

struct cfi_metatask {
	cfi_tasktype		tasktype;
	cfi_mt_status		status;
	union cfi_taskinfo	taskinfo;
	struct ctl_mem_element	*element;
	void			*cfi_context;
	STAILQ_ENTRY(cfi_metatask) links;
};
#endif

typedef enum {
	CFI_ERR_RETRY		= 0x000,
	CFI_ERR_FAIL		= 0x001,
	CFI_ERR_LUN_RESET	= 0x002,
	CFI_ERR_MASK		= 0x0ff,
	CFI_ERR_NO_DECREMENT	= 0x100
} cfi_error_action;

typedef enum {
	CFI_ERR_SOFT,
	CFI_ERR_HARD
} cfi_error_policy;

typedef enum {
	CFI_LUN_INQUIRY,
	CFI_LUN_READCAPACITY,
	CFI_LUN_READCAPACITY_16,
	CFI_LUN_READY
} cfi_lun_state;

struct cfi_lun {
	struct ctl_id target_id;
	int lun_id;
	struct scsi_inquiry_data inq_data;
	uint64_t num_blocks;
	uint32_t blocksize;
	int blocksize_powerof2;
	uint32_t cur_tag_num;
	cfi_lun_state state;
	struct ctl_mem_element *element;
	struct cfi_softc *softc;
	STAILQ_HEAD(, cfi_lun_io) io_list;
	STAILQ_ENTRY(cfi_lun) links;
};

struct cfi_lun_io {
	struct cfi_lun *lun;
	struct cfi_metatask *metatask;
	cfi_error_policy policy;
	void (*done_function)(union ctl_io *io);
	union ctl_io *ctl_io;
	struct cfi_lun_io *orig_lun_io;
	STAILQ_ENTRY(cfi_lun_io) links;
};

typedef enum {
	CFI_NONE	= 0x00,
	CFI_ONLINE	= 0x01,
} cfi_flags;

struct cfi_softc {
	struct ctl_frontend fe;
	char fe_name[40];
	struct mtx lock;
	cfi_flags flags;
	STAILQ_HEAD(, cfi_lun) lun_list;
	STAILQ_HEAD(, cfi_metatask) metatask_list;
	struct ctl_mem_pool lun_pool;
	struct ctl_mem_pool metatask_pool;
};

MALLOC_DEFINE(M_CTL_CFI, "ctlcfi", "CTL CFI");

static struct cfi_softc fetd_internal_softc;

int cfi_init(void);
void cfi_shutdown(void) __unused;
static void cfi_online(void *arg);
static void cfi_offline(void *arg);
static int cfi_targ_enable(void *arg, struct ctl_id targ_id);
static int cfi_targ_disable(void *arg, struct ctl_id targ_id);
static int cfi_lun_enable(void *arg, struct ctl_id target_id, int lun_id);
static int cfi_lun_disable(void *arg, struct ctl_id target_id, int lun_id);
static void cfi_datamove(union ctl_io *io);
static cfi_error_action cfi_checkcond_parse(union ctl_io *io,
					    struct cfi_lun_io *lun_io);
static cfi_error_action cfi_error_parse(union ctl_io *io,
					struct cfi_lun_io *lun_io);
static void cfi_init_io(union ctl_io *io, struct cfi_lun *lun,
			struct cfi_metatask *metatask, cfi_error_policy policy,
			int retries, struct cfi_lun_io *orig_lun_io,
			void (*done_function)(union ctl_io *io));
static void cfi_done(union ctl_io *io);
static void cfi_lun_probe_done(union ctl_io *io);
static void cfi_lun_probe(struct cfi_lun *lun, int have_lock);
static void cfi_metatask_done(struct cfi_softc *softc,
			      struct cfi_metatask *metatask);
static void cfi_metatask_bbr_errorparse(struct cfi_metatask *metatask,
					union ctl_io *io);
static void cfi_metatask_io_done(union ctl_io *io);
static void cfi_err_recovery_done(union ctl_io *io);
static void cfi_lun_io_done(union ctl_io *io);

static int cfi_module_event_handler(module_t, int /*modeventtype_t*/, void *);

static moduledata_t cfi_moduledata = {
	"ctlcfi",
	cfi_module_event_handler,
	NULL
};

DECLARE_MODULE(ctlcfi, cfi_moduledata, SI_SUB_CONFIGURE, SI_ORDER_FOURTH);
MODULE_VERSION(ctlcfi, 1);
MODULE_DEPEND(ctlcfi, ctl, 1, 1, 1);

int
cfi_init(void)
{
	struct cfi_softc *softc;
	struct ctl_frontend *fe;
	int retval;

	softc = &fetd_internal_softc;

	fe = &softc->fe;

	retval = 0;

	if (sizeof(struct cfi_lun_io) > CTL_PORT_PRIV_SIZE) {
		printf("%s: size of struct cfi_lun_io %zd > "
		       "CTL_PORT_PRIV_SIZE %d\n", __func__,
		       sizeof(struct cfi_lun_io),
		       CTL_PORT_PRIV_SIZE);
	}
	memset(softc, 0, sizeof(softc));

	mtx_init(&softc->lock, "CTL frontend mutex", NULL, MTX_DEF);
	softc->flags |= CTL_FLAG_MASTER_SHELF;

	STAILQ_INIT(&softc->lun_list);
	STAILQ_INIT(&softc->metatask_list);
	sprintf(softc->fe_name, "CTL internal");
	fe->port_type = CTL_PORT_INTERNAL;
	fe->num_requested_ctl_io = 100;
	fe->port_name = softc->fe_name;
	fe->port_online = cfi_online;
	fe->port_offline = cfi_offline;
	fe->onoff_arg = softc;
	fe->targ_enable = cfi_targ_enable;
	fe->targ_disable = cfi_targ_disable;
	fe->lun_enable = cfi_lun_enable;
	fe->lun_disable = cfi_lun_disable;
	fe->targ_lun_arg = softc;
	fe->fe_datamove = cfi_datamove;
	fe->fe_done = cfi_done;
	fe->max_targets = 15;
	fe->max_target_id = 15;

	if (ctl_frontend_register(fe, (softc->flags & CTL_FLAG_MASTER_SHELF)) != 0) 
	{
		printf("%s: internal frontend registration failed\n", __func__);
		retval = 1;
		goto bailout;
	}

	if (ctl_init_mem_pool(&softc->lun_pool,
			      sizeof(struct cfi_lun),
			      CTL_MEM_POOL_PERM_GROW, /*grow_inc*/ 3,
			      /* initial_pool_size */ CTL_MAX_LUNS) != 0) {
		printf("%s: can't initialize LUN memory pool\n", __func__);
		retval = 1;
		goto bailout_error;
	}

	if (ctl_init_mem_pool(&softc->metatask_pool,
			      sizeof(struct cfi_metatask),
			      CTL_MEM_POOL_PERM_GROW, /*grow_inc*/ 3,
			      /*initial_pool_size*/ 10) != 0) {
		printf("%s: can't initialize metatask memory pool\n", __func__);
		retval = 2;
		goto bailout_error;
	}
bailout:

	return (0);

bailout_error:

	switch (retval) {
	case 3:
		ctl_shrink_mem_pool(&softc->metatask_pool);
		/* FALLTHROUGH */
	case 2:
		ctl_shrink_mem_pool(&softc->lun_pool);
		/* FALLTHROUGH */
	case 1:
		ctl_frontend_deregister(fe);
		break;
	default:
		break;
	}

	return (ENOMEM);
}

void
cfi_shutdown(void)
{
	struct cfi_softc *softc;

	softc = &fetd_internal_softc;

	/*
	 * XXX KDM need to clear out any I/O pending on each LUN.
	 */
	if (ctl_frontend_deregister(&softc->fe) != 0)
		printf("%s: ctl_frontend_deregister() failed\n", __func__);

	if (ctl_shrink_mem_pool(&softc->lun_pool) != 0)
		printf("%s: error shrinking LUN pool\n", __func__);

	if (ctl_shrink_mem_pool(&softc->metatask_pool) != 0)
		printf("%s: error shrinking LUN pool\n", __func__);
}

static int
cfi_module_event_handler(module_t mod, int what, void *arg)
{

	switch (what) {
	case MOD_LOAD:
		return (cfi_init());
	case MOD_UNLOAD:
		return (EBUSY);
	default:
		return (EOPNOTSUPP);
	}
}

static void
cfi_online(void *arg)
{
	struct cfi_softc *softc;
	struct cfi_lun *lun;

	softc = (struct cfi_softc *)arg;

	softc->flags |= CFI_ONLINE;

	/*
	 * Go through and kick off the probe for each lun.  Should we check
	 * the LUN flags here to determine whether or not to probe it?
	 */
	mtx_lock(&softc->lock);
	STAILQ_FOREACH(lun, &softc->lun_list, links)
		cfi_lun_probe(lun, /*have_lock*/ 1);
	mtx_unlock(&softc->lock);
}

static void
cfi_offline(void *arg)
{
	struct cfi_softc *softc;

	softc = (struct cfi_softc *)arg;

	softc->flags &= ~CFI_ONLINE;
}

static int
cfi_targ_enable(void *arg, struct ctl_id targ_id)
{
	return (0);
}

static int
cfi_targ_disable(void *arg, struct ctl_id targ_id)
{
	return (0);
}

static int
cfi_lun_enable(void *arg, struct ctl_id target_id, int lun_id)
{
	struct ctl_mem_element *element;
	struct cfi_softc *softc;
	struct cfi_lun *lun;
	int found;

	softc = (struct cfi_softc *)arg;

	found = 0;
	mtx_lock(&softc->lock);
	STAILQ_FOREACH(lun, &softc->lun_list, links) {
		if ((lun->target_id.id == target_id.id)
		 && (lun->lun_id == lun_id)) {
			found = 1;
			break;
		}
	}
	mtx_unlock(&softc->lock);

	/*
	 * If we already have this target/LUN, there is no reason to add
	 * it to our lists again.
	 */
	if (found != 0)
		return (0);

	element = ctl_alloc_mem_element(&softc->lun_pool, /*can_wait*/ 0);

	if (element == NULL) {
		printf("%s: unable to allocate LUN structure\n", __func__);
		return (1);
	}

	lun = (struct cfi_lun *)element->bytes;

	lun->element = element;
	lun->target_id = target_id;
	lun->lun_id = lun_id;
	lun->cur_tag_num = 0;
	lun->state = CFI_LUN_INQUIRY;
	lun->softc = softc;
	STAILQ_INIT(&lun->io_list);

	mtx_lock(&softc->lock);
	STAILQ_INSERT_TAIL(&softc->lun_list, lun, links);
	mtx_unlock(&softc->lock);

	cfi_lun_probe(lun, /*have_lock*/ 0);

	return (0);
}

static int
cfi_lun_disable(void *arg, struct ctl_id target_id, int lun_id)
{
	struct cfi_softc *softc;
	struct cfi_lun *lun;
	int found;

	softc = (struct cfi_softc *)arg;

	found = 0;

	/*
	 * XXX KDM need to do an invalidate and then a free when any
	 * pending I/O has completed.  Or do we?  CTL won't free a LUN
	 * while any I/O is pending.  So we won't get this notification
	 * unless any I/O we have pending on a LUN has completed.
	 */
	mtx_lock(&softc->lock);
	STAILQ_FOREACH(lun, &softc->lun_list, links) {
		if ((lun->target_id.id == target_id.id)
		 && (lun->lun_id == lun_id)) {
			found = 1;
			break;
		}
	}
	if (found != 0)
		STAILQ_REMOVE(&softc->lun_list, lun, cfi_lun, links);

	mtx_unlock(&softc->lock);

	if (found == 0) {
		printf("%s: can't find target %ju lun %d\n", __func__,
		       (uintmax_t)target_id.id, lun_id);
		return (1);
	}

	ctl_free_mem_element(lun->element);

	return (0);
}

/*
 * XXX KDM run this inside a thread, or inside the caller's context?
 */
static void
cfi_datamove(union ctl_io *io)
{
	struct ctl_sg_entry *ext_sglist, *kern_sglist;
	struct ctl_sg_entry ext_entry, kern_entry;
	int ext_sglen, ext_sg_entries, kern_sg_entries;
	int ext_sg_start, ext_offset;
	int len_to_copy, len_copied;
	int kern_watermark, ext_watermark;
	int ext_sglist_malloced;
	struct ctl_scsiio *ctsio;
	int i, j;

	ext_sglist_malloced = 0;
	ext_sg_start = 0;
	ext_offset = 0;
	ext_sglist = NULL;

	CTL_DEBUG_PRINT(("%s\n", __func__));

	ctsio = &io->scsiio;

	/*
	 * If this is the case, we're probably doing a BBR read and don't
	 * actually need to transfer the data.  This will effectively
	 * bit-bucket the data.
	 */
	if (ctsio->ext_data_ptr == NULL)
		goto bailout;

	/*
	 * To simplify things here, if we have a single buffer, stick it in
	 * a S/G entry and just make it a single entry S/G list.
	 */
	if (ctsio->io_hdr.flags & CTL_FLAG_EDPTR_SGLIST) {
		int len_seen;

		ext_sglen = ctsio->ext_sg_entries * sizeof(*ext_sglist);

		/*
		 * XXX KDM GFP_KERNEL, don't know what the caller's context
		 * is.  Need to figure that out.
		 */
		ext_sglist = (struct ctl_sg_entry *)malloc(ext_sglen, M_CTL_CFI,
							   M_WAITOK);
		if (ext_sglist == NULL) {
			ctl_set_internal_failure(ctsio,
						 /*sks_valid*/ 0,
						 /*retry_count*/ 0);
			return;
		}
		ext_sglist_malloced = 1;
		if (memcpy(ext_sglist, ctsio->ext_data_ptr, ext_sglen) != 0) {
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

		len_to_copy = ctl_min(ext_sglist[i].len - ext_watermark,
				      kern_sglist[j].len - kern_watermark);

		ext_ptr = (uint8_t *)ext_sglist[i].addr;
		ext_ptr = ext_ptr + ext_watermark;
		if (io->io_hdr.flags & CTL_FLAG_BUS_ADDR) {
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
			CTL_DEBUG_PRINT(("%s: copying %d bytes to user\n",
					 __func__, len_to_copy));
			CTL_DEBUG_PRINT(("%s: from %p to %p\n", __func__,
					 kern_ptr, ext_ptr));
			memcpy(ext_ptr, kern_ptr, len_to_copy);
		} else {
			CTL_DEBUG_PRINT(("%s: copying %d bytes from user\n",
					 __func__, len_to_copy));
			CTL_DEBUG_PRINT(("%s: from %p to %p\n", __func__,
					 ext_ptr, kern_ptr));
			memcpy(kern_ptr, ext_ptr, len_to_copy);
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

	CTL_DEBUG_PRINT(("%s: ext_sg_entries: %d, kern_sg_entries: %d\n",
			 __func__, ext_sg_entries, kern_sg_entries));
	CTL_DEBUG_PRINT(("%s: ext_data_len = %d, kern_data_len = %d\n",
			 __func__, ctsio->ext_data_len, ctsio->kern_data_len));
	

	/* XXX KDM set residual?? */
bailout:

	if (ext_sglist_malloced != 0)
		free(ext_sglist, M_CTL_CFI);

	io->scsiio.be_move_done(io);

	return;
}

/*
 * For any sort of check condition, busy, etc., we just retry.  We do not
 * decrement the retry count for unit attention type errors.  These are
 * normal, and we want to save the retry count for "real" errors.  Otherwise,
 * we could end up with situations where a command will succeed in some
 * situations and fail in others, depending on whether a unit attention is
 * pending.  Also, some of our error recovery actions, most notably the
 * LUN reset action, will cause a unit attention.
 *
 * We can add more detail here later if necessary.
 */
static cfi_error_action
cfi_checkcond_parse(union ctl_io *io, struct cfi_lun_io *lun_io)
{
	cfi_error_action error_action;
	int error_code, sense_key, asc, ascq;

	/*
	 * Default to retrying the command.
	 */
	error_action = CFI_ERR_RETRY;

	scsi_extract_sense_len(&io->scsiio.sense_data,
			       io->scsiio.sense_len,
			       &error_code,
			       &sense_key,
			       &asc,
			       &ascq,
			       /*show_errors*/ 1);

	switch (error_code) {
	case SSD_DEFERRED_ERROR:
	case SSD_DESC_DEFERRED_ERROR:
		error_action |= CFI_ERR_NO_DECREMENT;
		break;
	case SSD_CURRENT_ERROR:
	case SSD_DESC_CURRENT_ERROR:
	default: {
		switch (sense_key) {
		case SSD_KEY_UNIT_ATTENTION:
			error_action |= CFI_ERR_NO_DECREMENT;
			break;
		case SSD_KEY_HARDWARE_ERROR:
			/*
			 * This is our generic "something bad happened"
			 * error code.  It often isn't recoverable.
			 */
			if ((asc == 0x44) && (ascq == 0x00))
				error_action = CFI_ERR_FAIL;
			break;
		case SSD_KEY_NOT_READY:
			/*
			 * If the LUN is powered down, there likely isn't
			 * much point in retrying right now.
			 */
			if ((asc == 0x04) && (ascq == 0x02))
				error_action = CFI_ERR_FAIL;
			/*
			 * If the LUN is offline, there probably isn't much
			 * point in retrying, either.
			 */
			if ((asc == 0x04) && (ascq == 0x03))
				error_action = CFI_ERR_FAIL;
			break;
		}
	}
	}

	return (error_action);
}

static cfi_error_action
cfi_error_parse(union ctl_io *io, struct cfi_lun_io *lun_io)
{
	cfi_error_action error_action;

	error_action = CFI_ERR_RETRY;

	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		switch (io->io_hdr.status & CTL_STATUS_MASK) {
		case CTL_SCSI_ERROR:
			switch (io->scsiio.scsi_status) {
			case SCSI_STATUS_RESERV_CONFLICT:
				/*
				 * For a reservation conflict, we'll usually
				 * want the hard error recovery policy, so
				 * we'll reset the LUN.
				 */
				if (lun_io->policy == CFI_ERR_HARD)
					error_action =
						CFI_ERR_LUN_RESET;
				else
					error_action = 
						CFI_ERR_RETRY;
				break;
			case SCSI_STATUS_CHECK_COND:
			default:
				error_action = cfi_checkcond_parse(io, lun_io);
				break;
			}
			break;
		default:
			error_action = CFI_ERR_RETRY;
			break;
		}
		break;
	case CTL_IO_TASK:
		/*
		 * In theory task management commands shouldn't fail...
		 */
		error_action = CFI_ERR_RETRY;
		break;
	default:
		printf("%s: invalid ctl_io type %d\n", __func__,
		       io->io_hdr.io_type);
		panic("%s: invalid ctl_io type %d\n", __func__,
		      io->io_hdr.io_type);
		break;
	}

	return (error_action);
}

static void
cfi_init_io(union ctl_io *io, struct cfi_lun *lun,
	    struct cfi_metatask *metatask, cfi_error_policy policy, int retries,
	    struct cfi_lun_io *orig_lun_io,
	    void (*done_function)(union ctl_io *io))
{
	struct cfi_lun_io *lun_io;

	io->io_hdr.nexus.initid.id = 7;
	io->io_hdr.nexus.targ_port = lun->softc->fe.targ_port;
	io->io_hdr.nexus.targ_target.id = lun->target_id.id;
	io->io_hdr.nexus.targ_lun = lun->lun_id;
	io->io_hdr.retries = retries;
	lun_io = (struct cfi_lun_io *)io->io_hdr.port_priv;
	io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr = lun_io;
	lun_io->lun = lun;
	lun_io->metatask = metatask;
	lun_io->ctl_io = io;
	lun_io->policy = policy;
	lun_io->orig_lun_io = orig_lun_io;
	lun_io->done_function = done_function;
	/*
	 * We only set the tag number for SCSI I/Os.  For task management
	 * commands, the tag number is only really needed for aborts, so
	 * the caller can set it if necessary.
	 */
	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		io->scsiio.tag_num = lun->cur_tag_num++;
		break;
	case CTL_IO_TASK:
	default:
		break;
	}
}

static void
cfi_done(union ctl_io *io)
{
	struct cfi_lun_io *lun_io;
	struct cfi_softc *softc;
	struct cfi_lun *lun;

	lun_io = (struct cfi_lun_io *)
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;

	lun = lun_io->lun;
	softc = lun->softc;

	/*
	 * Very minimal retry logic.  We basically retry if we got an error
	 * back, and the retry count is greater than 0.  If we ever want
	 * more sophisticated initiator type behavior, the CAM error
	 * recovery code in ../common might be helpful.
	 */
	if (((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)
	 && (io->io_hdr.retries > 0)) {
		ctl_io_status old_status;
		cfi_error_action error_action;

		error_action = cfi_error_parse(io, lun_io);

		switch (error_action & CFI_ERR_MASK) {
		case CFI_ERR_FAIL:
			goto done;
			break; /* NOTREACHED */
		case CFI_ERR_LUN_RESET: {
			union ctl_io *new_io;
			struct cfi_lun_io *new_lun_io;

			new_io = ctl_alloc_io(softc->fe.ctl_pool_ref);
			if (new_io == NULL) {
				printf("%s: unable to allocate ctl_io for "
				       "error recovery\n", __func__);
				goto done;
			}
			ctl_zero_io(new_io);

			new_io->io_hdr.io_type = CTL_IO_TASK;
			new_io->taskio.task_action = CTL_TASK_LUN_RESET;

			cfi_init_io(new_io,
				    /*lun*/ lun_io->lun,
				    /*metatask*/ NULL,
				    /*policy*/ CFI_ERR_SOFT,
				    /*retries*/ 0,
				    /*orig_lun_io*/lun_io,
				    /*done_function*/ cfi_err_recovery_done);
			

			new_lun_io = (struct cfi_lun_io *)
				new_io->io_hdr.port_priv;

			mtx_lock(&lun->softc->lock);
			STAILQ_INSERT_TAIL(&lun->io_list, new_lun_io, links);
			mtx_unlock(&lun->softc->lock);

			io = new_io;
			break;
		}
		case CFI_ERR_RETRY:
		default:
			if ((error_action & CFI_ERR_NO_DECREMENT) == 0)
				io->io_hdr.retries--;
			break;
		}

		old_status = io->io_hdr.status;
		io->io_hdr.status = CTL_STATUS_NONE;
#if 0
		io->io_hdr.flags &= ~CTL_FLAG_ALREADY_DONE;
#endif
		io->io_hdr.flags &= ~CTL_FLAG_ABORT;
		io->io_hdr.flags &= ~CTL_FLAG_SENT_2OTHER_SC;

		if (ctl_queue(io) != CTL_RETVAL_COMPLETE) {
			printf("%s: error returned from ctl_queue()!\n",
			       __func__);
			io->io_hdr.status = old_status;
		} else
			return;
	}
done:
	lun_io->done_function(io);
}

static void
cfi_lun_probe_done(union ctl_io *io)
{
	struct cfi_lun *lun;
	struct cfi_lun_io *lun_io;

	lun_io = (struct cfi_lun_io *)
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	lun = lun_io->lun;

	switch (lun->state) {
	case CFI_LUN_INQUIRY: {
		if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS) {
			/* print out something here?? */
			printf("%s: LUN %d probe failed because inquiry "
			       "failed\n", __func__, lun->lun_id);
			ctl_io_error_print(io, NULL);
		} else {

			if (SID_TYPE(&lun->inq_data) != T_DIRECT) {
				char path_str[40];

				lun->state = CFI_LUN_READY;
				ctl_scsi_path_string(io, path_str,
						     sizeof(path_str));
				printf("%s", path_str);
				scsi_print_inquiry(&lun->inq_data);
			} else {
				lun->state = CFI_LUN_READCAPACITY;
				cfi_lun_probe(lun, /*have_lock*/ 0);
			}
		}
		mtx_lock(&lun->softc->lock);
		STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
		mtx_unlock(&lun->softc->lock);
		ctl_free_io(io);
		break;
	}
	case CFI_LUN_READCAPACITY:
	case CFI_LUN_READCAPACITY_16: {
		uint64_t maxlba;
		uint32_t blocksize;

		maxlba = 0;
		blocksize = 0;

		if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS) {
			printf("%s: LUN %d probe failed because READ CAPACITY "
			       "failed\n", __func__, lun->lun_id);
			ctl_io_error_print(io, NULL);
		} else {

			if (lun->state == CFI_LUN_READCAPACITY) {
				struct scsi_read_capacity_data *rdcap;

				rdcap = (struct scsi_read_capacity_data *)
					io->scsiio.ext_data_ptr;

				maxlba = scsi_4btoul(rdcap->addr);
				blocksize = scsi_4btoul(rdcap->length);
				if (blocksize == 0) {
					printf("%s: LUN %d has invalid "
					       "blocksize 0, probe aborted\n",
					       __func__, lun->lun_id);
				} else if (maxlba == 0xffffffff) {
					lun->state = CFI_LUN_READCAPACITY_16;
					cfi_lun_probe(lun, /*have_lock*/ 0);
				} else
					lun->state = CFI_LUN_READY;
			} else {
				struct scsi_read_capacity_data_long *rdcap_long;

				rdcap_long = (struct
					scsi_read_capacity_data_long *)
					io->scsiio.ext_data_ptr;
				maxlba = scsi_8btou64(rdcap_long->addr);
				blocksize = scsi_4btoul(rdcap_long->length);

				if (blocksize == 0) {
					printf("%s: LUN %d has invalid "
					       "blocksize 0, probe aborted\n",
					       __func__, lun->lun_id);
				} else
					lun->state = CFI_LUN_READY;
			}
		}

		if (lun->state == CFI_LUN_READY) {
			char path_str[40];

			lun->num_blocks = maxlba + 1;
			lun->blocksize = blocksize;

			/*
			 * If this is true, the blocksize is a power of 2.
			 * We already checked for 0 above.
			 */
			if (((blocksize - 1) & blocksize) == 0) {
				int i;

				for (i = 0; i < 32; i++) {
					if ((blocksize & (1 << i)) != 0) {
						lun->blocksize_powerof2 = i;
						break;
					}
				}
			}
			ctl_scsi_path_string(io, path_str,sizeof(path_str));
			printf("%s", path_str);
			scsi_print_inquiry(&lun->inq_data);
			printf("%s %ju blocks, blocksize %d\n", path_str,
			       (uintmax_t)maxlba + 1, blocksize);
		}
		mtx_lock(&lun->softc->lock);
		STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
		mtx_unlock(&lun->softc->lock);
		free(io->scsiio.ext_data_ptr, M_CTL_CFI);
		ctl_free_io(io);
		break;
	}
	case CFI_LUN_READY:
	default:
		mtx_lock(&lun->softc->lock);
		/* How did we get here?? */
		STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
		mtx_unlock(&lun->softc->lock);
		ctl_free_io(io);
		break;
	}
}

static void
cfi_lun_probe(struct cfi_lun *lun, int have_lock)
{

	if (have_lock == 0)
		mtx_lock(&lun->softc->lock);
	if ((lun->softc->flags & CFI_ONLINE) == 0) {
		if (have_lock == 0)
			mtx_unlock(&lun->softc->lock);
		return;
	}
	if (have_lock == 0)
		mtx_unlock(&lun->softc->lock);

	switch (lun->state) {
	case CFI_LUN_INQUIRY: {
		struct cfi_lun_io *lun_io;
		union ctl_io *io;

		io = ctl_alloc_io(lun->softc->fe.ctl_pool_ref);
		if (io == NULL) {
			printf("%s: unable to alloc ctl_io for target %ju "
			       "lun %d probe\n", __func__,
			       (uintmax_t)lun->target_id.id, lun->lun_id);
			return;
		}
		ctl_scsi_inquiry(io,
				 /*data_ptr*/(uint8_t *)&lun->inq_data,
				 /*data_len*/ sizeof(lun->inq_data),
				 /*byte2*/ 0,
				 /*page_code*/ 0,
				 /*tag_type*/ CTL_TAG_SIMPLE,
				 /*control*/ 0);

		cfi_init_io(io,
			    /*lun*/ lun,
			    /*metatask*/ NULL,
			    /*policy*/ CFI_ERR_SOFT,
			    /*retries*/ 5,
			    /*orig_lun_io*/ NULL,
			    /*done_function*/
			    cfi_lun_probe_done);

		lun_io = (struct cfi_lun_io *)io->io_hdr.port_priv;

		if (have_lock == 0)
			mtx_lock(&lun->softc->lock);
		STAILQ_INSERT_TAIL(&lun->io_list, lun_io, links);
		if (have_lock == 0)
			mtx_unlock(&lun->softc->lock);

		if (ctl_queue(io) != CTL_RETVAL_COMPLETE) {
			printf("%s: error returned from ctl_queue()!\n",
			       __func__);
			STAILQ_REMOVE(&lun->io_list, lun_io,
				      cfi_lun_io, links);
			ctl_free_io(io);
		}
		break;
	}
	case CFI_LUN_READCAPACITY:
	case CFI_LUN_READCAPACITY_16: {
		struct cfi_lun_io *lun_io;
		uint8_t *dataptr;
		union ctl_io *io;

		io = ctl_alloc_io(lun->softc->fe.ctl_pool_ref);
		if (io == NULL) {
			printf("%s: unable to alloc ctl_io for target %ju "
			       "lun %d probe\n", __func__,
			       (uintmax_t)lun->target_id.id, lun->lun_id);
			return;
		}

		dataptr = malloc(sizeof(struct scsi_read_capacity_data_long),
				 M_CTL_CFI, M_NOWAIT);
		if (dataptr == NULL) {
			printf("%s: unable to allocate SCSI read capacity "
			       "buffer for target %ju lun %d\n", __func__,
			       (uintmax_t)lun->target_id.id, lun->lun_id);
			return;
		}
		if (lun->state == CFI_LUN_READCAPACITY) {
			ctl_scsi_read_capacity(io,
				/*data_ptr*/ dataptr,
				/*data_len*/
				sizeof(struct scsi_read_capacity_data_long),
				/*addr*/ 0,
				/*reladr*/ 0,
				/*pmi*/ 0,
				/*tag_type*/ CTL_TAG_SIMPLE,
				/*control*/ 0);
		} else {
			ctl_scsi_read_capacity_16(io,
				/*data_ptr*/ dataptr,
				/*data_len*/
				sizeof(struct scsi_read_capacity_data_long),
				/*addr*/ 0,
				/*reladr*/ 0,
				/*pmi*/ 0,
				/*tag_type*/ CTL_TAG_SIMPLE,
				/*control*/ 0);
		}
		cfi_init_io(io,
			    /*lun*/ lun,
			    /*metatask*/ NULL,
			    /*policy*/ CFI_ERR_SOFT,
			    /*retries*/ 7,
			    /*orig_lun_io*/ NULL,
			    /*done_function*/ cfi_lun_probe_done);

		lun_io = (struct cfi_lun_io *)io->io_hdr.port_priv;

		if (have_lock == 0)
			mtx_lock(&lun->softc->lock);
		STAILQ_INSERT_TAIL(&lun->io_list, lun_io, links);
		if (have_lock == 0)
			mtx_unlock(&lun->softc->lock);

		if (ctl_queue(io) != CTL_RETVAL_COMPLETE) {
			printf("%s: error returned from ctl_queue()!\n",
			       __func__);
			STAILQ_REMOVE(&lun->io_list, lun_io,
				      cfi_lun_io, links);
			free(dataptr, M_CTL_CFI);
			ctl_free_io(io);
		}
		break;
	}
	case CFI_LUN_READY:
	default:
		/* Why were we called? */
		break;
	}
}

static void
cfi_metatask_done(struct cfi_softc *softc, struct cfi_metatask *metatask)
{
	mtx_lock(&softc->lock);
	STAILQ_REMOVE(&softc->metatask_list, metatask, cfi_metatask, links);
	mtx_unlock(&softc->lock);

	/*
	 * Return status to the caller.  Caller allocated storage, and is
	 * responsible for calling cfi_free_metatask to release it once
	 * they've seen the status.
	 */
	metatask->callback(metatask->callback_arg, metatask);
}

static void
cfi_metatask_bbr_errorparse(struct cfi_metatask *metatask, union ctl_io *io)
{
	int error_code, sense_key, asc, ascq;

	if (metatask->tasktype != CFI_TASK_BBRREAD)
		return;

	if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS) {
		metatask->status = CFI_MT_SUCCESS;
		metatask->taskinfo.bbrread.status = CFI_BBR_SUCCESS;
		return;
	}

	if ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_SCSI_ERROR) {
		metatask->status = CFI_MT_ERROR;
		metatask->taskinfo.bbrread.status = CFI_BBR_ERROR;
		return;
	}

	metatask->taskinfo.bbrread.scsi_status = io->scsiio.scsi_status;
	memcpy(&metatask->taskinfo.bbrread.sense_data, &io->scsiio.sense_data,
	       ctl_min(sizeof(metatask->taskinfo.bbrread.sense_data),
		       sizeof(io->scsiio.sense_data)));

	if (io->scsiio.scsi_status == SCSI_STATUS_RESERV_CONFLICT) {
		metatask->status = CFI_MT_ERROR;
		metatask->taskinfo.bbrread.status = CFI_BBR_RESERV_CONFLICT;
		return;
	}

	if (io->scsiio.scsi_status != SCSI_STATUS_CHECK_COND) {
		metatask->status = CFI_MT_ERROR;
		metatask->taskinfo.bbrread.status = CFI_BBR_SCSI_ERROR;
		return;
	}

	scsi_extract_sense_len(&io->scsiio.sense_data,
			       io->scsiio.sense_len,
			       &error_code,
			       &sense_key,
			       &asc,
			       &ascq,
			       /*show_errors*/ 1);

	switch (error_code) {
	case SSD_DEFERRED_ERROR:
	case SSD_DESC_DEFERRED_ERROR:
		metatask->status = CFI_MT_ERROR;
		metatask->taskinfo.bbrread.status = CFI_BBR_SCSI_ERROR;
		break;
	case SSD_CURRENT_ERROR:
	case SSD_DESC_CURRENT_ERROR:
	default: {
		struct scsi_sense_data *sense;

		sense = &io->scsiio.sense_data;

		if ((asc == 0x04) && (ascq == 0x02)) {
			metatask->status = CFI_MT_ERROR;
			metatask->taskinfo.bbrread.status = CFI_BBR_LUN_STOPPED;
		} else if ((asc == 0x04) && (ascq == 0x03)) {
			metatask->status = CFI_MT_ERROR;
			metatask->taskinfo.bbrread.status =
				CFI_BBR_LUN_OFFLINE_CTL;
		} else if ((asc == 0x44) && (ascq == 0x00)) {
#ifdef NEEDTOPORT
			if (sense->sense_key_spec[0] & SSD_SCS_VALID) {
				uint16_t retry_count;

				retry_count = sense->sense_key_spec[1] << 8 |
					      sense->sense_key_spec[2];
				if (((retry_count & 0xf000) == CSC_RAIDCORE)
				 && ((retry_count & 0x0f00) == CSC_SHELF_SW)
				 && ((retry_count & 0xff) ==
				      RC_STS_DEVICE_OFFLINE)) {
					metatask->status = CFI_MT_ERROR;
					metatask->taskinfo.bbrread.status =
						CFI_BBR_LUN_OFFLINE_RC;
				} else {
					metatask->status = CFI_MT_ERROR;
					metatask->taskinfo.bbrread.status =
						CFI_BBR_SCSI_ERROR;
				}
			} else {
#endif /* NEEDTOPORT */
				metatask->status = CFI_MT_ERROR;
				metatask->taskinfo.bbrread.status =
					CFI_BBR_SCSI_ERROR;
#ifdef NEEDTOPORT
			}
#endif
		} else {
			metatask->status = CFI_MT_ERROR;
			metatask->taskinfo.bbrread.status = CFI_BBR_SCSI_ERROR;
		}
		break;
	}
	}
}

static void
cfi_metatask_io_done(union ctl_io *io)
{
	struct cfi_lun_io *lun_io;
	struct cfi_metatask *metatask;
	struct cfi_softc *softc;
	struct cfi_lun *lun;

	lun_io = (struct cfi_lun_io *)
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;

	lun = lun_io->lun;
	softc = lun->softc;

	metatask = lun_io->metatask;

	switch (metatask->tasktype) {
	case CFI_TASK_STARTUP:
	case CFI_TASK_SHUTDOWN: {
		int failed, done, is_start;

		failed = 0;
		done = 0;
		if (metatask->tasktype == CFI_TASK_STARTUP)
			is_start = 1;
		else
			is_start = 0;

		mtx_lock(&softc->lock);
		if ((io->io_hdr.status & CTL_STATUS_MASK) == CTL_SUCCESS)
			metatask->taskinfo.startstop.luns_complete++;
		else {
			metatask->taskinfo.startstop.luns_failed++;
			failed = 1;
		}
		if ((metatask->taskinfo.startstop.luns_complete +
		     metatask->taskinfo.startstop.luns_failed) >=
		     metatask->taskinfo.startstop.total_luns)
			done = 1;

		mtx_unlock(&softc->lock);

		if (failed != 0) {
			printf("%s: LUN %d %s request failed\n", __func__,
			       lun_io->lun->lun_id, (is_start == 1) ? "start" :
			       "stop");
			ctl_io_error_print(io, &lun_io->lun->inq_data);
		}
		if (done != 0) {
			if (metatask->taskinfo.startstop.luns_failed > 0)
				metatask->status = CFI_MT_ERROR;
			else
				metatask->status = CFI_MT_SUCCESS;
			cfi_metatask_done(softc, metatask);
		}
		mtx_lock(&softc->lock);
		STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
		mtx_unlock(&softc->lock);

		ctl_free_io(io);
		break;
	}
	case CFI_TASK_BBRREAD: {
		/*
		 * Translate the SCSI error into an enumeration.
		 */
		cfi_metatask_bbr_errorparse(metatask, io);

		mtx_lock(&softc->lock);
		STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
		mtx_unlock(&softc->lock);

		ctl_free_io(io);

		cfi_metatask_done(softc, metatask);
		break;
	}
	default:
		/*
		 * This shouldn't happen.
		 */
		mtx_lock(&softc->lock);
		STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
		mtx_unlock(&softc->lock);

		ctl_free_io(io);
		break;
	}
}

static void
cfi_err_recovery_done(union ctl_io *io)
{
	struct cfi_lun_io *lun_io, *orig_lun_io;
	struct cfi_lun *lun;
	union ctl_io *orig_io;

	lun_io = (struct cfi_lun_io *)io->io_hdr.port_priv;
	orig_lun_io = lun_io->orig_lun_io;
	orig_io = orig_lun_io->ctl_io;
	lun = lun_io->lun;

	if (io->io_hdr.status != CTL_SUCCESS) {
		printf("%s: error recovery action failed.  Original "
		       "error:\n", __func__);

		ctl_io_error_print(orig_lun_io->ctl_io, &lun->inq_data);

		printf("%s: error from error recovery action:\n", __func__);

		ctl_io_error_print(io, &lun->inq_data);

		printf("%s: trying original command again...\n", __func__);
	}

	mtx_lock(&lun->softc->lock);
	STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
	mtx_unlock(&lun->softc->lock);
	ctl_free_io(io);

	orig_io->io_hdr.retries--;
	orig_io->io_hdr.status = CTL_STATUS_NONE;

	if (ctl_queue(orig_io) != CTL_RETVAL_COMPLETE) {
		printf("%s: error returned from ctl_queue()!\n", __func__);
		STAILQ_REMOVE(&lun->io_list, orig_lun_io,
			      cfi_lun_io, links);
		ctl_free_io(orig_io);
	}
}

static void
cfi_lun_io_done(union ctl_io *io)
{
	struct cfi_lun *lun;
	struct cfi_lun_io *lun_io;

	lun_io = (struct cfi_lun_io *)
		io->io_hdr.ctl_private[CTL_PRIV_FRONTEND].ptr;
	lun = lun_io->lun;

	if (lun_io->metatask == NULL) {
		printf("%s: I/O has no metatask pointer, discarding\n",
		       __func__);
		STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
		ctl_free_io(io);
		return;
	}
	cfi_metatask_io_done(io);
}

void
cfi_action(struct cfi_metatask *metatask)
{
	struct cfi_softc *softc;

	softc = &fetd_internal_softc;

	mtx_lock(&softc->lock);

	STAILQ_INSERT_TAIL(&softc->metatask_list, metatask, links);

	if ((softc->flags & CFI_ONLINE) == 0) {
		mtx_unlock(&softc->lock);
		metatask->status = CFI_MT_PORT_OFFLINE;
		cfi_metatask_done(softc, metatask);
		return;
	} else
		mtx_unlock(&softc->lock);

	switch (metatask->tasktype) {
	case CFI_TASK_STARTUP:
	case CFI_TASK_SHUTDOWN: {
		union ctl_io *io;
		int da_luns, ios_allocated, do_start;
		struct cfi_lun *lun;
		STAILQ_HEAD(, ctl_io_hdr) tmp_io_list;

		da_luns = 0;
		ios_allocated = 0;
		STAILQ_INIT(&tmp_io_list);

		if (metatask->tasktype == CFI_TASK_STARTUP)
			do_start = 1;
		else
			do_start = 0;

		mtx_lock(&softc->lock);
		STAILQ_FOREACH(lun, &softc->lun_list, links) {
			if (lun->state != CFI_LUN_READY)
				continue;

			if (SID_TYPE(&lun->inq_data) != T_DIRECT)
				continue;
			da_luns++;
			io = ctl_alloc_io(softc->fe.ctl_pool_ref);
			if (io != NULL) {
				ios_allocated++;
				STAILQ_INSERT_TAIL(&tmp_io_list, &io->io_hdr,
						   links);
			}
		}

		if (ios_allocated < da_luns) {
			printf("%s: error allocating ctl_io for %s\n",
			       __func__, (do_start == 1) ? "startup" :
			       "shutdown");
			da_luns = ios_allocated;
		}

		metatask->taskinfo.startstop.total_luns = da_luns;

		STAILQ_FOREACH(lun, &softc->lun_list, links) {
			struct cfi_lun_io *lun_io;

			if (lun->state != CFI_LUN_READY)
				continue;

			if (SID_TYPE(&lun->inq_data) != T_DIRECT)
				continue;

			io = (union ctl_io *)STAILQ_FIRST(&tmp_io_list);
			if (io == NULL)
				break;

			STAILQ_REMOVE(&tmp_io_list, &io->io_hdr, ctl_io_hdr,
				      links);

			ctl_scsi_start_stop(io,
					    /*start*/ do_start,
					    /*load_eject*/ 0,
					    /*immediate*/ 0,
					    /*power_conditions*/
					    SSS_PC_START_VALID,
					    /*onoffline*/ 1,
					    /*ctl_tag_type*/ CTL_TAG_ORDERED,
					    /*control*/ 0);

			cfi_init_io(io,
				    /*lun*/ lun,
				    /*metatask*/ metatask,
				    /*policy*/ CFI_ERR_HARD,
				    /*retries*/ 3,
				    /*orig_lun_io*/ NULL,
				    /*done_function*/ cfi_lun_io_done);

			lun_io = (struct cfi_lun_io *) io->io_hdr.port_priv;

			STAILQ_INSERT_TAIL(&lun->io_list, lun_io, links);

			if (ctl_queue(io) != CTL_RETVAL_COMPLETE) {
				printf("%s: error returned from ctl_queue()!\n",
				       __func__);
				STAILQ_REMOVE(&lun->io_list, lun_io,
					      cfi_lun_io, links);
				ctl_free_io(io);
				metatask->taskinfo.startstop.total_luns--;
			}
		}

		if (STAILQ_FIRST(&tmp_io_list) != NULL) {
			printf("%s: error: tmp_io_list != NULL\n", __func__);
			for (io = (union ctl_io *)STAILQ_FIRST(&tmp_io_list);
			     io != NULL;
			     io = (union ctl_io *)STAILQ_FIRST(&tmp_io_list)) {
				STAILQ_REMOVE(&tmp_io_list, &io->io_hdr,
					      ctl_io_hdr, links);
				ctl_free_io(io);
			}
		}
		mtx_unlock(&softc->lock);

		break;
	}
	case CFI_TASK_BBRREAD: {
		union ctl_io *io;
		struct cfi_lun *lun;
		struct cfi_lun_io *lun_io;
		cfi_bbrread_status status;
		int req_lun_num;
		uint32_t num_blocks;

		status = CFI_BBR_SUCCESS;

		req_lun_num = metatask->taskinfo.bbrread.lun_num;

		mtx_lock(&softc->lock);
		STAILQ_FOREACH(lun, &softc->lun_list, links) {
			if (lun->lun_id != req_lun_num)
				continue;
			if (lun->state != CFI_LUN_READY) {
				status = CFI_BBR_LUN_UNCONFIG;
				break;
			} else
				break;
		}

		if (lun == NULL)
			status = CFI_BBR_NO_LUN;

		if (status != CFI_BBR_SUCCESS) {
			metatask->status = CFI_MT_ERROR;
			metatask->taskinfo.bbrread.status = status;
			mtx_unlock(&softc->lock);
			cfi_metatask_done(softc, metatask);
			break;
		}

		/*
		 * Convert the number of bytes given into blocks and check
		 * that the number of bytes is a multiple of the blocksize.
		 * CTL will verify that the LBA is okay.
		 */
		if (lun->blocksize_powerof2 != 0) {
			if ((metatask->taskinfo.bbrread.len &
			    (lun->blocksize - 1)) != 0) {
				metatask->status = CFI_MT_ERROR;
				metatask->taskinfo.bbrread.status =
					CFI_BBR_BAD_LEN;
				cfi_metatask_done(softc, metatask);
				break;
			}

			num_blocks = metatask->taskinfo.bbrread.len >>
				lun->blocksize_powerof2;
		} else {
			/*
			 * XXX KDM this could result in floating point
			 * division, which isn't supported in the kernel on
			 * x86 at least.
			 */
			if ((metatask->taskinfo.bbrread.len %
			     lun->blocksize) != 0) {
				metatask->status = CFI_MT_ERROR;
				metatask->taskinfo.bbrread.status =
					CFI_BBR_BAD_LEN;
				cfi_metatask_done(softc, metatask);
				break;
			}

			/*
			 * XXX KDM this could result in floating point
			 * division in some cases.
			 */
			num_blocks = metatask->taskinfo.bbrread.len /
				lun->blocksize;

		}

		io = ctl_alloc_io(softc->fe.ctl_pool_ref);
		if (io == NULL) {
			metatask->status = CFI_MT_ERROR;
			metatask->taskinfo.bbrread.status = CFI_BBR_NO_MEM;
			mtx_unlock(&softc->lock);
			cfi_metatask_done(softc, metatask);
			break;
		}

		/*
		 * XXX KDM need to do a read capacity to get the blocksize
		 * for this device.
		 */
		ctl_scsi_read_write(io,
				    /*data_ptr*/ NULL,
				    /*data_len*/ metatask->taskinfo.bbrread.len,
				    /*read_op*/ 1,
				    /*byte2*/ 0,
				    /*minimum_cdb_size*/ 0,
				    /*lba*/ metatask->taskinfo.bbrread.lba,
				    /*num_blocks*/ num_blocks,
				    /*tag_type*/ CTL_TAG_SIMPLE,
				    /*control*/ 0);

		cfi_init_io(io,
			    /*lun*/ lun,
			    /*metatask*/ metatask,
			    /*policy*/ CFI_ERR_SOFT,
			    /*retries*/ 3,
			    /*orig_lun_io*/ NULL,
			    /*done_function*/ cfi_lun_io_done);

		lun_io = (struct cfi_lun_io *)io->io_hdr.port_priv;

		STAILQ_INSERT_TAIL(&lun->io_list, lun_io, links);

		if (ctl_queue(io) != CTL_RETVAL_COMPLETE) {
			printf("%s: error returned from ctl_queue()!\n",
			       __func__);
			STAILQ_REMOVE(&lun->io_list, lun_io, cfi_lun_io, links);
			ctl_free_io(io);
			metatask->status = CFI_MT_ERROR;
			metatask->taskinfo.bbrread.status = CFI_BBR_ERROR;
			mtx_unlock(&softc->lock);
			cfi_metatask_done(softc, metatask);
			break;
		}

		mtx_unlock(&softc->lock);
		break;
	}
	default:
		panic("invalid metatask type %d", metatask->tasktype);
		break; /* NOTREACHED */
	}
}

#ifdef oldapi
void
cfi_shutdown_shelf(cfi_cb_t callback, void *callback_arg)
{
	struct ctl_mem_element *element;
	struct cfi_softc *softc;
	struct cfi_metatask *metatask;

	softc = &fetd_internal_softc;

	element = ctl_alloc_mem_element(&softc->metatask_pool, /*can_wait*/ 0);
	if (element == NULL) {
		callback(callback_arg,
			 /*status*/ CFI_MT_ERROR,
			 /*sluns_found*/ 0,
			 /*sluns_complete*/ 0,
			 /*sluns_failed*/ 0);
		return;
	}

	metatask = (struct cfi_metatask *)element->bytes;

	memset(metatask, 0, sizeof(*metatask));
	metatask->tasktype = CFI_TASK_SHUTDOWN;
	metatask->status = CFI_MT_NONE;
	metatask->taskinfo.startstop.callback = callback;
	metatask->taskinfo.startstop.callback_arg = callback_arg;
	metatask->element = element;

	cfi_action(softc, metatask);

	/*
	 * - send a report luns to lun 0, get LUN list.
	 * - send an inquiry to each lun
	 * - send a stop/offline to each direct access LUN
	 *    - if we get a reservation conflict, reset the LUN and then
	 *      retry sending the stop/offline
	 * - return status back to the caller
	 */
}

void
cfi_start_shelf(cfi_cb_t callback, void *callback_arg)
{
	struct ctl_mem_element *element;
	struct cfi_softc *softc;
	struct cfi_metatask *metatask;

	softc = &fetd_internal_softc;

	element = ctl_alloc_mem_element(&softc->metatask_pool, /*can_wait*/ 0);
	if (element == NULL) {
		callback(callback_arg,
			 /*status*/ CFI_MT_ERROR,
			 /*sluns_found*/ 0,
			 /*sluns_complete*/ 0,
			 /*sluns_failed*/ 0);
		return;
	}

	metatask = (struct cfi_metatask *)element->bytes;

	memset(metatask, 0, sizeof(*metatask));
	metatask->tasktype = CFI_TASK_STARTUP;
	metatask->status = CFI_MT_NONE;
	metatask->taskinfo.startstop.callback = callback;
	metatask->taskinfo.startstop.callback_arg = callback_arg;
	metatask->element = element;

	cfi_action(softc, metatask);

	/*
	 * - send a report luns to lun 0, get LUN list.
	 * - send an inquiry to each lun
	 * - send a stop/offline to each direct access LUN
	 *    - if we get a reservation conflict, reset the LUN and then
	 *      retry sending the stop/offline
	 * - return status back to the caller
	 */
}

#endif

struct cfi_metatask *
cfi_alloc_metatask(int can_wait)
{
	struct ctl_mem_element *element;
	struct cfi_metatask *metatask;
	struct cfi_softc *softc;

	softc = &fetd_internal_softc;

	element = ctl_alloc_mem_element(&softc->metatask_pool, can_wait);
	if (element == NULL)
		return (NULL);

	metatask = (struct cfi_metatask *)element->bytes;
	memset(metatask, 0, sizeof(*metatask));
	metatask->status = CFI_MT_NONE;
	metatask->element = element;

	return (metatask);
}

void
cfi_free_metatask(struct cfi_metatask *metatask)
{
	ctl_free_mem_element(metatask->element);
}

/*
 * vim: ts=8
 */
