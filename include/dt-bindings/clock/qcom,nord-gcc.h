/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
 */

#ifndef _DT_BINDINGS_CLK_QCOM_GCC_NORD_H
#define _DT_BINDINGS_CLK_QCOM_GCC_NORD_H

/* GCC clocks */
#define GCC_BOOT_ROM_AHB_CLK					0
#define GCC_GP1_CLK						1
#define GCC_GP1_CLK_SRC						2
#define GCC_GP2_CLK						3
#define GCC_GP2_CLK_SRC						4
#define GCC_GPLL0						5
#define GCC_GPLL0_OUT_EVEN					6
#define GCC_MMU_0_TCU_VOTE_CLK					7
#define GCC_PCIE_A_AUX_CLK					8
#define GCC_PCIE_A_AUX_CLK_SRC					9
#define GCC_PCIE_A_CFG_AHB_CLK					10
#define GCC_PCIE_A_DTI_QTC_CLK					11
#define GCC_PCIE_A_MSTR_AXI_CLK					12
#define GCC_PCIE_A_PHY_AUX_CLK					13
#define GCC_PCIE_A_PHY_AUX_CLK_SRC				14
#define GCC_PCIE_A_PHY_RCHNG_CLK				15
#define GCC_PCIE_A_PHY_RCHNG_CLK_SRC				16
#define GCC_PCIE_A_PIPE_CLK					17
#define GCC_PCIE_A_PIPE_CLK_SRC					18
#define GCC_PCIE_A_SLV_AXI_CLK					19
#define GCC_PCIE_A_SLV_Q2A_AXI_CLK				20
#define GCC_PCIE_B_AUX_CLK					21
#define GCC_PCIE_B_AUX_CLK_SRC					22
#define GCC_PCIE_B_CFG_AHB_CLK					23
#define GCC_PCIE_B_DTI_QTC_CLK					24
#define GCC_PCIE_B_MSTR_AXI_CLK					25
#define GCC_PCIE_B_PHY_AUX_CLK					26
#define GCC_PCIE_B_PHY_AUX_CLK_SRC				27
#define GCC_PCIE_B_PHY_RCHNG_CLK				28
#define GCC_PCIE_B_PHY_RCHNG_CLK_SRC				29
#define GCC_PCIE_B_PIPE_CLK					30
#define GCC_PCIE_B_PIPE_CLK_SRC					31
#define GCC_PCIE_B_SLV_AXI_CLK					32
#define GCC_PCIE_B_SLV_Q2A_AXI_CLK				33
#define GCC_PCIE_C_AUX_CLK					34
#define GCC_PCIE_C_AUX_CLK_SRC					35
#define GCC_PCIE_C_CFG_AHB_CLK					36
#define GCC_PCIE_C_DTI_QTC_CLK					37
#define GCC_PCIE_C_MSTR_AXI_CLK					38
#define GCC_PCIE_C_PHY_AUX_CLK					39
#define GCC_PCIE_C_PHY_AUX_CLK_SRC				40
#define GCC_PCIE_C_PHY_RCHNG_CLK				41
#define GCC_PCIE_C_PHY_RCHNG_CLK_SRC				42
#define GCC_PCIE_C_PIPE_CLK					43
#define GCC_PCIE_C_PIPE_CLK_SRC					44
#define GCC_PCIE_C_SLV_AXI_CLK					45
#define GCC_PCIE_C_SLV_Q2A_AXI_CLK				46
#define GCC_PCIE_D_AUX_CLK					47
#define GCC_PCIE_D_AUX_CLK_SRC					48
#define GCC_PCIE_D_CFG_AHB_CLK					49
#define GCC_PCIE_D_DTI_QTC_CLK					50
#define GCC_PCIE_D_MSTR_AXI_CLK					51
#define GCC_PCIE_D_PHY_AUX_CLK					52
#define GCC_PCIE_D_PHY_AUX_CLK_SRC				53
#define GCC_PCIE_D_PHY_RCHNG_CLK				54
#define GCC_PCIE_D_PHY_RCHNG_CLK_SRC				55
#define GCC_PCIE_D_PIPE_CLK					56
#define GCC_PCIE_D_PIPE_CLK_SRC					57
#define GCC_PCIE_D_SLV_AXI_CLK					58
#define GCC_PCIE_D_SLV_Q2A_AXI_CLK				59
#define GCC_PCIE_LINK_AHB_CLK					60
#define GCC_PCIE_LINK_XO_CLK					61
#define GCC_PCIE_NOC_ASYNC_BRIDGE_CLK				62
#define GCC_PCIE_NOC_CNOC_SF_QX_CLK				63
#define GCC_PCIE_NOC_M_CFG_CLK					64
#define GCC_PCIE_NOC_M_PDB_CLK					65
#define GCC_PCIE_NOC_MSTR_AXI_CLK				66
#define GCC_PCIE_NOC_PWRCTL_CLK					67
#define GCC_PCIE_NOC_QOSGEN_EXTREF_CLK				68
#define GCC_PCIE_NOC_REFGEN_CLK					69
#define GCC_PCIE_NOC_REFGEN_CLK_SRC				70
#define GCC_PCIE_NOC_S_CFG_CLK					71
#define GCC_PCIE_NOC_S_PDB_CLK					72
#define GCC_PCIE_NOC_SAFETY_CLK					73
#define GCC_PCIE_NOC_SAFETY_CLK_SRC				74
#define GCC_PCIE_NOC_SLAVE_AXI_CLK				75
#define GCC_PCIE_NOC_TSCTR_CLK					76
#define GCC_PCIE_NOC_XO_CLK					77
#define GCC_PDM2_CLK						78
#define GCC_PDM2_CLK_SRC					79
#define GCC_PDM_AHB_CLK						80
#define GCC_PDM_XO4_CLK						81
#define GCC_QUPV3_WRAP3_CORE_2X_CLK				82
#define GCC_QUPV3_WRAP3_CORE_CLK				83
#define GCC_QUPV3_WRAP3_M_CLK					84
#define GCC_QUPV3_WRAP3_QSPI_REF_CLK				85
#define GCC_QUPV3_WRAP3_QSPI_REF_CLK_SRC			86
#define GCC_QUPV3_WRAP3_S0_CLK					87
#define GCC_QUPV3_WRAP3_S0_CLK_SRC				88
#define GCC_QUPV3_WRAP3_S_AHB_CLK				89
#define GCC_SMMU_PCIE_QTC_VOTE_CLK				90

/* GCC power domains */
#define GCC_PCIE_A_GDSC						0
#define GCC_PCIE_A_PHY_GDSC					1
#define GCC_PCIE_B_GDSC						2
#define GCC_PCIE_B_PHY_GDSC					3
#define GCC_PCIE_C_GDSC						4
#define GCC_PCIE_C_PHY_GDSC					5
#define GCC_PCIE_D_GDSC						6
#define GCC_PCIE_D_PHY_GDSC					7
#define GCC_PCIE_NOC_GDSC					8

/* GCC resets */
#define GCC_PCIE_A_BCR						0
#define GCC_PCIE_A_LINK_DOWN_BCR				1
#define GCC_PCIE_A_NOCSR_COM_PHY_BCR				2
#define GCC_PCIE_A_PHY_BCR					3
#define GCC_PCIE_A_PHY_CFG_AHB_BCR				4
#define GCC_PCIE_A_PHY_COM_BCR					5
#define GCC_PCIE_A_PHY_NOCSR_COM_PHY_BCR			6
#define GCC_PCIE_B_BCR						7
#define GCC_PCIE_B_LINK_DOWN_BCR				8
#define GCC_PCIE_B_NOCSR_COM_PHY_BCR				9
#define GCC_PCIE_B_PHY_BCR					10
#define GCC_PCIE_B_PHY_CFG_AHB_BCR				11
#define GCC_PCIE_B_PHY_COM_BCR					12
#define GCC_PCIE_B_PHY_NOCSR_COM_PHY_BCR			13
#define GCC_PCIE_C_BCR						14
#define GCC_PCIE_C_LINK_DOWN_BCR				15
#define GCC_PCIE_C_NOCSR_COM_PHY_BCR				16
#define GCC_PCIE_C_PHY_BCR					17
#define GCC_PCIE_C_PHY_CFG_AHB_BCR				18
#define GCC_PCIE_C_PHY_COM_BCR					19
#define GCC_PCIE_C_PHY_NOCSR_COM_PHY_BCR			20
#define GCC_PCIE_D_BCR						21
#define GCC_PCIE_D_LINK_DOWN_BCR				22
#define GCC_PCIE_D_NOCSR_COM_PHY_BCR				23
#define GCC_PCIE_D_PHY_BCR					24
#define GCC_PCIE_D_PHY_CFG_AHB_BCR				25
#define GCC_PCIE_D_PHY_COM_BCR					26
#define GCC_PCIE_D_PHY_NOCSR_COM_PHY_BCR			27
#define GCC_PCIE_NOC_BCR					28
#define GCC_PDM_BCR						29
#define GCC_QUPV3_WRAPPER_3_BCR					30
#define GCC_TCSR_PCIE_BCR					31

#endif
