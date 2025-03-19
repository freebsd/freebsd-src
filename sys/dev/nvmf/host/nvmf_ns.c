/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023-2024 Chelsio Communications, Inc.
 * Written by: John Baldwin <jhb@FreeBSD.org>
 */

#include <sys/param.h>
#include <sys/bio.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/lock.h>
#include <sys/malloc.h>
#include <sys/memdesc.h>
#include <sys/mutex.h>
#include <sys/proc.h>
#include <sys/refcount.h>
#include <sys/sbuf.h>
#include <sys/stdarg.h>
#include <dev/nvme/nvme.h>
#include <dev/nvmf/host/nvmf_var.h>

struct nvmf_namespace {
	struct nvmf_softc *sc;
	uint64_t size;
	uint32_t id;
	u_int	flags;
	uint32_t lba_size;
	bool disconnected;
	bool shutdown;

	TAILQ_HEAD(, bio) pending_bios;
	struct mtx lock;
	volatile u_int active_bios;

	struct cdev *cdev;
};

static void	nvmf_ns_strategy(struct bio *bio);

static void
ns_printf(struct nvmf_namespace *ns, const char *fmt, ...)
{
	char buf[128];
	struct sbuf sb;
	va_list ap;

	sbuf_new(&sb, buf, sizeof(buf), SBUF_FIXEDLEN);
	sbuf_set_drain(&sb, sbuf_printf_drain, NULL);

	sbuf_printf(&sb, "%sn%u: ", device_get_nameunit(ns->sc->dev),
	    ns->id);

	va_start(ap, fmt);
	sbuf_vprintf(&sb, fmt, ap);
	va_end(ap);

	sbuf_finish(&sb);
	sbuf_delete(&sb);
}

/*
 * The I/O completion may trigger after the received CQE if the I/O
 * used a zero-copy mbuf that isn't harvested until after the NIC
 * driver processes TX completions.  Abuse bio_driver1 as a refcount.
 * Store I/O errors in bio_driver2.
 */
static __inline u_int *
bio_refs(struct bio *bio)
{
	return ((u_int *)&bio->bio_driver1);
}

static void
nvmf_ns_biodone(struct bio *bio)
{
	struct nvmf_namespace *ns;
	int error;

	if (!refcount_release(bio_refs(bio)))
		return;

	ns = bio->bio_dev->si_drv1;

	/* If a request is aborted, resubmit or queue it for resubmission. */
	if (bio->bio_error == ECONNABORTED && !nvmf_fail_disconnect) {
		bio->bio_error = 0;
		bio->bio_driver2 = 0;
		mtx_lock(&ns->lock);
		if (ns->disconnected) {
			if (nvmf_fail_disconnect || ns->shutdown) {
				mtx_unlock(&ns->lock);
				bio->bio_error = ECONNABORTED;
				bio->bio_flags |= BIO_ERROR;
				bio->bio_resid = bio->bio_bcount;
				biodone(bio);
			} else {
				TAILQ_INSERT_TAIL(&ns->pending_bios, bio,
				    bio_queue);
				mtx_unlock(&ns->lock);
			}
		} else {
			mtx_unlock(&ns->lock);
			nvmf_ns_strategy(bio);
		}
	} else {
		/*
		 * I/O errors take precedence over generic EIO from
		 * CQE errors.
		 */
		error = (intptr_t)bio->bio_driver2;
		if (error != 0)
			bio->bio_error = error;
		if (bio->bio_error != 0)
			bio->bio_flags |= BIO_ERROR;
		biodone(bio);
	}

	if (refcount_release(&ns->active_bios))
		wakeup(ns);
}

static void
nvmf_ns_io_complete(void *arg, size_t xfered, int error)
{
	struct bio *bio = arg;

	KASSERT(xfered <= bio->bio_bcount,
	    ("%s: xfered > bio_bcount", __func__));

	bio->bio_driver2 = (void *)(intptr_t)error;
	bio->bio_resid = bio->bio_bcount - xfered;

	nvmf_ns_biodone(bio);
}

static void
nvmf_ns_delete_complete(void *arg, size_t xfered, int error)
{
	struct bio *bio = arg;

	if (error != 0)
		bio->bio_resid = bio->bio_bcount;
	else
		bio->bio_resid = 0;

	free(bio->bio_driver2, M_NVMF);
	bio->bio_driver2 = (void *)(intptr_t)error;

	nvmf_ns_biodone(bio);
}

static void
nvmf_ns_bio_complete(void *arg, const struct nvme_completion *cqe)
{
	struct bio *bio = arg;

	if (nvmf_cqe_aborted(cqe))
		bio->bio_error = ECONNABORTED;
	else if (cqe->status != 0)
		bio->bio_error = EIO;

	nvmf_ns_biodone(bio);
}

static int
nvmf_ns_submit_bio(struct nvmf_namespace *ns, struct bio *bio)
{
	struct nvme_command cmd;
	struct nvmf_request *req;
	struct nvme_dsm_range *dsm_range;
	struct memdesc mem;
	uint64_t lba, lba_count;
	int error;

	dsm_range = NULL;
	memset(&cmd, 0, sizeof(cmd));
	switch (bio->bio_cmd) {
	case BIO_READ:
		lba = bio->bio_offset / ns->lba_size;
		lba_count = bio->bio_bcount / ns->lba_size;
		nvme_ns_read_cmd(&cmd, ns->id, lba, lba_count);
		break;
	case BIO_WRITE:
		lba = bio->bio_offset / ns->lba_size;
		lba_count = bio->bio_bcount / ns->lba_size;
		nvme_ns_write_cmd(&cmd, ns->id, lba, lba_count);
		break;
	case BIO_FLUSH:
		nvme_ns_flush_cmd(&cmd, ns->id);
		break;
	case BIO_DELETE:
		dsm_range = malloc(sizeof(*dsm_range), M_NVMF, M_NOWAIT |
		    M_ZERO);
		if (dsm_range == NULL)
			return (ENOMEM);
		lba = bio->bio_offset / ns->lba_size;
		lba_count = bio->bio_bcount / ns->lba_size;
		dsm_range->starting_lba = htole64(lba);
		dsm_range->length = htole32(lba_count);

		cmd.opc = NVME_OPC_DATASET_MANAGEMENT;
		cmd.nsid = htole32(ns->id);
		cmd.cdw10 = htole32(0);		/* 1 range */
		cmd.cdw11 = htole32(NVME_DSM_ATTR_DEALLOCATE);
		break;
	default:
		return (EOPNOTSUPP);
	}

	mtx_lock(&ns->lock);
	if (ns->disconnected) {
		if (nvmf_fail_disconnect || ns->shutdown) {
			error = ECONNABORTED;
		} else {
			TAILQ_INSERT_TAIL(&ns->pending_bios, bio, bio_queue);
			error = 0;
		}
		mtx_unlock(&ns->lock);
		free(dsm_range, M_NVMF);
		return (error);
	}

	req = nvmf_allocate_request(nvmf_select_io_queue(ns->sc), &cmd,
	    nvmf_ns_bio_complete, bio, M_NOWAIT);
	if (req == NULL) {
		mtx_unlock(&ns->lock);
		free(dsm_range, M_NVMF);
		return (ENOMEM);
	}

	switch (bio->bio_cmd) {
	case BIO_READ:
	case BIO_WRITE:
		refcount_init(bio_refs(bio), 2);
		mem = memdesc_bio(bio);
		nvmf_capsule_append_data(req->nc, &mem, bio->bio_bcount,
		    bio->bio_cmd == BIO_WRITE, nvmf_ns_io_complete, bio);
		break;
	case BIO_DELETE:
		refcount_init(bio_refs(bio), 2);
		mem = memdesc_vaddr(dsm_range, sizeof(*dsm_range));
		nvmf_capsule_append_data(req->nc, &mem, sizeof(*dsm_range),
		    true, nvmf_ns_delete_complete, bio);
		bio->bio_driver2 = dsm_range;
		break;
	default:
		refcount_init(bio_refs(bio), 1);
		KASSERT(bio->bio_resid == 0,
		    ("%s: input bio_resid != 0", __func__));
		break;
	}

	refcount_acquire(&ns->active_bios);
	nvmf_submit_request(req);
	mtx_unlock(&ns->lock);
	return (0);
}

static int
nvmf_ns_ioctl(struct cdev *dev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	struct nvmf_namespace *ns = dev->si_drv1;
	struct nvme_get_nsid *gnsid;
	struct nvme_pt_command *pt;

	switch (cmd) {
	case NVME_PASSTHROUGH_CMD:
		pt = (struct nvme_pt_command *)arg;
		pt->cmd.nsid = htole32(ns->id);
		return (nvmf_passthrough_cmd(ns->sc, pt, false));
	case NVME_GET_NSID:
		gnsid = (struct nvme_get_nsid *)arg;
		strlcpy(gnsid->cdev, device_get_nameunit(ns->sc->dev),
		    sizeof(gnsid->cdev));
		gnsid->nsid = ns->id;
		return (0);
	case DIOCGMEDIASIZE:
		*(off_t *)arg = ns->size;
		return (0);
	case DIOCGSECTORSIZE:
		*(u_int *)arg = ns->lba_size;
		return (0);
	default:
		return (ENOTTY);
	}
}

static int
nvmf_ns_open(struct cdev *dev, int oflags, int devtype, struct thread *td)
{
	int error;

	error = 0;
	if ((oflags & FWRITE) != 0)
		error = securelevel_gt(td->td_ucred, 0);
	return (error);
}

void
nvmf_ns_strategy(struct bio *bio)
{
	struct nvmf_namespace *ns;
	int error;

	ns = bio->bio_dev->si_drv1;

	error = nvmf_ns_submit_bio(ns, bio);
	if (error != 0) {
		bio->bio_error = error;
		bio->bio_flags |= BIO_ERROR;
		bio->bio_resid = bio->bio_bcount;
		biodone(bio);
	}
}

static struct cdevsw nvmf_ns_cdevsw = {
	.d_version = D_VERSION,
	.d_flags = D_DISK,
	.d_open = nvmf_ns_open,
	.d_read = physread,
	.d_write = physwrite,
	.d_strategy = nvmf_ns_strategy,
	.d_ioctl = nvmf_ns_ioctl
};

struct nvmf_namespace *
nvmf_init_ns(struct nvmf_softc *sc, uint32_t id,
    const struct nvme_namespace_data *data)
{
	struct make_dev_args mda;
	struct nvmf_namespace *ns;
	int error;
	uint8_t lbads, lbaf;

	ns = malloc(sizeof(*ns), M_NVMF, M_WAITOK | M_ZERO);
	ns->sc = sc;
	ns->id = id;
	TAILQ_INIT(&ns->pending_bios);
	mtx_init(&ns->lock, "nvmf ns", NULL, MTX_DEF);

	/* One dummy bio avoids dropping to 0 until destroy. */
	refcount_init(&ns->active_bios, 1);

	if (NVMEV(NVME_NS_DATA_DPS_PIT, data->dps) != 0) {
		ns_printf(ns, "End-to-end data protection not supported\n");
		goto fail;
	}

	lbaf = NVMEV(NVME_NS_DATA_FLBAS_FORMAT, data->flbas);
	if (lbaf > data->nlbaf) {
		ns_printf(ns, "Invalid LBA format index\n");
		goto fail;
	}

	if (NVMEV(NVME_NS_DATA_LBAF_MS, data->lbaf[lbaf]) != 0) {
		ns_printf(ns, "Namespaces with metadata are not supported\n");
		goto fail;
	}

	lbads = NVMEV(NVME_NS_DATA_LBAF_LBADS, data->lbaf[lbaf]);
	if (lbads == 0) {
		ns_printf(ns, "Invalid LBA format index\n");
		goto fail;
	}

	ns->lba_size = 1 << lbads;
	ns->size = data->nsze * ns->lba_size;

	if (nvme_ctrlr_has_dataset_mgmt(sc->cdata))
		ns->flags |= NVME_NS_DEALLOCATE_SUPPORTED;

	if (NVMEV(NVME_CTRLR_DATA_VWC_PRESENT, sc->cdata->vwc) != 0)
		ns->flags |= NVME_NS_FLUSH_SUPPORTED;

	/*
	 * XXX: Does any of the boundary splitting for NOIOB make any
	 * sense for Fabrics?
	 */

	make_dev_args_init(&mda);
	mda.mda_devsw = &nvmf_ns_cdevsw;
	mda.mda_uid = UID_ROOT;
	mda.mda_gid = GID_WHEEL;
	mda.mda_mode = 0600;
	mda.mda_si_drv1 = ns;
	error = make_dev_s(&mda, &ns->cdev, "%sn%u",
	    device_get_nameunit(sc->dev), id);
	if (error != 0)
		goto fail;
	ns->cdev->si_drv2 = make_dev_alias(ns->cdev, "%sns%u",
	    device_get_nameunit(sc->dev), id);

	ns->cdev->si_flags |= SI_UNMAPPED;

	return (ns);
fail:
	mtx_destroy(&ns->lock);
	free(ns, M_NVMF);
	return (NULL);
}

void
nvmf_disconnect_ns(struct nvmf_namespace *ns)
{
	mtx_lock(&ns->lock);
	ns->disconnected = true;
	mtx_unlock(&ns->lock);
}

void
nvmf_reconnect_ns(struct nvmf_namespace *ns)
{
	TAILQ_HEAD(, bio) bios;
	struct bio *bio;

	mtx_lock(&ns->lock);
	ns->disconnected = false;
	TAILQ_INIT(&bios);
	TAILQ_CONCAT(&bios, &ns->pending_bios, bio_queue);
	mtx_unlock(&ns->lock);

	while (!TAILQ_EMPTY(&bios)) {
		bio = TAILQ_FIRST(&bios);
		TAILQ_REMOVE(&bios, bio, bio_queue);
		nvmf_ns_strategy(bio);
	}
}

void
nvmf_shutdown_ns(struct nvmf_namespace *ns)
{
	TAILQ_HEAD(, bio) bios;
	struct bio *bio;

	mtx_lock(&ns->lock);
	ns->shutdown = true;
	TAILQ_INIT(&bios);
	TAILQ_CONCAT(&bios, &ns->pending_bios, bio_queue);
	mtx_unlock(&ns->lock);

	while (!TAILQ_EMPTY(&bios)) {
		bio = TAILQ_FIRST(&bios);
		TAILQ_REMOVE(&bios, bio, bio_queue);
		bio->bio_error = ECONNABORTED;
		bio->bio_flags |= BIO_ERROR;
		bio->bio_resid = bio->bio_bcount;
		biodone(bio);
	}
}

void
nvmf_destroy_ns(struct nvmf_namespace *ns)
{
	TAILQ_HEAD(, bio) bios;
	struct bio *bio;

	if (ns->cdev->si_drv2 != NULL)
		destroy_dev(ns->cdev->si_drv2);
	destroy_dev(ns->cdev);

	/*
	 * Wait for active I/O requests to drain.  The release drops
	 * the reference on the "dummy bio" when the namespace is
	 * created.
	 */
	mtx_lock(&ns->lock);
	if (!refcount_release(&ns->active_bios)) {
		while (ns->active_bios != 0)
			mtx_sleep(ns, &ns->lock, 0, "nvmfrmns", 0);
	}

	/* Abort any pending I/O requests. */
	TAILQ_INIT(&bios);
	TAILQ_CONCAT(&bios, &ns->pending_bios, bio_queue);
	mtx_unlock(&ns->lock);

	while (!TAILQ_EMPTY(&bios)) {
		bio = TAILQ_FIRST(&bios);
		TAILQ_REMOVE(&bios, bio, bio_queue);
		bio->bio_error = ECONNABORTED;
		bio->bio_flags |= BIO_ERROR;
		bio->bio_resid = bio->bio_bcount;
		biodone(bio);
	}

	mtx_destroy(&ns->lock);
	free(ns, M_NVMF);
}

bool
nvmf_update_ns(struct nvmf_namespace *ns,
    const struct nvme_namespace_data *data)
{
	uint8_t lbads, lbaf;

	if (NVMEV(NVME_NS_DATA_DPS_PIT, data->dps) != 0) {
		ns_printf(ns, "End-to-end data protection not supported\n");
		return (false);
	}

	lbaf = NVMEV(NVME_NS_DATA_FLBAS_FORMAT, data->flbas);
	if (lbaf > data->nlbaf) {
		ns_printf(ns, "Invalid LBA format index\n");
		return (false);
	}

	if (NVMEV(NVME_NS_DATA_LBAF_MS, data->lbaf[lbaf]) != 0) {
		ns_printf(ns, "Namespaces with metadata are not supported\n");
		return (false);
	}

	lbads = NVMEV(NVME_NS_DATA_LBAF_LBADS, data->lbaf[lbaf]);
	if (lbads == 0) {
		ns_printf(ns, "Invalid LBA format index\n");
		return (false);
	}

	ns->lba_size = 1 << lbads;
	ns->size = data->nsze * ns->lba_size;
	return (true);
}
