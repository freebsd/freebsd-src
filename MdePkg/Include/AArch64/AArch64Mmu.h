/** @file
*
*  Copyright (c) 2011-2021, Arm Limited. All rights reserved.<BR>
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef AARCH64_MMU_H_
#define AARCH64_MMU_H_

//
// Memory Attribute Indirection register Definitions
//
#define MAIR_ATTR_DEVICE_MEMORY                0x0ULL
#define MAIR_ATTR_NORMAL_MEMORY_NON_CACHEABLE  0x44ULL
#define MAIR_ATTR_NORMAL_MEMORY_WRITE_THROUGH  0xBBULL
#define MAIR_ATTR_NORMAL_MEMORY_WRITE_BACK     0xFFULL

#define MAIR_ATTR(n, value)  ((value) << (((n) >> 2)*8))

//
// Long-descriptor Translation Table format
//

// Return the smallest offset from the table level.
// The first offset starts at 12bit. There are 4 levels of 9-bit address range from level 3 to level 0
#define TT_ADDRESS_OFFSET_AT_LEVEL(TableLevel)  (12 + ((3 - (TableLevel)) * 9))

#define TT_BLOCK_ENTRY_SIZE_AT_LEVEL(Level)  (1ULL << TT_ADDRESS_OFFSET_AT_LEVEL(Level))

// Get the associated entry in the given Translation Table
#define TT_GET_ENTRY_FOR_ADDRESS(TranslationTable, Level, Address)  \
    ((UINTN)(TranslationTable) + ((((UINTN)(Address) >> TT_ADDRESS_OFFSET_AT_LEVEL(Level)) & (BIT9-1)) * sizeof(UINT64)))

// Return the smallest address granularity from the table level.
// The first offset starts at 12bit. There are 4 levels of 9-bit address range from level 3 to level 0
#define TT_ADDRESS_AT_LEVEL(TableLevel)  (1ULL << TT_ADDRESS_OFFSET_AT_LEVEL(TableLevel))

#define TT_LAST_BLOCK_ADDRESS(TranslationTable, EntryCount) \
    ((UINT64*)((EFI_PHYSICAL_ADDRESS)(TranslationTable) + (((EntryCount) - 1) * sizeof(UINT64))))

// There are 512 entries per table when 4K Granularity
#define TT_ENTRY_COUNT                  512
#define TT_ALIGNMENT_BLOCK_ENTRY        BIT12
#define TT_ALIGNMENT_DESCRIPTION_TABLE  BIT12

#define TT_ADDRESS_MASK_BLOCK_ENTRY        (0xFFFFFFFFFULL << 12)
#define TT_ADDRESS_MASK_DESCRIPTION_TABLE  (0xFFFFFFFFFULL << 12)

#define TT_TYPE_MASK                0x3
#define TT_TYPE_TABLE_ENTRY         0x3
#define TT_TYPE_BLOCK_ENTRY         0x1
#define TT_TYPE_BLOCK_ENTRY_LEVEL3  0x3

#define TT_ATTR_INDX_MASK                  (0x7 << 2)
#define TT_ATTR_INDX_DEVICE_MEMORY         (0x0 << 2)
#define TT_ATTR_INDX_MEMORY_NON_CACHEABLE  (0x1 << 2)
#define TT_ATTR_INDX_MEMORY_WRITE_THROUGH  (0x2 << 2)
#define TT_ATTR_INDX_MEMORY_WRITE_BACK     (0x3 << 2)

#define TT_AP_MASK   (0x3UL << 6)
#define TT_AP_NO_RW  (0x0UL << 6)
#define TT_AP_RW_RW  (0x1UL << 6)
#define TT_AP_NO_RO  (0x2UL << 6)
#define TT_AP_RO_RO  (0x3UL << 6)

#define TT_NS  BIT5
#define TT_AF  BIT10

#define TT_SH_NON_SHAREABLE    (0x0 << 8)
#define TT_SH_OUTER_SHAREABLE  (0x2 << 8)
#define TT_SH_INNER_SHAREABLE  (0x3 << 8)
#define TT_SH_MASK             (0x3 << 8)

#define TT_PXN_MASK  BIT53
#define TT_UXN_MASK  BIT54                              // EL1&0
#define TT_XN_MASK   BIT54                              // EL2 / EL3

#define TT_ATTRIBUTES_MASK  ((0xFFFULL << 52) | (0x3FFULL << 2))

#define TT_TABLE_PXN  BIT59
#define TT_TABLE_UXN  BIT60                             // EL1&0
#define TT_TABLE_XN   BIT60                             // EL2 / EL3
#define TT_TABLE_NS   BIT63

#define TT_TABLE_AP_MASK             (BIT62 | BIT61)
#define TT_TABLE_AP_NO_PERMISSION    (0x0ULL << 61)
#define TT_TABLE_AP_EL0_NO_ACCESS    (0x1ULL << 61)
#define TT_TABLE_AP_NO_WRITE_ACCESS  (0x2ULL << 61)

//
// Translation Control Register
//
#define TCR_T0SZ_MASK  0x3FUL

#define TCR_PS_4GB    (0UL << 16)
#define TCR_PS_64GB   (1UL << 16)
#define TCR_PS_1TB    (2UL << 16)
#define TCR_PS_4TB    (3UL << 16)
#define TCR_PS_16TB   (4UL << 16)
#define TCR_PS_256TB  (5UL << 16)

#define TCR_TG0_4KB  (0UL << 14)
#define TCR_TG1_4KB  (2UL << 30)

#define TCR_IPS_4GB    (0ULL << 32)
#define TCR_IPS_64GB   (1ULL << 32)
#define TCR_IPS_1TB    (2ULL << 32)
#define TCR_IPS_4TB    (3ULL << 32)
#define TCR_IPS_16TB   (4ULL << 32)
#define TCR_IPS_256TB  (5ULL << 32)

#define TCR_EPD1  (1UL << 23)

#define TTBR_ASID_FIELD  (48)
#define TTBR_ASID_MASK   (0xFF << TTBR_ASID_FIELD)
#define TTBR_BADDR_MASK  (0xFFFFFFFFFFFF )                     // The width of this field depends on the values in TxSZ. Addr occupies bottom 48bits

#define TCR_EL1_T0SZ_FIELD   (0)
#define TCR_EL1_EPD0_FIELD   (7)
#define TCR_EL1_IRGN0_FIELD  (8)
#define TCR_EL1_ORGN0_FIELD  (10)
#define TCR_EL1_SH0_FIELD    (12)
#define TCR_EL1_TG0_FIELD    (14)
#define TCR_EL1_T1SZ_FIELD   (16)
#define TCR_EL1_A1_FIELD     (22)
#define TCR_EL1_EPD1_FIELD   (23)
#define TCR_EL1_IRGN1_FIELD  (24)
#define TCR_EL1_ORGN1_FIELD  (26)
#define TCR_EL1_SH1_FIELD    (28)
#define TCR_EL1_TG1_FIELD    (30)
#define TCR_EL1_IPS_FIELD    (32)
#define TCR_EL1_AS_FIELD     (36)
#define TCR_EL1_TBI0_FIELD   (37)
#define TCR_EL1_TBI1_FIELD   (38)
#define TCR_EL1_T0SZ_MASK    (0x1FUL << TCR_EL1_T0SZ_FIELD)
#define TCR_EL1_EPD0_MASK    (0x01UL << TCR_EL1_EPD0_FIELD)
#define TCR_EL1_IRGN0_MASK   (0x03UL << TCR_EL1_IRGN0_FIELD)
#define TCR_EL1_ORGN0_MASK   (0x03UL << TCR_EL1_ORGN0_FIELD)
#define TCR_EL1_SH0_MASK     (0x03UL << TCR_EL1_SH0_FIELD)
#define TCR_EL1_TG0_MASK     (0x01UL << TCR_EL1_TG0_FIELD)
#define TCR_EL1_T1SZ_MASK    (0x1FUL << TCR_EL1_T1SZ_FIELD)
#define TCR_EL1_A1_MASK      (0x01UL << TCR_EL1_A1_FIELD)
#define TCR_EL1_EPD1_MASK    (0x01UL << TCR_EL1_EPD1_FIELD)
#define TCR_EL1_IRGN1_MASK   (0x03UL << TCR_EL1_IRGN1_FIELD)
#define TCR_EL1_ORGN1_MASK   (0x03UL << TCR_EL1_ORGN1_FIELD)
#define TCR_EL1_SH1_MASK     (0x03UL << TCR_EL1_SH1_FIELD)
#define TCR_EL1_TG1_MASK     (0x01UL << TCR_EL1_TG1_FIELD)
#define TCR_EL1_IPS_MASK     (0x07UL << TCR_EL1_IPS_FIELD)
#define TCR_EL1_AS_MASK      (0x01UL << TCR_EL1_AS_FIELD)
#define TCR_EL1_TBI0_MASK    (0x01UL << TCR_EL1_TBI0_FIELD)
#define TCR_EL1_TBI1_MASK    (0x01UL << TCR_EL1_TBI1_FIELD)

#define TCR_EL23_T0SZ_FIELD   (0)
#define TCR_EL23_IRGN0_FIELD  (8)
#define TCR_EL23_ORGN0_FIELD  (10)
#define TCR_EL23_SH0_FIELD    (12)
#define TCR_EL23_TG0_FIELD    (14)
#define TCR_EL23_PS_FIELD     (16)
#define TCR_EL23_T0SZ_MASK    (0x1FUL << TCR_EL23_T0SZ_FIELD)
#define TCR_EL23_IRGN0_MASK   (0x03UL << TCR_EL23_IRGN0_FIELD)
#define TCR_EL23_ORGN0_MASK   (0x03UL << TCR_EL23_ORGN0_FIELD)
#define TCR_EL23_SH0_MASK     (0x03UL << TCR_EL23_SH0_FIELD)
#define TCR_EL23_TG0_MASK     (0x01UL << TCR_EL23_TG0_FIELD)
#define TCR_EL23_PS_MASK      (0x07UL << TCR_EL23_PS_FIELD)

#define TCR_RGN_OUTER_NON_CACHEABLE        (0x0UL << 10)
#define TCR_RGN_OUTER_WRITE_BACK_ALLOC     (0x1UL << 10)
#define TCR_RGN_OUTER_WRITE_THROUGH        (0x2UL << 10)
#define TCR_RGN_OUTER_WRITE_BACK_NO_ALLOC  (0x3UL << 10)

#define TCR_RGN_INNER_NON_CACHEABLE        (0x0UL << 8)
#define TCR_RGN_INNER_WRITE_BACK_ALLOC     (0x1UL << 8)
#define TCR_RGN_INNER_WRITE_THROUGH        (0x2UL << 8)
#define TCR_RGN_INNER_WRITE_BACK_NO_ALLOC  (0x3UL << 8)

#define TCR_SH_NON_SHAREABLE    (0x0UL << 12)
#define TCR_SH_OUTER_SHAREABLE  (0x2UL << 12)
#define TCR_SH_INNER_SHAREABLE  (0x3UL << 12)

#define TCR_PASZ_32BITS_4GB    (0x0UL)
#define TCR_PASZ_36BITS_64GB   (0x1UL)
#define TCR_PASZ_40BITS_1TB    (0x2UL)
#define TCR_PASZ_42BITS_4TB    (0x3UL)
#define TCR_PASZ_44BITS_16TB   (0x4UL)
#define TCR_PASZ_48BITS_256TB  (0x5UL)

// The value written to the T*SZ fields are defined as 2^(64-T*SZ). So a 39Bit
// Virtual address range for 512GB of virtual space sets T*SZ to 25
#define INPUT_ADDRESS_SIZE_TO_TXSZ(a)  (64 - a)

// Uses LPAE Page Table format

#endif // AARCH64_MMU_H_
