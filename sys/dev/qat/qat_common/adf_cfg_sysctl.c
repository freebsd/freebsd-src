/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include "adf_accel_devices.h"
#include "adf_cfg.h"
#include "adf_cfg_sysctl.h"
#include "adf_cfg_device.h"
#include "adf_common_drv.h"
#include <sys/mutex.h>
#include <sys/sbuf.h>
#include <sys/priv.h>

#define ADF_CFG_SYSCTL_BUF_SZ ADF_CFG_MAX_VAL
#define ADF_CFG_UP_STR "up"
#define ADF_CFG_DOWN_STR "down"

#define ADF_CFG_MAX_USER_PROCESSES 64

static int
adf_cfg_down(struct adf_accel_dev *accel_dev)
{
	int ret = 0;

	if (!adf_dev_started(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Device qat_dev%d already down\n",
			      accel_dev->accel_id);
		return 0;
	}

	if (adf_dev_in_use(accel_dev)) {
		pr_err("QAT: Device %d in use\n", accel_dev->accel_id);
		goto out;
	}

	if (adf_dev_stop(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to stop qat_dev%d\n",
			      accel_dev->accel_id);
		ret = EFAULT;
		goto out;
	}

	adf_dev_shutdown(accel_dev);

out:
	return ret;
}

static int
adf_cfg_up(struct adf_accel_dev *accel_dev)
{
	int ret;

	if (adf_dev_started(accel_dev))
		return 0;

	if (NULL == accel_dev->hw_device->config_device)
		return ENXIO;

	ret = accel_dev->hw_device->config_device(accel_dev);
	if (ret) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to start qat_dev%d\n",
			      accel_dev->accel_id);
		return ret;
	}

	ret = adf_dev_init(accel_dev);
	if (!ret)
		ret = adf_dev_start(accel_dev);

	if (ret) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to start qat_dev%d\n",
			      accel_dev->accel_id);
		adf_dev_stop(accel_dev);
		adf_dev_shutdown(accel_dev);
	}

	if (!ret) {
		struct adf_cfg_device *cfg_dev = NULL;

		cfg_dev = accel_dev->cfg->dev;
		adf_cfg_device_clear(cfg_dev, accel_dev);
		free(cfg_dev, M_QAT);
		accel_dev->cfg->dev = NULL;
	}

	return 0;
}

static const char *const cfg_serv[] =
    { "sym;asym", "sym", "asym", "dc", "sym;dc", "asym;dc", "cy", "cy;dc" };

static const char *const cfg_mode[] = { "ks;us", "us", "ks" };

static int adf_cfg_sysctl_services_handle(SYSCTL_HANDLER_ARGS)
{
	struct adf_cfg_device_data *dev_cfg_data;
	struct adf_accel_dev *accel_dev;
	char buf[ADF_CFG_SYSCTL_BUF_SZ];
	unsigned int len;
	int ret = 0;
	int i = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	accel_dev = arg1;
	if (!accel_dev)
		return ENXIO;

	dev_cfg_data = accel_dev->cfg;
	if (!dev_cfg_data)
		return ENXIO;

	strlcpy(buf, dev_cfg_data->cfg_services, sizeof(buf));

	ret = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (ret != 0 || req->newptr == NULL)
		return ret;

	/* Handle config change */
	if (adf_dev_started(accel_dev)) {
		device_printf(
		    GET_DEV(accel_dev),
		    "QAT: configuration could be changed in down state only\n");
		return EINVAL;
	}

	len = strlen(buf);

	for (i = 0; i < ARRAY_SIZE(cfg_serv); i++) {
		if ((len > 0 && strncasecmp(cfg_serv[i], buf, len) == 0)) {
			strlcpy(dev_cfg_data->cfg_services,
				buf,
				ADF_CFG_MAX_VAL);
			break;
		}
	}

	if (i == ARRAY_SIZE(cfg_serv)) {
		device_printf(GET_DEV(accel_dev),
			      "Unknown service configuration\n");
		ret = EINVAL;
	}

	return ret;
}

static int adf_cfg_sysctl_mode_handle(SYSCTL_HANDLER_ARGS)
{
	struct adf_cfg_device_data *dev_cfg_data;
	struct adf_accel_dev *accel_dev;
	char buf[ADF_CFG_SYSCTL_BUF_SZ];
	unsigned int len;
	int ret = 0;
	int i = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	accel_dev = arg1;
	if (!accel_dev)
		return ENXIO;

	dev_cfg_data = accel_dev->cfg;
	if (!dev_cfg_data)
		return ENXIO;

	strlcpy(buf, dev_cfg_data->cfg_mode, sizeof(buf));

	ret = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (ret != 0 || req->newptr == NULL)
		return ret;

	/* Handle config change */
	if (adf_dev_started(accel_dev)) {
		device_printf(
		    GET_DEV(accel_dev),
		    "QAT: configuration could be changed in down state only\n");
		return EBUSY;
	}

	len = strlen(buf);

	for (i = 0; i < ARRAY_SIZE(cfg_mode); i++) {
		if ((len > 0 && strncasecmp(cfg_mode[i], buf, len) == 0)) {
			strlcpy(dev_cfg_data->cfg_mode, buf, ADF_CFG_MAX_VAL);
			break;
		}
	}

	if (i == ARRAY_SIZE(cfg_mode)) {
		device_printf(GET_DEV(accel_dev),
			      "Unknown configuration mode\n");
		ret = EINVAL;
	}

	return ret;
}

static int adf_cfg_sysctl_handle(SYSCTL_HANDLER_ARGS)
{
	struct adf_cfg_device_data *dev_cfg_data;
	struct adf_accel_dev *accel_dev;
	char buf[ADF_CFG_SYSCTL_BUF_SZ] = { 0 };
	unsigned int len;
	int ret = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	accel_dev = arg1;
	if (!accel_dev)
		return ENXIO;

	dev_cfg_data = accel_dev->cfg;
	if (!dev_cfg_data)
		return ENXIO;

	if (adf_dev_started(accel_dev)) {
		strlcpy(buf, ADF_CFG_UP_STR, sizeof(buf));
	} else {
		strlcpy(buf, ADF_CFG_DOWN_STR, sizeof(buf));
	}

	ret = sysctl_handle_string(oidp, buf, sizeof(buf), req);
	if (ret != 0 || req->newptr == NULL)
		return ret;

	len = strlen(buf);

	if ((len > 0 && strncasecmp(ADF_CFG_UP_STR, buf, len) == 0)) {
		ret = adf_cfg_up(accel_dev);

	} else if (len > 0 && strncasecmp(ADF_CFG_DOWN_STR, buf, len) == 0) {
		ret = adf_cfg_down(accel_dev);

	} else {
		device_printf(GET_DEV(accel_dev), "QAT: Invalid operation\n");
		ret = EINVAL;
	}

	return ret;
}

static int adf_cfg_sysctl_num_processes_handle(SYSCTL_HANDLER_ARGS)
{
	struct adf_cfg_device_data *dev_cfg_data;
	struct adf_accel_dev *accel_dev;
	uint32_t num_user_processes = 0;
	int ret = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	accel_dev = arg1;
	if (!accel_dev)
		return ENXIO;

	dev_cfg_data = accel_dev->cfg;
	if (!dev_cfg_data)
		return ENXIO;

	num_user_processes = dev_cfg_data->num_user_processes;

	ret = sysctl_handle_int(oidp, &num_user_processes, 0, req);
	if (ret != 0 || req->newptr == NULL)
		return ret;

	if (adf_dev_started(accel_dev)) {
		device_printf(
		    GET_DEV(accel_dev),
		    "QAT: configuration could be changed in down state only\n");
		return EBUSY;
	}

	if (num_user_processes > ADF_CFG_MAX_USER_PROCESSES) {
		return EINVAL;
	}

	dev_cfg_data->num_user_processes = num_user_processes;

	return ret;
}

int
adf_cfg_sysctl_add(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx;
	struct sysctl_oid *qat_sysctl_tree;

	if (!accel_dev)
		return EINVAL;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);

	SYSCTL_ADD_PROC(qat_sysctl_ctx,
			SYSCTL_CHILDREN(qat_sysctl_tree),
			OID_AUTO,
			"state",
			CTLTYPE_STRING | CTLFLAG_RW | CTLFLAG_NEEDGIANT,
			accel_dev,
			0,
			adf_cfg_sysctl_handle,
			"A",
			"QAT State");

	SYSCTL_ADD_PROC(qat_sysctl_ctx,
			SYSCTL_CHILDREN(qat_sysctl_tree),
			OID_AUTO,
			"cfg_services",
			CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_NEEDGIANT,
			accel_dev,
			0,
			adf_cfg_sysctl_services_handle,
			"A",
			"QAT services confguration");

	SYSCTL_ADD_PROC(qat_sysctl_ctx,
			SYSCTL_CHILDREN(qat_sysctl_tree),
			OID_AUTO,
			"cfg_mode",
			CTLTYPE_STRING | CTLFLAG_RWTUN | CTLFLAG_NEEDGIANT,
			accel_dev,
			0,
			adf_cfg_sysctl_mode_handle,
			"A",
			"QAT mode configuration");

	SYSCTL_ADD_PROC(qat_sysctl_ctx,
			SYSCTL_CHILDREN(qat_sysctl_tree),
			OID_AUTO,
			"num_user_processes",
			CTLTYPE_U32 | CTLFLAG_RWTUN | CTLFLAG_NEEDGIANT,
			accel_dev,
			0,
			adf_cfg_sysctl_num_processes_handle,
			"I",
			"QAT user processes number ");

	return 0;
}

void
adf_cfg_sysctl_remove(struct adf_accel_dev *accel_dev)
{
}
