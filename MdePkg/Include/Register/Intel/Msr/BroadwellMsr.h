/** @file
  MSR Definitions for Intel processors based on the Broadwell microarchitecture.

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

#ifndef __BROADWELL_MSR_H__
#define __BROADWELL_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel processors based on the Broadwell microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_BROADWELL_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x3D || \
    DisplayModel == 0x47 || \
    DisplayModel == 0x4F || \
    DisplayModel == 0x56    \
    )                       \
   )

/**
  Thread. See Table 2-2. See Section 18.6.2.2, "Global Counter Control
  Facilities.".

  @param  ECX  MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS (0x0000038E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS, Msr.Uint64);
  @endcode
  @note MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS is defined as IA32_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS    0x0000038E

/**
  MSR information returned for MSR index #MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Ovf_PMC0.
    ///
    UINT32  Ovf_PMC0:1;
    ///
    /// [Bit 1] Ovf_PMC1.
    ///
    UINT32  Ovf_PMC1:1;
    ///
    /// [Bit 2] Ovf_PMC2.
    ///
    UINT32  Ovf_PMC2:1;
    ///
    /// [Bit 3] Ovf_PMC3.
    ///
    UINT32  Ovf_PMC3:1;
    UINT32  Reserved1:28;
    ///
    /// [Bit 32] Ovf_FixedCtr0.
    ///
    UINT32  Ovf_FixedCtr0:1;
    ///
    /// [Bit 33] Ovf_FixedCtr1.
    ///
    UINT32  Ovf_FixedCtr1:1;
    ///
    /// [Bit 34] Ovf_FixedCtr2.
    ///
    UINT32  Ovf_FixedCtr2:1;
    UINT32  Reserved2:20;
    ///
    /// [Bit 55] Trace_ToPA_PMI. See Section 36.2.6.2, "Table of Physical
    /// Addresses (ToPA).".
    ///
    UINT32  Trace_ToPA_PMI:1;
    UINT32  Reserved3:5;
    ///
    /// [Bit 61] Ovf_Uncore.
    ///
    UINT32  Ovf_Uncore:1;
    ///
    /// [Bit 62] Ovf_BufDSSAVE.
    ///
    UINT32  OvfBuf:1;
    ///
    /// [Bit 63] CondChgd.
    ///
    UINT32  CondChgd:1;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_BROADWELL_IA32_PERF_GLOBAL_STATUS_REGISTER;


/**
  Core. C-State Configuration Control (R/W) Note: C-state values are processor
  specific C-state code names, unrelated to MWAIT extension C-state parameters
  or ACPI C-states. `See http://biosbits.org. <http://biosbits.org>`__.

  @param  ECX  MSR_BROADWELL_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_BROADWELL_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_BROADWELL_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_BROADWELL_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_BROADWELL_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_BROADWELL_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_BROADWELL_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_BROADWELL_PKG_CST_CONFIG_CONTROL     0x000000E2

/**
  MSR information returned for MSR index #MSR_BROADWELL_PKG_CST_CONFIG_CONTROL
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
    /// 0100b: C7 0101b: C7s 0110b: C8 0111b: C9 1000b: C10.
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
    ///
    /// [Bit 29] Enable Package C-State Auto-demotion (R/W).
    ///
    UINT32  CStateAutoDemotion:1;
    ///
    /// [Bit 30] Enable Package C-State Undemotion (R/W).
    ///
    UINT32  CStateUndemotion:1;
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
} MSR_BROADWELL_PKG_CST_CONFIG_CONTROL_REGISTER;


/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_BROADWELL_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_BROADWELL_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_BROADWELL_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_BROADWELL_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_BROADWELL_TURBO_RATIO_LIMIT);
  @endcode
  @note MSR_BROADWELL_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_BROADWELL_TURBO_RATIO_LIMIT          0x000001AD

/**
  MSR information returned for MSR index #MSR_BROADWELL_TURBO_RATIO_LIMIT
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
    ///
    /// [Bits 39:32] Package. Maximum Ratio Limit for 5C Maximum turbo ratio
    /// limit of 5core active.
    ///
    UINT32  Maximum5C:8;
    ///
    /// [Bits 47:40] Package. Maximum Ratio Limit for 6C Maximum turbo ratio
    /// limit of 6core active.
    ///
    UINT32  Maximum6C:8;
    UINT32  Reserved:16;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64  Uint64;
} MSR_BROADWELL_TURBO_RATIO_LIMIT_REGISTER;


/**
  Package. Uncore Ratio Limit (R/W) Out of reset, the min_ratio and max_ratio
  fields represent the widest possible range of uncore frequencies. Writing to
  these fields allows software to control the minimum and the maximum
  frequency that hardware will select.

  @param  ECX  MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT (0x00000620)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT);
  AsmWriteMsr64 (MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT, Msr.Uint64);
  @endcode
**/
#define MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT      0x00000620

/**
  MSR information returned for MSR index #MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT
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
    UINT32  Reserved2:1;
    ///
    /// [Bits 14:8] MIN_RATIO Writing to this field controls the minimum
    /// possible ratio of the LLC/Ring.
    ///
    UINT32  MIN_RATIO:7;
    UINT32  Reserved3:17;
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
} MSR_BROADWELL_MSRUNCORE_RATIO_LIMIT_REGISTER;

/**
  Package. PP0 Energy Status (R/O)  See Section 14.9.4, "PP0/PP1 RAPL
  Domains.".

  @param  ECX  MSR_BROADWELL_PP0_ENERGY_STATUS (0x00000639)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_BROADWELL_PP0_ENERGY_STATUS);
  @endcode
  @note MSR_BROADWELL_PP0_ENERGY_STATUS is defined as MSR_PP0_ENERGY_STATUS in SDM.
**/
#define MSR_BROADWELL_PP0_ENERGY_STATUS          0x00000639

#endif
