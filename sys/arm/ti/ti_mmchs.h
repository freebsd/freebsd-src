/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * $FreeBSD$
 */
#ifndef _TI_MMCHS_H_
#define _TI_MMCHS_H_

/**
 * Header file for the TI MMC/SD/SDIO driver.
 *
 * Simply contains register addresses and bit flags.
 */

/* Register offsets within each of the MMC/SD/SDIO controllers */
#define MMCHS_SYSCONFIG             0x010
#define MMCHS_SYSSTATUS             0x014
#define MMCHS_CSRE                  0x024
#define MMCHS_SYSTEST               0x028
#define MMCHS_CON                   0x02C
#define MMCHS_PWCNT                 0x030
#define MMCHS_BLK                   0x104
#define MMCHS_ARG                   0x108
#define MMCHS_CMD                   0x10C
#define MMCHS_RSP10                 0x110
#define MMCHS_RSP32                 0x114
#define MMCHS_RSP54                 0x118
#define MMCHS_RSP76                 0x11C
#define MMCHS_DATA                  0x120
#define MMCHS_PSTATE                0x124
#define MMCHS_HCTL                  0x128
#define MMCHS_SYSCTL                0x12C
#define MMCHS_STAT                  0x130
#define MMCHS_IE                    0x134
#define MMCHS_ISE                   0x138
#define MMCHS_AC12                  0x13C
#define MMCHS_CAPA                  0x140
#define MMCHS_CUR_CAPA              0x148
#define MMCHS_REV                   0x1FC

/* OMAP4 and OMAP4 have different register addresses */
#define OMAP3_MMCHS_REG_OFFSET      0x000
#define OMAP4_MMCHS_REG_OFFSET      0x100
#define AM335X_MMCHS_REG_OFFSET     0x100

/* Register bit settings */
#define	MMCHS_SYSCONFIG_CLK_FUN	    (2 << 8)
#define	MMCHS_SYSCONFIG_CLK_IFC	    (1 << 8)
#define	MMCHS_SYSCONFIG_SIDL	    (2 << 3)
#define	MMCHS_SYSCONFIG_ENW	    (1 << 2)
#define	MMCHS_SYSCONFIG_SRST	    (1 << 1)
#define	MMCHS_SYSCONFIG_AIDL	    (1 << 0)
#define MMCHS_STAT_BADA             (1UL << 29)
#define MMCHS_STAT_CERR             (1UL << 28)
#define MMCHS_STAT_ACE              (1UL << 24)
#define MMCHS_STAT_DEB              (1UL << 22)
#define MMCHS_STAT_DCRC             (1UL << 21)
#define MMCHS_STAT_DTO              (1UL << 20)
#define MMCHS_STAT_CIE              (1UL << 19)
#define MMCHS_STAT_CEB              (1UL << 18)
#define MMCHS_STAT_CCRC             (1UL << 17)
#define MMCHS_STAT_CTO              (1UL << 16)
#define MMCHS_STAT_ERRI             (1UL << 15)
#define MMCHS_STAT_OBI              (1UL << 9)
#define MMCHS_STAT_CIRQ             (1UL << 8)
#define MMCHS_STAT_BRR              (1UL << 5)
#define MMCHS_STAT_BWR              (1UL << 4)
#define MMCHS_STAT_BGE              (1UL << 2)
#define MMCHS_STAT_TC               (1UL << 1)
#define MMCHS_STAT_CC               (1UL << 0)

#define MMCHS_STAT_CLEAR_MASK       0x3BFF8337UL

#define MMCHS_SYSCTL_SRD            (1UL << 26)
#define MMCHS_SYSCTL_SRC            (1UL << 25)
#define MMCHS_SYSCTL_SRA            (1UL << 24)
#define MMCHS_SYSCTL_DTO(x)         (((x) & 0xf) << 16)
#define MMCHS_SYSCTL_DTO_MASK       MMCHS_SYSCTL_DTO(0xf)
#define MMCHS_SYSCTL_CLKD(x)        (((x) & 0x3ff) << 6)
#define MMCHS_SYSCTL_CLKD_MASK      MMCHS_SYSCTL_CLKD(0x3ff)
#define MMCHS_SYSCTL_CEN            (1UL << 2)
#define MMCHS_SYSCTL_ICS            (1UL << 1)
#define MMCHS_SYSCTL_ICE            (1UL << 0)

#define MMCHS_HCTL_OBWE             (1UL << 27)
#define MMCHS_HCTL_REM              (1UL << 26)
#define MMCHS_HCTL_INS              (1UL << 25)
#define MMCHS_HCTL_IWE              (1UL << 24)
#define MMCHS_HCTL_IBG              (1UL << 19)
#define MMCHS_HCTL_RWC              (1UL << 18)
#define MMCHS_HCTL_CR               (1UL << 17)
#define MMCHS_HCTL_SBGR             (1UL << 16)
#define MMCHS_HCTL_SDVS_MASK        (7UL << 9)
#define MMCHS_HCTL_SDVS_V18         (5UL << 9)
#define MMCHS_HCTL_SDVS_V30         (6UL << 9)
#define MMCHS_HCTL_SDVS_V33         (7UL << 9)
#define MMCHS_HCTL_SDBP             (1UL << 8)
#define MMCHS_HCTL_DTW              (1UL << 1)

#define MMCHS_CAPA_VS18             (1UL << 26)
#define MMCHS_CAPA_VS30             (1UL << 25)
#define MMCHS_CAPA_VS33             (1UL << 24)

#define MMCHS_CMD_CMD_TYPE_IO_ABORT (3UL << 21)
#define MMCHS_CMD_CMD_TYPE_FUNC_SEL (2UL << 21)
#define MMCHS_CMD_CMD_TYPE_SUSPEND  (1UL << 21)
#define MMCHS_CMD_CMD_TYPE_OTHERS   (0UL << 21)
#define MMCHS_CMD_CMD_TYPE_MASK     (3UL << 22)

#define MMCHS_CMD_DP                (1UL << 21)
#define MMCHS_CMD_CICE              (1UL << 20)
#define MMCHS_CMD_CCCE              (1UL << 19)

#define MMCHS_CMD_RSP_TYPE_MASK     (3UL << 16)
#define MMCHS_CMD_RSP_TYPE_NO       (0UL << 16)
#define MMCHS_CMD_RSP_TYPE_136      (1UL << 16)
#define MMCHS_CMD_RSP_TYPE_48       (2UL << 16)
#define MMCHS_CMD_RSP_TYPE_48_BSY   (3UL << 16)

#define MMCHS_CMD_MSBS              (1UL << 5)
#define MMCHS_CMD_DDIR              (1UL << 4)
#define MMCHS_CMD_ACEN              (1UL << 2)
#define MMCHS_CMD_BCE               (1UL << 1)
#define MMCHS_CMD_DE                (1UL << 0)

#define MMCHS_CON_CLKEXTFREE        (1UL << 16)
#define MMCHS_CON_PADEN             (1UL << 15)
#define MMCHS_CON_OBIE              (1UL << 14)
#define MMCHS_CON_OBIP              (1UL << 13)
#define MMCHS_CON_CEATA             (1UL << 12)
#define MMCHS_CON_CTPL              (1UL << 11)

#define MMCHS_CON_DVAL_8_4MS        (3UL << 9)
#define MMCHS_CON_DVAL_1MS          (2UL << 9)
#define MMCHS_CON_DVAL_231US        (1UL << 9)
#define MMCHS_CON_DVAL_33US         (0UL << 9)
#define MMCHS_CON_DVAL_MASK         (3UL << 9)

#define MMCHS_CON_WPP               (1UL << 8)
#define MMCHS_CON_CDP               (1UL << 7)
#define MMCHS_CON_MIT               (1UL << 6)
#define MMCHS_CON_DW8               (1UL << 5)
#define MMCHS_CON_MODE              (1UL << 4)
#define MMCHS_CON_STR               (1UL << 3)
#define MMCHS_CON_HR                (1UL << 2)
#define MMCHS_CON_INIT              (1UL << 1)
#define MMCHS_CON_OD                (1UL << 0)

#define MMCHS_CAPA_VS18             (1UL << 26)
#define MMCHS_CAPA_VS30             (1UL << 25)
#define MMCHS_CAPA_VS33             (1UL << 24)

#endif  /* _TI_MMCHS_H_ */
