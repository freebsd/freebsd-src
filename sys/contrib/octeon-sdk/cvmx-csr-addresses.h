/***********************license start***************
 *  Copyright (c) 2003-2009 Cavium Networks (support@cavium.com). All rights
 *  reserved.
 *
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions are
 *  met:
 *
 *      * Redistributions of source code must retain the above copyright
 *        notice, this list of conditions and the following disclaimer.
 *
 *      * Redistributions in binary form must reproduce the above
 *        copyright notice, this list of conditions and the following
 *        disclaimer in the documentation and/or other materials provided
 *        with the distribution.
 *
 *      * Neither the name of Cavium Networks nor the names of
 *        its contributors may be used to endorse or promote products
 *        derived from this software without specific prior written
 *        permission.
 *
 *  TO THE MAXIMUM EXTENT PERMITTED BY LAW, THE SOFTWARE IS PROVIDED "AS IS"
 *  AND WITH ALL FAULTS AND CAVIUM NETWORKS MAKES NO PROMISES, REPRESENTATIONS
 *  OR WARRANTIES, EITHER EXPRESS, IMPLIED, STATUTORY, OR OTHERWISE, WITH
 *  RESPECT TO THE SOFTWARE, INCLUDING ITS CONDITION, ITS CONFORMITY TO ANY
 *  REPRESENTATION OR DESCRIPTION, OR THE EXISTENCE OF ANY LATENT OR PATENT
 *  DEFECTS, AND CAVIUM SPECIFICALLY DISCLAIMS ALL IMPLIED (IF ANY) WARRANTIES
 *  OF TITLE, MERCHANTABILITY, NONINFRINGEMENT, FITNESS FOR A PARTICULAR
 *  PURPOSE, LACK OF VIRUSES, ACCURACY OR COMPLETENESS, QUIET ENJOYMENT, QUIET
 *  POSSESSION OR CORRESPONDENCE TO DESCRIPTION.  THE ENTIRE RISK ARISING OUT
 *  OF USE OR PERFORMANCE OF THE SOFTWARE LIES WITH YOU.
 *
 *
 *  For any questions regarding licensing please contact marketing@caviumnetworks.com
 *
 ***********************license end**************************************/

/**
 * @file
 *
 * Configuration and status register (CSR) address and for
 * Octeon. Include cvmx-csr.h instead of this file directly.
 *
 * This file is auto generated. Do not edit.
 *
 * <hr>$Revision: 41586 $<hr>
 *
 */
#ifndef __CVMX_CSR_ADDRESSES_H__
#define __CVMX_CSR_ADDRESSES_H__

#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
#include "cvmx-warn.h"
#endif

#define CVMX_AGL_GMX_BAD_REG CVMX_AGL_GMX_BAD_REG_FUNC()
static inline uint64_t CVMX_AGL_GMX_BAD_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_BAD_REG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000518ull);
}

#define CVMX_AGL_GMX_BIST CVMX_AGL_GMX_BIST_FUNC()
static inline uint64_t CVMX_AGL_GMX_BIST_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_BIST not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000400ull);
}

#define CVMX_AGL_GMX_DRV_CTL CVMX_AGL_GMX_DRV_CTL_FUNC()
static inline uint64_t CVMX_AGL_GMX_DRV_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_DRV_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00007F0ull);
}

#define CVMX_AGL_GMX_INF_MODE CVMX_AGL_GMX_INF_MODE_FUNC()
static inline uint64_t CVMX_AGL_GMX_INF_MODE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_INF_MODE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00007F8ull);
}

static inline uint64_t CVMX_AGL_GMX_PRTX_CFG(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_PRTX_CFG(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000010ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_ADR_CAM0(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_ADR_CAM0(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000180ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_ADR_CAM1(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_ADR_CAM1(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000188ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_ADR_CAM2(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_ADR_CAM2(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000190ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_ADR_CAM3(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_ADR_CAM3(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000198ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_ADR_CAM4(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_ADR_CAM4(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00001A0ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_ADR_CAM5(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_ADR_CAM5(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00001A8ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_ADR_CAM_EN(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_ADR_CAM_EN(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000108ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_ADR_CTL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_ADR_CTL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000100ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_DECISION(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_DECISION(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000040ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_FRM_CHK(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_FRM_CHK(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000020ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_FRM_CTL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_FRM_CTL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000018ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_FRM_MAX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_FRM_MAX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000030ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_FRM_MIN(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_FRM_MIN(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000028ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_IFG(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_IFG(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000058ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_INT_EN(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_INT_EN(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000008ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_INT_REG(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_INT_REG(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000000ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_JABBER(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_JABBER(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000038ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_PAUSE_DROP_TIME(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_PAUSE_DROP_TIME(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000068ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_CTL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_CTL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000050ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_OCTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_OCTS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000088ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_OCTS_CTL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_OCTS_CTL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000098ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_OCTS_DMAC(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_OCTS_DMAC(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00000A8ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_OCTS_DRP(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_OCTS_DRP(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00000B8ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_PKTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_PKTS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000080ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_PKTS_BAD(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_PKTS_BAD(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00000C0ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_PKTS_CTL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_PKTS_CTL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000090ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_PKTS_DMAC(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_PKTS_DMAC(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00000A0ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_STATS_PKTS_DRP(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_STATS_PKTS_DRP(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00000B0ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RXX_UDD_SKP(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RXX_UDD_SKP(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000048ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_RX_BP_DROPX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RX_BP_DROPX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000420ull) + (offset&1)*8;
}

static inline uint64_t CVMX_AGL_GMX_RX_BP_OFFX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RX_BP_OFFX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000460ull) + (offset&1)*8;
}

static inline uint64_t CVMX_AGL_GMX_RX_BP_ONX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_RX_BP_ONX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000440ull) + (offset&1)*8;
}

#define CVMX_AGL_GMX_RX_PRT_INFO CVMX_AGL_GMX_RX_PRT_INFO_FUNC()
static inline uint64_t CVMX_AGL_GMX_RX_PRT_INFO_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_RX_PRT_INFO not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00004E8ull);
}

#define CVMX_AGL_GMX_RX_TX_STATUS CVMX_AGL_GMX_RX_TX_STATUS_FUNC()
static inline uint64_t CVMX_AGL_GMX_RX_TX_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_RX_TX_STATUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00007E8ull);
}

static inline uint64_t CVMX_AGL_GMX_SMACX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_SMACX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000230ull) + (offset&1)*2048;
}

#define CVMX_AGL_GMX_STAT_BP CVMX_AGL_GMX_STAT_BP_FUNC()
static inline uint64_t CVMX_AGL_GMX_STAT_BP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_STAT_BP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000520ull);
}

static inline uint64_t CVMX_AGL_GMX_TXX_APPEND(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_APPEND(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000218ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_CTL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_CTL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000270ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_MIN_PKT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_MIN_PKT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000240ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_PAUSE_PKT_INTERVAL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_PAUSE_PKT_INTERVAL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000248ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_PAUSE_PKT_TIME(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_PAUSE_PKT_TIME(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000238ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_PAUSE_TOGO(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_PAUSE_TOGO(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000258ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_PAUSE_ZERO(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_PAUSE_ZERO(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000260ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_SOFT_PAUSE(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_SOFT_PAUSE(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000250ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT0(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT0(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000280ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT1(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT1(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000288ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT2(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT2(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000290ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT3(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT3(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000298ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT4(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT4(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00002A0ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT5(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT5(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00002A8ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT6(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT6(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00002B0ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT7(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT7(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00002B8ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT8(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT8(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00002C0ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STAT9(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STAT9(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00002C8ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_STATS_CTL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_STATS_CTL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000268ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_AGL_GMX_TXX_THRESH(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_AGL_GMX_TXX_THRESH(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000210ull) + (offset&1)*2048;
}

#define CVMX_AGL_GMX_TX_BP CVMX_AGL_GMX_TX_BP_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_BP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_BP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00004D0ull);
}

#define CVMX_AGL_GMX_TX_COL_ATTEMPT CVMX_AGL_GMX_TX_COL_ATTEMPT_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_COL_ATTEMPT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_COL_ATTEMPT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000498ull);
}

#define CVMX_AGL_GMX_TX_IFG CVMX_AGL_GMX_TX_IFG_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_IFG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_IFG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000488ull);
}

#define CVMX_AGL_GMX_TX_INT_EN CVMX_AGL_GMX_TX_INT_EN_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_INT_EN_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_INT_EN not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000508ull);
}

#define CVMX_AGL_GMX_TX_INT_REG CVMX_AGL_GMX_TX_INT_REG_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_INT_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_INT_REG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000500ull);
}

#define CVMX_AGL_GMX_TX_JAM CVMX_AGL_GMX_TX_JAM_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_JAM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_JAM not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E0000490ull);
}

#define CVMX_AGL_GMX_TX_LFSR CVMX_AGL_GMX_TX_LFSR_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_LFSR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_LFSR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00004F8ull);
}

#define CVMX_AGL_GMX_TX_OVR_BP CVMX_AGL_GMX_TX_OVR_BP_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_OVR_BP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_OVR_BP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00004C8ull);
}

#define CVMX_AGL_GMX_TX_PAUSE_PKT_DMAC CVMX_AGL_GMX_TX_PAUSE_PKT_DMAC_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_PAUSE_PKT_DMAC_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_PAUSE_PKT_DMAC not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00004A0ull);
}

#define CVMX_AGL_GMX_TX_PAUSE_PKT_TYPE CVMX_AGL_GMX_TX_PAUSE_PKT_TYPE_FUNC()
static inline uint64_t CVMX_AGL_GMX_TX_PAUSE_PKT_TYPE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_AGL_GMX_TX_PAUSE_PKT_TYPE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800E00004A8ull);
}

#define CVMX_ASX0_DBG_DATA_DRV CVMX_ASX0_DBG_DATA_DRV_FUNC()
static inline uint64_t CVMX_ASX0_DBG_DATA_DRV_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ASX0_DBG_DATA_DRV not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000208ull);
}

#define CVMX_ASX0_DBG_DATA_ENABLE CVMX_ASX0_DBG_DATA_ENABLE_FUNC()
static inline uint64_t CVMX_ASX0_DBG_DATA_ENABLE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ASX0_DBG_DATA_ENABLE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000200ull);
}

static inline uint64_t CVMX_ASXX_GMII_RX_CLK_SET(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_ASXX_GMII_RX_CLK_SET(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000180ull) + (block_id&0)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_GMII_RX_DAT_SET(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_ASXX_GMII_RX_DAT_SET(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000188ull) + (block_id&0)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_INT_EN(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_INT_EN(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000018ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_INT_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_INT_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000010ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_MII_RX_DAT_SET(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_ASXX_MII_RX_DAT_SET(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000190ull) + (block_id&0)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_PRT_LOOP(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_PRT_LOOP(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000040ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_BYPASS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_BYPASS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000248ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_BYPASS_SETTING(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_BYPASS_SETTING(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000250ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_COMP(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_COMP(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000220ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_DATA_DRV(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_DATA_DRV(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000218ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_FCRAM_MODE(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_FCRAM_MODE(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000210ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_NCTL_STRONG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_NCTL_STRONG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000230ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_NCTL_WEAK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_NCTL_WEAK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000240ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_PCTL_STRONG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_PCTL_STRONG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000228ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_PCTL_WEAK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_PCTL_WEAK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000238ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RLD_SETTING(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RLD_SETTING(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000258ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RX_CLK_SETX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_ASXX_RX_CLK_SETX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000020ull) + ((offset&3) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_ASXX_RX_PRT_EN(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RX_PRT_EN(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000000ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RX_WOL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RX_WOL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000100ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RX_WOL_MSK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RX_WOL_MSK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000108ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RX_WOL_POWOK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RX_WOL_POWOK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000118ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_RX_WOL_SIG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_RX_WOL_SIG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000110ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_TX_CLK_SETX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_ASXX_TX_CLK_SETX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000048ull) + ((offset&3) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_ASXX_TX_COMP_BYP(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_TX_COMP_BYP(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000068ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_ASXX_TX_HI_WATERX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_ASXX_TX_HI_WATERX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000080ull) + ((offset&3) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_ASXX_TX_PRT_EN(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_ASXX_TX_PRT_EN(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000008ull) + (block_id&1)*0x8000000ull;
}

#define CVMX_CIU_BIST CVMX_CIU_BIST_FUNC()
static inline uint64_t CVMX_CIU_BIST_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000730ull);
}

#define CVMX_CIU_DINT CVMX_CIU_DINT_FUNC()
static inline uint64_t CVMX_CIU_DINT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000720ull);
}

#define CVMX_CIU_FUSE CVMX_CIU_FUSE_FUNC()
static inline uint64_t CVMX_CIU_FUSE_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000728ull);
}

#define CVMX_CIU_GSTOP CVMX_CIU_GSTOP_FUNC()
static inline uint64_t CVMX_CIU_GSTOP_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000710ull);
}

static inline uint64_t CVMX_CIU_INTX_EN0(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 23) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7) || (offset == 32)))))
        cvmx_warn("CVMX_CIU_INTX_EN0(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000200ull) + (offset&63)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN0_W1C(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 23) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 32)))))
        cvmx_warn("CVMX_CIU_INTX_EN0_W1C(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000002200ull) + (offset&63)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN0_W1S(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 23) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 32)))))
        cvmx_warn("CVMX_CIU_INTX_EN0_W1S(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000006200ull) + (offset&63)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN1(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 23) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7) || (offset == 32)))))
        cvmx_warn("CVMX_CIU_INTX_EN1(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000208ull) + (offset&63)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN1_W1C(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 23) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 32)))))
        cvmx_warn("CVMX_CIU_INTX_EN1_W1C(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000002208ull) + (offset&63)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN1_W1S(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 23) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 32)))))
        cvmx_warn("CVMX_CIU_INTX_EN1_W1S(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000006208ull) + (offset&63)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN4_0(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_CIU_INTX_EN4_0(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000C80ull) + (offset&15)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN4_0_W1C(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15)))))
        cvmx_warn("CVMX_CIU_INTX_EN4_0_W1C(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000002C80ull) + (offset&15)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN4_0_W1S(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15)))))
        cvmx_warn("CVMX_CIU_INTX_EN4_0_W1S(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000006C80ull) + (offset&15)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN4_1(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_CIU_INTX_EN4_1(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000C88ull) + (offset&15)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN4_1_W1C(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15)))))
        cvmx_warn("CVMX_CIU_INTX_EN4_1_W1C(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000002C88ull) + (offset&15)*16;
}

static inline uint64_t CVMX_CIU_INTX_EN4_1_W1S(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15)))))
        cvmx_warn("CVMX_CIU_INTX_EN4_1_W1S(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000006C88ull) + (offset&15)*16;
}

static inline uint64_t CVMX_CIU_INTX_SUM0(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 23) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7) || (offset == 32)))))
        cvmx_warn("CVMX_CIU_INTX_SUM0(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000000ull) + (offset&63)*8;
}

static inline uint64_t CVMX_CIU_INTX_SUM4(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_CIU_INTX_SUM4(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000C00ull) + (offset&15)*8;
}

#define CVMX_CIU_INT_SUM1 CVMX_CIU_INT_SUM1_FUNC()
static inline uint64_t CVMX_CIU_INT_SUM1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000108ull);
}

static inline uint64_t CVMX_CIU_MBOX_CLRX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_CIU_MBOX_CLRX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000680ull) + (offset&15)*8;
}

static inline uint64_t CVMX_CIU_MBOX_SETX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_CIU_MBOX_SETX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000600ull) + (offset&15)*8;
}

#define CVMX_CIU_NMI CVMX_CIU_NMI_FUNC()
static inline uint64_t CVMX_CIU_NMI_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000718ull);
}

#define CVMX_CIU_PCI_INTA CVMX_CIU_PCI_INTA_FUNC()
static inline uint64_t CVMX_CIU_PCI_INTA_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000750ull);
}

#define CVMX_CIU_PP_DBG CVMX_CIU_PP_DBG_FUNC()
static inline uint64_t CVMX_CIU_PP_DBG_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000708ull);
}

static inline uint64_t CVMX_CIU_PP_POKEX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_CIU_PP_POKEX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000580ull) + (offset&15)*8;
}

#define CVMX_CIU_PP_RST CVMX_CIU_PP_RST_FUNC()
static inline uint64_t CVMX_CIU_PP_RST_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000700ull);
}

#define CVMX_CIU_QLM_DCOK CVMX_CIU_QLM_DCOK_FUNC()
static inline uint64_t CVMX_CIU_QLM_DCOK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_CIU_QLM_DCOK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000760ull);
}

#define CVMX_CIU_QLM_JTGC CVMX_CIU_QLM_JTGC_FUNC()
static inline uint64_t CVMX_CIU_QLM_JTGC_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_CIU_QLM_JTGC not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000768ull);
}

#define CVMX_CIU_QLM_JTGD CVMX_CIU_QLM_JTGD_FUNC()
static inline uint64_t CVMX_CIU_QLM_JTGD_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_CIU_QLM_JTGD not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000770ull);
}

#define CVMX_CIU_SOFT_BIST CVMX_CIU_SOFT_BIST_FUNC()
static inline uint64_t CVMX_CIU_SOFT_BIST_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000738ull);
}

#define CVMX_CIU_SOFT_PRST CVMX_CIU_SOFT_PRST_FUNC()
static inline uint64_t CVMX_CIU_SOFT_PRST_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000748ull);
}

#define CVMX_CIU_SOFT_PRST1 CVMX_CIU_SOFT_PRST1_FUNC()
static inline uint64_t CVMX_CIU_SOFT_PRST1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_CIU_SOFT_PRST1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000758ull);
}

#define CVMX_CIU_SOFT_RST CVMX_CIU_SOFT_RST_FUNC()
static inline uint64_t CVMX_CIU_SOFT_RST_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000740ull);
}

static inline uint64_t CVMX_CIU_TIMX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_CIU_TIMX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000480ull) + (offset&3)*8;
}

static inline uint64_t CVMX_CIU_WDOGX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_CIU_WDOGX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000500ull) + (offset&15)*8;
}

#define CVMX_DBG_DATA CVMX_DBG_DATA_FUNC()
static inline uint64_t CVMX_DBG_DATA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DBG_DATA not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000001E8ull);
}

#define CVMX_DFA_BST0 CVMX_DFA_BST0_FUNC()
static inline uint64_t CVMX_DFA_BST0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_BST0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800300007F0ull);
}

#define CVMX_DFA_BST1 CVMX_DFA_BST1_FUNC()
static inline uint64_t CVMX_DFA_BST1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_BST1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800300007F8ull);
}

#define CVMX_DFA_CFG CVMX_DFA_CFG_FUNC()
static inline uint64_t CVMX_DFA_CFG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_CFG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000000ull);
}

#define CVMX_DFA_DBELL CVMX_DFA_DBELL_FUNC()
static inline uint64_t CVMX_DFA_DBELL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_DBELL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001370000000000ull);
}

#define CVMX_DFA_DDR2_ADDR CVMX_DFA_DDR2_ADDR_FUNC()
static inline uint64_t CVMX_DFA_DDR2_ADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_ADDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000210ull);
}

#define CVMX_DFA_DDR2_BUS CVMX_DFA_DDR2_BUS_FUNC()
static inline uint64_t CVMX_DFA_DDR2_BUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_BUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000080ull);
}

#define CVMX_DFA_DDR2_CFG CVMX_DFA_DDR2_CFG_FUNC()
static inline uint64_t CVMX_DFA_DDR2_CFG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_CFG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000208ull);
}

#define CVMX_DFA_DDR2_COMP CVMX_DFA_DDR2_COMP_FUNC()
static inline uint64_t CVMX_DFA_DDR2_COMP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_COMP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000090ull);
}

#define CVMX_DFA_DDR2_EMRS CVMX_DFA_DDR2_EMRS_FUNC()
static inline uint64_t CVMX_DFA_DDR2_EMRS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_EMRS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000268ull);
}

#define CVMX_DFA_DDR2_FCNT CVMX_DFA_DDR2_FCNT_FUNC()
static inline uint64_t CVMX_DFA_DDR2_FCNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_FCNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000078ull);
}

#define CVMX_DFA_DDR2_MRS CVMX_DFA_DDR2_MRS_FUNC()
static inline uint64_t CVMX_DFA_DDR2_MRS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_MRS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000260ull);
}

#define CVMX_DFA_DDR2_OPT CVMX_DFA_DDR2_OPT_FUNC()
static inline uint64_t CVMX_DFA_DDR2_OPT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_OPT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000070ull);
}

#define CVMX_DFA_DDR2_PLL CVMX_DFA_DDR2_PLL_FUNC()
static inline uint64_t CVMX_DFA_DDR2_PLL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_PLL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000088ull);
}

#define CVMX_DFA_DDR2_TMG CVMX_DFA_DDR2_TMG_FUNC()
static inline uint64_t CVMX_DFA_DDR2_TMG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_DDR2_TMG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000218ull);
}

#define CVMX_DFA_DIFCTL CVMX_DFA_DIFCTL_FUNC()
static inline uint64_t CVMX_DFA_DIFCTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_DIFCTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001370600000000ull);
}

#define CVMX_DFA_DIFRDPTR CVMX_DFA_DIFRDPTR_FUNC()
static inline uint64_t CVMX_DFA_DIFRDPTR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_DIFRDPTR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001370200000000ull);
}

#define CVMX_DFA_ECLKCFG CVMX_DFA_ECLKCFG_FUNC()
static inline uint64_t CVMX_DFA_ECLKCFG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_DFA_ECLKCFG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000200ull);
}

#define CVMX_DFA_ERR CVMX_DFA_ERR_FUNC()
static inline uint64_t CVMX_DFA_ERR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_ERR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000028ull);
}

#define CVMX_DFA_MEMCFG0 CVMX_DFA_MEMCFG0_FUNC()
static inline uint64_t CVMX_DFA_MEMCFG0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_MEMCFG0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000008ull);
}

#define CVMX_DFA_MEMCFG1 CVMX_DFA_MEMCFG1_FUNC()
static inline uint64_t CVMX_DFA_MEMCFG1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_MEMCFG1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000010ull);
}

#define CVMX_DFA_MEMCFG2 CVMX_DFA_MEMCFG2_FUNC()
static inline uint64_t CVMX_DFA_MEMCFG2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_MEMCFG2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000060ull);
}

#define CVMX_DFA_MEMFADR CVMX_DFA_MEMFADR_FUNC()
static inline uint64_t CVMX_DFA_MEMFADR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_MEMFADR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000030ull);
}

#define CVMX_DFA_MEMFCR CVMX_DFA_MEMFCR_FUNC()
static inline uint64_t CVMX_DFA_MEMFCR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_MEMFCR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000038ull);
}

#define CVMX_DFA_MEMRLD CVMX_DFA_MEMRLD_FUNC()
static inline uint64_t CVMX_DFA_MEMRLD_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_MEMRLD not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000018ull);
}

#define CVMX_DFA_NCBCTL CVMX_DFA_NCBCTL_FUNC()
static inline uint64_t CVMX_DFA_NCBCTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_NCBCTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000020ull);
}

#define CVMX_DFA_RODT_COMP_CTL CVMX_DFA_RODT_COMP_CTL_FUNC()
static inline uint64_t CVMX_DFA_RODT_COMP_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_RODT_COMP_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000068ull);
}

#define CVMX_DFA_SBD_DBG0 CVMX_DFA_SBD_DBG0_FUNC()
static inline uint64_t CVMX_DFA_SBD_DBG0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_SBD_DBG0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000040ull);
}

#define CVMX_DFA_SBD_DBG1 CVMX_DFA_SBD_DBG1_FUNC()
static inline uint64_t CVMX_DFA_SBD_DBG1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_SBD_DBG1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000048ull);
}

#define CVMX_DFA_SBD_DBG2 CVMX_DFA_SBD_DBG2_FUNC()
static inline uint64_t CVMX_DFA_SBD_DBG2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_SBD_DBG2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000050ull);
}

#define CVMX_DFA_SBD_DBG3 CVMX_DFA_SBD_DBG3_FUNC()
static inline uint64_t CVMX_DFA_SBD_DBG3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_DFA_SBD_DBG3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180030000058ull);
}

#define CVMX_FPA_BIST_STATUS CVMX_FPA_BIST_STATUS_FUNC()
static inline uint64_t CVMX_FPA_BIST_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800280000E8ull);
}

#define CVMX_FPA_CTL_STATUS CVMX_FPA_CTL_STATUS_FUNC()
static inline uint64_t CVMX_FPA_CTL_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180028000050ull);
}

#define CVMX_FPA_FPF0_MARKS CVMX_FPA_FPF0_MARKS_FUNC()
static inline uint64_t CVMX_FPA_FPF0_MARKS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_FPA_FPF0_MARKS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180028000000ull);
}

#define CVMX_FPA_FPF0_SIZE CVMX_FPA_FPF0_SIZE_FUNC()
static inline uint64_t CVMX_FPA_FPF0_SIZE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_FPA_FPF0_SIZE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180028000058ull);
}

#define CVMX_FPA_FPF1_MARKS CVMX_FPA_FPFX_MARKS(1)
#define CVMX_FPA_FPF2_MARKS CVMX_FPA_FPFX_MARKS(2)
#define CVMX_FPA_FPF3_MARKS CVMX_FPA_FPFX_MARKS(3)
#define CVMX_FPA_FPF4_MARKS CVMX_FPA_FPFX_MARKS(4)
#define CVMX_FPA_FPF5_MARKS CVMX_FPA_FPFX_MARKS(5)
#define CVMX_FPA_FPF6_MARKS CVMX_FPA_FPFX_MARKS(6)
#define CVMX_FPA_FPF7_MARKS CVMX_FPA_FPFX_MARKS(7)
static inline uint64_t CVMX_FPA_FPFX_MARKS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset >= 1) && (offset <= 7)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset >= 1) && (offset <= 7)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset >= 1) && (offset <= 7))))))
        cvmx_warn("CVMX_FPA_FPFX_MARKS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180028000008ull) + (offset&7)*8 - 8*1;
}

static inline uint64_t CVMX_FPA_FPFX_SIZE(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset >= 1) && (offset <= 7)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset >= 1) && (offset <= 7)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset >= 1) && (offset <= 7))))))
        cvmx_warn("CVMX_FPA_FPFX_SIZE(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180028000060ull) + (offset&7)*8 - 8*1;
}

#define CVMX_FPA_INT_ENB CVMX_FPA_INT_ENB_FUNC()
static inline uint64_t CVMX_FPA_INT_ENB_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180028000048ull);
}

#define CVMX_FPA_INT_SUM CVMX_FPA_INT_SUM_FUNC()
static inline uint64_t CVMX_FPA_INT_SUM_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180028000040ull);
}

#define CVMX_FPA_QUE0_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(0)
#define CVMX_FPA_QUE1_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(1)
#define CVMX_FPA_QUE2_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(2)
#define CVMX_FPA_QUE3_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(3)
#define CVMX_FPA_QUE4_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(4)
#define CVMX_FPA_QUE5_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(5)
#define CVMX_FPA_QUE6_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(6)
#define CVMX_FPA_QUE7_PAGE_INDEX CVMX_FPA_QUEX_PAGE_INDEX(7)
static inline uint64_t CVMX_FPA_QUEX_AVAILABLE(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_FPA_QUEX_AVAILABLE(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180028000098ull) + (offset&7)*8;
}

static inline uint64_t CVMX_FPA_QUEX_PAGE_INDEX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_FPA_QUEX_PAGE_INDEX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800280000F0ull) + (offset&7)*8;
}

#define CVMX_FPA_QUE_ACT CVMX_FPA_QUE_ACT_FUNC()
static inline uint64_t CVMX_FPA_QUE_ACT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180028000138ull);
}

#define CVMX_FPA_QUE_EXP CVMX_FPA_QUE_EXP_FUNC()
static inline uint64_t CVMX_FPA_QUE_EXP_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180028000130ull);
}

#define CVMX_FPA_WART_CTL CVMX_FPA_WART_CTL_FUNC()
static inline uint64_t CVMX_FPA_WART_CTL_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800280000D8ull);
}

#define CVMX_FPA_WART_STATUS CVMX_FPA_WART_STATUS_FUNC()
static inline uint64_t CVMX_FPA_WART_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800280000E0ull);
}

static inline uint64_t CVMX_GMXX_BAD_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_BAD_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000518ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_BIST(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_BIST(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000400ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_CLK_EN(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_CLK_EN(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080007F0ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_HG2_CONTROL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_HG2_CONTROL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000550ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_INF_MODE(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_INF_MODE(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080007F8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_NXA_ADR(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_NXA_ADR(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000510ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_PRTX_CBFC_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset == 0)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset == 0)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_PRTX_CBFC_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000580ull) + ((offset&0) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_GMXX_PRTX_CFG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_PRTX_CFG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000010ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_ADR_CAM0(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_ADR_CAM0(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000180ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_ADR_CAM1(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_ADR_CAM1(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000188ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_ADR_CAM2(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_ADR_CAM2(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000190ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_ADR_CAM3(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_ADR_CAM3(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000198ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_ADR_CAM4(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_ADR_CAM4(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080001A0ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_ADR_CAM5(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_ADR_CAM5(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080001A8ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_ADR_CAM_EN(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_ADR_CAM_EN(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000108ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_ADR_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_ADR_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000100ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_DECISION(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_DECISION(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000040ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_FRM_CHK(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_FRM_CHK(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000020ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_FRM_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_FRM_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000018ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_FRM_MAX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_GMXX_RXX_FRM_MAX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000030ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_FRM_MIN(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_GMXX_RXX_FRM_MIN(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000028ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_IFG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_IFG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000058ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_INT_EN(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_INT_EN(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000008ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_INT_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_INT_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000000ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_JABBER(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_JABBER(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000038ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_PAUSE_DROP_TIME(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_PAUSE_DROP_TIME(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000068ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_RX_INBND(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_GMXX_RXX_RX_INBND(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000060ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000050ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_OCTS(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_OCTS(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000088ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_OCTS_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_OCTS_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000098ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_OCTS_DMAC(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_OCTS_DMAC(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080000A8ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_OCTS_DRP(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_OCTS_DRP(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080000B8ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_PKTS(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_PKTS(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000080ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_PKTS_BAD(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_PKTS_BAD(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080000C0ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_PKTS_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_PKTS_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000090ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_PKTS_DMAC(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_PKTS_DMAC(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080000A0ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_STATS_PKTS_DRP(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_STATS_PKTS_DRP(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080000B0ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RXX_UDD_SKP(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RXX_UDD_SKP(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000048ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_RX_BP_DROPX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RX_BP_DROPX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000420ull) + ((offset&3) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_GMXX_RX_BP_OFFX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RX_BP_OFFX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000460ull) + ((offset&3) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_GMXX_RX_BP_ONX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_RX_BP_ONX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000440ull) + ((offset&3) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_GMXX_RX_HG2_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_RX_HG2_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000548ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_RX_PASS_EN(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_GMXX_RX_PASS_EN(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080005F8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_RX_PASS_MAPX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 15)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 15)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_GMXX_RX_PASS_MAPX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000600ull) + ((offset&15) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_GMXX_RX_PRTS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_RX_PRTS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000410ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_RX_PRT_INFO(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_RX_PRT_INFO(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004E8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_RX_TX_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_RX_TX_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080007E8ull) + (block_id&0)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_RX_XAUI_BAD_COL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_RX_XAUI_BAD_COL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000538ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_RX_XAUI_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_RX_XAUI_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000530ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_SMACX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_SMACX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000230ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_STAT_BP(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_STAT_BP(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000520ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TXX_APPEND(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_APPEND(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000218ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_BURST(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_BURST(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000228ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_CBFC_XOFF(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset == 0)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset == 0)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_CBFC_XOFF(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080005A0ull) + ((offset&0) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_GMXX_TXX_CBFC_XON(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset == 0)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset == 0)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_CBFC_XON(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080005C0ull) + ((offset&0) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_GMXX_TXX_CLK(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_GMXX_TXX_CLK(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000208ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000270ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_MIN_PKT(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_MIN_PKT(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000240ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_PAUSE_PKT_INTERVAL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_PAUSE_PKT_INTERVAL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000248ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_PAUSE_PKT_TIME(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_PAUSE_PKT_TIME(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000238ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_PAUSE_TOGO(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_PAUSE_TOGO(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000258ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_PAUSE_ZERO(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_PAUSE_ZERO(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000260ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_SGMII_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_SGMII_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000300ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_SLOT(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_SLOT(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000220ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_SOFT_PAUSE(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_SOFT_PAUSE(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000250ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT0(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT0(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000280ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT1(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT1(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000288ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT2(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT2(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000290ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT3(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT3(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000298ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT4(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT4(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080002A0ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT5(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT5(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080002A8ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT6(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT6(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080002B0ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT7(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT7(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080002B8ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT8(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT8(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080002C0ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STAT9(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STAT9(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080002C8ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_STATS_CTL(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_STATS_CTL(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000268ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TXX_THRESH(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 2)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TXX_THRESH(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000210ull) + ((offset&3) + (block_id&1)*0x10000ull)*2048;
}

static inline uint64_t CVMX_GMXX_TX_BP(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_BP(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004D0ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_CLK_MSKX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 1)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 1)) && ((block_id == 0))))))
        cvmx_warn("CVMX_GMXX_TX_CLK_MSKX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000780ull) + ((offset&1) + (block_id&0)*0x0ull)*8;
}

static inline uint64_t CVMX_GMXX_TX_COL_ATTEMPT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_COL_ATTEMPT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000498ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_CORRUPT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_CORRUPT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004D8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_HG2_REG1(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_HG2_REG1(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000558ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_HG2_REG2(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_HG2_REG2(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000560ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_IFG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_IFG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000488ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_INT_EN(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_INT_EN(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000508ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_INT_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_INT_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000500ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_JAM(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_JAM(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000490ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_LFSR(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_LFSR(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004F8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_OVR_BP(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_OVR_BP(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004C8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_PAUSE_PKT_DMAC(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_PAUSE_PKT_DMAC(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004A0ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_PAUSE_PKT_TYPE(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_PAUSE_PKT_TYPE(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004A8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_PRTS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_PRTS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000480ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_SPI_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_GMXX_TX_SPI_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004C0ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_SPI_DRAIN(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_GMXX_TX_SPI_DRAIN(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004E0ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_SPI_MAX(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_GMXX_TX_SPI_MAX(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004B0ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_SPI_ROUNDX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 31)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_GMXX_TX_SPI_ROUNDX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000680ull) + ((offset&31) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_GMXX_TX_SPI_THRESH(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_GMXX_TX_SPI_THRESH(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800080004B8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_TX_XAUI_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_TX_XAUI_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000528ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GMXX_XAUI_EXT_LOOPBACK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_GMXX_XAUI_EXT_LOOPBACK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180008000540ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_GPIO_BIT_CFGX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 15)))))
        cvmx_warn("CVMX_GPIO_BIT_CFGX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000800ull) + (offset&15)*8;
}

#define CVMX_GPIO_BOOT_ENA CVMX_GPIO_BOOT_ENA_FUNC()
static inline uint64_t CVMX_GPIO_BOOT_ENA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)))
        cvmx_warn("CVMX_GPIO_BOOT_ENA not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00010700000008A8ull);
}

static inline uint64_t CVMX_GPIO_CLK_GENX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_GPIO_CLK_GENX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000008C0ull) + (offset&3)*8;
}

#define CVMX_GPIO_DBG_ENA CVMX_GPIO_DBG_ENA_FUNC()
static inline uint64_t CVMX_GPIO_DBG_ENA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)))
        cvmx_warn("CVMX_GPIO_DBG_ENA not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00010700000008A0ull);
}

#define CVMX_GPIO_INT_CLR CVMX_GPIO_INT_CLR_FUNC()
static inline uint64_t CVMX_GPIO_INT_CLR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000898ull);
}

#define CVMX_GPIO_RX_DAT CVMX_GPIO_RX_DAT_FUNC()
static inline uint64_t CVMX_GPIO_RX_DAT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000880ull);
}

#define CVMX_GPIO_TX_CLR CVMX_GPIO_TX_CLR_FUNC()
static inline uint64_t CVMX_GPIO_TX_CLR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000890ull);
}

#define CVMX_GPIO_TX_SET CVMX_GPIO_TX_SET_FUNC()
static inline uint64_t CVMX_GPIO_TX_SET_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001070000000888ull);
}

static inline uint64_t CVMX_GPIO_XBIT_CFGX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset >= 16) && (offset <= 23)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset >= 16) && (offset <= 23)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset >= 16) && (offset <= 23))))))
        cvmx_warn("CVMX_GPIO_XBIT_CFGX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000000900ull) + (offset&31)*8 - 8*16;
}

#define CVMX_IOB_BIST_STATUS CVMX_IOB_BIST_STATUS_FUNC()
static inline uint64_t CVMX_IOB_BIST_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F00007F8ull);
}

#define CVMX_IOB_CTL_STATUS CVMX_IOB_CTL_STATUS_FUNC()
static inline uint64_t CVMX_IOB_CTL_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000050ull);
}

#define CVMX_IOB_DWB_PRI_CNT CVMX_IOB_DWB_PRI_CNT_FUNC()
static inline uint64_t CVMX_IOB_DWB_PRI_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_IOB_DWB_PRI_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F0000028ull);
}

#define CVMX_IOB_FAU_TIMEOUT CVMX_IOB_FAU_TIMEOUT_FUNC()
static inline uint64_t CVMX_IOB_FAU_TIMEOUT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000000ull);
}

#define CVMX_IOB_I2C_PRI_CNT CVMX_IOB_I2C_PRI_CNT_FUNC()
static inline uint64_t CVMX_IOB_I2C_PRI_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_IOB_I2C_PRI_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F0000010ull);
}

#define CVMX_IOB_INB_CONTROL_MATCH CVMX_IOB_INB_CONTROL_MATCH_FUNC()
static inline uint64_t CVMX_IOB_INB_CONTROL_MATCH_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000078ull);
}

#define CVMX_IOB_INB_CONTROL_MATCH_ENB CVMX_IOB_INB_CONTROL_MATCH_ENB_FUNC()
static inline uint64_t CVMX_IOB_INB_CONTROL_MATCH_ENB_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000088ull);
}

#define CVMX_IOB_INB_DATA_MATCH CVMX_IOB_INB_DATA_MATCH_FUNC()
static inline uint64_t CVMX_IOB_INB_DATA_MATCH_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000070ull);
}

#define CVMX_IOB_INB_DATA_MATCH_ENB CVMX_IOB_INB_DATA_MATCH_ENB_FUNC()
static inline uint64_t CVMX_IOB_INB_DATA_MATCH_ENB_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000080ull);
}

#define CVMX_IOB_INT_ENB CVMX_IOB_INT_ENB_FUNC()
static inline uint64_t CVMX_IOB_INT_ENB_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000060ull);
}

#define CVMX_IOB_INT_SUM CVMX_IOB_INT_SUM_FUNC()
static inline uint64_t CVMX_IOB_INT_SUM_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000058ull);
}

#define CVMX_IOB_N2C_L2C_PRI_CNT CVMX_IOB_N2C_L2C_PRI_CNT_FUNC()
static inline uint64_t CVMX_IOB_N2C_L2C_PRI_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_IOB_N2C_L2C_PRI_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F0000020ull);
}

#define CVMX_IOB_N2C_RSP_PRI_CNT CVMX_IOB_N2C_RSP_PRI_CNT_FUNC()
static inline uint64_t CVMX_IOB_N2C_RSP_PRI_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_IOB_N2C_RSP_PRI_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F0000008ull);
}

#define CVMX_IOB_OUTB_COM_PRI_CNT CVMX_IOB_OUTB_COM_PRI_CNT_FUNC()
static inline uint64_t CVMX_IOB_OUTB_COM_PRI_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_IOB_OUTB_COM_PRI_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F0000040ull);
}

#define CVMX_IOB_OUTB_CONTROL_MATCH CVMX_IOB_OUTB_CONTROL_MATCH_FUNC()
static inline uint64_t CVMX_IOB_OUTB_CONTROL_MATCH_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000098ull);
}

#define CVMX_IOB_OUTB_CONTROL_MATCH_ENB CVMX_IOB_OUTB_CONTROL_MATCH_ENB_FUNC()
static inline uint64_t CVMX_IOB_OUTB_CONTROL_MATCH_ENB_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F00000A8ull);
}

#define CVMX_IOB_OUTB_DATA_MATCH CVMX_IOB_OUTB_DATA_MATCH_FUNC()
static inline uint64_t CVMX_IOB_OUTB_DATA_MATCH_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000090ull);
}

#define CVMX_IOB_OUTB_DATA_MATCH_ENB CVMX_IOB_OUTB_DATA_MATCH_ENB_FUNC()
static inline uint64_t CVMX_IOB_OUTB_DATA_MATCH_ENB_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F00000A0ull);
}

#define CVMX_IOB_OUTB_FPA_PRI_CNT CVMX_IOB_OUTB_FPA_PRI_CNT_FUNC()
static inline uint64_t CVMX_IOB_OUTB_FPA_PRI_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_IOB_OUTB_FPA_PRI_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F0000048ull);
}

#define CVMX_IOB_OUTB_REQ_PRI_CNT CVMX_IOB_OUTB_REQ_PRI_CNT_FUNC()
static inline uint64_t CVMX_IOB_OUTB_REQ_PRI_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_IOB_OUTB_REQ_PRI_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F0000038ull);
}

#define CVMX_IOB_P2C_REQ_PRI_CNT CVMX_IOB_P2C_REQ_PRI_CNT_FUNC()
static inline uint64_t CVMX_IOB_P2C_REQ_PRI_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_IOB_P2C_REQ_PRI_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F0000018ull);
}

#define CVMX_IOB_PKT_ERR CVMX_IOB_PKT_ERR_FUNC()
static inline uint64_t CVMX_IOB_PKT_ERR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800F0000068ull);
}

#define CVMX_IOB_TO_CMB_CREDITS CVMX_IOB_TO_CMB_CREDITS_FUNC()
static inline uint64_t CVMX_IOB_TO_CMB_CREDITS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_IOB_TO_CMB_CREDITS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800F00000B0ull);
}

#define CVMX_IPD_1ST_MBUFF_SKIP CVMX_IPD_1ST_MBUFF_SKIP_FUNC()
static inline uint64_t CVMX_IPD_1ST_MBUFF_SKIP_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000000ull);
}

#define CVMX_IPD_1st_NEXT_PTR_BACK CVMX_IPD_1st_NEXT_PTR_BACK_FUNC()
static inline uint64_t CVMX_IPD_1st_NEXT_PTR_BACK_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000150ull);
}

#define CVMX_IPD_2nd_NEXT_PTR_BACK CVMX_IPD_2nd_NEXT_PTR_BACK_FUNC()
static inline uint64_t CVMX_IPD_2nd_NEXT_PTR_BACK_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000158ull);
}

#define CVMX_IPD_BIST_STATUS CVMX_IPD_BIST_STATUS_FUNC()
static inline uint64_t CVMX_IPD_BIST_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F00000007F8ull);
}

#define CVMX_IPD_BP_PRT_RED_END CVMX_IPD_BP_PRT_RED_END_FUNC()
static inline uint64_t CVMX_IPD_BP_PRT_RED_END_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000328ull);
}

#define CVMX_IPD_CLK_COUNT CVMX_IPD_CLK_COUNT_FUNC()
static inline uint64_t CVMX_IPD_CLK_COUNT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000338ull);
}

#define CVMX_IPD_CTL_STATUS CVMX_IPD_CTL_STATUS_FUNC()
static inline uint64_t CVMX_IPD_CTL_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000018ull);
}

#define CVMX_IPD_INT_ENB CVMX_IPD_INT_ENB_FUNC()
static inline uint64_t CVMX_IPD_INT_ENB_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000160ull);
}

#define CVMX_IPD_INT_SUM CVMX_IPD_INT_SUM_FUNC()
static inline uint64_t CVMX_IPD_INT_SUM_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000168ull);
}

#define CVMX_IPD_NOT_1ST_MBUFF_SKIP CVMX_IPD_NOT_1ST_MBUFF_SKIP_FUNC()
static inline uint64_t CVMX_IPD_NOT_1ST_MBUFF_SKIP_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000008ull);
}

#define CVMX_IPD_PACKET_MBUFF_SIZE CVMX_IPD_PACKET_MBUFF_SIZE_FUNC()
static inline uint64_t CVMX_IPD_PACKET_MBUFF_SIZE_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000010ull);
}

#define CVMX_IPD_PKT_PTR_VALID CVMX_IPD_PKT_PTR_VALID_FUNC()
static inline uint64_t CVMX_IPD_PKT_PTR_VALID_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000358ull);
}

static inline uint64_t CVMX_IPD_PORTX_BP_PAGE_CNT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35))))))
        cvmx_warn("CVMX_IPD_PORTX_BP_PAGE_CNT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F0000000028ull) + (offset&63)*8;
}

static inline uint64_t CVMX_IPD_PORTX_BP_PAGE_CNT2(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_IPD_PORTX_BP_PAGE_CNT2(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F0000000368ull) + (offset&63)*8 - 8*36;
}

static inline uint64_t CVMX_IPD_PORT_BP_COUNTERS2_PAIRX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_IPD_PORT_BP_COUNTERS2_PAIRX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F0000000388ull) + (offset&63)*8 - 8*36;
}

static inline uint64_t CVMX_IPD_PORT_BP_COUNTERS_PAIRX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || (offset == 32))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35))))))
        cvmx_warn("CVMX_IPD_PORT_BP_COUNTERS_PAIRX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F00000001B8ull) + (offset&63)*8;
}

static inline uint64_t CVMX_IPD_PORT_QOS_INTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0) || (offset == 2) || (offset == 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset == 0) || (offset == 4)))))
        cvmx_warn("CVMX_IPD_PORT_QOS_INTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F0000000808ull) + (offset&7)*8;
}

static inline uint64_t CVMX_IPD_PORT_QOS_INT_ENBX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0) || (offset == 2) || (offset == 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset == 0) || (offset == 4)))))
        cvmx_warn("CVMX_IPD_PORT_QOS_INT_ENBX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F0000000848ull) + (offset&7)*8;
}

static inline uint64_t CVMX_IPD_PORT_QOS_X_CNT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31) || ((offset >= 128) && (offset <= 159)) || ((offset >= 256) && (offset <= 319)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31) || ((offset >= 256) && (offset <= 319))))))
        cvmx_warn("CVMX_IPD_PORT_QOS_X_CNT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F0000000888ull) + (offset&511)*8;
}

#define CVMX_IPD_PRC_HOLD_PTR_FIFO_CTL CVMX_IPD_PRC_HOLD_PTR_FIFO_CTL_FUNC()
static inline uint64_t CVMX_IPD_PRC_HOLD_PTR_FIFO_CTL_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000348ull);
}

#define CVMX_IPD_PRC_PORT_PTR_FIFO_CTL CVMX_IPD_PRC_PORT_PTR_FIFO_CTL_FUNC()
static inline uint64_t CVMX_IPD_PRC_PORT_PTR_FIFO_CTL_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000350ull);
}

#define CVMX_IPD_PTR_COUNT CVMX_IPD_PTR_COUNT_FUNC()
static inline uint64_t CVMX_IPD_PTR_COUNT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000320ull);
}

#define CVMX_IPD_PWP_PTR_FIFO_CTL CVMX_IPD_PWP_PTR_FIFO_CTL_FUNC()
static inline uint64_t CVMX_IPD_PWP_PTR_FIFO_CTL_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000340ull);
}

#define CVMX_IPD_QOS0_RED_MARKS CVMX_IPD_QOSX_RED_MARKS(0)
#define CVMX_IPD_QOS1_RED_MARKS CVMX_IPD_QOSX_RED_MARKS(1)
#define CVMX_IPD_QOS2_RED_MARKS CVMX_IPD_QOSX_RED_MARKS(2)
#define CVMX_IPD_QOS3_RED_MARKS CVMX_IPD_QOSX_RED_MARKS(3)
#define CVMX_IPD_QOS4_RED_MARKS CVMX_IPD_QOSX_RED_MARKS(4)
#define CVMX_IPD_QOS5_RED_MARKS CVMX_IPD_QOSX_RED_MARKS(5)
#define CVMX_IPD_QOS6_RED_MARKS CVMX_IPD_QOSX_RED_MARKS(6)
#define CVMX_IPD_QOS7_RED_MARKS CVMX_IPD_QOSX_RED_MARKS(7)
static inline uint64_t CVMX_IPD_QOSX_RED_MARKS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_IPD_QOSX_RED_MARKS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F0000000178ull) + (offset&7)*8;
}

#define CVMX_IPD_QUE0_FREE_PAGE_CNT CVMX_IPD_QUE0_FREE_PAGE_CNT_FUNC()
static inline uint64_t CVMX_IPD_QUE0_FREE_PAGE_CNT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000330ull);
}

#define CVMX_IPD_RED_PORT_ENABLE CVMX_IPD_RED_PORT_ENABLE_FUNC()
static inline uint64_t CVMX_IPD_RED_PORT_ENABLE_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F00000002D8ull);
}

#define CVMX_IPD_RED_PORT_ENABLE2 CVMX_IPD_RED_PORT_ENABLE2_FUNC()
static inline uint64_t CVMX_IPD_RED_PORT_ENABLE2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_IPD_RED_PORT_ENABLE2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00014F00000003A8ull);
}

#define CVMX_IPD_RED_QUE0_PARAM CVMX_IPD_RED_QUEX_PARAM(0)
#define CVMX_IPD_RED_QUE1_PARAM CVMX_IPD_RED_QUEX_PARAM(1)
#define CVMX_IPD_RED_QUE2_PARAM CVMX_IPD_RED_QUEX_PARAM(2)
#define CVMX_IPD_RED_QUE3_PARAM CVMX_IPD_RED_QUEX_PARAM(3)
#define CVMX_IPD_RED_QUE4_PARAM CVMX_IPD_RED_QUEX_PARAM(4)
#define CVMX_IPD_RED_QUE5_PARAM CVMX_IPD_RED_QUEX_PARAM(5)
#define CVMX_IPD_RED_QUE6_PARAM CVMX_IPD_RED_QUEX_PARAM(6)
#define CVMX_IPD_RED_QUE7_PARAM CVMX_IPD_RED_QUEX_PARAM(7)
static inline uint64_t CVMX_IPD_RED_QUEX_PARAM(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_IPD_RED_QUEX_PARAM(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00014F00000002E0ull) + (offset&7)*8;
}

#define CVMX_IPD_SUB_PORT_BP_PAGE_CNT CVMX_IPD_SUB_PORT_BP_PAGE_CNT_FUNC()
static inline uint64_t CVMX_IPD_SUB_PORT_BP_PAGE_CNT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000148ull);
}

#define CVMX_IPD_SUB_PORT_FCS CVMX_IPD_SUB_PORT_FCS_FUNC()
static inline uint64_t CVMX_IPD_SUB_PORT_FCS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000170ull);
}

#define CVMX_IPD_SUB_PORT_QOS_CNT CVMX_IPD_SUB_PORT_QOS_CNT_FUNC()
static inline uint64_t CVMX_IPD_SUB_PORT_QOS_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_IPD_SUB_PORT_QOS_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00014F0000000800ull);
}

#define CVMX_IPD_WQE_FPA_QUEUE CVMX_IPD_WQE_FPA_QUEUE_FUNC()
static inline uint64_t CVMX_IPD_WQE_FPA_QUEUE_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000020ull);
}

#define CVMX_IPD_WQE_PTR_VALID CVMX_IPD_WQE_PTR_VALID_FUNC()
static inline uint64_t CVMX_IPD_WQE_PTR_VALID_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00014F0000000360ull);
}

#define CVMX_KEY_BIST_REG CVMX_KEY_BIST_REG_FUNC()
static inline uint64_t CVMX_KEY_BIST_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_KEY_BIST_REG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180020000018ull);
}

#define CVMX_KEY_CTL_STATUS CVMX_KEY_CTL_STATUS_FUNC()
static inline uint64_t CVMX_KEY_CTL_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_KEY_CTL_STATUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180020000010ull);
}

#define CVMX_KEY_INT_ENB CVMX_KEY_INT_ENB_FUNC()
static inline uint64_t CVMX_KEY_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_KEY_INT_ENB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180020000008ull);
}

#define CVMX_KEY_INT_SUM CVMX_KEY_INT_SUM_FUNC()
static inline uint64_t CVMX_KEY_INT_SUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_KEY_INT_SUM not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180020000000ull);
}

#define CVMX_L2C_BST0 CVMX_L2C_BST0_FUNC()
static inline uint64_t CVMX_L2C_BST0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800800007F8ull);
}

#define CVMX_L2C_BST1 CVMX_L2C_BST1_FUNC()
static inline uint64_t CVMX_L2C_BST1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800800007F0ull);
}

#define CVMX_L2C_BST2 CVMX_L2C_BST2_FUNC()
static inline uint64_t CVMX_L2C_BST2_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800800007E8ull);
}

#define CVMX_L2C_CFG CVMX_L2C_CFG_FUNC()
static inline uint64_t CVMX_L2C_CFG_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000000ull);
}

#define CVMX_L2C_DBG CVMX_L2C_DBG_FUNC()
static inline uint64_t CVMX_L2C_DBG_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000030ull);
}

#define CVMX_L2C_DUT CVMX_L2C_DUT_FUNC()
static inline uint64_t CVMX_L2C_DUT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000050ull);
}

#define CVMX_L2C_GRPWRR0 CVMX_L2C_GRPWRR0_FUNC()
static inline uint64_t CVMX_L2C_GRPWRR0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_GRPWRR0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800800000C8ull);
}

#define CVMX_L2C_GRPWRR1 CVMX_L2C_GRPWRR1_FUNC()
static inline uint64_t CVMX_L2C_GRPWRR1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_GRPWRR1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800800000D0ull);
}

#define CVMX_L2C_INT_EN CVMX_L2C_INT_EN_FUNC()
static inline uint64_t CVMX_L2C_INT_EN_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_INT_EN not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180080000100ull);
}

#define CVMX_L2C_INT_STAT CVMX_L2C_INT_STAT_FUNC()
static inline uint64_t CVMX_L2C_INT_STAT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_INT_STAT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800800000F8ull);
}

#define CVMX_L2C_LCKBASE CVMX_L2C_LCKBASE_FUNC()
static inline uint64_t CVMX_L2C_LCKBASE_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000058ull);
}

#define CVMX_L2C_LCKOFF CVMX_L2C_LCKOFF_FUNC()
static inline uint64_t CVMX_L2C_LCKOFF_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000060ull);
}

#define CVMX_L2C_LFB0 CVMX_L2C_LFB0_FUNC()
static inline uint64_t CVMX_L2C_LFB0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000038ull);
}

#define CVMX_L2C_LFB1 CVMX_L2C_LFB1_FUNC()
static inline uint64_t CVMX_L2C_LFB1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000040ull);
}

#define CVMX_L2C_LFB2 CVMX_L2C_LFB2_FUNC()
static inline uint64_t CVMX_L2C_LFB2_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000048ull);
}

#define CVMX_L2C_LFB3 CVMX_L2C_LFB3_FUNC()
static inline uint64_t CVMX_L2C_LFB3_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800800000B8ull);
}

#define CVMX_L2C_OOB CVMX_L2C_OOB_FUNC()
static inline uint64_t CVMX_L2C_OOB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_OOB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800800000D8ull);
}

#define CVMX_L2C_OOB1 CVMX_L2C_OOB1_FUNC()
static inline uint64_t CVMX_L2C_OOB1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_OOB1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800800000E0ull);
}

#define CVMX_L2C_OOB2 CVMX_L2C_OOB2_FUNC()
static inline uint64_t CVMX_L2C_OOB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_OOB2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800800000E8ull);
}

#define CVMX_L2C_OOB3 CVMX_L2C_OOB3_FUNC()
static inline uint64_t CVMX_L2C_OOB3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_OOB3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800800000F0ull);
}

#define CVMX_L2C_PFC0 CVMX_L2C_PFCX(0)
#define CVMX_L2C_PFC1 CVMX_L2C_PFCX(1)
#define CVMX_L2C_PFC2 CVMX_L2C_PFCX(2)
#define CVMX_L2C_PFC3 CVMX_L2C_PFCX(3)
#define CVMX_L2C_PFCTL CVMX_L2C_PFCTL_FUNC()
static inline uint64_t CVMX_L2C_PFCTL_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000090ull);
}

static inline uint64_t CVMX_L2C_PFCX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_L2C_PFCX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180080000098ull) + (offset&3)*8;
}

#define CVMX_L2C_PPGRP CVMX_L2C_PPGRP_FUNC()
static inline uint64_t CVMX_L2C_PPGRP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_L2C_PPGRP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800800000C0ull);
}

#define CVMX_L2C_SPAR0 CVMX_L2C_SPAR0_FUNC()
static inline uint64_t CVMX_L2C_SPAR0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000068ull);
}

#define CVMX_L2C_SPAR1 CVMX_L2C_SPAR1_FUNC()
static inline uint64_t CVMX_L2C_SPAR1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_L2C_SPAR1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180080000070ull);
}

#define CVMX_L2C_SPAR2 CVMX_L2C_SPAR2_FUNC()
static inline uint64_t CVMX_L2C_SPAR2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_L2C_SPAR2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180080000078ull);
}

#define CVMX_L2C_SPAR3 CVMX_L2C_SPAR3_FUNC()
static inline uint64_t CVMX_L2C_SPAR3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_L2C_SPAR3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180080000080ull);
}

#define CVMX_L2C_SPAR4 CVMX_L2C_SPAR4_FUNC()
static inline uint64_t CVMX_L2C_SPAR4_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000088ull);
}

#define CVMX_L2D_BST0 CVMX_L2D_BST0_FUNC()
static inline uint64_t CVMX_L2D_BST0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000780ull);
}

#define CVMX_L2D_BST1 CVMX_L2D_BST1_FUNC()
static inline uint64_t CVMX_L2D_BST1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000788ull);
}

#define CVMX_L2D_BST2 CVMX_L2D_BST2_FUNC()
static inline uint64_t CVMX_L2D_BST2_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000790ull);
}

#define CVMX_L2D_BST3 CVMX_L2D_BST3_FUNC()
static inline uint64_t CVMX_L2D_BST3_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000798ull);
}

#define CVMX_L2D_ERR CVMX_L2D_ERR_FUNC()
static inline uint64_t CVMX_L2D_ERR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000010ull);
}

#define CVMX_L2D_FADR CVMX_L2D_FADR_FUNC()
static inline uint64_t CVMX_L2D_FADR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000018ull);
}

#define CVMX_L2D_FSYN0 CVMX_L2D_FSYN0_FUNC()
static inline uint64_t CVMX_L2D_FSYN0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000020ull);
}

#define CVMX_L2D_FSYN1 CVMX_L2D_FSYN1_FUNC()
static inline uint64_t CVMX_L2D_FSYN1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000028ull);
}

#define CVMX_L2D_FUS0 CVMX_L2D_FUS0_FUNC()
static inline uint64_t CVMX_L2D_FUS0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800800007A0ull);
}

#define CVMX_L2D_FUS1 CVMX_L2D_FUS1_FUNC()
static inline uint64_t CVMX_L2D_FUS1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800800007A8ull);
}

#define CVMX_L2D_FUS2 CVMX_L2D_FUS2_FUNC()
static inline uint64_t CVMX_L2D_FUS2_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800800007B0ull);
}

#define CVMX_L2D_FUS3 CVMX_L2D_FUS3_FUNC()
static inline uint64_t CVMX_L2D_FUS3_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800800007B8ull);
}

#define CVMX_L2T_ERR CVMX_L2T_ERR_FUNC()
static inline uint64_t CVMX_L2T_ERR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180080000008ull);
}

#define CVMX_LED_BLINK CVMX_LED_BLINK_FUNC()
static inline uint64_t CVMX_LED_BLINK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_LED_BLINK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A48ull);
}

#define CVMX_LED_CLK_PHASE CVMX_LED_CLK_PHASE_FUNC()
static inline uint64_t CVMX_LED_CLK_PHASE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_LED_CLK_PHASE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A08ull);
}

#define CVMX_LED_CYLON CVMX_LED_CYLON_FUNC()
static inline uint64_t CVMX_LED_CYLON_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_LED_CYLON not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001AF8ull);
}

#define CVMX_LED_DBG CVMX_LED_DBG_FUNC()
static inline uint64_t CVMX_LED_DBG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_LED_DBG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A18ull);
}

#define CVMX_LED_EN CVMX_LED_EN_FUNC()
static inline uint64_t CVMX_LED_EN_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_LED_EN not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A00ull);
}

#define CVMX_LED_POLARITY CVMX_LED_POLARITY_FUNC()
static inline uint64_t CVMX_LED_POLARITY_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_LED_POLARITY not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A50ull);
}

#define CVMX_LED_PRT CVMX_LED_PRT_FUNC()
static inline uint64_t CVMX_LED_PRT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_LED_PRT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A10ull);
}

#define CVMX_LED_PRT_FMT CVMX_LED_PRT_FMT_FUNC()
static inline uint64_t CVMX_LED_PRT_FMT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_LED_PRT_FMT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A30ull);
}

static inline uint64_t CVMX_LED_PRT_STATUSX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_LED_PRT_STATUSX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A80ull) + (offset&7)*8;
}

static inline uint64_t CVMX_LED_UDD_CNTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_LED_UDD_CNTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A20ull) + (offset&1)*8;
}

static inline uint64_t CVMX_LED_UDD_DATX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_LED_UDD_DATX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001A38ull) + (offset&1)*8;
}

static inline uint64_t CVMX_LED_UDD_DAT_CLRX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_LED_UDD_DAT_CLRX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001AC8ull) + (offset&1)*16;
}

static inline uint64_t CVMX_LED_UDD_DAT_SETX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_LED_UDD_DAT_SETX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001AC0ull) + (offset&1)*16;
}

static inline uint64_t CVMX_LMCX_BIST_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_BIST_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800880000F0ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_BIST_RESULT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_BIST_RESULT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800880000F8ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_COMP_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_COMP_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000028ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000010ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_CTL1(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_CTL1(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000090ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_DCLK_CNT_HI(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_DCLK_CNT_HI(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000070ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_DCLK_CNT_LO(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_DCLK_CNT_LO(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000068ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_DCLK_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_LMCX_DCLK_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800880000B8ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_DDR2_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_DDR2_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000018ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_DELAY_CFG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_DELAY_CFG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000088ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_DLL_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_DLL_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800880000C0ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_DUAL_MEMCFG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_DUAL_MEMCFG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000098ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_ECC_SYND(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_ECC_SYND(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000038ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_FADR(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_FADR(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000020ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_IFB_CNT_HI(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_IFB_CNT_HI(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000050ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_IFB_CNT_LO(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_IFB_CNT_LO(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000048ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_MEM_CFG0(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_MEM_CFG0(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000000ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_MEM_CFG1(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_MEM_CFG1(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000008ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_NXM(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_NXM(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800880000C8ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_OPS_CNT_HI(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_OPS_CNT_HI(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000060ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_OPS_CNT_LO(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_OPS_CNT_LO(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000058ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_PLL_BWCTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_PLL_BWCTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000040ull) + (block_id&0)*0x8000000ull;
}

static inline uint64_t CVMX_LMCX_PLL_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_PLL_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800880000A8ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_PLL_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_PLL_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800880000B0ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_READ_LEVEL_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_READ_LEVEL_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000140ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_READ_LEVEL_DBG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_READ_LEVEL_DBG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000148ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_READ_LEVEL_RANKX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_LMCX_READ_LEVEL_RANKX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000100ull) + ((offset&3) + (block_id&1)*0xC000000ull)*8;
}

static inline uint64_t CVMX_LMCX_RODT_COMP_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_RODT_COMP_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800880000A0ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_RODT_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_RODT_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000078ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_WODT_CTL0(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_WODT_CTL0(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000030ull) + (block_id&1)*0x60000000ull;
}

static inline uint64_t CVMX_LMCX_WODT_CTL1(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id == 0)))))
        cvmx_warn("CVMX_LMCX_WODT_CTL1(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180088000080ull) + (block_id&1)*0x60000000ull;
}

#define CVMX_MIO_BOOT_BIST_STAT CVMX_MIO_BOOT_BIST_STAT_FUNC()
static inline uint64_t CVMX_MIO_BOOT_BIST_STAT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800000000F8ull);
}

#define CVMX_MIO_BOOT_COMP CVMX_MIO_BOOT_COMP_FUNC()
static inline uint64_t CVMX_MIO_BOOT_COMP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_MIO_BOOT_COMP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800000000B8ull);
}

static inline uint64_t CVMX_MIO_BOOT_DMA_CFGX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 2))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_BOOT_DMA_CFGX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000100ull) + (offset&3)*8;
}

static inline uint64_t CVMX_MIO_BOOT_DMA_INTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 2))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_BOOT_DMA_INTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000138ull) + (offset&3)*8;
}

static inline uint64_t CVMX_MIO_BOOT_DMA_INT_ENX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 2))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_BOOT_DMA_INT_ENX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000150ull) + (offset&3)*8;
}

static inline uint64_t CVMX_MIO_BOOT_DMA_TIMX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 2))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_BOOT_DMA_TIMX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000120ull) + (offset&3)*8;
}

#define CVMX_MIO_BOOT_ERR CVMX_MIO_BOOT_ERR_FUNC()
static inline uint64_t CVMX_MIO_BOOT_ERR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800000000A0ull);
}

#define CVMX_MIO_BOOT_INT CVMX_MIO_BOOT_INT_FUNC()
static inline uint64_t CVMX_MIO_BOOT_INT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800000000A8ull);
}

#define CVMX_MIO_BOOT_LOC_ADR CVMX_MIO_BOOT_LOC_ADR_FUNC()
static inline uint64_t CVMX_MIO_BOOT_LOC_ADR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000000090ull);
}

static inline uint64_t CVMX_MIO_BOOT_LOC_CFGX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_BOOT_LOC_CFGX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000080ull) + (offset&1)*8;
}

#define CVMX_MIO_BOOT_LOC_DAT CVMX_MIO_BOOT_LOC_DAT_FUNC()
static inline uint64_t CVMX_MIO_BOOT_LOC_DAT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000000098ull);
}

#define CVMX_MIO_BOOT_PIN_DEFS CVMX_MIO_BOOT_PIN_DEFS_FUNC()
static inline uint64_t CVMX_MIO_BOOT_PIN_DEFS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_MIO_BOOT_PIN_DEFS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800000000C0ull);
}

static inline uint64_t CVMX_MIO_BOOT_REG_CFGX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_MIO_BOOT_REG_CFGX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000000ull) + (offset&7)*8;
}

static inline uint64_t CVMX_MIO_BOOT_REG_TIMX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_MIO_BOOT_REG_TIMX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000040ull) + (offset&7)*8;
}

#define CVMX_MIO_BOOT_THR CVMX_MIO_BOOT_THR_FUNC()
static inline uint64_t CVMX_MIO_BOOT_THR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800000000B0ull);
}

static inline uint64_t CVMX_MIO_FUS_BNK_DATX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_MIO_FUS_BNK_DATX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001520ull) + (offset&3)*8;
}

#define CVMX_MIO_FUS_DAT0 CVMX_MIO_FUS_DAT0_FUNC()
static inline uint64_t CVMX_MIO_FUS_DAT0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001400ull);
}

#define CVMX_MIO_FUS_DAT1 CVMX_MIO_FUS_DAT1_FUNC()
static inline uint64_t CVMX_MIO_FUS_DAT1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001408ull);
}

#define CVMX_MIO_FUS_DAT2 CVMX_MIO_FUS_DAT2_FUNC()
static inline uint64_t CVMX_MIO_FUS_DAT2_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001410ull);
}

#define CVMX_MIO_FUS_DAT3 CVMX_MIO_FUS_DAT3_FUNC()
static inline uint64_t CVMX_MIO_FUS_DAT3_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001418ull);
}

#define CVMX_MIO_FUS_EMA CVMX_MIO_FUS_EMA_FUNC()
static inline uint64_t CVMX_MIO_FUS_EMA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_MIO_FUS_EMA not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001550ull);
}

#define CVMX_MIO_FUS_PDF CVMX_MIO_FUS_PDF_FUNC()
static inline uint64_t CVMX_MIO_FUS_PDF_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_MIO_FUS_PDF not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001420ull);
}

#define CVMX_MIO_FUS_PLL CVMX_MIO_FUS_PLL_FUNC()
static inline uint64_t CVMX_MIO_FUS_PLL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_MIO_FUS_PLL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001580ull);
}

#define CVMX_MIO_FUS_PROG CVMX_MIO_FUS_PROG_FUNC()
static inline uint64_t CVMX_MIO_FUS_PROG_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001510ull);
}

#define CVMX_MIO_FUS_PROG_TIMES CVMX_MIO_FUS_PROG_TIMES_FUNC()
static inline uint64_t CVMX_MIO_FUS_PROG_TIMES_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_MIO_FUS_PROG_TIMES not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001518ull);
}

#define CVMX_MIO_FUS_RCMD CVMX_MIO_FUS_RCMD_FUNC()
static inline uint64_t CVMX_MIO_FUS_RCMD_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001500ull);
}

#define CVMX_MIO_FUS_SPR_REPAIR_RES CVMX_MIO_FUS_SPR_REPAIR_RES_FUNC()
static inline uint64_t CVMX_MIO_FUS_SPR_REPAIR_RES_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001548ull);
}

#define CVMX_MIO_FUS_SPR_REPAIR_SUM CVMX_MIO_FUS_SPR_REPAIR_SUM_FUNC()
static inline uint64_t CVMX_MIO_FUS_SPR_REPAIR_SUM_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001540ull);
}

#define CVMX_MIO_FUS_UNLOCK CVMX_MIO_FUS_UNLOCK_FUNC()
static inline uint64_t CVMX_MIO_FUS_UNLOCK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_MIO_FUS_UNLOCK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001578ull);
}

#define CVMX_MIO_FUS_WADR CVMX_MIO_FUS_WADR_FUNC()
static inline uint64_t CVMX_MIO_FUS_WADR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180000001508ull);
}

#define CVMX_MIO_NDF_DMA_CFG CVMX_MIO_NDF_DMA_CFG_FUNC()
static inline uint64_t CVMX_MIO_NDF_DMA_CFG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_NDF_DMA_CFG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000168ull);
}

#define CVMX_MIO_NDF_DMA_INT CVMX_MIO_NDF_DMA_INT_FUNC()
static inline uint64_t CVMX_MIO_NDF_DMA_INT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_NDF_DMA_INT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000170ull);
}

#define CVMX_MIO_NDF_DMA_INT_EN CVMX_MIO_NDF_DMA_INT_EN_FUNC()
static inline uint64_t CVMX_MIO_NDF_DMA_INT_EN_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_NDF_DMA_INT_EN not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000178ull);
}

#define CVMX_MIO_PLL_CTL CVMX_MIO_PLL_CTL_FUNC()
static inline uint64_t CVMX_MIO_PLL_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_MIO_PLL_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001448ull);
}

#define CVMX_MIO_PLL_SETTING CVMX_MIO_PLL_SETTING_FUNC()
static inline uint64_t CVMX_MIO_PLL_SETTING_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX)))
        cvmx_warn("CVMX_MIO_PLL_SETTING not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001440ull);
}

static inline uint64_t CVMX_MIO_TWSX_INT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_TWSX_INT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001010ull) + (offset&1)*512;
}

static inline uint64_t CVMX_MIO_TWSX_SW_TWSI(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_TWSX_SW_TWSI(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001000ull) + (offset&1)*512;
}

static inline uint64_t CVMX_MIO_TWSX_SW_TWSI_EXT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_TWSX_SW_TWSI_EXT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001018ull) + (offset&1)*512;
}

static inline uint64_t CVMX_MIO_TWSX_TWSI_SW(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_TWSX_TWSI_SW(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001008ull) + (offset&1)*512;
}

#define CVMX_MIO_UART2_DLH CVMX_MIO_UART2_DLH_FUNC()
static inline uint64_t CVMX_MIO_UART2_DLH_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_DLH not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000488ull);
}

#define CVMX_MIO_UART2_DLL CVMX_MIO_UART2_DLL_FUNC()
static inline uint64_t CVMX_MIO_UART2_DLL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_DLL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000480ull);
}

#define CVMX_MIO_UART2_FAR CVMX_MIO_UART2_FAR_FUNC()
static inline uint64_t CVMX_MIO_UART2_FAR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_FAR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000520ull);
}

#define CVMX_MIO_UART2_FCR CVMX_MIO_UART2_FCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_FCR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_FCR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000450ull);
}

#define CVMX_MIO_UART2_HTX CVMX_MIO_UART2_HTX_FUNC()
static inline uint64_t CVMX_MIO_UART2_HTX_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_HTX not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000708ull);
}

#define CVMX_MIO_UART2_IER CVMX_MIO_UART2_IER_FUNC()
static inline uint64_t CVMX_MIO_UART2_IER_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_IER not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000408ull);
}

#define CVMX_MIO_UART2_IIR CVMX_MIO_UART2_IIR_FUNC()
static inline uint64_t CVMX_MIO_UART2_IIR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_IIR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000410ull);
}

#define CVMX_MIO_UART2_LCR CVMX_MIO_UART2_LCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_LCR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_LCR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000418ull);
}

#define CVMX_MIO_UART2_LSR CVMX_MIO_UART2_LSR_FUNC()
static inline uint64_t CVMX_MIO_UART2_LSR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_LSR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000428ull);
}

#define CVMX_MIO_UART2_MCR CVMX_MIO_UART2_MCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_MCR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_MCR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000420ull);
}

#define CVMX_MIO_UART2_MSR CVMX_MIO_UART2_MSR_FUNC()
static inline uint64_t CVMX_MIO_UART2_MSR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_MSR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000430ull);
}

#define CVMX_MIO_UART2_RBR CVMX_MIO_UART2_RBR_FUNC()
static inline uint64_t CVMX_MIO_UART2_RBR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_RBR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000400ull);
}

#define CVMX_MIO_UART2_RFL CVMX_MIO_UART2_RFL_FUNC()
static inline uint64_t CVMX_MIO_UART2_RFL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_RFL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000608ull);
}

#define CVMX_MIO_UART2_RFW CVMX_MIO_UART2_RFW_FUNC()
static inline uint64_t CVMX_MIO_UART2_RFW_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_RFW not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000530ull);
}

#define CVMX_MIO_UART2_SBCR CVMX_MIO_UART2_SBCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_SBCR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_SBCR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000620ull);
}

#define CVMX_MIO_UART2_SCR CVMX_MIO_UART2_SCR_FUNC()
static inline uint64_t CVMX_MIO_UART2_SCR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_SCR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000438ull);
}

#define CVMX_MIO_UART2_SFE CVMX_MIO_UART2_SFE_FUNC()
static inline uint64_t CVMX_MIO_UART2_SFE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_SFE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000630ull);
}

#define CVMX_MIO_UART2_SRR CVMX_MIO_UART2_SRR_FUNC()
static inline uint64_t CVMX_MIO_UART2_SRR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_SRR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000610ull);
}

#define CVMX_MIO_UART2_SRT CVMX_MIO_UART2_SRT_FUNC()
static inline uint64_t CVMX_MIO_UART2_SRT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_SRT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000638ull);
}

#define CVMX_MIO_UART2_SRTS CVMX_MIO_UART2_SRTS_FUNC()
static inline uint64_t CVMX_MIO_UART2_SRTS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_SRTS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000618ull);
}

#define CVMX_MIO_UART2_STT CVMX_MIO_UART2_STT_FUNC()
static inline uint64_t CVMX_MIO_UART2_STT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_STT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000700ull);
}

#define CVMX_MIO_UART2_TFL CVMX_MIO_UART2_TFL_FUNC()
static inline uint64_t CVMX_MIO_UART2_TFL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_TFL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000600ull);
}

#define CVMX_MIO_UART2_TFR CVMX_MIO_UART2_TFR_FUNC()
static inline uint64_t CVMX_MIO_UART2_TFR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_TFR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000528ull);
}

#define CVMX_MIO_UART2_THR CVMX_MIO_UART2_THR_FUNC()
static inline uint64_t CVMX_MIO_UART2_THR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_THR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000440ull);
}

#define CVMX_MIO_UART2_USR CVMX_MIO_UART2_USR_FUNC()
static inline uint64_t CVMX_MIO_UART2_USR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_MIO_UART2_USR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000538ull);
}

static inline uint64_t CVMX_MIO_UARTX_DLH(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_DLH(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000888ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_DLL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_DLL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000880ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_FAR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_FAR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000920ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_FCR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_FCR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000850ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_HTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_HTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000B08ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_IER(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_IER(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000808ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_IIR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_IIR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000810ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_LCR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_LCR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000818ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_LSR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_LSR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000828ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_MCR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_MCR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000820ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_MSR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_MSR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000830ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_RBR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_RBR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000800ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_RFL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_RFL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000A08ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_RFW(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_RFW(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000930ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_SBCR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_SBCR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000A20ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_SCR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_SCR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000838ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_SFE(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_SFE(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000A30ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_SRR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_SRR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000A10ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_SRT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_SRT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000A38ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_SRTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_SRTS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000A18ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_STT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_STT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000B00ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_TFL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_TFL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000A00ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_TFR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_TFR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000928ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_THR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_THR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000840ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIO_UARTX_USR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIO_UARTX_USR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000000938ull) + (offset&1)*1024;
}

static inline uint64_t CVMX_MIXX_BIST(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_BIST(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100078ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_CTL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_CTL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100020ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_INTENA(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_INTENA(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100050ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_IRCNT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_IRCNT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100030ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_IRHWM(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_IRHWM(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100028ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_IRING1(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_IRING1(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100010ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_IRING2(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_IRING2(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100018ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_ISR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_ISR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100048ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_ORCNT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_ORCNT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100040ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_ORHWM(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_ORHWM(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100038ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_ORING1(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_ORING1(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100000ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_ORING2(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_ORING2(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100008ull) + (offset&1)*2048;
}

static inline uint64_t CVMX_MIXX_REMCNT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_MIXX_REMCNT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000100058ull) + (offset&1)*2048;
}

#define CVMX_MPI_CFG CVMX_MPI_CFG_FUNC()
static inline uint64_t CVMX_MPI_CFG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)))
        cvmx_warn("CVMX_MPI_CFG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070000001000ull);
}

static inline uint64_t CVMX_MPI_DATX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 8))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 8))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 8)))))
        cvmx_warn("CVMX_MPI_DATX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000001080ull) + (offset&15)*8;
}

#define CVMX_MPI_STS CVMX_MPI_STS_FUNC()
static inline uint64_t CVMX_MPI_STS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)))
        cvmx_warn("CVMX_MPI_STS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070000001008ull);
}

#define CVMX_MPI_TX CVMX_MPI_TX_FUNC()
static inline uint64_t CVMX_MPI_TX_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN30XX) || OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN50XX)))
        cvmx_warn("CVMX_MPI_TX not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070000001010ull);
}

#define CVMX_NDF_BT_PG_INFO CVMX_NDF_BT_PG_INFO_FUNC()
static inline uint64_t CVMX_NDF_BT_PG_INFO_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NDF_BT_PG_INFO not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070001000018ull);
}

#define CVMX_NDF_CMD CVMX_NDF_CMD_FUNC()
static inline uint64_t CVMX_NDF_CMD_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NDF_CMD not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070001000000ull);
}

#define CVMX_NDF_DRBELL CVMX_NDF_DRBELL_FUNC()
static inline uint64_t CVMX_NDF_DRBELL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NDF_DRBELL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070001000030ull);
}

#define CVMX_NDF_ECC_CNT CVMX_NDF_ECC_CNT_FUNC()
static inline uint64_t CVMX_NDF_ECC_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NDF_ECC_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070001000010ull);
}

#define CVMX_NDF_INT CVMX_NDF_INT_FUNC()
static inline uint64_t CVMX_NDF_INT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NDF_INT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070001000020ull);
}

#define CVMX_NDF_INT_EN CVMX_NDF_INT_EN_FUNC()
static inline uint64_t CVMX_NDF_INT_EN_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NDF_INT_EN not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070001000028ull);
}

#define CVMX_NDF_MISC CVMX_NDF_MISC_FUNC()
static inline uint64_t CVMX_NDF_MISC_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NDF_MISC not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070001000008ull);
}

#define CVMX_NDF_ST_REG CVMX_NDF_ST_REG_FUNC()
static inline uint64_t CVMX_NDF_ST_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NDF_ST_REG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001070001000038ull);
}

static inline uint64_t CVMX_NPEI_BAR1_INDEXX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_BAR1_INDEXX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000000ull + (offset&31)*16;
}

#define CVMX_NPEI_BIST_STATUS CVMX_NPEI_BIST_STATUS_FUNC()
static inline uint64_t CVMX_NPEI_BIST_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_BIST_STATUS not supported on this chip\n");
#endif
    return 0x0000000000000580ull;
}

#define CVMX_NPEI_BIST_STATUS2 CVMX_NPEI_BIST_STATUS2_FUNC()
static inline uint64_t CVMX_NPEI_BIST_STATUS2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_BIST_STATUS2 not supported on this chip\n");
#endif
    return 0x0000000000000680ull;
}

#define CVMX_NPEI_CTL_PORT0 CVMX_NPEI_CTL_PORT0_FUNC()
static inline uint64_t CVMX_NPEI_CTL_PORT0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_CTL_PORT0 not supported on this chip\n");
#endif
    return 0x0000000000000250ull;
}

#define CVMX_NPEI_CTL_PORT1 CVMX_NPEI_CTL_PORT1_FUNC()
static inline uint64_t CVMX_NPEI_CTL_PORT1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_CTL_PORT1 not supported on this chip\n");
#endif
    return 0x0000000000000260ull;
}

#define CVMX_NPEI_CTL_STATUS CVMX_NPEI_CTL_STATUS_FUNC()
static inline uint64_t CVMX_NPEI_CTL_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_CTL_STATUS not supported on this chip\n");
#endif
    return 0x0000000000000570ull;
}

#define CVMX_NPEI_CTL_STATUS2 CVMX_NPEI_CTL_STATUS2_FUNC()
static inline uint64_t CVMX_NPEI_CTL_STATUS2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_CTL_STATUS2 not supported on this chip\n");
#endif
    return 0x0000000000003C00ull;
}

#define CVMX_NPEI_DATA_OUT_CNT CVMX_NPEI_DATA_OUT_CNT_FUNC()
static inline uint64_t CVMX_NPEI_DATA_OUT_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_DATA_OUT_CNT not supported on this chip\n");
#endif
    return 0x00000000000005F0ull;
}

#define CVMX_NPEI_DBG_DATA CVMX_NPEI_DBG_DATA_FUNC()
static inline uint64_t CVMX_NPEI_DBG_DATA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_DBG_DATA not supported on this chip\n");
#endif
    return 0x0000000000000510ull;
}

#define CVMX_NPEI_DBG_SELECT CVMX_NPEI_DBG_SELECT_FUNC()
static inline uint64_t CVMX_NPEI_DBG_SELECT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_DBG_SELECT not supported on this chip\n");
#endif
    return 0x0000000000000500ull;
}

#define CVMX_NPEI_DMA0_INT_LEVEL CVMX_NPEI_DMA0_INT_LEVEL_FUNC()
static inline uint64_t CVMX_NPEI_DMA0_INT_LEVEL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_DMA0_INT_LEVEL not supported on this chip\n");
#endif
    return 0x00000000000005C0ull;
}

#define CVMX_NPEI_DMA1_INT_LEVEL CVMX_NPEI_DMA1_INT_LEVEL_FUNC()
static inline uint64_t CVMX_NPEI_DMA1_INT_LEVEL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_DMA1_INT_LEVEL not supported on this chip\n");
#endif
    return 0x00000000000005D0ull;
}

static inline uint64_t CVMX_NPEI_DMAX_COUNTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 4)))))
        cvmx_warn("CVMX_NPEI_DMAX_COUNTS(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000450ull + (offset&7)*16;
}

static inline uint64_t CVMX_NPEI_DMAX_DBELL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 4)))))
        cvmx_warn("CVMX_NPEI_DMAX_DBELL(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000003B0ull + (offset&7)*16;
}

static inline uint64_t CVMX_NPEI_DMAX_IBUFF_SADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 4)))))
        cvmx_warn("CVMX_NPEI_DMAX_IBUFF_SADDR(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000400ull + (offset&7)*16;
}

static inline uint64_t CVMX_NPEI_DMAX_NADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 4)))))
        cvmx_warn("CVMX_NPEI_DMAX_NADDR(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000004A0ull + (offset&7)*16;
}

#define CVMX_NPEI_DMA_CNTS CVMX_NPEI_DMA_CNTS_FUNC()
static inline uint64_t CVMX_NPEI_DMA_CNTS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_DMA_CNTS not supported on this chip\n");
#endif
    return 0x00000000000005E0ull;
}

#define CVMX_NPEI_DMA_CONTROL CVMX_NPEI_DMA_CONTROL_FUNC()
static inline uint64_t CVMX_NPEI_DMA_CONTROL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_DMA_CONTROL not supported on this chip\n");
#endif
    return 0x00000000000003A0ull;
}

#define CVMX_NPEI_DMA_PCIE_REQ_NUM CVMX_NPEI_DMA_PCIE_REQ_NUM_FUNC()
static inline uint64_t CVMX_NPEI_DMA_PCIE_REQ_NUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_DMA_PCIE_REQ_NUM not supported on this chip\n");
#endif
    return 0x00000000000005B0ull;
}

#define CVMX_NPEI_DMA_STATE1 CVMX_NPEI_DMA_STATE1_FUNC()
static inline uint64_t CVMX_NPEI_DMA_STATE1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NPEI_DMA_STATE1 not supported on this chip\n");
#endif
    return 0x00000000000006C0ull;
}

#define CVMX_NPEI_DMA_STATE1_P1 CVMX_NPEI_DMA_STATE1_P1_FUNC()
static inline uint64_t CVMX_NPEI_DMA_STATE1_P1_FUNC(void)
{
    return 0x0000000000000680ull;
}

#define CVMX_NPEI_DMA_STATE2 CVMX_NPEI_DMA_STATE2_FUNC()
static inline uint64_t CVMX_NPEI_DMA_STATE2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_NPEI_DMA_STATE2 not supported on this chip\n");
#endif
    return 0x00000000000006D0ull;
}

#define CVMX_NPEI_DMA_STATE2_P1 CVMX_NPEI_DMA_STATE2_P1_FUNC()
static inline uint64_t CVMX_NPEI_DMA_STATE2_P1_FUNC(void)
{
    return 0x0000000000000690ull;
}

#define CVMX_NPEI_DMA_STATE3_P1 CVMX_NPEI_DMA_STATE3_P1_FUNC()
static inline uint64_t CVMX_NPEI_DMA_STATE3_P1_FUNC(void)
{
    return 0x00000000000006A0ull;
}

#define CVMX_NPEI_DMA_STATE4_P1 CVMX_NPEI_DMA_STATE4_P1_FUNC()
static inline uint64_t CVMX_NPEI_DMA_STATE4_P1_FUNC(void)
{
    return 0x00000000000006B0ull;
}

#define CVMX_NPEI_DMA_STATE5_P1 CVMX_NPEI_DMA_STATE5_P1_FUNC()
static inline uint64_t CVMX_NPEI_DMA_STATE5_P1_FUNC(void)
{
    return 0x00000000000006C0ull;
}

#define CVMX_NPEI_INT_A_ENB CVMX_NPEI_INT_A_ENB_FUNC()
static inline uint64_t CVMX_NPEI_INT_A_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_INT_A_ENB not supported on this chip\n");
#endif
    return 0x0000000000000560ull;
}

#define CVMX_NPEI_INT_A_ENB2 CVMX_NPEI_INT_A_ENB2_FUNC()
static inline uint64_t CVMX_NPEI_INT_A_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_INT_A_ENB2 not supported on this chip\n");
#endif
    return 0x0000000000003CE0ull;
}

#define CVMX_NPEI_INT_A_SUM CVMX_NPEI_INT_A_SUM_FUNC()
static inline uint64_t CVMX_NPEI_INT_A_SUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_INT_A_SUM not supported on this chip\n");
#endif
    return 0x0000000000000550ull;
}

#define CVMX_NPEI_INT_ENB CVMX_NPEI_INT_ENB_FUNC()
static inline uint64_t CVMX_NPEI_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_INT_ENB not supported on this chip\n");
#endif
    return 0x0000000000000540ull;
}

#define CVMX_NPEI_INT_ENB2 CVMX_NPEI_INT_ENB2_FUNC()
static inline uint64_t CVMX_NPEI_INT_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_INT_ENB2 not supported on this chip\n");
#endif
    return 0x0000000000003CD0ull;
}

#define CVMX_NPEI_INT_INFO CVMX_NPEI_INT_INFO_FUNC()
static inline uint64_t CVMX_NPEI_INT_INFO_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_INT_INFO not supported on this chip\n");
#endif
    return 0x0000000000000590ull;
}

#define CVMX_NPEI_INT_SUM CVMX_NPEI_INT_SUM_FUNC()
static inline uint64_t CVMX_NPEI_INT_SUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_INT_SUM not supported on this chip\n");
#endif
    return 0x0000000000000530ull;
}

#define CVMX_NPEI_INT_SUM2 CVMX_NPEI_INT_SUM2_FUNC()
static inline uint64_t CVMX_NPEI_INT_SUM2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_INT_SUM2 not supported on this chip\n");
#endif
    return 0x0000000000003CC0ull;
}

#define CVMX_NPEI_LAST_WIN_RDATA0 CVMX_NPEI_LAST_WIN_RDATA0_FUNC()
static inline uint64_t CVMX_NPEI_LAST_WIN_RDATA0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_LAST_WIN_RDATA0 not supported on this chip\n");
#endif
    return 0x0000000000000600ull;
}

#define CVMX_NPEI_LAST_WIN_RDATA1 CVMX_NPEI_LAST_WIN_RDATA1_FUNC()
static inline uint64_t CVMX_NPEI_LAST_WIN_RDATA1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_LAST_WIN_RDATA1 not supported on this chip\n");
#endif
    return 0x0000000000000610ull;
}

#define CVMX_NPEI_MEM_ACCESS_CTL CVMX_NPEI_MEM_ACCESS_CTL_FUNC()
static inline uint64_t CVMX_NPEI_MEM_ACCESS_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MEM_ACCESS_CTL not supported on this chip\n");
#endif
    return 0x00000000000004F0ull;
}

static inline uint64_t CVMX_NPEI_MEM_ACCESS_SUBIDX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset >= 12) && (offset <= 27)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset >= 12) && (offset <= 27))))))
        cvmx_warn("CVMX_NPEI_MEM_ACCESS_SUBIDX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000340ull + (offset&31)*16 - 16*12;
}

#define CVMX_NPEI_MSI_ENB0 CVMX_NPEI_MSI_ENB0_FUNC()
static inline uint64_t CVMX_NPEI_MSI_ENB0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_ENB0 not supported on this chip\n");
#endif
    return 0x0000000000003C50ull;
}

#define CVMX_NPEI_MSI_ENB1 CVMX_NPEI_MSI_ENB1_FUNC()
static inline uint64_t CVMX_NPEI_MSI_ENB1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_ENB1 not supported on this chip\n");
#endif
    return 0x0000000000003C60ull;
}

#define CVMX_NPEI_MSI_ENB2 CVMX_NPEI_MSI_ENB2_FUNC()
static inline uint64_t CVMX_NPEI_MSI_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_ENB2 not supported on this chip\n");
#endif
    return 0x0000000000003C70ull;
}

#define CVMX_NPEI_MSI_ENB3 CVMX_NPEI_MSI_ENB3_FUNC()
static inline uint64_t CVMX_NPEI_MSI_ENB3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_ENB3 not supported on this chip\n");
#endif
    return 0x0000000000003C80ull;
}

#define CVMX_NPEI_MSI_RCV0 CVMX_NPEI_MSI_RCV0_FUNC()
static inline uint64_t CVMX_NPEI_MSI_RCV0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_RCV0 not supported on this chip\n");
#endif
    return 0x0000000000003C10ull;
}

#define CVMX_NPEI_MSI_RCV1 CVMX_NPEI_MSI_RCV1_FUNC()
static inline uint64_t CVMX_NPEI_MSI_RCV1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_RCV1 not supported on this chip\n");
#endif
    return 0x0000000000003C20ull;
}

#define CVMX_NPEI_MSI_RCV2 CVMX_NPEI_MSI_RCV2_FUNC()
static inline uint64_t CVMX_NPEI_MSI_RCV2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_RCV2 not supported on this chip\n");
#endif
    return 0x0000000000003C30ull;
}

#define CVMX_NPEI_MSI_RCV3 CVMX_NPEI_MSI_RCV3_FUNC()
static inline uint64_t CVMX_NPEI_MSI_RCV3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_RCV3 not supported on this chip\n");
#endif
    return 0x0000000000003C40ull;
}

#define CVMX_NPEI_MSI_RD_MAP CVMX_NPEI_MSI_RD_MAP_FUNC()
static inline uint64_t CVMX_NPEI_MSI_RD_MAP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_RD_MAP not supported on this chip\n");
#endif
    return 0x0000000000003CA0ull;
}

#define CVMX_NPEI_MSI_W1C_ENB0 CVMX_NPEI_MSI_W1C_ENB0_FUNC()
static inline uint64_t CVMX_NPEI_MSI_W1C_ENB0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_W1C_ENB0 not supported on this chip\n");
#endif
    return 0x0000000000003CF0ull;
}

#define CVMX_NPEI_MSI_W1C_ENB1 CVMX_NPEI_MSI_W1C_ENB1_FUNC()
static inline uint64_t CVMX_NPEI_MSI_W1C_ENB1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_W1C_ENB1 not supported on this chip\n");
#endif
    return 0x0000000000003D00ull;
}

#define CVMX_NPEI_MSI_W1C_ENB2 CVMX_NPEI_MSI_W1C_ENB2_FUNC()
static inline uint64_t CVMX_NPEI_MSI_W1C_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_W1C_ENB2 not supported on this chip\n");
#endif
    return 0x0000000000003D10ull;
}

#define CVMX_NPEI_MSI_W1C_ENB3 CVMX_NPEI_MSI_W1C_ENB3_FUNC()
static inline uint64_t CVMX_NPEI_MSI_W1C_ENB3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_W1C_ENB3 not supported on this chip\n");
#endif
    return 0x0000000000003D20ull;
}

#define CVMX_NPEI_MSI_W1S_ENB0 CVMX_NPEI_MSI_W1S_ENB0_FUNC()
static inline uint64_t CVMX_NPEI_MSI_W1S_ENB0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_W1S_ENB0 not supported on this chip\n");
#endif
    return 0x0000000000003D30ull;
}

#define CVMX_NPEI_MSI_W1S_ENB1 CVMX_NPEI_MSI_W1S_ENB1_FUNC()
static inline uint64_t CVMX_NPEI_MSI_W1S_ENB1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_W1S_ENB1 not supported on this chip\n");
#endif
    return 0x0000000000003D40ull;
}

#define CVMX_NPEI_MSI_W1S_ENB2 CVMX_NPEI_MSI_W1S_ENB2_FUNC()
static inline uint64_t CVMX_NPEI_MSI_W1S_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_W1S_ENB2 not supported on this chip\n");
#endif
    return 0x0000000000003D50ull;
}

#define CVMX_NPEI_MSI_W1S_ENB3 CVMX_NPEI_MSI_W1S_ENB3_FUNC()
static inline uint64_t CVMX_NPEI_MSI_W1S_ENB3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_W1S_ENB3 not supported on this chip\n");
#endif
    return 0x0000000000003D60ull;
}

#define CVMX_NPEI_MSI_WR_MAP CVMX_NPEI_MSI_WR_MAP_FUNC()
static inline uint64_t CVMX_NPEI_MSI_WR_MAP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_MSI_WR_MAP not supported on this chip\n");
#endif
    return 0x0000000000003C90ull;
}

#define CVMX_NPEI_PCIE_CREDIT_CNT CVMX_NPEI_PCIE_CREDIT_CNT_FUNC()
static inline uint64_t CVMX_NPEI_PCIE_CREDIT_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PCIE_CREDIT_CNT not supported on this chip\n");
#endif
    return 0x0000000000003D70ull;
}

#define CVMX_NPEI_PCIE_MSI_RCV CVMX_NPEI_PCIE_MSI_RCV_FUNC()
static inline uint64_t CVMX_NPEI_PCIE_MSI_RCV_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PCIE_MSI_RCV not supported on this chip\n");
#endif
    return 0x0000000000003CB0ull;
}

#define CVMX_NPEI_PCIE_MSI_RCV_B1 CVMX_NPEI_PCIE_MSI_RCV_B1_FUNC()
static inline uint64_t CVMX_NPEI_PCIE_MSI_RCV_B1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PCIE_MSI_RCV_B1 not supported on this chip\n");
#endif
    return 0x0000000000000650ull;
}

#define CVMX_NPEI_PCIE_MSI_RCV_B2 CVMX_NPEI_PCIE_MSI_RCV_B2_FUNC()
static inline uint64_t CVMX_NPEI_PCIE_MSI_RCV_B2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PCIE_MSI_RCV_B2 not supported on this chip\n");
#endif
    return 0x0000000000000660ull;
}

#define CVMX_NPEI_PCIE_MSI_RCV_B3 CVMX_NPEI_PCIE_MSI_RCV_B3_FUNC()
static inline uint64_t CVMX_NPEI_PCIE_MSI_RCV_B3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PCIE_MSI_RCV_B3 not supported on this chip\n");
#endif
    return 0x0000000000000670ull;
}

static inline uint64_t CVMX_NPEI_PKTX_CNTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_CNTS(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000002400ull + (offset&31)*16;
}

static inline uint64_t CVMX_NPEI_PKTX_INSTR_BADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_INSTR_BADDR(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000002800ull + (offset&31)*16;
}

static inline uint64_t CVMX_NPEI_PKTX_INSTR_BAOFF_DBELL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_INSTR_BAOFF_DBELL(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000002C00ull + (offset&31)*16;
}

static inline uint64_t CVMX_NPEI_PKTX_INSTR_FIFO_RSIZE(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_INSTR_FIFO_RSIZE(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000003000ull + (offset&31)*16;
}

static inline uint64_t CVMX_NPEI_PKTX_INSTR_HEADER(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_INSTR_HEADER(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000003400ull + (offset&31)*16;
}

static inline uint64_t CVMX_NPEI_PKTX_IN_BP(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_IN_BP(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000003800ull + (offset&31)*16;
}

static inline uint64_t CVMX_NPEI_PKTX_SLIST_BADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_SLIST_BADDR(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000001400ull + (offset&31)*16;
}

static inline uint64_t CVMX_NPEI_PKTX_SLIST_BAOFF_DBELL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_SLIST_BAOFF_DBELL(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000001800ull + (offset&31)*16;
}

static inline uint64_t CVMX_NPEI_PKTX_SLIST_FIFO_RSIZE(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKTX_SLIST_FIFO_RSIZE(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000001C00ull + (offset&31)*16;
}

#define CVMX_NPEI_PKT_CNT_INT CVMX_NPEI_PKT_CNT_INT_FUNC()
static inline uint64_t CVMX_NPEI_PKT_CNT_INT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_CNT_INT not supported on this chip\n");
#endif
    return 0x0000000000001110ull;
}

#define CVMX_NPEI_PKT_CNT_INT_ENB CVMX_NPEI_PKT_CNT_INT_ENB_FUNC()
static inline uint64_t CVMX_NPEI_PKT_CNT_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_CNT_INT_ENB not supported on this chip\n");
#endif
    return 0x0000000000001130ull;
}

#define CVMX_NPEI_PKT_DATA_OUT_ES CVMX_NPEI_PKT_DATA_OUT_ES_FUNC()
static inline uint64_t CVMX_NPEI_PKT_DATA_OUT_ES_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_DATA_OUT_ES not supported on this chip\n");
#endif
    return 0x00000000000010B0ull;
}

#define CVMX_NPEI_PKT_DATA_OUT_NS CVMX_NPEI_PKT_DATA_OUT_NS_FUNC()
static inline uint64_t CVMX_NPEI_PKT_DATA_OUT_NS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_DATA_OUT_NS not supported on this chip\n");
#endif
    return 0x00000000000010A0ull;
}

#define CVMX_NPEI_PKT_DATA_OUT_ROR CVMX_NPEI_PKT_DATA_OUT_ROR_FUNC()
static inline uint64_t CVMX_NPEI_PKT_DATA_OUT_ROR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_DATA_OUT_ROR not supported on this chip\n");
#endif
    return 0x0000000000001090ull;
}

#define CVMX_NPEI_PKT_DPADDR CVMX_NPEI_PKT_DPADDR_FUNC()
static inline uint64_t CVMX_NPEI_PKT_DPADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_DPADDR not supported on this chip\n");
#endif
    return 0x0000000000001080ull;
}

#define CVMX_NPEI_PKT_INPUT_CONTROL CVMX_NPEI_PKT_INPUT_CONTROL_FUNC()
static inline uint64_t CVMX_NPEI_PKT_INPUT_CONTROL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_INPUT_CONTROL not supported on this chip\n");
#endif
    return 0x0000000000001150ull;
}

#define CVMX_NPEI_PKT_INSTR_ENB CVMX_NPEI_PKT_INSTR_ENB_FUNC()
static inline uint64_t CVMX_NPEI_PKT_INSTR_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_INSTR_ENB not supported on this chip\n");
#endif
    return 0x0000000000001000ull;
}

#define CVMX_NPEI_PKT_INSTR_RD_SIZE CVMX_NPEI_PKT_INSTR_RD_SIZE_FUNC()
static inline uint64_t CVMX_NPEI_PKT_INSTR_RD_SIZE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_INSTR_RD_SIZE not supported on this chip\n");
#endif
    return 0x0000000000001190ull;
}

#define CVMX_NPEI_PKT_INSTR_SIZE CVMX_NPEI_PKT_INSTR_SIZE_FUNC()
static inline uint64_t CVMX_NPEI_PKT_INSTR_SIZE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_INSTR_SIZE not supported on this chip\n");
#endif
    return 0x0000000000001020ull;
}

#define CVMX_NPEI_PKT_INT_LEVELS CVMX_NPEI_PKT_INT_LEVELS_FUNC()
static inline uint64_t CVMX_NPEI_PKT_INT_LEVELS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_INT_LEVELS not supported on this chip\n");
#endif
    return 0x0000000000001100ull;
}

#define CVMX_NPEI_PKT_IN_BP CVMX_NPEI_PKT_IN_BP_FUNC()
static inline uint64_t CVMX_NPEI_PKT_IN_BP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_IN_BP not supported on this chip\n");
#endif
    return 0x00000000000006B0ull;
}

static inline uint64_t CVMX_NPEI_PKT_IN_DONEX_CNTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPEI_PKT_IN_DONEX_CNTS(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000002000ull + (offset&31)*16;
}

#define CVMX_NPEI_PKT_IN_INSTR_COUNTS CVMX_NPEI_PKT_IN_INSTR_COUNTS_FUNC()
static inline uint64_t CVMX_NPEI_PKT_IN_INSTR_COUNTS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_IN_INSTR_COUNTS not supported on this chip\n");
#endif
    return 0x00000000000006A0ull;
}

#define CVMX_NPEI_PKT_IN_PCIE_PORT CVMX_NPEI_PKT_IN_PCIE_PORT_FUNC()
static inline uint64_t CVMX_NPEI_PKT_IN_PCIE_PORT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_IN_PCIE_PORT not supported on this chip\n");
#endif
    return 0x00000000000011A0ull;
}

#define CVMX_NPEI_PKT_IPTR CVMX_NPEI_PKT_IPTR_FUNC()
static inline uint64_t CVMX_NPEI_PKT_IPTR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_IPTR not supported on this chip\n");
#endif
    return 0x0000000000001070ull;
}

#define CVMX_NPEI_PKT_OUTPUT_WMARK CVMX_NPEI_PKT_OUTPUT_WMARK_FUNC()
static inline uint64_t CVMX_NPEI_PKT_OUTPUT_WMARK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_OUTPUT_WMARK not supported on this chip\n");
#endif
    return 0x0000000000001160ull;
}

#define CVMX_NPEI_PKT_OUT_BMODE CVMX_NPEI_PKT_OUT_BMODE_FUNC()
static inline uint64_t CVMX_NPEI_PKT_OUT_BMODE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_OUT_BMODE not supported on this chip\n");
#endif
    return 0x00000000000010D0ull;
}

#define CVMX_NPEI_PKT_OUT_ENB CVMX_NPEI_PKT_OUT_ENB_FUNC()
static inline uint64_t CVMX_NPEI_PKT_OUT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_OUT_ENB not supported on this chip\n");
#endif
    return 0x0000000000001010ull;
}

#define CVMX_NPEI_PKT_PCIE_PORT CVMX_NPEI_PKT_PCIE_PORT_FUNC()
static inline uint64_t CVMX_NPEI_PKT_PCIE_PORT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_PCIE_PORT not supported on this chip\n");
#endif
    return 0x00000000000010E0ull;
}

#define CVMX_NPEI_PKT_PORT_IN_RST CVMX_NPEI_PKT_PORT_IN_RST_FUNC()
static inline uint64_t CVMX_NPEI_PKT_PORT_IN_RST_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_PORT_IN_RST not supported on this chip\n");
#endif
    return 0x0000000000000690ull;
}

#define CVMX_NPEI_PKT_SLIST_ES CVMX_NPEI_PKT_SLIST_ES_FUNC()
static inline uint64_t CVMX_NPEI_PKT_SLIST_ES_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_SLIST_ES not supported on this chip\n");
#endif
    return 0x0000000000001050ull;
}

#define CVMX_NPEI_PKT_SLIST_ID_SIZE CVMX_NPEI_PKT_SLIST_ID_SIZE_FUNC()
static inline uint64_t CVMX_NPEI_PKT_SLIST_ID_SIZE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_SLIST_ID_SIZE not supported on this chip\n");
#endif
    return 0x0000000000001180ull;
}

#define CVMX_NPEI_PKT_SLIST_NS CVMX_NPEI_PKT_SLIST_NS_FUNC()
static inline uint64_t CVMX_NPEI_PKT_SLIST_NS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_SLIST_NS not supported on this chip\n");
#endif
    return 0x0000000000001040ull;
}

#define CVMX_NPEI_PKT_SLIST_ROR CVMX_NPEI_PKT_SLIST_ROR_FUNC()
static inline uint64_t CVMX_NPEI_PKT_SLIST_ROR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_SLIST_ROR not supported on this chip\n");
#endif
    return 0x0000000000001030ull;
}

#define CVMX_NPEI_PKT_TIME_INT CVMX_NPEI_PKT_TIME_INT_FUNC()
static inline uint64_t CVMX_NPEI_PKT_TIME_INT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_TIME_INT not supported on this chip\n");
#endif
    return 0x0000000000001120ull;
}

#define CVMX_NPEI_PKT_TIME_INT_ENB CVMX_NPEI_PKT_TIME_INT_ENB_FUNC()
static inline uint64_t CVMX_NPEI_PKT_TIME_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_PKT_TIME_INT_ENB not supported on this chip\n");
#endif
    return 0x0000000000001140ull;
}

#define CVMX_NPEI_RSL_INT_BLOCKS CVMX_NPEI_RSL_INT_BLOCKS_FUNC()
static inline uint64_t CVMX_NPEI_RSL_INT_BLOCKS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_RSL_INT_BLOCKS not supported on this chip\n");
#endif
    return 0x0000000000000520ull;
}

#define CVMX_NPEI_SCRATCH_1 CVMX_NPEI_SCRATCH_1_FUNC()
static inline uint64_t CVMX_NPEI_SCRATCH_1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_SCRATCH_1 not supported on this chip\n");
#endif
    return 0x0000000000000270ull;
}

#define CVMX_NPEI_STATE1 CVMX_NPEI_STATE1_FUNC()
static inline uint64_t CVMX_NPEI_STATE1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_STATE1 not supported on this chip\n");
#endif
    return 0x0000000000000620ull;
}

#define CVMX_NPEI_STATE2 CVMX_NPEI_STATE2_FUNC()
static inline uint64_t CVMX_NPEI_STATE2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_STATE2 not supported on this chip\n");
#endif
    return 0x0000000000000630ull;
}

#define CVMX_NPEI_STATE3 CVMX_NPEI_STATE3_FUNC()
static inline uint64_t CVMX_NPEI_STATE3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_STATE3 not supported on this chip\n");
#endif
    return 0x0000000000000640ull;
}

#define CVMX_NPEI_WINDOW_CTL CVMX_NPEI_WINDOW_CTL_FUNC()
static inline uint64_t CVMX_NPEI_WINDOW_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_WINDOW_CTL not supported on this chip\n");
#endif
    return 0x0000000000000380ull;
}

#define CVMX_NPEI_WIN_RD_ADDR CVMX_NPEI_WIN_RD_ADDR_FUNC()
static inline uint64_t CVMX_NPEI_WIN_RD_ADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_WIN_RD_ADDR not supported on this chip\n");
#endif
    return 0x0000000000000210ull;
}

#define CVMX_NPEI_WIN_RD_DATA CVMX_NPEI_WIN_RD_DATA_FUNC()
static inline uint64_t CVMX_NPEI_WIN_RD_DATA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_WIN_RD_DATA not supported on this chip\n");
#endif
    return 0x0000000000000240ull;
}

#define CVMX_NPEI_WIN_WR_ADDR CVMX_NPEI_WIN_WR_ADDR_FUNC()
static inline uint64_t CVMX_NPEI_WIN_WR_ADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_WIN_WR_ADDR not supported on this chip\n");
#endif
    return 0x0000000000000200ull;
}

#define CVMX_NPEI_WIN_WR_DATA CVMX_NPEI_WIN_WR_DATA_FUNC()
static inline uint64_t CVMX_NPEI_WIN_WR_DATA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_WIN_WR_DATA not supported on this chip\n");
#endif
    return 0x0000000000000220ull;
}

#define CVMX_NPEI_WIN_WR_MASK CVMX_NPEI_WIN_WR_MASK_FUNC()
static inline uint64_t CVMX_NPEI_WIN_WR_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_NPEI_WIN_WR_MASK not supported on this chip\n");
#endif
    return 0x0000000000000230ull;
}

#define CVMX_NPI_BASE_ADDR_INPUT0 CVMX_NPI_BASE_ADDR_INPUTX(0)
#define CVMX_NPI_BASE_ADDR_INPUT1 CVMX_NPI_BASE_ADDR_INPUTX(1)
#define CVMX_NPI_BASE_ADDR_INPUT2 CVMX_NPI_BASE_ADDR_INPUTX(2)
#define CVMX_NPI_BASE_ADDR_INPUT3 CVMX_NPI_BASE_ADDR_INPUTX(3)
static inline uint64_t CVMX_NPI_BASE_ADDR_INPUTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_BASE_ADDR_INPUTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000070ull) + (offset&3)*16;
}

#define CVMX_NPI_BASE_ADDR_OUTPUT0 CVMX_NPI_BASE_ADDR_OUTPUTX(0)
#define CVMX_NPI_BASE_ADDR_OUTPUT1 CVMX_NPI_BASE_ADDR_OUTPUTX(1)
#define CVMX_NPI_BASE_ADDR_OUTPUT2 CVMX_NPI_BASE_ADDR_OUTPUTX(2)
#define CVMX_NPI_BASE_ADDR_OUTPUT3 CVMX_NPI_BASE_ADDR_OUTPUTX(3)
static inline uint64_t CVMX_NPI_BASE_ADDR_OUTPUTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_BASE_ADDR_OUTPUTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000000B8ull) + (offset&3)*8;
}

#define CVMX_NPI_BIST_STATUS CVMX_NPI_BIST_STATUS_FUNC()
static inline uint64_t CVMX_NPI_BIST_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_BIST_STATUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000003F8ull);
}

#define CVMX_NPI_BUFF_SIZE_OUTPUT0 CVMX_NPI_BUFF_SIZE_OUTPUTX(0)
#define CVMX_NPI_BUFF_SIZE_OUTPUT1 CVMX_NPI_BUFF_SIZE_OUTPUTX(1)
#define CVMX_NPI_BUFF_SIZE_OUTPUT2 CVMX_NPI_BUFF_SIZE_OUTPUTX(2)
#define CVMX_NPI_BUFF_SIZE_OUTPUT3 CVMX_NPI_BUFF_SIZE_OUTPUTX(3)
static inline uint64_t CVMX_NPI_BUFF_SIZE_OUTPUTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_BUFF_SIZE_OUTPUTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000000E0ull) + (offset&3)*8;
}

#define CVMX_NPI_COMP_CTL CVMX_NPI_COMP_CTL_FUNC()
static inline uint64_t CVMX_NPI_COMP_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_COMP_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000218ull);
}

#define CVMX_NPI_CTL_STATUS CVMX_NPI_CTL_STATUS_FUNC()
static inline uint64_t CVMX_NPI_CTL_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_CTL_STATUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000010ull);
}

#define CVMX_NPI_DBG_SELECT CVMX_NPI_DBG_SELECT_FUNC()
static inline uint64_t CVMX_NPI_DBG_SELECT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_DBG_SELECT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000008ull);
}

#define CVMX_NPI_DMA_CONTROL CVMX_NPI_DMA_CONTROL_FUNC()
static inline uint64_t CVMX_NPI_DMA_CONTROL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_DMA_CONTROL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000128ull);
}

#define CVMX_NPI_DMA_HIGHP_COUNTS CVMX_NPI_DMA_HIGHP_COUNTS_FUNC()
static inline uint64_t CVMX_NPI_DMA_HIGHP_COUNTS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_DMA_HIGHP_COUNTS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000148ull);
}

#define CVMX_NPI_DMA_HIGHP_NADDR CVMX_NPI_DMA_HIGHP_NADDR_FUNC()
static inline uint64_t CVMX_NPI_DMA_HIGHP_NADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_DMA_HIGHP_NADDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000158ull);
}

#define CVMX_NPI_DMA_LOWP_COUNTS CVMX_NPI_DMA_LOWP_COUNTS_FUNC()
static inline uint64_t CVMX_NPI_DMA_LOWP_COUNTS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_DMA_LOWP_COUNTS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000140ull);
}

#define CVMX_NPI_DMA_LOWP_NADDR CVMX_NPI_DMA_LOWP_NADDR_FUNC()
static inline uint64_t CVMX_NPI_DMA_LOWP_NADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_DMA_LOWP_NADDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000150ull);
}

#define CVMX_NPI_HIGHP_DBELL CVMX_NPI_HIGHP_DBELL_FUNC()
static inline uint64_t CVMX_NPI_HIGHP_DBELL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_HIGHP_DBELL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000120ull);
}

#define CVMX_NPI_HIGHP_IBUFF_SADDR CVMX_NPI_HIGHP_IBUFF_SADDR_FUNC()
static inline uint64_t CVMX_NPI_HIGHP_IBUFF_SADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_HIGHP_IBUFF_SADDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000110ull);
}

#define CVMX_NPI_INPUT_CONTROL CVMX_NPI_INPUT_CONTROL_FUNC()
static inline uint64_t CVMX_NPI_INPUT_CONTROL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_INPUT_CONTROL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000138ull);
}

#define CVMX_NPI_INT_ENB CVMX_NPI_INT_ENB_FUNC()
static inline uint64_t CVMX_NPI_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_INT_ENB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000020ull);
}

#define CVMX_NPI_INT_SUM CVMX_NPI_INT_SUM_FUNC()
static inline uint64_t CVMX_NPI_INT_SUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_INT_SUM not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000018ull);
}

#define CVMX_NPI_LOWP_DBELL CVMX_NPI_LOWP_DBELL_FUNC()
static inline uint64_t CVMX_NPI_LOWP_DBELL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_LOWP_DBELL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000118ull);
}

#define CVMX_NPI_LOWP_IBUFF_SADDR CVMX_NPI_LOWP_IBUFF_SADDR_FUNC()
static inline uint64_t CVMX_NPI_LOWP_IBUFF_SADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_LOWP_IBUFF_SADDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000108ull);
}

#define CVMX_NPI_MEM_ACCESS_SUBID3 CVMX_NPI_MEM_ACCESS_SUBIDX(3)
#define CVMX_NPI_MEM_ACCESS_SUBID4 CVMX_NPI_MEM_ACCESS_SUBIDX(4)
#define CVMX_NPI_MEM_ACCESS_SUBID5 CVMX_NPI_MEM_ACCESS_SUBIDX(5)
#define CVMX_NPI_MEM_ACCESS_SUBID6 CVMX_NPI_MEM_ACCESS_SUBIDX(6)
static inline uint64_t CVMX_NPI_MEM_ACCESS_SUBIDX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset >= 3) && (offset <= 6)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset >= 3) && (offset <= 6)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset >= 3) && (offset <= 6)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset >= 3) && (offset <= 6)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset >= 3) && (offset <= 6))))))
        cvmx_warn("CVMX_NPI_MEM_ACCESS_SUBIDX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000028ull) + (offset&7)*8 - 8*3;
}

#define CVMX_NPI_MSI_RCV CVMX_NPI_MSI_RCV_FUNC()
static inline uint64_t CVMX_NPI_MSI_RCV_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_MSI_RCV not supported on this chip\n");
#endif
    return 0x0000000000000190ull;
}

#define CVMX_NPI_NPI_MSI_RCV CVMX_NPI_NPI_MSI_RCV_FUNC()
static inline uint64_t CVMX_NPI_NPI_MSI_RCV_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_NPI_MSI_RCV not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001190ull);
}

#define CVMX_NPI_NUM_DESC_OUTPUT0 CVMX_NPI_NUM_DESC_OUTPUTX(0)
#define CVMX_NPI_NUM_DESC_OUTPUT1 CVMX_NPI_NUM_DESC_OUTPUTX(1)
#define CVMX_NPI_NUM_DESC_OUTPUT2 CVMX_NPI_NUM_DESC_OUTPUTX(2)
#define CVMX_NPI_NUM_DESC_OUTPUT3 CVMX_NPI_NUM_DESC_OUTPUTX(3)
static inline uint64_t CVMX_NPI_NUM_DESC_OUTPUTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_NUM_DESC_OUTPUTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000050ull) + (offset&3)*8;
}

#define CVMX_NPI_OUTPUT_CONTROL CVMX_NPI_OUTPUT_CONTROL_FUNC()
static inline uint64_t CVMX_NPI_OUTPUT_CONTROL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_OUTPUT_CONTROL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000100ull);
}

#define CVMX_NPI_P0_DBPAIR_ADDR CVMX_NPI_PX_DBPAIR_ADDR(0)
#define CVMX_NPI_P0_INSTR_ADDR CVMX_NPI_PX_INSTR_ADDR(0)
#define CVMX_NPI_P0_INSTR_CNTS CVMX_NPI_PX_INSTR_CNTS(0)
#define CVMX_NPI_P0_PAIR_CNTS CVMX_NPI_PX_PAIR_CNTS(0)
#define CVMX_NPI_P1_DBPAIR_ADDR CVMX_NPI_PX_DBPAIR_ADDR(1)
#define CVMX_NPI_P1_INSTR_ADDR CVMX_NPI_PX_INSTR_ADDR(1)
#define CVMX_NPI_P1_INSTR_CNTS CVMX_NPI_PX_INSTR_CNTS(1)
#define CVMX_NPI_P1_PAIR_CNTS CVMX_NPI_PX_PAIR_CNTS(1)
#define CVMX_NPI_P2_DBPAIR_ADDR CVMX_NPI_PX_DBPAIR_ADDR(2)
#define CVMX_NPI_P2_INSTR_ADDR CVMX_NPI_PX_INSTR_ADDR(2)
#define CVMX_NPI_P2_INSTR_CNTS CVMX_NPI_PX_INSTR_CNTS(2)
#define CVMX_NPI_P2_PAIR_CNTS CVMX_NPI_PX_PAIR_CNTS(2)
#define CVMX_NPI_P3_DBPAIR_ADDR CVMX_NPI_PX_DBPAIR_ADDR(3)
#define CVMX_NPI_P3_INSTR_ADDR CVMX_NPI_PX_INSTR_ADDR(3)
#define CVMX_NPI_P3_INSTR_CNTS CVMX_NPI_PX_INSTR_CNTS(3)
#define CVMX_NPI_P3_PAIR_CNTS CVMX_NPI_PX_PAIR_CNTS(3)
static inline uint64_t CVMX_NPI_PCI_BAR1_INDEXX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_NPI_PCI_BAR1_INDEXX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001100ull) + (offset&31)*4;
}

#define CVMX_NPI_PCI_BIST_REG CVMX_NPI_PCI_BIST_REG_FUNC()
static inline uint64_t CVMX_NPI_PCI_BIST_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN50XX)))
        cvmx_warn("CVMX_NPI_PCI_BIST_REG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000011C0ull);
}

#define CVMX_NPI_PCI_BURST_SIZE CVMX_NPI_PCI_BURST_SIZE_FUNC()
static inline uint64_t CVMX_NPI_PCI_BURST_SIZE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_BURST_SIZE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000000D8ull);
}

#define CVMX_NPI_PCI_CFG00 CVMX_NPI_PCI_CFG00_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG00_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG00 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001800ull);
}

#define CVMX_NPI_PCI_CFG01 CVMX_NPI_PCI_CFG01_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG01_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG01 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001804ull);
}

#define CVMX_NPI_PCI_CFG02 CVMX_NPI_PCI_CFG02_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG02_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG02 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001808ull);
}

#define CVMX_NPI_PCI_CFG03 CVMX_NPI_PCI_CFG03_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG03_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG03 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000180Cull);
}

#define CVMX_NPI_PCI_CFG04 CVMX_NPI_PCI_CFG04_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG04_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG04 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001810ull);
}

#define CVMX_NPI_PCI_CFG05 CVMX_NPI_PCI_CFG05_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG05_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG05 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001814ull);
}

#define CVMX_NPI_PCI_CFG06 CVMX_NPI_PCI_CFG06_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG06_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG06 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001818ull);
}

#define CVMX_NPI_PCI_CFG07 CVMX_NPI_PCI_CFG07_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG07_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG07 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000181Cull);
}

#define CVMX_NPI_PCI_CFG08 CVMX_NPI_PCI_CFG08_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG08_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG08 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001820ull);
}

#define CVMX_NPI_PCI_CFG09 CVMX_NPI_PCI_CFG09_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG09_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG09 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001824ull);
}

#define CVMX_NPI_PCI_CFG10 CVMX_NPI_PCI_CFG10_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG10_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG10 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001828ull);
}

#define CVMX_NPI_PCI_CFG11 CVMX_NPI_PCI_CFG11_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG11_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG11 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000182Cull);
}

#define CVMX_NPI_PCI_CFG12 CVMX_NPI_PCI_CFG12_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG12_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG12 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001830ull);
}

#define CVMX_NPI_PCI_CFG13 CVMX_NPI_PCI_CFG13_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG13_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG13 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001834ull);
}

#define CVMX_NPI_PCI_CFG15 CVMX_NPI_PCI_CFG15_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG15_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG15 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000183Cull);
}

#define CVMX_NPI_PCI_CFG16 CVMX_NPI_PCI_CFG16_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG16_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG16 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001840ull);
}

#define CVMX_NPI_PCI_CFG17 CVMX_NPI_PCI_CFG17_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG17_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG17 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001844ull);
}

#define CVMX_NPI_PCI_CFG18 CVMX_NPI_PCI_CFG18_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG18_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG18 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001848ull);
}

#define CVMX_NPI_PCI_CFG19 CVMX_NPI_PCI_CFG19_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG19_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG19 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000184Cull);
}

#define CVMX_NPI_PCI_CFG20 CVMX_NPI_PCI_CFG20_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG20_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG20 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001850ull);
}

#define CVMX_NPI_PCI_CFG21 CVMX_NPI_PCI_CFG21_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG21_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG21 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001854ull);
}

#define CVMX_NPI_PCI_CFG22 CVMX_NPI_PCI_CFG22_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG22_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG22 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001858ull);
}

#define CVMX_NPI_PCI_CFG56 CVMX_NPI_PCI_CFG56_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG56_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG56 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000018E0ull);
}

#define CVMX_NPI_PCI_CFG57 CVMX_NPI_PCI_CFG57_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG57_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG57 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000018E4ull);
}

#define CVMX_NPI_PCI_CFG58 CVMX_NPI_PCI_CFG58_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG58_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG58 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000018E8ull);
}

#define CVMX_NPI_PCI_CFG59 CVMX_NPI_PCI_CFG59_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG59_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG59 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000018ECull);
}

#define CVMX_NPI_PCI_CFG60 CVMX_NPI_PCI_CFG60_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG60_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG60 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000018F0ull);
}

#define CVMX_NPI_PCI_CFG61 CVMX_NPI_PCI_CFG61_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG61_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG61 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000018F4ull);
}

#define CVMX_NPI_PCI_CFG62 CVMX_NPI_PCI_CFG62_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG62_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG62 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000018F8ull);
}

#define CVMX_NPI_PCI_CFG63 CVMX_NPI_PCI_CFG63_FUNC()
static inline uint64_t CVMX_NPI_PCI_CFG63_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CFG63 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000018FCull);
}

#define CVMX_NPI_PCI_CNT_REG CVMX_NPI_PCI_CNT_REG_FUNC()
static inline uint64_t CVMX_NPI_PCI_CNT_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CNT_REG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000011B8ull);
}

#define CVMX_NPI_PCI_CTL_STATUS_2 CVMX_NPI_PCI_CTL_STATUS_2_FUNC()
static inline uint64_t CVMX_NPI_PCI_CTL_STATUS_2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_CTL_STATUS_2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000118Cull);
}

#define CVMX_NPI_PCI_INT_ARB_CFG CVMX_NPI_PCI_INT_ARB_CFG_FUNC()
static inline uint64_t CVMX_NPI_PCI_INT_ARB_CFG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_INT_ARB_CFG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000130ull);
}

#define CVMX_NPI_PCI_INT_ENB2 CVMX_NPI_PCI_INT_ENB2_FUNC()
static inline uint64_t CVMX_NPI_PCI_INT_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_INT_ENB2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000011A0ull);
}

#define CVMX_NPI_PCI_INT_SUM2 CVMX_NPI_PCI_INT_SUM2_FUNC()
static inline uint64_t CVMX_NPI_PCI_INT_SUM2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_INT_SUM2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001198ull);
}

#define CVMX_NPI_PCI_READ_CMD CVMX_NPI_PCI_READ_CMD_FUNC()
static inline uint64_t CVMX_NPI_PCI_READ_CMD_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_READ_CMD not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000048ull);
}

#define CVMX_NPI_PCI_READ_CMD_6 CVMX_NPI_PCI_READ_CMD_6_FUNC()
static inline uint64_t CVMX_NPI_PCI_READ_CMD_6_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_READ_CMD_6 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001180ull);
}

#define CVMX_NPI_PCI_READ_CMD_C CVMX_NPI_PCI_READ_CMD_C_FUNC()
static inline uint64_t CVMX_NPI_PCI_READ_CMD_C_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_READ_CMD_C not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001184ull);
}

#define CVMX_NPI_PCI_READ_CMD_E CVMX_NPI_PCI_READ_CMD_E_FUNC()
static inline uint64_t CVMX_NPI_PCI_READ_CMD_E_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_READ_CMD_E not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000001188ull);
}

#define CVMX_NPI_PCI_SCM_REG CVMX_NPI_PCI_SCM_REG_FUNC()
static inline uint64_t CVMX_NPI_PCI_SCM_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_SCM_REG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000011A8ull);
}

#define CVMX_NPI_PCI_TSR_REG CVMX_NPI_PCI_TSR_REG_FUNC()
static inline uint64_t CVMX_NPI_PCI_TSR_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PCI_TSR_REG not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000011B0ull);
}

#define CVMX_NPI_PORT32_INSTR_HDR CVMX_NPI_PORT32_INSTR_HDR_FUNC()
static inline uint64_t CVMX_NPI_PORT32_INSTR_HDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PORT32_INSTR_HDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000001F8ull);
}

#define CVMX_NPI_PORT33_INSTR_HDR CVMX_NPI_PORT33_INSTR_HDR_FUNC()
static inline uint64_t CVMX_NPI_PORT33_INSTR_HDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PORT33_INSTR_HDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000200ull);
}

#define CVMX_NPI_PORT34_INSTR_HDR CVMX_NPI_PORT34_INSTR_HDR_FUNC()
static inline uint64_t CVMX_NPI_PORT34_INSTR_HDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PORT34_INSTR_HDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000208ull);
}

#define CVMX_NPI_PORT35_INSTR_HDR CVMX_NPI_PORT35_INSTR_HDR_FUNC()
static inline uint64_t CVMX_NPI_PORT35_INSTR_HDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PORT35_INSTR_HDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000210ull);
}

#define CVMX_NPI_PORT_BP_CONTROL CVMX_NPI_PORT_BP_CONTROL_FUNC()
static inline uint64_t CVMX_NPI_PORT_BP_CONTROL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_PORT_BP_CONTROL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000001F0ull);
}

static inline uint64_t CVMX_NPI_PX_DBPAIR_ADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_PX_DBPAIR_ADDR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000180ull) + (offset&3)*8;
}

static inline uint64_t CVMX_NPI_PX_INSTR_ADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_PX_INSTR_ADDR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000001C0ull) + (offset&3)*8;
}

static inline uint64_t CVMX_NPI_PX_INSTR_CNTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_PX_INSTR_CNTS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000001A0ull) + (offset&3)*8;
}

static inline uint64_t CVMX_NPI_PX_PAIR_CNTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_PX_PAIR_CNTS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000160ull) + (offset&3)*8;
}

#define CVMX_NPI_RSL_INT_BLOCKS CVMX_NPI_RSL_INT_BLOCKS_FUNC()
static inline uint64_t CVMX_NPI_RSL_INT_BLOCKS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_RSL_INT_BLOCKS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000000ull);
}

#define CVMX_NPI_SIZE_INPUT0 CVMX_NPI_SIZE_INPUTX(0)
#define CVMX_NPI_SIZE_INPUT1 CVMX_NPI_SIZE_INPUTX(1)
#define CVMX_NPI_SIZE_INPUT2 CVMX_NPI_SIZE_INPUTX(2)
#define CVMX_NPI_SIZE_INPUT3 CVMX_NPI_SIZE_INPUTX(3)
static inline uint64_t CVMX_NPI_SIZE_INPUTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_NPI_SIZE_INPUTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000000078ull) + (offset&3)*16;
}

#define CVMX_NPI_WIN_READ_TO CVMX_NPI_WIN_READ_TO_FUNC()
static inline uint64_t CVMX_NPI_WIN_READ_TO_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_NPI_WIN_READ_TO not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000001E0ull);
}

#define CVMX_PCIEEP_CFG000 CVMX_PCIEEP_CFG000_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG000_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG000 not supported on this chip\n");
#endif
    return 0x0000000000000000ull;
}

#define CVMX_PCIEEP_CFG001 CVMX_PCIEEP_CFG001_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG001_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG001 not supported on this chip\n");
#endif
    return 0x0000000000000004ull;
}

#define CVMX_PCIEEP_CFG002 CVMX_PCIEEP_CFG002_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG002_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG002 not supported on this chip\n");
#endif
    return 0x0000000000000008ull;
}

#define CVMX_PCIEEP_CFG003 CVMX_PCIEEP_CFG003_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG003_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG003 not supported on this chip\n");
#endif
    return 0x000000000000000Cull;
}

#define CVMX_PCIEEP_CFG004 CVMX_PCIEEP_CFG004_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG004_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG004 not supported on this chip\n");
#endif
    return 0x0000000000000010ull;
}

#define CVMX_PCIEEP_CFG004_MASK CVMX_PCIEEP_CFG004_MASK_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG004_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG004_MASK not supported on this chip\n");
#endif
    return 0x0000000080000010ull;
}

#define CVMX_PCIEEP_CFG005 CVMX_PCIEEP_CFG005_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG005_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG005 not supported on this chip\n");
#endif
    return 0x0000000000000014ull;
}

#define CVMX_PCIEEP_CFG005_MASK CVMX_PCIEEP_CFG005_MASK_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG005_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG005_MASK not supported on this chip\n");
#endif
    return 0x0000000080000014ull;
}

#define CVMX_PCIEEP_CFG006 CVMX_PCIEEP_CFG006_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG006_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG006 not supported on this chip\n");
#endif
    return 0x0000000000000018ull;
}

#define CVMX_PCIEEP_CFG006_MASK CVMX_PCIEEP_CFG006_MASK_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG006_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG006_MASK not supported on this chip\n");
#endif
    return 0x0000000080000018ull;
}

#define CVMX_PCIEEP_CFG007 CVMX_PCIEEP_CFG007_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG007_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG007 not supported on this chip\n");
#endif
    return 0x000000000000001Cull;
}

#define CVMX_PCIEEP_CFG007_MASK CVMX_PCIEEP_CFG007_MASK_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG007_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG007_MASK not supported on this chip\n");
#endif
    return 0x000000008000001Cull;
}

#define CVMX_PCIEEP_CFG008 CVMX_PCIEEP_CFG008_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG008_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG008 not supported on this chip\n");
#endif
    return 0x0000000000000020ull;
}

#define CVMX_PCIEEP_CFG008_MASK CVMX_PCIEEP_CFG008_MASK_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG008_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG008_MASK not supported on this chip\n");
#endif
    return 0x0000000080000020ull;
}

#define CVMX_PCIEEP_CFG009 CVMX_PCIEEP_CFG009_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG009_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG009 not supported on this chip\n");
#endif
    return 0x0000000000000024ull;
}

#define CVMX_PCIEEP_CFG009_MASK CVMX_PCIEEP_CFG009_MASK_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG009_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG009_MASK not supported on this chip\n");
#endif
    return 0x0000000080000024ull;
}

#define CVMX_PCIEEP_CFG010 CVMX_PCIEEP_CFG010_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG010_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG010 not supported on this chip\n");
#endif
    return 0x0000000000000028ull;
}

#define CVMX_PCIEEP_CFG011 CVMX_PCIEEP_CFG011_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG011_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG011 not supported on this chip\n");
#endif
    return 0x000000000000002Cull;
}

#define CVMX_PCIEEP_CFG012 CVMX_PCIEEP_CFG012_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG012_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG012 not supported on this chip\n");
#endif
    return 0x0000000000000030ull;
}

#define CVMX_PCIEEP_CFG012_MASK CVMX_PCIEEP_CFG012_MASK_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG012_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG012_MASK not supported on this chip\n");
#endif
    return 0x0000000080000030ull;
}

#define CVMX_PCIEEP_CFG013 CVMX_PCIEEP_CFG013_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG013_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG013 not supported on this chip\n");
#endif
    return 0x0000000000000034ull;
}

#define CVMX_PCIEEP_CFG015 CVMX_PCIEEP_CFG015_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG015_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG015 not supported on this chip\n");
#endif
    return 0x000000000000003Cull;
}

#define CVMX_PCIEEP_CFG016 CVMX_PCIEEP_CFG016_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG016_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG016 not supported on this chip\n");
#endif
    return 0x0000000000000040ull;
}

#define CVMX_PCIEEP_CFG017 CVMX_PCIEEP_CFG017_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG017_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG017 not supported on this chip\n");
#endif
    return 0x0000000000000044ull;
}

#define CVMX_PCIEEP_CFG020 CVMX_PCIEEP_CFG020_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG020_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG020 not supported on this chip\n");
#endif
    return 0x0000000000000050ull;
}

#define CVMX_PCIEEP_CFG021 CVMX_PCIEEP_CFG021_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG021_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG021 not supported on this chip\n");
#endif
    return 0x0000000000000054ull;
}

#define CVMX_PCIEEP_CFG022 CVMX_PCIEEP_CFG022_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG022_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG022 not supported on this chip\n");
#endif
    return 0x0000000000000058ull;
}

#define CVMX_PCIEEP_CFG023 CVMX_PCIEEP_CFG023_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG023_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG023 not supported on this chip\n");
#endif
    return 0x000000000000005Cull;
}

#define CVMX_PCIEEP_CFG028 CVMX_PCIEEP_CFG028_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG028_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG028 not supported on this chip\n");
#endif
    return 0x0000000000000070ull;
}

#define CVMX_PCIEEP_CFG029 CVMX_PCIEEP_CFG029_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG029_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG029 not supported on this chip\n");
#endif
    return 0x0000000000000074ull;
}

#define CVMX_PCIEEP_CFG030 CVMX_PCIEEP_CFG030_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG030_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG030 not supported on this chip\n");
#endif
    return 0x0000000000000078ull;
}

#define CVMX_PCIEEP_CFG031 CVMX_PCIEEP_CFG031_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG031_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG031 not supported on this chip\n");
#endif
    return 0x000000000000007Cull;
}

#define CVMX_PCIEEP_CFG032 CVMX_PCIEEP_CFG032_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG032_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG032 not supported on this chip\n");
#endif
    return 0x0000000000000080ull;
}

#define CVMX_PCIEEP_CFG033 CVMX_PCIEEP_CFG033_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG033_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG033 not supported on this chip\n");
#endif
    return 0x0000000000000084ull;
}

#define CVMX_PCIEEP_CFG034 CVMX_PCIEEP_CFG034_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG034_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG034 not supported on this chip\n");
#endif
    return 0x0000000000000088ull;
}

#define CVMX_PCIEEP_CFG037 CVMX_PCIEEP_CFG037_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG037_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG037 not supported on this chip\n");
#endif
    return 0x0000000000000094ull;
}

#define CVMX_PCIEEP_CFG038 CVMX_PCIEEP_CFG038_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG038_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG038 not supported on this chip\n");
#endif
    return 0x0000000000000098ull;
}

#define CVMX_PCIEEP_CFG039 CVMX_PCIEEP_CFG039_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG039_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG039 not supported on this chip\n");
#endif
    return 0x000000000000009Cull;
}

#define CVMX_PCIEEP_CFG040 CVMX_PCIEEP_CFG040_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG040_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG040 not supported on this chip\n");
#endif
    return 0x00000000000000A0ull;
}

#define CVMX_PCIEEP_CFG041 CVMX_PCIEEP_CFG041_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG041_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG041 not supported on this chip\n");
#endif
    return 0x00000000000000A4ull;
}

#define CVMX_PCIEEP_CFG042 CVMX_PCIEEP_CFG042_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG042_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG042 not supported on this chip\n");
#endif
    return 0x00000000000000A8ull;
}

#define CVMX_PCIEEP_CFG064 CVMX_PCIEEP_CFG064_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG064_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG064 not supported on this chip\n");
#endif
    return 0x0000000000000100ull;
}

#define CVMX_PCIEEP_CFG065 CVMX_PCIEEP_CFG065_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG065_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG065 not supported on this chip\n");
#endif
    return 0x0000000000000104ull;
}

#define CVMX_PCIEEP_CFG066 CVMX_PCIEEP_CFG066_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG066_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG066 not supported on this chip\n");
#endif
    return 0x0000000000000108ull;
}

#define CVMX_PCIEEP_CFG067 CVMX_PCIEEP_CFG067_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG067_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG067 not supported on this chip\n");
#endif
    return 0x000000000000010Cull;
}

#define CVMX_PCIEEP_CFG068 CVMX_PCIEEP_CFG068_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG068_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG068 not supported on this chip\n");
#endif
    return 0x0000000000000110ull;
}

#define CVMX_PCIEEP_CFG069 CVMX_PCIEEP_CFG069_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG069_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG069 not supported on this chip\n");
#endif
    return 0x0000000000000114ull;
}

#define CVMX_PCIEEP_CFG070 CVMX_PCIEEP_CFG070_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG070_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG070 not supported on this chip\n");
#endif
    return 0x0000000000000118ull;
}

#define CVMX_PCIEEP_CFG071 CVMX_PCIEEP_CFG071_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG071_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG071 not supported on this chip\n");
#endif
    return 0x000000000000011Cull;
}

#define CVMX_PCIEEP_CFG072 CVMX_PCIEEP_CFG072_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG072_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG072 not supported on this chip\n");
#endif
    return 0x0000000000000120ull;
}

#define CVMX_PCIEEP_CFG073 CVMX_PCIEEP_CFG073_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG073_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG073 not supported on this chip\n");
#endif
    return 0x0000000000000124ull;
}

#define CVMX_PCIEEP_CFG074 CVMX_PCIEEP_CFG074_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG074_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG074 not supported on this chip\n");
#endif
    return 0x0000000000000128ull;
}

#define CVMX_PCIEEP_CFG448 CVMX_PCIEEP_CFG448_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG448_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG448 not supported on this chip\n");
#endif
    return 0x0000000000000700ull;
}

#define CVMX_PCIEEP_CFG449 CVMX_PCIEEP_CFG449_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG449_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG449 not supported on this chip\n");
#endif
    return 0x0000000000000704ull;
}

#define CVMX_PCIEEP_CFG450 CVMX_PCIEEP_CFG450_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG450_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG450 not supported on this chip\n");
#endif
    return 0x0000000000000708ull;
}

#define CVMX_PCIEEP_CFG451 CVMX_PCIEEP_CFG451_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG451_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG451 not supported on this chip\n");
#endif
    return 0x000000000000070Cull;
}

#define CVMX_PCIEEP_CFG452 CVMX_PCIEEP_CFG452_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG452_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG452 not supported on this chip\n");
#endif
    return 0x0000000000000710ull;
}

#define CVMX_PCIEEP_CFG453 CVMX_PCIEEP_CFG453_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG453_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG453 not supported on this chip\n");
#endif
    return 0x0000000000000714ull;
}

#define CVMX_PCIEEP_CFG454 CVMX_PCIEEP_CFG454_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG454_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG454 not supported on this chip\n");
#endif
    return 0x0000000000000718ull;
}

#define CVMX_PCIEEP_CFG455 CVMX_PCIEEP_CFG455_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG455_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG455 not supported on this chip\n");
#endif
    return 0x000000000000071Cull;
}

#define CVMX_PCIEEP_CFG456 CVMX_PCIEEP_CFG456_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG456_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG456 not supported on this chip\n");
#endif
    return 0x0000000000000720ull;
}

#define CVMX_PCIEEP_CFG458 CVMX_PCIEEP_CFG458_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG458_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG458 not supported on this chip\n");
#endif
    return 0x0000000000000728ull;
}

#define CVMX_PCIEEP_CFG459 CVMX_PCIEEP_CFG459_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG459_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG459 not supported on this chip\n");
#endif
    return 0x000000000000072Cull;
}

#define CVMX_PCIEEP_CFG460 CVMX_PCIEEP_CFG460_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG460_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG460 not supported on this chip\n");
#endif
    return 0x0000000000000730ull;
}

#define CVMX_PCIEEP_CFG461 CVMX_PCIEEP_CFG461_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG461_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG461 not supported on this chip\n");
#endif
    return 0x0000000000000734ull;
}

#define CVMX_PCIEEP_CFG462 CVMX_PCIEEP_CFG462_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG462_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG462 not supported on this chip\n");
#endif
    return 0x0000000000000738ull;
}

#define CVMX_PCIEEP_CFG463 CVMX_PCIEEP_CFG463_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG463_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG463 not supported on this chip\n");
#endif
    return 0x000000000000073Cull;
}

#define CVMX_PCIEEP_CFG464 CVMX_PCIEEP_CFG464_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG464_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG464 not supported on this chip\n");
#endif
    return 0x0000000000000740ull;
}

#define CVMX_PCIEEP_CFG465 CVMX_PCIEEP_CFG465_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG465_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG465 not supported on this chip\n");
#endif
    return 0x0000000000000744ull;
}

#define CVMX_PCIEEP_CFG466 CVMX_PCIEEP_CFG466_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG466_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG466 not supported on this chip\n");
#endif
    return 0x0000000000000748ull;
}

#define CVMX_PCIEEP_CFG467 CVMX_PCIEEP_CFG467_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG467_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG467 not supported on this chip\n");
#endif
    return 0x000000000000074Cull;
}

#define CVMX_PCIEEP_CFG468 CVMX_PCIEEP_CFG468_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG468_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG468 not supported on this chip\n");
#endif
    return 0x0000000000000750ull;
}

#define CVMX_PCIEEP_CFG490 CVMX_PCIEEP_CFG490_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG490_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG490 not supported on this chip\n");
#endif
    return 0x00000000000007A8ull;
}

#define CVMX_PCIEEP_CFG491 CVMX_PCIEEP_CFG491_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG491_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG491 not supported on this chip\n");
#endif
    return 0x00000000000007ACull;
}

#define CVMX_PCIEEP_CFG492 CVMX_PCIEEP_CFG492_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG492_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG492 not supported on this chip\n");
#endif
    return 0x00000000000007B0ull;
}

#define CVMX_PCIEEP_CFG516 CVMX_PCIEEP_CFG516_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG516_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG516 not supported on this chip\n");
#endif
    return 0x0000000000000810ull;
}

#define CVMX_PCIEEP_CFG517 CVMX_PCIEEP_CFG517_FUNC()
static inline uint64_t CVMX_PCIEEP_CFG517_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PCIEEP_CFG517 not supported on this chip\n");
#endif
    return 0x0000000000000814ull;
}

static inline uint64_t CVMX_PCIERCX_CFG000(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG000(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000000ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG001(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG001(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000004ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG002(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG002(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000008ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG003(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG003(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000000Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG004(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG004(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000010ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG005(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG005(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000014ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG006(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG006(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000018ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG007(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG007(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000001Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG008(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG008(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000020ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG009(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG009(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000024ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG010(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG010(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000028ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG011(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG011(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000002Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG012(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG012(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000030ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG013(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG013(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000034ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG014(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG014(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000038ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG015(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG015(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000003Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG016(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG016(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000040ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG017(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG017(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000044ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG020(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG020(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000050ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG021(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG021(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000054ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG022(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG022(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000058ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG023(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG023(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000005Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG028(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG028(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000070ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG029(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG029(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000074ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG030(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG030(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000078ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG031(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG031(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000007Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG032(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG032(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000080ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG033(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG033(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000084ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG034(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG034(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000088ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG035(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG035(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000008Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG036(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG036(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000090ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG037(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG037(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000094ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG038(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG038(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000098ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG039(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG039(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000009Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG040(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG040(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000000A0ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG041(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG041(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000000A4ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG042(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG042(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000000A8ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG064(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG064(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000100ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG065(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG065(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000104ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG066(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG066(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000108ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG067(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG067(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000010Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG068(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG068(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000110ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG069(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG069(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000114ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG070(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG070(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000118ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG071(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG071(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000011Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG072(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG072(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000120ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG073(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG073(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000124ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG074(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG074(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000128ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG075(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG075(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000012Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG076(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG076(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000130ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG077(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG077(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000134ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG448(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG448(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000700ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG449(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG449(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000704ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG450(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG450(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000708ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG451(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG451(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000070Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG452(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG452(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000710ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG453(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG453(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000714ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG454(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG454(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000718ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG455(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG455(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000071Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG456(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG456(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000720ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG458(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG458(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000728ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG459(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG459(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000072Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG460(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG460(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000730ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG461(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG461(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000734ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG462(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG462(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000738ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG463(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG463(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000073Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG464(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG464(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000740ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG465(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG465(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000744ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG466(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG466(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000748ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG467(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG467(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000074Cull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG468(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG468(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000750ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG490(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG490(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000007A8ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG491(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG491(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000007ACull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG492(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG492(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000007B0ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG516(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG516(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000810ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCIERCX_CFG517(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCIERCX_CFG517(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000814ull + (offset&1)*0;
}

static inline uint64_t CVMX_PCI_BAR1_INDEXX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PCI_BAR1_INDEXX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000100ull + (offset&31)*4;
}

#define CVMX_PCI_BIST_REG CVMX_PCI_BIST_REG_FUNC()
static inline uint64_t CVMX_PCI_BIST_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN50XX)))
        cvmx_warn("CVMX_PCI_BIST_REG not supported on this chip\n");
#endif
    return 0x00000000000001C0ull;
}

#define CVMX_PCI_CFG00 CVMX_PCI_CFG00_FUNC()
static inline uint64_t CVMX_PCI_CFG00_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG00 not supported on this chip\n");
#endif
    return 0x0000000000000000ull;
}

#define CVMX_PCI_CFG01 CVMX_PCI_CFG01_FUNC()
static inline uint64_t CVMX_PCI_CFG01_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG01 not supported on this chip\n");
#endif
    return 0x0000000000000004ull;
}

#define CVMX_PCI_CFG02 CVMX_PCI_CFG02_FUNC()
static inline uint64_t CVMX_PCI_CFG02_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG02 not supported on this chip\n");
#endif
    return 0x0000000000000008ull;
}

#define CVMX_PCI_CFG03 CVMX_PCI_CFG03_FUNC()
static inline uint64_t CVMX_PCI_CFG03_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG03 not supported on this chip\n");
#endif
    return 0x000000000000000Cull;
}

#define CVMX_PCI_CFG04 CVMX_PCI_CFG04_FUNC()
static inline uint64_t CVMX_PCI_CFG04_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG04 not supported on this chip\n");
#endif
    return 0x0000000000000010ull;
}

#define CVMX_PCI_CFG05 CVMX_PCI_CFG05_FUNC()
static inline uint64_t CVMX_PCI_CFG05_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG05 not supported on this chip\n");
#endif
    return 0x0000000000000014ull;
}

#define CVMX_PCI_CFG06 CVMX_PCI_CFG06_FUNC()
static inline uint64_t CVMX_PCI_CFG06_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG06 not supported on this chip\n");
#endif
    return 0x0000000000000018ull;
}

#define CVMX_PCI_CFG07 CVMX_PCI_CFG07_FUNC()
static inline uint64_t CVMX_PCI_CFG07_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG07 not supported on this chip\n");
#endif
    return 0x000000000000001Cull;
}

#define CVMX_PCI_CFG08 CVMX_PCI_CFG08_FUNC()
static inline uint64_t CVMX_PCI_CFG08_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG08 not supported on this chip\n");
#endif
    return 0x0000000000000020ull;
}

#define CVMX_PCI_CFG09 CVMX_PCI_CFG09_FUNC()
static inline uint64_t CVMX_PCI_CFG09_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG09 not supported on this chip\n");
#endif
    return 0x0000000000000024ull;
}

#define CVMX_PCI_CFG10 CVMX_PCI_CFG10_FUNC()
static inline uint64_t CVMX_PCI_CFG10_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG10 not supported on this chip\n");
#endif
    return 0x0000000000000028ull;
}

#define CVMX_PCI_CFG11 CVMX_PCI_CFG11_FUNC()
static inline uint64_t CVMX_PCI_CFG11_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG11 not supported on this chip\n");
#endif
    return 0x000000000000002Cull;
}

#define CVMX_PCI_CFG12 CVMX_PCI_CFG12_FUNC()
static inline uint64_t CVMX_PCI_CFG12_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG12 not supported on this chip\n");
#endif
    return 0x0000000000000030ull;
}

#define CVMX_PCI_CFG13 CVMX_PCI_CFG13_FUNC()
static inline uint64_t CVMX_PCI_CFG13_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG13 not supported on this chip\n");
#endif
    return 0x0000000000000034ull;
}

#define CVMX_PCI_CFG15 CVMX_PCI_CFG15_FUNC()
static inline uint64_t CVMX_PCI_CFG15_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG15 not supported on this chip\n");
#endif
    return 0x000000000000003Cull;
}

#define CVMX_PCI_CFG16 CVMX_PCI_CFG16_FUNC()
static inline uint64_t CVMX_PCI_CFG16_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG16 not supported on this chip\n");
#endif
    return 0x0000000000000040ull;
}

#define CVMX_PCI_CFG17 CVMX_PCI_CFG17_FUNC()
static inline uint64_t CVMX_PCI_CFG17_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG17 not supported on this chip\n");
#endif
    return 0x0000000000000044ull;
}

#define CVMX_PCI_CFG18 CVMX_PCI_CFG18_FUNC()
static inline uint64_t CVMX_PCI_CFG18_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG18 not supported on this chip\n");
#endif
    return 0x0000000000000048ull;
}

#define CVMX_PCI_CFG19 CVMX_PCI_CFG19_FUNC()
static inline uint64_t CVMX_PCI_CFG19_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG19 not supported on this chip\n");
#endif
    return 0x000000000000004Cull;
}

#define CVMX_PCI_CFG20 CVMX_PCI_CFG20_FUNC()
static inline uint64_t CVMX_PCI_CFG20_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG20 not supported on this chip\n");
#endif
    return 0x0000000000000050ull;
}

#define CVMX_PCI_CFG21 CVMX_PCI_CFG21_FUNC()
static inline uint64_t CVMX_PCI_CFG21_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG21 not supported on this chip\n");
#endif
    return 0x0000000000000054ull;
}

#define CVMX_PCI_CFG22 CVMX_PCI_CFG22_FUNC()
static inline uint64_t CVMX_PCI_CFG22_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG22 not supported on this chip\n");
#endif
    return 0x0000000000000058ull;
}

#define CVMX_PCI_CFG56 CVMX_PCI_CFG56_FUNC()
static inline uint64_t CVMX_PCI_CFG56_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG56 not supported on this chip\n");
#endif
    return 0x00000000000000E0ull;
}

#define CVMX_PCI_CFG57 CVMX_PCI_CFG57_FUNC()
static inline uint64_t CVMX_PCI_CFG57_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG57 not supported on this chip\n");
#endif
    return 0x00000000000000E4ull;
}

#define CVMX_PCI_CFG58 CVMX_PCI_CFG58_FUNC()
static inline uint64_t CVMX_PCI_CFG58_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG58 not supported on this chip\n");
#endif
    return 0x00000000000000E8ull;
}

#define CVMX_PCI_CFG59 CVMX_PCI_CFG59_FUNC()
static inline uint64_t CVMX_PCI_CFG59_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG59 not supported on this chip\n");
#endif
    return 0x00000000000000ECull;
}

#define CVMX_PCI_CFG60 CVMX_PCI_CFG60_FUNC()
static inline uint64_t CVMX_PCI_CFG60_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG60 not supported on this chip\n");
#endif
    return 0x00000000000000F0ull;
}

#define CVMX_PCI_CFG61 CVMX_PCI_CFG61_FUNC()
static inline uint64_t CVMX_PCI_CFG61_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG61 not supported on this chip\n");
#endif
    return 0x00000000000000F4ull;
}

#define CVMX_PCI_CFG62 CVMX_PCI_CFG62_FUNC()
static inline uint64_t CVMX_PCI_CFG62_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG62 not supported on this chip\n");
#endif
    return 0x00000000000000F8ull;
}

#define CVMX_PCI_CFG63 CVMX_PCI_CFG63_FUNC()
static inline uint64_t CVMX_PCI_CFG63_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CFG63 not supported on this chip\n");
#endif
    return 0x00000000000000FCull;
}

#define CVMX_PCI_CNT_REG CVMX_PCI_CNT_REG_FUNC()
static inline uint64_t CVMX_PCI_CNT_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CNT_REG not supported on this chip\n");
#endif
    return 0x00000000000001B8ull;
}

#define CVMX_PCI_CTL_STATUS_2 CVMX_PCI_CTL_STATUS_2_FUNC()
static inline uint64_t CVMX_PCI_CTL_STATUS_2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_CTL_STATUS_2 not supported on this chip\n");
#endif
    return 0x000000000000018Cull;
}

static inline uint64_t CVMX_PCI_DBELL_X(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCI_DBELL_X(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000080ull + (offset&3)*8;
}

#define CVMX_PCI_DMA_CNT0 CVMX_PCI_DMA_CNTX(0)
#define CVMX_PCI_DMA_CNT1 CVMX_PCI_DMA_CNTX(1)
static inline uint64_t CVMX_PCI_DMA_CNTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCI_DMA_CNTX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000000A0ull + (offset&1)*8;
}

#define CVMX_PCI_DMA_INT_LEV0 CVMX_PCI_DMA_INT_LEVX(0)
#define CVMX_PCI_DMA_INT_LEV1 CVMX_PCI_DMA_INT_LEVX(1)
static inline uint64_t CVMX_PCI_DMA_INT_LEVX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCI_DMA_INT_LEVX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000000A4ull + (offset&1)*8;
}

#define CVMX_PCI_DMA_TIME0 CVMX_PCI_DMA_TIMEX(0)
#define CVMX_PCI_DMA_TIME1 CVMX_PCI_DMA_TIMEX(1)
static inline uint64_t CVMX_PCI_DMA_TIMEX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCI_DMA_TIMEX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x00000000000000B0ull + (offset&1)*4;
}

#define CVMX_PCI_INSTR_COUNT0 CVMX_PCI_INSTR_COUNTX(0)
#define CVMX_PCI_INSTR_COUNT1 CVMX_PCI_INSTR_COUNTX(1)
#define CVMX_PCI_INSTR_COUNT2 CVMX_PCI_INSTR_COUNTX(2)
#define CVMX_PCI_INSTR_COUNT3 CVMX_PCI_INSTR_COUNTX(3)
static inline uint64_t CVMX_PCI_INSTR_COUNTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCI_INSTR_COUNTX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000084ull + (offset&3)*8;
}

#define CVMX_PCI_INT_ENB CVMX_PCI_INT_ENB_FUNC()
static inline uint64_t CVMX_PCI_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_INT_ENB not supported on this chip\n");
#endif
    return 0x0000000000000038ull;
}

#define CVMX_PCI_INT_ENB2 CVMX_PCI_INT_ENB2_FUNC()
static inline uint64_t CVMX_PCI_INT_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_INT_ENB2 not supported on this chip\n");
#endif
    return 0x00000000000001A0ull;
}

#define CVMX_PCI_INT_SUM CVMX_PCI_INT_SUM_FUNC()
static inline uint64_t CVMX_PCI_INT_SUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_INT_SUM not supported on this chip\n");
#endif
    return 0x0000000000000030ull;
}

#define CVMX_PCI_INT_SUM2 CVMX_PCI_INT_SUM2_FUNC()
static inline uint64_t CVMX_PCI_INT_SUM2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_INT_SUM2 not supported on this chip\n");
#endif
    return 0x0000000000000198ull;
}

#define CVMX_PCI_MSI_RCV CVMX_PCI_MSI_RCV_FUNC()
static inline uint64_t CVMX_PCI_MSI_RCV_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_MSI_RCV not supported on this chip\n");
#endif
    return 0x00000000000000F0ull;
}

#define CVMX_PCI_PKTS_SENT0 CVMX_PCI_PKTS_SENTX(0)
#define CVMX_PCI_PKTS_SENT1 CVMX_PCI_PKTS_SENTX(1)
#define CVMX_PCI_PKTS_SENT2 CVMX_PCI_PKTS_SENTX(2)
#define CVMX_PCI_PKTS_SENT3 CVMX_PCI_PKTS_SENTX(3)
static inline uint64_t CVMX_PCI_PKTS_SENTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCI_PKTS_SENTX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000040ull + (offset&3)*16;
}

#define CVMX_PCI_PKTS_SENT_INT_LEV0 CVMX_PCI_PKTS_SENT_INT_LEVX(0)
#define CVMX_PCI_PKTS_SENT_INT_LEV1 CVMX_PCI_PKTS_SENT_INT_LEVX(1)
#define CVMX_PCI_PKTS_SENT_INT_LEV2 CVMX_PCI_PKTS_SENT_INT_LEVX(2)
#define CVMX_PCI_PKTS_SENT_INT_LEV3 CVMX_PCI_PKTS_SENT_INT_LEVX(3)
static inline uint64_t CVMX_PCI_PKTS_SENT_INT_LEVX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCI_PKTS_SENT_INT_LEVX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000048ull + (offset&3)*16;
}

#define CVMX_PCI_PKTS_SENT_TIME0 CVMX_PCI_PKTS_SENT_TIMEX(0)
#define CVMX_PCI_PKTS_SENT_TIME1 CVMX_PCI_PKTS_SENT_TIMEX(1)
#define CVMX_PCI_PKTS_SENT_TIME2 CVMX_PCI_PKTS_SENT_TIMEX(2)
#define CVMX_PCI_PKTS_SENT_TIME3 CVMX_PCI_PKTS_SENT_TIMEX(3)
static inline uint64_t CVMX_PCI_PKTS_SENT_TIMEX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCI_PKTS_SENT_TIMEX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x000000000000004Cull + (offset&3)*16;
}

#define CVMX_PCI_PKT_CREDITS0 CVMX_PCI_PKT_CREDITSX(0)
#define CVMX_PCI_PKT_CREDITS1 CVMX_PCI_PKT_CREDITSX(1)
#define CVMX_PCI_PKT_CREDITS2 CVMX_PCI_PKT_CREDITSX(2)
#define CVMX_PCI_PKT_CREDITS3 CVMX_PCI_PKT_CREDITSX(3)
static inline uint64_t CVMX_PCI_PKT_CREDITSX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCI_PKT_CREDITSX(%lu) is invalid on this chip\n", offset);
#endif
    return 0x0000000000000044ull + (offset&3)*16;
}

#define CVMX_PCI_READ_CMD_6 CVMX_PCI_READ_CMD_6_FUNC()
static inline uint64_t CVMX_PCI_READ_CMD_6_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_READ_CMD_6 not supported on this chip\n");
#endif
    return 0x0000000000000180ull;
}

#define CVMX_PCI_READ_CMD_C CVMX_PCI_READ_CMD_C_FUNC()
static inline uint64_t CVMX_PCI_READ_CMD_C_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_READ_CMD_C not supported on this chip\n");
#endif
    return 0x0000000000000184ull;
}

#define CVMX_PCI_READ_CMD_E CVMX_PCI_READ_CMD_E_FUNC()
static inline uint64_t CVMX_PCI_READ_CMD_E_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_READ_CMD_E not supported on this chip\n");
#endif
    return 0x0000000000000188ull;
}

#define CVMX_PCI_READ_TIMEOUT CVMX_PCI_READ_TIMEOUT_FUNC()
static inline uint64_t CVMX_PCI_READ_TIMEOUT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_READ_TIMEOUT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000000B0ull);
}

#define CVMX_PCI_SCM_REG CVMX_PCI_SCM_REG_FUNC()
static inline uint64_t CVMX_PCI_SCM_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_SCM_REG not supported on this chip\n");
#endif
    return 0x00000000000001A8ull;
}

#define CVMX_PCI_TSR_REG CVMX_PCI_TSR_REG_FUNC()
static inline uint64_t CVMX_PCI_TSR_REG_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_TSR_REG not supported on this chip\n");
#endif
    return 0x00000000000001B0ull;
}

#define CVMX_PCI_WIN_RD_ADDR CVMX_PCI_WIN_RD_ADDR_FUNC()
static inline uint64_t CVMX_PCI_WIN_RD_ADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_WIN_RD_ADDR not supported on this chip\n");
#endif
    return 0x0000000000000008ull;
}

#define CVMX_PCI_WIN_RD_DATA CVMX_PCI_WIN_RD_DATA_FUNC()
static inline uint64_t CVMX_PCI_WIN_RD_DATA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_WIN_RD_DATA not supported on this chip\n");
#endif
    return 0x0000000000000020ull;
}

#define CVMX_PCI_WIN_WR_ADDR CVMX_PCI_WIN_WR_ADDR_FUNC()
static inline uint64_t CVMX_PCI_WIN_WR_ADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_WIN_WR_ADDR not supported on this chip\n");
#endif
    return 0x0000000000000000ull;
}

#define CVMX_PCI_WIN_WR_DATA CVMX_PCI_WIN_WR_DATA_FUNC()
static inline uint64_t CVMX_PCI_WIN_WR_DATA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_WIN_WR_DATA not supported on this chip\n");
#endif
    return 0x0000000000000010ull;
}

#define CVMX_PCI_WIN_WR_MASK CVMX_PCI_WIN_WR_MASK_FUNC()
static inline uint64_t CVMX_PCI_WIN_WR_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN50XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PCI_WIN_WR_MASK not supported on this chip\n");
#endif
    return 0x0000000000000018ull;
}

static inline uint64_t CVMX_PCMX_DMA_CFG(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_DMA_CFG(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010018ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_INT_ENA(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_INT_ENA(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010020ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_INT_SUM(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_INT_SUM(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010028ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXADDR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010068ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXCNT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXCNT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010060ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXMSK0(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXMSK0(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100C0ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXMSK1(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXMSK1(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100C8ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXMSK2(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXMSK2(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100D0ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXMSK3(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXMSK3(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100D8ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXMSK4(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXMSK4(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100E0ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXMSK5(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXMSK5(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100E8ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXMSK6(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXMSK6(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100F0ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXMSK7(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXMSK7(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100F8ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_RXSTART(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_RXSTART(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010058ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TDM_CFG(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TDM_CFG(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010010ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TDM_DBG(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TDM_DBG(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010030ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXADDR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010050ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXCNT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXCNT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010048ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXMSK0(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXMSK0(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010080ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXMSK1(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXMSK1(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010088ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXMSK2(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXMSK2(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010090ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXMSK3(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXMSK3(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010098ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXMSK4(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXMSK4(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100A0ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXMSK5(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXMSK5(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100A8ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXMSK6(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXMSK6(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100B0ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXMSK7(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXMSK7(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00010700000100B8ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCMX_TXSTART(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PCMX_TXSTART(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010040ull) + (offset&3)*16384;
}

static inline uint64_t CVMX_PCM_CLKX_CFG(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCM_CLKX_CFG(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010000ull) + (offset&1)*16384;
}

static inline uint64_t CVMX_PCM_CLKX_DBG(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCM_CLKX_DBG(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010038ull) + (offset&1)*16384;
}

static inline uint64_t CVMX_PCM_CLKX_GEN(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PCM_CLKX_GEN(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001070000010008ull) + (offset&1)*16384;
}

static inline uint64_t CVMX_PCSXX_10GBX_STATUS_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_10GBX_STATUS_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000828ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_BIST_STATUS_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_BIST_STATUS_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000870ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_BIT_LOCK_STATUS_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_BIT_LOCK_STATUS_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000850ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_CONTROL1_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_CONTROL1_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000800ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_CONTROL2_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_CONTROL2_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000818ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_INT_EN_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_INT_EN_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000860ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_INT_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_INT_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000858ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_LOG_ANL_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_LOG_ANL_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000868ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_MISC_CTL_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_MISC_CTL_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000848ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_RX_SYNC_STATES_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_RX_SYNC_STATES_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000838ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_SPD_ABIL_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_SPD_ABIL_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000810ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_STATUS1_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_STATUS1_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000808ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_STATUS2_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_STATUS2_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000820ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_TX_RX_POLARITY_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_TX_RX_POLARITY_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000840ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSXX_TX_RX_STATES_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PCSXX_TX_RX_STATES_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0000830ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PCSX_ANX_ADV_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_ANX_ADV_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001010ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_ANX_EXT_ST_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_ANX_EXT_ST_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001028ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_ANX_LP_ABIL_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_ANX_LP_ABIL_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001018ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_ANX_RESULTS_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_ANX_RESULTS_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001020ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_INTX_EN_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_INTX_EN_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001088ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_INTX_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_INTX_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001080ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_LINKX_TIMER_COUNT_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_LINKX_TIMER_COUNT_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001040ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_LOG_ANLX_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_LOG_ANLX_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001090ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_MISCX_CTL_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_MISCX_CTL_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001078ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_MRX_CONTROL_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_MRX_CONTROL_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001000ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_MRX_STATUS_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_MRX_STATUS_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001008ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_RXX_STATES_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_RXX_STATES_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001058ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_RXX_SYNC_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_RXX_SYNC_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001050ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_SGMX_AN_ADV_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_SGMX_AN_ADV_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001068ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_SGMX_LP_ADV_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_SGMX_LP_ADV_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001070ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_TXX_STATES_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_TXX_STATES_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001060ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PCSX_TX_RXX_POLARITY_REG(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id == 0))))))
        cvmx_warn("CVMX_PCSX_TX_RXX_POLARITY_REG(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800B0001048ull) + ((offset&3) + (block_id&1)*0x20000ull)*1024;
}

static inline uint64_t CVMX_PESCX_BIST_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_BIST_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000018ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_BIST_STATUS2(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_BIST_STATUS2(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000418ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_CFG_RD(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_CFG_RD(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000030ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_CFG_WR(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_CFG_WR(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000028ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_CPL_LUT_VALID(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_CPL_LUT_VALID(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000098ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_CTL_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_CTL_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000000ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_CTL_STATUS2(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_CTL_STATUS2(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000400ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_DBG_INFO(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_DBG_INFO(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000008ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_DBG_INFO_EN(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_DBG_INFO_EN(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C80000A0ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_DIAG_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_DIAG_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000020ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_P2N_BAR0_START(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_P2N_BAR0_START(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000080ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_P2N_BAR1_START(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_P2N_BAR1_START(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000088ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_P2N_BAR2_START(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_P2N_BAR2_START(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000090ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PESCX_P2P_BARX_END(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_PESCX_P2P_BARX_END(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000048ull) + ((offset&3) + (block_id&1)*0x800000ull)*16;
}

static inline uint64_t CVMX_PESCX_P2P_BARX_START(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 3)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 3)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_PESCX_P2P_BARX_START(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000040ull) + ((offset&3) + (block_id&1)*0x800000ull)*16;
}

static inline uint64_t CVMX_PESCX_TLP_CREDITS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_PESCX_TLP_CREDITS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800C8000038ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_PEXP_NPEI_BAR1_INDEXX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_BAR1_INDEXX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008000ull) + (offset&31)*16;
}

#define CVMX_PEXP_NPEI_BIST_STATUS CVMX_PEXP_NPEI_BIST_STATUS_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_BIST_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_BIST_STATUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008580ull);
}

#define CVMX_PEXP_NPEI_BIST_STATUS2 CVMX_PEXP_NPEI_BIST_STATUS2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_BIST_STATUS2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_BIST_STATUS2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008680ull);
}

#define CVMX_PEXP_NPEI_CTL_PORT0 CVMX_PEXP_NPEI_CTL_PORT0_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_CTL_PORT0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_CTL_PORT0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008250ull);
}

#define CVMX_PEXP_NPEI_CTL_PORT1 CVMX_PEXP_NPEI_CTL_PORT1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_CTL_PORT1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_CTL_PORT1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008260ull);
}

#define CVMX_PEXP_NPEI_CTL_STATUS CVMX_PEXP_NPEI_CTL_STATUS_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_CTL_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_CTL_STATUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008570ull);
}

#define CVMX_PEXP_NPEI_CTL_STATUS2 CVMX_PEXP_NPEI_CTL_STATUS2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_CTL_STATUS2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_CTL_STATUS2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC00ull);
}

#define CVMX_PEXP_NPEI_DATA_OUT_CNT CVMX_PEXP_NPEI_DATA_OUT_CNT_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DATA_OUT_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DATA_OUT_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000085F0ull);
}

#define CVMX_PEXP_NPEI_DBG_DATA CVMX_PEXP_NPEI_DBG_DATA_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DBG_DATA_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DBG_DATA not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008510ull);
}

#define CVMX_PEXP_NPEI_DBG_SELECT CVMX_PEXP_NPEI_DBG_SELECT_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DBG_SELECT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DBG_SELECT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008500ull);
}

#define CVMX_PEXP_NPEI_DMA0_INT_LEVEL CVMX_PEXP_NPEI_DMA0_INT_LEVEL_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA0_INT_LEVEL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DMA0_INT_LEVEL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000085C0ull);
}

#define CVMX_PEXP_NPEI_DMA1_INT_LEVEL CVMX_PEXP_NPEI_DMA1_INT_LEVEL_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA1_INT_LEVEL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DMA1_INT_LEVEL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000085D0ull);
}

static inline uint64_t CVMX_PEXP_NPEI_DMAX_COUNTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 4)))))
        cvmx_warn("CVMX_PEXP_NPEI_DMAX_COUNTS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008450ull) + (offset&7)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_DMAX_DBELL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 4)))))
        cvmx_warn("CVMX_PEXP_NPEI_DMAX_DBELL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000083B0ull) + (offset&7)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_DMAX_IBUFF_SADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 4)))))
        cvmx_warn("CVMX_PEXP_NPEI_DMAX_IBUFF_SADDR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008400ull) + (offset&7)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_DMAX_NADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 4))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 4)))))
        cvmx_warn("CVMX_PEXP_NPEI_DMAX_NADDR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000084A0ull) + (offset&7)*16;
}

#define CVMX_PEXP_NPEI_DMA_CNTS CVMX_PEXP_NPEI_DMA_CNTS_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_CNTS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DMA_CNTS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000085E0ull);
}

#define CVMX_PEXP_NPEI_DMA_CONTROL CVMX_PEXP_NPEI_DMA_CONTROL_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_CONTROL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DMA_CONTROL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000083A0ull);
}

#define CVMX_PEXP_NPEI_DMA_PCIE_REQ_NUM CVMX_PEXP_NPEI_DMA_PCIE_REQ_NUM_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_PCIE_REQ_NUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DMA_PCIE_REQ_NUM not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000085B0ull);
}

#define CVMX_PEXP_NPEI_DMA_STATE1 CVMX_PEXP_NPEI_DMA_STATE1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_STATE1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DMA_STATE1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000086C0ull);
}

#define CVMX_PEXP_NPEI_DMA_STATE1_P1 CVMX_PEXP_NPEI_DMA_STATE1_P1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_STATE1_P1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011F0000008680ull);
}

#define CVMX_PEXP_NPEI_DMA_STATE2 CVMX_PEXP_NPEI_DMA_STATE2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_STATE2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX)))
        cvmx_warn("CVMX_PEXP_NPEI_DMA_STATE2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000086D0ull);
}

#define CVMX_PEXP_NPEI_DMA_STATE2_P1 CVMX_PEXP_NPEI_DMA_STATE2_P1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_STATE2_P1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011F0000008690ull);
}

#define CVMX_PEXP_NPEI_DMA_STATE3_P1 CVMX_PEXP_NPEI_DMA_STATE3_P1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_STATE3_P1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011F00000086A0ull);
}

#define CVMX_PEXP_NPEI_DMA_STATE4_P1 CVMX_PEXP_NPEI_DMA_STATE4_P1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_STATE4_P1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011F00000086B0ull);
}

#define CVMX_PEXP_NPEI_DMA_STATE5_P1 CVMX_PEXP_NPEI_DMA_STATE5_P1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_DMA_STATE5_P1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011F00000086C0ull);
}

#define CVMX_PEXP_NPEI_INT_A_ENB CVMX_PEXP_NPEI_INT_A_ENB_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_INT_A_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_INT_A_ENB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008560ull);
}

#define CVMX_PEXP_NPEI_INT_A_ENB2 CVMX_PEXP_NPEI_INT_A_ENB2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_INT_A_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_INT_A_ENB2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BCE0ull);
}

#define CVMX_PEXP_NPEI_INT_A_SUM CVMX_PEXP_NPEI_INT_A_SUM_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_INT_A_SUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_INT_A_SUM not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008550ull);
}

#define CVMX_PEXP_NPEI_INT_ENB CVMX_PEXP_NPEI_INT_ENB_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_INT_ENB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008540ull);
}

#define CVMX_PEXP_NPEI_INT_ENB2 CVMX_PEXP_NPEI_INT_ENB2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_INT_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_INT_ENB2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BCD0ull);
}

#define CVMX_PEXP_NPEI_INT_INFO CVMX_PEXP_NPEI_INT_INFO_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_INT_INFO_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_INT_INFO not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008590ull);
}

#define CVMX_PEXP_NPEI_INT_SUM CVMX_PEXP_NPEI_INT_SUM_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_INT_SUM_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_INT_SUM not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008530ull);
}

#define CVMX_PEXP_NPEI_INT_SUM2 CVMX_PEXP_NPEI_INT_SUM2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_INT_SUM2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_INT_SUM2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BCC0ull);
}

#define CVMX_PEXP_NPEI_LAST_WIN_RDATA0 CVMX_PEXP_NPEI_LAST_WIN_RDATA0_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_LAST_WIN_RDATA0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_LAST_WIN_RDATA0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008600ull);
}

#define CVMX_PEXP_NPEI_LAST_WIN_RDATA1 CVMX_PEXP_NPEI_LAST_WIN_RDATA1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_LAST_WIN_RDATA1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_LAST_WIN_RDATA1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008610ull);
}

#define CVMX_PEXP_NPEI_MEM_ACCESS_CTL CVMX_PEXP_NPEI_MEM_ACCESS_CTL_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MEM_ACCESS_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MEM_ACCESS_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000084F0ull);
}

static inline uint64_t CVMX_PEXP_NPEI_MEM_ACCESS_SUBIDX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset >= 12) && (offset <= 27)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset >= 12) && (offset <= 27))))))
        cvmx_warn("CVMX_PEXP_NPEI_MEM_ACCESS_SUBIDX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008280ull) + (offset&31)*16 - 16*12;
}

#define CVMX_PEXP_NPEI_MSI_ENB0 CVMX_PEXP_NPEI_MSI_ENB0_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_ENB0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_ENB0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC50ull);
}

#define CVMX_PEXP_NPEI_MSI_ENB1 CVMX_PEXP_NPEI_MSI_ENB1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_ENB1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_ENB1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC60ull);
}

#define CVMX_PEXP_NPEI_MSI_ENB2 CVMX_PEXP_NPEI_MSI_ENB2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_ENB2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC70ull);
}

#define CVMX_PEXP_NPEI_MSI_ENB3 CVMX_PEXP_NPEI_MSI_ENB3_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_ENB3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_ENB3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC80ull);
}

#define CVMX_PEXP_NPEI_MSI_RCV0 CVMX_PEXP_NPEI_MSI_RCV0_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_RCV0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_RCV0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC10ull);
}

#define CVMX_PEXP_NPEI_MSI_RCV1 CVMX_PEXP_NPEI_MSI_RCV1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_RCV1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_RCV1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC20ull);
}

#define CVMX_PEXP_NPEI_MSI_RCV2 CVMX_PEXP_NPEI_MSI_RCV2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_RCV2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_RCV2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC30ull);
}

#define CVMX_PEXP_NPEI_MSI_RCV3 CVMX_PEXP_NPEI_MSI_RCV3_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_RCV3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_RCV3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC40ull);
}

#define CVMX_PEXP_NPEI_MSI_RD_MAP CVMX_PEXP_NPEI_MSI_RD_MAP_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_RD_MAP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_RD_MAP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BCA0ull);
}

#define CVMX_PEXP_NPEI_MSI_W1C_ENB0 CVMX_PEXP_NPEI_MSI_W1C_ENB0_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_W1C_ENB0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_W1C_ENB0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BCF0ull);
}

#define CVMX_PEXP_NPEI_MSI_W1C_ENB1 CVMX_PEXP_NPEI_MSI_W1C_ENB1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_W1C_ENB1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_W1C_ENB1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BD00ull);
}

#define CVMX_PEXP_NPEI_MSI_W1C_ENB2 CVMX_PEXP_NPEI_MSI_W1C_ENB2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_W1C_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_W1C_ENB2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BD10ull);
}

#define CVMX_PEXP_NPEI_MSI_W1C_ENB3 CVMX_PEXP_NPEI_MSI_W1C_ENB3_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_W1C_ENB3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_W1C_ENB3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BD20ull);
}

#define CVMX_PEXP_NPEI_MSI_W1S_ENB0 CVMX_PEXP_NPEI_MSI_W1S_ENB0_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_W1S_ENB0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_W1S_ENB0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BD30ull);
}

#define CVMX_PEXP_NPEI_MSI_W1S_ENB1 CVMX_PEXP_NPEI_MSI_W1S_ENB1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_W1S_ENB1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_W1S_ENB1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BD40ull);
}

#define CVMX_PEXP_NPEI_MSI_W1S_ENB2 CVMX_PEXP_NPEI_MSI_W1S_ENB2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_W1S_ENB2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_W1S_ENB2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BD50ull);
}

#define CVMX_PEXP_NPEI_MSI_W1S_ENB3 CVMX_PEXP_NPEI_MSI_W1S_ENB3_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_W1S_ENB3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_W1S_ENB3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BD60ull);
}

#define CVMX_PEXP_NPEI_MSI_WR_MAP CVMX_PEXP_NPEI_MSI_WR_MAP_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_MSI_WR_MAP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_MSI_WR_MAP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BC90ull);
}

#define CVMX_PEXP_NPEI_PCIE_CREDIT_CNT CVMX_PEXP_NPEI_PCIE_CREDIT_CNT_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PCIE_CREDIT_CNT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PCIE_CREDIT_CNT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BD70ull);
}

#define CVMX_PEXP_NPEI_PCIE_MSI_RCV CVMX_PEXP_NPEI_PCIE_MSI_RCV_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PCIE_MSI_RCV_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PCIE_MSI_RCV not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000BCB0ull);
}

#define CVMX_PEXP_NPEI_PCIE_MSI_RCV_B1 CVMX_PEXP_NPEI_PCIE_MSI_RCV_B1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PCIE_MSI_RCV_B1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PCIE_MSI_RCV_B1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008650ull);
}

#define CVMX_PEXP_NPEI_PCIE_MSI_RCV_B2 CVMX_PEXP_NPEI_PCIE_MSI_RCV_B2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PCIE_MSI_RCV_B2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PCIE_MSI_RCV_B2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008660ull);
}

#define CVMX_PEXP_NPEI_PCIE_MSI_RCV_B3 CVMX_PEXP_NPEI_PCIE_MSI_RCV_B3_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PCIE_MSI_RCV_B3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PCIE_MSI_RCV_B3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008670ull);
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_CNTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_CNTS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000A400ull) + (offset&31)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_INSTR_BADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_INSTR_BADDR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000A800ull) + (offset&31)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_INSTR_BAOFF_DBELL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_INSTR_BAOFF_DBELL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000AC00ull) + (offset&31)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_INSTR_FIFO_RSIZE(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_INSTR_FIFO_RSIZE(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000B000ull) + (offset&31)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_INSTR_HEADER(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_INSTR_HEADER(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000B400ull) + (offset&31)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_IN_BP(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_IN_BP(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000B800ull) + (offset&31)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_SLIST_BADDR(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_SLIST_BADDR(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009400ull) + (offset&31)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_SLIST_BAOFF_DBELL(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_SLIST_BAOFF_DBELL(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009800ull) + (offset&31)*16;
}

static inline uint64_t CVMX_PEXP_NPEI_PKTX_SLIST_FIFO_RSIZE(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKTX_SLIST_FIFO_RSIZE(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009C00ull) + (offset&31)*16;
}

#define CVMX_PEXP_NPEI_PKT_CNT_INT CVMX_PEXP_NPEI_PKT_CNT_INT_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_CNT_INT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_CNT_INT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009110ull);
}

#define CVMX_PEXP_NPEI_PKT_CNT_INT_ENB CVMX_PEXP_NPEI_PKT_CNT_INT_ENB_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_CNT_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_CNT_INT_ENB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009130ull);
}

#define CVMX_PEXP_NPEI_PKT_DATA_OUT_ES CVMX_PEXP_NPEI_PKT_DATA_OUT_ES_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_DATA_OUT_ES_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_DATA_OUT_ES not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000090B0ull);
}

#define CVMX_PEXP_NPEI_PKT_DATA_OUT_NS CVMX_PEXP_NPEI_PKT_DATA_OUT_NS_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_DATA_OUT_NS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_DATA_OUT_NS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000090A0ull);
}

#define CVMX_PEXP_NPEI_PKT_DATA_OUT_ROR CVMX_PEXP_NPEI_PKT_DATA_OUT_ROR_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_DATA_OUT_ROR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_DATA_OUT_ROR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009090ull);
}

#define CVMX_PEXP_NPEI_PKT_DPADDR CVMX_PEXP_NPEI_PKT_DPADDR_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_DPADDR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_DPADDR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009080ull);
}

#define CVMX_PEXP_NPEI_PKT_INPUT_CONTROL CVMX_PEXP_NPEI_PKT_INPUT_CONTROL_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_INPUT_CONTROL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_INPUT_CONTROL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009150ull);
}

#define CVMX_PEXP_NPEI_PKT_INSTR_ENB CVMX_PEXP_NPEI_PKT_INSTR_ENB_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_INSTR_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_INSTR_ENB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009000ull);
}

#define CVMX_PEXP_NPEI_PKT_INSTR_RD_SIZE CVMX_PEXP_NPEI_PKT_INSTR_RD_SIZE_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_INSTR_RD_SIZE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_INSTR_RD_SIZE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009190ull);
}

#define CVMX_PEXP_NPEI_PKT_INSTR_SIZE CVMX_PEXP_NPEI_PKT_INSTR_SIZE_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_INSTR_SIZE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_INSTR_SIZE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009020ull);
}

#define CVMX_PEXP_NPEI_PKT_INT_LEVELS CVMX_PEXP_NPEI_PKT_INT_LEVELS_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_INT_LEVELS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_INT_LEVELS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009100ull);
}

#define CVMX_PEXP_NPEI_PKT_IN_BP CVMX_PEXP_NPEI_PKT_IN_BP_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_IN_BP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_IN_BP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000086B0ull);
}

static inline uint64_t CVMX_PEXP_NPEI_PKT_IN_DONEX_CNTS(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 31))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 31)))))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_IN_DONEX_CNTS(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011F000000A000ull) + (offset&31)*16;
}

#define CVMX_PEXP_NPEI_PKT_IN_INSTR_COUNTS CVMX_PEXP_NPEI_PKT_IN_INSTR_COUNTS_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_IN_INSTR_COUNTS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_IN_INSTR_COUNTS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000086A0ull);
}

#define CVMX_PEXP_NPEI_PKT_IN_PCIE_PORT CVMX_PEXP_NPEI_PKT_IN_PCIE_PORT_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_IN_PCIE_PORT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_IN_PCIE_PORT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000091A0ull);
}

#define CVMX_PEXP_NPEI_PKT_IPTR CVMX_PEXP_NPEI_PKT_IPTR_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_IPTR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_IPTR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009070ull);
}

#define CVMX_PEXP_NPEI_PKT_OUTPUT_WMARK CVMX_PEXP_NPEI_PKT_OUTPUT_WMARK_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_OUTPUT_WMARK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_OUTPUT_WMARK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009160ull);
}

#define CVMX_PEXP_NPEI_PKT_OUT_BMODE CVMX_PEXP_NPEI_PKT_OUT_BMODE_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_OUT_BMODE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_OUT_BMODE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000090D0ull);
}

#define CVMX_PEXP_NPEI_PKT_OUT_ENB CVMX_PEXP_NPEI_PKT_OUT_ENB_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_OUT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_OUT_ENB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009010ull);
}

#define CVMX_PEXP_NPEI_PKT_PCIE_PORT CVMX_PEXP_NPEI_PKT_PCIE_PORT_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_PCIE_PORT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_PCIE_PORT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F00000090E0ull);
}

#define CVMX_PEXP_NPEI_PKT_PORT_IN_RST CVMX_PEXP_NPEI_PKT_PORT_IN_RST_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_PORT_IN_RST_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_PORT_IN_RST not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008690ull);
}

#define CVMX_PEXP_NPEI_PKT_SLIST_ES CVMX_PEXP_NPEI_PKT_SLIST_ES_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_SLIST_ES_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_SLIST_ES not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009050ull);
}

#define CVMX_PEXP_NPEI_PKT_SLIST_ID_SIZE CVMX_PEXP_NPEI_PKT_SLIST_ID_SIZE_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_SLIST_ID_SIZE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_SLIST_ID_SIZE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009180ull);
}

#define CVMX_PEXP_NPEI_PKT_SLIST_NS CVMX_PEXP_NPEI_PKT_SLIST_NS_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_SLIST_NS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_SLIST_NS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009040ull);
}

#define CVMX_PEXP_NPEI_PKT_SLIST_ROR CVMX_PEXP_NPEI_PKT_SLIST_ROR_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_SLIST_ROR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_SLIST_ROR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009030ull);
}

#define CVMX_PEXP_NPEI_PKT_TIME_INT CVMX_PEXP_NPEI_PKT_TIME_INT_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_TIME_INT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_TIME_INT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009120ull);
}

#define CVMX_PEXP_NPEI_PKT_TIME_INT_ENB CVMX_PEXP_NPEI_PKT_TIME_INT_ENB_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_PKT_TIME_INT_ENB_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_PKT_TIME_INT_ENB not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000009140ull);
}

#define CVMX_PEXP_NPEI_RSL_INT_BLOCKS CVMX_PEXP_NPEI_RSL_INT_BLOCKS_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_RSL_INT_BLOCKS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_RSL_INT_BLOCKS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008520ull);
}

#define CVMX_PEXP_NPEI_SCRATCH_1 CVMX_PEXP_NPEI_SCRATCH_1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_SCRATCH_1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_SCRATCH_1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008270ull);
}

#define CVMX_PEXP_NPEI_STATE1 CVMX_PEXP_NPEI_STATE1_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_STATE1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_STATE1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008620ull);
}

#define CVMX_PEXP_NPEI_STATE2 CVMX_PEXP_NPEI_STATE2_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_STATE2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_STATE2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008630ull);
}

#define CVMX_PEXP_NPEI_STATE3 CVMX_PEXP_NPEI_STATE3_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_STATE3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_STATE3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008640ull);
}

#define CVMX_PEXP_NPEI_WINDOW_CTL CVMX_PEXP_NPEI_WINDOW_CTL_FUNC()
static inline uint64_t CVMX_PEXP_NPEI_WINDOW_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PEXP_NPEI_WINDOW_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011F0000008380ull);
}

#define CVMX_PIP_BCK_PRS CVMX_PIP_BCK_PRS_FUNC()
static inline uint64_t CVMX_PIP_BCK_PRS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PIP_BCK_PRS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000038ull);
}

#define CVMX_PIP_BIST_STATUS CVMX_PIP_BIST_STATUS_FUNC()
static inline uint64_t CVMX_PIP_BIST_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000000ull);
}

static inline uint64_t CVMX_PIP_CRC_CTLX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PIP_CRC_CTLX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000040ull) + (offset&1)*8;
}

static inline uint64_t CVMX_PIP_CRC_IVX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PIP_CRC_IVX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000050ull) + (offset&1)*8;
}

static inline uint64_t CVMX_PIP_DEC_IPSECX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_PIP_DEC_IPSECX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000080ull) + (offset&3)*8;
}

#define CVMX_PIP_DSA_SRC_GRP CVMX_PIP_DSA_SRC_GRP_FUNC()
static inline uint64_t CVMX_PIP_DSA_SRC_GRP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PIP_DSA_SRC_GRP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000190ull);
}

#define CVMX_PIP_DSA_VID_GRP CVMX_PIP_DSA_VID_GRP_FUNC()
static inline uint64_t CVMX_PIP_DSA_VID_GRP_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PIP_DSA_VID_GRP not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000198ull);
}

static inline uint64_t CVMX_PIP_FRM_LEN_CHKX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PIP_FRM_LEN_CHKX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000180ull) + (offset&1)*8;
}

#define CVMX_PIP_GBL_CFG CVMX_PIP_GBL_CFG_FUNC()
static inline uint64_t CVMX_PIP_GBL_CFG_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000028ull);
}

#define CVMX_PIP_GBL_CTL CVMX_PIP_GBL_CTL_FUNC()
static inline uint64_t CVMX_PIP_GBL_CTL_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000020ull);
}

#define CVMX_PIP_HG_PRI_QOS CVMX_PIP_HG_PRI_QOS_FUNC()
static inline uint64_t CVMX_PIP_HG_PRI_QOS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PIP_HG_PRI_QOS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A00001A0ull);
}

#define CVMX_PIP_INT_EN CVMX_PIP_INT_EN_FUNC()
static inline uint64_t CVMX_PIP_INT_EN_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000010ull);
}

#define CVMX_PIP_INT_REG CVMX_PIP_INT_REG_FUNC()
static inline uint64_t CVMX_PIP_INT_REG_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000008ull);
}

#define CVMX_PIP_IP_OFFSET CVMX_PIP_IP_OFFSET_FUNC()
static inline uint64_t CVMX_PIP_IP_OFFSET_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000060ull);
}

static inline uint64_t CVMX_PIP_PRT_CFGX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_PRT_CFGX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000200ull) + (offset&63)*8;
}

static inline uint64_t CVMX_PIP_PRT_TAGX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_PRT_TAGX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000400ull) + (offset&63)*8;
}

static inline uint64_t CVMX_PIP_QOS_DIFFX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 63)))))
        cvmx_warn("CVMX_PIP_QOS_DIFFX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000600ull) + (offset&63)*8;
}

static inline uint64_t CVMX_PIP_QOS_VLANX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_PIP_QOS_VLANX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A00000C0ull) + (offset&7)*8;
}

static inline uint64_t CVMX_PIP_QOS_WATCHX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 3))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_PIP_QOS_WATCHX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000100ull) + (offset&7)*8;
}

#define CVMX_PIP_RAW_WORD CVMX_PIP_RAW_WORD_FUNC()
static inline uint64_t CVMX_PIP_RAW_WORD_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A00000B0ull);
}

#define CVMX_PIP_SFT_RST CVMX_PIP_SFT_RST_FUNC()
static inline uint64_t CVMX_PIP_SFT_RST_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000030ull);
}

static inline uint64_t CVMX_PIP_STAT0_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT0_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000800ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT1_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT1_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000808ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT2_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT2_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000810ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT3_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT3_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000818ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT4_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT4_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000820ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT5_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT5_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000828ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT6_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT6_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000830ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT7_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT7_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000838ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT8_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT8_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000840ull) + (offset&63)*80;
}

static inline uint64_t CVMX_PIP_STAT9_PRTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT9_PRTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0000848ull) + (offset&63)*80;
}

#define CVMX_PIP_STAT_CTL CVMX_PIP_STAT_CTL_FUNC()
static inline uint64_t CVMX_PIP_STAT_CTL_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000018ull);
}

static inline uint64_t CVMX_PIP_STAT_INB_ERRSX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT_INB_ERRSX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0001A10ull) + (offset&63)*32;
}

static inline uint64_t CVMX_PIP_STAT_INB_OCTSX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT_INB_OCTSX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0001A08ull) + (offset&63)*32;
}

static inline uint64_t CVMX_PIP_STAT_INB_PKTSX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 3) || ((offset >= 16) && (offset <= 19)) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 2) || ((offset >= 32) && (offset <= 33)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 35))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3) || ((offset >= 32) && (offset <= 35)) || ((offset >= 36) && (offset <= 39))))))
        cvmx_warn("CVMX_PIP_STAT_INB_PKTSX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0001A00ull) + (offset&63)*32;
}

static inline uint64_t CVMX_PIP_TAG_INCX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 63))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 63)))))
        cvmx_warn("CVMX_PIP_TAG_INCX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00011800A0001800ull) + (offset&63)*8;
}

#define CVMX_PIP_TAG_MASK CVMX_PIP_TAG_MASK_FUNC()
static inline uint64_t CVMX_PIP_TAG_MASK_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000070ull);
}

#define CVMX_PIP_TAG_SECRET CVMX_PIP_TAG_SECRET_FUNC()
static inline uint64_t CVMX_PIP_TAG_SECRET_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000068ull);
}

#define CVMX_PIP_TODO_ENTRY CVMX_PIP_TODO_ENTRY_FUNC()
static inline uint64_t CVMX_PIP_TODO_ENTRY_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00011800A0000078ull);
}

#define CVMX_PKO_MEM_COUNT0 CVMX_PKO_MEM_COUNT0_FUNC()
static inline uint64_t CVMX_PKO_MEM_COUNT0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001080ull);
}

#define CVMX_PKO_MEM_COUNT1 CVMX_PKO_MEM_COUNT1_FUNC()
static inline uint64_t CVMX_PKO_MEM_COUNT1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001088ull);
}

#define CVMX_PKO_MEM_DEBUG0 CVMX_PKO_MEM_DEBUG0_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001100ull);
}

#define CVMX_PKO_MEM_DEBUG1 CVMX_PKO_MEM_DEBUG1_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001108ull);
}

#define CVMX_PKO_MEM_DEBUG10 CVMX_PKO_MEM_DEBUG10_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG10_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001150ull);
}

#define CVMX_PKO_MEM_DEBUG11 CVMX_PKO_MEM_DEBUG11_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG11_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001158ull);
}

#define CVMX_PKO_MEM_DEBUG12 CVMX_PKO_MEM_DEBUG12_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG12_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001160ull);
}

#define CVMX_PKO_MEM_DEBUG13 CVMX_PKO_MEM_DEBUG13_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG13_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001168ull);
}

#define CVMX_PKO_MEM_DEBUG14 CVMX_PKO_MEM_DEBUG14_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG14_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN3XXX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PKO_MEM_DEBUG14 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050001170ull);
}

#define CVMX_PKO_MEM_DEBUG2 CVMX_PKO_MEM_DEBUG2_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG2_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001110ull);
}

#define CVMX_PKO_MEM_DEBUG3 CVMX_PKO_MEM_DEBUG3_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG3_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001118ull);
}

#define CVMX_PKO_MEM_DEBUG4 CVMX_PKO_MEM_DEBUG4_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG4_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001120ull);
}

#define CVMX_PKO_MEM_DEBUG5 CVMX_PKO_MEM_DEBUG5_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG5_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001128ull);
}

#define CVMX_PKO_MEM_DEBUG6 CVMX_PKO_MEM_DEBUG6_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG6_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001130ull);
}

#define CVMX_PKO_MEM_DEBUG7 CVMX_PKO_MEM_DEBUG7_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG7_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001138ull);
}

#define CVMX_PKO_MEM_DEBUG8 CVMX_PKO_MEM_DEBUG8_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG8_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001140ull);
}

#define CVMX_PKO_MEM_DEBUG9 CVMX_PKO_MEM_DEBUG9_FUNC()
static inline uint64_t CVMX_PKO_MEM_DEBUG9_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001148ull);
}

#define CVMX_PKO_MEM_PORT_PTRS CVMX_PKO_MEM_PORT_PTRS_FUNC()
static inline uint64_t CVMX_PKO_MEM_PORT_PTRS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PKO_MEM_PORT_PTRS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050001010ull);
}

#define CVMX_PKO_MEM_PORT_QOS CVMX_PKO_MEM_PORT_QOS_FUNC()
static inline uint64_t CVMX_PKO_MEM_PORT_QOS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PKO_MEM_PORT_QOS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050001018ull);
}

#define CVMX_PKO_MEM_PORT_RATE0 CVMX_PKO_MEM_PORT_RATE0_FUNC()
static inline uint64_t CVMX_PKO_MEM_PORT_RATE0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PKO_MEM_PORT_RATE0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050001020ull);
}

#define CVMX_PKO_MEM_PORT_RATE1 CVMX_PKO_MEM_PORT_RATE1_FUNC()
static inline uint64_t CVMX_PKO_MEM_PORT_RATE1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PKO_MEM_PORT_RATE1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050001028ull);
}

#define CVMX_PKO_MEM_QUEUE_PTRS CVMX_PKO_MEM_QUEUE_PTRS_FUNC()
static inline uint64_t CVMX_PKO_MEM_QUEUE_PTRS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001000ull);
}

#define CVMX_PKO_MEM_QUEUE_QOS CVMX_PKO_MEM_QUEUE_QOS_FUNC()
static inline uint64_t CVMX_PKO_MEM_QUEUE_QOS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050001008ull);
}

#define CVMX_PKO_REG_BIST_RESULT CVMX_PKO_REG_BIST_RESULT_FUNC()
static inline uint64_t CVMX_PKO_REG_BIST_RESULT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000080ull);
}

#define CVMX_PKO_REG_CMD_BUF CVMX_PKO_REG_CMD_BUF_FUNC()
static inline uint64_t CVMX_PKO_REG_CMD_BUF_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000010ull);
}

static inline uint64_t CVMX_PKO_REG_CRC_CTLX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PKO_REG_CRC_CTLX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180050000028ull) + (offset&1)*8;
}

#define CVMX_PKO_REG_CRC_ENABLE CVMX_PKO_REG_CRC_ENABLE_FUNC()
static inline uint64_t CVMX_PKO_REG_CRC_ENABLE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_PKO_REG_CRC_ENABLE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050000020ull);
}

static inline uint64_t CVMX_PKO_REG_CRC_IVX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_PKO_REG_CRC_IVX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180050000038ull) + (offset&1)*8;
}

#define CVMX_PKO_REG_DEBUG0 CVMX_PKO_REG_DEBUG0_FUNC()
static inline uint64_t CVMX_PKO_REG_DEBUG0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000098ull);
}

#define CVMX_PKO_REG_DEBUG1 CVMX_PKO_REG_DEBUG1_FUNC()
static inline uint64_t CVMX_PKO_REG_DEBUG1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_PKO_REG_DEBUG1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800500000A0ull);
}

#define CVMX_PKO_REG_DEBUG2 CVMX_PKO_REG_DEBUG2_FUNC()
static inline uint64_t CVMX_PKO_REG_DEBUG2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_PKO_REG_DEBUG2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800500000A8ull);
}

#define CVMX_PKO_REG_DEBUG3 CVMX_PKO_REG_DEBUG3_FUNC()
static inline uint64_t CVMX_PKO_REG_DEBUG3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_PKO_REG_DEBUG3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800500000B0ull);
}

#define CVMX_PKO_REG_ENGINE_INFLIGHT CVMX_PKO_REG_ENGINE_INFLIGHT_FUNC()
static inline uint64_t CVMX_PKO_REG_ENGINE_INFLIGHT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PKO_REG_ENGINE_INFLIGHT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050000050ull);
}

#define CVMX_PKO_REG_ENGINE_THRESH CVMX_PKO_REG_ENGINE_THRESH_FUNC()
static inline uint64_t CVMX_PKO_REG_ENGINE_THRESH_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_PKO_REG_ENGINE_THRESH not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050000058ull);
}

#define CVMX_PKO_REG_ERROR CVMX_PKO_REG_ERROR_FUNC()
static inline uint64_t CVMX_PKO_REG_ERROR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000088ull);
}

#define CVMX_PKO_REG_FLAGS CVMX_PKO_REG_FLAGS_FUNC()
static inline uint64_t CVMX_PKO_REG_FLAGS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000000ull);
}

#define CVMX_PKO_REG_GMX_PORT_MODE CVMX_PKO_REG_GMX_PORT_MODE_FUNC()
static inline uint64_t CVMX_PKO_REG_GMX_PORT_MODE_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000018ull);
}

#define CVMX_PKO_REG_INT_MASK CVMX_PKO_REG_INT_MASK_FUNC()
static inline uint64_t CVMX_PKO_REG_INT_MASK_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000090ull);
}

#define CVMX_PKO_REG_QUEUE_MODE CVMX_PKO_REG_QUEUE_MODE_FUNC()
static inline uint64_t CVMX_PKO_REG_QUEUE_MODE_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000048ull);
}

#define CVMX_PKO_REG_QUEUE_PTRS1 CVMX_PKO_REG_QUEUE_PTRS1_FUNC()
static inline uint64_t CVMX_PKO_REG_QUEUE_PTRS1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_PKO_REG_QUEUE_PTRS1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180050000100ull);
}

#define CVMX_PKO_REG_READ_IDX CVMX_PKO_REG_READ_IDX_FUNC()
static inline uint64_t CVMX_PKO_REG_READ_IDX_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180050000008ull);
}

#define CVMX_POW_BIST_STAT CVMX_POW_BIST_STAT_FUNC()
static inline uint64_t CVMX_POW_BIST_STAT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x00016700000003F8ull);
}

#define CVMX_POW_DS_PC CVMX_POW_DS_PC_FUNC()
static inline uint64_t CVMX_POW_DS_PC_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000398ull);
}

#define CVMX_POW_ECC_ERR CVMX_POW_ECC_ERR_FUNC()
static inline uint64_t CVMX_POW_ECC_ERR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000218ull);
}

#define CVMX_POW_INT_CTL CVMX_POW_INT_CTL_FUNC()
static inline uint64_t CVMX_POW_INT_CTL_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000220ull);
}

static inline uint64_t CVMX_POW_IQ_CNTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_POW_IQ_CNTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000340ull) + (offset&7)*8;
}

#define CVMX_POW_IQ_COM_CNT CVMX_POW_IQ_COM_CNT_FUNC()
static inline uint64_t CVMX_POW_IQ_COM_CNT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000388ull);
}

#define CVMX_POW_IQ_INT CVMX_POW_IQ_INT_FUNC()
static inline uint64_t CVMX_POW_IQ_INT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_POW_IQ_INT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000238ull);
}

#define CVMX_POW_IQ_INT_EN CVMX_POW_IQ_INT_EN_FUNC()
static inline uint64_t CVMX_POW_IQ_INT_EN_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_POW_IQ_INT_EN not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000240ull);
}

static inline uint64_t CVMX_POW_IQ_THRX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_POW_IQ_THRX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00016700000003A0ull) + (offset&7)*8;
}

#define CVMX_POW_NOS_CNT CVMX_POW_NOS_CNT_FUNC()
static inline uint64_t CVMX_POW_NOS_CNT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000228ull);
}

#define CVMX_POW_NW_TIM CVMX_POW_NW_TIM_FUNC()
static inline uint64_t CVMX_POW_NW_TIM_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000210ull);
}

#define CVMX_POW_PF_RST_MSK CVMX_POW_PF_RST_MSK_FUNC()
static inline uint64_t CVMX_POW_PF_RST_MSK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(!OCTEON_IS_MODEL(OCTEON_CN3XXX)))
        cvmx_warn("CVMX_POW_PF_RST_MSK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000230ull);
}

static inline uint64_t CVMX_POW_PP_GRP_MSKX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 11))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 3)))))
        cvmx_warn("CVMX_POW_PP_GRP_MSKX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000000ull) + (offset&15)*8;
}

static inline uint64_t CVMX_POW_QOS_RNDX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_POW_QOS_RNDX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x00016700000001C0ull) + (offset&7)*8;
}

static inline uint64_t CVMX_POW_QOS_THRX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_POW_QOS_THRX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000180ull) + (offset&7)*8;
}

#define CVMX_POW_TS_PC CVMX_POW_TS_PC_FUNC()
static inline uint64_t CVMX_POW_TS_PC_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000390ull);
}

#define CVMX_POW_WA_COM_PC CVMX_POW_WA_COM_PC_FUNC()
static inline uint64_t CVMX_POW_WA_COM_PC_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000380ull);
}

static inline uint64_t CVMX_POW_WA_PCX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 7))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 7)))))
        cvmx_warn("CVMX_POW_WA_PCX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000300ull) + (offset&7)*8;
}

#define CVMX_POW_WQ_INT CVMX_POW_WQ_INT_FUNC()
static inline uint64_t CVMX_POW_WQ_INT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000200ull);
}

static inline uint64_t CVMX_POW_WQ_INT_CNTX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 15)))))
        cvmx_warn("CVMX_POW_WQ_INT_CNTX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000100ull) + (offset&15)*8;
}

#define CVMX_POW_WQ_INT_PC CVMX_POW_WQ_INT_PC_FUNC()
static inline uint64_t CVMX_POW_WQ_INT_PC_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001670000000208ull);
}

static inline uint64_t CVMX_POW_WQ_INT_THRX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 15)))))
        cvmx_warn("CVMX_POW_WQ_INT_THRX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000080ull) + (offset&15)*8;
}

static inline uint64_t CVMX_POW_WS_PCX(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset <= 15))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 15)))))
        cvmx_warn("CVMX_POW_WS_PCX(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001670000000280ull) + (offset&15)*8;
}

#define CVMX_RAD_MEM_DEBUG0 CVMX_RAD_MEM_DEBUG0_FUNC()
static inline uint64_t CVMX_RAD_MEM_DEBUG0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_MEM_DEBUG0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070001000ull);
}

#define CVMX_RAD_MEM_DEBUG1 CVMX_RAD_MEM_DEBUG1_FUNC()
static inline uint64_t CVMX_RAD_MEM_DEBUG1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_MEM_DEBUG1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070001008ull);
}

#define CVMX_RAD_MEM_DEBUG2 CVMX_RAD_MEM_DEBUG2_FUNC()
static inline uint64_t CVMX_RAD_MEM_DEBUG2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_MEM_DEBUG2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070001010ull);
}

#define CVMX_RAD_REG_BIST_RESULT CVMX_RAD_REG_BIST_RESULT_FUNC()
static inline uint64_t CVMX_RAD_REG_BIST_RESULT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_BIST_RESULT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000080ull);
}

#define CVMX_RAD_REG_CMD_BUF CVMX_RAD_REG_CMD_BUF_FUNC()
static inline uint64_t CVMX_RAD_REG_CMD_BUF_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_CMD_BUF not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000008ull);
}

#define CVMX_RAD_REG_CTL CVMX_RAD_REG_CTL_FUNC()
static inline uint64_t CVMX_RAD_REG_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000000ull);
}

#define CVMX_RAD_REG_DEBUG0 CVMX_RAD_REG_DEBUG0_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000100ull);
}

#define CVMX_RAD_REG_DEBUG1 CVMX_RAD_REG_DEBUG1_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000108ull);
}

#define CVMX_RAD_REG_DEBUG10 CVMX_RAD_REG_DEBUG10_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG10_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG10 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000150ull);
}

#define CVMX_RAD_REG_DEBUG11 CVMX_RAD_REG_DEBUG11_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG11_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG11 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000158ull);
}

#define CVMX_RAD_REG_DEBUG12 CVMX_RAD_REG_DEBUG12_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG12_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG12 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000160ull);
}

#define CVMX_RAD_REG_DEBUG2 CVMX_RAD_REG_DEBUG2_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG2_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG2 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000110ull);
}

#define CVMX_RAD_REG_DEBUG3 CVMX_RAD_REG_DEBUG3_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG3_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG3 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000118ull);
}

#define CVMX_RAD_REG_DEBUG4 CVMX_RAD_REG_DEBUG4_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG4_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG4 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000120ull);
}

#define CVMX_RAD_REG_DEBUG5 CVMX_RAD_REG_DEBUG5_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG5_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG5 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000128ull);
}

#define CVMX_RAD_REG_DEBUG6 CVMX_RAD_REG_DEBUG6_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG6_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG6 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000130ull);
}

#define CVMX_RAD_REG_DEBUG7 CVMX_RAD_REG_DEBUG7_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG7_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG7 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000138ull);
}

#define CVMX_RAD_REG_DEBUG8 CVMX_RAD_REG_DEBUG8_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG8_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG8 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000140ull);
}

#define CVMX_RAD_REG_DEBUG9 CVMX_RAD_REG_DEBUG9_FUNC()
static inline uint64_t CVMX_RAD_REG_DEBUG9_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_DEBUG9 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000148ull);
}

#define CVMX_RAD_REG_ERROR CVMX_RAD_REG_ERROR_FUNC()
static inline uint64_t CVMX_RAD_REG_ERROR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_ERROR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000088ull);
}

#define CVMX_RAD_REG_INT_MASK CVMX_RAD_REG_INT_MASK_FUNC()
static inline uint64_t CVMX_RAD_REG_INT_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_INT_MASK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000090ull);
}

#define CVMX_RAD_REG_POLYNOMIAL CVMX_RAD_REG_POLYNOMIAL_FUNC()
static inline uint64_t CVMX_RAD_REG_POLYNOMIAL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_POLYNOMIAL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000010ull);
}

#define CVMX_RAD_REG_READ_IDX CVMX_RAD_REG_READ_IDX_FUNC()
static inline uint64_t CVMX_RAD_REG_READ_IDX_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX)))
        cvmx_warn("CVMX_RAD_REG_READ_IDX not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180070000018ull);
}

#define CVMX_RNM_BIST_STATUS CVMX_RNM_BIST_STATUS_FUNC()
static inline uint64_t CVMX_RNM_BIST_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180040000008ull);
}

#define CVMX_RNM_CTL_STATUS CVMX_RNM_CTL_STATUS_FUNC()
static inline uint64_t CVMX_RNM_CTL_STATUS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180040000000ull);
}

static inline uint64_t CVMX_SMIX_CLK(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_SMIX_CLK(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001818ull) + (offset&1)*256;
}

static inline uint64_t CVMX_SMIX_CMD(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_SMIX_CMD(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001800ull) + (offset&1)*256;
}

static inline uint64_t CVMX_SMIX_EN(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_SMIX_EN(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001820ull) + (offset&1)*256;
}

static inline uint64_t CVMX_SMIX_RD_DAT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_SMIX_RD_DAT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001810ull) + (offset&1)*256;
}

static inline uint64_t CVMX_SMIX_WR_DAT(unsigned long offset)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((offset <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((offset == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((offset <= 1)))))
        cvmx_warn("CVMX_SMIX_WR_DAT(%lu) is invalid on this chip\n", offset);
#endif
    return CVMX_ADD_IO_SEG(0x0001180000001808ull) + (offset&1)*256;
}

#define CVMX_SPX0_PLL_BW_CTL CVMX_SPX0_PLL_BW_CTL_FUNC()
static inline uint64_t CVMX_SPX0_PLL_BW_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX)))
        cvmx_warn("CVMX_SPX0_PLL_BW_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000388ull);
}

#define CVMX_SPX0_PLL_SETTING CVMX_SPX0_PLL_SETTING_FUNC()
static inline uint64_t CVMX_SPX0_PLL_SETTING_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN38XX)))
        cvmx_warn("CVMX_SPX0_PLL_SETTING not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000380ull);
}

static inline uint64_t CVMX_SPXX_BCKPRS_CNT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_BCKPRS_CNT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000340ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_BIST_STAT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_BIST_STAT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800900007F8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_CLK_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_CLK_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000348ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_CLK_STAT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_CLK_STAT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000350ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_DBG_DESKEW_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_DBG_DESKEW_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000368ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_DBG_DESKEW_STATE(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_DBG_DESKEW_STATE(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000370ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_DRV_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_DRV_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000358ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_ERR_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_ERR_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000320ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_INT_DAT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_INT_DAT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000318ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_INT_MSK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_INT_MSK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000308ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_INT_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_INT_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000300ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_INT_SYNC(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_INT_SYNC(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000310ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_TPA_ACC(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_TPA_ACC(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000338ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_TPA_MAX(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_TPA_MAX(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000330ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_TPA_SEL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_TPA_SEL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000328ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SPXX_TRN4_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SPXX_TRN4_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000360ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SRXX_COM_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SRXX_COM_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000200ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SRXX_IGN_RX_FULL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SRXX_IGN_RX_FULL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000218ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SRXX_SPI4_CALX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 31)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 31)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_SRXX_SPI4_CALX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000000ull) + ((offset&31) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_SRXX_SPI4_STAT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SRXX_SPI4_STAT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000208ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SRXX_SW_TICK_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SRXX_SW_TICK_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000220ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_SRXX_SW_TICK_DAT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_SRXX_SW_TICK_DAT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000228ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_ARB_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_ARB_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000608ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_BCKPRS_CNT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_BCKPRS_CNT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000688ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_COM_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_COM_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000600ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_DIP_CNT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_DIP_CNT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000690ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_IGN_CAL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_IGN_CAL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000610ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_INT_MSK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_INT_MSK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800900006A0ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_INT_REG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_INT_REG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000698ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_INT_SYNC(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_INT_SYNC(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800900006A8ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_MIN_BST(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_MIN_BST(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000618ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_SPI4_CALX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && (((offset <= 31)) && ((block_id <= 1)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && (((offset <= 31)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_STXX_SPI4_CALX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000400ull) + ((offset&31) + (block_id&1)*0x1000000ull)*8;
}

static inline uint64_t CVMX_STXX_SPI4_DAT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_SPI4_DAT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000628ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_SPI4_STAT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_SPI4_STAT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000630ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_STAT_BYTES_HI(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_STAT_BYTES_HI(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000648ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_STAT_BYTES_LO(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_STAT_BYTES_LO(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000680ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_STAT_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_STAT_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000638ull) + (block_id&1)*0x8000000ull;
}

static inline uint64_t CVMX_STXX_STAT_PKT_XMT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN38XX) && ((block_id <= 1))) ||
        (OCTEON_IS_MODEL(OCTEON_CN58XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_STXX_STAT_PKT_XMT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180090000640ull) + (block_id&1)*0x8000000ull;
}

#define CVMX_TIM_MEM_DEBUG0 CVMX_TIM_MEM_DEBUG0_FUNC()
static inline uint64_t CVMX_TIM_MEM_DEBUG0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058001100ull);
}

#define CVMX_TIM_MEM_DEBUG1 CVMX_TIM_MEM_DEBUG1_FUNC()
static inline uint64_t CVMX_TIM_MEM_DEBUG1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058001108ull);
}

#define CVMX_TIM_MEM_DEBUG2 CVMX_TIM_MEM_DEBUG2_FUNC()
static inline uint64_t CVMX_TIM_MEM_DEBUG2_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058001110ull);
}

#define CVMX_TIM_MEM_RING0 CVMX_TIM_MEM_RING0_FUNC()
static inline uint64_t CVMX_TIM_MEM_RING0_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058001000ull);
}

#define CVMX_TIM_MEM_RING1 CVMX_TIM_MEM_RING1_FUNC()
static inline uint64_t CVMX_TIM_MEM_RING1_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058001008ull);
}

#define CVMX_TIM_REG_BIST_RESULT CVMX_TIM_REG_BIST_RESULT_FUNC()
static inline uint64_t CVMX_TIM_REG_BIST_RESULT_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058000080ull);
}

#define CVMX_TIM_REG_ERROR CVMX_TIM_REG_ERROR_FUNC()
static inline uint64_t CVMX_TIM_REG_ERROR_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058000088ull);
}

#define CVMX_TIM_REG_FLAGS CVMX_TIM_REG_FLAGS_FUNC()
static inline uint64_t CVMX_TIM_REG_FLAGS_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058000000ull);
}

#define CVMX_TIM_REG_INT_MASK CVMX_TIM_REG_INT_MASK_FUNC()
static inline uint64_t CVMX_TIM_REG_INT_MASK_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058000090ull);
}

#define CVMX_TIM_REG_READ_IDX CVMX_TIM_REG_READ_IDX_FUNC()
static inline uint64_t CVMX_TIM_REG_READ_IDX_FUNC(void)
{
    return CVMX_ADD_IO_SEG(0x0001180058000008ull);
}

#define CVMX_TRA_BIST_STATUS CVMX_TRA_BIST_STATUS_FUNC()
static inline uint64_t CVMX_TRA_BIST_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_BIST_STATUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000010ull);
}

#define CVMX_TRA_CTL CVMX_TRA_CTL_FUNC()
static inline uint64_t CVMX_TRA_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000000ull);
}

#define CVMX_TRA_CYCLES_SINCE CVMX_TRA_CYCLES_SINCE_FUNC()
static inline uint64_t CVMX_TRA_CYCLES_SINCE_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_CYCLES_SINCE not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000018ull);
}

#define CVMX_TRA_CYCLES_SINCE1 CVMX_TRA_CYCLES_SINCE1_FUNC()
static inline uint64_t CVMX_TRA_CYCLES_SINCE1_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_CYCLES_SINCE1 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000028ull);
}

#define CVMX_TRA_FILT_ADR_ADR CVMX_TRA_FILT_ADR_ADR_FUNC()
static inline uint64_t CVMX_TRA_FILT_ADR_ADR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_FILT_ADR_ADR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000058ull);
}

#define CVMX_TRA_FILT_ADR_MSK CVMX_TRA_FILT_ADR_MSK_FUNC()
static inline uint64_t CVMX_TRA_FILT_ADR_MSK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_FILT_ADR_MSK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000060ull);
}

#define CVMX_TRA_FILT_CMD CVMX_TRA_FILT_CMD_FUNC()
static inline uint64_t CVMX_TRA_FILT_CMD_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_FILT_CMD not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000040ull);
}

#define CVMX_TRA_FILT_DID CVMX_TRA_FILT_DID_FUNC()
static inline uint64_t CVMX_TRA_FILT_DID_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_FILT_DID not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000050ull);
}

#define CVMX_TRA_FILT_SID CVMX_TRA_FILT_SID_FUNC()
static inline uint64_t CVMX_TRA_FILT_SID_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_FILT_SID not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000048ull);
}

#define CVMX_TRA_INT_STATUS CVMX_TRA_INT_STATUS_FUNC()
static inline uint64_t CVMX_TRA_INT_STATUS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_INT_STATUS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000008ull);
}

#define CVMX_TRA_READ_DAT CVMX_TRA_READ_DAT_FUNC()
static inline uint64_t CVMX_TRA_READ_DAT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_READ_DAT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000020ull);
}

#define CVMX_TRA_TRIG0_ADR_ADR CVMX_TRA_TRIG0_ADR_ADR_FUNC()
static inline uint64_t CVMX_TRA_TRIG0_ADR_ADR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG0_ADR_ADR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000098ull);
}

#define CVMX_TRA_TRIG0_ADR_MSK CVMX_TRA_TRIG0_ADR_MSK_FUNC()
static inline uint64_t CVMX_TRA_TRIG0_ADR_MSK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG0_ADR_MSK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A80000A0ull);
}

#define CVMX_TRA_TRIG0_CMD CVMX_TRA_TRIG0_CMD_FUNC()
static inline uint64_t CVMX_TRA_TRIG0_CMD_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG0_CMD not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000080ull);
}

#define CVMX_TRA_TRIG0_DID CVMX_TRA_TRIG0_DID_FUNC()
static inline uint64_t CVMX_TRA_TRIG0_DID_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG0_DID not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000090ull);
}

#define CVMX_TRA_TRIG0_SID CVMX_TRA_TRIG0_SID_FUNC()
static inline uint64_t CVMX_TRA_TRIG0_SID_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG0_SID not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A8000088ull);
}

#define CVMX_TRA_TRIG1_ADR_ADR CVMX_TRA_TRIG1_ADR_ADR_FUNC()
static inline uint64_t CVMX_TRA_TRIG1_ADR_ADR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG1_ADR_ADR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A80000D8ull);
}

#define CVMX_TRA_TRIG1_ADR_MSK CVMX_TRA_TRIG1_ADR_MSK_FUNC()
static inline uint64_t CVMX_TRA_TRIG1_ADR_MSK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG1_ADR_MSK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A80000E0ull);
}

#define CVMX_TRA_TRIG1_CMD CVMX_TRA_TRIG1_CMD_FUNC()
static inline uint64_t CVMX_TRA_TRIG1_CMD_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG1_CMD not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A80000C0ull);
}

#define CVMX_TRA_TRIG1_DID CVMX_TRA_TRIG1_DID_FUNC()
static inline uint64_t CVMX_TRA_TRIG1_DID_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG1_DID not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A80000D0ull);
}

#define CVMX_TRA_TRIG1_SID CVMX_TRA_TRIG1_SID_FUNC()
static inline uint64_t CVMX_TRA_TRIG1_SID_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN52XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_TRA_TRIG1_SID not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800A80000C8ull);
}

static inline uint64_t CVMX_USBCX_DAINT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DAINT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000818ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DAINTMSK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DAINTMSK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F001000081Cull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DCFG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DCFG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000800ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DCTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DCTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000804ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DIEPCTLX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 4)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_DIEPCTLX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000900ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_DIEPINTX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 4)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_DIEPINTX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000908ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_DIEPMSK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DIEPMSK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000810ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DIEPTSIZX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 4)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_DIEPTSIZX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000910ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_DOEPCTLX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 4)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_DOEPCTLX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000B00ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_DOEPINTX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 4)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_DOEPINTX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000B08ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_DOEPMSK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DOEPMSK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000814ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DOEPTSIZX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 4)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 4)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_DOEPTSIZX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000B10ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_DPTXFSIZX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((((offset >= 1) && (offset <= 4))) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((((offset >= 1) && (offset <= 4))) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((((offset >= 1) && (offset <= 4))) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((((offset >= 1) && (offset <= 4))) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((((offset >= 1) && (offset <= 4))) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_DPTXFSIZX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000100ull) + ((offset&7) + (block_id&1)*0x40000000000ull)*4;
}

static inline uint64_t CVMX_USBCX_DSTS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DSTS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000808ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DTKNQR1(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DTKNQR1(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000820ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DTKNQR2(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DTKNQR2(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000824ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DTKNQR3(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DTKNQR3(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000830ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_DTKNQR4(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_DTKNQR4(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000834ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GAHBCFG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GAHBCFG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000008ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GHWCFG1(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GHWCFG1(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000044ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GHWCFG2(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GHWCFG2(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000048ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GHWCFG3(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GHWCFG3(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F001000004Cull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GHWCFG4(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GHWCFG4(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000050ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GINTMSK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GINTMSK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000018ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GINTSTS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GINTSTS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000014ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GNPTXFSIZ(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GNPTXFSIZ(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000028ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GNPTXSTS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GNPTXSTS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F001000002Cull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GOTGCTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GOTGCTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000000ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GOTGINT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GOTGINT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000004ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GRSTCTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GRSTCTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000010ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GRXFSIZ(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GRXFSIZ(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000024ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GRXSTSPD(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GRXSTSPD(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010040020ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GRXSTSPH(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GRXSTSPH(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000020ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GRXSTSRD(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GRXSTSRD(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F001004001Cull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GRXSTSRH(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GRXSTSRH(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F001000001Cull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GSNPSID(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GSNPSID(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000040ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_GUSBCFG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_GUSBCFG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F001000000Cull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_HAINT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_HAINT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000414ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_HAINTMSK(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_HAINTMSK(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000418ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_HCCHARX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 7)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_HCCHARX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000500ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_HCFG(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_HCFG(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000400ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_HCINTMSKX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 7)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_HCINTMSKX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F001000050Cull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_HCINTX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 7)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_HCINTX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000508ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_HCSPLTX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 7)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_HCSPLTX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000504ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_HCTSIZX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 7)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_HCTSIZX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000510ull) + ((offset&7) + (block_id&1)*0x8000000000ull)*32;
}

static inline uint64_t CVMX_USBCX_HFIR(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_HFIR(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000404ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_HFNUM(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_HFNUM(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000408ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_HPRT(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_HPRT(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000440ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_HPTXFSIZ(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_HPTXFSIZ(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000100ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_HPTXSTS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_HPTXSTS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000410ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBCX_NPTXDFIFOX(unsigned long offset, unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && (((offset <= 7)) && ((block_id == 0)))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && (((offset <= 7)) && ((block_id <= 1))))))
        cvmx_warn("CVMX_USBCX_NPTXDFIFOX(%lu,%lu) is invalid on this chip\n", offset, block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010001000ull) + ((offset&7) + (block_id&1)*0x100000000ull)*4096;
}

static inline uint64_t CVMX_USBCX_PCGCCTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBCX_PCGCCTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0010000E00ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_BIST_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_BIST_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00011800680007F8ull) + (block_id&1)*0x10000000ull;
}

static inline uint64_t CVMX_USBNX_CLK_CTL(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_CLK_CTL(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180068000010ull) + (block_id&1)*0x10000000ull;
}

static inline uint64_t CVMX_USBNX_CTL_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_CTL_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000800ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_INB_CHN0(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_INB_CHN0(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000818ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_INB_CHN1(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_INB_CHN1(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000820ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_INB_CHN2(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_INB_CHN2(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000828ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_INB_CHN3(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_INB_CHN3(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000830ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_INB_CHN4(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_INB_CHN4(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000838ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_INB_CHN5(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_INB_CHN5(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000840ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_INB_CHN6(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_INB_CHN6(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000848ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_INB_CHN7(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_INB_CHN7(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000850ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_OUTB_CHN0(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_OUTB_CHN0(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000858ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_OUTB_CHN1(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_OUTB_CHN1(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000860ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_OUTB_CHN2(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_OUTB_CHN2(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000868ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_OUTB_CHN3(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_OUTB_CHN3(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000870ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_OUTB_CHN4(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_OUTB_CHN4(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000878ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_OUTB_CHN5(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_OUTB_CHN5(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000880ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_OUTB_CHN6(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_OUTB_CHN6(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000888ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA0_OUTB_CHN7(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA0_OUTB_CHN7(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000890ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_DMA_TEST(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_DMA_TEST(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x00016F0000000808ull) + (block_id&1)*0x100000000000ull;
}

static inline uint64_t CVMX_USBNX_INT_ENB(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_INT_ENB(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180068000008ull) + (block_id&1)*0x10000000ull;
}

static inline uint64_t CVMX_USBNX_INT_SUM(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_INT_SUM(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180068000000ull) + (block_id&1)*0x10000000ull;
}

static inline uint64_t CVMX_USBNX_USBP_CTL_STATUS(unsigned long block_id)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(
        (OCTEON_IS_MODEL(OCTEON_CN30XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN50XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN31XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN56XX) && ((block_id == 0))) ||
        (OCTEON_IS_MODEL(OCTEON_CN52XX) && ((block_id <= 1)))))
        cvmx_warn("CVMX_USBNX_USBP_CTL_STATUS(%lu) is invalid on this chip\n", block_id);
#endif
    return CVMX_ADD_IO_SEG(0x0001180068000018ull) + (block_id&1)*0x10000000ull;
}

#define CVMX_ZIP_CMD_BIST_RESULT CVMX_ZIP_CMD_BIST_RESULT_FUNC()
static inline uint64_t CVMX_ZIP_CMD_BIST_RESULT_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ZIP_CMD_BIST_RESULT not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180038000080ull);
}

#define CVMX_ZIP_CMD_BUF CVMX_ZIP_CMD_BUF_FUNC()
static inline uint64_t CVMX_ZIP_CMD_BUF_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ZIP_CMD_BUF not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180038000008ull);
}

#define CVMX_ZIP_CMD_CTL CVMX_ZIP_CMD_CTL_FUNC()
static inline uint64_t CVMX_ZIP_CMD_CTL_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ZIP_CMD_CTL not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180038000000ull);
}

#define CVMX_ZIP_CONSTANTS CVMX_ZIP_CONSTANTS_FUNC()
static inline uint64_t CVMX_ZIP_CONSTANTS_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ZIP_CONSTANTS not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x00011800380000A0ull);
}

#define CVMX_ZIP_DEBUG0 CVMX_ZIP_DEBUG0_FUNC()
static inline uint64_t CVMX_ZIP_DEBUG0_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ZIP_DEBUG0 not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180038000098ull);
}

#define CVMX_ZIP_ERROR CVMX_ZIP_ERROR_FUNC()
static inline uint64_t CVMX_ZIP_ERROR_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ZIP_ERROR not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180038000088ull);
}

#define CVMX_ZIP_INT_MASK CVMX_ZIP_INT_MASK_FUNC()
static inline uint64_t CVMX_ZIP_INT_MASK_FUNC(void)
{
#if CVMX_ENABLE_CSR_ADDRESS_CHECKING
    if (!(OCTEON_IS_MODEL(OCTEON_CN31XX) || OCTEON_IS_MODEL(OCTEON_CN38XX) || OCTEON_IS_MODEL(OCTEON_CN56XX) || OCTEON_IS_MODEL(OCTEON_CN58XX)))
        cvmx_warn("CVMX_ZIP_INT_MASK not supported on this chip\n");
#endif
    return CVMX_ADD_IO_SEG(0x0001180038000090ull);
}


#endif /* __CVMX_CSR_ADDRESSES_H__ */
