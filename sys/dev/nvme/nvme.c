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

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/module.h>

#include <vm/uma.h>

#include "nvme_private.h"

struct nvme_consumer {
	uint32_t		id;
	nvme_cons_ns_fn_t	ns_fn;
	nvme_cons_ctrlr_fn_t	ctrlr_fn;
	nvme_cons_async_fn_t	async_fn;
	nvme_cons_fail_fn_t	fail_fn;
};

struct nvme_consumer nvme_consumer[NVME_MAX_CONSUMERS];
#define	INVALID_CONSUMER_ID	0xFFFF

int32_t		nvme_retry_count;

MALLOC_DEFINE(M_NVME, "nvme", "nvme(4) memory allocations");

static void
nvme_init(void)
{
	uint32_t	i;

	for (i = 0; i < NVME_MAX_CONSUMERS; i++)
		nvme_consumer[i].id = INVALID_CONSUMER_ID;
}

SYSINIT(nvme_register, SI_SUB_DRIVERS, SI_ORDER_SECOND, nvme_init, NULL);

static void
nvme_uninit(void)
{
}

SYSUNINIT(nvme_unregister, SI_SUB_DRIVERS, SI_ORDER_SECOND, nvme_uninit, NULL);

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

	config_intrhook_drain(&ctrlr->config_hook);

	nvme_ctrlr_destruct(ctrlr, dev);
	return (0);
}

static void
nvme_notify(struct nvme_consumer *cons,
	    struct nvme_controller *ctrlr)
{
	struct nvme_namespace	*ns;
	void			*ctrlr_cookie;
	int			cmpset, ns_idx;

	/*
	 * The consumer may register itself after the nvme devices
	 *  have registered with the kernel, but before the
	 *  driver has completed initialization.  In that case,
	 *  return here, and when initialization completes, the
	 *  controller will make sure the consumer gets notified.
	 */
	if (!ctrlr->is_initialized)
		return;

	cmpset = atomic_cmpset_32(&ctrlr->notification_sent, 0, 1);
	if (cmpset == 0)
		return;

	if (cons->ctrlr_fn != NULL)
		ctrlr_cookie = (*cons->ctrlr_fn)(ctrlr);
	else
		ctrlr_cookie = (void *)(uintptr_t)0xdeadc0dedeadc0de;
	ctrlr->cons_cookie[cons->id] = ctrlr_cookie;

	/* ctrlr_fn has failed.  Nothing to notify here any more. */
	if (ctrlr_cookie == NULL) {
		(void)atomic_cmpset_32(&ctrlr->notification_sent, 1, 0);
		return;
	}

	if (ctrlr->is_failed) {
		ctrlr->cons_cookie[cons->id] = NULL;
		if (cons->fail_fn != NULL)
			(*cons->fail_fn)(ctrlr_cookie);
		/*
		 * Do not notify consumers about the namespaces of a
		 *  failed controller.
		 */
		return;
	}
	for (ns_idx = 0; ns_idx < min(ctrlr->cdata.nn, NVME_MAX_NAMESPACES); ns_idx++) {
		ns = &ctrlr->ns[ns_idx];
		if (ns->data.nsze == 0)
			continue;
		if (cons->ns_fn != NULL)
			ns->cons_cookie[cons->id] =
			    (*cons->ns_fn)(ns, ctrlr_cookie);
	}
}

void
nvme_notify_new_controller(struct nvme_controller *ctrlr)
{
	int i;

	for (i = 0; i < NVME_MAX_CONSUMERS; i++) {
		if (nvme_consumer[i].id != INVALID_CONSUMER_ID) {
			nvme_notify(&nvme_consumer[i], ctrlr);
		}
	}
}

static void
nvme_notify_new_consumer(struct nvme_consumer *cons)
{
	device_t		*devlist;
	struct nvme_controller	*ctrlr;
	int			dev_idx, devcount;

	if (devclass_get_devices(devclass_find("nvme"), &devlist, &devcount))
		return;

	for (dev_idx = 0; dev_idx < devcount; dev_idx++) {
		ctrlr = DEVICE2SOFTC(devlist[dev_idx]);
		nvme_notify(cons, ctrlr);
	}

	free(devlist, M_TEMP);
}

void
nvme_notify_async_consumers(struct nvme_controller *ctrlr,
			    const struct nvme_completion *async_cpl,
			    uint32_t log_page_id, void *log_page_buffer,
			    uint32_t log_page_size)
{
	struct nvme_consumer	*cons;
	void			*ctrlr_cookie;
	uint32_t		i;

	for (i = 0; i < NVME_MAX_CONSUMERS; i++) {
		cons = &nvme_consumer[i];
		if (cons->id != INVALID_CONSUMER_ID && cons->async_fn != NULL &&
		    (ctrlr_cookie = ctrlr->cons_cookie[i]) != NULL) {
			(*cons->async_fn)(ctrlr_cookie, async_cpl,
			    log_page_id, log_page_buffer, log_page_size);
		}
	}
}

void
nvme_notify_fail_consumers(struct nvme_controller *ctrlr)
{
	struct nvme_consumer	*cons;
	void			*ctrlr_cookie;
	uint32_t		i;

	/*
	 * This controller failed during initialization (i.e. IDENTIFY
	 *  command failed or timed out).  Do not notify any nvme
	 *  consumers of the failure here, since the consumer does not
	 *  even know about the controller yet.
	 */
	if (!ctrlr->is_initialized)
		return;

	for (i = 0; i < NVME_MAX_CONSUMERS; i++) {
		cons = &nvme_consumer[i];
		if (cons->id != INVALID_CONSUMER_ID &&
		    (ctrlr_cookie = ctrlr->cons_cookie[i]) != NULL) {
			ctrlr->cons_cookie[i] = NULL;
			if (cons->fail_fn != NULL)
				cons->fail_fn(ctrlr_cookie);
		}
	}
}

void
nvme_notify_ns(struct nvme_controller *ctrlr, int nsid)
{
	struct nvme_consumer	*cons;
	struct nvme_namespace	*ns;
	void			*ctrlr_cookie;
	uint32_t		i;

	KASSERT(nsid <= NVME_MAX_NAMESPACES,
	    ("%s: Namespace notification to nsid %d exceeds range\n",
		device_get_nameunit(ctrlr->dev), nsid));

	if (!ctrlr->is_initialized)
		return;

	ns = &ctrlr->ns[nsid - 1];
	for (i = 0; i < NVME_MAX_CONSUMERS; i++) {
		cons = &nvme_consumer[i];
		if (cons->id != INVALID_CONSUMER_ID && cons->ns_fn != NULL &&
		    (ctrlr_cookie = ctrlr->cons_cookie[i]) != NULL)
			ns->cons_cookie[i] = (*cons->ns_fn)(ns, ctrlr_cookie);
	}
}

struct nvme_consumer *
nvme_register_consumer(nvme_cons_ns_fn_t ns_fn, nvme_cons_ctrlr_fn_t ctrlr_fn,
		       nvme_cons_async_fn_t async_fn,
		       nvme_cons_fail_fn_t fail_fn)
{
	int i;

	/*
	 * TODO: add locking around consumer registration.
	 */
	for (i = 0; i < NVME_MAX_CONSUMERS; i++)
		if (nvme_consumer[i].id == INVALID_CONSUMER_ID) {
			nvme_consumer[i].id = i;
			nvme_consumer[i].ns_fn = ns_fn;
			nvme_consumer[i].ctrlr_fn = ctrlr_fn;
			nvme_consumer[i].async_fn = async_fn;
			nvme_consumer[i].fail_fn = fail_fn;

			nvme_notify_new_consumer(&nvme_consumer[i]);
			return (&nvme_consumer[i]);
		}

	printf("nvme(4): consumer not registered - no slots available\n");
	return (NULL);
}

void
nvme_unregister_consumer(struct nvme_consumer *consumer)
{

	consumer->id = INVALID_CONSUMER_ID;
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
