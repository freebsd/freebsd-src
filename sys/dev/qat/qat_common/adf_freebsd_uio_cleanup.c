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
#include <sys/param.h>
#include <sys/lock.h>
#include <sys/rwlock.h>
#include <sys/sglist.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <vm/vm.h>
#include <vm/pmap.h>
#include <vm/vm_kern.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_page.h>
#include <vm/vm_pager.h>
#include <vm/vm_param.h>

#define TX_RINGS_DISABLE 0
#define TX_RINGS_ENABLE 1
#define PKE_REQ_SIZE 64
#define BASE_ADDR_SHIFT 6
#define PKE_RX_RING_0 0
#define PKE_RX_RING_1 1

#define ADF_RING_EMPTY_RETRY_DELAY 2
#define ADF_RING_EMPTY_MAX_RETRY 15

struct bundle_orphan_ring {
	unsigned long tx_mask;
	unsigned long rx_mask;
	unsigned long asym_mask;
	int bank;
	struct resource *csr_base;
	struct adf_uio_control_bundle *bundle;
};

/*
 *    if orphan->tx_mask does not match with orphan->rx_mask
 */
static void
check_orphan_ring(struct adf_accel_dev *accel_dev,
		  struct bundle_orphan_ring *orphan,
		  struct adf_hw_device_data *hw_data)
{
	struct adf_hw_csr_ops *csr_ops = GET_CSR_OPS(accel_dev);
	int i;
	int tx_rx_gap = hw_data->tx_rx_gap;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;
	struct resource *csr_base = orphan->csr_base;
	int bank = orphan->bank;

	for (i = 0; i < num_rings_per_bank; i++) {
		if (test_bit(i, &orphan->tx_mask)) {
			int rx_ring = i + tx_rx_gap;

			if (!test_bit(rx_ring, &orphan->rx_mask)) {
				__clear_bit(i, &orphan->tx_mask);

				/* clean up this tx ring  */
				csr_ops->write_csr_ring_config(csr_base,
							       bank,
							       i,
							       0);
				csr_ops->write_csr_ring_base(csr_base,
							     bank,
							     i,
							     0);
			}

		} else if (test_bit(i, &orphan->rx_mask)) {
			int tx_ring = i - tx_rx_gap;

			if (!test_bit(tx_ring, &orphan->tx_mask)) {
				__clear_bit(i, &orphan->rx_mask);

				/* clean up this rx ring */
				csr_ops->write_csr_ring_config(csr_base,
							       bank,
							       i,
							       0);
				csr_ops->write_csr_ring_base(csr_base,
							     bank,
							     i,
							     0);
			}
		}
	}
}

static int
get_orphan_bundle(int bank,
		  struct adf_uio_control_accel *accel,
		  struct bundle_orphan_ring **orphan_bundle_out)
{
	int i;
	int ret = 0;
	struct resource *csr_base;
	unsigned long tx_mask;
	unsigned long asym_mask;
	struct adf_accel_dev *accel_dev = accel->accel_dev;
	struct adf_hw_csr_ops *csr_ops = GET_CSR_OPS(accel_dev);
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;
	struct bundle_orphan_ring *orphan_bundle = NULL;
	uint64_t base;
	struct list_head *entry;
	struct adf_uio_instance_rings *instance_rings;
	struct adf_uio_control_bundle *bundle;
	u16 ring_mask = 0;

	orphan_bundle =
	    malloc(sizeof(*orphan_bundle), M_QAT, M_WAITOK | M_ZERO);
	if (!orphan_bundle)
		return ENOMEM;

	csr_base = accel->bar->virt_addr;
	orphan_bundle->csr_base = csr_base;
	orphan_bundle->bank = bank;

	orphan_bundle->tx_mask = 0;
	orphan_bundle->rx_mask = 0;
	tx_mask = accel_dev->hw_device->tx_rings_mask;
	asym_mask = accel_dev->hw_device->asym_rings_mask;

	/* Get ring mask for this process. */
	bundle = &accel->bundle[bank];
	orphan_bundle->bundle = bundle;
	mutex_lock(&bundle->list_lock);
	list_for_each(entry, &bundle->list)
	{
		instance_rings =
		    list_entry(entry, struct adf_uio_instance_rings, list);
		if (instance_rings->user_pid == curproc->p_pid) {
			ring_mask = instance_rings->ring_mask;
			break;
		}
	}
	mutex_unlock(&bundle->list_lock);

	for (i = 0; i < num_rings_per_bank; i++) {
		base = csr_ops->read_csr_ring_base(csr_base, bank, i);

		if (!base)
			continue;
		if (!(ring_mask & 1 << i))
			continue; /* Not reserved for this process. */

		if (test_bit(i, &tx_mask))
			__set_bit(i, &orphan_bundle->tx_mask);
		else
			__set_bit(i, &orphan_bundle->rx_mask);

		if (test_bit(i, &asym_mask))
			__set_bit(i, &orphan_bundle->asym_mask);
	}

	if (orphan_bundle->tx_mask || orphan_bundle->rx_mask)
		check_orphan_ring(accel_dev, orphan_bundle, hw_data);

	*orphan_bundle_out = orphan_bundle;
	return ret;
}

static void
put_orphan_bundle(struct bundle_orphan_ring *bundle)
{
	if (!bundle)
		return;

	free(bundle, M_QAT);
}

/* cleanup all ring  */
static void
cleanup_all_ring(struct adf_uio_control_accel *accel,
		 struct bundle_orphan_ring *orphan)
{
	int i;
	struct resource *csr_base = orphan->csr_base;
	unsigned long mask = orphan->rx_mask | orphan->tx_mask;
	struct adf_accel_dev *accel_dev = accel->accel_dev;
	struct adf_hw_csr_ops *csr_ops = GET_CSR_OPS(accel_dev);
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;
	int bank = orphan->bank;

	mutex_lock(&orphan->bundle->lock);
	orphan->bundle->rings_enabled &= ~mask;
	adf_update_uio_ring_arb(orphan->bundle);
	mutex_unlock(&orphan->bundle->lock);

	for (i = 0; i < num_rings_per_bank; i++) {
		if (!test_bit(i, &mask))
			continue;

		csr_ops->write_csr_ring_config(csr_base, bank, i, 0);
		csr_ops->write_csr_ring_base(csr_base, bank, i, 0);
	}
}

/*
 * Return true, if number of messages in tx ring is equal to number
 * of messages in corresponding rx ring, else false.
 */
static bool
is_all_resp_recvd(struct adf_hw_csr_ops *csr_ops,
		  struct bundle_orphan_ring *bundle,
		  const u8 num_rings_per_bank)
{
	u32 rx_tail = 0, tx_head = 0, rx_ring_msg_offset = 0,
	    tx_ring_msg_offset = 0, tx_rx_offset = num_rings_per_bank / 2,
	    idx = 0, retry = 0, delay = ADF_RING_EMPTY_RETRY_DELAY;

	do {
		for_each_set_bit(idx, &bundle->tx_mask, tx_rx_offset)
		{
			rx_tail =
			    csr_ops->read_csr_ring_tail(bundle->csr_base,
							0,
							(idx + tx_rx_offset));
			tx_head = csr_ops->read_csr_ring_head(bundle->csr_base,
							      0,
							      idx);

			/*
			 * Normalize messages in tx rings to match rx ring
			 * message size, i.e., size of response message(32).
			 * Asym messages are 64 bytes each, so right shift
			 * by 1 to normalize to 32. Sym and compression
			 * messages are 128 bytes each, so right shift by 2
			 * to normalize to 32.
			 */
			if (bundle->asym_mask & (1 << idx))
				tx_ring_msg_offset = (tx_head >> 1);
			else
				tx_ring_msg_offset = (tx_head >> 2);

			rx_ring_msg_offset = rx_tail;

			if (tx_ring_msg_offset != rx_ring_msg_offset)
				break;
		}
		if (idx == tx_rx_offset)
			/* All Tx and Rx ring message counts match */
			return true;

		DELAY(delay);
		delay *= 2;
	} while (++retry < ADF_RING_EMPTY_MAX_RETRY);

	return false;
}

static int
bundle_need_cleanup(int bank, struct adf_uio_control_accel *accel)
{
	struct resource *csr_base = accel->bar->virt_addr;
	struct adf_accel_dev *accel_dev = accel->accel_dev;
	struct adf_hw_csr_ops *csr_ops = GET_CSR_OPS(accel_dev);
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;
	int i;

	if (!csr_base)
		return 0;

	for (i = 0; i < num_rings_per_bank; i++) {
		if (csr_ops->read_csr_ring_base(csr_base, bank, i))
			return 1;
	}

	return 0;
}

static void
cleanup_orphan_ring(struct bundle_orphan_ring *orphan,
		    struct adf_uio_control_accel *accel)
{
	struct adf_accel_dev *accel_dev = accel->accel_dev;
	struct adf_hw_csr_ops *csr_ops = GET_CSR_OPS(accel_dev);
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 number_rings_per_bank = hw_data->num_rings_per_bank;

	/* disable the interrupt */
	csr_ops->write_csr_int_col_en(orphan->csr_base, orphan->bank, 0);

	/*
	 * wait firmware finish the in-process ring
	 * 1. disable all tx rings
	 * 2. check if all responses are received
	 * 3. reset all rings
	 */
	adf_disable_ring_arb(accel_dev, orphan->csr_base, 0, orphan->tx_mask);

	if (!is_all_resp_recvd(csr_ops, orphan, number_rings_per_bank)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to clean up orphan rings");
		return;
	}

	/*
	 * When the execution reaches here, it is assumed that
	 * there is no inflight request in the rings and that
	 * there is no in-process ring.
	 */

	cleanup_all_ring(accel, orphan);
}

void
adf_uio_do_cleanup_orphan(int bank, struct adf_uio_control_accel *accel)
{
	int ret, pid_found;
	struct adf_uio_instance_rings *instance_rings, *tmp;
	struct adf_uio_control_bundle *bundle;
	/* orphan is local pointer allocated and deallocated in this function */
	struct bundle_orphan_ring *orphan = NULL;
	struct adf_accel_dev *accel_dev = accel->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;

	if (!bundle_need_cleanup(bank, accel))
		goto release;

	ret = get_orphan_bundle(bank, accel, &orphan);
	if (ret != 0)
		return;

	/*
	 * If driver supports ring pair reset, no matter process
	 * exits normally or abnormally, just do ring pair reset.
	 * ring pair reset will reset all ring pair registers to
	 * default value. Driver only needs to reset ring mask
	 */
	if (hw_data->ring_pair_reset) {
		hw_data->ring_pair_reset(
		    accel_dev, orphan->bundle->hardware_bundle_number);
		mutex_lock(&orphan->bundle->lock);
		/*
		 * If processes exit normally, rx_mask, tx_mask
		 * and rings_enabled are all 0, below expression
		 * have no impact on rings_enabled.
		 * If processes exit abnormally, rings_enabled
		 * will be set as 0 by below expression.
		 */
		orphan->bundle->rings_enabled &=
		    ~(orphan->rx_mask | orphan->tx_mask);
		mutex_unlock(&orphan->bundle->lock);
		goto out;
	}

	if (!orphan->tx_mask && !orphan->rx_mask)
		goto out;

	device_printf(GET_DEV(accel_dev),
		      "Process %d %s exit with orphan rings %lx:%lx\n",
		      curproc->p_pid,
		      curproc->p_comm,
		      orphan->tx_mask,
		      orphan->rx_mask);

	if (!test_bit(ADF_STATUS_RESTARTING, &accel_dev->status)) {
		cleanup_orphan_ring(orphan, accel);
	}
out:
	put_orphan_bundle(orphan);

release:

	bundle = &accel->bundle[bank];
	/*
	 * If the user process died without releasing the rings
	 * then force a release here.
	 */
	mutex_lock(&bundle->list_lock);
	pid_found = 0;
	list_for_each_entry_safe(instance_rings, tmp, &bundle->list, list)
	{
		if (instance_rings->user_pid == curproc->p_pid) {
			pid_found = 1;
			break;
		}
	}
	mutex_unlock(&bundle->list_lock);

	if (pid_found) {
		mutex_lock(&bundle->lock);
		bundle->rings_used &= ~instance_rings->ring_mask;
		mutex_unlock(&bundle->lock);
	}
}
