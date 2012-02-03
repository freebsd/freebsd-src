/***********************license start***************
 * Copyright (c) 2003-2010  Cavium Networks (support@cavium.com). All rights
 * reserved.
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *   * Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *
 *   * Redistributions in binary form must reproduce the above
 *     copyright notice, this list of conditions and the following
 *     disclaimer in the documentation and/or other materials provided
 *     with the distribution.

 *   * Neither the name of Cavium Networks nor the names of
 *     its contributors may be used to endorse or promote products
 *     derived from this software without specific prior written
 *     permission.

 * This Software, including technical data, may be subject to U.S. export  control
 * laws, including the U.S. Export Administration Act and its  associated
 * regulations, and may be subject to export or import  regulations in other
 * countries.

 * TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 * AND WITH ALL FAULTS AND CAVIUM  NETWORKS MAKES NO PROMISES, REPRESENTATIONS OR
 * WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH RESPECT TO
 * THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY REPRESENTATION OR
 * DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT DEFECTS, AND CAVIUM
 * SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES OF TITLE,
 * MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR PURPOSE, LACK OF
 * VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET POSSESSION OR
 * CORRESPONDENCE TO DESCRIPTION. THE ENTIRE  RISK ARISING OUT OF USE OR
 * PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 ***********************license end**************************************/


/**
 * cvmx-gpio-defs.h
 *
 * Configuration and status register (CSR) type definitions for
 * Octeon gpio.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 */
#ifndef __CVMX_GPIO_TYPEDEFS_H__
#define __CVMX_GPIO_TYPEDEFS_H__

#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_GPIO_BIT_CFGX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 15))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 15))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 15))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 15))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 15))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 15)))))
		cvmx_warn("CVMX_GPIO_BIT_CFGX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001070000000800ull) + ((offset) & 15) * 8;
}
#else
#define CVMX_GPIO_BIT_CFGX(offset) (CVMX_ADD_IO_SEG(0x0001070000000800ull) + ((offset) & 15) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_GPIO_BOOT_ENA CVMX_GPIO_BOOT_ENA_FUNC()
static inline uint64_t CVMX_GPIO_BOOT_ENA_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)))
		cvmx_warn("CVMX_GPIO_BOOT_ENA not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x00010700000008A8ull);
}
#else
#define CVMX_GPIO_BOOT_ENA (CVMX_ADD_IO_SEG(0x00010700000008A8ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_GPIO_CLK_GENX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 3)))))
		cvmx_warn("CVMX_GPIO_CLK_GENX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x00010700000008C0ull) + ((offset) & 3) * 8;
}
#else
#define CVMX_GPIO_CLK_GENX(offset) (CVMX_ADD_IO_SEG(0x00010700000008C0ull) + ((offset) & 3) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_GPIO_CLK_QLMX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_GPIO_CLK_QLMX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x00010700000008E0ull) + ((offset) & 1) * 8;
}
#else
#define CVMX_GPIO_CLK_QLMX(offset) (CVMX_ADD_IO_SEG(0x00010700000008E0ull) + ((offset) & 1) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_GPIO_DBG_ENA CVMX_GPIO_DBG_ENA_FUNC()
static inline uint64_t CVMX_GPIO_DBG_ENA_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)))
		cvmx_warn("CVMX_GPIO_DBG_ENA not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x00010700000008A0ull);
}
#else
#define CVMX_GPIO_DBG_ENA (CVMX_ADD_IO_SEG(0x00010700000008A0ull))
#endif
#define CVMX_GPIO_INT_CLR (CVMX_ADD_IO_SEG(0x0001070000000898ull))
#define CVMX_GPIO_RX_DAT (CVMX_ADD_IO_SEG(0x0001070000000880ull))
#define CVMX_GPIO_TX_CLR (CVMX_ADD_IO_SEG(0x0001070000000890ull))
#define CVMX_GPIO_TX_SET (CVMX_ADD_IO_SEG(0x0001070000000888ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_GPIO_XBIT_CFGX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset >= 16) && (offset <= 23)))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset >= 16) && (offset <= 23)))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset >= 16) && (offset <= 23))))))
		cvmx_warn("CVMX_GPIO_XBIT_CFGX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001070000000900ull) + ((offset) & 31) * 8 - 8*16;
}
#else
#define CVMX_GPIO_XBIT_CFGX(offset) (CVMX_ADD_IO_SEG(0x0001070000000900ull) + ((offset) & 31) * 8 - 8*16)
#endif

/**
 * cvmx_gpio_bit_cfg#
 */
union cvmx_gpio_bit_cfgx
{
	uint64_t u64;
	struct cvmx_gpio_bit_cfgx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_17_63               : 47;
	uint64_t synce_sel                    : 2;  /**< Selects the QLM clock output
                                                         x0=Normal GPIO output
                                                         01=GPIO QLM clock selected by GPIO_CLK_QLM0
                                                         11=GPIO QLM clock selected by GPIO_CLK_QLM1 */
	uint64_t clk_gen                      : 1;  /**< When TX_OE is set, GPIO pin becomes a clock */
	uint64_t clk_sel                      : 2;  /**< Selects which of the 4 GPIO clock generators */
	uint64_t fil_sel                      : 4;  /**< Global counter bit-select (controls sample rate) */
	uint64_t fil_cnt                      : 4;  /**< Number of consecutive samples to change state */
	uint64_t int_type                     : 1;  /**< Type of interrupt
                                                         0 = level (default)
                                                         1 = rising edge */
	uint64_t int_en                       : 1;  /**< Bit mask to indicate which bits to raise interrupt */
	uint64_t rx_xor                       : 1;  /**< Invert the GPIO pin */
	uint64_t tx_oe                        : 1;  /**< Drive the GPIO pin as an output pin */
#else
	uint64_t tx_oe                        : 1;
	uint64_t rx_xor                       : 1;
	uint64_t int_en                       : 1;
	uint64_t int_type                     : 1;
	uint64_t fil_cnt                      : 4;
	uint64_t fil_sel                      : 4;
	uint64_t clk_sel                      : 2;
	uint64_t clk_gen                      : 1;
	uint64_t synce_sel                    : 2;
	uint64_t reserved_17_63               : 47;
#endif
	} s;
	struct cvmx_gpio_bit_cfgx_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_12_63               : 52;
	uint64_t fil_sel                      : 4;  /**< Global counter bit-select (controls sample rate) */
	uint64_t fil_cnt                      : 4;  /**< Number of consecutive samples to change state */
	uint64_t int_type                     : 1;  /**< Type of interrupt
                                                         0 = level (default)
                                                         1 = rising edge */
	uint64_t int_en                       : 1;  /**< Bit mask to indicate which bits to raise interrupt */
	uint64_t rx_xor                       : 1;  /**< Invert the GPIO pin */
	uint64_t tx_oe                        : 1;  /**< Drive the GPIO pin as an output pin */
#else
	uint64_t tx_oe                        : 1;
	uint64_t rx_xor                       : 1;
	uint64_t int_en                       : 1;
	uint64_t int_type                     : 1;
	uint64_t fil_cnt                      : 4;
	uint64_t fil_sel                      : 4;
	uint64_t reserved_12_63               : 52;
#endif
	} cn30xx;
	struct cvmx_gpio_bit_cfgx_cn30xx      cn31xx;
	struct cvmx_gpio_bit_cfgx_cn30xx      cn38xx;
	struct cvmx_gpio_bit_cfgx_cn30xx      cn38xxp2;
	struct cvmx_gpio_bit_cfgx_cn30xx      cn50xx;
	struct cvmx_gpio_bit_cfgx_cn52xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_15_63               : 49;
	uint64_t clk_gen                      : 1;  /**< When TX_OE is set, GPIO pin becomes a clock */
	uint64_t clk_sel                      : 2;  /**< Selects which of the 4 GPIO clock generators */
	uint64_t fil_sel                      : 4;  /**< Global counter bit-select (controls sample rate) */
	uint64_t fil_cnt                      : 4;  /**< Number of consecutive samples to change state */
	uint64_t int_type                     : 1;  /**< Type of interrupt
                                                         0 = level (default)
                                                         1 = rising edge */
	uint64_t int_en                       : 1;  /**< Bit mask to indicate which bits to raise interrupt */
	uint64_t rx_xor                       : 1;  /**< Invert the GPIO pin */
	uint64_t tx_oe                        : 1;  /**< Drive the GPIO pin as an output pin */
#else
	uint64_t tx_oe                        : 1;
	uint64_t rx_xor                       : 1;
	uint64_t int_en                       : 1;
	uint64_t int_type                     : 1;
	uint64_t fil_cnt                      : 4;
	uint64_t fil_sel                      : 4;
	uint64_t clk_sel                      : 2;
	uint64_t clk_gen                      : 1;
	uint64_t reserved_15_63               : 49;
#endif
	} cn52xx;
	struct cvmx_gpio_bit_cfgx_cn52xx      cn52xxp1;
	struct cvmx_gpio_bit_cfgx_cn52xx      cn56xx;
	struct cvmx_gpio_bit_cfgx_cn52xx      cn56xxp1;
	struct cvmx_gpio_bit_cfgx_cn30xx      cn58xx;
	struct cvmx_gpio_bit_cfgx_cn30xx      cn58xxp1;
	struct cvmx_gpio_bit_cfgx_s           cn63xx;
	struct cvmx_gpio_bit_cfgx_s           cn63xxp1;
};
typedef union cvmx_gpio_bit_cfgx cvmx_gpio_bit_cfgx_t;

/**
 * cvmx_gpio_boot_ena
 */
union cvmx_gpio_boot_ena
{
	uint64_t u64;
	struct cvmx_gpio_boot_ena_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_12_63               : 52;
	uint64_t boot_ena                     : 4;  /**< Drive boot bus chip enables [7:4] on gpio [11:8] */
	uint64_t reserved_0_7                 : 8;
#else
	uint64_t reserved_0_7                 : 8;
	uint64_t boot_ena                     : 4;
	uint64_t reserved_12_63               : 52;
#endif
	} s;
	struct cvmx_gpio_boot_ena_s           cn30xx;
	struct cvmx_gpio_boot_ena_s           cn31xx;
	struct cvmx_gpio_boot_ena_s           cn50xx;
};
typedef union cvmx_gpio_boot_ena cvmx_gpio_boot_ena_t;

/**
 * cvmx_gpio_clk_gen#
 */
union cvmx_gpio_clk_genx
{
	uint64_t u64;
	struct cvmx_gpio_clk_genx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t n                            : 32; /**< Determines the frequency of the GPIO clk generator
                                                         NOTE: Fgpio_clk = Feclk * N / 2^32
                                                               N = (Fgpio_clk / Feclk) * 2^32
                                                         NOTE: writing N == 0 stops the clock generator
                                                         N  should be <= 2^31-1. */
#else
	uint64_t n                            : 32;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_gpio_clk_genx_s           cn52xx;
	struct cvmx_gpio_clk_genx_s           cn52xxp1;
	struct cvmx_gpio_clk_genx_s           cn56xx;
	struct cvmx_gpio_clk_genx_s           cn56xxp1;
	struct cvmx_gpio_clk_genx_s           cn63xx;
	struct cvmx_gpio_clk_genx_s           cn63xxp1;
};
typedef union cvmx_gpio_clk_genx cvmx_gpio_clk_genx_t;

/**
 * cvmx_gpio_clk_qlm#
 *
 * Notes:
 * Clock speed output for different modes ...
 *
 *                        Speed With      Speed with
 * SERDES speed (Gbaud)   DIV=0 (MHz)     DIV=1 (MHz)
 * **********************************************************
 *      1.25                 62.5            31.25
 *      2.5                 125              62.5
 *      3.125               156.25           78.125
 *      5.0                 250             125
 *      6.25                312.5           156.25
 */
union cvmx_gpio_clk_qlmx
{
	uint64_t u64;
	struct cvmx_gpio_clk_qlmx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_3_63                : 61;
	uint64_t div                          : 1;  /**< Internal clock divider
                                                         0=DIV2
                                                         1=DIV4 */
	uint64_t lane_sel                     : 2;  /**< Selects which RX lane clock from QLM2 to use as
                                                         the GPIO internal QLMx clock.  The GPIO block can
                                                         support upto two unique clocks to send out any
                                                         GPIO pin as configured by $GPIO_BIT_CFG[SYNCE_SEL]
                                                         The clock can either be a divided by 2 or divide
                                                         by 4 of the selected RX lane clock. */
#else
	uint64_t lane_sel                     : 2;
	uint64_t div                          : 1;
	uint64_t reserved_3_63                : 61;
#endif
	} s;
	struct cvmx_gpio_clk_qlmx_s           cn63xx;
	struct cvmx_gpio_clk_qlmx_s           cn63xxp1;
};
typedef union cvmx_gpio_clk_qlmx cvmx_gpio_clk_qlmx_t;

/**
 * cvmx_gpio_dbg_ena
 */
union cvmx_gpio_dbg_ena
{
	uint64_t u64;
	struct cvmx_gpio_dbg_ena_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_21_63               : 43;
	uint64_t dbg_ena                      : 21; /**< Enable the debug port to be driven on the gpio */
#else
	uint64_t dbg_ena                      : 21;
	uint64_t reserved_21_63               : 43;
#endif
	} s;
	struct cvmx_gpio_dbg_ena_s            cn30xx;
	struct cvmx_gpio_dbg_ena_s            cn31xx;
	struct cvmx_gpio_dbg_ena_s            cn50xx;
};
typedef union cvmx_gpio_dbg_ena cvmx_gpio_dbg_ena_t;

/**
 * cvmx_gpio_int_clr
 */
union cvmx_gpio_int_clr
{
	uint64_t u64;
	struct cvmx_gpio_int_clr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t type                         : 16; /**< Clear the interrupt rising edge detector */
#else
	uint64_t type                         : 16;
	uint64_t reserved_16_63               : 48;
#endif
	} s;
	struct cvmx_gpio_int_clr_s            cn30xx;
	struct cvmx_gpio_int_clr_s            cn31xx;
	struct cvmx_gpio_int_clr_s            cn38xx;
	struct cvmx_gpio_int_clr_s            cn38xxp2;
	struct cvmx_gpio_int_clr_s            cn50xx;
	struct cvmx_gpio_int_clr_s            cn52xx;
	struct cvmx_gpio_int_clr_s            cn52xxp1;
	struct cvmx_gpio_int_clr_s            cn56xx;
	struct cvmx_gpio_int_clr_s            cn56xxp1;
	struct cvmx_gpio_int_clr_s            cn58xx;
	struct cvmx_gpio_int_clr_s            cn58xxp1;
	struct cvmx_gpio_int_clr_s            cn63xx;
	struct cvmx_gpio_int_clr_s            cn63xxp1;
};
typedef union cvmx_gpio_int_clr cvmx_gpio_int_clr_t;

/**
 * cvmx_gpio_rx_dat
 */
union cvmx_gpio_rx_dat
{
	uint64_t u64;
	struct cvmx_gpio_rx_dat_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t dat                          : 24; /**< GPIO Read Data */
#else
	uint64_t dat                          : 24;
	uint64_t reserved_24_63               : 40;
#endif
	} s;
	struct cvmx_gpio_rx_dat_s             cn30xx;
	struct cvmx_gpio_rx_dat_s             cn31xx;
	struct cvmx_gpio_rx_dat_cn38xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t dat                          : 16; /**< GPIO Read Data */
#else
	uint64_t dat                          : 16;
	uint64_t reserved_16_63               : 48;
#endif
	} cn38xx;
	struct cvmx_gpio_rx_dat_cn38xx        cn38xxp2;
	struct cvmx_gpio_rx_dat_s             cn50xx;
	struct cvmx_gpio_rx_dat_cn38xx        cn52xx;
	struct cvmx_gpio_rx_dat_cn38xx        cn52xxp1;
	struct cvmx_gpio_rx_dat_cn38xx        cn56xx;
	struct cvmx_gpio_rx_dat_cn38xx        cn56xxp1;
	struct cvmx_gpio_rx_dat_cn38xx        cn58xx;
	struct cvmx_gpio_rx_dat_cn38xx        cn58xxp1;
	struct cvmx_gpio_rx_dat_cn38xx        cn63xx;
	struct cvmx_gpio_rx_dat_cn38xx        cn63xxp1;
};
typedef union cvmx_gpio_rx_dat cvmx_gpio_rx_dat_t;

/**
 * cvmx_gpio_tx_clr
 */
union cvmx_gpio_tx_clr
{
	uint64_t u64;
	struct cvmx_gpio_tx_clr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t clr                          : 24; /**< Bit mask to indicate which GPIO_TX_DAT bits to set
                                                         to '0'. When read, CLR returns the GPIO_TX_DAT
                                                         storage. */
#else
	uint64_t clr                          : 24;
	uint64_t reserved_24_63               : 40;
#endif
	} s;
	struct cvmx_gpio_tx_clr_s             cn30xx;
	struct cvmx_gpio_tx_clr_s             cn31xx;
	struct cvmx_gpio_tx_clr_cn38xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t clr                          : 16; /**< Bit mask to indicate which bits to drive to '0'. */
#else
	uint64_t clr                          : 16;
	uint64_t reserved_16_63               : 48;
#endif
	} cn38xx;
	struct cvmx_gpio_tx_clr_cn38xx        cn38xxp2;
	struct cvmx_gpio_tx_clr_s             cn50xx;
	struct cvmx_gpio_tx_clr_cn38xx        cn52xx;
	struct cvmx_gpio_tx_clr_cn38xx        cn52xxp1;
	struct cvmx_gpio_tx_clr_cn38xx        cn56xx;
	struct cvmx_gpio_tx_clr_cn38xx        cn56xxp1;
	struct cvmx_gpio_tx_clr_cn38xx        cn58xx;
	struct cvmx_gpio_tx_clr_cn38xx        cn58xxp1;
	struct cvmx_gpio_tx_clr_cn38xx        cn63xx;
	struct cvmx_gpio_tx_clr_cn38xx        cn63xxp1;
};
typedef union cvmx_gpio_tx_clr cvmx_gpio_tx_clr_t;

/**
 * cvmx_gpio_tx_set
 */
union cvmx_gpio_tx_set
{
	uint64_t u64;
	struct cvmx_gpio_tx_set_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t set                          : 24; /**< Bit mask to indicate which GPIO_TX_DAT bits to set
                                                         to '1'. When read, SET returns the GPIO_TX_DAT
                                                         storage. */
#else
	uint64_t set                          : 24;
	uint64_t reserved_24_63               : 40;
#endif
	} s;
	struct cvmx_gpio_tx_set_s             cn30xx;
	struct cvmx_gpio_tx_set_s             cn31xx;
	struct cvmx_gpio_tx_set_cn38xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t set                          : 16; /**< Bit mask to indicate which bits to drive to '1'. */
#else
	uint64_t set                          : 16;
	uint64_t reserved_16_63               : 48;
#endif
	} cn38xx;
	struct cvmx_gpio_tx_set_cn38xx        cn38xxp2;
	struct cvmx_gpio_tx_set_s             cn50xx;
	struct cvmx_gpio_tx_set_cn38xx        cn52xx;
	struct cvmx_gpio_tx_set_cn38xx        cn52xxp1;
	struct cvmx_gpio_tx_set_cn38xx        cn56xx;
	struct cvmx_gpio_tx_set_cn38xx        cn56xxp1;
	struct cvmx_gpio_tx_set_cn38xx        cn58xx;
	struct cvmx_gpio_tx_set_cn38xx        cn58xxp1;
	struct cvmx_gpio_tx_set_cn38xx        cn63xx;
	struct cvmx_gpio_tx_set_cn38xx        cn63xxp1;
};
typedef union cvmx_gpio_tx_set cvmx_gpio_tx_set_t;

/**
 * cvmx_gpio_xbit_cfg#
 */
union cvmx_gpio_xbit_cfgx
{
	uint64_t u64;
	struct cvmx_gpio_xbit_cfgx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_12_63               : 52;
	uint64_t fil_sel                      : 4;  /**< Global counter bit-select (controls sample rate) */
	uint64_t fil_cnt                      : 4;  /**< Number of consecutive samples to change state */
	uint64_t reserved_2_3                 : 2;
	uint64_t rx_xor                       : 1;  /**< Invert the GPIO pin */
	uint64_t tx_oe                        : 1;  /**< Drive the GPIO pin as an output pin */
#else
	uint64_t tx_oe                        : 1;
	uint64_t rx_xor                       : 1;
	uint64_t reserved_2_3                 : 2;
	uint64_t fil_cnt                      : 4;
	uint64_t fil_sel                      : 4;
	uint64_t reserved_12_63               : 52;
#endif
	} s;
	struct cvmx_gpio_xbit_cfgx_s          cn30xx;
	struct cvmx_gpio_xbit_cfgx_s          cn31xx;
	struct cvmx_gpio_xbit_cfgx_s          cn50xx;
};
typedef union cvmx_gpio_xbit_cfgx cvmx_gpio_xbit_cfgx_t;

#endif
