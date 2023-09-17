/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_freebsd.h"
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/cdefs.h>
#include <sys/types.h>
#include <sys/interrupt.h>
#include <dev/pci/pcivar.h>
#include <sys/param.h>
#include <linux/workqueue.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_cfg.h"
#include "adf_cfg_strings.h"
#include "adf_cfg_common.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include "adf_pfvf_utils.h"

static TASKQUEUE_DEFINE_THREAD(qat_vf);
static TASKQUEUE_DEFINE_THREAD(qat_bank_handler);

static struct workqueue_struct *adf_vf_stop_wq;
static DEFINE_MUTEX(vf_stop_wq_lock);

struct adf_vf_stop_data {
	struct adf_accel_dev *accel_dev;
	struct work_struct work;
};

static int
adf_enable_msi(struct adf_accel_dev *accel_dev)
{
	int stat;
	int count = 1;
	stat = pci_alloc_msi(accel_to_pci_dev(accel_dev), &count);
	if (stat) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to enable MSI interrupts\n");
		return stat;
	}

	return stat;
}

static void
adf_disable_msi(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_to_pci_dev(accel_dev);
	pci_release_msi(pdev);
}

static void
adf_dev_stop_async(struct work_struct *work)
{
	struct adf_vf_stop_data *stop_data =
	    container_of(work, struct adf_vf_stop_data, work);
	struct adf_accel_dev *accel_dev = stop_data->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	adf_dev_restarting_notify(accel_dev);
	adf_dev_stop(accel_dev);
	adf_dev_shutdown(accel_dev);

	/* Re-enable PF2VF interrupts */
	hw_data->enable_pf2vf_interrupt(accel_dev);
	kfree(stop_data);
}

int
adf_pf2vf_handle_pf_restarting(struct adf_accel_dev *accel_dev)
{
	struct adf_vf_stop_data *stop_data;

	clear_bit(ADF_STATUS_PF_RUNNING, &accel_dev->status);
	stop_data = kzalloc(sizeof(*stop_data), GFP_ATOMIC);
	if (!stop_data) {
		device_printf(GET_DEV(accel_dev),
			      "Couldn't schedule stop for vf_%d\n",
			      accel_dev->accel_id);
		return -ENOMEM;
	}
	stop_data->accel_dev = accel_dev;
	INIT_WORK(&stop_data->work, adf_dev_stop_async);
	queue_work(adf_vf_stop_wq, &stop_data->work);

	return 0;
}

int
adf_pf2vf_handle_pf_rp_reset(struct adf_accel_dev *accel_dev,
			     struct pfvf_message msg)
{
	accel_dev->u1.vf.rpreset_sts = msg.data;
	if (accel_dev->u1.vf.rpreset_sts == RPRESET_SUCCESS)
		device_printf(
		    GET_DEV(accel_dev),
		    "rpreset resp(success) from PF type:0x%x data:0x%x\n",
		    msg.type,
		    msg.data);
	else if (accel_dev->u1.vf.rpreset_sts == RPRESET_NOT_SUPPORTED)
		device_printf(
		    GET_DEV(accel_dev),
		    "rpreset resp(not supported) from PF type:0x%x data:0x%x\n",
		    msg.type,
		    msg.data);
	else if (accel_dev->u1.vf.rpreset_sts == RPRESET_INVAL_BANK)
		device_printf(
		    GET_DEV(accel_dev),
		    "rpreset resp(invalid bank) from PF type:0x%x data:0x%x\n",
		    msg.type,
		    msg.data);
	else
		device_printf(
		    GET_DEV(accel_dev),
		    "rpreset resp(timeout) from PF type:0x%x data:0x%x\nn",
		    msg.type,
		    msg.data);

	complete(&accel_dev->u1.vf.msg_received);

	return 0;
}

static void
adf_pf2vf_bh_handler(void *data, int pending)
{
	struct adf_accel_dev *accel_dev = data;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	if (adf_recv_and_handle_pf2vf_msg(accel_dev))
		/* Re-enable PF2VF interrupts */
		hw_data->enable_pf2vf_interrupt(accel_dev);

	return;
}

static int
adf_setup_pf2vf_bh(struct adf_accel_dev *accel_dev)
{
	TASK_INIT(&accel_dev->u1.vf.pf2vf_bh_tasklet,
		  0,
		  adf_pf2vf_bh_handler,
		  accel_dev);
	mutex_init(&accel_dev->u1.vf.vf2pf_lock);

	return 0;
}

static void
adf_cleanup_pf2vf_bh(struct adf_accel_dev *accel_dev)
{
	taskqueue_cancel(taskqueue_qat_vf,
			 &accel_dev->u1.vf.pf2vf_bh_tasklet,
			 NULL);
	taskqueue_drain(taskqueue_qat_vf, &accel_dev->u1.vf.pf2vf_bh_tasklet);
	mutex_destroy(&accel_dev->u1.vf.vf2pf_lock);
}

static void
adf_bh_handler(void *data, int pending)
{
	struct adf_etr_bank_data *bank = (void *)data;

	adf_response_handler((uintptr_t)bank);

	return;
}

static int
adf_setup_bh(struct adf_accel_dev *accel_dev)
{
	int i = 0;
	struct adf_etr_data *priv_data = accel_dev->transport;

	for (i = 0; i < GET_MAX_BANKS(accel_dev); i++) {
		TASK_INIT(&priv_data->banks[i].resp_handler,
			  0,
			  adf_bh_handler,
			  &priv_data->banks[i]);
	}

	return 0;
}

static void
adf_cleanup_bh(struct adf_accel_dev *accel_dev)
{
	int i = 0;
	struct adf_etr_data *transport;

	if (!accel_dev || !accel_dev->transport)
		return;

	transport = accel_dev->transport;
	for (i = 0; i < GET_MAX_BANKS(accel_dev); i++) {
		taskqueue_cancel(taskqueue_qat_bank_handler,
				 &transport->banks[i].resp_handler,
				 NULL);
		taskqueue_drain(taskqueue_qat_bank_handler,
				&transport->banks[i].resp_handler);
	}
}

static void
adf_isr(void *privdata)
{
	struct adf_accel_dev *accel_dev = privdata;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_hw_csr_ops *csr_ops = &hw_data->csr_info.csr_ops;
	int int_active_bundles = 0;
	int i = 0;

	/* Check for PF2VF interrupt */
	if (hw_data->interrupt_active_pf2vf(accel_dev)) {
		/* Disable PF to VF interrupt */
		hw_data->disable_pf2vf_interrupt(accel_dev);
		/* Schedule tasklet to handle interrupt BH */
		taskqueue_enqueue(taskqueue_qat_vf,
				  &accel_dev->u1.vf.pf2vf_bh_tasklet);
	}

	if (hw_data->get_int_active_bundles)
		int_active_bundles = hw_data->get_int_active_bundles(accel_dev);

	for (i = 0; i < GET_MAX_BANKS(accel_dev); i++) {
		if (int_active_bundles & BIT(i)) {
			struct adf_etr_data *etr_data = accel_dev->transport;
			struct adf_etr_bank_data *bank = &etr_data->banks[i];

			/* Disable Flag and Coalesce Ring Interrupts */
			csr_ops->write_csr_int_flag_and_col(bank->csr_addr,
							    bank->bank_number,
							    0);
			/* Schedule tasklet to handle interrupt BH */
			taskqueue_enqueue(taskqueue_qat_bank_handler,
					  &bank->resp_handler);
		}
	}
}

static int
adf_request_msi_irq(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_to_pci_dev(accel_dev);
	int ret;
	int rid = 1;
	int cpu;

	accel_dev->u1.vf.irq =
	    bus_alloc_resource_any(pdev, SYS_RES_IRQ, &rid, RF_ACTIVE);
	if (accel_dev->u1.vf.irq == NULL) {
		device_printf(GET_DEV(accel_dev), "failed to allocate IRQ\n");
		return ENXIO;
	}
	ret = bus_setup_intr(pdev,
			     accel_dev->u1.vf.irq,
			     INTR_TYPE_MISC | INTR_MPSAFE,
			     NULL,
			     adf_isr,
			     accel_dev,
			     &accel_dev->u1.vf.cookie);
	if (ret) {
		device_printf(GET_DEV(accel_dev), "failed to enable irq\n");
		goto errout;
	}

	cpu = accel_dev->accel_id % num_online_cpus();
	ret = bus_bind_intr(pdev, accel_dev->u1.vf.irq, cpu);
	if (ret) {
		device_printf(GET_DEV(accel_dev),
			      "failed to bind IRQ handler to cpu core\n");
		goto errout;
	}
	accel_dev->u1.vf.irq_enabled = true;

	return ret;
errout:
	bus_free_resource(pdev, SYS_RES_IRQ, accel_dev->u1.vf.irq);

	return ret;
}

/**
 * adf_vf_isr_resource_free() - Free IRQ for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function frees interrupts for acceleration device virtual function.
 */
void
adf_vf_isr_resource_free(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_to_pci_dev(accel_dev);

	if (accel_dev->u1.vf.irq_enabled) {
		bus_teardown_intr(pdev,
				  accel_dev->u1.vf.irq,
				  accel_dev->u1.vf.cookie);
		bus_free_resource(pdev, SYS_RES_IRQ, accel_dev->u1.vf.irq);
	}
	adf_cleanup_bh(accel_dev);
	adf_cleanup_pf2vf_bh(accel_dev);
	adf_disable_msi(accel_dev);
}

/**
 * adf_vf_isr_resource_alloc() - Allocate IRQ for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function allocates interrupts for acceleration device virtual function.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_vf_isr_resource_alloc(struct adf_accel_dev *accel_dev)
{
	if (adf_enable_msi(accel_dev))
		goto err_out;

	if (adf_setup_pf2vf_bh(accel_dev))
		goto err_disable_msi;

	if (adf_setup_bh(accel_dev))
		goto err_out;

	if (adf_request_msi_irq(accel_dev))
		goto err_disable_msi;

	return 0;

err_disable_msi:
	adf_disable_msi(accel_dev);

err_out:
	return -EFAULT;
}

/**
 * adf_flush_vf_wq() - Flush workqueue for VF
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function disables the PF/VF interrupts on the VF so that no new messages
 * are received and flushes the workqueue 'adf_vf_stop_wq'.
 *
 * Return: void.
 */
void
adf_flush_vf_wq(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	hw_data->disable_pf2vf_interrupt(accel_dev);

	if (adf_vf_stop_wq)
		flush_workqueue(adf_vf_stop_wq);
}

/**
 * adf_init_vf_wq() - Init workqueue for VF
 *
 * Function init workqueue 'adf_vf_stop_wq' for VF.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_init_vf_wq(void)
{
	int ret = 0;

	mutex_lock(&vf_stop_wq_lock);
	if (!adf_vf_stop_wq)
		adf_vf_stop_wq =
		    alloc_workqueue("adf_vf_stop_wq", WQ_MEM_RECLAIM, 0);

	if (!adf_vf_stop_wq)
		ret = ENOMEM;

	mutex_unlock(&vf_stop_wq_lock);
	return ret;
}

void
adf_exit_vf_wq(void)
{
	if (adf_vf_stop_wq)
		destroy_workqueue(adf_vf_stop_wq);

	adf_vf_stop_wq = NULL;
}
