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

#ifndef	VXGE_HAL_REGS_H
#define	VXGE_HAL_REGS_H

__EXTERN_BEGIN_DECLS

#pragma pack(1)
/* Using this strcture to calculate offsets */
typedef struct vxge_hal_pci_config_le_t {
	u16	vendor_id;		/* 0x00 */
	u16	device_id;		/* 0x02 */

	u16	command;		/* 0x04 */
	u16	status;			/* 0x06 */

	u8	revision;		/* 0x08 */
	u8	pciClass[3];		/* 0x09 */

	u8	cache_line_size;	/* 0x0c */
	u8	latency_timer;		/* 0x0d */
	u8	header_type;		/* 0x0e */
	u8	bist;			/* 0x0f */

	u32	base_addr0_lo;		/* 0x10 */
	u32	base_addr0_hi;		/* 0x14 */

	u32	base_addr1_lo;		/* 0x18 */
	u32	base_addr1_hi;		/* 0x1C */

	u32	base_addr2_lo;		/* 0x20 */
	u32	base_addr2_hi;		/* 0x24 */

	u32	cardbus_cis_pointer;	/* 0x28 */

	u16	subsystem_vendor_id;	/* 0x2c */
	u16	subsystem_id;		/* 0x2e */

	u32	rom_base;		/* 0x30 */
	u8	capabilities_pointer;	/* 0x34 */
	u8	rsvd_35[3];		/* 0x35 */
	u32	rsvd_38;		/* 0x38 */

	u8	interrupt_line;		/* 0x3c */
	u8	interrupt_pin;		/* 0x3d */
	u8	min_grant;		/* 0x3e */
	u8	max_latency;		/* 0x3f */

	u8	rsvd_b1[VXGE_HAL_PCI_CONFIG_SPACE_SIZE - 0x40];
} vxge_hal_pci_config_le_t;	/* 0x100 */

typedef struct vxge_hal_pci_config_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u16	device_id;		/* 0x02 */
	u16	vendor_id;		/* 0x00 */

	u16	status;			/* 0x06 */
	u16	command;		/* 0x04 */

	u8	pciClass[3];		/* 0x09 */
	u8	revision;		/* 0x08 */

	u8	bist;			/* 0x0f */
	u8	header_type;		/* 0x0e */
	u8	latency_timer;		/* 0x0d */
	u8	cache_line_size;	/* 0x0c */

	u32	base_addr0_lo;		/* 0x10 */
	u32	base_addr0_hi;		/* 0x14 */

	u32	base_addr1_lo;		/* 0x18 */
	u32	base_addr1_hi;		/* 0x1C */

	u32	not_Implemented1;	/* 0x20 */
	u32	not_Implemented2;	/* 0x24 */

	u32	cardbus_cis_pointer;	/* 0x28 */

	u16	subsystem_id;		/* 0x2e */
	u16	subsystem_vendor_id;	/* 0x2c */

	u32	rom_base;		/* 0x30 */
	u8	rsvd_35[3];		/* 0x35 */
	u8	capabilities_pointer;	/* 0x34 */
	u32	rsvd_38;		/* 0x38 */

	u8	max_latency;		/* 0x3f */
	u8	min_grant;		/* 0x3e */
	u8	interrupt_pin;		/* 0x3d */
	u8	interrupt_line;		/* 0x3c */
#else
	u16	vendor_id;		/* 0x00 */
	u16	device_id;		/* 0x02 */

	u16	command;		/* 0x04 */
	u16	status;			/* 0x06 */

	u8	revision;		/* 0x08 */
	u8	pciClass[3];		/* 0x09 */

	u8	cache_line_size;	/* 0x0c */
	u8	latency_timer;		/* 0x0d */
	u8	header_type;		/* 0x0e */
	u8	bist;			/* 0x0f */

	u32	base_addr0_lo;		/* 0x10 */
	u32	base_addr0_hi;		/* 0x14 */

	u32	base_addr1_lo;		/* 0x18 */
	u32	base_addr1_hi;		/* 0x1C */

	u32	not_Implemented1;	/* 0x20 */
	u32	not_Implemented2;	/* 0x24 */

	u32	cardbus_cis_pointer;	/* 0x28 */

	u16	subsystem_vendor_id;	/* 0x2c */
	u16	subsystem_id;		/* 0x2e */

	u32	rom_base;		/* 0x30 */
	u8	capabilities_pointer;	/* 0x34 */
	u8	rsvd_35[3];		/* 0x35 */
	u32	rsvd_38;		/* 0x38 */

	u8	interrupt_line;		/* 0x3c */
	u8	interrupt_pin;		/* 0x3d */
	u8	min_grant;		/* 0x3e */
	u8	max_latency;		/* 0x3f */

#endif
	u8	rsvd_b1[VXGE_HAL_PCI_CONFIG_SPACE_SIZE - 0x40];
} vxge_hal_pci_config_t;	/* 0x100 */

#define	VXGE_HAL_EEPROM_SIZE	(0x01 << 11)

#if defined(VXGE_OS_HOST_BIG_ENDIAN)
#define	VXGE_HAL_PCI_CAP_ID(ptr)	*((ptr) + 3)
#define	VXGE_HAL_PCI_CAP_NEXT(ptr)	*((ptr) + 2)
#else
#define	VXGE_HAL_PCI_CAP_ID(ptr)	*(ptr)
#define	VXGE_HAL_PCI_CAP_NEXT(ptr)	*((ptr) + 1)
#endif

/* Capability lists */

#define	VXGE_HAL_PCI_CAP_LIST_ID	0	/* Capability ID */
#define	VXGE_HAL_PCI_CAP_ID_PM		0x01	/* Power Management */
#define	VXGE_HAL_PCI_CAP_ID_AGP		0x02	/* Accelerated Graphics Port */
#define	VXGE_HAL_PCI_CAP_ID_VPD		0x03	/* Vital Product Data */
#define	VXGE_HAL_PCI_CAP_ID_SLOTID	0x04	/* Slot Identification */
#define	VXGE_HAL_PCI_CAP_ID_MSI		0x05	/* Message Signalled Intr */
#define	VXGE_HAL_PCI_CAP_ID_CHSWP	0x06	/* CompactPCI HotSwap */
#define	VXGE_HAL_PCI_CAP_ID_PCIX	0x07	/* PCIX */
#define	VXGE_HAL_PCI_CAP_ID_HT		0x08	/* Hypertransport */
#define	VXGE_HAL_PCI_CAP_ID_VS		0x09	/* Vendor Specific */
#define	VXGE_HAL_PCI_CAP_ID_DBGPORT	0x0A	/* Debug Port */
#define	VXGE_HAL_PCI_CAP_ID_CPCICSR	0x0B	/* CompPCI central res ctrl */
#define	VXGE_HAL_PCI_CAP_ID_SHPC	0x0C	/* PCI Standard Hot-Plug Ctrl */
#define	VXGE_HAL_PCI_CAP_ID_PCIBSVID	0x0D	/* PCI Bridge Subsys Vendr Id */
#define	VXGE_HAL_PCI_CAP_ID_AGP8X	0x0E	/* AGP 8x */
#define	VXGE_HAL_PCI_CAP_ID_SECDEV	0x0F	/* Secure Device */
#define	VXGE_HAL_PCI_CAP_ID_PCIE	0x10	/* PCI Express */
#define	VXGE_HAL_PCI_CAP_ID_MSIX	0x11	/* MSI-X */
#define	VXGE_HAL_PCI_CAP_LIST_NEXT	1	/* Next cap in the list */
#define	VXGE_HAL_PCI_CAP_FLAGS		2	/* Cap defined flags(16 bits) */

typedef struct vxge_hal_pm_capability_le_t {
	u8	capability_id;
	u8	next_capability_ptr;
	u16	capabilities_reg;
#define	VXGE_HAL_PCI_PM_CAP_VER_MASK	0x0007	/* Version */
#define	VXGE_HAL_PCI_PM_CAP_PME_CLOCK	0x0008	/* PME clock required */
#define	VXGE_HAL_PCI_PM_CAP_AUX_POWER	0x0010	/* Auxiliary power support */
#define	VXGE_HAL_PCI_PM_CAP_DSI		0x0020	/* Device specific init */
#define	VXGE_HAL_PCI_PM_AUX_CURRENT	0x01C0	/* Auxiliary current reqs */
#define	VXGE_HAL_PCI_PM_CAP_D1		0x0200	/* D1 power state support */
#define	VXGE_HAL_PCI_PM_CAP_D2		0x0400	/* D2 power state support */
#define	VXGE_HAL_PCI_PM_CAP_PME_D0	0x0800	/* PME# assertable from D0 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D1	0x1000	/* PME# assertable from D1 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D2	0x2000	/* PME# assertable from D2 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D3_HOT	0x4000	/* PME# assertable from D3hot */
#define	VXGE_HAL_PCI_PM_CAP_PME_D3_COLD	0x8000	/* PME# assertable from D3cold */
	u16	pm_ctrl;
#define	VXGE_HAL_PCI_PM_CTRL_STATE_MASK	0x0003	/* Curr power state(D0 to D3) */
#define	VXGE_HAL_PCI_PM_CTRL_NO_SOFT_RESET 0x0008	/* trans from D3hot to D0 */
#define	VXGE_HAL_PCI_PM_CTRL_PME_ENABLE	0x0100	/* PME pin enable */
#define	VXGE_HAL_PCI_PM_CTRL_DATA_SEL_MASK 0x1e00	/* Data select (??) */
#define	VXGE_HAL_PCI_PM_CTRL_DATA_SCALE_MASK 0x6000	/* Data scale (??) */
#define	VXGE_HAL_PCI_PM_CTRL_PME_STATUS	0x8000	/* PME pin status */
	u8	pm_ppb_ext;
#define	VXGE_HAL_PCI_PM_PPB_B2_B3	0x40	/* Stop clk when in D3hot(??) */
#define	VXGE_HAL_PCI_PM_BPCC_ENABLE	0x80	/* Bus pwr/clk ctrl enable(??) */
	u8	pm_data_reg;
} vxge_hal_pm_capability_le_t;

typedef struct vxge_hal_pm_capability_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u16	capabilities_reg;
#define	VXGE_HAL_PCI_PM_CAP_VER_MASK	0x0007	/* Version */
#define	VXGE_HAL_PCI_PM_CAP_PME_CLOCK   0x0008	/* PME clock required */
#define	VXGE_HAL_PCI_PM_CAP_AUX_POWER   0x0010	/* Auxiliary power support */
#define	VXGE_HAL_PCI_PM_CAP_DSI		0x0020	/* Dev specific init */
#define	VXGE_HAL_PCI_PM_AUX_CURRENT	0x01C0	/* Auxiliary current reqs */
#define	VXGE_HAL_PCI_PM_CAP_D1		0x0200	/* D1 power state support */
#define	VXGE_HAL_PCI_PM_CAP_D2		0x0400	/* D2 power state support */
#define	VXGE_HAL_PCI_PM_CAP_PME_D0	0x0800	/* PME# assertable from D0 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D1	0x1000	/* PME# assertable from D1 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D2	0x2000	/* PME# assertable from D2 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D3_HOT  0x4000	/* PME# assertable from D3hot */
#define	VXGE_HAL_PCI_PM_CAP_PME_D3_COLD 0x8000	/* PME# assertable from D3cold */
	u8	next_capability_ptr;
	u8	capability_id;
	u8	pm_data_reg;
	u8	pm_ppb_ext;
#define	VXGE_HAL_PCI_PM_PPB_B2_B3	0x40	/* Stop clk when in D3hot(??) */
#define	VXGE_HAL_PCI_PM_BPCC_ENABLE	0x80	/* Bus pwr/clk ctrl enable(??) */
	u16	pm_ctrl;
#define	VXGE_HAL_PCI_PM_CTRL_STATE_MASK	0x0003	/* Curr pwr state (D0 to D3) */
#define	VXGE_HAL_PCI_PM_CTRL_NO_SOFT_RESET 0x0008	/* dev trans D3hot to D0 */
#define	VXGE_HAL_PCI_PM_CTRL_PME_ENABLE	0x0100	/* PME pin enable */
#define	VXGE_HAL_PCI_PM_CTRL_DATA_SEL_MASK  0x1e00	/* Data select (??) */
#define	VXGE_HAL_PCI_PM_CTRL_DATA_SCALE_MASK 0x6000	/* Data scale (??) */
#define	VXGE_HAL_PCI_PM_CTRL_PME_STATUS	0x8000	/* PME pin status */
#else
	u8	capability_id;
	u8	next_capability_ptr;
	u16	capabilities_reg;
#define	VXGE_HAL_PCI_PM_CAP_VER_MASK	0x0007	/* Version */
#define	VXGE_HAL_PCI_PM_CAP_PME_CLOCK   0x0008	/* PME clock required */
#define	VXGE_HAL_PCI_PM_CAP_AUX_POWER   0x0010	/* Auxiliary power support */
#define	VXGE_HAL_PCI_PM_CAP_DSI		0x0020	/* Dev specific init */
#define	VXGE_HAL_PCI_PM_AUX_CURRENT	0x01C0	/* Auxiliary curr reqs */
#define	VXGE_HAL_PCI_PM_CAP_D1		0x0200	/* D1 power state support */
#define	VXGE_HAL_PCI_PM_CAP_D2		0x0400	/* D2 power state support */
#define	VXGE_HAL_PCI_PM_CAP_PME_D0	0x0800	/* PME# assertable from D0 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D1	0x1000	/* PME# assertable from D1 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D2	0x2000	/* PME# assertable from D2 */
#define	VXGE_HAL_PCI_PM_CAP_PME_D3_HOT  0x4000	/* PME# assertable from D3hot */
#define	VXGE_HAL_PCI_PM_CAP_PME_D3_COLD 0x8000	/* PME# assertable from D3cold */
	u16	pm_ctrl;
#define	VXGE_HAL_PCI_PM_CTRL_STATE_MASK	0x0003	/* Curr pwr state (D0 to D3) */
#define	VXGE_HAL_PCI_PM_CTRL_NO_SOFT_RESET 0x0008	/* dev trans D3hot to D0 */
#define	VXGE_HAL_PCI_PM_CTRL_PME_ENABLE	0x0100	/* PME pin enable */
#define	VXGE_HAL_PCI_PM_CTRL_DATA_SEL_MASK 0x1e00	/* Data select (??) */
#define	VXGE_HAL_PCI_PM_CTRL_DATA_SCALE_MASK 0x6000	/* Data scale (??) */
#define	VXGE_HAL_PCI_PM_CTRL_PME_STATUS	0x8000	/* PME pin status */
	u8	pm_ppb_ext;
#define	VXGE_HAL_PCI_PM_PPB_B2_B3	0x40	/* Stop clk when in D3hot(??) */
#define	VXGE_HAL_PCI_PM_BPCC_ENABLE	0x80	/* Bus pwr/clk ctrl enable(??) */
	u8	pm_data_reg;
#endif
} vxge_hal_pm_capability_t;

typedef struct vxge_hal_vpid_capability_le_t {
	u8	capability_id;
	u8	next_capability_ptr;
	u16	vpd_address;
#define	VXGE_HAL_PCI_VPID_COMPL_FALG	0x8000	/* Read Completion Flag */
	u32	vpd_data;
} vxge_hal_vpid_capability_le_t;

typedef struct vxge_hal_vpid_capability_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u16	vpd_address;
#define	VXGE_HAL_PCI_VPID_COMPL_FALG	0x8000	/* Read Completion Flag */
	u8	next_capability_ptr;
	u8	capability_id;
	u32	vpd_data;
#else
	u8	capability_id;
	u8	next_capability_ptr;
	u16	vpd_address;
#define	VXGE_HAL_PCI_VPID_COMPL_FALG	0x8000	/* Read Completion Flag */
	u32	vpd_data;
#endif
} vxge_hal_vpid_capability_t;

typedef struct vxge_hal_sid_capability_le_t {
	u8	capability_id;
	u8	next_capability_ptr;
	u8	sid_esr;
#define	VXGE_HAL_PCI_SID_ESR_NSLOTS	0x1f	/* Num of exp slots avail */
#define	VXGE_HAL_PCI_SID_ESR_FIC	0x20	/* First In Chassis Flag */
	u8	sid_chasis_nr;
} vxge_hal_sid_capability_le_t;

typedef struct vxge_hal_sid_capability_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u8	sid_chasis_nr;
	u8	sid_esr;
#define	VXGE_HAL_PCI_SID_ESR_NSLOTS	0x1f	/* Num of exp slots avail */
#define	VXGE_HAL_PCI_SID_ESR_FIC	0x20	/* First In Chassis Flag */
	u8	next_capability_ptr;
	u8	capability_id;
#else
	u8	capability_id;
	u8	next_capability_ptr;
	u8	sid_esr;
#define	VXGE_HAL_PCI_SID_ESR_NSLOTS	0x1f	/* Num of exp slots avail */
#define	VXGE_HAL_PCI_SID_ESR_FIC	0x20	/* First In Chassis Flag */
	u8	sid_chasis_nr;
#endif
} vxge_hal_sid_capability_t;

typedef struct vxge_hal_msi_capability_le_t {
	u8	capability_id;
	u8	next_capability_ptr;
	u16	msi_control;
#define	VXGE_HAL_PCI_MSI_FLAGS_PVMASK	0x0100	/* Per Vector Masking Capable */
#define	VXGE_HAL_PCI_MSI_FLAGS_64BIT	0x0080	/* 64-bit addresses allowed */
#define	VXGE_HAL_PCI_MSI_FLAGS_QSIZE	0x0070	/* Msg queue size configured */
#define	VXGE_HAL_PCI_MSI_FLAGS_QMASK	0x000e	/* Max queue size available */
#define	VXGE_HAL_PCI_MSI_FLAGS_ENABLE   0x0001	/* MSI feature enabled */
	union {
		struct {
			u32	msi_addr;
			u16	msi_data;
			u16	msi_unused;
		} ma32_no_pvm;
		struct {
			u32	msi_addr;
			u16	msi_data;
			u16	msi_unused;
			u32	msi_mask;
			u32	msi_pending;
		} ma32_pvm;
		struct {
			u32	msi_addr_lo;
			u32	msi_addr_hi;
			u16	msi_data;
			u16	msi_unused;
		} ma64_no_pvm;
		struct {
			u32	msi_addr_lo;
			u32	msi_addr_hi;
			u16	msi_data;
			u16	msi_unused;
			u32	msi_mask;
			u32	msi_pending;
		} ma64_pvm;
	} au;
} vxge_hal_msi_capability_le_t;

typedef struct vxge_hal_msi_capability_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u16	msi_control;
#define	VXGE_HAL_PCI_MSI_FLAGS_PVMASK	0x0100	/* Per Vector Masking Capable */
#define	VXGE_HAL_PCI_MSI_FLAGS_64BIT	0x0080	/* 64-bit addresses allowed */
#define	VXGE_HAL_PCI_MSI_FLAGS_QSIZE	0x0070	/* Msg queue size configured */
#define	VXGE_HAL_PCI_MSI_FLAGS_QMASK	0x000e	/* Max queue size available */
#define	VXGE_HAL_PCI_MSI_FLAGS_ENABLE   0x0001	/* MSI feature enabled */
	u8	next_capability_ptr;
	u8	capability_id;
	union {
		struct {
			u32	msi_addr;
			u16	msi_unused;
			u16	msi_data;
		} ma32_no_pvm;
		struct {
			u32	msi_addr;
			u16	msi_unused;
			u16	msi_data;
			u32	msi_mask;
			u32	msi_pending;
		} ma32_pvm;
		struct {
			u32	msi_addr_lo;
			u32	msi_addr_hi;
			u16	msi_unused;
			u16	msi_data;
		} ma64_no_pvm;
		struct {
			u32	msi_addr_lo;
			u32	msi_addr_hi;
			u16	msi_unused;
			u16	msi_data;
			u32	msi_mask;
			u32	msi_pending;
		} ma64_pvm;
	} au;
#else
	u8	capability_id;
	u8	next_capability_ptr;
	u16	msi_control;
#define	VXGE_HAL_PCI_MSI_FLAGS_PVMASK	0x0100	/* Per Vector Masking Capable */
#define	VXGE_HAL_PCI_MSI_FLAGS_64BIT	0x0080	/* 64-bit addresses allowed */
#define	VXGE_HAL_PCI_MSI_FLAGS_QSIZE	0x0070	/* Msg queue size configured */
#define	VXGE_HAL_PCI_MSI_FLAGS_QMASK	0x000e	/* Max queue size available */
#define	VXGE_HAL_PCI_MSI_FLAGS_ENABLE   0x0001	/* MSI feature enabled */
	union {
		struct {
			u32	msi_addr;
			u16	msi_data;
			u16	msi_unused;
		} ma32_no_pvm;
		struct {
			u32	msi_addr;
			u16	msi_data;
			u16	msi_unused;
			u32	msi_mask;
			u32	msi_pending;
		} ma32_pvm;
		struct {
			u32	msi_addr_lo;
			u32	msi_addr_hi;
			u16	msi_data;
			u16	msi_unused;
		} ma64_no_pvm;
		struct {
			u32	msi_addr_lo;
			u32	msi_addr_hi;
			u16	msi_data;
			u16	msi_unused;
			u32	msi_mask;
			u32	msi_pending;
		} ma64_pvm;
	} au;
#endif
} vxge_hal_msi_capability_t;

typedef struct vxge_hal_chswp_capability_le_t {
	u8	capability_id;
	u8	next_capability_ptr;
	u8	chswp_csr;
#define	VXGE_HAL_PCI_CHSWP_DHA	 0x01	/* Device Hiding Arm */
#define	VXGE_HAL_PCI_CHSWP_EIM	 0x02	/* ENUM# Signal Mask */
#define	VXGE_HAL_PCI_CHSWP_PIE	 0x04	/* Pending Insert or Extract */
#define	VXGE_HAL_PCI_CHSWP_LOO	 0x08	/* LED On / Off */
#define	VXGE_HAL_PCI_CHSWP_PI	  0x30	/* Programming Interface */
#define	VXGE_HAL_PCI_CHSWP_EXT	 0x40	/* ENUM# status - extraction */
#define	VXGE_HAL_PCI_CHSWP_INS	 0x80	/* ENUM# status - insertion */
} vxge_hal_chswp_capability_le_t;

typedef struct vxge_hal_chswp_capability_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u8	chswp_csr;
#define	VXGE_HAL_PCI_CHSWP_DHA	 0x01	/* Device Hiding Arm */
#define	VXGE_HAL_PCI_CHSWP_EIM	 0x02	/* ENUM# Signal Mask */
#define	VXGE_HAL_PCI_CHSWP_PIE	 0x04	/* Pending Insert or Extract */
#define	VXGE_HAL_PCI_CHSWP_LOO	 0x08	/* LED On / Off */
#define	VXGE_HAL_PCI_CHSWP_PI	  0x30	/* Programming Interface */
#define	VXGE_HAL_PCI_CHSWP_EXT	 0x40	/* ENUM# status - extraction */
#define	VXGE_HAL_PCI_CHSWP_INS	 0x80	/* ENUM# status - insertion */
	u8	next_capability_ptr;
	u8	capability_id;
#else
	u8	capability_id;
	u8	next_capability_ptr;
	u8	chswp_csr;
#define	VXGE_HAL_PCI_CHSWP_DHA	 0x01	/* Device Hiding Arm */
#define	VXGE_HAL_PCI_CHSWP_EIM	 0x02	/* ENUM# Signal Mask */
#define	VXGE_HAL_PCI_CHSWP_PIE	 0x04	/* Pending Insert or Extract */
#define	VXGE_HAL_PCI_CHSWP_LOO	 0x08	/* LED On / Off */
#define	VXGE_HAL_PCI_CHSWP_PI	  0x30	/* Programming Interface */
#define	VXGE_HAL_PCI_CHSWP_EXT	 0x40	/* ENUM# status - extraction */
#define	VXGE_HAL_PCI_CHSWP_INS	 0x80	/* ENUM# status - insertion */
#endif
} vxge_hal_chswp_capability_t;

typedef struct vxge_hal_shpc_capability_le_t {
	u8	capability_id;
	u8	next_capability_ptr;
} vxge_hal_shpc_capability_le_t;

typedef struct vxge_hal_shpc_capability_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u8	next_capability_ptr;
	u8	capability_id;
#else
	u8	capability_id;
	u8	next_capability_ptr;
#endif
} vxge_hal_shpc_capability_t;

typedef struct vxge_hal_msix_capability_le_t {
	u8	capability_id;
	u8	next_capability_ptr;
	u16	msix_control;
#define	VXGE_HAL_PCI_MSIX_FLAGS_ENABLE	0x8000	/* MSIX Enable */
#define	VXGE_HAL_PCI_MSIX_FLAGS_MASK	0x4000	/* Mask all vectors */
#define	VXGE_HAL_PCI_MSIX_FLAGS_TSIZE	0x001f	/* Table Size */
	u32	table_offset;
#define	VXGE_HAL_PCI_MSIX_TABLE_OFFSET	0xFFFFFFF8	/* Table offset mask */
#define	VXGE_HAL_PCI_MSIX_TABLE_BIR	0x00000007	/* Table BIR mask */
	u32	pba_offset;
#define	VXGE_HAL_PCI_MSIX_PBA_OFFSET	0xFFFFFFF8	/* Table offset mask */
#define	VXGE_HAL_PCI_MSIX_PBA_BIR	0x00000007	/* Table BIR mask */
} vxge_hal_msix_capability_le_t;

typedef struct vxge_hal_msix_capability_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u16	msix_control;
#define	VXGE_HAL_PCI_MSIX_FLAGS_ENABLE	0x8000	/* MSIX Enable */
#define	VXGE_HAL_PCI_MSIX_FLAGS_MASK	0x4000	/* Mask all vectors */
#define	VXGE_HAL_PCI_MSIX_FLAGS_TSIZE	0x001f	/* Table Size */
	u8	next_capability_ptr;
	u8	capability_id;
	u32	table_offset;
#define	VXGE_HAL_PCI_MSIX_TABLE_OFFSET	0xFFFFFFF8	/* Table offset mask */
#define	VXGE_HAL_PCI_MSIX_TABLE_BIR	0x00000007	/* Table BIR mask */
	u32	pba_offset;
#define	VXGE_HAL_PCI_MSIX_PBA_OFFSET	0xFFFFFFF8	/* Table offset mask */
#define	VXGE_HAL_PCI_MSIX_PBA_BIR	0x00000007	/* Table BIR mask */
#else
	u8	capability_id;
	u8	next_capability_ptr;
	u16	msix_control;
#define	VXGE_HAL_PCI_MSIX_FLAGS_ENABLE	0x8000	/* MSIX Enable */
#define	VXGE_HAL_PCI_MSIX_FLAGS_MASK	0x4000	/* Mask all vectors */
#define	VXGE_HAL_PCI_MSIX_FLAGS_TSIZE	0x001f	/* Table Size */
	u32	table_offset;
#define	VXGE_HAL_PCI_MSIX_TABLE_OFFSET	0xFFFFFFF8	/* Table offset mask */
#define	VXGE_HAL_PCI_MSIX_TABLE_BIR	0x00000007	/* Table BIR mask */
	u32	pba_offset;
#define	VXGE_HAL_PCI_MSIX_PBA_OFFSET	0xFFFFFFF8	/* Table offset mask */
#define	VXGE_HAL_PCI_MSIX_PBA_BIR	0x00000007	/* Table BIR mask */
#endif
} vxge_hal_msix_capability_t;

typedef struct vxge_hal_pci_caps_offset_t {
	u32	pm_cap_offset;
	u32	vpd_cap_offset;
	u32	sid_cap_offset;
	u32	msi_cap_offset;
	u32	vs_cap_offset;
	u32	shpc_cap_offset;
	u32	msix_cap_offset;
} vxge_hal_pci_caps_offset_t;

typedef struct vxge_hal_pci_e_capability_le_t {
	u8	capability_id;
	u8	next_capability_ptr;
	u16	pci_e_flags;
#define	VXGE_HAL_PCI_EXP_FLAGS_VERS	0x000f	/* Capability version */
#define	VXGE_HAL_PCI_EXP_FLAGS_TYPE	0x00f0	/* Device/Port type */
#define	VXGE_HAL_PCI_EXP_TYPE_ENDPOINT	0x0	/* Express Endpoint */
#define	VXGE_HAL_PCI_EXP_TYPE_LEG_END	0x1	/* Legacy Endpoint */
#define	VXGE_HAL_PCI_EXP_TYPE_ROOT_PORT	0x4	/* Root Port */
#define	VXGE_HAL_PCI_EXP_TYPE_UPSTREAM	0x5	/* Upstream Port */
#define	VXGE_HAL_PCI_EXP_TYPE_DOWNSTREAM 0x6	/* Downstream Port */
#define	VXGE_HAL_PCI_EXP_TYPE_PCI_BRIDGE 0x7	/* PCI/PCI-X Bridge */
#define	VXGE_HAL_PCI_EXP_FLAGS_SLOT	0x0100	/* Slot implemented */
#define	VXGE_HAL_PCI_EXP_FLAGS_IRQ	0x3e00	/* Interrupt msg number */
	u32	pci_e_devcap;
#define	VXGE_HAL_PCI_EXP_DEVCAP_PAYLOAD 0x07	/* Max_Payload_Size */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PHANTOM 0x18	/* Phantom functions */
#define	VXGE_HAL_PCI_EXP_DEVCAP_EXT_TAG 0x20	/* Extended tags */
#define	VXGE_HAL_PCI_EXP_DEVCAP_L0S	0x1c0	/* L0s Acceptable Latency */
#define	VXGE_HAL_PCI_EXP_DEVCAP_L1	0xe00	/* L1 Acceptable Latency */
#define	VXGE_HAL_PCI_EXP_DEVCAP_ATN_BUT	0x1000	/* Attention Button Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_ATN_IND	0x2000	/* Attention Ind Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_IND	0x4000	/* Power Indicator Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_VAL	0x3fc0000	/* Slot Power Limit Value */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_SCL 0xc000000	/* Slot Power Limit Scale */
	u16	pci_e_devctl;
#define	VXGE_HAL_PCI_EXP_DEVCTL_CERE	0x0001	/* Correctable Err Report En. */
#define	VXGE_HAL_PCI_EXP_DEVCTL_NFERE   0x0002	/* Non-Fatal Err Report En */
#define	VXGE_HAL_PCI_EXP_DEVCTL_FERE	0x0004	/* Fatal Error Report En */
#define	VXGE_HAL_PCI_EXP_DEVCTL_URRE	0x0008	/* Unsupported Req Report En. */
#define	VXGE_HAL_PCI_EXP_DEVCTL_RELAX_EN 0x0010	/* Enable relaxed ordering */
#define	VXGE_HAL_PCI_EXP_DEVCTL_PAYLOAD	0x00e0	/* Max_Payload_Size */
#define	VXGE_HAL_PCI_EXP_DEVCTL_EXT_TAG	0x0100	/* Extended Tag Field Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_PHANTOM	0x0200	/* Phantom Functions Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_AUX_PME	0x0400	/* Auxiliary Power PM Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_NOSNOOP_EN 0x0800	/* Enable No Snoop */
#define	VXGE_HAL_PCI_EXP_DEVCTL_READRQ	0x7000	/* Max_Read_Request_Size */
	u16	pci_e_devsta;
#define	VXGE_HAL_PCI_EXP_DEVSTA_CED	0x01	/* Correctable Error Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_NFED	0x02	/* Non-Fatal Error Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_FED	0x04	/* Fatal Error Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_URD	0x08	/* Unsupported Req Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_AUXPD	0x10	/* AUX Power Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_TRPND	0x20	/* Transactions Pending */
	u32	pci_e_lnkcap;
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_SPEED 0xf	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LS_2_5	0x1	/* 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LS_5	0x2	/* 5 and 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_WIDTH 0x3f0	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_RES	0x0	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X1	0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X2	0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X4	0x4	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X8	0x8	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X12	0xa	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X16	0x10	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X32	0x20	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_ASPM  0xc00	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_RES1  0x0	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_LO	0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_RES2  0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_L0_L1 0x3	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_LAT	0x7000	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_LT_64	0x0	/* Less than 64ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_64_128   0x1	/* 64ns to less than 128ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_128_256  0x2	/* 128ns to less than 256ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_256_512  0x3	/* 256ns to less than 512ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_512_1us  0x4	/* 512ns to less than 1s. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_1us_2us  0x5	/* 1s to less than 2s. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_2us_4us  0x6	/* 2s-4s. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_GT_4us   0x7	/* More than 4s. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_LAT	    0x38000	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_LT_1us   0x0	/* Less than 1us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_1us_2us  0x1	/* 1us to less than 2us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_2us_4us  0x2	/* 2us to less than 4us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_4us_8us  0x3	/* 4us to less than 8us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_8us_16us 0x4	/* 8us to less than 16us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_16us_32us 0x5	/* 16us to less than 32us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_32us_64us 0x6	/* 32us-64us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_GT_64us   0x7	/* More than 64us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_CLK_PWR_MGMT 0x40000	/* Clk power management. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_DOWN_ERR_CAP 0x80000	/* Down error capable. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_ACT_CAP  0x100000	/* DL active rep cap. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_BW_CAP   0x200000	/* DL bw reporting cap. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_PORT_NUM 0xff000000	/* Port number. */
	u16	pci_e_lnkctl;
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM	    0x3	/* ASPM Control. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_DISABLED 0x0	/* Disabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L0_EN  0x1	/* L0 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L1_EN  0x2	/* L1 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L0_L1_EN 0x3	/* L0 & L1 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB	    0x8	/* Read Completion Boundary. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB_64	    0x0	/* RCB 64 bytes. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB_128	    0x1	/* RCB 128 bytes. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_DISABLED    0x10	/* Disables the link. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RETRAIN	    0x20	/* Retrain the link. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_CCCFG	    0x40	/* Common clock config. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_EXT_SYNC    0x80	/* Extended Sync. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_CLK_PWRMGMT 0x100	/* Enable clk pwr mgmt. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_HW_AUTO_DIS 0x200	/* Hw autonomous with dis */
#define	VXGE_HAL_PCI_EXP_LNKCTL_BWM_INTR_EN 0x400	/* Bw mgt interrupt enable */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ABW_INTR_EN 0x800	/* Autonomous BW intr en */
	u16	pci_e_lnksta;
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_SPEED   0xf	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LS_2_5	    0x1	/* 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LS_5	    0x2	/* 5 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_WIDTH   0x3f0	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_RES	    0x0		/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X1	    0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X2	    0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X4	    0x4	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X8	    0x8	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X12	    0xa	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X16	    0x10	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X32	    0x20	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_TRAIN   0x800	/* Link training. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_SCLK_CFG    0x1000	/* Slot Clock Config. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_DLL_ACTIVE  0x2000	/* Data LL Active. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_BWM_STA	    0x4000	/* Bw mgmt intr enable */
#define	VXGE_HAL_PCI_EXP_LNKSTA_ABW_STA	    0x8000	/* Autonomous BW intr en */
	u32	pci_e_stlcap;
#define	VXGE_HAL_PCI_EXP_STLCAP_ATTN_BTTN   0x1	/* Attention Button Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_PWR_CTRL    0x2	/* Power Control Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_MRL_SENS    0x4	/* MRL Sesor Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_ATTN_IND    0x8	/* Attention Ind Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_PWR_IND	    0x10	/* Power Indicator Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_HP_SURP	    0x20	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_HP_CAP	    0x40	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_VAL  0x7F80	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_250  0xF0	/* 250 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_275  0xF1	/* 275 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_300  0xF2	/* 300 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_LIM  0x18000	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_1X   0x0	/* 1.0x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY10 0x1	/* 0.1x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY100 0x2	/* 0.01x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY1000 0x3	/* 0.001x */
#define	VXGE_HAL_PCI_EXP_STLCAP_EM_INTR_LOCK 0x20000	/* Ele-mec Intrlock Pres */
#define	VXGE_HAL_PCI_EXP_STLCAP_NO_CMD_CMPL  0x40000	/* No Cmd Compl Support */
#define	VXGE_HAL_PCI_EXP_STLCAP_PHY_SL_NO   0xFFF80000	/* Phys Slot Number */
	u16	pci_e_stlctl;
#define	VXGE_HAL_PCI_EXP_STLCTL_ATTN_BTN_EN  0x1	/* Atten Btn pressed enable */
#define	VXGE_HAL_PCI_EXP_STLCTL_PF_DET_EN    0x2	/* Power Fault Detected En */
#define	VXGE_HAL_PCI_EXP_STLCTL_MRL_SENS_EN  0x4	/* MRL Sensor Changed Enable */
#define	VXGE_HAL_PCI_EXP_STLCTL_PDET_CH_EN   0x8	/* Presence Detect Change En */
#define	VXGE_HAL_PCI_EXP_STLCTL_CC_INTR_EN   0x10	/* Cmd Compl Intr Enable */
#define	VXGE_HAL_PCI_EXP_STLCTL_HP_INTR_EN   0x20	/* Hot-Plug Intr Enable */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_CTRL 0xC0	/* Attention Ind Control */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_RES  0x0	/* Reserved */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_ON   0x1	/* On */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_BLNK 0x2	/* Blink */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_OFF  0x3	/* Off */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_CTRL 0x300	/* POwer Indicator Control */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_RES  0x0	/* Reserved */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_ON   0x1	/* On */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_BLNK 0x2	/* Blink */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_OFF  0x3	/* Off */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_CTRL 0x400	/* Power Controller Ctrl */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_on   0x0	/* Power on */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_off  0x1	/* Power off */
#define	VXGE_HAL_PCI_EXP_STLCTL_EM_IL_CTRL   0x800	/* Ele-mec Interlock Crl */
#define	VXGE_HAL_PCI_EXP_STLCTL_DLL_ST_CH_EN 0x1000	/* DL Layer State Ch En */
	u16	pci_e_stlsta;
#define	VXGE_HAL_PCI_EXP_STLSTA_ATTN_BTN    0x1	/* Attention Button Pressed */
#define	VXGE_HAL_PCI_EXP_STLSTA_PF_DET	    0x2	/* Power Fault Detected */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_CH 0x4	/* MRL Sensor Changed */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_CH	    0x8	/* Presence Detect Changed */
#define	VXGE_HAL_PCI_EXP_STLSTA_CMD_COMPL   0x10	/* Command Completed */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_STA 0x20	/* MRL Sensor State */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_CL 0x0	/* MRL Sensor State - closed */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_OP 0x1	/* MRL Sensor State - open */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_STA    0x400	/* Presence Detect State */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_EMPTY  0x0	/* Clost Empty */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_PRESENT 0x1	/* Card Present */
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_STA   0x80	/* Ele-mec Intrlock Control */
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_DIS   0x0	/* Disengaged */
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_EN    0x1	/* Engaged */
#define	VXGE_HAL_PCI_EXP_STLSTA_DLL_ST_CH   0x100	/* DL Layer State Changed */
	u16	pci_e_rtctl;
#define	VXGE_HAL_PCI_EXP_RTCTL_SECEE	0x01	/* Sys Err on Correctable Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_SENFEE	0x02	/* Sys Err on Non-Fatal Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_SEFEE	0x04	/* Sys Err on Fatal Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_PMEIE	0x08	/* PME Interrupt Enable */
#define	VXGE_HAL_PCI_EXP_RTCTL_CRSSVE	0x10	/* CRS SW Visibility Enable */
	u16	pci_e_rtcap;
#define	VXGE_HAL_PCI_EXP_RTCAP_CRS_SW_VIS   0x01	/* CRS SW Visibility */
	u32	pci_e_rtsta;
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_REQ_ID   0xFFFF	/* PME Requestor ID */
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_STATUS   0x10000	/* PME status */
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_PENDING  0x20000	/* PME Pending */
} vxge_hal_pci_e_capability_le_t;

typedef struct vxge_hal_pci_e_capability_t {
#if defined(VXGE_OS_HOST_BIG_ENDIAN)
	u16	pci_e_flags;
#define	VXGE_HAL_PCI_EXP_FLAGS_VERS	0x000f	/* Capability version */
#define	VXGE_HAL_PCI_EXP_FLAGS_TYPE	0x00f0	/* Device/Port type */
#define	VXGE_HAL_PCI_EXP_TYPE_ENDPOINT	0x0	/* Express Endpoint */
#define	VXGE_HAL_PCI_EXP_TYPE_LEG_END	0x1	/* Legacy Endpoint */
#define	VXGE_HAL_PCI_EXP_TYPE_ROOT_PORT	0x4	/* Root Port */
#define	VXGE_HAL_PCI_EXP_TYPE_UPSTREAM	0x5	/* Upstream Port */
#define	VXGE_HAL_PCI_EXP_TYPE_DOWNSTREAM 0x6	/* Downstream Port */
#define	VXGE_HAL_PCI_EXP_TYPE_PCI_BRIDGE 0x7	/* PCI/PCI-X Bridge */
#define	VXGE_HAL_PCI_EXP_FLAGS_SLOT	0x0100	/* Slot implemented */
#define	VXGE_HAL_PCI_EXP_FLAGS_IRQ	0x3e00	/* Interrupt message number */
	u8	next_capability_ptr;
	u8	capability_id;
	u32	pci_e_devcap;
#define	VXGE_HAL_PCI_EXP_DEVCAP_PAYLOAD	0x07	/* Max_Payload_Size */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PHANTOM	0x18	/* Phantom functions */
#define	VXGE_HAL_PCI_EXP_DEVCAP_EXT_TAG	0x20	/* Extended tags */
#define	VXGE_HAL_PCI_EXP_DEVCAP_L0S	0x1c0	/* L0s Acceptable Latency */
#define	VXGE_HAL_PCI_EXP_DEVCAP_L1	0xe00	/* L1 Acceptable Latency */
#define	VXGE_HAL_PCI_EXP_DEVCAP_ATN_BUT	0x1000	/* Attention Button Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_ATN_IND	0x2000	/* Attention Ind Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_IND	0x4000	/* Power Indicator Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_VAL	0x3fc0000	/* Slot Power Limit Value */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_SCL	0xc000000	/* Slot Power Limit Scale */
	u16	pci_e_devsta;
#define	VXGE_HAL_PCI_EXP_DEVSTA_CED	0x01	/* Correctable Err Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_NFED	0x02	/* Non-Fatal Error Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_FED	0x04	/* Fatal Error Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_URD	0x08	/* Unsupported Req Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_AUXPD	0x10	/* AUX Power Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_TRPND	0x20	/* Transactions Pending */
	u16	pci_e_devctl;
#define	VXGE_HAL_PCI_EXP_DEVCTL_CERE	0x0001	/* Corr'ble Err Reporting En. */
#define	VXGE_HAL_PCI_EXP_DEVCTL_NFERE	0x0002	/* Non-Fatal Err Reporting En */
#define	VXGE_HAL_PCI_EXP_DEVCTL_FERE	0x0004	/* Fatal Error Reporting En */
#define	VXGE_HAL_PCI_EXP_DEVCTL_URRE	0x0008	/* Unsupp Req Reporting En. */
#define	VXGE_HAL_PCI_EXP_DEVCTL_RELAX_EN 0x0010	/* Enable relaxed ordering */
#define	VXGE_HAL_PCI_EXP_DEVCTL_PAYLOAD	0x00e0	/* Max_Payload_Size */
#define	VXGE_HAL_PCI_EXP_DEVCTL_EXT_TAG	0x0100	/* Extended Tag Field Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_PHANTOM	0x0200	/* Phantom Functions Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_AUX_PME	0x0400	/* Auxiliary Power PM Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_NOSNOOP_EN 0x0800	/* Enable No Snoop */
#define	VXGE_HAL_PCI_EXP_DEVCTL_READRQ	0x7000	/* Max_Read_Request_Size */
	u32	pci_e_lnkcap;
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_SPEED 0xf	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LS_2_5	  0x1	/* 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LS_5	  0x2	/* 5 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_WIDTH 0x3f0	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_RES	  0x0	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X1	  0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X2	  0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X4	  0x4	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X8	  0x8	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X12	  0xa	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X16	  0x10	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X32	  0x20	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_ASPM  0xc00	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_RES1 0x0	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_LO  0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_RES2 0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_L0_L1 0x3	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_LAT	  0x7000	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_LT_64  0x0	/* Less than 64 ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_64_128 0x1	/* 64 ns to less than 128 ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_128_256 0x2	/* 128 ns to less than 256 ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_256_512 0x3	/* 256 ns to less than 512 ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_512_1us 0x4	/* 512 ns to less than 1us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_1us_2us 0x5	/* 1us to less than 2us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_2us_4us 0x6	/* 2us-4us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_GT_4us  0x7	/* More than 4us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_LAT	   0x38000	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_LT_1us  0x0	/* Less than 1us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_1us_2us   0x1	/* 1us to less than 2us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_2us_4us   0x2	/* 2us to less than 4us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_4us_8us   0x3	/* 4us to less than 8us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_8us_16us  0x4	/* 8us to less than 16us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_16us_32us 0x5	/* 16us to less than 32us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_32us_64us 0x6	/* 32us-64s. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_GT_64us   0x7	/* More than 64us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_CLK_PWR_MGMT 0x40000	/* Clk power management. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_DOWN_ERR_CAP 0x80000	/* Down error capable. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_ACT_CAP  0x100000	/* DL active rep cap. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_BW_CAP   0x200000	/* DL bw rep cap. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_PORT_NUM 0xff000000	/* Port number. */
	u16	pci_e_lnksta;
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_SPEED  0xf	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LS_2_5	   0x1	/* 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LS_5	   0x2	/* 5 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_WIDTH  0x3f0	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_RES	   0x0	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X1	   0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X2	   0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X4	   0x4	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X8	   0x8	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X12	   0xa	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X16	   0x10	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X32	   0x20	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_TRAIN  0x800	/* Link training. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_SCLK_CFG   0x1000	/* Slot Clock Config. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_DLL_ACTIVE 0x2000	/* Data LL Active. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_BWM_STA	   0x4000	/* Bw mgmt interrupt enable */
#define	VXGE_HAL_PCI_EXP_LNKSTA_ABW_STA	   0x8000	/* Autonomous BW intr en */
	u16	pci_e_lnkctl;
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM		0x3	/* ASPM Control. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_DISABLED	0x0	/* Disabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L0_EN	0x1	/* L0 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L1_EN	0x2	/* L1 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L0_L1_EN	0x3	/* L0 & L1 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB		0x8	/* Read Compl Boundary. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB_64		0x0	/* RCB 64 bytes. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB_128		0x1	/* RCB 128 bytes. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_DISABLED	0x10	/* Disables the link. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RETRAIN		0x20	/* Retrain the link. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_CCCFG		0x40	/* Common clock config. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_EXT_SYNC	0x80	/* Extended Sync. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_CLK_PWRMGMT	0x100	/* Enable clk pwr mgmt. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_HW_AUTO_DIS	0x200	/* Hw autonomous w/dis */
#define	VXGE_HAL_PCI_EXP_LNKCTL_BWM_INTR_EN	0x400	/* Bw mgmt intr enable */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ABW_INTR_EN	0x800	/* Autonomous BW int en */
	u32	pci_e_stlcap;
#define	VXGE_HAL_PCI_EXP_STLCAP_ATTN_BTTN   0x1	/* Attention Button Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_PWR_CTRL    0x2	/* Power Control Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_MRL_SENS    0x4	/* MRL Sesor Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_ATTN_IND    0x8	/* Attention Ind Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_PWR_IND	    0x10	/* Power Indicator Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_HP_SURP	    0x20	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_HP_CAP	    0x40	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_VAL  0x7F80	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_250  0xF0	/* 250 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_275  0xF1	/* 275 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_300  0xF2	/* 300 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_LIM  0x18000	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_1X   0x0	/* 1.0x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY10 0x1	/* 0.1x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY100 0x2	/* 0.01x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY1000 0x3	/* 0.001x */
#define	VXGE_HAL_PCI_EXP_STLCAP_EM_INTR_LOCK 0x20000	/* Ele-mec Intrlock Pres */
#define	VXGE_HAL_PCI_EXP_STLCAP_NO_CMD_CMPL 0x40000	/* No Command Compl Supp */
#define	VXGE_HAL_PCI_EXP_STLCAP_PHY_SL_NO   0xFFF80000	/* Phy Slot Number */
	u16	pci_e_stlsta;
#define	VXGE_HAL_PCI_EXP_STLSTA_ATTN_BTN    0x1	/* Attention Button Pressed */
#define	VXGE_HAL_PCI_EXP_STLSTA_PF_DET	    0x2	/* Power Fault Detected */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_CH 0x4	/* MRL Sensor Changed */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_CH	    0x8	/* Presence Detect Changed */
#define	VXGE_HAL_PCI_EXP_STLSTA_CMD_COMPL   0x10	/* Command Completed */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_STA 0x20	/* MRL Sensor State */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_CL 0x0	/* MRL Sensor State - closed */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_OP 0x1	/* MRL Sensor State - open */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_STA    0x400	/* Presence Detect State */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_EMPTY  0x0	/* Clost Empty */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_PRESENT 0x1	/* Card Present */
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_STA   0x80	/* Ele-mec Intrlock Ctrl */
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_DIS   0x0	/* Disengaged */
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_EN    0x1	/* Engaged */
#define	VXGE_HAL_PCI_EXP_STLSTA_DLL_ST_CH   0x100	/* DL State Changed */
	u16	pci_e_stlctl;
#define	VXGE_HAL_PCI_EXP_STLCTL_ATTN_BTN_EN 0x1	/* Atten Btn pressed en */
#define	VXGE_HAL_PCI_EXP_STLCTL_PF_DET_EN   0x2	/* Pwr Fault Detected En */
#define	VXGE_HAL_PCI_EXP_STLCTL_MRL_SENS_EN 0x4	/* MRL Sensor Changed En */
#define	VXGE_HAL_PCI_EXP_STLCTL_PDET_CH_EN  0x8	/* Presence Detect Changed En */
#define	VXGE_HAL_PCI_EXP_STLCTL_CC_INTR_EN  0x10	/* Cmmd Completed Intr En */
#define	VXGE_HAL_PCI_EXP_STLCTL_HP_INTR_EN  0x20	/* Hot-Plug Intr Enable */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_CTRL 0xC0	/* Attention Ind Ctrl */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_RES 0x0	/* Reserved */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_ON  0x1	/* On */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_BLNK 0x2	/* Blink */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_OFF 0x3	/* Off */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_CTRL 0x300	/* POwer Ind Control */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_RES 0x0	/* Reserved */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_ON  0x1	/* On */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_BLNK 0x2	/* Blink */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_OFF 0x3	/* Off */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_CTRL 0x400	/* Power Controller Ctrl */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_on  0x0	/* Power on */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_off 0x1	/* Power off */
#define	VXGE_HAL_PCI_EXP_STLCTL_EM_IL_CTRL  0x800	/* Ele-mec Intrlock Ctrl */
#define	VXGE_HAL_PCI_EXP_STLCTL_DLL_ST_CH_EN 0x1000	/* DL State Changed En */
	u16	pci_e_rtcap;
#define	VXGE_HAL_PCI_EXP_RTCAP_CRS_SW_VIS 0x01	/* CRS Software Visibility */
	u16	pci_e_rtctl;
#define	VXGE_HAL_PCI_EXP_RTCTL_SECEE	0x01	/* Sys Err on Correctable Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_SENFEE	0x02	/* Sys Err on Non-Fatal Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_SEFEE	0x04	/* Sys Err on Fatal Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_PMEIE	0x08	/* PME Intr Enable */
#define	VXGE_HAL_PCI_EXP_RTCTL_CRSSVE	0x10	/* CRS SW Visibility Enable */
	u32	pci_e_rtsta;
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_REQ_ID   0xFFFF	/* PME Requestor ID */
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_STATUS   0x10000	/* PME status */
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_PENDING  0x20000	/* PME Pending */
#else
	u8	capability_id;
	u8	next_capability_ptr;
	u16	pci_e_flags;
#define	VXGE_HAL_PCI_EXP_FLAGS_VERS	0x000f	/* Capability version */
#define	VXGE_HAL_PCI_EXP_FLAGS_TYPE	0x00f0	/* Device/Port type */
#define	VXGE_HAL_PCI_EXP_TYPE_ENDPOINT	0x0	/* Express Endpoint */
#define	VXGE_HAL_PCI_EXP_TYPE_LEG_END	0x1	/* Legacy Endpoint */
#define	VXGE_HAL_PCI_EXP_TYPE_ROOT_PORT	0x4	/* Root Port */
#define	VXGE_HAL_PCI_EXP_TYPE_UPSTREAM	0x5	/* Upstream Port */
#define	VXGE_HAL_PCI_EXP_TYPE_DOWNSTREAM 0x6	/* Downstream Port */
#define	VXGE_HAL_PCI_EXP_TYPE_PCI_BRIDGE 0x7	/* PCI/PCI-X Bridge */
#define	VXGE_HAL_PCI_EXP_FLAGS_SLOT	0x0100	/* Slot implemented */
#define	VXGE_HAL_PCI_EXP_FLAGS_IRQ	0x3e00	/* Interrupt message number */
	u32	pci_e_devcap;
#define	VXGE_HAL_PCI_EXP_DEVCAP_PAYLOAD 0x07	/* Max_Payload_Size */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PHANTOM 0x18	/* Phantom functions */
#define	VXGE_HAL_PCI_EXP_DEVCAP_EXT_TAG 0x20	/* Extended tags */
#define	VXGE_HAL_PCI_EXP_DEVCAP_L0S	0x1c0	/* L0s Acceptable Latency */
#define	VXGE_HAL_PCI_EXP_DEVCAP_L1	0xe00	/* L1 Acceptable Latency */
#define	VXGE_HAL_PCI_EXP_DEVCAP_ATN_BUT 0x1000	/* Attention Button Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_ATN_IND 0x2000	/* Attention Ind Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_IND 0x4000	/* Power Indicator Present */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_VAL 0x3fc0000	/* Slot Power Limit Value */
#define	VXGE_HAL_PCI_EXP_DEVCAP_PWR_SCL 0xc000000	/* Slot Power Limit Scale */
	u16	pci_e_devctl;
#define	VXGE_HAL_PCI_EXP_DEVCTL_CERE	0x0001	/* Corr'ble Err Reporting En. */
#define	VXGE_HAL_PCI_EXP_DEVCTL_NFERE	0x0002	/* Non-Fatal Err Reporting En */
#define	VXGE_HAL_PCI_EXP_DEVCTL_FERE	0x0004	/* Fatal Err Reporting En */
#define	VXGE_HAL_PCI_EXP_DEVCTL_URRE	0x0008	/* Unsupp Req Reporting En. */
#define	VXGE_HAL_PCI_EXP_DEVCTL_RELAX_EN 0x0010	/* Enable relaxed ordering */
#define	VXGE_HAL_PCI_EXP_DEVCTL_PAYLOAD 0x00e0	/* Max_Payload_Size */
#define	VXGE_HAL_PCI_EXP_DEVCTL_EXT_TAG 0x0100	/* Extended Tag Field Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_PHANTOM 0x0200	/* Phantom Functions Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_AUX_PME 0x0400	/* Auxiliary Power PM Enable */
#define	VXGE_HAL_PCI_EXP_DEVCTL_NOSNOOP_EN 0x0800	/* Enable No Snoop */
#define	VXGE_HAL_PCI_EXP_DEVCTL_READRQ  0x7000	/* Max_Read_Request_Size */
	u16	pci_e_devsta;
#define	VXGE_HAL_PCI_EXP_DEVSTA_CED	 0x01	/* Correctable Error Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_NFED	0x02	/* Non-Fatal Error Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_FED	 0x04	/* Fatal Error Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_URD	 0x08	/* Unsupp Request Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_AUXPD   0x10	/* AUX Power Detected */
#define	VXGE_HAL_PCI_EXP_DEVSTA_TRPND   0x20	/* Transactions Pending */
	u32	pci_e_lnkcap;
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_SPEED 0xf	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LS_2_5	0x1	/* 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LS_5	0x2	/* 5 and 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_WIDTH 0x3f0	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_RES  0x0	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X1   0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X2   0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X4   0x4	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X8   0x8	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X12  0xa	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X16  0x10	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LW_X32  0x20	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_ASPM  0xc00	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_RES1  0x0	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_LO    0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_RES2  0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LASPM_L0_L1 0x3	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_LAT	    0x7000	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_LT_64    0x0	/* Less than 64 ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_64_128   0x1	/* 64ns to less than 128ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_128_256  0x2	/* 128ns to less than 256ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_256_512  0x3	/* 256ns to less than 512ns. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_512_1us  0x4	/* 512ns to less than 1us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_1us_2us  0x5	/* 1us to less than 2us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_2us_4us  0x6	/* 2us-4us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L0_GT_4us   0x7	/* More than 4us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_LAT	    0x38000	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_LT_1us   0x0	/* Less than 1us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_1us_2us  0x1	/* 1us to less than 2us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_2us_4us  0x2	/* 2us to less than 4us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_4us_8us  0x3	/* 4us to less than 8us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_8us_16us 0x4	/* 8us to less than 16us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_16us_32us 0x5	/* 16us to less than 32us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_32us_64us 0x6	/* 32us-64us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_L1_GT_64us   0x7	/* More than 64us. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_CLK_PWR_MGMT 0x40000	/* Clock power mgmt */
#define	VXGE_HAL_PCI_EXP_LNKCAP_DOWN_ERR_CAP 0x80000	/* Down error capable. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_ACT_CAP  0x100000	/* DL active rep cap. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_BW_CAP   0x200000	/* DL bw rep cap. */
#define	VXGE_HAL_PCI_EXP_LNKCAP_LNK_PORT_NUM 0xff000000	/* Port number. */
	u16	pci_e_lnkctl;
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM	    0x3	/* ASPM Control. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_DISABLED 0x0	/* Disabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L0_EN  0x1	/* L0 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L1_EN  0x2	/* L1 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ASPM_L0_L1_EN 0x3	/* L0 & L1 entry enabled. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB	    0x8	/* Read Completion Boundary. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB_64	    0x0	/* RCB 64 bytes. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RCB_128	    0x1	/* RCB 128 bytes. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_DISABLED    0x10	/* Disables the link. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_RETRAIN	    0x20	/* Retrain the link. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_CCCFG	    0x40	/* Common clock config. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_EXT_SYNC    0x80	/* Extended Sync. */
#define	VXGE_HAL_PCI_EXP_LNKCTL_CLK_PWRMGMT 0x100	/* Enable clock power mgmt */
#define	VXGE_HAL_PCI_EXP_LNKCTL_HW_AUTO_DIS 0x200	/* HW autonomous with dis */
#define	VXGE_HAL_PCI_EXP_LNKCTL_BWM_INTR_EN 0x400	/* Bw mgmt interrupt enable */
#define	VXGE_HAL_PCI_EXP_LNKCTL_ABW_INTR_EN 0x800	/* Autonomous BW int enable */
	u16	pci_e_lnksta;
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_SPEED   0xf	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LS_2_5	    0x1	/* 2.5 Gb/s supported. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LS_5	    0x2	/* 5 and 2.5 Gb/s supported */
	/* Supported Link speeds. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_WIDTH   0x3f0
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_RES	    0x0	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X1	    0x1	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X2	    0x2	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X4	    0x4	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X8	    0x8	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X12	    0xa	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X16	    0x10	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LW_X32	    0x20	/* Reserved. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_LNK_TRAIN   0x800	/* Link training. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_SCLK_CFG    0x1000	/* Slot Clock Config. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_DLL_ACTIVE  0x2000	/* Data LL Active. */
#define	VXGE_HAL_PCI_EXP_LNKSTA_BWM_STA	    0x4000	/* Bw mgmt intr enable */
#define	VXGE_HAL_PCI_EXP_LNKSTA_ABW_STA	    0x8000	/* Autonomous BW intr en */
	u32	pci_e_stlcap;
#define	VXGE_HAL_PCI_EXP_STLCAP_ATTN_BTTN   0x1	/* Attention Button Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_PWR_CTRL    0x2	/* Power Control Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_MRL_SENS    0x4	/* MRL Sesor Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_ATTN_IND    0x8	/* Attention Ind Present */
	/* Power Indicator Present */
#define	VXGE_HAL_PCI_EXP_STLCAP_PWR_IND	    0x10
#define	VXGE_HAL_PCI_EXP_STLCAP_HP_SURP	    0x20	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_HP_CAP	    0x40	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_VAL  0x7F80	/* Hot-Plug Surprise */
	/* 250 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_250  0xF0
	/* 275 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_275  0xF1
	/* 300 W Slot Power Limit */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_300  0xF2
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_LIM  0x18000	/* Hot-Plug Surprise */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_1X   0x0	/* 1.0x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY10 0x1	/* 0.1x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY100 0x2	/* 0.01x */
#define	VXGE_HAL_PCI_EXP_STLCAP_SL_PWR_XBY1000 0x3	/* 0.001x */
#define	VXGE_HAL_PCI_EXP_STLCAP_EM_INTR_LOCK 0x20000	/* Ele-mec Intrlock Pres */
#define	VXGE_HAL_PCI_EXP_STLCAP_NO_CMD_CMPL 0x40000	/* No Cmd Completed Supp */
#define	VXGE_HAL_PCI_EXP_STLCAP_PHY_SL_NO   0xFFF80000	/* Phys Slot Number */
	u16	pci_e_stlctl;
#define	VXGE_HAL_PCI_EXP_STLCTL_ATTN_BTN_EN 0x1	/* Atten Bttn pressed en */
#define	VXGE_HAL_PCI_EXP_STLCTL_PF_DET_EN   0x2	/* Power Fault Detected En */
#define	VXGE_HAL_PCI_EXP_STLCTL_MRL_SENS_EN 0x4	/* MRL Sensor Changed Enable */
#define	VXGE_HAL_PCI_EXP_STLCTL_PDET_CH_EN  0x8	/* Presence Detect Changed En */
#define	VXGE_HAL_PCI_EXP_STLCTL_CC_INTR_EN  0x10	/* Cmd Compl Intr Enable */
#define	VXGE_HAL_PCI_EXP_STLCTL_HP_INTR_EN  0x20	/* Hot-Plug Intr Enable */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_CTRL 0xC0	/* Atten Ind Control */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_RES 0x0	/* Reserved */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_ON  0x1	/* On */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_BLNK 0x2	/* Blink */
#define	VXGE_HAL_PCI_EXP_STLCTL_ATN_IND_OFF 0x3	/* Off */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_CTRL 0x300	/* Power Ind Control */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_RES 0x0	/* Reserved */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_ON  0x1	/* On */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_BLNK 0x2	/* Blink */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWR_IND_OFF 0x3	/* Off */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_CTRL 0x400	/* Power Controller Ctrl */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_on  0x0	/* Power on */
#define	VXGE_HAL_PCI_EXP_STLCTL_PWRCTRL_off 0x1	/* Power off */
#define	VXGE_HAL_PCI_EXP_STLCTL_EM_IL_CTRL  0x800	/* Ele-mec Intrlock Ctrl */
#define	VXGE_HAL_PCI_EXP_STLCTL_DLL_ST_CH_EN 0x1000	/* DL State Changed En */
	u16	pci_e_stlsta;
#define	VXGE_HAL_PCI_EXP_STLSTA_ATTN_BTN    0x1	/* Attention Button Pressed */
#define	VXGE_HAL_PCI_EXP_STLSTA_PF_DET	    0x2	/* Power Fault Detected */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_CH 0x4	/* MRL Sensor Changed */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_CH	    0x8	/* Presence Detect Changed */
#define	VXGE_HAL_PCI_EXP_STLSTA_CMD_COMPL   0x10	/* Command Completed */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_STA 0x20	/* MRL Sensor State */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_CL 0x0	/* MRL Sensor State - closed */
#define	VXGE_HAL_PCI_EXP_STLSTA_MRL_SENS_OP 0x1	/* MRL Sensor State - open */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_STA    0x400	/* Presence Detect State */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_EMPTY  0x0	/* Clost Empty */
#define	VXGE_HAL_PCI_EXP_STLSTA_PDET_PRESENT 0x1	/* Card Present */
	/* Ele-mec Interlock Control */
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_STA   0x80
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_DIS   0x0	/* Disengaged */
#define	VXGE_HAL_PCI_EXP_STLSTA_EM_IL_EN    0x1	/* Engaged */
	/* DL Layer State Changed */
#define	VXGE_HAL_PCI_EXP_STLSTA_DLL_ST_CH   0x100
	u16	pci_e_rtctl;
#define	VXGE_HAL_PCI_EXP_RTCTL_SECEE	0x01	/* Sys Err on Correctable Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_SENFEE	0x02	/* Sys Err on Non-Fatal Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_SEFEE	0x04	/* Sys Err on Fatal Error */
#define	VXGE_HAL_PCI_EXP_RTCTL_PMEIE	0x08	/* PME Interrupt Enable */
#define	VXGE_HAL_PCI_EXP_RTCTL_CRSSVE	0x10	/* CRS SW Visibility Enable */
	u16	pci_e_rtcap;
#define	VXGE_HAL_PCI_EXP_RTCAP_CRS_SW_VIS   0x01	/* CRS SW Visibility */
	u32	pci_e_rtsta;
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_REQ_ID   0xFFFF	/* PME Requestor ID */
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_STATUS   0x10000	/* PME status */
#define	VXGE_HAL_PCI_EXP_RTSTA_PME_PENDING  0x20000	/* PME Pending */
#endif
} vxge_hal_pci_e_capability_t;

typedef u32 vxge_hal_pci_e_caps_offset_t;

#define	VXGE_HAL_PCI_EXT_CAP_ID(header)		(header & 0x0000ffff)
#define	VXGE_HAL_PCI_EXT_CAP_VER(header)	((header >> 16) & 0xf)
#define	VXGE_HAL_PCI_EXT_CAP_NEXT(header)	((header >> 20) & 0xffc)

#define	VXGE_HAL_PCI_EXT_CAP_ID_ERR	    1
#define	VXGE_HAL_PCI_EXT_CAP_ID_VC	    2
#define	VXGE_HAL_PCI_EXT_CAP_ID_DSN	    3
#define	VXGE_HAL_PCI_EXT_CAP_ID_PWR	    4

typedef struct vxge_hal_err_capability_t {
	u32	pci_err_header;
	u32	pci_err_uncor_status;
#define	VXGE_HAL_PCI_ERR_UNC_TRAIN	0x00000001	/* Training */
#define	VXGE_HAL_PCI_ERR_UNC_DLP	0x00000010	/* Data Link Protocol */
#define	VXGE_HAL_PCI_ERR_UNC_POISON_TLP 0x00001000	/* Poisoned TLP */
#define	VXGE_HAL_PCI_ERR_UNC_FCP	0x00002000	/* Flow Control Protocol */
#define	VXGE_HAL_PCI_ERR_UNC_COMP_TIME  0x00004000	/* Completion Timeout */
#define	VXGE_HAL_PCI_ERR_UNC_COMP_ABORT 0x00008000	/* Completer Abort */
#define	VXGE_HAL_PCI_ERR_UNC_UNX_COMP   0x00010000	/* Unexpected Completion */
#define	VXGE_HAL_PCI_ERR_UNC_RX_OVER	0x00020000	/* Receiver Overflow */
#define	VXGE_HAL_PCI_ERR_UNC_MALF_TLP   0x00040000	/* Malformed TLP */
#define	VXGE_HAL_PCI_ERR_UNC_ECRC	0x00080000	/* ECRC Error Status */
#define	VXGE_HAL_PCI_ERR_UNC_UNSUP	0x00100000	/* Unsupported Request */
	u32	pci_err_uncor_mask;
	u32	pci_err_uncor_server;
	u32	pci_err_cor_status;
#define	VXGE_HAL_PCI_ERR_COR_RCVR	0x00000001	/* Receiver Error Status */
#define	VXGE_HAL_PCI_ERR_COR_BAD_TLP	0x00000040	/* Bad TLP Status */
#define	VXGE_HAL_PCI_ERR_COR_BAD_DLLP   0x00000080	/* Bad DLLP Status */
#define	VXGE_HAL_PCI_ERR_COR_REP_ROLL   0x00000100	/* REPLAY_NUM Rollover */
#define	VXGE_HAL_PCI_ERR_COR_REP_TIMER  0x00001000	/* Replay Timer Timeout */
#define	VXGE_HAL_PCI_ERR_COR_MASK	20	/* Correctable Error Mask */
	u32	pci_err_cap;
#define	VXGE_HAL_PCI_ERR_CAP_FEP(x)	((x) & 31)	/* First Error Pointer */
	/* ECRC Generation Capable */
#define	VXGE_HAL_PCI_ERR_CAP_ECRC_GENC  0x00000020

#define	VXGE_HAL_PCI_ERR_CAP_ECRC_GENE  0x00000040	/* ECRC Generation Enable */
#define	VXGE_HAL_PCI_ERR_CAP_ECRC_CHKC  0x00000080	/* ECRC Check Capable */
#define	VXGE_HAL_PCI_ERR_CAP_ECRC_CHKE  0x00000100	/* ECRC Check Enable */
	u32	err_header_log;
#define	VXGE_HAL_PCI_ERR_HEADER_LOG(x)  ((x) >> 31)	/* Error Header Log */
	u32	unused2[3];
	u32	pci_err_root_command;
	u32	pci_err_root_status;
	u32	pci_err_root_cor_src;
	u32	pci_err_root_src;
} vxge_hal_err_capability_t;

typedef struct vxge_hal_vc_capability_t {
	u32	pci_vc_header;
	u32	pci_vc_port_reg1;
	u32	pci_vc_port_reg2;
	u32	pci_vc_port_ctrl;
	u32	pci_vc_port_status;
	u32	pci_vc_res_cap;
	u32	pci_vc_res_ctrl;
	u32	pci_vc_res_status;
} vxge_hal_vc_capability_t;

typedef struct vxge_hal_pwr_budget_capability_t {
	u32	pci_pwr_header;
	u32	pci_pwr_dsr;
	u32	pci_pwr_data;
#define	VXGE_HAL_PCI_PWR_DATA_BASE(x)   ((x) & 0xff)	/* Base Power */
#define	VXGE_HAL_PCI_PWR_DATA_SCALE(x)  (((x) >> 8) & 3)	/* Data Scale */
#define	VXGE_HAL_PCI_PWR_DATA_PM_SUB(x) (((x) >> 10) & 7)	/* PM Sub State */
#define	VXGE_HAL_PCI_PWR_DATA_PM_STATE(x) (((x) >> 13) & 3)	/* PM State */
#define	VXGE_HAL_PCI_PWR_DATA_TYPE(x)   (((x) >> 15) & 7)	/* Type */
#define	VXGE_HAL_PCI_PWR_DATA_RAIL(x)   (((x) >> 18) & 7)	/* Power Rail */
	u32	pci_pwr_cap;
#define	VXGE_HAL_PCI_PWR_CAP_BUDGET(x)  ((x) & 1)	/* Include in sys budget */
} vxge_hal_pwr_budget_capability_t;

typedef struct vxge_hal_pci_e_ext_caps_offset_t {
	u32	err_cap_offset;
	u32	vc_cap_offset;
	u32	dsn_cap_offset;
	u32	pwr_budget_cap_offset;
} vxge_hal_pci_e_ext_caps_offset_t;

#pragma pack()

__EXTERN_END_DECLS

#endif	/* VXGE_HAL_REGS_H */
