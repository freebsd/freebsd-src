/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include <sys/types.h>
#include <linux/random.h>
#include "qat_freebsd.h"

#include "adf_heartbeat.h"
#include "adf_common_drv.h"
#include "adf_cfg.h"
#include "adf_cfg_strings.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_transport_internal.h"

#define MAX_HB_TICKS 0xFFFFFFFF

static int
adf_check_hb_poll_freq(struct adf_accel_dev *accel_dev)
{
	u64 curr_hb_check_time = 0;
	char timer_str[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	unsigned int timer_val = ADF_CFG_HB_DEFAULT_VALUE;

	curr_hb_check_time = adf_clock_get_current_time();

	if (!adf_cfg_get_param_value(accel_dev,
				     ADF_GENERAL_SEC,
				     ADF_HEARTBEAT_TIMER,
				     (char *)timer_str)) {
		if (compat_strtouint((char *)timer_str,
				     ADF_CFG_BASE_DEC,
				     &timer_val))
			timer_val = ADF_CFG_HB_DEFAULT_VALUE;
	}
	if ((curr_hb_check_time - accel_dev->heartbeat->last_hb_check_time) <
	    timer_val) {
		return EINVAL;
	}
	accel_dev->heartbeat->last_hb_check_time = curr_hb_check_time;

	return 0;
}

int
adf_heartbeat_init(struct adf_accel_dev *accel_dev)
{
	if (accel_dev->heartbeat)
		adf_heartbeat_clean(accel_dev);

	accel_dev->heartbeat =
	    malloc(sizeof(*accel_dev->heartbeat), M_QAT, M_WAITOK | M_ZERO);

	return 0;
}

void
adf_heartbeat_clean(struct adf_accel_dev *accel_dev)
{
	free(accel_dev->heartbeat, M_QAT);
	accel_dev->heartbeat = NULL;
}

int
adf_get_hb_timer(struct adf_accel_dev *accel_dev, unsigned int *value)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	char timer_str[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };
	unsigned int timer_val = ADF_CFG_HB_DEFAULT_VALUE;
	u32 clk_per_sec = 0;

	/* HB clock may be different than AE clock */
	if (hw_data->get_hb_clock) {
		clk_per_sec = (u32)hw_data->get_hb_clock(hw_data);
	} else if (hw_data->get_ae_clock) {
		clk_per_sec = (u32)hw_data->get_ae_clock(hw_data);
	} else {
		return EINVAL;
	}

	/* Get Heartbeat Timer value from the configuration */
	if (!adf_cfg_get_param_value(accel_dev,
				     ADF_GENERAL_SEC,
				     ADF_HEARTBEAT_TIMER,
				     (char *)timer_str)) {
		if (compat_strtouint((char *)timer_str,
				     ADF_CFG_BASE_DEC,
				     &timer_val))
			timer_val = ADF_CFG_HB_DEFAULT_VALUE;
	}

	if (timer_val < ADF_MIN_HB_TIMER_MS) {
		device_printf(GET_DEV(accel_dev),
			      "%s value cannot be lesser than %u\n",
			      ADF_HEARTBEAT_TIMER,
			      ADF_MIN_HB_TIMER_MS);
		return EINVAL;
	}

	/* Convert msec to clocks */
	clk_per_sec = clk_per_sec / 1000;
	*value = timer_val * clk_per_sec;

	return 0;
}

int
adf_get_heartbeat_status(struct adf_accel_dev *accel_dev)
{
	struct icp_qat_fw_init_admin_hb_cnt *live_s, *last_s, *curr_s;
	struct adf_hw_device_data *hw_device = accel_dev->hw_device;
	const size_t max_aes = hw_device->get_num_aes(hw_device);
	const size_t hb_ctrs = hw_device->heartbeat_ctr_num;
	const size_t stats_size =
	    max_aes * hb_ctrs * sizeof(struct icp_qat_fw_init_admin_hb_cnt);
	int ret = 0;
	size_t ae, thr;
	u16 *count_s;
	unsigned long ae_mask = 0;

	/*
	 * Memory layout of Heartbeat
	 *
	 * +----------------+----------------+---------+
	 * |   Live value   |   Last value   |  Count  |
	 * +----------------+----------------+---------+
	 * \_______________/\_______________/\________/
	 *         ^                ^            ^
	 *         |                |            |
	 *         |                |            max_aes * hb_ctrs *
	 *         |                |            sizeof(u16)
	 *         |                |
	 *         |                max_aes * hb_ctrs *
	 *         |                sizeof(icp_qat_fw_init_admin_hb_cnt)
	 *         |
	 *         max_aes * hb_ctrs *
	 *         sizeof(icp_qat_fw_init_admin_hb_cnt)
	 */
	live_s = (struct icp_qat_fw_init_admin_hb_cnt *)
		     accel_dev->admin->virt_hb_addr;
	last_s = live_s + (max_aes * hb_ctrs);
	count_s = (u16 *)(last_s + (max_aes * hb_ctrs));

	curr_s = malloc(stats_size, M_QAT, M_WAITOK | M_ZERO);

	memcpy(curr_s, live_s, stats_size);
	ae_mask = hw_device->ae_mask;

	for_each_set_bit(ae, &ae_mask, max_aes)
	{
		struct icp_qat_fw_init_admin_hb_cnt *curr =
		    curr_s + ae * hb_ctrs;
		struct icp_qat_fw_init_admin_hb_cnt *prev =
		    last_s + ae * hb_ctrs;
		u16 *count = count_s + ae * hb_ctrs;

		for (thr = 0; thr < hb_ctrs; ++thr) {
			u16 req = curr[thr].req_heartbeat_cnt;
			u16 resp = curr[thr].resp_heartbeat_cnt;
			u16 last = prev[thr].resp_heartbeat_cnt;

			if ((thr == ADF_AE_ADMIN_THREAD || req != resp) &&
			    resp == last) {
				u16 retry = ++count[thr];

				if (retry >= ADF_CFG_HB_COUNT_THRESHOLD)
					ret = EIO;
			} else {
				count[thr] = 0;
			}
		}
	}

	/* Copy current stats for the next iteration */
	memcpy(last_s, curr_s, stats_size);
	free(curr_s, M_QAT);

	return ret;
}

int
adf_heartbeat_status(struct adf_accel_dev *accel_dev,
		     enum adf_device_heartbeat_status *hb_status)
{
	/* Heartbeat is not implemented in VFs at the moment so they do not
	 * set get_heartbeat_status. Also, in case the device is not up,
	 * unsupported should be returned */
	if (!accel_dev || !accel_dev->hw_device ||
	    !accel_dev->hw_device->get_heartbeat_status ||
	    !accel_dev->heartbeat) {
		*hb_status = DEV_HB_UNSUPPORTED;
		return 0;
	}

	if (!adf_dev_started(accel_dev) ||
	    test_bit(ADF_STATUS_RESTARTING, &accel_dev->status)) {
		*hb_status = DEV_HB_UNRESPONSIVE;
		accel_dev->heartbeat->last_hb_status = DEV_HB_UNRESPONSIVE;
		return 0;
	}

	if (adf_check_hb_poll_freq(accel_dev) == EINVAL) {
		*hb_status = accel_dev->heartbeat->last_hb_status;
		return 0;
	}

	accel_dev->heartbeat->hb_sent_counter++;
	if (unlikely(accel_dev->hw_device->get_heartbeat_status(accel_dev))) {
		device_printf(GET_DEV(accel_dev),
			      "ERROR: QAT is not responding.\n");
		*hb_status = DEV_HB_UNRESPONSIVE;
		accel_dev->heartbeat->last_hb_status = DEV_HB_UNRESPONSIVE;
		accel_dev->heartbeat->hb_failed_counter++;
		return adf_notify_fatal_error(accel_dev);
	}

	*hb_status = DEV_HB_ALIVE;
	accel_dev->heartbeat->last_hb_status = DEV_HB_ALIVE;

	return 0;
}
