/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2025 Intel Corporation */
#ifndef ADF_GEN4_HW_CSR_DATA_H_
#define ADF_GEN4_HW_CSR_DATA_H_

#include "adf_accel_devices.h"

/* Transport access */
#define ADF_BANK_INT_SRC_SEL_MASK 0x44UL
#define ADF_RING_CSR_RING_CONFIG 0x1000
#define ADF_RING_CSR_RING_LBASE 0x1040
#define ADF_RING_CSR_RING_UBASE 0x1080
#define ADF_RING_CSR_RING_HEAD 0x0C0
#define ADF_RING_CSR_RING_TAIL 0x100
#define ADF_RING_CSR_E_STAT 0x14C
#define ADF_RING_CSR_INT_FLAG 0x170
#define ADF_RING_CSR_INT_SRCSEL 0x174
#define ADF_RING_CSR_INT_COL_CTL 0x180
#define ADF_RING_CSR_INT_FLAG_AND_COL 0x184
#define ADF_RING_CSR_INT_COL_CTL_ENABLE 0x80000000
#define ADF_RING_CSR_INT_COL_EN 0x17C
#define ADF_RING_CSR_ADDR_OFFSET 0x100000
#define ADF_RING_BUNDLE_SIZE 0x2000

/* Ring reset */
#define ADF_RPRESET_POLL_TIMEOUT_US (5 * USEC_PER_SEC)
#define ADF_RPRESET_POLL_DELAY_US 20
#define ADF_WQM_CSR_RPRESETCTL_RESET BIT(0)
#define ADF_WQM_CSR_RPRESETCTL(bank) (0x6000 + ((bank) << 3))
#define ADF_WQM_CSR_RPRESETSTS_STATUS BIT(0)
#define ADF_WQM_CSR_RPRESETSTS(bank) (ADF_WQM_CSR_RPRESETCTL(bank) + 4)

#define ADF_WQM_CSR_RPRESETCTL_SHIFT 0
#define ADF_WQM_CSR_RPRESETCTL_DRAIN_SHIFT 2
#define ADF_WQM_CSR_RPRESETCTL_MASK (BIT(3) - 1)
#define ADF_WQM_CSR_RPRESETCTL(bank) (0x6000 + ((bank) << 3))
#define ADF_WQM_CSR_RPRESETSTS_SHIFT 0
#define ADF_WQM_CSR_RPRESETSTS_MASK (BIT(0))
#define ADF_WQM_CSR_RPRESETSTS(bank) (ADF_WQM_CSR_RPRESETCTL(bank) + 4)

#define BUILD_RING_BASE_ADDR(addr, size)                                       \
	((((addr) >> 6) & (GENMASK_ULL(63, 0) << (size))) << 6)
#define READ_CSR_RING_HEAD(csr_base_addr, bank, ring)                          \
	ADF_CSR_RD((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_RING_HEAD + ((ring) << 2))
#define READ_CSR_RING_TAIL(csr_base_addr, bank, ring)                          \
	ADF_CSR_RD((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_RING_TAIL + ((ring) << 2))
#define READ_CSR_E_STAT(csr_base_addr, bank)                                   \
	ADF_CSR_RD((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_E_STAT)
#define WRITE_CSR_RING_CONFIG(csr_base_addr, bank, ring, value)                \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_RING_CONFIG + ((ring) << 2),               \
		   value)
#define WRITE_CSR_RING_BASE(csr_base_addr, bank, ring, value)                  \
	do {                                                                   \
		struct resource *_csr_base_addr = csr_base_addr;               \
		u32 _bank = bank;                                              \
		u32 _ring = ring;                                              \
		bus_addr_t _value = value;                                     \
		u32 l_base = 0, u_base = 0;                                    \
		l_base = lower_32_bits(_value);                                \
		u_base = upper_32_bits(_value);                                \
		ADF_CSR_WR((_csr_base_addr),                                   \
			   ADF_RING_CSR_ADDR_OFFSET +                          \
			       ADF_RING_BUNDLE_SIZE * (_bank) +                \
			       ADF_RING_CSR_RING_LBASE + ((_ring) << 2),       \
			   l_base);                                            \
		ADF_CSR_WR((_csr_base_addr),                                   \
			   ADF_RING_CSR_ADDR_OFFSET +                          \
			       ADF_RING_BUNDLE_SIZE * (_bank) +                \
			       ADF_RING_CSR_RING_UBASE + ((_ring) << 2),       \
			   u_base);                                            \
	} while (0)

static inline u64
read_base_gen4(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	u32 l_base, u_base;
	u64 addr;

	l_base = ADF_CSR_RD(csr_base_addr,
			    ADF_RING_CSR_ADDR_OFFSET +
				(ADF_RING_BUNDLE_SIZE * bank) +
				ADF_RING_CSR_RING_LBASE + (ring << 2));
	u_base = ADF_CSR_RD(csr_base_addr,
			    ADF_RING_CSR_ADDR_OFFSET +
				(ADF_RING_BUNDLE_SIZE * bank) +
				ADF_RING_CSR_RING_UBASE + (ring << 2));

	addr = (u64)l_base & 0x00000000FFFFFFFFULL;
	addr |= (u64)u_base << 32 & 0xFFFFFFFF00000000ULL;

	return addr;
}

#define READ_CSR_RING_BASE(csr_base_addr, bank, ring)                          \
	read_base_gen4((csr_base_addr), (bank), (ring))

#define WRITE_CSR_RING_HEAD(csr_base_addr, bank, ring, value)                  \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_RING_HEAD + ((ring) << 2),                 \
		   value)
#define WRITE_CSR_RING_TAIL(csr_base_addr, bank, ring, value)                  \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_RING_TAIL + ((ring) << 2),                 \
		   value)
#define WRITE_CSR_INT_FLAG(csr_base_addr, bank, value)                         \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_INT_FLAG,                                  \
		   (value))
#define WRITE_CSR_INT_SRCSEL(csr_base_addr, bank)                              \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_INT_SRCSEL,                                \
		   ADF_BANK_INT_SRC_SEL_MASK)
#define WRITE_CSR_INT_COL_EN(csr_base_addr, bank, value)                       \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_INT_COL_EN,                                \
		   (value))
#define WRITE_CSR_INT_COL_CTL(csr_base_addr, bank, value)                      \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_INT_COL_CTL,                               \
		   ADF_RING_CSR_INT_COL_CTL_ENABLE | (value))
#define WRITE_CSR_INT_FLAG_AND_COL(csr_base_addr, bank, value)                 \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_INT_FLAG_AND_COL,                          \
		   (value))

/* Arbiter configuration */
#define ADF_RING_CSR_RING_SRV_ARB_EN 0x19C

#define READ_CSR_RING_SRV_ARB_EN(csr_base_addr, bank)                          \
	ADF_CSR_RD((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_RING_SRV_ARB_EN)

#define WRITE_CSR_RING_SRV_ARB_EN(csr_base_addr, bank, value)                  \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET + ADF_RING_BUNDLE_SIZE * (bank) +  \
		       ADF_RING_CSR_RING_SRV_ARB_EN,                           \
		   (value))

/* WDT timers
 *
 * Timeout is in cycles. Clock speed may vary across products but this
 * value should be a few milli-seconds.
 */
#define ADF_SSM_WDT_DEFAULT_VALUE 0x7000000ULL
#define ADF_SSM_WDT_PKE_DEFAULT_VALUE 0x8000000
#define ADF_SSMWDTL_OFFSET 0x54
#define ADF_SSMWDTH_OFFSET 0x5C
#define ADF_SSMWDTPKEL_OFFSET 0x58
#define ADF_SSMWDTPKEH_OFFSET 0x60

#define ADF_NUM_HB_CNT_PER_AE (ADF_NUM_THREADS_PER_AE)

int adf_gen4_set_ssm_wdtimer(struct adf_accel_dev *accel_dev);
void adf_gen4_init_hw_csr_info(struct adf_hw_csr_info *csr_info);
int adf_gen4_ring_pair_reset(struct adf_accel_dev *accel_dev, u32 bank_number);
#endif
