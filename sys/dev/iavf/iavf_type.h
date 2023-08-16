/* SPDX-License-Identifier: BSD-3-Clause */
/*  Copyright (c) 2021, Intel Corporation
 *  All rights reserved.
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *      this list of conditions and the following disclaimer.
 *
 *   2. Redistributions in binary form must reproduce the above copyright
 *      notice, this list of conditions and the following disclaimer in the
 *      documentation and/or other materials provided with the distribution.
 *
 *   3. Neither the name of the Intel Corporation nor the names of its
 *      contributors may be used to endorse or promote products derived from
 *      this software without specific prior written permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 *  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 *  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 *  ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 *  LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 *  CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 *  SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 *  INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 *  CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 *  ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 *  POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _IAVF_TYPE_H_
#define _IAVF_TYPE_H_

#include "iavf_status.h"
#include "iavf_osdep.h"
#include "iavf_register.h"
#include "iavf_adminq.h"
#include "iavf_devids.h"

#define IAVF_RXQ_CTX_DBUFF_SHIFT	7

#define BIT(a) (1UL << (a))
#define BIT_ULL(a) (1ULL << (a))

#ifndef IAVF_MASK
/* IAVF_MASK is a macro used on 32 bit registers */
#define IAVF_MASK(mask, shift) (mask << shift)
#endif

#define IAVF_MAX_PF			16
#define IAVF_MAX_PF_VSI			64
#define IAVF_MAX_PF_QP			128
#define IAVF_MAX_VSI_QP			16
#define IAVF_MAX_VF_VSI			4
#define IAVF_MAX_CHAINED_RX_BUFFERS	5

/* something less than 1 minute */
#define IAVF_HEARTBEAT_TIMEOUT		(HZ * 50)

/* Check whether address is multicast. */
#define IAVF_IS_MULTICAST(address) (bool)(((u8 *)(address))[0] & ((u8)0x01))

/* Check whether an address is broadcast. */
#define IAVF_IS_BROADCAST(address)	\
	((((u8 *)(address))[0] == ((u8)0xff)) && \
	(((u8 *)(address))[1] == ((u8)0xff)))

/* forward declaration */
struct iavf_hw;
typedef void (*IAVF_ADMINQ_CALLBACK)(struct iavf_hw *, struct iavf_aq_desc *);

#define ETH_ALEN	6
/* Data type manipulation macros. */
#define IAVF_HI_DWORD(x)	((u32)((((x) >> 16) >> 16) & 0xFFFFFFFF))
#define IAVF_LO_DWORD(x)	((u32)((x) & 0xFFFFFFFF))

#define IAVF_HI_WORD(x)		((u16)(((x) >> 16) & 0xFFFF))
#define IAVF_LO_WORD(x)		((u16)((x) & 0xFFFF))

#define IAVF_HI_BYTE(x)		((u8)(((x) >> 8) & 0xFF))
#define IAVF_LO_BYTE(x)		((u8)((x) & 0xFF))

/* Number of Transmit Descriptors must be a multiple of 8. */
#define IAVF_REQ_TX_DESCRIPTOR_MULTIPLE	8
/* Number of Receive Descriptors must be a multiple of 32 if
 * the number of descriptors is greater than 32.
 */
#define IAVF_REQ_RX_DESCRIPTOR_MULTIPLE	32

#define IAVF_DESC_UNUSED(R)	\
	((((R)->next_to_clean > (R)->next_to_use) ? 0 : (R)->count) + \
	(R)->next_to_clean - (R)->next_to_use - 1)

/* bitfields for Tx queue mapping in QTX_CTL */
#define IAVF_QTX_CTL_VF_QUEUE	0x0
#define IAVF_QTX_CTL_VM_QUEUE	0x1
#define IAVF_QTX_CTL_PF_QUEUE	0x2

/* debug masks - set these bits in hw->debug_mask to control output */
enum iavf_debug_mask {
	IAVF_DEBUG_INIT			= 0x00000001,
	IAVF_DEBUG_RELEASE		= 0x00000002,

	IAVF_DEBUG_LINK			= 0x00000010,
	IAVF_DEBUG_PHY			= 0x00000020,
	IAVF_DEBUG_HMC			= 0x00000040,
	IAVF_DEBUG_NVM			= 0x00000080,
	IAVF_DEBUG_LAN			= 0x00000100,
	IAVF_DEBUG_FLOW			= 0x00000200,
	IAVF_DEBUG_DCB			= 0x00000400,
	IAVF_DEBUG_DIAG			= 0x00000800,
	IAVF_DEBUG_FD			= 0x00001000,
	IAVF_DEBUG_PACKAGE		= 0x00002000,

	IAVF_DEBUG_IWARP		= 0x00F00000,

	IAVF_DEBUG_AQ_MESSAGE		= 0x01000000,
	IAVF_DEBUG_AQ_DESCRIPTOR	= 0x02000000,
	IAVF_DEBUG_AQ_DESC_BUFFER	= 0x04000000,
	IAVF_DEBUG_AQ_COMMAND		= 0x06000000,
	IAVF_DEBUG_AQ			= 0x0F000000,

	IAVF_DEBUG_USER			= 0xF0000000,

	IAVF_DEBUG_ALL			= 0xFFFFFFFF
};

/* PCI Bus Info */
#define IAVF_PCI_LINK_STATUS		0xB2
#define IAVF_PCI_LINK_WIDTH		0x3F0
#define IAVF_PCI_LINK_WIDTH_1		0x10
#define IAVF_PCI_LINK_WIDTH_2		0x20
#define IAVF_PCI_LINK_WIDTH_4		0x40
#define IAVF_PCI_LINK_WIDTH_8		0x80
#define IAVF_PCI_LINK_SPEED		0xF
#define IAVF_PCI_LINK_SPEED_2500	0x1
#define IAVF_PCI_LINK_SPEED_5000	0x2
#define IAVF_PCI_LINK_SPEED_8000	0x3

#define IAVF_MDIO_CLAUSE22_STCODE_MASK	IAVF_MASK(1, \
						  IAVF_GLGEN_MSCA_STCODE_SHIFT)
#define IAVF_MDIO_CLAUSE22_OPCODE_WRITE_MASK	IAVF_MASK(1, \
						  IAVF_GLGEN_MSCA_OPCODE_SHIFT)
#define IAVF_MDIO_CLAUSE22_OPCODE_READ_MASK	IAVF_MASK(2, \
						  IAVF_GLGEN_MSCA_OPCODE_SHIFT)

#define IAVF_MDIO_CLAUSE45_STCODE_MASK	IAVF_MASK(0, \
						  IAVF_GLGEN_MSCA_STCODE_SHIFT)
#define IAVF_MDIO_CLAUSE45_OPCODE_ADDRESS_MASK	IAVF_MASK(0, \
						  IAVF_GLGEN_MSCA_OPCODE_SHIFT)
#define IAVF_MDIO_CLAUSE45_OPCODE_WRITE_MASK	IAVF_MASK(1, \
						  IAVF_GLGEN_MSCA_OPCODE_SHIFT)
#define IAVF_MDIO_CLAUSE45_OPCODE_READ_INC_ADDR_MASK	IAVF_MASK(2, \
						  IAVF_GLGEN_MSCA_OPCODE_SHIFT)
#define IAVF_MDIO_CLAUSE45_OPCODE_READ_MASK	IAVF_MASK(3, \
						  IAVF_GLGEN_MSCA_OPCODE_SHIFT)

#define IAVF_PHY_COM_REG_PAGE			0x1E
#define IAVF_PHY_LED_LINK_MODE_MASK		0xF0
#define IAVF_PHY_LED_MANUAL_ON			0x100
#define IAVF_PHY_LED_PROV_REG_1			0xC430
#define IAVF_PHY_LED_MODE_MASK			0xFFFF
#define IAVF_PHY_LED_MODE_ORIG			0x80000000

/* Memory types */
enum iavf_memset_type {
	IAVF_NONDMA_MEM = 0,
	IAVF_DMA_MEM
};

/* Memcpy types */
enum iavf_memcpy_type {
	IAVF_NONDMA_TO_NONDMA = 0,
	IAVF_NONDMA_TO_DMA,
	IAVF_DMA_TO_DMA,
	IAVF_DMA_TO_NONDMA
};

/* These are structs for managing the hardware information and the operations.
 * The structures of function pointers are filled out at init time when we
 * know for sure exactly which hardware we're working with.  This gives us the
 * flexibility of using the same main driver code but adapting to slightly
 * different hardware needs as new parts are developed.  For this architecture,
 * the Firmware and AdminQ are intended to insulate the driver from most of the
 * future changes, but these structures will also do part of the job.
 */
enum iavf_mac_type {
	IAVF_MAC_UNKNOWN = 0,
	IAVF_MAC_XL710,
	IAVF_MAC_VF,
	IAVF_MAC_X722,
	IAVF_MAC_X722_VF,
	IAVF_MAC_GENERIC,
};

enum iavf_vsi_type {
	IAVF_VSI_MAIN	= 0,
	IAVF_VSI_VMDQ1	= 1,
	IAVF_VSI_VMDQ2	= 2,
	IAVF_VSI_CTRL	= 3,
	IAVF_VSI_FCOE	= 4,
	IAVF_VSI_MIRROR	= 5,
	IAVF_VSI_SRIOV	= 6,
	IAVF_VSI_FDIR	= 7,
	IAVF_VSI_IWARP	= 8,
	IAVF_VSI_TYPE_UNKNOWN
};

enum iavf_queue_type {
	IAVF_QUEUE_TYPE_RX = 0,
	IAVF_QUEUE_TYPE_TX,
	IAVF_QUEUE_TYPE_PE_CEQ,
	IAVF_QUEUE_TYPE_UNKNOWN
};

#define IAVF_HW_CAP_MAX_GPIO			30
#define IAVF_HW_CAP_MDIO_PORT_MODE_MDIO		0
#define IAVF_HW_CAP_MDIO_PORT_MODE_I2C		1

enum iavf_acpi_programming_method {
	IAVF_ACPI_PROGRAMMING_METHOD_HW_FVL = 0,
	IAVF_ACPI_PROGRAMMING_METHOD_AQC_FPK = 1
};

#define IAVF_WOL_SUPPORT_MASK			0x1
#define IAVF_ACPI_PROGRAMMING_METHOD_MASK	0x2
#define IAVF_PROXY_SUPPORT_MASK			0x4

/* Capabilities of a PF or a VF or the whole device */
struct iavf_hw_capabilities {
	/* Cloud filter modes:
	 * Mode1: Filter on L4 port only
	 * Mode2: Filter for non-tunneled traffic
	 * Mode3: Filter for tunnel traffic
	 */
#define IAVF_CLOUD_FILTER_MODE1	0x6
#define IAVF_CLOUD_FILTER_MODE2	0x7
#define IAVF_CLOUD_FILTER_MODE3	0x8
#define IAVF_SWITCH_MODE_MASK	0xF

	bool dcb;
	bool fcoe;
	bool iwarp;
	u32 num_vsis;
	u32 num_rx_qp;
	u32 num_tx_qp;
	u32 base_queue;
	u32 num_msix_vectors_vf;
	bool apm_wol_support;
	enum iavf_acpi_programming_method acpi_prog_method;
	bool proxy_support;
};

struct iavf_mac_info {
	enum iavf_mac_type type;
	u8 addr[ETH_ALEN];
	u8 perm_addr[ETH_ALEN];
	u8 san_addr[ETH_ALEN];
	u8 port_addr[ETH_ALEN];
	u16 max_fcoeq;
};

#define IAVF_NVM_EXEC_GET_AQ_RESULT		0x0
#define IAVF_NVM_EXEC_FEATURES			0xe
#define IAVF_NVM_EXEC_STATUS			0xf

/* NVMUpdate features API */
#define IAVF_NVMUPD_FEATURES_API_VER_MAJOR		0
#define IAVF_NVMUPD_FEATURES_API_VER_MINOR		14
#define IAVF_NVMUPD_FEATURES_API_FEATURES_ARRAY_LEN	12

#define IAVF_NVMUPD_FEATURE_FLAT_NVM_SUPPORT		BIT(0)

struct iavf_nvmupd_features {
	u8 major;
	u8 minor;
	u16 size;
	u8 features[IAVF_NVMUPD_FEATURES_API_FEATURES_ARRAY_LEN];
};

#define IAVF_MODULE_SFF_DIAG_CAPAB	0x40
/* PCI bus types */
enum iavf_bus_type {
	iavf_bus_type_unknown = 0,
	iavf_bus_type_pci,
	iavf_bus_type_pcix,
	iavf_bus_type_pci_express,
	iavf_bus_type_reserved
};

/* PCI bus speeds */
enum iavf_bus_speed {
	iavf_bus_speed_unknown	= 0,
	iavf_bus_speed_33	= 33,
	iavf_bus_speed_66	= 66,
	iavf_bus_speed_100	= 100,
	iavf_bus_speed_120	= 120,
	iavf_bus_speed_133	= 133,
	iavf_bus_speed_2500	= 2500,
	iavf_bus_speed_5000	= 5000,
	iavf_bus_speed_8000	= 8000,
	iavf_bus_speed_reserved
};

/* PCI bus widths */
enum iavf_bus_width {
	iavf_bus_width_unknown	= 0,
	iavf_bus_width_pcie_x1	= 1,
	iavf_bus_width_pcie_x2	= 2,
	iavf_bus_width_pcie_x4	= 4,
	iavf_bus_width_pcie_x8	= 8,
	iavf_bus_width_32	= 32,
	iavf_bus_width_64	= 64,
	iavf_bus_width_reserved
};

/* Bus parameters */
struct iavf_bus_info {
	enum iavf_bus_speed speed;
	enum iavf_bus_width width;
	enum iavf_bus_type type;

	u16 func;
	u16 device;
	u16 lan_id;
	u16 bus_id;
};

#define IAVF_MAX_USER_PRIORITY		8
#define IAVF_TLV_STATUS_OPER		0x1
#define IAVF_TLV_STATUS_SYNC		0x2
#define IAVF_TLV_STATUS_ERR		0x4
#define IAVF_CEE_OPER_MAX_APPS		3
#define IAVF_APP_PROTOID_FCOE		0x8906
#define IAVF_APP_PROTOID_ISCSI		0x0cbc
#define IAVF_APP_PROTOID_FIP		0x8914
#define IAVF_APP_SEL_ETHTYPE		0x1
#define IAVF_APP_SEL_TCPIP		0x2
#define IAVF_CEE_APP_SEL_ETHTYPE	0x0
#define IAVF_CEE_APP_SEL_TCPIP		0x1

/* Port hardware description */
struct iavf_hw {
	u8 *hw_addr;
	void *back;

	/* subsystem structs */
	struct iavf_mac_info mac;
	struct iavf_bus_info bus;

	/* pci info */
	u16 device_id;
	u16 vendor_id;
	u16 subsystem_device_id;
	u16 subsystem_vendor_id;
	u8 revision_id;

	/* capabilities for entire device and PCI func */
	struct iavf_hw_capabilities dev_caps;

	/* Admin Queue info */
	struct iavf_adminq_info aq;

	/* WoL and proxy support */
	u16 num_wol_proxy_filters;
	u16 wol_proxy_vsi_seid;

#define IAVF_HW_FLAG_AQ_SRCTL_ACCESS_ENABLE BIT_ULL(0)
#define IAVF_HW_FLAG_802_1AD_CAPABLE        BIT_ULL(1)
#define IAVF_HW_FLAG_AQ_PHY_ACCESS_CAPABLE  BIT_ULL(2)
#define IAVF_HW_FLAG_NVM_READ_REQUIRES_LOCK BIT_ULL(3)
#define IAVF_HW_FLAG_FW_LLDP_STOPPABLE	    BIT_ULL(4)
	u64 flags;

	/* NVMUpdate features */
	struct iavf_nvmupd_features nvmupd_features;

	/* debug mask */
	u32 debug_mask;
	char err_str[16];
};

struct iavf_driver_version {
	u8 major_version;
	u8 minor_version;
	u8 build_version;
	u8 subbuild_version;
	u8 driver_string[32];
};

/* RX Descriptors */
union iavf_16byte_rx_desc {
	struct {
		__le64 pkt_addr; /* Packet buffer address */
		__le64 hdr_addr; /* Header buffer address */
	} read;
	struct {
		struct {
			struct {
				union {
					__le16 mirroring_status;
					__le16 fcoe_ctx_id;
				} mirr_fcoe;
				__le16 l2tag1;
			} lo_dword;
			union {
				__le32 rss; /* RSS Hash */
				__le32 fd_id; /* Flow director filter id */
				__le32 fcoe_param; /* FCoE DDP Context id */
			} hi_dword;
		} qword0;
		struct {
			/* ext status/error/pktype/length */
			__le64 status_error_len;
		} qword1;
	} wb;  /* writeback */
};

union iavf_32byte_rx_desc {
	struct {
		__le64  pkt_addr; /* Packet buffer address */
		__le64  hdr_addr; /* Header buffer address */
			/* bit 0 of hdr_buffer_addr is DD bit */
		__le64  rsvd1;
		__le64  rsvd2;
	} read;
	struct {
		struct {
			struct {
				union {
					__le16 mirroring_status;
					__le16 fcoe_ctx_id;
				} mirr_fcoe;
				__le16 l2tag1;
			} lo_dword;
			union {
				__le32 rss; /* RSS Hash */
				__le32 fcoe_param; /* FCoE DDP Context id */
				/* Flow director filter id in case of
				 * Programming status desc WB
				 */
				__le32 fd_id;
			} hi_dword;
		} qword0;
		struct {
			/* status/error/pktype/length */
			__le64 status_error_len;
		} qword1;
		struct {
			__le16 ext_status; /* extended status */
			__le16 rsvd;
			__le16 l2tag2_1;
			__le16 l2tag2_2;
		} qword2;
		struct {
			union {
				__le32 flex_bytes_lo;
				__le32 pe_status;
			} lo_dword;
			union {
				__le32 flex_bytes_hi;
				__le32 fd_id;
			} hi_dword;
		} qword3;
	} wb;  /* writeback */
};

#define IAVF_RXD_QW0_MIRROR_STATUS_SHIFT	8
#define IAVF_RXD_QW0_MIRROR_STATUS_MASK	(0x3FUL << \
					 IAVF_RXD_QW0_MIRROR_STATUS_SHIFT)
#define IAVF_RXD_QW0_FCOEINDX_SHIFT	0
#define IAVF_RXD_QW0_FCOEINDX_MASK	(0xFFFUL << \
					 IAVF_RXD_QW0_FCOEINDX_SHIFT)

enum iavf_rx_desc_status_bits {
	/* Note: These are predefined bit offsets */
	IAVF_RX_DESC_STATUS_DD_SHIFT		= 0,
	IAVF_RX_DESC_STATUS_EOF_SHIFT		= 1,
	IAVF_RX_DESC_STATUS_L2TAG1P_SHIFT	= 2,
	IAVF_RX_DESC_STATUS_L3L4P_SHIFT		= 3,
	IAVF_RX_DESC_STATUS_CRCP_SHIFT		= 4,
	IAVF_RX_DESC_STATUS_TSYNINDX_SHIFT	= 5, /* 2 BITS */
	IAVF_RX_DESC_STATUS_TSYNVALID_SHIFT	= 7,
	IAVF_RX_DESC_STATUS_EXT_UDP_0_SHIFT	= 8,

	IAVF_RX_DESC_STATUS_UMBCAST_SHIFT	= 9, /* 2 BITS */
	IAVF_RX_DESC_STATUS_FLM_SHIFT		= 11,
	IAVF_RX_DESC_STATUS_FLTSTAT_SHIFT	= 12, /* 2 BITS */
	IAVF_RX_DESC_STATUS_LPBK_SHIFT		= 14,
	IAVF_RX_DESC_STATUS_IPV6EXADD_SHIFT	= 15,
	IAVF_RX_DESC_STATUS_RESERVED_SHIFT	= 16, /* 2 BITS */
	IAVF_RX_DESC_STATUS_INT_UDP_0_SHIFT	= 18,
	IAVF_RX_DESC_STATUS_LAST /* this entry must be last!!! */
};

#define IAVF_RXD_QW1_STATUS_SHIFT	0
#define IAVF_RXD_QW1_STATUS_MASK	((BIT(IAVF_RX_DESC_STATUS_LAST) - 1) \
					 << IAVF_RXD_QW1_STATUS_SHIFT)

#define IAVF_RXD_QW1_STATUS_TSYNINDX_SHIFT IAVF_RX_DESC_STATUS_TSYNINDX_SHIFT
#define IAVF_RXD_QW1_STATUS_TSYNINDX_MASK  (0x3UL << \
					    IAVF_RXD_QW1_STATUS_TSYNINDX_SHIFT)

#define IAVF_RXD_QW1_STATUS_TSYNVALID_SHIFT IAVF_RX_DESC_STATUS_TSYNVALID_SHIFT
#define IAVF_RXD_QW1_STATUS_TSYNVALID_MASK   BIT_ULL(IAVF_RXD_QW1_STATUS_TSYNVALID_SHIFT)

#define IAVF_RXD_QW1_STATUS_UMBCAST_SHIFT	IAVF_RX_DESC_STATUS_UMBCAST
#define IAVF_RXD_QW1_STATUS_UMBCAST_MASK	(0x3UL << \
					 IAVF_RXD_QW1_STATUS_UMBCAST_SHIFT)

enum iavf_rx_desc_fltstat_values {
	IAVF_RX_DESC_FLTSTAT_NO_DATA	= 0,
	IAVF_RX_DESC_FLTSTAT_RSV_FD_ID	= 1, /* 16byte desc? FD_ID : RSV */
	IAVF_RX_DESC_FLTSTAT_RSV	= 2,
	IAVF_RX_DESC_FLTSTAT_RSS_HASH	= 3,
};

#define IAVF_RXD_PACKET_TYPE_UNICAST	0
#define IAVF_RXD_PACKET_TYPE_MULTICAST	1
#define IAVF_RXD_PACKET_TYPE_BROADCAST	2
#define IAVF_RXD_PACKET_TYPE_MIRRORED	3

#define IAVF_RXD_QW1_ERROR_SHIFT	19
#define IAVF_RXD_QW1_ERROR_MASK		(0xFFUL << IAVF_RXD_QW1_ERROR_SHIFT)

enum iavf_rx_desc_error_bits {
	/* Note: These are predefined bit offsets */
	IAVF_RX_DESC_ERROR_RXE_SHIFT		= 0,
	IAVF_RX_DESC_ERROR_RECIPE_SHIFT		= 1,
	IAVF_RX_DESC_ERROR_HBO_SHIFT		= 2,
	IAVF_RX_DESC_ERROR_L3L4E_SHIFT		= 3, /* 3 BITS */
	IAVF_RX_DESC_ERROR_IPE_SHIFT		= 3,
	IAVF_RX_DESC_ERROR_L4E_SHIFT		= 4,
	IAVF_RX_DESC_ERROR_EIPE_SHIFT		= 5,
	IAVF_RX_DESC_ERROR_OVERSIZE_SHIFT	= 6,
	IAVF_RX_DESC_ERROR_PPRS_SHIFT		= 7
};

enum iavf_rx_desc_error_l3l4e_fcoe_masks {
	IAVF_RX_DESC_ERROR_L3L4E_NONE		= 0,
	IAVF_RX_DESC_ERROR_L3L4E_PROT		= 1,
	IAVF_RX_DESC_ERROR_L3L4E_FC		= 2,
	IAVF_RX_DESC_ERROR_L3L4E_DMAC_ERR	= 3,
	IAVF_RX_DESC_ERROR_L3L4E_DMAC_WARN	= 4
};

#define IAVF_RXD_QW1_PTYPE_SHIFT	30
#define IAVF_RXD_QW1_PTYPE_MASK		(0xFFULL << IAVF_RXD_QW1_PTYPE_SHIFT)

/* Packet type non-ip values */
enum iavf_rx_l2_ptype {
	IAVF_RX_PTYPE_L2_RESERVED			= 0,
	IAVF_RX_PTYPE_L2_MAC_PAY2			= 1,
	IAVF_RX_PTYPE_L2_TIMESYNC_PAY2			= 2,
	IAVF_RX_PTYPE_L2_FIP_PAY2			= 3,
	IAVF_RX_PTYPE_L2_OUI_PAY2			= 4,
	IAVF_RX_PTYPE_L2_MACCNTRL_PAY2			= 5,
	IAVF_RX_PTYPE_L2_LLDP_PAY2			= 6,
	IAVF_RX_PTYPE_L2_ECP_PAY2			= 7,
	IAVF_RX_PTYPE_L2_EVB_PAY2			= 8,
	IAVF_RX_PTYPE_L2_QCN_PAY2			= 9,
	IAVF_RX_PTYPE_L2_EAPOL_PAY2			= 10,
	IAVF_RX_PTYPE_L2_ARP				= 11,
	IAVF_RX_PTYPE_L2_FCOE_PAY3			= 12,
	IAVF_RX_PTYPE_L2_FCOE_FCDATA_PAY3		= 13,
	IAVF_RX_PTYPE_L2_FCOE_FCRDY_PAY3		= 14,
	IAVF_RX_PTYPE_L2_FCOE_FCRSP_PAY3		= 15,
	IAVF_RX_PTYPE_L2_FCOE_FCOTHER_PA		= 16,
	IAVF_RX_PTYPE_L2_FCOE_VFT_PAY3			= 17,
	IAVF_RX_PTYPE_L2_FCOE_VFT_FCDATA		= 18,
	IAVF_RX_PTYPE_L2_FCOE_VFT_FCRDY			= 19,
	IAVF_RX_PTYPE_L2_FCOE_VFT_FCRSP			= 20,
	IAVF_RX_PTYPE_L2_FCOE_VFT_FCOTHER		= 21,
	IAVF_RX_PTYPE_GRENAT4_MAC_PAY3			= 58,
	IAVF_RX_PTYPE_GRENAT4_MACVLAN_IPV6_ICMP_PAY4	= 87,
	IAVF_RX_PTYPE_GRENAT6_MAC_PAY3			= 124,
	IAVF_RX_PTYPE_GRENAT6_MACVLAN_IPV6_ICMP_PAY4	= 153,
	IAVF_RX_PTYPE_PARSER_ABORTED			= 255
};

struct iavf_rx_ptype_decoded {
	u32 ptype:8;
	u32 known:1;
	u32 outer_ip:1;
	u32 outer_ip_ver:1;
	u32 outer_frag:1;
	u32 tunnel_type:3;
	u32 tunnel_end_prot:2;
	u32 tunnel_end_frag:1;
	u32 inner_prot:4;
	u32 payload_layer:3;
};

enum iavf_rx_ptype_outer_ip {
	IAVF_RX_PTYPE_OUTER_L2	= 0,
	IAVF_RX_PTYPE_OUTER_IP	= 1
};

enum iavf_rx_ptype_outer_ip_ver {
	IAVF_RX_PTYPE_OUTER_NONE	= 0,
	IAVF_RX_PTYPE_OUTER_IPV4	= 0,
	IAVF_RX_PTYPE_OUTER_IPV6	= 1
};

enum iavf_rx_ptype_outer_fragmented {
	IAVF_RX_PTYPE_NOT_FRAG	= 0,
	IAVF_RX_PTYPE_FRAG	= 1
};

enum iavf_rx_ptype_tunnel_type {
	IAVF_RX_PTYPE_TUNNEL_NONE		= 0,
	IAVF_RX_PTYPE_TUNNEL_IP_IP		= 1,
	IAVF_RX_PTYPE_TUNNEL_IP_GRENAT		= 2,
	IAVF_RX_PTYPE_TUNNEL_IP_GRENAT_MAC	= 3,
	IAVF_RX_PTYPE_TUNNEL_IP_GRENAT_MAC_VLAN	= 4,
};

enum iavf_rx_ptype_tunnel_end_prot {
	IAVF_RX_PTYPE_TUNNEL_END_NONE	= 0,
	IAVF_RX_PTYPE_TUNNEL_END_IPV4	= 1,
	IAVF_RX_PTYPE_TUNNEL_END_IPV6	= 2,
};

enum iavf_rx_ptype_inner_prot {
	IAVF_RX_PTYPE_INNER_PROT_NONE		= 0,
	IAVF_RX_PTYPE_INNER_PROT_UDP		= 1,
	IAVF_RX_PTYPE_INNER_PROT_TCP		= 2,
	IAVF_RX_PTYPE_INNER_PROT_SCTP		= 3,
	IAVF_RX_PTYPE_INNER_PROT_ICMP		= 4,
	IAVF_RX_PTYPE_INNER_PROT_TIMESYNC	= 5
};

enum iavf_rx_ptype_payload_layer {
	IAVF_RX_PTYPE_PAYLOAD_LAYER_NONE	= 0,
	IAVF_RX_PTYPE_PAYLOAD_LAYER_PAY2	= 1,
	IAVF_RX_PTYPE_PAYLOAD_LAYER_PAY3	= 2,
	IAVF_RX_PTYPE_PAYLOAD_LAYER_PAY4	= 3,
};

#define IAVF_RX_PTYPE_BIT_MASK		0x0FFFFFFF
#define IAVF_RX_PTYPE_SHIFT		56

#define IAVF_RXD_QW1_LENGTH_PBUF_SHIFT	38
#define IAVF_RXD_QW1_LENGTH_PBUF_MASK	(0x3FFFULL << \
					 IAVF_RXD_QW1_LENGTH_PBUF_SHIFT)

#define IAVF_RXD_QW1_LENGTH_HBUF_SHIFT	52
#define IAVF_RXD_QW1_LENGTH_HBUF_MASK	(0x7FFULL << \
					 IAVF_RXD_QW1_LENGTH_HBUF_SHIFT)

#define IAVF_RXD_QW1_LENGTH_SPH_SHIFT	63
#define IAVF_RXD_QW1_LENGTH_SPH_MASK	BIT_ULL(IAVF_RXD_QW1_LENGTH_SPH_SHIFT)

#define IAVF_RXD_QW1_NEXTP_SHIFT	38
#define IAVF_RXD_QW1_NEXTP_MASK		(0x1FFFULL << IAVF_RXD_QW1_NEXTP_SHIFT)

#define IAVF_RXD_QW2_EXT_STATUS_SHIFT	0
#define IAVF_RXD_QW2_EXT_STATUS_MASK	(0xFFFFFUL << \
					 IAVF_RXD_QW2_EXT_STATUS_SHIFT)

enum iavf_rx_desc_ext_status_bits {
	/* Note: These are predefined bit offsets */
	IAVF_RX_DESC_EXT_STATUS_L2TAG2P_SHIFT	= 0,
	IAVF_RX_DESC_EXT_STATUS_L2TAG3P_SHIFT	= 1,
	IAVF_RX_DESC_EXT_STATUS_FLEXBL_SHIFT	= 2, /* 2 BITS */
	IAVF_RX_DESC_EXT_STATUS_FLEXBH_SHIFT	= 4, /* 2 BITS */
	IAVF_RX_DESC_EXT_STATUS_FDLONGB_SHIFT	= 9,
	IAVF_RX_DESC_EXT_STATUS_FCOELONGB_SHIFT	= 10,
	IAVF_RX_DESC_EXT_STATUS_PELONGB_SHIFT	= 11,
};

#define IAVF_RXD_QW2_L2TAG2_SHIFT	0
#define IAVF_RXD_QW2_L2TAG2_MASK	(0xFFFFUL << IAVF_RXD_QW2_L2TAG2_SHIFT)

#define IAVF_RXD_QW2_L2TAG3_SHIFT	16
#define IAVF_RXD_QW2_L2TAG3_MASK	(0xFFFFUL << IAVF_RXD_QW2_L2TAG3_SHIFT)

enum iavf_rx_desc_pe_status_bits {
	/* Note: These are predefined bit offsets */
	IAVF_RX_DESC_PE_STATUS_QPID_SHIFT	= 0, /* 18 BITS */
	IAVF_RX_DESC_PE_STATUS_L4PORT_SHIFT	= 0, /* 16 BITS */
	IAVF_RX_DESC_PE_STATUS_IPINDEX_SHIFT	= 16, /* 8 BITS */
	IAVF_RX_DESC_PE_STATUS_QPIDHIT_SHIFT	= 24,
	IAVF_RX_DESC_PE_STATUS_APBVTHIT_SHIFT	= 25,
	IAVF_RX_DESC_PE_STATUS_PORTV_SHIFT	= 26,
	IAVF_RX_DESC_PE_STATUS_URG_SHIFT	= 27,
	IAVF_RX_DESC_PE_STATUS_IPFRAG_SHIFT	= 28,
	IAVF_RX_DESC_PE_STATUS_IPOPT_SHIFT	= 29
};

#define IAVF_RX_PROG_STATUS_DESC_LENGTH_SHIFT		38
#define IAVF_RX_PROG_STATUS_DESC_LENGTH			0x2000000

#define IAVF_RX_PROG_STATUS_DESC_QW1_PROGID_SHIFT	2
#define IAVF_RX_PROG_STATUS_DESC_QW1_PROGID_MASK	(0x7UL << \
				IAVF_RX_PROG_STATUS_DESC_QW1_PROGID_SHIFT)

#define IAVF_RX_PROG_STATUS_DESC_QW1_STATUS_SHIFT	0
#define IAVF_RX_PROG_STATUS_DESC_QW1_STATUS_MASK	(0x7FFFUL << \
				IAVF_RX_PROG_STATUS_DESC_QW1_STATUS_SHIFT)

#define IAVF_RX_PROG_STATUS_DESC_QW1_ERROR_SHIFT	19
#define IAVF_RX_PROG_STATUS_DESC_QW1_ERROR_MASK		(0x3FUL << \
				IAVF_RX_PROG_STATUS_DESC_QW1_ERROR_SHIFT)

enum iavf_rx_prog_status_desc_status_bits {
	/* Note: These are predefined bit offsets */
	IAVF_RX_PROG_STATUS_DESC_DD_SHIFT	= 0,
	IAVF_RX_PROG_STATUS_DESC_PROG_ID_SHIFT	= 2 /* 3 BITS */
};

enum iavf_rx_prog_status_desc_prog_id_masks {
	IAVF_RX_PROG_STATUS_DESC_FD_FILTER_STATUS	= 1,
	IAVF_RX_PROG_STATUS_DESC_FCOE_CTXT_PROG_STATUS	= 2,
	IAVF_RX_PROG_STATUS_DESC_FCOE_CTXT_INVL_STATUS	= 4,
};

enum iavf_rx_prog_status_desc_error_bits {
	/* Note: These are predefined bit offsets */
	IAVF_RX_PROG_STATUS_DESC_FD_TBL_FULL_SHIFT	= 0,
	IAVF_RX_PROG_STATUS_DESC_NO_FD_ENTRY_SHIFT	= 1,
	IAVF_RX_PROG_STATUS_DESC_FCOE_TBL_FULL_SHIFT	= 2,
	IAVF_RX_PROG_STATUS_DESC_FCOE_CONFLICT_SHIFT	= 3
};

#define IAVF_TWO_BIT_MASK	0x3
#define IAVF_THREE_BIT_MASK	0x7
#define IAVF_FOUR_BIT_MASK	0xF
#define IAVF_EIGHTEEN_BIT_MASK	0x3FFFF

/* TX Descriptor */
struct iavf_tx_desc {
	__le64 buffer_addr; /* Address of descriptor's data buf */
	__le64 cmd_type_offset_bsz;
};

#define IAVF_TXD_QW1_DTYPE_SHIFT	0
#define IAVF_TXD_QW1_DTYPE_MASK		(0xFUL << IAVF_TXD_QW1_DTYPE_SHIFT)

enum iavf_tx_desc_dtype_value {
	IAVF_TX_DESC_DTYPE_DATA		= 0x0,
	IAVF_TX_DESC_DTYPE_NOP		= 0x1, /* same as Context desc */
	IAVF_TX_DESC_DTYPE_CONTEXT	= 0x1,
	IAVF_TX_DESC_DTYPE_FCOE_CTX	= 0x2,
	IAVF_TX_DESC_DTYPE_FILTER_PROG	= 0x8,
	IAVF_TX_DESC_DTYPE_DDP_CTX	= 0x9,
	IAVF_TX_DESC_DTYPE_FLEX_DATA	= 0xB,
	IAVF_TX_DESC_DTYPE_FLEX_CTX_1	= 0xC,
	IAVF_TX_DESC_DTYPE_FLEX_CTX_2	= 0xD,
	IAVF_TX_DESC_DTYPE_DESC_DONE	= 0xF
};

#define IAVF_TXD_QW1_CMD_SHIFT	4
#define IAVF_TXD_QW1_CMD_MASK	(0x3FFUL << IAVF_TXD_QW1_CMD_SHIFT)

enum iavf_tx_desc_cmd_bits {
	IAVF_TX_DESC_CMD_EOP			= 0x0001,
	IAVF_TX_DESC_CMD_RS			= 0x0002,
	IAVF_TX_DESC_CMD_ICRC			= 0x0004,
	IAVF_TX_DESC_CMD_IL2TAG1		= 0x0008,
	IAVF_TX_DESC_CMD_DUMMY			= 0x0010,
	IAVF_TX_DESC_CMD_IIPT_NONIP		= 0x0000, /* 2 BITS */
	IAVF_TX_DESC_CMD_IIPT_IPV6		= 0x0020, /* 2 BITS */
	IAVF_TX_DESC_CMD_IIPT_IPV4		= 0x0040, /* 2 BITS */
	IAVF_TX_DESC_CMD_IIPT_IPV4_CSUM		= 0x0060, /* 2 BITS */
	IAVF_TX_DESC_CMD_FCOET			= 0x0080,
	IAVF_TX_DESC_CMD_L4T_EOFT_UNK		= 0x0000, /* 2 BITS */
	IAVF_TX_DESC_CMD_L4T_EOFT_TCP		= 0x0100, /* 2 BITS */
	IAVF_TX_DESC_CMD_L4T_EOFT_SCTP		= 0x0200, /* 2 BITS */
	IAVF_TX_DESC_CMD_L4T_EOFT_UDP		= 0x0300, /* 2 BITS */
	IAVF_TX_DESC_CMD_L4T_EOFT_EOF_N		= 0x0000, /* 2 BITS */
	IAVF_TX_DESC_CMD_L4T_EOFT_EOF_T		= 0x0100, /* 2 BITS */
	IAVF_TX_DESC_CMD_L4T_EOFT_EOF_NI	= 0x0200, /* 2 BITS */
	IAVF_TX_DESC_CMD_L4T_EOFT_EOF_A		= 0x0300, /* 2 BITS */
};

#define IAVF_TXD_QW1_OFFSET_SHIFT	16
#define IAVF_TXD_QW1_OFFSET_MASK	(0x3FFFFULL << \
					 IAVF_TXD_QW1_OFFSET_SHIFT)

enum iavf_tx_desc_length_fields {
	/* Note: These are predefined bit offsets */
	IAVF_TX_DESC_LENGTH_MACLEN_SHIFT	= 0, /* 7 BITS */
	IAVF_TX_DESC_LENGTH_IPLEN_SHIFT		= 7, /* 7 BITS */
	IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT	= 14 /* 4 BITS */
};

#define IAVF_TXD_QW1_MACLEN_MASK (0x7FUL << IAVF_TX_DESC_LENGTH_MACLEN_SHIFT)
#define IAVF_TXD_QW1_IPLEN_MASK  (0x7FUL << IAVF_TX_DESC_LENGTH_IPLEN_SHIFT)
#define IAVF_TXD_QW1_L4LEN_MASK  (0xFUL << IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT)
#define IAVF_TXD_QW1_FCLEN_MASK  (0xFUL << IAVF_TX_DESC_LENGTH_L4_FC_LEN_SHIFT)

#define IAVF_TXD_QW1_TX_BUF_SZ_SHIFT	34
#define IAVF_TXD_QW1_TX_BUF_SZ_MASK	(0x3FFFULL << \
					 IAVF_TXD_QW1_TX_BUF_SZ_SHIFT)

#define IAVF_TXD_QW1_L2TAG1_SHIFT	48
#define IAVF_TXD_QW1_L2TAG1_MASK	(0xFFFFULL << IAVF_TXD_QW1_L2TAG1_SHIFT)

/* Context descriptors */
struct iavf_tx_context_desc {
	__le32 tunneling_params;
	__le16 l2tag2;
	__le16 rsvd;
	__le64 type_cmd_tso_mss;
};

#define IAVF_TXD_CTX_QW1_DTYPE_SHIFT	0
#define IAVF_TXD_CTX_QW1_DTYPE_MASK	(0xFUL << IAVF_TXD_CTX_QW1_DTYPE_SHIFT)

#define IAVF_TXD_CTX_QW1_CMD_SHIFT	4
#define IAVF_TXD_CTX_QW1_CMD_MASK	(0xFFFFUL << IAVF_TXD_CTX_QW1_CMD_SHIFT)

enum iavf_tx_ctx_desc_cmd_bits {
	IAVF_TX_CTX_DESC_TSO		= 0x01,
	IAVF_TX_CTX_DESC_TSYN		= 0x02,
	IAVF_TX_CTX_DESC_IL2TAG2	= 0x04,
	IAVF_TX_CTX_DESC_IL2TAG2_IL2H	= 0x08,
	IAVF_TX_CTX_DESC_SWTCH_NOTAG	= 0x00,
	IAVF_TX_CTX_DESC_SWTCH_UPLINK	= 0x10,
	IAVF_TX_CTX_DESC_SWTCH_LOCAL	= 0x20,
	IAVF_TX_CTX_DESC_SWTCH_VSI	= 0x30,
	IAVF_TX_CTX_DESC_SWPE		= 0x40
};

struct iavf_nop_desc {
	__le64 rsvd;
	__le64 dtype_cmd;
};

#define IAVF_TXD_NOP_QW1_DTYPE_SHIFT	0
#define IAVF_TXD_NOP_QW1_DTYPE_MASK	(0xFUL << IAVF_TXD_NOP_QW1_DTYPE_SHIFT)

#define IAVF_TXD_NOP_QW1_CMD_SHIFT	4
#define IAVF_TXD_NOP_QW1_CMD_MASK	(0x7FUL << IAVF_TXD_NOP_QW1_CMD_SHIFT)

enum iavf_tx_nop_desc_cmd_bits {
	/* Note: These are predefined bit offsets */
	IAVF_TX_NOP_DESC_EOP_SHIFT	= 0,
	IAVF_TX_NOP_DESC_RS_SHIFT	= 1,
	IAVF_TX_NOP_DESC_RSV_SHIFT	= 2 /* 5 bits */
};

/* Packet Classifier Types for filters */
enum iavf_filter_pctype {
	/* Note: Values 0-28 are reserved for future use.
	 * Value 29, 30, 32 are not supported on XL710 and X710.
	 */
	IAVF_FILTER_PCTYPE_NONF_UNICAST_IPV4_UDP	= 29,
	IAVF_FILTER_PCTYPE_NONF_MULTICAST_IPV4_UDP	= 30,
	IAVF_FILTER_PCTYPE_NONF_IPV4_UDP		= 31,
	IAVF_FILTER_PCTYPE_NONF_IPV4_TCP_SYN_NO_ACK	= 32,
	IAVF_FILTER_PCTYPE_NONF_IPV4_TCP		= 33,
	IAVF_FILTER_PCTYPE_NONF_IPV4_SCTP		= 34,
	IAVF_FILTER_PCTYPE_NONF_IPV4_OTHER		= 35,
	IAVF_FILTER_PCTYPE_FRAG_IPV4			= 36,
	/* Note: Values 37-38 are reserved for future use.
	 * Value 39, 40, 42 are not supported on XL710 and X710.
	 */
	IAVF_FILTER_PCTYPE_NONF_UNICAST_IPV6_UDP	= 39,
	IAVF_FILTER_PCTYPE_NONF_MULTICAST_IPV6_UDP	= 40,
	IAVF_FILTER_PCTYPE_NONF_IPV6_UDP		= 41,
	IAVF_FILTER_PCTYPE_NONF_IPV6_TCP_SYN_NO_ACK	= 42,
	IAVF_FILTER_PCTYPE_NONF_IPV6_TCP		= 43,
	IAVF_FILTER_PCTYPE_NONF_IPV6_SCTP		= 44,
	IAVF_FILTER_PCTYPE_NONF_IPV6_OTHER		= 45,
	IAVF_FILTER_PCTYPE_FRAG_IPV6			= 46,
	/* Note: Value 47 is reserved for future use */
	IAVF_FILTER_PCTYPE_FCOE_OX			= 48,
	IAVF_FILTER_PCTYPE_FCOE_RX			= 49,
	IAVF_FILTER_PCTYPE_FCOE_OTHER			= 50,
	/* Note: Values 51-62 are reserved for future use */
	IAVF_FILTER_PCTYPE_L2_PAYLOAD			= 63,
};

#define IAVF_TXD_FLTR_QW1_DTYPE_SHIFT	0
#define IAVF_TXD_FLTR_QW1_DTYPE_MASK	(0xFUL << IAVF_TXD_FLTR_QW1_DTYPE_SHIFT)

#define IAVF_TXD_FLTR_QW1_ATR_SHIFT	(0xEULL + \
					 IAVF_TXD_FLTR_QW1_CMD_SHIFT)
#define IAVF_TXD_FLTR_QW1_ATR_MASK	BIT_ULL(IAVF_TXD_FLTR_QW1_ATR_SHIFT)


#define IAVF_TXD_CTX_QW1_TSO_LEN_SHIFT	30
#define IAVF_TXD_CTX_QW1_TSO_LEN_MASK	(0x3FFFFULL << \
					 IAVF_TXD_CTX_QW1_TSO_LEN_SHIFT)

#define IAVF_TXD_CTX_QW1_MSS_SHIFT	50
#define IAVF_TXD_CTX_QW1_MSS_MASK	(0x3FFFULL << \
					 IAVF_TXD_CTX_QW1_MSS_SHIFT)

#define IAVF_TXD_CTX_QW1_VSI_SHIFT	50
#define IAVF_TXD_CTX_QW1_VSI_MASK	(0x1FFULL << IAVF_TXD_CTX_QW1_VSI_SHIFT)

#define IAVF_TXD_CTX_QW0_EXT_IP_SHIFT	0
#define IAVF_TXD_CTX_QW0_EXT_IP_MASK	(0x3ULL << \
					 IAVF_TXD_CTX_QW0_EXT_IP_SHIFT)

enum iavf_tx_ctx_desc_eipt_offload {
	IAVF_TX_CTX_EXT_IP_NONE		= 0x0,
	IAVF_TX_CTX_EXT_IP_IPV6		= 0x1,
	IAVF_TX_CTX_EXT_IP_IPV4_NO_CSUM	= 0x2,
	IAVF_TX_CTX_EXT_IP_IPV4		= 0x3
};

#define IAVF_TXD_CTX_QW0_EXT_IPLEN_SHIFT	2
#define IAVF_TXD_CTX_QW0_EXT_IPLEN_MASK	(0x3FULL << \
					 IAVF_TXD_CTX_QW0_EXT_IPLEN_SHIFT)

#define IAVF_TXD_CTX_QW0_NATT_SHIFT	9
#define IAVF_TXD_CTX_QW0_NATT_MASK	(0x3ULL << IAVF_TXD_CTX_QW0_NATT_SHIFT)

#define IAVF_TXD_CTX_UDP_TUNNELING	BIT_ULL(IAVF_TXD_CTX_QW0_NATT_SHIFT)
#define IAVF_TXD_CTX_GRE_TUNNELING	(0x2ULL << IAVF_TXD_CTX_QW0_NATT_SHIFT)

#define IAVF_TXD_CTX_QW0_EIP_NOINC_SHIFT	11
#define IAVF_TXD_CTX_QW0_EIP_NOINC_MASK \
				       BIT_ULL(IAVF_TXD_CTX_QW0_EIP_NOINC_SHIFT)

#define IAVF_TXD_CTX_EIP_NOINC_IPID_CONST	IAVF_TXD_CTX_QW0_EIP_NOINC_MASK

#define IAVF_TXD_CTX_QW0_NATLEN_SHIFT	12
#define IAVF_TXD_CTX_QW0_NATLEN_MASK	(0X7FULL << \
					 IAVF_TXD_CTX_QW0_NATLEN_SHIFT)

#define IAVF_TXD_CTX_QW0_DECTTL_SHIFT	19
#define IAVF_TXD_CTX_QW0_DECTTL_MASK	(0xFULL << \
					 IAVF_TXD_CTX_QW0_DECTTL_SHIFT)

#define IAVF_TXD_CTX_QW0_L4T_CS_SHIFT	23
#define IAVF_TXD_CTX_QW0_L4T_CS_MASK	BIT_ULL(IAVF_TXD_CTX_QW0_L4T_CS_SHIFT)

/* Statistics collected by each port, VSI, VEB, and S-channel */
struct iavf_eth_stats {
	u64 rx_bytes;			/* gorc */
	u64 rx_unicast;			/* uprc */
	u64 rx_multicast;		/* mprc */
	u64 rx_broadcast;		/* bprc */
	u64 rx_discards;		/* rdpc */
	u64 rx_unknown_protocol;	/* rupp */
	u64 tx_bytes;			/* gotc */
	u64 tx_unicast;			/* uptc */
	u64 tx_multicast;		/* mptc */
	u64 tx_broadcast;		/* bptc */
	u64 tx_discards;		/* tdpc */
	u64 tx_errors;			/* tepc */
};
#define IAVF_SR_PCIE_ANALOG_CONFIG_PTR		0x03
#define IAVF_SR_PHY_ANALOG_CONFIG_PTR		0x04
#define IAVF_SR_OPTION_ROM_PTR			0x05
#define IAVF_SR_RO_PCIR_REGS_AUTO_LOAD_PTR	0x06
#define IAVF_SR_AUTO_GENERATED_POINTERS_PTR	0x07
#define IAVF_SR_PCIR_REGS_AUTO_LOAD_PTR		0x08
#define IAVF_SR_EMP_GLOBAL_MODULE_PTR		0x09
#define IAVF_SR_RO_PCIE_LCB_PTR			0x0A
#define IAVF_SR_EMP_IMAGE_PTR			0x0B
#define IAVF_SR_PE_IMAGE_PTR			0x0C
#define IAVF_SR_CSR_PROTECTED_LIST_PTR		0x0D
#define IAVF_SR_MNG_CONFIG_PTR			0x0E
#define IAVF_SR_PBA_FLAGS			0x15
#define IAVF_SR_PBA_BLOCK_PTR			0x16
#define IAVF_SR_BOOT_CONFIG_PTR			0x17
#define IAVF_SR_PERMANENT_SAN_MAC_ADDRESS_PTR	0x28
#define IAVF_SR_NVM_MAP_VERSION			0x29
#define IAVF_SR_NVM_IMAGE_VERSION		0x2A
#define IAVF_SR_NVM_STRUCTURE_VERSION		0x2B
#define IAVF_SR_PXE_SETUP_PTR			0x30
#define IAVF_SR_PXE_CONFIG_CUST_OPTIONS_PTR	0x31
#define IAVF_SR_NVM_ORIGINAL_EETRACK_LO		0x34
#define IAVF_SR_NVM_ORIGINAL_EETRACK_HI		0x35
#define IAVF_SR_SW_ETHERNET_MAC_ADDRESS_PTR	0x37
#define IAVF_SR_POR_REGS_AUTO_LOAD_PTR		0x38
#define IAVF_SR_EMPR_REGS_AUTO_LOAD_PTR		0x3A
#define IAVF_SR_GLOBR_REGS_AUTO_LOAD_PTR	0x3B
#define IAVF_SR_CORER_REGS_AUTO_LOAD_PTR	0x3C
#define IAVF_SR_PHY_ACTIVITY_LIST_PTR		0x3D
#define IAVF_SR_1ST_FREE_PROVISION_AREA_PTR	0x40
#define IAVF_SR_4TH_FREE_PROVISION_AREA_PTR	0x42
#define IAVF_SR_3RD_FREE_PROVISION_AREA_PTR	0x44
#define IAVF_SR_2ND_FREE_PROVISION_AREA_PTR	0x46
#define IAVF_SR_EMP_SR_SETTINGS_PTR		0x48
#define IAVF_SR_FEATURE_CONFIGURATION_PTR	0x49
#define IAVF_SR_CONFIGURATION_METADATA_PTR	0x4D
#define IAVF_SR_IMMEDIATE_VALUES_PTR		0x4E
#define IAVF_SR_OCP_CFG_WORD0			0x2B
#define IAVF_SR_OCP_ENABLED			BIT(15)
#define IAVF_SR_BUF_ALIGNMENT		4096

struct iavf_lldp_variables {
	u16 length;
	u16 adminstatus;
	u16 msgfasttx;
	u16 msgtxinterval;
	u16 txparams;
	u16 timers;
	u16 crc8;
};

/* Offsets into Alternate Ram */
#define IAVF_ALT_STRUCT_FIRST_PF_OFFSET		0   /* in dwords */
#define IAVF_ALT_STRUCT_DWORDS_PER_PF		64   /* in dwords */
#define IAVF_ALT_STRUCT_OUTER_VLAN_TAG_OFFSET	0xD  /* in dwords */
#define IAVF_ALT_STRUCT_USER_PRIORITY_OFFSET	0xC  /* in dwords */
#define IAVF_ALT_STRUCT_MIN_BW_OFFSET		0xE  /* in dwords */
#define IAVF_ALT_STRUCT_MAX_BW_OFFSET		0xF  /* in dwords */

/* Alternate Ram Bandwidth Masks */
#define IAVF_ALT_BW_VALUE_MASK		0xFF
#define IAVF_ALT_BW_RELATIVE_MASK	0x40000000
#define IAVF_ALT_BW_VALID_MASK		0x80000000

#define IAVF_DDP_TRACKID_RDONLY		0
#define IAVF_DDP_TRACKID_INVALID	0xFFFFFFFF
#define SECTION_TYPE_RB_MMIO	0x00001800
#define SECTION_TYPE_RB_AQ	0x00001801
#define SECTION_TYPE_PROTO	0x80000002
#define SECTION_TYPE_PCTYPE	0x80000003
#define SECTION_TYPE_PTYPE	0x80000004
struct iavf_profile_tlv_section_record {
	u8 rtype;
	u8 type;
	u16 len;
	u8 data[12];
};

/* Generic AQ section in proflie */
struct iavf_profile_aq_section {
	u16 opcode;
	u16 flags;
	u8  param[16];
	u16 datalen;
	u8  data[1];
};

#endif /* _IAVF_TYPE_H_ */
