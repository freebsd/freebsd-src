/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (C) 2012-2014 Intel Corporation
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

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/module.h>

#include <vm/uma.h>

#include "nvme_private.h"
#include "nvme_if.h"

int32_t		nvme_retry_count;

MALLOC_DEFINE(M_NVME, "nvme", "nvme(4) memory allocations");

int
nvme_shutdown(device_t dev)
{
	struct nvme_controller	*ctrlr;

	ctrlr = DEVICE2SOFTC(dev);
	nvme_ctrlr_shutdown(ctrlr);

	return (0);
}

int
nvme_attach(device_t dev)
{
	struct nvme_controller	*ctrlr = DEVICE2SOFTC(dev);
	int			status;

	status = nvme_ctrlr_construct(ctrlr, dev);
	if (status != 0) {
		nvme_ctrlr_destruct(ctrlr, dev);
		return (status);
	}

	ctrlr->config_hook.ich_func = nvme_ctrlr_start_config_hook;
	ctrlr->config_hook.ich_arg = ctrlr;

	if (config_intrhook_establish(&ctrlr->config_hook) != 0)
		return (ENOMEM);

	return (0);
}

int
nvme_detach(device_t dev)
{
	struct nvme_controller	*ctrlr = DEVICE2SOFTC(dev);
	int error;

	config_intrhook_drain(&ctrlr->config_hook);

	error = bus_generic_detach(dev);
	if (error)
		return (error);

	nvme_ctrlr_destruct(ctrlr, dev);
	return (0);
}

void
nvme_notify_async(struct nvme_controller *ctrlr,
    const struct nvme_completion *async_cpl,
    uint32_t log_page_id, void *log_page_buffer, uint32_t log_page_size)
{
	device_t *children;
	int n_children;

	if (device_get_children(ctrlr->dev, &children, &n_children) != 0)
		return;

	for (int i = 0; i < n_children; i++)
		NVME_HANDLE_AEN(children[i], async_cpl, log_page_id,
		    log_page_buffer, log_page_size);

	free(children, M_TEMP);
}

void
nvme_notify_fail(struct nvme_controller *ctrlr)
{
	device_t *children;
	int n_children;

	/*
	 * This controller failed during initialization (i.e. IDENTIFY
	 *  command failed or timed out).  Do not notify any nvme
	 *  consumers of the failure here, since the consumer does not
	 *  even know about the controller yet.
	 */
	if (!ctrlr->is_initialized)
		return;

	if (device_get_children(ctrlr->dev, &children, &n_children) != 0)
		return;

	for (int i = 0; i < n_children; i++)
		NVME_CONTROLLER_FAILED(children[i]);

	free(children, M_TEMP);
}

void
nvme_completion_poll_cb(void *arg, const struct nvme_completion *cpl)
{
	struct nvme_completion_poll_status	*status = arg;

	/*
	 * Copy status into the argument passed by the caller, so that
	 *  the caller can check the status to determine if the
	 *  the request passed or failed.
	 */
	memcpy(&status->cpl, cpl, sizeof(*cpl));
	atomic_store_rel_int(&status->done, 1);
}

static int
nvme_modevent(module_t mod __unused, int type __unused, void *argp __unused)
{
       return (0);
}

static moduledata_t nvme_mod = {
       "nvme",
       nvme_modevent,
       0
};

DECLARE_MODULE(nvme, nvme_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(nvme, 1);
MODULE_DEPEND(nvme, cam, 1, 1, 1);
