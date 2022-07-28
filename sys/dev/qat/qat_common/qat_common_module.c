/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "qat_freebsd.h"
#include "adf_common_drv.h"

static int __init
qat_common_register(void)
{
	if (adf_init_aer())
		return EFAULT;

	if (adf_init_fatal_error_wq())
		return EFAULT;

	return 0;
}

static void __exit
qat_common_unregister(void)
{
	adf_exit_vf_wq();
	adf_exit_aer();
	adf_exit_fatal_error_wq();
	adf_clean_vf_map(false);
}

static int
qat_common_modevent(module_t mod, int type, void *data)
{
	switch (type) {
	case MOD_LOAD:
		return qat_common_register();
	case MOD_UNLOAD:
		qat_common_unregister();
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static moduledata_t qat_common_mod = { "qat_common", qat_common_modevent, 0 };

DECLARE_MODULE(qat_common, qat_common_mod, SI_SUB_DRIVERS, SI_ORDER_FIRST);
MODULE_VERSION(qat_common, 1);
MODULE_DEPEND(qat_common, firmware, 1, 1, 1);
MODULE_DEPEND(qat_common, linuxkpi, 1, 1, 1);
