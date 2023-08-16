/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include <dev/pci/pcireg.h>
#include "adf_c4xxx_reset.h"

static void
adf_check_uncorr_status(struct adf_accel_dev *accel_dev)
{
	u32 uncorr_err;
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;

	uncorr_err = pci_read_config(pdev, PCI_EXP_AERUCS, 4);
	if (uncorr_err & PCIE_C4XXX_VALID_ERR_MASK) {
		device_printf(GET_DEV(accel_dev),
			      "Uncorrectable error occurred during reset\n");
		device_printf(GET_DEV(accel_dev),
			      "Error code value: 0x%04x\n",
			      uncorr_err);
	}
}

static void
adf_c4xxx_dev_reset(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	u8 count = 0;
	uintptr_t device_id1;
	uintptr_t device_id2;

	/* Read device ID before triggering reset */
	device_id1 = pci_read_config(pdev, PCIR_DEVICE, 2);
	hw_device->reset_device(accel_dev);

	/* Wait for reset to complete */
	do {
		/* Ensure we have the configuration space restored */
		device_id2 = pci_read_config(pdev, PCIR_DEVICE, 2);
		if (device_id1 == device_id2) {
			/* Check if a PCIe uncorrectable error occurred
			 * during the reset
			 */
			adf_check_uncorr_status(accel_dev);
			return;
		}
		count++;
		pause_ms("adfstop", 100);
	} while (count < ADF_PCIE_FLR_ATTEMPT);
	device_printf(GET_DEV(accel_dev),
		      "Too many attempts to read back config space.\n");
}

void
adf_c4xxx_dev_restore(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	device_t pdev = accel_dev->accel_pci_dev.pci_dev;
	u32 pmisclbar1;
	u32 pmisclbar2;
	u32 pmiscubar1;
	u32 pmiscubar2;

	if (hw_device->reset_device) {
		device_printf(GET_DEV(accel_dev),
			      "Resetting device qat_dev%d\n",
			      accel_dev->accel_id);

		/* Read pmiscubar and pmisclbar */
		pmisclbar1 = pci_read_config(pdev, ADF_PMISC_L_OFFSET, 4);
		pmiscubar1 = pci_read_config(pdev, ADF_PMISC_U_OFFSET, 4);

		adf_c4xxx_dev_reset(accel_dev);
		pci_restore_state(pdev);

		/* Read pmiscubar and pmisclbar */
		pmisclbar2 = pci_read_config(pdev, ADF_PMISC_L_OFFSET, 4);
		pmiscubar2 = pci_read_config(pdev, ADF_PMISC_U_OFFSET, 4);

		/* Check if restore operation has completed successfully */
		if (pmisclbar1 != pmisclbar2 || pmiscubar1 != pmiscubar2) {
			device_printf(
			    GET_DEV(accel_dev),
			    "Failed to restore device configuration\n");
			return;
		}
		pci_save_state(pdev);
	}

	if (hw_device->post_reset) {
		dev_dbg(GET_DEV(accel_dev), "Performing post reset restore\n");
		hw_device->post_reset(accel_dev);
	}
}
