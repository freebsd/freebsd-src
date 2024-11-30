/** @file
  CPUCFG definitions.

  Copyright (c) 2024, Loongson Technology Corporation Limited. All rights reserved.<BR>

  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#ifndef CPUCFG_H_
#define CPUCFG_H_

/**
  CPUCFG REG0 Information

  @code
  CPUCFG_REG0_INFO_DATA
 **/
#define CPUCFG_REG0_INFO  0x0

/**
  CPUCFG REG0 Information returned data.
  #CPUCFG_REG0_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 31:0] Processor Identity.
    ///
    UINT32    PRID : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_REG0_INFO_DATA;

/**
  CPUCFG REG1 Information

  @code
  CPUCFG_REG1_INFO_DATA
 **/
#define CPUCFG_REG1_INFO  0x1

/**
  CPUCFG REG1 Information returned data.
  #CPUCFG_REG1_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 1:0] Architecture:
    ///           2'b00 indicates the implementation of simplified LoongAarch32;
    ///           2'b01 indicates the implementation of LoongAarch32;
    ///           2'b10 indicates the implementation of LoongAarch64;
    ///           2'b11 reserved;
    ///
    UINT32    ARCH      : 2;
    ///
    /// [Bit 2] Paging mapping mode. A value of 1 indicates the processor MMU supports
    /// page mapping mode.
    ///
    UINT32    PGMMU     : 1;
    ///
    /// [Bit 3] A value of 1 indicates the processor supports the IOCSR instruction.
    ///
    UINT32    IOCSR     : 1;
    ///
    /// [Bit 11:4] Physical address bits. The supported physical address bits PALEN value
    /// minus 1.
    ///
    UINT32    PALEN     : 8;
    ///
    /// [Bit 19:12] Virtual address bits. The supported virtual address bits VALEN value
    /// minus 1.
    ///
    UINT32    VALEN     : 8;
    ///
    /// [Bit 20] Non-aligned Memory Access. A value of 1 indicates the processor supports
    /// non-aligned memory access.
    ///
    UINT32    UAL       : 1;
    ///
    /// [Bit 21] Page Read Inhibit. A value of 1 indicates the processor supports page
    /// attribute of "Read Inhibit".
    ///
    UINT32    RI        : 1;
    ///
    /// [Bit 22] Page Execution Protection. A value of 1 indicates the processor supports
    /// page attribute of "Execution Protection".
    ///
    UINT32    EP        : 1;
    ///
    /// [Bit 23] A value of 1 indicates the processor supports for page attributes of RPLV.
    ///
    UINT32    RPLV      : 1;
    ///
    /// [Bit 24] Huge Page. A value of 1 indicates the processor supports page attribute
    /// of huge page.
    ///
    UINT32    HP        : 1;
    ///
    /// [Bit 25] A value of 1 indicates that the string of processor product information
    /// is recorded at address 0 of the IOCSR access space.
    ///
    UINT32    IOCSR_BRD : 1;
    ///
    /// [Bit 26] A value of 1 indicates that the external interrupt uses the message
    /// interrupt mode, otherwise it is the level interrupt line mode.
    ///
    UINT32    MSG_INT   : 1;
    ///
    /// [Bit 31:27] Reserved.
    ///
    UINT32    Reserved  : 5;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_REG1_INFO_DATA;

/**
  CPUCFG REG2 Information

  @code
  CPUCFG_REG2_INFO_DATA
 **/
#define CPUCFG_REG2_INFO  0x2

/**
  CPUCFG REG2 Information returned data.
  #CPUCFG_REG2_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 0] Basic Floating-Point. A value of 1 indicates the processor supports basic
    /// floating-point instructions.
    ///
    UINT32    FP       : 1;
    ///
    /// [Bit 1] Sigle-Precision. A value of 1 indicates the processor supports sigle-precision
    /// floating-point numbers.
    ///
    UINT32    FP_SP    : 1;
    ///
    /// [Bit 2] Double-Precision. A value of 1 indicates the processor supports double-precision
    /// floating-point numbers.
    ///
    UINT32    FP_DP    : 1;
    ///
    /// [Bit 5:3] The version number of the floating-point arithmetic standard. 1 is the initial
    /// version number, indicating that it is compatible with the IEEE 754-2008 standard.
    ///
    UINT32    FP_ver   : 3;
    ///
    /// [Bit 6] 128-bit Vector Extension. A value of 1 indicates the processor supports 128-bit
    /// vector extension.
    ///
    UINT32    LSX      : 1;
    ///
    /// [Bit 7] 256-bit Vector Extension. A value of 1 indicates the processor supports 256-bit
    /// vector extension.
    ///
    UINT32    LASX     : 1;
    ///
    /// [Bit 8] Complex Vector Operation Instructions. A value of 1 indicates the processor supports
    /// complex vector operation instructions.
    ///
    UINT32    COMPLEX  : 1;
    ///
    /// [Bit 9] Encryption And Decryption Vector Instructions. A value of 1 indicates the processor
    /// supports encryption and decryption vector instructions.
    ///
    UINT32    CRYPTO   : 1;
    ///
    /// [Bit 10] Virtualization Expansion. A value of 1 indicates the processor supports
    /// virtualization expansion.
    ///
    UINT32    LVZ      : 1;
    ///
    /// [Bit 13:11] The version number of the virtualization hardware acceleration specification.
    /// 1 is the initial version number.
    ///
    UINT32    LVZ_ver  : 3;
    ///
    /// [Bit 14] Constant Frequency Counter And Timer. A value of 1 indicates the processor supports
    /// constant frequency counter and timer.
    ///
    UINT32    LLFTP    : 1;
    ///
    /// [Bit 17:15] Constant frequency counter and timer version number. 1 is the initial version.
    ///
    UINT32    LLTP_ver : 3;
    ///
    /// [Bit 18] X86 Binary Translation Extension. A value of 1 indicates the processor supports
    /// X86 binary translation extension.
    ///
    UINT32    LBT_X86  : 1;
    ///
    /// [Bit 19] ARM Binary Translation Extension. A value of 1 indicates the processor supports
    /// ARM binary translation extension.
    ///
    UINT32    LBT_ARM  : 1;
    ///
    /// [Bit 20] MIPS Binary Translation Extension. A value of 1 indicates the processor supports
    /// MIPS binary translation extension.
    ///
    UINT32    LBT_MIPS : 1;
    ///
    /// [Bit 21] Software Page Table Walking Instruction. A value of 1 indicates the processor
    /// supports software page table walking instruction.
    ///
    UINT32    LSPW     : 1;
    ///
    /// [Bit 22] Atomic Memory Access Instruction. A value of 1 indicates the processor supports
    /// AM* atomic memory access instruction.
    ///
    UINT32    LAM      : 1;
    ///
    /// [Bit 31:23] Reserved.
    ///
    UINT32    Reserved : 9;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_REG2_INFO_DATA;

/**
  CPUCFG REG3 Information

  @code
  CPUCFG_REG3_INFO_DATA
 **/
#define CPUCFG_REG3_INFO  0x3

/**
  CPUCFG REG3 Information returned data.
  #CPUCFG_REG3_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 0] Hardware Cache Coherent DMA. A value of 1 indicates the processor supports
    /// hardware cache coherent DMA.
    ///
    UINT32    CCDMA     : 1;
    ///
    /// [Bit 1] Store Fill Buffer. A value of 1 indicates the processor supports store fill
    /// buffer (SFB).
    ///
    UINT32    SFB       : 1;
    ///
    /// [Bit 2] Uncache Accelerate. A value of 1 indicates the processor supports uncache
    /// accelerate.
    ///
    UINT32    UCACC     : 1;
    ///
    /// [Bit 3] A value of 1 indicates the processor supports LL instruction to fetch exclusive
    /// block function.
    ///
    UINT32    LLEXC     : 1;
    ///
    /// [Bit 4] A value of 1 indicates the processor supports random delay function after SC
    /// instruction.
    ///
    UINT32    SCDLY     : 1;
    ///
    /// [Bit 5] A value of 1 indicates the processor supports LL automatic with dbar function.
    ///
    UINT32    LLDBAR    : 1;
    ///
    /// [Bit 6] A value of 1 indicates the processor supports the hardware maintains the
    /// consistency between ITLB and TLB.
    ///
    UINT32    ITLBT     : 1;
    ///
    /// [Bit 7] A value of 1 indicates the processor supports the hardware maintains the data
    /// consistency between ICache and DCache in one processor core.
    ///
    UINT32    ICACHET   : 1;
    ///
    /// [Bit 10:8] The maximum number of directory levels supported by the page walk instruction.
    ///
    UINT32    SPW_LVL   : 3;
    ///
    /// [Bit 11] A value of 1 indicates the processor supports the page walk instruction fills
    /// the TLB in half when it encounters a large page.
    ///
    UINT32    SPW_HP_HF : 1;
    ///
    /// [Bit 12] Virtual Address Range. A value of 1 indicates the processor supports the software
    /// configuration can be used to shorten the virtual address range.
    ///
    UINT32    RVA       : 1;
    ///
    /// [Bit 16:13] The maximum configurable virtual address is shortened by -1.
    ///
    UINT32    RVAMAX_1  : 4;
    ///
    /// [Bit 31:17] Reserved.
    ///
    UINT32    Reserved  : 15;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_REG3_INFO_DATA;

/**
  CPUCFG REG4 Information

  @code
  CPUCFG_REG4_INFO_DATA
 **/
#define CPUCFG_REG4_INFO  0x4

/**
  CPUCFG REG4 Information returned data.
  #CPUCFG_REG4_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 31:0] Constant frequency timer and the crystal frequency corresponding to the clock
    /// used by the timer.
    ///
    UINT32    CC_FREQ : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_REG4_INFO_DATA;

/**
  CPUCFG REG5 Information

  @code
  CPUCFG_REG5_INFO_DATA
 **/
#define CPUCFG_REG5_INFO  0x5

/**
  CPUCFG REG5 Information returned data.
  #CPUCFG_REG5_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 15:0] Constant frequency timer and the corresponding multiplication factor of the
    /// clock used by the timer.
    ///
    UINT32    CC_MUL : 16;
    ///
    /// [Bit 31:16] Constant frequency timer and the division coefficient corresponding to the
    /// clock used by the timer
    ///
    UINT32    CC_DIV : 16;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_REG5_INFO_DATA;

/**
  CPUCFG REG6 Information

  @code
  CPUCFG_REG6_INFO_DATA
 **/
#define CPUCFG_REG6_INFO  0x6

/**
  CPUCFG REG6 Information returned data.
  #CPUCFG_REG6_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 0] Performance Counter. A value of 1 indicates the processor supports performance
    /// counter.
    ///
    UINT32    PMP      : 1;
    ///
    /// [Bit 3:1] In the performance monitor, the architecture defines the version number of the
    /// event, and 1 is the initial version
    ///
    UINT32    PMVER    : 3;
    ///
    /// [Bit 7:4] Number of performance monitors minus 1.
    ///
    UINT32    PMNUM    : 4;
    ///
    /// [Bit 13:8] Number of bits of a performance monitor minus 1.
    ///
    UINT32    PMBITS   : 6;
    ///
    /// [Bit 14] A value of 1 indicates the processor supports reading performance counter in user mode.
    ///
    UINT32    UPM      : 1;
    ///
    /// [Bit 31:15] Reserved.
    ///
    UINT32    Reserved : 17;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_REG6_INFO_DATA;

/**
  CPUCFG REG16 Information

  @code
  CPUCFG_REG16_INFO_DATA
 **/
#define CPUCFG_REG16_INFO  0x10

/**
  CPUCFG REG16 Information returned data.
  #CPUCFG_REG16_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 0] A value of 1 indicates the processor has a first-level instruction cache
    /// or a first-level unified cache
    ///
    UINT32    L1_IU_Present   : 1;
    ///
    /// [Bit 1] A value of 1 indicates that the cache shown by L1 IU_Present is the
    /// unified cache.
    ///
    UINT32    L1_IU_Unify     : 1;
    ///
    /// [Bit 2] A value of 1 indicates the processor has a first-level data cache.
    ///
    UINT32    L1_D_Present    : 1;
    ///
    /// [Bit 3] A value of 1 indicates the processor has a second-level instruction cache
    /// or a second-level unified cache.
    ///
    UINT32    L2_IU_Present   : 1;
    ///
    /// [Bit 4] A value of 1 indicates that the cache shown by L2 IU_Present is the
    /// unified cache.
    ///
    UINT32    L2_IU_Unify     : 1;
    ///
    /// [Bit 5] A value of 1 indicates that the cache shown by L2 IU_Present is private
    /// to each core.
    ///
    UINT32    L2_IU_Private   : 1;
    ///
    /// [Bit 6] A value of 1 indicates that the cache shown by L2 IU_Present has an inclusive
    /// relationship to the lower levels (L1).
    ///
    UINT32    L2_IU_Inclusive : 1;
    ///
    /// [Bit 7] A value of 1 indicates the processor has a second-level data cache.
    ///
    UINT32    L2_D_Present    : 1;
    ///
    /// [Bit 8] A value of 1 indicates that the second-level data cache is private to each core.
    ///
    UINT32    L2_D_Private    : 1;
    ///
    /// [Bit 9] A value of 1 indicates that the second-level data cache has a containment
    /// relationship to the lower level (L1).
    ///
    UINT32    L2_D_Inclusive  : 1;
    ///
    /// [Bit 10] A value of 1 indicates the processor has a three-level instruction cache
    /// or a second-level unified Cache.
    ///
    UINT32    L3_IU_Present   : 1;
    ///
    /// [Bit 11] A value of 1 indicates that the cache shown by L3 IU_Present is the
    /// unified cache.
    ///
    UINT32    L3_IU_Unify     : 1;
    ///
    /// [Bit 12] A value of 1 indicates that the cache shown by L3 IU_Present is private
    /// to each core.
    ///
    UINT32    L3_IU_Private   : 1;
    ///
    /// [Bit 13] A value of 1 indicates that the cache shown by L3 IU_Present has an inclusive
    /// relationship to the lower levels (L1 and L2).
    ///
    UINT32    L3_IU_Inclusive : 1;
    ///
    /// [Bit 14] A value of 1 indicates the processor has a three-level data cache.
    ///
    UINT32    L3_D_Present    : 1;
    ///
    /// [Bit 15] A value of 1 indicates that the three-level data cache is private to each core.
    ///
    UINT32    L3_D_Private    : 1;
    ///
    /// [Bit 16] A value of 1 indicates that the three-level data cache has a containment
    /// relationship to the lower level (L1 and L2).
    ///
    UINT32    L3_D_Inclusive  : 1;
    ///
    /// [Bit 31:17] Reserved.
    ///
    UINT32    Reserved        : 15;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_REG16_INFO_DATA;

/**
  CPUCFG REG17, REG18, REG19 and REG20 Information

  @code
  CPUCFG_CACHE_INFO_DATA
 **/
#define CPUCFG_REG17_INFO  0x11 /// L1 unified cache.
#define CPUCFG_REG18_INFO  0x12 /// L1 data cache.
#define CPUCFG_REG19_INFO  0x13 /// L2 unified cache.
#define CPUCFG_REG20_INFO  0x14 /// L3 unified cache.

/**
  CPUCFG CACHE Information returned data.
  #CPUCFG_REG17_INFO
  #CPUCFG_REG18_INFO
  #CPUCFG_REG19_INFO
  #CPUCFG_REG20_INFO
 **/
typedef union {
  struct {
    ///
    /// [Bit 15:0] Number of channels minus 1.
    ///
    UINT32    Way_1         : 16;
    ///
    /// [Bit 23:16] Log2 (number of cache rows per channel).
    ///
    UINT32    Index_log2    : 8;
    ///
    /// [Bit 30:24] Log2 (cache row bytes).
    ///
    UINT32    Linesize_log2 : 7;
    ///
    /// [Bit 31] Reserved.
    ///
    UINT32    Reserved      : 1;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
} CPUCFG_CACHE_INFO_DATA;
#endif
