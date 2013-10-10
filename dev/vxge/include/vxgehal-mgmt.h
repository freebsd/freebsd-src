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

#ifndef	VXGE_HAL_MGMT_H
#define	VXGE_HAL_MGMT_H

__EXTERN_BEGIN_DECLS

/*
 * struct vxge_hal_mgmt_about_info_t - About info.
 * @vendor: PCI Vendor ID.
 * @device: PCI Device ID.
 * @subsys_vendor: PCI Subsystem Vendor ID.
 * @subsys_device: PCI Subsystem Device ID.
 * @board_rev: PCI Board revision, e.g. 3 - for Xena 3.
 * @vendor_name: Exar Corp.
 * @chip_name: X3100.
 * @media: Fiber, copper.
 * @hal_major: HAL major version number.
 * @hal_minor: HAL minor version number.
 * @hal_fix: HAL fix number.
 * @hal_build: HAL build number.
 * @ll_major: Link-layer ULD major version number.
 * @ll_minor: Link-layer ULD minor version number.
 * @ll_fix: Link-layer ULD fix version number.
 * @ll_build: Link-layer ULD build number.
 */
typedef struct vxge_hal_mgmt_about_info_t {
	u16		vendor;
	u16		device;
	u16		subsys_vendor;
	u16		subsys_device;
	u8		board_rev;
	char		vendor_name[16];
	char		chip_name[16];
	char		media[16];
	char		hal_major[4];
	char		hal_minor[4];
	char		hal_fix[4];
	char		hal_build[16];
	char		ll_major[4];
	char		ll_minor[4];
	char		ll_fix[4];
	char		ll_build[16];
} vxge_hal_mgmt_about_info_t;


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
    u32 *size);

/*
 * vxge_hal_mgmt_pci_config - Retrieve PCI configuration.
 * @devh: HAL device handle.
 * @buffer: Buffer for PCI configuration space.
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
vxge_hal_mgmt_pci_config(vxge_hal_device_h devh, u8 *buffer, u32 *size);

/*
 * struct vxge_hal_mgmt_pm_cap_t - Power Management Capabilities
 * @pm_cap_ver: Version
 * @pm_cap_pme_clock: PME clock required
 * @pm_cap_aux_power: Auxilliary power support
 * @pm_cap_dsi: Device specific initialization
 * @pm_cap_aux_current: auxiliary current requirements
 * @pm_cap_cap_d0: D1 power state support
 * @pm_cap_cap_d1: D2 power state support
 * @pm_cap_pme_d0: PME# can be asserted from D3hot
 * @pm_cap_pme_d1: PME# can be asserted from D3hot
 * @pm_cap_pme_d2: PME# can be asserted from D3hot
 * @pm_cap_pme_d3_hot: PME# can be asserted from D3hot
 * @pm_cap_pme_d3_cold: PME# can be asserted from D3cold
 * @pm_ctrl_state: Current power state (D0 to D3)
 * @pm_ctrl_no_soft_reset: Devices transitioning from D3hot to D0
 * @pm_ctrl_pme_enable: PME pin enable
 * @pm_ctrl_pme_data_sel: Data select
 * @pm_ctrl_pme_data_scale: Data scale
 * @pm_ctrl_pme_status: PME pin status
 * @pm_ppb_ext_b2_b3: Stop clock when in D3hot
 * @pm_ppb_ext_ecc_en: Bus power/clock control enable
 * @pm_data_reg: state dependent data requested by pm_ctrl_pme_data_sel
 *
 * Power Management Capabilities structure
 */
typedef struct vxge_hal_mgmt_pm_cap_t {
	u32	pm_cap_ver;
	u32	pm_cap_pme_clock;
	u32	pm_cap_aux_power;
	u32	pm_cap_dsi;
	u32	pm_cap_aux_current;
	u32	pm_cap_cap_d0;
	u32	pm_cap_cap_d1;
	u32	pm_cap_pme_d0;
	u32	pm_cap_pme_d1;
	u32	pm_cap_pme_d2;
	u32	pm_cap_pme_d3_hot;
	u32	pm_cap_pme_d3_cold;
	u32	pm_ctrl_state;
	u32	pm_ctrl_no_soft_reset;
	u32	pm_ctrl_pme_enable;
	u32	pm_ctrl_pme_data_sel;
	u32	pm_ctrl_pme_data_scale;
	u32	pm_ctrl_pme_status;
	u32	pm_ppb_ext_b2_b3;
	u32	pm_ppb_ext_ecc_en;
	u32	pm_data_reg;
} vxge_hal_mgmt_pm_cap_t;

/*
 * vxge_hal_mgmt_pm_capabilities_get - Returns the pm capabilities
 * @devh: HAL device handle.
 * @pm_cap: Power Management Capabilities
 *
 * Return the pm capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_pm_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_mgmt_pm_cap_t *pm_cap);

/*
 * struct vxge_hal_mgmt_sid_cap_t - Slot ID Capabilities
 * @sid_number_of_slots: Number of solts
 * @sid_first_in_chasis: First in chasis flag
 * @sid_chasis_number: Chasis Number
 *
 * Slot ID Capabilities structure
 */
typedef struct vxge_hal_mgmt_sid_cap_t {
	u32	sid_number_of_slots;
	u32	sid_first_in_chasis;
	u32	sid_chasis_number;
} vxge_hal_mgmt_sid_cap_t;

/*
 * vxge_hal_mgmt_sid_capabilities_get - Returns the sid capabilities
 * @devh: HAL device handle.
 * @sid_cap: Slot Id Capabilities
 *
 * Return the pm capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_sid_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_mgmt_sid_cap_t *sid_cap);

/*
 * struct vxge_hal_mgmt_msi_cap_t - MSI Capabilities
 * @enable: 1 - MSI enabled, 0 - MSI not enabled
 * @is_pvm_capable: 1 - PVM capable, 0 - Not PVM Capable (valid for get only)
 * @is_64bit_addr_capable: 1 - 64 bit address capable, 0 - 32 bit address only
 *		(valid for get only)
 * @vectors_allocated: Number of vectors allocated
 *		000-1 vectors
 *		001-2 vectors
 *		010-4 vectors
 *		011-8 vectors
 *		100-16 vectors
 *		101-32 vectors
 * @max_vectors_capable: Maximum number of vectors that can be allocated
 *		(valid for get only)
 *		000-1 vectors
 *		001-2 vectors
 *		010-4 vectors
 *		011-8 vectors
 *		100-16 vectors
 *		101-32 vectors
 * @address: MSI address
 * @data: MSI Data
 * @mask_bits: For each Mask bit that is set, the function is prohibited from
 *		sending the associated message
 * @pending_bits: For each Pending bit that is set, the function has a
 *		pending associated message.
 *
 * MSI Capabilities structure
 */
typedef struct vxge_hal_mgmt_msi_cap_t {
	u32	enable;
	u32	is_pvm_capable;
	u32	is_64bit_addr_capable;
	u32	vectors_allocated;
	u32	max_vectors_capable;
#define	VXGE_HAL_MGMT_MSI_CAP_VECTORS_1		0
#define	VXGE_HAL_MGMT_MSI_CAP_VECTORS_2		1
#define	VXGE_HAL_MGMT_MSI_CAP_VECTORS_4		2
#define	VXGE_HAL_MGMT_MSI_CAP_VECTORS_8		3
#define	VXGE_HAL_MGMT_MSI_CAP_VECTORS_16	4
#define	VXGE_HAL_MGMT_MSI_CAP_VECTORS_32	5
	u64	address;
	u16	data;
	u32	mask_bits;
	u32	pending_bits;
} vxge_hal_mgmt_msi_cap_t;

/*
 * vxge_hal_mgmt_msi_capabilities_get - Returns the msi capabilities
 * @devh: HAL device handle.
 * @msi_cap: MSI Capabilities
 *
 * Return the msi capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_msi_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_mgmt_msi_cap_t *msi_cap);

/*
 * vxge_hal_mgmt_msi_capabilities_set - Sets the msi capabilities
 * @devh: HAL device handle.
 * @msi_cap: MSI Capabilities
 *
 * Sets the msi capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_msi_capabilities_set(vxge_hal_device_h devh,
    vxge_hal_mgmt_msi_cap_t *msi_cap);

/*
 * struct vxge_hal_mgmt_msix_cap_t - MSIX Capabilities
 * @enable: 1 - MSIX enabled, 0 - MSIX not enabled
 * @mask_all_vect: 1 - Mask all vectors, 0 - Do not mask all vectors
 * @table_size: MSIX Table Size-1
 * @table_offset: Offset of the table from the table_bir
 * @table_bir: Table Bar address register number 0-BAR0, 2-BAR1, 4-BAR2
 * @pba_offset: Offset of the PBA from the pba_bir
 * @pba_bir: PBA Bar address register number 0-BAR0, 2-BAR1, 4-BAR2
 *
 * MSIS Capabilities structure
 */
typedef struct vxge_hal_mgmt_msix_cap_t {
	u32	enable;
	u32	mask_all_vect;
	u32	table_size;
	u32	table_offset;
	u32	table_bir;
#define	VXGE_HAL_MGMT_MSIX_CAP_TABLE_BAR0	0
#define	VXGE_HAL_MGMT_MSIX_CAP_TABLE_BAR1	2
#define	VXGE_HAL_MGMT_MSIX_CAP_TABLE_BAR2	4
	u32	pba_offset;
	u32	pba_bir;
#define	VXGE_HAL_MGMT_MSIX_CAP_PBA_BAR0		0
#define	VXGE_HAL_MGMT_MSIX_CAP_PBA_BAR1		2
#define	VXGE_HAL_MGMT_MSIX_CAP_PBA_BAR2		4
} vxge_hal_mgmt_msix_cap_t;

/*
 * vxge_hal_mgmt_msix_capabilities_get - Returns the msix capabilities
 * @devh: HAL device handle.
 * @msix_cap: MSIX Capabilities
 *
 * Return the msix capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_msix_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_mgmt_msix_cap_t *msix_cap);

/*
 * struct vxge_hal_pci_err_cap_t - PCI Error Capabilities
 * @pci_err_header: Error header
 * @pci_err_uncor_status: Uncorrectable error status
 *		0x00000001 - Training
 *		0x00000010 - Data Link Protocol
 *		0x00001000 - Poisoned TLP
 *		0x00002000 - Flow Control Protocol
 *		0x00004000 - Completion Timeout
 *		0x00008000 - Completer Abort
 *		0x00010000 - Unexpected Completion
 *		0x00020000 - Receiver Overflow
 *		0x00040000 - Malformed TLP
 *		0x00080000 - ECRC Error Status
 *		0x00100000 - Unsupported Request
 * @pci_err_uncor_mask: Uncorrectable mask
 * @pci_err_uncor_server: Uncorrectable server
 * @pci_err_cor_status: Correctable status
 *		0x00000001 - Receiver Error Status
 *		0x00000040 - Bad TLP Status
 *		0x00000080 - Bad DLLP Status
 *		0x00000100 - REPLAY_NUM Rollover
 *		0x00001000 - Replay Timer Timeout
 *		VXGE_HAL_PCI_ERR_COR_MASK	20
 * @pci_err_cap: Error capability
 *		0x00000020 - ECRC Generation Capable
 *		0x00000040 - ECRC Generation Enable
 *		0x00000080 - ECRC Check Capable
 *		0x00000100 - ECRC Check Enable
 * @err_header_log: Error header log
 * @unused: Reserved
 * @pci_err_root_command: Error root command
 * @pci_err_root_status: Error root status
 * @pci_err_root_cor_src:  Error root correctible source
 * @pci_err_root_src: Error root source
 *
 * MSIS Capabilities structure
 */
typedef struct vxge_hal_pci_err_cap_t {
	u32	pci_err_header;
	u32	pci_err_uncor_status;
#define	VXGE_HAL_PCI_ERR_CAP_UNC_TRAIN	    0x00000001	/* Training */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_DLP	    0x00000010	/* Data Link Protocol */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_POISON_TLP 0x00001000	/* Poisoned TLP */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_FCP	    0x00002000	/* Flow Ctrl Protocol */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_COMP_TIME  0x00004000	/* Completion Timeout */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_COMP_ABORT 0x00008000	/* Completer Abort */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_UNX_COMP   0x00010000	/* Unexpected Compl */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_RX_OVER    0x00020000	/* Receiver Overflow */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_MALF_TLP   0x00040000	/* Malformed TLP */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_ECRC	    0x00080000	/* ECRC Error Status */
#define	VXGE_HAL_PCI_ERR_CAP_UNC_UNSUP	    0x00100000 /* Unsupported Request */
	u32	pci_err_uncor_mask;
	u32	pci_err_uncor_server;
	u32	pci_err_cor_status;
#define	VXGE_HAL_PCI_ERR_CAP_COR_RCVR	    0x00000001	/* Recv Err Status */
#define	VXGE_HAL_PCI_ERR_CAP_COR_BAD_TLP    0x00000040	/* Bad TLP Status */
#define	VXGE_HAL_PCI_ERR_CAP_COR_BAD_DLLP   0x00000080	/* Bad DLLP Status */
#define	VXGE_HAL_PCI_ERR_CAP_COR_REP_ROLL   0x00000100	/* REPLAY Rollover */
#define	VXGE_HAL_PCI_ERR_CAP_COR_REP_TIMER  0x00001000	/* Replay Timeout */
#define	VXGE_HAL_PCI_ERR_CAP_COR_MASK	20	/* Corrble Err Mask */
	u32	pci_err_cap;
#define	VXGE_HAL_PCI_ERR_CAP_CAP_FEP(x)	 ((x) & 31)	/* First Err Ptr */
#define	VXGE_HAL_PCI_ERR_CAP_CAP_ECRC_GENC  0x00000020	/* ECRC Gen Capable */
#define	VXGE_HAL_PCI_ERR_CAP_CAP_ECRC_GENE  0x00000040	/* ECRC Gen Enable */
#define	VXGE_HAL_PCI_ERR_CAP_CAP_ECRC_CHKC  0x00000080	/* ECRC Chk Capable */
#define	VXGE_HAL_PCI_ERR_CAP_CAP_ECRC_CHKE  0x00000100	/* ECRC Chk Enable */
	u32	err_header_log;
#define	VXGE_HAL_PCI_ERR_CAP_HEADER_LOG(x)  ((x) >> 31)	/* Error Hdr Log */
	u32	unused[3];
	u32	pci_err_root_command;
	u32	pci_err_root_status;
	u32	pci_err_root_cor_src;
	u32	pci_err_root_src;
} vxge_hal_pci_err_cap_t;

/*
 * vxge_hal_mgmt_pci_err_capabilities_get - Returns the pci error capabilities
 * @devh: HAL device handle.
 * @err_cap: PCI-E Extended Error Capabilities
 *
 * Return the PCI-E Extended Error capabilities
 */
vxge_hal_status_e
vxge_hal_mgmt_pci_err_capabilities_get(vxge_hal_device_h devh,
    vxge_hal_pci_err_cap_t *err_cap);

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
vxge_hal_mgmt_driver_config(vxge_hal_driver_config_t *drv_config, u32 *size);

#if defined(VXGE_TRACE_INTO_CIRCULAR_ARR)

/*
 * vxge_hal_mgmt_trace_read - Read trace buffer contents.
 * @buffer: Buffer to store the trace buffer contents.
 * @buf_size: Size of the buffer.
 * @offset: Offset in the internal trace buffer to read data.
 * @read_length: Size of the valid data in the buffer.
 *
 * Read  HAL trace buffer contents starting from the offset
 * upto the size of the buffer or till EOF is reached.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_EOF_TRACE_BUF - No more data in the trace buffer.
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_trace_read(char *buffer,
    unsigned buf_size,
    unsigned *offset,
    unsigned *read_length);

#endif

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
    vxge_hal_device_config_t *dev_config, u32 *size);


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
    int value_bits, u32 *value);

/*
 * enum vxge_hal_mgmt_reg_type_e - Register types.
 *
 * @vxge_hal_mgmt_reg_type_legacy: Legacy registers
 * @vxge_hal_mgmt_reg_type_toc: TOC Registers
 * @vxge_hal_mgmt_reg_type_common: Common Registers
 * @vxge_hal_mgmt_reg_type_memrepair: Memrepair Registers
 * @vxge_hal_mgmt_reg_type_pcicfgmgmt: pci cfg management registers
 * @vxge_hal_mgmt_reg_type_mrpcim: mrpcim registers
 * @vxge_hal_mgmt_reg_type_srpcim: srpcim registers
 * @vxge_hal_mgmt_reg_type_vpmgmt: vpath management registers
 * @vxge_hal_mgmt_reg_type_vpath: vpath registers
 *
 * Register type enumaration
 */
typedef enum vxge_hal_mgmt_reg_type_e {
	vxge_hal_mgmt_reg_type_legacy = 0,
	vxge_hal_mgmt_reg_type_toc = 1,
	vxge_hal_mgmt_reg_type_common = 2,
	vxge_hal_mgmt_reg_type_memrepair = 3,
	vxge_hal_mgmt_reg_type_pcicfgmgmt = 4,
	vxge_hal_mgmt_reg_type_mrpcim = 5,
	vxge_hal_mgmt_reg_type_srpcim = 6,
	vxge_hal_mgmt_reg_type_vpmgmt = 7,
	vxge_hal_mgmt_reg_type_vpath = 8
} vxge_hal_mgmt_reg_type_e;

/*
 * vxge_hal_mgmt_reg_read - Read X3100 register.
 * @devh: HAL device handle.
 * @type: Register types as defined in enum vxge_hal_mgmt_reg_type_e {}
 * @index: For pcicfgmgmt, srpcim, vpmgmt, vpath this gives the Index
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
    u32 index,
    u32 offset,
    u64 *value);

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
    u32 index,
    u32 offset,
    u64 value);

/*
 * vxge_hal_mgmt_bar0_read - Read X3100 register located at the offset
 *			     from bar0.
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
    u64 *value);

/*
 * vxge_hal_mgmt_bar1_read - Read X3100 register located at the offset
 *			     from bar1.
 * @devh: HAL device handle.
 * @offset: Register offset from bar1
 * @value: Register value. Returned by HAL.
 * Read X3100 register.
 *
 * Returns: VXGE_HAL_OK - success.
 * VXGE_HAL_ERR_INVALID_DEVICE - Device is not valid.
 * VXGE_HAL_ERR_INVALID_OFFSET - Register offset in the space is not valid.
 *
 */
vxge_hal_status_e
vxge_hal_mgmt_bar1_read(vxge_hal_device_h devh,
    u32 offset,
    u64 *value);

/*
 * vxge_hal_mgmt_bar0_Write - Write X3100 register located at the offset
 *			     from bar0.
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
    u64 value);

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
    vxge_hal_mgmt_reg_type_e type,
    u32 vp_id,
    u8 *config,
    u32 *size);

/*
 * vxge_hal_mgmt_read_xfp_current_temp - Read current temparature of given port
 * @devh: HAL device handle.
 * @port: Port number
 *
 * This routine only gets the temperature for XFP modules. Also, updating of the
 * NVRAM can sometimes fail and so the reading we might get may not be uptodate.
 */
u32	vxge_hal_mgmt_read_xfp_current_temp(vxge_hal_device_h devh, u32 port);

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
vxge_hal_mgmt_pma_loopback(vxge_hal_device_h devh, u32 port, u32 enable);

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
vxge_hal_mgmt_xgmii_loopback(vxge_hal_device_h devh, u32 port, u32 enable);

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_MGMT_H */
