/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef ADF_4XXXVF_HW_DATA_H_
#define ADF_4XXXVF_HW_DATA_H_

#define ADF_4XXXIOV_PMISC_BAR 1
#define ADF_4XXXIOV_ACCELERATORS_MASK 0x1
#define ADF_4XXXIOV_ACCELENGINES_MASK 0x1
#define ADF_4XXXIOV_MAX_ACCELERATORS 1
#define ADF_4XXXIOV_MAX_ACCELENGINES 1
#define ADF_4XXXIOV_NUM_RINGS_PER_BANK 2
#define ADF_4XXXIOV_RX_RINGS_OFFSET 1
#define ADF_4XXXIOV_TX_RINGS_MASK 0x1
#define ADF_4XXXIOV_ETR_BAR 0
#define ADF_4XXXIOV_ETR_MAX_BANKS 4

#define ADF_4XXXIOV_VINTSOU_OFFSET 0x0
#define ADF_4XXXIOV_VINTMSK_OFFSET 0x4
#define ADF_4XXXIOV_VINTSOUPF2VM_OFFSET 0x1000
#define ADF_4XXXIOV_VINTMSKPF2VM_OFFSET 0x1004
#define ADF_4XXX_DEF_ASYM_MASK 0x1

/* Virtual function fuses */
#define ADF_4XXXIOV_VFFUSECTL0_OFFSET (0x40)
#define ADF_4XXXIOV_VFFUSECTL1_OFFSET (0x44)
#define ADF_4XXXIOV_VFFUSECTL2_OFFSET (0x4C)
#define ADF_4XXXIOV_VFFUSECTL4_OFFSET (0x1C4)
#define ADF_4XXXIOV_VFFUSECTL5_OFFSET (0x1C8)

/*qat_4xxxvf fuse bits are same as qat_4xxx*/
enum icp_qat_4xxxvf_slice_mask {
	ICP_ACCEL_4XXXVF_MASK_CIPHER_SLICE = 0x01,
	ICP_ACCEL_4XXXVF_MASK_AUTH_SLICE = 0x02,
	ICP_ACCEL_4XXXVF_MASK_PKE_SLICE = 0x04,
	ICP_ACCEL_4XXXVF_MASK_COMPRESS_SLICE = 0x08,
	ICP_ACCEL_4XXXVF_MASK_UCS_SLICE = 0x10,
	ICP_ACCEL_4XXXVF_MASK_EIA3_SLICE = 0x20,
	/*SM3&SM4 are indicated by same bit*/
	ICP_ACCEL_4XXXVF_MASK_SMX_SLICE = 0x80,
};

void adf_init_hw_data_4xxxiov(struct adf_hw_device_data *hw_data);
void adf_clean_hw_data_4xxxiov(struct adf_hw_device_data *hw_data);
u32 adf_4xxxvf_get_hw_cap(struct adf_accel_dev *accel_dev);
#endif
