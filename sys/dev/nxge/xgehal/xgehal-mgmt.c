/*-
 * Copyright (c) 2002-2007 Neterion, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */

#include <dev/nxge/include/xgehal-mgmt.h>
#include <dev/nxge/include/xgehal-driver.h>
#include <dev/nxge/include/xgehal-device.h>

/**
 * xge_hal_mgmt_about - Retrieve about info.
 * @devh: HAL device handle.
 * @about_info: Filled in by HAL. See xge_hal_mgmt_about_info_t{}.
 * @size: Size of the @about_info buffer. HAL will return error if the
 *        size is smaller than sizeof(xge_hal_mgmt_about_info_t).
 *
 * Retrieve information such as PCI device and vendor IDs, board
 * revision number, HAL version number, etc.
 *
 * Returns: XGE_HAL_OK - success;
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 * XGE_HAL_FAIL - Failed to retrieve the information.
 *
 * See also: xge_hal_mgmt_about_info_t{}.
 */
xge_hal_status_e
xge_hal_mgmt_about(xge_hal_device_h devh, xge_hal_mgmt_about_info_t *about_info,
	    int size)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (size != sizeof(xge_hal_mgmt_about_info_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, vendor_id),
	    &about_info->vendor);

	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, device_id),
	    &about_info->device);

	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, subsystem_vendor_id),
	    &about_info->subsys_vendor);

	xge_os_pci_read16(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, subsystem_id),
	    &about_info->subsys_device);

	xge_os_pci_read8(hldev->pdev, hldev->cfgh,
	    xge_offsetof(xge_hal_pci_config_le_t, revision),
	    &about_info->board_rev);

	xge_os_strcpy(about_info->vendor_name, XGE_DRIVER_VENDOR);
	xge_os_strcpy(about_info->chip_name, XGE_CHIP_FAMILY);
	xge_os_strcpy(about_info->media, XGE_SUPPORTED_MEDIA_0);

	xge_os_strcpy(about_info->hal_major, XGE_HAL_VERSION_MAJOR);
	xge_os_strcpy(about_info->hal_minor, XGE_HAL_VERSION_MINOR);
	xge_os_strcpy(about_info->hal_fix,   XGE_HAL_VERSION_FIX);
	xge_os_strcpy(about_info->hal_build, XGE_HAL_VERSION_BUILD);

	xge_os_strcpy(about_info->ll_major, XGELL_VERSION_MAJOR);
	xge_os_strcpy(about_info->ll_minor, XGELL_VERSION_MINOR);
	xge_os_strcpy(about_info->ll_fix,   XGELL_VERSION_FIX);
	xge_os_strcpy(about_info->ll_build, XGELL_VERSION_BUILD);

	about_info->transponder_temperature =
	    xge_hal_read_xfp_current_temp(devh);

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_reg_read - Read Xframe register.
 * @devh: HAL device handle.
 * @bar_id: 0 - for BAR0, 1- for BAR1.
 * @offset: Register offset in the Base Address Register (BAR) space.
 * @value: Register value. Returned by HAL.
 * Read Xframe register.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 * XGE_HAL_ERR_INVALID_BAR_ID - BAR id is not valid.
 *
 * See also: xge_hal_aux_bar0_read(), xge_hal_aux_bar1_read().
 */
xge_hal_status_e
xge_hal_mgmt_reg_read(xge_hal_device_h devh, int bar_id, unsigned int offset,
	    u64 *value)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (bar_id == 0) {
	    if (offset > sizeof(xge_hal_pci_bar0_t)-8) {
	        return XGE_HAL_ERR_INVALID_OFFSET;
	    }
	    *value = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                     (void *)(hldev->bar0 + offset));
	} else if (bar_id == 1 &&
	       (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA ||
	        xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC))  {
	    int i;
	    for (i=0; i<XGE_HAL_MAX_FIFO_NUM_HERC; i++) {
	        if (offset == i*0x2000 || offset == i*0x2000+0x18) {
	            break;
	        }
	    }
	    if (i == XGE_HAL_MAX_FIFO_NUM_HERC) {
	        return XGE_HAL_ERR_INVALID_OFFSET;
	    }
	    *value = xge_os_pio_mem_read64(hldev->pdev, hldev->regh1,
	                     (void *)(hldev->bar1 + offset));
	} else if (bar_id == 1) {
	    /* FIXME: check TITAN BAR1 offsets */
	} else {
	    return XGE_HAL_ERR_INVALID_BAR_ID;
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_reg_write - Write Xframe register.
 * @devh: HAL device handle.
 * @bar_id: 0 - for BAR0, 1- for BAR1.
 * @offset: Register offset in the Base Address Register (BAR) space.
 * @value: Register value.
 *
 * Write Xframe register.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 * XGE_HAL_ERR_INVALID_BAR_ID - BAR id is not valid.
 *
 * See also: xge_hal_aux_bar0_write().
 */
xge_hal_status_e
xge_hal_mgmt_reg_write(xge_hal_device_h devh, int bar_id, unsigned int offset,
	    u64 value)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (bar_id == 0) {
	    if (offset > sizeof(xge_hal_pci_bar0_t)-8) {
	        return XGE_HAL_ERR_INVALID_OFFSET;
	    }
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, value,
	                 (void *)(hldev->bar0 + offset));
	} else if (bar_id == 1 &&
	       (xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA ||
	        xge_hal_device_check_id(hldev) == XGE_HAL_CARD_HERC))  {
	    int i;
	    for (i=0; i<XGE_HAL_MAX_FIFO_NUM_HERC; i++) {
	        if (offset == i*0x2000 || offset == i*0x2000+0x18) {
	            break;
	        }
	    }
	    if (i == XGE_HAL_MAX_FIFO_NUM_HERC) {
	        return XGE_HAL_ERR_INVALID_OFFSET;
	    }
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh1, value,
	                 (void *)(hldev->bar1 + offset));
	} else if (bar_id == 1) {
	    /* FIXME: check TITAN BAR1 offsets */
	} else {
	    return XGE_HAL_ERR_INVALID_BAR_ID;
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_hw_stats - Get Xframe hardware statistics.
 * @devh: HAL device handle.
 * @hw_stats: Hardware statistics. Returned by HAL.
 *            See xge_hal_stats_hw_info_t{}.
 * @size: Size of the @hw_stats buffer. HAL will return an error
 * if the size is smaller than sizeof(xge_hal_stats_hw_info_t).
 * Get Xframe hardware statistics.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: xge_hal_mgmt_sw_stats().
 */
xge_hal_status_e
xge_hal_mgmt_hw_stats(xge_hal_device_h devh, xge_hal_mgmt_hw_stats_t *hw_stats,
	    int size)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_stats_hw_info_t *hw_info;

	xge_assert(xge_hal_device_check_id(hldev) != XGE_HAL_CARD_TITAN);

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (size != sizeof(xge_hal_stats_hw_info_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	if ((status = xge_hal_stats_hw (devh, &hw_info)) != XGE_HAL_OK) {
	    return status;
	}

	xge_os_memcpy(hw_stats, hw_info, sizeof(xge_hal_stats_hw_info_t));

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_hw_stats_off - TBD.
 * @devh: HAL device handle.
 * @off: TBD
 * @size: TBD
 * @out: TBD
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: xge_hal_mgmt_sw_stats().
 */
xge_hal_status_e
xge_hal_mgmt_hw_stats_off(xge_hal_device_h devh, int off, int size, char *out)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_stats_hw_info_t *hw_info;

	xge_assert(xge_hal_device_check_id(hldev) != XGE_HAL_CARD_TITAN);

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (off > sizeof(xge_hal_stats_hw_info_t)-4 ||
	    size > 8) {
	    return XGE_HAL_ERR_INVALID_OFFSET;
	}

	if ((status = xge_hal_stats_hw (devh, &hw_info)) != XGE_HAL_OK) {
	    return status;
	}

	xge_os_memcpy(out, (char*)hw_info + off, size);

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_pcim_stats - Get Titan hardware statistics.
 * @devh: HAL device handle.
 * @pcim_stats: PCIM statistics. Returned by HAL.
 *            See xge_hal_stats_hw_info_t{}.
 * @size: Size of the @hw_stats buffer. HAL will return an error
 * if the size is smaller than sizeof(xge_hal_stats_hw_info_t).
 * Get Xframe hardware statistics.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: xge_hal_mgmt_sw_stats().
 */
xge_hal_status_e
xge_hal_mgmt_pcim_stats(xge_hal_device_h devh,
	    xge_hal_mgmt_pcim_stats_t *pcim_stats, int size)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_stats_pcim_info_t   *pcim_info;

	xge_assert(xge_hal_device_check_id(hldev) == XGE_HAL_CARD_TITAN);

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (size != sizeof(xge_hal_stats_pcim_info_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	if ((status = xge_hal_stats_pcim (devh, &pcim_info)) != XGE_HAL_OK) {
	    return status;
	}

	xge_os_memcpy(pcim_stats, pcim_info,
	    sizeof(xge_hal_stats_pcim_info_t));

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_pcim_stats_off - TBD.
 * @devh: HAL device handle.
 * @off: TBD
 * @size: TBD
 * @out: TBD
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: xge_hal_mgmt_sw_stats().
 */
xge_hal_status_e
xge_hal_mgmt_pcim_stats_off(xge_hal_device_h devh, int off, int size,
	            char *out)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_stats_pcim_info_t   *pcim_info;

	xge_assert(xge_hal_device_check_id(hldev) == XGE_HAL_CARD_TITAN);

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (off > sizeof(xge_hal_stats_pcim_info_t)-8 ||
	    size > 8) {
	    return XGE_HAL_ERR_INVALID_OFFSET;
	}

	if ((status = xge_hal_stats_pcim (devh, &pcim_info)) != XGE_HAL_OK) {
	    return status;
	}

	xge_os_memcpy(out, (char*)pcim_info + off, size);

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_sw_stats - Get per-device software statistics.
 * @devh: HAL device handle.
 * @sw_stats: Hardware statistics. Returned by HAL.
 *            See xge_hal_stats_sw_err_t{}.
 * @size: Size of the @sw_stats buffer. HAL will return an error
 * if the size is smaller than sizeof(xge_hal_stats_sw_err_t).
 * Get device software statistics, including ECC and Parity error
 * counters, etc.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: xge_hal_stats_sw_err_t{}, xge_hal_mgmt_hw_stats().
 */
xge_hal_status_e
xge_hal_mgmt_sw_stats(xge_hal_device_h devh, xge_hal_mgmt_sw_stats_t *sw_stats,
	    int size)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (size != sizeof(xge_hal_stats_sw_err_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	if (!hldev->stats.is_initialized ||
	    !hldev->stats.is_enabled) {
	    return XGE_HAL_INF_STATS_IS_NOT_READY;
	}

	/* Updating xpak stats value */
	__hal_updt_stats_xpak(hldev);

	xge_os_memcpy(sw_stats, &hldev->stats.sw_dev_err_stats,
	            sizeof(xge_hal_stats_sw_err_t));

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_device_stats - Get HAL device statistics.
 * @devh: HAL device handle.
 * @device_stats: HAL device "soft" statistics. Maintained by HAL itself.
 *            (as opposed to xge_hal_mgmt_hw_stats() - those are
 *            maintained by the Xframe hardware).
 *            Returned by HAL.
 *            See xge_hal_stats_device_info_t{}.
 * @size: Size of the @device_stats buffer. HAL will return an error
 * if the size is smaller than sizeof(xge_hal_stats_device_info_t).
 *
 * Get HAL (layer) statistic counters.
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 * XGE_HAL_INF_STATS_IS_NOT_READY - Statistics information is not
 * currently available.
 *
 */
xge_hal_status_e
xge_hal_mgmt_device_stats(xge_hal_device_h devh,
	    xge_hal_mgmt_device_stats_t *device_stats, int size)
{
	xge_hal_status_e status;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_stats_device_info_t *device_info;

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (size != sizeof(xge_hal_stats_device_info_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	if ((status = xge_hal_stats_device (devh, &device_info)) !=
	XGE_HAL_OK) {
	    return status;
	}

	xge_os_memcpy(device_stats, device_info,
	        sizeof(xge_hal_stats_device_info_t));

	return XGE_HAL_OK;
}

/*
 * __hal_update_ring_bump - Update the ring bump counter for the
 * particular channel.
 * @hldev: HAL device handle.
 * @queue: the queue who's data is to be collected.
 * @chinfo: pointer to the statistics structure of the given channel.
 * Usage: See xge_hal_aux_stats_hal_read{}
 */

static void
__hal_update_ring_bump(xge_hal_device_t *hldev, int queue,
	xge_hal_stats_channel_info_t *chinfo)
{
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 rbc = 0;
	int reg = (queue / 4);
	void * addr;

	addr = (reg == 1)? (&bar0->ring_bump_counter2) :
	    (&bar0->ring_bump_counter1);
	rbc = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0, addr);
	chinfo->ring_bump_cnt = XGE_HAL_RING_BUMP_CNT(queue, rbc);
}

/**
 * xge_hal_mgmt_channel_stats - Get HAL channel statistics.
 * @channelh: HAL channel handle.
 * @channel_stats: HAL channel statistics. Maintained by HAL itself
 *            (as opposed to xge_hal_mgmt_hw_stats() - those are
 *            maintained by the Xframe hardware).
 *            Returned by HAL.
 *            See xge_hal_stats_channel_info_t{}.
 * @size: Size of the @channel_stats buffer. HAL will return an error
 * if the size is smaller than sizeof(xge_hal_mgmt_channel_stats_t).
 *
 * Get HAL per-channel statistic counters.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 * XGE_HAL_INF_STATS_IS_NOT_READY - Statistics information is not
 * currently available.
 *
 */
xge_hal_status_e
xge_hal_mgmt_channel_stats(xge_hal_channel_h channelh,
	    xge_hal_mgmt_channel_stats_t *channel_stats, int size)
{
	xge_hal_status_e status;
	xge_hal_stats_channel_info_t *channel_info;
	xge_hal_channel_t *channel = (xge_hal_channel_t* ) channelh;

	if (size != sizeof(xge_hal_stats_channel_info_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	if ((status = xge_hal_stats_channel (channelh, &channel_info)) !=
	                            XGE_HAL_OK) {
	    return status;
	}

	if (xge_hal_device_check_id(channel->devh) == XGE_HAL_CARD_HERC) {
	    __hal_update_ring_bump( (xge_hal_device_t *) channel->devh, channel->post_qid, channel_info);
	}

	xge_os_memcpy(channel_stats, channel_info,
	        sizeof(xge_hal_stats_channel_info_t));

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_pcireg_read - Read PCI configuration at a specified
 * offset.
 * @devh: HAL device handle.
 * @offset: Offset in the 256 byte PCI configuration space.
 * @value_bits: 8, 16, or 32 (bits) to read.
 * @value: Value returned by HAL.
 *
 * Read PCI configuration, given device and offset in the PCI space.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 * XGE_HAL_ERR_INVALID_VALUE_BIT_SIZE - Invalid bits size. Valid
 * values(8/16/32).
 *
 */
xge_hal_status_e
xge_hal_mgmt_pcireg_read(xge_hal_device_h devh, unsigned int offset,
	    int value_bits, u32 *value)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (offset > sizeof(xge_hal_pci_config_t)-value_bits/8) {
	    return XGE_HAL_ERR_INVALID_OFFSET;
	}

	if (value_bits == 8) {
	    xge_os_pci_read8(hldev->pdev, hldev->cfgh, offset, (u8*)value);
	} else if (value_bits == 16) {
	    xge_os_pci_read16(hldev->pdev, hldev->cfgh, offset,
	    (u16*)value);
	} else if (value_bits == 32) {
	    xge_os_pci_read32(hldev->pdev, hldev->cfgh, offset, value);
	} else {
	    return XGE_HAL_ERR_INVALID_VALUE_BIT_SIZE;
	}

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_device_config - Retrieve device configuration.
 * @devh: HAL device handle.
 * @dev_config: Device configuration, see xge_hal_device_config_t{}.
 * @size: Size of the @dev_config buffer. HAL will return an error
 * if the size is smaller than sizeof(xge_hal_mgmt_device_config_t).
 *
 * Get device configuration. Permits to retrieve at run-time configuration
 * values that were used to initialize and configure the device.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: xge_hal_device_config_t{}, xge_hal_mgmt_driver_config().
 */
xge_hal_status_e
xge_hal_mgmt_device_config(xge_hal_device_h devh,
	    xge_hal_mgmt_device_config_t    *dev_config, int size)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (size != sizeof(xge_hal_mgmt_device_config_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	xge_os_memcpy(dev_config, &hldev->config,
	sizeof(xge_hal_device_config_t));

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_driver_config - Retrieve driver configuration.
 * @drv_config: Device configuration, see xge_hal_driver_config_t{}.
 * @size: Size of the @dev_config buffer. HAL will return an error
 * if the size is smaller than sizeof(xge_hal_mgmt_driver_config_t).
 *
 * Get driver configuration. Permits to retrieve at run-time configuration
 * values that were used to configure the device at load-time.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_DRIVER_NOT_INITIALIZED - HAL is not initialized.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version is not maching.
 *
 * See also: xge_hal_driver_config_t{}, xge_hal_mgmt_device_config().
 */
xge_hal_status_e
xge_hal_mgmt_driver_config(xge_hal_mgmt_driver_config_t *drv_config, int size)
{

	if (g_xge_hal_driver == NULL) {
	    return XGE_HAL_ERR_DRIVER_NOT_INITIALIZED;
	}

	if (size != sizeof(xge_hal_mgmt_driver_config_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	xge_os_memcpy(drv_config, &g_xge_hal_driver->config,
	        sizeof(xge_hal_mgmt_driver_config_t));

	return XGE_HAL_OK;
}

/**
 * xge_hal_mgmt_pci_config - Retrieve PCI configuration.
 * @devh: HAL device handle.
 * @pci_config: 256 byte long buffer for PCI configuration space.
 * @size: Size of the @ buffer. HAL will return an error
 * if the size is smaller than sizeof(xge_hal_mgmt_pci_config_t).
 *
 * Get PCI configuration. Permits to retrieve at run-time configuration
 * values that were used to configure the device at load-time.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * XGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 */
xge_hal_status_e
xge_hal_mgmt_pci_config(xge_hal_device_h devh,
	    xge_hal_mgmt_pci_config_t *pci_config, int size)
{
	int i;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;

	if ((hldev == NULL) || (hldev->magic != XGE_HAL_MAGIC)) {
	    return XGE_HAL_ERR_INVALID_DEVICE;
	}

	if (size != sizeof(xge_hal_mgmt_pci_config_t)) {
	    return XGE_HAL_ERR_VERSION_CONFLICT;
	}

	/* refresh PCI config space */
	for (i = 0; i < 0x68/4+1; i++) {
	    xge_os_pci_read32(hldev->pdev, hldev->cfgh, i*4,
	                    (u32*)&hldev->pci_config_space + i);
	}

	xge_os_memcpy(pci_config, &hldev->pci_config_space,
	        sizeof(xge_hal_mgmt_pci_config_t));

	return XGE_HAL_OK;
}

#ifdef XGE_TRACE_INTO_CIRCULAR_ARR
/**
 * xge_hal_mgmt_trace_read - Read trace buffer contents.
 * @buffer: Buffer to store the trace buffer contents.
 * @buf_size: Size of the buffer.
 * @offset: Offset in the internal trace buffer to read data.
 * @read_length: Size of the valid data in the buffer.
 *
 * Read  HAL trace buffer contents starting from the offset
 * upto the size of the buffer or till EOF is reached.
 *
 * Returns: XGE_HAL_OK - success.
 * XGE_HAL_EOF_TRACE_BUF - No more data in the trace buffer.
 *
 */
xge_hal_status_e
xge_hal_mgmt_trace_read (char       *buffer,
	        unsigned    buf_size,
	        unsigned    *offset,
	        unsigned    *read_length)
{
	int data_offset;
	int start_offset;

	if ((g_xge_os_tracebuf == NULL) ||
	    (g_xge_os_tracebuf->offset == g_xge_os_tracebuf->size - 2)) {
	    return XGE_HAL_EOF_TRACE_BUF;
	}

	data_offset = g_xge_os_tracebuf->offset + 1;

	if  (*offset >= (unsigned)xge_os_strlen(g_xge_os_tracebuf->data +
	data_offset)) {

	    return XGE_HAL_EOF_TRACE_BUF;
	}

	xge_os_memzero(buffer, buf_size);

	start_offset  =  data_offset + *offset;
	*read_length = xge_os_strlen(g_xge_os_tracebuf->data +
	start_offset);

	if (*read_length  >=  buf_size) {
	    *read_length = buf_size - 1;
	}

	xge_os_memcpy(buffer, g_xge_os_tracebuf->data + start_offset,
	*read_length);

	*offset += *read_length;
	(*read_length) ++;

	return XGE_HAL_OK;
}

#endif

/**
 * xge_hal_restore_link_led - Restore link LED to its original state.
 * @devh: HAL device handle.
 */
void
xge_hal_restore_link_led(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	/*
	 * If the current link state is UP, switch on LED else make it
	 * off.
	 */

	/*
	 * For Xena 3 and lower revision cards, adapter control needs to be
	 * used for making LED ON/OFF.
	 */
	if ((xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA) &&
	   (xge_hal_device_rev(hldev) <= 3)) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                      &bar0->adapter_control);
	    if (hldev->link_state == XGE_HAL_LINK_UP) {
	        val64 |= XGE_HAL_ADAPTER_LED_ON;
	    } else {
	        val64 &= ~XGE_HAL_ADAPTER_LED_ON;
	    }

	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                &bar0->adapter_control);
	    return;
	}

	/*
	 * Use beacon control register to control the LED.
	 * LED link output corresponds to bit 8 of the beacon control
	 * register. Note that, in the case of Xena, beacon control register
	 * represents the gpio control register. In the case of Herc, LED
	 * handling is done by beacon control register as opposed to gpio
	 * control register in Xena. Beacon control is used only to toggle
	 * and the value written into it does not depend on the link state.
	 * It is upto the ULD to toggle the LED even number of times which 
	 * brings the LED to it's original state. 
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->beacon_control);
	val64 |= 0x0000800000000000ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	               val64, &bar0->beacon_control);
}

/**
 * xge_hal_flick_link_led - Flick (blink) link LED.
 * @devh: HAL device handle.
 *
 * Depending on the card revision flicker the link LED by using the
 * beacon control or the adapter_control register.
 */
void
xge_hal_flick_link_led(xge_hal_device_h devh)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64 = 0;

	/*
	 * For Xena 3 and lower revision cards, adapter control needs to be
	 * used for making LED ON/OFF.
	 */
	if ((xge_hal_device_check_id(hldev) == XGE_HAL_CARD_XENA) &&
	   (xge_hal_device_rev(hldev) <= 3)) {
	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                      &bar0->adapter_control);
	    val64 ^= XGE_HAL_ADAPTER_LED_ON;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	                &bar0->adapter_control);
	    return;
	}

	/*
	 * Use beacon control register to control the Link LED.
	 * Note that, in the case of Xena, beacon control register represents
	 * the gpio control register. In the case of Herc, LED handling is
	 * done by beacon control register as opposed to gpio control register
	 * in Xena.
	 */
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                  &bar0->beacon_control);
	val64 ^= XGE_HAL_GPIO_CTRL_GPIO_0;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	               &bar0->beacon_control);
}

/**
 * xge_hal_read_eeprom - Read 4 bytes of data from user given offset.
 * @devh: HAL device handle.
 * @off: offset at which the data must be written
 * @data: output parameter where the data is stored.
 *
 * Read 4 bytes of data from the user given offset and return the
 * read data.
 * Note: will allow to read only part of the EEPROM visible through the
 * I2C bus.
 * Returns: -1 on failure, 0 on success.
 */
xge_hal_status_e
xge_hal_read_eeprom(xge_hal_device_h devh, int off, u32* data)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_status_e ret = XGE_HAL_FAIL;
	u32 exit_cnt = 0;
	u64 val64;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;

	val64 = XGE_HAL_I2C_CONTROL_DEV_ID(XGE_DEV_ID) |
	    XGE_HAL_I2C_CONTROL_ADDR(off) |
	    XGE_HAL_I2C_CONTROL_BYTE_CNT(0x3) |
	    XGE_HAL_I2C_CONTROL_READ | XGE_HAL_I2C_CONTROL_CNTL_START;

	__hal_serial_mem_write64(hldev, val64, &bar0->i2c_control);

	while (exit_cnt < 5) {
	    val64 = __hal_serial_mem_read64(hldev, &bar0->i2c_control);
	    if (XGE_HAL_I2C_CONTROL_CNTL_END(val64)) {
	        *data = XGE_HAL_I2C_CONTROL_GET_DATA(val64);
	        ret = XGE_HAL_OK;
	        break;
	    }
	    exit_cnt++;
	}

	return ret;
}

/*
 * xge_hal_write_eeprom - actually writes the relevant part of the data
 value.
 * @devh: HAL device handle.
 * @off: offset at which the data must be written
 * @data : The data that is to be written
 * @cnt : Number of bytes of the data that are actually to be written into
 * the Eeprom. (max of 3)
 *
 * Actually writes the relevant part of the data value into the Eeprom
 * through the I2C bus.
 * Return value:
 * 0 on success, -1 on failure.
 */

xge_hal_status_e
xge_hal_write_eeprom(xge_hal_device_h devh, int off, u32 data, int cnt)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_status_e ret = XGE_HAL_FAIL;
	u32 exit_cnt = 0;
	u64 val64;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;

	val64 = XGE_HAL_I2C_CONTROL_DEV_ID(XGE_DEV_ID) |
	    XGE_HAL_I2C_CONTROL_ADDR(off) |
	    XGE_HAL_I2C_CONTROL_BYTE_CNT(cnt) |
	    XGE_HAL_I2C_CONTROL_SET_DATA(data) |
	    XGE_HAL_I2C_CONTROL_CNTL_START;
	__hal_serial_mem_write64(hldev, val64, &bar0->i2c_control);

	while (exit_cnt < 5) {
	    val64 = __hal_serial_mem_read64(hldev, &bar0->i2c_control);
	    if (XGE_HAL_I2C_CONTROL_CNTL_END(val64)) {
	        if (!(val64 & XGE_HAL_I2C_CONTROL_NACK))
	            ret = XGE_HAL_OK;
	        break;
	    }
	    exit_cnt++;
	}

	return ret;
}

/*
 * xge_hal_register_test - reads and writes into all clock domains.
 * @hldev : private member of the device structure.
 * xge_nic structure.
 * @data : variable that returns the result of each of the test conducted b
 * by the driver.
 *
 * Read and write into all clock domains. The NIC has 3 clock domains,
 * see that registers in all the three regions are accessible.
 * Return value:
 * 0 on success.
 */
xge_hal_status_e
xge_hal_register_test(xge_hal_device_h devh, u64 *data)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64 = 0;
	int fail = 0;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->pif_rd_swapper_fb);
	if (val64 != 0x123456789abcdefULL) {
	    fail = 1;
	    xge_debug_osdep(XGE_TRACE, "Read Test level 1 fails");
	}

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->rmac_pause_cfg);
	if (val64 != 0xc000ffff00000000ULL) {
	    fail = 1;
	    xge_debug_osdep(XGE_TRACE, "Read Test level 2 fails");
	}

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->rx_queue_cfg);
	if (val64 != 0x0808080808080808ULL) {
	    fail = 1;
	    xge_debug_osdep(XGE_TRACE, "Read Test level 3 fails");
	}

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->xgxs_efifo_cfg);
	if (val64 != 0x000000001923141EULL) {
	    fail = 1;
	    xge_debug_osdep(XGE_TRACE, "Read Test level 4 fails");
	}

	val64 = 0x5A5A5A5A5A5A5A5AULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->xmsi_data);
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->xmsi_data);
	if (val64 != 0x5A5A5A5A5A5A5A5AULL) {
	    fail = 1;
	    xge_debug_osdep(XGE_ERR, "Write Test level 1 fails");
	}

	val64 = 0xA5A5A5A5A5A5A5A5ULL;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->xmsi_data);
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->xmsi_data);
	if (val64 != 0xA5A5A5A5A5A5A5A5ULL) {
	    fail = 1;
	    xge_debug_osdep(XGE_ERR, "Write Test level 2 fails");
	}

	*data = fail;
	return XGE_HAL_OK;
}

/*
 * xge_hal_rldram_test - offline test for access to the RldRam chip on
 the NIC
 * @devh: HAL device handle.
 * @data: variable that returns the result of each of the test
 * conducted by the driver.
 *
 * This is one of the offline test that tests the read and write
 * access to the RldRam chip on the NIC.
 * Return value:
 * 0 on success.
 */
xge_hal_status_e
xge_hal_rldram_test(xge_hal_device_h devh, u64 *data)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;
	int cnt, iteration = 0, test_pass = 0;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->adapter_control);
	val64 &= ~XGE_HAL_ADAPTER_ECC_EN;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->adapter_control);

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->mc_rldram_test_ctrl);
	val64 |= XGE_HAL_MC_RLDRAM_TEST_MODE;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_ctrl);

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->mc_rldram_mrs);
	val64 |= XGE_HAL_MC_RLDRAM_QUEUE_SIZE_ENABLE;
	__hal_serial_mem_write64(hldev, val64, &bar0->i2c_control);

	val64 |= XGE_HAL_MC_RLDRAM_MRS_ENABLE;
	__hal_serial_mem_write64(hldev, val64, &bar0->i2c_control);

	while (iteration < 2) {
	    val64 = 0x55555555aaaa0000ULL;
	    if (iteration == 1) {
	        val64 ^= 0xFFFFFFFFFFFF0000ULL;
	    }
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_d0);

	    val64 = 0xaaaa5a5555550000ULL;
	    if (iteration == 1) {
	        val64 ^= 0xFFFFFFFFFFFF0000ULL;
	    }
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_d1);

	    val64 = 0x55aaaaaaaa5a0000ULL;
	    if (iteration == 1) {
	        val64 ^= 0xFFFFFFFFFFFF0000ULL;
	    }
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_d2);

	    val64 = (u64) (0x0000003fffff0000ULL);
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_add);


	    val64 = XGE_HAL_MC_RLDRAM_TEST_MODE;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_ctrl);

	    val64 |=
	        XGE_HAL_MC_RLDRAM_TEST_MODE | XGE_HAL_MC_RLDRAM_TEST_WRITE |
	        XGE_HAL_MC_RLDRAM_TEST_GO;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_ctrl);

	    for (cnt = 0; cnt < 5; cnt++) {
	        val64 = xge_os_pio_mem_read64(hldev->pdev,
	            hldev->regh0, &bar0->mc_rldram_test_ctrl);
	        if (val64 & XGE_HAL_MC_RLDRAM_TEST_DONE)
	            break;
	        xge_os_mdelay(200);
	    }

	    if (cnt == 5)
	        break;

	    val64 = XGE_HAL_MC_RLDRAM_TEST_MODE;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_ctrl);

	    val64 |= XGE_HAL_MC_RLDRAM_TEST_MODE |
	    XGE_HAL_MC_RLDRAM_TEST_GO;
	    xge_os_pio_mem_write64(hldev->pdev, hldev->regh0, val64,
	        &bar0->mc_rldram_test_ctrl);

	    for (cnt = 0; cnt < 5; cnt++) {
	        val64 = xge_os_pio_mem_read64(hldev->pdev,
	            hldev->regh0, &bar0->mc_rldram_test_ctrl);
	        if (val64 & XGE_HAL_MC_RLDRAM_TEST_DONE)
	            break;
	        xge_os_mdelay(500);
	    }

	    if (cnt == 5)
	        break;

	    val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->mc_rldram_test_ctrl);
	    if (val64 & XGE_HAL_MC_RLDRAM_TEST_PASS)
	        test_pass = 1;

	    iteration++;
	}

	if (!test_pass)
	    *data = 1;
	else
	    *data = 0;

	return XGE_HAL_OK;
}

/*
 * xge_hal_pma_loopback - Enable or disable PMA loopback
 * @devh: HAL device handle.
 * @enable:Boolean set to 1 to enable and 0 to disable.
 *
 * Enable or disable PMA loopback.
 * Return value:
 * 0 on success.
 */
xge_hal_status_e
xge_hal_pma_loopback( xge_hal_device_h devh, int enable )
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;
	u16 data;

	/*
	 * This code if for MAC loopbak
	 * Should be enabled through another parameter
	 */
#if 0
	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	&bar0->mac_cfg);
	if ( enable )
	{
	    val64 |= ( XGE_HAL_MAC_CFG_TMAC_LOOPBACK | XGE_HAL_MAC_CFG_RMAC_PROM_ENABLE );
	}
	__hal_pio_mem_write32_upper(hldev->pdev, hldev->regh0,
	        (u32)(val64 >> 32), (char*)&bar0->mac_cfg);
	xge_os_mdelay(1);
#endif

	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(0)  |
	    XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(1)   |
	    XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)   |
	    XGE_HAL_MDIO_CONTROL_MMD_CTRL(0)       |
	    XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_ADDRESS);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(0)  |
	    XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(1)   |
	    XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)   |
	    XGE_HAL_MDIO_CONTROL_MMD_CTRL(0)       |
	    XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_READ);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 = __hal_serial_mem_read64(hldev, &bar0->mdio_control);

	data = (u16)XGE_HAL_MDIO_CONTROL_MMD_DATA_GET(val64);

#define _HAL_LOOPBK_PMA         1

	if( enable )
	    data |= 1;
	else
	    data &= 0xfe;

	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(0)  |
	     XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(1)   |
	     XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)   |
	     XGE_HAL_MDIO_CONTROL_MMD_CTRL(0)       |
	     XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_ADDRESS);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(0)  |
	    XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(1)   |
	    XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)   |
	    XGE_HAL_MDIO_CONTROL_MMD_DATA(data)    |
	    XGE_HAL_MDIO_CONTROL_MMD_CTRL(0x0)     |
	    XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_WRITE);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(0)  |
	    XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(1)   |
	    XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)   |
	    XGE_HAL_MDIO_CONTROL_MMD_CTRL(0x0)     |
	    XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_READ);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	return XGE_HAL_OK;
}

u16
xge_hal_mdio_read( xge_hal_device_h devh, u32 mmd_type, u64 addr )
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64 = 0x0;
	u16 rval16 = 0x0;
	u8  i = 0;

	/* address transaction */
	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(addr)  |
	    XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(mmd_type)   |
	    XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)   |
	    XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_ADDRESS);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);
	do
	{
	    val64 = __hal_serial_mem_read64(hldev, &bar0->mdio_control);
	    if (i++ > 10)
	    {
	        break;
	    }
	}while((val64 & XGE_HAL_MDIO_CONTROL_MMD_CTRL(0xF)) != XGE_HAL_MDIO_CONTROL_MMD_CTRL(1));

	/* Data transaction */
	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(addr)  |
	    XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(mmd_type)   |
	    XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)   |
	    XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_READ);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	i = 0;

	do
	{
	    val64 = __hal_serial_mem_read64(hldev, &bar0->mdio_control);
	    if (i++ > 10)
	    {
	        break;
	    }
	}while((val64 & XGE_HAL_MDIO_CONTROL_MMD_CTRL(0xF)) != XGE_HAL_MDIO_CONTROL_MMD_CTRL(1));

	rval16 = (u16)XGE_HAL_MDIO_CONTROL_MMD_DATA_GET(val64);

	return rval16;
}

xge_hal_status_e
xge_hal_mdio_write( xge_hal_device_h devh, u32 mmd_type, u64 addr, u32 value )
{
	u64 val64 = 0x0;
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u8  i = 0;
	/* address transaction */

	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(addr)  |
	    XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(mmd_type)   |
	    XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)   |
	    XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_ADDRESS);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	do
	{
	    val64 = __hal_serial_mem_read64(hldev, &bar0->mdio_control);
	    if (i++ > 10)
	    {
	        break;
	    }
	} while((val64 & XGE_HAL_MDIO_CONTROL_MMD_CTRL(0xF)) !=
	    XGE_HAL_MDIO_CONTROL_MMD_CTRL(1));

	/* Data transaction */

	val64 = 0x0;

	val64 = XGE_HAL_MDIO_CONTROL_MMD_INDX_ADDR(addr)    |
	    XGE_HAL_MDIO_CONTROL_MMD_DEV_ADDR(mmd_type) |
	    XGE_HAL_MDIO_CONTROL_MMD_PRT_ADDR(0)        |
	    XGE_HAL_MDIO_CONTROL_MMD_DATA(value)        |
	    XGE_HAL_MDIO_CONTROL_MMD_OP(XGE_HAL_MDIO_OP_WRITE);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	val64 |= XGE_HAL_MDIO_CONTROL_MMD_CTRL(XGE_HAL_MDIO_CTRL_START);
	__hal_serial_mem_write64(hldev, val64, &bar0->mdio_control);

	i = 0;

	do
	{
	    val64 = __hal_serial_mem_read64(hldev, &bar0->mdio_control);
	    if (i++ > 10)
	    {
	        break;
	    }
	}while((val64 & XGE_HAL_MDIO_CONTROL_MMD_CTRL(0xF)) != XGE_HAL_MDIO_CONTROL_MMD_CTRL(1));

	return XGE_HAL_OK;
}

/*
 * xge_hal_eeprom_test - to verify that EEprom in the xena can be
 programmed.
 * @devh: HAL device handle.
 * @data:variable that returns the result of each of the test conducted by
 * the driver.
 *
 * Verify that EEPROM in the xena can be programmed using I2C_CONTROL
 * register.
 * Return value:
 * 0 on success.
 */
xge_hal_status_e
xge_hal_eeprom_test(xge_hal_device_h devh, u64 *data)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	int fail     = 0;
	u32 ret_data = 0;

	/* Test Write Error at offset 0 */
	if (!xge_hal_write_eeprom(hldev, 0, 0, 3))
	    fail = 1;

	/* Test Write at offset 4f0 */
	if (xge_hal_write_eeprom(hldev, 0x4F0, 0x01234567, 3))
	    fail = 1;
	if (xge_hal_read_eeprom(hldev, 0x4F0, &ret_data))
	    fail = 1;

	if (ret_data != 0x01234567)
	    fail = 1;

	/* Reset the EEPROM data go FFFF */
	(void) xge_hal_write_eeprom(hldev, 0x4F0, 0xFFFFFFFF, 3);

	/* Test Write Request Error at offset 0x7c */
	if (!xge_hal_write_eeprom(hldev, 0x07C, 0, 3))
	    fail = 1;

	/* Test Write Request at offset 0x7fc */
	if (xge_hal_write_eeprom(hldev, 0x7FC, 0x01234567, 3))
	    fail = 1;
	if (xge_hal_read_eeprom(hldev, 0x7FC, &ret_data))
	    fail = 1;

	if (ret_data != 0x01234567)
	    fail = 1;

	/* Reset the EEPROM data go FFFF */
	(void) xge_hal_write_eeprom(hldev, 0x7FC, 0xFFFFFFFF, 3);

	/* Test Write Error at offset 0x80 */
	if (!xge_hal_write_eeprom(hldev, 0x080, 0, 3))
	    fail = 1;

	/* Test Write Error at offset 0xfc */
	if (!xge_hal_write_eeprom(hldev, 0x0FC, 0, 3))
	    fail = 1;

	/* Test Write Error at offset 0x100 */
	if (!xge_hal_write_eeprom(hldev, 0x100, 0, 3))
	    fail = 1;

	/* Test Write Error at offset 4ec */
	if (!xge_hal_write_eeprom(hldev, 0x4EC, 0, 3))
	    fail = 1;

	*data = fail;
	return XGE_HAL_OK;
}

/*
 * xge_hal_bist_test - invokes the MemBist test of the card .
 * @devh: HAL device handle.
 * xge_nic structure.
 * @data:variable that returns the result of each of the test conducted by
 * the driver.
 *
 * This invokes the MemBist test of the card. We give around
 * 2 secs time for the Test to complete. If it's still not complete
 * within this peiod, we consider that the test failed.
 * Return value:
 * 0 on success and -1 on failure.
 */
xge_hal_status_e
xge_hal_bist_test(xge_hal_device_h devh, u64 *data)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	u8 bist = 0;
	int cnt = 0;
	xge_hal_status_e ret = XGE_HAL_FAIL;

	xge_os_pci_read8(hldev->pdev, hldev->cfgh, 0x0f, &bist);
	bist |= 0x40;
	xge_os_pci_write8(hldev->pdev, hldev->cfgh, 0x0f, bist);

	while (cnt < 20) {
	    xge_os_pci_read8(hldev->pdev, hldev->cfgh, 0x0f, &bist);
	    if (!(bist & 0x40)) {
	        *data = (bist & 0x0f);
	        ret = XGE_HAL_OK;
	        break;
	    }
	    xge_os_mdelay(100);
	    cnt++;
	}

	return ret;
}

/*
 * xge_hal_link_test - verifies the link state of the nic
 * @devh: HAL device handle.
 * @data: variable that returns the result of each of the test conducted by
 * the driver.
 *
 * Verify the link state of the NIC and updates the input
 * argument 'data' appropriately.
 * Return value:
 * 0 on success.
 */
xge_hal_status_e
xge_hal_link_test(xge_hal_device_h devh, u64 *data)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	        &bar0->adapter_status);
	if (val64 & XGE_HAL_ADAPTER_STATUS_RMAC_LOCAL_FAULT)
	    *data = 1;

	return XGE_HAL_OK;
}


/**
 * xge_hal_getpause_data -Pause frame frame generation and reception.
 * @devh: HAL device handle.
 * @tx : A field to return the pause generation capability of the NIC.
 * @rx : A field to return the pause reception capability of the NIC.
 *
 * Returns the Pause frame generation and reception capability of the NIC.
 * Return value:
 *  void
 */
void xge_hal_getpause_data(xge_hal_device_h devh, int *tx, int *rx)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	            &bar0->rmac_pause_cfg);
	if (val64 & XGE_HAL_RMAC_PAUSE_GEN_EN)
	    *tx = 1;
	if (val64 & XGE_HAL_RMAC_PAUSE_RCV_EN)
	    *rx = 1;
}

/**
 * xge_hal_setpause_data -  set/reset pause frame generation.
 * @devh: HAL device handle.
 * @tx: A field that indicates the pause generation capability to be
 * set on the NIC.
 * @rx: A field that indicates the pause reception capability to be
 * set on the NIC.
 *
 * It can be used to set or reset Pause frame generation or reception
 * support of the NIC.
 * Return value:
 * int, returns 0 on Success
 */

int xge_hal_setpause_data(xge_hal_device_h devh, int tx, int rx)
{
	xge_hal_device_t *hldev = (xge_hal_device_t*)devh;
	xge_hal_pci_bar0_t *bar0 = (xge_hal_pci_bar0_t *)hldev->bar0;
	u64 val64;

	val64 = xge_os_pio_mem_read64(hldev->pdev, hldev->regh0,
	                &bar0->rmac_pause_cfg);
	if (tx)
	    val64 |= XGE_HAL_RMAC_PAUSE_GEN_EN;
	else
	    val64 &= ~XGE_HAL_RMAC_PAUSE_GEN_EN;
	if (rx)
	    val64 |= XGE_HAL_RMAC_PAUSE_RCV_EN;
	else
	    val64 &= ~XGE_HAL_RMAC_PAUSE_RCV_EN;
	xge_os_pio_mem_write64(hldev->pdev, hldev->regh0,
	             val64, &bar0->rmac_pause_cfg);
	return 0;
}

/**
 * xge_hal_read_xfp_current_temp -
 * @hldev: HAL device handle.
 *
 * This routine only gets the temperature for XFP modules. Also, updating of the
 * NVRAM can sometimes fail and so the reading we might get may not be uptodate.
 */
u32 xge_hal_read_xfp_current_temp(xge_hal_device_h hldev)
{
	u16 val_1, val_2, i = 0;
	u32 actual;

	/* First update the NVRAM table of XFP. */

	(void) xge_hal_mdio_write(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, 0x8000, 0x3);


	/* Now wait for the transfer to complete */
	do
	{
	    xge_os_mdelay( 50 ); // wait 50 milliseonds

	    val_1 =  xge_hal_mdio_read(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, 0x8000);

	    if ( i++ > 10 )
	    {
	        // waited 500 ms which should be plenty of time.
	        break;
	    }
	}while (( val_1 & 0x000C ) != 0x0004);

	/* Now NVRAM table of XFP should be updated, so read the temp */
	val_1 =  (u8) xge_hal_mdio_read(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, 0x8067);
	val_2 =  (u8) xge_hal_mdio_read(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, 0x8068);

	actual = ((val_1 << 8) | val_2);

	if (actual >= 32768)
	    actual = actual- 65536;
	actual =  actual/256;

	return actual;
}

/**
 * __hal_chk_xpak_counter -  check the Xpak error count and log the msg.
 * @hldev: pointer to xge_hal_device_t structure
 * @type:  xpak stats error type
 * @value: xpak stats value
 *
 * It is used to log the error message based on the xpak stats value
 * Return value:
 * None
 */

void __hal_chk_xpak_counter(xge_hal_device_t *hldev, int type, u32 value)
{
	/*
	 * If the value is high for three consecutive cylce,
	 * log a error message
	 */
	if(value == 3)
	{
	    switch(type)
	    {
	    case 1:
	        hldev->stats.sw_dev_err_stats.xpak_counter.
	            excess_temp = 0;

	        /*
	         * Notify the ULD on Excess Xpak temperature alarm msg
	         */
	        if (g_xge_hal_driver->uld_callbacks.xpak_alarm_log) {
	            g_xge_hal_driver->uld_callbacks.xpak_alarm_log(
	                hldev->upper_layer_info,
	                XGE_HAL_XPAK_ALARM_EXCESS_TEMP);
	        }
	        break;
	    case 2:
	        hldev->stats.sw_dev_err_stats.xpak_counter.
	            excess_bias_current = 0;

	        /*
	         * Notify the ULD on Excess  xpak bias current alarm msg
	         */
	        if (g_xge_hal_driver->uld_callbacks.xpak_alarm_log) {
	            g_xge_hal_driver->uld_callbacks.xpak_alarm_log(
	                hldev->upper_layer_info,
	                XGE_HAL_XPAK_ALARM_EXCESS_BIAS_CURRENT);
	        }
	        break;
	    case 3:
	        hldev->stats.sw_dev_err_stats.xpak_counter.
	            excess_laser_output = 0;

	        /*
	         * Notify the ULD on Excess Xpak Laser o/p power
	         * alarm msg
	         */
	        if (g_xge_hal_driver->uld_callbacks.xpak_alarm_log) {
	            g_xge_hal_driver->uld_callbacks.xpak_alarm_log(
	                hldev->upper_layer_info,
	                XGE_HAL_XPAK_ALARM_EXCESS_LASER_OUTPUT);
	        }
	        break;
	    default:
	        xge_debug_osdep(XGE_TRACE, "Incorrect XPAK Alarm "
	        "type ");
	    }
	}

}

/**
 * __hal_updt_stats_xpak -  update the Xpak error count.
 * @hldev: pointer to xge_hal_device_t structure
 *
 * It is used to update the xpak stats value
 * Return value:
 * None
 */
void __hal_updt_stats_xpak(xge_hal_device_t *hldev)
{
	u16 val_1;
	u64 addr;

	/* Check the communication with the MDIO slave */
	addr = 0x0000;
	val_1 = 0x0;
	val_1 = xge_hal_mdio_read(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, addr);
	if((val_1 == 0xFFFF) || (val_1 == 0x0000))
	    {
	            xge_debug_osdep(XGE_TRACE, "ERR: MDIO slave access failed - "
	                      "Returned %x", val_1);
	            return;
	    }

	/* Check for the expected value of 2040 at PMA address 0x0000 */
	if(val_1 != 0x2040)
	    {
	            xge_debug_osdep(XGE_TRACE, "Incorrect value at PMA address 0x0000 - ");
	            xge_debug_osdep(XGE_TRACE, "Returned: %llx- Expected: 0x2040",
	            (unsigned long long)(unsigned long)val_1);
	            return;
	    }

	/* Loading the DOM register to MDIO register */
	    addr = 0xA100;
	    (void) xge_hal_mdio_write(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, addr, 0x0);
	    val_1 = xge_hal_mdio_read(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, addr);

	/*
	 * Reading the Alarm flags
	 */
	    addr = 0xA070;
	    val_1 = 0x0;
	    val_1 = xge_hal_mdio_read(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, addr);
	if(CHECKBIT(val_1, 0x7))
	{
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        alarm_transceiver_temp_high++;
	    hldev->stats.sw_dev_err_stats.xpak_counter.excess_temp++;
	    __hal_chk_xpak_counter(hldev, 0x1,
	        hldev->stats.sw_dev_err_stats.xpak_counter.excess_temp);
	} else {
	    hldev->stats.sw_dev_err_stats.xpak_counter.excess_temp = 0;
	}
	if(CHECKBIT(val_1, 0x6))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        alarm_transceiver_temp_low++;

	if(CHECKBIT(val_1, 0x3))
	{
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        alarm_laser_bias_current_high++;
	    hldev->stats.sw_dev_err_stats.xpak_counter.
	        excess_bias_current++;
	    __hal_chk_xpak_counter(hldev, 0x2,
	        hldev->stats.sw_dev_err_stats.xpak_counter.
	        excess_bias_current);
	} else {
	    hldev->stats.sw_dev_err_stats.xpak_counter.
	        excess_bias_current = 0;
	}
	if(CHECKBIT(val_1, 0x2))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        alarm_laser_bias_current_low++;

	if(CHECKBIT(val_1, 0x1))
	{
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        alarm_laser_output_power_high++;
	    hldev->stats.sw_dev_err_stats.xpak_counter.
	        excess_laser_output++;
	    __hal_chk_xpak_counter(hldev, 0x3,
	        hldev->stats.sw_dev_err_stats.xpak_counter.
	            excess_laser_output);
	} else {
	    hldev->stats.sw_dev_err_stats.xpak_counter.
	            excess_laser_output = 0;
	}
	if(CHECKBIT(val_1, 0x0))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	            alarm_laser_output_power_low++;

	/*
	 * Reading the warning flags
	 */
	    addr = 0xA074;
	    val_1 = 0x0;
	    val_1 = xge_hal_mdio_read(hldev, XGE_HAL_MDIO_MMD_PMA_DEV_ADDR, addr);
	if(CHECKBIT(val_1, 0x7))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        warn_transceiver_temp_high++;
	if(CHECKBIT(val_1, 0x6))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        warn_transceiver_temp_low++;
	if(CHECKBIT(val_1, 0x3))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        warn_laser_bias_current_high++;
	if(CHECKBIT(val_1, 0x2))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        warn_laser_bias_current_low++;
	if(CHECKBIT(val_1, 0x1))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        warn_laser_output_power_high++;
	if(CHECKBIT(val_1, 0x0))
	    hldev->stats.sw_dev_err_stats.stats_xpak.
	        warn_laser_output_power_low++;
}
