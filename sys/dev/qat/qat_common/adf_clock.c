/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "adf_accel_devices.h"
#include "adf_common_drv.h"

#include <linux/delay.h>

#define MEASURE_CLOCK_RETRIES 10
#define MEASURE_CLOCK_DELTA_THRESHOLD 100
#define MEASURE_CLOCK_DELAY 10000
#define ME_CLK_DIVIDER 16

#define CLK_DBGFS_FILE "frequency"
#define HB_SYSCTL_ERR(RC)                                                      \
	do {                                                                   \
		if (!RC) {                                                     \
			device_printf(GET_DEV(accel_dev),                      \
				      "Memory allocation failed in \
				adf_heartbeat_dbg_add\n");                     \
			return ENOMEM;                                         \
		}                                                              \
	} while (0)

int
adf_clock_debugfs_add(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	struct sysctl_ctx_list *qat_sysctl_ctx;
	struct sysctl_oid *qat_sysctl_tree;
	struct sysctl_oid *rc = 0;

	qat_sysctl_ctx =
	    device_get_sysctl_ctx(accel_dev->accel_pci_dev.pci_dev);
	qat_sysctl_tree =
	    device_get_sysctl_tree(accel_dev->accel_pci_dev.pci_dev);

	rc = SYSCTL_ADD_UINT(qat_sysctl_ctx,
			     SYSCTL_CHILDREN(qat_sysctl_tree),
			     OID_AUTO,
			     CLK_DBGFS_FILE,
			     CTLFLAG_RD,
			     &hw_data->clock_frequency,
			     0,
			     "clock frequency");
	HB_SYSCTL_ERR(rc);
	return 0;
}

/**
 * adf_dev_measure_clock() -- Measure the CPM clock frequency
 * @accel_dev: Pointer to acceleration device.
 * @frequency: Pointer to returned frequency in Hz.
 *
 * Return: 0 on success, error code otherwise.
 */
static int
measure_clock(struct adf_accel_dev *accel_dev, u32 *frequency)
{
	struct timespec ts1;
	struct timespec ts2;
	struct timespec ts3;
	struct timespec ts4;
	struct timespec delta;
	u64 delta_us = 0;
	u64 timestamp1 = 0;
	u64 timestamp2 = 0;
	u64 temp = 0;
	int tries = 0;

	if (!accel_dev || !frequency)
		return EIO;
	do {
		nanotime(&ts1);
		if (adf_get_fw_timestamp(accel_dev, &timestamp1)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to get fw timestamp\n");
			return EIO;
		}
		nanotime(&ts2);

		delta = timespec_sub(ts2, ts1);
		temp = delta.tv_nsec;
		do_div(temp, NSEC_PER_USEC);

		delta_us = delta.tv_sec * USEC_PER_SEC + temp;
	} while (delta_us > MEASURE_CLOCK_DELTA_THRESHOLD &&
		 ++tries < MEASURE_CLOCK_RETRIES);

	if (tries >= MEASURE_CLOCK_RETRIES) {
		device_printf(GET_DEV(accel_dev),
			      "Excessive clock measure delay\n");
		return EIO;
	}

	usleep_range(MEASURE_CLOCK_DELAY, MEASURE_CLOCK_DELAY * 2);
	tries = 0;
	do {
		nanotime(&ts3);
		if (adf_get_fw_timestamp(accel_dev, &timestamp2)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to get fw timestamp\n");
			return EIO;
		}
		nanotime(&ts4);

		delta = timespec_sub(ts4, ts3);
		temp = delta.tv_nsec;
		do_div(temp, NSEC_PER_USEC);

		delta_us = delta.tv_sec * USEC_PER_SEC + temp;
	} while (delta_us > MEASURE_CLOCK_DELTA_THRESHOLD &&
		 ++tries < MEASURE_CLOCK_RETRIES);

	if (tries >= MEASURE_CLOCK_RETRIES) {
		device_printf(GET_DEV(accel_dev),
			      "Excessive clock measure delay\n");
		return EIO;
	}

	delta = timespec_sub(ts3, ts1);
	temp =
	    delta.tv_sec * NSEC_PER_SEC + delta.tv_nsec + (NSEC_PER_USEC / 2);
	do_div(temp, NSEC_PER_USEC);
	delta_us = temp;
	/* Don't pretend that this gives better than 100KHz resolution */
	temp = (timestamp2 - timestamp1) * ME_CLK_DIVIDER * 10 + (delta_us / 2);
	do_div(temp, delta_us);
	*frequency = temp * 100000;

	return 0;
}

/**
 * adf_dev_measure_clock() -- Measure the CPM clock frequency
 * @accel_dev: Pointer to acceleration device.
 * @frequency: Pointer to returned frequency in Hz.
 * @min: Minimum expected frequency
 * @max: Maximum expected frequency
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_dev_measure_clock(struct adf_accel_dev *accel_dev,
		      u32 *frequency,
		      u32 min,
		      u32 max)
{
	int ret;
	u32 freq;

	ret = measure_clock(accel_dev, &freq);
	if (ret)
		return ret;

	if (freq < min) {
		device_printf(GET_DEV(accel_dev),
			      "Slow clock %d MHz measured, assuming %d\n",
			      freq,
			      min);
		freq = min;
	} else if (freq > max) {
		device_printf(GET_DEV(accel_dev),
			      "Fast clock %d MHz measured, assuming %d\n",
			      freq,
			      max);
		freq = max;
	}
	*frequency = freq;
	return 0;
}

static inline u64
timespec_to_ms(const struct timespec *ts)
{
	return (uint64_t)(ts->tv_sec * (1000)) + (ts->tv_nsec / NSEC_PER_MSEC);
}

u64
adf_clock_get_current_time(void)
{
	struct timespec ts;

	getnanotime(&ts);
	return timespec_to_ms(&ts);
}
