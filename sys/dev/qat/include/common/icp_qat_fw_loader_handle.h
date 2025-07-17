/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef __ICP_QAT_FW_LOADER_HANDLE_H__
#define __ICP_QAT_FW_LOADER_HANDLE_H__
#include "icp_qat_uclo.h"

struct icp_qat_fw_loader_ae_data {
	unsigned int state;
	unsigned int ustore_size;
	unsigned int free_addr;
	unsigned int free_size;
	unsigned int live_ctx_mask;
};

struct icp_qat_fw_loader_hal_handle {
	struct icp_qat_fw_loader_ae_data aes[ICP_QAT_UCLO_MAX_AE];
	unsigned int ae_mask;
	unsigned int admin_ae_mask;
	unsigned int slice_mask;
	unsigned int revision_id;
	unsigned int ae_max_num;
	unsigned int upc_mask;
	unsigned int max_ustore;
};

struct icp_qat_fw_loader_handle {
	struct icp_qat_fw_loader_hal_handle *hal_handle;
	struct adf_accel_dev *accel_dev;
	device_t pci_dev;
	void *obj_handle;
	void *sobj_handle;
	void *mobj_handle;
	bool fw_auth;
	unsigned int cfg_ae_mask;
	rman_res_t hal_sram_size;
	struct resource *hal_sram_addr_v;
	unsigned int hal_sram_offset;
	struct resource *hal_misc_addr_v;
	uintptr_t hal_cap_g_ctl_csr_addr_v;
	uintptr_t hal_cap_ae_xfer_csr_addr_v;
	uintptr_t hal_cap_ae_local_csr_addr_v;
	uintptr_t hal_ep_csr_addr_v;
};

struct icp_firml_dram_desc {
	struct bus_dmamem dram_mem;

	struct resource *dram_base_addr;
	void *dram_base_addr_v;
	bus_addr_t dram_bus_addr;
	u64 dram_size;
};
#endif
