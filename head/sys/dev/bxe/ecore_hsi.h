/*-
 * Copyright (c) 2007-2014 QLogic Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS'
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>
__FBSDID("$FreeBSD$");

#ifndef ECORE_HSI_H
#define ECORE_HSI_H

#define FW_ENCODE_32BIT_PATTERN 0x1e1e1e1e

struct license_key {
    uint32_t reserved[6];

    uint32_t max_iscsi_conn;
#define LICENSE_MAX_ISCSI_TRGT_CONN_MASK  0xFFFF
#define LICENSE_MAX_ISCSI_TRGT_CONN_SHIFT 0
#define LICENSE_MAX_ISCSI_INIT_CONN_MASK  0xFFFF0000
#define LICENSE_MAX_ISCSI_INIT_CONN_SHIFT 16

    uint32_t reserved_a;

    uint32_t max_fcoe_conn;
#define LICENSE_MAX_FCOE_TRGT_CONN_MASK  0xFFFF
#define LICENSE_MAX_FCOE_TRGT_CONN_SHIFT 0
#define LICENSE_MAX_FCOE_INIT_CONN_MASK  0xFFFF0000
#define LICENSE_MAX_FCOE_INIT_CONN_SHIFT 16

    uint32_t reserved_b[4];
};

typedef struct license_key license_key_t;


/****************************************************************************
 * Shared HW configuration                                                  *
 ****************************************************************************/
#define PIN_CFG_NA                          0x00000000
#define PIN_CFG_GPIO0_P0                    0x00000001
#define PIN_CFG_GPIO1_P0                    0x00000002
#define PIN_CFG_GPIO2_P0                    0x00000003
#define PIN_CFG_GPIO3_P0                    0x00000004
#define PIN_CFG_GPIO0_P1                    0x00000005
#define PIN_CFG_GPIO1_P1                    0x00000006
#define PIN_CFG_GPIO2_P1                    0x00000007
#define PIN_CFG_GPIO3_P1                    0x00000008
#define PIN_CFG_EPIO0                       0x00000009
#define PIN_CFG_EPIO1                       0x0000000a
#define PIN_CFG_EPIO2                       0x0000000b
#define PIN_CFG_EPIO3                       0x0000000c
#define PIN_CFG_EPIO4                       0x0000000d
#define PIN_CFG_EPIO5                       0x0000000e
#define PIN_CFG_EPIO6                       0x0000000f
#define PIN_CFG_EPIO7                       0x00000010
#define PIN_CFG_EPIO8                       0x00000011
#define PIN_CFG_EPIO9                       0x00000012
#define PIN_CFG_EPIO10                      0x00000013
#define PIN_CFG_EPIO11                      0x00000014
#define PIN_CFG_EPIO12                      0x00000015
#define PIN_CFG_EPIO13                      0x00000016
#define PIN_CFG_EPIO14                      0x00000017
#define PIN_CFG_EPIO15                      0x00000018
#define PIN_CFG_EPIO16                      0x00000019
#define PIN_CFG_EPIO17                      0x0000001a
#define PIN_CFG_EPIO18                      0x0000001b
#define PIN_CFG_EPIO19                      0x0000001c
#define PIN_CFG_EPIO20                      0x0000001d
#define PIN_CFG_EPIO21                      0x0000001e
#define PIN_CFG_EPIO22                      0x0000001f
#define PIN_CFG_EPIO23                      0x00000020
#define PIN_CFG_EPIO24                      0x00000021
#define PIN_CFG_EPIO25                      0x00000022
#define PIN_CFG_EPIO26                      0x00000023
#define PIN_CFG_EPIO27                      0x00000024
#define PIN_CFG_EPIO28                      0x00000025
#define PIN_CFG_EPIO29                      0x00000026
#define PIN_CFG_EPIO30                      0x00000027
#define PIN_CFG_EPIO31                      0x00000028

/* EPIO definition */
#define EPIO_CFG_NA                         0x00000000
#define EPIO_CFG_EPIO0                      0x00000001
#define EPIO_CFG_EPIO1                      0x00000002
#define EPIO_CFG_EPIO2                      0x00000003
#define EPIO_CFG_EPIO3                      0x00000004
#define EPIO_CFG_EPIO4                      0x00000005
#define EPIO_CFG_EPIO5                      0x00000006
#define EPIO_CFG_EPIO6                      0x00000007
#define EPIO_CFG_EPIO7                      0x00000008
#define EPIO_CFG_EPIO8                      0x00000009
#define EPIO_CFG_EPIO9                      0x0000000a
#define EPIO_CFG_EPIO10                     0x0000000b
#define EPIO_CFG_EPIO11                     0x0000000c
#define EPIO_CFG_EPIO12                     0x0000000d
#define EPIO_CFG_EPIO13                     0x0000000e
#define EPIO_CFG_EPIO14                     0x0000000f
#define EPIO_CFG_EPIO15                     0x00000010
#define EPIO_CFG_EPIO16                     0x00000011
#define EPIO_CFG_EPIO17                     0x00000012
#define EPIO_CFG_EPIO18                     0x00000013
#define EPIO_CFG_EPIO19                     0x00000014
#define EPIO_CFG_EPIO20                     0x00000015
#define EPIO_CFG_EPIO21                     0x00000016
#define EPIO_CFG_EPIO22                     0x00000017
#define EPIO_CFG_EPIO23                     0x00000018
#define EPIO_CFG_EPIO24                     0x00000019
#define EPIO_CFG_EPIO25                     0x0000001a
#define EPIO_CFG_EPIO26                     0x0000001b
#define EPIO_CFG_EPIO27                     0x0000001c
#define EPIO_CFG_EPIO28                     0x0000001d
#define EPIO_CFG_EPIO29                     0x0000001e
#define EPIO_CFG_EPIO30                     0x0000001f
#define EPIO_CFG_EPIO31                     0x00000020

struct mac_addr {
	uint32_t upper;
	uint32_t lower;
};


struct shared_hw_cfg {			 /* NVRAM Offset */
	/* Up to 16 bytes of NULL-terminated string */
	uint8_t  part_num[16];		    /* 0x104 */

	uint32_t config;			/* 0x114 */
	#define SHARED_HW_CFG_MDIO_VOLTAGE_MASK             0x00000001
		#define SHARED_HW_CFG_MDIO_VOLTAGE_SHIFT             0
		#define SHARED_HW_CFG_MDIO_VOLTAGE_1_2V              0x00000000
		#define SHARED_HW_CFG_MDIO_VOLTAGE_2_5V              0x00000001

	#define SHARED_HW_CFG_PORT_SWAP                     0x00000004

	    #define SHARED_HW_CFG_BEACON_WOL_EN                  0x00000008

	    #define SHARED_HW_CFG_PCIE_GEN3_DISABLED            0x00000000
	    #define SHARED_HW_CFG_PCIE_GEN3_ENABLED             0x00000010

	#define SHARED_HW_CFG_MFW_SELECT_MASK               0x00000700
		#define SHARED_HW_CFG_MFW_SELECT_SHIFT               8
	/* Whatever MFW found in NVM
	   (if multiple found, priority order is: NC-SI, UMP, IPMI) */
		#define SHARED_HW_CFG_MFW_SELECT_DEFAULT             0x00000000
		#define SHARED_HW_CFG_MFW_SELECT_NC_SI               0x00000100
		#define SHARED_HW_CFG_MFW_SELECT_UMP                 0x00000200
		#define SHARED_HW_CFG_MFW_SELECT_IPMI                0x00000300
	/* Use SPIO4 as an arbiter between: 0-NC_SI, 1-IPMI
	  (can only be used when an add-in board, not BMC, pulls-down SPIO4) */
		#define SHARED_HW_CFG_MFW_SELECT_SPIO4_NC_SI_IPMI    0x00000400
	/* Use SPIO4 as an arbiter between: 0-UMP, 1-IPMI
	  (can only be used when an add-in board, not BMC, pulls-down SPIO4) */
		#define SHARED_HW_CFG_MFW_SELECT_SPIO4_UMP_IPMI      0x00000500
	/* Use SPIO4 as an arbiter between: 0-NC-SI, 1-UMP
	  (can only be used when an add-in board, not BMC, pulls-down SPIO4) */
		#define SHARED_HW_CFG_MFW_SELECT_SPIO4_NC_SI_UMP     0x00000600

	/* Adjust the PCIe G2 Tx amplitude driver for all Tx lanes. For
	   backwards compatibility, value of 0 is disabling this feature.
	    That means that though 0 is a valid value, it cannot be
	    configured. */
	#define SHARED_HW_CFG_G2_TX_DRIVE_MASK                        0x0000F000
	#define SHARED_HW_CFG_G2_TX_DRIVE_SHIFT                       12

	#define SHARED_HW_CFG_LED_MODE_MASK                 0x000F0000
		#define SHARED_HW_CFG_LED_MODE_SHIFT                 16
		#define SHARED_HW_CFG_LED_MAC1                       0x00000000
		#define SHARED_HW_CFG_LED_PHY1                       0x00010000
		#define SHARED_HW_CFG_LED_PHY2                       0x00020000
		#define SHARED_HW_CFG_LED_PHY3                       0x00030000
		#define SHARED_HW_CFG_LED_MAC2                       0x00040000
		#define SHARED_HW_CFG_LED_PHY4                       0x00050000
		#define SHARED_HW_CFG_LED_PHY5                       0x00060000
		#define SHARED_HW_CFG_LED_PHY6                       0x00070000
		#define SHARED_HW_CFG_LED_MAC3                       0x00080000
		#define SHARED_HW_CFG_LED_PHY7                       0x00090000
		#define SHARED_HW_CFG_LED_PHY9                       0x000a0000
		#define SHARED_HW_CFG_LED_PHY11                      0x000b0000
		#define SHARED_HW_CFG_LED_MAC4                       0x000c0000
		#define SHARED_HW_CFG_LED_PHY8                       0x000d0000
		#define SHARED_HW_CFG_LED_EXTPHY1                    0x000e0000
		#define SHARED_HW_CFG_LED_EXTPHY2                    0x000f0000

    #define SHARED_HW_CFG_SRIOV_MASK                    0x40000000
		#define SHARED_HW_CFG_SRIOV_DISABLED                 0x00000000
		#define SHARED_HW_CFG_SRIOV_ENABLED                  0x40000000

	#define SHARED_HW_CFG_ATC_MASK                      0x80000000
		#define SHARED_HW_CFG_ATC_DISABLED                   0x00000000
		#define SHARED_HW_CFG_ATC_ENABLED                    0x80000000

	uint32_t config2;			    /* 0x118 */

	#define SHARED_HW_CFG_PCIE_GEN2_MASK                0x00000100
	    #define SHARED_HW_CFG_PCIE_GEN2_SHIFT                8
	    #define SHARED_HW_CFG_PCIE_GEN2_DISABLED             0x00000000
	#define SHARED_HW_CFG_PCIE_GEN2_ENABLED              0x00000100

	#define SHARED_HW_CFG_SMBUS_TIMING_MASK             0x00001000
		#define SHARED_HW_CFG_SMBUS_TIMING_100KHZ            0x00000000
		#define SHARED_HW_CFG_SMBUS_TIMING_400KHZ            0x00001000

	#define SHARED_HW_CFG_HIDE_PORT1                    0x00002000


		/* Output low when PERST is asserted */
	#define SHARED_HW_CFG_SPIO4_FOLLOW_PERST_MASK       0x00008000
		#define SHARED_HW_CFG_SPIO4_FOLLOW_PERST_DISABLED    0x00000000
		#define SHARED_HW_CFG_SPIO4_FOLLOW_PERST_ENABLED     0x00008000

	#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_MASK    0x00070000
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_SHIFT    16
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_HW       0x00000000
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_0DB      0x00010000
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_3_5DB    0x00020000
		#define SHARED_HW_CFG_PCIE_GEN2_PREEMPHASIS_6_0DB    0x00030000

	/*  The fan failure mechanism is usually related to the PHY type
	      since the power consumption of the board is determined by the PHY.
	      Currently, fan is required for most designs with SFX7101, BCM8727
	      and BCM8481. If a fan is not required for a board which uses one
	      of those PHYs, this field should be set to "Disabled". If a fan is
	      required for a different PHY type, this option should be set to
	      "Enabled". The fan failure indication is expected on SPIO5 */
	#define SHARED_HW_CFG_FAN_FAILURE_MASK              0x00180000
		#define SHARED_HW_CFG_FAN_FAILURE_SHIFT              19
		#define SHARED_HW_CFG_FAN_FAILURE_PHY_TYPE           0x00000000
		#define SHARED_HW_CFG_FAN_FAILURE_DISABLED           0x00080000
		#define SHARED_HW_CFG_FAN_FAILURE_ENABLED            0x00100000

		/* ASPM Power Management support */
	#define SHARED_HW_CFG_ASPM_SUPPORT_MASK             0x00600000
		#define SHARED_HW_CFG_ASPM_SUPPORT_SHIFT             21
		#define SHARED_HW_CFG_ASPM_SUPPORT_L0S_L1_ENABLED    0x00000000
		#define SHARED_HW_CFG_ASPM_SUPPORT_L0S_DISABLED      0x00200000
		#define SHARED_HW_CFG_ASPM_SUPPORT_L1_DISABLED       0x00400000
		#define SHARED_HW_CFG_ASPM_SUPPORT_L0S_L1_DISABLED   0x00600000

	/* The value of PM_TL_IGNORE_REQS (bit0) in PCI register
	   tl_control_0 (register 0x2800) */
	#define SHARED_HW_CFG_PREVENT_L1_ENTRY_MASK         0x00800000
		#define SHARED_HW_CFG_PREVENT_L1_ENTRY_DISABLED      0x00000000
		#define SHARED_HW_CFG_PREVENT_L1_ENTRY_ENABLED       0x00800000


	/*  Set the MDC/MDIO access for the first external phy */
	#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_MASK         0x1C000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_SHIFT         26
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_PHY_TYPE      0x00000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_EMAC0         0x04000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_EMAC1         0x08000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_BOTH          0x0c000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS1_SWAPPED       0x10000000

	/*  Set the MDC/MDIO access for the second external phy */
	#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_MASK         0xE0000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_SHIFT         29
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_PHY_TYPE      0x00000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_EMAC0         0x20000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_EMAC1         0x40000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_BOTH          0x60000000
		#define SHARED_HW_CFG_MDC_MDIO_ACCESS2_SWAPPED       0x80000000

	/*  Max number of PF MSIX vectors */
	uint32_t config_3;                                       /* 0x11C */
	#define SHARED_HW_CFG_PF_MSIX_MAX_NUM_MASK                    0x0000007F
	#define SHARED_HW_CFG_PF_MSIX_MAX_NUM_SHIFT                   0

	uint32_t ump_nc_si_config;			/* 0x120 */
	#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_MASK       0x00000003
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_SHIFT       0
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_MAC         0x00000000
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_PHY         0x00000001
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_MII         0x00000000
		#define SHARED_HW_CFG_UMP_NC_SI_MII_MODE_RMII        0x00000002

	/* Reserved bits: 226-230 */

	/*  The output pin template BSC_SEL which selects the I2C for this
	port in the I2C Mux */
	uint32_t board;			/* 0x124 */
	#define SHARED_HW_CFG_E3_I2C_MUX0_MASK              0x0000003F
	    #define SHARED_HW_CFG_E3_I2C_MUX0_SHIFT              0

	#define SHARED_HW_CFG_E3_I2C_MUX1_MASK              0x00000FC0
	#define SHARED_HW_CFG_E3_I2C_MUX1_SHIFT                      6
	/* Use the PIN_CFG_XXX defines on top */
	#define SHARED_HW_CFG_BOARD_REV_MASK                0x00FF0000
	#define SHARED_HW_CFG_BOARD_REV_SHIFT                        16

	#define SHARED_HW_CFG_BOARD_MAJOR_VER_MASK          0x0F000000
	#define SHARED_HW_CFG_BOARD_MAJOR_VER_SHIFT                  24

	#define SHARED_HW_CFG_BOARD_MINOR_VER_MASK          0xF0000000
	#define SHARED_HW_CFG_BOARD_MINOR_VER_SHIFT                  28

	uint32_t wc_lane_config;				    /* 0x128 */
	#define SHARED_HW_CFG_LANE_SWAP_CFG_MASK            0x0000FFFF
		#define SHARED_HW_CFG_LANE_SWAP_CFG_SHIFT            0
		#define SHARED_HW_CFG_LANE_SWAP_CFG_32103210         0x00001b1b
		#define SHARED_HW_CFG_LANE_SWAP_CFG_32100123         0x00001be4
		#define SHARED_HW_CFG_LANE_SWAP_CFG_31200213         0x000027d8
		#define SHARED_HW_CFG_LANE_SWAP_CFG_02133120         0x0000d827
		#define SHARED_HW_CFG_LANE_SWAP_CFG_01233210         0x0000e41b
		#define SHARED_HW_CFG_LANE_SWAP_CFG_01230123         0x0000e4e4
	#define SHARED_HW_CFG_LANE_SWAP_CFG_TX_MASK         0x000000FF
	#define SHARED_HW_CFG_LANE_SWAP_CFG_TX_SHIFT                 0
	#define SHARED_HW_CFG_LANE_SWAP_CFG_RX_MASK         0x0000FF00
	#define SHARED_HW_CFG_LANE_SWAP_CFG_RX_SHIFT                 8

	/* TX lane Polarity swap */
	#define SHARED_HW_CFG_TX_LANE0_POL_FLIP_ENABLED     0x00010000
	#define SHARED_HW_CFG_TX_LANE1_POL_FLIP_ENABLED     0x00020000
	#define SHARED_HW_CFG_TX_LANE2_POL_FLIP_ENABLED     0x00040000
	#define SHARED_HW_CFG_TX_LANE3_POL_FLIP_ENABLED     0x00080000
	/* TX lane Polarity swap */
	#define SHARED_HW_CFG_RX_LANE0_POL_FLIP_ENABLED     0x00100000
	#define SHARED_HW_CFG_RX_LANE1_POL_FLIP_ENABLED     0x00200000
	#define SHARED_HW_CFG_RX_LANE2_POL_FLIP_ENABLED     0x00400000
	#define SHARED_HW_CFG_RX_LANE3_POL_FLIP_ENABLED     0x00800000

	/*  Selects the port layout of the board */
	#define SHARED_HW_CFG_E3_PORT_LAYOUT_MASK           0x0F000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_SHIFT           24
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_2P_01           0x00000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_2P_10           0x01000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_4P_0123         0x02000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_4P_1032         0x03000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_4P_2301         0x04000000
		#define SHARED_HW_CFG_E3_PORT_LAYOUT_4P_3210         0x05000000
};


/****************************************************************************
 * Port HW configuration                                                    *
 ****************************************************************************/
struct port_hw_cfg {		    /* port 0: 0x12c  port 1: 0x2bc */

	uint32_t pci_id;
	#define PORT_HW_CFG_PCI_DEVICE_ID_MASK              0x0000FFFF
	#define PORT_HW_CFG_PCI_DEVICE_ID_SHIFT             0

	#define PORT_HW_CFG_PCI_VENDOR_ID_MASK              0xFFFF0000
	#define PORT_HW_CFG_PCI_VENDOR_ID_SHIFT             16

	uint32_t pci_sub_id;
	#define PORT_HW_CFG_PCI_SUBSYS_VENDOR_ID_MASK       0x0000FFFF
	#define PORT_HW_CFG_PCI_SUBSYS_VENDOR_ID_SHIFT      0

	#define PORT_HW_CFG_PCI_SUBSYS_DEVICE_ID_MASK       0xFFFF0000
	#define PORT_HW_CFG_PCI_SUBSYS_DEVICE_ID_SHIFT      16

	uint32_t power_dissipated;
	#define PORT_HW_CFG_POWER_DIS_D0_MASK               0x000000FF
	#define PORT_HW_CFG_POWER_DIS_D0_SHIFT                       0
	#define PORT_HW_CFG_POWER_DIS_D1_MASK               0x0000FF00
	#define PORT_HW_CFG_POWER_DIS_D1_SHIFT                       8
	#define PORT_HW_CFG_POWER_DIS_D2_MASK               0x00FF0000
	#define PORT_HW_CFG_POWER_DIS_D2_SHIFT                       16
	#define PORT_HW_CFG_POWER_DIS_D3_MASK               0xFF000000
	#define PORT_HW_CFG_POWER_DIS_D3_SHIFT                       24

	uint32_t power_consumed;
	#define PORT_HW_CFG_POWER_CONS_D0_MASK              0x000000FF
	#define PORT_HW_CFG_POWER_CONS_D0_SHIFT                      0
	#define PORT_HW_CFG_POWER_CONS_D1_MASK              0x0000FF00
	#define PORT_HW_CFG_POWER_CONS_D1_SHIFT                      8
	#define PORT_HW_CFG_POWER_CONS_D2_MASK              0x00FF0000
	#define PORT_HW_CFG_POWER_CONS_D2_SHIFT                      16
	#define PORT_HW_CFG_POWER_CONS_D3_MASK              0xFF000000
	#define PORT_HW_CFG_POWER_CONS_D3_SHIFT                      24

	uint32_t mac_upper;
	uint32_t mac_lower;                                      /* 0x140 */
	#define PORT_HW_CFG_UPPERMAC_MASK                   0x0000FFFF
	#define PORT_HW_CFG_UPPERMAC_SHIFT                           0


	uint32_t iscsi_mac_upper;  /* Upper 16 bits are always zeroes */
	uint32_t iscsi_mac_lower;

	uint32_t rdma_mac_upper;   /* Upper 16 bits are always zeroes */
	uint32_t rdma_mac_lower;

	uint32_t serdes_config;
	#define PORT_HW_CFG_SERDES_TX_DRV_PRE_EMPHASIS_MASK 0x0000FFFF
	#define PORT_HW_CFG_SERDES_TX_DRV_PRE_EMPHASIS_SHIFT         0

	#define PORT_HW_CFG_SERDES_RX_DRV_EQUALIZER_MASK    0xFFFF0000
	#define PORT_HW_CFG_SERDES_RX_DRV_EQUALIZER_SHIFT            16


	/*  Default values: 2P-64, 4P-32 */
	uint32_t reserved;

	uint32_t vf_config;					    /* 0x15C */
	#define PORT_HW_CFG_VF_PCI_DEVICE_ID_MASK           0xFFFF0000
	#define PORT_HW_CFG_VF_PCI_DEVICE_ID_SHIFT                   16

	uint32_t mf_pci_id;					    /* 0x160 */
	#define PORT_HW_CFG_MF_PCI_DEVICE_ID_MASK           0x0000FFFF
	#define PORT_HW_CFG_MF_PCI_DEVICE_ID_SHIFT                   0

	/*  Controls the TX laser of the SFP+ module */
	uint32_t sfp_ctrl;					    /* 0x164 */
	#define PORT_HW_CFG_TX_LASER_MASK                   0x000000FF
		#define PORT_HW_CFG_TX_LASER_SHIFT                   0
		#define PORT_HW_CFG_TX_LASER_MDIO                    0x00000000
		#define PORT_HW_CFG_TX_LASER_GPIO0                   0x00000001
		#define PORT_HW_CFG_TX_LASER_GPIO1                   0x00000002
		#define PORT_HW_CFG_TX_LASER_GPIO2                   0x00000003
		#define PORT_HW_CFG_TX_LASER_GPIO3                   0x00000004

	/*  Controls the fault module LED of the SFP+ */
	#define PORT_HW_CFG_FAULT_MODULE_LED_MASK           0x0000FF00
		#define PORT_HW_CFG_FAULT_MODULE_LED_SHIFT           8
		#define PORT_HW_CFG_FAULT_MODULE_LED_GPIO0           0x00000000
		#define PORT_HW_CFG_FAULT_MODULE_LED_GPIO1           0x00000100
		#define PORT_HW_CFG_FAULT_MODULE_LED_GPIO2           0x00000200
		#define PORT_HW_CFG_FAULT_MODULE_LED_GPIO3           0x00000300
		#define PORT_HW_CFG_FAULT_MODULE_LED_DISABLED        0x00000400

	/*  The output pin TX_DIS that controls the TX laser of the SFP+
	  module. Use the PIN_CFG_XXX defines on top */
	uint32_t e3_sfp_ctrl;				    /* 0x168 */
	#define PORT_HW_CFG_E3_TX_LASER_MASK                0x000000FF
	#define PORT_HW_CFG_E3_TX_LASER_SHIFT                        0

	/*  The output pin for SFPP_TYPE which turns on the Fault module LED */
	#define PORT_HW_CFG_E3_FAULT_MDL_LED_MASK           0x0000FF00
	#define PORT_HW_CFG_E3_FAULT_MDL_LED_SHIFT                   8

	/*  The input pin MOD_ABS that indicates whether SFP+ module is
	  present or not. Use the PIN_CFG_XXX defines on top */
	#define PORT_HW_CFG_E3_MOD_ABS_MASK                 0x00FF0000
	#define PORT_HW_CFG_E3_MOD_ABS_SHIFT                         16

	/*  The output pin PWRDIS_SFP_X which disable the power of the SFP+
	  module. Use the PIN_CFG_XXX defines on top */
	#define PORT_HW_CFG_E3_PWR_DIS_MASK                 0xFF000000
	#define PORT_HW_CFG_E3_PWR_DIS_SHIFT                         24

	/*
	 * The input pin which signals module transmit fault. Use the
	 * PIN_CFG_XXX defines on top
	 */
	uint32_t e3_cmn_pin_cfg;				    /* 0x16C */
	#define PORT_HW_CFG_E3_TX_FAULT_MASK                0x000000FF
	#define PORT_HW_CFG_E3_TX_FAULT_SHIFT                        0

	/*  The output pin which reset the PHY. Use the PIN_CFG_XXX defines on
	 top */
	#define PORT_HW_CFG_E3_PHY_RESET_MASK               0x0000FF00
	#define PORT_HW_CFG_E3_PHY_RESET_SHIFT                       8

	/*
	 * The output pin which powers down the PHY. Use the PIN_CFG_XXX
	 * defines on top
	 */
	#define PORT_HW_CFG_E3_PWR_DOWN_MASK                0x00FF0000
	#define PORT_HW_CFG_E3_PWR_DOWN_SHIFT                        16

	/*  The output pin values BSC_SEL which selects the I2C for this port
	  in the I2C Mux */
	#define PORT_HW_CFG_E3_I2C_MUX0_MASK                0x01000000
	#define PORT_HW_CFG_E3_I2C_MUX1_MASK                0x02000000


	/*
	 * The input pin I_FAULT which indicate over-current has occurred.
	 * Use the PIN_CFG_XXX defines on top
	 */
	uint32_t e3_cmn_pin_cfg1;				    /* 0x170 */
	#define PORT_HW_CFG_E3_OVER_CURRENT_MASK            0x000000FF
	#define PORT_HW_CFG_E3_OVER_CURRENT_SHIFT                    0

	/*  pause on host ring */
	uint32_t generic_features;                               /* 0x174 */
	#define PORT_HW_CFG_PAUSE_ON_HOST_RING_MASK                   0x00000001
	#define PORT_HW_CFG_PAUSE_ON_HOST_RING_SHIFT                  0
	#define PORT_HW_CFG_PAUSE_ON_HOST_RING_DISABLED               0x00000000
	#define PORT_HW_CFG_PAUSE_ON_HOST_RING_ENABLED                0x00000001

	/* SFP+ Tx Equalization: NIC recommended and tested value is 0xBEB2
	 * LOM recommended and tested value is 0xBEB2. Using a different
	 * value means using a value not tested by BRCM
	 */
	uint32_t sfi_tap_values;                                 /* 0x178 */
	#define PORT_HW_CFG_TX_EQUALIZATION_MASK                      0x0000FFFF
	#define PORT_HW_CFG_TX_EQUALIZATION_SHIFT                     0

	/* SFP+ Tx driver broadcast IDRIVER: NIC recommended and tested
	 * value is 0x2. LOM recommended and tested value is 0x2. Using a
	 * different value means using a value not tested by BRCM
	 */
	#define PORT_HW_CFG_TX_DRV_BROADCAST_MASK                     0x000F0000
	#define PORT_HW_CFG_TX_DRV_BROADCAST_SHIFT                    16

	uint32_t reserved0[5];				    /* 0x17c */

	uint32_t aeu_int_mask;				    /* 0x190 */

	uint32_t media_type;					    /* 0x194 */
	#define PORT_HW_CFG_MEDIA_TYPE_PHY0_MASK            0x000000FF
	#define PORT_HW_CFG_MEDIA_TYPE_PHY0_SHIFT                    0

	#define PORT_HW_CFG_MEDIA_TYPE_PHY1_MASK            0x0000FF00
	#define PORT_HW_CFG_MEDIA_TYPE_PHY1_SHIFT                    8

	#define PORT_HW_CFG_MEDIA_TYPE_PHY2_MASK            0x00FF0000
	#define PORT_HW_CFG_MEDIA_TYPE_PHY2_SHIFT                    16

	/*  4 times 16 bits for all 4 lanes. In case external PHY is present
	      (not direct mode), those values will not take effect on the 4 XGXS
	      lanes. For some external PHYs (such as 8706 and 8726) the values
	      will be used to configure the external PHY  in those cases, not
	      all 4 values are needed. */
	uint16_t xgxs_config_rx[4];			/* 0x198 */
	uint16_t xgxs_config_tx[4];			/* 0x1A0 */


	/* For storing FCOE mac on shared memory */
	uint32_t fcoe_fip_mac_upper;
	#define PORT_HW_CFG_FCOE_UPPERMAC_MASK              0x0000ffff
	#define PORT_HW_CFG_FCOE_UPPERMAC_SHIFT                      0
	uint32_t fcoe_fip_mac_lower;

	uint32_t fcoe_wwn_port_name_upper;
	uint32_t fcoe_wwn_port_name_lower;

	uint32_t fcoe_wwn_node_name_upper;
	uint32_t fcoe_wwn_node_name_lower;

	/*  wwpn for npiv enabled */
	uint32_t wwpn_for_npiv_config;                           /* 0x1C0 */
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ENABLED_MASK                0x00000001
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ENABLED_SHIFT               0
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ENABLED_DISABLED            0x00000000
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ENABLED_ENABLED             0x00000001

	/*  wwpn for npiv valid addresses */
	uint32_t wwpn_for_npiv_valid_addresses;                  /* 0x1C4 */
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ADDRESS_BITMAP_MASK         0x0000FFFF
	#define PORT_HW_CFG_WWPN_FOR_NPIV_ADDRESS_BITMAP_SHIFT        0

	struct mac_addr wwpn_for_niv_macs[16];

	/* Reserved bits: 2272-2336 For storing FCOE mac on shared memory */
	uint32_t Reserved1[14];

	uint32_t pf_allocation;                                  /* 0x280 */
	/* number of vfs per PF, if 0 - sriov disabled */
	#define PORT_HW_CFG_NUMBER_OF_VFS_MASK                        0x000000FF
	#define PORT_HW_CFG_NUMBER_OF_VFS_SHIFT                       0

	/*  Enable RJ45 magjack pair swapping on 10GBase-T PHY (0=default),
	      84833 only */
	uint32_t xgbt_phy_cfg;				    /* 0x284 */
	#define PORT_HW_CFG_RJ45_PAIR_SWAP_MASK             0x000000FF
	#define PORT_HW_CFG_RJ45_PAIR_SWAP_SHIFT                     0

		uint32_t default_cfg;			    /* 0x288 */
	#define PORT_HW_CFG_GPIO0_CONFIG_MASK               0x00000003
		#define PORT_HW_CFG_GPIO0_CONFIG_SHIFT               0
		#define PORT_HW_CFG_GPIO0_CONFIG_NA                  0x00000000
		#define PORT_HW_CFG_GPIO0_CONFIG_LOW                 0x00000001
		#define PORT_HW_CFG_GPIO0_CONFIG_HIGH                0x00000002
		#define PORT_HW_CFG_GPIO0_CONFIG_INPUT               0x00000003

	#define PORT_HW_CFG_GPIO1_CONFIG_MASK               0x0000000C
		#define PORT_HW_CFG_GPIO1_CONFIG_SHIFT               2
		#define PORT_HW_CFG_GPIO1_CONFIG_NA                  0x00000000
		#define PORT_HW_CFG_GPIO1_CONFIG_LOW                 0x00000004
		#define PORT_HW_CFG_GPIO1_CONFIG_HIGH                0x00000008
		#define PORT_HW_CFG_GPIO1_CONFIG_INPUT               0x0000000c

	#define PORT_HW_CFG_GPIO2_CONFIG_MASK               0x00000030
		#define PORT_HW_CFG_GPIO2_CONFIG_SHIFT               4
		#define PORT_HW_CFG_GPIO2_CONFIG_NA                  0x00000000
		#define PORT_HW_CFG_GPIO2_CONFIG_LOW                 0x00000010
		#define PORT_HW_CFG_GPIO2_CONFIG_HIGH                0x00000020
		#define PORT_HW_CFG_GPIO2_CONFIG_INPUT               0x00000030

	#define PORT_HW_CFG_GPIO3_CONFIG_MASK               0x000000C0
		#define PORT_HW_CFG_GPIO3_CONFIG_SHIFT               6
		#define PORT_HW_CFG_GPIO3_CONFIG_NA                  0x00000000
		#define PORT_HW_CFG_GPIO3_CONFIG_LOW                 0x00000040
		#define PORT_HW_CFG_GPIO3_CONFIG_HIGH                0x00000080
		#define PORT_HW_CFG_GPIO3_CONFIG_INPUT               0x000000c0

	/*  When KR link is required to be set to force which is not
	      KR-compliant, this parameter determine what is the trigger for it.
	      When GPIO is selected, low input will force the speed. Currently
	      default speed is 1G. In the future, it may be widen to select the
	      forced speed in with another parameter. Note when force-1G is
	      enabled, it override option 56: Link Speed option. */
	#define PORT_HW_CFG_FORCE_KR_ENABLER_MASK           0x00000F00
		#define PORT_HW_CFG_FORCE_KR_ENABLER_SHIFT           8
		#define PORT_HW_CFG_FORCE_KR_ENABLER_NOT_FORCED      0x00000000
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO0_P0        0x00000100
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO1_P0        0x00000200
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO2_P0        0x00000300
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO3_P0        0x00000400
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO0_P1        0x00000500
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO1_P1        0x00000600
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO2_P1        0x00000700
		#define PORT_HW_CFG_FORCE_KR_ENABLER_GPIO3_P1        0x00000800
		#define PORT_HW_CFG_FORCE_KR_ENABLER_FORCED          0x00000900
	/*  Enable to determine with which GPIO to reset the external phy */
	#define PORT_HW_CFG_EXT_PHY_GPIO_RST_MASK           0x000F0000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_SHIFT           16
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_PHY_TYPE        0x00000000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO0_P0        0x00010000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO1_P0        0x00020000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO2_P0        0x00030000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO3_P0        0x00040000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO0_P1        0x00050000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO1_P1        0x00060000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO2_P1        0x00070000
		#define PORT_HW_CFG_EXT_PHY_GPIO_RST_GPIO3_P1        0x00080000

	/*  Enable BAM on KR */
	#define PORT_HW_CFG_ENABLE_BAM_ON_KR_MASK           0x00100000
	#define PORT_HW_CFG_ENABLE_BAM_ON_KR_SHIFT                   20
	#define PORT_HW_CFG_ENABLE_BAM_ON_KR_DISABLED                0x00000000
	#define PORT_HW_CFG_ENABLE_BAM_ON_KR_ENABLED                 0x00100000

	/*  Enable Common Mode Sense */
	#define PORT_HW_CFG_ENABLE_CMS_MASK                 0x00200000
	#define PORT_HW_CFG_ENABLE_CMS_SHIFT                         21
	#define PORT_HW_CFG_ENABLE_CMS_DISABLED                      0x00000000
	#define PORT_HW_CFG_ENABLE_CMS_ENABLED                       0x00200000

	/*  Determine the Serdes electrical interface   */
	#define PORT_HW_CFG_NET_SERDES_IF_MASK              0x0F000000
	#define PORT_HW_CFG_NET_SERDES_IF_SHIFT                      24
	#define PORT_HW_CFG_NET_SERDES_IF_SGMII                      0x00000000
	#define PORT_HW_CFG_NET_SERDES_IF_XFI                        0x01000000
	#define PORT_HW_CFG_NET_SERDES_IF_SFI                        0x02000000
	#define PORT_HW_CFG_NET_SERDES_IF_KR                         0x03000000
	#define PORT_HW_CFG_NET_SERDES_IF_DXGXS                      0x04000000
	#define PORT_HW_CFG_NET_SERDES_IF_KR2                        0x05000000

	/*  SFP+ main TAP and post TAP volumes */
	#define PORT_HW_CFG_TAP_LEVELS_MASK                           0x70000000
	#define PORT_HW_CFG_TAP_LEVELS_SHIFT                          28
	#define PORT_HW_CFG_TAP_LEVELS_POST_15_MAIN_43                0x00000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_14_MAIN_44                0x10000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_13_MAIN_45                0x20000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_12_MAIN_46                0x30000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_11_MAIN_47                0x40000000
	#define PORT_HW_CFG_TAP_LEVELS_POST_10_MAIN_48                0x50000000

	uint32_t speed_capability_mask2;			    /* 0x28C */
	#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_MASK       0x0000FFFF
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_SHIFT       0
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_10M_FULL    0x00000001
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_10M_HALF    0x00000002
	    #define PORT_HW_CFG_SPEED_CAPABILITY2_D3_100M_HALF   0x00000004
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_100M_FULL   0x00000008
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_1G          0x00000010
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_2_5G        0x00000020
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_10G         0x00000040
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D3_20G         0x00000080

	#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_MASK       0xFFFF0000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_SHIFT       16
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_10M_FULL    0x00010000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_10M_HALF    0x00020000
	    #define PORT_HW_CFG_SPEED_CAPABILITY2_D0_100M_HALF   0x00040000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_100M_FULL   0x00080000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_1G          0x00100000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_2_5G        0x00200000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_10G         0x00400000
		#define PORT_HW_CFG_SPEED_CAPABILITY2_D0_20G         0x00800000


	/*  In the case where two media types (e.g. copper and fiber) are
	      present and electrically active at the same time, PHY Selection
	      will determine which of the two PHYs will be designated as the
	      Active PHY and used for a connection to the network.  */
	uint32_t multi_phy_config;				    /* 0x290 */
	#define PORT_HW_CFG_PHY_SELECTION_MASK              0x00000007
		#define PORT_HW_CFG_PHY_SELECTION_SHIFT              0
		#define PORT_HW_CFG_PHY_SELECTION_HARDWARE_DEFAULT   0x00000000
		#define PORT_HW_CFG_PHY_SELECTION_FIRST_PHY          0x00000001
		#define PORT_HW_CFG_PHY_SELECTION_SECOND_PHY         0x00000002
		#define PORT_HW_CFG_PHY_SELECTION_FIRST_PHY_PRIORITY 0x00000003
		#define PORT_HW_CFG_PHY_SELECTION_SECOND_PHY_PRIORITY 0x00000004

	/*  When enabled, all second phy nvram parameters will be swapped
	      with the first phy parameters */
	#define PORT_HW_CFG_PHY_SWAPPED_MASK                0x00000008
		#define PORT_HW_CFG_PHY_SWAPPED_SHIFT                3
		#define PORT_HW_CFG_PHY_SWAPPED_DISABLED             0x00000000
		#define PORT_HW_CFG_PHY_SWAPPED_ENABLED              0x00000008


	/*  Address of the second external phy */
	uint32_t external_phy_config2;			    /* 0x294 */
	#define PORT_HW_CFG_XGXS_EXT_PHY2_ADDR_MASK         0x000000FF
	#define PORT_HW_CFG_XGXS_EXT_PHY2_ADDR_SHIFT                 0

	/*  The second XGXS external PHY type */
	#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_MASK         0x0000FF00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_SHIFT         8
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_DIRECT        0x00000000
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8071       0x00000100
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8072       0x00000200
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8073       0x00000300
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8705       0x00000400
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8706       0x00000500
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8726       0x00000600
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8481       0x00000700
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_SFX7101       0x00000800
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8727       0x00000900
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8727_NOC   0x00000a00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM84823      0x00000b00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM54640      0x00000c00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM84833      0x00000d00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM54618SE    0x00000e00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM8722       0x00000f00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM54616      0x00001000
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_BCM84834      0x00001100
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_FAILURE       0x0000fd00
		#define PORT_HW_CFG_XGXS_EXT_PHY2_TYPE_NOT_CONN      0x0000ff00


	/*  4 times 16 bits for all 4 lanes. For some external PHYs (such as
	      8706, 8726 and 8727) not all 4 values are needed. */
	uint16_t xgxs_config2_rx[4];				    /* 0x296 */
	uint16_t xgxs_config2_tx[4];				    /* 0x2A0 */

	uint32_t lane_config;
	#define PORT_HW_CFG_LANE_SWAP_CFG_MASK              0x0000FFFF
		#define PORT_HW_CFG_LANE_SWAP_CFG_SHIFT              0
		/* AN and forced */
		#define PORT_HW_CFG_LANE_SWAP_CFG_01230123           0x00001b1b
		/* forced only */
		#define PORT_HW_CFG_LANE_SWAP_CFG_01233210           0x00001be4
		/* forced only */
		#define PORT_HW_CFG_LANE_SWAP_CFG_31203120           0x0000d8d8
		/* forced only */
		#define PORT_HW_CFG_LANE_SWAP_CFG_32103210           0x0000e4e4
	#define PORT_HW_CFG_LANE_SWAP_CFG_TX_MASK           0x000000FF
	#define PORT_HW_CFG_LANE_SWAP_CFG_TX_SHIFT                   0
	#define PORT_HW_CFG_LANE_SWAP_CFG_RX_MASK           0x0000FF00
	#define PORT_HW_CFG_LANE_SWAP_CFG_RX_SHIFT                   8
	#define PORT_HW_CFG_LANE_SWAP_CFG_MASTER_MASK       0x0000C000
	#define PORT_HW_CFG_LANE_SWAP_CFG_MASTER_SHIFT               14

	/*  Indicate whether to swap the external phy polarity */
	#define PORT_HW_CFG_SWAP_PHY_POLARITY_MASK          0x00010000
		#define PORT_HW_CFG_SWAP_PHY_POLARITY_DISABLED       0x00000000
		#define PORT_HW_CFG_SWAP_PHY_POLARITY_ENABLED        0x00010000


	uint32_t external_phy_config;
	#define PORT_HW_CFG_XGXS_EXT_PHY_ADDR_MASK          0x000000FF
	#define PORT_HW_CFG_XGXS_EXT_PHY_ADDR_SHIFT                  0

	#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_MASK          0x0000FF00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SHIFT          8
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT         0x00000000
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8071        0x00000100
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8072        0x00000200
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8073        0x00000300
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8705        0x00000400
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8706        0x00000500
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8726        0x00000600
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8481        0x00000700
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_SFX7101        0x00000800
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727        0x00000900
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8727_NOC    0x00000a00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84823       0x00000b00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM54640       0x00000c00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84833       0x00000d00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM54618SE     0x00000e00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM8722        0x00000f00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM54616       0x00001000
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_BCM84834       0x00001100
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_DIRECT_WC      0x0000fc00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_FAILURE        0x0000fd00
		#define PORT_HW_CFG_XGXS_EXT_PHY_TYPE_NOT_CONN       0x0000ff00

	#define PORT_HW_CFG_SERDES_EXT_PHY_ADDR_MASK        0x00FF0000
	#define PORT_HW_CFG_SERDES_EXT_PHY_ADDR_SHIFT                16

	#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_MASK        0xFF000000
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_SHIFT        24
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT       0x00000000
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_BCM5482      0x01000000
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_DIRECT_SD    0x02000000
		#define PORT_HW_CFG_SERDES_EXT_PHY_TYPE_NOT_CONN     0xff000000

	uint32_t speed_capability_mask;
	#define PORT_HW_CFG_SPEED_CAPABILITY_D3_MASK        0x0000FFFF
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_SHIFT        0
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_10M_FULL     0x00000001
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_10M_HALF     0x00000002
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_100M_HALF    0x00000004
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_100M_FULL    0x00000008
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_1G           0x00000010
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_2_5G         0x00000020
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_10G          0x00000040
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_20G          0x00000080
		#define PORT_HW_CFG_SPEED_CAPABILITY_D3_RESERVED     0x0000f000

	#define PORT_HW_CFG_SPEED_CAPABILITY_D0_MASK        0xFFFF0000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_SHIFT        16
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_FULL     0x00010000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_10M_HALF     0x00020000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_HALF    0x00040000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_100M_FULL    0x00080000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_1G           0x00100000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_2_5G         0x00200000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_10G          0x00400000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_20G          0x00800000
		#define PORT_HW_CFG_SPEED_CAPABILITY_D0_RESERVED     0xf0000000

	/*  A place to hold the original MAC address as a backup */
	uint32_t backup_mac_upper;			/* 0x2B4 */
	uint32_t backup_mac_lower;			/* 0x2B8 */

};


/****************************************************************************
 * Shared Feature configuration                                             *
 ****************************************************************************/
struct shared_feat_cfg {		 /* NVRAM Offset */

	uint32_t config;			/* 0x450 */
	#define SHARED_FEATURE_BMC_ECHO_MODE_EN             0x00000001

	/* Use NVRAM values instead of HW default values */
	#define SHARED_FEAT_CFG_OVERRIDE_PREEMPHASIS_CFG_MASK \
							    0x00000002
		#define SHARED_FEAT_CFG_OVERRIDE_PREEMPHASIS_CFG_DISABLED \
								     0x00000000
		#define SHARED_FEAT_CFG_OVERRIDE_PREEMPHASIS_CFG_ENABLED \
								     0x00000002

	#define SHARED_FEAT_CFG_NCSI_ID_METHOD_MASK         0x00000008
		#define SHARED_FEAT_CFG_NCSI_ID_METHOD_SPIO          0x00000000
		#define SHARED_FEAT_CFG_NCSI_ID_METHOD_NVRAM         0x00000008

	#define SHARED_FEAT_CFG_NCSI_ID_MASK                0x00000030
	#define SHARED_FEAT_CFG_NCSI_ID_SHIFT                        4

	/*  Override the OTP back to single function mode. When using GPIO,
	      high means only SF, 0 is according to CLP configuration */
	#define SHARED_FEAT_CFG_FORCE_SF_MODE_MASK          0x00000700
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_SHIFT          8
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_MF_ALLOWED     0x00000000
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_FORCED_SF      0x00000100
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_SPIO4          0x00000200
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_SWITCH_INDEPT  0x00000300
		#define SHARED_FEAT_CFG_FORCE_SF_MODE_AFEX_MODE      0x00000400

	/*  Act as if the FCoE license is invalid */
	#define SHARED_FEAT_CFG_PREVENT_FCOE                0x00001000

    /*  Force FLR capability to all ports */
	#define SHARED_FEAT_CFG_FORCE_FLR_CAPABILITY        0x00002000

	/*  Act as if the iSCSI license is invalid */
	#define SHARED_FEAT_CFG_PREVENT_ISCSI_MASK                    0x00004000
	#define SHARED_FEAT_CFG_PREVENT_ISCSI_SHIFT                   14
	#define SHARED_FEAT_CFG_PREVENT_ISCSI_DISABLED                0x00000000
	#define SHARED_FEAT_CFG_PREVENT_ISCSI_ENABLED                 0x00004000

	/* The interval in seconds between sending LLDP packets. Set to zero
	   to disable the feature */
	#define SHARED_FEAT_CFG_LLDP_XMIT_INTERVAL_MASK     0x00FF0000
	#define SHARED_FEAT_CFG_LLDP_XMIT_INTERVAL_SHIFT             16

	/* The assigned device type ID for LLDP usage */
	#define SHARED_FEAT_CFG_LLDP_DEVICE_TYPE_ID_MASK    0xFF000000
	#define SHARED_FEAT_CFG_LLDP_DEVICE_TYPE_ID_SHIFT            24

};


/****************************************************************************
 * Port Feature configuration                                               *
 ****************************************************************************/
struct port_feat_cfg {		    /* port 0: 0x454  port 1: 0x4c8 */

	uint32_t config;
	#define PORT_FEAT_CFG_BAR1_SIZE_MASK                 0x0000000F
		#define PORT_FEAT_CFG_BAR1_SIZE_SHIFT                 0
		#define PORT_FEAT_CFG_BAR1_SIZE_DISABLED              0x00000000
		#define PORT_FEAT_CFG_BAR1_SIZE_64K                   0x00000001
		#define PORT_FEAT_CFG_BAR1_SIZE_128K                  0x00000002
		#define PORT_FEAT_CFG_BAR1_SIZE_256K                  0x00000003
		#define PORT_FEAT_CFG_BAR1_SIZE_512K                  0x00000004
		#define PORT_FEAT_CFG_BAR1_SIZE_1M                    0x00000005
		#define PORT_FEAT_CFG_BAR1_SIZE_2M                    0x00000006
		#define PORT_FEAT_CFG_BAR1_SIZE_4M                    0x00000007
		#define PORT_FEAT_CFG_BAR1_SIZE_8M                    0x00000008
		#define PORT_FEAT_CFG_BAR1_SIZE_16M                   0x00000009
		#define PORT_FEAT_CFG_BAR1_SIZE_32M                   0x0000000a
		#define PORT_FEAT_CFG_BAR1_SIZE_64M                   0x0000000b
		#define PORT_FEAT_CFG_BAR1_SIZE_128M                  0x0000000c
		#define PORT_FEAT_CFG_BAR1_SIZE_256M                  0x0000000d
		#define PORT_FEAT_CFG_BAR1_SIZE_512M                  0x0000000e
		#define PORT_FEAT_CFG_BAR1_SIZE_1G                    0x0000000f
	#define PORT_FEAT_CFG_BAR2_SIZE_MASK                 0x000000F0
		#define PORT_FEAT_CFG_BAR2_SIZE_SHIFT                 4
		#define PORT_FEAT_CFG_BAR2_SIZE_DISABLED              0x00000000
		#define PORT_FEAT_CFG_BAR2_SIZE_64K                   0x00000010
		#define PORT_FEAT_CFG_BAR2_SIZE_128K                  0x00000020
		#define PORT_FEAT_CFG_BAR2_SIZE_256K                  0x00000030
		#define PORT_FEAT_CFG_BAR2_SIZE_512K                  0x00000040
		#define PORT_FEAT_CFG_BAR2_SIZE_1M                    0x00000050
		#define PORT_FEAT_CFG_BAR2_SIZE_2M                    0x00000060
		#define PORT_FEAT_CFG_BAR2_SIZE_4M                    0x00000070
		#define PORT_FEAT_CFG_BAR2_SIZE_8M                    0x00000080
		#define PORT_FEAT_CFG_BAR2_SIZE_16M                   0x00000090
		#define PORT_FEAT_CFG_BAR2_SIZE_32M                   0x000000a0
		#define PORT_FEAT_CFG_BAR2_SIZE_64M                   0x000000b0
		#define PORT_FEAT_CFG_BAR2_SIZE_128M                  0x000000c0
		#define PORT_FEAT_CFG_BAR2_SIZE_256M                  0x000000d0
		#define PORT_FEAT_CFG_BAR2_SIZE_512M                  0x000000e0
		#define PORT_FEAT_CFG_BAR2_SIZE_1G                    0x000000f0

	#define PORT_FEAT_CFG_DCBX_MASK                     0x00000100
		#define PORT_FEAT_CFG_DCBX_DISABLED                  0x00000000
		#define PORT_FEAT_CFG_DCBX_ENABLED                   0x00000100

    #define PORT_FEAT_CFG_AUTOGREEEN_MASK               0x00000200
	    #define PORT_FEAT_CFG_AUTOGREEEN_SHIFT               9
	    #define PORT_FEAT_CFG_AUTOGREEEN_DISABLED            0x00000000
	    #define PORT_FEAT_CFG_AUTOGREEEN_ENABLED             0x00000200

	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_MASK                0x00000C00
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_SHIFT               10
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_DEFAULT             0x00000000
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_FCOE                0x00000400
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_ISCSI               0x00000800
	#define PORT_FEAT_CFG_STORAGE_PERSONALITY_BOTH                0x00000c00

	#define PORT_FEATURE_EN_SIZE_MASK                   0x0f000000
	#define PORT_FEATURE_EN_SIZE_SHIFT                       24
	#define PORT_FEATURE_WOL_ENABLED                         0x01000000
	#define PORT_FEATURE_MBA_ENABLED                         0x02000000
	#define PORT_FEATURE_MFW_ENABLED                         0x04000000

	/* Advertise expansion ROM even if MBA is disabled */
	#define PORT_FEAT_CFG_FORCE_EXP_ROM_ADV_MASK        0x08000000
		#define PORT_FEAT_CFG_FORCE_EXP_ROM_ADV_DISABLED     0x00000000
		#define PORT_FEAT_CFG_FORCE_EXP_ROM_ADV_ENABLED      0x08000000

	/* Check the optic vendor via i2c against a list of approved modules
	   in a separate nvram image */
	#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_MASK         0xE0000000
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_SHIFT         29
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_NO_ENFORCEMENT \
								     0x00000000
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_DISABLE_TX_LASER \
								     0x20000000
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_WARNING_MSG   0x40000000
		#define PORT_FEAT_CFG_OPT_MDL_ENFRCMNT_POWER_DOWN    0x60000000

	uint32_t wol_config;
	/* Default is used when driver sets to "auto" mode */
	#define PORT_FEATURE_WOL_ACPI_UPON_MGMT             0x00000010

	uint32_t mba_config;
	#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_MASK       0x00000007
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_SHIFT       0
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_PXE         0x00000000
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_RPL         0x00000001
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_BOOTP       0x00000002
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_ISCSIB      0x00000003
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_FCOE_BOOT   0x00000004
		#define PORT_FEATURE_MBA_BOOT_AGENT_TYPE_NONE        0x00000007

	#define PORT_FEATURE_MBA_BOOT_RETRY_MASK            0x00000038
	#define PORT_FEATURE_MBA_BOOT_RETRY_SHIFT                    3

    #define PORT_FEATURE_MBA_SETUP_PROMPT_ENABLE        0x00000400
	#define PORT_FEATURE_MBA_HOTKEY_MASK                0x00000800
		#define PORT_FEATURE_MBA_HOTKEY_CTRL_S               0x00000000
		#define PORT_FEATURE_MBA_HOTKEY_CTRL_B               0x00000800

	#define PORT_FEATURE_MBA_EXP_ROM_SIZE_MASK          0x000FF000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_SHIFT          12
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_DISABLED       0x00000000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_2K             0x00001000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_4K             0x00002000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_8K             0x00003000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_16K            0x00004000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_32K            0x00005000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_64K            0x00006000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_128K           0x00007000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_256K           0x00008000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_512K           0x00009000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_1M             0x0000a000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_2M             0x0000b000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_4M             0x0000c000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_8M             0x0000d000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_16M            0x0000e000
		#define PORT_FEATURE_MBA_EXP_ROM_SIZE_32M            0x0000f000
	#define PORT_FEATURE_MBA_MSG_TIMEOUT_MASK           0x00F00000
	#define PORT_FEATURE_MBA_MSG_TIMEOUT_SHIFT                   20
	#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_MASK        0x03000000
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_SHIFT        24
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_AUTO         0x00000000
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_BBS          0x01000000
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_INT18H       0x02000000
		#define PORT_FEATURE_MBA_BIOS_BOOTSTRAP_INT19H       0x03000000
	#define PORT_FEATURE_MBA_LINK_SPEED_MASK            0x3C000000
		#define PORT_FEATURE_MBA_LINK_SPEED_SHIFT            26
		#define PORT_FEATURE_MBA_LINK_SPEED_AUTO             0x00000000
		#define PORT_FEATURE_MBA_LINK_SPEED_10M_HALF         0x04000000
		#define PORT_FEATURE_MBA_LINK_SPEED_10M_FULL         0x08000000
		#define PORT_FEATURE_MBA_LINK_SPEED_100M_HALF        0x0c000000
		#define PORT_FEATURE_MBA_LINK_SPEED_100M_FULL        0x10000000
		#define PORT_FEATURE_MBA_LINK_SPEED_1G               0x14000000
		#define PORT_FEATURE_MBA_LINK_SPEED_2_5G             0x18000000
		#define PORT_FEATURE_MBA_LINK_SPEED_10G              0x1c000000
		#define PORT_FEATURE_MBA_LINK_SPEED_20G              0x20000000

	uint32_t Reserved0;                                      /* 0x460 */

	uint32_t mba_vlan_cfg;
	#define PORT_FEATURE_MBA_VLAN_TAG_MASK              0x0000FFFF
	#define PORT_FEATURE_MBA_VLAN_TAG_SHIFT                      0
	#define PORT_FEATURE_MBA_VLAN_EN                    0x00010000
	#define PORT_FEATUTE_BOFM_CFGD_EN                   0x00020000
	#define PORT_FEATURE_BOFM_CFGD_FTGT                 0x00040000
	#define PORT_FEATURE_BOFM_CFGD_VEN                  0x00080000

	uint32_t Reserved1;
	uint32_t smbus_config;
	#define PORT_FEATURE_SMBUS_ADDR_MASK                0x000000fe
	#define PORT_FEATURE_SMBUS_ADDR_SHIFT                        1

	uint32_t vf_config;
	#define PORT_FEAT_CFG_VF_BAR2_SIZE_MASK             0x0000000F
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_SHIFT             0
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_DISABLED          0x00000000
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_4K                0x00000001
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_8K                0x00000002
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_16K               0x00000003
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_32K               0x00000004
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_64K               0x00000005
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_128K              0x00000006
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_256K              0x00000007
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_512K              0x00000008
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_1M                0x00000009
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_2M                0x0000000a
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_4M                0x0000000b
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_8M                0x0000000c
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_16M               0x0000000d
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_32M               0x0000000e
		#define PORT_FEAT_CFG_VF_BAR2_SIZE_64M               0x0000000f

	uint32_t link_config;    /* Used as HW defaults for the driver */

    #define PORT_FEATURE_FLOW_CONTROL_MASK              0x00000700
		#define PORT_FEATURE_FLOW_CONTROL_SHIFT              8
		#define PORT_FEATURE_FLOW_CONTROL_AUTO               0x00000000
		#define PORT_FEATURE_FLOW_CONTROL_TX                 0x00000100
		#define PORT_FEATURE_FLOW_CONTROL_RX                 0x00000200
		#define PORT_FEATURE_FLOW_CONTROL_BOTH               0x00000300
		#define PORT_FEATURE_FLOW_CONTROL_NONE               0x00000400
		#define PORT_FEATURE_FLOW_CONTROL_SAFC_RX            0x00000500
		#define PORT_FEATURE_FLOW_CONTROL_SAFC_TX            0x00000600
		#define PORT_FEATURE_FLOW_CONTROL_SAFC_BOTH          0x00000700

    #define PORT_FEATURE_LINK_SPEED_MASK                0x000F0000
		#define PORT_FEATURE_LINK_SPEED_SHIFT                16
		#define PORT_FEATURE_LINK_SPEED_AUTO                 0x00000000
		#define PORT_FEATURE_LINK_SPEED_10M_HALF             0x00010000
		#define PORT_FEATURE_LINK_SPEED_10M_FULL             0x00020000
		#define PORT_FEATURE_LINK_SPEED_100M_HALF            0x00030000
		#define PORT_FEATURE_LINK_SPEED_100M_FULL            0x00040000
		#define PORT_FEATURE_LINK_SPEED_1G                   0x00050000
		#define PORT_FEATURE_LINK_SPEED_2_5G                 0x00060000
		#define PORT_FEATURE_LINK_SPEED_10G_CX4              0x00070000
		#define PORT_FEATURE_LINK_SPEED_20G                  0x00080000

	#define PORT_FEATURE_CONNECTED_SWITCH_MASK          0x03000000
		#define PORT_FEATURE_CONNECTED_SWITCH_SHIFT          24
		/* (forced) low speed switch (< 10G) */
		#define PORT_FEATURE_CON_SWITCH_1G_SWITCH            0x00000000
		/* (forced) high speed switch (>= 10G) */
		#define PORT_FEATURE_CON_SWITCH_10G_SWITCH           0x01000000
		#define PORT_FEATURE_CON_SWITCH_AUTO_DETECT          0x02000000
		#define PORT_FEATURE_CON_SWITCH_ONE_TIME_DETECT      0x03000000


	/* The default for MCP link configuration,
	   uses the same defines as link_config */
	uint32_t mfw_wol_link_cfg;

	/* The default for the driver of the second external phy,
	   uses the same defines as link_config */
	uint32_t link_config2;				    /* 0x47C */

	/* The default for MCP of the second external phy,
	   uses the same defines as link_config */
	uint32_t mfw_wol_link_cfg2;				    /* 0x480 */


	/*  EEE power saving mode */
	uint32_t eee_power_mode;                                 /* 0x484 */
	#define PORT_FEAT_CFG_EEE_POWER_MODE_MASK                     0x000000FF
	#define PORT_FEAT_CFG_EEE_POWER_MODE_SHIFT                    0
	#define PORT_FEAT_CFG_EEE_POWER_MODE_DISABLED                 0x00000000
	#define PORT_FEAT_CFG_EEE_POWER_MODE_BALANCED                 0x00000001
	#define PORT_FEAT_CFG_EEE_POWER_MODE_AGGRESSIVE               0x00000002
	#define PORT_FEAT_CFG_EEE_POWER_MODE_LOW_LATENCY              0x00000003


	uint32_t Reserved2[16];                                  /* 0x488 */
};

/****************************************************************************
 * Device Information                                                       *
 ****************************************************************************/
struct shm_dev_info {				/* size */

	uint32_t    bc_rev; /* 8 bits each: major, minor, build */	       /* 4 */

	struct shared_hw_cfg     shared_hw_config;	      /* 40 */

	struct port_hw_cfg       port_hw_config[PORT_MAX];     /* 400*2=800 */

	struct shared_feat_cfg   shared_feature_config;		   /* 4 */

	struct port_feat_cfg     port_feature_config[PORT_MAX];/* 116*2=232 */

};

struct extended_dev_info_shared_cfg {             /* NVRAM OFFSET */

	/*  Threshold in celcius to start using the fan */
	uint32_t temperature_monitor1;                           /* 0x4000 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_THRESH_MASK     0x0000007F
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_THRESH_SHIFT    0

	/*  Threshold in celcius to shut down the board */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_THRESH_MASK    0x00007F00
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_THRESH_SHIFT   8

	/*  EPIO of fan temperature status */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_MASK       0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_SHIFT      16
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_NA         0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO0      0x00010000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO1      0x00020000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO2      0x00030000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO3      0x00040000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO4      0x00050000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO5      0x00060000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO6      0x00070000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO7      0x00080000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO8      0x00090000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO9      0x000a0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO10     0x000b0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO11     0x000c0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO12     0x000d0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO13     0x000e0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO14     0x000f0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO15     0x00100000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO16     0x00110000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO17     0x00120000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO18     0x00130000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO19     0x00140000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO20     0x00150000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO21     0x00160000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO22     0x00170000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO23     0x00180000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO24     0x00190000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO25     0x001a0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO26     0x001b0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO27     0x001c0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO28     0x001d0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO29     0x001e0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO30     0x001f0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_FAN_EPIO_EPIO31     0x00200000

	/*  EPIO of shut down temperature status */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_MASK      0xFF000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_SHIFT     24
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_NA        0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO0     0x01000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO1     0x02000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO2     0x03000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO3     0x04000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO4     0x05000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO5     0x06000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO6     0x07000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO7     0x08000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO8     0x09000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO9     0x0a000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO10    0x0b000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO11    0x0c000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO12    0x0d000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO13    0x0e000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO14    0x0f000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO15    0x10000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO16    0x11000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO17    0x12000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO18    0x13000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO19    0x14000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO20    0x15000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO21    0x16000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO22    0x17000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO23    0x18000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO24    0x19000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO25    0x1a000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO26    0x1b000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO27    0x1c000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO28    0x1d000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO29    0x1e000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO30    0x1f000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SHUT_EPIO_EPIO31    0x20000000


	/*  EPIO of shut down temperature status */
	uint32_t temperature_monitor2;                           /* 0x4004 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_PERIOD_MASK         0x0000FFFF
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_PERIOD_SHIFT        0


	/*  MFW flavor to be used */
	uint32_t mfw_cfg;                                        /* 0x4008 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_MFW_FLAVOR_MASK          0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_MFW_FLAVOR_SHIFT         0
	#define EXTENDED_DEV_INFO_SHARED_CFG_MFW_FLAVOR_NA            0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_MFW_FLAVOR_A             0x00000001

	/*  Should NIC data query remain enabled upon last drv unload */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_EN_LAST_DRV_MASK     0x00000100
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_EN_LAST_DRV_SHIFT    8
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_EN_LAST_DRV_DISABLED 0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OCBB_EN_LAST_DRV_ENABLED  0x00000100

	/*  Hide DCBX feature in CCM/BACS menus */
	#define EXTENDED_DEV_INFO_SHARED_CFG_HIDE_DCBX_FEAT_MASK      0x00010000
	#define EXTENDED_DEV_INFO_SHARED_CFG_HIDE_DCBX_FEAT_SHIFT     16
	#define EXTENDED_DEV_INFO_SHARED_CFG_HIDE_DCBX_FEAT_DISABLED  0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_HIDE_DCBX_FEAT_ENABLED   0x00010000

	uint32_t smbus_config;                                   /* 0x400C */
	#define EXTENDED_DEV_INFO_SHARED_CFG_SMBUS_ADDR_MASK          0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_SMBUS_ADDR_SHIFT         0

	/*  Switching regulator loop gain */
	uint32_t board_cfg;                                      /* 0x4010 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_MASK           0x0000000F
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_SHIFT          0
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_HW_DEFAULT     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X2             0x00000008
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X4             0x00000009
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X8             0x0000000a
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X16            0x0000000b
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_DIV8           0x0000000c
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_DIV4           0x0000000d
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_DIV2           0x0000000e
	#define EXTENDED_DEV_INFO_SHARED_CFG_LOOP_GAIN_X1             0x0000000f

	/*  whether shadow swim feature is supported */
	#define EXTENDED_DEV_INFO_SHARED_CFG_SHADOW_SWIM_MASK         0x00000100
	#define EXTENDED_DEV_INFO_SHARED_CFG_SHADOW_SWIM_SHIFT        8
	#define EXTENDED_DEV_INFO_SHARED_CFG_SHADOW_SWIM_DISABLED     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_SHADOW_SWIM_ENABLED      0x00000100

    /*  whether to show/hide SRIOV menu in CCM */
	#define EXTENDED_DEV_INFO_SHARED_CFG_SRIOV_SHOW_MENU_MASK     0x00000200
	#define EXTENDED_DEV_INFO_SHARED_CFG_SRIOV_SHOW_MENU_SHIFT    9
	#define EXTENDED_DEV_INFO_SHARED_CFG_SRIOV_SHOW_MENU          0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_SRIOV_HIDE_MENU          0x00000200

	/*  Overide PCIE revision ID when enabled the,
	    revision ID will set to B1=='0x11' */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVR_REV_ID_MASK          0x00000400
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVR_REV_ID_SHIFT         10
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVR_REV_ID_DISABLED      0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVR_REV_ID_ENABLED       0x00000400

	/*  Threshold in celcius for max continuous operation */
	uint32_t temperature_report;                             /* 0x4014 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_MCOT_MASK           0x0000007F
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_MCOT_SHIFT          0

	/*  Threshold in celcius for sensor caution */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SCT_MASK            0x00007F00
	#define EXTENDED_DEV_INFO_SHARED_CFG_TEMP_SCT_SHIFT           8

	/*  wwn node prefix to be used (unless value is 0) */
	uint32_t wwn_prefix;                                     /* 0x4018 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_NODE_PREFIX0_MASK    0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_NODE_PREFIX0_SHIFT   0

	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_NODE_PREFIX1_MASK    0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_NODE_PREFIX1_SHIFT   8

	/*  wwn port prefix to be used (unless value is 0) */
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_PORT_PREFIX0_MASK    0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_PORT_PREFIX0_SHIFT   16

	/*  wwn port prefix to be used (unless value is 0) */
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_PORT_PREFIX1_MASK    0xFF000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_WWN_PORT_PREFIX1_SHIFT   24

	/*  General debug nvm cfg */
	uint32_t dbg_cfg_flags;                                  /* 0x401C */
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_MASK                 0x000FFFFF
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SHIFT                0
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_ENABLE               0x00000001
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_EN_SIGDET_FILTER     0x00000002
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_LP_TX_PRESET7    0x00000004
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_TX_ANA_DEFAULT   0x00000008
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_PLL_ANA_DEFAULT  0x00000010
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_FORCE_G1PLL_RETUNE   0x00000020
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_RX_ANA_DEFAULT   0x00000040
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_FORCE_SERDES_RX_CLK  0x00000080
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_DIS_RX_LP_EIEOS      0x00000100
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_FINALIZE_UCODE       0x00000200
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_HOLDOFF_REQ          0x00000400
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_RX_SIGDET_OVERRIDE   0x00000800
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_GP_PORG_UC_RESET     0x00001000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SUPPRESS_COMPEN_EVT  0x00002000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_ADJ_TXEQ_P0_P1       0x00004000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_G3_PLL_RETUNE        0x00008000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_SET_MAC_PHY_CTL8     0x00010000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_DIS_MAC_G3_FRM_ERR   0x00020000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_INFERRED_EI          0x00040000
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_GEN3_COMPLI_ENA      0x00080000

	/*  Debug signet rx threshold */
	uint32_t dbg_rx_sigdet_threshold;                        /* 0x4020 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_RX_SIGDET_MASK       0x00000007
	#define EXTENDED_DEV_INFO_SHARED_CFG_DBG_RX_SIGDET_SHIFT      0

    /*  Enable IFFE feature */
	uint32_t iffe_features;                                  /* 0x4024 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_ENABLE_IFFE_MASK         0x00000001
	#define EXTENDED_DEV_INFO_SHARED_CFG_ENABLE_IFFE_SHIFT        0
	#define EXTENDED_DEV_INFO_SHARED_CFG_ENABLE_IFFE_DISABLED     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_ENABLE_IFFE_ENABLED      0x00000001

	/*  Allowable port enablement (bitmask for ports 3-1) */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_PORT_MASK       0x0000000E
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_PORT_SHIFT      1

	/*  Allow iSCSI offload override */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_ISCSI_MASK      0x00000010
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_ISCSI_SHIFT     4
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_ISCSI_DISABLED  0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_ISCSI_ENABLED   0x00000010

	/*  Allow FCoE offload override */
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_FCOE_MASK       0x00000020
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_FCOE_SHIFT      5
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_FCOE_DISABLED   0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_OVERRIDE_FCOE_ENABLED    0x00000020

	/*  Tie to adaptor */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TIE_ADAPTOR_MASK         0x00008000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TIE_ADAPTOR_SHIFT        15
	#define EXTENDED_DEV_INFO_SHARED_CFG_TIE_ADAPTOR_DISABLED     0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TIE_ADAPTOR_ENABLED      0x00008000

	/*  Currently enabled port(s) (bitmask for ports 3-1) */
	uint32_t current_iffe_mask;                              /* 0x4028 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_CFG_MASK         0x0000000E
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_CFG_SHIFT        1

	/*  Current iSCSI offload  */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_ISCSI_MASK       0x00000010
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_ISCSI_SHIFT      4
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_ISCSI_DISABLED   0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_ISCSI_ENABLED    0x00000010

	/*  Current FCoE offload  */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_FCOE_MASK        0x00000020
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_FCOE_SHIFT       5
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_FCOE_DISABLED    0x00000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CURRENT_FCOE_ENABLED     0x00000020

	/* FW set this pin to "0" (assert) these signal if either of its MAC
	 * or PHY specific threshold values is exceeded.
	 * Values are standard GPIO/EPIO pins.
	 */
	uint32_t threshold_pin;                                  /* 0x402C */
	#define EXTENDED_DEV_INFO_SHARED_CFG_TCONTROL_PIN_MASK        0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_TCONTROL_PIN_SHIFT       0
	#define EXTENDED_DEV_INFO_SHARED_CFG_TWARNING_PIN_MASK        0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_TWARNING_PIN_SHIFT       8
	#define EXTENDED_DEV_INFO_SHARED_CFG_TCRITICAL_PIN_MASK       0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_TCRITICAL_PIN_SHIFT      16

	/* MAC die temperature threshold in Celsius. */
	uint32_t mac_threshold_val;                              /* 0x4030 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CONTROL_MAC_THRESH_MASK  0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_CONTROL_MAC_THRESH_SHIFT 0
	#define EXTENDED_DEV_INFO_SHARED_CFG_WARNING_MAC_THRESH_MASK  0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_WARNING_MAC_THRESH_SHIFT 8
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRITICAL_MAC_THRESH_MASK 0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRITICAL_MAC_THRESH_SHIFT 16

	/*  PHY die temperature threshold in Celsius. */
	uint32_t phy_threshold_val;                              /* 0x4034 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_CONTROL_PHY_THRESH_MASK  0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_CONTROL_PHY_THRESH_SHIFT 0
	#define EXTENDED_DEV_INFO_SHARED_CFG_WARNING_PHY_THRESH_MASK  0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_WARNING_PHY_THRESH_SHIFT 8
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRITICAL_PHY_THRESH_MASK 0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_CRITICAL_PHY_THRESH_SHIFT 16

	/* External pins to communicate with host.
	 * Values are standard GPIO/EPIO pins.
	 */
	uint32_t host_pin;                                       /* 0x4038 */
	#define EXTENDED_DEV_INFO_SHARED_CFG_I2C_ISOLATE_MASK         0x000000FF
	#define EXTENDED_DEV_INFO_SHARED_CFG_I2C_ISOLATE_SHIFT        0
	#define EXTENDED_DEV_INFO_SHARED_CFG_MEZZ_FAULT_MASK          0x0000FF00
	#define EXTENDED_DEV_INFO_SHARED_CFG_MEZZ_FAULT_SHIFT         8
	#define EXTENDED_DEV_INFO_SHARED_CFG_MEZZ_VPD_UPDATE_MASK     0x00FF0000
	#define EXTENDED_DEV_INFO_SHARED_CFG_MEZZ_VPD_UPDATE_SHIFT    16
	#define EXTENDED_DEV_INFO_SHARED_CFG_VPD_CACHE_COMP_MASK      0xFF000000
	#define EXTENDED_DEV_INFO_SHARED_CFG_VPD_CACHE_COMP_SHIFT     24
};


#if !defined(__LITTLE_ENDIAN) && !defined(__BIG_ENDIAN)
	#error "Missing either LITTLE_ENDIAN or BIG_ENDIAN definition."
#endif

#define FUNC_0              0
#define FUNC_1              1
#define FUNC_2              2
#define FUNC_3              3
#define FUNC_4              4
#define FUNC_5              5
#define FUNC_6              6
#define FUNC_7              7
#define E1_FUNC_MAX         2
#define E1H_FUNC_MAX            8
#define E2_FUNC_MAX         4   /* per path */

#define VN_0                0
#define VN_1                1
#define VN_2                2
#define VN_3                3
#define E1VN_MAX            1
#define E1HVN_MAX           4

#define E2_VF_MAX           64  /* HC_REG_VF_CONFIGURATION_SIZE */
/* This value (in milliseconds) determines the frequency of the driver
 * issuing the PULSE message code.  The firmware monitors this periodic
 * pulse to determine when to switch to an OS-absent mode. */
#define DRV_PULSE_PERIOD_MS     250

/* This value (in milliseconds) determines how long the driver should
 * wait for an acknowledgement from the firmware before timing out.  Once
 * the firmware has timed out, the driver will assume there is no firmware
 * running and there won't be any firmware-driver synchronization during a
 * driver reset. */
#define FW_ACK_TIME_OUT_MS      5000

#define FW_ACK_POLL_TIME_MS     1

#define FW_ACK_NUM_OF_POLL  (FW_ACK_TIME_OUT_MS/FW_ACK_POLL_TIME_MS)

#define MFW_TRACE_SIGNATURE     0x54524342

/****************************************************************************
 * Driver <-> FW Mailbox                                                    *
 ****************************************************************************/
struct drv_port_mb {

	uint32_t link_status;
	/* Driver should update this field on any link change event */

	#define LINK_STATUS_NONE				(0<<0)
	#define LINK_STATUS_LINK_FLAG_MASK			0x00000001
	#define LINK_STATUS_LINK_UP				0x00000001
	#define LINK_STATUS_SPEED_AND_DUPLEX_MASK		0x0000001E
	#define LINK_STATUS_SPEED_AND_DUPLEX_AN_NOT_COMPLETE	(0<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_10THD		(1<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_10TFD		(2<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_100TXHD		(3<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_100T4		(4<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_100TXFD		(5<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_1000THD		(6<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_1000TFD		(7<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_1000XFD		(7<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_2500THD		(8<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_2500TFD		(9<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_2500XFD		(9<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_10GTFD		(10<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_10GXFD		(10<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_20GTFD		(11<<1)
	#define LINK_STATUS_SPEED_AND_DUPLEX_20GXFD		(11<<1)

	#define LINK_STATUS_AUTO_NEGOTIATE_FLAG_MASK		0x00000020
	#define LINK_STATUS_AUTO_NEGOTIATE_ENABLED		0x00000020

	#define LINK_STATUS_AUTO_NEGOTIATE_COMPLETE		0x00000040
	#define LINK_STATUS_PARALLEL_DETECTION_FLAG_MASK	0x00000080
	#define LINK_STATUS_PARALLEL_DETECTION_USED		0x00000080

	#define LINK_STATUS_LINK_PARTNER_1000TFD_CAPABLE	0x00000200
	#define LINK_STATUS_LINK_PARTNER_1000THD_CAPABLE	0x00000400
	#define LINK_STATUS_LINK_PARTNER_100T4_CAPABLE		0x00000800
	#define LINK_STATUS_LINK_PARTNER_100TXFD_CAPABLE	0x00001000
	#define LINK_STATUS_LINK_PARTNER_100TXHD_CAPABLE	0x00002000
	#define LINK_STATUS_LINK_PARTNER_10TFD_CAPABLE		0x00004000
	#define LINK_STATUS_LINK_PARTNER_10THD_CAPABLE		0x00008000

	#define LINK_STATUS_TX_FLOW_CONTROL_FLAG_MASK		0x00010000
	#define LINK_STATUS_TX_FLOW_CONTROL_ENABLED		0x00010000

	#define LINK_STATUS_RX_FLOW_CONTROL_FLAG_MASK		0x00020000
	#define LINK_STATUS_RX_FLOW_CONTROL_ENABLED		0x00020000

	#define LINK_STATUS_LINK_PARTNER_FLOW_CONTROL_MASK	0x000C0000
	#define LINK_STATUS_LINK_PARTNER_NOT_PAUSE_CAPABLE	(0<<18)
	#define LINK_STATUS_LINK_PARTNER_SYMMETRIC_PAUSE	(1<<18)
	#define LINK_STATUS_LINK_PARTNER_ASYMMETRIC_PAUSE	(2<<18)
	#define LINK_STATUS_LINK_PARTNER_BOTH_PAUSE		(3<<18)

	#define LINK_STATUS_SERDES_LINK				0x00100000

	#define LINK_STATUS_LINK_PARTNER_2500XFD_CAPABLE	0x00200000
	#define LINK_STATUS_LINK_PARTNER_2500XHD_CAPABLE	0x00400000
	#define LINK_STATUS_LINK_PARTNER_10GXFD_CAPABLE		0x00800000
	#define LINK_STATUS_LINK_PARTNER_20GXFD_CAPABLE		0x10000000

	#define LINK_STATUS_PFC_ENABLED				0x20000000

	#define LINK_STATUS_PHYSICAL_LINK_FLAG			0x40000000
	#define LINK_STATUS_SFP_TX_FAULT			0x80000000

	uint32_t port_stx;

	uint32_t stat_nig_timer;

	/* MCP firmware does not use this field */
	uint32_t ext_phy_fw_version;

};


struct drv_func_mb {

	uint32_t drv_mb_header;
	#define DRV_MSG_CODE_MASK                       0xffff0000
	#define DRV_MSG_CODE_LOAD_REQ                   0x10000000
	#define DRV_MSG_CODE_LOAD_DONE                  0x11000000
	#define DRV_MSG_CODE_UNLOAD_REQ_WOL_EN          0x20000000
	#define DRV_MSG_CODE_UNLOAD_REQ_WOL_DIS         0x20010000
	#define DRV_MSG_CODE_UNLOAD_REQ_WOL_MCP         0x20020000
	#define DRV_MSG_CODE_UNLOAD_DONE                0x21000000
	#define DRV_MSG_CODE_DCC_OK                     0x30000000
	#define DRV_MSG_CODE_DCC_FAILURE                0x31000000
	#define DRV_MSG_CODE_DIAG_ENTER_REQ             0x50000000
	#define DRV_MSG_CODE_DIAG_EXIT_REQ              0x60000000
	#define DRV_MSG_CODE_VALIDATE_KEY               0x70000000
	#define DRV_MSG_CODE_GET_CURR_KEY               0x80000000
	#define DRV_MSG_CODE_GET_UPGRADE_KEY            0x81000000
	#define DRV_MSG_CODE_GET_MANUF_KEY              0x82000000
	#define DRV_MSG_CODE_LOAD_L2B_PRAM              0x90000000

	/*
	 * The optic module verification command requires bootcode
	 * v5.0.6 or later, te specific optic module verification command
	 * requires bootcode v5.2.12 or later
	 */
	#define DRV_MSG_CODE_VRFY_FIRST_PHY_OPT_MDL     0xa0000000
	#define REQ_BC_VER_4_VRFY_FIRST_PHY_OPT_MDL     0x00050006
	#define DRV_MSG_CODE_VRFY_SPECIFIC_PHY_OPT_MDL  0xa1000000
	#define REQ_BC_VER_4_VRFY_SPECIFIC_PHY_OPT_MDL  0x00050234
	#define DRV_MSG_CODE_VRFY_AFEX_SUPPORTED        0xa2000000
	#define REQ_BC_VER_4_VRFY_AFEX_SUPPORTED        0x00070002
	#define REQ_BC_VER_4_SFP_TX_DISABLE_SUPPORTED   0x00070014
	#define REQ_BC_VER_4_MT_SUPPORTED               0x00070201
	#define REQ_BC_VER_4_PFC_STATS_SUPPORTED        0x00070201
	#define REQ_BC_VER_4_FCOE_FEATURES              0x00070209

	#define DRV_MSG_CODE_DCBX_ADMIN_PMF_MSG         0xb0000000
	#define DRV_MSG_CODE_DCBX_PMF_DRV_OK            0xb2000000
	#define REQ_BC_VER_4_DCBX_ADMIN_MSG_NON_PMF     0x00070401

	#define DRV_MSG_CODE_VF_DISABLED_DONE           0xc0000000

	#define DRV_MSG_CODE_AFEX_DRIVER_SETMAC         0xd0000000
	#define DRV_MSG_CODE_AFEX_LISTGET_ACK           0xd1000000
	#define DRV_MSG_CODE_AFEX_LISTSET_ACK           0xd2000000
	#define DRV_MSG_CODE_AFEX_STATSGET_ACK          0xd3000000
	#define DRV_MSG_CODE_AFEX_VIFSET_ACK            0xd4000000

	#define DRV_MSG_CODE_DRV_INFO_ACK               0xd8000000
	#define DRV_MSG_CODE_DRV_INFO_NACK              0xd9000000

	#define DRV_MSG_CODE_EEE_RESULTS_ACK            0xda000000

	#define DRV_MSG_CODE_RMMOD                      0xdb000000
	#define REQ_BC_VER_4_RMMOD_CMD                  0x0007080f

	#define DRV_MSG_CODE_SET_MF_BW                  0xe0000000
	#define REQ_BC_VER_4_SET_MF_BW                  0x00060202
	#define DRV_MSG_CODE_SET_MF_BW_ACK              0xe1000000

	#define DRV_MSG_CODE_LINK_STATUS_CHANGED        0x01000000

	#define DRV_MSG_CODE_INITIATE_FLR               0x02000000
	#define REQ_BC_VER_4_INITIATE_FLR               0x00070213

	#define BIOS_MSG_CODE_LIC_CHALLENGE             0xff010000
	#define BIOS_MSG_CODE_LIC_RESPONSE              0xff020000
	#define BIOS_MSG_CODE_VIRT_MAC_PRIM             0xff030000
	#define BIOS_MSG_CODE_VIRT_MAC_ISCSI            0xff040000

	#define DRV_MSG_CODE_IMG_OFFSET_REQ             0xe2000000
	#define DRV_MSG_CODE_IMG_SIZE_REQ               0xe3000000

	#define DRV_MSG_SEQ_NUMBER_MASK                 0x0000ffff

	uint32_t drv_mb_param;
	#define DRV_MSG_CODE_SET_MF_BW_MIN_MASK         0x00ff0000
	#define DRV_MSG_CODE_SET_MF_BW_MAX_MASK         0xff000000

	#define DRV_MSG_CODE_UNLOAD_NON_D3_POWER        0x00000001
	#define DRV_MSG_CODE_UNLOAD_SKIP_LINK_RESET     0x00000002

	#define DRV_MSG_CODE_LOAD_REQ_WITH_LFA          0x0000100a
	#define DRV_MSG_CODE_LOAD_REQ_FORCE_LFA         0x00002000

	#define DRV_MSG_CODE_USR_BLK_IMAGE_REQ          0x00000001
	#define DRV_MSG_CODE_ISCSI_PERS_IMAGE_REQ       0x00000002

	uint32_t fw_mb_header;
	#define FW_MSG_CODE_MASK                        0xffff0000
	#define FW_MSG_CODE_DRV_LOAD_COMMON             0x10100000
	#define FW_MSG_CODE_DRV_LOAD_PORT               0x10110000
	#define FW_MSG_CODE_DRV_LOAD_FUNCTION           0x10120000
	/* Load common chip is supported from bc 6.0.0  */
	#define REQ_BC_VER_4_DRV_LOAD_COMMON_CHIP       0x00060000
	#define FW_MSG_CODE_DRV_LOAD_COMMON_CHIP        0x10130000

	#define FW_MSG_CODE_DRV_LOAD_REFUSED            0x10200000
	#define FW_MSG_CODE_DRV_LOAD_DONE               0x11100000
	#define FW_MSG_CODE_DRV_UNLOAD_COMMON           0x20100000
	#define FW_MSG_CODE_DRV_UNLOAD_PORT             0x20110000
	#define FW_MSG_CODE_DRV_UNLOAD_FUNCTION         0x20120000
	#define FW_MSG_CODE_DRV_UNLOAD_DONE             0x21100000
	#define FW_MSG_CODE_DCC_DONE                    0x30100000
	#define FW_MSG_CODE_LLDP_DONE                   0x40100000
	#define FW_MSG_CODE_DIAG_ENTER_DONE             0x50100000
	#define FW_MSG_CODE_DIAG_REFUSE                 0x50200000
	#define FW_MSG_CODE_DIAG_EXIT_DONE              0x60100000
	#define FW_MSG_CODE_VALIDATE_KEY_SUCCESS        0x70100000
	#define FW_MSG_CODE_VALIDATE_KEY_FAILURE        0x70200000
	#define FW_MSG_CODE_GET_KEY_DONE                0x80100000
	#define FW_MSG_CODE_NO_KEY                      0x80f00000
	#define FW_MSG_CODE_LIC_INFO_NOT_READY          0x80f80000
	#define FW_MSG_CODE_L2B_PRAM_LOADED             0x90100000
	#define FW_MSG_CODE_L2B_PRAM_T_LOAD_FAILURE     0x90210000
	#define FW_MSG_CODE_L2B_PRAM_C_LOAD_FAILURE     0x90220000
	#define FW_MSG_CODE_L2B_PRAM_X_LOAD_FAILURE     0x90230000
	#define FW_MSG_CODE_L2B_PRAM_U_LOAD_FAILURE     0x90240000
	#define FW_MSG_CODE_VRFY_OPT_MDL_SUCCESS        0xa0100000
	#define FW_MSG_CODE_VRFY_OPT_MDL_INVLD_IMG      0xa0200000
	#define FW_MSG_CODE_VRFY_OPT_MDL_UNAPPROVED     0xa0300000
	#define FW_MSG_CODE_VF_DISABLED_DONE            0xb0000000
	#define FW_MSG_CODE_HW_SET_INVALID_IMAGE        0xb0100000

	#define FW_MSG_CODE_AFEX_DRIVER_SETMAC_DONE     0xd0100000
	#define FW_MSG_CODE_AFEX_LISTGET_ACK            0xd1100000
	#define FW_MSG_CODE_AFEX_LISTSET_ACK            0xd2100000
	#define FW_MSG_CODE_AFEX_STATSGET_ACK           0xd3100000
	#define FW_MSG_CODE_AFEX_VIFSET_ACK             0xd4100000

	#define FW_MSG_CODE_DRV_INFO_ACK                0xd8100000
	#define FW_MSG_CODE_DRV_INFO_NACK               0xd9100000

	#define FW_MSG_CODE_EEE_RESULS_ACK              0xda100000

	#define FW_MSG_CODE_RMMOD_ACK                   0xdb100000

	#define FW_MSG_CODE_SET_MF_BW_SENT              0xe0000000
	#define FW_MSG_CODE_SET_MF_BW_DONE              0xe1000000

	#define FW_MSG_CODE_LINK_CHANGED_ACK            0x01100000

	#define FW_MSG_CODE_FLR_ACK                     0x02000000
	#define FW_MSG_CODE_FLR_NACK                    0x02100000

	#define FW_MSG_CODE_LIC_CHALLENGE               0xff010000
	#define FW_MSG_CODE_LIC_RESPONSE                0xff020000
	#define FW_MSG_CODE_VIRT_MAC_PRIM               0xff030000
	#define FW_MSG_CODE_VIRT_MAC_ISCSI              0xff040000

	#define FW_MSG_CODE_IMG_OFFSET_RESPONSE         0xe2100000
	#define FW_MSG_CODE_IMG_SIZE_RESPONSE           0xe3100000

	#define FW_MSG_SEQ_NUMBER_MASK                  0x0000ffff

	uint32_t fw_mb_param;

	#define FW_PARAM_INVALID_IMG                    0xffffffff

	uint32_t drv_pulse_mb;
	#define DRV_PULSE_SEQ_MASK                      0x00007fff
	#define DRV_PULSE_SYSTEM_TIME_MASK              0xffff0000
	/*
	 * The system time is in the format of
	 * (year-2001)*12*32 + month*32 + day.
	 */
	#define DRV_PULSE_ALWAYS_ALIVE                  0x00008000
	/*
	 * Indicate to the firmware not to go into the
	 * OS-absent when it is not getting driver pulse.
	 * This is used for debugging as well for PXE(MBA).
	 */

	uint32_t mcp_pulse_mb;
	#define MCP_PULSE_SEQ_MASK                      0x00007fff
	#define MCP_PULSE_ALWAYS_ALIVE                  0x00008000
	/* Indicates to the driver not to assert due to lack
	 * of MCP response */
	#define MCP_EVENT_MASK                          0xffff0000
	#define MCP_EVENT_OTHER_DRIVER_RESET_REQ        0x00010000

	uint32_t iscsi_boot_signature;
	uint32_t iscsi_boot_block_offset;

	uint32_t drv_status;
	#define DRV_STATUS_PMF                          0x00000001
	#define DRV_STATUS_VF_DISABLED                  0x00000002
	#define DRV_STATUS_SET_MF_BW                    0x00000004
	#define DRV_STATUS_LINK_EVENT                   0x00000008

	#define DRV_STATUS_DCC_EVENT_MASK               0x0000ff00
	#define DRV_STATUS_DCC_DISABLE_ENABLE_PF        0x00000100
	#define DRV_STATUS_DCC_BANDWIDTH_ALLOCATION     0x00000200
	#define DRV_STATUS_DCC_CHANGE_MAC_ADDRESS       0x00000400
	#define DRV_STATUS_DCC_RESERVED1                0x00000800
	#define DRV_STATUS_DCC_SET_PROTOCOL             0x00001000
	#define DRV_STATUS_DCC_SET_PRIORITY             0x00002000

	#define DRV_STATUS_DCBX_EVENT_MASK              0x000f0000
	#define DRV_STATUS_DCBX_NEGOTIATION_RESULTS     0x00010000
	#define DRV_STATUS_AFEX_EVENT_MASK              0x03f00000
	#define DRV_STATUS_AFEX_LISTGET_REQ             0x00100000
	#define DRV_STATUS_AFEX_LISTSET_REQ             0x00200000
	#define DRV_STATUS_AFEX_STATSGET_REQ            0x00400000
	#define DRV_STATUS_AFEX_VIFSET_REQ              0x00800000

	#define DRV_STATUS_DRV_INFO_REQ                 0x04000000

	#define DRV_STATUS_EEE_NEGOTIATION_RESULTS      0x08000000

	uint32_t virt_mac_upper;
	#define VIRT_MAC_SIGN_MASK                      0xffff0000
	#define VIRT_MAC_SIGNATURE                      0x564d0000
	uint32_t virt_mac_lower;

};


/****************************************************************************
 * Management firmware state                                                *
 ****************************************************************************/
/* Allocate 440 bytes for management firmware */
#define MGMTFW_STATE_WORD_SIZE                          110

struct mgmtfw_state {
	uint32_t opaque[MGMTFW_STATE_WORD_SIZE];
};


/****************************************************************************
 * Multi-Function configuration                                             *
 ****************************************************************************/
struct shared_mf_cfg {

	uint32_t clp_mb;
	#define SHARED_MF_CLP_SET_DEFAULT               0x00000000
	/* set by CLP */
	#define SHARED_MF_CLP_EXIT                      0x00000001
	/* set by MCP */
	#define SHARED_MF_CLP_EXIT_DONE                 0x00010000

};

struct port_mf_cfg {

	uint32_t dynamic_cfg;    /* device control channel */
	#define PORT_MF_CFG_E1HOV_TAG_MASK              0x0000ffff
	#define PORT_MF_CFG_E1HOV_TAG_SHIFT             0
	#define PORT_MF_CFG_E1HOV_TAG_DEFAULT         PORT_MF_CFG_E1HOV_TAG_MASK

	uint32_t reserved[1];

};

struct func_mf_cfg {

	uint32_t config;
	/* E/R/I/D */
	/* function 0 of each port cannot be hidden */
	#define FUNC_MF_CFG_FUNC_HIDE                   0x00000001

	#define FUNC_MF_CFG_PROTOCOL_MASK               0x00000006
	#define FUNC_MF_CFG_PROTOCOL_FCOE               0x00000000
	#define FUNC_MF_CFG_PROTOCOL_ETHERNET           0x00000002
	#define FUNC_MF_CFG_PROTOCOL_ETHERNET_WITH_RDMA 0x00000004
	#define FUNC_MF_CFG_PROTOCOL_ISCSI              0x00000006
	#define FUNC_MF_CFG_PROTOCOL_DEFAULT \
				FUNC_MF_CFG_PROTOCOL_ETHERNET_WITH_RDMA

	#define FUNC_MF_CFG_FUNC_DISABLED               0x00000008
	#define FUNC_MF_CFG_FUNC_DELETED                0x00000010

	#define FUNC_MF_CFG_FUNC_BOOT_MASK              0x00000060
	#define FUNC_MF_CFG_FUNC_BOOT_BIOS_CTRL         0x00000000
	#define FUNC_MF_CFG_FUNC_BOOT_VCM_DISABLED      0x00000020
	#define FUNC_MF_CFG_FUNC_BOOT_VCM_ENABLED       0x00000040

	/* PRI */
	/* 0 - low priority, 3 - high priority */
	#define FUNC_MF_CFG_TRANSMIT_PRIORITY_MASK      0x00000300
	#define FUNC_MF_CFG_TRANSMIT_PRIORITY_SHIFT     8
	#define FUNC_MF_CFG_TRANSMIT_PRIORITY_DEFAULT   0x00000000

	/* MINBW, MAXBW */
	/* value range - 0..100, increments in 100Mbps */
	#define FUNC_MF_CFG_MIN_BW_MASK                 0x00ff0000
	#define FUNC_MF_CFG_MIN_BW_SHIFT                16
	#define FUNC_MF_CFG_MIN_BW_DEFAULT              0x00000000
	#define FUNC_MF_CFG_MAX_BW_MASK                 0xff000000
	#define FUNC_MF_CFG_MAX_BW_SHIFT                24
	#define FUNC_MF_CFG_MAX_BW_DEFAULT              0x64000000

	uint32_t mac_upper;	    /* MAC */
	#define FUNC_MF_CFG_UPPERMAC_MASK               0x0000ffff
	#define FUNC_MF_CFG_UPPERMAC_SHIFT              0
	#define FUNC_MF_CFG_UPPERMAC_DEFAULT           FUNC_MF_CFG_UPPERMAC_MASK
	uint32_t mac_lower;
	#define FUNC_MF_CFG_LOWERMAC_DEFAULT            0xffffffff

	uint32_t e1hov_tag;	/* VNI */
	#define FUNC_MF_CFG_E1HOV_TAG_MASK              0x0000ffff
	#define FUNC_MF_CFG_E1HOV_TAG_SHIFT             0
	#define FUNC_MF_CFG_E1HOV_TAG_DEFAULT         FUNC_MF_CFG_E1HOV_TAG_MASK

	/* afex default VLAN ID - 12 bits */
	#define FUNC_MF_CFG_AFEX_VLAN_MASK              0x0fff0000
	#define FUNC_MF_CFG_AFEX_VLAN_SHIFT             16

	uint32_t afex_config;
	#define FUNC_MF_CFG_AFEX_COS_FILTER_MASK                     0x000000ff
	#define FUNC_MF_CFG_AFEX_COS_FILTER_SHIFT                    0
	#define FUNC_MF_CFG_AFEX_MBA_ENABLED_MASK                    0x0000ff00
	#define FUNC_MF_CFG_AFEX_MBA_ENABLED_SHIFT                   8
	#define FUNC_MF_CFG_AFEX_MBA_ENABLED_VAL                     0x00000100
	#define FUNC_MF_CFG_AFEX_VLAN_MODE_MASK                      0x000f0000
	#define FUNC_MF_CFG_AFEX_VLAN_MODE_SHIFT                     16

	uint32_t pf_allocation;
	/* number of vfs in function, if 0 - sriov disabled */
	#define FUNC_MF_CFG_NUMBER_OF_VFS_MASK                      0x000000FF
	#define FUNC_MF_CFG_NUMBER_OF_VFS_SHIFT                     0
};

enum mf_cfg_afex_vlan_mode {
	FUNC_MF_CFG_AFEX_VLAN_TRUNK_MODE = 0,
	FUNC_MF_CFG_AFEX_VLAN_ACCESS_MODE,
	FUNC_MF_CFG_AFEX_VLAN_TRUNK_TAG_NATIVE_MODE
};

/* This structure is not applicable and should not be accessed on 57711 */
struct func_ext_cfg {
	uint32_t func_cfg;
	#define MACP_FUNC_CFG_FLAGS_MASK                0x0000007F
	#define MACP_FUNC_CFG_FLAGS_SHIFT               0
	#define MACP_FUNC_CFG_FLAGS_ENABLED             0x00000001
	#define MACP_FUNC_CFG_FLAGS_ETHERNET            0x00000002
	#define MACP_FUNC_CFG_FLAGS_ISCSI_OFFLOAD       0x00000004
	#define MACP_FUNC_CFG_FLAGS_FCOE_OFFLOAD        0x00000008
    #define MACP_FUNC_CFG_PAUSE_ON_HOST_RING        0x00000080

	uint32_t iscsi_mac_addr_upper;
	uint32_t iscsi_mac_addr_lower;

	uint32_t fcoe_mac_addr_upper;
	uint32_t fcoe_mac_addr_lower;

	uint32_t fcoe_wwn_port_name_upper;
	uint32_t fcoe_wwn_port_name_lower;

	uint32_t fcoe_wwn_node_name_upper;
	uint32_t fcoe_wwn_node_name_lower;

	uint32_t preserve_data;
	#define MF_FUNC_CFG_PRESERVE_L2_MAC             (1<<0)
	#define MF_FUNC_CFG_PRESERVE_ISCSI_MAC          (1<<1)
	#define MF_FUNC_CFG_PRESERVE_FCOE_MAC           (1<<2)
	#define MF_FUNC_CFG_PRESERVE_FCOE_WWN_P         (1<<3)
	#define MF_FUNC_CFG_PRESERVE_FCOE_WWN_N         (1<<4)
	#define MF_FUNC_CFG_PRESERVE_TX_BW              (1<<5)
};

struct mf_cfg {

	struct shared_mf_cfg    shared_mf_config;       /* 0x4 */
	struct port_mf_cfg  port_mf_config[NVM_PATH_MAX][PORT_MAX];
    /* 0x10*2=0x20 */
	/* for all chips, there are 8 mf functions */
	struct func_mf_cfg  func_mf_config[E1H_FUNC_MAX]; /* 0x18 * 8 = 0xc0 */
	/*
	 * Extended configuration per function  - this array does not exist and
	 * should not be accessed on 57711
	 */
	struct func_ext_cfg func_ext_config[E1H_FUNC_MAX]; /* 0x28 * 8 = 0x140*/
}; /* 0x224 */

/****************************************************************************
 * Shared Memory Region                                                     *
 ****************************************************************************/
struct shmem_region {		       /*   SharedMem Offset (size) */

	uint32_t         validity_map[PORT_MAX];  /* 0x0 (4*2 = 0x8) */
	#define SHR_MEM_FORMAT_REV_MASK                     0xff000000
	#define SHR_MEM_FORMAT_REV_ID                       ('A'<<24)
	/* validity bits */
	#define SHR_MEM_VALIDITY_PCI_CFG                    0x00100000
	#define SHR_MEM_VALIDITY_MB                         0x00200000
	#define SHR_MEM_VALIDITY_DEV_INFO                   0x00400000
	#define SHR_MEM_VALIDITY_RESERVED                   0x00000007
	/* One licensing bit should be set */
	#define SHR_MEM_VALIDITY_LIC_KEY_IN_EFFECT_MASK     0x00000038
	#define SHR_MEM_VALIDITY_LIC_MANUF_KEY_IN_EFFECT    0x00000008
	#define SHR_MEM_VALIDITY_LIC_UPGRADE_KEY_IN_EFFECT  0x00000010
	#define SHR_MEM_VALIDITY_LIC_NO_KEY_IN_EFFECT       0x00000020
	/* Active MFW */
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_UNKNOWN         0x00000000
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_MASK            0x000001c0
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_IPMI            0x00000040
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_UMP             0x00000080
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_NCSI            0x000000c0
	#define SHR_MEM_VALIDITY_ACTIVE_MFW_NONE            0x000001c0

	struct shm_dev_info dev_info;	     /* 0x8     (0x438) */

	license_key_t       drv_lic_key[PORT_MAX]; /* 0x440 (52*2=0x68) */

	/* FW information (for internal FW use) */
	uint32_t         fw_info_fio_offset;		/* 0x4a8       (0x4) */
	struct mgmtfw_state mgmtfw_state;	/* 0x4ac     (0x1b8) */

	struct drv_port_mb  port_mb[PORT_MAX];	/* 0x664 (16*2=0x20) */


#ifdef BMAPI
	/* This is a variable length array */
	/* the number of function depends on the chip type */
	struct drv_func_mb func_mb[1];	/* 0x684 (44*2/4/8=0x58/0xb0/0x160) */
#else
	/* the number of function depends on the chip type */
	struct drv_func_mb  func_mb[];	/* 0x684 (44*2/4/8=0x58/0xb0/0x160) */
#endif /* BMAPI */

}; /* 57710 = 0x6dc | 57711 = 0x7E4 | 57712 = 0x734 */

/****************************************************************************
 * Shared Memory 2 Region                                                   *
 ****************************************************************************/
/* The fw_flr_ack is actually built in the following way:                   */
/* 8 bit:  PF ack                                                           */
/* 64 bit: VF ack                                                           */
/* 8 bit:  ios_dis_ack                                                      */
/* In order to maintain endianity in the mailbox hsi, we want to keep using */
/* uint32_t. The fw must have the VF right after the PF since this is how it     */
/* access arrays(it expects always the VF to reside after the PF, and that  */
/* makes the calculation much easier for it. )                              */
/* In order to answer both limitations, and keep the struct small, the code */
/* will abuse the structure defined here to achieve the actual partition    */
/* above                                                                    */
/****************************************************************************/
struct fw_flr_ack {
	uint32_t         pf_ack;
	uint32_t         vf_ack[1];
	uint32_t         iov_dis_ack;
};

struct fw_flr_mb {
	uint32_t         aggint;
	uint32_t         opgen_addr;
	struct fw_flr_ack ack;
};

struct eee_remote_vals {
	uint32_t         tx_tw;
	uint32_t         rx_tw;
};

/**** SUPPORT FOR SHMEM ARRRAYS ***
 * The SHMEM HSI is aligned on 32 bit boundaries which makes it difficult to
 * define arrays with storage types smaller then unsigned dwords.
 * The macros below add generic support for SHMEM arrays with numeric elements
 * that can span 2,4,8 or 16 bits. The array underlying type is a 32 bit dword
 * array with individual bit-filed elements accessed using shifts and masks.
 *
 */

/* eb is the bitwidth of a single element */
#define SHMEM_ARRAY_MASK(eb)		((1<<(eb))-1)
#define SHMEM_ARRAY_ENTRY(i, eb)	((i)/(32/(eb)))

/* the bit-position macro allows the used to flip the order of the arrays
 * elements on a per byte or word boundary.
 *
 * example: an array with 8 entries each 4 bit wide. This array will fit into
 * a single dword. The diagrmas below show the array order of the nibbles.
 *
 * SHMEM_ARRAY_BITPOS(i, 4, 4) defines the stadard ordering:
 *
 *                |                |                |               |
 *   0    |   1   |   2    |   3   |   4    |   5   |   6   |   7   |
 *                |                |                |               |
 *
 * SHMEM_ARRAY_BITPOS(i, 4, 8) defines a flip ordering per byte:
 *
 *                |                |                |               |
 *   1   |   0    |   3    |   2   |   5    |   4   |   7   |   6   |
 *                |                |                |               |
 *
 * SHMEM_ARRAY_BITPOS(i, 4, 16) defines a flip ordering per word:
 *
 *                |                |                |               |
 *   3   |   2    |   1   |   0    |   7   |   6    |   5   |   4   |
 *                |                |                |               |
 */
#define SHMEM_ARRAY_BITPOS(i, eb, fb)	\
	((((32/(fb)) - 1 - ((i)/((fb)/(eb))) % (32/(fb))) * (fb)) + \
	(((i)%((fb)/(eb))) * (eb)))

#define SHMEM_ARRAY_GET(a, i, eb, fb)					\
	((a[SHMEM_ARRAY_ENTRY(i, eb)] >> SHMEM_ARRAY_BITPOS(i, eb, fb)) &  \
	SHMEM_ARRAY_MASK(eb))

#define SHMEM_ARRAY_SET(a, i, eb, fb, val)				\
do {									   \
	a[SHMEM_ARRAY_ENTRY(i, eb)] &= ~(SHMEM_ARRAY_MASK(eb) <<	   \
	SHMEM_ARRAY_BITPOS(i, eb, fb));					   \
	a[SHMEM_ARRAY_ENTRY(i, eb)] |= (((val) & SHMEM_ARRAY_MASK(eb)) <<  \
	SHMEM_ARRAY_BITPOS(i, eb, fb));					   \
} while (0)


/****START OF DCBX STRUCTURES DECLARATIONS****/
#define DCBX_MAX_NUM_PRI_PG_ENTRIES	8
#define DCBX_PRI_PG_BITWIDTH		4
#define DCBX_PRI_PG_FBITS		8
#define DCBX_PRI_PG_GET(a, i)		\
	SHMEM_ARRAY_GET(a, i, DCBX_PRI_PG_BITWIDTH, DCBX_PRI_PG_FBITS)
#define DCBX_PRI_PG_SET(a, i, val)	\
	SHMEM_ARRAY_SET(a, i, DCBX_PRI_PG_BITWIDTH, DCBX_PRI_PG_FBITS, val)
#define DCBX_MAX_NUM_PG_BW_ENTRIES	8
#define DCBX_BW_PG_BITWIDTH		8
#define DCBX_PG_BW_GET(a, i)		\
	SHMEM_ARRAY_GET(a, i, DCBX_BW_PG_BITWIDTH, DCBX_BW_PG_BITWIDTH)
#define DCBX_PG_BW_SET(a, i, val)	\
	SHMEM_ARRAY_SET(a, i, DCBX_BW_PG_BITWIDTH, DCBX_BW_PG_BITWIDTH, val)
#define DCBX_STRICT_PRI_PG		15
#define DCBX_MAX_APP_PROTOCOL		16
#define DCBX_MAX_APP_LOCAL	    32
#define FCOE_APP_IDX			0
#define ISCSI_APP_IDX			1
#define PREDEFINED_APP_IDX_MAX		2


/* Big/Little endian have the same representation. */
struct dcbx_ets_feature {
	/*
	 * For Admin MIB - is this feature supported by the
	 * driver | For Local MIB - should this feature be enabled.
	 */
	uint32_t enabled;
	uint32_t  pg_bw_tbl[2];
	uint32_t  pri_pg_tbl[1];
};

/* Driver structure in LE */
struct dcbx_pfc_feature {
#ifdef __BIG_ENDIAN
	uint8_t pri_en_bitmap;
	#define DCBX_PFC_PRI_0 0x01
	#define DCBX_PFC_PRI_1 0x02
	#define DCBX_PFC_PRI_2 0x04
	#define DCBX_PFC_PRI_3 0x08
	#define DCBX_PFC_PRI_4 0x10
	#define DCBX_PFC_PRI_5 0x20
	#define DCBX_PFC_PRI_6 0x40
	#define DCBX_PFC_PRI_7 0x80
	uint8_t pfc_caps;
	uint8_t reserved;
	uint8_t enabled;
#elif defined(__LITTLE_ENDIAN)
	uint8_t enabled;
	uint8_t reserved;
	uint8_t pfc_caps;
	uint8_t pri_en_bitmap;
	#define DCBX_PFC_PRI_0 0x01
	#define DCBX_PFC_PRI_1 0x02
	#define DCBX_PFC_PRI_2 0x04
	#define DCBX_PFC_PRI_3 0x08
	#define DCBX_PFC_PRI_4 0x10
	#define DCBX_PFC_PRI_5 0x20
	#define DCBX_PFC_PRI_6 0x40
	#define DCBX_PFC_PRI_7 0x80
#endif
};

struct dcbx_app_priority_entry {
#ifdef __BIG_ENDIAN
	uint16_t  app_id;
	uint8_t  pri_bitmap;
	uint8_t  appBitfield;
	#define DCBX_APP_ENTRY_VALID         0x01
	#define DCBX_APP_ENTRY_SF_MASK       0x30
	#define DCBX_APP_ENTRY_SF_SHIFT      4
	#define DCBX_APP_SF_ETH_TYPE         0x10
	#define DCBX_APP_SF_PORT             0x20
#elif defined(__LITTLE_ENDIAN)
	uint8_t appBitfield;
	#define DCBX_APP_ENTRY_VALID         0x01
	#define DCBX_APP_ENTRY_SF_MASK       0x30
	#define DCBX_APP_ENTRY_SF_SHIFT      4
	#define DCBX_APP_SF_ETH_TYPE         0x10
	#define DCBX_APP_SF_PORT             0x20
	uint8_t  pri_bitmap;
	uint16_t  app_id;
#endif
};


/* FW structure in BE */
struct dcbx_app_priority_feature {
#ifdef __BIG_ENDIAN
	uint8_t reserved;
	uint8_t default_pri;
	uint8_t tc_supported;
	uint8_t enabled;
#elif defined(__LITTLE_ENDIAN)
	uint8_t enabled;
	uint8_t tc_supported;
	uint8_t default_pri;
	uint8_t reserved;
#endif
	struct dcbx_app_priority_entry  app_pri_tbl[DCBX_MAX_APP_PROTOCOL];
};

/* FW structure in BE */
struct dcbx_features {
	/* PG feature */
	struct dcbx_ets_feature ets;
	/* PFC feature */
	struct dcbx_pfc_feature pfc;
	/* APP feature */
	struct dcbx_app_priority_feature app;
};

/* LLDP protocol parameters */
/* FW structure in BE */
struct lldp_params {
#ifdef __BIG_ENDIAN
	uint8_t  msg_fast_tx_interval;
	uint8_t  msg_tx_hold;
	uint8_t  msg_tx_interval;
	uint8_t  admin_status;
	#define LLDP_TX_ONLY  0x01
	#define LLDP_RX_ONLY  0x02
	#define LLDP_TX_RX    0x03
	#define LLDP_DISABLED 0x04
	uint8_t  reserved1;
	uint8_t  tx_fast;
	uint8_t  tx_crd_max;
	uint8_t  tx_crd;
#elif defined(__LITTLE_ENDIAN)
	uint8_t  admin_status;
	#define LLDP_TX_ONLY  0x01
	#define LLDP_RX_ONLY  0x02
	#define LLDP_TX_RX    0x03
	#define LLDP_DISABLED 0x04
	uint8_t  msg_tx_interval;
	uint8_t  msg_tx_hold;
	uint8_t  msg_fast_tx_interval;
	uint8_t  tx_crd;
	uint8_t  tx_crd_max;
	uint8_t  tx_fast;
	uint8_t  reserved1;
#endif
	#define REM_CHASSIS_ID_STAT_LEN 4
	#define REM_PORT_ID_STAT_LEN 4
	/* Holds remote Chassis ID TLV header, subtype and 9B of payload. */
	uint32_t peer_chassis_id[REM_CHASSIS_ID_STAT_LEN];
	/* Holds remote Port ID TLV header, subtype and 9B of payload. */
	uint32_t peer_port_id[REM_PORT_ID_STAT_LEN];
};

struct lldp_dcbx_stat {
	#define LOCAL_CHASSIS_ID_STAT_LEN 2
	#define LOCAL_PORT_ID_STAT_LEN 2
	/* Holds local Chassis ID 8B payload of constant subtype 4. */
	uint32_t local_chassis_id[LOCAL_CHASSIS_ID_STAT_LEN];
	/* Holds local Port ID 8B payload of constant subtype 3. */
	uint32_t local_port_id[LOCAL_PORT_ID_STAT_LEN];
	/* Number of DCBX frames transmitted. */
	uint32_t num_tx_dcbx_pkts;
	/* Number of DCBX frames received. */
	uint32_t num_rx_dcbx_pkts;
};

/* ADMIN MIB - DCBX local machine default configuration. */
struct lldp_admin_mib {
	uint32_t     ver_cfg_flags;
	#define DCBX_ETS_CONFIG_TX_ENABLED       0x00000001
	#define DCBX_PFC_CONFIG_TX_ENABLED       0x00000002
	#define DCBX_APP_CONFIG_TX_ENABLED       0x00000004
	#define DCBX_ETS_RECO_TX_ENABLED         0x00000008
	#define DCBX_ETS_RECO_VALID              0x00000010
	#define DCBX_ETS_WILLING                 0x00000020
	#define DCBX_PFC_WILLING                 0x00000040
	#define DCBX_APP_WILLING                 0x00000080
	#define DCBX_VERSION_CEE                 0x00000100
	#define DCBX_VERSION_IEEE                0x00000200
	#define DCBX_DCBX_ENABLED                0x00000400
	#define DCBX_CEE_VERSION_MASK            0x0000f000
	#define DCBX_CEE_VERSION_SHIFT           12
	#define DCBX_CEE_MAX_VERSION_MASK        0x000f0000
	#define DCBX_CEE_MAX_VERSION_SHIFT       16
	struct dcbx_features     features;
};

/* REMOTE MIB - remote machine DCBX configuration. */
struct lldp_remote_mib {
	uint32_t prefix_seq_num;
	uint32_t flags;
	#define DCBX_ETS_TLV_RX                  0x00000001
	#define DCBX_PFC_TLV_RX                  0x00000002
	#define DCBX_APP_TLV_RX                  0x00000004
	#define DCBX_ETS_RX_ERROR                0x00000010
	#define DCBX_PFC_RX_ERROR                0x00000020
	#define DCBX_APP_RX_ERROR                0x00000040
	#define DCBX_ETS_REM_WILLING             0x00000100
	#define DCBX_PFC_REM_WILLING             0x00000200
	#define DCBX_APP_REM_WILLING             0x00000400
	#define DCBX_REMOTE_ETS_RECO_VALID       0x00001000
	#define DCBX_REMOTE_MIB_VALID            0x00002000
	struct dcbx_features features;
	uint32_t suffix_seq_num;
};

/* LOCAL MIB - operational DCBX configuration - transmitted on Tx LLDPDU. */
struct lldp_local_mib {
	uint32_t prefix_seq_num;
	/* Indicates if there is mismatch with negotiation results. */
	uint32_t error;
	#define DCBX_LOCAL_ETS_ERROR             0x00000001
	#define DCBX_LOCAL_PFC_ERROR             0x00000002
	#define DCBX_LOCAL_APP_ERROR             0x00000004
	#define DCBX_LOCAL_PFC_MISMATCH          0x00000010
	#define DCBX_LOCAL_APP_MISMATCH          0x00000020
	#define DCBX_REMOTE_MIB_ERROR            0x00000040
	#define DCBX_REMOTE_ETS_TLV_NOT_FOUND    0x00000080
	#define DCBX_REMOTE_PFC_TLV_NOT_FOUND    0x00000100
	#define DCBX_REMOTE_APP_TLV_NOT_FOUND    0x00000200
	struct dcbx_features   features;
	uint32_t suffix_seq_num;
};

struct lldp_local_mib_ext {
	uint32_t prefix_seq_num;
	/* APP TLV extension - 16 more entries for negotiation results*/
	struct dcbx_app_priority_entry  app_pri_tbl_ext[DCBX_MAX_APP_PROTOCOL];
	uint32_t suffix_seq_num;
};
/***END OF DCBX STRUCTURES DECLARATIONS***/

/***********************************************************/
/*                         Elink section                   */
/***********************************************************/
#define SHMEM_LINK_CONFIG_SIZE 2
struct shmem_lfa {
	uint32_t req_duplex;
	#define REQ_DUPLEX_PHY0_MASK        0x0000ffff
	#define REQ_DUPLEX_PHY0_SHIFT       0
	#define REQ_DUPLEX_PHY1_MASK        0xffff0000
	#define REQ_DUPLEX_PHY1_SHIFT       16
	uint32_t req_flow_ctrl;
	#define REQ_FLOW_CTRL_PHY0_MASK     0x0000ffff
	#define REQ_FLOW_CTRL_PHY0_SHIFT    0
	#define REQ_FLOW_CTRL_PHY1_MASK     0xffff0000
	#define REQ_FLOW_CTRL_PHY1_SHIFT    16
	uint32_t req_line_speed; /* Also determine AutoNeg */
	#define REQ_LINE_SPD_PHY0_MASK      0x0000ffff
	#define REQ_LINE_SPD_PHY0_SHIFT     0
	#define REQ_LINE_SPD_PHY1_MASK      0xffff0000
	#define REQ_LINE_SPD_PHY1_SHIFT     16
	uint32_t speed_cap_mask[SHMEM_LINK_CONFIG_SIZE];
	uint32_t additional_config;
	#define REQ_FC_AUTO_ADV_MASK        0x0000ffff
	#define REQ_FC_AUTO_ADV0_SHIFT      0
	#define NO_LFA_DUE_TO_DCC_MASK      0x00010000
	uint32_t lfa_sts;
	#define LFA_LINK_FLAP_REASON_OFFSET		0
	#define LFA_LINK_FLAP_REASON_MASK		0x000000ff
		#define LFA_LINK_DOWN			    0x1
		#define LFA_LOOPBACK_ENABLED		0x2
		#define LFA_DUPLEX_MISMATCH		    0x3
		#define LFA_MFW_IS_TOO_OLD		    0x4
		#define LFA_LINK_SPEED_MISMATCH		0x5
		#define LFA_FLOW_CTRL_MISMATCH		0x6
		#define LFA_SPEED_CAP_MISMATCH		0x7
		#define LFA_DCC_LFA_DISABLED		0x8
		#define LFA_EEE_MISMATCH		0x9

	#define LINK_FLAP_AVOIDANCE_COUNT_OFFSET	8
	#define LINK_FLAP_AVOIDANCE_COUNT_MASK		0x0000ff00

	#define LINK_FLAP_COUNT_OFFSET			16
	#define LINK_FLAP_COUNT_MASK			0x00ff0000

	#define LFA_FLAGS_MASK				0xff000000
	#define SHMEM_LFA_DONT_CLEAR_STAT		(1<<24)

};

struct shmem2_region {

	uint32_t size;					/* 0x0000 */

	uint32_t dcc_support;				/* 0x0004 */
	#define SHMEM_DCC_SUPPORT_NONE                      0x00000000
	#define SHMEM_DCC_SUPPORT_DISABLE_ENABLE_PF_TLV     0x00000001
	#define SHMEM_DCC_SUPPORT_BANDWIDTH_ALLOCATION_TLV  0x00000004
	#define SHMEM_DCC_SUPPORT_CHANGE_MAC_ADDRESS_TLV    0x00000008
	#define SHMEM_DCC_SUPPORT_SET_PROTOCOL_TLV          0x00000040
	#define SHMEM_DCC_SUPPORT_SET_PRIORITY_TLV          0x00000080

	uint32_t ext_phy_fw_version2[PORT_MAX];		/* 0x0008 */
	/*
	 * For backwards compatibility, if the mf_cfg_addr does not exist
	 * (the size filed is smaller than 0xc) the mf_cfg resides at the
	 * end of struct shmem_region
	 */
	uint32_t mf_cfg_addr;				/* 0x0010 */
	#define SHMEM_MF_CFG_ADDR_NONE                  0x00000000

	struct fw_flr_mb flr_mb;			/* 0x0014 */
	uint32_t dcbx_lldp_params_offset;			/* 0x0028 */
	#define SHMEM_LLDP_DCBX_PARAMS_NONE             0x00000000
	uint32_t dcbx_neg_res_offset;			/* 0x002c */
	#define SHMEM_DCBX_NEG_RES_NONE			0x00000000
	uint32_t dcbx_remote_mib_offset;			/* 0x0030 */
	#define SHMEM_DCBX_REMOTE_MIB_NONE              0x00000000
	/*
	 * The other shmemX_base_addr holds the other path's shmem address
	 * required for example in case of common phy init, or for path1 to know
	 * the address of mcp debug trace which is located in offset from shmem
	 * of path0
	 */
	uint32_t other_shmem_base_addr;			/* 0x0034 */
	uint32_t other_shmem2_base_addr;			/* 0x0038 */
	/*
	 * mcp_vf_disabled is set by the MCP to indicate the driver about VFs
	 * which were disabled/flred
	 */
	uint32_t mcp_vf_disabled[E2_VF_MAX / 32];		/* 0x003c */

	/*
	 * drv_ack_vf_disabled is set by the PF driver to ack handled disabled
	 * VFs
	 */
	uint32_t drv_ack_vf_disabled[E2_FUNC_MAX][E2_VF_MAX / 32]; /* 0x0044 */

	uint32_t dcbx_lldp_dcbx_stat_offset;			/* 0x0064 */
	#define SHMEM_LLDP_DCBX_STAT_NONE               0x00000000

	/*
	 * edebug_driver_if field is used to transfer messages between edebug
	 * app to the driver through shmem2.
	 *
	 * message format:
	 * bits 0-2 -  function number / instance of driver to perform request
	 * bits 3-5 -  op code / is_ack?
	 * bits 6-63 - data
	 */
	uint32_t edebug_driver_if[2];			/* 0x0068 */
	#define EDEBUG_DRIVER_IF_OP_CODE_GET_PHYS_ADDR  1
	#define EDEBUG_DRIVER_IF_OP_CODE_GET_BUS_ADDR   2
	#define EDEBUG_DRIVER_IF_OP_CODE_DISABLE_STAT   3

	uint32_t nvm_retain_bitmap_addr;			/* 0x0070 */

	/* afex support of that driver */
	uint32_t afex_driver_support;			/* 0x0074 */
	#define SHMEM_AFEX_VERSION_MASK                  0x100f
	#define SHMEM_AFEX_SUPPORTED_VERSION_ONE         0x1001
	#define SHMEM_AFEX_REDUCED_DRV_LOADED            0x8000

	/* driver receives addr in scratchpad to which it should respond */
	uint32_t afex_scratchpad_addr_to_write[E2_FUNC_MAX];

	/*
	 * generic params from MCP to driver (value depends on the msg sent
	 * to driver
	 */
	uint32_t afex_param1_to_driver[E2_FUNC_MAX];		/* 0x0088 */
	uint32_t afex_param2_to_driver[E2_FUNC_MAX];		/* 0x0098 */

	uint32_t swim_base_addr;				/* 0x0108 */
	uint32_t swim_funcs;
	uint32_t swim_main_cb;

	/*
	 * bitmap notifying which VIF profiles stored in nvram are enabled by
	 * switch
	 */
	uint32_t afex_profiles_enabled[2];

	/* generic flags controlled by the driver */
	uint32_t drv_flags;
	#define DRV_FLAGS_DCB_CONFIGURED		0x0
	#define DRV_FLAGS_DCB_CONFIGURATION_ABORTED	0x1
	#define DRV_FLAGS_DCB_MFW_CONFIGURED	0x2

    #define DRV_FLAGS_PORT_MASK	((1 << DRV_FLAGS_DCB_CONFIGURED) | \
			(1 << DRV_FLAGS_DCB_CONFIGURATION_ABORTED) | \
			(1 << DRV_FLAGS_DCB_MFW_CONFIGURED))
	/* Port offset*/
	#define DRV_FLAGS_P0_OFFSET		0
	#define DRV_FLAGS_P1_OFFSET		16
	#define DRV_FLAGS_GET_PORT_OFFSET(_port)	((0 == _port) ? \
						DRV_FLAGS_P0_OFFSET : \
						DRV_FLAGS_P1_OFFSET)

	#define DRV_FLAGS_GET_PORT_MASK(_port)	(DRV_FLAGS_PORT_MASK << \
	DRV_FLAGS_GET_PORT_OFFSET(_port))

	#define DRV_FLAGS_FILED_BY_PORT(_field_bit, _port)	(1 << ( \
	(_field_bit) + DRV_FLAGS_GET_PORT_OFFSET(_port)))

	/* pointer to extended dev_info shared data copied from nvm image */
	uint32_t extended_dev_info_shared_addr;
	uint32_t ncsi_oem_data_addr;

	uint32_t sensor_data_addr;
	uint32_t buffer_block_addr;
	uint32_t sensor_data_req_update_interval;
	uint32_t temperature_in_half_celsius;
	uint32_t glob_struct_in_host;

	uint32_t dcbx_neg_res_ext_offset;
	#define SHMEM_DCBX_NEG_RES_EXT_NONE			0x00000000

	uint32_t drv_capabilities_flag[E2_FUNC_MAX];
	#define DRV_FLAGS_CAPABILITIES_LOADED_SUPPORTED 0x00000001
	#define DRV_FLAGS_CAPABILITIES_LOADED_L2        0x00000002
	#define DRV_FLAGS_CAPABILITIES_LOADED_FCOE      0x00000004
	#define DRV_FLAGS_CAPABILITIES_LOADED_ISCSI     0x00000008

	uint32_t extended_dev_info_shared_cfg_size;

	uint32_t dcbx_en[PORT_MAX];

	/* The offset points to the multi threaded meta structure */
	uint32_t multi_thread_data_offset;

	/* address of DMAable host address holding values from the drivers */
	uint32_t drv_info_host_addr_lo;
	uint32_t drv_info_host_addr_hi;

	/* general values written by the MFW (such as current version) */
	uint32_t drv_info_control;
	#define DRV_INFO_CONTROL_VER_MASK          0x000000ff
	#define DRV_INFO_CONTROL_VER_SHIFT         0
	#define DRV_INFO_CONTROL_OP_CODE_MASK      0x0000ff00
	#define DRV_INFO_CONTROL_OP_CODE_SHIFT     8
	uint32_t ibft_host_addr; /* initialized by option ROM */

	struct eee_remote_vals eee_remote_vals[PORT_MAX];
	uint32_t pf_allocation[E2_FUNC_MAX];
	#define PF_ALLOACTION_MSIX_VECTORS_MASK    0x000000ff /* real value, as PCI config space can show only maximum of 64 vectors */
	#define PF_ALLOACTION_MSIX_VECTORS_SHIFT   0

	/* the status of EEE auto-negotiation
	 * bits 15:0 the configured tx-lpi entry timer value. Depends on bit 31.
	 * bits 19:16 the supported modes for EEE.
	 * bits 23:20 the speeds advertised for EEE.
	 * bits 27:24 the speeds the Link partner advertised for EEE.
	 * The supported/adv. modes in bits 27:19 originate from the
	 * SHMEM_EEE_XXX_ADV definitions (where XXX is replaced by speed).
	 * bit 28 when 1'b1 EEE was requested.
	 * bit 29 when 1'b1 tx lpi was requested.
	 * bit 30 when 1'b1 EEE was negotiated. Tx lpi will be asserted iff
	 * 30:29 are 2'b11.
	 * bit 31 when 1'b0 bits 15:0 contain a PORT_FEAT_CFG_EEE_ define as
	 * value. When 1'b1 those bits contains a value times 16 microseconds.
	 */
	uint32_t eee_status[PORT_MAX];
	#define SHMEM_EEE_TIMER_MASK		   0x0000ffff
	#define SHMEM_EEE_SUPPORTED_MASK	   0x000f0000
	#define SHMEM_EEE_SUPPORTED_SHIFT	   16
	#define SHMEM_EEE_ADV_STATUS_MASK	   0x00f00000
		#define SHMEM_EEE_100M_ADV	   (1U<<0)
		#define SHMEM_EEE_1G_ADV	   (1U<<1)
		#define SHMEM_EEE_10G_ADV	   (1U<<2)
	#define SHMEM_EEE_ADV_STATUS_SHIFT	   20
	#define	SHMEM_EEE_LP_ADV_STATUS_MASK	   0x0f000000
	#define SHMEM_EEE_LP_ADV_STATUS_SHIFT	   24
	#define SHMEM_EEE_REQUESTED_BIT		   0x10000000
	#define SHMEM_EEE_LPI_REQUESTED_BIT	   0x20000000
	#define SHMEM_EEE_ACTIVE_BIT		   0x40000000
	#define SHMEM_EEE_TIME_OUTPUT_BIT	   0x80000000

	uint32_t sizeof_port_stats;

	/* Link Flap Avoidance */
	uint32_t lfa_host_addr[PORT_MAX];

    /* External PHY temperature in deg C. */
	uint32_t extphy_temps_in_celsius;
	#define EXTPHY1_TEMP_MASK                  0x0000ffff
	#define EXTPHY1_TEMP_SHIFT                 0

	uint32_t ocdata_info_addr;			/* Offset 0x148 */
	uint32_t drv_func_info_addr;			/* Offset 0x14C */
	uint32_t drv_func_info_size;			/* Offset 0x150 */
	uint32_t link_attr_sync[PORT_MAX];		/* Offset 0x154 */
	#define LINK_ATTR_SYNC_KR2_ENABLE	(1<<0)

	uint32_t ibft_host_addr_hi;  /* Initialize by uEFI ROM */
};


struct emac_stats {
	uint32_t     rx_stat_ifhcinoctets;
	uint32_t     rx_stat_ifhcinbadoctets;
	uint32_t     rx_stat_etherstatsfragments;
	uint32_t     rx_stat_ifhcinucastpkts;
	uint32_t     rx_stat_ifhcinmulticastpkts;
	uint32_t     rx_stat_ifhcinbroadcastpkts;
	uint32_t     rx_stat_dot3statsfcserrors;
	uint32_t     rx_stat_dot3statsalignmenterrors;
	uint32_t     rx_stat_dot3statscarriersenseerrors;
	uint32_t     rx_stat_xonpauseframesreceived;
	uint32_t     rx_stat_xoffpauseframesreceived;
	uint32_t     rx_stat_maccontrolframesreceived;
	uint32_t     rx_stat_xoffstateentered;
	uint32_t     rx_stat_dot3statsframestoolong;
	uint32_t     rx_stat_etherstatsjabbers;
	uint32_t     rx_stat_etherstatsundersizepkts;
	uint32_t     rx_stat_etherstatspkts64octets;
	uint32_t     rx_stat_etherstatspkts65octetsto127octets;
	uint32_t     rx_stat_etherstatspkts128octetsto255octets;
	uint32_t     rx_stat_etherstatspkts256octetsto511octets;
	uint32_t     rx_stat_etherstatspkts512octetsto1023octets;
	uint32_t     rx_stat_etherstatspkts1024octetsto1522octets;
	uint32_t     rx_stat_etherstatspktsover1522octets;

	uint32_t     rx_stat_falsecarriererrors;

	uint32_t     tx_stat_ifhcoutoctets;
	uint32_t     tx_stat_ifhcoutbadoctets;
	uint32_t     tx_stat_etherstatscollisions;
	uint32_t     tx_stat_outxonsent;
	uint32_t     tx_stat_outxoffsent;
	uint32_t     tx_stat_flowcontroldone;
	uint32_t     tx_stat_dot3statssinglecollisionframes;
	uint32_t     tx_stat_dot3statsmultiplecollisionframes;
	uint32_t     tx_stat_dot3statsdeferredtransmissions;
	uint32_t     tx_stat_dot3statsexcessivecollisions;
	uint32_t     tx_stat_dot3statslatecollisions;
	uint32_t     tx_stat_ifhcoutucastpkts;
	uint32_t     tx_stat_ifhcoutmulticastpkts;
	uint32_t     tx_stat_ifhcoutbroadcastpkts;
	uint32_t     tx_stat_etherstatspkts64octets;
	uint32_t     tx_stat_etherstatspkts65octetsto127octets;
	uint32_t     tx_stat_etherstatspkts128octetsto255octets;
	uint32_t     tx_stat_etherstatspkts256octetsto511octets;
	uint32_t     tx_stat_etherstatspkts512octetsto1023octets;
	uint32_t     tx_stat_etherstatspkts1024octetsto1522octets;
	uint32_t     tx_stat_etherstatspktsover1522octets;
	uint32_t     tx_stat_dot3statsinternalmactransmiterrors;
};


struct bmac1_stats {
	uint32_t	tx_stat_gtpkt_lo;
	uint32_t	tx_stat_gtpkt_hi;
	uint32_t	tx_stat_gtxpf_lo;
	uint32_t	tx_stat_gtxpf_hi;
	uint32_t	tx_stat_gtfcs_lo;
	uint32_t	tx_stat_gtfcs_hi;
	uint32_t	tx_stat_gtmca_lo;
	uint32_t	tx_stat_gtmca_hi;
	uint32_t	tx_stat_gtbca_lo;
	uint32_t	tx_stat_gtbca_hi;
	uint32_t	tx_stat_gtfrg_lo;
	uint32_t	tx_stat_gtfrg_hi;
	uint32_t	tx_stat_gtovr_lo;
	uint32_t	tx_stat_gtovr_hi;
	uint32_t	tx_stat_gt64_lo;
	uint32_t	tx_stat_gt64_hi;
	uint32_t	tx_stat_gt127_lo;
	uint32_t	tx_stat_gt127_hi;
	uint32_t	tx_stat_gt255_lo;
	uint32_t	tx_stat_gt255_hi;
	uint32_t	tx_stat_gt511_lo;
	uint32_t	tx_stat_gt511_hi;
	uint32_t	tx_stat_gt1023_lo;
	uint32_t	tx_stat_gt1023_hi;
	uint32_t	tx_stat_gt1518_lo;
	uint32_t	tx_stat_gt1518_hi;
	uint32_t	tx_stat_gt2047_lo;
	uint32_t	tx_stat_gt2047_hi;
	uint32_t	tx_stat_gt4095_lo;
	uint32_t	tx_stat_gt4095_hi;
	uint32_t	tx_stat_gt9216_lo;
	uint32_t	tx_stat_gt9216_hi;
	uint32_t	tx_stat_gt16383_lo;
	uint32_t	tx_stat_gt16383_hi;
	uint32_t	tx_stat_gtmax_lo;
	uint32_t	tx_stat_gtmax_hi;
	uint32_t	tx_stat_gtufl_lo;
	uint32_t	tx_stat_gtufl_hi;
	uint32_t	tx_stat_gterr_lo;
	uint32_t	tx_stat_gterr_hi;
	uint32_t	tx_stat_gtbyt_lo;
	uint32_t	tx_stat_gtbyt_hi;

	uint32_t	rx_stat_gr64_lo;
	uint32_t	rx_stat_gr64_hi;
	uint32_t	rx_stat_gr127_lo;
	uint32_t	rx_stat_gr127_hi;
	uint32_t	rx_stat_gr255_lo;
	uint32_t	rx_stat_gr255_hi;
	uint32_t	rx_stat_gr511_lo;
	uint32_t	rx_stat_gr511_hi;
	uint32_t	rx_stat_gr1023_lo;
	uint32_t	rx_stat_gr1023_hi;
	uint32_t	rx_stat_gr1518_lo;
	uint32_t	rx_stat_gr1518_hi;
	uint32_t	rx_stat_gr2047_lo;
	uint32_t	rx_stat_gr2047_hi;
	uint32_t	rx_stat_gr4095_lo;
	uint32_t	rx_stat_gr4095_hi;
	uint32_t	rx_stat_gr9216_lo;
	uint32_t	rx_stat_gr9216_hi;
	uint32_t	rx_stat_gr16383_lo;
	uint32_t	rx_stat_gr16383_hi;
	uint32_t	rx_stat_grmax_lo;
	uint32_t	rx_stat_grmax_hi;
	uint32_t	rx_stat_grpkt_lo;
	uint32_t	rx_stat_grpkt_hi;
	uint32_t	rx_stat_grfcs_lo;
	uint32_t	rx_stat_grfcs_hi;
	uint32_t	rx_stat_grmca_lo;
	uint32_t	rx_stat_grmca_hi;
	uint32_t	rx_stat_grbca_lo;
	uint32_t	rx_stat_grbca_hi;
	uint32_t	rx_stat_grxcf_lo;
	uint32_t	rx_stat_grxcf_hi;
	uint32_t	rx_stat_grxpf_lo;
	uint32_t	rx_stat_grxpf_hi;
	uint32_t	rx_stat_grxuo_lo;
	uint32_t	rx_stat_grxuo_hi;
	uint32_t	rx_stat_grjbr_lo;
	uint32_t	rx_stat_grjbr_hi;
	uint32_t	rx_stat_grovr_lo;
	uint32_t	rx_stat_grovr_hi;
	uint32_t	rx_stat_grflr_lo;
	uint32_t	rx_stat_grflr_hi;
	uint32_t	rx_stat_grmeg_lo;
	uint32_t	rx_stat_grmeg_hi;
	uint32_t	rx_stat_grmeb_lo;
	uint32_t	rx_stat_grmeb_hi;
	uint32_t	rx_stat_grbyt_lo;
	uint32_t	rx_stat_grbyt_hi;
	uint32_t	rx_stat_grund_lo;
	uint32_t	rx_stat_grund_hi;
	uint32_t	rx_stat_grfrg_lo;
	uint32_t	rx_stat_grfrg_hi;
	uint32_t	rx_stat_grerb_lo;
	uint32_t	rx_stat_grerb_hi;
	uint32_t	rx_stat_grfre_lo;
	uint32_t	rx_stat_grfre_hi;
	uint32_t	rx_stat_gripj_lo;
	uint32_t	rx_stat_gripj_hi;
};

struct bmac2_stats {
	uint32_t	tx_stat_gtpk_lo; /* gtpok */
	uint32_t	tx_stat_gtpk_hi; /* gtpok */
	uint32_t	tx_stat_gtxpf_lo; /* gtpf */
	uint32_t	tx_stat_gtxpf_hi; /* gtpf */
	uint32_t	tx_stat_gtpp_lo; /* NEW BMAC2 */
	uint32_t	tx_stat_gtpp_hi; /* NEW BMAC2 */
	uint32_t	tx_stat_gtfcs_lo;
	uint32_t	tx_stat_gtfcs_hi;
	uint32_t	tx_stat_gtuca_lo; /* NEW BMAC2 */
	uint32_t	tx_stat_gtuca_hi; /* NEW BMAC2 */
	uint32_t	tx_stat_gtmca_lo;
	uint32_t	tx_stat_gtmca_hi;
	uint32_t	tx_stat_gtbca_lo;
	uint32_t	tx_stat_gtbca_hi;
	uint32_t	tx_stat_gtovr_lo;
	uint32_t	tx_stat_gtovr_hi;
	uint32_t	tx_stat_gtfrg_lo;
	uint32_t	tx_stat_gtfrg_hi;
	uint32_t	tx_stat_gtpkt1_lo; /* gtpkt */
	uint32_t	tx_stat_gtpkt1_hi; /* gtpkt */
	uint32_t	tx_stat_gt64_lo;
	uint32_t	tx_stat_gt64_hi;
	uint32_t	tx_stat_gt127_lo;
	uint32_t	tx_stat_gt127_hi;
	uint32_t	tx_stat_gt255_lo;
	uint32_t	tx_stat_gt255_hi;
	uint32_t	tx_stat_gt511_lo;
	uint32_t	tx_stat_gt511_hi;
	uint32_t	tx_stat_gt1023_lo;
	uint32_t	tx_stat_gt1023_hi;
	uint32_t	tx_stat_gt1518_lo;
	uint32_t	tx_stat_gt1518_hi;
	uint32_t	tx_stat_gt2047_lo;
	uint32_t	tx_stat_gt2047_hi;
	uint32_t	tx_stat_gt4095_lo;
	uint32_t	tx_stat_gt4095_hi;
	uint32_t	tx_stat_gt9216_lo;
	uint32_t	tx_stat_gt9216_hi;
	uint32_t	tx_stat_gt16383_lo;
	uint32_t	tx_stat_gt16383_hi;
	uint32_t	tx_stat_gtmax_lo;
	uint32_t	tx_stat_gtmax_hi;
	uint32_t	tx_stat_gtufl_lo;
	uint32_t	tx_stat_gtufl_hi;
	uint32_t	tx_stat_gterr_lo;
	uint32_t	tx_stat_gterr_hi;
	uint32_t	tx_stat_gtbyt_lo;
	uint32_t	tx_stat_gtbyt_hi;

	uint32_t	rx_stat_gr64_lo;
	uint32_t	rx_stat_gr64_hi;
	uint32_t	rx_stat_gr127_lo;
	uint32_t	rx_stat_gr127_hi;
	uint32_t	rx_stat_gr255_lo;
	uint32_t	rx_stat_gr255_hi;
	uint32_t	rx_stat_gr511_lo;
	uint32_t	rx_stat_gr511_hi;
	uint32_t	rx_stat_gr1023_lo;
	uint32_t	rx_stat_gr1023_hi;
	uint32_t	rx_stat_gr1518_lo;
	uint32_t	rx_stat_gr1518_hi;
	uint32_t	rx_stat_gr2047_lo;
	uint32_t	rx_stat_gr2047_hi;
	uint32_t	rx_stat_gr4095_lo;
	uint32_t	rx_stat_gr4095_hi;
	uint32_t	rx_stat_gr9216_lo;
	uint32_t	rx_stat_gr9216_hi;
	uint32_t	rx_stat_gr16383_lo;
	uint32_t	rx_stat_gr16383_hi;
	uint32_t	rx_stat_grmax_lo;
	uint32_t	rx_stat_grmax_hi;
	uint32_t	rx_stat_grpkt_lo;
	uint32_t	rx_stat_grpkt_hi;
	uint32_t	rx_stat_grfcs_lo;
	uint32_t	rx_stat_grfcs_hi;
	uint32_t	rx_stat_gruca_lo;
	uint32_t	rx_stat_gruca_hi;
	uint32_t	rx_stat_grmca_lo;
	uint32_t	rx_stat_grmca_hi;
	uint32_t	rx_stat_grbca_lo;
	uint32_t	rx_stat_grbca_hi;
	uint32_t	rx_stat_grxpf_lo; /* grpf */
	uint32_t	rx_stat_grxpf_hi; /* grpf */
	uint32_t	rx_stat_grpp_lo;
	uint32_t	rx_stat_grpp_hi;
	uint32_t	rx_stat_grxuo_lo; /* gruo */
	uint32_t	rx_stat_grxuo_hi; /* gruo */
	uint32_t	rx_stat_grjbr_lo;
	uint32_t	rx_stat_grjbr_hi;
	uint32_t	rx_stat_grovr_lo;
	uint32_t	rx_stat_grovr_hi;
	uint32_t	rx_stat_grxcf_lo; /* grcf */
	uint32_t	rx_stat_grxcf_hi; /* grcf */
	uint32_t	rx_stat_grflr_lo;
	uint32_t	rx_stat_grflr_hi;
	uint32_t	rx_stat_grpok_lo;
	uint32_t	rx_stat_grpok_hi;
	uint32_t	rx_stat_grmeg_lo;
	uint32_t	rx_stat_grmeg_hi;
	uint32_t	rx_stat_grmeb_lo;
	uint32_t	rx_stat_grmeb_hi;
	uint32_t	rx_stat_grbyt_lo;
	uint32_t	rx_stat_grbyt_hi;
	uint32_t	rx_stat_grund_lo;
	uint32_t	rx_stat_grund_hi;
	uint32_t	rx_stat_grfrg_lo;
	uint32_t	rx_stat_grfrg_hi;
	uint32_t	rx_stat_grerb_lo; /* grerrbyt */
	uint32_t	rx_stat_grerb_hi; /* grerrbyt */
	uint32_t	rx_stat_grfre_lo; /* grfrerr */
	uint32_t	rx_stat_grfre_hi; /* grfrerr */
	uint32_t	rx_stat_gripj_lo;
	uint32_t	rx_stat_gripj_hi;
};

struct mstat_stats {
	struct {
		/* OTE MSTAT on E3 has a bug where this register's contents are
		 * actually tx_gtxpok + tx_gtxpf + (possibly)tx_gtxpp
		 */
		uint32_t tx_gtxpok_lo;
		uint32_t tx_gtxpok_hi;
		uint32_t tx_gtxpf_lo;
		uint32_t tx_gtxpf_hi;
		uint32_t tx_gtxpp_lo;
		uint32_t tx_gtxpp_hi;
		uint32_t tx_gtfcs_lo;
		uint32_t tx_gtfcs_hi;
		uint32_t tx_gtuca_lo;
		uint32_t tx_gtuca_hi;
		uint32_t tx_gtmca_lo;
		uint32_t tx_gtmca_hi;
		uint32_t tx_gtgca_lo;
		uint32_t tx_gtgca_hi;
		uint32_t tx_gtpkt_lo;
		uint32_t tx_gtpkt_hi;
		uint32_t tx_gt64_lo;
		uint32_t tx_gt64_hi;
		uint32_t tx_gt127_lo;
		uint32_t tx_gt127_hi;
		uint32_t tx_gt255_lo;
		uint32_t tx_gt255_hi;
		uint32_t tx_gt511_lo;
		uint32_t tx_gt511_hi;
		uint32_t tx_gt1023_lo;
		uint32_t tx_gt1023_hi;
		uint32_t tx_gt1518_lo;
		uint32_t tx_gt1518_hi;
		uint32_t tx_gt2047_lo;
		uint32_t tx_gt2047_hi;
		uint32_t tx_gt4095_lo;
		uint32_t tx_gt4095_hi;
		uint32_t tx_gt9216_lo;
		uint32_t tx_gt9216_hi;
		uint32_t tx_gt16383_lo;
		uint32_t tx_gt16383_hi;
		uint32_t tx_gtufl_lo;
		uint32_t tx_gtufl_hi;
		uint32_t tx_gterr_lo;
		uint32_t tx_gterr_hi;
		uint32_t tx_gtbyt_lo;
		uint32_t tx_gtbyt_hi;
		uint32_t tx_collisions_lo;
		uint32_t tx_collisions_hi;
		uint32_t tx_singlecollision_lo;
		uint32_t tx_singlecollision_hi;
		uint32_t tx_multiplecollisions_lo;
		uint32_t tx_multiplecollisions_hi;
		uint32_t tx_deferred_lo;
		uint32_t tx_deferred_hi;
		uint32_t tx_excessivecollisions_lo;
		uint32_t tx_excessivecollisions_hi;
		uint32_t tx_latecollisions_lo;
		uint32_t tx_latecollisions_hi;
	} stats_tx;

	struct {
		uint32_t rx_gr64_lo;
		uint32_t rx_gr64_hi;
		uint32_t rx_gr127_lo;
		uint32_t rx_gr127_hi;
		uint32_t rx_gr255_lo;
		uint32_t rx_gr255_hi;
		uint32_t rx_gr511_lo;
		uint32_t rx_gr511_hi;
		uint32_t rx_gr1023_lo;
		uint32_t rx_gr1023_hi;
		uint32_t rx_gr1518_lo;
		uint32_t rx_gr1518_hi;
		uint32_t rx_gr2047_lo;
		uint32_t rx_gr2047_hi;
		uint32_t rx_gr4095_lo;
		uint32_t rx_gr4095_hi;
		uint32_t rx_gr9216_lo;
		uint32_t rx_gr9216_hi;
		uint32_t rx_gr16383_lo;
		uint32_t rx_gr16383_hi;
		uint32_t rx_grpkt_lo;
		uint32_t rx_grpkt_hi;
		uint32_t rx_grfcs_lo;
		uint32_t rx_grfcs_hi;
		uint32_t rx_gruca_lo;
		uint32_t rx_gruca_hi;
		uint32_t rx_grmca_lo;
		uint32_t rx_grmca_hi;
		uint32_t rx_grbca_lo;
		uint32_t rx_grbca_hi;
		uint32_t rx_grxpf_lo;
		uint32_t rx_grxpf_hi;
		uint32_t rx_grxpp_lo;
		uint32_t rx_grxpp_hi;
		uint32_t rx_grxuo_lo;
		uint32_t rx_grxuo_hi;
		uint32_t rx_grovr_lo;
		uint32_t rx_grovr_hi;
		uint32_t rx_grxcf_lo;
		uint32_t rx_grxcf_hi;
		uint32_t rx_grflr_lo;
		uint32_t rx_grflr_hi;
		uint32_t rx_grpok_lo;
		uint32_t rx_grpok_hi;
		uint32_t rx_grbyt_lo;
		uint32_t rx_grbyt_hi;
		uint32_t rx_grund_lo;
		uint32_t rx_grund_hi;
		uint32_t rx_grfrg_lo;
		uint32_t rx_grfrg_hi;
		uint32_t rx_grerb_lo;
		uint32_t rx_grerb_hi;
		uint32_t rx_grfre_lo;
		uint32_t rx_grfre_hi;

		uint32_t rx_alignmenterrors_lo;
		uint32_t rx_alignmenterrors_hi;
		uint32_t rx_falsecarrier_lo;
		uint32_t rx_falsecarrier_hi;
		uint32_t rx_llfcmsgcnt_lo;
		uint32_t rx_llfcmsgcnt_hi;
	} stats_rx;
};

union mac_stats {
	struct emac_stats	emac_stats;
	struct bmac1_stats	bmac1_stats;
	struct bmac2_stats	bmac2_stats;
	struct mstat_stats	mstat_stats;
};


struct mac_stx {
	/* in_bad_octets */
	uint32_t     rx_stat_ifhcinbadoctets_hi;
	uint32_t     rx_stat_ifhcinbadoctets_lo;

	/* out_bad_octets */
	uint32_t     tx_stat_ifhcoutbadoctets_hi;
	uint32_t     tx_stat_ifhcoutbadoctets_lo;

	/* crc_receive_errors */
	uint32_t     rx_stat_dot3statsfcserrors_hi;
	uint32_t     rx_stat_dot3statsfcserrors_lo;
	/* alignment_errors */
	uint32_t     rx_stat_dot3statsalignmenterrors_hi;
	uint32_t     rx_stat_dot3statsalignmenterrors_lo;
	/* carrier_sense_errors */
	uint32_t     rx_stat_dot3statscarriersenseerrors_hi;
	uint32_t     rx_stat_dot3statscarriersenseerrors_lo;
	/* false_carrier_detections */
	uint32_t     rx_stat_falsecarriererrors_hi;
	uint32_t     rx_stat_falsecarriererrors_lo;

	/* runt_packets_received */
	uint32_t     rx_stat_etherstatsundersizepkts_hi;
	uint32_t     rx_stat_etherstatsundersizepkts_lo;
	/* jabber_packets_received */
	uint32_t     rx_stat_dot3statsframestoolong_hi;
	uint32_t     rx_stat_dot3statsframestoolong_lo;

	/* error_runt_packets_received */
	uint32_t     rx_stat_etherstatsfragments_hi;
	uint32_t     rx_stat_etherstatsfragments_lo;
	/* error_jabber_packets_received */
	uint32_t     rx_stat_etherstatsjabbers_hi;
	uint32_t     rx_stat_etherstatsjabbers_lo;

	/* control_frames_received */
	uint32_t     rx_stat_maccontrolframesreceived_hi;
	uint32_t     rx_stat_maccontrolframesreceived_lo;
	uint32_t     rx_stat_mac_xpf_hi;
	uint32_t     rx_stat_mac_xpf_lo;
	uint32_t     rx_stat_mac_xcf_hi;
	uint32_t     rx_stat_mac_xcf_lo;

	/* xoff_state_entered */
	uint32_t     rx_stat_xoffstateentered_hi;
	uint32_t     rx_stat_xoffstateentered_lo;
	/* pause_xon_frames_received */
	uint32_t     rx_stat_xonpauseframesreceived_hi;
	uint32_t     rx_stat_xonpauseframesreceived_lo;
	/* pause_xoff_frames_received */
	uint32_t     rx_stat_xoffpauseframesreceived_hi;
	uint32_t     rx_stat_xoffpauseframesreceived_lo;
	/* pause_xon_frames_transmitted */
	uint32_t     tx_stat_outxonsent_hi;
	uint32_t     tx_stat_outxonsent_lo;
	/* pause_xoff_frames_transmitted */
	uint32_t     tx_stat_outxoffsent_hi;
	uint32_t     tx_stat_outxoffsent_lo;
	/* flow_control_done */
	uint32_t     tx_stat_flowcontroldone_hi;
	uint32_t     tx_stat_flowcontroldone_lo;

	/* ether_stats_collisions */
	uint32_t     tx_stat_etherstatscollisions_hi;
	uint32_t     tx_stat_etherstatscollisions_lo;
	/* single_collision_transmit_frames */
	uint32_t     tx_stat_dot3statssinglecollisionframes_hi;
	uint32_t     tx_stat_dot3statssinglecollisionframes_lo;
	/* multiple_collision_transmit_frames */
	uint32_t     tx_stat_dot3statsmultiplecollisionframes_hi;
	uint32_t     tx_stat_dot3statsmultiplecollisionframes_lo;
	/* deferred_transmissions */
	uint32_t     tx_stat_dot3statsdeferredtransmissions_hi;
	uint32_t     tx_stat_dot3statsdeferredtransmissions_lo;
	/* excessive_collision_frames */
	uint32_t     tx_stat_dot3statsexcessivecollisions_hi;
	uint32_t     tx_stat_dot3statsexcessivecollisions_lo;
	/* late_collision_frames */
	uint32_t     tx_stat_dot3statslatecollisions_hi;
	uint32_t     tx_stat_dot3statslatecollisions_lo;

	/* frames_transmitted_64_bytes */
	uint32_t     tx_stat_etherstatspkts64octets_hi;
	uint32_t     tx_stat_etherstatspkts64octets_lo;
	/* frames_transmitted_65_127_bytes */
	uint32_t     tx_stat_etherstatspkts65octetsto127octets_hi;
	uint32_t     tx_stat_etherstatspkts65octetsto127octets_lo;
	/* frames_transmitted_128_255_bytes */
	uint32_t     tx_stat_etherstatspkts128octetsto255octets_hi;
	uint32_t     tx_stat_etherstatspkts128octetsto255octets_lo;
	/* frames_transmitted_256_511_bytes */
	uint32_t     tx_stat_etherstatspkts256octetsto511octets_hi;
	uint32_t     tx_stat_etherstatspkts256octetsto511octets_lo;
	/* frames_transmitted_512_1023_bytes */
	uint32_t     tx_stat_etherstatspkts512octetsto1023octets_hi;
	uint32_t     tx_stat_etherstatspkts512octetsto1023octets_lo;
	/* frames_transmitted_1024_1522_bytes */
	uint32_t     tx_stat_etherstatspkts1024octetsto1522octets_hi;
	uint32_t     tx_stat_etherstatspkts1024octetsto1522octets_lo;
	/* frames_transmitted_1523_9022_bytes */
	uint32_t     tx_stat_etherstatspktsover1522octets_hi;
	uint32_t     tx_stat_etherstatspktsover1522octets_lo;
	uint32_t     tx_stat_mac_2047_hi;
	uint32_t     tx_stat_mac_2047_lo;
	uint32_t     tx_stat_mac_4095_hi;
	uint32_t     tx_stat_mac_4095_lo;
	uint32_t     tx_stat_mac_9216_hi;
	uint32_t     tx_stat_mac_9216_lo;
	uint32_t     tx_stat_mac_16383_hi;
	uint32_t     tx_stat_mac_16383_lo;

	/* internal_mac_transmit_errors */
	uint32_t     tx_stat_dot3statsinternalmactransmiterrors_hi;
	uint32_t     tx_stat_dot3statsinternalmactransmiterrors_lo;

	/* if_out_discards */
	uint32_t     tx_stat_mac_ufl_hi;
	uint32_t     tx_stat_mac_ufl_lo;
};


#define MAC_STX_IDX_MAX                     2

struct host_port_stats {
	uint32_t            host_port_stats_counter;

	struct mac_stx mac_stx[MAC_STX_IDX_MAX];

	uint32_t            brb_drop_hi;
	uint32_t            brb_drop_lo;

	uint32_t            not_used; /* obsolete as of MFW 7.2.1 */

	uint32_t            pfc_frames_tx_hi;
	uint32_t            pfc_frames_tx_lo;
	uint32_t            pfc_frames_rx_hi;
	uint32_t            pfc_frames_rx_lo;

	uint32_t            eee_lpi_count_hi;
	uint32_t            eee_lpi_count_lo;
};


struct host_func_stats {
	uint32_t     host_func_stats_start;

	uint32_t     total_bytes_received_hi;
	uint32_t     total_bytes_received_lo;

	uint32_t     total_bytes_transmitted_hi;
	uint32_t     total_bytes_transmitted_lo;

	uint32_t     total_unicast_packets_received_hi;
	uint32_t     total_unicast_packets_received_lo;

	uint32_t     total_multicast_packets_received_hi;
	uint32_t     total_multicast_packets_received_lo;

	uint32_t     total_broadcast_packets_received_hi;
	uint32_t     total_broadcast_packets_received_lo;

	uint32_t     total_unicast_packets_transmitted_hi;
	uint32_t     total_unicast_packets_transmitted_lo;

	uint32_t     total_multicast_packets_transmitted_hi;
	uint32_t     total_multicast_packets_transmitted_lo;

	uint32_t     total_broadcast_packets_transmitted_hi;
	uint32_t     total_broadcast_packets_transmitted_lo;

	uint32_t     valid_bytes_received_hi;
	uint32_t     valid_bytes_received_lo;

	uint32_t     host_func_stats_end;
};

/* VIC definitions */
#define VICSTATST_UIF_INDEX 2

/*
 * stats collected for afex.
 * NOTE: structure is exactly as expected to be received by the switch.
 *       order must remain exactly as is unless protocol changes !
 */
struct afex_stats {
	uint32_t tx_unicast_frames_hi;
	uint32_t tx_unicast_frames_lo;
	uint32_t tx_unicast_bytes_hi;
	uint32_t tx_unicast_bytes_lo;
	uint32_t tx_multicast_frames_hi;
	uint32_t tx_multicast_frames_lo;
	uint32_t tx_multicast_bytes_hi;
	uint32_t tx_multicast_bytes_lo;
	uint32_t tx_broadcast_frames_hi;
	uint32_t tx_broadcast_frames_lo;
	uint32_t tx_broadcast_bytes_hi;
	uint32_t tx_broadcast_bytes_lo;
	uint32_t tx_frames_discarded_hi;
	uint32_t tx_frames_discarded_lo;
	uint32_t tx_frames_dropped_hi;
	uint32_t tx_frames_dropped_lo;

	uint32_t rx_unicast_frames_hi;
	uint32_t rx_unicast_frames_lo;
	uint32_t rx_unicast_bytes_hi;
	uint32_t rx_unicast_bytes_lo;
	uint32_t rx_multicast_frames_hi;
	uint32_t rx_multicast_frames_lo;
	uint32_t rx_multicast_bytes_hi;
	uint32_t rx_multicast_bytes_lo;
	uint32_t rx_broadcast_frames_hi;
	uint32_t rx_broadcast_frames_lo;
	uint32_t rx_broadcast_bytes_hi;
	uint32_t rx_broadcast_bytes_lo;
	uint32_t rx_frames_discarded_hi;
	uint32_t rx_frames_discarded_lo;
	uint32_t rx_frames_dropped_hi;
	uint32_t rx_frames_dropped_lo;
};

/* To maintain backward compatibility between FW and drivers, new elements */
/* should be added to the end of the structure. */

/* Per  Port Statistics    */
struct port_info {
	uint32_t size; /* size of this structure (i.e. sizeof(port_info))  */
	uint32_t enabled;      /* 0 =Disabled, 1= Enabled */
	uint32_t link_speed;   /* multiplier of 100Mb */
	uint32_t wol_support;  /* WoL Support (i.e. Non-Zero if WOL supported ) */
	uint32_t flow_control; /* 802.3X Flow Ctrl. 0=off 1=RX 2=TX 3=RX&TX.*/
	uint32_t flex10;     /* Flex10 mode enabled. non zero = yes */
	uint32_t rx_drops;  /* RX Discards. Counters roll over, never reset */
	uint32_t rx_errors; /* RX Errors. Physical Port Stats L95, All PFs and NC-SI.
				   This is flagged by Consumer as an error. */
	uint32_t rx_uncast_lo;   /* RX Unicast Packets. Free running counters: */
	uint32_t rx_uncast_hi;   /* RX Unicast Packets. Free running counters: */
	uint32_t rx_mcast_lo;    /* RX Multicast Packets  */
	uint32_t rx_mcast_hi;    /* RX Multicast Packets  */
	uint32_t rx_bcast_lo;    /* RX Broadcast Packets  */
	uint32_t rx_bcast_hi;    /* RX Broadcast Packets  */
	uint32_t tx_uncast_lo;   /* TX Unicast Packets   */
	uint32_t tx_uncast_hi;   /* TX Unicast Packets   */
	uint32_t tx_mcast_lo;    /* TX Multicast Packets  */
	uint32_t tx_mcast_hi;    /* TX Multicast Packets  */
	uint32_t tx_bcast_lo;    /* TX Broadcast Packets  */
	uint32_t tx_bcast_hi;    /* TX Broadcast Packets  */
	uint32_t tx_errors;      /* TX Errors              */
	uint32_t tx_discards;    /* TX Discards          */
	uint32_t rx_frames_lo;   /* RX Frames received  */
	uint32_t rx_frames_hi;   /* RX Frames received  */
	uint32_t rx_bytes_lo;    /* RX Bytes received    */
	uint32_t rx_bytes_hi;    /* RX Bytes received    */
	uint32_t tx_frames_lo;   /* TX Frames sent      */
	uint32_t tx_frames_hi;   /* TX Frames sent      */
	uint32_t tx_bytes_lo;    /* TX Bytes sent        */
	uint32_t tx_bytes_hi;    /* TX Bytes sent        */
	uint32_t link_status;  /* Port P Link Status. 1:0 bit for port enabled.
				1:1 bit for link good,
				2:1 Set if link changed between last poll. */
	uint32_t tx_pfc_frames_lo;   /* PFC Frames sent.    */
	uint32_t tx_pfc_frames_hi;   /* PFC Frames sent.    */
	uint32_t rx_pfc_frames_lo;   /* PFC Frames Received. */
	uint32_t rx_pfc_frames_hi;   /* PFC Frames Received. */
};


#define BCM_5710_FW_MAJOR_VERSION			7
#define BCM_5710_FW_MINOR_VERSION			8
#define BCM_5710_FW_REVISION_VERSION		51
#define BCM_5710_FW_ENGINEERING_VERSION		0
#define BCM_5710_FW_COMPILE_FLAGS			1


/*
 * attention bits $$KEEP_ENDIANNESS$$
 */
struct atten_sp_status_block
{
	uint32_t attn_bits /* 16 bit of attention signal lines */;
	uint32_t attn_bits_ack /* 16 bit of attention signal ack */;
	uint8_t status_block_id /* status block id */;
	uint8_t reserved0 /* resreved for padding */;
	uint16_t attn_bits_index /* attention bits running index */;
	uint32_t reserved1 /* resreved for padding */;
};


/*
 * The eth aggregative context of Cstorm
 */
struct cstorm_eth_ag_context
{
	uint32_t __reserved0[10];
};


/*
 * dmae command structure
 */
struct dmae_command
{
	uint32_t opcode;
#define DMAE_COMMAND_SRC (0x1<<0) /* BitField opcode	Whether the source is the PCIe or the GRC. 0- The source is the PCIe 1- The source is the GRC. */
#define DMAE_COMMAND_SRC_SHIFT 0
#define DMAE_COMMAND_DST (0x3<<1) /* BitField opcode	The destination of the DMA can be: 0-None 1-PCIe 2-GRC 3-None  */
#define DMAE_COMMAND_DST_SHIFT 1
#define DMAE_COMMAND_C_DST (0x1<<3) /* BitField opcode	The destination of the completion: 0-PCIe 1-GRC */
#define DMAE_COMMAND_C_DST_SHIFT 3
#define DMAE_COMMAND_C_TYPE_ENABLE (0x1<<4) /* BitField opcode	Whether to write a completion word to the completion destination: 0-Do not write a completion word 1-Write the completion word  */
#define DMAE_COMMAND_C_TYPE_ENABLE_SHIFT 4
#define DMAE_COMMAND_C_TYPE_CRC_ENABLE (0x1<<5) /* BitField opcode	Whether to write a CRC word to the completion destination 0-Do not write a CRC word 1-Write a CRC word  */
#define DMAE_COMMAND_C_TYPE_CRC_ENABLE_SHIFT 5
#define DMAE_COMMAND_C_TYPE_CRC_OFFSET (0x7<<6) /* BitField opcode	The CRC word should be taken from the DMAE GRC space from address 9+X, where X is the value in these bits. */
#define DMAE_COMMAND_C_TYPE_CRC_OFFSET_SHIFT 6
#define DMAE_COMMAND_ENDIANITY (0x3<<9) /* BitField opcode	swapping mode. */
#define DMAE_COMMAND_ENDIANITY_SHIFT 9
#define DMAE_COMMAND_PORT (0x1<<11) /* BitField opcode	Which network port ID to present to the PCI request interface */
#define DMAE_COMMAND_PORT_SHIFT 11
#define DMAE_COMMAND_CRC_RESET (0x1<<12) /* BitField opcode	reset crc result */
#define DMAE_COMMAND_CRC_RESET_SHIFT 12
#define DMAE_COMMAND_SRC_RESET (0x1<<13) /* BitField opcode	reset source address in next go */
#define DMAE_COMMAND_SRC_RESET_SHIFT 13
#define DMAE_COMMAND_DST_RESET (0x1<<14) /* BitField opcode	reset dest address in next go */
#define DMAE_COMMAND_DST_RESET_SHIFT 14
#define DMAE_COMMAND_E1HVN (0x3<<15) /* BitField opcode	vnic number E2 and onwards source vnic */
#define DMAE_COMMAND_E1HVN_SHIFT 15
#define DMAE_COMMAND_DST_VN (0x3<<17) /* BitField opcode	E2 and onwards dest vnic */
#define DMAE_COMMAND_DST_VN_SHIFT 17
#define DMAE_COMMAND_C_FUNC (0x1<<19) /* BitField opcode	E2 and onwards which function gets the completion src_vn(e1hvn)-0 dst_vn-1 */
#define DMAE_COMMAND_C_FUNC_SHIFT 19
#define DMAE_COMMAND_ERR_POLICY (0x3<<20) /* BitField opcode	E2 and onwards what to do when theres a completion and a PCI error regular-0 error indication-1 no completion-2 */
#define DMAE_COMMAND_ERR_POLICY_SHIFT 20
#define DMAE_COMMAND_RESERVED0 (0x3FF<<22) /* BitField opcode	 */
#define DMAE_COMMAND_RESERVED0_SHIFT 22
	uint32_t src_addr_lo /* source address low/grc address */;
	uint32_t src_addr_hi /* source address hi */;
	uint32_t dst_addr_lo /* dest address low/grc address */;
	uint32_t dst_addr_hi /* dest address hi */;
#if defined(__BIG_ENDIAN)
	uint16_t opcode_iov;
#define DMAE_COMMAND_SRC_VFID (0x3F<<0) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	source VF id */
#define DMAE_COMMAND_SRC_VFID_SHIFT 0
#define DMAE_COMMAND_SRC_VFPF (0x1<<6) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	selects the source function PF-0, VF-1 */
#define DMAE_COMMAND_SRC_VFPF_SHIFT 6
#define DMAE_COMMAND_RESERVED1 (0x1<<7) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	 */
#define DMAE_COMMAND_RESERVED1_SHIFT 7
#define DMAE_COMMAND_DST_VFID (0x3F<<8) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	destination VF id */
#define DMAE_COMMAND_DST_VFID_SHIFT 8
#define DMAE_COMMAND_DST_VFPF (0x1<<14) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	selects the destination function PF-0, VF-1 */
#define DMAE_COMMAND_DST_VFPF_SHIFT 14
#define DMAE_COMMAND_RESERVED2 (0x1<<15) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	 */
#define DMAE_COMMAND_RESERVED2_SHIFT 15
	uint16_t len /* copy length */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t len /* copy length */;
	uint16_t opcode_iov;
#define DMAE_COMMAND_SRC_VFID (0x3F<<0) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	source VF id */
#define DMAE_COMMAND_SRC_VFID_SHIFT 0
#define DMAE_COMMAND_SRC_VFPF (0x1<<6) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	selects the source function PF-0, VF-1 */
#define DMAE_COMMAND_SRC_VFPF_SHIFT 6
#define DMAE_COMMAND_RESERVED1 (0x1<<7) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	 */
#define DMAE_COMMAND_RESERVED1_SHIFT 7
#define DMAE_COMMAND_DST_VFID (0x3F<<8) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	destination VF id */
#define DMAE_COMMAND_DST_VFID_SHIFT 8
#define DMAE_COMMAND_DST_VFPF (0x1<<14) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	selects the destination function PF-0, VF-1 */
#define DMAE_COMMAND_DST_VFPF_SHIFT 14
#define DMAE_COMMAND_RESERVED2 (0x1<<15) /* BitField opcode_iovE2 and onward, set to 0 for backward compatibility	 */
#define DMAE_COMMAND_RESERVED2_SHIFT 15
#endif
	uint32_t comp_addr_lo /* completion address low/grc address */;
	uint32_t comp_addr_hi /* completion address hi */;
	uint32_t comp_val /* value to write to completion address */;
	uint32_t crc32 /* crc32 result */;
	uint32_t crc32_c /* crc32_c result */;
#if defined(__BIG_ENDIAN)
	uint16_t crc16_c /* crc16_c result */;
	uint16_t crc16 /* crc16 result */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t crc16 /* crc16 result */;
	uint16_t crc16_c /* crc16_c result */;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t reserved3;
	uint16_t crc_t10 /* crc_t10 result */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t crc_t10 /* crc_t10 result */;
	uint16_t reserved3;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t xsum8 /* checksum8 result */;
	uint16_t xsum16 /* checksum16 result */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t xsum16 /* checksum16 result */;
	uint16_t xsum8 /* checksum8 result */;
#endif
};


/*
 * common data for all protocols
 */
struct doorbell_hdr
{
	uint8_t header;
#define DOORBELL_HDR_RX (0x1<<0) /* BitField header	1 for rx doorbell, 0 for tx doorbell */
#define DOORBELL_HDR_RX_SHIFT 0
#define DOORBELL_HDR_DB_TYPE (0x1<<1) /* BitField header	0 for normal doorbell, 1 for advertise wnd doorbell */
#define DOORBELL_HDR_DB_TYPE_SHIFT 1
#define DOORBELL_HDR_DPM_SIZE (0x3<<2) /* BitField header	rdma tx only: DPM transaction size specifier (64/128/256/512 bytes) */
#define DOORBELL_HDR_DPM_SIZE_SHIFT 2
#define DOORBELL_HDR_CONN_TYPE (0xF<<4) /* BitField header	connection type */
#define DOORBELL_HDR_CONN_TYPE_SHIFT 4
};

/*
 * Ethernet doorbell
 */
struct eth_tx_doorbell
{
#if defined(__BIG_ENDIAN)
	uint16_t npackets /* number of data bytes that were added in the doorbell */;
	uint8_t params;
#define ETH_TX_DOORBELL_NUM_BDS (0x3F<<0) /* BitField params	number of buffer descriptors that were added in the doorbell */
#define ETH_TX_DOORBELL_NUM_BDS_SHIFT 0
#define ETH_TX_DOORBELL_RESERVED_TX_FIN_FLAG (0x1<<6) /* BitField params	tx fin command flag */
#define ETH_TX_DOORBELL_RESERVED_TX_FIN_FLAG_SHIFT 6
#define ETH_TX_DOORBELL_SPARE (0x1<<7) /* BitField params	doorbell queue spare flag */
#define ETH_TX_DOORBELL_SPARE_SHIFT 7
	struct doorbell_hdr hdr;
#elif defined(__LITTLE_ENDIAN)
	struct doorbell_hdr hdr;
	uint8_t params;
#define ETH_TX_DOORBELL_NUM_BDS (0x3F<<0) /* BitField params	number of buffer descriptors that were added in the doorbell */
#define ETH_TX_DOORBELL_NUM_BDS_SHIFT 0
#define ETH_TX_DOORBELL_RESERVED_TX_FIN_FLAG (0x1<<6) /* BitField params	tx fin command flag */
#define ETH_TX_DOORBELL_RESERVED_TX_FIN_FLAG_SHIFT 6
#define ETH_TX_DOORBELL_SPARE (0x1<<7) /* BitField params	doorbell queue spare flag */
#define ETH_TX_DOORBELL_SPARE_SHIFT 7
	uint16_t npackets /* number of data bytes that were added in the doorbell */;
#endif
};


/*
 * 3 lines. status block $$KEEP_ENDIANNESS$$
 */
struct hc_status_block_e1x
{
	uint16_t index_values[HC_SB_MAX_INDICES_E1X] /* indices reported by cstorm */;
	uint16_t running_index[HC_SB_MAX_SM] /* Status Block running indices */;
	uint32_t rsrv[11];
};

/*
 * host status block
 */
struct host_hc_status_block_e1x
{
	struct hc_status_block_e1x sb /* fast path indices */;
};


/*
 * 3 lines. status block $$KEEP_ENDIANNESS$$
 */
struct hc_status_block_e2
{
	uint16_t index_values[HC_SB_MAX_INDICES_E2] /* indices reported by cstorm */;
	uint16_t running_index[HC_SB_MAX_SM] /* Status Block running indices */;
	uint32_t reserved[11];
};

/*
 * host status block
 */
struct host_hc_status_block_e2
{
	struct hc_status_block_e2 sb /* fast path indices */;
};


/*
 * 5 lines. slow-path status block $$KEEP_ENDIANNESS$$
 */
struct hc_sp_status_block
{
	uint16_t index_values[HC_SP_SB_MAX_INDICES] /* indices reported by cstorm */;
	uint16_t running_index /* Status Block running index */;
	uint16_t rsrv;
	uint32_t rsrv1;
};

/*
 * host status block
 */
struct host_sp_status_block
{
	struct atten_sp_status_block atten_status_block /* attention bits section */;
	struct hc_sp_status_block sp_sb /* slow path indices */;
};


/*
 * IGU driver acknowledgment register
 */
struct igu_ack_register
{
#if defined(__BIG_ENDIAN)
	uint16_t sb_id_and_flags;
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID (0x1F<<0) /* BitField sb_id_and_flags	0-15: non default status blocks, 16: default status block */
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT 0
#define IGU_ACK_REGISTER_STORM_ID (0x7<<5) /* BitField sb_id_and_flags	0-3:storm id, 4: attn status block (valid in default sb only) */
#define IGU_ACK_REGISTER_STORM_ID_SHIFT 5
#define IGU_ACK_REGISTER_UPDATE_INDEX (0x1<<8) /* BitField sb_id_and_flags	if set, acknowledges status block index */
#define IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT 8
#define IGU_ACK_REGISTER_INTERRUPT_MODE (0x3<<9) /* BitField sb_id_and_flags	interrupt enable/disable/nop: use IGU_INT_xxx constants */
#define IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT 9
#define IGU_ACK_REGISTER_RESERVED (0x1F<<11) /* BitField sb_id_and_flags	 */
#define IGU_ACK_REGISTER_RESERVED_SHIFT 11
	uint16_t status_block_index /* status block index acknowledgement */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t status_block_index /* status block index acknowledgement */;
	uint16_t sb_id_and_flags;
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID (0x1F<<0) /* BitField sb_id_and_flags	0-15: non default status blocks, 16: default status block */
#define IGU_ACK_REGISTER_STATUS_BLOCK_ID_SHIFT 0
#define IGU_ACK_REGISTER_STORM_ID (0x7<<5) /* BitField sb_id_and_flags	0-3:storm id, 4: attn status block (valid in default sb only) */
#define IGU_ACK_REGISTER_STORM_ID_SHIFT 5
#define IGU_ACK_REGISTER_UPDATE_INDEX (0x1<<8) /* BitField sb_id_and_flags	if set, acknowledges status block index */
#define IGU_ACK_REGISTER_UPDATE_INDEX_SHIFT 8
#define IGU_ACK_REGISTER_INTERRUPT_MODE (0x3<<9) /* BitField sb_id_and_flags	interrupt enable/disable/nop: use IGU_INT_xxx constants */
#define IGU_ACK_REGISTER_INTERRUPT_MODE_SHIFT 9
#define IGU_ACK_REGISTER_RESERVED (0x1F<<11) /* BitField sb_id_and_flags	 */
#define IGU_ACK_REGISTER_RESERVED_SHIFT 11
#endif
};


/*
 * IGU driver acknowledgement register
 */
struct igu_backward_compatible
{
	uint32_t sb_id_and_flags;
#define IGU_BACKWARD_COMPATIBLE_SB_INDEX (0xFFFF<<0) /* BitField sb_id_and_flags	 */
#define IGU_BACKWARD_COMPATIBLE_SB_INDEX_SHIFT 0
#define IGU_BACKWARD_COMPATIBLE_SB_SELECT (0x1F<<16) /* BitField sb_id_and_flags	 */
#define IGU_BACKWARD_COMPATIBLE_SB_SELECT_SHIFT 16
#define IGU_BACKWARD_COMPATIBLE_SEGMENT_ACCESS (0x7<<21) /* BitField sb_id_and_flags	0-3:storm id, 4: attn status block (valid in default sb only) */
#define IGU_BACKWARD_COMPATIBLE_SEGMENT_ACCESS_SHIFT 21
#define IGU_BACKWARD_COMPATIBLE_BUPDATE (0x1<<24) /* BitField sb_id_and_flags	if set, acknowledges status block index */
#define IGU_BACKWARD_COMPATIBLE_BUPDATE_SHIFT 24
#define IGU_BACKWARD_COMPATIBLE_ENABLE_INT (0x3<<25) /* BitField sb_id_and_flags	interrupt enable/disable/nop: use IGU_INT_xxx constants */
#define IGU_BACKWARD_COMPATIBLE_ENABLE_INT_SHIFT 25
#define IGU_BACKWARD_COMPATIBLE_RESERVED_0 (0x1F<<27) /* BitField sb_id_and_flags	 */
#define IGU_BACKWARD_COMPATIBLE_RESERVED_0_SHIFT 27
	uint32_t reserved_2;
};


/*
 * IGU driver acknowledgement register
 */
struct igu_regular
{
	uint32_t sb_id_and_flags;
#define IGU_REGULAR_SB_INDEX (0xFFFFF<<0) /* BitField sb_id_and_flags	 */
#define IGU_REGULAR_SB_INDEX_SHIFT 0
#define IGU_REGULAR_RESERVED0 (0x1<<20) /* BitField sb_id_and_flags	 */
#define IGU_REGULAR_RESERVED0_SHIFT 20
#define IGU_REGULAR_SEGMENT_ACCESS (0x7<<21) /* BitField sb_id_and_flags	21-23 (use enum igu_seg_access) */
#define IGU_REGULAR_SEGMENT_ACCESS_SHIFT 21
#define IGU_REGULAR_BUPDATE (0x1<<24) /* BitField sb_id_and_flags	 */
#define IGU_REGULAR_BUPDATE_SHIFT 24
#define IGU_REGULAR_ENABLE_INT (0x3<<25) /* BitField sb_id_and_flags	interrupt enable/disable/nop (use enum igu_int_cmd) */
#define IGU_REGULAR_ENABLE_INT_SHIFT 25
#define IGU_REGULAR_RESERVED_1 (0x1<<27) /* BitField sb_id_and_flags	 */
#define IGU_REGULAR_RESERVED_1_SHIFT 27
#define IGU_REGULAR_CLEANUP_TYPE (0x3<<28) /* BitField sb_id_and_flags	 */
#define IGU_REGULAR_CLEANUP_TYPE_SHIFT 28
#define IGU_REGULAR_CLEANUP_SET (0x1<<30) /* BitField sb_id_and_flags	 */
#define IGU_REGULAR_CLEANUP_SET_SHIFT 30
#define IGU_REGULAR_BCLEANUP (0x1<<31) /* BitField sb_id_and_flags	 */
#define IGU_REGULAR_BCLEANUP_SHIFT 31
	uint32_t reserved_2;
};

/*
 * IGU driver acknowledgement register
 */
union igu_consprod_reg
{
	struct igu_regular regular;
	struct igu_backward_compatible backward_compatible;
};


/*
 * Igu control commands
 */
enum igu_ctrl_cmd
{
	IGU_CTRL_CMD_TYPE_RD,
	IGU_CTRL_CMD_TYPE_WR,
	MAX_IGU_CTRL_CMD};


/*
 * Control register for the IGU command register
 */
struct igu_ctrl_reg
{
	uint32_t ctrl_data;
#define IGU_CTRL_REG_ADDRESS (0xFFF<<0) /* BitField ctrl_data	 */
#define IGU_CTRL_REG_ADDRESS_SHIFT 0
#define IGU_CTRL_REG_FID (0x7F<<12) /* BitField ctrl_data	 */
#define IGU_CTRL_REG_FID_SHIFT 12
#define IGU_CTRL_REG_RESERVED (0x1<<19) /* BitField ctrl_data	 */
#define IGU_CTRL_REG_RESERVED_SHIFT 19
#define IGU_CTRL_REG_TYPE (0x1<<20) /* BitField ctrl_data	 (use enum igu_ctrl_cmd) */
#define IGU_CTRL_REG_TYPE_SHIFT 20
#define IGU_CTRL_REG_UNUSED (0x7FF<<21) /* BitField ctrl_data	 */
#define IGU_CTRL_REG_UNUSED_SHIFT 21
};


/*
 * Igu interrupt command
 */
enum igu_int_cmd
{
	IGU_INT_ENABLE,
	IGU_INT_DISABLE,
	IGU_INT_NOP,
	IGU_INT_NOP2,
	MAX_IGU_INT_CMD};


/*
 * Igu segments
 */
enum igu_seg_access
{
	IGU_SEG_ACCESS_NORM,
	IGU_SEG_ACCESS_DEF,
	IGU_SEG_ACCESS_ATTN,
	MAX_IGU_SEG_ACCESS};


/*
 * Parser parsing flags field
 */
struct parsing_flags
{
	uint16_t flags;
#define PARSING_FLAGS_ETHERNET_ADDRESS_TYPE (0x1<<0) /* BitField flagscontext flags	0=non-unicast, 1=unicast (use enum prs_flags_eth_addr_type) */
#define PARSING_FLAGS_ETHERNET_ADDRESS_TYPE_SHIFT 0
#define PARSING_FLAGS_VLAN (0x1<<1) /* BitField flagscontext flags	0 or 1 */
#define PARSING_FLAGS_VLAN_SHIFT 1
#define PARSING_FLAGS_EXTRA_VLAN (0x1<<2) /* BitField flagscontext flags	0 or 1 */
#define PARSING_FLAGS_EXTRA_VLAN_SHIFT 2
#define PARSING_FLAGS_OVER_ETHERNET_PROTOCOL (0x3<<3) /* BitField flagscontext flags	0=un-known, 1=Ipv4, 2=Ipv6,3=LLC SNAP un-known. LLC SNAP here refers only to LLC/SNAP packets that do not have Ipv4 or Ipv6 above them. Ipv4 and Ipv6 indications are even if they are over LLC/SNAP and not directly over Ethernet (use enum prs_flags_over_eth) */
#define PARSING_FLAGS_OVER_ETHERNET_PROTOCOL_SHIFT 3
#define PARSING_FLAGS_IP_OPTIONS (0x1<<5) /* BitField flagscontext flags	0=no IP options / extension headers. 1=IP options / extension header exist */
#define PARSING_FLAGS_IP_OPTIONS_SHIFT 5
#define PARSING_FLAGS_FRAGMENTATION_STATUS (0x1<<6) /* BitField flagscontext flags	0=non-fragmented, 1=fragmented */
#define PARSING_FLAGS_FRAGMENTATION_STATUS_SHIFT 6
#define PARSING_FLAGS_OVER_IP_PROTOCOL (0x3<<7) /* BitField flagscontext flags	0=un-known, 1=TCP, 2=UDP (use enum prs_flags_over_ip) */
#define PARSING_FLAGS_OVER_IP_PROTOCOL_SHIFT 7
#define PARSING_FLAGS_PURE_ACK_INDICATION (0x1<<9) /* BitField flagscontext flags	0=packet with data, 1=pure-ACK (use enum prs_flags_ack_type) */
#define PARSING_FLAGS_PURE_ACK_INDICATION_SHIFT 9
#define PARSING_FLAGS_TCP_OPTIONS_EXIST (0x1<<10) /* BitField flagscontext flags	0=no TCP options. 1=TCP options */
#define PARSING_FLAGS_TCP_OPTIONS_EXIST_SHIFT 10
#define PARSING_FLAGS_TIME_STAMP_EXIST_FLAG (0x1<<11) /* BitField flagscontext flags	According to the TCP header options parsing */
#define PARSING_FLAGS_TIME_STAMP_EXIST_FLAG_SHIFT 11
#define PARSING_FLAGS_CONNECTION_MATCH (0x1<<12) /* BitField flagscontext flags	connection match in searcher indication */
#define PARSING_FLAGS_CONNECTION_MATCH_SHIFT 12
#define PARSING_FLAGS_LLC_SNAP (0x1<<13) /* BitField flagscontext flags	LLC SNAP indication */
#define PARSING_FLAGS_LLC_SNAP_SHIFT 13
#define PARSING_FLAGS_RESERVED0 (0x3<<14) /* BitField flagscontext flags	 */
#define PARSING_FLAGS_RESERVED0_SHIFT 14
};


/*
 * Parsing flags for TCP ACK type
 */
enum prs_flags_ack_type
{
	PRS_FLAG_PUREACK_PIGGY,
	PRS_FLAG_PUREACK_PURE,
	MAX_PRS_FLAGS_ACK_TYPE};


/*
 * Parsing flags for Ethernet address type
 */
enum prs_flags_eth_addr_type
{
	PRS_FLAG_ETHTYPE_NON_UNICAST,
	PRS_FLAG_ETHTYPE_UNICAST,
	MAX_PRS_FLAGS_ETH_ADDR_TYPE};


/*
 * Parsing flags for over-ethernet protocol
 */
enum prs_flags_over_eth
{
	PRS_FLAG_OVERETH_UNKNOWN,
	PRS_FLAG_OVERETH_IPV4,
	PRS_FLAG_OVERETH_IPV6,
	PRS_FLAG_OVERETH_LLCSNAP_UNKNOWN,
	MAX_PRS_FLAGS_OVER_ETH};


/*
 * Parsing flags for over-IP protocol
 */
enum prs_flags_over_ip
{
	PRS_FLAG_OVERIP_UNKNOWN,
	PRS_FLAG_OVERIP_TCP,
	PRS_FLAG_OVERIP_UDP,
	MAX_PRS_FLAGS_OVER_IP};


/*
 * SDM operation gen command (generate aggregative interrupt)
 */
struct sdm_op_gen
{
	uint32_t command;
#define SDM_OP_GEN_COMP_PARAM (0x1F<<0) /* BitField commandcomp_param and comp_type	thread ID/aggr interrupt number/counter depending on the completion type */
#define SDM_OP_GEN_COMP_PARAM_SHIFT 0
#define SDM_OP_GEN_COMP_TYPE (0x7<<5) /* BitField commandcomp_param and comp_type	Direct messages to CM / PCI switch are not supported in operation_gen completion */
#define SDM_OP_GEN_COMP_TYPE_SHIFT 5
#define SDM_OP_GEN_AGG_VECT_IDX (0xFF<<8) /* BitField commandcomp_param and comp_type	bit index in aggregated interrupt vector */
#define SDM_OP_GEN_AGG_VECT_IDX_SHIFT 8
#define SDM_OP_GEN_AGG_VECT_IDX_VALID (0x1<<16) /* BitField commandcomp_param and comp_type	 */
#define SDM_OP_GEN_AGG_VECT_IDX_VALID_SHIFT 16
#define SDM_OP_GEN_RESERVED (0x7FFF<<17) /* BitField commandcomp_param and comp_type	 */
#define SDM_OP_GEN_RESERVED_SHIFT 17
};


/*
 * Timers connection context
 */
struct timers_block_context
{
	uint32_t __reserved_0 /* data of client 0 of the timers block*/;
	uint32_t __reserved_1 /* data of client 1 of the timers block*/;
	uint32_t __reserved_2 /* data of client 2 of the timers block*/;
	uint32_t flags;
#define __TIMERS_BLOCK_CONTEXT_NUM_OF_ACTIVE_TIMERS (0x3<<0) /* BitField flagscontext flags	number of active timers running */
#define __TIMERS_BLOCK_CONTEXT_NUM_OF_ACTIVE_TIMERS_SHIFT 0
#define TIMERS_BLOCK_CONTEXT_CONN_VALID_FLG (0x1<<2) /* BitField flagscontext flags	flag: is connection valid (should be set by driver to 1 in toe/iscsi connections) */
#define TIMERS_BLOCK_CONTEXT_CONN_VALID_FLG_SHIFT 2
#define __TIMERS_BLOCK_CONTEXT_RESERVED0 (0x1FFFFFFF<<3) /* BitField flagscontext flags	 */
#define __TIMERS_BLOCK_CONTEXT_RESERVED0_SHIFT 3
};


/*
 * The eth aggregative context of Tstorm
 */
struct tstorm_eth_ag_context
{
	uint32_t __reserved0[14];
};


/*
 * The eth aggregative context of Ustorm
 */
struct ustorm_eth_ag_context
{
	uint32_t __reserved0;
#if defined(__BIG_ENDIAN)
	uint8_t cdu_usage /* Will be used by the CDU for validation of the CID/connection type on doorbells. */;
	uint8_t __reserved2;
	uint16_t __reserved1;
#elif defined(__LITTLE_ENDIAN)
	uint16_t __reserved1;
	uint8_t __reserved2;
	uint8_t cdu_usage /* Will be used by the CDU for validation of the CID/connection type on doorbells. */;
#endif
	uint32_t __reserved3[6];
};


/*
 * The eth aggregative context of Xstorm
 */
struct xstorm_eth_ag_context
{
	uint32_t reserved0;
#if defined(__BIG_ENDIAN)
	uint8_t cdu_reserved /* Used by the CDU for validation and debugging */;
	uint8_t reserved2;
	uint16_t reserved1;
#elif defined(__LITTLE_ENDIAN)
	uint16_t reserved1;
	uint8_t reserved2;
	uint8_t cdu_reserved /* Used by the CDU for validation and debugging */;
#endif
	uint32_t reserved3[30];
};


/*
 * doorbell message sent to the chip
 */
struct doorbell
{
#if defined(__BIG_ENDIAN)
	uint16_t zero_fill2 /* driver must zero this field! */;
	uint8_t zero_fill1 /* driver must zero this field! */;
	struct doorbell_hdr header;
#elif defined(__LITTLE_ENDIAN)
	struct doorbell_hdr header;
	uint8_t zero_fill1 /* driver must zero this field! */;
	uint16_t zero_fill2 /* driver must zero this field! */;
#endif
};


/*
 * doorbell message sent to the chip
 */
struct doorbell_set_prod
{
#if defined(__BIG_ENDIAN)
	uint16_t prod /* Producer index to be set */;
	uint8_t zero_fill1 /* driver must zero this field! */;
	struct doorbell_hdr header;
#elif defined(__LITTLE_ENDIAN)
	struct doorbell_hdr header;
	uint8_t zero_fill1 /* driver must zero this field! */;
	uint16_t prod /* Producer index to be set */;
#endif
};


struct regpair
{
	uint32_t lo /* low word for reg-pair */;
	uint32_t hi /* high word for reg-pair */;
};


struct regpair_native
{
	uint32_t lo /* low word for reg-pair */;
	uint32_t hi /* high word for reg-pair */;
};


/*
 * Classify rule opcodes in E2/E3
 */
enum classify_rule
{
	CLASSIFY_RULE_OPCODE_MAC /* Add/remove a MAC address */,
	CLASSIFY_RULE_OPCODE_VLAN /* Add/remove a VLAN */,
	CLASSIFY_RULE_OPCODE_PAIR /* Add/remove a MAC-VLAN pair */,
	MAX_CLASSIFY_RULE};


/*
 * Classify rule types in E2/E3
 */
enum classify_rule_action_type
{
	CLASSIFY_RULE_REMOVE,
	CLASSIFY_RULE_ADD,
	MAX_CLASSIFY_RULE_ACTION_TYPE};


/*
 * client init ramrod data $$KEEP_ENDIANNESS$$
 */
struct client_init_general_data
{
	uint8_t client_id /* client_id */;
	uint8_t statistics_counter_id /* statistics counter id */;
	uint8_t statistics_en_flg /* statistics en flg */;
	uint8_t is_fcoe_flg /* is this an fcoe connection. (1 bit is used) */;
	uint8_t activate_flg /* if 0 - the client is deactivate else the client is activate client (1 bit is used) */;
	uint8_t sp_client_id /* the slow path rings client Id. */;
	uint16_t mtu /* Host MTU from client config */;
	uint8_t statistics_zero_flg /* if set FW will reset the statistic counter of this client */;
	uint8_t func_id /* PCI function ID (0-71) */;
	uint8_t cos /* The connection cos, if applicable */;
	uint8_t traffic_type;
	uint32_t reserved0;
};


/*
 * client init rx data $$KEEP_ENDIANNESS$$
 */
struct client_init_rx_data
{
	uint8_t tpa_en;
#define CLIENT_INIT_RX_DATA_TPA_EN_IPV4 (0x1<<0) /* BitField tpa_entpa_enable	tpa enable flg ipv4 */
#define CLIENT_INIT_RX_DATA_TPA_EN_IPV4_SHIFT 0
#define CLIENT_INIT_RX_DATA_TPA_EN_IPV6 (0x1<<1) /* BitField tpa_entpa_enable	tpa enable flg ipv6 */
#define CLIENT_INIT_RX_DATA_TPA_EN_IPV6_SHIFT 1
#define CLIENT_INIT_RX_DATA_TPA_MODE (0x1<<2) /* BitField tpa_entpa_enable	tpa mode (LRO or GRO) (use enum tpa_mode) */
#define CLIENT_INIT_RX_DATA_TPA_MODE_SHIFT 2
#define CLIENT_INIT_RX_DATA_RESERVED5 (0x1F<<3) /* BitField tpa_entpa_enable	 */
#define CLIENT_INIT_RX_DATA_RESERVED5_SHIFT 3
	uint8_t vmqueue_mode_en_flg /* If set, working in VMQueue mode (always consume one sge) */;
	uint8_t extra_data_over_sgl_en_flg /* if set, put over sgl data from end of input message */;
	uint8_t cache_line_alignment_log_size /* The log size of cache line alignment in bytes. Must be a power of 2. */;
	uint8_t enable_dynamic_hc /* If set, dynamic HC is enabled */;
	uint8_t max_sges_for_packet /* The maximal number of SGEs that can be used for one packet. depends on MTU and SGE size. must be 0 if SGEs are disabled */;
	uint8_t client_qzone_id /* used in E2 only, to specify the HW queue zone ID used for this client rx producers */;
	uint8_t drop_ip_cs_err_flg /* If set, this client drops packets with IP checksum error */;
	uint8_t drop_tcp_cs_err_flg /* If set, this client drops packets with TCP checksum error */;
	uint8_t drop_ttl0_flg /* If set, this client drops packets with TTL=0 */;
	uint8_t drop_udp_cs_err_flg /* If set, this client drops packets with UDP checksum error */;
	uint8_t inner_vlan_removal_enable_flg /* If set, inner VLAN removal is enabled for this client */;
	uint8_t outer_vlan_removal_enable_flg /* If set, outer VLAN removal is enabled for this client */;
	uint8_t status_block_id /* rx status block id */;
	uint8_t rx_sb_index_number /* status block indices */;
	uint8_t dont_verify_rings_pause_thr_flg /* If set, the rings pause thresholds will not be verified by firmware. */;
	uint8_t max_tpa_queues /* maximal TPA queues allowed for this client */;
	uint8_t silent_vlan_removal_flg /* if set, and the vlan is equal to requested vlan according to mask, the vlan will be remove without notifying the driver */;
	uint16_t max_bytes_on_bd /* Maximum bytes that can be placed on a BD. The BD allocated size should include 2 more bytes (ip alignment) and alignment size (in case the address is not aligned) */;
	uint16_t sge_buff_size /* Size of the buffers pointed by SGEs */;
	uint8_t approx_mcast_engine_id /* In Everest2, if is_approx_mcast is set, this field specified which approximate multicast engine is associate with this client */;
	uint8_t rss_engine_id /* In Everest2, if rss_mode is set, this field specified which RSS engine is associate with this client */;
	struct regpair bd_page_base /* BD page base address at the host */;
	struct regpair sge_page_base /* SGE page base address at the host */;
	struct regpair cqe_page_base /* Completion queue base address */;
	uint8_t is_leading_rss;
	uint8_t is_approx_mcast;
	uint16_t max_agg_size /* maximal size for the aggregated TPA packets, reprted by the host */;
	uint16_t state;
#define CLIENT_INIT_RX_DATA_UCAST_DROP_ALL (0x1<<0) /* BitField staterx filters state	drop all unicast packets */
#define CLIENT_INIT_RX_DATA_UCAST_DROP_ALL_SHIFT 0
#define CLIENT_INIT_RX_DATA_UCAST_ACCEPT_ALL (0x1<<1) /* BitField staterx filters state	accept all unicast packets (subject to vlan) */
#define CLIENT_INIT_RX_DATA_UCAST_ACCEPT_ALL_SHIFT 1
#define CLIENT_INIT_RX_DATA_UCAST_ACCEPT_UNMATCHED (0x1<<2) /* BitField staterx filters state	accept all unmatched unicast packets (subject to vlan) */
#define CLIENT_INIT_RX_DATA_UCAST_ACCEPT_UNMATCHED_SHIFT 2
#define CLIENT_INIT_RX_DATA_MCAST_DROP_ALL (0x1<<3) /* BitField staterx filters state	drop all multicast packets */
#define CLIENT_INIT_RX_DATA_MCAST_DROP_ALL_SHIFT 3
#define CLIENT_INIT_RX_DATA_MCAST_ACCEPT_ALL (0x1<<4) /* BitField staterx filters state	accept all multicast packets (subject to vlan) */
#define CLIENT_INIT_RX_DATA_MCAST_ACCEPT_ALL_SHIFT 4
#define CLIENT_INIT_RX_DATA_BCAST_ACCEPT_ALL (0x1<<5) /* BitField staterx filters state	accept all broadcast packets (subject to vlan) */
#define CLIENT_INIT_RX_DATA_BCAST_ACCEPT_ALL_SHIFT 5
#define CLIENT_INIT_RX_DATA_ACCEPT_ANY_VLAN (0x1<<6) /* BitField staterx filters state	accept packets matched only by MAC (without checking vlan) */
#define CLIENT_INIT_RX_DATA_ACCEPT_ANY_VLAN_SHIFT 6
#define CLIENT_INIT_RX_DATA_RESERVED2 (0x1FF<<7) /* BitField staterx filters state	 */
#define CLIENT_INIT_RX_DATA_RESERVED2_SHIFT 7
	uint16_t cqe_pause_thr_low /* number of remaining cqes under which, we send pause message */;
	uint16_t cqe_pause_thr_high /* number of remaining cqes above which, we send un-pause message */;
	uint16_t bd_pause_thr_low /* number of remaining bds under which, we send pause message */;
	uint16_t bd_pause_thr_high /* number of remaining bds above which, we send un-pause message */;
	uint16_t sge_pause_thr_low /* number of remaining sges under which, we send pause message */;
	uint16_t sge_pause_thr_high /* number of remaining sges above which, we send un-pause message */;
	uint16_t rx_cos_mask /* the bits that will be set on pfc/ safc paket whith will be genratet when this ring is full. for regular flow control set this to 1 */;
	uint16_t silent_vlan_value /* The vlan to compare, in case, silent vlan is set */;
	uint16_t silent_vlan_mask /* The vlan mask, in case, silent vlan is set */;
	uint32_t reserved6[2];
};

/*
 * client init tx data $$KEEP_ENDIANNESS$$
 */
struct client_init_tx_data
{
	uint8_t enforce_security_flg /* if set, security checks will be made for this connection */;
	uint8_t tx_status_block_id /* the number of status block to update */;
	uint8_t tx_sb_index_number /* the index to use inside the status block */;
	uint8_t tss_leading_client_id /* client ID of the leading TSS client, for TX classification source knock out */;
	uint8_t tx_switching_flg /* if set, tx switching will be done to packets on this connection */;
	uint8_t anti_spoofing_flg /* if set, anti spoofing check will be done to packets on this connection */;
	uint16_t default_vlan /* default vlan tag (id+pri). (valid if default_vlan_flg is set) */;
	struct regpair tx_bd_page_base /* BD page base address at the host for TxBdCons */;
	uint16_t state;
#define CLIENT_INIT_TX_DATA_UCAST_ACCEPT_ALL (0x1<<0) /* BitField statetx filters state	accept all unicast packets (subject to vlan) */
#define CLIENT_INIT_TX_DATA_UCAST_ACCEPT_ALL_SHIFT 0
#define CLIENT_INIT_TX_DATA_MCAST_ACCEPT_ALL (0x1<<1) /* BitField statetx filters state	accept all multicast packets (subject to vlan) */
#define CLIENT_INIT_TX_DATA_MCAST_ACCEPT_ALL_SHIFT 1
#define CLIENT_INIT_TX_DATA_BCAST_ACCEPT_ALL (0x1<<2) /* BitField statetx filters state	accept all broadcast packets (subject to vlan) */
#define CLIENT_INIT_TX_DATA_BCAST_ACCEPT_ALL_SHIFT 2
#define CLIENT_INIT_TX_DATA_ACCEPT_ANY_VLAN (0x1<<3) /* BitField statetx filters state	accept packets matched only by MAC (without checking vlan) */
#define CLIENT_INIT_TX_DATA_ACCEPT_ANY_VLAN_SHIFT 3
#define CLIENT_INIT_TX_DATA_RESERVED0 (0xFFF<<4) /* BitField statetx filters state	 */
#define CLIENT_INIT_TX_DATA_RESERVED0_SHIFT 4
	uint8_t default_vlan_flg /* is default vlan valid for this client. */;
	uint8_t force_default_pri_flg /* if set, force default priority */;
	uint8_t tunnel_lso_inc_ip_id /* In case of LSO over IPv4 tunnel, whether to increment IP ID on external IP header or internal IP header */;
	uint8_t refuse_outband_vlan_flg /* if set, the FW will not add outband vlan on packet (even if will exist on BD). */;
	uint8_t tunnel_non_lso_pcsum_location /* In case of non-Lso encapsulated packets with L4 checksum offload, the pseudo checksum location - on packet or on BD. */;
	uint8_t tunnel_non_lso_outer_ip_csum_location /* In case of non-Lso encapsulated packets with outer L3 ip checksum offload, the pseudo checksum location - on packet or on BD. */;
};

/*
 * client init ramrod data $$KEEP_ENDIANNESS$$
 */
struct client_init_ramrod_data
{
	struct client_init_general_data general /* client init general data */;
	struct client_init_rx_data rx /* client init rx data */;
	struct client_init_tx_data tx /* client init tx data */;
};


/*
 * client update ramrod data $$KEEP_ENDIANNESS$$
 */
struct client_update_ramrod_data
{
	uint8_t client_id /* the client to update */;
	uint8_t func_id /* PCI function ID this client belongs to (0-71) */;
	uint8_t inner_vlan_removal_enable_flg /* If set, inner VLAN removal is enabled for this client, will be change according to change flag */;
	uint8_t inner_vlan_removal_change_flg /* If set, inner VLAN removal flag will be set according to the enable flag */;
	uint8_t outer_vlan_removal_enable_flg /* If set, outer VLAN removal is enabled for this client, will be change according to change flag */;
	uint8_t outer_vlan_removal_change_flg /* If set, outer VLAN removal flag will be set according to the enable flag */;
	uint8_t anti_spoofing_enable_flg /* If set, anti spoofing is enabled for this client, will be change according to change flag */;
	uint8_t anti_spoofing_change_flg /* If set, anti spoofing flag will be set according to anti spoofing flag */;
	uint8_t activate_flg /* if 0 - the client is deactivate else the client is activate client (1 bit is used) */;
	uint8_t activate_change_flg /* If set, activate_flg will be checked */;
	uint16_t default_vlan /* default vlan tag (id+pri). (valid if default_vlan_flg is set) */;
	uint8_t default_vlan_enable_flg;
	uint8_t default_vlan_change_flg;
	uint16_t silent_vlan_value /* The vlan to compare, in case, silent vlan is set */;
	uint16_t silent_vlan_mask /* The vlan mask, in case, silent vlan is set */;
	uint8_t silent_vlan_removal_flg /* if set, and the vlan is equal to requested vlan according to mask, the vlan will be remove without notifying the driver */;
	uint8_t silent_vlan_change_flg;
	uint8_t refuse_outband_vlan_flg /* If set, the FW will not add outband vlan on packet (even if will exist on BD). */;
	uint8_t refuse_outband_vlan_change_flg /* If set, refuse_outband_vlan_flg will be updated. */;
	uint8_t tx_switching_flg /* If set, tx switching will be done to packets on this connection. */;
	uint8_t tx_switching_change_flg /* If set, tx_switching_flg will be updated. */;
	uint32_t reserved1;
	uint32_t echo /* echo value to be sent to driver on event ring */;
};


/*
 * The eth storm context of Cstorm
 */
struct cstorm_eth_st_context
{
	uint32_t __reserved0[4];
};


struct double_regpair
{
	uint32_t regpair0_lo /* low word for reg-pair0 */;
	uint32_t regpair0_hi /* high word for reg-pair0 */;
	uint32_t regpair1_lo /* low word for reg-pair1 */;
	uint32_t regpair1_hi /* high word for reg-pair1 */;
};


/*
 * Ethernet address typesm used in ethernet tx BDs
 */
enum eth_addr_type
{
	UNKNOWN_ADDRESS,
	UNICAST_ADDRESS,
	MULTICAST_ADDRESS,
	BROADCAST_ADDRESS,
	MAX_ETH_ADDR_TYPE};


/*
 *  $$KEEP_ENDIANNESS$$
 */
struct eth_classify_cmd_header
{
	uint8_t cmd_general_data;
#define ETH_CLASSIFY_CMD_HEADER_RX_CMD (0x1<<0) /* BitField cmd_general_data	should this cmd be applied for Rx */
#define ETH_CLASSIFY_CMD_HEADER_RX_CMD_SHIFT 0
#define ETH_CLASSIFY_CMD_HEADER_TX_CMD (0x1<<1) /* BitField cmd_general_data	should this cmd be applied for Tx */
#define ETH_CLASSIFY_CMD_HEADER_TX_CMD_SHIFT 1
#define ETH_CLASSIFY_CMD_HEADER_OPCODE (0x3<<2) /* BitField cmd_general_data	command opcode for MAC/VLAN/PAIR (use enum classify_rule) */
#define ETH_CLASSIFY_CMD_HEADER_OPCODE_SHIFT 2
#define ETH_CLASSIFY_CMD_HEADER_IS_ADD (0x1<<4) /* BitField cmd_general_data	 (use enum classify_rule_action_type) */
#define ETH_CLASSIFY_CMD_HEADER_IS_ADD_SHIFT 4
#define ETH_CLASSIFY_CMD_HEADER_RESERVED0 (0x7<<5) /* BitField cmd_general_data	 */
#define ETH_CLASSIFY_CMD_HEADER_RESERVED0_SHIFT 5
	uint8_t func_id /* the function id */;
	uint8_t client_id;
	uint8_t reserved1;
};


/*
 * header for eth classification config ramrod $$KEEP_ENDIANNESS$$
 */
struct eth_classify_header
{
	uint8_t rule_cnt /* number of rules in classification config ramrod */;
	uint8_t reserved0;
	uint16_t reserved1;
	uint32_t echo /* echo value to be sent to driver on event ring */;
};


/*
 * Command for adding/removing a MAC classification rule $$KEEP_ENDIANNESS$$
 */
struct eth_classify_mac_cmd
{
	struct eth_classify_cmd_header header;
	uint16_t reserved0;
	uint16_t inner_mac;
	uint16_t mac_lsb;
	uint16_t mac_mid;
	uint16_t mac_msb;
	uint16_t reserved1;
};


/*
 * Command for adding/removing a MAC-VLAN pair classification rule $$KEEP_ENDIANNESS$$
 */
struct eth_classify_pair_cmd
{
	struct eth_classify_cmd_header header;
	uint16_t reserved0;
	uint16_t inner_mac;
	uint16_t mac_lsb;
	uint16_t mac_mid;
	uint16_t mac_msb;
	uint16_t vlan;
};


/*
 * Command for adding/removing a VLAN classification rule $$KEEP_ENDIANNESS$$
 */
struct eth_classify_vlan_cmd
{
	struct eth_classify_cmd_header header;
	uint32_t reserved0;
	uint32_t reserved1;
	uint16_t reserved2;
	uint16_t vlan;
};

/*
 * union for eth classification rule $$KEEP_ENDIANNESS$$
 */
union eth_classify_rule_cmd
{
	struct eth_classify_mac_cmd mac;
	struct eth_classify_vlan_cmd vlan;
	struct eth_classify_pair_cmd pair;
};

/*
 * parameters for eth classification configuration ramrod $$KEEP_ENDIANNESS$$
 */
struct eth_classify_rules_ramrod_data
{
	struct eth_classify_header header;
	union eth_classify_rule_cmd rules[CLASSIFY_RULES_COUNT];
};


/*
 * The data contain client ID need to the ramrod $$KEEP_ENDIANNESS$$
 */
struct eth_common_ramrod_data
{
	uint32_t client_id /* id of this client. (5 bits are used) */;
	uint32_t reserved1;
};


/*
 * The eth storm context of Ustorm
 */
struct ustorm_eth_st_context
{
	uint32_t reserved0[52];
};

/*
 * The eth storm context of Tstorm
 */
struct tstorm_eth_st_context
{
	uint32_t __reserved0[28];
};

/*
 * The eth storm context of Xstorm
 */
struct xstorm_eth_st_context
{
	uint32_t reserved0[60];
};

/*
 * Ethernet connection context
 */
struct eth_context
{
	struct ustorm_eth_st_context ustorm_st_context /* Ustorm storm context */;
	struct tstorm_eth_st_context tstorm_st_context /* Tstorm storm context */;
	struct xstorm_eth_ag_context xstorm_ag_context /* Xstorm aggregative context */;
	struct tstorm_eth_ag_context tstorm_ag_context /* Tstorm aggregative context */;
	struct cstorm_eth_ag_context cstorm_ag_context /* Cstorm aggregative context */;
	struct ustorm_eth_ag_context ustorm_ag_context /* Ustorm aggregative context */;
	struct timers_block_context timers_context /* Timers block context */;
	struct xstorm_eth_st_context xstorm_st_context /* Xstorm storm context */;
	struct cstorm_eth_st_context cstorm_st_context /* Cstorm storm context */;
};


/*
 * union for sgl and raw data.
 */
union eth_sgl_or_raw_data
{
	uint16_t sgl[8] /* Scatter-gather list of SGEs used by this packet. This list includes the indices of the SGEs. */;
	uint32_t raw_data[4] /* raw data from Tstorm to the driver. */;
};

/*
 * eth FP end aggregation CQE parameters struct $$KEEP_ENDIANNESS$$
 */
struct eth_end_agg_rx_cqe
{
	uint8_t type_error_flags;
#define ETH_END_AGG_RX_CQE_TYPE (0x3<<0) /* BitField type_error_flags	 (use enum eth_rx_cqe_type) */
#define ETH_END_AGG_RX_CQE_TYPE_SHIFT 0
#define ETH_END_AGG_RX_CQE_SGL_RAW_SEL (0x1<<2) /* BitField type_error_flags	 (use enum eth_rx_fp_sel) */
#define ETH_END_AGG_RX_CQE_SGL_RAW_SEL_SHIFT 2
#define ETH_END_AGG_RX_CQE_RESERVED0 (0x1F<<3) /* BitField type_error_flags	 */
#define ETH_END_AGG_RX_CQE_RESERVED0_SHIFT 3
	uint8_t reserved1;
	uint8_t queue_index /* The aggregation queue index of this packet */;
	uint8_t reserved2;
	uint32_t timestamp_delta /* timestamp delta between first packet to last packet in aggregation */;
	uint16_t num_of_coalesced_segs /* Num of coalesced segments. */;
	uint16_t pkt_len /* Packet length */;
	uint8_t pure_ack_count /* Number of pure acks coalesced. */;
	uint8_t reserved3;
	uint16_t reserved4;
	union eth_sgl_or_raw_data sgl_or_raw_data /* union for sgl and raw data. */;
	uint32_t reserved5[8];
};


/*
 * regular eth FP CQE parameters struct $$KEEP_ENDIANNESS$$
 */
struct eth_fast_path_rx_cqe
{
	uint8_t type_error_flags;
#define ETH_FAST_PATH_RX_CQE_TYPE (0x3<<0) /* BitField type_error_flags	 (use enum eth_rx_cqe_type) */
#define ETH_FAST_PATH_RX_CQE_TYPE_SHIFT 0
#define ETH_FAST_PATH_RX_CQE_SGL_RAW_SEL (0x1<<2) /* BitField type_error_flags	 (use enum eth_rx_fp_sel) */
#define ETH_FAST_PATH_RX_CQE_SGL_RAW_SEL_SHIFT 2
#define ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG (0x1<<3) /* BitField type_error_flags	Physical layer errors */
#define ETH_FAST_PATH_RX_CQE_PHY_DECODE_ERR_FLG_SHIFT 3
#define ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG (0x1<<4) /* BitField type_error_flags	IP checksum error */
#define ETH_FAST_PATH_RX_CQE_IP_BAD_XSUM_FLG_SHIFT 4
#define ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG (0x1<<5) /* BitField type_error_flags	TCP/UDP checksum error */
#define ETH_FAST_PATH_RX_CQE_L4_BAD_XSUM_FLG_SHIFT 5
#define ETH_FAST_PATH_RX_CQE_RESERVED0 (0x3<<6) /* BitField type_error_flags	 */
#define ETH_FAST_PATH_RX_CQE_RESERVED0_SHIFT 6
	uint8_t status_flags;
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE (0x7<<0) /* BitField status_flags	 (use enum eth_rss_hash_type) */
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_TYPE_SHIFT 0
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG (0x1<<3) /* BitField status_flags	RSS hashing on/off */
#define ETH_FAST_PATH_RX_CQE_RSS_HASH_FLG_SHIFT 3
#define ETH_FAST_PATH_RX_CQE_BROADCAST_FLG (0x1<<4) /* BitField status_flags	if set to 1, this is a broadcast packet */
#define ETH_FAST_PATH_RX_CQE_BROADCAST_FLG_SHIFT 4
#define ETH_FAST_PATH_RX_CQE_MAC_MATCH_FLG (0x1<<5) /* BitField status_flags	if set to 1, the MAC address was matched in the tstorm CAM search */
#define ETH_FAST_PATH_RX_CQE_MAC_MATCH_FLG_SHIFT 5
#define ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG (0x1<<6) /* BitField status_flags	IP checksum validation was not performed (if packet is not IPv4) */
#define ETH_FAST_PATH_RX_CQE_IP_XSUM_NO_VALIDATION_FLG_SHIFT 6
#define ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG (0x1<<7) /* BitField status_flags	TCP/UDP checksum validation was not performed (if packet is not TCP/UDP or IPv6 extheaders exist) */
#define ETH_FAST_PATH_RX_CQE_L4_XSUM_NO_VALIDATION_FLG_SHIFT 7
	uint8_t queue_index /* The aggregation queue index of this packet */;
	uint8_t placement_offset /* Placement offset from the start of the BD, in bytes */;
	uint32_t rss_hash_result /* RSS toeplitz hash result */;
	uint16_t vlan_tag /* Ethernet VLAN tag field */;
	uint16_t pkt_len_or_gro_seg_len /* Packet length (for non-TPA CQE) or GRO Segment Length (for TPA in GRO Mode) otherwise 0 */;
	uint16_t len_on_bd /* Number of bytes placed on the BD */;
	struct parsing_flags pars_flags;
	union eth_sgl_or_raw_data sgl_or_raw_data /* union for sgl and raw data. */;
	uint32_t reserved1[8];
};


/*
 * Command for setting classification flags for a client $$KEEP_ENDIANNESS$$
 */
struct eth_filter_rules_cmd
{
	uint8_t cmd_general_data;
#define ETH_FILTER_RULES_CMD_RX_CMD (0x1<<0) /* BitField cmd_general_data	should this cmd be applied for Rx */
#define ETH_FILTER_RULES_CMD_RX_CMD_SHIFT 0
#define ETH_FILTER_RULES_CMD_TX_CMD (0x1<<1) /* BitField cmd_general_data	should this cmd be applied for Tx */
#define ETH_FILTER_RULES_CMD_TX_CMD_SHIFT 1
#define ETH_FILTER_RULES_CMD_RESERVED0 (0x3F<<2) /* BitField cmd_general_data	 */
#define ETH_FILTER_RULES_CMD_RESERVED0_SHIFT 2
	uint8_t func_id /* the function id */;
	uint8_t client_id /* the client id */;
	uint8_t reserved1;
	uint16_t state;
#define ETH_FILTER_RULES_CMD_UCAST_DROP_ALL (0x1<<0) /* BitField state	drop all unicast packets */
#define ETH_FILTER_RULES_CMD_UCAST_DROP_ALL_SHIFT 0
#define ETH_FILTER_RULES_CMD_UCAST_ACCEPT_ALL (0x1<<1) /* BitField state	accept all unicast packets (subject to vlan) */
#define ETH_FILTER_RULES_CMD_UCAST_ACCEPT_ALL_SHIFT 1
#define ETH_FILTER_RULES_CMD_UCAST_ACCEPT_UNMATCHED (0x1<<2) /* BitField state	accept all unmatched unicast packets */
#define ETH_FILTER_RULES_CMD_UCAST_ACCEPT_UNMATCHED_SHIFT 2
#define ETH_FILTER_RULES_CMD_MCAST_DROP_ALL (0x1<<3) /* BitField state	drop all multicast packets */
#define ETH_FILTER_RULES_CMD_MCAST_DROP_ALL_SHIFT 3
#define ETH_FILTER_RULES_CMD_MCAST_ACCEPT_ALL (0x1<<4) /* BitField state	accept all multicast packets (subject to vlan) */
#define ETH_FILTER_RULES_CMD_MCAST_ACCEPT_ALL_SHIFT 4
#define ETH_FILTER_RULES_CMD_BCAST_ACCEPT_ALL (0x1<<5) /* BitField state	accept all broadcast packets (subject to vlan) */
#define ETH_FILTER_RULES_CMD_BCAST_ACCEPT_ALL_SHIFT 5
#define ETH_FILTER_RULES_CMD_ACCEPT_ANY_VLAN (0x1<<6) /* BitField state	accept packets matched only by MAC (without checking vlan) */
#define ETH_FILTER_RULES_CMD_ACCEPT_ANY_VLAN_SHIFT 6
#define ETH_FILTER_RULES_CMD_RESERVED2 (0x1FF<<7) /* BitField state	 */
#define ETH_FILTER_RULES_CMD_RESERVED2_SHIFT 7
	uint16_t reserved3;
	struct regpair reserved4;
};


/*
 * parameters for eth classification filters ramrod $$KEEP_ENDIANNESS$$
 */
struct eth_filter_rules_ramrod_data
{
	struct eth_classify_header header;
	struct eth_filter_rules_cmd rules[FILTER_RULES_COUNT];
};


/*
 * parameters for eth classification configuration ramrod $$KEEP_ENDIANNESS$$
 */
struct eth_general_rules_ramrod_data
{
	struct eth_classify_header header;
	union eth_classify_rule_cmd rules[CLASSIFY_RULES_COUNT];
};


/*
 * The data for Halt ramrod
 */
struct eth_halt_ramrod_data
{
	uint32_t client_id /* id of this client. (5 bits are used) */;
	uint32_t reserved0;
};


/*
 * destination and source mac address.
 */
struct eth_mac_addresses
{
#if defined(__BIG_ENDIAN)
	uint16_t dst_mid /* destination mac address 16 middle bits */;
	uint16_t dst_lo /* destination mac address 16 low bits */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t dst_lo /* destination mac address 16 low bits */;
	uint16_t dst_mid /* destination mac address 16 middle bits */;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t src_lo /* source mac address 16 low bits */;
	uint16_t dst_hi /* destination mac address 16 high bits */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t dst_hi /* destination mac address 16 high bits */;
	uint16_t src_lo /* source mac address 16 low bits */;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t src_hi /* source mac address 16 high bits */;
	uint16_t src_mid /* source mac address 16 middle bits */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t src_mid /* source mac address 16 middle bits */;
	uint16_t src_hi /* source mac address 16 high bits */;
#endif
};


/*
 * tunneling related data.
 */
struct eth_tunnel_data
{
#if defined(__BIG_ENDIAN)
	uint16_t dst_mid /* destination mac address 16 middle bits */;
	uint16_t dst_lo /* destination mac address 16 low bits */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t dst_lo /* destination mac address 16 low bits */;
	uint16_t dst_mid /* destination mac address 16 middle bits */;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t fw_ip_hdr_csum /* Fw Ip header checksum (with ALL ip header fields) for the outer IP header */;
	uint16_t dst_hi /* destination mac address 16 high bits */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t dst_hi /* destination mac address 16 high bits */;
	uint16_t fw_ip_hdr_csum /* Fw Ip header checksum (with ALL ip header fields) for the outer IP header */;
#endif
#if defined(__BIG_ENDIAN)
	uint8_t flags;
#define ETH_TUNNEL_DATA_IP_HDR_TYPE_OUTER (0x1<<0) /* BitField flags	Set in case outer IP header is ipV6 */
#define ETH_TUNNEL_DATA_IP_HDR_TYPE_OUTER_SHIFT 0
#define ETH_TUNNEL_DATA_RESERVED (0x7F<<1) /* BitField flags	Should be set with 0 */
#define ETH_TUNNEL_DATA_RESERVED_SHIFT 1
	uint8_t ip_hdr_start_inner_w /* Inner IP header offset in WORDs (16-bit) from start of packet */;
	uint16_t pseudo_csum /* Pseudo checksum with  length  field=0 */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t pseudo_csum /* Pseudo checksum with  length  field=0 */;
	uint8_t ip_hdr_start_inner_w /* Inner IP header offset in WORDs (16-bit) from start of packet */;
	uint8_t flags;
#define ETH_TUNNEL_DATA_IP_HDR_TYPE_OUTER (0x1<<0) /* BitField flags	Set in case outer IP header is ipV6 */
#define ETH_TUNNEL_DATA_IP_HDR_TYPE_OUTER_SHIFT 0
#define ETH_TUNNEL_DATA_RESERVED (0x7F<<1) /* BitField flags	Should be set with 0 */
#define ETH_TUNNEL_DATA_RESERVED_SHIFT 1
#endif
};

/*
 * union for mac addresses and for tunneling data. considered as tunneling data only if (tunnel_exist == 1).
 */
union eth_mac_addr_or_tunnel_data
{
	struct eth_mac_addresses mac_addr /* destination and source mac addresses. */;
	struct eth_tunnel_data tunnel_data /* tunneling related data. */;
};


/*
 * Command for setting multicast classification for a client $$KEEP_ENDIANNESS$$
 */
struct eth_multicast_rules_cmd
{
	uint8_t cmd_general_data;
#define ETH_MULTICAST_RULES_CMD_RX_CMD (0x1<<0) /* BitField cmd_general_data	should this cmd be applied for Rx */
#define ETH_MULTICAST_RULES_CMD_RX_CMD_SHIFT 0
#define ETH_MULTICAST_RULES_CMD_TX_CMD (0x1<<1) /* BitField cmd_general_data	should this cmd be applied for Tx */
#define ETH_MULTICAST_RULES_CMD_TX_CMD_SHIFT 1
#define ETH_MULTICAST_RULES_CMD_IS_ADD (0x1<<2) /* BitField cmd_general_data	1 for add rule, 0 for remove rule */
#define ETH_MULTICAST_RULES_CMD_IS_ADD_SHIFT 2
#define ETH_MULTICAST_RULES_CMD_RESERVED0 (0x1F<<3) /* BitField cmd_general_data	 */
#define ETH_MULTICAST_RULES_CMD_RESERVED0_SHIFT 3
	uint8_t func_id /* the function id */;
	uint8_t bin_id /* the bin to add this function to (0-255) */;
	uint8_t engine_id /* the approximate multicast engine id */;
	uint32_t reserved2;
	struct regpair reserved3;
};


/*
 * parameters for multicast classification ramrod $$KEEP_ENDIANNESS$$
 */
struct eth_multicast_rules_ramrod_data
{
	struct eth_classify_header header;
	struct eth_multicast_rules_cmd rules[MULTICAST_RULES_COUNT];
};


/*
 * Place holder for ramrods protocol specific data
 */
struct ramrod_data
{
	uint32_t data_lo;
	uint32_t data_hi;
};

/*
 * union for ramrod data for Ethernet protocol (CQE) (force size of 16 bits)
 */
union eth_ramrod_data
{
	struct ramrod_data general;
};


/*
 * RSS toeplitz hash type, as reported in CQE
 */
enum eth_rss_hash_type
{
	DEFAULT_HASH_TYPE,
	IPV4_HASH_TYPE,
	TCP_IPV4_HASH_TYPE,
	IPV6_HASH_TYPE,
	TCP_IPV6_HASH_TYPE,
	VLAN_PRI_HASH_TYPE,
	E1HOV_PRI_HASH_TYPE,
	DSCP_HASH_TYPE,
	MAX_ETH_RSS_HASH_TYPE};


/*
 * Ethernet RSS mode
 */
enum eth_rss_mode
{
	ETH_RSS_MODE_DISABLED,
	ETH_RSS_MODE_ESX51 /* RSS mode for Vmware ESX 5.1 (Only do RSS if packet is UDP with dst port that matches the UDP 4-tuble Destination Port mask and value) */,
	ETH_RSS_MODE_REGULAR /* Regular (ndis-like) RSS */,
	ETH_RSS_MODE_VLAN_PRI /* RSS based on inner-vlan priority field */,
	ETH_RSS_MODE_E1HOV_PRI /* RSS based on outer-vlan priority field */,
	ETH_RSS_MODE_IP_DSCP /* RSS based on IPv4 DSCP field */,
	MAX_ETH_RSS_MODE};


/*
 * parameters for RSS update ramrod (E2) $$KEEP_ENDIANNESS$$
 */
struct eth_rss_update_ramrod_data
{
	uint8_t rss_engine_id;
	uint8_t capabilities;
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_CAPABILITY (0x1<<0) /* BitField capabilitiesFunction RSS capabilities	configuration of the IpV4 2-tupple capability */
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_CAPABILITY_SHIFT 0
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_TCP_CAPABILITY (0x1<<1) /* BitField capabilitiesFunction RSS capabilities	configuration of the IpV4 4-tupple capability for TCP */
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_TCP_CAPABILITY_SHIFT 1
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_UDP_CAPABILITY (0x1<<2) /* BitField capabilitiesFunction RSS capabilities	configuration of the IpV4 4-tupple capability for UDP */
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV4_UDP_CAPABILITY_SHIFT 2
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_CAPABILITY (0x1<<3) /* BitField capabilitiesFunction RSS capabilities	configuration of the IpV6 2-tupple capability */
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_CAPABILITY_SHIFT 3
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_TCP_CAPABILITY (0x1<<4) /* BitField capabilitiesFunction RSS capabilities	configuration of the IpV6 4-tupple capability for TCP */
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_TCP_CAPABILITY_SHIFT 4
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_UDP_CAPABILITY (0x1<<5) /* BitField capabilitiesFunction RSS capabilities	configuration of the IpV6 4-tupple capability for UDP */
#define ETH_RSS_UPDATE_RAMROD_DATA_IPV6_UDP_CAPABILITY_SHIFT 5
#define ETH_RSS_UPDATE_RAMROD_DATA_EN_5_TUPLE_CAPABILITY (0x1<<6) /* BitField capabilitiesFunction RSS capabilities	configuration of the 5-tupple capability */
#define ETH_RSS_UPDATE_RAMROD_DATA_EN_5_TUPLE_CAPABILITY_SHIFT 6
#define ETH_RSS_UPDATE_RAMROD_DATA_UPDATE_RSS_KEY (0x1<<7) /* BitField capabilitiesFunction RSS capabilities	if set update the rss keys */
#define ETH_RSS_UPDATE_RAMROD_DATA_UPDATE_RSS_KEY_SHIFT 7
	uint8_t rss_result_mask /* The mask for the lower byte of RSS result - defines which section of the indirection table will be used. To enable all table put here 0x7F */;
	uint8_t rss_mode /* The RSS mode for this function */;
	uint16_t udp_4tuple_dst_port_mask /* If UDP 4-tuple enabled, packets that match the mask and value are 4-tupled, the rest are 2-tupled. (Set to 0 to match all) */;
	uint16_t udp_4tuple_dst_port_value /* If UDP 4-tuple enabled, packets that match the mask and value are 4-tupled, the rest are 2-tupled. (Set to 0 to match all) */;
	uint8_t indirection_table[T_ETH_INDIRECTION_TABLE_SIZE] /* RSS indirection table */;
	uint32_t rss_key[T_ETH_RSS_KEY] /* RSS key supplied as by OS */;
	uint32_t echo;
	uint32_t reserved3;
};


/*
 * The eth Rx Buffer Descriptor
 */
struct eth_rx_bd
{
	uint32_t addr_lo /* Single continuous buffer low pointer */;
	uint32_t addr_hi /* Single continuous buffer high pointer */;
};


/*
 * Eth Rx Cqe structure- general structure for ramrods $$KEEP_ENDIANNESS$$
 */
struct common_ramrod_eth_rx_cqe
{
	uint8_t ramrod_type;
#define COMMON_RAMROD_ETH_RX_CQE_TYPE (0x3<<0) /* BitField ramrod_type	 (use enum eth_rx_cqe_type) */
#define COMMON_RAMROD_ETH_RX_CQE_TYPE_SHIFT 0
#define COMMON_RAMROD_ETH_RX_CQE_ERROR (0x1<<2) /* BitField ramrod_type	 */
#define COMMON_RAMROD_ETH_RX_CQE_ERROR_SHIFT 2
#define COMMON_RAMROD_ETH_RX_CQE_RESERVED0 (0x1F<<3) /* BitField ramrod_type	 */
#define COMMON_RAMROD_ETH_RX_CQE_RESERVED0_SHIFT 3
	uint8_t conn_type /* only 3 bits are used */;
	uint16_t reserved1 /* protocol specific data */;
	uint32_t conn_and_cmd_data;
#define COMMON_RAMROD_ETH_RX_CQE_CID (0xFFFFFF<<0) /* BitField conn_and_cmd_data	 */
#define COMMON_RAMROD_ETH_RX_CQE_CID_SHIFT 0
#define COMMON_RAMROD_ETH_RX_CQE_CMD_ID (0xFF<<24) /* BitField conn_and_cmd_data	command id of the ramrod- use RamrodCommandIdEnum */
#define COMMON_RAMROD_ETH_RX_CQE_CMD_ID_SHIFT 24
	struct ramrod_data protocol_data /* protocol specific data */;
	uint32_t echo;
	uint32_t reserved2[11];
};

/*
 * Rx Last CQE in page (in ETH)
 */
struct eth_rx_cqe_next_page
{
	uint32_t addr_lo /* Next page low pointer */;
	uint32_t addr_hi /* Next page high pointer */;
	uint32_t reserved[14];
};

/*
 * union for all eth rx cqe types (fix their sizes)
 */
union eth_rx_cqe
{
	struct eth_fast_path_rx_cqe fast_path_cqe;
	struct common_ramrod_eth_rx_cqe ramrod_cqe;
	struct eth_rx_cqe_next_page next_page_cqe;
	struct eth_end_agg_rx_cqe end_agg_cqe;
};


/*
 * Values for RX ETH CQE type field
 */
enum eth_rx_cqe_type
{
	RX_ETH_CQE_TYPE_ETH_FASTPATH /* Fast path CQE */,
	RX_ETH_CQE_TYPE_ETH_RAMROD /* Slow path CQE */,
	RX_ETH_CQE_TYPE_ETH_START_AGG /* Fast path CQE */,
	RX_ETH_CQE_TYPE_ETH_STOP_AGG /* Slow path CQE */,
	MAX_ETH_RX_CQE_TYPE};


/*
 * Type of SGL/Raw field in ETH RX fast path CQE
 */
enum eth_rx_fp_sel
{
	ETH_FP_CQE_REGULAR /* Regular CQE- no extra data */,
	ETH_FP_CQE_RAW /* Extra data is raw data- iscsi OOO */,
	MAX_ETH_RX_FP_SEL};


/*
 * The eth Rx SGE Descriptor
 */
struct eth_rx_sge
{
	uint32_t addr_lo /* Single continuous buffer low pointer */;
	uint32_t addr_hi /* Single continuous buffer high pointer */;
};


/*
 * common data for all protocols $$KEEP_ENDIANNESS$$
 */
struct spe_hdr
{
	uint32_t conn_and_cmd_data;
#define SPE_HDR_CID (0xFFFFFF<<0) /* BitField conn_and_cmd_data	 */
#define SPE_HDR_CID_SHIFT 0
#define SPE_HDR_CMD_ID (0xFF<<24) /* BitField conn_and_cmd_data	command id of the ramrod- use enum common_spqe_cmd_id/eth_spqe_cmd_id/toe_spqe_cmd_id  */
#define SPE_HDR_CMD_ID_SHIFT 24
	uint16_t type;
#define SPE_HDR_CONN_TYPE (0xFF<<0) /* BitField type	connection type. (3 bits are used) (use enum connection_type) */
#define SPE_HDR_CONN_TYPE_SHIFT 0
#define SPE_HDR_FUNCTION_ID (0xFF<<8) /* BitField type	 */
#define SPE_HDR_FUNCTION_ID_SHIFT 8
	uint16_t reserved1;
};

/*
 * specific data for ethernet slow path element
 */
union eth_specific_data
{
	uint8_t protocol_data[8] /* to fix this structure size to 8 bytes */;
	struct regpair client_update_ramrod_data /* The address of the data for client update ramrod */;
	struct regpair client_init_ramrod_init_data /* The data for client setup ramrod */;
	struct eth_halt_ramrod_data halt_ramrod_data /* Includes the client id to be deleted */;
	struct regpair update_data_addr /* physical address of the eth_rss_update_ramrod_data struct, as allocated by the driver */;
	struct eth_common_ramrod_data common_ramrod_data /* The data contain client ID need to the ramrod */;
	struct regpair classify_cfg_addr /* physical address of the eth_classify_rules_ramrod_data struct, as allocated by the driver */;
	struct regpair filter_cfg_addr /* physical address of the eth_filter_cfg_ramrod_data struct, as allocated by the driver */;
	struct regpair mcast_cfg_addr /* physical address of the eth_mcast_cfg_ramrod_data struct, as allocated by the driver */;
};

/*
 * Ethernet slow path element
 */
struct eth_spe
{
	struct spe_hdr hdr /* common data for all protocols */;
	union eth_specific_data data /* data specific to ethernet protocol */;
};


/*
 * Ethernet command ID for slow path elements
 */
enum eth_spqe_cmd_id
{
	RAMROD_CMD_ID_ETH_UNUSED,
	RAMROD_CMD_ID_ETH_CLIENT_SETUP /* Setup a new L2 client */,
	RAMROD_CMD_ID_ETH_HALT /* Halt an L2 client */,
	RAMROD_CMD_ID_ETH_FORWARD_SETUP /* Setup a new FW channel */,
	RAMROD_CMD_ID_ETH_TX_QUEUE_SETUP /* Setup a new Tx only queue */,
	RAMROD_CMD_ID_ETH_CLIENT_UPDATE /* Update an L2 client configuration */,
	RAMROD_CMD_ID_ETH_EMPTY /* Empty ramrod - used to synchronize iSCSI OOO */,
	RAMROD_CMD_ID_ETH_TERMINATE /* Terminate an L2 client */,
	RAMROD_CMD_ID_ETH_TPA_UPDATE /* update the tpa roles in L2 client */,
	RAMROD_CMD_ID_ETH_CLASSIFICATION_RULES /* Add/remove classification filters for L2 client (in E2/E3 only) */,
	RAMROD_CMD_ID_ETH_FILTER_RULES /* Add/remove classification filters for L2 client (in E2/E3 only) */,
	RAMROD_CMD_ID_ETH_MULTICAST_RULES /* Add/remove multicast classification bin (in E2/E3 only) */,
	RAMROD_CMD_ID_ETH_RSS_UPDATE /* Update RSS configuration */,
	RAMROD_CMD_ID_ETH_SET_MAC /* Update RSS configuration */,
	MAX_ETH_SPQE_CMD_ID};


/*
 * eth tpa update command
 */
enum eth_tpa_update_command
{
	TPA_UPDATE_NONE_COMMAND /* nop command */,
	TPA_UPDATE_ENABLE_COMMAND /* enable command */,
	TPA_UPDATE_DISABLE_COMMAND /* disable command */,
	MAX_ETH_TPA_UPDATE_COMMAND};


/*
 * In case of LSO over IPv4 tunnel, whether to increment IP ID on external IP header or internal IP header
 */
enum eth_tunnel_lso_inc_ip_id
{
	EXT_HEADER /* Increment IP ID of external header (HW works on external, FW works on internal */,
	INT_HEADER /* Increment IP ID of internal header (HW works on internal, FW works on external */,
	MAX_ETH_TUNNEL_LSO_INC_IP_ID};


/*
 * In case tunnel exist and L4 checksum offload (or outer ip header checksum), the pseudo checksum location, on packet or on BD.
 */
enum eth_tunnel_non_lso_csum_location
{
	CSUM_ON_PKT /* checksum is on the packet. */,
	CSUM_ON_BD /* checksum is on the BD. */,
	MAX_ETH_TUNNEL_NON_LSO_CSUM_LOCATION};


/*
 * Tx regular BD structure $$KEEP_ENDIANNESS$$
 */
struct eth_tx_bd
{
	uint32_t addr_lo /* Single continuous buffer low pointer */;
	uint32_t addr_hi /* Single continuous buffer high pointer */;
	uint16_t total_pkt_bytes /* Size of the entire packet, valid for non-LSO packets */;
	uint16_t nbytes /* Size of the data represented by the BD */;
	uint8_t reserved[4] /* keeps same size as other eth tx bd types */;
};


/*
 * structure for easy accessibility to assembler
 */
struct eth_tx_bd_flags
{
	uint8_t as_bitfield;
#define ETH_TX_BD_FLAGS_IP_CSUM (0x1<<0) /* BitField as_bitfield	IP CKSUM flag,Relevant in START */
#define ETH_TX_BD_FLAGS_IP_CSUM_SHIFT 0
#define ETH_TX_BD_FLAGS_L4_CSUM (0x1<<1) /* BitField as_bitfield	L4 CKSUM flag,Relevant in START */
#define ETH_TX_BD_FLAGS_L4_CSUM_SHIFT 1
#define ETH_TX_BD_FLAGS_VLAN_MODE (0x3<<2) /* BitField as_bitfield	00 - no vlan; 01 - inband Vlan; 10 outband Vlan (use enum eth_tx_vlan_type) */
#define ETH_TX_BD_FLAGS_VLAN_MODE_SHIFT 2
#define ETH_TX_BD_FLAGS_START_BD (0x1<<4) /* BitField as_bitfield	Start of packet BD */
#define ETH_TX_BD_FLAGS_START_BD_SHIFT 4
#define ETH_TX_BD_FLAGS_IS_UDP (0x1<<5) /* BitField as_bitfield	flag that indicates that the current packet is a udp packet */
#define ETH_TX_BD_FLAGS_IS_UDP_SHIFT 5
#define ETH_TX_BD_FLAGS_SW_LSO (0x1<<6) /* BitField as_bitfield	LSO flag, Relevant in START */
#define ETH_TX_BD_FLAGS_SW_LSO_SHIFT 6
#define ETH_TX_BD_FLAGS_IPV6 (0x1<<7) /* BitField as_bitfield	set in case ipV6 packet, Relevant in START */
#define ETH_TX_BD_FLAGS_IPV6_SHIFT 7
};

/*
 * The eth Tx Buffer Descriptor $$KEEP_ENDIANNESS$$
 */
struct eth_tx_start_bd
{
	uint32_t addr_lo /* Single continuous buffer low pointer */;
	uint32_t addr_hi /* Single continuous buffer high pointer */;
	uint16_t nbd /* Num of BDs in packet: include parsInfoBD, Relevant in START(only in Everest) */;
	uint16_t nbytes /* Size of the data represented by the BD */;
	uint16_t vlan_or_ethertype /* Vlan structure: vlan_id is in lsb, then cfi and then priority vlan_id 12 bits (lsb), cfi 1 bit, priority 3 bits. In E2, this field should be set with etherType for VFs with no vlan */;
	struct eth_tx_bd_flags bd_flags;
	uint8_t general_data;
#define ETH_TX_START_BD_HDR_NBDS (0xF<<0) /* BitField general_data	contains the number of BDs that contain Ethernet/IP/TCP headers, for full/partial LSO modes */
#define ETH_TX_START_BD_HDR_NBDS_SHIFT 0
#define ETH_TX_START_BD_FORCE_VLAN_MODE (0x1<<4) /* BitField general_data	force vlan mode according to bds (vlan mode can change accroding to global configuration) */
#define ETH_TX_START_BD_FORCE_VLAN_MODE_SHIFT 4
#define ETH_TX_START_BD_PARSE_NBDS (0x3<<5) /* BitField general_data	Determines the number of parsing BDs in packet. Number of parsing BDs in packet is (parse_nbds+1). */
#define ETH_TX_START_BD_PARSE_NBDS_SHIFT 5
#define ETH_TX_START_BD_TUNNEL_EXIST (0x1<<7) /* BitField general_data	set in case of tunneling encapsulated packet */
#define ETH_TX_START_BD_TUNNEL_EXIST_SHIFT 7
};

/*
 * Tx parsing BD structure for ETH E1/E1h $$KEEP_ENDIANNESS$$
 */
struct eth_tx_parse_bd_e1x
{
	uint16_t global_data;
#define ETH_TX_PARSE_BD_E1X_IP_HDR_START_OFFSET_W (0xF<<0) /* BitField global_data	IP header Offset in WORDs from start of packet */
#define ETH_TX_PARSE_BD_E1X_IP_HDR_START_OFFSET_W_SHIFT 0
#define ETH_TX_PARSE_BD_E1X_ETH_ADDR_TYPE (0x3<<4) /* BitField global_data	marks ethernet address type (use enum eth_addr_type) */
#define ETH_TX_PARSE_BD_E1X_ETH_ADDR_TYPE_SHIFT 4
#define ETH_TX_PARSE_BD_E1X_PSEUDO_CS_WITHOUT_LEN (0x1<<6) /* BitField global_data	 */
#define ETH_TX_PARSE_BD_E1X_PSEUDO_CS_WITHOUT_LEN_SHIFT 6
#define ETH_TX_PARSE_BD_E1X_LLC_SNAP_EN (0x1<<7) /* BitField global_data	 */
#define ETH_TX_PARSE_BD_E1X_LLC_SNAP_EN_SHIFT 7
#define ETH_TX_PARSE_BD_E1X_NS_FLG (0x1<<8) /* BitField global_data	an optional addition to ECN that protects against accidental or malicious concealment of marked packets from the TCP sender. */
#define ETH_TX_PARSE_BD_E1X_NS_FLG_SHIFT 8
#define ETH_TX_PARSE_BD_E1X_RESERVED0 (0x7F<<9) /* BitField global_data	reserved bit, should be set with 0 */
#define ETH_TX_PARSE_BD_E1X_RESERVED0_SHIFT 9
	uint8_t tcp_flags;
#define ETH_TX_PARSE_BD_E1X_FIN_FLG (0x1<<0) /* BitField tcp_flagsState flags	End of data flag */
#define ETH_TX_PARSE_BD_E1X_FIN_FLG_SHIFT 0
#define ETH_TX_PARSE_BD_E1X_SYN_FLG (0x1<<1) /* BitField tcp_flagsState flags	Synchronize sequence numbers flag */
#define ETH_TX_PARSE_BD_E1X_SYN_FLG_SHIFT 1
#define ETH_TX_PARSE_BD_E1X_RST_FLG (0x1<<2) /* BitField tcp_flagsState flags	Reset connection flag */
#define ETH_TX_PARSE_BD_E1X_RST_FLG_SHIFT 2
#define ETH_TX_PARSE_BD_E1X_PSH_FLG (0x1<<3) /* BitField tcp_flagsState flags	Push flag */
#define ETH_TX_PARSE_BD_E1X_PSH_FLG_SHIFT 3
#define ETH_TX_PARSE_BD_E1X_ACK_FLG (0x1<<4) /* BitField tcp_flagsState flags	Acknowledgment number valid flag */
#define ETH_TX_PARSE_BD_E1X_ACK_FLG_SHIFT 4
#define ETH_TX_PARSE_BD_E1X_URG_FLG (0x1<<5) /* BitField tcp_flagsState flags	Urgent pointer valid flag */
#define ETH_TX_PARSE_BD_E1X_URG_FLG_SHIFT 5
#define ETH_TX_PARSE_BD_E1X_ECE_FLG (0x1<<6) /* BitField tcp_flagsState flags	ECN-Echo */
#define ETH_TX_PARSE_BD_E1X_ECE_FLG_SHIFT 6
#define ETH_TX_PARSE_BD_E1X_CWR_FLG (0x1<<7) /* BitField tcp_flagsState flags	Congestion Window Reduced */
#define ETH_TX_PARSE_BD_E1X_CWR_FLG_SHIFT 7
	uint8_t ip_hlen_w /* IP header length in WORDs */;
	uint16_t total_hlen_w /* IP+TCP+ETH */;
	uint16_t tcp_pseudo_csum /* Checksum of pseudo header with  length  field=0 */;
	uint16_t lso_mss /* for LSO mode */;
	uint16_t ip_id /* for LSO mode */;
	uint32_t tcp_send_seq /* for LSO mode */;
};

/*
 * Tx parsing BD structure for ETH E2 $$KEEP_ENDIANNESS$$
 */
struct eth_tx_parse_bd_e2
{
	union eth_mac_addr_or_tunnel_data data /* union for mac addresses and for tunneling data. considered as tunneling data only if (tunnel_exist == 1). */;
	uint32_t parsing_data;
#define ETH_TX_PARSE_BD_E2_L4_HDR_START_OFFSET_W (0x7FF<<0) /* BitField parsing_data	TCP/UDP header Offset in WORDs from start of packet */
#define ETH_TX_PARSE_BD_E2_L4_HDR_START_OFFSET_W_SHIFT 0
#define ETH_TX_PARSE_BD_E2_TCP_HDR_LENGTH_DW (0xF<<11) /* BitField parsing_data	TCP header size in DOUBLE WORDS */
#define ETH_TX_PARSE_BD_E2_TCP_HDR_LENGTH_DW_SHIFT 11
#define ETH_TX_PARSE_BD_E2_IPV6_WITH_EXT_HDR (0x1<<15) /* BitField parsing_data	a flag to indicate an ipv6 packet with extension headers. If set on LSO packet, pseudo CS should be placed in TCP CS field without length field */
#define ETH_TX_PARSE_BD_E2_IPV6_WITH_EXT_HDR_SHIFT 15
#define ETH_TX_PARSE_BD_E2_LSO_MSS (0x3FFF<<16) /* BitField parsing_data	for LSO mode */
#define ETH_TX_PARSE_BD_E2_LSO_MSS_SHIFT 16
#define ETH_TX_PARSE_BD_E2_ETH_ADDR_TYPE (0x3<<30) /* BitField parsing_data	marks ethernet address type (use enum eth_addr_type) */
#define ETH_TX_PARSE_BD_E2_ETH_ADDR_TYPE_SHIFT 30
};

/*
 * Tx 2nd parsing BD structure for ETH packet $$KEEP_ENDIANNESS$$
 */
struct eth_tx_parse_2nd_bd
{
	uint16_t global_data;
#define ETH_TX_PARSE_2ND_BD_IP_HDR_START_OUTER_W (0xF<<0) /* BitField global_data	Outer IP header offset in WORDs (16-bit) from start of packet */
#define ETH_TX_PARSE_2ND_BD_IP_HDR_START_OUTER_W_SHIFT 0
#define ETH_TX_PARSE_2ND_BD_RESERVED0 (0x1<<4) /* BitField global_data	should be set with 0 */
#define ETH_TX_PARSE_2ND_BD_RESERVED0_SHIFT 4
#define ETH_TX_PARSE_2ND_BD_LLC_SNAP_EN (0x1<<5) /* BitField global_data	 */
#define ETH_TX_PARSE_2ND_BD_LLC_SNAP_EN_SHIFT 5
#define ETH_TX_PARSE_2ND_BD_NS_FLG (0x1<<6) /* BitField global_data	an optional addition to ECN that protects against accidental or malicious concealment of marked packets from the TCP sender. */
#define ETH_TX_PARSE_2ND_BD_NS_FLG_SHIFT 6
#define ETH_TX_PARSE_2ND_BD_TUNNEL_UDP_EXIST (0x1<<7) /* BitField global_data	Set in case UDP header exists in tunnel outer hedears. */
#define ETH_TX_PARSE_2ND_BD_TUNNEL_UDP_EXIST_SHIFT 7
#define ETH_TX_PARSE_2ND_BD_IP_HDR_LEN_OUTER_W (0x1F<<8) /* BitField global_data	Outer IP header length in WORDs (16-bit). Valid only for IpV4. */
#define ETH_TX_PARSE_2ND_BD_IP_HDR_LEN_OUTER_W_SHIFT 8
#define ETH_TX_PARSE_2ND_BD_RESERVED1 (0x7<<13) /* BitField global_data	should be set with 0 */
#define ETH_TX_PARSE_2ND_BD_RESERVED1_SHIFT 13
	uint16_t reserved2;
	uint8_t tcp_flags;
#define ETH_TX_PARSE_2ND_BD_FIN_FLG (0x1<<0) /* BitField tcp_flagsState flags	End of data flag */
#define ETH_TX_PARSE_2ND_BD_FIN_FLG_SHIFT 0
#define ETH_TX_PARSE_2ND_BD_SYN_FLG (0x1<<1) /* BitField tcp_flagsState flags	Synchronize sequence numbers flag */
#define ETH_TX_PARSE_2ND_BD_SYN_FLG_SHIFT 1
#define ETH_TX_PARSE_2ND_BD_RST_FLG (0x1<<2) /* BitField tcp_flagsState flags	Reset connection flag */
#define ETH_TX_PARSE_2ND_BD_RST_FLG_SHIFT 2
#define ETH_TX_PARSE_2ND_BD_PSH_FLG (0x1<<3) /* BitField tcp_flagsState flags	Push flag */
#define ETH_TX_PARSE_2ND_BD_PSH_FLG_SHIFT 3
#define ETH_TX_PARSE_2ND_BD_ACK_FLG (0x1<<4) /* BitField tcp_flagsState flags	Acknowledgment number valid flag */
#define ETH_TX_PARSE_2ND_BD_ACK_FLG_SHIFT 4
#define ETH_TX_PARSE_2ND_BD_URG_FLG (0x1<<5) /* BitField tcp_flagsState flags	Urgent pointer valid flag */
#define ETH_TX_PARSE_2ND_BD_URG_FLG_SHIFT 5
#define ETH_TX_PARSE_2ND_BD_ECE_FLG (0x1<<6) /* BitField tcp_flagsState flags	ECN-Echo */
#define ETH_TX_PARSE_2ND_BD_ECE_FLG_SHIFT 6
#define ETH_TX_PARSE_2ND_BD_CWR_FLG (0x1<<7) /* BitField tcp_flagsState flags	Congestion Window Reduced */
#define ETH_TX_PARSE_2ND_BD_CWR_FLG_SHIFT 7
	uint8_t reserved3;
	uint8_t tunnel_udp_hdr_start_w /* Offset (in WORDs) from start of packet to tunnel UDP header. (if exist) */;
	uint8_t fw_ip_hdr_to_payload_w /* In IpV4, the length (in WORDs) from the FW IpV4 header start to the payload start. In IpV6, the length (in WORDs) from the FW IpV6 header end to the payload start. However, if extension headers are included, their length is counted here as well. */;
	uint16_t fw_ip_csum_wo_len_flags_frag /* For the IP header which is set by the FW, the IP checksum without length, flags and fragment offset. */;
	uint16_t hw_ip_id /* The IP ID to be set by HW for LSO packets in tunnel mode. */;
	uint32_t tcp_send_seq /* The TCP sequence number for LSO packets. */;
};

/*
 * The last BD in the BD memory will hold a pointer to the next BD memory
 */
struct eth_tx_next_bd
{
	uint32_t addr_lo /* Single continuous buffer low pointer */;
	uint32_t addr_hi /* Single continuous buffer high pointer */;
	uint8_t reserved[8] /* keeps same size as other eth tx bd types */;
};

/*
 * union for 4 Bd types
 */
union eth_tx_bd_types
{
	struct eth_tx_start_bd start_bd /* the first bd in a packets */;
	struct eth_tx_bd reg_bd /* the common bd */;
	struct eth_tx_parse_bd_e1x parse_bd_e1x /* parsing info BD for e1/e1h */;
	struct eth_tx_parse_bd_e2 parse_bd_e2 /* parsing info BD for e2 */;
	struct eth_tx_parse_2nd_bd parse_2nd_bd /* 2nd parsing info BD */;
	struct eth_tx_next_bd next_bd /* Bd that contains the address of the next page */;
};

/*
 * array of 13 bds as appears in the eth xstorm context
 */
struct eth_tx_bds_array
{
	union eth_tx_bd_types bds[13];
};


/*
 * VLAN mode on TX BDs
 */
enum eth_tx_vlan_type
{
	X_ETH_NO_VLAN,
	X_ETH_OUTBAND_VLAN,
	X_ETH_INBAND_VLAN,
	X_ETH_FW_ADDED_VLAN /* Driver should not use this! */,
	MAX_ETH_TX_VLAN_TYPE};


/*
 * Ethernet VLAN filtering mode in E1x
 */
enum eth_vlan_filter_mode
{
	ETH_VLAN_FILTER_ANY_VLAN /* Dont filter by vlan */,
	ETH_VLAN_FILTER_SPECIFIC_VLAN /* Only the vlan_id is allowed */,
	ETH_VLAN_FILTER_CLASSIFY /* Vlan will be added to CAM for classification */,
	MAX_ETH_VLAN_FILTER_MODE};


/*
 * MAC filtering configuration command header $$KEEP_ENDIANNESS$$
 */
struct mac_configuration_hdr
{
	uint8_t length /* number of entries valid in this command (6 bits) */;
	uint8_t offset /* offset of the first entry in the list */;
	uint16_t client_id /* the client id which this ramrod is sent on. 5b is used. */;
	uint32_t echo /* echo value to be sent to driver on event ring */;
};

/*
 * MAC address in list for ramrod $$KEEP_ENDIANNESS$$
 */
struct mac_configuration_entry
{
	uint16_t lsb_mac_addr /* 2 LSB of MAC address (should be given in big endien - driver should do hton to this number!!!) */;
	uint16_t middle_mac_addr /* 2 middle bytes of MAC address (should be given in big endien - driver should do hton to this number!!!) */;
	uint16_t msb_mac_addr /* 2 MSB of MAC address (should be given in big endien - driver should do hton to this number!!!) */;
	uint16_t vlan_id /* The inner vlan id (12b). Used either in vlan_in_cam for mac_valn pair or for vlan filtering */;
	uint8_t pf_id /* The pf id, for multi function mode */;
	uint8_t flags;
#define MAC_CONFIGURATION_ENTRY_ACTION_TYPE (0x1<<0) /* BitField flags	configures the action to be done in cam (used only is slow path handlers) (use enum set_mac_action_type) */
#define MAC_CONFIGURATION_ENTRY_ACTION_TYPE_SHIFT 0
#define MAC_CONFIGURATION_ENTRY_RDMA_MAC (0x1<<1) /* BitField flags	If set, this MAC also belongs to RDMA client */
#define MAC_CONFIGURATION_ENTRY_RDMA_MAC_SHIFT 1
#define MAC_CONFIGURATION_ENTRY_VLAN_FILTERING_MODE (0x3<<2) /* BitField flags	 (use enum eth_vlan_filter_mode) */
#define MAC_CONFIGURATION_ENTRY_VLAN_FILTERING_MODE_SHIFT 2
#define MAC_CONFIGURATION_ENTRY_OVERRIDE_VLAN_REMOVAL (0x1<<4) /* BitField flags	BitField flags  0 - cant remove vlan 1 - can remove vlan. relevant only to everest1 */
#define MAC_CONFIGURATION_ENTRY_OVERRIDE_VLAN_REMOVAL_SHIFT 4
#define MAC_CONFIGURATION_ENTRY_BROADCAST (0x1<<5) /* BitField flags	BitField flags   0 - not broadcast 1 - broadcast. relevant only to everest1 */
#define MAC_CONFIGURATION_ENTRY_BROADCAST_SHIFT 5
#define MAC_CONFIGURATION_ENTRY_RESERVED1 (0x3<<6) /* BitField flags	 */
#define MAC_CONFIGURATION_ENTRY_RESERVED1_SHIFT 6
	uint16_t reserved0;
	uint32_t clients_bit_vector /* Bit vector for the clients which should receive this MAC. */;
};

/*
 * MAC filtering configuration command
 */
struct mac_configuration_cmd
{
	struct mac_configuration_hdr hdr /* header */;
	struct mac_configuration_entry config_table[64] /* table of 64 MAC configuration entries: addresses and target table entries */;
};


/*
 * Set-MAC command type (in E1x)
 */
enum set_mac_action_type
{
	T_ETH_MAC_COMMAND_INVALIDATE,
	T_ETH_MAC_COMMAND_SET,
	MAX_SET_MAC_ACTION_TYPE};


/*
 * Ethernet TPA Modes
 */
enum tpa_mode
{
	TPA_LRO /* LRO mode TPA */,
	TPA_GRO /* GRO mode TPA */,
	MAX_TPA_MODE};


/*
 * tpa update ramrod data $$KEEP_ENDIANNESS$$
 */
struct tpa_update_ramrod_data
{
	uint8_t update_ipv4 /* none, enable or disable */;
	uint8_t update_ipv6 /* none, enable or disable */;
	uint8_t client_id /* client init flow control data */;
	uint8_t max_tpa_queues /* maximal TPA queues allowed for this client */;
	uint8_t max_sges_for_packet /* The maximal number of SGEs that can be used for one packet. depends on MTU and SGE size. must be 0 if SGEs are disabled */;
	uint8_t complete_on_both_clients /* If set and the client has different sp_client, completion will be sent to both rings */;
	uint8_t dont_verify_rings_pause_thr_flg /* If set, the rings pause thresholds will not be verified by firmware. */;
	uint8_t tpa_mode /* TPA mode to use (LRO or GRO) */;
	uint16_t sge_buff_size /* Size of the buffers pointed by SGEs */;
	uint16_t max_agg_size /* maximal size for the aggregated TPA packets, reprted by the host */;
	uint32_t sge_page_base_lo /* The address to fetch the next sges from (low) */;
	uint32_t sge_page_base_hi /* The address to fetch the next sges from (high) */;
	uint16_t sge_pause_thr_low /* number of remaining sges under which, we send pause message */;
	uint16_t sge_pause_thr_high /* number of remaining sges above which, we send un-pause message */;
};


/*
 * approximate-match multicast filtering for E1H per function in Tstorm
 */
struct tstorm_eth_approximate_match_multicast_filtering
{
	uint32_t mcast_add_hash_bit_array[8] /* Bit array for multicast hash filtering.Each bit supports a hash function result if to accept this multicast dst address. */;
};


/*
 * Common configuration parameters per function in Tstorm $$KEEP_ENDIANNESS$$
 */
struct tstorm_eth_function_common_config
{
	uint16_t config_flags;
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_CAPABILITY (0x1<<0) /* BitField config_flagsGeneral configuration flags	configuration of the port RSS IpV4 2-tupple capability */
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_CAPABILITY_SHIFT 0
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_TCP_CAPABILITY (0x1<<1) /* BitField config_flagsGeneral configuration flags	configuration of the port RSS IpV4 4-tupple capability */
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV4_TCP_CAPABILITY_SHIFT 1
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_CAPABILITY (0x1<<2) /* BitField config_flagsGeneral configuration flags	configuration of the port RSS IpV4 2-tupple capability */
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_CAPABILITY_SHIFT 2
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_TCP_CAPABILITY (0x1<<3) /* BitField config_flagsGeneral configuration flags	configuration of the port RSS IpV6 4-tupple capability */
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_IPV6_TCP_CAPABILITY_SHIFT 3
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_MODE (0x7<<4) /* BitField config_flagsGeneral configuration flags	RSS mode of operation (use enum eth_rss_mode) */
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_RSS_MODE_SHIFT 4
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_VLAN_FILTERING_ENABLE (0x1<<7) /* BitField config_flagsGeneral configuration flags	0 - Dont filter by vlan, 1 - Filter according to the vlans specificied in mac_filter_config */
#define TSTORM_ETH_FUNCTION_COMMON_CONFIG_VLAN_FILTERING_ENABLE_SHIFT 7
#define __TSTORM_ETH_FUNCTION_COMMON_CONFIG_RESERVED0 (0xFF<<8) /* BitField config_flagsGeneral configuration flags	 */
#define __TSTORM_ETH_FUNCTION_COMMON_CONFIG_RESERVED0_SHIFT 8
	uint8_t rss_result_mask /* The mask for the lower byte of RSS result - defines which section of the indirection table will be used. To enable all table put here 0x7F */;
	uint8_t reserved1;
	uint16_t vlan_id[2] /* VLANs of this function. VLAN filtering is determine according to vlan_filtering_enable. */;
};


/*
 * MAC filtering configuration parameters per port in Tstorm $$KEEP_ENDIANNESS$$
 */
struct tstorm_eth_mac_filter_config
{
	uint32_t ucast_drop_all /* bit vector in which the clients which drop all unicast packets are set */;
	uint32_t ucast_accept_all /* bit vector in which clients that accept all unicast packets are set */;
	uint32_t mcast_drop_all /* bit vector in which the clients which drop all multicast packets are set */;
	uint32_t mcast_accept_all /* bit vector in which clients that accept all multicast packets are set */;
	uint32_t bcast_accept_all /* bit vector in which clients that accept all broadcast packets are set */;
	uint32_t vlan_filter[2] /* bit vector for VLAN filtering. Clients which enforce filtering of vlan[x] should be marked in vlan_filter[x]. In E1 only vlan_filter[1] is checked. The primary vlan is taken from the CAM target table. */;
	uint32_t unmatched_unicast /* bit vector in which clients that accept unmatched unicast packets are set */;
};


/*
 * tx only queue init ramrod data $$KEEP_ENDIANNESS$$
 */
struct tx_queue_init_ramrod_data
{
	struct client_init_general_data general /* client init general data */;
	struct client_init_tx_data tx /* client init tx data */;
};


/*
 * Three RX producers for ETH
 */
struct ustorm_eth_rx_producers
{
#if defined(__BIG_ENDIAN)
	uint16_t bd_prod /* Producer of the RX BD ring */;
	uint16_t cqe_prod /* Producer of the RX CQE ring */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t cqe_prod /* Producer of the RX CQE ring */;
	uint16_t bd_prod /* Producer of the RX BD ring */;
#endif
#if defined(__BIG_ENDIAN)
	uint16_t reserved;
	uint16_t sge_prod /* Producer of the RX SGE ring */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t sge_prod /* Producer of the RX SGE ring */;
	uint16_t reserved;
#endif
};


/*
 * The data afex vif list ramrod need $$KEEP_ENDIANNESS$$
 */
struct afex_vif_list_ramrod_data
{
	uint8_t afex_vif_list_command /* set get, clear all a VIF list id defined by enum vif_list_rule_kind */;
	uint8_t func_bit_map /* the function bit map to set */;
	uint16_t vif_list_index /* the VIF list, in a per pf vector  to add this function to */;
	uint8_t func_to_clear /* the func id to clear in case of clear func mode */;
	uint8_t echo;
	uint16_t reserved1;
};


/*
 * cfc delete event data  $$KEEP_ENDIANNESS$$
 */
struct cfc_del_event_data
{
	uint32_t cid /* cid of deleted connection */;
	uint32_t reserved0;
	uint32_t reserved1;
};


/*
 * per-port SAFC demo variables
 */
struct cmng_flags_per_port
{
	uint32_t cmng_enables;
#define CMNG_FLAGS_PER_PORT_FAIRNESS_VN (0x1<<0) /* BitField cmng_enablesenables flag for fairness and rate shaping between protocols, vnics and COSes	if set, enable fairness between vnics */
#define CMNG_FLAGS_PER_PORT_FAIRNESS_VN_SHIFT 0
#define CMNG_FLAGS_PER_PORT_RATE_SHAPING_VN (0x1<<1) /* BitField cmng_enablesenables flag for fairness and rate shaping between protocols, vnics and COSes	if set, enable rate shaping between vnics */
#define CMNG_FLAGS_PER_PORT_RATE_SHAPING_VN_SHIFT 1
#define CMNG_FLAGS_PER_PORT_FAIRNESS_COS (0x1<<2) /* BitField cmng_enablesenables flag for fairness and rate shaping between protocols, vnics and COSes	if set, enable fairness between COSes */
#define CMNG_FLAGS_PER_PORT_FAIRNESS_COS_SHIFT 2
#define CMNG_FLAGS_PER_PORT_FAIRNESS_COS_MODE (0x1<<3) /* BitField cmng_enablesenables flag for fairness and rate shaping between protocols, vnics and COSes	 (use enum fairness_mode) */
#define CMNG_FLAGS_PER_PORT_FAIRNESS_COS_MODE_SHIFT 3
#define __CMNG_FLAGS_PER_PORT_RESERVED0 (0xFFFFFFF<<4) /* BitField cmng_enablesenables flag for fairness and rate shaping between protocols, vnics and COSes	reserved */
#define __CMNG_FLAGS_PER_PORT_RESERVED0_SHIFT 4
	uint32_t __reserved1;
};


/*
 * per-port rate shaping variables
 */
struct rate_shaping_vars_per_port
{
	uint32_t rs_periodic_timeout /* timeout of periodic timer */;
	uint32_t rs_threshold /* threshold, below which we start to stop queues */;
};

/*
 * per-port fairness variables
 */
struct fairness_vars_per_port
{
	uint32_t upper_bound /* Quota for a protocol/vnic */;
	uint32_t fair_threshold /* almost-empty threshold */;
	uint32_t fairness_timeout /* timeout of fairness timer */;
	uint32_t reserved0;
};

/*
 * per-port SAFC variables
 */
struct safc_struct_per_port
{
#if defined(__BIG_ENDIAN)
	uint16_t __reserved1;
	uint8_t __reserved0;
	uint8_t safc_timeout_usec /* timeout to stop queues on SAFC pause command */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t safc_timeout_usec /* timeout to stop queues on SAFC pause command */;
	uint8_t __reserved0;
	uint16_t __reserved1;
#endif
	uint8_t cos_to_traffic_types[MAX_COS_NUMBER] /* translate cos to service traffics types */;
	uint16_t cos_to_pause_mask[NUM_OF_SAFC_BITS] /* QM pause mask for each class of service in the SAFC frame */;
};

/*
 * Per-port congestion management variables
 */
struct cmng_struct_per_port
{
	struct rate_shaping_vars_per_port rs_vars;
	struct fairness_vars_per_port fair_vars;
	struct safc_struct_per_port safc_vars;
	struct cmng_flags_per_port flags;
};

/*
 * a single rate shaping counter. can be used as protocol or vnic counter
 */
struct rate_shaping_counter
{
	uint32_t quota /* Quota for a protocol/vnic */;
#if defined(__BIG_ENDIAN)
	uint16_t __reserved0;
	uint16_t rate /* Vnic/Protocol rate in units of Mega-bits/sec */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t rate /* Vnic/Protocol rate in units of Mega-bits/sec */;
	uint16_t __reserved0;
#endif
};

/*
 * per-vnic rate shaping variables
 */
struct rate_shaping_vars_per_vn
{
	struct rate_shaping_counter vn_counter /* per-vnic counter */;
};

/*
 * per-vnic fairness variables
 */
struct fairness_vars_per_vn
{
	uint32_t cos_credit_delta[MAX_COS_NUMBER] /* used for incrementing the credit */;
	uint32_t vn_credit_delta /* used for incrementing the credit */;
	uint32_t __reserved0;
};

/*
 * cmng port init state
 */
struct cmng_vnic
{
	struct rate_shaping_vars_per_vn vnic_max_rate[4];
	struct fairness_vars_per_vn vnic_min_rate[4];
};

/*
 * cmng port init state
 */
struct cmng_init
{
	struct cmng_struct_per_port port;
	struct cmng_vnic vnic;
};


/*
 * driver parameters for congestion management init, all rates are in Mbps
 */
struct cmng_init_input
{
	uint32_t port_rate;
	uint16_t vnic_min_rate[4] /* rates are in Mbps */;
	uint16_t vnic_max_rate[4] /* rates are in Mbps */;
	uint16_t cos_min_rate[MAX_COS_NUMBER] /* rates are in Mbps */;
	uint16_t cos_to_pause_mask[MAX_COS_NUMBER];
	struct cmng_flags_per_port flags;
};


/*
 * Protocol-common command ID for slow path elements
 */
enum common_spqe_cmd_id
{
	RAMROD_CMD_ID_COMMON_UNUSED,
	RAMROD_CMD_ID_COMMON_FUNCTION_START /* Start a function (for PFs only) */,
	RAMROD_CMD_ID_COMMON_FUNCTION_STOP /* Stop a function (for PFs only) */,
	RAMROD_CMD_ID_COMMON_FUNCTION_UPDATE /* niv update function */,
	RAMROD_CMD_ID_COMMON_CFC_DEL /* Delete a connection from CFC */,
	RAMROD_CMD_ID_COMMON_CFC_DEL_WB /* Delete a connection from CFC (with write back) */,
	RAMROD_CMD_ID_COMMON_STAT_QUERY /* Collect statistics counters */,
	RAMROD_CMD_ID_COMMON_STOP_TRAFFIC /* Stop Tx traffic (before DCB updates) */,
	RAMROD_CMD_ID_COMMON_START_TRAFFIC /* Start Tx traffic (after DCB updates) */,
	RAMROD_CMD_ID_COMMON_AFEX_VIF_LISTS /* niv vif lists */,
	RAMROD_CMD_ID_COMMON_SET_TIMESYNC /* Set Timesync Parameters (E3 Only) */,
	MAX_COMMON_SPQE_CMD_ID};


/*
 * Per-protocol connection types
 */
enum connection_type
{
	ETH_CONNECTION_TYPE /* Ethernet */,
	TOE_CONNECTION_TYPE /* TOE */,
	RDMA_CONNECTION_TYPE /* RDMA */,
	ISCSI_CONNECTION_TYPE /* iSCSI */,
	FCOE_CONNECTION_TYPE /* FCoE */,
	RESERVED_CONNECTION_TYPE_0,
	RESERVED_CONNECTION_TYPE_1,
	RESERVED_CONNECTION_TYPE_2,
	NONE_CONNECTION_TYPE /* General- used for common slow path */,
	MAX_CONNECTION_TYPE};


/*
 * Cos modes
 */
enum cos_mode
{
	OVERRIDE_COS /* Firmware deduce cos according to DCB */,
	STATIC_COS /* Firmware has constant queues per CoS */,
	FW_WRR /* Firmware keep fairness between different CoSes */,
	MAX_COS_MODE};


/*
 * Dynamic HC counters set by the driver
 */
struct hc_dynamic_drv_counter
{
	uint32_t val[HC_SB_MAX_DYNAMIC_INDICES] /* 4 bytes * 4 indices = 2 lines */;
};

/*
 * zone A per-queue data
 */
struct cstorm_queue_zone_data
{
	struct hc_dynamic_drv_counter hc_dyn_drv_cnt /* 4 bytes * 4 indices = 2 lines */;
	struct regpair reserved[2];
};


/*
 * Vf-PF channel data in cstorm ram (non-triggered zone)
 */
struct vf_pf_channel_zone_data
{
	uint32_t msg_addr_lo /* the message address on VF memory */;
	uint32_t msg_addr_hi /* the message address on VF memory */;
};

/*
 * zone for VF non-triggered data
 */
struct non_trigger_vf_zone
{
	struct vf_pf_channel_zone_data vf_pf_channel /* vf-pf channel zone data */;
};

/*
 * Vf-PF channel trigger zone in cstorm ram
 */
struct vf_pf_channel_zone_trigger
{
	uint8_t addr_valid /* indicates that a vf-pf message is pending. MUST be set AFTER the message address.  */;
};

/*
 * zone that triggers the in-bound interrupt
 */
struct trigger_vf_zone
{
#if defined(__BIG_ENDIAN)
	uint16_t reserved1;
	uint8_t reserved0;
	struct vf_pf_channel_zone_trigger vf_pf_channel;
#elif defined(__LITTLE_ENDIAN)
	struct vf_pf_channel_zone_trigger vf_pf_channel;
	uint8_t reserved0;
	uint16_t reserved1;
#endif
	uint32_t reserved2;
};

/*
 * zone B per-VF data
 */
struct cstorm_vf_zone_data
{
	struct non_trigger_vf_zone non_trigger /* zone for VF non-triggered data */;
	struct trigger_vf_zone trigger /* zone that triggers the in-bound interrupt */;
};


/*
 * Dynamic host coalescing init parameters, per state machine
 */
struct dynamic_hc_sm_config
{
	uint32_t threshold[3] /* thresholds of number of outstanding bytes */;
	uint8_t shift_per_protocol[HC_SB_MAX_DYNAMIC_INDICES] /* bytes difference of each protocol is shifted right by this value */;
	uint8_t hc_timeout0[HC_SB_MAX_DYNAMIC_INDICES] /* timeout for level 0 for each protocol, in units of usec */;
	uint8_t hc_timeout1[HC_SB_MAX_DYNAMIC_INDICES] /* timeout for level 1 for each protocol, in units of usec */;
	uint8_t hc_timeout2[HC_SB_MAX_DYNAMIC_INDICES] /* timeout for level 2 for each protocol, in units of usec */;
	uint8_t hc_timeout3[HC_SB_MAX_DYNAMIC_INDICES] /* timeout for level 3 for each protocol, in units of usec */;
};

/*
 * Dynamic host coalescing init parameters
 */
struct dynamic_hc_config
{
	struct dynamic_hc_sm_config sm_config[HC_SB_MAX_SM] /* Configuration per state machine */;
};


struct e2_integ_data
{
#if defined(__BIG_ENDIAN)
	uint8_t flags;
#define E2_INTEG_DATA_TESTING_EN (0x1<<0) /* BitField flags	integration testing enabled */
#define E2_INTEG_DATA_TESTING_EN_SHIFT 0
#define E2_INTEG_DATA_LB_TX (0x1<<1) /* BitField flags	flag indicating this connection will transmit on loopback */
#define E2_INTEG_DATA_LB_TX_SHIFT 1
#define E2_INTEG_DATA_COS_TX (0x1<<2) /* BitField flags	flag indicating this connection will transmit according to cos field */
#define E2_INTEG_DATA_COS_TX_SHIFT 2
#define E2_INTEG_DATA_OPPORTUNISTICQM (0x1<<3) /* BitField flags	flag indicating this connection will activate the opportunistic QM credit flow */
#define E2_INTEG_DATA_OPPORTUNISTICQM_SHIFT 3
#define E2_INTEG_DATA_DPMTESTRELEASEDQ (0x1<<4) /* BitField flags	flag indicating this connection will release the door bell queue (DQ) */
#define E2_INTEG_DATA_DPMTESTRELEASEDQ_SHIFT 4
#define E2_INTEG_DATA_RESERVED (0x7<<5) /* BitField flags	 */
#define E2_INTEG_DATA_RESERVED_SHIFT 5
	uint8_t cos /* cos of the connection (relevant only in cos transmitting connections, when cosTx is set */;
	uint8_t voq /* voq to return credit on. Normally equal to port (i.e. always 0 in E2 operational connections). in cos tests equal to cos. in loopback tests equal to LB_PORT (=4) */;
	uint8_t pbf_queue /* pbf queue to transmit on. Normally equal to port (i.e. always 0 in E2 operational connections). in cos tests equal to cos. in loopback tests equal to LB_PORT (=4) */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t pbf_queue /* pbf queue to transmit on. Normally equal to port (i.e. always 0 in E2 operational connections). in cos tests equal to cos. in loopback tests equal to LB_PORT (=4) */;
	uint8_t voq /* voq to return credit on. Normally equal to port (i.e. always 0 in E2 operational connections). in cos tests equal to cos. in loopback tests equal to LB_PORT (=4) */;
	uint8_t cos /* cos of the connection (relevant only in cos transmitting connections, when cosTx is set */;
	uint8_t flags;
#define E2_INTEG_DATA_TESTING_EN (0x1<<0) /* BitField flags	integration testing enabled */
#define E2_INTEG_DATA_TESTING_EN_SHIFT 0
#define E2_INTEG_DATA_LB_TX (0x1<<1) /* BitField flags	flag indicating this connection will transmit on loopback */
#define E2_INTEG_DATA_LB_TX_SHIFT 1
#define E2_INTEG_DATA_COS_TX (0x1<<2) /* BitField flags	flag indicating this connection will transmit according to cos field */
#define E2_INTEG_DATA_COS_TX_SHIFT 2
#define E2_INTEG_DATA_OPPORTUNISTICQM (0x1<<3) /* BitField flags	flag indicating this connection will activate the opportunistic QM credit flow */
#define E2_INTEG_DATA_OPPORTUNISTICQM_SHIFT 3
#define E2_INTEG_DATA_DPMTESTRELEASEDQ (0x1<<4) /* BitField flags	flag indicating this connection will release the door bell queue (DQ) */
#define E2_INTEG_DATA_DPMTESTRELEASEDQ_SHIFT 4
#define E2_INTEG_DATA_RESERVED (0x7<<5) /* BitField flags	 */
#define E2_INTEG_DATA_RESERVED_SHIFT 5
#endif
#if defined(__BIG_ENDIAN)
	uint16_t reserved3;
	uint8_t reserved2;
	uint8_t ramEn /* context area reserved for reading enable bit from ram */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t ramEn /* context area reserved for reading enable bit from ram */;
	uint8_t reserved2;
	uint16_t reserved3;
#endif
};


/*
 * set mac event data  $$KEEP_ENDIANNESS$$
 */
struct eth_event_data
{
	uint32_t echo /* set mac echo data to return to driver */;
	uint32_t reserved0;
	uint32_t reserved1;
};


/*
 * pf-vf event data  $$KEEP_ENDIANNESS$$
 */
struct vf_pf_event_data
{
	uint8_t vf_id /* VF ID (0-63) */;
	uint8_t reserved0;
	uint16_t reserved1;
	uint32_t msg_addr_lo /* message address on Vf (low 32 bits) */;
	uint32_t msg_addr_hi /* message address on Vf (high 32 bits) */;
};

/*
 * VF FLR event data  $$KEEP_ENDIANNESS$$
 */
struct vf_flr_event_data
{
	uint8_t vf_id /* VF ID (0-63) */;
	uint8_t reserved0;
	uint16_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
};

/*
 * malicious VF event data  $$KEEP_ENDIANNESS$$
 */
struct malicious_vf_event_data
{
	uint8_t vf_id /* VF ID (0-63) */;
	uint8_t err_id /* reason for malicious notification */;
	uint16_t reserved1;
	uint32_t reserved2;
	uint32_t reserved3;
};

/*
 * vif list event data  $$KEEP_ENDIANNESS$$
 */
struct vif_list_event_data
{
	uint8_t func_bit_map /* bit map of pf indice */;
	uint8_t echo;
	uint16_t reserved0;
	uint32_t reserved1;
	uint32_t reserved2;
};

/*
 * function update event data  $$KEEP_ENDIANNESS$$
 */
struct function_update_event_data
{
	uint8_t echo;
	uint8_t reserved;
	uint16_t reserved0;
	uint32_t reserved1;
	uint32_t reserved2;
};

/*
 * union for all event ring message types
 */
union event_data
{
	struct vf_pf_event_data vf_pf_event /* vf-pf event data */;
	struct eth_event_data eth_event /* set mac event data */;
	struct cfc_del_event_data cfc_del_event /* cfc delete event data */;
	struct vf_flr_event_data vf_flr_event /* vf flr event data */;
	struct malicious_vf_event_data malicious_vf_event /* malicious vf event data */;
	struct vif_list_event_data vif_list_event /* vif list event data */;
	struct function_update_event_data function_update_event /* function update event data */;
};


/*
 * per PF event ring data
 */
struct event_ring_data
{
	struct regpair_native base_addr /* ring base address */;
#if defined(__BIG_ENDIAN)
	uint8_t index_id /* index ID within the status block */;
	uint8_t sb_id /* status block ID */;
	uint16_t producer /* event ring producer */;
#elif defined(__LITTLE_ENDIAN)
	uint16_t producer /* event ring producer */;
	uint8_t sb_id /* status block ID */;
	uint8_t index_id /* index ID within the status block */;
#endif
	uint32_t reserved0;
};


/*
 * event ring message element (each element is 128 bits) $$KEEP_ENDIANNESS$$
 */
struct event_ring_msg
{
	uint8_t opcode;
	uint8_t error /* error on the mesasage */;
	uint16_t reserved1;
	union event_data data /* message data (96 bits data) */;
};

/*
 * event ring next page element (128 bits)
 */
struct event_ring_next
{
	struct regpair addr /* Address of the next page of the ring */;
	uint32_t reserved[2];
};

/*
 * union for event ring element types (each element is 128 bits)
 */
union event_ring_elem
{
	struct event_ring_msg message /* event ring message */;
	struct event_ring_next next_page /* event ring next page */;
};


/*
 * Common event ring opcodes
 */
enum event_ring_opcode
{
	EVENT_RING_OPCODE_VF_PF_CHANNEL,
	EVENT_RING_OPCODE_FUNCTION_START /* Start a function (for PFs only) */,
	EVENT_RING_OPCODE_FUNCTION_STOP /* Stop a function (for PFs only) */,
	EVENT_RING_OPCODE_CFC_DEL /* Delete a connection from CFC */,
	EVENT_RING_OPCODE_CFC_DEL_WB /* Delete a connection from CFC (with write back) */,
	EVENT_RING_OPCODE_STAT_QUERY /* Collect statistics counters */,
	EVENT_RING_OPCODE_STOP_TRAFFIC /* Stop Tx traffic (before DCB updates) */,
	EVENT_RING_OPCODE_START_TRAFFIC /* Start Tx traffic (after DCB updates) */,
	EVENT_RING_OPCODE_VF_FLR /* VF FLR indication for PF */,
	EVENT_RING_OPCODE_MALICIOUS_VF /* Malicious VF operation detected */,
	EVENT_RING_OPCODE_FORWARD_SETUP /* Initialize forward channel */,
	EVENT_RING_OPCODE_RSS_UPDATE_RULES /* Update RSS configuration */,
	EVENT_RING_OPCODE_FUNCTION_UPDATE /* function update */,
	EVENT_RING_OPCODE_AFEX_VIF_LISTS /* event ring opcode niv vif lists */,
	EVENT_RING_OPCODE_SET_MAC /* Add/remove MAC (in E1x only) */,
	EVENT_RING_OPCODE_CLASSIFICATION_RULES /* Add/remove MAC or VLAN (in E2/E3 only) */,
	EVENT_RING_OPCODE_FILTERS_RULES /* Add/remove classification filters for L2 client (in E2/E3 only) */,
	EVENT_RING_OPCODE_MULTICAST_RULES /* Add/remove multicast classification bin (in E2/E3 only) */,
	EVENT_RING_OPCODE_SET_TIMESYNC /* Set Timesync Parameters (E3 Only) */,
	MAX_EVENT_RING_OPCODE};


/*
 * Modes for fairness algorithm
 */
enum fairness_mode
{
	FAIRNESS_COS_WRR_MODE /* Weighted round robin mode (used in Google) */,
	FAIRNESS_COS_ETS_MODE /* ETS mode (used in FCoE) */,
	MAX_FAIRNESS_MODE};


/*
 * Priority and cos $$KEEP_ENDIANNESS$$
 */
struct priority_cos
{
	uint8_t priority /* Priority */;
	uint8_t cos /* Cos */;
	uint16_t reserved1;
};

/*
 * The data for flow control configuration $$KEEP_ENDIANNESS$$
 */
struct flow_control_configuration
{
	struct priority_cos traffic_type_to_priority_cos[MAX_TRAFFIC_TYPES] /* traffic_type to priority cos */;
	uint8_t dcb_enabled /* If DCB mode is enabled then traffic class to priority array is fully initialized and there must be inner VLAN */;
	uint8_t dcb_version /* DCB version Increase by one on each DCB update */;
	uint8_t dont_add_pri_0 /* In case, the priority is 0, and the packet has no vlan, the firmware wont add vlan */;
	uint8_t reserved1;
	uint32_t reserved2;
};


/*
 *  $$KEEP_ENDIANNESS$$
 */
struct function_start_data
{
	uint8_t function_mode /* the function mode */;
	uint8_t allow_npar_tx_switching /* If set, inter-pf tx switching is allowed in Switch Independant function mode. (E2/E3 Only) */;
	uint16_t sd_vlan_tag /* value of Vlan in case of switch depended multi-function mode */;
	uint16_t vif_id /* value of VIF id in case of NIV multi-function mode */;
	uint8_t path_id;
	uint8_t network_cos_mode /* The cos mode for network traffic. */;
	uint8_t dmae_cmd_id /* The DMAE command id to use for FW DMAE transactions */;
	uint8_t gre_tunnel_mode /* GRE Tunnel Mode to enable on the Function (E2/E3 Only) */;
	uint8_t gre_tunnel_rss /* Type of RSS to perform on GRE Tunneled packets */;
	uint8_t nvgre_clss_en /* If set, NVGRE tunneled packets are classified according to their inner MAC (gre_mode must be NVGRE_TUNNEL) */;
	uint16_t reserved1[2];
};


/*
 *  $$KEEP_ENDIANNESS$$
 */
struct function_update_data
{
	uint8_t vif_id_change_flg /* If set, vif_id will be checked */;
	uint8_t afex_default_vlan_change_flg /* If set, afex_default_vlan will be checked */;
	uint8_t allowed_priorities_change_flg /* If set, allowed_priorities will be checked */;
	uint8_t network_cos_mode_change_flg /* If set, network_cos_mode will be checked */;
	uint16_t vif_id /* value of VIF id in case of NIV multi-function mode */;
	uint16_t afex_default_vlan /* value of default Vlan in case of NIV mf */;
	uint8_t allowed_priorities /* bit vector of allowed Vlan priorities for this VIF */;
	uint8_t network_cos_mode /* The cos mode for network traffic. */;
	uint8_t lb_mode_en_change_flg /* If set, lb_mode_en will be checked */;
	uint8_t lb_mode_en /* If set, niv loopback mode will be enabled */;
	uint8_t tx_switch_suspend_change_flg /* If set, tx_switch_suspend will be checked */;
	uint8_t tx_switch_suspend /* If set, TX switching TO this function will be disabled and packets will be dropped */;
	uint8_t echo;
	uint8_t reserved1;
	uint8_t update_gre_cfg_flg /* If set, GRE config for the function will be updated according to the gre_tunnel_rss and nvgre_clss_en fields */;
	uint8_t gre_tunnel_mode /* GRE Tunnel Mode to enable on the Function (E2/E3 Only) */;
	uint8_t gre_tunnel_rss /* Type of RSS to perform on GRE Tunneled packets */;
	uint8_t nvgre_clss_en /* If set, NVGRE tunneled packets are classified according to their inner MAC (gre_mode must be NVGRE_TUNNEL) */;
	uint32_t reserved3;
};


/*
 * FW version stored in the Xstorm RAM
 */
struct fw_version
{
#if defined(__BIG_ENDIAN)
	uint8_t engineering /* firmware current engineering version */;
	uint8_t revision /* firmware current revision version */;
	uint8_t minor /* firmware current minor version */;
	uint8_t major /* firmware current major version */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t major /* firmware current major version */;
	uint8_t minor /* firmware current minor version */;
	uint8_t revision /* firmware current revision version */;
	uint8_t engineering /* firmware current engineering version */;
#endif
	uint32_t flags;
#define FW_VERSION_OPTIMIZED (0x1<<0) /* BitField flags	if set, this is optimized ASM */
#define FW_VERSION_OPTIMIZED_SHIFT 0
#define FW_VERSION_BIG_ENDIEN (0x1<<1) /* BitField flags	if set, this is big-endien ASM */
#define FW_VERSION_BIG_ENDIEN_SHIFT 1
#define FW_VERSION_CHIP_VERSION (0x3<<2) /* BitField flags	0 - E1, 1 - E1H */
#define FW_VERSION_CHIP_VERSION_SHIFT 2
#define __FW_VERSION_RESERVED (0xFFFFFFF<<4) /* BitField flags	 */
#define __FW_VERSION_RESERVED_SHIFT 4
};


/*
 * GRE RSS Mode
 */
enum gre_rss_mode
{
	GRE_OUTER_HEADERS_RSS /* RSS for GRE Packets is performed on the outer headers */,
	GRE_INNER_HEADERS_RSS /* RSS for GRE Packets is performed on the inner headers */,
	NVGRE_KEY_ENTROPY_RSS /* RSS for NVGRE Packets is done based on a hash containing the entropy bits from the GRE Key Field (gre_tunnel must be NVGRE_TUNNEL) */,
	MAX_GRE_RSS_MODE};


/*
 * GRE Tunnel Mode
 */
enum gre_tunnel_type
{
	NO_GRE_TUNNEL,
	NVGRE_TUNNEL /* NV-GRE Tunneling Microsoft L2 over GRE. GRE header contains mandatory Key Field. */,
	L2GRE_TUNNEL /* L2-GRE Tunneling General L2 over GRE. GRE can contain Key field with Tenant ID and Sequence Field */,
	IPGRE_TUNNEL /* IP-GRE Tunneling IP over GRE. GRE may contain Key field with Tenant ID, Sequence Field and/or Checksum Field */,
	MAX_GRE_TUNNEL_TYPE};


/*
 * Dynamic Host-Coalescing - Driver(host) counters
 */
struct hc_dynamic_sb_drv_counters
{
	uint32_t dynamic_hc_drv_counter[HC_SB_MAX_DYNAMIC_INDICES] /* Dynamic HC counters written by drivers */;
};


/*
 * 2 bytes. configuration/state parameters for a single protocol index
 */
struct hc_index_data
{
#if defined(__BIG_ENDIAN)
	uint8_t flags;
#define HC_INDEX_DATA_SM_ID (0x1<<0) /* BitField flags	Index to a state machine. Can be 0 or 1 */
#define HC_INDEX_DATA_SM_ID_SHIFT 0
#define HC_INDEX_DATA_HC_ENABLED (0x1<<1) /* BitField flags	if set, host coalescing would be done for this index */
#define HC_INDEX_DATA_HC_ENABLED_SHIFT 1
#define HC_INDEX_DATA_DYNAMIC_HC_ENABLED (0x1<<2) /* BitField flags	if set, dynamic HC will be done for this index */
#define HC_INDEX_DATA_DYNAMIC_HC_ENABLED_SHIFT 2
#define HC_INDEX_DATA_RESERVE (0x1F<<3) /* BitField flags	 */
#define HC_INDEX_DATA_RESERVE_SHIFT 3
	uint8_t timeout /* the timeout values for this index. Units are 4 usec */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t timeout /* the timeout values for this index. Units are 4 usec */;
	uint8_t flags;
#define HC_INDEX_DATA_SM_ID (0x1<<0) /* BitField flags	Index to a state machine. Can be 0 or 1 */
#define HC_INDEX_DATA_SM_ID_SHIFT 0
#define HC_INDEX_DATA_HC_ENABLED (0x1<<1) /* BitField flags	if set, host coalescing would be done for this index */
#define HC_INDEX_DATA_HC_ENABLED_SHIFT 1
#define HC_INDEX_DATA_DYNAMIC_HC_ENABLED (0x1<<2) /* BitField flags	if set, dynamic HC will be done for this index */
#define HC_INDEX_DATA_DYNAMIC_HC_ENABLED_SHIFT 2
#define HC_INDEX_DATA_RESERVE (0x1F<<3) /* BitField flags	 */
#define HC_INDEX_DATA_RESERVE_SHIFT 3
#endif
};


/*
 * HC state-machine
 */
struct hc_status_block_sm
{
#if defined(__BIG_ENDIAN)
	uint8_t igu_seg_id;
	uint8_t igu_sb_id /* sb_id within the IGU */;
	uint8_t timer_value /* Determines the time_to_expire */;
	uint8_t __flags;
#elif defined(__LITTLE_ENDIAN)
	uint8_t __flags;
	uint8_t timer_value /* Determines the time_to_expire */;
	uint8_t igu_sb_id /* sb_id within the IGU */;
	uint8_t igu_seg_id;
#endif
	uint32_t time_to_expire /* The time in which it expects to wake up */;
};

/*
 * hold PCI identification variables- used in various places in firmware
 */
struct pci_entity
{
#if defined(__BIG_ENDIAN)
	uint8_t vf_valid /* If set, this is a VF, otherwise it is PF */;
	uint8_t vf_id /* VF ID (0-63). Value of 0xFF means VF not valid */;
	uint8_t vnic_id /* Virtual NIC ID (0-3) */;
	uint8_t pf_id /* PCI physical function number (0-7). The LSB of this field is the port ID */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t pf_id /* PCI physical function number (0-7). The LSB of this field is the port ID */;
	uint8_t vnic_id /* Virtual NIC ID (0-3) */;
	uint8_t vf_id /* VF ID (0-63). Value of 0xFF means VF not valid */;
	uint8_t vf_valid /* If set, this is a VF, otherwise it is PF */;
#endif
};

/*
 * The fast-path status block meta-data, common to all chips
 */
struct hc_sb_data
{
	struct regpair_native host_sb_addr /* Host status block address */;
	struct hc_status_block_sm state_machine[HC_SB_MAX_SM] /* Holds the state machines of the status block */;
	struct pci_entity p_func /* vnic / port of the status block to be set by the driver */;
#if defined(__BIG_ENDIAN)
	uint8_t rsrv0;
	uint8_t state;
	uint8_t dhc_qzone_id /* used in E2 only, to specify the HW queue zone ID used for this status block dynamic HC counters */;
	uint8_t same_igu_sb_1b /* Indicate that both state-machines acts like single sm */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t same_igu_sb_1b /* Indicate that both state-machines acts like single sm */;
	uint8_t dhc_qzone_id /* used in E2 only, to specify the HW queue zone ID used for this status block dynamic HC counters */;
	uint8_t state;
	uint8_t rsrv0;
#endif
	struct regpair_native rsrv1[2];
};


/*
 * Segment types for host coaslescing
 */
enum hc_segment
{
	HC_REGULAR_SEGMENT,
	HC_DEFAULT_SEGMENT,
	MAX_HC_SEGMENT};


/*
 * The fast-path status block meta-data
 */
struct hc_sp_status_block_data
{
	struct regpair_native host_sb_addr /* Host status block address */;
#if defined(__BIG_ENDIAN)
	uint8_t rsrv1;
	uint8_t state;
	uint8_t igu_seg_id /* segment id of the IGU */;
	uint8_t igu_sb_id /* sb_id within the IGU */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t igu_sb_id /* sb_id within the IGU */;
	uint8_t igu_seg_id /* segment id of the IGU */;
	uint8_t state;
	uint8_t rsrv1;
#endif
	struct pci_entity p_func /* vnic / port of the status block to be set by the driver */;
};


/*
 * The fast-path status block meta-data
 */
struct hc_status_block_data_e1x
{
	struct hc_index_data index_data[HC_SB_MAX_INDICES_E1X] /* configuration/state parameters for a single protocol index */;
	struct hc_sb_data common /* The fast-path status block meta-data, common to all chips */;
};


/*
 * The fast-path status block meta-data
 */
struct hc_status_block_data_e2
{
	struct hc_index_data index_data[HC_SB_MAX_INDICES_E2] /* configuration/state parameters for a single protocol index */;
	struct hc_sb_data common /* The fast-path status block meta-data, common to all chips */;
};


/*
 * IGU block operartion modes (in Everest2)
 */
enum igu_mode
{
	HC_IGU_BC_MODE /* Backward compatible mode */,
	HC_IGU_NBC_MODE /* Non-backward compatible mode */,
	MAX_IGU_MODE};


/*
 * IP versions
 */
enum ip_ver
{
	IP_V4,
	IP_V6,
	MAX_IP_VER};


/*
 * Malicious VF error ID
 */
enum malicious_vf_error_id
{
	VF_PF_CHANNEL_NOT_READY /* Writing to VF/PF channel when it is not ready */,
	ETH_ILLEGAL_BD_LENGTHS /* TX BD lengths error was detected */,
	ETH_PACKET_TOO_SHORT /* TX packet is shorter then reported on BDs */,
	ETH_PAYLOAD_TOO_BIG /* TX packet is greater then MTU */,
	ETH_ILLEGAL_ETH_TYPE /* TX packet reported without VLAN but eth type is 0x8100 */,
	ETH_ILLEGAL_LSO_HDR_LEN /* LSO header length on BDs and on hdr_nbd do not match */,
	ETH_TOO_MANY_BDS /* Tx packet has too many BDs */,
	ETH_ZERO_HDR_NBDS /* hdr_nbds field is zero */,
	ETH_START_BD_NOT_SET /* start_bd should be set on first TX BD in packet */,
	ETH_ILLEGAL_PARSE_NBDS /* Tx packet with parse_nbds field which is not legal */,
	ETH_IPV6_AND_CHECKSUM /* Tx packet with IP checksum on IPv6 */,
	ETH_VLAN_FLG_INCORRECT /* Tx packet with incorrect VLAN flag */,
	ETH_ILLEGAL_LSO_MSS /* Tx LSO packet with illegal MSS value */,
	ETH_TUNNEL_NOT_SUPPORTED /* Tunneling packets are not supported in current connection */,
	MAX_MALICIOUS_VF_ERROR_ID};


/*
 * Multi-function modes
 */
enum mf_mode
{
	SINGLE_FUNCTION,
	MULTI_FUNCTION_SD /* Switch dependent (vlan based) */,
	MULTI_FUNCTION_SI /* Switch independent (mac based) */,
	MULTI_FUNCTION_AFEX /* Switch dependent (niv based) */,
	MAX_MF_MODE};


/*
 * Protocol-common statistics collected by the Tstorm (per pf) $$KEEP_ENDIANNESS$$
 */
struct tstorm_per_pf_stats
{
	struct regpair rcv_error_bytes /* number of bytes received with errors */;
};

/*
 *  $$KEEP_ENDIANNESS$$
 */
struct per_pf_stats
{
	struct tstorm_per_pf_stats tstorm_pf_statistics;
};


/*
 * Protocol-common statistics collected by the Tstorm (per port) $$KEEP_ENDIANNESS$$
 */
struct tstorm_per_port_stats
{
	uint32_t mac_discard /* number of packets with mac errors */;
	uint32_t mac_filter_discard /* the number of good frames dropped because of no perfect match to MAC/VLAN address */;
	uint32_t brb_truncate_discard /* the number of packtes that were dropped because they were truncated in BRB */;
	uint32_t mf_tag_discard /* the number of good frames dropped because of no match to the outer vlan/VNtag */;
	uint32_t packet_drop /* general packet drop conter- incremented for every packet drop */;
	uint32_t reserved;
};

/*
 *  $$KEEP_ENDIANNESS$$
 */
struct per_port_stats
{
	struct tstorm_per_port_stats tstorm_port_statistics;
};


/*
 * Protocol-common statistics collected by the Tstorm (per client) $$KEEP_ENDIANNESS$$
 */
struct tstorm_per_queue_stats
{
	struct regpair rcv_ucast_bytes /* number of bytes in unicast packets received without errors and pass the filter */;
	uint32_t rcv_ucast_pkts /* number of unicast packets received without errors and pass the filter */;
	uint32_t checksum_discard /* number of total packets received with checksum error */;
	struct regpair rcv_bcast_bytes /* number of bytes in broadcast packets received without errors and pass the filter */;
	uint32_t rcv_bcast_pkts /* number of packets in broadcast packets received without errors and pass the filter */;
	uint32_t pkts_too_big_discard /* number of too long packets received */;
	struct regpair rcv_mcast_bytes /* number of bytes in multicast packets received without errors and pass the filter */;
	uint32_t rcv_mcast_pkts /* number of packets in multicast packets received without errors and pass the filter */;
	uint32_t ttl0_discard /* the number of good frames dropped because of TTL=0 */;
	uint16_t no_buff_discard;
	uint16_t reserved0;
	uint32_t reserved1;
};

/*
 * Protocol-common statistics collected by the Ustorm (per client) $$KEEP_ENDIANNESS$$
 */
struct ustorm_per_queue_stats
{
	struct regpair ucast_no_buff_bytes /* the number of unicast bytes received from network dropped because of no buffer at host */;
	struct regpair mcast_no_buff_bytes /* the number of multicast bytes received from network dropped because of no buffer at host */;
	struct regpair bcast_no_buff_bytes /* the number of broadcast bytes received from network dropped because of no buffer at host */;
	uint32_t ucast_no_buff_pkts /* the number of unicast frames received from network dropped because of no buffer at host */;
	uint32_t mcast_no_buff_pkts /* the number of unicast frames received from network dropped because of no buffer at host */;
	uint32_t bcast_no_buff_pkts /* the number of unicast frames received from network dropped because of no buffer at host */;
	uint32_t coalesced_pkts /* the number of packets coalesced in all aggregations */;
	struct regpair coalesced_bytes /* the number of bytes coalesced in all aggregations */;
	uint32_t coalesced_events /* the number of aggregations */;
	uint32_t coalesced_aborts /* the number of exception which avoid aggregation */;
};

/*
 * Protocol-common statistics collected by the Xstorm (per client)  $$KEEP_ENDIANNESS$$
 */
struct xstorm_per_queue_stats
{
	struct regpair ucast_bytes_sent /* number of total bytes sent without errors */;
	struct regpair mcast_bytes_sent /* number of total bytes sent without errors */;
	struct regpair bcast_bytes_sent /* number of total bytes sent without errors */;
	uint32_t ucast_pkts_sent /* number of total packets sent without errors */;
	uint32_t mcast_pkts_sent /* number of total packets sent without errors */;
	uint32_t bcast_pkts_sent /* number of total packets sent without errors */;
	uint32_t error_drop_pkts /* number of total packets drooped due to errors */;
};

/*
 *  $$KEEP_ENDIANNESS$$
 */
struct per_queue_stats
{
	struct tstorm_per_queue_stats tstorm_queue_statistics;
	struct ustorm_per_queue_stats ustorm_queue_statistics;
	struct xstorm_per_queue_stats xstorm_queue_statistics;
};


/*
 * FW version stored in first line of pram $$KEEP_ENDIANNESS$$
 */
struct pram_fw_version
{
	uint8_t major /* firmware current major version */;
	uint8_t minor /* firmware current minor version */;
	uint8_t revision /* firmware current revision version */;
	uint8_t engineering /* firmware current engineering version */;
	uint8_t flags;
#define PRAM_FW_VERSION_OPTIMIZED (0x1<<0) /* BitField flags	if set, this is optimized ASM */
#define PRAM_FW_VERSION_OPTIMIZED_SHIFT 0
#define PRAM_FW_VERSION_STORM_ID (0x3<<1) /* BitField flags	storm_id identification */
#define PRAM_FW_VERSION_STORM_ID_SHIFT 1
#define PRAM_FW_VERSION_BIG_ENDIEN (0x1<<3) /* BitField flags	if set, this is big-endien ASM */
#define PRAM_FW_VERSION_BIG_ENDIEN_SHIFT 3
#define PRAM_FW_VERSION_CHIP_VERSION (0x3<<4) /* BitField flags	0 - E1, 1 - E1H */
#define PRAM_FW_VERSION_CHIP_VERSION_SHIFT 4
#define __PRAM_FW_VERSION_RESERVED0 (0x3<<6) /* BitField flags	 */
#define __PRAM_FW_VERSION_RESERVED0_SHIFT 6
};


/*
 * Ethernet slow path element
 */
union protocol_common_specific_data
{
	uint8_t protocol_data[8] /* to fix this structure size to 8 bytes */;
	struct regpair phy_address /* SPE physical address */;
	struct regpair mac_config_addr /* physical address of the MAC configuration command, as allocated by the driver */;
	struct afex_vif_list_ramrod_data afex_vif_list_data /* The data afex vif list ramrod need */;
};

/*
 * The send queue element
 */
struct protocol_common_spe
{
	struct spe_hdr hdr /* SPE header */;
	union protocol_common_specific_data data /* data specific to common protocol */;
};


/*
 * The data for the Set Timesync Ramrod $$KEEP_ENDIANNESS$$
 */
struct set_timesync_ramrod_data
{
	uint8_t drift_adjust_cmd /* Timesync Drift Adjust Command */;
	uint8_t offset_cmd /* Timesync Offset Command */;
	uint8_t add_sub_drift_adjust_value /* Whether to add(1)/subtract(0) Drift Adjust Value from the Offset */;
	uint8_t drift_adjust_value /* Drift Adjust Value (in ns) */;
	uint32_t drift_adjust_period /* Drift Adjust Period (in us) */;
	struct regpair offset_delta /* Timesync Offset Delta (in ns) */;
};


/*
 * The send queue element
 */
struct slow_path_element
{
	struct spe_hdr hdr /* common data for all protocols */;
	struct regpair protocol_data /* additional data specific to the protocol */;
};


/*
 * Protocol-common statistics counter $$KEEP_ENDIANNESS$$
 */
struct stats_counter
{
	uint16_t xstats_counter /* xstorm statistics counter */;
	uint16_t reserved0;
	uint32_t reserved1;
	uint16_t tstats_counter /* tstorm statistics counter */;
	uint16_t reserved2;
	uint32_t reserved3;
	uint16_t ustats_counter /* ustorm statistics counter */;
	uint16_t reserved4;
	uint32_t reserved5;
	uint16_t cstats_counter /* ustorm statistics counter */;
	uint16_t reserved6;
	uint32_t reserved7;
};


/*
 *  $$KEEP_ENDIANNESS$$
 */
struct stats_query_entry
{
	uint8_t kind;
	uint8_t index /* queue index */;
	uint16_t funcID /* the func the statistic will send to */;
	uint32_t reserved;
	struct regpair address /* pxp address */;
};

/*
 * statistic command $$KEEP_ENDIANNESS$$
 */
struct stats_query_cmd_group
{
	struct stats_query_entry query[STATS_QUERY_CMD_COUNT];
};


/*
 * statistic command header $$KEEP_ENDIANNESS$$
 */
struct stats_query_header
{
	uint8_t cmd_num /* command number */;
	uint8_t reserved0;
	uint16_t drv_stats_counter;
	uint32_t reserved1;
	struct regpair stats_counters_addrs /* stats counter */;
};


/*
 * Types of statistcis query entry
 */
enum stats_query_type
{
	STATS_TYPE_QUEUE,
	STATS_TYPE_PORT,
	STATS_TYPE_PF,
	STATS_TYPE_TOE,
	STATS_TYPE_FCOE,
	MAX_STATS_QUERY_TYPE};


/*
 * Indicate of the function status block state
 */
enum status_block_state
{
	SB_DISABLED,
	SB_ENABLED,
	SB_CLEANED,
	MAX_STATUS_BLOCK_STATE};


/*
 * Storm IDs (including attentions for IGU related enums)
 */
enum storm_id
{
	USTORM_ID,
	CSTORM_ID,
	XSTORM_ID,
	TSTORM_ID,
	ATTENTION_ID,
	MAX_STORM_ID};


/*
 * Taffic types used in ETS and flow control algorithms
 */
enum traffic_type
{
	LLFC_TRAFFIC_TYPE_NW /* Networking */,
	LLFC_TRAFFIC_TYPE_FCOE /* FCoE */,
	LLFC_TRAFFIC_TYPE_ISCSI /* iSCSI */,
	MAX_TRAFFIC_TYPE};


/*
 * zone A per-queue data
 */
struct tstorm_queue_zone_data
{
	struct regpair reserved[4];
};


/*
 * zone B per-VF data
 */
struct tstorm_vf_zone_data
{
	struct regpair reserved;
};


/*
 * Add or Subtract Value for Set Timesync Ramrod
 */
enum ts_add_sub_value
{
	TS_SUB_VALUE /* Subtract Value */,
	TS_ADD_VALUE /* Add Value */,
	MAX_TS_ADD_SUB_VALUE};


/*
 * Drift-Adjust Commands for Set Timesync Ramrod
 */
enum ts_drift_adjust_cmd
{
	TS_DRIFT_ADJUST_KEEP /* Keep Drift-Adjust at current values */,
	TS_DRIFT_ADJUST_SET /* Set Drift-Adjust */,
	TS_DRIFT_ADJUST_RESET /* Reset Drift-Adjust */,
	MAX_TS_DRIFT_ADJUST_CMD};


/*
 * Offset Commands for Set Timesync Ramrod
 */
enum ts_offset_cmd
{
	TS_OFFSET_KEEP /* Keep Offset at current values */,
	TS_OFFSET_INC /* Increase Offset by Offset Delta */,
	TS_OFFSET_DEC /* Decrease Offset by Offset Delta */,
	MAX_TS_OFFSET_CMD};


/*
 * zone A per-queue data
 */
struct ustorm_queue_zone_data
{
	struct ustorm_eth_rx_producers eth_rx_producers /* ETH RX rings producers */;
	struct regpair reserved[3];
};


/*
 * zone B per-VF data
 */
struct ustorm_vf_zone_data
{
	struct regpair reserved;
};


/*
 * data per VF-PF channel
 */
struct vf_pf_channel_data
{
#if defined(__BIG_ENDIAN)
	uint16_t reserved0;
	uint8_t valid /* flag for channel validity. (cleared when identify a VF as malicious) */;
	uint8_t state /* channel state (ready / waiting for ack) */;
#elif defined(__LITTLE_ENDIAN)
	uint8_t state /* channel state (ready / waiting for ack) */;
	uint8_t valid /* flag for channel validity. (cleared when identify a VF as malicious) */;
	uint16_t reserved0;
#endif
	uint32_t reserved1;
};


/*
 * State of VF-PF channel
 */
enum vf_pf_channel_state
{
	VF_PF_CHANNEL_STATE_READY /* Channel is ready to accept a message from VF */,
	VF_PF_CHANNEL_STATE_WAITING_FOR_ACK /* Channel waits for an ACK from PF */,
	MAX_VF_PF_CHANNEL_STATE};


/*
 * vif_list_rule_kind
 */
enum vif_list_rule_kind
{
	VIF_LIST_RULE_SET,
	VIF_LIST_RULE_GET,
	VIF_LIST_RULE_CLEAR_ALL,
	VIF_LIST_RULE_CLEAR_FUNC,
	MAX_VIF_LIST_RULE_KIND};


/*
 * zone A per-queue data
 */
struct xstorm_queue_zone_data
{
	struct regpair reserved[4];
};


/*
 * zone B per-VF data
 */
struct xstorm_vf_zone_data
{
	struct regpair reserved;
};


#endif /* ECORE_HSI_H */

