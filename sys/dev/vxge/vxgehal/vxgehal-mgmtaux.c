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

#define	VXGE_HAL_AUX_SEPA		' '

#define	__hal_aux_snprintf(retbuf, bufsize, fmt, key, value, retsize) \
	if (bufsize <= 0) \
		return (VXGE_HAL_ERR_OUT_OF_SPACE); \
	retsize = vxge_os_snprintf(retbuf, bufsize, fmt, key, \
	    VXGE_HAL_AUX_SEPA, value); \
	if (retsize < 0 || retsize >= bufsize) \
		return (VXGE_HAL_ERR_OUT_OF_SPACE);

#define	__HAL_AUX_ENTRY_DECLARE(size, buf) \
	int entrysize = 0, leftsize = size; \
	char *ptr; ptr = buf;

#define	__HAL_AUX_ENTRY(key, value, fmt) \
	__hal_aux_snprintf(ptr, leftsize, "%s%c"fmt"\n", key, value, entrysize)\
	ptr += entrysize; leftsize -= entrysize;

#define	__HAL_AUX_CONFIG_ENTRY(key, value, fmt) \
	if (value == VXGE_HAL_USE_FLASH_DEFAULT) { \
		__HAL_AUX_ENTRY(key, "FLASH DEFAULT", "%s"); \
	} else { \
		__HAL_AUX_ENTRY(key, value, fmt); \
	}

#define	__HAL_AUX_ENTRY_END(bufsize, retsize) \
	*retsize = bufsize - leftsize;

#define	__hal_aux_pci_link_info(name, index, var) {	\
		__HAL_AUX_ENTRY(name,			\
		    (u64)pcim.link_info[index].var, "%llu") \
	}

#define	__hal_aux_pci_aggr_info(name, index, var) { \
		__HAL_AUX_ENTRY(name,				\
		    (u64)pcim.aggr_info[index].var, "%llu") \
	}

/*
 * vxge_hal_aux_about_read - Retrieve and format about info.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Retrieve about info (using vxge_hal_mgmt_about()) and sprintf it
 * into the provided @retbuf.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 * VXGE_HAL_FAIL - Failed to retrieve the information.
 *
 * See also: vxge_hal_mgmt_about(), vxge_hal_aux_device_dump().
 */
vxge_hal_status_e
vxge_hal_aux_about_read(vxge_hal_device_h devh, int bufsize,
    char *retbuf, int *retsize)
{
	u32 size = sizeof(vxge_hal_mgmt_about_info_t);
	vxge_hal_status_e status;
	vxge_hal_mgmt_about_info_t about_info;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	status = vxge_hal_mgmt_about(devh, &about_info, &size);
	if (status != VXGE_HAL_OK)
		return (status);

	__HAL_AUX_ENTRY("vendor", about_info.vendor, "0x%x");
	__HAL_AUX_ENTRY("device", about_info.device, "0x%x");
	__HAL_AUX_ENTRY("subsys_vendor", about_info.subsys_vendor, "0x%x");
	__HAL_AUX_ENTRY("subsys_device", about_info.subsys_device, "0x%x");
	__HAL_AUX_ENTRY("board_rev", about_info.board_rev, "0x%x");
	__HAL_AUX_ENTRY("vendor_name", about_info.vendor_name, "%s");
	__HAL_AUX_ENTRY("chip_name", about_info.chip_name, "%s");
	__HAL_AUX_ENTRY("media", about_info.media, "%s");
	__HAL_AUX_ENTRY("hal_major", about_info.hal_major, "%s");
	__HAL_AUX_ENTRY("hal_minor", about_info.hal_minor, "%s");
	__HAL_AUX_ENTRY("hal_fix", about_info.hal_fix, "%s");
	__HAL_AUX_ENTRY("hal_build", about_info.hal_build, "%s");
	__HAL_AUX_ENTRY("ll_major", about_info.ll_major, "%s");
	__HAL_AUX_ENTRY("ll_minor", about_info.ll_minor, "%s");
	__HAL_AUX_ENTRY("ll_fix", about_info.ll_fix, "%s");
	__HAL_AUX_ENTRY("ll_build", about_info.ll_build, "%s");

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_driver_config_read - Read Driver configuration.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read driver configuration,
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: vxge_hal_aux_device_config_read().
 */
vxge_hal_status_e
vxge_hal_aux_driver_config_read(int bufsize, char *retbuf, int *retsize)
{
	u32 size = sizeof(vxge_hal_driver_config_t);
	vxge_hal_status_e status;
	vxge_hal_driver_config_t drv_config;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	status = vxge_hal_mgmt_driver_config(&drv_config, &size);
	if (status != VXGE_HAL_OK)
		return (status);

	__HAL_AUX_ENTRY("Debug Level",
	    g_vxge_hal_driver->debug_level, "%u");
	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_pci_config_read - Retrieve and format PCI Configuration
 * info.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Retrieve about info (using vxge_hal_mgmt_pci_config()) and sprintf it
 * into the provided @retbuf.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: vxge_hal_mgmt_pci_config(), vxge_hal_aux_device_dump().
 */
vxge_hal_status_e
vxge_hal_aux_pci_config_read(
    vxge_hal_device_h devh,
    int bufsize,
    char *retbuf,
    int *retsize)
{
	u8 cap_id;
	u16 ext_cap_id;
	u16 next_ptr;
	u32 size = sizeof(vxge_hal_pci_config_t);
	vxge_hal_status_e status;
	vxge_hal_pci_config_t *pci_config =
	&((__hal_device_t *) devh)->pci_config_space;
	vxge_hal_mgmt_pm_cap_t pm_cap;
	vxge_hal_mgmt_sid_cap_t sid_cap;
	vxge_hal_mgmt_msi_cap_t msi_cap;
	vxge_hal_mgmt_msix_cap_t msix_cap;
	vxge_hal_pci_err_cap_t err_cap;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	status = vxge_hal_mgmt_pci_config(devh, (u8 *) pci_config, &size);
	if (status != VXGE_HAL_OK)
		return (status);

	__HAL_AUX_ENTRY("vendor_id", pci_config->vendor_id, "0x%04X");
	__HAL_AUX_ENTRY("device_id", pci_config->device_id, "0x%04X");
	__HAL_AUX_ENTRY("command", pci_config->command, "0x%04X");
	__HAL_AUX_ENTRY("status", pci_config->status, "0x%04X");
	__HAL_AUX_ENTRY("revision", pci_config->revision, "0x%02X");
	__HAL_AUX_ENTRY("pciClass1", pci_config->pciClass[0], "0x%02X");
	__HAL_AUX_ENTRY("pciClass2", pci_config->pciClass[1], "0x%02X");
	__HAL_AUX_ENTRY("pciClass3", pci_config->pciClass[2], "0x%02X");
	__HAL_AUX_ENTRY("cache_line_size",
	    pci_config->cache_line_size, "0x%02X");
	__HAL_AUX_ENTRY("header_type", pci_config->header_type, "0x%02X");
	__HAL_AUX_ENTRY("bist", pci_config->bist, "0x%02X");
	__HAL_AUX_ENTRY("base_addr0_lo", pci_config->base_addr0_lo, "0x%08X");
	__HAL_AUX_ENTRY("base_addr0_hi", pci_config->base_addr0_hi, "0x%08X");
	__HAL_AUX_ENTRY("base_addr1_lo", pci_config->base_addr1_lo, "0x%08X");
	__HAL_AUX_ENTRY("base_addr1_hi", pci_config->base_addr1_hi, "0x%08X");
	__HAL_AUX_ENTRY("not_Implemented1",
	    pci_config->not_Implemented1, "0x%08X");
	__HAL_AUX_ENTRY("not_Implemented2", pci_config->not_Implemented2,
	    "0x%08X");
	__HAL_AUX_ENTRY("cardbus_cis_pointer", pci_config->cardbus_cis_pointer,
	    "0x%08X");
	__HAL_AUX_ENTRY("subsystem_vendor_id", pci_config->subsystem_vendor_id,
	    "0x%04X");
	__HAL_AUX_ENTRY("subsystem_id", pci_config->subsystem_id, "0x%04X");
	__HAL_AUX_ENTRY("rom_base", pci_config->rom_base, "0x%08X");
	__HAL_AUX_ENTRY("capabilities_pointer",
	    pci_config->capabilities_pointer, "0x%02X");
	__HAL_AUX_ENTRY("interrupt_line", pci_config->interrupt_line, "0x%02X");
	__HAL_AUX_ENTRY("interrupt_pin", pci_config->interrupt_pin, "0x%02X");
	__HAL_AUX_ENTRY("min_grant", pci_config->min_grant, "0x%02X");
	__HAL_AUX_ENTRY("max_latency", pci_config->max_latency, "0x%02X");

	next_ptr = pci_config->capabilities_pointer;

	while (next_ptr != 0) {

		cap_id = VXGE_HAL_PCI_CAP_ID((((u8 *) pci_config) + next_ptr));

		switch (cap_id) {

		case VXGE_HAL_PCI_CAP_ID_PM:
			status = vxge_hal_mgmt_pm_capabilities_get(devh,
			    &pm_cap);
			if (status != VXGE_HAL_OK)
				return (status);

			__HAL_AUX_ENTRY("PM Capability",
			    cap_id, "0x%02X");
			__HAL_AUX_ENTRY("pm_cap_ver",
			    pm_cap.pm_cap_ver, "%u");
			__HAL_AUX_ENTRY("pm_cap_pme_clock",
			    pm_cap.pm_cap_pme_clock, "%u");
			__HAL_AUX_ENTRY("pm_cap_aux_power",
			    pm_cap.pm_cap_aux_power, "%u");
			__HAL_AUX_ENTRY("pm_cap_dsi",
			    pm_cap.pm_cap_dsi, "%u");
			__HAL_AUX_ENTRY("pm_cap_aux_current",
			    pm_cap.pm_cap_aux_current, "%u");
			__HAL_AUX_ENTRY("pm_cap_cap_d0",
			    pm_cap.pm_cap_cap_d0, "%u");
			__HAL_AUX_ENTRY("pm_cap_cap_d1",
			    pm_cap.pm_cap_cap_d1, "%u");
			__HAL_AUX_ENTRY("pm_cap_pme_d0",
			    pm_cap.pm_cap_pme_d0, "%u");
			__HAL_AUX_ENTRY("pm_cap_pme_d1",
			    pm_cap.pm_cap_pme_d1, "%u");
			__HAL_AUX_ENTRY("pm_cap_pme_d2",
			    pm_cap.pm_cap_pme_d2, "%u");
			__HAL_AUX_ENTRY("pm_cap_pme_d3_hot",
			    pm_cap.pm_cap_pme_d3_hot, "%u");
			__HAL_AUX_ENTRY("pm_cap_pme_d3_cold",
			    pm_cap.pm_cap_pme_d3_cold, "%u");
			__HAL_AUX_ENTRY("pm_ctrl_state",
			    pm_cap.pm_ctrl_state, "%u");
			__HAL_AUX_ENTRY("pm_ctrl_no_soft_reset",
			    pm_cap.pm_ctrl_no_soft_reset, "%u");
			__HAL_AUX_ENTRY("pm_ctrl_pme_enable",
			    pm_cap.pm_ctrl_pme_enable, "%u");
			__HAL_AUX_ENTRY("pm_ctrl_pme_data_sel",
			    pm_cap.pm_ctrl_pme_data_sel, "%u");
			__HAL_AUX_ENTRY("pm_ctrl_pme_data_scale",
			    pm_cap.pm_ctrl_pme_data_scale, "%u");
			__HAL_AUX_ENTRY("pm_ctrl_pme_status",
			    pm_cap.pm_ctrl_pme_status, "%u");
			__HAL_AUX_ENTRY("pm_ppb_ext_b2_b3",
			    pm_cap.pm_ppb_ext_b2_b3, "%u");
			__HAL_AUX_ENTRY("pm_ppb_ext_ecc_en",
			    pm_cap.pm_ppb_ext_ecc_en, "%u");
			__HAL_AUX_ENTRY("pm_data_reg",
			    pm_cap.pm_data_reg, "%u");
			break;
		case VXGE_HAL_PCI_CAP_ID_VPD:
			break;
		case VXGE_HAL_PCI_CAP_ID_SLOTID:
			status = vxge_hal_mgmt_sid_capabilities_get(devh,
			    &sid_cap);
			if (status != VXGE_HAL_OK)
				return (status);

			__HAL_AUX_ENTRY("SID Capability", cap_id, "0x%02X");
			__HAL_AUX_ENTRY("sid_number_of_slots",
			    sid_cap.sid_number_of_slots, "%u");
			__HAL_AUX_ENTRY("sid_first_in_chasis",
			    sid_cap.sid_first_in_chasis, "%u");
			__HAL_AUX_ENTRY("sid_chasis_number",
			    sid_cap.sid_chasis_number, "0x%u");
			break;
		case VXGE_HAL_PCI_CAP_ID_MSI:
			status = vxge_hal_mgmt_msi_capabilities_get(devh,
			    &msi_cap);
			if (status != VXGE_HAL_OK)
				return (status);

			__HAL_AUX_ENTRY("MSI Capability", cap_id, "0x%02X");
			__HAL_AUX_ENTRY("MSI Enable", msi_cap.enable, "%u");
			__HAL_AUX_ENTRY("MSI 64bit Address Capable",
			    msi_cap.is_64bit_addr_capable, "%u");
			__HAL_AUX_ENTRY("MSI PVM Capable",
			    msi_cap.is_pvm_capable, "0x%02X");
			__HAL_AUX_ENTRY("MSI Vectors Allocated",
			    msi_cap.vectors_allocated, "0x%02X");
			__HAL_AUX_ENTRY("MSI Max Vectors",
			    msi_cap.max_vectors_capable, "0x%02X");
			if (msi_cap.is_64bit_addr_capable) {
				__HAL_AUX_ENTRY("MSI address",
				    msi_cap.address, "0x%016llX");
			} else {
				__HAL_AUX_ENTRY("MSI address",
				    msi_cap.address, "0x%08llX");
			}
			__HAL_AUX_ENTRY("MSI Data", msi_cap.data, "0x%04X");
			if (msi_cap.is_pvm_capable) {
				__HAL_AUX_ENTRY("MSI Mask bits",
				    msi_cap.mask_bits, "0x%08X");
				__HAL_AUX_ENTRY("MSI Pending bits",
				    msi_cap.pending_bits, "0x%08X");
			}
			break;
		case VXGE_HAL_PCI_CAP_ID_VS:
			break;
		case VXGE_HAL_PCI_CAP_ID_SHPC:
			break;
		case VXGE_HAL_PCI_CAP_ID_PCIE:
			break;
		case VXGE_HAL_PCI_CAP_ID_MSIX:
			status = vxge_hal_mgmt_msix_capabilities_get(devh,
			    &msix_cap);
			if (status != VXGE_HAL_OK)
				return (status);

			__HAL_AUX_ENTRY("MSIX Capability", cap_id, "0x%02X");
			__HAL_AUX_ENTRY("MSIX Enable", msix_cap.enable, "%u");
			__HAL_AUX_ENTRY("MSIX Mask All vectors",
			    msix_cap.mask_all_vect, "%u");
			__HAL_AUX_ENTRY("MSIX Table Size",
			    msix_cap.table_size, "%u");
			__HAL_AUX_ENTRY("MSIX Table Offset",
			    msix_cap.table_offset, "%u");
			__HAL_AUX_ENTRY("MSIX Table BIR",
			    msix_cap.table_bir, "%u");
			__HAL_AUX_ENTRY("MSIX PBA Offset",
			    msix_cap.pba_offset, "%u");
			__HAL_AUX_ENTRY("MSIX PBA BIR", msix_cap.pba_bir, "%u");
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
			__HAL_AUX_ENTRY("Unexpected Capability",
			    cap_id, "0x%02X");
			break;
		default:
			__HAL_AUX_ENTRY("Unknown Capability",
			    cap_id, "0x%02X");
			break;
		}

		next_ptr =
		    VXGE_HAL_PCI_CAP_NEXT((((u8 *) pci_config) + next_ptr));

	}

	/* CONSTCOND */
	if (VXGE_HAL_PCI_CONFIG_SPACE_SIZE > 0x100) {

		next_ptr = 0x100;

		while (next_ptr != 0) {

			ext_cap_id = (u16) VXGE_HAL_PCI_EXT_CAP_ID(
			    *(u32 *)((void *)(((u8 *) pci_config) + next_ptr)));

			switch (ext_cap_id) {

			case VXGE_HAL_PCI_EXT_CAP_ID_ERR:
				status =
				    vxge_hal_mgmt_pci_err_capabilities_get(devh,
				    &err_cap);
				if (status != VXGE_HAL_OK)
					return (status);

				__HAL_AUX_ENTRY("pci_err_header",
				    err_cap.pci_err_header, "0x%08X");
				__HAL_AUX_ENTRY("pci_err_uncor_status",
				    err_cap.pci_err_uncor_status, "0x%08X");
				__HAL_AUX_ENTRY("pci_err_uncor_mask",
				    err_cap.pci_err_uncor_mask, "0x%08X");
				__HAL_AUX_ENTRY("pci_err_uncor_server",
				    err_cap.pci_err_uncor_server, "0x%08X");
				__HAL_AUX_ENTRY("pci_err_cor_status",
				    err_cap.pci_err_cor_status, "0x%08X");
				__HAL_AUX_ENTRY("pci_err_cap",
				    err_cap.pci_err_cap, "0x%08X");
				__HAL_AUX_ENTRY("err_header_log",
				    err_cap.err_header_log, "%u");
				__HAL_AUX_ENTRY("pci_err_root_command",
				    err_cap.pci_err_root_command, "0x%08X");
				__HAL_AUX_ENTRY("pci_err_root_status",
				    err_cap.pci_err_root_status, "0x%08X");
				__HAL_AUX_ENTRY("pci_err_root_cor_src",
				    err_cap.pci_err_root_cor_src, "0x%04X");
				__HAL_AUX_ENTRY("pci_err_root_src",
				    err_cap.pci_err_root_src, "0x%04X");
				break;
			case VXGE_HAL_PCI_EXT_CAP_ID_VC:
				break;
			case VXGE_HAL_PCI_EXT_CAP_ID_DSN:
				break;
			case VXGE_HAL_PCI_EXT_CAP_ID_PWR:
				break;
			default:
				__HAL_AUX_ENTRY("Unknown Capability", cap_id,
				    "0x%02X");
				break;
			}
			next_ptr = (u16) VXGE_HAL_PCI_EXT_CAP_NEXT(
			    *(u32 *)((void *)(((u8 *) pci_config) + next_ptr)));

		}

	}

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_device_config_read - Read device configuration.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device configuration,
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_VERSION_CONFLICT - Version it not maching.
 *
 * See also: vxge_hal_aux_driver_config_read().
 */
vxge_hal_status_e
vxge_hal_aux_device_config_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize)
{
	int i;
	u32 size = sizeof(vxge_hal_device_config_t);
	vxge_hal_status_e status;
	vxge_hal_mac_config_t *mac_config;
	vxge_hal_device_config_t *dev_config;
	vxge_hal_device_t *hldev = (vxge_hal_device_t *) devh;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	dev_config = (vxge_hal_device_config_t *) vxge_os_malloc(hldev->pdev,
	    sizeof(vxge_hal_device_config_t));
	if (dev_config == NULL) {
		return (VXGE_HAL_FAIL);
	}

	status = vxge_hal_mgmt_device_config(devh, dev_config, &size);
	if (status != VXGE_HAL_OK) {
		vxge_os_free(hldev->pdev, dev_config,
		    sizeof(vxge_hal_device_config_t));
		return (status);
	}

	__HAL_AUX_CONFIG_ENTRY("DMA Block Pool size-Minimum",
	    dev_config->dma_blockpool_min, "%u");
	__HAL_AUX_CONFIG_ENTRY("DMA Block Pool size-Initial",
	    dev_config->dma_blockpool_initial, "%u");
	__HAL_AUX_CONFIG_ENTRY("DMA Block Pool size-Increment",
	    dev_config->dma_blockpool_incr, "%u");
	__HAL_AUX_CONFIG_ENTRY("DMA Block Pool size-Maximum",
	    dev_config->dma_blockpool_max, "%u");
	for (i = 0; i < VXGE_HAL_MAC_MAX_WIRE_PORTS; i++) {
		mac_config = &dev_config->mrpcim_config.mac_config;
		__HAL_AUX_CONFIG_ENTRY("port_id",
		    mac_config->wire_port_config[i].port_id, "%u");
		__HAL_AUX_CONFIG_ENTRY("media",
		    mac_config->wire_port_config[i].media, "%u");
		__HAL_AUX_CONFIG_ENTRY("mtu",
		    mac_config->wire_port_config[i].mtu, "%u");
		__HAL_AUX_CONFIG_ENTRY("autoneg_mode",
		    mac_config->wire_port_config[i].autoneg_mode, "%u");
		__HAL_AUX_CONFIG_ENTRY("fixed_use_fsm",
		    mac_config->wire_port_config[i].fixed_use_fsm, "%u");
		__HAL_AUX_CONFIG_ENTRY("antp_use_fsm",
		    mac_config->wire_port_config[i].antp_use_fsm, "%u");
		__HAL_AUX_CONFIG_ENTRY("anbe_use_fsm",
		    mac_config->wire_port_config[i].anbe_use_fsm, "%u");
		__HAL_AUX_CONFIG_ENTRY("link_stability_period",
		    mac_config->wire_port_config[i].link_stability_period,
		    "%u");
		__HAL_AUX_CONFIG_ENTRY("port_stability_period",
		    mac_config->wire_port_config[i].port_stability_period,
		    "%u");
		__HAL_AUX_CONFIG_ENTRY("tmac_en",
		    mac_config->wire_port_config[i].tmac_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("rmac_en",
		    mac_config->wire_port_config[i].rmac_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("tmac_pad",
		    mac_config->wire_port_config[i].tmac_pad, "%u");
		__HAL_AUX_CONFIG_ENTRY("tmac_pad_byte",
		    mac_config->wire_port_config[i].tmac_pad_byte, "%u");
		__HAL_AUX_CONFIG_ENTRY("tmac_util_period",
		    mac_config->wire_port_config[i].tmac_util_period, "%u");
		__HAL_AUX_CONFIG_ENTRY("rmac_strip_fcs",
		    mac_config->wire_port_config[i].rmac_strip_fcs, "%u");
		__HAL_AUX_CONFIG_ENTRY("rmac_prom_en",
		    mac_config->wire_port_config[i].rmac_prom_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("rmac_discard_pfrm",
		    mac_config->wire_port_config[i].rmac_discard_pfrm, "%u");
		__HAL_AUX_CONFIG_ENTRY("rmac_util_period",
		    mac_config->wire_port_config[i].rmac_util_period, "%u");
		__HAL_AUX_CONFIG_ENTRY("rmac_pause_gen_en",
		    mac_config->wire_port_config[i].rmac_pause_gen_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("rmac_pause_rcv_en",
		    mac_config->wire_port_config[i].rmac_pause_rcv_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("rmac_pause_time",
		    mac_config->wire_port_config[i].rmac_pause_time, "%u");
		__HAL_AUX_CONFIG_ENTRY("limiter_en",
		    mac_config->wire_port_config[i].limiter_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("max_limit",
		    mac_config->wire_port_config[i].max_limit, "%u");
	}

	/* CONSTCOND */
	__HAL_AUX_CONFIG_ENTRY("port_id",
	    VXGE_HAL_MAC_SWITCH_PORT, "%u");
	__HAL_AUX_CONFIG_ENTRY("mtu",
	    mac_config->switch_port_config.mtu, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_en",
	    mac_config->switch_port_config.tmac_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("rmac_en",
	    mac_config->switch_port_config.rmac_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_pad",
	    mac_config->switch_port_config.tmac_pad, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_pad_byte",
	    mac_config->switch_port_config.tmac_pad_byte, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_util_period",
	    mac_config->switch_port_config.tmac_util_period, "%u");
	__HAL_AUX_CONFIG_ENTRY("rmac_strip_fcs",
	    mac_config->switch_port_config.rmac_strip_fcs, "%u");
	__HAL_AUX_CONFIG_ENTRY("rmac_prom_en",
	    mac_config->switch_port_config.rmac_prom_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("rmac_discard_pfrm",
	    mac_config->switch_port_config.rmac_discard_pfrm, "%u");
	__HAL_AUX_CONFIG_ENTRY("rmac_util_period",
	    mac_config->switch_port_config.rmac_util_period, "%u");
	__HAL_AUX_CONFIG_ENTRY("rmac_pause_gen_en",
	    mac_config->switch_port_config.rmac_pause_gen_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("rmac_pause_rcv_en",
	    mac_config->switch_port_config.rmac_pause_rcv_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("rmac_pause_time",
	    mac_config->switch_port_config.rmac_pause_time, "%u");
	__HAL_AUX_CONFIG_ENTRY("limiter_en",
	    mac_config->switch_port_config.limiter_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("max_limit",
	    mac_config->switch_port_config.max_limit, "%u");

	__HAL_AUX_CONFIG_ENTRY("network_stability_period",
	    mac_config->network_stability_period, "%u");
	for (i = 0; i < 16; i++) {
		__HAL_AUX_CONFIG_ENTRY("mc_pause_threshold[i]",
		    mac_config->mc_pause_threshold[i], "%u");
	}
	__HAL_AUX_CONFIG_ENTRY("tmac_perma_stop_en",
	    mac_config->tmac_perma_stop_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_tx_switch_dis",
	    mac_config->tmac_tx_switch_dis, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_lossy_switch_en",
	    mac_config->tmac_lossy_switch_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_lossy_wire_en",
	    mac_config->tmac_lossy_wire_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_bcast_to_wire_dis",
	    mac_config->tmac_bcast_to_wire_dis, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_bcast_to_switch_dis",
	    mac_config->tmac_bcast_to_switch_dis, "%u");
	__HAL_AUX_CONFIG_ENTRY("tmac_host_append_fcs_en",
	    mac_config->tmac_host_append_fcs_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("tpa_support_snap_ab_n",
	    mac_config->tpa_support_snap_ab_n, "%u");
	__HAL_AUX_CONFIG_ENTRY("tpa_ecc_enable_n",
	    mac_config->tpa_ecc_enable_n, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_ignore_frame_err",
	    mac_config->rpa_ignore_frame_err, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_support_snap_ab_n",
	    mac_config->rpa_support_snap_ab_n, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_search_for_hao",
	    mac_config->rpa_search_for_hao, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_support_ipv6_mobile_hdrs",
	    mac_config->rpa_support_ipv6_mobile_hdrs, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_ipv6_stop_searching",
	    mac_config->rpa_ipv6_stop_searching, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_no_ps_if_unknown",
	    mac_config->rpa_no_ps_if_unknown, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_search_for_etype",
	    mac_config->rpa_search_for_etype, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_l4_comp_csum",
	    mac_config->rpa_repl_l4_comp_csum, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_l3_incl_cf",
	    mac_config->rpa_repl_l3_incl_cf, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_l3_comp_csum",
	    mac_config->rpa_repl_l3_comp_csum, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_ipv4_tcp_incl_ph",
	    mac_config->rpa_repl_ipv4_tcp_incl_ph, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_ipv6_tcp_incl_ph",
	    mac_config->rpa_repl_ipv6_tcp_incl_ph, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_ipv4_udp_incl_ph",
	    mac_config->rpa_repl_ipv4_udp_incl_ph, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_ipv6_udp_incl_ph",
	    mac_config->rpa_repl_ipv6_udp_incl_ph, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_l4_incl_cf",
	    mac_config->rpa_repl_l4_incl_cf, "%u");
	__HAL_AUX_CONFIG_ENTRY("rpa_repl_strip_vlan_tag",
	    mac_config->rpa_repl_strip_vlan_tag, "%u");
	__HAL_AUX_CONFIG_ENTRY("ISR Polling count",
	    dev_config->isr_polling_cnt, "%u");
	__HAL_AUX_CONFIG_ENTRY("Maximum Payload Size",
	    dev_config->max_payload_size, "%u");
	__HAL_AUX_CONFIG_ENTRY("MMRB Count",
	    dev_config->mmrb_count, "%u");
	__HAL_AUX_CONFIG_ENTRY("Statistics Refresh Time",
	    dev_config->stats_refresh_time_sec, "%u");
	__HAL_AUX_CONFIG_ENTRY("Interrupt Mode",
	    dev_config->intr_mode, "%u");
	__HAL_AUX_CONFIG_ENTRY("Dump on Unknwon Error",
	    dev_config->dump_on_unknown, "%u");
	__HAL_AUX_CONFIG_ENTRY("Dump on Serious Error",
	    dev_config->dump_on_serr, "%u");
	__HAL_AUX_CONFIG_ENTRY("Dump on Critical Error",
	    dev_config->dump_on_critical, "%u");
	__HAL_AUX_CONFIG_ENTRY("Dump on ECC Error",
	    dev_config->dump_on_eccerr, "%u");
	__HAL_AUX_CONFIG_ENTRY("RTH Enable",
	    dev_config->rth_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("RTS MAC Enable",
	    dev_config->rts_mac_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("RTS QOS Enable",
	    dev_config->rts_qos_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("RTS Port Enable",
	    dev_config->rts_port_en, "%u");
	__HAL_AUX_CONFIG_ENTRY("Max CQE Groups",
	    dev_config->max_cqe_groups, "%u");
	__HAL_AUX_CONFIG_ENTRY("Max Number of OD Groups",
	    dev_config->max_num_wqe_od_groups, "%u");
	__HAL_AUX_CONFIG_ENTRY("No WQE Threshold",
	    dev_config->no_wqe_threshold, "%u");
	__HAL_AUX_CONFIG_ENTRY("Refill Threshold-High",
	    dev_config->refill_threshold_high, "%u");
	__HAL_AUX_CONFIG_ENTRY("Refill Threshold-Low",
	    dev_config->refill_threshold_low, "%u");
	__HAL_AUX_CONFIG_ENTRY("Ack Block Limit",
	    dev_config->ack_blk_limit, "%u");
	__HAL_AUX_CONFIG_ENTRY("Poll or Doorbell",
	    dev_config->poll_or_doorbell, "%u");
	__HAL_AUX_CONFIG_ENTRY("stats_read_method",
	    dev_config->stats_read_method, "%u");
	__HAL_AUX_CONFIG_ENTRY("Device Poll Timeout",
	    dev_config->device_poll_millis, "%u");
	__HAL_AUX_CONFIG_ENTRY("debug_level",
	    dev_config->debug_level, "%u");
	__HAL_AUX_CONFIG_ENTRY("debug_mask",
	    dev_config->debug_mask, "%u");

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)
	__HAL_AUX_CONFIG_ENTRY("Trace buffer size",
	    dev_config->tracebuf_size, "%u");
#endif

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {
		if (!(((__hal_device_t *) hldev)->vpath_assignments & mBIT(i)))
			continue;

		__HAL_AUX_CONFIG_ENTRY("Virtual Path id",
		    dev_config->vp_config[i].vp_id, "%u");
		__HAL_AUX_CONFIG_ENTRY("No Snoop",
		    dev_config->vp_config[i].no_snoop, "%u");
		__HAL_AUX_CONFIG_ENTRY("mtu",
		    dev_config->vp_config[i].mtu, "%u");
		__HAL_AUX_CONFIG_ENTRY("TPA LSOv2 Enable",
		    dev_config->vp_config[i].tpa_lsov2_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("TPA Ignore Frame Error",
		    dev_config->vp_config[i].tpa_ignore_frame_error, "%u");
		__HAL_AUX_CONFIG_ENTRY("TPA IPv6 Keep Searching",
		    dev_config->vp_config[i].tpa_ipv6_keep_searching, "%u");
		__HAL_AUX_CONFIG_ENTRY("TPA L4 pseudo header present",
		    dev_config->vp_config[i].tpa_l4_pshdr_present, "%u");
		__HAL_AUX_CONFIG_ENTRY("TPA support mobile IPv6 Headers",
		    dev_config->vp_config[i].tpa_support_mobile_ipv6_hdrs,
		    "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA IPv4 TCP Include pseudo header",
		    dev_config->vp_config[i].rpa_ipv4_tcp_incl_ph, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA IPv6 TCP Include pseudo header",
		    dev_config->vp_config[i].rpa_ipv6_tcp_incl_ph, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA IPv4 UDP Include pseudo header",
		    dev_config->vp_config[i].rpa_ipv4_udp_incl_ph, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA IPv6 UDP Include pseudo header",
		    dev_config->vp_config[i].rpa_ipv6_udp_incl_ph, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA L4 Include CF",
		    dev_config->vp_config[i].rpa_l4_incl_cf, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA Strip VLAN Tag",
		    dev_config->vp_config[i].rpa_strip_vlan_tag, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA L4 Comp Csum Enable",
		    dev_config->vp_config[i].rpa_l4_comp_csum, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA L3 Include CF Enable",
		    dev_config->vp_config[i].rpa_l3_incl_cf, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA L3 Comp Csum",
		    dev_config->vp_config[i].rpa_l3_comp_csum, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA Unicast All Address Enable",
		    dev_config->vp_config[i].rpa_ucast_all_addr_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA Unicast All Address Enable",
		    dev_config->vp_config[i].rpa_ucast_all_addr_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA Multicast All Address Enable",
		    dev_config->vp_config[i].rpa_mcast_all_addr_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA Broadcast Enable",
		    dev_config->vp_config[i].rpa_bcast_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("RPA All VID Enable",
		    dev_config->vp_config[i].rpa_all_vid_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("VP Queue L2 Flow",
		    dev_config->vp_config[i].vp_queue_l2_flow, "%u");

		__HAL_AUX_CONFIG_ENTRY("Ring blocks",
		    dev_config->vp_config[i].ring.ring_length, "%u");
		__HAL_AUX_CONFIG_ENTRY("Buffer Mode",
		    dev_config->vp_config[i].ring.buffer_mode, "%u");
		__HAL_AUX_CONFIG_ENTRY("Scatter Mode",
		    dev_config->vp_config[i].ring.scatter_mode, "%u");
		__HAL_AUX_CONFIG_ENTRY("Post Mode",
		    dev_config->vp_config[i].ring.post_mode, "%u");
		__HAL_AUX_CONFIG_ENTRY("Maximum Frame Length",
		    dev_config->vp_config[i].ring.max_frm_len, "%u");
		__HAL_AUX_CONFIG_ENTRY("No Snoop Bits",
		    dev_config->vp_config[i].ring.no_snoop_bits, "%u");
		__HAL_AUX_CONFIG_ENTRY("Rx Timer Value",
		    dev_config->vp_config[i].ring.rx_timer_val, "%u");
		__HAL_AUX_CONFIG_ENTRY("Greedy return",
		    dev_config->vp_config[i].ring.greedy_return, "%u");
		__HAL_AUX_CONFIG_ENTRY("Rx Timer CI",
		    dev_config->vp_config[i].ring.rx_timer_ci, "%u");
		__HAL_AUX_CONFIG_ENTRY("Backoff Interval",
		    dev_config->vp_config[i].ring.backoff_interval_us, "%u");
		__HAL_AUX_CONFIG_ENTRY("Indicate Max Packets",
		    dev_config->vp_config[i].ring.indicate_max_pkts, "%u");


		__HAL_AUX_CONFIG_ENTRY("FIFO Blocks",
		    dev_config->vp_config[i].fifo.fifo_length, "%u");
		__HAL_AUX_CONFIG_ENTRY("Max Frags",
		    dev_config->vp_config[i].fifo.max_frags, "%u");
		__HAL_AUX_CONFIG_ENTRY("Alignment Size",
		    dev_config->vp_config[i].fifo.alignment_size, "%u");
		__HAL_AUX_CONFIG_ENTRY("Maximum Aligned Frags",
		    dev_config->vp_config[i].fifo.max_aligned_frags, "%u");
		__HAL_AUX_CONFIG_ENTRY("Interrupt Enable",
		    dev_config->vp_config[i].fifo.intr, "%u");
		__HAL_AUX_CONFIG_ENTRY("No Snoop Bits",
		    dev_config->vp_config[i].fifo.no_snoop_bits, "%u");


		__HAL_AUX_CONFIG_ENTRY("Interrupt Enable",
		    dev_config->vp_config[i].tti.intr_enable, "%u");
		__HAL_AUX_CONFIG_ENTRY("BTimer Value",
		    dev_config->vp_config[i].tti.btimer_val, "%u");
		__HAL_AUX_CONFIG_ENTRY("Timer AC Enable",
		    dev_config->vp_config[i].tti.timer_ac_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Timer CI Enable",
		    dev_config->vp_config[i].tti.timer_ci_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Timer RI Enable",
		    dev_config->vp_config[i].tti.timer_ri_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Timer Event SF",
		    dev_config->vp_config[i].tti.rtimer_event_sf, "%u");
		__HAL_AUX_CONFIG_ENTRY("RTimer Value",
		    dev_config->vp_config[i].tti.rtimer_val, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Sel",
		    dev_config->vp_config[i].tti.util_sel, "%u");
		__HAL_AUX_CONFIG_ENTRY("LTimer Value",
		    dev_config->vp_config[i].tti.ltimer_val, "%u");
		__HAL_AUX_CONFIG_ENTRY("Tx Frame Count Enable",
		    dev_config->vp_config[i].tti.txfrm_cnt_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Txd Count Enable",
		    dev_config->vp_config[i].tti.txd_cnt_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Range A",
		    dev_config->vp_config[i].tti.urange_a, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Event Count A",
		    dev_config->vp_config[i].tti.uec_a, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Range B",
		    dev_config->vp_config[i].tti.urange_b, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Event Count B",
		    dev_config->vp_config[i].tti.uec_b, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Range C",
		    dev_config->vp_config[i].tti.urange_c, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Event Count C",
		    dev_config->vp_config[i].tti.uec_c, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Event Count D",
		    dev_config->vp_config[i].tti.uec_d, "%u");
		__HAL_AUX_CONFIG_ENTRY("Ufca Interrupt Threshold",
		    dev_config->vp_config[i].tti.ufca_intr_thres, "%u");
		__HAL_AUX_CONFIG_ENTRY("Ufca Low Limit",
		    dev_config->vp_config[i].tti.ufca_lo_lim, "%u");
		__HAL_AUX_CONFIG_ENTRY("Ufca High Limit",
		    dev_config->vp_config[i].tti.ufca_hi_lim, "%u");
		__HAL_AUX_CONFIG_ENTRY("Ufca lbolt period",
		    dev_config->vp_config[i].tti.ufca_lbolt_period, "%u");

		__HAL_AUX_CONFIG_ENTRY("Interrupt Enable",
		    dev_config->vp_config[i].rti.intr_enable, "%u");
		__HAL_AUX_CONFIG_ENTRY("BTimer Value",
		    dev_config->vp_config[i].rti.btimer_val, "%u");
		__HAL_AUX_CONFIG_ENTRY("Timer AC Enable",
		    dev_config->vp_config[i].rti.timer_ac_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Timer CI Enable",
		    dev_config->vp_config[i].rti.timer_ci_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Timer RI Enable",
		    dev_config->vp_config[i].rti.timer_ri_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Timer Event SF",
		    dev_config->vp_config[i].rti.rtimer_event_sf, "%u");
		__HAL_AUX_CONFIG_ENTRY("RTimer Value",
		    dev_config->vp_config[i].rti.rtimer_val, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Sel",
		    dev_config->vp_config[i].rti.util_sel, "%u");
		__HAL_AUX_CONFIG_ENTRY("LTimer Value",
		    dev_config->vp_config[i].rti.ltimer_val, "%u");
		__HAL_AUX_CONFIG_ENTRY("Tx Frame Count Enable",
		    dev_config->vp_config[i].rti.txfrm_cnt_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Txd Count Enable",
		    dev_config->vp_config[i].rti.txd_cnt_en, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Range A",
		    dev_config->vp_config[i].rti.urange_a, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Event Count A",
		    dev_config->vp_config[i].rti.uec_a, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Range B",
		    dev_config->vp_config[i].rti.urange_b, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Event Count B",
		    dev_config->vp_config[i].rti.uec_b, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Range C",
		    dev_config->vp_config[i].rti.urange_c, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Event Count C",
		    dev_config->vp_config[i].rti.uec_c, "%u");
		__HAL_AUX_CONFIG_ENTRY("Util Event Count D",
		    dev_config->vp_config[i].rti.uec_d, "%u");
		__HAL_AUX_CONFIG_ENTRY("Ufca Interrupt Threshold",
		    dev_config->vp_config[i].rti.ufca_intr_thres, "%u");
		__HAL_AUX_CONFIG_ENTRY("Ufca Low Limit",
		    dev_config->vp_config[i].rti.ufca_lo_lim, "%u");
		__HAL_AUX_CONFIG_ENTRY("Ufca High Limit",
		    dev_config->vp_config[i].rti.ufca_hi_lim, "%u");
		__HAL_AUX_CONFIG_ENTRY("Ufca lbolt period",
		    dev_config->vp_config[i].rti.ufca_lbolt_period, "%u");
	}

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	vxge_os_free(hldev->pdev, dev_config,
	    sizeof(vxge_hal_device_config_t));

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_bar0_read - Read and format X3100 BAR0 register.
 * @devh: HAL device handle.
 * @offset: Register offset in the BAR0 space.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read X3100 register from BAR0 space.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_OUT_OF_SPACE - Buffer size is very small.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 *
 * See also: vxge_hal_mgmt_reg_read().
 */
vxge_hal_status_e
vxge_hal_aux_bar0_read(vxge_hal_device_h devh,
    unsigned int offset, int bufsize, char *retbuf,
    int *retsize)
{
	vxge_hal_status_e status;
	u64 retval;

	status = vxge_hal_mgmt_bar0_read(devh, offset, &retval);
	if (status != VXGE_HAL_OK)
		return (status);

	if (bufsize < VXGE_OS_SPRINTF_STRLEN)
		return (VXGE_HAL_ERR_OUT_OF_SPACE);

	*retsize = vxge_os_snprintf(retbuf, bufsize,
	    "0x%04X%c0x%08X%08X\n", offset,
	    VXGE_HAL_AUX_SEPA, (u32) (retval >> 32), (u32) retval);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_bar1_read - Read and format X3100 BAR1 register.
 * @devh: HAL device handle.
 * @offset: Register offset in the BAR1 space.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read X3100 register from BAR1 space.
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_OUT_OF_SPACE - Buffer size is very small.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 *
 * See also: vxge_hal_mgmt_reg_read().
 */
vxge_hal_status_e
vxge_hal_aux_bar1_read(vxge_hal_device_h devh,
    unsigned int offset, int bufsize, char *retbuf,
    int *retsize)
{
	vxge_hal_status_e status;
	u64 retval;

	status = vxge_hal_mgmt_bar1_read(devh, offset, &retval);
	if (status != VXGE_HAL_OK)
		return (status);

	if (bufsize < VXGE_OS_SPRINTF_STRLEN)
		return (VXGE_HAL_ERR_OUT_OF_SPACE);

	*retsize = vxge_os_snprintf(retbuf, bufsize, "0x%04X%c0x%08X%08X\n",
	    offset, VXGE_HAL_AUX_SEPA, (u32) (retval >> 32), (u32) retval);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_bar0_write - Write BAR0 register.
 * @devh: HAL device handle.
 * @offset: Register offset in the BAR0 space.
 * @value: Regsister value (to write).
 *
 * Write BAR0 register.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the BAR space is not
 * valid.
 *
 * See also: vxge_hal_mgmt_reg_write().
 */
vxge_hal_status_e
vxge_hal_aux_bar0_write(vxge_hal_device_h devh,
    unsigned int offset, u64 value)
{
	vxge_hal_status_e status;

	status = vxge_hal_mgmt_bar0_write(devh, offset, value);
	if (status != VXGE_HAL_OK)
		return (status);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_stats_vpath_hw_read - Read vpath hardware statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read vpath hardware statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_hw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_hw_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize)
{
	vxge_hal_status_e status;
	vxge_hal_vpath_stats_hw_info_t hw_info;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(vpath_handle != NULL);

	status = vxge_hal_vpath_hw_stats_enable(vpath_handle);
	if (status != VXGE_HAL_OK)
		return (status);

	status = vxge_hal_vpath_hw_stats_get(vpath_handle, &hw_info);
	if (status != VXGE_HAL_OK)
		return (status);

	__HAL_AUX_ENTRY("ini_num_mwr_sent",
	    hw_info.ini_num_mwr_sent, "%u");
	__HAL_AUX_ENTRY("ini_num_mrd_sent",
	    hw_info.ini_num_mrd_sent, "%u");
	__HAL_AUX_ENTRY("ini_num_cpl_rcvd",
	    hw_info.ini_num_cpl_rcvd, "%u");
	__HAL_AUX_ENTRY("ini_num_mwr_byte_sent",
	    hw_info.ini_num_mwr_byte_sent, "%llu");
	__HAL_AUX_ENTRY("ini_num_cpl_byte_rcvd",
	    hw_info.ini_num_cpl_byte_rcvd, "%llu");
	__HAL_AUX_ENTRY("wrcrdtarb_xoff",
	    hw_info.wrcrdtarb_xoff, "%u");
	__HAL_AUX_ENTRY("rdcrdtarb_xoff",
	    hw_info.rdcrdtarb_xoff, "%u");
	__HAL_AUX_ENTRY("vpath_genstats_count0",
	    hw_info.vpath_genstats_count0, "%u");
	__HAL_AUX_ENTRY("vpath_genstats_count1",
	    hw_info.vpath_genstats_count1, "%u");
	__HAL_AUX_ENTRY("vpath_genstats_count2",
	    hw_info.vpath_genstats_count2, "%u");
	__HAL_AUX_ENTRY("vpath_genstats_count3",
	    hw_info.vpath_genstats_count3, "%u");
	__HAL_AUX_ENTRY("vpath_genstats_count4",
	    hw_info.vpath_genstats_count4, "%u");
	__HAL_AUX_ENTRY("vpath_genstats_count5",
	    hw_info.vpath_genstats_count5, "%u");
	__HAL_AUX_ENTRY("tx_ttl_eth_frms",
	    hw_info.tx_stats.tx_ttl_eth_frms, "%llu");
	__HAL_AUX_ENTRY("tx_ttl_eth_octets",
	    hw_info.tx_stats.tx_ttl_eth_octets, "%llu");
	__HAL_AUX_ENTRY("tx_data_octets",
	    hw_info.tx_stats.tx_data_octets, "%llu");
	__HAL_AUX_ENTRY("tx_mcast_frms",
	    hw_info.tx_stats.tx_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_bcast_frms",
	    hw_info.tx_stats.tx_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_ucast_frms",
	    hw_info.tx_stats.tx_ucast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_tagged_frms",
	    hw_info.tx_stats.tx_tagged_frms, "%llu");
	__HAL_AUX_ENTRY("tx_vld_ip",
	    hw_info.tx_stats.tx_vld_ip, "%llu");
	__HAL_AUX_ENTRY("tx_vld_ip_octets",
	    hw_info.tx_stats.tx_vld_ip_octets, "%llu");
	__HAL_AUX_ENTRY("tx_icmp",
	    hw_info.tx_stats.tx_icmp, "%llu");
	__HAL_AUX_ENTRY("tx_tcp",
	    hw_info.tx_stats.tx_tcp, "%llu");
	__HAL_AUX_ENTRY("tx_rst_tcp",
	    hw_info.tx_stats.tx_rst_tcp, "%llu");
	__HAL_AUX_ENTRY("tx_udp",
	    hw_info.tx_stats.tx_udp, "%llu");
	__HAL_AUX_ENTRY("tx_unknown_protocol",
	    hw_info.tx_stats.tx_unknown_protocol, "%u");
	__HAL_AUX_ENTRY("tx_lost_ip",
	    hw_info.tx_stats.tx_lost_ip, "%u");
	__HAL_AUX_ENTRY("tx_parse_error",
	    hw_info.tx_stats.tx_parse_error, "%u");
	__HAL_AUX_ENTRY("tx_tcp_offload",
	    hw_info.tx_stats.tx_tcp_offload, "%llu");
	__HAL_AUX_ENTRY("tx_retx_tcp_offload",
	    hw_info.tx_stats.tx_retx_tcp_offload, "%llu");
	__HAL_AUX_ENTRY("tx_lost_ip_offload",
	    hw_info.tx_stats.tx_lost_ip_offload, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_eth_frms",
	    hw_info.rx_stats.rx_ttl_eth_frms, "%llu");
	__HAL_AUX_ENTRY("rx_vld_frms",
	    hw_info.rx_stats.rx_vld_frms, "%llu");
	__HAL_AUX_ENTRY("rx_offload_frms",
	    hw_info.rx_stats.rx_offload_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_eth_octets",
	    hw_info.rx_stats.rx_ttl_eth_octets, "%llu");
	__HAL_AUX_ENTRY("rx_data_octets",
	    hw_info.rx_stats.rx_data_octets, "%llu");
	__HAL_AUX_ENTRY("rx_offload_octets",
	    hw_info.rx_stats.rx_offload_octets, "%llu");
	__HAL_AUX_ENTRY("rx_vld_mcast_frms",
	    hw_info.rx_stats.rx_vld_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_vld_bcast_frms",
	    hw_info.rx_stats.rx_vld_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_ucast_frms",
	    hw_info.rx_stats.rx_accepted_ucast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_nucast_frms",
	    hw_info.rx_stats.rx_accepted_nucast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_tagged_frms",
	    hw_info.rx_stats.rx_tagged_frms, "%llu");
	__HAL_AUX_ENTRY("rx_long_frms",
	    hw_info.rx_stats.rx_long_frms, "%llu");
	__HAL_AUX_ENTRY("rx_usized_frms",
	    hw_info.rx_stats.rx_usized_frms, "%llu");
	__HAL_AUX_ENTRY("rx_osized_frms",
	    hw_info.rx_stats.rx_osized_frms, "%llu");
	__HAL_AUX_ENTRY("rx_frag_frms",
	    hw_info.rx_stats.rx_frag_frms, "%llu");
	__HAL_AUX_ENTRY("rx_jabber_frms",
	    hw_info.rx_stats.rx_jabber_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_64_frms",
	    hw_info.rx_stats.rx_ttl_64_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_65_127_frms",
	    hw_info.rx_stats.rx_ttl_65_127_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_128_255_frms",
	    hw_info.rx_stats.rx_ttl_128_255_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_256_511_frms",
	    hw_info.rx_stats.rx_ttl_256_511_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_512_1023_frms",
	    hw_info.rx_stats.rx_ttl_512_1023_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_1024_1518_frms",
	    hw_info.rx_stats.rx_ttl_1024_1518_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_1519_4095_frms",
	    hw_info.rx_stats.rx_ttl_1519_4095_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_4096_8191_frms",
	    hw_info.rx_stats.rx_ttl_4096_8191_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_8192_max_frms",
	    hw_info.rx_stats.rx_ttl_8192_max_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_gt_max_frms",
	    hw_info.rx_stats.rx_ttl_gt_max_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ip",
	    hw_info.rx_stats.rx_ip, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_ip",
	    hw_info.rx_stats.rx_accepted_ip, "%llu");
	__HAL_AUX_ENTRY("rx_ip_octets",
	    hw_info.rx_stats.rx_ip_octets, "%llu");
	__HAL_AUX_ENTRY("rx_err_ip",
	    hw_info.rx_stats.rx_err_ip, "%llu");
	__HAL_AUX_ENTRY("rx_icmp",
	    hw_info.rx_stats.rx_icmp, "%llu");
	__HAL_AUX_ENTRY("rx_tcp",
	    hw_info.rx_stats.rx_tcp, "%llu");
	__HAL_AUX_ENTRY("rx_udp",
	    hw_info.rx_stats.rx_udp, "%llu");
	__HAL_AUX_ENTRY("rx_err_tcp",
	    hw_info.rx_stats.rx_err_tcp, "%llu");
	__HAL_AUX_ENTRY("rx_lost_frms",
	    hw_info.rx_stats.rx_lost_frms, "%llu");
	__HAL_AUX_ENTRY("rx_lost_ip",
	    hw_info.rx_stats.rx_lost_ip, "%llu");
	__HAL_AUX_ENTRY("rx_lost_ip_offload",
	    hw_info.rx_stats.rx_lost_ip_offload, "%llu");
	__HAL_AUX_ENTRY("rx_various_discard",
	    hw_info.rx_stats.rx_various_discard, "%u");
	__HAL_AUX_ENTRY("rx_sleep_discard",
	    hw_info.rx_stats.rx_sleep_discard, "%u");
	__HAL_AUX_ENTRY("rx_red_discard",
	    hw_info.rx_stats.rx_red_discard, "%u");
	__HAL_AUX_ENTRY("rx_queue_full_discard",
	    hw_info.rx_stats.rx_queue_full_discard, "%u");
	__HAL_AUX_ENTRY("rx_mpa_ok_frms",
	    hw_info.rx_stats.rx_mpa_ok_frms, "%llu");
	__HAL_AUX_ENTRY("prog_event_vnum1",
	    hw_info.prog_event_vnum1, "%u");
	__HAL_AUX_ENTRY("prog_event_vnum0",
	    hw_info.prog_event_vnum0, "%u");
	__HAL_AUX_ENTRY("prog_event_vnum3",
	    hw_info.prog_event_vnum3, "%u");
	__HAL_AUX_ENTRY("prog_event_vnum2",
	    hw_info.prog_event_vnum2, "%u");
	__HAL_AUX_ENTRY("rx_multi_cast_frame_discard",
	    hw_info.rx_multi_cast_frame_discard, "%u");
	__HAL_AUX_ENTRY("rx_frm_transferred",
	    hw_info.rx_frm_transferred, "%u");
	__HAL_AUX_ENTRY("rxd_returned",
	    hw_info.rxd_returned, "%u");
	__HAL_AUX_ENTRY("rx_mpa_len_fail_frms",
	    hw_info.rx_mpa_len_fail_frms, "%u");
	__HAL_AUX_ENTRY("rx_mpa_mrk_fail_frms",
	    hw_info.rx_mpa_mrk_fail_frms, "%u");
	__HAL_AUX_ENTRY("rx_mpa_crc_fail_frms",
	    hw_info.rx_mpa_crc_fail_frms, "%u");
	__HAL_AUX_ENTRY("rx_permitted_frms",
	    hw_info.rx_permitted_frms, "%u");
	__HAL_AUX_ENTRY("rx_vp_reset_discarded_frms",
	    hw_info.rx_vp_reset_discarded_frms, "%llu");
	__HAL_AUX_ENTRY("rx_wol_frms",
	    hw_info.rx_wol_frms, "%llu");
	__HAL_AUX_ENTRY("tx_vp_reset_discarded_frms",
	    hw_info.tx_vp_reset_discarded_frms, "%llu");

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_stats_device_hw_read - Read device hardware statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device hardware statistics. This is a subset of stats counters
 * from vxge_hal_device_stats_hw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_device_hw_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize)
{
	u32 i;
	int rsize = 0;
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(devh);

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		__HAL_AUX_ENTRY("H/W stats for vpath id", i, "%u");

		status = vxge_hal_aux_stats_vpath_hw_read(
		    VXGE_HAL_VIRTUAL_PATH_HANDLE(&hldev->virtual_paths[i]),
		    leftsize, ptr, &rsize);

		if (status != VXGE_HAL_OK)
			return (status);

		ptr += rsize;
		leftsize -= rsize;

	}

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

#define	__HAL_AUX_VPATH_SW_COMMON_INFO(prefix, common) {\
	__HAL_AUX_ENTRY(prefix"full_cnt", (common)->full_cnt, "%u");\
	__HAL_AUX_ENTRY(prefix"usage_cnt", (common)->usage_cnt, "%u");\
	__HAL_AUX_ENTRY(prefix"usage_max", (common)->usage_max, "%u");\
	__HAL_AUX_ENTRY(prefix"avg_compl_per_intr_cnt",\
		(common)->avg_compl_per_intr_cnt, "%u");\
	__HAL_AUX_ENTRY(prefix"total_compl_cnt",\
		(common)->total_compl_cnt, "%u");\
}

/*
 * vxge_hal_aux_stats_vpath_sw_fifo_read - Read vpath fifo software statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read vpath fifo software statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_sw_fifo_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_sw_fifo_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize)
{
	u32 i;
	u8 strbuf[256];
	vxge_hal_status_e status;
	vxge_hal_vpath_stats_sw_fifo_info_t *fifo_info;
	vxge_hal_vpath_stats_sw_info_t sw_stats;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(vpath_handle != NULL);

	status = vxge_hal_vpath_sw_stats_get(vpath_handle, &sw_stats);
	if (status != VXGE_HAL_OK)
		return (status);

	fifo_info = &sw_stats.fifo_stats;

	__HAL_AUX_VPATH_SW_COMMON_INFO("fifo_",
	    &fifo_info->common_stats);

	__HAL_AUX_ENTRY("total_posts",
	    fifo_info->total_posts, "%u");
	__HAL_AUX_ENTRY("total_buffers",
	    fifo_info->total_buffers, "%u");
	__HAL_AUX_ENTRY("avg_buffers_per_post",
	    fifo_info->avg_buffers_per_post, "%u");
	__HAL_AUX_ENTRY("copied_frags",
	    fifo_info->copied_frags, "%u");
	__HAL_AUX_ENTRY("copied_buffers",
	    fifo_info->copied_buffers, "%u");
	__HAL_AUX_ENTRY("avg_buffer_size",
	    fifo_info->avg_buffer_size, "%u");
	__HAL_AUX_ENTRY("avg_post_size",
	    fifo_info->avg_post_size, "%u");
	__HAL_AUX_ENTRY("total_frags",
	    fifo_info->total_frags, "%u");
	__HAL_AUX_ENTRY("copied_frags",
	    fifo_info->copied_frags, "%u");
	__HAL_AUX_ENTRY("total_posts_dang_dtrs",
	    fifo_info->total_posts_dang_dtrs, "%u");
	__HAL_AUX_ENTRY("total_posts_dang_frags",
	    fifo_info->total_posts_dang_frags, "%u");

	for (i = 0; i < 16; i++) {
		(void) vxge_os_snprintf((char *) strbuf,
		    sizeof(strbuf), "txd_t_code_err_cnt[%d]", i);
		__HAL_AUX_ENTRY(strbuf,
		    fifo_info->txd_t_code_err_cnt[i], "%u");
	}

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_stats_vpath_sw_ring_read - Read vpath ring software statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read vpath ring software statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_sw_ring_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_sw_ring_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize)
{
	u32 i;
	u8 strbuf[256];
	vxge_hal_status_e status;
	vxge_hal_vpath_stats_sw_ring_info_t *ring_info;
	vxge_hal_vpath_stats_sw_info_t sw_stats;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(vpath_handle != NULL);

	status = vxge_hal_vpath_sw_stats_get(vpath_handle, &sw_stats);
	if (status != VXGE_HAL_OK)
		return (status);

	ring_info = &sw_stats.ring_stats;

	__HAL_AUX_VPATH_SW_COMMON_INFO("ring_",
	    &ring_info->common_stats);

	for (i = 0; i < 16; i++) {
		(void) vxge_os_snprintf((char *) strbuf,
		    sizeof(strbuf), "rxd_t_code_err_cnt[%d]", i);
		__HAL_AUX_ENTRY(strbuf,
		    ring_info->rxd_t_code_err_cnt[i], "%u");
	}

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}


/*
 * vxge_hal_aux_stats_vpath_sw_err_read - Read vpath err software statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read vpath err software statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_sw_err_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_sw_err_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize)
{
	vxge_hal_vpath_stats_sw_err_t *err_info;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(vpath_handle != NULL);

	err_info = &vp->vpath->sw_stats->error_stats;

	__HAL_AUX_ENTRY("unknown_alarms",
	    err_info->unknown_alarms, "%u");
	__HAL_AUX_ENTRY("network_sustained_fault",
	    err_info->network_sustained_fault, "%u");
	__HAL_AUX_ENTRY("network_sustained_ok",
	    err_info->network_sustained_ok, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo0_overwrite",
	    err_info->kdfcctl_fifo0_overwrite, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo0_poison",
	    err_info->kdfcctl_fifo0_poison, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo0_dma_error",
	    err_info->kdfcctl_fifo0_dma_error, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo1_overwrite",
	    err_info->kdfcctl_fifo1_overwrite, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo1_poison",
	    err_info->kdfcctl_fifo1_poison, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo1_dma_error",
	    err_info->kdfcctl_fifo1_dma_error, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo2_overwrite",
	    err_info->kdfcctl_fifo2_overwrite, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo2_poison",
	    err_info->kdfcctl_fifo2_poison, "%u");
	__HAL_AUX_ENTRY("kdfcctl_fifo2_dma_error",
	    err_info->kdfcctl_fifo2_dma_error, "%u");
	__HAL_AUX_ENTRY("dblgen_fifo0_overflow",
	    err_info->dblgen_fifo0_overflow, "%u");
	__HAL_AUX_ENTRY("dblgen_fifo1_overflow",
	    err_info->dblgen_fifo1_overflow, "%u");
	__HAL_AUX_ENTRY("dblgen_fifo2_overflow",
	    err_info->dblgen_fifo2_overflow, "%u");
	__HAL_AUX_ENTRY("statsb_pif_chain_error",
	    err_info->statsb_pif_chain_error, "%u");
	__HAL_AUX_ENTRY("statsb_drop_timeout",
	    err_info->statsb_drop_timeout, "%u");
	__HAL_AUX_ENTRY("target_illegal_access",
	    err_info->target_illegal_access, "%u");
	__HAL_AUX_ENTRY("ini_serr_det",
	    err_info->ini_serr_det, "%u");
	__HAL_AUX_ENTRY("pci_config_status_err",
	    err_info->pci_config_status_err, "%u");
	__HAL_AUX_ENTRY("pci_config_uncor_err",
	    err_info->pci_config_uncor_err, "%u");
	__HAL_AUX_ENTRY("pci_config_cor_err",
	    err_info->pci_config_cor_err, "%u");
	__HAL_AUX_ENTRY("mrpcim_to_vpath_alarms",
	    err_info->mrpcim_to_vpath_alarms, "%u");
	__HAL_AUX_ENTRY("srpcim_to_vpath_alarms",
	    err_info->srpcim_to_vpath_alarms, "%u");
	__HAL_AUX_ENTRY("srpcim_msg_to_vpath",
	    err_info->srpcim_msg_to_vpath, "%u");
	__HAL_AUX_ENTRY("prc_ring_bumps",
	    err_info->prc_ring_bumps, "%u");
	__HAL_AUX_ENTRY("prc_rxdcm_sc_err",
	    err_info->prc_rxdcm_sc_err, "%u");
	__HAL_AUX_ENTRY("prc_rxdcm_sc_abort",
	    err_info->prc_rxdcm_sc_abort, "%u");
	__HAL_AUX_ENTRY("prc_quanta_size_err",
	    err_info->prc_quanta_size_err, "%u");

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_stats_vpath_sw_read - Read vpath soft statistics.
 * @vpath_handle: HAL Vpath handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device hardware statistics. This is a subset of stats counters
 * from vxge_hal_vpath_stats_sw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_vpath_sw_read(
    vxge_hal_vpath_h vpath_handle,
    int bufsize,
    char *retbuf,
    int *retsize)
{
	int rsize = 0;
	vxge_hal_status_e status;
	vxge_hal_vpath_stats_sw_info_t *sw_info;
	__hal_vpath_handle_t *vp = (__hal_vpath_handle_t *) vpath_handle;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(vpath_handle != NULL);

	sw_info = vp->vpath->sw_stats;

	__HAL_AUX_ENTRY("soft_reset_cnt", sw_info->soft_reset_cnt, "%u");


	status = vxge_hal_aux_stats_vpath_sw_err_read(vpath_handle,
	    leftsize, ptr, &rsize);
	if (status != VXGE_HAL_OK)
		return (status);

	ptr += rsize;
	leftsize -= rsize;

	status = vxge_hal_aux_stats_vpath_sw_ring_read(vpath_handle,
	    leftsize, ptr, &rsize);
	if (status != VXGE_HAL_OK)
		return (status);

	ptr += rsize;
	leftsize -= rsize;

	status = vxge_hal_aux_stats_vpath_sw_fifo_read(vpath_handle,
	    leftsize, ptr, &rsize);
	if (status != VXGE_HAL_OK)
		return (status);


	leftsize -= rsize;

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_stats_device_sw_read - Read device software statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device software statistics. This is a subset of stats counters
 * from vxge_hal_device_stats_sw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_device_sw_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize)
{
	u32 i;
	int rsize = 0;
	vxge_hal_status_e status;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(devh);

	__HAL_AUX_ENTRY("not_traffic_intr_cnt",
	    hldev->header.not_traffic_intr_cnt, "%u");
	__HAL_AUX_ENTRY("traffic_intr_cnt",
	    hldev->header.traffic_intr_cnt, "%u");
	__HAL_AUX_ENTRY("total_intr_cnt",
	    hldev->header.not_traffic_intr_cnt +
	    hldev->header.traffic_intr_cnt, "%u");
	__HAL_AUX_ENTRY("soft_reset_cnt",
	    hldev->stats.sw_dev_info_stats.soft_reset_cnt, "%u");

	for (i = 0; i < VXGE_HAL_MAX_VIRTUAL_PATHS; i++) {

		if (!(hldev->vpaths_deployed & mBIT(i)))
			continue;

		__HAL_AUX_ENTRY("S/W stats for vpath id", i, "%u");

		status = vxge_hal_aux_stats_vpath_sw_read(
		    VXGE_HAL_VIRTUAL_PATH_HANDLE(&hldev->virtual_paths[i]),
		    leftsize, ptr, &rsize);

		if (status != VXGE_HAL_OK)
			return (status);

		ptr += rsize;
		leftsize -= rsize;
	}

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_stats_device_sw_err_read - Read device software error statistics
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device software error statistics. This is a subset of stats counters
 * from vxge_hal_device_stats_sw_info_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_device_sw_err_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize)
{
	vxge_hal_device_stats_sw_err_t *sw_err;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(devh);

	sw_err = &hldev->stats.sw_dev_err_stats;

	__HAL_AUX_ENTRY("mrpcim_alarms", sw_err->mrpcim_alarms, "%u");
	__HAL_AUX_ENTRY("srpcim_alarms", sw_err->srpcim_alarms, "%u");
	__HAL_AUX_ENTRY("vpath_alarms", sw_err->vpath_alarms, "%u");

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_stats_device_read - Read device statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device statistics. This is a subset of stats counters
 * from vxge_hal_device_stats_t {}.
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_device_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize)
{
	char *ptr = retbuf;
	int rsize = 0, leftsize = bufsize;
	vxge_hal_status_e status;

	vxge_assert(devh);

	status = vxge_hal_aux_stats_device_hw_read(devh,
	    leftsize, ptr, &rsize);
	if (status != VXGE_HAL_OK)
		return (status);

	ptr += rsize;
	leftsize -= rsize;

	status = vxge_hal_aux_stats_device_sw_err_read(devh,
	    leftsize, ptr, &rsize);
	if (status != VXGE_HAL_OK)
		return (status);

	ptr += rsize;
	leftsize -= rsize;

	status = vxge_hal_aux_stats_device_sw_read(devh,
	    leftsize, ptr, &rsize);
	if (status != VXGE_HAL_OK)
		return (status);

	leftsize -= rsize;

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_stats_xpak_read - Read device xpak statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device xpak statistics. This is valid for function 0 device only
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_xpak_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize)
{
	u32 i;
	vxge_hal_mrpcim_xpak_stats_t *xpak_stats;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM))
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	for (i = 0; i < VXGE_HAL_MAC_MAX_WIRE_PORTS; i++) {

		xpak_stats = &hldev->mrpcim->xpak_stats[i];

		__HAL_AUX_ENTRY("Wire Port Id : ", i, "%u");
		__HAL_AUX_ENTRY("alarm_transceiver_temp_high",
		    xpak_stats->excess_bias_current, "%u");
		__HAL_AUX_ENTRY("alarm_transceiver_temp_high",
		    xpak_stats->excess_laser_output, "%u");
		__HAL_AUX_ENTRY("alarm_transceiver_temp_high",
		    xpak_stats->excess_temp, "%u");
		__HAL_AUX_ENTRY("alarm_transceiver_temp_high",
		    xpak_stats->alarm_transceiver_temp_high, "%u");
		__HAL_AUX_ENTRY("alarm_transceiver_temp_low",
		    xpak_stats->alarm_transceiver_temp_low, "%u");
		__HAL_AUX_ENTRY("alarm_laser_bias_current_high",
		    xpak_stats->alarm_laser_bias_current_high, "%u");
		__HAL_AUX_ENTRY("alarm_laser_bias_current_low",
		    xpak_stats->alarm_laser_bias_current_low, "%u");
		__HAL_AUX_ENTRY("alarm_laser_output_power_high",
		    xpak_stats->alarm_laser_output_power_high, "%u");
		__HAL_AUX_ENTRY("alarm_laser_output_power_low",
		    xpak_stats->alarm_laser_output_power_low, "%u");
		__HAL_AUX_ENTRY("warn_transceiver_temp_high",
		    xpak_stats->warn_transceiver_temp_high, "%u");
		__HAL_AUX_ENTRY("warn_transceiver_temp_low",
		    xpak_stats->warn_transceiver_temp_low, "%u");
		__HAL_AUX_ENTRY("warn_laser_bias_current_high",
		    xpak_stats->warn_laser_bias_current_high, "%u");
		__HAL_AUX_ENTRY("warn_laser_bias_current_low",
		    xpak_stats->warn_laser_bias_current_low, "%u");
		__HAL_AUX_ENTRY("warn_laser_output_power_high",
		    xpak_stats->warn_laser_output_power_high, "%u");
		__HAL_AUX_ENTRY("warn_laser_output_power_low",
		    xpak_stats->warn_laser_output_power_low, "%u");

	}

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}
/*
 * vxge_hal_aux_stats_mrpcim_read - Read device mrpcim statistics.
 * @devh: HAL device handle.
 * @bufsize: Buffer size.
 * @retbuf: Buffer pointer.
 * @retsize: Size of the result. Cannot be greater than @bufsize.
 *
 * Read device mrpcim statistics. This is valid for function 0 device only
 *
 */
vxge_hal_status_e
vxge_hal_aux_stats_mrpcim_read(vxge_hal_device_h devh,
    int bufsize, char *retbuf, int *retsize)
{
	vxge_hal_status_e status;
	vxge_hal_mrpcim_stats_hw_info_t mrpcim_info;
	__hal_device_t *hldev = (__hal_device_t *) devh;

	__HAL_AUX_ENTRY_DECLARE(bufsize, retbuf);

	vxge_assert(devh);

	if (!(hldev->access_rights & VXGE_HAL_DEVICE_ACCESS_RIGHT_MRPCIM))
		return (VXGE_HAL_ERR_PRIVILAGED_OPEARATION);

	status = vxge_hal_mrpcim_stats_enable(devh);
	if (status != VXGE_HAL_OK)
		return (status);

	status = vxge_hal_mrpcim_stats_get(devh, &mrpcim_info);
	if (status != VXGE_HAL_OK)
		return (status);

	__HAL_AUX_ENTRY("pic_ini_rd_drop", mrpcim_info.pic_ini_rd_drop, "%u");
	__HAL_AUX_ENTRY("pic_ini_wr_drop", mrpcim_info.pic_ini_wr_drop, "%u");

	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane0",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[0].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane1",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[1].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane2",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[2].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane3",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[3].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane4",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[4].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane5",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[5].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane6",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[6].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane7",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[7].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane8",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[8].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane9",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[9].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane10",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[10].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane11",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[11].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane12",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[12].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane13",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[13].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane14",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[14].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane15",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[15].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_ph_crdt_depleted_vplane16",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[16].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");

	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane0",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[0].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane1",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[1].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane2",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[2].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane3",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[3].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane4",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[4].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane5",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[5].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane6",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[6].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane7",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[7].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane8",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[8].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane9",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[9].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane10",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[10].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane11",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[11].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane12",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[12].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane13",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[13].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane14",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[14].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane15",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[15].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_wrcrdtarb_pd_crdt_depleted_vplane16",
	    mrpcim_info.pic_wrcrdtarb_pd_crdt_depleted_vplane[16].
	    pic_wrcrdtarb_pd_crdt_depleted, "%u");

	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane0",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[0].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane1",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[1].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane2",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[2].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane3",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[3].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane4",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[4].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane5",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[5].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane6",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[6].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane7",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[7].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane8",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[8].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane9",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[9].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane10",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[10].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane11",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[11].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane12",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[12].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane13",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[13].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane14",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[14].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane15",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[15].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");
	__HAL_AUX_ENTRY("pic_rdcrdtarb_nph_crdt_depleted_vplane16",
	    mrpcim_info.pic_rdcrdtarb_nph_crdt_depleted_vplane[16].
	    pic_rdcrdtarb_nph_crdt_depleted, "%u");

	__HAL_AUX_ENTRY("pic_ini_rd_vpin_drop",
	    mrpcim_info.pic_ini_rd_vpin_drop, "%u");
	__HAL_AUX_ENTRY("pic_ini_wr_vpin_drop",
	    mrpcim_info.pic_ini_wr_vpin_drop, "%u");
	__HAL_AUX_ENTRY("pic_genstats_count0",
	    mrpcim_info.pic_genstats_count0, "%u");
	__HAL_AUX_ENTRY("pic_genstats_count1",
	    mrpcim_info.pic_genstats_count1, "%u");
	__HAL_AUX_ENTRY("pic_genstats_count2",
	    mrpcim_info.pic_genstats_count2, "%u");
	__HAL_AUX_ENTRY("pic_genstats_count3",
	    mrpcim_info.pic_genstats_count3, "%u");
	__HAL_AUX_ENTRY("pic_genstats_count4",
	    mrpcim_info.pic_genstats_count4, "%u");
	__HAL_AUX_ENTRY("pic_genstats_count5",
	    mrpcim_info.pic_genstats_count5, "%u");
	__HAL_AUX_ENTRY("pci_rstdrop_cpl",
	    mrpcim_info.pci_rstdrop_cpl, "%u");
	__HAL_AUX_ENTRY("pci_rstdrop_msg",
	    mrpcim_info.pci_rstdrop_msg, "%u");
	__HAL_AUX_ENTRY("pci_rstdrop_client1",
	    mrpcim_info.pci_rstdrop_client1, "%u");
	__HAL_AUX_ENTRY("pci_rstdrop_client0",
	    mrpcim_info.pci_rstdrop_client0, "%u");
	__HAL_AUX_ENTRY("pci_rstdrop_client2",
	    mrpcim_info.pci_rstdrop_client2, "%u");

	__HAL_AUX_ENTRY("pci_depl_cplh_vplane0",
	    mrpcim_info.pci_depl_h_vplane[0].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane0",
	    mrpcim_info.pci_depl_h_vplane[0].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane0",
	    mrpcim_info.pci_depl_h_vplane[0].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane1",
	    mrpcim_info.pci_depl_h_vplane[1].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane1",
	    mrpcim_info.pci_depl_h_vplane[1].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane1",
	    mrpcim_info.pci_depl_h_vplane[1].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane2",
	    mrpcim_info.pci_depl_h_vplane[2].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane2",
	    mrpcim_info.pci_depl_h_vplane[2].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane2",
	    mrpcim_info.pci_depl_h_vplane[2].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane3",
	    mrpcim_info.pci_depl_h_vplane[3].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane3",
	    mrpcim_info.pci_depl_h_vplane[3].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane3",
	    mrpcim_info.pci_depl_h_vplane[3].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane4",
	    mrpcim_info.pci_depl_h_vplane[4].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane4",
	    mrpcim_info.pci_depl_h_vplane[4].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane4",
	    mrpcim_info.pci_depl_h_vplane[4].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane5",
	    mrpcim_info.pci_depl_h_vplane[5].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane5",
	    mrpcim_info.pci_depl_h_vplane[5].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane5",
	    mrpcim_info.pci_depl_h_vplane[5].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane6",
	    mrpcim_info.pci_depl_h_vplane[6].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane6",
	    mrpcim_info.pci_depl_h_vplane[6].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane6",
	    mrpcim_info.pci_depl_h_vplane[6].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane7",
	    mrpcim_info.pci_depl_h_vplane[7].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane7",
	    mrpcim_info.pci_depl_h_vplane[7].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane7",
	    mrpcim_info.pci_depl_h_vplane[7].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane8",
	    mrpcim_info.pci_depl_h_vplane[8].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane8",
	    mrpcim_info.pci_depl_h_vplane[8].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane8",
	    mrpcim_info.pci_depl_h_vplane[8].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane9",
	    mrpcim_info.pci_depl_h_vplane[9].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane9",
	    mrpcim_info.pci_depl_h_vplane[9].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane9",
	    mrpcim_info.pci_depl_h_vplane[9].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane10",
	    mrpcim_info.pci_depl_h_vplane[10].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane10",
	    mrpcim_info.pci_depl_h_vplane[10].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane10",
	    mrpcim_info.pci_depl_h_vplane[10].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane11",
	    mrpcim_info.pci_depl_h_vplane[11].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane11",
	    mrpcim_info.pci_depl_h_vplane[11].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane11",
	    mrpcim_info.pci_depl_h_vplane[11].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane12",
	    mrpcim_info.pci_depl_h_vplane[12].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane12",
	    mrpcim_info.pci_depl_h_vplane[12].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane12",
	    mrpcim_info.pci_depl_h_vplane[12].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane13",
	    mrpcim_info.pci_depl_h_vplane[13].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane13",
	    mrpcim_info.pci_depl_h_vplane[13].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane13",
	    mrpcim_info.pci_depl_h_vplane[13].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane14",
	    mrpcim_info.pci_depl_h_vplane[14].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane14",
	    mrpcim_info.pci_depl_h_vplane[14].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane14",
	    mrpcim_info.pci_depl_h_vplane[14].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane15",
	    mrpcim_info.pci_depl_h_vplane[15].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane15",
	    mrpcim_info.pci_depl_h_vplane[15].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane15",
	    mrpcim_info.pci_depl_h_vplane[15].pci_depl_ph, "%u");
	__HAL_AUX_ENTRY("pci_depl_cplh_vplane16",
	    mrpcim_info.pci_depl_h_vplane[16].pci_depl_cplh, "%u");
	__HAL_AUX_ENTRY("pci_depl_nph_vplane16",
	    mrpcim_info.pci_depl_h_vplane[16].pci_depl_nph, "%u");
	__HAL_AUX_ENTRY("pci_depl_ph_vplane16",
	    mrpcim_info.pci_depl_h_vplane[16].pci_depl_ph, "%u");

	__HAL_AUX_ENTRY("pci_depl_cpld_vplane0",
	    mrpcim_info.pci_depl_d_vplane[0].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane0",
	    mrpcim_info.pci_depl_d_vplane[0].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane0",
	    mrpcim_info.pci_depl_d_vplane[0].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane1",
	    mrpcim_info.pci_depl_d_vplane[1].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane1",
	    mrpcim_info.pci_depl_d_vplane[1].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane1",
	    mrpcim_info.pci_depl_d_vplane[1].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane2",
	    mrpcim_info.pci_depl_d_vplane[2].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane2",
	    mrpcim_info.pci_depl_d_vplane[2].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane2",
	    mrpcim_info.pci_depl_d_vplane[2].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane3",
	    mrpcim_info.pci_depl_d_vplane[3].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane3",
	    mrpcim_info.pci_depl_d_vplane[3].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane3",
	    mrpcim_info.pci_depl_d_vplane[3].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane4",
	    mrpcim_info.pci_depl_d_vplane[4].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane4",
	    mrpcim_info.pci_depl_d_vplane[4].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane4",
	    mrpcim_info.pci_depl_d_vplane[4].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane5",
	    mrpcim_info.pci_depl_d_vplane[5].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane5",
	    mrpcim_info.pci_depl_d_vplane[5].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane5",
	    mrpcim_info.pci_depl_d_vplane[5].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane6",
	    mrpcim_info.pci_depl_d_vplane[6].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane6",
	    mrpcim_info.pci_depl_d_vplane[6].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane6",
	    mrpcim_info.pci_depl_d_vplane[6].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane7",
	    mrpcim_info.pci_depl_d_vplane[7].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane7",
	    mrpcim_info.pci_depl_d_vplane[7].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane7",
	    mrpcim_info.pci_depl_d_vplane[7].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane8",
	    mrpcim_info.pci_depl_d_vplane[8].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane8",
	    mrpcim_info.pci_depl_d_vplane[8].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane8",
	    mrpcim_info.pci_depl_d_vplane[8].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane9",
	    mrpcim_info.pci_depl_d_vplane[9].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane9",
	    mrpcim_info.pci_depl_d_vplane[9].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane9",
	    mrpcim_info.pci_depl_d_vplane[9].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane10",
	    mrpcim_info.pci_depl_d_vplane[10].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane10",
	    mrpcim_info.pci_depl_d_vplane[10].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane10",
	    mrpcim_info.pci_depl_d_vplane[10].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane11",
	    mrpcim_info.pci_depl_d_vplane[11].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane11",
	    mrpcim_info.pci_depl_d_vplane[11].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane11",
	    mrpcim_info.pci_depl_d_vplane[11].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane12",
	    mrpcim_info.pci_depl_d_vplane[12].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane12",
	    mrpcim_info.pci_depl_d_vplane[12].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane12",
	    mrpcim_info.pci_depl_d_vplane[12].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane13",
	    mrpcim_info.pci_depl_d_vplane[13].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane13",
	    mrpcim_info.pci_depl_d_vplane[13].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane13",
	    mrpcim_info.pci_depl_d_vplane[13].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane14",
	    mrpcim_info.pci_depl_d_vplane[14].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane14",
	    mrpcim_info.pci_depl_d_vplane[14].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane14",
	    mrpcim_info.pci_depl_d_vplane[14].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane15",
	    mrpcim_info.pci_depl_d_vplane[15].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane15",
	    mrpcim_info.pci_depl_d_vplane[15].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane15",
	    mrpcim_info.pci_depl_d_vplane[15].pci_depl_pd, "%u");
	__HAL_AUX_ENTRY("pci_depl_cpld_vplane16",
	    mrpcim_info.pci_depl_d_vplane[16].pci_depl_cpld, "%u");
	__HAL_AUX_ENTRY("pci_depl_npd_vplane16",
	    mrpcim_info.pci_depl_d_vplane[16].pci_depl_npd, "%u");
	__HAL_AUX_ENTRY("pci_depl_pd_vplane16",
	    mrpcim_info.pci_depl_d_vplane[16].pci_depl_pd, "%u");

	__HAL_AUX_ENTRY("tx_ttl_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_ttl_frms, "%llu");
	__HAL_AUX_ENTRY("tx_ttl_octets_PORT0",
	    mrpcim_info.xgmac_port[0].tx_ttl_octets, "%llu");
	__HAL_AUX_ENTRY("tx_data_octets_PORT0",
	    mrpcim_info.xgmac_port[0].tx_data_octets, "%llu");
	__HAL_AUX_ENTRY("tx_mcast_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_bcast_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_ucast_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_ucast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_tagged_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_tagged_frms, "%llu");
	__HAL_AUX_ENTRY("tx_vld_ip_PORT0",
	    mrpcim_info.xgmac_port[0].tx_vld_ip, "%llu");
	__HAL_AUX_ENTRY("tx_vld_ip_octets_PORT0",
	    mrpcim_info.xgmac_port[0].tx_vld_ip_octets, "%llu");
	__HAL_AUX_ENTRY("tx_icmp_PORT0",
	    mrpcim_info.xgmac_port[0].tx_icmp, "%llu");
	__HAL_AUX_ENTRY("tx_tcp_PORT0",
	    mrpcim_info.xgmac_port[0].tx_tcp, "%llu");
	__HAL_AUX_ENTRY("tx_rst_tcp_PORT0",
	    mrpcim_info.xgmac_port[0].tx_rst_tcp, "%llu");
	__HAL_AUX_ENTRY("tx_udp_PORT0",
	    mrpcim_info.xgmac_port[0].tx_udp, "%llu");
	__HAL_AUX_ENTRY("tx_parse_error_PORT0",
	    mrpcim_info.xgmac_port[0].tx_parse_error, "%u");
	__HAL_AUX_ENTRY("tx_unknown_protocol_PORT0",
	    mrpcim_info.xgmac_port[0].tx_unknown_protocol, "%u");
	__HAL_AUX_ENTRY("tx_pause_ctrl_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_pause_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("tx_marker_pdu_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_marker_pdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_lacpdu_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_lacpdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_drop_ip_PORT0",
	    mrpcim_info.xgmac_port[0].tx_drop_ip, "%u");
	__HAL_AUX_ENTRY("tx_marker_resp_pdu_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_marker_resp_pdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_char2_match_PORT0",
	    mrpcim_info.xgmac_port[0].tx_xgmii_char2_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_char1_match_PORT0",
	    mrpcim_info.xgmac_port[0].tx_xgmii_char1_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_column2_match_PORT0",
	    mrpcim_info.xgmac_port[0].tx_xgmii_column2_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_column1_match_PORT0",
	    mrpcim_info.xgmac_port[0].tx_xgmii_column1_match, "%u");
	__HAL_AUX_ENTRY("tx_any_err_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_any_err_frms, "%u");
	__HAL_AUX_ENTRY("tx_drop_frms_PORT0",
	    mrpcim_info.xgmac_port[0].tx_drop_frms, "%u");
	__HAL_AUX_ENTRY("rx_ttl_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_vld_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_vld_frms, "%llu");
	__HAL_AUX_ENTRY("rx_offload_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_offload_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_octets_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_octets, "%llu");
	__HAL_AUX_ENTRY("rx_data_octets_PORT0",
	    mrpcim_info.xgmac_port[0].rx_data_octets, "%llu");
	__HAL_AUX_ENTRY("rx_offload_octets_PORT0",
	    mrpcim_info.xgmac_port[0].rx_offload_octets, "%llu");
	__HAL_AUX_ENTRY("rx_vld_mcast_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_vld_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_vld_bcast_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_vld_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_ucast_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_accepted_ucast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_nucast_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_accepted_nucast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_tagged_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_tagged_frms, "%llu");
	__HAL_AUX_ENTRY("rx_long_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_long_frms, "%llu");
	__HAL_AUX_ENTRY("rx_usized_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_usized_frms, "%llu");
	__HAL_AUX_ENTRY("rx_osized_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_osized_frms, "%llu");
	__HAL_AUX_ENTRY("rx_frag_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_frag_frms, "%llu");
	__HAL_AUX_ENTRY("rx_jabber_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_jabber_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_64_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_64_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_65_127_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_65_127_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_128_255_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_128_255_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_256_511_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_256_511_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_512_1023_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_512_1023_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_1024_1518_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_1024_1518_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_1519_4095_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_1519_4095_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_4096_8191_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_4096_8191_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_8192_max_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_8192_max_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_gt_max_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ttl_gt_max_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ip_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ip, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_ip_PORT0",
	    mrpcim_info.xgmac_port[0].rx_accepted_ip, "%llu");
	__HAL_AUX_ENTRY("rx_ip_octets_PORT0",
	    mrpcim_info.xgmac_port[0].rx_ip_octets, "%llu");
	__HAL_AUX_ENTRY("rx_err_ip_PORT0",
	    mrpcim_info.xgmac_port[0].rx_err_ip, "%llu");
	__HAL_AUX_ENTRY("rx_icmp_PORT0",
	    mrpcim_info.xgmac_port[0].rx_icmp, "%llu");
	__HAL_AUX_ENTRY("rx_tcp_PORT0",
	    mrpcim_info.xgmac_port[0].rx_tcp, "%llu");
	__HAL_AUX_ENTRY("rx_udp_PORT0",
	    mrpcim_info.xgmac_port[0].rx_udp, "%llu");
	__HAL_AUX_ENTRY("rx_err_tcp_PORT0",
	    mrpcim_info.xgmac_port[0].rx_err_tcp, "%llu");
	__HAL_AUX_ENTRY("rx_pause_cnt_PORT0",
	    mrpcim_info.xgmac_port[0].rx_pause_count, "%llu");
	__HAL_AUX_ENTRY("rx_pause_ctrl_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_pause_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_unsup_ctrl_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_unsup_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_fcs_err_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_fcs_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_in_rng_len_err_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_in_rng_len_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_out_rng_len_err_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_out_rng_len_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_drop_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_drop_frms, "%llu");
	__HAL_AUX_ENTRY("rx_discarded_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_discarded_frms, "%llu");
	__HAL_AUX_ENTRY("rx_drop_ip_PORT0",
	    mrpcim_info.xgmac_port[0].rx_drop_ip, "%llu");
	__HAL_AUX_ENTRY("rx_drp_udp_PORT0",
	    mrpcim_info.xgmac_port[0].rx_drop_udp, "%llu");
	__HAL_AUX_ENTRY("rx_marker_pdu_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_marker_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_lacpdu_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_lacpdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_unknown_pdu_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_unknown_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_marker_resp_pdu_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_marker_resp_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_fcs_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_fcs_discard, "%u");
	__HAL_AUX_ENTRY("rx_illegal_pdu_frms_PORT0",
	    mrpcim_info.xgmac_port[0].rx_illegal_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_switch_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_switch_discard, "%u");
	__HAL_AUX_ENTRY("rx_len_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_len_discard, "%u");
	__HAL_AUX_ENTRY("rx_rpa_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_rpa_discard, "%u");
	__HAL_AUX_ENTRY("rx_l2_mgmt_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_l2_mgmt_discard, "%u");
	__HAL_AUX_ENTRY("rx_rts_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_rts_discard, "%u");
	__HAL_AUX_ENTRY("rx_trash_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_trash_discard, "%u");
	__HAL_AUX_ENTRY("rx_buff_full_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_buff_full_discard, "%u");
	__HAL_AUX_ENTRY("rx_red_discard_PORT0",
	    mrpcim_info.xgmac_port[0].rx_red_discard, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_ctrl_err_cnt_PORT0",
	    mrpcim_info.xgmac_port[0].rx_xgmii_ctrl_err_cnt, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_data_err_cnt_PORT0",
	    mrpcim_info.xgmac_port[0].rx_xgmii_data_err_cnt, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_char1_match_PORT0",
	    mrpcim_info.xgmac_port[0].rx_xgmii_char1_match, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_err_sym_PORT0",
	    mrpcim_info.xgmac_port[0].rx_xgmii_err_sym, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_column1_match_PORT0",
	    mrpcim_info.xgmac_port[0].rx_xgmii_column1_match, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_char2_match_PORT0",
	    mrpcim_info.xgmac_port[0].rx_xgmii_char2_match, "%u");
	__HAL_AUX_ENTRY("rx_local_fault_PORT0",
	    mrpcim_info.xgmac_port[0].rx_local_fault, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_column2_match_PORT0",
	    mrpcim_info.xgmac_port[0].rx_xgmii_column2_match, "%u");
	__HAL_AUX_ENTRY("rx_jettison_PORT0",
	    mrpcim_info.xgmac_port[0].rx_jettison, "%u");
	__HAL_AUX_ENTRY("rx_remote_fault_PORT0",
	    mrpcim_info.xgmac_port[0].rx_remote_fault, "%u");

	__HAL_AUX_ENTRY("tx_ttl_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_ttl_frms, "%llu");
	__HAL_AUX_ENTRY("tx_ttl_octets_PORT1",
	    mrpcim_info.xgmac_port[1].tx_ttl_octets, "%llu");
	__HAL_AUX_ENTRY("tx_data_octets_PORT1",
	    mrpcim_info.xgmac_port[1].tx_data_octets, "%llu");
	__HAL_AUX_ENTRY("tx_mcast_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_bcast_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_ucast_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_ucast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_tagged_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_tagged_frms, "%llu");
	__HAL_AUX_ENTRY("tx_vld_ip_PORT1",
	    mrpcim_info.xgmac_port[1].tx_vld_ip, "%llu");
	__HAL_AUX_ENTRY("tx_vld_ip_octets_PORT1",
	    mrpcim_info.xgmac_port[1].tx_vld_ip_octets, "%llu");
	__HAL_AUX_ENTRY("tx_icmp_PORT1",
	    mrpcim_info.xgmac_port[1].tx_icmp, "%llu");
	__HAL_AUX_ENTRY("tx_tcp_PORT1",
	    mrpcim_info.xgmac_port[1].tx_tcp, "%llu");
	__HAL_AUX_ENTRY("tx_rst_tcp_PORT1",
	    mrpcim_info.xgmac_port[1].tx_rst_tcp, "%llu");
	__HAL_AUX_ENTRY("tx_udp_PORT1",
	    mrpcim_info.xgmac_port[1].tx_udp, "%llu");
	__HAL_AUX_ENTRY("tx_parse_error_PORT1",
	    mrpcim_info.xgmac_port[1].tx_parse_error, "%u");
	__HAL_AUX_ENTRY("tx_unknown_protocol_PORT1",
	    mrpcim_info.xgmac_port[1].tx_unknown_protocol, "%u");
	__HAL_AUX_ENTRY("tx_pause_ctrl_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_pause_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("tx_marker_pdu_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_marker_pdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_lacpdu_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_lacpdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_drop_ip_PORT1",
	    mrpcim_info.xgmac_port[1].tx_drop_ip, "%u");
	__HAL_AUX_ENTRY("tx_marker_resp_pdu_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_marker_resp_pdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_char2_match_PORT1",
	    mrpcim_info.xgmac_port[1].tx_xgmii_char2_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_char1_match_PORT1",
	    mrpcim_info.xgmac_port[1].tx_xgmii_char1_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_column2_match_PORT1",
	    mrpcim_info.xgmac_port[1].tx_xgmii_column2_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_column1_match_PORT1",
	    mrpcim_info.xgmac_port[1].tx_xgmii_column1_match, "%u");
	__HAL_AUX_ENTRY("tx_any_err_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_any_err_frms, "%u");
	__HAL_AUX_ENTRY("tx_drop_frms_PORT1",
	    mrpcim_info.xgmac_port[1].tx_drop_frms, "%u");
	__HAL_AUX_ENTRY("rx_ttl_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_vld_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_vld_frms, "%llu");
	__HAL_AUX_ENTRY("rx_offload_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_offload_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_octets_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_octets, "%llu");
	__HAL_AUX_ENTRY("rx_data_octets_PORT1",
	    mrpcim_info.xgmac_port[1].rx_data_octets, "%llu");
	__HAL_AUX_ENTRY("rx_offload_octets_PORT1",
	    mrpcim_info.xgmac_port[1].rx_offload_octets, "%llu");
	__HAL_AUX_ENTRY("rx_vld_mcast_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_vld_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_vld_bcast_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_vld_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_ucast_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_accepted_ucast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_nucast_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_accepted_nucast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_tagged_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_tagged_frms, "%llu");
	__HAL_AUX_ENTRY("rx_long_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_long_frms, "%llu");
	__HAL_AUX_ENTRY("rx_usized_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_usized_frms, "%llu");
	__HAL_AUX_ENTRY("rx_osized_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_osized_frms, "%llu");
	__HAL_AUX_ENTRY("rx_frag_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_frag_frms, "%llu");
	__HAL_AUX_ENTRY("rx_jabber_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_jabber_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_64_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_64_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_65_127_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_65_127_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_128_255_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_128_255_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_256_511_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_256_511_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_512_1023_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_512_1023_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_1024_1518_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_1024_1518_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_1519_4095_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_1519_4095_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_4096_8191_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_4096_8191_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_8192_max_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_8192_max_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_gt_max_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ttl_gt_max_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ip_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ip, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_ip_PORT1",
	    mrpcim_info.xgmac_port[1].rx_accepted_ip, "%llu");
	__HAL_AUX_ENTRY("rx_ip_octets_PORT1",
	    mrpcim_info.xgmac_port[1].rx_ip_octets, "%llu");
	__HAL_AUX_ENTRY("rx_err_ip_PORT1",
	    mrpcim_info.xgmac_port[1].rx_err_ip, "%llu");
	__HAL_AUX_ENTRY("rx_icmp_PORT1",
	    mrpcim_info.xgmac_port[1].rx_icmp, "%llu");
	__HAL_AUX_ENTRY("rx_tcp_PORT1",
	    mrpcim_info.xgmac_port[1].rx_tcp, "%llu");
	__HAL_AUX_ENTRY("rx_udp_PORT1",
	    mrpcim_info.xgmac_port[1].rx_udp, "%llu");
	__HAL_AUX_ENTRY("rx_err_tcp_PORT1",
	    mrpcim_info.xgmac_port[1].rx_err_tcp, "%llu");
	__HAL_AUX_ENTRY("rx_pause_count_PORT1",
	    mrpcim_info.xgmac_port[1].rx_pause_count, "%llu");
	__HAL_AUX_ENTRY("rx_pause_ctrl_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_pause_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_unsup_ctrl_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_unsup_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_fcs_err_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_fcs_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_in_rng_len_err_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_in_rng_len_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_out_rng_len_err_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_out_rng_len_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_drop_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_drop_frms, "%llu");
	__HAL_AUX_ENTRY("rx_discarded_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_discarded_frms, "%llu");
	__HAL_AUX_ENTRY("rx_drop_ip_PORT1",
	    mrpcim_info.xgmac_port[1].rx_drop_ip, "%llu");
	__HAL_AUX_ENTRY("rx_drop_udp_PORT1",
	    mrpcim_info.xgmac_port[1].rx_drop_udp, "%llu");
	__HAL_AUX_ENTRY("rx_marker_pdu_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_marker_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_lacpdu_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_lacpdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_unknown_pdu_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_unknown_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_marker_resp_pdu_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_marker_resp_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_fcs_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_fcs_discard, "%u");
	__HAL_AUX_ENTRY("rx_illegal_pdu_frms_PORT1",
	    mrpcim_info.xgmac_port[1].rx_illegal_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_switch_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_switch_discard, "%u");
	__HAL_AUX_ENTRY("rx_len_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_len_discard, "%u");
	__HAL_AUX_ENTRY("rx_rpa_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_rpa_discard, "%u");
	__HAL_AUX_ENTRY("rx_l2_mgmt_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_l2_mgmt_discard, "%u");
	__HAL_AUX_ENTRY("rx_rts_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_rts_discard, "%u");
	__HAL_AUX_ENTRY("rx_trash_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_trash_discard, "%u");
	__HAL_AUX_ENTRY("rx_buff_full_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_buff_full_discard, "%u");
	__HAL_AUX_ENTRY("rx_red_discard_PORT1",
	    mrpcim_info.xgmac_port[1].rx_red_discard, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_ctrl_err_cnt_PORT1",
	    mrpcim_info.xgmac_port[1].rx_xgmii_ctrl_err_cnt, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_data_err_cnt_PORT1",
	    mrpcim_info.xgmac_port[1].rx_xgmii_data_err_cnt, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_char1_match_PORT1",
	    mrpcim_info.xgmac_port[1].rx_xgmii_char1_match, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_err_sym_PORT1",
	    mrpcim_info.xgmac_port[1].rx_xgmii_err_sym, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_column1_match_PORT1",
	    mrpcim_info.xgmac_port[1].rx_xgmii_column1_match, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_char2_match_PORT1",
	    mrpcim_info.xgmac_port[1].rx_xgmii_char2_match, "%u");
	__HAL_AUX_ENTRY("rx_local_fault_PORT1",
	    mrpcim_info.xgmac_port[1].rx_local_fault, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_column2_match_PORT1",
	    mrpcim_info.xgmac_port[1].rx_xgmii_column2_match, "%u");
	__HAL_AUX_ENTRY("rx_jettison_PORT1",
	    mrpcim_info.xgmac_port[1].rx_jettison, "%u");
	__HAL_AUX_ENTRY("rx_remote_fault_PORT1",
	    mrpcim_info.xgmac_port[1].rx_remote_fault, "%u");

	__HAL_AUX_ENTRY("tx_ttl_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_ttl_frms, "%llu");
	__HAL_AUX_ENTRY("tx_ttl_octets_PORT2",
	    mrpcim_info.xgmac_port[2].tx_ttl_octets, "%llu");
	__HAL_AUX_ENTRY("tx_data_octets_PORT2",
	    mrpcim_info.xgmac_port[2].tx_data_octets, "%llu");
	__HAL_AUX_ENTRY("tx_mcast_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_bcast_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_ucast_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_ucast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_tagged_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_tagged_frms, "%llu");
	__HAL_AUX_ENTRY("tx_vld_ip_PORT2",
	    mrpcim_info.xgmac_port[2].tx_vld_ip, "%llu");
	__HAL_AUX_ENTRY("tx_vld_ip_octets_PORT2",
	    mrpcim_info.xgmac_port[2].tx_vld_ip_octets, "%llu");
	__HAL_AUX_ENTRY("tx_icmp_PORT2",
	    mrpcim_info.xgmac_port[2].tx_icmp, "%llu");
	__HAL_AUX_ENTRY("tx_tcp_PORT2",
	    mrpcim_info.xgmac_port[2].tx_tcp, "%llu");
	__HAL_AUX_ENTRY("tx_rst_tcp_PORT2",
	    mrpcim_info.xgmac_port[2].tx_rst_tcp, "%llu");
	__HAL_AUX_ENTRY("tx_udp_PORT2",
	    mrpcim_info.xgmac_port[2].tx_udp, "%llu");
	__HAL_AUX_ENTRY("tx_parse_error_PORT2",
	    mrpcim_info.xgmac_port[2].tx_parse_error, "%u");
	__HAL_AUX_ENTRY("tx_unknown_protocol_PORT2",
	    mrpcim_info.xgmac_port[2].tx_unknown_protocol, "%u");
	__HAL_AUX_ENTRY("tx_pause_ctrl_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_pause_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("tx_marker_pdu_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_marker_pdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_lacpdu_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_lacpdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_drop_ip_PORT2",
	    mrpcim_info.xgmac_port[2].tx_drop_ip, "%u");
	__HAL_AUX_ENTRY("tx_marker_resp_pdu_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_marker_resp_pdu_frms, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_char2_match_PORT2",
	    mrpcim_info.xgmac_port[2].tx_xgmii_char2_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_char1_match_PORT2",
	    mrpcim_info.xgmac_port[2].tx_xgmii_char1_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_column2_match_PORT2",
	    mrpcim_info.xgmac_port[2].tx_xgmii_column2_match, "%u");
	__HAL_AUX_ENTRY("tx_xgmii_column1_match_PORT2",
	    mrpcim_info.xgmac_port[2].tx_xgmii_column1_match, "%u");
	__HAL_AUX_ENTRY("tx_any_err_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_any_err_frms, "%u");
	__HAL_AUX_ENTRY("tx_drop_frms_PORT2",
	    mrpcim_info.xgmac_port[2].tx_drop_frms, "%u");
	__HAL_AUX_ENTRY("rx_ttl_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_vld_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_vld_frms, "%llu");
	__HAL_AUX_ENTRY("rx_offload_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_offload_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_octets_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_octets, "%llu");
	__HAL_AUX_ENTRY("rx_data_octets_PORT2",
	    mrpcim_info.xgmac_port[2].rx_data_octets, "%llu");
	__HAL_AUX_ENTRY("rx_offload_octets_PORT2",
	    mrpcim_info.xgmac_port[2].rx_offload_octets, "%llu");
	__HAL_AUX_ENTRY("rx_vld_mcast_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_vld_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_vld_bcast_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_vld_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_ucast_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_accepted_ucast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_nucast_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_accepted_nucast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_tagged_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_tagged_frms, "%llu");
	__HAL_AUX_ENTRY("rx_long_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_long_frms, "%llu");
	__HAL_AUX_ENTRY("rx_usized_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_usized_frms, "%llu");
	__HAL_AUX_ENTRY("rx_osized_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_osized_frms, "%llu");
	__HAL_AUX_ENTRY("rx_frag_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_frag_frms, "%llu");
	__HAL_AUX_ENTRY("rx_jabber_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_jabber_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_64_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_64_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_65_127_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_65_127_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_128_255_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_128_255_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_256_511_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_256_511_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_512_1023_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_512_1023_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_1024_1518_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_1024_1518_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_1519_4095_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_1519_4095_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_4096_8191_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_4096_8191_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_8192_max_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_8192_max_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ttl_gt_max_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ttl_gt_max_frms, "%llu");
	__HAL_AUX_ENTRY("rx_ip_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ip, "%llu");
	__HAL_AUX_ENTRY("rx_accepted_ip_PORT2",
	    mrpcim_info.xgmac_port[2].rx_accepted_ip, "%llu");
	__HAL_AUX_ENTRY("rx_ip_octets_PORT2",
	    mrpcim_info.xgmac_port[2].rx_ip_octets, "%llu");
	__HAL_AUX_ENTRY("rx_err_ip_PORT2",
	    mrpcim_info.xgmac_port[2].rx_err_ip, "%llu");
	__HAL_AUX_ENTRY("rx_icmp_PORT2",
	    mrpcim_info.xgmac_port[2].rx_icmp, "%llu");
	__HAL_AUX_ENTRY("rx_tcp_PORT2",
	    mrpcim_info.xgmac_port[2].rx_tcp, "%llu");
	__HAL_AUX_ENTRY("rx_udp_PORT2",
	    mrpcim_info.xgmac_port[2].rx_udp, "%llu");
	__HAL_AUX_ENTRY("rx_err_tcp_PORT2",
	    mrpcim_info.xgmac_port[2].rx_err_tcp, "%llu");
	__HAL_AUX_ENTRY("rx_pause_count_PORT2",
	    mrpcim_info.xgmac_port[2].rx_pause_count, "%llu");
	__HAL_AUX_ENTRY("rx_pause_ctrl_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_pause_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_unsup_ctrl_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_unsup_ctrl_frms, "%llu");
	__HAL_AUX_ENTRY("rx_fcs_err_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_fcs_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_in_rng_len_err_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_in_rng_len_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_out_rng_len_err_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_out_rng_len_err_frms, "%llu");
	__HAL_AUX_ENTRY("rx_drop_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_drop_frms, "%llu");
	__HAL_AUX_ENTRY("rx_discarded_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_discarded_frms, "%llu");
	__HAL_AUX_ENTRY("rx_drop_ip_PORT2",
	    mrpcim_info.xgmac_port[2].rx_drop_ip, "%llu");
	__HAL_AUX_ENTRY("rx_drop_udp_PORT2",
	    mrpcim_info.xgmac_port[2].rx_drop_udp, "%llu");
	__HAL_AUX_ENTRY("rx_marker_pdu_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_marker_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_lacpdu_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_lacpdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_unknown_pdu_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_unknown_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_marker_resp_pdu_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_marker_resp_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_fcs_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_fcs_discard, "%u");
	__HAL_AUX_ENTRY("rx_illegal_pdu_frms_PORT2",
	    mrpcim_info.xgmac_port[2].rx_illegal_pdu_frms, "%u");
	__HAL_AUX_ENTRY("rx_switch_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_switch_discard, "%u");
	__HAL_AUX_ENTRY("rx_len_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_len_discard, "%u");
	__HAL_AUX_ENTRY("rx_rpa_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_rpa_discard, "%u");
	__HAL_AUX_ENTRY("rx_l2_mgmt_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_l2_mgmt_discard, "%u");
	__HAL_AUX_ENTRY("rx_rts_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_rts_discard, "%u");
	__HAL_AUX_ENTRY("rx_trash_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_trash_discard, "%u");
	__HAL_AUX_ENTRY("rx_buff_full_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_buff_full_discard, "%u");
	__HAL_AUX_ENTRY("rx_red_discard_PORT2",
	    mrpcim_info.xgmac_port[2].rx_red_discard, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_ctrl_err_cnt_PORT2",
	    mrpcim_info.xgmac_port[2].rx_xgmii_ctrl_err_cnt, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_data_err_cnt_PORT2",
	    mrpcim_info.xgmac_port[2].rx_xgmii_data_err_cnt, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_char1_match_PORT2",
	    mrpcim_info.xgmac_port[2].rx_xgmii_char1_match, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_err_sym_PORT2",
	    mrpcim_info.xgmac_port[2].rx_xgmii_err_sym, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_column1_match_PORT2",
	    mrpcim_info.xgmac_port[2].rx_xgmii_column1_match, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_char2_match_PORT2",
	    mrpcim_info.xgmac_port[2].rx_xgmii_char2_match, "%u");
	__HAL_AUX_ENTRY("rx_local_fault_PORT2",
	    mrpcim_info.xgmac_port[2].rx_local_fault, "%u");
	__HAL_AUX_ENTRY("rx_xgmii_column2_match_PORT2",
	    mrpcim_info.xgmac_port[2].rx_xgmii_column2_match, "%u");
	__HAL_AUX_ENTRY("rx_jettison_PORT2",
	    mrpcim_info.xgmac_port[2].rx_jettison, "%u");
	__HAL_AUX_ENTRY("rx_remote_fault_PORT2",
	    mrpcim_info.xgmac_port[2].rx_remote_fault, "%u");

	__HAL_AUX_ENTRY("tx_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].tx_frms, "%llu");
	__HAL_AUX_ENTRY("tx_data_octets_AGGR0",
	    mrpcim_info.xgmac_aggr[0].tx_data_octets, "%llu");
	__HAL_AUX_ENTRY("tx_mcast_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].tx_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_bcast_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].tx_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_discarded_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].tx_discarded_frms, "%llu");
	__HAL_AUX_ENTRY("tx_errored_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].tx_errored_frms, "%llu");
	__HAL_AUX_ENTRY("rx_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].rx_frms, "%llu");
	__HAL_AUX_ENTRY("rx_data_octets_AGGR0",
	    mrpcim_info.xgmac_aggr[0].rx_data_octets, "%llu");
	__HAL_AUX_ENTRY("rx_mcast_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].rx_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_bcast_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].rx_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_discarded_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].rx_discarded_frms, "%llu");
	__HAL_AUX_ENTRY("rx_errored_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].rx_errored_frms, "%llu");
	__HAL_AUX_ENTRY("rx_unknown_slow_proto_frms_AGGR0",
	    mrpcim_info.xgmac_aggr[0].rx_unknown_slow_proto_frms, "%llu");

	__HAL_AUX_ENTRY("tx_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].tx_frms, "%llu");
	__HAL_AUX_ENTRY("tx_data_octets_AGGR1",
	    mrpcim_info.xgmac_aggr[1].tx_data_octets, "%llu");
	__HAL_AUX_ENTRY("tx_mcast_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].tx_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_bcast_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].tx_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("tx_discarded_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].tx_discarded_frms, "%llu");
	__HAL_AUX_ENTRY("tx_errored_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].tx_errored_frms, "%llu");
	__HAL_AUX_ENTRY("rx_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].rx_frms, "%llu");
	__HAL_AUX_ENTRY("rx_data_octets_AGGR1",
	    mrpcim_info.xgmac_aggr[1].rx_data_octets, "%llu");
	__HAL_AUX_ENTRY("rx_mcast_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].rx_mcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_bcast_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].rx_bcast_frms, "%llu");
	__HAL_AUX_ENTRY("rx_discarded_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].rx_discarded_frms, "%llu");
	__HAL_AUX_ENTRY("rx_errored_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].rx_errored_frms, "%llu");
	__HAL_AUX_ENTRY("rx_unknown_slow_proto_frms_AGGR1",
	    mrpcim_info.xgmac_aggr[1].rx_unknown_slow_proto_frms, "%llu");

	__HAL_AUX_ENTRY("xgmac_global_prog_event_gnum0",
	    mrpcim_info.xgmac_global_prog_event_gnum0, "%llu");
	__HAL_AUX_ENTRY("xgmac_global_prog_event_gnum1",
	    mrpcim_info.xgmac_global_prog_event_gnum1, "%llu");

	__HAL_AUX_ENTRY("xgmac_orp_lro_events",
	    mrpcim_info.xgmac_orp_lro_events, "%llu");

	__HAL_AUX_ENTRY("xgmac_orp_bs_events",
	    mrpcim_info.xgmac_orp_bs_events, "%llu");

	__HAL_AUX_ENTRY("xgmac_orp_iwarp_events",
	    mrpcim_info.xgmac_orp_iwarp_events, "%llu");

	__HAL_AUX_ENTRY("xgmac_tx_permitted_frms",
	    mrpcim_info.xgmac_tx_permitted_frms, "%u");

	__HAL_AUX_ENTRY("xgmac_port2_tx_any_frms",
	    mrpcim_info.xgmac_port2_tx_any_frms, "%u");
	__HAL_AUX_ENTRY("xgmac_port1_tx_any_frms",
	    mrpcim_info.xgmac_port1_tx_any_frms, "%u");
	__HAL_AUX_ENTRY("xgmac_port0_tx_any_frms",
	    mrpcim_info.xgmac_port0_tx_any_frms, "%u");

	__HAL_AUX_ENTRY("xgmac_port2_rx_any_frms",
	    mrpcim_info.xgmac_port2_rx_any_frms, "%u");
	__HAL_AUX_ENTRY("xgmac_port1_rx_any_frms",
	    mrpcim_info.xgmac_port1_rx_any_frms, "%u");
	__HAL_AUX_ENTRY("xgmac_port0_rx_any_frms",
	    mrpcim_info.xgmac_port0_rx_any_frms, "%u");

	__HAL_AUX_ENTRY_END(bufsize, retsize);

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_vpath_ring_dump - Dump vpath ring.
 * @vpath_handle: Vpath handle.
 *
 * Dump vpath ring.
 */
vxge_hal_status_e
vxge_hal_aux_vpath_ring_dump(vxge_hal_vpath_h vpath_handle)
{
	u32 i;
	char buffer[4096];
	__hal_ring_t *ring;
	vxge_hal_rxd_h rxdh;
	__hal_virtualpath_t *vpath;
	vxge_hal_ring_rxd_1_t *rxd1;
	vxge_hal_ring_rxd_3_t *rxd3;
	vxge_hal_ring_rxd_5_t *rxd5;

	vxge_assert(vpath_handle != NULL);

	vpath = (__hal_virtualpath_t *)
	    ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	ring = (__hal_ring_t *) vpath->ringh;

	vxge_os_println("********* vxge RING DUMP BEGIN **********");

	vxge_os_println("********* vxge RING RXD LIST **********");

	__hal_channel_for_each_dtr(&ring->channel, rxdh, i) {

		(void) vxge_os_snprintf(buffer, sizeof(buffer),
		    "%d : 0x"VXGE_OS_STXFMT, i, (ptr_t) rxdh);

		vxge_os_println(buffer);

		switch (ring->buffer_mode) {
		case 1:
			rxd1 = (vxge_hal_ring_rxd_1_t *) rxdh;
			(void) vxge_os_snprintf(buffer, sizeof(buffer),
			    "\thost_control = 0x"VXGE_OS_LLXFMT", "
			    "control_0 = 0x"VXGE_OS_LLXFMT", "
			    "control_1 = 0x"VXGE_OS_LLXFMT", "
			    "buffer0_ptr = 0x"VXGE_OS_LLXFMT,
			    rxd1->host_control, rxd1->control_0,
			    rxd1->control_1, rxd1->buffer0_ptr);
			break;
		case 3:
			rxd3 = (vxge_hal_ring_rxd_3_t *) rxdh;
			(void) vxge_os_snprintf(buffer, sizeof(buffer),
			    "\thost_control = 0x"VXGE_OS_LLXFMT", "
			    "control_0 = 0x"VXGE_OS_LLXFMT", "
			    "control_1 = 0x"VXGE_OS_LLXFMT", "
			    "buffer0_ptr = 0x"VXGE_OS_LLXFMT", "
			    "buffer1_ptr = 0x"VXGE_OS_LLXFMT", "
			    "buffer2_ptr = 0x"VXGE_OS_LLXFMT,
			    rxd3->host_control, rxd3->control_0,
			    rxd3->control_1, rxd3->buffer0_ptr,
			    rxd3->buffer1_ptr, rxd3->buffer2_ptr);
			break;
		case 5:
			rxd5 = (vxge_hal_ring_rxd_5_t *) rxdh;
			(void) vxge_os_snprintf(buffer, sizeof(buffer),
			    "\thost_control = 0x%x, "
			    "control_0 = 0x"VXGE_OS_LLXFMT", "
			    "control_1 = 0x"VXGE_OS_LLXFMT", "
			    "control_2 = 0x%x, "
			    "buffer0_ptr = 0x"VXGE_OS_LLXFMT", "
			    "buffer1_ptr = 0x"VXGE_OS_LLXFMT", "
			    "buffer2_ptr = 0x"VXGE_OS_LLXFMT", "
			    "buffer3_ptr = 0x"VXGE_OS_LLXFMT", "
			    "buffer4_ptr = 0x"VXGE_OS_LLXFMT,
			    rxd5->host_control, rxd5->control_0,
			    rxd5->control_1, rxd5->control_2,
			    rxd5->buffer0_ptr, rxd5->buffer1_ptr,
			    rxd5->buffer2_ptr, rxd5->buffer3_ptr,
			    rxd5->buffer4_ptr);
			break;
		default:
			continue;
		}

		vxge_os_println(buffer);
	}

	vxge_os_println("******* vxge RING RXD LIST END **********");

	vxge_os_println("********* vxge RING DUMP END **********");

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_vpath_fifo_dump - Dump vpath fifo.
 * @vpath_handle: Vpath handle.
 *
 * Dump vpath fifo.
 */
vxge_hal_status_e
vxge_hal_aux_vpath_fifo_dump(vxge_hal_vpath_h vpath_handle)
{
	u32 i, j;
	char buffer[4096];
	__hal_fifo_t *fifo;
	vxge_hal_txdl_h txdlh;
	__hal_virtualpath_t *vpath;
	vxge_hal_fifo_txd_t *txd;
	__hal_fifo_txdl_priv_t *txdl_priv;

	vxge_assert(vpath_handle != NULL);

	vpath = (__hal_virtualpath_t *)
	    ((__hal_vpath_handle_t *) vpath_handle)->vpath;

	fifo = (__hal_fifo_t *) vpath->fifoh;

	vxge_os_println("********* vxge FIFO DUMP BEGIN **********");

	vxge_os_println("********* vxge FIFO TXDL LIST **********");

	__hal_channel_for_each_dtr(&fifo->channel, txdlh, j) {

		(void) vxge_os_snprintf(buffer, sizeof(buffer),
		    "TXDL %d : 0x"VXGE_OS_STXFMT, j, (ptr_t) txdlh);

		vxge_os_println(buffer);

		txdl_priv = VXGE_HAL_FIFO_HAL_PRIV(fifo, txdlh);

		for (i = 0, txd = (vxge_hal_fifo_txd_t *) txdlh;
		    i < txdl_priv->frags; i++, txd++) {

			(void) vxge_os_snprintf(buffer, sizeof(buffer),
			    "\tcontrol_0 = 0x"VXGE_OS_LLXFMT", "
			    "control_1 = 0x"VXGE_OS_LLXFMT", "
			    "buffer_ptr = 0x"VXGE_OS_LLXFMT", "
			    "host_control = 0x"VXGE_OS_LLXFMT,
			    txd->control_0, txd->control_1,
			    txd->buffer_pointer, txd->host_control);

			vxge_os_println(buffer);
		}

	}

	vxge_os_println("******* vxge FIFO TXDL LIST END **********");

	vxge_os_println("********* vxge FIFO DUMP END **********");

	return (VXGE_HAL_OK);
}

/*
 * vxge_hal_aux_device_dump - Dump driver "about" info and device state.
 * @devh: HAL device handle.
 *
 * Dump driver & device "about" info and device state,
 * including all BAR0 registers, hardware and software statistics, PCI
 * configuration space.
 */
vxge_hal_status_e
vxge_hal_aux_device_dump(vxge_hal_device_h devh)
{
	vxge_hal_status_e status = VXGE_HAL_OK;
	__hal_device_t *hldev = (__hal_device_t *) devh;
	int retsize;
	u32 offset, i;
	u64 retval;

	vxge_assert(hldev->dump_buf != NULL);

	vxge_os_println("********* vxge DEVICE DUMP BEGIN **********");

	status = vxge_hal_aux_about_read(hldev, VXGE_HAL_DUMP_BUF_SIZE,
	    hldev->dump_buf, &retsize);
	if (status != VXGE_HAL_OK)
		goto error;

	vxge_os_println(hldev->dump_buf);

	vxge_os_println("******* PCI Config Reg **********");

	status = vxge_hal_aux_pci_config_read(hldev, VXGE_HAL_DUMP_BUF_SIZE,
	    hldev->dump_buf, &retsize);
	if (status != VXGE_HAL_OK)
		goto error;

	vxge_os_println(hldev->dump_buf);

	vxge_os_println("******* Legacy Reg **********");

	for (offset = 0; offset < sizeof(vxge_hal_legacy_reg_t); offset += 8) {
		status = vxge_hal_mgmt_reg_read(devh,
		    vxge_hal_mgmt_reg_type_legacy, 0, offset, &retval);

		if (status != VXGE_HAL_OK)
			goto error;

		if (!retval)
			continue;

		vxge_os_printf("0x%04x 0x%08x%08x", offset,
		    (u32) (retval >> 32), (u32) retval);
	}
	vxge_os_println("\n");

	vxge_os_println("******* TOC Reg *********");

	for (offset = 0; offset < sizeof(vxge_hal_toc_reg_t); offset += 8) {
		status = vxge_hal_mgmt_reg_read(devh,
		    vxge_hal_mgmt_reg_type_toc, 0, offset, &retval);
		if (status != VXGE_HAL_OK)
			goto error;

		if (!retval)
			continue;

		vxge_os_printf("0x%04x 0x%08x%08x", offset,
		    (u32) (retval >> 32), (u32) retval);
	}
	vxge_os_println("\n");

	vxge_os_println("******* Common Reg **********");

	for (offset = 0; offset < sizeof(vxge_hal_common_reg_t); offset += 8) {
		status = vxge_hal_mgmt_reg_read(devh,
		    vxge_hal_mgmt_reg_type_common, 0, offset, &retval);
		if (status != VXGE_HAL_OK)
			goto error;

		if (!retval)
			continue;

		vxge_os_printf("0x%04x 0x%08x%08x", offset,
		    (u32) (retval >> 32), (u32) retval);
	}
	vxge_os_println("\n");

	for (i = 0; i < VXGE_HAL_TITAN_PCICFGMGMT_REG_SPACES; i++) {
		vxge_os_printf("****** PCI Config Mgmt Reg : %d ********\n", i);

		for (offset = 0; offset < sizeof(vxge_hal_pcicfgmgmt_reg_t);
		    offset += 8) {
			status = vxge_hal_mgmt_reg_read(devh,
			    vxge_hal_mgmt_reg_type_pcicfgmgmt,
			    i, offset, &retval);
			if (status != VXGE_HAL_OK)
				continue;

			if (!retval)
				continue;

			vxge_os_printf("0x%04x 0x%08x%08x", offset,
			    (u32) (retval >> 32), (u32) retval);
		}
	}
	vxge_os_println("\n");

	vxge_os_println("******* MRPCIM Reg **********");

	for (offset = 0; offset < sizeof(vxge_hal_mrpcim_reg_t);
	    offset += 8) {
		status = vxge_hal_mgmt_reg_read(devh,
		    vxge_hal_mgmt_reg_type_mrpcim, 0, offset, &retval);
		if (status != VXGE_HAL_OK)
			continue;

		if (!retval)
			continue;

		vxge_os_printf("0x%04x 0x%08x%08x", offset,
		    (u32) (retval >> 32), (u32) retval);
	}
	vxge_os_println("\n");

	for (i = 0; i < VXGE_HAL_TITAN_SRPCIM_REG_SPACES; i++) {
		vxge_os_printf("******* SRPCIM Reg : %d **********\n", i);

		for (offset = 0; offset < sizeof(vxge_hal_srpcim_reg_t);
		    offset += 8) {
			status = vxge_hal_mgmt_reg_read(devh,
			    vxge_hal_mgmt_reg_type_srpcim, i, offset, &retval);
			if (status != VXGE_HAL_OK)
				continue;

			if (!retval)
				continue;

			vxge_os_printf("0x%04x 0x%08x%08x", offset,
			    (u32) (retval >> 32), (u32) retval);
		}
	}
	vxge_os_println("\n");

	for (i = 0; i < VXGE_HAL_TITAN_VPMGMT_REG_SPACES; i++) {
		vxge_os_printf("******* VPATH MGMT Reg : %d **********\n", i);

		for (offset = 0; offset < sizeof(vxge_hal_vpmgmt_reg_t);
		    offset += 8) {
			status = vxge_hal_mgmt_reg_read(devh,
			    vxge_hal_mgmt_reg_type_vpmgmt, i, offset, &retval);
			if (status != VXGE_HAL_OK)
				continue;

			if (!retval)
				continue;

			vxge_os_printf("0x%04x 0x%08x%08x", offset,
			    (u32) (retval >> 32), (u32) retval);
		}
	}
	vxge_os_println("\n");

	for (i = 0; i < VXGE_HAL_TITAN_VPATH_REG_SPACES; i++) {
		vxge_os_printf("******* VPATH Reg : %d **********\n", i);

		for (offset = 0; offset < sizeof(vxge_hal_vpath_reg_t);
		    offset += 8) {
			status = vxge_hal_mgmt_reg_read(devh,
			    vxge_hal_mgmt_reg_type_vpath, i, offset, &retval);
			if (status != VXGE_HAL_OK)
				continue;

			if (!retval)
				continue;

			vxge_os_printf("0x%04x 0x%08x%08x", offset,
			    (u32) (retval >> 32), (u32) retval);
		}
	}

	vxge_os_println("\n");

	status = vxge_hal_aux_stats_mrpcim_read(hldev, VXGE_HAL_DUMP_BUF_SIZE,
	    hldev->dump_buf,
	    &retsize);
	if (status == VXGE_HAL_OK) {
		vxge_os_println("******* MRPCIM Stats **********");
		vxge_os_println(hldev->dump_buf);
	}

	vxge_os_println("******* Device Stats **********");

	status = vxge_hal_aux_stats_device_read(hldev, VXGE_HAL_DUMP_BUF_SIZE,
	    hldev->dump_buf, &retsize);
	if (status != VXGE_HAL_OK)
		goto error;

	vxge_os_println(hldev->dump_buf);

	vxge_os_println("********* DEVICE DUMP END **********");

error:
	return (status);
}
