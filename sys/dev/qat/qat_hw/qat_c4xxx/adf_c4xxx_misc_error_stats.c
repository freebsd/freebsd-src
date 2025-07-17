/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include "adf_c4xxx_hw_data.h"
#include "adf_c4xxx_misc_error_stats.h"
#include "adf_common_drv.h"
#include "adf_cfg_common.h"
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/priv.h>

#define MISC_ERROR_DBG_FILE "misc_error_stats"
#define LINE                                                                   \
	"+-----------------------------------------------------------------+\n"
#define BANNER                                                                 \
	"|          Miscellaneous Error Statistics for Qat Device          |\n"

static void *misc_counter;

struct adf_dev_miscellaneous_stats {
	u64 misc_counter;
};

static int qat_misc_error_show(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	sbuf_new_for_sysctl(&sb, NULL, 256, req);
	sbuf_printf(&sb, "\n");
	sbuf_printf(&sb, LINE);
	sbuf_printf(&sb,
		    "| Miscellaneous Error:   %40llu |\n",
		    (unsigned long long)((struct adf_dev_miscellaneous_stats *)
					     misc_counter)
			->misc_counter);

	sbuf_finish(&sb);
	SYSCTL_OUT(req, sbuf_data(&sb), sbuf_len(&sb));
	sbuf_delete(&sb);

	return 0;
}

/**
 * adf_misc_error_add_c4xxx() - Create debugfs entry for
 * acceleration device Freq counters.
 * @accel_dev:  Pointer to acceleration device.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_misc_error_add_c4xxx(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx = NULL;
	struct sysctl_oid *qat_sysctl_tree = NULL;
	struct sysctl_oid *misc_er_file = NULL;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);

	misc_er_file = SYSCTL_ADD_PROC(qat_sysctl_ctx,
				       SYSCTL_CHILDREN(qat_sysctl_tree),
				       OID_AUTO,
				       MISC_ERROR_DBG_FILE,
				       CTLTYPE_STRING | CTLFLAG_RD,
				       accel_dev,
				       0,
				       qat_misc_error_show,
				       "A",
				       "QAT Miscellaneous Error Statistics");
	accel_dev->misc_error_dbgfile = misc_er_file;
	if (!accel_dev->misc_error_dbgfile) {
		device_printf(
		    GET_DEV(accel_dev),
		    "Failed to create qat miscellaneous error debugfs entry.\n");
		return ENOENT;
	}

	misc_counter = kmalloc(PAGE_SIZE, GFP_KERNEL);
	if (!misc_counter)
		return ENOMEM;

	memset(misc_counter, 0, PAGE_SIZE);

	return 0;
}

/**
 * adf_misc_error_remove_c4xxx() - Remove debugfs entry for
 * acceleration device misc error counter.
 * @accel_dev:  Pointer to acceleration device.
 *
 * Return: void
 */
void
adf_misc_error_remove_c4xxx(struct adf_accel_dev *accel_dev)
{
	if (accel_dev->misc_error_dbgfile) {
		remove_oid(accel_dev, accel_dev->misc_error_dbgfile);
		accel_dev->misc_error_dbgfile = NULL;
	}

	kfree(misc_counter);
	misc_counter = NULL;
}
