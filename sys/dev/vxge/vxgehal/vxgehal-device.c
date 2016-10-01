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
 * vxge_hal_pio_mem_write32_upper
 *
 * Endiann-aware implementation of vxge_os_pio_mem_write32().
 * Since X3100 has 64bit registers, we differintiate uppper and lower
 * parts.
 */
void
vxge_hal_pio_mem_write32_upper(pci_dev_h pdev, pci_reg_h regh, u32 val, void *addr)
{
#if defined(VXGE_OS_HOST_BIG_ENDIAN) && !defined(VXGE_OS_PIO_LITTLE_ENDIAN)
	vxge_os_pio_mem_write32(pdev, regh, val, addr);
#else
	vxge_os_pio_mem_write32(pdev, regh, val, (void *) ((char *) addr + 4));
#endif
}

/*
 * vxge_hal_pio_mem_write32_lower
 *
 * Endiann-aware implementation of vxge_os_pio_mem_write32().
 * Since X3100 has 64bit registers, we differintiate uppper and lower
 * parts.
 */
void
vxge_hal_pio_mem_write32_lower(pci_dev_h pdev, pci_reg_h regh, u32 val,
    void *addr)
{
#if defined(VXGE_OS_HOST_BIG_ENDIAN) && !defined(VXGE_OS_PIO_LITTLE_ENDIAN)
	vxge_os_pio_mem_write32(pdev, regh, val,
	    (void *) ((char *) addr + 4));
#else
	vxge_os_pio_mem_write32(pdev, regh, val, addr);
#endif
}

/*
 * vxge_hal_device_pciconfig_get - Read the content of given address
 *			 in pci config space.
 * @devh: Device handle.
 * @offset: Configuration address(offset)to read from
 * @length: Length of the data (1, 2 or 4 bytes)
 * @val: Pointer to a buffer to return the content of the address
 *
 * Read from the pci config space.
 *
 */
vxge_hal_status_e
vxge_hal_device_pciconfig_get(
    vxge_hal_device_h devh,
    u32 offset,
    u32 length,
    void *val)
{
	u32 i;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (val != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpath_assignments & mBIT(i)))
			continue;

		status = __hal_vpath_pci_read(hldev, i,
		    offset, length, val);

		if (status == VXGE_HAL_OK)
			break;

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);

}

/*
 * __hal_device_pci_caps_list_process
 * @hldev: HAL device handle.
 *
 * Process PCI capabilities and initialize the offsets
 */
void
__hal_device_pci_caps_list_process(__hal_device_t *hldev)
{
	u8 cap_id;
	u16 ext_cap_id;
	u16 next_ptr;
	u32 *ptr_32;
	vxge_hal_pci_config_t *pci_config = &hldev->pci_config_space_bios;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	next_ptr = pci_config->capabilities_pointer;

	while (next_ptr != 0) {

		cap_id = VXGE_HAL_PCI_CAP_ID((((u8 *) pci_config) + next_ptr));

		switch (cap_id) {

		case VXGE_HAL_PCI_CAP_ID_PM:
			hldev->pci_caps.pm_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_CAP_ID_VPD:
			hldev->pci_caps.vpd_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_CAP_ID_SLOTID:
			hldev->pci_caps.sid_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_CAP_ID_MSI:
			hldev->pci_caps.msi_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_CAP_ID_VS:
			hldev->pci_caps.vs_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_CAP_ID_SHPC:
			hldev->pci_caps.shpc_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_CAP_ID_PCIE:
			hldev->pci_e_caps = next_ptr;
			break;
		case VXGE_HAL_PCI_CAP_ID_MSIX:
			hldev->pci_caps.msix_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_CAP_ID_AGP:
		case VXGE_HAL_PCI_CAP_ID_CHSWP:
		case VXGE_HAL_PCI_CAP_ID_PCIX:
		case VXGE_HAL_PCI_CAP_ID_HT:
		case VXGE_HAL_PCI_CAP_ID_DBGPORT:
		case VXGE_HAL_PCI_CAP_ID_CPCICSR:
		case VXGE_HAL_PCI_CAP_ID_PCIBSVID:
		case VXGE_HAL_PCI_CAP_ID_AGP8X:
		case VXGE_HAL_PCI_CAP_ID_SECDEV:
			vxge_hal_info_log_device("Unexpected Capability = %d",
			    cap_id);
			break;
		default:
			vxge_hal_info_log_device("Unknown capability = %d",
			    cap_id);
			break;
		}

		next_ptr =
		    VXGE_HAL_PCI_CAP_NEXT((((u8 *) pci_config) + next_ptr));

	}

	/* CONSTCOND */
	if (VXGE_HAL_PCI_CONFIG_SPACE_SIZE <= 0x100) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return;
	}

	next_ptr = 0x100;
	while (next_ptr != 0) {

		ptr_32 = (u32 *) ((void *) (((u8 *) pci_config) + next_ptr));
		ext_cap_id = (u16) (VXGE_HAL_PCI_EXT_CAP_ID(*ptr_32));

		switch (ext_cap_id) {

		case VXGE_HAL_PCI_EXT_CAP_ID_ERR:
			hldev->pci_e_ext_caps.err_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_EXT_CAP_ID_VC:
			hldev->pci_e_ext_caps.vc_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_EXT_CAP_ID_DSN:
			hldev->pci_e_ext_caps.dsn_cap_offset = next_ptr;
			break;
		case VXGE_HAL_PCI_EXT_CAP_ID_PWR:
			hldev->pci_e_ext_caps.pwr_budget_cap_offset = next_ptr;
			break;

		default:
			vxge_hal_info_log_device("Unknown capability = %d",
			    ext_cap_id);
			break;
		}

		next_ptr = (u16) VXGE_HAL_PCI_EXT_CAP_NEXT(*ptr_32);
	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_device_pci_e_init
 * @hldev: HAL device handle.
 *
 * Initialize certain PCI/PCI-X configuration registers
 * with recommended values. Save config space for future hw resets.
 */
void
__hal_device_pci_e_init(__hal_device_t *hldev)
{
	int i;
	u16 cmd;
	u32 *ptr_32;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	/* save original PCI config space to restore it on device_terminate() */
	ptr_32 = (u32 *) ((void *) &hldev->pci_config_space_bios);
	for (i = 0; i < VXGE_HAL_PCI_CONFIG_SPACE_SIZE / 4; i++) {
		(void) __hal_vpath_pci_read(hldev,
		    hldev->first_vp_id,
		    i * 4,
		    4,
		    ptr_32 + i);
	}

	__hal_device_pci_caps_list_process(hldev);

	/* Set the PErr Repconse bit and SERR in PCI command register. */
	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    vxge_offsetof(vxge_hal_pci_config_le_t, command),
	    2,
	    &cmd);
	cmd |= 0x140;
	vxge_os_pci_write16(hldev->header.pdev, hldev->header.cfgh,
	    vxge_offsetof(vxge_hal_pci_config_le_t, command), cmd);

	/* save PCI config space for future resets */
	ptr_32 = (u32 *) ((void *) &hldev->pci_config_space);
	for (i = 0; i < VXGE_HAL_PCI_CONFIG_SPACE_SIZE / 4; i++) {
		(void) __hal_vpath_pci_read(hldev,
		    hldev->first_vp_id,
		    i * 4,
		    4,
		    ptr_32 + i);
	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_device_bus_master_enable
 * @hldev: HAL device handle.
 *
 * Enable bus mastership.
 */
static void
__hal_device_bus_master_enable(__hal_device_t *hldev)
{
	u16 cmd;
	u16 bus_master = 4;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    vxge_offsetof(vxge_hal_pci_config_le_t, command),
	    2,
	    &cmd);
	/* already enabled? do nothing */
	if (cmd & bus_master)
		return;

	cmd |= bus_master;
	vxge_os_pci_write16(hldev->header.pdev, hldev->header.cfgh,
	    vxge_offsetof(vxge_hal_pci_config_le_t, command), cmd);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_register_poll
 * @pdev: PCI device object.
 * @regh: BAR mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @reg: register to poll for
 * @op: 0 - bit reset, 1 - bit set
 * @mask: mask for logical "and" condition based on %op
 * @max_millis: maximum time to try to poll in milliseconds
 *
 * Will poll certain register for specified amount of time.
 * Will poll until masked bit is not cleared.
 */
vxge_hal_status_e
vxge_hal_device_register_poll(
    pci_dev_h pdev,
    pci_reg_h regh,
    u64 *reg,
    u32 op,
    u64 mask,
    u32 max_millis)
{
	u64 val64;
	u32 i = 0;
	vxge_hal_status_e ret = VXGE_HAL_FAIL;

	vxge_os_udelay(10);

	do {
		val64 = vxge_os_pio_mem_read64(pdev, regh, reg);
		if (op == 0 && !(val64 & mask)) {
			return (VXGE_HAL_OK);
		} else if (op == 1 && (val64 & mask) == mask)
			return (VXGE_HAL_OK);
		vxge_os_udelay(100);
	} while (++i <= 9);

	do {
		val64 = vxge_os_pio_mem_read64(pdev, regh, reg);
		if (op == 0 && !(val64 & mask)) {
			return (VXGE_HAL_OK);
		} else if (op == 1 && (val64 & mask) == mask) {
			return (VXGE_HAL_OK);
		}
		vxge_os_udelay(1000);
	} while (++i < max_millis);

	return (ret);
}

/*
 * __hal_device_register_stall
 * @pdev: PCI device object.
 * @regh: BAR mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @reg: register to poll for
 * @op: 0 - bit reset, 1 - bit set
 * @mask: mask for logical "and" condition based on %op
 * @max_millis: maximum time to try to poll in milliseconds
 *
 * Will poll certain register for specified amount of time.
 * Will poll until masked bit is not cleared.
 */
vxge_hal_status_e
__hal_device_register_stall(
    pci_dev_h pdev,
    pci_reg_h regh,
    u64 *reg,
    u32 op,
    u64 mask,
    u32 max_millis)
{
	u64 val64;
	u32 i = 0;
	vxge_hal_status_e ret = VXGE_HAL_FAIL;

	vxge_os_stall(10);

	do {
		val64 = vxge_os_pio_mem_read64(pdev, regh, reg);
		if (op == 0 && !(val64 & mask)) {
			return (VXGE_HAL_OK);
		} else if (op == 1 && (val64 & mask) == mask)
			return (VXGE_HAL_OK);
		vxge_os_stall(100);
	} while (++i <= 9);

	do {
		val64 = vxge_os_pio_mem_read64(pdev, regh, reg);
		if (op == 0 && !(val64 & mask)) {
			return (VXGE_HAL_OK);
		} else if (op == 1 && (val64 & mask) == mask) {
			return (VXGE_HAL_OK);
		}
		vxge_os_stall(1000);
	} while (++i < max_millis);

	return (ret);
}

void*
vxge_hal_device_get_legacy_reg(pci_dev_h pdev, pci_reg_h regh, u8 *bar0)
{
	vxge_hal_legacy_reg_t *legacy_reg = NULL;

	/*
	 * If length of Bar0 is 16MB, then assume we are configured
	 * in MF8P_VP2 mode and add 8MB to get legacy_reg offsets
	 */
	if (vxge_os_pci_res_len(pdev, regh) == 0x1000000)
		legacy_reg = (vxge_hal_legacy_reg_t *)
		    ((void *) (bar0 + 0x800000));
	else
		legacy_reg = (vxge_hal_legacy_reg_t *)
		    ((void *) bar0);

	return (legacy_reg);
}

/*
 * __hal_device_reg_addr_get
 * @hldev: HAL Device object.
 *
 * This routine sets the swapper and reads the toc pointer and initializes the
 * register location pointers in the device object. It waits until the ric is
 * completed initializing registers.
 */
vxge_hal_status_e
__hal_device_reg_addr_get(__hal_device_t *hldev)
{
	u64 val64;
	u32 i;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(hldev);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	hldev->legacy_reg = (vxge_hal_legacy_reg_t *)
	    vxge_hal_device_get_legacy_reg(hldev->header.pdev,
		hldev->header.regh0, hldev->header.bar0);

	status = __hal_legacy_swapper_set(hldev->header.pdev,
	    hldev->header.regh0,
	    hldev->legacy_reg);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->legacy_reg->toc_first_pointer);

	hldev->toc_reg = (vxge_hal_toc_reg_t *)
	    ((void *) (hldev->header.bar0 + val64));

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->toc_reg->toc_common_pointer);

	hldev->common_reg = (vxge_hal_common_reg_t *)
	    ((void *) (hldev->header.bar0 + val64));

	vxge_hal_info_log_device("COMMON = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev->common_reg);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->toc_reg->toc_memrepair_pointer);

	hldev->memrepair_reg = (vxge_hal_memrepair_reg_t *)
	    ((void *) (hldev->header.bar0 + val64));

	vxge_hal_info_log_device("MEM REPAIR = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev->memrepair_reg);

	for (i = 0; i < VXGE_HAL_TITAN_PCICFGMGMT_REG_SPACES; i++) {
		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->toc_reg->toc_pcicfgmgmt_pointer[i]);

		hldev->pcicfgmgmt_reg[i] = (vxge_hal_pcicfgmgmt_reg_t *)
		    ((void *) (hldev->header.bar0 + val64));
		vxge_hal_info_log_device("PCICFG_MGMT[%d] = "
		    "0x"VXGE_OS_STXFMT, i, (ptr_t) hldev->pcicfgmgmt_reg[i]);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->toc_reg->toc_mrpcim_pointer);

	hldev->mrpcim_reg = (vxge_hal_mrpcim_reg_t *)
	    ((void *) (hldev->header.bar0 + val64));

	vxge_hal_info_log_device("MEM REPAIR = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev->mrpcim_reg);

	for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->toc_reg->toc_srpcim_pointer[i]);

		hldev->srpcim_reg[i] = (vxge_hal_srpcim_reg_t *)
		    ((void *) (hldev->header.bar0 + val64));
		vxge_hal_info_log_device("SRPCIM[%d] =0x"VXGE_OS_STXFMT, i,
		    (ptr_t) hldev->srpcim_reg[i]);
	}

	for (i = 0; i < VXGE_HAL_TITAN_VPMGMT_REG_SPACES; i++) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->toc_reg->toc_vpmgmt_pointer[i]);

		hldev->vpmgmt_reg[i] = (vxge_hal_vpmgmt_reg_t *)
		    ((void *) (hldev->header.bar0 + val64));

		vxge_hal_info_log_device("VPMGMT[%d] = 0x"VXGE_OS_STXFMT, i,
		    (ptr_t) hldev->vpmgmt_reg[i]);
	}

	for (i = 0; i < VXGE_HAL_TITAN_VPATH_REG_SPACES; i++) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->toc_reg->toc_vpath_pointer[i]);

		hldev->vpath_reg[i] = (vxge_hal_vpath_reg_t *)
		    ((void *) (hldev->header.bar0 + val64));

		vxge_hal_info_log_device("VPATH[%d] = 0x"VXGE_OS_STXFMT, i,
		    (ptr_t) hldev->vpath_reg[i]);

	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->toc_reg->toc_kdfc);

	switch (VXGE_HAL_TOC_GET_KDFC_INITIAL_BIR(val64)) {
	case 0:
		hldev->kdfc = hldev->header.bar0 +
		    VXGE_HAL_TOC_GET_KDFC_INITIAL_OFFSET(val64);
		break;
	case 2:
		hldev->kdfc = hldev->header.bar1 +
		    VXGE_HAL_TOC_GET_KDFC_INITIAL_OFFSET(val64);
		break;
	case 4:
		hldev->kdfc = hldev->header.bar2 +
		    VXGE_HAL_TOC_GET_KDFC_INITIAL_OFFSET(val64);
		break;
	default:
		vxge_hal_info_log_device("Invalid BIR = 0x"VXGE_OS_STXFMT,
		    (ptr_t) VXGE_HAL_TOC_GET_KDFC_INITIAL_BIR(val64));
		break;
	}

	vxge_hal_info_log_device("KDFC = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev->kdfc);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->toc_reg->toc_usdc);

	switch (VXGE_HAL_TOC_GET_USDC_INITIAL_BIR(val64)) {
	case 0:
		hldev->usdc = hldev->header.bar0 +
		    VXGE_HAL_TOC_GET_USDC_INITIAL_OFFSET(val64);
		break;
	case 2:
		hldev->usdc = hldev->header.bar1 +
		    VXGE_HAL_TOC_GET_USDC_INITIAL_OFFSET(val64);
		break;
	case 4:
		hldev->usdc = hldev->header.bar2 +
		    VXGE_HAL_TOC_GET_USDC_INITIAL_OFFSET(val64);
		break;
	default:
		vxge_hal_info_log_device("Invalid BIR = 0x"VXGE_OS_STXFMT,
		    (ptr_t) VXGE_HAL_TOC_GET_USDC_INITIAL_BIR(val64));
		break;
	}

	vxge_hal_info_log_device("USDC = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev->usdc);

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->vpath_rst_in_prog, 0,
	    VXGE_HAL_VPATH_RST_IN_PROG_VPATH_RST_IN_PROG(0x1ffff),
	    VXGE_HAL_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HAL_OK) {
		vxge_hal_err_log_device("%s:vpath_rst_in_prog is not cleared",
		    __func__);
	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_device_id_get
 * @hldev: HAL Device object.
 *
 * This routine returns sets the device id and revision numbers into the device
 * structure
 */
void
__hal_device_id_get(__hal_device_t *hldev)
{
	vxge_assert(hldev);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    vxge_offsetof(vxge_hal_pci_config_le_t, device_id),
	    2,
	    &hldev->header.device_id);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    vxge_offsetof(vxge_hal_pci_config_le_t, revision),
	    2,
	    &hldev->header.revision);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_device_access_rights_get: Get Access Rights of the driver
 * @host_type: Host type.
 * @func_id: Function Id
 *
 * This routine returns the Access Rights of the driver
 */
u32
__hal_device_access_rights_get(u32 host_type, u32 func_id)
{
	u32 access_rights = VXGE_HAL_DEVICE_ACCESS_RIGHT_VPATH;

	switch (host_type) {
	case VXGE_HAL_NO_MR_NO_SR_NORMAL_FUNCTION:
		if (func_id == 0) {
			access_rights |=
			    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM |
			    VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM;
		}
		break;
	case VXGE_HAL_MR_NO_SR_VH0_BASE_FUNCTION:
		access_rights |= VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM |
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	case VXGE_HAL_NO_MR_SR_VH0_FUNCTION0:
		access_rights |= VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM |
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	case VXGE_HAL_NO_MR_SR_VH0_VIRTUAL_FUNCTION:
	case VXGE_HAL_SR_VH_VIRTUAL_FUNCTION:
		break;
	case VXGE_HAL_MR_SR_VH0_INVALID_CONFIG:
		break;
	case VXGE_HAL_SR_VH_FUNCTION0:
	case VXGE_HAL_VH_NORMAL_FUNCTION:
		access_rights |= VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM;
		break;
	}

	return (access_rights);
}

/*
 * __hal_device_host_info_get
 * @hldev: HAL Device object.
 *
 * This routine returns the host type assignments
 */
void
__hal_device_host_info_get(__hal_device_t *hldev)
{
	u64 val64;
	u32 i;

	vxge_assert(hldev);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->host_type_assignments);

	hldev->host_type = (u32)
	    VXGE_HAL_HOST_TYPE_ASSIGNMENTS_GET_HOST_TYPE_ASSIGNMENTS(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->vplane_assignments);

	hldev->srpcim_id = (u32)
	    VXGE_HAL_VPLANE_ASSIGNMENTS_GET_VPLANE_ASSIGNMENTS(val64);

	hldev->vpath_assignments = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->vpath_assignments);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpath_assignments & mBIT(i)))
			continue;

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->debug_assignments);
		hldev->vh_id =
		    (u32) VXGE_HAL_DEBUG_ASSIGNMENTS_GET_VHLABEL(val64);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->vpmgmt_reg[i]->vpath_to_func_map_cfg1);
		hldev->func_id =
		    (u32) VXGE_HAL_VPATH_TO_FUNC_MAP_CFG1_GET_CFG1(val64);

		hldev->access_rights = __hal_device_access_rights_get(
		    hldev->host_type, hldev->func_id);

		if (hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {
			hldev->manager_up = TRUE;
		} else {
			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &hldev->vpmgmt_reg[i]->srpcim_to_vpath_wmsg);

			hldev->manager_up = __hal_ifmsg_is_manager_up(val64);
		}

		hldev->first_vp_id = i;

		break;

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_device_pci_e_info_get - Get PCI_E bus informations such as link_width
 *				  and signalling rate
 * @hldev: HAL device.
 * @signalling_rate:	pointer to a variable of enumerated type
 *			vxge_hal_pci_e_signalling_rate_e {}.
 * @link_width:		pointer to a variable of enumerated type
 *			vxge_hal_pci_e_link_width_e {}.
 *
 * Get pci-e signalling rate and link width.
 *
 * Returns: one of the vxge_hal_status_e {} enumerated types.
 * VXGE_HAL_OK			- for success.
 * VXGE_HAL_ERR_INVALID_PCI_INFO - for invalid PCI information from the card.
 * VXGE_HAL_ERR_BAD_DEVICE_ID	- for invalid card.
 *
 */
static vxge_hal_status_e
__hal_device_pci_e_info_get(
    __hal_device_t *hldev,
    vxge_hal_pci_e_signalling_rate_e *signalling_rate,
    vxge_hal_pci_e_link_width_e *link_width)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_pci_e_capability_t *pci_e_caps;

	vxge_assert((hldev != NULL) && (signalling_rate != NULL) &&
	    (link_width != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT
	    ", signalling_rate = 0x"VXGE_OS_STXFMT", "
	    "link_width = 0x"VXGE_OS_STXFMT, (ptr_t) hldev,
	    (ptr_t) signalling_rate, (ptr_t) link_width);

	pci_e_caps = (vxge_hal_pci_e_capability_t *)
	    (((u8 *) &hldev->pci_config_space_bios) + hldev->pci_e_caps);

	switch (pci_e_caps->pci_e_lnkcap & VXGE_HAL_PCI_EXP_LNKCAP_LNK_SPEED) {
	case VXGE_HAL_PCI_EXP_LNKCAP_LS_2_5:
		*signalling_rate = VXGE_HAL_PCI_E_SIGNALLING_RATE_2_5GB;
		break;
	case VXGE_HAL_PCI_EXP_LNKCAP_LS_5:
		*signalling_rate = VXGE_HAL_PCI_E_SIGNALLING_RATE_5GB;
		break;
	default:
		*signalling_rate =
		    VXGE_HAL_PCI_E_SIGNALLING_RATE_UNKNOWN;
		break;
	}

	switch ((pci_e_caps->pci_e_lnksta &
	    VXGE_HAL_PCI_EXP_LNKCAP_LNK_WIDTH) >> 4) {
	case VXGE_HAL_PCI_EXP_LNKCAP_LW_X1:
		*link_width = VXGE_HAL_PCI_E_LINK_WIDTH_X1;
		break;
	case VXGE_HAL_PCI_EXP_LNKCAP_LW_X2:
		*link_width = VXGE_HAL_PCI_E_LINK_WIDTH_X2;
		break;
	case VXGE_HAL_PCI_EXP_LNKCAP_LW_X4:
		*link_width = VXGE_HAL_PCI_E_LINK_WIDTH_X4;
		break;
	case VXGE_HAL_PCI_EXP_LNKCAP_LW_X8:
		*link_width = VXGE_HAL_PCI_E_LINK_WIDTH_X8;
		break;
	case VXGE_HAL_PCI_EXP_LNKCAP_LW_X12:
		*link_width = VXGE_HAL_PCI_E_LINK_WIDTH_X12;
		break;
	case VXGE_HAL_PCI_EXP_LNKCAP_LW_X16:
		*link_width = VXGE_HAL_PCI_E_LINK_WIDTH_X16;
		break;
	case VXGE_HAL_PCI_EXP_LNKCAP_LW_X32:
		*link_width = VXGE_HAL_PCI_E_LINK_WIDTH_X32;
		break;
	case VXGE_HAL_PCI_EXP_LNKCAP_LW_RES:
	default:
		*link_width = VXGE_HAL_PCI_E_LINK_WIDTH_UNKNOWN;
		break;
	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (status);
}

/*
 * __hal_device_hw_initialize
 * @hldev: HAL device handle.
 *
 * Initialize X3100-V hardware.
 */
vxge_hal_status_e
__hal_device_hw_initialize(__hal_device_t *hldev)
{
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(hldev);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	__hal_device_pci_e_init(hldev);

	/* update the pci mode, frequency, and width */
	if (__hal_device_pci_e_info_get(hldev, &hldev->header.signalling_rate,
	    &hldev->header.link_width) != VXGE_HAL_OK) {
		hldev->header.signalling_rate =
		    VXGE_HAL_PCI_E_SIGNALLING_RATE_UNKNOWN;
		hldev->header.link_width = VXGE_HAL_PCI_E_LINK_WIDTH_UNKNOWN;
		/*
		 * FIXME: this cannot happen.
		 * But if it happens we cannot continue just like that
		 */
		vxge_hal_err_log_device("unable to get pci info == > %s : %d",
		    __func__, __LINE__);
	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM) {
		status = __hal_srpcim_initialize(hldev);
	}

	if (hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {
		status = __hal_mrpcim_initialize(hldev);
	}

	if (status == VXGE_HAL_OK) {
		hldev->hw_is_initialized = 1;
		hldev->header.terminating = 0;
	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_reset - Reset device.
 * @devh: HAL device handle.
 *
 * Soft-reset the device, reset the device stats except reset_cnt.
 *
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_device_reset(vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!hldev->header.is_initialized) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);
		return (VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);
	}

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		status = vxge_hal_vpath_reset(
		    VXGE_HAL_VIRTUAL_PATH_HANDLE(&hldev->virtual_paths[i]));

		if (status != VXGE_HAL_OK) {
			vxge_hal_err_log_device("vpath %d Reset Failed", i);
		}

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_reset_poll - Poll the device for reset complete.
 * @devh: HAL device handle.
 *
 * Poll the device for reset complete
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_device_reset_poll(vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!hldev->header.is_initialized) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);
		return (VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);
	}

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		status = vxge_hal_vpath_reset_poll(
		    VXGE_HAL_VIRTUAL_PATH_HANDLE(&hldev->virtual_paths[i]));

		if (status != VXGE_HAL_OK) {
			vxge_hal_err_log_device("vpath %d Reset Poll Failed",
			    i);
		}

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_device_mrpcim_reset_poll - Poll the device for mrpcim reset complete
 * @devh: HAL device handle.
 *
 * Poll the device for mrpcim reset complete
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 * VXGE_HAL_ERR_MANAGER_NOT_FOUND - MRPCIM/SRPCIM manager not found
 * VXGE_HAL_ERR_TIME_OUT - Device Reset timed out
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_device_mrpcim_reset_poll(vxge_hal_device_h devh)
{
	u32 i = 0;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!hldev->header.is_initialized) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);
		return (VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);
	}

	if (!hldev->manager_up) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_MANAGER_NOT_FOUND);
		return (VXGE_HAL_ERR_MANAGER_NOT_FOUND);
	}

	status = __hal_ifmsg_device_reset_end_poll(
	    hldev, hldev->first_vp_id);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_TIME_OUT);
		return (VXGE_HAL_ERR_TIME_OUT);
	}

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		status = vxge_hal_vpath_reset_poll(
		    VXGE_HAL_VIRTUAL_PATH_HANDLE(&hldev->virtual_paths[i]));

		if (status != VXGE_HAL_OK) {
			vxge_hal_err_log_device("vpath %d Reset Poll Failed",
			    i);
		}

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_device_status - Check whether X3100 hardware is ready for
 * operation.
 * @devh: HAL device handle.
 * @hw_status: X3100 status register. Returned by HAL.
 *
 * Check whether X3100 hardware is ready for operation.
 * The checking includes TDMA, RDMA, PFC, PIC, MC_DRAM, and the rest
 * hardware functional blocks.
 *
 * Returns: VXGE_HAL_OK if the device is ready for operation. Otherwise
 * returns VXGE_HAL_FAIL. Also, fills in  adapter status (in @hw_status).
 *
 * See also: vxge_hal_status_e {}.
 * Usage: See ex_open {}.
 */
vxge_hal_status_e
vxge_hal_device_status(vxge_hal_device_h devh, u64 *hw_status)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (hw_status != NULL));

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	*hw_status = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->adapter_status);

	vxge_hal_trace_log_device("Adapter_Status = 0x"VXGE_OS_LLXFMT,
	    *hw_status);

	if (!(*hw_status & VXGE_HAL_ADAPTER_STATUS_RTDMA_RTDMA_READY)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_RTDMA_RTDMA_READY);
		return (VXGE_HAL_ERR_RTDMA_RTDMA_READY);
	}

	if (!(*hw_status & VXGE_HAL_ADAPTER_STATUS_WRDMA_WRDMA_READY)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_WRDMA_WRDMA_READY);
		return (VXGE_HAL_ERR_WRDMA_WRDMA_READY);
	}

	if (!(*hw_status & VXGE_HAL_ADAPTER_STATUS_KDFC_KDFC_READY)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_KDFC_KDFC_READY);
		return (VXGE_HAL_ERR_KDFC_KDFC_READY);
	}

	if (!(*hw_status & VXGE_HAL_ADAPTER_STATUS_TPA_TMAC_BUF_EMPTY)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_TPA_TMAC_BUF_EMPTY);
		return (VXGE_HAL_ERR_TPA_TMAC_BUF_EMPTY);
	}

	if (!(*hw_status & VXGE_HAL_ADAPTER_STATUS_RDCTL_PIC_QUIESCENT)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_RDCTL_PIC_QUIESCENT);
		return (VXGE_HAL_ERR_RDCTL_PIC_QUIESCENT);
	}

	if (*hw_status & VXGE_HAL_ADAPTER_STATUS_XGMAC_NETWORK_FAULT) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_XGMAC_NETWORK_FAULT);
		return (VXGE_HAL_ERR_XGMAC_NETWORK_FAULT);
	}

	if (!(*hw_status & VXGE_HAL_ADAPTER_STATUS_ROCRC_OFFLOAD_QUIESCENT)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_ROCRC_OFFLOAD_QUIESCENT);
		return (VXGE_HAL_ERR_ROCRC_OFFLOAD_QUIESCENT);
	}

	if (!(*hw_status &
	    VXGE_HAL_ADAPTER_STATUS_G3IF_FB_G3IF_FB_GDDR3_READY)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_G3IF_FB_G3IF_FB_GDDR3_READY);
		return (VXGE_HAL_ERR_G3IF_FB_G3IF_FB_GDDR3_READY);
	}

	if (!(*hw_status &
	    VXGE_HAL_ADAPTER_STATUS_G3IF_CM_G3IF_CM_GDDR3_READY)) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_G3IF_CM_G3IF_CM_GDDR3_READY);
		return (VXGE_HAL_ERR_G3IF_CM_G3IF_CM_GDDR3_READY);
	}

#ifndef	VXGE_HAL_TITAN_EMULATION
	if (*hw_status & VXGE_HAL_ADAPTER_STATUS_RIC_RIC_RUNNING) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_RIC_RIC_RUNNING);
		return (VXGE_HAL_ERR_RIC_RIC_RUNNING);
	}
#endif

	if (*hw_status & VXGE_HAL_ADAPTER_STATUS_CMG_C_PLL_IN_LOCK) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_CMG_C_PLL_IN_LOCK);
		return (VXGE_HAL_ERR_CMG_C_PLL_IN_LOCK);
	}

	if (*hw_status & VXGE_HAL_ADAPTER_STATUS_XGMAC_X_PLL_IN_LOCK) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_XGMAC_X_PLL_IN_LOCK);
		return (VXGE_HAL_ERR_XGMAC_X_PLL_IN_LOCK);
	}

	if (*hw_status & VXGE_HAL_ADAPTER_STATUS_FBIF_M_PLL_IN_LOCK) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_FBIF_M_PLL_IN_LOCK);
		return (VXGE_HAL_ERR_FBIF_M_PLL_IN_LOCK);
	}

	if (!(*hw_status & VXGE_HAL_ADAPTER_STATUS_PCC_PCC_IDLE(0xFF))) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PCC_PCC_IDLE);
		return (VXGE_HAL_ERR_PCC_PCC_IDLE);
	}

	if (!(*hw_status &
	    VXGE_HAL_ADAPTER_STATUS_ROCRC_RC_PRC_QUIESCENT(0xFF))) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_ROCRC_RC_PRC_QUIESCENT);
		return (VXGE_HAL_ERR_ROCRC_RC_PRC_QUIESCENT);
	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0x"VXGE_OS_STXFMT,
	    __FILE__, __func__, __LINE__, (ptr_t) *hw_status);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_device_is_slot_freeze
 * @devh: the device
 *
 * Returns non-zero if the slot is freezed.
 * The determination is made based on the adapter_status
 * register which will never give all FFs, unless PCI read
 * cannot go through.
 */
int
vxge_hal_device_is_slot_freeze(vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;
	u16 device_id;
	u64 adapter_status;

	vxge_assert(devh);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	adapter_status = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->adapter_status);

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    vxge_offsetof(vxge_hal_pci_config_le_t, device_id),
	    2,
	    &device_id);
	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__,
	    (adapter_status == VXGE_HAL_ALL_FOXES) || (device_id == 0xffff));

	return ((adapter_status == VXGE_HAL_ALL_FOXES) ||
	    (device_id == 0xffff));
}

/*
 * vxge_hal_device_intr_enable - Enable interrupts.
 * @devh: HAL device handle.
 * @op: One of the vxge_hal_device_intr_e enumerated values specifying
 *	  the type(s) of interrupts to enable.
 *
 * Enable X3100 interrupts. The function is to be executed the last in
 * X3100 initialization sequence.
 *
 * See also: vxge_hal_device_intr_disable()
 */
void
vxge_hal_device_intr_enable(
    vxge_hal_device_h devh)
{
	u32 i;
	u64 val64;
	u32 val32;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	vxge_hal_device_mask_all(hldev);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		(void) __hal_vpath_intr_enable(&hldev->virtual_paths[i]);
	}

	if ((hldev->header.config.intr_mode == VXGE_HAL_INTR_MODE_IRQLINE) ||
	    (hldev->header.config.intr_mode == VXGE_HAL_INTR_MODE_EMULATED_INTA)) {

		val64 = hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX] |
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_BMAP];

		if (val64 != 0) {
			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    val64,
			    &hldev->common_reg->tim_int_status0);

			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    ~val64,
			    &hldev->common_reg->tim_int_mask0);
		}

		val32 = hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX] |
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_BMAP];

		if (val32 != 0) {
			vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
			    hldev->header.regh0,
			    val32,
			    &hldev->common_reg->tim_int_status1);

			vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
			    hldev->header.regh0,
			    ~val32,
			    &hldev->common_reg->tim_int_mask1);
		}
	}

	vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->titan_general_int_status);

	vxge_hal_device_unmask_all(hldev);

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_intr_disable - Disable X3100 interrupts.
 * @devh: HAL device handle.
 * @op: One of the vxge_hal_device_intr_e enumerated values specifying
 *	  the type(s) of interrupts to disable.
 *
 * Disable X3100 interrupts.
 *
 * See also: vxge_hal_device_intr_enable()
 */
void
vxge_hal_device_intr_disable(
    vxge_hal_device_h devh)
{
	u32 i;
	u64 val64;
	u32 val32;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	vxge_hal_device_mask_all(hldev);

	if ((hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_IRQLINE) ||
	    (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_EMULATED_INTA)) {

		val64 = hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX] |
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_BMAP];

		if (val64 != 0) {
			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    val64,
			    &hldev->common_reg->tim_int_mask0);
		}

		val32 = hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX] |
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_BMAP];

		if (val32 != 0) {
			vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
			    hldev->header.regh0,
			    val32,
			    &hldev->common_reg->tim_int_mask1);
		}
	}

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		(void) __hal_vpath_intr_disable(&hldev->virtual_paths[i]);
	}

	vxge_hal_device_unmask_all(hldev);

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_mask_all - Mask all device interrupts.
 * @devh: HAL device handle.
 *
 * Mask	all	device interrupts.
 *
 * See also: vxge_hal_device_unmask_all()
 */
void
vxge_hal_device_mask_all(
    vxge_hal_device_h devh)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	val64 = VXGE_HAL_TITAN_MASK_ALL_INT_ALARM |
	    VXGE_HAL_TITAN_MASK_ALL_INT_TRAFFIC;

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->titan_mask_all_int);

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_unmask_all - Unmask all device interrupts.
 * @devh: HAL device handle.
 *
 * Unmask all device interrupts.
 *
 * See also: vxge_hal_device_mask_all()
 */
void
vxge_hal_device_unmask_all(
    vxge_hal_device_h devh)
{
	u64 val64 = 0;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (hldev->header.config.intr_mode == VXGE_HAL_INTR_MODE_IRQLINE)
		val64 = VXGE_HAL_TITAN_MASK_ALL_INT_TRAFFIC;

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->titan_mask_all_int);

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_begin_irq - Begin IRQ processing.
 * @devh: HAL device handle.
 * @skip_alarms: Do not clear the alarms
 * @reason: "Reason" for the interrupt,	the value of X3100's
 *			general_int_status register.
 *
 * The function	performs two actions, It first checks whether (shared IRQ) the
 * interrupt was raised	by the device. Next, it	masks the device interrupts.
 *
 * Note:
 * vxge_hal_device_begin_irq() does not flush MMIO writes through the
 * bridge. Therefore, two back-to-back interrupts are potentially possible.
 * It is the responsibility	of the ULD to make sure	that only one
 * vxge_hal_device_continue_irq() runs at a time.
 *
 * Returns: 0, if the interrupt	is not "ours" (note that in this case the
 * device remain enabled).
 * Otherwise, vxge_hal_device_begin_irq() returns 64bit general adapter
 * status.
 * See also: vxge_hal_device_handle_irq()
 */
vxge_hal_status_e
vxge_hal_device_begin_irq(
    vxge_hal_device_h devh,
    u32 skip_alarms,
    u64 *reason)
{
	u32 i;
	u64 val64;
	u64 adapter_status;
	u64 vpath_mask;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	vxge_hal_status_e ret = VXGE_HAL_ERR_WRONG_IRQ;
	vxge_hal_status_e status;

	vxge_assert((hldev != NULL) && (reason != NULL));

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq(
	    "devh = 0x"VXGE_OS_STXFMT", skip_alarms = %d, "
	    "reason = 0x"VXGE_OS_STXFMT, (ptr_t) devh,
	    skip_alarms, (ptr_t) reason);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->titan_general_int_status);

	if (vxge_os_unlikely(!val64)) {
		/* not Titan interrupt	 */
		*reason = 0;
		ret = VXGE_HAL_ERR_WRONG_IRQ;
		vxge_hal_info_log_device_irq("wrong_isr general_int_status = \
		    0x%llx", val64);
		vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, ret);
		return (ret);
	}

	if (vxge_os_unlikely(val64 == VXGE_HAL_ALL_FOXES)) {

		adapter_status = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->adapter_status);

		if (adapter_status == VXGE_HAL_ALL_FOXES) {
			vxge_hal_info_log_device_irq("%s:Slot is frozen",
			    __func__);
			__hal_device_handle_error(hldev,
			    NULL_VPID, VXGE_HAL_EVENT_SLOT_FREEZE);
			*reason = 0;
			ret = VXGE_HAL_ERR_SLOT_FREEZE;
			goto exit;

		}
	}

	*reason = val64;

	vpath_mask = hldev->vpaths_deployed >>
	    (64 - VXGE_HAL_MAX_VIRTUAL_PATHS);

	if (val64 &
	    VXGE_HAL_TITAN_GENERAL_INT_STATUS_VPATH_TRAFFIC_INT(vpath_mask)) {
		hldev->header.traffic_intr_cnt++;
		ret = VXGE_HAL_TRAFFIC_INTERRUPT;
		vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, ret);
		return (ret);
	}

	hldev->header.not_traffic_intr_cnt++;

	if (vxge_os_unlikely(val64 &
	    VXGE_HAL_TITAN_GENERAL_INT_STATUS_VPATH_ALARM_INT)) {

		for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

			if (!(hldev->vpaths_deployed & mBIT(i)))
				continue;

			status = __hal_vpath_alarm_process(
			    &hldev->virtual_paths[i],
			    skip_alarms);

			if (status != VXGE_HAL_ERR_WRONG_IRQ)
				ret = status;

		}

	}
exit:
	vxge_hal_trace_log_device_irq(
	    "<==Error in  %s:%s:%d result = 0x%x general_int_status= 0x%llx",
	    __FILE__, __func__, __LINE__, ret, val64);
	return (ret);
}

/*
 * vxge_hal_device_continue_irq - Continue handling IRQ:	process	all
 *				completed descriptors.
 * @devh: HAL device handle.
 *
 * Process completed descriptors and unmask the	device interrupts.
 *
 * The vxge_hal_device_continue_irq() walks all open virtual paths
 * and calls upper-layer driver	(ULD) via supplied completion
 * callback.
 *
 * Note	that the vxge_hal_device_continue_irq is	part of	the _fast_ path.
 * To optimize the processing, the function does _not_ check for
 * errors and alarms.
 *
 * Returns: VXGE_HAL_OK.
 *
 * See also: vxge_hal_device_handle_irq()
 * vxge_hal_ring_rxd_next_completed(),
 * vxge_hal_fifo_txdl_next_completed(), vxge_hal_ring_callback_f {},
 * vxge_hal_fifo_callback_f {}.
 */
vxge_hal_status_e
vxge_hal_device_continue_irq(
    vxge_hal_device_h devh)
{
	u32 i;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d", __FILE__,
	    __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		(void) vxge_hal_vpath_continue_irq(
		    VXGE_HAL_VIRTUAL_PATH_HANDLE(&hldev->virtual_paths[i]));

	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_device_handle_irq - Handle device IRQ.
 * @devh: HAL device handle.
 * @skip_alarms: Do not clear the alarms
 *
 * Perform the complete	handling of the	line interrupt.	The function
 * performs two	calls.
 * First it uses vxge_hal_device_begin_irq() to check the reason for
 * the interrupt and mask the device interrupts.
 * Second, it calls	vxge_hal_device_continue_irq() to process all
 * completed descriptors and re-enable the interrupts.
 *
 * Returns: VXGE_HAL_OK - success;
 * VXGE_HAL_ERR_WRONG_IRQ - (shared) IRQ produced by other device.
 *
 * See also: vxge_hal_device_begin_irq(), vxge_hal_device_continue_irq().
 */
vxge_hal_status_e
vxge_hal_device_handle_irq(
    vxge_hal_device_h devh,
    u32 skip_alarms)
{
	u64 reason;
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT", \
	    skip_alarms = %d",
	    (ptr_t) devh, skip_alarms);

	vxge_hal_device_mask_all(hldev);

	status = vxge_hal_device_begin_irq(hldev, skip_alarms, &reason);
	if (vxge_os_unlikely(status == VXGE_HAL_ERR_WRONG_IRQ)) {
		vxge_hal_device_unmask_all(hldev);
		goto exit;
	}
	if (status == VXGE_HAL_TRAFFIC_INTERRUPT) {

		vxge_hal_device_clear_rx(hldev);

		status = vxge_hal_device_continue_irq(hldev);

		vxge_hal_device_clear_tx(hldev);

	}

	if (vxge_os_unlikely((status == VXGE_HAL_ERR_CRITICAL) && skip_alarms))
		/* ULD needs to unmask explicitely */
		goto exit;

	vxge_hal_device_unmask_all(hldev);

exit:
	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_device_handle_link_up_ind
 * @hldev: HAL device handle.
 *
 * Link up indication handler. The function is invoked by HAL when
 * X3100 indicates that the link is up for programmable amount of time.
 */
vxge_hal_status_e
__hal_device_handle_link_up_ind(__hal_device_t *hldev)
{
	vxge_assert(hldev);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	/*
	 * If the previous link state is not down, return.
	 */
	if (hldev->header.link_state == VXGE_HAL_LINK_UP) {
		vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	hldev->header.link_state = VXGE_HAL_LINK_UP;

	/* notify ULD */
	if (g_vxge_hal_driver->uld_callbacks.link_up) {
		g_vxge_hal_driver->uld_callbacks.link_up(
		    hldev,
		    hldev->header.upper_layer_data);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * __hal_device_handle_link_down_ind
 * @hldev: HAL device handle.
 *
 * Link down indication handler. The function is invoked by HAL when
 * X3100 indicates that the link is down.
 */
vxge_hal_status_e
__hal_device_handle_link_down_ind(__hal_device_t *hldev)
{
	vxge_assert(hldev);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	/*
	 * If the previous link state is not down, return.
	 */
	if (hldev->header.link_state == VXGE_HAL_LINK_DOWN) {
		vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	hldev->header.link_state = VXGE_HAL_LINK_DOWN;

	/* notify ULD */
	if (g_vxge_hal_driver->uld_callbacks.link_down) {
		g_vxge_hal_driver->uld_callbacks.link_down(
		    hldev,
		    hldev->header.upper_layer_data);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_device_link_state_test - Test the link state.
 * @devh: HAL device handle.
 *
 * Test link state.
 * Returns: link state.
 */
vxge_hal_device_link_state_e
vxge_hal_device_link_state_test(
    vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_device_link_state_e status = VXGE_HAL_LINK_NONE;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpath_assignments & mBIT(i)))
			continue;

		status =
		    __hal_vpath_link_state_test(&hldev->virtual_paths[i]);

		break;

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_link_state_poll - Poll for the link state.
 * @devh: HAL device handle.
 *
 * Get link state.
 * Returns: link state.
 */
vxge_hal_device_link_state_e
vxge_hal_device_link_state_poll(
    vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_device_link_state_e link_state = VXGE_HAL_LINK_NONE;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpath_assignments & mBIT(i)))
			continue;

		hldev->header.link_state = VXGE_HAL_LINK_NONE;

		link_state =
		    __hal_vpath_link_state_poll(&hldev->virtual_paths[i]);

		break;

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, link_state);
	return (link_state);
}

/*
 * vxge_hal_device_data_rate_poll - Poll for the data rate.
 * @devh: HAL device handle.
 *
 * Get data rate.
 * Returns: data rate.
 */
vxge_hal_device_data_rate_e
vxge_hal_device_data_rate_poll(
    vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_device_data_rate_e data_rate = VXGE_HAL_DATA_RATE_UNKNOWN;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		data_rate =
		    __hal_vpath_data_rate_poll(&hldev->virtual_paths[i]);

		break;

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, data_rate);
	return (data_rate);
}

/*
 * vxge_hal_device_lag_mode_get - Get Current LAG Mode
 * @devh: HAL device handle.
 *
 * Get Current LAG Mode
 */
vxge_hal_device_lag_mode_e
vxge_hal_device_lag_mode_get(
    vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_device_lag_mode_e lag_mode = VXGE_HAL_DEVICE_LAG_MODE_UNKNOWN;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		lag_mode =
		    __hal_vpath_lag_mode_get(&hldev->virtual_paths[i]);

		break;

	}

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, lag_mode);
	return (lag_mode);
}


/*
 * __hal_device_handle_error - Handle error
 * @hldev: HAL device
 * @vp_id: Vpath Id
 * @type: Error type. Please see vxge_hal_event_e {}
 *
 * Handle error.
 */
void
__hal_device_handle_error(
    __hal_device_t *hldev,
    u32 vp_id,
    vxge_hal_event_e type)
{
	vxge_assert(hldev);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq(
	    "hldev = 0x"VXGE_OS_STXFMT", vp_id = %d, type = %d",
	    (ptr_t) hldev, vp_id, type);

	switch (type) {
	default:
		vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_TYPE);
		return;
	case VXGE_HAL_EVENT_UNKNOWN:
		if (hldev->header.config.dump_on_unknown) {
			(void) vxge_hal_aux_device_dump(hldev);
		}
		break;
	case VXGE_HAL_EVENT_SERR:
		if (hldev->header.config.dump_on_serr) {
			(void) vxge_hal_aux_device_dump(hldev);
		}
		break;
	case VXGE_HAL_EVENT_CRITICAL:
	case VXGE_HAL_EVENT_SRPCIM_CRITICAL:
	case VXGE_HAL_EVENT_MRPCIM_CRITICAL:
		if (hldev->header.config.dump_on_critical) {
			(void) vxge_hal_aux_device_dump(hldev);
		}
		break;
	case VXGE_HAL_EVENT_ECCERR:
		if (hldev->header.config.dump_on_eccerr) {
			(void) vxge_hal_aux_device_dump(hldev);
		}
		break;
	case VXGE_HAL_EVENT_KDFCCTL:
		break;
	case VXGE_HAL_EVENT_DEVICE_RESET_START:
		break;
	case VXGE_HAL_EVENT_DEVICE_RESET_COMPLETE:
		break;
	case VXGE_HAL_EVENT_VPATH_RESET_START:
		break;
	case VXGE_HAL_EVENT_VPATH_RESET_COMPLETE:
		break;
	case VXGE_HAL_EVENT_SLOT_FREEZE:
		break;
	}


	/* notify ULD */
	if (g_vxge_hal_driver->uld_callbacks.crit_err) {
		g_vxge_hal_driver->uld_callbacks.crit_err(
		    (vxge_hal_device_h) hldev,
		    hldev->header.upper_layer_data,
		    type,
		    vp_id);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_mask_tx - Mask Tx interrupts.
 * @devh: HAL device.
 *
 * Mask	Tx device interrupts.
 *
 * See also: vxge_hal_device_unmask_tx(), vxge_hal_device_mask_rx(),
 * vxge_hal_device_clear_tx().
 */
void
vxge_hal_device_mask_tx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] != 0) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX],
		    &hldev->common_reg->tim_int_mask0);
	}

	if (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] != 0) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX],
		    &hldev->common_reg->tim_int_mask1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_clear_tx - Acknowledge (that is, clear) the
 * condition that has caused the TX	interrupt.
 * @devh: HAL device.
 *
 * Acknowledge (that is, clear)	the	condition that has caused
 * the Tx interrupt.
 * See also: vxge_hal_device_begin_irq(), vxge_hal_device_continue_irq(),
 * vxge_hal_device_clear_rx(), vxge_hal_device_mask_tx().
 */
void
vxge_hal_device_clear_tx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] != 0) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX],
		    &hldev->common_reg->tim_int_status0);
	}

	if (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] != 0) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX],
		    &hldev->common_reg->tim_int_status1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_unmask_tx - Unmask Tx	interrupts.
 * @devh: HAL device.
 *
 * Unmask Tx device interrupts.
 *
 * See also: vxge_hal_device_mask_tx(), vxge_hal_device_clear_tx().
 */
void
vxge_hal_device_unmask_tx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] != 0) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    ~(hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX]),
		    &hldev->common_reg->tim_int_mask0);
	}

	if (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] != 0) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    ~(hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX]),
		    &hldev->common_reg->tim_int_mask1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_mask_rx - Mask Rx	interrupts.
 * @devh: HAL device.
 *
 * Mask	Rx device interrupts.
 *
 * See also: vxge_hal_device_unmask_rx(), vxge_hal_device_mask_tx(),
 * vxge_hal_device_clear_rx().
 */
void
vxge_hal_device_mask_rx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX] != 0) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX],
		    &hldev->common_reg->tim_int_mask0);
	}

	if (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX] != 0) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX],
		    &hldev->common_reg->tim_int_mask1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_clear_rx - Acknowledge (that is, clear) the
 * condition that has caused the RX	interrupt.
 * @devh: HAL device.
 *
 * Acknowledge (that is, clear)	the	condition that has caused
 * the Rx interrupt.
 * See also: vxge_hal_device_begin_irq(), vxge_hal_device_continue_irq(),
 * vxge_hal_device_clear_tx(), vxge_hal_device_mask_rx().
 */
void
vxge_hal_device_clear_rx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX] != 0) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX],
		    &hldev->common_reg->tim_int_status0);
	}

	if (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX] != 0) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX],
		    &hldev->common_reg->tim_int_status1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_unmask_rx - Unmask Rx	interrupts.
 * @devh: HAL device.
 *
 * Unmask Rx device interrupts.
 *
 * See also: vxge_hal_device_mask_rx(), vxge_hal_device_clear_rx().
 */
void
vxge_hal_device_unmask_rx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX] != 0) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    ~(hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX]),
		    &hldev->common_reg->tim_int_mask0);
	}

	if (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX] != 0) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    ~(hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX]),
		    &hldev->common_reg->tim_int_mask1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_mask_tx_rx - Mask Tx and Rx interrupts.
 * @devh: HAL device.
 *
 * Mask Tx and Rx device interrupts.
 *
 * See also: vxge_hal_device_unmask_tx_rx(), vxge_hal_device_clear_tx_rx().
 */
void
vxge_hal_device_mask_tx_rx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if ((hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] != 0) ||
	    (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX] != 0)) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX]),
		    &hldev->common_reg->tim_int_mask0);
	}

	if ((hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] != 0) ||
	    (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX] != 0)) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX]),
		    &hldev->common_reg->tim_int_mask1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_clear_tx_rx - Acknowledge (that is, clear) the
 * condition that has caused the Tx and RX interrupt.
 * @devh: HAL device.
 *
 * Acknowledge (that is, clear)	the	condition that has caused
 * the Tx and Rx interrupt.
 * See also: vxge_hal_device_begin_irq(), vxge_hal_device_continue_irq(),
 * vxge_hal_device_mask_tx_rx(), vxge_hal_device_unmask_tx_rx().
 */
void
vxge_hal_device_clear_tx_rx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if ((hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] != 0) ||
	    (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX] != 0)) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX]),
		    &hldev->common_reg->tim_int_status0);
	}

	if ((hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] != 0) ||
	    (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX] != 0)) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX]),
		    &hldev->common_reg->tim_int_status1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_unmask_tx_rx - Unmask Tx and Rx interrupts.
 * @devh: HAL device.
 *
 * Unmask Rx device interrupts.
 *
 * See also: vxge_hal_device_mask_tx_rx(), vxge_hal_device_clear_tx_rx().
 */
void
vxge_hal_device_unmask_tx_rx(
    vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_device_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device_irq("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if ((hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] != 0) ||
	    (hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX] != 0)) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    ~(hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask0[VXGE_HAL_VPATH_INTR_RX]),
		    &hldev->common_reg->tim_int_mask0);
	}

	if ((hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] != 0) ||
	    (hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX] != 0)) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    ~(hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_TX] |
		    hldev->tim_int_mask1[VXGE_HAL_VPATH_INTR_RX]),
		    &hldev->common_reg->tim_int_mask1);
	}

	vxge_hal_trace_log_device_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_hw_info_get - Get the hw information
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @bar0: Address of BAR0 in PCI config
 * @hw_info: Buffer to return vxge_hal_device_hw_info_t {} structure
 *
 * Returns the vpath mask that has the bits set for each vpath allocated
 * for the driver, FW version information and the first mac addresse for
 * each vpath
 */
vxge_hal_status_e
vxge_hal_device_hw_info_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u8 *bar0,
    vxge_hal_device_hw_info_t *hw_info)
{
	u32 i;
	u64 val64;
	vxge_hal_legacy_reg_t *legacy_reg;
	vxge_hal_toc_reg_t *toc_reg;
	vxge_hal_mrpcim_reg_t *mrpcim_reg;
	vxge_hal_common_reg_t *common_reg;
	vxge_hal_vpath_reg_t *vpath_reg;
	vxge_hal_vpmgmt_reg_t *vpmgmt_reg;
	vxge_hal_status_e status;

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "bar0 = 0x"VXGE_OS_STXFMT", hw_info = 0x"VXGE_OS_STXFMT,
	    (ptr_t) pdev, (ptr_t) regh0, (ptr_t) bar0, (ptr_t) hw_info);

	vxge_assert((bar0 != NULL) && (hw_info != NULL));

	vxge_os_memzero(hw_info, sizeof(vxge_hal_device_hw_info_t));

	legacy_reg = (vxge_hal_legacy_reg_t *)
	    vxge_hal_device_get_legacy_reg(pdev, regh0, bar0);

	status = __hal_legacy_swapper_set(pdev, regh0, legacy_reg);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &legacy_reg->toc_first_pointer);

	toc_reg = (vxge_hal_toc_reg_t *) ((void *) (bar0 + val64));

	val64 =
	    vxge_os_pio_mem_read64(pdev, regh0, &toc_reg->toc_common_pointer);

	common_reg = (vxge_hal_common_reg_t *) ((void *) (bar0 + val64));

	status = vxge_hal_device_register_poll(pdev, regh0,
	    &common_reg->vpath_rst_in_prog, 0,
	    VXGE_HAL_VPATH_RST_IN_PROG_VPATH_RST_IN_PROG(0x1ffff),
	    VXGE_HAL_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	hw_info->vpath_mask = vxge_os_pio_mem_read64(pdev, regh0,
	    &common_reg->vpath_assignments);

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &common_reg->host_type_assignments);

	hw_info->host_type = (u32)
	    VXGE_HAL_HOST_TYPE_ASSIGNMENTS_GET_HOST_TYPE_ASSIGNMENTS(val64);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!((hw_info->vpath_mask) & mBIT(i)))
			continue;

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &toc_reg->toc_vpmgmt_pointer[i]);

		vpmgmt_reg = (vxge_hal_vpmgmt_reg_t *)
		    ((void *) (bar0 + val64));

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &vpmgmt_reg->vpath_to_func_map_cfg1);
		hw_info->func_id = (u32)
		    VXGE_HAL_VPATH_TO_FUNC_MAP_CFG1_GET_CFG1(
		    val64);

		if (__hal_device_access_rights_get(hw_info->host_type,
		    hw_info->func_id) & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM) {

			val64 = vxge_os_pio_mem_read64(pdev, regh0,
			    &toc_reg->toc_mrpcim_pointer);

			mrpcim_reg = (vxge_hal_mrpcim_reg_t *)
			    ((void *) (bar0 + val64));

			vxge_os_pio_mem_write64(pdev, regh0,
			    0,
			    &mrpcim_reg->xgmac_gen_fw_memo_mask);
			vxge_os_wmb();
		}

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &toc_reg->toc_vpath_pointer[i]);

		vpath_reg = (vxge_hal_vpath_reg_t *) ((void *) (bar0 + val64));

		(void) __hal_vpath_fw_flash_ver_get(pdev, regh0, i, vpath_reg,
		    &hw_info->fw_version,
		    &hw_info->fw_date,
		    &hw_info->flash_version,
		    &hw_info->flash_date);

		(void) __hal_vpath_card_info_get(pdev, regh0, i, vpath_reg,
		    hw_info->serial_number,
		    hw_info->part_number,
		    hw_info->product_description);

		(void) __hal_vpath_pmd_info_get(pdev, regh0, i, vpath_reg,
		    &hw_info->ports,
		    &hw_info->pmd_port0,
		    &hw_info->pmd_port1);

		hw_info->function_mode =
		    __hal_vpath_pci_func_mode_get(pdev, regh0, i, vpath_reg);

		break;
	}

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!((hw_info->vpath_mask) & mBIT(i)))
			continue;

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &toc_reg->toc_vpath_pointer[i]);

		vpath_reg = (vxge_hal_vpath_reg_t *) ((void *) (bar0 + val64));

		status = __hal_vpath_hw_addr_get(pdev, regh0, i, vpath_reg,
		    hw_info->mac_addrs[i], hw_info->mac_addr_masks[i]);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);

		}

	}

	vxge_hal_trace_log_driver("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_device_initialize - Initialize X3100 device.
 * @hldev: HAL device handle.
 * @attr: pointer to vxge_hal_device_attr_t structure
 * @device_config: Configuration to be _applied_ to the device,
 *		For the X3100 configuration "knobs" please
 *		refer to vxge_hal_device_config_t and X3100
 *		User Guide.
 *
 * Initialize X3100 device. Note that all the arguments of this public API
 * are 'IN', including @hldev. Upper-layer driver (ULD) cooperates with
 * OS to find new X3100 device, locate its PCI and memory spaces.
 *
 * When done, the ULD allocates sizeof(__hal_device_t) bytes for HAL
 * to enable the latter to perform X3100 hardware initialization.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED - Driver is not initialized.
 * VXGE_HAL_ERR_BAD_DEVICE_CONFIG - Device configuration params are not
 * valid.
 * VXGE_HAL_ERR_OUT_OF_MEMORY - Memory allocation failed.
 * VXGE_HAL_ERR_BAD_SUBSYSTEM_ID - Device subsystem id is invalid.
 * VXGE_HAL_ERR_INVALID_MAC_ADDRESS - Device mac address in not valid.
 * VXGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to retrieve the mac
 * address within the time(timeout) or TTI/RTI initialization failed.
 * VXGE_HAL_ERR_SWAPPER_CTRL - Failed to configure swapper control.
 *
 * See also: __hal_device_terminate(), vxge_hal_status_e {}
 * vxge_hal_device_attr_t {}.
 */
vxge_hal_status_e
vxge_hal_device_initialize(
    vxge_hal_device_h *devh,
    vxge_hal_device_attr_t *attr,
    vxge_hal_device_config_t *device_config)
{
	u32 i;
	u32 nblocks = 0;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert((devh != NULL) &&
	    (attr != NULL) && (device_config != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "devh = 0x"VXGE_OS_STXFMT", attr = 0x"VXGE_OS_STXFMT", "
	    "device_config = 0x"VXGE_OS_STXFMT, (ptr_t) devh, (ptr_t) attr,
	    (ptr_t) device_config);

	/* sanity check */
	if (g_vxge_hal_driver == NULL ||
	    !g_vxge_hal_driver->is_initialized) {
		vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED);
		return (VXGE_HAL_ERR_DRIVER_NOT_INITIALIZED);
	}

	status = __hal_device_config_check(device_config);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	hldev = (__hal_device_t *) vxge_os_malloc(attr->pdev,
	    sizeof(__hal_device_t));

	if (hldev == NULL) {
		vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	vxge_os_memzero(hldev, sizeof(__hal_device_t));

	hldev->header.magic = VXGE_HAL_DEVICE_MAGIC;

	__hal_channel_init_pending_list(hldev);

	vxge_hal_device_debug_set(hldev,
	    device_config->debug_level,
	    device_config->debug_mask);

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
	hldev->trace_buf.size = device_config->tracebuf_size;
	hldev->trace_buf.data =
	    (u8 *) vxge_os_malloc(attr->pdev, hldev->trace_buf.size);
	if (hldev->trace_buf.data == NULL) {
		vxge_os_printf("cannot allocate trace buffer!\n");
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}
	hldev->trace_buf.offset = 0;
	hldev->trace_buf.wrapped_count = 0;
	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);
#endif

	vxge_hal_info_log_device("device 0x"VXGE_OS_STXFMT" is initializing",
	    (ptr_t) hldev);

	/* apply config */
	vxge_os_memcpy(&hldev->header.config, device_config,
	    sizeof(vxge_hal_device_config_t));

	hldev->header.regh0 = attr->regh0;
	hldev->header.regh1 = attr->regh1;
	hldev->header.regh2 = attr->regh2;
	hldev->header.bar0 = attr->bar0;
	hldev->header.bar1 = attr->bar1;
	hldev->header.bar2 = attr->bar2;
	hldev->header.pdev = attr->pdev;
	hldev->header.irqh = attr->irqh;
	hldev->header.cfgh = attr->cfgh;

	if ((status = __hal_device_reg_addr_get(hldev)) != VXGE_HAL_OK) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		vxge_hal_device_terminate(hldev);
		return (status);
	}

	__hal_device_id_get(hldev);

	__hal_device_host_info_get(hldev);


	nblocks += 1;		/* For MRPCIM stats */

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpath_assignments & mBIT(i)))
			continue;

		if (device_config->vp_config[i].ring.enable ==
		    VXGE_HAL_RING_ENABLE) {
			nblocks +=
			    (device_config->vp_config[i].ring.ring_length +
			    vxge_hal_ring_rxds_per_block_get(
			    device_config->vp_config[i].ring.buffer_mode) - 1) /
			    vxge_hal_ring_rxds_per_block_get(
			    device_config->vp_config[i].ring.buffer_mode);
		}

		if ((device_config->vp_config[i].fifo.enable ==
		    VXGE_HAL_FIFO_ENABLE) &&
		    ((device_config->vp_config[i].fifo.max_frags *
		    sizeof(vxge_hal_fifo_txd_t)) <=
		    VXGE_OS_HOST_PAGE_SIZE)) {
			nblocks +=
			    ((device_config->vp_config[i].fifo.fifo_length *
			    sizeof(vxge_hal_fifo_txd_t) *
			    device_config->vp_config[i].fifo.max_frags) +
			    VXGE_OS_HOST_PAGE_SIZE - 1) /
			    VXGE_OS_HOST_PAGE_SIZE;
		}


		nblocks += 1;	/* For vpath stats */

	}

	if (__hal_blockpool_create(hldev,
	    &hldev->block_pool,
	    device_config->dma_blockpool_initial + nblocks,
	    device_config->dma_blockpool_incr,
	    device_config->dma_blockpool_min,
	    device_config->dma_blockpool_max + nblocks) != VXGE_HAL_OK) {
		vxge_hal_info_log_device("%s:__hal_blockpool_create failed",
		    __func__);
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_MEMORY);
		vxge_hal_device_terminate(hldev);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}


	status = __hal_device_hw_initialize(hldev);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		vxge_hal_device_terminate(hldev);
		return (status);
	}

	hldev->dump_buf = (char *) vxge_os_malloc(hldev->header.pdev,
	    VXGE_HAL_DUMP_BUF_SIZE);
	if (hldev->dump_buf == NULL) {
		vxge_hal_info_log_device("%s:vxge_os_malloc failed ",
		    __func__);
		vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_OUT_OF_MEMORY);
		vxge_hal_device_terminate(hldev);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	hldev->header.is_initialized = 1;

	*devh = hldev;

	vxge_hal_trace_log_device("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_device_terminate - Terminate X3100 device.
 * @devh: HAL device handle.
 *
 * Terminate HAL device.
 *
 * See also: vxge_hal_device_initialize().
 */
void
vxge_hal_device_terminate(vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(g_vxge_hal_driver != NULL);
	vxge_assert(hldev != NULL);
	vxge_assert(hldev->header.magic == VXGE_HAL_DEVICE_MAGIC);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	hldev->header.terminating = 1;
	hldev->header.is_initialized = 0;
	hldev->in_poll = 0;
	hldev->header.magic = VXGE_HAL_DEVICE_DEAD;

	if (hldev->dump_buf) {
		vxge_os_free(hldev->header.pdev, hldev->dump_buf,
		    VXGE_HAL_DUMP_BUF_SIZE);
		hldev->dump_buf = NULL;
	}

	if (hldev->srpcim != NULL)
		(void) __hal_srpcim_terminate(hldev);

	if (hldev->mrpcim != NULL)
		(void) __hal_mrpcim_terminate(hldev);

	__hal_channel_destroy_pending_list(hldev);


	__hal_blockpool_destroy(&hldev->block_pool);

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
	if (hldev->trace_buf.size) {
		vxge_os_free(NULL,
		    hldev->trace_buf.data,
		    hldev->trace_buf.size);
	}
#endif

	vxge_os_free(hldev->header.pdev, hldev, sizeof(__hal_device_t));

	vxge_hal_trace_log_driver("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_device_enable - Enable device.
 * @devh: HAL device handle.
 *
 * Enable the specified device: bring up the link/interface.
 *
 */
vxge_hal_status_e
vxge_hal_device_enable(
    vxge_hal_device_h devh)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!hldev->hw_is_initialized) {

		status = __hal_device_hw_initialize(hldev);
		if (status != VXGE_HAL_OK) {
			vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}
	}

	__hal_device_bus_master_enable(hldev);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_disable - Disable X3100 adapter.
 * @devh: HAL device handle.
 *
 * Disable this device. To gracefully reset the adapter, the host should:
 *
 *	- call vxge_hal_device_disable();
 *
 *	- call vxge_hal_device_intr_disable();
 *
 *	- do some work (error recovery, change mtu, reset, etc);
 *
 *	- call vxge_hal_device_enable();
 *
 *	- call vxge_hal_device_intr_enable().
 *
 * Note: Disabling the device does _not_ include disabling of interrupts.
 * After disabling the device stops receiving new frames but those frames
 * that were already in the pipe will keep coming for some few milliseconds.
 *
 *
 */
vxge_hal_status_e
vxge_hal_device_disable(
    vxge_hal_device_h devh)
{
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(devh != NULL);

#if (VXGE_COMPONENT_HAL_DEVICE & VXGE_DEBUG_MODULE_MASK)

	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	vxge_hal_trace_log_device("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
#endif

	return (status);
}

/*
 * vxge_hal_device_hw_stats_enable - Enable device h/w statistics.
 * @devh: HAL Device.
 *
 * Enable the DMA vpath statistics for the device. The function is to be called
 * to re-enable the adapter to update stats into the host memory
 *
 * See also: vxge_hal_device_hw_stats_disable()
 */
vxge_hal_status_e
vxge_hal_device_hw_stats_enable(
    vxge_hal_device_h devh)
{
	u32 i;
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->stats_cfg0);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		vxge_os_memcpy(hldev->virtual_paths[i].hw_stats_sav,
		    hldev->virtual_paths[i].hw_stats,
		    sizeof(vxge_hal_vpath_stats_hw_info_t));
		if (hldev->header.config.stats_read_method ==
		    VXGE_HAL_STATS_READ_METHOD_DMA) {
			val64 |=
			    VXGE_HAL_STATS_CFG0_STATS_ENABLE((1 << (16 - i)));
		} else {
			status = __hal_vpath_hw_stats_get(
			    &hldev->virtual_paths[i],
			    hldev->virtual_paths[i].hw_stats);
		}

	}

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->stats_cfg0);

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_hw_stats_disable - Disable device h/w statistics.
 * @devh: HAL Device.
 *
 * Enable the DMA vpath statistics for the device. The function is to be called
 * to disable the adapter to update stats into the host memory. This function
 * is not needed to be called, normally.
 *
 * See also: vxge_hal_device_hw_stats_enable()
 */
vxge_hal_status_e
vxge_hal_device_hw_stats_disable(
    vxge_hal_device_h devh)
{
	u32 i;
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->stats_cfg0);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		val64 &= ~VXGE_HAL_STATS_CFG0_STATS_ENABLE((1 << (16 - i)));

	}

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->stats_cfg0);

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_hw_stats_get - Get the device hw statistics.
 * @devh: HAL Device.
 * @hw_stats: Hardware stats
 *
 * Returns the vpath h/w stats for the device.
 *
 * See also: vxge_hal_device_hw_stats_enable(),
 * vxge_hal_device_hw_stats_disable()
 */
vxge_hal_status_e
vxge_hal_device_hw_stats_get(
    vxge_hal_device_h devh,
    vxge_hal_device_stats_hw_info_t *hw_stats)
{
	u32 i;
	u64 val64 = 0;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (hw_stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "devh = 0x"VXGE_OS_STXFMT", hw_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) hw_stats);

	if (hldev->header.config.stats_read_method ==
	    VXGE_HAL_STATS_READ_METHOD_DMA) {

		for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

			if (!(hldev->vpaths_deployed & mBIT(i)))
				continue;

			val64 |=
			    VXGE_HAL_STATS_CFG0_STATS_ENABLE((1 << (16 - i)));

		}

		status = vxge_hal_device_register_poll(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->stats_cfg0,
		    0,
		    val64,
		    hldev->header.config.device_poll_millis);

	}

	if (status == VXGE_HAL_OK) {
		vxge_os_memcpy(hw_stats,
		    &hldev->stats.hw_dev_info_stats,
		    sizeof(vxge_hal_device_stats_hw_info_t));
	}

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_sw_stats_get - Get the device sw statistics.
 * @devh: HAL Device.
 * @sw_stats: Software stats
 *
 * Returns the vpath s/w stats for the device.
 *
 * See also: vxge_hal_device_hw_stats_get()
 */
vxge_hal_status_e
vxge_hal_device_sw_stats_get(
    vxge_hal_device_h devh,
    vxge_hal_device_stats_sw_info_t *sw_stats)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (sw_stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "devh = 0x"VXGE_OS_STXFMT", sw_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) sw_stats);

	vxge_os_memcpy(sw_stats,
	    &hldev->stats.sw_dev_info_stats,
	    sizeof(vxge_hal_device_stats_sw_info_t));

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_stats_get - Get the device statistics.
 * @devh: HAL Device.
 * @stats: Device stats
 *
 * Returns the device stats for the device.
 *
 * See also: vxge_hal_device_hw_stats_get(), vxge_hal_device_sw_stats_get()
 */
vxge_hal_status_e
vxge_hal_device_stats_get(
    vxge_hal_device_h devh,
    vxge_hal_device_stats_t *stats)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "devh = 0x"VXGE_OS_STXFMT", stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) stats);

	vxge_os_memcpy(stats,
	    &hldev->stats,
	    sizeof(vxge_hal_device_stats_t));

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_device_xmac_stats_get - Get the Device XMAC Statistics
 * @devh: HAL device handle.
 * @xmac_stats: Buffer to return XMAC Statistics.
 *
 * Get the XMAC Statistics
 *
 */
vxge_hal_status_e
vxge_hal_device_xmac_stats_get(vxge_hal_device_h devh,
    vxge_hal_device_xmac_stats_t *xmac_stats)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	u32 i;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (xmac_stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "devh = 0x"VXGE_OS_STXFMT", xmac_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) xmac_stats);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {


		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		status = __hal_vpath_xmac_tx_stats_get(&hldev->virtual_paths[i],
		    &xmac_stats->vpath_tx_stats[i]);

		if (status != VXGE_HAL_OK) {
			vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		status = __hal_vpath_xmac_rx_stats_get(&hldev->virtual_paths[i],
		    &xmac_stats->vpath_rx_stats[i]);

		if (status != VXGE_HAL_OK) {
			vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

	}

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)

/*
 * vxge_hal_device_trace_write - Write the trace from the given buffer into
 *				 circular trace buffer
 * @devh: HAL device handle.
 * @trace_buf: Buffer containing the trace.
 * @trace_len: Length of the trace in the buffer
 *
 * Writes the trace from the given buffer into the circular trace buffer
 *
 */
void
vxge_hal_device_trace_write(vxge_hal_device_h devh,
    u8 *trace_buf,
    u32 trace_len)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;
	u32 offset;

	if (hldev == NULL)
		return;

	offset = hldev->trace_buf.offset;

	if (trace_len > 1) {

		u32 leftsize = hldev->trace_buf.size - offset;

		if (trace_len > leftsize) {
			vxge_os_memzero(hldev->trace_buf.data + offset,
			    leftsize);
			offset = 0;
			hldev->trace_buf.wrapped_count++;
		}

		vxge_os_memcpy(hldev->trace_buf.data + offset,
		    trace_buf, trace_len);
		offset += trace_len;
		hldev->trace_buf.offset = offset;

	}
}

/*
 * vxge_hal_device_trace_dump - Dump the trace buffer.
 * @devh: HAL device handle.
 *
 * Dump the trace buffer contents.
 */
void
vxge_hal_device_trace_dump(vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;
	u32 offset, i = 0;

	if (hldev == NULL)
		return;

	offset = hldev->trace_buf.offset;

	vxge_os_printf("################ Trace dump Begin ###############\n");

	if (hldev->trace_buf.wrapped_count) {
		for (i = hldev->trace_buf.offset;
		    i < hldev->trace_buf.size; i += offset) {
			if (*(hldev->trace_buf.data + i))
				vxge_os_printf(hldev->trace_buf.data + i);
			offset = vxge_os_strlen(hldev->trace_buf.data + i) + 1;
		}
	}

	for (i = 0; i < hldev->trace_buf.offset; i += offset) {
		if (*(hldev->trace_buf.data + i))
			vxge_os_printf(hldev->trace_buf.data + i);
		offset = vxge_os_strlen(hldev->trace_buf.data + i) + 1;
	}

	vxge_os_printf("################ Trace dump End ###############\n");

}

/*
 * vxge_hal_device_trace_read - Read trace buffer contents.
 * @devh: HAL device handle.
 * @buffer: Buffer to store the trace buffer contents.
 * @buf_size: Size of the buffer.
 * @read_length: Size of the valid data in the buffer.
 *
 * Read  HAL trace buffer contents starting from the offset
 * up to the size of the buffer or till EOF is reached.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_EOF_TRACE_BUF - No more data in the trace buffer.
 *
 */
vxge_hal_status_e
vxge_hal_device_trace_read(vxge_hal_device_h devh,
    char *buffer,
    unsigned buf_size,
    unsigned *read_length)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;
	u32 offset, i = 0, buf_off = 0;

	*read_length = 0;
	*buffer = 0;

	if (hldev == NULL)
		return (VXGE_HAL_FAIL);

	offset = hldev->trace_buf.offset;

	if (hldev->trace_buf.wrapped_count) {
		for (i = hldev->trace_buf.offset;
		    i < hldev->trace_buf.size; i += offset) {
			if (*(hldev->trace_buf.data + i)) {
				vxge_os_sprintf(buffer + buf_off, "%s\n",
				    hldev->trace_buf.data + i);
				buf_off += vxge_os_strlen(
				    hldev->trace_buf.data + i) + 1;
				if (buf_off > buf_size)
					return (VXGE_HAL_ERR_OUT_OF_MEMORY);
			}
			offset = vxge_os_strlen(hldev->trace_buf.data + i) + 1;
		}
	}

	for (i = 0; i < hldev->trace_buf.offset; i += offset) {
		if (*(hldev->trace_buf.data + i)) {
			vxge_os_sprintf(buffer + buf_off, "%s\n",
			    hldev->trace_buf.data + i);
			buf_off += vxge_os_strlen(
			    hldev->trace_buf.data + i) + 1;
			if (buf_off > buf_size)
				return (VXGE_HAL_ERR_OUT_OF_MEMORY);
		}
		offset = vxge_os_strlen(hldev->trace_buf.data + i) + 1;
	}

	*read_length = buf_off;
	*(buffer + buf_off + 1) = 0;

	return (VXGE_HAL_OK);
}

#endif

/*
 * vxge_hal_device_debug_set - Set the debug module, level and timestamp
 * @devh: Hal device object
 * @level: Debug level as defined in enum vxge_debug_level_e
 * @module masks: An or value of component masks as defined in vxge_debug.h
 *
 * This routine is used to dynamically change the debug output
 */
void
vxge_hal_device_debug_set(
    vxge_hal_device_h devh,
    vxge_debug_level_e level,
    u32 mask)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	hldev->header.debug_module_mask = mask;
	hldev->header.debug_level = level;

	hldev->d_trace_mask = 0;
	hldev->d_info_mask = 0;
	hldev->d_err_mask = 0;

	switch (level) {
	case VXGE_TRACE:
		hldev->d_trace_mask = mask;
		/* FALLTHROUGH */

	case VXGE_INFO:
		hldev->d_info_mask = mask;
		/* FALLTHROUGH */

	case VXGE_ERR:
		hldev->d_err_mask = mask;
		/* FALLTHROUGH */

	default:
		break;
	}
}

/*
 * vxge_hal_device_flick_link_led - Flick (blink) link LED.
 * @devh: HAL device handle.
 * @port : Port number 0, or 1
 * @on_off: TRUE if flickering to be on, FALSE to be off
 *
 * Flicker the link LED.
 */
vxge_hal_status_e
vxge_hal_device_flick_link_led(vxge_hal_device_h devh, u32 port, u32 on_off)
{
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT
	    ", port = %d, on_off = %d", (ptr_t) devh, port, on_off);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_DEVICE);

		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	status = __hal_vpath_flick_link_led(hldev,
	    hldev->first_vp_id, port, on_off);

	vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_device_getpause_data -Pause frame frame generation and reception.
 * @devh: HAL device handle.
 * @port : Port number 0, 1, or 2
 * @tx : A field to return the pause generation capability of the NIC.
 * @rx : A field to return the pause reception capability of the NIC.
 *
 * Returns the Pause frame generation and reception capability of the NIC.
 * Return value:
 * status
 */
vxge_hal_status_e
vxge_hal_device_getpause_data(
    vxge_hal_device_h devh,
    u32 port,
    u32 *tx,
    u32 *rx)
{
	u32 i;
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_ERR_VPATH_NOT_AVAILABLE;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_device("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_device("devh = 0x"VXGE_OS_STXFMT", "
	    "port = %d, tx = 0x"VXGE_OS_STXFMT", "
	    "rx = 0x"VXGE_OS_STXFMT, (ptr_t) devh, port, (ptr_t) tx,
	    (ptr_t) rx);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (port >= VXGE_HAL_MAC_MAX_PORTS) {
		vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_PORT);
		return (VXGE_HAL_ERR_INVALID_PORT);
	}

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpath_assignments & mBIT(i)))
			continue;

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->vpmgmt_reg[i]->
		    rxmac_pause_cfg_port_vpmgmt_clone[port]);

		if (val64 & VXGE_HAL_RXMAC_PAUSE_CFG_PORT_VPMGMT_CLONE_GEN_EN)
			*tx = 1;

		if (val64 & VXGE_HAL_RXMAC_PAUSE_CFG_PORT_VPMGMT_CLONE_RCV_EN)
			*rx = 1;

		status = VXGE_HAL_OK;

		break;
	}

	vxge_hal_trace_log_device("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

vxge_hal_status_e
vxge_hal_device_is_privileged(u32 host_type, u32 func_id)
{
	u32 access_rights;
	vxge_hal_status_e status = VXGE_HAL_ERR_PRIVILAGED_OPEARATION;

	access_rights = __hal_device_access_rights_get(host_type, func_id);

	if (access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)
		status = VXGE_HAL_OK;

	return (status);
}
