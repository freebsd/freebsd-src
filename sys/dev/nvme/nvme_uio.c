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
#include <sys/conf.h>
#include <sys/proc.h>
#include <sys/uio.h>

#include "nvme_private.h"

static void
nvme_uio_done(void *arg, const struct nvme_completion *cpl)
{
	struct mtx *mtx;
	struct uio *uio = arg;

	if (!nvme_completion_is_error(cpl))
		uio->uio_resid = 0;

	mtx = mtx_pool_find(mtxpool_sleep, arg);
	mtx_lock(mtx);
	wakeup(arg);
	mtx_unlock(mtx);
}

void
nvme_payload_map_uio(void *arg, bus_dma_segment_t *seg, int nseg,
    bus_size_t mapsize, int error)
{
	struct nvme_tracker	*tr = arg;

	/*
	 * Now that we know the actual size of the uio, divide it by the
	 *  sector size that we stored in cdw12.
	 */
	tr->req->cmd.cdw12 = (mapsize / tr->req->cmd.cdw12)-1;
	nvme_payload_map(arg, seg, nseg, error);
}

static int
nvme_read_uio(struct nvme_namespace *ns, struct uio *uio)
{
	struct nvme_request	*req;
	struct nvme_command	*cmd;
	uint64_t		lba;

	req = nvme_allocate_request_uio(uio, nvme_uio_done, uio);

	if (req == NULL)
		return (ENOMEM);

	cmd = &req->cmd;
	cmd->opc = NVME_OPC_READ;
	cmd->nsid = ns->id;
	lba = uio->uio_offset / nvme_ns_get_sector_size(ns);

	*(uint64_t *)&cmd->cdw10 = lba;
	/*
	 * Store the sector size in cdw12 (where the LBA count normally goes).
	 *  We'll adjust cdw12 in the map_uio callback based on the mapsize
	 *  parameter.  This allows us to not have to store the namespace
	 *  in the request simply to get the sector size in the map_uio
	 *  callback.
	 */
	cmd->cdw12 = nvme_ns_get_sector_size(ns);

	nvme_ctrlr_submit_io_request(ns->ctrlr, req);

	return (0);
}

static int
nvme_write_uio(struct nvme_namespace *ns, struct uio *uio)
{
	struct nvme_request	*req;
	struct nvme_command	*cmd;
	uint64_t		lba;

	req = nvme_allocate_request_uio(uio, nvme_uio_done, uio);

	if (req == NULL)
		return (ENOMEM);

	cmd = &req->cmd;
	cmd->opc = NVME_OPC_WRITE;
	cmd->nsid = ns->id;
	lba = uio->uio_offset / nvme_ns_get_sector_size(ns);

	*(uint64_t *)&cmd->cdw10 = lba;
	/*
	 * Store the sector size in cdw12 (where the LBA count normally goes).
	 *  We'll adjust cdw12 in the map_uio callback based on the mapsize
	 *  parameter.  This allows us to not have to store the namespace
	 *  in the request simply to get the sector size in the map_uio
	 *  callback.
	 */
	cmd->cdw12 = nvme_ns_get_sector_size(ns);

	nvme_ctrlr_submit_io_request(ns->ctrlr, req);

	return (0);
}

int
nvme_ns_physio(struct cdev *dev, struct uio *uio, int ioflag)
{
	struct uio		uio_tmp;
	struct iovec		uio_iov_tmp;
	struct nvme_namespace	*ns;
	struct mtx		*mtx;
	int			i, nvme_err, physio_err = 0;
#if __FreeBSD_version > 900017
	int			ref;
#endif

	PHOLD(curproc);

	ns = dev->si_drv1;
	mtx = mtx_pool_find(mtxpool_sleep, &uio_tmp);

#if __FreeBSD_version > 900017
	dev_refthread(dev, &ref);
#else
	dev_refthread(dev);
#endif

	/*
	 * NVM Express doesn't really support true SGLs.  All SG elements
	 *  must be PAGE_SIZE, except for the first and last element.
	 *  Because of this, we need to break up each iovec into a separate
	 *  NVMe command - otherwise we could end up with sub-PAGE_SIZE
	 *  elements in the middle of an SGL which is not allowed.
	 */
	uio_tmp.uio_iov = &uio_iov_tmp;
	uio_tmp.uio_iovcnt = 1;
	uio_tmp.uio_offset = uio->uio_offset;
	uio_tmp.uio_segflg = uio->uio_segflg;
	uio_tmp.uio_rw = uio->uio_rw;
	uio_tmp.uio_td = uio->uio_td;

	for (i = 0; i < uio->uio_iovcnt; i++) {

		uio_iov_tmp.iov_base = uio->uio_iov[i].iov_base;
		uio_iov_tmp.iov_len = uio->uio_iov[i].iov_len;
		uio_tmp.uio_resid = uio_iov_tmp.iov_len;

		mtx_lock(mtx);

		if (uio->uio_rw == UIO_READ)
			nvme_err = nvme_read_uio(ns, &uio_tmp);
		else
			nvme_err = nvme_write_uio(ns, &uio_tmp);

		if (nvme_err == 0)
			msleep(&uio_tmp, mtx, PRIBIO, "nvme_physio", 0);

		mtx_unlock(mtx);

		if (uio_tmp.uio_resid == 0) {
			uio->uio_resid -= uio_iov_tmp.iov_len;
			uio->uio_offset += uio_iov_tmp.iov_len;
		} else {
			physio_err = EFAULT;
			break;
		}

		uio_tmp.uio_offset += uio_iov_tmp.iov_len;
	}

#if __FreeBSD_version > 900017
	dev_relthread(dev, ref);
#else
	dev_relthread(dev);
#endif

	PRELE(curproc);
	return (physio_err);
}
