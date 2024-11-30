/** @file
*
*  Copyright (c) 2011-2013, ARM Limited. All rights reserved.
*
*  SPDX-License-Identifier: BSD-2-Clause-Patent
*
**/

#ifndef ARMV7_MMU_H_
#define ARMV7_MMU_H_

#define TTBR_NOT_OUTER_SHAREABLE            BIT5
#define TTBR_RGN_OUTER_NON_CACHEABLE        0
#define TTBR_RGN_OUTER_WRITE_BACK_ALLOC     BIT3
#define TTBR_RGN_OUTER_WRITE_THROUGH        BIT4
#define TTBR_RGN_OUTER_WRITE_BACK_NO_ALLOC  (BIT3|BIT4)
#define TTBR_SHAREABLE                      BIT1
#define TTBR_NON_SHAREABLE                  0
#define TTBR_INNER_CACHEABLE                BIT0
#define TTBR_INNER_NON_CACHEABLE            0
#define TTBR_RGN_INNER_NON_CACHEABLE        0
#define TTBR_RGN_INNER_WRITE_BACK_ALLOC     BIT6
#define TTBR_RGN_INNER_WRITE_THROUGH        BIT0
#define TTBR_RGN_INNER_WRITE_BACK_NO_ALLOC  (BIT0|BIT6)

#define TTBR_WRITE_THROUGH        ( TTBR_RGN_OUTER_WRITE_THROUGH | TTBR_INNER_CACHEABLE | TTBR_SHAREABLE)
#define TTBR_WRITE_BACK_NO_ALLOC  ( TTBR_RGN_OUTER_WRITE_BACK_NO_ALLOC | TTBR_INNER_CACHEABLE | TTBR_SHAREABLE)
#define TTBR_NON_CACHEABLE        ( TTBR_RGN_OUTER_NON_CACHEABLE | TTBR_INNER_NON_CACHEABLE )
#define TTBR_WRITE_BACK_ALLOC     ( TTBR_RGN_OUTER_WRITE_BACK_ALLOC | TTBR_INNER_CACHEABLE | TTBR_SHAREABLE)

#define TTBR_MP_WRITE_THROUGH        ( TTBR_RGN_OUTER_WRITE_THROUGH | TTBR_RGN_INNER_WRITE_THROUGH | TTBR_SHAREABLE)
#define TTBR_MP_WRITE_BACK_NO_ALLOC  ( TTBR_RGN_OUTER_WRITE_BACK_NO_ALLOC | TTBR_RGN_INNER_WRITE_BACK_NO_ALLOC | TTBR_SHAREABLE)
#define TTBR_MP_NON_CACHEABLE        ( TTBR_RGN_OUTER_NON_CACHEABLE | TTBR_RGN_INNER_NON_CACHEABLE )
#define TTBR_MP_WRITE_BACK_ALLOC     ( TTBR_RGN_OUTER_WRITE_BACK_ALLOC | TTBR_RGN_INNER_WRITE_BACK_ALLOC | TTBR_SHAREABLE)

#define TRANSLATION_TABLE_SECTION_COUNT           4096
#define TRANSLATION_TABLE_SECTION_SIZE            (sizeof(UINT32) * TRANSLATION_TABLE_SECTION_COUNT)
#define TRANSLATION_TABLE_SECTION_ALIGNMENT       (sizeof(UINT32) * TRANSLATION_TABLE_SECTION_COUNT)
#define TRANSLATION_TABLE_SECTION_ALIGNMENT_MASK  (TRANSLATION_TABLE_SECTION_ALIGNMENT - 1)

#define TRANSLATION_TABLE_PAGE_COUNT           256
#define TRANSLATION_TABLE_PAGE_SIZE            (sizeof(UINT32) * TRANSLATION_TABLE_PAGE_COUNT)
#define TRANSLATION_TABLE_PAGE_ALIGNMENT       (sizeof(UINT32) * TRANSLATION_TABLE_PAGE_COUNT)
#define TRANSLATION_TABLE_PAGE_ALIGNMENT_MASK  (TRANSLATION_TABLE_PAGE_ALIGNMENT - 1)

#define TRANSLATION_TABLE_ENTRY_FOR_VIRTUAL_ADDRESS(table, address)  ((UINT32 *)(table) + (((UINTN)(address)) >> 20))

// Translation table descriptor types
#define TT_DESCRIPTOR_SECTION_TYPE_MASK          ((1UL << 18) | (3UL << 0))
#define TT_DESCRIPTOR_SECTION_TYPE_FAULT         (0UL << 0)
#define TT_DESCRIPTOR_SECTION_TYPE_PAGE_TABLE    (1UL << 0)
#define TT_DESCRIPTOR_SECTION_TYPE_SECTION       ((0UL << 18) | (2UL << 0))
#define TT_DESCRIPTOR_SECTION_TYPE_SUPERSECTION  ((1UL << 18) | (2UL << 0))
#define TT_DESCRIPTOR_SECTION_TYPE_IS_PAGE_TABLE(Desc)  (((Desc) & 3UL) == TT_DESCRIPTOR_SECTION_TYPE_PAGE_TABLE)

// Translation table descriptor types
#define TT_DESCRIPTOR_PAGE_TYPE_MASK   (1UL << 1)
#define TT_DESCRIPTOR_PAGE_TYPE_FAULT  (0UL << 1)
#define TT_DESCRIPTOR_PAGE_TYPE_PAGE   (1UL << 1)

// Section descriptor definitions
#define TT_DESCRIPTOR_SECTION_SIZE  (0x00100000)

#define TT_DESCRIPTOR_SECTION_NS_MASK  (1UL << 19)
#define TT_DESCRIPTOR_SECTION_NS       (1UL << 19)

#define TT_DESCRIPTOR_SECTION_NG_MASK    (1UL << 17)
#define TT_DESCRIPTOR_SECTION_NG_GLOBAL  (0UL << 17)
#define TT_DESCRIPTOR_SECTION_NG_LOCAL   (1UL << 17)

#define TT_DESCRIPTOR_PAGE_NG_MASK    (1UL << 11)
#define TT_DESCRIPTOR_PAGE_NG_GLOBAL  (0UL << 11)
#define TT_DESCRIPTOR_PAGE_NG_LOCAL   (1UL << 11)

#define TT_DESCRIPTOR_SECTION_S_MASK        (1UL << 16)
#define TT_DESCRIPTOR_SECTION_S_NOT_SHARED  (0UL << 16)
#define TT_DESCRIPTOR_SECTION_S_SHARED      (1UL << 16)

#define TT_DESCRIPTOR_PAGE_S_MASK        (1UL << 10)
#define TT_DESCRIPTOR_PAGE_S_NOT_SHARED  (0UL << 10)
#define TT_DESCRIPTOR_PAGE_S_SHARED      (1UL << 10)

#define TT_DESCRIPTOR_SECTION_AP_MASK   ((1UL << 15) | (1UL << 11))
#define TT_DESCRIPTOR_SECTION_AP_NO_RW  ((0UL << 15) | (0UL << 11))
#define TT_DESCRIPTOR_SECTION_AP_RW_RW  ((0UL << 15) | (1UL << 11))
#define TT_DESCRIPTOR_SECTION_AP_NO_RO  ((1UL << 15) | (0UL << 11))
#define TT_DESCRIPTOR_SECTION_AP_RO_RO  ((1UL << 15) | (1UL << 11))

#define TT_DESCRIPTOR_SECTION_AF  (1UL << 10)

#define TT_DESCRIPTOR_PAGE_AP_MASK   ((1UL << 9) | (1UL << 5))
#define TT_DESCRIPTOR_PAGE_AP_NO_RW  ((0UL << 9) | (0UL << 5))
#define TT_DESCRIPTOR_PAGE_AP_RW_RW  ((0UL << 9) | (1UL << 5))
#define TT_DESCRIPTOR_PAGE_AP_NO_RO  ((1UL << 9) | (0UL << 5))
#define TT_DESCRIPTOR_PAGE_AP_RO_RO  ((1UL << 9) | (1UL << 5))

#define TT_DESCRIPTOR_PAGE_AF  (1UL << 4)

#define TT_DESCRIPTOR_SECTION_XN_MASK  (0x1UL << 4)
#define TT_DESCRIPTOR_PAGE_XN_MASK     (0x1UL << 0)

#define TT_DESCRIPTOR_SECTION_CACHE_POLICY_MASK                    ((3UL << 12) | (1UL << 3) | (1UL << 2))
#define TT_DESCRIPTOR_SECTION_CACHEABLE_MASK                       (1UL << 3)
#define TT_DESCRIPTOR_SECTION_CACHE_POLICY_STRONGLY_ORDERED        ((0UL << 12) | (0UL << 3) | (0UL << 2))
#define TT_DESCRIPTOR_SECTION_CACHE_POLICY_SHAREABLE_DEVICE        ((0UL << 12) | (0UL << 3) | (1UL << 2))
#define TT_DESCRIPTOR_SECTION_CACHE_POLICY_WRITE_THROUGH_NO_ALLOC  ((0UL << 12) | (1UL << 3) | (0UL << 2))
#define TT_DESCRIPTOR_SECTION_CACHE_POLICY_WRITE_BACK_NO_ALLOC     ((0UL << 12) | (1UL << 3) | (1UL << 2))
#define TT_DESCRIPTOR_SECTION_CACHE_POLICY_NON_CACHEABLE           ((1UL << 12) | (0UL << 3) | (0UL << 2))
#define TT_DESCRIPTOR_SECTION_CACHE_POLICY_WRITE_BACK_ALLOC        ((1UL << 12) | (1UL << 3) | (1UL << 2))
#define TT_DESCRIPTOR_SECTION_CACHE_POLICY_NON_SHAREABLE_DEVICE    ((2UL << 12) | (0UL << 3) | (0UL << 2))

#define TT_DESCRIPTOR_PAGE_SIZE  (0x00001000)

#define TT_DESCRIPTOR_PAGE_CACHE_POLICY_MASK                    ((3UL << 6) | (1UL << 3) | (1UL << 2))
#define TT_DESCRIPTOR_PAGE_CACHEABLE_MASK                       (1UL << 3)
#define TT_DESCRIPTOR_PAGE_CACHE_POLICY_STRONGLY_ORDERED        ((0UL << 6) | (0UL << 3) | (0UL << 2))
#define TT_DESCRIPTOR_PAGE_CACHE_POLICY_SHAREABLE_DEVICE        ((0UL << 6) | (0UL << 3) | (1UL << 2))
#define TT_DESCRIPTOR_PAGE_CACHE_POLICY_WRITE_THROUGH_NO_ALLOC  ((0UL << 6) | (1UL << 3) | (0UL << 2))
#define TT_DESCRIPTOR_PAGE_CACHE_POLICY_WRITE_BACK_NO_ALLOC     ((0UL << 6) | (1UL << 3) | (1UL << 2))
#define TT_DESCRIPTOR_PAGE_CACHE_POLICY_NON_CACHEABLE           ((1UL << 6) | (0UL << 3) | (0UL << 2))
#define TT_DESCRIPTOR_PAGE_CACHE_POLICY_WRITE_BACK_ALLOC        ((1UL << 6) | (1UL << 3) | (1UL << 2))
#define TT_DESCRIPTOR_PAGE_CACHE_POLICY_NON_SHAREABLE_DEVICE    ((2UL << 6) | (0UL << 3) | (0UL << 2))

#define TT_DESCRIPTOR_CONVERT_TO_PAGE_AP(Desc)            ((((Desc) & TT_DESCRIPTOR_SECTION_AP_MASK) >> 6) & TT_DESCRIPTOR_PAGE_AP_MASK)
#define TT_DESCRIPTOR_CONVERT_TO_PAGE_NG(Desc)            ((((Desc) & TT_DESCRIPTOR_SECTION_NG_MASK) >> 6) & TT_DESCRIPTOR_PAGE_NG_MASK)
#define TT_DESCRIPTOR_CONVERT_TO_PAGE_S(Desc)             ((((Desc) & TT_DESCRIPTOR_SECTION_S_MASK) >> 6) & TT_DESCRIPTOR_PAGE_S_MASK)
#define TT_DESCRIPTOR_CONVERT_TO_PAGE_AF(Desc)            ((((Desc) & TT_DESCRIPTOR_SECTION_AF) >> 6) & TT_DESCRIPTOR_PAGE_AF)
#define TT_DESCRIPTOR_CONVERT_TO_PAGE_XN(Desc)            ((((Desc) & TT_DESCRIPTOR_SECTION_XN_MASK) >> 4) & TT_DESCRIPTOR_PAGE_XN_MASK)
#define TT_DESCRIPTOR_CONVERT_TO_PAGE_CACHE_POLICY(Desc)  ((((Desc) & (0x3 << 12)) >> 6) | (Desc & (0x3 << 2)))

#define TT_DESCRIPTOR_CONVERT_TO_SECTION_AP(Desc)            ((((Desc) & TT_DESCRIPTOR_PAGE_AP_MASK) << 6) & TT_DESCRIPTOR_SECTION_AP_MASK)
#define TT_DESCRIPTOR_CONVERT_TO_SECTION_S(Desc)             ((((Desc) & TT_DESCRIPTOR_PAGE_S_MASK) << 6) & TT_DESCRIPTOR_SECTION_S_MASK)
#define TT_DESCRIPTOR_CONVERT_TO_SECTION_AF(Desc)            ((((Desc) & TT_DESCRIPTOR_PAGE_AF) << 6) & TT_DESCRIPTOR_SECTION_AF)
#define TT_DESCRIPTOR_CONVERT_TO_SECTION_XN(Desc)            ((((Desc) & TT_DESCRIPTOR_PAGE_XN_MASK) << 4) & TT_DESCRIPTOR_SECTION_XN_MASK)
#define TT_DESCRIPTOR_CONVERT_TO_SECTION_CACHE_POLICY(Desc)  ((((Desc) & (0x3 << 6)) << 6) | (Desc & (0x3 << 2)))

#define TT_DESCRIPTOR_SECTION_ATTRIBUTE_MASK  (TT_DESCRIPTOR_SECTION_NS_MASK | TT_DESCRIPTOR_SECTION_NG_MASK |               \
                                                             TT_DESCRIPTOR_SECTION_S_MASK | TT_DESCRIPTOR_SECTION_AP_MASK | \
                                                             TT_DESCRIPTOR_SECTION_AF | \
                                                             TT_DESCRIPTOR_SECTION_XN_MASK | TT_DESCRIPTOR_SECTION_CACHE_POLICY_MASK)

#define TT_DESCRIPTOR_PAGE_ATTRIBUTE_MASK  (TT_DESCRIPTOR_PAGE_NG_MASK | TT_DESCRIPTOR_PAGE_S_MASK |                  \
                                                             TT_DESCRIPTOR_PAGE_AP_MASK | TT_DESCRIPTOR_PAGE_XN_MASK | \
                                                             TT_DESCRIPTOR_PAGE_AF | \
                                                             TT_DESCRIPTOR_PAGE_CACHE_POLICY_MASK)

#define TT_DESCRIPTOR_SECTION_DOMAIN_MASK  (0x0FUL << 5)
#define TT_DESCRIPTOR_SECTION_DOMAIN(a)  (((a) & 0x0FUL) << 5)

#define TT_DESCRIPTOR_SECTION_BASE_ADDRESS_MASK       (0xFFF00000)
#define TT_DESCRIPTOR_SECTION_PAGETABLE_ADDRESS_MASK  (0xFFFFFC00)
#define TT_DESCRIPTOR_SECTION_BASE_ADDRESS(a)  ((a) & TT_DESCRIPTOR_SECTION_BASE_ADDRESS_MASK)
#define TT_DESCRIPTOR_SECTION_BASE_SHIFT  20

#define TT_DESCRIPTOR_PAGE_BASE_ADDRESS_MASK  (0xFFFFF000)
#define TT_DESCRIPTOR_PAGE_INDEX_MASK         (0x000FF000)
#define TT_DESCRIPTOR_PAGE_BASE_ADDRESS(a)  ((a) & TT_DESCRIPTOR_PAGE_BASE_ADDRESS_MASK)
#define TT_DESCRIPTOR_PAGE_BASE_SHIFT  12

#define TT_DESCRIPTOR_SECTION_DEFAULT  (TT_DESCRIPTOR_SECTION_TYPE_SECTION | \
                                        TT_DESCRIPTOR_SECTION_NG_GLOBAL    | \
                                        TT_DESCRIPTOR_SECTION_S_SHARED     | \
                                        TT_DESCRIPTOR_SECTION_DOMAIN(0)    | \
                                        TT_DESCRIPTOR_SECTION_AP_RW_RW     | \
                                        TT_DESCRIPTOR_SECTION_AF)

#define TT_DESCRIPTOR_SECTION_WRITE_BACK  (TT_DESCRIPTOR_SECTION_DEFAULT | \
                                           TT_DESCRIPTOR_SECTION_CACHE_POLICY_WRITE_BACK_ALLOC)

#define TT_DESCRIPTOR_SECTION_WRITE_THROUGH  (TT_DESCRIPTOR_SECTION_DEFAULT | \
                                              TT_DESCRIPTOR_SECTION_CACHE_POLICY_WRITE_THROUGH_NO_ALLOC)

#define TT_DESCRIPTOR_SECTION_DEVICE  (TT_DESCRIPTOR_SECTION_DEFAULT | \
                                       TT_DESCRIPTOR_SECTION_CACHE_POLICY_SHAREABLE_DEVICE)

#define TT_DESCRIPTOR_SECTION_UNCACHED  (TT_DESCRIPTOR_SECTION_DEFAULT | \
                                         TT_DESCRIPTOR_SECTION_CACHE_POLICY_NON_CACHEABLE)

#define TT_DESCRIPTOR_PAGE_WRITE_BACK     (TT_DESCRIPTOR_PAGE_TYPE_PAGE                                                           |          \
                                                        TT_DESCRIPTOR_PAGE_NG_GLOBAL                                                      | \
                                                        TT_DESCRIPTOR_PAGE_S_SHARED                                                       | \
                                                        TT_DESCRIPTOR_PAGE_AP_RW_RW                                                       | \
                                                        TT_DESCRIPTOR_PAGE_AF                                                             | \
                                                        TT_DESCRIPTOR_PAGE_CACHE_POLICY_WRITE_BACK_ALLOC)
#define TT_DESCRIPTOR_PAGE_WRITE_THROUGH  (TT_DESCRIPTOR_PAGE_TYPE_PAGE                                                           |          \
                                                        TT_DESCRIPTOR_PAGE_NG_GLOBAL                                                      | \
                                                        TT_DESCRIPTOR_PAGE_S_SHARED                                                       | \
                                                        TT_DESCRIPTOR_PAGE_AP_RW_RW                                                       | \
                                                        TT_DESCRIPTOR_PAGE_AF                                                             | \
                                                        TT_DESCRIPTOR_PAGE_CACHE_POLICY_WRITE_THROUGH_NO_ALLOC)
#define TT_DESCRIPTOR_PAGE_DEVICE         (TT_DESCRIPTOR_PAGE_TYPE_PAGE                                                           |          \
                                                        TT_DESCRIPTOR_PAGE_NG_GLOBAL                                                      | \
                                                        TT_DESCRIPTOR_PAGE_S_NOT_SHARED                                                   | \
                                                        TT_DESCRIPTOR_PAGE_AP_RW_RW                                                       | \
                                                        TT_DESCRIPTOR_PAGE_AF                                                             | \
                                                        TT_DESCRIPTOR_PAGE_XN_MASK                                                        | \
                                                        TT_DESCRIPTOR_PAGE_CACHE_POLICY_SHAREABLE_DEVICE)
#define TT_DESCRIPTOR_PAGE_UNCACHED       (TT_DESCRIPTOR_PAGE_TYPE_PAGE                                                           |          \
                                                        TT_DESCRIPTOR_PAGE_NG_GLOBAL                                                      | \
                                                        TT_DESCRIPTOR_PAGE_S_NOT_SHARED                                                   | \
                                                        TT_DESCRIPTOR_PAGE_AP_RW_RW                                                       | \
                                                        TT_DESCRIPTOR_PAGE_AF                                                             | \
                                                        TT_DESCRIPTOR_PAGE_CACHE_POLICY_NON_CACHEABLE)

// First Level Descriptors
typedef UINT32 ARM_FIRST_LEVEL_DESCRIPTOR;

// Second Level Descriptors
typedef UINT32 ARM_PAGE_TABLE_ENTRY;

UINT32
ConvertSectionAttributesToPageAttributes (
  IN UINT32  SectionAttributes
  );

#endif // ARMV7_MMU_H_
