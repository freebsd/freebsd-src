/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#include "qat_freebsd.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"
#include "adf_accel_devices.h"
#include "icp_qat_uclo.h"
#include "icp_qat_fw.h"
#include "icp_qat_fw_init_admin.h"
#include "adf_cfg_strings.h"
#include "adf_uio_control.h"
#include "adf_uio_cleanup.h"
#include "adf_uio.h"
#include "adf_transport_access_macros.h"
#include "adf_transport_internal.h"
#include <sys/bus.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/module.h>
#include <sys/sx.h>
#include <sys/malloc.h>
#include <machine/atomic.h>
#include <dev/pci/pcivar.h>
#include <sys/conf.h>
#include <sys/systm.h>
#include <sys/queue.h>
#include <sys/proc.h>
#include <sys/types.h>
#include <sys/priv.h>
#include <linux/list.h>
#include "adf_accel_devices.h"
#include "adf_common_drv.h"
#include "adf_cfg.h"
#include "adf_cfg_common.h"
#include "adf_cfg_user.h"
#include "adf_heartbeat.h"
#include "adf_cfg_device.h"

#define DEVICE_NAME "qat_adf_ctl"

static struct sx adf_ctl_lock;

static d_ioctl_t adf_ctl_ioctl;

void *misc_counter;

static struct cdevsw adf_ctl_cdevsw = {
	.d_version = D_VERSION,
	.d_ioctl = adf_ctl_ioctl,
	.d_name = DEVICE_NAME,
};

static struct cdev *adf_ctl_dev;

static void adf_chr_drv_destroy(void)
{
	destroy_dev(adf_ctl_dev);
}

struct adf_user_addr_info {
	struct list_head list;
	void *user_addr;
};

static int adf_chr_drv_create(void)
{
	adf_ctl_dev = make_dev(&adf_ctl_cdevsw, 0, UID_ROOT, GID_WHEEL, 0600,
			       DEVICE_NAME);

	if (!adf_ctl_dev) {
		printf("QAT: failed to create device\n");
		goto err_cdev_del;
	}
	return 0;
err_cdev_del:
	return EFAULT;
}

static int adf_ctl_alloc_resources(struct adf_user_cfg_ctl_data **ctl_data,
				   caddr_t arg)
{
	*ctl_data = (struct adf_user_cfg_ctl_data *)arg;
	return 0;
}

static int adf_copy_keyval_to_user(struct adf_accel_dev *accel_dev,
				   struct adf_user_cfg_ctl_data *ctl_data)
{
	struct adf_user_cfg_key_val key_val;
	struct adf_user_cfg_section section;
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES] = {0};
	char *user_ptr;

	if (copyin(ctl_data->config_section, &section,
		   sizeof(struct adf_user_cfg_section))) {
		device_printf(GET_DEV(accel_dev),
			      "failed to copy section info\n");
		return EFAULT;
	}

	if (copyin(section.params, &key_val,
		   sizeof(struct adf_user_cfg_key_val))) {
		device_printf(GET_DEV(accel_dev), "failed to copy key val\n");
		return EFAULT;
	}

	user_ptr = ((char *)section.params) + ADF_CFG_MAX_KEY_LEN_IN_BYTES;

	if (adf_cfg_get_param_value(
		accel_dev, section.name, key_val.key, val)) {
		return EFAULT;
	}

	if (copyout(val, user_ptr,
		    ADF_CFG_MAX_VAL_LEN_IN_BYTES)) {
		device_printf(GET_DEV(accel_dev),
			      "failed to copy keyvalue to user!\n");
		return EFAULT;
	}

	return 0;
}

static int adf_ctl_ioctl_get_num_devices(unsigned int cmd,
					 caddr_t arg)
{
	adf_devmgr_get_num_dev((uint32_t *)arg);

	return 0;
}

static int adf_ctl_ioctl_get_status(unsigned int cmd,
				    caddr_t arg)
{
	struct adf_hw_device_data *hw_data;
	struct adf_dev_status_info *dev_info;
	struct adf_accel_dev *accel_dev;

	dev_info = (struct adf_dev_status_info *)arg;

	accel_dev = adf_devmgr_get_dev_by_id(dev_info->accel_id);
	if (!accel_dev)
		return ENODEV;

	hw_data = accel_dev->hw_device;
	dev_info->state = adf_dev_started(accel_dev) ? DEV_UP : DEV_DOWN;
	dev_info->num_ae = hw_data->get_num_aes(hw_data);
	dev_info->num_accel = hw_data->get_num_accels(hw_data);
	dev_info->num_logical_accel = hw_data->num_logical_accel;
	dev_info->banks_per_accel = hw_data->num_banks
	/ hw_data->num_logical_accel;
	strlcpy(dev_info->name, hw_data->dev_class->name,
		sizeof(dev_info->name));
	dev_info->instance_id = hw_data->instance_id;
	dev_info->type = hw_data->dev_class->type;
	dev_info->bus = pci_get_bus(accel_to_pci_dev(accel_dev));
	dev_info->dev = pci_get_slot(accel_to_pci_dev(accel_dev));
	dev_info->fun = pci_get_function(accel_to_pci_dev(accel_dev));
	dev_info->domain = pci_get_domain(accel_to_pci_dev(accel_dev));

	dev_info->pci_device_id = pci_get_device(accel_to_pci_dev(accel_dev));

	dev_info->node_id = accel_dev->accel_pci_dev.node;
	dev_info->sku = accel_dev->accel_pci_dev.sku;

	dev_info->device_mem_available = accel_dev->aram_info ?
		accel_dev->aram_info->inter_buff_aram_region_size : 0;

	return 0;
}

static int adf_ctl_ioctl_heartbeat(unsigned int cmd,
				   caddr_t arg)
{
	int ret = 0;
	struct adf_accel_dev *accel_dev;
	struct adf_dev_heartbeat_status_ctl *hb_status;

	hb_status = (struct adf_dev_heartbeat_status_ctl *)arg;

	accel_dev = adf_devmgr_get_dev_by_id(hb_status->device_id);
	if (!accel_dev)
		return ENODEV;

	if (adf_heartbeat_status(accel_dev, &hb_status->status)) {
		device_printf(GET_DEV(accel_dev),
			      "failed to get heartbeat status\n");
		return EAGAIN;
	}
	return ret;
}

static int adf_ctl_ioctl_dev_get_value(unsigned int cmd,
				       caddr_t arg)
{
	int ret = 0;
	struct adf_user_cfg_ctl_data *ctl_data;
	struct adf_accel_dev *accel_dev;

	ret = adf_ctl_alloc_resources(&ctl_data, arg);
	if (ret)
		return ret;

	accel_dev = adf_devmgr_get_dev_by_id(ctl_data->device_id);
	if (!accel_dev) {
		printf("QAT: Device %d not found\n", ctl_data->device_id);
		ret = ENODEV;
		goto out;
	}

	ret = adf_copy_keyval_to_user(accel_dev, ctl_data);
	if (ret) {
		ret = ENODEV;
		goto out;
	}
out:
	return ret;
}

static struct adf_uio_control_bundle
	*adf_ctl_ioctl_bundle(struct adf_user_reserve_ring reserve)
{
	struct adf_accel_dev *accel_dev;
	struct adf_uio_control_accel *accel;
	struct adf_uio_control_bundle *bundle = NULL;
	u8 num_rings_per_bank = 0;

	accel_dev = adf_devmgr_get_dev_by_id(reserve.accel_id);
	if (!accel_dev) {
		pr_err("QAT: Failed to get accel_dev\n");
		return NULL;
	}
	num_rings_per_bank = accel_dev->hw_device->num_rings_per_bank;

	accel = accel_dev->accel;
	if (!accel) {
		pr_err("QAT: Failed to get accel\n");
		return NULL;
	}

	if (reserve.bank_nr >= GET_MAX_BANKS(accel_dev)) {
		pr_err("QAT: Invalid bank number %d\n", reserve.bank_nr);
		return NULL;
	}
	if (reserve.ring_mask & ~((1 << num_rings_per_bank) - 1)) {
		pr_err("QAT: Invalid ring mask %0X\n", reserve.ring_mask);
		return NULL;
	}
	if (accel->num_ker_bundles > reserve.bank_nr) {
		pr_err("QAT: Invalid user reserved bank\n");
		return NULL;
	}
	bundle = &accel->bundle[reserve.bank_nr];

	return bundle;
}

static int adf_ctl_ioctl_reserve_ring(caddr_t arg)
{
	struct adf_user_reserve_ring reserve = {0};
	struct adf_uio_control_bundle *bundle;
	struct adf_uio_instance_rings *instance_rings;
	int pid_entry_found = 0;

	reserve = *((struct adf_user_reserve_ring *)arg);

	bundle = adf_ctl_ioctl_bundle(reserve);
	if (!bundle) {
		pr_err("QAT: Failed to get bundle\n");
		return -EINVAL;
	}

	mutex_lock(&bundle->lock);
	if (bundle->rings_used & reserve.ring_mask) {
		pr_err("QAT: Bundle %d, rings 0x%04X already reserved\n",
		       reserve.bank_nr,
		       reserve.ring_mask);
		mutex_unlock(&bundle->lock);
		return -EINVAL;
	}
	mutex_unlock(&bundle->lock);

	/* Find the list entry for this process */
	mutex_lock(&bundle->list_lock);
	list_for_each_entry(instance_rings, &bundle->list, list) {
		if (instance_rings->user_pid == curproc->p_pid) {
			pid_entry_found = 1;
			break;
		}
	}
	mutex_unlock(&bundle->list_lock);

	if (!pid_entry_found) {
		pr_err("QAT: process %d not found\n", curproc->p_pid);
		return -EINVAL;
	}

	instance_rings->ring_mask |= reserve.ring_mask;
	mutex_lock(&bundle->lock);
	bundle->rings_used |= reserve.ring_mask;
	mutex_unlock(&bundle->lock);

	return 0;
}

static int adf_ctl_ioctl_release_ring(caddr_t arg)
{
	struct adf_user_reserve_ring reserve;
	struct adf_uio_control_bundle *bundle;
	struct adf_uio_instance_rings *instance_rings;
	int pid_entry_found;

	reserve = *((struct adf_user_reserve_ring *)arg);

	bundle = adf_ctl_ioctl_bundle(reserve);
	if (!bundle) {
		pr_err("QAT: Failed to get bundle\n");
		return -EINVAL;
	}

	/* Find the list entry for this process */
	pid_entry_found = 0;
	mutex_lock(&bundle->list_lock);
	list_for_each_entry(instance_rings, &bundle->list, list) {
		if (instance_rings->user_pid == curproc->p_pid) {
			pid_entry_found = 1;
			break;
		}
	}
	mutex_unlock(&bundle->list_lock);

	if (!pid_entry_found) {
		pr_err("QAT: No ring reservation found for PID %d\n",
		       curproc->p_pid);
		return -EINVAL;
	}

	if ((instance_rings->ring_mask & reserve.ring_mask) !=
	    reserve.ring_mask) {
		pr_err("QAT: Attempt to release rings not reserved by this process\n");
		return -EINVAL;
	}

	instance_rings->ring_mask &= ~reserve.ring_mask;
	mutex_lock(&bundle->lock);
	bundle->rings_used &= ~reserve.ring_mask;
	mutex_unlock(&bundle->lock);

	return 0;
}

static int adf_ctl_ioctl_enable_ring(caddr_t arg)
{
	struct adf_user_reserve_ring reserve;
	struct adf_uio_control_bundle *bundle;

	reserve = *((struct adf_user_reserve_ring *)arg);

	bundle = adf_ctl_ioctl_bundle(reserve);
	if (!bundle) {
		pr_err("QAT: Failed to get bundle\n");
		return -EINVAL;
	}

	mutex_lock(&bundle->lock);
	bundle->rings_enabled |= reserve.ring_mask;
	adf_update_uio_ring_arb(bundle);
	mutex_unlock(&bundle->lock);

	return 0;
}

static int adf_ctl_ioctl_disable_ring(caddr_t arg)
{
	struct adf_user_reserve_ring reserve;
	struct adf_uio_control_bundle *bundle;

	reserve = *((struct adf_user_reserve_ring *)arg);

	bundle = adf_ctl_ioctl_bundle(reserve);
	if (!bundle) {
		pr_err("QAT: Failed to get bundle\n");
		return -EINVAL;
	}

	mutex_lock(&bundle->lock);
	bundle->rings_enabled &= ~reserve.ring_mask;
	adf_update_uio_ring_arb(bundle);
	mutex_unlock(&bundle->lock);

	return 0;
}

static int adf_ctl_ioctl(struct cdev *dev,
			 u_long cmd,
			 caddr_t arg,
			 int fflag,
			 struct thread *td)
{
	int ret = 0;
	bool allowed = false;
	int i;
	static const unsigned int unrestricted_cmds[] = {
		IOCTL_GET_NUM_DEVICES,     IOCTL_STATUS_ACCEL_DEV,
		IOCTL_HEARTBEAT_ACCEL_DEV, IOCTL_GET_CFG_VAL,
		IOCTL_RESERVE_RING,	IOCTL_RELEASE_RING,
		IOCTL_ENABLE_RING,	 IOCTL_DISABLE_RING,
	};

	if (priv_check(curthread, PRIV_DRIVER)) {
		for (i = 0; i < ARRAY_SIZE(unrestricted_cmds); i++) {
			if (cmd == unrestricted_cmds[i]) {
				allowed = true;
				break;
			}
		}
		if (!allowed)
			return EACCES;
	}

	/* All commands have an argument */
	if (!arg)
		return EFAULT;

	if (sx_xlock_sig(&adf_ctl_lock))
		return EINTR;

	switch (cmd) {
	case IOCTL_GET_NUM_DEVICES:
		ret = adf_ctl_ioctl_get_num_devices(cmd, arg);
		break;
	case IOCTL_STATUS_ACCEL_DEV:
		ret = adf_ctl_ioctl_get_status(cmd, arg);
		break;
	case IOCTL_GET_CFG_VAL:
		ret = adf_ctl_ioctl_dev_get_value(cmd, arg);
		break;
	case IOCTL_RESERVE_RING:
		ret = adf_ctl_ioctl_reserve_ring(arg);
		break;
	case IOCTL_RELEASE_RING:
		ret = adf_ctl_ioctl_release_ring(arg);
		break;
	case IOCTL_ENABLE_RING:
		ret = adf_ctl_ioctl_enable_ring(arg);
		break;
	case IOCTL_DISABLE_RING:
		ret = adf_ctl_ioctl_disable_ring(arg);
		break;
	case IOCTL_HEARTBEAT_ACCEL_DEV:
		ret = adf_ctl_ioctl_heartbeat(cmd, arg);
		break;
	default:
		printf("QAT: Invalid ioctl\n");
		ret = ENOTTY;
		break;
	}
	sx_xunlock(&adf_ctl_lock);
	return ret;
}

int
adf_register_ctl_device_driver(void)
{
	sx_init(&adf_ctl_lock, "adf ctl");

	if (adf_chr_drv_create())
		goto err_chr_dev;

	adf_state_init();
	if (adf_processes_dev_register() != 0)
		goto err_processes_dev_register;
	return 0;

err_processes_dev_register:
	adf_chr_drv_destroy();
err_chr_dev:
	sx_destroy(&adf_ctl_lock);
	return EFAULT;
}

void
adf_unregister_ctl_device_driver(void)
{
	adf_processes_dev_unregister();
	adf_state_destroy();
	adf_chr_drv_destroy();
	adf_clean_vf_map(false);
	sx_destroy(&adf_ctl_lock);
}
