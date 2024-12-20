/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_freebsd.h"
#include <adf_accel_devices.h>
#include <adf_common_drv.h>
#include <adf_cfg.h>
#include "adf_4xxxvf_hw_data.h"
#include "adf_gen4_hw_data.h"
#include "adf_fw_counters.h"
#include "adf_cfg_device.h"
#include <sys/types.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <machine/bus_dma.h>
#include <dev/pci/pcireg.h>

static MALLOC_DEFINE(M_QAT_4XXXVF, "qat_4xxxvf", "qat_4xxxvf");

#define ADF_SYSTEM_DEVICE(device_id)                                           \
	{                                                                      \
		PCI_VENDOR_ID_INTEL, device_id                                 \
	}

static const struct pci_device_id adf_pci_tbl[] =
    { ADF_SYSTEM_DEVICE(ADF_4XXXIOV_PCI_DEVICE_ID),
      ADF_SYSTEM_DEVICE(ADF_401XXIOV_PCI_DEVICE_ID),
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
					"Intel " ADF_4XXXVF_DEVICE_NAME
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
	struct adf_accel_dev *pf;
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

	/*
	 * As adf_clean_hw_data_4xxxiov() will update class index, before
	 * index is updated, vf must be remove from accel_table.
	 */
	pf = adf_devmgr_pci_to_accel_dev(pci_find_pf(accel_pci_dev->pci_dev));
	adf_devmgr_rm_dev(accel_dev, pf);

	if (accel_dev->hw_device) {
		switch (pci_get_device(accel_pci_dev->pci_dev)) {
		case ADF_4XXXIOV_PCI_DEVICE_ID:
		case ADF_401XXIOV_PCI_DEVICE_ID:
			adf_clean_hw_data_4xxxiov(accel_dev->hw_device);
			break;
		default:
			break;
		}
		free(accel_dev->hw_device, M_QAT_4XXXVF);
		accel_dev->hw_device = NULL;
	}
	adf_cfg_dev_remove(accel_dev);
}

static int
adf_attach(device_t dev)
{
	struct adf_accel_dev *accel_dev;
	struct adf_accel_dev *pf;
	struct adf_accel_pci *accel_pci_dev;
	struct adf_hw_device_data *hw_data;
	unsigned int bar_nr;
	int ret = 0;
	int rid;
	struct adf_cfg_device *cfg_dev = NULL;

	accel_dev = device_get_softc(dev);
	accel_dev->is_vf = true;
	pf = adf_devmgr_pci_to_accel_dev(pci_find_pf(dev));

	INIT_LIST_HEAD(&accel_dev->crypto_list);
	accel_pci_dev = &accel_dev->accel_pci_dev;
	accel_pci_dev->pci_dev = dev;

	if (bus_get_domain(dev, &accel_pci_dev->node) != 0)
		accel_pci_dev->node = 0;

	/* Add accel device to accel table */
	if (adf_devmgr_add_dev(accel_dev, pf)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to add new accelerator device.\n");
		return -EFAULT;
	}
	/* Allocate and configure device configuration structure */
	hw_data = malloc(sizeof(*hw_data), M_QAT_4XXXVF, M_WAITOK | M_ZERO);
	accel_dev->hw_device = hw_data;
	adf_init_hw_data_4xxxiov(accel_dev->hw_device);
	accel_pci_dev->revid = pci_get_revid(dev);

	hw_data->fuses = pci_read_config(dev, ADF_4XXXIOV_VFFUSECTL4_OFFSET, 4);

	/* Get Accelerators and Accelerators Engines masks */
	hw_data->accel_mask = hw_data->get_accel_mask(accel_dev);
	hw_data->ae_mask = hw_data->get_ae_mask(accel_dev);
	hw_data->admin_ae_mask = hw_data->ae_mask;
	accel_pci_dev->sku = hw_data->get_sku(hw_data);

	/* Create device configuration table */
	ret = adf_cfg_dev_add(accel_dev);
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
				 /* BUS_SPACE_UNRESTRICTED */ 1,
				 BUS_SPACE_MAXSIZE,
				 0,
				 NULL,
				 NULL,
				 &accel_dev->dma_tag);

	hw_data->accel_capabilities_mask = adf_4xxxvf_get_hw_cap(accel_dev);

	/* Find and map all the device's BARS */
	/* Logical BARs configuration for 64bit BARs:
	     bar 0 and 1 - logical BAR0
	     bar 2 and 3 - logical BAR1
	     bar 4 and 5 - logical BAR3
	*/
	for (bar_nr = 0;
	     bar_nr < (ADF_PCI_MAX_BARS * 2) && bar_nr < PCIR_MAX_BAR_0;
	     bar_nr += 2) {
		struct adf_bar *bar;

		rid = PCIR_BAR(bar_nr);
		bar = &accel_pci_dev->pci_bars[bar_nr / 2];
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
		bar->size = rman_get_size(bar->virt_addr);
	}
	pci_enable_busmaster(dev);

	/* Completion for VF2PF request/response message exchange */
	init_completion(&accel_dev->u1.vf.msg_received);
	mutex_init(&accel_dev->u1.vf.rpreset_lock);

	ret = hw_data->config_device(accel_dev);
	if (ret)
		goto out_err;

	ret = adf_dev_init(accel_dev);
	if (!ret)
		ret = adf_dev_start(accel_dev);

	if (ret) {
		device_printf(
		    GET_DEV(accel_dev),
		    "Failed to start - make sure PF enabled services match VF configuration.\n");
		adf_dev_stop(accel_dev);
		adf_dev_shutdown(accel_dev);
		return 0;
	}

	cfg_dev = accel_dev->cfg->dev;
	adf_cfg_device_clear(cfg_dev, accel_dev);
	free(cfg_dev, M_QAT);
	accel_dev->cfg->dev = NULL;

	return ret;

out_err:
	adf_cleanup_accel(accel_dev);
	return ret;
}

static int
adf_detach(device_t dev)
{
	struct adf_accel_dev *accel_dev = device_get_softc(dev);

	if (!accel_dev) {
		printf("QAT: Driver removal failed\n");
		return EFAULT;
	}

	adf_flush_vf_wq(accel_dev);
	clear_bit(ADF_STATUS_RESTARTING, &accel_dev->status);
	adf_dev_stop(accel_dev);
	adf_dev_shutdown(accel_dev);
	adf_cleanup_accel(accel_dev);
	return 0;
}

static int
adf_modevent(module_t mod, int type, void *data)
{

	switch (type) {
	case MOD_UNLOAD:
		adf_clean_vf_map(true);
		return 0;
	default:
		return EOPNOTSUPP;
	}
}

static device_method_t adf_methods[] = { DEVMETHOD(device_probe, adf_probe),
					 DEVMETHOD(device_attach, adf_attach),
					 DEVMETHOD(device_detach, adf_detach),

					 DEVMETHOD_END };

static driver_t adf_driver = { "qat",
			       adf_methods,
			       sizeof(struct adf_accel_dev) };

DRIVER_MODULE_ORDERED(qat_4xxxvf,
		      pci,
		      adf_driver,
		      adf_modevent,
		      NULL,
		      SI_ORDER_THIRD);
MODULE_VERSION(qat_4xxxvf, 1);
MODULE_DEPEND(qat_4xxxvf, qat_common, 1, 1, 1);
MODULE_DEPEND(qat_4xxxvf, qat_api, 1, 1, 1);
MODULE_DEPEND(qat_4xxxvf, linuxkpi, 1, 1, 1);
