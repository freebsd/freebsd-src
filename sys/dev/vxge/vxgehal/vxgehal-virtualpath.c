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
 * __hal_vpath_fw_memo_get - Get the fw memo interface parameters
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle, or simply PCI device @pdev
 * (Linux and the rest.)
 * @vp_id: Vpath id
 * @vpath_reg: Pointer to vpath registers
 * @action: Action for FW Interface
 * @param_index: Index of the parameter
 * @data0: Buffer to return data 0 register contents
 * @data1: Buffer to return data 1 register contents
 *
 * Returns FW memo interface parameters
 *
 */
vxge_hal_status_e
__hal_vpath_fw_memo_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    u32 action,
    u64 param_index,
    u64 *data0,
    u64 *data1)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((vpath_reg != NULL) && (data0 != NULL) && (data1 != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "vp_id = %d, vpath_reg = 0x"VXGE_OS_STXFMT", action = %d, "
	    "param_index = %lld, data0 = 0x"VXGE_OS_STXFMT", "
	    "data1 = 0x"VXGE_OS_STXFMT, (ptr_t) pdev, (ptr_t) regh0,
	    vp_id, (ptr_t) vpath_reg, action, param_index,
	    (ptr_t) data0, (ptr_t) data1);

	vxge_os_pio_mem_write64(pdev,
	    regh0,
	    0,
	    &vpath_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_os_pio_mem_write64(pdev,
	    regh0,
	    param_index,
	    &vpath_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(pdev,
	    regh0,
	    0,
	    &vpath_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(action) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	vxge_hal_pio_mem_write32_lower(pdev,
	    regh0,
	    (u32) bVAL32(val64, 32),
	    &vpath_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(pdev,
	    regh0,
	    (u32) bVAL32(val64, 0),
	    &vpath_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(pdev, regh0,
	    &vpath_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * VXGE_HAL_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &vpath_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {

		*data0 = vxge_os_pio_mem_read64(pdev, regh0,
		    &vpath_reg->rts_access_steer_data0);

		*data1 = vxge_os_pio_mem_read64(pdev, regh0,
		    &vpath_reg->rts_access_steer_data1);

		status = VXGE_HAL_OK;

	} else {
		status = VXGE_HAL_FAIL;
	}


	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_fw_flash_ver_get - Get the fw version
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle, or simply PCI device @pdev
 * (Linux and the rest.)
 * @vp_id: Vpath id
 * @vpath_reg: Pointer to vpath registers
 * @fw_version: Buffer to return FW Version (Major)
 * @fw_date: Buffer to return FW Version (date)
 * @flash_version: Buffer to return FW Version (Major)
 * @flash_date: Buffer to return FW Version (date)
 *
 * Returns FW Version
 *
 */
vxge_hal_status_e
__hal_vpath_fw_flash_ver_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    vxge_hal_device_version_t *fw_version,
    vxge_hal_device_date_t *fw_date,
    vxge_hal_device_version_t *flash_version,
    vxge_hal_device_date_t *flash_date)
{
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((vpath_reg != NULL) && (fw_version != NULL) &&
	    (fw_date != NULL) && (flash_version != NULL) &&
	    (flash_date != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "vp_id = %d, vpath_reg = 0x"VXGE_OS_STXFMT", "
	    "fw_version = 0x"VXGE_OS_STXFMT", "
	    "fw_date = 0x"VXGE_OS_STXFMT", "
	    "flash_version = 0x"VXGE_OS_STXFMT", "
	    "flash_date = 0x"VXGE_OS_STXFMT,
	    (ptr_t) pdev, (ptr_t) regh0, vp_id, (ptr_t) vpath_reg,
	    (ptr_t) fw_version, (ptr_t) fw_date,
	    (ptr_t) flash_version, (ptr_t) flash_date);

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_VERSION,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_FW_VERSION,
	    &data1, &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	fw_date->day =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FW_VER_DAY(data1);
	fw_date->month =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MONTH(data1);
	fw_date->year =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FW_VER_YEAR(data1);

	(void) vxge_os_snprintf(fw_date->date, sizeof(fw_date->date),
	    "%2.2d/%2.2d/%4.4d",
	    fw_date->month, fw_date->day, fw_date->year);

	fw_version->major =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MAJOR(data1);
	fw_version->minor =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FW_VER_MINOR(data1);
	fw_version->build =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FW_VER_BUILD(data1);

	(void) vxge_os_snprintf(fw_version->version,
	    sizeof(fw_version->version),
	    "%d.%d.%d", fw_version->major,
	    fw_version->minor, fw_version->build);

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_FLASH_VERSION,
	    &data1, &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	flash_date->day =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FLASH_VER_DAY(data1);
	flash_date->month =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FLASH_VER_MONTH(data1);
	flash_date->year =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FLASH_VER_YEAR(data1);

	(void) vxge_os_snprintf(flash_date->date, sizeof(flash_date->date),
	    "%2.2d/%2.2d/%4.4d", flash_date->month, flash_date->day,
	    flash_date->year);

	flash_version->major =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FLASH_VER_MAJOR(data1);
	flash_version->minor =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FLASH_VER_MINOR(data1);
	flash_version->build =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FLASH_VER_BUILD(data1);

	(void) vxge_os_snprintf(flash_version->version,
	    sizeof(flash_version->version),
	    "%d.%d.%d", flash_version->major,
	    flash_version->minor, flash_version->build);

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_card_info_get - Get the card infor
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle, or simply PCI device @pdev
 * (Linux and the rest.)
 * @vp_id: Vpath id
 * @vpath_reg: Pointer to vpath registers
 * @serial_number: Buffer to return card serial number
 * @part_number: Buffer to return card part number
 * @product_description: Buffer to return card description
 *
 * Returns Card Info
 *
 */
vxge_hal_status_e
__hal_vpath_card_info_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    u8 *serial_number,
    u8 *part_number,
    u8 *product_description)
{
	u32 i, j;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((vpath_reg != NULL) && (serial_number != NULL) &&
	    (part_number != NULL) && (product_description != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "vp_id = %d, vpath_reg = 0x"VXGE_OS_STXFMT", "
	    "serial_number = 0x"VXGE_OS_STXFMT", "
	    "part_number = 0x"VXGE_OS_STXFMT", "
	    "product_description = 0x"VXGE_OS_STXFMT,
	    (ptr_t) pdev, (ptr_t) regh0, vp_id, (ptr_t) vpath_reg,
	    (ptr_t) serial_number, (ptr_t) part_number,
	    (ptr_t) product_description);

	*serial_number = 0;
	*part_number = 0;
	*product_description = 0;

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_SERIAL_NUMBER,
	    &data1, &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	/* LINTED */
	((u64 *) serial_number)[0] = vxge_os_ntohll(data1);

	/* LINTED */
	((u64 *) serial_number)[1] = vxge_os_ntohll(data2);

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PART_NUMBER,
	    &data1, &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	/* LINTED */
	((u64 *) part_number)[0] = vxge_os_ntohll(data1);

	/* LINTED */
	((u64 *) part_number)[1] = vxge_os_ntohll(data2);

	j = 0;

	for (i = VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_DESC_0;
	    i <= VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_DESC_3;
	    i++) {

		status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
		    i,
		    &data1, &data2);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		/* LINTED */
		((u64 *) product_description)[j++] = vxge_os_ntohll(data1);

		/* LINTED */
		((u64 *) product_description)[j++] = vxge_os_ntohll(data2);

	}

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_pmd_info_get - Get the PMD info
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle, or simply PCI device @pdev
 * (Linux and the rest.)
 * @vp_id: Vpath id
 * @vpath_reg: Pointer to vpath registers
 * @ports: Number of ports supported
 * @pmd_port0: Buffer to return PMD info for port 0
 * @pmd_port1: Buffer to return PMD info for port 1
 *
 * Returns PMD Info
 *
 */
vxge_hal_status_e
__hal_vpath_pmd_info_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    u32 *ports,
    vxge_hal_device_pmd_info_t *pmd_port0,
    vxge_hal_device_pmd_info_t *pmd_port1)
{
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((vpath_reg != NULL) &&
	    (pmd_port0 != NULL) && (pmd_port1 != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "vp_id = %d, vpath_reg = 0x"VXGE_OS_STXFMT", "
	    "ports = 0x"VXGE_OS_STXFMT", "
	    "pmd_port0 = 0x"VXGE_OS_STXFMT", "
	    "pmd_port1 = 0x"VXGE_OS_STXFMT,
	    (ptr_t) pdev, (ptr_t) regh0, vp_id, (ptr_t) vpath_reg,
	    (ptr_t) ports, (ptr_t) pmd_port0, (ptr_t) pmd_port1);

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORTS,
	    &data1, &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	*ports = (u32) data1;

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORT0_PMD_TYPE,
	    &data1, &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (data1) {

		pmd_port0->type = (u32) data1;

		status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORT0_PMD_VENDOR,
		    &data1, &data2);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		/* LINTED */
		((u64 *) pmd_port0->vendor)[0] = vxge_os_ntohll(data1);

		/* LINTED */
		((u64 *) pmd_port0->vendor)[1] = vxge_os_ntohll(data2);

		status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORT0_PMD_PARTNO,
		    &data1, &data2);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		/* LINTED */
		((u64 *) pmd_port0->part_num)[0] = vxge_os_ntohll(data1);

		/* LINTED */
		((u64 *) pmd_port0->part_num)[1] = vxge_os_ntohll(data2);

		status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORT0_PMD_SERNO,
		    &data1, &data2);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		/* LINTED */
		((u64 *) pmd_port0->ser_num)[0] = vxge_os_ntohll(data1);

		/* LINTED */
		((u64 *) pmd_port0->ser_num)[1] = vxge_os_ntohll(data2);
	} else {
		vxge_os_memzero(pmd_port0, sizeof(vxge_hal_device_pmd_info_t));
	}

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORT1_PMD_TYPE,
	    &data1, &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (data1) {

		pmd_port1->type = (u32) data1;

		status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORT1_PMD_VENDOR,
		    &data1, &data2);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		/* LINTED */
		((u64 *) pmd_port1->vendor)[0] = vxge_os_ntohll(data1);

		/* LINTED */
		((u64 *) pmd_port1->vendor)[1] = vxge_os_ntohll(data2);

		status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORT1_PMD_PARTNO,
		    &data1, &data2);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		/* LINTED */
		((u64 *) pmd_port1->part_num)[0] = vxge_os_ntohll(data1);

		/* LINTED */
		((u64 *) pmd_port1->part_num)[1] = vxge_os_ntohll(data2);

		status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_MEMO_CARD_INFO,
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PORT1_PMD_SERNO,
		    &data1, &data2);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		/* LINTED */
		((u64 *) pmd_port1->ser_num)[0] = vxge_os_ntohll(data1);

		/* LINTED */
		((u64 *) pmd_port1->ser_num)[1] = vxge_os_ntohll(data2);

	} else {
		vxge_os_memzero(pmd_port1, sizeof(vxge_hal_device_pmd_info_t));
	}

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_pci_func_mode_get - Get the pci mode
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle, or simply PCI device @pdev
 * (Linux and the rest.)
 * @vp_id: Vpath id
 * @vpath_reg: Pointer to vpath registers
 *
 * Returns pci function mode
 *
 */
u64
__hal_vpath_pci_func_mode_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg)
{
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath_reg != NULL);

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "vp_id = %d, vpath_reg = 0x"VXGE_OS_STXFMT,
	    (ptr_t) pdev, (ptr_t) regh0, vp_id, (ptr_t) vpath_reg);

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_GET_FUNC_MODE,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_PCI_MODE,
	    &data1, &data2);

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (data1);
}

/*
 * __hal_vpath_lag_mode_get - Get the LAG mode
 * @vpath: VIrtual Path
 *
 * Returns the LAG mode in use
 */
vxge_hal_device_lag_mode_e
__hal_vpath_lag_mode_get(__hal_virtualpath_t *vpath)
{
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	u32 lag_mode = VXGE_HAL_DEVICE_LAG_MODE_UNKNOWN;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	(void) __hal_vpath_fw_memo_get(hldev->header.pdev, hldev->header.regh0,
	    vpath->vp_id, vpath->vp_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_PORT_INFO,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_LAG_MODE,
	    &data1, &data2);

	if (VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_MEMO_ITEM_STATUS(data1) ==
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MEMO_ITEM_STATUS_SUCCESS) {
		lag_mode = (u32)
		    VXGE_HAL_RTS_ACCESS_STEER_DATA1_MEMO_ITEM_GET_LAG_MODE(data2);
		status = VXGE_HAL_OK;
	} else {
		status = VXGE_HAL_FAIL;
	}

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return ((vxge_hal_device_lag_mode_e) lag_mode);
}

/*
 * __hal_vpath_vpath_map_get - Get the vpath map
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle, or simply PCI device @pdev
 * (Linux and the rest.)
 * @vp_id: Vpath id
 * @vh: Virtual Hierrachy
 * @func: Function number
 * @vpath_reg: Pointer to vpath registers
 *
 * Returns vpath map for a give hierarchy and function
 *
 */
u64
__hal_vpath_vpath_map_get(pci_dev_h pdev, pci_reg_h regh0,
    u32 vp_id, u32 vh, u32 func,
    vxge_hal_vpath_reg_t *vpath_reg)
{
	u64 i;
	u64 val64 = 0ULL;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath_reg != NULL);

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "vp_id = %d, vh = %d, func = %d, vpath_reg = 0x"VXGE_OS_STXFMT,
	    (ptr_t) pdev, (ptr_t) regh0, vp_id, vh, func, (ptr_t) vpath_reg);

	status = __hal_vpath_fw_memo_get(pdev, regh0, vp_id, vpath_reg,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_VPATH_MAP,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_VH(vh) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNCTION(func),
	    &data1, &data2);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		if (data2 & VXGE_HAL_RTS_ACCESS_STEER_DATA1_IS_VPATH_ASSIGNED(i))
			val64 |= mBIT(i);
	}

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (val64);
}

/*
 * __hal_vpath_pci_read - Read the content of given address
 *			 in pci config space.
 * @vpath: Virtual Path object.
 * @offset: Configuration address(offset)to read from
 * @length: Length of the data (1, 2 or 4 bytes)
 * @val: Pointer to a buffer to return the content of the address
 *
 * Read from the vpath pci config space.
 *
 */
vxge_hal_status_e
__hal_vpath_pci_read(struct __hal_device_t *hldev,
    u32 vp_id, u32 offset,
    u32 length, void *val)
{
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((hldev != NULL) && (val != NULL));

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("hldev = 0x"VXGE_OS_STXFMT", vp_id = %d, "
	    "offset = %d, val = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev, vp_id, offset, (ptr_t) val);

	switch (length) {
	case 1:
		vxge_os_pci_read8(hldev->header.pdev,
		    hldev->header.cfgh,
		    offset,
		    ((u8 *) val));
		break;
	case 2:
		vxge_os_pci_read16(hldev->header.pdev,
		    hldev->header.cfgh,
		    offset,
		    ((u16 *) val));
		break;
	case 4:
		vxge_os_pci_read32(hldev->header.pdev,
		    hldev->header.cfgh,
		    offset,
		    ((u32 *) val));
		break;
	default:
		status = VXGE_HAL_FAIL;
		vxge_os_memzero(val, length);
		break;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_fw_upgrade - Upgrade the firmware
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle, or simply PCI device @pdev
 * (Linux and the rest.)
 * @vp_id: Vpath id
 * @vpath_reg: Pointer to vpath registers
 * @buffer: Buffer containing F/W image
 * @length: Length of F/W image
 *
 * Upgrade the firmware
 *
 */
vxge_hal_status_e
__hal_vpath_fw_upgrade(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    u8 *buffer,
    u32 length)
{
	u32 i = 0;
	u64 val64;
	u32 not_done = TRUE;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((vpath_reg != NULL) && (buffer != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "vp_id = %d, vpath_reg = 0x"VXGE_OS_STXFMT", "
	    "buffer = 0x"VXGE_OS_STXFMT", length = %d\n",
	    (ptr_t) pdev, (ptr_t) regh0, vp_id, (ptr_t) vpath_reg,
	    (ptr_t) buffer, length);

	vxge_os_pio_mem_write64(pdev,
	    regh0,
	    0,
	    &vpath_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_os_pio_mem_write64(pdev,
	    regh0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FW_UPGRADE_STREAM_SKIP,
	    &vpath_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(pdev,
	    regh0,
	    0,
	    &vpath_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_UPGRADE) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET_FW_UPGRADE_MODE);

	vxge_hal_pio_mem_write32_lower(pdev, regh0,
	    (u32) bVAL32(val64, 32),
	    &vpath_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(pdev, regh0,
	    (u32) bVAL32(val64, 0),
	    &vpath_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = __hal_device_register_stall(pdev, regh0,
	    &vpath_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * VXGE_HAL_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &vpath_reg->rts_access_steer_ctrl);

	if (!(val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS)) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	while (not_done) {
		if ((i + 16) > length) {
			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
			return (VXGE_HAL_FAIL);
		}
		vxge_os_pio_mem_write64(pdev, regh0, ((u64) (buffer[i])) |
		    ((u64) (buffer[i + 1]) << 8) |
		    ((u64) (buffer[i + 2]) << 16) |
		    ((u64) (buffer[i + 3]) << 24) |
		    ((u64) (buffer[i + 4]) << 32) |
		    ((u64) (buffer[i + 5]) << 40) |
		    ((u64) (buffer[i + 6]) << 48) |
		    ((u64) (buffer[i + 7]) << 56),
		    &vpath_reg->rts_access_steer_data0);

		vxge_os_pio_mem_write64(pdev, regh0,
		    ((u64) (buffer[i + 8])) |
		    ((u64) (buffer[i + 9]) << 8) |
		    ((u64) (buffer[i + 10]) << 16) |
		    ((u64) (buffer[i + 11]) << 24) |
		    ((u64) (buffer[i + 12]) << 32) |
		    ((u64) (buffer[i + 13]) << 40) |
		    ((u64) (buffer[i + 14]) << 48) |
		    ((u64) (buffer[i + 15]) << 56),
		    &vpath_reg->rts_access_steer_data1);
		vxge_os_wmb();

		val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_UPGRADE) |
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET_FW_UPGRADE_DATA);
		vxge_hal_pio_mem_write32_lower(pdev, regh0,
		    (u32) bVAL32(val64, 32),
		    &vpath_reg->rts_access_steer_ctrl);

		vxge_os_wmb();

		vxge_hal_pio_mem_write32_upper(pdev, regh0,
		    (u32) bVAL32(val64, 0), &vpath_reg->rts_access_steer_ctrl);
		vxge_os_wmb();

		status = __hal_device_register_stall(pdev, regh0,
		    &vpath_reg->rts_access_steer_ctrl, 0,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
		    WAIT_FACTOR * VXGE_HAL_DEF_DEVICE_POLL_MILLIS);
		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &vpath_reg->rts_access_steer_ctrl);
		if (!(val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS)) {
			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
			return (VXGE_HAL_FAIL);
		}

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &vpath_reg->rts_access_steer_data0);
		switch (VXGE_HAL_RTS_ACCESS_STEER_DATA0_FW_UPGRADE_GET_RET_CODE(val64)) {
		case VXGE_HAL_RTS_ACCESS_STEER_DATA0_FW_UPGRADE_GET_RET_CODE_OK:
			i += 16;
			break;
		case VXGE_HAL_RTS_ACCESS_STEER_DATA0_FW_UPGRADE_GET_RET_CODE_DONE:
			not_done = FALSE;
			break;
		case VXGE_HAL_RTS_ACCESS_STEER_DATA0_FW_UPGRADE_GET_RET_CODE_SKIP:
			i += 16;
			i += (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_FW_UPGRADE_GET_SKIP_BYTES(val64);
			break;
		case VXGE_HAL_RTS_ACCESS_STEER_DATA0_FW_UPGRADE_GET_RET_CODE_ERROR:
		default:
			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
			return (VXGE_HAL_FAIL);
		}
	}

	vxge_os_pio_mem_write64(pdev,
	    regh0,
	    0,
	    &vpath_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(pdev,
	    regh0,
	    0,
	    &vpath_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FW_UPGRADE) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET_FW_UPGRADE_COMMIT);

	vxge_hal_pio_mem_write32_lower(pdev,
	    regh0,
	    (u32) bVAL32(val64, 32),
	    &vpath_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(pdev,
	    regh0,
	    (u32) bVAL32(val64, 0),
	    &vpath_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = __hal_device_register_stall(pdev, regh0,
	    &vpath_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    100 * VXGE_HAL_DEF_DEVICE_POLL_MILLIS);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(pdev, regh0,
	    &vpath_reg->rts_access_steer_ctrl);

	if (!(val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS)) {

		vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, VXGE_HAL_OK);

	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_flick_link_led - Flick (blink) link LED.
 * @hldev: HAL device.
 * @vp_id: Vpath Id
 * @port : Port number 0, or 1
 * @on_off: TRUE if flickering to be on, FALSE to be off
 *
 * Flicker the link LED.
 */
vxge_hal_status_e
__hal_vpath_flick_link_led(struct __hal_device_t *hldev,
    u32 vp_id, u32 port, u32 on_off)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_vpath_reg_t *vp_reg;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "hldev = 0x"VXGE_OS_STXFMT", vp_id = %d, port = %d, on_off = %d",
	    (ptr_t) hldev, vp_id, port, on_off);

	vp_reg = hldev->vpath_reg[vp_id];

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) on_off,
	    &vp_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LED_CONTROL) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_udp_rth_set - Enable or Disable UDP/RTH.
 * @hldev: HAL device.
 * @vp_id: Vpath Id
 * @on_off: TRUE if UDP/RTH to be enabled, FALSE to be disabled
 *
 * Enable or Disable UDP/RTH.
 */
vxge_hal_status_e
__hal_vpath_udp_rth_set(
    struct __hal_device_t *hldev,
    u32 vp_id,
    u32 on_off)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_vpath_reg_t *vp_reg;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "hldev = 0x"VXGE_OS_STXFMT", vp_id = %d, on_off = %d",
	    (ptr_t) hldev, vp_id, on_off);

	vp_reg = hldev->vpath_reg[vp_id];

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    ((on_off) ? VXGE_HAL_RTS_ACCESS_STEER_DATA0_UDP_RTH_ENABLE : 0),
	    &vp_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_UDP_RTH) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_pcie_func_mode_set - Set PCI-E function mode.
 * @hldev: HAL device.
 * @vp_id: Vpath Id
 * @func_mode: func_mode to be set
 *
 * Set PCI-E function mode.
 */
vxge_hal_status_e
__hal_vpath_pcie_func_mode_set(struct __hal_device_t *hldev,
    u32 vp_id, u32 func_mode)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_vpath_reg_t *vp_reg;

	vxge_assert(hldev != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "hldev = 0x"VXGE_OS_STXFMT", vp_id = %d, func_mode = %d",
	    (ptr_t) hldev, vp_id, func_mode);

	vp_reg = hldev->vpath_reg[vp_id];

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_FUNC_MODE(func_mode),
	    &vp_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_FUNC_MODE) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl);

	if (!(val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS)) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_COMMIT) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(0);

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {
		status = VXGE_HAL_OK;
	} else {
		status = VXGE_HAL_FAIL;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_udp_rth_disable - Enable UDP/RTH.
 * @vpath_handle: Vpath handle.
 *
 * Disable udp rth
 *
 */
vxge_hal_status_e
vxge_hal_vpath_udp_rth_disable(vxge_hal_vpath_h vpath_handle)
{
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	status = __hal_vpath_udp_rth_set(hldev,
	    vpath->vp_id,
	    FALSE);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * __hal_vpath_rts_table_get - Get the entries from RTS access tables
 * @vpath_handle: Vpath handle.
 * @action: Identifies the action to take on the specified entry. The
 *	    interpretation of this field depends on the DATA_STRUCT_SEL field
 *	    DA, VID, ETYPE, PN, RANGE_PN:
 *		8'd0 - ADD_ENTRY (Add an entry to the table. This command may be
 *		rejected by management/administration).
 *		8'd1 - DELETE_ENTRY (Add an entry to the table. This command may
 *		be rejected by management/administration)
 *		8'd2 - LIST_FIRST_ENTRY
 *		8'd3 - LIST_NEXT_ENTRY
 *		RTH_GEN_CFG, RTH_IT, RTH_JHASH_CFG, RTH_MASK, RTH_KEY, QOS, DS:
 *		8'd0 - READ_ENTRY
 *		  8'd1 - WRITE_ENTRY
 *		Note: This field is updated by the H/W during an operation and
 *		is used to report additional TBD status information back to the
 *		host.
 * @rts_table: Identifies the RTS data structure (i.e. lookup table) to access.
 *		0; DA; Destination Address 1; VID; VLAN ID 2; ETYPE; Ethertype
 *		3; PN; Layer 4 Port Number 4; Reserved 5; RTH_GEN_CFG; Receive
 *		Traffic Hashing General Configuration 6; RTH_IT; Receive Traffic
 *		Hashing Indirection Table 7; RTH_JHASH_CFG; Receive-Traffic
 *		Hashing Jenkins Hash Configuration 8; RTH_MASK; Receive Traffic
 *		Hashing Mask 9; RTH_KEY; Receive-Traffic Hashing Key 10; QOS;
 *		VLAN Quality of Service 11; DS; IP Differentiated Services
 * @offset: Applies to RTH_IT, RTH_MASK, RTH_KEY, QOS, DS structures only.
 *		The interpretation of this field depends on the DATA_STRUCT_SEL
 *		field:
 *		RTH_IT - {BUCKET_NUM[0:7]} (Bucket Number)
 *		RTH_MASK - {5'b0,
 *		INDEX_8BYTE} (8-byte Index)
 *		RTH_KEY - {5'b0, INDEX_8BYTE} (8-byte Index)
 *		QOS - {5'b0, PRI} (Priority)
 *		DS - {5'b0, CP} (Codepoint)
 * @data1: Pointer to the data 1 to be read from the table
 * @data2: Pointer to the data 2 to be read from the table
 *
 * Read from the RTS table
 *
 */
vxge_hal_status_e
__hal_vpath_rts_table_get(
    vxge_hal_vpath_h vpath_handle,
    u32 action,
    u32 rts_table,
    u32 offset,
    u64 *data1,
    u64 *data2)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((vpath_handle != NULL) &&
	    (data1 != NULL) && (data2 != NULL));

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", action = %d, rts_table = %d, "
	    "offset = %d, data1 = 0x"VXGE_OS_STXFMT", data2 = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, action, rts_table, offset, (ptr_t) data1,
	    (ptr_t) data2);

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(action) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(rts_table) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(offset);


	if ((rts_table ==
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT) ||
	    (rts_table ==
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT) ||
	    (rts_table ==
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASK) ||
	    (rts_table ==
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_KEY)) {
		val64 |= VXGE_HAL_RTS_ACCESS_STEER_CTRL_TABLE_SEL;
	}

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &vpath->vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vpath->vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rts_access_steer_ctrl);

	if (!(val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS)) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_FAIL);
		return (VXGE_HAL_FAIL);
	}

	*data1 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rts_access_steer_data0);

	if ((rts_table ==
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA) ||
	    (rts_table ==
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT)) {
		*data2 = vxge_os_pio_mem_read64(
		    hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->rts_access_steer_data1);
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_rts_table_set - Set the entries of RTS access tables
 * @vpath_handle: Vpath handle.
 * @action: Identifies the action to take on the specified entry. The
 *		interpretation of this field depends on DATA_STRUCT_SEL field
 *		DA, VID, ETYPE, PN, RANGE_PN:
 *		8'd0 - ADD_ENTRY (Add an entry to the table. This command may be
 *		   rejected by management/administration).
 *		8'd1 - DELETE_ENTRY (Add an entry to the table. This command may
 *		   be rejected by management/administration)
 *		8'd2 - LIST_FIRST_ENTRY
 *		8'd3 - LIST_NEXT_ENTRY
 *		RTH_GEN_CFG, RTH_IT, RTH_JHASH_CFG, RTH_MASK, RTH_KEY, QOS, DS:
 *		8'd0 - READ_ENTRY
 *		  8'd1 - WRITE_ENTRY
 *		Note: This field is updated by the H/W during an operation and
 *		is used to report additional TBD status information back to the
 *		host.
 * @rts_table: Identifies the RTS data structure (i.e. lookup table) to access.
 *		0; DA; Destination Address 1; VID; VLAN ID 2; ETYPE; Ethertype
 *		3; PN; Layer 4 Port Number 4; Reserved 5; RTH_GEN_CFG; Receive
 *		Traffic Hashing General Configuration 6; RTH_IT; Receive Traffic
 *		Hashing Indirection Table 7; RTH_JHASH_CFG; Receive-Traffic
 *		Hashing Jenkins Hash Configuration 8; RTH_MASK; Receive Traffic
 *		Hashing Mask 9; RTH_KEY; Receive-Traffic Hashing Key 10; QOS;
 *		VLAN Quality of Service 11; DS; IP Differentiated Services
 * @offset: Applies to RTH_IT, RTH_MASK, RTH_KEY, QOS, DS structures only.
 *		The interpretation of this field depends on the DATA_STRUCT_SEL
 *		field:
 *		RTH_IT - {BUCKET_NUM[0:7]} (Bucket Number)
 *		RTH_MASK - {5'b0,
 *		INDEX_8BYTE} (8-byte Index)
 *		RTH_KEY - {5'b0, INDEX_8BYTE} (8-byte Index)
 *		QOS - {5'b0, PRI} (Priority)
 *		DS - {5'b0, CP} (Codepoint)
 * @data1: data 1 to be written to the table
 * @data2: data 2 to be written to the table
 *
 * Read from the RTS table
 *
 */
vxge_hal_status_e
__hal_vpath_rts_table_set(
    vxge_hal_vpath_h vpath_handle,
    u32 action,
    u32 rts_table,
    u32 offset,
    u64 data1,
    u64 data2)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", action = %d, rts_table = %d, "
	    "offset = %d, data1 = 0x"VXGE_OS_LLXFMT", data2 = 0x"VXGE_OS_LLXFMT,
	    (ptr_t) vpath_handle, action, rts_table, offset, data1, data2);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    data1,
	    &vpath->vp_reg->rts_access_steer_data0);
	vxge_os_wmb();

	if ((rts_table ==
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA) ||
	    (rts_table ==
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT)) {
		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    data2,
		    &vpath->vp_reg->rts_access_steer_data1);
		vxge_os_wmb();

	}

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(action) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(rts_table) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(offset);

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &vpath->vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vpath->vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status = vxge_hal_device_register_poll(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {

		status = VXGE_HAL_OK;

	} else {
		status = VXGE_HAL_FAIL;
	}


	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}


/*
 * vxge_hal_vpath_mac_addr_add - Add the mac address entry for this vpath
 *		  to MAC address table.
 * @vpath_handle: Vpath handle.
 * @macaddr: MAC address to be added for this vpath into the list
 * @macaddr_mask: MAC address mask for macaddr
 * @duplicate_mode: Duplicate MAC address add mode. Please see
 *		vxge_hal_vpath_mac_addr_add_mode_e {}
 *
 * Adds the given mac address and mac address mask into the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_delete, vxge_hal_vpath_mac_addr_get and
 * vxge_hal_vpath_mac_addr_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mac_addr_add(
    vxge_hal_vpath_h vpath_handle,
    macaddr_t macaddr,
    macaddr_t macaddr_mask,
    vxge_hal_vpath_mac_addr_add_mode_e duplicate_mode)
{
	u32 i;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "macaddr = %02x-%02x-%02x-%02x-%02x-%02x, "
	    "macaddr_mask = %02x-%02x-%02x-%02x-%02x-%02x",
	    (ptr_t) vpath_handle, macaddr[0], macaddr[1], macaddr[2],
	    macaddr[3], macaddr[4], macaddr[5], macaddr_mask[0],
	    macaddr_mask[1], macaddr_mask[2], macaddr_mask[3],
	    macaddr_mask[4], macaddr_mask[5]);

	for (i = 0; i < VXGE_HAL_ETH_ALEN; i++) {
		data1 <<= 8;
		data1 |= (u8) macaddr[i];
	}

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

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_ADD_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA,
	    0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_DA_MAC_ADDR(data1),
	    VXGE_HAL_RTS_ACCESS_STEER_DATA1_DA_MAC_ADDR_MASK(data2) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA1_DA_MAC_ADDR_MODE(i));

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * __hal_vpath_hw_addr_get - Get the hw address entry for this vpath
 *		  from MAC address table.
 * @pdev: PCI device object.
 * @regh0: BAR0 mapped memory handle, or simply PCI device @pdev
 * (Linux and the rest.)
 * @vp_id: Vpath id
 * @vpath_reg: Pointer to vpath registers
 * @macaddr: First MAC address entry for this vpath in the list
 * @macaddr_mask: MAC address mask for macaddr
 *
 * Returns the first mac address and mac address mask in the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_get_next
 *
 */
vxge_hal_status_e
__hal_vpath_hw_addr_get(
    pci_dev_h pdev,
    pci_reg_h regh0,
    u32 vp_id,
    vxge_hal_vpath_reg_t *vpath_reg,
    macaddr_t macaddr,
    macaddr_t macaddr_mask)
{
	u32 i;
	u64 val64;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	u64 action = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_FIRST_ENTRY;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((vpath_reg != NULL) && (macaddr != NULL) &&
	    (macaddr_mask != NULL));

	vxge_hal_trace_log_driver("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_driver(
	    "pdev = 0x"VXGE_OS_STXFMT", regh0 = 0x"VXGE_OS_STXFMT", "
	    "vp_id = %d, vpath_reg = 0x"VXGE_OS_STXFMT", "
	    "macaddr = 0x"VXGE_OS_STXFMT", macaddr_mask = 0x"VXGE_OS_STXFMT,
	    (ptr_t) pdev, (ptr_t) regh0, vp_id, (ptr_t) vpath_reg,
	    (ptr_t) macaddr, (ptr_t) macaddr_mask);

	/* CONSTCOND */
	while (TRUE) {

		val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(action) |
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA) |
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE |
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(0);

		vxge_hal_pio_mem_write32_lower(pdev,
		    regh0,
		    (u32) bVAL32(val64, 32),
		    &vpath_reg->rts_access_steer_ctrl);

		vxge_os_wmb();

		vxge_hal_pio_mem_write32_upper(pdev,
		    regh0,
		    (u32) bVAL32(val64, 0),
		    &vpath_reg->rts_access_steer_ctrl);

		vxge_os_wmb();

		status = vxge_hal_device_register_poll(pdev, regh0,
		    &vpath_reg->rts_access_steer_ctrl, 0,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
		    WAIT_FACTOR * VXGE_HAL_DEF_DEVICE_POLL_MILLIS);

		if (status != VXGE_HAL_OK) {

			vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}

		val64 = vxge_os_pio_mem_read64(pdev, regh0,
		    &vpath_reg->rts_access_steer_ctrl);

		if (val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS) {
			data1 = vxge_os_pio_mem_read64(pdev, regh0,
			    &vpath_reg->rts_access_steer_data0);
			data2 = vxge_os_pio_mem_read64(pdev, regh0,
			    &vpath_reg->rts_access_steer_data1);
			data1 =
			    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_DA_MAC_ADDR(data1);
			data2 =
			    VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_DA_MAC_ADDR_MASK(data2);

			if (VXGE_HAL_IS_UNICAST(data1)) {

				for (i = VXGE_HAL_ETH_ALEN; i > 0; i--) {
					macaddr[i - 1] = (u8) (data1 & 0xFF);
					data1 >>= 8;
				}
				for (i = VXGE_HAL_ETH_ALEN; i > 0; i--) {
				    macaddr_mask[i - 1] = (u8) (data2 & 0xFF);
				    data2 >>= 8;
				}
				status = VXGE_HAL_OK;
				break;
			}
			action = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXT_ENTRY;
		} else {
			status = VXGE_HAL_FAIL;
			break;
		}
	}

	vxge_hal_trace_log_driver("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_mac_addr_get - Get the first mac address entry for this vpath
 *		  from MAC address table.
 * @vpath_handle: Vpath handle.
 * @macaddr: First MAC address entry for this vpath in the list
 * @macaddr_mask: MAC address mask for macaddr
 *
 * Returns the first mac address and mac address mask in the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mac_addr_get(
    vxge_hal_vpath_h vpath_handle,
    macaddr_t macaddr,
    macaddr_t macaddr_mask)
{
	u32 i;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_FIRST_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA,
	    0,
	    &data1,
	    &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	data1 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_DA_MAC_ADDR(data1);

	data2 = VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_DA_MAC_ADDR_MASK(data2);

	for (i = VXGE_HAL_ETH_ALEN; i > 0; i--) {
		macaddr[i - 1] = (u8) (data1 & 0xFF);
		data1 >>= 8;
	}

	for (i = VXGE_HAL_ETH_ALEN; i > 0; i--) {
		macaddr_mask[i - 1] = (u8) (data2 & 0xFF);
		data2 >>= 8;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * vxge_hal_vpath_mac_addr_get_next - Get the next mac address entry for vpath
 *		  from MAC address table.
 * @vpath_handle: Vpath handle.
 * @macaddr: Next MAC address entry for this vpath in the list
 * @macaddr_mask: MAC address mask for macaddr
 *
 * Returns the next mac address and mac address mask in the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_get
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mac_addr_get_next(
    vxge_hal_vpath_h vpath_handle,
    macaddr_t macaddr,
    macaddr_t macaddr_mask)
{
	u32 i;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXT_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA,
	    0,
	    &data1,
	    &data2);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	data1 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_DA_MAC_ADDR(data1);

	data2 = VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_DA_MAC_ADDR_MASK(data2);

	for (i = VXGE_HAL_ETH_ALEN; i > 0; i--) {
		macaddr[i - 1] = (u8) (data1 & 0xFF);
		data1 >>= 8;
	}

	for (i = VXGE_HAL_ETH_ALEN; i > 0; i--) {
		macaddr_mask[i - 1] = (u8) (data2 & 0xFF);
		data2 >>= 8;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}


/*
 * vxge_hal_vpath_mac_addr_delete - Delete the mac address entry for this vpath
 *		  to MAC address table.
 * @vpath_handle: Vpath handle.
 * @macaddr: MAC address to be added for this vpath into the list
 * @macaddr_mask: MAC address mask for macaddr
 *
 * Delete the given mac address and mac address mask into the list for this
 * vpath.
 * see also: vxge_hal_vpath_mac_addr_add, vxge_hal_vpath_mac_addr_get and
 * vxge_hal_vpath_mac_addr_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mac_addr_delete(
    vxge_hal_vpath_h vpath_handle,
    macaddr_t macaddr,
    macaddr_t macaddr_mask)
{
	u32 i;
	u64 data1 = 0ULL;
	u64 data2 = 0ULL;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "macaddr = %02x-%02x-%02x-%02x-%02x-%02x, "
	    "macaddr_mask = %02x-%02x-%02x-%02x-%02x-%02x",
	    (ptr_t) vpath_handle, macaddr[0], macaddr[1], macaddr[2],
	    macaddr[3], macaddr[4], macaddr[5], macaddr_mask[0],
	    macaddr_mask[1], macaddr_mask[2], macaddr_mask[3],
	    macaddr_mask[4], macaddr_mask[5]);

	for (i = 0; i < VXGE_HAL_ETH_ALEN; i++) {
		data1 <<= 8;
		data1 |= (u8) macaddr[i];
	}

	for (i = 0; i < VXGE_HAL_ETH_ALEN; i++) {
		data2 <<= 8;
		data2 |= (u8) macaddr_mask[i];
	}

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_DELETE_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_DA,
	    0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_DA_MAC_ADDR(data1),
	    VXGE_HAL_RTS_ACCESS_STEER_DATA1_DA_MAC_ADDR_MASK(data2));

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_vid_add - Add the vlan id entry for this vpath
 *		  to vlan id table.
 * @vpath_handle: Vpath handle.
 * @vid: vlan id to be added for this vpath into the list
 *
 * Adds the given vlan id into the list for this  vpath.
 * see also: vxge_hal_vpath_vid_delete, vxge_hal_vpath_vid_get and
 * vxge_hal_vpath_vid_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_vid_add(
    vxge_hal_vpath_h vpath_handle,
    u64 vid)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT", vid = %d",
	    (ptr_t) vpath_handle, (u32) vid);

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_ADD_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_VID,
	    0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_VLAN_ID(vid),
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_vid_get - Get the first vid entry for this vpath
 *		  from vlan id table.
 * @vpath_handle: Vpath handle.
 * @vid: Buffer to return vlan id
 *
 * Returns the first vlan id in the list for this vpath.
 * see also: vxge_hal_vpath_vid_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_vid_get(
    vxge_hal_vpath_h vpath_handle,
    u64 *vid)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (vid != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", vid = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) vid);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_FIRST_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_VID,
	    0,
	    vid,
	    NULL);

	*vid = VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_VLAN_ID(*vid);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * vxge_hal_vpath_vid_get_next - Get the next vid entry for this vpath
 *		  from vlan id table.
 * @vpath_handle: Vpath handle.
 * @vid: Buffer to return vlan id
 *
 * Returns the next vlan id in the list for this vpath.
 * see also: vxge_hal_vpath_vid_get
 *
 */
vxge_hal_status_e
vxge_hal_vpath_vid_get_next(
    vxge_hal_vpath_h vpath_handle,
    u64 *vid)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (vid != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", vid = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) vid);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXT_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_VID,
	    0,
	    vid,
	    NULL);

	*vid = VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_VLAN_ID(*vid);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_vid_delete - Delete the vlan id entry for this vpath
 *		  to vlan id table.
 * @vpath_handle: Vpath handle.
 * @vid: vlan id to be added for this vpath into the list
 *
 * Adds the given vlan id into the list for this  vpath.
 * see also: vxge_hal_vpath_vid_add, vxge_hal_vpath_vid_get and
 * vxge_hal_vpath_vid_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_vid_delete(
    vxge_hal_vpath_h vpath_handle,
    u64 vid)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT", vid = %d",
	    (ptr_t) vpath_handle, (u32) vid);

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_DELETE_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_VID,
	    0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_VLAN_ID(vid),
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * vxge_hal_vpath_etype_add - Add the Ethertype entry for this vpath
 *		  to Ethertype table.
 * @vpath_handle: Vpath handle.
 * @etype: ethertype to be added for this vpath into the list
 *
 * Adds the given Ethertype into the list for this  vpath.
 * see also: vxge_hal_vpath_etype_delete, vxge_hal_vpath_etype_get and
 * vxge_hal_vpath_etype_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_etype_add(
    vxge_hal_vpath_h vpath_handle,
    u64 etype)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"
	    VXGE_OS_STXFMT", etype = %d",
	    (ptr_t) vpath_handle, (u32) etype);

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_ADD_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_ETYPE,
	    0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_ETYPE(etype),
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_etype_get - Get the first ethertype entry for this vpath
 *		  from Ethertype table.
 * @vpath_handle: Vpath handle.
 * @etype: Buffer to return Ethertype
 *
 * Returns the first ethype entry in the list for this vpath.
 * see also: vxge_hal_vpath_etype_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_etype_get(
    vxge_hal_vpath_h vpath_handle,
    u64 *etype)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (etype != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", etype = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) etype);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_FIRST_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_ETYPE,
	    0,
	    etype,
	    NULL);

	*etype = VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_ETYPE(*etype);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_etype_get_next - Get the next Ethertype entry for this vpath
 *		  from Ethertype table.
 * @vpath_handle: Vpath handle.
 * @etype: Buffer to return Ethwrtype
 *
 * Returns the next Ethwrtype in the list for this vpath.
 * see also: vxge_hal_vpath_etype_get
 *
 */
vxge_hal_status_e
vxge_hal_vpath_etype_get_next(
    vxge_hal_vpath_h vpath_handle,
    u64 *etype)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (etype != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", etype = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) etype);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXT_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_ETYPE,
	    0,
	    etype,
	    NULL);

	*etype = VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_ETYPE(*etype);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * vxge_hal_vpath_etype_delete - Delete the Ethertype entry for this vpath
 *		  to Ethertype table.
 * @vpath_handle: Vpath handle.
 * @etype: ethertype to be added for this vpath into the list
 *
 * Adds the given Ethertype into the list for this  vpath.
 * see also: vxge_hal_vpath_etype_add, vxge_hal_vpath_etype_get and
 * vxge_hal_vpath_etype_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_etype_delete(vxge_hal_vpath_h vpath_handle, u64 etype)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"
	    VXGE_OS_STXFMT", etype = %d",
	    (ptr_t) vpath_handle, (u32) etype);

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_DELETE_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_ETYPE,
	    0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_ETYPE(etype),
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_port_add - Add the port entry for this vpath
 *		  to port number table.
 * @vpath_handle: Vpath handle.
 * @port_type: if 0 - Src port or 1 - Dest port
 * @protocol: if 0 - TCP or 1 - UDP
 * @port: port to be added for this vpath into the list
 *
 * Adds the given port into the list for this  vpath.
 * see also: vxge_hal_vpath_port_delete, vxge_hal_vpath_port_get and
 * vxge_hal_vpath_port_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_port_add(
    vxge_hal_vpath_h vpath_handle,
    u32 port_type,
    u32 protocol,
    u32 port)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", port_type = %d, "
	    "protocol = %d, port = %d", (ptr_t) vpath_handle, port_type,
	    protocol, port);

	val64 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_PN_PORT_NUM(port);

	if (port_type)
		val64 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_PN_SRC_DEST_SEL;

	if (protocol)
		val64 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_PN_TCP_UDP_SEL;

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_ADD_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_PN,
	    0,
	    val64,
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * vxge_hal_vpath_port_get
 * Get the first port number entry for this vpath from port number table.
 * @vpath_handle: Vpath handle.
 * @port_type: Buffer to return if 0 - Src port or 1 - Dest port
 * @protocol: Buffer to return if 0 - TCP or 1 - UDP
 * @port: Buffer to return port number
 *
 * Returns the first port number entry in the list for this vpath.
 * see also: vxge_hal_vpath_port_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_port_get(
    vxge_hal_vpath_h vpath_handle,
    u32 *port_type,
    u32 *protocol,
    u32 *port)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (port_type != NULL) &&
	    (protocol != NULL) && (port != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", port_type = 0x"VXGE_OS_STXFMT
	    ", protocol = 0x"VXGE_OS_STXFMT", port = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) port_type, (ptr_t) protocol,
	    (ptr_t) port);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_FIRST_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_PN,
	    0,
	    &val64,
	    NULL);

	*port_type =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_PN_SRC_DEST_SEL(val64);
	*protocol =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_PN_TCP_UDP_SEL(val64);
	*port = (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_PN_PORT_NUM(val64);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * vxge_hal_vpath_port_get_next
 * Get the next port number entry for this vpath from port number table.
 * @vpath_handle: Vpath handle.
 * @port_type: Buffer to return if 0 - Src port or 1 - Dest port
 * @protocol: Buffer to return if 0 - TCP or 1 - UDP
 * @port: Buffer to return port number
 *
 * Returns the next port number entry in the list for this vpath.
 * see also: vxge_hal_vpath_port_get
 */
vxge_hal_status_e
vxge_hal_vpath_port_get_next(
    vxge_hal_vpath_h vpath_handle,
    u32 *port_type,
    u32 *protocol,
    u32 *port)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (port_type != NULL) &&
	    (protocol != NULL) && (port != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", port_type = 0x"VXGE_OS_STXFMT
	    ", protocol = 0x"VXGE_OS_STXFMT", port = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) port_type, (ptr_t) protocol,
	    (ptr_t) port);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_LIST_NEXT_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_PN,
	    0,
	    &val64,
	    NULL);

	*port_type =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_PN_SRC_DEST_SEL(val64);

	*protocol =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_PN_TCP_UDP_SEL(val64);

	*port = (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_PN_PORT_NUM(val64);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__,
	    status);

	return (status);
}

/*
 * vxge_hal_vpath_port_delete
 * Delete the port entry for this vpath to port number table.
 * @vpath_handle: Vpath handle.
 * @port_type: if 0 - Src port or 1 - Dest port
 * @protocol: if 0 - TCP or 1 - UDP
 * @port: port to be added for this vpath into the list
 *
 * Adds the given port into the list for this  vpath.
 * see also: vxge_hal_vpath_port_add, vxge_hal_vpath_port_get and
 * vxge_hal_vpath_port_get_next
 *
 */
vxge_hal_status_e
vxge_hal_vpath_port_delete(
    vxge_hal_vpath_h vpath_handle,
    u32 port_type,
    u32 protocol,
    u32 port)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", port_type = %d, "
	    "protocol = %d, port = %d", (ptr_t) vpath_handle, port_type,
	    protocol, port);

	val64 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_PN_PORT_NUM(port);

	if (port_type)
		val64 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_PN_SRC_DEST_SEL;

	if (protocol)
		val64 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_PN_TCP_UDP_SEL;

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_DELETE_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_PN,
	    0,
	    val64,
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_set - Set/configure RTS hashing.
 * @vpath_handle: Virtual Path handle.
 * @algorithm: Algorithm Select
 * @hash_type: Hash Type
 * @bucket_size: no of least significant bits to be used for hashing.
 * @it_switch: Itable switch required
 *
 * Used to set/configure all RTS hashing related stuff.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_itable_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_set(vxge_hal_vpath_h vpath_handle,
    vxge_hal_rth_algoritms_t algorithm,
    vxge_hal_rth_hash_types_t *hash_type,
    u16 bucket_size,
    u16 it_switch)
{
	u64 data0, data1;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp;

	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath_handle != NULL);

	vp = (__hal_vpath_handle_t *) vpath_handle;
	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", algorithm = %d, "
	    "hash_type = 0x"VXGE_OS_STXFMT", bucket_size = %d",
	    (ptr_t) vpath_handle, algorithm, (ptr_t) hash_type,
	    bucket_size);

	(void) __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_GEN_CFG,
	    0,
	    &data0,
	    &data1);

	if (algorithm == RTH_ALG_NONE) {

		data0 &= ~VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_EN;

	} else {

		if (it_switch) {

			if (VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_ACTIVE_TABLE(
			    data0))
				data0 = 0;
			else
				data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_ACTIVE_TABLE;

		} else {
			data0 &= VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_ACTIVE_TABLE;

		}

		data0 |= VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_EN |
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_BUCKET_SIZE(bucket_size) |
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_ALG_SEL(algorithm);

		if (hash_type->hash_type_tcpipv4_en)
			data0 |=
			    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV4_EN;

		if (hash_type->hash_type_ipv4_en)
			data0 |= VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV4_EN;

		if (hash_type->hash_type_tcpipv6_en)
			data0 |=
			    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV6_EN;

		if (hash_type->hash_type_ipv6_en)
			data0 |= VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV6_EN;

		if (hash_type->hash_type_tcpipv6ex_en)
			data0 |=
			    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_TCP_IPV6_EX_EN;

		if (hash_type->hash_type_ipv6ex_en)
			data0 |= VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_GEN_RTH_IPV6_EX_EN;

	}

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_GEN_CFG,
	    0,
	    data0,
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_get - Read RTS hashing.
 * @vpath_handle: Virtual Path handle.
 * @algorithm: Buffer to return Algorithm Select
 * @hash_type: Buffer to return Hash Type
 * @table_select: Buffer to return active Table
 * @bucket_size: Buffer to return no of least significant bits used for hashing.
 *
 * Used to read all RTS hashing related stuff.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_itable_set(),
 *		vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_get(vxge_hal_vpath_h vpath_handle,
    vxge_hal_rth_algoritms_t *algorithm,
    vxge_hal_rth_hash_types_t *hash_type,
    u8 *table_select,
    u16 *bucket_size)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", algorithm = 0x"VXGE_OS_STXFMT
	    ", hash_type = 0x"VXGE_OS_STXFMT", "
	    "table_select = 0x"VXGE_OS_STXFMT", "
	    "bucket_size = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle,
	    (ptr_t) algorithm, (ptr_t) hash_type,
	    (ptr_t) table_select, (ptr_t) bucket_size);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_GEN_CFG,
	    0,
	    &val64,
	    NULL);

	*algorithm = (vxge_hal_rth_algoritms_t)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_ALG_SEL(val64);

	hash_type->hash_type_tcpipv4_en = ((u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_TCP_IPV4_EN(val64))
	    ? 1 : 0;

	hash_type->hash_type_ipv4_en = ((u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_IPV4_EN(val64))
	    ? 1 : 0;

	hash_type->hash_type_tcpipv6_en = ((u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_TCP_IPV6_EN(val64))
	    ? 1 : 0;

	hash_type->hash_type_ipv6_en = ((u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_IPV6_EN(val64))
	    ? 1 : 0;

	hash_type->hash_type_tcpipv6ex_en = ((u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_TCP_IPV6_EX_EN(
	    val64)) ? 1 : 0;

	hash_type->hash_type_ipv6ex_en = ((u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_RTH_IPV6_EX_EN(val64))
	    ? 1 : 0;

	*table_select = ((u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_ACTIVE_TABLE(val64))
	    ? 1 : 0;

	*bucket_size = (u16)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_GEN_BUCKET_SIZE(val64);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_key_set - Configure 40byte secret for hash calc.
 *
 * @vpath_handle: Virtual Path ahandle.
 * @KeySize: Number of 64-bit words
 * @Key: up to 40-byte array of 64-bit values
 * This function configures the 40-byte secret which is used for hash
 * calculation.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_key_set(vxge_hal_vpath_h vpath_handle,
    u8 KeySize, u64 *Key)
{
	u32 i;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (Key != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", KeySize = %d"
	    ", Key = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle, KeySize,
	    (ptr_t) Key);

	for (i = 0; i < KeySize; i++) {

		status = __hal_vpath_rts_table_set(vpath_handle,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_KEY,
		    i,
		    vxge_os_htonll(*Key++),
		    0);

		if (status != VXGE_HAL_OK)
			break;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_key_get - Read 40byte secret for hash calc.
 *
 * @vpath_handle: Virtual Path ahandle.
 * @KeySize: Number of 64-bit words
 * @Key: Buffer to return the key
 * This function reads the 40-byte secret which is used for hash
 * calculation.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set(),
 *		vxge_hal_vpath_rts_rth_key_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_key_get(vxge_hal_vpath_h vpath_handle,
    u8 KeySize, u64 *Key)
{
	u32 i;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (Key != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", KeySize = %d"
	    ", Key = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle, KeySize,
	    (ptr_t) Key);

	for (i = 0; i < KeySize; i++) {

		status = __hal_vpath_rts_table_get(vpath_handle,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_KEY,
		    i,
		    Key++,
		    NULL);

		if (status != VXGE_HAL_OK)
			break;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_jhash_cfg_set - Configure JHASH algorithm
 *
 * @vpath_handle: Virtual Path ahandle.
 * @golden_ratio: Golden ratio
 * @init_value: Initial value
 * This function configures JENKIN's HASH algorithm
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_jhash_cfg_set(vxge_hal_vpath_h vpath_handle,
    u32 golden_ratio, u32 init_value)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", golden_ratio = %d"
	    ", init_value = %d", (ptr_t) vpath_handle, golden_ratio,
	    init_value);

	status = __hal_vpath_rts_table_set(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_JHASH_CFG,
	    0,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_JHASH_CFG_GOLDEN_RATIO(
	    golden_ratio) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_JHASH_CFG_INIT_VALUE(
	    init_value),
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_jhash_cfg_get - Read JHASH algorithm
 *
 * @vpath_handle: Virtual Path ahandle.
 * @golden_ratio: Buffer to return Golden ratio
 * @init_value: Buffer to return Initial value
 * This function reads JENKIN's HASH algorithm
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set(),
 *		vxge_hal_vpath_rts_rth_jhash_cfg_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_jhash_cfg_get(vxge_hal_vpath_h vpath_handle,
    u32 * golden_ratio, u32 *init_value)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "golden_ratio = 0x"VXGE_OS_STXFMT", init_value = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) golden_ratio, (ptr_t) init_value);

	status = __hal_vpath_rts_table_get(vpath_handle,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_JHASH_CFG,
	    0,
	    &val64,
	    NULL);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	*golden_ratio = (u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_JHASH_CFG_GOLDEN_RATIO(
	    val64);

	*init_value = (u32)
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_JHASH_CFG_INIT_VALUE(
	    val64);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_mask_set - Set/configure JHASH mask.
 * @vpath_handle: Virtual Path ahandle.
 * @table_size: Size of the mask table
 * @hash_mask_ipv6sa: IPv6SA Hash Mask
 * @hash_mask_ipv6da: IPv6DA Hash Mask
 * @hash_mask_ipv4sa: IPv4SA Hash Mask
 * @hash_mask_ipv4da: IPv4DA Hash Mask
 * @hash_mask_l4sp: L4SP Hash Mask
 * @hash_mask_l4dp: L4DP Hash Mask
 *
 * Used to set/configure indirection table.
 * It enables the required no of entries in the IT.
 * It adds entries to the IT.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_mask_set(vxge_hal_vpath_h vpath_handle,
    u32 table_size,
    u32 *hash_mask_ipv6sa,
    u32 *hash_mask_ipv6da,
    u32 *hash_mask_ipv4sa,
    u32 *hash_mask_ipv4da,
    u32 *hash_mask_l4sp,
    u32 *hash_mask_l4dp)
{
	u32 i;
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (hash_mask_ipv6sa != NULL) &&
	    (hash_mask_ipv6da != NULL) && (hash_mask_ipv4sa != NULL) &&
	    (hash_mask_ipv4da != NULL) && (hash_mask_l4sp != NULL) &&
	    (hash_mask_l4dp != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "table_size = %d, hash_mask_ipv6sa = 0x"VXGE_OS_STXFMT
	    ", hash_mask_ipv6da = 0x"VXGE_OS_STXFMT
	    ", hash_mask_ipv4sa = 0x"VXGE_OS_STXFMT
	    ", hash_mask_ipv4da = 0x"VXGE_OS_STXFMT
	    ", hash_mask_l4sp = 0x"VXGE_OS_STXFMT
	    ", hash_mask_l4dp = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, table_size, (ptr_t) hash_mask_ipv6sa,
	    (ptr_t) hash_mask_ipv6da, (ptr_t) hash_mask_ipv4sa,
	    (ptr_t) hash_mask_ipv4da, (ptr_t) hash_mask_l4sp,
	    (ptr_t) hash_mask_l4dp);

	for (i = 0; i < table_size; i++) {

		val64 =
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV6_SA_MASK(
		    *hash_mask_ipv6sa++) |
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV6_DA_MASK(
		    *hash_mask_ipv6da++) |
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV4_SA_MASK(
		    *hash_mask_ipv4sa++) |
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV4_DA_MASK(
		    *hash_mask_ipv4da++) |
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_L4SP_MASK(
		    *hash_mask_l4sp++) |
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_L4DP_MASK(
		    *hash_mask_l4dp++);

		status = __hal_vpath_rts_table_set(vpath_handle,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASK,
		    i,
		    val64,
		    0);

		if (status != VXGE_HAL_OK)
			break;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_mask_get - Read JHASH mask.
 * @vpath_handle: Virtual Path ahandle.
 * @table_size: Size of the mask table
 * @hash_mask_ipv6sa: Buffer to return IPv6SA Hash Mask
 * @hash_mask_ipv6da: Buffer to return IPv6DA Hash Mask
 * @hash_mask_ipv4sa: Buffer to return IPv4SA Hash Mask
 * @hash_mask_ipv4da: Buffer to return IPv4DA Hash Mask
 * @hash_mask_l4sp: Buffer to return L4SP Hash Mask
 * @hash_mask_l4dp: Buffer to return L4DP Hash Mask
 *
 * Used to read rth mask.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set(),
 *	  vxge_hal_vpath_rts_rth_mask_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_mask_get(vxge_hal_vpath_h vpath_handle,
    u32 table_size,
    u32 *hash_mask_ipv6sa,
    u32 *hash_mask_ipv6da,
    u32 *hash_mask_ipv4sa,
    u32 *hash_mask_ipv4da,
    u32 *hash_mask_l4sp,
    u32 *hash_mask_l4dp)
{
	u32 i;
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (hash_mask_ipv6sa != NULL) &&
	    (hash_mask_ipv6da != NULL) && (hash_mask_ipv4sa != NULL) &&
	    (hash_mask_ipv4da != NULL) && (hash_mask_l4sp != NULL) &&
	    (hash_mask_l4dp != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "table_size = %d, hash_mask_ipv6sa = 0x"VXGE_OS_STXFMT
	    ", hash_mask_ipv6da = 0x"VXGE_OS_STXFMT
	    ", hash_mask_ipv4sa = 0x"VXGE_OS_STXFMT
	    ", hash_mask_ipv4da = 0x"VXGE_OS_STXFMT
	    ", hash_mask_l4sp = 0x"VXGE_OS_STXFMT
	    ", hash_mask_l4dp = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, table_size, (ptr_t) hash_mask_ipv6sa,
	    (ptr_t) hash_mask_ipv6da, (ptr_t) hash_mask_ipv4sa,
	    (ptr_t) hash_mask_ipv4da, (ptr_t) hash_mask_l4sp,
	    (ptr_t) hash_mask_l4dp);

	for (i = 0; i < table_size; i++) {

		status = __hal_vpath_rts_table_get(vpath_handle,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY,
		    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MASK,
		    i,
		    &val64,
		    NULL);

		if (status != VXGE_HAL_OK)
			break;

		*hash_mask_ipv6sa++ = (u32)
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_MASK_IPV6_SA_MASK(
		    val64);

		*hash_mask_ipv6da++ = (u32)
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV6_DA_MASK(
		    val64);

		*hash_mask_ipv4sa++ = (u32)
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV4_SA_MASK(
		    val64);

		*hash_mask_ipv4da++ = (u32)
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_IPV4_DA_MASK(
		    val64);

		*hash_mask_l4sp++ = (u32)
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_L4SP_MASK(val64);

		*hash_mask_l4dp++ = (u32)
		    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_MASK_L4DP_MASK(val64);

	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_itable_set - Set/configure indirection table (IT).
 * @vpath_handles: Virtual Path handles.
 * @vpath_count: Number of vpath handles passed in vpath_handles
 * @itable: Pointer to indirection table
 * @itable_size: Number of entries in itable
 *
 * Used to set/configure indirection table.
 * It enables the required no of entries in the IT.
 * It adds entries to the IT.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_itable_set(vxge_hal_vpath_h *vpath_handles,
    u32 vpath_count,
    u8 *itable,
    u32 itable_size)
{
	u32 i, j, k, l, items[4];
	u64 data0;
	u64 data1;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handles[0];

	vxge_assert((vpath_handles != NULL) && (itable != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", vpath_count = %d, "
	    "itable = 0x"VXGE_OS_STXFMT", itable_size = %d",
	    (ptr_t) vpath_handles, vpath_count, (ptr_t) itable, itable_size);

	if (hldev->header.config.rth_it_type == VXGE_HAL_RTH_IT_TYPE_SOLO_IT) {

		for (j = 0; j < itable_size; j++) {

			data1 = 0;

			data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_SOLO_IT_BUCKET_DATA(
			    itable[j]);

			status = __hal_vpath_rts_table_set(vpath_handles[0],
			    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
			    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT,
			    j,
			    data0,
			    data1);

			if (status != VXGE_HAL_OK) {
				vxge_hal_trace_log_vpath(
				    "<== %s:%s:%d Result: %d",
				    __FILE__, __func__, __LINE__,
				    status);

				return (status);
			}
		}

		for (j = 0; j < itable_size; j++) {

			data1 = 0;

			data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_SOLO_IT_ENTRY_EN |
			    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_SOLO_IT_BUCKET_DATA(itable[j]);

			status = __hal_vpath_rts_table_set(vpath_handles[itable[j]],
			    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
			    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT,
			    j, data0, data1);

			if (status != VXGE_HAL_OK) {
				vxge_hal_trace_log_vpath(
				    "<== %s:%s:%d Result: %d",
				    __FILE__, __func__, __LINE__,
				    status);
				return (status);
			}
		}

	} else {
		for (i = 0; i < vpath_count; i++) {

			for (k = 0, j = 0; k < itable_size; k++) {

				if (itable[k] != i)
					continue;

				for (l = j; l < 4; l++)
					items[l] = k;

				if ((j++ == 3) || (k == (itable_size - 1))) {

					data0 =
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_BUCKET_NUM(
					    items[0]) |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_ENTRY_EN |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_BUCKET_DATA(
					    itable[items[0]]) |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_NUM(
					    items[1]) |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_ENTRY_EN |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_DATA(
					    itable[items[1]]);

					data1 =
					    VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_BUCKET_NUM(
					    items[2]) |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_ENTRY_EN |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_BUCKET_DATA(
					    itable[items[2]]) |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_BUCKET_NUM(
					    items[3]) |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_ENTRY_EN |
					    VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_BUCKET_DATA(
					    itable[items[3]]);

					status =
					    __hal_vpath_rts_table_set(vpath_handles[i],
					    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
					    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT,
					    0,
					    data0,
					    data1);

					if (status != VXGE_HAL_OK) {
						vxge_hal_trace_log_vpath(
						    "<== %s:%s:%d  Result: %d",
						    __FILE__, __func__,
						    __LINE__, status);

						return (status);
					}

					j = 0;
				}
			}
		}
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_itable_get - Read indirection table(IT).
 * @vpath_handles: Virtual Path handles.
 * @vpath_count: Number of vpath handles passed in vpath_handles
 * @itable: Pointer to the buffer to return indirection table
 * @itable_size: pointer to buffer to return Number of entries in itable
 *
 * Used to read indirection table.
 *
 * See also: vxge_hal_vpath_rts_rth_clr(), vxge_hal_vpath_rts_rth_set(),
 *		vxge_hal_vpath_rts_rth_itable_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_itable_get(vxge_hal_vpath_h *vpath_handles,
    u32 vpath_count,
    u8 *itable,
    u32 itable_size)
{
	u32 i, j;
	u64 data0;
	u64 data1;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handles[0];

	vxge_assert((vpath_handles != NULL) && (itable != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", vpath_count = %d, "
	    "itable = 0x"VXGE_OS_STXFMT", itable_size = %d",
	    (ptr_t) vpath_handles, vpath_count, (ptr_t) itable, itable_size);

	if (hldev->header.config.rth_it_type == VXGE_HAL_RTH_IT_TYPE_SOLO_IT) {

		for (i = 0; i < vpath_count; i++) {

			for (j = 0; j < itable_size; j++) {

				status = __hal_vpath_rts_table_get(vpath_handles[i],
				    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY,
				    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT,
				    j,
				    &data0,
				    &data1);

				if (status != VXGE_HAL_OK) {
					vxge_hal_trace_log_vpath(
					    "<== %s:%s:%d Result: %d",
					    __FILE__, __func__, __LINE__,
					    status);

					return (status);
				}

				if ((u8) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_SOLO_IT_ENTRY_EN(data0)) {
					itable[j] = (u8) i;
				}
			}
		}
	} else {

		for (i = 0; i < vpath_count; i++) {

			for (j = 0; j < itable_size; ) {

				data0 = 0;
				data1 = 0;

				if (j < itable_size)
					data0 =
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_BUCKET_NUM(j);

				if (j + 1 < itable_size)
					data0 |= VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_NUM(j + 1);

				if (j + 2 < itable_size)
					data1 = VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_BUCKET_NUM(j + 2);

				if (j + 3 < itable_size)
					data1 |= VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_BUCKET_NUM(j + 3);

				status = __hal_vpath_rts_table_get(
				    vpath_handles[i],
				    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_READ_ENTRY,
				    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT,
				    0, &data0, &data1);

				if (status != VXGE_HAL_OK) {
					vxge_hal_trace_log_vpath(
					    "<== %s:%s:%d Result: %d",
					    __FILE__, __func__, __LINE__,
					    status);

					return (status);
				}

				if (j < itable_size) {
					if ((u8)
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_ITEM0_ENTRY_EN(data0)) {
						itable[j] = (u8) i;
					}

					j++;
				}

				if (j < itable_size) {
					if ((u8)
					    VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_RTH_ITEM1_ENTRY_EN(data0)) {
						itable[j] = (u8) i;
					}
					j++;
				}

				if (j < itable_size) {
					if ((u8)
					    VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM0_ENTRY_EN(data1)) {
						itable[j] = (u8) i;
					}
					j++;
				}

				if (j < itable_size) {
					if ((u8)
					    VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_RTH_ITEM1_ENTRY_EN(data1)) {
						itable[j] = (u8) i;
					}
					j++;
				}
			}
		}
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_rts_rth_clr - Clear RTS hashing.
 * @vpath_handles: Virtual Path handles.
 * @vpath_count: Number of vpath handles passed in vpath_handles
 *
 * This function is used to clear all RTS hashing related stuff.
 *
 * See also: vxge_hal_vpath_rts_rth_set(), vxge_hal_vpath_rts_rth_itable_set().
 */
vxge_hal_status_e
vxge_hal_vpath_rts_rth_clr(
    vxge_hal_vpath_h *vpath_handles,
    u32 vpath_count)
{
	u64 data0, data1;
	u32 i, j;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath_handles != NULL);

	vp = (__hal_vpath_handle_t *) vpath_handles[0];

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handles = 0x"VXGE_OS_STXFMT", vpath_count = %d",
	    (ptr_t) vpath_handles, vpath_count);

	if (hldev->header.config.rth_it_type == VXGE_HAL_RTH_IT_TYPE_SOLO_IT) {

		for (j = 0; j < VXGE_HAL_MAX_ITABLE_ENTRIES; j++) {

			data0 = 0;
			data1 = 0;

			status = __hal_vpath_rts_table_set(vpath_handles[0],
			    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
			    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_SOLO_IT,
			    j,
			    data0,
			    data1);

			if (status != VXGE_HAL_OK) {
				vxge_hal_trace_log_vpath(
				    "<== %s:%s:%d Result: %d",
				    __FILE__, __func__, __LINE__,
				    status);

				return (status);
			}
		}
	} else {
		for (i = 0; i < vpath_count; i++) {

			for (j = 0; j < VXGE_HAL_MAX_ITABLE_ENTRIES; j += 4) {

				data0 =
				    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM0_BUCKET_NUM(j) |
				    VXGE_HAL_RTS_ACCESS_STEER_DATA0_RTH_ITEM1_BUCKET_NUM(j + 1);

				data1 =
				    VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM0_BUCKET_NUM(j + 2) |
				    VXGE_HAL_RTS_ACCESS_STEER_DATA1_RTH_ITEM1_BUCKET_NUM(j + 3);

				status = __hal_vpath_rts_table_set(vpath_handles[i],
				    VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_WRITE_ENTRY,
				    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_RTH_MULTI_IT,
				    0,
				    data0,
				    data1);

				if (status != VXGE_HAL_OK) {
					vxge_hal_trace_log_vpath(
					    "<== %s:%s:%d Result: %d",
					    __FILE__, __func__, __LINE__,
					    status);
					return (status);
				}
			}
		}
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_promisc_enable - Enable promiscuous mode.
 * @vpath_handle: Vpath handle.
 *
 * Enable promiscuous mode of X3100 operation.
 *
 * See also: vxge_hal_vpath_promisc_disable().
 */
vxge_hal_status_e
vxge_hal_vpath_promisc_enable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->promisc_en == VXGE_HAL_VP_PROMISC_ENABLE) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 |= VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN |
	    VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN |
	    VXGE_HAL_RXMAC_VCFG0_BCAST_EN |
	    VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->promisc_en = VXGE_HAL_VP_PROMISC_ENABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_promisc_disable - Disable promiscuous mode.
 * @vpath_handle: Vpath handle.
 *
 * Disable promiscuous mode of X3100 operation.
 *
 * See also: vxge_hal_vpath_promisc_enable().
 */
vxge_hal_status_e
vxge_hal_vpath_promisc_disable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->promisc_en == VXGE_HAL_VP_PROMISC_DISABLE) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	if (vpath->vp_config->rpa_ucast_all_addr_en ==
	    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_DISABLE) {
		val64 &= ~VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN;
	}

	if (vpath->vp_config->rpa_mcast_all_addr_en ==
	    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_DISABLE) {
		val64 &= ~VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN;
	}

	if (vpath->vp_config->rpa_bcast_en ==
	    VXGE_HAL_VPATH_RPA_BCAST_DISABLE) {
		val64 &= ~VXGE_HAL_RXMAC_VCFG0_BCAST_EN;
	}

	if (vpath->vp_config->rpa_all_vid_en ==
	    VXGE_HAL_VPATH_RPA_ALL_VID_DISABLE) {
		val64 &= ~VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;
	}

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->promisc_en = VXGE_HAL_VP_PROMISC_DISABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_bcast_enable - Enable broadcast
 * @vpath_handle: Vpath handle.
 *
 * Enable receiving broadcasts.
 */
vxge_hal_status_e
vxge_hal_vpath_bcast_enable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_HANDLE);

		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_bcast_en ==
	    VXGE_HAL_VPATH_RPA_BCAST_ENABLE) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 |= VXGE_HAL_RXMAC_VCFG0_BCAST_EN;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_bcast_en = VXGE_HAL_VPATH_RPA_BCAST_ENABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_bcast_disable - Disable broadcast
 * @vpath_handle: Vpath handle.
 *
 * Disable receiving broadcasts.
 */
vxge_hal_status_e
vxge_hal_vpath_bcast_disable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_bcast_en ==
	    VXGE_HAL_VPATH_RPA_BCAST_DISABLE) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 &= ~VXGE_HAL_RXMAC_VCFG0_BCAST_EN;

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_bcast_en = VXGE_HAL_VPATH_RPA_BCAST_DISABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_mcast_enable - Enable multicast addresses.
 * @vpath_handle: Vpath handle.
 *
 * Enable X3100 multicast addresses.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mcast_enable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_mcast_all_addr_en ==
	    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_ENABLE) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 |= VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN;

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_mcast_all_addr_en =
	    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_ENABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_mcast_disable - Disable  multicast addresses.
 * @vpath_handle: Vpath handle.
 *
 * Disable X3100 multicast addresses.
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to disable mcast
 * feature within the time(timeout).
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mcast_disable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_mcast_all_addr_en ==
	    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_DISABLE) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 &= ~VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN;

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_mcast_all_addr_en =
	    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_DISABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_ucast_enable - Enable unicast addresses.
 * @vpath_handle: Vpath handle.
 *
 * Enable X3100 unicast addresses.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_ucast_enable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_ucast_all_addr_en ==
	    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_ENABLE) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 |= VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN;

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_ucast_all_addr_en =
	    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_ENABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_ucast_disable - Disable  unicast addresses.
 * @vpath_handle: Vpath handle.
 *
 * Disable X3100 unicast addresses.
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_INF_MEM_STROBE_CMD_EXECUTING - Failed to disable mcast
 * feature within the time(timeout).
 *
 */
vxge_hal_status_e
vxge_hal_vpath_ucast_disable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_ucast_all_addr_en ==
	    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_DISABLE) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 &= ~VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN;

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_ucast_all_addr_en =
	    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_DISABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_all_vid_enable - Enable all Vlan Ids.
 * @vpath_handle: Vpath handle.
 *
 * Enable X3100 vlan ids.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_all_vid_enable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_all_vid_en ==
	    VXGE_HAL_VPATH_RPA_ALL_VID_ENABLE) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 |= VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_all_vid_en = VXGE_HAL_VPATH_RPA_ALL_VID_ENABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_all_vid_disable - Disable all Vlan Ids.
 * @vpath_handle: Vpath handle.
 *
 * Disable X3100  vlan ids.
 * Returns: VXGE_HAL_OK - success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_all_vid_disable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_all_vid_en ==
	    VXGE_HAL_VPATH_RPA_ALL_VID_DISABLE) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	val64 &= ~VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;

	vxge_os_pio_mem_write64(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_all_vid_en = VXGE_HAL_VPATH_RPA_ALL_VID_DISABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_strip_vlan_tag_enable - Enable strip vlan tag.
 * @vpath_handle: Vpath handle.
 *
 * Enable X3100  strip vlan tag.
 * Returns: VXGE_HAL_OK on success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_strip_vlan_tag_enable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_strip_vlan_tag ==
	    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_ENABLE) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->xmac_rpa_vcfg);

	val64 |= VXGE_HAL_XMAC_RPA_VCFG_STRIP_VLAN_TAG;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->xmac_rpa_vcfg);

	vpath->vp_config->rpa_strip_vlan_tag =
	    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_ENABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_strip_vlan_tag_disable - Disable strip vlan tag.
 * @vpath_handle: Vpath handle.
 *
 * Disable X3100  strip vlan tag.
 * Returns: VXGE_HAL_OK - success.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_strip_vlan_tag_disable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	if (vpath->vp_config->rpa_strip_vlan_tag ==
	    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_DISABLE) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_OK);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->xmac_rpa_vcfg);

	val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_STRIP_VLAN_TAG;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->xmac_rpa_vcfg);

	vpath->vp_config->rpa_strip_vlan_tag =
	    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_DISABLE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_tpa_set - Set tpa parameters.
 * @vpath_handle: Virtual Path ahandle.
 * @params: vxge_hal_vpath_tpa_params {} structure with parameters
 *
 * The function	sets the tpa parametrs for the vpath.
 *
 * See also: vxge_hal_vpath_tpa_params {}
 */
vxge_hal_status_e
vxge_hal_vpath_tpa_set(vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_tpa_params *params)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert((vpath_handle != NULL) && (params != NULL));

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", params = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) params);

	if (vpath->fifoh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->tpa_cfg);

	if (params->tpa_ignore_frame_error != VXGE_HAL_DEFAULT_32) {
		if (params->tpa_ignore_frame_error)
			val64 |= VXGE_HAL_TPA_CFG_IGNORE_FRAME_ERR;
		else
			val64 &= ~VXGE_HAL_TPA_CFG_IGNORE_FRAME_ERR;
	}

	if (params->tpa_ipv6_keep_searching != VXGE_HAL_DEFAULT_32) {
		if (params->tpa_ipv6_keep_searching)
			val64 &= ~VXGE_HAL_TPA_CFG_IPV6_STOP_SEARCHING;
		else
			val64 |= VXGE_HAL_TPA_CFG_IPV6_STOP_SEARCHING;
	}

	if (params->tpa_l4_pshdr_present != VXGE_HAL_DEFAULT_32) {
		if (params->tpa_l4_pshdr_present)
			val64 |= VXGE_HAL_TPA_CFG_L4_PSHDR_PRESENT;
		else
			val64 &= ~VXGE_HAL_TPA_CFG_L4_PSHDR_PRESENT;
	}

	if (params->tpa_support_mobile_ipv6_hdrs != VXGE_HAL_DEFAULT_32) {
		if (params->tpa_support_mobile_ipv6_hdrs)
			val64 |= VXGE_HAL_TPA_CFG_SUPPORT_MOBILE_IPV6_HDRS;
		else
			val64 &= ~VXGE_HAL_TPA_CFG_SUPPORT_MOBILE_IPV6_HDRS;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->tpa_cfg);

	vpath->vp_config->tpa_ignore_frame_error =
	    params->tpa_ignore_frame_error;
	vpath->vp_config->tpa_l4_pshdr_present =
	    params->tpa_l4_pshdr_present;
	vpath->vp_config->tpa_support_mobile_ipv6_hdrs =
	    params->tpa_support_mobile_ipv6_hdrs;

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->tx_protocol_assist_cfg);

	if (params->tpa_lsov2_en != VXGE_HAL_DEFAULT_32) {
		if (params->tpa_lsov2_en)
			val64 |= VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_LSOV2_EN;
		else
			val64 &= ~VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_LSOV2_EN;
	}

	if (params->tpa_ipv6_keep_searching != VXGE_HAL_DEFAULT_32) {
		if (params->tpa_ipv6_keep_searching)
			val64 |=
			    VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_IPV6_KEEP_SEARCHING;
		else
			val64 &=
			    ~VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_IPV6_KEEP_SEARCHING;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->tx_protocol_assist_cfg);

	vpath->vp_config->tpa_lsov2_en = params->tpa_lsov2_en;
	vpath->vp_config->tpa_ipv6_keep_searching =
	    params->tpa_ipv6_keep_searching;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_rpa_set - Set rpa parameters.
 * @vpath_handle: Virtual Path ahandle.
 * @params: vxge_hal_vpath_rpa_params {} structure with parameters
 *
 * The function	sets the rpa parametrs for the vpath.
 *
 * See also: vxge_hal_vpath_rpa_params {}
 */
vxge_hal_status_e
vxge_hal_vpath_rpa_set(vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_rpa_params *params)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert((vpath_handle != NULL) && (params != NULL));

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", params = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) params);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->xmac_rpa_vcfg);

	if (params->rpa_ipv4_tcp_incl_ph != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_ipv4_tcp_incl_ph)
			val64 |= VXGE_HAL_XMAC_RPA_VCFG_IPV4_TCP_INCL_PH;
		else
			val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_IPV4_TCP_INCL_PH;
	}

	if (params->rpa_ipv6_tcp_incl_ph != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_ipv6_tcp_incl_ph)
			val64 |= VXGE_HAL_XMAC_RPA_VCFG_IPV6_TCP_INCL_PH;
		else
			val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_IPV6_TCP_INCL_PH;
	}

	if (params->rpa_ipv4_udp_incl_ph != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_ipv4_udp_incl_ph)
			val64 |= VXGE_HAL_XMAC_RPA_VCFG_IPV4_UDP_INCL_PH;
		else
			val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_IPV4_UDP_INCL_PH;
	}

	if (params->rpa_ipv6_udp_incl_ph != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_ipv6_udp_incl_ph)
			val64 |= VXGE_HAL_XMAC_RPA_VCFG_IPV6_UDP_INCL_PH;
		else
			val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_IPV6_UDP_INCL_PH;
	}

	if (params->rpa_l4_incl_cf != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_l4_incl_cf)
			val64 |= VXGE_HAL_XMAC_RPA_VCFG_L4_INCL_CF;
		else
			val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_L4_INCL_CF;
	}

	if (params->rpa_strip_vlan_tag != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_strip_vlan_tag)
			val64 |= VXGE_HAL_XMAC_RPA_VCFG_STRIP_VLAN_TAG;
		else
			val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_STRIP_VLAN_TAG;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->xmac_rpa_vcfg);

	vpath->vp_config->rpa_ipv4_tcp_incl_ph = params->rpa_ipv4_tcp_incl_ph;
	vpath->vp_config->rpa_ipv6_tcp_incl_ph = params->rpa_ipv6_tcp_incl_ph;
	vpath->vp_config->rpa_ipv4_udp_incl_ph = params->rpa_ipv4_udp_incl_ph;
	vpath->vp_config->rpa_ipv6_udp_incl_ph = params->rpa_ipv6_udp_incl_ph;
	vpath->vp_config->rpa_l4_incl_cf = params->rpa_l4_incl_cf;
	vpath->vp_config->rpa_strip_vlan_tag = params->rpa_strip_vlan_tag;

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxmac_vcfg0);

	if (params->rpa_ucast_all_addr_en != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_ucast_all_addr_en)
			val64 |= VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN;
		else
			val64 &= ~VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN;
	}

	if (params->rpa_mcast_all_addr_en != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_mcast_all_addr_en)
			val64 |= VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN;
		else
			val64 &= ~VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN;
	}

	if (params->rpa_bcast_en != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_bcast_en)
			val64 |= VXGE_HAL_RXMAC_VCFG0_BCAST_EN;
		else
			val64 &= ~VXGE_HAL_RXMAC_VCFG0_BCAST_EN;
	}

	if (params->rpa_all_vid_en != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_all_vid_en)
			val64 |= VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;
		else
			val64 &= ~VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rxmac_vcfg0);

	vpath->vp_config->rpa_ucast_all_addr_en = params->rpa_ucast_all_addr_en;
	vpath->vp_config->rpa_mcast_all_addr_en = params->rpa_mcast_all_addr_en;
	vpath->vp_config->rpa_bcast_en = params->rpa_bcast_en;
	vpath->vp_config->rpa_all_vid_en = params->rpa_all_vid_en;

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->fau_rpa_vcfg);

	if (params->rpa_l4_comp_csum != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_l4_comp_csum)
			val64 |= VXGE_HAL_FAU_RPA_VCFG_L4_COMP_CSUM;
		else
			val64 &= ~VXGE_HAL_FAU_RPA_VCFG_L4_COMP_CSUM;
	}

	if (params->rpa_l3_incl_cf != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_l3_incl_cf)
			val64 |= VXGE_HAL_FAU_RPA_VCFG_L3_INCL_CF;
		else
			val64 &= ~VXGE_HAL_FAU_RPA_VCFG_L3_INCL_CF;
	}

	if (params->rpa_l3_comp_csum != VXGE_HAL_DEFAULT_32) {
		if (params->rpa_l3_comp_csum)
			val64 |= VXGE_HAL_FAU_RPA_VCFG_L3_COMP_CSUM;
		else
			val64 &= ~VXGE_HAL_FAU_RPA_VCFG_L3_COMP_CSUM;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->fau_rpa_vcfg);

	vpath->vp_config->rpa_l4_comp_csum = params->rpa_l4_comp_csum;
	vpath->vp_config->rpa_l3_incl_cf = params->rpa_l3_incl_cf;
	vpath->vp_config->rpa_l3_comp_csum = params->rpa_l3_comp_csum;

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_intr_enable - Enable vpath interrupts.
 * @vpath: Virtual Path.
 * @op: One of the vxge_hal_vpath_intr_e enumerated values specifying
 *	  the type(s) of interrupts to enable.
 *
 * Enable vpath interrupts. The function is to be executed the last in
 * vpath initialization sequence.
 *
 * See also: __hal_vpath_intr_disable()
 */
vxge_hal_status_e
__hal_vpath_intr_enable(__hal_virtualpath_t *vpath)
{
	u64 val64;
	__hal_device_t *hldev;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->kdfcctl_errors_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->general_errors_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->pci_config_errors_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->mrpcim_to_vpath_alarm_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->srpcim_to_vpath_alarm_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->vpath_ppif_int_status);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->srpcim_msg_to_vpath_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->vpath_pcipif_int_status);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->prc_alarm_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->wrdma_alarm_status);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->asic_ntwk_vp_err_reg);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->xgmac_vp_int_status);

	vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_general_int_status);

	/* Unmask the individual interrupts. */
	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vpath->vp_reg->kdfcctl_errors_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vpath->vp_reg->general_errors_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->pci_config_errors_mask);

	if (hldev->first_vp_id != vpath->vp_id) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) VXGE_HAL_INTR_MASK_ALL,
		    &vpath->vp_reg->mrpcim_to_vpath_alarm_mask);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) VXGE_HAL_INTR_MASK_ALL,
		    &vpath->vp_reg->srpcim_to_vpath_alarm_mask);
	} else {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    0,
		    &vpath->vp_reg->mrpcim_to_vpath_alarm_mask);

		if (hldev->access_rights &
		    VXGE_HAL_DEVICE_ACCESS_RIGHT_SRPCIM) {
			vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
			    hldev->header.regh0,
			    0,
			    &vpath->vp_reg->srpcim_to_vpath_alarm_mask);
		}
	}

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vpath->vp_reg->vpath_ppif_int_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->srpcim_msg_to_vpath_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vpath->vp_reg->vpath_pcipif_int_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(VXGE_HAL_PRC_ALARM_REG_PRC_RING_BUMP, 0),
	    &vpath->vp_reg->prc_alarm_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vpath->vp_reg->wrdma_alarm_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vpath->vp_reg->xgmac_vp_int_mask);

	if (hldev->first_vp_id != vpath->vp_id) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) VXGE_HAL_INTR_MASK_ALL,
		    &vpath->vp_reg->asic_ntwk_vp_err_mask);
	} else {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32((
		    VXGE_HAL_ASIC_NTWK_VP_ERR_REG_REAF_FAULT |
		    VXGE_HAL_ASIC_NTWK_VP_ERR_REG_REAF_OK), 0),
		    &vpath->vp_reg->asic_ntwk_vp_err_mask);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->tim_int_en);

	/* val64 |= VXGE_HAL_TIM_SET_INT_EN_VP(1 << (16 - vpath->vp_id)); */

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->tim_set_int_en);

	vxge_hal_pio_mem_write32_upper(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vpath->vp_reg->vpath_general_int_mask);

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);

}

/*
 * vxge_hal_vpath_intr_enable - Enable vpath interrupts.
 * @vpath_handle: Virtual Path handle.
 * @op: One of the vxge_hal_vpath_intr_e enumerated values specifying
 *	  the type(s) of interrupts to enable.
 *
 * Enable vpath interrupts. The function is to be executed the last in
 * vpath initialization sequence.
 *
 * See also: vxge_hal_vpath_intr_disable()
 */
vxge_hal_status_e
vxge_hal_vpath_intr_enable(vxge_hal_vpath_h vpath_handle)
{
	vxge_hal_status_e status;

	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp;

	vxge_assert(vpath_handle != NULL);

	vp = (__hal_vpath_handle_t *) vpath_handle;
	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	status = __hal_vpath_intr_enable((__hal_virtualpath_t *) vp->vpath);

	vxge_hal_vpath_unmask_all(vpath_handle);

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);

}

/*
 * __hal_vpath_intr_disable - Disable vpath interrupts.
 * @vpath: Virtual Path.
 * @op: One of the vxge_hal_vpath_intr_e enumerated values specifying
 *	  the type(s) of interrupts to enable.
 *
 * Disable vpath interrupts. The function is to be executed the last in
 * vpath initialization sequence.
 *
 * See also: __hal_vpath_intr_enable()
 */
vxge_hal_status_e
__hal_vpath_intr_disable(__hal_virtualpath_t *vpath)
{
	u64 val64;
	__hal_device_t *hldev;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);

		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->vpath_general_int_mask);

	val64 = VXGE_HAL_TIM_CLR_INT_EN_VP(1 << (16 - vpath->vp_id));

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->tim_clr_int_en);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->kdfcctl_errors_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->general_errors_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->pci_config_errors_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->mrpcim_to_vpath_alarm_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->srpcim_to_vpath_alarm_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->vpath_ppif_int_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->srpcim_msg_to_vpath_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->vpath_pcipif_int_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->prc_alarm_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->wrdma_alarm_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->asic_ntwk_vp_err_mask);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) VXGE_HAL_INTR_MASK_ALL,
	    &vpath->vp_reg->xgmac_vp_int_mask);

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);

}
/*
 * vxge_hal_vpath_intr_disable - Disable vpath interrupts.
 * @vpath_handle: Virtual Path handle.
 * @op: One of the vxge_hal_vpath_intr_e enumerated values specifying
 *	  the type(s) of interrupts to disable.
 *
 * Disable vpath interrupts.
 *
 * See also: vxge_hal_vpath_intr_enable()
 */
vxge_hal_status_e
vxge_hal_vpath_intr_disable(vxge_hal_vpath_h vpath_handle)
{
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;
	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	vxge_hal_vpath_mask_all(vpath_handle);

	(void) __hal_vpath_intr_disable(vpath);

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_mask_all - Mask all vpath interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Mask	all vpath interrupts.
 *
 * See also: vxge_hal_vpath_unmask_all()
 */
void
vxge_hal_vpath_mask_all(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;

	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp;

	vxge_assert(vpath_handle != NULL);

	vp = (__hal_vpath_handle_t *) vpath_handle;
	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	val64 = VXGE_HAL_VPATH_GENERAL_INT_MASK_PIC_INT |
	    VXGE_HAL_VPATH_GENERAL_INT_MASK_PCI_INT |
	    VXGE_HAL_VPATH_GENERAL_INT_MASK_WRDMA_INT |
	    VXGE_HAL_VPATH_GENERAL_INT_MASK_XMAC_INT;

	vxge_hal_pio_mem_write32_upper(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vp->vpath->vp_reg->vpath_general_int_mask);

	if (vp->vpath->vp_id < 16) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask0);

		val64 |= vBIT(0xf, (vp->vpath->vp_id * 4), 4);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_mask0);

	} else {

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(0xf), 0),
		    &hldev->common_reg->tim_int_mask1);

	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_unmask_all - Unmask all vpath interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Unmask all vpath interrupts.
 *
 * See also: vxge_hal_vpath_mask_all()
 */
void
vxge_hal_vpath_unmask_all(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vp == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return;
	}

	vxge_hal_pio_mem_write32_upper(
	    hldev->header.pdev,
	    hldev->header.regh0,
	    0,
	    &vp->vpath->vp_reg->vpath_general_int_mask);

	if (vp->vpath->vp_id < 16) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask0);

		val64 &= ~(vBIT(0xf, (vp->vpath->vp_id * 4), 4));

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_mask0);

	} else {

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    0,
		    &hldev->common_reg->tim_int_mask1);

	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * __hal_vpath_link_state_test - Test for the link state.
 * @vpath: Virtual Path.
 *
 * Test link state.
 * Returns: link state.
 */
vxge_hal_device_link_state_e
__hal_vpath_link_state_test(__hal_virtualpath_t *vpath)
{
	__hal_device_t *hldev;

	vxge_assert(vpath != NULL);
	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_ASIC_NTWK_VP_CTRL_REQ_TEST_NTWK,
	    &vpath->vp_reg->asic_ntwk_vp_ctrl);

	(void) vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->asic_ntwk_vp_ctrl,
	    0,
	    VXGE_HAL_ASIC_NTWK_VP_CTRL_REQ_TEST_NTWK,
	    hldev->header.config.device_poll_millis);

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (hldev->header.link_state);
}

/*
 * __hal_vpath_link_state_poll - Poll for the link state.
 * @vpath: Virtual Path.
 *
 * Get link state.
 * Returns: link state.
 */
vxge_hal_device_link_state_e
__hal_vpath_link_state_poll(__hal_virtualpath_t *vpath)
{
	u64 val64;
	__hal_device_t *hldev;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	if (vpath == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_LINK_NONE);
		return (VXGE_HAL_LINK_NONE);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vpmgmt_reg->xgmac_gen_status_vpmgmt_clone);

	if (val64 & VXGE_HAL_XGMAC_GEN_STATUS_VPMGMT_CLONE_XMACJ_NTWK_OK) {

		(void) __hal_device_handle_link_up_ind(vpath->hldev);

		if (val64 &
		    VXGE_HAL_XGMAC_GEN_STATUS_VPMGMT_CLONE_XMACJ_NTWK_DATA_RATE) {
			VXGE_HAL_DEVICE_DATA_RATE_SET(vpath->hldev,
			    VXGE_HAL_DATA_RATE_10G);

		} else {
			VXGE_HAL_DEVICE_DATA_RATE_SET(vpath->hldev,
			    VXGE_HAL_DATA_RATE_1G);

		}
	} else {
		(void) __hal_device_handle_link_down_ind(vpath->hldev);
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (vpath->hldev->header.link_state);
}

/*
 * __hal_vpath_data_rate_poll - Poll for the data rate.
 * @vpath: Virtual Path.
 *
 * Get data rate.
 * Returns: data rate.
 */
vxge_hal_device_data_rate_e
__hal_vpath_data_rate_poll(
    __hal_virtualpath_t *vpath)
{
	u64 val64;
	__hal_device_t *hldev;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	if (vpath == NULL) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_DATA_RATE_UNKNOWN);
		return (VXGE_HAL_DATA_RATE_UNKNOWN);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vpmgmt_reg->xgmac_gen_status_vpmgmt_clone);

	if (val64 &
	    VXGE_HAL_XGMAC_GEN_STATUS_VPMGMT_CLONE_XMACJ_NTWK_DATA_RATE) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);

		return (VXGE_HAL_DATA_RATE_10G);

	} else {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
		    __FILE__, __func__, __LINE__);

		return (VXGE_HAL_DATA_RATE_1G);

	}
}

/*
 * __hal_vpath_alarm_process - Process Alarms.
 * @vpath: Virtual Path.
 * @skip_alarms: Do not clear the alarms
 *
 * Process vpath alarms.
 *
 */
vxge_hal_status_e
__hal_vpath_alarm_process(__hal_virtualpath_t *vpath, u32 skip_alarms)
{
	u64 val64;
	u64 alarm_status;
	u64 pic_status = 0;
	u64 pif_status;
	u64 wrdma_status;
	u64 xgmac_status;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	alarm_status = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_general_int_status);

	vxge_hal_info_log_vpath_irq(
	    "alarm_status = 0x"VXGE_OS_STXFMT, (ptr_t) alarm_status);

	if (vxge_os_unlikely(!alarm_status)) {
		status = VXGE_HAL_ERR_WRONG_IRQ;
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (alarm_status & VXGE_HAL_VPATH_GENERAL_INT_STATUS_PIC_INT) {

		pic_status = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->vpath_ppif_int_status);

		vxge_hal_info_log_vpath_irq(
		    "pic_status = 0x"VXGE_OS_STXFMT, (ptr_t) pic_status);

		if (pic_status &
		    VXGE_HAL_VPATH_PPIF_INT_STATUS_GENERAL_ERRORS_INT) {
			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->general_errors_reg);

			vxge_hal_info_log_vpath_irq(
			    "general_errors_reg = 0x"VXGE_OS_STXFMT,
			    (ptr_t) val64);
			if (val64 & VXGE_HAL_GENERAL_ERRORS_REG_INI_SERR_DET) {

				vpath->sw_stats->error_stats.ini_serr_det++;
				vxge_hal_info_log_vpath_irq("%s:"
				    "VXGE_HAL_GENERAL_ERRORS_REG_INI_SERR_DET",
				    __func__);

				__hal_device_handle_error(hldev, vpath->vp_id,
				    VXGE_HAL_EVENT_SERR);

				if (!skip_alarms) {
					vxge_os_pio_mem_write64(hldev->header.pdev,
					    hldev->header.regh0,
					    VXGE_HAL_GENERAL_ERRORS_REG_INI_SERR_DET,
					    &vpath->vp_reg->general_errors_reg);
				}

				vxge_hal_trace_log_vpath_irq("<== %s:%s:%d \
				    Result = 0", __FILE__, __func__, __LINE__);

				return (VXGE_HAL_ERR_EVENT_SERR);
			}
		}

		if (pic_status & VXGE_HAL_VPATH_PPIF_INT_STATUS_PCI_CONFIG_ERRORS_INT) {
			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->pci_config_errors_reg);

			vxge_hal_info_log_vpath_irq(
			    "pci_config_errors_reg = 0x"VXGE_OS_STXFMT,
			    (ptr_t) val64);

			if (val64 &
			    VXGE_HAL_PCI_CONFIG_ERRORS_REG_STATUS_ERR) {
				vpath->sw_stats->error_stats.pci_config_status_err++;
				vxge_hal_info_log_vpath_irq("%s: \
				    VXGE_HAL_PCI_CONFIG_ERRORS_REG_STATUS_ERR",
				    __func__);
			}

			if (val64 & VXGE_HAL_PCI_CONFIG_ERRORS_REG_UNCOR_ERR) {
				vpath->sw_stats->error_stats.pci_config_uncor_err++;
				vxge_hal_info_log_vpath_irq("%s: \
				    VXGE_HAL_PCI_CONFIG_ERRORS_REG_UNCOR_ERR",
				    __func__);
			}

			if (val64 & VXGE_HAL_PCI_CONFIG_ERRORS_REG_COR_ERR) {
				vpath->sw_stats->error_stats.pci_config_cor_err++;
				vxge_hal_info_log_vpath_irq("%s: \
				    VXGE_HAL_PCI_CONFIG_ERRORS_REG_COR_ERR",
				    __func__);
			}

			if (!skip_alarms)
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0, VXGE_HAL_INTR_MASK_ALL,
				    &vpath->vp_reg->pci_config_errors_reg);
		}

		if (pic_status &
		    VXGE_HAL_VPATH_PPIF_INT_STATUS_MRPCIM_TO_VPATH_ALARM_INT) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->mrpcim_to_vpath_alarm_reg);

			vxge_hal_info_log_vpath_irq(
			    "mrpcim_to_vpath_alarm_reg = 0x"VXGE_OS_STXFMT,
			    (ptr_t) val64);

			if (val64 &
			    VXGE_HAL_MRPCIM_TO_VPATH_ALARM_REG_ALARM) {

				vpath->sw_stats->error_stats.mrpcim_to_vpath_alarms++;
				hldev->stats.sw_dev_err_stats.mrpcim_alarms++;
				vxge_hal_info_log_vpath_irq(
				    "%s:VXGE_HAL_MRPCIM_TO_VPATH_ALARM_REG_ALARM",
				    __func__);

				__hal_device_handle_error(hldev, vpath->vp_id,
				    VXGE_HAL_EVENT_MRPCIM_CRITICAL);

				if (!skip_alarms)
					vxge_os_pio_mem_write64(hldev->header.pdev,
					    hldev->header.regh0,
					    VXGE_HAL_MRPCIM_TO_VPATH_ALARM_REG_ALARM,
					    &vpath->vp_reg->mrpcim_to_vpath_alarm_reg);
					return (VXGE_HAL_ERR_EVENT_MRPCIM_CRITICAL);
			}

			if (!skip_alarms)
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_INTR_MASK_ALL,
				    &vpath->vp_reg->mrpcim_to_vpath_alarm_reg);
		}

		if (pic_status &
		    VXGE_HAL_VPATH_PPIF_INT_STATUS_SRPCIM_TO_VPATH_ALARM_INT) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->srpcim_to_vpath_alarm_reg);

			vxge_hal_info_log_vpath_irq(
			    "srpcim_to_vpath_alarm_reg = 0x"VXGE_OS_STXFMT,
			    (ptr_t) val64);

			vpath->sw_stats->error_stats.srpcim_to_vpath_alarms++;
			hldev->stats.sw_dev_err_stats.srpcim_alarms++;

			vxge_hal_info_log_vpath_irq(
			    "%s:VXGE_HAL_SRPCIM_TO_VPATH_ALARM_REG_GET_ALARM",
			    __func__);

			status = vxge_hal_srpcim_alarm_process(
			    (vxge_hal_device_h) hldev, skip_alarms);

			if (!skip_alarms)
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_INTR_MASK_ALL,
				    &vpath->vp_reg->srpcim_to_vpath_alarm_reg);

			if (status == VXGE_HAL_ERR_EVENT_SRPCIM_CRITICAL)
				return (status);
		}
	}

	if (alarm_status & VXGE_HAL_VPATH_GENERAL_INT_STATUS_WRDMA_INT) {

		wrdma_status = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->wrdma_alarm_status);

		vxge_hal_info_log_vpath_irq(
		    "wrdma_alarm_status = 0x"VXGE_OS_STXFMT,
		    (ptr_t) wrdma_status);

		if (wrdma_status &
		    VXGE_HAL_WRDMA_ALARM_STATUS_PRC_ALARM_PRC_INT) {
			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->prc_alarm_reg);

			vxge_hal_info_log_vpath_irq(
			    "prc_alarm_reg = 0x"VXGE_OS_STXFMT, (ptr_t) val64);

			if (val64 & VXGE_HAL_PRC_ALARM_REG_PRC_RING_BUMP) {
				vpath->sw_stats->error_stats.prc_ring_bumps++;
				vxge_hal_info_log_vpath_irq(
				    "%s:VXGE_HAL_PRC_ALARM_REG_PRC_RING_BUMP",
				    __func__);
			}

			if (val64 & VXGE_HAL_PRC_ALARM_REG_PRC_RXDCM_SC_ERR) {
				vpath->sw_stats->error_stats.prc_rxdcm_sc_err++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_PRC_ALARM_REG_PRC_RXDCM_SC_ERR",
				    __func__);
				__hal_device_handle_error(hldev,
				    vpath->vp_id,
				    VXGE_HAL_EVENT_CRITICAL);

				if (!skip_alarms) {
					vxge_os_pio_mem_write64(hldev->header.pdev,
					    hldev->header.regh0,
					    VXGE_HAL_PRC_ALARM_REG_PRC_RXDCM_SC_ERR,
					    &vpath->vp_reg->prc_alarm_reg);
				}

				vxge_hal_trace_log_vpath_irq(
				    "<== %s:%s:%d Result = %d",
				    __FILE__, __func__, __LINE__,
				    VXGE_HAL_ERR_EVENT_CRITICAL);

				return (VXGE_HAL_ERR_EVENT_CRITICAL);
			}

			if (val64 & VXGE_HAL_PRC_ALARM_REG_PRC_RXDCM_SC_ABORT) {
				vpath->sw_stats->error_stats.prc_rxdcm_sc_abort++;
				vxge_hal_info_log_vpath_irq("%s: \
				    VXGE_HAL_PRC_ALARM_REG_PRC_RXDCM_SC_ABORT",
				    __func__);

				__hal_device_handle_error(hldev, vpath->vp_id,
				    VXGE_HAL_EVENT_CRITICAL);

				if (!skip_alarms)
					vxge_os_pio_mem_write64(hldev->header.pdev,
					    hldev->header.regh0,
					    VXGE_HAL_PRC_ALARM_REG_PRC_RXDCM_SC_ABORT,
					    &vpath->vp_reg->prc_alarm_reg);

				vxge_hal_trace_log_vpath_irq(
				    "<== %s:%s:%d Result = %d",
				    __FILE__, __func__, __LINE__,
				    VXGE_HAL_ERR_EVENT_CRITICAL);

				return (VXGE_HAL_ERR_EVENT_CRITICAL);
			}

			if (val64 & VXGE_HAL_PRC_ALARM_REG_PRC_QUANTA_SIZE_ERR) {
				vpath->sw_stats->error_stats.prc_quanta_size_err++;
				vxge_hal_info_log_vpath_irq("%s: \
				    VXGE_HAL_PRC_ALARM_REG_PRC_QUANTA_SIZE_ERR",
				    __func__);
			}

			if (!skip_alarms)
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0, VXGE_HAL_INTR_MASK_ALL,
				    &vpath->vp_reg->prc_alarm_reg);
			}
	}

	if (alarm_status & VXGE_HAL_VPATH_GENERAL_INT_STATUS_PIC_INT) {

		if (pic_status &
		    VXGE_HAL_VPATH_PPIF_INT_STATUS_GENERAL_ERRORS_INT) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->general_errors_reg);

			vxge_hal_info_log_vpath_irq(
			    "general_errors_reg = 0x"VXGE_OS_STXFMT,
			    (ptr_t) val64);

			if (val64 &
			    VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO0_OVRFLOW) {

				vpath->sw_stats->error_stats.dblgen_fifo0_overflow++;
				vxge_hal_info_log_vpath_irq(
				    "%s:"
				    "VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO0_OVRFLOW",
				    __func__);

				__hal_device_handle_error(hldev, vpath->vp_id,
				    VXGE_HAL_EVENT_KDFCCTL);

				if (!skip_alarms) {
					vxge_os_pio_mem_write64(hldev->header.pdev,
					    hldev->header.regh0,
					    VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO0_OVRFLOW,
					    &vpath->vp_reg->general_errors_reg);
				}

				vxge_hal_trace_log_vpath_irq("<== %s:%s:%d \
				    Result = %d",
				    __FILE__, __func__, __LINE__,
				    VXGE_HAL_ERR_EVENT_KDFCCTL);
				return (VXGE_HAL_ERR_EVENT_KDFCCTL);
			}

			if (val64 & VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO1_OVRFLOW) {
				vpath->sw_stats->error_stats.dblgen_fifo1_overflow++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO1_OVRFLOW",
				    __func__);

			}

			if (val64 &
			    VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO2_OVRFLOW) {
				vpath->sw_stats->error_stats.dblgen_fifo2_overflow++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_GENERAL_ERRORS_REG_DBLGEN_FIFO2_OVRFLOW",
				    __func__);
			}

			if (val64 &
			    VXGE_HAL_GENERAL_ERRORS_REG_STATSB_PIF_CHAIN_ERR) {
				vpath->sw_stats->error_stats.statsb_pif_chain_error++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_GENERAL_ERRORS_REG_STATSB_PIF_CHAIN_ERR",
				    __func__);
			}

			if (val64 &
			    VXGE_HAL_GENERAL_ERRORS_REG_STATSB_DROP_TIMEOUT) {
				vpath->sw_stats->error_stats.statsb_drop_timeout++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_GENERAL_ERRORS_REG_STATSB_DROP_TIMEOUT",
				    __func__);
			}

			if (val64 & VXGE_HAL_GENERAL_ERRORS_REG_TGT_ILLEGAL_ACCESS) {
				vpath->sw_stats->error_stats.target_illegal_access++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_GENERAL_ERRORS_REG_TGT_ILLEGAL_ACCESS",
				    __func__);
			}

			if (!skip_alarms)
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_INTR_MASK_ALL,
				    &vpath->vp_reg->general_errors_reg);
		}

		if (pic_status &
		    VXGE_HAL_VPATH_PPIF_INT_STATUS_KDFCCTL_ERRORS_INT) {
			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->kdfcctl_errors_reg);

			vxge_hal_info_log_vpath_irq(
			    "kdfcctl_errors_reg = 0x"VXGE_OS_STXFMT,
			    (ptr_t) val64);

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_OVRWR) {
				vpath->sw_stats->error_stats.kdfcctl_fifo0_overwrite++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_OVRWR",
				    __func__);
				__hal_device_handle_error(hldev, vpath->vp_id,
				    VXGE_HAL_EVENT_KDFCCTL);

				if (!skip_alarms) {
					vxge_os_pio_mem_write64(hldev->header.pdev,
					    hldev->header.regh0,
					    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_OVRWR,
					    &vpath->vp_reg->kdfcctl_errors_reg);
				}

				vxge_hal_trace_log_vpath_irq("<== %s:%s:%d \
				    Result = %d", __FILE__, __func__, __LINE__,
				    VXGE_HAL_ERR_EVENT_KDFCCTL);
				return (VXGE_HAL_ERR_EVENT_KDFCCTL);
			}

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_OVRWR) {
				vpath->sw_stats->error_stats.kdfcctl_fifo1_overwrite++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_OVRWR",
				    __func__);

			}

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_OVRWR) {
				vpath->sw_stats->error_stats.kdfcctl_fifo2_overwrite++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_OVRWR",
				    __func__);
			}

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_POISON) {
				vpath->sw_stats->error_stats.kdfcctl_fifo0_poison++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_POISON",
				    __func__);
				__hal_device_handle_error(hldev, vpath->vp_id,
				    VXGE_HAL_EVENT_KDFCCTL);

				if (!skip_alarms) {
					vxge_os_pio_mem_write64(hldev->header.pdev,
					    hldev->header.regh0,
					    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_POISON,
					    &vpath->vp_reg->kdfcctl_errors_reg);
				}

				vxge_hal_trace_log_vpath_irq("<== %s:%s:%d \
				    Result = %d", __FILE__, __func__, __LINE__,
				    VXGE_HAL_ERR_EVENT_KDFCCTL);
				return (VXGE_HAL_ERR_EVENT_KDFCCTL);
			}

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_POISON) {
				vpath->sw_stats->error_stats.kdfcctl_fifo1_poison++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_POISON",
				    __func__);

			}

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_POISON) {
				vpath->sw_stats->error_stats.kdfcctl_fifo2_poison++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_POISON",
				    __func__);
			}

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_DMA_ERR) {
				vpath->sw_stats->error_stats.kdfcctl_fifo0_dma_error++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_DMA_ERR",
				    __func__);

				__hal_device_handle_error(hldev, vpath->vp_id,
				    VXGE_HAL_EVENT_KDFCCTL);

				if (!skip_alarms) {
					vxge_os_pio_mem_write64(hldev->header.pdev,
					    hldev->header.regh0,
					    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO0_DMA_ERR,
					    &vpath->vp_reg->kdfcctl_errors_reg);
				}

				vxge_hal_trace_log_vpath_irq("<== %s:%s:%d \
				    Result = %d", __FILE__, __func__, __LINE__,
				    VXGE_HAL_ERR_EVENT_KDFCCTL);
				return (VXGE_HAL_ERR_EVENT_KDFCCTL);
			}

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_DMA_ERR) {
				vpath->sw_stats->error_stats.kdfcctl_fifo1_dma_error++;
				vxge_hal_info_log_vpath_irq("%s:"
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO1_DMA_ERR",
				    __func__);
			}

			if (val64 &
			    VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_DMA_ERR) {
				vpath->sw_stats->error_stats.kdfcctl_fifo2_dma_error++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_KDFCCTL_ERRORS_REG_KDFCCTL_FIFO2_DMA_ERR",
				    __func__);
			}

			if (!skip_alarms) {
				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_INTR_MASK_ALL,
				    &vpath->vp_reg->kdfcctl_errors_reg);
			}
		}
	}

	if (alarm_status & VXGE_HAL_VPATH_GENERAL_INT_STATUS_PCI_INT) {

		pif_status = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->vpath_pcipif_int_status);

		vxge_hal_info_log_vpath_irq(
		    "vpath_pcipif_int_status = 0x"VXGE_OS_STXFMT,
		    (ptr_t) pif_status);

		if (pif_status &
		    VXGE_HAL_VPATH_PCIPIF_INT_STATUS_SRPCIM_MSG_TO_VPATH_INT) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->srpcim_msg_to_vpath_reg);

			vxge_hal_info_log_vpath_irq(
			    "srpcim_msg_to_vpath_reg = 0x"VXGE_OS_STXFMT,
			    (ptr_t) val64);

			if (val64 &
			    VXGE_HAL_SRPCIM_MSG_TO_VPATH_REG_INT) {

				val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
				    hldev->header.regh0,
				    &vpath->vpmgmt_reg->srpcim_to_vpath_wmsg);

				__hal_ifmsg_wmsg_process(vpath, val64);

				vpath->sw_stats->error_stats.srpcim_msg_to_vpath++;

				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    0,
				    &vpath->vpmgmt_reg->srpcim_to_vpath_wmsg);

				vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
				    hldev->header.regh0,
				    (u32) VXGE_HAL_INTR_MASK_ALL,
				    &vpath->vp_reg->srpcim_msg_to_vpath_mask);

				vxge_hal_info_log_vpath_irq("%s:"
				    "VXGE_HAL_SRPCIM_MSG_TO_VPATH_REG_INT",
				    __func__);
			}

			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    VXGE_HAL_INTR_MASK_ALL,
			    &vpath->vp_reg->srpcim_msg_to_vpath_reg);
		}
	}

	if (alarm_status & VXGE_HAL_VPATH_GENERAL_INT_STATUS_XMAC_INT) {

		xgmac_status = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->xgmac_vp_int_status);

		vxge_hal_info_log_vpath_irq("xgmac_status = 0x"VXGE_OS_STXFMT,
		    (ptr_t) xgmac_status);

		if (xgmac_status &
		    VXGE_HAL_XGMAC_VP_INT_STATUS_ASIC_NTWK_VP_ERR_INT) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->asic_ntwk_vp_err_reg);

			vxge_hal_info_log_vpath_irq(
			    "asic_ntwk_vp_err_reg = 0x"VXGE_OS_STXFMT,
			    (ptr_t) val64);

			if (((val64 & VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_FAULT) &&
			    (!(val64 & VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_OK))) ||
			    ((val64 & VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_FAULT_OCCURRED) &&
			    (!(val64 & VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_OK_OCCURRED)))) {
				vpath->sw_stats->error_stats.network_sustained_fault++;
				vxge_hal_info_log_vpath_irq("%s:" \
				    "VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_FAULT",
				    __func__);
				vxge_os_pio_mem_write64(vpath->hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_FAULT,
				    &vpath->vp_reg->asic_ntwk_vp_err_mask);

				(void) __hal_device_handle_link_down_ind(hldev);
			}

			if (((val64 & VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_OK) &&
			    (!(val64 & VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_FAULT))) ||
			    ((val64 & VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_OK_OCCURRED) &&
			    (!(val64 & VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_FAULT_OCCURRED)))) {
				vpath->sw_stats->error_stats.network_sustained_ok++;
				vxge_hal_info_log_vpath_irq(
				    "%s:VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_OK",
				    __func__);

				vxge_os_pio_mem_write64(hldev->header.pdev,
				    hldev->header.regh0,
				    VXGE_HAL_ASIC_NTWK_VP_ERR_REG_SUS_OK,
				    &vpath->vp_reg->asic_ntwk_vp_err_mask);

				(void) __hal_device_handle_link_up_ind(hldev);
			}

			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    VXGE_HAL_INTR_MASK_ALL,
			    &vpath->vp_reg->asic_ntwk_vp_err_reg);
			return (VXGE_HAL_INF_LINK_UP_DOWN);
		}
	}

	if (alarm_status & ~(
	    VXGE_HAL_VPATH_GENERAL_INT_STATUS_PIC_INT |
	    VXGE_HAL_VPATH_GENERAL_INT_STATUS_PCI_INT |
	    VXGE_HAL_VPATH_GENERAL_INT_STATUS_WRDMA_INT |
	    VXGE_HAL_VPATH_GENERAL_INT_STATUS_XMAC_INT)) {

		vpath->sw_stats->error_stats.unknown_alarms++;
		vxge_hal_info_log_vpath_irq(
		    "%s:%s:%d Unknown Alarm", __FILE__, __func__, __LINE__);

		__hal_device_handle_error(hldev, vpath->vp_id,
		    VXGE_HAL_EVENT_UNKNOWN);
		status = VXGE_HAL_ERR_EVENT_UNKNOWN;

	} else {
		hldev->stats.sw_dev_err_stats.vpath_alarms++;
		status = VXGE_HAL_OK;
	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (status);
}

/*
 * vxge_hal_vpath_begin_irq - Begin IRQ processing.
 * @vpath_handle: Virtual Path handle.
 * @skip_alarms: Do not clear the alarms
 * @reason: "Reason" for the interrupt,	the value of vpath's
 *			general_int_status register.
 *
 * The function	performs two actions, It first checks whether (shared IRQ) the
 * interrupt was raised	by the device. Next, it	masks the device interrupts.
 *
 * Note:
 * vxge_hal_vpath_begin_irq() does not flush MMIO writes through the
 * bridge. Therefore, two back-to-back interrupts are potentially possible.
 * It is the responsibility	of the ULD to make sure	that only one
 * vxge_hal_vpath_continue_irq() runs at a time.
 *
 * Returns: 0, if the interrupt	is not "ours" (note that in this case the
 * vpath remain enabled).
 * Otherwise, vxge_hal_vpath_begin_irq() returns 64bit general adapter
 * status.
 * See also: vxge_hal_vpath_handle_irq()
 */
vxge_hal_status_e
vxge_hal_vpath_begin_irq(vxge_hal_vpath_h vpath_handle,
    u32 skip_alarms, u64 *reason)
{
	u64 val64;
	u64 adapter_status;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	vxge_hal_status_e ret_val = VXGE_HAL_OK;

	vxge_assert((vpath_handle != NULL) && (reason != NULL));

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", skip_alarms = %d, "
	    "reason = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle,
	    skip_alarms, (ptr_t) reason);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->titan_general_int_status);

	if (vxge_os_unlikely(!val64)) {
		/* not Titan interrupt	 */
		*reason = 0;
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_WRONG_IRQ);
		return (VXGE_HAL_ERR_WRONG_IRQ);
	}

	if (vxge_os_unlikely(val64 == VXGE_HAL_ALL_FOXES)) {

		adapter_status = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->adapter_status);

		if (adapter_status == VXGE_HAL_ALL_FOXES) {
			__hal_device_handle_error(hldev,
			    vpath->vp_id,
			    VXGE_HAL_EVENT_SLOT_FREEZE);

			*reason = 0;
			ret_val = VXGE_HAL_ERR_SLOT_FREEZE;
			goto exit;
		}
	}

	if (val64 &
	    VXGE_HAL_TITAN_GENERAL_INT_STATUS_VPATH_TRAFFIC_INT(
	    1 << (16 - vpath->vp_id))) {

		if (vpath->vp_id < 16) {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &hldev->common_reg->tim_int_mask0);

			*reason = bVAL4(val64, (vpath->vp_id * 4));
		} else {

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &hldev->common_reg->tim_int_mask1);

			*reason = bVAL4(val64, 0);
		}

		return (VXGE_HAL_OK);
	}

	*reason = VXGE_HAL_INTR_ALARM;

	if (vxge_os_unlikely(val64 &
	    VXGE_HAL_TITAN_GENERAL_INT_STATUS_MRPCIM_ALARM_INT)) {
		vxge_hal_info_log_vpath_irq(
		    "%s:VXGE_HAL_TITAN_GENERAL_INT_STATUS_MRPCIM_ALARM_INT",
		    __func__);
		ret_val = VXGE_HAL_ERR_CRITICAL;
		goto exit;
	}

	if (vxge_os_unlikely(val64 &
	    VXGE_HAL_TITAN_GENERAL_INT_STATUS_SRPCIM_ALARM_INT)) {
		vxge_hal_info_log_vpath_irq(
		    "%s:VXGE_HAL_TITAN_GENERAL_INT_STATUS_SRPCIM_ALARM_INT",
		    __func__);
		ret_val = VXGE_HAL_ERR_CRITICAL;
		goto exit;
	}

	if (vxge_os_unlikely(val64 &
	    VXGE_HAL_TITAN_GENERAL_INT_STATUS_VPATH_ALARM_INT)) {
		vxge_hal_info_log_vpath_irq(
		    "%s:VXGE_HAL_TITAN_GENERAL_INT_STATUS_VPATH_ALARM_INT",
		    __func__);
		ret_val = __hal_vpath_alarm_process(vpath, skip_alarms);
	}

exit:
	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (ret_val);

}

/*
 * vxge_hal_vpath_continue_irq - Continue handling IRQ:	process	all
 *				completed descriptors.
 * @vpath_handle: Virtual Path handle.
 *
 * Process completed descriptors and unmask the	vpath interrupts.
 *
 * The vxge_hal_vpath_continue_irq() calls upper-layer driver (ULD)
 * via supplied completion callback.
 *
 * Note	that the vxge_hal_vpath_continue_irq is	part of	the _fast_ path.
 * To optimize the processing, the function does _not_ check for
 * errors and alarms.
 *
 * Returns: VXGE_HAL_OK.
 *
 * See also: vxge_hal_vpath_handle_irq()
 * vxge_hal_ring_rxd_next_completed(),
 * vxge_hal_fifo_txdl_next_completed(), vxge_hal_ring_callback_f {},
 * vxge_hal_fifo_callback_f {}.
 */
vxge_hal_status_e
vxge_hal_vpath_continue_irq(vxge_hal_vpath_h vpath_handle)
{
	u32 got_rx = 1, got_tx = 1;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	u32 isr_polling_cnt;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	isr_polling_cnt = hldev->header.config.isr_polling_cnt;

	do {
		if (got_rx && (vp->vpath->ringh != NULL))
			(void) vxge_hal_vpath_poll_rx(vpath_handle, &got_rx);

		if (got_tx && (vp->vpath->fifoh != NULL))
			(void) vxge_hal_vpath_poll_tx(vpath_handle, &got_tx);

		if (!got_rx && !got_tx)
			break;

	} while (isr_polling_cnt--);

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_handle_irq - Handle vpath IRQ.
 * @vpath_handle: Virtual Path handle.
 * @skip_alarms: Do not clear the alarms
 *
 * Perform the complete	handling of the	line interrupt.	The function
 * performs two	calls.
 * First it uses vxge_hal_vpath_begin_irq() to check the reason for
 * the interrupt and mask the vpath interrupts.
 * Second, it calls vxge_hal_vpath_continue_irq() to process all
 * completed descriptors and re-enable the interrupts.
 *
 * Returns: VXGE_HAL_OK - success;
 * VXGE_HAL_ERR_WRONG_IRQ - (shared) IRQ produced by other device.
 *
 * See also: vxge_hal_vpath_begin_irq(), vxge_hal_vpath_continue_irq().
 */
vxge_hal_status_e
vxge_hal_vpath_handle_irq(vxge_hal_vpath_h vpath_handle, u32 skip_alarms)
{
	u64 reason;
	vxge_hal_status_e status;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	vxge_hal_vpath_mask_all(vpath_handle);

	status = vxge_hal_vpath_begin_irq(vpath_handle,
	    skip_alarms, &reason);

	if (status != VXGE_HAL_OK) {
		vxge_hal_vpath_unmask_all(vpath_handle);
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (reason & VXGE_HAL_INTR_ALARM) {
		if (skip_alarms) {
			/* ULD needs to unmask explicitely */
			vxge_hal_trace_log_vpath_irq(
			    "<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_ERR_CRITICAL);
			return (VXGE_HAL_ERR_CRITICAL);
		} else {
			vxge_hal_vpath_unmask_all(vpath_handle);
			vxge_hal_trace_log_vpath_irq(
			    "<== %s:%s:%d Result = %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}
	}

	if (reason & VXGE_HAL_INTR_RX)
		vxge_hal_vpath_clear_rx(vpath_handle);

	status = vxge_hal_vpath_continue_irq(vpath_handle);

	vxge_hal_vpath_clear_tx(vpath_handle);

	vxge_hal_vpath_unmask_all(vpath_handle);

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
	return (status);
}

/*
 * vxge_hal_vpath_mask_tx - Mask Tx interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Mask	Tx device interrupts.
 *
 * See also: vxge_hal_vpath_unmask_tx(), vxge_hal_vpath_mask_rx(),
 * vxge_hal_vpath_clear_tx().
 */
void
vxge_hal_vpath_mask_tx(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->fifoh == NULL) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_HANDLE);
		return;
	}

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;
	}

	if (vpath->vp_id < 16) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask0);

		val64 |= vBIT(VXGE_HAL_INTR_TX, (vpath->vp_id * 4), 4);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_mask0);

	} else {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask1);

		val64 |= VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(VXGE_HAL_INTR_TX);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->tim_int_mask1);
	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_clear_tx - Acknowledge (that is, clear) the
 * condition that has caused the TX interrupt.
 * @vpath_handle: Virtual Path handle.
 *
 * Acknowledge (that is, clear)	the condition that has caused
 * the Tx interrupt.
 * See also: vxge_hal_vpath_begin_irq(), vxge_hal_vpath_continue_irq(),
 * vxge_hal_vpath_clear_rx(), vxge_hal_vpath_mask_tx().
 */
void
vxge_hal_vpath_clear_tx(vxge_hal_vpath_h vpath_handle)
{
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->fifoh == NULL) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_HANDLE);
		return;
	}

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;
	}

	if (vpath->vp_id < 16) {

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    vBIT(VXGE_HAL_INTR_TX, (vpath->vp_id * 4), 4),
		    &hldev->common_reg->tim_int_status0);

	} else {

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    vBIT(VXGE_HAL_INTR_TX, 0, 4),
		    0),
		    &hldev->common_reg->tim_int_status1);

	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_unmask_tx - Unmask Tx	interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Unmask Tx vpath interrupts.
 *
 * See also: vxge_hal_vpath_mask_tx(), vxge_hal_vpath_clear_tx().
 */
void
vxge_hal_vpath_unmask_tx(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->fifoh == NULL) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_INVALID_HANDLE);
		return;
	}

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);
		return;
	}

	if (vpath->vp_id < 16) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask0);

		val64 &= ~vBIT(VXGE_HAL_INTR_TX, (vpath->vp_id * 4), 4);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_mask0);

	} else {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask1);

		val64 &=
		    ~VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(VXGE_HAL_INTR_TX);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->tim_int_mask1);

	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_mask_rx - Mask Rx	interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Mask	Rx vpath interrupts.
 *
 * See also: vxge_hal_vpath_unmask_rx(), vxge_hal_vpath_mask_tx(),
 * vxge_hal_vpath_clear_rx().
 */
void
vxge_hal_vpath_mask_rx(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return;
	}

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;
	}

	if (vpath->vp_id < 16) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask0);

		val64 |= vBIT(VXGE_HAL_INTR_RX, (vpath->vp_id * 4), 4);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_mask0);

	} else {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask1);

		val64 |= VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(VXGE_HAL_INTR_RX);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->tim_int_mask1);

	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}


/*
 * vxge_hal_vpath_clear_rx - Acknowledge (that is, clear) the
 * condition that has caused the RX	interrupt.
 * @vpath_handle: Virtual Path handle.
 *
 * Acknowledge (that is, clear)	the condition that has caused
 * the Rx interrupt.
 * See also: vxge_hal_vpath_begin_irq(), vxge_hal_vpath_continue_irq(),
 * vxge_hal_vpath_clear_tx(), vxge_hal_vpath_mask_rx().
 */
void
vxge_hal_vpath_clear_rx(vxge_hal_vpath_h vpath_handle)
{
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return;
	}

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;
	}

	if (vpath->vp_id < 16) {

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    vBIT(VXGE_HAL_INTR_RX, (vpath->vp_id * 4), 4),
		    &hldev->common_reg->tim_int_status0);

	} else {

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(vBIT(VXGE_HAL_INTR_RX, 0, 4), 0),
		    &hldev->common_reg->tim_int_status1);

	}


	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_unmask_rx - Unmask Rx	interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Unmask Rx vpath interrupts.
 *
 * See also: vxge_hal_vpath_mask_rx(), vxge_hal_vpath_clear_rx().
 */
void
vxge_hal_vpath_unmask_rx(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->ringh == NULL) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return;
	}

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;
	}

	if (vpath->vp_id < 16) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask0);

		val64 &= ~vBIT(VXGE_HAL_INTR_RX, (vpath->vp_id * 4), 4);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_mask0);

	} else {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask1);

		val64 &=
		    ~VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(VXGE_HAL_INTR_RX);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->tim_int_mask1);

	}


	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_mask_tx_rx - Mask Tx and Rx interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Mask	Tx and Rx vpath interrupts.
 *
 * See also: vxge_hal_vpath_unmask_tx_rx(), vxge_hal_vpath_clear_tx_rx().
 */
void
vxge_hal_vpath_mask_tx_rx(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;
	}

	if (vpath->vp_id < 16) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask0);

		val64 |= vBIT(VXGE_HAL_INTR_TX, (vpath->vp_id * 4), 4) |
		    vBIT(VXGE_HAL_INTR_RX, (vpath->vp_id * 4), 4);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_mask0);

	} else {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask1);

		val64 |=
		    VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(VXGE_HAL_INTR_TX) |
		    VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(VXGE_HAL_INTR_RX);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->tim_int_mask1);

	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}


/*
 * vxge_hal_vpath_clear_tx_rx - Acknowledge (that is, clear) the
 * condition that has caused the Tx and RX interrupt.
 * @vpath_handle: Virtual Path handle.
 *
 * Acknowledge (that is, clear)	the condition that has caused
 * the Tx and Rx interrupt.
 * See also: vxge_hal_vpath_begin_irq(), vxge_hal_vpath_continue_irq(),
 * vxge_hal_vpath_clear_tx_rx(), vxge_hal_vpath_mask_tx_rx().
 */
void
vxge_hal_vpath_clear_tx_rx(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;
	}

	val64 = 0;

	if (vpath->vp_id < 16) {

		if (vpath->fifoh != NULL)
			val64 |= vBIT(VXGE_HAL_INTR_TX, (vpath->vp_id * 4), 4);
		else
			val64 &= ~vBIT(VXGE_HAL_INTR_TX, (vpath->vp_id * 4), 4);

		if (vpath->ringh != NULL)
			val64 |= vBIT(VXGE_HAL_INTR_RX, (vpath->vp_id * 4), 4);
		else
			val64 &= ~vBIT(VXGE_HAL_INTR_RX, (vpath->vp_id * 4), 4);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_status0);

	} else {

		if (vpath->fifoh != NULL)
			val64 |= vBIT(VXGE_HAL_INTR_TX, 0, 4);

		if (vpath->ringh != NULL)
			val64 |= vBIT(VXGE_HAL_INTR_RX, 0, 4);
		else
			val64 &= ~vBIT(VXGE_HAL_INTR_RX, 0, 4);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->tim_int_status1);

	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_unmask_tx_rx - Unmask Tx and Rx interrupts.
 * @vpath_handle: Virtual Path handle.
 *
 * Unmask Tx and Rx vpath interrupts.
 *
 * See also: vxge_hal_vpath_mask_tx_rx(), vxge_hal_vpath_clear_tx_rx().
 */
void
vxge_hal_vpath_unmask_tx_rx(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;
	}

	if (vpath->vp_id < 16) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask0);

		if (vpath->fifoh != NULL)
			val64 &= ~vBIT(VXGE_HAL_INTR_TX, (vpath->vp_id * 4), 4);
		else
			val64 |= vBIT(VXGE_HAL_INTR_TX, (vpath->vp_id * 4), 4);

		if (vpath->ringh != NULL)
			val64 &= ~vBIT(VXGE_HAL_INTR_RX, (vpath->vp_id * 4), 4);
		else
			val64 |= vBIT(VXGE_HAL_INTR_RX, (vpath->vp_id * 4), 4);

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &hldev->common_reg->tim_int_mask0);

	} else {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->tim_int_mask1);

		if (vpath->fifoh != NULL)
			val64 &= ~VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(
			    VXGE_HAL_INTR_TX);
		else
			val64 |= VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(
			    VXGE_HAL_INTR_TX);

		if (vpath->ringh != NULL)
			val64 &= ~VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(
			    VXGE_HAL_INTR_RX);
		else
			val64 |= VXGE_HAL_TIM_INT_MASK1_TIM_INT_MASK1(
			    VXGE_HAL_INTR_RX);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->tim_int_mask1);

	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_alarm_process - Process Alarms.
 * @vpath: Virtual Path.
 * @skip_alarms: Do not clear the alarms
 *
 * Process vpath alarms.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_alarm_process(vxge_hal_vpath_h vpath_handle, u32 skip_alarms)
{
	vxge_hal_status_e status;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	status = __hal_vpath_alarm_process(
	    vp->vpath,
	    skip_alarms);

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_msix_mode - Is MSIX enabled?
 * @vpath_handle: Virtual Path handle.
 *
 * Returns 0 if MSI is enabled for the specified device,
 * non-zero otherwise.
 */
u32
vxge_hal_vpath_msix_mode(vxge_hal_vpath_h vpath_handle)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (hldev->header.msix_enabled);
}

/*
 * vxge_hal_vpath_msix_set
 * Associate MSIX vectors with TIM interrupts and alrms
 * @vpath_handle: Virtual Path handle.
 * @tim_msix_id: MSIX vectors associated with VXGE_HAL_VPATH_MSIX_MAX number of
 *		interrupts(Can be repeated). If fifo or ring are not enabled
 *		the MSIX vector for that should be set to 0
 * @alarm_msix_id: MSIX vector for alarm.
 *
 * This API will associate a given MSIX vector numbers with the four TIM
 * interrupts and alarm interrupt.
 */
vxge_hal_status_e
vxge_hal_vpath_msix_set(vxge_hal_vpath_h vpath_handle,
    int *tim_msix_id,
    int alarm_msix_id)
{
	u32 i;
	u32 j;
	u32 rvp_id;
	u32 msix_id;
	u64 val64;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vp != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "tim_msix_id0 = %d, tim_msix_id1 = %d, tim_msix_id2 = %d, "
	    "tim_msix_id3 = %d, alarm_msix_id = %d", (ptr_t) vpath_handle,
	    tim_msix_id[0], tim_msix_id[1], tim_msix_id[2], tim_msix_id[3],
	    alarm_msix_id);

	for (i = 0; i < VXGE_HAL_VPATH_MSIX_MAX + 1; i++) {

		if (i == VXGE_HAL_VPATH_MSIX_MAX)
			msix_id = alarm_msix_id;
		else
			msix_id = tim_msix_id[i];

		rvp_id = msix_id / VXGE_HAL_VPATH_MSIX_MAX;

		for (j = 0; j < VXGE_HAL_MAX_VIRTUAL_PATHS; j++) {

			if (!(hldev->vpath_assignments & mBIT(j)))
				continue;

			if (rvp_id-- == 0) {
				hldev->msix_map[msix_id].vp_id = j;
				hldev->msix_map[msix_id].int_num =
				    msix_id % VXGE_HAL_VPATH_MSIX_MAX;
				break;
			}
		}
	}

	val64 = VXGE_HAL_INTERRUPT_CFG0_GROUP0_MSIX_FOR_TXTI(
	    hldev->msix_map[tim_msix_id[0]].vp_id * VXGE_HAL_VPATH_MSIX_MAX +
	    hldev->msix_map[tim_msix_id[0]].int_num) |
	    VXGE_HAL_INTERRUPT_CFG0_GROUP1_MSIX_FOR_TXTI(
	    hldev->msix_map[tim_msix_id[1]].vp_id * VXGE_HAL_VPATH_MSIX_MAX +
	    hldev->msix_map[tim_msix_id[1]].int_num) |
	    VXGE_HAL_INTERRUPT_CFG0_GROUP2_MSIX_FOR_TXTI(
	    hldev->msix_map[tim_msix_id[2]].vp_id * VXGE_HAL_VPATH_MSIX_MAX +
	    hldev->msix_map[tim_msix_id[2]].int_num) |
	    VXGE_HAL_INTERRUPT_CFG0_GROUP3_MSIX_FOR_TXTI(
	    hldev->msix_map[tim_msix_id[3]].vp_id * VXGE_HAL_VPATH_MSIX_MAX +
	    hldev->msix_map[tim_msix_id[3]].int_num);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vp->vpath->vp_reg->interrupt_cfg0);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_INTERRUPT_CFG2_ALARM_MAP_TO_MSG(
	    hldev->msix_map[alarm_msix_id].vp_id * VXGE_HAL_VPATH_MSIX_MAX +
	    hldev->msix_map[alarm_msix_id].int_num),
	    &vp->vpath->vp_reg->interrupt_cfg2);

	if (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_MSIX_ONE_SHOT) {

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    VXGE_HAL_ONE_SHOT_VECT0_EN_ONE_SHOT_VECT0_EN, 0),
		    &vp->vpath->vp_reg->one_shot_vect0_en);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    VXGE_HAL_ONE_SHOT_VECT1_EN_ONE_SHOT_VECT1_EN, 0),
		    &vp->vpath->vp_reg->one_shot_vect1_en);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    VXGE_HAL_ONE_SHOT_VECT2_EN_ONE_SHOT_VECT2_EN, 0),
		    &vp->vpath->vp_reg->one_shot_vect2_en);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    VXGE_HAL_ONE_SHOT_VECT3_EN_ONE_SHOT_VECT3_EN, 0),
		    &vp->vpath->vp_reg->one_shot_vect3_en);

	} else if (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_EMULATED_INTA) {
		/* For emulated-INTA we are only using MSI-X 1 to be one shot */
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    VXGE_HAL_ONE_SHOT_VECT1_EN_ONE_SHOT_VECT1_EN, 0),
		    &vp->vpath->vp_reg->one_shot_vect1_en);

	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_msix_mask - Mask MSIX Vector.
 * @vpath_handle: Virtual Path handle.
 * @msix_id:  MSIX ID
 *
 * The function masks the msix interrupt for the given msix_id
 *
 * Note:
 *
 * Returns: 0,
 * Otherwise, VXGE_HAL_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_msix_mask(vxge_hal_vpath_h vpath_handle, int msix_id)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", msix_id = %d",
	    (ptr_t) vpath_handle, msix_id);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(mBIT(hldev->msix_map[msix_id].vp_id), 0),
	    &hldev->common_reg->set_msix_mask_vect[
	    hldev->msix_map[msix_id].int_num]);

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_msix_clear - Clear MSIX Vector.
 * @vpath_handle: Virtual Path handle.
 * @msix_id:  MSI ID
 *
 * The function clears the msix interrupt for the given msix_id
 *
 * Note:
 *
 * Returns: 0,
 * Otherwise, VXGE_HAL_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_msix_clear(vxge_hal_vpath_h vpath_handle, int msix_id)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp;

	vxge_assert(vpath_handle != NULL);

	vp = (__hal_vpath_handle_t *) vpath_handle;
	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", msix_id = %d",
	    (ptr_t) vpath_handle, msix_id);

	if ((hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_MSIX_ONE_SHOT) ||
	    (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_EMULATED_INTA)) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(mBIT(hldev->msix_map[msix_id].vp_id), 0),
		    &hldev->common_reg->clr_msix_one_shot_vec[
		    hldev->msix_map[msix_id].int_num]);

		if (hldev->header.config.intr_mode ==
		    VXGE_HAL_INTR_MODE_EMULATED_INTA) {
			/* Adding read to flush the write,
			 * for HP-ISS platform
			 */
			vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &hldev->common_reg->titan_general_int_status);
		}
	} else {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(mBIT(hldev->msix_map[msix_id].vp_id), 0),
		    &hldev->common_reg->clear_msix_mask_vect[
		    hldev->msix_map[msix_id].int_num]);
	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/* NEW CODE BEGIN */

vxge_hal_status_e
vxge_hal_vpath_mf_msix_set(vxge_hal_vpath_h vpath_handle,
    int *tim_msix_id,
    int alarm_msix_id)
{

	u64 val64;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp;

	vxge_assert(vpath_handle != NULL);

	vp = (__hal_vpath_handle_t *) vpath_handle;
	hldev = vp->vpath->hldev;

	/* Write the internal msi-x vectors numbers */
	val64 = VXGE_HAL_INTERRUPT_CFG0_GROUP0_MSIX_FOR_TXTI(tim_msix_id[0]) |
	    VXGE_HAL_INTERRUPT_CFG0_GROUP1_MSIX_FOR_TXTI(tim_msix_id[1]);

#if defined(VXGE_EMULATED_INTA)
	if (hldev->config.intr_mode ==
	    VXGE_HAL_INTR_MODE_EMULATED_INTA)
		val64 |= VXGE_HAL_INTERRUPT_CFG0_GROUP2_MSIX_FOR_TXTI(
		    (vp->vpath->vp_id * 4) + tim_msix_id[2]);
#endif

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vp->vpath->vp_reg->interrupt_cfg0);

	vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp->vpath->vp_reg->interrupt_cfg0);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_INTERRUPT_CFG2_ALARM_MAP_TO_MSG(
	    (hldev->first_vp_id * 4) + alarm_msix_id),
	    &vp->vpath->vp_reg->interrupt_cfg2);

	if (
#if defined(VXGE_EMULATED_INTA)
	    (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_EMULATED_INTA) ||
#endif
	    (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_MSIX_ONE_SHOT)) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    VXGE_HAL_ONE_SHOT_VECT1_EN_ONE_SHOT_VECT1_EN, 0),
		    &vp->vpath->vp_reg->one_shot_vect1_en);
	}

	if (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_MSIX_ONE_SHOT) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    VXGE_HAL_ONE_SHOT_VECT2_EN_ONE_SHOT_VECT2_EN, 0),
		    &vp->vpath->vp_reg->one_shot_vect2_en);

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0, (u32) bVAL32(
		    VXGE_HAL_ONE_SHOT_VECT3_EN_ONE_SHOT_VECT3_EN, 0),
		    &vp->vpath->vp_reg->one_shot_vect3_en);
	}

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_mf_msix_mask - Mask MSIX Vector.
 * @vp: Virtual Path handle.
 * @msix_id:  MSIX ID
 *
 * The function masks the msix interrupt for the given msix_id
 *
 * Returns: 0,
 * Otherwise, VXGE_HW_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_mf_msix_mask(vxge_hal_vpath_h vpath_handle, int msix_id)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_hal_pio_mem_write32_upper(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0, (u32) bVAL32(mBIT(msix_id >> 2), 0),
	    &vp->vpath->hldev->common_reg->set_msix_mask_vect[msix_id % 4]);
}

/*
 * vxge_hal_vpath_mf_msix_clear - Clear MSIX Vector.
 * @vp: Virtual Path handle.
 * @msix_id:  MSI ID
 *
 * The function clears the msix interrupt for the given msix_id
 *
 * Returns: 0,
 * Otherwise, VXGE_HW_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_mf_msix_clear(vxge_hal_vpath_h vpath_handle, int msix_id)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	if (
#if defined(VXGE_EMULATED_INTA)
	    (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_EMULATED_INTA) ||
#endif
	    (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_MSIX_ONE_SHOT)) {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(mBIT((msix_id >> 2)), 0),
		    &hldev->common_reg->clr_msix_one_shot_vec[msix_id % 4]);
	} else {
		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(mBIT((msix_id >> 2)), 0),
		    &hldev->common_reg->clear_msix_mask_vect[msix_id % 4]);
	}
}

/*
 * vxge_hal_vpath_mf_msix_unmask - Unmask the MSIX Vector.
 * @vp: Virtual Path handle.
 * @msix_id:  MSI ID
 *
 * The function unmasks the msix interrupt for the given msix_id
 *
 * Returns: 0,
 * Otherwise, VXGE_HW_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_mf_msix_unmask(vxge_hal_vpath_h vpath_handle, int msix_id)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	vxge_hal_pio_mem_write32_upper(vp->vpath->hldev->header.pdev,
	    vp->vpath->hldev->header.regh0,
	    (u32) bVAL32(mBIT(msix_id >> 2), 0),
	    &vp->vpath->hldev->common_reg->
	    clear_msix_mask_vect[msix_id % 4]);
}

/* NEW CODE ENDS */

/*
 * vxge_hal_vpath_msix_unmask - Unmask the MSIX Vector.
 * @vpath_handle: Virtual Path handle.
 * @msix_id:  MSI ID
 *
 * The function unmasks the msix interrupt for the given msix_id
 *
 * Note:
 *
 * Returns: 0,
 * Otherwise, VXGE_HAL_ERR_WRONG_IRQ if the msix index is out of range
 * status.
 * See also:
 */
void
vxge_hal_vpath_msix_unmask(vxge_hal_vpath_h vpath_handle, int msix_id)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq(
	    "vpath_handle = 0x"VXGE_OS_STXFMT", msix_id = %d",
	    (ptr_t) vpath_handle, msix_id);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(mBIT(hldev->msix_map[msix_id].vp_id), 0),
	    &hldev->common_reg->clear_msix_mask_vect[
	    hldev->msix_map[msix_id].int_num]);

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_msix_mask_all - Mask all MSIX vectors for the vpath.
 * @vpath_handle: Virtual Path handle.
 *
 * The function masks all msix interrupt for the given vpath
 *
 */
void
vxge_hal_vpath_msix_mask_all(vxge_hal_vpath_h vpath_handle)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq(
	    "vpath_handle = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(mBIT(vp->vpath->vp_id), 0),
	    &hldev->common_reg->set_msix_mask_all_vect);

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);

}

/*
 * vxge_hal_vpath_msix_unmask_all - Unmask all MSIX vectors for the vpath.
 * @vpath_handle: Virtual Path handle.
 *
 * The function unmasks the msix interrupt for the given vpath
 *
 */
void
vxge_hal_vpath_msix_unmask_all(vxge_hal_vpath_h vpath_handle)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(mBIT(vp->vpath->vp_id), 0),
	    &hldev->common_reg->clear_msix_mask_all_vect);

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d Result = 0",
	    __FILE__, __func__, __LINE__);
}

/*
 * vxge_hal_vpath_poll_rx - Poll Rx Virtual Path for completed
 *			descriptors and process the same.
 * @vpath_handle: Virtual Path ahandle.
 * @got_rx: Buffer to return the flag set if receive interrupt is occurred
 *
 * The function	polls the Rx for the completed	descriptors and	calls
 * the upper-layer driver (ULD)	via supplied completion	callback.
 *
 * Returns: VXGE_HAL_OK, if the polling is completed successful.
 * VXGE_HAL_COMPLETIONS_REMAIN: There are still more completed
 * descriptors available which are yet to be processed.
 *
 * See also: vxge_hal_vpath_poll_tx()
 */
vxge_hal_status_e
vxge_hal_vpath_poll_rx(vxge_hal_vpath_h vpath_handle, u32 *got_rx)
{
	u8 t_code;
	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_rxd_h first_rxdh;
	void *rxd_priv;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	__hal_ring_t *ring;

	vxge_assert((vpath_handle != NULL) && (got_rx != NULL));

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq(
	    "vpathh = 0x"VXGE_OS_STXFMT", got_rx = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) got_rx);

	ring = (__hal_ring_t *) vpath->ringh;
	if (ring == NULL) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	ring->cmpl_cnt = 0;
	ring->channel.poll_bytes = 0;
	*got_rx = 0;

	if ((status = vxge_hal_ring_rxd_next_completed(vpath_handle,
	    &first_rxdh, &rxd_priv, &t_code)) == VXGE_HAL_OK) {
		if (ring->callback(vpath_handle, first_rxdh, rxd_priv,
		    t_code, ring->channel.userdata) != VXGE_HAL_OK) {
			status = VXGE_HAL_COMPLETIONS_REMAIN;
		}

		(*got_rx)++;
	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_poll_tx - Poll Tx for completed descriptors and process
 *			the same.
 * @vpath_handle: Virtual Path ahandle.
 * @got_tx: Buffer to return the flag set if transmit interrupt is occurred
 *
 * The function	polls the Tx for the completed	descriptors and	calls
 * the upper-layer driver (ULD)	via supplied completion callback.
 *
 * Returns: VXGE_HAL_OK, if the polling is completed successful.
 * VXGE_HAL_COMPLETIONS_REMAIN: There are still more completed
 * descriptors available which are yet to be processed.
 *
 * See also: vxge_hal_vpath_poll_rx().
 */
vxge_hal_status_e
vxge_hal_vpath_poll_tx(vxge_hal_vpath_h vpath_handle, u32 *got_tx)
{
	vxge_hal_fifo_tcode_e t_code;
	vxge_hal_txdl_h first_txdlh;
	void *txdl_priv;
	__hal_virtualpath_t *vpath;
	__hal_fifo_t *fifo;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((vpath_handle != NULL) && (got_tx != NULL));

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath_irq("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath_irq(
	    "vpathh = 0x"VXGE_OS_STXFMT", got_tx = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) got_tx);

	fifo = (__hal_fifo_t *) vpath->fifoh;
	if (fifo == NULL) {
		vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	fifo->channel.poll_bytes = 0;
	*got_tx = 0;

	if ((status = vxge_hal_fifo_txdl_next_completed(vpath_handle,
	    &first_txdlh, &txdl_priv, &t_code)) == VXGE_HAL_OK) {
		if (fifo->callback(vpath_handle, first_txdlh, txdl_priv,
		    t_code, fifo->channel.userdata) != VXGE_HAL_OK) {
			status = VXGE_HAL_COMPLETIONS_REMAIN;
		}

		(*got_tx)++;
	}

	vxge_hal_trace_log_vpath_irq("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}


/*
 * __hal_vpath_mgmt_read
 * @hldev: HAL device
 * @vpath: Virtual path structure
 *
 * This routine reads the vpath_mgmt registers
 */
vxge_hal_status_e
__hal_vpath_mgmt_read(
    __hal_device_t *hldev,
    __hal_virtualpath_t *vpath)
{
	u32 i, mtu;
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((hldev != NULL) && (vpath != NULL));

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "hldev = 0x"VXGE_OS_STXFMT", vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) hldev, (ptr_t) vpath);

	vpath->sess_grps_available = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vpmgmt_reg->sgrp_own);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vpmgmt_reg->vpath_is_first);

	vpath->is_first_vpath =
	    (u32) VXGE_HAL_VPATH_IS_FIRST_GET_VPATH_IS_FIRST(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vpmgmt_reg->tim_vpath_assignment);

	vpath->bmap_root_assigned =
	    (u32) VXGE_HAL_TIM_VPATH_ASSIGNMENT_GET_BMAP_ROOT(val64);

	mtu = 0;

	for (i = 0; i < VXGE_HAL_MAC_MAX_WIRE_PORTS; i++) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vpmgmt_reg->rxmac_cfg0_port_vpmgmt_clone[i]);

		if (mtu < (u32)
		    VXGE_HAL_RXMAC_CFG0_PORT_VPMGMT_CLONE_GET_MAX_PYLD_LEN(
		    val64)) {
			mtu = (u32)
			    VXGE_HAL_RXMAC_CFG0_PORT_VPMGMT_CLONE_GET_MAX_PYLD_LEN(
			    val64);
		}
	}

	vpath->max_mtu = mtu + VXGE_HAL_MAC_HEADER_MAX_SIZE;

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vpmgmt_reg->xmac_vsport_choices_vp);

	vpath->vsport_choices =
	    (u32) VXGE_HAL_XMAC_VSPORT_CHOICES_VP_GET_VSPORT_VECTOR(val64);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (val64 & mBIT(i))
			vpath->vsport_number = i;

	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vpmgmt_reg->xgmac_gen_status_vpmgmt_clone);

	if (val64 & VXGE_HAL_XGMAC_GEN_STATUS_VPMGMT_CLONE_XMACJ_NTWK_OK) {

		VXGE_HAL_DEVICE_LINK_STATE_SET(vpath->hldev, VXGE_HAL_LINK_UP);

	} else {

		VXGE_HAL_DEVICE_LINK_STATE_SET(vpath->hldev,
		    VXGE_HAL_LINK_DOWN);

	}

	if (val64 &
	    VXGE_HAL_XGMAC_GEN_STATUS_VPMGMT_CLONE_XMACJ_NTWK_DATA_RATE) {

		VXGE_HAL_DEVICE_DATA_RATE_SET(vpath->hldev,
		    VXGE_HAL_DATA_RATE_10G);

	} else {

		VXGE_HAL_DEVICE_DATA_RATE_SET(vpath->hldev,
		    VXGE_HAL_DATA_RATE_1G);

	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_reset_check - Check if resetting the vpath completed
 *
 * @vpath: Virtual Path
 *
 * This routine checks the vpath_rst_in_prog register to see if adapter
 * completed the reset process for the vpath
 */
vxge_hal_status_e
__hal_vpath_reset_check(
    __hal_virtualpath_t *vpath)
{
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->vpath_rst_in_prog,
	    0,
	    VXGE_HAL_VPATH_RST_IN_PROG_VPATH_RST_IN_PROG(
	    1 << (16 - vpath->vp_id)),
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	vxge_hal_trace_log_vpath("<== %s:%s:%d Result = %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_hw_reset
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine resets the vpath on the device
 */
vxge_hal_status_e
__hal_vpath_hw_reset(vxge_hal_device_h devh, u32 vp_id)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;

	__hal_device_t *hldev;

	vxge_assert(devh != NULL);

	hldev = (__hal_device_t *) devh;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	val64 = VXGE_HAL_CMN_RSTHDLR_CFG0_SW_RESET_VPATH(1 << (16 - vp_id));
	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->cmn_rsthdlr_cfg0);

	(void) __hal_ifmsg_wmsg_post(hldev,
	    vp_id,
	    VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_BROADCAST,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_VPATH_RESET_BEGIN,
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_sw_reset
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine resets the vpath structures
 */
vxge_hal_status_e
__hal_vpath_sw_reset(
    vxge_hal_device_h devh,
    u32 vp_id)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;

	vxge_assert(devh != NULL);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	if (vpath->ringh) {

		status = __hal_ring_reset(vpath->ringh);

		if (status != VXGE_HAL_OK) {
			vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}
	}

	if (vpath->fifoh) {

		status = __hal_fifo_reset(vpath->fifoh);

		if (status != VXGE_HAL_OK) {
			vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_enable
 * @vpath_handle: Handle to the vpath object
 *
 * This routine clears the vpath reset and puts vpath in service
 */
vxge_hal_status_e
vxge_hal_vpath_enable(
    vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vxge_assert(vpath_handle != NULL);

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	hldev = vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	val64 = VXGE_HAL_CMN_RSTHDLR_CFG1_CLR_VPATH_RESET(
	    1 << (16 - vpath->vp_id));

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->cmn_rsthdlr_cfg1);

	(void) __hal_ifmsg_wmsg_post(hldev,
	    vpath->vp_id,
	    VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_BROADCAST,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_VPATH_RESET_END,
	    0);

	VXGE_HAL_RING_POST_DOORBELL(vpath_handle, vpath->ringh);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}


/*
 * __hal_vpath_prc_configure
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine configures the prc registers of virtual path
 * using the config passed
 */
vxge_hal_status_e
__hal_vpath_prc_configure(
    vxge_hal_device_h devh,
    u32 vp_id)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;
	vxge_hal_vp_config_t *vp_config;

	vxge_assert(devh != NULL);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	vp_config = vpath->vp_config;

	if (vp_config->ring.enable == VXGE_HAL_RING_DISABLE) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->prc_cfg1);

	if (vp_config->ring.rx_timer_val !=
	    VXGE_HAL_RING_USE_FLASH_DEFAULT_RX_TIMER_VAL) {
		val64 &= ~VXGE_HAL_PRC_CFG1_RX_TIMER_VAL(0x1fffffff);
		val64 |= VXGE_HAL_PRC_CFG1_RX_TIMER_VAL(
		    vp_config->ring.rx_timer_val);
	}

	val64 |= VXGE_HAL_PRC_CFG1_RTI_TINT_DISABLE;

	if (vp_config->ring.greedy_return !=
	    VXGE_HAL_RING_GREEDY_RETURN_USE_FLASH_DEFAULT) {
		if (vp_config->ring.greedy_return)
			val64 |= VXGE_HAL_PRC_CFG1_GREEDY_RETURN;
		else
			val64 &= ~VXGE_HAL_PRC_CFG1_GREEDY_RETURN;
	}

	if (vp_config->ring.rx_timer_ci !=
	    VXGE_HAL_RING_RX_TIMER_CI_USE_FLASH_DEFAULT) {
		if (vp_config->ring.rx_timer_ci)
			val64 |= VXGE_HAL_PRC_CFG1_RX_TIMER_CI;
		else
			val64 &= ~VXGE_HAL_PRC_CFG1_RX_TIMER_CI;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->prc_cfg1);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->prc_cfg7);

	if (vpath->vp_config->ring.scatter_mode !=
	    VXGE_HAL_RING_SCATTER_MODE_USE_FLASH_DEFAULT) {

		val64 &= ~VXGE_HAL_PRC_CFG7_SCATTER_MODE(0x3);

		switch (vpath->vp_config->ring.scatter_mode) {
		case VXGE_HAL_RING_SCATTER_MODE_A:
			val64 |= VXGE_HAL_PRC_CFG7_SCATTER_MODE(
			    VXGE_HAL_PRC_CFG7_SCATTER_MODE_A);
			break;
		case VXGE_HAL_RING_SCATTER_MODE_B:
			val64 |= VXGE_HAL_PRC_CFG7_SCATTER_MODE(
			    VXGE_HAL_PRC_CFG7_SCATTER_MODE_B);
			break;
		case VXGE_HAL_RING_SCATTER_MODE_C:
			val64 |= VXGE_HAL_PRC_CFG7_SCATTER_MODE(
			    VXGE_HAL_PRC_CFG7_SCATTER_MODE_C);
			break;
		}
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->prc_cfg7);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->prc_cfg6);

	if (vpath->vp_config->ring.post_mode !=
	    VXGE_HAL_RING_POST_MODE_USE_FLASH_DEFAULT) {

		if (vpath->vp_config->ring.post_mode ==
		    VXGE_HAL_RING_POST_MODE_DOORBELL)
			val64 |= VXGE_HAL_PRC_CFG6_DOORBELL_MODE_EN;
		else
			val64 &= ~VXGE_HAL_PRC_CFG6_DOORBELL_MODE_EN;

	} else {

		vpath->vp_config->ring.post_mode =
		    ((val64 & VXGE_HAL_PRC_CFG6_DOORBELL_MODE_EN) ?
		    VXGE_HAL_RING_POST_MODE_DOORBELL :
		    VXGE_HAL_RING_POST_MODE_LEGACY);

	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->prc_cfg6);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_PRC_CFG5_RXD0_ADD(
	    __hal_ring_first_block_address_get(vpath->ringh) >> 3),
	    &vpath->vp_reg->prc_cfg5);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->prc_cfg4);

	val64 |= VXGE_HAL_PRC_CFG4_IN_SVC;

	val64 &= ~VXGE_HAL_PRC_CFG4_RING_MODE(0x3);

	if (vp_config->ring.buffer_mode == VXGE_HAL_RING_RXD_BUFFER_MODE_1) {
		val64 |= VXGE_HAL_PRC_CFG4_RING_MODE(
		    VXGE_HAL_PRC_CFG4_RING_MODE_ONE_BUFFER);
	} else {
		if (vp_config->ring.buffer_mode ==
		    VXGE_HAL_RING_RXD_BUFFER_MODE_3) {
			val64 |= VXGE_HAL_PRC_CFG4_RING_MODE(
			    VXGE_HAL_PRC_CFG4_RING_MODE_THREE_BUFFER);
		} else {
			val64 |= VXGE_HAL_PRC_CFG4_RING_MODE(
			    VXGE_HAL_PRC_CFG4_RING_MODE_FIVE_BUFFER);
		}
	}

	if (vp_config->ring.no_snoop_bits !=
	    VXGE_HAL_RING_NO_SNOOP_USE_FLASH_DEFAULT) {

		val64 &= ~(VXGE_HAL_PRC_CFG4_FRM_NO_SNOOP |
		    VXGE_HAL_PRC_CFG4_RXD_NO_SNOOP);

		if (vp_config->ring.no_snoop_bits ==
		    VXGE_HAL_RING_NO_SNOOP_RXD) {
			val64 |= VXGE_HAL_PRC_CFG4_RXD_NO_SNOOP;
		} else {
			if (vp_config->ring.no_snoop_bits ==
			    VXGE_HAL_RING_NO_SNOOP_FRM) {
				val64 |= VXGE_HAL_PRC_CFG4_FRM_NO_SNOOP;
			} else {
				if (vp_config->ring.no_snoop_bits ==
				    VXGE_HAL_RING_NO_SNOOP_ALL) {
					val64 |= VXGE_HAL_PRC_CFG4_FRM_NO_SNOOP;
					val64 |= VXGE_HAL_PRC_CFG4_RXD_NO_SNOOP;
				}
			}
		}

	}

	if (hldev->header.config.rth_en == VXGE_HAL_RTH_DISABLE)
		val64 |= VXGE_HAL_PRC_CFG4_RTH_DISABLE;
	else
		val64 &= ~VXGE_HAL_PRC_CFG4_RTH_DISABLE;

	val64 |= VXGE_HAL_PRC_CFG4_SIGNAL_BENIGN_OVFLW;

	val64 |= VXGE_HAL_PRC_CFG4_BIMODAL_INTERRUPT;

	if (vp_config->ring.backoff_interval_us !=
	    VXGE_HAL_USE_FLASH_DEFAULT_BACKOFF_INTERVAL_US) {

		val64 &= ~VXGE_HAL_PRC_CFG4_BACKOFF_INTERVAL(0xffffff);

		val64 |= VXGE_HAL_PRC_CFG4_BACKOFF_INTERVAL(
		    vp_config->ring.backoff_interval_us * 1000 / 4);

	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->prc_cfg4);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_vpath_kdfc_configure
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine configures the kdfc registers of virtual path
 * using the config passed
 */
vxge_hal_status_e
__hal_vpath_kdfc_configure(
    vxge_hal_device_h devh,
    u32 vp_id)
{
	u64 val64;
	u64 vpath_stride;
	u64 fifo_stride;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;

	vxge_assert(devh != NULL);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	status = __hal_kdfc_swapper_set((vxge_hal_device_t *) hldev, vp_id);


	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);

	}

	if ((vpath->vp_config->ring.post_mode ==
	    VXGE_HAL_RING_POST_MODE_DOORBELL) &&
	    (vxge_hal_device_check_id(devh) == VXGE_HAL_CARD_TITAN_1)) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->rxdmem_size);

		vpath->rxd_mem_size =
		    (u32) VXGE_HAL_RXDMEM_SIZE_PRC_RXDMEM_SIZE(val64) * 8;

	} else {

		vpath->rxd_mem_size = (VXGE_HAL_MAX_RING_LENGTH /
		    vxge_hal_ring_rxds_per_block_get(
		    vpath->vp_config->ring.buffer_mode)) *
		    VXGE_OS_HOST_PAGE_SIZE;

	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->kdfc_drbl_triplet_total);

	vpath->max_kdfc_db =
	    (u32) VXGE_HAL_KDFC_DRBL_TRIPLET_TOTAL_GET_KDFC_MAX_SIZE(val64 + 1) / 2;

	vpath->max_ofl_db = 0;

	if (vpath->vp_config->fifo.enable == VXGE_HAL_FIFO_ENABLE) {

		vpath->max_nofl_db = vpath->max_kdfc_db - 1;
		vpath->max_msg_db = 0;

		if (vpath->max_nofl_db < vpath->vp_config->fifo.fifo_length) {

			vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__,
			    VXGE_HAL_BADCFG_FIFO_LENGTH);
			return (VXGE_HAL_BADCFG_FIFO_LENGTH);
		}

	} else {

		vpath->max_nofl_db = 0;
		vpath->max_msg_db = vpath->max_kdfc_db;
	}

	val64 = 0;

	if (vpath->max_nofl_db)
		val64 |= VXGE_HAL_KDFC_FIFO_TRPL_PARTITION_LENGTH_0(
		    (vpath->max_nofl_db * 2) - 1);

	if (vpath->max_msg_db)
		val64 |= VXGE_HAL_KDFC_FIFO_TRPL_PARTITION_LENGTH_1(
		    (vpath->max_msg_db * 2) - 1);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->kdfc_fifo_trpl_partition);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_KDFC_FIFO_TRPL_CTRL_TRIPLET_ENABLE,
	    &vpath->vp_reg->kdfc_fifo_trpl_ctrl);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->kdfc_trpl_fifo_0_ctrl);

	if (vpath->max_nofl_db) {

		val64 &= ~(VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_MODE(0x3) |
		    VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_SELECT(0xFF));

		val64 |= VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_MODE(
		    VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_MODE_NON_OFFLOAD_ONLY) |
#if !defined(VXGE_OS_HOST_BIG_ENDIAN)
		    VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_SWAP_EN |
#endif
		    VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_SELECT(0);

		if (vpath->vp_config->no_snoop !=
		    VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->no_snoop)
				val64 |=
				    VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_NO_SNOOP;
			else
				val64 &=
				    ~VXGE_HAL_KDFC_TRPL_FIFO_0_CTRL_NO_SNOOP;
		}
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->kdfc_trpl_fifo_0_ctrl);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->kdfc_trpl_fifo_1_ctrl);

	if (vpath->max_msg_db) {

		val64 &= ~(VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_MODE(0x3) |
		    VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_SELECT(0xFF));

		val64 |= VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_MODE(
		    VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_MODE_MESSAGES_ONLY) |
#if !defined(VXGE_OS_HOST_BIG_ENDIAN)
		    VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_SWAP_EN |
#endif
		    VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_SELECT(0);

		if (vpath->vp_config->no_snoop !=
		    VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->no_snoop)
				val64 |=
				    VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_NO_SNOOP;
			else
				val64 &=
				    ~VXGE_HAL_KDFC_TRPL_FIFO_1_CTRL_NO_SNOOP;
		}
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->kdfc_trpl_fifo_1_ctrl);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->kdfc_trpl_fifo_2_ctrl);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->kdfc_trpl_fifo_0_wb_address);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->kdfc_trpl_fifo_1_wb_address);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->kdfc_trpl_fifo_2_wb_address);


	vxge_os_wmb();

	vpath_stride = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->toc_reg->toc_kdfc_vpath_stride);

	fifo_stride = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->toc_reg->toc_kdfc_fifo_stride);

	vpath->nofl_db = (__hal_non_offload_db_wrapper_t *) ((void *)(hldev->kdfc +
	    (vp_id * VXGE_HAL_TOC_KDFC_VPATH_STRIDE_GET_TOC_KDFC_VPATH_STRIDE(
	    vpath_stride))));

	vpath->msg_db = (__hal_messaging_db_wrapper_t *) ((void *)(hldev->kdfc +
	    (vp_id * VXGE_HAL_TOC_KDFC_VPATH_STRIDE_GET_TOC_KDFC_VPATH_STRIDE(
	    vpath_stride)) +
	    VXGE_HAL_TOC_KDFC_FIFO_STRIDE_GET_TOC_KDFC_FIFO_STRIDE(
	    fifo_stride)));

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_vpath_mac_configure
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine configures the mac of virtual path using the config passed
 */
vxge_hal_status_e
__hal_vpath_mac_configure(
    vxge_hal_device_h devh,
    u32 vp_id)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;
	vxge_hal_vp_config_t *vp_config;

	vxge_assert(devh != NULL);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	vp_config = vpath->vp_config;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    VXGE_HAL_XMAC_VSPORT_CHOICE_VSPORT_NUMBER(vpath->vsport_number),
	    &vpath->vp_reg->xmac_vsport_choice);

	if (vp_config->ring.enable == VXGE_HAL_RING_ENABLE) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->xmac_rpa_vcfg);

		if (vp_config->rpa_ipv4_tcp_incl_ph !=
		    VXGE_HAL_VPATH_RPA_IPV4_TCP_INCL_PH_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_ipv4_tcp_incl_ph)
				val64 |=
				    VXGE_HAL_XMAC_RPA_VCFG_IPV4_TCP_INCL_PH;
			else
				val64 &=
				    ~VXGE_HAL_XMAC_RPA_VCFG_IPV4_TCP_INCL_PH;
		}

		if (vp_config->rpa_ipv6_tcp_incl_ph !=
		    VXGE_HAL_VPATH_RPA_IPV6_TCP_INCL_PH_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_ipv6_tcp_incl_ph)
				val64 |=
				    VXGE_HAL_XMAC_RPA_VCFG_IPV6_TCP_INCL_PH;
			else
				val64 &=
				    ~VXGE_HAL_XMAC_RPA_VCFG_IPV6_TCP_INCL_PH;
		}

		if (vp_config->rpa_ipv4_udp_incl_ph !=
		    VXGE_HAL_VPATH_RPA_IPV4_UDP_INCL_PH_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_ipv4_udp_incl_ph)
				val64 |=
				    VXGE_HAL_XMAC_RPA_VCFG_IPV4_UDP_INCL_PH;
			else
				val64 &=
				    ~VXGE_HAL_XMAC_RPA_VCFG_IPV4_UDP_INCL_PH;
		}

		if (vp_config->rpa_ipv6_udp_incl_ph !=
		    VXGE_HAL_VPATH_RPA_IPV6_UDP_INCL_PH_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_ipv6_udp_incl_ph)
				val64 |=
				    VXGE_HAL_XMAC_RPA_VCFG_IPV6_UDP_INCL_PH;
			else
				val64 &=
				    ~VXGE_HAL_XMAC_RPA_VCFG_IPV6_UDP_INCL_PH;
		}

		if (vp_config->rpa_l4_incl_cf !=
		    VXGE_HAL_VPATH_RPA_L4_INCL_CF_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_l4_incl_cf)
				val64 |= VXGE_HAL_XMAC_RPA_VCFG_L4_INCL_CF;
			else
				val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_L4_INCL_CF;
		}

		if (vp_config->rpa_strip_vlan_tag !=
		    VXGE_HAL_VPATH_RPA_STRIP_VLAN_TAG_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_strip_vlan_tag)
				val64 |= VXGE_HAL_XMAC_RPA_VCFG_STRIP_VLAN_TAG;
			else
				val64 &= ~VXGE_HAL_XMAC_RPA_VCFG_STRIP_VLAN_TAG;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->xmac_rpa_vcfg);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->rxmac_vcfg0);

		if (vp_config->mtu !=
		    VXGE_HAL_VPATH_USE_FLASH_DEFAULT_INITIAL_MTU) {
			val64 &= ~VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(0x3fff);
			if ((vp_config->mtu + VXGE_HAL_MAC_HEADER_MAX_SIZE) <
			    vpath->max_mtu)
				val64 |= VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(
				    vp_config->mtu +
				    VXGE_HAL_MAC_HEADER_MAX_SIZE);
			else
				val64 |= VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(
				    vpath->max_mtu);
		}

		if (vp_config->rpa_ucast_all_addr_en !=
		    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_ucast_all_addr_en)
				val64 |= VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN;
			else
				val64 &=
				    ~VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN;
		} else {
			if (val64 & VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN) {
				vp_config->rpa_ucast_all_addr_en =
				    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_ENABLE;
			} else {
				vp_config->rpa_ucast_all_addr_en =
				    VXGE_HAL_VPATH_RPA_UCAST_ALL_ADDR_DISABLE;
			}
		}

		if (vp_config->rpa_mcast_all_addr_en !=
		    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_mcast_all_addr_en)
				val64 |= VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN;
			else
				val64 &=
				    ~VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN;
		} else {
			if (val64 & VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN) {
				vp_config->rpa_mcast_all_addr_en =
				    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_ENABLE;
			} else {
				vp_config->rpa_mcast_all_addr_en =
				    VXGE_HAL_VPATH_RPA_MCAST_ALL_ADDR_DISABLE;
			}
		}

		if (vp_config->rpa_bcast_en !=
		    VXGE_HAL_VPATH_RPA_BCAST_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_bcast_en)
				val64 |= VXGE_HAL_RXMAC_VCFG0_BCAST_EN;
			else
				val64 &= ~VXGE_HAL_RXMAC_VCFG0_BCAST_EN;
		} else {
			if (val64 & VXGE_HAL_RXMAC_VCFG0_BCAST_EN) {
				vp_config->rpa_bcast_en =
				    VXGE_HAL_VPATH_RPA_BCAST_ENABLE;
			} else {
				vp_config->rpa_bcast_en =
				    VXGE_HAL_VPATH_RPA_BCAST_DISABLE;
			}
		}

		if (vp_config->rpa_all_vid_en !=
		    VXGE_HAL_VPATH_RPA_ALL_VID_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_all_vid_en)
				val64 |= VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;
			else
				val64 &= ~VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;
		} else {
			if (val64 & VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN) {
				vp_config->rpa_all_vid_en =
				    VXGE_HAL_VPATH_RPA_ALL_VID_ENABLE;
			} else {
				vp_config->rpa_all_vid_en =
				    VXGE_HAL_VPATH_RPA_ALL_VID_DISABLE;
			}
		}

		if (vpath->promisc_en == VXGE_HAL_VP_PROMISC_ENABLE) {
			val64 |= VXGE_HAL_RXMAC_VCFG0_UCAST_ALL_ADDR_EN |
			    VXGE_HAL_RXMAC_VCFG0_MCAST_ALL_ADDR_EN |
			    VXGE_HAL_RXMAC_VCFG0_BCAST_EN |
			    VXGE_HAL_RXMAC_VCFG0_ALL_VID_EN;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->rxmac_vcfg0);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->rxmac_vcfg1);

		val64 &= ~(VXGE_HAL_RXMAC_VCFG1_RTS_RTH_MULTI_IT_BD_MODE(0x3) |
		    VXGE_HAL_RXMAC_VCFG1_RTS_RTH_MULTI_IT_EN_MODE);

		if (hldev->header.config.rth_it_type ==
		    VXGE_HAL_RTH_IT_TYPE_MULTI_IT) {
			val64 |=
			    VXGE_HAL_RXMAC_VCFG1_RTS_RTH_MULTI_IT_BD_MODE(0x2) |
			    VXGE_HAL_RXMAC_VCFG1_RTS_RTH_MULTI_IT_EN_MODE;
		}

		if (vp_config->vp_queue_l2_flow !=
		    VXGE_HAL_VPATH_VP_Q_L2_FLOW_USE_FLASH_DEFAULT) {
			if (vp_config->vp_queue_l2_flow)
				val64 |= VXGE_HAL_RXMAC_VCFG1_CONTRIB_L2_FLOW;
			else
				val64 &= ~VXGE_HAL_RXMAC_VCFG1_CONTRIB_L2_FLOW;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->rxmac_vcfg1);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->fau_rpa_vcfg);

		if (vp_config->rpa_l4_comp_csum !=
		    VXGE_HAL_VPATH_RPA_L4_COMP_CSUM_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_l4_comp_csum)
				val64 |= VXGE_HAL_FAU_RPA_VCFG_L4_COMP_CSUM;
			else
				val64 &= ~VXGE_HAL_FAU_RPA_VCFG_L4_COMP_CSUM;
		}

		if (vp_config->rpa_l3_incl_cf !=
		    VXGE_HAL_VPATH_RPA_L3_INCL_CF_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_l3_incl_cf)
				val64 |= VXGE_HAL_FAU_RPA_VCFG_L3_INCL_CF;
			else
				val64 &= ~VXGE_HAL_FAU_RPA_VCFG_L3_INCL_CF;
		}

		if (vp_config->rpa_l3_comp_csum !=
		    VXGE_HAL_VPATH_RPA_L3_COMP_CSUM_USE_FLASH_DEFAULT) {
			if (vp_config->rpa_l3_comp_csum)
				val64 |= VXGE_HAL_FAU_RPA_VCFG_L3_COMP_CSUM;
			else
				val64 &= ~VXGE_HAL_FAU_RPA_VCFG_L3_COMP_CSUM;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->fau_rpa_vcfg);
	}

	if (vp_config->fifo.enable == VXGE_HAL_FIFO_ENABLE) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->tpa_cfg);

		if (vp_config->tpa_ignore_frame_error !=
		    VXGE_HAL_VPATH_TPA_IGNORE_FRAME_ERROR_USE_FLASH_DEFAULT) {
			if (vp_config->tpa_ignore_frame_error)
				val64 |= VXGE_HAL_TPA_CFG_IGNORE_FRAME_ERR;
			else
				val64 &= ~VXGE_HAL_TPA_CFG_IGNORE_FRAME_ERR;
		}

		if (vp_config->tpa_ipv6_keep_searching !=
		    VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_USE_FLASH_DEFAULT) {
			if (vp_config->tpa_ipv6_keep_searching)
				val64 |= VXGE_HAL_TPA_CFG_IPV6_STOP_SEARCHING;
			else
				val64 &= ~VXGE_HAL_TPA_CFG_IPV6_STOP_SEARCHING;
		}

		if (vp_config->tpa_l4_pshdr_present !=
		    VXGE_HAL_VPATH_TPA_L4_PSHDR_PRESENT_USE_FLASH_DEFAULT) {
			if (vp_config->tpa_l4_pshdr_present)
				val64 |= VXGE_HAL_TPA_CFG_L4_PSHDR_PRESENT;
			else
				val64 &= ~VXGE_HAL_TPA_CFG_L4_PSHDR_PRESENT;
		}

		if (vp_config->tpa_support_mobile_ipv6_hdrs !=
		    VXGE_HAL_VPATH_TPA_SUPPORT_MOBILE_IPV6_HDRS_DEFAULT) {
			if (vp_config->tpa_support_mobile_ipv6_hdrs)
				val64 |=
				    VXGE_HAL_TPA_CFG_SUPPORT_MOBILE_IPV6_HDRS;
			else
				val64 &=
				    ~VXGE_HAL_TPA_CFG_SUPPORT_MOBILE_IPV6_HDRS;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->tpa_cfg);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->tx_protocol_assist_cfg);

		if (vp_config->tpa_lsov2_en !=
		    VXGE_HAL_VPATH_TPA_LSOV2_EN_USE_FLASH_DEFAULT) {
			if (vp_config->tpa_lsov2_en)
				val64 |=
				    VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_LSOV2_EN;
			else
				val64 &=
				    ~VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_LSOV2_EN;
		}

		if (vp_config->tpa_ipv6_keep_searching !=
		    VXGE_HAL_VPATH_TPA_IPV6_KEEP_SEARCHING_USE_FLASH_DEFAULT) {
			if (vp_config->tpa_ipv6_keep_searching)
				val64 |= VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_IPV6_KEEP_SEARCHING;
			else
				val64 &= ~VXGE_HAL_TX_PROTOCOL_ASSIST_CFG_IPV6_KEEP_SEARCHING;
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->tx_protocol_assist_cfg);

	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_vpath_tim_configure
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine configures the tim registers of virtual path
 * using the config passed
 */
vxge_hal_status_e
__hal_vpath_tim_configure(
    vxge_hal_device_h devh,
    u32 vp_id)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_dest_addr);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_vpath_map);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_bitmap);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_remap);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rtdma_rd_optimization_ctrl);

	val64 |= VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_FB_ADDR_BDRY_EN;

	if (hldev->header.config.intr_mode == VXGE_HAL_INTR_MODE_EMULATED_INTA)
		val64 = 0x1000150012000100ULL;	/* override for HPISS */

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rtdma_rd_optimization_ctrl);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->tim_wrkld_clc);

	val64 |= VXGE_HAL_TIM_WRKLD_CLC_WRKLD_EVAL_PRD(0x5BE9) |
	    VXGE_HAL_TIM_WRKLD_CLC_CNT_FRM_BYTE |
	    VXGE_HAL_TIM_WRKLD_CLC_WRKLD_EVAL_DIV(0x15) |
	    VXGE_HAL_TIM_WRKLD_CLC_CNT_RX_TX(3);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->tim_wrkld_clc);

	if (vpath->vp_config->ring.enable == VXGE_HAL_RING_ENABLE) {

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    VXGE_HAL_TIM_RING_ASSN_INT_NUM(vpath->rx_intr_num),
		    &vpath->vp_reg->tim_ring_assn);

	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->tim_pci_cfg);

	val64 |= VXGE_HAL_TIM_PCI_CFG_ADD_PAD;

	if (vpath->vp_config->no_snoop !=
	    VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT) {
		if (vpath->vp_config->no_snoop)
			val64 |= VXGE_HAL_TIM_PCI_CFG_NO_SNOOP;
		else
			val64 &= ~VXGE_HAL_TIM_PCI_CFG_NO_SNOOP;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->tim_pci_cfg);

	if (vpath->vp_config->fifo.enable == VXGE_HAL_FIFO_ENABLE) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_TX]);

		if (vpath->vp_config->tti.btimer_val !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_BTIMER_VAL) {
			val64 &=
			    ~VXGE_HAL_TIM_CFG1_INT_NUM_BTIMER_VAL(0x3ffffff);
			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_BTIMER_VAL(
			    vpath->vp_config->tti.btimer_val);
		}

		val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_BITMP_EN;

		if (vpath->vp_config->tti.txfrm_cnt_en !=
		    VXGE_HAL_TXFRM_CNT_EN_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->tti.txfrm_cnt_en)
				val64 |=
				    VXGE_HAL_TIM_CFG1_INT_NUM_TXFRM_CNT_EN;
			else
				val64 &=
				    ~VXGE_HAL_TIM_CFG1_INT_NUM_TXFRM_CNT_EN;
		}

		if (vpath->vp_config->tti.txd_cnt_en !=
		    VXGE_HAL_TXD_CNT_EN_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->tti.txd_cnt_en)
				val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_TXD_CNT_EN;
			else
				val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_TXD_CNT_EN;
		}

		if (vpath->vp_config->tti.timer_ac_en !=
		    VXGE_HAL_TIM_TIMER_AC_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->tti.timer_ac_en)
				val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_AC;
			else
				val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_AC;
		}

		if (vpath->vp_config->tti.timer_ci_en !=
		    VXGE_HAL_TIM_TIMER_CI_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->tti.timer_ci_en)
				val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;
			else
				val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;
		}

		if (vpath->vp_config->tti.urange_a !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_A) {
			val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_URNG_A(0x3f);
			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_URNG_A(
			    vpath->vp_config->tti.urange_a);
		}

		if (vpath->vp_config->tti.urange_b !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_B) {
			val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_URNG_B(0x3f);
			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_URNG_B(
			    vpath->vp_config->tti.urange_b);
		}

		if (vpath->vp_config->tti.urange_c !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_C) {
			val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_URNG_C(0x3f);
			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_URNG_C(
			    vpath->vp_config->tti.urange_c);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_TX]);

		vpath->tim_tti_cfg1_saved = val64;

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->tim_cfg2_int_num[VXGE_HAL_VPATH_INTR_TX]);

		if (vpath->vp_config->tti.uec_a !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_A) {
			val64 &= ~VXGE_HAL_TIM_CFG2_INT_NUM_UEC_A(0xffff);
			val64 |= VXGE_HAL_TIM_CFG2_INT_NUM_UEC_A(
			    vpath->vp_config->tti.uec_a);
		}

		if (vpath->vp_config->tti.uec_b !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_B) {
			val64 &= ~VXGE_HAL_TIM_CFG2_INT_NUM_UEC_B(0xffff);
			val64 |= VXGE_HAL_TIM_CFG2_INT_NUM_UEC_B(
			    vpath->vp_config->tti.uec_b);
		}

		if (vpath->vp_config->tti.uec_c !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_C) {
			val64 &= ~VXGE_HAL_TIM_CFG2_INT_NUM_UEC_C(0xffff);
			val64 |= VXGE_HAL_TIM_CFG2_INT_NUM_UEC_C(
			    vpath->vp_config->tti.uec_c);
		}

		if (vpath->vp_config->tti.uec_d !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_D) {
			val64 &= ~VXGE_HAL_TIM_CFG2_INT_NUM_UEC_D(0xffff);
			val64 |= VXGE_HAL_TIM_CFG2_INT_NUM_UEC_D(
			    vpath->vp_config->tti.uec_d);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->tim_cfg2_int_num[VXGE_HAL_VPATH_INTR_TX]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->tim_cfg3_int_num[VXGE_HAL_VPATH_INTR_TX]);

		if (vpath->vp_config->tti.timer_ri_en !=
		    VXGE_HAL_TIM_TIMER_RI_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->tti.timer_ri_en)
				val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_TIMER_RI;
			else
				val64 &= ~VXGE_HAL_TIM_CFG3_INT_NUM_TIMER_RI;
		}

		if (vpath->vp_config->tti.rtimer_event_sf !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_EVENT_SF) {
			val64 &=
			    ~VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_EVENT_SF(0xf);
			val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_EVENT_SF(
			    vpath->vp_config->tti.rtimer_event_sf);
		}

		if (vpath->vp_config->tti.rtimer_val !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_VAL) {
			val64 &= ~VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(
			    0x3ffffff);
			val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(
			    vpath->vp_config->tti.rtimer_val);
		}

		if (vpath->vp_config->tti.util_sel !=
		    VXGE_HAL_TIM_UTIL_SEL_USE_FLASH_DEFAULT) {
			val64 &= ~VXGE_HAL_TIM_CFG3_INT_NUM_UTIL_SEL(0x3f);
			val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_UTIL_SEL(
			    vpath->vp_config->tti.util_sel);
		}

		if (vpath->vp_config->tti.ltimer_val !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_LTIMER_VAL) {
			val64 &=
			    ~VXGE_HAL_TIM_CFG3_INT_NUM_LTIMER_VAL(0x3ffffff);
			val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_LTIMER_VAL(
			    vpath->vp_config->tti.ltimer_val);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->tim_cfg3_int_num[VXGE_HAL_VPATH_INTR_TX]);

		vpath->tim_tti_cfg3_saved = val64;
	}

	if (vpath->vp_config->ring.enable == VXGE_HAL_RING_ENABLE) {

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_RX]);

		if (vpath->vp_config->rti.btimer_val !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_BTIMER_VAL) {
			val64 &=
			    ~VXGE_HAL_TIM_CFG1_INT_NUM_BTIMER_VAL(0x3ffffff);
			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_BTIMER_VAL(
			    vpath->vp_config->rti.btimer_val);
		}

		val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_BITMP_EN;

		if (vpath->vp_config->rti.txfrm_cnt_en !=
		    VXGE_HAL_TXFRM_CNT_EN_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->rti.txfrm_cnt_en)
				val64 |=
				    VXGE_HAL_TIM_CFG1_INT_NUM_TXFRM_CNT_EN;
			else
				val64 &=
				    ~VXGE_HAL_TIM_CFG1_INT_NUM_TXFRM_CNT_EN;
		}

		if (vpath->vp_config->rti.txd_cnt_en !=
		    VXGE_HAL_TXD_CNT_EN_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->rti.txd_cnt_en)
				val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_TXD_CNT_EN;
			else
				val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_TXD_CNT_EN;
		}

		if (vpath->vp_config->rti.timer_ac_en !=
		    VXGE_HAL_TIM_TIMER_AC_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->rti.timer_ac_en)
				val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_AC;
			else
				val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_AC;
		}

		if (vpath->vp_config->rti.timer_ci_en !=
		    VXGE_HAL_TIM_TIMER_CI_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->rti.timer_ci_en)
				val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;
			else
				val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;
		}

		if (vpath->vp_config->rti.urange_a !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_A) {
			val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_URNG_A(0x3f);
			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_URNG_A(
			    vpath->vp_config->rti.urange_a);
		}

		if (vpath->vp_config->rti.urange_b !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_B) {
			val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_URNG_B(0x3f);
			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_URNG_B(
			    vpath->vp_config->rti.urange_b);
		}

		if (vpath->vp_config->rti.urange_c !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_URANGE_C) {
			val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_URNG_C(0x3f);
			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_URNG_C(
			    vpath->vp_config->rti.urange_c);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_RX]);

		vpath->tim_rti_cfg1_saved = val64;

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->tim_cfg2_int_num[VXGE_HAL_VPATH_INTR_RX]);

		if (vpath->vp_config->rti.uec_a !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_A) {
			val64 &= ~VXGE_HAL_TIM_CFG2_INT_NUM_UEC_A(0xffff);
			val64 |= VXGE_HAL_TIM_CFG2_INT_NUM_UEC_A(
			    vpath->vp_config->rti.uec_a);
		}

		if (vpath->vp_config->rti.uec_b !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_B) {
			val64 &= ~VXGE_HAL_TIM_CFG2_INT_NUM_UEC_B(0xffff);
			val64 |= VXGE_HAL_TIM_CFG2_INT_NUM_UEC_B(
			    vpath->vp_config->rti.uec_b);
		}

		if (vpath->vp_config->rti.uec_c !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_C) {
			val64 &= ~VXGE_HAL_TIM_CFG2_INT_NUM_UEC_C(0xffff);
			val64 |= VXGE_HAL_TIM_CFG2_INT_NUM_UEC_C(
			    vpath->vp_config->rti.uec_c);
		}

		if (vpath->vp_config->rti.uec_d !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_UEC_D) {
			val64 &= ~VXGE_HAL_TIM_CFG2_INT_NUM_UEC_D(0xffff);
			val64 |= VXGE_HAL_TIM_CFG2_INT_NUM_UEC_D(
			    vpath->vp_config->rti.uec_d);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->tim_cfg2_int_num[VXGE_HAL_VPATH_INTR_RX]);

		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->tim_cfg3_int_num[VXGE_HAL_VPATH_INTR_RX]);

		if (vpath->vp_config->rti.timer_ri_en !=
		    VXGE_HAL_TIM_TIMER_RI_USE_FLASH_DEFAULT) {
			if (vpath->vp_config->rti.timer_ri_en)
				val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_TIMER_RI;
			else
				val64 &= ~VXGE_HAL_TIM_CFG3_INT_NUM_TIMER_RI;
		}

		if (vpath->vp_config->rti.rtimer_event_sf !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_EVENT_SF) {
			val64 &=
			    ~VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_EVENT_SF(0xf);
			val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_EVENT_SF(
			    vpath->vp_config->rti.rtimer_event_sf);
		}

		if (vpath->vp_config->rti.rtimer_val !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_RTIMER_VAL) {
			val64 &=
			    ~VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(0x3ffffff);
			val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(
			    vpath->vp_config->rti.rtimer_val);
		}

		if (vpath->vp_config->rti.util_sel !=
		    VXGE_HAL_TIM_UTIL_SEL_USE_FLASH_DEFAULT) {
			val64 &= ~VXGE_HAL_TIM_CFG3_INT_NUM_UTIL_SEL(0x3f);
			val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_UTIL_SEL(
			    vpath->vp_config->rti.util_sel);
		}

		if (vpath->vp_config->rti.ltimer_val !=
		    VXGE_HAL_USE_FLASH_DEFAULT_TIM_LTIMER_VAL) {
			val64 &=
			    ~VXGE_HAL_TIM_CFG3_INT_NUM_LTIMER_VAL(0x3ffffff);
			val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_LTIMER_VAL(
			    vpath->vp_config->rti.ltimer_val);
		}

		vxge_os_pio_mem_write64(hldev->header.pdev,
		    hldev->header.regh0,
		    val64,
		    &vpath->vp_reg->tim_cfg3_int_num[VXGE_HAL_VPATH_INTR_RX]);

		vpath->tim_rti_cfg3_saved = val64;
	}

	val64 = 0;

	if (hldev->header.config.intr_mode ==
	    VXGE_HAL_INTR_MODE_EMULATED_INTA) {

		val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_BTIMER_VAL(1) |
		    VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;

	}

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_EINTA]);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_cfg2_int_num[VXGE_HAL_VPATH_INTR_EINTA]);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_cfg3_int_num[VXGE_HAL_VPATH_INTR_EINTA]);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_BMAP]);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_cfg2_int_num[VXGE_HAL_VPATH_INTR_BMAP]);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    (u64) 0,
	    &vpath->vp_reg->tim_cfg3_int_num[VXGE_HAL_VPATH_INTR_BMAP]);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_is_rxdmem_leak - Check for the rxd memory leak.
 * @vpath_handle: Virtual Path handle.
 *
 * The function checks for the rxd memory leak.
 *
 */
u32
vxge_hal_vpath_is_rxdmem_leak(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	u32 new_qw_count, rxd_spat, bRet = 0;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vp != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vp->vpath->vp_config->ring.enable == VXGE_HAL_RING_DISABLE) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, bRet);
		return (bRet);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp->vpath->vp_reg->prc_rxd_doorbell);

	new_qw_count = (u32) VXGE_HAL_PRC_RXD_DOORBELL_GET_NEW_QW_CNT(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp->vpath->vp_reg->prc_cfg6);

	rxd_spat = (u32) VXGE_HAL_PRC_CFG6_GET_RXD_SPAT(val64);

	bRet = (new_qw_count > (rxd_spat * 3 / 2));

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, bRet);

	return (bRet);
}

/*
 * vxge_hal_vpath_mtu_check - check MTU value for ranges
 * @vpath_handle: Virtal path handle
 * @new_mtu: new MTU value to check
 *
 * Will do sanity check for new MTU value.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_MTU_SIZE - MTU is invalid.
 *
 * See also: vxge_hal_vpath_mtu_set()
 */
vxge_hal_status_e
vxge_hal_device_mtu_check(vxge_hal_vpath_h vpath_handle,
    unsigned long new_mtu)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vp == NULL) {
		vxge_hal_trace_log_vpath(
		    "<== %s:%s:%d  Result: %d", __FILE__, __func__,
		    __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	new_mtu += VXGE_HAL_MAC_HEADER_MAX_SIZE;

	if ((new_mtu < VXGE_HAL_MIN_MTU) || (new_mtu > vp->vpath->max_mtu)) {
		status = VXGE_HAL_ERR_INVALID_MTU_SIZE;
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_mtu_set - Set MTU.
 * @vpath_handle: Virtal path handle
 * @new_mtu: New MTU size to configure.
 *
 * Set new MTU value. Example, to use jumbo frames:
 * vxge_hal_vpath_mtu_set(my_device, 9600);
 *
 */
vxge_hal_status_e
vxge_hal_vpath_mtu_set(vxge_hal_vpath_h vpath_handle,
    unsigned long new_mtu)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vp == NULL) {
		vxge_hal_trace_log_vpath(
		    "<== %s:%s:%d  Result: %d", __FILE__, __func__,
		    __LINE__, VXGE_HAL_ERR_INVALID_HANDLE);
		return (VXGE_HAL_ERR_INVALID_HANDLE);
	}

	new_mtu += VXGE_HAL_MAC_HEADER_MAX_SIZE;

	if ((new_mtu < VXGE_HAL_MIN_MTU) || (new_mtu > vp->vpath->max_mtu)) {
		status = VXGE_HAL_ERR_INVALID_MTU_SIZE;
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp->vpath->vp_reg->rxmac_vcfg0);

	val64 &= ~VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(0x3fff);
	val64 |= VXGE_HAL_RXMAC_VCFG0_RTS_MAX_FRM_LEN(new_mtu);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vp->vpath->vp_reg->rxmac_vcfg0);

	vp->vpath->vp_config->mtu = new_mtu - VXGE_HAL_MAC_HEADER_MAX_SIZE;

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}


/*
 * __hal_vpath_size_quantum_set
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine configures the size quantum of virtual path
 * using the config passed
 */
vxge_hal_status_e
__hal_vpath_size_quantum_set(
    vxge_hal_device_h devh,
    u32 vp_id)
{
	u64 val64;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	switch (__vxge_os_cacheline_size) {
	case 8:
		val64 = 0;
		break;
	case 16:
		val64 = 1;
		break;
	case 32:
		val64 = 2;
		break;
	case 64:
		val64 = 3;
		break;
	default:
	case 128:
		val64 = 4;
		break;
	case 256:
		val64 = 5;
		break;
	case 512:
		val64 = 6;
		break;
	}

	vxge_os_pio_mem_write64(hldev->header.pdev, hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->vpath_general_cfg2);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_hw_initialize
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine initializes the registers of virtual path
 * using the config passed
 */
vxge_hal_status_e
__hal_vpath_hw_initialize(
    vxge_hal_device_h devh,
    u32 vp_id)
{
	u64 val64;
	u32 mrrs;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_virtualpath_t *vpath;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	vxge_hal_pci_e_capability_t *pci_e_cap;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", vp_id = %d",
	    (ptr_t) devh, vp_id);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	if (!(hldev->vpath_assignments & mBIT(vp_id))) {

		vxge_hal_trace_log_vpath(
		    "<== %s:%s:%d  Result: %d", __FILE__, __func__,
		    __LINE__, VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);
		return (VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);
	}

	status = __hal_vpath_swapper_set((vxge_hal_device_t *) hldev, vp_id);
	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = __hal_vpath_size_quantum_set(hldev, vp_id);
	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = __hal_vpath_mac_configure(hldev, vp_id);
	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = __hal_vpath_kdfc_configure(hldev, vp_id);
	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}


	status = __hal_vpath_tim_configure(hldev, vp_id);
	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vxge_os_pio_mem_write64(hldev->header.pdev, hldev->header.regh0,
	    VXGE_HAL_USDC_VPATH_SGRP_ASSIGN(
	    vpath->sess_grps_available),
	    &vpath->vp_reg->usdc_vpath);

	vxge_os_wmb();

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->qcc_pci_cfg);

	val64 |= VXGE_HAL_QCC_PCI_CFG_ADD_PAD_CQE_SPACE |
	    VXGE_HAL_QCC_PCI_CFG_ADD_PAD_WQE |
	    VXGE_HAL_QCC_PCI_CFG_ADD_PAD_SRQIR |
	    VXGE_HAL_QCC_PCI_CFG_CTL_STR_CQE_SPACE |
	    VXGE_HAL_QCC_PCI_CFG_CTL_STR_WQE |
	    VXGE_HAL_QCC_PCI_CFG_CTL_STR_SRQIR;

	if (vpath->vp_config->no_snoop !=
	    VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT) {
		if (vpath->vp_config->no_snoop) {
			val64 |= VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_CQE_SPACE |
			    VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_WQE |
			    VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_SRQIR;
		} else {
			val64 &= ~(VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_CQE_SPACE |
			    VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_WQE |
			    VXGE_HAL_QCC_PCI_CFG_NO_SNOOP_SRQIR);
		}
	}

	vxge_os_pio_mem_write64(hldev->header.pdev, hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->qcc_pci_cfg);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->h2l_vpath_config);

	if (vpath->vp_config->no_snoop !=
	    VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT) {
		if (vpath->vp_config->no_snoop) {
			val64 |= VXGE_HAL_H2L_VPATH_CONFIG_OD_NO_SNOOP;
		} else {
			val64 &= ~VXGE_HAL_H2L_VPATH_CONFIG_OD_NO_SNOOP;
		}
	}

	vxge_os_pio_mem_write64(hldev->header.pdev, hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->h2l_vpath_config);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->ph2l_vp_cfg0);

	if (vpath->vp_config->no_snoop !=
	    VXGE_HAL_VPATH_NO_SNOOP_USE_FLASH_DEFAULT) {
		if (vpath->vp_config->no_snoop) {
			val64 |= VXGE_HAL_PH2L_VP_CFG0_NOSNOOP_DATA;
		} else {
			val64 &= ~VXGE_HAL_PH2L_VP_CFG0_NOSNOOP_DATA;
		}
	}

	vxge_os_pio_mem_write64(hldev->header.pdev, hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->ph2l_vp_cfg0);

	vxge_os_pio_mem_write64(hldev->header.pdev, hldev->header.regh0,
	    0,
	    &vpath->vp_reg->gendma_int);

	pci_e_cap = (vxge_hal_pci_e_capability_t *)
	    (((char *)&hldev->pci_config_space_bios) + hldev->pci_e_caps);

	mrrs = pci_e_cap->pci_e_devctl >> 12;

	val64 = VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_GEN_INT_AFTER_ABORT |
	    VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_FB_FILL_THRESH(mrrs) |
	    VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_FB_ADDR_BDRY_EN |
	    VXGE_HAL_RTDMA_RD_OPTIMIZATION_CTRL_TXD_FILL_THRESH(1);

	vxge_os_pio_mem_write64(hldev->header.pdev, hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->rtdma_rd_optimization_ctrl);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_vp_initialize - Initialize Virtual Path structure
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 * @config: Configuration for the virtual path
 *
 * This routine initializes virtual path using the config passed
 */
vxge_hal_status_e
__hal_vp_initialize(vxge_hal_device_h devh,
    u32 vp_id,
    vxge_hal_vp_config_t *config)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert((hldev != NULL) && (config != NULL));

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "devh = 0x"VXGE_OS_STXFMT", vp_id = %d, config = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, vp_id, (ptr_t) config);

	if (!(hldev->vpath_assignments & mBIT(vp_id))) {

		vxge_hal_trace_log_vpath(
		    "<== %s:%s:%d  Result: %d", __FILE__, __func__,
		    __LINE__, VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);
		return (VXGE_HAL_ERR_VPATH_NOT_AVAILABLE);
	}

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];
	vpath->vp_id = vp_id;

	vpath->vp_open = VXGE_HAL_VP_OPEN;

	vpath->hldev = (__hal_device_t *) devh;

	vpath->vp_config = config;

	vpath->vp_reg = hldev->vpath_reg[vp_id];

	vpath->vpmgmt_reg = hldev->vpmgmt_reg[vp_id];

	status = __hal_vpath_hw_reset(devh, vp_id);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_vpath(
		    "vpath is already in reset  %s:%s:%d",
		    __FILE__, __func__, __LINE__);
	}

	status = __hal_vpath_reset_check(vpath);

	if (status != VXGE_HAL_OK) {
		vxge_os_memzero(vpath, sizeof(__hal_virtualpath_t));
		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);

		return (status);
	}

	status = __hal_vpath_mgmt_read(hldev, vpath);

	if (status != VXGE_HAL_OK) {
		vxge_os_memzero(vpath, sizeof(__hal_virtualpath_t));
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vpath->tx_intr_num =
	    (vp_id * VXGE_HAL_MAX_INTR_PER_VP) + VXGE_HAL_VPATH_INTR_TX;

	vpath->rx_intr_num =
	    (vp_id * VXGE_HAL_MAX_INTR_PER_VP) + VXGE_HAL_VPATH_INTR_RX;

	vpath->einta_intr_num =
	    (vp_id * VXGE_HAL_MAX_INTR_PER_VP) + VXGE_HAL_VPATH_INTR_EINTA;

	vpath->bmap_intr_num =
	    (vp_id * VXGE_HAL_MAX_INTR_PER_VP) + VXGE_HAL_VPATH_INTR_BMAP;


#if defined(VXGE_HAL_VP_CBS)
	vxge_os_spin_lock_init(&vpath->vpath_handles_lock, hldev->pdev);
#elif defined(VXGE_HAL_VP_CBS_IRQ)
	vxge_os_spin_lock_init_irq(&vpath->vpath_handles_lock, hldev->irqh);
#endif

	vxge_list_init(&vpath->vpath_handles);

	vpath->sw_stats = &hldev->stats.sw_dev_info_stats.vpath_info[vp_id];

	vxge_os_memzero(&vpath->sw_stats->obj_counts,
	    sizeof(vxge_hal_vpath_sw_obj_count_t));

	VXGE_HAL_DEVICE_TIM_INT_MASK_SET(vpath->hldev, vpath->vp_id);

	status = __hal_vpath_hw_initialize(vpath->hldev, vpath->vp_id);

	if (status != VXGE_HAL_OK) {
		__hal_vp_terminate(devh, vp_id);
	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * __hal_vp_terminate - Terminate Virtual Path structure
 * @hldev: Handle to the device object
 * @vp_id: Virtual Path Id
 *
 * This routine closes all channels it opened and freeup memory
 */
void
__hal_vp_terminate(vxge_hal_device_h devh, u32 vp_id)
{
	__hal_virtualpath_t *vpath;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(devh != NULL);

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "devh = 0x"VXGE_OS_STXFMT", vp_id = %d", (ptr_t) devh, vp_id);

	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[vp_id];

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {

		vxge_hal_trace_log_vpath(
		    "<== %s:%s:%d  Result: %d", __FILE__, __func__,
		    __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return;

	}

	VXGE_HAL_DEVICE_TIM_INT_MASK_RESET(vpath->hldev, vpath->vp_id);


#if defined(VXGE_HAL_VP_CBS)
	vxge_os_spin_lock_destroy(
	    &vpath->vpath_handles_lock, hldev->header.pdev);
#elif defined(VXGE_HAL_VP_CBS_IRQ)
	vxge_os_spin_lock_destroy_irq(
	    &vpath->vpath_handles_lock, hldev->header.pdev);
#endif

	vxge_os_memzero(vpath, sizeof(__hal_virtualpath_t));

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
}


/*
 * vxge_hal_vpath_obj_count_get - Get the Object usage count for a given
 *		 virtual path
 * @vpath_handle: Virtal path handle
 * @obj_counts: Buffer to return object counts
 *
 * This function returns the object counts for virtual path.
 */
vxge_hal_status_e
vxge_hal_vpath_obj_count_get(
    vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_sw_obj_count_t *obj_count)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	if ((vpath_handle == NULL) || (obj_count == NULL))
		return (VXGE_HAL_FAIL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT", "
	    "obj_count = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle,
	    (ptr_t) obj_count);

	vxge_os_memcpy(obj_count, &vp->vpath->sw_stats->obj_counts,
	    sizeof(vxge_hal_vpath_sw_obj_count_t));

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_open - Open a virtual path on a given adapter
 * @devh: handle to device object
 * @attr: Virtual path attributes
 * @cb_fn: Call back to be called to complete an asynchronous function call
 * @client_handle: handle to be returned in the callback
 * @vpath_handle: Buffer to return a handle to the vpath
 *
 * This function is used to open access to virtual path of an
 * adapter for offload, LRO and SPDM operations. This function returns
 * synchronously.
 */
vxge_hal_status_e
vxge_hal_vpath_open(vxge_hal_device_h devh,
    vxge_hal_vpath_attr_t *attr,
    vxge_hal_vpath_callback_f cb_fn,
    vxge_hal_client_h client_handle,
    vxge_hal_vpath_h *vpath_handle)
{
	__hal_device_t *hldev = (__hal_device_t *) devh;
	__hal_virtualpath_t *vpath;
	__hal_vpath_handle_t *vp;
	vxge_hal_status_e status;

	vxge_assert((devh != NULL) && (attr != NULL) && (cb_fn != NULL) &&
	    (vpath_handle != NULL));

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("devh = 0x"VXGE_OS_STXFMT", "
	    "attr = 0x"VXGE_OS_STXFMT", cb_fn = 0x"VXGE_OS_STXFMT", "
	    "client_handle = 0x"VXGE_OS_STXFMT", "
	    "vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) devh, (ptr_t) attr, (ptr_t) cb_fn,
	    (ptr_t) client_handle, (ptr_t) vpath_handle);


	vpath = (__hal_virtualpath_t *) &hldev->virtual_paths[attr->vp_id];

	if (vpath->vp_open == VXGE_HAL_VP_OPEN) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_INVALID_STATE);
		return (VXGE_HAL_ERR_INVALID_STATE);
	}

	status = __hal_vp_initialize(hldev, attr->vp_id,
	    &hldev->header.config.vp_config[attr->vp_id]);

	if (status != VXGE_HAL_OK) {

		vxge_hal_err_log_vpath(
		    "virtual Paths: __hal_vp_initialize failed == > %s : %d",
		    __func__, __LINE__);

		goto vpath_open_exit1;

	}

	vp = (__hal_vpath_handle_t *) vxge_os_malloc(hldev->header.pdev,
	    sizeof(__hal_vpath_handle_t));

	if (vp == NULL) {

		status = VXGE_HAL_ERR_OUT_OF_MEMORY;

		goto vpath_open_exit2;

	}

	vxge_os_memzero(vp, sizeof(__hal_vpath_handle_t));

	vp->vpath = vpath;
	vp->cb_fn = cb_fn;
	vp->client_handle = client_handle;


	if (vp->vpath->vp_config->fifo.enable == VXGE_HAL_FIFO_ENABLE) {

		status = __hal_fifo_create(vp, &attr->fifo_attr);
		if (status != VXGE_HAL_OK) {
			goto vpath_open_exit6;
		}
	}

	if (vp->vpath->vp_config->ring.enable == VXGE_HAL_RING_ENABLE) {

		status = __hal_ring_create(vp, &attr->ring_attr);
		if (status != VXGE_HAL_OK) {
			goto vpath_open_exit7;
		}

		status = __hal_vpath_prc_configure(devh, attr->vp_id);
		if (status != VXGE_HAL_OK) {
			goto vpath_open_exit8;
		}
	}



	vp->vpath->stats_block = __hal_blockpool_block_allocate(devh,
	    VXGE_OS_HOST_PAGE_SIZE);

	if (vp->vpath->stats_block == NULL) {

		status = VXGE_HAL_ERR_OUT_OF_MEMORY;

		goto vpath_open_exit8;

	}

	vp->vpath->hw_stats =
	    (vxge_hal_vpath_stats_hw_info_t *) vp->vpath->stats_block->memblock;

	vxge_os_memzero(vp->vpath->hw_stats,
	    sizeof(vxge_hal_vpath_stats_hw_info_t));

	hldev->stats.hw_dev_info_stats.vpath_info[attr->vp_id] =
	    vp->vpath->hw_stats;

	vp->vpath->hw_stats_sav =
	    &hldev->stats.hw_dev_info_stats.vpath_info_sav[attr->vp_id];

	vxge_os_memzero(vp->vpath->hw_stats_sav,
	    sizeof(vxge_hal_vpath_stats_hw_info_t));

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    vp->vpath->stats_block->dma_addr,
	    &vpath->vp_reg->stats_cfg);

	status = vxge_hal_vpath_hw_stats_enable(vp);

	if (status != VXGE_HAL_OK) {

		goto vpath_open_exit8;

	}

	vxge_list_insert(&vp->item, &vpath->vpath_handles);

	hldev->vpaths_deployed |= mBIT(vpath->vp_id);
	*vpath_handle = vp;

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);

vpath_open_exit8:
	if (vpath->ringh != NULL)
		__hal_ring_delete(vp);
vpath_open_exit7:
	if (vpath->fifoh != NULL)
		__hal_fifo_delete(vp);
vpath_open_exit6:

	vxge_os_free(hldev->header.pdev, vp,
	    sizeof(__hal_vpath_handle_t));
vpath_open_exit2:
	__hal_vp_terminate(devh, attr->vp_id);
vpath_open_exit1:
	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_id - Get virtual path ID
 * @vpath_handle: Handle got from previous vpath open
 *
 * This function returns virtual path id
 */
u32
vxge_hal_vpath_id(
    vxge_hal_vpath_h vpath_handle)
{
	u32 id;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	id = ((__hal_vpath_handle_t *) vpath_handle)->vpath->vp_id;

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (id);
}

/*
 * vxge_hal_vpath_close - Close the handle got from previous vpath (vpath) open
 * @vpath_handle: Handle got from previous vpath open
 *
 * This function is used to close access to virtual path opened
 * earlier.
 */
vxge_hal_status_e
vxge_hal_vpath_close(
    vxge_hal_vpath_h vpath_handle)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_virtualpath_t *vpath;
	__hal_device_t *hldev;
	u32 vp_id;
	u32 is_empty = TRUE;

	vxge_assert(vpath_handle != NULL);

	vpath = (__hal_virtualpath_t *) vp->vpath;

	hldev = (__hal_device_t *) vpath->hldev;

	vp_id = vpath->vp_id;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath(
	    "vpath_handle = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

#if defined(VXGE_HAL_VP_CBS)
	vxge_os_spin_lock(&vpath->vpath_handles_lock);
#elif defined(VXGE_HAL_VP_CBS_IRQ)
	vxge_os_spin_lock_irq(&vpath->vpath_handles_lock, flags);
#endif

	vxge_list_remove(&vp->item);

	if (!vxge_list_is_empty(&vpath->vpath_handles)) {
		vxge_list_insert(&vp->item, &vpath->vpath_handles);
		is_empty = FALSE;
	}

#if defined(VXGE_HAL_VP_CBS)
	vxge_os_spin_unlock(&vpath->vpath_handles_lock);
#elif defined(VXGE_HAL_VP_CBS_IRQ)
	vxge_os_spin_unlock_irq(&vpath->vpath_handles_lock, flags);
#endif

	if (!is_empty) {
		vxge_hal_err_log_vpath("clients are still attached == > %s : %d",
		    __func__, __LINE__);
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 1",
		    __FILE__, __func__, __LINE__);
		return (VXGE_HAL_FAIL);
	}

	vpath->hldev->vpaths_deployed &= ~mBIT(vp_id);

	if (vpath->ringh != NULL)
		__hal_ring_delete(vpath_handle);

	if (vpath->fifoh != NULL)
		__hal_fifo_delete(vpath_handle);


	if (vpath->stats_block != NULL) {
		__hal_blockpool_block_free(hldev, vpath->stats_block);
	}

	vxge_os_free(hldev->header.pdev,
	    vpath_handle, sizeof(__hal_vpath_handle_t));

	__hal_vp_terminate(hldev, vp_id);

	vpath->vp_open = VXGE_HAL_VP_NOT_OPEN;

	(void) __hal_ifmsg_wmsg_post(hldev,
	    vp_id,
	    VXGE_HAL_RTS_ACCESS_STEER_MSG_DEST_BROADCAST,
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_MSG_TYPE_VPATH_RESET_END,
	    0);

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_reset - Resets vpath
 * @vpath_handle: Handle got from previous vpath open
 *
 * This function is used to request a reset of vpath
 */
vxge_hal_status_e
vxge_hal_vpath_reset(
    vxge_hal_vpath_h vpath_handle)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	vxge_hal_status_e status;
	u32 count = 0, total_count = 0;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vp->vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	vxge_hw_vpath_set_zero_rx_frm_len(hldev, vp->vpath->vp_id);

	vxge_hw_vpath_wait_receive_idle(hldev, vp->vpath->vp_id,
	    &count, &total_count);

	status = __hal_vpath_hw_reset((vxge_hal_device_h) hldev,
	    vp->vpath->vp_id);

	if (status == VXGE_HAL_OK)
		vp->vpath->sw_stats->soft_reset_cnt++;

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_reset_poll - Poll for reset complete
 * @vpath_handle: Handle got from previous vpath open
 *
 * This function is used to poll for the vpath reset completion
 */
vxge_hal_status_e
vxge_hal_vpath_reset_poll(
    vxge_hal_vpath_h vpath_handle)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_vpath("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_vpath("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	if (vp->vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	status = __hal_vpath_reset_check(vp->vpath);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d Result = %d",
		    __FILE__, __func__, __LINE__, status);

		return (status);
	}

	status = __hal_vpath_sw_reset((vxge_hal_device_h) hldev,
	    vp->vpath->vp_id);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	vxge_os_memzero(vp->vpath->sw_stats,
	    sizeof(vxge_hal_vpath_stats_sw_info_t));

	status = __hal_vpath_hw_initialize((vxge_hal_device_h) hldev,
	    vp->vpath->vp_id);

	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	if (vp->vpath->ringh != NULL) {

		status = __hal_vpath_prc_configure(
		    (vxge_hal_device_h) hldev,
		    vp->vpath->vp_id);

		if (status != VXGE_HAL_OK) {
			vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
			    __FILE__, __func__, __LINE__, status);
			return (status);
		}
	}

	vxge_os_memzero(vp->vpath->hw_stats,
	    sizeof(vxge_hal_vpath_stats_hw_info_t));

	vxge_os_memzero(vp->vpath->hw_stats_sav,
	    sizeof(vxge_hal_vpath_stats_hw_info_t));

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    vp->vpath->stats_block->dma_addr,
	    &vp->vpath->vp_reg->stats_cfg);


	status = vxge_hal_vpath_hw_stats_enable(vp);

	if (status != VXGE_HAL_OK) {

		vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);

	}

	vxge_hal_trace_log_vpath("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_hw_stats_enable - Enable vpath h/wstatistics.
 * @vpath_handle: Virtual Path handle.
 *
 * Enable the DMA vpath statistics. The function is to be called to re-enable
 * the adapter to update stats into the host memory
 *
 * See also: vxge_hal_vpath_hw_stats_disable(), vxge_hal_vpath_hw_stats_get()
 */
vxge_hal_status_e
vxge_hal_vpath_hw_stats_enable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	vpath = vp->vpath;

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	vxge_os_memcpy(vpath->hw_stats_sav,
	    vpath->hw_stats,
	    sizeof(vxge_hal_vpath_stats_hw_info_t));

	if (hldev->header.config.stats_read_method ==
	    VXGE_HAL_STATS_READ_METHOD_DMA) {
		val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &hldev->common_reg->stats_cfg0);

		val64 |= VXGE_HAL_STATS_CFG0_STATS_ENABLE(
		    (1 << (16 - vpath->vp_id)));

		vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
		    hldev->header.regh0,
		    (u32) bVAL32(val64, 0),
		    &hldev->common_reg->stats_cfg0);
	} else {
		status = __hal_vpath_hw_stats_get(
		    vpath,
		    vpath->hw_stats);
	}

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_hw_stats_disable - Disable vpath h/w statistics.
 * @vpath_handle: Virtual Path handle.
 *
 * Enable the DMA vpath statistics. The function is to be called to disable
 * the adapter to update stats into the host memory. This function is not
 * needed to be called, normally.
 *
 * See also: vxge_hal_vpath_hw_stats_enable(), vxge_hal_vpath_hw_stats_get()
 */
vxge_hal_status_e
vxge_hal_vpath_hw_stats_disable(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "vpath = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle);

	vpath = (__hal_virtualpath_t *)
	    ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->stats_cfg0);

	val64 &= ~VXGE_HAL_STATS_CFG0_STATS_ENABLE((1 << (16 - vpath->vp_id)));

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &hldev->common_reg->stats_cfg0);

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_hw_stats_get - Get the vpath hw statistics.
 * @vpath_handle: Virtual Path handle.
 * @hw_stats: Hardware stats
 *
 * Returns the vpath h/w stats.
 *
 * See also: vxge_hal_vpath_hw_stats_enable(),
 * vxge_hal_vpath_hw_stats_disable()
 */
vxge_hal_status_e
vxge_hal_vpath_hw_stats_get(vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_stats_hw_info_t *hw_stats)
{
	__hal_virtualpath_t *vpath;
	__hal_device_t *hldev;
	vxge_hal_status_e status;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (hw_stats != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "vpath = 0x"VXGE_OS_STXFMT", hw_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) hw_stats);

	vpath = (__hal_virtualpath_t *)
	    ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &hldev->common_reg->stats_cfg0,
	    0,
	    VXGE_HAL_STATS_CFG0_STATS_ENABLE((1 << (16 - vpath->vp_id))),
	    hldev->header.config.device_poll_millis);

	if (status == VXGE_HAL_OK) {
		vxge_os_memcpy(hw_stats,
		    vpath->hw_stats,
		    sizeof(vxge_hal_vpath_stats_hw_info_t));
	}

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_sw_stats_get - Get the vpath sw statistics.
 * @vpath_handle: Virtual Path handle.
 * @sw_stats: Software stats
 *
 * Returns the vpath s/w stats.
 *
 * See also: vxge_hal_vpath_hw_stats_get()
 */
vxge_hal_status_e
vxge_hal_vpath_sw_stats_get(vxge_hal_vpath_h vpath_handle,
    vxge_hal_vpath_stats_sw_info_t *sw_stats)
{
	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert((vpath_handle != NULL) && (sw_stats != NULL));

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "vpath = 0x"VXGE_OS_STXFMT", sw_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle, (ptr_t) sw_stats);

	vpath = (__hal_virtualpath_t *)
	    ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);

		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	if (hldev->header.traffic_intr_cnt) {
		int intrcnt = hldev->header.traffic_intr_cnt;

		if (!intrcnt)
			intrcnt = 1;

		vpath->sw_stats->fifo_stats.common_stats.avg_compl_per_intr_cnt =
		    vpath->sw_stats->fifo_stats.common_stats.total_compl_cnt / intrcnt;

		if (vpath->sw_stats->fifo_stats.common_stats.avg_compl_per_intr_cnt ==
		    0) {
			/* to not confuse user */
			vpath->sw_stats->fifo_stats.common_stats.avg_compl_per_intr_cnt = 1;
		}

		vpath->sw_stats->ring_stats.common_stats.avg_compl_per_intr_cnt =
		    vpath->sw_stats->ring_stats.common_stats.total_compl_cnt / intrcnt;

		if (vpath->sw_stats->ring_stats.common_stats.avg_compl_per_intr_cnt ==
		    0) {
			/* to not confuse user */
			vpath->sw_stats->ring_stats.common_stats.avg_compl_per_intr_cnt = 1;
		}
	}

	if (vpath->sw_stats->fifo_stats.total_posts) {
		vpath->sw_stats->fifo_stats.avg_buffers_per_post =
		    vpath->sw_stats->fifo_stats.total_buffers /
		    vpath->sw_stats->fifo_stats.total_posts;

		vpath->sw_stats->fifo_stats.avg_post_size =
		    (u32) (vpath->hw_stats->tx_stats.tx_ttl_eth_octets /
		    vpath->sw_stats->fifo_stats.total_posts);
	}

	if (vpath->sw_stats->fifo_stats.total_buffers) {
		vpath->sw_stats->fifo_stats.avg_buffer_size =
		    (u32) (vpath->hw_stats->tx_stats.tx_ttl_eth_octets /
		    vpath->sw_stats->fifo_stats.total_buffers);
	}

	vxge_os_memcpy(sw_stats,
	    vpath->sw_stats,
	    sizeof(vxge_hal_vpath_stats_sw_info_t));

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_stats_access - Get the statistics from the given location
 *			  and offset and perform an operation
 * @vpath: Virtual path.
 * @operation: Operation to be performed
 * @location: Location (one of vpath id, aggregate or port)
 * @offset: Offset with in the location
 * @stat: Pointer to a buffer to return the value
 *
 * Get the statistics from the given location and offset.
 *
 */
vxge_hal_status_e
__hal_vpath_stats_access(
    __hal_virtualpath_t *vpath,
    u32 operation,
    u32 offset,
    u64 *stat)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);

		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	val64 = VXGE_HAL_XMAC_STATS_ACCESS_CMD_OP(operation) |
	    VXGE_HAL_XMAC_STATS_ACCESS_CMD_STROBE |
	    VXGE_HAL_XMAC_STATS_ACCESS_CMD_OFFSET_SEL(offset);

	vxge_hal_pio_mem_write32_lower(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 32),
	    &vpath->vp_reg->xmac_stats_access_cmd);
	vxge_os_wmb();

	vxge_hal_pio_mem_write32_upper(hldev->header.pdev,
	    hldev->header.regh0,
	    (u32) bVAL32(val64, 0),
	    &vpath->vp_reg->xmac_stats_access_cmd);
	vxge_os_wmb();

	status = vxge_hal_device_register_poll(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->xmac_stats_access_cmd,
	    0,
	    VXGE_HAL_XMAC_STATS_ACCESS_CMD_STROBE,
	    hldev->header.config.device_poll_millis);

	if ((status == VXGE_HAL_OK) && (operation == VXGE_HAL_STATS_OP_READ)) {

		*stat = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vpath->vp_reg->xmac_stats_access_data);

	} else {
		*stat = 0;
	}

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);
	return (status);
}

/*
 * vxge_hal_vpath_stats_access
 * Get statistics from given location and offset to perform an operation
 * @vpath_handle: Virtual path handle.
 * @operation: Operation to be performed
 * @offset: Offset with in the location
 * @stat: Pointer to a buffer to return the value
 *
 * Get the statistics from the given location and offset.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_stats_access(
    vxge_hal_vpath_h vpath_handle,
    u32 operation,
    u32 offset,
    u64 *stat)
{
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	vxge_hal_status_e status;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "vpath_handle = 0x"VXGE_OS_STXFMT, (ptr_t) vpath_handle);

	status = __hal_vpath_stats_access(vp->vpath,
	    operation,
	    offset,
	    stat);

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_xmac_tx_stats_get - Get the TX Statistics of a vpath
 * @vpath: vpath
 * @vpath_tx_stats: Buffer to return TX Statistics of vpath.
 *
 * Get the TX Statistics of a vpath
 *
 */
vxge_hal_status_e
__hal_vpath_xmac_tx_stats_get(__hal_virtualpath_t *vpath,
    vxge_hal_xmac_vpath_tx_stats_t *vpath_tx_stats)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__,
		    __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_TTL_ETH_FRMS_OFFSET);

	vpath_tx_stats->tx_ttl_eth_frms =
	    VXGE_HAL_STATS_GET_VPATH_TX_TTL_ETH_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_TTL_ETH_OCTETS_OFFSET);

	vpath_tx_stats->tx_ttl_eth_octets =
	    VXGE_HAL_STATS_GET_VPATH_TX_TTL_ETH_OCTETS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_DATA_OCTETS_OFFSET);

	vpath_tx_stats->tx_data_octets =
	    VXGE_HAL_STATS_GET_VPATH_TX_DATA_OCTETS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_MCAST_FRMS_OFFSET);

	vpath_tx_stats->tx_mcast_frms =
	    VXGE_HAL_STATS_GET_VPATH_TX_MCAST_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_BCAST_FRMS_OFFSET);

	vpath_tx_stats->tx_bcast_frms =
	    VXGE_HAL_STATS_GET_VPATH_TX_BCAST_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_UCAST_FRMS_OFFSET);

	vpath_tx_stats->tx_ucast_frms =
	    VXGE_HAL_STATS_GET_VPATH_TX_UCAST_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_TAGGED_FRMS_OFFSET);

	vpath_tx_stats->tx_tagged_frms =
	    VXGE_HAL_STATS_GET_VPATH_TX_TAGGED_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_TX_VLD_IP_OFFSET);

	vpath_tx_stats->tx_vld_ip =
	    VXGE_HAL_STATS_GET_VPATH_TX_VLD_IP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_VLD_IP_OCTETS_OFFSET);

	vpath_tx_stats->tx_vld_ip_octets =
	    VXGE_HAL_STATS_GET_VPATH_TX_VLD_IP_OCTETS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_TX_ICMP_OFFSET);

	vpath_tx_stats->tx_icmp =
	    VXGE_HAL_STATS_GET_VPATH_TX_ICMP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_TX_TCP_OFFSET);

	vpath_tx_stats->tx_tcp =
	    VXGE_HAL_STATS_GET_VPATH_TX_TCP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_RST_TCP_OFFSET);

	vpath_tx_stats->tx_rst_tcp =
	    VXGE_HAL_STATS_GET_VPATH_TX_RST_TCP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_UDP_OFFSET);

	vpath_tx_stats->tx_udp =
	    VXGE_HAL_STATS_GET_VPATH_TX_UDP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_LOST_IP_OFFSET);

	vpath_tx_stats->tx_lost_ip =
	    (u32) VXGE_HAL_STATS_GET_VPATH_TX_LOST_IP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_UNKNOWN_PROTOCOL_OFFSET);

	vpath_tx_stats->tx_unknown_protocol =
	    (u32) VXGE_HAL_STATS_GET_VPATH_TX_UNKNOWN_PROTOCOL(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_PARSE_ERROR_OFFSET);

	vpath_tx_stats->tx_parse_error =
	    (u32) VXGE_HAL_STATS_GET_VPATH_TX_PARSE_ERROR(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_TCP_OFFLOAD_OFFSET);

	vpath_tx_stats->tx_tcp_offload =
	    VXGE_HAL_STATS_GET_VPATH_TX_TCP_OFFLOAD(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_RETX_TCP_OFFLOAD_OFFSET);

	vpath_tx_stats->tx_retx_tcp_offload =
	    VXGE_HAL_STATS_GET_VPATH_TX_RETX_TCP_OFFLOAD(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_TX_LOST_IP_OFFLOAD_OFFSET);

	vpath_tx_stats->tx_lost_ip_offload =
	    VXGE_HAL_STATS_GET_VPATH_TX_LOST_IP_OFFLOAD(val64);

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * __hal_vpath_xmac_rx_stats_get - Get the RX Statistics of a vpath
 * @vpath: vpath
 * @vpath_rx_stats: Buffer to return RX Statistics of vpath.
 *
 * Get the RX Statistics of a vpath
 *
 */
vxge_hal_status_e
__hal_vpath_xmac_rx_stats_get(__hal_virtualpath_t *vpath,
    vxge_hal_xmac_vpath_rx_stats_t *vpath_rx_stats)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert(vpath != NULL);

	hldev = vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__,
		    VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_ETH_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_eth_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_ETH_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_VLD_FRMS_OFFSET);

	vpath_rx_stats->rx_vld_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_VLD_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_OFFLOAD_FRMS_OFFSET);

	vpath_rx_stats->rx_offload_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_OFFLOAD_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_ETH_OCTETS_OFFSET);

	vpath_rx_stats->rx_ttl_eth_octets =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_ETH_OCTETS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_DATA_OCTETS_OFFSET);

	vpath_rx_stats->rx_data_octets =
	    VXGE_HAL_STATS_GET_VPATH_RX_DATA_OCTETS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_OFFLOAD_OCTETS_OFFSET);

	vpath_rx_stats->rx_offload_octets =
	    VXGE_HAL_STATS_GET_VPATH_RX_OFFLOAD_OCTETS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_VLD_MCAST_FRMS_OFFSET);

	vpath_rx_stats->rx_vld_mcast_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_VLD_MCAST_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_VLD_BCAST_FRMS_OFFSET);

	vpath_rx_stats->rx_vld_bcast_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_VLD_BCAST_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_ACC_UCAST_FRMS_OFFSET);

	vpath_rx_stats->rx_accepted_ucast_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_ACC_UCAST_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_ACC_NUCAST_FRMS_OFFSET);

	vpath_rx_stats->rx_accepted_nucast_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_ACC_NUCAST_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TAGGED_FRMS_OFFSET);

	vpath_rx_stats->rx_tagged_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TAGGED_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_LONG_FRMS_OFFSET);

	vpath_rx_stats->rx_long_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_LONG_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_USIZED_FRMS_OFFSET);

	vpath_rx_stats->rx_usized_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_USIZED_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_OSIZED_FRMS_OFFSET);

	vpath_rx_stats->rx_osized_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_OSIZED_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_FRAG_FRMS_OFFSET);

	vpath_rx_stats->rx_frag_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_FRAG_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_JABBER_FRMS_OFFSET);

	vpath_rx_stats->rx_jabber_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_JABBER_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_64_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_64_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_64_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_65_127_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_65_127_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_65_127_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_128_255_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_128_255_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_128_255_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_256_511_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_256_511_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_256_511_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_512_1023_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_512_1023_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_512_1023_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_1024_1518_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_1024_1518_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_1024_1518_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_1519_4095_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_1519_4095_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_1519_4095_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_4096_8191_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_4096_8191_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_4096_8191_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_8192_MAX_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_8192_max_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_8192_MAX_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_TTL_GT_MAX_FRMS_OFFSET);

	vpath_rx_stats->rx_ttl_gt_max_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_TTL_GT_MAX_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_IP_OFFSET);

	vpath_rx_stats->rx_ip =
	    VXGE_HAL_STATS_GET_VPATH_RX_IP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_ACC_IP_OFFSET);

	vpath_rx_stats->rx_accepted_ip =
	    VXGE_HAL_STATS_GET_VPATH_RX_ACC_IP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_IP_OCTETS_OFFSET);

	vpath_rx_stats->rx_ip_octets =
	    VXGE_HAL_STATS_GET_VPATH_RX_IP_OCTETS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_ERR_IP_OFFSET);

	vpath_rx_stats->rx_err_ip =
	    VXGE_HAL_STATS_GET_VPATH_RX_ERR_IP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_ICMP_OFFSET);

	vpath_rx_stats->rx_icmp =
	    VXGE_HAL_STATS_GET_VPATH_RX_ICMP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_TCP_OFFSET);

	vpath_rx_stats->rx_tcp =
	    VXGE_HAL_STATS_GET_VPATH_RX_TCP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_UDP_OFFSET);

	vpath_rx_stats->rx_udp =
	    VXGE_HAL_STATS_GET_VPATH_RX_UDP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_ERR_TCP_OFFSET);

	vpath_rx_stats->rx_err_tcp =
	    VXGE_HAL_STATS_GET_VPATH_RX_ERR_TCP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_LOST_FRMS_OFFSET);

	vpath_rx_stats->rx_lost_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_LOST_FRMS(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(VXGE_HAL_STATS_VPATH_RX_LOST_IP_OFFSET);

	vpath_rx_stats->rx_lost_ip =
	    VXGE_HAL_STATS_GET_VPATH_RX_LOST_IP(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_LOST_IP_OFFLOAD_OFFSET);

	vpath_rx_stats->rx_lost_ip_offload =
	    VXGE_HAL_STATS_GET_VPATH_RX_LOST_IP_OFFLOAD(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_QUEUE_FULL_DISCARD_OFFSET);

	vpath_rx_stats->rx_queue_full_discard =
	    (u16) VXGE_HAL_STATS_GET_VPATH_RX_QUEUE_FULL_DISCARD(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_RED_DISCARD_OFFSET);

	vpath_rx_stats->rx_red_discard =
	    (u16) VXGE_HAL_STATS_GET_VPATH_RX_RED_DISCARD(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_SLEEP_DISCARD_OFFSET);

	vpath_rx_stats->rx_sleep_discard =
	    (u16) VXGE_HAL_STATS_GET_VPATH_RX_SLEEP_DISCARD(val64);

	vpath_rx_stats->rx_various_discard =
	    vpath_rx_stats->rx_queue_full_discard;

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_RX_MPA_OK_FRMS_OFFSET);

	vpath_rx_stats->rx_mpa_ok_frms =
	    VXGE_HAL_STATS_GET_VPATH_RX_MPA_OK_FRMS(val64);

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: 0",
	    __FILE__, __func__, __LINE__);
	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_vpath_xmac_tx_stats_get - Get the TX Statistics of a vpath
 * @vpath_handle: vpath handle.
 * @vpath_tx_stats: Buffer to return TX Statistics of vpath.
 *
 * Get the TX Statistics of a vpath
 *
 */
vxge_hal_status_e
vxge_hal_vpath_xmac_tx_stats_get(vxge_hal_vpath_h vpath_handle,
    vxge_hal_xmac_vpath_tx_stats_t *vpath_tx_stats)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	status = __hal_vpath_xmac_tx_stats_get(vp->vpath, vpath_tx_stats);

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_xmac_rx_stats_get - Get the RX Statistics of a vpath
 * @vpath_handle: vpath handle.
 * @vpath_rx_stats: Buffer to return RX Statistics of vpath.
 *
 * Get the RX Statistics of a vpath
 *
 */
vxge_hal_status_e
vxge_hal_vpath_xmac_rx_stats_get(vxge_hal_vpath_h vpath_handle,
    vxge_hal_xmac_vpath_rx_stats_t *vpath_rx_stats)
{
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert(vpath_handle != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("vpath_handle = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	status = __hal_vpath_xmac_rx_stats_get(vp->vpath, vpath_rx_stats);

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * __hal_vpath_hw_stats_get - Get the vpath hw statistics.
 * @vpath: Virtual Path.
 * @hw_stats: Hardware stats
 *
 * Returns the vpath h/w stats.
 *
 * See also: vxge_hal_vpath_hw_stats_enable(),
 * vxge_hal_vpath_hw_stats_disable()
 */
vxge_hal_status_e
__hal_vpath_hw_stats_get(__hal_virtualpath_t *vpath,
    vxge_hal_vpath_stats_hw_info_t *hw_stats)
{
	u64 val64;
	__hal_device_t *hldev;
	vxge_hal_status_e status;

	vxge_assert((vpath != NULL) && (hw_stats != NULL));

	hldev = vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats(
	    "vpath = 0x"VXGE_OS_STXFMT", hw_stats = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath, (ptr_t) hw_stats);

	if (vpath->vp_open == VXGE_HAL_VP_NOT_OPEN) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, VXGE_HAL_ERR_VPATH_NOT_OPEN);
		return (VXGE_HAL_ERR_VPATH_NOT_OPEN);
	}

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_debug_stats0);

	hw_stats->ini_num_mwr_sent =
	    (u32) VXGE_HAL_VPATH_DEBUG_STATS0_GET_INI_NUM_MWR_SENT(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_debug_stats1);

	hw_stats->ini_num_mrd_sent =
	    (u32) VXGE_HAL_VPATH_DEBUG_STATS1_GET_INI_NUM_MRD_SENT(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_debug_stats2);

	hw_stats->ini_num_cpl_rcvd =
	    (u32) VXGE_HAL_VPATH_DEBUG_STATS2_GET_INI_NUM_CPL_RCVD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_debug_stats3);

	hw_stats->ini_num_mwr_byte_sent =
	    VXGE_HAL_VPATH_DEBUG_STATS3_GET_INI_NUM_MWR_BYTE_SENT(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_debug_stats4);

	hw_stats->ini_num_cpl_byte_rcvd =
	    VXGE_HAL_VPATH_DEBUG_STATS4_GET_INI_NUM_CPL_BYTE_RCVD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_debug_stats5);

	hw_stats->wrcrdtarb_xoff =
	    (u32) VXGE_HAL_VPATH_DEBUG_STATS5_GET_WRCRDTARB_XOFF(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_debug_stats6);

	hw_stats->rdcrdtarb_xoff =
	    (u32) VXGE_HAL_VPATH_DEBUG_STATS6_GET_RDCRDTARB_XOFF(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_genstats_count01);

	hw_stats->vpath_genstats_count0 =
	    (u32) VXGE_HAL_VPATH_GENSTATS_COUNT01_GET_PPIF_VPATH_GENSTATS_COUNT0(
	    val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_genstats_count01);

	hw_stats->vpath_genstats_count1 =
	    (u32) VXGE_HAL_VPATH_GENSTATS_COUNT01_GET_PPIF_VPATH_GENSTATS_COUNT1(
	    val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_genstats_count23);

	hw_stats->vpath_genstats_count2 =
	    (u32) VXGE_HAL_VPATH_GENSTATS_COUNT23_GET_PPIF_VPATH_GENSTATS_COUNT2(
	    val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_genstats_count01);

	hw_stats->vpath_genstats_count3 =
	    (u32) VXGE_HAL_VPATH_GENSTATS_COUNT23_GET_PPIF_VPATH_GENSTATS_COUNT3(
	    val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_genstats_count4);

	hw_stats->vpath_genstats_count4 =
	    (u32) VXGE_HAL_VPATH_GENSTATS_COUNT4_GET_PPIF_VPATH_GENSTATS_COUNT4(
	    val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->vpath_genstats_count5);

	hw_stats->vpath_genstats_count5 = (u32)
	    VXGE_HAL_VPATH_GENSTATS_COUNT5_GET_PPIF_VPATH_GENSTATS_COUNT5(
	    val64);

	status = __hal_vpath_xmac_tx_stats_get(vpath, &hw_stats->tx_stats);
	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	status = __hal_vpath_xmac_rx_stats_get(vpath, &hw_stats->rx_stats);
	if (status != VXGE_HAL_OK) {
		vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
		    __FILE__, __func__, __LINE__, status);
		return (status);
	}

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_PROG_EVENT_VNUM0_OFFSET);

	hw_stats->prog_event_vnum0 =
	    (u32) VXGE_HAL_STATS_GET_VPATH_PROG_EVENT_VNUM0(val64);

	hw_stats->prog_event_vnum1 =
	    (u32) VXGE_HAL_STATS_GET_VPATH_PROG_EVENT_VNUM1(val64);

	VXGE_HAL_VPATH_STATS_PIO_READ(
	    VXGE_HAL_STATS_VPATH_PROG_EVENT_VNUM2_OFFSET);

	hw_stats->prog_event_vnum2 =
	    (u32) VXGE_HAL_STATS_GET_VPATH_PROG_EVENT_VNUM2(val64);

	hw_stats->prog_event_vnum3 =
	    (u32) VXGE_HAL_STATS_GET_VPATH_PROG_EVENT_VNUM3(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rx_multi_cast_stats);

	hw_stats->rx_multi_cast_frame_discard =
	    (u16) VXGE_HAL_RX_MULTI_CAST_STATS_GET_FRAME_DISCARD(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rx_frm_transferred);

	hw_stats->rx_frm_transferred =
	    (u32) VXGE_HAL_RX_FRM_TRANSFERRED_GET_RX_FRM_TRANSFERRED(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->rxd_returned);

	hw_stats->rxd_returned =
	    (u16) VXGE_HAL_RXD_RETURNED_GET_RXD_RETURNED(val64);


	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->dbg_stats_rx_mpa);

	hw_stats->rx_mpa_len_fail_frms =
	    (u16) VXGE_HAL_DBG_STATS_GET_RX_MPA_LEN_FAIL_FRMS(val64);
	hw_stats->rx_mpa_mrk_fail_frms =
	    (u16) VXGE_HAL_DBG_STATS_GET_RX_MPA_MRK_FAIL_FRMS(val64);
	hw_stats->rx_mpa_crc_fail_frms =
	    (u16) VXGE_HAL_DBG_STATS_GET_RX_MPA_CRC_FAIL_FRMS(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->dbg_stats_rx_fau);

	hw_stats->rx_permitted_frms =
	    (u16) VXGE_HAL_DBG_STATS_GET_RX_FAU_RX_PERMITTED_FRMS(val64);
	hw_stats->rx_vp_reset_discarded_frms = (u16)
	    VXGE_HAL_DBG_STATS_GET_RX_FAU_RX_VP_RESET_DISCARDED_FRMS(val64);
	hw_stats->rx_wol_frms =
	    (u16) VXGE_HAL_DBG_STATS_GET_RX_FAU_RX_WOL_FRMS(val64);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vpath->vp_reg->tx_vp_reset_discarded_frms);

	hw_stats->tx_vp_reset_discarded_frms = (u16)
	    VXGE_HAL_TX_VP_RESET_DISCARDED_FRMS_GET_TX_VP_RESET_DISCARDED_FRMS(
	    val64);

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}

/*
 * vxge_hal_vpath_stats_clear - Clear all the statistics of vpath
 * @vpath_handle: Virtual path handle.
 *
 * Clear the statistics of the given vpath.
 *
 */
vxge_hal_status_e
vxge_hal_vpath_stats_clear(vxge_hal_vpath_h vpath_handle)
{
	u64 stat;
	vxge_hal_status_e status;
	__hal_device_t *hldev;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	vxge_assert(vp != NULL);

	hldev = vp->vpath->hldev;

	vxge_hal_trace_log_stats("==> %s:%s:%d",
	    __FILE__, __func__, __LINE__);

	vxge_hal_trace_log_stats("vpath = 0x"VXGE_OS_STXFMT,
	    (ptr_t) vpath_handle);

	vxge_os_memcpy(vp->vpath->hw_stats_sav,
	    vp->vpath->hw_stats,
	    sizeof(vxge_hal_vpath_stats_hw_info_t));

	vxge_os_memzero(vp->vpath->hw_stats,
	    sizeof(vxge_hal_vpath_stats_hw_info_t));

	vxge_os_memzero(vp->vpath->sw_stats,
	    sizeof(vxge_hal_vpath_stats_sw_info_t));

	status = vxge_hal_vpath_stats_access(
	    vpath_handle,
	    VXGE_HAL_STATS_OP_CLEAR_ALL_VPATH_STATS,
	    0,
	    &stat);

	vxge_hal_trace_log_stats("<== %s:%s:%d  Result: %d",
	    __FILE__, __func__, __LINE__, status);

	return (status);
}


/*
 * vxge_hal_set_fw_api - Setup FW api
 * @devh: Device Handle.
 *
 */
vxge_hal_status_e
vxge_hal_set_fw_api(vxge_hal_device_h devh,
    u64 vp_id, u32 action, u32 offset,
    u64 data0, u64 data1)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	u64 val64;
	u32 fw_memo = VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL_FW_MEMO;

	vxge_hal_vpath_reg_t *vp_reg;

	__hal_device_t *hldev = (__hal_device_t *) devh;
	vxge_assert(hldev != NULL);

	/* Assumption: Privileged vpath is zero */
	vp_reg = hldev->vpath_reg[vp_id];

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0, data0,
	    &vp_reg->rts_access_steer_data0);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0, data1,
	    &vp_reg->rts_access_steer_data1);

	vxge_os_wmb();

	val64 = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION(action) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_DATA_STRUCT_SEL(fw_memo) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_OFFSET(offset) |
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE;

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0, val64,
	    &vp_reg->rts_access_steer_ctrl);

	vxge_os_wmb();

	status =
	    vxge_hal_device_register_poll(
	    hldev->header.pdev, hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl, 0,
	    VXGE_HAL_RTS_ACCESS_STEER_CTRL_STROBE,
	    WAIT_FACTOR * hldev->header.config.device_poll_millis);

	if (status != VXGE_HAL_OK)
		return (VXGE_HAL_FAIL);

	val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_ctrl);

	if (val64 & VXGE_HAL_RTS_ACCESS_STEER_CTRL_RMACJ_STATUS)
		status = VXGE_HAL_OK;
	else
		status = VXGE_HAL_FAIL;

	return (status);
}

/*
 * vxge_hal_get_active_config - Get active configuration
 * @devh: Device Handle.
 *
 */
vxge_hal_status_e
vxge_hal_get_active_config(vxge_hal_device_h devh,
    vxge_hal_xmac_nwif_actconfig req_config,
    u64 *cur_config)
{
	u32 action;
	u64 data0 = 0x0, data1 = 0x0;
	u32 cmd = VXGE_HAL_XMAC_NWIF_Cmd_Get_Active_Config;

	vxge_hal_vpath_reg_t *vp_reg;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_assert(hldev != NULL);

	/* Assumption: Privileged vpath is zero */
	vp_reg = hldev->vpath_reg[0];

	/* get port mode */
	data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_SET_NWIF_CMD(cmd) | req_config;
	action = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_PORT_CTRL;

	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, data1);
	if (status == VXGE_HAL_OK) {
		*cur_config = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vp_reg->rts_access_steer_data1);
	}

	return (status);
}

/*
 * vxge_hal_set_port_mode - Set dual port mode
 * override the default dual port mode
 * @devh: Device Handle.
 *
 */
vxge_hal_status_e
vxge_hal_set_port_mode(vxge_hal_device_h devh,
    vxge_hal_xmac_nwif_dp_mode port_mode)
{
	u32 action;
	u64 data0 = 0x0, data1 = 0x0;
	u32 cmd = VXGE_HAL_XMAC_NWIF_Cmd_SetMode;

	vxge_hal_status_e status = VXGE_HAL_OK;

	if ((port_mode < VXGE_HAL_DP_NP_MODE_DEFAULT) ||
	    (port_mode > VXGE_HAL_DP_NP_MODE_DISABLE_PORT_MGMT)) {

		vxge_os_printf("Invalid port mode : %d\n", port_mode);
		return (VXGE_HAL_ERR_INVALID_DP_MODE);
	}

	data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_SET_NWIF_CMD(cmd);
	data1 = port_mode;
	action = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_PORT_CTRL;

	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, data1);

	return (status);
}

/*
 * vxge_hal_set_port_mode - Set dual port mode
 * change behavior on failure *
 * @devh: Device Handle.
 */
vxge_hal_status_e
vxge_hal_set_behavior_on_failure(vxge_hal_device_h devh,
    vxge_hal_xmac_nwif_behavior_on_failure behave_on_failure)
{
	u32 action;
	u64 data0 = 0x0, data1 = 0x0;
	u32 cmd = VXGE_HAL_XMAC_NWIF_Cmd_CfgSetBehaviourOnFailure;
	vxge_hal_status_e status = VXGE_HAL_OK;

	if ((behave_on_failure < VXGE_HAL_XMAC_NWIF_OnFailure_NoMove) ||
	    (behave_on_failure >
	    VXGE_HAL_XMAC_NWIF_OnFailure_OtherPortBackOnRestore)) {
		vxge_os_printf("Invalid setting for failure behavior : %d\n",
		    behave_on_failure);

		return (VXGE_HAL_FAIL);
	}

	data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_SET_NWIF_CMD(cmd);
	data1 = behave_on_failure;
	action = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_PORT_CTRL;

	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, data1);

	return (status);
}

vxge_hal_status_e
vxge_hal_set_l2switch_mode(vxge_hal_device_h devh,
    enum vxge_hal_xmac_nwif_l2_switch_status l2_switch)
{
	u32 action;
	u64 data0 = 0x0, data1 = 0x0;
	u32 cmd = VXGE_HAL_XMAC_NWIF_Cmd_CfgDualPort_L2SwitchEnable;

	vxge_hal_status_e status = VXGE_HAL_OK;

	if ((l2_switch < VXGE_HAL_XMAC_NWIF_L2_SWITCH_DISABLE) ||
	    (l2_switch > VXGE_HAL_XMAC_NWIF_L2_SWITCH_ENABLE)) {
		vxge_os_printf("Invalid setting for failure behavior : %d\n",
		    l2_switch);

		return (VXGE_HAL_ERR_INVALID_L2_SWITCH_STATE);
	}

	data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_SET_NWIF_CMD(cmd);
	data1 = l2_switch;
	action = VXGE_HAL_RTS_ACCESS_FW_MEMO_ACTION_PRIV_NWIF;

	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, data1);

	return (status);
}

/* Get function mode */
vxge_hal_status_e
vxge_hal_func_mode_get(vxge_hal_device_h devh, u32 *func_mode)
{
	int vp_id;
	u32 action;
	u64 val64;

	vxge_hal_status_e status = VXGE_HAL_OK;
	vxge_hal_vpath_reg_t *vp_reg;

	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_assert(hldev != NULL);
	/* get the first vpath number assigned to this function */
	vp_id = hldev->first_vp_id;

	vp_reg = hldev->vpath_reg[vp_id];
	action = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_GET_FUNC_MODE;

	status = vxge_hal_set_fw_api(devh, vp_id, action, 0x0, 0x0, 0x0);
	if (status == VXGE_HAL_OK) {
		val64 = vxge_os_pio_mem_read64(
		    hldev->header.pdev, hldev->header.regh0,
		    &vp_reg->rts_access_steer_data0);

		*func_mode =
		    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_FUNC_MODE(val64);
	}

	return (status);
}

vxge_hal_status_e
vxge_hal_func_mode_count(vxge_hal_device_h devh, u32 func_mode, u32 *num_funcs)
{
	int vp_id;
	u32 action;
	u64 val64, data0;

	vxge_hal_vpath_reg_t *vp_reg;
	vxge_hal_status_e status = VXGE_HAL_OK;

	__hal_device_t *hldev = (__hal_device_t *) devh;

	vp_id = hldev->first_vp_id;
	vp_reg = hldev->vpath_reg[0];

	data0 = func_mode;
	action = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_GET_FUNC_COUNT;

	status = vxge_hal_set_fw_api(devh, vp_id, action, 0x0, data0, 0x0);
	if (status == VXGE_HAL_OK) {

		val64 = vxge_os_pio_mem_read64(
		    hldev->header.pdev, hldev->header.regh0,
		    &vp_reg->rts_access_steer_data0);

		*num_funcs = (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA0_GET_NUM_FUNC(val64);
	}

	return (status);
}

vxge_hal_status_e
vxge_hal_config_vpath_map(vxge_hal_device_h devh, u64 port_map)
{
	u32 action;
	u64 data0 = 0x0, data1 = 0x0;
	u32 cmd = VXGE_HAL_XMAC_NWIF_Cmd_CfgDualPort_VPathVector;
	vxge_hal_status_e status = VXGE_HAL_OK;

	action = VXGE_HAL_RTS_ACCESS_FW_MEMO_ACTION_PRIV_NWIF;
	data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_SET_NWIF_CMD(cmd);
	data1 = port_map;

	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, data1);

	return (status);
}

vxge_hal_status_e
vxge_hal_get_vpath_mask(vxge_hal_device_h devh,
    u32 vf_id, u32 * num_vp, u64 * data1)
{
	u32 action, vhn = 0;
	u64 data0 = 0x0;

	vxge_hal_vpath_reg_t *vp_reg;
	vxge_hal_status_e status = VXGE_HAL_OK;

	__hal_device_t *hldev = (__hal_device_t *) devh;
	vp_reg = hldev->vpath_reg[0];

	data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_VFID(vf_id) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_VHN(vhn);

	action = VXGE_HAL_PRIV_VPATH_ACTION;
	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, 0x0);
	if (status == VXGE_HAL_OK) {

		data0 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vp_reg->rts_access_steer_data0);

		*num_vp = (u32) ((data0 >> 16) & 0xFF);
		*data1 = vxge_os_pio_mem_read64(hldev->header.pdev,
		    hldev->header.regh0,
		    &vp_reg->rts_access_steer_data1);
	}

	return (status);
}

vxge_hal_status_e
vxge_hal_get_vpath_list(vxge_hal_device_h devh, u32 vf_id,
    u64 *vpath_list, u32 *vpath_count)
{
	u32 i, j = 0;
	u64 pos, vpath_mask;
	vxge_hal_status_e status = VXGE_HAL_OK;

	*vpath_count = 0;

	status = vxge_hal_get_vpath_mask(devh, vf_id, vpath_count, &vpath_mask);
	if (status == VXGE_HAL_OK) {
		for (i = VXGE_HAL_VPATH_BMAP_END;
		    i >= VXGE_HAL_VPATH_BMAP_START; i--) {
			if (bVAL1(vpath_mask, i)) {
				pos = VXGE_HAL_VPATH_BMAP_END - i;
				vpath_list[j] = pos;
				j++;
			}
		}
	}

	return (status);
}

vxge_hal_status_e
vxge_hal_rx_bw_priority_set(vxge_hal_device_h devh, u64 vp_id)
{
	u64 data0 = 0x0, data1 = 0x0;
	u32 action, bandwidth, priority, set = 0;

	vxge_hal_vpath_reg_t *vp_reg;
	vxge_hal_status_e status = VXGE_HAL_OK;

	__hal_device_t *hldev = (__hal_device_t *) devh;
	vp_reg = hldev->vpath_reg[0];
	action = VXGE_HAL_BW_CONTROL;

	bandwidth =
	    ((vxge_hal_device_t *) (devh))->config.vp_config[vp_id].bandwidth;

	priority =
	    ((vxge_hal_device_t *) (devh))->config.vp_config[vp_id].priority;

	/*
	 * Get bandwidth and priority settings
	 * and perform read-modify-write operation
	 */
	data0 = 1;
	data0 |= vp_id << 32;

	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, data1);
	if (status != VXGE_HAL_OK)
		goto _exit;

	data1 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_data1);

	/* Set */
	data0 = 0;
	data0 |= vp_id << 32;

	/* Rx Bandwidth */
	if (bandwidth != VXGE_HAL_VPATH_BW_LIMIT_DEFAULT) {
		set = 1;
		data1 &= ~VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_RX_MAX_BW(0xff);
		bandwidth = (bandwidth * 256) / 10000;
		data1 |=
		    VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_RX_MAX_BW(bandwidth);
		data1 |= VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_VPATH_OR_FUNC(1);
	}

	/* Priority */
	if (priority != VXGE_HAL_VPATH_PRIORITY_DEFAULT) {
		set = 1;
		data1 &= ~VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_RX_PRIORITY(0x7);
		data1 |=
		    VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_RX_PRIORITY(priority);
	}

	if (set == 1)
		status = vxge_hal_set_fw_api(devh, 0, action,
		    0x0, data0, data1);

_exit:
	return (status);
}

vxge_hal_status_e
vxge_hal_tx_bw_priority_set(vxge_hal_device_h devh, u64 vp_id)
{
	u64 data0 = 0x0, data1 = 0x0;
	u32 action, bandwidth, priority, set = 0;

	vxge_hal_vpath_reg_t *vp_reg;
	vxge_hal_status_e status = VXGE_HAL_OK;

	__hal_device_t *hldev = (__hal_device_t *) devh;
	vp_reg = hldev->vpath_reg[0];
	action = VXGE_HAL_BW_CONTROL;

	bandwidth =
	    ((vxge_hal_device_t *) (devh))->config.vp_config[vp_id].bandwidth;

	priority =
	    ((vxge_hal_device_t *) (devh))->config.vp_config[vp_id].priority;

	/*
	 * Get bandwidth and priority settings and
	 * perform a read-modify-write operation
	 */
	data0 = 1;
	data0 |= vp_id << 32;

	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, data1);
	if (status != VXGE_HAL_OK)
		goto _exit;

	data1 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_data1);

	/* Set */
	data0 = 0;
	data0 |= vp_id << 32;

	/* Tx Bandwidth */
	if (bandwidth != VXGE_HAL_VPATH_BW_LIMIT_DEFAULT) {
		set = 1;
		data1 &= ~VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_TX_MAX_BW(0xff);
		bandwidth = (bandwidth * 256) / 10000;
		data1 |=
		    VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_TX_MAX_BW(bandwidth);
	}

	/* Priority */
	if (priority != VXGE_HAL_VPATH_PRIORITY_DEFAULT) {
		set = 1;
		data1 &= ~VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_TX_PRIORITY(0x7);
		data1 |=
		    VXGE_HAL_RTS_ACCESS_STEER_DATA1_SET_TX_PRIORITY(priority);
	}

	if (set == 1)
		status = vxge_hal_set_fw_api(devh, 0, action,
		    0x0, data0, data1);

_exit:
	return (status);
}

vxge_hal_status_e
vxge_hal_bw_priority_get(vxge_hal_device_h devh, u64 vp_id,
    u32 *bandwidth, u32 *priority)
{
	u32 action;
	u64 data0 = 0x0, data1 = 0x0;

	vxge_hal_vpath_reg_t *vp_reg;
	vxge_hal_status_e status = VXGE_HAL_OK;

	__hal_device_t *hldev = (__hal_device_t *) devh;
	vp_reg = hldev->vpath_reg[0];
	action = VXGE_HAL_BW_CONTROL;

	/* Get rx bandwidth and rx priority settings */
	data0 = 1;
	data0 |= vp_id << 32;

	status = vxge_hal_set_fw_api(devh, 0, action, 0x0, data0, data1);
	if (status != VXGE_HAL_OK)
		return (status);

	data1 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_data1);

	*priority = (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_RX_PRIORITY(data1);

	/*
	 * Bandwidth setting is stored in increments of approx. 39 Mb/s
	 * so revert it back to get the b/w value
	 */
	*bandwidth = (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_RX_MAX_BW(data1);
	*bandwidth = ((*bandwidth) * 10000) / 256;

	return (status);
}

vxge_hal_status_e
vxge_hal_vf_rx_bw_get(vxge_hal_device_h devh, u64 func_id,
    u32 *bandwidth, u32 *priority)
{
	u32 action;
	u64 data0 = 0x0, data1 = 0x0;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	vxge_hal_vpath_reg_t *vp_reg;
	vxge_hal_status_e status = VXGE_HAL_OK;

	vp_reg = hldev->vpath_reg[func_id];
	action = VXGE_HAL_RTS_ACCESS_FW_MEMO_ACTION_NON_PRIV_BANDWIDTH_CTRL;

	/* Get rx bandwidth and rx priority settings */
	data0 = 3;
	data0 |= func_id << 32;

	status = vxge_hal_set_fw_api(devh, func_id, action, 0x0, data0, data1);
	if (status != VXGE_HAL_OK)
		return (status);

	data1 = vxge_os_pio_mem_read64(hldev->header.pdev,
	    hldev->header.regh0,
	    &vp_reg->rts_access_steer_data1);

	*priority =
	    (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_RX_PRIORITY(data1);

	/*
	 * Bandwidth setting is stored in increments of approx. 39 Mb/s
	 * so revert it back to get the b/w value
	 */
	*bandwidth = (u32) VXGE_HAL_RTS_ACCESS_STEER_DATA1_GET_RX_MAX_BW(data1);
	*bandwidth = ((*bandwidth) * 10000) / 256;

	return (status);
}

void
vxge_hal_vpath_dynamic_tti_rtimer_set(vxge_hal_vpath_h vpath_handle,
    u32 timer_val)
{
	u64 val64, timer;

	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;
	hldev = vpath->hldev;

	val64 = vpath->tim_tti_cfg3_saved;
	timer = (timer_val * 1000) / 272;

	val64 &= ~VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(0x3ffffff);
	if (timer)
		val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(timer) |
		    VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_EVENT_SF(5);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->tim_cfg3_int_num[VXGE_HAL_VPATH_INTR_TX]);

	/*
	 * tti_cfg3_saved is not updated again because it is
	 * initialized at one place only - init time.
	 */
}

void
vxge_hal_vpath_dynamic_rti_rtimer_set(vxge_hal_vpath_h vpath_handle,
    u32 timer_val)
{
	u64 val64, timer;

	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;
	hldev = vpath->hldev;

	val64 = vpath->tim_rti_cfg3_saved;
	timer = (timer_val * 1000) / 272;

	val64 &= ~VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(0x3ffffff);
	if (timer)
		val64 |= VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_VAL(timer) |
		    VXGE_HAL_TIM_CFG3_INT_NUM_RTIMER_EVENT_SF(4);

	vxge_os_pio_mem_write64(hldev->header.pdev,
	    hldev->header.regh0,
	    val64,
	    &vpath->vp_reg->tim_cfg3_int_num[VXGE_HAL_VPATH_INTR_RX]);

	/*
	 * rti_cfg3_saved is not updated again because it is
	 * initialized at one place only - init time.
	 */
}

void
vxge_hal_vpath_tti_ci_set(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;

	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;
	hldev = vpath->hldev;

	if (vpath->vp_config->fifo.enable == VXGE_HAL_FIFO_ENABLE) {
		if (vpath->vp_config->tti.timer_ci_en != VXGE_HAL_TIM_TIMER_CI_ENABLE) {
			vpath->vp_config->tti.timer_ci_en = VXGE_HAL_TIM_TIMER_CI_ENABLE;

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_TX]);

			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;
			vpath->tim_rti_cfg1_saved = val64;

			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    val64,
			    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_TX]);
		}
	}
}

void
vxge_hal_vpath_tti_ci_reset(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;

	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;
	hldev = vpath->hldev;

	if (vpath->vp_config->fifo.enable == VXGE_HAL_FIFO_ENABLE) {
		if (vpath->vp_config->tti.timer_ci_en != VXGE_HAL_TIM_TIMER_CI_DISABLE) {
			vpath->vp_config->tti.timer_ci_en = VXGE_HAL_TIM_TIMER_CI_DISABLE;

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_TX]);

			val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;
			vpath->tim_rti_cfg1_saved = val64;

			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    val64,
			    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_TX]);
		}
	}
}

void
vxge_hal_vpath_rti_ci_set(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;

	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;
	hldev = vpath->hldev;

	if (vpath->vp_config->ring.enable == VXGE_HAL_RING_ENABLE) {
		if (vpath->vp_config->rti.timer_ci_en != VXGE_HAL_TIM_TIMER_CI_ENABLE) {
			vpath->vp_config->rti.timer_ci_en = VXGE_HAL_TIM_TIMER_CI_ENABLE;

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_RX]);

			val64 |= VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;
			vpath->tim_rti_cfg1_saved = val64;

			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    val64,
			    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_RX]);
		}
	}
}

void
vxge_hal_vpath_rti_ci_reset(vxge_hal_vpath_h vpath_handle)
{
	u64 val64;

	__hal_device_t *hldev;
	__hal_virtualpath_t *vpath;

	vpath = ((__hal_vpath_handle_t *) vpath_handle)->vpath;
	hldev = vpath->hldev;

	if (vpath->vp_config->ring.enable == VXGE_HAL_RING_ENABLE) {
		if (vpath->vp_config->rti.timer_ci_en != VXGE_HAL_TIM_TIMER_CI_DISABLE) {
			vpath->vp_config->rti.timer_ci_en = VXGE_HAL_TIM_TIMER_CI_DISABLE;

			val64 = vxge_os_pio_mem_read64(hldev->header.pdev,
			    hldev->header.regh0,
			    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_RX]);

			val64 &= ~VXGE_HAL_TIM_CFG1_INT_NUM_TIMER_CI;
			vpath->tim_rti_cfg1_saved = val64;

			vxge_os_pio_mem_write64(hldev->header.pdev,
			    hldev->header.regh0,
			    val64,
			    &vpath->vp_reg->tim_cfg1_int_num[VXGE_HAL_VPATH_INTR_RX]);
		}
	}
}

vxge_hal_status_e
vxge_hal_send_message(vxge_hal_device_h devh, u64 vp_id, u8 msg_type,
    u8 msg_dst, u32 msg_data, u64 *msg_sent_to_vpaths)
{
	u32 action;
	u64 data0 = 0x0, data1 = 0x0;
	u32 attempts = VXGE_HAL_MSG_SEND_RETRY;
	vxge_hal_status_e status = VXGE_HAL_OK;

	data0 = VXGE_HAL_RTS_ACCESS_STEER_DATA0_SEND_MSG_TYPE(msg_type) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_SEND_MSG_DEST(msg_dst) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_SEND_MSG_SRC(vp_id) |
	    VXGE_HAL_RTS_ACCESS_STEER_DATA0_SEND_MSG_DATA(msg_data);

	action = VXGE_HAL_RTS_ACCESS_STEER_CTRL_ACTION_SEND_MSG;

	do {
		status = vxge_hal_set_fw_api(devh, vp_id, action, 0x0,
		    data0, data1);
		if (status != VXGE_HAL_OK) {
			attempts--;
			if (attempts == 0)
			return (status);
		}
	} while (status != VXGE_HAL_OK);

	if (msg_sent_to_vpaths != NULL) {
		/* The API returns a vector of VPATHs the message
		* was sent to in the event the destination is a
		* broadcast message or being sent to the privileged VPATH
		*/
		*msg_sent_to_vpaths = data0 & VXGE_HAL_MSG_SEND_TO_VPATH_MASK;
	}

	return (status);
}
