/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_dev_err.h"
#include "adf_freebsd_pfvf_ctrs_dbg.h"

#define MAX_REPORT_LINES (14)
#define MAX_REPORT_LINE_LEN (64)
#define MAX_REPORT_SIZE (MAX_REPORT_LINES * MAX_REPORT_LINE_LEN)

static void
adf_pfvf_ctrs_prepare_report(char *rep, struct pfvf_stats *pfvf_counters)
{
	unsigned int value = 0;
	char *string = "unknown";
	unsigned int pos = 0;
	char *ptr = rep;

	for (pos = 0; pos < MAX_REPORT_LINES; pos++) {
		switch (pos) {
		case 0:
			string = "Messages written to CSR";
			value = pfvf_counters->tx;
			break;
		case 1:
			string = "Messages read from CSR";
			value = pfvf_counters->rx;
			break;
		case 2:
			string = "Spurious Interrupt";
			value = pfvf_counters->spurious;
			break;
		case 3:
			string = "Block messages sent";
			value = pfvf_counters->blk_tx;
			break;
		case 4:
			string = "Block messages received";
			value = pfvf_counters->blk_rx;
			break;
		case 5:
			string = "Blocks received with CRC errors";
			value = pfvf_counters->crc_err;
			break;
		case 6:
			string = "CSR in use";
			value = pfvf_counters->busy;
			break;
		case 7:
			string = "No acknowledgment";
			value = pfvf_counters->no_ack;
			break;
		case 8:
			string = "Collisions";
			value = pfvf_counters->collision;
			break;
		case 9:
			string = "Put msg timeout";
			value = pfvf_counters->tx_timeout;
			break;
		case 10:
			string = "No response received";
			value = pfvf_counters->rx_timeout;
			break;
		case 11:
			string = "Responses received";
			value = pfvf_counters->rx_rsp;
			break;
		case 12:
			string = "Messages re-transmitted";
			value = pfvf_counters->retry;
			break;
		case 13:
			string = "Put event timeout";
			value = pfvf_counters->event_timeout;
			break;
		default:
			value = 0;
		}
		if (value)
			ptr += snprintf(ptr,
					(MAX_REPORT_SIZE - (ptr - rep)),
					"%s %u\n",
					string,
					value);
	}
}

static int adf_pfvf_ctrs_show(SYSCTL_HANDLER_ARGS)
{
	struct pfvf_stats *pfvf_counters = arg1;
	char report[MAX_REPORT_SIZE];

	if (!pfvf_counters)
		return EINVAL;

	explicit_bzero(report, sizeof(report));
	adf_pfvf_ctrs_prepare_report(report, pfvf_counters);
	sysctl_handle_string(oidp, report, sizeof(report), req);
	return 0;
}

int
adf_pfvf_ctrs_dbg_add(struct adf_accel_dev *accel_dev)
{
	struct sysctl_ctx_list *qat_sysctl_ctx;
	struct sysctl_oid *qat_pfvf_ctrs_sysctl_tree;
	struct sysctl_oid *oid_pfvf;
	device_t dev;

	if (!accel_dev || accel_dev->accel_id > ADF_MAX_DEVICES)
		return EINVAL;

	dev = GET_DEV(accel_dev);

	qat_sysctl_ctx = device_get_sysctl_ctx(dev);
	qat_pfvf_ctrs_sysctl_tree = device_get_sysctl_tree(dev);

	oid_pfvf = SYSCTL_ADD_PROC(qat_sysctl_ctx,
				   SYSCTL_CHILDREN(qat_pfvf_ctrs_sysctl_tree),
				   OID_AUTO,
				   "pfvf_counters",
				   CTLTYPE_STRING | CTLFLAG_RD,
				   &accel_dev->u1.vf.pfvf_counters,
				   0,
				   adf_pfvf_ctrs_show,
				   "A",
				   "QAT PFVF counters");

	if (!oid_pfvf) {
		device_printf(dev, "Failure creating PFVF counters sysctl\n");
		return ENOMEM;
	}
	return 0;
}
