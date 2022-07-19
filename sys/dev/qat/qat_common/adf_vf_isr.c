/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "qat_freebsd.h"
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/cdefs.h>
#include <sys/types.h>
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
#include "adf_pf2vf_msg.h"

#define ADF_VINTSOU_BUN BIT(0)
#define ADF_VINTSOU_PF2VF BIT(1)

static TASKQUEUE_DEFINE_THREAD(qat_vf);

static struct workqueue_struct *adf_vf_stop_wq;
static DEFINE_MUTEX(vf_stop_wq_lock);

struct adf_vf_stop_data {
	struct adf_accel_dev *accel_dev;
	struct work_struct vf_stop_work;
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
	    container_of(work, struct adf_vf_stop_data, vf_stop_work);
	struct adf_accel_dev *accel_dev = stop_data->accel_dev;

	adf_dev_restarting_notify(accel_dev);
	adf_dev_stop(accel_dev);
	adf_dev_shutdown(accel_dev);

	/* Re-enable PF2VF interrupts */
	adf_enable_pf2vf_interrupts(accel_dev);
	kfree(stop_data);
}

static void
adf_pf2vf_bh_handler(void *data, int pending)
{
	struct adf_accel_dev *accel_dev = data;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *pmisc =
	    &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	struct resource *pmisc_bar_addr = pmisc->virt_addr;
	u32 msg;
	bool is_notification = false;

	/* Read the message from PF */
	msg = ADF_CSR_RD(pmisc_bar_addr, hw_data->get_pf2vf_offset(0));
	if (!(msg & ADF_PF2VF_INT)) {
		device_printf(GET_DEV(accel_dev),
			      "Spurious PF2VF interrupt. msg %X. Ignored\n",
			      msg);
		accel_dev->u1.vf.pfvf_counters.spurious++;
		goto out;
	}
	accel_dev->u1.vf.pfvf_counters.rx++;

	if (!(msg & ADF_PF2VF_MSGORIGIN_SYSTEM)) {
		device_printf(GET_DEV(accel_dev),
			      "Ignore non-system PF2VF message(0x%x)\n",
			      msg);
		/*
		 * To ack, clear the VF2PFINT bit.
		 * Because this must be a legacy message, the far side
		 * must clear the in-use pattern.
		 */
		msg &= ~ADF_PF2VF_INT;
		ADF_CSR_WR(pmisc_bar_addr, hw_data->get_pf2vf_offset(0), msg);
		goto out;
	}

	switch ((msg & ADF_PF2VF_MSGTYPE_MASK) >> ADF_PF2VF_MSGTYPE_SHIFT) {
	case ADF_PF2VF_MSGTYPE_RESTARTING: {
		struct adf_vf_stop_data *stop_data;

		is_notification = true;

		device_printf(GET_DEV(accel_dev),
			      "Restarting msg received from PF 0x%x\n",
			      msg);

		clear_bit(ADF_STATUS_PF_RUNNING, &accel_dev->status);
		stop_data = kzalloc(sizeof(*stop_data), GFP_ATOMIC);
		if (!stop_data) {
			device_printf(GET_DEV(accel_dev),
				      "Couldn't schedule stop for vf_%d\n",
				      accel_dev->accel_id);
			goto out;
		}
		stop_data->accel_dev = accel_dev;
		INIT_WORK(&stop_data->vf_stop_work, adf_dev_stop_async);
		queue_work(adf_vf_stop_wq, &stop_data->vf_stop_work);
		break;
	}
	case ADF_PF2VF_MSGTYPE_VERSION_RESP:
		device_printf(GET_DEV(accel_dev),
			      "Version resp received from PF 0x%x\n",
			      msg);
		is_notification = false;
		accel_dev->u1.vf.pf_version =
		    (msg & ADF_PF2VF_VERSION_RESP_VERS_MASK) >>
		    ADF_PF2VF_VERSION_RESP_VERS_SHIFT;
		accel_dev->u1.vf.compatible =
		    (msg & ADF_PF2VF_VERSION_RESP_RESULT_MASK) >>
		    ADF_PF2VF_VERSION_RESP_RESULT_SHIFT;
		accel_dev->u1.vf.iov_msg_completion = 1;
		wakeup(&accel_dev->u1.vf.iov_msg_completion);
		break;
	case ADF_PF2VF_MSGTYPE_BLOCK_RESP:
		is_notification = false;
		accel_dev->u1.vf.pf2vf_block_byte =
		    (msg & ADF_PF2VF_BLOCK_RESP_DATA_MASK) >>
		    ADF_PF2VF_BLOCK_RESP_DATA_SHIFT;
		accel_dev->u1.vf.pf2vf_block_resp_type =
		    (msg & ADF_PF2VF_BLOCK_RESP_TYPE_MASK) >>
		    ADF_PF2VF_BLOCK_RESP_TYPE_SHIFT;
		accel_dev->u1.vf.iov_msg_completion = 1;
		wakeup(&accel_dev->u1.vf.iov_msg_completion);
		break;
	case ADF_PF2VF_MSGTYPE_FATAL_ERROR:
		device_printf(GET_DEV(accel_dev),
			      "Fatal error received from PF 0x%x\n",
			      msg);
		is_notification = true;
		if (adf_notify_fatal_error(accel_dev))
			device_printf(GET_DEV(accel_dev),
				      "Couldn't notify fatal error\n");
		break;
	default:
		device_printf(GET_DEV(accel_dev),
			      "Unknown PF2VF message(0x%x)\n",
			      msg);
	}

	/* To ack, clear the PF2VFINT bit */
	msg &= ~ADF_PF2VF_INT;
	/*
	 * Clear the in-use pattern if the sender won't do it.
	 * Because the compatibility version must be the first message
	 * exchanged between the VF and PF, the pf.version must be
	 * set at this time.
	 * The in-use pattern is not cleared for notifications so that
	 * it can be used for collision detection.
	 */
	if (accel_dev->u1.vf.pf_version >= ADF_PFVF_COMPATIBILITY_FAST_ACK &&
	    !is_notification)
		msg &= ~ADF_PF2VF_IN_USE_BY_PF_MASK;
	ADF_CSR_WR(pmisc_bar_addr, hw_data->get_pf2vf_offset(0), msg);

out:
	/* Re-enable PF2VF interrupts */
	adf_enable_pf2vf_interrupts(accel_dev);
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
adf_isr(void *privdata)
{
	struct adf_accel_dev *accel_dev = privdata;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *pmisc =
	    &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	struct resource *pmisc_bar_addr = pmisc->virt_addr;
	u32 v_int, v_mask;
	int handled = 0;

	/* Read VF INT source CSR to determine the source of VF interrupt */
	v_int = ADF_CSR_RD(pmisc_bar_addr, hw_data->get_vintsou_offset());
	v_mask = ADF_CSR_RD(pmisc_bar_addr, hw_data->get_vintmsk_offset(0));

	/* Check for PF2VF interrupt */
	if ((v_int & ~v_mask) & ADF_VINTSOU_PF2VF) {
		/* Disable PF to VF interrupt */
		adf_disable_pf2vf_interrupts(accel_dev);

		/* Schedule tasklet to handle interrupt BH */
		taskqueue_enqueue(taskqueue_qat_vf,
				  &accel_dev->u1.vf.pf2vf_bh_tasklet);
		handled = 1;
	}

	if ((v_int & ~v_mask) & ADF_VINTSOU_BUN) {
		struct adf_etr_data *etr_data = accel_dev->transport;
		struct adf_etr_bank_data *bank = &etr_data->banks[0];

		/* Disable Flag and Coalesce Ring Interrupts */
		WRITE_CSR_INT_FLAG_AND_COL(bank->csr_addr,
					   bank->bank_number,
					   0);
		adf_response_handler((uintptr_t)&etr_data->banks[0]);
		handled = 1;
	}

	if (handled)
		return;
}

static int
adf_request_msi_irq(struct adf_accel_dev *accel_dev)
{
	device_t pdev = accel_to_pci_dev(accel_dev);
	int ret;
	int rid = 1;
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
		device_printf(GET_DEV(accel_dev),
			      "failed to enable irq for %s\n",
			      accel_dev->u1.vf.irq_name);
		return ret;
	}
	return ret;
}

static int
adf_setup_bh(struct adf_accel_dev *accel_dev)
{
	return 0;
}

static void
adf_cleanup_bh(struct adf_accel_dev *accel_dev)
{
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
	bus_teardown_intr(pdev, accel_dev->u1.vf.irq, accel_dev->u1.vf.cookie);
	bus_free_resource(pdev, SYS_RES_IRQ, accel_dev->u1.vf.irq);
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
		goto err_out;

	if (adf_setup_bh(accel_dev))
		goto err_out;

	if (adf_request_msi_irq(accel_dev))
		goto err_out;

	return 0;
err_out:
	adf_vf_isr_resource_free(accel_dev);
	return EFAULT;
}

/**
 * adf_flush_vf_wq() - Flush workqueue for VF
 *
 * Function flushes workqueue 'adf_vf_stop_wq' for VF.
 *
 * Return: void.
 */
void
adf_flush_vf_wq(void)
{
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

/**
 * adf_exit_vf_wq() - Destroy workqueue for VF
 *
 * Function destroy workqueue 'adf_vf_stop_wq' for VF.
 *
 * Return: void.
 */
void
adf_exit_vf_wq(void)
{
	if (adf_vf_stop_wq) {
		destroy_workqueue(adf_vf_stop_wq);
		adf_vf_stop_wq = NULL;
	}
}
