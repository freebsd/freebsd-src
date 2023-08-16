/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "adf_c4xxx_hw_data.h"
#include "adf_fw_counters.h"
#include "adf_cfg_device.h"
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <machine/bus_dma.h>
#include <dev/pci/pcireg.h>
#include "adf_heartbeat_dbg.h"
#include "adf_cnvnr_freq_counters.h"

static MALLOC_DEFINE(M_QAT_C4XXX, "qat_c4xx", "qat_c4xx");

#define ADF_SYSTEM_DEVICE(device_id)                                           \
	{                                                                      \
		PCI_VENDOR_ID_INTEL, device_id                                 \
	}

static const struct pci_device_id adf_pci_tbl[] =
    { ADF_SYSTEM_DEVICE(ADF_C4XXX_PCI_DEVICE_ID),
      {
	  0,
      } };

static int
adf_probe(device_t dev)
{
	const struct pci_device_id *id;

	for (id = adf_pci_tbl; id->vendor != 0; id++) {
		if (pci_get_vendor(dev) == id->vendor &&
		    pci_get_device(dev) == id->device) {
			device_set_desc(dev,
					"Intel " ADF_C4XXX_DEVICE_NAME
					" QuickAssist");
			return BUS_PROBE_GENERIC;
		}
	}
	return ENXIO;
}

static void
adf_cleanup_accel(struct adf_accel_dev *accel_dev)
{
	struct adf_accel_pci *accel_pci_dev = &accel_dev->accel_pci_dev;
	int i;

	if (accel_dev->dma_tag)
		bus_dma_tag_destroy(accel_dev->dma_tag);
	for (i = 0; i < ADF_PCI_MAX_BARS; i++) {
		struct adf_bar *bar = &accel_pci_dev->pci_bars[i];

		if (bar->virt_addr)
			bus_free_resource(accel_pci_dev->pci_dev,
					  SYS_RES_MEMORY,
					  bar->virt_addr);
	}

	if (accel_dev->hw_device) {
		switch (pci_get_device(accel_pci_dev->pci_dev)) {
		case ADF_C4XXX_PCI_DEVICE_ID:
			adf_clean_hw_data_c4xxx(accel_dev->hw_device);
			break;
		default:
			break;
		}
		free(accel_dev->hw_device, M_QAT_C4XXX);
		accel_dev->hw_device = NULL;
	}
	adf_cfg_dev_remove(accel_dev);
	adf_devmgr_rm_dev(accel_dev, NULL);
}

static int
adf_attach(device_t dev)
{
	struct adf_accel_dev *accel_dev;
	struct adf_accel_pci *accel_pci_dev;
	struct adf_hw_device_data *hw_data;
	unsigned int i, bar_nr;
	int ret, rid;
	struct adf_cfg_device *cfg_dev = NULL;

	/* Set pci MaxPayLoad to 256. Implemented to avoid the issue of
	 * Pci-passthrough causing Maxpayload to be reset to 128 bytes
	 * when the device is reset.
	 */
	if (pci_get_max_payload(dev) != 256)
		pci_set_max_payload(dev, 256);

	accel_dev = device_get_softc(dev);

	INIT_LIST_HEAD(&accel_dev->crypto_list);
	accel_pci_dev = &accel_dev->accel_pci_dev;
	accel_pci_dev->pci_dev = dev;

	if (bus_get_domain(dev, &accel_pci_dev->node) != 0)
		accel_pci_dev->node = 0;

	/* XXX: Revisit if we actually need a devmgr table at all. */

	/* Add accel device to accel table.
	 * This should be called before adf_cleanup_accel is called
	 */
	if (adf_devmgr_add_dev(accel_dev, NULL)) {
		device_printf(dev, "Failed to add new accelerator device.\n");
		return ENXIO;
	}

	/* Allocate and configure device configuration structure */
	hw_data = malloc(sizeof(*hw_data), M_QAT_C4XXX, M_WAITOK | M_ZERO);

	accel_dev->hw_device = hw_data;
	adf_init_hw_data_c4xxx(accel_dev->hw_device);
	accel_pci_dev->revid = pci_get_revid(dev);
	hw_data->fuses = pci_read_config(dev, ADF_DEVICE_FUSECTL_OFFSET, 4);

	/* Get PPAERUCM values and store */
	ret = adf_aer_store_ppaerucm_reg(dev, hw_data);
	if (ret)
		goto out_err;

	/* Get Accelerators and Accelerators Engines masks */
	hw_data->accel_mask = hw_data->get_accel_mask(accel_dev);
	hw_data->ae_mask = hw_data->get_ae_mask(accel_dev);
	hw_data->admin_ae_mask = hw_data->ae_mask;

	/* If the device has no acceleration engines then ignore it. */
	if (!hw_data->accel_mask || !hw_data->ae_mask ||
	    (~hw_data->ae_mask & 0x01)) {
		device_printf(dev, "No acceleration units found\n");
		ret = ENXIO;
		goto out_err;
	}

	/* Create device configuration table */
	ret = adf_cfg_dev_add(accel_dev);
	if (ret)
		goto out_err;

	ret = adf_clock_debugfs_add(accel_dev);
	if (ret)
		goto out_err;

	pci_set_max_read_req(dev, 1024);

	ret = bus_dma_tag_create(bus_get_dma_tag(dev),
				 1,
				 0,
				 BUS_SPACE_MAXADDR,
				 BUS_SPACE_MAXADDR,
				 NULL,
				 NULL,
				 BUS_SPACE_MAXSIZE,
				 /*BUS_SPACE_UNRESTRICTED*/ 1,
				 BUS_SPACE_MAXSIZE,
				 0,
				 NULL,
				 NULL,
				 &accel_dev->dma_tag);
	if (ret)
		goto out_err;

	if (hw_data->get_accel_cap) {
		hw_data->accel_capabilities_mask =
		    hw_data->get_accel_cap(accel_dev);
	}

	accel_pci_dev->sku = hw_data->get_sku(hw_data);

	/* Find and map all the device's BARS */
	i = 0;
	for (bar_nr = 0; i < ADF_PCI_MAX_BARS && bar_nr < PCIR_MAX_BAR_0;
	     bar_nr++) {
		struct adf_bar *bar;

		/*
		 * XXX: This isn't quite right as it will ignore a BAR
		 * that wasn't assigned a valid resource range by the
		 * firmware.
		 */
		rid = PCIR_BAR(bar_nr);
		if (bus_get_resource(dev, SYS_RES_MEMORY, rid, NULL, NULL) != 0)
			continue;
		bar = &accel_pci_dev->pci_bars[i++];
		bar->virt_addr = bus_alloc_resource_any(dev,
							SYS_RES_MEMORY,
							&rid,
							RF_ACTIVE);
		if (!bar->virt_addr) {
			device_printf(dev, "Failed to map BAR %d\n", bar_nr);
			ret = ENXIO;
			goto out_err;
		}
		bar->base_addr = rman_get_start(bar->virt_addr);
		bar->size = rman_get_start(bar->virt_addr);
	}
	pci_enable_busmaster(dev);

	if (!accel_dev->hw_device->config_device) {
		ret = EFAULT;
		goto out_err;
	}

	ret = accel_dev->hw_device->config_device(accel_dev);
	if (ret)
		goto out_err;

	ret = adf_dev_init(accel_dev);
	if (ret)
		goto out_dev_shutdown;

	ret = adf_dev_start(accel_dev);
	if (ret)
		goto out_dev_stop;

	cfg_dev = accel_dev->cfg->dev;
	adf_cfg_device_clear(cfg_dev, accel_dev);
	free(cfg_dev, M_QAT);
	accel_dev->cfg->dev = NULL;
	return ret;
out_dev_stop:
	adf_dev_stop(accel_dev);
out_dev_shutdown:
	adf_dev_shutdown(accel_dev);
out_err:
	adf_cleanup_accel(accel_dev);
	return ret;
}

static int
adf_detach(device_t dev)
{
	struct adf_accel_dev *accel_dev = device_get_softc(dev);

	if (adf_dev_stop(accel_dev)) {
		device_printf(dev, "Failed to stop QAT accel dev\n");
		return EBUSY;
	}

	adf_dev_shutdown(accel_dev);

	adf_cleanup_accel(accel_dev);

	return 0;
}

static device_method_t adf_methods[] = { DEVMETHOD(device_probe, adf_probe),
					 DEVMETHOD(device_attach, adf_attach),
					 DEVMETHOD(device_detach, adf_detach),

					 DEVMETHOD_END };

static driver_t adf_driver = { "qat",
			       adf_methods,
			       sizeof(struct adf_accel_dev) };

DRIVER_MODULE_ORDERED(qat_c4xxx, pci, adf_driver, NULL, NULL, SI_ORDER_THIRD);
MODULE_VERSION(qat_c4xxx, 1);
MODULE_DEPEND(qat_c4xxx, qat_common, 1, 1, 1);
MODULE_DEPEND(qat_c4xxx, qat_api, 1, 1, 1);
MODULE_DEPEND(qat_c4xxx, linuxkpi, 1, 1, 1);
