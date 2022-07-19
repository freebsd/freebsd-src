/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "adf_c4xxx_hw_data.h"
#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/io.h>
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <adf_accel_devices.h>
#include <adf_common_drv.h>
#include <adf_cfg.h>

/* String buffer size */
#define AE_INFO_BUFFER_SIZE 50

#define AE_CONFIG_DBG_FILE "ae_config"

static u8
find_first_me_index(const u32 au_mask)
{
	u8 i;
	u32 mask = au_mask;

	/* Retrieve the index of the first ME of an accel unit */
	for (i = 0; i < ADF_C4XXX_MAX_ACCELENGINES; i++) {
		if (mask & BIT(i))
			return i;
	}

	return 0;
}

static u8
get_au_index(u8 au_mask)
{
	u8 au_index = 0;

	while (au_mask) {
		if (au_mask == BIT(0))
			return au_index;
		au_index++;
		au_mask = au_mask >> 1;
	}

	return 0;
}

static int adf_ae_config_show(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;
	struct adf_accel_dev *accel_dev = arg1;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_accel_unit *accel_unit = accel_dev->au_info->au;
	u8 i, j;
	u8 au_index;
	u8 ae_index;
	u8 num_aes;
	int ret = 0;
	u32 num_au = hw_data->get_num_accel_units(hw_data);

	sbuf_new_for_sysctl(&sb, NULL, 2048, req);

	sbuf_printf(&sb, "\n");
	for (i = 0; i < num_au; i++) {
		/* Retrieve accel unit index */
		au_index = get_au_index(accel_unit[i].au_mask);

		/* Retrieve index of fist ME in current accel unit */
		ae_index = find_first_me_index(accel_unit[i].ae_mask);
		num_aes = accel_unit[i].num_ae;

		/* Retrieve accel unit type */
		switch (accel_unit[i].services) {
		case ADF_ACCEL_CRYPTO:
			sbuf_printf(&sb,
				    "\tAccel unit %d - CRYPTO\n",
				    au_index);
			/* Display ME assignment for a particular accel unit */
			for (j = ae_index; j < (num_aes + ae_index); j++)
				sbuf_printf(&sb, "\t\tAE[%d]: crypto\n", j);
			break;
		case ADF_ACCEL_COMPRESSION:
			sbuf_printf(&sb,
				    "\tAccel unit %d - COMPRESSION\n",
				    au_index);
			/* Display ME assignment for a particular accel unit */
			for (j = ae_index; j < (num_aes + ae_index); j++)
				sbuf_printf(&sb,
					    "\t\tAE[%d]: compression\n",
					    j);
			break;
		case ADF_ACCEL_SERVICE_NULL:
		default:
			break;
		}
	}

	sbuf_finish(&sb);
	ret = SYSCTL_OUT(req, sbuf_data(&sb), sbuf_len(&sb));
	sbuf_delete(&sb);

	return ret;
}

static int
c4xxx_add_debugfs_ae_config(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx = NULL;
	struct sysctl_oid *qat_sysctl_tree = NULL;
	struct sysctl_oid *ae_conf_ctl = NULL;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);

	ae_conf_ctl = SYSCTL_ADD_PROC(qat_sysctl_ctx,
				      SYSCTL_CHILDREN(qat_sysctl_tree),
				      OID_AUTO,
				      AE_CONFIG_DBG_FILE,
				      CTLTYPE_STRING | CTLFLAG_RD,
				      accel_dev,
				      0,
				      adf_ae_config_show,
				      "A",
				      "AE config");
	accel_dev->debugfs_ae_config = ae_conf_ctl;
	if (!accel_dev->debugfs_ae_config) {
		device_printf(GET_DEV(accel_dev),
			      "Could not create debug ae config entry.\n");
		return EFAULT;
	}
	return 0;
}

int
c4xxx_init_ae_config(struct adf_accel_dev *accel_dev)
{
	int ret = 0;

	/* Add a new file in debug file system with h/w version. */
	ret = c4xxx_add_debugfs_ae_config(accel_dev);
	if (ret) {
		c4xxx_exit_ae_config(accel_dev);
		device_printf(GET_DEV(accel_dev),
			      "Could not create debugfs ae config file\n");
		return EINVAL;
	}

	return 0;
}

void
c4xxx_exit_ae_config(struct adf_accel_dev *accel_dev)
{
	if (!accel_dev->debugfs_ae_config)
		return;

	/* Delete ae configuration file */
	remove_oid(accel_dev, accel_dev->debugfs_ae_config);

	accel_dev->debugfs_ae_config = NULL;
}
