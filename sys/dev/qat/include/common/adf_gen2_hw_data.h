/* SPDX-License-Identifier: BSD-3-Clause  */
/* Copyright(c) 2021 Intel Corporation */
#ifndef ADF_GEN2_HW_DATA_H_
#define ADF_GEN2_HW_DATA_H_

#include "adf_accel_devices.h"
#include "adf_cfg_common.h"

/* Transport access */
#define ADF_BANK_INT_SRC_SEL_MASK_0 0x4444444CUL
#define ADF_BANK_INT_SRC_SEL_MASK_X 0x44444444UL
#define ADF_RING_CSR_RING_CONFIG 0x000
#define ADF_RING_CSR_RING_LBASE 0x040
#define ADF_RING_CSR_RING_UBASE 0x080
#define ADF_RING_CSR_RING_HEAD 0x0C0
#define ADF_RING_CSR_RING_TAIL 0x100
#define ADF_RING_CSR_E_STAT 0x14C
#define ADF_RING_CSR_INT_FLAG 0x170
#define ADF_RING_CSR_INT_SRCSEL 0x174
#define ADF_RING_CSR_INT_SRCSEL_2 0x178
#define ADF_RING_CSR_INT_COL_EN 0x17C
#define ADF_RING_CSR_INT_COL_CTL 0x180
#define ADF_RING_CSR_INT_FLAG_AND_COL 0x184
#define ADF_RING_CSR_INT_COL_CTL_ENABLE 0x80000000
#define ADF_RING_CSR_ADDR_OFFSET 0x0
#define ADF_RING_BUNDLE_SIZE 0x1000
#define ADF_GEN2_RX_RINGS_OFFSET 8
#define ADF_GEN2_TX_RINGS_MASK 0xFF

#define BUILD_RING_BASE_ADDR(addr, size)                                       \
	(((addr) >> 6) & (GENMASK_ULL(63, 0) << (size)))
#define READ_CSR_RING_HEAD(csr_base_addr, bank, ring)                          \
	ADF_CSR_RD(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) + ADF_RING_CSR_RING_HEAD +  \
		       ((ring) << 2))
#define READ_CSR_RING_TAIL(csr_base_addr, bank, ring)                          \
	ADF_CSR_RD(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) + ADF_RING_CSR_RING_TAIL +  \
		       ((ring) << 2))
#define READ_CSR_E_STAT(csr_base_addr, bank)                                   \
	ADF_CSR_RD(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) + ADF_RING_CSR_E_STAT)
#define WRITE_CSR_RING_CONFIG(csr_base_addr, bank, ring, value)                \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) +                           \
		       ADF_RING_CSR_RING_CONFIG + ((ring) << 2),               \
		   value)

static inline uint64_t
read_base(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	u32 l_base, u_base;
	u64 addr;

	l_base = ADF_CSR_RD(csr_base_addr,
			    (ADF_RING_BUNDLE_SIZE * bank) +
				ADF_RING_CSR_RING_LBASE + (ring << 2));
	u_base = ADF_CSR_RD(csr_base_addr,
			    (ADF_RING_BUNDLE_SIZE * bank) +
				ADF_RING_CSR_RING_UBASE + (ring << 2));

	addr = (uint64_t)l_base & 0x00000000FFFFFFFFULL;
	addr |= (uint64_t)u_base << 32 & 0xFFFFFFFF00000000ULL;

	return addr;
}

#define READ_CSR_RING_BASE(csr_base_addr, bank, ring)                          \
	read_base(csr_base_addr, bank, ring)

#define WRITE_CSR_RING_BASE(csr_base_addr, bank, ring, value)                  \
	do {                                                                   \
		u32 l_base = 0, u_base = 0;                                    \
		l_base = (u32)((value)&0xFFFFFFFF);                            \
		u_base = (u32)(((value)&0xFFFFFFFF00000000ULL) >> 32);         \
		ADF_CSR_WR(csr_base_addr,                                      \
			   (ADF_RING_BUNDLE_SIZE * (bank)) +                   \
			       ADF_RING_CSR_RING_LBASE + ((ring) << 2),        \
			   l_base);                                            \
		ADF_CSR_WR(csr_base_addr,                                      \
			   (ADF_RING_BUNDLE_SIZE * (bank)) +                   \
			       ADF_RING_CSR_RING_UBASE + ((ring) << 2),        \
			   u_base);                                            \
	} while (0)

#define WRITE_CSR_RING_HEAD(csr_base_addr, bank, ring, value)                  \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) + ADF_RING_CSR_RING_HEAD +  \
		       ((ring) << 2),                                          \
		   value)
#define WRITE_CSR_RING_TAIL(csr_base_addr, bank, ring, value)                  \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) + ADF_RING_CSR_RING_TAIL +  \
		       ((ring) << 2),                                          \
		   value)
#define WRITE_CSR_INT_FLAG(csr_base_addr, bank, value)                         \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) + ADF_RING_CSR_INT_FLAG,    \
		   value)
#define WRITE_CSR_INT_SRCSEL(csr_base_addr, bank)                              \
	do {                                                                   \
		ADF_CSR_WR(csr_base_addr,                                      \
			   (ADF_RING_BUNDLE_SIZE * (bank)) +                   \
			       ADF_RING_CSR_INT_SRCSEL,                        \
			   ADF_BANK_INT_SRC_SEL_MASK_0);                       \
		ADF_CSR_WR(csr_base_addr,                                      \
			   (ADF_RING_BUNDLE_SIZE * (bank)) +                   \
			       ADF_RING_CSR_INT_SRCSEL_2,                      \
			   ADF_BANK_INT_SRC_SEL_MASK_X);                       \
	} while (0)
#define WRITE_CSR_INT_COL_EN(csr_base_addr, bank, value)                       \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) + ADF_RING_CSR_INT_COL_EN,  \
		   value)
#define WRITE_CSR_INT_COL_CTL(csr_base_addr, bank, value)                      \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) + ADF_RING_CSR_INT_COL_CTL, \
		   ADF_RING_CSR_INT_COL_CTL_ENABLE | (value))
#define WRITE_CSR_INT_FLAG_AND_COL(csr_base_addr, bank, value)                 \
	ADF_CSR_WR(csr_base_addr,                                              \
		   (ADF_RING_BUNDLE_SIZE * (bank)) +                           \
		       ADF_RING_CSR_INT_FLAG_AND_COL,                          \
		   value)

/* AE to function map */
#define AE2FUNCTION_MAP_A_OFFSET (0x3A400 + 0x190)
#define AE2FUNCTION_MAP_B_OFFSET (0x3A400 + 0x310)
#define AE2FUNCTION_MAP_REG_SIZE 4
#define AE2FUNCTION_MAP_VALID BIT(7)

#define READ_CSR_AE2FUNCTION_MAP_A(pmisc_bar_addr, index)                      \
	ADF_CSR_RD(pmisc_bar_addr,                                             \
		   AE2FUNCTION_MAP_A_OFFSET +                                  \
		       AE2FUNCTION_MAP_REG_SIZE * (index))
#define WRITE_CSR_AE2FUNCTION_MAP_A(pmisc_bar_addr, index, value)              \
	ADF_CSR_WR(pmisc_bar_addr,                                             \
		   AE2FUNCTION_MAP_A_OFFSET +                                  \
		       AE2FUNCTION_MAP_REG_SIZE * (index),                     \
		   value)
#define READ_CSR_AE2FUNCTION_MAP_B(pmisc_bar_addr, index)                      \
	ADF_CSR_RD(pmisc_bar_addr,                                             \
		   AE2FUNCTION_MAP_B_OFFSET +                                  \
		       AE2FUNCTION_MAP_REG_SIZE * (index))
#define WRITE_CSR_AE2FUNCTION_MAP_B(pmisc_bar_addr, index, value)              \
	ADF_CSR_WR(pmisc_bar_addr,                                             \
		   AE2FUNCTION_MAP_B_OFFSET +                                  \
		       AE2FUNCTION_MAP_REG_SIZE * (index),                     \
		   value)

/* Admin Interface Offsets */
#define ADF_ADMINMSGUR_OFFSET (0x3A000 + 0x574)
#define ADF_ADMINMSGLR_OFFSET (0x3A000 + 0x578)
#define ADF_MAILBOX_BASE_OFFSET 0x20970

/* Arbiter configuration */
#define ADF_ARB_OFFSET 0x30000
#define ADF_ARB_WRK_2_SER_MAP_OFFSET 0x180
#define ADF_ARB_CONFIG (BIT(31) | BIT(6) | BIT(0))
#define ADF_ARB_REG_SLOT 0x1000
#define ADF_ARB_RINGSRVARBEN_OFFSET 0x19C

#define READ_CSR_RING_SRV_ARB_EN(csr_addr, index)                              \
	ADF_CSR_RD(csr_addr,                                                   \
		   ADF_ARB_RINGSRVARBEN_OFFSET + (ADF_ARB_REG_SLOT * (index)))

#define WRITE_CSR_RING_SRV_ARB_EN(csr_addr, index, value)                      \
	ADF_CSR_WR(csr_addr,                                                   \
		   ADF_ARB_RINGSRVARBEN_OFFSET + (ADF_ARB_REG_SLOT * (index)), \
		   value)

/* Power gating */
#define ADF_POWERGATE_DC BIT(23)
#define ADF_POWERGATE_PKE BIT(24)

/* Default ring mapping */
#define ADF_GEN2_DEFAULT_RING_TO_SRV_MAP                                       \
	(CRYPTO << ADF_CFG_SERV_RING_PAIR_0_SHIFT |                            \
	 CRYPTO << ADF_CFG_SERV_RING_PAIR_1_SHIFT |                            \
	 UNUSED << ADF_CFG_SERV_RING_PAIR_2_SHIFT |                            \
	 COMP << ADF_CFG_SERV_RING_PAIR_3_SHIFT)

/* Error detection and correction */
#define ADF_GEN2_AE_CTX_ENABLES(i) ((i)*0x1000 + 0x20818)
#define ADF_GEN2_AE_MISC_CONTROL(i) ((i)*0x1000 + 0x20960)
#define ADF_GEN2_ENABLE_AE_ECC_ERR BIT(28)
#define ADF_GEN2_ENABLE_AE_ECC_PARITY_CORR (BIT(24) | BIT(12))
#define ADF_GEN2_UERRSSMSH(i) ((i)*0x4000 + 0x18)
#define ADF_GEN2_CERRSSMSH(i) ((i)*0x4000 + 0x10)
#define ADF_GEN2_ERRSSMSH_EN BIT(3)

#define ADF_NUM_HB_CNT_PER_AE (ADF_NUM_THREADS_PER_AE + ADF_NUM_PKE_STRAND)

void adf_gen2_init_hw_csr_info(struct adf_hw_csr_info *csr_info);

#endif
