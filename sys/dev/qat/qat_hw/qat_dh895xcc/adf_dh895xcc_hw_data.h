/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_DH895x_HW_DATA_H_
#define ADF_DH895x_HW_DATA_H_

/* PCIe configuration space */
#define ADF_DH895XCC_SRAM_BAR 0
#define ADF_DH895XCC_PMISC_BAR 1
#define ADF_DH895XCC_ETR_BAR 2
#define ADF_DH895XCC_RX_RINGS_OFFSET 8
#define ADF_DH895XCC_TX_RINGS_MASK 0xFF
#define ADF_DH895XCC_FUSECTL_SKU_MASK 0x300000
#define ADF_DH895XCC_FUSECTL_SKU_SHIFT 20
#define ADF_DH895XCC_FUSECTL_SKU_1 0x0
#define ADF_DH895XCC_FUSECTL_SKU_2 0x1
#define ADF_DH895XCC_FUSECTL_SKU_3 0x2
#define ADF_DH895XCC_FUSECTL_SKU_4 0x3
#define ADF_DH895XCC_MAX_ACCELERATORS 6
#define ADF_DH895XCC_MAX_ACCELENGINES 12
#define ADF_DH895XCC_ACCELERATORS_REG_OFFSET 13
#define ADF_DH895XCC_ACCELERATORS_MASK 0x3F
#define ADF_DH895XCC_ACCELENGINES_MASK 0xFFF
#define ADF_DH895XCC_ETR_MAX_BANKS 32
#define ADF_DH895XCC_SMIAPF0_MASK_OFFSET (0x3A000 + 0x28)
#define ADF_DH895XCC_SMIAPF1_MASK_OFFSET (0x3A000 + 0x30)
#define ADF_DH895XCC_SMIA0_MASK 0xFFFFFFFF
#define ADF_DH895XCC_SMIA1_MASK 0x1
/* Error detection and correction */
#define ADF_DH895XCC_AE_CTX_ENABLES(i) (i * 0x1000 + 0x20818)
#define ADF_DH895XCC_AE_MISC_CONTROL(i) (i * 0x1000 + 0x20960)
#define ADF_DH895XCC_ENABLE_AE_ECC_ERR BIT(28)
#define ADF_DH895XCC_ENABLE_AE_ECC_PARITY_CORR (BIT(24) | BIT(12))
#define ADF_DH895XCC_UERRSSMSH(i) (i * 0x4000 + 0x18)
#define ADF_DH895XCC_CERRSSMSH(i) (i * 0x4000 + 0x10)
#define ADF_DH895XCC_ERRSSMSH_EN BIT(3)
#define ADF_DH895XCC_ERRSOU3 (0x3A000 + 0x0C)
#define ADF_DH895XCC_ERRSOU5 (0x3A000 + 0xD8)
/* BIT(2) enables the logging of push/pull data errors. */
#define ADF_DH895XCC_PPERR_EN (BIT(2))

/* Masks for VF2PF interrupts */
#define ADF_DH895XCC_VF2PF1_16 (0xFFFF << 9)
#define ADF_DH895XCC_VF2PF17_32 (0xFFFF)
#define ADF_DH895XCC_ERRSOU3_VF2PF_L(errsou3) (((errsou3)&0x01FFFE00) >> 9)
#define ADF_DH895XCC_ERRSOU5_VF2PF_U(errsou5) (((errsou5)&0x0000FFFF) << 16)
#define ADF_DH895XCC_ERRMSK3_VF2PF_L(vf_mask) (((vf_mask)&0xFFFF) << 9)
#define ADF_DH895XCC_ERRMSK5_VF2PF_U(vf_mask) ((vf_mask) >> 16)

/* Masks for correctable error interrupts. */
#define ADF_DH895XCC_ERRMSK0_CERR (BIT(24) | BIT(16) | BIT(8) | BIT(0))
#define ADF_DH895XCC_ERRMSK1_CERR (BIT(24) | BIT(16) | BIT(8) | BIT(0))
#define ADF_DH895XCC_ERRMSK3_CERR (BIT(7))
#define ADF_DH895XCC_ERRMSK4_CERR (BIT(24) | BIT(16) | BIT(8) | BIT(0))
#define ADF_DH895XCC_ERRMSK5_CERR (0)

/* Masks for uncorrectable error interrupts. */
#define ADF_DH895XCC_ERRMSK0_UERR (BIT(25) | BIT(17) | BIT(9) | BIT(1))
#define ADF_DH895XCC_ERRMSK1_UERR (BIT(25) | BIT(17) | BIT(9) | BIT(1))
#define ADF_DH895XCC_ERRMSK3_UERR                                              \
	(BIT(8) | BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(0))
#define ADF_DH895XCC_ERRMSK4_UERR (BIT(25) | BIT(17) | BIT(9) | BIT(1))
#define ADF_DH895XCC_ERRMSK5_UERR (BIT(19) | BIT(18) | BIT(17) | BIT(16))

/* RI CPP control */
#define ADF_DH895XCC_RICPPINTCTL (0x3A000 + 0x110)
/*
 * BIT(1) enables error detection and reporting on the RI CPP Pull interface.
 * BIT(0) enables error detection and reporting on the RI CPP Push interface.
 */
#define ADF_DH895XCC_RICPP_EN (BIT(1) | BIT(0))

/* TI CPP control */
#define ADF_DH895XCC_TICPPINTCTL (0x3A400 + 0x138)
/*
 * BIT(1) enables error detection and reporting on the TI CPP Pull interface.
 * BIT(0) enables error detection and reporting on the TI CPP Push interface.
 */
#define ADF_DH895XCC_TICPP_EN (BIT(1) | BIT(0))

/* CFC Uncorrectable Errors */
#define ADF_DH895XCC_CPP_SHAC_ERR_CTRL (0x30000 + 0xC00)
/*
 * BIT(1) enables interrupt.
 * BIT(0) enables detecting and logging of push/pull data errors.
 */
#define ADF_DH895XCC_CPP_SHAC_UE (BIT(1) | BIT(0))

/* Correctable SecureRAM Error Reg */
#define ADF_DH895XCC_ESRAMCERR (0x3AC00 + 0x00)
/* BIT(3) enables fixing and logging of correctable errors. */
#define ADF_DH895XCC_ESRAM_CERR (BIT(3))

/* Uncorrectable SecureRAM Error Reg */
#define ADF_DH895XCC_ESRAMUERR (ADF_SECRAMUERR)
/*
 * BIT(17) enables interrupt.
 * BIT(3) enables detecting and logging of uncorrectable errors.
 */
#define ADF_DH895XCC_ESRAM_UERR (BIT(17) | BIT(3))

/* Miscellaneous Memory Target Errors Register */
/*
 * BIT(3) enables detecting and logging push/pull data errors.
 * BIT(2) enables interrupt.
 */
#define ADF_DH895XCC_TGT_UERR (BIT(3) | BIT(2))

#define ADF_DH895XCC_SLICEPWRDOWN(i) ((i)*0x4000 + 0x2C)
/* Enabling PKE4-PKE0. */
#define ADF_DH895XCC_MMP_PWR_UP_MSK (BIT(7) | BIT(6) | BIT(5) | BIT(4) | BIT(3))

/* CPM Uncorrectable Errors */
#define ADF_DH895XCC_INTMASKSSM(i) ((i)*0x4000 + 0x0)
/* Disabling interrupts for correctable errors. */
#define ADF_DH895XCC_INTMASKSSM_UERR                                           \
	(BIT(11) | BIT(9) | BIT(7) | BIT(5) | BIT(3) | BIT(1))

/* MMP */
/* BIT(3) enables correction. */
#define ADF_DH895XCC_CERRSSMMMP_EN (BIT(3))

/* BIT(3) enables logging. */
#define ADF_DH895XCC_UERRSSMMMP_EN (BIT(3))

#define ADF_DH895XCC_PF2VF_OFFSET(i) (0x3A000 + 0x280 + ((i)*0x04))
#define ADF_DH895XCC_VINTMSK_OFFSET(i) (0x3A000 + 0x200 + ((i)*0x04))

/* Arbiter configuration */
#define ADF_DH895XCC_ARB_OFFSET 0x30000
#define ADF_DH895XCC_ARB_WRK_2_SER_MAP_OFFSET 0x180
#define ADF_DH895XCC_ARB_WQCFG_OFFSET 0x100

/* Admin Interface Reg Offset */
#define ADF_DH895XCC_ADMINMSGUR_OFFSET (0x3A000 + 0x574)
#define ADF_DH895XCC_ADMINMSGLR_OFFSET (0x3A000 + 0x578)
#define ADF_DH895XCC_MAILBOX_BASE_OFFSET 0x20970

/* FW names */
#define ADF_DH895XCC_FW "qat_dh895xcc_fw"
#define ADF_DH895XCC_MMP "qat_dh895xcc_mmp_fw"

void adf_init_hw_data_dh895xcc(struct adf_hw_device_data *hw_data);
void adf_clean_hw_data_dh895xcc(struct adf_hw_device_data *hw_data);
#define ADF_DH895XCC_AE_FREQ (933 * 1000000)
#endif
