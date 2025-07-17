/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef ADF_200XX_HW_DATA_H_
#define ADF_200XX_HW_DATA_H_

/* PCIe configuration space */
#define ADF_200XX_PMISC_BAR 1
#define ADF_200XX_ETR_BAR 2
#define ADF_200XX_RX_RINGS_OFFSET 8
#define ADF_200XX_TX_RINGS_MASK 0xFF
#define ADF_200XX_MAX_ACCELERATORS 3
#define ADF_200XX_MAX_ACCELENGINES 6
#define ADF_200XX_ACCELERATORS_REG_OFFSET 16
#define ADF_200XX_ACCELERATORS_MASK 0x7
#define ADF_200XX_ACCELENGINES_MASK 0x3F
#define ADF_200XX_ETR_MAX_BANKS 16
#define ADF_200XX_SMIAPF0_MASK_OFFSET (0x3A000 + 0x28)
#define ADF_200XX_SMIAPF1_MASK_OFFSET (0x3A000 + 0x30)
#define ADF_200XX_SMIA0_MASK 0xFFFF
#define ADF_200XX_SMIA1_MASK 0x1
#define ADF_200XX_SOFTSTRAP_CSR_OFFSET 0x2EC
#define ADF_200XX_POWERGATE_PKE BIT(24)
#define ADF_200XX_POWERGATE_CY BIT(23)

#define ADF_200XX_PFIEERRUNCSTSR 0x280

/* Error detection and correction */
#define ADF_200XX_AE_CTX_ENABLES(i) ((i)*0x1000 + 0x20818)
#define ADF_200XX_AE_MISC_CONTROL(i) ((i)*0x1000 + 0x20960)
#define ADF_200XX_ENABLE_AE_ECC_ERR BIT(28)
#define ADF_200XX_ENABLE_AE_ECC_PARITY_CORR (BIT(24) | BIT(12))
#define ADF_200XX_UERRSSMSH(i) (i * 0x4000 + 0x18)
#define ADF_200XX_CERRSSMSH(i) (i * 0x4000 + 0x10)
#define ADF_200XX_ERRSSMSH_EN BIT(3)
#define ADF_200XX_ERRSOU3 (0x3A000 + 0x0C)
#define ADF_200XX_ERRSOU5 (0x3A000 + 0xD8)

/* BIT(2) enables the logging of push/pull data errors. */
#define ADF_200XX_PPERR_EN (BIT(2))

/* Mask for VF2PF interrupts */
#define ADF_200XX_VF2PF1_16 (0xFFFF << 9)
#define ADF_200XX_ERRSOU3_VF2PF(errsou3) (((errsou3)&0x01FFFE00) >> 9)
#define ADF_200XX_ERRMSK3_VF2PF(vf_mask) (((vf_mask)&0xFFFF) << 9)

/* Masks for correctable error interrupts. */
#define ADF_200XX_ERRMSK0_CERR (BIT(24) | BIT(16) | BIT(8) | BIT(0))
#define ADF_200XX_ERRMSK1_CERR (BIT(8) | BIT(0))
#define ADF_200XX_ERRMSK5_CERR (0)

/* Masks for uncorrectable error interrupts. */
#define ADF_200XX_ERRMSK0_UERR (BIT(25) | BIT(17) | BIT(9) | BIT(1))
#define ADF_200XX_ERRMSK1_UERR (BIT(9) | BIT(1))
#define ADF_200XX_ERRMSK3_UERR                                                 \
	(BIT(6) | BIT(5) | BIT(4) | BIT(3) | BIT(2) | BIT(0))
#define ADF_200XX_ERRMSK5_UERR (BIT(16))

/* RI CPP control */
#define ADF_200XX_RICPPINTCTL (0x3A000 + 0x110)
/*
 * BIT(2) enables error detection and reporting on the RI Parity Error.
 * BIT(1) enables error detection and reporting on the RI CPP Pull interface.
 * BIT(0) enables error detection and reporting on the RI CPP Push interface.
 */
#define ADF_200XX_RICPP_EN (BIT(2) | BIT(1) | BIT(0))

/* TI CPP control */
#define ADF_200XX_TICPPINTCTL (0x3A400 + 0x138)
/*
 * BIT(3) enables error detection and reporting on the ETR Parity Error.
 * BIT(2) enables error detection and reporting on the TI Parity Error.
 * BIT(1) enables error detection and reporting on the TI CPP Pull interface.
 * BIT(0) enables error detection and reporting on the TI CPP Push interface.
 */
#define ADF_200XX_TICPP_EN (BIT(3) | BIT(2) | BIT(1) | BIT(0))

/* CFC Uncorrectable Errors */
#define ADF_200XX_CPP_CFC_ERR_CTRL (0x30000 + 0xC00)
/*
 * BIT(1) enables interrupt.
 * BIT(0) enables detecting and logging of push/pull data errors.
 */
#define ADF_200XX_CPP_CFC_UE (BIT(1) | BIT(0))

#define ADF_200XX_SLICEPWRDOWN(i) ((i)*0x4000 + 0x2C)
/* Enabling PKE4-PKE0. */
#define ADF_200XX_MMP_PWR_UP_MSK                                               \
	(BIT(20) | BIT(19) | BIT(18) | BIT(17) | BIT(16))

/* CPM Uncorrectable Errors */
#define ADF_200XX_INTMASKSSM(i) ((i)*0x4000 + 0x0)
/* Disabling interrupts for correctable errors. */
#define ADF_200XX_INTMASKSSM_UERR                                              \
	(BIT(11) | BIT(9) | BIT(7) | BIT(5) | BIT(3) | BIT(1))

/* MMP */
/* BIT(3) enables correction. */
#define ADF_200XX_CERRSSMMMP_EN (BIT(3))

/* BIT(3) enables logging. */
#define ADF_200XX_UERRSSMMMP_EN (BIT(3))

#define ADF_200XX_PF2VF_OFFSET(i) (0x3A000 + 0x280 + ((i)*0x04))
#define ADF_200XX_VINTMSK_OFFSET(i) (0x3A000 + 0x200 + ((i)*0x04))

/* Arbiter configuration */
#define ADF_200XX_ARB_OFFSET 0x30000
#define ADF_200XX_ARB_WRK_2_SER_MAP_OFFSET 0x180
#define ADF_200XX_ARB_WQCFG_OFFSET 0x100

/* Admin Interface Reg Offset */
#define ADF_200XX_ADMINMSGUR_OFFSET (0x3A000 + 0x574)
#define ADF_200XX_ADMINMSGLR_OFFSET (0x3A000 + 0x578)
#define ADF_200XX_MAILBOX_BASE_OFFSET 0x20970

/* Firmware Binary */
#define ADF_200XX_FW "qat_200xx_fw"
#define ADF_200XX_MMP "qat_200xx_mmp_fw"

void adf_init_hw_data_200xx(struct adf_hw_device_data *hw_data);
void adf_clean_hw_data_200xx(struct adf_hw_device_data *hw_data);

#define ADF_200XX_AE_FREQ (685 * 1000000)
#define ADF_200XX_MIN_AE_FREQ (333 * 1000000)
#define ADF_200XX_MAX_AE_FREQ (685 * 1000000)

#endif
