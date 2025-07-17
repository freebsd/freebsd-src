/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include "adf_c4xxx_hw_data.h"
#include "adf_c4xxx_pke_replay_stats.h"
#include "adf_common_drv.h"
#include "icp_qat_fw_init_admin.h"
#include <sys/sbuf.h>
#include <sys/sysctl.h>
#include <sys/priv.h>

#define PKE_REPLAY_DBG_FILE "pke_replay_stats"
#define LINE                                                                   \
	"+-----------------------------------------------------------------+\n"
#define BANNER                                                                 \
	"|             PKE Replay Statistics for Qat Device                |\n"

static int qat_pke_replay_counters_show(SYSCTL_HANDLER_ARGS)
{
	struct sbuf sb;
	struct adf_accel_dev *accel_dev = arg1;
	int ret = 0;
	u64 suc_counter = 0;
	u64 unsuc_counter = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	sbuf_new_for_sysctl(&sb, NULL, 256, req);

	sbuf_printf(&sb, "\n");
	sbuf_printf(&sb, LINE);

	ret = adf_get_fw_pke_stats(accel_dev, &suc_counter, &unsuc_counter);
	if (ret)
		return ret;

	sbuf_printf(
	    &sb,
	    "| Successful Replays:    %40llu |\n| Unsuccessful Replays:  %40llu |\n",
	    (unsigned long long)suc_counter,
	    (unsigned long long)unsuc_counter);

	sbuf_finish(&sb);
	SYSCTL_OUT(req, sbuf_data(&sb), sbuf_len(&sb));
	sbuf_delete(&sb);

	return 0;
}

/**
 * adf_pke_replay_counters_add_c4xxx() - Create debugfs entry for
 * acceleration device Freq counters.
 * @accel_dev:  Pointer to acceleration device.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_pke_replay_counters_add_c4xxx(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx = NULL;
	struct sysctl_oid *qat_sysctl_tree = NULL;
	struct sysctl_oid *pke_rep_file = NULL;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);

	pke_rep_file = SYSCTL_ADD_PROC(qat_sysctl_ctx,
				       SYSCTL_CHILDREN(qat_sysctl_tree),
				       OID_AUTO,
				       PKE_REPLAY_DBG_FILE,
				       CTLTYPE_STRING | CTLFLAG_RD,
				       accel_dev,
				       0,
				       qat_pke_replay_counters_show,
				       "A",
				       "QAT PKE Replay Statistics");
	accel_dev->pke_replay_dbgfile = pke_rep_file;
	if (!accel_dev->pke_replay_dbgfile) {
		device_printf(
		    GET_DEV(accel_dev),
		    "Failed to create qat pke replay debugfs entry.\n");
		return ENOENT;
	}
	return 0;
}

/**
 * adf_pke_replay_counters_remove_c4xxx() - Remove debugfs entry for
 * acceleration device Freq counters.
 * @accel_dev:  Pointer to acceleration device.
 *
 * Return: void
 */
void
adf_pke_replay_counters_remove_c4xxx(struct adf_accel_dev *accel_dev)
{
	if (accel_dev->pke_replay_dbgfile) {
		remove_oid(accel_dev, accel_dev->pke_replay_dbgfile);
		accel_dev->pke_replay_dbgfile = NULL;
	}
}
