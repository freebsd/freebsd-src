/** @file
  MSR Definitions for Intel Atom processors based on the Goldmont Plus microarchitecture.

  Provides defines for Machine Specific Registers(MSR) indexes. Data structures
  are provided for MSRs that contain one or more bit fields.  If the MSR value
  returned is a single 32-bit or 64-bit value, then a data structure is not
  provided for that MSR.

  Copyright (c) 2018 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  Intel(R) 64 and IA-32 Architectures Software Developer's Manual, Volume 4,
  May 2018, Volume 4: Model-Specific-Registers (MSR)

**/

#ifndef __GOLDMONT_PLUS_MSR_H__
#define __GOLDMONT_PLUS_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel Atom processors based on the Goldmont plus microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_GOLDMONT_PLUS_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x7A    \
    )                       \
   )

/**
  Core. (R/W) See Table 2-2. See Section 18.6.2.4, "Processor Event Based
  Sampling (PEBS).".

  @param  ECX  MSR_GOLDMONT_PLUS_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PLUS_PEBS_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_GOLDMONT_PLUS_PEBS_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_GOLDMONT_PLUS_PEBS_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_GOLDMONT_PLUS_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_GOLDMONT_PLUS_PEBS_ENABLE, Msr.Uint64);
  @endcode
**/
#define MSR_GOLDMONT_PLUS_PEBS_ENABLE            0x000003F1

/**
  MSR information returned for MSR index #MSR_GOLDMONT_PLUS_PEBS_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Enable PEBS trigger and recording for the programmed event
    /// (precise or otherwise) on IA32_PMC0.
    ///
    UINT32  Fix_Me_1:1;
    ///
    /// [Bit 1] Enable PEBS trigger and recording for the programmed event
    /// (precise or otherwise) on IA32_PMC1.
    ///
    UINT32  Fix_Me_2:1;
    ///
    /// [Bit 2] Enable PEBS trigger and recording for the programmed event
    /// (precise or otherwise) on IA32_PMC2.
    ///
    UINT32  Fix_Me_3:1;
    ///
    /// [Bit 3] Enable PEBS trigger and recording for the programmed event
    /// (precise or otherwise) on IA32_PMC3.
    ///
    UINT32  Fix_Me_4:1;
    UINT32  Reserved1:28;
    ///
    /// [Bit 32] Enable PEBS trigger and recording for IA32_FIXED_CTR0.
    ///
    UINT32  Fix_Me_5:1;
    ///
    /// [Bit 33] Enable PEBS trigger and recording for IA32_FIXED_CTR1.
    ///
    UINT32  Fix_Me_6:1;
    ///
    /// [Bit 34] Enable PEBS trigger and recording for IA32_FIXED_CTR2.
    ///
    UINT32  Fix_Me_7:1;
    UINT32  Reserved2:29;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_GOLDMONT_PLUS_PEBS_ENABLE_REGISTER;


/**
  Core. Last Branch Record N From IP (R/W) One of the three MSRs that make up
  the first entry of the 32-entry LBR stack. The From_IP part of the stack
  contains pointers to the source instruction. See also: -  Last Branch Record
  Stack TOS at 1C9H. -  Section 17.7, "Last Branch, Call Stack, Interrupt, and
  .. Exception Recording for Processors based on Goldmont Plus
  Microarchitecture.".

  @param  ECX  MSR_GOLDMONT_PLUS_LASTBRANCH_N_FROM_IP (0x0000068N)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PLUS_LASTBRANCH_N_FROM_IP);
  AsmWriteMsr64 (MSR_GOLDMONT_PLUS_LASTBRANCH_N_FROM_IP, Msr);
  @endcode
**/
#define MSR_GOLDMONT_PLUS_LASTBRANCH_0_FROM_IP    0x00000680
#define MSR_GOLDMONT_PLUS_LASTBRANCH_1_FROM_IP    0x00000681
#define MSR_GOLDMONT_PLUS_LASTBRANCH_2_FROM_IP    0x00000682
#define MSR_GOLDMONT_PLUS_LASTBRANCH_3_FROM_IP    0x00000683
#define MSR_GOLDMONT_PLUS_LASTBRANCH_4_FROM_IP    0x00000684
#define MSR_GOLDMONT_PLUS_LASTBRANCH_5_FROM_IP    0x00000685
#define MSR_GOLDMONT_PLUS_LASTBRANCH_6_FROM_IP    0x00000686
#define MSR_GOLDMONT_PLUS_LASTBRANCH_7_FROM_IP    0x00000687
#define MSR_GOLDMONT_PLUS_LASTBRANCH_8_FROM_IP    0x00000688
#define MSR_GOLDMONT_PLUS_LASTBRANCH_9_FROM_IP    0x00000689
#define MSR_GOLDMONT_PLUS_LASTBRANCH_10_FROM_IP   0x0000068A
#define MSR_GOLDMONT_PLUS_LASTBRANCH_11_FROM_IP   0x0000068B
#define MSR_GOLDMONT_PLUS_LASTBRANCH_12_FROM_IP   0x0000068C
#define MSR_GOLDMONT_PLUS_LASTBRANCH_13_FROM_IP   0x0000068D
#define MSR_GOLDMONT_PLUS_LASTBRANCH_14_FROM_IP   0x0000068E
#define MSR_GOLDMONT_PLUS_LASTBRANCH_15_FROM_IP   0x0000068F
#define MSR_GOLDMONT_PLUS_LASTBRANCH_16_FROM_IP   0x00000690
#define MSR_GOLDMONT_PLUS_LASTBRANCH_17_FROM_IP   0x00000691
#define MSR_GOLDMONT_PLUS_LASTBRANCH_18_FROM_IP   0x00000692
#define MSR_GOLDMONT_PLUS_LASTBRANCH_19_FROM_IP   0x00000693
#define MSR_GOLDMONT_PLUS_LASTBRANCH_20_FROM_IP   0x00000694
#define MSR_GOLDMONT_PLUS_LASTBRANCH_21_FROM_IP   0x00000695
#define MSR_GOLDMONT_PLUS_LASTBRANCH_22_FROM_IP   0x00000696
#define MSR_GOLDMONT_PLUS_LASTBRANCH_23_FROM_IP   0x00000697
#define MSR_GOLDMONT_PLUS_LASTBRANCH_24_FROM_IP   0x00000698
#define MSR_GOLDMONT_PLUS_LASTBRANCH_25_FROM_IP   0x00000699
#define MSR_GOLDMONT_PLUS_LASTBRANCH_26_FROM_IP   0x0000069A
#define MSR_GOLDMONT_PLUS_LASTBRANCH_27_FROM_IP   0x0000069B
#define MSR_GOLDMONT_PLUS_LASTBRANCH_28_FROM_IP   0x0000069C
#define MSR_GOLDMONT_PLUS_LASTBRANCH_29_FROM_IP   0x0000069D
#define MSR_GOLDMONT_PLUS_LASTBRANCH_30_FROM_IP   0x0000069E
#define MSR_GOLDMONT_PLUS_LASTBRANCH_31_FROM_IP   0x0000069F

/**
  Core. Last Branch Record N To IP (R/W) One of the three MSRs that make up
  the first entry of the 32-entry LBR stack. The To_IP part of the stack
  contains pointers to the Destination instruction. See also: - Section 17.7,
  "Last Branch, Call Stack, Interrupt, and Exception Recording for Processors
  based on Goldmont Plus Microarchitecture.".

  @param  ECX  MSR_GOLDMONT_PLUS_LASTBRANCH_N_TO_IP (0x000006C0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PLUS_LASTBRANCH_N_TO_IP);
  AsmWriteMsr64 (MSR_GOLDMONT_PLUS_LASTBRANCH_N_TO_IP, Msr);
  @endcode
**/
#define MSR_GOLDMONT_PLUS_LASTBRANCH_0_TO_IP      0x000006C0
#define MSR_GOLDMONT_PLUS_LASTBRANCH_1_TO_IP      0x000006C1
#define MSR_GOLDMONT_PLUS_LASTBRANCH_2_TO_IP      0x000006C2
#define MSR_GOLDMONT_PLUS_LASTBRANCH_3_TO_IP      0x000006C3
#define MSR_GOLDMONT_PLUS_LASTBRANCH_4_TO_IP      0x000006C4
#define MSR_GOLDMONT_PLUS_LASTBRANCH_5_TO_IP      0x000006C5
#define MSR_GOLDMONT_PLUS_LASTBRANCH_6_TO_IP      0x000006C6
#define MSR_GOLDMONT_PLUS_LASTBRANCH_7_TO_IP      0x000006C7
#define MSR_GOLDMONT_PLUS_LASTBRANCH_8_TO_IP      0x000006C8
#define MSR_GOLDMONT_PLUS_LASTBRANCH_9_TO_IP      0x000006C9
#define MSR_GOLDMONT_PLUS_LASTBRANCH_10_TO_IP     0x000006CA
#define MSR_GOLDMONT_PLUS_LASTBRANCH_11_TO_IP     0x000006CB
#define MSR_GOLDMONT_PLUS_LASTBRANCH_12_TO_IP     0x000006CC
#define MSR_GOLDMONT_PLUS_LASTBRANCH_13_TO_IP     0x000006CD
#define MSR_GOLDMONT_PLUS_LASTBRANCH_14_TO_IP     0x000006CE
#define MSR_GOLDMONT_PLUS_LASTBRANCH_15_TO_IP     0x000006CF
#define MSR_GOLDMONT_PLUS_LASTBRANCH_16_TO_IP     0x000006D0
#define MSR_GOLDMONT_PLUS_LASTBRANCH_17_TO_IP     0x000006D1
#define MSR_GOLDMONT_PLUS_LASTBRANCH_18_TO_IP     0x000006D2
#define MSR_GOLDMONT_PLUS_LASTBRANCH_19_TO_IP     0x000006D3
#define MSR_GOLDMONT_PLUS_LASTBRANCH_20_TO_IP     0x000006D4
#define MSR_GOLDMONT_PLUS_LASTBRANCH_21_TO_IP     0x000006D5
#define MSR_GOLDMONT_PLUS_LASTBRANCH_22_TO_IP     0x000006D6
#define MSR_GOLDMONT_PLUS_LASTBRANCH_23_TO_IP     0x000006D7
#define MSR_GOLDMONT_PLUS_LASTBRANCH_24_TO_IP     0x000006D8
#define MSR_GOLDMONT_PLUS_LASTBRANCH_25_TO_IP     0x000006D9
#define MSR_GOLDMONT_PLUS_LASTBRANCH_26_TO_IP     0x000006DA
#define MSR_GOLDMONT_PLUS_LASTBRANCH_27_TO_IP     0x000006DB
#define MSR_GOLDMONT_PLUS_LASTBRANCH_28_TO_IP     0x000006DC
#define MSR_GOLDMONT_PLUS_LASTBRANCH_29_TO_IP     0x000006DD
#define MSR_GOLDMONT_PLUS_LASTBRANCH_30_TO_IP     0x000006DE
#define MSR_GOLDMONT_PLUS_LASTBRANCH_31_TO_IP     0x000006DF


/**
  Core. Last Branch Record N Additional Information (R/W) One of the three
  MSRs that make up the first entry of the 32-entry LBR stack. This part of
  the stack contains flag and elapsed cycle information. See also: -  Last
  Branch Record Stack TOS at 1C9H. -  Section 17.9.1, "LBR Stack.".

  @param  ECX  MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_N (0x00000DCN)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_N);
  AsmWriteMsr64 (MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_N, Msr);
  @endcode
**/
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_0      0x00000DC0
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_1      0x00000DC1
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_2      0x00000DC2
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_3      0x00000DC3
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_4      0x00000DC4
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_5      0x00000DC5
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_6      0x00000DC6
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_7      0x00000DC7
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_8      0x00000DC8
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_9      0x00000DC9
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_10     0x00000DCA
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_11     0x00000DCB
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_12     0x00000DCC
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_13     0x00000DCD
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_14     0x00000DCE
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_15     0x00000DCF
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_16     0x00000DD0
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_17     0x00000DD1
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_18     0x00000DD2
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_19     0x00000DD3
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_20     0x00000DD4
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_21     0x00000DD5
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_22     0x00000DD6
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_23     0x00000DD7
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_24     0x00000DD8
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_25     0x00000DD9
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_26     0x00000DDA
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_27     0x00000DDB
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_28     0x00000DDC
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_29     0x00000DDD
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_30     0x00000DDE
#define MSR_GOLDMONT_PLUS_LASTBRANCH_INFO_31     0x00000DDF

#endif
