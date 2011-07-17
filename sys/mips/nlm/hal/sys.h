/*-
 * Copyright 2003-2011 Netlogic Microsystems (Netlogic). All rights
 * reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY Netlogic Microsystems ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL NETLOGIC OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD$
 * NETLOGIC_BSD */

#ifndef __NLM_SYS_H__
#define __NLM_SYS_H__

/**
* @file_name sys.h
* @author Netlogic Microsystems
* @brief HAL for System configuration registers
*/
#define	XLP_SYS_CHIP_RESET_REG			0x40
#define	XLP_SYS_POWER_ON_RESET_REG		0x41
#define	XLP_SYS_EFUSE_DEVICE_CFG_STATUS0_REG	0x42
#define	XLP_SYS_EFUSE_DEVICE_CFG_STATUS1_REG	0x43
#define	XLP_SYS_EFUSE_DEVICE_CFG_STATUS2_REG	0x44
#define	XLP_SYS_EFUSE_DEVICE_CFG3_REG		0x45
#define	XLP_SYS_EFUSE_DEVICE_CFG4_REG		0x46
#define	XLP_SYS_EFUSE_DEVICE_CFG5_REG		0x47
#define	XLP_SYS_EFUSE_DEVICE_CFG6_REG		0x48
#define	XLP_SYS_EFUSE_DEVICE_CFG7_REG		0x49
#define	XLP_SYS_PLL_CTRL_REG			0x4a
#define	XLP_SYS_CPU_RESET_REG			0x4b
#define	XLP_SYS_CPU_NONCOHERENT_MODE_REG	0x4d
#define	XLP_SYS_CORE_DFS_DIS_CTRL_REG		0x4e
#define	XLP_SYS_CORE_DFS_RST_CTRL_REG		0x4f
#define	XLP_SYS_CORE_DFS_BYP_CTRL_REG		0x50
#define	XLP_SYS_CORE_DFS_PHA_CTRL_REG		0x51
#define	XLP_SYS_CORE_DFS_DIV_INC_CTRL_REG	0x52
#define	XLP_SYS_CORE_DFS_DIV_DEC_CTRL_REG	0x53
#define	XLP_SYS_CORE_DFS_DIV_VALUE_REG		0x54
#define	XLP_SYS_RESET_REG			0x55
#define	XLP_SYS_DFS_DIS_CTRL_REG		0x56
#define	XLP_SYS_DFS_RST_CTRL_REG		0x57
#define	XLP_SYS_DFS_BYP_CTRL_REG		0x58
#define	XLP_SYS_DFS_DIV_INC_CTRL_REG		0x59
#define	XLP_SYS_DFS_DIV_DEC_CTRL_REG		0x5a
#define	XLP_SYS_DFS_DIV_VALUE0_REG		0x5b
#define	XLP_SYS_DFS_DIV_VALUE1_REG		0x5c
#define	XLP_SYS_SENSE_AMP_DLY_REG		0x5d
#define	XLP_SYS_SOC_SENSE_AMP_DLY_REG		0x5e
#define	XLP_SYS_CTRL0_REG			0x5f
#define	XLP_SYS_CTRL1_REG			0x60
#define	XLP_SYS_TIMEOUT_BS1_REG			0x61
#define	XLP_SYS_BYTE_SWAP_REG			0x62
#define	XLP_SYS_VRM_VID_REG			0x63
#define	XLP_SYS_PWR_RAM_CMD_REG			0x64
#define	XLP_SYS_PWR_RAM_ADDR_REG		0x65
#define	XLP_SYS_PWR_RAM_DATA0_REG		0x66
#define	XLP_SYS_PWR_RAM_DATA1_REG		0x67
#define	XLP_SYS_PWR_RAM_DATA2_REG		0x68
#define	XLP_SYS_PWR_UCODE_REG			0x69
#define	XLP_SYS_CPU0_PWR_STATUS_REG		0x6a
#define	XLP_SYS_CPU1_PWR_STATUS_REG		0x6b
#define	XLP_SYS_CPU2_PWR_STATUS_REG		0x6c
#define	XLP_SYS_CPU3_PWR_STATUS_REG		0x6d
#define	XLP_SYS_CPU4_PWR_STATUS_REG		0x6e
#define	XLP_SYS_CPU5_PWR_STATUS_REG		0x6f
#define	XLP_SYS_CPU6_PWR_STATUS_REG		0x70
#define	XLP_SYS_CPU7_PWR_STATUS_REG		0x71
#define	XLP_SYS_STATUS_REG			0x72
#define	XLP_SYS_INT_POL_REG			0x73
#define	XLP_SYS_INT_TYPE_REG			0x74
#define	XLP_SYS_INT_STATUS_REG			0x75
#define	XLP_SYS_INT_MASK0_REG			0x76
#define	XLP_SYS_INT_MASK1_REG			0x77
#define	XLP_SYS_UCO_S_ECC_REG			0x78
#define	XLP_SYS_UCO_M_ECC_REG			0x79
#define	XLP_SYS_UCO_ADDR_REG			0x7a
#define	XLP_SYS_UCO_INSTR_REG			0x7b
#define	XLP_SYS_MEM_BIST0_REG			0x7c
#define	XLP_SYS_MEM_BIST1_REG			0x7d
#define	XLP_SYS_MEM_BIST2_REG			0x7e
#define	XLP_SYS_MEM_BIST3_REG			0x7f
#define	XLP_SYS_MEM_BIST4_REG			0x80
#define	XLP_SYS_MEM_BIST5_REG			0x81
#define	XLP_SYS_MEM_BIST6_REG			0x82
#define	XLP_SYS_MEM_BIST7_REG			0x83
#define	XLP_SYS_MEM_BIST8_REG			0x84
#define	XLP_SYS_MEM_BIST9_REG			0x85
#define	XLP_SYS_MEM_BIST10_REG			0x86
#define	XLP_SYS_MEM_BIST11_REG			0x87
#define	XLP_SYS_MEM_BIST12_REG			0x88
#define	XLP_SYS_SCRTCH0_REG			0x89
#define	XLP_SYS_SCRTCH1_REG			0x8a
#define	XLP_SYS_SCRTCH2_REG			0x8b
#define	XLP_SYS_SCRTCH3_REG			0x8c

#if !defined(LOCORE) && !defined(__ASSEMBLY__)

#define	nlm_rdreg_sys(b, r)		nlm_read_reg_kseg(b,r)
#define	nlm_wreg_sys(b, r, v)		nlm_write_reg_kseg(b,r,v)
#define	nlm_pcibase_sys(node)		nlm_pcicfg_base(XLP_IO_SYS_OFFSET(node))
#define	nlm_regbase_sys(node)		nlm_pcibase_sys(node)

#endif

#endif
