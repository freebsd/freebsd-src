/** @file

  Copyright (c) 2024 Loongson Technology Corporation Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Glossary:
    - EXC     - Exception
    - CSR     - CPU Status Register
**/

#ifndef LOONGARCH_CSR_H_
#define LOONGARCH_CSR_H_

#include <Base.h>

//
// CSR register numbers
//

//
// Basic CSR registers
//
#define LOONGARCH_CSR_CRMD     0x0
#define LOONGARCH_CSR_PRMD     0x1
#define LOONGARCH_CSR_EUEN     0x2
#define CSR_EUEN_LBTEN_SHIFT   3
#define CSR_EUEN_LBTEN         (0x1ULL << CSR_EUEN_LBTEN_SHIFT)
#define CSR_EUEN_LASXEN_SHIFT  2
#define CSR_EUEN_LASXEN        (0x1ULL << CSR_EUEN_LASXEN_SHIFT)
#define CSR_EUEN_LSXEN_SHIFT   1
#define CSR_EUEN_LSXEN         (0x1ULL << CSR_EUEN_LSXEN_SHIFT)
#define CSR_EUEN_FPEN_SHIFT    0
#define CSR_EUEN_FPEN          (0x1ULL << CSR_EUEN_FPEN_SHIFT)
#define LOONGARCH_CSR_MISC     0x3
#define LOONGARCH_CSR_ECFG     0x4

#define LOONGARCH_CSR_ESTAT       0x5
#define CSR_ESTAT_ESUBCODE_SHIFT  22
#define CSR_ESTAT_ESUBCODE_WIDTH  9
#define CSR_ESTAT_ESUBCODE        (0x1ffULL << CSR_ESTAT_ESUBCODE_SHIFT)
#define CSR_ESTAT_EXC_SHIFT       16
#define CSR_ESTAT_EXC_WIDTH       6
#define CSR_ESTAT_EXC             (0x3FULL << CSR_ESTAT_EXC_SHIFT)
#define CSR_ESTAT_IS_SHIFT        0
#define CSR_ESTAT_IS_WIDTH        15
#define CSR_ESTAT_IS              (0x7FFFULL << CSR_ESTAT_IS_SHIFT)

#define LOONGARCH_CSR_ERA    0x6
#define LOONGARCH_CSR_BADV   0x7
#define LOONGARCH_CSR_BADI   0x8
#define LOONGARCH_CSR_EBASE  0xC     // Exception entry base address

//
// TLB related CSR registers
//
#define LOONGARCH_CSR_TLBIDX      0x10      // TLB Index, EHINV, PageSize, NP
#define LOONGARCH_CSR_TLBEHI      0x11      // TLB EntryHi
#define LOONGARCH_CSR_TLBELO0     0x12      // TLB EntryLo0
#define LOONGARCH_CSR_TLBELO1     0x13      // TLB EntryLo1
#define LOONGARCH_CSR_ASID        0x18      // ASID
#define LOONGARCH_CSR_PGDL        0x19      // Page table base address when VA[47] = 0
#define LOONGARCH_CSR_PGDH        0x1A      // Page table base address when VA[47] = 1
#define LOONGARCH_CSR_PGD         0x1B      // Page table base
#define LOONGARCH_CSR_PWCTL0      0x1C      // PWCtl0
#define LOONGARCH_CSR_PWCTL1      0x1D      // PWCtl1
#define LOONGARCH_CSR_STLBPGSIZE  0x1E
#define LOONGARCH_CSR_RVACFG      0x1F

///
/// Page table property definitions
///
#define PAGE_VALID_SHIFT   0
#define PAGE_DIRTY_SHIFT   1
#define PAGE_PLV_SHIFT     2  // 2~3, two bits
#define CACHE_SHIFT        4  // 4~5, two bits
#define PAGE_GLOBAL_SHIFT  6
#define PAGE_HUGE_SHIFT    6  // HUGE is a PMD bit

#define PAGE_HGLOBAL_SHIFT  12 // HGlobal is a PMD bit
#define PAGE_PFN_SHIFT      12
#define PAGE_PFN_END_SHIFT  48
#define PAGE_NO_READ_SHIFT  61
#define PAGE_NO_EXEC_SHIFT  62
#define PAGE_RPLV_SHIFT     63

///
/// Used by TLB hardware (placed in EntryLo*)
///
#define PAGE_VALID    ((UINTN)(1) << PAGE_VALID_SHIFT)
#define PAGE_DIRTY    ((UINTN)(1) << PAGE_DIRTY_SHIFT)
#define PAGE_PLV      ((UINTN)(3) << PAGE_PLV_SHIFT)
#define PAGE_GLOBAL   ((UINTN)(1) << PAGE_GLOBAL_SHIFT)
#define PAGE_HUGE     ((UINTN)(1) << PAGE_HUGE_SHIFT)
#define PAGE_HGLOBAL  ((UINTN)(1) << PAGE_HGLOBAL_SHIFT)
#define PAGE_NO_READ  ((UINTN)(1) << PAGE_NO_READ_SHIFT)
#define PAGE_NO_EXEC  ((UINTN)(1) << PAGE_NO_EXEC_SHIFT)
#define PAGE_RPLV     ((UINTN)(1) << PAGE_RPLV_SHIFT)
#define CACHE_MASK    ((UINTN)(3) << CACHE_SHIFT)
#define PFN_SHIFT     (EFI_PAGE_SHIFT - 12 + PAGE_PFN_SHIFT)

#define PLV_KERNEL  0
#define PLV_USER    3

#define PAGE_USER    (PLV_USER << PAGE_PLV_SHIFT)
#define PAGE_KERNEL  (PLV_KERN << PAGE_PLV_SHIFT)

#define CACHE_SUC  (0 << CACHE_SHIFT) // Strong-ordered UnCached
#define CACHE_CC   (1 << CACHE_SHIFT) // Coherent Cached
#define CACHE_WUC  (2 << CACHE_SHIFT) // Weak-ordered UnCached

//
// Config CSR registers
//
#define LOONGARCH_CSR_CPUID   0x20    // CPU core ID
#define LOONGARCH_CSR_PRCFG1  0x21    // Config1
#define LOONGARCH_CSR_PRCFG2  0x22    // Config2
#define LOONGARCH_CSR_PRCFG3  0x23    // Config3

//
// Kscratch registers
//
#define LOONGARCH_CSR_KS0  0x30
#define LOONGARCH_CSR_KS1  0x31
#define LOONGARCH_CSR_KS2  0x32
#define LOONGARCH_CSR_KS3  0x33
#define LOONGARCH_CSR_KS4  0x34
#define LOONGARCH_CSR_KS5  0x35
#define LOONGARCH_CSR_KS6  0x36
#define LOONGARCH_CSR_KS7  0x37
#define LOONGARCH_CSR_KS8  0x38

//
// Stable timer registers
//
#define LOONGARCH_CSR_TMID           0x40  // Timer ID
#define LOONGARCH_CSR_TMCFG          0x41
#define LOONGARCH_CSR_TMCFG_EN       (1ULL << 0)
#define LOONGARCH_CSR_TMCFG_PERIOD   (1ULL << 1)
#define LOONGARCH_CSR_TMCFG_TIMEVAL  (0x3FFFFFFFFFFFULL << 2)
#define LOONGARCH_CSR_TVAL           0x42    // Timer value
#define LOONGARCH_CSR_CNTC           0x43    // Timer offset
#define LOONGARCH_CSR_TINTCLR        0x44    // Timer interrupt clear

//
// TLB refill exception base address
//
#define LOONGARCH_CSR_TLBREBASE  0x88    // TLB refill exception entry
#define LOONGARCH_CSR_TLBRBADV   0x89    // TLB refill badvaddr
#define LOONGARCH_CSR_TLBRERA    0x8a    // TLB refill ERA
#define LOONGARCH_CSR_TLBRSAVE   0x8b    // KScratch for TLB refill exception
#define LOONGARCH_CSR_TLBRELO0   0x8c    // TLB refill entrylo0
#define LOONGARCH_CSR_TLBRELO1   0x8d    // TLB refill entrylo1
#define LOONGARCH_CSR_TLBREHI    0x8e    // TLB refill entryhi

//
// Direct map windows registers
//
#define LOONGARCH_CSR_DMWIN0  0x180   // 64 direct map win0: MEM & IF
#define LOONGARCH_CSR_DMWIN1  0x181   // 64 direct map win1: MEM & IF
#define LOONGARCH_CSR_DMWIN2  0x182   // 64 direct map win2: MEM
#define LOONGARCH_CSR_DMWIN3  0x183   // 64 direct map win3: MEM
//
// CSR register numbers end
//

//
// IOCSR register numbers
//
#define LOONGARCH_IOCSR_FEATURES  0x8
#define  IOCSRF_TEMP              (1ULL << 0)
#define  IOCSRF_NODECNT           (1ULL << 1)
#define  IOCSRF_MSI               (1ULL << 2)
#define  IOCSRF_EXTIOI            (1ULL << 3)
#define  IOCSRF_CSRIPI            (1ULL << 4)
#define  IOCSRF_FREQCSR           (1ULL << 5)
#define  IOCSRF_FREQSCALE         (1ULL << 6)
#define  IOCSRF_DVFSV1            (1ULL << 7)
#define  IOCSRF_EXTIOI_DECODE     (1ULL << 9)
#define  IOCSRF_FLATMODE          (1ULL << 10)
#define  IOCSRF_VM                (1ULL << 11)

#define LOONGARCH_IOCSR_VENDOR  0x10

#define LOONGARCH_IOCSR_CPUNAME  0x20

#define LOONGARCH_IOCSR_NODECNT  0x408

#define LOONGARCH_IOCSR_MISC_FUNC     0x420
#define  IOCSR_MISC_FUNC_TIMER_RESET  (1ULL << 21)
#define  IOCSR_MISC_FUNC_EXT_IOI_EN   (1ULL << 48)

#define LOONGARCH_IOCSR_CPUTEMP  0x428

//
// PerCore CSR, only accessable by local cores
//
#define LOONGARCH_IOCSR_IPI_STATUS  0x1000
#define LOONGARCH_IOCSR_IPI_EN      0x1004
#define LOONGARCH_IOCSR_IPI_SET     0x1008
#define LOONGARCH_IOCSR_IPI_CLEAR   0x100c
#define LOONGARCH_IOCSR_MBUF0       0x1020
#define LOONGARCH_IOCSR_MBUF1       0x1028
#define LOONGARCH_IOCSR_MBUF2       0x1030
#define LOONGARCH_IOCSR_MBUF3       0x1038

#define LOONGARCH_IOCSR_IPI_SEND   0x1040
#define  IOCSR_IPI_SEND_IP_SHIFT   0
#define  IOCSR_IPI_SEND_CPU_SHIFT  16
#define  IOCSR_IPI_SEND_BLOCKING   (1ULL << 31)

#define LOONGARCH_IOCSR_MBUF_SEND   0x1048
#define  IOCSR_MBUF_SEND_BLOCKING   (1ULL << 31)
#define  IOCSR_MBUF_SEND_BOX_SHIFT  2
#define  IOCSR_MBUF_SEND_BOX_LO(box)  (box << 1)
#define  IOCSR_MBUF_SEND_BOX_HI(box)  ((box << 1) + 1)
#define  IOCSR_MBUF_SEND_CPU_SHIFT  16
#define  IOCSR_MBUF_SEND_BUF_SHIFT  32
#define  IOCSR_MBUF_SEND_H32_MASK   0xFFFFFFFF00000000ULL

#define LOONGARCH_IOCSR_ANY_SEND    0x1158
#define  IOCSR_ANY_SEND_BLOCKING    (1ULL << 31)
#define  IOCSR_ANY_SEND_CPU_SHIFT   16
#define  IOCSR_ANY_SEND_MASK_SHIFT  27
#define  IOCSR_ANY_SEND_BUF_SHIFT   32
#define  IOCSR_ANY_SEND_H32_MASK    0xFFFFFFFF00000000ULL

//
// Register offset and bit definition for CSR access
//
#define LOONGARCH_IOCSR_TIMER_CFG   0x1060
#define LOONGARCH_IOCSR_TIMER_TICK  0x1070
#define  IOCSR_TIMER_CFG_RESERVED   BIT63
#define  IOCSR_TIMER_CFG_PERIODIC   BIT62
#define  IOCSR_TIMER_CFG_EN         BIT61
#define  IOCSR_TIMER_MASK           0x0FFFFFFFFFFFFULL
#define  IOCSR_TIMER_INITVAL_RST    (0xFFFFULL << 48)
//
// IOCSR register numbers end
//

//
// Invalid addr with global=1 or matched asid in current TLB
//
#define INVTLB_ADDR_GTRUE_OR_ASID  0x6

//
// Bits 8 and 9 of FPU Status Register specify the rounding mode
//
#define FPU_CSR_RM  0x300
#define FPU_CSR_RN  0x000   // nearest
#define FPU_CSR_RZ  0x100   // towards zero
#define FPU_CSR_RU  0x200   // towards +Infinity
#define FPU_CSR_RD  0x300   // towards -Infinity

#define DEFAULT_PAGE_SIZE     0x0c
#define CSR_TLBIDX_SIZE_MASK  0x3f000000
#define CSR_TLBIDX_PS_SHIFT   24
#define CSR_TLBIDX_SIZE       CSR_TLBIDX_PS_SHIFT
#define CSR_TLBREHI_PS_SHIFT  0x0
#define CSR_TLBREHI_PS        0x3f

#endif
