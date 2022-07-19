/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "qat_freebsd.h"
#include "adf_common_drv.h"
#include "adf_cfg_device.h"
#include "adf_cfg_dev_dbg.h"
#include <sys/bus.h>
#include <sys/libkern.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/sbuf.h>
#include <sys/sx.h>
#include <sys/systm.h>
#include <sys/malloc.h>

static int qat_dev_cfg_show(SYSCTL_HANDLER_ARGS)
{
	struct adf_cfg_device_data *dev_cfg;
	struct adf_cfg_section *sec;
	struct adf_cfg_key_val *ptr;
	struct sbuf sb;
	int error;

	sbuf_new_for_sysctl(&sb, NULL, 128, req);
	dev_cfg = arg1;
	sx_slock(&dev_cfg->lock);
	list_for_each_entry(sec, &dev_cfg->sec_list, list)
	{
		sbuf_printf(&sb, "[%s]\n", sec->name);
		list_for_each_entry(ptr, &sec->param_head, list)
		{
			sbuf_printf(&sb, "%s = %s\n", ptr->key, ptr->val);
		}
	}
	sx_sunlock(&dev_cfg->lock);
	error = sbuf_finish(&sb);
	sbuf_delete(&sb);
	return error;
}

int
adf_cfg_dev_dbg_add(struct adf_accel_dev *accel_dev)
{
	struct adf_cfg_device_data *dev_cfg_data = accel_dev->cfg;
	device_t dev;

	dev = GET_DEV(accel_dev);
	dev_cfg_data->debug =
	    SYSCTL_ADD_PROC(device_get_sysctl_ctx(dev),
			    SYSCTL_CHILDREN(device_get_sysctl_tree(dev)),
			    OID_AUTO,
			    "dev_cfg",
			    CTLFLAG_RD | CTLTYPE_STRING,
			    dev_cfg_data,
			    0,
			    qat_dev_cfg_show,
			    "A",
			    "Device configuration");

	if (!dev_cfg_data->debug) {
		device_printf(dev, "Failed to create qat cfg sysctl.\n");
		return ENXIO;
	}
	return 0;
}

void
adf_cfg_dev_dbg_remove(struct adf_accel_dev *accel_dev)
{
	struct adf_cfg_device_data *dev_cfg_data = accel_dev->cfg;

	if (dev_cfg_data->dev) {
		adf_cfg_device_clear(dev_cfg_data->dev, accel_dev);
		free(dev_cfg_data->dev, M_QAT);
		dev_cfg_data->dev = NULL;
	}
}
