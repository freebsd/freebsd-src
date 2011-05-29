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
 * cvmx-mio-defs.h
 *
 * Configuration and status register (CSR) type definitions for
 * Octeon mio.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision$<hr>
 *
 */
#ifndef __CVMX_MIO_TYPEDEFS_H__
#define __CVMX_MIO_TYPEDEFS_H__

#define CVMX_MIO_BOOT_BIST_STAT (CVMX_ADD_IO_SEG(0x00011800000000F8ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_BOOT_COMP CVMX_MIO_BOOT_COMP_FUNC()
static inline uint64_t CVMX_MIO_BOOT_COMP_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_BOOT_COMP not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x00011800000000B8ull);
}
#else
#define CVMX_MIO_BOOT_COMP (CVMX_ADD_IO_SEG(0x00011800000000B8ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_BOOT_DMA_CFGX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 2))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_BOOT_DMA_CFGX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000100ull) + ((offset) & 3) * 8;
}
#else
#define CVMX_MIO_BOOT_DMA_CFGX(offset) (CVMX_ADD_IO_SEG(0x0001180000000100ull) + ((offset) & 3) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_BOOT_DMA_INTX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 2))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_BOOT_DMA_INTX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000138ull) + ((offset) & 3) * 8;
}
#else
#define CVMX_MIO_BOOT_DMA_INTX(offset) (CVMX_ADD_IO_SEG(0x0001180000000138ull) + ((offset) & 3) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_BOOT_DMA_INT_ENX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 2))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_BOOT_DMA_INT_ENX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000150ull) + ((offset) & 3) * 8;
}
#else
#define CVMX_MIO_BOOT_DMA_INT_ENX(offset) (CVMX_ADD_IO_SEG(0x0001180000000150ull) + ((offset) & 3) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_BOOT_DMA_TIMX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 2))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_BOOT_DMA_TIMX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000120ull) + ((offset) & 3) * 8;
}
#else
#define CVMX_MIO_BOOT_DMA_TIMX(offset) (CVMX_ADD_IO_SEG(0x0001180000000120ull) + ((offset) & 3) * 8)
#endif
#define CVMX_MIO_BOOT_ERR (CVMX_ADD_IO_SEG(0x00011800000000A0ull))
#define CVMX_MIO_BOOT_INT (CVMX_ADD_IO_SEG(0x00011800000000A8ull))
#define CVMX_MIO_BOOT_LOC_ADR (CVMX_ADD_IO_SEG(0x0001180000000090ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_BOOT_LOC_CFGX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_BOOT_LOC_CFGX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000080ull) + ((offset) & 1) * 8;
}
#else
#define CVMX_MIO_BOOT_LOC_CFGX(offset) (CVMX_ADD_IO_SEG(0x0001180000000080ull) + ((offset) & 1) * 8)
#endif
#define CVMX_MIO_BOOT_LOC_DAT (CVMX_ADD_IO_SEG(0x0001180000000098ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_BOOT_PIN_DEFS CVMX_MIO_BOOT_PIN_DEFS_FUNC()
static inline uint64_t CVMX_MIO_BOOT_PIN_DEFS_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_BOOT_PIN_DEFS not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x00011800000000C0ull);
}
#else
#define CVMX_MIO_BOOT_PIN_DEFS (CVMX_ADD_IO_SEG(0x00011800000000C0ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_BOOT_REG_CFGX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 7)))))
		cvmx_warn("CVMX_MIO_BOOT_REG_CFGX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000000ull) + ((offset) & 7) * 8;
}
#else
#define CVMX_MIO_BOOT_REG_CFGX(offset) (CVMX_ADD_IO_SEG(0x0001180000000000ull) + ((offset) & 7) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_BOOT_REG_TIMX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 7)))))
		cvmx_warn("CVMX_MIO_BOOT_REG_TIMX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000040ull) + ((offset) & 7) * 8;
}
#else
#define CVMX_MIO_BOOT_REG_TIMX(offset) (CVMX_ADD_IO_SEG(0x0001180000000040ull) + ((offset) & 7) * 8)
#endif
#define CVMX_MIO_BOOT_THR (CVMX_ADD_IO_SEG(0x00011800000000B0ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_FUS_BNK_DATX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_FUS_BNK_DATX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000001520ull) + ((offset) & 3) * 8;
}
#else
#define CVMX_MIO_FUS_BNK_DATX(offset) (CVMX_ADD_IO_SEG(0x0001180000001520ull) + ((offset) & 3) * 8)
#endif
#define CVMX_MIO_FUS_DAT0 (CVMX_ADD_IO_SEG(0x0001180000001400ull))
#define CVMX_MIO_FUS_DAT1 (CVMX_ADD_IO_SEG(0x0001180000001408ull))
#define CVMX_MIO_FUS_DAT2 (CVMX_ADD_IO_SEG(0x0001180000001410ull))
#define CVMX_MIO_FUS_DAT3 (CVMX_ADD_IO_SEG(0x0001180000001418ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_EMA CVMX_MIO_FUS_EMA_FUNC()
static inline uint64_t CVMX_MIO_FUS_EMA_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_FUS_EMA not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001550ull);
}
#else
#define CVMX_MIO_FUS_EMA (CVMX_ADD_IO_SEG(0x0001180000001550ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_PDF CVMX_MIO_FUS_PDF_FUNC()
static inline uint64_t CVMX_MIO_FUS_PDF_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_FUS_PDF not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001420ull);
}
#else
#define CVMX_MIO_FUS_PDF (CVMX_ADD_IO_SEG(0x0001180000001420ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_PLL CVMX_MIO_FUS_PLL_FUNC()
static inline uint64_t CVMX_MIO_FUS_PLL_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_FUS_PLL not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001580ull);
}
#else
#define CVMX_MIO_FUS_PLL (CVMX_ADD_IO_SEG(0x0001180000001580ull))
#endif
#define CVMX_MIO_FUS_PROG (CVMX_ADD_IO_SEG(0x0001180000001510ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_PROG_TIMES CVMX_MIO_FUS_PROG_TIMES_FUNC()
static inline uint64_t CVMX_MIO_FUS_PROG_TIMES_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN5XXX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_FUS_PROG_TIMES not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001518ull);
}
#else
#define CVMX_MIO_FUS_PROG_TIMES (CVMX_ADD_IO_SEG(0x0001180000001518ull))
#endif
#define CVMX_MIO_FUS_RCMD (CVMX_ADD_IO_SEG(0x0001180000001500ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_READ_TIMES CVMX_MIO_FUS_READ_TIMES_FUNC()
static inline uint64_t CVMX_MIO_FUS_READ_TIMES_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_FUS_READ_TIMES not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001570ull);
}
#else
#define CVMX_MIO_FUS_READ_TIMES (CVMX_ADD_IO_SEG(0x0001180000001570ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_REPAIR_RES0 CVMX_MIO_FUS_REPAIR_RES0_FUNC()
static inline uint64_t CVMX_MIO_FUS_REPAIR_RES0_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_FUS_REPAIR_RES0 not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001558ull);
}
#else
#define CVMX_MIO_FUS_REPAIR_RES0 (CVMX_ADD_IO_SEG(0x0001180000001558ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_REPAIR_RES1 CVMX_MIO_FUS_REPAIR_RES1_FUNC()
static inline uint64_t CVMX_MIO_FUS_REPAIR_RES1_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_FUS_REPAIR_RES1 not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001560ull);
}
#else
#define CVMX_MIO_FUS_REPAIR_RES1 (CVMX_ADD_IO_SEG(0x0001180000001560ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_REPAIR_RES2 CVMX_MIO_FUS_REPAIR_RES2_FUNC()
static inline uint64_t CVMX_MIO_FUS_REPAIR_RES2_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_FUS_REPAIR_RES2 not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001568ull);
}
#else
#define CVMX_MIO_FUS_REPAIR_RES2 (CVMX_ADD_IO_SEG(0x0001180000001568ull))
#endif
#define CVMX_MIO_FUS_SPR_REPAIR_RES (CVMX_ADD_IO_SEG(0x0001180000001548ull))
#define CVMX_MIO_FUS_SPR_REPAIR_SUM (CVMX_ADD_IO_SEG(0x0001180000001540ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_FUS_UNLOCK CVMX_MIO_FUS_UNLOCK_FUNC()
static inline uint64_t CVMX_MIO_FUS_UNLOCK_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX)))
		cvmx_warn("CVMX_MIO_FUS_UNLOCK not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001578ull);
}
#else
#define CVMX_MIO_FUS_UNLOCK (CVMX_ADD_IO_SEG(0x0001180000001578ull))
#endif
#define CVMX_MIO_FUS_WADR (CVMX_ADD_IO_SEG(0x0001180000001508ull))
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_GPIO_COMP CVMX_MIO_GPIO_COMP_FUNC()
static inline uint64_t CVMX_MIO_GPIO_COMP_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_GPIO_COMP not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x00011800000000C8ull);
}
#else
#define CVMX_MIO_GPIO_COMP (CVMX_ADD_IO_SEG(0x00011800000000C8ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_NDF_DMA_CFG CVMX_MIO_NDF_DMA_CFG_FUNC()
static inline uint64_t CVMX_MIO_NDF_DMA_CFG_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_NDF_DMA_CFG not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000168ull);
}
#else
#define CVMX_MIO_NDF_DMA_CFG (CVMX_ADD_IO_SEG(0x0001180000000168ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_NDF_DMA_INT CVMX_MIO_NDF_DMA_INT_FUNC()
static inline uint64_t CVMX_MIO_NDF_DMA_INT_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_NDF_DMA_INT not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000170ull);
}
#else
#define CVMX_MIO_NDF_DMA_INT (CVMX_ADD_IO_SEG(0x0001180000000170ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_NDF_DMA_INT_EN CVMX_MIO_NDF_DMA_INT_EN_FUNC()
static inline uint64_t CVMX_MIO_NDF_DMA_INT_EN_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_NDF_DMA_INT_EN not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000178ull);
}
#else
#define CVMX_MIO_NDF_DMA_INT_EN (CVMX_ADD_IO_SEG(0x0001180000000178ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_PLL_CTL CVMX_MIO_PLL_CTL_FUNC()
static inline uint64_t CVMX_MIO_PLL_CTL_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX)))
		cvmx_warn("CVMX_MIO_PLL_CTL not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001448ull);
}
#else
#define CVMX_MIO_PLL_CTL (CVMX_ADD_IO_SEG(0x0001180000001448ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_PLL_SETTING CVMX_MIO_PLL_SETTING_FUNC()
static inline uint64_t CVMX_MIO_PLL_SETTING_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX)))
		cvmx_warn("CVMX_MIO_PLL_SETTING not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001440ull);
}
#else
#define CVMX_MIO_PLL_SETTING (CVMX_ADD_IO_SEG(0x0001180000001440ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_PTP_CLOCK_CFG CVMX_MIO_PTP_CLOCK_CFG_FUNC()
static inline uint64_t CVMX_MIO_PTP_CLOCK_CFG_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_PTP_CLOCK_CFG not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001070000000F00ull);
}
#else
#define CVMX_MIO_PTP_CLOCK_CFG (CVMX_ADD_IO_SEG(0x0001070000000F00ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_PTP_CLOCK_COMP CVMX_MIO_PTP_CLOCK_COMP_FUNC()
static inline uint64_t CVMX_MIO_PTP_CLOCK_COMP_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_PTP_CLOCK_COMP not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001070000000F18ull);
}
#else
#define CVMX_MIO_PTP_CLOCK_COMP (CVMX_ADD_IO_SEG(0x0001070000000F18ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_PTP_CLOCK_HI CVMX_MIO_PTP_CLOCK_HI_FUNC()
static inline uint64_t CVMX_MIO_PTP_CLOCK_HI_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_PTP_CLOCK_HI not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001070000000F10ull);
}
#else
#define CVMX_MIO_PTP_CLOCK_HI (CVMX_ADD_IO_SEG(0x0001070000000F10ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_PTP_CLOCK_LO CVMX_MIO_PTP_CLOCK_LO_FUNC()
static inline uint64_t CVMX_MIO_PTP_CLOCK_LO_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_PTP_CLOCK_LO not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001070000000F08ull);
}
#else
#define CVMX_MIO_PTP_CLOCK_LO (CVMX_ADD_IO_SEG(0x0001070000000F08ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_PTP_EVT_CNT CVMX_MIO_PTP_EVT_CNT_FUNC()
static inline uint64_t CVMX_MIO_PTP_EVT_CNT_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_PTP_EVT_CNT not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001070000000F28ull);
}
#else
#define CVMX_MIO_PTP_EVT_CNT (CVMX_ADD_IO_SEG(0x0001070000000F28ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_PTP_TIMESTAMP CVMX_MIO_PTP_TIMESTAMP_FUNC()
static inline uint64_t CVMX_MIO_PTP_TIMESTAMP_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_PTP_TIMESTAMP not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001070000000F20ull);
}
#else
#define CVMX_MIO_PTP_TIMESTAMP (CVMX_ADD_IO_SEG(0x0001070000000F20ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_RST_BOOT CVMX_MIO_RST_BOOT_FUNC()
static inline uint64_t CVMX_MIO_RST_BOOT_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_RST_BOOT not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001600ull);
}
#else
#define CVMX_MIO_RST_BOOT (CVMX_ADD_IO_SEG(0x0001180000001600ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_RST_CFG CVMX_MIO_RST_CFG_FUNC()
static inline uint64_t CVMX_MIO_RST_CFG_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_RST_CFG not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001610ull);
}
#else
#define CVMX_MIO_RST_CFG (CVMX_ADD_IO_SEG(0x0001180000001610ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_RST_CTLX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_RST_CTLX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000001618ull) + ((offset) & 1) * 8;
}
#else
#define CVMX_MIO_RST_CTLX(offset) (CVMX_ADD_IO_SEG(0x0001180000001618ull) + ((offset) & 1) * 8)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_RST_DELAY CVMX_MIO_RST_DELAY_FUNC()
static inline uint64_t CVMX_MIO_RST_DELAY_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_RST_DELAY not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001608ull);
}
#else
#define CVMX_MIO_RST_DELAY (CVMX_ADD_IO_SEG(0x0001180000001608ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_RST_INT CVMX_MIO_RST_INT_FUNC()
static inline uint64_t CVMX_MIO_RST_INT_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_RST_INT not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001628ull);
}
#else
#define CVMX_MIO_RST_INT (CVMX_ADD_IO_SEG(0x0001180000001628ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_RST_INT_EN CVMX_MIO_RST_INT_EN_FUNC()
static inline uint64_t CVMX_MIO_RST_INT_EN_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN63XX)))
		cvmx_warn("CVMX_MIO_RST_INT_EN not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000001630ull);
}
#else
#define CVMX_MIO_RST_INT_EN (CVMX_ADD_IO_SEG(0x0001180000001630ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_TWSX_INT(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_TWSX_INT(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000001010ull) + ((offset) & 1) * 512;
}
#else
#define CVMX_MIO_TWSX_INT(offset) (CVMX_ADD_IO_SEG(0x0001180000001010ull) + ((offset) & 1) * 512)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_TWSX_SW_TWSI(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_TWSX_SW_TWSI(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000001000ull) + ((offset) & 1) * 512;
}
#else
#define CVMX_MIO_TWSX_SW_TWSI(offset) (CVMX_ADD_IO_SEG(0x0001180000001000ull) + ((offset) & 1) * 512)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_TWSX_SW_TWSI_EXT(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_TWSX_SW_TWSI_EXT(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000001018ull) + ((offset) & 1) * 512;
}
#else
#define CVMX_MIO_TWSX_SW_TWSI_EXT(offset) (CVMX_ADD_IO_SEG(0x0001180000001018ull) + ((offset) & 1) * 512)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_TWSX_TWSI_SW(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_TWSX_TWSI_SW(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000001008ull) + ((offset) & 1) * 512;
}
#else
#define CVMX_MIO_TWSX_TWSI_SW(offset) (CVMX_ADD_IO_SEG(0x0001180000001008ull) + ((offset) & 1) * 512)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_DLH CVMX_MIO_UART2_DLH_FUNC()
static inline uint64_t CVMX_MIO_UART2_DLH_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_DLH not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000488ull);
}
#else
#define CVMX_MIO_UART2_DLH (CVMX_ADD_IO_SEG(0x0001180000000488ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_DLL CVMX_MIO_UART2_DLL_FUNC()
static inline uint64_t CVMX_MIO_UART2_DLL_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_DLL not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000480ull);
}
#else
#define CVMX_MIO_UART2_DLL (CVMX_ADD_IO_SEG(0x0001180000000480ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_FAR CVMX_MIO_UART2_FAR_FUNC()
static inline uint64_t CVMX_MIO_UART2_FAR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_FAR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000520ull);
}
#else
#define CVMX_MIO_UART2_FAR (CVMX_ADD_IO_SEG(0x0001180000000520ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_FCR CVMX_MIO_UART2_FCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_FCR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_FCR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000450ull);
}
#else
#define CVMX_MIO_UART2_FCR (CVMX_ADD_IO_SEG(0x0001180000000450ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_HTX CVMX_MIO_UART2_HTX_FUNC()
static inline uint64_t CVMX_MIO_UART2_HTX_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_HTX not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000708ull);
}
#else
#define CVMX_MIO_UART2_HTX (CVMX_ADD_IO_SEG(0x0001180000000708ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_IER CVMX_MIO_UART2_IER_FUNC()
static inline uint64_t CVMX_MIO_UART2_IER_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_IER not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000408ull);
}
#else
#define CVMX_MIO_UART2_IER (CVMX_ADD_IO_SEG(0x0001180000000408ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_IIR CVMX_MIO_UART2_IIR_FUNC()
static inline uint64_t CVMX_MIO_UART2_IIR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_IIR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000410ull);
}
#else
#define CVMX_MIO_UART2_IIR (CVMX_ADD_IO_SEG(0x0001180000000410ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_LCR CVMX_MIO_UART2_LCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_LCR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_LCR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000418ull);
}
#else
#define CVMX_MIO_UART2_LCR (CVMX_ADD_IO_SEG(0x0001180000000418ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_LSR CVMX_MIO_UART2_LSR_FUNC()
static inline uint64_t CVMX_MIO_UART2_LSR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_LSR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000428ull);
}
#else
#define CVMX_MIO_UART2_LSR (CVMX_ADD_IO_SEG(0x0001180000000428ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_MCR CVMX_MIO_UART2_MCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_MCR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_MCR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000420ull);
}
#else
#define CVMX_MIO_UART2_MCR (CVMX_ADD_IO_SEG(0x0001180000000420ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_MSR CVMX_MIO_UART2_MSR_FUNC()
static inline uint64_t CVMX_MIO_UART2_MSR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_MSR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000430ull);
}
#else
#define CVMX_MIO_UART2_MSR (CVMX_ADD_IO_SEG(0x0001180000000430ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_RBR CVMX_MIO_UART2_RBR_FUNC()
static inline uint64_t CVMX_MIO_UART2_RBR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_RBR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000400ull);
}
#else
#define CVMX_MIO_UART2_RBR (CVMX_ADD_IO_SEG(0x0001180000000400ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_RFL CVMX_MIO_UART2_RFL_FUNC()
static inline uint64_t CVMX_MIO_UART2_RFL_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_RFL not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000608ull);
}
#else
#define CVMX_MIO_UART2_RFL (CVMX_ADD_IO_SEG(0x0001180000000608ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_RFW CVMX_MIO_UART2_RFW_FUNC()
static inline uint64_t CVMX_MIO_UART2_RFW_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_RFW not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000530ull);
}
#else
#define CVMX_MIO_UART2_RFW (CVMX_ADD_IO_SEG(0x0001180000000530ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_SBCR CVMX_MIO_UART2_SBCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_SBCR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_SBCR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000620ull);
}
#else
#define CVMX_MIO_UART2_SBCR (CVMX_ADD_IO_SEG(0x0001180000000620ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_SCR CVMX_MIO_UART2_SCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_SCR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_SCR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000438ull);
}
#else
#define CVMX_MIO_UART2_SCR (CVMX_ADD_IO_SEG(0x0001180000000438ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_SFE CVMX_MIO_UART2_SFE_FUNC()
static inline uint64_t CVMX_MIO_UART2_SFE_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_SFE not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000630ull);
}
#else
#define CVMX_MIO_UART2_SFE (CVMX_ADD_IO_SEG(0x0001180000000630ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_SRR CVMX_MIO_UART2_SRR_FUNC()
static inline uint64_t CVMX_MIO_UART2_SRR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_SRR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000610ull);
}
#else
#define CVMX_MIO_UART2_SRR (CVMX_ADD_IO_SEG(0x0001180000000610ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_SRT CVMX_MIO_UART2_SRT_FUNC()
static inline uint64_t CVMX_MIO_UART2_SRT_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_SRT not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000638ull);
}
#else
#define CVMX_MIO_UART2_SRT (CVMX_ADD_IO_SEG(0x0001180000000638ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_SRTS CVMX_MIO_UART2_SRTS_FUNC()
static inline uint64_t CVMX_MIO_UART2_SRTS_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_SRTS not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000618ull);
}
#else
#define CVMX_MIO_UART2_SRTS (CVMX_ADD_IO_SEG(0x0001180000000618ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_STT CVMX_MIO_UART2_STT_FUNC()
static inline uint64_t CVMX_MIO_UART2_STT_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_STT not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000700ull);
}
#else
#define CVMX_MIO_UART2_STT (CVMX_ADD_IO_SEG(0x0001180000000700ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_TFL CVMX_MIO_UART2_TFL_FUNC()
static inline uint64_t CVMX_MIO_UART2_TFL_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_TFL not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000600ull);
}
#else
#define CVMX_MIO_UART2_TFL (CVMX_ADD_IO_SEG(0x0001180000000600ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_TFR CVMX_MIO_UART2_TFR_FUNC()
static inline uint64_t CVMX_MIO_UART2_TFR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_TFR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000528ull);
}
#else
#define CVMX_MIO_UART2_TFR (CVMX_ADD_IO_SEG(0x0001180000000528ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_THR CVMX_MIO_UART2_THR_FUNC()
static inline uint64_t CVMX_MIO_UART2_THR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_THR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000440ull);
}
#else
#define CVMX_MIO_UART2_THR (CVMX_ADD_IO_SEG(0x0001180000000440ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#define CVMX_MIO_UART2_USR CVMX_MIO_UART2_USR_FUNC()
static inline uint64_t CVMX_MIO_UART2_USR_FUNC(void)
{
	if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
		cvmx_warn("CVMX_MIO_UART2_USR not supported on this chip\n");
	return CVMX_ADD_IO_SEG(0x0001180000000538ull);
}
#else
#define CVMX_MIO_UART2_USR (CVMX_ADD_IO_SEG(0x0001180000000538ull))
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_DLH(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_DLH(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000888ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_DLH(offset) (CVMX_ADD_IO_SEG(0x0001180000000888ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_DLL(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_DLL(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000880ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_DLL(offset) (CVMX_ADD_IO_SEG(0x0001180000000880ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_FAR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_FAR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000920ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_FAR(offset) (CVMX_ADD_IO_SEG(0x0001180000000920ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_FCR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_FCR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000850ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_FCR(offset) (CVMX_ADD_IO_SEG(0x0001180000000850ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_HTX(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_HTX(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000B08ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_HTX(offset) (CVMX_ADD_IO_SEG(0x0001180000000B08ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_IER(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_IER(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000808ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_IER(offset) (CVMX_ADD_IO_SEG(0x0001180000000808ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_IIR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_IIR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000810ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_IIR(offset) (CVMX_ADD_IO_SEG(0x0001180000000810ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_LCR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_LCR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000818ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_LCR(offset) (CVMX_ADD_IO_SEG(0x0001180000000818ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_LSR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_LSR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000828ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_LSR(offset) (CVMX_ADD_IO_SEG(0x0001180000000828ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_MCR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_MCR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000820ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_MCR(offset) (CVMX_ADD_IO_SEG(0x0001180000000820ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_MSR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_MSR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000830ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_MSR(offset) (CVMX_ADD_IO_SEG(0x0001180000000830ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_RBR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_RBR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000800ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_RBR(offset) (CVMX_ADD_IO_SEG(0x0001180000000800ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_RFL(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_RFL(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000A08ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_RFL(offset) (CVMX_ADD_IO_SEG(0x0001180000000A08ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_RFW(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_RFW(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000930ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_RFW(offset) (CVMX_ADD_IO_SEG(0x0001180000000930ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_SBCR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_SBCR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000A20ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_SBCR(offset) (CVMX_ADD_IO_SEG(0x0001180000000A20ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_SCR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_SCR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000838ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_SCR(offset) (CVMX_ADD_IO_SEG(0x0001180000000838ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_SFE(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_SFE(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000A30ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_SFE(offset) (CVMX_ADD_IO_SEG(0x0001180000000A30ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_SRR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_SRR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000A10ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_SRR(offset) (CVMX_ADD_IO_SEG(0x0001180000000A10ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_SRT(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_SRT(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000A38ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_SRT(offset) (CVMX_ADD_IO_SEG(0x0001180000000A38ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_SRTS(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_SRTS(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000A18ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_SRTS(offset) (CVMX_ADD_IO_SEG(0x0001180000000A18ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_STT(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_STT(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000B00ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_STT(offset) (CVMX_ADD_IO_SEG(0x0001180000000B00ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_TFL(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_TFL(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000A00ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_TFL(offset) (CVMX_ADD_IO_SEG(0x0001180000000A00ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_TFR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_TFR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000928ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_TFR(offset) (CVMX_ADD_IO_SEG(0x0001180000000928ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_THR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_THR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000840ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_THR(offset) (CVMX_ADD_IO_SEG(0x0001180000000840ull) + ((offset) & 1) * 1024)
#endif
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
static inline uint64_t CVMX_MIO_UARTX_USR(unsigned long offset)
{
	if (!(
	      (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
	      (OCTEON_IS_MODEL(OCTEON_CN63XX) && ((offset <= 1)))))
		cvmx_warn("CVMX_MIO_UARTX_USR(%lu) is invalid on this chip\n", offset);
	return CVMX_ADD_IO_SEG(0x0001180000000938ull) + ((offset) & 1) * 1024;
}
#else
#define CVMX_MIO_UARTX_USR(offset) (CVMX_ADD_IO_SEG(0x0001180000000938ull) + ((offset) & 1) * 1024)
#endif

/**
 * cvmx_mio_boot_bist_stat
 *
 * MIO_BOOT_BIST_STAT = MIO Boot BIST Status Register
 *
 * Contains the BIST status for the MIO boot memories.  '0' = pass, '1' = fail.
 */
union cvmx_mio_boot_bist_stat
{
	uint64_t u64;
	struct cvmx_mio_boot_bist_stat_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_0_63                : 64;
#else
	uint64_t reserved_0_63                : 64;
#endif
	} s;
	struct cvmx_mio_boot_bist_stat_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_4_63                : 60;
	uint64_t ncbo_1                       : 1;  /**< NCB output FIFO 1 BIST status */
	uint64_t ncbo_0                       : 1;  /**< NCB output FIFO 0 BIST status */
	uint64_t loc                          : 1;  /**< Local memory BIST status */
	uint64_t ncbi                         : 1;  /**< NCB input FIFO BIST status */
#else
	uint64_t ncbi                         : 1;
	uint64_t loc                          : 1;
	uint64_t ncbo_0                       : 1;
	uint64_t ncbo_1                       : 1;
	uint64_t reserved_4_63                : 60;
#endif
	} cn30xx;
	struct cvmx_mio_boot_bist_stat_cn30xx cn31xx;
	struct cvmx_mio_boot_bist_stat_cn38xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_3_63                : 61;
	uint64_t ncbo_0                       : 1;  /**< NCB output FIFO BIST status */
	uint64_t loc                          : 1;  /**< Local memory BIST status */
	uint64_t ncbi                         : 1;  /**< NCB input FIFO BIST status */
#else
	uint64_t ncbi                         : 1;
	uint64_t loc                          : 1;
	uint64_t ncbo_0                       : 1;
	uint64_t reserved_3_63                : 61;
#endif
	} cn38xx;
	struct cvmx_mio_boot_bist_stat_cn38xx cn38xxp2;
	struct cvmx_mio_boot_bist_stat_cn50xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_6_63                : 58;
	uint64_t pcm_1                        : 1;  /**< PCM memory 1 BIST status */
	uint64_t pcm_0                        : 1;  /**< PCM memory 0 BIST status */
	uint64_t ncbo_1                       : 1;  /**< NCB output FIFO 1 BIST status */
	uint64_t ncbo_0                       : 1;  /**< NCB output FIFO 0 BIST status */
	uint64_t loc                          : 1;  /**< Local memory BIST status */
	uint64_t ncbi                         : 1;  /**< NCB input FIFO BIST status */
#else
	uint64_t ncbi                         : 1;
	uint64_t loc                          : 1;
	uint64_t ncbo_0                       : 1;
	uint64_t ncbo_1                       : 1;
	uint64_t pcm_0                        : 1;
	uint64_t pcm_1                        : 1;
	uint64_t reserved_6_63                : 58;
#endif
	} cn50xx;
	struct cvmx_mio_boot_bist_stat_cn52xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_6_63                : 58;
	uint64_t ndf                          : 2;  /**< NAND flash BIST status */
	uint64_t ncbo_0                       : 1;  /**< NCB output FIFO BIST status */
	uint64_t dma                          : 1;  /**< DMA memory BIST status */
	uint64_t loc                          : 1;  /**< Local memory BIST status */
	uint64_t ncbi                         : 1;  /**< NCB input FIFO BIST status */
#else
	uint64_t ncbi                         : 1;
	uint64_t loc                          : 1;
	uint64_t dma                          : 1;
	uint64_t ncbo_0                       : 1;
	uint64_t ndf                          : 2;
	uint64_t reserved_6_63                : 58;
#endif
	} cn52xx;
	struct cvmx_mio_boot_bist_stat_cn52xxp1
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_4_63                : 60;
	uint64_t ncbo_0                       : 1;  /**< NCB output FIFO BIST status */
	uint64_t dma                          : 1;  /**< DMA memory BIST status */
	uint64_t loc                          : 1;  /**< Local memory BIST status */
	uint64_t ncbi                         : 1;  /**< NCB input FIFO BIST status */
#else
	uint64_t ncbi                         : 1;
	uint64_t loc                          : 1;
	uint64_t dma                          : 1;
	uint64_t ncbo_0                       : 1;
	uint64_t reserved_4_63                : 60;
#endif
	} cn52xxp1;
	struct cvmx_mio_boot_bist_stat_cn52xxp1 cn56xx;
	struct cvmx_mio_boot_bist_stat_cn52xxp1 cn56xxp1;
	struct cvmx_mio_boot_bist_stat_cn38xx cn58xx;
	struct cvmx_mio_boot_bist_stat_cn38xx cn58xxp1;
	struct cvmx_mio_boot_bist_stat_cn63xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_9_63                : 55;
	uint64_t stat                         : 9;  /**< BIST status */
#else
	uint64_t stat                         : 9;
	uint64_t reserved_9_63                : 55;
#endif
	} cn63xx;
	struct cvmx_mio_boot_bist_stat_cn63xx cn63xxp1;
};
typedef union cvmx_mio_boot_bist_stat cvmx_mio_boot_bist_stat_t;

/**
 * cvmx_mio_boot_comp
 *
 * MIO_BOOT_COMP = MIO Boot Compensation Register
 *
 * Reset value is as follows:
 *
 * no pullups,               PCTL=38, NCTL=30 (25 ohm termination)
 * pullup on boot_ad[9],     PCTL=19, NCTL=15 (50 ohm termination)
 * pullup on boot_ad[10],    PCTL=15, NCTL=12 (65 ohm termination)
 * pullups on boot_ad[10:9], PCTL=15, NCTL=12 (65 ohm termination)
 */
union cvmx_mio_boot_comp
{
	uint64_t u64;
	struct cvmx_mio_boot_comp_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_0_63                : 64;
#else
	uint64_t reserved_0_63                : 64;
#endif
	} s;
	struct cvmx_mio_boot_comp_cn50xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_10_63               : 54;
	uint64_t pctl                         : 5;  /**< Boot bus PCTL */
	uint64_t nctl                         : 5;  /**< Boot bus NCTL */
#else
	uint64_t nctl                         : 5;
	uint64_t pctl                         : 5;
	uint64_t reserved_10_63               : 54;
#endif
	} cn50xx;
	struct cvmx_mio_boot_comp_cn50xx      cn52xx;
	struct cvmx_mio_boot_comp_cn50xx      cn52xxp1;
	struct cvmx_mio_boot_comp_cn50xx      cn56xx;
	struct cvmx_mio_boot_comp_cn50xx      cn56xxp1;
	struct cvmx_mio_boot_comp_cn63xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_12_63               : 52;
	uint64_t pctl                         : 6;  /**< Boot bus PCTL */
	uint64_t nctl                         : 6;  /**< Boot bus NCTL */
#else
	uint64_t nctl                         : 6;
	uint64_t pctl                         : 6;
	uint64_t reserved_12_63               : 52;
#endif
	} cn63xx;
	struct cvmx_mio_boot_comp_cn63xx      cn63xxp1;
};
typedef union cvmx_mio_boot_comp cvmx_mio_boot_comp_t;

/**
 * cvmx_mio_boot_dma_cfg#
 *
 * MIO_BOOT_DMA_CFG = MIO Boot DMA Config Register (1 per engine * 2 engines)
 *
 * SIZE is specified in number of bus transfers, where one transfer is equal to the following number
 * of bytes dependent on MIO_BOOT_DMA_TIMn[WIDTH] and MIO_BOOT_DMA_TIMn[DDR]:
 *
 * WIDTH     DDR      Transfer Size (bytes)
 * ----------------------------------------
 *   0        0               2
 *   0        1               4
 *   1        0               4
 *   1        1               8
 *
 * Note: ADR must be aligned to the bus width (i.e. 16 bit aligned if WIDTH=0, 32 bit aligned if WIDTH=1).
 */
union cvmx_mio_boot_dma_cfgx
{
	uint64_t u64;
	struct cvmx_mio_boot_dma_cfgx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t en                           : 1;  /**< DMA Engine X enable */
	uint64_t rw                           : 1;  /**< DMA Engine X R/W bit (0 = read, 1 = write) */
	uint64_t clr                          : 1;  /**< DMA Engine X clear EN on device terminated burst */
	uint64_t reserved_60_60               : 1;
	uint64_t swap32                       : 1;  /**< DMA Engine X 32 bit swap */
	uint64_t swap16                       : 1;  /**< DMA Engine X 16 bit swap */
	uint64_t swap8                        : 1;  /**< DMA Engine X 8 bit swap */
	uint64_t endian                       : 1;  /**< DMA Engine X NCB endian mode (0 = big, 1 = little) */
	uint64_t size                         : 20; /**< DMA Engine X size */
	uint64_t adr                          : 36; /**< DMA Engine X address */
#else
	uint64_t adr                          : 36;
	uint64_t size                         : 20;
	uint64_t endian                       : 1;
	uint64_t swap8                        : 1;
	uint64_t swap16                       : 1;
	uint64_t swap32                       : 1;
	uint64_t reserved_60_60               : 1;
	uint64_t clr                          : 1;
	uint64_t rw                           : 1;
	uint64_t en                           : 1;
#endif
	} s;
	struct cvmx_mio_boot_dma_cfgx_s       cn52xx;
	struct cvmx_mio_boot_dma_cfgx_s       cn52xxp1;
	struct cvmx_mio_boot_dma_cfgx_s       cn56xx;
	struct cvmx_mio_boot_dma_cfgx_s       cn56xxp1;
	struct cvmx_mio_boot_dma_cfgx_s       cn63xx;
	struct cvmx_mio_boot_dma_cfgx_s       cn63xxp1;
};
typedef union cvmx_mio_boot_dma_cfgx cvmx_mio_boot_dma_cfgx_t;

/**
 * cvmx_mio_boot_dma_int#
 *
 * MIO_BOOT_DMA_INT = MIO Boot DMA Interrupt Register (1 per engine * 2 engines)
 *
 */
union cvmx_mio_boot_dma_intx
{
	uint64_t u64;
	struct cvmx_mio_boot_dma_intx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t dmarq                        : 1;  /**< DMA Engine X DMARQ asserted interrupt */
	uint64_t done                         : 1;  /**< DMA Engine X request completion interrupt */
#else
	uint64_t done                         : 1;
	uint64_t dmarq                        : 1;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_boot_dma_intx_s       cn52xx;
	struct cvmx_mio_boot_dma_intx_s       cn52xxp1;
	struct cvmx_mio_boot_dma_intx_s       cn56xx;
	struct cvmx_mio_boot_dma_intx_s       cn56xxp1;
	struct cvmx_mio_boot_dma_intx_s       cn63xx;
	struct cvmx_mio_boot_dma_intx_s       cn63xxp1;
};
typedef union cvmx_mio_boot_dma_intx cvmx_mio_boot_dma_intx_t;

/**
 * cvmx_mio_boot_dma_int_en#
 *
 * MIO_BOOT_DMA_INT_EN = MIO Boot DMA Interrupt Enable Register (1 per engine * 2 engines)
 *
 */
union cvmx_mio_boot_dma_int_enx
{
	uint64_t u64;
	struct cvmx_mio_boot_dma_int_enx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t dmarq                        : 1;  /**< DMA Engine X DMARQ asserted interrupt enable */
	uint64_t done                         : 1;  /**< DMA Engine X request completion interrupt enable */
#else
	uint64_t done                         : 1;
	uint64_t dmarq                        : 1;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_boot_dma_int_enx_s    cn52xx;
	struct cvmx_mio_boot_dma_int_enx_s    cn52xxp1;
	struct cvmx_mio_boot_dma_int_enx_s    cn56xx;
	struct cvmx_mio_boot_dma_int_enx_s    cn56xxp1;
	struct cvmx_mio_boot_dma_int_enx_s    cn63xx;
	struct cvmx_mio_boot_dma_int_enx_s    cn63xxp1;
};
typedef union cvmx_mio_boot_dma_int_enx cvmx_mio_boot_dma_int_enx_t;

/**
 * cvmx_mio_boot_dma_tim#
 *
 * MIO_BOOT_DMA_TIM = MIO Boot DMA Timing Register (1 per engine * 2 engines)
 *
 * DMACK_PI inverts the assertion level of boot_dmack[n].  The default polarity of boot_dmack[1:0] is
 * selected on the first de-assertion of reset by the values on boot_ad[12:11], where 0 is active high
 * and 1 is active low (see MIO_BOOT_PIN_DEFS for a read-only copy of the default polarity).
 * boot_ad[12:11] have internal pulldowns, so place a pullup on boot_ad[n+11] for active low default
 * polarity on engine n.  To interface with CF cards in True IDE Mode, either a pullup should be placed
 * on boot_ad[n+11] OR the corresponding DMACK_PI[n] should be set.
 *
 * DMARQ_PI inverts the assertion level of boot_dmarq[n].  The default polarity of boot_dmarq[1:0] is
 * active high, thus setting the polarity inversion bits changes the polarity to active low.  To
 * interface with CF cards in True IDE Mode, the corresponding DMARQ_PI[n] should be clear.
 *
 * TIM_MULT specifies the timing multiplier for an engine.  The timing multiplier applies to all timing
 * parameters, except for DMARQ and RD_DLY, which simply count eclks.  TIM_MULT is encoded as follows:
 * 0 = 4x, 1 = 1x, 2 = 2x, 3 = 8x.
 *
 * RD_DLY specifies the read sample delay in eclk cycles for an engine.  For reads, the data bus is
 * normally sampled on the same eclk edge that drives boot_oe_n high (and also low in DDR mode).
 * This parameter can delay that sampling edge by up to 7 eclks.  Note: the number of eclk cycles
 * counted by the OE_A and DMACK_H + PAUSE timing parameters must be greater than RD_DLY.
 *
 * If DDR is set, then WE_N must be less than WE_A.
 */
union cvmx_mio_boot_dma_timx
{
	uint64_t u64;
	struct cvmx_mio_boot_dma_timx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t dmack_pi                     : 1;  /**< DMA Engine X DMA ack polarity inversion */
	uint64_t dmarq_pi                     : 1;  /**< DMA Engine X DMA request polarity inversion */
	uint64_t tim_mult                     : 2;  /**< DMA Engine X timing multiplier */
	uint64_t rd_dly                       : 3;  /**< DMA Engine X read sample delay */
	uint64_t ddr                          : 1;  /**< DMA Engine X DDR mode */
	uint64_t width                        : 1;  /**< DMA Engine X bus width (0 = 16 bits, 1 = 32 bits) */
	uint64_t reserved_48_54               : 7;
	uint64_t pause                        : 6;  /**< DMA Engine X pause count */
	uint64_t dmack_h                      : 6;  /**< DMA Engine X DMA ack hold count */
	uint64_t we_n                         : 6;  /**< DMA Engine X write enable negated count */
	uint64_t we_a                         : 6;  /**< DMA Engine X write enable asserted count */
	uint64_t oe_n                         : 6;  /**< DMA Engine X output enable negated count */
	uint64_t oe_a                         : 6;  /**< DMA Engine X output enable asserted count */
	uint64_t dmack_s                      : 6;  /**< DMA Engine X DMA ack setup count */
	uint64_t dmarq                        : 6;  /**< DMA Engine X DMA request count (must be non-zero) */
#else
	uint64_t dmarq                        : 6;
	uint64_t dmack_s                      : 6;
	uint64_t oe_a                         : 6;
	uint64_t oe_n                         : 6;
	uint64_t we_a                         : 6;
	uint64_t we_n                         : 6;
	uint64_t dmack_h                      : 6;
	uint64_t pause                        : 6;
	uint64_t reserved_48_54               : 7;
	uint64_t width                        : 1;
	uint64_t ddr                          : 1;
	uint64_t rd_dly                       : 3;
	uint64_t tim_mult                     : 2;
	uint64_t dmarq_pi                     : 1;
	uint64_t dmack_pi                     : 1;
#endif
	} s;
	struct cvmx_mio_boot_dma_timx_s       cn52xx;
	struct cvmx_mio_boot_dma_timx_s       cn52xxp1;
	struct cvmx_mio_boot_dma_timx_s       cn56xx;
	struct cvmx_mio_boot_dma_timx_s       cn56xxp1;
	struct cvmx_mio_boot_dma_timx_s       cn63xx;
	struct cvmx_mio_boot_dma_timx_s       cn63xxp1;
};
typedef union cvmx_mio_boot_dma_timx cvmx_mio_boot_dma_timx_t;

/**
 * cvmx_mio_boot_err
 *
 * MIO_BOOT_ERR = MIO Boot Error Register
 *
 * Contains the address decode error and wait mode error bits.  Address decode error is set when a
 * boot bus access does not hit in any of the 8 remote regions or 2 local regions.  Wait mode error is
 * set when wait mode is enabled and the external wait signal is not de-asserted after 32k eclk cycles.
 */
union cvmx_mio_boot_err
{
	uint64_t u64;
	struct cvmx_mio_boot_err_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t wait_err                     : 1;  /**< Wait mode error */
	uint64_t adr_err                      : 1;  /**< Address decode error */
#else
	uint64_t adr_err                      : 1;
	uint64_t wait_err                     : 1;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_boot_err_s            cn30xx;
	struct cvmx_mio_boot_err_s            cn31xx;
	struct cvmx_mio_boot_err_s            cn38xx;
	struct cvmx_mio_boot_err_s            cn38xxp2;
	struct cvmx_mio_boot_err_s            cn50xx;
	struct cvmx_mio_boot_err_s            cn52xx;
	struct cvmx_mio_boot_err_s            cn52xxp1;
	struct cvmx_mio_boot_err_s            cn56xx;
	struct cvmx_mio_boot_err_s            cn56xxp1;
	struct cvmx_mio_boot_err_s            cn58xx;
	struct cvmx_mio_boot_err_s            cn58xxp1;
	struct cvmx_mio_boot_err_s            cn63xx;
	struct cvmx_mio_boot_err_s            cn63xxp1;
};
typedef union cvmx_mio_boot_err cvmx_mio_boot_err_t;

/**
 * cvmx_mio_boot_int
 *
 * MIO_BOOT_INT = MIO Boot Interrupt Register
 *
 * Contains the interrupt enable bits for address decode error and wait mode error.
 */
union cvmx_mio_boot_int
{
	uint64_t u64;
	struct cvmx_mio_boot_int_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t wait_int                     : 1;  /**< Wait mode error interrupt enable */
	uint64_t adr_int                      : 1;  /**< Address decode error interrupt enable */
#else
	uint64_t adr_int                      : 1;
	uint64_t wait_int                     : 1;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_boot_int_s            cn30xx;
	struct cvmx_mio_boot_int_s            cn31xx;
	struct cvmx_mio_boot_int_s            cn38xx;
	struct cvmx_mio_boot_int_s            cn38xxp2;
	struct cvmx_mio_boot_int_s            cn50xx;
	struct cvmx_mio_boot_int_s            cn52xx;
	struct cvmx_mio_boot_int_s            cn52xxp1;
	struct cvmx_mio_boot_int_s            cn56xx;
	struct cvmx_mio_boot_int_s            cn56xxp1;
	struct cvmx_mio_boot_int_s            cn58xx;
	struct cvmx_mio_boot_int_s            cn58xxp1;
	struct cvmx_mio_boot_int_s            cn63xx;
	struct cvmx_mio_boot_int_s            cn63xxp1;
};
typedef union cvmx_mio_boot_int cvmx_mio_boot_int_t;

/**
 * cvmx_mio_boot_loc_adr
 *
 * MIO_BOOT_LOC_ADR = MIO Boot Local Memory Address Register
 *
 * Specifies the address for reading or writing the local memory.  This address will post-increment
 * following an access to the MIO Boot Local Memory Data Register (MIO_BOOT_LOC_DAT).
 *
 * Local memory region 0 exists from addresses 0x00 - 0x78.
 * Local memory region 1 exists from addresses 0x80 - 0xf8.
 */
union cvmx_mio_boot_loc_adr
{
	uint64_t u64;
	struct cvmx_mio_boot_loc_adr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t adr                          : 5;  /**< Local memory address */
	uint64_t reserved_0_2                 : 3;
#else
	uint64_t reserved_0_2                 : 3;
	uint64_t adr                          : 5;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_boot_loc_adr_s        cn30xx;
	struct cvmx_mio_boot_loc_adr_s        cn31xx;
	struct cvmx_mio_boot_loc_adr_s        cn38xx;
	struct cvmx_mio_boot_loc_adr_s        cn38xxp2;
	struct cvmx_mio_boot_loc_adr_s        cn50xx;
	struct cvmx_mio_boot_loc_adr_s        cn52xx;
	struct cvmx_mio_boot_loc_adr_s        cn52xxp1;
	struct cvmx_mio_boot_loc_adr_s        cn56xx;
	struct cvmx_mio_boot_loc_adr_s        cn56xxp1;
	struct cvmx_mio_boot_loc_adr_s        cn58xx;
	struct cvmx_mio_boot_loc_adr_s        cn58xxp1;
	struct cvmx_mio_boot_loc_adr_s        cn63xx;
	struct cvmx_mio_boot_loc_adr_s        cn63xxp1;
};
typedef union cvmx_mio_boot_loc_adr cvmx_mio_boot_loc_adr_t;

/**
 * cvmx_mio_boot_loc_cfg#
 *
 * MIO_BOOT_LOC_CFG = MIO Boot Local Region Config Register (1 per region * 2 regions)
 *
 * Contains local region enable and local region base address parameters.  Each local region is 128
 * bytes organized as 16 entries x 8 bytes.
 *
 * Base address specifies address bits [31:7] of the region.
 */
union cvmx_mio_boot_loc_cfgx
{
	uint64_t u64;
	struct cvmx_mio_boot_loc_cfgx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t en                           : 1;  /**< Local region X enable */
	uint64_t reserved_28_30               : 3;
	uint64_t base                         : 25; /**< Local region X base address */
	uint64_t reserved_0_2                 : 3;
#else
	uint64_t reserved_0_2                 : 3;
	uint64_t base                         : 25;
	uint64_t reserved_28_30               : 3;
	uint64_t en                           : 1;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_mio_boot_loc_cfgx_s       cn30xx;
	struct cvmx_mio_boot_loc_cfgx_s       cn31xx;
	struct cvmx_mio_boot_loc_cfgx_s       cn38xx;
	struct cvmx_mio_boot_loc_cfgx_s       cn38xxp2;
	struct cvmx_mio_boot_loc_cfgx_s       cn50xx;
	struct cvmx_mio_boot_loc_cfgx_s       cn52xx;
	struct cvmx_mio_boot_loc_cfgx_s       cn52xxp1;
	struct cvmx_mio_boot_loc_cfgx_s       cn56xx;
	struct cvmx_mio_boot_loc_cfgx_s       cn56xxp1;
	struct cvmx_mio_boot_loc_cfgx_s       cn58xx;
	struct cvmx_mio_boot_loc_cfgx_s       cn58xxp1;
	struct cvmx_mio_boot_loc_cfgx_s       cn63xx;
	struct cvmx_mio_boot_loc_cfgx_s       cn63xxp1;
};
typedef union cvmx_mio_boot_loc_cfgx cvmx_mio_boot_loc_cfgx_t;

/**
 * cvmx_mio_boot_loc_dat
 *
 * MIO_BOOT_LOC_DAT = MIO Boot Local Memory Data Register
 *
 * This is a pseudo-register that will read/write the local memory at the address specified by the MIO
 * Boot Local Address Register (MIO_BOOT_LOC_ADR) when accessed.
 */
union cvmx_mio_boot_loc_dat
{
	uint64_t u64;
	struct cvmx_mio_boot_loc_dat_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t data                         : 64; /**< Local memory data */
#else
	uint64_t data                         : 64;
#endif
	} s;
	struct cvmx_mio_boot_loc_dat_s        cn30xx;
	struct cvmx_mio_boot_loc_dat_s        cn31xx;
	struct cvmx_mio_boot_loc_dat_s        cn38xx;
	struct cvmx_mio_boot_loc_dat_s        cn38xxp2;
	struct cvmx_mio_boot_loc_dat_s        cn50xx;
	struct cvmx_mio_boot_loc_dat_s        cn52xx;
	struct cvmx_mio_boot_loc_dat_s        cn52xxp1;
	struct cvmx_mio_boot_loc_dat_s        cn56xx;
	struct cvmx_mio_boot_loc_dat_s        cn56xxp1;
	struct cvmx_mio_boot_loc_dat_s        cn58xx;
	struct cvmx_mio_boot_loc_dat_s        cn58xxp1;
	struct cvmx_mio_boot_loc_dat_s        cn63xx;
	struct cvmx_mio_boot_loc_dat_s        cn63xxp1;
};
typedef union cvmx_mio_boot_loc_dat cvmx_mio_boot_loc_dat_t;

/**
 * cvmx_mio_boot_pin_defs
 *
 * MIO_BOOT_PIN_DEFS = MIO Boot Pin Defaults Register
 *
 */
union cvmx_mio_boot_pin_defs
{
	uint64_t u64;
	struct cvmx_mio_boot_pin_defs_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t ale                          : 1;  /**< Region 0 default ALE mode */
	uint64_t width                        : 1;  /**< Region 0 default bus width */
	uint64_t dmack_p2                     : 1;  /**< boot_dmack[2] default polarity */
	uint64_t dmack_p1                     : 1;  /**< boot_dmack[1] default polarity */
	uint64_t dmack_p0                     : 1;  /**< boot_dmack[0] default polarity */
	uint64_t term                         : 2;  /**< Selects default driver termination */
	uint64_t nand                         : 1;  /**< Region 0 is NAND flash */
	uint64_t reserved_0_7                 : 8;
#else
	uint64_t reserved_0_7                 : 8;
	uint64_t nand                         : 1;
	uint64_t term                         : 2;
	uint64_t dmack_p0                     : 1;
	uint64_t dmack_p1                     : 1;
	uint64_t dmack_p2                     : 1;
	uint64_t width                        : 1;
	uint64_t ale                          : 1;
	uint64_t reserved_16_63               : 48;
#endif
	} s;
	struct cvmx_mio_boot_pin_defs_cn52xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t ale                          : 1;  /**< Region 0 default ALE mode */
	uint64_t width                        : 1;  /**< Region 0 default bus width */
	uint64_t reserved_13_13               : 1;
	uint64_t dmack_p1                     : 1;  /**< boot_dmack[1] default polarity */
	uint64_t dmack_p0                     : 1;  /**< boot_dmack[0] default polarity */
	uint64_t term                         : 2;  /**< Selects default driver termination */
	uint64_t nand                         : 1;  /**< Region 0 is NAND flash */
	uint64_t reserved_0_7                 : 8;
#else
	uint64_t reserved_0_7                 : 8;
	uint64_t nand                         : 1;
	uint64_t term                         : 2;
	uint64_t dmack_p0                     : 1;
	uint64_t dmack_p1                     : 1;
	uint64_t reserved_13_13               : 1;
	uint64_t width                        : 1;
	uint64_t ale                          : 1;
	uint64_t reserved_16_63               : 48;
#endif
	} cn52xx;
	struct cvmx_mio_boot_pin_defs_cn56xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_16_63               : 48;
	uint64_t ale                          : 1;  /**< Region 0 default ALE mode */
	uint64_t width                        : 1;  /**< Region 0 default bus width */
	uint64_t dmack_p2                     : 1;  /**< boot_dmack[2] default polarity */
	uint64_t dmack_p1                     : 1;  /**< boot_dmack[1] default polarity */
	uint64_t dmack_p0                     : 1;  /**< boot_dmack[0] default polarity */
	uint64_t term                         : 2;  /**< Selects default driver termination */
	uint64_t reserved_0_8                 : 9;
#else
	uint64_t reserved_0_8                 : 9;
	uint64_t term                         : 2;
	uint64_t dmack_p0                     : 1;
	uint64_t dmack_p1                     : 1;
	uint64_t dmack_p2                     : 1;
	uint64_t width                        : 1;
	uint64_t ale                          : 1;
	uint64_t reserved_16_63               : 48;
#endif
	} cn56xx;
	struct cvmx_mio_boot_pin_defs_cn52xx  cn63xx;
	struct cvmx_mio_boot_pin_defs_cn52xx  cn63xxp1;
};
typedef union cvmx_mio_boot_pin_defs cvmx_mio_boot_pin_defs_t;

/**
 * cvmx_mio_boot_reg_cfg#
 */
union cvmx_mio_boot_reg_cfgx
{
	uint64_t u64;
	struct cvmx_mio_boot_reg_cfgx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_44_63               : 20;
	uint64_t dmack                        : 2;  /**< Region X DMACK */
	uint64_t tim_mult                     : 2;  /**< Region X timing multiplier */
	uint64_t rd_dly                       : 3;  /**< Region X read sample delay */
	uint64_t sam                          : 1;  /**< Region X SAM mode */
	uint64_t we_ext                       : 2;  /**< Region X write enable count extension */
	uint64_t oe_ext                       : 2;  /**< Region X output enable count extension */
	uint64_t en                           : 1;  /**< Region X enable */
	uint64_t orbit                        : 1;  /**< Region X or bit */
	uint64_t ale                          : 1;  /**< Region X ALE mode */
	uint64_t width                        : 1;  /**< Region X bus width */
	uint64_t size                         : 12; /**< Region X size */
	uint64_t base                         : 16; /**< Region X base address */
#else
	uint64_t base                         : 16;
	uint64_t size                         : 12;
	uint64_t width                        : 1;
	uint64_t ale                          : 1;
	uint64_t orbit                        : 1;
	uint64_t en                           : 1;
	uint64_t oe_ext                       : 2;
	uint64_t we_ext                       : 2;
	uint64_t sam                          : 1;
	uint64_t rd_dly                       : 3;
	uint64_t tim_mult                     : 2;
	uint64_t dmack                        : 2;
	uint64_t reserved_44_63               : 20;
#endif
	} s;
	struct cvmx_mio_boot_reg_cfgx_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_37_63               : 27;
	uint64_t sam                          : 1;  /**< Region X SAM mode */
	uint64_t we_ext                       : 2;  /**< Region X write enable count extension */
	uint64_t oe_ext                       : 2;  /**< Region X output enable count extension */
	uint64_t en                           : 1;  /**< Region X enable */
	uint64_t orbit                        : 1;  /**< Region X or bit */
	uint64_t ale                          : 1;  /**< Region X ALE mode */
	uint64_t width                        : 1;  /**< Region X bus width */
	uint64_t size                         : 12; /**< Region X size */
	uint64_t base                         : 16; /**< Region X base address */
#else
	uint64_t base                         : 16;
	uint64_t size                         : 12;
	uint64_t width                        : 1;
	uint64_t ale                          : 1;
	uint64_t orbit                        : 1;
	uint64_t en                           : 1;
	uint64_t oe_ext                       : 2;
	uint64_t we_ext                       : 2;
	uint64_t sam                          : 1;
	uint64_t reserved_37_63               : 27;
#endif
	} cn30xx;
	struct cvmx_mio_boot_reg_cfgx_cn30xx  cn31xx;
	struct cvmx_mio_boot_reg_cfgx_cn38xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t en                           : 1;  /**< Region X enable */
	uint64_t orbit                        : 1;  /**< Region X or bit */
	uint64_t reserved_28_29               : 2;
	uint64_t size                         : 12; /**< Region X size */
	uint64_t base                         : 16; /**< Region X base address */
#else
	uint64_t base                         : 16;
	uint64_t size                         : 12;
	uint64_t reserved_28_29               : 2;
	uint64_t orbit                        : 1;
	uint64_t en                           : 1;
	uint64_t reserved_32_63               : 32;
#endif
	} cn38xx;
	struct cvmx_mio_boot_reg_cfgx_cn38xx  cn38xxp2;
	struct cvmx_mio_boot_reg_cfgx_cn50xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_42_63               : 22;
	uint64_t tim_mult                     : 2;  /**< Region X timing multiplier */
	uint64_t rd_dly                       : 3;  /**< Region X read sample delay */
	uint64_t sam                          : 1;  /**< Region X SAM mode */
	uint64_t we_ext                       : 2;  /**< Region X write enable count extension */
	uint64_t oe_ext                       : 2;  /**< Region X output enable count extension */
	uint64_t en                           : 1;  /**< Region X enable */
	uint64_t orbit                        : 1;  /**< Region X or bit */
	uint64_t ale                          : 1;  /**< Region X ALE mode */
	uint64_t width                        : 1;  /**< Region X bus width */
	uint64_t size                         : 12; /**< Region X size */
	uint64_t base                         : 16; /**< Region X base address */
#else
	uint64_t base                         : 16;
	uint64_t size                         : 12;
	uint64_t width                        : 1;
	uint64_t ale                          : 1;
	uint64_t orbit                        : 1;
	uint64_t en                           : 1;
	uint64_t oe_ext                       : 2;
	uint64_t we_ext                       : 2;
	uint64_t sam                          : 1;
	uint64_t rd_dly                       : 3;
	uint64_t tim_mult                     : 2;
	uint64_t reserved_42_63               : 22;
#endif
	} cn50xx;
	struct cvmx_mio_boot_reg_cfgx_s       cn52xx;
	struct cvmx_mio_boot_reg_cfgx_s       cn52xxp1;
	struct cvmx_mio_boot_reg_cfgx_s       cn56xx;
	struct cvmx_mio_boot_reg_cfgx_s       cn56xxp1;
	struct cvmx_mio_boot_reg_cfgx_cn30xx  cn58xx;
	struct cvmx_mio_boot_reg_cfgx_cn30xx  cn58xxp1;
	struct cvmx_mio_boot_reg_cfgx_s       cn63xx;
	struct cvmx_mio_boot_reg_cfgx_s       cn63xxp1;
};
typedef union cvmx_mio_boot_reg_cfgx cvmx_mio_boot_reg_cfgx_t;

/**
 * cvmx_mio_boot_reg_tim#
 */
union cvmx_mio_boot_reg_timx
{
	uint64_t u64;
	struct cvmx_mio_boot_reg_timx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t pagem                        : 1;  /**< Region X page mode */
	uint64_t waitm                        : 1;  /**< Region X wait mode */
	uint64_t pages                        : 2;  /**< Region X page size */
	uint64_t ale                          : 6;  /**< Region X ALE count */
	uint64_t page                         : 6;  /**< Region X page count */
	uint64_t wait                         : 6;  /**< Region X wait count */
	uint64_t pause                        : 6;  /**< Region X pause count */
	uint64_t wr_hld                       : 6;  /**< Region X write hold count */
	uint64_t rd_hld                       : 6;  /**< Region X read hold count */
	uint64_t we                           : 6;  /**< Region X write enable count */
	uint64_t oe                           : 6;  /**< Region X output enable count */
	uint64_t ce                           : 6;  /**< Region X chip enable count */
	uint64_t adr                          : 6;  /**< Region X address count */
#else
	uint64_t adr                          : 6;
	uint64_t ce                           : 6;
	uint64_t oe                           : 6;
	uint64_t we                           : 6;
	uint64_t rd_hld                       : 6;
	uint64_t wr_hld                       : 6;
	uint64_t pause                        : 6;
	uint64_t wait                         : 6;
	uint64_t page                         : 6;
	uint64_t ale                          : 6;
	uint64_t pages                        : 2;
	uint64_t waitm                        : 1;
	uint64_t pagem                        : 1;
#endif
	} s;
	struct cvmx_mio_boot_reg_timx_s       cn30xx;
	struct cvmx_mio_boot_reg_timx_s       cn31xx;
	struct cvmx_mio_boot_reg_timx_cn38xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t pagem                        : 1;  /**< Region X page mode */
	uint64_t waitm                        : 1;  /**< Region X wait mode */
	uint64_t pages                        : 2;  /**< Region X page size (NOT IN PASS 1) */
	uint64_t reserved_54_59               : 6;
	uint64_t page                         : 6;  /**< Region X page count */
	uint64_t wait                         : 6;  /**< Region X wait count */
	uint64_t pause                        : 6;  /**< Region X pause count */
	uint64_t wr_hld                       : 6;  /**< Region X write hold count */
	uint64_t rd_hld                       : 6;  /**< Region X read hold count */
	uint64_t we                           : 6;  /**< Region X write enable count */
	uint64_t oe                           : 6;  /**< Region X output enable count */
	uint64_t ce                           : 6;  /**< Region X chip enable count */
	uint64_t adr                          : 6;  /**< Region X address count */
#else
	uint64_t adr                          : 6;
	uint64_t ce                           : 6;
	uint64_t oe                           : 6;
	uint64_t we                           : 6;
	uint64_t rd_hld                       : 6;
	uint64_t wr_hld                       : 6;
	uint64_t pause                        : 6;
	uint64_t wait                         : 6;
	uint64_t page                         : 6;
	uint64_t reserved_54_59               : 6;
	uint64_t pages                        : 2;
	uint64_t waitm                        : 1;
	uint64_t pagem                        : 1;
#endif
	} cn38xx;
	struct cvmx_mio_boot_reg_timx_cn38xx  cn38xxp2;
	struct cvmx_mio_boot_reg_timx_s       cn50xx;
	struct cvmx_mio_boot_reg_timx_s       cn52xx;
	struct cvmx_mio_boot_reg_timx_s       cn52xxp1;
	struct cvmx_mio_boot_reg_timx_s       cn56xx;
	struct cvmx_mio_boot_reg_timx_s       cn56xxp1;
	struct cvmx_mio_boot_reg_timx_s       cn58xx;
	struct cvmx_mio_boot_reg_timx_s       cn58xxp1;
	struct cvmx_mio_boot_reg_timx_s       cn63xx;
	struct cvmx_mio_boot_reg_timx_s       cn63xxp1;
};
typedef union cvmx_mio_boot_reg_timx cvmx_mio_boot_reg_timx_t;

/**
 * cvmx_mio_boot_thr
 *
 * MIO_BOOT_THR = MIO Boot Threshold Register
 *
 * Contains MIO Boot threshold values:
 *
 * FIF_THR = Assert ncb__busy when the Boot NCB input FIFO reaches this level (not typically for
 *           customer use).
 *
 * DMA_THR = When non-DMA accesses are pending, perform a DMA access after this value of non-DMA
 *           accesses have completed.  If set to zero, only perform a DMA access when non-DMA
 *           accesses are not pending.
 */
union cvmx_mio_boot_thr
{
	uint64_t u64;
	struct cvmx_mio_boot_thr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_22_63               : 42;
	uint64_t dma_thr                      : 6;  /**< DMA threshold */
	uint64_t reserved_14_15               : 2;
	uint64_t fif_cnt                      : 6;  /**< Current NCB FIFO count */
	uint64_t reserved_6_7                 : 2;
	uint64_t fif_thr                      : 6;  /**< NCB busy threshold */
#else
	uint64_t fif_thr                      : 6;
	uint64_t reserved_6_7                 : 2;
	uint64_t fif_cnt                      : 6;
	uint64_t reserved_14_15               : 2;
	uint64_t dma_thr                      : 6;
	uint64_t reserved_22_63               : 42;
#endif
	} s;
	struct cvmx_mio_boot_thr_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_14_63               : 50;
	uint64_t fif_cnt                      : 6;  /**< Current NCB FIFO count */
	uint64_t reserved_6_7                 : 2;
	uint64_t fif_thr                      : 6;  /**< NCB busy threshold */
#else
	uint64_t fif_thr                      : 6;
	uint64_t reserved_6_7                 : 2;
	uint64_t fif_cnt                      : 6;
	uint64_t reserved_14_63               : 50;
#endif
	} cn30xx;
	struct cvmx_mio_boot_thr_cn30xx       cn31xx;
	struct cvmx_mio_boot_thr_cn30xx       cn38xx;
	struct cvmx_mio_boot_thr_cn30xx       cn38xxp2;
	struct cvmx_mio_boot_thr_cn30xx       cn50xx;
	struct cvmx_mio_boot_thr_s            cn52xx;
	struct cvmx_mio_boot_thr_s            cn52xxp1;
	struct cvmx_mio_boot_thr_s            cn56xx;
	struct cvmx_mio_boot_thr_s            cn56xxp1;
	struct cvmx_mio_boot_thr_cn30xx       cn58xx;
	struct cvmx_mio_boot_thr_cn30xx       cn58xxp1;
	struct cvmx_mio_boot_thr_s            cn63xx;
	struct cvmx_mio_boot_thr_s            cn63xxp1;
};
typedef union cvmx_mio_boot_thr cvmx_mio_boot_thr_t;

/**
 * cvmx_mio_fus_bnk_dat#
 *
 * Notes:
 * The intial state of MIO_FUS_BNK_DAT* is as if bank6 was just read i.e. DAT* = fus[895:768]
 *
 */
union cvmx_mio_fus_bnk_datx
{
	uint64_t u64;
	struct cvmx_mio_fus_bnk_datx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t dat                          : 64; /**< Efuse bank store
                                                         For reads, the DAT gets the fus bank last read
                                                         For write, the DAT determines which fuses to blow */
#else
	uint64_t dat                          : 64;
#endif
	} s;
	struct cvmx_mio_fus_bnk_datx_s        cn50xx;
	struct cvmx_mio_fus_bnk_datx_s        cn52xx;
	struct cvmx_mio_fus_bnk_datx_s        cn52xxp1;
	struct cvmx_mio_fus_bnk_datx_s        cn56xx;
	struct cvmx_mio_fus_bnk_datx_s        cn56xxp1;
	struct cvmx_mio_fus_bnk_datx_s        cn58xx;
	struct cvmx_mio_fus_bnk_datx_s        cn58xxp1;
	struct cvmx_mio_fus_bnk_datx_s        cn63xx;
	struct cvmx_mio_fus_bnk_datx_s        cn63xxp1;
};
typedef union cvmx_mio_fus_bnk_datx cvmx_mio_fus_bnk_datx_t;

/**
 * cvmx_mio_fus_dat0
 */
union cvmx_mio_fus_dat0
{
	uint64_t u64;
	struct cvmx_mio_fus_dat0_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t man_info                     : 32; /**< Fuse information - manufacturing info [31:0] */
#else
	uint64_t man_info                     : 32;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_mio_fus_dat0_s            cn30xx;
	struct cvmx_mio_fus_dat0_s            cn31xx;
	struct cvmx_mio_fus_dat0_s            cn38xx;
	struct cvmx_mio_fus_dat0_s            cn38xxp2;
	struct cvmx_mio_fus_dat0_s            cn50xx;
	struct cvmx_mio_fus_dat0_s            cn52xx;
	struct cvmx_mio_fus_dat0_s            cn52xxp1;
	struct cvmx_mio_fus_dat0_s            cn56xx;
	struct cvmx_mio_fus_dat0_s            cn56xxp1;
	struct cvmx_mio_fus_dat0_s            cn58xx;
	struct cvmx_mio_fus_dat0_s            cn58xxp1;
	struct cvmx_mio_fus_dat0_s            cn63xx;
	struct cvmx_mio_fus_dat0_s            cn63xxp1;
};
typedef union cvmx_mio_fus_dat0 cvmx_mio_fus_dat0_t;

/**
 * cvmx_mio_fus_dat1
 */
union cvmx_mio_fus_dat1
{
	uint64_t u64;
	struct cvmx_mio_fus_dat1_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t man_info                     : 32; /**< Fuse information - manufacturing info [63:32] */
#else
	uint64_t man_info                     : 32;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_mio_fus_dat1_s            cn30xx;
	struct cvmx_mio_fus_dat1_s            cn31xx;
	struct cvmx_mio_fus_dat1_s            cn38xx;
	struct cvmx_mio_fus_dat1_s            cn38xxp2;
	struct cvmx_mio_fus_dat1_s            cn50xx;
	struct cvmx_mio_fus_dat1_s            cn52xx;
	struct cvmx_mio_fus_dat1_s            cn52xxp1;
	struct cvmx_mio_fus_dat1_s            cn56xx;
	struct cvmx_mio_fus_dat1_s            cn56xxp1;
	struct cvmx_mio_fus_dat1_s            cn58xx;
	struct cvmx_mio_fus_dat1_s            cn58xxp1;
	struct cvmx_mio_fus_dat1_s            cn63xx;
	struct cvmx_mio_fus_dat1_s            cn63xxp1;
};
typedef union cvmx_mio_fus_dat1 cvmx_mio_fus_dat1_t;

/**
 * cvmx_mio_fus_dat2
 *
 * Notes:
 * CHIP_ID is consumed in several places within Octeon.
 *
 *    * Core COP0 ProcessorIdentification[Revision]
 *    * Core EJTAG DeviceIdentification[Version]
 *    * PCI_CFG02[RID]
 *    * JTAG controller
 *
 * Note: The JTAG controller gets CHIP_ID[3:0] solely from the laser fuses.
 * Modification to the efuses will not change what the JTAG controller reports
 * for CHIP_ID.
 */
union cvmx_mio_fus_dat2
{
	uint64_t u64;
	struct cvmx_mio_fus_dat2_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_35_63               : 29;
	uint64_t dorm_crypto                  : 1;  /**< Fuse information - Dormant Encryption enable */
	uint64_t fus318                       : 1;  /**< Fuse information - a copy of fuse318 */
	uint64_t raid_en                      : 1;  /**< Fuse information - RAID enabled */
	uint64_t reserved_30_31               : 2;
	uint64_t nokasu                       : 1;  /**< Fuse information - Disable Kasumi */
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable */
	uint64_t rst_sht                      : 1;  /**< Fuse information - When set, use short reset count */
	uint64_t bist_dis                     : 1;  /**< Fuse information - BIST Disable */
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t reserved_0_15                : 16;
#else
	uint64_t reserved_0_15                : 16;
	uint64_t chip_id                      : 8;
	uint64_t bist_dis                     : 1;
	uint64_t rst_sht                      : 1;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t nokasu                       : 1;
	uint64_t reserved_30_31               : 2;
	uint64_t raid_en                      : 1;
	uint64_t fus318                       : 1;
	uint64_t dorm_crypto                  : 1;
	uint64_t reserved_35_63               : 29;
#endif
	} s;
	struct cvmx_mio_fus_dat2_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_29_63               : 35;
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable */
	uint64_t rst_sht                      : 1;  /**< Fuse information - When set, use short reset count */
	uint64_t bist_dis                     : 1;  /**< Fuse information - BIST Disable */
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t pll_off                      : 4;  /**< Fuse information - core pll offset
                                                         Used to compute the base offset for the core pll.
                                                         the offset will be (PLL_OFF ^ 8)
                                                         Note, these fuses can only be set from laser fuse */
	uint64_t reserved_1_11                : 11;
	uint64_t pp_dis                       : 1;  /**< Fuse information - PP_DISABLES */
#else
	uint64_t pp_dis                       : 1;
	uint64_t reserved_1_11                : 11;
	uint64_t pll_off                      : 4;
	uint64_t chip_id                      : 8;
	uint64_t bist_dis                     : 1;
	uint64_t rst_sht                      : 1;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t reserved_29_63               : 35;
#endif
	} cn30xx;
	struct cvmx_mio_fus_dat2_cn31xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_29_63               : 35;
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable */
	uint64_t rst_sht                      : 1;  /**< Fuse information - When set, use short reset count */
	uint64_t bist_dis                     : 1;  /**< Fuse information - BIST Disable */
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t pll_off                      : 4;  /**< Fuse information - core pll offset
                                                         Used to compute the base offset for the core pll.
                                                         the offset will be (PLL_OFF ^ 8)
                                                         Note, these fuses can only be set from laser fuse */
	uint64_t reserved_2_11                : 10;
	uint64_t pp_dis                       : 2;  /**< Fuse information - PP_DISABLES */
#else
	uint64_t pp_dis                       : 2;
	uint64_t reserved_2_11                : 10;
	uint64_t pll_off                      : 4;
	uint64_t chip_id                      : 8;
	uint64_t bist_dis                     : 1;
	uint64_t rst_sht                      : 1;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t reserved_29_63               : 35;
#endif
	} cn31xx;
	struct cvmx_mio_fus_dat2_cn38xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_29_63               : 35;
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2)
                                                         (PASS2 Only) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable
                                                         (PASS2 Only) */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable
                                                         (PASS2 Only) */
	uint64_t rst_sht                      : 1;  /**< Fuse information - When set, use short reset count */
	uint64_t bist_dis                     : 1;  /**< Fuse information - BIST Disable */
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t pp_dis                       : 16; /**< Fuse information - PP_DISABLES */
#else
	uint64_t pp_dis                       : 16;
	uint64_t chip_id                      : 8;
	uint64_t bist_dis                     : 1;
	uint64_t rst_sht                      : 1;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t reserved_29_63               : 35;
#endif
	} cn38xx;
	struct cvmx_mio_fus_dat2_cn38xx       cn38xxp2;
	struct cvmx_mio_fus_dat2_cn50xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_34_63               : 30;
	uint64_t fus318                       : 1;  /**< Fuse information - a copy of fuse318 */
	uint64_t raid_en                      : 1;  /**< Fuse information - RAID enabled
                                                         (5020 does not have RAID co-processor) */
	uint64_t reserved_30_31               : 2;
	uint64_t nokasu                       : 1;  /**< Fuse information - Disable Kasumi */
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2)
                                                         (5020 does not have DFA co-processor) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable */
	uint64_t rst_sht                      : 1;  /**< Fuse information - When set, use short reset count */
	uint64_t bist_dis                     : 1;  /**< Fuse information - BIST Disable */
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t reserved_2_15                : 14;
	uint64_t pp_dis                       : 2;  /**< Fuse information - PP_DISABLES */
#else
	uint64_t pp_dis                       : 2;
	uint64_t reserved_2_15                : 14;
	uint64_t chip_id                      : 8;
	uint64_t bist_dis                     : 1;
	uint64_t rst_sht                      : 1;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t nokasu                       : 1;
	uint64_t reserved_30_31               : 2;
	uint64_t raid_en                      : 1;
	uint64_t fus318                       : 1;
	uint64_t reserved_34_63               : 30;
#endif
	} cn50xx;
	struct cvmx_mio_fus_dat2_cn52xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_34_63               : 30;
	uint64_t fus318                       : 1;  /**< Fuse information - a copy of fuse318 */
	uint64_t raid_en                      : 1;  /**< Fuse information - RAID enabled */
	uint64_t reserved_30_31               : 2;
	uint64_t nokasu                       : 1;  /**< Fuse information - Disable Kasumi */
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable */
	uint64_t rst_sht                      : 1;  /**< Fuse information - When set, use short reset count */
	uint64_t bist_dis                     : 1;  /**< Fuse information - BIST Disable */
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t reserved_4_15                : 12;
	uint64_t pp_dis                       : 4;  /**< Fuse information - PP_DISABLES */
#else
	uint64_t pp_dis                       : 4;
	uint64_t reserved_4_15                : 12;
	uint64_t chip_id                      : 8;
	uint64_t bist_dis                     : 1;
	uint64_t rst_sht                      : 1;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t nokasu                       : 1;
	uint64_t reserved_30_31               : 2;
	uint64_t raid_en                      : 1;
	uint64_t fus318                       : 1;
	uint64_t reserved_34_63               : 30;
#endif
	} cn52xx;
	struct cvmx_mio_fus_dat2_cn52xx       cn52xxp1;
	struct cvmx_mio_fus_dat2_cn56xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_34_63               : 30;
	uint64_t fus318                       : 1;  /**< Fuse information - a copy of fuse318 */
	uint64_t raid_en                      : 1;  /**< Fuse information - RAID enabled */
	uint64_t reserved_30_31               : 2;
	uint64_t nokasu                       : 1;  /**< Fuse information - Disable Kasumi */
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable */
	uint64_t rst_sht                      : 1;  /**< Fuse information - When set, use short reset count */
	uint64_t bist_dis                     : 1;  /**< Fuse information - BIST Disable */
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t reserved_12_15               : 4;
	uint64_t pp_dis                       : 12; /**< Fuse information - PP_DISABLES */
#else
	uint64_t pp_dis                       : 12;
	uint64_t reserved_12_15               : 4;
	uint64_t chip_id                      : 8;
	uint64_t bist_dis                     : 1;
	uint64_t rst_sht                      : 1;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t nokasu                       : 1;
	uint64_t reserved_30_31               : 2;
	uint64_t raid_en                      : 1;
	uint64_t fus318                       : 1;
	uint64_t reserved_34_63               : 30;
#endif
	} cn56xx;
	struct cvmx_mio_fus_dat2_cn56xx       cn56xxp1;
	struct cvmx_mio_fus_dat2_cn58xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_30_63               : 34;
	uint64_t nokasu                       : 1;  /**< Fuse information - Disable Kasumi */
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable */
	uint64_t rst_sht                      : 1;  /**< Fuse information - When set, use short reset count */
	uint64_t bist_dis                     : 1;  /**< Fuse information - BIST Disable */
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t pp_dis                       : 16; /**< Fuse information - PP_DISABLES */
#else
	uint64_t pp_dis                       : 16;
	uint64_t chip_id                      : 8;
	uint64_t bist_dis                     : 1;
	uint64_t rst_sht                      : 1;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t nokasu                       : 1;
	uint64_t reserved_30_63               : 34;
#endif
	} cn58xx;
	struct cvmx_mio_fus_dat2_cn58xx       cn58xxp1;
	struct cvmx_mio_fus_dat2_cn63xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_35_63               : 29;
	uint64_t dorm_crypto                  : 1;  /**< Fuse information - Dormant Encryption enable */
	uint64_t fus318                       : 1;  /**< Fuse information - a copy of fuse318 */
	uint64_t raid_en                      : 1;  /**< Fuse information - RAID enabled */
	uint64_t reserved_29_31               : 3;
	uint64_t nodfa_cp2                    : 1;  /**< Fuse information - DFA Disable (CP2) */
	uint64_t nomul                        : 1;  /**< Fuse information - VMUL disable */
	uint64_t nocrypto                     : 1;  /**< Fuse information - AES/DES/HASH disable */
	uint64_t reserved_24_25               : 2;
	uint64_t chip_id                      : 8;  /**< Fuse information - CHIP_ID */
	uint64_t reserved_6_15                : 10;
	uint64_t pp_dis                       : 6;  /**< Fuse information - PP_DISABLES */
#else
	uint64_t pp_dis                       : 6;
	uint64_t reserved_6_15                : 10;
	uint64_t chip_id                      : 8;
	uint64_t reserved_24_25               : 2;
	uint64_t nocrypto                     : 1;
	uint64_t nomul                        : 1;
	uint64_t nodfa_cp2                    : 1;
	uint64_t reserved_29_31               : 3;
	uint64_t raid_en                      : 1;
	uint64_t fus318                       : 1;
	uint64_t dorm_crypto                  : 1;
	uint64_t reserved_35_63               : 29;
#endif
	} cn63xx;
	struct cvmx_mio_fus_dat2_cn63xx       cn63xxp1;
};
typedef union cvmx_mio_fus_dat2 cvmx_mio_fus_dat2_t;

/**
 * cvmx_mio_fus_dat3
 */
union cvmx_mio_fus_dat3
{
	uint64_t u64;
	struct cvmx_mio_fus_dat3_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_58_63               : 6;
	uint64_t pll_ctl                      : 10; /**< Fuse information - PLL control */
	uint64_t dfa_info_dte                 : 3;  /**< Fuse information - DFA information (DTE) */
	uint64_t dfa_info_clm                 : 4;  /**< Fuse information - DFA information (Cluster mask) */
	uint64_t reserved_40_40               : 1;
	uint64_t ema                          : 2;  /**< Fuse information - EMA */
	uint64_t efus_lck_rsv                 : 1;  /**< Fuse information - efuse lockdown */
	uint64_t efus_lck_man                 : 1;  /**< Fuse information - efuse lockdown */
	uint64_t pll_half_dis                 : 1;  /**< Fuse information - RCLK PLL control */
	uint64_t l2c_crip                     : 3;  /**< Fuse information - L2C Cripple (1/8, 1/4, 1/2) */
	uint64_t pll_div4                     : 1;  /**< Fuse information - PLL DIV4 mode
                                                         (laser fuse only) */
	uint64_t reserved_29_30               : 2;
	uint64_t bar2_en                      : 1;  /**< Fuse information - BAR2 Present (when blown '1') */
	uint64_t efus_lck                     : 1;  /**< Fuse information - efuse lockdown */
	uint64_t efus_ign                     : 1;  /**< Fuse information - efuse ignore */
	uint64_t nozip                        : 1;  /**< Fuse information - ZIP disable */
	uint64_t nodfa_dte                    : 1;  /**< Fuse information - DFA Disable (DTE) */
	uint64_t icache                       : 24; /**< Fuse information - ICACHE Hard Repair Data */
#else
	uint64_t icache                       : 24;
	uint64_t nodfa_dte                    : 1;
	uint64_t nozip                        : 1;
	uint64_t efus_ign                     : 1;
	uint64_t efus_lck                     : 1;
	uint64_t bar2_en                      : 1;
	uint64_t reserved_29_30               : 2;
	uint64_t pll_div4                     : 1;
	uint64_t l2c_crip                     : 3;
	uint64_t pll_half_dis                 : 1;
	uint64_t efus_lck_man                 : 1;
	uint64_t efus_lck_rsv                 : 1;
	uint64_t ema                          : 2;
	uint64_t reserved_40_40               : 1;
	uint64_t dfa_info_clm                 : 4;
	uint64_t dfa_info_dte                 : 3;
	uint64_t pll_ctl                      : 10;
	uint64_t reserved_58_63               : 6;
#endif
	} s;
	struct cvmx_mio_fus_dat3_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t pll_div4                     : 1;  /**< Fuse information - PLL DIV4 mode
                                                         (laser fuse only) */
	uint64_t reserved_29_30               : 2;
	uint64_t bar2_en                      : 1;  /**< Fuse information - BAR2 Enable (when blown '1') */
	uint64_t efus_lck                     : 1;  /**< Fuse information - efuse lockdown */
	uint64_t efus_ign                     : 1;  /**< Fuse information - efuse ignore
                                                         This bit only has side effects when blown in
                                                         the laser fuses.  It is ignore if only set in
                                                         efuse store. */
	uint64_t nozip                        : 1;  /**< Fuse information - ZIP disable */
	uint64_t nodfa_dte                    : 1;  /**< Fuse information - DFA Disable (DTE) */
	uint64_t icache                       : 24; /**< Fuse information - ICACHE Hard Repair Data */
#else
	uint64_t icache                       : 24;
	uint64_t nodfa_dte                    : 1;
	uint64_t nozip                        : 1;
	uint64_t efus_ign                     : 1;
	uint64_t efus_lck                     : 1;
	uint64_t bar2_en                      : 1;
	uint64_t reserved_29_30               : 2;
	uint64_t pll_div4                     : 1;
	uint64_t reserved_32_63               : 32;
#endif
	} cn30xx;
	struct cvmx_mio_fus_dat3_cn31xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t pll_div4                     : 1;  /**< Fuse information - PLL DIV4 mode
                                                         (laser fuse only) */
	uint64_t zip_crip                     : 2;  /**< Fuse information - Zip Cripple
                                                         (O2P Only) */
	uint64_t bar2_en                      : 1;  /**< Fuse information - BAR2 Enable (when blown '1') */
	uint64_t efus_lck                     : 1;  /**< Fuse information - efuse lockdown */
	uint64_t efus_ign                     : 1;  /**< Fuse information - efuse ignore
                                                         This bit only has side effects when blown in
                                                         the laser fuses.  It is ignore if only set in
                                                         efuse store. */
	uint64_t nozip                        : 1;  /**< Fuse information - ZIP disable */
	uint64_t nodfa_dte                    : 1;  /**< Fuse information - DFA Disable (DTE) */
	uint64_t icache                       : 24; /**< Fuse information - ICACHE Hard Repair Data */
#else
	uint64_t icache                       : 24;
	uint64_t nodfa_dte                    : 1;
	uint64_t nozip                        : 1;
	uint64_t efus_ign                     : 1;
	uint64_t efus_lck                     : 1;
	uint64_t bar2_en                      : 1;
	uint64_t zip_crip                     : 2;
	uint64_t pll_div4                     : 1;
	uint64_t reserved_32_63               : 32;
#endif
	} cn31xx;
	struct cvmx_mio_fus_dat3_cn38xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_31_63               : 33;
	uint64_t zip_crip                     : 2;  /**< Fuse information - Zip Cripple
                                                         (PASS3 Only) */
	uint64_t bar2_en                      : 1;  /**< Fuse information - BAR2 Enable (when blown '1')
                                                         (PASS2 Only) */
	uint64_t efus_lck                     : 1;  /**< Fuse information - efuse lockdown
                                                         (PASS2 Only) */
	uint64_t efus_ign                     : 1;  /**< Fuse information - efuse ignore
                                                         This bit only has side effects when blown in
                                                         the laser fuses.  It is ignore if only set in
                                                         efuse store.
                                                         (PASS2 Only) */
	uint64_t nozip                        : 1;  /**< Fuse information - ZIP disable
                                                         (PASS2 Only) */
	uint64_t nodfa_dte                    : 1;  /**< Fuse information - DFA Disable (DTE)
                                                         (PASS2 Only) */
	uint64_t icache                       : 24; /**< Fuse information - ICACHE Hard Repair Data */
#else
	uint64_t icache                       : 24;
	uint64_t nodfa_dte                    : 1;
	uint64_t nozip                        : 1;
	uint64_t efus_ign                     : 1;
	uint64_t efus_lck                     : 1;
	uint64_t bar2_en                      : 1;
	uint64_t zip_crip                     : 2;
	uint64_t reserved_31_63               : 33;
#endif
	} cn38xx;
	struct cvmx_mio_fus_dat3_cn38xxp2
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_29_63               : 35;
	uint64_t bar2_en                      : 1;  /**< Fuse information - BAR2 Enable (when blown '1')
                                                         (PASS2 Only) */
	uint64_t efus_lck                     : 1;  /**< Fuse information - efuse lockdown
                                                         (PASS2 Only) */
	uint64_t efus_ign                     : 1;  /**< Fuse information - efuse ignore
                                                         This bit only has side effects when blown in
                                                         the laser fuses.  It is ignore if only set in
                                                         efuse store.
                                                         (PASS2 Only) */
	uint64_t nozip                        : 1;  /**< Fuse information - ZIP disable
                                                         (PASS2 Only) */
	uint64_t nodfa_dte                    : 1;  /**< Fuse information - DFA Disable (DTE)
                                                         (PASS2 Only) */
	uint64_t icache                       : 24; /**< Fuse information - ICACHE Hard Repair Data */
#else
	uint64_t icache                       : 24;
	uint64_t nodfa_dte                    : 1;
	uint64_t nozip                        : 1;
	uint64_t efus_ign                     : 1;
	uint64_t efus_lck                     : 1;
	uint64_t bar2_en                      : 1;
	uint64_t reserved_29_63               : 35;
#endif
	} cn38xxp2;
	struct cvmx_mio_fus_dat3_cn38xx       cn50xx;
	struct cvmx_mio_fus_dat3_cn38xx       cn52xx;
	struct cvmx_mio_fus_dat3_cn38xx       cn52xxp1;
	struct cvmx_mio_fus_dat3_cn38xx       cn56xx;
	struct cvmx_mio_fus_dat3_cn38xx       cn56xxp1;
	struct cvmx_mio_fus_dat3_cn38xx       cn58xx;
	struct cvmx_mio_fus_dat3_cn38xx       cn58xxp1;
	struct cvmx_mio_fus_dat3_cn63xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_58_63               : 6;
	uint64_t pll_ctl                      : 10; /**< Fuse information - PLL control */
	uint64_t dfa_info_dte                 : 3;  /**< Fuse information - DFA information (DTE) */
	uint64_t dfa_info_clm                 : 4;  /**< Fuse information - DFA information (Cluster mask) */
	uint64_t reserved_40_40               : 1;
	uint64_t ema                          : 2;  /**< Fuse information - EMA */
	uint64_t efus_lck_rsv                 : 1;  /**< Fuse information - efuse lockdown */
	uint64_t efus_lck_man                 : 1;  /**< Fuse information - efuse lockdown */
	uint64_t pll_half_dis                 : 1;  /**< Fuse information - RCLK PLL control */
	uint64_t l2c_crip                     : 3;  /**< Fuse information - L2C Cripple (1/8, 1/4, 1/2) */
	uint64_t reserved_31_31               : 1;
	uint64_t zip_info                     : 2;  /**< Fuse information - Zip information */
	uint64_t bar2_en                      : 1;  /**< Fuse information - BAR2 Present (when blown '1') */
	uint64_t efus_lck                     : 1;  /**< Fuse information - efuse lockdown */
	uint64_t efus_ign                     : 1;  /**< Fuse information - efuse ignore */
	uint64_t nozip                        : 1;  /**< Fuse information - ZIP disable */
	uint64_t nodfa_dte                    : 1;  /**< Fuse information - DFA Disable (DTE) */
	uint64_t reserved_0_23                : 24;
#else
	uint64_t reserved_0_23                : 24;
	uint64_t nodfa_dte                    : 1;
	uint64_t nozip                        : 1;
	uint64_t efus_ign                     : 1;
	uint64_t efus_lck                     : 1;
	uint64_t bar2_en                      : 1;
	uint64_t zip_info                     : 2;
	uint64_t reserved_31_31               : 1;
	uint64_t l2c_crip                     : 3;
	uint64_t pll_half_dis                 : 1;
	uint64_t efus_lck_man                 : 1;
	uint64_t efus_lck_rsv                 : 1;
	uint64_t ema                          : 2;
	uint64_t reserved_40_40               : 1;
	uint64_t dfa_info_clm                 : 4;
	uint64_t dfa_info_dte                 : 3;
	uint64_t pll_ctl                      : 10;
	uint64_t reserved_58_63               : 6;
#endif
	} cn63xx;
	struct cvmx_mio_fus_dat3_cn63xx       cn63xxp1;
};
typedef union cvmx_mio_fus_dat3 cvmx_mio_fus_dat3_t;

/**
 * cvmx_mio_fus_ema
 */
union cvmx_mio_fus_ema
{
	uint64_t u64;
	struct cvmx_mio_fus_ema_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_7_63                : 57;
	uint64_t eff_ema                      : 3;  /**< Reserved */
	uint64_t reserved_3_3                 : 1;
	uint64_t ema                          : 3;  /**< Reserved */
#else
	uint64_t ema                          : 3;
	uint64_t reserved_3_3                 : 1;
	uint64_t eff_ema                      : 3;
	uint64_t reserved_7_63                : 57;
#endif
	} s;
	struct cvmx_mio_fus_ema_s             cn50xx;
	struct cvmx_mio_fus_ema_s             cn52xx;
	struct cvmx_mio_fus_ema_s             cn52xxp1;
	struct cvmx_mio_fus_ema_s             cn56xx;
	struct cvmx_mio_fus_ema_s             cn56xxp1;
	struct cvmx_mio_fus_ema_cn58xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t ema                          : 2;  /**< EMA Settings */
#else
	uint64_t ema                          : 2;
	uint64_t reserved_2_63                : 62;
#endif
	} cn58xx;
	struct cvmx_mio_fus_ema_cn58xx        cn58xxp1;
	struct cvmx_mio_fus_ema_s             cn63xx;
	struct cvmx_mio_fus_ema_s             cn63xxp1;
};
typedef union cvmx_mio_fus_ema cvmx_mio_fus_ema_t;

/**
 * cvmx_mio_fus_pdf
 */
union cvmx_mio_fus_pdf
{
	uint64_t u64;
	struct cvmx_mio_fus_pdf_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t pdf                          : 64; /**< Fuse information - Product Definition Field */
#else
	uint64_t pdf                          : 64;
#endif
	} s;
	struct cvmx_mio_fus_pdf_s             cn50xx;
	struct cvmx_mio_fus_pdf_s             cn52xx;
	struct cvmx_mio_fus_pdf_s             cn52xxp1;
	struct cvmx_mio_fus_pdf_s             cn56xx;
	struct cvmx_mio_fus_pdf_s             cn56xxp1;
	struct cvmx_mio_fus_pdf_s             cn58xx;
	struct cvmx_mio_fus_pdf_s             cn63xx;
	struct cvmx_mio_fus_pdf_s             cn63xxp1;
};
typedef union cvmx_mio_fus_pdf cvmx_mio_fus_pdf_t;

/**
 * cvmx_mio_fus_pll
 *
 * Notes:
 * The core clkout postscaler should be placed in reset at least 10 ref clocks prior to changing
 * the core clkout select.  The core clkout postscaler should remain under reset for at least 10
 * ref clocks after the core clkout select changes.
 *
 * The pnr clkout postscaler should be placed in reset at least 10 ref clocks prior to changing
 * the pnr clkout select.  The pnr clkout postscaler should remain under reset for at least 10
 * ref clocks after the pnr clkout select changes.
 */
union cvmx_mio_fus_pll
{
	uint64_t u64;
	struct cvmx_mio_fus_pll_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t c_cout_rst                   : 1;  /**< Core clkout postscaler reset */
	uint64_t c_cout_sel                   : 2;  /**< Core clkout select
                                                         (0=RCLK,1=PS output,2=PLL output, 3=GND)         |   $PR */
	uint64_t pnr_cout_rst                 : 1;  /**< PNR  clkout postscaler reset */
	uint64_t pnr_cout_sel                 : 2;  /**< PNR  clkout select
                                                         (0=SCLK,1=PS output,2=PLL output, 3=GND)         |   $PR */
	uint64_t rfslip                       : 1;  /**< Reserved */
	uint64_t fbslip                       : 1;  /**< Reserved */
#else
	uint64_t fbslip                       : 1;
	uint64_t rfslip                       : 1;
	uint64_t pnr_cout_sel                 : 2;
	uint64_t pnr_cout_rst                 : 1;
	uint64_t c_cout_sel                   : 2;
	uint64_t c_cout_rst                   : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_fus_pll_cn50xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t rfslip                       : 1;  /**< PLL reference clock slip */
	uint64_t fbslip                       : 1;  /**< PLL feedback clock slip */
#else
	uint64_t fbslip                       : 1;
	uint64_t rfslip                       : 1;
	uint64_t reserved_2_63                : 62;
#endif
	} cn50xx;
	struct cvmx_mio_fus_pll_cn50xx        cn52xx;
	struct cvmx_mio_fus_pll_cn50xx        cn52xxp1;
	struct cvmx_mio_fus_pll_cn50xx        cn56xx;
	struct cvmx_mio_fus_pll_cn50xx        cn56xxp1;
	struct cvmx_mio_fus_pll_cn50xx        cn58xx;
	struct cvmx_mio_fus_pll_cn50xx        cn58xxp1;
	struct cvmx_mio_fus_pll_s             cn63xx;
	struct cvmx_mio_fus_pll_s             cn63xxp1;
};
typedef union cvmx_mio_fus_pll cvmx_mio_fus_pll_t;

/**
 * cvmx_mio_fus_prog
 *
 * DON'T PUT IN HRM*
 *
 *
 * Notes:
 * This CSR is not present in the HRM.
 *
 * To write a bank of fuses, SW must set MIO_FUS_WADR[ADDR] to the bank to be
 * programmed and then set each bit within MIO_FUS_BNK_DATX to indicate which
 * fuses to blow.  Once ADDR, and DAT are setup, SW can write to
 * MIO_FUS_PROG[PROG] to start the bank write and poll on PROG.  Once PROG is
 * clear, the bank write is complete.
 *
 * A soft blow is still subject to lockdown fuses.  After a soft/warm reset, the
 * chip will behave as though the fuses were actually blown.  A cold reset restores
 * the actual fuse valuse.
 */
union cvmx_mio_fus_prog
{
	uint64_t u64;
	struct cvmx_mio_fus_prog_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t soft                         : 1;  /**< When set with PROG, causes only the local storeage
                                                         to change.  Will not really blow any fuses.  HW
                                                         will clear when the program operation is complete */
	uint64_t prog                         : 1;  /**< Blow the fuse bank
                                                         SW will set PROG, and then the HW will clear
                                                         when the program operation is complete */
#else
	uint64_t prog                         : 1;
	uint64_t soft                         : 1;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_fus_prog_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t prog                         : 1;  /**< Blow the fuse
                                                         SW will set PROG, hold it for 10us, then clear it */
#else
	uint64_t prog                         : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} cn30xx;
	struct cvmx_mio_fus_prog_cn30xx       cn31xx;
	struct cvmx_mio_fus_prog_cn30xx       cn38xx;
	struct cvmx_mio_fus_prog_cn30xx       cn38xxp2;
	struct cvmx_mio_fus_prog_cn30xx       cn50xx;
	struct cvmx_mio_fus_prog_cn30xx       cn52xx;
	struct cvmx_mio_fus_prog_cn30xx       cn52xxp1;
	struct cvmx_mio_fus_prog_cn30xx       cn56xx;
	struct cvmx_mio_fus_prog_cn30xx       cn56xxp1;
	struct cvmx_mio_fus_prog_cn30xx       cn58xx;
	struct cvmx_mio_fus_prog_cn30xx       cn58xxp1;
	struct cvmx_mio_fus_prog_s            cn63xx;
	struct cvmx_mio_fus_prog_s            cn63xxp1;
};
typedef union cvmx_mio_fus_prog cvmx_mio_fus_prog_t;

/**
 * cvmx_mio_fus_prog_times
 *
 * DON'T PUT IN HRM*
 *
 *
 * Notes:
 * This CSR is not present in the HRM.
 *
 * All values must be > 0 for correct electrical operation.
 *
 * IFB fuses are 0..1791
 * L6G fuses are 1792 to 2047
 *
 * The reset values are for IFB fuses for ref_clk of 100MHZ
 */
union cvmx_mio_fus_prog_times
{
	uint64_t u64;
	struct cvmx_mio_fus_prog_times_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_35_63               : 29;
	uint64_t vgate_pin                    : 1;  /**< efuse vgate pin (L6G) */
	uint64_t fsrc_pin                     : 1;  /**< efuse fsource pin (L6G) */
	uint64_t prog_pin                     : 1;  /**< efuse program pin (IFB) */
	uint64_t reserved_6_31                : 26;
	uint64_t setup                        : 6;  /**< efuse timing param

                                                         SETUP = (tWRS/refclk period)-1

                                                         For IFB: tWRS =  20ns
                                                         For L6G: tWRS =  20ns */
#else
	uint64_t setup                        : 6;
	uint64_t reserved_6_31                : 26;
	uint64_t prog_pin                     : 1;
	uint64_t fsrc_pin                     : 1;
	uint64_t vgate_pin                    : 1;
	uint64_t reserved_35_63               : 29;
#endif
	} s;
	struct cvmx_mio_fus_prog_times_cn50xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_33_63               : 31;
	uint64_t prog_pin                     : 1;  /**< efuse program pin */
	uint64_t out                          : 8;  /**< efuse timing param (ref_clks to delay 10ns) */
	uint64_t sclk_lo                      : 4;  /**< efuse timing param (ref_clks to delay 5ns) */
	uint64_t sclk_hi                      : 12; /**< efuse timing param (ref_clks to delay 1000ns) */
	uint64_t setup                        : 8;  /**< efuse timing param (ref_clks to delay 10ns) */
#else
	uint64_t setup                        : 8;
	uint64_t sclk_hi                      : 12;
	uint64_t sclk_lo                      : 4;
	uint64_t out                          : 8;
	uint64_t prog_pin                     : 1;
	uint64_t reserved_33_63               : 31;
#endif
	} cn50xx;
	struct cvmx_mio_fus_prog_times_cn50xx cn52xx;
	struct cvmx_mio_fus_prog_times_cn50xx cn52xxp1;
	struct cvmx_mio_fus_prog_times_cn50xx cn56xx;
	struct cvmx_mio_fus_prog_times_cn50xx cn56xxp1;
	struct cvmx_mio_fus_prog_times_cn50xx cn58xx;
	struct cvmx_mio_fus_prog_times_cn50xx cn58xxp1;
	struct cvmx_mio_fus_prog_times_cn63xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_35_63               : 29;
	uint64_t vgate_pin                    : 1;  /**< efuse vgate pin (L6G) */
	uint64_t fsrc_pin                     : 1;  /**< efuse fsource pin (L6G) */
	uint64_t prog_pin                     : 1;  /**< efuse program pin (IFB) */
	uint64_t out                          : 7;  /**< efuse timing param

                                                         OUT = (tOUT/refclk period)-1

                                                         For IFB: tOUT =  20ns
                                                         For L6G: tOUT =  20ns */
	uint64_t sclk_lo                      : 4;  /**< efuse timing param

                                                         SCLK_LO=(tSLO/refclk period)-1

                                                         For IFB: tSLO =  20ns
                                                         For L6G: tSLO =  20ns */
	uint64_t sclk_hi                      : 15; /**< efuse timing param
                                                         ***NOTE: Pass 1.x reset value is 20000

                                                         SCLK_HI=(tSHI/refclk period)-1

                                                         For IFB: tSHI =  200us
                                                         For L6G: tSHI =  25us */
	uint64_t setup                        : 6;  /**< efuse timing param

                                                         SETUP = (tWRS/refclk period)-1

                                                         For IFB: tWRS =  20ns
                                                         For L6G: tWRS =  20ns */
#else
	uint64_t setup                        : 6;
	uint64_t sclk_hi                      : 15;
	uint64_t sclk_lo                      : 4;
	uint64_t out                          : 7;
	uint64_t prog_pin                     : 1;
	uint64_t fsrc_pin                     : 1;
	uint64_t vgate_pin                    : 1;
	uint64_t reserved_35_63               : 29;
#endif
	} cn63xx;
	struct cvmx_mio_fus_prog_times_cn63xx cn63xxp1;
};
typedef union cvmx_mio_fus_prog_times cvmx_mio_fus_prog_times_t;

/**
 * cvmx_mio_fus_rcmd
 *
 * Notes:
 * To read an efuse, SW writes MIO_FUS_RCMD[ADDR,PEND] with the byte address of
 * the fuse in question, then SW can poll MIO_FUS_RCMD[PEND].  When PEND is
 * clear, then MIO_FUS_RCMD[DAT] is valid.  In addition, if the efuse read went
 * to the efuse banks (eg. ((ADDR/16) not [0,1,7]) || EFUSE) SW can read
 * MIO_FUS_BNK_DATX which contains all 128 fuses in the bank associated in
 * ADDR.
 */
union cvmx_mio_fus_rcmd
{
	uint64_t u64;
	struct cvmx_mio_fus_rcmd_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t dat                          : 8;  /**< 8bits of fuse data */
	uint64_t reserved_13_15               : 3;
	uint64_t pend                         : 1;  /**< SW sets this bit on a write to start FUSE read
                                                         operation.  HW clears when read is complete and
                                                         the DAT is valid */
	uint64_t reserved_9_11                : 3;
	uint64_t efuse                        : 1;  /**< When set, return data from the efuse storage
                                                         rather than the local storage */
	uint64_t addr                         : 8;  /**< The byte address of the fuse to read */
#else
	uint64_t addr                         : 8;
	uint64_t efuse                        : 1;
	uint64_t reserved_9_11                : 3;
	uint64_t pend                         : 1;
	uint64_t reserved_13_15               : 3;
	uint64_t dat                          : 8;
	uint64_t reserved_24_63               : 40;
#endif
	} s;
	struct cvmx_mio_fus_rcmd_cn30xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t dat                          : 8;  /**< 8bits of fuse data */
	uint64_t reserved_13_15               : 3;
	uint64_t pend                         : 1;  /**< SW sets this bit on a write to start FUSE read
                                                         operation.  HW clears when read is complete and
                                                         the DAT is valid */
	uint64_t reserved_9_11                : 3;
	uint64_t efuse                        : 1;  /**< When set, return data from the efuse storage
                                                         rather than the local storage for the 320 HW fuses */
	uint64_t reserved_7_7                 : 1;
	uint64_t addr                         : 7;  /**< The byte address of the fuse to read */
#else
	uint64_t addr                         : 7;
	uint64_t reserved_7_7                 : 1;
	uint64_t efuse                        : 1;
	uint64_t reserved_9_11                : 3;
	uint64_t pend                         : 1;
	uint64_t reserved_13_15               : 3;
	uint64_t dat                          : 8;
	uint64_t reserved_24_63               : 40;
#endif
	} cn30xx;
	struct cvmx_mio_fus_rcmd_cn30xx       cn31xx;
	struct cvmx_mio_fus_rcmd_cn30xx       cn38xx;
	struct cvmx_mio_fus_rcmd_cn30xx       cn38xxp2;
	struct cvmx_mio_fus_rcmd_cn30xx       cn50xx;
	struct cvmx_mio_fus_rcmd_s            cn52xx;
	struct cvmx_mio_fus_rcmd_s            cn52xxp1;
	struct cvmx_mio_fus_rcmd_s            cn56xx;
	struct cvmx_mio_fus_rcmd_s            cn56xxp1;
	struct cvmx_mio_fus_rcmd_cn30xx       cn58xx;
	struct cvmx_mio_fus_rcmd_cn30xx       cn58xxp1;
	struct cvmx_mio_fus_rcmd_s            cn63xx;
	struct cvmx_mio_fus_rcmd_s            cn63xxp1;
};
typedef union cvmx_mio_fus_rcmd cvmx_mio_fus_rcmd_t;

/**
 * cvmx_mio_fus_read_times
 *
 * Notes:
 * IFB fuses are 0..1791
 * L6G fuses are 1792 to 2047
 *
 * The reset values are for IFB fuses for refclk up to 100MHZ when core PLL is enagaged
 * The reset values are for IFB fuses for refclk up to 500MHZ when core PLL is not enagaged
 *
 * If any of the formulas above result in a value less than zero, the corresponding
 * timing parameter should be set to zero.
 *
 * Prior to issuing a read to the fuse banks (via. MIO_FUS_RCMD), this register
 * should be written with the timing parameters which correspond to the fuse bank type (IFB vs L6G)
 * that will be read.
 *
 * This register should not be written while MIO_FUS_RCMD[PEND]=1.
 */
union cvmx_mio_fus_read_times
{
	uint64_t u64;
	struct cvmx_mio_fus_read_times_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_26_63               : 38;
	uint64_t sch                          : 4;  /**< Hold CS for (SCH+1) refclks after FSET desserts

                                                         SCH = (tSCH/refclk period)-1

                                                         For IFB: tSCH = 160ns
                                                         For L6G: tSCH =  10ns */
	uint64_t fsh                          : 4;  /**< Hold FSET for (FSH+1) refclks after PRCHG deasserts

                                                         FSH = (tFSH/refclk period)-1

                                                         For IFB: tFSH = 160ns
                                                         For L6G: tFSH =  10ns */
	uint64_t prh                          : 4;  /**< Assert PRCHG (PRH+1) refclks after SIGDEV deasserts

                                                         PRH = (tPRH/refclk period)-1

                                                         For IFB: tPRH =  70ns
                                                         For L6G: tPRH =  10ns */
	uint64_t sdh                          : 4;  /**< Hold SIGDEV for (SDH+1) refclks after FSET asserts

                                                         SDH = (tSDH/refclk period)-1

                                                         For IFB: tPRH =  10ns
                                                         For L6G: tPRH =  10ns */
	uint64_t setup                        : 10; /**< Assert CS for (SETUP+1) refclks before asserting
                                                         SIGDEV, FSET, or PRCHG

                                                         SETUP=(tRDS/refclk period)-1

                                                         For IFB: tRDS = 10000ns
                                                         For L6G: tRDS = max(tSCS,tSDS,tPRS)
                                                           where tSCS   = 10ns
                                                                 tSDS   = 10ns
                                                                 tPRS   = 10ns */
#else
	uint64_t setup                        : 10;
	uint64_t sdh                          : 4;
	uint64_t prh                          : 4;
	uint64_t fsh                          : 4;
	uint64_t sch                          : 4;
	uint64_t reserved_26_63               : 38;
#endif
	} s;
	struct cvmx_mio_fus_read_times_s      cn63xx;
	struct cvmx_mio_fus_read_times_s      cn63xxp1;
};
typedef union cvmx_mio_fus_read_times cvmx_mio_fus_read_times_t;

/**
 * cvmx_mio_fus_repair_res0
 */
union cvmx_mio_fus_repair_res0
{
	uint64_t u64;
	struct cvmx_mio_fus_repair_res0_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_55_63               : 9;
	uint64_t too_many                     : 1;  /**< Too many defects */
	uint64_t repair2                      : 18; /**< BISR Results */
	uint64_t repair1                      : 18; /**< BISR Results */
	uint64_t repair0                      : 18; /**< BISR Results */
#else
	uint64_t repair0                      : 18;
	uint64_t repair1                      : 18;
	uint64_t repair2                      : 18;
	uint64_t too_many                     : 1;
	uint64_t reserved_55_63               : 9;
#endif
	} s;
	struct cvmx_mio_fus_repair_res0_s     cn63xx;
	struct cvmx_mio_fus_repair_res0_s     cn63xxp1;
};
typedef union cvmx_mio_fus_repair_res0 cvmx_mio_fus_repair_res0_t;

/**
 * cvmx_mio_fus_repair_res1
 */
union cvmx_mio_fus_repair_res1
{
	uint64_t u64;
	struct cvmx_mio_fus_repair_res1_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_54_63               : 10;
	uint64_t repair5                      : 18; /**< BISR Results */
	uint64_t repair4                      : 18; /**< BISR Results */
	uint64_t repair3                      : 18; /**< BISR Results */
#else
	uint64_t repair3                      : 18;
	uint64_t repair4                      : 18;
	uint64_t repair5                      : 18;
	uint64_t reserved_54_63               : 10;
#endif
	} s;
	struct cvmx_mio_fus_repair_res1_s     cn63xx;
	struct cvmx_mio_fus_repair_res1_s     cn63xxp1;
};
typedef union cvmx_mio_fus_repair_res1 cvmx_mio_fus_repair_res1_t;

/**
 * cvmx_mio_fus_repair_res2
 */
union cvmx_mio_fus_repair_res2
{
	uint64_t u64;
	struct cvmx_mio_fus_repair_res2_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_18_63               : 46;
	uint64_t repair6                      : 18; /**< BISR Results */
#else
	uint64_t repair6                      : 18;
	uint64_t reserved_18_63               : 46;
#endif
	} s;
	struct cvmx_mio_fus_repair_res2_s     cn63xx;
	struct cvmx_mio_fus_repair_res2_s     cn63xxp1;
};
typedef union cvmx_mio_fus_repair_res2 cvmx_mio_fus_repair_res2_t;

/**
 * cvmx_mio_fus_spr_repair_res
 *
 * Notes:
 * Pass3 Only
 *
 */
union cvmx_mio_fus_spr_repair_res
{
	uint64_t u64;
	struct cvmx_mio_fus_spr_repair_res_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_42_63               : 22;
	uint64_t repair2                      : 14; /**< Reserved (see  MIO_FUS_REPAIR_RES*) */
	uint64_t repair1                      : 14; /**< Reserved (see  MIO_FUS_REPAIR_RES*) */
	uint64_t repair0                      : 14; /**< Reserved (see  MIO_FUS_REPAIR_RES*) */
#else
	uint64_t repair0                      : 14;
	uint64_t repair1                      : 14;
	uint64_t repair2                      : 14;
	uint64_t reserved_42_63               : 22;
#endif
	} s;
	struct cvmx_mio_fus_spr_repair_res_s  cn30xx;
	struct cvmx_mio_fus_spr_repair_res_s  cn31xx;
	struct cvmx_mio_fus_spr_repair_res_s  cn38xx;
	struct cvmx_mio_fus_spr_repair_res_s  cn50xx;
	struct cvmx_mio_fus_spr_repair_res_s  cn52xx;
	struct cvmx_mio_fus_spr_repair_res_s  cn52xxp1;
	struct cvmx_mio_fus_spr_repair_res_s  cn56xx;
	struct cvmx_mio_fus_spr_repair_res_s  cn56xxp1;
	struct cvmx_mio_fus_spr_repair_res_s  cn58xx;
	struct cvmx_mio_fus_spr_repair_res_s  cn58xxp1;
	struct cvmx_mio_fus_spr_repair_res_s  cn63xx;
	struct cvmx_mio_fus_spr_repair_res_s  cn63xxp1;
};
typedef union cvmx_mio_fus_spr_repair_res cvmx_mio_fus_spr_repair_res_t;

/**
 * cvmx_mio_fus_spr_repair_sum
 *
 * Notes:
 * Pass3 Only
 *
 */
union cvmx_mio_fus_spr_repair_sum
{
	uint64_t u64;
	struct cvmx_mio_fus_spr_repair_sum_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t too_many                     : 1;  /**< Reserved (see  MIO_FUS_REPAIR_RES*) */
#else
	uint64_t too_many                     : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_fus_spr_repair_sum_s  cn30xx;
	struct cvmx_mio_fus_spr_repair_sum_s  cn31xx;
	struct cvmx_mio_fus_spr_repair_sum_s  cn38xx;
	struct cvmx_mio_fus_spr_repair_sum_s  cn50xx;
	struct cvmx_mio_fus_spr_repair_sum_s  cn52xx;
	struct cvmx_mio_fus_spr_repair_sum_s  cn52xxp1;
	struct cvmx_mio_fus_spr_repair_sum_s  cn56xx;
	struct cvmx_mio_fus_spr_repair_sum_s  cn56xxp1;
	struct cvmx_mio_fus_spr_repair_sum_s  cn58xx;
	struct cvmx_mio_fus_spr_repair_sum_s  cn58xxp1;
	struct cvmx_mio_fus_spr_repair_sum_s  cn63xx;
	struct cvmx_mio_fus_spr_repair_sum_s  cn63xxp1;
};
typedef union cvmx_mio_fus_spr_repair_sum cvmx_mio_fus_spr_repair_sum_t;

/**
 * cvmx_mio_fus_unlock
 */
union cvmx_mio_fus_unlock
{
	uint64_t u64;
	struct cvmx_mio_fus_unlock_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t key                          : 24; /**< When set to the typical value, allows SW to
                                                         program the efuses */
#else
	uint64_t key                          : 24;
	uint64_t reserved_24_63               : 40;
#endif
	} s;
	struct cvmx_mio_fus_unlock_s          cn30xx;
	struct cvmx_mio_fus_unlock_s          cn31xx;
};
typedef union cvmx_mio_fus_unlock cvmx_mio_fus_unlock_t;

/**
 * cvmx_mio_fus_wadr
 */
union cvmx_mio_fus_wadr
{
	uint64_t u64;
	struct cvmx_mio_fus_wadr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_10_63               : 54;
	uint64_t addr                         : 10; /**< Which of the banks of 128 fuses to blow */
#else
	uint64_t addr                         : 10;
	uint64_t reserved_10_63               : 54;
#endif
	} s;
	struct cvmx_mio_fus_wadr_s            cn30xx;
	struct cvmx_mio_fus_wadr_s            cn31xx;
	struct cvmx_mio_fus_wadr_s            cn38xx;
	struct cvmx_mio_fus_wadr_s            cn38xxp2;
	struct cvmx_mio_fus_wadr_cn50xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t addr                         : 2;  /**< Which of the four banks of 256 fuses to blow */
#else
	uint64_t addr                         : 2;
	uint64_t reserved_2_63                : 62;
#endif
	} cn50xx;
	struct cvmx_mio_fus_wadr_cn52xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_3_63                : 61;
	uint64_t addr                         : 3;  /**< Which of the four banks of 256 fuses to blow */
#else
	uint64_t addr                         : 3;
	uint64_t reserved_3_63                : 61;
#endif
	} cn52xx;
	struct cvmx_mio_fus_wadr_cn52xx       cn52xxp1;
	struct cvmx_mio_fus_wadr_cn52xx       cn56xx;
	struct cvmx_mio_fus_wadr_cn52xx       cn56xxp1;
	struct cvmx_mio_fus_wadr_cn50xx       cn58xx;
	struct cvmx_mio_fus_wadr_cn50xx       cn58xxp1;
	struct cvmx_mio_fus_wadr_cn63xx
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_4_63                : 60;
	uint64_t addr                         : 4;  /**< Which of the banks of 128 fuses to blow */
#else
	uint64_t addr                         : 4;
	uint64_t reserved_4_63                : 60;
#endif
	} cn63xx;
	struct cvmx_mio_fus_wadr_cn63xx       cn63xxp1;
};
typedef union cvmx_mio_fus_wadr cvmx_mio_fus_wadr_t;

/**
 * cvmx_mio_gpio_comp
 *
 * MIO_GPIO_COMP = MIO GPIO Compensation Register
 *
 */
union cvmx_mio_gpio_comp
{
	uint64_t u64;
	struct cvmx_mio_gpio_comp_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_12_63               : 52;
	uint64_t pctl                         : 6;  /**< GPIO bus PCTL */
	uint64_t nctl                         : 6;  /**< GPIO bus NCTL */
#else
	uint64_t nctl                         : 6;
	uint64_t pctl                         : 6;
	uint64_t reserved_12_63               : 52;
#endif
	} s;
	struct cvmx_mio_gpio_comp_s           cn63xx;
	struct cvmx_mio_gpio_comp_s           cn63xxp1;
};
typedef union cvmx_mio_gpio_comp cvmx_mio_gpio_comp_t;

/**
 * cvmx_mio_ndf_dma_cfg
 *
 * MIO_NDF_DMA_CFG = MIO NAND Flash DMA Config Register
 *
 * SIZE is specified in number of 64 bit transfers (encoded in -1 notation).
 *
 * ADR must be 64 bit aligned.
 */
union cvmx_mio_ndf_dma_cfg
{
	uint64_t u64;
	struct cvmx_mio_ndf_dma_cfg_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t en                           : 1;  /**< DMA Engine enable */
	uint64_t rw                           : 1;  /**< DMA Engine R/W bit (0 = read, 1 = write) */
	uint64_t clr                          : 1;  /**< DMA Engine clear EN on device terminated burst */
	uint64_t reserved_60_60               : 1;
	uint64_t swap32                       : 1;  /**< DMA Engine 32 bit swap */
	uint64_t swap16                       : 1;  /**< DMA Engine 16 bit swap */
	uint64_t swap8                        : 1;  /**< DMA Engine 8 bit swap */
	uint64_t endian                       : 1;  /**< DMA Engine NCB endian mode (0 = big, 1 = little) */
	uint64_t size                         : 20; /**< DMA Engine size */
	uint64_t adr                          : 36; /**< DMA Engine address */
#else
	uint64_t adr                          : 36;
	uint64_t size                         : 20;
	uint64_t endian                       : 1;
	uint64_t swap8                        : 1;
	uint64_t swap16                       : 1;
	uint64_t swap32                       : 1;
	uint64_t reserved_60_60               : 1;
	uint64_t clr                          : 1;
	uint64_t rw                           : 1;
	uint64_t en                           : 1;
#endif
	} s;
	struct cvmx_mio_ndf_dma_cfg_s         cn52xx;
	struct cvmx_mio_ndf_dma_cfg_s         cn63xx;
	struct cvmx_mio_ndf_dma_cfg_s         cn63xxp1;
};
typedef union cvmx_mio_ndf_dma_cfg cvmx_mio_ndf_dma_cfg_t;

/**
 * cvmx_mio_ndf_dma_int
 *
 * MIO_NDF_DMA_INT = MIO NAND Flash DMA Interrupt Register
 *
 */
union cvmx_mio_ndf_dma_int
{
	uint64_t u64;
	struct cvmx_mio_ndf_dma_int_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t done                         : 1;  /**< DMA Engine request completion interrupt */
#else
	uint64_t done                         : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_ndf_dma_int_s         cn52xx;
	struct cvmx_mio_ndf_dma_int_s         cn63xx;
	struct cvmx_mio_ndf_dma_int_s         cn63xxp1;
};
typedef union cvmx_mio_ndf_dma_int cvmx_mio_ndf_dma_int_t;

/**
 * cvmx_mio_ndf_dma_int_en
 *
 * MIO_NDF_DMA_INT_EN = MIO NAND Flash DMA Interrupt Enable Register
 *
 */
union cvmx_mio_ndf_dma_int_en
{
	uint64_t u64;
	struct cvmx_mio_ndf_dma_int_en_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t done                         : 1;  /**< DMA Engine request completion interrupt enable */
#else
	uint64_t done                         : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_ndf_dma_int_en_s      cn52xx;
	struct cvmx_mio_ndf_dma_int_en_s      cn63xx;
	struct cvmx_mio_ndf_dma_int_en_s      cn63xxp1;
};
typedef union cvmx_mio_ndf_dma_int_en cvmx_mio_ndf_dma_int_en_t;

/**
 * cvmx_mio_pll_ctl
 */
union cvmx_mio_pll_ctl
{
	uint64_t u64;
	struct cvmx_mio_pll_ctl_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_5_63                : 59;
	uint64_t bw_ctl                       : 5;  /**< Core PLL bandwidth control */
#else
	uint64_t bw_ctl                       : 5;
	uint64_t reserved_5_63                : 59;
#endif
	} s;
	struct cvmx_mio_pll_ctl_s             cn30xx;
	struct cvmx_mio_pll_ctl_s             cn31xx;
};
typedef union cvmx_mio_pll_ctl cvmx_mio_pll_ctl_t;

/**
 * cvmx_mio_pll_setting
 */
union cvmx_mio_pll_setting
{
	uint64_t u64;
	struct cvmx_mio_pll_setting_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_17_63               : 47;
	uint64_t setting                      : 17; /**< Core PLL setting */
#else
	uint64_t setting                      : 17;
	uint64_t reserved_17_63               : 47;
#endif
	} s;
	struct cvmx_mio_pll_setting_s         cn30xx;
	struct cvmx_mio_pll_setting_s         cn31xx;
};
typedef union cvmx_mio_pll_setting cvmx_mio_pll_setting_t;

/**
 * cvmx_mio_ptp_clock_cfg
 *
 * MIO_PTP_CLOCK_CFG = Configuration
 *
 */
union cvmx_mio_ptp_clock_cfg
{
	uint64_t u64;
	struct cvmx_mio_ptp_clock_cfg_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_24_63               : 40;
	uint64_t evcnt_in                     : 6;  /**< Source for event counter input
                                                         0x00-0x0f : GPIO[EVCNT_IN[3:0]]
                                                         0x10      : QLM0_REF_CLK
                                                         0x11      : QLM1_REF_CLK
                                                         0x12      : QLM2_REF_CLK
                                                         0x13-0x3f : Reserved */
	uint64_t evcnt_edge                   : 1;  /**< Event counter input edge
                                                         0 = falling edge
                                                         1 = rising edge */
	uint64_t evcnt_en                     : 1;  /**< Enable event counter */
	uint64_t tstmp_in                     : 6;  /**< Source for timestamp input
                                                         0x00-0x0f : GPIO[TSTMP_IN[3:0]]
                                                         0x10      : QLM0_REF_CLK
                                                         0x11      : QLM1_REF_CLK
                                                         0x12      : QLM2_REF_CLK
                                                         0x13-0x3f : Reserved */
	uint64_t tstmp_edge                   : 1;  /**< External timestamp input edge
                                                         0 = falling edge
                                                         1 = rising edge */
	uint64_t tstmp_en                     : 1;  /**< Enable external timestamp */
	uint64_t ext_clk_in                   : 6;  /**< Source for external clock
                                                         0x00-0x0f : GPIO[EXT_CLK_IN[3:0]]
                                                         0x10      : QLM0_REF_CLK
                                                         0x11      : QLM1_REF_CLK
                                                         0x12      : QLM2_REF_CLK
                                                         0x13-0x3f : Reserved */
	uint64_t ext_clk_en                   : 1;  /**< Use positive edge of external clock */
	uint64_t ptp_en                       : 1;  /**< Enable PTP Module */
#else
	uint64_t ptp_en                       : 1;
	uint64_t ext_clk_en                   : 1;
	uint64_t ext_clk_in                   : 6;
	uint64_t tstmp_en                     : 1;
	uint64_t tstmp_edge                   : 1;
	uint64_t tstmp_in                     : 6;
	uint64_t evcnt_en                     : 1;
	uint64_t evcnt_edge                   : 1;
	uint64_t evcnt_in                     : 6;
	uint64_t reserved_24_63               : 40;
#endif
	} s;
	struct cvmx_mio_ptp_clock_cfg_s       cn63xx;
	struct cvmx_mio_ptp_clock_cfg_s       cn63xxp1;
};
typedef union cvmx_mio_ptp_clock_cfg cvmx_mio_ptp_clock_cfg_t;

/**
 * cvmx_mio_ptp_clock_comp
 *
 * MIO_PTP_CLOCK_COMP = Compensator
 *
 */
union cvmx_mio_ptp_clock_comp
{
	uint64_t u64;
	struct cvmx_mio_ptp_clock_comp_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t nanosec                      : 32; /**< Nanoseconds */
	uint64_t frnanosec                    : 32; /**< Fractions of Nanoseconds */
#else
	uint64_t frnanosec                    : 32;
	uint64_t nanosec                      : 32;
#endif
	} s;
	struct cvmx_mio_ptp_clock_comp_s      cn63xx;
	struct cvmx_mio_ptp_clock_comp_s      cn63xxp1;
};
typedef union cvmx_mio_ptp_clock_comp cvmx_mio_ptp_clock_comp_t;

/**
 * cvmx_mio_ptp_clock_hi
 *
 * MIO_PTP_CLOCK_HI = Hi bytes of CLOCK
 *
 * Writes to MIO_PTP_CLOCK_HI also clear MIO_PTP_CLOCK_LO. To update all 96 bits, write MIO_PTP_CLOCK_HI followed
 * by MIO_PTP_CLOCK_LO
 */
union cvmx_mio_ptp_clock_hi
{
	uint64_t u64;
	struct cvmx_mio_ptp_clock_hi_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t nanosec                      : 64; /**< Nanoseconds */
#else
	uint64_t nanosec                      : 64;
#endif
	} s;
	struct cvmx_mio_ptp_clock_hi_s        cn63xx;
	struct cvmx_mio_ptp_clock_hi_s        cn63xxp1;
};
typedef union cvmx_mio_ptp_clock_hi cvmx_mio_ptp_clock_hi_t;

/**
 * cvmx_mio_ptp_clock_lo
 *
 * MIO_PTP_CLOCK_LO = Lo bytes of CLOCK
 *
 */
union cvmx_mio_ptp_clock_lo
{
	uint64_t u64;
	struct cvmx_mio_ptp_clock_lo_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t frnanosec                    : 32; /**< Fractions of Nanoseconds */
#else
	uint64_t frnanosec                    : 32;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_mio_ptp_clock_lo_s        cn63xx;
	struct cvmx_mio_ptp_clock_lo_s        cn63xxp1;
};
typedef union cvmx_mio_ptp_clock_lo cvmx_mio_ptp_clock_lo_t;

/**
 * cvmx_mio_ptp_evt_cnt
 *
 * MIO_PTP_EVT_CNT = Event Counter
 *
 * Writes to MIO_PTP_EVT_CNT increment this register by the written data. The register counts down by
 * 1 for every MIO_PTP_CLOCK_CFG[EVCNT_EDGE] edge of MIO_PTP_CLOCK_CFG[EVCNT_IN]. When register equals
 * 0, an interrupt gets gerated
 */
union cvmx_mio_ptp_evt_cnt
{
	uint64_t u64;
	struct cvmx_mio_ptp_evt_cnt_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t cntr                         : 64; /**< Nanoseconds */
#else
	uint64_t cntr                         : 64;
#endif
	} s;
	struct cvmx_mio_ptp_evt_cnt_s         cn63xx;
	struct cvmx_mio_ptp_evt_cnt_s         cn63xxp1;
};
typedef union cvmx_mio_ptp_evt_cnt cvmx_mio_ptp_evt_cnt_t;

/**
 * cvmx_mio_ptp_timestamp
 *
 * MIO_PTP_TIMESTAMP = Timestamp latched on MIO_PTP_CLOCK_CFG[TSTMP_EDGE] edge of MIO_PTP_CLOCK_CFG[TSTMP_IN]
 *
 */
union cvmx_mio_ptp_timestamp
{
	uint64_t u64;
	struct cvmx_mio_ptp_timestamp_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t nanosec                      : 64; /**< Nanoseconds */
#else
	uint64_t nanosec                      : 64;
#endif
	} s;
	struct cvmx_mio_ptp_timestamp_s       cn63xx;
	struct cvmx_mio_ptp_timestamp_s       cn63xxp1;
};
typedef union cvmx_mio_ptp_timestamp cvmx_mio_ptp_timestamp_t;

/**
 * cvmx_mio_rst_boot
 */
union cvmx_mio_rst_boot
{
	uint64_t u64;
	struct cvmx_mio_rst_boot_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_36_63               : 28;
	uint64_t c_mul                        : 6;  /**< Core clock multiplier:
                                                           C_MUL = (core clk speed) / (ref clock speed)
                                                         "ref clock speed" should always be 50MHz.
                                                         If PLL_QLM_REF_CLK_EN=0, "ref clock" comes
                                                              from PLL_REF_CLK pin.
                                                         If PLL_QLM_REF_CLK_EN=1, "ref clock" is
                                                              1/2 speed of QLMC_REF_CLK_* pins. */
	uint64_t pnr_mul                      : 6;  /**< Coprocessor clock multiplier:
                                                           PNR_MUL = (coprocessor clk speed) /
                                                                           (ref clock speed)
                                                         See C_MUL comments about ref clock. */
	uint64_t qlm2_spd                     : 4;  /**< QLM2_SPD pins sampled at DCOK assertion */
	uint64_t qlm1_spd                     : 4;  /**< QLM1_SPD pins sampled at DCOK assertion */
	uint64_t qlm0_spd                     : 4;  /**< QLM0_SPD pins sampled at DCOK assertion */
	uint64_t lboot                        : 10; /**< Last boot cause mask, resets only with dock.

                                                         bit9 - Soft reset due to watchdog
                                                         bit8 - Soft reset due to CIU_SOFT_RST write
                                                         bit7 - Warm reset due to cntl0 link-down or
                                                                hot-reset
                                                         bit6 - Warm reset due to cntl1 link-down or
                                                                hot-reset
                                                         bit5 - Cntl1 reset due to PERST1_L pin
                                                         bit4 - Cntl0 reset due to PERST0_L pin
                                                         bit3 - Warm reset due to PERST1_L pin
                                                         bit2 - Warm reset due to PERST0_L pin
                                                         bit1 - Warm reset due to CHIP_RESET_L pin
                                                         bit0 - Cold reset due to DCOK pin */
	uint64_t rboot                        : 1;  /**< Determines whether core 0 remains in reset after
                                                         after chip cold/warm/soft reset. */
	uint64_t rboot_pin                    : 1;  /**< Read-only access to REMOTE_BOOT pin */
#else
	uint64_t rboot_pin                    : 1;
	uint64_t rboot                        : 1;
	uint64_t lboot                        : 10;
	uint64_t qlm0_spd                     : 4;
	uint64_t qlm1_spd                     : 4;
	uint64_t qlm2_spd                     : 4;
	uint64_t pnr_mul                      : 6;
	uint64_t c_mul                        : 6;
	uint64_t reserved_36_63               : 28;
#endif
	} s;
	struct cvmx_mio_rst_boot_s            cn63xx;
	struct cvmx_mio_rst_boot_s            cn63xxp1;
};
typedef union cvmx_mio_rst_boot cvmx_mio_rst_boot_t;

/**
 * cvmx_mio_rst_cfg
 *
 * Notes:
 * Cold reset will always performs a full bist.
 *
 */
union cvmx_mio_rst_cfg
{
	uint64_t u64;
	struct cvmx_mio_rst_cfg_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t bist_delay                   : 58; /**< Reserved */
	uint64_t reserved_3_5                 : 3;
	uint64_t cntl_clr_bist                : 1;  /**< Peform clear bist during cntl only reset,
                                                         instead of a full bist. A warm/soft reset will
                                                         not change this field. */
	uint64_t warm_clr_bist                : 1;  /**< Peform clear bist during warm reset, instead
                                                         of a full bist. A warm/soft reset will not
                                                         change this field. */
	uint64_t soft_clr_bist                : 1;  /**< Peform clear bist during soft reset, instead
                                                         of a full bist. A warm/soft reset will not
                                                         change this field. */
#else
	uint64_t soft_clr_bist                : 1;
	uint64_t warm_clr_bist                : 1;
	uint64_t cntl_clr_bist                : 1;
	uint64_t reserved_3_5                 : 3;
	uint64_t bist_delay                   : 58;
#endif
	} s;
	struct cvmx_mio_rst_cfg_s             cn63xx;
	struct cvmx_mio_rst_cfg_cn63xxp1
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t bist_delay                   : 58; /**< Reserved */
	uint64_t reserved_2_5                 : 4;
	uint64_t warm_clr_bist                : 1;  /**< Peform clear bist during warm reset, instead
                                                         of a full bist. A warm/soft reset will not
                                                         change this field. */
	uint64_t soft_clr_bist                : 1;  /**< Peform clear bist during soft reset, instead
                                                         of a full bist. A warm/soft reset will not
                                                         change this field. */
#else
	uint64_t soft_clr_bist                : 1;
	uint64_t warm_clr_bist                : 1;
	uint64_t reserved_2_5                 : 4;
	uint64_t bist_delay                   : 58;
#endif
	} cn63xxp1;
};
typedef union cvmx_mio_rst_cfg cvmx_mio_rst_cfg_t;

/**
 * cvmx_mio_rst_ctl#
 */
union cvmx_mio_rst_ctlx
{
	uint64_t u64;
	struct cvmx_mio_rst_ctlx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_10_63               : 54;
	uint64_t prst_link                    : 1;  /**< Controls whether corresponding controller
                                                         link-down or hot-reset causes the assertion of
                                                         CIU_SOFT_PRST*[SOFT_PRST]

                                                         A warm/soft reset will not change this field.
                                                         On cold reset, this field is initialized to 0
                                                         follows:
                                                            0 = when corresponding strap QLM*_HOST_MODE=1
                                                            1 = when corresponding strap QLM*_HOST_MODE=0

                                                         ***NOTE: Added in pass 2.0 */
	uint64_t rst_done                     : 1;  /**< Read-only access to controller reset status

                                                         RESET_DONE is always zero (i.e. the controller
                                                         is held in reset) when:
                                                           - CIU_SOFT_PRST*[SOFT_PRST]=1, or
                                                           - RST_RCV==1 and PERST*_L pin is asserted */
	uint64_t rst_link                     : 1;  /**< Controls whether corresponding controller
                                                         link-down or hot-reset causes a warm chip reset
                                                         On cold reset, this field is initialized as
                                                         follows:
                                                            0 = when corresponding strap QLM*_HOST_MODE=1
                                                            1 = when corresponding strap QLM*_HOST_MODE=0

                                                         Note that a link-down or hot-reset event can
                                                         never cause a warm chip reset when the
                                                         controller is in reset (i.e. can never cause a
                                                         warm reset when RST_DONE==0). */
	uint64_t host_mode                    : 1;  /**< RO access to corresponding strap QLM*_HOST_MODE */
	uint64_t prtmode                      : 2;  /**< Port mode
                                                            0 = port is EP mode
                                                            1 = port is RC mode
                                                            2,3 = Reserved
                                                         A warm/soft reset will not change this field.
                                                         On cold reset, this field is initialized as
                                                         follows:
                                                            0 = when corresponding strap QLM*_HOST_MODE=0
                                                            1 = when corresponding strap QLM*_HOST_MODE=1 */
	uint64_t rst_drv                      : 1;  /**< Controls whether corresponding PERST*_L chip pin
                                                         is driven by the OCTEON.  A warm/soft reset
                                                         will not change this field.  On cold reset,
                                                         this field is initialized as follows:
                                                          0 = when corresponding strap QLM*_HOST_MODE=0
                                                          1 = when corresponding strap QLM*_HOST_MODE=1

                                                         When set, OCTEON drives the corresponding
                                                         PERST*_L pin. Otherwise, OCTEON does not drive
                                                         the corresponding PERST*_L pin. */
	uint64_t rst_rcv                      : 1;  /**< Controls whether corresponding PERST*_L chip pin
                                                         is recieved by OCTEON.  A warm/soft reset
                                                         will not change this field.  On cold reset,
                                                         this field is initialized as follows:
                                                          0 = when corresponding strap QLM*_HOST_MODE=1
                                                          1 = when corresponding strap QLM*_HOST_MODE=0

                                                         When RST_RCV==1, the PERST*_L value is
                                                         received and may be used to reset the
                                                         controller and (optionally, based on RST_CHIP)
                                                         warm reset the chip.

                                                         When RST_RCV==1 (and RST_CHIP=0),
                                                         MIO_RST_INT[PERST*] gets set when the PERST*_L
                                                         pin asserts. (This interrupt can alert SW
                                                         whenever the external reset pin initiates a
                                                         controller reset sequence.)

                                                         RST_VAL gives the PERST*_L pin value when
                                                         RST_RCV==1.

                                                         When RST_RCV==0, the PERST*_L pin value is
                                                         ignored. */
	uint64_t rst_chip                     : 1;  /**< Controls whether corresponding PERST*_L chip
                                                         pin causes a chip warm reset like CHIP_RESET_L.
                                                         A warm/soft reset will not change this field.
                                                         On cold reset, this field is initialized to 0.

                                                         RST_CHIP is not used when RST_RCV==0.

                                                         When RST_RCV==0, RST_CHIP is ignored.

                                                         When RST_RCV==1, RST_CHIP==1, and PERST*_L
                                                         asserts, a chip warm reset will be generated. */
	uint64_t rst_val                      : 1;  /**< Read-only access to corresponding PERST*_L pin
                                                         Unpredictable when RST_RCV==0. Reads as 1 when
                                                         RST_RCV==1 and the PERST*_L pin is asserted.
                                                         Reads as 0 when RST_RCV==1 and the PERST*_L
                                                         pin is not asserted. */
#else
	uint64_t rst_val                      : 1;
	uint64_t rst_chip                     : 1;
	uint64_t rst_rcv                      : 1;
	uint64_t rst_drv                      : 1;
	uint64_t prtmode                      : 2;
	uint64_t host_mode                    : 1;
	uint64_t rst_link                     : 1;
	uint64_t rst_done                     : 1;
	uint64_t prst_link                    : 1;
	uint64_t reserved_10_63               : 54;
#endif
	} s;
	struct cvmx_mio_rst_ctlx_s            cn63xx;
	struct cvmx_mio_rst_ctlx_cn63xxp1
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_9_63                : 55;
	uint64_t rst_done                     : 1;  /**< Read-only access to controller reset status

                                                         RESET_DONE is always zero (i.e. the controller
                                                         is held in reset) when:
                                                           - CIU_SOFT_PRST*[SOFT_PRST]=1, or
                                                           - RST_RCV==1 and PERST*_L pin is asserted */
	uint64_t rst_link                     : 1;  /**< Controls whether corresponding controller
                                                         link-down or hot-reset causes a warm chip reset
                                                         On cold reset, this field is initialized as
                                                         follows:
                                                            0 = when corresponding strap QLM*_HOST_MODE=1
                                                            1 = when corresponding strap QLM*_HOST_MODE=0

                                                         Note that a link-down or hot-reset event can
                                                         never cause a warm chip reset when the
                                                         controller is in reset (i.e. can never cause a
                                                         warm reset when RST_DONE==0). */
	uint64_t host_mode                    : 1;  /**< RO access to corresponding strap QLM*_HOST_MODE */
	uint64_t prtmode                      : 2;  /**< Port mode
                                                            0 = port is EP mode
                                                            1 = port is RC mode
                                                            2,3 = Reserved
                                                         A warm/soft reset will not change this field.
                                                         On cold reset, this field is initialized as
                                                         follows:
                                                            0 = when corresponding strap QLM*_HOST_MODE=0
                                                            1 = when corresponding strap QLM*_HOST_MODE=1 */
	uint64_t rst_drv                      : 1;  /**< Controls whether corresponding PERST*_L chip pin
                                                         is driven by the OCTEON.  A warm/soft reset
                                                         will not change this field.  On cold reset,
                                                         this field is initialized as follows:
                                                          0 = when corresponding strap QLM*_HOST_MODE=0
                                                          1 = when corresponding strap QLM*_HOST_MODE=1

                                                         When set, OCTEON drives the corresponding
                                                         PERST*_L pin. Otherwise, OCTEON does not drive
                                                         the corresponding PERST*_L pin. */
	uint64_t rst_rcv                      : 1;  /**< Controls whether corresponding PERST*_L chip pin
                                                         is recieved by OCTEON.  A warm/soft reset
                                                         will not change this field.  On cold reset,
                                                         this field is initialized as follows:
                                                          0 = when corresponding strap QLM*_HOST_MODE=1
                                                          1 = when corresponding strap QLM*_HOST_MODE=0

                                                         When RST_RCV==1, the PERST*_L value is
                                                         received and may be used to reset the
                                                         controller and (optionally, based on RST_CHIP)
                                                         warm reset the chip.

                                                         When RST_RCV==1 (and RST_CHIP=0),
                                                         MIO_RST_INT[PERST*] gets set when the PERST*_L
                                                         pin asserts. (This interrupt can alert SW
                                                         whenever the external reset pin initiates a
                                                         controller reset sequence.)

                                                         RST_VAL gives the PERST*_L pin value when
                                                         RST_RCV==1.

                                                         When RST_RCV==0, the PERST*_L pin value is
                                                         ignored. */
	uint64_t rst_chip                     : 1;  /**< Controls whether corresponding PERST*_L chip
                                                         pin causes a chip warm reset like CHIP_RESET_L.
                                                         A warm/soft reset will not change this field.
                                                         On cold reset, this field is initialized to 0.

                                                         RST_CHIP is not used when RST_RCV==0.

                                                         When RST_RCV==0, RST_CHIP is ignored.

                                                         When RST_RCV==1, RST_CHIP==1, and PERST*_L
                                                         asserts, a chip warm reset will be generated. */
	uint64_t rst_val                      : 1;  /**< Read-only access to corresponding PERST*_L pin
                                                         Unpredictable when RST_RCV==0. Reads as 1 when
                                                         RST_RCV==1 and the PERST*_L pin is asserted.
                                                         Reads as 0 when RST_RCV==1 and the PERST*_L
                                                         pin is not asserted. */
#else
	uint64_t rst_val                      : 1;
	uint64_t rst_chip                     : 1;
	uint64_t rst_rcv                      : 1;
	uint64_t rst_drv                      : 1;
	uint64_t prtmode                      : 2;
	uint64_t host_mode                    : 1;
	uint64_t rst_link                     : 1;
	uint64_t rst_done                     : 1;
	uint64_t reserved_9_63                : 55;
#endif
	} cn63xxp1;
};
typedef union cvmx_mio_rst_ctlx cvmx_mio_rst_ctlx_t;

/**
 * cvmx_mio_rst_delay
 */
union cvmx_mio_rst_delay
{
	uint64_t u64;
	struct cvmx_mio_rst_delay_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_32_63               : 32;
	uint64_t soft_rst_dly                 : 16; /**< A soft reset immediately causes an early soft
                                                         reset notification.  However, the assertion of
                                                         soft reset will be delayed this many sclks.
                                                         A warm/soft reset will not change this field.
                                                         NOTE: This must be at least 500 dclks */
	uint64_t warm_rst_dly                 : 16; /**< A warm reset immediately causes an early warm
                                                         reset notification.  However, the assertion of
                                                         warm reset will be delayed this many sclks.
                                                         A warm/soft reset will not change this field.
                                                         NOTE: This must be at least 500 dclks */
#else
	uint64_t warm_rst_dly                 : 16;
	uint64_t soft_rst_dly                 : 16;
	uint64_t reserved_32_63               : 32;
#endif
	} s;
	struct cvmx_mio_rst_delay_s           cn63xx;
	struct cvmx_mio_rst_delay_s           cn63xxp1;
};
typedef union cvmx_mio_rst_delay cvmx_mio_rst_delay_t;

/**
 * cvmx_mio_rst_int
 *
 * MIO_RST_INT = MIO Reset Interrupt Register
 *
 */
union cvmx_mio_rst_int
{
	uint64_t u64;
	struct cvmx_mio_rst_int_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_10_63               : 54;
	uint64_t perst1                       : 1;  /**< PERST1_L asserted while MIO_RST_CTL1[RST_RCV]=1
                                                         and MIO_RST_CTL1[RST_CHIP]=0 */
	uint64_t perst0                       : 1;  /**< PERST0_L asserted while MIO_RST_CTL0[RST_RCV]=1
                                                         and MIO_RST_CTL0[RST_CHIP]=0 */
	uint64_t reserved_2_7                 : 6;
	uint64_t rst_link1                    : 1;  /**< A controller1 link-down/hot-reset occurred while
                                                         MIO_RST_CTL1[RST_LINK]=0.  Software must assert
                                                         then de-assert CIU_SOFT_PRST1[SOFT_PRST] */
	uint64_t rst_link0                    : 1;  /**< A controller0 link-down/hot-reset occurred while
                                                         MIO_RST_CTL0[RST_LINK]=0.  Software must assert
                                                         then de-assert CIU_SOFT_PRST[SOFT_PRST] */
#else
	uint64_t rst_link0                    : 1;
	uint64_t rst_link1                    : 1;
	uint64_t reserved_2_7                 : 6;
	uint64_t perst0                       : 1;
	uint64_t perst1                       : 1;
	uint64_t reserved_10_63               : 54;
#endif
	} s;
	struct cvmx_mio_rst_int_s             cn63xx;
	struct cvmx_mio_rst_int_s             cn63xxp1;
};
typedef union cvmx_mio_rst_int cvmx_mio_rst_int_t;

/**
 * cvmx_mio_rst_int_en
 *
 * MIO_RST_INT_EN = MIO Reset Interrupt Enable Register
 *
 */
union cvmx_mio_rst_int_en
{
	uint64_t u64;
	struct cvmx_mio_rst_int_en_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_10_63               : 54;
	uint64_t perst1                       : 1;  /**< Controller1 PERST reset interrupt enable */
	uint64_t perst0                       : 1;  /**< Controller0 PERST reset interrupt enable */
	uint64_t reserved_2_7                 : 6;
	uint64_t rst_link1                    : 1;  /**< Controller1 link-down/hot reset interrupt enable */
	uint64_t rst_link0                    : 1;  /**< Controller0 link-down/hot reset interrupt enable */
#else
	uint64_t rst_link0                    : 1;
	uint64_t rst_link1                    : 1;
	uint64_t reserved_2_7                 : 6;
	uint64_t perst0                       : 1;
	uint64_t perst1                       : 1;
	uint64_t reserved_10_63               : 54;
#endif
	} s;
	struct cvmx_mio_rst_int_en_s          cn63xx;
	struct cvmx_mio_rst_int_en_s          cn63xxp1;
};
typedef union cvmx_mio_rst_int_en cvmx_mio_rst_int_en_t;

/**
 * cvmx_mio_tws#_int
 *
 * MIO_TWSX_INT = TWSX Interrupt Register
 *
 * This register contains the TWSI interrupt enable mask and the interrupt source bits.  Note: the
 * interrupt source bit for the TWSI core interrupt (CORE_INT) is read-only, the appropriate sequence
 * must be written to the TWSI core to clear this interrupt.  The other interrupt source bits are write-
 * one-to-clear.  TS_INT is set on the update of the MIO_TWS_TWSI_SW register (i.e. when it is written
 * by a TWSI device).  ST_INT is set whenever the valid bit of the MIO_TWS_SW_TWSI is cleared (see above
 * for reasons).
 *
 * Note: When using the high-level controller, CORE_EN should be clear and CORE_INT should be ignored.
 * Conversely, when the high-level controller is disabled, ST_EN / TS_EN should be clear and ST_INT /
 * TS_INT should be ignored.
 *
 * This register also contains a read-only copy of the TWSI bus (SCL and SDA) as well as control bits to
 * override the current state of the TWSI bus (SCL_OVR and SDA_OVR).  Setting an override bit high will
 * result in the open drain driver being activated, thus driving the corresponding signal low.
 */
union cvmx_mio_twsx_int
{
	uint64_t u64;
	struct cvmx_mio_twsx_int_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_12_63               : 52;
	uint64_t scl                          : 1;  /**< SCL */
	uint64_t sda                          : 1;  /**< SDA */
	uint64_t scl_ovr                      : 1;  /**< SCL override */
	uint64_t sda_ovr                      : 1;  /**< SDA override */
	uint64_t reserved_7_7                 : 1;
	uint64_t core_en                      : 1;  /**< TWSI core interrupt enable */
	uint64_t ts_en                        : 1;  /**< MIO_TWS_TWSI_SW register update interrupt enable */
	uint64_t st_en                        : 1;  /**< MIO_TWS_SW_TWSI register update interrupt enable */
	uint64_t reserved_3_3                 : 1;
	uint64_t core_int                     : 1;  /**< TWSI core interrupt */
	uint64_t ts_int                       : 1;  /**< MIO_TWS_TWSI_SW register update interrupt */
	uint64_t st_int                       : 1;  /**< MIO_TWS_SW_TWSI register update interrupt */
#else
	uint64_t st_int                       : 1;
	uint64_t ts_int                       : 1;
	uint64_t core_int                     : 1;
	uint64_t reserved_3_3                 : 1;
	uint64_t st_en                        : 1;
	uint64_t ts_en                        : 1;
	uint64_t core_en                      : 1;
	uint64_t reserved_7_7                 : 1;
	uint64_t sda_ovr                      : 1;
	uint64_t scl_ovr                      : 1;
	uint64_t sda                          : 1;
	uint64_t scl                          : 1;
	uint64_t reserved_12_63               : 52;
#endif
	} s;
	struct cvmx_mio_twsx_int_s            cn30xx;
	struct cvmx_mio_twsx_int_s            cn31xx;
	struct cvmx_mio_twsx_int_s            cn38xx;
	struct cvmx_mio_twsx_int_cn38xxp2
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_7_63                : 57;
	uint64_t core_en                      : 1;  /**< TWSI core interrupt enable */
	uint64_t ts_en                        : 1;  /**< MIO_TWS_TWSI_SW register update interrupt enable */
	uint64_t st_en                        : 1;  /**< MIO_TWS_SW_TWSI register update interrupt enable */
	uint64_t reserved_3_3                 : 1;
	uint64_t core_int                     : 1;  /**< TWSI core interrupt */
	uint64_t ts_int                       : 1;  /**< MIO_TWS_TWSI_SW register update interrupt */
	uint64_t st_int                       : 1;  /**< MIO_TWS_SW_TWSI register update interrupt */
#else
	uint64_t st_int                       : 1;
	uint64_t ts_int                       : 1;
	uint64_t core_int                     : 1;
	uint64_t reserved_3_3                 : 1;
	uint64_t st_en                        : 1;
	uint64_t ts_en                        : 1;
	uint64_t core_en                      : 1;
	uint64_t reserved_7_63                : 57;
#endif
	} cn38xxp2;
	struct cvmx_mio_twsx_int_s            cn50xx;
	struct cvmx_mio_twsx_int_s            cn52xx;
	struct cvmx_mio_twsx_int_s            cn52xxp1;
	struct cvmx_mio_twsx_int_s            cn56xx;
	struct cvmx_mio_twsx_int_s            cn56xxp1;
	struct cvmx_mio_twsx_int_s            cn58xx;
	struct cvmx_mio_twsx_int_s            cn58xxp1;
	struct cvmx_mio_twsx_int_s            cn63xx;
	struct cvmx_mio_twsx_int_s            cn63xxp1;
};
typedef union cvmx_mio_twsx_int cvmx_mio_twsx_int_t;

/**
 * cvmx_mio_tws#_sw_twsi
 *
 * MIO_TWSX_SW_TWSI = TWSX Software to TWSI Register
 *
 * This register allows software to
 *    - initiate TWSI interface master-mode operations with a write and read the result with a read
 *    - load four bytes for later retrieval (slave mode) with a write and check validity with a read
 *    - launch a TWSI controller configuration read/write with a write and read the result with a read
 *
 * This register should be read or written by software, and read by the TWSI device. The TWSI device can
 * use either two-byte or five-byte reads to reference this register.
 *
 * The TWSI device considers this register valid when V==1 and SLONLY==1.
 */
union cvmx_mio_twsx_sw_twsi
{
	uint64_t u64;
	struct cvmx_mio_twsx_sw_twsi_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t v                            : 1;  /**< Valid bit
                                                         - Set on a write (should always be written with
                                                           a 1)
                                                         - Cleared when a TWSI master mode op completes
                                                         - Cleared when a TWSI configuration register
                                                           access completes
                                                         - Cleared when the TWSI device reads the
                                                           register if SLONLY==1 */
	uint64_t slonly                       : 1;  /**< Slave Only Mode
                                                         - No operation is initiated with a write when
                                                           this bit is set - only D field is updated in
                                                           this case
                                                         - When clear, a write initiates either a TWSI
                                                           master-mode operation or a TWSI configuration
                                                           register access */
	uint64_t eia                          : 1;  /**< Extended Internal Address - send additional
                                                         internal address byte (MSB of IA is from IA field
                                                         of MIO_TWS_SW_TWSI_EXT) */
	uint64_t op                           : 4;  /**< Opcode field - When the register is written with
                                                         SLONLY==0, initiate a read or write:
                                                           0000 => 7-bit Byte Master Mode TWSI Op
                                                           0001 => 7-bit Byte Combined Read Master Mode Op
                                                                   7-bit Byte Write w/ IA Master Mode Op
                                                           0010 => 10-bit Byte Master Mode TWSI Op
                                                           0011 => 10-bit Byte Combined Read Master Mode Op
                                                                   10-bit Byte Write w/ IA Master Mode Op
                                                           0100 => TWSI Master Clock Register
                                                           0110 => See EOP field
                                                           1000 => 7-bit 4-byte Master Mode TWSI Op
                                                           1001 => 7-bit 4-byte Comb. Read Master Mode Op
                                                                   7-bit 4-byte Write w/ IA Master Mode Op
                                                           1010 => 10-bit 4-byte Master Mode TWSI Op
                                                           1011 => 10-bit 4-byte Comb. Read Master Mode Op
                                                                   10-bit 4-byte Write w/ IA Master Mode Op */
	uint64_t r                            : 1;  /**< Read bit or result
                                                         - If set on a write when SLONLY==0, the
                                                           operation is a read
                                                         - On a read, this bit returns the result
                                                           indication for the most recent master mode
                                                           operation (1 = success, 0 = fail) */
	uint64_t sovr                         : 1;  /**< Size Override - if set, use the SIZE field to
                                                         determine Master Mode Op size rather than what
                                                         the Opcode field specifies.  For operations
                                                         greater than 4 bytes, the additional data will be
                                                         contained in the D field of MIO_TWS_SW_TWSI_EXT */
	uint64_t size                         : 3;  /**< Size in bytes of Master Mode Op if the Size
                                                         Override bit is set.  Specified in -1 notation
                                                         (i.e. 0 = 1 byte, 1 = 2 bytes ... 7 = 8 bytes) */
	uint64_t scr                          : 2;  /**< Scratch - unused, but retain state */
	uint64_t a                            : 10; /**< Address field
                                                          - the address of the remote device for a master
                                                            mode operation
                                                          - A<9:7> are only used for 10-bit addressing
                                                         Note that when mastering a 7-bit OP, A<6:0> should
                                                         not take any of the values 0x78, 0x79, 0x7A nor
                                                         0x7B (these 7-bit addresses are reserved to
                                                         extend to 10-bit addressing). */
	uint64_t ia                           : 5;  /**< Internal Address - Used when launching a master
                                                         mode combined read / write with internal address
                                                         (lower 3 bits are contained in the EOP_IA field) */
	uint64_t eop_ia                       : 3;  /**< Extra opcode (when OP<3:0> == 0110 and SLONLY==0):
                                                           000 => TWSI Slave Address Register
                                                           001 => TWSI Data Register
                                                           010 => TWSI Control Register
                                                           011 => TWSI Clock Control Register (when R == 0)
                                                           011 => TWSI Status Register (when R == 1)
                                                           100 => TWSI Extended Slave Register
                                                           111 => TWSI Soft Reset Register
                                                         Also the lower 3 bits of Internal Address when
                                                           launching a master mode combined read / write
                                                           with internal address */
	uint64_t d                            : 32; /**< Data Field
                                                         Used on a write when
                                                           - initiating a master-mode write (SLONLY==0)
                                                           - writing a TWSI config register (SLONLY==0)
                                                           - a slave mode write (SLONLY==1)
                                                         The read value is updated by
                                                           - a write to this register
                                                           - master mode completion (contains result or
                                                             error code)
                                                           - TWSI config register read (contains result) */
#else
	uint64_t d                            : 32;
	uint64_t eop_ia                       : 3;
	uint64_t ia                           : 5;
	uint64_t a                            : 10;
	uint64_t scr                          : 2;
	uint64_t size                         : 3;
	uint64_t sovr                         : 1;
	uint64_t r                            : 1;
	uint64_t op                           : 4;
	uint64_t eia                          : 1;
	uint64_t slonly                       : 1;
	uint64_t v                            : 1;
#endif
	} s;
	struct cvmx_mio_twsx_sw_twsi_s        cn30xx;
	struct cvmx_mio_twsx_sw_twsi_s        cn31xx;
	struct cvmx_mio_twsx_sw_twsi_s        cn38xx;
	struct cvmx_mio_twsx_sw_twsi_s        cn38xxp2;
	struct cvmx_mio_twsx_sw_twsi_s        cn50xx;
	struct cvmx_mio_twsx_sw_twsi_s        cn52xx;
	struct cvmx_mio_twsx_sw_twsi_s        cn52xxp1;
	struct cvmx_mio_twsx_sw_twsi_s        cn56xx;
	struct cvmx_mio_twsx_sw_twsi_s        cn56xxp1;
	struct cvmx_mio_twsx_sw_twsi_s        cn58xx;
	struct cvmx_mio_twsx_sw_twsi_s        cn58xxp1;
	struct cvmx_mio_twsx_sw_twsi_s        cn63xx;
	struct cvmx_mio_twsx_sw_twsi_s        cn63xxp1;
};
typedef union cvmx_mio_twsx_sw_twsi cvmx_mio_twsx_sw_twsi_t;

/**
 * cvmx_mio_tws#_sw_twsi_ext
 *
 * MIO_TWSX_SW_TWSI_EXT = TWSX Software to TWSI Extension Register
 *
 * This register contains an additional byte of internal address and 4 additional bytes of data to be
 * used with TWSI master mode operations.  IA will be sent as the first byte of internal address when
 * performing master mode combined read / write with internal address operations and the EIA bit of
 * MIO_TWS_SW_TWSI is set.  D extends the data field of MIO_TWS_SW_TWSI for a total of 8 bytes (SOVR
 * must be set to perform operations greater than 4 bytes).
 */
union cvmx_mio_twsx_sw_twsi_ext
{
	uint64_t u64;
	struct cvmx_mio_twsx_sw_twsi_ext_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_40_63               : 24;
	uint64_t ia                           : 8;  /**< Extended Internal Address */
	uint64_t d                            : 32; /**< Extended Data Field */
#else
	uint64_t d                            : 32;
	uint64_t ia                           : 8;
	uint64_t reserved_40_63               : 24;
#endif
	} s;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn30xx;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn31xx;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn38xx;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn38xxp2;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn50xx;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn52xx;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn52xxp1;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn56xx;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn56xxp1;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn58xx;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn58xxp1;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn63xx;
	struct cvmx_mio_twsx_sw_twsi_ext_s    cn63xxp1;
};
typedef union cvmx_mio_twsx_sw_twsi_ext cvmx_mio_twsx_sw_twsi_ext_t;

/**
 * cvmx_mio_tws#_twsi_sw
 *
 * MIO_TWSX_TWSI_SW = TWSX TWSI to Software Register
 *
 * This register allows the TWSI device to transfer data to software and later check that software has
 * received the information.
 *
 * This register should be read or written by the TWSI device, and read by software. The TWSI device can
 * use one-byte or four-byte payload writes, and two-byte payload reads.
 *
 * The TWSI device considers this register valid when V==1.
 */
union cvmx_mio_twsx_twsi_sw
{
	uint64_t u64;
	struct cvmx_mio_twsx_twsi_sw_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t v                            : 2;  /**< Valid Bits
                                                         - Not directly writable
                                                         - Set to 1 on any write by the TWSI device
                                                         - Cleared on any read by software */
	uint64_t reserved_32_61               : 30;
	uint64_t d                            : 32; /**< Data Field - updated on a write by the TWSI device */
#else
	uint64_t d                            : 32;
	uint64_t reserved_32_61               : 30;
	uint64_t v                            : 2;
#endif
	} s;
	struct cvmx_mio_twsx_twsi_sw_s        cn30xx;
	struct cvmx_mio_twsx_twsi_sw_s        cn31xx;
	struct cvmx_mio_twsx_twsi_sw_s        cn38xx;
	struct cvmx_mio_twsx_twsi_sw_s        cn38xxp2;
	struct cvmx_mio_twsx_twsi_sw_s        cn50xx;
	struct cvmx_mio_twsx_twsi_sw_s        cn52xx;
	struct cvmx_mio_twsx_twsi_sw_s        cn52xxp1;
	struct cvmx_mio_twsx_twsi_sw_s        cn56xx;
	struct cvmx_mio_twsx_twsi_sw_s        cn56xxp1;
	struct cvmx_mio_twsx_twsi_sw_s        cn58xx;
	struct cvmx_mio_twsx_twsi_sw_s        cn58xxp1;
	struct cvmx_mio_twsx_twsi_sw_s        cn63xx;
	struct cvmx_mio_twsx_twsi_sw_s        cn63xxp1;
};
typedef union cvmx_mio_twsx_twsi_sw cvmx_mio_twsx_twsi_sw_t;

/**
 * cvmx_mio_uart#_dlh
 *
 * MIO_UARTX_DLH = MIO UARTX Divisor Latch High Register
 *
 * The DLH (Divisor Latch High) register in conjunction with DLL (Divisor Latch Low) register form a
 * 16-bit, read/write, Divisor Latch register that contains the baud rate divisor for the UART. It is
 * accessed by first setting the DLAB bit (bit 7) in the Line Control Register (LCR). The output baud
 * rate is equal to eclk frequency divided by sixteen times the value of the baud rate divisor, as
 * follows: baud rate = eclk / (16 * divisor).
 *
 * Note that the BUSY bit (bit 0) of the UART Status Register (USR) must be clear before writing this
 * register. BUSY bit is always clear in PASS3.
 *
 * Note that with the Divisor Latch Registers (DLL and DLH) set to zero, the baud clock is disabled
 * and no serial communications will occur. Also, once the DLL or DLH is set, at least 8 clock cycles
 * of eclk should be allowed to pass before transmitting or receiving data.
 *
 * Note: The address below is an alias to simplify these CSR descriptions. It should be known that the
 * IER and DLH registers are the same.
 */
union cvmx_mio_uartx_dlh
{
	uint64_t u64;
	struct cvmx_mio_uartx_dlh_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t dlh                          : 8;  /**< Divisor Latch High Register */
#else
	uint64_t dlh                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_dlh_s           cn30xx;
	struct cvmx_mio_uartx_dlh_s           cn31xx;
	struct cvmx_mio_uartx_dlh_s           cn38xx;
	struct cvmx_mio_uartx_dlh_s           cn38xxp2;
	struct cvmx_mio_uartx_dlh_s           cn50xx;
	struct cvmx_mio_uartx_dlh_s           cn52xx;
	struct cvmx_mio_uartx_dlh_s           cn52xxp1;
	struct cvmx_mio_uartx_dlh_s           cn56xx;
	struct cvmx_mio_uartx_dlh_s           cn56xxp1;
	struct cvmx_mio_uartx_dlh_s           cn58xx;
	struct cvmx_mio_uartx_dlh_s           cn58xxp1;
	struct cvmx_mio_uartx_dlh_s           cn63xx;
	struct cvmx_mio_uartx_dlh_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_dlh cvmx_mio_uartx_dlh_t;
typedef cvmx_mio_uartx_dlh_t cvmx_uart_dlh_t;

/**
 * cvmx_mio_uart#_dll
 *
 * MIO_UARTX_DLL = MIO UARTX Divisor Latch Low Register
 *
 * The DLH (Divisor Latch High) register in conjunction with DLL (Divisor Latch Low) register form a
 * 16-bit, read/write, Divisor Latch register that contains the baud rate divisor for the UART. It is
 * accessed by first setting the DLAB bit (bit 7) in the Line Control Register (LCR). The output baud
 * rate is equal to eclk frequency divided by sixteen times the value of the baud rate divisor, as
 * follows: baud rate = eclk / (16 * divisor).
 *
 * Note that the BUSY bit (bit 0) of the UART Status Register (USR) must be clear before writing this
 * register. BUSY bit is always clear in PASS3.
 *
 * Note that with the Divisor Latch Registers (DLL and DLH) set to zero, the baud clock is disabled
 * and no serial communications will occur. Also, once the DLL or DLH is set, at least 8 clock cycles
 * of eclk should be allowed to pass before transmitting or receiving data.
 *
 * Note: The address below is an alias to simplify these CSR descriptions. It should be known that the
 * RBR, THR, and DLL registers are the same.
 */
union cvmx_mio_uartx_dll
{
	uint64_t u64;
	struct cvmx_mio_uartx_dll_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t dll                          : 8;  /**< Divisor Latch Low Register */
#else
	uint64_t dll                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_dll_s           cn30xx;
	struct cvmx_mio_uartx_dll_s           cn31xx;
	struct cvmx_mio_uartx_dll_s           cn38xx;
	struct cvmx_mio_uartx_dll_s           cn38xxp2;
	struct cvmx_mio_uartx_dll_s           cn50xx;
	struct cvmx_mio_uartx_dll_s           cn52xx;
	struct cvmx_mio_uartx_dll_s           cn52xxp1;
	struct cvmx_mio_uartx_dll_s           cn56xx;
	struct cvmx_mio_uartx_dll_s           cn56xxp1;
	struct cvmx_mio_uartx_dll_s           cn58xx;
	struct cvmx_mio_uartx_dll_s           cn58xxp1;
	struct cvmx_mio_uartx_dll_s           cn63xx;
	struct cvmx_mio_uartx_dll_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_dll cvmx_mio_uartx_dll_t;
typedef cvmx_mio_uartx_dll_t cvmx_uart_dll_t;

/**
 * cvmx_mio_uart#_far
 *
 * MIO_UARTX_FAR = MIO UARTX FIFO Access Register
 *
 * The FIFO Access Register (FAR) is used to enable a FIFO access mode for testing, so that the receive
 * FIFO can be written by software and the transmit FIFO can be read by software when the FIFOs are
 * enabled. When FIFOs are not enabled it allows the RBR to be written by software and the THR to be read
 * by software. Note, that when the FIFO access mode is enabled/disabled, the control portion of the
 * receive FIFO and transmit FIFO is reset and the FIFOs are treated as empty.
 */
union cvmx_mio_uartx_far
{
	uint64_t u64;
	struct cvmx_mio_uartx_far_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t far                          : 1;  /**< FIFO Access Register */
#else
	uint64_t far                          : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uartx_far_s           cn30xx;
	struct cvmx_mio_uartx_far_s           cn31xx;
	struct cvmx_mio_uartx_far_s           cn38xx;
	struct cvmx_mio_uartx_far_s           cn38xxp2;
	struct cvmx_mio_uartx_far_s           cn50xx;
	struct cvmx_mio_uartx_far_s           cn52xx;
	struct cvmx_mio_uartx_far_s           cn52xxp1;
	struct cvmx_mio_uartx_far_s           cn56xx;
	struct cvmx_mio_uartx_far_s           cn56xxp1;
	struct cvmx_mio_uartx_far_s           cn58xx;
	struct cvmx_mio_uartx_far_s           cn58xxp1;
	struct cvmx_mio_uartx_far_s           cn63xx;
	struct cvmx_mio_uartx_far_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_far cvmx_mio_uartx_far_t;
typedef cvmx_mio_uartx_far_t cvmx_uart_far_t;

/**
 * cvmx_mio_uart#_fcr
 *
 * MIO_UARTX_FCR = MIO UARTX FIFO Control Register
 *
 * The FIFO Control Register (FCR) is a write-only register that controls the read and write data FIFO
 * operation. When FIFOs and Programmable THRE Interrupt mode are enabled, this register also controls
 * the THRE Interrupt empty threshold level.
 *
 * Setting bit 0 of the FCR enables the transmit and receive FIFOs. Whenever the value of this bit is
 * changed both the TX and RX FIFOs will be reset.
 *
 * Writing a '1' to bit 1 of the FCR resets and flushes data in the receive FIFO. Note that this bit is
 * self-clearing and it is not necessary to clear this bit.
 *
 * Writing a '1' to bit 2 of the FCR resets and flushes data in the transmit FIFO. Note that this bit is
 * self-clearing and it is not necessary to clear this bit.
 *
 * If the FIFOs and Programmable THRE Interrupt mode are enabled, bits 4 and 5 control the empty
 * threshold level at which THRE Interrupts are generated when the mode is active.  See the following
 * table for encodings:
 *
 * TX Trigger
 * ----------
 * 00 = empty FIFO
 * 01 = 2 chars in FIFO
 * 10 = FIFO 1/4 full
 * 11 = FIFO 1/2 full
 *
 * If the FIFO mode is enabled (bit 0 of the FCR is set to '1') bits 6 and 7 are active. Bit 6 and bit 7
 * set the trigger level in the receiver FIFO for the Enable Received Data Available Interrupt (ERBFI).
 * In auto flow control mode the trigger is used to determine when the rts_n signal will be deasserted.
 * See the following table for encodings:
 *
 * RX Trigger
 * ----------
 * 00 = 1 char in FIFO
 * 01 = FIFO 1/4 full
 * 10 = FIFO 1/2 full
 * 11 = FIFO 2 chars less than full
 *
 * Note: The address below is an alias to simplify these CSR descriptions. It should be known that the
 * IIR and FCR registers are the same.
 */
union cvmx_mio_uartx_fcr
{
	uint64_t u64;
	struct cvmx_mio_uartx_fcr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t rxtrig                       : 2;  /**< RX Trigger */
	uint64_t txtrig                       : 2;  /**< TX Trigger */
	uint64_t reserved_3_3                 : 1;
	uint64_t txfr                         : 1;  /**< TX FIFO reset */
	uint64_t rxfr                         : 1;  /**< RX FIFO reset */
	uint64_t en                           : 1;  /**< FIFO enable */
#else
	uint64_t en                           : 1;
	uint64_t rxfr                         : 1;
	uint64_t txfr                         : 1;
	uint64_t reserved_3_3                 : 1;
	uint64_t txtrig                       : 2;
	uint64_t rxtrig                       : 2;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_fcr_s           cn30xx;
	struct cvmx_mio_uartx_fcr_s           cn31xx;
	struct cvmx_mio_uartx_fcr_s           cn38xx;
	struct cvmx_mio_uartx_fcr_s           cn38xxp2;
	struct cvmx_mio_uartx_fcr_s           cn50xx;
	struct cvmx_mio_uartx_fcr_s           cn52xx;
	struct cvmx_mio_uartx_fcr_s           cn52xxp1;
	struct cvmx_mio_uartx_fcr_s           cn56xx;
	struct cvmx_mio_uartx_fcr_s           cn56xxp1;
	struct cvmx_mio_uartx_fcr_s           cn58xx;
	struct cvmx_mio_uartx_fcr_s           cn58xxp1;
	struct cvmx_mio_uartx_fcr_s           cn63xx;
	struct cvmx_mio_uartx_fcr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_fcr cvmx_mio_uartx_fcr_t;
typedef cvmx_mio_uartx_fcr_t cvmx_uart_fcr_t;

/**
 * cvmx_mio_uart#_htx
 *
 * MIO_UARTX_HTX = MIO UARTX Halt TX Register
 *
 * The Halt TX Register (HTX) is used to halt transmissions for testing, so that the transmit FIFO can be
 * filled by software when FIFOs are enabled. If FIFOs are not enabled, setting the HTX register will
 * have no effect.
 */
union cvmx_mio_uartx_htx
{
	uint64_t u64;
	struct cvmx_mio_uartx_htx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t htx                          : 1;  /**< Halt TX */
#else
	uint64_t htx                          : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uartx_htx_s           cn30xx;
	struct cvmx_mio_uartx_htx_s           cn31xx;
	struct cvmx_mio_uartx_htx_s           cn38xx;
	struct cvmx_mio_uartx_htx_s           cn38xxp2;
	struct cvmx_mio_uartx_htx_s           cn50xx;
	struct cvmx_mio_uartx_htx_s           cn52xx;
	struct cvmx_mio_uartx_htx_s           cn52xxp1;
	struct cvmx_mio_uartx_htx_s           cn56xx;
	struct cvmx_mio_uartx_htx_s           cn56xxp1;
	struct cvmx_mio_uartx_htx_s           cn58xx;
	struct cvmx_mio_uartx_htx_s           cn58xxp1;
	struct cvmx_mio_uartx_htx_s           cn63xx;
	struct cvmx_mio_uartx_htx_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_htx cvmx_mio_uartx_htx_t;
typedef cvmx_mio_uartx_htx_t cvmx_uart_htx_t;

/**
 * cvmx_mio_uart#_ier
 *
 * MIO_UARTX_IER = MIO UARTX Interrupt Enable Register
 *
 * Interrupt Enable Register (IER) is a read/write register that contains four bits that enable
 * the generation of interrupts. These four bits are the Enable Received Data Available Interrupt
 * (ERBFI), the Enable Transmitter Holding Register Empty Interrupt (ETBEI), the Enable Receiver Line
 * Status Interrupt (ELSI), and the Enable Modem Status Interrupt (EDSSI).
 *
 * The IER also contains an enable bit (PTIME) for the Programmable THRE Interrupt mode.
 *
 * Note: The Divisor Latch Address Bit (DLAB) of the Line Control Register (LCR) must be clear to access
 * this register.
 *
 * Note: The address below is an alias to simplify these CSR descriptions. It should be known that the
 * IER and DLH registers are the same.
 */
union cvmx_mio_uartx_ier
{
	uint64_t u64;
	struct cvmx_mio_uartx_ier_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t ptime                        : 1;  /**< Programmable THRE Interrupt mode enable */
	uint64_t reserved_4_6                 : 3;
	uint64_t edssi                        : 1;  /**< Enable Modem Status Interrupt */
	uint64_t elsi                         : 1;  /**< Enable Receiver Line Status Interrupt */
	uint64_t etbei                        : 1;  /**< Enable Transmitter Holding Register Empty Interrupt */
	uint64_t erbfi                        : 1;  /**< Enable Received Data Available Interrupt */
#else
	uint64_t erbfi                        : 1;
	uint64_t etbei                        : 1;
	uint64_t elsi                         : 1;
	uint64_t edssi                        : 1;
	uint64_t reserved_4_6                 : 3;
	uint64_t ptime                        : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_ier_s           cn30xx;
	struct cvmx_mio_uartx_ier_s           cn31xx;
	struct cvmx_mio_uartx_ier_s           cn38xx;
	struct cvmx_mio_uartx_ier_s           cn38xxp2;
	struct cvmx_mio_uartx_ier_s           cn50xx;
	struct cvmx_mio_uartx_ier_s           cn52xx;
	struct cvmx_mio_uartx_ier_s           cn52xxp1;
	struct cvmx_mio_uartx_ier_s           cn56xx;
	struct cvmx_mio_uartx_ier_s           cn56xxp1;
	struct cvmx_mio_uartx_ier_s           cn58xx;
	struct cvmx_mio_uartx_ier_s           cn58xxp1;
	struct cvmx_mio_uartx_ier_s           cn63xx;
	struct cvmx_mio_uartx_ier_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_ier cvmx_mio_uartx_ier_t;
typedef cvmx_mio_uartx_ier_t cvmx_uart_ier_t;

/**
 * cvmx_mio_uart#_iir
 *
 * MIO_UARTX_IIR = MIO UARTX Interrupt Identity Register
 *
 * The Interrupt Identity Register (IIR) is a read-only register that identifies the source of an
 * interrupt. The upper two bits of the register are FIFO-enabled bits. These bits are '00' if the FIFOs
 * are disabled, and '11' if they are enabled. The lower four bits identify the highest priority pending
 * interrupt. The following table defines interrupt source decoding, interrupt priority, and interrupt
 * reset control:
 *
 * Interrupt   Priority   Interrupt         Interrupt                                       Interrupt
 * ID          Level      Type              Source                                          Reset By
 * ---------------------------------------------------------------------------------------------------------------------------------
 * 0001        -          None              None                                            -
 *
 * 0110        Highest    Receiver Line     Overrun, parity, or framing errors or break     Reading the Line Status Register
 *                        Status            interrupt
 *
 * 0100        Second     Received Data     Receiver data available (FIFOs disabled) or     Reading the Receiver Buffer Register
 *                        Available         RX FIFO trigger level reached (FIFOs            (FIFOs disabled) or the FIFO drops below
 *                                          enabled)                                        the trigger level (FIFOs enabled)
 *
 * 1100        Second     Character         No characters in or out of the RX FIFO          Reading the Receiver Buffer Register
 *                        Timeout           during the last 4 character times and there
 *                        Indication        is at least 1 character in it during this
 *                                          time
 *
 * 0010        Third      Transmitter       Transmitter Holding Register Empty              Reading the Interrupt Identity Register
 *                        Holding           (Programmable THRE Mode disabled) or TX         (if source of interrupt) or writing into
 *                        Register          FIFO at or below threshold (Programmable        THR (FIFOs or THRE Mode disabled) or TX
 *                        Empty             THRE Mode enabled)                              FIFO above threshold (FIFOs and THRE
 *                                                                                          Mode enabled)
 *
 * 0000        Fourth     Modem Status      Clear To Send (CTS) or Data Set Ready (DSR)     Reading the Modem Status Register
 *                        Changed           or Ring Indicator (RI) or Data Carrier
 *                                          Detect (DCD) changed (note: if auto flow
 *                                          control mode is enabled, a change in CTS
 *                                          will not cause an interrupt)
 *
 * 0111        Fifth      Busy Detect       Software has tried to write to the Line         Reading the UART Status Register
 *                        Indication        Control Register while the BUSY bit of the
 *                                          UART Status Register was set
 *
 * Note: The Busy Detect Indication interrupt has been removed from PASS3 and will never assert.
 *
 * Note: The address below is an alias to simplify these CSR descriptions. It should be known that the
 * IIR and FCR registers are the same.
 */
union cvmx_mio_uartx_iir
{
	uint64_t u64;
	struct cvmx_mio_uartx_iir_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t fen                          : 2;  /**< FIFO-enabled bits */
	uint64_t reserved_4_5                 : 2;
	cvmx_uart_iid_t iid                   : 4;  /**< Interrupt ID */
#else
	cvmx_uart_iid_t iid                   : 4;
	uint64_t reserved_4_5                 : 2;
	uint64_t fen                          : 2;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_iir_s           cn30xx;
	struct cvmx_mio_uartx_iir_s           cn31xx;
	struct cvmx_mio_uartx_iir_s           cn38xx;
	struct cvmx_mio_uartx_iir_s           cn38xxp2;
	struct cvmx_mio_uartx_iir_s           cn50xx;
	struct cvmx_mio_uartx_iir_s           cn52xx;
	struct cvmx_mio_uartx_iir_s           cn52xxp1;
	struct cvmx_mio_uartx_iir_s           cn56xx;
	struct cvmx_mio_uartx_iir_s           cn56xxp1;
	struct cvmx_mio_uartx_iir_s           cn58xx;
	struct cvmx_mio_uartx_iir_s           cn58xxp1;
	struct cvmx_mio_uartx_iir_s           cn63xx;
	struct cvmx_mio_uartx_iir_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_iir cvmx_mio_uartx_iir_t;
typedef cvmx_mio_uartx_iir_t cvmx_uart_iir_t;

/**
 * cvmx_mio_uart#_lcr
 *
 * MIO_UARTX_LCR = MIO UARTX Line Control Register
 *
 * The Line Control Register (LCR) controls the format of the data that is transmitted and received by
 * the UART.
 *
 * LCR bits 0 and 1 are the Character Length Select field. This field is used to select the number of
 * data bits per character that are transmitted and received. See the following table for encodings:
 *
 * CLS
 * ---
 * 00 = 5 bits (bits 0-4 sent)
 * 01 = 6 bits (bits 0-5 sent)
 * 10 = 7 bits (bits 0-6 sent)
 * 11 = 8 bits (all bits sent)
 *
 * LCR bit 2 controls the number of stop bits transmitted. If bit 2 is a '0', one stop bit is transmitted
 * in the serial data. If bit 2 is a '1' and the data bits are set to '00', one and a half stop bits are
 * generated. Otherwise, two stop bits are generated and transmitted in the serial data out. Note that
 * regardless of the number of stop bits selected the receiver will only check the first stop bit.
 *
 * LCR bit 3 is the Parity Enable bit. This bit is used to enable and disable parity generation and
 * detection in transmitted and received serial character respectively.
 *
 * LCR bit 4 is the Even Parity Select bit. If parity is enabled, bit 4 selects between even and odd
 * parity. If bit 4 is a '1', an even number of ones is transmitted or checked. If bit 4 is a '0', an odd
 * number of ones is transmitted or checked.
 *
 * LCR bit 6 is the Break Control bit. Setting the Break bit sends a break signal by holding the sout
 * line low (when not in Loopback mode, as determined by Modem Control Register bit 4). When in Loopback
 * mode, the break condition is internally looped back to the receiver.
 *
 * LCR bit 7 is the Divisor Latch Address bit. Setting this bit enables reading and writing of the
 * Divisor Latch register (DLL and DLH) to set the baud rate of the UART. This bit must be cleared after
 * initial baud rate setup in order to access other registers.
 *
 * Note: The LCR is writeable only when the UART is not busy (when the BUSY bit (bit 0) of the UART
 * Status Register (USR) is clear). The LCR is always readable. In PASS3, the LCR is always writable
 * because the BUSY bit is always clear.
 */
union cvmx_mio_uartx_lcr
{
	uint64_t u64;
	struct cvmx_mio_uartx_lcr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t dlab                         : 1;  /**< Divisor Latch Address bit */
	uint64_t brk                          : 1;  /**< Break Control bit */
	uint64_t reserved_5_5                 : 1;
	uint64_t eps                          : 1;  /**< Even Parity Select bit */
	uint64_t pen                          : 1;  /**< Parity Enable bit */
	uint64_t stop                         : 1;  /**< Stop Control bit */
	cvmx_uart_bits_t cls                  : 2;  /**< Character Length Select */
#else
	cvmx_uart_bits_t cls                  : 2;
	uint64_t stop                         : 1;
	uint64_t pen                          : 1;
	uint64_t eps                          : 1;
	uint64_t reserved_5_5                 : 1;
	uint64_t brk                          : 1;
	uint64_t dlab                         : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_lcr_s           cn30xx;
	struct cvmx_mio_uartx_lcr_s           cn31xx;
	struct cvmx_mio_uartx_lcr_s           cn38xx;
	struct cvmx_mio_uartx_lcr_s           cn38xxp2;
	struct cvmx_mio_uartx_lcr_s           cn50xx;
	struct cvmx_mio_uartx_lcr_s           cn52xx;
	struct cvmx_mio_uartx_lcr_s           cn52xxp1;
	struct cvmx_mio_uartx_lcr_s           cn56xx;
	struct cvmx_mio_uartx_lcr_s           cn56xxp1;
	struct cvmx_mio_uartx_lcr_s           cn58xx;
	struct cvmx_mio_uartx_lcr_s           cn58xxp1;
	struct cvmx_mio_uartx_lcr_s           cn63xx;
	struct cvmx_mio_uartx_lcr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_lcr cvmx_mio_uartx_lcr_t;
typedef cvmx_mio_uartx_lcr_t cvmx_uart_lcr_t;

/**
 * cvmx_mio_uart#_lsr
 *
 * MIO_UARTX_LSR = MIO UARTX Line Status Register
 *
 * The Line Status Register (LSR) contains status of the receiver and transmitter data transfers. This
 * status can be read by the user at anytime.
 *
 * LSR bit 0 is the Data Ready (DR) bit. When set, this bit indicates the receiver contains at least one
 * character in the RBR or the receiver FIFO. This bit is cleared when the RBR is read in the non-FIFO
 * mode, or when the receiver FIFO is empty, in FIFO mode.
 *
 * LSR bit 1 is the Overrun Error (OE) bit. When set, this bit indicates an overrun error has occurred
 * because a new data character was received before the previous data was read. In the non-FIFO mode, the
 * OE bit is set when a new character arrives in the receiver before the previous character was read from
 * the RBR. When this happens, the data in the RBR is overwritten. In the FIFO mode, an overrun error
 * occurs when the FIFO is full and a new character arrives at the receiver. The data in the FIFO is
 * retained and the data in the receive shift register is lost.
 *
 * LSR bit 2 is the Parity Error (PE) bit. This bit is set whenever there is a parity error in the
 * receiver if the Parity Enable (PEN) bit in the LCR is set. In the FIFO mode, since the parity error is
 * associated with a character received, it is revealed when the character with the parity error arrives
 * at the top of the FIFO. It should be noted that the Parity Error (PE) bit will be set if a break
 * interrupt has occurred, as indicated by the Break Interrupt (BI) bit.
 *
 * LSR bit 3 is the Framing Error (FE) bit. This bit is set whenever there is a framing error in the
 * receiver. A framing error occurs when the receiver does not detect a valid STOP bit in the received
 * data. In the FIFO mode, since the framing error is associated with a character received, it is
 * revealed when the character with the framing error is at the top of the FIFO. When a framing error
 * occurs the UART will try resynchronize. It does this by assuming that the error was due to the start
 * bit of the next character and then continues receiving the other bits (i.e. data and/or parity and
 * stop). It should be noted that the Framing Error (FE) bit will be set if a break interrupt has
 * occurred, as indicated by the Break Interrupt (BI) bit.
 *
 * Note: The OE, PE, and FE bits are reset when a read of the LSR is performed.
 *
 * LSR bit 4 is the Break Interrupt (BI) bit. This bit is set whenever the serial input (sin) is held in
 * a 0 state for longer than the sum of start time + data bits + parity + stop bits. A break condition on
 * sin causes one and only one character, consisting of all zeros, to be received by the UART. In the
 * FIFO mode, the character associated with the break condition is carried through the FIFO and is
 * revealed when the character is at the top of the FIFO. Reading the LSR clears the BI bit. In the non-
 * FIFO mode, the BI indication occurs immediately and persists until the LSR is read.
 *
 * LSR bit 5 is the Transmitter Holding Register Empty (THRE) bit. When Programmable THRE Interrupt mode
 * is disabled, this bit indicates that the UART can accept a new character for transmission. This bit is
 * set whenever data is transferred from the THR (or TX FIFO) to the transmitter shift register and no
 * new data has been written to the THR (or TX FIFO). This also causes a THRE Interrupt to occur, if the
 * THRE Interrupt is enabled. When FIFOs and Programmable THRE Interrupt mode are enabled, LSR bit 5
 * functionality is switched to indicate the transmitter FIFO is full, and no longer controls THRE
 * Interrupts, which are then controlled by the FCR[5:4] threshold setting.
 *
 * LSR bit 6 is the Transmitter Empty (TEMT) bit. In the FIFO mode, this bit is set whenever the
 * Transmitter Shift Register and the FIFO are both empty. In the non-FIFO mode, this bit is set whenever
 * the Transmitter Holding Register and the Transmitter Shift Register are both empty. This bit is
 * typically used to make sure it is safe to change control registers. Changing control registers while
 * the transmitter is busy can result in corrupt data being transmitted.
 *
 * LSR bit 7 is the Error in Receiver FIFO (FERR) bit. This bit is active only when FIFOs are enabled. It
 * is set when there is at least one parity error, framing error, or break indication in the FIFO. This
 * bit is cleared when the LSR is read and the character with the error is at the top of the receiver
 * FIFO and there are no subsequent errors in the FIFO.
 */
union cvmx_mio_uartx_lsr
{
	uint64_t u64;
	struct cvmx_mio_uartx_lsr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t ferr                         : 1;  /**< Error in Receiver FIFO bit */
	uint64_t temt                         : 1;  /**< Transmitter Empty bit */
	uint64_t thre                         : 1;  /**< Transmitter Holding Register Empty bit */
	uint64_t bi                           : 1;  /**< Break Interrupt bit */
	uint64_t fe                           : 1;  /**< Framing Error bit */
	uint64_t pe                           : 1;  /**< Parity Error bit */
	uint64_t oe                           : 1;  /**< Overrun Error bit */
	uint64_t dr                           : 1;  /**< Data Ready bit */
#else
	uint64_t dr                           : 1;
	uint64_t oe                           : 1;
	uint64_t pe                           : 1;
	uint64_t fe                           : 1;
	uint64_t bi                           : 1;
	uint64_t thre                         : 1;
	uint64_t temt                         : 1;
	uint64_t ferr                         : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_lsr_s           cn30xx;
	struct cvmx_mio_uartx_lsr_s           cn31xx;
	struct cvmx_mio_uartx_lsr_s           cn38xx;
	struct cvmx_mio_uartx_lsr_s           cn38xxp2;
	struct cvmx_mio_uartx_lsr_s           cn50xx;
	struct cvmx_mio_uartx_lsr_s           cn52xx;
	struct cvmx_mio_uartx_lsr_s           cn52xxp1;
	struct cvmx_mio_uartx_lsr_s           cn56xx;
	struct cvmx_mio_uartx_lsr_s           cn56xxp1;
	struct cvmx_mio_uartx_lsr_s           cn58xx;
	struct cvmx_mio_uartx_lsr_s           cn58xxp1;
	struct cvmx_mio_uartx_lsr_s           cn63xx;
	struct cvmx_mio_uartx_lsr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_lsr cvmx_mio_uartx_lsr_t;
typedef cvmx_mio_uartx_lsr_t cvmx_uart_lsr_t;

/**
 * cvmx_mio_uart#_mcr
 *
 * MIO_UARTX_MCR = MIO UARTX Modem Control Register
 *
 * The lower four bits of the Modem Control Register (MCR) directly manipulate the outputs of the UART.
 * The DTR (bit 0), RTS (bit 1), OUT1 (bit 2), and OUT2 (bit 3) bits are inverted and then drive the
 * corresponding UART outputs, dtr_n, rts_n, out1_n, and out2_n.  In loopback mode, these outputs are
 * driven inactive high while the values in these locations are internally looped back to the inputs.
 *
 * Note: When Auto RTS is enabled, the rts_n output is controlled in the same way, but is also gated
 * with the receiver FIFO threshold trigger (rts_n is inactive high when above the threshold). The
 * rts_n output will be de-asserted whenever RTS (bit 1) is set low.
 *
 * Note: The UART0 out1_n and out2_n outputs are not present on the pins of the chip, but the UART0 OUT1
 * and OUT2 bits still function in Loopback mode.  The UART1 dtr_n, out1_n, and out2_n outputs are not
 * present on the pins of the chip, but the UART1 DTR, OUT1, and OUT2 bits still function in Loopback
 * mode.
 *
 * MCR bit 4 is the Loopback bit. When set, data on the sout line is held high, while serial data output
 * is looped back to the sin line, internally. In this mode all the interrupts are fully functional. This
 * feature is used for diagnostic purposes. Also, in loopback mode, the modem control inputs (dsr_n,
 * cts_n, ri_n, dcd_n) are disconnected and the four modem control outputs (dtr_n, rts_n, out1_n, out1_n)
 * are looped back to the inputs, internally.
 *
 * MCR bit 5 is the Auto Flow Control Enable (AFCE) bit. When FIFOs are enabled and this bit is set,
 * 16750-compatible Auto RTS and Auto CTS serial data flow control features are enabled.
 *
 * Auto RTS becomes active when the following occurs:
 * 1. MCR bit 1 is set
 * 2. FIFOs are enabled by setting FIFO Control Register (FCR) bit 0
 * 3. MCR bit 5 is set (must be set after FCR bit 0)
 *
 * When active, the rts_n output is forced inactive-high when the receiver FIFO level reaches the
 * threshold set by FCR[7:6]. When rts_n is connected to the cts_n input of another UART device, the
 * other UART stops sending serial data until the receiver FIFO has available space.
 *
 * The selectable receiver FIFO threshold values are: 1, 1/4, 1/2, and 2 less than full. Since one
 * additional character may be transmitted to the UART after rts_n has become inactive (due to data
 * already having entered the transmitter block in the other UART), setting the threshold to 2 less
 * than full allows maximum use of the FIFO with a safety zone of one character.
 *
 * Once the receiver FIFO becomes completely empty by reading the Receiver Buffer Register (RBR), rts_n
 * again becomes active-low, signalling the other UART to continue sending data. It is important to note
 * that, even if everything else is set to Enabled and the correct MCR bits are set, if the FIFOs are
 * disabled through FCR[0], Auto Flow Control is also disabled. When Auto RTS is disabled or inactive,
 * rts_n is controlled solely by MCR[1].
 *
 * Auto CTS becomes active when the following occurs:
 * 1. FIFOs are enabled by setting FIFO Control Register (FCR) bit 0
 * 2. MCR bit 5 is set (must be set after FCR bit 0)
 *
 * When active, the UART transmitter is disabled whenever the cts_n input becomes inactive-high. This
 * prevents overflowing the FIFO of the receiving UART.
 *
 * Note that, if the cts_n input is not inactivated before the middle of the last stop bit, another
 * character is transmitted before the transmitter is disabled. While the transmitter is disabled, the
 * transmitter FIFO can still be written to, and even overflowed. Therefore, when using this mode, either
 * the true FIFO depth (64 characters) must be known to software, or the Programmable THRE Interrupt mode
 * must be enabled to access the FIFO full status through the Line Status Register. When using the FIFO
 * full status, software can poll this before each write to the Transmitter FIFO.
 *
 * Note: FIFO full status is also available in the UART Status Register (USR) or the actual level of the
 * FIFO may be read through the Transmit FIFO Level (TFL) register.
 *
 * When the cts_n input becomes active-low again, transmission resumes. It is important to note that,
 * even if everything else is set to Enabled, Auto Flow Control is also disabled if the FIFOs are
 * disabled through FCR[0]. When Auto CTS is disabled or inactive, the transmitter is unaffected by
 * cts_n.
 */
union cvmx_mio_uartx_mcr
{
	uint64_t u64;
	struct cvmx_mio_uartx_mcr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_6_63                : 58;
	uint64_t afce                         : 1;  /**< Auto Flow Control Enable bit */
	uint64_t loop                         : 1;  /**< Loopback bit */
	uint64_t out2                         : 1;  /**< OUT2 output bit */
	uint64_t out1                         : 1;  /**< OUT1 output bit */
	uint64_t rts                          : 1;  /**< Request To Send output bit */
	uint64_t dtr                          : 1;  /**< Data Terminal Ready output bit */
#else
	uint64_t dtr                          : 1;
	uint64_t rts                          : 1;
	uint64_t out1                         : 1;
	uint64_t out2                         : 1;
	uint64_t loop                         : 1;
	uint64_t afce                         : 1;
	uint64_t reserved_6_63                : 58;
#endif
	} s;
	struct cvmx_mio_uartx_mcr_s           cn30xx;
	struct cvmx_mio_uartx_mcr_s           cn31xx;
	struct cvmx_mio_uartx_mcr_s           cn38xx;
	struct cvmx_mio_uartx_mcr_s           cn38xxp2;
	struct cvmx_mio_uartx_mcr_s           cn50xx;
	struct cvmx_mio_uartx_mcr_s           cn52xx;
	struct cvmx_mio_uartx_mcr_s           cn52xxp1;
	struct cvmx_mio_uartx_mcr_s           cn56xx;
	struct cvmx_mio_uartx_mcr_s           cn56xxp1;
	struct cvmx_mio_uartx_mcr_s           cn58xx;
	struct cvmx_mio_uartx_mcr_s           cn58xxp1;
	struct cvmx_mio_uartx_mcr_s           cn63xx;
	struct cvmx_mio_uartx_mcr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_mcr cvmx_mio_uartx_mcr_t;
typedef cvmx_mio_uartx_mcr_t cvmx_uart_mcr_t;

/**
 * cvmx_mio_uart#_msr
 *
 * MIO_UARTX_MSR = MIO UARTX Modem Status Register
 *
 * The Modem Status Register (MSR) contains the current status of the modem control input lines and if
 * they changed.
 *
 * DCTS (bit 0), DDSR (bit 1), and DDCD (bit 3) bits record whether the modem control lines (cts_n,
 * dsr_n, and dcd_n) have changed since the last time the user read the MSR. TERI (bit 2) indicates ri_n
 * has changed from an active-low, to an inactive-high state since the last time the MSR was read. In
 * Loopback mode, DCTS reflects changes on MCR bit 1 (RTS), DDSR reflects changes on MCR bit 0 (DTR), and
 * DDCD reflects changes on MCR bit 3 (Out2), while TERI reflects when MCR bit 2 (Out1) has changed state
 * from a high to a low.
 *
 * Note: if the DCTS bit is not set and the cts_n signal is asserted (low) and a reset occurs (software
 * or otherwise), then the DCTS bit will get set when the reset is removed if the cts_n signal remains
 * asserted.
 *
 * The CTS, DSR, RI, and DCD Modem Status bits contain information on the current state of the modem
 * control lines. CTS (bit 4) is the compliment of cts_n, DSR (bit 5) is the compliment of dsr_n, RI
 * (bit 6) is the compliment of ri_n, and DCD (bit 7) is the compliment of dcd_n. In Loopback mode, CTS
 * is the same as MCR bit 1 (RTS), DSR is the same as MCR bit 0 (DTR), RI is the same as MCR bit 2
 * (Out1), and DCD is the same as MCR bit 3 (Out2).
 *
 * Note: The UART0 dsr_n and ri_n inputs are internally tied to power and not present on the pins of chip.
 * Thus the UART0 DSR and RI bits will be '0' when not in Loopback mode.  The UART1 dsr_n, ri_n, and dcd_n
 * inputs are internally tied to power and not present on the pins of chip. Thus the UART1 DSR, RI, and
 * DCD bits will be '0' when not in Loopback mode.
 */
union cvmx_mio_uartx_msr
{
	uint64_t u64;
	struct cvmx_mio_uartx_msr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t dcd                          : 1;  /**< Data Carrier Detect input bit */
	uint64_t ri                           : 1;  /**< Ring Indicator input bit */
	uint64_t dsr                          : 1;  /**< Data Set Ready input bit */
	uint64_t cts                          : 1;  /**< Clear To Send input bit */
	uint64_t ddcd                         : 1;  /**< Delta Data Carrier Detect bit */
	uint64_t teri                         : 1;  /**< Trailing Edge of Ring Indicator bit */
	uint64_t ddsr                         : 1;  /**< Delta Data Set Ready bit */
	uint64_t dcts                         : 1;  /**< Delta Clear To Send bit */
#else
	uint64_t dcts                         : 1;
	uint64_t ddsr                         : 1;
	uint64_t teri                         : 1;
	uint64_t ddcd                         : 1;
	uint64_t cts                          : 1;
	uint64_t dsr                          : 1;
	uint64_t ri                           : 1;
	uint64_t dcd                          : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_msr_s           cn30xx;
	struct cvmx_mio_uartx_msr_s           cn31xx;
	struct cvmx_mio_uartx_msr_s           cn38xx;
	struct cvmx_mio_uartx_msr_s           cn38xxp2;
	struct cvmx_mio_uartx_msr_s           cn50xx;
	struct cvmx_mio_uartx_msr_s           cn52xx;
	struct cvmx_mio_uartx_msr_s           cn52xxp1;
	struct cvmx_mio_uartx_msr_s           cn56xx;
	struct cvmx_mio_uartx_msr_s           cn56xxp1;
	struct cvmx_mio_uartx_msr_s           cn58xx;
	struct cvmx_mio_uartx_msr_s           cn58xxp1;
	struct cvmx_mio_uartx_msr_s           cn63xx;
	struct cvmx_mio_uartx_msr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_msr cvmx_mio_uartx_msr_t;
typedef cvmx_mio_uartx_msr_t cvmx_uart_msr_t;

/**
 * cvmx_mio_uart#_rbr
 *
 * MIO_UARTX_RBR = MIO UARTX Receive Buffer Register
 *
 * The Receive Buffer Register (RBR) is a read-only register that contains the data byte received on the
 * serial input port (sin). The data in this register is valid only if the Data Ready (DR) bit in the
 * Line status Register (LSR) is set. When the FIFOs are programmed OFF, the data in the RBR must be
 * read before the next data arrives, otherwise it is overwritten, resulting in an overrun error. When
 * the FIFOs are programmed ON, this register accesses the head of the receive FIFO. If the receive FIFO
 * is full (64 characters) and this register is not read before the next data character arrives, then the
 * data already in the FIFO is preserved, but any incoming data is lost. An overrun error also occurs.
 *
 * Note: The Divisor Latch Address Bit (DLAB) of the Line Control Register (LCR) must be clear to access
 * this register.
 *
 * Note: The address below is an alias to simplify these CSR descriptions. It should be known that the
 * RBR, THR, and DLL registers are the same.
 */
union cvmx_mio_uartx_rbr
{
	uint64_t u64;
	struct cvmx_mio_uartx_rbr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t rbr                          : 8;  /**< Receive Buffer Register */
#else
	uint64_t rbr                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_rbr_s           cn30xx;
	struct cvmx_mio_uartx_rbr_s           cn31xx;
	struct cvmx_mio_uartx_rbr_s           cn38xx;
	struct cvmx_mio_uartx_rbr_s           cn38xxp2;
	struct cvmx_mio_uartx_rbr_s           cn50xx;
	struct cvmx_mio_uartx_rbr_s           cn52xx;
	struct cvmx_mio_uartx_rbr_s           cn52xxp1;
	struct cvmx_mio_uartx_rbr_s           cn56xx;
	struct cvmx_mio_uartx_rbr_s           cn56xxp1;
	struct cvmx_mio_uartx_rbr_s           cn58xx;
	struct cvmx_mio_uartx_rbr_s           cn58xxp1;
	struct cvmx_mio_uartx_rbr_s           cn63xx;
	struct cvmx_mio_uartx_rbr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_rbr cvmx_mio_uartx_rbr_t;
typedef cvmx_mio_uartx_rbr_t cvmx_uart_rbr_t;

/**
 * cvmx_mio_uart#_rfl
 *
 * MIO_UARTX_RFL = MIO UARTX Receive FIFO Level Register
 *
 * The Receive FIFO Level Register (RFL) indicates the number of data entries in the receive FIFO.
 */
union cvmx_mio_uartx_rfl
{
	uint64_t u64;
	struct cvmx_mio_uartx_rfl_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_7_63                : 57;
	uint64_t rfl                          : 7;  /**< Receive FIFO Level Register */
#else
	uint64_t rfl                          : 7;
	uint64_t reserved_7_63                : 57;
#endif
	} s;
	struct cvmx_mio_uartx_rfl_s           cn30xx;
	struct cvmx_mio_uartx_rfl_s           cn31xx;
	struct cvmx_mio_uartx_rfl_s           cn38xx;
	struct cvmx_mio_uartx_rfl_s           cn38xxp2;
	struct cvmx_mio_uartx_rfl_s           cn50xx;
	struct cvmx_mio_uartx_rfl_s           cn52xx;
	struct cvmx_mio_uartx_rfl_s           cn52xxp1;
	struct cvmx_mio_uartx_rfl_s           cn56xx;
	struct cvmx_mio_uartx_rfl_s           cn56xxp1;
	struct cvmx_mio_uartx_rfl_s           cn58xx;
	struct cvmx_mio_uartx_rfl_s           cn58xxp1;
	struct cvmx_mio_uartx_rfl_s           cn63xx;
	struct cvmx_mio_uartx_rfl_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_rfl cvmx_mio_uartx_rfl_t;
typedef cvmx_mio_uartx_rfl_t cvmx_uart_rfl_t;

/**
 * cvmx_mio_uart#_rfw
 *
 * MIO_UARTX_RFW = MIO UARTX Receive FIFO Write Register
 *
 * The Receive FIFO Write Register (RFW) is only valid when FIFO access mode is enabled (FAR bit 0 is
 * set). When FIFOs are enabled, this register is used to write data to the receive FIFO. Each
 * consecutive write pushes the new data to the next write location in the receive FIFO. When FIFOs are
 * not enabled, this register is used to write data to the RBR.
 */
union cvmx_mio_uartx_rfw
{
	uint64_t u64;
	struct cvmx_mio_uartx_rfw_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_10_63               : 54;
	uint64_t rffe                         : 1;  /**< Receive FIFO Framing Error */
	uint64_t rfpe                         : 1;  /**< Receive FIFO Parity Error */
	uint64_t rfwd                         : 8;  /**< Receive FIFO Write Data */
#else
	uint64_t rfwd                         : 8;
	uint64_t rfpe                         : 1;
	uint64_t rffe                         : 1;
	uint64_t reserved_10_63               : 54;
#endif
	} s;
	struct cvmx_mio_uartx_rfw_s           cn30xx;
	struct cvmx_mio_uartx_rfw_s           cn31xx;
	struct cvmx_mio_uartx_rfw_s           cn38xx;
	struct cvmx_mio_uartx_rfw_s           cn38xxp2;
	struct cvmx_mio_uartx_rfw_s           cn50xx;
	struct cvmx_mio_uartx_rfw_s           cn52xx;
	struct cvmx_mio_uartx_rfw_s           cn52xxp1;
	struct cvmx_mio_uartx_rfw_s           cn56xx;
	struct cvmx_mio_uartx_rfw_s           cn56xxp1;
	struct cvmx_mio_uartx_rfw_s           cn58xx;
	struct cvmx_mio_uartx_rfw_s           cn58xxp1;
	struct cvmx_mio_uartx_rfw_s           cn63xx;
	struct cvmx_mio_uartx_rfw_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_rfw cvmx_mio_uartx_rfw_t;
typedef cvmx_mio_uartx_rfw_t cvmx_uart_rfw_t;

/**
 * cvmx_mio_uart#_sbcr
 *
 * MIO_UARTX_SBCR = MIO UARTX Shadow Break Control Register
 *
 * The Shadow Break Control Register (SBCR) is a shadow register for the BREAK bit (LCR bit 6) that can
 * be used to remove the burden of having to perform a read-modify-write on the LCR.
 */
union cvmx_mio_uartx_sbcr
{
	uint64_t u64;
	struct cvmx_mio_uartx_sbcr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t sbcr                         : 1;  /**< Shadow Break Control */
#else
	uint64_t sbcr                         : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uartx_sbcr_s          cn30xx;
	struct cvmx_mio_uartx_sbcr_s          cn31xx;
	struct cvmx_mio_uartx_sbcr_s          cn38xx;
	struct cvmx_mio_uartx_sbcr_s          cn38xxp2;
	struct cvmx_mio_uartx_sbcr_s          cn50xx;
	struct cvmx_mio_uartx_sbcr_s          cn52xx;
	struct cvmx_mio_uartx_sbcr_s          cn52xxp1;
	struct cvmx_mio_uartx_sbcr_s          cn56xx;
	struct cvmx_mio_uartx_sbcr_s          cn56xxp1;
	struct cvmx_mio_uartx_sbcr_s          cn58xx;
	struct cvmx_mio_uartx_sbcr_s          cn58xxp1;
	struct cvmx_mio_uartx_sbcr_s          cn63xx;
	struct cvmx_mio_uartx_sbcr_s          cn63xxp1;
};
typedef union cvmx_mio_uartx_sbcr cvmx_mio_uartx_sbcr_t;
typedef cvmx_mio_uartx_sbcr_t cvmx_uart_sbcr_t;

/**
 * cvmx_mio_uart#_scr
 *
 * MIO_UARTX_SCR = MIO UARTX Scratchpad Register
 *
 * The Scratchpad Register (SCR) is an 8-bit read/write register for programmers to use as a temporary
 * storage space.
 */
union cvmx_mio_uartx_scr
{
	uint64_t u64;
	struct cvmx_mio_uartx_scr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t scr                          : 8;  /**< Scratchpad Register */
#else
	uint64_t scr                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_scr_s           cn30xx;
	struct cvmx_mio_uartx_scr_s           cn31xx;
	struct cvmx_mio_uartx_scr_s           cn38xx;
	struct cvmx_mio_uartx_scr_s           cn38xxp2;
	struct cvmx_mio_uartx_scr_s           cn50xx;
	struct cvmx_mio_uartx_scr_s           cn52xx;
	struct cvmx_mio_uartx_scr_s           cn52xxp1;
	struct cvmx_mio_uartx_scr_s           cn56xx;
	struct cvmx_mio_uartx_scr_s           cn56xxp1;
	struct cvmx_mio_uartx_scr_s           cn58xx;
	struct cvmx_mio_uartx_scr_s           cn58xxp1;
	struct cvmx_mio_uartx_scr_s           cn63xx;
	struct cvmx_mio_uartx_scr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_scr cvmx_mio_uartx_scr_t;
typedef cvmx_mio_uartx_scr_t cvmx_uart_scr_t;

/**
 * cvmx_mio_uart#_sfe
 *
 * MIO_UARTX_SFE = MIO UARTX Shadow FIFO Enable Register
 *
 * The Shadow FIFO Enable Register (SFE) is a shadow register for the FIFO enable bit (FCR bit 0) that
 * can be used to remove the burden of having to store the previously written value to the FCR in memory
 * and having to mask this value so that only the FIFO enable bit gets updated.
 */
union cvmx_mio_uartx_sfe
{
	uint64_t u64;
	struct cvmx_mio_uartx_sfe_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t sfe                          : 1;  /**< Shadow FIFO Enable */
#else
	uint64_t sfe                          : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uartx_sfe_s           cn30xx;
	struct cvmx_mio_uartx_sfe_s           cn31xx;
	struct cvmx_mio_uartx_sfe_s           cn38xx;
	struct cvmx_mio_uartx_sfe_s           cn38xxp2;
	struct cvmx_mio_uartx_sfe_s           cn50xx;
	struct cvmx_mio_uartx_sfe_s           cn52xx;
	struct cvmx_mio_uartx_sfe_s           cn52xxp1;
	struct cvmx_mio_uartx_sfe_s           cn56xx;
	struct cvmx_mio_uartx_sfe_s           cn56xxp1;
	struct cvmx_mio_uartx_sfe_s           cn58xx;
	struct cvmx_mio_uartx_sfe_s           cn58xxp1;
	struct cvmx_mio_uartx_sfe_s           cn63xx;
	struct cvmx_mio_uartx_sfe_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_sfe cvmx_mio_uartx_sfe_t;
typedef cvmx_mio_uartx_sfe_t cvmx_uart_sfe_t;

/**
 * cvmx_mio_uart#_srr
 *
 * MIO_UARTX_SRR = MIO UARTX Software Reset Register
 *
 * The Software Reset Register (SRR) is a write-only register that resets the UART and/or the receive
 * FIFO and/or the transmit FIFO.
 *
 * Bit 0 of the SRR is the UART Soft Reset (USR) bit.  Setting this bit resets the UART.
 *
 * Bit 1 of the SRR is a shadow copy of the RX FIFO Reset bit (FCR bit 1). This can be used to remove
 * the burden on software having to store previously written FCR values (which are pretty static) just
 * to reset the receive FIFO.
 *
 * Bit 2 of the SRR is a shadow copy of the TX FIFO Reset bit (FCR bit 2). This can be used to remove
 * the burden on software having to store previously written FCR values (which are pretty static) just
 * to reset the transmit FIFO.
 */
union cvmx_mio_uartx_srr
{
	uint64_t u64;
	struct cvmx_mio_uartx_srr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_3_63                : 61;
	uint64_t stfr                         : 1;  /**< Shadow TX FIFO Reset */
	uint64_t srfr                         : 1;  /**< Shadow RX FIFO Reset */
	uint64_t usr                          : 1;  /**< UART Soft Reset */
#else
	uint64_t usr                          : 1;
	uint64_t srfr                         : 1;
	uint64_t stfr                         : 1;
	uint64_t reserved_3_63                : 61;
#endif
	} s;
	struct cvmx_mio_uartx_srr_s           cn30xx;
	struct cvmx_mio_uartx_srr_s           cn31xx;
	struct cvmx_mio_uartx_srr_s           cn38xx;
	struct cvmx_mio_uartx_srr_s           cn38xxp2;
	struct cvmx_mio_uartx_srr_s           cn50xx;
	struct cvmx_mio_uartx_srr_s           cn52xx;
	struct cvmx_mio_uartx_srr_s           cn52xxp1;
	struct cvmx_mio_uartx_srr_s           cn56xx;
	struct cvmx_mio_uartx_srr_s           cn56xxp1;
	struct cvmx_mio_uartx_srr_s           cn58xx;
	struct cvmx_mio_uartx_srr_s           cn58xxp1;
	struct cvmx_mio_uartx_srr_s           cn63xx;
	struct cvmx_mio_uartx_srr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_srr cvmx_mio_uartx_srr_t;
typedef cvmx_mio_uartx_srr_t cvmx_uart_srr_t;

/**
 * cvmx_mio_uart#_srt
 *
 * MIO_UARTX_SRT = MIO UARTX Shadow RX Trigger Register
 *
 * The Shadow RX Trigger Register (SRT) is a shadow register for the RX Trigger bits (FCR bits 7:6) that
 * can be used to remove the burden of having to store the previously written value to the FCR in memory
 * and having to mask this value so that only the RX Trigger bits get updated.
 */
union cvmx_mio_uartx_srt
{
	uint64_t u64;
	struct cvmx_mio_uartx_srt_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t srt                          : 2;  /**< Shadow RX Trigger */
#else
	uint64_t srt                          : 2;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_uartx_srt_s           cn30xx;
	struct cvmx_mio_uartx_srt_s           cn31xx;
	struct cvmx_mio_uartx_srt_s           cn38xx;
	struct cvmx_mio_uartx_srt_s           cn38xxp2;
	struct cvmx_mio_uartx_srt_s           cn50xx;
	struct cvmx_mio_uartx_srt_s           cn52xx;
	struct cvmx_mio_uartx_srt_s           cn52xxp1;
	struct cvmx_mio_uartx_srt_s           cn56xx;
	struct cvmx_mio_uartx_srt_s           cn56xxp1;
	struct cvmx_mio_uartx_srt_s           cn58xx;
	struct cvmx_mio_uartx_srt_s           cn58xxp1;
	struct cvmx_mio_uartx_srt_s           cn63xx;
	struct cvmx_mio_uartx_srt_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_srt cvmx_mio_uartx_srt_t;
typedef cvmx_mio_uartx_srt_t cvmx_uart_srt_t;

/**
 * cvmx_mio_uart#_srts
 *
 * MIO_UARTX_SRTS = MIO UARTX Shadow Request To Send Register
 *
 * The Shadow Request To Send Register (SRTS) is a shadow register for the RTS bit (MCR bit 1) that can
 * be used to remove the burden of having to perform a read-modify-write on the MCR.
 */
union cvmx_mio_uartx_srts
{
	uint64_t u64;
	struct cvmx_mio_uartx_srts_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t srts                         : 1;  /**< Shadow Request To Send */
#else
	uint64_t srts                         : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uartx_srts_s          cn30xx;
	struct cvmx_mio_uartx_srts_s          cn31xx;
	struct cvmx_mio_uartx_srts_s          cn38xx;
	struct cvmx_mio_uartx_srts_s          cn38xxp2;
	struct cvmx_mio_uartx_srts_s          cn50xx;
	struct cvmx_mio_uartx_srts_s          cn52xx;
	struct cvmx_mio_uartx_srts_s          cn52xxp1;
	struct cvmx_mio_uartx_srts_s          cn56xx;
	struct cvmx_mio_uartx_srts_s          cn56xxp1;
	struct cvmx_mio_uartx_srts_s          cn58xx;
	struct cvmx_mio_uartx_srts_s          cn58xxp1;
	struct cvmx_mio_uartx_srts_s          cn63xx;
	struct cvmx_mio_uartx_srts_s          cn63xxp1;
};
typedef union cvmx_mio_uartx_srts cvmx_mio_uartx_srts_t;
typedef cvmx_mio_uartx_srts_t cvmx_uart_srts_t;

/**
 * cvmx_mio_uart#_stt
 *
 * MIO_UARTX_STT = MIO UARTX Shadow TX Trigger Register
 *
 * The Shadow TX Trigger Register (STT) is a shadow register for the TX Trigger bits (FCR bits 5:4) that
 * can be used to remove the burden of having to store the previously written value to the FCR in memory
 * and having to mask this value so that only the TX Trigger bits get updated.
 */
union cvmx_mio_uartx_stt
{
	uint64_t u64;
	struct cvmx_mio_uartx_stt_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t stt                          : 2;  /**< Shadow TX Trigger */
#else
	uint64_t stt                          : 2;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_uartx_stt_s           cn30xx;
	struct cvmx_mio_uartx_stt_s           cn31xx;
	struct cvmx_mio_uartx_stt_s           cn38xx;
	struct cvmx_mio_uartx_stt_s           cn38xxp2;
	struct cvmx_mio_uartx_stt_s           cn50xx;
	struct cvmx_mio_uartx_stt_s           cn52xx;
	struct cvmx_mio_uartx_stt_s           cn52xxp1;
	struct cvmx_mio_uartx_stt_s           cn56xx;
	struct cvmx_mio_uartx_stt_s           cn56xxp1;
	struct cvmx_mio_uartx_stt_s           cn58xx;
	struct cvmx_mio_uartx_stt_s           cn58xxp1;
	struct cvmx_mio_uartx_stt_s           cn63xx;
	struct cvmx_mio_uartx_stt_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_stt cvmx_mio_uartx_stt_t;
typedef cvmx_mio_uartx_stt_t cvmx_uart_stt_t;

/**
 * cvmx_mio_uart#_tfl
 *
 * MIO_UARTX_TFL = MIO UARTX Transmit FIFO Level Register
 *
 * The Transmit FIFO Level Register (TFL) indicates the number of data entries in the transmit FIFO.
 */
union cvmx_mio_uartx_tfl
{
	uint64_t u64;
	struct cvmx_mio_uartx_tfl_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_7_63                : 57;
	uint64_t tfl                          : 7;  /**< Transmit FIFO Level Register */
#else
	uint64_t tfl                          : 7;
	uint64_t reserved_7_63                : 57;
#endif
	} s;
	struct cvmx_mio_uartx_tfl_s           cn30xx;
	struct cvmx_mio_uartx_tfl_s           cn31xx;
	struct cvmx_mio_uartx_tfl_s           cn38xx;
	struct cvmx_mio_uartx_tfl_s           cn38xxp2;
	struct cvmx_mio_uartx_tfl_s           cn50xx;
	struct cvmx_mio_uartx_tfl_s           cn52xx;
	struct cvmx_mio_uartx_tfl_s           cn52xxp1;
	struct cvmx_mio_uartx_tfl_s           cn56xx;
	struct cvmx_mio_uartx_tfl_s           cn56xxp1;
	struct cvmx_mio_uartx_tfl_s           cn58xx;
	struct cvmx_mio_uartx_tfl_s           cn58xxp1;
	struct cvmx_mio_uartx_tfl_s           cn63xx;
	struct cvmx_mio_uartx_tfl_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_tfl cvmx_mio_uartx_tfl_t;
typedef cvmx_mio_uartx_tfl_t cvmx_uart_tfl_t;

/**
 * cvmx_mio_uart#_tfr
 *
 * MIO_UARTX_TFR = MIO UARTX Transmit FIFO Read Register
 *
 * The Transmit FIFO Read Register (TFR) is only valid when FIFO access mode is enabled (FAR bit 0 is
 * set). When FIFOs are enabled, reading this register gives the data at the top of the transmit FIFO.
 * Each consecutive read pops the transmit FIFO and gives the next data value that is currently at the
 * top of the FIFO. When FIFOs are not enabled, reading this register gives the data in the THR.
 */
union cvmx_mio_uartx_tfr
{
	uint64_t u64;
	struct cvmx_mio_uartx_tfr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t tfr                          : 8;  /**< Transmit FIFO Read Register */
#else
	uint64_t tfr                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_tfr_s           cn30xx;
	struct cvmx_mio_uartx_tfr_s           cn31xx;
	struct cvmx_mio_uartx_tfr_s           cn38xx;
	struct cvmx_mio_uartx_tfr_s           cn38xxp2;
	struct cvmx_mio_uartx_tfr_s           cn50xx;
	struct cvmx_mio_uartx_tfr_s           cn52xx;
	struct cvmx_mio_uartx_tfr_s           cn52xxp1;
	struct cvmx_mio_uartx_tfr_s           cn56xx;
	struct cvmx_mio_uartx_tfr_s           cn56xxp1;
	struct cvmx_mio_uartx_tfr_s           cn58xx;
	struct cvmx_mio_uartx_tfr_s           cn58xxp1;
	struct cvmx_mio_uartx_tfr_s           cn63xx;
	struct cvmx_mio_uartx_tfr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_tfr cvmx_mio_uartx_tfr_t;
typedef cvmx_mio_uartx_tfr_t cvmx_uart_tfr_t;

/**
 * cvmx_mio_uart#_thr
 *
 * MIO_UARTX_THR = MIO UARTX Transmit Holding Register
 *
 * Transmit Holding Register (THR) is a write-only register that contains data to be transmitted on the
 * serial output port (sout). Data can be written to the THR any time that the THR Empty (THRE) bit of
 * the Line Status Register (LSR) is set.
 *
 * If FIFOs are not enabled and THRE is set, writing a single character to the THR clears the THRE. Any
 * additional writes to the THR before the THRE is set again causes the THR data to be overwritten.
 *
 * If FIFOs are enabled and THRE is set (and Programmable THRE mode disabled), 64 characters of data may
 * be written to the THR before the FIFO is full. Any attempt to write data when the FIFO is full results
 * in the write data being lost.
 *
 * Note: The Divisor Latch Address Bit (DLAB) of the Line Control Register (LCR) must be clear to access
 * this register.
 *
 * Note: The address below is an alias to simplify these CSR descriptions. It should be known that the
 * RBR, THR, and DLL registers are the same.
 */
union cvmx_mio_uartx_thr
{
	uint64_t u64;
	struct cvmx_mio_uartx_thr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t thr                          : 8;  /**< Transmit Holding Register */
#else
	uint64_t thr                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uartx_thr_s           cn30xx;
	struct cvmx_mio_uartx_thr_s           cn31xx;
	struct cvmx_mio_uartx_thr_s           cn38xx;
	struct cvmx_mio_uartx_thr_s           cn38xxp2;
	struct cvmx_mio_uartx_thr_s           cn50xx;
	struct cvmx_mio_uartx_thr_s           cn52xx;
	struct cvmx_mio_uartx_thr_s           cn52xxp1;
	struct cvmx_mio_uartx_thr_s           cn56xx;
	struct cvmx_mio_uartx_thr_s           cn56xxp1;
	struct cvmx_mio_uartx_thr_s           cn58xx;
	struct cvmx_mio_uartx_thr_s           cn58xxp1;
	struct cvmx_mio_uartx_thr_s           cn63xx;
	struct cvmx_mio_uartx_thr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_thr cvmx_mio_uartx_thr_t;
typedef cvmx_mio_uartx_thr_t cvmx_uart_thr_t;

/**
 * cvmx_mio_uart#_usr
 *
 * MIO_UARTX_USR = MIO UARTX UART Status Register
 *
 * The UART Status Register (USR) contains UART status information.
 *
 * USR bit 0 is the BUSY bit.  When set this bit indicates that a serial transfer is in progress, when
 * clear it indicates that the UART is idle or inactive.
 *
 * Note: In PASS3, the BUSY bit will always be clear.
 *
 * USR bits 1-4 indicate the following FIFO status: TX FIFO Not Full (TFNF), TX FIFO Empty (TFE), RX
 * FIFO Not Empty (RFNE), and RX FIFO Full (RFF).
 */
union cvmx_mio_uartx_usr
{
	uint64_t u64;
	struct cvmx_mio_uartx_usr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_5_63                : 59;
	uint64_t rff                          : 1;  /**< RX FIFO Full */
	uint64_t rfne                         : 1;  /**< RX FIFO Not Empty */
	uint64_t tfe                          : 1;  /**< TX FIFO Empty */
	uint64_t tfnf                         : 1;  /**< TX FIFO Not Full */
	uint64_t busy                         : 1;  /**< Busy bit (always 0 in PASS3) */
#else
	uint64_t busy                         : 1;
	uint64_t tfnf                         : 1;
	uint64_t tfe                          : 1;
	uint64_t rfne                         : 1;
	uint64_t rff                          : 1;
	uint64_t reserved_5_63                : 59;
#endif
	} s;
	struct cvmx_mio_uartx_usr_s           cn30xx;
	struct cvmx_mio_uartx_usr_s           cn31xx;
	struct cvmx_mio_uartx_usr_s           cn38xx;
	struct cvmx_mio_uartx_usr_s           cn38xxp2;
	struct cvmx_mio_uartx_usr_s           cn50xx;
	struct cvmx_mio_uartx_usr_s           cn52xx;
	struct cvmx_mio_uartx_usr_s           cn52xxp1;
	struct cvmx_mio_uartx_usr_s           cn56xx;
	struct cvmx_mio_uartx_usr_s           cn56xxp1;
	struct cvmx_mio_uartx_usr_s           cn58xx;
	struct cvmx_mio_uartx_usr_s           cn58xxp1;
	struct cvmx_mio_uartx_usr_s           cn63xx;
	struct cvmx_mio_uartx_usr_s           cn63xxp1;
};
typedef union cvmx_mio_uartx_usr cvmx_mio_uartx_usr_t;
typedef cvmx_mio_uartx_usr_t cvmx_uart_usr_t;

/**
 * cvmx_mio_uart2_dlh
 */
union cvmx_mio_uart2_dlh
{
	uint64_t u64;
	struct cvmx_mio_uart2_dlh_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t dlh                          : 8;  /**< Divisor Latch High Register */
#else
	uint64_t dlh                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_dlh_s           cn52xx;
	struct cvmx_mio_uart2_dlh_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_dlh cvmx_mio_uart2_dlh_t;

/**
 * cvmx_mio_uart2_dll
 */
union cvmx_mio_uart2_dll
{
	uint64_t u64;
	struct cvmx_mio_uart2_dll_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t dll                          : 8;  /**< Divisor Latch Low Register */
#else
	uint64_t dll                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_dll_s           cn52xx;
	struct cvmx_mio_uart2_dll_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_dll cvmx_mio_uart2_dll_t;

/**
 * cvmx_mio_uart2_far
 */
union cvmx_mio_uart2_far
{
	uint64_t u64;
	struct cvmx_mio_uart2_far_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t far                          : 1;  /**< FIFO Access Register */
#else
	uint64_t far                          : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uart2_far_s           cn52xx;
	struct cvmx_mio_uart2_far_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_far cvmx_mio_uart2_far_t;

/**
 * cvmx_mio_uart2_fcr
 */
union cvmx_mio_uart2_fcr
{
	uint64_t u64;
	struct cvmx_mio_uart2_fcr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t rxtrig                       : 2;  /**< RX Trigger */
	uint64_t txtrig                       : 2;  /**< TX Trigger */
	uint64_t reserved_3_3                 : 1;
	uint64_t txfr                         : 1;  /**< TX FIFO reset */
	uint64_t rxfr                         : 1;  /**< RX FIFO reset */
	uint64_t en                           : 1;  /**< FIFO enable */
#else
	uint64_t en                           : 1;
	uint64_t rxfr                         : 1;
	uint64_t txfr                         : 1;
	uint64_t reserved_3_3                 : 1;
	uint64_t txtrig                       : 2;
	uint64_t rxtrig                       : 2;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_fcr_s           cn52xx;
	struct cvmx_mio_uart2_fcr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_fcr cvmx_mio_uart2_fcr_t;

/**
 * cvmx_mio_uart2_htx
 */
union cvmx_mio_uart2_htx
{
	uint64_t u64;
	struct cvmx_mio_uart2_htx_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t htx                          : 1;  /**< Halt TX */
#else
	uint64_t htx                          : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uart2_htx_s           cn52xx;
	struct cvmx_mio_uart2_htx_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_htx cvmx_mio_uart2_htx_t;

/**
 * cvmx_mio_uart2_ier
 */
union cvmx_mio_uart2_ier
{
	uint64_t u64;
	struct cvmx_mio_uart2_ier_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t ptime                        : 1;  /**< Programmable THRE Interrupt mode enable */
	uint64_t reserved_4_6                 : 3;
	uint64_t edssi                        : 1;  /**< Enable Modem Status Interrupt */
	uint64_t elsi                         : 1;  /**< Enable Receiver Line Status Interrupt */
	uint64_t etbei                        : 1;  /**< Enable Transmitter Holding Register Empty Interrupt */
	uint64_t erbfi                        : 1;  /**< Enable Received Data Available Interrupt */
#else
	uint64_t erbfi                        : 1;
	uint64_t etbei                        : 1;
	uint64_t elsi                         : 1;
	uint64_t edssi                        : 1;
	uint64_t reserved_4_6                 : 3;
	uint64_t ptime                        : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_ier_s           cn52xx;
	struct cvmx_mio_uart2_ier_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_ier cvmx_mio_uart2_ier_t;

/**
 * cvmx_mio_uart2_iir
 */
union cvmx_mio_uart2_iir
{
	uint64_t u64;
	struct cvmx_mio_uart2_iir_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t fen                          : 2;  /**< FIFO-enabled bits */
	uint64_t reserved_4_5                 : 2;
	uint64_t iid                          : 4;  /**< Interrupt ID */
#else
	uint64_t iid                          : 4;
	uint64_t reserved_4_5                 : 2;
	uint64_t fen                          : 2;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_iir_s           cn52xx;
	struct cvmx_mio_uart2_iir_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_iir cvmx_mio_uart2_iir_t;

/**
 * cvmx_mio_uart2_lcr
 */
union cvmx_mio_uart2_lcr
{
	uint64_t u64;
	struct cvmx_mio_uart2_lcr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t dlab                         : 1;  /**< Divisor Latch Address bit */
	uint64_t brk                          : 1;  /**< Break Control bit */
	uint64_t reserved_5_5                 : 1;
	uint64_t eps                          : 1;  /**< Even Parity Select bit */
	uint64_t pen                          : 1;  /**< Parity Enable bit */
	uint64_t stop                         : 1;  /**< Stop Control bit */
	uint64_t cls                          : 2;  /**< Character Length Select */
#else
	uint64_t cls                          : 2;
	uint64_t stop                         : 1;
	uint64_t pen                          : 1;
	uint64_t eps                          : 1;
	uint64_t reserved_5_5                 : 1;
	uint64_t brk                          : 1;
	uint64_t dlab                         : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_lcr_s           cn52xx;
	struct cvmx_mio_uart2_lcr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_lcr cvmx_mio_uart2_lcr_t;

/**
 * cvmx_mio_uart2_lsr
 */
union cvmx_mio_uart2_lsr
{
	uint64_t u64;
	struct cvmx_mio_uart2_lsr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t ferr                         : 1;  /**< Error in Receiver FIFO bit */
	uint64_t temt                         : 1;  /**< Transmitter Empty bit */
	uint64_t thre                         : 1;  /**< Transmitter Holding Register Empty bit */
	uint64_t bi                           : 1;  /**< Break Interrupt bit */
	uint64_t fe                           : 1;  /**< Framing Error bit */
	uint64_t pe                           : 1;  /**< Parity Error bit */
	uint64_t oe                           : 1;  /**< Overrun Error bit */
	uint64_t dr                           : 1;  /**< Data Ready bit */
#else
	uint64_t dr                           : 1;
	uint64_t oe                           : 1;
	uint64_t pe                           : 1;
	uint64_t fe                           : 1;
	uint64_t bi                           : 1;
	uint64_t thre                         : 1;
	uint64_t temt                         : 1;
	uint64_t ferr                         : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_lsr_s           cn52xx;
	struct cvmx_mio_uart2_lsr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_lsr cvmx_mio_uart2_lsr_t;

/**
 * cvmx_mio_uart2_mcr
 */
union cvmx_mio_uart2_mcr
{
	uint64_t u64;
	struct cvmx_mio_uart2_mcr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_6_63                : 58;
	uint64_t afce                         : 1;  /**< Auto Flow Control Enable bit */
	uint64_t loop                         : 1;  /**< Loopback bit */
	uint64_t out2                         : 1;  /**< OUT2 output bit */
	uint64_t out1                         : 1;  /**< OUT1 output bit */
	uint64_t rts                          : 1;  /**< Request To Send output bit */
	uint64_t dtr                          : 1;  /**< Data Terminal Ready output bit */
#else
	uint64_t dtr                          : 1;
	uint64_t rts                          : 1;
	uint64_t out1                         : 1;
	uint64_t out2                         : 1;
	uint64_t loop                         : 1;
	uint64_t afce                         : 1;
	uint64_t reserved_6_63                : 58;
#endif
	} s;
	struct cvmx_mio_uart2_mcr_s           cn52xx;
	struct cvmx_mio_uart2_mcr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_mcr cvmx_mio_uart2_mcr_t;

/**
 * cvmx_mio_uart2_msr
 */
union cvmx_mio_uart2_msr
{
	uint64_t u64;
	struct cvmx_mio_uart2_msr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t dcd                          : 1;  /**< Data Carrier Detect input bit */
	uint64_t ri                           : 1;  /**< Ring Indicator input bit */
	uint64_t dsr                          : 1;  /**< Data Set Ready input bit */
	uint64_t cts                          : 1;  /**< Clear To Send input bit */
	uint64_t ddcd                         : 1;  /**< Delta Data Carrier Detect bit */
	uint64_t teri                         : 1;  /**< Trailing Edge of Ring Indicator bit */
	uint64_t ddsr                         : 1;  /**< Delta Data Set Ready bit */
	uint64_t dcts                         : 1;  /**< Delta Clear To Send bit */
#else
	uint64_t dcts                         : 1;
	uint64_t ddsr                         : 1;
	uint64_t teri                         : 1;
	uint64_t ddcd                         : 1;
	uint64_t cts                          : 1;
	uint64_t dsr                          : 1;
	uint64_t ri                           : 1;
	uint64_t dcd                          : 1;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_msr_s           cn52xx;
	struct cvmx_mio_uart2_msr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_msr cvmx_mio_uart2_msr_t;

/**
 * cvmx_mio_uart2_rbr
 */
union cvmx_mio_uart2_rbr
{
	uint64_t u64;
	struct cvmx_mio_uart2_rbr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t rbr                          : 8;  /**< Receive Buffer Register */
#else
	uint64_t rbr                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_rbr_s           cn52xx;
	struct cvmx_mio_uart2_rbr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_rbr cvmx_mio_uart2_rbr_t;

/**
 * cvmx_mio_uart2_rfl
 */
union cvmx_mio_uart2_rfl
{
	uint64_t u64;
	struct cvmx_mio_uart2_rfl_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_7_63                : 57;
	uint64_t rfl                          : 7;  /**< Receive FIFO Level Register */
#else
	uint64_t rfl                          : 7;
	uint64_t reserved_7_63                : 57;
#endif
	} s;
	struct cvmx_mio_uart2_rfl_s           cn52xx;
	struct cvmx_mio_uart2_rfl_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_rfl cvmx_mio_uart2_rfl_t;

/**
 * cvmx_mio_uart2_rfw
 */
union cvmx_mio_uart2_rfw
{
	uint64_t u64;
	struct cvmx_mio_uart2_rfw_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_10_63               : 54;
	uint64_t rffe                         : 1;  /**< Receive FIFO Framing Error */
	uint64_t rfpe                         : 1;  /**< Receive FIFO Parity Error */
	uint64_t rfwd                         : 8;  /**< Receive FIFO Write Data */
#else
	uint64_t rfwd                         : 8;
	uint64_t rfpe                         : 1;
	uint64_t rffe                         : 1;
	uint64_t reserved_10_63               : 54;
#endif
	} s;
	struct cvmx_mio_uart2_rfw_s           cn52xx;
	struct cvmx_mio_uart2_rfw_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_rfw cvmx_mio_uart2_rfw_t;

/**
 * cvmx_mio_uart2_sbcr
 */
union cvmx_mio_uart2_sbcr
{
	uint64_t u64;
	struct cvmx_mio_uart2_sbcr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t sbcr                         : 1;  /**< Shadow Break Control */
#else
	uint64_t sbcr                         : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uart2_sbcr_s          cn52xx;
	struct cvmx_mio_uart2_sbcr_s          cn52xxp1;
};
typedef union cvmx_mio_uart2_sbcr cvmx_mio_uart2_sbcr_t;

/**
 * cvmx_mio_uart2_scr
 */
union cvmx_mio_uart2_scr
{
	uint64_t u64;
	struct cvmx_mio_uart2_scr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t scr                          : 8;  /**< Scratchpad Register */
#else
	uint64_t scr                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_scr_s           cn52xx;
	struct cvmx_mio_uart2_scr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_scr cvmx_mio_uart2_scr_t;

/**
 * cvmx_mio_uart2_sfe
 */
union cvmx_mio_uart2_sfe
{
	uint64_t u64;
	struct cvmx_mio_uart2_sfe_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t sfe                          : 1;  /**< Shadow FIFO Enable */
#else
	uint64_t sfe                          : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uart2_sfe_s           cn52xx;
	struct cvmx_mio_uart2_sfe_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_sfe cvmx_mio_uart2_sfe_t;

/**
 * cvmx_mio_uart2_srr
 */
union cvmx_mio_uart2_srr
{
	uint64_t u64;
	struct cvmx_mio_uart2_srr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_3_63                : 61;
	uint64_t stfr                         : 1;  /**< Shadow TX FIFO Reset */
	uint64_t srfr                         : 1;  /**< Shadow RX FIFO Reset */
	uint64_t usr                          : 1;  /**< UART Soft Reset */
#else
	uint64_t usr                          : 1;
	uint64_t srfr                         : 1;
	uint64_t stfr                         : 1;
	uint64_t reserved_3_63                : 61;
#endif
	} s;
	struct cvmx_mio_uart2_srr_s           cn52xx;
	struct cvmx_mio_uart2_srr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_srr cvmx_mio_uart2_srr_t;

/**
 * cvmx_mio_uart2_srt
 */
union cvmx_mio_uart2_srt
{
	uint64_t u64;
	struct cvmx_mio_uart2_srt_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t srt                          : 2;  /**< Shadow RX Trigger */
#else
	uint64_t srt                          : 2;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_uart2_srt_s           cn52xx;
	struct cvmx_mio_uart2_srt_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_srt cvmx_mio_uart2_srt_t;

/**
 * cvmx_mio_uart2_srts
 */
union cvmx_mio_uart2_srts
{
	uint64_t u64;
	struct cvmx_mio_uart2_srts_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_1_63                : 63;
	uint64_t srts                         : 1;  /**< Shadow Request To Send */
#else
	uint64_t srts                         : 1;
	uint64_t reserved_1_63                : 63;
#endif
	} s;
	struct cvmx_mio_uart2_srts_s          cn52xx;
	struct cvmx_mio_uart2_srts_s          cn52xxp1;
};
typedef union cvmx_mio_uart2_srts cvmx_mio_uart2_srts_t;

/**
 * cvmx_mio_uart2_stt
 */
union cvmx_mio_uart2_stt
{
	uint64_t u64;
	struct cvmx_mio_uart2_stt_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_2_63                : 62;
	uint64_t stt                          : 2;  /**< Shadow TX Trigger */
#else
	uint64_t stt                          : 2;
	uint64_t reserved_2_63                : 62;
#endif
	} s;
	struct cvmx_mio_uart2_stt_s           cn52xx;
	struct cvmx_mio_uart2_stt_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_stt cvmx_mio_uart2_stt_t;

/**
 * cvmx_mio_uart2_tfl
 */
union cvmx_mio_uart2_tfl
{
	uint64_t u64;
	struct cvmx_mio_uart2_tfl_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_7_63                : 57;
	uint64_t tfl                          : 7;  /**< Transmit FIFO Level Register */
#else
	uint64_t tfl                          : 7;
	uint64_t reserved_7_63                : 57;
#endif
	} s;
	struct cvmx_mio_uart2_tfl_s           cn52xx;
	struct cvmx_mio_uart2_tfl_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_tfl cvmx_mio_uart2_tfl_t;

/**
 * cvmx_mio_uart2_tfr
 */
union cvmx_mio_uart2_tfr
{
	uint64_t u64;
	struct cvmx_mio_uart2_tfr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t tfr                          : 8;  /**< Transmit FIFO Read Register */
#else
	uint64_t tfr                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_tfr_s           cn52xx;
	struct cvmx_mio_uart2_tfr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_tfr cvmx_mio_uart2_tfr_t;

/**
 * cvmx_mio_uart2_thr
 */
union cvmx_mio_uart2_thr
{
	uint64_t u64;
	struct cvmx_mio_uart2_thr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_8_63                : 56;
	uint64_t thr                          : 8;  /**< Transmit Holding Register */
#else
	uint64_t thr                          : 8;
	uint64_t reserved_8_63                : 56;
#endif
	} s;
	struct cvmx_mio_uart2_thr_s           cn52xx;
	struct cvmx_mio_uart2_thr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_thr cvmx_mio_uart2_thr_t;

/**
 * cvmx_mio_uart2_usr
 */
union cvmx_mio_uart2_usr
{
	uint64_t u64;
	struct cvmx_mio_uart2_usr_s
	{
#if __BYTE_ORDER == __BIG_ENDIAN
	uint64_t reserved_5_63                : 59;
	uint64_t rff                          : 1;  /**< RX FIFO Full */
	uint64_t rfne                         : 1;  /**< RX FIFO Not Empty */
	uint64_t tfe                          : 1;  /**< TX FIFO Empty */
	uint64_t tfnf                         : 1;  /**< TX FIFO Not Full */
	uint64_t busy                         : 1;  /**< Busy bit (always 0 in PASS3) */
#else
	uint64_t busy                         : 1;
	uint64_t tfnf                         : 1;
	uint64_t tfe                          : 1;
	uint64_t rfne                         : 1;
	uint64_t rff                          : 1;
	uint64_t reserved_5_63                : 59;
#endif
	} s;
	struct cvmx_mio_uart2_usr_s           cn52xx;
	struct cvmx_mio_uart2_usr_s           cn52xxp1;
};
typedef union cvmx_mio_uart2_usr cvmx_mio_uart2_usr_t;

#endif
