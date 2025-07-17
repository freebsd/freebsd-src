/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2003 Silicon Graphics International Corp.
 * Copyright (c) 2009-2011 Spectra Logic Corporation
 * Copyright (c) 2012,2021 The FreeBSD Foundation
 * Copyright (c) 2014-2021 Alexander Motin <mav@FreeBSD.org>
 * All rights reserved.
 *
 * Portions of this software were developed by Edward Tomasz Napierala
 * under sponsorship from the FreeBSD Foundation.
 *
 * Portions of this software were developed by Ka Ho Ng <khng@FreeBSD.org>
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
 * $Id: //depot/users/kenm/FreeBSD-test2/sys/cam/ctl/ctl_backend_block.c#5 $
 */
/*
 * CAM Target Layer driver backend for block devices.
 *
 * Author: Ken Merry <ken@FreeBSD.org>
 */
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/types.h>
#include <sys/kthread.h>
#include <sys/bio.h>
#include <sys/fcntl.h>
#include <sys/limits.h>
#include <sys/lock.h>
#include <sys/mutex.h>
#include <sys/condvar.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/ioccom.h>
#include <sys/queue.h>
#include <sys/sbuf.h>
#include <sys/endian.h>
#include <sys/uio.h>
#include <sys/buf.h>
#include <sys/taskqueue.h>
#include <sys/vnode.h>
#include <sys/namei.h>
#include <sys/mount.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/filedesc.h>
#include <sys/filio.h>
#include <sys/proc.h>
#include <sys/pcpu.h>
#include <sys/module.h>
#include <sys/sdt.h>
#include <sys/devicestat.h>
#include <sys/sysctl.h>
#include <sys/nv.h>
#include <sys/dnv.h>
#include <sys/sx.h>
#include <sys/unistd.h>

#include <geom/geom.h>

#include <cam/cam.h>
#include <cam/scsi/scsi_all.h>
#include <cam/scsi/scsi_da.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_backend.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_scsi_all.h>
#include <cam/ctl/ctl_private.h>
#include <cam/ctl/ctl_error.h>

/*
 * The idea here is to allocate enough S/G space to handle at least 1MB I/Os.
 * On systems with small maxphys it can be 8 128KB segments.  On large systems
 * it can be up to 8 1MB segments.  I/Os larger than that we'll split.
 */
#define	CTLBLK_MAX_SEGS		8
#define	CTLBLK_HALF_SEGS	(CTLBLK_MAX_SEGS / 2)
#define	CTLBLK_MIN_SEG		(128 * 1024)
#define	CTLBLK_MAX_SEG		MIN(1024 * 1024, MAX(CTLBLK_MIN_SEG, maxphys))
#define	CTLBLK_MAX_IO_SIZE	(CTLBLK_MAX_SEG * CTLBLK_MAX_SEGS)

#ifdef CTLBLK_DEBUG
#define DPRINTF(fmt, args...) \
    printf("cbb(%s:%d): " fmt, __FUNCTION__, __LINE__, ##args)
#else
#define DPRINTF(fmt, args...) do {} while(0)
#endif

#define PRIV(io)	\
    ((struct ctl_ptr_len_flags *)&(io)->io_hdr.ctl_private[CTL_PRIV_BACKEND])
#define ARGS(io)	\
    ((struct ctl_lba_len_flags *)&(io)->io_hdr.ctl_private[CTL_PRIV_LBA_LEN])
#define	DSM_RANGE(io)	((io)->io_hdr.ctl_private[CTL_PRIV_LBA_LEN].integer)

SDT_PROVIDER_DEFINE(cbb);

typedef enum {
	CTL_BE_BLOCK_LUN_UNCONFIGURED	= 0x01,
	CTL_BE_BLOCK_LUN_WAITING	= 0x04,
} ctl_be_block_lun_flags;

typedef enum {
	CTL_BE_BLOCK_NONE,
	CTL_BE_BLOCK_DEV,
	CTL_BE_BLOCK_FILE
} ctl_be_block_type;

struct ctl_be_block_filedata {
	struct ucred *cred;
};

union ctl_be_block_bedata {
	struct ctl_be_block_filedata file;
};

struct ctl_be_block_io;
struct ctl_be_block_lun;

typedef void (*cbb_dispatch_t)(struct ctl_be_block_lun *be_lun,
			       struct ctl_be_block_io *beio);
typedef uint64_t (*cbb_getattr_t)(struct ctl_be_block_lun *be_lun,
				  const char *attrname);

/*
 * Backend LUN structure.  There is a 1:1 mapping between a block device
 * and a backend block LUN, and between a backend block LUN and a CTL LUN.
 */
struct ctl_be_block_lun {
	struct ctl_be_lun cbe_lun;		/* Must be first element. */
	struct ctl_lun_create_params params;
	char *dev_path;
	ctl_be_block_type dev_type;
	struct vnode *vn;
	union ctl_be_block_bedata backend;
	cbb_dispatch_t dispatch;
	cbb_dispatch_t lun_flush;
	cbb_dispatch_t unmap;
	cbb_dispatch_t get_lba_status;
	cbb_getattr_t getattr;
	uint64_t size_blocks;
	uint64_t size_bytes;
	struct ctl_be_block_softc *softc;
	struct devstat *disk_stats;
	ctl_be_block_lun_flags flags;
	SLIST_ENTRY(ctl_be_block_lun) links;
	struct taskqueue *io_taskqueue;
	struct task io_task;
	int num_threads;
	STAILQ_HEAD(, ctl_io_hdr) input_queue;
	STAILQ_HEAD(, ctl_io_hdr) config_read_queue;
	STAILQ_HEAD(, ctl_io_hdr) config_write_queue;
	STAILQ_HEAD(, ctl_io_hdr) datamove_queue;
	struct mtx_padalign io_lock;
	struct mtx_padalign queue_lock;
};

/*
 * Overall softc structure for the block backend module.
 */
struct ctl_be_block_softc {
	struct sx			 modify_lock;
	struct mtx			 lock;
	int				 num_luns;
	SLIST_HEAD(, ctl_be_block_lun)	 lun_list;
	uma_zone_t			 beio_zone;
	uma_zone_t			 bufmin_zone;
	uma_zone_t			 bufmax_zone;
};

static struct ctl_be_block_softc backend_block_softc;

/*
 * Per-I/O information.
 */
struct ctl_be_block_io {
	union ctl_io			*io;
	struct ctl_sg_entry		sg_segs[CTLBLK_MAX_SEGS];
	struct iovec			xiovecs[CTLBLK_MAX_SEGS];
	int				refcnt;
	int				bio_cmd;
	int				two_sglists;
	int				num_segs;
	int				num_bios_sent;
	int				num_bios_done;
	int				send_complete;
	int				first_error;
	uint64_t			first_error_offset;
	struct bintime			ds_t0;
	devstat_tag_type		ds_tag_type;
	devstat_trans_flags		ds_trans_type;
	uint64_t			io_len;
	uint64_t			io_offset;
	int				io_arg;
	struct ctl_be_block_softc	*softc;
	struct ctl_be_block_lun		*lun;
	void (*beio_cont)(struct ctl_be_block_io *beio); /* to continue processing */
};

static int cbb_num_threads = 32;
SYSCTL_NODE(_kern_cam_ctl, OID_AUTO, block, CTLFLAG_RD | CTLFLAG_MPSAFE, 0,
	    "CAM Target Layer Block Backend");
SYSCTL_INT(_kern_cam_ctl_block, OID_AUTO, num_threads, CTLFLAG_RWTUN,
           &cbb_num_threads, 0, "Number of threads per backing file");

static struct ctl_be_block_io *ctl_alloc_beio(struct ctl_be_block_softc *softc);
static void ctl_free_beio(struct ctl_be_block_io *beio);
static void ctl_complete_beio(struct ctl_be_block_io *beio);
static int ctl_be_block_move_done(union ctl_io *io, bool samethr);
static void ctl_be_block_biodone(struct bio *bio);
static void ctl_be_block_flush_file(struct ctl_be_block_lun *be_lun,
				    struct ctl_be_block_io *beio);
static void ctl_be_block_dispatch_file(struct ctl_be_block_lun *be_lun,
				       struct ctl_be_block_io *beio);
static void ctl_be_block_gls_file(struct ctl_be_block_lun *be_lun,
				  struct ctl_be_block_io *beio);
static uint64_t ctl_be_block_getattr_file(struct ctl_be_block_lun *be_lun,
					 const char *attrname);
static void ctl_be_block_unmap_file(struct ctl_be_block_lun *be_lun,
				    struct ctl_be_block_io *beio);
static void ctl_be_block_flush_dev(struct ctl_be_block_lun *be_lun,
				   struct ctl_be_block_io *beio);
static void ctl_be_block_unmap_dev(struct ctl_be_block_lun *be_lun,
				   struct ctl_be_block_io *beio);
static void ctl_be_block_dispatch_dev(struct ctl_be_block_lun *be_lun,
				      struct ctl_be_block_io *beio);
static uint64_t ctl_be_block_getattr_dev(struct ctl_be_block_lun *be_lun,
					 const char *attrname);
static void ctl_be_block_cr_dispatch(struct ctl_be_block_lun *be_lun,
				    union ctl_io *io);
static void ctl_be_block_cw_dispatch(struct ctl_be_block_lun *be_lun,
				    union ctl_io *io);
static void ctl_be_block_dispatch(struct ctl_be_block_lun *be_lun,
				  union ctl_io *io);
static void ctl_be_block_worker(void *context, int pending);
static int ctl_be_block_submit(union ctl_io *io);
static int ctl_be_block_ioctl(struct cdev *dev, u_long cmd, caddr_t addr,
				   int flag, struct thread *td);
static int ctl_be_block_open_file(struct ctl_be_block_lun *be_lun,
				  struct ctl_lun_req *req);
static int ctl_be_block_open_dev(struct ctl_be_block_lun *be_lun,
				 struct ctl_lun_req *req);
static int ctl_be_block_close(struct ctl_be_block_lun *be_lun);
static int ctl_be_block_open(struct ctl_be_block_lun *be_lun,
			     struct ctl_lun_req *req);
static int ctl_be_block_create(struct ctl_be_block_softc *softc,
			       struct ctl_lun_req *req);
static int ctl_be_block_rm(struct ctl_be_block_softc *softc,
			   struct ctl_lun_req *req);
static int ctl_be_block_modify(struct ctl_be_block_softc *softc,
			   struct ctl_lun_req *req);
static void ctl_be_block_lun_shutdown(struct ctl_be_lun *cbe_lun);
static int ctl_be_block_config_write(union ctl_io *io);
static int ctl_be_block_config_read(union ctl_io *io);
static int ctl_be_block_lun_info(struct ctl_be_lun *cbe_lun, struct sbuf *sb);
static uint64_t ctl_be_block_lun_attr(struct ctl_be_lun *cbe_lun, const char *attrname);
static int ctl_be_block_init(void);
static int ctl_be_block_shutdown(void);

static struct ctl_backend_driver ctl_be_block_driver = 
{
	.name = "block",
	.flags = CTL_BE_FLAG_HAS_CONFIG,
	.init = ctl_be_block_init,
	.shutdown = ctl_be_block_shutdown,
	.data_submit = ctl_be_block_submit,
	.config_read = ctl_be_block_config_read,
	.config_write = ctl_be_block_config_write,
	.ioctl = ctl_be_block_ioctl,
	.lun_info = ctl_be_block_lun_info,
	.lun_attr = ctl_be_block_lun_attr
};

MALLOC_DEFINE(M_CTLBLK, "ctlblock", "Memory used for CTL block backend");
CTL_BACKEND_DECLARE(cbb, ctl_be_block_driver);

static void
ctl_alloc_seg(struct ctl_be_block_softc *softc, struct ctl_sg_entry *sg,
    size_t len)
{

	if (len <= CTLBLK_MIN_SEG) {
		sg->addr = uma_zalloc(softc->bufmin_zone, M_WAITOK);
	} else {
		KASSERT(len <= CTLBLK_MAX_SEG,
		    ("Too large alloc %zu > %lu", len, CTLBLK_MAX_SEG));
		sg->addr = uma_zalloc(softc->bufmax_zone, M_WAITOK);
	}
	sg->len = len;
}

static void
ctl_free_seg(struct ctl_be_block_softc *softc, struct ctl_sg_entry *sg)
{

	if (sg->len <= CTLBLK_MIN_SEG) {
		uma_zfree(softc->bufmin_zone, sg->addr);
	} else {
		KASSERT(sg->len <= CTLBLK_MAX_SEG,
		    ("Too large free %zu > %lu", sg->len, CTLBLK_MAX_SEG));
		uma_zfree(softc->bufmax_zone, sg->addr);
	}
}

static struct ctl_be_block_io *
ctl_alloc_beio(struct ctl_be_block_softc *softc)
{
	struct ctl_be_block_io *beio;

	beio = uma_zalloc(softc->beio_zone, M_WAITOK | M_ZERO);
	beio->softc = softc;
	beio->refcnt = 1;
	return (beio);
}

static void
ctl_real_free_beio(struct ctl_be_block_io *beio)
{
	struct ctl_be_block_softc *softc = beio->softc;
	int i;

	for (i = 0; i < beio->num_segs; i++) {
		ctl_free_seg(softc, &beio->sg_segs[i]);

		/* For compare we had two equal S/G lists. */
		if (beio->two_sglists) {
			ctl_free_seg(softc,
			    &beio->sg_segs[i + CTLBLK_HALF_SEGS]);
		}
	}

	uma_zfree(softc->beio_zone, beio);
}

static void
ctl_refcnt_beio(void *arg, int diff)
{
	struct ctl_be_block_io *beio = arg;

	if (atomic_fetchadd_int(&beio->refcnt, diff) + diff == 0)
		ctl_real_free_beio(beio);
}

static void
ctl_free_beio(struct ctl_be_block_io *beio)
{

	ctl_refcnt_beio(beio, -1);
}

static void
ctl_complete_beio(struct ctl_be_block_io *beio)
{
	union ctl_io *io = beio->io;

	if (beio->beio_cont != NULL) {
		beio->beio_cont(beio);
	} else {
		ctl_free_beio(beio);
		ctl_data_submit_done(io);
	}
}

static void
ctl_be_block_io_error(union ctl_io *io, int bio_cmd, uint16_t retry_count)
{
	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		if (bio_cmd == BIO_FLUSH) {
			/* XXX KDM is there is a better error here? */
			ctl_set_internal_failure(&io->scsiio,
						 /*sks_valid*/ 1,
						 retry_count);
		} else {
			ctl_set_medium_error(&io->scsiio, bio_cmd == BIO_READ);
		}
		break;
	case CTL_IO_NVME:
		switch (bio_cmd) {
		case BIO_FLUSH:
		case BIO_WRITE:
			ctl_nvme_set_write_fault(&io->nvmeio);
			break;
		case BIO_READ:
			ctl_nvme_set_unrecoverable_read_error(&io->nvmeio);
			break;
		default:
			ctl_nvme_set_internal_error(&io->nvmeio);
			break;
		}
		break;
	default:
		__assert_unreachable();
	}
}

static size_t
cmp(uint8_t *a, uint8_t *b, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++) {
		if (a[i] != b[i])
			break;
	}
	return (i);
}

static void
ctl_be_block_compare(union ctl_io *io)
{
	struct ctl_be_block_io *beio;
	uint64_t off, res;
	int i;

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;
	off = 0;
	for (i = 0; i < beio->num_segs; i++) {
		res = cmp(beio->sg_segs[i].addr,
		    beio->sg_segs[i + CTLBLK_HALF_SEGS].addr,
		    beio->sg_segs[i].len);
		off += res;
		if (res < beio->sg_segs[i].len)
			break;
	}
	if (i < beio->num_segs) {
		ctl_io_set_compare_failure(io, off);
	} else
		ctl_io_set_success(io);
}

static int
ctl_be_block_move_done(union ctl_io *io, bool samethr)
{
	struct ctl_be_block_io *beio;
	struct ctl_be_block_lun *be_lun;
	struct ctl_lba_len_flags *lbalen;

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;

	DPRINTF("entered\n");
	ctl_add_kern_rel_offset(io, ctl_kern_data_len(io));

	/*
	 * We set status at this point for read and compare commands.
	 */
	if ((io->io_hdr.flags & CTL_FLAG_ABORT) == 0 &&
	    (io->io_hdr.status & CTL_STATUS_MASK) == CTL_STATUS_NONE) {
		lbalen = ARGS(io);
		if (lbalen->flags & CTL_LLF_READ) {
			ctl_io_set_success(io);
		} else if (lbalen->flags & CTL_LLF_COMPARE) {
			/* We have two data blocks ready for comparison. */
			ctl_be_block_compare(io);
		}
	}

	/*
	 * If this is a read, or a write with errors, it is done.
	 */
	if ((beio->bio_cmd == BIO_READ)
	 || ((io->io_hdr.flags & CTL_FLAG_ABORT) != 0)
	 || ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE)) {
		ctl_complete_beio(beio);
		return (0);
	}

	/*
	 * At this point, we have a write and the DMA completed successfully.
	 * If we were called synchronously in the original thread then just
	 * dispatch, otherwise we now have to queue it to the task queue to
	 * execute the backend I/O.  That is because we do blocking
	 * memory allocations, and in the file backing case, blocking I/O.
	 * This move done routine is generally called in the SIM's
	 * interrupt context, and therefore we cannot block.
	 */
	be_lun = (struct ctl_be_block_lun *)CTL_BACKEND_LUN(io);
	if (samethr) {
		be_lun->dispatch(be_lun, beio);
	} else {
		mtx_lock(&be_lun->queue_lock);
		STAILQ_INSERT_TAIL(&be_lun->datamove_queue, &io->io_hdr, links);
		mtx_unlock(&be_lun->queue_lock);
		taskqueue_enqueue(be_lun->io_taskqueue, &be_lun->io_task);
	}
	return (0);
}

static void
ctl_be_block_biodone(struct bio *bio)
{
	struct ctl_be_block_io *beio = bio->bio_caller1;
	struct ctl_be_block_lun *be_lun = beio->lun;
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	union ctl_io *io;
	int error;

	io = beio->io;

	DPRINTF("entered\n");

	error = bio->bio_error;
	mtx_lock(&be_lun->io_lock);
	if (error != 0 &&
	    (beio->first_error == 0 ||
	     bio->bio_offset < beio->first_error_offset)) {
		beio->first_error = error;
		beio->first_error_offset = bio->bio_offset;
	}

	beio->num_bios_done++;

	/*
	 * XXX KDM will this cause WITNESS to complain?  Holding a lock
	 * during the free might cause it to complain.
	 */
	g_destroy_bio(bio);

	/*
	 * If the send complete bit isn't set, or we aren't the last I/O to
	 * complete, then we're done.
	 */
	if ((beio->send_complete == 0)
	 || (beio->num_bios_done < beio->num_bios_sent)) {
		mtx_unlock(&be_lun->io_lock);
		return;
	}

	/*
	 * At this point, we've verified that we are the last I/O to
	 * complete, so it's safe to drop the lock.
	 */
	devstat_end_transaction(beio->lun->disk_stats, beio->io_len,
	    beio->ds_tag_type, beio->ds_trans_type,
	    /*now*/ NULL, /*then*/&beio->ds_t0);
	mtx_unlock(&be_lun->io_lock);

	/*
	 * If there are any errors from the backing device, we fail the
	 * entire I/O with a medium error.
	 */
	error = beio->first_error;
	if (error != 0) {
		if (error == EOPNOTSUPP) {
			ctl_io_set_invalid_opcode(io);
		} else if (error == ENOSPC || error == EDQUOT) {
			ctl_io_set_space_alloc_fail(io);
		} else if (error == EROFS || error == EACCES) {
			ctl_io_set_hw_write_protected(io);
		} else {
			ctl_be_block_io_error(io, beio->bio_cmd,
			    /*retry_count*/ 0xbad2);
		}
		ctl_complete_beio(beio);
		return;
	}

	/*
	 * If this is a write, a flush, a delete or verify, we're all done.
	 * If this is a read, we can now send the data to the user.
	 */
	if ((beio->bio_cmd == BIO_WRITE)
	 || (beio->bio_cmd == BIO_FLUSH)
	 || (beio->bio_cmd == BIO_DELETE)
	 || (ARGS(io)->flags & CTL_LLF_VERIFY)) {
		ctl_io_set_success(io);
		ctl_complete_beio(beio);
	} else {
		if ((ARGS(io)->flags & CTL_LLF_READ) &&
		    beio->beio_cont == NULL) {
			ctl_io_set_success(io);
			if (cbe_lun->serseq >= CTL_LUN_SERSEQ_SOFT)
				ctl_serseq_done(io);
		}
		ctl_datamove(io);
	}
}

static void
ctl_be_block_flush_file(struct ctl_be_block_lun *be_lun,
			struct ctl_be_block_io *beio)
{
	union ctl_io *io = beio->io;
	struct mount *mountpoint;
	int error;

	DPRINTF("entered\n");

	binuptime(&beio->ds_t0);
	devstat_start_transaction(beio->lun->disk_stats, &beio->ds_t0);

	(void) vn_start_write(be_lun->vn, &mountpoint, V_WAIT);

	vn_lock(be_lun->vn, vn_lktype_write(mountpoint, be_lun->vn) |
	    LK_RETRY);
	error = VOP_FSYNC(be_lun->vn, beio->io_arg ? MNT_NOWAIT : MNT_WAIT,
	    curthread);
	VOP_UNLOCK(be_lun->vn);

	vn_finished_write(mountpoint);

	mtx_lock(&be_lun->io_lock);
	devstat_end_transaction(beio->lun->disk_stats, beio->io_len,
	    beio->ds_tag_type, beio->ds_trans_type,
	    /*now*/ NULL, /*then*/&beio->ds_t0);
	mtx_unlock(&be_lun->io_lock);

	if (error == 0)
		ctl_io_set_success(io);
	else {
		ctl_be_block_io_error(io, BIO_FLUSH,
		    /*retry_count*/ 0xbad1);
	}

	ctl_complete_beio(beio);
}

SDT_PROBE_DEFINE1(cbb, , read, file_start, "uint64_t");
SDT_PROBE_DEFINE1(cbb, , write, file_start, "uint64_t");
SDT_PROBE_DEFINE1(cbb, , read, file_done,"uint64_t");
SDT_PROBE_DEFINE1(cbb, , write, file_done, "uint64_t");

static void
ctl_be_block_dispatch_file(struct ctl_be_block_lun *be_lun,
			   struct ctl_be_block_io *beio)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct ctl_be_block_filedata *file_data;
	union ctl_io *io;
	struct uio xuio;
	struct iovec *xiovec;
	size_t s;
	int error, flags, i;

	DPRINTF("entered\n");

	file_data = &be_lun->backend.file;
	io = beio->io;
	flags = 0;
	if (ARGS(io)->flags & CTL_LLF_DPO)
		flags |= IO_DIRECT;
	if (beio->bio_cmd == BIO_WRITE && ARGS(io)->flags & CTL_LLF_FUA)
		flags |= IO_SYNC;

	bzero(&xuio, sizeof(xuio));
	if (beio->bio_cmd == BIO_READ) {
		SDT_PROBE0(cbb, , read, file_start);
		xuio.uio_rw = UIO_READ;
	} else {
		SDT_PROBE0(cbb, , write, file_start);
		xuio.uio_rw = UIO_WRITE;
	}
	xuio.uio_offset = beio->io_offset;
	xuio.uio_resid = beio->io_len;
	xuio.uio_segflg = UIO_SYSSPACE;
	xuio.uio_iov = beio->xiovecs;
	xuio.uio_iovcnt = beio->num_segs;
	xuio.uio_td = curthread;

	for (i = 0, xiovec = xuio.uio_iov; i < xuio.uio_iovcnt; i++, xiovec++) {
		xiovec->iov_base = beio->sg_segs[i].addr;
		xiovec->iov_len = beio->sg_segs[i].len;
	}

	binuptime(&beio->ds_t0);
	devstat_start_transaction(beio->lun->disk_stats, &beio->ds_t0);

	if (beio->bio_cmd == BIO_READ) {
		vn_lock(be_lun->vn, LK_SHARED | LK_RETRY);

		if (beio->beio_cont == NULL &&
		    cbe_lun->serseq == CTL_LUN_SERSEQ_SOFT)
			ctl_serseq_done(io);
		/*
		 * UFS pays attention to IO_DIRECT for reads.  If the
		 * DIRECTIO option is configured into the kernel, it calls
		 * ffs_rawread().  But that only works for single-segment
		 * uios with user space addresses.  In our case, with a
		 * kernel uio, it still reads into the buffer cache, but it
		 * will just try to release the buffer from the cache later
		 * on in ffs_read().
		 *
		 * ZFS does not pay attention to IO_DIRECT for reads.
		 *
		 * UFS does not pay attention to IO_SYNC for reads.
		 *
		 * ZFS pays attention to IO_SYNC (which translates into the
		 * Solaris define FRSYNC for zfs_read()) for reads.  It
		 * attempts to sync the file before reading.
		 */
		error = VOP_READ(be_lun->vn, &xuio, flags, file_data->cred);

		VOP_UNLOCK(be_lun->vn);
		SDT_PROBE0(cbb, , read, file_done);
		if (error == 0 && xuio.uio_resid > 0) {
			/*
			 * If we read less then requested (EOF), then
			 * we should clean the rest of the buffer.
			 */
			s = beio->io_len - xuio.uio_resid;
			for (i = 0; i < beio->num_segs; i++) {
				if (s >= beio->sg_segs[i].len) {
					s -= beio->sg_segs[i].len;
					continue;
				}
				bzero((uint8_t *)beio->sg_segs[i].addr + s,
				    beio->sg_segs[i].len - s);
				s = 0;
			}
		}
	} else {
		struct mount *mountpoint;

		(void)vn_start_write(be_lun->vn, &mountpoint, V_WAIT);
		vn_lock(be_lun->vn, vn_lktype_write(mountpoint,
		    be_lun->vn) | LK_RETRY);

		/*
		 * UFS pays attention to IO_DIRECT for writes.  The write
		 * is done asynchronously.  (Normally the write would just
		 * get put into cache.
		 *
		 * UFS pays attention to IO_SYNC for writes.  It will
		 * attempt to write the buffer out synchronously if that
		 * flag is set.
		 *
		 * ZFS does not pay attention to IO_DIRECT for writes.
		 *
		 * ZFS pays attention to IO_SYNC (a.k.a. FSYNC or FRSYNC)
		 * for writes.  It will flush the transaction from the
		 * cache before returning.
		 */
		error = VOP_WRITE(be_lun->vn, &xuio, flags, file_data->cred);
		VOP_UNLOCK(be_lun->vn);

		vn_finished_write(mountpoint);
		SDT_PROBE0(cbb, , write, file_done);
        }

	mtx_lock(&be_lun->io_lock);
	devstat_end_transaction(beio->lun->disk_stats, beio->io_len,
	    beio->ds_tag_type, beio->ds_trans_type,
	    /*now*/ NULL, /*then*/&beio->ds_t0);
	mtx_unlock(&be_lun->io_lock);

	/*
	 * If we got an error, set the sense data to "MEDIUM ERROR" and
	 * return the I/O to the user.
	 */
	if (error != 0) {
		if (error == ENOSPC || error == EDQUOT) {
			ctl_io_set_space_alloc_fail(io);
		} else if (error == EROFS || error == EACCES) {
			ctl_io_set_hw_write_protected(io);
		} else {
			ctl_be_block_io_error(io, beio->bio_cmd, 0);
		}
		ctl_complete_beio(beio);
		return;
	}

	/*
	 * If this is a write or a verify, we're all done.
	 * If this is a read, we can now send the data to the user.
	 */
	if ((beio->bio_cmd == BIO_WRITE) ||
	    (ARGS(io)->flags & CTL_LLF_VERIFY)) {
		ctl_io_set_success(io);
		ctl_complete_beio(beio);
	} else {
		if ((ARGS(io)->flags & CTL_LLF_READ) &&
		    beio->beio_cont == NULL) {
			ctl_io_set_success(io);
			if (cbe_lun->serseq > CTL_LUN_SERSEQ_SOFT)
				ctl_serseq_done(io);
		}
		ctl_datamove(io);
	}
}

static void
ctl_be_block_gls_file(struct ctl_be_block_lun *be_lun,
			struct ctl_be_block_io *beio)
{
	union ctl_io *io = beio->io;
	struct ctl_lba_len_flags *lbalen = ARGS(io);
	struct scsi_get_lba_status_data *data;
	off_t roff, off;
	int error, status;

	DPRINTF("entered\n");

	CTL_IO_ASSERT(io, SCSI);

	off = roff = ((off_t)lbalen->lba) * be_lun->cbe_lun.blocksize;
	vn_lock(be_lun->vn, LK_SHARED | LK_RETRY);
	error = VOP_IOCTL(be_lun->vn, FIOSEEKHOLE, &off,
	    0, curthread->td_ucred, curthread);
	if (error == 0 && off > roff)
		status = 0;	/* mapped up to off */
	else {
		error = VOP_IOCTL(be_lun->vn, FIOSEEKDATA, &off,
		    0, curthread->td_ucred, curthread);
		if (error == 0 && off > roff)
			status = 1;	/* deallocated up to off */
		else {
			status = 0;	/* unknown up to the end */
			off = be_lun->size_bytes;
		}
	}
	VOP_UNLOCK(be_lun->vn);

	data = (struct scsi_get_lba_status_data *)io->scsiio.kern_data_ptr;
	scsi_u64to8b(lbalen->lba, data->descr[0].addr);
	scsi_ulto4b(MIN(UINT32_MAX, off / be_lun->cbe_lun.blocksize -
	    lbalen->lba), data->descr[0].length);
	data->descr[0].status = status;

	ctl_complete_beio(beio);
}

static uint64_t
ctl_be_block_getattr_file(struct ctl_be_block_lun *be_lun, const char *attrname)
{
	struct vattr		vattr;
	struct statfs		statfs;
	uint64_t		val;
	int			error;

	val = UINT64_MAX;
	if (be_lun->vn == NULL)
		return (val);
	vn_lock(be_lun->vn, LK_SHARED | LK_RETRY);
	if (strcmp(attrname, "blocksused") == 0) {
		error = VOP_GETATTR(be_lun->vn, &vattr, curthread->td_ucred);
		if (error == 0)
			val = vattr.va_bytes / be_lun->cbe_lun.blocksize;
	}
	if (strcmp(attrname, "blocksavail") == 0 &&
	    !VN_IS_DOOMED(be_lun->vn)) {
		error = VFS_STATFS(be_lun->vn->v_mount, &statfs);
		if (error == 0)
			val = statfs.f_bavail * statfs.f_bsize /
			    be_lun->cbe_lun.blocksize;
	}
	VOP_UNLOCK(be_lun->vn);
	return (val);
}

static void
ctl_be_block_unmap_file(struct ctl_be_block_lun *be_lun,
		        struct ctl_be_block_io *beio)
{
	struct ctl_be_block_filedata *file_data;
	union ctl_io *io;
	struct ctl_ptr_len_flags *ptrlen;
	struct scsi_unmap_desc *buf, *end;
	struct mount *mp;
	off_t off, len;
	int error;

	io = beio->io;
	file_data = &be_lun->backend.file;
	mp = NULL;
	error = 0;

	binuptime(&beio->ds_t0);
	devstat_start_transaction(be_lun->disk_stats, &beio->ds_t0);

	(void)vn_start_write(be_lun->vn, &mp, V_WAIT);
	vn_lock(be_lun->vn, vn_lktype_write(mp, be_lun->vn) | LK_RETRY);
	if (beio->io_offset == -1) {
		beio->io_len = 0;
		ptrlen = (struct ctl_ptr_len_flags *)
		    &io->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
		buf = (struct scsi_unmap_desc *)ptrlen->ptr;
		end = buf + ptrlen->len / sizeof(*buf);
		for (; buf < end; buf++) {
			off = (off_t)scsi_8btou64(buf->lba) *
			    be_lun->cbe_lun.blocksize;
			len = (off_t)scsi_4btoul(buf->length) *
			    be_lun->cbe_lun.blocksize;
			beio->io_len += len;
			error = vn_deallocate(be_lun->vn, &off, &len,
			    0, IO_NOMACCHECK | IO_NODELOCKED, file_data->cred,
			    NOCRED);
			if (error != 0)
				break;
		}
	} else {
		/* WRITE_SAME */
		off = beio->io_offset;
		len = beio->io_len;
		error = vn_deallocate(be_lun->vn, &off, &len, 0,
		    IO_NOMACCHECK | IO_NODELOCKED, file_data->cred, NOCRED);
	}
	VOP_UNLOCK(be_lun->vn);
	vn_finished_write(mp);

	mtx_lock(&be_lun->io_lock);
	devstat_end_transaction(beio->lun->disk_stats, beio->io_len,
	    beio->ds_tag_type, beio->ds_trans_type,
	    /*now*/ NULL, /*then*/&beio->ds_t0);
	mtx_unlock(&be_lun->io_lock);

	/*
	 * If we got an error, set the sense data to "MEDIUM ERROR" and
	 * return the I/O to the user.
	 */
	switch (error) {
	case 0:
		ctl_io_set_success(io);
		break;
	case ENOSPC:
	case EDQUOT:
		ctl_io_set_space_alloc_fail(io);
		break;
	case EROFS:
	case EACCES:
		ctl_io_set_hw_write_protected(io);
		break;
	default:
		ctl_be_block_io_error(io, BIO_DELETE, 0);
	}
	ctl_complete_beio(beio);
}

static void
ctl_be_block_dispatch_zvol(struct ctl_be_block_lun *be_lun,
			   struct ctl_be_block_io *beio)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	union ctl_io *io;
	struct cdevsw *csw;
	struct cdev *dev;
	struct uio xuio;
	struct iovec *xiovec;
	int error, flags, i, ref;

	DPRINTF("entered\n");

	io = beio->io;
	flags = 0;
	if (ARGS(io)->flags & CTL_LLF_DPO)
		flags |= IO_DIRECT;
	if (beio->bio_cmd == BIO_WRITE && ARGS(io)->flags & CTL_LLF_FUA)
		flags |= IO_SYNC;

	bzero(&xuio, sizeof(xuio));
	if (beio->bio_cmd == BIO_READ) {
		SDT_PROBE0(cbb, , read, file_start);
		xuio.uio_rw = UIO_READ;
	} else {
		SDT_PROBE0(cbb, , write, file_start);
		xuio.uio_rw = UIO_WRITE;
	}
	xuio.uio_offset = beio->io_offset;
	xuio.uio_resid = beio->io_len;
	xuio.uio_segflg = UIO_SYSSPACE;
	xuio.uio_iov = beio->xiovecs;
	xuio.uio_iovcnt = beio->num_segs;
	xuio.uio_td = curthread;

	for (i = 0, xiovec = xuio.uio_iov; i < xuio.uio_iovcnt; i++, xiovec++) {
		xiovec->iov_base = beio->sg_segs[i].addr;
		xiovec->iov_len = beio->sg_segs[i].len;
	}

	binuptime(&beio->ds_t0);
	devstat_start_transaction(beio->lun->disk_stats, &beio->ds_t0);

	csw = devvn_refthread(be_lun->vn, &dev, &ref);
	if (csw) {
		if (beio->bio_cmd == BIO_READ) {
			if (beio->beio_cont == NULL &&
			    cbe_lun->serseq == CTL_LUN_SERSEQ_SOFT)
				ctl_serseq_done(io);
			error = csw->d_read(dev, &xuio, flags);
		} else
			error = csw->d_write(dev, &xuio, flags);
		dev_relthread(dev, ref);
	} else
		error = ENXIO;

	if (beio->bio_cmd == BIO_READ)
		SDT_PROBE0(cbb, , read, file_done);
	else
		SDT_PROBE0(cbb, , write, file_done);

	mtx_lock(&be_lun->io_lock);
	devstat_end_transaction(beio->lun->disk_stats, beio->io_len,
	    beio->ds_tag_type, beio->ds_trans_type,
	    /*now*/ NULL, /*then*/&beio->ds_t0);
	mtx_unlock(&be_lun->io_lock);

	/*
	 * If we got an error, set the sense data to "MEDIUM ERROR" and
	 * return the I/O to the user.
	 */
	if (error != 0) {
		if (error == ENOSPC || error == EDQUOT) {
			ctl_io_set_space_alloc_fail(io);
		} else if (error == EROFS || error == EACCES) {
			ctl_io_set_hw_write_protected(io);
		} else {
			ctl_be_block_io_error(io, beio->bio_cmd, 0);
		}
		ctl_complete_beio(beio);
		return;
	}

	/*
	 * If this is a write or a verify, we're all done.
	 * If this is a read, we can now send the data to the user.
	 */
	if ((beio->bio_cmd == BIO_WRITE) ||
	    (ARGS(io)->flags & CTL_LLF_VERIFY)) {
		ctl_io_set_success(io);
		ctl_complete_beio(beio);
	} else {
		if ((ARGS(io)->flags & CTL_LLF_READ) &&
		    beio->beio_cont == NULL) {
			ctl_io_set_success(io);
			if (cbe_lun->serseq > CTL_LUN_SERSEQ_SOFT)
				ctl_serseq_done(io);
		}
		ctl_datamove(io);
	}
}

static void
ctl_be_block_gls_zvol(struct ctl_be_block_lun *be_lun,
			struct ctl_be_block_io *beio)
{
	union ctl_io *io = beio->io;
	struct cdevsw *csw;
	struct cdev *dev;
	struct ctl_lba_len_flags *lbalen = ARGS(io);
	struct scsi_get_lba_status_data *data;
	off_t roff, off;
	int error, ref, status;

	DPRINTF("entered\n");

	CTL_IO_ASSERT(io, SCSI);

	csw = devvn_refthread(be_lun->vn, &dev, &ref);
	if (csw == NULL) {
		status = 0;	/* unknown up to the end */
		off = be_lun->size_bytes;
		goto done;
	}
	off = roff = ((off_t)lbalen->lba) * be_lun->cbe_lun.blocksize;
	error = csw->d_ioctl(dev, FIOSEEKHOLE, (caddr_t)&off, FREAD,
	    curthread);
	if (error == 0 && off > roff)
		status = 0;	/* mapped up to off */
	else {
		error = csw->d_ioctl(dev, FIOSEEKDATA, (caddr_t)&off, FREAD,
		    curthread);
		if (error == 0 && off > roff)
			status = 1;	/* deallocated up to off */
		else {
			status = 0;	/* unknown up to the end */
			off = be_lun->size_bytes;
		}
	}
	dev_relthread(dev, ref);

done:
	data = (struct scsi_get_lba_status_data *)io->scsiio.kern_data_ptr;
	scsi_u64to8b(lbalen->lba, data->descr[0].addr);
	scsi_ulto4b(MIN(UINT32_MAX, off / be_lun->cbe_lun.blocksize -
	    lbalen->lba), data->descr[0].length);
	data->descr[0].status = status;

	ctl_complete_beio(beio);
}

static void
ctl_be_block_flush_dev(struct ctl_be_block_lun *be_lun,
		       struct ctl_be_block_io *beio)
{
	struct bio *bio;
	struct cdevsw *csw;
	struct cdev *dev;
	int ref;

	DPRINTF("entered\n");

	/* This can't fail, it's a blocking allocation. */
	bio = g_alloc_bio();

	bio->bio_cmd	    = BIO_FLUSH;
	bio->bio_offset	    = 0;
	bio->bio_data	    = 0;
	bio->bio_done	    = ctl_be_block_biodone;
	bio->bio_caller1    = beio;
	bio->bio_pblkno	    = 0;

	/*
	 * We don't need to acquire the LUN lock here, because we are only
	 * sending one bio, and so there is no other context to synchronize
	 * with.
	 */
	beio->num_bios_sent = 1;
	beio->send_complete = 1;

	binuptime(&beio->ds_t0);
	devstat_start_transaction(be_lun->disk_stats, &beio->ds_t0);

	csw = devvn_refthread(be_lun->vn, &dev, &ref);
	if (csw) {
		bio->bio_dev = dev;
		csw->d_strategy(bio);
		dev_relthread(dev, ref);
	} else {
		bio->bio_error = ENXIO;
		ctl_be_block_biodone(bio);
	}
}

static void
ctl_be_block_unmap_dev_range(struct ctl_be_block_lun *be_lun,
		       struct ctl_be_block_io *beio,
		       uint64_t off, uint64_t len, int last)
{
	struct bio *bio;
	uint64_t maxlen;
	struct cdevsw *csw;
	struct cdev *dev;
	int ref;

	csw = devvn_refthread(be_lun->vn, &dev, &ref);
	maxlen = LONG_MAX - (LONG_MAX % be_lun->cbe_lun.blocksize);
	while (len > 0) {
		bio = g_alloc_bio();
		bio->bio_cmd	    = BIO_DELETE;
		bio->bio_dev	    = dev;
		bio->bio_offset	    = off;
		bio->bio_length	    = MIN(len, maxlen);
		bio->bio_data	    = 0;
		bio->bio_done	    = ctl_be_block_biodone;
		bio->bio_caller1    = beio;
		bio->bio_pblkno     = off / be_lun->cbe_lun.blocksize;

		off += bio->bio_length;
		len -= bio->bio_length;

		mtx_lock(&be_lun->io_lock);
		beio->num_bios_sent++;
		if (last && len == 0)
			beio->send_complete = 1;
		mtx_unlock(&be_lun->io_lock);

		if (csw) {
			csw->d_strategy(bio);
		} else {
			bio->bio_error = ENXIO;
			ctl_be_block_biodone(bio);
		}
	}
	if (csw)
		dev_relthread(dev, ref);
}

static void
ctl_be_block_unmap_dev(struct ctl_be_block_lun *be_lun,
		       struct ctl_be_block_io *beio)
{
	union ctl_io *io;
	struct ctl_ptr_len_flags *ptrlen;
	struct scsi_unmap_desc *buf, *end;
	uint64_t len;

	io = beio->io;

	DPRINTF("entered\n");

	binuptime(&beio->ds_t0);
	devstat_start_transaction(be_lun->disk_stats, &beio->ds_t0);

	if (beio->io_offset == -1) {
		beio->io_len = 0;
		ptrlen = (struct ctl_ptr_len_flags *)&io->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];
		buf = (struct scsi_unmap_desc *)ptrlen->ptr;
		end = buf + ptrlen->len / sizeof(*buf);
		for (; buf < end; buf++) {
			len = (uint64_t)scsi_4btoul(buf->length) *
			    be_lun->cbe_lun.blocksize;
			beio->io_len += len;
			ctl_be_block_unmap_dev_range(be_lun, beio,
			    scsi_8btou64(buf->lba) * be_lun->cbe_lun.blocksize,
			    len, (end - buf < 2) ? TRUE : FALSE);
		}
	} else
		ctl_be_block_unmap_dev_range(be_lun, beio,
		    beio->io_offset, beio->io_len, TRUE);
}

static void
ctl_be_block_dispatch_dev(struct ctl_be_block_lun *be_lun,
			  struct ctl_be_block_io *beio)
{
	TAILQ_HEAD(, bio) queue = TAILQ_HEAD_INITIALIZER(queue);
	struct bio *bio;
	struct cdevsw *csw;
	struct cdev *dev;
	off_t cur_offset;
	int i, max_iosize, ref;

	DPRINTF("entered\n");
	csw = devvn_refthread(be_lun->vn, &dev, &ref);

	/*
	 * We have to limit our I/O size to the maximum supported by the
	 * backend device.
	 */
	if (csw) {
		max_iosize = dev->si_iosize_max;
		if (max_iosize <= 0)
			max_iosize = DFLTPHYS;
	} else
		max_iosize = maxphys;

	cur_offset = beio->io_offset;
	for (i = 0; i < beio->num_segs; i++) {
		size_t cur_size;
		uint8_t *cur_ptr;

		cur_size = beio->sg_segs[i].len;
		cur_ptr = beio->sg_segs[i].addr;

		while (cur_size > 0) {
			/* This can't fail, it's a blocking allocation. */
			bio = g_alloc_bio();

			KASSERT(bio != NULL, ("g_alloc_bio() failed!\n"));

			bio->bio_cmd = beio->bio_cmd;
			bio->bio_dev = dev;
			bio->bio_caller1 = beio;
			bio->bio_length = min(cur_size, max_iosize);
			bio->bio_offset = cur_offset;
			bio->bio_data = cur_ptr;
			bio->bio_done = ctl_be_block_biodone;
			bio->bio_pblkno = cur_offset / be_lun->cbe_lun.blocksize;

			cur_offset += bio->bio_length;
			cur_ptr += bio->bio_length;
			cur_size -= bio->bio_length;

			TAILQ_INSERT_TAIL(&queue, bio, bio_queue);
			beio->num_bios_sent++;
		}
	}
	beio->send_complete = 1;
	binuptime(&beio->ds_t0);
	devstat_start_transaction(be_lun->disk_stats, &beio->ds_t0);

	/*
	 * Fire off all allocated requests!
	 */
	while ((bio = TAILQ_FIRST(&queue)) != NULL) {
		TAILQ_REMOVE(&queue, bio, bio_queue);
		if (csw)
			csw->d_strategy(bio);
		else {
			bio->bio_error = ENXIO;
			ctl_be_block_biodone(bio);
		}
	}
	if (csw)
		dev_relthread(dev, ref);
}

static uint64_t
ctl_be_block_getattr_dev(struct ctl_be_block_lun *be_lun, const char *attrname)
{
	struct diocgattr_arg	arg;
	struct cdevsw *csw;
	struct cdev *dev;
	int error, ref;

	csw = devvn_refthread(be_lun->vn, &dev, &ref);
	if (csw == NULL)
		return (UINT64_MAX);
	strlcpy(arg.name, attrname, sizeof(arg.name));
	arg.len = sizeof(arg.value.off);
	if (csw->d_ioctl) {
		error = csw->d_ioctl(dev, DIOCGATTR, (caddr_t)&arg, FREAD,
		    curthread);
	} else
		error = ENODEV;
	dev_relthread(dev, ref);
	if (error != 0)
		return (UINT64_MAX);
	return (arg.value.off);
}

static void
ctl_be_block_namespace_data(struct ctl_be_block_lun *be_lun,
			    union ctl_io *io)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct nvme_namespace_data *nsdata;

	nsdata = (struct nvme_namespace_data *)io->nvmeio.kern_data_ptr;
	memset(nsdata, 0, sizeof(*nsdata));
	nsdata->nsze = htole64(be_lun->size_blocks);
	nsdata->ncap = nsdata->nsze;
	nsdata->nuse = nsdata->nsze;
	nsdata->nlbaf = 1 - 1;
	nsdata->dlfeat = NVMEM(NVME_NS_DATA_DLFEAT_DWZ) |
	    NVMEF(NVME_NS_DATA_DLFEAT_READ, NVME_NS_DATA_DLFEAT_READ_00);
	nsdata->flbas = NVMEF(NVME_NS_DATA_FLBAS_FORMAT, 0);
	nsdata->lbaf[0] = NVMEF(NVME_NS_DATA_LBAF_LBADS,
	    ffs(cbe_lun->blocksize) - 1);

	ctl_lun_nsdata_ids(cbe_lun, nsdata);
	ctl_config_read_done(io);
}

static void
ctl_be_block_nvme_ids(struct ctl_be_block_lun *be_lun,
		      union ctl_io *io)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;

	ctl_lun_nvme_ids(cbe_lun, io->nvmeio.kern_data_ptr);
	ctl_config_read_done(io);
}

static void
ctl_be_block_cw_dispatch_sync(struct ctl_be_block_lun *be_lun,
			    union ctl_io *io)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct ctl_be_block_io *beio;
	struct ctl_lba_len_flags *lbalen;

	DPRINTF("entered\n");
	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;
	lbalen = (struct ctl_lba_len_flags *)&io->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];

	beio->io_len = lbalen->len * cbe_lun->blocksize;
	beio->io_offset = lbalen->lba * cbe_lun->blocksize;
	beio->io_arg = (lbalen->flags & SSC_IMMED) != 0;
	beio->bio_cmd = BIO_FLUSH;
	beio->ds_trans_type = DEVSTAT_NO_DATA;
	DPRINTF("SYNC\n");
	be_lun->lun_flush(be_lun, beio);
}

static void
ctl_be_block_cw_done_ws(struct ctl_be_block_io *beio)
{
	union ctl_io *io;

	io = beio->io;
	ctl_free_beio(beio);
	if ((io->io_hdr.flags & CTL_FLAG_ABORT) ||
	    ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE &&
	     (io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)) {
		ctl_config_write_done(io);
		return;
	}

	ctl_be_block_config_write(io);
}

static void
ctl_be_block_cw_dispatch_ws(struct ctl_be_block_lun *be_lun,
			    union ctl_io *io)
{
	struct ctl_be_block_softc *softc = be_lun->softc;
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct ctl_be_block_io *beio;
	struct ctl_lba_len_flags *lbalen;
	uint64_t len_left, lba;
	uint32_t pb, pbo, adj;
	int i, seglen;
	uint8_t *buf, *end;

	DPRINTF("entered\n");

	CTL_IO_ASSERT(io, SCSI);

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;
	lbalen = ARGS(io);

	if (lbalen->flags & ~(SWS_LBDATA | SWS_UNMAP | SWS_ANCHOR | SWS_NDOB) ||
	    (lbalen->flags & (SWS_UNMAP | SWS_ANCHOR) && be_lun->unmap == NULL)) {
		ctl_free_beio(beio);
		ctl_set_invalid_field(&io->scsiio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 1,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_config_write_done(io);
		return;
	}

	if (lbalen->flags & (SWS_UNMAP | SWS_ANCHOR)) {
		beio->io_offset = lbalen->lba * cbe_lun->blocksize;
		beio->io_len = (uint64_t)lbalen->len * cbe_lun->blocksize;
		beio->bio_cmd = BIO_DELETE;
		beio->ds_trans_type = DEVSTAT_FREE;

		be_lun->unmap(be_lun, beio);
		return;
	}

	beio->bio_cmd = BIO_WRITE;
	beio->ds_trans_type = DEVSTAT_WRITE;

	DPRINTF("WRITE SAME at LBA %jx len %u\n",
	       (uintmax_t)lbalen->lba, lbalen->len);

	pb = cbe_lun->blocksize << be_lun->cbe_lun.pblockexp;
	if (be_lun->cbe_lun.pblockoff > 0)
		pbo = pb - cbe_lun->blocksize * be_lun->cbe_lun.pblockoff;
	else
		pbo = 0;
	len_left = (uint64_t)lbalen->len * cbe_lun->blocksize;
	for (i = 0, lba = 0; i < CTLBLK_MAX_SEGS && len_left > 0; i++) {
		/*
		 * Setup the S/G entry for this chunk.
		 */
		seglen = MIN(CTLBLK_MAX_SEG, len_left);
		if (pb > cbe_lun->blocksize) {
			adj = ((lbalen->lba + lba) * cbe_lun->blocksize +
			    seglen - pbo) % pb;
			if (seglen > adj)
				seglen -= adj;
			else
				seglen -= seglen % cbe_lun->blocksize;
		} else
			seglen -= seglen % cbe_lun->blocksize;
		ctl_alloc_seg(softc, &beio->sg_segs[i], seglen);

		DPRINTF("segment %d addr %p len %zd\n", i,
			beio->sg_segs[i].addr, beio->sg_segs[i].len);

		beio->num_segs++;
		len_left -= seglen;

		buf = beio->sg_segs[i].addr;
		end = buf + seglen;
		for (; buf < end; buf += cbe_lun->blocksize) {
			if (lbalen->flags & SWS_NDOB) {
				memset(buf, 0, cbe_lun->blocksize);
			} else {
				memcpy(buf, io->scsiio.kern_data_ptr,
				    cbe_lun->blocksize);
			}
			if (lbalen->flags & SWS_LBDATA)
				scsi_ulto4b(lbalen->lba + lba, buf);
			lba++;
		}
	}

	beio->io_offset = lbalen->lba * cbe_lun->blocksize;
	beio->io_len = lba * cbe_lun->blocksize;

	/* We can not do all in one run. Correct and schedule rerun. */
	if (len_left > 0) {
		lbalen->lba += lba;
		lbalen->len -= lba;
		beio->beio_cont = ctl_be_block_cw_done_ws;
	}

	be_lun->dispatch(be_lun, beio);
}

static void
ctl_be_block_cw_dispatch_unmap(struct ctl_be_block_lun *be_lun,
			       union ctl_io *io)
{
	struct ctl_be_block_io *beio;
	struct ctl_ptr_len_flags *ptrlen;

	DPRINTF("entered\n");

	CTL_IO_ASSERT(io, SCSI);

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;
	ptrlen = (struct ctl_ptr_len_flags *)&io->io_hdr.ctl_private[CTL_PRIV_LBA_LEN];

	if ((ptrlen->flags & ~SU_ANCHOR) != 0 || be_lun->unmap == NULL) {
		ctl_free_beio(beio);
		ctl_set_invalid_field(&io->scsiio,
				      /*sks_valid*/ 0,
				      /*command*/ 1,
				      /*field*/ 0,
				      /*bit_valid*/ 0,
				      /*bit*/ 0);
		ctl_config_write_done(io);
		return;
	}

	beio->io_len = 0;
	beio->io_offset = -1;
	beio->bio_cmd = BIO_DELETE;
	beio->ds_trans_type = DEVSTAT_FREE;
	DPRINTF("UNMAP\n");
	be_lun->unmap(be_lun, beio);
}

static void
ctl_be_block_cw_dispatch_flush(struct ctl_be_block_lun *be_lun,
			       union ctl_io *io)
{
	struct ctl_be_block_io *beio;

	DPRINTF("entered\n");
	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;

	beio->io_len = be_lun->size_bytes;
	beio->io_offset = 0;
	beio->io_arg = 1;
	beio->bio_cmd = BIO_FLUSH;
	beio->ds_trans_type = DEVSTAT_NO_DATA;
	DPRINTF("FLUSH\n");
	be_lun->lun_flush(be_lun, beio);
}

static void
ctl_be_block_cw_dispatch_wu(struct ctl_be_block_lun *be_lun,
			    union ctl_io *io)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct ctl_be_block_io *beio;
	struct ctl_lba_len_flags *lbalen;

	CTL_IO_ASSERT(io, NVME);

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;
	lbalen = ARGS(io);

	/*
	 * XXX: Not quite right as reads will return zeroes rather
	 * than failing.
	 */
	beio->io_offset = lbalen->lba * cbe_lun->blocksize;
	beio->io_len = (uint64_t)lbalen->len * cbe_lun->blocksize;
	beio->bio_cmd = BIO_DELETE;
	beio->ds_trans_type = DEVSTAT_FREE;

	be_lun->unmap(be_lun, beio);
}

static void
ctl_be_block_cw_dispatch_wz(struct ctl_be_block_lun *be_lun,
			    union ctl_io *io)
{
	struct ctl_be_block_softc *softc = be_lun->softc;
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct ctl_be_block_io *beio;
	struct ctl_lba_len_flags *lbalen;
	uint64_t len_left, lba;
	uint32_t pb, pbo, adj;
	int i, seglen;

	DPRINTF("entered\n");

	CTL_IO_ASSERT(io, NVME);

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;
	lbalen = ARGS(io);

	if ((le32toh(io->nvmeio.cmd.cdw12) & (1U << 25)) != 0 &&
	    be_lun->unmap != NULL) {
		beio->io_offset = lbalen->lba * cbe_lun->blocksize;
		beio->io_len = (uint64_t)lbalen->len * cbe_lun->blocksize;
		beio->bio_cmd = BIO_DELETE;
		beio->ds_trans_type = DEVSTAT_FREE;

		be_lun->unmap(be_lun, beio);
		return;
	}

	beio->bio_cmd = BIO_WRITE;
	beio->ds_trans_type = DEVSTAT_WRITE;

	DPRINTF("WRITE ZEROES at LBA %jx len %u\n",
	       (uintmax_t)lbalen->lba, lbalen->len);

	pb = cbe_lun->blocksize << be_lun->cbe_lun.pblockexp;
	if (be_lun->cbe_lun.pblockoff > 0)
		pbo = pb - cbe_lun->blocksize * be_lun->cbe_lun.pblockoff;
	else
		pbo = 0;
	len_left = (uint64_t)lbalen->len * cbe_lun->blocksize;
	for (i = 0, lba = 0; i < CTLBLK_MAX_SEGS && len_left > 0; i++) {
		/*
		 * Setup the S/G entry for this chunk.
		 */
		seglen = MIN(CTLBLK_MAX_SEG, len_left);
		if (pb > cbe_lun->blocksize) {
			adj = ((lbalen->lba + lba) * cbe_lun->blocksize +
			    seglen - pbo) % pb;
			if (seglen > adj)
				seglen -= adj;
			else
				seglen -= seglen % cbe_lun->blocksize;
		} else
			seglen -= seglen % cbe_lun->blocksize;
		ctl_alloc_seg(softc, &beio->sg_segs[i], seglen);

		DPRINTF("segment %d addr %p len %zd\n", i,
			beio->sg_segs[i].addr, beio->sg_segs[i].len);

		beio->num_segs++;
		len_left -= seglen;

		memset(beio->sg_segs[i].addr, 0, seglen);
		lba += seglen / cbe_lun->blocksize;
	}

	beio->io_offset = lbalen->lba * cbe_lun->blocksize;
	beio->io_len = lba * cbe_lun->blocksize;

	/* We can not do all in one run. Correct and schedule rerun. */
	if (len_left > 0) {
		lbalen->lba += lba;
		lbalen->len -= lba;
		beio->beio_cont = ctl_be_block_cw_done_ws;
	}

	be_lun->dispatch(be_lun, beio);
}

static void
ctl_be_block_cw_dispatch_dsm(struct ctl_be_block_lun *be_lun,
			     union ctl_io *io)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct ctl_be_block_io *beio;
	struct nvme_dsm_range *r;
	uint64_t lba;
	uint32_t num_blocks;
	u_int i, ranges;

	CTL_IO_ASSERT(io, NVME);

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;

	if (be_lun->unmap == NULL) {
		ctl_free_beio(beio);
		ctl_nvme_set_success(&io->nvmeio);
		ctl_config_write_done(io);
		return;
	}

	ranges = le32toh(io->nvmeio.cmd.cdw10) & 0xff;
	r = (struct nvme_dsm_range *)io->nvmeio.kern_data_ptr;

	/* Find the next range to delete. */
	for (i = DSM_RANGE(io); i < ranges; i++) {
		if ((le32toh(r[i].attributes) & (1U << 2)) != 0)
			break;
	}

	/* If no range to delete, complete the operation. */
	if (i == ranges) {
		ctl_free_beio(beio);
		ctl_nvme_set_success(&io->nvmeio);
		ctl_config_write_done(io);
		return;
	}

	/* If this is not the last range, request a rerun after this range. */
	if (i + 1 < ranges) {
		DSM_RANGE(io) = i + 1;
		beio->beio_cont = ctl_be_block_cw_done_ws;
	}

	lba = le64toh(r[i].starting_lba);
	num_blocks = le32toh(r[i].length);

	beio->io_offset = lba * cbe_lun->blocksize;
	beio->io_len = (uint64_t)num_blocks * cbe_lun->blocksize;
	beio->bio_cmd = BIO_DELETE;
	beio->ds_trans_type = DEVSTAT_FREE;

	be_lun->unmap(be_lun, beio);
}

static void
ctl_be_block_scsi_cr_done(struct ctl_be_block_io *beio)
{
	union ctl_io *io;

	io = beio->io;
	ctl_free_beio(beio);
	ctl_config_read_done(io);
}

static void
ctl_be_block_scsi_cr_dispatch(struct ctl_be_block_lun *be_lun,
			      union ctl_io *io)
{
	struct ctl_be_block_io *beio;
	struct ctl_be_block_softc *softc;

	DPRINTF("entered\n");

	softc = be_lun->softc;
	beio = ctl_alloc_beio(softc);
	beio->io = io;
	beio->lun = be_lun;
	beio->beio_cont = ctl_be_block_scsi_cr_done;
	PRIV(io)->ptr = (void *)beio;

	switch (io->scsiio.cdb[0]) {
	case SERVICE_ACTION_IN:		/* GET LBA STATUS */
		beio->bio_cmd = -1;
		beio->ds_trans_type = DEVSTAT_NO_DATA;
		beio->ds_tag_type = DEVSTAT_TAG_ORDERED;
		beio->io_len = 0;
		if (be_lun->get_lba_status)
			be_lun->get_lba_status(be_lun, beio);
		else
			ctl_be_block_scsi_cr_done(beio);
		break;
	default:
		panic("Unhandled CDB type %#x", io->scsiio.cdb[0]);
		break;
	}
}

static void
ctl_be_block_nvme_cr_dispatch(struct ctl_be_block_lun *be_lun,
			      union ctl_io *io)
{
	uint8_t cns;

	DPRINTF("entered\n");

	MPASS(io->nvmeio.cmd.opc == NVME_OPC_IDENTIFY);

	cns = le32toh(io->nvmeio.cmd.cdw10) & 0xff;
	switch (cns) {
	case 0:
		ctl_be_block_namespace_data(be_lun, io);
		break;
	case 3:
		ctl_be_block_nvme_ids(be_lun, io);
		break;
	default:
		__assert_unreachable();
	}
}

static void
ctl_be_block_cr_dispatch(struct ctl_be_block_lun *be_lun,
			 union ctl_io *io)
{
	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		ctl_be_block_scsi_cr_dispatch(be_lun, io);
		break;
	case CTL_IO_NVME_ADMIN:
		ctl_be_block_nvme_cr_dispatch(be_lun, io);
		break;
	default:
		__assert_unreachable();
	}
}

static void
ctl_be_block_cw_done(struct ctl_be_block_io *beio)
{
	union ctl_io *io;

	io = beio->io;
	ctl_free_beio(beio);
	ctl_config_write_done(io);
}

static void
ctl_be_block_scsi_cw_dispatch(struct ctl_be_block_lun *be_lun,
			      union ctl_io *io)
{
	struct ctl_be_block_io *beio;

	DPRINTF("entered\n");

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;

	switch (io->scsiio.tag_type) {
	case CTL_TAG_ORDERED:
		beio->ds_tag_type = DEVSTAT_TAG_ORDERED;
		break;
	case CTL_TAG_HEAD_OF_QUEUE:
		beio->ds_tag_type = DEVSTAT_TAG_HEAD;
		break;
	case CTL_TAG_UNTAGGED:
	case CTL_TAG_SIMPLE:
	case CTL_TAG_ACA:
	default:
		beio->ds_tag_type = DEVSTAT_TAG_SIMPLE;
		break;
	}

	switch (io->scsiio.cdb[0]) {
	case SYNCHRONIZE_CACHE:
	case SYNCHRONIZE_CACHE_16:
		ctl_be_block_cw_dispatch_sync(be_lun, io);
		break;
	case WRITE_SAME_10:
	case WRITE_SAME_16:
		ctl_be_block_cw_dispatch_ws(be_lun, io);
		break;
	case UNMAP:
		ctl_be_block_cw_dispatch_unmap(be_lun, io);
		break;
	default:
		panic("Unhandled CDB type %#x", io->scsiio.cdb[0]);
		break;
	}
}

static void
ctl_be_block_nvme_cw_dispatch(struct ctl_be_block_lun *be_lun,
			      union ctl_io *io)
{
	struct ctl_be_block_io *beio;

	DPRINTF("entered\n");

	beio = (struct ctl_be_block_io *)PRIV(io)->ptr;
	beio->ds_tag_type = DEVSTAT_TAG_SIMPLE;

	switch (io->nvmeio.cmd.opc) {
	case NVME_OPC_FLUSH:
		ctl_be_block_cw_dispatch_flush(be_lun, io);
		break;
	case NVME_OPC_WRITE_UNCORRECTABLE:
		ctl_be_block_cw_dispatch_wu(be_lun, io);
		break;
	case NVME_OPC_WRITE_ZEROES:
		ctl_be_block_cw_dispatch_wz(be_lun, io);
		break;
	case NVME_OPC_DATASET_MANAGEMENT:
		ctl_be_block_cw_dispatch_dsm(be_lun, io);
		break;
	default:
		__assert_unreachable();
	}
}

static void
ctl_be_block_cw_dispatch(struct ctl_be_block_lun *be_lun,
			 union ctl_io *io)
{
	struct ctl_be_block_io *beio;
	struct ctl_be_block_softc *softc;

	softc = be_lun->softc;
	beio = ctl_alloc_beio(softc);
	beio->io = io;
	beio->lun = be_lun;
	beio->beio_cont = ctl_be_block_cw_done;
	PRIV(io)->ptr = (void *)beio;

	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		ctl_be_block_scsi_cw_dispatch(be_lun, io);
		break;
	case CTL_IO_NVME:
		ctl_be_block_nvme_cw_dispatch(be_lun, io);
		break;
	default:
		__assert_unreachable();
	}
}

SDT_PROBE_DEFINE1(cbb, , read, start, "uint64_t");
SDT_PROBE_DEFINE1(cbb, , write, start, "uint64_t");
SDT_PROBE_DEFINE1(cbb, , read, alloc_done, "uint64_t");
SDT_PROBE_DEFINE1(cbb, , write, alloc_done, "uint64_t");

static void
ctl_be_block_next(struct ctl_be_block_io *beio)
{
	struct ctl_be_block_lun *be_lun;
	union ctl_io *io;

	io = beio->io;
	be_lun = beio->lun;
	ctl_free_beio(beio);
	if ((io->io_hdr.flags & CTL_FLAG_ABORT) ||
	    ((io->io_hdr.status & CTL_STATUS_MASK) != CTL_STATUS_NONE &&
	     (io->io_hdr.status & CTL_STATUS_MASK) != CTL_SUCCESS)) {
		ctl_data_submit_done(io);
		return;
	}

	io->io_hdr.status &= ~CTL_STATUS_MASK;
	io->io_hdr.status |= CTL_STATUS_NONE;

	mtx_lock(&be_lun->queue_lock);
	STAILQ_INSERT_TAIL(&be_lun->input_queue, &io->io_hdr, links);
	mtx_unlock(&be_lun->queue_lock);
	taskqueue_enqueue(be_lun->io_taskqueue, &be_lun->io_task);
}

static void
ctl_be_block_dispatch(struct ctl_be_block_lun *be_lun,
			   union ctl_io *io)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct ctl_be_block_io *beio;
	struct ctl_be_block_softc *softc;
	struct ctl_lba_len_flags *lbalen;
	struct ctl_ptr_len_flags *bptrlen;
	uint64_t len_left, lbas;
	int i;

	softc = be_lun->softc;

	DPRINTF("entered\n");

	lbalen = ARGS(io);
	if (lbalen->flags & CTL_LLF_WRITE) {
		SDT_PROBE0(cbb, , write, start);
	} else {
		SDT_PROBE0(cbb, , read, start);
	}

	beio = ctl_alloc_beio(softc);
	beio->io = io;
	beio->lun = be_lun;
	bptrlen = PRIV(io);
	bptrlen->ptr = (void *)beio;

	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		switch (io->scsiio.tag_type) {
		case CTL_TAG_ORDERED:
			beio->ds_tag_type = DEVSTAT_TAG_ORDERED;
			break;
		case CTL_TAG_HEAD_OF_QUEUE:
			beio->ds_tag_type = DEVSTAT_TAG_HEAD;
			break;
		case CTL_TAG_UNTAGGED:
		case CTL_TAG_SIMPLE:
		case CTL_TAG_ACA:
		default:
			beio->ds_tag_type = DEVSTAT_TAG_SIMPLE;
			break;
		}
		break;
	case CTL_IO_NVME:
		beio->ds_tag_type = DEVSTAT_TAG_SIMPLE;
		break;
	default:
		__assert_unreachable();
	}

	if (lbalen->flags & CTL_LLF_WRITE) {
		beio->bio_cmd = BIO_WRITE;
		beio->ds_trans_type = DEVSTAT_WRITE;
	} else {
		beio->bio_cmd = BIO_READ;
		beio->ds_trans_type = DEVSTAT_READ;
	}

	DPRINTF("%s at LBA %jx len %u @%ju\n",
	       (beio->bio_cmd == BIO_READ) ? "READ" : "WRITE",
	       (uintmax_t)lbalen->lba, lbalen->len, bptrlen->len);
	lbas = CTLBLK_MAX_IO_SIZE;
	if (lbalen->flags & CTL_LLF_COMPARE) {
		beio->two_sglists = 1;
		lbas /= 2;
	}
	lbas = MIN(lbalen->len - bptrlen->len, lbas / cbe_lun->blocksize);
	beio->io_offset = (lbalen->lba + bptrlen->len) * cbe_lun->blocksize;
	beio->io_len = lbas * cbe_lun->blocksize;
	bptrlen->len += lbas;

	for (i = 0, len_left = beio->io_len; len_left > 0; i++) {
		KASSERT(i < CTLBLK_MAX_SEGS, ("Too many segs (%d >= %d)",
		    i, CTLBLK_MAX_SEGS));

		/*
		 * Setup the S/G entry for this chunk.
		 */
		ctl_alloc_seg(softc, &beio->sg_segs[i],
		    MIN(CTLBLK_MAX_SEG, len_left));

		DPRINTF("segment %d addr %p len %zd\n", i,
			beio->sg_segs[i].addr, beio->sg_segs[i].len);

		/* Set up second segment for compare operation. */
		if (beio->two_sglists) {
			ctl_alloc_seg(softc,
			    &beio->sg_segs[i + CTLBLK_HALF_SEGS],
			    beio->sg_segs[i].len);
		}

		beio->num_segs++;
		len_left -= beio->sg_segs[i].len;
	}
	if (bptrlen->len < lbalen->len)
		beio->beio_cont = ctl_be_block_next;
	ctl_set_be_move_done(io, ctl_be_block_move_done);
	/* For compare we have separate S/G lists for read and datamove. */
	if (beio->two_sglists)
		ctl_set_kern_data_ptr(io, &beio->sg_segs[CTLBLK_HALF_SEGS]);
	else
		ctl_set_kern_data_ptr(io, beio->sg_segs);
	ctl_set_kern_data_len(io, beio->io_len);
	ctl_set_kern_sg_entries(io, beio->num_segs);
	ctl_set_kern_data_ref(io, ctl_refcnt_beio);
	ctl_set_kern_data_arg(io, beio);
	io->io_hdr.flags |= CTL_FLAG_ALLOCATED;

	/*
	 * For the read case, we need to read the data into our buffers and
	 * then we can send it back to the user.  For the write case, we
	 * need to get the data from the user first.
	 */
	if (beio->bio_cmd == BIO_READ) {
		SDT_PROBE0(cbb, , read, alloc_done);
		be_lun->dispatch(be_lun, beio);
	} else {
		SDT_PROBE0(cbb, , write, alloc_done);
		ctl_datamove(io);
	}
}

static void
ctl_be_block_worker(void *context, int pending)
{
	struct ctl_be_block_lun *be_lun = (struct ctl_be_block_lun *)context;
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	union ctl_io *io;
	struct ctl_be_block_io *beio;

	DPRINTF("entered\n");
	/*
	 * Fetch and process I/Os from all queues.  If we detect LUN
	 * CTL_LUN_FLAG_NO_MEDIA status here -- it is result of a race,
	 * so make response maximally opaque to not confuse initiator.
	 */
	for (;;) {
		mtx_lock(&be_lun->queue_lock);
		io = (union ctl_io *)STAILQ_FIRST(&be_lun->datamove_queue);
		if (io != NULL) {
			DPRINTF("datamove queue\n");
			STAILQ_REMOVE_HEAD(&be_lun->datamove_queue, links);
			mtx_unlock(&be_lun->queue_lock);
			beio = (struct ctl_be_block_io *)PRIV(io)->ptr;
			if (cbe_lun->flags & CTL_LUN_FLAG_NO_MEDIA) {
				ctl_io_set_busy(io);
				ctl_complete_beio(beio);
				continue;
			}
			be_lun->dispatch(be_lun, beio);
			continue;
		}
		io = (union ctl_io *)STAILQ_FIRST(&be_lun->config_write_queue);
		if (io != NULL) {
			DPRINTF("config write queue\n");
			STAILQ_REMOVE_HEAD(&be_lun->config_write_queue, links);
			mtx_unlock(&be_lun->queue_lock);
			if (cbe_lun->flags & CTL_LUN_FLAG_NO_MEDIA) {
				ctl_io_set_busy(io);
				ctl_config_write_done(io);
				continue;
			}
			ctl_be_block_cw_dispatch(be_lun, io);
			continue;
		}
		io = (union ctl_io *)STAILQ_FIRST(&be_lun->config_read_queue);
		if (io != NULL) {
			DPRINTF("config read queue\n");
			STAILQ_REMOVE_HEAD(&be_lun->config_read_queue, links);
			mtx_unlock(&be_lun->queue_lock);
			if (cbe_lun->flags & CTL_LUN_FLAG_NO_MEDIA) {
				ctl_io_set_busy(io);
				ctl_config_read_done(io);
				continue;
			}
			ctl_be_block_cr_dispatch(be_lun, io);
			continue;
		}
		io = (union ctl_io *)STAILQ_FIRST(&be_lun->input_queue);
		if (io != NULL) {
			DPRINTF("input queue\n");
			STAILQ_REMOVE_HEAD(&be_lun->input_queue, links);
			mtx_unlock(&be_lun->queue_lock);
			if (cbe_lun->flags & CTL_LUN_FLAG_NO_MEDIA) {
				ctl_io_set_busy(io);
				ctl_data_submit_done(io);
				continue;
			}
			ctl_be_block_dispatch(be_lun, io);
			continue;
		}

		/*
		 * If we get here, there is no work left in the queues, so
		 * just break out and let the task queue go to sleep.
		 */
		mtx_unlock(&be_lun->queue_lock);
		break;
	}
}

/*
 * Entry point from CTL to the backend for I/O.  We queue everything to a
 * work thread, so this just puts the I/O on a queue and wakes up the
 * thread.
 */
static int
ctl_be_block_submit(union ctl_io *io)
{
	struct ctl_be_block_lun *be_lun;

	DPRINTF("entered\n");

	be_lun = (struct ctl_be_block_lun *)CTL_BACKEND_LUN(io);

	CTL_IO_ASSERT(io, SCSI, NVME);

	PRIV(io)->len = 0;

	mtx_lock(&be_lun->queue_lock);
	STAILQ_INSERT_TAIL(&be_lun->input_queue, &io->io_hdr, links);
	mtx_unlock(&be_lun->queue_lock);
	taskqueue_enqueue(be_lun->io_taskqueue, &be_lun->io_task);

	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_be_block_ioctl(struct cdev *dev, u_long cmd, caddr_t addr,
			int flag, struct thread *td)
{
	struct ctl_be_block_softc *softc = &backend_block_softc;
	int error;

	error = 0;
	switch (cmd) {
	case CTL_LUN_REQ: {
		struct ctl_lun_req *lun_req;

		lun_req = (struct ctl_lun_req *)addr;

		switch (lun_req->reqtype) {
		case CTL_LUNREQ_CREATE:
			error = ctl_be_block_create(softc, lun_req);
			break;
		case CTL_LUNREQ_RM:
			error = ctl_be_block_rm(softc, lun_req);
			break;
		case CTL_LUNREQ_MODIFY:
			error = ctl_be_block_modify(softc, lun_req);
			break;
		default:
			lun_req->status = CTL_LUN_ERROR;
			snprintf(lun_req->error_str, sizeof(lun_req->error_str),
				 "invalid LUN request type %d",
				 lun_req->reqtype);
			break;
		}
		break;
	}
	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static int
ctl_be_block_open_file(struct ctl_be_block_lun *be_lun, struct ctl_lun_req *req)
{
	struct ctl_be_lun *cbe_lun;
	struct ctl_be_block_filedata *file_data;
	struct ctl_lun_create_params *params;
	const char		     *value;
	struct vattr		      vattr;
	off_t			      ps, pss, po, pos, us, uss, uo, uos;
	int			      error;
	long			      pconf;

	cbe_lun = &be_lun->cbe_lun;
	file_data = &be_lun->backend.file;
	params = &be_lun->params;

	be_lun->dev_type = CTL_BE_BLOCK_FILE;
	be_lun->dispatch = ctl_be_block_dispatch_file;
	be_lun->lun_flush = ctl_be_block_flush_file;
	be_lun->get_lba_status = ctl_be_block_gls_file;
	be_lun->getattr = ctl_be_block_getattr_file;
	be_lun->unmap = ctl_be_block_unmap_file;
	cbe_lun->flags &= ~CTL_LUN_FLAG_UNMAP;

	error = VOP_GETATTR(be_lun->vn, &vattr, curthread->td_ucred);
	if (error != 0) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "error calling VOP_GETATTR() for file %s",
			 be_lun->dev_path);
		return (error);
	}

	error = VOP_PATHCONF(be_lun->vn, _PC_DEALLOC_PRESENT, &pconf);
	if (error != 0) {
		snprintf(req->error_str, sizeof(req->error_str),
		    "error calling VOP_PATHCONF() for file %s",
		    be_lun->dev_path);
		return (error);
	}
	if (pconf == 1)
		cbe_lun->flags |= CTL_LUN_FLAG_UNMAP;

	file_data->cred = crhold(curthread->td_ucred);
	if (params->lun_size_bytes != 0)
		be_lun->size_bytes = params->lun_size_bytes;
	else
		be_lun->size_bytes = vattr.va_size;

	/*
	 * For files we can use any logical block size.  Prefer 512 bytes
	 * for compatibility reasons.  If file's vattr.va_blocksize
	 * (preferred I/O block size) is bigger and multiple to chosen
	 * logical block size -- report it as physical block size.
	 */
	if (params->blocksize_bytes != 0)
		cbe_lun->blocksize = params->blocksize_bytes;
	else if (cbe_lun->lun_type == T_CDROM)
		cbe_lun->blocksize = 2048;
	else
		cbe_lun->blocksize = 512;
	be_lun->size_blocks = be_lun->size_bytes / cbe_lun->blocksize;
	cbe_lun->maxlba = (be_lun->size_blocks == 0) ?
	    0 : (be_lun->size_blocks - 1);

	us = ps = vattr.va_blocksize;
	uo = po = 0;

	value = dnvlist_get_string(cbe_lun->options, "pblocksize", NULL);
	if (value != NULL)
		ctl_expand_number(value, &ps);
	value = dnvlist_get_string(cbe_lun->options, "pblockoffset", NULL);
	if (value != NULL)
		ctl_expand_number(value, &po);
	pss = ps / cbe_lun->blocksize;
	pos = po / cbe_lun->blocksize;
	if ((pss > 0) && (pss * cbe_lun->blocksize == ps) && (pss >= pos) &&
	    ((pss & (pss - 1)) == 0) && (pos * cbe_lun->blocksize == po)) {
		cbe_lun->pblockexp = fls(pss) - 1;
		cbe_lun->pblockoff = (pss - pos) % pss;
	}

	value = dnvlist_get_string(cbe_lun->options, "ublocksize", NULL);
	if (value != NULL)
		ctl_expand_number(value, &us);
	value = dnvlist_get_string(cbe_lun->options, "ublockoffset", NULL);
	if (value != NULL)
		ctl_expand_number(value, &uo);
	uss = us / cbe_lun->blocksize;
	uos = uo / cbe_lun->blocksize;
	if ((uss > 0) && (uss * cbe_lun->blocksize == us) && (uss >= uos) &&
	    ((uss & (uss - 1)) == 0) && (uos * cbe_lun->blocksize == uo)) {
		cbe_lun->ublockexp = fls(uss) - 1;
		cbe_lun->ublockoff = (uss - uos) % uss;
	}

	/*
	 * Sanity check.  The media size has to be at least one
	 * sector long.
	 */
	if (be_lun->size_bytes < cbe_lun->blocksize) {
		error = EINVAL;
		snprintf(req->error_str, sizeof(req->error_str),
			 "file %s size %ju < block size %u", be_lun->dev_path,
			 (uintmax_t)be_lun->size_bytes, cbe_lun->blocksize);
	}

	cbe_lun->opttxferlen = CTLBLK_MAX_IO_SIZE / cbe_lun->blocksize;
	return (error);
}

static int
ctl_be_block_open_dev(struct ctl_be_block_lun *be_lun, struct ctl_lun_req *req)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct ctl_lun_create_params *params;
	struct cdevsw		     *csw;
	struct cdev		     *dev;
	const char		     *value;
	int			      error, atomic, maxio, ref, unmap, tmp;
	off_t			      ps, pss, po, pos, us, uss, uo, uos, otmp;

	params = &be_lun->params;

	be_lun->dev_type = CTL_BE_BLOCK_DEV;
	csw = devvn_refthread(be_lun->vn, &dev, &ref);
	if (csw == NULL)
		return (ENXIO);
	if (strcmp(csw->d_name, "zvol") == 0) {
		be_lun->dispatch = ctl_be_block_dispatch_zvol;
		be_lun->get_lba_status = ctl_be_block_gls_zvol;
		atomic = maxio = CTLBLK_MAX_IO_SIZE;
	} else {
		be_lun->dispatch = ctl_be_block_dispatch_dev;
		be_lun->get_lba_status = NULL;
		atomic = 0;
		maxio = dev->si_iosize_max;
		if (maxio <= 0)
			maxio = DFLTPHYS;
		if (maxio > CTLBLK_MAX_SEG)
			maxio = CTLBLK_MAX_SEG;
	}
	be_lun->lun_flush = ctl_be_block_flush_dev;
	be_lun->getattr = ctl_be_block_getattr_dev;
	be_lun->unmap = ctl_be_block_unmap_dev;

	if (!csw->d_ioctl) {
		dev_relthread(dev, ref);
		snprintf(req->error_str, sizeof(req->error_str),
			 "no d_ioctl for device %s!", be_lun->dev_path);
		return (ENODEV);
	}

	error = csw->d_ioctl(dev, DIOCGSECTORSIZE, (caddr_t)&tmp, FREAD,
			       curthread);
	if (error) {
		dev_relthread(dev, ref);
		snprintf(req->error_str, sizeof(req->error_str),
			 "error %d returned for DIOCGSECTORSIZE ioctl "
			 "on %s!", error, be_lun->dev_path);
		return (error);
	}

	/*
	 * If the user has asked for a blocksize that is greater than the
	 * backing device's blocksize, we can do it only if the blocksize
	 * the user is asking for is an even multiple of the underlying 
	 * device's blocksize.
	 */
	if ((params->blocksize_bytes != 0) &&
	    (params->blocksize_bytes >= tmp)) {
		if (params->blocksize_bytes % tmp == 0) {
			cbe_lun->blocksize = params->blocksize_bytes;
		} else {
			dev_relthread(dev, ref);
			snprintf(req->error_str, sizeof(req->error_str),
				 "requested blocksize %u is not an even "
				 "multiple of backing device blocksize %u",
				 params->blocksize_bytes, tmp);
			return (EINVAL);
		}
	} else if (params->blocksize_bytes != 0) {
		dev_relthread(dev, ref);
		snprintf(req->error_str, sizeof(req->error_str),
			 "requested blocksize %u < backing device "
			 "blocksize %u", params->blocksize_bytes, tmp);
		return (EINVAL);
	} else if (cbe_lun->lun_type == T_CDROM)
		cbe_lun->blocksize = MAX(tmp, 2048);
	else
		cbe_lun->blocksize = tmp;

	error = csw->d_ioctl(dev, DIOCGMEDIASIZE, (caddr_t)&otmp, FREAD,
			     curthread);
	if (error) {
		dev_relthread(dev, ref);
		snprintf(req->error_str, sizeof(req->error_str),
			 "error %d returned for DIOCGMEDIASIZE "
			 " ioctl on %s!", error,
			 be_lun->dev_path);
		return (error);
	}

	if (params->lun_size_bytes != 0) {
		if (params->lun_size_bytes > otmp) {
			dev_relthread(dev, ref);
			snprintf(req->error_str, sizeof(req->error_str),
				 "requested LUN size %ju > backing device "
				 "size %ju",
				 (uintmax_t)params->lun_size_bytes,
				 (uintmax_t)otmp);
			return (EINVAL);
		}

		be_lun->size_bytes = params->lun_size_bytes;
	} else
		be_lun->size_bytes = otmp;
	be_lun->size_blocks = be_lun->size_bytes / cbe_lun->blocksize;
	cbe_lun->maxlba = (be_lun->size_blocks == 0) ?
	    0 : (be_lun->size_blocks - 1);

	error = csw->d_ioctl(dev, DIOCGSTRIPESIZE, (caddr_t)&ps, FREAD,
	    curthread);
	if (error)
		ps = po = 0;
	else {
		error = csw->d_ioctl(dev, DIOCGSTRIPEOFFSET, (caddr_t)&po,
		    FREAD, curthread);
		if (error)
			po = 0;
	}
	us = ps;
	uo = po;

	value = dnvlist_get_string(cbe_lun->options, "pblocksize", NULL);
	if (value != NULL)
		ctl_expand_number(value, &ps);
	value = dnvlist_get_string(cbe_lun->options, "pblockoffset", NULL);
	if (value != NULL)
		ctl_expand_number(value, &po);
	pss = ps / cbe_lun->blocksize;
	pos = po / cbe_lun->blocksize;
	if ((pss > 0) && (pss * cbe_lun->blocksize == ps) && (pss >= pos) &&
	    ((pss & (pss - 1)) == 0) && (pos * cbe_lun->blocksize == po)) {
		cbe_lun->pblockexp = fls(pss) - 1;
		cbe_lun->pblockoff = (pss - pos) % pss;
	}

	value = dnvlist_get_string(cbe_lun->options, "ublocksize", NULL);
	if (value != NULL)
		ctl_expand_number(value, &us);
	value = dnvlist_get_string(cbe_lun->options, "ublockoffset", NULL);
	if (value != NULL)
		ctl_expand_number(value, &uo);
	uss = us / cbe_lun->blocksize;
	uos = uo / cbe_lun->blocksize;
	if ((uss > 0) && (uss * cbe_lun->blocksize == us) && (uss >= uos) &&
	    ((uss & (uss - 1)) == 0) && (uos * cbe_lun->blocksize == uo)) {
		cbe_lun->ublockexp = fls(uss) - 1;
		cbe_lun->ublockoff = (uss - uos) % uss;
	}

	cbe_lun->atomicblock = atomic / cbe_lun->blocksize;
	cbe_lun->opttxferlen = maxio / cbe_lun->blocksize;

	if (be_lun->dispatch == ctl_be_block_dispatch_zvol) {
		unmap = 1;
	} else {
		struct diocgattr_arg	arg;

		strlcpy(arg.name, "GEOM::candelete", sizeof(arg.name));
		arg.len = sizeof(arg.value.i);
		error = csw->d_ioctl(dev, DIOCGATTR, (caddr_t)&arg, FREAD,
		    curthread);
		unmap = (error == 0) ? arg.value.i : 0;
	}
	value = dnvlist_get_string(cbe_lun->options, "unmap", NULL);
	if (value != NULL)
		unmap = (strcmp(value, "on") == 0);
	if (unmap)
		cbe_lun->flags |= CTL_LUN_FLAG_UNMAP;
	else
		cbe_lun->flags &= ~CTL_LUN_FLAG_UNMAP;

	dev_relthread(dev, ref);
	return (0);
}

static int
ctl_be_block_close(struct ctl_be_block_lun *be_lun)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	int flags;

	if (be_lun->vn) {
		flags = FREAD;
		if ((cbe_lun->flags & CTL_LUN_FLAG_READONLY) == 0)
			flags |= FWRITE;
		(void)vn_close(be_lun->vn, flags, NOCRED, curthread);
		be_lun->vn = NULL;

		switch (be_lun->dev_type) {
		case CTL_BE_BLOCK_DEV:
			break;
		case CTL_BE_BLOCK_FILE:
			if (be_lun->backend.file.cred != NULL) {
				crfree(be_lun->backend.file.cred);
				be_lun->backend.file.cred = NULL;
			}
			break;
		case CTL_BE_BLOCK_NONE:
			break;
		default:
			panic("Unexpected backend type %d", be_lun->dev_type);
			break;
		}
		be_lun->dev_type = CTL_BE_BLOCK_NONE;
	}
	return (0);
}

static int
ctl_be_block_open(struct ctl_be_block_lun *be_lun, struct ctl_lun_req *req)
{
	struct ctl_be_lun *cbe_lun = &be_lun->cbe_lun;
	struct nameidata nd;
	const char	*value;
	int		 error, flags;

	error = 0;
	if (rootvnode == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "Root filesystem is not mounted");
		return (1);
	}
	pwd_ensure_dirs();

	value = dnvlist_get_string(cbe_lun->options, "file", NULL);
	if (value == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "no file argument specified");
		return (1);
	}
	free(be_lun->dev_path, M_CTLBLK);
	be_lun->dev_path = strdup(value, M_CTLBLK);

	flags = FREAD;
	value = dnvlist_get_string(cbe_lun->options, "readonly", NULL);
	if (value != NULL) {
		if (strcmp(value, "on") != 0)
			flags |= FWRITE;
	} else if (cbe_lun->lun_type == T_DIRECT)
		flags |= FWRITE;

again:
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, be_lun->dev_path);
	error = vn_open(&nd, &flags, 0, NULL);
	if ((error == EROFS || error == EACCES) && (flags & FWRITE)) {
		flags &= ~FWRITE;
		goto again;
	}
	if (error) {
		/*
		 * This is the only reasonable guess we can make as far as
		 * path if the user doesn't give us a fully qualified path.
		 * If they want to specify a file, they need to specify the
		 * full path.
		 */
		if (be_lun->dev_path[0] != '/') {
			char *dev_name;

			asprintf(&dev_name, M_CTLBLK, "/dev/%s",
				be_lun->dev_path);
			free(be_lun->dev_path, M_CTLBLK);
			be_lun->dev_path = dev_name;
			goto again;
		}
		snprintf(req->error_str, sizeof(req->error_str),
		    "error opening %s: %d", be_lun->dev_path, error);
		return (error);
	}
	if (flags & FWRITE)
		cbe_lun->flags &= ~CTL_LUN_FLAG_READONLY;
	else
		cbe_lun->flags |= CTL_LUN_FLAG_READONLY;

	NDFREE_PNBUF(&nd);
	be_lun->vn = nd.ni_vp;

	/* We only support disks and files. */
	if (vn_isdisk_error(be_lun->vn, &error)) {
		error = ctl_be_block_open_dev(be_lun, req);
	} else if (be_lun->vn->v_type == VREG) {
		error = ctl_be_block_open_file(be_lun, req);
	} else {
		error = EINVAL;
		snprintf(req->error_str, sizeof(req->error_str),
			 "%s is not a disk or plain file", be_lun->dev_path);
	}
	VOP_UNLOCK(be_lun->vn);

	if (error != 0)
		ctl_be_block_close(be_lun);
	cbe_lun->serseq = CTL_LUN_SERSEQ_OFF;
	if (be_lun->dispatch != ctl_be_block_dispatch_dev)
		cbe_lun->serseq = CTL_LUN_SERSEQ_SOFT;
	value = dnvlist_get_string(cbe_lun->options, "serseq", NULL);
	if (value != NULL && strcmp(value, "on") == 0)
		cbe_lun->serseq = CTL_LUN_SERSEQ_ON;
	else if (value != NULL && strcmp(value, "read") == 0)
		cbe_lun->serseq = CTL_LUN_SERSEQ_READ;
	else if (value != NULL && strcmp(value, "soft") == 0)
		cbe_lun->serseq = CTL_LUN_SERSEQ_SOFT;
	else if (value != NULL && strcmp(value, "off") == 0)
		cbe_lun->serseq = CTL_LUN_SERSEQ_OFF;
	return (0);
}

static int
ctl_be_block_create(struct ctl_be_block_softc *softc, struct ctl_lun_req *req)
{
	struct ctl_be_lun *cbe_lun;
	struct ctl_be_block_lun *be_lun;
	struct ctl_lun_create_params *params;
	char num_thread_str[16];
	char tmpstr[32];
	const char *value;
	int retval, num_threads;
	int tmp_num_threads;

	params = &req->reqdata.create;
	retval = 0;
	req->status = CTL_LUN_OK;

	be_lun = malloc(sizeof(*be_lun), M_CTLBLK, M_ZERO | M_WAITOK);
	cbe_lun = &be_lun->cbe_lun;
	be_lun->params = req->reqdata.create;
	be_lun->softc = softc;
	STAILQ_INIT(&be_lun->input_queue);
	STAILQ_INIT(&be_lun->config_read_queue);
	STAILQ_INIT(&be_lun->config_write_queue);
	STAILQ_INIT(&be_lun->datamove_queue);
	mtx_init(&be_lun->io_lock, "ctlblock io", NULL, MTX_DEF);
	mtx_init(&be_lun->queue_lock, "ctlblock queue", NULL, MTX_DEF);
	cbe_lun->options = nvlist_clone(req->args_nvl);

	if (params->flags & CTL_LUN_FLAG_DEV_TYPE)
		cbe_lun->lun_type = params->device_type;
	else
		cbe_lun->lun_type = T_DIRECT;
	be_lun->flags = 0;
	cbe_lun->flags = 0;
	value = dnvlist_get_string(cbe_lun->options, "ha_role", NULL);
	if (value != NULL) {
		if (strcmp(value, "primary") == 0)
			cbe_lun->flags |= CTL_LUN_FLAG_PRIMARY;
	} else if (control_softc->flags & CTL_FLAG_ACTIVE_SHELF)
		cbe_lun->flags |= CTL_LUN_FLAG_PRIMARY;

	if (cbe_lun->lun_type == T_DIRECT ||
	    cbe_lun->lun_type == T_CDROM) {
		be_lun->size_bytes = params->lun_size_bytes;
		if (params->blocksize_bytes != 0)
			cbe_lun->blocksize = params->blocksize_bytes;
		else if (cbe_lun->lun_type == T_CDROM)
			cbe_lun->blocksize = 2048;
		else
			cbe_lun->blocksize = 512;
		be_lun->size_blocks = be_lun->size_bytes / cbe_lun->blocksize;
		cbe_lun->maxlba = (be_lun->size_blocks == 0) ?
		    0 : (be_lun->size_blocks - 1);

		if ((cbe_lun->flags & CTL_LUN_FLAG_PRIMARY) ||
		    control_softc->ha_mode == CTL_HA_MODE_SER_ONLY) {
			retval = ctl_be_block_open(be_lun, req);
			if (retval != 0) {
				retval = 0;
				req->status = CTL_LUN_WARNING;
			}
		}
		num_threads = cbb_num_threads;
	} else {
		num_threads = 1;
	}

	value = dnvlist_get_string(cbe_lun->options, "num_threads", NULL);
	if (value != NULL) {
		tmp_num_threads = strtol(value, NULL, 0);

		/*
		 * We don't let the user specify less than one
		 * thread, but hope he's clueful enough not to
		 * specify 1000 threads.
		 */
		if (tmp_num_threads < 1) {
			snprintf(req->error_str, sizeof(req->error_str),
				 "invalid number of threads %s",
				 num_thread_str);
			goto bailout_error;
		}
		num_threads = tmp_num_threads;
	}

	if (be_lun->vn == NULL)
		cbe_lun->flags |= CTL_LUN_FLAG_NO_MEDIA;
	/* Tell the user the blocksize we ended up using */
	params->lun_size_bytes = be_lun->size_bytes;
	params->blocksize_bytes = cbe_lun->blocksize;
	if (params->flags & CTL_LUN_FLAG_ID_REQ) {
		cbe_lun->req_lun_id = params->req_lun_id;
		cbe_lun->flags |= CTL_LUN_FLAG_ID_REQ;
	} else
		cbe_lun->req_lun_id = 0;

	cbe_lun->lun_shutdown = ctl_be_block_lun_shutdown;
	cbe_lun->be = &ctl_be_block_driver;

	if ((params->flags & CTL_LUN_FLAG_SERIAL_NUM) == 0) {
		snprintf(tmpstr, sizeof(tmpstr), "MYSERIAL%04d",
			 softc->num_luns);
		strncpy((char *)cbe_lun->serial_num, tmpstr,
			MIN(sizeof(cbe_lun->serial_num), sizeof(tmpstr)));

		/* Tell the user what we used for a serial number */
		strncpy((char *)params->serial_num, tmpstr,
			MIN(sizeof(params->serial_num), sizeof(tmpstr)));
	} else { 
		strncpy((char *)cbe_lun->serial_num, params->serial_num,
			MIN(sizeof(cbe_lun->serial_num),
			sizeof(params->serial_num)));
	}
	if ((params->flags & CTL_LUN_FLAG_DEVID) == 0) {
		snprintf(tmpstr, sizeof(tmpstr), "MYDEVID%04d", softc->num_luns);
		strncpy((char *)cbe_lun->device_id, tmpstr,
			MIN(sizeof(cbe_lun->device_id), sizeof(tmpstr)));

		/* Tell the user what we used for a device ID */
		strncpy((char *)params->device_id, tmpstr,
			MIN(sizeof(params->device_id), sizeof(tmpstr)));
	} else {
		strncpy((char *)cbe_lun->device_id, params->device_id,
			MIN(sizeof(cbe_lun->device_id),
			    sizeof(params->device_id)));
	}

	TASK_INIT(&be_lun->io_task, /*priority*/0, ctl_be_block_worker, be_lun);

	be_lun->io_taskqueue = taskqueue_create("ctlblocktq", M_WAITOK,
	    taskqueue_thread_enqueue, /*context*/&be_lun->io_taskqueue);

	if (be_lun->io_taskqueue == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "unable to create taskqueue");
		goto bailout_error;
	}

	/*
	 * Note that we start the same number of threads by default for
	 * both the file case and the block device case.  For the file
	 * case, we need multiple threads to allow concurrency, because the
	 * vnode interface is designed to be a blocking interface.  For the
	 * block device case, ZFS zvols at least will block the caller's
	 * context in many instances, and so we need multiple threads to
	 * overcome that problem.  Other block devices don't need as many
	 * threads, but they shouldn't cause too many problems.
	 *
	 * If the user wants to just have a single thread for a block
	 * device, he can specify that when the LUN is created, or change
	 * the tunable/sysctl to alter the default number of threads.
	 */
	retval = taskqueue_start_threads_in_proc(&be_lun->io_taskqueue,
					 /*num threads*/num_threads,
					 /*priority*/PUSER,
					 /*proc*/control_softc->ctl_proc,
					 /*thread name*/"block");

	if (retval != 0)
		goto bailout_error;

	be_lun->num_threads = num_threads;

	retval = ctl_add_lun(&be_lun->cbe_lun);
	if (retval != 0) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "ctl_add_lun() returned error %d, see dmesg for "
			 "details", retval);
		retval = 0;
		goto bailout_error;
	}

	be_lun->disk_stats = devstat_new_entry("cbb", cbe_lun->lun_id,
					       cbe_lun->blocksize,
					       DEVSTAT_ALL_SUPPORTED,
					       cbe_lun->lun_type
					       | DEVSTAT_TYPE_IF_OTHER,
					       DEVSTAT_PRIORITY_OTHER);

	mtx_lock(&softc->lock);
	softc->num_luns++;
	SLIST_INSERT_HEAD(&softc->lun_list, be_lun, links);
	mtx_unlock(&softc->lock);

	params->req_lun_id = cbe_lun->lun_id;

	return (retval);

bailout_error:
	req->status = CTL_LUN_ERROR;

	if (be_lun->io_taskqueue != NULL)
		taskqueue_free(be_lun->io_taskqueue);
	ctl_be_block_close(be_lun);
	if (be_lun->dev_path != NULL)
		free(be_lun->dev_path, M_CTLBLK);
	nvlist_destroy(cbe_lun->options);
	mtx_destroy(&be_lun->queue_lock);
	mtx_destroy(&be_lun->io_lock);
	free(be_lun, M_CTLBLK);

	return (retval);
}

static int
ctl_be_block_rm(struct ctl_be_block_softc *softc, struct ctl_lun_req *req)
{
	struct ctl_lun_rm_params *params;
	struct ctl_be_block_lun *be_lun;
	struct ctl_be_lun *cbe_lun;
	int retval;

	params = &req->reqdata.rm;

	sx_xlock(&softc->modify_lock);
	mtx_lock(&softc->lock);
	SLIST_FOREACH(be_lun, &softc->lun_list, links) {
		if (be_lun->cbe_lun.lun_id == params->lun_id) {
			SLIST_REMOVE(&softc->lun_list, be_lun,
			    ctl_be_block_lun, links);
			softc->num_luns--;
			break;
		}
	}
	mtx_unlock(&softc->lock);
	sx_xunlock(&softc->modify_lock);
	if (be_lun == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "LUN %u is not managed by the block backend",
			 params->lun_id);
		goto bailout_error;
	}
	cbe_lun = &be_lun->cbe_lun;

	if (be_lun->vn != NULL) {
		cbe_lun->flags |= CTL_LUN_FLAG_NO_MEDIA;
		ctl_lun_no_media(cbe_lun);
		taskqueue_drain_all(be_lun->io_taskqueue);
		ctl_be_block_close(be_lun);
	}

	mtx_lock(&softc->lock);
	be_lun->flags |= CTL_BE_BLOCK_LUN_WAITING;
	mtx_unlock(&softc->lock);

	retval = ctl_remove_lun(cbe_lun);
	if (retval != 0) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "error %d returned from ctl_remove_lun() for "
			 "LUN %d", retval, params->lun_id);
		mtx_lock(&softc->lock);
		be_lun->flags &= ~CTL_BE_BLOCK_LUN_WAITING;
		mtx_unlock(&softc->lock);
		goto bailout_error;
	}

	mtx_lock(&softc->lock);
	while ((be_lun->flags & CTL_BE_BLOCK_LUN_UNCONFIGURED) == 0) {
		retval = msleep(be_lun, &softc->lock, PCATCH, "ctlblockrm", 0);
		if (retval == EINTR)
			break;
	}
	be_lun->flags &= ~CTL_BE_BLOCK_LUN_WAITING;
	if (be_lun->flags & CTL_BE_BLOCK_LUN_UNCONFIGURED) {
		mtx_unlock(&softc->lock);
		free(be_lun, M_CTLBLK);
	} else {
		mtx_unlock(&softc->lock);
		return (EINTR);
	}

	req->status = CTL_LUN_OK;
	return (0);

bailout_error:
	req->status = CTL_LUN_ERROR;
	return (0);
}

static int
ctl_be_block_modify(struct ctl_be_block_softc *softc, struct ctl_lun_req *req)
{
	struct ctl_lun_modify_params *params;
	struct ctl_be_block_lun *be_lun;
	struct ctl_be_lun *cbe_lun;
	const char *value;
	uint64_t oldsize;
	int error, wasprim;

	params = &req->reqdata.modify;

	sx_xlock(&softc->modify_lock);
	mtx_lock(&softc->lock);
	SLIST_FOREACH(be_lun, &softc->lun_list, links) {
		if (be_lun->cbe_lun.lun_id == params->lun_id)
			break;
	}
	mtx_unlock(&softc->lock);
	if (be_lun == NULL) {
		snprintf(req->error_str, sizeof(req->error_str),
			 "LUN %u is not managed by the block backend",
			 params->lun_id);
		goto bailout_error;
	}
	cbe_lun = &be_lun->cbe_lun;

	if (params->lun_size_bytes != 0)
		be_lun->params.lun_size_bytes = params->lun_size_bytes;

	if (req->args_nvl != NULL) {
		nvlist_destroy(cbe_lun->options);
		cbe_lun->options = nvlist_clone(req->args_nvl);
	}

	wasprim = (cbe_lun->flags & CTL_LUN_FLAG_PRIMARY);
	value = dnvlist_get_string(cbe_lun->options, "ha_role", NULL);
	if (value != NULL) {
		if (strcmp(value, "primary") == 0)
			cbe_lun->flags |= CTL_LUN_FLAG_PRIMARY;
		else
			cbe_lun->flags &= ~CTL_LUN_FLAG_PRIMARY;
	} else if (control_softc->flags & CTL_FLAG_ACTIVE_SHELF)
		cbe_lun->flags |= CTL_LUN_FLAG_PRIMARY;
	else
		cbe_lun->flags &= ~CTL_LUN_FLAG_PRIMARY;
	if (wasprim != (cbe_lun->flags & CTL_LUN_FLAG_PRIMARY)) {
		if (cbe_lun->flags & CTL_LUN_FLAG_PRIMARY)
			ctl_lun_primary(cbe_lun);
		else
			ctl_lun_secondary(cbe_lun);
	}

	oldsize = be_lun->size_blocks;
	if ((cbe_lun->flags & CTL_LUN_FLAG_PRIMARY) ||
	    control_softc->ha_mode == CTL_HA_MODE_SER_ONLY) {
		if (be_lun->vn == NULL)
			error = ctl_be_block_open(be_lun, req);
		else if (vn_isdisk_error(be_lun->vn, &error))
			error = ctl_be_block_open_dev(be_lun, req);
		else if (be_lun->vn->v_type == VREG) {
			vn_lock(be_lun->vn, LK_SHARED | LK_RETRY);
			error = ctl_be_block_open_file(be_lun, req);
			VOP_UNLOCK(be_lun->vn);
		} else
			error = EINVAL;
		if ((cbe_lun->flags & CTL_LUN_FLAG_NO_MEDIA) &&
		    be_lun->vn != NULL) {
			cbe_lun->flags &= ~CTL_LUN_FLAG_NO_MEDIA;
			ctl_lun_has_media(cbe_lun);
		} else if ((cbe_lun->flags & CTL_LUN_FLAG_NO_MEDIA) == 0 &&
		    be_lun->vn == NULL) {
			cbe_lun->flags |= CTL_LUN_FLAG_NO_MEDIA;
			ctl_lun_no_media(cbe_lun);
		}
		cbe_lun->flags &= ~CTL_LUN_FLAG_EJECTED;
	} else {
		if (be_lun->vn != NULL) {
			cbe_lun->flags |= CTL_LUN_FLAG_NO_MEDIA;
			ctl_lun_no_media(cbe_lun);
			taskqueue_drain_all(be_lun->io_taskqueue);
			error = ctl_be_block_close(be_lun);
		} else
			error = 0;
	}
	if (be_lun->size_blocks != oldsize)
		ctl_lun_capacity_changed(cbe_lun);

	/* Tell the user the exact size we ended up using */
	params->lun_size_bytes = be_lun->size_bytes;

	sx_xunlock(&softc->modify_lock);
	req->status = error ? CTL_LUN_WARNING : CTL_LUN_OK;
	return (0);

bailout_error:
	sx_xunlock(&softc->modify_lock);
	req->status = CTL_LUN_ERROR;
	return (0);
}

static void
ctl_be_block_lun_shutdown(struct ctl_be_lun *cbe_lun)
{
	struct ctl_be_block_lun *be_lun = (struct ctl_be_block_lun *)cbe_lun;
	struct ctl_be_block_softc *softc = be_lun->softc;

	taskqueue_drain_all(be_lun->io_taskqueue);
	taskqueue_free(be_lun->io_taskqueue);
	if (be_lun->disk_stats != NULL)
		devstat_remove_entry(be_lun->disk_stats);
	nvlist_destroy(be_lun->cbe_lun.options);
	free(be_lun->dev_path, M_CTLBLK);
	mtx_destroy(&be_lun->queue_lock);
	mtx_destroy(&be_lun->io_lock);

	mtx_lock(&softc->lock);
	be_lun->flags |= CTL_BE_BLOCK_LUN_UNCONFIGURED;
	if (be_lun->flags & CTL_BE_BLOCK_LUN_WAITING)
		wakeup(be_lun);
	else
		free(be_lun, M_CTLBLK);
	mtx_unlock(&softc->lock);
}

static int
ctl_be_block_scsi_config_write(union ctl_io *io)
{
	struct ctl_be_block_lun *be_lun;
	struct ctl_be_lun *cbe_lun;
	int retval;

	DPRINTF("entered\n");

	cbe_lun = CTL_BACKEND_LUN(io);
	be_lun = (struct ctl_be_block_lun *)cbe_lun;

	retval = 0;
	switch (io->scsiio.cdb[0]) {
	case SYNCHRONIZE_CACHE:
	case SYNCHRONIZE_CACHE_16:
	case WRITE_SAME_10:
	case WRITE_SAME_16:
	case UNMAP:
		/*
		 * The upper level CTL code will filter out any CDBs with
		 * the immediate bit set and return the proper error.
		 *
		 * We don't really need to worry about what LBA range the
		 * user asked to be synced out.  When they issue a sync
		 * cache command, we'll sync out the whole thing.
		 */
		mtx_lock(&be_lun->queue_lock);
		STAILQ_INSERT_TAIL(&be_lun->config_write_queue, &io->io_hdr,
				   links);
		mtx_unlock(&be_lun->queue_lock);
		taskqueue_enqueue(be_lun->io_taskqueue, &be_lun->io_task);
		break;
	case START_STOP_UNIT: {
		struct scsi_start_stop_unit *cdb;
		struct ctl_lun_req req;

		cdb = (struct scsi_start_stop_unit *)io->scsiio.cdb;
		if ((cdb->how & SSS_PC_MASK) != 0) {
			ctl_set_success(&io->scsiio);
			ctl_config_write_done(io);
			break;
		}
		if (cdb->how & SSS_START) {
			if ((cdb->how & SSS_LOEJ) && be_lun->vn == NULL) {
				retval = ctl_be_block_open(be_lun, &req);
				cbe_lun->flags &= ~CTL_LUN_FLAG_EJECTED;
				if (retval == 0) {
					cbe_lun->flags &= ~CTL_LUN_FLAG_NO_MEDIA;
					ctl_lun_has_media(cbe_lun);
				} else {
					cbe_lun->flags |= CTL_LUN_FLAG_NO_MEDIA;
					ctl_lun_no_media(cbe_lun);
				}
			}
			ctl_start_lun(cbe_lun);
		} else {
			ctl_stop_lun(cbe_lun);
			if (cdb->how & SSS_LOEJ) {
				cbe_lun->flags |= CTL_LUN_FLAG_NO_MEDIA;
				cbe_lun->flags |= CTL_LUN_FLAG_EJECTED;
				ctl_lun_ejected(cbe_lun);
				if (be_lun->vn != NULL)
					ctl_be_block_close(be_lun);
			}
		}

		ctl_set_success(&io->scsiio);
		ctl_config_write_done(io);
		break;
	}
	case PREVENT_ALLOW:
		ctl_set_success(&io->scsiio);
		ctl_config_write_done(io);
		break;
	default:
		ctl_set_invalid_opcode(&io->scsiio);
		ctl_config_write_done(io);
		retval = CTL_RETVAL_COMPLETE;
		break;
	}

	return (retval);
}

static int
ctl_be_block_nvme_config_write(union ctl_io *io)
{
	struct ctl_be_block_lun *be_lun;

	DPRINTF("entered\n");

	be_lun = (struct ctl_be_block_lun *)CTL_BACKEND_LUN(io);

	switch (io->nvmeio.cmd.opc) {
	case NVME_OPC_DATASET_MANAGEMENT:
		DSM_RANGE(io) = 0;
		/* FALLTHROUGH */
	case NVME_OPC_FLUSH:
	case NVME_OPC_WRITE_UNCORRECTABLE:
	case NVME_OPC_WRITE_ZEROES:
		mtx_lock(&be_lun->queue_lock);
		STAILQ_INSERT_TAIL(&be_lun->config_write_queue, &io->io_hdr,
				   links);
		mtx_unlock(&be_lun->queue_lock);
		taskqueue_enqueue(be_lun->io_taskqueue, &be_lun->io_task);
		break;
	default:
		ctl_nvme_set_invalid_opcode(&io->nvmeio);
		ctl_config_write_done(io);
		break;
	}
	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_be_block_config_write(union ctl_io *io)
{
	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		return (ctl_be_block_scsi_config_write(io));
	case CTL_IO_NVME:
		return (ctl_be_block_nvme_config_write(io));
	default:
		__assert_unreachable();
	}
}

static int
ctl_be_block_scsi_config_read(union ctl_io *io)
{
	struct ctl_be_block_lun *be_lun;
	int retval = 0;

	DPRINTF("entered\n");

	be_lun = (struct ctl_be_block_lun *)CTL_BACKEND_LUN(io);

	switch (io->scsiio.cdb[0]) {
	case SERVICE_ACTION_IN:
		if (io->scsiio.cdb[1] == SGLS_SERVICE_ACTION) {
			mtx_lock(&be_lun->queue_lock);
			STAILQ_INSERT_TAIL(&be_lun->config_read_queue,
			    &io->io_hdr, links);
			mtx_unlock(&be_lun->queue_lock);
			taskqueue_enqueue(be_lun->io_taskqueue,
			    &be_lun->io_task);
			retval = CTL_RETVAL_QUEUED;
			break;
		}
		ctl_set_invalid_field(&io->scsiio,
				      /*sks_valid*/ 1,
				      /*command*/ 1,
				      /*field*/ 1,
				      /*bit_valid*/ 1,
				      /*bit*/ 4);
		ctl_config_read_done(io);
		retval = CTL_RETVAL_COMPLETE;
		break;
	default:
		ctl_set_invalid_opcode(&io->scsiio);
		ctl_config_read_done(io);
		retval = CTL_RETVAL_COMPLETE;
		break;
	}

	return (retval);
}

static int
ctl_be_block_nvme_config_read(union ctl_io *io)
{
	struct ctl_be_block_lun *be_lun;

	DPRINTF("entered\n");

	be_lun = (struct ctl_be_block_lun *)CTL_BACKEND_LUN(io);

	switch (io->nvmeio.cmd.opc) {
	case NVME_OPC_IDENTIFY:
	{
		uint8_t cns;

		cns = le32toh(io->nvmeio.cmd.cdw10) & 0xff;
		switch (cns) {
		case 0:
		case 3:
			mtx_lock(&be_lun->queue_lock);
			STAILQ_INSERT_TAIL(&be_lun->config_read_queue,
			    &io->io_hdr, links);
			mtx_unlock(&be_lun->queue_lock);
			taskqueue_enqueue(be_lun->io_taskqueue,
			    &be_lun->io_task);
			return (CTL_RETVAL_QUEUED);
		default:
			ctl_nvme_set_invalid_field(&io->nvmeio);
			ctl_config_read_done(io);
			break;
		}
		break;
	}
	default:
		ctl_nvme_set_invalid_opcode(&io->nvmeio);
		ctl_config_read_done(io);
		break;
	}
	return (CTL_RETVAL_COMPLETE);
}

static int
ctl_be_block_config_read(union ctl_io *io)
{
	switch (io->io_hdr.io_type) {
	case CTL_IO_SCSI:
		return (ctl_be_block_scsi_config_read(io));
	case CTL_IO_NVME_ADMIN:
		return (ctl_be_block_nvme_config_read(io));
	default:
		__assert_unreachable();
	}
}

static int
ctl_be_block_lun_info(struct ctl_be_lun *cbe_lun, struct sbuf *sb)
{
	struct ctl_be_block_lun *lun = (struct ctl_be_block_lun *)cbe_lun;
	int retval;

	retval = sbuf_cat(sb, "\t<num_threads>");
	if (retval != 0)
		goto bailout;
	retval = sbuf_printf(sb, "%d", lun->num_threads);
	if (retval != 0)
		goto bailout;
	retval = sbuf_cat(sb, "</num_threads>\n");

bailout:
	return (retval);
}

static uint64_t
ctl_be_block_lun_attr(struct ctl_be_lun *cbe_lun, const char *attrname)
{
	struct ctl_be_block_lun *lun = (struct ctl_be_block_lun *)cbe_lun;

	if (lun->getattr == NULL)
		return (UINT64_MAX);
	return (lun->getattr(lun, attrname));
}

static int
ctl_be_block_init(void)
{
	struct ctl_be_block_softc *softc = &backend_block_softc;

	sx_init(&softc->modify_lock, "ctlblock modify");
	mtx_init(&softc->lock, "ctlblock", NULL, MTX_DEF);
	softc->beio_zone = uma_zcreate("beio", sizeof(struct ctl_be_block_io),
	    NULL, NULL, NULL, NULL, UMA_ALIGN_PTR, 0);
	softc->bufmin_zone = uma_zcreate("ctlblockmin", CTLBLK_MIN_SEG,
	    NULL, NULL, NULL, NULL, /*align*/ 0, /*flags*/0);
	if (CTLBLK_MIN_SEG < CTLBLK_MAX_SEG)
		softc->bufmax_zone = uma_zcreate("ctlblockmax", CTLBLK_MAX_SEG,
		    NULL, NULL, NULL, NULL, /*align*/ 0, /*flags*/0);
	SLIST_INIT(&softc->lun_list);
	return (0);
}

static int
ctl_be_block_shutdown(void)
{
	struct ctl_be_block_softc *softc = &backend_block_softc;
	struct ctl_be_block_lun *lun;

	mtx_lock(&softc->lock);
	while ((lun = SLIST_FIRST(&softc->lun_list)) != NULL) {
		SLIST_REMOVE_HEAD(&softc->lun_list, links);
		softc->num_luns--;
		/*
		 * Drop our lock here.  Since ctl_remove_lun() can call
		 * back into us, this could potentially lead to a recursive
		 * lock of the same mutex, which would cause a hang.
		 */
		mtx_unlock(&softc->lock);
		ctl_remove_lun(&lun->cbe_lun);
		mtx_lock(&softc->lock);
	}
	mtx_unlock(&softc->lock);
	uma_zdestroy(softc->bufmin_zone);
	if (CTLBLK_MIN_SEG < CTLBLK_MAX_SEG)
		uma_zdestroy(softc->bufmax_zone);
	uma_zdestroy(softc->beio_zone);
	mtx_destroy(&softc->lock);
	sx_destroy(&softc->modify_lock);
	return (0);
}
