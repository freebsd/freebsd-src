/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "icp_qat_uclo.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_cfg_strings.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_transport_internal.h"

#define ADF_ARB_NUM 4
#define ADF_ARB_REG_SIZE 0x4
#define ADF_ARB_WTR_SIZE 0x20
#define ADF_ARB_OFFSET 0x30000
#define ADF_ARB_REG_SLOT 0x1000
#define ADF_ARB_WTR_OFFSET 0x010
#define ADF_ARB_RO_EN_OFFSET 0x090
#define ADF_ARB_WQCFG_OFFSET 0x100
#define ADF_ARB_WRK_2_SER_MAP_OFFSET 0x180
#define ADF_ARB_RINGSRVARBEN_OFFSET 0x19C

#define WRITE_CSR_ARB_RINGSRVARBEN(csr_addr, index, value)                     \
	ADF_CSR_WR(csr_addr,                                                   \
		   ADF_ARB_RINGSRVARBEN_OFFSET + (ADF_ARB_REG_SLOT * (index)), \
		   value)

#define WRITE_CSR_ARB_SARCONFIG(csr_addr, csr_offset, index, value)            \
	ADF_CSR_WR(csr_addr, (csr_offset) + (ADF_ARB_REG_SIZE * (index)), value)
#define READ_CSR_ARB_RINGSRVARBEN(csr_addr, index)                             \
	ADF_CSR_RD(csr_addr,                                                   \
		   ADF_ARB_RINGSRVARBEN_OFFSET + (ADF_ARB_REG_SLOT * (index)))

static DEFINE_MUTEX(csr_arb_lock);

#define WRITE_CSR_ARB_WRK_2_SER_MAP(                                           \
    csr_addr, csr_offset, wrk_to_ser_map_offset, index, value)                 \
	ADF_CSR_WR(csr_addr,                                                   \
		   ((csr_offset) + (wrk_to_ser_map_offset)) +                  \
		       (ADF_ARB_REG_SIZE * (index)),                           \
		   value)

int
adf_init_arb(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct arb_info info;
	struct resource *csr = accel_dev->transport->banks[0].csr_addr;
	u32 arb_cfg = 0x1 << 31 | 0x4 << 4 | 0x1;
	u32 arb;

	hw_data->get_arb_info(&info);

	/* Service arb configured for 32 bytes responses and
	 * ring flow control check enabled.
	 */
	for (arb = 0; arb < ADF_ARB_NUM; arb++)
		WRITE_CSR_ARB_SARCONFIG(csr, info.arbiter_offset, arb, arb_cfg);

	return 0;
}

int
adf_init_gen2_arb(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct arb_info info;
	struct resource *csr = accel_dev->transport->banks[0].csr_addr;
	u32 i;
	const u32 *thd_2_arb_cfg;

	/* invoke common adf_init_arb */
	adf_init_arb(accel_dev);

	hw_data->get_arb_info(&info);

	/* Map worker threads to service arbiters */
	hw_data->get_arb_mapping(accel_dev, &thd_2_arb_cfg);
	if (!thd_2_arb_cfg)
		return EFAULT;

	for (i = 0; i < hw_data->num_engines; i++)
		WRITE_CSR_ARB_WRK_2_SER_MAP(csr,
					    info.arbiter_offset,
					    info.wrk_thd_2_srv_arb_map,
					    i,
					    *(thd_2_arb_cfg + i));
	return 0;
}

void
adf_update_ring_arb(struct adf_etr_ring_data *ring)
{
	int shift;
	u32 arben, arben_tx, arben_rx, arb_mask;
	struct adf_accel_dev *accel_dev = ring->bank->accel_dev;
	struct adf_hw_csr_info *csr_info = &accel_dev->hw_device->csr_info;
	struct adf_hw_csr_ops *csr_ops = &csr_info->csr_ops;

	arb_mask = csr_info->arb_enable_mask;
	shift = hweight32(arb_mask);

	arben_tx = ring->bank->ring_mask & arb_mask;
	arben_rx = (ring->bank->ring_mask >> shift) & arb_mask;
	arben = arben_tx & arben_rx;
	csr_ops->write_csr_ring_srv_arb_en(ring->bank->csr_addr,
					   ring->bank->bank_number,
					   arben);
}

void
adf_update_uio_ring_arb(struct adf_uio_control_bundle *bundle)
{
	int shift;
	u32 arben, arben_tx, arben_rx, arb_mask;
	struct adf_accel_dev *accel_dev = bundle->uio_priv.accel->accel_dev;
	struct adf_hw_csr_info *csr_info = &accel_dev->hw_device->csr_info;
	struct adf_hw_csr_ops *csr_ops = &csr_info->csr_ops;

	arb_mask = csr_info->arb_enable_mask;
	shift = hweight32(arb_mask);

	arben_tx = bundle->rings_enabled & arb_mask;
	arben_rx = (bundle->rings_enabled >> shift) & arb_mask;
	arben = arben_tx & arben_rx;
	csr_ops->write_csr_ring_srv_arb_en(bundle->csr_addr,
					   bundle->hardware_bundle_number,
					   arben);
}
void
adf_enable_ring_arb(struct adf_accel_dev *accel_dev,
		    void *csr_addr,
		    unsigned int bank_nr,
		    unsigned int mask)
{
	struct adf_hw_csr_ops *csr_ops = GET_CSR_OPS(accel_dev);
	u32 arbenable;

	if (!csr_addr)
		return;

	mutex_lock(&csr_arb_lock);
	arbenable = csr_ops->read_csr_ring_srv_arb_en(csr_addr, bank_nr);
	arbenable |= mask & 0xFF;
	csr_ops->write_csr_ring_srv_arb_en(csr_addr, bank_nr, arbenable);

	mutex_unlock(&csr_arb_lock);
}

void
adf_disable_ring_arb(struct adf_accel_dev *accel_dev,
		     void *csr_addr,
		     unsigned int bank_nr,
		     unsigned int mask)
{
	struct adf_hw_csr_ops *csr_ops = GET_CSR_OPS(accel_dev);
	struct resource *csr = csr_addr;
	u32 arbenable;

	if (!csr_addr)
		return;

	mutex_lock(&csr_arb_lock);
	arbenable = csr_ops->read_csr_ring_srv_arb_en(csr, bank_nr);
	arbenable &= ~mask & 0xFF;
	csr_ops->write_csr_ring_srv_arb_en(csr, bank_nr, arbenable);
	mutex_unlock(&csr_arb_lock);
}

void
adf_exit_arb(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_csr_ops *csr_ops = GET_CSR_OPS(accel_dev);
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct arb_info info;
	struct resource *csr;
	unsigned int i;

	if (!accel_dev->transport)
		return;

	csr = accel_dev->transport->banks[0].csr_addr;

	hw_data->get_arb_info(&info);

	/* Reset arbiter configuration */
	for (i = 0; i < ADF_ARB_NUM; i++)
		WRITE_CSR_ARB_SARCONFIG(csr, info.arbiter_offset, i, 0);

	/* Unmap worker threads to service arbiters */
	if (hw_data->get_arb_mapping) {
		for (i = 0; i < hw_data->num_engines; i++)
			WRITE_CSR_ARB_WRK_2_SER_MAP(csr,
						    info.arbiter_offset,
						    info.wrk_thd_2_srv_arb_map,
						    i,
						    0);
	}

	/* Disable arbitration on all rings */
	for (i = 0; i < GET_MAX_BANKS(accel_dev); i++)
		csr_ops->write_csr_ring_srv_arb_en(csr, i, 0);
}

void
adf_disable_arb(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_csr_ops *csr_ops;
	struct resource *csr;
	unsigned int i;

	if (!accel_dev || !accel_dev->transport)
		return;

	csr = accel_dev->transport->banks[0].csr_addr;
	csr_ops = GET_CSR_OPS(accel_dev);

	/* Disable arbitration on all rings */
	for (i = 0; i < GET_MAX_BANKS(accel_dev); i++)
		csr_ops->write_csr_ring_srv_arb_en(csr, i, 0);
}
