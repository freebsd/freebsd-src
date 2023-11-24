/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_C4XXX_HW_DATA_H_
#define ADF_C4XXX_HW_DATA_H_

#include <adf_accel_devices.h>

/* PCIe configuration space */
#define ADF_C4XXX_SRAM_BAR 0
#define ADF_C4XXX_PMISC_BAR 1
#define ADF_C4XXX_ETR_BAR 2
#define ADF_C4XXX_RX_RINGS_OFFSET 4
#define ADF_C4XXX_TX_RINGS_MASK 0xF

#define ADF_C4XXX_MAX_ACCELERATORS 12
#define ADF_C4XXX_MAX_ACCELUNITS 6
#define ADF_C4XXX_MAX_ACCELENGINES 32
#define ADF_C4XXX_ACCELERATORS_REG_OFFSET 16

/* Soft straps offsets */
#define ADF_C4XXX_SOFTSTRAPPULL0_OFFSET (0x344)
#define ADF_C4XXX_SOFTSTRAPPULL1_OFFSET (0x348)
#define ADF_C4XXX_SOFTSTRAPPULL2_OFFSET (0x34C)

/* Physical function fuses offsets */
#define ADF_C4XXX_FUSECTL0_OFFSET (0x350)
#define ADF_C4XXX_FUSECTL1_OFFSET (0x354)
#define ADF_C4XXX_FUSECTL2_OFFSET (0x358)

#define ADF_C4XXX_FUSE_PKE_MASK (0xFFF000)
#define ADF_C4XXX_FUSE_COMP_MASK (0x000FFF)
#define ADF_C4XXX_FUSE_PROD_SKU_MASK BIT(31)

#define ADF_C4XXX_LEGFUSE_BASE_SKU_MASK (BIT(2) | BIT(3))

#define ADF_C4XXX_FUSE_DISABLE_INLINE_INGRESS BIT(12)
#define ADF_C4XXX_FUSE_DISABLE_INLINE_EGRESS BIT(13)
#define ADF_C4XXX_FUSE_DISABLE_INLINE_MASK                                     \
	(ADF_C4XXX_FUSE_DISABLE_INLINE_INGRESS |                               \
	 ADF_C4XXX_FUSE_DISABLE_INLINE_EGRESS)

#define ADF_C4XXX_ACCELERATORS_MASK (0xFFF)
#define ADF_C4XXX_ACCELENGINES_MASK (0xFFFFFFFF)

#define ADF_C4XXX_ETR_MAX_BANKS 128
#define ADF_C4XXX_SMIAPF0_MASK_OFFSET (0x60000 + 0x20)
#define ADF_C4XXX_SMIAPF1_MASK_OFFSET (0x60000 + 0x24)
#define ADF_C4XXX_SMIAPF2_MASK_OFFSET (0x60000 + 0x28)
#define ADF_C4XXX_SMIAPF3_MASK_OFFSET (0x60000 + 0x2C)
#define ADF_C4XXX_SMIAPF4_MASK_OFFSET (0x60000 + 0x30)
#define ADF_C4XXX_SMIA0_MASK 0xFFFFFFFF
#define ADF_C4XXX_SMIA1_MASK 0xFFFFFFFF
#define ADF_C4XXX_SMIA2_MASK 0xFFFFFFFF
#define ADF_C4XXX_SMIA3_MASK 0xFFFFFFFF
#define ADF_C4XXX_SMIA4_MASK 0x1
/* Bank and ring configuration */
#define ADF_C4XXX_NUM_RINGS_PER_BANK 8
/* Error detection and correction */
#define ADF_C4XXX_AE_CTX_ENABLES(i) (0x40818 + ((i)*0x1000))
#define ADF_C4XXX_AE_MISC_CONTROL(i) (0x40960 + ((i)*0x1000))
#define ADF_C4XXX_ENABLE_AE_ECC_ERR BIT(28)
#define ADF_C4XXX_ENABLE_AE_ECC_PARITY_CORR (BIT(24) | BIT(12))
#define ADF_C4XXX_UERRSSMSH(i) (0x18 + ((i)*0x4000))
#define ADF_C4XXX_UERRSSMSH_INTS_CLEAR_MASK (~BIT(0) ^ BIT(16))
#define ADF_C4XXX_CERRSSMSH(i) (0x10 + ((i)*0x4000))
#define ADF_C4XXX_CERRSSMSH_INTS_CLEAR_MASK (~BIT(0))
#define ADF_C4XXX_ERRSSMSH_EN BIT(3)
#define ADF_C4XXX_PF2VF_OFFSET(i) (0x62400 + ((i)*0x04))
#define ADF_C4XXX_VINTMSK_OFFSET(i) (0x62200 + ((i)*0x04))

/* Doorbell interrupt detection in ERRSOU11 */
#define ADF_C4XXX_DOORBELL_INT_SRC BIT(10)

/* Doorbell interrupt register definitions */
#define ADF_C4XXX_ETH_DOORBELL_INT (0x60108)

/* Clear <3:0> in ETH_DOORBELL_INT */
#define ADF_C4XXX_ETH_DOORBELL_MASK 0xF

/* Doorbell register definitions */
#define ADF_C4XXX_NUM_ETH_DOORBELL_REGS (4)
#define ADF_C4XXX_ETH_DOORBELL(i) (0x61500 + ((i)*0x04))

/* Error source registers */
#define ADF_C4XXX_ERRSOU0 (0x60000 + 0x40)
#define ADF_C4XXX_ERRSOU1 (0x60000 + 0x44)
#define ADF_C4XXX_ERRSOU2 (0x60000 + 0x48)
#define ADF_C4XXX_ERRSOU3 (0x60000 + 0x4C)
#define ADF_C4XXX_ERRSOU4 (0x60000 + 0x50)
#define ADF_C4XXX_ERRSOU5 (0x60000 + 0x54)
#define ADF_C4XXX_ERRSOU6 (0x60000 + 0x58)
#define ADF_C4XXX_ERRSOU7 (0x60000 + 0x5C)
#define ADF_C4XXX_ERRSOU8 (0x60000 + 0x60)
#define ADF_C4XXX_ERRSOU9 (0x60000 + 0x64)
#define ADF_C4XXX_ERRSOU10 (0x60000 + 0x68)
#define ADF_C4XXX_ERRSOU11 (0x60000 + 0x6C)

/* Error source mask registers */
#define ADF_C4XXX_ERRMSK0 (0x60000 + 0xC0)
#define ADF_C4XXX_ERRMSK1 (0x60000 + 0xC4)
#define ADF_C4XXX_ERRMSK2 (0x60000 + 0xC8)
#define ADF_C4XXX_ERRMSK3 (0x60000 + 0xCC)
#define ADF_C4XXX_ERRMSK4 (0x60000 + 0xD0)
#define ADF_C4XXX_ERRMSK5 (0x60000 + 0xD4)
#define ADF_C4XXX_ERRMSK6 (0x60000 + 0xD8)
#define ADF_C4XXX_ERRMSK7 (0x60000 + 0xDC)
#define ADF_C4XXX_ERRMSK8 (0x60000 + 0xE0)
#define ADF_C4XXX_ERRMSK9 (0x60000 + 0xE4)
#define ADF_C4XXX_ERRMSK10 (0x60000 + 0xE8)
#define ADF_C4XXX_ERRMSK11 (0x60000 + 0xEC)

/* Slice Hang enabling related registers  */
#define ADF_C4XXX_SHINTMASKSSM (0x1018)
#define ADF_C4XXX_SSMWDTL (0x54)
#define ADF_C4XXX_SSMWDTH (0x5C)
#define ADF_C4XXX_SSMWDTPKEL (0x58)
#define ADF_C4XXX_SSMWDTPKEH (0x60)
#define ADF_C4XXX_SLICEHANGSTATUS (0x4C)
#define ADF_C4XXX_IASLICEHANGSTATUS (0x50)

#define ADF_C4XXX_SHINTMASKSSM_VAL (0x00)

/* Set default value of Slice Hang watchdogs in clock cycles */
#define ADF_C4XXX_SSM_WDT_64BIT_DEFAULT_VALUE 0x3D0900
#define ADF_C4XXX_SSM_WDT_PKE_64BIT_DEFAULT_VALUE 0x3000000

/* Return interrupt accelerator source mask */
#define ADF_C4XXX_IRQ_SRC_MASK(accel) (1 << (accel))

/* Return address of SHINTMASKSSM register for a given accelerator */
#define ADF_C4XXX_SHINTMASKSSM_OFFSET(accel)                                   \
	(ADF_C4XXX_SHINTMASKSSM + ((accel)*0x4000))

/* Return address of SSMWDTL register for a given accelerator */
#define ADF_C4XXX_SSMWDTL_OFFSET(accel) (ADF_C4XXX_SSMWDTL + ((accel)*0x4000))

/* Return address of SSMWDTH register for a given accelerator */
#define ADF_C4XXX_SSMWDTH_OFFSET(accel) (ADF_C4XXX_SSMWDTH + ((accel)*0x4000))

/* Return address of SSMWDTPKEL register for a given accelerator */
#define ADF_C4XXX_SSMWDTPKEL_OFFSET(accel)                                     \
	(ADF_C4XXX_SSMWDTPKEL + ((accel)*0x4000))

/* Return address of SSMWDTPKEH register for a given accelerator */
#define ADF_C4XXX_SSMWDTPKEH_OFFSET(accel)                                     \
	(ADF_C4XXX_SSMWDTPKEH + ((accel)*0x4000))

/* Return address of SLICEHANGSTATUS register for a given accelerator */
#define ADF_C4XXX_SLICEHANGSTATUS_OFFSET(accel)                                \
	(ADF_C4XXX_SLICEHANGSTATUS + ((accel)*0x4000))

/* Return address of IASLICEHANGSTATUS register for a given accelerator */
#define ADF_C4XXX_IASLICEHANGSTATUS_OFFSET(accel)                              \
	(ADF_C4XXX_IASLICEHANGSTATUS + ((accel)*0x4000))

/* RAS enabling related registers */
#define ADF_C4XXX_SSMFEATREN (0x2010)
#define ADF_C4XXX_SSMSOFTERRORPARITY_MASK (0x1008)

/* Return address of SSMFEATREN register for given accel */
#define ADF_C4XXX_GET_SSMFEATREN_OFFSET(accel)                                 \
	(ADF_C4XXX_SSMFEATREN + ((accel)*0x4000))

/* Return address of SSMSOFTERRORPARITY_MASK register for given accel */
#define ADF_C4XXX_GET_SSMSOFTERRORPARITY_MASK_OFFSET(accel)                    \
	(ADF_C4XXX_SSMSOFTERRORPARITY_MASK + ((accel)*0x4000))

/* RAS enabling related registers values to be written */
#define ADF_C4XXX_SSMFEATREN_VAL (0xFD)
#define ADF_C4XXX_SSMSOFTERRORPARITY_MASK_VAL (0x00)

/* Enable VF2PF interrupt in ERRMSK4 to ERRMSK7 */
#define ADF_C4XXX_VF2PF0_31 0x0
#define ADF_C4XXX_VF2PF32_63 0x0
#define ADF_C4XXX_VF2PF64_95 0x0
#define ADF_C4XXX_VF2PF96_127 0x0

/* AEx Correctable Error Mask in ERRMSK8 */
#define ADF_C4XXX_ERRMSK8_COERR 0x0
#define ADF_C4XXX_ERRSOU8_MECORR_MASK BIT(0)
#define ADF_C4XXX_HI_ME_COR_ERRLOG (0x60104)
#define ADF_C4XXX_HI_ME_COR_ERRLOG_ENABLE (0x61600)
#define ADF_C4XXX_HI_ME_COR_ERRLOG_ENABLE_MASK (0xFFFFFFFF)
#define ADF_C4XXX_HI_ME_COR_ERRLOG_SIZE_IN_BITS (32)

/* Group of registers related to ERRSOU9 handling
 *
 * AEx Uncorrectable Error Mask in ERRMSK9
 * CPP Command Parity Errors Mask in ERRMSK9
 * RI Memory Parity Errors Mask in ERRMSK9
 * TI Memory Parity Errors Mask in ERRMSK9
 */
#define ADF_C4XXX_ERRMSK9_IRQ_MASK 0x0
#define ADF_C4XXX_ME_UNCORR_ERROR BIT(0)
#define ADF_C4XXX_CPP_CMD_PAR_ERR BIT(1)
#define ADF_C4XXX_RI_MEM_PAR_ERR BIT(2)
#define ADF_C4XXX_TI_MEM_PAR_ERR BIT(3)

#define ADF_C4XXX_ERRSOU9_ERROR_MASK                                           \
	(ADF_C4XXX_ME_UNCORR_ERROR | ADF_C4XXX_CPP_CMD_PAR_ERR |               \
	 ADF_C4XXX_RI_MEM_PAR_ERR | ADF_C4XXX_TI_MEM_PAR_ERR)

#define ADF_C4XXX_HI_ME_UNCERR_LOG (0x60100)
#define ADF_C4XXX_HI_ME_UNCERR_LOG_ENABLE (0x61608)
#define ADF_C4XXX_HI_ME_UNCERR_LOG_ENABLE_MASK (0xFFFFFFFF)
#define ADF_C4XXX_HI_ME_UNCOR_ERRLOG_BITS (32)

/* HI CPP Agents Command parity Error Log
 * CSR name: hicppagentcmdparerrlog
 */
#define ADF_C4XXX_HI_CPP_AGENT_CMD_PAR_ERR_LOG (0x6010C)
#define ADF_C4XXX_HI_CPP_AGENT_CMD_PAR_ERR_LOG_ENABLE (0x61604)
#define ADF_C4XXX_HI_CPP_AGENT_CMD_PAR_ERR_LOG_ENABLE_MASK (0xFFFFFFFF)
#define ADF_C4XXX_TI_CMD_PAR_ERR BIT(0)
#define ADF_C4XXX_RI_CMD_PAR_ERR BIT(1)
#define ADF_C4XXX_ICI_CMD_PAR_ERR BIT(2)
#define ADF_C4XXX_ICE_CMD_PAR_ERR BIT(3)
#define ADF_C4XXX_ARAM_CMD_PAR_ERR BIT(4)
#define ADF_C4XXX_CFC_CMD_PAR_ERR BIT(5)
#define ADF_C4XXX_SSM_CMD_PAR_ERR(value) (((u32)(value) >> 6) & 0xFFF)

/* RI Memory Parity Error Status Register
 * CSR name: rimem_parerr_sts
 */
#define ADF_C4XXX_RI_MEM_PAR_ERR_STS (0x61610)
#define ADF_C4XXX_RI_MEM_PAR_ERR_EN0 (0x61614)
#define ADF_C4XXX_RI_MEM_PAR_ERR_FERR (0x61618)
#define ADF_C4XXX_RI_MEM_PAR_ERR_EN0_MASK (0x7FFFFF)
#define ADF_C4XXX_RI_MEM_MSIX_TBL_INT_MASK (BIT(22))
#define ADF_C4XXX_RI_MEM_PAR_ERR_STS_MASK                                      \
	(ADF_C4XXX_RI_MEM_PAR_ERR_EN0_MASK ^ ADF_C4XXX_RI_MEM_MSIX_TBL_INT_MASK)

/* TI Memory Parity Error Status Register
 * CSR name: ti_mem_par_err_sts0, ti_mem_par_err_sts1
 */
#define ADF_C4XXX_TI_MEM_PAR_ERR_STS0 (0x68604)
#define ADF_C4XXX_TI_MEM_PAR_ERR_EN0 (0x68608)
#define ADF_C4XXX_TI_MEM_PAR_ERR_EN0_MASK (0xFFFFFFFF)
#define ADF_C4XXX_TI_MEM_PAR_ERR_STS1 (0x68610)
#define ADF_C4XXX_TI_MEM_PAR_ERR_EN1 (0x68614)
#define ADF_C4XXX_TI_MEM_PAR_ERR_EN1_MASK (0x7FFFF)
#define ADF_C4XXX_TI_MEM_PAR_ERR_STS1_MASK (ADF_C4XXX_TI_MEM_PAR_ERR_EN1_MASK)
#define ADF_C4XXX_TI_MEM_PAR_ERR_FIRST_ERROR (0x68618)

/* Enable SSM<11:0> in ERRMSK10 */
#define ADF_C4XXX_ERRMSK10_SSM_ERR 0x0
#define ADF_C4XXX_ERRSOU10_RAS_MASK 0x1FFF
#define ADF_C4XXX_ERRSOU10_PUSHPULL_MASK BIT(12)

#define ADF_C4XXX_IASTATSSM_UERRSSMSH_MASK BIT(0)
#define ADF_C4XXX_IASTATSSM_CERRSSMSH_MASK BIT(1)
#define ADF_C4XXX_IASTATSSM_UERRSSMMMP0_MASK BIT(2)
#define ADF_C4XXX_IASTATSSM_CERRSSMMMP0_MASK BIT(3)
#define ADF_C4XXX_IASTATSSM_UERRSSMMMP1_MASK BIT(4)
#define ADF_C4XXX_IASTATSSM_CERRSSMMMP1_MASK BIT(5)
#define ADF_C4XXX_IASTATSSM_UERRSSMMMP2_MASK BIT(6)
#define ADF_C4XXX_IASTATSSM_CERRSSMMMP2_MASK BIT(7)
#define ADF_C4XXX_IASTATSSM_UERRSSMMMP3_MASK BIT(8)
#define ADF_C4XXX_IASTATSSM_CERRSSMMMP3_MASK BIT(9)
#define ADF_C4XXX_IASTATSSM_UERRSSMMMP4_MASK BIT(10)
#define ADF_C4XXX_IASTATSSM_CERRSSMMMP4_MASK BIT(11)
#define ADF_C4XXX_IASTATSSM_PPERR_MASK BIT(12)
#define ADF_C4XXX_IASTATSSM_SPPPAR_ERR_MASK BIT(14)
#define ADF_C4XXX_IASTATSSM_CPPPAR_ERR_MASK BIT(15)
#define ADF_C4XXX_IASTATSSM_RFPAR_ERR_MASK BIT(16)

#define ADF_C4XXX_IAINTSTATSSM(i) ((i)*0x4000 + 0x206C)
#define ADF_C4XXX_IASTATSSM_MASK 0x1DFFF
#define ADF_C4XXX_IASTATSSM_CLR_MASK 0xFFFE2000
#define ADF_C4XXX_IASTATSSM_BITS 17
#define ADF_C4XXX_IASTATSSM_SLICE_HANG_ERR_BIT 13
#define ADF_C4XXX_IASTATSSM_SPP_PAR_ERR_BIT 14
#define ADF_C4XXX_IASTATSSM_CPP_PAR_ERR_BIT 15

/* Accelerator Interrupt Mask (SSM)
 * CSR name: intmaskssm[0..11]
 * Returns address of INTMASKSSM register for a given accel.
 * This register is used to unmask SSM interrupts to host
 * reported by ERRSOU10.
 */
#define ADF_C4XXX_GET_INTMASKSSM_OFFSET(accel) ((accel)*0x4000)

/* Base address of SPP parity error mask register
 * CSR name: sppparerrmsk[0..11]
 */
#define ADF_C4XXX_SPPPARERRMSK_OFFSET (0x2028)

/* Returns address of SPPPARERRMSK register for a given accel.
 * This register is used to unmask SPP parity errors interrupts to host
 * reported by ERRSOU10.
 */
#define ADF_C4XXX_GET_SPPPARERRMSK_OFFSET(accel)                               \
	(ADF_C4XXX_SPPPARERRMSK_OFFSET + ((accel)*0x4000))

#define ADF_C4XXX_EXPRPSSMCPR0(i) ((i)*0x4000 + 0x400)
#define ADF_C4XXX_EXPRPSSMXLT0(i) ((i)*0x4000 + 0x500)
#define ADF_C4XXX_EXPRPSSMCPR1(i) ((i)*0x4000 + 0x1400)
#define ADF_C4XXX_EXPRPSSMXLT1(i) ((i)*0x4000 + 0x1500)

#define ADF_C4XXX_EXPRPSSM_FATAL_MASK BIT(2)
#define ADF_C4XXX_EXPRPSSM_SOFT_MASK BIT(3)

#define ADF_C4XXX_PPERR_INTS_CLEAR_MASK BIT(0)

#define ADF_C4XXX_SSMSOFTERRORPARITY(i) ((i)*0x4000 + 0x1000)
#define ADF_C4XXX_SSMCPPERR(i) ((i)*0x4000 + 0x2030)

/* ethernet doorbell in ERRMSK11
 * timisc in ERRMSK11
 * rimisc in ERRMSK11
 * ppmiscerr in ERRMSK11
 * cerr in ERRMSK11
 * uerr in ERRMSK11
 * ici in ERRMSK11
 * ice in ERRMSK11
 */
#define ADF_C4XXX_ERRMSK11_ERR 0x0
/*
 * BIT(7) disables ICI interrupt
 * BIT(8) disables ICE interrupt
 */
#define ADF_C4XXX_ERRMSK11_ERR_DISABLE_ICI_ICE_INTR (BIT(7) | BIT(8))

/* RAS mask for errors reported by ERRSOU11 */
#define ADF_C4XXX_ERRSOU11_ERROR_MASK (0x1FF)
#define ADF_C4XXX_TI_MISC BIT(0)
#define ADF_C4XXX_RI_PUSH_PULL_PAR_ERR BIT(1)
#define ADF_C4XXX_TI_PUSH_PULL_PAR_ERR BIT(2)
#define ADF_C4XXX_ARAM_CORR_ERR BIT(3)
#define ADF_C4XXX_ARAM_UNCORR_ERR BIT(4)
#define ADF_C4XXX_TI_PULL_PAR_ERR BIT(5)
#define ADF_C4XXX_RI_PUSH_PAR_ERR BIT(6)
#define ADF_C4XXX_INLINE_INGRESS_INTR BIT(7)
#define ADF_C4XXX_INLINE_EGRESS_INTR BIT(8)

/* TI Misc error status */
#define ADF_C4XXX_TI_MISC_STS (0x6854C)
#define ADF_C4XXX_TI_MISC_ERR_MASK (BIT(0))
#define ADF_C4XXX_GET_TI_MISC_ERR_TYPE(status) ((status) >> 1 & 0x3)
#define ADF_C4XXX_TI_BME_RESP_ORDER_ERR (0x1)
#define ADF_C4XXX_TI_RESP_ORDER_ERR (0x2)

/* RI CPP interface status register */
#define ADF_C4XXX_RI_CPP_INT_STS (0x61118)
#define ADF_C4XXX_RI_CPP_INT_STS_PUSH_ERR BIT(0)
#define ADF_C4XXX_RI_CPP_INT_STS_PULL_ERR BIT(1)
#define ADF_C4XXX_RI_CPP_INT_STS_PUSH_DATA_PAR_ERR BIT(2)
#define ADF_C4XXX_GET_CPP_BUS_FROM_STS(status) ((status) >> 31 & 0x1)

/* RI CPP interface control register. */
#define ADF_C4XXX_RICPPINTCTL (0x61000 + 0x004)
/*
 * BIT(3) enables error parity checking on CPP.
 * BIT(2) enables error detection and reporting on the RI Parity Error.
 * BIT(1) enables error detection and reporting on the RI CPP Pull interface.
 * BIT(0) enables error detection and reporting on the RI CPP Push interface.
 */
#define ADF_C4XXX_RICPP_EN (BIT(3) | BIT(2) | BIT(1) | BIT(0))

/* TI CPP interface status register */
#define ADF_C4XXX_TI_CPP_INT_STS (0x6853C)
#define ADF_C4XXX_TI_CPP_INT_STS_PUSH_ERR BIT(0)
#define ADF_C4XXX_TI_CPP_INT_STS_PULL_ERR BIT(1)
#define ADF_C4XXX_TI_CPP_INT_STS_PUSH_DATA_PAR_ERR BIT(2)

#define ADF_C4XXX_TICPPINTCTL (0x68000 + 0x538)
/*
 * BIT(4) enables 'stop and scream' feature for TI RF.
 * BIT(3) enables CPP command and pull data parity checking.
 * BIT(2) enables data parity error detection and reporting on the TI CPP
 *        Pull interface.
 * BIT(1) enables error detection and reporting on the TI CPP Pull interface.
 * BIT(0) enables error detection and reporting on the TI CPP Push interface.
 */
#define ADF_C4XXX_TICPP_EN (BIT(4) | BIT(3) | BIT(2) | BIT(1) | BIT(0))

/* CPP error control and logging register */
#define ADF_C4XXX_CPP_CFC_ERR_CTRL (0x70000 + 0xC00)

/*
 * BIT(1) enables generation of irqs to the PCIe endpoint
 *        for the errors specified in CPP_CFC_ERR_STATUS
 * BIT(0) enables detecting and logging of push/pull data errors.
 */
#define ADF_C4XXX_CPP_CFC_UE (BIT(1) | BIT(0))

/* ARAM error interrupt enable registers */
#define ADF_C4XXX_ARAMCERR (0x101700)
#define ADF_C4XXX_ARAMUERR (0x101704)
#define ADF_C4XXX_CPPMEMTGTERR (0x101710)
#define ADF_C4XXX_ARAM_CORR_ERR_MASK (BIT(0))
#define ADF_C4XXX_ARAM_UNCORR_ERR_MASK (BIT(0))
#define ADF_C4XXX_CLEAR_CSR_BIT(csr, bit_num) ((csr) &= ~(BIT(bit_num)))

/* ARAM correctable errors defined in ARAMCERR
 * Bit<3> Enable fixing and logging correctable errors by hardware.
 * Bit<26> Enable interrupt to host for ARAM correctable errors.
 */
#define ADF_C4XXX_ARAM_CERR (BIT(3) | BIT(26))

/* ARAM correctable errors defined in ARAMUERR
 * Bit<3> Enable detection and logging of ARAM uncorrectable errors.
 * Bit<19> Enable interrupt to host for ARAM uncorrectable errors.
 */
#define ADF_C4XXX_ARAM_UERR (BIT(3) | BIT(19))

/* Misc memory target error registers in CPPMEMTGTERR
 * Bit<2> CPP memory push/pull error enable bit
 * Bit<7> RI push/pull error enable bit
 * Bit<8> ARAM pull data parity check bit
 * Bit<9> RAS push error enable bit
 */
#define ADF_C4XXX_TGT_UERR (BIT(9) | BIT(8) | BIT(7) | BIT(2))

/* Slice power down register */
#define ADF_C4XXX_SLICEPWRDOWN(i) (((i)*0x4000) + 0x2C)

/* Enabling PKE0 to PKE4. */
#define ADF_C4XXX_MMP_PWR_UP_MSK                                               \
	(BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16))

/* Error registers for MMP0-MMP4. */
#define ADF_C4XXX_MAX_MMP (5)

#define ADF_C4XXX_MMP_BASE(i) ((i)*0x1000 % 0x3800)
#define ADF_C4XXX_CERRSSMMMP(i, n) ((i)*0x4000 + ADF_C4XXX_MMP_BASE(n) + 0x380)
#define ADF_C4XXX_UERRSSMMMP(i, n) ((i)*0x4000 + ADF_C4XXX_MMP_BASE(n) + 0x388)
#define ADF_C4XXX_UERRSSMMMPAD(i, n)                                           \
	((i)*0x4000 + ADF_C4XXX_MMP_BASE(n) + 0x38C)
#define ADF_C4XXX_INTMASKSSM(i) ((i)*0x4000 + 0x0)

#define ADF_C4XXX_UERRSSMMMP_INTS_CLEAR_MASK ((BIT(16) | BIT(0)))
#define ADF_C4XXX_CERRSSMMMP_INTS_CLEAR_MASK BIT(0)

/* Bit<3> enables logging of MMP uncorrectable errors */
#define ADF_C4XXX_UERRSSMMMP_EN BIT(3)

/* Bit<3> enables logging of MMP correctable errors */
#define ADF_C4XXX_CERRSSMMMP_EN BIT(3)

#define ADF_C4XXX_ERRMSK_VF2PF_OFFSET(i) (ADF_C4XXX_ERRMSK4 + ((i)*0x04))

/* RAM base address registers */
#define ADF_C4XXX_RAMBASEADDRHI 0x71020

#define ADF_C4XXX_NUM_ARAM_ENTRIES 8

/* ARAM region sizes in bytes */
#define ADF_C4XXX_1MB_SIZE (1024 * 1024)
#define ADF_C4XXX_2MB_ARAM_SIZE (2 * ADF_C4XXX_1MB_SIZE)
#define ADF_C4XXX_4MB_ARAM_SIZE (4 * ADF_C4XXX_1MB_SIZE)
#define ADF_C4XXX_DEFAULT_MMP_REGION_SIZE (1024 * 256)
#define ADF_C4XXX_DEFAULT_SKM_REGION_SIZE (1024 * 256)
#define ADF_C4XXX_AU_COMPR_INTERM_SIZE (1024 * 128 * 2 * 2)
#define ADF_C4XXX_DEF_ASYM_MASK 0x1

/* Arbiter configuration */
#define ADF_C4XXX_ARB_OFFSET 0x80000
#define ADF_C4XXX_ARB_WQCFG_OFFSET 0x200

/* Admin Interface Reg Offset */
#define ADF_C4XXX_ADMINMSGUR_OFFSET (0x60000 + 0x8000 + 0x400 + 0x174)
#define ADF_C4XXX_ADMINMSGLR_OFFSET (0x60000 + 0x8000 + 0x400 + 0x178)
#define ADF_C4XXX_MAILBOX_BASE_OFFSET 0x40970

/* AE to function mapping */
#define ADF_C4XXX_AE2FUNC_REG_PER_AE 8
#define ADF_C4XXX_AE2FUNC_MAP_OFFSET 0x68800
#define ADF_C4XXX_AE2FUNC_MAP_REG_SIZE 4
#define ADF_C4XXX_AE2FUNC_MAP_VALID BIT(8)

/* Enable each of the units on the chip */
#define ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC 0x7096C
#define ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_DISABLE_ALL 0x0
#define ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_ICE_ENABLE BIT(4)
#define ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_ICI_ENABLE BIT(3)
#define ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_ARAM BIT(2)

/* Clock is fully sets up after some delay */
#define ADF_C4XXX_GLOBAL_CLK_ENABLE_GENERIC_RESTART_DELAY 10
#define ADF_C4XXX_GLOBAL_CLK_RESTART_LOOP 10

/* Reset each of the PPC units on the chip  */
#define ADF_C4XXX_IXP_RESET_GENERIC 0x70940
#define ADF_C4XXX_IXP_RESET_GENERIC_OUT_OF_RESET_TRIGGER 0x0
#define ADF_C4XXX_IXP_RESET_GENERIC_INLINE_INGRESS BIT(4)
#define ADF_C4XXX_IXP_RESET_GENERIC_INLINE_EGRESS BIT(3)
#define ADF_C4XXX_IXP_RESET_GENERIC_ARAM BIT(2)

/* Default accel unit configuration */
#define ADF_C4XXX_NUM_CY_AU                                                    \
	{                                                                      \
		[DEV_SKU_1] = 4, [DEV_SKU_1_CY] = 6, [DEV_SKU_2] = 3,          \
		[DEV_SKU_2_CY] = 4, [DEV_SKU_3] = 1, [DEV_SKU_3_CY] = 2,       \
		[DEV_SKU_UNKNOWN] = 0                                          \
	}
#define ADF_C4XXX_NUM_DC_AU                                                    \
	{                                                                      \
		[DEV_SKU_1] = 2, [DEV_SKU_1_CY] = 0, [DEV_SKU_2] = 1,          \
		[DEV_SKU_2_CY] = 0, [DEV_SKU_3] = 1, [DEV_SKU_3_CY] = 0,       \
		[DEV_SKU_UNKNOWN] = 0                                          \
	}

#define ADF_C4XXX_NUM_ACCEL_PER_AU 2
#define ADF_C4XXX_NUM_INLINE_AU                                                \
	{                                                                      \
		[DEV_SKU_1] = 0, [DEV_SKU_1_CY] = 0, [DEV_SKU_2] = 0,          \
		[DEV_SKU_2_CY] = 0, [DEV_SKU_3] = 0, [DEV_SKU_3_CY] = 0,       \
		[DEV_SKU_UNKNOWN] = 0                                          \
	}
#define ADF_C4XXX_6_AE 6
#define ADF_C4XXX_4_AE 4
#define ADF_C4XXX_100 100
#define ADF_C4XXX_ROUND_LIMIT 5
#define ADF_C4XXX_PERCENTAGE "%"

#define ADF_C4XXX_ARB_CY 0x12222222
#define ADF_C4XXX_ARB_DC 0x00000888

/* Default accel firmware maximal object*/
#define ADF_C4XXX_MAX_OBJ 4

/* Default 4 partitions for services */
#define ADF_C4XXX_PART_ASYM 0
#define ADF_C4XXX_PART_SYM 1
#define ADF_C4XXX_PART_UNUSED 2
#define ADF_C4XXX_PART_DC 3
#define ADF_C4XXX_PARTS_PER_GRP 16

#define ADF_C4XXX_PARTITION_LUT_OFFSET 0x81000
#define ADF_C4XXX_WRKTHD2PARTMAP 0x82000
#define ADF_C4XXX_WQM_SIZE 0x4

#define ADF_C4XXX_DEFAULT_PARTITIONS                                           \
	(ADF_C4XXX_PART_ASYM | ADF_C4XXX_PART_SYM << 8 |                       \
	 ADF_C4XXX_PART_UNUSED << 16 | ADF_C4XXX_PART_DC << 24)

/* SKU configurations */
#define ADF_C4XXX_HIGH_SKU_AES 32
#define ADF_C4XXX_MED_SKU_AES 24
#define ADF_C4XXX_LOW_SKU_AES 12

#define READ_CSR_WQM(csr_addr, csr_offset, index)                              \
	ADF_CSR_RD(csr_addr, (csr_offset) + ((index)*ADF_C4XXX_WQM_SIZE))

#define WRITE_CSR_WQM(csr_addr, csr_offset, index, value)                      \
	ADF_CSR_WR(csr_addr, (csr_offset) + ((index)*ADF_C4XXX_WQM_SIZE), value)

/* Firmware Binary */
#define ADF_C4XXX_FW "qat_c4xxx_fw"
#define ADF_C4XXX_MMP "qat_c4xxx_mmp_fw"
#define ADF_C4XXX_INLINE_OBJ "qat_c4xxx_inline.bin"
#define ADF_C4XXX_DC_OBJ "qat_c4xxx_dc.bin"
#define ADF_C4XXX_CY_OBJ "qat_c4xxx_cy.bin"
#define ADF_C4XXX_SYM_OBJ "qat_c4xxx_sym.bin"

void adf_init_hw_data_c4xxx(struct adf_hw_device_data *hw_data);
void adf_clean_hw_data_c4xxx(struct adf_hw_device_data *hw_data);
int adf_init_arb_c4xxx(struct adf_accel_dev *accel_dev);
void adf_exit_arb_c4xxx(struct adf_accel_dev *accel_dev);

#define ADF_C4XXX_AE_FREQ (800 * 1000000)
#define ADF_C4XXX_MIN_AE_FREQ (571 * 1000000)
#define ADF_C4XXX_MAX_AE_FREQ (800 * 1000000)

int c4xxx_init_ae_config(struct adf_accel_dev *accel_dev);
void c4xxx_exit_ae_config(struct adf_accel_dev *accel_dev);
void remove_oid(struct adf_accel_dev *accel_dev, struct sysctl_oid *oid);
#endif
