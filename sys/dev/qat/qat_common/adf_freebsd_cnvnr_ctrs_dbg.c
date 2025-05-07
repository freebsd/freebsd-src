/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/systm.h>
#include <sys/priv.h>
#include "adf_cnvnr_freq_counters.h"
#include "adf_common_drv.h"
#include "adf_cfg.h"
#include "icp_qat_fw_init_admin.h"

#define ADF_CNVNR_ERR_MASK 0xFFF

#define LINE                                                                   \
	"+-----------------------------------------------------------------+\n"
#define BANNER                                                                 \
	"|             CNV Error Freq Statistics for Qat Device            |\n"
#define NEW_LINE "\n"
#define REPORT_ENTRY_FORMAT                                                    \
	"|[AE %2d]: TotalErrors: %5d : LastError: %s [%5d]  |\n"
#define MAX_LINE_LENGTH 128
#define MAX_REPORT_SIZE ((ADF_MAX_ACCELENGINES + 3) * MAX_LINE_LENGTH)

#define PRINT_LINE(line)                                                       \
	(snprintf(                                                             \
	    report_ptr, MAX_REPORT_SIZE - (report_ptr - report), "%s", line))

const char  *cnvnr_err_str[] = {"No Error      ",
				"Checksum Error",
				"Length Error-P",
				"Decomp Error  ",
				"Xlat Error    ",
				"Length Error-C",
				"Unknown Error "};

/* Handler for HB status check */
static int qat_cnvnr_ctrs_dbg_read(SYSCTL_HANDLER_ARGS)
{
	struct adf_accel_dev *accel_dev = arg1;
	struct adf_hw_device_data *hw_device;
	struct icp_qat_fw_init_admin_req request;
	struct icp_qat_fw_init_admin_resp response;
	unsigned long dc_ae_msk = 0;
	u8 num_aes = 0, ae = 0, error_type = 0, bytes_written = 0;
	s16 latest_error = 0;
	char report[MAX_REPORT_SIZE];
	char *report_ptr = report;

	if (priv_check(curthread, PRIV_DRIVER) != 0)
		return EPERM;

	/* Defensive check */
	if (!accel_dev || accel_dev->accel_id > ADF_MAX_DEVICES)
		return EINVAL;

	if (!adf_dev_started(accel_dev)) {
		device_printf(GET_DEV(accel_dev), "QAT Device not started\n");
		return EINVAL;
	}

	hw_device = accel_dev->hw_device;
	if (!hw_device) {
		device_printf(GET_DEV(accel_dev), "Failed to get hw_device.\n");
		return EFAULT;
	}

	/* Clean report memory */
	explicit_bzero(report, sizeof(report));

	/* Adding banner to report */
	bytes_written = PRINT_LINE(NEW_LINE);
	if (bytes_written <= 0)
		return EINVAL;
	report_ptr += bytes_written;

	bytes_written = PRINT_LINE(LINE);
	if (bytes_written <= 0)
		return EINVAL;
	report_ptr += bytes_written;

	bytes_written = PRINT_LINE(BANNER);
	if (bytes_written <= 0)
		return EINVAL;
	report_ptr += bytes_written;

	bytes_written = PRINT_LINE(LINE);
	if (bytes_written <= 0)
		return EINVAL;
	report_ptr += bytes_written;

	if (accel_dev->au_info)
		dc_ae_msk = accel_dev->au_info->dc_ae_msk;

	/* Extracting number of Acceleration Engines */
	num_aes = hw_device->get_num_aes(hw_device);
	explicit_bzero(&request, sizeof(struct icp_qat_fw_init_admin_req));
	for (ae = 0; ae < num_aes; ae++) {
		if (accel_dev->au_info && !test_bit(ae, &dc_ae_msk))
			continue;
		explicit_bzero(&response,
			       sizeof(struct icp_qat_fw_init_admin_resp));
		request.cmd_id = ICP_QAT_FW_CNV_STATS_GET;
		if (adf_put_admin_msg_sync(
			accel_dev, ae, &request, &response) ||
		    response.status) {
			return EFAULT;
		}
		error_type = CNV_ERROR_TYPE_GET(response.latest_error);
		if (error_type == CNV_ERR_TYPE_DECOMP_PRODUCED_LENGTH_ERROR ||
		    error_type == CNV_ERR_TYPE_DECOMP_CONSUMED_LENGTH_ERROR) {
			latest_error =
			    CNV_ERROR_LENGTH_DELTA_GET(response.latest_error);
		} else if (error_type == CNV_ERR_TYPE_DECOMPRESSION_ERROR ||
			   error_type == CNV_ERR_TYPE_TRANSLATION_ERROR) {
			latest_error =
			    CNV_ERROR_DECOMP_STATUS_GET(response.latest_error);
		} else {
			latest_error =
			    response.latest_error & ADF_CNVNR_ERR_MASK;
		}

		bytes_written =
		    snprintf(report_ptr,
			     MAX_REPORT_SIZE - (report_ptr - report),
			     REPORT_ENTRY_FORMAT,
			     ae,
			     response.error_count,
			     cnvnr_err_str[error_type],
			     latest_error);
		if (bytes_written <= 0) {
			device_printf(
			    GET_DEV(accel_dev),
			    "ERROR: No space left in CnV ctrs line buffer\n"
			    "\tAcceleration ID: %d, Engine: %d\n",
			    accel_dev->accel_id,
			    ae);
			break;
		}
		report_ptr += bytes_written;
	}

	sysctl_handle_string(oidp, report, sizeof(report), req);
	return 0;
}

int
adf_cnvnr_freq_counters_add(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx;
	struct sysctl_oid *qat_cnvnr_ctrs_sysctl_tree;

	/* Defensive checks */
	if (!accel_dev)
		return EINVAL;

	/* Creating context and tree */
	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_cnvnr_ctrs_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);

	/* Create "cnv_error" string type leaf - with callback */
	accel_dev->cnv_error_oid =
	    SYSCTL_ADD_PROC(qat_sysctl_ctx,
			    SYSCTL_CHILDREN(qat_cnvnr_ctrs_sysctl_tree),
			    OID_AUTO,
			    "cnv_error",
			    CTLTYPE_STRING | CTLFLAG_RD,
			    accel_dev,
			    0,
			    qat_cnvnr_ctrs_dbg_read,
			    "IU",
			    "QAT CnVnR status");

	if (!accel_dev->cnv_error_oid) {
		device_printf(
		    GET_DEV(accel_dev),
		    "Failed to create qat cnvnr freq counters sysctl entry.\n");
		return ENOMEM;
	}
	return 0;
}

void
adf_cnvnr_freq_counters_remove(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx;

	if (!accel_dev)
		return;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);

	if (accel_dev->cnv_error_oid) {
		sysctl_ctx_entry_del(qat_sysctl_ctx, accel_dev->cnv_error_oid);
		sysctl_remove_oid(accel_dev->cnv_error_oid, 1, 1);
		accel_dev->cnv_error_oid = NULL;
	}
}
