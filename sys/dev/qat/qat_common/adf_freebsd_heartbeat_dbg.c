/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/priv.h>
#include "adf_heartbeat_dbg.h"
#include "adf_common_drv.h"
#include "adf_cfg.h"
#include "adf_heartbeat.h"

#define HB_SYSCTL_ERR(RC)                                                           \
	do {                                                                        \
		if (RC == NULL) {                                                   \
			printf(                                                     \
			    "Memory allocation failed in adf_heartbeat_dbg_add\n"); \
			return ENOMEM;                                              \
		}                                                                   \
	} while (0)


static int qat_dev_hb_read_sent(SYSCTL_HANDLER_ARGS)
{
	struct adf_accel_dev *accel_dev = arg1;
	struct adf_heartbeat *hb;
	int error = EFAULT;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	if (accel_dev == NULL)
		return EINVAL;

	hb = accel_dev->heartbeat;

	error = sysctl_handle_int(oidp, &hb->hb_sent_counter, 0, req);
	if (error || !req->newptr)
		return error;

	return (0);
}

static int qat_dev_hb_read_failed(SYSCTL_HANDLER_ARGS)
{
	struct adf_accel_dev *accel_dev = arg1;
	struct adf_heartbeat *hb;
	int error = EFAULT;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	if (accel_dev == NULL)
		return EINVAL;

	hb = accel_dev->heartbeat;

	error = sysctl_handle_int(oidp, &hb->hb_failed_counter, 0, req);
	if (error || !req->newptr)
		return error;

	return (0);
}

/* Handler for HB status check */
static int qat_dev_hb_read(SYSCTL_HANDLER_ARGS)
{
	enum adf_device_heartbeat_status hb_status = DEV_HB_UNRESPONSIVE;
	struct adf_accel_dev *accel_dev = arg1;
	struct adf_heartbeat *hb;
	int ret = 0;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	if (accel_dev == NULL) {
		return EINVAL;
	}
	hb = accel_dev->heartbeat;

	/* if FW is loaded, proceed else set heartbeat down */
	if (test_bit(ADF_STATUS_AE_UCODE_LOADED, &accel_dev->status)) {
		adf_heartbeat_status(accel_dev, &hb_status);
	}
	if (hb_status == DEV_HB_ALIVE) {
		hb->heartbeat.hb_sysctlvar = 1;
	} else {
		hb->heartbeat.hb_sysctlvar = 0;
	}
	ret = sysctl_handle_int(oidp, &hb->heartbeat.hb_sysctlvar, 0, req);
	return ret;
}

int
adf_heartbeat_dbg_add(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_hb_sysctl_ctx;
	struct sysctl_oid *qat_hb_sysctl_tree;
	struct adf_heartbeat *hb;

	if (accel_dev == NULL) {
		return EINVAL;
	}

	if (adf_heartbeat_init(accel_dev))
		return EINVAL;

	hb = accel_dev->heartbeat;
	qat_hb_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_hb_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);

	hb->heartbeat_sent.oid =
	    SYSCTL_ADD_PROC(qat_hb_sysctl_ctx,
			    SYSCTL_CHILDREN(qat_hb_sysctl_tree),
			    OID_AUTO,
			    "heartbeat_sent",
			    CTLTYPE_INT | CTLFLAG_RD,
			    accel_dev,
			    0,
			    qat_dev_hb_read_sent,
			    "IU",
			    "HB failed count");
	HB_SYSCTL_ERR(hb->heartbeat_sent.oid);

	hb->heartbeat_failed.oid =
	    SYSCTL_ADD_PROC(qat_hb_sysctl_ctx,
			    SYSCTL_CHILDREN(qat_hb_sysctl_tree),
			    OID_AUTO,
			    "heartbeat_failed",
			    CTLTYPE_INT | CTLFLAG_RD,
			    accel_dev,
			    0,
			    qat_dev_hb_read_failed,
			    "IU",
			    "HB failed count");
	HB_SYSCTL_ERR(hb->heartbeat_failed.oid);

	hb->heartbeat.oid = SYSCTL_ADD_PROC(qat_hb_sysctl_ctx,
					    SYSCTL_CHILDREN(qat_hb_sysctl_tree),
					    OID_AUTO,
					    "heartbeat",
					    CTLTYPE_INT | CTLFLAG_RD,
					    accel_dev,
					    0,
					    qat_dev_hb_read,
					    "IU",
					    "QAT device status");
	HB_SYSCTL_ERR(hb->heartbeat.oid);
	return 0;
}

int
adf_heartbeat_dbg_del(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx;
	struct adf_heartbeat *hb;

	if (!accel_dev) {
		return EINVAL;
	}

	hb = accel_dev->heartbeat;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);

	if (hb->heartbeat.oid) {
		sysctl_ctx_entry_del(qat_sysctl_ctx, hb->heartbeat.oid);
		sysctl_remove_oid(hb->heartbeat.oid, 1, 1);
		hb->heartbeat.oid = NULL;
	}

	if (hb->heartbeat_failed.oid) {
		sysctl_ctx_entry_del(qat_sysctl_ctx, hb->heartbeat_failed.oid);
		sysctl_remove_oid(hb->heartbeat_failed.oid, 1, 1);
		hb->heartbeat_failed.oid = NULL;
	}

	if (hb->heartbeat_sent.oid) {
		sysctl_ctx_entry_del(qat_sysctl_ctx, hb->heartbeat_sent.oid);
		sysctl_remove_oid(hb->heartbeat_sent.oid, 1, 1);
		hb->heartbeat_sent.oid = NULL;
	}

	adf_heartbeat_clean(accel_dev);

	return 0;
}
