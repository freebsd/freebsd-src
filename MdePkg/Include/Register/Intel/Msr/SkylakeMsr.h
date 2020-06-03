/** @file
  MSR Definitions for Intel processors based on the Skylake/Kabylake/Coffeelake/Cannonlake microarchitecture.

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

#ifndef __SKYLAKE_MSR_H__
#define __SKYLAKE_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel processors based on the Skylake microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_SKYLAKE_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x4E || \
    DisplayModel == 0x5E || \
    DisplayModel == 0x55 || \
    DisplayModel == 0x8E || \
    DisplayModel == 0x9E || \
    DisplayModel == 0x66    \
    )                       \
   )

/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_SKYLAKE_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_TURBO_RATIO_LIMIT);
  @endcode
  @note MSR_SKYLAKE_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_SKYLAKE_TURBO_RATIO_LIMIT            0x000001AD

/**
  MSR information returned for MSR index #MSR_SKYLAKE_TURBO_RATIO_LIMIT
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
} MSR_SKYLAKE_TURBO_RATIO_LIMIT_REGISTER;


/**
  Thread. Last Branch Record Stack TOS (R/W)  Contains an index (bits 0-4)
  that points to the MSR containing the most recent branch record.

  @param  ECX  MSR_SKYLAKE_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_SKYLAKE_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_SKYLAKE_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_SKYLAKE_LASTBRANCH_TOS               0x000001C9


/**
  Core. Power Control Register See http://biosbits.org.

  @param  ECX  MSR_SKYLAKE_POWER_CTL (0x000001FC)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_POWER_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_POWER_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_POWER_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_POWER_CTL);
  AsmWriteMsr64 (MSR_SKYLAKE_POWER_CTL, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_POWER_CTL                     0x000001FC

/**
  MSR information returned for MSR index #MSR_SKYLAKE_POWER_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:1;
    ///
    /// [Bit 1] Package. C1E Enable (R/W) When set to '1', will enable the CPU
    /// to switch to the Minimum Enhanced Intel SpeedStep Technology operating
    /// point when all execution cores enter MWAIT (C1).
    ///
    UINT32  C1EEnable:1;
    UINT32  Reserved2:17;
    ///
    /// [Bit 19] Disable Race to Halt Optimization (R/W) Setting this bit
    /// disables the Race to Halt optimization and avoids this optimization
    /// limitation to execute below the most efficient frequency ratio.
    /// Default value is 0 for processors that support Race to Halt
    /// optimization. Default value is 1 for processors that do not support
    /// Race to Halt optimization.
    ///
    UINT32  Fix_Me_1:1;
    ///
    /// [Bit 20] Disable Energy Efficiency Optimization (R/W) Setting this bit
    /// disables the P-States energy efficiency optimization. Default value is
    /// 0. Disable/enable the energy efficiency optimization in P-State legacy
    /// mode (when IA32_PM_ENABLE[HWP_ENABLE] = 0), has an effect only in the
    /// turbo range or into PERF_MIN_CTL value if it is not zero set. In HWP
    /// mode (IA32_PM_ENABLE[HWP_ENABLE] == 1), has an effect between the OS
    /// desired or OS maximize to the OS minimize performance setting.
    ///
    UINT32  DisableEnergyEfficiencyOptimization:1;
    UINT32  Reserved3:11;
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
} MSR_SKYLAKE_POWER_CTL_REGISTER;


/**
  Package. Lower 64 Bit CR_SGXOWNEREPOCH (W) Writes do not update
  CR_SGXOWNEREPOCH if CPUID.(EAX=12H, ECX=0):EAX.SGX1 is 1 on any thread in
  the package. Lower 64 bits of an 128-bit external entropy value for key
  derivation of an enclave.

  @param  ECX  MSR_SKYLAKE_SGXOWNEREPOCH0 (0x00000300)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = 0;
  AsmWriteMsr64 (MSR_SKYLAKE_SGXOWNEREPOCH0, Msr);
  @endcode
  @note MSR_SKYLAKE_SGXOWNEREPOCH0 is defined as MSR_SGXOWNER0 in SDM.
**/
#define MSR_SKYLAKE_SGXOWNEREPOCH0                    0x00000300

//
// Define MSR_SKYLAKE_SGXOWNER0 for compatibility due to name change in the SDM.
//
#define MSR_SKYLAKE_SGXOWNER0                         MSR_SKYLAKE_SGXOWNEREPOCH0
/**
  Package. Upper 64 Bit CR_SGXOWNEREPOCH (W) Writes do not update
  CR_SGXOWNEREPOCH if CPUID.(EAX=12H, ECX=0):EAX.SGX1 is 1 on any thread in
  the package. Upper 64 bits of an 128-bit external entropy value for key
  derivation of an enclave.

  @param  ECX  MSR_SKYLAKE_SGXOWNEREPOCH1 (0x00000301)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = 0;
  AsmWriteMsr64 (MSR_SKYLAKE_SGXOWNEREPOCH1, Msr);
  @endcode
  @note MSR_SKYLAKE_SGXOWNEREPOCH1 is defined as MSR_SGXOWNER1 in SDM.
**/
#define MSR_SKYLAKE_SGXOWNEREPOCH1                0x00000301

//
// Define MSR_SKYLAKE_SGXOWNER1 for compatibility due to name change in the SDM.
//
#define MSR_SKYLAKE_SGXOWNER1                     MSR_SKYLAKE_SGXOWNEREPOCH1


/**
  See Table 2-2. See Section 18.2.4, "Architectural Performance Monitoring
  Version 4.".

  @param  ECX  MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS (0x0000038E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS is defined as IA32_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS      0x0000038E

/**
  MSR information returned for MSR index #MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Thread. Ovf_PMC0.
    ///
    UINT32  Ovf_PMC0:1;
    ///
    /// [Bit 1] Thread. Ovf_PMC1.
    ///
    UINT32  Ovf_PMC1:1;
    ///
    /// [Bit 2] Thread. Ovf_PMC2.
    ///
    UINT32  Ovf_PMC2:1;
    ///
    /// [Bit 3] Thread. Ovf_PMC3.
    ///
    UINT32  Ovf_PMC3:1;
    ///
    /// [Bit 4] Thread. Ovf_PMC4 (if CPUID.0AH:EAX[15:8] > 4).
    ///
    UINT32  Ovf_PMC4:1;
    ///
    /// [Bit 5] Thread. Ovf_PMC5 (if CPUID.0AH:EAX[15:8] > 5).
    ///
    UINT32  Ovf_PMC5:1;
    ///
    /// [Bit 6] Thread. Ovf_PMC6 (if CPUID.0AH:EAX[15:8] > 6).
    ///
    UINT32  Ovf_PMC6:1;
    ///
    /// [Bit 7] Thread. Ovf_PMC7 (if CPUID.0AH:EAX[15:8] > 7).
    ///
    UINT32  Ovf_PMC7:1;
    UINT32  Reserved1:24;
    ///
    /// [Bit 32] Thread. Ovf_FixedCtr0.
    ///
    UINT32  Ovf_FixedCtr0:1;
    ///
    /// [Bit 33] Thread. Ovf_FixedCtr1.
    ///
    UINT32  Ovf_FixedCtr1:1;
    ///
    /// [Bit 34] Thread. Ovf_FixedCtr2.
    ///
    UINT32  Ovf_FixedCtr2:1;
    UINT32  Reserved2:20;
    ///
    /// [Bit 55] Thread. Trace_ToPA_PMI.
    ///
    UINT32  Trace_ToPA_PMI:1;
    UINT32  Reserved3:2;
    ///
    /// [Bit 58] Thread. LBR_Frz.
    ///
    UINT32  LBR_Frz:1;
    ///
    /// [Bit 59] Thread. CTR_Frz.
    ///
    UINT32  CTR_Frz:1;
    ///
    /// [Bit 60] Thread. ASCI.
    ///
    UINT32  ASCI:1;
    ///
    /// [Bit 61] Thread. Ovf_Uncore.
    ///
    UINT32  Ovf_Uncore:1;
    ///
    /// [Bit 62] Thread. Ovf_BufDSSAVE.
    ///
    UINT32  Ovf_BufDSSAVE:1;
    ///
    /// [Bit 63] Thread. CondChgd.
    ///
    UINT32  CondChgd:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_REGISTER;


/**
  See Table 2-2. See Section 18.2.4, "Architectural Performance Monitoring
  Version 4.".

  @param  ECX  MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET (0x00000390)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET);
  AsmWriteMsr64 (MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET is defined as IA32_PERF_GLOBAL_STATUS_RESET in SDM.
**/
#define MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET 0x00000390

/**
  MSR information returned for MSR index
  #MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Thread. Set 1 to clear Ovf_PMC0.
    ///
    UINT32  Ovf_PMC0:1;
    ///
    /// [Bit 1] Thread. Set 1 to clear Ovf_PMC1.
    ///
    UINT32  Ovf_PMC1:1;
    ///
    /// [Bit 2] Thread. Set 1 to clear Ovf_PMC2.
    ///
    UINT32  Ovf_PMC2:1;
    ///
    /// [Bit 3] Thread. Set 1 to clear Ovf_PMC3.
    ///
    UINT32  Ovf_PMC3:1;
    ///
    /// [Bit 4] Thread. Set 1 to clear Ovf_PMC4 (if CPUID.0AH:EAX[15:8] > 4).
    ///
    UINT32  Ovf_PMC4:1;
    ///
    /// [Bit 5] Thread. Set 1 to clear Ovf_PMC5 (if CPUID.0AH:EAX[15:8] > 5).
    ///
    UINT32  Ovf_PMC5:1;
    ///
    /// [Bit 6] Thread. Set 1 to clear Ovf_PMC6 (if CPUID.0AH:EAX[15:8] > 6).
    ///
    UINT32  Ovf_PMC6:1;
    ///
    /// [Bit 7] Thread. Set 1 to clear Ovf_PMC7 (if CPUID.0AH:EAX[15:8] > 7).
    ///
    UINT32  Ovf_PMC7:1;
    UINT32  Reserved1:24;
    ///
    /// [Bit 32] Thread. Set 1 to clear Ovf_FixedCtr0.
    ///
    UINT32  Ovf_FixedCtr0:1;
    ///
    /// [Bit 33] Thread. Set 1 to clear Ovf_FixedCtr1.
    ///
    UINT32  Ovf_FixedCtr1:1;
    ///
    /// [Bit 34] Thread. Set 1 to clear Ovf_FixedCtr2.
    ///
    UINT32  Ovf_FixedCtr2:1;
    UINT32  Reserved2:20;
    ///
    /// [Bit 55] Thread. Set 1 to clear Trace_ToPA_PMI.
    ///
    UINT32  Trace_ToPA_PMI:1;
    UINT32  Reserved3:2;
    ///
    /// [Bit 58] Thread. Set 1 to clear LBR_Frz.
    ///
    UINT32  LBR_Frz:1;
    ///
    /// [Bit 59] Thread. Set 1 to clear CTR_Frz.
    ///
    UINT32  CTR_Frz:1;
    ///
    /// [Bit 60] Thread. Set 1 to clear ASCI.
    ///
    UINT32  ASCI:1;
    ///
    /// [Bit 61] Thread. Set 1 to clear Ovf_Uncore.
    ///
    UINT32  Ovf_Uncore:1;
    ///
    /// [Bit 62] Thread. Set 1 to clear Ovf_BufDSSAVE.
    ///
    UINT32  Ovf_BufDSSAVE:1;
    ///
    /// [Bit 63] Thread. Set 1 to clear CondChgd.
    ///
    UINT32  CondChgd:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_RESET_REGISTER;


/**
  See Table 2-2. See Section 18.2.4, "Architectural Performance Monitoring
  Version 4.".

  @param  ECX  MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET (0x00000391)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET);
  AsmWriteMsr64 (MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET is defined as IA32_PERF_GLOBAL_STATUS_SET in SDM.
**/
#define MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET  0x00000391

/**
  MSR information returned for MSR index
  #MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Thread. Set 1 to cause Ovf_PMC0 = 1.
    ///
    UINT32  Ovf_PMC0:1;
    ///
    /// [Bit 1] Thread. Set 1 to cause Ovf_PMC1 = 1.
    ///
    UINT32  Ovf_PMC1:1;
    ///
    /// [Bit 2] Thread. Set 1 to cause Ovf_PMC2 = 1.
    ///
    UINT32  Ovf_PMC2:1;
    ///
    /// [Bit 3] Thread. Set 1 to cause Ovf_PMC3 = 1.
    ///
    UINT32  Ovf_PMC3:1;
    ///
    /// [Bit 4] Thread. Set 1 to cause Ovf_PMC4=1 (if CPUID.0AH:EAX[15:8] > 4).
    ///
    UINT32  Ovf_PMC4:1;
    ///
    /// [Bit 5] Thread. Set 1 to cause Ovf_PMC5=1 (if CPUID.0AH:EAX[15:8] > 5).
    ///
    UINT32  Ovf_PMC5:1;
    ///
    /// [Bit 6] Thread. Set 1 to cause Ovf_PMC6=1 (if CPUID.0AH:EAX[15:8] > 6).
    ///
    UINT32  Ovf_PMC6:1;
    ///
    /// [Bit 7] Thread. Set 1 to cause Ovf_PMC7=1 (if CPUID.0AH:EAX[15:8] > 7).
    ///
    UINT32  Ovf_PMC7:1;
    UINT32  Reserved1:24;
    ///
    /// [Bit 32] Thread. Set 1 to cause Ovf_FixedCtr0 = 1.
    ///
    UINT32  Ovf_FixedCtr0:1;
    ///
    /// [Bit 33] Thread. Set 1 to cause Ovf_FixedCtr1 = 1.
    ///
    UINT32  Ovf_FixedCtr1:1;
    ///
    /// [Bit 34] Thread. Set 1 to cause Ovf_FixedCtr2 = 1.
    ///
    UINT32  Ovf_FixedCtr2:1;
    UINT32  Reserved2:20;
    ///
    /// [Bit 55] Thread. Set 1 to cause Trace_ToPA_PMI = 1.
    ///
    UINT32  Trace_ToPA_PMI:1;
    UINT32  Reserved3:2;
    ///
    /// [Bit 58] Thread. Set 1 to cause LBR_Frz = 1.
    ///
    UINT32  LBR_Frz:1;
    ///
    /// [Bit 59] Thread. Set 1 to cause CTR_Frz = 1.
    ///
    UINT32  CTR_Frz:1;
    ///
    /// [Bit 60] Thread. Set 1 to cause ASCI = 1.
    ///
    UINT32  ASCI:1;
    ///
    /// [Bit 61] Thread. Set 1 to cause Ovf_Uncore.
    ///
    UINT32  Ovf_Uncore:1;
    ///
    /// [Bit 62] Thread. Set 1 to cause Ovf_BufDSSAVE.
    ///
    UINT32  Ovf_BufDSSAVE:1;
    UINT32  Reserved4:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_IA32_PERF_GLOBAL_STATUS_SET_REGISTER;


/**
  Thread. FrontEnd Precise Event Condition Select (R/W).

  @param  ECX  MSR_SKYLAKE_PEBS_FRONTEND (0x000003F7)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PEBS_FRONTEND_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PEBS_FRONTEND_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PEBS_FRONTEND_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PEBS_FRONTEND);
  AsmWriteMsr64 (MSR_SKYLAKE_PEBS_FRONTEND, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_PEBS_FRONTEND is defined as MSR_PEBS_FRONTEND in SDM.
**/
#define MSR_SKYLAKE_PEBS_FRONTEND                0x000003F7

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PEBS_FRONTEND
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] Event Code Select.
    ///
    UINT32  EventCodeSelect:3;
    UINT32  Reserved1:1;
    ///
    /// [Bit 4] Event Code Select High.
    ///
    UINT32  EventCodeSelectHigh:1;
    UINT32  Reserved2:3;
    ///
    /// [Bits 19:8] IDQ_Bubble_Length Specifier.
    ///
    UINT32  IDQ_Bubble_Length:12;
    ///
    /// [Bits 22:20] IDQ_Bubble_Width Specifier.
    ///
    UINT32  IDQ_Bubble_Width:3;
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
} MSR_SKYLAKE_PEBS_FRONTEND_REGISTER;


/**
  Package. PP0 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_SKYLAKE_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_SKYLAKE_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_SKYLAKE_PP0_ENERGY_STATUS            0x00000639


/**
  Platform*. Platform Energy Counter. (R/O). This MSR is valid only if both
  platform vendor hardware implementation and BIOS enablement support it. This
  MSR will read 0 if not valid.

  @param  ECX  MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER (0x0000064D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER);
  @endcode
  @note MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER is defined as MSR_PLATFORM_ENERGY_COUNTER in SDM.
**/
#define MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER      0x0000064D

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Total energy consumed by all devices in the platform that
    /// receive power from integrated power delivery mechanism, Included
    /// platform devices are processor cores, SOC, memory, add-on or
    /// peripheral devices that get powered directly from the platform power
    /// delivery means. The energy units are specified in the
    /// MSR_RAPL_POWER_UNIT.Enery_Status_Unit.
    ///
    UINT32  TotalEnergy:32;
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
} MSR_SKYLAKE_PLATFORM_ENERGY_COUNTER_REGISTER;


/**
  Thread. Productive Performance Count. (R/O). Hardware's view of workload
  scalability. See Section 14.4.5.1.

  @param  ECX  MSR_SKYLAKE_PPERF (0x0000064E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_PPERF);
  @endcode
  @note MSR_SKYLAKE_PPERF is defined as MSR_PPERF in SDM.
**/
#define MSR_SKYLAKE_PPERF                        0x0000064E


/**
  Package. Indicator of Frequency Clipping in Processor Cores (R/W) (frequency
  refers to processor core frequency).

  @param  ECX  MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS (0x0000064F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS is defined as MSR_CORE_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS      0x0000064F

/**
  MSR information returned for MSR index #MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS
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
    /// [Bit 4] Residency State Regulation Status (R0) When set, frequency is
    /// reduced below the operating system request due to residency state
    /// regulation limit.
    ///
    UINT32  ResidencyStateRegulationStatus:1;
    ///
    /// [Bit 5] Running Average Thermal Limit Status (R0) When set, frequency
    /// is reduced below the operating system request due to Running Average
    /// Thermal Limit (RATL).
    ///
    UINT32  RunningAverageThermalLimitStatus:1;
    ///
    /// [Bit 6] VR Therm Alert Status (R0) When set, frequency is reduced
    /// below the operating system request due to a thermal alert from a
    /// processor Voltage Regulator (VR).
    ///
    UINT32  VRThermAlertStatus:1;
    ///
    /// [Bit 7] VR Therm Design Current Status (R0) When set, frequency is
    /// reduced below the operating system request due to VR thermal design
    /// current limit.
    ///
    UINT32  VRThermDesignCurrentStatus:1;
    ///
    /// [Bit 8] Other Status (R0) When set, frequency is reduced below the
    /// operating system request due to electrical or other constraints.
    ///
    UINT32  OtherStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 10] Package/Platform-Level Power Limiting PL1 Status (R0) When
    /// set, frequency is reduced below the operating system request due to
    /// package/platform-level power limiting PL1.
    ///
    UINT32  PL1Status:1;
    ///
    /// [Bit 11] Package/Platform-Level PL2 Power Limiting Status (R0) When
    /// set, frequency is reduced below the operating system request due to
    /// package/platform-level power limiting PL2/PL3.
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
    /// [Bit 20] Residency State Regulation Log  When set, indicates that the
    /// Residency State Regulation Status bit has asserted since the log bit
    /// was last cleared. This log bit will remain set until cleared by
    /// software writing 0.
    ///
    UINT32  ResidencyStateRegulationLog:1;
    ///
    /// [Bit 21] Running Average Thermal Limit Log  When set, indicates that
    /// the RATL Status bit has asserted since the log bit was last cleared.
    /// This log bit will remain set until cleared by software writing 0.
    ///
    UINT32  RunningAverageThermalLimitLog:1;
    ///
    /// [Bit 22] VR Therm Alert Log  When set, indicates that the VR Therm
    /// Alert Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermAlertLog:1;
    ///
    /// [Bit 23] VR Thermal Design Current Log  When set, indicates that the
    /// VR TDC Status bit has asserted since the log bit was last cleared.
    /// This log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermalDesignCurrentLog:1;
    ///
    /// [Bit 24] Other Log  When set, indicates that the Other Status bit has
    /// asserted since the log bit was last cleared. This log bit will remain
    /// set until cleared by software writing 0.
    ///
    UINT32  OtherLog:1;
    UINT32  Reserved5:1;
    ///
    /// [Bit 26] Package/Platform-Level PL1 Power Limiting Log  When set,
    /// indicates that the Package or Platform Level PL1 Power Limiting Status
    /// bit has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PL1Log:1;
    ///
    /// [Bit 27] Package/Platform-Level PL2 Power Limiting Log When set,
    /// indicates that the Package or Platform Level PL2/PL3 Power Limiting
    /// Status bit has asserted since the log bit was last cleared. This log
    /// bit will remain set until cleared by software writing 0.
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
} MSR_SKYLAKE_CORE_PERF_LIMIT_REASONS_REGISTER;


/**
  Package. HDC Configuration (R/W)..

  @param  ECX  MSR_SKYLAKE_PKG_HDC_CONFIG (0x00000652)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PKG_HDC_CONFIG_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PKG_HDC_CONFIG_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PKG_HDC_CONFIG_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PKG_HDC_CONFIG);
  AsmWriteMsr64 (MSR_SKYLAKE_PKG_HDC_CONFIG, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_PKG_HDC_CONFIG is defined as MSR_PKG_HDC_CONFIG in SDM.
**/
#define MSR_SKYLAKE_PKG_HDC_CONFIG               0x00000652

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PKG_HDC_CONFIG
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] PKG_Cx_Monitor.  Configures Package Cx state threshold for
    /// MSR_PKG_HDC_DEEP_RESIDENCY.
    ///
    UINT32  PKG_Cx_Monitor:3;
    UINT32  Reserved1:29;
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
} MSR_SKYLAKE_PKG_HDC_CONFIG_REGISTER;


/**
  Core. Core HDC Idle Residency. (R/O). Core_Cx_Duty_Cycle_Cnt.

  @param  ECX  MSR_SKYLAKE_CORE_HDC_RESIDENCY (0x00000653)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_CORE_HDC_RESIDENCY);
  @endcode
  @note MSR_SKYLAKE_CORE_HDC_RESIDENCY is defined as MSR_CORE_HDC_RESIDENCY in SDM.
**/
#define MSR_SKYLAKE_CORE_HDC_RESIDENCY           0x00000653


/**
  Package. Accumulate the cycles the package was in C2 state and at least one
  logical processor was in forced idle. (R/O). Pkg_C2_Duty_Cycle_Cnt.

  @param  ECX  MSR_SKYLAKE_PKG_HDC_SHALLOW_RESIDENCY (0x00000655)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_PKG_HDC_SHALLOW_RESIDENCY);
  @endcode
  @note MSR_SKYLAKE_PKG_HDC_SHALLOW_RESIDENCY is defined as MSR_PKG_HDC_SHALLOW_RESIDENCY in SDM.
**/
#define MSR_SKYLAKE_PKG_HDC_SHALLOW_RESIDENCY    0x00000655


/**
  Package. Package Cx HDC Idle Residency. (R/O). Pkg_Cx_Duty_Cycle_Cnt.

  @param  ECX  MSR_SKYLAKE_PKG_HDC_DEEP_RESIDENCY (0x00000656)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_PKG_HDC_DEEP_RESIDENCY);
  @endcode
  @note MSR_SKYLAKE_PKG_HDC_DEEP_RESIDENCY is defined as MSR_PKG_HDC_DEEP_RESIDENCY in SDM.
**/
#define MSR_SKYLAKE_PKG_HDC_DEEP_RESIDENCY       0x00000656


/**
  Package. Core-count Weighted C0 Residency. (R/O). Increment at the same rate
  as the TSC. The increment each cycle is weighted by the number of processor
  cores in the package that reside in C0. If N cores are simultaneously in C0,
  then each cycle the counter increments by N.

  @param  ECX  MSR_SKYLAKE_WEIGHTED_CORE_C0 (0x00000658)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_WEIGHTED_CORE_C0);
  @endcode
  @note MSR_SKYLAKE_WEIGHTED_CORE_C0 is defined as MSR_WEIGHTED_CORE_C0 in SDM.
**/
#define MSR_SKYLAKE_WEIGHTED_CORE_C0             0x00000658


/**
  Package. Any Core C0 Residency. (R/O). Increment at the same rate as the
  TSC. The increment each cycle is one if any processor core in the package is
  in C0.

  @param  ECX  MSR_SKYLAKE_ANY_CORE_C0 (0x00000659)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_ANY_CORE_C0);
  @endcode
  @note MSR_SKYLAKE_ANY_CORE_C0 is defined as MSR_ANY_CORE_C0 in SDM.
**/
#define MSR_SKYLAKE_ANY_CORE_C0                  0x00000659


/**
  Package. Any Graphics Engine C0 Residency. (R/O). Increment at the same rate
  as the TSC. The increment each cycle is one if any processor graphic
  device's compute engines are in C0.

  @param  ECX  MSR_SKYLAKE_ANY_GFXE_C0 (0x0000065A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_ANY_GFXE_C0);
  @endcode
  @note MSR_SKYLAKE_ANY_GFXE_C0 is defined as MSR_ANY_GFXE_C0 in SDM.
**/
#define MSR_SKYLAKE_ANY_GFXE_C0                  0x0000065A


/**
  Package. Core and Graphics Engine Overlapped C0 Residency. (R/O). Increment
  at the same rate as the TSC. The increment each cycle is one if at least one
  compute engine of the processor graphics is in C0 and at least one processor
  core in the package is also in C0.

  @param  ECX  MSR_SKYLAKE_CORE_GFXE_OVERLAP_C0 (0x0000065B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_CORE_GFXE_OVERLAP_C0);
  @endcode
  @note MSR_SKYLAKE_CORE_GFXE_OVERLAP_C0 is defined as MSR_CORE_GFXE_OVERLAP_C0 in SDM.
**/
#define MSR_SKYLAKE_CORE_GFXE_OVERLAP_C0         0x0000065B


/**
  Platform*. Platform Power Limit Control (R/W-L) Allows platform BIOS to
  limit power consumption of the platform devices to the specified values. The
  Long Duration power consumption is specified via Platform_Power_Limit_1 and
  Platform_Power_Limit_1_Time. The Short Duration power consumption limit is
  specified via the Platform_Power_Limit_2 with duration chosen by the
  processor. The processor implements an exponential-weighted algorithm in the
  placement of the time windows.

  @param  ECX  MSR_SKYLAKE_PLATFORM_POWER_LIMIT (0x0000065C)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PLATFORM_POWER_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PLATFORM_POWER_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PLATFORM_POWER_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PLATFORM_POWER_LIMIT);
  AsmWriteMsr64 (MSR_SKYLAKE_PLATFORM_POWER_LIMIT, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_PLATFORM_POWER_LIMIT is defined as MSR_PLATFORM_POWER_LIMIT in SDM.
**/
#define MSR_SKYLAKE_PLATFORM_POWER_LIMIT         0x0000065C

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PLATFORM_POWER_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] Platform Power Limit #1. Average Power limit value which
    /// the platform must not exceed over a time window as specified by
    /// Power_Limit_1_TIME field. The default value is the Thermal Design
    /// Power (TDP) and varies with product skus. The unit is specified in
    /// MSR_RAPLPOWER_UNIT.
    ///
    UINT32  PlatformPowerLimit1:15;
    ///
    /// [Bit 15] Enable Platform Power Limit #1. When set, enables the
    /// processor to apply control policy such that the platform power does
    /// not exceed Platform Power limit #1 over the time window specified by
    /// Power Limit #1 Time Window.
    ///
    UINT32  EnablePlatformPowerLimit1:1;
    ///
    /// [Bit 16] Platform Clamping Limitation #1. When set, allows the
    /// processor to go below the OS requested P states in order to maintain
    /// the power below specified Platform Power Limit #1 value. This bit is
    /// writeable only when CPUID (EAX=6):EAX[4] is set.
    ///
    UINT32  PlatformClampingLimitation1:1;
    ///
    /// [Bits 23:17] Time Window for Platform Power Limit #1. Specifies the
    /// duration of the time window over which Platform Power Limit 1 value
    /// should be maintained for sustained long duration. This field is made
    /// up of two numbers from the following equation: Time Window = (float)
    /// ((1+(X/4))*(2^Y)), where: X. = POWER_LIMIT_1_TIME[23:22] Y. =
    /// POWER_LIMIT_1_TIME[21:17]. The maximum allowed value in this field is
    /// defined in MSR_PKG_POWER_INFO[PKG_MAX_WIN]. The default value is 0DH,
    /// The unit is specified in MSR_RAPLPOWER_UNIT[Time Unit].
    ///
    UINT32  Time:7;
    UINT32  Reserved1:8;
    ///
    /// [Bits 46:32] Platform Power Limit #2. Average Power limit value which
    /// the platform must not exceed over the Short Duration time window
    /// chosen by the processor. The recommended default value is 1.25 times
    /// the Long Duration Power Limit (i.e. Platform Power Limit # 1).
    ///
    UINT32  PlatformPowerLimit2:15;
    ///
    /// [Bit 47] Enable Platform Power Limit #2. When set, enables the
    /// processor to apply control policy such that the platform power does
    /// not exceed Platform Power limit #2 over the Short Duration time window.
    ///
    UINT32  EnablePlatformPowerLimit2:1;
    ///
    /// [Bit 48] Platform Clamping Limitation #2. When set, allows the
    /// processor to go below the OS requested P states in order to maintain
    /// the power below specified Platform Power Limit #2 value.
    ///
    UINT32  PlatformClampingLimitation2:1;
    UINT32  Reserved2:14;
    ///
    /// [Bit 63] Lock. Setting this bit will lock all other bits of this MSR
    /// until system RESET.
    ///
    UINT32  Lock:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_PLATFORM_POWER_LIMIT_REGISTER;


/**
  Thread. Last Branch Record n From IP (R/W) One of 32 triplets of last
  branch record registers on the last branch record stack. This part of the
  stack contains pointers to the source instruction. See also: -  Last Branch
  Record Stack TOS at 1C9H -  Section 17.10.

  @param  ECX  MSR_SKYLAKE_LASTBRANCH_n_FROM_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_LASTBRANCH_16_FROM_IP);
  AsmWriteMsr64 (MSR_SKYLAKE_LASTBRANCH_16_FROM_IP, Msr);
  @endcode
  @note MSR_SKYLAKE_LASTBRANCH_16_FROM_IP is defined as MSR_LASTBRANCH_16_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_17_FROM_IP is defined as MSR_LASTBRANCH_17_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_18_FROM_IP is defined as MSR_LASTBRANCH_18_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_19_FROM_IP is defined as MSR_LASTBRANCH_19_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_20_FROM_IP is defined as MSR_LASTBRANCH_20_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_21_FROM_IP is defined as MSR_LASTBRANCH_21_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_22_FROM_IP is defined as MSR_LASTBRANCH_22_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_23_FROM_IP is defined as MSR_LASTBRANCH_23_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_24_FROM_IP is defined as MSR_LASTBRANCH_24_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_25_FROM_IP is defined as MSR_LASTBRANCH_25_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_26_FROM_IP is defined as MSR_LASTBRANCH_26_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_27_FROM_IP is defined as MSR_LASTBRANCH_27_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_28_FROM_IP is defined as MSR_LASTBRANCH_28_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_29_FROM_IP is defined as MSR_LASTBRANCH_29_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_30_FROM_IP is defined as MSR_LASTBRANCH_30_FROM_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_31_FROM_IP is defined as MSR_LASTBRANCH_31_FROM_IP in SDM.
  @{
**/
#define MSR_SKYLAKE_LASTBRANCH_16_FROM_IP        0x00000690
#define MSR_SKYLAKE_LASTBRANCH_17_FROM_IP        0x00000691
#define MSR_SKYLAKE_LASTBRANCH_18_FROM_IP        0x00000692
#define MSR_SKYLAKE_LASTBRANCH_19_FROM_IP        0x00000693
#define MSR_SKYLAKE_LASTBRANCH_20_FROM_IP        0x00000694
#define MSR_SKYLAKE_LASTBRANCH_21_FROM_IP        0x00000695
#define MSR_SKYLAKE_LASTBRANCH_22_FROM_IP        0x00000696
#define MSR_SKYLAKE_LASTBRANCH_23_FROM_IP        0x00000697
#define MSR_SKYLAKE_LASTBRANCH_24_FROM_IP        0x00000698
#define MSR_SKYLAKE_LASTBRANCH_25_FROM_IP        0x00000699
#define MSR_SKYLAKE_LASTBRANCH_26_FROM_IP        0x0000069A
#define MSR_SKYLAKE_LASTBRANCH_27_FROM_IP        0x0000069B
#define MSR_SKYLAKE_LASTBRANCH_28_FROM_IP        0x0000069C
#define MSR_SKYLAKE_LASTBRANCH_29_FROM_IP        0x0000069D
#define MSR_SKYLAKE_LASTBRANCH_30_FROM_IP        0x0000069E
#define MSR_SKYLAKE_LASTBRANCH_31_FROM_IP        0x0000069F
/// @}


/**
  Package. Indicator of Frequency Clipping in the Processor Graphics (R/W)
  (frequency refers to processor graphics frequency).

  @param  ECX  MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS (0x000006B0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS is defined as MSR_GRAPHICS_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS  0x000006B0

/**
  MSR information returned for MSR index
  #MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] PROCHOT Status (R0) When set, frequency is reduced due to
    /// assertion of external PROCHOT.
    ///
    UINT32  PROCHOT_Status:1;
    ///
    /// [Bit 1] Thermal Status (R0) When set, frequency is reduced due to a
    /// thermal event.
    ///
    UINT32  ThermalStatus:1;
    UINT32  Reserved1:3;
    ///
    /// [Bit 5] Running Average Thermal Limit Status (R0) When set, frequency
    /// is reduced due to running average thermal limit.
    ///
    UINT32  RunningAverageThermalLimitStatus:1;
    ///
    /// [Bit 6] VR Therm Alert Status (R0) When set, frequency is reduced due
    /// to a thermal alert from a processor Voltage Regulator.
    ///
    UINT32  VRThermAlertStatus:1;
    ///
    /// [Bit 7] VR Thermal Design Current Status (R0) When set, frequency is
    /// reduced due to VR TDC limit.
    ///
    UINT32  VRThermalDesignCurrentStatus:1;
    ///
    /// [Bit 8] Other Status (R0) When set, frequency is reduced due to
    /// electrical or other constraints.
    ///
    UINT32  OtherStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 10] Package/Platform-Level Power Limiting PL1 Status (R0) When
    /// set, frequency is reduced due to package/platform-level power limiting
    /// PL1.
    ///
    UINT32  PL1Status:1;
    ///
    /// [Bit 11] Package/Platform-Level PL2 Power Limiting Status (R0) When
    /// set, frequency is reduced due to package/platform-level power limiting
    /// PL2/PL3.
    ///
    UINT32  PL2Status:1;
    ///
    /// [Bit 12] Inefficient Operation Status (R0) When set, processor
    /// graphics frequency is operating below target frequency.
    ///
    UINT32  InefficientOperationStatus:1;
    UINT32  Reserved3:3;
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
    UINT32  Reserved4:3;
    ///
    /// [Bit 21] Running Average Thermal Limit Log  When set, indicates that
    /// the RATL Status bit has asserted since the log bit was last cleared.
    /// This log bit will remain set until cleared by software writing 0.
    ///
    UINT32  RunningAverageThermalLimitLog:1;
    ///
    /// [Bit 22] VR Therm Alert Log  When set, indicates that the VR Therm
    /// Alert Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermAlertLog:1;
    ///
    /// [Bit 23] VR Thermal Design Current Log  When set, indicates that the
    /// VR Therm Alert Status bit has asserted since the log bit was last
    /// cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  VRThermalDesignCurrentLog:1;
    ///
    /// [Bit 24] Other Log  When set, indicates that the OTHER Status bit has
    /// asserted since the log bit was last cleared. This log bit will remain
    /// set until cleared by software writing 0.
    ///
    UINT32  OtherLog:1;
    UINT32  Reserved5:1;
    ///
    /// [Bit 26] Package/Platform-Level PL1 Power Limiting Log  When set,
    /// indicates that the Package/Platform Level PL1 Power Limiting Status
    /// bit has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PL1Log:1;
    ///
    /// [Bit 27] Package/Platform-Level PL2 Power Limiting Log When set,
    /// indicates that the Package/Platform Level PL2 Power Limiting Status
    /// bit has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PL2Log:1;
    ///
    /// [Bit 28] Inefficient Operation Log When set, indicates that the
    /// Inefficient Operation Status bit has asserted since the log bit was
    /// last cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  InefficientOperationLog:1;
    UINT32  Reserved6:3;
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
} MSR_SKYLAKE_GRAPHICS_PERF_LIMIT_REASONS_REGISTER;


/**
  Package. Indicator of Frequency Clipping in the Ring Interconnect (R/W)
  (frequency refers to ring interconnect in the uncore).

  @param  ECX  MSR_SKYLAKE_RING_PERF_LIMIT_REASONS (0x000006B1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_RING_PERF_LIMIT_REASONS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_RING_PERF_LIMIT_REASONS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_RING_PERF_LIMIT_REASONS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_RING_PERF_LIMIT_REASONS);
  AsmWriteMsr64 (MSR_SKYLAKE_RING_PERF_LIMIT_REASONS, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_RING_PERF_LIMIT_REASONS is defined as MSR_RING_PERF_LIMIT_REASONS in SDM.
**/
#define MSR_SKYLAKE_RING_PERF_LIMIT_REASONS      0x000006B1

/**
  MSR information returned for MSR index #MSR_SKYLAKE_RING_PERF_LIMIT_REASONS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] PROCHOT Status (R0) When set, frequency is reduced due to
    /// assertion of external PROCHOT.
    ///
    UINT32  PROCHOT_Status:1;
    ///
    /// [Bit 1] Thermal Status (R0) When set, frequency is reduced due to a
    /// thermal event.
    ///
    UINT32  ThermalStatus:1;
    UINT32  Reserved1:3;
    ///
    /// [Bit 5] Running Average Thermal Limit Status (R0) When set, frequency
    /// is reduced due to running average thermal limit.
    ///
    UINT32  RunningAverageThermalLimitStatus:1;
    ///
    /// [Bit 6] VR Therm Alert Status (R0) When set, frequency is reduced due
    /// to a thermal alert from a processor Voltage Regulator.
    ///
    UINT32  VRThermAlertStatus:1;
    ///
    /// [Bit 7] VR Thermal Design Current Status (R0) When set, frequency is
    /// reduced due to VR TDC limit.
    ///
    UINT32  VRThermalDesignCurrentStatus:1;
    ///
    /// [Bit 8] Other Status (R0) When set, frequency is reduced due to
    /// electrical or other constraints.
    ///
    UINT32  OtherStatus:1;
    UINT32  Reserved2:1;
    ///
    /// [Bit 10] Package/Platform-Level Power Limiting PL1 Status (R0) When
    /// set, frequency is reduced due to package/Platform-level power limiting
    /// PL1.
    ///
    UINT32  PL1Status:1;
    ///
    /// [Bit 11] Package/Platform-Level PL2 Power Limiting Status (R0) When
    /// set, frequency is reduced due to package/Platform-level power limiting
    /// PL2/PL3.
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
    UINT32  Reserved4:3;
    ///
    /// [Bit 21] Running Average Thermal Limit Log  When set, indicates that
    /// the RATL Status bit has asserted since the log bit was last cleared.
    /// This log bit will remain set until cleared by software writing 0.
    ///
    UINT32  RunningAverageThermalLimitLog:1;
    ///
    /// [Bit 22] VR Therm Alert Log  When set, indicates that the VR Therm
    /// Alert Status bit has asserted since the log bit was last cleared. This
    /// log bit will remain set until cleared by software writing 0.
    ///
    UINT32  VRThermAlertLog:1;
    ///
    /// [Bit 23] VR Thermal Design Current Log  When set, indicates that the
    /// VR Therm Alert Status bit has asserted since the log bit was last
    /// cleared. This log bit will remain set until cleared by software
    /// writing 0.
    ///
    UINT32  VRThermalDesignCurrentLog:1;
    ///
    /// [Bit 24] Other Log  When set, indicates that the OTHER Status bit has
    /// asserted since the log bit was last cleared. This log bit will remain
    /// set until cleared by software writing 0.
    ///
    UINT32  OtherLog:1;
    UINT32  Reserved5:1;
    ///
    /// [Bit 26] Package/Platform-Level PL1 Power Limiting Log  When set,
    /// indicates that the Package/Platform Level PL1 Power Limiting Status
    /// bit has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PL1Log:1;
    ///
    /// [Bit 27] Package/Platform-Level PL2 Power Limiting Log When set,
    /// indicates that the Package/Platform Level PL2 Power Limiting Status
    /// bit has asserted since the log bit was last cleared. This log bit will
    /// remain set until cleared by software writing 0.
    ///
    UINT32  PL2Log:1;
    UINT32  Reserved6:4;
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
} MSR_SKYLAKE_RING_PERF_LIMIT_REASONS_REGISTER;


/**
  Thread. Last Branch Record n To IP (R/W) One of 32 triplets of last branch
  record registers on the last branch record stack. This part of the stack
  contains pointers to the destination instruction. See also: -  Last Branch
  Record Stack TOS at 1C9H -  Section 17.10.

  @param  ECX  MSR_SKYLAKE_LASTBRANCH_n_TO_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_LASTBRANCH_16_TO_IP);
  AsmWriteMsr64 (MSR_SKYLAKE_LASTBRANCH_16_TO_IP, Msr);
  @endcode
  @note MSR_SKYLAKE_LASTBRANCH_16_TO_IP is defined as MSR_LASTBRANCH_16_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_17_TO_IP is defined as MSR_LASTBRANCH_17_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_18_TO_IP is defined as MSR_LASTBRANCH_18_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_19_TO_IP is defined as MSR_LASTBRANCH_19_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_20_TO_IP is defined as MSR_LASTBRANCH_20_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_21_TO_IP is defined as MSR_LASTBRANCH_21_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_22_TO_IP is defined as MSR_LASTBRANCH_22_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_23_TO_IP is defined as MSR_LASTBRANCH_23_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_24_TO_IP is defined as MSR_LASTBRANCH_24_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_25_TO_IP is defined as MSR_LASTBRANCH_25_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_26_TO_IP is defined as MSR_LASTBRANCH_26_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_27_TO_IP is defined as MSR_LASTBRANCH_27_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_28_TO_IP is defined as MSR_LASTBRANCH_28_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_29_TO_IP is defined as MSR_LASTBRANCH_29_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_30_TO_IP is defined as MSR_LASTBRANCH_30_TO_IP in SDM.
        MSR_SKYLAKE_LASTBRANCH_31_TO_IP is defined as MSR_LASTBRANCH_31_TO_IP in SDM.
  @{
**/
#define MSR_SKYLAKE_LASTBRANCH_16_TO_IP          0x000006D0
#define MSR_SKYLAKE_LASTBRANCH_17_TO_IP          0x000006D1
#define MSR_SKYLAKE_LASTBRANCH_18_TO_IP          0x000006D2
#define MSR_SKYLAKE_LASTBRANCH_19_TO_IP          0x000006D3
#define MSR_SKYLAKE_LASTBRANCH_20_TO_IP          0x000006D4
#define MSR_SKYLAKE_LASTBRANCH_21_TO_IP          0x000006D5
#define MSR_SKYLAKE_LASTBRANCH_22_TO_IP          0x000006D6
#define MSR_SKYLAKE_LASTBRANCH_23_TO_IP          0x000006D7
#define MSR_SKYLAKE_LASTBRANCH_24_TO_IP          0x000006D8
#define MSR_SKYLAKE_LASTBRANCH_25_TO_IP          0x000006D9
#define MSR_SKYLAKE_LASTBRANCH_26_TO_IP          0x000006DA
#define MSR_SKYLAKE_LASTBRANCH_27_TO_IP          0x000006DB
#define MSR_SKYLAKE_LASTBRANCH_28_TO_IP          0x000006DC
#define MSR_SKYLAKE_LASTBRANCH_29_TO_IP          0x000006DD
#define MSR_SKYLAKE_LASTBRANCH_30_TO_IP          0x000006DE
#define MSR_SKYLAKE_LASTBRANCH_31_TO_IP          0x000006DF
/// @}


/**
  Thread. Last Branch Record n Additional Information (R/W) One of 32 triplet
  of last branch record registers on the last branch record stack. This part
  of the stack contains flag, TSX-related and elapsed cycle information. See
  also: -  Last Branch Record Stack TOS at 1C9H -  Section 17.7.1, "LBR
  Stack.".

  @param  ECX  MSR_SKYLAKE_LBR_INFO_n
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_LBR_INFO_0);
  AsmWriteMsr64 (MSR_SKYLAKE_LBR_INFO_0, Msr);
  @endcode
  @note MSR_SKYLAKE_LBR_INFO_0  is defined as MSR_LBR_INFO_0  in SDM.
        MSR_SKYLAKE_LBR_INFO_1  is defined as MSR_LBR_INFO_1  in SDM.
        MSR_SKYLAKE_LBR_INFO_2  is defined as MSR_LBR_INFO_2  in SDM.
        MSR_SKYLAKE_LBR_INFO_3  is defined as MSR_LBR_INFO_3  in SDM.
        MSR_SKYLAKE_LBR_INFO_4  is defined as MSR_LBR_INFO_4  in SDM.
        MSR_SKYLAKE_LBR_INFO_5  is defined as MSR_LBR_INFO_5  in SDM.
        MSR_SKYLAKE_LBR_INFO_6  is defined as MSR_LBR_INFO_6  in SDM.
        MSR_SKYLAKE_LBR_INFO_7  is defined as MSR_LBR_INFO_7  in SDM.
        MSR_SKYLAKE_LBR_INFO_8  is defined as MSR_LBR_INFO_8  in SDM.
        MSR_SKYLAKE_LBR_INFO_9  is defined as MSR_LBR_INFO_9  in SDM.
        MSR_SKYLAKE_LBR_INFO_10 is defined as MSR_LBR_INFO_10 in SDM.
        MSR_SKYLAKE_LBR_INFO_11 is defined as MSR_LBR_INFO_11 in SDM.
        MSR_SKYLAKE_LBR_INFO_12 is defined as MSR_LBR_INFO_12 in SDM.
        MSR_SKYLAKE_LBR_INFO_13 is defined as MSR_LBR_INFO_13 in SDM.
        MSR_SKYLAKE_LBR_INFO_14 is defined as MSR_LBR_INFO_14 in SDM.
        MSR_SKYLAKE_LBR_INFO_15 is defined as MSR_LBR_INFO_15 in SDM.
        MSR_SKYLAKE_LBR_INFO_16 is defined as MSR_LBR_INFO_16 in SDM.
        MSR_SKYLAKE_LBR_INFO_17 is defined as MSR_LBR_INFO_17 in SDM.
        MSR_SKYLAKE_LBR_INFO_18 is defined as MSR_LBR_INFO_18 in SDM.
        MSR_SKYLAKE_LBR_INFO_19 is defined as MSR_LBR_INFO_19 in SDM.
        MSR_SKYLAKE_LBR_INFO_20 is defined as MSR_LBR_INFO_20 in SDM.
        MSR_SKYLAKE_LBR_INFO_21 is defined as MSR_LBR_INFO_21 in SDM.
        MSR_SKYLAKE_LBR_INFO_22 is defined as MSR_LBR_INFO_22 in SDM.
        MSR_SKYLAKE_LBR_INFO_23 is defined as MSR_LBR_INFO_23 in SDM.
        MSR_SKYLAKE_LBR_INFO_24 is defined as MSR_LBR_INFO_24 in SDM.
        MSR_SKYLAKE_LBR_INFO_25 is defined as MSR_LBR_INFO_25 in SDM.
        MSR_SKYLAKE_LBR_INFO_26 is defined as MSR_LBR_INFO_26 in SDM.
        MSR_SKYLAKE_LBR_INFO_27 is defined as MSR_LBR_INFO_27 in SDM.
        MSR_SKYLAKE_LBR_INFO_28 is defined as MSR_LBR_INFO_28 in SDM.
        MSR_SKYLAKE_LBR_INFO_29 is defined as MSR_LBR_INFO_29 in SDM.
        MSR_SKYLAKE_LBR_INFO_30 is defined as MSR_LBR_INFO_30 in SDM.
        MSR_SKYLAKE_LBR_INFO_31 is defined as MSR_LBR_INFO_31 in SDM.
  @{
**/
#define MSR_SKYLAKE_LBR_INFO_0                   0x00000DC0
#define MSR_SKYLAKE_LBR_INFO_1                   0x00000DC1
#define MSR_SKYLAKE_LBR_INFO_2                   0x00000DC2
#define MSR_SKYLAKE_LBR_INFO_3                   0x00000DC3
#define MSR_SKYLAKE_LBR_INFO_4                   0x00000DC4
#define MSR_SKYLAKE_LBR_INFO_5                   0x00000DC5
#define MSR_SKYLAKE_LBR_INFO_6                   0x00000DC6
#define MSR_SKYLAKE_LBR_INFO_7                   0x00000DC7
#define MSR_SKYLAKE_LBR_INFO_8                   0x00000DC8
#define MSR_SKYLAKE_LBR_INFO_9                   0x00000DC9
#define MSR_SKYLAKE_LBR_INFO_10                  0x00000DCA
#define MSR_SKYLAKE_LBR_INFO_11                  0x00000DCB
#define MSR_SKYLAKE_LBR_INFO_12                  0x00000DCC
#define MSR_SKYLAKE_LBR_INFO_13                  0x00000DCD
#define MSR_SKYLAKE_LBR_INFO_14                  0x00000DCE
#define MSR_SKYLAKE_LBR_INFO_15                  0x00000DCF
#define MSR_SKYLAKE_LBR_INFO_16                  0x00000DD0
#define MSR_SKYLAKE_LBR_INFO_17                  0x00000DD1
#define MSR_SKYLAKE_LBR_INFO_18                  0x00000DD2
#define MSR_SKYLAKE_LBR_INFO_19                  0x00000DD3
#define MSR_SKYLAKE_LBR_INFO_20                  0x00000DD4
#define MSR_SKYLAKE_LBR_INFO_21                  0x00000DD5
#define MSR_SKYLAKE_LBR_INFO_22                  0x00000DD6
#define MSR_SKYLAKE_LBR_INFO_23                  0x00000DD7
#define MSR_SKYLAKE_LBR_INFO_24                  0x00000DD8
#define MSR_SKYLAKE_LBR_INFO_25                  0x00000DD9
#define MSR_SKYLAKE_LBR_INFO_26                  0x00000DDA
#define MSR_SKYLAKE_LBR_INFO_27                  0x00000DDB
#define MSR_SKYLAKE_LBR_INFO_28                  0x00000DDC
#define MSR_SKYLAKE_LBR_INFO_29                  0x00000DDD
#define MSR_SKYLAKE_LBR_INFO_30                  0x00000DDE
#define MSR_SKYLAKE_LBR_INFO_31                  0x00000DDF
/// @}


/**
  Package. Uncore fixed counter control (R/W).

  @param  ECX  MSR_SKYLAKE_UNC_PERF_FIXED_CTRL (0x00000394)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_PERF_FIXED_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_PERF_FIXED_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_UNC_PERF_FIXED_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_UNC_PERF_FIXED_CTRL);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_PERF_FIXED_CTRL, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_UNC_PERF_FIXED_CTRL is defined as MSR_UNC_PERF_FIXED_CTRL in SDM.
**/
#define MSR_SKYLAKE_UNC_PERF_FIXED_CTRL          0x00000394

/**
  MSR information returned for MSR index #MSR_SKYLAKE_UNC_PERF_FIXED_CTRL
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
} MSR_SKYLAKE_UNC_PERF_FIXED_CTRL_REGISTER;


/**
  Package. Uncore fixed counter.

  @param  ECX  MSR_SKYLAKE_UNC_PERF_FIXED_CTR (0x00000395)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_PERF_FIXED_CTR_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_PERF_FIXED_CTR_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_UNC_PERF_FIXED_CTR_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_UNC_PERF_FIXED_CTR);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_PERF_FIXED_CTR, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_UNC_PERF_FIXED_CTR is defined as MSR_UNC_PERF_FIXED_CTR in SDM.
**/
#define MSR_SKYLAKE_UNC_PERF_FIXED_CTR           0x00000395

/**
  MSR information returned for MSR index #MSR_SKYLAKE_UNC_PERF_FIXED_CTR
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
    /// [Bits 43:32] Current count.
    ///
    UINT32  CurrentCountHi:12;
    UINT32  Reserved:20;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_UNC_PERF_FIXED_CTR_REGISTER;


/**
  Package. Uncore C-Box configuration information (R/O).

  @param  ECX  MSR_SKYLAKE_UNC_CBO_CONFIG (0x00000396)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_CBO_CONFIG_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_CBO_CONFIG_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_UNC_CBO_CONFIG_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_CONFIG);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_CONFIG is defined as MSR_UNC_CBO_CONFIG in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_CONFIG               0x00000396

/**
  MSR information returned for MSR index #MSR_SKYLAKE_UNC_CBO_CONFIG
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 3:0] Specifies the number of C-Box units with programmable
    /// counters (including processor cores and processor graphics),.
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
} MSR_SKYLAKE_UNC_CBO_CONFIG_REGISTER;


/**
  Package. Uncore Arb unit, performance counter 0.

  @param  ECX  MSR_SKYLAKE_UNC_ARB_PERFCTR0 (0x000003B0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_ARB_PERFCTR0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_ARB_PERFCTR0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_ARB_PERFCTR0 is defined as MSR_UNC_ARB_PERFCTR0 in SDM.
**/
#define MSR_SKYLAKE_UNC_ARB_PERFCTR0             0x000003B0


/**
  Package. Uncore Arb unit, performance counter 1.

  @param  ECX  MSR_SKYLAKE_UNC_ARB_PERFCTR1 (0x000003B1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_ARB_PERFCTR1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_ARB_PERFCTR1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_ARB_PERFCTR1 is defined as MSR_UNC_ARB_PERFCTR1 in SDM.
**/
#define MSR_SKYLAKE_UNC_ARB_PERFCTR1             0x000003B1


/**
  Package. Uncore Arb unit, counter 0 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_ARB_PERFEVTSEL0 (0x000003B2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_ARB_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_ARB_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_ARB_PERFEVTSEL0 is defined as MSR_UNC_ARB_PERFEVTSEL0 in SDM.
**/
#define MSR_SKYLAKE_UNC_ARB_PERFEVTSEL0          0x000003B2


/**
  Package. Uncore Arb unit, counter 1 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_ARB_PERFEVTSEL1 (0x000003B3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_ARB_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_ARB_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_ARB_PERFEVTSEL1 is defined as MSR_SKYLAKE_UNC_ARB_PERFEVTSEL1 in SDM.
**/
#define MSR_SKYLAKE_UNC_ARB_PERFEVTSEL1          0x000003B3


/**
  Package. Uncore C-Box 0, counter 0 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL0 (0x00000700)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL0 is defined as MSR_UNC_CBO_0_PERFEVTSEL0 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL0        0x00000700


/**
  Package. Uncore C-Box 0, counter 1 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL1 (0x00000701)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL1 is defined as MSR_UNC_CBO_0_PERFEVTSEL1 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_0_PERFEVTSEL1        0x00000701


/**
  Package. Uncore C-Box 0, performance counter 0.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_0_PERFCTR0 (0x00000706)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_0_PERFCTR0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_0_PERFCTR0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_0_PERFCTR0 is defined as MSR_UNC_CBO_0_PERFCTR0 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_0_PERFCTR0           0x00000706


/**
  Package. Uncore C-Box 0, performance counter 1.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_0_PERFCTR1 (0x00000707)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_0_PERFCTR1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_0_PERFCTR1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_0_PERFCTR1 is defined as MSR_UNC_CBO_0_PERFCTR1 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_0_PERFCTR1           0x00000707


/**
  Package. Uncore C-Box 1, counter 0 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL0 (0x00000710)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL0 is defined as MSR_UNC_CBO_1_PERFEVTSEL0 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL0        0x00000710


/**
  Package. Uncore C-Box 1, counter 1 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL1 (0x00000711)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL1 is defined as MSR_UNC_CBO_1_PERFEVTSEL1 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_1_PERFEVTSEL1        0x00000711


/**
  Package. Uncore C-Box 1, performance counter 0.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_1_PERFCTR0 (0x00000716)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_1_PERFCTR0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_1_PERFCTR0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_1_PERFCTR0 is defined as MSR_UNC_CBO_1_PERFCTR0 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_1_PERFCTR0           0x00000716


/**
  Package. Uncore C-Box 1, performance counter 1.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_1_PERFCTR1 (0x00000717)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_1_PERFCTR1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_1_PERFCTR1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_1_PERFCTR1 is defined as MSR_UNC_CBO_1_PERFCTR1 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_1_PERFCTR1           0x00000717


/**
  Package. Uncore C-Box 2, counter 0 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL0 (0x00000720)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL0 is defined as MSR_UNC_CBO_2_PERFEVTSEL0 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL0        0x00000720


/**
  Package. Uncore C-Box 2, counter 1 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL1 (0x00000721)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL1 is defined as MSR_UNC_CBO_2_PERFEVTSEL1 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_2_PERFEVTSEL1        0x00000721


/**
  Package. Uncore C-Box 2, performance counter 0.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_2_PERFCTR0 (0x00000726)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_2_PERFCTR0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_2_PERFCTR0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_2_PERFCTR0 is defined as MSR_UNC_CBO_2_PERFCTR0 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_2_PERFCTR0           0x00000726


/**
  Package. Uncore C-Box 2, performance counter 1.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_2_PERFCTR1 (0x00000727)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_2_PERFCTR1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_2_PERFCTR1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_2_PERFCTR1 is defined as MSR_UNC_CBO_2_PERFCTR1 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_2_PERFCTR1           0x00000727


/**
  Package. Uncore C-Box 3, counter 0 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL0 (0x00000730)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL0 is defined as MSR_UNC_CBO_3_PERFEVTSEL0 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL0        0x00000730


/**
  Package. Uncore C-Box 3, counter 1 event select MSR.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL1 (0x00000731)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL1 is defined as MSR_UNC_CBO_3_PERFEVTSEL1 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_3_PERFEVTSEL1        0x00000731


/**
  Package. Uncore C-Box 3, performance counter 0.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_3_PERFCTR0 (0x00000736)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_3_PERFCTR0);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_3_PERFCTR0, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_3_PERFCTR0 is defined as MSR_UNC_CBO_3_PERFCTR0 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_3_PERFCTR0           0x00000736


/**
  Package. Uncore C-Box 3, performance counter 1.

  @param  ECX  MSR_SKYLAKE_UNC_CBO_3_PERFCTR1 (0x00000737)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_UNC_CBO_3_PERFCTR1);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_CBO_3_PERFCTR1, Msr);
  @endcode
  @note MSR_SKYLAKE_UNC_CBO_3_PERFCTR1 is defined as MSR_UNC_CBO_3_PERFCTR1 in SDM.
**/
#define MSR_SKYLAKE_UNC_CBO_3_PERFCTR1           0x00000737


/**
  Package. Uncore PMU global control.

  @param  ECX  MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL (0x00000E01)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL is defined as MSR_UNC_PERF_GLOBAL_CTRL in SDM.
**/
#define MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL         0x00000E01

/**
  MSR information returned for MSR index #MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Slice 0 select.
    ///
    UINT32  PMI_Sel_Slice0:1;
    ///
    /// [Bit 1] Slice 1 select.
    ///
    UINT32  PMI_Sel_Slice1:1;
    ///
    /// [Bit 2] Slice 2 select.
    ///
    UINT32  PMI_Sel_Slice2:1;
    ///
    /// [Bit 3] Slice 3 select.
    ///
    UINT32  PMI_Sel_Slice3:1;
    ///
    /// [Bit 4] Slice 4select.
    ///
    UINT32  PMI_Sel_Slice4:1;
    UINT32  Reserved1:14;
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
} MSR_SKYLAKE_UNC_PERF_GLOBAL_CTRL_REGISTER;


/**
  Package. Uncore PMU main status.

  @param  ECX  MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS (0x00000E02)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS, Msr.Uint64);
  @endcode
  @note MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS is defined as MSR_UNC_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS       0x00000E02

/**
  MSR information returned for MSR index #MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS
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
} MSR_SKYLAKE_UNC_PERF_GLOBAL_STATUS_REGISTER;


/**
  Package. NPK Address Used by AET Messages (R/W).

  @param  ECX  MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE (0x00000080)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE);
  AsmWriteMsr64 (MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE   0x00000080

/**
  MSR information returned for MSR index
  #MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Lock Bit If set, this MSR cannot be re-written anymore. Lock
    /// bit has to be set in order for the AET packets to be directed to NPK
    /// MMIO.
    ///
    UINT32  Fix_Me_1:1;
    UINT32  Reserved:17;
    ///
    /// [Bits 31:18] ACPIBAR_BASE_ADDRESS AET target address in NPK MMIO space.
    ///
    UINT32  ACPIBAR_BASE_ADDRESS:14;
    ///
    /// [Bits 63:32] ACPIBAR_BASE_ADDRESS AET target address in NPK MMIO space.
    ///
    UINT32  Fix_Me_2:32;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_TRACE_HUB_STH_ACPIBAR_BASE_REGISTER;


/**
  Core. Processor Reserved Memory Range Register - Physical Base Control
  Register (R/W).

  @param  ECX  MSR_SKYLAKE_PRMRR_PHYS_BASE (0x000001F4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PRMRR_PHYS_BASE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PRMRR_PHYS_BASE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PRMRR_PHYS_BASE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PRMRR_PHYS_BASE);
  AsmWriteMsr64 (MSR_SKYLAKE_PRMRR_PHYS_BASE, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_PRMRR_PHYS_BASE              0x000001F4

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PRMRR_PHYS_BASE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] MemType PRMRR BASE MemType.
    ///
    UINT32  MemTypePRMRRBASEMemType:3;
    UINT32  Reserved1:9;
    ///
    /// [Bits 31:12] Base PRMRR Base Address.
    ///
    UINT32  BasePRMRRBaseAddress:20;
    ///
    /// [Bits 45:32] Base PRMRR Base Address.
    ///
    UINT32  Fix_Me_1:14;
    UINT32  Reserved2:18;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_PRMRR_PHYS_BASE_REGISTER;


/**
  Core. Processor Reserved Memory Range Register - Physical Mask Control
  Register (R/W).

  @param  ECX  MSR_SKYLAKE_PRMRR_PHYS_MASK (0x000001F5)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PRMRR_PHYS_MASK_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PRMRR_PHYS_MASK_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PRMRR_PHYS_MASK_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PRMRR_PHYS_MASK);
  AsmWriteMsr64 (MSR_SKYLAKE_PRMRR_PHYS_MASK, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_PRMRR_PHYS_MASK              0x000001F5

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PRMRR_PHYS_MASK
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:10;
    ///
    /// [Bit 10] Lock Lock bit for the PRMRR.
    ///
    UINT32  Fix_Me_1:1;
    ///
    /// [Bit 11] VLD Enable bit for the PRMRR.
    ///
    UINT32  VLD:1;
    ///
    /// [Bits 31:12] Mask PRMRR MASK bits.
    ///
    UINT32  Fix_Me_2:20;
    ///
    /// [Bits 45:32] Mask PRMRR MASK bits.
    ///
    UINT32  Fix_Me_3:14;
    UINT32  Reserved2:18;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_PRMRR_PHYS_MASK_REGISTER;


/**
  Core. Valid PRMRR Configurations (R/W).

  @param  ECX  MSR_SKYLAKE_PRMRR_VALID_CONFIG (0x000001FB)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PRMRR_VALID_CONFIG_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PRMRR_VALID_CONFIG_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PRMRR_VALID_CONFIG_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PRMRR_VALID_CONFIG);
  AsmWriteMsr64 (MSR_SKYLAKE_PRMRR_VALID_CONFIG, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_PRMRR_VALID_CONFIG           0x000001FB

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PRMRR_VALID_CONFIG
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] 1M supported MEE size.
    ///
    UINT32  Fix_Me_1:1;
    UINT32  Reserved1:4;
    ///
    /// [Bit 5] 32M supported MEE size.
    ///
    UINT32  Fix_Me_2:1;
    ///
    /// [Bit 6] 64M supported MEE size.
    ///
    UINT32  Fix_Me_3:1;
    ///
    /// [Bit 7] 128M supported MEE size.
    ///
    UINT32  Fix_Me_4:1;
    UINT32  Reserved2:24;
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
} MSR_SKYLAKE_PRMRR_VALID_CONFIG_REGISTER;


/**
  Package. (R/W) The PRMRR range is used to protect Xucode memory from
  unauthorized reads and writes. Any IO access to this range is aborted. This
  register controls the location of the PRMRR range by indicating its starting
  address. It functions in tandem with the PRMRR mask register.

  @param  ECX  MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE (0x000002F4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE);
  AsmWriteMsr64 (MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE       0x000002F4

/**
  MSR information returned for MSR index #MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:12;
    ///
    /// [Bits 31:12] Range Base This field corresponds to bits 38:12 of the
    /// base address memory range which is allocated to PRMRR memory.
    ///
    UINT32  Fix_Me_1:20;
    ///
    /// [Bits 38:32] Range Base This field corresponds to bits 38:12 of the
    /// base address memory range which is allocated to PRMRR memory.
    ///
    UINT32  Fix_Me_2:7;
    UINT32  Reserved2:25;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_UNCORE_PRMRR_PHYS_BASE_REGISTER;


/**
  Package. (R/W) This register controls the size of the PRMRR range by
  indicating which address bits must match the PRMRR base register value.

  @param  ECX  MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK (0x000002F5)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK);
  AsmWriteMsr64 (MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK       0x000002F5

/**
  MSR information returned for MSR index #MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:10;
    ///
    /// [Bit 10] Lock Setting this bit locks all writeable settings in this
    /// register, including itself.
    ///
    UINT32  Fix_Me_1:1;
    ///
    /// [Bit 11] Range_En Indicates whether the PRMRR range is enabled and
    /// valid.
    ///
    UINT32  Fix_Me_2:1;
    UINT32  Reserved2:20;
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
} MSR_SKYLAKE_UNCORE_PRMRR_PHYS_MASK_REGISTER;

/**
  Package. Ring Ratio Limit (R/W) This register provides Min/Max Ratio Limits
  for the LLC and Ring.

  @param  ECX  MSR_SKYLAKE_RING_RATIO_LIMIT (0x00000620)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_RING_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_RING_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_RING_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_RING_RATIO_LIMIT);
  AsmWriteMsr64 (MSR_SKYLAKE_RING_RATIO_LIMIT, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_RING_RATIO_LIMIT             0x00000620

/**
  MSR information returned for MSR index #MSR_SKYLAKE_RING_RATIO_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 6:0] MAX_Ratio This field is used to limit the max ratio of the
    /// LLC/Ring.
    ///
    UINT32  Fix_Me_1:7;
    UINT32  Reserved1:1;
    ///
    /// [Bits 14:8] MIN_Ratio Writing to this field controls the minimum
    /// possible ratio of the LLC/Ring.
    ///
    UINT32  Fix_Me_2:7;
    UINT32  Reserved2:17;
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
} MSR_SKYLAKE_RING_RATIO_LIMIT_REGISTER;


/**
  Branch Monitoring Global Control (R/W).

  @param  ECX  MSR_SKYLAKE_BR_DETECT_CTRL (0x00000350)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_BR_DETECT_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_BR_DETECT_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_BR_DETECT_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_BR_DETECT_CTRL);
  AsmWriteMsr64 (MSR_SKYLAKE_BR_DETECT_CTRL, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_BR_DETECT_CTRL               0x00000350

/**
  MSR information returned for MSR index #MSR_SKYLAKE_BR_DETECT_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] EnMonitoring Global enable for branch monitoring.
    ///
    UINT32  EnMonitoring:1;
    ///
    /// [Bit 1] EnExcept Enable branch monitoring event signaling on threshold
    /// trip. The branch monitoring event handler is signaled via the existing
    /// PMI signaling mechanism as programmed from the corresponding local
    /// APIC LVT entry.
    ///
    UINT32  EnExcept:1;
    ///
    /// [Bit 2] EnLBRFrz Enable LBR freeze on threshold trip. This will cause
    /// the LBR frozen bit 58 to be set in IA32_PERF_GLOBAL_STATUS when a
    /// triggering condition occurs and this bit is enabled.
    ///
    UINT32  EnLBRFrz:1;
    ///
    /// [Bit 3] DisableInGuest When set to '1', branch monitoring, event
    /// triggering and LBR freeze actions are disabled when operating at VMX
    /// non-root operation.
    ///
    UINT32  DisableInGuest:1;
    UINT32  Reserved1:4;
    ///
    /// [Bits 17:8] WindowSize Window size defined by WindowCntSel. Values 0 -
    /// 1023 are supported. Once the Window counter reaches the WindowSize
    /// count both the Window Counter and all Branch Monitoring Counters are
    /// cleared.
    ///
    UINT32  WindowSize:10;
    UINT32  Reserved2:6;
    ///
    /// [Bits 25:24] WindowCntSel Window event count select: '00 =
    /// Instructions retired. '01 = Branch instructions retired '10 = Return
    /// instructions retired. '11 = Indirect branch instructions retired.
    ///
    UINT32  WindowCntSel:2;
    ///
    /// [Bit 26] CntAndMode When set to '1', the overall branch monitoring
    /// event triggering condition is true only if all enabled counters'
    /// threshold conditions are true. When '0', the threshold tripping
    /// condition is true if any enabled counters' threshold is true.
    ///
    UINT32  CntAndMode:1;
    UINT32  Reserved3:5;
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
} MSR_SKYLAKE_BR_DETECT_CTRL_REGISTER;

/**
  Branch Monitoring Global Status (R/W).

  @param  ECX  MSR_SKYLAKE_BR_DETECT_STATUS (0x00000351)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_BR_DETECT_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_BR_DETECT_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_BR_DETECT_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_BR_DETECT_STATUS);
  AsmWriteMsr64 (MSR_SKYLAKE_BR_DETECT_STATUS, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_BR_DETECT_STATUS             0x00000351

/**
  MSR information returned for MSR index #MSR_SKYLAKE_BR_DETECT_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Branch Monitoring Event Signaled When set to '1', Branch
    /// Monitoring event signaling is blocked until this bit is cleared by
    /// software.
    ///
    UINT32  BranchMonitoringEventSignaled:1;
    ///
    /// [Bit 1] LBRsValid This status bit is set to '1' if the LBR state is
    /// considered valid for sampling by branch monitoring software.
    ///
    UINT32  LBRsValid:1;
    UINT32  Reserved1:6;
    ///
    /// [Bit 8] CntrHit0 Branch monitoring counter #0 threshold hit. This
    /// status bit is sticky and once set requires clearing by software.
    /// Counter operation continues independent of the state of the bit.
    ///
    UINT32  CntrHit0:1;
    ///
    /// [Bit 9] CntrHit1 Branch monitoring counter #1 threshold hit. This
    /// status bit is sticky and once set requires clearing by software.
    /// Counter operation continues independent of the state of the bit.
    ///
    UINT32  CntrHit1:1;
    UINT32  Reserved2:6;
    ///
    /// [Bits 25:16] CountWindow The current value of the window counter. The
    /// count value is frozen on a valid branch monitoring triggering
    /// condition. This is a 10-bit unsigned value.
    ///
    UINT32  CountWindow:10;
    UINT32  Reserved3:6;
    ///
    /// [Bits 39:32] Count0 The current value of counter 0 updated after each
    /// occurrence of the event being counted. The count value is frozen on a
    /// valid branch monitoring triggering condition (in which case CntrHit0
    /// will also be set). This is an 8-bit signed value (2's complement).
    /// Heuristic events which only increment will saturate and freeze at
    /// maximum value 0xFF (256). RET-CALL event counter saturate at maximum
    /// value 0x7F (+127) and minimum value 0x80 (-128).
    ///
    UINT32  Count0:8;
    ///
    /// [Bits 47:40] Count1 The current value of counter 1 updated after each
    /// occurrence of the event being counted. The count value is frozen on a
    /// valid branch monitoring triggering condition (in which case CntrHit1
    /// will also be set). This is an 8-bit signed value (2's complement).
    /// Heuristic events which only increment will saturate and freeze at
    /// maximum value 0xFF (256). RET-CALL event counter saturate at maximum
    /// value 0x7F (+127) and minimum value 0x80 (-128).
    ///
    UINT32  Count1:8;
    UINT32  Reserved4:16;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32  Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_BR_DETECT_STATUS_REGISTER;


/**
  Package. Package C3 Residency Counter (R/O). Note: C-state values are
  processor specific C-state code names, unrelated to MWAIT extension C-state
  parameters or ACPI C-states.

  @param  ECX  MSR_SKYLAKE_PKG_C3_RESIDENCY (0x000003F8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_PKG_C3_RESIDENCY);
  @endcode
**/
#define MSR_SKYLAKE_PKG_C3_RESIDENCY             0x000003F8


/**
  Core. Core C1 Residency Counter (R/O). Value since last reset for the Core
  C1 residency. Counter rate is the Max Non-Turbo frequency (same as TSC).
  This counter counts in case both of the core's threads are in an idle state
  and at least one of the core's thread residency is in a C1 state or in one
  of its sub states. The counter is updated only after a core C state exit.
  Note: Always reads 0 if core C1 is unsupported. A value of zero indicates
  that this processor does not support core C1 or never entered core C1 level
  state.

  @param  ECX  MSR_SKYLAKE_CORE_C1_RESIDENCY (0x00000660)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_CORE_C1_RESIDENCY);
  @endcode
**/
#define MSR_SKYLAKE_CORE_C1_RESIDENCY            0x00000660


/**
  Core. Core C3 Residency Counter (R/O). Will always return 0.

  @param  ECX  MSR_SKYLAKE_CORE_C3_RESIDENCY (0x00000662)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_CORE_C3_RESIDENCY);
  @endcode
**/
#define MSR_SKYLAKE_CORE_C3_RESIDENCY            0x00000662


/**
  Package. Protected Processor Inventory Number Enable Control (R/W).

  @param  ECX  MSR_SKYLAKE_PPIN_CTL (0x0000004E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PPIN_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PPIN_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PPIN_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PPIN_CTL);
  AsmWriteMsr64 (MSR_SKYLAKE_PPIN_CTL, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_PPIN_CTL                     0x0000004E

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PPIN_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] LockOut (R/WO) See Table 2-25.
    ///
    UINT32  LockOut:1;
    ///
    /// [Bit 1] Enable_PPIN (R/W) See Table 2-25.
    ///
    UINT32  Enable_PPIN:1;
    UINT32  Reserved1:30;
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
} MSR_SKYLAKE_PPIN_CTL_REGISTER;


/**
  Package. Protected Processor Inventory Number (R/O). Protected Processor
  Inventory Number (R/O) See Table 2-25.

  @param  ECX  MSR_SKYLAKE_PPIN (0x0000004F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_PPIN);
  @endcode
**/
#define MSR_SKYLAKE_PPIN                         0x0000004F


/**
  Package. Platform Information Contains power management and other model
  specific features enumeration. See http://biosbits.org.

  @param  ECX  MSR_SKYLAKE_PLATFORM_INFO (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PLATFORM_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PLATFORM_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PLATFORM_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PLATFORM_INFO);
  AsmWriteMsr64 (MSR_SKYLAKE_PLATFORM_INFO, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_PLATFORM_INFO                0x000000CE

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PLATFORM_INFO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:8;
    ///
    /// [Bits 15:8] Package. Maximum Non-Turbo Ratio (R/O) See Table 2-25.
    ///
    UINT32  MaximumNon_TurboRatio:8;
    UINT32  Reserved2:7;
    ///
    /// [Bit 23] Package. PPIN_CAP (R/O) See Table 2-25.
    ///
    UINT32  PPIN_CAP:1;
    UINT32  Reserved3:4;
    ///
    /// [Bit 28] Package. Programmable Ratio Limit for Turbo Mode (R/O) See
    /// Table 2-25.
    ///
    UINT32  ProgrammableRatioLimit:1;
    ///
    /// [Bit 29] Package. Programmable TDP Limit for Turbo Mode (R/O) See
    /// Table 2-25.
    ///
    UINT32  ProgrammableTDPLimit:1;
    ///
    /// [Bit 30] Package. Programmable TJ OFFSET (R/O) See Table 2-25.
    ///
    UINT32  ProgrammableTJOFFSET:1;
    UINT32  Reserved4:1;
    UINT32  Reserved5:8;
    ///
    /// [Bits 47:40] Package. Maximum Efficiency Ratio (R/O) See Table 2-25.
    ///
    UINT32  MaximumEfficiencyRatio:8;
    UINT32  Reserved6:16;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_PLATFORM_INFO_REGISTER;


/**
  Core. C-State Configuration Control (R/W) Note: C-state values are processor
  specific C-state code names, unrelated to MWAIT extension C-state parameters
  or ACPI C-states. `See http://biosbits.org. <http://biosbits.org/>`__.

  @param  ECX  MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL       0x000000E2

/**
  MSR information returned for MSR index #MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] Package C-State Limit (R/W) Specifies the lowest
    /// processor-specific C-state code name (consuming the least power) for
    /// the package. The default is set as factory-configured package Cstate
    /// limit. The following C-state code name encodings are supported: 000b:
    /// C0/C1 (no package C-state support) 001b: C2 010b: C6 (non-retention)
    /// 011b: C6 (retention) 111b: No Package C state limits. All C states
    /// supported by the processor are available.
    ///
    UINT32  C_StateLimit:3;
    UINT32  Reserved1:7;
    ///
    /// [Bit 10] I/O MWAIT Redirection Enable (R/W).
    ///
    UINT32  MWAITRedirectionEnable:1;
    UINT32  Reserved2:4;
    ///
    /// [Bit 15] CFG Lock (R/WO).
    ///
    UINT32  CFGLock:1;
    ///
    /// [Bit 16] Automatic C-State Conversion Enable (R/W) If 1, the processor
    /// will convert HALT or MWAT(C1) to MWAIT(C6).
    ///
    UINT32  AutomaticC_StateConversionEnable:1;
    UINT32  Reserved3:8;
    ///
    /// [Bit 25] C3 State Auto Demotion Enable (R/W).
    ///
    UINT32  C3StateAutoDemotionEnable:1;
    ///
    /// [Bit 26] C1 State Auto Demotion Enable (R/W).
    ///
    UINT32  C1StateAutoDemotionEnable:1;
    ///
    /// [Bit 27] Enable C3 Undemotion (R/W).
    ///
    UINT32  EnableC3Undemotion:1;
    ///
    /// [Bit 28] Enable C1 Undemotion (R/W).
    ///
    UINT32  EnableC1Undemotion:1;
    ///
    /// [Bit 29] Package C State Demotion Enable (R/W).
    ///
    UINT32  CStateDemotionEnable:1;
    ///
    /// [Bit 30] Package C State UnDemotion Enable (R/W).
    ///
    UINT32  CStateUnDemotionEnable:1;
    UINT32  Reserved4:1;
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
} MSR_SKYLAKE_PKG_CST_CONFIG_CONTROL_REGISTER;


/**
  Thread. Global Machine Check Capability (R/O).

  @param  ECX  MSR_SKYLAKE_IA32_MCG_CAP (0x00000179)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_MCG_CAP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_MCG_CAP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_IA32_MCG_CAP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_IA32_MCG_CAP);
  @endcode
**/
#define MSR_SKYLAKE_IA32_MCG_CAP                 0x00000179

/**
  MSR information returned for MSR index #MSR_SKYLAKE_IA32_MCG_CAP
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] Count.
    ///
    UINT32  Count:8;
    ///
    /// [Bit 8] MCG_CTL_P.
    ///
    UINT32  MCG_CTL_P:1;
    ///
    /// [Bit 9] MCG_EXT_P.
    ///
    UINT32  MCG_EXT_P:1;
    ///
    /// [Bit 10] MCP_CMCI_P.
    ///
    UINT32  MCP_CMCI_P:1;
    ///
    /// [Bit 11] MCG_TES_P.
    ///
    UINT32  MCG_TES_P:1;
    UINT32  Reserved1:4;
    ///
    /// [Bits 23:16] MCG_EXT_CNT.
    ///
    UINT32  MCG_EXT_CNT:8;
    ///
    /// [Bit 24] MCG_SER_P.
    ///
    UINT32  MCG_SER_P:1;
    ///
    /// [Bit 25] MCG_EM_P.
    ///
    UINT32  MCG_EM_P:1;
    ///
    /// [Bit 26] MCG_ELOG_P.
    ///
    UINT32  MCG_ELOG_P:1;
    UINT32  Reserved2:5;
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
} MSR_SKYLAKE_IA32_MCG_CAP_REGISTER;


/**
  THREAD. Enhanced SMM Capabilities (SMM-RO) Reports SMM capability
  Enhancement. Accessible only while in SMM.

  @param  ECX  MSR_SKYLAKE_SMM_MCA_CAP (0x0000017D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_SMM_MCA_CAP_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_SMM_MCA_CAP_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_SMM_MCA_CAP_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_SMM_MCA_CAP);
  AsmWriteMsr64 (MSR_SKYLAKE_SMM_MCA_CAP, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_SMM_MCA_CAP                  0x0000017D

/**
  MSR information returned for MSR index #MSR_SKYLAKE_SMM_MCA_CAP
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
    /// SMM code access restriction is supported and a host-space interface is
    /// available to SMM handler.
    ///
    UINT32  SMM_Code_Access_Chk:1;
    ///
    /// [Bit 59] Long_Flow_Indication (SMM-RO) If set to 1 indicates that the
    /// SMM long flow indicator is supported and a host-space interface is
    /// available to SMM handler.
    ///
    UINT32  Long_Flow_Indication:1;
    UINT32  Reserved3:4;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_SMM_MCA_CAP_REGISTER;


/**
  Package. Temperature Target.

  @param  ECX  MSR_SKYLAKE_TEMPERATURE_TARGET (0x000001A2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_TEMPERATURE_TARGET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_TEMPERATURE_TARGET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_TEMPERATURE_TARGET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_TEMPERATURE_TARGET);
  AsmWriteMsr64 (MSR_SKYLAKE_TEMPERATURE_TARGET, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_TEMPERATURE_TARGET           0x000001A2

/**
  MSR information returned for MSR index #MSR_SKYLAKE_TEMPERATURE_TARGET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32  Reserved1:16;
    ///
    /// [Bits 23:16] Temperature Target (RO) See Table 2-25.
    ///
    UINT32  TemperatureTarget:8;
    ///
    /// [Bits 27:24] TCC Activation Offset (R/W) See Table 2-25.
    ///
    UINT32  TCCActivationOffset:4;
    UINT32  Reserved2:4;
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
} MSR_SKYLAKE_TEMPERATURE_TARGET_REGISTER;

/**
  Package. This register defines the active core ranges for each frequency
  point. NUMCORE[0:7] must be populated in ascending order. NUMCORE[i+1] must
  be greater than NUMCORE[i]. Entries with NUMCORE[i] == 0 will be ignored.
  The last valid entry must have NUMCORE >= the number of cores in the SKU. If
  any of the rules above are broken, the configuration is silently rejected.

  @param  ECX  MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES (0x000001AE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES);
  AsmWriteMsr64 (MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES      0x000001AE

/**
  MSR information returned for MSR index #MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] NUMCORE_0 Defines the active core ranges for each frequency
    /// point.
    ///
    UINT32  NUMCORE_0:8;
    ///
    /// [Bits 15:8] NUMCORE_1 Defines the active core ranges for each
    /// frequency point.
    ///
    UINT32  NUMCORE_1:8;
    ///
    /// [Bits 23:16] NUMCORE_2 Defines the active core ranges for each
    /// frequency point.
    ///
    UINT32  NUMCORE_2:8;
    ///
    /// [Bits 31:24] NUMCORE_3 Defines the active core ranges for each
    /// frequency point.
    ///
    UINT32  NUMCORE_3:8;
    ///
    /// [Bits 39:32] NUMCORE_4 Defines the active core ranges for each
    /// frequency point.
    ///
    UINT32  NUMCORE_4:8;
    ///
    /// [Bits 47:40] NUMCORE_5 Defines the active core ranges for each
    /// frequency point.
    ///
    UINT32  NUMCORE_5:8;
    ///
    /// [Bits 55:48] NUMCORE_6 Defines the active core ranges for each
    /// frequency point.
    ///
    UINT32  NUMCORE_6:8;
    ///
    /// [Bits 63:56] NUMCORE_7 Defines the active core ranges for each
    /// frequency point.
    ///
    UINT32  NUMCORE_7:8;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_TURBO_RATIO_LIMIT_CORES_REGISTER;


/**
  Package. Unit Multipliers Used in RAPL Interfaces (R/O).

  @param  ECX  MSR_SKYLAKE_RAPL_POWER_UNIT (0x00000606)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_RAPL_POWER_UNIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_RAPL_POWER_UNIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_RAPL_POWER_UNIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_RAPL_POWER_UNIT);
  @endcode
**/
#define MSR_SKYLAKE_RAPL_POWER_UNIT              0x00000606

/**
  MSR information returned for MSR index #MSR_SKYLAKE_RAPL_POWER_UNIT
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
} MSR_SKYLAKE_RAPL_POWER_UNIT_REGISTER;


/**
  Package. DRAM RAPL Power Limit Control (R/W) See Section 14.9.5, "DRAM RAPL
  Domain.".

  @param  ECX  MSR_SKYLAKE_DRAM_POWER_LIMIT (0x00000618)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_DRAM_POWER_LIMIT);
  AsmWriteMsr64 (MSR_SKYLAKE_DRAM_POWER_LIMIT, Msr);
  @endcode
**/
#define MSR_SKYLAKE_DRAM_POWER_LIMIT             0x00000618


/**
  Package. DRAM Energy Status (R/O) Energy consumed by DRAM devices.

  @param  ECX  MSR_SKYLAKE_DRAM_ENERGY_STATUS (0x00000619)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_DRAM_ENERGY_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_DRAM_ENERGY_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_DRAM_ENERGY_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_DRAM_ENERGY_STATUS);
  @endcode
**/
#define MSR_SKYLAKE_DRAM_ENERGY_STATUS           0x00000619

/**
  MSR information returned for MSR index #MSR_SKYLAKE_DRAM_ENERGY_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] Energy in 15.3 micro-joules. Requires BIOS configuration
    /// to enable DRAM RAPL mode 0 (Direct VR).
    ///
    UINT32  Energy:32;
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
} MSR_SKYLAKE_DRAM_ENERGY_STATUS_REGISTER;


/**
  Package. DRAM Performance Throttling Status (R/O) See Section 14.9.5, "DRAM
  RAPL Domain.".

  @param  ECX  MSR_SKYLAKE_DRAM_PERF_STATUS (0x0000061B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_DRAM_PERF_STATUS);
  @endcode
**/
#define MSR_SKYLAKE_DRAM_PERF_STATUS             0x0000061B


/**
  Package. DRAM RAPL Parameters (R/W) See Section 14.9.5, "DRAM RAPL Domain.".

  @param  ECX  MSR_SKYLAKE_DRAM_POWER_INFO (0x0000061C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_DRAM_POWER_INFO);
  AsmWriteMsr64 (MSR_SKYLAKE_DRAM_POWER_INFO, Msr);
  @endcode
**/
#define MSR_SKYLAKE_DRAM_POWER_INFO              0x0000061C


/**
  Package. Uncore Ratio Limit (R/W) Out of reset, the min_ratio and max_ratio
  fields represent the widest possible range of uncore frequencies. Writing to
  these fields allows software to control the minimum and the maximum
  frequency that hardware will select.

  @param  ECX  MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT (0x00000620)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT);
  AsmWriteMsr64 (MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT        0x00000620

/**
  MSR information returned for MSR index #MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 6:0] MAX_RATIO This field is used to limit the max ratio of the
    /// LLC/Ring.
    ///
    UINT32  MAX_RATIO:7;
    UINT32  Reserved1:1;
    ///
    /// [Bits 14:8] MIN_RATIO Writing to this field controls the minimum
    /// possible ratio of the LLC/Ring.
    ///
    UINT32  MIN_RATIO:7;
    UINT32  Reserved2:17;
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
} MSR_SKYLAKE_MSRUNCORE_RATIO_LIMIT_REGISTER;


/**
  Package. Reserved (R/O) Reads return 0.

  @param  ECX  MSR_SKYLAKE_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_SKYLAKE_PP0_ENERGY_STATUS);
  @endcode
**/
#define MSR_SKYLAKE_PP0_ENERGY_STATUS            0x00000639


/**
  THREAD. Monitoring Event Select Register (R/W) If CPUID.(EAX=07H,
  ECX=0):EBX.RDT-M[bit 12] = 1.

  @param  ECX  MSR_SKYLAKE_IA32_QM_EVTSEL (0x00000C8D)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_QM_EVTSEL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_QM_EVTSEL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_IA32_QM_EVTSEL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_IA32_QM_EVTSEL);
  AsmWriteMsr64 (MSR_SKYLAKE_IA32_QM_EVTSEL, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_IA32_QM_EVTSEL               0x00000C8D

/**
  MSR information returned for MSR index #MSR_SKYLAKE_IA32_QM_EVTSEL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 7:0] EventID (RW) Event encoding: 0x00: No monitoring. 0x01: L3
    /// occupancy monitoring. 0x02: Total memory bandwidth monitoring. 0x03:
    /// Local memory bandwidth monitoring. All other encoding reserved.
    ///
    UINT32  EventID:8;
    UINT32  Reserved1:24;
    ///
    /// [Bits 41:32] RMID (RW).
    ///
    UINT32  RMID:10;
    UINT32  Reserved2:22;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_IA32_QM_EVTSEL_REGISTER;


/**
  THREAD. Resource Association Register (R/W).

  @param  ECX  MSR_SKYLAKE_IA32_PQR_ASSOC (0x00000C8F)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_PQR_ASSOC_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_PQR_ASSOC_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_IA32_PQR_ASSOC_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_IA32_PQR_ASSOC);
  AsmWriteMsr64 (MSR_SKYLAKE_IA32_PQR_ASSOC, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_IA32_PQR_ASSOC               0x00000C8F

/**
  MSR information returned for MSR index #MSR_SKYLAKE_IA32_PQR_ASSOC
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 9:0] RMID.
    ///
    UINT32  RMID:10;
    UINT32  Reserved1:22;
    ///
    /// [Bits 51:32] COS (R/W).
    ///
    UINT32  COS:20;
    UINT32  Reserved2:12;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_SKYLAKE_IA32_PQR_ASSOC_REGISTER;


/**
  Package. L3 Class Of Service Mask - COS N (R/W) If CPUID.(EAX=10H,
  ECX=1):EDX.COS_MAX[15:0] >=0.

  @param  ECX  MSR_SKYLAKE_IA32_L3_QOS_MASK_N
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_L3_QOS_MASK_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_SKYLAKE_IA32_L3_QOS_MASK_REGISTER.

  <b>Example usage</b>
  @code
  MSR_SKYLAKE_IA32_L3_QOS_MASK_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_SKYLAKE_IA32_L3_QOS_MASK_N);
  AsmWriteMsr64 (MSR_SKYLAKE_IA32_L3_QOS_MASK_N, Msr.Uint64);
  @endcode
**/
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_0           0x00000C90
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_1           0x00000C91
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_2           0x00000C92
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_3           0x00000C93
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_4           0x00000C94
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_5           0x00000C95
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_6           0x00000C96
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_7           0x00000C97
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_8           0x00000C98
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_9           0x00000C99
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_10          0x00000C9A
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_11          0x00000C9B
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_12          0x00000C9C
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_13          0x00000C9D
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_14          0x00000C9E
#define MSR_SKYLAKE_IA32_L3_QOS_MASK_15          0x00000C9F

/**
  MSR information returned for MSR index #MSR_SKYLAKE_IA32_L3_QOS_MASK_N
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 19:0] CBM: Bit vector of available L3 ways for COS N enforcement.
    ///
    UINT32  CBM:20;
    UINT32  Reserved2:12;
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
} MSR_SKYLAKE_IA32_L3_QOS_MASK_REGISTER;


#endif
