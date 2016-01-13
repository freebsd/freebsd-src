/*-
 * Copyright (c) 2012-2015 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 *
 * $FreeBSD$
 */

#ifndef _SYS_EFX_CHECK_H
#define	_SYS_EFX_CHECK_H

#include "efsys.h"

/*
 * Check that the efsys.h header in client code has a valid combination of
 * EFSYS_OPT_xxx options.
 *
 * NOTE: Keep checks for obsolete options here to ensure that they are removed
 * from client code (and do not reappear in merges from other branches).
 */

/* Support NVRAM based boot config */
#if EFSYS_OPT_BOOTCFG
# if !EFSYS_OPT_NVRAM
#  error "BOOTCFG requires NVRAM"
# endif
#endif /* EFSYS_OPT_BOOTCFG */

/* Verify chip implements accessed registers */
#if EFSYS_OPT_CHECK_REG
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "CHECK_REG requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_CHECK_REG */

/* Decode fatal errors */
#if EFSYS_OPT_DECODE_INTR_FATAL
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA)
#  error "INTR_FATAL requires FALCON or SIENA"
# endif
#endif /* EFSYS_OPT_DECODE_INTR_FATAL */

/* Support diagnostic hardware tests */
#if EFSYS_OPT_DIAG
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "DIAG requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_DIAG */

/* Support optimized EVQ data access */
#if EFSYS_OPT_EV_PREFETCH
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "EV_PREFETCH requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_EV_PREFETCH */

/* Support overriding the NVRAM and VPD configuration */
#if EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE
# if !EFSYS_OPT_FALCON
#  error "FALCON_NIC_CFG_OVERRIDE requires FALCON"
# endif
#endif /* EFSYS_OPT_FALCON_NIC_CFG_OVERRIDE */

/* Support hardware packet filters */
#if EFSYS_OPT_FILTER
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "FILTER requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_FILTER */

#if (EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
# if !EFSYS_OPT_FILTER
#  error "HUNTINGTON or MEDFORD requires FILTER"
# endif
#endif /* EFSYS_OPT_HUNTINGTON */

/* Support hardware loopback modes */
#if EFSYS_OPT_LOOPBACK
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "LOOPBACK requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_LOOPBACK */

/* Support Falcon GMAC */
#if EFSYS_OPT_MAC_FALCON_GMAC
# if !EFSYS_OPT_FALCON
#  error "MAC_FALCON_GMAC requires FALCON"
# endif
#endif /* EFSYS_OPT_MAC_FALCON_GMAC */

/* Support Falcon XMAC */
#if EFSYS_OPT_MAC_FALCON_XMAC
# if !EFSYS_OPT_FALCON
#  error "MAC_FALCON_XMAC requires FALCON"
# endif
#endif /* EFSYS_OPT_MAC_FALCON_XMAC */

/* Support MAC statistics */
#if EFSYS_OPT_MAC_STATS
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "MAC_STATS requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_MAC_STATS */

/* Support management controller messages */
#if EFSYS_OPT_MCDI
# if !(EFSYS_OPT_SIENA || EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "MCDI requires SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_MCDI */

#if (EFSYS_OPT_SIENA || EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
# if !EFSYS_OPT_MCDI
#  error "SIENA or HUNTINGTON or MEDFORD requires MCDI"
# endif
#endif

/* Support MCDI logging */
#if EFSYS_OPT_MCDI_LOGGING
# if !EFSYS_OPT_MCDI
#  error "MCDI_LOGGING requires MCDI"
# endif
#endif /* EFSYS_OPT_MCDI_LOGGING */

/* Support MCDI proxy authorization */
#if EFSYS_OPT_MCDI_PROXY_AUTH
# if !EFSYS_OPT_MCDI
#  error "MCDI_PROXY_AUTH requires MCDI"
# endif
#endif /* EFSYS_OPT_MCDI_PROXY_AUTH */

/* Support LM87 monitor */
#if EFSYS_OPT_MON_LM87
# if !EFSYS_OPT_FALCON
#  error "MON_LM87 requires FALCON"
# endif
#endif /* EFSYS_OPT_MON_LM87 */

/* Support MAX6647 monitor */
#if EFSYS_OPT_MON_MAX6647
# if !EFSYS_OPT_FALCON
#  error "MON_MAX6647 requires FALCON"
# endif
#endif /* EFSYS_OPT_MON_MAX6647 */

/* Support null monitor */
#if EFSYS_OPT_MON_NULL
# if !EFSYS_OPT_FALCON
#  error "MON_NULL requires FALCON"
# endif
#endif /* EFSYS_OPT_MON_NULL */

/* Obsolete option */
#ifdef EFSYS_OPT_MON_SIENA
#  error "MON_SIENA is obsolete (replaced by MON_MCDI)."
#endif /* EFSYS_OPT_MON_SIENA*/

/* Obsolete option */
#ifdef EFSYS_OPT_MON_HUNTINGTON
#  error "MON_HUNTINGTON is obsolete (replaced by MON_MCDI)."
#endif /* EFSYS_OPT_MON_HUNTINGTON*/

/* Support monitor statistics (voltage/temperature) */
#if EFSYS_OPT_MON_STATS
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "MON_STATS requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_MON_STATS */

/* Support Monitor via mcdi */
#if EFSYS_OPT_MON_MCDI
# if !(EFSYS_OPT_SIENA || EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "MON_MCDI requires SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_MON_MCDI*/

/* Support printable names for statistics */
#if EFSYS_OPT_NAMES
# if !(EFSYS_OPT_LOOPBACK || EFSYS_OPT_MAC_STATS || EFSYS_OPT_MCDI || \
	EFSYS_MON_STATS || EFSYS_OPT_PHY_PROPS || EFSYS_OPT_PHY_STATS || \
	EFSYS_OPT_QSTATS)
#  error "NAMES requires LOOPBACK or xxxSTATS or MCDI or PHY_PROPS"
# endif
#endif /* EFSYS_OPT_NAMES */

/* Support non volatile configuration */
#if EFSYS_OPT_NVRAM
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "NVRAM requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_NVRAM */

/* Support Falcon bootrom */
#if EFSYS_OPT_NVRAM_FALCON_BOOTROM
# if !EFSYS_OPT_NVRAM
#  error "NVRAM_FALCON_BOOTROM requires NVRAM"
# endif
# if !EFSYS_OPT_FALCON
#  error "NVRAM_FALCON_BOOTROM requires FALCON"
# endif
#endif /* EFSYS_OPT_NVRAM_FALCON_BOOTROM */

/* Support NVRAM config for SFT9001 */
#if EFSYS_OPT_NVRAM_SFT9001
# if !EFSYS_OPT_NVRAM
#  error "NVRAM_SFT9001 requires NVRAM"
# endif
# if !EFSYS_OPT_FALCON
#  error "NVRAM_SFT9001 requires FALCON"
# endif
#endif /* EFSYS_OPT_NVRAM_SFT9001 */

/* Support NVRAM config for SFX7101 */
#if EFSYS_OPT_NVRAM_SFX7101
# if !EFSYS_OPT_NVRAM
#  error "NVRAM_SFX7101 requires NVRAM"
# endif
# if !EFSYS_OPT_FALCON
#  error "NVRAM_SFX7101 requires FALCON"
# endif
#endif /* EFSYS_OPT_NVRAM_SFX7101 */

/* Support PCIe interface tuning */
#if EFSYS_OPT_PCIE_TUNE
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA)
#  error "PCIE_TUNE requires FALCON or SIENA"
# endif
#endif /* EFSYS_OPT_PCIE_TUNE */

/* Obsolete option */
#if EFSYS_OPT_PHY_BIST
#  error "PHY_BIST is obsolete (replaced by BIST)."
#endif /* EFSYS_OPT_PHY_BIST */

/* Support PHY flags */
#if EFSYS_OPT_PHY_FLAGS
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA)
#  error "PHY_FLAGS requires FALCON or SIENA"
# endif
#endif /* EFSYS_OPT_PHY_FLAGS */

/* Support for PHY LED control */
#if EFSYS_OPT_PHY_LED_CONTROL
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA)
#  error "PHY_LED_CONTROL requires FALCON or SIENA"
# endif
#endif /* EFSYS_OPT_PHY_LED_CONTROL */

/* Support NULL PHY */
#if EFSYS_OPT_PHY_NULL
# if !EFSYS_OPT_FALCON
#  error "PHY_NULL requires FALCON"
# endif
#endif /* EFSYS_OPT_PHY_NULL */

/* Obsolete option */
#ifdef EFSYS_OPT_PHY_PM8358
# error "EFSYS_OPT_PHY_PM8358 is obsolete and is not supported."
#endif

/* Support PHY properties */
#if EFSYS_OPT_PHY_PROPS
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA)
#  error "PHY_PROPS requires FALCON or SIENA"
# endif
#endif /* EFSYS_OPT_PHY_PROPS */

/* Support QT2022C2 PHY */
#if EFSYS_OPT_PHY_QT2022C2
# if !EFSYS_OPT_FALCON
#  error "PHY_QT2022C2 requires FALCON"
# endif
#endif /* EFSYS_OPT_PHY_QT2022C2 */

/* Support QT2025C PHY (Wakefield NIC) */
#if EFSYS_OPT_PHY_QT2025C
# if !EFSYS_OPT_FALCON
#  error "PHY_QT2025C requires FALCON"
# endif
#endif /* EFSYS_OPT_PHY_QT2025C */

/* Support SFT9001 PHY (Starbolt NIC) */
#if EFSYS_OPT_PHY_SFT9001
# if !EFSYS_OPT_FALCON
#  error "PHY_SFT9001 requires FALCON"
# endif
#endif /* EFSYS_OPT_PHY_SFT9001 */

/* Support SFX7101 PHY (SFE4001 NIC) */
#if EFSYS_OPT_PHY_SFX7101
# if !EFSYS_OPT_FALCON
#  error "PHY_SFX7101 requires FALCON"
# endif
#endif /* EFSYS_OPT_PHY_SFX7101 */

/* Support PHY statistics */
#if EFSYS_OPT_PHY_STATS
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA)
#  error "PHY_STATS requires FALCON or SIENA"
# endif
#endif /* EFSYS_OPT_PHY_STATS */

/* Support TXC43128 PHY (SFE4003 NIC) */
#if EFSYS_OPT_PHY_TXC43128
# if !EFSYS_OPT_FALCON
#  error "PHY_TXC43128 requires FALCON"
# endif
#endif /* EFSYS_OPT_PHY_TXC43128 */

/* Support EVQ/RXQ/TXQ statistics */
#if EFSYS_OPT_QSTATS
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "QSTATS requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_QSTATS */

/* Obsolete option */
#ifdef EFSYS_OPT_RX_HDR_SPLIT
# error "RX_HDR_SPLIT is obsolete and is not supported"
#endif /* EFSYS_OPT_RX_HDR_SPLIT */

/* Support receive scaling (RSS) */
#if EFSYS_OPT_RX_SCALE
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "RX_SCALE requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_RX_SCALE */

/* Support receive scatter DMA */
#if EFSYS_OPT_RX_SCATTER
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "RX_SCATTER requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_RX_SCATTER */

/* Obsolete option */
#ifdef EFSYS_OPT_STAT_NAME
# error "STAT_NAME is obsolete (replaced by NAMES)."
#endif

/* Support PCI Vital Product Data (VPD) */
#if EFSYS_OPT_VPD
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "VPD requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_VPD */

/* Support Wake on LAN */
#if EFSYS_OPT_WOL
# if !EFSYS_OPT_SIENA
#  error "WOL requires SIENA"
# endif
#endif /* EFSYS_OPT_WOL */

/* Obsolete option */
#ifdef EFSYS_OPT_MCAST_FILTER_LIST
#  error "MCAST_FILTER_LIST is obsolete and is not supported"
#endif /* EFSYS_OPT_MCAST_FILTER_LIST */

/* Support BIST */
#if EFSYS_OPT_BIST
# if !(EFSYS_OPT_FALCON || EFSYS_OPT_SIENA || \
	EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD)
#  error "BIST requires FALCON or SIENA or HUNTINGTON or MEDFORD"
# endif
#endif /* EFSYS_OPT_BIST */

#endif /* _SYS_EFX_CHECK_H */
