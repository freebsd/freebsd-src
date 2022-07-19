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
#include <linux/delay.h>
#include "adf_accel_devices.h"
#include "adf_transport_internal.h"
#include "adf_transport_access_macros.h"
#include "adf_cfg.h"
#include "adf_common_drv.h"

#define QAT_RING_ALIGNMENT 64

static inline u32
adf_modulo(u32 data, u32 shift)
{
	u32 div = data >> shift;
	u32 mult = div << shift;

	return data - mult;
}

static inline int
adf_check_ring_alignment(u64 addr, u64 size)
{
	if (((size - 1) & addr) != 0)
		return EFAULT;
	return 0;
}

static int
adf_verify_ring_size(u32 msg_size, u32 msg_num)
{
	int i = ADF_MIN_RING_SIZE;

	for (; i <= ADF_MAX_RING_SIZE; i++)
		if ((msg_size * msg_num) == ADF_SIZE_TO_RING_SIZE_IN_BYTES(i))
			return i;

	return ADF_DEFAULT_RING_SIZE;
}

static int
adf_reserve_ring(struct adf_etr_bank_data *bank, u32 ring)
{
	mtx_lock(&bank->lock);
	if (bank->ring_mask & (1 << ring)) {
		mtx_unlock(&bank->lock);
		return EFAULT;
	}
	bank->ring_mask |= (1 << ring);
	mtx_unlock(&bank->lock);
	return 0;
}

static void
adf_unreserve_ring(struct adf_etr_bank_data *bank, u32 ring)
{
	mtx_lock(&bank->lock);
	bank->ring_mask &= ~(1 << ring);
	mtx_unlock(&bank->lock);
}

static void
adf_enable_ring_irq(struct adf_etr_bank_data *bank, u32 ring)
{
	mtx_lock(&bank->lock);
	bank->irq_mask |= (1 << ring);
	mtx_unlock(&bank->lock);
	WRITE_CSR_INT_COL_EN(bank->csr_addr, bank->bank_number, bank->irq_mask);
	WRITE_CSR_INT_COL_CTL(bank->csr_addr,
			      bank->bank_number,
			      bank->irq_coalesc_timer);
}

static void
adf_disable_ring_irq(struct adf_etr_bank_data *bank, u32 ring)
{
	mtx_lock(&bank->lock);
	bank->irq_mask &= ~(1 << ring);
	mtx_unlock(&bank->lock);
	WRITE_CSR_INT_COL_EN(bank->csr_addr, bank->bank_number, bank->irq_mask);
}

int
adf_send_message(struct adf_etr_ring_data *ring, u32 *msg)
{
	u32 msg_size = 0;

	if (atomic_add_return(1, ring->inflights) > ring->max_inflights) {
		atomic_dec(ring->inflights);
		return EAGAIN;
	}

	msg_size = ADF_MSG_SIZE_TO_BYTES(ring->msg_size);
	mtx_lock(&ring->lock);
	memcpy((void *)((uintptr_t)ring->base_addr + ring->tail),
	       msg,
	       msg_size);

	ring->tail = adf_modulo(ring->tail + msg_size,
				ADF_RING_SIZE_MODULO(ring->ring_size));

	WRITE_CSR_RING_TAIL(ring->bank->csr_addr,
			    ring->bank->bank_number,
			    ring->ring_number,
			    ring->tail);
	ring->csr_tail_offset = ring->tail;
	mtx_unlock(&ring->lock);
	return 0;
}

int
adf_handle_response(struct adf_etr_ring_data *ring, u32 quota)
{
	u32 msg_counter = 0;
	u32 *msg = (u32 *)((uintptr_t)ring->base_addr + ring->head);

	if (!quota)
		quota = ADF_NO_RESPONSE_QUOTA;

	while ((*msg != ADF_RING_EMPTY_SIG) && (msg_counter < quota)) {
		ring->callback((u32 *)msg);
		atomic_dec(ring->inflights);
		*msg = ADF_RING_EMPTY_SIG;
		ring->head = adf_modulo(ring->head + ADF_MSG_SIZE_TO_BYTES(
							 ring->msg_size),
					ADF_RING_SIZE_MODULO(ring->ring_size));
		msg_counter++;
		msg = (u32 *)((uintptr_t)ring->base_addr + ring->head);
	}
	if (msg_counter > 0)
		WRITE_CSR_RING_HEAD(ring->bank->csr_addr,
				    ring->bank->bank_number,
				    ring->ring_number,
				    ring->head);
	return msg_counter;
}

int
adf_poll_bank(u32 accel_id, u32 bank_num, u32 quota)
{
	int num_resp;
	struct adf_accel_dev *accel_dev;
	struct adf_etr_data *trans_data;
	struct adf_etr_bank_data *bank;
	struct adf_etr_ring_data *ring;
	u32 rings_not_empty;
	u32 ring_num;
	u32 resp_total = 0;
	u32 num_rings_per_bank;

	/* Find the accel device associated with the accelId
	 * passed in.
	 */
	accel_dev = adf_devmgr_get_dev_by_id(accel_id);
	if (!accel_dev) {
		pr_err("There is no device with id: %d\n", accel_id);
		return EINVAL;
	}

	trans_data = accel_dev->transport;
	bank = &trans_data->banks[bank_num];
	mtx_lock(&bank->lock);

	/* Read the ring status CSR to determine which rings are empty. */
	rings_not_empty = READ_CSR_E_STAT(bank->csr_addr, bank->bank_number);
	/* Complement to find which rings have data to be processed. */
	rings_not_empty = (~rings_not_empty) & bank->ring_mask;

	/* Return RETRY if the bank polling rings
	 * are all empty.
	 */
	if (!(rings_not_empty & bank->ring_mask)) {
		mtx_unlock(&bank->lock);
		return EAGAIN;
	}

	/*
	 * Loop over all rings within this bank.
	 * The ring structure is global to all
	 * rings hence while we loop over all rings in the
	 * bank we use ring_number to get the global ring.
	 */
	num_rings_per_bank = accel_dev->hw_device->num_rings_per_bank;
	for (ring_num = 0; ring_num < num_rings_per_bank; ring_num++) {
		ring = &bank->rings[ring_num];

		/* And with polling ring mask.
		 * If the there is no data on this ring
		 * move to the next one.
		 */
		if (!(rings_not_empty & (1 << ring->ring_number)))
			continue;

		/* Poll the ring. */
		num_resp = adf_handle_response(ring, quota);
		resp_total += num_resp;
	}

	mtx_unlock(&bank->lock);
	/* Return SUCCESS if there's any response message
	 * returned.
	 */
	if (resp_total)
		return 0;
	return EAGAIN;
}

int
adf_poll_all_banks(u32 accel_id, u32 quota)
{
	int status = EAGAIN;
	struct adf_accel_dev *accel_dev;
	struct adf_etr_data *trans_data;
	struct adf_etr_bank_data *bank;
	u32 bank_num;
	u32 stat_total = 0;

	/* Find the accel device associated with the accelId
	 * passed in.
	 */
	accel_dev = adf_devmgr_get_dev_by_id(accel_id);
	if (!accel_dev) {
		pr_err("There is no device with id: %d\n", accel_id);
		return EINVAL;
	}

	/* Loop over banks and call adf_poll_bank */
	trans_data = accel_dev->transport;
	for (bank_num = 0; bank_num < GET_MAX_BANKS(accel_dev); bank_num++) {
		bank = &trans_data->banks[bank_num];
		/* if there are no polling rings on this bank
		 * continue to the next bank number.
		 */
		if (bank->ring_mask == 0)
			continue;
		status = adf_poll_bank(accel_id, bank_num, quota);
		/* The successful status should be AGAIN or 0 */
		if (status == 0)
			stat_total++;
		else if (status != EAGAIN)
			return status;
	}

	/* Return SUCCESS if adf_poll_bank returned SUCCESS
	 * at any stage. adf_poll_bank cannot
	 * return fail in the above case.
	 */
	if (stat_total)
		return 0;

	return EAGAIN;
}

static void
adf_configure_tx_ring(struct adf_etr_ring_data *ring)
{
	u32 ring_config = BUILD_RING_CONFIG(ring->ring_size);

	WRITE_CSR_RING_CONFIG(ring->bank->csr_addr,
			      ring->bank->bank_number,
			      ring->ring_number,
			      ring_config);
}

static void
adf_configure_rx_ring(struct adf_etr_ring_data *ring)
{
	u32 ring_config = BUILD_RESP_RING_CONFIG(ring->ring_size,
						 ADF_RING_NEAR_WATERMARK_512,
						 ADF_RING_NEAR_WATERMARK_0);

	WRITE_CSR_RING_CONFIG(ring->bank->csr_addr,
			      ring->bank->bank_number,
			      ring->ring_number,
			      ring_config);
}

static int
adf_init_ring(struct adf_etr_ring_data *ring)
{
	struct adf_etr_bank_data *bank = ring->bank;
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u64 ring_base;
	u32 ring_size_bytes = ADF_SIZE_TO_RING_SIZE_IN_BYTES(ring->ring_size);

	ring_size_bytes = ADF_RING_SIZE_BYTES_MIN(ring_size_bytes);
	int ret;

	ret = bus_dma_mem_create(&ring->dma_mem,
				 accel_dev->dma_tag,
				 ring_size_bytes,
				 BUS_SPACE_MAXADDR,
				 ring_size_bytes,
				 M_WAITOK | M_ZERO);
	if (ret)
		return ret;
	ring->base_addr = ring->dma_mem.dma_vaddr;
	ring->dma_addr = ring->dma_mem.dma_baddr;

	memset(ring->base_addr, 0x7F, ring_size_bytes);
	/* The base_addr has to be aligned to the size of the buffer */
	if (adf_check_ring_alignment(ring->dma_addr, ring_size_bytes)) {
		device_printf(GET_DEV(accel_dev), "Ring address not aligned\n");
		bus_dma_mem_free(&ring->dma_mem);
		ring->base_addr = NULL;
		return EFAULT;
	}

	if (hw_data->tx_rings_mask & (1 << ring->ring_number))
		adf_configure_tx_ring(ring);
	else
		adf_configure_rx_ring(ring);

	ring_base = BUILD_RING_BASE_ADDR(ring->dma_addr, ring->ring_size);
	WRITE_CSR_RING_BASE(ring->bank->csr_addr,
			    ring->bank->bank_number,
			    ring->ring_number,
			    ring_base);
	mtx_init(&ring->lock, "adf bank", NULL, MTX_DEF);
	return 0;
}

static void
adf_cleanup_ring(struct adf_etr_ring_data *ring)
{
	u32 ring_size_bytes = ADF_SIZE_TO_RING_SIZE_IN_BYTES(ring->ring_size);
	ring_size_bytes = ADF_RING_SIZE_BYTES_MIN(ring_size_bytes);

	if (ring->base_addr) {
		explicit_bzero(ring->base_addr, ring_size_bytes);
		bus_dma_mem_free(&ring->dma_mem);
	}
	mtx_destroy(&ring->lock);
}

int
adf_create_ring(struct adf_accel_dev *accel_dev,
		const char *section,
		u32 bank_num,
		u32 num_msgs,
		u32 msg_size,
		const char *ring_name,
		adf_callback_fn callback,
		int poll_mode,
		struct adf_etr_ring_data **ring_ptr)
{
	struct adf_etr_data *transport_data = accel_dev->transport;
	struct adf_etr_bank_data *bank;
	struct adf_etr_ring_data *ring;
	char val[ADF_CFG_MAX_VAL_LEN_IN_BYTES];
	u32 ring_num;
	int ret;
	u8 num_rings_per_bank = accel_dev->hw_device->num_rings_per_bank;

	if (bank_num >= GET_MAX_BANKS(accel_dev)) {
		device_printf(GET_DEV(accel_dev), "Invalid bank number\n");
		return EFAULT;
	}
	if (msg_size > ADF_MSG_SIZE_TO_BYTES(ADF_MAX_MSG_SIZE)) {
		device_printf(GET_DEV(accel_dev), "Invalid msg size\n");
		return EFAULT;
	}
	if (ADF_MAX_INFLIGHTS(adf_verify_ring_size(msg_size, num_msgs),
			      ADF_BYTES_TO_MSG_SIZE(msg_size)) < 2) {
		device_printf(GET_DEV(accel_dev),
			      "Invalid ring size for given msg size\n");
		return EFAULT;
	}
	if (adf_cfg_get_param_value(accel_dev, section, ring_name, val)) {
		device_printf(GET_DEV(accel_dev),
			      "Section %s, no such entry : %s\n",
			      section,
			      ring_name);
		return EFAULT;
	}
	if (compat_strtouint(val, 10, &ring_num)) {
		device_printf(GET_DEV(accel_dev), "Can't get ring number\n");
		return EFAULT;
	}
	if (ring_num >= num_rings_per_bank) {
		device_printf(GET_DEV(accel_dev), "Invalid ring number\n");
		return EFAULT;
	}

	bank = &transport_data->banks[bank_num];
	if (adf_reserve_ring(bank, ring_num)) {
		device_printf(GET_DEV(accel_dev),
			      "Ring %d, %s already exists.\n",
			      ring_num,
			      ring_name);
		return EFAULT;
	}
	ring = &bank->rings[ring_num];
	ring->ring_number = ring_num;
	ring->bank = bank;
	ring->callback = callback;
	ring->msg_size = ADF_BYTES_TO_MSG_SIZE(msg_size);
	ring->ring_size = adf_verify_ring_size(msg_size, num_msgs);
	ring->max_inflights =
	    ADF_MAX_INFLIGHTS(ring->ring_size, ring->msg_size);
	ring->head = 0;
	ring->tail = 0;
	ring->csr_tail_offset = 0;
	ret = adf_init_ring(ring);
	if (ret)
		goto err;

	/* Enable HW arbitration for the given ring */
	adf_update_ring_arb(ring);

	if (adf_ring_debugfs_add(ring, ring_name)) {
		device_printf(GET_DEV(accel_dev),
			      "Couldn't add ring debugfs entry\n");
		ret = EFAULT;
		goto err;
	}

	/* Enable interrupts if needed */
	if (callback && !poll_mode)
		adf_enable_ring_irq(bank, ring->ring_number);
	*ring_ptr = ring;
	return 0;
err:
	adf_cleanup_ring(ring);
	adf_unreserve_ring(bank, ring_num);
	adf_update_ring_arb(ring);
	return ret;
}

void
adf_remove_ring(struct adf_etr_ring_data *ring)
{
	struct adf_etr_bank_data *bank = ring->bank;

	/* Disable interrupts for the given ring */
	adf_disable_ring_irq(bank, ring->ring_number);

	/* Clear PCI config space */
	WRITE_CSR_RING_CONFIG(bank->csr_addr,
			      bank->bank_number,
			      ring->ring_number,
			      0);
	WRITE_CSR_RING_BASE(bank->csr_addr,
			    bank->bank_number,
			    ring->ring_number,
			    0);
	adf_ring_debugfs_rm(ring);
	adf_unreserve_ring(bank, ring->ring_number);
	/* Disable HW arbitration for the given ring */
	adf_update_ring_arb(ring);
	adf_cleanup_ring(ring);
}

static void
adf_ring_response_handler(struct adf_etr_bank_data *bank)
{
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;
	u32 empty_rings, i;

	empty_rings = READ_CSR_E_STAT(bank->csr_addr, bank->bank_number);
	empty_rings = ~empty_rings & bank->irq_mask;

	for (i = 0; i < num_rings_per_bank; ++i) {
		if (empty_rings & (1 << i))
			adf_handle_response(&bank->rings[i], 0);
	}
}

void
adf_response_handler(uintptr_t bank_addr)
{
	struct adf_etr_bank_data *bank = (void *)bank_addr;

	/* Handle all the responses and re-enable IRQs */
	adf_ring_response_handler(bank);
	WRITE_CSR_INT_FLAG_AND_COL(bank->csr_addr,
				   bank->bank_number,
				   bank->irq_mask);
}

static inline int
adf_get_cfg_int(struct adf_accel_dev *accel_dev,
		const char *section,
		const char *format,
		u32 key,
		u32 *value)
{
	char key_buf[ADF_CFG_MAX_KEY_LEN_IN_BYTES];
	char val_buf[ADF_CFG_MAX_VAL_LEN_IN_BYTES];

	snprintf(key_buf, ADF_CFG_MAX_KEY_LEN_IN_BYTES, format, key);

	if (adf_cfg_get_param_value(accel_dev, section, key_buf, val_buf))
		return EFAULT;

	if (compat_strtouint(val_buf, 10, value))
		return EFAULT;
	return 0;
}

static void
adf_get_coalesc_timer(struct adf_etr_bank_data *bank,
		      const char *section,
		      u32 bank_num_in_accel)
{
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u32 coalesc_timer = ADF_COALESCING_DEF_TIME;

	adf_get_cfg_int(accel_dev,
			section,
			ADF_ETRMGR_COALESCE_TIMER_FORMAT,
			bank_num_in_accel,
			&coalesc_timer);

	if (hw_data->get_clock_speed)
		bank->irq_coalesc_timer =
		    (coalesc_timer *
		     (hw_data->get_clock_speed(hw_data) / USEC_PER_SEC)) /
		    NSEC_PER_USEC;
	else
		bank->irq_coalesc_timer = coalesc_timer;

	if (bank->irq_coalesc_timer > ADF_COALESCING_MAX_TIME)
		bank->irq_coalesc_timer = ADF_COALESCING_MAX_TIME;
	else if (bank->irq_coalesc_timer < ADF_COALESCING_MIN_TIME)
		bank->irq_coalesc_timer = ADF_COALESCING_MIN_TIME;
}

static int
adf_init_bank(struct adf_accel_dev *accel_dev,
	      struct adf_etr_bank_data *bank,
	      u32 bank_num,
	      struct resource *csr_addr)
{
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct adf_etr_ring_data *ring;
	struct adf_etr_ring_data *tx_ring;
	u32 i, coalesc_enabled = 0;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;
	u32 size = 0;

	explicit_bzero(bank, sizeof(*bank));
	bank->bank_number = bank_num;
	bank->csr_addr = csr_addr;
	bank->accel_dev = accel_dev;
	mtx_init(&bank->lock, "adf bank", NULL, MTX_DEF);

	/* Allocate the rings in the bank */
	size = num_rings_per_bank * sizeof(struct adf_etr_ring_data);
	bank->rings = kzalloc_node(size,
				   M_WAITOK | M_ZERO,
				   dev_to_node(GET_DEV(accel_dev)));

	/* Enable IRQ coalescing always. This will allow to use
	 * the optimised flag and coalesc register.
	 * If it is disabled in the config file just use min time value */
	if ((adf_get_cfg_int(accel_dev,
			     "Accelerator0",
			     ADF_ETRMGR_COALESCING_ENABLED_FORMAT,
			     bank_num,
			     &coalesc_enabled) == 0) &&
	    coalesc_enabled)
		adf_get_coalesc_timer(bank, "Accelerator0", bank_num);
	else
		bank->irq_coalesc_timer = ADF_COALESCING_MIN_TIME;

	for (i = 0; i < num_rings_per_bank; i++) {
		WRITE_CSR_RING_CONFIG(csr_addr, bank_num, i, 0);
		WRITE_CSR_RING_BASE(csr_addr, bank_num, i, 0);
		ring = &bank->rings[i];
		if (hw_data->tx_rings_mask & (1 << i)) {
			ring->inflights =
			    kzalloc_node(sizeof(atomic_t),
					 M_WAITOK | M_ZERO,
					 dev_to_node(GET_DEV(accel_dev)));
		} else {
			if (i < hw_data->tx_rx_gap) {
				device_printf(GET_DEV(accel_dev),
					      "Invalid tx rings mask config\n");
				goto err;
			}
			tx_ring = &bank->rings[i - hw_data->tx_rx_gap];
			ring->inflights = tx_ring->inflights;
		}
	}

	if (adf_bank_debugfs_add(bank)) {
		device_printf(GET_DEV(accel_dev),
			      "Failed to add bank debugfs entry\n");
		goto err;
	}

	WRITE_CSR_INT_FLAG(csr_addr, bank_num, ADF_BANK_INT_FLAG_CLEAR_MASK);
	WRITE_CSR_INT_SRCSEL(csr_addr, bank_num);
	return 0;
err:
	for (i = 0; i < num_rings_per_bank; i++) {
		ring = &bank->rings[i];
		if (hw_data->tx_rings_mask & (1 << i)) {
			kfree(ring->inflights);
			ring->inflights = NULL;
		}
	}
	kfree(bank->rings);
	return ENOMEM;
}

/**
 * adf_init_etr_data() - Initialize transport rings for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function initializes the communications channels (rings) to the
 * acceleration device accel_dev.
 * To be used by QAT device specific drivers.
 *
 * Return: 0 on success, error code otherwise.
 */
int
adf_init_etr_data(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	struct resource *csr_addr;
	u32 size;
	u32 num_banks = 0;
	int i, ret;

	etr_data = kzalloc_node(sizeof(*etr_data),
				M_WAITOK | M_ZERO,
				dev_to_node(GET_DEV(accel_dev)));

	num_banks = GET_MAX_BANKS(accel_dev);
	size = num_banks * sizeof(struct adf_etr_bank_data);
	etr_data->banks = kzalloc_node(size,
				       M_WAITOK | M_ZERO,
				       dev_to_node(GET_DEV(accel_dev)));

	accel_dev->transport = etr_data;
	i = hw_data->get_etr_bar_id(hw_data);
	csr_addr = accel_dev->accel_pci_dev.pci_bars[i].virt_addr;

	etr_data->debug =
	    SYSCTL_ADD_NODE(&accel_dev->sysctl_ctx,
			    SYSCTL_CHILDREN(
				device_get_sysctl_tree(GET_DEV(accel_dev))),
			    OID_AUTO,
			    "transport",
			    CTLFLAG_RD,
			    NULL,
			    "Transport parameters");
	if (!etr_data->debug) {
		device_printf(GET_DEV(accel_dev),
			      "Unable to create transport debugfs entry\n");
		ret = ENOENT;
		goto err_bank_all;
	}

	for (i = 0; i < num_banks; i++) {
		ret =
		    adf_init_bank(accel_dev, &etr_data->banks[i], i, csr_addr);
		if (ret)
			goto err_bank_all;
	}

	return 0;

err_bank_all:
	kfree(etr_data->banks);
	kfree(etr_data);
	accel_dev->transport = NULL;
	return ret;
}

static void
cleanup_bank(struct adf_etr_bank_data *bank)
{
	u32 i;
	struct adf_accel_dev *accel_dev = bank->accel_dev;
	struct adf_hw_device_data *hw_data = accel_dev->hw_device;
	u8 num_rings_per_bank = hw_data->num_rings_per_bank;

	for (i = 0; i < num_rings_per_bank; i++) {
		struct adf_accel_dev *accel_dev = bank->accel_dev;
		struct adf_hw_device_data *hw_data = accel_dev->hw_device;
		struct adf_etr_ring_data *ring = &bank->rings[i];

		if (bank->ring_mask & (1 << i))
			adf_cleanup_ring(ring);

		if (hw_data->tx_rings_mask & (1 << i)) {
			kfree(ring->inflights);
			ring->inflights = NULL;
		}
	}
	kfree(bank->rings);
	adf_bank_debugfs_rm(bank);
	mtx_destroy(&bank->lock);
	explicit_bzero(bank, sizeof(*bank));
}

static void
adf_cleanup_etr_handles(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data = accel_dev->transport;
	u32 i, num_banks = GET_MAX_BANKS(accel_dev);

	for (i = 0; i < num_banks; i++)
		cleanup_bank(&etr_data->banks[i]);
}

/**
 * adf_cleanup_etr_data() - Clear transport rings for acceleration device
 * @accel_dev:  Pointer to acceleration device.
 *
 * Function is the clears the communications channels (rings) of the
 * acceleration device accel_dev.
 * To be used by QAT device specific drivers.
 *
 * Return: void
 */
void
adf_cleanup_etr_data(struct adf_accel_dev *accel_dev)
{
	struct adf_etr_data *etr_data = accel_dev->transport;

	if (etr_data) {
		adf_cleanup_etr_handles(accel_dev);
		kfree(etr_data->banks);
		kfree(etr_data);
		accel_dev->transport = NULL;
	}
}
