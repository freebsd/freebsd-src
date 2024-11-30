/** @file
  MSR Definitions for Intel processors based on the Nehalem microarchitecture.

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

#ifndef __NEHALEM_MSR_H__
#define __NEHALEM_MSR_H__

#include <Register/Intel/ArchitecturalMsr.h>

/**
  Is Intel processors based on the Nehalem microarchitecture?

  @param   DisplayFamily  Display Family ID
  @param   DisplayModel   Display Model ID

  @retval  TRUE   Yes, it is.
  @retval  FALSE  No, it isn't.
**/
#define IS_NEHALEM_PROCESSOR(DisplayFamily, DisplayModel) \
  (DisplayFamily == 0x06 && \
   (                        \
    DisplayModel == 0x1A || \
    DisplayModel == 0x1E || \
    DisplayModel == 0x1F || \
    DisplayModel == 0x2E    \
    )                       \
   )

/**
  Package. Model Specific Platform ID (R).

  @param  ECX  MSR_NEHALEM_PLATFORM_ID (0x00000017)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PLATFORM_ID_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PLATFORM_ID_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_PLATFORM_ID_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PLATFORM_ID);
  @endcode
  @note MSR_NEHALEM_PLATFORM_ID is defined as MSR_PLATFORM_ID in SDM.
**/
#define MSR_NEHALEM_PLATFORM_ID  0x00000017

/**
  MSR information returned for MSR index #MSR_NEHALEM_PLATFORM_ID
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1  : 32;
    UINT32    Reserved2  : 18;
    ///
    /// [Bits 52:50] See Table 2-2.
    ///
    UINT32    PlatformId : 3;
    UINT32    Reserved3  : 11;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_PLATFORM_ID_REGISTER;

/**
  Thread. SMI Counter (R/O).

  @param  ECX  MSR_NEHALEM_SMI_COUNT (0x00000034)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_SMI_COUNT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_SMI_COUNT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_SMI_COUNT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_SMI_COUNT);
  @endcode
  @note MSR_NEHALEM_SMI_COUNT is defined as MSR_SMI_COUNT in SDM.
**/
#define MSR_NEHALEM_SMI_COUNT  0x00000034

/**
  MSR information returned for MSR index #MSR_NEHALEM_SMI_COUNT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 31:0] SMI Count (R/O)  Running count of SMI events since last
    /// RESET.
    ///
    UINT32    SMICount : 32;
    UINT32    Reserved : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_SMI_COUNT_REGISTER;

/**
  Package. see http://biosbits.org.

  @param  ECX  MSR_NEHALEM_PLATFORM_INFO (0x000000CE)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PLATFORM_INFO_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PLATFORM_INFO_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_PLATFORM_INFO_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PLATFORM_INFO);
  AsmWriteMsr64 (MSR_NEHALEM_PLATFORM_INFO, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_PLATFORM_INFO is defined as MSR_PLATFORM_INFO in SDM.
**/
#define MSR_NEHALEM_PLATFORM_INFO  0x000000CE

/**
  MSR information returned for MSR index #MSR_NEHALEM_PLATFORM_INFO
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1              : 8;
    ///
    /// [Bits 15:8] Package. Maximum Non-Turbo Ratio (R/O)  The is the ratio
    /// of the frequency that invariant TSC runs at. The invariant TSC
    /// frequency can be computed by multiplying this ratio by 133.33 MHz.
    ///
    UINT32    MaximumNonTurboRatio   : 8;
    UINT32    Reserved2              : 12;
    ///
    /// [Bit 28] Package. Programmable Ratio Limit for Turbo Mode (R/O)  When
    /// set to 1, indicates that Programmable Ratio Limits for Turbo mode is
    /// enabled, and when set to 0, indicates Programmable Ratio Limits for
    /// Turbo mode is disabled.
    ///
    UINT32    RatioLimit             : 1;
    ///
    /// [Bit 29] Package. Programmable TDC-TDP Limit for Turbo Mode (R/O)
    /// When set to 1, indicates that TDC/TDP Limits for Turbo mode are
    /// programmable, and when set to 0, indicates TDC and TDP Limits for
    /// Turbo mode are not programmable.
    ///
    UINT32    TDC_TDPLimit           : 1;
    UINT32    Reserved3              : 2;
    UINT32    Reserved4              : 8;
    ///
    /// [Bits 47:40] Package. Maximum Efficiency Ratio (R/O)  The is the
    /// minimum ratio (maximum efficiency) that the processor can operates, in
    /// units of 133.33MHz.
    ///
    UINT32    MaximumEfficiencyRatio : 8;
    UINT32    Reserved5              : 16;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_PLATFORM_INFO_REGISTER;

/**
  Core. C-State Configuration Control (R/W)  Note: C-state values are
  processor specific C-state code names, unrelated to MWAIT extension C-state
  parameters or ACPI CStates. See http://biosbits.org.

  @param  ECX  MSR_NEHALEM_PKG_CST_CONFIG_CONTROL (0x000000E2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PKG_CST_CONFIG_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PKG_CST_CONFIG_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_PKG_CST_CONFIG_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PKG_CST_CONFIG_CONTROL);
  AsmWriteMsr64 (MSR_NEHALEM_PKG_CST_CONFIG_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_PKG_CST_CONFIG_CONTROL is defined as MSR_PKG_CST_CONFIG_CONTROL in SDM.
**/
#define MSR_NEHALEM_PKG_CST_CONFIG_CONTROL  0x000000E2

/**
  MSR information returned for MSR index #MSR_NEHALEM_PKG_CST_CONFIG_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 2:0] Package C-State Limit (R/W)  Specifies the lowest
    /// processor-specific C-state code name (consuming the least power). for
    /// the package. The default is set as factory-configured package C-state
    /// limit. The following C-state code name encodings are supported: 000b:
    /// C0 (no package C-sate support) 001b: C1 (Behavior is the same as 000b)
    /// 010b: C3 011b: C6 100b: C7 101b and 110b: Reserved 111: No package
    /// C-state limit. Note: This field cannot be used to limit package
    /// C-state to C3.
    ///
    UINT32    Limit              : 3;
    UINT32    Reserved1          : 7;
    ///
    /// [Bit 10] I/O MWAIT Redirection Enable (R/W)  When set, will map
    /// IO_read instructions sent to IO register specified by
    /// MSR_PMG_IO_CAPTURE_BASE to MWAIT instructions.
    ///
    UINT32    IO_MWAIT           : 1;
    UINT32    Reserved2          : 4;
    ///
    /// [Bit 15] CFG Lock (R/WO)  When set, lock bits 15:0 of this register
    /// until next reset.
    ///
    UINT32    CFGLock            : 1;
    UINT32    Reserved3          : 8;
    ///
    /// [Bit 24] Interrupt filtering enable (R/W)  When set, processor cores
    /// in a deep C-State will wake only when the event message is destined
    /// for that core. When 0, all processor cores in a deep C-State will wake
    /// for an event message.
    ///
    UINT32    InterruptFiltering : 1;
    ///
    /// [Bit 25] C3 state auto demotion enable (R/W)  When set, the processor
    /// will conditionally demote C6/C7 requests to C3 based on uncore
    /// auto-demote information.
    ///
    UINT32    C3AutoDemotion     : 1;
    ///
    /// [Bit 26] C1 state auto demotion enable (R/W)  When set, the processor
    /// will conditionally demote C3/C6/C7 requests to C1 based on uncore
    /// auto-demote information.
    ///
    UINT32    C1AutoDemotion     : 1;
    ///
    /// [Bit 27] Enable C3 Undemotion (R/W).
    ///
    UINT32    C3Undemotion       : 1;
    ///
    /// [Bit 28] Enable C1 Undemotion (R/W).
    ///
    UINT32    C1Undemotion       : 1;
    ///
    /// [Bit 29] Package C State Demotion Enable (R/W).
    ///
    UINT32    CStateDemotion     : 1;
    ///
    /// [Bit 30] Package C State UnDemotion Enable (R/W).
    ///
    UINT32    CStateUndemotion   : 1;
    UINT32    Reserved4          : 1;
    UINT32    Reserved5          : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_PKG_CST_CONFIG_CONTROL_REGISTER;

/**
  Core. Power Management IO Redirection in C-state (R/W) See
  http://biosbits.org.

  @param  ECX  MSR_NEHALEM_PMG_IO_CAPTURE_BASE (0x000000E4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PMG_IO_CAPTURE_BASE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PMG_IO_CAPTURE_BASE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_PMG_IO_CAPTURE_BASE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PMG_IO_CAPTURE_BASE);
  AsmWriteMsr64 (MSR_NEHALEM_PMG_IO_CAPTURE_BASE, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_PMG_IO_CAPTURE_BASE is defined as MSR_PMG_IO_CAPTURE_BASE in SDM.
**/
#define MSR_NEHALEM_PMG_IO_CAPTURE_BASE  0x000000E4

/**
  MSR information returned for MSR index #MSR_NEHALEM_PMG_IO_CAPTURE_BASE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 15:0] LVL_2 Base Address (R/W)  Specifies the base address
    /// visible to software for IO redirection. If IO MWAIT Redirection is
    /// enabled, reads to this address will be consumed by the power
    /// management logic and decoded to MWAIT instructions. When IO port
    /// address redirection is enabled, this is the IO port address reported
    /// to the OS/software.
    ///
    UINT32    Lvl2Base    : 16;
    ///
    /// [Bits 18:16] C-state Range (R/W)  Specifies the encoding value of the
    /// maximum C-State code name to be included when IO read to MWAIT
    /// redirection is enabled by MSR_PKG_CST_CONFIG_CONTROL[bit10]: 000b - C3
    /// is the max C-State to include 001b - C6 is the max C-State to include
    /// 010b - C7 is the max C-State to include.
    ///
    UINT32    CStateRange : 3;
    UINT32    Reserved1   : 13;
    UINT32    Reserved2   : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_PMG_IO_CAPTURE_BASE_REGISTER;

/**
  Enable Misc. Processor Features (R/W)  Allows a variety of processor
  functions to be enabled and disabled.

  @param  ECX  MSR_NEHALEM_IA32_MISC_ENABLE (0x000001A0)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_IA32_MISC_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_IA32_MISC_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_IA32_MISC_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_IA32_MISC_ENABLE);
  AsmWriteMsr64 (MSR_NEHALEM_IA32_MISC_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_IA32_MISC_ENABLE is defined as IA32_MISC_ENABLE in SDM.
**/
#define MSR_NEHALEM_IA32_MISC_ENABLE  0x000001A0

/**
  MSR information returned for MSR index #MSR_NEHALEM_IA32_MISC_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Thread. Fast-Strings Enable See Table 2-2.
    ///
    UINT32    FastStrings                    : 1;
    UINT32    Reserved1                      : 2;
    ///
    /// [Bit 3] Thread. Automatic Thermal Control Circuit Enable (R/W) See
    /// Table 2-2. Default value is 1.
    ///
    UINT32    AutomaticThermalControlCircuit : 1;
    UINT32    Reserved2                      : 3;
    ///
    /// [Bit 7] Thread. Performance Monitoring Available (R) See Table 2-2.
    ///
    UINT32    PerformanceMonitoring          : 1;
    UINT32    Reserved3                      : 3;
    ///
    /// [Bit 11] Thread. Branch Trace Storage Unavailable (RO) See Table 2-2.
    ///
    UINT32    BTS                            : 1;
    ///
    /// [Bit 12] Thread. Processor Event Based Sampling Unavailable (RO) See
    /// Table 2-2.
    ///
    UINT32    PEBS                           : 1;
    UINT32    Reserved4                      : 3;
    ///
    /// [Bit 16] Package. Enhanced Intel SpeedStep Technology Enable (R/W) See
    /// Table 2-2.
    ///
    UINT32    EIST                           : 1;
    UINT32    Reserved5                      : 1;
    ///
    /// [Bit 18] Thread. ENABLE MONITOR FSM. (R/W) See Table 2-2.
    ///
    UINT32    MONITOR                        : 1;
    UINT32    Reserved6                      : 3;
    ///
    /// [Bit 22] Thread. Limit CPUID Maxval (R/W) See Table 2-2.
    ///
    UINT32    LimitCpuidMaxval               : 1;
    ///
    /// [Bit 23] Thread. xTPR Message Disable (R/W) See Table 2-2.
    ///
    UINT32    xTPR_Message_Disable           : 1;
    UINT32    Reserved7                      : 8;
    UINT32    Reserved8                      : 2;
    ///
    /// [Bit 34] Thread. XD Bit Disable (R/W) See Table 2-2.
    ///
    UINT32    XD                             : 1;
    UINT32    Reserved9                      : 3;
    ///
    /// [Bit 38] Package. Turbo Mode Disable (R/W) When set to 1 on processors
    /// that support Intel Turbo Boost Technology, the turbo mode feature is
    /// disabled and the IDA_Enable feature flag will be clear (CPUID.06H:
    /// EAX[1]=0). When set to a 0 on processors that support IDA, CPUID.06H:
    /// EAX[1] reports the processor's support of turbo mode is enabled. Note:
    /// the power-on default value is used by BIOS to detect hardware support
    /// of turbo mode. If power-on default value is 1, turbo mode is available
    /// in the processor. If power-on default value is 0, turbo mode is not
    /// available.
    ///
    UINT32    TurboModeDisable : 1;
    UINT32    Reserved10       : 25;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_IA32_MISC_ENABLE_REGISTER;

/**
  Thread.

  @param  ECX  MSR_NEHALEM_TEMPERATURE_TARGET (0x000001A2)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_TEMPERATURE_TARGET_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_TEMPERATURE_TARGET_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_TEMPERATURE_TARGET_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_TEMPERATURE_TARGET);
  AsmWriteMsr64 (MSR_NEHALEM_TEMPERATURE_TARGET, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_TEMPERATURE_TARGET is defined as MSR_TEMPERATURE_TARGET in SDM.
**/
#define MSR_NEHALEM_TEMPERATURE_TARGET  0x000001A2

/**
  MSR information returned for MSR index #MSR_NEHALEM_TEMPERATURE_TARGET
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1         : 16;
    ///
    /// [Bits 23:16] Temperature Target (R)  The minimum temperature at which
    /// PROCHOT# will be asserted. The value is degree C.
    ///
    UINT32    TemperatureTarget : 8;
    UINT32    Reserved2         : 8;
    UINT32    Reserved3         : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_TEMPERATURE_TARGET_REGISTER;

/**
  Miscellaneous Feature Control (R/W).

  @param  ECX  MSR_NEHALEM_MISC_FEATURE_CONTROL (0x000001A4)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_MISC_FEATURE_CONTROL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_MISC_FEATURE_CONTROL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_MISC_FEATURE_CONTROL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_MISC_FEATURE_CONTROL);
  AsmWriteMsr64 (MSR_NEHALEM_MISC_FEATURE_CONTROL, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_MISC_FEATURE_CONTROL is defined as MSR_MISC_FEATURE_CONTROL in SDM.
**/
#define MSR_NEHALEM_MISC_FEATURE_CONTROL  0x000001A4

/**
  MSR information returned for MSR index #MSR_NEHALEM_MISC_FEATURE_CONTROL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Core. L2 Hardware Prefetcher Disable (R/W)  If 1, disables the
    /// L2 hardware prefetcher, which fetches additional lines of code or data
    /// into the L2 cache.
    ///
    UINT32    L2HardwarePrefetcherDisable          : 1;
    ///
    /// [Bit 1] Core. L2 Adjacent Cache Line Prefetcher Disable (R/W)  If 1,
    /// disables the adjacent cache line prefetcher, which fetches the cache
    /// line that comprises a cache line pair (128 bytes).
    ///
    UINT32    L2AdjacentCacheLinePrefetcherDisable : 1;
    ///
    /// [Bit 2] Core. DCU Hardware Prefetcher Disable (R/W)  If 1, disables
    /// the L1 data cache prefetcher, which fetches the next cache line into
    /// L1 data cache.
    ///
    UINT32    DCUHardwarePrefetcherDisable         : 1;
    ///
    /// [Bit 3] Core. DCU IP Prefetcher Disable (R/W)  If 1, disables the L1
    /// data cache IP prefetcher, which uses sequential load history (based on
    /// instruction Pointer of previous loads) to determine whether to
    /// prefetch additional lines.
    ///
    UINT32    DCUIPPrefetcherDisable               : 1;
    UINT32    Reserved1                            : 28;
    UINT32    Reserved2                            : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_MISC_FEATURE_CONTROL_REGISTER;

/**
  Thread. Offcore Response Event Select Register (R/W).

  @param  ECX  MSR_NEHALEM_OFFCORE_RSP_0 (0x000001A6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_OFFCORE_RSP_0);
  AsmWriteMsr64 (MSR_NEHALEM_OFFCORE_RSP_0, Msr);
  @endcode
  @note MSR_NEHALEM_OFFCORE_RSP_0 is defined as MSR_OFFCORE_RSP_0 in SDM.
**/
#define MSR_NEHALEM_OFFCORE_RSP_0  0x000001A6

/**
  See http://biosbits.org.

  @param  ECX  MSR_NEHALEM_MISC_PWR_MGMT (0x000001AA)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_MISC_PWR_MGMT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_MISC_PWR_MGMT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_MISC_PWR_MGMT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_MISC_PWR_MGMT);
  AsmWriteMsr64 (MSR_NEHALEM_MISC_PWR_MGMT, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_MISC_PWR_MGMT is defined as MSR_MISC_PWR_MGMT in SDM.
**/
#define MSR_NEHALEM_MISC_PWR_MGMT  0x000001AA

/**
  MSR information returned for MSR index #MSR_NEHALEM_MISC_PWR_MGMT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Package. EIST Hardware Coordination Disable (R/W) When 0,
    /// enables hardware coordination of Enhanced Intel Speedstep Technology
    /// request from processor cores; When 1, disables hardware coordination
    /// of Enhanced Intel Speedstep Technology requests.
    ///
    UINT32    EISTHardwareCoordinationDisable : 1;
    ///
    /// [Bit 1] Thread. Energy/Performance Bias Enable (R/W)  This bit makes
    /// the IA32_ENERGY_PERF_BIAS register (MSR 1B0h) visible to software with
    /// Ring 0 privileges. This bit's status (1 or 0) is also reflected by
    /// CPUID.(EAX=06h):ECX[3].
    ///
    UINT32    EnergyPerformanceBiasEnable     : 1;
    UINT32    Reserved1                       : 30;
    UINT32    Reserved2                       : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_MISC_PWR_MGMT_REGISTER;

/**
  See http://biosbits.org.

  @param  ECX  MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT (0x000001AC)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT);
  AsmWriteMsr64 (MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT is defined as MSR_TURBO_POWER_CURRENT_LIMIT in SDM.
**/
#define MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT  0x000001AC

/**
  MSR information returned for MSR index #MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 14:0] Package. TDP Limit (R/W)  TDP limit in 1/8 Watt
    /// granularity.
    ///
    UINT32    TDPLimit               : 15;
    ///
    /// [Bit 15] Package. TDP Limit Override Enable (R/W)  A value = 0
    /// indicates override is not active, and a value = 1 indicates active.
    ///
    UINT32    TDPLimitOverrideEnable : 1;
    ///
    /// [Bits 30:16] Package. TDC Limit (R/W)  TDC limit in 1/8 Amp
    /// granularity.
    ///
    UINT32    TDCLimit               : 15;
    ///
    /// [Bit 31] Package. TDC Limit Override Enable (R/W)  A value = 0
    /// indicates override is not active, and a value = 1 indicates active.
    ///
    UINT32    TDCLimitOverrideEnable : 1;
    UINT32    Reserved               : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_TURBO_POWER_CURRENT_LIMIT_REGISTER;

/**
  Package. Maximum Ratio Limit of Turbo Mode RO if MSR_PLATFORM_INFO.[28] = 0,
  RW if MSR_PLATFORM_INFO.[28] = 1.

  @param  ECX  MSR_NEHALEM_TURBO_RATIO_LIMIT (0x000001AD)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_TURBO_RATIO_LIMIT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_TURBO_RATIO_LIMIT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_TURBO_RATIO_LIMIT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_TURBO_RATIO_LIMIT);
  @endcode
  @note MSR_NEHALEM_TURBO_RATIO_LIMIT is defined as MSR_TURBO_RATIO_LIMIT in SDM.
**/
#define MSR_NEHALEM_TURBO_RATIO_LIMIT  0x000001AD

/**
  MSR information returned for MSR index #MSR_NEHALEM_TURBO_RATIO_LIMIT
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
    UINT32    Maximum1C : 8;
    ///
    /// [Bits 15:8] Package. Maximum Ratio Limit for 2C Maximum turbo ratio
    /// limit of 2 core active.
    ///
    UINT32    Maximum2C : 8;
    ///
    /// [Bits 23:16] Package. Maximum Ratio Limit for 3C Maximum turbo ratio
    /// limit of 3 core active.
    ///
    UINT32    Maximum3C : 8;
    ///
    /// [Bits 31:24] Package. Maximum Ratio Limit for 4C Maximum turbo ratio
    /// limit of 4 core active.
    ///
    UINT32    Maximum4C : 8;
    UINT32    Reserved  : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_TURBO_RATIO_LIMIT_REGISTER;

/**
  Core. Last Branch Record Filtering Select Register (R/W) See Section 17.9.2,
  "Filtering of Last Branch Records.".

  @param  ECX  MSR_NEHALEM_LBR_SELECT (0x000001C8)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_LBR_SELECT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_LBR_SELECT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_LBR_SELECT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_LBR_SELECT);
  AsmWriteMsr64 (MSR_NEHALEM_LBR_SELECT, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_LBR_SELECT is defined as MSR_LBR_SELECT in SDM.
**/
#define MSR_NEHALEM_LBR_SELECT  0x000001C8

/**
  MSR information returned for MSR index #MSR_NEHALEM_LBR_SELECT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] CPL_EQ_0.
    ///
    UINT32    CPL_EQ_0      : 1;
    ///
    /// [Bit 1] CPL_NEQ_0.
    ///
    UINT32    CPL_NEQ_0     : 1;
    ///
    /// [Bit 2] JCC.
    ///
    UINT32    JCC           : 1;
    ///
    /// [Bit 3] NEAR_REL_CALL.
    ///
    UINT32    NEAR_REL_CALL : 1;
    ///
    /// [Bit 4] NEAR_IND_CALL.
    ///
    UINT32    NEAR_IND_CALL : 1;
    ///
    /// [Bit 5] NEAR_RET.
    ///
    UINT32    NEAR_RET      : 1;
    ///
    /// [Bit 6] NEAR_IND_JMP.
    ///
    UINT32    NEAR_IND_JMP  : 1;
    ///
    /// [Bit 7] NEAR_REL_JMP.
    ///
    UINT32    NEAR_REL_JMP  : 1;
    ///
    /// [Bit 8] FAR_BRANCH.
    ///
    UINT32    FAR_BRANCH    : 1;
    UINT32    Reserved1     : 23;
    UINT32    Reserved2     : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_LBR_SELECT_REGISTER;

/**
  Thread. Last Branch Record Stack TOS (R/W)  Contains an index (bits 0-3)
  that points to the MSR containing the most recent branch record. See
  MSR_LASTBRANCH_0_FROM_IP (at 680H).

  @param  ECX  MSR_NEHALEM_LASTBRANCH_TOS (0x000001C9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_LASTBRANCH_TOS);
  AsmWriteMsr64 (MSR_NEHALEM_LASTBRANCH_TOS, Msr);
  @endcode
  @note MSR_NEHALEM_LASTBRANCH_TOS is defined as MSR_LASTBRANCH_TOS in SDM.
**/
#define MSR_NEHALEM_LASTBRANCH_TOS  0x000001C9

/**
  Thread. Last Exception Record From Linear IP (R)  Contains a pointer to the
  last branch instruction that the processor executed prior to the last
  exception that was generated or the last interrupt that was handled.

  @param  ECX  MSR_NEHALEM_LER_FROM_LIP (0x000001DD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_LER_FROM_LIP);
  @endcode
  @note MSR_NEHALEM_LER_FROM_LIP is defined as MSR_LER_FROM_LIP in SDM.
**/
#define MSR_NEHALEM_LER_FROM_LIP  0x000001DD

/**
  Thread. Last Exception Record To Linear IP (R)  This area contains a pointer
  to the target of the last branch instruction that the processor executed
  prior to the last exception that was generated or the last interrupt that
  was handled.

  @param  ECX  MSR_NEHALEM_LER_TO_LIP (0x000001DE)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_LER_TO_LIP);
  @endcode
  @note MSR_NEHALEM_LER_TO_LIP is defined as MSR_LER_TO_LIP in SDM.
**/
#define MSR_NEHALEM_LER_TO_LIP  0x000001DE

/**
  Core. Power Control Register. See http://biosbits.org.

  @param  ECX  MSR_NEHALEM_POWER_CTL (0x000001FC)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_POWER_CTL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_POWER_CTL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_POWER_CTL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_POWER_CTL);
  AsmWriteMsr64 (MSR_NEHALEM_POWER_CTL, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_POWER_CTL is defined as MSR_POWER_CTL in SDM.
**/
#define MSR_NEHALEM_POWER_CTL  0x000001FC

/**
  MSR information returned for MSR index #MSR_NEHALEM_POWER_CTL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1 : 1;
    ///
    /// [Bit 1] Package. C1E Enable (R/W)  When set to '1', will enable the
    /// CPU to switch to the Minimum Enhanced Intel SpeedStep Technology
    /// operating point when all execution cores enter MWAIT (C1).
    ///
    UINT32    C1EEnable : 1;
    UINT32    Reserved2 : 30;
    UINT32    Reserved3 : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_POWER_CTL_REGISTER;

/**
  Thread. (RO).

  @param  ECX  MSR_NEHALEM_PERF_GLOBAL_STATUS (0x0000038E)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PERF_GLOBAL_STATUS_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PERF_GLOBAL_STATUS_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_PERF_GLOBAL_STATUS_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PERF_GLOBAL_STATUS);
  @endcode
  @note MSR_NEHALEM_PERF_GLOBAL_STATUS is defined as MSR_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_NEHALEM_PERF_GLOBAL_STATUS  0x0000038E

/**
  MSR information returned for MSR index #MSR_NEHALEM_PERF_GLOBAL_STATUS
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1  : 32;
    UINT32    Reserved2  : 29;
    ///
    /// [Bit 61] UNC_Ovf Uncore overflowed if 1.
    ///
    UINT32    Ovf_Uncore : 1;
    UINT32    Reserved3  : 2;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_PERF_GLOBAL_STATUS_REGISTER;

/**
  Thread. (R/W).

  @param  ECX  MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL (0x00000390)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL is defined as MSR_PERF_GLOBAL_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL  0x00000390

/**
  MSR information returned for MSR index #MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    UINT32    Reserved1  : 32;
    UINT32    Reserved2  : 29;
    ///
    /// [Bit 61] CLR_UNC_Ovf Set 1 to clear UNC_Ovf.
    ///
    UINT32    Ovf_Uncore : 1;
    UINT32    Reserved3  : 2;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_PERF_GLOBAL_OVF_CTRL_REGISTER;

/**
  Thread. See Section 18.3.1.1.1, "Processor Event Based Sampling (PEBS).".

  @param  ECX  MSR_NEHALEM_PEBS_ENABLE (0x000003F1)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PEBS_ENABLE_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PEBS_ENABLE_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_PEBS_ENABLE_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PEBS_ENABLE);
  AsmWriteMsr64 (MSR_NEHALEM_PEBS_ENABLE, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_PEBS_ENABLE is defined as MSR_PEBS_ENABLE in SDM.
**/
#define MSR_NEHALEM_PEBS_ENABLE  0x000003F1

/**
  MSR information returned for MSR index #MSR_NEHALEM_PEBS_ENABLE
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] Enable PEBS on IA32_PMC0. (R/W).
    ///
    UINT32    PEBS_EN_PMC0 : 1;
    ///
    /// [Bit 1] Enable PEBS on IA32_PMC1. (R/W).
    ///
    UINT32    PEBS_EN_PMC1 : 1;
    ///
    /// [Bit 2] Enable PEBS on IA32_PMC2. (R/W).
    ///
    UINT32    PEBS_EN_PMC2 : 1;
    ///
    /// [Bit 3] Enable PEBS on IA32_PMC3. (R/W).
    ///
    UINT32    PEBS_EN_PMC3 : 1;
    UINT32    Reserved1    : 28;
    ///
    /// [Bit 32] Enable Load Latency on IA32_PMC0. (R/W).
    ///
    UINT32    LL_EN_PMC0   : 1;
    ///
    /// [Bit 33] Enable Load Latency on IA32_PMC1. (R/W).
    ///
    UINT32    LL_EN_PMC1   : 1;
    ///
    /// [Bit 34] Enable Load Latency on IA32_PMC2. (R/W).
    ///
    UINT32    LL_EN_PMC2   : 1;
    ///
    /// [Bit 35] Enable Load Latency on IA32_PMC3. (R/W).
    ///
    UINT32    LL_EN_PMC3   : 1;
    UINT32    Reserved2    : 28;
  } Bits;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_PEBS_ENABLE_REGISTER;

/**
  Thread. See Section 18.3.1.1.2, "Load Latency Performance Monitoring
  Facility.".

  @param  ECX  MSR_NEHALEM_PEBS_LD_LAT (0x000003F6)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PEBS_LD_LAT_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_PEBS_LD_LAT_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_PEBS_LD_LAT_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_PEBS_LD_LAT);
  AsmWriteMsr64 (MSR_NEHALEM_PEBS_LD_LAT, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_PEBS_LD_LAT is defined as MSR_PEBS_LD_LAT in SDM.
**/
#define MSR_NEHALEM_PEBS_LD_LAT  0x000003F6

/**
  MSR information returned for MSR index #MSR_NEHALEM_PEBS_LD_LAT
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bits 15:0] Minimum threshold latency value of tagged load operation
    /// that will be counted. (R/W).
    ///
    UINT32    MinimumThreshold : 16;
    UINT32    Reserved1        : 16;
    UINT32    Reserved2        : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_PEBS_LD_LAT_REGISTER;

/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C3
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C3 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_NEHALEM_PKG_C3_RESIDENCY (0x000003F8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_PKG_C3_RESIDENCY);
  AsmWriteMsr64 (MSR_NEHALEM_PKG_C3_RESIDENCY, Msr);
  @endcode
  @note MSR_NEHALEM_PKG_C3_RESIDENCY is defined as MSR_PKG_C3_RESIDENCY in SDM.
**/
#define MSR_NEHALEM_PKG_C3_RESIDENCY  0x000003F8

/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C6
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C6 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_NEHALEM_PKG_C6_RESIDENCY (0x000003F9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_PKG_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_NEHALEM_PKG_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_NEHALEM_PKG_C6_RESIDENCY is defined as MSR_PKG_C6_RESIDENCY in SDM.
**/
#define MSR_NEHALEM_PKG_C6_RESIDENCY  0x000003F9

/**
  Package. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. Package C7
  Residency Counter. (R/O) Value since last reset that this package is in
  processor-specific C7 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_NEHALEM_PKG_C7_RESIDENCY (0x000003FA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_PKG_C7_RESIDENCY);
  AsmWriteMsr64 (MSR_NEHALEM_PKG_C7_RESIDENCY, Msr);
  @endcode
  @note MSR_NEHALEM_PKG_C7_RESIDENCY is defined as MSR_PKG_C7_RESIDENCY in SDM.
**/
#define MSR_NEHALEM_PKG_C7_RESIDENCY  0x000003FA

/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. CORE C3
  Residency Counter. (R/O) Value since last reset that this core is in
  processor-specific C3 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_NEHALEM_CORE_C3_RESIDENCY (0x000003FC)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_CORE_C3_RESIDENCY);
  AsmWriteMsr64 (MSR_NEHALEM_CORE_C3_RESIDENCY, Msr);
  @endcode
  @note MSR_NEHALEM_CORE_C3_RESIDENCY is defined as MSR_CORE_C3_RESIDENCY in SDM.
**/
#define MSR_NEHALEM_CORE_C3_RESIDENCY  0x000003FC

/**
  Core. Note: C-state values are processor specific C-state code names,
  unrelated to MWAIT extension C-state parameters or ACPI CStates. CORE C6
  Residency Counter. (R/O) Value since last reset that this core is in
  processor-specific C6 states. Count at the same frequency as the TSC.

  @param  ECX  MSR_NEHALEM_CORE_C6_RESIDENCY (0x000003FD)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_CORE_C6_RESIDENCY);
  AsmWriteMsr64 (MSR_NEHALEM_CORE_C6_RESIDENCY, Msr);
  @endcode
  @note MSR_NEHALEM_CORE_C6_RESIDENCY is defined as MSR_CORE_C6_RESIDENCY in SDM.
**/
#define MSR_NEHALEM_CORE_C6_RESIDENCY  0x000003FD

/**
  Thread. Last Branch Record n From IP (R/W) One of sixteen pairs of last
  branch record registers on the last branch record stack. The From_IP part of
  the stack contains pointers to the source instruction. See also: -  Last
  Branch Record Stack TOS at 1C9H -  Section 17.7.1 and record format in
  Section 17.4.8.1.

  @param  ECX  MSR_NEHALEM_LASTBRANCH_n_FROM_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_LASTBRANCH_0_FROM_IP);
  AsmWriteMsr64 (MSR_NEHALEM_LASTBRANCH_0_FROM_IP, Msr);
  @endcode
  @note MSR_NEHALEM_LASTBRANCH_0_FROM_IP  is defined as MSR_LASTBRANCH_0_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_1_FROM_IP  is defined as MSR_LASTBRANCH_1_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_2_FROM_IP  is defined as MSR_LASTBRANCH_2_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_3_FROM_IP  is defined as MSR_LASTBRANCH_3_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_4_FROM_IP  is defined as MSR_LASTBRANCH_4_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_5_FROM_IP  is defined as MSR_LASTBRANCH_5_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_6_FROM_IP  is defined as MSR_LASTBRANCH_6_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_7_FROM_IP  is defined as MSR_LASTBRANCH_7_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_8_FROM_IP  is defined as MSR_LASTBRANCH_8_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_9_FROM_IP  is defined as MSR_LASTBRANCH_9_FROM_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_10_FROM_IP is defined as MSR_LASTBRANCH_10_FROM_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_11_FROM_IP is defined as MSR_LASTBRANCH_11_FROM_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_12_FROM_IP is defined as MSR_LASTBRANCH_12_FROM_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_13_FROM_IP is defined as MSR_LASTBRANCH_13_FROM_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_14_FROM_IP is defined as MSR_LASTBRANCH_14_FROM_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_15_FROM_IP is defined as MSR_LASTBRANCH_15_FROM_IP in SDM.
  @{
**/
#define MSR_NEHALEM_LASTBRANCH_0_FROM_IP   0x00000680
#define MSR_NEHALEM_LASTBRANCH_1_FROM_IP   0x00000681
#define MSR_NEHALEM_LASTBRANCH_2_FROM_IP   0x00000682
#define MSR_NEHALEM_LASTBRANCH_3_FROM_IP   0x00000683
#define MSR_NEHALEM_LASTBRANCH_4_FROM_IP   0x00000684
#define MSR_NEHALEM_LASTBRANCH_5_FROM_IP   0x00000685
#define MSR_NEHALEM_LASTBRANCH_6_FROM_IP   0x00000686
#define MSR_NEHALEM_LASTBRANCH_7_FROM_IP   0x00000687
#define MSR_NEHALEM_LASTBRANCH_8_FROM_IP   0x00000688
#define MSR_NEHALEM_LASTBRANCH_9_FROM_IP   0x00000689
#define MSR_NEHALEM_LASTBRANCH_10_FROM_IP  0x0000068A
#define MSR_NEHALEM_LASTBRANCH_11_FROM_IP  0x0000068B
#define MSR_NEHALEM_LASTBRANCH_12_FROM_IP  0x0000068C
#define MSR_NEHALEM_LASTBRANCH_13_FROM_IP  0x0000068D
#define MSR_NEHALEM_LASTBRANCH_14_FROM_IP  0x0000068E
#define MSR_NEHALEM_LASTBRANCH_15_FROM_IP  0x0000068F
/// @}

/**
  Thread. Last Branch Record n To IP (R/W) One of sixteen pairs of last branch
  record registers on the last branch record stack. This part of the stack
  contains pointers to the destination instruction.

  @param  ECX  MSR_NEHALEM_LASTBRANCH_n_TO_IP
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_LASTBRANCH_0_TO_IP);
  AsmWriteMsr64 (MSR_NEHALEM_LASTBRANCH_0_TO_IP, Msr);
  @endcode
  @note MSR_NEHALEM_LASTBRANCH_0_TO_IP  is defined as MSR_LASTBRANCH_0_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_1_TO_IP  is defined as MSR_LASTBRANCH_1_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_2_TO_IP  is defined as MSR_LASTBRANCH_2_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_3_TO_IP  is defined as MSR_LASTBRANCH_3_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_4_TO_IP  is defined as MSR_LASTBRANCH_4_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_5_TO_IP  is defined as MSR_LASTBRANCH_5_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_6_TO_IP  is defined as MSR_LASTBRANCH_6_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_7_TO_IP  is defined as MSR_LASTBRANCH_7_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_8_TO_IP  is defined as MSR_LASTBRANCH_8_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_9_TO_IP  is defined as MSR_LASTBRANCH_9_TO_IP  in SDM.
        MSR_NEHALEM_LASTBRANCH_10_TO_IP is defined as MSR_LASTBRANCH_10_TO_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_11_TO_IP is defined as MSR_LASTBRANCH_11_TO_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_12_TO_IP is defined as MSR_LASTBRANCH_12_TO_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_13_TO_IP is defined as MSR_LASTBRANCH_13_TO_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_14_TO_IP is defined as MSR_LASTBRANCH_14_TO_IP in SDM.
        MSR_NEHALEM_LASTBRANCH_15_TO_IP is defined as MSR_LASTBRANCH_15_TO_IP in SDM.
  @{
**/
#define MSR_NEHALEM_LASTBRANCH_0_TO_IP   0x000006C0
#define MSR_NEHALEM_LASTBRANCH_1_TO_IP   0x000006C1
#define MSR_NEHALEM_LASTBRANCH_2_TO_IP   0x000006C2
#define MSR_NEHALEM_LASTBRANCH_3_TO_IP   0x000006C3
#define MSR_NEHALEM_LASTBRANCH_4_TO_IP   0x000006C4
#define MSR_NEHALEM_LASTBRANCH_5_TO_IP   0x000006C5
#define MSR_NEHALEM_LASTBRANCH_6_TO_IP   0x000006C6
#define MSR_NEHALEM_LASTBRANCH_7_TO_IP   0x000006C7
#define MSR_NEHALEM_LASTBRANCH_8_TO_IP   0x000006C8
#define MSR_NEHALEM_LASTBRANCH_9_TO_IP   0x000006C9
#define MSR_NEHALEM_LASTBRANCH_10_TO_IP  0x000006CA
#define MSR_NEHALEM_LASTBRANCH_11_TO_IP  0x000006CB
#define MSR_NEHALEM_LASTBRANCH_12_TO_IP  0x000006CC
#define MSR_NEHALEM_LASTBRANCH_13_TO_IP  0x000006CD
#define MSR_NEHALEM_LASTBRANCH_14_TO_IP  0x000006CE
#define MSR_NEHALEM_LASTBRANCH_15_TO_IP  0x000006CF
/// @}

/**
  Package.

  @param  ECX  MSR_NEHALEM_GQ_SNOOP_MESF (0x00000301)
  @param  EAX  Lower 32-bits of MSR value.
               Described by the type MSR_NEHALEM_GQ_SNOOP_MESF_REGISTER.
  @param  EDX  Upper 32-bits of MSR value.
               Described by the type MSR_NEHALEM_GQ_SNOOP_MESF_REGISTER.

  <b>Example usage</b>
  @code
  MSR_NEHALEM_GQ_SNOOP_MESF_REGISTER  Msr;

  Msr.Uint64 = AsmReadMsr64 (MSR_NEHALEM_GQ_SNOOP_MESF);
  AsmWriteMsr64 (MSR_NEHALEM_GQ_SNOOP_MESF, Msr.Uint64);
  @endcode
  @note MSR_NEHALEM_GQ_SNOOP_MESF is defined as MSR_GQ_SNOOP_MESF in SDM.
**/
#define MSR_NEHALEM_GQ_SNOOP_MESF  0x00000301

/**
  MSR information returned for MSR index #MSR_NEHALEM_GQ_SNOOP_MESF
**/
typedef union {
  ///
  /// Individual bit fields
  ///
  struct {
    ///
    /// [Bit 0] From M to S (R/W).
    ///
    UINT32    FromMtoS  : 1;
    ///
    /// [Bit 1] From E to S (R/W).
    ///
    UINT32    FromEtoS  : 1;
    ///
    /// [Bit 2] From S to S (R/W).
    ///
    UINT32    FromStoS  : 1;
    ///
    /// [Bit 3] From F to S (R/W).
    ///
    UINT32    FromFtoS  : 1;
    ///
    /// [Bit 4] From M to I (R/W).
    ///
    UINT32    FromMtoI  : 1;
    ///
    /// [Bit 5] From E to I (R/W).
    ///
    UINT32    FromEtoI  : 1;
    ///
    /// [Bit 6] From S to I (R/W).
    ///
    UINT32    FromStoI  : 1;
    ///
    /// [Bit 7] From F to I (R/W).
    ///
    UINT32    FromFtoI  : 1;
    UINT32    Reserved1 : 24;
    UINT32    Reserved2 : 32;
  } Bits;
  ///
  /// All bit fields as a 32-bit value
  ///
  UINT32    Uint32;
  ///
  /// All bit fields as a 64-bit value
  ///
  UINT64    Uint64;
} MSR_NEHALEM_GQ_SNOOP_MESF_REGISTER;

/**
  Package. See Section 18.3.1.2.1, "Uncore Performance Monitoring Management
  Facility.".

  @param  ECX  MSR_NEHALEM_UNCORE_PERF_GLOBAL_CTRL (0x00000391)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_UNCORE_PERF_GLOBAL_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_UNCORE_PERF_GLOBAL_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_UNCORE_PERF_GLOBAL_CTRL is defined as MSR_UNCORE_PERF_GLOBAL_CTRL in SDM.
**/
#define MSR_NEHALEM_UNCORE_PERF_GLOBAL_CTRL  0x00000391

/**
  Package. See Section 18.3.1.2.1, "Uncore Performance Monitoring Management
  Facility.".

  @param  ECX  MSR_NEHALEM_UNCORE_PERF_GLOBAL_STATUS (0x00000392)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_UNCORE_PERF_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_UNCORE_PERF_GLOBAL_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_UNCORE_PERF_GLOBAL_STATUS is defined as MSR_UNCORE_PERF_GLOBAL_STATUS in SDM.
**/
#define MSR_NEHALEM_UNCORE_PERF_GLOBAL_STATUS  0x00000392

/**
  Package. See Section 18.3.1.2.1, "Uncore Performance Monitoring Management
  Facility.".

  @param  ECX  MSR_NEHALEM_UNCORE_PERF_GLOBAL_OVF_CTRL (0x00000393)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_UNCORE_PERF_GLOBAL_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_UNCORE_PERF_GLOBAL_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_UNCORE_PERF_GLOBAL_OVF_CTRL is defined as MSR_UNCORE_PERF_GLOBAL_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_UNCORE_PERF_GLOBAL_OVF_CTRL  0x00000393

/**
  Package. See Section 18.3.1.2.1, "Uncore Performance Monitoring Management
  Facility.".

  @param  ECX  MSR_NEHALEM_UNCORE_FIXED_CTR0 (0x00000394)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_UNCORE_FIXED_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_UNCORE_FIXED_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_UNCORE_FIXED_CTR0 is defined as MSR_UNCORE_FIXED_CTR0 in SDM.
**/
#define MSR_NEHALEM_UNCORE_FIXED_CTR0  0x00000394

/**
  Package. See Section 18.3.1.2.1, "Uncore Performance Monitoring Management
  Facility.".

  @param  ECX  MSR_NEHALEM_UNCORE_FIXED_CTR_CTRL (0x00000395)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_UNCORE_FIXED_CTR_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_UNCORE_FIXED_CTR_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_UNCORE_FIXED_CTR_CTRL is defined as MSR_UNCORE_FIXED_CTR_CTRL in SDM.
**/
#define MSR_NEHALEM_UNCORE_FIXED_CTR_CTRL  0x00000395

/**
  Package. See Section 18.3.1.2.3, "Uncore Address/Opcode Match MSR.".

  @param  ECX  MSR_NEHALEM_UNCORE_ADDR_OPCODE_MATCH (0x00000396)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_UNCORE_ADDR_OPCODE_MATCH);
  AsmWriteMsr64 (MSR_NEHALEM_UNCORE_ADDR_OPCODE_MATCH, Msr);
  @endcode
  @note MSR_NEHALEM_UNCORE_ADDR_OPCODE_MATCH is defined as MSR_UNCORE_ADDR_OPCODE_MATCH in SDM.
**/
#define MSR_NEHALEM_UNCORE_ADDR_OPCODE_MATCH  0x00000396

/**
  Package. See Section 18.3.1.2.2, "Uncore Performance Event Configuration
  Facility.".

  @param  ECX  MSR_NEHALEM_UNCORE_PMCi
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_UNCORE_PMC0);
  AsmWriteMsr64 (MSR_NEHALEM_UNCORE_PMC0, Msr);
  @endcode
  @note MSR_NEHALEM_UNCORE_PMC0 is defined as MSR_UNCORE_PMC0 in SDM.
        MSR_NEHALEM_UNCORE_PMC1 is defined as MSR_UNCORE_PMC1 in SDM.
        MSR_NEHALEM_UNCORE_PMC2 is defined as MSR_UNCORE_PMC2 in SDM.
        MSR_NEHALEM_UNCORE_PMC3 is defined as MSR_UNCORE_PMC3 in SDM.
        MSR_NEHALEM_UNCORE_PMC4 is defined as MSR_UNCORE_PMC4 in SDM.
        MSR_NEHALEM_UNCORE_PMC5 is defined as MSR_UNCORE_PMC5 in SDM.
        MSR_NEHALEM_UNCORE_PMC6 is defined as MSR_UNCORE_PMC6 in SDM.
        MSR_NEHALEM_UNCORE_PMC7 is defined as MSR_UNCORE_PMC7 in SDM.
  @{
**/
#define MSR_NEHALEM_UNCORE_PMC0  0x000003B0
#define MSR_NEHALEM_UNCORE_PMC1  0x000003B1
#define MSR_NEHALEM_UNCORE_PMC2  0x000003B2
#define MSR_NEHALEM_UNCORE_PMC3  0x000003B3
#define MSR_NEHALEM_UNCORE_PMC4  0x000003B4
#define MSR_NEHALEM_UNCORE_PMC5  0x000003B5
#define MSR_NEHALEM_UNCORE_PMC6  0x000003B6
#define MSR_NEHALEM_UNCORE_PMC7  0x000003B7
/// @}

/**
  Package. See Section 18.3.1.2.2, "Uncore Performance Event Configuration
  Facility.".

  @param  ECX  MSR_NEHALEM_UNCORE_PERFEVTSELi
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_UNCORE_PERFEVTSEL0);
  AsmWriteMsr64 (MSR_NEHALEM_UNCORE_PERFEVTSEL0, Msr);
  @endcode
  @note MSR_NEHALEM_UNCORE_PERFEVTSEL0 is defined as MSR_UNCORE_PERFEVTSEL0 in SDM.
        MSR_NEHALEM_UNCORE_PERFEVTSEL1 is defined as MSR_UNCORE_PERFEVTSEL1 in SDM.
        MSR_NEHALEM_UNCORE_PERFEVTSEL2 is defined as MSR_UNCORE_PERFEVTSEL2 in SDM.
        MSR_NEHALEM_UNCORE_PERFEVTSEL3 is defined as MSR_UNCORE_PERFEVTSEL3 in SDM.
        MSR_NEHALEM_UNCORE_PERFEVTSEL4 is defined as MSR_UNCORE_PERFEVTSEL4 in SDM.
        MSR_NEHALEM_UNCORE_PERFEVTSEL5 is defined as MSR_UNCORE_PERFEVTSEL5 in SDM.
        MSR_NEHALEM_UNCORE_PERFEVTSEL6 is defined as MSR_UNCORE_PERFEVTSEL6 in SDM.
        MSR_NEHALEM_UNCORE_PERFEVTSEL7 is defined as MSR_UNCORE_PERFEVTSEL7 in SDM.
  @{
**/
#define MSR_NEHALEM_UNCORE_PERFEVTSEL0  0x000003C0
#define MSR_NEHALEM_UNCORE_PERFEVTSEL1  0x000003C1
#define MSR_NEHALEM_UNCORE_PERFEVTSEL2  0x000003C2
#define MSR_NEHALEM_UNCORE_PERFEVTSEL3  0x000003C3
#define MSR_NEHALEM_UNCORE_PERFEVTSEL4  0x000003C4
#define MSR_NEHALEM_UNCORE_PERFEVTSEL5  0x000003C5
#define MSR_NEHALEM_UNCORE_PERFEVTSEL6  0x000003C6
#define MSR_NEHALEM_UNCORE_PERFEVTSEL7  0x000003C7
/// @}

/**
  Package. Uncore W-box perfmon fixed counter.

  @param  ECX  MSR_NEHALEM_W_PMON_FIXED_CTR (0x00000394)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_FIXED_CTR);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_FIXED_CTR, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_FIXED_CTR is defined as MSR_W_PMON_FIXED_CTR in SDM.
**/
#define MSR_NEHALEM_W_PMON_FIXED_CTR  0x00000394

/**
  Package. Uncore U-box perfmon fixed counter control MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_FIXED_CTR_CTL (0x00000395)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_FIXED_CTR_CTL);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_FIXED_CTR_CTL, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_FIXED_CTR_CTL is defined as MSR_W_PMON_FIXED_CTR_CTL in SDM.
**/
#define MSR_NEHALEM_W_PMON_FIXED_CTR_CTL  0x00000395

/**
  Package. Uncore U-box perfmon global control MSR.

  @param  ECX  MSR_NEHALEM_U_PMON_GLOBAL_CTRL (0x00000C00)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_U_PMON_GLOBAL_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_U_PMON_GLOBAL_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_U_PMON_GLOBAL_CTRL is defined as MSR_U_PMON_GLOBAL_CTRL in SDM.
**/
#define MSR_NEHALEM_U_PMON_GLOBAL_CTRL  0x00000C00

/**
  Package. Uncore U-box perfmon global status MSR.

  @param  ECX  MSR_NEHALEM_U_PMON_GLOBAL_STATUS (0x00000C01)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_U_PMON_GLOBAL_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_U_PMON_GLOBAL_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_U_PMON_GLOBAL_STATUS is defined as MSR_U_PMON_GLOBAL_STATUS in SDM.
**/
#define MSR_NEHALEM_U_PMON_GLOBAL_STATUS  0x00000C01

/**
  Package. Uncore U-box perfmon global overflow control MSR.

  @param  ECX  MSR_NEHALEM_U_PMON_GLOBAL_OVF_CTRL (0x00000C02)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_U_PMON_GLOBAL_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_U_PMON_GLOBAL_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_U_PMON_GLOBAL_OVF_CTRL is defined as MSR_U_PMON_GLOBAL_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_U_PMON_GLOBAL_OVF_CTRL  0x00000C02

/**
  Package. Uncore U-box perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_U_PMON_EVNT_SEL (0x00000C10)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_U_PMON_EVNT_SEL);
  AsmWriteMsr64 (MSR_NEHALEM_U_PMON_EVNT_SEL, Msr);
  @endcode
  @note MSR_NEHALEM_U_PMON_EVNT_SEL is defined as MSR_U_PMON_EVNT_SEL in SDM.
**/
#define MSR_NEHALEM_U_PMON_EVNT_SEL  0x00000C10

/**
  Package. Uncore U-box perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_U_PMON_CTR (0x00000C11)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_U_PMON_CTR);
  AsmWriteMsr64 (MSR_NEHALEM_U_PMON_CTR, Msr);
  @endcode
  @note MSR_NEHALEM_U_PMON_CTR is defined as MSR_U_PMON_CTR in SDM.
**/
#define MSR_NEHALEM_U_PMON_CTR  0x00000C11

/**
  Package. Uncore B-box 0 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_BOX_CTRL (0x00000C20)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_BOX_CTRL is defined as MSR_B0_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_B0_PMON_BOX_CTRL  0x00000C20

/**
  Package. Uncore B-box 0 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_BOX_STATUS (0x00000C21)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_BOX_STATUS is defined as MSR_B0_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_B0_PMON_BOX_STATUS  0x00000C21

/**
  Package. Uncore B-box 0 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_BOX_OVF_CTRL (0x00000C22)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_BOX_OVF_CTRL is defined as MSR_B0_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_B0_PMON_BOX_OVF_CTRL  0x00000C22

/**
  Package. Uncore B-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_EVNT_SEL0 (0x00000C30)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_EVNT_SEL0 is defined as MSR_B0_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_B0_PMON_EVNT_SEL0  0x00000C30

/**
  Package. Uncore B-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_CTR0 (0x00000C31)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_CTR0 is defined as MSR_B0_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_B0_PMON_CTR0  0x00000C31

/**
  Package. Uncore B-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_EVNT_SEL1 (0x00000C32)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_EVNT_SEL1 is defined as MSR_B0_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_B0_PMON_EVNT_SEL1  0x00000C32

/**
  Package. Uncore B-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_CTR1 (0x00000C33)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_CTR1 is defined as MSR_B0_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_B0_PMON_CTR1  0x00000C33

/**
  Package. Uncore B-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_EVNT_SEL2 (0x00000C34)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_EVNT_SEL2 is defined as MSR_B0_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_B0_PMON_EVNT_SEL2  0x00000C34

/**
  Package. Uncore B-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_CTR2 (0x00000C35)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_CTR2 is defined as MSR_B0_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_B0_PMON_CTR2  0x00000C35

/**
  Package. Uncore B-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_EVNT_SEL3 (0x00000C36)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_EVNT_SEL3 is defined as MSR_B0_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_B0_PMON_EVNT_SEL3  0x00000C36

/**
  Package. Uncore B-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_CTR3 (0x00000C37)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_CTR3 is defined as MSR_B0_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_B0_PMON_CTR3  0x00000C37

/**
  Package. Uncore S-box 0 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_BOX_CTRL (0x00000C40)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_BOX_CTRL is defined as MSR_S0_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_S0_PMON_BOX_CTRL  0x00000C40

/**
  Package. Uncore S-box 0 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_BOX_STATUS (0x00000C41)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_BOX_STATUS is defined as MSR_S0_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_S0_PMON_BOX_STATUS  0x00000C41

/**
  Package. Uncore S-box 0 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_BOX_OVF_CTRL (0x00000C42)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_BOX_OVF_CTRL is defined as MSR_S0_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_S0_PMON_BOX_OVF_CTRL  0x00000C42

/**
  Package. Uncore S-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_EVNT_SEL0 (0x00000C50)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_EVNT_SEL0 is defined as MSR_S0_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_S0_PMON_EVNT_SEL0  0x00000C50

/**
  Package. Uncore S-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_CTR0 (0x00000C51)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_CTR0 is defined as MSR_S0_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_S0_PMON_CTR0  0x00000C51

/**
  Package. Uncore S-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_EVNT_SEL1 (0x00000C52)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_EVNT_SEL1 is defined as MSR_S0_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_S0_PMON_EVNT_SEL1  0x00000C52

/**
  Package. Uncore S-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_CTR1 (0x00000C53)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_CTR1 is defined as MSR_S0_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_S0_PMON_CTR1  0x00000C53

/**
  Package. Uncore S-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_EVNT_SEL2 (0x00000C54)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_EVNT_SEL2 is defined as MSR_S0_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_S0_PMON_EVNT_SEL2  0x00000C54

/**
  Package. Uncore S-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_CTR2 (0x00000C55)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_CTR2 is defined as MSR_S0_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_S0_PMON_CTR2  0x00000C55

/**
  Package. Uncore S-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_EVNT_SEL3 (0x00000C56)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_EVNT_SEL3 is defined as MSR_S0_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_S0_PMON_EVNT_SEL3  0x00000C56

/**
  Package. Uncore S-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_CTR3 (0x00000C57)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_CTR3 is defined as MSR_S0_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_S0_PMON_CTR3  0x00000C57

/**
  Package. Uncore B-box 1 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_BOX_CTRL (0x00000C60)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_BOX_CTRL is defined as MSR_B1_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_B1_PMON_BOX_CTRL  0x00000C60

/**
  Package. Uncore B-box 1 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_BOX_STATUS (0x00000C61)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_BOX_STATUS is defined as MSR_B1_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_B1_PMON_BOX_STATUS  0x00000C61

/**
  Package. Uncore B-box 1 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_BOX_OVF_CTRL (0x00000C62)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_BOX_OVF_CTRL is defined as MSR_B1_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_B1_PMON_BOX_OVF_CTRL  0x00000C62

/**
  Package. Uncore B-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_EVNT_SEL0 (0x00000C70)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_EVNT_SEL0 is defined as MSR_B1_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_B1_PMON_EVNT_SEL0  0x00000C70

/**
  Package. Uncore B-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_CTR0 (0x00000C71)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_CTR0 is defined as MSR_B1_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_B1_PMON_CTR0  0x00000C71

/**
  Package. Uncore B-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_EVNT_SEL1 (0x00000C72)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_EVNT_SEL1 is defined as MSR_B1_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_B1_PMON_EVNT_SEL1  0x00000C72

/**
  Package. Uncore B-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_CTR1 (0x00000C73)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_CTR1 is defined as MSR_B1_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_B1_PMON_CTR1  0x00000C73

/**
  Package. Uncore B-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_EVNT_SEL2 (0x00000C74)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_EVNT_SEL2 is defined as MSR_B1_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_B1_PMON_EVNT_SEL2  0x00000C74

/**
  Package. Uncore B-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_CTR2 (0x00000C75)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_CTR2 is defined as MSR_B1_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_B1_PMON_CTR2  0x00000C75

/**
  Package. Uncore B-box 1vperfmon event select MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_EVNT_SEL3 (0x00000C76)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_EVNT_SEL3 is defined as MSR_B1_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_B1_PMON_EVNT_SEL3  0x00000C76

/**
  Package. Uncore B-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_CTR3 (0x00000C77)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_CTR3 is defined as MSR_B1_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_B1_PMON_CTR3  0x00000C77

/**
  Package. Uncore W-box perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_BOX_CTRL (0x00000C80)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_BOX_CTRL is defined as MSR_W_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_W_PMON_BOX_CTRL  0x00000C80

/**
  Package. Uncore W-box perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_BOX_STATUS (0x00000C81)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_BOX_STATUS is defined as MSR_W_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_W_PMON_BOX_STATUS  0x00000C81

/**
  Package. Uncore W-box perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_BOX_OVF_CTRL (0x00000C82)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_BOX_OVF_CTRL is defined as MSR_W_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_W_PMON_BOX_OVF_CTRL  0x00000C82

/**
  Package. Uncore W-box perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_EVNT_SEL0 (0x00000C90)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_EVNT_SEL0 is defined as MSR_W_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_W_PMON_EVNT_SEL0  0x00000C90

/**
  Package. Uncore W-box perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_CTR0 (0x00000C91)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_CTR0 is defined as MSR_W_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_W_PMON_CTR0  0x00000C91

/**
  Package. Uncore W-box perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_EVNT_SEL1 (0x00000C92)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_EVNT_SEL1 is defined as MSR_W_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_W_PMON_EVNT_SEL1  0x00000C92

/**
  Package. Uncore W-box perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_CTR1 (0x00000C93)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_CTR1 is defined as MSR_W_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_W_PMON_CTR1  0x00000C93

/**
  Package. Uncore W-box perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_EVNT_SEL2 (0x00000C94)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_EVNT_SEL2 is defined as MSR_W_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_W_PMON_EVNT_SEL2  0x00000C94

/**
  Package. Uncore W-box perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_CTR2 (0x00000C95)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_CTR2 is defined as MSR_W_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_W_PMON_CTR2  0x00000C95

/**
  Package. Uncore W-box perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_EVNT_SEL3 (0x00000C96)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_EVNT_SEL3 is defined as MSR_W_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_W_PMON_EVNT_SEL3  0x00000C96

/**
  Package. Uncore W-box perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_W_PMON_CTR3 (0x00000C97)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_W_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_W_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_W_PMON_CTR3 is defined as MSR_W_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_W_PMON_CTR3  0x00000C97

/**
  Package. Uncore M-box 0 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_BOX_CTRL (0x00000CA0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_BOX_CTRL is defined as MSR_M0_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_M0_PMON_BOX_CTRL  0x00000CA0

/**
  Package. Uncore M-box 0 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_BOX_STATUS (0x00000CA1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_BOX_STATUS is defined as MSR_M0_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_M0_PMON_BOX_STATUS  0x00000CA1

/**
  Package. Uncore M-box 0 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_BOX_OVF_CTRL (0x00000CA2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_BOX_OVF_CTRL is defined as MSR_M0_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_M0_PMON_BOX_OVF_CTRL  0x00000CA2

/**
  Package. Uncore M-box 0 perfmon time stamp unit select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_TIMESTAMP (0x00000CA4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_TIMESTAMP);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_TIMESTAMP, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_TIMESTAMP is defined as MSR_M0_PMON_TIMESTAMP in SDM.
**/
#define MSR_NEHALEM_M0_PMON_TIMESTAMP  0x00000CA4

/**
  Package. Uncore M-box 0 perfmon DSP unit select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_DSP (0x00000CA5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_DSP);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_DSP, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_DSP is defined as MSR_M0_PMON_DSP in SDM.
**/
#define MSR_NEHALEM_M0_PMON_DSP  0x00000CA5

/**
  Package. Uncore M-box 0 perfmon ISS unit select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_ISS (0x00000CA6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_ISS);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_ISS, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_ISS is defined as MSR_M0_PMON_ISS in SDM.
**/
#define MSR_NEHALEM_M0_PMON_ISS  0x00000CA6

/**
  Package. Uncore M-box 0 perfmon MAP unit select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_MAP (0x00000CA7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_MAP);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_MAP, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_MAP is defined as MSR_M0_PMON_MAP in SDM.
**/
#define MSR_NEHALEM_M0_PMON_MAP  0x00000CA7

/**
  Package. Uncore M-box 0 perfmon MIC THR select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_MSC_THR (0x00000CA8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_MSC_THR);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_MSC_THR, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_MSC_THR is defined as MSR_M0_PMON_MSC_THR in SDM.
**/
#define MSR_NEHALEM_M0_PMON_MSC_THR  0x00000CA8

/**
  Package. Uncore M-box 0 perfmon PGT unit select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_PGT (0x00000CA9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_PGT);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_PGT, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_PGT is defined as MSR_M0_PMON_PGT in SDM.
**/
#define MSR_NEHALEM_M0_PMON_PGT  0x00000CA9

/**
  Package. Uncore M-box 0 perfmon PLD unit select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_PLD (0x00000CAA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_PLD);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_PLD, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_PLD is defined as MSR_M0_PMON_PLD in SDM.
**/
#define MSR_NEHALEM_M0_PMON_PLD  0x00000CAA

/**
  Package. Uncore M-box 0 perfmon ZDP unit select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_ZDP (0x00000CAB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_ZDP);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_ZDP, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_ZDP is defined as MSR_M0_PMON_ZDP in SDM.
**/
#define MSR_NEHALEM_M0_PMON_ZDP  0x00000CAB

/**
  Package. Uncore M-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_EVNT_SEL0 (0x00000CB0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_EVNT_SEL0 is defined as MSR_M0_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_EVNT_SEL0  0x00000CB0

/**
  Package. Uncore M-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_CTR0 (0x00000CB1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_CTR0 is defined as MSR_M0_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_CTR0  0x00000CB1

/**
  Package. Uncore M-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_EVNT_SEL1 (0x00000CB2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_EVNT_SEL1 is defined as MSR_M0_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_EVNT_SEL1  0x00000CB2

/**
  Package. Uncore M-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_CTR1 (0x00000CB3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_CTR1 is defined as MSR_M0_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_CTR1  0x00000CB3

/**
  Package. Uncore M-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_EVNT_SEL2 (0x00000CB4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_EVNT_SEL2 is defined as MSR_M0_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_EVNT_SEL2  0x00000CB4

/**
  Package. Uncore M-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_CTR2 (0x00000CB5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_CTR2 is defined as MSR_M0_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_CTR2  0x00000CB5

/**
  Package. Uncore M-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_EVNT_SEL3 (0x00000CB6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_EVNT_SEL3 is defined as MSR_M0_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_EVNT_SEL3  0x00000CB6

/**
  Package. Uncore M-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_CTR3 (0x00000CB7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_CTR3 is defined as MSR_M0_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_CTR3  0x00000CB7

/**
  Package. Uncore M-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_EVNT_SEL4 (0x00000CB8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_EVNT_SEL4 is defined as MSR_M0_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_EVNT_SEL4  0x00000CB8

/**
  Package. Uncore M-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_CTR4 (0x00000CB9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_CTR4 is defined as MSR_M0_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_CTR4  0x00000CB9

/**
  Package. Uncore M-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_EVNT_SEL5 (0x00000CBA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_EVNT_SEL5 is defined as MSR_M0_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_EVNT_SEL5  0x00000CBA

/**
  Package. Uncore M-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_CTR5 (0x00000CBB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_CTR5 is defined as MSR_M0_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_M0_PMON_CTR5  0x00000CBB

/**
  Package. Uncore S-box 1 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_BOX_CTRL (0x00000CC0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_BOX_CTRL is defined as MSR_S1_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_S1_PMON_BOX_CTRL  0x00000CC0

/**
  Package. Uncore S-box 1 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_BOX_STATUS (0x00000CC1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_BOX_STATUS is defined as MSR_S1_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_S1_PMON_BOX_STATUS  0x00000CC1

/**
  Package. Uncore S-box 1 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_BOX_OVF_CTRL (0x00000CC2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_BOX_OVF_CTRL is defined as MSR_S1_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_S1_PMON_BOX_OVF_CTRL  0x00000CC2

/**
  Package. Uncore S-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_EVNT_SEL0 (0x00000CD0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_EVNT_SEL0 is defined as MSR_S1_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_S1_PMON_EVNT_SEL0  0x00000CD0

/**
  Package. Uncore S-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_CTR0 (0x00000CD1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_CTR0 is defined as MSR_S1_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_S1_PMON_CTR0  0x00000CD1

/**
  Package. Uncore S-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_EVNT_SEL1 (0x00000CD2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_EVNT_SEL1 is defined as MSR_S1_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_S1_PMON_EVNT_SEL1  0x00000CD2

/**
  Package. Uncore S-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_CTR1 (0x00000CD3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_CTR1 is defined as MSR_S1_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_S1_PMON_CTR1  0x00000CD3

/**
  Package. Uncore S-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_EVNT_SEL2 (0x00000CD4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_EVNT_SEL2 is defined as MSR_S1_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_S1_PMON_EVNT_SEL2  0x00000CD4

/**
  Package. Uncore S-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_CTR2 (0x00000CD5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_CTR2 is defined as MSR_S1_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_S1_PMON_CTR2  0x00000CD5

/**
  Package. Uncore S-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_EVNT_SEL3 (0x00000CD6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_EVNT_SEL3 is defined as MSR_S1_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_S1_PMON_EVNT_SEL3  0x00000CD6

/**
  Package. Uncore S-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_CTR3 (0x00000CD7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_CTR3 is defined as MSR_S1_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_S1_PMON_CTR3  0x00000CD7

/**
  Package. Uncore M-box 1 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_BOX_CTRL (0x00000CE0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_BOX_CTRL is defined as MSR_M1_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_M1_PMON_BOX_CTRL  0x00000CE0

/**
  Package. Uncore M-box 1 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_BOX_STATUS (0x00000CE1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_BOX_STATUS is defined as MSR_M1_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_M1_PMON_BOX_STATUS  0x00000CE1

/**
  Package. Uncore M-box 1 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_BOX_OVF_CTRL (0x00000CE2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_BOX_OVF_CTRL is defined as MSR_M1_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_M1_PMON_BOX_OVF_CTRL  0x00000CE2

/**
  Package. Uncore M-box 1 perfmon time stamp unit select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_TIMESTAMP (0x00000CE4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_TIMESTAMP);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_TIMESTAMP, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_TIMESTAMP is defined as MSR_M1_PMON_TIMESTAMP in SDM.
**/
#define MSR_NEHALEM_M1_PMON_TIMESTAMP  0x00000CE4

/**
  Package. Uncore M-box 1 perfmon DSP unit select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_DSP (0x00000CE5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_DSP);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_DSP, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_DSP is defined as MSR_M1_PMON_DSP in SDM.
**/
#define MSR_NEHALEM_M1_PMON_DSP  0x00000CE5

/**
  Package. Uncore M-box 1 perfmon ISS unit select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_ISS (0x00000CE6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_ISS);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_ISS, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_ISS is defined as MSR_M1_PMON_ISS in SDM.
**/
#define MSR_NEHALEM_M1_PMON_ISS  0x00000CE6

/**
  Package. Uncore M-box 1 perfmon MAP unit select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_MAP (0x00000CE7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_MAP);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_MAP, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_MAP is defined as MSR_M1_PMON_MAP in SDM.
**/
#define MSR_NEHALEM_M1_PMON_MAP  0x00000CE7

/**
  Package. Uncore M-box 1 perfmon MIC THR select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_MSC_THR (0x00000CE8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_MSC_THR);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_MSC_THR, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_MSC_THR is defined as MSR_M1_PMON_MSC_THR in SDM.
**/
#define MSR_NEHALEM_M1_PMON_MSC_THR  0x00000CE8

/**
  Package. Uncore M-box 1 perfmon PGT unit select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_PGT (0x00000CE9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_PGT);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_PGT, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_PGT is defined as MSR_M1_PMON_PGT in SDM.
**/
#define MSR_NEHALEM_M1_PMON_PGT  0x00000CE9

/**
  Package. Uncore M-box 1 perfmon PLD unit select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_PLD (0x00000CEA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_PLD);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_PLD, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_PLD is defined as MSR_M1_PMON_PLD in SDM.
**/
#define MSR_NEHALEM_M1_PMON_PLD  0x00000CEA

/**
  Package. Uncore M-box 1 perfmon ZDP unit select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_ZDP (0x00000CEB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_ZDP);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_ZDP, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_ZDP is defined as MSR_M1_PMON_ZDP in SDM.
**/
#define MSR_NEHALEM_M1_PMON_ZDP  0x00000CEB

/**
  Package. Uncore M-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_EVNT_SEL0 (0x00000CF0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_EVNT_SEL0 is defined as MSR_M1_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_EVNT_SEL0  0x00000CF0

/**
  Package. Uncore M-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_CTR0 (0x00000CF1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_CTR0 is defined as MSR_M1_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_CTR0  0x00000CF1

/**
  Package. Uncore M-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_EVNT_SEL1 (0x00000CF2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_EVNT_SEL1 is defined as MSR_M1_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_EVNT_SEL1  0x00000CF2

/**
  Package. Uncore M-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_CTR1 (0x00000CF3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_CTR1 is defined as MSR_M1_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_CTR1  0x00000CF3

/**
  Package. Uncore M-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_EVNT_SEL2 (0x00000CF4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_EVNT_SEL2 is defined as MSR_M1_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_EVNT_SEL2  0x00000CF4

/**
  Package. Uncore M-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_CTR2 (0x00000CF5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_CTR2 is defined as MSR_M1_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_CTR2  0x00000CF5

/**
  Package. Uncore M-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_EVNT_SEL3 (0x00000CF6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_EVNT_SEL3 is defined as MSR_M1_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_EVNT_SEL3  0x00000CF6

/**
  Package. Uncore M-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_CTR3 (0x00000CF7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_CTR3 is defined as MSR_M1_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_CTR3  0x00000CF7

/**
  Package. Uncore M-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_EVNT_SEL4 (0x00000CF8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_EVNT_SEL4 is defined as MSR_M1_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_EVNT_SEL4  0x00000CF8

/**
  Package. Uncore M-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_CTR4 (0x00000CF9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_CTR4 is defined as MSR_M1_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_CTR4  0x00000CF9

/**
  Package. Uncore M-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_EVNT_SEL5 (0x00000CFA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_EVNT_SEL5 is defined as MSR_M1_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_EVNT_SEL5  0x00000CFA

/**
  Package. Uncore M-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_CTR5 (0x00000CFB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_CTR5 is defined as MSR_M1_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_M1_PMON_CTR5  0x00000CFB

/**
  Package. Uncore C-box 0 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_BOX_CTRL (0x00000D00)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_BOX_CTRL is defined as MSR_C0_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_C0_PMON_BOX_CTRL  0x00000D00

/**
  Package. Uncore C-box 0 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_BOX_STATUS (0x00000D01)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_BOX_STATUS is defined as MSR_C0_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_C0_PMON_BOX_STATUS  0x00000D01

/**
  Package. Uncore C-box 0 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_BOX_OVF_CTRL (0x00000D02)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_BOX_OVF_CTRL is defined as MSR_C0_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_C0_PMON_BOX_OVF_CTRL  0x00000D02

/**
  Package. Uncore C-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_EVNT_SEL0 (0x00000D10)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_EVNT_SEL0 is defined as MSR_C0_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_EVNT_SEL0  0x00000D10

/**
  Package. Uncore C-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_CTR0 (0x00000D11)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_CTR0 is defined as MSR_C0_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_CTR0  0x00000D11

/**
  Package. Uncore C-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_EVNT_SEL1 (0x00000D12)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_EVNT_SEL1 is defined as MSR_C0_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_EVNT_SEL1  0x00000D12

/**
  Package. Uncore C-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_CTR1 (0x00000D13)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_CTR1 is defined as MSR_C0_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_CTR1  0x00000D13

/**
  Package. Uncore C-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_EVNT_SEL2 (0x00000D14)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_EVNT_SEL2 is defined as MSR_C0_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_EVNT_SEL2  0x00000D14

/**
  Package. Uncore C-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_CTR2 (0x00000D15)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_CTR2 is defined as MSR_C0_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_CTR2  0x00000D15

/**
  Package. Uncore C-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_EVNT_SEL3 (0x00000D16)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_EVNT_SEL3 is defined as MSR_C0_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_EVNT_SEL3  0x00000D16

/**
  Package. Uncore C-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_CTR3 (0x00000D17)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_CTR3 is defined as MSR_C0_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_CTR3  0x00000D17

/**
  Package. Uncore C-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_EVNT_SEL4 (0x00000D18)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_EVNT_SEL4 is defined as MSR_C0_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_EVNT_SEL4  0x00000D18

/**
  Package. Uncore C-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_CTR4 (0x00000D19)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_CTR4 is defined as MSR_C0_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_CTR4  0x00000D19

/**
  Package. Uncore C-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_EVNT_SEL5 (0x00000D1A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_EVNT_SEL5 is defined as MSR_C0_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_EVNT_SEL5  0x00000D1A

/**
  Package. Uncore C-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C0_PMON_CTR5 (0x00000D1B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C0_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_C0_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_C0_PMON_CTR5 is defined as MSR_C0_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_C0_PMON_CTR5  0x00000D1B

/**
  Package. Uncore C-box 4 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_BOX_CTRL (0x00000D20)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_BOX_CTRL is defined as MSR_C4_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_C4_PMON_BOX_CTRL  0x00000D20

/**
  Package. Uncore C-box 4 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_BOX_STATUS (0x00000D21)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_BOX_STATUS is defined as MSR_C4_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_C4_PMON_BOX_STATUS  0x00000D21

/**
  Package. Uncore C-box 4 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_BOX_OVF_CTRL (0x00000D22)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_BOX_OVF_CTRL is defined as MSR_C4_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_C4_PMON_BOX_OVF_CTRL  0x00000D22

/**
  Package. Uncore C-box 4 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_EVNT_SEL0 (0x00000D30)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_EVNT_SEL0 is defined as MSR_C4_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_EVNT_SEL0  0x00000D30

/**
  Package. Uncore C-box 4 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_CTR0 (0x00000D31)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_CTR0 is defined as MSR_C4_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_CTR0  0x00000D31

/**
  Package. Uncore C-box 4 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_EVNT_SEL1 (0x00000D32)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_EVNT_SEL1 is defined as MSR_C4_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_EVNT_SEL1  0x00000D32

/**
  Package. Uncore C-box 4 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_CTR1 (0x00000D33)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_CTR1 is defined as MSR_C4_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_CTR1  0x00000D33

/**
  Package. Uncore C-box 4 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_EVNT_SEL2 (0x00000D34)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_EVNT_SEL2 is defined as MSR_C4_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_EVNT_SEL2  0x00000D34

/**
  Package. Uncore C-box 4 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_CTR2 (0x00000D35)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_CTR2 is defined as MSR_C4_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_CTR2  0x00000D35

/**
  Package. Uncore C-box 4 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_EVNT_SEL3 (0x00000D36)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_EVNT_SEL3 is defined as MSR_C4_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_EVNT_SEL3  0x00000D36

/**
  Package. Uncore C-box 4 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_CTR3 (0x00000D37)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_CTR3 is defined as MSR_C4_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_CTR3  0x00000D37

/**
  Package. Uncore C-box 4 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_EVNT_SEL4 (0x00000D38)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_EVNT_SEL4 is defined as MSR_C4_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_EVNT_SEL4  0x00000D38

/**
  Package. Uncore C-box 4 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_CTR4 (0x00000D39)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_CTR4 is defined as MSR_C4_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_CTR4  0x00000D39

/**
  Package. Uncore C-box 4 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_EVNT_SEL5 (0x00000D3A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_EVNT_SEL5 is defined as MSR_C4_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_EVNT_SEL5  0x00000D3A

/**
  Package. Uncore C-box 4 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C4_PMON_CTR5 (0x00000D3B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C4_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_C4_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_C4_PMON_CTR5 is defined as MSR_C4_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_C4_PMON_CTR5  0x00000D3B

/**
  Package. Uncore C-box 2 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_BOX_CTRL (0x00000D40)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_BOX_CTRL is defined as MSR_C2_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_C2_PMON_BOX_CTRL  0x00000D40

/**
  Package. Uncore C-box 2 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_BOX_STATUS (0x00000D41)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_BOX_STATUS is defined as MSR_C2_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_C2_PMON_BOX_STATUS  0x00000D41

/**
  Package. Uncore C-box 2 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_BOX_OVF_CTRL (0x00000D42)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_BOX_OVF_CTRL is defined as MSR_C2_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_C2_PMON_BOX_OVF_CTRL  0x00000D42

/**
  Package. Uncore C-box 2 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_EVNT_SEL0 (0x00000D50)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_EVNT_SEL0 is defined as MSR_C2_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_EVNT_SEL0  0x00000D50

/**
  Package. Uncore C-box 2 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_CTR0 (0x00000D51)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_CTR0 is defined as MSR_C2_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_CTR0  0x00000D51

/**
  Package. Uncore C-box 2 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_EVNT_SEL1 (0x00000D52)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_EVNT_SEL1 is defined as MSR_C2_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_EVNT_SEL1  0x00000D52

/**
  Package. Uncore C-box 2 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_CTR1 (0x00000D53)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_CTR1 is defined as MSR_C2_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_CTR1  0x00000D53

/**
  Package. Uncore C-box 2 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_EVNT_SEL2 (0x00000D54)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_EVNT_SEL2 is defined as MSR_C2_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_EVNT_SEL2  0x00000D54

/**
  Package. Uncore C-box 2 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_CTR2 (0x00000D55)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_CTR2 is defined as MSR_C2_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_CTR2  0x00000D55

/**
  Package. Uncore C-box 2 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_EVNT_SEL3 (0x00000D56)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_EVNT_SEL3 is defined as MSR_C2_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_EVNT_SEL3  0x00000D56

/**
  Package. Uncore C-box 2 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_CTR3 (0x00000D57)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_CTR3 is defined as MSR_C2_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_CTR3  0x00000D57

/**
  Package. Uncore C-box 2 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_EVNT_SEL4 (0x00000D58)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_EVNT_SEL4 is defined as MSR_C2_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_EVNT_SEL4  0x00000D58

/**
  Package. Uncore C-box 2 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_CTR4 (0x00000D59)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_CTR4 is defined as MSR_C2_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_CTR4  0x00000D59

/**
  Package. Uncore C-box 2 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_EVNT_SEL5 (0x00000D5A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_EVNT_SEL5 is defined as MSR_C2_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_EVNT_SEL5  0x00000D5A

/**
  Package. Uncore C-box 2 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C2_PMON_CTR5 (0x00000D5B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C2_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_C2_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_C2_PMON_CTR5 is defined as MSR_C2_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_C2_PMON_CTR5  0x00000D5B

/**
  Package. Uncore C-box 6 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_BOX_CTRL (0x00000D60)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_BOX_CTRL is defined as MSR_C6_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_C6_PMON_BOX_CTRL  0x00000D60

/**
  Package. Uncore C-box 6 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_BOX_STATUS (0x00000D61)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_BOX_STATUS is defined as MSR_C6_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_C6_PMON_BOX_STATUS  0x00000D61

/**
  Package. Uncore C-box 6 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_BOX_OVF_CTRL (0x00000D62)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_BOX_OVF_CTRL is defined as MSR_C6_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_C6_PMON_BOX_OVF_CTRL  0x00000D62

/**
  Package. Uncore C-box 6 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_EVNT_SEL0 (0x00000D70)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_EVNT_SEL0 is defined as MSR_C6_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_EVNT_SEL0  0x00000D70

/**
  Package. Uncore C-box 6 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_CTR0 (0x00000D71)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_CTR0 is defined as MSR_C6_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_CTR0  0x00000D71

/**
  Package. Uncore C-box 6 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_EVNT_SEL1 (0x00000D72)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_EVNT_SEL1 is defined as MSR_C6_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_EVNT_SEL1  0x00000D72

/**
  Package. Uncore C-box 6 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_CTR1 (0x00000D73)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_CTR1 is defined as MSR_C6_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_CTR1  0x00000D73

/**
  Package. Uncore C-box 6 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_EVNT_SEL2 (0x00000D74)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_EVNT_SEL2 is defined as MSR_C6_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_EVNT_SEL2  0x00000D74

/**
  Package. Uncore C-box 6 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_CTR2 (0x00000D75)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_CTR2 is defined as MSR_C6_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_CTR2  0x00000D75

/**
  Package. Uncore C-box 6 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_EVNT_SEL3 (0x00000D76)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_EVNT_SEL3 is defined as MSR_C6_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_EVNT_SEL3  0x00000D76

/**
  Package. Uncore C-box 6 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_CTR3 (0x00000D77)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_CTR3 is defined as MSR_C6_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_CTR3  0x00000D77

/**
  Package. Uncore C-box 6 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_EVNT_SEL4 (0x00000D78)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_EVNT_SEL4 is defined as MSR_C6_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_EVNT_SEL4  0x00000D78

/**
  Package. Uncore C-box 6 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_CTR4 (0x00000D79)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_CTR4 is defined as MSR_C6_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_CTR4  0x00000D79

/**
  Package. Uncore C-box 6 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_EVNT_SEL5 (0x00000D7A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_EVNT_SEL5 is defined as MSR_C6_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_EVNT_SEL5  0x00000D7A

/**
  Package. Uncore C-box 6 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C6_PMON_CTR5 (0x00000D7B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C6_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_C6_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_C6_PMON_CTR5 is defined as MSR_C6_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_C6_PMON_CTR5  0x00000D7B

/**
  Package. Uncore C-box 1 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_BOX_CTRL (0x00000D80)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_BOX_CTRL is defined as MSR_C1_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_C1_PMON_BOX_CTRL  0x00000D80

/**
  Package. Uncore C-box 1 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_BOX_STATUS (0x00000D81)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_BOX_STATUS is defined as MSR_C1_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_C1_PMON_BOX_STATUS  0x00000D81

/**
  Package. Uncore C-box 1 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_BOX_OVF_CTRL (0x00000D82)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_BOX_OVF_CTRL is defined as MSR_C1_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_C1_PMON_BOX_OVF_CTRL  0x00000D82

/**
  Package. Uncore C-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_EVNT_SEL0 (0x00000D90)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_EVNT_SEL0 is defined as MSR_C1_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_EVNT_SEL0  0x00000D90

/**
  Package. Uncore C-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_CTR0 (0x00000D91)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_CTR0 is defined as MSR_C1_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_CTR0  0x00000D91

/**
  Package. Uncore C-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_EVNT_SEL1 (0x00000D92)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_EVNT_SEL1 is defined as MSR_C1_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_EVNT_SEL1  0x00000D92

/**
  Package. Uncore C-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_CTR1 (0x00000D93)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_CTR1 is defined as MSR_C1_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_CTR1  0x00000D93

/**
  Package. Uncore C-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_EVNT_SEL2 (0x00000D94)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_EVNT_SEL2 is defined as MSR_C1_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_EVNT_SEL2  0x00000D94

/**
  Package. Uncore C-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_CTR2 (0x00000D95)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_CTR2 is defined as MSR_C1_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_CTR2  0x00000D95

/**
  Package. Uncore C-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_EVNT_SEL3 (0x00000D96)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_EVNT_SEL3 is defined as MSR_C1_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_EVNT_SEL3  0x00000D96

/**
  Package. Uncore C-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_CTR3 (0x00000D97)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_CTR3 is defined as MSR_C1_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_CTR3  0x00000D97

/**
  Package. Uncore C-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_EVNT_SEL4 (0x00000D98)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_EVNT_SEL4 is defined as MSR_C1_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_EVNT_SEL4  0x00000D98

/**
  Package. Uncore C-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_CTR4 (0x00000D99)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_CTR4 is defined as MSR_C1_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_CTR4  0x00000D99

/**
  Package. Uncore C-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_EVNT_SEL5 (0x00000D9A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_EVNT_SEL5 is defined as MSR_C1_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_EVNT_SEL5  0x00000D9A

/**
  Package. Uncore C-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C1_PMON_CTR5 (0x00000D9B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C1_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_C1_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_C1_PMON_CTR5 is defined as MSR_C1_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_C1_PMON_CTR5  0x00000D9B

/**
  Package. Uncore C-box 5 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_BOX_CTRL (0x00000DA0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_BOX_CTRL is defined as MSR_C5_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_C5_PMON_BOX_CTRL  0x00000DA0

/**
  Package. Uncore C-box 5 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_BOX_STATUS (0x00000DA1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_BOX_STATUS is defined as MSR_C5_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_C5_PMON_BOX_STATUS  0x00000DA1

/**
  Package. Uncore C-box 5 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_BOX_OVF_CTRL (0x00000DA2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_BOX_OVF_CTRL is defined as MSR_C5_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_C5_PMON_BOX_OVF_CTRL  0x00000DA2

/**
  Package. Uncore C-box 5 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_EVNT_SEL0 (0x00000DB0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_EVNT_SEL0 is defined as MSR_C5_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_EVNT_SEL0  0x00000DB0

/**
  Package. Uncore C-box 5 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_CTR0 (0x00000DB1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_CTR0 is defined as MSR_C5_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_CTR0  0x00000DB1

/**
  Package. Uncore C-box 5 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_EVNT_SEL1 (0x00000DB2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_EVNT_SEL1 is defined as MSR_C5_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_EVNT_SEL1  0x00000DB2

/**
  Package. Uncore C-box 5 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_CTR1 (0x00000DB3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_CTR1 is defined as MSR_C5_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_CTR1  0x00000DB3

/**
  Package. Uncore C-box 5 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_EVNT_SEL2 (0x00000DB4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_EVNT_SEL2 is defined as MSR_C5_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_EVNT_SEL2  0x00000DB4

/**
  Package. Uncore C-box 5 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_CTR2 (0x00000DB5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_CTR2 is defined as MSR_C5_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_CTR2  0x00000DB5

/**
  Package. Uncore C-box 5 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_EVNT_SEL3 (0x00000DB6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_EVNT_SEL3 is defined as MSR_C5_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_EVNT_SEL3  0x00000DB6

/**
  Package. Uncore C-box 5 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_CTR3 (0x00000DB7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_CTR3 is defined as MSR_C5_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_CTR3  0x00000DB7

/**
  Package. Uncore C-box 5 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_EVNT_SEL4 (0x00000DB8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_EVNT_SEL4 is defined as MSR_C5_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_EVNT_SEL4  0x00000DB8

/**
  Package. Uncore C-box 5 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_CTR4 (0x00000DB9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_CTR4 is defined as MSR_C5_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_CTR4  0x00000DB9

/**
  Package. Uncore C-box 5 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_EVNT_SEL5 (0x00000DBA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_EVNT_SEL5 is defined as MSR_C5_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_EVNT_SEL5  0x00000DBA

/**
  Package. Uncore C-box 5 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C5_PMON_CTR5 (0x00000DBB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C5_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_C5_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_C5_PMON_CTR5 is defined as MSR_C5_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_C5_PMON_CTR5  0x00000DBB

/**
  Package. Uncore C-box 3 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_BOX_CTRL (0x00000DC0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_BOX_CTRL is defined as MSR_C3_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_C3_PMON_BOX_CTRL  0x00000DC0

/**
  Package. Uncore C-box 3 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_BOX_STATUS (0x00000DC1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_BOX_STATUS is defined as MSR_C3_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_C3_PMON_BOX_STATUS  0x00000DC1

/**
  Package. Uncore C-box 3 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_BOX_OVF_CTRL (0x00000DC2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_BOX_OVF_CTRL is defined as MSR_C3_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_C3_PMON_BOX_OVF_CTRL  0x00000DC2

/**
  Package. Uncore C-box 3 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_EVNT_SEL0 (0x00000DD0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_EVNT_SEL0 is defined as MSR_C3_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_EVNT_SEL0  0x00000DD0

/**
  Package. Uncore C-box 3 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_CTR0 (0x00000DD1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_CTR0 is defined as MSR_C3_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_CTR0  0x00000DD1

/**
  Package. Uncore C-box 3 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_EVNT_SEL1 (0x00000DD2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_EVNT_SEL1 is defined as MSR_C3_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_EVNT_SEL1  0x00000DD2

/**
  Package. Uncore C-box 3 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_CTR1 (0x00000DD3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_CTR1 is defined as MSR_C3_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_CTR1  0x00000DD3

/**
  Package. Uncore C-box 3 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_EVNT_SEL2 (0x00000DD4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_EVNT_SEL2 is defined as MSR_C3_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_EVNT_SEL2  0x00000DD4

/**
  Package. Uncore C-box 3 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_CTR2 (0x00000DD5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_CTR2 is defined as MSR_C3_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_CTR2  0x00000DD5

/**
  Package. Uncore C-box 3 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_EVNT_SEL3 (0x00000DD6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_EVNT_SEL3 is defined as MSR_C3_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_EVNT_SEL3  0x00000DD6

/**
  Package. Uncore C-box 3 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_CTR3 (0x00000DD7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_CTR3 is defined as MSR_C3_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_CTR3  0x00000DD7

/**
  Package. Uncore C-box 3 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_EVNT_SEL4 (0x00000DD8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_EVNT_SEL4 is defined as MSR_C3_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_EVNT_SEL4  0x00000DD8

/**
  Package. Uncore C-box 3 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_CTR4 (0x00000DD9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_CTR4 is defined as MSR_C3_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_CTR4  0x00000DD9

/**
  Package. Uncore C-box 3 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_EVNT_SEL5 (0x00000DDA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_EVNT_SEL5 is defined as MSR_C3_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_EVNT_SEL5  0x00000DDA

/**
  Package. Uncore C-box 3 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C3_PMON_CTR5 (0x00000DDB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C3_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_C3_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_C3_PMON_CTR5 is defined as MSR_C3_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_C3_PMON_CTR5  0x00000DDB

/**
  Package. Uncore C-box 7 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_BOX_CTRL (0x00000DE0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_BOX_CTRL is defined as MSR_C7_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_C7_PMON_BOX_CTRL  0x00000DE0

/**
  Package. Uncore C-box 7 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_BOX_STATUS (0x00000DE1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_BOX_STATUS is defined as MSR_C7_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_C7_PMON_BOX_STATUS  0x00000DE1

/**
  Package. Uncore C-box 7 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_BOX_OVF_CTRL (0x00000DE2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_BOX_OVF_CTRL is defined as MSR_C7_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_C7_PMON_BOX_OVF_CTRL  0x00000DE2

/**
  Package. Uncore C-box 7 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_EVNT_SEL0 (0x00000DF0)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_EVNT_SEL0 is defined as MSR_C7_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_EVNT_SEL0  0x00000DF0

/**
  Package. Uncore C-box 7 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_CTR0 (0x00000DF1)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_CTR0 is defined as MSR_C7_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_CTR0  0x00000DF1

/**
  Package. Uncore C-box 7 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_EVNT_SEL1 (0x00000DF2)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_EVNT_SEL1 is defined as MSR_C7_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_EVNT_SEL1  0x00000DF2

/**
  Package. Uncore C-box 7 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_CTR1 (0x00000DF3)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_CTR1 is defined as MSR_C7_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_CTR1  0x00000DF3

/**
  Package. Uncore C-box 7 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_EVNT_SEL2 (0x00000DF4)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_EVNT_SEL2 is defined as MSR_C7_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_EVNT_SEL2  0x00000DF4

/**
  Package. Uncore C-box 7 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_CTR2 (0x00000DF5)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_CTR2 is defined as MSR_C7_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_CTR2  0x00000DF5

/**
  Package. Uncore C-box 7 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_EVNT_SEL3 (0x00000DF6)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_EVNT_SEL3 is defined as MSR_C7_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_EVNT_SEL3  0x00000DF6

/**
  Package. Uncore C-box 7 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_CTR3 (0x00000DF7)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_CTR3 is defined as MSR_C7_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_CTR3  0x00000DF7

/**
  Package. Uncore C-box 7 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_EVNT_SEL4 (0x00000DF8)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_EVNT_SEL4 is defined as MSR_C7_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_EVNT_SEL4  0x00000DF8

/**
  Package. Uncore C-box 7 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_CTR4 (0x00000DF9)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_CTR4 is defined as MSR_C7_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_CTR4  0x00000DF9

/**
  Package. Uncore C-box 7 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_EVNT_SEL5 (0x00000DFA)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_EVNT_SEL5 is defined as MSR_C7_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_EVNT_SEL5  0x00000DFA

/**
  Package. Uncore C-box 7 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_C7_PMON_CTR5 (0x00000DFB)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_C7_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_C7_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_C7_PMON_CTR5 is defined as MSR_C7_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_C7_PMON_CTR5  0x00000DFB

/**
  Package. Uncore R-box 0 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_BOX_CTRL (0x00000E00)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_BOX_CTRL is defined as MSR_R0_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_R0_PMON_BOX_CTRL  0x00000E00

/**
  Package. Uncore R-box 0 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_BOX_STATUS (0x00000E01)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_BOX_STATUS is defined as MSR_R0_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_R0_PMON_BOX_STATUS  0x00000E01

/**
  Package. Uncore R-box 0 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_BOX_OVF_CTRL (0x00000E02)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_BOX_OVF_CTRL is defined as MSR_R0_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_R0_PMON_BOX_OVF_CTRL  0x00000E02

/**
  Package. Uncore R-box 0 perfmon IPERF0 unit Port 0 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_IPERF0_P0 (0x00000E04)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P0);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P0, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_IPERF0_P0 is defined as MSR_R0_PMON_IPERF0_P0 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_IPERF0_P0  0x00000E04

/**
  Package. Uncore R-box 0 perfmon IPERF0 unit Port 1 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_IPERF0_P1 (0x00000E05)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P1);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P1, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_IPERF0_P1 is defined as MSR_R0_PMON_IPERF0_P1 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_IPERF0_P1  0x00000E05

/**
  Package. Uncore R-box 0 perfmon IPERF0 unit Port 2 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_IPERF0_P2 (0x00000E06)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P2);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P2, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_IPERF0_P2 is defined as MSR_R0_PMON_IPERF0_P2 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_IPERF0_P2  0x00000E06

/**
  Package. Uncore R-box 0 perfmon IPERF0 unit Port 3 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_IPERF0_P3 (0x00000E07)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P3);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P3, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_IPERF0_P3 is defined as MSR_R0_PMON_IPERF0_P3 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_IPERF0_P3  0x00000E07

/**
  Package. Uncore R-box 0 perfmon IPERF0 unit Port 4 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_IPERF0_P4 (0x00000E08)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P4);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P4, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_IPERF0_P4 is defined as MSR_R0_PMON_IPERF0_P4 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_IPERF0_P4  0x00000E08

/**
  Package. Uncore R-box 0 perfmon IPERF0 unit Port 5 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_IPERF0_P5 (0x00000E09)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P5);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P5, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_IPERF0_P5 is defined as MSR_R0_PMON_IPERF0_P5 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_IPERF0_P5  0x00000E09

/**
  Package. Uncore R-box 0 perfmon IPERF0 unit Port 6 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_IPERF0_P6 (0x00000E0A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P6);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P6, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_IPERF0_P6 is defined as MSR_R0_PMON_IPERF0_P6 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_IPERF0_P6  0x00000E0A

/**
  Package. Uncore R-box 0 perfmon IPERF0 unit Port 7 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_IPERF0_P7 (0x00000E0B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P7);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_IPERF0_P7, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_IPERF0_P7 is defined as MSR_R0_PMON_IPERF0_P7 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_IPERF0_P7  0x00000E0B

/**
  Package. Uncore R-box 0 perfmon QLX unit Port 0 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_QLX_P0 (0x00000E0C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_QLX_P0);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_QLX_P0, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_QLX_P0 is defined as MSR_R0_PMON_QLX_P0 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_QLX_P0  0x00000E0C

/**
  Package. Uncore R-box 0 perfmon QLX unit Port 1 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_QLX_P1 (0x00000E0D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_QLX_P1);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_QLX_P1, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_QLX_P1 is defined as MSR_R0_PMON_QLX_P1 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_QLX_P1  0x00000E0D

/**
  Package. Uncore R-box 0 perfmon QLX unit Port 2 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_QLX_P2 (0x00000E0E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_QLX_P2);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_QLX_P2, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_QLX_P2 is defined as MSR_R0_PMON_QLX_P2 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_QLX_P2  0x00000E0E

/**
  Package. Uncore R-box 0 perfmon QLX unit Port 3 select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_QLX_P3 (0x00000E0F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_QLX_P3);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_QLX_P3, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_QLX_P3 is defined as MSR_R0_PMON_QLX_P3 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_QLX_P3  0x00000E0F

/**
  Package. Uncore R-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_EVNT_SEL0 (0x00000E10)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL0);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL0, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_EVNT_SEL0 is defined as MSR_R0_PMON_EVNT_SEL0 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_EVNT_SEL0  0x00000E10

/**
  Package. Uncore R-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_CTR0 (0x00000E11)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_CTR0);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_CTR0, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_CTR0 is defined as MSR_R0_PMON_CTR0 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_CTR0  0x00000E11

/**
  Package. Uncore R-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_EVNT_SEL1 (0x00000E12)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL1);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL1, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_EVNT_SEL1 is defined as MSR_R0_PMON_EVNT_SEL1 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_EVNT_SEL1  0x00000E12

/**
  Package. Uncore R-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_CTR1 (0x00000E13)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_CTR1);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_CTR1, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_CTR1 is defined as MSR_R0_PMON_CTR1 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_CTR1  0x00000E13

/**
  Package. Uncore R-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_EVNT_SEL2 (0x00000E14)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL2);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL2, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_EVNT_SEL2 is defined as MSR_R0_PMON_EVNT_SEL2 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_EVNT_SEL2  0x00000E14

/**
  Package. Uncore R-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_CTR2 (0x00000E15)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_CTR2);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_CTR2, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_CTR2 is defined as MSR_R0_PMON_CTR2 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_CTR2  0x00000E15

/**
  Package. Uncore R-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_EVNT_SEL3 (0x00000E16)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL3);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL3, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_EVNT_SEL3 is defined as MSR_R0_PMON_EVNT_SEL3 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_EVNT_SEL3  0x00000E16

/**
  Package. Uncore R-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_CTR3 (0x00000E17)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_CTR3);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_CTR3, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_CTR3 is defined as MSR_R0_PMON_CTR3 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_CTR3  0x00000E17

/**
  Package. Uncore R-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_EVNT_SEL4 (0x00000E18)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL4);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL4, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_EVNT_SEL4 is defined as MSR_R0_PMON_EVNT_SEL4 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_EVNT_SEL4  0x00000E18

/**
  Package. Uncore R-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_CTR4 (0x00000E19)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_CTR4);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_CTR4, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_CTR4 is defined as MSR_R0_PMON_CTR4 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_CTR4  0x00000E19

/**
  Package. Uncore R-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_EVNT_SEL5 (0x00000E1A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL5);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL5, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_EVNT_SEL5 is defined as MSR_R0_PMON_EVNT_SEL5 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_EVNT_SEL5  0x00000E1A

/**
  Package. Uncore R-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_CTR5 (0x00000E1B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_CTR5);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_CTR5, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_CTR5 is defined as MSR_R0_PMON_CTR5 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_CTR5  0x00000E1B

/**
  Package. Uncore R-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_EVNT_SEL6 (0x00000E1C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL6);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL6, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_EVNT_SEL6 is defined as MSR_R0_PMON_EVNT_SEL6 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_EVNT_SEL6  0x00000E1C

/**
  Package. Uncore R-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_CTR6 (0x00000E1D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_CTR6);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_CTR6, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_CTR6 is defined as MSR_R0_PMON_CTR6 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_CTR6  0x00000E1D

/**
  Package. Uncore R-box 0 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_EVNT_SEL7 (0x00000E1E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL7);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_EVNT_SEL7, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_EVNT_SEL7 is defined as MSR_R0_PMON_EVNT_SEL7 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_EVNT_SEL7  0x00000E1E

/**
  Package. Uncore R-box 0 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R0_PMON_CTR7 (0x00000E1F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R0_PMON_CTR7);
  AsmWriteMsr64 (MSR_NEHALEM_R0_PMON_CTR7, Msr);
  @endcode
  @note MSR_NEHALEM_R0_PMON_CTR7 is defined as MSR_R0_PMON_CTR7 in SDM.
**/
#define MSR_NEHALEM_R0_PMON_CTR7  0x00000E1F

/**
  Package. Uncore R-box 1 perfmon local box control MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_BOX_CTRL (0x00000E20)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_BOX_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_BOX_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_BOX_CTRL is defined as MSR_R1_PMON_BOX_CTRL in SDM.
**/
#define MSR_NEHALEM_R1_PMON_BOX_CTRL  0x00000E20

/**
  Package. Uncore R-box 1 perfmon local box status MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_BOX_STATUS (0x00000E21)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_BOX_STATUS);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_BOX_STATUS, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_BOX_STATUS is defined as MSR_R1_PMON_BOX_STATUS in SDM.
**/
#define MSR_NEHALEM_R1_PMON_BOX_STATUS  0x00000E21

/**
  Package. Uncore R-box 1 perfmon local box overflow control MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_BOX_OVF_CTRL (0x00000E22)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_BOX_OVF_CTRL);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_BOX_OVF_CTRL, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_BOX_OVF_CTRL is defined as MSR_R1_PMON_BOX_OVF_CTRL in SDM.
**/
#define MSR_NEHALEM_R1_PMON_BOX_OVF_CTRL  0x00000E22

/**
  Package. Uncore R-box 1 perfmon IPERF1 unit Port 8 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_IPERF1_P8 (0x00000E24)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P8);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P8, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_IPERF1_P8 is defined as MSR_R1_PMON_IPERF1_P8 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_IPERF1_P8  0x00000E24

/**
  Package. Uncore R-box 1 perfmon IPERF1 unit Port 9 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_IPERF1_P9 (0x00000E25)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P9);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P9, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_IPERF1_P9 is defined as MSR_R1_PMON_IPERF1_P9 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_IPERF1_P9  0x00000E25

/**
  Package. Uncore R-box 1 perfmon IPERF1 unit Port 10 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_IPERF1_P10 (0x00000E26)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P10);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P10, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_IPERF1_P10 is defined as MSR_R1_PMON_IPERF1_P10 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_IPERF1_P10  0x00000E26

/**
  Package. Uncore R-box 1 perfmon IPERF1 unit Port 11 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_IPERF1_P11 (0x00000E27)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P11);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P11, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_IPERF1_P11 is defined as MSR_R1_PMON_IPERF1_P11 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_IPERF1_P11  0x00000E27

/**
  Package. Uncore R-box 1 perfmon IPERF1 unit Port 12 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_IPERF1_P12 (0x00000E28)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P12);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P12, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_IPERF1_P12 is defined as MSR_R1_PMON_IPERF1_P12 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_IPERF1_P12  0x00000E28

/**
  Package. Uncore R-box 1 perfmon IPERF1 unit Port 13 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_IPERF1_P13 (0x00000E29)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P13);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P13, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_IPERF1_P13 is defined as MSR_R1_PMON_IPERF1_P13 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_IPERF1_P13  0x00000E29

/**
  Package. Uncore R-box 1 perfmon IPERF1 unit Port 14 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_IPERF1_P14 (0x00000E2A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P14);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P14, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_IPERF1_P14 is defined as MSR_R1_PMON_IPERF1_P14 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_IPERF1_P14  0x00000E2A

/**
  Package. Uncore R-box 1 perfmon IPERF1 unit Port 15 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_IPERF1_P15 (0x00000E2B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P15);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_IPERF1_P15, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_IPERF1_P15 is defined as MSR_R1_PMON_IPERF1_P15 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_IPERF1_P15  0x00000E2B

/**
  Package. Uncore R-box 1 perfmon QLX unit Port 4 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_QLX_P4 (0x00000E2C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_QLX_P4);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_QLX_P4, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_QLX_P4 is defined as MSR_R1_PMON_QLX_P4 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_QLX_P4  0x00000E2C

/**
  Package. Uncore R-box 1 perfmon QLX unit Port 5 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_QLX_P5 (0x00000E2D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_QLX_P5);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_QLX_P5, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_QLX_P5 is defined as MSR_R1_PMON_QLX_P5 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_QLX_P5  0x00000E2D

/**
  Package. Uncore R-box 1 perfmon QLX unit Port 6 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_QLX_P6 (0x00000E2E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_QLX_P6);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_QLX_P6, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_QLX_P6 is defined as MSR_R1_PMON_QLX_P6 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_QLX_P6  0x00000E2E

/**
  Package. Uncore R-box 1 perfmon QLX unit Port 7 select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_QLX_P7 (0x00000E2F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_QLX_P7);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_QLX_P7, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_QLX_P7 is defined as MSR_R1_PMON_QLX_P7 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_QLX_P7  0x00000E2F

/**
  Package. Uncore R-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_EVNT_SEL8 (0x00000E30)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL8);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL8, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_EVNT_SEL8 is defined as MSR_R1_PMON_EVNT_SEL8 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_EVNT_SEL8  0x00000E30

/**
  Package. Uncore R-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_CTR8 (0x00000E31)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_CTR8);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_CTR8, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_CTR8 is defined as MSR_R1_PMON_CTR8 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_CTR8  0x00000E31

/**
  Package. Uncore R-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_EVNT_SEL9 (0x00000E32)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL9);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL9, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_EVNT_SEL9 is defined as MSR_R1_PMON_EVNT_SEL9 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_EVNT_SEL9  0x00000E32

/**
  Package. Uncore R-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_CTR9 (0x00000E33)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_CTR9);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_CTR9, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_CTR9 is defined as MSR_R1_PMON_CTR9 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_CTR9  0x00000E33

/**
  Package. Uncore R-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_EVNT_SEL10 (0x00000E34)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL10);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL10, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_EVNT_SEL10 is defined as MSR_R1_PMON_EVNT_SEL10 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_EVNT_SEL10  0x00000E34

/**
  Package. Uncore R-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_CTR10 (0x00000E35)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_CTR10);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_CTR10, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_CTR10 is defined as MSR_R1_PMON_CTR10 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_CTR10  0x00000E35

/**
  Package. Uncore R-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_EVNT_SEL11 (0x00000E36)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL11);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL11, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_EVNT_SEL11 is defined as MSR_R1_PMON_EVNT_SEL11 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_EVNT_SEL11  0x00000E36

/**
  Package. Uncore R-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_CTR11 (0x00000E37)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_CTR11);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_CTR11, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_CTR11 is defined as MSR_R1_PMON_CTR11 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_CTR11  0x00000E37

/**
  Package. Uncore R-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_EVNT_SEL12 (0x00000E38)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL12);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL12, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_EVNT_SEL12 is defined as MSR_R1_PMON_EVNT_SEL12 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_EVNT_SEL12  0x00000E38

/**
  Package. Uncore R-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_CTR12 (0x00000E39)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_CTR12);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_CTR12, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_CTR12 is defined as MSR_R1_PMON_CTR12 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_CTR12  0x00000E39

/**
  Package. Uncore R-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_EVNT_SEL13 (0x00000E3A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL13);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL13, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_EVNT_SEL13 is defined as MSR_R1_PMON_EVNT_SEL13 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_EVNT_SEL13  0x00000E3A

/**
  Package. Uncore R-box 1perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_CTR13 (0x00000E3B)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_CTR13);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_CTR13, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_CTR13 is defined as MSR_R1_PMON_CTR13 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_CTR13  0x00000E3B

/**
  Package. Uncore R-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_EVNT_SEL14 (0x00000E3C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL14);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL14, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_EVNT_SEL14 is defined as MSR_R1_PMON_EVNT_SEL14 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_EVNT_SEL14  0x00000E3C

/**
  Package. Uncore R-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_CTR14 (0x00000E3D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_CTR14);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_CTR14, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_CTR14 is defined as MSR_R1_PMON_CTR14 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_CTR14  0x00000E3D

/**
  Package. Uncore R-box 1 perfmon event select MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_EVNT_SEL15 (0x00000E3E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL15);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_EVNT_SEL15, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_EVNT_SEL15 is defined as MSR_R1_PMON_EVNT_SEL15 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_EVNT_SEL15  0x00000E3E

/**
  Package. Uncore R-box 1 perfmon counter MSR.

  @param  ECX  MSR_NEHALEM_R1_PMON_CTR15 (0x00000E3F)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_R1_PMON_CTR15);
  AsmWriteMsr64 (MSR_NEHALEM_R1_PMON_CTR15, Msr);
  @endcode
  @note MSR_NEHALEM_R1_PMON_CTR15 is defined as MSR_R1_PMON_CTR15 in SDM.
**/
#define MSR_NEHALEM_R1_PMON_CTR15  0x00000E3F

/**
  Package. Uncore B-box 0 perfmon local box match MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_MATCH (0x00000E45)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_MATCH);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_MATCH, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_MATCH is defined as MSR_B0_PMON_MATCH in SDM.
**/
#define MSR_NEHALEM_B0_PMON_MATCH  0x00000E45

/**
  Package. Uncore B-box 0 perfmon local box mask MSR.

  @param  ECX  MSR_NEHALEM_B0_PMON_MASK (0x00000E46)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B0_PMON_MASK);
  AsmWriteMsr64 (MSR_NEHALEM_B0_PMON_MASK, Msr);
  @endcode
  @note MSR_NEHALEM_B0_PMON_MASK is defined as MSR_B0_PMON_MASK in SDM.
**/
#define MSR_NEHALEM_B0_PMON_MASK  0x00000E46

/**
  Package. Uncore S-box 0 perfmon local box match MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_MATCH (0x00000E49)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_MATCH);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_MATCH, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_MATCH is defined as MSR_S0_PMON_MATCH in SDM.
**/
#define MSR_NEHALEM_S0_PMON_MATCH  0x00000E49

/**
  Package. Uncore S-box 0 perfmon local box mask MSR.

  @param  ECX  MSR_NEHALEM_S0_PMON_MASK (0x00000E4A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S0_PMON_MASK);
  AsmWriteMsr64 (MSR_NEHALEM_S0_PMON_MASK, Msr);
  @endcode
  @note MSR_NEHALEM_S0_PMON_MASK is defined as MSR_S0_PMON_MASK in SDM.
**/
#define MSR_NEHALEM_S0_PMON_MASK  0x00000E4A

/**
  Package. Uncore B-box 1 perfmon local box match MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_MATCH (0x00000E4D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_MATCH);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_MATCH, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_MATCH is defined as MSR_B1_PMON_MATCH in SDM.
**/
#define MSR_NEHALEM_B1_PMON_MATCH  0x00000E4D

/**
  Package. Uncore B-box 1 perfmon local box mask MSR.

  @param  ECX  MSR_NEHALEM_B1_PMON_MASK (0x00000E4E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_B1_PMON_MASK);
  AsmWriteMsr64 (MSR_NEHALEM_B1_PMON_MASK, Msr);
  @endcode
  @note MSR_NEHALEM_B1_PMON_MASK is defined as MSR_B1_PMON_MASK in SDM.
**/
#define MSR_NEHALEM_B1_PMON_MASK  0x00000E4E

/**
  Package. Uncore M-box 0 perfmon local box address match/mask config MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_MM_CONFIG (0x00000E54)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_MM_CONFIG);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_MM_CONFIG, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_MM_CONFIG is defined as MSR_M0_PMON_MM_CONFIG in SDM.
**/
#define MSR_NEHALEM_M0_PMON_MM_CONFIG  0x00000E54

/**
  Package. Uncore M-box 0 perfmon local box address match MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_ADDR_MATCH (0x00000E55)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_ADDR_MATCH);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_ADDR_MATCH, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_ADDR_MATCH is defined as MSR_M0_PMON_ADDR_MATCH in SDM.
**/
#define MSR_NEHALEM_M0_PMON_ADDR_MATCH  0x00000E55

/**
  Package. Uncore M-box 0 perfmon local box address mask MSR.

  @param  ECX  MSR_NEHALEM_M0_PMON_ADDR_MASK (0x00000E56)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M0_PMON_ADDR_MASK);
  AsmWriteMsr64 (MSR_NEHALEM_M0_PMON_ADDR_MASK, Msr);
  @endcode
  @note MSR_NEHALEM_M0_PMON_ADDR_MASK is defined as MSR_M0_PMON_ADDR_MASK in SDM.
**/
#define MSR_NEHALEM_M0_PMON_ADDR_MASK  0x00000E56

/**
  Package. Uncore S-box 1 perfmon local box match MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_MATCH (0x00000E59)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_MATCH);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_MATCH, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_MATCH is defined as MSR_S1_PMON_MATCH in SDM.
**/
#define MSR_NEHALEM_S1_PMON_MATCH  0x00000E59

/**
  Package. Uncore S-box 1 perfmon local box mask MSR.

  @param  ECX  MSR_NEHALEM_S1_PMON_MASK (0x00000E5A)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_S1_PMON_MASK);
  AsmWriteMsr64 (MSR_NEHALEM_S1_PMON_MASK, Msr);
  @endcode
  @note MSR_NEHALEM_S1_PMON_MASK is defined as MSR_S1_PMON_MASK in SDM.
**/
#define MSR_NEHALEM_S1_PMON_MASK  0x00000E5A

/**
  Package. Uncore M-box 1 perfmon local box address match/mask config MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_MM_CONFIG (0x00000E5C)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_MM_CONFIG);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_MM_CONFIG, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_MM_CONFIG is defined as MSR_M1_PMON_MM_CONFIG in SDM.
**/
#define MSR_NEHALEM_M1_PMON_MM_CONFIG  0x00000E5C

/**
  Package. Uncore M-box 1 perfmon local box address match MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_ADDR_MATCH (0x00000E5D)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_ADDR_MATCH);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_ADDR_MATCH, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_ADDR_MATCH is defined as MSR_M1_PMON_ADDR_MATCH in SDM.
**/
#define MSR_NEHALEM_M1_PMON_ADDR_MATCH  0x00000E5D

/**
  Package. Uncore M-box 1 perfmon local box address mask MSR.

  @param  ECX  MSR_NEHALEM_M1_PMON_ADDR_MASK (0x00000E5E)
  @param  EAX  Lower 32-bits of MSR value.
  @param  EDX  Upper 32-bits of MSR value.

  <b>Example usage</b>
  @code
  UINT64  Msr;

  Msr = AsmReadMsr64 (MSR_NEHALEM_M1_PMON_ADDR_MASK);
  AsmWriteMsr64 (MSR_NEHALEM_M1_PMON_ADDR_MASK, Msr);
  @endcode
  @note MSR_NEHALEM_M1_PMON_ADDR_MASK is defined as MSR_M1_PMON_ADDR_MASK in SDM.
**/
#define MSR_NEHALEM_M1_PMON_ADDR_MASK  0x00000E5E

#endif
