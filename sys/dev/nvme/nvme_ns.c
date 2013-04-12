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
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/ioccom.h>
#include <sys/module.h>
#include <sys/proc.h>

#include <dev/pci/pcivar.h>

#include "nvme_private.h"

static int
nvme_ns_ioctl(struct cdev *cdev, u_long cmd, caddr_t arg, int flag,
    struct thread *td)
{
	struct nvme_completion_poll_status	status;
	struct nvme_namespace			*ns;
	struct nvme_controller			*ctrlr;

	ns = cdev->si_drv1;
	ctrlr = ns->ctrlr;

	switch (cmd) {
	case NVME_IDENTIFY_NAMESPACE:
#ifdef CHATHAM2
		/*
		 * Don't refresh data on Chatham, since Chatham returns
		 *  garbage on IDENTIFY anyways.
		 */
		if (pci_get_devid(ctrlr->dev) == CHATHAM_PCI_ID) {
			memcpy(arg, &ns->data, sizeof(ns->data));
			break;
		}
#endif
		/* Refresh data before returning to user. */
		status.done = FALSE;
		nvme_ctrlr_cmd_identify_namespace(ctrlr, ns->id, &ns->data,
		    nvme_completion_poll_cb, &status);
		while (status.done == FALSE)
			DELAY(5);
		if (nvme_completion_is_error(&status.cpl))
			return (ENXIO);
		memcpy(arg, &ns->data, sizeof(ns->data));
		break;
	case NVME_IO_TEST:
	case NVME_BIO_TEST:
		nvme_ns_test(ns, cmd, arg);
		break;
	case DIOCGMEDIASIZE:
		*(off_t *)arg = (off_t)nvme_ns_get_size(ns);
		break;
	case DIOCGSECTORSIZE:
		*(u_int *)arg = nvme_ns_get_sector_size(ns);
		break;
	default:
		return (ENOTTY);
	}

	return (0);
}

static int
nvme_ns_open(struct cdev *dev __unused, int flags, int fmt __unused,
    struct thread *td)
{
	int error = 0;

	if (flags & FWRITE)
		error = securelevel_gt(td->td_ucred, 0);

	return (error);
}

static int
nvme_ns_close(struct cdev *dev __unused, int flags, int fmt __unused,
    struct thread *td)
{

	return (0);
}

static void
nvme_ns_strategy_done(void *arg, const struct nvme_completion *cpl)
{
	struct bio *bp = arg;

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
nvme_ns_strategy(struct bio *bp)
{
	struct nvme_namespace	*ns;
	int			err;

	ns = bp->bio_dev->si_drv1;
	err = nvme_ns_bio_process(ns, bp, nvme_ns_strategy_done);

	if (err) {
		bp->bio_error = err;
		bp->bio_flags |= BIO_ERROR;
		bp->bio_resid = bp->bio_bcount;
		biodone(bp);
	}

}

static struct cdevsw nvme_ns_cdevsw = {
	.d_version =	D_VERSION,
#ifdef NVME_UNMAPPED_BIO_SUPPORT
	.d_flags =	D_DISK | D_UNMAPPED_IO,
	.d_read =	physread,
	.d_write =	physwrite,
#else
	.d_flags =	D_DISK,
	.d_read =	nvme_ns_physio,
	.d_write =	nvme_ns_physio,
#endif
	.d_open =	nvme_ns_open,
	.d_close =	nvme_ns_close,
	.d_strategy =	nvme_ns_strategy,
	.d_ioctl =	nvme_ns_ioctl
};

uint32_t
nvme_ns_get_max_io_xfer_size(struct nvme_namespace *ns)
{
	return ns->ctrlr->max_xfer_size;
}

uint32_t
nvme_ns_get_sector_size(struct nvme_namespace *ns)
{
	return (1 << ns->data.lbaf[0].lbads);
}

uint64_t
nvme_ns_get_num_sectors(struct nvme_namespace *ns)
{
	return (ns->data.nsze);
}

uint64_t
nvme_ns_get_size(struct nvme_namespace *ns)
{
	return (nvme_ns_get_num_sectors(ns) * nvme_ns_get_sector_size(ns));
}

uint32_t
nvme_ns_get_flags(struct nvme_namespace *ns)
{
	return (ns->flags);
}

const char *
nvme_ns_get_serial_number(struct nvme_namespace *ns)
{
	return ((const char *)ns->ctrlr->cdata.sn);
}

const char *
nvme_ns_get_model_number(struct nvme_namespace *ns)
{
	return ((const char *)ns->ctrlr->cdata.mn);
}

const struct nvme_namespace_data *
nvme_ns_get_data(struct nvme_namespace *ns)
{

	return (&ns->data);
}

static void
nvme_ns_bio_done(void *arg, const struct nvme_completion *status)
{
	struct bio	*bp = arg;
	nvme_cb_fn_t	bp_cb_fn;

	bp_cb_fn = bp->bio_driver1;

	if (bp->bio_driver2)
		free(bp->bio_driver2, M_NVME);

	bp_cb_fn(bp, status);
}

int
nvme_ns_bio_process(struct nvme_namespace *ns, struct bio *bp,
	nvme_cb_fn_t cb_fn)
{
	struct nvme_dsm_range	*dsm_range;
	int			err;

	bp->bio_driver1 = cb_fn;

	switch (bp->bio_cmd) {
	case BIO_READ:
		err = nvme_ns_cmd_read_bio(ns, bp, nvme_ns_bio_done, bp);
		break;
	case BIO_WRITE:
		err = nvme_ns_cmd_write_bio(ns, bp, nvme_ns_bio_done, bp);
		break;
	case BIO_FLUSH:
		err = nvme_ns_cmd_flush(ns, nvme_ns_bio_done, bp);
		break;
	case BIO_DELETE:
		dsm_range =
		    malloc(sizeof(struct nvme_dsm_range), M_NVME,
		    M_ZERO | M_WAITOK);
		dsm_range->length =
		    bp->bio_bcount/nvme_ns_get_sector_size(ns);
		dsm_range->starting_lba =
		    bp->bio_offset/nvme_ns_get_sector_size(ns);
		bp->bio_driver2 = dsm_range;
		err = nvme_ns_cmd_deallocate(ns, dsm_range, 1,
			nvme_ns_bio_done, bp);
		if (err != 0)
			free(dsm_range, M_NVME);
		break;
	default:
		err = EIO;
		break;
	}

	return (err);
}

#ifdef CHATHAM2
static void
nvme_ns_populate_chatham_data(struct nvme_namespace *ns)
{
	struct nvme_controller		*ctrlr;
	struct nvme_namespace_data	*nsdata;

	ctrlr = ns->ctrlr;
	nsdata = &ns->data;

	nsdata->nsze = ctrlr->chatham_lbas;
	nsdata->ncap = ctrlr->chatham_lbas;
	nsdata->nuse = ctrlr->chatham_lbas;

	/* Chatham2 doesn't support thin provisioning. */
	nsdata->nsfeat.thin_prov = 0;

	/* Set LBA size to 512 bytes. */
	nsdata->lbaf[0].lbads = 9;
}
#endif /* CHATHAM2 */

int
nvme_ns_construct(struct nvme_namespace *ns, uint16_t id,
    struct nvme_controller *ctrlr)
{
	struct nvme_completion_poll_status	status;

	ns->ctrlr = ctrlr;
	ns->id = id;

	/*
	 * Namespaces are reconstructed after a controller reset, so check
	 *  to make sure we only call mtx_init once on each mtx.
	 *
	 * TODO: Move this somewhere where it gets called at controller
	 *  construction time, which is not invoked as part of each
	 *  controller reset.
	 */
	if (!mtx_initialized(&ns->lock))
		mtx_init(&ns->lock, "nvme ns lock", NULL, MTX_DEF);

#ifdef CHATHAM2
	if (pci_get_devid(ctrlr->dev) == CHATHAM_PCI_ID)
		nvme_ns_populate_chatham_data(ns);
	else {
#endif
		status.done = FALSE;
		nvme_ctrlr_cmd_identify_namespace(ctrlr, id, &ns->data,
		    nvme_completion_poll_cb, &status);
		while (status.done == FALSE)
			DELAY(5);
		if (nvme_completion_is_error(&status.cpl)) {
			nvme_printf(ctrlr, "nvme_identify_namespace failed\n");
			return (ENXIO);
		}
#ifdef CHATHAM2
	}
#endif

	if (ctrlr->cdata.oncs.dsm)
		ns->flags |= NVME_NS_DEALLOCATE_SUPPORTED;

	if (ctrlr->cdata.vwc.present)
		ns->flags |= NVME_NS_FLUSH_SUPPORTED;

	/*
	 * cdev may have already been created, if we are reconstructing the
	 *  namespace after a controller-level reset.
	 */
	if (ns->cdev != NULL)
		return (0);

/*
 * MAKEDEV_ETERNAL was added in r210923, for cdevs that will never
 *  be destroyed.  This avoids refcounting on the cdev object.
 *  That should be OK case here, as long as we're not supporting PCIe
 *  surprise removal nor namespace deletion.
 */
#ifdef MAKEDEV_ETERNAL_KLD
	ns->cdev = make_dev_credf(MAKEDEV_ETERNAL_KLD, &nvme_ns_cdevsw, 0,
	    NULL, UID_ROOT, GID_WHEEL, 0600, "nvme%dns%d",
	    device_get_unit(ctrlr->dev), ns->id);
#else
	ns->cdev = make_dev_credf(0, &nvme_ns_cdevsw, 0,
	    NULL, UID_ROOT, GID_WHEEL, 0600, "nvme%dns%d",
	    device_get_unit(ctrlr->dev), ns->id);
#endif

	if (ns->cdev != NULL)
		ns->cdev->si_drv1 = ns;

	return (0);
}

void nvme_ns_destruct(struct nvme_namespace *ns)
{

	if (ns->cdev != NULL)
		destroy_dev(ns->cdev);
}
