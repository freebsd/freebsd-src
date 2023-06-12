/* SPDX-License-Identifier: BSD-3-Clause */
/* Copyright(c) 2007-2022 Intel Corporation */
/* $FreeBSD$ */
#ifndef ADF_GEN4VF_HW_CSR_DATA_H_
#define ADF_GEN4VF_HW_CSR_DATA_H_

#define ADF_RING_CSR_ADDR_OFFSET_GEN4VF 0x0
#define ADF_RING_BUNDLE_SIZE_GEN4 0x2000
#define ADF_RING_CSR_RING_HEAD 0x0C0
#define ADF_RING_CSR_RING_TAIL 0x100
#define ADF_RING_CSR_E_STAT 0x14C
#define ADF_RING_CSR_RING_CONFIG_GEN4 0x1000
#define ADF_RING_CSR_RING_LBASE_GEN4 0x1040
#define ADF_RING_CSR_RING_UBASE_GEN4 0x1080
#define ADF_RING_CSR_INT_FLAG 0x170
#define ADF_RING_CSR_INT_FLAG_AND_COL 0x184
#define ADF_RING_CSR_NEXT_INT_SRCSEL 0x4
#define ADF_RING_CSR_INT_SRCSEL 0x174
#define ADF_RING_CSR_INT_COL_EN 0x17C
#define ADF_RING_CSR_INT_COL_CTL 0x180
#define ADF_RING_CSR_RING_SRV_ARB_EN 0x19C
#define ADF_BANK_INT_SRC_SEL_MASK_GEN4 0x44UL
#define ADF_RING_CSR_INT_COL_CTL_ENABLE 0x80000000
#define ADF_BANK_INT_FLAG_CLEAR_MASK_GEN4 0x3
#define ADF_RINGS_PER_INT_SRCSEL_GEN4 2

#define BUILD_RING_BASE_ADDR_GEN4(addr, size)                                  \
	((((addr) >> 6) & (0xFFFFFFFFFFFFFFFFULL << (size))) << 6)
#define READ_CSR_RING_HEAD_GEN4VF(csr_base_addr, bank, ring)                   \
	ADF_CSR_RD((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_RING_HEAD + ((ring) << 2))
#define READ_CSR_RING_TAIL_GEN4VF(csr_base_addr, bank, ring)                   \
	ADF_CSR_RD((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_RING_TAIL + ((ring) << 2))
#define READ_CSR_E_STAT_GEN4VF(csr_base_addr, bank)                            \
	ADF_CSR_RD((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_E_STAT)
#define WRITE_CSR_RING_CONFIG_GEN4VF(csr_base_addr, bank, ring, value)         \
	ADF_CSR_WR((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_RING_CONFIG_GEN4 + ((ring) << 2),          \
		   (value))
#define WRITE_CSR_RING_BASE_GEN4VF(csr_base_addr, bank, ring, value)           \
	do {                                                                   \
		struct resource *_csr_base_addr = csr_base_addr;               \
		u32 _bank = bank;                                              \
		u32 _ring = ring;                                              \
		dma_addr_t _value = value;                                     \
		u32 l_base = 0, u_base = 0;                                    \
		l_base = (u32)((_value)&0xFFFFFFFF);                           \
		u_base = (u32)(((_value)&0xFFFFFFFF00000000ULL) >> 32);        \
		ADF_CSR_WR((_csr_base_addr),                                   \
			   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                  \
			    ADF_RING_BUNDLE_SIZE_GEN4 * (_bank)) +             \
			       ADF_RING_CSR_RING_LBASE_GEN4 + ((_ring) << 2),  \
			   l_base);                                            \
		ADF_CSR_WR((_csr_base_addr),                                   \
			   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                  \
			    ADF_RING_BUNDLE_SIZE_GEN4 * (_bank)) +             \
			       ADF_RING_CSR_RING_UBASE_GEN4 + ((_ring) << 2),  \
			   u_base);                                            \
	} while (0)

static inline u64
read_base_gen4vf(struct resource *csr_base_addr, u32 bank, u32 ring)
{
	u32 l_base, u_base;
	u64 addr;

	l_base = ADF_CSR_RD(csr_base_addr,
			    (ADF_RING_BUNDLE_SIZE_GEN4 * bank) +
				ADF_RING_CSR_RING_LBASE_GEN4 + (ring << 2));
	u_base = ADF_CSR_RD(csr_base_addr,
			    (ADF_RING_BUNDLE_SIZE_GEN4 * bank) +
				ADF_RING_CSR_RING_UBASE_GEN4 + (ring << 2));

	addr = (u64)l_base & 0x00000000FFFFFFFFULL;
	addr |= (u64)u_base << 32 & 0xFFFFFFFF00000000ULL;

	return addr;
}

#define WRITE_CSR_INT_SRCSEL_GEN4VF(csr_base_addr, bank)                       \
	ADF_CSR_WR((csr_base_addr),                                            \
		   ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                           \
		       ADF_RING_BUNDLE_SIZE_GEN4 * (bank) +                    \
		       ADF_RING_CSR_INT_SRCSEL,                                \
		   ADF_BANK_INT_SRC_SEL_MASK_GEN4)

#define READ_CSR_RING_BASE_GEN4VF(csr_base_addr, bank, ring)                   \
	read_base_gen4vf((csr_base_addr), (bank), (ring))

#define WRITE_CSR_RING_HEAD_GEN4VF(csr_base_addr, bank, ring, value)           \
	ADF_CSR_WR((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_RING_HEAD + ((ring) << 2),                 \
		   (value))
#define WRITE_CSR_RING_TAIL_GEN4VF(csr_base_addr, bank, ring, value)           \
	ADF_CSR_WR((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_RING_TAIL + ((ring) << 2),                 \
		   (value))
#define WRITE_CSR_INT_FLAG_GEN4VF(csr_base_addr, bank, value)                  \
	ADF_CSR_WR((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_INT_FLAG,                                  \
		   (value))
#define WRITE_CSR_INT_COL_EN_GEN4VF(csr_base_addr, bank, value)                \
	ADF_CSR_WR((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_INT_COL_EN,                                \
		   (value))
#define WRITE_CSR_INT_COL_CTL_GEN4VF(csr_base_addr, bank, value)               \
	ADF_CSR_WR((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_INT_COL_CTL,                               \
		   (value))
#define WRITE_CSR_INT_FLAG_AND_COL_GEN4VF(csr_base_addr, bank, value)          \
	ADF_CSR_WR((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_INT_FLAG_AND_COL,                          \
		   (value))
#define READ_CSR_RING_SRV_ARB_EN_GEN4VF(csr_base_addr, bank)                   \
	ADF_CSR_RD((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_RING_SRV_ARB_EN)
#define WRITE_CSR_RING_SRV_ARB_EN_GEN4VF(csr_base_addr, bank, value)           \
	ADF_CSR_WR((csr_base_addr),                                            \
		   (ADF_RING_CSR_ADDR_OFFSET_GEN4VF +                          \
		    ADF_RING_BUNDLE_SIZE_GEN4 * (bank)) +                      \
		       ADF_RING_CSR_RING_SRV_ARB_EN,                           \
		   (value))

struct adf_hw_csr_info;
void gen4vf_init_hw_csr_info(struct adf_hw_csr_info *csr_info);

#endif /* ADF_GEN4VF_HW_CSR_DATA_H_ */
