/*-
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2025 UOS Project Contributors
 *
 * MediaTek MT8395 (Genio 1200) / MT8195 Clock ID definitions.
 * These correspond to the clock IDs used in DTS clock-cells references
 * and match the Linux dt-bindings/clock/mt8195-clk.h numbering.
 *
 * SoC Clock Hierarchy (abbreviated):
 *   XTAL 26MHz
 *     └─ APMIXEDSYS (PLLs: MAINPLL, UNIVPLL, MSDCPLL, MMPLL, TVDPLL, APLL1/2)
 *           └─ TOPCKGEN (muxes + dividers for all peripheral clocks)
 *                 ├─ INFRACFG_AO (infrastructure clock gates)
 *                 ├─ PERICFG_AO (peripheral clock gates)
 *                 └─ [domain clocks: MM, IMG, CAM, IPE, VDE, VEN, MFG, DPE...]
 */

#ifndef __MT8395_CLK_H__
#define __MT8395_CLK_H__

/* ------------------------------------------------------------------ */
/* APMIXEDSYS - PLL outputs                                            */
/* ------------------------------------------------------------------ */
#define CLK_APMIXED_MAINPLL		1
#define CLK_APMIXED_UNIVPLL		2
#define CLK_APMIXED_MSDCPLL		3
#define CLK_APMIXED_MMPLL		4
#define CLK_APMIXED_TVDPLL1		5
#define CLK_APMIXED_TVDPLL2		6
#define CLK_APMIXED_APLL1		7
#define CLK_APMIXED_APLL2		8
#define CLK_APMIXED_MPLL		9
#define CLK_APMIXED_IMGPLL		10
#define CLK_APMIXED_NR_CLK		11

/* ------------------------------------------------------------------ */
/* TOPCKGEN mux clocks                                                 */
/* ------------------------------------------------------------------ */
#define CLK_TOP_AXI_SEL			100
#define CLK_TOP_SPM_SEL			101
#define CLK_TOP_SCP_SEL			102
#define CLK_TOP_BUS_AXIMEM_SEL		103
#define CLK_TOP_VPP0_SEL		104
#define CLK_TOP_VPP1_SEL		105
#define CLK_TOP_VDO0_SEL		106
#define CLK_TOP_VDO1_SEL		107
#define CLK_TOP_DISP_SEL		108
#define CLK_TOP_MDP0_SEL		109
#define CLK_TOP_MDP1_SEL		110
#define CLK_TOP_IMG_SEL			111
#define CLK_TOP_IPE_SEL			112
#define CLK_TOP_CAM_SEL			113
#define CLK_TOP_CCU_SEL			114
#define CLK_TOP_CAMTM_SEL		115
#define CLK_TOP_DSP_SEL			116
#define CLK_TOP_DSP1_SEL		117
#define CLK_TOP_DSP2_SEL		118
#define CLK_TOP_DSP3_SEL		119
#define CLK_TOP_DSP4_SEL		120
#define CLK_TOP_DSP5_SEL		121
#define CLK_TOP_DSP6_SEL		122
#define CLK_TOP_DSP7_SEL		123
#define CLK_TOP_MFG_CORE_TMP_SEL	124
#define CLK_TOP_CAMTG_SEL		125
#define CLK_TOP_CAMTG2_SEL		126
#define CLK_TOP_CAMTG3_SEL		127
#define CLK_TOP_CAMTG4_SEL		128
#define CLK_TOP_CAMTG5_SEL		129
#define CLK_TOP_CAMTG6_SEL		130
#define CLK_TOP_UART_SEL		131	/* UART clock mux */
#define CLK_TOP_SPI_SEL			132	/* SPI clock mux */
#define CLK_TOP_MSDC50_0_HCLK_SEL	133
#define CLK_TOP_MSDC50_0_SEL		134	/* eMMC clock mux */
#define CLK_TOP_MSDC30_1_SEL		135	/* SD card clock mux */
#define CLK_TOP_MSDC30_2_SEL		136
#define CLK_TOP_INTDIR_SEL		137
#define CLK_TOP_AUD_INTBUS_SEL		138
#define CLK_TOP_AUDIO_H_SEL		139
#define CLK_TOP_PWRAP_ULPOSC_SEL	140
#define CLK_TOP_ATB_SEL			141
#define CLK_TOP_SSPM_SEL		142
#define CLK_TOP_DP_SEL			143
#define CLK_TOP_EDP_SEL			144
#define CLK_TOP_DPI_SEL			145
#define CLK_TOP_DISP_PIXEL_SEL		146
#define CLK_TOP_I2C_SEL			147	/* I2C clock mux */
#define CLK_TOP_SENINF_SEL		148
#define CLK_TOP_SENINF1_SEL		149
#define CLK_TOP_SENINF2_SEL		150
#define CLK_TOP_SENINF3_SEL		151
#define CLK_TOP_DXCC_SEL		152
#define CLK_TOP_SSPXTP_SEL		153
#define CLK_TOP_USB_PHY_SEL		154
#define CLK_TOP_USBTOP_P0_SEL		155	/* USB3 clock mux */
#define CLK_TOP_USBTOP_P1_SEL		156
#define CLK_TOP_SSUSB_XHCI_P0_SEL	157
#define CLK_TOP_SSUSB_XHCI_P1_SEL	158
#define CLK_TOP_SSUSB_XHCI_P2_SEL	159
#define CLK_TOP_U3_OCC_250M_SEL		160
#define CLK_TOP_ADSP_SEL		161
#define CLK_TOP_AUDIO_LOCAL_BUS_SEL	162
#define CLK_TOP_AUD_ENGINE_SEL		163
#define CLK_TOP_AUD_24M_SEL		164
#define CLK_TOP_AUD_48K_TIMING_SEL	165
#define CLK_TOP_VCORE_INFRA_SEL		166
#define CLK_TOP_PWRMCU_SEL		167
#define CLK_TOP_PCIE_PHY_SEL		168
#define CLK_TOP_PEXTP_P0_SEL		169	/* PCIe clock mux */
#define CLK_TOP_PEXTP_P1_SEL		170
#define CLK_TOP_PEXTP_TL_P0_SEL	171
#define CLK_TOP_PEXTP_TL_P1_SEL	172
#define CLK_TOP_PEXTP_TL_P2_SEL	173
#define CLK_TOP_PEXTP_TL_P3_SEL	174
#define CLK_TOP_TL_SEL			175
#define CLK_TOP_EMI_INTERFACE_546_SEL	176
#define CLK_TOP_SDF_SEL			177
#define CLK_TOP_UARTHUB_BCLK_SEL	178
#define CLK_TOP_GCC_MEM_SEL		179
#define CLK_TOP_GCC_LP_SEL		180
#define CLK_TOP_MSDC_MACRO_1P_SEL	181
#define CLK_TOP_MSDC_MACRO_2P_SEL	182
#define CLK_TOP_MSDC50_0_BUS_HCLK_SEL	183
#define CLK_TOP_MSDC50_2_BUS_HCLK_SEL	184
#define CLK_TOP_ETHERNET_SEL		185	/* Ethernet clock mux */
#define CLK_TOP_ETH_GMII_SEL		186
#define CLK_TOP_NR_CLK			187

/* ------------------------------------------------------------------ */
/* INFRACFG_AO - Infrastructure clock gates                           */
/* ------------------------------------------------------------------ */
#define CLK_INFRA_PMIC_TMR		200
#define CLK_INFRA_PMIC_AP		201
#define CLK_INFRA_PMIC_MD		202
#define CLK_INFRA_PMIC_CONN		203
#define CLK_INFRA_SEJ			204
#define CLK_INFRA_APXGPT		205
#define CLK_INFRA_GCE			206
#define CLK_INFRA_GCE2			207
#define CLK_INFRA_THERM			208
#define CLK_INFRA_I2C0			209	/* I2C0 gate */
#define CLK_INFRA_I2C1			210
#define CLK_INFRA_I2C2			211
#define CLK_INFRA_I2C3			212
#define CLK_INFRA_I2C4			213
#define CLK_INFRA_I2C5			214
#define CLK_INFRA_I2C6			215
#define CLK_INFRA_I2C7			216
#define CLK_INFRA_I2C8			217
#define CLK_INFRA_PWM_HCLK		218
#define CLK_INFRA_PWM1			219
#define CLK_INFRA_PWM2			220
#define CLK_INFRA_PWM3			221
#define CLK_INFRA_PWM4			222
#define CLK_INFRA_PWM5			223
#define CLK_INFRA_PWM			224
#define CLK_INFRA_UART0			225	/* UART0 gate */
#define CLK_INFRA_UART1			226
#define CLK_INFRA_UART2			227
#define CLK_INFRA_UART3			228
#define CLK_INFRA_UART4			229
#define CLK_INFRA_GCE_26M		230
#define CLK_INFRA_CQ_DMA_FPC		231
#define CLK_INFRA_BTIF			232
#define CLK_INFRA_SPI0			233	/* SPI0 gate */
#define CLK_INFRA_SPI1			234
#define CLK_INFRA_SPI2			235
#define CLK_INFRA_SPI3			236
#define CLK_INFRA_SPI4			237
#define CLK_INFRA_SPI5			238
#define CLK_INFRA_SPI6			239
#define CLK_INFRA_SPI7			240
#define CLK_INFRA_MSDC_0P		241
#define CLK_INFRA_MSDC_1P		242
#define CLK_INFRA_MSDC_2P		243
#define CLK_INFRA_MSDC0_CK		244	/* eMMC/MSDC0 clock gate */
#define CLK_INFRA_MSDC1_CK		245	/* SD/MSDC1 clock gate */
#define CLK_INFRA_MSDC2_CK		246
#define CLK_INFRA_MSDCFDE_CK		247
#define CLK_INFRA_TRNG			248
#define CLK_INFRA_AUXADC		249
#define CLK_INFRA_CPUM			250
#define CLK_INFRA_CCIF1_AP		251
#define CLK_INFRA_CCIF1_MD		252
#define CLK_INFRA_AUXADC_MD		253
#define CLK_INFRA_AP_DMA		254
#define CLK_INFRA_DEBUGSYS		255
#define CLK_INFRA_AUDIO			256
#define CLK_INFRA_CCIF_AP		257
#define CLK_INFRA_DXCC_SEC_CORE		258
#define CLK_INFRA_DXCC_AO		259
#define CLK_INFRA_DEVMPU_BCLK		260
#define CLK_INFRA_DRAMC_F26M		261
#define CLK_INFRA_IRTX			262
#define CLK_INFRA_CCIF_MD		263
#define CLK_INFRA_SSPM			264
#define CLK_INFRA_SSPM_BUS_HCLK	265
#define CLK_INFRA_APDMA			266
#define CLK_INFRA_DISP_PWM		267
#define CLK_INFRA_CLDMA_BCLK		268
#define CLK_INFRA_SPI8			269
#define CLK_INFRA_SPI9			270
#define CLK_INFRA_USB_CK		271	/* USB clock gate */
#define CLK_INFRA_USB_CK_P1		272
#define CLK_INFRA_NR_CLK		273

/* ------------------------------------------------------------------ */
/* PERICFG_AO - Peripheral clock gates                                 */
/* ------------------------------------------------------------------ */
#define CLK_PERAOP_UART0		300	/* UART0 peri gate */
#define CLK_PERAOP_UART1		301
#define CLK_PERAOP_UART2		302
#define CLK_PERAOP_UART3		303
#define CLK_PERAOP_PWM_HCLK		304
#define CLK_PERAOP_PWM_BCLK		305
#define CLK_PERAOP_PWM_FBCLK1		306
#define CLK_PERAOP_PWM_FBCLK2		307
#define CLK_PERAOP_PWM_FBCLK3		308
#define CLK_PERAOP_PWM_FBCLK4		309
#define CLK_PERAOP_SPI0_BCLK		310
#define CLK_PERAOP_SPI1_BCLK		311
#define CLK_PERAOP_SPI2_BCLK		312
#define CLK_PERAOP_SPI3_BCLK		313
#define CLK_PERAOP_SPI4_BCLK		314
#define CLK_PERAOP_SPI5_BCLK		315
#define CLK_PERAOP_SPI6_BCLK		316
#define CLK_PERAOP_SPI7_BCLK		317
#define CLK_PERAOP_DMA_BCLK		318
#define CLK_PERAOP_SSUSB0_FRMCNT	319	/* USB3 frame count */
#define CLK_PERAOP_MSDC1		320	/* SD/MSDC1 peri gate */
#define CLK_PERAOP_MSDC1_HCLK		321
#define CLK_PERAOP_MSDC2		322
#define CLK_PERAOP_MSDC2_HCLK		323
#define CLK_PERAOP_ETHERNET		324	/* Ethernet peri gate */
#define CLK_PERAOP_ETHERNET_BUS		325
#define CLK_PERAOP_NR_CLK		326

#endif /* __MT8395_CLK_H__ */
