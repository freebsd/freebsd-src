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
#include <sys/bus.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <sys/systm.h>

#define ADF_PPAERUCM_MASK (BIT(14) | BIT(20) | BIT(22))

static struct workqueue_struct *fatal_error_wq;
struct adf_fatal_error_data {
	struct adf_accel_dev *accel_dev;
	struct work_struct work;
};

static struct workqueue_struct *device_reset_wq;

void
linux_complete_common(struct completion *c, int all)
{
	int wakeup_swapper;

	sleepq_lock(c);
	c->done++;
	if (all)
		wakeup_swapper = sleepq_broadcast(c, SLEEPQ_SLEEP, 0, 0);
	else
		wakeup_swapper = sleepq_signal(c, SLEEPQ_SLEEP, 0, 0);
	sleepq_release(c);
	if (wakeup_swapper)
		kick_proc0();
}

/* reset dev data */
struct adf_reset_dev_data {
	int mode;
	struct adf_accel_dev *accel_dev;
	struct completion compl;
	struct work_struct reset_work;
};

int
adf_aer_store_ppaerucm_reg(device_t dev, struct adf_hw_device_data *hw_data)
{
	unsigned int aer_offset, reg_val = 0;

	if (!hw_data)
		return -EINVAL;

	if (pci_find_extcap(dev, PCIZ_AER, &aer_offset) == 0) {
		reg_val =
		    pci_read_config(dev, aer_offset + PCIR_AER_UC_MASK, 4);

		hw_data->aerucm_mask = reg_val;
	} else {
		device_printf(dev,
			      "Unable to find AER capability of the device\n");
		return -ENODEV;
	}

	return 0;
}

void
adf_reset_sbr(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_to_pci_dev(accel_dev);
	device_t parent = device_get_parent(device_get_parent(pdev));
	uint16_t bridge_ctl = 0;

	if (accel_dev->is_vf)
		return;

	if (!parent)
		parent = pdev;

	if (!pcie_wait_for_pending_transactions(pdev, 0))
		device_printf(GET_DEV(accel_dev),
			      "Transaction still in progress. Proceeding\n");

	device_printf(GET_DEV(accel_dev), "Secondary bus reset\n");

	pci_save_state(pdev);
	bridge_ctl = pci_read_config(parent, PCIR_BRIDGECTL_1, 2);
	bridge_ctl |= PCIB_BCR_SECBUS_RESET;
	pci_write_config(parent, PCIR_BRIDGECTL_1, bridge_ctl, 2);
	pause_ms("adfrst", 100);
	bridge_ctl &= ~PCIB_BCR_SECBUS_RESET;
	pci_write_config(parent, PCIR_BRIDGECTL_1, bridge_ctl, 2);
	pause_ms("adfrst", 100);
	pci_restore_state(pdev);
}

void
adf_reset_flr(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_to_pci_dev(accel_dev);

	pci_save_state(pdev);
	if (pcie_flr(pdev,
		     max(pcie_get_max_completion_timeout(pdev) / 1000, 10),
		     true)) {
		pci_restore_state(pdev);
		return;
	}
	pci_restore_state(pdev);
	device_printf(GET_DEV(accel_dev),
		      "FLR qat_dev%d failed trying secondary bus reset\n",
		      accel_dev->accel_id);
	adf_reset_sbr(accel_dev);
}

void
adf_dev_pre_reset(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	device_t pdev = accel_to_pci_dev(accel_dev);
	u32 aer_offset, reg_val = 0;

	if (pci_find_extcap(pdev, PCIZ_AER, &aer_offset) == 0) {
		reg_val =
		    pci_read_config(pdev, aer_offset + PCIR_AER_UC_MASK, 4);
		reg_val |= ADF_PPAERUCM_MASK;
		pci_write_config(pdev,
				 aer_offset + PCIR_AER_UC_MASK,
				 reg_val,
				 4);
	} else {
		device_printf(pdev,
			      "Unable to find AER capability of the device\n");
	}

	if (hw_device->disable_arb) {
		device_printf(GET_DEV(accel_dev), "Disable arbiter.\n");
		hw_device->disable_arb(accel_dev);
	}
}

void
adf_dev_post_reset(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	device_t pdev = accel_to_pci_dev(accel_dev);
	u32 aer_offset;

	if (pci_find_extcap(pdev, PCIZ_AER, &aer_offset) == 0) {
		pci_write_config(pdev,
				 aer_offset + PCIR_AER_UC_MASK,
				 hw_device->aerucm_mask,
				 4);
	} else {
		device_printf(pdev,
			      "Unable to find AER capability of the device\n");
	}
}

void
adf_dev_restore(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	device_t pdev = accel_to_pci_dev(accel_dev);

	if (hw_device->pre_reset) {
		dev_dbg(GET_DEV(accel_dev), "Performing pre reset save\n");
		hw_device->pre_reset(accel_dev);
	}

	if (hw_device->reset_device) {
		device_printf(GET_DEV(accel_dev),
			      "Resetting device qat_dev%d\n",
			      accel_dev->accel_id);
		hw_device->reset_device(accel_dev);
		pci_restore_state(pdev);
		pci_save_state(pdev);
	}

	if (hw_device->post_reset) {
		dev_dbg(GET_DEV(accel_dev), "Performing post reset restore\n");
		hw_device->post_reset(accel_dev);
	}
}

static void
adf_device_reset_worker(struct work_struct *work)
{
	struct adf_reset_dev_data *reset_data =
	    container_of(work, struct adf_reset_dev_data, reset_work);
	struct adf_accel_dev *accel_dev = reset_data->accel_dev;

	if (adf_dev_restarting_notify(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Unable to send RESTARTING notification.\n");
		return;
	}

	if (adf_dev_stop(accel_dev)) {
		device_printf(GET_DEV(accel_dev), "Stopping device failed.\n");
		return;
	}

	adf_dev_shutdown(accel_dev);

	if (adf_dev_init(accel_dev) || adf_dev_start(accel_dev)) {
		/* The device hanged and we can't restart it */
		/* so stop here */
		device_printf(GET_DEV(accel_dev), "Restart device failed\n");
		if (reset_data->mode == ADF_DEV_RESET_ASYNC)
			kfree(reset_data);
		WARN(1, "QAT: device restart failed. Device is unusable\n");
		return;
	}

	adf_dev_restarted_notify(accel_dev);
	clear_bit(ADF_STATUS_RESTARTING, &accel_dev->status);

	/* The dev is back alive. Notify the caller if in sync mode */
	if (reset_data->mode == ADF_DEV_RESET_SYNC)
		complete(&reset_data->compl);
	else
		kfree(reset_data);
}

int
adf_dev_aer_schedule_reset(struct adf_accel_dev *accel_dev,
			   enum adf_dev_reset_mode mode)
{
	struct adf_reset_dev_data *reset_data;
	if (!adf_dev_started(accel_dev) ||
	    test_bit(ADF_STATUS_RESTARTING, &accel_dev->status))
		return 0;
	set_bit(ADF_STATUS_RESTARTING, &accel_dev->status);
	reset_data = kzalloc(sizeof(*reset_data), GFP_ATOMIC);
	if (!reset_data)
		return -ENOMEM;
	reset_data->accel_dev = accel_dev;
	init_completion(&reset_data->compl);
	reset_data->mode = mode;
	INIT_WORK(&reset_data->reset_work, adf_device_reset_worker);
	queue_work(device_reset_wq, &reset_data->reset_work);
	/* If in sync mode wait for the result */
	if (mode == ADF_DEV_RESET_SYNC) {
		int ret = 0;
		/* Maximum device reset time is 10 seconds */
		unsigned long wait_jiffies = msecs_to_jiffies(10000);
		unsigned long timeout =
		    wait_for_completion_timeout(&reset_data->compl,
						wait_jiffies);
		if (!timeout) {
			device_printf(GET_DEV(accel_dev),
				      "Reset device timeout expired\n");
			ret = -EFAULT;
		}
		kfree(reset_data);
		return ret;
	}
	return 0;
}

int
adf_dev_autoreset(struct adf_accel_dev *accel_dev)
{
	if (accel_dev->autoreset_on_error)
		return adf_dev_reset(accel_dev, ADF_DEV_RESET_ASYNC);
	return 0;
}

static void
adf_notify_fatal_error_work(struct work_struct *work)
{
	struct adf_fatal_error_data *wq_data =
	    container_of(work, struct adf_fatal_error_data, work);
	struct adf_accel_dev *accel_dev = wq_data->accel_dev;

	adf_error_notifier((uintptr_t)accel_dev);
	if (!accel_dev->is_vf) {
		adf_dev_autoreset(accel_dev);
	}

	kfree(wq_data);
}

int
adf_notify_fatal_error(struct adf_accel_dev *accel_dev)
{
	struct adf_fatal_error_data *wq_data;

	wq_data = kzalloc(sizeof(*wq_data), GFP_ATOMIC);
	if (!wq_data) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to allocate memory\n");
		return ENOMEM;
	}
	wq_data->accel_dev = accel_dev;

	INIT_WORK(&wq_data->work, adf_notify_fatal_error_work);
	queue_work(fatal_error_wq, &wq_data->work);

	return 0;
}

int __init
adf_init_fatal_error_wq(void)
{
	fatal_error_wq = create_workqueue("qat_fatal_error_wq");
	return !fatal_error_wq ? EFAULT : 0;
}

void
adf_exit_fatal_error_wq(void)
{
	if (fatal_error_wq)
		destroy_workqueue(fatal_error_wq);
	fatal_error_wq = NULL;
}

int
adf_init_aer(void)
{
	device_reset_wq = create_workqueue("qat_device_reset_wq");
	return !device_reset_wq ? -EFAULT : 0;
}

void
adf_exit_aer(void)
{
	if (device_reset_wq)
		destroy_workqueue(device_reset_wq);
	device_reset_wq = NULL;
}
