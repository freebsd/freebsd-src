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
#include "adf_dev_err.h"
#include "adf_uio.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include <sys/mutex.h>
#include <linux/delay.h>
#include "adf_accel_devices.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "icp_qat_fw.h"

/* Mask used to check the CompressAndVerify capability bit */
#define DC_CNV_EXTENDED_CAPABILITY (0x01)

/* Mask used to check the CompressAndVerifyAndRecover capability bit */
#define DC_CNVNR_EXTENDED_CAPABILITY (0x100)

static LIST_HEAD(service_table);
static DEFINE_MUTEX(service_lock);

static void
adf_service_add(struct service_hndl *service)
{
	mutex_lock(&service_lock);
	list_add(&service->list, &service_table);
	mutex_unlock(&service_lock);
}

int
adf_service_register(struct service_hndl *service)
{
	memset(service->init_status, 0, sizeof(service->init_status));
	memset(service->start_status, 0, sizeof(service->start_status));
	adf_service_add(service);
	return 0;
}

static void
adf_service_remove(struct service_hndl *service)
{
	mutex_lock(&service_lock);
	list_del(&service->list);
	mutex_unlock(&service_lock);
}

int
adf_service_unregister(struct service_hndl *service)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(service->init_status); i++) {
		if (service->init_status[i] || service->start_status[i]) {
			pr_err("QAT: Could not remove active service [%d]\n",
			       i);
			return EFAULT;
		}
	}
	adf_service_remove(service);
	return 0;
}

static int
adf_cfg_add_device_params(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char hw_version[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	char mmp_version[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	struct adf_hw_device_data *hw_data = NULL;
	unsigned long val;
	if (!accel_dev)
		return -EINVAL;

	hw_data = accel_dev->hw_device;

	if (adf_cfg_section_add(accel_dev, ADF_GENERAL_SEC))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_MAX_BANKS);
	val = GET_MAX_BANKS(accel_dev);
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_CAPABILITIES_MASK);
	val = hw_data->accel_capabilities_mask;
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)val, ADF_HEX))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_PKG_ID);
	val = accel_dev->accel_id;
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_NODE_ID);
	val = dev_to_node(GET_DEV(accel_dev));
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC))
		goto err;

	snprintf(key, sizeof(key), ADF_DEV_MAX_RINGS_PER_BANK);
	val = hw_data->num_rings_per_bank;
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)&val, ADF_DEC))
		goto err;

	snprintf(key, sizeof(key), ADF_HW_REV_ID_KEY);
	snprintf(hw_version,
		 ADF_CFG_MAX_VAL_LEN_IN_BYTES,
		 "%d",
		 accel_dev->accel_pci_dev.revid);
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)hw_version, ADF_STR))
		goto err;

	snprintf(key, sizeof(key), ADF_MMP_VER_KEY);
	snprintf(mmp_version,
		 ADF_CFG_MAX_VAL_LEN_IN_BYTES,
		 "%d.%d.%d",
		 accel_dev->fw_versions.mmp_version_major,
		 accel_dev->fw_versions.mmp_version_minor,
		 accel_dev->fw_versions.mmp_version_patch);
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)mmp_version, ADF_STR))
		goto err;

	return 0;
err:
	device_printf(GET_DEV(accel_dev),
		      "Failed to add internal values to accel_dev cfg\n");
	return -EINVAL;
}

static int
adf_cfg_add_fw_version(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char fw_version[ADF_CFG_MAX_VAL_LEN_IN_BYTES];

	snprintf(key, sizeof(key), ADF_UOF_VER_KEY);
	snprintf(fw_version,
		 ADF_CFG_MAX_VAL_LEN_IN_BYTES,
		 "%d.%d.%d",
		 accel_dev->fw_versions.fw_version_major,
		 accel_dev->fw_versions.fw_version_minor,
		 accel_dev->fw_versions.fw_version_patch);
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)fw_version, ADF_STR))
		return EFAULT;

	return 0;
}

static int
adf_cfg_add_ext_params(struct adf_accel_dev *accel_dev)
{
	char key[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	unsigned long val;

	snprintf(key, sizeof(key), ADF_DC_EXTENDED_FEATURES);

	val = hw_data->extended_dc_capabilities;
	if (adf_cfg_add_key_value_param(
		accel_dev, ADF_GENERAL_SEC, key, (void *)val, ADF_HEX))
		return -EINVAL;

	return 0;
}

void
adf_error_notifier(uintptr_t arg)
{
	struct adf_accel_dev *accel_dev = (struct adf_accel_dev *)arg;
	struct service_hndl *service;
	struct list_head *list_itr;

	list_for_each(list_itr, &service_table)
	{
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_ERROR))
			device_printf(GET_DEV(accel_dev),
				      "Failed to send error event to %s.\n",
				      service->name);
	}
}

/**
 * adf_set_ssm_wdtimer() - Initialize the slice hang watchdog timer.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_set_ssm_wdtimer(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_bar *misc_bar =
	    &GET_BARS(accel_dev)[hw_data->get_misc_bar_id(hw_data)];
	struct resource *csr = misc_bar->virt_addr;
	u32 i;
	unsigned int mask;
	u32 clk_per_sec = hw_data->get_clock_speed(hw_data);
	u32 timer_val = ADF_WDT_TIMER_SYM_COMP_MS * (clk_per_sec / 1000);
	u32 timer_val_pke = ADF_GEN2_SSM_WDT_PKE_DEFAULT_VALUE;
	char timer_str[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = { 0 };

	/* Get Watch Dog Timer for CySym+Comp from the configuration */
	if (!adf_cfg_get_param_value(accel_dev,
				     ADF_GENERAL_SEC,
				     ADF_DEV_SSM_WDT_BULK,
				     (char *)timer_str)) {
		if (!compat_strtouint((char *)timer_str,
				      ADF_CFG_BASE_DEC,
				      &timer_val))
			/* Convert msec to CPP clocks */
			timer_val = timer_val * (clk_per_sec / 1000);
	}
	/* Get Watch Dog Timer for CyAsym from the configuration */
	if (!adf_cfg_get_param_value(accel_dev,
				     ADF_GENERAL_SEC,
				     ADF_DEV_SSM_WDT_PKE,
				     (char *)timer_str)) {
		if (!compat_strtouint((char *)timer_str,
				      ADF_CFG_BASE_DEC,
				      &timer_val_pke))
			/* Convert msec to CPP clocks */
			timer_val_pke = timer_val_pke * (clk_per_sec / 1000);
	}

	for (i = 0, mask = hw_data->accel_mask; mask; i++, mask >>= 1) {
		if (!(mask & 1))
			continue;
		/* Enable Watch Dog Timer for CySym + Comp */
		ADF_CSR_WR(csr, ADF_SSMWDT(i), timer_val);
		/* Enable Watch Dog Timer for CyAsym */
		ADF_CSR_WR(csr, ADF_SSMWDTPKE(i), timer_val_pke);
	}
	return 0;
}

/**
 * adf_dev_init() - Init data structures and services for the given accel device
 * @accel_dev: Pointer to acceleration device.
 *
 * Initialize the ring data structures and the admin comms and arbitration
 * services.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_dev_init(struct adf_accel_dev *accel_dev)
{
	struct service_hndl *service;
	struct list_head *list_itr;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	char value[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	int ret = 0;
	sysctl_ctx_init(&accel_dev->sysctl_ctx);
	set_bit(ADF_STATUS_SYSCTL_CTX_INITIALISED, &accel_dev->status);

	if (!hw_data) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to init device - hw_data not set\n");
		return EFAULT;
	}
	if (hw_data->reset_hw_units)
		hw_data->reset_hw_units(accel_dev);

	if (!test_bit(ADF_STATUS_CONFIGURED, &accel_dev->status) &&
	    !accel_dev->is_vf) {
		device_printf(GET_DEV(accel_dev), "Device not configured\n");
		return EFAULT;
	}

	if (adf_init_etr_data(accel_dev)) {
		device_printf(GET_DEV(accel_dev), "Failed initialize etr\n");
		return EFAULT;
	}

	if (hw_data->init_device && hw_data->init_device(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to initialize device\n");
		return EFAULT;
	}

	if (hw_data->init_accel_units && hw_data->init_accel_units(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed initialize accel_units\n");
		return EFAULT;
	}

	if (hw_data->init_admin_comms && hw_data->init_admin_comms(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed initialize admin comms\n");
		return EFAULT;
	}

	if (hw_data->init_arb && hw_data->init_arb(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed initialize hw arbiter\n");
		return EFAULT;
	}

	if (hw_data->set_asym_rings_mask)
		hw_data->set_asym_rings_mask(accel_dev);

	hw_data->enable_ints(accel_dev);

	if (adf_ae_init(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to initialise Acceleration Engine\n");
		return EFAULT;
	}

	set_bit(ADF_STATUS_AE_INITIALISED, &accel_dev->status);

	if (adf_ae_fw_load(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to load acceleration FW\n");
		return EFAULT;
	}
	set_bit(ADF_STATUS_AE_UCODE_LOADED, &accel_dev->status);

	if (hw_data->alloc_irq(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to allocate interrupts\n");
		return EFAULT;
	}
	set_bit(ADF_STATUS_IRQ_ALLOCATED, &accel_dev->status);

	if (hw_data->init_ras && hw_data->init_ras(accel_dev)) {
		device_printf(GET_DEV(accel_dev), "Failed to init RAS\n");
		return EFAULT;
	}

	hw_data->enable_ints(accel_dev);

	hw_data->enable_error_correction(accel_dev);

	ret = hw_data->csr_info.pfvf_ops.enable_comms(accel_dev);
	if (ret)
		return ret;

	if (adf_cfg_add_device_params(accel_dev))
		return EFAULT;

	if (hw_data->add_pke_stats && hw_data->add_pke_stats(accel_dev))
		return EFAULT;

	if (hw_data->add_misc_error && hw_data->add_misc_error(accel_dev))
		return EFAULT;
	/*
	 * Subservice initialisation is divided into two stages: init and start.
	 * This is to facilitate any ordering dependencies between services
	 * prior to starting any of the accelerators.
	 */
	list_for_each(list_itr, &service_table)
	{
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_INIT)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to initialise service %s\n",
				      service->name);
			return EFAULT;
		}
		set_bit(accel_dev->accel_id, service->init_status);
	}

	/* Read autoreset on error parameter */
	ret = adf_cfg_get_param_value(accel_dev,
				      ADF_GENERAL_SEC,
				      ADF_AUTO_RESET_ON_ERROR,
				      value);
	if (!ret) {
		if (compat_strtouint(value,
				     10,
				     &accel_dev->autoreset_on_error)) {
			device_printf(
			    GET_DEV(accel_dev),
			    "Failed converting %s to a decimal value\n",
			    ADF_AUTO_RESET_ON_ERROR);
			return EFAULT;
		}
	}

	return 0;
}

/**
 * adf_dev_start() - Start acceleration service for the given accel device
 * @accel_dev:    Pointer to acceleration device.
 *
 * Function notifies all the registered services that the acceleration device
 * is ready to be used.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_dev_start(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct service_hndl *service;
	struct list_head *list_itr;

	set_bit(ADF_STATUS_STARTING, &accel_dev->status);
	if (adf_devmgr_verify_id(&accel_dev->accel_id)) {
		device_printf(GET_DEV(accel_dev),
			      "QAT: Device %d not found\n",
			      accel_dev->accel_id);
		return ENODEV;
	}
	if (adf_ae_start(accel_dev)) {
		device_printf(GET_DEV(accel_dev), "AE Start Failed\n");
		return EFAULT;
	}

	set_bit(ADF_STATUS_AE_STARTED, &accel_dev->status);
	if (hw_data->send_admin_init(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to send init message\n");
		return EFAULT;
	}

	if (adf_cfg_add_fw_version(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to update configuration FW version\n");
		return EFAULT;
	}

	if (hw_data->measure_clock)
		hw_data->measure_clock(accel_dev);

	/*
	 * Set ssm watch dog timer for slice hang detection
	 * Note! Not supported on devices older than C62x
	 */
	if (hw_data->set_ssm_wdtimer && hw_data->set_ssm_wdtimer(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "QAT: Failed to set ssm watch dog timer\n");
		return EFAULT;
	}

	if (hw_data->int_timer_init && hw_data->int_timer_init(accel_dev)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to init heartbeat interrupt timer\n");
		return -EFAULT;
	}

	list_for_each(list_itr, &service_table)
	{
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_START)) {
			device_printf(GET_DEV(accel_dev),
				      "Failed to start service %s\n",
				      service->name);
			return EFAULT;
		}
		set_bit(accel_dev->accel_id, service->start_status);
	}

	if (accel_dev->is_vf || !accel_dev->u1.pf.vf_info) {
		/*Register UIO devices */
		if (adf_uio_register(accel_dev)) {
			adf_uio_remove(accel_dev);
			device_printf(GET_DEV(accel_dev),
				      "Failed to register UIO devices\n");
			set_bit(ADF_STATUS_STARTING, &accel_dev->status);
			clear_bit(ADF_STATUS_STARTED, &accel_dev->status);
			return ENODEV;
		}
	}

	if (!test_bit(ADF_STATUS_RESTARTING, &accel_dev->status) &&
	    adf_cfg_add_ext_params(accel_dev))
		return EFAULT;

	clear_bit(ADF_STATUS_STARTING, &accel_dev->status);
	set_bit(ADF_STATUS_STARTED, &accel_dev->status);

	return 0;
}

/**
 * adf_dev_stop() - Stop acceleration service for the given accel device
 * @accel_dev:    Pointer to acceleration device.
 *
 * Function notifies all the registered services that the acceleration device
 * is shuting down.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_dev_stop(struct adf_accel_dev *accel_dev)
{
	struct service_hndl *service;
	struct list_head *list_itr;

	if (adf_devmgr_verify_id(&accel_dev->accel_id)) {
		device_printf(GET_DEV(accel_dev),
			      "QAT: Device %d not found\n",
			      accel_dev->accel_id);
		return ENODEV;
	}
	if (!adf_dev_started(accel_dev) &&
	    !test_bit(ADF_STATUS_STARTING, &accel_dev->status)) {
		return 0;
	}

	if (adf_dev_stop_notify_sync(accel_dev)) {
		device_printf(
		    GET_DEV(accel_dev),
		    "Waiting for device un-busy failed. Retries limit reached\n");
		return EBUSY;
	}

	clear_bit(ADF_STATUS_STARTING, &accel_dev->status);
	clear_bit(ADF_STATUS_STARTED, &accel_dev->status);

	if (accel_dev->hw_device->int_timer_exit)
		accel_dev->hw_device->int_timer_exit(accel_dev);

	list_for_each(list_itr, &service_table)
	{
		service = list_entry(list_itr, struct service_hndl, list);
		if (!test_bit(accel_dev->accel_id, service->start_status))
			continue;
		clear_bit(accel_dev->accel_id, service->start_status);
	}

	if (accel_dev->is_vf || !accel_dev->u1.pf.vf_info) {
		/* Remove UIO Devices */
		adf_uio_remove(accel_dev);
	}

	if (test_bit(ADF_STATUS_AE_STARTED, &accel_dev->status)) {
		if (adf_ae_stop(accel_dev))
			device_printf(GET_DEV(accel_dev),
				      "failed to stop AE\n");
		else
			clear_bit(ADF_STATUS_AE_STARTED, &accel_dev->status);
	}

	return 0;
}

/**
 * adf_dev_shutdown() - shutdown acceleration services and data strucutures
 * @accel_dev: Pointer to acceleration device
 *
 * Cleanup the ring data structures and the admin comms and arbitration
 * services.
 */
void
adf_dev_shutdown(struct adf_accel_dev *accel_dev)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct service_hndl *service;
	struct list_head *list_itr;

	if (test_bit(ADF_STATUS_SYSCTL_CTX_INITIALISED, &accel_dev->status)) {
		sysctl_ctx_free(&accel_dev->sysctl_ctx);
		clear_bit(ADF_STATUS_SYSCTL_CTX_INITIALISED,
			  &accel_dev->status);
	}

	if (!hw_data) {
		device_printf(
		    GET_DEV(accel_dev),
		    "QAT: Failed to shutdown device - hw_data not set\n");
		return;
	}

	if (test_bit(ADF_STATUS_AE_UCODE_LOADED, &accel_dev->status)) {
		adf_ae_fw_release(accel_dev);
		clear_bit(ADF_STATUS_AE_UCODE_LOADED, &accel_dev->status);
	}

	if (test_bit(ADF_STATUS_AE_INITIALISED, &accel_dev->status)) {
		if (adf_ae_shutdown(accel_dev))
			device_printf(GET_DEV(accel_dev),
				      "Failed to shutdown Accel Engine\n");
		else
			clear_bit(ADF_STATUS_AE_INITIALISED,
				  &accel_dev->status);
	}

	list_for_each(list_itr, &service_table)
	{
		service = list_entry(list_itr, struct service_hndl, list);
		if (!test_bit(accel_dev->accel_id, service->init_status))
			continue;
		if (service->event_hld(accel_dev, ADF_EVENT_SHUTDOWN))
			device_printf(GET_DEV(accel_dev),
				      "Failed to shutdown service %s\n",
				      service->name);
		else
			clear_bit(accel_dev->accel_id, service->init_status);
	}

	hw_data->disable_iov(accel_dev);

	if (test_bit(ADF_STATUS_IRQ_ALLOCATED, &accel_dev->status)) {
		hw_data->free_irq(accel_dev);
		clear_bit(ADF_STATUS_IRQ_ALLOCATED, &accel_dev->status);
	}

	/* Delete configuration only if not restarting */
	if (!test_bit(ADF_STATUS_RESTARTING, &accel_dev->status))
		adf_cfg_del_all(accel_dev);

	if (hw_data->remove_pke_stats)
		hw_data->remove_pke_stats(accel_dev);

	if (hw_data->remove_misc_error)
		hw_data->remove_misc_error(accel_dev);

	if (hw_data->exit_ras)
		hw_data->exit_ras(accel_dev);

	if (hw_data->exit_arb)
		hw_data->exit_arb(accel_dev);

	if (hw_data->exit_admin_comms)
		hw_data->exit_admin_comms(accel_dev);

	if (hw_data->exit_accel_units)
		hw_data->exit_accel_units(accel_dev);

	adf_cleanup_etr_data(accel_dev);
	if (hw_data->restore_device)
		hw_data->restore_device(accel_dev);
}

/**
 * adf_dev_reset() - Reset acceleration service for the given accel device
 * @accel_dev:    Pointer to acceleration device.
 * @mode: Specifies reset mode - synchronous or asynchronous.
 * Function notifies all the registered services that the acceleration device
 * is resetting.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_dev_reset(struct adf_accel_dev *accel_dev, enum adf_dev_reset_mode mode)
{
	return adf_dev_aer_schedule_reset(accel_dev, mode);
}

int
adf_dev_restarting_notify(struct adf_accel_dev *accel_dev)
{
	struct service_hndl *service;
	struct list_head *list_itr;

	list_for_each(list_itr, &service_table)
	{
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_RESTARTING))
			device_printf(GET_DEV(accel_dev),
				      "Failed to restart service %s.\n",
				      service->name);
	}
	return 0;
}

int
adf_dev_restarting_notify_sync(struct adf_accel_dev *accel_dev)
{
	int times;

	adf_dev_restarting_notify(accel_dev);
	for (times = 0; times < ADF_STOP_RETRY; times++) {
		if (!adf_dev_in_use(accel_dev))
			break;
		dev_dbg(GET_DEV(accel_dev), "retry times=%d\n", times);
		pause_ms("adfstop", 100);
	}
	if (adf_dev_in_use(accel_dev)) {
		clear_bit(ADF_STATUS_RESTARTING, &accel_dev->status);
		device_printf(GET_DEV(accel_dev),
			      "Device still in use during reset sequence.\n");
		return EBUSY;
	}

	return 0;
}

int
adf_dev_stop_notify_sync(struct adf_accel_dev *accel_dev)
{
	int times;

	struct service_hndl *service;
	struct list_head *list_itr;

	list_for_each(list_itr, &service_table)
	{
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_STOP))
			device_printf(GET_DEV(accel_dev),
				      "Failed to restart service %s.\n",
				      service->name);
	}

	for (times = 0; times < ADF_STOP_RETRY; times++) {
		if (!adf_dev_in_use(accel_dev))
			break;
		dev_dbg(GET_DEV(accel_dev), "retry times=%d\n", times);
		pause_ms("adfstop", 100);
	}
	if (adf_dev_in_use(accel_dev)) {
		clear_bit(ADF_STATUS_RESTARTING, &accel_dev->status);
		device_printf(GET_DEV(accel_dev),
			      "Device still in use during stop sequence.\n");
		return EBUSY;
	}

	return 0;
}

int
adf_dev_restarted_notify(struct adf_accel_dev *accel_dev)
{
	struct service_hndl *service;
	struct list_head *list_itr;

	list_for_each(list_itr, &service_table)
	{
		service = list_entry(list_itr, struct service_hndl, list);
		if (service->event_hld(accel_dev, ADF_EVENT_RESTARTED))
			device_printf(GET_DEV(accel_dev),
				      "Failed to restart service %s.\n",
				      service->name);
	}
	return 0;
}
