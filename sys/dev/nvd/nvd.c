/*-
 * Copyright (C) 2012 Intel Corporation
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
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/module.h>
#include <sys/systm.h>
#include <sys/taskqueue.h>

#include <geom/geom.h>
#include <geom/geom_disk.h>

#include <dev/nvme/nvme.h>

struct nvd_disk;

static disk_ioctl_t nvd_ioctl;
static disk_strategy_t nvd_strategy;

static void *nvd_new_disk(struct nvme_namespace *ns, void *ctrlr);
static void destroy_geom_disk(struct nvd_disk *ndisk);

static void *nvd_new_controller(struct nvme_controller *ctrlr);
static void nvd_controller_fail(void *ctrlr);

static int nvd_load(void);
static void nvd_unload(void);

MALLOC_DEFINE(M_NVD, "nvd", "nvd(4) allocations");

struct nvme_consumer *consumer_handle;

struct nvd_disk {

	struct bio_queue_head	bioq;
	struct task		bioqtask;
	struct mtx		bioqlock;

	struct disk		*disk;
	struct taskqueue	*tq;
	struct nvme_namespace	*ns;

	uint32_t		cur_depth;

	TAILQ_ENTRY(nvd_disk)	global_tailq;
	TAILQ_ENTRY(nvd_disk)	ctrlr_tailq;
};

struct nvd_controller {

	TAILQ_ENTRY(nvd_controller)	tailq;
	TAILQ_HEAD(, nvd_disk)		disk_head;
};

static TAILQ_HEAD(, nvd_controller)	ctrlr_head;
static TAILQ_HEAD(disk_list, nvd_disk)	disk_head;

static int nvd_modevent(module_t mod, int type, void *arg)
{
	int error = 0;

	switch (type) {
	case MOD_LOAD:
		error = nvd_load();
		break;
	case MOD_UNLOAD:
		nvd_unload();
		break;
	default:
		break;
	}

	return (error);
}

moduledata_t nvd_mod = {
	"nvd",
	(modeventhand_t)nvd_modevent,
	0
};

DECLARE_MODULE(nvd, nvd_mod, SI_SUB_DRIVERS, SI_ORDER_ANY);
MODULE_VERSION(nvd, 1);
MODULE_DEPEND(nvd, nvme, 1, 1, 1);

static int
nvd_load()
{

	TAILQ_INIT(&ctrlr_head);
	TAILQ_INIT(&disk_head);

	consumer_handle = nvme_register_consumer(nvd_new_disk,
	    nvd_new_controller, NULL, nvd_controller_fail);

	return (consumer_handle != NULL ? 0 : -1);
}

static void
nvd_unload()
{
	struct nvd_controller	*ctrlr;
	struct nvd_disk		*disk;

	while (!TAILQ_EMPTY(&ctrlr_head)) {
		ctrlr = TAILQ_FIRST(&ctrlr_head);
		TAILQ_REMOVE(&ctrlr_head, ctrlr, tailq);
		free(ctrlr, M_NVD);
	}

	while (!TAILQ_EMPTY(&disk_head)) {
		disk = TAILQ_FIRST(&disk_head);
		TAILQ_REMOVE(&disk_head, disk, global_tailq);
		destroy_geom_disk(disk);
		free(disk, M_NVD);
	}

	nvme_unregister_consumer(consumer_handle);
}

static void
nvd_strategy(struct bio *bp)
{
	struct nvd_disk *ndisk;

	ndisk = (struct nvd_disk *)bp->bio_disk->d_drv1;

	mtx_lock(&ndisk->bioqlock);
	bioq_insert_tail(&ndisk->bioq, bp);
	mtx_unlock(&ndisk->bioqlock);
	taskqueue_enqueue(ndisk->tq, &ndisk->bioqtask);
}

static int
nvd_ioctl(struct disk *ndisk, u_long cmd, void *data, int fflag,
    struct thread *td)
{
	int ret = 0;

	switch (cmd) {
	default:
		ret = EIO;
	}

	return (ret);
}

static void
nvd_done(void *arg, const struct nvme_completion *cpl)
{
	struct bio *bp;
	struct nvd_disk *ndisk;

	bp = (struct bio *)arg;

	ndisk = bp->bio_disk->d_drv1;

	atomic_add_int(&ndisk->cur_depth, -1);

	/*
	 * TODO: add more extensive translation of NVMe status codes
	 *  to different bio error codes (i.e. EIO, EINVAL, etc.)
	 */
	if (nvme_completion_is_error(cpl)) {
		bp->bio_error = EIO;
		bp->bio_flags |= BIO_ERROR;
		bp->bio_resid = bp->bio_bcount;
	} else
		bp->bio_resid = 0;

	biodone(bp);
}

static void
nvd_bioq_process(void *arg, int pending)
{
	struct nvd_disk *ndisk = arg;
	struct bio *bp;
	int err;

	for (;;) {
		mtx_lock(&ndisk->bioqlock);
		bp = bioq_takefirst(&ndisk->bioq);
		mtx_unlock(&ndisk->bioqlock);
		if (bp == NULL)
			break;

#ifdef BIO_ORDERED
		/*
		 * BIO_ORDERED flag dictates that all outstanding bios
		 *  must be completed before processing the bio with
		 *  BIO_ORDERED flag set.
		 */
		if (bp->bio_flags & BIO_ORDERED) {
			while (ndisk->cur_depth > 0) {
				pause("nvd flush", 1);
			}
		}
#endif

		bp->bio_driver1 = NULL;
		atomic_add_int(&ndisk->cur_depth, 1);

		err = nvme_ns_bio_process(ndisk->ns, bp, nvd_done);

		if (err) {
			atomic_add_int(&ndisk->cur_depth, -1);
			bp->bio_error = err;
			bp->bio_flags |= BIO_ERROR;
			bp->bio_resid = bp->bio_bcount;
			biodone(bp);
		}

#ifdef BIO_ORDERED
		/*
		 * BIO_ORDERED flag dictates that the bio with BIO_ORDERED
		 *  flag set must be completed before proceeding with
		 *  additional bios.
		 */
		if (bp->bio_flags & BIO_ORDERED) {
			while (ndisk->cur_depth > 0) {
				pause("nvd flush", 1);
			}
		}
#endif
	}
}

static void *
nvd_new_controller(struct nvme_controller *ctrlr)
{
	struct nvd_controller	*nvd_ctrlr;

	nvd_ctrlr = malloc(sizeof(struct nvd_controller), M_NVD,
	    M_ZERO | M_WAITOK);

	TAILQ_INIT(&nvd_ctrlr->disk_head);
	TAILQ_INSERT_TAIL(&ctrlr_head, nvd_ctrlr, tailq);

	return (nvd_ctrlr);
}

static void *
nvd_new_disk(struct nvme_namespace *ns, void *ctrlr_arg)
{
	struct nvd_disk		*ndisk;
	struct disk		*disk;
	struct nvd_controller	*ctrlr = ctrlr_arg;

	ndisk = malloc(sizeof(struct nvd_disk), M_NVD, M_ZERO | M_WAITOK);

	disk = disk_alloc();
	disk->d_strategy = nvd_strategy;
	disk->d_ioctl = nvd_ioctl;
	disk->d_name = "nvd";
	disk->d_drv1 = ndisk;

	disk->d_maxsize = nvme_ns_get_max_io_xfer_size(ns);
	disk->d_sectorsize = nvme_ns_get_sector_size(ns);
	disk->d_mediasize = (off_t)nvme_ns_get_size(ns);

	if (TAILQ_EMPTY(&disk_head))
		disk->d_unit = 0;
	else
		disk->d_unit =
		    TAILQ_LAST(&disk_head, disk_list)->disk->d_unit + 1;

	disk->d_flags = 0;

	if (nvme_ns_get_flags(ns) & NVME_NS_DEALLOCATE_SUPPORTED)
		disk->d_flags |= DISKFLAG_CANDELETE;

	if (nvme_ns_get_flags(ns) & NVME_NS_FLUSH_SUPPORTED)
		disk->d_flags |= DISKFLAG_CANFLUSHCACHE;

	strlcpy(disk->d_ident, nvme_ns_get_serial_number(ns),
	    sizeof(disk->d_ident));

#if __FreeBSD_version >= 900034
	strlcpy(disk->d_descr, nvme_ns_get_model_number(ns),
	    sizeof(disk->d_descr));
#endif

	disk_create(disk, DISK_VERSION);

	ndisk->ns = ns;
	ndisk->disk = disk;
	ndisk->cur_depth = 0;

	mtx_init(&ndisk->bioqlock, "NVD bioq lock", NULL, MTX_DEF);
	bioq_init(&ndisk->bioq);

	TASK_INIT(&ndisk->bioqtask, 0, nvd_bioq_process, ndisk);
	ndisk->tq = taskqueue_create("nvd_taskq", M_WAITOK,
	    taskqueue_thread_enqueue, &ndisk->tq);
	taskqueue_start_threads(&ndisk->tq, 1, PI_DISK, "nvd taskq");

	TAILQ_INSERT_TAIL(&disk_head, ndisk, global_tailq);
	TAILQ_INSERT_TAIL(&ctrlr->disk_head, ndisk, ctrlr_tailq);

	return (NULL);
}

static void
destroy_geom_disk(struct nvd_disk *ndisk)
{
	struct bio *bp;

	taskqueue_free(ndisk->tq);
	disk_destroy(ndisk->disk);

	mtx_lock(&ndisk->bioqlock);
	for (;;) {
		bp = bioq_takefirst(&ndisk->bioq);
		if (bp == NULL)
			break;
		bp->bio_error = EIO;
		bp->bio_flags |= BIO_ERROR;
		bp->bio_resid = bp->bio_bcount;

		biodone(bp);
	}
	mtx_unlock(&ndisk->bioqlock);

	mtx_destroy(&ndisk->bioqlock);
}

static void
nvd_controller_fail(void *ctrlr_arg)
{
	struct nvd_controller	*ctrlr = ctrlr_arg;
	struct nvd_disk		*disk;

	while (!TAILQ_EMPTY(&ctrlr->disk_head)) {
		disk = TAILQ_FIRST(&ctrlr->disk_head);
		TAILQ_REMOVE(&disk_head, disk, global_tailq);
		TAILQ_REMOVE(&ctrlr->disk_head, disk, ctrlr_tailq);
		destroy_geom_disk(disk);
		free(disk, M_NVD);
	}

	TAILQ_REMOVE(&ctrlr_head, ctrlr, tailq);
	free(ctrlr, M_NVD);
}

