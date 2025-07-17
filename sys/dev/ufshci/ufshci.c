/*-
 * Copyright (c) 2025, Samsung Electronics Co., Ltd.
 * Written by Jaeyoon Choi
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <sys/param.h>
#include <sys/bus.h>
#include <sys/conf.h>
#include <sys/module.h>

#include "ufshci_private.h"

MALLOC_DEFINE(M_UFSHCI, "ufshci", "ufshci(4) memory allocations");

int
ufshci_attach(device_t dev)
{
	struct ufshci_controller *ctrlr = device_get_softc(dev);
	int status;

	status = ufshci_ctrlr_construct(ctrlr, dev);
	if (status != 0) {
		ufshci_ctrlr_destruct(ctrlr, dev);
		return (status);
	}

	ctrlr->config_hook.ich_func = ufshci_ctrlr_start_config_hook;
	ctrlr->config_hook.ich_arg = ctrlr;

	if (config_intrhook_establish(&ctrlr->config_hook) != 0)
		return (ENOMEM);

	return (0);
}

int
ufshci_detach(device_t dev)
{
	struct ufshci_controller *ctrlr = device_get_softc(dev);

	config_intrhook_drain(&ctrlr->config_hook);

	ufshci_ctrlr_destruct(ctrlr, dev);

	return (0);
}

void
ufshci_completion_poll_cb(void *arg, const struct ufshci_completion *cpl,
    bool error)
{
	struct ufshci_completion_poll_status *status = arg;

	/*
	 * Copy status into the argument passed by the caller, so that the
	 * caller can check the status to determine if the the request passed
	 * or failed.
	 */
	memcpy(&status->cpl.response_upiu, &cpl->response_upiu, cpl->size);
	status->error = error;
	atomic_store_rel_int(&status->done, 1);
}

static int
ufshci_modevent(module_t mod __unused, int type __unused, void *argp __unused)
{
	return (0);
}

static moduledata_t ufshci_mod = { "ufshci", ufshci_modevent, 0 };

DECLARE_MODULE(ufshci, ufshci_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(ufshci, 1);
MODULE_DEPEND(ufshci, cam, 1, 1, 1);
