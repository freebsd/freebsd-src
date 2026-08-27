/* SPDX-License-Identifier: (GPL-2.0-only OR BSD-2-Clause) */
/*
 * Tenstorrent Atlantis PRCM Clock and Reset Indices
 *
 * Copyright (c) 2026 Tenstorrent
 */

#ifndef _DT_BINDINGS_ATLANTIS_PRCM_RCPU_H
#define _DT_BINDINGS_ATLANTIS_PRCM_RCPU_H

/*
 * RCPU Domain Clock IDs
 */
#define CLK_RCPU_PLL		0
#define CLK_RCPU_ROOT		1
#define CLK_RCPU_DIV2		2
#define CLK_RCPU_DIV4		3
#define CLK_RCPU_RTC		4
#define CLK_SMNDMA0_ACLK	5
#define CLK_SMNDMA1_ACLK	6
#define CLK_WDT0_PCLK		7
#define CLK_WDT1_PCLK		8
#define CLK_TIMER_PCLK		9
#define CLK_PVTC_PCLK		10
#define CLK_PMU_PCLK		11
#define CLK_MAILBOX_HCLK	12
#define CLK_SEC_SPACC_HCLK	13
#define CLK_SEC_OTP_HCLK	14
#define CLK_TRNG_PCLK		15
#define CLK_SEC_CRC_HCLK	16
#define CLK_SMN_HCLK		17
#define CLK_AHB0_HCLK		18
#define CLK_SMN_PCLK		19
#define CLK_SMN_CLK		20
#define CLK_SCRATCHPAD_CLK	21
#define CLK_RCPU_CORE_CLK	22
#define CLK_RCPU_ROM_CLK	23
#define CLK_OTP_LOAD_CLK	24
#define CLK_NOC_PLL		25
#define CLK_NOCC_CLK		26
#define CLK_NOCC_DIV2		27
#define CLK_NOCC_DIV4		28
#define CLK_NOCC_RTC		29
#define CLK_NOCC_CAN		30
#define CLK_QSPI_SCLK		31
#define CLK_QSPI_HCLK		32
#define CLK_I2C0_PCLK		33
#define CLK_I2C1_PCLK		34
#define CLK_I2C2_PCLK		35
#define CLK_I2C3_PCLK		36
#define CLK_I2C4_PCLK		37
#define CLK_UART0_PCLK		38
#define CLK_UART1_PCLK		39
#define CLK_UART2_PCLK		40
#define CLK_UART3_PCLK		41
#define CLK_UART4_PCLK		42
#define CLK_SPI0_PCLK		43
#define CLK_SPI1_PCLK		44
#define CLK_SPI2_PCLK		45
#define CLK_SPI3_PCLK		46
#define CLK_GPIO_PCLK		47
#define CLK_CAN0_HCLK		48
#define CLK_CAN0_CLK		49
#define CLK_CAN1_HCLK		50
#define CLK_CAN1_CLK		51
#define CLK_CAN0_TIMER_CLK	52
#define CLK_CAN1_TIMER_CLK	53

/* RCPU domain reset */
#define RST_SMNDMA0		0
#define RST_SMNDMA1		1
#define RST_WDT0		2
#define RST_WDT1		3
#define RST_TMR			4
#define RST_PVTC		5
#define RST_PMU			6
#define RST_MAILBOX		7
#define RST_SPACC		8
#define RST_OTP			9
#define RST_TRNG		10
#define RST_CRC			11
#define RST_QSPI		12
#define RST_I2C0		13
#define RST_I2C1		14
#define RST_I2C2		15
#define RST_I2C3		16
#define RST_I2C4		17
#define RST_UART0		18
#define RST_UART1		19
#define RST_UART2		20
#define RST_UART3		21
#define RST_UART4		22
#define RST_SPI0		23
#define RST_SPI1		24
#define RST_SPI2		25
#define RST_SPI3		26
#define RST_GPIO		27
#define RST_CAN0		28
#define RST_CAN1		29
#define RST_I2S0		30
#define RST_I2S1		31

#endif /* _DT_BINDINGS_ATLANTIS_PRCM_RCPU_H */
