/** @file
  MSR Definitions for Intel processors based on the Haswell microarchitecture.

  Provides defines for Machine Specific Registers(MSR) indexes. Data structures
  are provided for MSRs that contain one or more bit fields.  If the MSR value
  returned is a single 32-bit or 64-bit value, then a data structure is not
  provided for that MSR.

  Copyright (c) 2016 - 2019, Intel Corporation. All rights reserved.<BR>
  SPDX-License-Identifier: BSD-2-Clause-Patent

  @par Specification Reference:
  Intel(R) 64 and IA-32 Architectures Software Developer's Manual, Volume 4,
  May 2018, Volume 4: Model-Specific-Registers (MSR)

**/

#ifndef __HASWELL_MSR_H__
#define __HASWELL_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel processors based on the Haswell microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_HASWELL_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x3C || \
    DisplayModel == 0x45 || \
    DisplayModel == 0x46    \
    )                       \
   )

/**
  Package.

  @param  ECX  MSR_HASWELL_PLATFORM_INFO (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_PLATFORM_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_PLATFORM_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_PLATFORM_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_PLATFORM_INFO);
  AsmWriteMsr64 (MSR_HASWELL_PLATFORM_INFO, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_PLATFORM_INFO is defined as MSR_PLATFORM_INFO in SDM.
**/
#define MSR_HASWELL_PLATFORM_INFO                0x000000CE

/**
  MSR information returned for MSR index #MSR_HASWELL_PLATFORM_INFO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:8;
    ///
    /// [Bits 15:8] Package. Maximum Non-Turbo Ratio (R/O)  The is the ratio
    /// of the frequency that invariant TSC runs at. Frequency = ratio * 100
    /// MHz.
    ///
    UINT32  MaximumNonTurboRatio:8;
    UINT32  Reserved2:12;
    ///
    /// [Bit 28] Package. Programmable Ratio Limit for Turbo Mode (R/O)  When
    /// set to 1, indicates that Programmable Ratio Limits for Turbo mode is
    /// enabled, and when set to 0, indicates Programmable Ratio Limits for
    /// Turbo mode is disabled.
    ///
    UINT32  RatioLimit:1;
    ///
    /// [Bit 29] Package. Programmable TDP Limit for Turbo Mode (R/O)  When
    /// set to 1, indicates that TDP Limits for Turbo mode are programmable,
    /// and when set to 0, indicates TDP Limit for Turbo mode is not
    /// programmable.
    ///
    UINT32  TDPLimit:1;
    UINT32  Reserved3:2;
    ///
    /// [Bit 32] Package. Low Power Mode Support (LPM) (R/O)  When set to 1,
    /// indicates that LPM is supported, and when set to 0, indicates LPM is
    /// not supported.
    ///
    UINT32  LowPowerModeSupport:1;
    ///
    /// [Bits 34:33] Package. Number of ConfigTDP Levels (R/O) 00: Only Base
    /// TDP level available. 01: One additional TDP level available. 02: Two
    /// additional TDP level available. 11: Reserved.
    ///
    UINT32  ConfigTDPLevels:2;
    UINT32  Reserved4:5;
    ///
    /// [Bits 47:40] Package. Maximum Efficiency Ratio (R/O)  The is the
    /// minimum ratio (maximum efficiency) that the processor can operates, in
    /// units of 100MHz.
    ///
    UINT32  MaximumEfficiencyRatio:8;
    ///
    /// [Bits 55:48] Package. Minimum Operating Ratio (R/O) Contains the
    /// minimum supported operating ratio in units of 100 MHz.
    ///
    UINT32  MinimumOperatingRatio:8;
    UINT32  Reserved5:8;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_PLATFORM_INFO_REGISTER;


/**
  Thread. Performance Event Select for Counter n (R/W) Supports all fields
  described inTable 2-2 and the fields below.

  @param  ECX  MSR_HASWELL_IA32_PERFEVTSELn
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_IA32_PERFEVTSEL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_IA32_PERFEVTSEL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_IA32_PERFEVTSEL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_IA32_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_IA32_PERFEVTSEL0, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_IA32_PERFEVTSEL0 is defined as IA32_PERFEVTSEL0 in SDM.
        MSR_HASWELL_IA32_PERFEVTSEL1 is defined as IA32_PERFEVTSEL1 in SDM.
        MSR_HASWELL_IA32_PERFEVTSEL3 is defined as IA32_PERFEVTSEL3 in SDM.
  @{
**/
#define MSR_HASWELL_IA32_PERFEVTSEL0             0x00000186
#define MSR_HASWELL_IA32_PERFEVTSEL1             0x00000187
#define MSR_HASWELL_IA32_PERFEVTSEL3             0x00000189
/// @}

/**
  MSR information returned for MSR indexes #MSR_HASWELL_IA32_PERFEVTSEL0,
  #MSR_HASWELL_IA32_PERFEVTSEL1, and #MSR_HASWELL_IA32_PERFEVTSEL3.
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Event Select: Selects a performance event logic unit.
    ///
    UINT32  EventSelect:8;
    ///
    /// [Bits 15:8] UMask: Qualifies the microarchitectural condition to
    /// detect on the selected event logic.
    ///
    UINT32  UMASK:8;
    ///
    /// [Bit 16] USR: Counts while in privilege level is not ring 0.
    ///
    UINT32  USR:1;
    ///
    /// [Bit 17] OS: Counts while in privilege level is ring 0.
    ///
    UINT32  OS:1;
    ///
    /// [Bit 18] Edge: Enables edge detection if set.
    ///
    UINT32  E:1;
    ///
    /// [Bit 19] PC: enables pin control.
    ///
    UINT32  PC:1;
    ///
    /// [Bit 20] INT: enables interrupt on counter overflow.
    ///
    UINT32  INT:1;
    ///
    /// [Bit 21] AnyThread: When set to 1, it enables counting the associated
    /// event conditions occurring across all logical processors sharing a
    /// processor core. When set to 0, the counter only increments the
    /// associated event conditions occurring in the logical processor which
    /// programmed the MSR.
    ///
    UINT32  ANY:1;
    ///
    /// [Bit 22] EN: enables the corresponding performance counter to commence
    /// counting when this bit is set.
    ///
    UINT32  EN:1;
    ///
    /// [Bit 23] INV: invert the CMASK.
    ///
    UINT32  INV:1;
    ///
    /// [Bits 31:24] CMASK: When CMASK is not zero, the corresponding
    /// performance counter increments each cycle if the event count is
    /// greater than or equal to the CMASK.
    ///
    UINT32  CMASK:8;
    UINT32  Reserved:32;
    ///
    /// [Bit 32] IN_TX: see Section 18.3.6.5.1 When IN_TX (bit 32) is set,
    /// AnyThread (bit 21) should be cleared to prevent incorrect results.
    ///
    UINT32  IN_TX:1;
    UINT32  Reserved2:31;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_IA32_PERFEVTSEL_REGISTER;


/**
  Thread. Performance Event Select for Counter 2 (R/W) Supports all fields
  described inTable 2-2 and the fields below.

  @param  ECX  MSR_HASWELL_IA32_PERFEVTSEL2 (0x00000188)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_IA32_PERFEVTSEL2_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_IA32_PERFEVTSEL2_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_IA32_PERFEVTSEL2_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_IA32_PERFEVTSEL2);
  AsmWriteMsr64 (MSR_HASWELL_IA32_PERFEVTSEL2, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_IA32_PERFEVTSEL2 is defined as IA32_PERFEVTSEL2 in SDM.
**/
#define MSR_HASWELL_IA32_PERFEVTSEL2             0x00000188

/**
  MSR information returned for MSR index #MSR_HASWELL_IA32_PERFEVTSEL2
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Event Select: Selects a performance event logic unit.
    ///
    UINT32  EventSelect:8;
    ///
    /// [Bits 15:8] UMask: Qualifies the microarchitectural condition to
    /// detect on the selected event logic.
    ///
    UINT32  UMASK:8;
    ///
    /// [Bit 16] USR: Counts while in privilege level is not ring 0.
    ///
    UINT32  USR:1;
    ///
    /// [Bit 17] OS: Counts while in privilege level is ring 0.
    ///
    UINT32  OS:1;
    ///
    /// [Bit 18] Edge: Enables edge detection if set.
    ///
    UINT32  E:1;
    ///
    /// [Bit 19] PC: enables pin control.
    ///
    UINT32  PC:1;
    ///
    /// [Bit 20] INT: enables interrupt on counter overflow.
    ///
    UINT32  INT:1;
    ///
    /// [Bit 21] AnyThread: When set to 1, it enables counting the associated
    /// event conditions occurring across all logical processors sharing a
    /// processor core. When set to 0, the counter only increments the
    /// associated event conditions occurring in the logical processor which
    /// programmed the MSR.
    ///
    UINT32  ANY:1;
    ///
    /// [Bit 22] EN: enables the corresponding performance counter to commence
    /// counting when this bit is set.
    ///
    UINT32  EN:1;
    ///
    /// [Bit 23] INV: invert the CMASK.
    ///
    UINT32  INV:1;
    ///
    /// [Bits 31:24] CMASK: When CMASK is not zero, the corresponding
    /// performance counter increments each cycle if the event count is
    /// greater than or equal to the CMASK.
    ///
    UINT32  CMASK:8;
    UINT32  Reserved:32;
    ///
    /// [Bit 32] IN_TX: see Section 18.3.6.5.1 When IN_TX (bit 32) is set,
    /// AnyThread (bit 21) should be cleared to prevent incorrect results.
    ///
    UINT32  IN_TX:1;
    ///
    /// [Bit 33] IN_TXCP: see Section 18.3.6.5.1 When IN_TXCP=1 & IN_TX=1 and
    /// in sampling, spurious PMI may occur and transactions may continuously
    /// abort near overflow conditions. Software should favor using IN_TXCP
    /// for counting over sampling. If sampling, software should use large
    /// "sample-after" value after clearing the counter configured to use
    /// IN_TXCP and also always reset the counter even when no overflow
    /// condition was reported.
    ///
    UINT32  IN_TXCP:1;
    UINT32  Reserved2:30;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_IA32_PERFEVTSEL2_REGISTER;


/**
  Thread. Last Branch Record Filtering Select Register (R/W).

  @param  ECX  MSR_HASWELL_LBR_SELECT (0x000001C8)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_LBR_SELECT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_LBR_SELECT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_LBR_SELECT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_LBR_SELECT);
  AsmWriteMsr64 (MSR_HASWELL_LBR_SELECT, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_LBR_SELECT is defined as MSR_LBR_SELECT in SDM.
**/
#define MSR_HASWELL_LBR_SELECT                   0x000001C8

/**
  MSR information returned for MSR index #MSR_HASWELL_LBR_SELECT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] CPL_EQ_0.
    ///
    UINT32  CPL_EQ_0:1;
    ///
    /// [Bit 1] CPL_NEQ_0.
    ///
    UINT32  CPL_NEQ_0:1;
    ///
    /// [Bit 2] JCC.
    ///
    UINT32  JCC:1;
    ///
    /// [Bit 3] NEAR_REL_CALL.
    ///
    UINT32  NEAR_REL_CALL:1;
    ///
    /// [Bit 4] NEAR_IND_CALL.
    ///
    UINT32  NEAR_IND_CALL:1;
    ///
    /// [Bit 5] NEAR_RET.
    ///
    UINT32  NEAR_RET:1;
    ///
    /// [Bit 6] NEAR_IND_JMP.
    ///
    UINT32  NEAR_IND_JMP:1;
    ///
    /// [Bit 7] NEAR_REL_JMP.
    ///
    UINT32  NEAR_REL_JMP:1;
    ///
    /// [Bit 8] FAR_BRANCH.
    ///
    UINT32  FAR_BRANCH:1;
    ///
    /// [Bit 9] EN_CALL_STACK.
    ///
    UINT32  EN_CALL_STACK:1;
    UINT32  Reserved1:22;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_LBR_SELECT_REGISTER;


/**
  Package. Package C6/C7 Interrupt Response Limit 1 (R/W)  This MSR defines
  the interrupt response time limit used by the processor to manage transition
  to package C6 or C7 state. The latency programmed in this register is for
  the shorter-latency sub C-states used by an MWAIT hint to C6 or C7 state.
  Note: C-state values are processor specific C-state code names, unrelated to
  MWAIT extension C-state parameters or ACPI C-States.

  @param  ECX  MSR_HASWELL_PKGC_IRTL1 (0x0000060B)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKGC_IRTL1_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKGC_IRTL1_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_PKGC_IRTL1_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_PKGC_IRTL1);
  AsmWriteMsr64 (MSR_HASWELL_PKGC_IRTL1, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_PKGC_IRTL1 is defined as MSR_PKGC_IRTL1 in SDM.
**/
#define MSR_HASWELL_PKGC_IRTL1                   0x0000060B

/**
  MSR information returned for MSR index #MSR_HASWELL_PKGC_IRTL1
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 9:0] Interrupt response time limit (R/W)  Specifies the limit
    /// that should be used to decide if the package should be put into a
    /// package C6 or C7 state.
    ///
    UINT32  InterruptResponseTimeLimit:10;
    ///
    /// [Bits 12:10] Time Unit (R/W) Specifies the encoding value of time unit
    /// of the interrupt response time limit. See Table 2-19 for supported
    /// time unit encodings.
    ///
    UINT32  TimeUnit:3;
    UINT32  Reserved1:2;
    ///
    /// [Bit 15] Valid (R/W)  Indicates whether the values in bits 12:0 are
    /// valid and can be used by the processor for package C-sate management.
    ///
    UINT32  Valid:1;
    UINT32  Reserved2:16;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_PKGC_IRTL1_REGISTER;


/**
  Package. Package C6/C7 Interrupt Response Limit 2 (R/W)  This MSR defines
  the interrupt response time limit used by the processor to manage transition
  to package C6 or C7 state. The latency programmed in this register is for
  the longer-latency sub Cstates used by an MWAIT hint to C6 or C7 state.
  Note: C-state values are processor specific C-state code names, unrelated to
  MWAIT extension C-state parameters or ACPI C-States.

  @param  ECX  MSR_HASWELL_PKGC_IRTL2 (0x0000060C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKGC_IRTL2_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKGC_IRTL2_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_PKGC_IRTL2_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_PKGC_IRTL2);
  AsmWriteMsr64 (MSR_HASWELL_PKGC_IRTL2, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_PKGC_IRTL2 is defined as MSR_PKGC_IRTL2 in SDM.
**/
#define MSR_HASWELL_PKGC_IRTL2                   0x0000060C

/**
  MSR information returned for MSR index #MSR_HASWELL_PKGC_IRTL2
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 9:0] Interrupt response time limit (R/W) Specifies the limit
    /// that should be used to decide if the package should be put into a
    /// package C6 or C7 state.
    ///
    UINT32  InterruptResponseTimeLimit:10;
    ///
    /// [Bits 12:10] Time Unit (R/W) Specifies the encoding value of time unit
    /// of the interrupt response time limit. See Table 2-19 for supported
    /// time unit encodings.
    ///
    UINT32  TimeUnit:3;
    UINT32  Reserved1:2;
    ///
    /// [Bit 15] Valid (R/W)  Indicates whether the values in bits 12:0 are
    /// valid and can be used by the processor for package C-sate management.
    ///
    UINT32  Valid:1;
    UINT32  Reserved2:16;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_PKGC_IRTL2_REGISTER;


/**
  Package. PKG Perf Status (R/O) See Section 14.9.3, "Package RAPL Domain.".

  @param  ECX  MSR_HASWELL_PKG_PERF_STATUS (0x00000613)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_PKG_PERF_STATUS);
  @endcode
  @note MSR_HASWELL_PKG_PERF_STATUS is defined as MSR_PKG_PERF_STATUS in SDM.
**/
#define MSR_HASWELL_PKG_PERF_STATUS              0x00000613


/**
  Package. DRAM Energy Status (R/O)  See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_HASWELL_DRAM_ENERGY_STATUS (0x00000619)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_DRAM_ENERGY_STATUS);
  @endcode
  @note MSR_HASWELL_DRAM_ENERGY_STATUS is defined as MSR_DRAM_ENERGY_STATUS in SDM.
**/
#define MSR_HASWELL_DRAM_ENERGY_STATUS           0x00000619


/**
  Package. DRAM Performance Throttling Status (R/O) See Section 14.9.5, "DRAM
  RAPL Domain.".

  @param  ECX  MSR_HASWELL_DRAM_PERF_STATUS (0x0000061B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_DRAM_PERF_STATUS);
  @endcode
  @note MSR_HASWELL_DRAM_PERF_STATUS is defined as MSR_DRAM_PERF_STATUS in SDM.
**/
#define MSR_HASWELL_DRAM_PERF_STATUS             0x0000061B


/**
  Package. Base TDP Ratio (R/O).

  @param  ECX  MSR_HASWELL_CONFIG_TDP_NOMINAL (0x00000648)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_CONFIG_TDP_NOMINAL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_CONFIG_TDP_NOMINAL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_CONFIG_TDP_NOMINAL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_CONFIG_TDP_NOMINAL);
  @endcode
  @note MSR_HASWELL_CONFIG_TDP_NOMINAL is defined as MSR_CONFIG_TDP_NOMINAL in SDM.
**/
#define MSR_HASWELL_CONFIG_TDP_NOMINAL           0x00000648

/**
  MSR information returned for MSR index #MSR_HASWELL_CONFIG_TDP_NOMINAL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Config_TDP_Base Base TDP level ratio to be used for this
    /// specific processor (in units of 100 MHz).
    ///
    UINT32  Config_TDP_Base:8;
    UINT32  Reserved1:24;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_CONFIG_TDP_NOMINAL_REGISTER;


/**
  Package. ConfigTDP Level 1 ratio and power level (R/O).

  @param  ECX  MSR_HASWELL_CONFIG_TDP_LEVEL1 (0x00000649)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_CONFIG_TDP_LEVEL1_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_CONFIG_TDP_LEVEL1_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_CONFIG_TDP_LEVEL1_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_CONFIG_TDP_LEVEL1);
  @endcode
  @note MSR_HASWELL_CONFIG_TDP_LEVEL1 is defined as MSR_CONFIG_TDP_LEVEL1 in SDM.
**/
#define MSR_HASWELL_CONFIG_TDP_LEVEL1            0x00000649

/**
  MSR information returned for MSR index #MSR_HASWELL_CONFIG_TDP_LEVEL1
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] PKG_TDP_LVL1. Power setting for ConfigTDP Level 1.
    ///
    UINT32  PKG_TDP_LVL1:15;
    UINT32  Reserved1:1;
    ///
    /// [Bits 23:16] Config_TDP_LVL1_Ratio. ConfigTDP level 1 ratio to be used
    /// for this specific processor.
    ///
    UINT32  Config_TDP_LVL1_Ratio:8;
    UINT32  Reserved2:8;
    ///
    /// [Bits 46:32] PKG_MAX_PWR_LVL1. Max Power setting allowed for ConfigTDP
    /// Level 1.
    ///
    UINT32  PKG_MAX_PWR_LVL1:15;
    ///
    /// [Bits 62:47] PKG_MIN_PWR_LVL1. MIN Power setting allowed for ConfigTDP
    /// Level 1.
    ///
    UINT32  PKG_MIN_PWR_LVL1:16;
    UINT32  Reserved3:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_CONFIG_TDP_LEVEL1_REGISTER;


/**
  Package. ConfigTDP Level 2 ratio and power level (R/O).

  @param  ECX  MSR_HASWELL_CONFIG_TDP_LEVEL2 (0x0000064A)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_CONFIG_TDP_LEVEL2_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_CONFIG_TDP_LEVEL2_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_CONFIG_TDP_LEVEL2_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_CONFIG_TDP_LEVEL2);
  @endcode
  @note MSR_HASWELL_CONFIG_TDP_LEVEL2 is defined as MSR_CONFIG_TDP_LEVEL2 in SDM.
**/
#define MSR_HASWELL_CONFIG_TDP_LEVEL2            0x0000064A

/**
  MSR information returned for MSR index #MSR_HASWELL_CONFIG_TDP_LEVEL2
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] PKG_TDP_LVL2. Power setting for ConfigTDP Level 2.
    ///
    UINT32  PKG_TDP_LVL2:15;
    UINT32  Reserved1:1;
    ///
    /// [Bits 23:16] Config_TDP_LVL2_Ratio. ConfigTDP level 2 ratio to be used
    /// for this specific processor.
    ///
    UINT32  Config_TDP_LVL2_Ratio:8;
    UINT32  Reserved2:8;
    ///
    /// [Bits 46:32] PKG_MAX_PWR_LVL2. Max Power setting allowed for ConfigTDP
    /// Level 2.
    ///
    UINT32  PKG_MAX_PWR_LVL2:15;
    ///
    /// [Bits 62:47] PKG_MIN_PWR_LVL2. MIN Power setting allowed for ConfigTDP
    /// Level 2.
    ///
    UINT32  PKG_MIN_PWR_LVL2:16;
    UINT32  Reserved3:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_CONFIG_TDP_LEVEL2_REGISTER;


/**
  Package. ConfigTDP Control (R/W).

  @param  ECX  MSR_HASWELL_CONFIG_TDP_CONTROL (0x0000064B)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_CONFIG_TDP_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_CONFIG_TDP_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_CONFIG_TDP_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_CONFIG_TDP_CONTROL);
  AsmWriteMsr64 (MSR_HASWELL_CONFIG_TDP_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_CONFIG_TDP_CONTROL is defined as MSR_CONFIG_TDP_CONTROL in SDM.
**/
#define MSR_HASWELL_CONFIG_TDP_CONTROL           0x0000064B

/**
  MSR information returned for MSR index #MSR_HASWELL_CONFIG_TDP_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 1:0] TDP_LEVEL (RW/L) System BIOS can program this field.
    ///
    UINT32  TDP_LEVEL:2;
    UINT32  Reserved1:29;
    ///
    /// [Bit 31] Config_TDP_Lock (RW/L) When this bit is set, the content of
    /// this register is locked until a reset.
    ///
    UINT32  Config_TDP_Lock:1;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_CONFIG_TDP_CONTROL_REGISTER;


/**
  Package. ConfigTDP Control (R/W).

  @param  ECX  MSR_HASWELL_TURBO_ACTIVATION_RATIO (0x0000064C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_TURBO_ACTIVATION_RATIO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_TURBO_ACTIVATION_RATIO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_TURBO_ACTIVATION_RATIO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_TURBO_ACTIVATION_RATIO);
  AsmWriteMsr64 (MSR_HASWELL_TURBO_ACTIVATION_RATIO, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_TURBO_ACTIVATION_RATIO is defined as MSR_TURBO_ACTIVATION_RATIO in SDM.
**/
#define MSR_HASWELL_TURBO_ACTIVATION_RATIO       0x0000064C

/**
  MSR information returned for MSR index #MSR_HASWELL_TURBO_ACTIVATION_RATIO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] MAX_NON_TURBO_RATIO (RW/L) System BIOS can program this
    /// field.
    ///
    UINT32  MAX_NON_TURBO_RATIO:8;
    UINT32  Reserved1:23;
    ///
    /// [Bit 31] TURBO_ACTIVATION_RATIO_Lock (RW/L) When this bit is set, the
    /// content of this register is locked until a reset.
    ///
    UINT32  TURBO_ACTIVATION_RATIO_Lock:1;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_TURBO_ACTIVATION_RATIO_REGISTER;


/**
  Core. C-State Configuration Control (R/W) Note: C-state values are processor
  specific C-state code names, unrelated to MWAIT extension C-state parameters
  or ACPI Cstates. `See http://biosbits.org. <http://biosbits.org>`__.

  @param  ECX  MSR_HASWELL_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_HASWELL_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_HASWELL_PKG_CST_CONFIG_CONTROL       0x000000E2

/**
  MSR information returned for MSR index #MSR_HASWELL_PKG_CST_CONFIG_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Package C-State Limit (R/W) Specifies the lowest
    /// processor-specific C-state code name (consuming the least power) for
    /// the package. The default is set as factory-configured package C-state
    /// limit. The following C-state code name encodings are supported: 0000b:
    /// C0/C1 (no package C-state support) 0001b: C2 0010b: C3 0011b: C6
    /// 0100b: C7 0101b: C7s Package C states C7 are not available to
    /// processor with signature 06_3CH.
    ///
    UINT32  Limit:4;
    UINT32  Reserved1:6;
    ///
    /// [Bit 10] I/O MWAIT Redirection Enable (R/W).
    ///
    UINT32  IO_MWAIT:1;
    UINT32  Reserved2:4;
    ///
    /// [Bit 15] CFG Lock (R/WO).
    ///
    UINT32  CFGLock:1;
    UINT32  Reserved3:9;
    ///
    /// [Bit 25] C3 State Auto Demotion Enable (R/W).
    ///
    UINT32  C3AutoDemotion:1;
    ///
    /// [Bit 26] C1 State Auto Demotion Enable (R/W).
    ///
    UINT32  C1AutoDemotion:1;
    ///
    /// [Bit 27] Enable C3 Undemotion (R/W).
    ///
    UINT32  C3Undemotion:1;
    ///
    /// [Bit 28] Enable C1 Undemotion (R/W).
    ///
    UINT32  C1Undemotion:1;
    UINT32  Reserved4:3;
    UINT32  Reserved5:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_PKG_CST_CONFIG_CONTROL_REGISTER;


/**
  THREAD. Enhanced SMM Capabilities (SMM-RO) Reports SMM capability
  Enhancement. Accessible only while in SMM.

  @param  ECX  MSR_HASWELL_SMM_MCA_CAP (0x0000017D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_SMM_MCA_CAP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_SMM_MCA_CAP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_SMM_MCA_CAP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_SMM_MCA_CAP);
  AsmWriteMsr64 (MSR_HASWELL_SMM_MCA_CAP, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_SMM_MCA_CAP is defined as MSR_SMM_MCA_CAP in SDM.
**/
#define MSR_HASWELL_SMM_MCA_CAP                  0x0000017D

/**
  MSR information returned for MSR index #MSR_HASWELL_SMM_MCA_CAP
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:32;
    UINT32  Reserved2:26;
    ///
    /// [Bit 58] SMM_Code_Access_Chk (SMM-RO) If set to 1 indicates that the
    /// SMM code access restriction is supported and the
    /// MSR_SMM_FEATURE_CONTROL is supported.
    ///
    UINT32  SMM_Code_Access_Chk:1;
    ///
    /// [Bit 59] Long_Flow_Indication (SMM-RO) If set to 1 indicates that the
    /// SMM long flow indicator is supported and the MSR_SMM_DELAYED is
    /// supported.
    ///
    UINT32  Long_Flow_Indication:1;
    UINT32  Reserved3:4;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_SMM_MCA_CAP_REGISTER;


/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_HASWELL_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_TURBO_RATIO_LIMIT);
  @endcode
  @note MSR_HASWELL_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_HASWELL_TURBO_RATIO_LIMIT            0x000001AD

/**
  MSR information returned for MSR index #MSR_HASWELL_TURBO_RATIO_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Package. Maximum Ratio Limit for 1C Maximum turbo ratio
    /// limit of 1 core active.
    ///
    UINT32  Maximum1C:8;
    ///
    /// [Bits 15:8] Package. Maximum Ratio Limit for 2C Maximum turbo ratio
    /// limit of 2 core active.
    ///
    UINT32  Maximum2C:8;
    ///
    /// [Bits 23:16] Package. Maximum Ratio Limit for 3C Maximum turbo ratio
    /// limit of 3 core active.
    ///
    UINT32  Maximum3C:8;
    ///
    /// [Bits 31:24] Package. Maximum Ratio Limit for 4C Maximum turbo ratio
    /// limit of 4 core active.
    ///
    UINT32  Maximum4C:8;
    UINT32  Reserved:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_TURBO_RATIO_LIMIT_REGISTER;


/**
  Package. Uncore PMU global control.

  @param  ECX  MSR_HASWELL_UNC_PERF_GLOBAL_CTRL (0x00000391)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_PERF_GLOBAL_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_PERF_GLOBAL_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_UNC_PERF_GLOBAL_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_UNC_PERF_GLOBAL_CTRL);
  AsmWriteMsr64 (MSR_HASWELL_UNC_PERF_GLOBAL_CTRL, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_UNC_PERF_GLOBAL_CTRL is defined as MSR_UNC_PERF_GLOBAL_CTRL in SDM.
**/
#define MSR_HASWELL_UNC_PERF_GLOBAL_CTRL         0x00000391

/**
  MSR information returned for MSR index #MSR_HASWELL_UNC_PERF_GLOBAL_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Core 0 select.
    ///
    UINT32  PMI_Sel_Core0:1;
    ///
    /// [Bit 1] Core 1 select.
    ///
    UINT32  PMI_Sel_Core1:1;
    ///
    /// [Bit 2] Core 2 select.
    ///
    UINT32  PMI_Sel_Core2:1;
    ///
    /// [Bit 3] Core 3 select.
    ///
    UINT32  PMI_Sel_Core3:1;
    UINT32  Reserved1:15;
    UINT32  Reserved2:10;
    ///
    /// [Bit 29] Enable all uncore counters.
    ///
    UINT32  EN:1;
    ///
    /// [Bit 30] Enable wake on PMI.
    ///
    UINT32  WakePMI:1;
    ///
    /// [Bit 31] Enable Freezing counter when overflow.
    ///
    UINT32  FREEZE:1;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_UNC_PERF_GLOBAL_CTRL_REGISTER;


/**
  Package. Uncore PMU main status.

  @param  ECX  MSR_HASWELL_UNC_PERF_GLOBAL_STATUS (0x00000392)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_PERF_GLOBAL_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_PERF_GLOBAL_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_UNC_PERF_GLOBAL_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_UNC_PERF_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_HASWELL_UNC_PERF_GLOBAL_STATUS, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_UNC_PERF_GLOBAL_STATUS is defined as MSR_UNC_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_HASWELL_UNC_PERF_GLOBAL_STATUS       0x00000392

/**
  MSR information returned for MSR index #MSR_HASWELL_UNC_PERF_GLOBAL_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Fixed counter overflowed.
    ///
    UINT32  Fixed:1;
    ///
    /// [Bit 1] An ARB counter overflowed.
    ///
    UINT32  ARB:1;
    UINT32  Reserved1:1;
    ///
    /// [Bit 3] A CBox counter overflowed (on any slice).
    ///
    UINT32  CBox:1;
    UINT32  Reserved2:28;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_UNC_PERF_GLOBAL_STATUS_REGISTER;


/**
  Package. Uncore fixed counter control (R/W).

  @param  ECX  MSR_HASWELL_UNC_PERF_FIXED_CTRL (0x00000394)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_PERF_FIXED_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_PERF_FIXED_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_UNC_PERF_FIXED_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_UNC_PERF_FIXED_CTRL);
  AsmWriteMsr64 (MSR_HASWELL_UNC_PERF_FIXED_CTRL, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_UNC_PERF_FIXED_CTRL is defined as MSR_UNC_PERF_FIXED_CTRL in SDM.
**/
#define MSR_HASWELL_UNC_PERF_FIXED_CTRL          0x00000394

/**
  MSR information returned for MSR index #MSR_HASWELL_UNC_PERF_FIXED_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:20;
    ///
    /// [Bit 20] Enable overflow propagation.
    ///
    UINT32  EnableOverflow:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 22] Enable counting.
    ///
    UINT32  EnableCounting:1;
    UINT32  Reserved3:9;
    UINT32  Reserved4:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_UNC_PERF_FIXED_CTRL_REGISTER;


/**
  Package. Uncore fixed counter.

  @param  ECX  MSR_HASWELL_UNC_PERF_FIXED_CTR (0x00000395)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_PERF_FIXED_CTR_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_PERF_FIXED_CTR_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_UNC_PERF_FIXED_CTR_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_UNC_PERF_FIXED_CTR);
  AsmWriteMsr64 (MSR_HASWELL_UNC_PERF_FIXED_CTR, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_UNC_PERF_FIXED_CTR is defined as MSR_UNC_PERF_FIXED_CTR in SDM.
**/
#define MSR_HASWELL_UNC_PERF_FIXED_CTR           0x00000395

/**
  MSR information returned for MSR index #MSR_HASWELL_UNC_PERF_FIXED_CTR
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Current count.
    ///
    UINT32  CurrentCount:32;
    ///
    /// [Bits 47:32] Current count.
    ///
    UINT32  CurrentCountHi:16;
    UINT32  Reserved:16;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_UNC_PERF_FIXED_CTR_REGISTER;


/**
  Package. Uncore C-Box configuration information (R/O).

  @param  ECX  MSR_HASWELL_UNC_CBO_CONFIG (0x00000396)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_CBO_CONFIG_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_UNC_CBO_CONFIG_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_UNC_CBO_CONFIG_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_CONFIG);
  @endcode
  @note MSR_HASWELL_UNC_CBO_CONFIG is defined as MSR_UNC_CBO_CONFIG in SDM.
**/
#define MSR_HASWELL_UNC_CBO_CONFIG               0x00000396

/**
  MSR information returned for MSR index #MSR_HASWELL_UNC_CBO_CONFIG
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Encoded number of C-Box, derive value by "-1".
    ///
    UINT32  CBox:4;
    UINT32  Reserved1:28;
    UINT32  Reserved2:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_UNC_CBO_CONFIG_REGISTER;


/**
  Package. Uncore Arb unit, performance counter 0.

  @param  ECX  MSR_HASWELL_UNC_ARB_PERFCTR0 (0x000003B0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_ARB_PERFCTR0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_ARB_PERFCTR0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_ARB_PERFCTR0 is defined as MSR_UNC_ARB_PERFCTR0 in SDM.
**/
#define MSR_HASWELL_UNC_ARB_PERFCTR0             0x000003B0


/**
  Package. Uncore Arb unit, performance counter 1.

  @param  ECX  MSR_HASWELL_UNC_ARB_PERFCTR1 (0x000003B1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_ARB_PERFCTR1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_ARB_PERFCTR1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_ARB_PERFCTR1 is defined as MSR_UNC_ARB_PERFCTR1 in SDM.
**/
#define MSR_HASWELL_UNC_ARB_PERFCTR1             0x000003B1


/**
  Package. Uncore Arb unit, counter 0 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_ARB_PERFEVTSEL0 (0x000003B2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_ARB_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_ARB_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_ARB_PERFEVTSEL0 is defined as MSR_UNC_ARB_PERFEVTSEL0 in SDM.
**/
#define MSR_HASWELL_UNC_ARB_PERFEVTSEL0          0x000003B2


/**
  Package. Uncore Arb unit, counter 1 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_ARB_PERFEVTSEL1 (0x000003B3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_ARB_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_ARB_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_ARB_PERFEVTSEL1 is defined as MSR_UNC_ARB_PERFEVTSEL1 in SDM.
**/
#define MSR_HASWELL_UNC_ARB_PERFEVTSEL1          0x000003B3


/**
  Package. Enhanced SMM Feature Control (SMM-RW) Reports SMM capability
  Enhancement. Accessible only while in SMM.

  @param  ECX  MSR_HASWELL_SMM_FEATURE_CONTROL (0x000004E0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_SMM_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_SMM_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_SMM_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_SMM_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_HASWELL_SMM_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_SMM_FEATURE_CONTROL is defined as MSR_SMM_FEATURE_CONTROL in SDM.
**/
#define MSR_HASWELL_SMM_FEATURE_CONTROL          0x000004E0

/**
  MSR information returned for MSR index #MSR_HASWELL_SMM_FEATURE_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Lock (SMM-RWO) When set to '1' locks this register from
    /// further changes.
    ///
    UINT32  Lock:1;
    UINT32  Reserved1:1;
    ///
    /// [Bit 2] SMM_Code_Chk_En (SMM-RW) This control bit is available only if
    /// MSR_SMM_MCA_CAP[58] == 1. When set to '0' (default) none of the
    /// logical processors are prevented from executing SMM code outside the
    /// ranges defined by the SMRR. When set to '1' any logical processor in
    /// the package that attempts to execute SMM code not within the ranges
    /// defined by the SMRR will assert an unrecoverable MCE.
    ///
    UINT32  SMM_Code_Chk_En:1;
    UINT32  Reserved2:29;
    UINT32  Reserved3:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_SMM_FEATURE_CONTROL_REGISTER;


/**
  Package. SMM Delayed (SMM-RO) Reports the interruptible state of all logical
  processors in the package. Available only while in SMM and
  MSR_SMM_MCA_CAP[LONG_FLOW_INDICATION] == 1.

  [Bits 31:0] LOG_PROC_STATE (SMM-RO) Each bit represents a logical
  processor of its state in a long flow of internal operation which
  delays servicing an interrupt. The corresponding bit will be set at
  the start of long events such as: Microcode Update Load, C6, WBINVD,
  Ratio Change, Throttle. The bit is automatically cleared at the end of
  each long event. The reset value of this field is 0. Only bit
  positions below N = CPUID.(EAX=0BH, ECX=PKG_LVL):EBX[15:0] can be
  updated.

  [Bits 63:32] LOG_PROC_STATE (SMM-RO) Each bit represents a logical
  processor of its state in a long flow of internal operation which
  delays servicing an interrupt. The corresponding bit will be set at
  the start of long events such as: Microcode Update Load, C6, WBINVD,
  Ratio Change, Throttle. The bit is automatically cleared at the end of
  each long event. The reset value of this field is 0. Only bit
  positions below N = CPUID.(EAX=0BH, ECX=PKG_LVL):EBX[15:0] can be
  updated.

  @param  ECX  MSR_HASWELL_SMM_DELAYED (0x000004E2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_SMM_DELAYED);
  @endcode
  @note MSR_HASWELL_SMM_DELAYED is defined as MSR_SMM_DELAYED in SDM.
**/
#define MSR_HASWELL_SMM_DELAYED                  0x000004E2


/**
  Package. SMM Blocked (SMM-RO) Reports the blocked state of all logical
  processors in the package. Available only while in SMM.

  [Bits 31:0] LOG_PROC_STATE (SMM-RO) Each bit represents a logical
  processor of its blocked state to service an SMI. The corresponding
  bit will be set if the logical processor is in one of the following
  states: Wait For SIPI or SENTER Sleep. The reset value of this field
  is 0FFFH. Only bit positions below N = CPUID.(EAX=0BH,
  ECX=PKG_LVL):EBX[15:0] can be updated.


  [Bits 63:32] LOG_PROC_STATE (SMM-RO) Each bit represents a logical
  processor of its blocked state to service an SMI. The corresponding
  bit will be set if the logical processor is in one of the following
  states: Wait For SIPI or SENTER Sleep. The reset value of this field
  is 0FFFH. Only bit positions below N = CPUID.(EAX=0BH,
  ECX=PKG_LVL):EBX[15:0] can be updated.

  @param  ECX  MSR_HASWELL_SMM_BLOCKED (0x000004E3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_SMM_BLOCKED);
  @endcode
  @note MSR_HASWELL_SMM_BLOCKED is defined as MSR_SMM_BLOCKED in SDM.
**/
#define MSR_HASWELL_SMM_BLOCKED                  0x000004E3


/**
  Package. Unit Multipliers used in RAPL Interfaces (R/O).

  @param  ECX  MSR_HASWELL_RAPL_POWER_UNIT (0x00000606)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_RAPL_POWER_UNIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_RAPL_POWER_UNIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_RAPL_POWER_UNIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_RAPL_POWER_UNIT);
  @endcode
  @note MSR_HASWELL_RAPL_POWER_UNIT is defined as MSR_RAPL_POWER_UNIT in SDM.
**/
#define MSR_HASWELL_RAPL_POWER_UNIT              0x00000606

/**
  MSR information returned for MSR index #MSR_HASWELL_RAPL_POWER_UNIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Package. Power Units See Section 14.9.1, "RAPL Interfaces.".
    ///
    UINT32  PowerUnits:4;
    UINT32  Reserved1:4;
    ///
    /// [Bits 12:8] Package. Energy Status Units Energy related information
    /// (in Joules) is based on the multiplier, 1/2^ESU; where ESU is an
    /// unsigned integer represented by bits 12:8. Default value is 0EH (or 61
    /// micro-joules).
    ///
    UINT32  EnergyStatusUnits:5;
    UINT32  Reserved2:3;
    ///
    /// [Bits 19:16] Package. Time Units See Section 14.9.1, "RAPL
    /// Interfaces.".
    ///
    UINT32  TimeUnits:4;
    UINT32  Reserved3:12;
    UINT32  Reserved4:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_RAPL_POWER_UNIT_REGISTER;


/**
  Package. PP0 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_HASWELL_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_HASWELL_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_HASWELL_PP0_ENERGY_STATUS            0x00000639


/**
  Package. PP1 RAPL Power Limit Control (R/W) See Section 14.9.4, "PP0/PP1
  RAPL Domains.".

  @param  ECX  MSR_HASWELL_PP1_POWER_LIMIT (0x00000640)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_PP1_POWER_LIMIT);
  AsmWriteMsr64 (MSR_HASWELL_PP1_POWER_LIMIT, Msr);
  @endcode
  @note MSR_HASWELL_PP1_POWER_LIMIT is defined as MSR_PP1_POWER_LIMIT in SDM.
**/
#define MSR_HASWELL_PP1_POWER_LIMIT              0x00000640


/**
  Package. PP1 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_HASWELL_PP1_ENERGY_STATUS (0x00000641)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_PP1_ENERGY_STATUS);
  @endcode
  @note MSR_HASWELL_PP1_ENERGY_STATUS is defined as MSR_PP1_ENERGY_STATUS in SDM.
**/
#define MSR_HASWELL_PP1_ENERGY_STATUS            0x00000641


/**
  Package. PP1 Balance Policy (R/W) See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_HASWELL_PP1_POLICY (0x00000642)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_PP1_POLICY);
  AsmWriteMsr64 (MSR_HASWELL_PP1_POLICY, Msr);
  @endcode
  @note MSR_HASWELL_PP1_POLICY is defined as MSR_PP1_POLICY in SDM.
**/
#define MSR_HASWELL_PP1_POLICY                   0x00000642


/**
  Package. Indicator of Frequency Clipping in Processor Cores (R/W) (frequency
  refers to processor core frequency).

  @param  ECX  MSR_HASWELL_CORE_PERF_LIMIT_REASONS (0x00000690)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_CORE_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_CORE_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_CORE_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_CORE_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_HASWELL_CORE_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_CORE_PERF_LIMIT_REASONS is defined as MSR_CORE_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_HASWELL_CORE_PERF_LIMIT_REASONS      0x00000690

/**
  MSR information returned for MSR index #MSR_HASWELL_CORE_PERF_LIMIT_REASONS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] PROCHOT Status (R0) When set, processor core frequency is
    /// reduced below the operating system request due to assertion of
    /// external PROCHOT.
    ///
    UINT32  PROCHOT_Status:1;
    ///
    /// [Bit 1] Thermal Status (R0) When set, frequency is reduced below the
    /// operating system request due to a thermal event.
    ///
    UINT32  ThermalStatus:1;
    UINT32  Reserved1:2;
    ///
    /// [Bit 4] Graphics Driver Status (R0) When set, frequency is reduced
    /// below the operating system request due to Processor Graphics driver
    /// override.
    ///
    UINT32  GraphicsDriverStatus:1;
    ///
    /// [Bit 5] Autonomous Utilization-Based Frequency Control Status (R0)
    /// When set, frequency is reduced below the operating system request
    /// because the processor has detected that utilization is low.
    ///
    UINT32  AutonomousUtilizationBasedFrequencyControlStatus:1;
    ///
    /// [Bit 6] VR Therm Alert Status (R0) When set, frequency is reduced
    /// below the operating system request due to a thermal alert from the
    /// Voltage Regulator.
    ///
    UINT32  VRThermAlertStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 8] Electrical Design Point Status (R0) When set, frequency is
    /// reduced below the operating system request due to electrical design
    /// point constraints (e.g. maximum electrical current consumption).
    ///
    UINT32  ElectricalDesignPointStatus:1;
    ///
    /// [Bit 9] Core Power Limiting Status (R0) When set, frequency is reduced
    /// below the operating system request due to domain-level power limiting.
    ///
    UINT32  PLStatus:1;
    ///
    /// [Bit 10] Package-Level Power Limiting PL1 Status (R0) When set,
    /// frequency is reduced below the operating system request due to
    /// package-level power limiting PL1.
    ///
    UINT32  PL1Status:1;
    ///
    /// [Bit 11] Package-Level PL2 Power Limiting Status (R0) When set,
    /// frequency is reduced below the operating system request due to
    /// package-level power limiting PL2.
    ///
    UINT32  PL2Status:1;
    ///
    /// [Bit 12] Max Turbo Limit Status (R0) When set, frequency is reduced
    /// below the operating system request due to multi-core turbo limits.
    ///
    UINT32  MaxTurboLimitStatus:1;
    ///
    /// [Bit 13] Turbo Transition Attenuation Status (R0) When set, frequency
    /// is reduced below the operating system request due to Turbo transition
    /// attenuation. This prevents performance degradation due to frequent
    /// operating ratio changes.
    ///
    UINT32  TurboTransitionAttenuationStatus:1;
    UINT32  Reserved3:2;
    ///
    /// [Bit 16] PROCHOT Log  When set, indicates that the PROCHOT Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PROCHOT_Log:1;
    ///
    /// [Bit 17] Thermal Log  When set, indicates that the Thermal Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  ThermalLog:1;
    UINT32  Reserved4:2;
    ///
    /// [Bit 20] Graphics Driver Log  When set, indicates that the Graphics
    /// Driver Status bit has asserted since the log bit was last cleared.
    /// This log bit will remain set until cleared by software writing 0.
    ///
    UINT32  GraphicsDriverLog:1;
    ///
    /// [Bit 21] Autonomous Utilization-Based Frequency Control Log  When set,
    /// indicates that the Autonomous Utilization-Based Frequency Control
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  AutonomousUtilizationBasedFrequencyControlLog:1;
    ///
    /// [Bit 22] VR Therm Alert Log  When set, indicates that the VR Therm
    /// Alert Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermAlertLog:1;
    UINT32  Reserved5:1;
    ///
    /// [Bit 24] Electrical Design Point Log  When set, indicates that the EDP
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  ElectricalDesignPointLog:1;
    ///
    /// [Bit 25] Core Power Limiting Log  When set, indicates that the Core
    /// Power Limiting Status bit has asserted since the log bit was last
    /// cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  PLLog:1;
    ///
    /// [Bit 26] Package-Level PL1 Power Limiting Log  When set, indicates
    /// that the Package Level PL1 Power Limiting Status bit has asserted
    /// since the log bit was last cleared. This log bit will remain set until
    /// cleared by software writing 0.
    ///
    UINT32  PL1Log:1;
    ///
    /// [Bit 27] Package-Level PL2 Power Limiting Log When set, indicates that
    /// the Package Level PL2 Power Limiting Status bit has asserted since the
    /// log bit was last cleared. This log bit will remain set until cleared
    /// by software writing 0.
    ///
    UINT32  PL2Log:1;
    ///
    /// [Bit 28] Max Turbo Limit Log When set, indicates that the Max Turbo
    /// Limit Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  MaxTurboLimitLog:1;
    ///
    /// [Bit 29] Turbo Transition Attenuation Log When set, indicates that the
    /// Turbo Transition Attenuation Status bit has asserted since the log bit
    /// was last cleared. This log bit will remain set until cleared by
    /// software writing 0.
    ///
    UINT32  TurboTransitionAttenuationLog:1;
    UINT32  Reserved6:2;
    UINT32  Reserved7:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_CORE_PERF_LIMIT_REASONS_REGISTER;


/**
  Package. Indicator of Frequency Clipping in the Processor Graphics (R/W)
  (frequency refers to processor graphics frequency).

  @param  ECX  MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS (0x000006B0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS is defined as MSR_GRAPHICS_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS  0x000006B0

/**
  MSR information returned for MSR index
  #MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] PROCHOT Status (R0) When set, frequency is reduced below the
    /// operating system request due to assertion of external PROCHOT.
    ///
    UINT32  PROCHOT_Status:1;
    ///
    /// [Bit 1] Thermal Status (R0) When set, frequency is reduced below the
    /// operating system request due to a thermal event.
    ///
    UINT32  ThermalStatus:1;
    UINT32  Reserved1:2;
    ///
    /// [Bit 4] Graphics Driver Status (R0) When set, frequency is reduced
    /// below the operating system request due to Processor Graphics driver
    /// override.
    ///
    UINT32  GraphicsDriverStatus:1;
    ///
    /// [Bit 5] Autonomous Utilization-Based Frequency Control Status (R0)
    /// When set, frequency is reduced below the operating system request
    /// because the processor has detected that utilization is low.
    ///
    UINT32  AutonomousUtilizationBasedFrequencyControlStatus:1;
    ///
    /// [Bit 6] VR Therm Alert Status (R0) When set, frequency is reduced
    /// below the operating system request due to a thermal alert from the
    /// Voltage Regulator.
    ///
    UINT32  VRThermAlertStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 8] Electrical Design Point Status (R0) When set, frequency is
    /// reduced below the operating system request due to electrical design
    /// point constraints (e.g. maximum electrical current consumption).
    ///
    UINT32  ElectricalDesignPointStatus:1;
    ///
    /// [Bit 9] Graphics Power Limiting Status (R0) When set, frequency is
    /// reduced below the operating system request due to domain-level power
    /// limiting.
    ///
    UINT32  GraphicsPowerLimitingStatus:1;
    ///
    /// [Bit 10] Package-Level Power Limiting PL1 Status (R0) When set,
    /// frequency is reduced below the operating system request due to
    /// package-level power limiting PL1.
    ///
    UINT32  PL1STatus:1;
    ///
    /// [Bit 11] Package-Level PL2 Power Limiting Status (R0) When set,
    /// frequency is reduced below the operating system request due to
    /// package-level power limiting PL2.
    ///
    UINT32  PL2Status:1;
    UINT32  Reserved3:4;
    ///
    /// [Bit 16] PROCHOT Log  When set, indicates that the PROCHOT Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PROCHOT_Log:1;
    ///
    /// [Bit 17] Thermal Log  When set, indicates that the Thermal Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  ThermalLog:1;
    UINT32  Reserved4:2;
    ///
    /// [Bit 20] Graphics Driver Log  When set, indicates that the Graphics
    /// Driver Status bit has asserted since the log bit was last cleared.
    /// This log bit will remain set until cleared by software writing 0.
    ///
    UINT32  GraphicsDriverLog:1;
    ///
    /// [Bit 21] Autonomous Utilization-Based Frequency Control Log  When set,
    /// indicates that the Autonomous Utilization-Based Frequency Control
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  AutonomousUtilizationBasedFrequencyControlLog:1;
    ///
    /// [Bit 22] VR Therm Alert Log  When set, indicates that the VR Therm
    /// Alert Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermAlertLog:1;
    UINT32  Reserved5:1;
    ///
    /// [Bit 24] Electrical Design Point Log  When set, indicates that the EDP
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  ElectricalDesignPointLog:1;
    ///
    /// [Bit 25] Core Power Limiting Log  When set, indicates that the Core
    /// Power Limiting Status bit has asserted since the log bit was last
    /// cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  CorePowerLimitingLog:1;
    ///
    /// [Bit 26] Package-Level PL1 Power Limiting Log  When set, indicates
    /// that the Package Level PL1 Power Limiting Status bit has asserted
    /// since the log bit was last cleared. This log bit will remain set until
    /// cleared by software writing 0.
    ///
    UINT32  PL1Log:1;
    ///
    /// [Bit 27] Package-Level PL2 Power Limiting Log When set, indicates that
    /// the Package Level PL2 Power Limiting Status bit has asserted since the
    /// log bit was last cleared. This log bit will remain set until cleared
    /// by software writing 0.
    ///
    UINT32  PL2Log:1;
    ///
    /// [Bit 28] Max Turbo Limit Log When set, indicates that the Max Turbo
    /// Limit Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  MaxTurboLimitLog:1;
    ///
    /// [Bit 29] Turbo Transition Attenuation Log When set, indicates that the
    /// Turbo Transition Attenuation Status bit has asserted since the log bit
    /// was last cleared. This log bit will remain set until cleared by
    /// software writing 0.
    ///
    UINT32  TurboTransitionAttenuationLog:1;
    UINT32  Reserved6:2;
    UINT32  Reserved7:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_GRAPHICS_PERF_LIMIT_REASONS_REGISTER;


/**
  Package. Indicator of Frequency Clipping in the Ring Interconnect (R/W)
  (frequency refers to ring interconnect in the uncore).

  @param  ECX  MSR_HASWELL_RING_PERF_LIMIT_REASONS (0x000006B1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_RING_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_RING_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_RING_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_RING_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_HASWELL_RING_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_RING_PERF_LIMIT_REASONS is defined as MSR_RING_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_HASWELL_RING_PERF_LIMIT_REASONS      0x000006B1

/**
  MSR information returned for MSR index #MSR_HASWELL_RING_PERF_LIMIT_REASONS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] PROCHOT Status (R0) When set, frequency is reduced below the
    /// operating system request due to assertion of external PROCHOT.
    ///
    UINT32  PROCHOT_Status:1;
    ///
    /// [Bit 1] Thermal Status (R0) When set, frequency is reduced below the
    /// operating system request due to a thermal event.
    ///
    UINT32  ThermalStatus:1;
    UINT32  Reserved1:4;
    ///
    /// [Bit 6] VR Therm Alert Status (R0) When set, frequency is reduced
    /// below the operating system request due to a thermal alert from the
    /// Voltage Regulator.
    ///
    UINT32  VRThermAlertStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 8] Electrical Design Point Status (R0) When set, frequency is
    /// reduced below the operating system request due to electrical design
    /// point constraints (e.g. maximum electrical current consumption).
    ///
    UINT32  ElectricalDesignPointStatus:1;
    UINT32  Reserved3:1;
    ///
    /// [Bit 10] Package-Level Power Limiting PL1 Status (R0) When set,
    /// frequency is reduced below the operating system request due to
    /// package-level power limiting PL1.
    ///
    UINT32  PL1STatus:1;
    ///
    /// [Bit 11] Package-Level PL2 Power Limiting Status (R0) When set,
    /// frequency is reduced below the operating system request due to
    /// package-level power limiting PL2.
    ///
    UINT32  PL2Status:1;
    UINT32  Reserved4:4;
    ///
    /// [Bit 16] PROCHOT Log  When set, indicates that the PROCHOT Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PROCHOT_Log:1;
    ///
    /// [Bit 17] Thermal Log  When set, indicates that the Thermal Status bit
    /// has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  ThermalLog:1;
    UINT32  Reserved5:2;
    ///
    /// [Bit 20] Graphics Driver Log  When set, indicates that the Graphics
    /// Driver Status bit has asserted since the log bit was last cleared.
    /// This log bit will remain set until cleared by software writing 0.
    ///
    UINT32  GraphicsDriverLog:1;
    ///
    /// [Bit 21] Autonomous Utilization-Based Frequency Control Log  When set,
    /// indicates that the Autonomous Utilization-Based Frequency Control
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  AutonomousUtilizationBasedFrequencyControlLog:1;
    ///
    /// [Bit 22] VR Therm Alert Log  When set, indicates that the VR Therm
    /// Alert Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermAlertLog:1;
    UINT32  Reserved6:1;
    ///
    /// [Bit 24] Electrical Design Point Log  When set, indicates that the EDP
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
    ///
    UINT32  ElectricalDesignPointLog:1;
    ///
    /// [Bit 25] Core Power Limiting Log  When set, indicates that the Core
    /// Power Limiting Status bit has asserted since the log bit was last
    /// cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  CorePowerLimitingLog:1;
    ///
    /// [Bit 26] Package-Level PL1 Power Limiting Log  When set, indicates
    /// that the Package Level PL1 Power Limiting Status bit has asserted
    /// since the log bit was last cleared. This log bit will remain set until
    /// cleared by software writing 0.
    ///
    UINT32  PL1Log:1;
    ///
    /// [Bit 27] Package-Level PL2 Power Limiting Log When set, indicates that
    /// the Package Level PL2 Power Limiting Status bit has asserted since the
    /// log bit was last cleared. This log bit will remain set until cleared
    /// by software writing 0.
    ///
    UINT32  PL2Log:1;
    ///
    /// [Bit 28] Max Turbo Limit Log When set, indicates that the Max Turbo
    /// Limit Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  MaxTurboLimitLog:1;
    ///
    /// [Bit 29] Turbo Transition Attenuation Log When set, indicates that the
    /// Turbo Transition Attenuation Status bit has asserted since the log bit
    /// was last cleared. This log bit will remain set until cleared by
    /// software writing 0.
    ///
    UINT32  TurboTransitionAttenuationLog:1;
    UINT32  Reserved7:2;
    UINT32  Reserved8:32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_RING_PERF_LIMIT_REASONS_REGISTER;


/**
  Package. Uncore C-Box 0, counter 0 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_CBO_0_PERFEVTSEL0 (0x00000700)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_0_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_0_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_0_PERFEVTSEL0 is defined as MSR_UNC_CBO_0_PERFEVTSEL0 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_0_PERFEVTSEL0        0x00000700


/**
  Package. Uncore C-Box 0, counter 1 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_CBO_0_PERFEVTSEL1 (0x00000701)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_0_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_0_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_0_PERFEVTSEL1 is defined as MSR_UNC_CBO_0_PERFEVTSEL1 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_0_PERFEVTSEL1        0x00000701


/**
  Package. Uncore C-Box 0, performance counter 0.

  @param  ECX  MSR_HASWELL_UNC_CBO_0_PERFCTR0 (0x00000706)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_0_PERFCTR0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_0_PERFCTR0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_0_PERFCTR0 is defined as MSR_UNC_CBO_0_PERFCTR0 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_0_PERFCTR0           0x00000706


/**
  Package. Uncore C-Box 0, performance counter 1.

  @param  ECX  MSR_HASWELL_UNC_CBO_0_PERFCTR1 (0x00000707)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_0_PERFCTR1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_0_PERFCTR1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_0_PERFCTR1 is defined as MSR_UNC_CBO_0_PERFCTR1 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_0_PERFCTR1           0x00000707


/**
  Package. Uncore C-Box 1, counter 0 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_CBO_1_PERFEVTSEL0 (0x00000710)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_1_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_1_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_1_PERFEVTSEL0 is defined as MSR_UNC_CBO_1_PERFEVTSEL0 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_1_PERFEVTSEL0        0x00000710


/**
  Package. Uncore C-Box 1, counter 1 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_CBO_1_PERFEVTSEL1 (0x00000711)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_1_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_1_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_1_PERFEVTSEL1 is defined as MSR_UNC_CBO_1_PERFEVTSEL1 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_1_PERFEVTSEL1        0x00000711


/**
  Package. Uncore C-Box 1, performance counter 0.

  @param  ECX  MSR_HASWELL_UNC_CBO_1_PERFCTR0 (0x00000716)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_1_PERFCTR0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_1_PERFCTR0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_1_PERFCTR0 is defined as MSR_UNC_CBO_1_PERFCTR0 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_1_PERFCTR0           0x00000716


/**
  Package. Uncore C-Box 1, performance counter 1.

  @param  ECX  MSR_HASWELL_UNC_CBO_1_PERFCTR1 (0x00000717)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_1_PERFCTR1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_1_PERFCTR1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_1_PERFCTR1 is defined as MSR_UNC_CBO_1_PERFCTR1 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_1_PERFCTR1           0x00000717


/**
  Package. Uncore C-Box 2, counter 0 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_CBO_2_PERFEVTSEL0 (0x00000720)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_2_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_2_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_2_PERFEVTSEL0 is defined as MSR_UNC_CBO_2_PERFEVTSEL0 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_2_PERFEVTSEL0        0x00000720


/**
  Package. Uncore C-Box 2, counter 1 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_CBO_2_PERFEVTSEL1 (0x00000721)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_2_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_2_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_2_PERFEVTSEL1 is defined as MSR_UNC_CBO_2_PERFEVTSEL1 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_2_PERFEVTSEL1        0x00000721


/**
  Package. Uncore C-Box 2, performance counter 0.

  @param  ECX  MSR_HASWELL_UNC_CBO_2_PERFCTR0 (0x00000726)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_2_PERFCTR0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_2_PERFCTR0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_2_PERFCTR0 is defined as MSR_UNC_CBO_2_PERFCTR0 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_2_PERFCTR0           0x00000726


/**
  Package. Uncore C-Box 2, performance counter 1.

  @param  ECX  MSR_HASWELL_UNC_CBO_2_PERFCTR1 (0x00000727)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_2_PERFCTR1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_2_PERFCTR1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_2_PERFCTR1 is defined as MSR_UNC_CBO_2_PERFCTR1 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_2_PERFCTR1           0x00000727


/**
  Package. Uncore C-Box 3, counter 0 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_CBO_3_PERFEVTSEL0 (0x00000730)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_3_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_3_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_3_PERFEVTSEL0 is defined as MSR_UNC_CBO_3_PERFEVTSEL0 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_3_PERFEVTSEL0        0x00000730


/**
  Package. Uncore C-Box 3, counter 1 event select MSR.

  @param  ECX  MSR_HASWELL_UNC_CBO_3_PERFEVTSEL1 (0x00000731)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_3_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_3_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_3_PERFEVTSEL1 is defined as MSR_UNC_CBO_3_PERFEVTSEL1 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_3_PERFEVTSEL1        0x00000731


/**
  Package. Uncore C-Box 3, performance counter 0.

  @param  ECX  MSR_HASWELL_UNC_CBO_3_PERFCTR0 (0x00000736)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_3_PERFCTR0);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_3_PERFCTR0, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_3_PERFCTR0 is defined as MSR_UNC_CBO_3_PERFCTR0 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_3_PERFCTR0           0x00000736


/**
  Package. Uncore C-Box 3, performance counter 1.

  @param  ECX  MSR_HASWELL_UNC_CBO_3_PERFCTR1 (0x00000737)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_HASWELL_UNC_CBO_3_PERFCTR1);
  AsmWriteMsr64 (MSR_HASWELL_UNC_CBO_3_PERFCTR1, Msr);
  @endcode
  @note MSR_HASWELL_UNC_CBO_3_PERFCTR1 is defined as MSR_UNC_CBO_3_PERFCTR1 in SDM.
**/
#define MSR_HASWELL_UNC_CBO_3_PERFCTR1           0x00000737


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI C-States.

  @param  ECX  MSR_HASWELL_PKG_C8_RESIDENCY (0x00000630)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKG_C8_RESIDENCY_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKG_C8_RESIDENCY_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_PKG_C8_RESIDENCY_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_PKG_C8_RESIDENCY);
  AsmWriteMsr64 (MSR_HASWELL_PKG_C8_RESIDENCY, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_PKG_C8_RESIDENCY is defined as MSR_PKG_C8_RESIDENCY in SDM.
**/
#define MSR_HASWELL_PKG_C8_RESIDENCY             0x00000630

/**
  MSR information returned for MSR index #MSR_HASWELL_PKG_C8_RESIDENCY
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Package C8 Residency Counter. (R/O) Value since last reset
    /// that this package is in processor-specific C8 states. Count at the
    /// same frequency as the TSC.
    ///
    UINT32  C8ResidencyCounter:32;
    ///
    /// [Bits 59:32] Package C8 Residency Counter. (R/O) Value since last
    /// reset that this package is in processor-specific C8 states. Count at
    /// the same frequency as the TSC.
    ///
    UINT32  C8ResidencyCounterHi:28;
    UINT32  Reserved:4;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_PKG_C8_RESIDENCY_REGISTER;


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI C-States.

  @param  ECX  MSR_HASWELL_PKG_C9_RESIDENCY (0x00000631)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKG_C9_RESIDENCY_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKG_C9_RESIDENCY_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_PKG_C9_RESIDENCY_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_PKG_C9_RESIDENCY);
  AsmWriteMsr64 (MSR_HASWELL_PKG_C9_RESIDENCY, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_PKG_C9_RESIDENCY is defined as MSR_PKG_C9_RESIDENCY in SDM.
**/
#define MSR_HASWELL_PKG_C9_RESIDENCY             0x00000631

/**
  MSR information returned for MSR index #MSR_HASWELL_PKG_C9_RESIDENCY
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Package C9 Residency Counter. (R/O) Value since last reset
    /// that this package is in processor-specific C9 states. Count at the
    /// same frequency as the TSC.
    ///
    UINT32  C9ResidencyCounter:32;
    ///
    /// [Bits 59:32] Package C9 Residency Counter. (R/O) Value since last
    /// reset that this package is in processor-specific C9 states. Count at
    /// the same frequency as the TSC.
    ///
    UINT32  C9ResidencyCounterHi:28;
    UINT32  Reserved:4;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_PKG_C9_RESIDENCY_REGISTER;


/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI C-States.

  @param  ECX  MSR_HASWELL_PKG_C10_RESIDENCY (0x00000632)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKG_C10_RESIDENCY_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_HASWELL_PKG_C10_RESIDENCY_REGISTER.

  <b>Example usage</b>
  @code
  MSR_HASWELL_PKG_C10_RESIDENCY_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_HASWELL_PKG_C10_RESIDENCY);
  AsmWriteMsr64 (MSR_HASWELL_PKG_C10_RESIDENCY, Msr.Uint64);
  @endcode
  @note MSR_HASWELL_PKG_C10_RESIDENCY is defined as MSR_PKG_C10_RESIDENCY in SDM.
**/
#define MSR_HASWELL_PKG_C10_RESIDENCY            0x00000632

/**
  MSR information returned for MSR index #MSR_HASWELL_PKG_C10_RESIDENCY
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Package C10 Residency Counter. (R/O) Value since last
    /// reset that this package is in processor-specific C10 states. Count at
    /// the same frequency as the TSC.
    ///
    UINT32  C10ResidencyCounter:32;
    ///
    /// [Bits 59:32] Package C10 Residency Counter. (R/O) Value since last
    /// reset that this package is in processor-specific C10 states. Count at
    /// the same frequency as the TSC.
    ///
    UINT32  C10ResidencyCounterHi:28;
    UINT32  Reserved:4;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_HASWELL_PKG_C10_RESIDENCY_REGISTER;

#endif
