/*-
 * Copyright(c) 2002-2011 Exar Corp.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification are permitted provided the following conditions are met:
 *
 *    1. Redistributions of source code must retain the above copyright notice,
 *       this list of conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *
 *    3. Neither the name of the Exar Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived from
 *       this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*$FreeBSD$*/

#include <dev/vxge/vxgehal/vxgehal.h>

/*
 * vxge_hal_mgmt_about - Retrieve about info.
 * @devh: HAL device handle.
 * @about_info: Filled in by HAL. See vxge_hal_mgmt_about_info_t {}.
 * @size: Pointer to buffer containing the Size of the @buffer_info.
 * HAL will return an error if the size is smaller than
 * sizeof(vxge_hal_mgmt_about_info_t) and returns required size in this field
 *
 * Retrieve information such as PCI device and vendor IDs, board
 * revision number, HAL version number, etc.
 *
 * Returns: VXGE_HAL_OK - success;
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 * VXGE_HAL_ERR_OUT_OF_SPACE - If the buffer is not sufficient
 * VXGE_HAL_FAIL - Failed to retrieve the information.
 *
 * See also: vxge_hal_mgmt_about_info_t {}.
 */
vxge_hal_status_e
vxge_hal_mgmt_about(vxge_hal_device_h devh,
    vxge_hal_mgmt_about_info_t *about_info,
    u32 *size)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (about_info != NULL) && (size != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "hldev = 0x"VXGE_OS_STXFMT", about_info = 0x"VXGE_OS_STXFMT", "
	    "size = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev, (ptr_t) about_info, (ptr_t) size);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (*size < sizeof(vxge_hal_mgmt_about_info_t)) {
		*size = sizeof(vxge_hal_mgmt_about_info_t);
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_SPACE);
		return (VXGE_HAL_ERR_OUT_OF_SPACE);
	}

	about_info->vendor = hldev->pci_config_space_bios.vendor_id;
	about_info->device = hldev->pci_config_space_bios.device_id;
	about_info->subsys_vendor =
	    hldev->pci_config_space_bios.subsystem_vendor_id;
	about_info->subsys_device = hldev->pci_config_space_bios.subsystem_id;
	about_info->board_rev = hldev->pci_config_space_bios.revision;

	vxge_os_strlcpy(about_info->vendor_name, VXGE_DRIVER_VENDOR,
	    sizeof(about_info->vendor_name));
	vxge_os_strlcpy(about_info->chip_name, VXGE_CHIP_FAMILY,
	    sizeof(about_info->chip_name));
	vxge_os_strlcpy(about_info->media, VXGE_SUPPORTED_MEDIA_0,
	    sizeof(about_info->media));

	(void) vxge_os_snprintf(about_info->hal_major,
	    sizeof(about_info->hal_major), "%d", VXGE_HAL_VERSION_MAJOR);
	(void) vxge_os_snprintf(about_info->hal_minor,
	    sizeof(about_info->hal_minor), "%d", VXGE_HAL_VERSION_MINOR);
	(void) vxge_os_snprintf(about_info->hal_fix,
	    sizeof(about_info->hal_fix), "%d", VXGE_HAL_VERSION_FIX);
	(void) vxge_os_snprintf(about_info->hal_build,
	    sizeof(about_info->hal_build), "%d", VXGE_HAL_VERSION_BUILD);

	(void) vxge_os_snprintf(about_info->ll_major,
	    sizeof(about_info->ll_major), "%d", XGELL_VERSION_MAJOR);
	(void) vxge_os_snprintf(about_info->ll_minor,
	    sizeof(about_info->ll_minor), "%d", XGELL_VERSION_MINOR);
	(void) vxge_os_snprintf(about_info->ll_fix,
	    sizeof(about_info->ll_fix), "%d", XGELL_VERSION_FIX);
	(void) vxge_os_snprintf(about_info->ll_build,
	    sizeof(about_info->ll_build), "%d", XGELL_VERSION_BUILD);

	*size = sizeof(vxge_hal_mgmt_about_info_t);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_pci_config - Retrieve PCI configuration.
 * @devh: HAL device handle.
 * @buffer: Buffer to return pci config.
 * @size: Pointer to buffer containing the Size of the @buffer.
 * HAL will return an error if the size is smaller than
 * sizeof(vxge_hal_pci_config_t) and returns required size in this field
 *
 * Get PCI configuration. Permits to retrieve at run-time configuration
 * values that were used to configure the device at load-time.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 * VXGE_HAL_ERR_OUT_OF_SPACE - If the buffer is not sufficient
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_pci_config(vxge_hal_device_h devh, u8 *buffer, u32 *size)
{
	int i;
	vxge_hal_pci_config_t *pci_config = (vxge_hal_pci_config_t *) buffer;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (buffer != NULL) && (size != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT", "
	    "buffer = 0x"VXGE_OS_STXFMT", "
	    "size = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev, (ptr_t) buffer, (ptr_t) size);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (*size < sizeof(vxge_hal_pci_config_t)) {
		*size = sizeof(vxge_hal_pci_config_t);
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_SPACE);
		return (VXGE_HAL_ERR_OUT_OF_SPACE);
	}

	/* refresh PCI config space */
	for (i = 0; i < VXGE_HAL_PCI_CONFIG_SPACE_SIZE / 4; i++) {
		(void) __hal_vpath_pci_read(hldev,
		    hldev->first_vp_id,
		    i * 4,
		    4,
		    (u32 *) ((void *)&hldev->pci_config_space) + i);
	}

	vxge_os_memcpy(pci_config, &hldev->pci_config_space,
	    sizeof(vxge_hal_pci_config_t));

	*size = sizeof(vxge_hal_pci_config_t);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_msi_capabilities_get - Returns the msi capabilities
 * @devh: HAL device handle.
 * @msi_cap: MSI Capabilities
 *
 * Return the msi capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_msi_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_mgmt_msi_cap_t *msi_cap)
{
	u16 msi_control_reg;
	u32 addr32;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (msi_cap != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "hldev = 0x"VXGE_OS_STXFMT", msi_cap = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev, (ptr_t) msi_cap);

	if (hldev->pci_caps.msi_cap_offset == 0) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	vxge_os_memzero(msi_cap, sizeof(vxge_hal_mgmt_msi_cap_t));

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.msi_cap_offset +
	    vxge_offsetof(vxge_hal_msi_capability_le_t, msi_control),
	    2,
	    &msi_control_reg);

	if (msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_ENABLE)
		msi_cap->enable = 1;

	if (msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_PVMASK)
		msi_cap->is_pvm_capable = 1;

	if (msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_64BIT)
		msi_cap->is_64bit_addr_capable = 1;

	msi_cap->vectors_allocated =
	    (msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_QSIZE) >> 4;

	msi_cap->max_vectors_capable =
	    (msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_QMASK) >> 1;

	if (msi_cap->is_64bit_addr_capable) {
		if (msi_cap->is_pvm_capable) {
			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_addr_hi),
			    4, &addr32);

			msi_cap->address = ((u64) addr32) << 32;

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_addr_lo),
			    4, &addr32);

			msi_cap->address |= (u64) addr32;

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_data),
			    2, &msi_cap->data);

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_mask),
			    4, &msi_cap->mask_bits);

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_pending),
			    4, &msi_cap->pending_bits);
		} else {
			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_no_pvm.msi_addr_hi),
			    4, &addr32);

			msi_cap->address = ((u64) addr32) << 32;

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_no_pvm.msi_addr_lo),
			    4, &addr32);

			msi_cap->address |= (u64) addr32;

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_no_pvm.msi_data),
			    2, &msi_cap->data);

		}
	} else {
		if (msi_cap->is_pvm_capable) {
			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_pvm.msi_addr),
			    4, &addr32);

			msi_cap->address = (u64) addr32;

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_pvm.msi_data),
			    2, &msi_cap->data);

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_pvm.msi_mask),
			    4, &msi_cap->mask_bits);

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_pvm.msi_pending),
			    4, &msi_cap->pending_bits);

		} else {
			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_no_pvm.msi_addr),
			    4, &addr32);

			msi_cap->address = (u64) addr32;

			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_no_pvm.msi_data),
			    2, &msi_cap->data);
		}
	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_msi_capabilities_set - Sets the msi capabilities
 * @devh: HAL device handle.
 * @msi_cap: MSI Capabilities
 *
 * Sets the msi capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_msi_capabilities_set(vxge_hal_device_h devh,
    vxge_hal_mgmt_msi_cap_t *msi_cap)
{
	u16 msi_control_reg;
	u32 addr32;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (msi_cap != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT","
	    "msi_cap = 0x"VXGE_OS_STXFMT, (ptr_t) hldev, (ptr_t) msi_cap);

	if (hldev->pci_caps.msi_cap_offset == 0) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.msi_cap_offset +
	    vxge_offsetof(vxge_hal_msi_capability_le_t, msi_control),
	    2, &msi_control_reg);

	if (msi_cap->enable)
		msi_control_reg |= VXGE_HAL_PCI_MSI_FLAGS_ENABLE;
	else
		msi_control_reg &= ~VXGE_HAL_PCI_MSI_FLAGS_ENABLE;

	if (msi_cap->vectors_allocated >
	    (u32) ((msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_QMASK) >> 1)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	msi_control_reg &= ~VXGE_HAL_PCI_MSI_FLAGS_QSIZE;

	msi_control_reg |= (msi_cap->vectors_allocated & 0x7) << 4;

	if (msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_64BIT) {
		if (msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_PVMASK) {

			addr32 = (u32) (msi_cap->address >> 32);

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_addr_hi), addr32);

			addr32 = (u32) msi_cap->address;

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_addr_lo), addr32);

			vxge_os_pci_write16(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_data), msi_cap->data);

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_mask), msi_cap->mask_bits);

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_pvm.msi_pending), msi_cap->pending_bits);
		} else {
			addr32 = (u32) (msi_cap->address >> 32);

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_no_pvm.msi_addr_hi), addr32);

			addr32 = (u32) msi_cap->address;

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_no_pvm.msi_addr_lo), addr32);

			vxge_os_pci_write16(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma64_no_pvm.msi_data), msi_cap->data);

		}
	} else {
		if (msi_control_reg & VXGE_HAL_PCI_MSI_FLAGS_PVMASK) {

			addr32 = (u32) msi_cap->address;

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_pvm.msi_addr), addr32);

			vxge_os_pci_write16(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_pvm.msi_data), msi_cap->data);

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_pvm.msi_mask), msi_cap->mask_bits);

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_pvm.msi_pending), msi_cap->pending_bits);

		} else {
			addr32 = (u32) msi_cap->address;

			vxge_os_pci_write32(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_no_pvm.msi_addr), addr32);

			vxge_os_pci_write16(hldev->header.pdev,
			    hldev->header.cfgh,
			    hldev->pci_caps.msi_cap_offset +
			    vxge_offsetof(vxge_hal_msi_capability_le_t,
			    au.ma32_no_pvm.msi_data), msi_cap->data);
		}
	}

	vxge_os_pci_write16(hldev->header.pdev, hldev->header.cfgh,
	    hldev->pci_caps.msi_cap_offset +
	    vxge_offsetof(vxge_hal_msi_capability_le_t, msi_control),
	    msi_control_reg);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_msix_capabilities_get - Returns the msix capabilities
 * @devh: HAL device handle.
 * @msix_cap: MSIX Capabilities
 *
 * Return the msix capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_msix_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_mgmt_msix_cap_t *msix_cap)
{
	u16 msix_control_reg;
	u32 msix_offset;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (msix_cap != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "hldev = 0x"VXGE_OS_STXFMT", msix_cap = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev, (ptr_t) msix_cap);

	if (hldev->pci_caps.msix_cap_offset == 0) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	vxge_os_memzero(msix_cap, sizeof(vxge_hal_mgmt_msix_cap_t));

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.msix_cap_offset +
	    vxge_offsetof(vxge_hal_msix_capability_le_t, msix_control),
	    2, &msix_control_reg);

	if (msix_control_reg & VXGE_HAL_PCI_MSIX_FLAGS_ENABLE)
		msix_cap->enable = 1;

	if (msix_control_reg & VXGE_HAL_PCI_MSIX_FLAGS_MASK)
		msix_cap->mask_all_vect = 1;

	msix_cap->table_size =
	    (msix_control_reg & VXGE_HAL_PCI_MSIX_FLAGS_TSIZE) + 1;

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.msix_cap_offset +
	    vxge_offsetof(vxge_hal_msix_capability_le_t, table_offset),
	    4, &msix_offset);

	msix_cap->table_offset =
	    (msix_offset & VXGE_HAL_PCI_MSIX_TABLE_OFFSET) >> 3;

	msix_cap->table_bir = msix_offset & VXGE_HAL_PCI_MSIX_TABLE_BIR;

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.msix_cap_offset +
	    vxge_offsetof(vxge_hal_msix_capability_le_t, pba_offset),
	    4, &msix_offset);

	msix_cap->pba_offset =
	    (msix_offset & VXGE_HAL_PCI_MSIX_PBA_OFFSET) >> 3;

	msix_cap->pba_bir = msix_offset & VXGE_HAL_PCI_MSIX_PBA_BIR;

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_pm_capabilities_get - Returns the pm capabilities
 * @devh: HAL device handle.
 * @pm_cap: pm Capabilities
 *
 * Return the pm capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_pm_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_mgmt_pm_cap_t *pm_cap)
{
	u16 pm_cap_reg;
	u16 pm_control_reg;
	u8 pm_ppb_ext;
	u8 pm_data_reg;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (pm_cap != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT", "
	    "pm_cap = 0x"VXGE_OS_STXFMT, (ptr_t) hldev, (ptr_t) pm_cap);

	if (hldev->pci_caps.pm_cap_offset == 0) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	vxge_os_memzero(pm_cap, sizeof(vxge_hal_mgmt_pm_cap_t));

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.pm_cap_offset +
	    vxge_offsetof(vxge_hal_pm_capability_le_t, capabilities_reg),
	    2, &pm_cap_reg);

	pm_cap->pm_cap_ver =
	    (u32) (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_VER_MASK);

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_PME_CLOCK)
		pm_cap->pm_cap_pme_clock = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_AUX_POWER)
		pm_cap->pm_cap_aux_power = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_DSI)
		pm_cap->pm_cap_dsi = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_AUX_CURRENT)
		pm_cap->pm_cap_aux_current = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_D1)
		pm_cap->pm_cap_cap_d0 = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_D2)
		pm_cap->pm_cap_cap_d1 = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_PME_D0)
		pm_cap->pm_cap_pme_d0 = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_PME_D1)
		pm_cap->pm_cap_pme_d1 = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_PME_D2)
		pm_cap->pm_cap_pme_d2 = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_PME_D3_HOT)
		pm_cap->pm_cap_pme_d3_hot = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CAP_PME_D3_COLD)
		pm_cap->pm_cap_pme_d3_cold = 1;

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.pm_cap_offset +
	    vxge_offsetof(vxge_hal_pm_capability_le_t, pm_ctrl),
	    2, &pm_control_reg);

	pm_cap->pm_ctrl_state =
	    pm_control_reg & VXGE_HAL_PCI_PM_CTRL_STATE_MASK;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CTRL_NO_SOFT_RESET)
		pm_cap->pm_ctrl_no_soft_reset = 1;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CTRL_PME_ENABLE)
		pm_cap->pm_ctrl_pme_enable = 1;

	pm_cap->pm_ctrl_pme_data_sel =
	    (u32) (pm_control_reg & VXGE_HAL_PCI_PM_CTRL_DATA_SEL_MASK) >> 10;

	pm_cap->pm_ctrl_pme_data_scale =
	    (u32) (pm_control_reg & VXGE_HAL_PCI_PM_CTRL_DATA_SCALE_MASK) >> 13;

	if (pm_cap_reg & VXGE_HAL_PCI_PM_CTRL_PME_STATUS)
		pm_cap->pm_ctrl_pme_status = 1;

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.pm_cap_offset +
	    vxge_offsetof(vxge_hal_pm_capability_le_t, pm_ctrl),
	    1, &pm_ppb_ext);

	if (pm_ppb_ext & VXGE_HAL_PCI_PM_PPB_B2_B3)
		pm_cap->pm_ppb_ext_b2_b3 = 1;

	if (pm_ppb_ext & VXGE_HAL_PCI_PM_BPCC_ENABLE)
		pm_cap->pm_ppb_ext_ecc_en = 1;

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.pm_cap_offset +
	    vxge_offsetof(vxge_hal_pm_capability_le_t, pm_data_reg),
	    1, &pm_data_reg);

	pm_cap->pm_data_reg = (u32) pm_data_reg;

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_sid_capabilities_get - Returns the sid capabilities
 * @devh: HAL device handle.
 * @sid_cap: Slot Id Capabilities
 *
 * Return the Slot Id capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_sid_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_mgmt_sid_cap_t *sid_cap)
{
	u8 chasis_num_reg;
	u8 slot_num_reg;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (sid_cap != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT
	    ", sid_cap = 0x"VXGE_OS_STXFMT, (ptr_t) hldev, (ptr_t) sid_cap);

	if (hldev->pci_caps.sid_cap_offset == 0) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	vxge_os_memzero(sid_cap, sizeof(vxge_hal_mgmt_sid_cap_t));

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.sid_cap_offset +
	    vxge_offsetof(vxge_hal_sid_capability_le_t, sid_esr),
	    1, &slot_num_reg);

	sid_cap->sid_number_of_slots =
	    (u32) (slot_num_reg & VXGE_HAL_PCI_SID_ESR_NSLOTS);

	if (slot_num_reg & VXGE_HAL_PCI_SID_ESR_FIC)
		sid_cap->sid_number_of_slots = 1;

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_caps.sid_cap_offset +
	    vxge_offsetof(vxge_hal_sid_capability_le_t, sid_chasis_nr),
	    1, &chasis_num_reg);

	sid_cap->sid_chasis_number = (u32) chasis_num_reg;

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_pci_err_capabilities_get - Returns the pci error capabilities
 * @devh: HAL device handle.
 * @err_cap: PCI-E Extended Error Capabilities
 *
 * Return the PCI-E Extended Error capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_pci_err_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_pci_err_cap_t *err_cap)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (err_cap != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT
	    ",sid_cap = 0x"VXGE_OS_STXFMT, (ptr_t) hldev, (ptr_t) err_cap);

	if (hldev->pci_e_ext_caps.err_cap_offset == 0) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	vxge_os_memzero(err_cap, sizeof(vxge_hal_pci_err_cap_t));

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_header),
	    4,
	    &err_cap->pci_err_header);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_uncor_status),
	    4,
	    &err_cap->pci_err_uncor_status);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_uncor_mask),
	    4,
	    &err_cap->pci_err_uncor_mask);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_uncor_server),
	    4,
	    &err_cap->pci_err_uncor_server);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_cor_status),
	    4,
	    &err_cap->pci_err_cor_status);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_cap),
	    4,
	    &err_cap->pci_err_cap);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, err_header_log),
	    4,
	    &err_cap->err_header_log);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_root_command),
	    4,
	    &err_cap->pci_err_root_command);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_root_status),
	    4,
	    &err_cap->pci_err_root_status);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_root_cor_src),
	    4,
	    &err_cap->pci_err_root_cor_src);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    hldev->pci_e_ext_caps.err_cap_offset +
	    vxge_offsetof(vxge_hal_err_capability_t, pci_err_root_src),
	    4,
	    &err_cap->pci_err_root_src);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_driver_config - Retrieve driver configuration.
 * @drv_config: Device configuration, see vxge_hal_driver_config_t {}.
 * @size: Pointer to buffer containing the Size of the @drv_config.
 * HAL will return an error if the size is smaller than
 * sizeof(vxge_hal_driver_config_t) and returns required size in this field
 *
 * Get driver configuration. Permits to retrieve at run-time configuration
 * values that were used to configure the device at load-time.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED - HAL is not initialized.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version is not maching.
 * VXGE_HAL_ERR_OUT_OF_SPACE - If the buffer is not sufficient
 *
 * See also: vxge_hal_driver_config_t {}, vxge_hal_mgmt_device_config().
 */
vxge_hal_status_e
vxge_hal_mgmt_driver_config(vxge_hal_driver_config_t *drv_config, u32 *size)
{

	vxge_assert((drv_config != NULL) && (size != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "drv_config = 0x"VXGE_OS_STXFMT", size = 0x"VXGE_OS_STXFMT,
	    (ptr_t) drv_config, (ptr_t) size);

	if (g_vxge_hal_driver == NULL) {
		vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED);
		return (VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED);
	}

	if (*size < sizeof(vxge_hal_driver_config_t)) {
		*size = sizeof(vxge_hal_driver_config_t);
		vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_SPACE);
		return (VXGE_HAL_ERR_OUT_OF_SPACE);
	}

	vxge_os_memcpy(drv_config, &g_vxge_hal_driver->config,
	    sizeof(vxge_hal_driver_config_t));

	*size = sizeof(vxge_hal_driver_config_t);

	vxge_hal_trace_log_driver("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}


/*
 * vxge_hal_mgmt_device_config - Retrieve device configuration.
 * @devh: HAL device handle.
 * @dev_config: Device configuration, see vxge_hal_device_config_t {}.
 * @size: Pointer to buffer containing the Size of the @dev_config.
 * HAL will return an error if the size is smaller than
 * sizeof(vxge_hal_device_config_t) and returns required size in this field
 *
 * Get device configuration. Permits to retrieve at run-time configuration
 * values that were used to initialize and configure the device.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 * VXGE_HAL_ERR_OUT_OF_SPACE - If the buffer is not sufficient
 *
 * See also: vxge_hal_device_config_t {}, vxge_hal_mgmt_driver_config().
 */
vxge_hal_status_e
vxge_hal_mgmt_device_config(vxge_hal_device_h devh,
    vxge_hal_device_config_t *dev_config, u32 *size)
{
	vxge_hal_device_t *hldev = (vxge_hal_device_t *) devh;

	vxge_assert((devh != NULL) && (dev_config != NULL) && (size != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "devh = 0x"VXGE_OS_STXFMT", dev_config = 0x"VXGE_OS_STXFMT", "
	    "size = 0x"VXGE_OS_STXFMT, (ptr_t) devh, (ptr_t) dev_config,
	    (ptr_t) size);

	if (hldev->magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (*size < sizeof(vxge_hal_device_config_t)) {
		*size = sizeof(vxge_hal_device_config_t);
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_SPACE);
		return (VXGE_HAL_ERR_OUT_OF_SPACE);
	}

	vxge_os_memcpy(dev_config, &hldev->config,
	    sizeof(vxge_hal_device_config_t));

	*size = sizeof(vxge_hal_device_config_t);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_pcireg_read - Read PCI configuration at a specified
 * offset.
 * @devh: HAL device handle.
 * @offset: Offset in the 256 byte PCI configuration space.
 * @value_bits: 8, 16, or 32 (bits) to read.
 * @value: Value returned by HAL.
 *
 * Read PCI configuration, given device and offset in the PCI space.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 * VXGE_HAL_ERR_INVALID_VALUE_BIT_SIZE - Invalid bits size. Valid
 * values(8/16/32).
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_pcireg_read(vxge_hal_device_h devh, unsigned int offset,
    int value_bits, u32 *value)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (value != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "devh = 0x"VXGE_OS_STXFMT", offset = %d, value_bits = %d, "
	    "value = 0x"VXGE_OS_STXFMT, (ptr_t) devh, offset,
	    value_bits, (ptr_t) value);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (offset > sizeof(vxge_hal_pci_config_t) - value_bits / 8) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_OFFSET);
	}

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    offset,
	    value_bits / 8,
	    value);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_reg_read - Read X3100 register.
 * @devh: HAL device handle.
 * @type: Register types as defined in enum vxge_hal_mgmt_reg_type_e {}
 * @Index: For pcicfgmgmt, srpcim, vpmgmt, vpath this gives the Index
 *		ignored for others
 * @offset: Register offset in the register space qualified by the type and
 *		index.
 * @value: Register value. Returned by HAL.
 * Read X3100 register.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_TYPE - Type is not valid.
 * VXGE_HAL_ERR_INVALID_INDEX - Index is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the space is not valid.
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_reg_read(vxge_hal_device_h devh,
    vxge_hal_mgmt_reg_type_e type,
    u32 vp_id,
    u32 offset,
    u64 *value)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (value != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "devh = 0x"VXGE_OS_STXFMT", type = %d, "
	    "vp_id = %d, offset = %d, value = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, type, vp_id, offset, (ptr_t) value);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	switch (type) {
	case vxge_hal_mgmt_reg_type_legacy:
		if (offset > sizeof(vxge_hal_legacy_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->legacy_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_toc:
		if (offset > sizeof(vxge_hal_toc_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->toc_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_common:
		if (offset > sizeof(vxge_hal_common_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->common_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_memrepair:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (offset > sizeof(vxge_hal_memrepair_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->memrepair_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_pcicfgmgmt:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (vp_id > VXGE_HAL_TITAN_PCICFGMGMT_REG_SPACES - 1) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(vxge_hal_pcicfgmgmt_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->pcicfgmgmt_reg[vp_id]) + offset));
		break;
	case vxge_hal_mgmt_reg_type_mrpcim:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (offset > sizeof(vxge_hal_mrpcim_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->mrpcim_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_srpcim:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (vp_id > VXGE_HAL_TITAN_SRPCIM_REG_SPACES - 1) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(vxge_hal_srpcim_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->srpcim_reg[vp_id]) + offset));
		break;
	case vxge_hal_mgmt_reg_type_vpmgmt:
		if ((vp_id > VXGE_HAL_TITAN_VPMGMT_REG_SPACES - 1) ||
		    (!(hldev->vpath_assignments & mBIT(vp_id)))) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(vxge_hal_vpmgmt_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->vpmgmt_reg[vp_id]) + offset));
		break;
	case vxge_hal_mgmt_reg_type_vpath:
		if ((vp_id > VXGE_HAL_TITAN_VPATH_REG_SPACES - 1) ||
		    (!(hldev->vpath_assignments & mBIT(vp_id)))) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (vp_id > VXGE_HAL_TITAN_VPATH_REG_SPACES - 1) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(vxge_hal_vpath_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		*value = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    (void *)(((ptr_t) hldev->vpath_reg[vp_id]) + offset));
		break;
	default:
		status = VXGE_HAL_ERR_INVALID_TYPE;
		break;
	}

	vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mgmt_reg_Write - Write X3100 register.
 * @devh: HAL device handle.
 * @type: Register types as defined in enum vxge_hal_mgmt_reg_type_e {}
 * @index: For pcicfgmgmt, srpcim, vpmgmt, vpath this gives the Index
 *		ignored for others
 * @offset: Register offset in the register space qualified by the type and
 *		index.
 * @value: Register value to be written.
 * Write X3100 register.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_TYPE - Type is not valid.
 * VXGE_HAL_ERR_INVALID_INDEX - Index is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the space is not valid.
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_reg_write(vxge_hal_device_h devh,
    vxge_hal_mgmt_reg_type_e type,
    u32 vp_id,
    u32 offset,
    u64 value)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
		"devh = 0x"VXGE_OS_STXFMT", type = %d, "
	    "index = %d, offset = %d, value = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, type, vp_id, offset, (ptr_t) value);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);

		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	switch (type) {
	case vxge_hal_mgmt_reg_type_legacy:
		if (offset > sizeof(vxge_hal_legacy_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->legacy_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_toc:
		if (offset > sizeof(vxge_hal_toc_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->toc_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_common:
		if (offset > sizeof(vxge_hal_common_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->common_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_memrepair:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (offset > sizeof(vxge_hal_memrepair_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->memrepair_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_pcicfgmgmt:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (vp_id > VXGE_HAL_TITAN_PCICFGMGMT_REG_SPACES - 1) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(vxge_hal_pcicfgmgmt_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->pcicfgmgmt_reg[vp_id]) + offset));
		break;
	case vxge_hal_mgmt_reg_type_mrpcim:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (offset > sizeof(vxge_hal_mrpcim_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->mrpcim_reg) + offset));
		break;
	case vxge_hal_mgmt_reg_type_srpcim:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}
		if (vp_id > VXGE_HAL_TITAN_SRPCIM_REG_SPACES - 1) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(vxge_hal_srpcim_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->srpcim_reg[vp_id]) + offset));
		break;
	case vxge_hal_mgmt_reg_type_vpmgmt:
		if ((vp_id > VXGE_HAL_TITAN_VPMGMT_REG_SPACES - 1) ||
		    (!(hldev->vpath_assignments & mBIT(vp_id)))) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(vxge_hal_vpmgmt_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->vpmgmt_reg[vp_id]) + offset));
		break;
	case vxge_hal_mgmt_reg_type_vpath:
		if ((vp_id > VXGE_HAL_TITAN_VPATH_REG_SPACES - 1) ||
		    (!(hldev->vpath_assignments & mBIT(vp_id)))) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}
		if (offset > sizeof(vxge_hal_vpath_reg_t) - 8) {
			status = VXGE_HAL_ERR_INVALID_OFFSET;
			break;
		}
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    value,
		    (void *)(((ptr_t) hldev->vpath_reg[vp_id]) + offset));
		break;
	default:
		status = VXGE_HAL_ERR_INVALID_TYPE;
		break;
	}

	vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mgmt_bar0_read - Read X3100 register located at the offset
 *                           from bar0.
 * @devh: HAL device handle.
 * @offset: Register offset from bar0
 * @value: Register value. Returned by HAL.
 * Read X3100 register.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the space is not valid.
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_bar0_read(vxge_hal_device_h devh,
    u32 offset,
    u64 *value)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "devh = 0x"VXGE_OS_STXFMT", offset = %d, value = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, offset, (ptr_t) value);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (((ptr_t) hldev->header.bar0 + offset) >
	    ((ptr_t) hldev->vpath_reg[VXGE_HAL_MAX_VIRTUAL_PATHS - 1] +
	    sizeof(vxge_hal_vpath_reg_t) - 8)) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_OFFSET);
		return (VXGE_HAL_ERR_INVALID_OFFSET);
	}

	*value = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    (void *)(((ptr_t) hldev->header.bar0) + offset));

	vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mgmt_bar1_read - Read X3100 register located at the offset
 *                           from bar1.
 * @devh: HAL device handle.
 * @offset: Register offset from bar1
 * @value: Register value. Returned by HAL.
 * Read X3100 register.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_bar1_read(vxge_hal_device_h devh,
    u32 offset,
    u64 *value)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "devh = 0x"VXGE_OS_STXFMT", offset = %d, value = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, offset, (ptr_t) value);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	*value = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    (void *)(((ptr_t) hldev->header.bar1) + offset));

	vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mgmt_bar0_Write - Write X3100 register located at the offset
 *			    from bar0.
 * @devh: HAL device handle.
 * @offset: Register offset from bar0
 * @value: Register value to be written.
 * Write X3100 register.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the space is not valid.
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_bar0_write(vxge_hal_device_h devh,
    u32 offset,
    u64 value)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "devh = 0x"VXGE_OS_STXFMT", offset = %d, value = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, offset, (ptr_t) value);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}
	if (((ptr_t) hldev->header.bar0 + offset) >
	    ((ptr_t) hldev->vpath_reg[VXGE_HAL_MAX_VIRTUAL_PATHS - 1] +
	    sizeof(vxge_hal_vpath_reg_t) - 8)) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_OFFSET);
		return (VXGE_HAL_ERR_INVALID_OFFSET);
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    value,
	    (void *)(((ptr_t) hldev->header.bar0) + offset));

	vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mgmt_register_config - Retrieve register configuration.
 * @devh: HAL device handle.
 * @type: Register types as defined in enum vxge_hal_mgmt_reg_type_e {}
 * @Index: For pcicfgmgmt, srpcim, vpmgmt, vpath this gives the Index
 *		ignored for others
 * @config: Device configuration, see vxge_hal_device_config_t {}.
 * @size: Pointer to buffer containing the Size of the @reg_config.
 * HAL will return an error if the size is smaller than
 * requested register space and returns required size in this field
 *
 * Get register configuration. Permits to retrieve register values.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_TYPE - Type is not valid.
 * VXGE_HAL_ERR_INVALID_INDEX - Index is not valid.
 * VXGE_HAL_ERR_OUT_OF_SPACE - If the buffer is not sufficient
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_register_config(vxge_hal_device_h devh,
    vxge_hal_mgmt_reg_type_e type, u32 vp_id, u8 *config, u32 *size)
{
	u32 offset;
	u64 *reg_config = (u64 *) ((void *)config);
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (reg_config != NULL) && (size != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device(
	    "devh = 0x"VXGE_OS_STXFMT", type = %d, index = %d, "
	    "reg_config = 0x"VXGE_OS_STXFMT", size = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, type, vp_id, (ptr_t) reg_config, (ptr_t) size);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	switch (type) {
	case vxge_hal_mgmt_reg_type_legacy:
		if (*size < sizeof(vxge_hal_legacy_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_legacy_reg_t);
			break;
		}

		for (offset = 0; offset < sizeof(vxge_hal_legacy_reg_t);
		    offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->legacy_reg) + offset));
		}
		*size = sizeof(vxge_hal_legacy_reg_t);
		break;

	case vxge_hal_mgmt_reg_type_toc:
		if (*size < sizeof(vxge_hal_toc_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_toc_reg_t);
			break;
		}

		for (offset = 0; offset < sizeof(vxge_hal_toc_reg_t);
		    offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->toc_reg) + offset));
		}
		*size = sizeof(vxge_hal_toc_reg_t);
		break;

	case vxge_hal_mgmt_reg_type_common:
		if (*size < sizeof(vxge_hal_common_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_common_reg_t);
			break;
		}

		for (offset = 0; offset < sizeof(vxge_hal_common_reg_t);
		    offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->common_reg) + offset));
		}
		*size = sizeof(vxge_hal_common_reg_t);
		break;

	case vxge_hal_mgmt_reg_type_memrepair:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}

		if (*size < sizeof(vxge_hal_memrepair_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_memrepair_reg_t);
			break;
		}

		for (offset = 0;
		    offset < sizeof(vxge_hal_memrepair_reg_t); offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->memrepair_reg) + offset));
		}
		*size = sizeof(vxge_hal_memrepair_reg_t);
		break;

	case vxge_hal_mgmt_reg_type_pcicfgmgmt:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}

		if (vp_id > VXGE_HAL_TITAN_PCICFGMGMT_REG_SPACES - 1) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}

		if (*size < sizeof(vxge_hal_pcicfgmgmt_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_pcicfgmgmt_reg_t);
			break;
		}

		for (offset = 0; offset < sizeof(vxge_hal_pcicfgmgmt_reg_t);
		    offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->pcicfgmgmt_reg[vp_id]) + offset));
		}
		*size = sizeof(vxge_hal_pcicfgmgmt_reg_t);
		break;

	case vxge_hal_mgmt_reg_type_mrpcim:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}

		if (*size < sizeof(vxge_hal_mrpcim_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_mrpcim_reg_t);
			break;
		}

		for (offset = 0; offset < sizeof(vxge_hal_mrpcim_reg_t);
		    offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->mrpcim_reg) + offset));
		}
		*size = sizeof(vxge_hal_mrpcim_reg_t);
		break;

	case vxge_hal_mgmt_reg_type_srpcim:
		if (!(hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM)) {
			status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;
			break;
		}

		if (vp_id > VXGE_HAL_TITAN_SRPCIM_REG_SPACES - 1) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}

		if (*size < sizeof(vxge_hal_srpcim_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_srpcim_reg_t);
			break;
		}

		for (offset = 0; offset < sizeof(vxge_hal_srpcim_reg_t);
		    offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->srpcim_reg[vp_id]) + offset));
		}
		*size = sizeof(vxge_hal_srpcim_reg_t);
		break;

	case vxge_hal_mgmt_reg_type_vpmgmt:
		if ((vp_id > VXGE_HAL_TITAN_VPMGMT_REG_SPACES - 1) ||
		    (!(hldev->vpath_assignments & mBIT(vp_id)))) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}

		if (*size < sizeof(vxge_hal_vpmgmt_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_vpmgmt_reg_t);
			break;
		}

		for (offset = 0; offset < sizeof(vxge_hal_vpmgmt_reg_t);
		    offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->vpmgmt_reg[vp_id]) + offset));
		}
		*size = sizeof(vxge_hal_vpmgmt_reg_t);
		break;

	case vxge_hal_mgmt_reg_type_vpath:
		if ((vp_id > VXGE_HAL_TITAN_VPATH_REG_SPACES - 1) ||
		    (!(hldev->vpath_assignments & mBIT(vp_id)))) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}

		if (vp_id > VXGE_HAL_TITAN_VPATH_REG_SPACES - 1) {
			status = VXGE_HAL_ERR_INVALID_INDEX;
			break;
		}

		if (*size < sizeof(vxge_hal_vpath_reg_t)) {
			status = VXGE_HAL_ERR_OUT_OF_SPACE;
			*size = sizeof(vxge_hal_vpath_reg_t);
			break;
		}

		for (offset = 0; offset < sizeof(vxge_hal_vpath_reg_t);
		    offset += 8) {
			*reg_config++ = vxge_os_pio_mem_read64(
			    hldev->header.pdev,
			    hldev->header.regh0,
			    (void *)(((ptr_t) hldev->vpath_reg[vp_id]) + offset));
		}
		*size = sizeof(vxge_hal_vpath_reg_t);
		break;

	default:
		status = VXGE_HAL_ERR_INVALID_TYPE;
		break;
	}

	vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mgmt_read_xfp_current_temp - Read current temparature of given port
 * @hldev: HAL device handle.
 * @port: Port number
 *
 * This routine only gets the temperature for XFP modules. Also, updating of the
 * NVRAM can sometimes fail and so the reading we might get may not be uptodate.
 */
u32
vxge_hal_mgmt_read_xfp_current_temp(vxge_hal_device_h devh, u32 port)
{
	u16 val1, val2, count = 0;
	u32 actual;
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT", port = %d",
	    (ptr_t) devh, port);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	val1 = VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_CONTROL_256_BYTES;

	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_WRITE,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_CONTROL,
	    &val1);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	/* Now wait for the transfer to complete */
	do {
		vxge_os_mdelay(50);	/* wait 50 milliseonds */

		status = __hal_mrpcim_mdio_access(devh, port,
		    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ,
		    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
		    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_CONTROL,
		    &val1);

		if (status != VXGE_HAL_OK) {
			vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		if (count++ > 10) {
			/* waited 500 ms which should be plenty of time */
			break;
		}

	} while ((val1 &
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_CONTROL_STAT_MASK)
	    == VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_CONTROL_PROGRESS);

	if ((val1 &
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_CONTROL_STAT_MASK) ==
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_CONTROL_FAILED) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (VXGE_HAL_FAIL);
	}

	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_DATA_XFP_TEMP_1,
	    &val1);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_EEPROM_NVR_DATA_XFP_TEMP_2,
	    &val2);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	actual = ((val1 << 8) | val2);

	if (actual >= 32768)
		actual = actual - 65536;

	actual = actual / 256;

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (actual);

}

/*
 * vxge_hal_mgmt_pma_loopback - Enable or disable PMA loopback
 * @devh: HAL device handle.
 * @port: Port number
 * @enable:Boolean set to 1 to enable and 0 to disable.
 *
 * Enable or disable PMA loopback.
 * Return value:
 * 0 on success.
 */
vxge_hal_status_e
vxge_hal_mgmt_pma_loopback(vxge_hal_device_h devh, u32 port, u32 enable)
{
	u16 val;
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", port = %d, enable = %d",
	    (ptr_t) devh, port, enable);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_PMA_CONTROL_1,
	    &val);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (enable)
		val |=
		    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_PMA_CONTROL_1_LOOPBACK;
	else
		val &=
		    ~VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_PMA_CONTROL_1_LOOPBACK;

	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_WRITE,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_PMA_CONTROL_1,
	    &val);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mgmt_xgmii_loopback - Enable or disable xgmii loopback
 * @devh: HAL device handle.
 * @port: Port number
 * @enable:Boolean set to 1 to enable and 0 to disable.
 *
 * Enable or disable xgmii loopback.
 * Return value:
 * 0 on success.
 */
vxge_hal_status_e
vxge_hal_mgmt_xgmii_loopback(vxge_hal_device_h devh, u32 port, u32 enable)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", port = %d, enable = %d",
	    (ptr_t) devh, port, enable);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->xmac_cfg_port[port]);
	if (enable)
		val64 |= VXGE_HAL_XMAC_CFG_PORT_XGMII_LOOPBACK;
	else
		val64 &= ~VXGE_HAL_XMAC_CFG_PORT_XGMII_LOOPBACK;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->xmac_cfg_port[port]);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

