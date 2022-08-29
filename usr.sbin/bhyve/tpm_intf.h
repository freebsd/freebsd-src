/*-
 * SPDX-License-Identifier: BSD-2-Clause-FreeBSD
 *
 * Copyright (c) 2022 Beckhoff Automation GmbH & Co. KG
 * Author: Corvin Köhne <c.koehne@beckhoff.com>
 */

#pragma once

#include "config.h"
#include "tpm_device.h"

#define TPM_INTF_TYPE_FIFO_PTP 0x0
#define TPM_INTF_TYPE_CRB 0x1
#define TPM_INTF_TYPE_FIFO_TIS 0xF

#define TPM_INTF_VERSION_FIFO 0
#define TPM_INTF_VERSION_CRB 1

#define TPM_INTF_CAP_CRB_DATA_XFER_SIZE_4 0
#define TPM_INTF_CAP_CRB_DATA_XFER_SIZE_8 1
#define TPM_INTF_CAP_CRB_DATA_XFER_SIZE_32 2
#define TPM_INTF_CAP_CRB_DATA_XFER_SIZE_64 3

#define TPM_INTF_SELECTOR_FIFO 0
#define TPM_INTF_SELECTOR_CRB 1

struct tpm_intf {
	const char *name;

	int (*init)(void **sc);
	void (*deinit)(void *sc);
	int (*build_acpi_table)(void *sc);
};
#define TPM_INTF_SET(x) DATA_SET(tpm_intf_set, x)
