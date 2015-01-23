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
 * vxge_hal_mrpcim_serial_number_get - Returns the serial number
 * @devh: HAL device handle.
 *
 * Return the serial number
 */
const u8 *
vxge_hal_mrpcim_serial_number_get(vxge_hal_device_h devh)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (NULL);
	}

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (hldev->mrpcim->vpd_data.serial_num);
}

/*
 * vxge_hal_mrpcim_vpath_map_get - Returns the assigned vpaths map
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @bar0: Address of BAR0 in PCI config
 * @func: Function Number
 *
 * Returns the assigned vpaths map
 */
u64
vxge_hal_mrpcim_vpath_map_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u8 *bar0,
    u32 func)
{
	u64 val64;
	vxge_hal_legacy_reg_t *legacy_reg;
	vxge_hal_toc_reg_t *toc_reg;
	vxge_hal_vpath_reg_t *vpath_reg;

	vxge_assert(bar0 != NULL);

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "bar0 = 0x"VXGE_OS_STXFMT", func = %d",
	    (ptr_t) pdev, (ptr_t) regh0, (ptr_t) bar0, func);

	legacy_reg = (vxge_hal_legacy_reg_t *)
	    vxge_hal_device_get_legacy_reg(pdev, regh0, bar0);

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &legacy_reg->toc_first_pointer);

	toc_reg = (vxge_hal_toc_reg_t *) ((void *)(bar0 + val64));

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &toc_reg->toc_vpath_pointer[0]);

	vpath_reg = (vxge_hal_vpath_reg_t *) ((void *)(bar0 + val64));

	val64 = __hal_vpath_vpath_map_get(pdev, regh0, 0, 0, func, vpath_reg);

	vxge_hal_trace_log_driver("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (val64);
}

/*
 * vxge_hal_mrpcim_pcie_func_mode_set - Set PCI-E function mode
 * @devh: Device Handle.
 * @func_mode: PCI-E func mode. Please see vxge_hal_pcie_function_mode_e{}
 *
 * Set PCI-E function mode.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_pcie_func_mode_set(
    vxge_hal_device_h devh,
    vxge_hal_pcie_function_mode_e func_mode)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;
	u32 fmode;
	vxge_hal_status_e status;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver("devh = 0x"VXGE_OS_STXFMT
	    ",func_mode = %d", (ptr_t) devh, func_mode);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	switch (func_mode) {
	case VXGE_HAL_PCIE_FUNC_MODE_SF1_VP17:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_SF1_VP17;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_MF8_VP2:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_MF8_VP2;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_SR17_VP1:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_SR17_VP1;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_MR17_VP1:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_MR17_VP1;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_MR8_VP2:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_MR8_VP2;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_MF17_VP1:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_MF17_VP1;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_SR8_VP2:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_SR8_VP2;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_SR4_VP4:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_SR4_VP4;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_MF2_VP8:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_MF2_VP8;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_MF4_VP4:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_MF4_VP4;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_MR4_VP4:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_MR4_VP4;
		break;
	case VXGE_HAL_PCIE_FUNC_MODE_MF8P_VP2:
		fmode =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE_MF8P_VP2;
		break;
	default:
		vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_TYPE);

		return (VXGE_HAL_ERR_INVALID_TYPE);
	}

	status = __hal_vpath_pcie_func_mode_set(hldev, hldev->first_vp_id, fmode);

	vxge_hal_trace_log_driver("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (status);

}

/*
 * vxge_hal_mrpcim_fw_upgrade - Upgrade firmware
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle (Solaris), or simply PCI device @pdev
 *	(Linux and the rest.)
 * @bar0: Address of BAR0 in PCI config
 * @buffer: Buffer containing F/W image
 * @length: F/W image length
 *
 * Upgrade firmware
 */
vxge_hal_status_e
vxge_hal_mrpcim_fw_upgrade(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u8 *bar0,
    u8 *buffer,
    u32 length)
{
	u64 val64, vpath_mask;
	u32 host_type, func_id, i;
	vxge_hal_legacy_reg_t *legacy_reg;
	vxge_hal_toc_reg_t *toc_reg;
	vxge_hal_mrpcim_reg_t *mrpcim_reg;
	vxge_hal_common_reg_t *common_reg;
	vxge_hal_vpmgmt_reg_t *vpmgmt_reg;
	vxge_hal_vpath_reg_t *vpath_reg;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((bar0 != NULL) && (buffer != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "bar0 = 0x"VXGE_OS_STXFMT", buffer = 0x"VXGE_OS_STXFMT", "
	    "length = %d", (ptr_t) pdev, (ptr_t) regh0, (ptr_t) bar0,
	    (ptr_t) buffer, length);

	legacy_reg = (vxge_hal_legacy_reg_t *)
	    vxge_hal_device_get_legacy_reg(pdev, regh0, bar0);

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &legacy_reg->toc_first_pointer);

	toc_reg = (vxge_hal_toc_reg_t *) ((void *)(bar0 + val64));

	val64 =
	    vxge_os_pio_mem_read64(pdev, regh0, &toc_reg->toc_common_pointer);

	common_reg = (vxge_hal_common_reg_t *) ((void *)(bar0 + val64));

	vpath_mask = vxge_os_pio_mem_read64(pdev, regh0,
	    &common_reg->vpath_assignments);

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &common_reg->host_type_assignments);

	host_type = (u32)
	    VXGE_HAL_HOST_TYPE_ASSIGNMENTS_GET_HOST_TYPE_ASSIGNMENTS(val64);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!((vpath_mask) & mBIT(i)))
			continue;

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &toc_reg->toc_vpmgmt_pointer[i]);

		vpmgmt_reg = (vxge_hal_vpmgmt_reg_t *) ((void *)(bar0 + val64));

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &vpmgmt_reg->vpath_to_func_map_cfg1);

		func_id = (u32) VXGE_HAL_VPATH_TO_FUNC_MAP_CFG1_GET_CFG1(val64);

		if (!(__hal_device_access_rights_get(host_type, func_id) &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {

			vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

			return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		}

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &toc_reg->toc_vpath_pointer[i]);

		vpath_reg = (vxge_hal_vpath_reg_t *) ((void *)(bar0 + val64));

		status = __hal_vpath_fw_upgrade(pdev, regh0,
		    i, vpath_reg, buffer, length);

		break;
	}

	if (status == VXGE_HAL_OK) {
		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &toc_reg->toc_mrpcim_pointer);

		mrpcim_reg = (vxge_hal_mrpcim_reg_t *) ((void *)(bar0 + val64));

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &mrpcim_reg->sw_reset_cfg1);

		val64 |= VXGE_HAL_SW_RESET_CFG1_TYPE;

		vxge_os_pio_mem_write64(pdev, regh0,
		    val64,
		    &mrpcim_reg->sw_reset_cfg1);

		vxge_os_pio_mem_write64(pdev, regh0,
		    VXGE_HAL_PF_SW_RESET_PF_SW_RESET(
		    VXGE_HAL_PF_SW_RESET_COMMAND),
		    &mrpcim_reg->bf_sw_reset);

		vxge_os_mdelay(100);
	}

	vxge_hal_trace_log_driver("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_mrpcim_vpath_qos_set - Set the priority, Guaranteed and maximum
 *				 bandwidth for a vpath.
 * @devh: HAL device handle.
 * @vp_id: Vpath Id.
 * @priority: Priority
 * @min_bandwidth: Minimum Bandwidth
 * @max_bandwidth: Maximum Bandwidth
 *
 * Set the Guaranteed and maximum bandwidth for a given vpath
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_vpath_qos_set(
    vxge_hal_device_h devh,
    u32 vp_id,
    u32 priority,
    u32 min_bandwidth,
    u32 max_bandwidth)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_vpath_qos_config_t config;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT", vp_id = %d, "
	    "priority = %d, min_bandwidth = %d, max_bandwidth = %d",
	    (ptr_t) devh, vp_id, priority, min_bandwidth, max_bandwidth);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	if (vp_id >= VXGE_HAL_MAX_VIRTUAL_PATHS) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);

		return (VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);
	}

	config.priority = priority;
	config.min_bandwidth = min_bandwidth;
	config.max_bandwidth = max_bandwidth;

	if ((status = __hal_vpath_qos_config_check(&config)) != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (status == VXGE_HAL_OK) {
		hldev->header.config.mrpcim_config.vp_qos[vp_id].priority =
		    priority;
		hldev->header.config.mrpcim_config.vp_qos[vp_id].min_bandwidth =
		    min_bandwidth;
		hldev->header.config.mrpcim_config.vp_qos[vp_id].max_bandwidth =
		    max_bandwidth;
	}

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mrpcim_vpath_qos_get - Get the priority, Guaranteed and maximum
 *				 bandwidth for a vpath.
 * @devh: HAL device handle.
 * @vp_id: Vpath Id.
 * @priority: Buffer to return Priority
 * @min_bandwidth: Buffer to return Minimum Bandwidth
 * @max_bandwidth: Buffer to return Maximum Bandwidth
 *
 * Get the Guaranteed and maximum bandwidth for a given vpath
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_vpath_qos_get(
    vxge_hal_device_h devh,
    u32 vp_id,
    u32 *priority,
    u32 *min_bandwidth,
    u32 *max_bandwidth)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", vp_id = %d, "
	    "priority = 0x"VXGE_OS_STXFMT", "
	    "min_bandwidth = 0x"VXGE_OS_STXFMT", "
	    "max_bandwidth = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, vp_id, (ptr_t) priority,
	    (ptr_t) min_bandwidth, (ptr_t) max_bandwidth);

	if (vp_id >= VXGE_HAL_MAX_VIRTUAL_PATHS) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);

		return (VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);
	}

	*priority =
	    hldev->header.config.mrpcim_config.vp_qos[vp_id].min_bandwidth;

	*min_bandwidth =
	    hldev->header.config.mrpcim_config.vp_qos[vp_id].min_bandwidth;

	*max_bandwidth =
	    hldev->header.config.mrpcim_config.vp_qos[vp_id].max_bandwidth;

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_mrpcim_mdio_access - Access the MDIO device
 * @devh: HAL Device handle.
 * @port: Port id
 * @operation: Type of operation
 * @device: MMD device address
 * @addr: MMD address
 * @data: MMD data
 *
 * Access the data from a MDIO Device.
 *
 */
vxge_hal_status_e
__hal_mrpcim_mdio_access(
    vxge_hal_device_h devh,
    u32 port,
    u32 operation,
    u32 device,
    u16 addr,
    u16 *data)
{
	u64 val64;
	u32 prtad;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (data != NULL));

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", operation = %d, "
	    "device = %d, addr = %d, data = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, operation, device, addr, (ptr_t) data);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	if (device == VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_DTE_XS) {
		if (port == 0)
			prtad = hldev->mrpcim->mdio_dte_prtad0;
		else
			prtad = hldev->mrpcim->mdio_dte_prtad1;
	} else {
		if (port == 0)
			prtad = hldev->mrpcim->mdio_phy_prtad0;
		else
			prtad = hldev->mrpcim->mdio_phy_prtad1;
	}

	val64 = VXGE_HAL_MDIO_MGR_ACCESS_PORT_STROBE_ONE |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE(operation) |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD(device) |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR(addr) |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DATA(*data) |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ST_PATTERN(0) |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_PREAMBLE |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_PRTAD(prtad) |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_STROBE_TWO;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->mdio_mgr_access_port[port]);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mdio_mgr_access_port[port],
	    0,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_STROBE_ONE |
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_STROBE_TWO,
	    hldev->header.config.device_poll_millis);

	if ((status == VXGE_HAL_OK) &&
	    ((operation == VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_READ_INCR) ||
	    (operation == VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_READ) ||
	    (operation ==
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ_INCR) ||
	    (operation == VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ))) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->mdio_mgr_access_port[port]);

		*data = (u16) VXGE_HAL_MDIO_MGR_ACCESS_GET_PORT_DATA(val64);

	} else {
		*data = 0;
	}

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mrpcim_intr_enable - Enable the interrupts on mrpcim.
 * @devh: HAL device handle.
 *
 * Enable mrpcim interrupts
 *
 * See also: vxge_hal_mrpcim_intr_disable().
 */
vxge_hal_status_e
vxge_hal_mrpcim_intr_enable(vxge_hal_device_h devh)
{
	u32 i;
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_mrpcim_reg_t *mrpcim_reg;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	mrpcim_reg = hldev->mrpcim_reg;

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->ini_errors_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->dma_errors_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->tgt_errors_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->config_errors_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->crdt_errors_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->mrpcim_general_errors_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->pll_errors_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->mrpcim_ppif_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->dbecc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->general_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->pcipif_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->pda_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->pcc_error_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->lso_error_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->sm_error_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rtdma_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rc_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rxdrm_sm_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rxdcm_sm_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rxdwm_sm_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rda_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rda_ecc_db_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rqa_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->frf_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rocrc_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->wde0_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->wde1_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->wde2_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->wde3_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->wrdma_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3cmct_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3cmct_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->gsscc_err_reg);

	for (i = 0; i < 3; i++) {

		VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->gssc_err0_reg[i]);

		VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->gssc_err1_reg[i]);

	}

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->gcmg1_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->gxtmc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->gcp_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->cmc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->gcmg2_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3ifcmd_cml_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3ifcmd_cml_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3ifcmd_cmu_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3ifcmd_cmu_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->psscc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->pcmg1_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->pxtmc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->cp_exc_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->cp_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->pcmg2_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->dam_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->pcmg3_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->xmac_gen_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->xgxs_gen_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->asic_ntwk_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->xgmac_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rxmac_ecc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rxmac_various_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->rxmac_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->txmac_gen_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->txmac_ecc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->tmac_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3ifcmd_fb_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3ifcmd_fb_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->mc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->grocrc_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->fau_ecc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->mc_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3fbct_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->g3fbct_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->orp_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->ptm_alarm_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->tpa_error_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->tpa_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->kdfc_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->doorbell_int_status);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->tim_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->msg_exc_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->msg_err_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->msg_err2_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->msg_err3_reg);

	VXGE_HAL_MRPCIM_ERROR_REG_CLEAR(&mrpcim_reg->msg_int_status);

	vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &mrpcim_reg->mrpcim_general_int_status);

	/* unmask interrupts */
	val64 = VXGE_HAL_INI_ERRORS_REG_DCPL_FSM_ERR |
	    VXGE_HAL_INI_ERRORS_REG_INI_BUF_DB_ERR |
	    VXGE_HAL_INI_ERRORS_REG_INI_DATA_OVERFLOW |
	    VXGE_HAL_INI_ERRORS_REG_INI_HDR_OVERFLOW;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->ini_errors_mask);

	val64 = VXGE_HAL_DMA_ERRORS_REG_RDARB_FSM_ERR |
	    VXGE_HAL_DMA_ERRORS_REG_WRARB_FSM_ERR |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_WR_HDR_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_WR_HDR_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_WR_DATA_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_WR_DATA_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_MSG_WR_HDR_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_MSG_WR_HDR_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_MSG_WR_DATA_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_MSG_WR_DATA_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_STATS_WR_HDR_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_STATS_WR_HDR_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_STATS_WR_DATA_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_STATS_WR_DATA_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_WR_HDR_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_WR_HDR_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_WR_DATA_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_WR_DATA_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_RD_HDR_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_WRDMA_RD_HDR_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_RD_HDR_OVERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DMA_RTDMA_RD_HDR_UNDERFLOW |
	    VXGE_HAL_DMA_ERRORS_REG_DBLGEN_FSM_ERR |
	    VXGE_HAL_DMA_ERRORS_REG_DBLGEN_CREDIT_FSM_ERR |
	    VXGE_HAL_DMA_ERRORS_REG_DBLGEN_DMA_WRR_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->dma_errors_mask);

	val64 = VXGE_HAL_TGT_ERRORS_REG_TGT_REQ_FSM_ERR |
	    VXGE_HAL_TGT_ERRORS_REG_TGT_CPL_FSM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->tgt_errors_mask);

	val64 = VXGE_HAL_CONFIG_ERRORS_REG_I2C_MAIN_FSM_ERR |
	    VXGE_HAL_CONFIG_ERRORS_REG_I2C_REG_FSM_ERR |
	    VXGE_HAL_CONFIG_ERRORS_REG_CFGM_I2C_TIMEOUT |
	    VXGE_HAL_CONFIG_ERRORS_REG_RIC_I2C_TIMEOUT |
	    VXGE_HAL_CONFIG_ERRORS_REG_CFGM_FSM_ERR |
	    VXGE_HAL_CONFIG_ERRORS_REG_RIC_FSM_ERR |
	    VXGE_HAL_CONFIG_ERRORS_REG_PIFM_TIMEOUT |
	    VXGE_HAL_CONFIG_ERRORS_REG_PIFM_FSM_ERR |
	    VXGE_HAL_CONFIG_ERRORS_REG_PIFM_TO_FSM_ERR |
	    VXGE_HAL_CONFIG_ERRORS_REG_RIC_RIC_RD_TIMEOUT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->config_errors_mask);

	val64 = VXGE_HAL_CRDT_ERRORS_REG_WRCRDTARB_FSM_ERR |
	    VXGE_HAL_CRDT_ERRORS_REG_WRCRDTARB_INTCTL_ILLEGAL_CRD_DEAL |
	    VXGE_HAL_CRDT_ERRORS_REG_WRCRDTARB_PDA_ILLEGAL_CRD_DEAL |
	    VXGE_HAL_CRDT_ERRORS_REG_WRCRDTARB_PCI_MSG_ILLEGAL_CRD_DEAL |
	    VXGE_HAL_CRDT_ERRORS_REG_RDCRDTARB_FSM_ERR |
	    VXGE_HAL_CRDT_ERRORS_REG_RDCRDTARB_RDA_ILLEGAL_CRD_DEAL |
	    VXGE_HAL_CRDT_ERRORS_REG_RDCRDTARB_PDA_ILLEGAL_CRD_DEAL |
	    VXGE_HAL_CRDT_ERRORS_REG_RDCRDTARB_DBLGEN_ILLEGAL_CRD_DEAL;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->crdt_errors_mask);

	val64 = VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_STATSB_FSM_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_XGEN_FSM_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_XMEM_FSM_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_KDFCCTL_FSM_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_MRIOVCTL_FSM_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_SPI_FLSH_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_SPI_IIC_ACK_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_SPI_IIC_CHKSUM_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_INI_SERR_DET |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_INTCTL_MSIX_FSM_ERR |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_INTCTL_MSI_OVERFLOW |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_PPIF_PCI_NOT_FLUSH_SW_RESET |
	    VXGE_HAL_MRPCIM_GENERAL_ERRORS_REG_PPIF_SW_RESET_FSM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->mrpcim_general_errors_mask);

	val64 = VXGE_HAL_PLL_ERRORS_REG_CORE_CMG_PLL_OOL |
	    VXGE_HAL_PLL_ERRORS_REG_CORE_FB_PLL_OOL |
	    VXGE_HAL_PLL_ERRORS_REG_CORE_X_PLL_OOL;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->pll_errors_mask);

	val64 = VXGE_HAL_MRPCIM_PPIF_INT_STATUS_INI_ERRORS_INI_INT |
	    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_DMA_ERRORS_DMA_INT |
	    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_TGT_ERRORS_TGT_INT |
	    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CONFIG_ERRORS_CONFIG_INT |
	    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_CRDT_ERRORS_CRDT_INT |
	    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_MRPCIM_GENERAL_ERRORS_GENERAL_INT |
	    VXGE_HAL_MRPCIM_PPIF_INT_STATUS_PLL_ERRORS_PLL_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->mrpcim_ppif_int_mask);

	val64 = VXGE_HAL_DBECC_ERR_REG_PCI_RETRY_BUF_DB_ERR |
	    VXGE_HAL_DBECC_ERR_REG_PCI_RETRY_SOT_DB_ERR |
	    VXGE_HAL_DBECC_ERR_REG_PCI_P_HDR_DB_ERR |
	    VXGE_HAL_DBECC_ERR_REG_PCI_P_DATA_DB_ERR |
	    VXGE_HAL_DBECC_ERR_REG_PCI_NP_HDR_DB_ERR |
	    VXGE_HAL_DBECC_ERR_REG_PCI_NP_DATA_DB_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->dbecc_err_mask);

	val64 = VXGE_HAL_GENERAL_ERR_REG_PCI_LINK_RST_FSM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->general_err_mask);

	val64 = VXGE_HAL_PCIPIF_INT_STATUS_DBECC_ERR_DBECC_ERR_INT |
	    VXGE_HAL_PCIPIF_INT_STATUS_GENERAL_ERR_GENERAL_ERR_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->pcipif_int_mask);

	val64 = VXGE_HAL_PDA_ALARM_REG_PDA_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->pda_alarm_mask);

	val64 = 0;

	for (i = 0; i < 8; i++) {
		val64 |= VXGE_HAL_PCC_ERROR_REG_PCC_PCC_FRM_BUF_DBE(i) |
		    VXGE_HAL_PCC_ERROR_REG_PCC_PCC_TXDO_DBE(i) |
		    VXGE_HAL_PCC_ERROR_REG_PCC_PCC_FSM_ERR_ALARM(i) |
		    VXGE_HAL_PCC_ERROR_REG_PCC_PCC_SERR(i);
	}

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->pcc_error_mask);

	val64 = 0;

	for (i = 0; i < 8; i++) {
		val64 |= VXGE_HAL_LSO_ERROR_REG_PCC_LSO_FSM_ERR_ALARM(i);
	}

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->lso_error_mask);

	val64 = VXGE_HAL_SM_ERROR_REG_SM_FSM_ERR_ALARM;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->sm_error_mask);

	val64 = VXGE_HAL_RTDMA_INT_STATUS_PDA_ALARM_PDA_INT |
	    VXGE_HAL_RTDMA_INT_STATUS_PCC_ERROR_PCC_INT |
	    VXGE_HAL_RTDMA_INT_STATUS_LSO_ERROR_LSO_INT |
	    VXGE_HAL_RTDMA_INT_STATUS_SM_ERROR_SM_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rtdma_int_mask);

	val64 = VXGE_HAL_RC_ALARM_REG_FTC_SM_ERR |
	    VXGE_HAL_RC_ALARM_REG_FTC_SM_PHASE_ERR |
	    VXGE_HAL_RC_ALARM_REG_BTDWM_SM_ERR |
	    VXGE_HAL_RC_ALARM_REG_BTC_SM_ERR |
	    VXGE_HAL_RC_ALARM_REG_BTDCM_SM_ERR |
	    VXGE_HAL_RC_ALARM_REG_BTDRM_SM_ERR |
	    VXGE_HAL_RC_ALARM_REG_RMM_RXD_RC_ECC_DB_ERR |
	    VXGE_HAL_RC_ALARM_REG_RHS_RXD_RHS_ECC_DB_ERR |
	    VXGE_HAL_RC_ALARM_REG_RMM_SM_ERR |
	    VXGE_HAL_RC_ALARM_REG_BTC_VPATH_MISMATCH_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rc_alarm_mask);

	val64 = 0;

	for (i = 0; i < 17; i++) {
		val64 |= VXGE_HAL_RXDRM_SM_ERR_REG_PRC_VP(i);
	}

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rxdrm_sm_err_mask);

	val64 = 0;

	for (i = 0; i < 17; i++) {
		val64 |= VXGE_HAL_RXDCM_SM_ERR_REG_PRC_VP(i);
	}

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rxdcm_sm_err_mask);

	val64 = 0;

	for (i = 0; i < 17; i++) {
		val64 |= VXGE_HAL_RXDWM_SM_ERR_REG_PRC_VP(i);
	}

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rxdwm_sm_err_mask);

	val64 = VXGE_HAL_RDA_ERR_REG_RDA_SM0_ERR_ALARM |
	    VXGE_HAL_RDA_ERR_REG_RDA_RXD_ECC_DB_ERR |
	    VXGE_HAL_RDA_ERR_REG_RDA_FRM_ECC_DB_ERR |
	    VXGE_HAL_RDA_ERR_REG_RDA_UQM_ECC_DB_ERR |
	    VXGE_HAL_RDA_ERR_REG_RDA_IMM_ECC_DB_ERR |
	    VXGE_HAL_RDA_ERR_REG_RDA_TIM_ECC_DB_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rda_err_mask);

	val64 = 0;

	for (i = 0; i < 17; i++) {
		val64 |= VXGE_HAL_RDA_ECC_DB_REG_RDA_RXD_ERR(i);
	}

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rda_ecc_db_mask);

	val64 = VXGE_HAL_RQA_ERR_REG_RQA_SM_ERR_ALARM;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rqa_err_mask);

	val64 = 0;

	for (i = 0; i < 17; i++) {
		val64 |= VXGE_HAL_FRF_ALARM_REG_PRC_VP_FRF_SM_ERR(i);
	}

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->frf_alarm_mask);

	val64 = VXGE_HAL_ROCRC_ALARM_REG_QCQ_QCC_BYP_ECC_DB |
	    VXGE_HAL_ROCRC_ALARM_REG_NOA_NMA_SM_ERR |
	    VXGE_HAL_ROCRC_ALARM_REG_NOA_IMMM_ECC_DB |
	    VXGE_HAL_ROCRC_ALARM_REG_UDQ_UMQM_ECC_DB |
	    VXGE_HAL_ROCRC_ALARM_REG_NOA_RCBM_ECC_DB |
	    VXGE_HAL_ROCRC_ALARM_REG_NOA_WCT_CMD_FIFO_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rocrc_alarm_mask);

	val64 = VXGE_HAL_WDE0_ALARM_REG_WDE0_DCC_SM_ERR |
	    VXGE_HAL_WDE0_ALARM_REG_WDE0_PRM_SM_ERR |
	    VXGE_HAL_WDE0_ALARM_REG_WDE0_CP_SM_ERR |
	    VXGE_HAL_WDE0_ALARM_REG_WDE0_CP_CMD_ERR |
	    VXGE_HAL_WDE0_ALARM_REG_WDE0_PCR_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->wde0_alarm_mask);

	val64 = VXGE_HAL_WDE1_ALARM_REG_WDE1_DCC_SM_ERR |
	    VXGE_HAL_WDE1_ALARM_REG_WDE1_PRM_SM_ERR |
	    VXGE_HAL_WDE1_ALARM_REG_WDE1_CP_SM_ERR |
	    VXGE_HAL_WDE1_ALARM_REG_WDE1_CP_CMD_ERR |
	    VXGE_HAL_WDE1_ALARM_REG_WDE1_PCR_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->wde1_alarm_mask);

	val64 = VXGE_HAL_WDE2_ALARM_REG_WDE2_DCC_SM_ERR |
	    VXGE_HAL_WDE2_ALARM_REG_WDE2_PRM_SM_ERR |
	    VXGE_HAL_WDE2_ALARM_REG_WDE2_CP_SM_ERR |
	    VXGE_HAL_WDE2_ALARM_REG_WDE2_CP_CMD_ERR |
	    VXGE_HAL_WDE2_ALARM_REG_WDE2_PCR_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->wde2_alarm_mask);

	val64 = VXGE_HAL_WDE3_ALARM_REG_WDE3_DCC_SM_ERR |
	    VXGE_HAL_WDE3_ALARM_REG_WDE3_PRM_SM_ERR |
	    VXGE_HAL_WDE3_ALARM_REG_WDE3_CP_SM_ERR |
	    VXGE_HAL_WDE3_ALARM_REG_WDE3_CP_CMD_ERR |
	    VXGE_HAL_WDE3_ALARM_REG_WDE3_PCR_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->wde3_alarm_mask);

	val64 = VXGE_HAL_WRDMA_INT_STATUS_RC_ALARM_RC_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_RXDRM_SM_ERR_RXDRM_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_RXDCM_SM_ERR_RXDCM_SM_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_RXDWM_SM_ERR_RXDWM_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_RDA_ERR_RDA_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_RDA_ECC_DB_RDA_ECC_DB_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_FRF_ALARM_FRF_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_ROCRC_ALARM_ROCRC_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_WDE0_ALARM_WDE0_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_WDE1_ALARM_WDE1_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_WDE2_ALARM_WDE2_INT |
	    VXGE_HAL_WRDMA_INT_STATUS_WDE3_ALARM_WDE3_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->wrdma_int_mask);

	val64 = VXGE_HAL_G3CMCT_ERR_REG_G3IF_SM_ERR |
	    VXGE_HAL_G3CMCT_ERR_REG_G3IF_GDDR3_DECC |
	    VXGE_HAL_G3CMCT_ERR_REG_G3IF_GDDR3_U_DECC |
	    VXGE_HAL_G3CMCT_ERR_REG_G3IF_CTRL_FIFO_DECC;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->g3cmct_err_mask);

	val64 = VXGE_HAL_G3CMCT_INT_STATUS_ERR_G3IF_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->g3cmct_int_mask);

	val64 = VXGE_HAL_GSSCC_ERR_REG_SSCC_SSR_DB_ERR(0x3) |
	    VXGE_HAL_GSSCC_ERR_REG_SSCC_TSR_DB_ERR(0x3f) |
	    VXGE_HAL_GSSCC_ERR_REG_SSCC_CP2STE_UFLOW_ERR |
	    VXGE_HAL_GSSCC_ERR_REG_SSCC_CP2TTE_UFLOW_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->gsscc_err_mask);

	for (i = 0; i < 3; i++) {

		val64 = VXGE_HAL_GSSC_ERR0_REG_SSCC_STATE_DB_ERR(0xff) |
		    VXGE_HAL_GSSC_ERR0_REG_SSCC_CM_RESP_DB_ERR(0xf) |
		    VXGE_HAL_GSSC_ERR0_REG_SSCC_SSR_RESP_DB_ERR(0x3) |
		    VXGE_HAL_GSSC_ERR0_REG_SSCC_TSR_RESP_DB_ERR(0x3f);

		VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
		    &mrpcim_reg->gssc_err0_mask[i]);

		val64 = VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_RESP_DB_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_SCREQ_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_RESP_OFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_RESP_R_WN_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_RESP_UFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_REQ_OFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_CM_REQ_UFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_FSM_OFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_FSM_UFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_REQ_OFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_REQ_UFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_RESP_OFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_RESP_R_WN_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_SSR_RESP_UFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_REQ_OFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_REQ_UFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_RESP_OFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_RESP_R_WN_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_TSR_RESP_UFLOW_ERR |
		    VXGE_HAL_GSSC_ERR1_REG_SSCC_SCRESP_ERR;

		VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
		    &mrpcim_reg->gssc_err1_mask[i]);

	}

	val64 = VXGE_HAL_GCMG1_INT_STATUS_GSSCC_ERR_GSSCC_INT |
	    VXGE_HAL_GCMG1_INT_STATUS_GSSC0_ERR0_GSSC0_0_INT |
	    VXGE_HAL_GCMG1_INT_STATUS_GSSC0_ERR1_GSSC0_1_INT |
	    VXGE_HAL_GCMG1_INT_STATUS_GSSC1_ERR0_GSSC1_0_INT |
	    VXGE_HAL_GCMG1_INT_STATUS_GSSC1_ERR1_GSSC1_1_INT |
	    VXGE_HAL_GCMG1_INT_STATUS_GSSC2_ERR0_GSSC2_0_INT |
	    VXGE_HAL_GCMG1_INT_STATUS_GSSC2_ERR1_GSSC2_1_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->gcmg1_int_mask);

	val64 = VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_MEM_DB_ERR(0xf) |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CMC_RD_DATA_DB_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_REQ_FIFO_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_REQ_DATA_FIFO_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_WR_RSP_FIFO_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_RD_RSP_FIFO_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_WRP_FIFO_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_WRP_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_RRP_FIFO_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_RRP_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_DATA_SM_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_CMC0_IF_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_CFC_SM_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_DFETCH_CREDIT_OVERFLOW |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_DFETCH_CREDIT_UNDERFLOW |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_DFETCH_SM_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_RCTRL_CREDIT_OVERFLOW |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_RCTRL_CREDIT_UNDERFLOW |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_RCTRL_SM_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_WCOMPL_SM_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_WCOMPL_TAG_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_WREQ_SM_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_BDT_CMI_WREQ_FIFO_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CP2BDT_RFIFO_POP_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_XTMC_BDT_CMI_OP_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_XTMC_BDT_DFETCH_OP_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_XTMC_BDT_DFIFO_ERR |
	    VXGE_HAL_GXTMC_ERR_REG_XTMC_CMI_ARB_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->gxtmc_err_mask);

	val64 = VXGE_HAL_GCP_ERR_REG_CP_H2L2CP_FIFO_ERR |
	    VXGE_HAL_GCP_ERR_REG_CP_STC2CP_FIFO_ERR |
	    VXGE_HAL_GCP_ERR_REG_CP_STE2CP_FIFO_ERR |
	    VXGE_HAL_GCP_ERR_REG_CP_TTE2CP_FIFO_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->gcp_err_mask);

	val64 = VXGE_HAL_CMC_ERR_REG_CMC_CMC_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->cmc_err_mask);

	val64 = VXGE_HAL_GCMG2_INT_STATUS_GXTMC_ERR_GXTMC_INT |
	    VXGE_HAL_GCMG2_INT_STATUS_GCP_ERR_GCP_INT |
	    VXGE_HAL_GCMG2_INT_STATUS_CMC_ERR_CMC_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->gcmg2_int_mask);

	val64 = VXGE_HAL_G3IFCMD_CML_ERR_REG_G3IF_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->g3ifcmd_cml_err_mask);

	val64 = VXGE_HAL_G3IFCMD_CML_INT_STATUS_ERR_G3IF_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->g3ifcmd_cml_int_mask);

	val64 = VXGE_HAL_G3IFCMD_CMU_ERR_REG_G3IF_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->g3ifcmd_cmu_err_mask);

	val64 = VXGE_HAL_G3IFCMD_CMU_INT_STATUS_ERR_G3IF_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->g3ifcmd_cmu_int_mask);

	val64 = VXGE_HAL_PSSCC_ERR_REG_SSCC_CP2STE_OFLOW_ERR |
	    VXGE_HAL_PSSCC_ERR_REG_SSCC_CP2TTE_OFLOW_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->psscc_err_mask);

	val64 = VXGE_HAL_PCMG1_INT_STATUS_PSSCC_ERR_PSSCC_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->pcmg1_int_mask);

	val64 = VXGE_HAL_PXTMC_ERR_REG_XTMC_XT_PIF_SRAM_DB_ERR(0x3) |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_REQ_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_PRSP_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_WRSP_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_REQ_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_PRSP_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_WRSP_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_REQ_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_PRSP_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_WRSP_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_REQ_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_REQ_DATA_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_WR_RSP_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_RD_RSP_FIFO_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_REQ_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_RSP_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_REQ_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_RSP_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_REQ_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_RSP_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_XIL_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_ARB_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_RAM_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CMW_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CMR_SHADOW_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_REQ_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MPT_RSP_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_REQ_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UPT_RSP_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_REQ_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CPT_RSP_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_XIL_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_ARB_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CMW_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CMR_FSM_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_RD_PROT_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_RD_PROT_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_RD_PROT_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_WR_PROT_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_WR_PROT_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_WR_PROT_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_MXP_INV_ADDR_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_UXP_INV_ADDR_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CXP_INV_ADDR_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CP2BDT_DFIFO_PUSH_ERR |
	    VXGE_HAL_PXTMC_ERR_REG_XTMC_CP2BDT_RFIFO_PUSH_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->pxtmc_err_mask);

	val64 = VXGE_HAL_CP_EXC_REG_CP_CP_CAUSE_CRIT_INT |
	    VXGE_HAL_CP_EXC_REG_CP_CP_SERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->cp_exc_mask);

	val64 = VXGE_HAL_CP_ERR_REG_CP_CP_DCACHE_DB_ERR(0xff) |
	    VXGE_HAL_CP_ERR_REG_CP_CP_ICACHE_DB_ERR(0x3) |
	    VXGE_HAL_CP_ERR_REG_CP_CP_DTAG_DB_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_CP_ITAG_DB_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_CP_TRACE_DB_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_DMA2CP_DB_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_MP2CP_DB_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_QCC2CP_DB_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_STC2CP_DB_ERR(0x3) |
	    VXGE_HAL_CP_ERR_REG_CP_H2L2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_STC2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_STE2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_TTE2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_SWIF2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_CP2DMA_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_DAM2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_MP2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_QCC2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_DMA2CP_FIFO_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_CP_WAKE_FSM_INTEGRITY_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_CP_PMON_FSM_INTEGRITY_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_DMA_RD_SHADOW_ERR |
	    VXGE_HAL_CP_ERR_REG_CP_PIFT_CREDIT_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->cp_err_mask);

	val64 = VXGE_HAL_PCMG2_INT_STATUS_PXTMC_ERR_PXTMC_INT |
	    VXGE_HAL_PCMG2_INT_STATUS_CP_EXC_CP_XT_EXC_INT |
	    VXGE_HAL_PCMG2_INT_STATUS_CP_ERR_CP_ERR_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->pcmg2_int_mask);

	val64 = VXGE_HAL_DAM_ERR_REG_DAM_RDSB_ECC_DB_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_WRSB_ECC_DB_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_HPPEDAT_ECC_DB_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_LPPEDAT_ECC_DB_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_WRRESP_ECC_DB_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_HPRD_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_LPRD_0_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_LPRD_1_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_HPPEDAT_OVERFLOW_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_LPPEDAT_OVERFLOW_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_WRRESP_OVERFLOW_ERR |
	    VXGE_HAL_DAM_ERR_REG_DAM_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->dam_err_mask);

	val64 = VXGE_HAL_PCMG3_INT_STATUS_DAM_ERR_DAM_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->pcmg3_int_mask);

	val64 = VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE0_DB_ERR(0x3) |
	    VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE1_DB_ERR(0x3) |
	    VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE2_DB_ERR(0x3) |
	    VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE3_DB_ERR(0x3) |
	    VXGE_HAL_XMAC_GEN_ERR_REG_XSTATS_RMAC_STATS_TILE4_DB_ERR(0x3) |
	    VXGE_HAL_XMAC_GEN_ERR_REG_XMACJ_XMAC_FSM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->xmac_gen_err_mask);

	val64 = VXGE_HAL_XGXS_GEN_ERR_REG_XGXS_XGXS_FSM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->xgxs_gen_err_mask);

	val64 = VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_DOWN |
	    VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_UP |
	    VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_WENT_DOWN |
	    VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_WENT_UP |
	    VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_REAFFIRMED_FAULT |
	    VXGE_HAL_ASIC_NTWK_ERR_REG_XMACJ_NTWK_REAFFIRMED_OK;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->asic_ntwk_err_mask);

	val64 = VXGE_HAL_XGMAC_INT_STATUS_XMAC_GEN_ERR_XMAC_GEN_INT |
	    VXGE_HAL_XGMAC_INT_STATUS_XGXS_GEN_ERR_XGXS_GEN_INT |
	    VXGE_HAL_XGMAC_INT_STATUS_ASIC_NTWK_ERR_ASIC_NTWK_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->xgmac_int_mask);

	val64 =
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT0_RMAC_RTS_PART_DB_ERR(0xf) |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT1_RMAC_RTS_PART_DB_ERR(0xf) |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RMAC_PORT2_RMAC_RTS_PART_DB_ERR(0xf) |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT0_DB_ERR(0x3) |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DA_LKP_PRT1_DB_ERR(0x3) |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_VID_LKP_DB_ERR |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT0_DB_ERR |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT1_DB_ERR |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_PN_LKP_PRT2_DB_ERR |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_MASK_DB_ERR(0x3f) |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_RTH_LKP_DB_ERR(0x7) |
	    VXGE_HAL_RXMAC_ECC_ERR_REG_RTSJ_RMAC_DS_LKP_DB_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->rxmac_ecc_err_mask);

	val64 = VXGE_HAL_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT0_FSM_ERR |
	    VXGE_HAL_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT1_FSM_ERR |
	    VXGE_HAL_RXMAC_VARIOUS_ERR_REG_RMAC_RMAC_PORT2_FSM_ERR |
	    VXGE_HAL_RXMAC_VARIOUS_ERR_REG_RMACJ_RMACJ_FSM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->rxmac_various_err_mask);

	val64 = VXGE_HAL_RXMAC_INT_STATUS_RXMAC_ECC_ERR_RXMAC_ECC_INT |
	    VXGE_HAL_RXMAC_INT_STATUS_RXMAC_VARIOUS_ERR_RXMAC_VARIOUS_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->rxmac_int_mask);

	val64 = VXGE_HAL_TXMAC_GEN_ERR_REG_TMACJ_PERMANENT_STOP;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->txmac_gen_err_mask);

	val64 = VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2MAC_DB_ERR |
	    VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2M_SB_DB_ERR |
	    VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMAC_TPA2M_DA_DB_ERR |
	    VXGE_HAL_TXMAC_ECC_ERR_REG_TMAC_TMAC_PORT0_FSM_ERR |
	    VXGE_HAL_TXMAC_ECC_ERR_REG_TMAC_TMAC_PORT1_FSM_ERR |
	    VXGE_HAL_TXMAC_ECC_ERR_REG_TMAC_TMAC_PORT2_FSM_ERR |
	    VXGE_HAL_TXMAC_ECC_ERR_REG_TMACJ_TMACJ_FSM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->txmac_ecc_err_mask);

	val64 = VXGE_HAL_TMAC_INT_STATUS_TXMAC_GEN_ERR_TXMAC_GEN_INT |
	    VXGE_HAL_TMAC_INT_STATUS_TXMAC_ECC_ERR_TXMAC_ECC_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->tmac_int_mask);

	val64 = VXGE_HAL_G3IFCMD_FB_ERR_REG_G3IF_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->g3ifcmd_fb_err_mask);

	val64 = VXGE_HAL_G3IFCMD_FB_INT_STATUS_ERR_G3IF_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64,
	    &mrpcim_reg->g3ifcmd_fb_int_mask);

	val64 = VXGE_HAL_MC_ERR_REG_MC_XFMD_MEM_ECC_DB_ERR_A |
	    VXGE_HAL_MC_ERR_REG_MC_XFMD_MEM_ECC_DB_ERR_B |
	    VXGE_HAL_MC_ERR_REG_MC_G3IF_RD_FIFO_ECC_DB_ERR |
	    VXGE_HAL_MC_ERR_REG_MC_MIRI_ECC_DB_ERR_0 |
	    VXGE_HAL_MC_ERR_REG_MC_MIRI_ECC_DB_ERR_1 |
	    VXGE_HAL_MC_ERR_REG_MC_SM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->mc_err_mask);

	val64 = VXGE_HAL_GROCRC_ALARM_REG_XFMD_WR_FIFO_ERR |
	    VXGE_HAL_GROCRC_ALARM_REG_WDE2MSR_RD_FIFO_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->grocrc_alarm_mask);

	val64 = VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT0_FAU_MAC2F_N_DB_ERR |
	    VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT0_FAU_MAC2F_W_DB_ERR(0x3) |
	    VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT1_FAU_MAC2F_N_DB_ERR |
	    VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT1_FAU_MAC2F_W_DB_ERR(0x3) |
	    VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT2_FAU_MAC2F_N_DB_ERR |
	    VXGE_HAL_FAU_ECC_ERR_REG_FAU_PORT2_FAU_MAC2F_W_DB_ERR(0x3) |
	    VXGE_HAL_FAU_ECC_ERR_REG_FAU_FAU_XFMD_INS_DB_ERR(0x3) |
	    VXGE_HAL_FAU_ECC_ERR_REG_FAUJ_FAU_FSM_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->fau_ecc_err_mask);

	val64 = VXGE_HAL_MC_INT_STATUS_MC_ERR_MC_INT |
	    VXGE_HAL_MC_INT_STATUS_GROCRC_ALARM_ROCRC_INT |
	    VXGE_HAL_MC_INT_STATUS_FAU_ECC_ERR_FAU_ECC_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->mc_int_mask);

	val64 = VXGE_HAL_G3FBCT_ERR_REG_G3IF_SM_ERR |
	    VXGE_HAL_G3FBCT_ERR_REG_G3IF_GDDR3_DECC |
	    VXGE_HAL_G3FBCT_ERR_REG_G3IF_GDDR3_U_DECC |
	    VXGE_HAL_G3FBCT_ERR_REG_G3IF_CTRL_FIFO_DECC;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->g3fbct_err_mask);

	val64 = VXGE_HAL_G3FBCT_INT_STATUS_ERR_G3IF_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->g3fbct_int_mask);

	val64 = VXGE_HAL_ORP_ERR_REG_ORP_FIFO_DB_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_XFMD_FIFO_UFLOW_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_FRM_FIFO_UFLOW_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_XFMD_RCV_FSM_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_OUTREAD_FSM_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_OUTQEM_FSM_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_XFMD_RCV_SHADOW_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_OUTREAD_SHADOW_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_OUTQEM_SHADOW_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_OUTFRM_SHADOW_ERR |
	    VXGE_HAL_ORP_ERR_REG_ORP_OPTPRS_SHADOW_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->orp_err_mask);

	val64 = VXGE_HAL_PTM_ALARM_REG_PTM_RDCTRL_SYNC_ERR |
	    VXGE_HAL_PTM_ALARM_REG_PTM_RDCTRL_FIFO_ERR |
	    VXGE_HAL_PTM_ALARM_REG_XFMD_RD_FIFO_ERR |
	    VXGE_HAL_PTM_ALARM_REG_WDE2MSR_WR_FIFO_ERR |
	    VXGE_HAL_PTM_ALARM_REG_PTM_FRMM_ECC_DB_ERR(0x3);

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->ptm_alarm_mask);

	val64 = VXGE_HAL_TPA_ERROR_REG_TPA_FSM_ERR_ALARM |
	    VXGE_HAL_TPA_ERROR_REG_TPA_TPA_DA_LKUP_PRT0_DB_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->tpa_error_mask);

	val64 = VXGE_HAL_TPA_INT_STATUS_ORP_ERR_ORP_INT |
	    VXGE_HAL_TPA_INT_STATUS_PTM_ALARM_PTM_INT |
	    VXGE_HAL_TPA_INT_STATUS_TPA_ERROR_TPA_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->tpa_int_mask);

	val64 = VXGE_HAL_KDFC_ERR_REG_KDFC_KDFC_ECC_DB_ERR |
	    VXGE_HAL_KDFC_ERR_REG_KDFC_KDFC_SM_ERR_ALARM;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->kdfc_err_mask);

	val64 = VXGE_HAL_DOORBELL_INT_STATUS_KDFC_ERR_REG_TXDMA_KDFC_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->doorbell_int_mask);

	val64 = VXGE_HAL_TIM_ERR_REG_TIM_VBLS_DB_ERR |
	    VXGE_HAL_TIM_ERR_REG_TIM_BMAP_PA_DB_ERR |
	    VXGE_HAL_TIM_ERR_REG_TIM_BMAP_PB_DB_ERR |
	    VXGE_HAL_TIM_ERR_REG_TIM_BMAP_MSG_DB_ERR |
	    VXGE_HAL_TIM_ERR_REG_TIM_BMAP_MEM_CNTRL_SM_ERR |
	    VXGE_HAL_TIM_ERR_REG_TIM_BMAP_MSG_MEM_CNTRL_SM_ERR |
	    VXGE_HAL_TIM_ERR_REG_TIM_MPIF_PCIWR_ERR |
	    VXGE_HAL_TIM_ERR_REG_TIM_ROCRC_BMAP_UPDT_FIFO_ERR |
	    VXGE_HAL_TIM_ERR_REG_TIM_CREATE_BMAPMSG_FIFO_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->tim_err_mask);

	val64 = VXGE_HAL_MSG_EXC_REG_MP_MXP_CAUSE_CRIT_INT |
	    VXGE_HAL_MSG_EXC_REG_UP_UXP_CAUSE_CRIT_INT |
	    VXGE_HAL_MSG_EXC_REG_MP_MXP_SERR |
	    VXGE_HAL_MSG_EXC_REG_UP_UXP_SERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->msg_exc_mask);

	val64 = VXGE_HAL_MSG_ERR_REG_UP_UXP_WAKE_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MP_MXP_WAKE_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_DMA_READ_CMD_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_DMA_RESP_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_OWN_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_PDA_ACC_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MP_MXP_PMON_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_UP_UXP_PMON_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_XFMDQRY_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_FRMQRY_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_UMQ_WRITE_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_UMQ_BWR_PF_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_REG_RESP_FIFO_ERR |
	    VXGE_HAL_MSG_ERR_REG_UP_UXP_DTAG_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_UP_UXP_ITAG_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MP_MXP_DTAG_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MP_MXP_ITAG_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_UP_UXP_TRACE_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MP_MXP_TRACE_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_CMG2MSG_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_TXPE2MSG_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_RXPE2MSG_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_RPE2MSG_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_REG_READ_FIFO_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_MXP2UXP_FIFO_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_KDFC_SIF_FIFO_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_CXP2SWIF_FIFO_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_UMQ_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_BWR_PF_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_BWR_SIF_FIFO_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMQ_ECC_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMA_READ_FIFO_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_DMA_RESP_ECC_DB_ERR |
	    VXGE_HAL_MSG_ERR_REG_MSG_QUE_UXP2MXP_FIFO_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->msg_err_mask);

	val64 =
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_CMG2MSG_DISPATCH_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_DMQ_DISPATCH_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_SWIF_DISPATCH_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_PIC_WRITE_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_SWIFREG_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_TIM_WRITE_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_UMQ_TA_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_TXPE_TA_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_RXPE_TA_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_SWIF_TA_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_DMA_TA_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_CP_TA_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA16_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA15_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA14_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA13_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA12_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA11_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA10_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA9_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA8_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA7_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA6_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA5_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA4_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA3_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA2_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA1_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_LONGTERMUMQ_TA0_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_FBMC_OWN_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_TXPE2MSG_DISPATCH_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_RXPE2MSG_DISPATCH_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_RPE2MSG_DISPATCH_FSM_INTEGRITY_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MP_MP_PIFT_IF_CREDIT_CNT_ERR |
	    VXGE_HAL_MSG_ERR2_REG_UP_UP_PIFT_IF_CREDIT_CNT_ERR |
	    VXGE_HAL_MSG_ERR2_REG_MSG_QUE_UMQ2PIC_CMD_FIFO_ERR |
	    VXGE_HAL_MSG_ERR2_REG_TIM_TIM2MSG_CMD_FIFO_ERR;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->msg_err2_mask);

	val64 = VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR0 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR1 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR2 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR3 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR4 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR5 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR6 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_DCACHE_DB_ERR7 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_ICACHE_DB_ERR0 |
	    VXGE_HAL_MSG_ERR3_REG_UP_UXP_ICACHE_DB_ERR1 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR0 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR1 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR2 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR3 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR4 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR5 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR6 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_DCACHE_DB_ERR7 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_ICACHE_DB_ERR0 |
	    VXGE_HAL_MSG_ERR3_REG_MP_MXP_ICACHE_DB_ERR1;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->msg_err3_mask);

	val64 = VXGE_HAL_MSG_INT_STATUS_TIM_ERR_TIM_INT |
	    VXGE_HAL_MSG_INT_STATUS_MSG_EXC_MSG_XT_EXC_INT |
	    VXGE_HAL_MSG_INT_STATUS_MSG_ERR3_MSG_ERR3_INT |
	    VXGE_HAL_MSG_INT_STATUS_MSG_ERR2_MSG_ERR2_INT |
	    VXGE_HAL_MSG_INT_STATUS_MSG_ERR_MSG_ERR_INT;

	VXGE_HAL_MRPCIM_ERROR_REG_UNMASK(val64, &mrpcim_reg->msg_int_mask);

	val64 = VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PIC_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PCI_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_RTDMA_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_WRDMA_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3CMCT_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_GCMG1_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_GCMG2_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3CMIFL_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3CMIFU_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PCMG1_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PCMG2_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_PCMG3_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_XMAC_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_RXMAC_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_TMAC_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3FBIF_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_FBMC_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_G3FBCT_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_TPA_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_DRBELL_INT |
	    VXGE_HAL_MRPCIM_GENERAL_INT_STATUS_MSG_INT;

	vxge_hal_pio_mem_write32_upper(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(~val64, 0),
	    &mrpcim_reg->mrpcim_general_int_mask);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_mrpcim_intr_disable - Disable the interrupts on mrpcim.
 * @devh: HAL device handle.
 *
 * Disable mrpcim interrupts
 *
 * See also: vxge_hal_mrpcim_intr_enable().
 */
vxge_hal_status_e
vxge_hal_mrpcim_intr_disable(vxge_hal_device_h devh)
{
	u32 i;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_mrpcim_reg_t *mrpcim_reg;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	mrpcim_reg = hldev->mrpcim_reg;

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->ini_errors_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->dma_errors_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->tgt_errors_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->config_errors_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->crdt_errors_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->mrpcim_general_errors_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->pll_errors_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->mrpcim_ppif_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->dbecc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->general_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->pcipif_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->pda_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->pcc_error_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->lso_error_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->sm_error_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rtdma_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rc_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rxdrm_sm_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rxdcm_sm_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rxdwm_sm_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rda_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rda_ecc_db_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rqa_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->frf_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rocrc_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->wde0_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->wde1_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->wde2_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->wde3_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->wrdma_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3cmct_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3cmct_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->gsscc_err_mask);

	for (i = 0; i < 3; i++) {

		VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->gssc_err0_mask[i]);

		VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->gssc_err1_mask[i]);

	}

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->gcmg1_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->gxtmc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->gcp_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->cmc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->gcmg2_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3ifcmd_cml_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3ifcmd_cml_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3ifcmd_cmu_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3ifcmd_cmu_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->psscc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->pcmg1_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->pxtmc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->cp_exc_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->cp_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->pcmg2_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->dam_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->pcmg3_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->xmac_gen_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->xgxs_gen_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->asic_ntwk_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->xgmac_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rxmac_ecc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rxmac_various_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->rxmac_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->txmac_gen_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->txmac_ecc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->tmac_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3ifcmd_fb_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3ifcmd_fb_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->mc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->grocrc_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->fau_ecc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->mc_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3fbct_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->g3fbct_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->orp_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->ptm_alarm_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->tpa_error_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->tpa_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->kdfc_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->doorbell_int_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->tim_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->msg_exc_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->msg_err_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->msg_err2_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->msg_err3_mask);

	VXGE_HAL_MRPCIM_ERROR_REG_MASK(&mrpcim_reg->msg_int_mask);

	vxge_hal_pio_mem_write32_upper(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &mrpcim_reg->mrpcim_general_int_mask);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_mrpcim_reset - Reset the entire device.
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
vxge_hal_mrpcim_reset(vxge_hal_device_h devh)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	if (!hldev->header.is_initialized)
		return (VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);

	if (hldev->device_resetting == 1) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_RESET_IN_PROGRESS);

		return (VXGE_HAL_ERR_RESET_IN_PROGRESS);
	}

	(void) __hal_ifmsg_wmsg_post(hldev,
	    hldev->first_vp_id,
	    VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_BROADCAST,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_DEVICE_RESET_BEGIN,
	    0);

	vxge_os_mdelay(100);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->sw_reset_cfg1);

	val64 |= VXGE_HAL_SW_RESET_CFG1_TYPE;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->sw_reset_cfg1);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_PF_SW_RESET_PF_SW_RESET(
	    VXGE_HAL_PF_SW_RESET_COMMAND),
	    &hldev->mrpcim_reg->bf_sw_reset);

	hldev->stats.sw_dev_info_stats.soft_reset_cnt++;

	hldev->device_resetting = 1;

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, VXGE_HAL_PENDING);

	return (VXGE_HAL_PENDING);
}

/*
 * vxge_hal_mrpcim_reset_poll - Poll the device for reset complete.
 * @devh: HAL device handle.
 *
 * Soft-reset the device, reset the device stats except reset_cnt.
 *
 * After reset is done, will try to re-initialize HW.
 *
 * Returns:  VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED - Device is not initialized.
 * VXGE_HAL_ERR_RESET_FAILED - Reset failed.
 *
 * See also: vxge_hal_status_e {}.
 */
vxge_hal_status_e
vxge_hal_mrpcim_reset_poll(vxge_hal_device_h devh)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d", __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	if (!hldev->header.is_initialized) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);
		return (VXGE_HAL_ERR_DEVICE_NOT_INITIALIZED);
	}

	if ((status = __hal_device_reg_addr_get(hldev)) != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		hldev->device_resetting = 0;
		return (status);
	}

	__hal_device_id_get(hldev);

	__hal_device_host_info_get(hldev);

	hldev->hw_is_initialized = 0;

	hldev->device_resetting = 0;

	vxge_os_memzero(hldev->mrpcim->mrpcim_stats,
	    sizeof(vxge_hal_mrpcim_stats_hw_info_t));

	vxge_os_memzero(&hldev->mrpcim->mrpcim_stats_sav,
	    sizeof(vxge_hal_mrpcim_stats_hw_info_t));

	status = __hal_mrpcim_mac_configure(hldev);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = __hal_mrpcim_lag_configure(hldev);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mdio_gen_cfg_port[0]);

	hldev->mrpcim->mdio_phy_prtad0 =
	    (u32) VXGE_HAL_MDIO_GEN_CFG_PORT_GET_MDIO_PHY_PRTAD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mdio_gen_cfg_port[1]);

	hldev->mrpcim->mdio_phy_prtad1 =
	    (u32) VXGE_HAL_MDIO_GEN_CFG_PORT_GET_MDIO_PHY_PRTAD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->xgxs_static_cfg_port[0]);

	hldev->mrpcim->mdio_dte_prtad0 =
	    (u32) VXGE_HAL_XGXS_STATIC_CFG_PORT_GET_MDIO_DTE_PRTAD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->xgxs_static_cfg_port[1]);

	hldev->mrpcim->mdio_dte_prtad1 =
	    (u32) VXGE_HAL_XGXS_STATIC_CFG_PORT_GET_MDIO_DTE_PRTAD(val64);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    hldev->mrpcim->mrpcim_stats_block->dma_addr,
	    &hldev->mrpcim_reg->mrpcim_stats_start_host_addr);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    hldev->vpath_assignments,
	    &hldev->mrpcim_reg->rxmac_authorize_all_addr);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    hldev->vpath_assignments,
	    &hldev->mrpcim_reg->rxmac_authorize_all_vid);

	(void) __hal_ifmsg_wmsg_post(hldev,
	    hldev->first_vp_id,
	    VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_BROADCAST,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_DEVICE_RESET_END,
	    0);

	(void) vxge_hal_device_reset_poll(devh);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_mrpcim_xpak_counter_check -  check the Xpak error count and log the msg
 * @hldev: pointer to __hal_device_t structure
 * @port: Port number
 * @type:  xpak stats error type
 * @value: xpak stats value
 *
 * It is used to log the error message based on the xpak stats value
 * Return value:
 * None
 */
void
__hal_mrpcim_xpak_counter_check(__hal_device_t *hldev,
    u32 port, u32 type, u32 value)
{
	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "hldev = 0x"VXGE_OS_STXFMT", port = %d, type = %d, value = %d",
	    (ptr_t) hldev, port, type, value);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {

		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__,
		    __LINE__, VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return;

	}

	/*
	 * If the value is high for three consecutive cylce,
	 * log a error message
	 */
	if (value == 3) {
		switch (type) {
		case VXGE_HAL_XPAK_ALARM_EXCESS_TEMP:
			hldev->mrpcim->xpak_stats[port].excess_temp = 0;

			/*
			 * Notify the ULD on Excess Xpak temperature alarm msg
			 */
			if (g_vxge_hal_driver->uld_callbacks.xpak_alarm_log) {
				g_vxge_hal_driver->uld_callbacks.xpak_alarm_log(
				    hldev->header.upper_layer_data,
				    port,
				    VXGE_HAL_XPAK_ALARM_EXCESS_TEMP);
			}
			break;
		case VXGE_HAL_XPAK_ALARM_EXCESS_BIAS_CURRENT:
			hldev->mrpcim->xpak_stats[port].excess_bias_current = 0;

			/*
			 * Notify the ULD on Excess  xpak bias current alarm msg
			 */
			if (g_vxge_hal_driver->uld_callbacks.xpak_alarm_log) {
				g_vxge_hal_driver->uld_callbacks.xpak_alarm_log(
				    hldev->header.upper_layer_data,
				    port,
				    VXGE_HAL_XPAK_ALARM_EXCESS_BIAS_CURRENT);
			}
			break;
		case VXGE_HAL_XPAK_ALARM_EXCESS_LASER_OUTPUT:
			hldev->mrpcim->xpak_stats[port].excess_laser_output = 0;

			/*
			 * Notify the ULD on Excess Xpak Laser o/p power
			 * alarm msg
			 */
			if (g_vxge_hal_driver->uld_callbacks.xpak_alarm_log) {
				g_vxge_hal_driver->uld_callbacks.xpak_alarm_log(
				    hldev->header.upper_layer_data,
				    port,
				    VXGE_HAL_XPAK_ALARM_EXCESS_LASER_OUTPUT);
			}
			break;
		default:
			vxge_hal_info_log_stats("%s",
			    "Incorrect XPAK Alarm type");
		}
	}

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_mrpcim_xpak_stats_poll -  Poll and update the Xpak error count.
 * @devh: HAL device handle
 * @port: Port number
 *
 * It is used to update the xpak stats value
 */
vxge_hal_status_e
vxge_hal_mrpcim_xpak_stats_poll(
    vxge_hal_device_h devh, u32 port)
{
	u16 val;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("hldev = 0x"VXGE_OS_STXFMT", port = %d",
	    (ptr_t) hldev, port);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {

		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	/* Loading the DOM register to MDIO register */

	val = 0;

	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_WRITE,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_CMD_STAT,
	    &val);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_CMD_STAT,
	    &val);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	/*
	 * Reading the Alarm flags
	 */
	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_ALARM_FLAG,
	    &val);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (val &
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_ALARM_FLAG_TEMP_HIGH) {
		hldev->mrpcim->xpak_stats[port].alarm_transceiver_temp_high++;
		hldev->mrpcim->xpak_stats[port].excess_temp++;
		__hal_mrpcim_xpak_counter_check(hldev, port,
		    VXGE_HAL_XPAK_ALARM_EXCESS_TEMP,
		    hldev->mrpcim->xpak_stats[port].excess_temp);
	} else {
		hldev->mrpcim->xpak_stats[port].excess_temp = 0;
	}

	if (val &
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_ALARM_FLAG_TEMP_LOW) {
		hldev->mrpcim->xpak_stats[port].alarm_transceiver_temp_low++;
	}

	if (val &
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_ALARM_FLAG_CUR_HIGH) {
		hldev->mrpcim->xpak_stats[port].alarm_laser_bias_current_high++;
		hldev->mrpcim->xpak_stats[port].excess_bias_current++;
		__hal_mrpcim_xpak_counter_check(hldev, port,
		    VXGE_HAL_XPAK_ALARM_EXCESS_BIAS_CURRENT,
		    hldev->mrpcim->xpak_stats[port].excess_bias_current);
	} else {
		hldev->mrpcim->xpak_stats[port].excess_bias_current = 0;
	}

	if (val &
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_ALARM_FLAG_CUR_LOW) {
		hldev->mrpcim->xpak_stats[port].alarm_laser_bias_current_low++;
	}

	if (val &
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_ALARM_FLAG_PWR_HIGH) {
		hldev->mrpcim->xpak_stats[port].alarm_laser_output_power_high++;
		hldev->mrpcim->xpak_stats[port].excess_laser_output++;
		__hal_mrpcim_xpak_counter_check(hldev, port,
		    VXGE_HAL_XPAK_ALARM_EXCESS_LASER_OUTPUT,
		    hldev->mrpcim->xpak_stats[port].excess_laser_output);
	} else {
		hldev->mrpcim->xpak_stats[port].excess_laser_output = 0;
	}

	if (val &
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_ALARM_FLAG_PWR_LOW) {
		hldev->mrpcim->xpak_stats[port].alarm_laser_output_power_low++;
	}

	/*
	 * Reading the warning flags
	 */
	status = __hal_mrpcim_mdio_access(devh, port,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_OP_TYPE_ADDR_READ,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_DEVAD_PMA_PMD,
	    VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_WARN_FLAG,
	    &val);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (val & VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_WARN_FLAG_TEMP_HIGH)
		hldev->mrpcim->xpak_stats[port].warn_transceiver_temp_high++;
	if (val & VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_WARN_FLAG_TEMP_LOW)
		hldev->mrpcim->xpak_stats[port].warn_transceiver_temp_low++;
	if (val & VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_WARN_FLAG_CUR_HIGH)
		hldev->mrpcim->xpak_stats[port].warn_laser_bias_current_high++;
	if (val & VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_WARN_FLAG_CUR_LOW)
		hldev->mrpcim->xpak_stats[port].warn_laser_bias_current_low++;
	if (val & VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_WARN_FLAG_PWR_HIGH)
		hldev->mrpcim->xpak_stats[port].warn_laser_output_power_high++;
	if (val & VXGE_HAL_MDIO_MGR_ACCESS_PORT_ADDR_DOM_TX_WARN_FLAG_PWR_LOW)
		hldev->mrpcim->xpak_stats[port].warn_laser_output_power_low++;

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mrpcim_stats_enable - Enable mrpcim statistics.
 * @devh: HAL Device.
 *
 * Enable the DMA mrpcim statistics for the device. The function is to be called
 * to re-enable the adapter to update stats into the host memory
 *
 * See also: vxge_hal_mrpcim_stats_disable()
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_enable(vxge_hal_device_h devh)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {

		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	vxge_os_memcpy(&hldev->mrpcim->mrpcim_stats_sav,
	    hldev->mrpcim->mrpcim_stats,
	    sizeof(vxge_hal_mrpcim_stats_hw_info_t));

	if (hldev->header.config.stats_read_method ==
	    VXGE_HAL_STATS_READ_METHOD_DMA) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->mrpcim_general_cfg2);

		val64 |= VXGE_HAL_MRPCIM_GENERAL_CFG2_MRPCIM_STATS_ENABLE;

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->mrpcim_general_cfg2);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->stats_cfg0);

		val64 |= VXGE_HAL_STATS_CFG0_STATS_ENABLE(
		    (1 << (16 - hldev->first_vp_id)));

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->stats_cfg0);
	} else {
		status = __hal_mrpcim_stats_get(
		    hldev,
		    hldev->mrpcim->mrpcim_stats);
	}

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mrpcim_stats_disable - Disable mrpcim statistics.
 * @devh: HAL Device.
 *
 * Enable the DMA mrpcim statistics for the device. The function is to be called
 * to disable the adapter to update stats into the host memory. This function
 * is not needed to be called, normally.
 *
 * See also: vxge_hal_mrpcim_stats_enable()
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_disable(vxge_hal_device_h devh)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {

		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mrpcim_general_cfg2);

	val64 &= ~VXGE_HAL_MRPCIM_GENERAL_CFG2_MRPCIM_STATS_ENABLE;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->mrpcim_general_cfg2);

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mrpcim_stats_get - Get the device mrpcim statistics.
 * @devh: HAL Device.
 * @stats: mrpcim stats
 *
 * Returns the device mrpcim stats for the device.
 *
 * See also: vxge_hal_device_stats_get()
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_get(
    vxge_hal_device_h devh,
    vxge_hal_mrpcim_stats_hw_info_t *stats)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((hldev != NULL) && (stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "devh = 0x"VXGE_OS_STXFMT", stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) stats);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {

		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	if (hldev->header.config.stats_read_method ==
	    VXGE_HAL_STATS_READ_METHOD_DMA) {

		status = vxge_hal_device_register_poll(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->stats_cfg0,
		    0,
		    VXGE_HAL_STATS_CFG0_STATS_ENABLE(
		    (1 << (16 - hldev->first_vp_id))),
		    hldev->header.config.device_poll_millis);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->mrpcim_general_cfg2);

		val64 &= ~VXGE_HAL_MRPCIM_GENERAL_CFG2_MRPCIM_STATS_ENABLE;

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->mrpcim_general_cfg2);
	}

	if (status == VXGE_HAL_OK) {
		vxge_os_memcpy(stats,
		    hldev->mrpcim->mrpcim_stats,
		    sizeof(vxge_hal_mrpcim_stats_hw_info_t));
	}

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mrpcim_stats_access - Access the statistics from the given location
 *			  and offset and perform an operation
 * @devh: HAL Device handle.
 * @operation: Operation to be performed
 * @location: Location (one of vpath id, aggregate or port)
 * @offset: Offset with in the location
 * @stat: Pointer to a buffer to return the value
 *
 * Get the statistics from the given location and offset.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_access(
    vxge_hal_device_h devh,
    u32 operation,
    u32 location,
    u32 offset,
    u64 *stat)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (stat != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT", operation = %d, "
	    "location = %d, offset = %d, stat = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, operation, location, offset, (ptr_t) stat);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	val64 = VXGE_HAL_XMAC_STATS_SYS_CMD_OP(operation) |
	    VXGE_HAL_XMAC_STATS_SYS_CMD_STROBE |
	    VXGE_HAL_XMAC_STATS_SYS_CMD_LOC_SEL(location) |
	    VXGE_HAL_XMAC_STATS_SYS_CMD_OFFSET_SEL(offset);


	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &hldev->mrpcim_reg->xmac_stats_sys_cmd);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->mrpcim_reg->xmac_stats_sys_cmd);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->xmac_stats_sys_cmd,
	    0,
	    VXGE_HAL_XMAC_STATS_SYS_CMD_STROBE,
	    hldev->header.config.device_poll_millis);

	if ((status == VXGE_HAL_OK) && (operation == VXGE_HAL_STATS_OP_READ)) {

		*stat = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->xmac_stats_sys_data);

	} else {
		*stat = 0;
	}

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mrpcim_xmac_aggr_stats_get - Get the Statistics on aggregate port
 * @devh: HAL device handle.
 * @port: Number of the port (0 or 1)
 * @aggr_stats: Buffer to return Statistics on aggregate port.
 *
 * Get the Statistics on aggregate port
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_xmac_aggr_stats_get(vxge_hal_device_h devh,
    u32 port,
    vxge_hal_xmac_aggr_stats_t *aggr_stats)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (aggr_stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT", port = %d, "
	    "aggr_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, port, (ptr_t) aggr_stats);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_TX_FRMS_OFFSET(port));

	aggr_stats->tx_frms =
	    VXGE_HAL_STATS_GET_AGGRn_TX_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_TX_DATA_OCTETS_OFFSET(port));

	aggr_stats->tx_data_octets =
	    VXGE_HAL_STATS_GET_AGGRn_TX_DATA_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_TX_MCAST_FRMS_OFFSET(port));

	aggr_stats->tx_mcast_frms =
	    VXGE_HAL_STATS_GET_AGGRn_TX_MCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_TX_BCAST_FRMS_OFFSET(port));

	aggr_stats->tx_bcast_frms =
	    VXGE_HAL_STATS_GET_AGGRn_TX_BCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_TX_DISCARDED_FRMS_OFFSET(port));

	aggr_stats->tx_discarded_frms =
	    VXGE_HAL_STATS_GET_AGGRn_TX_DISCARDED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_TX_ERRORED_FRMS_OFFSET(port));

	aggr_stats->tx_errored_frms =
	    VXGE_HAL_STATS_GET_AGGRn_TX_ERRORED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_RX_FRMS_OFFSET(port));

	aggr_stats->rx_frms =
	    VXGE_HAL_STATS_GET_AGGRn_RX_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_RX_DATA_OCTETS_OFFSET(port));

	aggr_stats->rx_data_octets =
	    VXGE_HAL_STATS_GET_AGGRn_RX_DATA_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_RX_MCAST_FRMS_OFFSET(port));

	aggr_stats->rx_mcast_frms =
	    VXGE_HAL_STATS_GET_AGGRn_RX_MCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_RX_BCAST_FRMS_OFFSET(port));

	aggr_stats->rx_bcast_frms =
	    VXGE_HAL_STATS_GET_AGGRn_RX_BCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_RX_DISCARDED_FRMS_OFFSET(port));

	aggr_stats->rx_discarded_frms =
	    VXGE_HAL_STATS_GET_AGGRn_RX_DISCARDED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_RX_ERRORED_FRMS_OFFSET(port));

	aggr_stats->rx_errored_frms =
	    VXGE_HAL_STATS_GET_AGGRn_RX_ERRORED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_AGGRn_RX_U_SLOW_PROTO_FRMS_OFFSET(port));

	aggr_stats->rx_unknown_slow_proto_frms =
	    VXGE_HAL_STATS_GET_AGGRn_RX_U_SLOW_PROTO_FRMS(val64);

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (VXGE_HAL_OK);
}


/*
 * vxge_hal_mrpcim_xmac_port_stats_get - Get the Statistics on a port
 * @devh: HAL device handle.
 * @port: Number of the port (wire 0, wire 1 or LAG)
 * @port_stats: Buffer to return Statistics on a port.
 *
 * Get the Statistics on port
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_xmac_port_stats_get(vxge_hal_device_h devh,
    u32 port,
    vxge_hal_xmac_port_stats_t *port_stats)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert((devh != NULL) && (port_stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT", port = %d, "
	    "port_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, port, (ptr_t) port_stats);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_TTL_FRMS_OFFSET(port));

	port_stats->tx_ttl_frms =
	    VXGE_HAL_STATS_GET_PORTn_TX_TTL_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_TTL_FRMS_OFFSET(port));

	port_stats->tx_ttl_octets =
	    VXGE_HAL_STATS_GET_PORTn_TX_TTL_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_DATA_OCTETS_OFFSET(port));

	port_stats->tx_data_octets =
	    VXGE_HAL_STATS_GET_PORTn_TX_DATA_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_MCAST_FRMS_OFFSET(port));

	port_stats->tx_mcast_frms =
	    VXGE_HAL_STATS_GET_PORTn_TX_MCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_BCAST_FRMS_OFFSET(port));

	port_stats->tx_bcast_frms =
	    VXGE_HAL_STATS_GET_PORTn_TX_BCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_UCAST_FRMS_OFFSET(port));

	port_stats->tx_ucast_frms =
	    VXGE_HAL_STATS_GET_PORTn_TX_UCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_TAGGED_FRMS_OFFSET(port));

	port_stats->tx_tagged_frms =
	    VXGE_HAL_STATS_GET_PORTn_TX_TAGGED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_VLD_IP_OFFSET(port));

	port_stats->tx_vld_ip =
	    VXGE_HAL_STATS_GET_PORTn_TX_VLD_IP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_VLD_IP_OCTETS_OFFSET(port));

	port_stats->tx_vld_ip_octets =
	    VXGE_HAL_STATS_GET_PORTn_TX_VLD_IP_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_ICMP_OFFSET(port));

	port_stats->tx_icmp =
	    VXGE_HAL_STATS_GET_PORTn_TX_ICMP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_TCP_OFFSET(port));

	port_stats->tx_tcp =
	    VXGE_HAL_STATS_GET_PORTn_TX_TCP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_RST_TCP_OFFSET(port));

	port_stats->tx_rst_tcp =
	    VXGE_HAL_STATS_GET_PORTn_TX_RST_TCP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_UDP_OFFSET(port));

	port_stats->tx_udp =
	    VXGE_HAL_STATS_GET_PORTn_TX_UDP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_UNKNOWN_PROTOCOL_OFFSET(port));

	port_stats->tx_unknown_protocol =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_UNKNOWN_PROTOCOL(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_PARSE_ERROR_OFFSET(port));

	port_stats->tx_parse_error =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_PARSE_ERROR(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_PAUSE_CTRL_FRMS_OFFSET(port));

	port_stats->tx_pause_ctrl_frms =
	    VXGE_HAL_STATS_GET_PORTn_TX_PAUSE_CTRL_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_LACPDU_FRMS_OFFSET(port));

	port_stats->tx_lacpdu_frms =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_LACPDU_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_MRKR_PDU_FRMS_OFFSET(port));

	port_stats->tx_marker_pdu_frms =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_MRKR_PDU_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_MRKR_RESP_PDU_FRMS_OFFSET(port));

	port_stats->tx_marker_resp_pdu_frms =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_MRKR_RESP_PDU_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_DROP_IP_OFFSET(port));

	port_stats->tx_drop_ip =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_DROP_IP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_XGMII_CHAR1_MATCH_OFFSET(port));

	port_stats->tx_xgmii_char1_match =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_XGMII_CHAR1_MATCH(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_XGMII_CHAR2_MATCH_OFFSET(port));

	port_stats->tx_xgmii_char2_match =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_XGMII_CHAR2_MATCH(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_XGMII_COL1_MATCH_OFFSET(port));

	port_stats->tx_xgmii_column1_match =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_XGMII_COL1_MATCH(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_XGMII_COL2_MATCH_OFFSET(port));

	port_stats->tx_xgmii_column2_match =
	    (u32) VXGE_HAL_STATS_GET_PORTn_TX_XGMII_COL2_MATCH(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_DROP_FRMS_OFFSET(port));

	port_stats->tx_drop_frms =
	    (u16) VXGE_HAL_STATS_GET_PORTn_TX_DROP_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_TX_ANY_ERR_FRMS_OFFSET(port));

	port_stats->tx_any_err_frms =
	    (u16) VXGE_HAL_STATS_GET_PORTn_TX_ANY_ERR_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_FRMS_OFFSET(port));

	port_stats->rx_ttl_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_VLD_FRMS_OFFSET(port));

	port_stats->rx_vld_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_VLD_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_OFFLOAD_FRMS_OFFSET(port));

	port_stats->rx_offload_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_OFFLOAD_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_OCTETS_OFFSET(port));

	port_stats->rx_ttl_octets =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_DATA_OCTETS_OFFSET(port));

	port_stats->rx_data_octets =
	    VXGE_HAL_STATS_GET_PORTn_RX_DATA_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_OFFLOAD_OCTETS_OFFSET(port));

	port_stats->rx_offload_octets =
	    VXGE_HAL_STATS_GET_PORTn_RX_OFFLOAD_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_VLD_MCAST_FRMS_OFFSET(port));

	port_stats->rx_vld_mcast_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_VLD_MCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_VLD_BCAST_FRMS_OFFSET(port));

	port_stats->rx_vld_bcast_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_VLD_BCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_ACC_UCAST_FRMS_OFFSET(port));

	port_stats->rx_accepted_ucast_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_ACC_UCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_ACC_NUCAST_FRMS_OFFSET(port));

	port_stats->rx_accepted_nucast_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_ACC_NUCAST_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TAGGED_FRMS_OFFSET(port));

	port_stats->rx_tagged_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TAGGED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_LONG_FRMS_OFFSET(port));

	port_stats->rx_long_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_LONG_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_USIZED_FRMS_OFFSET(port));

	port_stats->rx_usized_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_USIZED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_OSIZED_FRMS_OFFSET(port));

	port_stats->rx_osized_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_OSIZED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_FRAG_FRMS_OFFSET(port));

	port_stats->rx_frag_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_FRAG_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_JABBER_FRMS_OFFSET(port));

	port_stats->rx_jabber_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_JABBER_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_64_FRMS_OFFSET(port));

	port_stats->rx_ttl_64_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_64_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_65_127_FRMS_OFFSET(port));

	port_stats->rx_ttl_65_127_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_65_127_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_128_255_FRMS_OFFSET(port));

	port_stats->rx_ttl_128_255_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_128_255_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_256_511_FRMS_OFFSET(port));

	port_stats->rx_ttl_256_511_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_256_511_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_512_1023_FRMS_OFFSET(port));

	port_stats->rx_ttl_512_1023_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_512_1023_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_1024_1518_FRMS_OFFSET(port));

	port_stats->rx_ttl_1024_1518_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_1024_1518_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_1519_4095_FRMS_OFFSET(port));

	port_stats->rx_ttl_1519_4095_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_1519_4095_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_4096_81915_FRMS_OFFSET(port));

	port_stats->rx_ttl_4096_8191_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_4096_8191_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_8192_MAX_FRMS_OFFSET(port));

	port_stats->rx_ttl_8192_max_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_8192_MAX_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TTL_GT_MAX_FRMS_OFFSET(port));

	port_stats->rx_ttl_gt_max_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_TTL_GT_MAX_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_IP_OFFSET(port));

	port_stats->rx_ip = VXGE_HAL_STATS_GET_PORTn_RX_IP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_ACC_IP_OFFSET(port));

	port_stats->rx_accepted_ip =
	    VXGE_HAL_STATS_GET_PORTn_RX_ACC_IP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_IP_OCTETS_OFFSET(port));

	port_stats->rx_ip_octets =
	    VXGE_HAL_STATS_GET_PORTn_RX_IP_OCTETS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_ERR_IP_OFFSET(port));

	port_stats->rx_err_ip =
	    VXGE_HAL_STATS_GET_PORTn_RX_ERR_IP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_ICMP_OFFSET(port));

	port_stats->rx_icmp = VXGE_HAL_STATS_GET_PORTn_RX_ICMP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TCP_OFFSET(port));

	port_stats->rx_tcp = VXGE_HAL_STATS_GET_PORTn_RX_TCP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_UDP_OFFSET(port));

	port_stats->rx_udp = VXGE_HAL_STATS_GET_PORTn_RX_UDP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_ERR_TCP_OFFSET(port));

	port_stats->rx_err_tcp = VXGE_HAL_STATS_GET_PORTn_RX_ERR_TCP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_PAUSE_CNT_OFFSET(port));

	port_stats->rx_pause_count =
	    VXGE_HAL_STATS_GET_PORTn_RX_PAUSE_CNT(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_PAUSE_CTRL_FRMS_OFFSET(port));

	port_stats->rx_pause_ctrl_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_PAUSE_CTRL_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_UNSUP_CTRL_FRMS_OFFSET(port));

	port_stats->rx_unsup_ctrl_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_UNSUP_CTRL_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_FCS_ERR_FRMS_OFFSET(port));

	port_stats->rx_fcs_err_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_FCS_ERR_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_IN_RNG_LEN_ERR_FRMS_OFFSET(port));

	port_stats->rx_in_rng_len_err_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_IN_RNG_LEN_ERR_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_OUT_RNG_LEN_ERR_FRMS_OFFSET(port));

	port_stats->rx_out_rng_len_err_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_OUT_RNG_LEN_ERR_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_DROP_FRMS_OFFSET(port));

	port_stats->rx_drop_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_DROP_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_DISCARDED_FRMS_OFFSET(port));

	port_stats->rx_discarded_frms =
	    VXGE_HAL_STATS_GET_PORTn_RX_DISCARDED_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_DROP_IP_OFFSET(port));

	port_stats->rx_drop_ip =
	    VXGE_HAL_STATS_GET_PORTn_RX_DROP_IP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_DRP_UDP_OFFSET(port));

	port_stats->rx_drop_udp =
	    VXGE_HAL_STATS_GET_PORTn_RX_DRP_UDP(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_LACPDU_FRMS_OFFSET(port));

	port_stats->rx_lacpdu_frms =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_LACPDU_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_MRKR_PDU_FRMS_OFFSET(port));

	port_stats->rx_marker_pdu_frms =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_MRKR_PDU_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_MRKR_RESP_PDU_FRMS_OFFSET(port));

	port_stats->rx_marker_resp_pdu_frms =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_MRKR_RESP_PDU_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_UNKNOWN_PDU_FRMS_OFFSET(port));

	port_stats->rx_unknown_pdu_frms =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_UNKNOWN_PDU_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_ILLEGAL_PDU_FRMS_OFFSET(port));

	port_stats->rx_illegal_pdu_frms =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_ILLEGAL_PDU_FRMS(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_FCS_DISCARD_OFFSET(port));

	port_stats->rx_fcs_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_FCS_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_LEN_DISCARD_OFFSET(port));

	port_stats->rx_len_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_LEN_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_SWITCH_DISCARD_OFFSET(port));

	port_stats->rx_switch_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_SWITCH_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_L2_MGMT_DISCARD_OFFSET(port));

	port_stats->rx_l2_mgmt_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_L2_MGMT_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_RPA_DISCARD_OFFSET(port));

	port_stats->rx_rpa_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_RPA_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_TRASH_DISCARD_OFFSET(port));

	port_stats->rx_trash_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_TRASH_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_RTS_DISCARD_OFFSET(port));

	port_stats->rx_rts_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_RTS_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_RED_DISCARD_OFFSET(port));

	port_stats->rx_red_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_RED_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_BUFF_FULL_DISCARD_OFFSET(port));

	port_stats->rx_buff_full_discard =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_BUFF_FULL_DISCARD(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_XGMII_DATA_ERR_CNT_OFFSET(port));

	port_stats->rx_xgmii_data_err_cnt =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_XGMII_DATA_ERR_CNT(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_XGMII_CTRL_ERR_CNT_OFFSET(port));

	port_stats->rx_xgmii_ctrl_err_cnt =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_XGMII_CTRL_ERR_CNT(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_XGMII_ERR_SYM_OFFSET(port));

	port_stats->rx_xgmii_err_sym =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_XGMII_ERR_SYM(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_XGMII_CHAR1_MATCH_OFFSET(port));

	port_stats->rx_xgmii_char1_match =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_XGMII_CHAR1_MATCH(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_XGMII_CHAR2_MATCH_OFFSET(port));

	port_stats->rx_xgmii_char2_match =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_XGMII_CHAR2_MATCH(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_XGMII_COL1_MATCH_OFFSET(port));

	port_stats->rx_xgmii_column1_match =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_XGMII_COL1_MATCH(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_XGMII_COL2_MATCH_OFFSET(port));

	port_stats->rx_xgmii_column2_match =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_XGMII_COL2_MATCH(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_LOCAL_FAULT_OFFSET(port));

	port_stats->rx_local_fault =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_LOCAL_FAULT(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_REMOTE_FAULT_OFFSET(port));

	port_stats->rx_remote_fault =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_REMOTE_FAULT(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_PORT,
	    VXGE_HAL_STATS_PORTn_RX_JETTISON_OFFSET(port));

	port_stats->rx_jettison =
	    (u32) VXGE_HAL_STATS_GET_PORTn_RX_JETTISON(val64);


	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (VXGE_HAL_OK);
}


/*
 * vxge_hal_mrpcim_xmac_stats_get - Get the XMAC Statistics
 * @devh: HAL device handle.
 * @xmac_stats: Buffer to return XMAC Statistics.
 *
 * Get the XMAC Statistics
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_xmac_stats_get(vxge_hal_device_h devh,
    vxge_hal_mrpcim_xmac_stats_t *xmac_stats)
{
	u32 i;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((devh != NULL) && (xmac_stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "hldev = 0x"VXGE_OS_STXFMT", mrpcim_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) xmac_stats);


	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	status = vxge_hal_mrpcim_xmac_aggr_stats_get(devh,
	    0,
	    &xmac_stats->aggr_stats[0]);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = vxge_hal_mrpcim_xmac_aggr_stats_get(devh,
	    1,
	    &xmac_stats->aggr_stats[1]);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	for (i = 0; i < VXGE_HAL_MAC_MAX_PORTS; i++) {

		status = vxge_hal_mrpcim_xmac_port_stats_get(devh,
		    i,
		    &xmac_stats->port_stats[i]);

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

/*
 * _hal_mrpcim_stats_get - Get the mrpcim statistics using PIO
 * @hldev: hal device.
 * @mrpcim_stats: MRPCIM stats
 *
 * Returns the mrpcim stats.
 *
 * See also: vxge_hal_mrpcim_stats_enable(), vxge_hal_mrpcim_stats_disable()
 */
vxge_hal_status_e
__hal_mrpcim_stats_get(
    __hal_device_t *hldev,
    vxge_hal_mrpcim_stats_hw_info_t *mrpcim_stats)
{
	u32 i;
	u64 val64;
	vxge_hal_device_h devh = (vxge_hal_device_h) hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((hldev != NULL) && (mrpcim_stats != NULL));

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "hldev = 0x"VXGE_OS_STXFMT", mrpcim_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev, (ptr_t) mrpcim_stats);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mrpcim_debug_stats0);

	mrpcim_stats->pic_ini_rd_drop =
	    (u32) VXGE_HAL_MRPCIM_DEBUG_STATS0_GET_INI_RD_DROP(val64);

	mrpcim_stats->pic_ini_wr_drop =
	    (u32) VXGE_HAL_MRPCIM_DEBUG_STATS0_GET_INI_WR_DROP(val64);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->mrpcim_debug_stats1_vplane[i]);

		mrpcim_stats->pic_wrcrdtarb_ph_crdt_depleted_vplane[i].
		    pic_wrcrdtarb_ph_crdt_depleted = (u32)
		    VXGE_HAL_MRPCIM_DEBUG_STATS1_GET_VPLANE_WRCRDTARB_PH_CRDT_DEPLETED(
		    val64);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->mrpcim_debug_stats2_vplane[i]);

		mrpcim_stats->pic_wrcrdtarb_pd_crdt_depleted_vplane[i].
		    pic_wrcrdtarb_pd_crdt_depleted = (u32)
		    VXGE_HAL_MRPCIM_DEBUG_STATS2_GET_VPLANE_WRCRDTARB_PD_CRDT_DEPLETED(
		    val64);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->mrpcim_debug_stats3_vplane[i]);

		mrpcim_stats->pic_rdcrdtarb_nph_crdt_depleted_vplane[i].
		    pic_rdcrdtarb_nph_crdt_depleted = (u32)
		    VXGE_HAL_MRPCIM_DEBUG_STATS3_GET_VPLANE_RDCRDTARB_NPH_CRDT_DEPLETED(
		    val64);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mrpcim_debug_stats4);

	mrpcim_stats->pic_ini_rd_vpin_drop =
	    (u32) VXGE_HAL_MRPCIM_DEBUG_STATS4_GET_INI_RD_VPIN_DROP(val64);

	mrpcim_stats->pic_ini_wr_vpin_drop =
	    (u32) VXGE_HAL_MRPCIM_DEBUG_STATS4_GET_INI_WR_VPIN_DROP(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->genstats_count01);

	mrpcim_stats->pic_genstats_count0 =
	    (u32) VXGE_HAL_GENSTATS_COUNT01_GET_GENSTATS_COUNT0(val64);

	mrpcim_stats->pic_genstats_count1 =
	    (u32) VXGE_HAL_GENSTATS_COUNT01_GET_GENSTATS_COUNT1(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->genstats_count23);

	mrpcim_stats->pic_genstats_count2 =
	    (u32) VXGE_HAL_GENSTATS_COUNT23_GET_GENSTATS_COUNT2(val64);

	mrpcim_stats->pic_genstats_count3 =
	    (u32) VXGE_HAL_GENSTATS_COUNT23_GET_GENSTATS_COUNT3(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->genstats_count4);

	mrpcim_stats->pic_genstats_count4 =
	    (u32) VXGE_HAL_GENSTATS_COUNT4_GET_GENSTATS_COUNT4(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->genstats_count5);

	mrpcim_stats->pic_genstats_count5 =
	    (u32) VXGE_HAL_GENSTATS_COUNT5_GET_GENSTATS_COUNT5(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->debug_stats0);

	mrpcim_stats->pci_rstdrop_cpl =
	    (u32) VXGE_HAL_DEBUG_STATS0_GET_RSTDROP_CPL(val64);

	mrpcim_stats->pci_rstdrop_msg =
	    (u32) VXGE_HAL_DEBUG_STATS0_GET_RSTDROP_MSG(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->debug_stats1);

	mrpcim_stats->pci_rstdrop_client0 =
	    (u32) VXGE_HAL_DEBUG_STATS1_GET_RSTDROP_CLIENT0(val64);

	mrpcim_stats->pci_rstdrop_client1 =
	    (u32) VXGE_HAL_DEBUG_STATS1_GET_RSTDROP_CLIENT1(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->debug_stats2);

	mrpcim_stats->pci_rstdrop_client2 =
	    (u32) VXGE_HAL_DEBUG_STATS2_GET_RSTDROP_CLIENT2(val64);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->debug_stats3_vplane);

		mrpcim_stats->pci_depl_h_vplane[i].pci_depl_cplh =
		    (u16) VXGE_HAL_DEBUG_STATS3_GET_VPLANE_DEPL_CPLH(val64);

		mrpcim_stats->pci_depl_h_vplane[i].pci_depl_nph =
		    (u16) VXGE_HAL_DEBUG_STATS3_GET_VPLANE_DEPL_NPH(val64);

		mrpcim_stats->pci_depl_h_vplane[i].pci_depl_ph =
		    (u16) VXGE_HAL_DEBUG_STATS3_GET_VPLANE_DEPL_PH(val64);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->debug_stats4_vplane);

		mrpcim_stats->pci_depl_d_vplane[i].pci_depl_cpld =
		    (u16) VXGE_HAL_DEBUG_STATS4_GET_VPLANE_DEPL_CPLD(val64);

		mrpcim_stats->pci_depl_d_vplane[i].pci_depl_npd =
		    (u16) VXGE_HAL_DEBUG_STATS4_GET_VPLANE_DEPL_NPD(val64);

		mrpcim_stats->pci_depl_d_vplane[i].pci_depl_pd =
		    (u16) VXGE_HAL_DEBUG_STATS4_GET_VPLANE_DEPL_PD(val64);
	}

	status = vxge_hal_mrpcim_xmac_aggr_stats_get(hldev,
	    0,
	    &mrpcim_stats->xgmac_aggr[0]);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = vxge_hal_mrpcim_xmac_aggr_stats_get(hldev,
	    1,
	    &mrpcim_stats->xgmac_aggr[1]);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	for (i = 0; i < VXGE_HAL_MAC_MAX_PORTS; i++) {

		status = vxge_hal_mrpcim_xmac_port_stats_get(hldev,
		    i,
		    &mrpcim_stats->xgmac_port[i]);

		if (status != VXGE_HAL_OK) {
			vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

	}

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_GLOBAL_PROG_EVENT_GNUM0_OFFSET);

	mrpcim_stats->xgmac_global_prog_event_gnum0 =
	    VXGE_HAL_STATS_GET_GLOBAL_PROG_EVENT_GNUM0(val64);

	VXGE_HAL_MRPCIM_STATS_PIO_READ(VXGE_HAL_STATS_LOC_AGGR,
	    VXGE_HAL_STATS_GLOBAL_PROG_EVENT_GNUM1_OFFSET);

	mrpcim_stats->xgmac_global_prog_event_gnum1 =
	    VXGE_HAL_STATS_GET_GLOBAL_PROG_EVENT_GNUM1(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->orp_lro_events);

	mrpcim_stats->xgmac_orp_lro_events =
	    VXGE_HAL_ORP_LRO_EVENTS_GET_ORP_LRO_EVENTS(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->orp_bs_events);

	mrpcim_stats->xgmac_orp_bs_events =
	    VXGE_HAL_ORP_BS_EVENTS_GET_ORP_BS_EVENTS(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->orp_iwarp_events);

	mrpcim_stats->xgmac_orp_iwarp_events =
	    VXGE_HAL_ORP_IWARP_EVENTS_GET_ORP_IWARP_EVENTS(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->dbg_stats_tpa_tx_path);

	mrpcim_stats->xgmac_tx_permitted_frms =
	    (u32) VXGE_HAL_DBG_STATS_TPA_TX_PATH_GET_TX_PERMITTED_FRMS(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->dbg_stat_tx_any_frms);

	mrpcim_stats->xgmac_port0_tx_any_frms =
	    (u8) VXGE_HAL_DBG_STAT_TX_ANY_FRMS_GET_PORT0_TX_ANY_FRMS(val64);

	mrpcim_stats->xgmac_port1_tx_any_frms =
	    (u8) VXGE_HAL_DBG_STAT_TX_ANY_FRMS_GET_PORT1_TX_ANY_FRMS(val64);

	mrpcim_stats->xgmac_port2_tx_any_frms =
	    (u8) VXGE_HAL_DBG_STAT_TX_ANY_FRMS_GET_PORT2_TX_ANY_FRMS(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->dbg_stat_rx_any_frms);

	mrpcim_stats->xgmac_port0_rx_any_frms =
	    (u8) VXGE_HAL_DBG_STAT_RX_ANY_FRMS_GET_PORT0_RX_ANY_FRMS(val64);

	mrpcim_stats->xgmac_port1_rx_any_frms =
	    (u8) VXGE_HAL_DBG_STAT_RX_ANY_FRMS_GET_PORT1_RX_ANY_FRMS(val64);

	mrpcim_stats->xgmac_port2_rx_any_frms =
	    (u8) VXGE_HAL_DBG_STAT_RX_ANY_FRMS_GET_PORT2_RX_ANY_FRMS(val64);

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_mrpcim_stats_clear - Clear the statistics of the device
 * @devh: HAL Device handle.
 *
 * Clear the statistics of the given Device.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_stats_clear(vxge_hal_device_h devh)
{
	u32 i;
	u64 stat;
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	vxge_os_memcpy(&hldev->mrpcim->mrpcim_stats_sav,
	    hldev->mrpcim->mrpcim_stats,
	    sizeof(vxge_hal_mrpcim_stats_hw_info_t));

	vxge_os_memzero(hldev->mrpcim->mrpcim_stats,
	    sizeof(vxge_hal_mrpcim_stats_hw_info_t));

	vxge_os_memzero(&hldev->stats.sw_dev_err_stats,
	    sizeof(vxge_hal_device_stats_sw_err_t));

	hldev->stats.sw_dev_info_stats.soft_reset_cnt = 0;

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		(void) vxge_hal_vpath_stats_clear(
		    VXGE_HAL_VIRTUAL_PATH_HANDLE(&hldev->virtual_paths[i]));

	}

	status = vxge_hal_mrpcim_stats_access(
	    devh,
	    VXGE_HAL_STATS_OP_CLEAR_ALL_STATS,
	    0,
	    0,
	    &stat);

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_mrpcim_udp_rth_enable - Enable UDP/RTH.
 * @devh: HAL device handle.
 *
 * enable udp rth
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_udp_rth_enable(
    vxge_hal_device_h devh)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {

		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	}

	status = __hal_vpath_udp_rth_set(hldev,
	    hldev->first_vp_id,
	    TRUE);

	vxge_hal_trace_log_stats("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_mrpcim_mac_configure - Initialize mac
 * @hldev: hal device.
 *
 * Initializes mac
 *
 */
vxge_hal_status_e
__hal_mrpcim_mac_configure(__hal_device_t *hldev)
{
	u64 val64;
	u32 i, port_id;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_mac_config_t *mac_config =
	&hldev->header.config.mrpcim_config.mac_config;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	for (i = 0; i < VXGE_HAL_MAC_MAX_WIRE_PORTS; i++) {

		port_id = mac_config->wire_port_config[i].port_id;

		if (mac_config->wire_port_config[i].tmac_en ==
		    VXGE_HAL_WIRE_PORT_TMAC_DEFAULT) {
			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &hldev->mrpcim_reg->txmac_cfg0_port[port_id]);

			if (val64 & VXGE_HAL_TXMAC_CFG0_PORT_TMAC_EN) {
				mac_config->wire_port_config[i].tmac_en =
				    VXGE_HAL_WIRE_PORT_TMAC_ENABLE;
			} else {
				mac_config->wire_port_config[i].tmac_en =
				    VXGE_HAL_WIRE_PORT_TMAC_DISABLE;
			}

		}

		if (mac_config->wire_port_config[i].rmac_en ==
		    VXGE_HAL_WIRE_PORT_RMAC_DEFAULT) {
			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &hldev->mrpcim_reg->rxmac_cfg0_port[port_id]);

			if (val64 & VXGE_HAL_RXMAC_CFG0_PORT_RMAC_EN) {
				mac_config->wire_port_config[i].rmac_en =
				    VXGE_HAL_WIRE_PORT_RMAC_ENABLE;
			} else {
				mac_config->wire_port_config[i].rmac_en =
				    VXGE_HAL_WIRE_PORT_RMAC_DISABLE;
			}

		}

		if ((!(mac_config->wire_port_config[i].rmac_en)) &&
		    (!(mac_config->wire_port_config[i].tmac_en)))
			val64 = 0;
		else
			val64 = VXGE_HAL_XGMAC_MAIN_CFG_PORT_PORT_EN;

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->xgmac_main_cfg_port[port_id]);

		if (!val64)
			continue;

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_cfg0_port[port_id]);

		if (mac_config->wire_port_config[i].rmac_en)
			val64 |= VXGE_HAL_RXMAC_CFG0_PORT_RMAC_EN;
		else
			val64 &= ~VXGE_HAL_RXMAC_CFG0_PORT_RMAC_EN;

		if (mac_config->wire_port_config[i].rmac_strip_fcs !=
		    VXGE_HAL_WIRE_PORT_RMAC_STRIP_FCS_DEFAULT) {
			if (mac_config->wire_port_config[i].rmac_strip_fcs)
				val64 |= VXGE_HAL_RXMAC_CFG0_PORT_STRIP_FCS;
			else
				val64 &= ~VXGE_HAL_RXMAC_CFG0_PORT_STRIP_FCS;
		}

		if (mac_config->wire_port_config[i].rmac_discard_pfrm !=
		    VXGE_HAL_WIRE_PORT_RMAC_DISCARD_PFRM_DEFAULT) {
			if (mac_config->wire_port_config[i].rmac_discard_pfrm)
				val64 |= VXGE_HAL_RXMAC_CFG0_PORT_DISCARD_PFRM;
			else
				val64 &= ~VXGE_HAL_RXMAC_CFG0_PORT_DISCARD_PFRM;
		}

		if (mac_config->wire_port_config[i].mtu !=
		    VXGE_HAL_WIRE_PORT_DEF_INITIAL_MTU) {

			val64 &=
			    ~VXGE_HAL_RXMAC_CFG0_PORT_MAX_PYLD_LEN(0x3fff);

			val64 |= VXGE_HAL_RXMAC_CFG0_PORT_MAX_PYLD_LEN(
			    mac_config->wire_port_config[i].mtu);

		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rxmac_cfg0_port[port_id]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_cfg2_port[port_id]);

		if (mac_config->wire_port_config[i].rmac_prom_en !=
		    VXGE_HAL_WIRE_PORT_RMAC_PROM_EN_DEFAULT) {
			if (mac_config->wire_port_config[i].rmac_prom_en)
				val64 |= VXGE_HAL_RXMAC_CFG2_PORT_PROM_EN;
			else
				val64 &= ~VXGE_HAL_RXMAC_CFG2_PORT_PROM_EN;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rxmac_cfg2_port[port_id]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_pause_cfg_port[port_id]);

		if (mac_config->wire_port_config[i].rmac_pause_gen_en !=
		    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_GEN_EN_DEFAULT) {
			if (mac_config->wire_port_config[i].rmac_pause_gen_en)
				val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_GEN_EN;
			else
				val64 &= ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_GEN_EN;

		}

		if (mac_config->wire_port_config[i].rmac_pause_rcv_en !=
		    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_RCV_EN_DEFAULT) {
			if (mac_config->wire_port_config[i].rmac_pause_rcv_en)
				val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_RCV_EN;
			else
				val64 &= ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_RCV_EN;

		}

		if (mac_config->wire_port_config[i].rmac_pause_time !=
		    VXGE_HAL_WIRE_PORT_DEF_RMAC_HIGH_PTIME) {
			val64 &=
			    ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_HIGH_PTIME(0xffff);

			val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_HIGH_PTIME(
			    mac_config->wire_port_config[i].rmac_pause_time);

		}

		if (mac_config->wire_port_config[i].rmac_pause_time !=
		    VXGE_HAL_WIRE_PORT_RMAC_PAUSE_LIMITER_DEFAULT) {
			if (mac_config->wire_port_config[i].limiter_en)
				val64 |=
				    VXGE_HAL_RXMAC_PAUSE_CFG_PORT_LIMITER_EN;
			else
				val64 &=
				    ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_LIMITER_EN;

		}

		if (mac_config->wire_port_config[i].max_limit !=
		    VXGE_HAL_WIRE_PORT_DEF_RMAC_MAX_LIMIT) {
			val64 &= ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_MAX_LIMIT(0xff);

			val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_MAX_LIMIT(
			    mac_config->wire_port_config[i].max_limit);

		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rxmac_pause_cfg_port[port_id]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_link_util_port[port_id]);

		if (mac_config->wire_port_config[i].rmac_util_period !=
		    VXGE_HAL_WIRE_PORT_DEF_TMAC_UTIL_PERIOD) {
			val64 &=
			    ~VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_UTIL_CFG(0xf);

			val64 |= VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_UTIL_CFG(
			    mac_config->wire_port_config[i].rmac_util_period);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rxmac_link_util_port[port_id]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->xgmac_debounce_port[port_id]);

		if (mac_config->wire_port_config[i].link_stability_period !=
		    VXGE_HAL_WIRE_PORT_DEF_LINK_STABILITY_PERIOD) {
			val64 &=
			    ~(VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_LINK_UP(0xf) |
			    VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_LINK_DOWN(0xf));

			val64 |= VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_LINK_UP(
			    mac_config->wire_port_config[i].link_stability_period) |
			    VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_LINK_DOWN(
			    mac_config->wire_port_config[i].link_stability_period);
		}

		if (mac_config->wire_port_config[i].port_stability_period !=
		    VXGE_HAL_WIRE_PORT_DEF_PORT_STABILITY_PERIOD) {
			val64 &=
			    ~(VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_PORT_UP(0xf) |
			    VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_PORT_DOWN(0xf));

			val64 |= VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_PORT_UP(
			    mac_config->wire_port_config[i].port_stability_period) |
			    VXGE_HAL_XGMAC_DEBOUNCE_PORT_PERIOD_PORT_DOWN(
			    mac_config->wire_port_config[i].port_stability_period);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->xgmac_debounce_port[port_id]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->txmac_cfg0_port[port_id]);

		if (mac_config->wire_port_config[i].tmac_en)
			val64 |= VXGE_HAL_TXMAC_CFG0_PORT_TMAC_EN;
		else
			val64 &= ~VXGE_HAL_TXMAC_CFG0_PORT_TMAC_EN;

		if (mac_config->wire_port_config[i].tmac_pad !=
		    VXGE_HAL_WIRE_PORT_TMAC_PAD_DEFAULT) {
			if (mac_config->wire_port_config[i].tmac_pad)
				val64 |= VXGE_HAL_TXMAC_CFG0_PORT_APPEND_PAD;
			else
				val64 &= ~VXGE_HAL_TXMAC_CFG0_PORT_APPEND_PAD;
		}

		if (mac_config->wire_port_config[i].tmac_pad_byte !=
		    VXGE_HAL_WIRE_PORT_TMAC_PAD_DEFAULT) {
			val64 &= ~VXGE_HAL_TXMAC_CFG0_PORT_PAD_BYTE(0xff);

			val64 |= VXGE_HAL_TXMAC_CFG0_PORT_PAD_BYTE(
			    mac_config->wire_port_config[i].tmac_pad_byte);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->txmac_cfg0_port[port_id]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->txmac_link_util_port);

		if (mac_config->wire_port_config[i].tmac_util_period !=
		    VXGE_HAL_WIRE_PORT_DEF_TMAC_UTIL_PERIOD) {
			val64 &=
			    ~VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_UTIL_CFG(0xf);

			val64 |= VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_UTIL_CFG(
			    mac_config->wire_port_config[i].tmac_util_period);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->txmac_link_util_port[port_id]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->ratemgmt_cfg_port);

		if (mac_config->wire_port_config[i].autoneg_mode !=
		    VXGE_HAL_WIRE_PORT_AUTONEG_MODE_DEFAULT) {

			val64 &= ~VXGE_HAL_RATEMGMT_CFG_PORT_MODE(0x3);

			val64 |= VXGE_HAL_RATEMGMT_CFG_PORT_MODE(
			    mac_config->wire_port_config[i].autoneg_mode);
		}

		if (mac_config->wire_port_config[i].autoneg_rate !=
		    VXGE_HAL_WIRE_PORT_AUTONEG_RATE_DEFAULT) {

			if (mac_config->wire_port_config[i].autoneg_rate)
				val64 |= VXGE_HAL_RATEMGMT_CFG_PORT_RATE;
			else
				val64 &= ~VXGE_HAL_RATEMGMT_CFG_PORT_RATE;

		}

		if (mac_config->wire_port_config[i].fixed_use_fsm !=
		    VXGE_HAL_WIRE_PORT_FIXED_USE_FSM_DEFAULT) {

			if (mac_config->wire_port_config[i].fixed_use_fsm)
				val64 |=
				    VXGE_HAL_RATEMGMT_CFG_PORT_FIXED_USE_FSM;
			else
				val64 &=
				    ~VXGE_HAL_RATEMGMT_CFG_PORT_FIXED_USE_FSM;

		}

		if (mac_config->wire_port_config[i].antp_use_fsm !=
		    VXGE_HAL_WIRE_PORT_ANTP_USE_FSM_DEFAULT) {

			if (mac_config->wire_port_config[i].antp_use_fsm)
				val64 |=
				    VXGE_HAL_RATEMGMT_CFG_PORT_ANTP_USE_FSM;
			else
				val64 &=
				    ~VXGE_HAL_RATEMGMT_CFG_PORT_ANTP_USE_FSM;

		}

		if (mac_config->wire_port_config[i].anbe_use_fsm !=
		    VXGE_HAL_WIRE_PORT_ANBE_USE_FSM_DEFAULT) {

			if (mac_config->wire_port_config[i].anbe_use_fsm)
				val64 |=
				    VXGE_HAL_RATEMGMT_CFG_PORT_ANBE_USE_FSM;
			else
				val64 &=
				    ~VXGE_HAL_RATEMGMT_CFG_PORT_ANBE_USE_FSM;

		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->ratemgmt_cfg_port[port_id]);

	}

	if (mac_config->switch_port_config.tmac_en ==
	    VXGE_HAL_SWITCH_PORT_TMAC_DEFAULT) {
		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->txmac_cfg0_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		if (val64 & VXGE_HAL_TXMAC_CFG0_PORT_TMAC_EN) {
			mac_config->switch_port_config.tmac_en =
			    VXGE_HAL_SWITCH_PORT_TMAC_ENABLE;
		} else {
			mac_config->switch_port_config.tmac_en =
			    VXGE_HAL_SWITCH_PORT_TMAC_DISABLE;
		}

	}

	if (mac_config->switch_port_config.rmac_en ==
	    VXGE_HAL_SWITCH_PORT_RMAC_DEFAULT) {
		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_cfg0_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		if (val64 & VXGE_HAL_RXMAC_CFG0_PORT_RMAC_EN) {
			mac_config->switch_port_config.rmac_en =
			    VXGE_HAL_SWITCH_PORT_RMAC_ENABLE;
		} else {
			mac_config->switch_port_config.rmac_en =
			    VXGE_HAL_SWITCH_PORT_RMAC_DISABLE;
		}

	}

	if (mac_config->switch_port_config.rmac_en ||
	    mac_config->switch_port_config.tmac_en) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_cfg0_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		if (mac_config->switch_port_config.rmac_en)
			val64 |= VXGE_HAL_RXMAC_CFG0_PORT_RMAC_EN;
		else
			val64 &= ~VXGE_HAL_RXMAC_CFG0_PORT_RMAC_EN;

		if (mac_config->switch_port_config.rmac_strip_fcs !=
		    VXGE_HAL_SWITCH_PORT_RMAC_STRIP_FCS_DEFAULT) {
			if (mac_config->switch_port_config.rmac_strip_fcs)
				val64 |= VXGE_HAL_RXMAC_CFG0_PORT_STRIP_FCS;
			else
				val64 &= ~VXGE_HAL_RXMAC_CFG0_PORT_STRIP_FCS;
		}

		if (mac_config->switch_port_config.rmac_discard_pfrm !=
		    VXGE_HAL_SWITCH_PORT_RMAC_DISCARD_PFRM_DEFAULT) {
			if (mac_config->switch_port_config.rmac_discard_pfrm)
				val64 |= VXGE_HAL_RXMAC_CFG0_PORT_DISCARD_PFRM;
			else
				val64 &= ~VXGE_HAL_RXMAC_CFG0_PORT_DISCARD_PFRM;
		}

		if (mac_config->switch_port_config.mtu !=
		    VXGE_HAL_SWITCH_PORT_DEF_INITIAL_MTU) {

			val64 &= ~VXGE_HAL_RXMAC_CFG0_PORT_MAX_PYLD_LEN(0x3fff);

			val64 |= VXGE_HAL_RXMAC_CFG0_PORT_MAX_PYLD_LEN(
			    mac_config->switch_port_config.mtu);

		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rxmac_cfg0_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_cfg2_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		if (mac_config->switch_port_config.rmac_prom_en !=
		    VXGE_HAL_SWITCH_PORT_RMAC_PROM_EN_DEFAULT) {
			if (mac_config->switch_port_config.rmac_prom_en)
				val64 |= VXGE_HAL_RXMAC_CFG2_PORT_PROM_EN;
			else
				val64 &= ~VXGE_HAL_RXMAC_CFG2_PORT_PROM_EN;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rxmac_cfg2_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_pause_cfg_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		if (mac_config->switch_port_config.rmac_pause_gen_en !=
		    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_GEN_EN_DEFAULT) {
			if (mac_config->switch_port_config.rmac_pause_gen_en)
				val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_GEN_EN;
			else
				val64 &= ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_GEN_EN;

		}

		if (mac_config->switch_port_config.rmac_pause_rcv_en !=
		    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_RCV_EN_DEFAULT) {
			if (mac_config->switch_port_config.rmac_pause_rcv_en)
				val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_RCV_EN;
			else
				val64 &= ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_RCV_EN;

		}

		if (mac_config->switch_port_config.rmac_pause_time !=
		    VXGE_HAL_SWITCH_PORT_DEF_RMAC_HIGH_PTIME) {
			val64 &=
			    ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_HIGH_PTIME(0xffff);

			val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_HIGH_PTIME(
			    mac_config->switch_port_config.rmac_pause_time);

		}

		if (mac_config->switch_port_config.rmac_pause_time !=
		    VXGE_HAL_SWITCH_PORT_RMAC_PAUSE_LIMITER_DEFAULT) {
			if (mac_config->switch_port_config.limiter_en)
				val64 |=
				    VXGE_HAL_RXMAC_PAUSE_CFG_PORT_LIMITER_EN;
			else
				val64 &=
				    ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_LIMITER_EN;

		}

		if (mac_config->switch_port_config.max_limit !=
		    VXGE_HAL_SWITCH_PORT_DEF_RMAC_MAX_LIMIT) {
			val64 &= ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_MAX_LIMIT(0xff);

			val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_MAX_LIMIT(
			    mac_config->switch_port_config.max_limit);

		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rxmac_pause_cfg_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rxmac_link_util_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		if (mac_config->switch_port_config.rmac_util_period !=
		    VXGE_HAL_SWITCH_PORT_DEF_TMAC_UTIL_PERIOD) {
			val64 &=
			    ~VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_UTIL_CFG(0xf);

			val64 |= VXGE_HAL_RXMAC_LINK_UTIL_PORT_RMAC_UTIL_CFG(
			    mac_config->switch_port_config.rmac_util_period);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rxmac_link_util_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->txmac_cfg0_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		if (mac_config->switch_port_config.tmac_en)
			val64 |= VXGE_HAL_TXMAC_CFG0_PORT_TMAC_EN;
		else
			val64 &= ~VXGE_HAL_TXMAC_CFG0_PORT_TMAC_EN;

		if (mac_config->switch_port_config.tmac_pad !=
		    VXGE_HAL_SWITCH_PORT_TMAC_PAD_DEFAULT) {
			if (mac_config->switch_port_config.tmac_pad)
				val64 |= VXGE_HAL_TXMAC_CFG0_PORT_APPEND_PAD;
			else
				val64 &= ~VXGE_HAL_TXMAC_CFG0_PORT_APPEND_PAD;
		}

		if (mac_config->switch_port_config.tmac_pad_byte !=
		    VXGE_HAL_SWITCH_PORT_TMAC_PAD_DEFAULT) {
			val64 &= ~VXGE_HAL_TXMAC_CFG0_PORT_PAD_BYTE(0xff);

			val64 |= VXGE_HAL_TXMAC_CFG0_PORT_PAD_BYTE(
			    mac_config->switch_port_config.tmac_pad_byte);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->txmac_cfg0_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->txmac_link_util_port);

		if (mac_config->switch_port_config.tmac_util_period !=
		    VXGE_HAL_SWITCH_PORT_DEF_TMAC_UTIL_PERIOD) {
			val64 &=
			    ~VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_UTIL_CFG(0xf);

			val64 |= VXGE_HAL_TXMAC_LINK_UTIL_PORT_TMAC_UTIL_CFG(
			    mac_config->switch_port_config.tmac_util_period);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->txmac_link_util_port[
		    VXGE_HAL_MAC_SWITCH_PORT]);

	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->txmac_gen_cfg1);

	if (mac_config->tmac_perma_stop_en !=
	    VXGE_HAL_MAC_TMAC_PERMA_STOP_DEFAULT) {

		if (mac_config->tmac_perma_stop_en)
			val64 |= VXGE_HAL_TXMAC_GEN_CFG1_TMAC_PERMA_STOP_EN;
		else
			val64 &= ~VXGE_HAL_TXMAC_GEN_CFG1_TMAC_PERMA_STOP_EN;

	}

	if (mac_config->tmac_tx_switch_dis !=
	    VXGE_HAL_MAC_TMAC_TX_SWITCH_DEFAULT) {

		if (mac_config->tmac_tx_switch_dis)
			val64 |= VXGE_HAL_TXMAC_GEN_CFG1_TX_SWITCH_DISABLE;
		else
			val64 &= ~VXGE_HAL_TXMAC_GEN_CFG1_TX_SWITCH_DISABLE;

	}

	if (mac_config->tmac_lossy_switch_en !=
	    VXGE_HAL_MAC_TMAC_LOSSY_SWITCH_DEFAULT) {

		if (mac_config->tmac_lossy_switch_en)
			val64 |= VXGE_HAL_TXMAC_GEN_CFG1_LOSSY_SWITCH;
		else
			val64 &= ~VXGE_HAL_TXMAC_GEN_CFG1_LOSSY_SWITCH;

	}

	if (mac_config->tmac_lossy_switch_en !=
	    VXGE_HAL_MAC_TMAC_LOSSY_WIRE_DEFAULT) {

		if (mac_config->tmac_lossy_wire_en)
			val64 |= VXGE_HAL_TXMAC_GEN_CFG1_LOSSY_WIRE;
		else
			val64 &= ~VXGE_HAL_TXMAC_GEN_CFG1_LOSSY_WIRE;

	}

	if (mac_config->tmac_bcast_to_wire_dis !=
	    VXGE_HAL_MAC_TMAC_BCAST_TO_WIRE_DEFAULT) {

		if (mac_config->tmac_bcast_to_wire_dis)
			val64 |= VXGE_HAL_TXMAC_GEN_CFG1_BLOCK_BCAST_TO_WIRE;
		else
			val64 &= ~VXGE_HAL_TXMAC_GEN_CFG1_BLOCK_BCAST_TO_WIRE;

	}

	if (mac_config->tmac_bcast_to_wire_dis !=
	    VXGE_HAL_MAC_TMAC_BCAST_TO_SWITCH_DEFAULT) {

		if (mac_config->tmac_bcast_to_switch_dis)
			val64 |= VXGE_HAL_TXMAC_GEN_CFG1_BLOCK_BCAST_TO_SWITCH;
		else
			val64 &= ~VXGE_HAL_TXMAC_GEN_CFG1_BLOCK_BCAST_TO_SWITCH;

	}

	if (mac_config->tmac_host_append_fcs_en !=
	    VXGE_HAL_MAC_TMAC_HOST_APPEND_FCS_DEFAULT) {

		if (mac_config->tmac_host_append_fcs_en)
			val64 |= VXGE_HAL_TXMAC_GEN_CFG1_HOST_APPEND_FCS;
		else
			val64 &= ~VXGE_HAL_TXMAC_GEN_CFG1_HOST_APPEND_FCS;

	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->txmac_gen_cfg1);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->rxmac_rx_pa_cfg0);

	if (mac_config->rpa_ignore_frame_err !=
	    VXGE_HAL_MAC_RPA_IGNORE_FRAME_ERR_DEFAULT) {

		if (mac_config->rpa_ignore_frame_err)
			val64 |= VXGE_HAL_RXMAC_RX_PA_CFG0_IGNORE_FRAME_ERR;
		else
			val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG0_IGNORE_FRAME_ERR;

	}

	if (mac_config->rpa_support_snap_ab_n !=
	    VXGE_HAL_MAC_RPA_SUPPORT_SNAP_AB_N_DEFAULT) {

		if (mac_config->rpa_support_snap_ab_n)
			val64 |= VXGE_HAL_RXMAC_RX_PA_CFG0_SUPPORT_SNAP_AB_N;
		else
			val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG0_SUPPORT_SNAP_AB_N;

	}

	if (mac_config->rpa_search_for_hao !=
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_HAO_DEFAULT) {

		if (mac_config->rpa_search_for_hao)
			val64 |= VXGE_HAL_RXMAC_RX_PA_CFG0_SEARCH_FOR_HAO;
		else
			val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG0_SEARCH_FOR_HAO;

	}

	if (mac_config->rpa_support_ipv6_mobile_hdrs !=
	    VXGE_HAL_MAC_RPA_SUPPORT_IPV6_MOBILE_HDRS_DEFAULT) {

		if (mac_config->rpa_support_ipv6_mobile_hdrs)
			val64 |=
			    VXGE_HAL_RXMAC_RX_PA_CFG0_SUPPORT_MOBILE_IPV6_HDRS;
		else
			val64 &=
			    ~VXGE_HAL_RXMAC_RX_PA_CFG0_SUPPORT_MOBILE_IPV6_HDRS;

	}

	if (mac_config->rpa_ipv6_stop_searching !=
	    VXGE_HAL_MAC_RPA_IPV6_STOP_SEARCHING_DEFAULT) {

		if (mac_config->rpa_ipv6_stop_searching)
			val64 |= VXGE_HAL_RXMAC_RX_PA_CFG0_IPV6_STOP_SEARCHING;
		else
			val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG0_IPV6_STOP_SEARCHING;

	}

	if (mac_config->rpa_no_ps_if_unknown !=
	    VXGE_HAL_MAC_RPA_NO_PS_IF_UNKNOWN_DEFAULT) {

		if (mac_config->rpa_no_ps_if_unknown)
			val64 |= VXGE_HAL_RXMAC_RX_PA_CFG0_NO_PS_IF_UNKNOWN;
		else
			val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG0_NO_PS_IF_UNKNOWN;

	}

	if (mac_config->rpa_search_for_etype !=
	    VXGE_HAL_MAC_RPA_SEARCH_FOR_ETYPE_DEFAULT) {

		if (mac_config->rpa_search_for_etype)
			val64 |= VXGE_HAL_RXMAC_RX_PA_CFG0_SEARCH_FOR_ETYPE;
		else
			val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG0_SEARCH_FOR_ETYPE;

	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->rxmac_rx_pa_cfg0);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->fau_pa_cfg);

	if (mac_config->rpa_repl_l4_comp_csum !=
	    VXGE_HAL_MAC_RPA_REPL_l4_COMP_CSUM_DEFAULT) {

		if (mac_config->rpa_repl_l4_comp_csum)
			val64 |= VXGE_HAL_FAU_PA_CFG_REPL_L4_COMP_CSUM;
		else
			val64 &= ~VXGE_HAL_FAU_PA_CFG_REPL_L4_COMP_CSUM;

	}

	if (mac_config->rpa_repl_l3_incl_cf !=
	    VXGE_HAL_MAC_RPA_REPL_L3_INCL_CF_DEFAULT) {

		if (mac_config->rpa_repl_l3_incl_cf)
			val64 |= VXGE_HAL_FAU_PA_CFG_REPL_L3_INCL_CF;
		else
			val64 &= ~VXGE_HAL_FAU_PA_CFG_REPL_L3_INCL_CF;

	}

	if (mac_config->rpa_repl_l3_comp_csum !=
	    VXGE_HAL_MAC_RPA_REPL_l3_COMP_CSUM_DEFAULT) {

		if (mac_config->rpa_repl_l3_comp_csum)
			val64 |= VXGE_HAL_FAU_PA_CFG_REPL_L3_COMP_CSUM;
		else
			val64 &= ~VXGE_HAL_FAU_PA_CFG_REPL_L3_COMP_CSUM;

	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->fau_pa_cfg);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->rxmac_rx_pa_cfg1);

	if (mac_config->rpa_repl_ipv4_tcp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV4_TCP_INCL_PH_DEFAULT) {

		if (mac_config->rpa_repl_ipv4_tcp_incl_ph)
			val64 |=
			    VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV4_TCP_INCL_PH;
		else
			val64 &=
			    ~VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV4_TCP_INCL_PH;

	}

	if (mac_config->rpa_repl_ipv6_tcp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV6_TCP_INCL_PH_DEFAULT) {

		if (mac_config->rpa_repl_ipv6_tcp_incl_ph)
			val64 |=
			    VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV6_TCP_INCL_PH;
		else
			val64 &=
			    ~VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV6_TCP_INCL_PH;

	}

	if (mac_config->rpa_repl_ipv4_udp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV4_UDP_INCL_PH_DEFAULT) {

		if (mac_config->rpa_repl_ipv4_udp_incl_ph)
			val64 |=
			    VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV4_UDP_INCL_PH;
		else
			val64 &=
			    ~VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV4_UDP_INCL_PH;

	}

	if (mac_config->rpa_repl_ipv6_udp_incl_ph !=
	    VXGE_HAL_MAC_RPA_REPL_IPV6_UDP_INCL_PH_DEFAULT) {

		if (mac_config->rpa_repl_ipv6_udp_incl_ph)
			val64 |=
			    VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV6_UDP_INCL_PH;
		else
			val64 &=
			    ~VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_IPV6_UDP_INCL_PH;

	}

	if (mac_config->rpa_repl_l4_incl_cf !=
	    VXGE_HAL_MAC_RPA_REPL_L4_INCL_CF_DEFAULT) {

		if (mac_config->rpa_repl_l4_incl_cf)
			val64 |= VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_L4_INCL_CF;
		else
			val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_L4_INCL_CF;

	}

	if (mac_config->rpa_repl_strip_vlan_tag !=
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_DEFAULT) {

		if (mac_config->rpa_repl_strip_vlan_tag)
			val64 |= VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_STRIP_VLAN_TAG;
		else
			val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_STRIP_VLAN_TAG;


	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->rxmac_rx_pa_cfg1);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->xmac_gen_cfg);

	if (mac_config->network_stability_period !=
	    VXGE_HAL_MAC_DEF_NETWORK_STABILITY_PERIOD) {

		val64 &= ~(VXGE_HAL_XMAC_GEN_CFG_PERIOD_NTWK_DOWN(0xf) |
		    VXGE_HAL_XMAC_GEN_CFG_PERIOD_NTWK_UP(0xf));

		val64 |= VXGE_HAL_XMAC_GEN_CFG_PERIOD_NTWK_DOWN(
		    mac_config->network_stability_period) |
		    VXGE_HAL_XMAC_GEN_CFG_PERIOD_NTWK_UP(
		    mac_config->network_stability_period);

	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->xmac_gen_cfg);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->tpa_global_cfg);

	if (mac_config->tpa_support_snap_ab_n !=
	    VXGE_HAL_MAC_TPA_SUPPORT_SNAP_AB_N_DEFAULT) {

		if (mac_config->tpa_support_snap_ab_n)
			val64 |= VXGE_HAL_TPA_GLOBAL_CFG_SUPPORT_SNAP_AB_N;
		else
			val64 &= ~VXGE_HAL_TPA_GLOBAL_CFG_SUPPORT_SNAP_AB_N;

	}

	if (mac_config->tpa_ecc_enable_n !=
	    VXGE_HAL_MAC_TPA_ECC_ENABLE_N_DEFAULT) {

		if (mac_config->tpa_ecc_enable_n)
			val64 |= VXGE_HAL_TPA_GLOBAL_CFG_ECC_ENABLE_N;
		else
			val64 &= ~VXGE_HAL_TPA_GLOBAL_CFG_ECC_ENABLE_N;

	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->tpa_global_cfg);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);

}

/*
 * __hal_mrpcim_lag_configure - Initialize LAG registers
 * @hldev: hal device.
 *
 * Initializes LAG registers
 *
 */
vxge_hal_status_e
__hal_mrpcim_lag_configure(__hal_device_t *hldev)
{
	u64 val64;
	u64 mac_addr;
	u32 i, j;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_lag_config_t *lag_config =
	&hldev->header.config.mrpcim_config.lag_config;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);


	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_cfg);

	if (lag_config->lag_en == VXGE_HAL_LAG_LAG_EN_DEFAULT) {

		if (val64 & VXGE_HAL_LAG_CFG_EN)
			lag_config->lag_en = VXGE_HAL_LAG_LAG_EN_ENABLE;
		else
			lag_config->lag_en = VXGE_HAL_LAG_LAG_EN_DISABLE;

	}

	if (lag_config->lag_en == VXGE_HAL_LAG_LAG_EN_DISABLE) {

		if (val64 & VXGE_HAL_LAG_CFG_EN) {
			val64 &= ~VXGE_HAL_LAG_CFG_EN;
			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    val64,
			    &hldev->mrpcim_reg->lag_cfg);
		}

		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);

		return (VXGE_HAL_OK);

	}

	if (lag_config->lag_mode != VXGE_HAL_LAG_LAG_MODE_DEFAULT) {
		val64 &= ~VXGE_HAL_LAG_CFG_MODE(0x3);
		val64 |= VXGE_HAL_LAG_CFG_MODE(lag_config->lag_mode);
	} else {
		lag_config->lag_mode = (u32) VXGE_HAL_LAG_CFG_GET_MODE(val64);
	}

	if (lag_config->la_mode_config.tx_discard !=
	    VXGE_HAL_LAG_TX_DISCARD_DEFAULT) {
		if (lag_config->la_mode_config.tx_discard ==
		    VXGE_HAL_LAG_TX_DISCARD_ENABLE)
			val64 |= VXGE_HAL_LAG_CFG_TX_DISCARD_BEHAV;
		else
			val64 &= ~VXGE_HAL_LAG_CFG_TX_DISCARD_BEHAV;
	}

	if (lag_config->la_mode_config.rx_discard !=
	    VXGE_HAL_LAG_RX_DISCARD_DEFAULT) {
		if (lag_config->la_mode_config.rx_discard ==
		    VXGE_HAL_LAG_RX_DISCARD_ENABLE)
			val64 |= VXGE_HAL_LAG_CFG_RX_DISCARD_BEHAV;
		else
			val64 &= ~VXGE_HAL_LAG_CFG_RX_DISCARD_BEHAV;
	}

	if (lag_config->sl_mode_config.pref_indiv_port !=
	    VXGE_HAL_LAG_PREF_INDIV_PORT_DEFAULT) {
		if (lag_config->sl_mode_config.pref_indiv_port ==
		    VXGE_HAL_LAG_RX_DISCARD_ENABLE)
			val64 |= VXGE_HAL_LAG_CFG_PREF_INDIV_PORT_NUM;
		else
			val64 &= ~VXGE_HAL_LAG_CFG_PREF_INDIV_PORT_NUM;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_cfg);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_tx_cfg);

	if (lag_config->incr_tx_aggr_stats !=
	    VXGE_HAL_LAG_INCR_TX_AGGR_STATS_DEFAULT) {
		if (lag_config->incr_tx_aggr_stats ==
		    VXGE_HAL_LAG_INCR_TX_AGGR_STATS_ENABLE)
			val64 |= VXGE_HAL_LAG_TX_CFG_INCR_TX_AGGR_STATS;
		else
			val64 &= ~VXGE_HAL_LAG_TX_CFG_INCR_TX_AGGR_STATS;
	}

	if (lag_config->la_mode_config.distrib_alg_sel !=
	    VXGE_HAL_LAG_DISTRIB_ALG_SEL_DEFAULT) {
		val64 &= ~VXGE_HAL_LAG_TX_CFG_DISTRIB_ALG_SEL(0x3);
		val64 |= VXGE_HAL_LAG_TX_CFG_DISTRIB_ALG_SEL(
		    lag_config->la_mode_config.distrib_alg_sel);
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    lag_config->la_mode_config.distrib_dest,
		    &hldev->mrpcim_reg->lag_distrib_dest);
	} else {
		lag_config->la_mode_config.distrib_alg_sel =
		    (u32) VXGE_HAL_LAG_TX_CFG_GET_DISTRIB_ALG_SEL(val64);
		lag_config->la_mode_config.distrib_dest =
		    vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_distrib_dest);
	}

	if (lag_config->la_mode_config.distrib_remap_if_fail !=
	    VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_DEFAULT) {
		if (lag_config->la_mode_config.distrib_remap_if_fail ==
		    VXGE_HAL_LAG_DISTRIB_REMAP_IF_FAIL_ENABLE)
			val64 |= VXGE_HAL_LAG_TX_CFG_DISTRIB_REMAP_IF_FAIL;
		else
			val64 &= ~VXGE_HAL_LAG_TX_CFG_DISTRIB_REMAP_IF_FAIL;
	}

	if (lag_config->la_mode_config.coll_max_delay !=
	    VXGE_HAL_LAG_DEF_COLL_MAX_DELAY) {
		val64 &= ~VXGE_HAL_LAG_TX_CFG_COLL_MAX_DELAY(0xffff);
		val64 |= VXGE_HAL_LAG_TX_CFG_DISTRIB_ALG_SEL(
		    lag_config->la_mode_config.coll_max_delay);
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_tx_cfg);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_active_passive_cfg);

	if (lag_config->ap_mode_config.hot_standby !=
	    VXGE_HAL_LAG_HOT_STANDBY_DEFAULT) {
		if (lag_config->ap_mode_config.hot_standby ==
		    VXGE_HAL_LAG_HOT_STANDBY_KEEP_UP_PORT)
			val64 |= VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_HOT_STANDBY;
		else
			val64 &= ~VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_HOT_STANDBY;
	}

	if (lag_config->ap_mode_config.lacp_decides !=
	    VXGE_HAL_LAG_LACP_DECIDES_DEFAULT) {
		if (lag_config->ap_mode_config.lacp_decides ==
		    VXGE_HAL_LAG_LACP_DECIDES_ENBALE)
			val64 |= VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_LACP_DECIDES;
		else
			val64 &= ~VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_LACP_DECIDES;
	}

	if (lag_config->ap_mode_config.pref_active_port !=
	    VXGE_HAL_LAG_PREF_ACTIVE_PORT_DEFAULT) {
		if (lag_config->ap_mode_config.pref_active_port ==
		    VXGE_HAL_LAG_PREF_ACTIVE_PORT_1)
			val64 |=
			    VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_PREF_ACTIVE_PORT_NUM;
		else
			val64 &=
			    ~VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_PREF_ACTIVE_PORT_NUM;
	}

	if (lag_config->ap_mode_config.auto_failback !=
	    VXGE_HAL_LAG_AUTO_FAILBACK_DEFAULT) {
		if (lag_config->ap_mode_config.auto_failback ==
		    VXGE_HAL_LAG_AUTO_FAILBACK_ENBALE)
			val64 |= VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_AUTO_FAILBACK;
		else
			val64 &= ~VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_AUTO_FAILBACK;
	}

	if (lag_config->ap_mode_config.failback_en !=
	    VXGE_HAL_LAG_FAILBACK_EN_DEFAULT) {
		if (lag_config->ap_mode_config.failback_en ==
		    VXGE_HAL_LAG_FAILBACK_EN_ENBALE)
			val64 |= VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_FAILBACK_EN;
		else
			val64 &= ~VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_FAILBACK_EN;
	}

	if (lag_config->ap_mode_config.cold_failover_timeout !=
	    VXGE_HAL_LAG_DEF_COLD_FAILOVER_TIMEOUT) {
		val64 &= ~VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_COLD_FAILOVER_TIMEOUT(
		    0xffff);
		val64 |= VXGE_HAL_LAG_ACTIVE_PASSIVE_CFG_COLD_FAILOVER_TIMEOUT(
		    lag_config->ap_mode_config.cold_failover_timeout);
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_active_passive_cfg);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_lacp_cfg);

	if (lag_config->lacp_config.lacp_en !=
	    VXGE_HAL_LAG_LACP_EN_DEFAULT) {
		if (lag_config->lacp_config.lacp_en ==
		    VXGE_HAL_LAG_LACP_EN_ENABLE)
			val64 |= VXGE_HAL_LAG_LACP_CFG_EN;
		else
			val64 &= ~VXGE_HAL_LAG_LACP_CFG_EN;
	}

	if (lag_config->lacp_config.lacp_begin !=
	    VXGE_HAL_LAG_LACP_BEGIN_DEFAULT) {
		if (lag_config->lacp_config.lacp_begin ==
		    VXGE_HAL_LAG_LACP_BEGIN_RESET)
			val64 |= VXGE_HAL_LAG_LACP_CFG_LACP_BEGIN;
		else
			val64 &= ~VXGE_HAL_LAG_LACP_CFG_LACP_BEGIN;
	}

	if (lag_config->lacp_config.discard_lacp !=
	    VXGE_HAL_LAG_DISCARD_LACP_DEFAULT) {
		if (lag_config->lacp_config.discard_lacp ==
		    VXGE_HAL_LAG_DISCARD_LACP_ENABLE)
			val64 |= VXGE_HAL_LAG_LACP_CFG_DISCARD_LACP;
		else
			val64 &= ~VXGE_HAL_LAG_LACP_CFG_DISCARD_LACP;
	}

	if (lag_config->lacp_config.liberal_len_chk !=
	    VXGE_HAL_LAG_LIBERAL_LEN_CHK_DEFAULT) {
		if (lag_config->lacp_config.liberal_len_chk ==
		    VXGE_HAL_LAG_LIBERAL_LEN_CHK_ENABLE)
			val64 |= VXGE_HAL_LAG_LACP_CFG_LIBERAL_LEN_CHK;
		else
			val64 &= ~VXGE_HAL_LAG_LACP_CFG_LIBERAL_LEN_CHK;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_lacp_cfg);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_marker_cfg);

	if (lag_config->lacp_config.marker_gen_recv_en !=
	    VXGE_HAL_LAG_MARKER_GEN_RECV_EN_DEFAULT) {
		if (lag_config->lacp_config.marker_gen_recv_en ==
		    VXGE_HAL_LAG_MARKER_GEN_RECV_EN_ENABLE)
			val64 |= VXGE_HAL_LAG_MARKER_CFG_GEN_RCVR_EN;
		else
			val64 &= ~VXGE_HAL_LAG_MARKER_CFG_GEN_RCVR_EN;
	}

	if (lag_config->lacp_config.marker_resp_en !=
	    VXGE_HAL_LAG_MARKER_RESP_EN_DEFAULT) {
		if (lag_config->lacp_config.marker_resp_en ==
		    VXGE_HAL_LAG_MARKER_RESP_EN_ENABLE)
			val64 |= VXGE_HAL_LAG_MARKER_CFG_RESP_EN;
		else
			val64 &= ~VXGE_HAL_LAG_MARKER_CFG_RESP_EN;
	}

	if (lag_config->lacp_config.marker_resp_timeout !=
	    VXGE_HAL_LAG_DEF_MARKER_RESP_TIMEOUT) {
		val64 &= ~VXGE_HAL_LAG_MARKER_CFG_RESP_TIMEOUT(0xffff);
		val64 |= VXGE_HAL_LAG_MARKER_CFG_RESP_TIMEOUT(
		    lag_config->lacp_config.marker_resp_timeout);
	}

	if (lag_config->lacp_config.slow_proto_mrkr_min_interval !=
	    VXGE_HAL_LAG_DEF_SLOW_PROTO_MRKR_MIN_INTERVAL) {
		val64 &= ~VXGE_HAL_LAG_MARKER_CFG_SLOW_PROTO_MRKR_MIN_INTERVAL(
		    0xffff);
		val64 |= VXGE_HAL_LAG_MARKER_CFG_SLOW_PROTO_MRKR_MIN_INTERVAL(
		    lag_config->lacp_config.slow_proto_mrkr_min_interval);
	}

	if (lag_config->lacp_config.throttle_mrkr_resp !=
	    VXGE_HAL_LAG_THROTTLE_MRKR_RESP_DEFAULT) {
		if (lag_config->lacp_config.throttle_mrkr_resp ==
		    VXGE_HAL_LAG_THROTTLE_MRKR_RESP_ENABLE)
			val64 |= VXGE_HAL_LAG_MARKER_CFG_THROTTLE_MRKR_RESP;
		else
			val64 &= ~VXGE_HAL_LAG_MARKER_CFG_THROTTLE_MRKR_RESP;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_marker_cfg);

	for (i = 0; i < VXGE_HAL_LAG_PORT_MAX_PORTS; i++) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_port_cfg[i]);

		if (lag_config->port_config[i].lag_en !=
		    VXGE_HAL_LAG_PORT_LAG_EN_DEFAULT) {
			if (lag_config->port_config[i].lag_en ==
			    VXGE_HAL_LAG_PORT_LAG_EN_ENABLE)
				val64 |= VXGE_HAL_LAG_PORT_CFG_EN;
			else
				val64 &= ~VXGE_HAL_LAG_PORT_CFG_EN;
		}

		if (lag_config->port_config[i].discard_slow_proto !=
		    VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_DEFAULT) {
			if (lag_config->port_config[i].discard_slow_proto ==
			    VXGE_HAL_LAG_PORT_DISCARD_SLOW_PROTO_ENABLE)
				val64 |=
				    VXGE_HAL_LAG_PORT_CFG_DISCARD_SLOW_PROTO;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_CFG_DISCARD_SLOW_PROTO;
		}

		if (lag_config->port_config[i].host_chosen_aggr !=
		    VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_DEFAULT) {
			if (lag_config->port_config[i].host_chosen_aggr ==
			    VXGE_HAL_LAG_PORT_HOST_CHOSEN_AGGR_1)
				val64 |=
				    VXGE_HAL_LAG_PORT_CFG_HOST_CHOSEN_AGGR;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_CFG_HOST_CHOSEN_AGGR;
		}

		if (lag_config->port_config[i].discard_unknown_slow_proto !=
		    VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_DEFAULT) {
			if (lag_config->port_config[i].discard_unknown_slow_proto ==
			    VXGE_HAL_LAG_PORT_DISCARD_UNKNOWN_SLOW_PROTO_ENABLE)
				val64 |=
				    VXGE_HAL_LAG_PORT_CFG_DISCARD_UNKNOWN_SLOW_PROTO;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_CFG_DISCARD_UNKNOWN_SLOW_PROTO;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_port_cfg[i]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_port_actor_admin_cfg[i]);

		if (lag_config->port_config[i].actor_port_num !=
		    VXGE_HAL_LAG_PORT_DEF_ACTOR_PORT_NUM) {
			val64 &= ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_PORT_NUM(
			    0xffff);
			val64 |= VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_PORT_NUM(
			    lag_config->port_config[i].actor_port_num);
		}

		if (lag_config->port_config[i].actor_port_priority !=
		    VXGE_HAL_LAG_PORT_DEF_ACTOR_PORT_PRIORITY) {
			val64 &= ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_PORT_PRI(
			    0xffff);
			val64 |= VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_PORT_PRI(
			    lag_config->port_config[i].actor_port_priority);
		}

		if (lag_config->port_config[i].actor_key_10g !=
		    VXGE_HAL_LAG_PORT_DEF_ACTOR_KEY_10G) {
			val64 &= ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_KEY_10G(
			    0xffff);
			val64 |= VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_KEY_10G(
			    lag_config->port_config[i].actor_key_10g);
		}

		if (lag_config->port_config[i].actor_key_1g !=
		    VXGE_HAL_LAG_PORT_DEF_ACTOR_KEY_1G) {
			val64 &= ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_KEY_1G(
			    0xffff);
			val64 |= VXGE_HAL_LAG_PORT_ACTOR_ADMIN_CFG_KEY_1G(
			    lag_config->port_config[i].actor_key_1g);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_port_actor_admin_cfg[i]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_port_actor_admin_state[i]);

		if (lag_config->port_config[i].actor_lacp_activity !=
		    VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_DEFAULT) {
			if (lag_config->port_config[i].actor_lacp_activity ==
			    VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_ACTIVE)
				val64 |=
				    VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_LACP_ACTIVITY;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_LACP_ACTIVITY;
		}

		if (lag_config->port_config[i].actor_lacp_timeout !=
		    VXGE_HAL_LAG_PORT_ACTOR_LACP_ACTIVITY_DEFAULT) {
			if (lag_config->port_config[i].actor_lacp_timeout ==
			    VXGE_HAL_LAG_PORT_ACTOR_LACP_TIMEOUT_SHORT)
				val64 |=
				    VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_LACP_TIMEOUT;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_LACP_TIMEOUT;
		}

		if (lag_config->port_config[i].actor_aggregation !=
		    VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_DEFAULT) {
			if (lag_config->port_config[i].actor_aggregation ==
			    VXGE_HAL_LAG_PORT_ACTOR_AGGREGATION_AGGREGATEABLE)
				val64 |=
				    VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_AGGREGATION;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_AGGREGATION;
		}

		if (lag_config->port_config[i].actor_synchronization !=
		    VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_DEFAULT) {
			if (lag_config->port_config[i].actor_aggregation ==
			    VXGE_HAL_LAG_PORT_ACTOR_SYNCHRONIZATION_IN_SYNC)
				val64 |= VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_SYNCHRONIZATION;
			else
				val64 &= ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_SYNCHRONIZATION;
		}

		if (lag_config->port_config[i].actor_collecting !=
		    VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_DEFAULT) {
			if (lag_config->port_config[i].actor_collecting ==
			    VXGE_HAL_LAG_PORT_ACTOR_COLLECTING_ENABLE)
				val64 |=
				    VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_COLLECTING;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_COLLECTING;
		}

		if (lag_config->port_config[i].actor_distributing !=
		    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_DEFAULT) {
			if (lag_config->port_config[i].actor_distributing ==
			    VXGE_HAL_LAG_PORT_ACTOR_DISTRIBUTING_ENABLE)
				val64 |=
				    VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_DISTRIBUTING;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_DISTRIBUTING;
		}

		if (lag_config->port_config[i].actor_defaulted !=
		    VXGE_HAL_LAG_PORT_ACTOR_DEFAULTED_DEFAULT) {
			if (lag_config->port_config[i].actor_defaulted ==
			    VXGE_HAL_LAG_PORT_ACTOR_NOT_DEFAULTED)
				val64 |= VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_DEFAULTED;
			else
				val64 &= ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_DEFAULTED;
		}

		if (lag_config->port_config[i].actor_expired !=
		    VXGE_HAL_LAG_PORT_ACTOR_EXPIRED_DEFAULT) {
			if (lag_config->port_config[i].actor_expired ==
			    VXGE_HAL_LAG_PORT_ACTOR_NOT_EXPIRED)
				val64 |= VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_EXPIRED;
			else
				val64 &= ~VXGE_HAL_LAG_PORT_ACTOR_ADMIN_STATE_EXPIRED;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_port_actor_admin_state[i]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_port_partner_admin_cfg[i]);

		if (lag_config->port_config[i].partner_sys_pri !=
		    VXGE_HAL_LAG_PORT_DEF_PARTNER_SYS_PRI) {
			val64 &= ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_SYS_PRI(
			    0xffff);
			val64 |= VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_SYS_PRI(
			    lag_config->port_config[i].partner_sys_pri);
		}

		if (lag_config->port_config[i].partner_key !=
		    VXGE_HAL_LAG_PORT_DEF_PARTNER_KEY) {
			val64 &= ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_KEY(
			    0xffff);
			val64 |= VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_KEY(
			    lag_config->port_config[i].partner_key);
		}

		if (lag_config->port_config[i].partner_port_num !=
		    VXGE_HAL_LAG_PORT_DEF_PARTNER_PORT_NUM) {
			val64 &= ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_PORT_NUM(
			    0xffff);
			val64 |= VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_PORT_NUM(
			    lag_config->port_config[i].partner_port_num);
		}

		if (lag_config->port_config[i].partner_port_priority !=
		    VXGE_HAL_LAG_PORT_DEF_PARTNER_PORT_PRIORITY) {
			val64 &= ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_PORT_PRI(
			    0xffff);
			val64 |= VXGE_HAL_LAG_PORT_PARTNER_ADMIN_CFG_PORT_PRI(
			    lag_config->port_config[i].actor_port_priority);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_port_partner_admin_cfg[i]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_port_partner_admin_state[i]);

		if (lag_config->port_config[i].partner_lacp_activity !=
		    VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_DEFAULT) {
			if (lag_config->port_config[i].partner_lacp_activity ==
			    VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_ACTIVE)
				val64 |= VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_LACP_ACTIVITY;
			else
				val64 &= ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_LACP_ACTIVITY;
		}

		if (lag_config->port_config[i].partner_lacp_timeout !=
		    VXGE_HAL_LAG_PORT_PARTNER_LACP_ACTIVITY_DEFAULT) {
			if (lag_config->port_config[i].partner_lacp_timeout ==
			    VXGE_HAL_LAG_PORT_PARTNER_LACP_TIMEOUT_SHORT)
				val64 |=
				    VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_LACP_TIMEOUT;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_LACP_TIMEOUT;
		}

		if (lag_config->port_config[i].partner_aggregation !=
		    VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_DEFAULT) {
			if (lag_config->port_config[i].partner_aggregation ==
			    VXGE_HAL_LAG_PORT_PARTNER_AGGREGATION_AGGREGATEABLE)
				val64 |=
				    VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_AGGREGATION;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_AGGREGATION;
		}

		if (lag_config->port_config[i].partner_synchronization !=
		    VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_DEFAULT) {
			if (lag_config->port_config[i].partner_aggregation ==
			    VXGE_HAL_LAG_PORT_PARTNER_SYNCHRONIZATION_IN_SYNC)
				val64 |= VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_SYNCHRONIZATION;
			else
				val64 &= ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_SYNCHRONIZATION;
		}

		if (lag_config->port_config[i].partner_collecting !=
		    VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_DEFAULT) {
			if (lag_config->port_config[i].partner_collecting ==
			    VXGE_HAL_LAG_PORT_PARTNER_COLLECTING_ENABLE)
				val64 |=
				    VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_COLLECTING;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_COLLECTING;
		}

		if (lag_config->port_config[i].partner_distributing !=
		    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_DEFAULT) {
			if (lag_config->port_config[i].partner_distributing ==
			    VXGE_HAL_LAG_PORT_PARTNER_DISTRIBUTING_ENABLE)
				val64 |=
				    VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_DISTRIBUTING;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_DISTRIBUTING;
		}

		if (lag_config->port_config[i].partner_defaulted !=
		    VXGE_HAL_LAG_PORT_PARTNER_DEFAULTED_DEFAULT) {
			if (lag_config->port_config[i].partner_defaulted ==
			    VXGE_HAL_LAG_PORT_PARTNER_NOT_DEFAULTED)
				val64 |=
				    VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_DEFAULTED;
			else
				val64 &=
				    ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_DEFAULTED;
		}

		if (lag_config->port_config[i].partner_expired !=
		    VXGE_HAL_LAG_PORT_PARTNER_EXPIRED_DEFAULT) {
			if (lag_config->port_config[i].partner_expired ==
			    VXGE_HAL_LAG_PORT_PARTNER_NOT_EXPIRED)
				val64 |= VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_EXPIRED;
			else
				val64 &= ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_STATE_EXPIRED;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_port_partner_admin_state[i]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_port_partner_admin_sys_id[i]);

		mac_addr = 0;

		for (j = 0; j < VXGE_HAL_ETH_ALEN; j++) {
			mac_addr <<= 8;
			mac_addr |=
			    (u8) lag_config->port_config[i].partner_mac_addr[j];
		}

		if (mac_addr != 0xffffffffffffULL) {
			val64 &= ~VXGE_HAL_LAG_PORT_PARTNER_ADMIN_SYS_ID_ADDR(
			    0xffffffffffffULL);
			val64 |= VXGE_HAL_LAG_PORT_PARTNER_ADMIN_SYS_ID_ADDR(
			    mac_addr);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_port_partner_admin_sys_id[i]);

	}

	for (i = 0; i < VXGE_HAL_LAG_AGGR_MAX_PORTS; i++) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_aggr_id_cfg[i]);

		val64 &= ~VXGE_HAL_LAG_AGGR_ID_CFG_ID(0xffff);
		val64 |= VXGE_HAL_LAG_AGGR_ID_CFG_ID(
		    lag_config->aggr_config[i].aggr_id);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_aggr_id_cfg[i]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_aggr_addr_cfg[i]);

		mac_addr = 0;

		for (j = 0; j < VXGE_HAL_ETH_ALEN; j++) {
			mac_addr <<= 8;
			mac_addr |= (u8) lag_config->aggr_config[i].mac_addr[j];
		}

		if (mac_addr != 0xffffffffffffULL) {
			val64 &=
			    ~VXGE_HAL_LAG_AGGR_ADDR_CFG_ADDR(0xffffffffffffULL);
			val64 |= VXGE_HAL_LAG_AGGR_ADDR_CFG_ADDR(mac_addr);
		}

		if (lag_config->aggr_config[i].use_port_mac_addr !=
		    VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_DEFAULT) {
			if (lag_config->aggr_config[i].use_port_mac_addr ==
			    VXGE_HAL_LAG_AGGR_USE_PORT_MAC_ADDR_ENABLE)
				val64 |=
				    VXGE_HAL_LAG_AGGR_ADDR_CFG_USE_PORT_ADDR;
			else
				val64 &=
				    ~VXGE_HAL_LAG_AGGR_ADDR_CFG_USE_PORT_ADDR;
		}

		if (lag_config->aggr_config[i].mac_addr_sel !=
		    VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_DEFAULT) {
			if (lag_config->aggr_config[i].mac_addr_sel ==
			    VXGE_HAL_LAG_AGGR_MAC_ADDR_SEL_PORT_1)
				val64 |= VXGE_HAL_LAG_AGGR_ADDR_CFG_ADDR_SEL;
			else
				val64 &= ~VXGE_HAL_LAG_AGGR_ADDR_CFG_ADDR_SEL;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_aggr_addr_cfg[i]);

		if (lag_config->aggr_config[i].admin_key ==
		    VXGE_HAL_LAG_AGGR_DEF_ADMIN_KEY) {
			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &hldev->mrpcim_reg->lag_aggr_admin_key[i]);

			val64 &= ~VXGE_HAL_LAG_AGGR_ADMIN_KEY_KEY(0xffff);
			val64 |= VXGE_HAL_LAG_AGGR_ADMIN_KEY_KEY(
			    lag_config->aggr_config[i].admin_key);

			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    val64,
			    &hldev->mrpcim_reg->lag_aggr_admin_key[i]);
		}
	}

	if (lag_config->sys_pri != VXGE_HAL_LAG_DEF_SYS_PRI) {
		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->lag_sys_cfg);

		val64 &= ~VXGE_HAL_LAG_SYS_CFG_SYS_PRI(0xffff);
		val64 |= VXGE_HAL_LAG_SYS_CFG_SYS_PRI(
		    lag_config->sys_pri);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->lag_sys_cfg);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_sys_id);

	mac_addr = 0;

	for (j = 0; j < VXGE_HAL_ETH_ALEN; j++) {
		mac_addr <<= 8;
		mac_addr |= (u8) lag_config->mac_addr[j];
	}

	if (mac_addr != 0xffffffffffffULL) {
		val64 &= ~VXGE_HAL_LAG_SYS_ID_ADDR(0xffffffffffffULL);
		val64 |= VXGE_HAL_LAG_SYS_ID_ADDR(mac_addr);
	}

	if (lag_config->use_port_mac_addr !=
	    VXGE_HAL_LAG_USE_PORT_MAC_ADDR_DEFAULT) {
		if (lag_config->use_port_mac_addr ==
		    VXGE_HAL_LAG_USE_PORT_MAC_ADDR_ENABLE)
			val64 |= VXGE_HAL_LAG_SYS_ID_USE_PORT_ADDR;
		else
			val64 &= ~VXGE_HAL_LAG_SYS_ID_USE_PORT_ADDR;
	}

	if (lag_config->mac_addr_sel != VXGE_HAL_LAG_MAC_ADDR_SEL_DEFAULT) {
		if (lag_config->mac_addr_sel ==
		    VXGE_HAL_LAG_MAC_ADDR_SEL_PORT_1)
			val64 |= VXGE_HAL_LAG_SYS_ID_ADDR_SEL;
		else
			val64 &= ~VXGE_HAL_LAG_SYS_ID_ADDR_SEL;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_sys_id);


	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_aggr_alt_admin_key);

	if (lag_config->ap_mode_config.alt_admin_key !=
	    VXGE_HAL_LAG_DEF_ALT_ADMIN_KEY) {
		val64 &= ~VXGE_HAL_LAG_AGGR_ALT_ADMIN_KEY_KEY(0xffff);
		val64 |= VXGE_HAL_LAG_AGGR_ALT_ADMIN_KEY_KEY(
		    lag_config->ap_mode_config.alt_admin_key);
	}

	if (lag_config->ap_mode_config.alt_aggr !=
	    VXGE_HAL_LAG_ALT_AGGR_DEFAULT) {
		if (lag_config->ap_mode_config.alt_aggr ==
		    VXGE_HAL_LAG_ALT_AGGR_1)
			val64 |= VXGE_HAL_LAG_AGGR_ALT_ADMIN_KEY_ALT_AGGR;
		else
			val64 &= ~VXGE_HAL_LAG_AGGR_ALT_ADMIN_KEY_ALT_AGGR;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_aggr_alt_admin_key);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_timer_cfg_1);

	if (lag_config->fast_per_time != VXGE_HAL_LAG_DEF_FAST_PER_TIME) {
		val64 &= ~VXGE_HAL_LAG_TIMER_CFG_1_FAST_PER(0xffff);
		val64 |= VXGE_HAL_LAG_TIMER_CFG_1_FAST_PER(
		    lag_config->fast_per_time);
	}

	if (lag_config->slow_per_time != VXGE_HAL_LAG_DEF_SLOW_PER_TIME) {
		val64 &= ~VXGE_HAL_LAG_TIMER_CFG_1_SLOW_PER(0xffff);
		val64 |= VXGE_HAL_LAG_TIMER_CFG_1_SLOW_PER(
		    lag_config->slow_per_time);
	}

	if (lag_config->short_timeout != VXGE_HAL_LAG_DEF_SHORT_TIMEOUT) {
		val64 &= ~VXGE_HAL_LAG_TIMER_CFG_1_SHORT_TIMEOUT(0xffff);
		val64 |= VXGE_HAL_LAG_TIMER_CFG_1_SHORT_TIMEOUT(
		    lag_config->short_timeout);
	}

	if (lag_config->long_timeout != VXGE_HAL_LAG_DEF_LONG_TIMEOUT) {
		val64 &= ~VXGE_HAL_LAG_TIMER_CFG_1_LONG_TIMEOUT(0xffff);
		val64 |= VXGE_HAL_LAG_TIMER_CFG_1_LONG_TIMEOUT(
		    lag_config->short_timeout);
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_timer_cfg_1);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->lag_timer_cfg_2);

	if (lag_config->churn_det_time != VXGE_HAL_LAG_DEF_CHURN_DET_TIME) {
		val64 &= ~VXGE_HAL_LAG_TIMER_CFG_2_CHURN_DET(0xffff);
		val64 |= VXGE_HAL_LAG_TIMER_CFG_2_CHURN_DET(
		    lag_config->churn_det_time);
	}

	if (lag_config->aggr_wait_time != VXGE_HAL_LAG_DEF_AGGR_WAIT_TIME) {
		val64 &= ~VXGE_HAL_LAG_TIMER_CFG_2_AGGR_WAIT(0xffff);
		val64 |= VXGE_HAL_LAG_TIMER_CFG_2_AGGR_WAIT(
		    lag_config->slow_per_time);
	}

	if (lag_config->short_timer_scale !=
	    VXGE_HAL_LAG_SHORT_TIMER_SCALE_DEFAULT) {
		val64 &= ~VXGE_HAL_LAG_TIMER_CFG_2_SHORT_TIMER_SCALE(0xffff);
		val64 |= VXGE_HAL_LAG_TIMER_CFG_2_SHORT_TIMER_SCALE(
		    lag_config->short_timer_scale);
	}

	if (lag_config->long_timer_scale !=
	    VXGE_HAL_LAG_LONG_TIMER_SCALE_DEFAULT) {
		val64 &= ~VXGE_HAL_LAG_TIMER_CFG_2_LONG_TIMER_SCALE(0xffff);
		val64 |= VXGE_HAL_LAG_TIMER_CFG_2_LONG_TIMER_SCALE(
		    lag_config->long_timer_scale);
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->lag_timer_cfg_2);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);

}

/*
 * __hal_mrpcim_get_vpd_data - Getting vpd_data.
 *
 * @hldev: HAL device handle.
 *
 * Getting  product name and serial number from vpd capabilites structure
 *
 */
void
__hal_mrpcim_get_vpd_data(__hal_device_t *hldev)
{
	u8 *vpd_data;
	u16 data;
	u32 data32;
	u32 i, j, count, fail = 0;
	u32 addr_offset, data_offset;
	u32 max_count = hldev->header.config.device_poll_millis * 10;

	vxge_assert(hldev);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	addr_offset = hldev->pci_caps.vpd_cap_offset +
	    vxge_offsetof(vxge_hal_vpid_capability_le_t, vpd_address);

	data_offset = hldev->pci_caps.vpd_cap_offset +
	    vxge_offsetof(vxge_hal_vpid_capability_le_t, vpd_data);

	vxge_os_strlcpy((char *) hldev->mrpcim->vpd_data.product_name,
	    "10 Gigabit Ethernet Adapter",
	    sizeof(hldev->mrpcim->vpd_data.product_name));
	vxge_os_strlcpy((char *) hldev->mrpcim->vpd_data.serial_num,
	    "not available",
	    sizeof(hldev->mrpcim->vpd_data.serial_num));

	if (hldev->func_id != 0) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return;
	}
	vpd_data = (u8 *) vxge_os_malloc(hldev->header.pdev,
	    VXGE_HAL_VPD_BUFFER_SIZE + 16);
	if (vpd_data == 0)
		return;

	for (i = 0; i < VXGE_HAL_VPD_BUFFER_SIZE; i += 4) {
		vxge_os_pci_write16(hldev->header.pdev,
		    hldev->header.cfgh,
		    addr_offset, (u16) i);
		for (count = 0; count < max_count; count++) {
			vxge_os_udelay(100);
			(void) __hal_vpath_pci_read(hldev,
			    hldev->first_vp_id,
			    addr_offset, 2, &data);
			if (data & VXGE_HAL_PCI_VPID_COMPL_FALG)
				break;
		}

		if (count >= max_count) {
			vxge_hal_info_log_device("%s:ERR, \
			    Reading VPD data failed", __func__);
			fail = 1;
			break;
		}
		(void) __hal_vpath_pci_read(hldev,
		    hldev->first_vp_id,
		    data_offset,
		    4,
		    &data32);

		for (j = 0; j < 4; j++) {
			vpd_data[i + j] = (u8) (data32 & 0xff);
			data32 >>= 8;
		}
	}

	if (!fail) {

		/* read serial number of adapter */
		for (count = 0; count < VXGE_HAL_VPD_BUFFER_SIZE; count++) {
			if ((vpd_data[count] == 'S') &&
			    (vpd_data[count + 1] == 'N') &&
			    (vpd_data[count + 2] < VXGE_HAL_VPD_LENGTH)) {
				(void) vxge_os_memzero(
				    hldev->mrpcim->vpd_data.serial_num,
				    VXGE_HAL_VPD_LENGTH);
				(void) vxge_os_memcpy(
				    hldev->mrpcim->vpd_data.serial_num,
				    &vpd_data[count + 3],
				    vpd_data[count + 2]);
				break;
			}
		}

		if (vpd_data[1] < VXGE_HAL_VPD_LENGTH) {
			(void) vxge_os_memzero(
			    hldev->mrpcim->vpd_data.product_name, vpd_data[1]);
			(void) vxge_os_memcpy(hldev->mrpcim->vpd_data.product_name,
			    &vpd_data[3], vpd_data[1]);
		}
	}

	vxge_os_free(hldev->header.pdev,
	    vpd_data,
	    VXGE_HAL_VPD_BUFFER_SIZE + 16);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, fail);
}

/*
 * __hal_mrpcim_rts_table_access - Get/Set the entries from RTS access tables
 * @devh: Device handle.
 * @action: Write Enable. 0 - Read Operation; 1 - Write Operation
 * @rts_table: Data structure select. Identifies the RTS data structure
 *		(i.e. lookup table) to access.
 *		0; DA; Destination Address
 *		1; VID; VLAN ID
 *		2; ETYPE; Ethertype
 *		3; PN; Layer 4 Port Number
 *		4; RANGE_PN; Range of Layer 4 Port Numbers
 *		5; RTH_GEN_CFG; Receive-Traffic Hashing General Configuration
 *		6; RTH_SOLO_IT; Receive-Traffic Hashing Indirection Table
 *		(Single Bucket Programming)
 *		7; RTH_JHASH_CFG; Receive-Traffic Hashing Jenkins Hash Config
 *		8; RTH_MASK; Receive-Traffic Hashing Mask
 *		9; RTH_KEY; Receive-Traffic Hashing Key
 *		10; QOS; VLAN Quality of Service
 *		11; DS; IP Differentiated Services
 * @offset: Offset (into the data structure) to execute the command on.
 * @data1: Pointer to the data 1 to be read from the table
 * @data2: Pointer to the data 2 to be read from the table
 * @vpath_vector: Identifies the candidate VPATH(s) for the given entry.
 *		These VPATH(s) determine the set of target destinations for
 *		a frame that matches this steering entry. Any or all bits
 *		can be set, which handles 16+1 virtual paths in an 'n-hot'
 *		basis. VPATH 0 is the MSbit.
 *
 * Read from the RTS table
 *
 */
vxge_hal_status_e
__hal_mrpcim_rts_table_access(
    vxge_hal_device_h devh,
    u32 action,
    u32 rts_table,
    u32 offset,
    u64 *data1,
    u64 *data2,
    u64 *vpath_vector)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((devh != NULL) && (data1 != NULL) &&
	    (data2 != NULL) && (vpath_vector != NULL));

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", action = %d, rts_table = %d, "
	    "offset = %d, data1 = 0x"VXGE_OS_STXFMT", "
	    "data2 = 0x"VXGE_OS_STXFMT", vpath_vector = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, action, rts_table, offset, (ptr_t) data1,
	    (ptr_t) data2, (ptr_t) vpath_vector);

	val64 = VXGE_HAL_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL(rts_table) |
	    VXGE_HAL_RTS_MGR_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_MGR_STEER_CTRL_OFFSET(offset);

	if (action == VXGE_HAL_RTS_MGR_STEER_CTRL_WE_WRITE)
		val64 = VXGE_HAL_RTS_MGR_STEER_CTRL_WE;

	if ((rts_table ==
	    VXGE_HAL_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT) ||
	    (rts_table ==
	    VXGE_HAL_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT) ||
	    (rts_table ==
	    VXGE_HAL_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASK) ||
	    (rts_table ==
	    VXGE_HAL_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_RTH_KEY)) {
		val64 |= VXGE_HAL_RTS_MGR_STEER_CTRL_TABLE_SEL;
	}

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &hldev->mrpcim_reg->rts_mgr_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->mrpcim_reg->rts_mgr_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->rts_mgr_steer_ctrl, 0,
	    VXGE_HAL_RTS_MGR_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->rts_mgr_steer_ctrl);

	if ((val64 & VXGE_HAL_RTS_MGR_STEER_CTRL_RMACJ_STATUS) &&
	    (action == VXGE_HAL_RTS_MGR_STEER_CTRL_WE_READ)) {

		*data1 = vxge_os_pio_mem_read64(
		    hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rts_mgr_steer_data0);

		*data2 = vxge_os_pio_mem_read64(
		    hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rts_mgr_steer_data1);

		*vpath_vector = vxge_os_pio_mem_read64(
		    hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rts_mgr_steer_vpath_vector);

		status = VXGE_HAL_OK;

	} else {
		status = VXGE_HAL_FAIL;
	}


	vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);
	return (status);
}

/*
 * vxge_hal_mrpcim_mac_addr_add - Add the mac address entry
 *				into MAC address table.
 * @devh: Device handle.
 * @offset: Index into the DA table to add the mac address.
 * @macaddr: MAC address to be added for this vpath into the list
 * @macaddr_mask: MAC address mask for macaddr
 * @vpath_vector: Bit mask specifying the vpaths to which
 *		the mac address applies
 * @duplicate_mode: Duplicate MAC address add mode. Please see
 *		vxge_hal_vpath_mac_addr_add_mode_e {}
 *
 * Adds the given mac address, mac address mask and vpath vector into the list
 *
 * see also: vxge_hal_mrpcim_mac_addr_get
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_mac_addr_add(
    vxge_hal_device_h devh,
    u32 offset,
    macaddr_t macaddr,
    macaddr_t macaddr_mask,
    u64 vpath_vector,
    u32 duplicate_mode)
{
	u32 i;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", offset = %d, "
	    "macaddr = %02x-%02x-%02x-%02x-%02x-%02x, "
	    "macaddr_mask = %02x-%02x-%02x-%02x-%02x-%02x, "
	    "vpath_vector = 0x"VXGE_OS_LLXFMT,
	    (ptr_t) devh, offset, macaddr[0], macaddr[1], macaddr[2],
	    macaddr[3], macaddr[4], macaddr[5], macaddr_mask[0],
	    macaddr_mask[1], macaddr_mask[2], macaddr_mask[3],
	    macaddr_mask[4], macaddr_mask[5], vpath_vector);

	for (i = 0; i < VXGE_HAL_ETH_ALEN; i++) {
		data1 <<= 8;
		data1 |= (u8) macaddr[i];
	}

	data1 = VXGE_HAL_RTS_MGR_STEER_DATA0_DA_MAC_ADDR(data1);

	for (i = 0; i < VXGE_HAL_ETH_ALEN; i++) {
		data2 <<= 8;
		data2 |= (u8) macaddr_mask[i];
	}

	switch (duplicate_mode) {
	case VXGE_HAL_VPATH_MAC_ADDR_ADD_DUPLICATE:
		i = 0;
		break;
	case VXGE_HAL_VPATH_MAC_ADDR_DISCARD_DUPLICATE:
		i = 1;
		break;
	case VXGE_HAL_VPATH_MAC_ADDR_REPLACE_DUPLICATE:
		i = 2;
		break;
	default:
		i = 0;
		break;
	}

	data2 = VXGE_HAL_RTS_MGR_STEER_DATA1_DA_MAC_ADDR_MASK(data2) |
	    VXGE_HAL_RTS_MGR_STEER_DATA1_DA_MAC_ADDR_MODE(i);

	status = __hal_mrpcim_rts_table_access(devh,
	    VXGE_HAL_RTS_MGR_STEER_CTRL_WE_WRITE,
	    VXGE_HAL_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_DA,
	    offset,
	    &data1,
	    &data2,
	    &vpath_vector);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_mrpcim_mac_addr_get - Read the mac address entry into
 *				MAC address table.
 * @devh: Device handle.
 * @offset: Index into the DA table to execute the command on.
 * @macaddr: Buffer to return MAC address to be added for this vpath
 *		into the list
 * @macaddr_mask: Buffer to return MAC address mask for macaddr
 * @vpath_vector: Buffer to return Bit mask specifying the vpaths
 *		to which the mac address applies
 *
 * Reads the mac address, mac address mask and vpath vector from
 *		the given offset
 *
 * see also: vxge_hal_mrpcim_mac_addr_add
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_mac_addr_get(
    vxge_hal_device_h devh,
    u32 offset,
    macaddr_t macaddr,
    macaddr_t macaddr_mask,
    u64 *vpath_vector)
{
	u32 i;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	status = __hal_mrpcim_rts_table_access(devh,
	    VXGE_HAL_RTS_MGR_STEER_CTRL_WE_WRITE,
	    VXGE_HAL_RTS_MGR_STEER_CTRL_DATA_STRUCT_SEL_DA,
	    offset,
	    &data1,
	    &data2,
	    vpath_vector);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	data1 = VXGE_HAL_RTS_MGR_STEER_DATA0_GET_DA_MAC_ADDR(data1);

	data2 = VXGE_HAL_RTS_MGR_STEER_DATA1_GET_DA_MAC_ADDR_MASK(data2);

	for (i = VXGE_HAL_ETH_ALEN; i > 0; i--) {
		macaddr[i - 1] = (u8) (data1 & 0xFF);
		data1 >>= 8;
	}

	for (i = VXGE_HAL_ETH_ALEN; i > 0; i--) {
		macaddr_mask[i - 1] = (u8) (data2 & 0xFF);
		data2 >>= 8;
	}

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_mrpcim_strip_repl_vlan_tag_enable - Enable strip Repl vlan tag.
 * @devh: Device handle.
 *
 * Enable X3100 strip Repl vlan tag.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_strip_repl_vlan_tag_enable(
    vxge_hal_device_h devh)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	if (hldev->header.config.mrpcim_config.mac_config.
	    rpa_repl_strip_vlan_tag ==
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_ENABLE) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->rxmac_rx_pa_cfg1);

	val64 |= VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_STRIP_VLAN_TAG;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->rxmac_rx_pa_cfg1);

	hldev->header.config.mrpcim_config.mac_config.rpa_repl_strip_vlan_tag =
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_ENABLE;

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mrpcim_strip_repl_vlan_tag_disable - Disable strip Repl vlan tag.
 * @devh: Device handle.
 *
 * Disable X3100 strip Repl vlan tag.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_strip_repl_vlan_tag_disable(
    vxge_hal_device_h devh)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	if (hldev->header.config.mrpcim_config.mac_config.
	    rpa_repl_strip_vlan_tag ==
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_DISABLE) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->rxmac_rx_pa_cfg1);

	val64 &= ~VXGE_HAL_RXMAC_RX_PA_CFG1_REPL_STRIP_VLAN_TAG;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->rxmac_rx_pa_cfg1);

	hldev->header.config.mrpcim_config.mac_config.rpa_repl_strip_vlan_tag =
	    VXGE_HAL_MAC_RPA_REPL_STRIP_VLAN_TAG_DISABLE;

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mrpcim_lag_config_get - Get the LAG config.
 * @devh: Device handle.
 * @lconfig: LAG Configuration
 *
 * Returns the current LAG configuration.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_lag_config_get(
    vxge_hal_device_h devh,
    vxge_hal_lag_config_t *lconfig)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", lconfig = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) lconfig);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	vxge_os_memcpy(lconfig,
	    &hldev->header.config.mrpcim_config.lag_config,
	    sizeof(vxge_hal_lag_config_t));

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mrpcim_lag_config_set - Set the LAG config.
 * @devh: Device handle.
 * @lconfig: LAG Configuration
 *
 * Sets the LAG configuration.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_mrpcim_lag_config_set(
    vxge_hal_device_h devh,
    vxge_hal_lag_config_t *lconfig)
{
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", lconfig = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) lconfig);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	status = __hal_device_lag_config_check(lconfig);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vxge_os_memcpy(&hldev->header.config.mrpcim_config.lag_config,
	    lconfig,
	    sizeof(vxge_hal_lag_config_t));

	status = __hal_mrpcim_lag_configure(hldev);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mrpcim_getpause_data -Pause frame frame generation and reception.
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
vxge_hal_mrpcim_getpause_data(
    vxge_hal_device_h devh,
    u32 port,
    u32 *tx,
    u32 *rx)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", port = %d, tx = 0x"VXGE_OS_STXFMT", "
	    "rx = 0x"VXGE_OS_STXFMT, (ptr_t) devh, port, (ptr_t) tx,
	    (ptr_t) rx);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (port >= VXGE_HAL_MAC_MAX_PORTS) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_PORT);
		return (VXGE_HAL_ERR_INVALID_PORT);
	}

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev, hldev->header.regh0,
	    &hldev->mrpcim_reg->rxmac_pause_cfg_port[port]);

	if (val64 & VXGE_HAL_RXMAC_PAUSE_CFG_PORT_GEN_EN)
		*tx = 1;

	if (val64 & VXGE_HAL_RXMAC_PAUSE_CFG_PORT_RCV_EN)
		*rx = 1;


	vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mrpcim_setpause_data -  set/reset pause frame generation.
 * @devh: HAL device handle.
 * @port : Port number 0, 1, or 2
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

vxge_hal_status_e
vxge_hal_mrpcim_setpause_data(
    vxge_hal_device_h devh,
    u32 port,
    u32 tx,
    u32 rx)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim(
	    "devh = 0x"VXGE_OS_STXFMT", port = %d, tx = %d, rx = %d",
	    (ptr_t) devh, port, tx, rx);

	if (hldev->header.magic != VXGE_HAL_DEVICE_MAGIC) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_DEVICE);
		return (VXGE_HAL_ERR_INVALID_DEVICE);
	}

	if (port >= VXGE_HAL_MAC_MAX_PORTS) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_PORT);
		return (VXGE_HAL_ERR_INVALID_PORT);
	}

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev, hldev->header.regh0,
	    &hldev->mrpcim_reg->rxmac_pause_cfg_port[port]);
	if (tx)
		val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_GEN_EN;
	else
		val64 &= ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_GEN_EN;
	if (rx)
		val64 |= VXGE_HAL_RXMAC_PAUSE_CFG_PORT_RCV_EN;
	else
		val64 &= ~VXGE_HAL_RXMAC_PAUSE_CFG_PORT_RCV_EN;

	vxge_os_pio_mem_write64(hldev->header.pdev, hldev->header.regh0,
	    val64, &hldev->mrpcim_reg->rxmac_pause_cfg_port[port]);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_mrpcim_bist_test - invokes the MemBist test of the card .
 * @devh: HAL device handle.
 * vxge_nic structure.
 * @data:variable that returns the result of each of the test conducted by
 * the driver.
 *
 * This invokes the MemBist test of the card. We give around
 * 2 secs time for the Test to complete. If it's still not complete
 * within this peiod, we consider that the test failed.
 * Return value:
 * 0 on success and -1 on failure.
 */
vxge_hal_status_e
vxge_hal_mrpcim_bist_test(vxge_hal_device_h devh, u64 *data)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;
	u8 bist = 0;
	int retry = 0;
	vxge_hal_status_e status = VXGE_HAL_FAIL;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("devh = 0x"VXGE_OS_STXFMT,
			(ptr_t)devh);

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

	(void) __hal_vpath_pci_read(hldev,
	    hldev->first_vp_id,
	    vxge_offsetof(vxge_hal_pci_config_le_t, bist),
	    1,
	    &bist);
	bist |= 0x40;
	vxge_os_pci_write8(hldev->header.pdev, hldev->header.cfgh,
	    vxge_offsetof(vxge_hal_pci_config_le_t, bist), bist);

	while (retry < 20) {
		(void) __hal_vpath_pci_read(hldev,
		    hldev->first_vp_id,
		    vxge_offsetof(vxge_hal_pci_config_le_t, bist),
		    1,
		    &bist);
		if (!(bist & 0x40)) {
			*data = (bist & 0x0f);
			status = VXGE_HAL_OK;
			break;
		}
		vxge_os_mdelay(100);
		retry++;
	}

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_mrpcim_initialize - Initialize mrpcim
 * @hldev: hal device.
 *
 * Initializes mrpcim
 *
 * See also: __hal_mrpcim_terminate()
 */
vxge_hal_status_e
__hal_mrpcim_initialize(__hal_device_t *hldev)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("hldev = 0x"VXGE_OS_STXFMT,
			(ptr_t)hldev);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM)) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);
	}

	hldev->mrpcim = (__hal_mrpcim_t *)
	    vxge_os_malloc(hldev->header.pdev, sizeof(__hal_mrpcim_t));

	if (hldev->mrpcim == NULL) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);
		return (VXGE_HAL_ERR_OUT_OF_MEMORY);
	}

	vxge_os_memzero(hldev->mrpcim, sizeof(__hal_mrpcim_t));

	__hal_mrpcim_get_vpd_data(hldev);

	hldev->mrpcim->mrpcim_stats_block =
	    __hal_blockpool_block_allocate(hldev, VXGE_OS_HOST_PAGE_SIZE);

	if (hldev->mrpcim->mrpcim_stats_block == NULL) {

		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_OUT_OF_MEMORY);

		return (VXGE_HAL_ERR_OUT_OF_MEMORY);

	}

	hldev->mrpcim->mrpcim_stats = (vxge_hal_mrpcim_stats_hw_info_t *)
	    hldev->mrpcim->mrpcim_stats_block->memblock;

	vxge_os_memzero(hldev->mrpcim->mrpcim_stats,
	    sizeof(vxge_hal_mrpcim_stats_hw_info_t));

	vxge_os_memzero(&hldev->mrpcim->mrpcim_stats_sav,
	    sizeof(vxge_hal_mrpcim_stats_hw_info_t));

	status = __hal_mrpcim_mac_configure(hldev);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = __hal_mrpcim_lag_configure(hldev);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mdio_gen_cfg_port[0]);

	hldev->mrpcim->mdio_phy_prtad0 =
	    (u32) VXGE_HAL_MDIO_GEN_CFG_PORT_GET_MDIO_PHY_PRTAD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mdio_gen_cfg_port[1]);

	hldev->mrpcim->mdio_phy_prtad1 =
	    (u32) VXGE_HAL_MDIO_GEN_CFG_PORT_GET_MDIO_PHY_PRTAD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->xgxs_static_cfg_port[0]);

	hldev->mrpcim->mdio_dte_prtad0 =
	    (u32) VXGE_HAL_XGXS_STATIC_CFG_PORT_GET_MDIO_DTE_PRTAD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->xgxs_static_cfg_port[1]);

	hldev->mrpcim->mdio_dte_prtad1 =
	    (u32) VXGE_HAL_XGXS_STATIC_CFG_PORT_GET_MDIO_DTE_PRTAD(val64);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    hldev->mrpcim->mrpcim_stats_block->dma_addr,
	    &hldev->mrpcim_reg->mrpcim_stats_start_host_addr);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->mrpcim_reg->mrpcim_general_cfg2);

	val64 &= ~VXGE_HAL_MRPCIM_GENERAL_CFG2_MRPCIM_STATS_MAP_TO_VPATH(0x1f);
	val64 |= VXGE_HAL_MRPCIM_GENERAL_CFG2_MRPCIM_STATS_MAP_TO_VPATH(
	    hldev->first_vp_id);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &hldev->mrpcim_reg->mrpcim_general_cfg2);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    vBIT(0xFFFFFFFFFFFFFFFFULL, 0, VXGE_HAL_MAX_VIRTUAL_PATHS),
	    &hldev->mrpcim_reg->rxmac_authorize_all_addr);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    vBIT(0xFFFFFFFFFFFFFFFFULL, 0, VXGE_HAL_MAX_VIRTUAL_PATHS),
	    &hldev->mrpcim_reg->rxmac_authorize_all_vid);

	if (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_EMULATED_INTA) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->mrpcim_reg->rdcrdtarb_cfg0);

		/* Set MOST to 8 for HP-ISS platform */
		val64 &= ~VXGE_HAL_RDCRDTARB_CFG0_MAX_OUTSTANDING_RDS(0x3f);

		val64 |= VXGE_HAL_RDCRDTARB_CFG0_MAX_OUTSTANDING_RDS(8);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->mrpcim_reg->rdcrdtarb_cfg0);
	}

	(void) __hal_ifmsg_wmsg_post(hldev,
	    hldev->first_vp_id,
	    VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_BROADCAST,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_PRIV_DRIVER_UP,
	    0);

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);

}

/*
 * __hal_mrpcim_terminate - Terminates mrpcim
 * @hldev: hal device.
 *
 * Terminates mrpcim.
 *
 * See also: __hal_mrpcim_initialize()
 */
vxge_hal_status_e
__hal_mrpcim_terminate(__hal_device_t *hldev)
{
	vxge_hal_device_h devh = (vxge_hal_device_h) hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((hldev != NULL) && (hldev->mrpcim != NULL));

	vxge_hal_trace_log_mrpcim("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_mrpcim("hldev = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev);

	if (hldev->mrpcim == NULL) {
		vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	(void) __hal_ifmsg_wmsg_post(hldev,
	    hldev->first_vp_id,
	    VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_BROADCAST,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_PRIV_DRIVER_DOWN,
	    0);

	if (hldev->mrpcim->mrpcim_stats_block != NULL) {
		__hal_blockpool_block_free(devh,
		    hldev->mrpcim->mrpcim_stats_block);
		hldev->mrpcim->mrpcim_stats_block = NULL;
	}

	vxge_os_free(hldev->header.pdev,
	    hldev->mrpcim, sizeof(__hal_mrpcim_t));

	hldev->mrpcim = NULL;

	vxge_hal_trace_log_mrpcim("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}
