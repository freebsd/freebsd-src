/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "adf_cfg.h"
#include "cpa.h"
#include "icp_accel_devices.h"
#include "adf_common_drv.h"
#include "icp_adf_debug.h"
#include "icp_adf_init.h"
#include "lac_sal_ctrl.h"

extern struct mtx *adfDevicesLock;

static int
adf_module_load(void)
{
	CpaStatus ret = CPA_STATUS_SUCCESS;

	qatUtilsMutexInit(&adfDevicesLock);
	ret = SalCtrl_AdfServicesRegister();
	if (ret != CPA_STATUS_SUCCESS) {
		qatUtilsMutexDestroy(&adfDevicesLock);
		return EFAULT;
	}

	return 0;
}

static int
adf_module_unload(void)
{
	CpaStatus ret = CPA_STATUS_SUCCESS;

	ret = SalCtrl_AdfServicesUnregister();
	if (ret != CPA_STATUS_SUCCESS) {
		return EBUSY;
	}
	qatUtilsMutexDestroy(&adfDevicesLock);

	return 0;
}

static int
adf_modevent(module_t mod, int type, void *arg)
{
	int error;

	switch (type) {
	case MOD_LOAD:
		error = adf_module_load();
		break;
	case MOD_UNLOAD:
		error = adf_module_unload();
		break;
	default:
		error = EOPNOTSUPP;
		break;
	}

	return (error);
}

static moduledata_t adf_mod = { "qat_api", adf_modevent, 0 };

DECLARE_MODULE(qat_api, adf_mod, SI_SUB_DRIVERS, SI_ORDER_SECOND);
MODULE_VERSION(qat_api, 1);
MODULE_DEPEND(qat_api, qat_common, 1, 1, 1);
MODULE_DEPEND(qat_api, linuxkpi, 1, 1, 1);
