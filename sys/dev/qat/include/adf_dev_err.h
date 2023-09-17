/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
#ifndef ADF_DEV_ERR_H_
#define ADF_DEV_ERR_H_

#include <sys/types.h>
#include <dev/pci/pcivar.h>
#include "adf_accel_devices.h"

#define ADF_ERRSOU0 (0x3A000 + 0x00)
#define ADF_ERRSOU1 (0x3A000 + 0x04)
#define ADF_ERRSOU2 (0x3A000 + 0x08)
#define ADF_ERRSOU3 (0x3A000 + 0x0C)
#define ADF_ERRSOU4 (0x3A000 + 0xD0)
#define ADF_ERRSOU5 (0x3A000 + 0xD8)
#define ADF_ERRMSK0 (0x3A000 + 0x10)
#define ADF_ERRMSK1 (0x3A000 + 0x14)
#define ADF_ERRMSK2 (0x3A000 + 0x18)
#define ADF_ERRMSK3 (0x3A000 + 0x1C)
#define ADF_ERRMSK4 (0x3A000 + 0xD4)
#define ADF_ERRMSK5 (0x3A000 + 0xDC)
#define ADF_EMSK3_CPM0_MASK BIT(2)
#define ADF_EMSK3_CPM1_MASK BIT(3)
#define ADF_EMSK5_CPM2_MASK BIT(16)
#define ADF_EMSK5_CPM3_MASK BIT(17)
#define ADF_EMSK5_CPM4_MASK BIT(18)
#define ADF_RICPPINTSTS (0x3A000 + 0x114)
#define ADF_RIERRPUSHID (0x3A000 + 0x118)
#define ADF_RIERRPULLID (0x3A000 + 0x11C)
#define ADF_CPP_CFC_ERR_STATUS (0x30000 + 0xC04)
#define ADF_CPP_CFC_ERR_PPID (0x30000 + 0xC08)
#define ADF_TICPPINTSTS (0x3A400 + 0x13C)
#define ADF_TIERRPUSHID (0x3A400 + 0x140)
#define ADF_TIERRPULLID (0x3A400 + 0x144)
#define ADF_SECRAMUERR (0x3AC00 + 0x04)
#define ADF_SECRAMUERRAD (0x3AC00 + 0x0C)
#define ADF_CPPMEMTGTERR (0x3AC00 + 0x10)
#define ADF_ERRPPID (0x3AC00 + 0x14)
#define ADF_INTSTATSSM(i) ((i)*0x4000 + 0x04)
#define ADF_INTSTATSSM_SHANGERR BIT(13)
#define ADF_PPERR(i) ((i)*0x4000 + 0x08)
#define ADF_PPERRID(i) ((i)*0x4000 + 0x0C)
#define ADF_CERRSSMSH(i) ((i)*0x4000 + 0x10)
#define ADF_UERRSSMSH(i) ((i)*0x4000 + 0x18)
#define ADF_UERRSSMSHAD(i) ((i)*0x4000 + 0x1C)
#define ADF_SLICEHANGSTATUS(i) ((i)*0x4000 + 0x4C)
#define ADF_SLICE_HANG_AUTH0_MASK BIT(0)
#define ADF_SLICE_HANG_AUTH1_MASK BIT(1)
#define ADF_SLICE_HANG_AUTH2_MASK BIT(2)
#define ADF_SLICE_HANG_CPHR0_MASK BIT(4)
#define ADF_SLICE_HANG_CPHR1_MASK BIT(5)
#define ADF_SLICE_HANG_CPHR2_MASK BIT(6)
#define ADF_SLICE_HANG_CMP0_MASK BIT(8)
#define ADF_SLICE_HANG_CMP1_MASK BIT(9)
#define ADF_SLICE_HANG_XLT0_MASK BIT(12)
#define ADF_SLICE_HANG_XLT1_MASK BIT(13)
#define ADF_SLICE_HANG_MMP0_MASK BIT(16)
#define ADF_SLICE_HANG_MMP1_MASK BIT(17)
#define ADF_SLICE_HANG_MMP2_MASK BIT(18)
#define ADF_SLICE_HANG_MMP3_MASK BIT(19)
#define ADF_SLICE_HANG_MMP4_MASK BIT(20)
#define ADF_SSMWDT(i) ((i)*0x4000 + 0x54)
#define ADF_SSMWDTPKE(i) ((i)*0x4000 + 0x58)
#define ADF_SHINTMASKSSM(i) ((i)*0x4000 + 0x1018)
#define ADF_ENABLE_SLICE_HANG 0x000000
#define ADF_MAX_MMP (5)
#define ADF_MMP_BASE(i) ((i)*0x1000 % 0x3800)
#define ADF_CERRSSMMMP(i, n) ((i)*0x4000 + ADF_MMP_BASE(n) + 0x380)
#define ADF_UERRSSMMMP(i, n) ((i)*0x4000 + ADF_MMP_BASE(n) + 0x388)
#define ADF_UERRSSMMMPAD(i, n) ((i)*0x4000 + ADF_MMP_BASE(n) + 0x38C)

bool adf_handle_slice_hang(struct adf_accel_dev *accel_dev,
			   u8 accel_num,
			   struct resource *csr,
			   u32 slice_hang_offset);
bool adf_check_slice_hang(struct adf_accel_dev *accel_dev);
void adf_print_err_registers(struct adf_accel_dev *accel_dev);

#endif
