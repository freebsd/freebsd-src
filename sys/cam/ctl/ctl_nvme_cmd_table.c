/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2023 Chelsio Communications, Inc.
 */

#include <dev/nvme/nvme.h>

#include <cam/ctl/ctl.h>
#include <cam/ctl/ctl_ha.h>
#include <cam/ctl/ctl_io.h>
#include <cam/ctl/ctl_ioctl.h>
#include <cam/ctl/ctl_private.h>

/* Administrative Command Set (CTL_IO_NVME_ADMIN). */
const struct ctl_nvme_cmd_entry nvme_admin_cmd_table[256] =
{
	[NVME_OPC_IDENTIFY] = { ctl_nvme_identify, CTL_FLAG_DATA_IN |
				CTL_CMD_FLAG_OK_ON_NO_LUN },
};

/* NVM Command Set (CTL_IO_NVME). */
const struct ctl_nvme_cmd_entry nvme_nvm_cmd_table[256] =
{
	[NVME_OPC_FLUSH] = { ctl_nvme_flush, CTL_FLAG_DATA_NONE },
	[NVME_OPC_WRITE] = { ctl_nvme_read_write, CTL_FLAG_DATA_OUT },
	[NVME_OPC_READ] = { ctl_nvme_read_write, CTL_FLAG_DATA_IN },
	[NVME_OPC_WRITE_UNCORRECTABLE] = { ctl_nvme_write_uncorrectable,
					   CTL_FLAG_DATA_NONE },
	[NVME_OPC_COMPARE] = { ctl_nvme_compare, CTL_FLAG_DATA_OUT },
	[NVME_OPC_WRITE_ZEROES] = { ctl_nvme_write_zeroes, CTL_FLAG_DATA_NONE },
	[NVME_OPC_DATASET_MANAGEMENT] = { ctl_nvme_dataset_management,
					  CTL_FLAG_DATA_OUT },
	[NVME_OPC_VERIFY] = { ctl_nvme_verify, CTL_FLAG_DATA_NONE },
};
