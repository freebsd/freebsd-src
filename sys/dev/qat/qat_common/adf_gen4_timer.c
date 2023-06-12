/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#include "adf_accel_devices.h"
#include "adf_heartbeat.h"
#include "adf_common_drv.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_gen4_timer.h"

#include "adf_dev_err.h"

#define ADF_GEN4_INT_TIMER_VALUE_IN_MS 200
/* Interval within timer interrupt. Value in miliseconds. */

#define ADF_GEN4_MAX_INT_TIMER_VALUE_IN_MS 0xFFFFFFFF
/* MAX Interval within timer interrupt. Value in miliseconds. */

static u64
adf_get_next_timeout(u32 timeout_val)
{
	u64 timeout = msecs_to_jiffies(timeout_val);

	return rounddown(jiffies + timeout, timeout);
}

static void
adf_hb_irq_bh_handler(struct work_struct *work)
{
	struct icp_qat_fw_init_admin_req req = { 0 };
	struct icp_qat_fw_init_admin_resp resp = { 0 };
	struct adf_hb_timer_data *hb_timer_data =
	    container_of(work, struct adf_hb_timer_data, hb_int_timer_work);
	struct adf_accel_dev *accel_dev = hb_timer_data->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 ae_mask = hw_data->ae_mask;

	if (!accel_dev->int_timer || !accel_dev->int_timer->enabled)
		goto end;

	/* Update heartbeat count via init/admin cmd */
	if (!accel_dev->admin) {
		device_printf(GET_DEV(accel_dev),
			      "adf_admin is not available\n");
		goto end;
	}

	req.cmd_id = ICP_QAT_FW_HEARTBEAT_SYNC;
	req.heartbeat_ticks = accel_dev->int_timer->int_cnt;

	if (adf_send_admin(accel_dev, &req, &resp, ae_mask))
		device_printf(GET_DEV(accel_dev),
			      "Failed to update qat's HB count\n");

end:
	kfree(hb_timer_data);
}

static void
timer_handler(struct timer_list *tl)
{
	struct adf_int_timer *int_timer = from_timer(int_timer, tl, timer);
	struct adf_accel_dev *accel_dev = int_timer->accel_dev;
	struct adf_hb_timer_data *hb_timer_data = NULL;
	u64 timeout_val = adf_get_next_timeout(int_timer->timeout_val);
	/* Update TL TBD */

	/* Schedule a heartbeat work queue to update HB */
	hb_timer_data = kzalloc(sizeof(*hb_timer_data), GFP_ATOMIC);
	if (hb_timer_data) {
		hb_timer_data->accel_dev = accel_dev;

		INIT_WORK(&hb_timer_data->hb_int_timer_work,
			  adf_hb_irq_bh_handler);
		queue_work(int_timer->timer_irq_wq,
			   &hb_timer_data->hb_int_timer_work);
	} else {
		device_printf(GET_DEV(accel_dev),
			      "Failed to alloc heartbeat timer data\n");
	}
	int_timer->int_cnt++;
	mod_timer(tl, timeout_val);
}

int
adf_int_timer_init(struct adf_accel_dev *accel_dev)
{
	u64 timeout_val = adf_get_next_timeout(ADF_GEN4_INT_TIMER_VALUE_IN_MS);
	struct adf_int_timer *int_timer = NULL;
	char wqname[32] = { 0 };

	if (!accel_dev)
		return 0;

	int_timer = kzalloc(sizeof(*int_timer), GFP_KERNEL);
	if (!int_timer)
		return -ENOMEM;

	sprintf(wqname, "qat_timer_wq_%d", accel_dev->accel_id);

	int_timer->timer_irq_wq = alloc_workqueue(wqname, WQ_MEM_RECLAIM, 1);

	if (!int_timer->timer_irq_wq) {
		kfree(int_timer);
		return -ENOMEM;
	}

	int_timer->accel_dev = accel_dev;
	int_timer->timeout_val = ADF_GEN4_INT_TIMER_VALUE_IN_MS;
	int_timer->int_cnt = 0;
	int_timer->enabled = true;
	accel_dev->int_timer = int_timer;

	timer_setup(&int_timer->timer, timer_handler, 0);
	mod_timer(&int_timer->timer, timeout_val);

	return 0;
}

void
adf_int_timer_exit(struct adf_accel_dev *accel_dev)
{
	if (accel_dev && accel_dev->int_timer) {
		del_timer_sync(&accel_dev->int_timer->timer);
		accel_dev->int_timer->enabled = false;

		if (accel_dev->int_timer->timer_irq_wq) {
			flush_workqueue(accel_dev->int_timer->timer_irq_wq);
			destroy_workqueue(accel_dev->int_timer->timer_irq_wq);
		}

		kfree(accel_dev->int_timer);
		accel_dev->int_timer = NULL;
	}
}
