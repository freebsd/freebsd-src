/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
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
#include <sys/types.h>
#include <sys/bus.h>
#include <sys/smp.h>
#include <dev/pci/pcivar.h>
#include <sys/malloc.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_cfg.h"
#include "adf_cfg_strings.h"
#include "adf_cfg_common.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include "adf_dev_err.h"

TASKQUEUE_DEFINE_THREAD(qat_pf);

static int
adf_enable_msix(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *info_pci_dev = &accel_dev->accel_pci_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	int msix_num_entries = 1;
	int count = 0;
	int error = 0;
	int num_vectors = 0;
	u_int *vectors;

	/* If SR-IOV is disabled, add entries for each bank */
	if (!accel_dev->u1.pf.vf_info) {
		msix_num_entries += hw_data->num_banks;
		num_vectors = 0;
		vectors = NULL;
	} else {
		num_vectors = hw_data->num_banks + 1;
		vectors = malloc(num_vectors * sizeof(u_int),
				 M_QAT,
				 M_WAITOK | M_ZERO);
		vectors[hw_data->num_banks] = 1;
	}

	count = msix_num_entries;
	error = pci_alloc_msix(info_pci_dev->pci_dev, &count);
	if (error == 0 && count != msix_num_entries) {
		pci_release_msi(info_pci_dev->pci_dev);
		error = EFBIG;
	}
	if (error) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to enable MSI-X IRQ(s)\n");
		free(vectors, M_QAT);
		return error;
	}

	if (vectors != NULL) {
		error =
		    pci_remap_msix(info_pci_dev->pci_dev, num_vectors, vectors);
		free(vectors, M_QAT);
		if (error) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to remap MSI-X IRQ(s)\n");
			pci_release_msi(info_pci_dev->pci_dev);
			return error;
		}
	}

	return 0;
}

static void
adf_disable_msix(struct adf_accel_pci *info_pci_dev)
{
	pci_release_msi(info_pci_dev->pci_dev);
}

static void
adf_msix_isr_bundle(void *bank_ptr)
{
	struct adf_etr_bank_data *bank = bank_ptr;
	struct adf_etr_data *priv_data = bank->accel_dev->transport;

	WRITE_CSR_INT_FLAG_AND_COL(bank->csr_addr, bank->bank_number, 0);
	adf_response_handler((uintptr_t)&priv_data->banks[bank->bank_number]);
	return;
}

static void
adf_msix_isr_ae(void *dev_ptr)
{
	struct adf_accel_dev *accel_dev = dev_ptr;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *pmisc =
	    &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	struct resource *pmisc_bar_addr = pmisc->virt_addr;
	u32 errsou3;
	u32 errsou5;
	bool reset_required = false;

	if (hw_data->ras_interrupts &&
	    hw_data->ras_interrupts(accel_dev, &reset_required))
		if (reset_required) {
			adf_notify_fatal_error(accel_dev);
			goto exit;
		}

	if (hw_data->check_slice_hang && hw_data->check_slice_hang(accel_dev)) {
	}

exit:
	errsou3 = ADF_CSR_RD(pmisc_bar_addr, ADF_ERRSOU3);
	errsou5 = ADF_CSR_RD(pmisc_bar_addr, ADF_ERRSOU5);
	if (errsou3 | errsou5)
		adf_print_err_registers(accel_dev);
	else
		device_printf(GET_DEV(accel_dev), "spurious AE interrupt\n");

	return;
}

static int
adf_get_irq_affinity(struct adf_accel_dev *accel_dev, int bank)
{
	int core = CPU_FIRST();
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	char bankName[ADF_CFG_MAX_KEY_LEN_IN_BYTES];

	snprintf(bankName,
		 ADF_CFG_MAX_KEY_LEN_IN_BYTES - 1,
		 ADF_ETRMGR_CORE_AFFINITY_FORMAT,
		 bank);
	bankName[ADF_CFG_MAX_KEY_LEN_IN_BYTES - 1] = '\0';

	if (adf_cfg_get_param_value(accel_dev, "Accelerator0", bankName, val)) {
		device_printf(GET_DEV(accel_dev),
			      "No CoreAffinity Set - using default core: %d\n",
			      core);
	} else {
		if (compat_strtouint(val, 10, &core)) {
			device_printf(GET_DEV(accel_dev),
				      "Can't get cpu core ID\n");
		}
	}
	return (core);
}

static int
adf_request_irqs(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *info_pci_dev = &accel_dev->accel_pci_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct msix_entry *msixe = info_pci_dev->msix_entries.entries;
	int ret = 0, rid = 0, i = 0;
	struct adf_etr_data *etr_data = accel_dev->transport;
	int computed_core = 0;

	/* Request msix irq for all banks unless SR-IOV enabled */
	if (!accel_dev->u1.pf.vf_info) {
		for (i = 0; i < hw_data->num_banks; i++) {
			struct adf_etr_bank_data *bank = &etr_data->banks[i];

			rid = i + 1;
			msixe[i].irq =
			    bus_alloc_resource_any(info_pci_dev->pci_dev,
						   SYS_RES_IRQ,
						   &rid,
						   RF_ACTIVE);
			if (msixe[i].irq == NULL) {
				device_printf(
				    GET_DEV(accel_dev),
				    "failed to allocate IRQ for bundle %d\n",
				    i);
				return ENXIO;
			}

			ret = bus_setup_intr(info_pci_dev->pci_dev,
					     msixe[i].irq,
					     INTR_TYPE_MISC | INTR_MPSAFE,
					     NULL,
					     adf_msix_isr_bundle,
					     bank,
					     &msixe[i].cookie);
			if (ret) {
				device_printf(
				    GET_DEV(accel_dev),
				    "failed to enable IRQ for bundle %d\n",
				    i);
				bus_release_resource(info_pci_dev->pci_dev,
						     SYS_RES_IRQ,
						     rid,
						     msixe[i].irq);
				msixe[i].irq = NULL;
				return ret;
			}

			computed_core = adf_get_irq_affinity(accel_dev, i);
			bus_describe_intr(info_pci_dev->pci_dev,
					  msixe[i].irq,
					  msixe[i].cookie,
					  "b%d",
					  i);
			bus_bind_intr(info_pci_dev->pci_dev,
				      msixe[i].irq,
				      computed_core);
		}
	}

	/* Request msix irq for AE */
	rid = hw_data->num_banks + 1;
	msixe[i].irq = bus_alloc_resource_any(info_pci_dev->pci_dev,
					      SYS_RES_IRQ,
					      &rid,
					      RF_ACTIVE);
	if (msixe[i].irq == NULL) {
		device_printf(GET_DEV(accel_dev),
			      "failed to allocate IRQ for ae-cluster\n");
		return ENXIO;
	}

	ret = bus_setup_intr(info_pci_dev->pci_dev,
			     msixe[i].irq,
			     INTR_TYPE_MISC | INTR_MPSAFE,
			     NULL,
			     adf_msix_isr_ae,
			     accel_dev,
			     &msixe[i].cookie);
	if (ret) {
		device_printf(GET_DEV(accel_dev),
			      "failed to enable IRQ for ae-cluster\n");
		bus_release_resource(info_pci_dev->pci_dev,
				     SYS_RES_IRQ,
				     rid,
				     msixe[i].irq);
		msixe[i].irq = NULL;
		return ret;
	}

	bus_describe_intr(info_pci_dev->pci_dev,
			  msixe[i].irq,
			  msixe[i].cookie,
			  "ae");
	return ret;
}

static void
adf_free_irqs(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *info_pci_dev = &accel_dev->accel_pci_dev;
	struct msix_entry *msixe = info_pci_dev->msix_entries.entries;
	int i = 0;

	if (info_pci_dev->msix_entries.num_entries > 0) {
		for (i = 0; i < info_pci_dev->msix_entries.num_entries; i++) {
			if (msixe[i].irq != NULL && msixe[i].cookie != NULL) {
				bus_teardown_intr(info_pci_dev->pci_dev,
						  msixe[i].irq,
						  msixe[i].cookie);
				bus_free_resource(info_pci_dev->pci_dev,
						  SYS_RES_IRQ,
						  msixe[i].irq);
			}
		}
	}
}

static int
adf_isr_alloc_msix_entry_table(struct adf_accel_dev *accel_dev)
{
	struct msix_entry *entries;
	u32 msix_num_entries = 1;

	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	/* If SR-IOV is disabled (vf_info is NULL), add entries for each bank */
	if (!accel_dev->u1.pf.vf_info)
		msix_num_entries += hw_data->num_banks;

	entries = malloc(msix_num_entries * sizeof(struct msix_entry),
			 M_QAT,
			 M_WAITOK | M_ZERO);

	accel_dev->accel_pci_dev.msix_entries.num_entries = msix_num_entries;
	accel_dev->accel_pci_dev.msix_entries.entries = entries;
	return 0;
}

static void
adf_isr_free_msix_entry_table(struct adf_accel_dev *accel_dev)
{

	free(accel_dev->accel_pci_dev.msix_entries.entries, M_QAT);
	accel_dev->accel_pci_dev.msix_entries.entries = NULL;
}

/**
 * adf_vf_isr_resource_free() - Free IRQ for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function frees interrupts for acceleration device.
 */
void
adf_isr_resource_free(struct adf_accel_dev *accel_dev)
{
	adf_free_irqs(accel_dev);
	adf_disable_msix(&accel_dev->accel_pci_dev);
	adf_isr_free_msix_entry_table(accel_dev);
}

/**
 * adf_vf_isr_resource_alloc() - Allocate IRQ for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function allocates interrupts for acceleration device.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_isr_resource_alloc(struct adf_accel_dev *accel_dev)
{
	int ret;

	ret = adf_isr_alloc_msix_entry_table(accel_dev);
	if (ret)
		return ret;
	if (adf_enable_msix(accel_dev))
		goto err_out;

	if (adf_request_irqs(accel_dev))
		goto err_out;

	return 0;
err_out:
	adf_isr_resource_free(accel_dev);
	return EFAULT;
}
